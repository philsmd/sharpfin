/*
 * linux/reciva/reciva_backlight_dummy.c
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
 * Dummy Backlight control
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

   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

static char acModuleName[] = "Reciva Backlight Dummy";


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
  return 0;
}

/****************************************************************************
 * Module tidy up
 ****************************************************************************/
static void __exit 
reciva_backlight_exit(void)
{
  printk("RBL:%s module: unloaded\n", acModuleName);
}


/****************************************************************************
 * Backlight control
 ****************************************************************************/
void reciva_bl_set_backlight(int level)
{
}  



   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/

EXPORT_SYMBOL(reciva_bl_set_backlight);

module_init(reciva_backlight_init);
module_exit(reciva_backlight_exit);

MODULE_LICENSE("GPL");


