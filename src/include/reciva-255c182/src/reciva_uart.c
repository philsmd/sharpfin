/*
 * Reciva UART for s3c2410
 *
 * Copyright (c) 2006 Reciva Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 * Description:
 * UART using a general purpose IO pin for tx and on chip uart for rx
 * GPH7 (RXD2) for rx
 * GPB9 (general purpose gpio) for tx
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
#include <linux/serial.h>

#include <asm/arch-bast/param.h>
#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/arch/regs-gpio.h>
#include <asm/byteorder.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>
#include <asm/io.h>
#include <asm/arch-s3c2410/S3C2410-timer.h>
#include <asm/arch-s3c2410/S3C2410-irq.h>

#include <asm/hardware/serial_s3c2410.h>

#include "reciva_util.h"
#include "reciva_gpio.h"
#include "reciva_uart.h"


   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

typedef enum
{
  PIN_LEVEL_LOW,
  PIN_LEVEL_HIGH,
} pin_level_t;

typedef enum
{
  STOP_BIT,
  START_BIT,
  DATA,
} NextBit_t;

/* Buffer size needs to be a power of 2 */
#define BUF_SIZE (int)(1<<10)
typedef struct
{
  int rd_index;       // Read index
  int wr_index;       // Write index
  char buf[BUF_SIZE]; 
} buffer_t;


   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

static void set_level_txd(pin_level_t level);
static void clock_bit_out(void);
static void uart_tx_interrupt (int irq, void *dev_id, struct pt_regs *regs);
static void uart_rx_interrupt (int irq, void *dev_id, struct pt_regs *regs);
static void start_timer(void);
static void stop_timer(void);
static int set_baud_rate(int baudrate);
static int set_tx_baud_rate(int baudrate);
static int set_rx_baud_rate(int baudrate);
static void kick_uart_send_ticker(void);
static void process_uart_rx_data(char data);
static void send_msg_to_rx_driver(void);

/* Buffer related */
static void buf_init(buffer_t *b);
static void buf_inc_rd_index(buffer_t *b);
static void buf_inc_wr_index(buffer_t *b);


   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

/* Gets transmitted when module is loaded */
#define TEST_MESSAGE "Reciva Radio\r\n"

/* Default baud rate */
#define DEFAULT_BAUD_RATE 1200

/* Debug prefix */
#define PREFIX "RURT:"

/* Interrupt device ID */
#define DEV_ID 1

/* GPIO registers */
#define GPIO_REG(x) (unsigned int)((volatile unsigned int *)(S3C2410_VA_GPIO + (x)))

/* GPIO port B */
#define GPBCON GPIO_REG(0x10)
#define GPBDAT GPIO_REG(0x14)
#define GPBUP GPIO_REG(0x18)

/* GPIO port H */
#define GPHCON GPIO_REG(0x70)
#define GPHDAT GPIO_REG(0x74)
#define GPHUP GPIO_REG(0x78)

/* UART registers */
#define UART2_BASE           ((void *) (S3C2410_VA_UART + S3C2410_UART2_OFF))
#define UART_REG_ULCON2      (unsigned int)(UART2_BASE + S3C2410_UARTLCON_OFF)
#define UART_REG_UCON2       (unsigned int)(UART2_BASE + S3C2410_UARTCON_OFF)
#define UART_REG_UFCON2      (unsigned int)(UART2_BASE + S3C2410_UARTFCON_OFF)
#define UART_REG_UTRSTAT2    (unsigned int)(UART2_BASE + S3C2410_UARTTRSTAT_OFF)
#define UART_REG_UERSTAT2    (unsigned int)(UART2_BASE + S3C2410_UARTERSTAT_OFF)
#define UART_REG_UFSTAT2     (unsigned int)(UART2_BASE + S3C2410_UARTFSTAT_OFF)
#define UART_REG_UMSTAT2     (unsigned int)(UART2_BASE + S3C2410_UARTMSTAT_OFF)
#define UART_REG_UTHX2       (unsigned int)(UART2_BASE + S3C2410_UARTTXH0_OFF)
#define UART_REG_URHX2       (unsigned int)(UART2_BASE + S3C2410_UARTRXH0_OFF)
#define UART_REG_UBRDIV2     (unsigned int)(UART2_BASE + S3C2410_UARTBRDIV_OFF)



   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

