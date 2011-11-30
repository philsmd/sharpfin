/*
 * linux/reciva/reciva_quad_piher_20pulse.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2004 Reciva Ltd. All Rights Reserved
 * 
 * Version 1.0 2004-08-05  John Stirling <js@reciva.com>
 *
 */

/*********************************************************************
 *
 * Description :
 *
 * Rotary Encoder driver for Piher CI-11 (20 pulse version - 1 click
 * produces 4 transitions)
 *
 * Manufacturer: PIHER
 * Manufacturer Part Number:CI-11CO-V1Y22-HF4CF
 * Data sheet http://www.farnell.com/datasheets/44911.pdf
 *
 *********************************************************************/

   
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
#include <asm/arch-s3c2410/S3C2410-gpio.h>
#include <asm/io.h>

#include "reciva_quad_pins.h"
#include "reciva_keypad_generic.h"
#include "reciva_util.h"
#include "reciva_quad.h"

   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

static int __init register_quad (void);
static void __exit unregister_quad(void);
static void reciva_quad_int_handler(int irq, void *dev, struct pt_regs *regs);
static void do_quad(unsigned int old, unsigned int new);

   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

/* The status of the 2 SHAFT GPIO pins in a more convenient form */
#define SHAFT_PINS reciva_qp_pins_read()

   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

/* Communication with the application */
static struct input_dev *input_dev;

/* Stores the most recently read status of the GPIO pins
 * bit 0 : SHAFT 0
 * bit 1 : SHAFT 1 */
static unsigned int shaft_old; 

static int pulse_threshold = 4;		// set to 2 for 10-pulse version
static int pin_config;			// set to 1 to use GPG3/5 not GPG14/15
static int pins_reversed;		// set to 1 to invert the pins

static int pin0, pin1;
static int irq_pin0, irq_pin1;

   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/

static int 
reciva_qp_pins_read (void)
{
  unsigned long pin_stat = readl(S3C2410_GPGDAT);
  unsigned int iPins;
  
  iPins = (pin_stat & (1 << pin0)) ? (pins_reversed ? 2 : 1) : 0;
  iPins |= (pin_stat & (1 << pin1)) ? (pins_reversed ? 1 : 2) : 0;

  return iPins;
}  

/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init
register_quad (void)
{
  printk("register_quad\n");

  /* Set up GPIO pins */
  switch (pin_config)
    {
    case 0:
      /* SHAFT0 = GPG14 (pin function = EINT22)
       * SHAFT1 = GPG15 (pin function = EINT23) */
      pin0 = 14;
      pin1 = 15;
      irq_pin0 = IRQ_EINT22;
      irq_pin1 = IRQ_EINT23;
      break;
    case 1:  
      /* SHAFT0 = GPG3 (pin function = EINT11)
       * SHAFT1 = GPG5 (pin function = EINT13) */
      pin0 = 3;
      pin1 = 5;
      irq_pin0 = IRQ_EINT11;
      irq_pin1 = IRQ_EINT13;
      break;
    default:
      return -ENODEV;
    }
  rutl_regwrite((2 << (pin0 << 1)) | (2 << (pin1 << 1)),  // Set
		(3 << (pin0 << 1)) | (3 << (pin1 << 1)),  // Clear
		S3C2410_GPGCON);
  rutl_regwrite((0 << pin0) | (0 << pin1),  // Set
		(1 << pin0) | (1 << pin1),  // Clear 
		S3C2410_GPGUP);
  
  switch (pin_config)
    {
    case 0:
      /* Enable interrupt on both pins (both edges)
       * Don't need to clear bits as we're setting all that we're interested in */
      rutl_regwrite(0xff000000,  // Set
		    0,           // Clear 
		    S3C2410_EXTINT2);

      /* Set the length of filter for external interrupt
       * Filter clock = PCLK
       * Filter width = 0x7f (max) */
      rutl_regwrite(0x7f7f0000,  // Set
		    0xffff0000,  // Clear 
		    S3C2410_EINFLT3);
      break;
    case 1:
      /* Enable interrupt on both pins (both edges)
       * Don't need to clear bits as we're setting all that we're interested in */
      rutl_regwrite((7 << 20) | (7 << 12),  // Set
		    0,           // Clear 
		    S3C2410_EXTINT1);
      break;
    default:
      BUG ();
    }

  /* Set up the interrupts */
  request_irq(irq_pin0, reciva_quad_int_handler, 0, "shaft0", NULL);
  request_irq(irq_pin1, reciva_quad_int_handler, 0, "shaft1", NULL);

  /* Note initial state of shaft encoder gpio pins */
  shaft_old = SHAFT_PINS;

  /* Set up input system */
  input_dev = kmalloc (sizeof (*input_dev), GFP_KERNEL);
  memset (input_dev, 0, sizeof (*input_dev));
#ifdef KERNEL_26
  init_input_dev (input_dev);
#endif
  input_dev->evbit[0] = BIT(EV_REL);
  set_bit (REL_Y, input_dev->relbit);
  input_dev->name = "reciva_quad";
  input_register_device (input_dev);

  return 0;
}

