/*
 * Reciva USB
 * Copyright (c) 2006 Reciva Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 *
 * Description:
 * USB configuration and power control
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/slab.h>

#include <asm/delay.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>

#include "reciva_usb.h"
#include "reciva_util.h"
#include "reciva_gpio.h"


/* Determines if we're a USB host or device */
typedef enum
{
  RUSB_CONFIG_DEVICE = 0,
  RUSB_CONFIG_HOST = 1,
} usb_config_t;
static usb_config_t usb_config = RUSB_CONFIG_DEVICE;
MODULE_PARM(usb_config, "i");



   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

static int reciva_ioctl ( struct inode *inode, struct file *file,
                          unsigned int cmd, unsigned long arg);
static void set_power(int on);

                             
   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

/* Debug prefix */
#define PREFIX "RUSB:"

/* GPIO registers */
#define GPIO_REG(x) (unsigned int)((volatile unsigned int *)(S3C2410_VA_GPIO + (x)))

/* Misc Control Register MISCCR */
#define MISCCR GPIO_REG(0x80)


   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

static char acModuleName[] = "Reciva USB";

/* IOCTL related */
static struct file_operations reciva_fops =
{
  owner:    THIS_MODULE,
  ioctl:    reciva_ioctl,
};
static struct miscdevice reciva_miscdev =
{
  MISC_DYNAMIC_MINOR,
  "reciva_usb",
  &reciva_fops
};


   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/


/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
reciva_init(void)
{
  printk(PREFIX "%s module: loaded usb_config=%d\n", acModuleName, usb_config);

  /* Register the IOCTL */
  misc_register (&reciva_miscdev);
  
  /* Tell GPIO module which GPIO pins we are using */
  switch (usb_config)
  {
    case RUSB_CONFIG_DEVICE:
      rutl_regwrite((0 << 3),  (1 << 3),  MISCCR); // USBPAD=0
      break;
    case RUSB_CONFIG_HOST:
      rutl_regwrite((1 << 3),  (0 << 3),  MISCCR); // USBPAD=1
      break;
  }

  return 0;
}

/****************************************************************************
 * Module tidy up
 ****************************************************************************/
static void __exit 
reciva_exit(void)
{
  printk("RMC:%s module: unloaded\n", acModuleName);
  misc_deregister (&reciva_miscdev);
}

/****************************************************************************
 * Command handler
 * Parameters : Standard ioctl params
 * Returns : 0 == success, otherwise error code
 ****************************************************************************/
static int
reciva_ioctl ( struct inode *inode, struct file *file,
               unsigned int cmd, unsigned long arg)
{
  int i;
  switch(cmd)
  {
    case IOC_USB_POWER:
      if (copy_from_user(&i, (void *)arg, sizeof(i)))
        return -EFAULT;

      set_power(i);
      break;

    default:
      return -ENODEV;
  }

  return 0;
}

/****************************************************************************
 * Controls the USB power
 ****************************************************************************/
static void set_power(int on)
{
}  



   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/

module_init(reciva_init);
module_exit(reciva_exit);

MODULE_LICENSE("GPL");