static char acModuleName[] = "Reciva UART";

/* Rx driver */
static const reciva_uart_rx_driver_t *rx_driver = NULL;

/* Read and Write buffers */
static buffer_t write_buffer;
static buffer_t read_buffer;

static NextBit_t nextBit = START_BIT;

static volatile int iTimerRunning = 0;
static wait_queue_head_t wait_queue;


   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/

/****************************************************************************
 * Write data to tx buffer
 * Data starts getting transmitted after this is called
 ****************************************************************************/
int reciva_uart_write_internal(const char *buf, size_t count)
{
  size_t amount_written = 0;

  while ((count-- > 0) && 
         (((write_buffer.wr_index + 1) & (BUF_SIZE-1)) != write_buffer.rd_index))
  {
    write_buffer.buf[write_buffer.wr_index] = buf[amount_written];
    amount_written++;
    buf_inc_wr_index(&write_buffer);
  }

  /* Need to ensure the tick interrupts are enabled */
  if (amount_written > 0)
  { 
    kick_uart_send_ticker();
  }

  return amount_written;
}

/****************************************************************************
 * Register to receive rx data from this module
 ****************************************************************************/
int reciva_uart_register_rx_driver(const reciva_uart_rx_driver_t *driver)
{
  printk(PREFIX "register_rx_driver : d=%p\n", driver);
  rx_driver = driver;
  return 0;
}

/****************************************************************************
 * Indicates if the uart buffer is empty
 ****************************************************************************/
int reciva_uart_tx_buffer_empty(void)
{
  int empty = 0;
  if (write_buffer.wr_index == write_buffer.rd_index)
    empty = 1;

  return empty;
}


   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/


/****************************************************************************
 * Initialise a buffer struct
 ****************************************************************************/
static void buf_init(buffer_t *b)
{
  b->rd_index = 0;
  b->wr_index = 0;
}

/****************************************************************************
 * Increment buffer read and write indexes
 ****************************************************************************/
static void buf_inc_rd_index(buffer_t *b)
{
  b->rd_index++;
  b->rd_index &= (BUF_SIZE-1);
}
static void buf_inc_wr_index(buffer_t *b)
{
  b->wr_index++;
  b->wr_index &= (BUF_SIZE-1);
}

/****************************************************************************
 * Set the level of the TXD pin
 ****************************************************************************/
static void set_level_txd(pin_level_t level)
{
  if (level == PIN_LEVEL_LOW)
    rutl_regwrite((0 << 9), (1 << 9), GPBDAT);    // Clear
  else
    rutl_regwrite((1 << 9), (0 << 9), GPBDAT);    // Set  
}

/****************************************************************************
 * Clock a UART tx bit out
 ****************************************************************************/
static void clock_bit_out(void)
{
  static int bitoffset;

  switch (nextBit)
  {
    case START_BIT:
      set_level_txd(PIN_LEVEL_LOW);
      nextBit = DATA;
      bitoffset = 0;
      break;

    case DATA:
      /* If 'started', we've sent the start bit and need to send the
       * next data bit (LSB first) */
      if (write_buffer.buf[write_buffer.rd_index] & (1 << bitoffset))
      {
        set_level_txd(PIN_LEVEL_HIGH);
      }
      else
      {
        set_level_txd(PIN_LEVEL_LOW);
      }
      bitoffset++;
      if (bitoffset == 8)
      {
        bitoffset = 0;
        nextBit = STOP_BIT;
      }
      break;

    case STOP_BIT:
      set_level_txd(PIN_LEVEL_HIGH);
      nextBit = START_BIT;
      buf_inc_rd_index(&write_buffer);
      break;
  }
}

/****************************************************************************
 * Start the timer used to clock the Tx data bits out
 ****************************************************************************/
