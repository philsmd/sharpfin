/*
 * linux/reciva/reciva_mosa_ms6335_monitor.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2004 Reciva Ltd. All Rights Reserved
 * 
 * Monitors tha status of the mosa_ms6336 DAC and reinitialises registers
 * if it detects a problem.
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

#include <asm/delay.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>
#include <asm/io.h>


#include "reciva_util.h"
#include "reciva_gpio.h"

/* drivers/sound/mosa_ms6335.c */
extern void mosa_ms6335_dac_init(void);


   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

#define PREFIX "RMMM:"

#define DEBOUNCE_TIMEOUT        ((HZ*1000)/1000) /* 1 s */

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

#define EXTINT1 GPIO_REG(0x8c)
#define EXTINT2 GPIO_REG(0x90)

/* External interrupt filter */
#define EINTFLT2 GPIO_REG(0x9c)



   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

static char acModuleName[] = "Reciva MOSA MS6335 Monitor";

/* Debounce timer */
static struct timer_list timer;  
static int debouncing = 0;



/****************************************************************************
 * Reset the DAC registers
 ****************************************************************************/
static void reset_dac(void)
{
  printk(PREFIX "RESET DAC\n");
  mosa_ms6335_dac_init();
}

/****************************************************************************
 * Handles the debounce timeout
 ****************************************************************************/
static void debounce_timeout_handler(unsigned long ignore)
{
  int gpg5 = __raw_readl(GPGDAT) & (1 << 5);
  printk(PREFIX "Debounce TIMEOUT gpg5=%d\n", gpg5);
  if (gpg5)
    reset_dac();    

  debouncing = 0;
}

/****************************************************************************
 * Handles an interrupt on GPG5
 * Parameters - Standard interrupt handler params. Not used.
 ****************************************************************************/
static void reciva_int_handler(int irq, void *dev, struct pt_regs *regs)
{
  int gpg5 = __raw_readl(GPGDAT) & (1 << 5);

  if (debouncing == 0)
  {
    printk(PREFIX "GPIOint GPG5=%d\n", gpg5);
    debouncing = 1;
    mod_timer(&timer, jiffies + DEBOUNCE_TIMEOUT);
  }
}

/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
reciva_init(void)
{
  printk(PREFIX "%s module: loaded\n", acModuleName);

  /* Register any GPIO pins we are using */
  rgpio_register("GPG5 (input)", acModuleName);

  /* Setup EINT13 - both edge triggered */
  rutl_regwrite((0xf << 20), (0xf << 20), EXTINT1); // Set as output

  /* GPG5 - EINT13 */
  rutl_regwrite((0 << 5), (1 << 5), GPGUP) ;   // Enable pullup
  rutl_regwrite((2 << 10), (3 << 10), GPGCON); // EINTn

  /* Request interrupt */
  request_irq(IRQ_EINT13, reciva_int_handler, 0, "EINT13", (void *)13);

  /* Initialise debounce timer */
  init_timer(&timer);
  timer.function = debounce_timeout_handler;

  return 0;
}

/****************************************************************************
 * Module exit
 ****************************************************************************/
static void __exit 
reciva_exit(void)
{
  printk(PREFIX "%s module: unloaded\n", acModuleName);

  disable_irq (IRQ_EINT13);
  free_irq(IRQ_EINT13, (void *)13);

  del_timer(&timer);
}


module_init(reciva_init);
module_exit(reciva_exit);

MODULE_AUTHOR("John Stirling <js@reciva.com>");
MODULE_DESCRIPTION("Reciva Mosa MS6335 Monitor");
MODULE_LICENSE("GPL");



