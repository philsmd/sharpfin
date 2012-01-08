/*
 * Reciva I2C master for s3c2410 (for unit testing only)
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

#include "reciva_i2c_master.h"
#include "reciva_util.h"
#include "reciva_gpio.h"


   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

typedef enum 
{
  /* No transfer currently in progress */
  RI2C_STATE_IDLE,

  /* Master Write currently in progress */
  RI2C_STATE_WRITE,

  /* First master read is in progress (to determine the length of the
   * following read) */
  RI2C_STATE_READ1,
  RI2C_STATE_READ1A,

  /* Second master read is in progress (data) */
  RI2C_STATE_READ2,
  RI2C_STATE_READ2A,

} state_t;


   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/



   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

#define PREFIX "RI2CMU:"

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

#define IICCON_PRESCALER 0x0f

#define MODULE_NAME "Reciva I2C Master UnitTest"

   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

static char acModuleName[] = MODULE_NAME;
static wait_queue_head_t wait_queue;

/* Tx buffer */
#define BUF_SIZE 200
static char write_buffer[BUF_SIZE];
static int write_length = 0;
static int write_index = 0;
static int write_in_progress = 0;

static state_t current_state = RI2C_STATE_IDLE;


/* Rx buffer */
static char rx_buffer[BUF_SIZE];

/* Expected number of bytes in the current Master Rx transfer */
static int rx_length; 

/* Count of rx bytes in the current Master Rx transfer */
static int rx_count = 0; 


   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/

/****************************************************************************
 * Returns a string containing some I2C register values
 ****************************************************************************/
static char *reg_debug_string(void)
{
  static char temp[200];
  int iiccon = __raw_readl(IICCON);
  int iicstat = __raw_readl(IICSTAT);
  int iicds = __raw_readl(IICDS);
  int iicadd = __raw_readl(IICADD);

  sprintf(temp, "(iiccon=%08x iicstat=%08x iicadd=%08x iicds=%08x)", 
            iiccon, iicstat, iicadd, iicds);

  return temp;
}

/****************************************************************************
 * Start a MASTER WRITE
 ****************************************************************************/
static void master_write(const char *data, int count)
{
  memcpy(write_buffer, data, count);
  write_length = count;
  write_index = 0;
  write_in_progress = 1;
  current_state = RI2C_STATE_WRITE;

  __raw_writel (0xd0, IICSTAT);
  __raw_writel (0xa0 | IICCON_PRESCALER, IICCON);

  /* Set up data here */
  __raw_writel (0x66, IICDS);   // slave address
  __raw_writel (0xf0, IICSTAT); // start
}

/****************************************************************************
 * Start a MASTER READ
 ****************************************************************************/