/****************************************************************************
 * Module cleanup
 ****************************************************************************/
static void __exit
unregister_quad (void)
{
  printk("unregister_quad\n");

  free_irq(irq_pin0, NULL);
  free_irq(irq_pin1, NULL);

  input_unregister_device (input_dev);
  kfree (input_dev);
}

/****************************************************************************
 * Handles an interrupt on GPIO14/15
 * Parameters : Standard interrupt handler params. Not used.
 ****************************************************************************/
static void reciva_quad_int_handler(int irq, void *dev, struct pt_regs *regs)
{
  unsigned int shaft_new = SHAFT_PINS;

  /* Work out rotation direction and let application know about it */
  do_quad(shaft_old, shaft_new);
  
  /* Note old value for next time round */
  shaft_old = shaft_new;
}

/****************************************************************************
 * Works out the direction of movement of the rotary encoder and reports
 * this to application.
 *
 * Rotary Encoder Operation :
 *
 * A clockwise click produces a change in the rotary encoder
 * outputs from as shown below. 1 click will produce 4 transitions from
 * eg 00-10-11-01 or 11-01-00-10
 * 
 *                      ______
 * SHAFT1 _____________|      |___________
 *                         ______
 * SHAFT0 ________________|      |________
 *
 *
 *   An anticlockwise click produces a change in the rotary encoder
 *   outputs as shown below.
 *                         ______
 * SHAFT1 ________________|      |________
 *                      ______
 * SHAFT0 _____________|      |___________
 *
 ****************************************************************************/
static void do_quad(unsigned int shaft_old, unsigned int shaft_new)
{
  int direction;
  static int direction_old = 'x';
  static int iCount = 0;
  /* Use this to determine the direction of rotation */
  static int lookup[4][4] =
  {
    {'x', 'a', 'c', 'x'},
    {'c', 'x', 'x', 'a'},
    {'a', 'x', 'x', 'c'},
    {'x', 'c', 'a', 'x'}
  };
    
  /* Work out direction of rotation */
  direction = lookup[shaft_old][shaft_new];

  /* Need to have 4 in a row to register as a rotation */
  if (direction_old == direction)
  {  
    iCount++;

    if (iCount >= pulse_threshold)
    {
      iCount = 0;
      switch (direction)
      {
        case 'c':
          /* Clockwise */
          input_report_rel (input_dev, REL_Y, +1);
          break;
        case 'a':
          /* Anticlockwise */
          input_report_rel (input_dev, REL_Y, -1);
          break;
        default:
          /* Unexpected transition - ignore */
          break;
      }  
    }
  }

  else
  {
    iCount = 1;
    direction_old = direction;
  }  
}  
 

module_init(register_quad);
module_exit(unregister_quad);

MODULE_AUTHOR("John Stirling <js@reciva.com>");
MODULE_DESCRIPTION("Reciva Quad");
MODULE_LICENSE("GPL");
MODULE_PARM(pulse_threshold, "i");
MODULE_PARM(pin_config, "i");
MODULE_PARM(pins_reversed, "i");
