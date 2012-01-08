/*
 * linux/reciva/reciva_keypad_config983.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2006 Reciva Ltd. All Rights Reserved
 * 
 * Keyboard driver for Reciva config983
 *
 * Version 1.0 2005-04-28  John Stirling <js@reciva.com>
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
static int rkd_decode_key_press(int row, int column);
static int *rkd_keys_present(void);
static int *rkd_alt_key_functions(void);


   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

/* Converts a columnn and row into an ascii key press value */
static const int aacKeyDecode[RKD_NUM_ROWS][RKD_NUM_COLS] =
{
  /* Column 0     Column 1       Column 2        Column 3 */
  {RKD_UNUSED,  RKD_UNUSED,    RKD_UNUSED,     RKD_UNUSED}, // Row 0
  {RKD_UNUSED,  RKD_UNUSED,    RKD_UNUSED,     RKD_UNUSED}, // Row 1
  {RKD_UNUSED,  RKD_UNUSED,    RKD_UNUSED,     RKD_UNUSED}, // Row 2
  {RKD_UNUSED,  RKD_UNUSED,    RKD_SELECT,     RKD_UNUSED}  // Row 3
};

/* A list of keys that are present (used for Manufacture Test Mode) */
static int aiKeysPresent[RKD_MAX_KEYS] =
{
  RKD_SWITCH_MODE_TO_IRADIO,
  RKD_SWITCH_MODE_TO_MEDIA_PLAYER,
  RKD_SWITCH_MODE_TO_LINE_IN,
  RKD_POWER,
  RKD_ALARM,
  RKD_VOL_UP,
  RKD_VOL_DOWN,
  RKD_PRESET_1,
  RKD_PRESET_2,
  RKD_PRESET_3,
  RKD_PRESET_4,
  RKD_PRESET_5,
  RKD_PRESET_6,
  RKD_RC_UP,
  RKD_RC_DOWN,
  RKD_SELECT,
  RKD_REPLY,
  RKD_BROWSE_QUEUE,
  RKD_BACK,
  RKD_SKIP_PREVIOUS,
  RKD_PLAY_PAUSE, 
  RKD_SKIP_NEXT,
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

static char acModuleName[] = "Reciva Config983 Keypad Driver";

static const reciva_keypad_driver_t driver =
{
  name:              acModuleName,
  decode_key_press:  rkd_decode_key_press,
  keys_present:      rkd_keys_present,
  alt_key_functions: rkd_alt_key_functions,
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
static int rkd_decode_key_press(int row, int column)
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
static int *rkd_keys_present()
{
  return aiKeysPresent;
}

/****************************************************************************
 * Return pointer to array containing a list of alternate key functions
 ****************************************************************************/
static int *rkd_alt_key_functions(void)
{
  return aiAlternateKeyFunctions;
}


   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/

module_init(rkd_init);
module_exit(rkd_exit);

MODULE_AUTHOR("John Stirling <js@reciva.com>");
MODULE_DESCRIPTION("Reciva Config983 Keypad Driver");
MODULE_LICENSE("GPL");


