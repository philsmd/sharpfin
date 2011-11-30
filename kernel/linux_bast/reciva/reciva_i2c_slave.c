/*
 * Reciva I2C slave for s3c2410
 *
 * Copyright (c) 2006 Reciva Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 * Description:
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/soundcard.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>

#include <asm/uaccess.h>              
#include <asm/hardware.h>
#include <asm/arch/regs-gpio.h>
#include <asm/byteorder.h>
#include <asm/irq.h>
#include <asm/io.h>

#include "reciva_i2c_slave.h"
#include "reciva_util.h"
#include "reciva_gpio.h"


   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

#define BUF_SIZE (1 << 10)
typedef struct
{
  char buf[BUF_SIZE];
  int rd_index;
  int wr_index;
} buffer_t;

/* A Master Read is done in two phases:
 * 1. 1 byte read to establish how many read bytes are available
 * 2. x byte read to read the data */
typedef enum 
{
  /* Master has not extracted the length yet */
  RI2CS_LENGTH_REQ_PENDING,

  /* Master has extracted the length and is now ready to read the data */
  RI2CS_DATA_READ_PENDING,
} slave_tx_state_t;



   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

static void reset_module(void);
static int set_exit_on_tx_end(void);


   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

#define PREFIX "RI2CS:"

/* GPIO registers */
#define GPIO_REG(x) (unsigned int)((volatile unsigned int *)(S3C2410_VA_GPIO + (x)))

/* GPIO port C */
#define GPCCON GPIO_REG(0x20)
#define GPCDAT GPIO_REG(0x24)
#define GPCUP GPIO_REG(0x28)
/* GPIO port E */
#define GPECON GPIO_REG(0x40)
#define GPEDAT GPIO_REG(0x44)
#define GPEUP GPIO_REG(0x48)
/* GPIO port G */
#define GPGCON GPIO_REG(0x60)
#define GPGDAT GPIO_REG(0x64)
#define GPGUP GPIO_REG(0x68)

/* IIC registers */ 
#define IICCON   (S3C2410_VA_IIC + 0x00)
#define IICSTAT  (S3C2410_VA_IIC + 0x04) 
#define IICADD   (S3C2410_VA_IIC + 0x08) 
#define IICDS    (S3C2410_VA_IIC + 0x0c) 

/* Interrupt device ID */
#define DEV_ID 1


   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

static char acModuleName[] = "Reciva I2C Slave";
static wait_queue_head_t wait_queue;

/* Tx and Rx buffers */
static buffer_t rx_buffer;
static buffer_t tx_buffer;

/* Indicates if slave tx data is valid */
static int slave_tx_data_valid = 0;

/* Indicates slave is waiting for some data */
static int slave_tx_ready = 0;
static spinlock_t tx_ready_lock = SPIN_LOCK_UNLOCKED;

/* Count of bytes transmitted in current Slave Tx transfer */
static int tx_count;

/* Total number of bytes to be transmitted in current Slave Tx transfer */
static int tx_length;

/* The master will perform two Master Reads to read back data from the slave
 * The first read is used to determine the length, the 2nd is to read the actual 
 * data */
static slave_tx_state_t slave_tx_state = RI2CS_LENGTH_REQ_PENDING;

/* Slave address lookup */
static int slave_address_lookup[] =
{
  0x00,  // 0x00
  0x01,  // 0x01
  0x02,  // 0x02
  0x03,  // 0x03
  0x10,  // 0x04
  0x11,  // 0x05
  0x12,  // 0x06
  0x13,  // 0x07
  0x20,  // 0x08
  0x21,  // 0x09
  0x22,  // 0x0a
  0x23,  // 0x0b
  0x30,  // 0x0c
  0x31,  // 0x0d
  0x32,  // 0x0e
  0x33,  // 0x0f
};

/* The device slave address */
static int slave_address = 0;

/* Flag to signal that I2C should be switched off after next TX
 * 
 * Because of the two stage write/read process, a master sending a command
 * will put the slave into receive mode. If there is no application to
 * process the command, this can hang the bus.
 *
 * To try and void this the I2C is not enabled until a file handle is opened.
 * Similarly the application will signal its final transmission allowing this
 * driver to disable I2C once the transmission has finished. */
static int exit_on_tx_end = 0;

   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/

/****************************************************************************
 * Initialise a buffer_t struct
 ****************************************************************************/
static void buf_init(buffer_t *buffer)
{
  buffer->rd_index = 0;
  buffer->wr_index = 0;
}

/****************************************************************************
 * Inc read index
 ****************************************************************************/
static void buf_inc_rd_index(buffer_t *buffer)
{
  buffer->rd_index++;
  if (buffer->rd_index >= BUF_SIZE)
    buffer->rd_index = 0;
}

