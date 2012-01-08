/*
 * Slave Mode Status Master
 *
 * Copyright (c) 2006 Reciva Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 * Description:
 * GPIO pins claimed by this module:
 * GPG5 : nMRDY. Module Ready (active low)
 * GPG7 : nINTERRUPT. Attention Request (active low)
 * 
 * Slave mode status signals. Poll to get current status.
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

#include "reciva_slave_mode_status_master.h"
#include "reciva_util.h"
#include "reciva_gpio.h"


   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

#define PREFIX "RSMSM:"

/* GPIO registers */
#define GPIO_REG(x) (unsigned int)((volatile unsigned int *)(S3C2410_VA_GPIO + (x)))

/* GPIO port G */
#define GPGCON GPIO_REG(0x60)
#define GPGDAT GPIO_REG(0x64)
#define GPGUP GPIO_REG(0x68)

#define EXTINT1 GPIO_REG(0x8c)
#define EXTINT2 GPIO_REG(0x90)


   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

static char acModuleName[] = "Reciva Slave Mode Status";
static wait_queue_head_t wait_queue;

static int status_pins_changed = 0;
static spinlock_t status_lock = SPIN_LOCK_UNLOCKED;

   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/

/****************************************************************************
 * Returns the current pin status
 ****************************************************************************/
static int get_pin_status(void)
{
  int pin_status = 0;
  int gpgdat = __raw_readl(GPGDAT);

  /* nMRDY (GPG5) */
  if (gpgdat & (1 << 5))
    pin_status |= SMSM_BITFIELD_nMRDY;

  /* nINTERRUPT (GPG7) */
  if (gpgdat & (1 << 7))
    pin_status |= SMSM_BITFIELD_nINTERRUPT;

  return pin_status;
}

/****************************************************************************
 * Handles an interrupt on GPG5 or GPG7
 * Parameters - Standard interrupt handler params. Not used.
 ****************************************************************************/
static void reciva_int_handler(int irq, void *dev, struct pt_regs *regs)
{
  unsigned long flags;
  spin_lock_irqsave(&status_lock, flags);
  status_pins_changed = 1;
  spin_unlock_irqrestore(&status_lock, flags);
  wake_up_interruptible(&wait_queue);
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

  if (status_pins_changed)
    return POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM;
  else
    return 0;
}

/****************************************************************************
 * Read data from device
 ****************************************************************************/
static int 
reciva_read (struct file *filp, char *buffer, size_t count, loff_t *ppos)
{
  /* Do nothing */
  return count;
}

/****************************************************************************
 * Write data to device
 ****************************************************************************/
static int 
reciva_write (struct file *filp, const char *buf, size_t count, loff_t *f_os)
{
  /* Do nothing */
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
  int status = 0;
  unsigned long flags;

  switch(cmd)
  {
    case IOC_SMSM_GET_STATUS:
      spin_lock_irqsave(&status_lock, flags);
      status_pins_changed = 0;
      status = get_pin_status();
      spin_unlock_irqrestore(&status_lock, flags);
      if (put_user (status, (int *)arg))
        return -EFAULT;
      break;

    case IOC_SMSM_OK_TO_SEND:
      status = !(get_pin_status() & SMSM_BITFIELD_nMRDY);
      if (put_user (status, (int *)arg))
        return -EFAULT;
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
  "reciva_smsm",
  &reciva_fops
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

  /* Register the device */
  misc_register (&reciva_miscdev);

  /* Tell GPIO module which GPIO pins we are using */
  rgpio_register("GPG5 (nMRDY)", acModuleName);
  rgpio_register("GPG7 (nINTERRUPT)", acModuleName);

  /* Setup EINT13, EINT15 - both edge triggered */
  rutl_regwrite((0xf << 20), (0xf << 20), EXTINT1); // EINT13
  rutl_regwrite((0xf << 28), (0xf << 28), EXTINT1);  // EINT15

  /* GPG5 - EINT13 */
  rutl_regwrite((0 << 5), (1 << 5), GPGUP) ;   // Enable pullup
  rutl_regwrite((2 << 10), (3 << 10), GPGCON); // EINTn

  /* GPG7 - EINT15 */
  rutl_regwrite((0 << 7), (1 << 7), GPGUP) ;   // Enable pullup
  rutl_regwrite((2 << 14), (3 << 14), GPGCON); // EINTn

  /* Request interrupts */
  request_irq(IRQ_EINT13, reciva_int_handler, 0, "EINT13", (void *)13);
  request_irq(IRQ_EINT15, reciva_int_handler, 0, "EINT15", (void *)15);

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

  disable_irq (IRQ_EINT13);
  free_irq(IRQ_EINT13, (void *)13);
  disable_irq (IRQ_EINT15);
  free_irq(IRQ_EINT15, (void *)15);
}

module_init(reciva_module_init);
module_exit(reciva_module_exit);

MODULE_LICENSE("GPL");


