/*
 * linux/reciva/reciva_keypad_config975.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2004 Reciva Ltd. All Rights Reserved
 * 
 * Keyboard driver for config975
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

#include <asm/delay.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/mach-types.h>

#include "reciva_keypad_driver.h"
#include "reciva_keypad_generic.h"

   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

/* Driver functions */
static int reciva_decode_key_press(int row, int column);
static int *reciva_keys_present(void);
static int *reciva_alt_key_functions(void);


   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

/* Converts a columnn and row into an ascii key press value */
static const int aacKeyDecode[RKD_NUM_ROWS][RKD_NUM_COLS] =
{
  /* Column 0             Column 1          Column 2      Column 3 */
  {RKD_BACK,              RKD_SELECT,       RKD_UNUSED,   RKD_UNUSED}, // Row 0
  {RKD_VOL_DOWN,          RKD_UNUSED,       RKD_VOL_UP,   RKD_UNUSED}, // Row 1
  {RKD_IR_FM_MODE_SWITCH, RKD_REPLY,        RKD_UNUSED,   RKD_UNUSED},// Row 2
  {RKD_UNUSED,            RKD_UNUSED,       RKD_UNUSED,   RKD_UNUSED}  // Row 3
};

/* A list of keys that are present (used for Manufacture Test Mode) */
static int aiKeysPresent[RKD_MAX_KEYS] =
{
  /* Don't need to include RKD_UP, RKD_DOWN as these are always
   * assumed to be present */
  RKD_IR_FM_MODE_SWITCH,
  RKD_REPLY,
  RKD_VOL_UP,
  RKD_BACK,
  RKD_VOL_DOWN,
  RKD_SELECT,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0
};

/* A list of alternate key functions */
static int aiAlternateKeyFunctions[RKD_MAX_KEYS] =
{
  RKD_UNUSED,
  RKD_UNUSED,
  RKD_UNUSED,
  RKD_UNUSED,
  RKD_UNUSED,
  RKD_UNUSED,
  RKD_UNUSED,
  RKD_UNUSED,
  RKD_UNUSED,
  RKD_UNUSED,
  RKD_UNUSED,
  RKD_UNUSED,
  RKD_UNUSED,
  RKD_UNUSED,
  RKD_UNUSED,
  RKD_UNUSED,
  RKD_UNUSED,
  RKD_UNUSED,
  RKD_UNUSED,
  RKD_UNUSED,
  RKD_UNUSED,
  RKD_UNUSED,
  RKD_UNUSED,
  RKD_UNUSED,
  RKD_UNUSED,
  RKD_UNUSED,
  RKD_UNUSED,
  RKD_UNUSED,
  RKD_UNUSED,
  RKD_UNUSED,
  RKD_UNUSED,
  RKD_UNUSED,
};

static char acModuleName[] = "Reciva config975 Keypad Driver";

static const reciva_keypad_driver_t driver =
{
  name:              acModuleName,
  decode_key_press:  reciva_decode_key_press,
  keys_present:      reciva_keys_present,
  alt_key_functions: reciva_alt_key_functions,
};


   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/


/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
rkd_init(void)
{
  printk("RKD:%s module: loaded\n", acModuleName);
  rkg_register(&driver); /* Register driver */
  return 0;
}

/****************************************************************************
 * Module tidy up
 ****************************************************************************/
static void __exit 
rkd_exit(void)
{
  printk("RKD:%s module: unloaded\n", acModuleName);
}


/****************************************************************************
 * Decode key press
 * row - keypad row
 * column - keypad column
 ****************************************************************************/
static int reciva_decode_key_press(int row, int column)
{
  if (row > RKD_NUM_ROWS)
    row = 0;
  if (column > RKD_NUM_COLS)
    column = 0;

  return aacKeyDecode[row][column];
}  

/****************************************************************************
 * Return pointer to array containing a list of keys that are supported by
 * this hardware
 ****************************************************************************/
static int *reciva_keys_present()
{
  return aiKeysPresent;
}

/****************************************************************************
 * Return pointer to array containing a list of alternate key functions
 ****************************************************************************/
static int *reciva_alt_key_functions(void)
{
  return aiAlternateKeyFunctions;
}


   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/

module_init(rkd_init);
module_exit(rkd_exit);

MODULE_AUTHOR("John Stirling <js@reciva.com>");
MODULE_DESCRIPTION("Reciva Keypad cf975");
MODULE_LICENSE("GPL");