/****************************************************************************
 * Inc write index
 ****************************************************************************/
static void buf_inc_wr_index(buffer_t *buffer)
{
  buffer->wr_index++;
  if (buffer->wr_index >= BUF_SIZE)
    buffer->wr_index = 0;
}

/****************************************************************************
 * Reads a byte from the buffer
 ****************************************************************************/
static char buf_read(buffer_t *buffer)
{
  char data = buffer->buf[buffer->rd_index];
  buf_inc_rd_index(buffer);
  return data;
}

/****************************************************************************
 * Write a byte to the buffer
 ****************************************************************************/
static void buf_write(buffer_t *buffer, char data)
{
  buffer->buf[buffer->wr_index] = data;
  buf_inc_wr_index(buffer);
}

/****************************************************************************
 * Indicates if the buffer is empty
 ****************************************************************************/
static int buf_data_in_buffer(buffer_t *buffer)
{
  int count = 0;

  count = buffer->wr_index - buffer->rd_index;
  count &= (BUF_SIZE-1);

  return count;
}

/****************************************************************************
 * Returns the I2C slave address
 ****************************************************************************/
static int get_slave_address(void)
{
  /* Read status of I2CADDR lines */
  int gpgdat = __raw_readl(GPGDAT);
  printk(PREFIX "Slave Address (raw) = %x\n", gpgdat & 0xf00);
  gpgdat >>= 8;
  gpgdat &= 0x0f;
  
  /* Convert this into a slave address */
  int addr = slave_address_lookup[gpgdat];
  printk(PREFIX "Slave Address (converted) = %02x\n", addr);
  return addr;
}

/****************************************************************************
 * Start or continue a Slave Tx transfer
 ****************************************************************************/
static void slave_tx(int iicstat)
{
  char temp = 0x00;

  if (0)
  {
    printk("slave_tx iicstat %02X, state %d, valid %d\n", iicstat, slave_tx_state, slave_tx_data_valid);
  }

  unsigned long flags;
  spin_lock_irqsave(&tx_ready_lock, flags);

  if (slave_tx_data_valid)
  {
    switch (slave_tx_state)
    {
      case RI2CS_LENGTH_REQ_PENDING:
        if (iicstat & 0x04)
        {
          /* It's a 2 byte read to determine the length of the following one */
          temp = buf_data_in_buffer(&tx_buffer);
          __raw_writel ((temp & 0x0000ff00) >> 8, IICDS);   // Data length MSB
          rutl_regwrite( 0, (1 << 4), IICCON); // Clear pending bit
          tx_count = 1; 
        }
        else
        {
          if (tx_count == 1)
          {
            temp = buf_data_in_buffer(&tx_buffer);
            __raw_writel ((temp & 0x000000ff), IICDS);   // Data length LSB
            rutl_regwrite( 0, (1 << 4), IICCON); // Clear pending bit
            tx_count++;
          }
          else
          {
            /* No more data to tx - back to Slave Rx mode */
            rutl_regwrite( 0, (1 << 4), IICCON); // Clear pending bit
            rutl_regwrite( 0, (3 << 6), IICSTAT); // Back to slave rx mode
            slave_tx_state = RI2CS_DATA_READ_PENDING;
            tx_count = 0;
          }
        }
        break;
      case RI2CS_DATA_READ_PENDING:
        if (tx_count < tx_length) 
        {
          if (buf_data_in_buffer(&tx_buffer))
            temp = buf_read(&tx_buffer);

          __raw_writel (temp, IICDS);          // Data
          rutl_regwrite( 0, (1 << 4), IICCON); // Clear pending bit
          tx_count++;
        }
        else
        {
          /* All data transmitted */
          printk("slave_tx finished %d bytes (exit_on_tx_end %d)\n",
                 tx_length, exit_on_tx_end);

          rutl_regwrite( 0, (1 << 4), IICCON); // Clear pending bit
          if (exit_on_tx_end)
          {
            __raw_writel (0x00, IICSTAT); // Switch off I2C data rx/tx
            exit_on_tx_end = 0;
          }
          else
          {
            rutl_regwrite( 0, (3 << 6), IICSTAT); // Back to slave rx mode
          }
        }
        break;
    }
  }
  else
  {
    slave_tx_ready = 1;
  }

  spin_unlock_irqrestore(&tx_ready_lock, flags);
}

   /*************************************************************************/
   /***                        File Operations - START                    ***/
   /*************************************************************************/

/****************************************************************************
 * Returns status of device. Indicates if there is data available to read
 ****************************************************************************/
