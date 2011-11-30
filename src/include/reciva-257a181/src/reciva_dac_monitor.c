/*
 * linux/reciva/reciva_dac_monitor.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2004 Reciva Ltd. All Rights Reserved
 * 
 * Detects and corrects DAC errors. Used for ESD workarounds
 *
 * Version 1.0 2003-12-12  John Stirling <js@reciva.com>
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/smp_lock.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/ioctl.h>

#include <asm/delay.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#include "reciva_util.h"
#include "reciva_gpio.h"
#include "reciva_dac_monitor.h"

/* drivers/sound/.c */
extern void reciva_dac_init(void);
extern int  reciva_dac_check_registers(void);


   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

#define PREFIX "RDM:"

#define GPIO_REG(x) (unsigned int)((volatile unsigned int *)(S3C2410_VA_GPIO + (x)))

/* GPIO port B */
#define GPBCON GPIO_REG(0x10)
#define GPBDAT GPIO_REG(0x14)
#define GPBUP GPIO_REG(0x18)
/* GPIO port D */
#define GPDCON	GPIO_REG(0x30)
#define GPDDAT  GPIO_REG(0x34)
#define GPDUP   GPIO_REG(0x38)
/* GPIO port E */
#define GPECON	GPIO_REG(0x40)
#define GPEDAT	GPIO_REG(0x44)
/* GPIO port F */
#define GPFCON	GPIO_REG(0x50)
#define GPFDAT	GPIO_REG(0x54)
/* GPIO port G */
#define GPGCON	GPIO_REG(0x60)
#define GPGDAT	GPIO_REG(0x64)
#define GPGUP   GPIO_REG(0x68)




   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

static char acModuleName[] = "Reciva DAC Monitor";

/* Used to determine the GPIO pin being used for error detection */
typedef enum
{
  DAC_CONFIG_GPG5                       = 5,
  DAC_CONFIG_GPG7                       = 7,
} dac_config_t;
static dac_config_t dac_config = DAC_CONFIG_GPG5;
MODULE_PARM(dac_config, "i");

/* Pin used to detect the error
 * Assume it's going to be one of the GPG pins */
static int gpg_pin;


/****************************************************************************
 * Handle command from application
 ****************************************************************************/
static int
reciva_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
  int temp;
  int gpg;

  switch(cmd)
  {
    /* Check registers and reset DAC if error detected.
     * return 1:error or 0:no error */
    case IOC_DACM_REGISTER_CHECK:
      temp = reciva_dac_check_registers();
      if (put_user(temp, (int *)arg))
        return -EFAULT;
      break;

    /* Check if the error pin is active
     * return 1:error or 0:no error */
    case IOC_DACM_ERROR_PIN_CHECK:
      gpg = __raw_readl(GPGDAT) & (1 << gpg_pin);
      temp = 0;
      if (gpg == 0)
      {
        reciva_dac_init();
        temp = 1;
      }
      if (put_user(temp, (int *)arg))
        return -EFAULT;
      break;

    default:
      return -ENODEV;
  }

  return 0;
}

/* IOCTL related */
static struct file_operations reciva_fops =
{
  owner:    THIS_MODULE,
  ioctl:    reciva_ioctl,
};
static struct miscdevice reciva_miscdev =
{
  MISC_DYNAMIC_MINOR,
  "dac_monitor",
  &reciva_fops
};


/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
reciva_init(void)
{
  printk(PREFIX "%s module: loaded\n", acModuleName);

  switch (dac_config)
  { 
    case DAC_CONFIG_GPG5:
      rgpio_register("GPG5  (input)", acModuleName);
      gpg_pin = 5;
      break;
    case DAC_CONFIG_GPG7:
      rgpio_register("GPG7  (input)", acModuleName);
      gpg_pin = 7;
      break;
  }

  /* GPG - input */
  rutl_regwrite((0 << gpg_pin), (1 << gpg_pin), GPGUP) ;   // Enable pullup
  rutl_regwrite((0 << (gpg_pin*2)), (3 << (gpg_pin*2)), GPGCON); // Input

  /* Register the device */
  misc_register (&reciva_miscdev);

  return 0;
}

/****************************************************************************
 * Module exit
 ****************************************************************************/
static void __exit 
reciva_exit(void)
{
  printk(PREFIX "%s module: unloaded\n", acModuleName);

  /* Unregister the device */
  misc_deregister(&reciva_miscdev);
}


module_init(reciva_init);
module_exit(reciva_exit);

MODULE_AUTHOR("John Stirling <js@reciva.com>");
MODULE_DESCRIPTION("Reciva DAC Monitor");
MODULE_LICENSE("GPL");



