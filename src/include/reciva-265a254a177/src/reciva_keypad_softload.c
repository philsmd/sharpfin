/*
 * linux/reciva/reciva_keypad_softload.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2004, 2007 Reciva Ltd. All Rights Reserved
 * 
 * Soft keymap driver
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
#include <asm/uaccess.h>

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
static int reciva_ioctl_extension_hook(struct inode *inode, struct file *file, 
				       unsigned int cmd, unsigned long arg);

   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

/* Converts a columnn and row into an ascii key press value */
static int aacKeyDecode[RKD_NUM_ROWS][RKD_NUM_COLS];

static const char acModuleName[] = "Reciva Soft Mapped Keypad Driver";

static const reciva_keypad_driver_t driver =
{
  name:			acModuleName,
  decode_key_press:	reciva_decode_key_press,
  ioctl_extension_hook:	reciva_ioctl_extension_hook,
};

   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/

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
 * Load keymap data from user space
 ****************************************************************************/
static int do_load_keymap (unsigned long arg)
{
  struct reciva_keymap_load k;
  int x, y;

  if (copy_from_user (&k, (void *)arg, sizeof (k)))
    return -EFAULT;

  printk("RKD: load_keymap r=%d c=%d\n", k.nrows, k.ncols);

  if (k.nrows > RKD_NUM_ROWS || k.ncols > RKD_NUM_COLS)
    return -EINVAL;

  int *buf = kmalloc((k.nrows * k.ncols * sizeof (int)), GFP_KERNEL);
  if (!buf)
    return -ENOMEM;

  if (copy_from_user (buf, (void *)k.codes, (k.nrows * k.ncols * sizeof (int))))
    {
      kfree (buf);
      return -EFAULT;
    }

  for (x = 0; x < k.ncols; x++)
    for (y = 0; y < k.nrows; y++)
      aacKeyDecode[y][x] = buf[x + (y * k.ncols)];

  kfree (buf);  
  
  return 0;
}

/****************************************************************************
 * Handle local ioctls
 ****************************************************************************/
static int reciva_ioctl_extension_hook(struct inode *inode, struct file *file, 
				       unsigned int cmd, unsigned long arg)
{
  switch (cmd)
    {
    case IOC_KEY_LOAD_MAP:
      printk("RKD: IOC_KEY_LOAD_MAP\n");
      return do_load_keymap (arg);
    }

  return -ENODEV;
}

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

module_init(rkd_init);
module_exit(rkd_exit);

MODULE_AUTHOR("John Stirling <js@reciva.com>");
MODULE_DESCRIPTION("Reciva Keypad Softload");
MODULE_LICENSE("GPL");
