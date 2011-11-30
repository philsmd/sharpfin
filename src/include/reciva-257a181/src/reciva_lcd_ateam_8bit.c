/*
 * linux/reciva/reciva_lcd_ateam_8bit.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2004 Reciva Ltd. All Rights Reserved
 *
 * Non generic ATeam LCD Driver. 8 Bit Interface.
 *
 * Copyright (c) 2004 Reciva Ltd
 * Version 1.0 2005-02-08  John Stirling <js@reciva.com>
 *
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

#include "reciva_lcd_ateam.h"

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
#define GPIO_REG(x) *((volatile unsigned int *)(S3C2410_VA_GPIO + (x)))

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

/* Set this if we are using GPC for E1, E2 */
#define USE_GPC


   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

static char acModuleName[] = "Reciva LCD ATeam 8 Bit";


   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/


/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init
reciva_la8_init(void)
{
  printk("RLA8:%s module: loaded\n", acModuleName);
  return 0;
}

/****************************************************************************
 * Module tidy up
 ****************************************************************************/
static void __exit
reciva_la8_exit(void)
{
  printk("RLA8:%s module: unloaded\n", acModuleName);
}


/****************************************************************************
 * Set up the mode. 8 bit in this case.
 ****************************************************************************/
void reciva_lcd_init_mode(void)
{
  reciva_lcd_cycle (0, 0x3c, 39);
}

/****************************************************************************
 * Sends one 4 bit instruction to the LCD module.
 * iA0 - level of A0 signal
 * iData - data to be written - bits 3:0
 * iDelay - delay in microseconds at end of cycle
 ****************************************************************************/
void reciva_lcd_cycle (int iA0, int iData, int iDelay)
{
  unsigned int temp = GPCDAT;

  /* GPC8 - GPC15             : LCD module data D0 - D7
   * GPB2 (and GPC0, GPA12)    : (LDN0) LCD module E1
   * GPB3 (and GPC1, GPA13)    : (LDN1) LCD module E2  not used
   * GPC2                     : (LDN2) LCD module A0
   * GPC3                     : (LDN3)
   * GPB1 (and GPC4)          : (LDN4) LCD brightness
   * GPC5                     : (LDN5) LCD power */

  /* Set up A0 (GPC2) */
  if (iA0)
    temp |= (1 << 2);   /* A0 = '1' */
  else
    temp &= ~(1 << 2);  /* A0 = '0' */
  GPCDAT = temp;

  udelay (20);

  /* Set E high (GPC0 or GPC1) */
#ifdef USE_GPC
  temp |= (1 << 0);  /* E1 = '1' */
  GPCDAT = temp;
#else
  GPADAT |= (1 << 12);  // e1='1'
#endif

  /* Set up data */
  temp &= ~(0x0000ff00);
  temp |= (iData  << 8);
  GPCDAT = temp;

  udelay (20);

  /* Set E low */
#ifdef USE_GPC
  temp &= ~(1 << 0);  /* E1 = '1' */
  GPCDAT = temp;
#else
  GPADAT &= ~(1 << 12);  // e1='0'
#endif

  udelay (20);
  udelay (iDelay);
}




   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/

EXPORT_SYMBOL(reciva_lcd_init_mode);
EXPORT_SYMBOL(reciva_lcd_cycle);

module_init(reciva_la8_init);
module_exit(reciva_la8_exit);

MODULE_AUTHOR("John Stirling <js@reciva.com>");
MODULE_DESCRIPTION("Reciva LCD ATeam 8 Bit");
MODULE_LICENSE("GPL");