static void start_timer(void)
{
  iTimerRunning = 1;
  rutl_regwrite(S3C2410_TCON_T2START | S3C2410_TCON_T2RELOAD, 
                S3C2410_TCON_T2INVERT | S3C2410_TCON_T2MANUALUPD, 
                S3C2410_TCON);
}

/****************************************************************************
 * Stop the timer (tx data bit timer)
 ****************************************************************************/
static void stop_timer(void)
{
  iTimerRunning = 0;
  rutl_regwrite(S3C2410_TCON_T2RELOAD, 
                S3C2410_TCON_T2START | S3C2410_TCON_T2INVERT | S3C2410_TCON_T2MANUALUPD, 
                S3C2410_TCON);
}

/****************************************************************************
 * Tick interrupt at the correct rate for clocking out tx data bits
 ****************************************************************************/
static void uart_tx_interrupt (int irq, void *dev_id, struct pt_regs *regs)
{
  clock_bit_out();
  if (write_buffer.rd_index != write_buffer.wr_index)
  {
    /* There's data in the tx buffer - set the timer going for another tick */
    start_timer();
  }
  else
  {
    /* There's nothing left in the tx buffer */
    stop_timer();
  }
}

/****************************************************************************
 * UART rx interrupt
 ****************************************************************************/
static void uart_rx_interrupt (int irq, void *dev_id, struct pt_regs *regs)
{
  int uerstat2 = __raw_readl(UART_REG_UERSTAT2);
  int ufstat2 = __raw_readl(UART_REG_UFSTAT2);
  int urhx2 = __raw_readl(UART_REG_URHX2);

  if (ufstat2 & 0x0f)
  {
    /* Rx data available */
    if ((uerstat2 & 0x0f) == 0x00)
    {
      /* No errors - process the data */
      process_uart_rx_data((char)urhx2);

      /* This makes poll work */
      wake_up_interruptible(&wait_queue);
    }
    else
    {
      printk("UART ERROR\n");
    }
  }
}

/****************************************************************************
 * Do something with data received from the UART
 ****************************************************************************/
static void process_uart_rx_data(char data)
{
  read_buffer.buf[read_buffer.wr_index] = data;
  buf_inc_wr_index(&read_buffer);

  if (rx_driver && rx_driver->uart_rx)
  {
    if (data == '\r')
    {
      send_msg_to_rx_driver();      
    }
  }      
}

/****************************************************************************
 * Send a message to the rx driver
 ****************************************************************************/
static void send_msg_to_rx_driver(void)
{
  char temp_buf[BUF_SIZE];
  int bytes_written = 0;
  
  while (read_buffer.buf[read_buffer.rd_index] != '\r' &&
         read_buffer.rd_index != read_buffer.wr_index)
  {
    temp_buf[bytes_written] = read_buffer.buf[read_buffer.rd_index];
    bytes_written++;
    buf_inc_rd_index(&read_buffer);    
  }

  /* Discard new lines */
  if (read_buffer.buf[read_buffer.rd_index] == '\r')
    buf_inc_rd_index(&read_buffer);    

  /* Send on to rx driver */
  temp_buf[bytes_written] = 0;
  rx_driver->uart_rx(temp_buf);
}

/****************************************************************************
 * Starts the uart ticker sending bits down the wire if it's not already
 * doing so
 ****************************************************************************/
static void kick_uart_send_ticker(void)
{
  if (!iTimerRunning)
  {
    /* set the timer going for 1 tick */
    start_timer();
  } 
}

   /*************************************************************************/
   /***                        File Operations - START                    ***/
   /*************************************************************************/

/****************************************************************************
 * Returns status of device. Indicates if there is data available to read
 ****************************************************************************/
static unsigned int 
uart_poll (struct file *file, poll_table *wait)
{
  poll_wait(file, &wait_queue, wait);
  if (read_buffer.wr_index != read_buffer.rd_index)
  {
    /* Message available to read */
    return POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM;
  }
  else 
  {
    /* Device is writable */
    return POLLOUT | POLLWRNORM;
  }
  return 0;
}

/****************************************************************************
 * Read data from device
 ****************************************************************************/
