/*
 * linux/reciva/reciva_backlight_appb.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2005 Reciva Ltd. All Rights Reserved
 * 
 * Version 1.0 2005-06-16  John Stirling <js@reciva.com>
 *
 * Description :
 * Backlight control for App B application boards
 * J3-12 (GPB1)
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/version.h>

#include <asm/delay.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,23)
  #include <asm/plat-s3c/regs-timer.h>
# else
  #include <asm/arch/regs-timer.h>
# endif
#else
  #include <asm/arch-s3c2410/S3C2410-timer.h>
#endif

#include "reciva_backlight.h"
#include "reciva_util.h"


   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

/* GPIO registers */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
  #define GPIO_REG(x) (unsigned int)((volatile unsigned int *)(S3C24XX_VA_GPIO + (x)))
#else
  #define GPIO_REG(x) (unsigned int)((volatile unsigned int *)(S3C2410_VA_GPIO + (x)))
#endif

/* GPIO port B */
#define GPBCON GPIO_REG(0x10)
#define GPBDAT GPIO_REG(0x14)
#define GPBUP GPIO_REG(0x18)
/* GPIO port C */
#define GPCCON GPIO_REG(0x20)
#define GPCDAT GPIO_REG(0x24)
#define GPCUP GPIO_REG(0x28)


   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

static char acModuleName[] = "Reciva Backlight AppB";


   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/

/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
reciva_backlight_init(void)
{
  printk("RBL:%s module: loaded\n", acModuleName);
  unsigned long tmp;

  /* LDN4 (LCDBRIGHT) - connected to F1 (GPB1) and K2 (GPC4) using F1 */
  rutl_regwrite((2 << 2), (3 << 2), GPBCON); // Select function TOUT1
  rutl_regwrite((0 << 1), (1 << 1), GPBDAT); // Clear
  rutl_regwrite((1 << 1), (0 << 1), GPBUP);  // Disable pullup

  rutl_regwrite((0 << 4), (3 << 8), GPCCON); // Set as input
  rutl_regwrite((0 << 4), (1 << 4), GPCDAT); // Clear
  rutl_regwrite((1 << 4), (1 << 4), GPCUP);  // Disable pullup

  // set up PWM for backlight
  __raw_writel (32, S3C2410_TCNTB(1));
  tmp = __raw_readl (S3C2410_TCFG0);
  tmp &= ~0xff;
  tmp |= 125;		// timer 0/1 prescale value
  __raw_writel (tmp, S3C2410_TCFG0);
  tmp = __raw_readl (S3C2410_TCFG1);
  tmp &= ~(15 << 4);
  tmp |= (3 << 4);	// 1/16 mux for timer1
  __raw_writel (tmp, S3C2410_TCFG1);
  tmp = __raw_readl (S3C2410_TCON);
  tmp &= ~(15 << 8);
  tmp |= (S3C2410_TCON_T1RELOAD | S3C2410_TCON_T1MANUALUPD | S3C2410_TCON_T1START);
  __raw_writel (tmp, S3C2410_TCON);
  tmp &= ~(S3C2410_TCON_T1MANUALUPD);
  __raw_writel (tmp, S3C2410_TCON);

  /* Turn backlight on */
  reciva_bl_set_backlight(31);

  return 0;
}

/****************************************************************************
 * Module tidy up
 ****************************************************************************/
static void __exit 
reciva_backlight_exit(void)
{
  reciva_bl_set_backlight(5);
  printk("RBL:%s module: unloaded\n", acModuleName);
}


/****************************************************************************
 * Backlight control
 ****************************************************************************/
void reciva_bl_set_backlight(int level)
{
  if (level < 0 )
    level = 0;
  else if (level > BL_DEFAULT_MAX_BACKLIGHT)
    level = BL_DEFAULT_MAX_BACKLIGHT;

  /* NB. This solves the same issue as different code in
   * reciva_backlight_potas.c. The two modules could probably be merged in
   * future, in which case, only one solution is required. */
  /* Still get pulses if we set it to zero
   * Setting to 32 removes the pulses
   * XXX ideally need to work out why, but don't have time now */
  if (level == 0)
    level = 32;

  __raw_writel (level, S3C2410_TCMPB(1));
}  


/****************************************************************************
 * Get the backlight control range
 ****************************************************************************/
int reciva_bl_get_max_backlight(void)
{
  return BL_DEFAULT_MAX_BACKLIGHT;
}


   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/

EXPORT_SYMBOL(reciva_bl_set_backlight);
EXPORT_SYMBOL(reciva_bl_get_max_backlight);

module_init(reciva_backlight_init);
module_exit(reciva_backlight_exit);

MODULE_LICENSE("GPL");
