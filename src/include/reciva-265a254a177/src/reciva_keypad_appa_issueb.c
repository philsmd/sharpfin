/*
 * linux/reciva/reciva_keypad_appa_issueb.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2004 Reciva Ltd. All Rights Reserved
 * 
 * Keyboard driver for Reciva AppA IssueB Application boards
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
static int appa_decode_key_press(int row, int column);
static int *appa_keys_present(void);
static int *appa_alt_key_functions(void);


   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

/* Converts a columnn and row into an ascii key press value */
static const int aacKeyDecode[RKD_NUM_ROWS][RKD_NUM_COLS] =
{
  {RKD_POWER,   RKD_POWER,    RKD_POWER,    RKD_UNUSED},
  {RKD_REPLY,   RKD_PRESET_4, RKD_PRESET_1, RKD_UNUSED},
  {RKD_SELECT,  RKD_PRESET_5, RKD_PRESET_2, RKD_UNUSED},
  {RKD_BACK,    RKD_PRESET_6, RKD_PRESET_3, RKD_UNUSED}
};

/* A list of keys that are present (used for Manufacture Test Mode) */
static int aiKeysPresent[RKD_MAX_KEYS] =
{
  RKD_PRESET_1,
  RKD_PRESET_2,
  RKD_PRESET_3,
  RKD_PRESET_4,
  RKD_PRESET_5,
  RKD_PRESET_6,
  RKD_POWER,
  RKD_SELECT,
  RKD_BACK,
  RKD_REPLY,
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
  RKD_SKIP_PREVIOUS, /* PRESET1 */
  RKD_PLAY_PAUSE,    /* PRESET2 */
  RKD_SKIP_NEXT,     /* PRESET3 */
  RKD_PLAYBACK_MODE, /* PRESET4 */
  RKD_STOP,          /* PRESET5 */
  RKD_BROWSE_QUEUE,  /* PRESET6 */
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

static char acModuleName[] = "Reciva AppA IssueB Keypad Driver";

static const reciva_keypad_driver_t driver =
{
  name:              acModuleName,
  decode_key_press:  appa_decode_key_press,
  keys_present:      appa_keys_present,
  alt_key_functions: appa_alt_key_functions,
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
static int appa_decode_key_press(int row, int column)
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
static int *appa_keys_present()
{
  return aiKeysPresent;
}  

/****************************************************************************
 * Return pointer to array containing a list of alternate key functions
 ****************************************************************************/
static int *appa_alt_key_functions(void)
{
  return aiAlternateKeyFunctions;
}


module_init(rkd_init);
module_exit(rkd_exit);

MODULE_AUTHOR("John Stirling <js@reciva.com>");
MODULE_DESCRIPTION("Reciva AppA IssueB Keypad Driver");
MODULE_LICENSE("GPL");