static int 
uart_read (struct file *filp, char *buffer, size_t count, loff_t *ppos)
{
  char temp_buf[BUF_SIZE];
  int bytes_read = 0;
  
  /* Copy data into temp buffer */
  while (read_buffer.wr_index != read_buffer.rd_index && 
         bytes_read < count)
  {
    temp_buf[bytes_read] = read_buffer.buf[read_buffer.rd_index];
    buf_inc_rd_index(&read_buffer);
    bytes_read++;
  }

  /* Copy data to user space */
  if (bytes_read)
  {
    if (copy_to_user (buffer, temp_buf, bytes_read))
      return -EFAULT;
  }

  return bytes_read;
}

/****************************************************************************
 * Write data to device
****************************************************************************/
static int 
uart_write (struct file *filp, const char *buf, size_t count, loff_t *f_os)
{
  char temp_buf[count+1];
  copy_from_user(temp_buf, buf, count);
  temp_buf[count] = 0;

  return reciva_uart_write_internal(temp_buf, count);
}

/****************************************************************************
 * Open device. This is the first operation performed on the file structure.
 ****************************************************************************/
static int 
uart_open(struct inode * inode, struct file * file)
{
  return 0;
}

/****************************************************************************
 * Release file structure. Invoked when the file structure is being released
 ****************************************************************************/
static int 
uart_release(struct inode * inode, struct file * file)
{
  return 0;
}

/****************************************************************************
 * Set the tx and rx baud rates
 ****************************************************************************/
static int set_baud_rate(int baudrate)
{
  int ret = 0;
  printk(PREFIX "Set baud rate : %d\n", baudrate);

  if (baudrate > 115200 || baudrate < 0)
  {
    ret = -1;
  }
  else if ((set_tx_baud_rate(baudrate) < 0) || 
           (set_rx_baud_rate(baudrate) < 0))
  {
    ret = -1;
  }

  return ret;
}

/****************************************************************************
 * Set the tx baud rate
 ****************************************************************************/
static int set_tx_baud_rate(int baudrate)
{
  unsigned long tcnt;

  /* TCLK1 not connected.  Divide down PCLK instead.  */
  rutl_regwrite((0<<0), S3C2410_TCFG1_MUX2_MASK, S3C2410_TCFG1);
  tcnt = ((PCLK) / 2) / baudrate;

  /* check to see if timer is within 16bit range... */
  if (tcnt > 0xffff) {
    panic("setup_timer: HZ is too small, cannot configure timer!");
    return -1;
  }

  __raw_writel(tcnt, S3C2410_TCNTB(2));

  /* Ensure timer is stopped... */
  rutl_regwrite(S3C2410_TCON_T2MANUALUPD, 
                S3C2410_TCON_T2INVERT | S3C2410_TCON_T2RELOAD | S3C2410_TCON_T2START, 
                S3C2410_TCON);

  /* Load the tick period into TCNT */
  __raw_writel(tcnt, S3C2410_TCNTB(2));
  __raw_writel(tcnt, S3C2410_TCMPB(2));

  return 0;
}

/****************************************************************************
 * Set the rx baud rate
 ****************************************************************************/
static int set_rx_baud_rate(int baudrate)
{
  int divider = (PCLK/(baudrate*16)) - 1;
  rutl_regwrite(divider & 0x0000ffff,   // Set
                0xffffffff,             // Clear
                UART_REG_UBRDIV2);
  return 0;
}

/****************************************************************************
 * Handle command from application
 ****************************************************************************/
static int
uart_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
  int empty = 0;

  switch(cmd)
  {
    case IOC_UART_SET_BAUD:
    {
      int baudrate;
      if (copy_from_user(&baudrate, (void *)arg, sizeof(int)))
        return -EFAULT;
      if (set_baud_rate(baudrate) < 0)
        return -EINVAL;
    }
    break;

    /* Get status of TX buffer
     * 1 indicates buffer is empty */
    case IOC_UART_GET_TX_BUFFER_EMPTY:
    {
      if (write_buffer.wr_index == write_buffer.rd_index)
        empty = 1;
      else
        empty = 0;

      if (put_user(empty, (int *)arg))
        return -EFAULT;
    }
    break;

    case IOC_UART_CLEAR_RX_BUFFER:
    {
      printk("IOC_UART_CLEAR_RX_BUFFER\n");
      buf_init(&read_buffer);
    }
    break;


    default:
      return -ENODEV;
  }

  return 0;
}

