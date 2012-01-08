/*
 * linux/reciva/reciva_backlight_potas.c
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
 * Backlight control for Potas application boards
 * GPB0
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

static char acModuleName[] = "Reciva Backlight Potas";

static int backlight_control_disabled = 0;


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

  if (backlight_control_disabled)
    return 0;

  /* Menu/Volume LEDs (GPB0). */
  rutl_regwrite((0 << 0), (1 << 0), GPBDAT);   // Clear
  rutl_regwrite((1 << 0),  (0 << 0), GPBUP);   // Disable pullup
  rutl_regwrite((2 << 0), (3 << 0), GPBCON); // Select functio TOUT0

  // set up PWM for backlight
  __raw_writel (32, S3C2410_TCNTB(0));
  tmp = __raw_readl (S3C2410_TCFG0);
  tmp &= ~0xff;
  tmp |= 125;		// timer 0/1 prescale value
  __raw_writel (tmp, S3C2410_TCFG0);
  tmp = __raw_readl (S3C2410_TCFG1);
  tmp &= ~(15 << 4);
  tmp |= (3 << 0);	// 1/16 mux for timer0
  __raw_writel (tmp, S3C2410_TCFG1);
  tmp = __raw_readl (S3C2410_TCON);
  tmp &= ~(15 << 8);
  tmp |= (S3C2410_TCON_T0RELOAD | S3C2410_TCON_T0MANUALUPD | S3C2410_TCON_T0START);
  __raw_writel (tmp, S3C2410_TCON);
  tmp &= ~(S3C2410_TCON_T0MANUALUPD);
  __raw_writel (tmp, S3C2410_TCON);

  /* Turn backlight on */
  reciva_bl_set_backlight(BL_DEFAULT_MAX_BACKLIGHT);

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
  if (backlight_control_disabled)
    return;

  if (level < 0 )
    level = 0;
  else if (level > BL_DEFAULT_MAX_BACKLIGHT)
    level = BL_DEFAULT_MAX_BACKLIGHT;

  // NB. Following code solves same issue as different code in
  // reciva_backlight_appb.c. The two modules could probably be merged in
  // future, in which case, only one solution is required.
  if (level == 0)
  {
    // If PWM h/w is left running, small pulses continue to be generated
    // which can leave the backlight gently glowing, which can upset users.
    // To save the users from getting upset, we need to:
    // Disable PWM, switch the pin to normal output, and set it low
    rutl_regwrite( 0, S3C2410_TCON_T0START, (int)S3C2410_TCON );
    rutl_regwrite((1 << 0), (3 << 0), GPBCON);
    rutl_regwrite((0 << 0), (1 << 0), GPBDAT);
  }
  else
  {
    // Users also get upset if their backlight doesn't come back up,
    // so we have to reverse the above procedure (except clearing the pin)
    rutl_regwrite((2 << 0), (3 << 0), GPBCON);
    rutl_regwrite( S3C2410_TCON_T0START, 0, (int)S3C2410_TCON );
    
    __raw_writel (level, S3C2410_TCMPB(0));
  }
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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
  module_param(backlight_control_disabled, int, S_IRUGO);
#else
  MODULE_PARM(backlight_control_disabled, "i");
#endif

