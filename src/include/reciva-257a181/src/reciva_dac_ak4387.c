/*
 * linux/reciva/reciva_dac_ak4387.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2007 Reciva Ltd. All Rights Reserved
 * 
 * Simplistic initialisation of the AK4387 DAC
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/hardware.h>
#include <asm/arch/regs-gpio.h>

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

static char acModuleName[] = "Reciva DAC AK4387";

   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/

#define AK4387_SPI  S3C2410_GPGDAT
#define AK4387_CSN  (1 << 3)
#define AK4387_CCLK (1 << 5)
#define AK4387_CDTI (1 << 7)

#define AK4387_RSTN_PORT S3C2410_GPBDAT
#define AK4387_RSTN (1 << 7)

/****************************************************************************
 * Module initialisation
 *
 * 1) Ensure reset properly by cycling RSTN
 * 2) Write the audio format to register 00
 *    (all the other register default's should be OK)
 * 
 ****************************************************************************/
static int __init 
ak4387_init(void)
{
  printk("%s module: loaded\n", acModuleName);

  /* Set up the gpio pins */

  /* Set up data */
  rutl_regwrite(AK4387_CSN | AK4387_CCLK | AK4387_CDTI, 0, AK4387_SPI);
  rutl_regwrite(0, AK4387_RSTN, S3C2410_GPBDAT);
  
  /* Disable pullups */
  rutl_regwrite(AK4387_CSN | AK4387_CCLK | AK4387_CDTI, 0, S3C2410_GPGUP);
  rutl_regwrite(AK4387_RSTN, 0, S3C2410_GPBUP);

  /* Set as ouput */
  rutl_regwrite( (1 << 6) | (1 << 10) | (1 << 14),
                 (3 << 6) | (3 << 10) | (3 << 14),
                 S3C2410_GPGCON);
  rutl_regwrite( (1 << 14), (3 << 14), S3C2410_GPBCON);

  // Wait for reset to complete (>150ns), then bring power on
  udelay(1);
  rutl_regwrite(AK4387_RSTN, 0, S3C2410_GPBDAT);
  udelay(1);
  
  // Select chip, prepare clock
  rutl_regwrite(0, AK4387_CSN | AK4387_CCLK, AK4387_SPI);
  udelay(1);

  // Register 00: MCLK frequency "Manual Setting Mode"
  //              Audio data Mode 0 - 16-bit LSB justified
  unsigned int uiData = 0x6003;
  int i;
  for (i=0; i<16; i++)
  {
    // Set-up bit
    rutl_regwrite((uiData & 0x8000) ? AK4387_CDTI : 0, AK4387_CDTI, AK4387_SPI);
    udelay(1);

    // Clock data
    rutl_regwrite(AK4387_CCLK, 0, AK4387_SPI);
    udelay(1);
    rutl_regwrite(0, AK4387_CCLK, AK4387_SPI);  // clocked in on rising edge
    udelay(1);
    
    uiData <<= 1;
  }
  
  // Unselect chip
  rutl_regwrite(AK4387_CSN, 0, AK4387_SPI);

  return 0;
}

/****************************************************************************
 * Module tidy up
 ****************************************************************************/
static void __exit 
ak4387_exit(void)
{
  // Power down chip
  rutl_regwrite(0, AK4387_RSTN, S3C2410_GPBDAT);
  printk("%s module: unloaded\n", acModuleName);
}


   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/

module_init(ak4387_init);
module_exit(ak4387_exit);

MODULE_AUTHOR("John Stirling <js@reciva.com>");
MODULE_DESCRIPTION("Reciva DAC AK4387 driver");
MODULE_LICENSE("GPL");