static unsigned int 
reciva_poll (struct file *file, poll_table *wait)
{
  poll_wait(file, &wait_queue, wait);
  if (buf_data_in_buffer(&rx_buffer))
  {
    /* Message available to read */
    return POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM;
  }
  else 
  {
    /* Device is writable */
    return POLLOUT | POLLWRNORM;
  }
}

/****************************************************************************
 * Read data from device
 ****************************************************************************/
static int 
reciva_read (struct file *filp, char *buffer, size_t count, loff_t *ppos)
{
  int ret;
  int read_count = 0;
  char buf[count];

  /* Read data into a local buffer */ 
  while (read_count < count && buf_data_in_buffer(&rx_buffer))
  {
    buf[read_count] = buf_read(&rx_buffer);
    read_count++;
  }

  /* Then copy it to user application */
  if (copy_to_user (buffer, buf, read_count))
    ret = -EFAULT;
  else
    ret = read_count;

  return ret;
}

/****************************************************************************
 * Write data to device
 ****************************************************************************/
static int 
reciva_write (struct file *filp, const char *buf, size_t count, loff_t *f_os)
{
  int write_count = 0;
  char local_buf[count];

  /* Copy into tx buffer */
  copy_from_user(local_buf, buf, count);
  buf_init(&tx_buffer);
  while (write_count < count)
  {
    buf_write(&tx_buffer, local_buf[write_count]);
    write_count++;
  }

  /* Mark tx data valid */
  slave_tx_data_valid = 1;

  /* Start the slave tx if appropriate */
  tx_length = write_count;
  unsigned long flags;
  spin_lock_irqsave(&tx_ready_lock, flags);
  if (slave_tx_ready)
  {
    slave_tx(0x04);
  }
  spin_unlock_irqrestore(&tx_ready_lock, flags);

  return write_count;
}

/****************************************************************************
 * Open device. This is the first operation performed on the file structure.
 ****************************************************************************/
static int 
reciva_open(struct inode * inode, struct file * file)
{
  printk(PREFIX "open\n");
  return 0;
}

/****************************************************************************
 * Release file structure. Invoked when the file structure is being released
 ****************************************************************************/
static int 
reciva_release(struct inode * inode, struct file * file)
{
  int iicstat = __raw_readl(IICSTAT);
  printk(KERN_ERR PREFIX "release iicstat %02X\n", iicstat);

  return 0;
}

/****************************************************************************
 * Handle command from application
 ****************************************************************************/
static int
reciva_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
  printk(PREFIX "IOCTL (%d)\n", cmd); 
  switch(cmd)
  {
    case IOC_I2C_RESET:
      printk(PREFIX "IOC_I2C_RESET\n"); 
      reset_module();
      break;

    case IOC_I2C_EXIT_ON_NEXT_TX:
      printk(PREFIX "IOC_I2C_EXIT_ON_NEXT_TX\n"); 
      return set_exit_on_tx_end();
      break;

    default:
      return -ENODEV;
  }

  return 0;
}

static struct file_operations reciva_fops =
{
  owner:    THIS_MODULE,
  ioctl:    reciva_ioctl,
  read:     reciva_read,
  write:    reciva_write,
  poll:     reciva_poll,  
  open:     reciva_open,
  release:  reciva_release,
};

static struct miscdevice reciva_miscdev =
{
  MISC_DYNAMIC_MINOR,
  "reciva_i2c_slave",
  &reciva_fops
};

   /*************************************************************************/
   /***                        File Operations - END                      ***/
   /*************************************************************************/


/****************************************************************************
 * Reset the module
 ****************************************************************************/
static void reset_module(void)
{
  __raw_writel (0xa0, IICCON);  // Enable irq and ack generation

  // Check to see if there is already a final TX pending (e.g. device closed then
  // opened in quick sucession). If not, reset I2C to receive mode.
  unsigned long flags;
  spin_lock_irqsave(&tx_ready_lock, flags);
  if (!exit_on_tx_end)
  {
    __raw_writel (0x10, IICSTAT); // Slave receive mode
  }
  exit_on_tx_end = 0;
  spin_unlock_irqrestore(&tx_ready_lock, flags);

  buf_init(&tx_buffer);
  buf_init(&rx_buffer);
}

/****************************************************************************
 * Set module to shut down I2C after next tx
 ****************************************************************************/
static int set_exit_on_tx_end(void)
{
  // This should only be called when we're waiting for data from app to
  // return to the master so there is no need to use the tx_ready_lock as
  // there should be no data to transmit yet
  if (slave_tx_data_valid)
  {
    return -EALREADY;
  }
    
  exit_on_tx_end = 1;

  return 0;
}

