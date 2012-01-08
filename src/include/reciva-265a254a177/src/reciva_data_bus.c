/*
 * Reciva general purpose data bus
 *
 * Copyright (c) 2006 Reciva Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 * Description:
 * General purpose read and write via data bus. 
 * Chip select is controlled elsewhere
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
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#else
  #include <asm/arch-bast/param.h>
#endif
#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/arch/regs-gpio.h>
#include <asm/byteorder.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>
#include <asm/io.h>
#include <asm/mach-types.h>

#include "reciva_data_bus.h"

   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

/* Debug prefix */
#define PREFIX "RDB:"


   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

static char acModuleName[] = "Reciva Data Bus";

// 0 - CLOCK = J1-16 (GPB0)
//     DATA  = J1-18 (GPB7)
//     CS    = J2-7  (GPG7/2)
// 1 - CLOCK = J1-9  (GPE7)
//     DATA  = J1-7  (GPE8)
//     CS    = none
// 2 - CLOCK = J1-5  (GPE9)
//     DATA  = J1-7  (GPE8)
//     CS    = J1-9  (GPE7)
typedef enum
{
  RDB_BOARD_TYPE0           = 0,
  RDB_BOARD_TYPE1           = 1,
  RDB_BOARD_TYPE2           = 2,

} board_type_t;
static board_type_t board_type;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
module_param(board_type, uint, S_IRUGO);
#else
MODULE_PARM(board_type, "i");
#endif

/* Enable extra debug output */
static int enable_debug;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
module_param(enable_debug, int, S_IRUGO);
#else
MODULE_PARM(enable_debug, "i");
#endif

/* Number of microseconds to delay after setting level on each pin
 * s3c2410_gpio_setpin reads current value of data pins when doing a single
 * bit write so need to give time for any previous write to take effect
 * when doing back to back writes on individual pins */
static int pin_set_delay_us;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
module_param(pin_set_delay_us, uint, S_IRUGO);
#else
MODULE_PARM(pin_set_delay_us, "i");
#endif

static int clock_pin;
static int data_pin;
static int cs_pin;


   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/

/****************************************************************************
 * Common gpio setup
 ****************************************************************************/
static void setup_gpio(void)
{
  /* CLOCK */
  s3c2410_gpio_setpin (clock_pin, 0);
  s3c2410_gpio_cfgpin (clock_pin, S3C2410_GPIO_OUTPUT);
  s3c2410_gpio_pullup (clock_pin, 1); // disable pullup

  /* DATA */
  s3c2410_gpio_pullup (data_pin, 1); // disable pullup
  
  /* CS - active low  */
  if (cs_pin != -1)
  {
    s3c2410_gpio_setpin (cs_pin, 1);
    s3c2410_gpio_cfgpin (cs_pin, S3C2410_GPIO_OUTPUT);
    s3c2410_gpio_pullup (cs_pin, 1); // disable pullup
  }
}

/****************************************************************************
 * Setup gpio for writing
 ****************************************************************************/
static void setup_gpio_for_write(void)
{
  s3c2410_gpio_cfgpin (data_pin, S3C2410_GPIO_OUTPUT);
}

/****************************************************************************
 * Setup gpio for reading
 ****************************************************************************/
static void setup_gpio_for_read(void)
{
  s3c2410_gpio_cfgpin (data_pin, S3C2410_GPIO_INPUT);
}

/****************************************************************************
 * Get level of data pin
 ****************************************************************************/
static int get_level_data(void)
{
  return s3c2410_gpio_getpin (data_pin);
}

/****************************************************************************
 * Set level of data pin
 ****************************************************************************/
static void set_level_data(int level)
{
  s3c2410_gpio_setpin (data_pin, level);
  udelay(pin_set_delay_us);
}

/****************************************************************************
 * Set level of clock pin
 ****************************************************************************/
static void set_level_clock(int level)
{
  s3c2410_gpio_setpin (clock_pin, level);
  udelay(pin_set_delay_us);
}

/****************************************************************************
 * Read a byte from the device
 ****************************************************************************/
static unsigned char read_byte(void)
{
  unsigned char data = 0;
  int i;

  for (i=0; i<8; i++)
  {
    set_level_clock(0);
    set_level_clock(1);

    if (get_level_data())
      data |= 0x01;

    data <<= 1;
  }

  set_level_clock(0);

  return data;
}

