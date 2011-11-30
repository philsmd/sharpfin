/*
 * Slave Mode Driver Serial
 *
 * Copyright (c) 2006 Reciva Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 * Description:
 * Slave mode controller driver - serial
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/kernel.h>

#include <asm/delay.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/mach-types.h>

#include <stdio.h>

#include "reciva_slave_mode_controller.h"
#include "reciva_uart.h"
#include "reciva_util.h"



   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

static int __init reciva_module_init(void);
static void __exit reciva_module_exit(void);

/* Required driver functions - SMC */
static void reset_module(void);
static const char *get_name(void);
static void write_data(const char *data, int length);
static smc_driver_id_t get_driver_id(void);
static int tx_buffer_empty(void);

/* Required driver functions - UART */
static void uart_rx_data(char *data);


   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

#define PREFIX "RSMDSER:"

   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

static char acModuleName[] = "Reciva Slave Mode Driver Serial";
static smc_driver_id_t driver_id = SMC_DRIVER_SERIAL;

/* Slave Mode Controller driver
 * Data from generic Slave Mode Controller arrives here */
static const reciva_smc_driver_t driver = 
{
  smc_get_name:             get_name,
  smc_reset:                reset_module,
  smc_write:                write_data,
  smc_get_driver_id:        get_driver_id,
  smc_get_tx_buffer_empty:  tx_buffer_empty,

};

/* UART rx driver
 * Rx data from the UART arrives here */
static const reciva_uart_rx_driver_t uart_rx_driver = 
{
  uart_rx:                  uart_rx_data,
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

  /* Register to receive UART rx data */
  err = reciva_uart_register_rx_driver(&uart_rx_driver);
  if (err < 0)
  {
    printk (PREFIX "failure - UART Rx (err=%d)\n", err);
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
 * data - binary data
 ****************************************************************************/
static void write_data(const char *data, int length)
{
  int byte_length = length;
  int ascii_length = (byte_length * 2) + 1; // Add space for '\r'
  char *ascii_buf = kmalloc(sizeof(char) * ascii_length, GFP_KERNEL);

  /* Convert to ascii encoded hex before sending it on to UART */
  int count = 0;
  while (byte_length > 0)
  {
    unsigned int msn = (data[count] & 0xf0) >> 4;
    unsigned int lsn = data[count] & 0x0f;
    char temp_buf[10];

    snprintf(temp_buf, sizeof(temp_buf)/sizeof(temp_buf[0]), "%1x", msn);
    ascii_buf[count*2 + 0] = temp_buf[0];

    snprintf(temp_buf, sizeof(temp_buf)/sizeof(temp_buf[0]), "%1x", lsn);
    ascii_buf[count*2 + 1] = temp_buf[0];

    count++;
    byte_length--;
  }
  ascii_buf[ascii_length-1] = '\r';

  if (ascii_length > 0)
    reciva_uart_write_internal(ascii_buf, ascii_length);

  kfree(ascii_buf);
}

/****************************************************************************
 * Return the driver ID
 ****************************************************************************/
static smc_driver_id_t get_driver_id(void)
{
  return driver_id;
}

/****************************************************************************
 * Indicates if the tx buffer is empty
 ****************************************************************************/
static int tx_buffer_empty(void)
{
  return reciva_uart_tx_buffer_empty();
}

/****************************************************************************
 * Data from UART arrives here
 * data : null terminated ascii encoded hex string
 ****************************************************************************/
static void uart_rx_data(char *data)
{
  int byte_length = 0;
  int ascii_length = strlen(data);
  
  /* ASCII encoded hex - must be a multiple of 2 */
  if ((strlen(data) > 0) && (strlen(data) % 2 == 0))
  {
    unsigned int msn;
    unsigned int lsn;
    char temp_buf[2];
    temp_buf[1] = 0;    

    while(ascii_length > 0)
    {
      temp_buf[0] = toupper(data[byte_length*2]);    
      msn = simple_strtol(temp_buf, NULL, 16);
  
      temp_buf[0] = toupper(data[byte_length*2 + 1]);    
      lsn = simple_strtol(temp_buf, NULL, 16);
  
      data[byte_length] = ((msn << 4) & 0xf0) | (lsn & 0x0f);
      byte_length++;
      ascii_length -= 2;
    }

    /* Send it on to SMC */
    reciva_smc_command(data, byte_length);
  }
}


module_init(reciva_module_init);
module_exit(reciva_module_exit);

MODULE_LICENSE("GPL");


