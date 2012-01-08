/*
 * linux/reciva/reciva_gpio_expander_generic.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2007 Reciva Ltd. All Rights Reserved
 * 
 * GPIO expander - generic
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


#include <asm/hardware.h>
#include <asm/arch/regs-gpio.h>
#include <asm/uaccess.h> 

#include "reciva_gpio_expander_generic.h"
#include "reciva_util.h"

   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

static int reciva_ioctl(struct inode *inode, struct file *file,
                        unsigned int cmd, unsigned long arg);

   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

#define PFX "RGEG:"

   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

static char acModuleName[] = "Reciva GPIO Expander Generic";

static struct file_operations reciva_fops =
{
  owner:    THIS_MODULE,
  ioctl:    reciva_ioctl,
};

static struct miscdevice reciva_miscdev =
{
  MISC_DYNAMIC_MINOR,
  "reciva_gpio_expand",
  &reciva_fops
};

static const reciva_rge_driver_t *driver;


/****************************************************************************
 * Register driver
 ****************************************************************************/
int reciva_rge_register_driver(const reciva_rge_driver_t *d)
{
  printk(PFX "register driver: %s\n", d->name);
  driver = d;
  return 0;
}

/****************************************************************************
 * Command handler
 * Parameters : Standard ioctl params
 * Returns : 0 == success, otherwise error code
 ****************************************************************************/
static int
reciva_ioctl(struct inode *inode, struct file *file,
             unsigned int cmd, unsigned long arg)
{
  //printk(PFX "IOCTL: %d %d\n", cmd, IOC_RGE_PIN_WRITE);
  struct reciva_rge_pin_control pc;

  switch (cmd)
  {
    case IOC_RGE_PIN_WRITE:
      //printk(PFX "PIN_WRITE: %p\n", driver);
      if (copy_from_user(&pc, (void *)arg, sizeof(pc)))
      {
        //printk(PFX "EFAULT\n");
        return -EFAULT;
      } 

      if (driver && driver->pin_write)
        driver->pin_write(pc.pin, pc.data);

      return 0;

    default:
      return -ENODEV;
  }

  return 0;
}
  
/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
reciva_init(void)
{
  printk("%s module: loaded\n", acModuleName);

  /* Register the device */
  misc_register (&reciva_miscdev);

  return 0;
}

/****************************************************************************
 * Module tidy up
 ****************************************************************************/
static void __exit 
reciva_exit(void)
{
  printk("%s module: unloaded\n", acModuleName);
  misc_deregister (&reciva_miscdev);
}


module_init(reciva_init);
module_exit(reciva_exit);

EXPORT_SYMBOL(reciva_rge_register_driver);

MODULE_AUTHOR("John Stirling <js@reciva.com>");
MODULE_DESCRIPTION("Reciva GPIO Expander Generic");
MODULE_LICENSE("GPL");