static void master_read(void)
{
  printk(PREFIX "master_read IN %s\n", reg_debug_string());
//  __raw_writel (0xd0, IICSTAT);
  __raw_writel (0xd0, IICSTAT);
  __raw_writel (0xa0 | IICCON_PRESCALER, IICCON);

  /* Set up data here */
  __raw_writel (0x66, IICDS);   // slave address
  __raw_writel (0xb0, IICSTAT); // start
  printk(PREFIX "master_read OUT %s\n", reg_debug_string());
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
  if (rx_count)
  {  
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
  printk(PREFIX "read c=%d rc=%d\n", count, rx_count);
  int ret;
  int length = count;

  /* Copy data to user application
   * Ignore the offset */
  if (count > rx_count)
    length = rx_count;

  if (copy_to_user (buffer, rx_buffer, length))
    ret = -EFAULT;
  else
    ret = length;

  rx_count=0;
  return ret;
}

/****************************************************************************
 * Write data to device
 ****************************************************************************/
static int 
reciva_write (struct file *filp, const char *buf, size_t count, loff_t *f_os)
{
  printk(PREFIX "write c=%d\n", count);
  master_write(buf, count);
  return count;
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
  printk(PREFIX "release\n");
  return 0;
}

/****************************************************************************
 * Handle command from application
 ****************************************************************************/
static int
reciva_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
  int empty;

  switch(cmd)
  {
    /* Get status of TX buffer
     * 1 indicates buffer is empty */
    case IOC_I2C_GET_TX_BUFFER_EMPTY:
    {
      if (write_in_progress == 0)
        empty = 1;
      else
        empty = 0;

      if (put_user(empty, (int *)arg))
        return -EFAULT;
    }
    break;

    case IOC_I2C_READ_REQ:
      master_read();
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
  "reciva_i2c_master",
  &reciva_fops
};

   /*************************************************************************/
   /***                        File Operations - END                      ***/
   /*************************************************************************/
            
/****************************************************************************
 * Handle interrupt when in state WRITE
 ****************************************************************************/
static void handle_irq_write(void)
{
  int iiccon = __raw_readl(IICCON);
  int iicstat = __raw_readl(IICSTAT);
  int iicds = __raw_readl(IICDS);

  printk(PREFIX "handle_irq_write\n");
  printk(PREFIX "  IRQ IICCON=0x%08x IICSTAT=0x%08x IICDS=0x%08x\n", 
                                                 iiccon, 
                                                 iicstat,
                                                 iicds);

  if (write_length && write_index < write_length)
  {
    printk(PREFIX "WRITE wl=%d wi=%d d=%02x\n", write_length, write_index, write_buffer[write_index]);
    __raw_writel (write_buffer[write_index++], IICDS);
    __raw_writel (0xa0 | IICCON_PRESCALER, IICCON); // XXX 0xa0
  }
  else
  {
    /* STOP */
    printk(PREFIX "STOP\n");
    printk(PREFIX "  wl=%d wi=%d d=%02x\n", write_length, write_index, write_buffer[write_index]);
    __raw_writel (0xc0, IICSTAT);
    __raw_writel (0xa0 | IICCON_PRESCALER, IICCON);

    write_in_progress = 0;

    /* Kick off the 1st Master Read (to establish the length of data available) */
    current_state = RI2C_STATE_READ1;
    master_read();
  }
}

/****************************************************************************
 * Handle interrupt when in state READ1
 ****************************************************************************/
static void handle_irq_read1(void)
{
  int iiccon = __raw_readl(IICCON);
  int iicstat = __raw_readl(IICSTAT);
  int iicds = __raw_readl(IICDS);

  printk(PREFIX "handle_irq_read1\n");
  printk(PREFIX "  IRQ IICCON=0x%08x IICSTAT=0x%08x IICDS=0x%08x\n", 
                                                 iiccon, 
                                                 iicstat,
                                                 iicds);

  current_state = RI2C_STATE_READ1A;
  rutl_regwrite(0, (1 << 4), IICCON); // Clear pending bit
}

/****************************************************************************
 * Handle interrupt when in state READ1
 ****************************************************************************/
static void handle_irq_read1a(void)
{
  int iiccon = __raw_readl(IICCON);
  int iicstat = __raw_readl(IICSTAT);
  int iicds = __raw_readl(IICDS);

  printk(PREFIX "handle_irq_read1a\n");
  printk(PREFIX "  IRQ IICCON=0x%08x IICSTAT=0x%08x IICDS=0x%08x\n", 
                                                 iiccon, 
                                                 iicstat,
                                                 iicds);

  rx_length = iicds; 
  rx_count = 0; 

  printk(PREFIX "  STOP (Data Length=%d)\n", iicds);
  __raw_writel (0x80, IICSTAT); // STOP
  rutl_regwrite(0, (1 << 4), IICCON);  // Clear pending bit

  /* Kick off the 2nd Master Read (to read the actual data) */
  current_state = RI2C_STATE_READ2;
  master_read();
}

/****************************************************************************
 * Handle interrupt when in state READ2
 ****************************************************************************/
static void handle_irq_read2(void)
{
  int iiccon = __raw_readl(IICCON);
  int iicstat = __raw_readl(IICSTAT);
  int iicds = __raw_readl(IICDS);

  printk(PREFIX "handle_irq_read2\n");
  printk(PREFIX "  IRQ IICCON=0x%08x IICSTAT=0x%08x IICDS=0x%08x\n", 
                                                 iiccon, 
                                                 iicstat,
                                                 iicds);

  current_state = RI2C_STATE_READ2A;
  rutl_regwrite(0, (1 << 4), IICCON); // Clear pending bit
}

/****************************************************************************
 * Handle interrupt when in state READ2
 ****************************************************************************/
static void handle_irq_read2a(void)
{
  int iiccon = __raw_readl(IICCON);
  int iicstat = __raw_readl(IICSTAT);
  int iicds = __raw_readl(IICDS);

  printk(PREFIX "handle_irq_read2a rc=%02x rl=%02x d=%c\n", rx_count, rx_length, iicds);
  printk(PREFIX "  IRQ IICCON=0x%08x IICSTAT=0x%08x IICDS=0x%08x\n", 
                                                 iiccon, 
                                                 iicstat,
                                                 iicds);

  rx_buffer[rx_count] = iicds;
  rx_count++; 
  if (rx_count < rx_length)
  {
    rutl_regwrite(0, (1 << 4), IICCON); // Clear pending bit
  }
  else
  {
    printk(PREFIX "  STOP\n");
    current_state = RI2C_STATE_IDLE;
    __raw_writel (0x80, IICSTAT);       // STOP
    rutl_regwrite(0, (1 << 4), IICCON); // Clear pending bit

    /* And inform app that new data has arrived */
    wake_up_interruptible(&wait_queue);
  }
}


/****************************************************************************
 * I2C interrupt handler
 ****************************************************************************/
static void reciva_i2c_interrupt (int irq, void *dev_id, struct pt_regs *regs)
{
  printk(PREFIX "IRQ");
  switch (current_state)
  {
    case RI2C_STATE_IDLE:
      break;

    case RI2C_STATE_WRITE:
      handle_irq_write();
      break;

    case RI2C_STATE_READ1:
      handle_irq_read1();
      break;
    case RI2C_STATE_READ1A:
      handle_irq_read1a();
      break;

    case RI2C_STATE_READ2:
      handle_irq_read2();
      break;
    case RI2C_STATE_READ2A:
      handle_irq_read2a();
      break;
  }
}

/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
reciva_module_init(void)
{
  printk(PREFIX "%s module: loaded\n", acModuleName);
  int ret = 0;

  init_waitqueue_head (&wait_queue);

  /* Register the device */
  misc_register (&reciva_miscdev);

  /* Set up GPIO pins as SDA, SCL */
  /* GPE14 */
  rutl_regwrite((0 << 14), (1 << 14), GPEUP) ; // Enable pullup
  rutl_regwrite((2 << 28), (3 << 28), GPECON); // Set as I2C SCL
  /* GPE15 */
  rutl_regwrite((0 << 15), (1 << 15), GPEUP) ; // Enable pullup
  rutl_regwrite((2 << 30), (3 << 30), GPECON); // Set as I2C SDA

  /* Enable irq and ack generation, prescaler = 0x0f */
  __raw_writel (0xa0 | IICCON_PRESCALER, IICCON);

  /* Master Tx mode  */
  __raw_writel (0xc0, IICSTAT);

  /* Request I2C interrupt */
  request_irq(IRQ_IIC, 
              reciva_i2c_interrupt, 
              0,
              "reciva_i2c", 
              (void *)DEV_ID);

  return ret;
}

/****************************************************************************
 * Module tidy up
 ****************************************************************************/
static void __exit 
reciva_module_exit(void)
{
  printk(PREFIX "%s module: unloaded\n", acModuleName);

  /* Unregister the device */
  misc_deregister(&reciva_miscdev);

  disable_irq (IRQ_IIC);
  free_irq(IRQ_IIC, (void *)DEV_ID);

  __raw_writel (0xc0, IICSTAT);
  __raw_writel (0xa0 | IICCON_PRESCALER, IICCON);
}


module_init(reciva_module_init);
module_exit(reciva_module_exit);

MODULE_LICENSE("GPL");


