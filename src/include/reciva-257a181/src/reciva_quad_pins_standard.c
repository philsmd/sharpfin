/*
 * linux/reciva/reciva_quad_pins_standard.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2004 Reciva Ltd. All Rights Reserved
 * 
 * Quad pin mapping.
 *
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

#include "reciva_quad_pins.h"

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

/* GPIO port G */
#define GPGDAT GPIO_REG(0x64)


   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

static char acModuleName[] = "Reciva Quad Pins Standard";


   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/


/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
reciva_qp_init(void)
{
  printk("RQP:%s module: loaded\n", acModuleName);
  return 0;
}

/****************************************************************************
 * Module tidy up
 ****************************************************************************/
static void __exit 
reciva_qp_exit(void)
{
  printk("RQP:%s module: unloaded\n", acModuleName);
}


/****************************************************************************
 * Returns status of quad pins
 ****************************************************************************/
int reciva_qp_pins(void)
{
  int iPins = (GPGDAT >> 14) & 0x03;
  return iPins;
}  



   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/

EXPORT_SYMBOL(reciva_qp_pins);

module_init(reciva_qp_init);
module_exit(reciva_qp_exit);

MODULE_AUTHOR("John Stirling <js@reciva.com>");
MODULE_DESCRIPTION("Reciva Quad Pins Standard");
MODULE_LICENSE("GPL");


