/*
 * linux/reciva/reciva_gpio.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2005 Reciva Ltd. All Rights Reserved
 * 
 * GPIO related stuff
 *
 * Version 1.0 2005-04-14  John Stirling <js@reciva.com>
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

#include "reciva_gpio.h"


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

static char acModuleName[] = "Reciva GPIO";

   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/


/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
rgpio_init(void)
{
  printk("RGPIO:%s module: loaded\n", acModuleName);
  return 0;
}

/****************************************************************************
 * Module tidy up
 ****************************************************************************/
static void __exit 
rgpio_exit(void)
{
  printk("RGPIO:%s module: unloaded\n", acModuleName);
}

/****************************************************************************
 * This should be called by any reciva module that claims a gpio line.
 * Just to help keep track of who owns which GPIO lines
 * bits_to_set - bit mask defining which bits should be set
 * bits_to_clear - bit mask defining which bits should be cleared
 * address - register address
 ****************************************************************************/
void rgpio_register(const char *gpio_lines_claimed, 
                    const char *owner)
{
  printk("RGPIO:REGISTER %s\n", acModuleName);
  printk("RGPIO:  GPIO Lines: %s\n", gpio_lines_claimed);
  printk("RGPIO:  Claimed by module: %s\n", owner);
}  



   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/

EXPORT_SYMBOL(rgpio_register);

module_init(rgpio_init);
module_exit(rgpio_exit);

MODULE_AUTHOR("John Stirling <js@reciva.com>");
MODULE_DESCRIPTION("Reciva GPIO");
MODULE_LICENSE("GPL");