/****************************************************************************
 * I2C interrupt handler
 ****************************************************************************/
static void reciva_i2c_interrupt (int irq, void *dev_id, struct pt_regs *regs)
{
  int iiccon = __raw_readl(IICCON);
  int iicstat = __raw_readl(IICSTAT);
  int iicds = __raw_readl(IICDS);

  if (0)
  {
    printk(PREFIX "IRQ IICCON=0x%08x IICSTAT=0x%08x IICDS=0x%08x\n", 
                                                 iiccon, 
                                                 iicstat,
                                                 iicds);
  }

  if ((iicstat & 0xc0) == 0x00)
  {
    /* Slave receive mode (Master Write) */

    /* Reset the slave tx status */
    slave_tx_data_valid = 0;
    slave_tx_ready = 0;
    slave_tx_state = RI2CS_LENGTH_REQ_PENDING;

    /* Clear pending bit */
    rutl_regwrite(0, (1 << 4), IICCON);

    if (iicstat & 0x04)
    {
      /* Address match only, no data to read yet */
      //printk(PREFIX "Address match\n"); 
    }
    else
    {
      /* Copy to rx buffer */
      buf_write(&rx_buffer, (char)iicds);

      /* And inform app that new data has arrived */
      wake_up_interruptible(&wait_queue);
      //printk(PREFIX "Data %02x\n", iicds); 
    }
  }
  else if ((iicstat & 0xc0) == 0x40)
  {
    /* Slave tx mode (Master Read) */
    slave_tx(iicstat);
  }
}

/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
reciva_module_init(void)
{
  printk(PREFIX "%s module: loaded\n", acModuleName);

  init_waitqueue_head (&wait_queue);

  buf_init(&tx_buffer);
  buf_init(&rx_buffer);

  /* Tell GPIO module which GPIO pins we are using */
  rgpio_register("GPE14 (SCL)", acModuleName);
  rgpio_register("GPE15 (SDA)", acModuleName);
  rgpio_register("GPG11 (I2CADDR3)", acModuleName);
  rgpio_register("GPG10 (I2CADDR2)", acModuleName);
  rgpio_register("GPG9 (I2CADDR1)", acModuleName);
  rgpio_register("GPG8 (I2CADDR0)", acModuleName);

  /* GPE14 */
  rutl_regwrite((0 << 14), (1 << 14), GPEUP) ; // Enable pullup
  rutl_regwrite((2 << 28), (3 << 28), GPECON); // Set as I2C SCL
  /* GPE15 */
  rutl_regwrite((0 << 15), (1 << 15), GPEUP) ; // Enable pullup
  rutl_regwrite((2 << 30), (3 << 30), GPECON); // Set as I2C SDA
  /* GPG8 */
  rutl_regwrite((1 << 8), (0 << 8), GPGUP) ;   // Disable pullup
  rutl_regwrite((0 << 16), (3 << 16), GPGCON); // Set as input
  /* GPG9 */
  rutl_regwrite((1 << 9), (0 << 9), GPGUP) ;   // Disable pullup
  rutl_regwrite((0 << 18), (3 << 18), GPGCON); // Set as input
  /* GPG10 */
  rutl_regwrite((1 << 10), (0 << 10), GPGUP) ; // Disable pullup
  rutl_regwrite((0 << 20), (3 << 20), GPGCON); // Set as input
  /* GPG11 */
  rutl_regwrite((1 << 11), (0 << 11), GPGUP) ; // Disable pullup
  rutl_regwrite((0 << 22), (3 << 22), GPGCON); // Set as input

  /* Register the device */
  misc_register (&reciva_miscdev);

  /* Set up the slave address */
  slave_address = get_slave_address();
  __raw_writel (slave_address << 1, IICADD);

  /* Enable irq and ack generation */
  __raw_writel (0xa0, IICCON);

  /* Initialise to disable I2C - will be enabled when the user
   * calls reset ioctl */
  __raw_writel (0x00, IICSTAT);

  /* Request I2C interrupt */
  request_irq(IRQ_IIC, 
              reciva_i2c_interrupt, 
              0,
              "reciva_i2c", 
              (void *)DEV_ID);

  return 0;
}

/****************************************************************************
 * Module tidy up
 ****************************************************************************/
static void __exit 
reciva_module_exit(void)
{
  printk(PREFIX "%s module: unloaded\n", acModuleName);

  /* Clear up the IRQ */
  disable_irq (IRQ_IIC);
  free_irq(IRQ_IIC, (void *)DEV_ID);

  /* Unregister the device */
  misc_deregister(&reciva_miscdev);
}


module_init(reciva_module_init);
module_exit(reciva_module_exit);

MODULE_LICENSE("GPL");