/****************************************************************************
 * Read data from device
 * dst = destination
 * count = number of bytes to read
 ****************************************************************************/
static int read_data(char *dst, int count)
{
  int i;

  setup_gpio_for_read();

  for (i=0; i<count; i++)
    *dst++ = read_byte();

  return count;
}

/****************************************************************************
 * Write a single byte to the device
 * Data clocked out on rising edge of clock
 ****************************************************************************/
static void write_byte(char data)
{
  int i;

  if (enable_debug)
    printk(PREFIX "  %s d=%02x\n", __FUNCTION__, data);

  setup_gpio_for_write();

  for (i=0; i<8; i++)
  {
    set_level_clock(0);

    if (data & 0x80)
      set_level_data(1);
    else
      set_level_data(0);

    set_level_clock(1);

    data <<= 1;
  }

  set_level_clock(0);
}

/****************************************************************************
 * Write data to the device
 * src - source
 * count = number of bytes to write
 ****************************************************************************/
static void write_data(char *src, int count)
{
  if (enable_debug)
    printk(PREFIX "%s c=%d\n", __FUNCTION__, count);

  if (cs_pin != -1)
    s3c2410_gpio_setpin (cs_pin, 0);

  while (count--)
    write_byte(*src++);

  if (cs_pin != -1)
    s3c2410_gpio_setpin (cs_pin, 1);
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
  /* Message always available to read */
  return POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM;
}

/****************************************************************************
 * Read data from device
 ****************************************************************************/
static int 
reciva_read (struct file *filp, char *buffer, size_t count, loff_t *ppos)
{
  char temp_buf[count];
  int bytes_read = 0;
  
  /* Copy data into temp buffer */
  bytes_read = read_data(temp_buf, count);

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
reciva_write (struct file *filp, const char *buf, size_t count, loff_t *f_os)
{
  char temp_buf[count+1];
  copy_from_user(temp_buf, buf, count);
  temp_buf[count] = 0;

  write_data(temp_buf, count);

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
  "reciva_data_bus",
  &reciva_fops
};

   /*************************************************************************/
   /***                        File Operations - END                      ***/
   /*************************************************************************/

/****************************************************************************
 * Write data to the device
 * src - source
 * count = number of bytes to write
 ****************************************************************************/
void reciva_data_bus_write(char *src, int count)
{
  write_data(src,count);
}


/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
reciva_module_init(void)
{
  printk(PREFIX "%s module: loaded\n", acModuleName);
  printk(PREFIX "  board_type=%d\n", board_type);
  printk(PREFIX "  pin_set_delay_us=%d\n", pin_set_delay_us);

  switch (board_type)
  {
    case RDB_BOARD_TYPE0:
      printk(PREFIX "  CLOCK = J1-16 (GPB0)\n");
      printk(PREFIX "  DATA  = J1-18 (GPB7)\n");
      printk(PREFIX "  CS    = J2-7  (GPG7/2)\n");
      clock_pin = S3C2410_GPB0;
      data_pin = S3C2410_GPB7;
      cs_pin = machine_is_rirm2() ? S3C2410_GPG7 : S3C2410_GPG2;
      break;

    case RDB_BOARD_TYPE1:
      printk(PREFIX "  CLOCK = J1-9  (GPE7)\n");
      printk(PREFIX "  DATA  = J1-7  (GPE8)\n");
      clock_pin = S3C2410_GPE7;
      data_pin = S3C2410_GPE8;
      cs_pin = -1;
      break;

    case RDB_BOARD_TYPE2:
      printk(PREFIX "  CLOCK = J1-5  (GPE9)\n");
      printk(PREFIX "  DATA  = J1-7  (GPE8)\n");
      printk(PREFIX "  CS    = J1-9  (GPE7)\n");
      clock_pin = S3C2410_GPE9;
      data_pin = S3C2410_GPE8;
      cs_pin = S3C2410_GPE7;
      break;

  }

  setup_gpio();

  /* Register the device */
  misc_register (&reciva_miscdev);

  return 0;
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
}
 
EXPORT_SYMBOL(reciva_data_bus_write);

module_init(reciva_module_init);
module_exit(reciva_module_exit);

MODULE_LICENSE("GPL");


