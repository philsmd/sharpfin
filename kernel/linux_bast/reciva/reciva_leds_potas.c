/*
 * linux/reciva/reciva_leds_potas.c
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
 * LED control
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

#include "reciva_leds.h"
#include "reciva_util.h"
#include "reciva_gpio.h"


/* This module takes a parameter to define the pins used
 * 0 = GPH7 (Volume LED), GPB7 (Menu LED) 
 * 1 = GPH7 (Volume LED), GPC4 (Menu LED)
 * 2 = GPH7 (Volume/Menu LED)
 * 3 = No LEDs */
static int led_config = 0;
MODULE_PARM(led_config, "i");


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
#define GPIO_REG(x) (unsigned int)((volatile unsigned int *)(S3C2410_VA_GPIO + (x)))

/* GPIO port A */
#define GPACON GPIO_REG(0x00)
#define GPADAT GPIO_REG(0x04)
#define GPAUP GPIO_REG(0x08)
/* GPIO port B */
#define GPBCON GPIO_REG(0x10)
#define GPBDAT GPIO_REG(0x14)
#define GPBUP GPIO_REG(0x18)
/* GPIO port C */
#define GPCCON GPIO_REG(0x20)
#define GPCDAT GPIO_REG(0x24)
#define GPCUP GPIO_REG(0x28)
/* GPIO port G */
#define GPGCON GPIO_REG(0x60)
#define GPGDAT GPIO_REG(0x64)
#define GPGUP GPIO_REG(0x68)
/* GPIO port H */
#define GPHCON GPIO_REG(0x70)
#define GPHDAT GPIO_REG(0x74)
#define GPHUP GPIO_REG(0x78)


   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

static char acModuleName[] = "Reciva LEDs Potas";


   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/


/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
reciva_led_init(void)
{
  printk("RLED:%s module: loaded led_config=%d\n", acModuleName, led_config);

  /* Tell GPIO module which GPIO pins we are using */

  /* Set up Menu LED */
  switch (led_config)
  {
    case 0: /* GPH7 (Volume) GPB7 (Menu) */
    default:
      /* Menu LED (GPB7). Active high. */
      rgpio_register("GPB7 (Menu LED)", acModuleName);
      rutl_regwrite((0 << 7), (1 << 7), GPBDAT);   // Clear
      rutl_regwrite((1 << 7),  (0 << 7), GPBUP);   // Disable pullup
      rutl_regwrite((1 << 14), (3 << 14), GPBCON); // Set as ouput
      break;
    case 1: /* GPH7 (Volume) GPC4 (Menu) */
      /* Menu LED (GPC4). Active high. */
      rgpio_register("GPC4 (Menu LED)", acModuleName);
      rutl_regwrite((0 << 4), (1 << 4), GPCDAT);   // Clear
      rutl_regwrite((1 << 4),  (0 << 4), GPCUP);   // Disable pullup
      rutl_regwrite((1 << 8), (3 << 8), GPCCON);   // Set as ouput
      break;
    case 2: /* GPH7 only */
      break;
    case 3: /* No LEDs */
      break;
  }

  /* Set up Volume LED */
  switch (led_config)
  {
    case 0: /* GPH7 (Volume) GPB7 (Menu) */
    case 1: /* GPH7 (Volume) GPC4 (Menu) */
    case 2: /* GPH7 only */
    default:
      /* Volume LED (GPH7). Active high. */
      rgpio_register("GPH7 (Volume LED)", acModuleName);
      rutl_regwrite((0 << 7), (1 << 7), GPHDAT);   // Clear
      rutl_regwrite((1 << 7),  (0 << 7), GPHUP);   // Disable pullup
      rutl_regwrite((1 << 14), (3 << 14), GPHCON); // Set as ouput
      break;
    case 3: /* No LEDs */
      break;
  }

  /* Turn them all on */
  reciva_led_set(RLED_VOLUME | RLED_MENU);

  return 0;
}

/****************************************************************************
 * Module tidy up
 ****************************************************************************/
static void __exit 
reciva_led_exit(void)
{
  /* Turn them all off */
  reciva_led_set(RLED_NONE);
  printk("RLED:%s module: unloaded\n", acModuleName);
}

/****************************************************************************
 * LED control
 ****************************************************************************/
void reciva_led_set(int bitmask)
{
  /* Set Volume LED */
  switch (led_config)
  {
    case 0: /* GPH7 (Volume) GPB7 (Menu) */
    case 1: /* GPH7 (Volume) GPC4 (Menu) */
    default:
      if (bitmask & RLED_VOLUME)
        rutl_regwrite((1 << 7), (0 << 7), GPHDAT); // Set
      else
        rutl_regwrite((0 << 7), (1 << 7), GPHDAT); // Clear
      break;
    case 2: /* GPH7 only */
      if (bitmask & RLED_MENU)
        rutl_regwrite((1 << 7), (0 << 7), GPHDAT); // Set
      else
        rutl_regwrite((0 << 7), (1 << 7), GPHDAT); // Clear
      break;
    case 3: /* No LEDs */
      break;
  }

  /* Set Menu LED */
  switch (led_config)
  {
    case 0:
    default:
      /* GPH7 (Volume), GPB7 (Menu) */

      /* Menu LED (GPB7) */
      if (bitmask & RLED_MENU)
        rutl_regwrite((1 << 7), (0 << 7), GPBDAT); // Set
      else
        rutl_regwrite((0 << 7), (1 << 7), GPBDAT); // Clear

      break;

    case 1:
      /* GPH7 (Volume), GPC4 (Menu) */
      if (bitmask & RLED_MENU)
        rutl_regwrite((1 << 4), (0 << 4), GPCDAT); // Set
      else
        rutl_regwrite((0 << 4), (1 << 4), GPCDAT); // Clear

      break;

    case 2: /* GPH7 only */
    case 3: /* No LEDs */
      break;
  }
}  

/****************************************************************************
 * Returns bitmask indicating which LEDs are supported
 ****************************************************************************/
int reciva_get_leds_supported(void)
{
  int bitmask = RLED_VOLUME | RLED_MENU;

  switch (led_config)
  {
    case 0:
    case 1:
    case 3:
    default:
      break;
    case 2:
      bitmask = RLED_MENU;
      break;
  }

  return bitmask;
}


   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/

EXPORT_SYMBOL(reciva_led_set);
EXPORT_SYMBOL(reciva_get_leds_supported);

module_init(reciva_led_init);
module_exit(reciva_led_exit);

MODULE_LICENSE("GPL");


