/*
 * Reciva Audio Muting Control.
 * Copyright (c) 2004 Reciva Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
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

#include "reciva_audio_mute.h"
#include "reciva_util.h"
#include "reciva_gpio.h"

/* This module takes a parameter to define the audio mute pin
 * 0 = no audio mute pin
 * 1 = GPG3
 * 2 = GPC1
 * 3 = GPC3 */
static int mute_pin = 1;
MODULE_PARM(mute_pin, "i");



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

static char acModuleName[] = "Reciva Audio Mute";

/* IOCTL related */
static struct file_operations reciva_fops =
{
  owner:    THIS_MODULE,
  ioctl:    reciva_ioctl,
};
static struct miscdevice reciva_miscdev =
{
  MISC_DYNAMIC_MINOR,
  "audio_mute",
  &reciva_fops
};


   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/


/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
reciva_audio_mute_init(void)
{
  printk("RAM:%s module: loaded mute_pin=%d\n", acModuleName, mute_pin);

  /* Tell GPIO module which GPIO pins we are using */
  switch (mute_pin)
  {
    case 0:
    default:
      /* No hardware mute pin */
      break;
    case 1:
      rgpio_register("GPG3 (AUDIO_MUTE)", acModuleName);
      break;
    case 2:
      rgpio_register("GPC1 (AUDIO_MUTE)", acModuleName);
      break;
    case 3:
      rgpio_register("GPC3 (AUDIO_MUTE)", acModuleName);
      break;
  }

  /* Register the IOCTL */
  misc_register (&reciva_miscdev);
  
  /* Set up data first - enable mute */
  reciva_audio_mute(1);

  /* .. then enable pin as output */  
  switch (mute_pin)
  {
    case 0:
    default:
      /* No hardware mute pin */
      break;
    case 1:
      // GPG3
      rutl_regwrite((1 << 6), (3 << 6), GPGCON);
      break;
    case 2:
      // GPC1
      rutl_regwrite((1 << 2), (3 << 2), GPCCON);
      break;
    case 3:
      // GPC1
      rutl_regwrite((1 << 6), (3 << 6), GPCCON);
      break;
  }
  
  return 0;
}

/****************************************************************************
 * Module tidy up
 ****************************************************************************/
static void __exit 
reciva_audio_mute_exit(void)
{
  printk("RAM:%s module: unloaded\n", acModuleName);
  misc_deregister (&reciva_miscdev);
  reciva_audio_mute(0);
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
    case IOC_AUDIO_MUTE:
      if (copy_from_user(&i, (void *)arg, sizeof(i)))
        return -EFAULT;

      reciva_audio_mute(i);
      break;

    default:
      return -ENODEV;
  }

  return 0;
}

/****************************************************************************
 * Controls hardware audio mute signal
 ****************************************************************************/
void reciva_audio_mute(int on)
{
  switch (mute_pin)
  {
    case 0:
    default:
      /* No hardware mute pin */
      break;
    case 1:
      // GPG3
      if (on)
        rutl_regwrite((1 << 3), (0 << 3), GPGDAT);   // Enable mute
      else
        rutl_regwrite((0 << 3), (1 << 3), GPGDAT);   // Disable mute
      break;
    case 2:
      // GPC1
      if (on)
        rutl_regwrite((1 << 1), (0 << 1), GPCDAT);   // Enable mute
      else
        rutl_regwrite((0 << 1), (1 << 1), GPCDAT);   // Disable mute
      break;
    case 3:
      // GPC3
      if (on)
        rutl_regwrite((1 << 3), (0 << 3), GPCDAT);   // Enable mute
      else
        rutl_regwrite((0 << 3), (1 << 3), GPCDAT);   // Disable mute
      break;
  }
}  



   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/

EXPORT_SYMBOL(reciva_audio_mute);

module_init(reciva_audio_mute_init);
module_exit(reciva_audio_mute_exit);

MODULE_LICENSE("GPL");

