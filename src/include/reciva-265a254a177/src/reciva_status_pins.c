/*
 * linux/reciva/reciva_status_pins.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2005, 2008 Reciva Ltd. All Rights Reserved
 * 
 * Allows application to control some GPIO pins usualy used for the LCD
 * Intended to be used when LCD is not plugged in to indicate status
 * information
 *
 * Version 1.0 2005-04-14  John Stirling <js@reciva.com>
 *
 */

#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/i2c.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>

   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

#define LCD_IOCTL_BASE  'L'
#define IOC_RSP_SET     _IOW(LCD_IOCTL_BASE, 1, int)

   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

static int reciva_ioctl ( struct inode *inode, struct file *file,
                          unsigned int cmd, unsigned long arg);

   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

static char acModuleName[] = "Reciva Status Pins";

static struct file_operations reciva_fops =
{
  owner:    THIS_MODULE,
  ioctl:    reciva_ioctl,
};

static struct miscdevice reciva_miscdev =
{
  MISC_DYNAMIC_MINOR,
  "rsp",
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
  printk("RSP:%s module: loaded\n", acModuleName);

  /* Set up GP8-10 as outputs driver low */
  s3c2410_gpio_setpin(S3C2410_GPC8, 0);
  s3c2410_gpio_setpin(S3C2410_GPC9, 0);
  s3c2410_gpio_setpin(S3C2410_GPC10, 0);
  s3c2410_gpio_cfgpin(S3C2410_GPC8, S3C2410_GPIO_OUTPUT);
  s3c2410_gpio_cfgpin(S3C2410_GPC9, S3C2410_GPIO_OUTPUT);
  s3c2410_gpio_cfgpin(S3C2410_GPC10, S3C2410_GPIO_OUTPUT);

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
  printk("RSP:%s module: unloaded\n", acModuleName);
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
  int temp;

  switch(cmd)
  {
    case IOC_RSP_SET:
      if (copy_from_user(&temp, (void *)arg, sizeof(temp)))
        return -EFAULT;

      // Set the pins
			s3c2410_gpio_setpin(S3C2410_GPC8, temp & 1);
			s3c2410_gpio_setpin(S3C2410_GPC9, (temp & 2) >> 1);
			s3c2410_gpio_setpin(S3C2410_GPC10, (temp & 4) >> 2);
      break;

    default:
      return -ENODEV;
  }

  return 0;
}

   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/

module_init(reciva_init);
module_exit(reciva_exit);

MODULE_AUTHOR("John Stirling <js@reciva.com>");
MODULE_DESCRIPTION("Reciva Status Pins driver");
MODULE_LICENSE("GPL");


