/*
 * Slave Mode Driver I2C
 *
 * Copyright (c) 2006 Reciva Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 * Description:
 * XXX
 *
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

#include "reciva_slave_mode_driver_i2c.h"
#include "reciva_slave_mode_controller.h"
#include "reciva_util.h"


   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

static int __init reciva_module_init(void);
static void __exit reciva_module_exit(void);

/* Required driver functions */
static void reset_module(void);
static const char *get_name(void);
static void write_data(const char *data, int length);
static smc_driver_id_t get_driver_id(void);


   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

#define PREFIX "RSMDI2C:"

   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

static char acModuleName[] = "Reciva Slave Mode Driver I2C";
static smc_driver_id_t driver_id = SMC_DRIVER_I2C;

/* Note that any elements not explicitly initialised here are guaranteed to 
 * be zero initialised */
static const reciva_smc_driver_t driver = 
{
  smc_get_name:             get_name,
  smc_reset:                reset_module,
  smc_write:                write_data,
  smc_get_driver_id:        get_driver_id,
  smc_get_tx_buffer_empty:  NULL,

};



   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/


/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
reciva_module_init(void)
{
  printk(PREFIX "%s module: loaded\n", acModuleName);

  /* Register with the main Slave Mode Controller module */
  int err = reciva_smc_register_driver(&driver);
  if (err < 0)
  {
    printk (PREFIX "failure loading module (err=%d)\n", err);
    reciva_module_exit();
  }

  return err;
}

/****************************************************************************
 * Module tidy up
 ****************************************************************************/
static void __exit 
reciva_module_exit(void)
{
  printk(PREFIX "%s module: unloaded\n", acModuleName);
}

/****************************************************************************
 * Reset the module
 ****************************************************************************/
static void reset_module(void)
{
  printk(PREFIX "reset\n");
}

/****************************************************************************
 * Get module name
 ****************************************************************************/
static const char *get_name(void)
{
  return acModuleName;
}

/****************************************************************************
 * Data from generic slave mode controller arrives here
 ****************************************************************************/
static void write_data(const char *data, int length)
{
  printk(PREFIX "write_data\n");
}

/****************************************************************************
 * Return the driver ID
 ****************************************************************************/
static smc_driver_id_t get_driver_id(void)
{
  return driver_id;
}


module_init(reciva_module_init);
module_exit(reciva_module_exit);

MODULE_LICENSE("GPL");


