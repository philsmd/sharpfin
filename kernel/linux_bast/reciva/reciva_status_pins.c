/*
 * linux/reciva/reciva_status_pins.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2005 Reciva Ltd. All Rights Reserved
 * 
 * Allows application to control some GPIO pins usualy used for the LCD
 * Intended to be used when LCD is not plugged in to indicate status
 * information
 *
 * Version 1.0 2005-04-14  John Stirling <js@reciva.com>
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/smp_lock.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/io.h>


#include "reciva_util.h"
#include "reciva_status_pins.h"

   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

static int reciva_ioctl ( struct inode *inode, struct file *file,
                          unsigned int cmd, unsigned long arg);

   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

/* GPIO registers */
//#define GPIO_REG(x) *((volatile unsigned int *)(S3C2410_VA_GPIO + (x)))
#define GPIO_REG(x) (unsigned int)((volatile unsigned int *)(S3C2410_VA_GPIO + (x)))

/* GPIO port A */
#define GPACON GPIO_REG(0x00)
#define GPADAT GPIO_REG(0x04)
#define GPAUP GPIO_REG(0x08)
/* GPIO port B */
#define GPBCON GPIO_REG(0x10)
#define GPBDAT GPIO_REG(0x14)
#define GPBUP GPIO_REG(0x18)
/* GPIO port C */
#define GPCCON GPIO_REG(0x20)
#define GPCDAT GPIO_REG(0x24)
#define GPCUP GPIO_REG(0x28)
/* GPIO port G */
#define GPGCON GPIO_REG(0x60)
#define GPGDAT GPIO_REG(0x64)
#define GPGUP GPIO_REG(0x68)
/* GPIO port H */
#define GPHCON GPIO_REG(0x70)
#define GPHDAT GPIO_REG(0x74)
#define GPHUP GPIO_REG(0x78)

   

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

  /* Set up the gpio pins */

  /* Set up data */
  rutl_regwrite(0x00000000, // Set
                0x00000700, // Clear
                GPCDAT);
  
  /* Disable pullups */
  rutl_regwrite(0x00000700, // Set
                0x00000000, // Clear
                GPCUP);

  /* Set as ouput */
  rutl_regwrite(0x00150000, // Set
                0x00000000, // Clear
                GPCCON);

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
      unsigned int bits_to_set = (temp & 0x07) << 8;
      unsigned int bits_to_clear = (~bits_to_set) & 0x00000700;
      rutl_regwrite(bits_to_set,   // Set
                    bits_to_clear, // Clear
                    GPCDAT);

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


