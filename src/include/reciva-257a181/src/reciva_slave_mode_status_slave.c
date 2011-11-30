/*
 * Slave Mode Status
 *
 * Copyright (c) 2006 Reciva Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 * Description:
 * GPIO pins claimed by this module:
 * GPC8 : nMRDY. Module Ready (active low)
 * GPC9 : nINTERRUPT. Attention Request (active low)
 * 
 * Control the slave mode status pins
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

#include "reciva_slave_mode_status_slave.h"
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

#define PREFIX "RSMSS:"

/* GPIO registers */
#define GPIO_REG(x) (unsigned int)((volatile unsigned int *)(S3C2410_VA_GPIO + (x)))

/* GPIO port C */
#define GPCCON GPIO_REG(0x20)
#define GPCDAT GPIO_REG(0x24)
#define GPCUP GPIO_REG(0x28)



   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

static char acModuleName[] = "Reciva Slave Mode Status S";



   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/

/****************************************************************************
 * Sets level of nMRDY pin (GPC8)
 ****************************************************************************/
static void set_level_nmrdy(int level)
{
  printk(PREFIX "%s %d\n", __FUNCTION__, level);

  if (level == 0)
    rutl_regwrite((0 << 8), (1 << 8), GPCDAT);    // Clear
  else
    rutl_regwrite((1 << 8), (0 << 8), GPCDAT);    // Set
}

/****************************************************************************
 * Sets level of nINTERRUPT pin (GPC9)
 ****************************************************************************/
static void set_level_ninterrupt(int level)
{
  printk(PREFIX "%s %d\n", __FUNCTION__, level);

  if (level == 0)
    rutl_regwrite((0 << 9), (1 << 9), GPCDAT);    // Clear
  else
    rutl_regwrite((1 << 9), (0 << 9), GPCDAT);    // Set
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
  /* Message available to read */
  return POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM;
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
  set_level_nmrdy(1); // signal that module is no longer active
  return 0;
}

/****************************************************************************
 * Handle command from application
 ****************************************************************************/
static int
reciva_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
  int i;

  printk(PREFIX "ioctl %d\n", cmd);

  switch(cmd)
  {
    case IOC_SMSS_SET_NMRDY:
      if (copy_from_user(&i, (void *)arg, sizeof(i)))
        return -EFAULT;

      set_level_nmrdy(i);
      break;

    case IOC_SMSS_SET_NINTERRUPT:
      if (copy_from_user(&i, (void *)arg, sizeof(i)))
        return -EFAULT;

      set_level_ninterrupt(i);
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
  "reciva_smss",
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

  /* Tell GPIO module which GPIO pins we are using */
  rgpio_register("GPC8 (nMRDY)", acModuleName);
  rgpio_register("GPC9 (nINTERRUPT)", acModuleName);

  /* nMRDY (GPC8). Active low. */
  rutl_regwrite((1 << 8), (0 << 8), GPCDAT);    // Set high (inactive)
  rutl_regwrite((1 << 8), (0 << 8), GPCUP);     // Disable pullup
  rutl_regwrite((1 << 16), (3 << 16), GPCCON);  // Set as output

  /* nINTERRUPT (GPC9). Active low. */
  rutl_regwrite((1 << 9), (0 << 9), GPCDAT);   // Set high (inactive)
  rutl_regwrite((1 << 9), (0 << 9), GPCUP);    // Disable pullup
  rutl_regwrite((1 << 18), (3 << 18), GPCCON); // Set as output

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

module_init(reciva_module_init);
module_exit(reciva_module_exit);

MODULE_LICENSE("GPL");