static struct file_operations uart_fops =
{
  owner:    THIS_MODULE,
  ioctl:    uart_ioctl,
  read:     uart_read,
  write:    uart_write,
  poll:     uart_poll,  
  open:     uart_open,
  release:  uart_release,
};

static struct miscdevice uart_miscdev =
{
  MISC_DYNAMIC_MINOR,
  "reciva_uart",
  &uart_fops
};

   /*************************************************************************/
   /***                        File Operations - END                      ***/
   /*************************************************************************/

/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
reciva_module_init(void)
{
  printk(PREFIX "%s module: loaded\n", acModuleName);

  init_waitqueue_head (&wait_queue);

  /* Reset the buffers */
  buf_init(&write_buffer);
  buf_init(&read_buffer);

  /* Tell GPIO module which GPIO pins we are using */
  rgpio_register("GPB9 (TXD)", acModuleName);
  rgpio_register("GPH7 (RXD2)", acModuleName);
  
  /* TXD (GPB9) */
  rutl_regwrite((1 << 9), (0 << 9), GPBDAT);      // Set high
  rutl_regwrite((1 << 9), (0 << 9), GPBUP);       // Disable pullup
  rutl_regwrite((1 << 18), (3 << 18), GPBCON);    // Set as output

  /* RXD (GPH7) */
  rutl_regwrite((1 << 7), (0 << 7), GPHUP);     // Disable pullup
  rutl_regwrite((2 << 14), (3 << 14), GPHCON);  // RXD2

  /* Register the device */
  misc_register (&uart_miscdev);


  /* UART Rx registers - START */

  /* ULCON2 - UART Line Control
   * 8 data bits, 1 stop bit, no parity */ 
  rutl_regwrite(S3C2410_LCON_CS8,     // Set 
                0xffffffff,           // Clear
                UART_REG_ULCON2);     

  /* UCON2 - UART Control
   * [10] : 0 (use PCLK)
   * [9]  : 0 (tx interrupt - pulse interrupt)
   * [8]  : 0 (rx interrupt type - pulse interrupt)
   * [7]  : 1 (enable rx timeout interrupt)
   * [6]  : 0 (don't generate interrupt on errors)
   * [5]  : 0 (loopback disabled)
   * [4]  : 0 (normal stransmit)
   * [3:2]: 00 (Transmit Mode - disable)
   * [1:0]: 01 (Receive Mode - polling or interrupt) */ 
  rutl_regwrite(0x00000081,           // Set 
                0xffffffff,           // Clear
                UART_REG_UCON2);     

  /* UFCON2 - UART FIFO Control
   * disable fifo */ 
  rutl_regwrite(0,                    // Set 
                0xffffffff,           // Clear
                UART_REG_UFCON2);     

  /* UART Rx registers - END */

  nextBit = START_BIT;
  set_baud_rate(DEFAULT_BAUD_RATE);

  /* Request timer interrupt (for tx) */
  request_irq(IRQ_TIMER2, 
              uart_tx_interrupt, 
              SA_INTERRUPT, 
              "TIMER2_UART_TXD", 
              (void *)DEV_ID);

  /* Request UART interrupt (for rx) */
  request_irq(IRQ_S3CUART_RX2, 
              uart_rx_interrupt, 
              0,
              "UART2-RXD", 
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

  /* Free interrupts */
  free_irq(IRQ_TIMER2, (void *)DEV_ID);
  free_irq(IRQ_S3CUART_RX2, (void *)DEV_ID);

  /* Unregister the device */
  misc_deregister(&uart_miscdev);
}
 
EXPORT_SYMBOL(reciva_uart_tx_buffer_empty);
EXPORT_SYMBOL(reciva_uart_write_internal);
EXPORT_SYMBOL(reciva_uart_register_rx_driver);

module_init(reciva_module_init);
module_exit(reciva_module_exit);

MODULE_LICENSE("GPL");


