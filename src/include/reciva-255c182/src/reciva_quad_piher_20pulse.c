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
static void do_quad(void);


   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

/* GPIO registers */
#define GPIO_REG(x) (unsigned int)((volatile unsigned int *)(S3C2410_VA_GPIO + (x)))

/* GPIO port G */
#define GPGCON GPIO_REG(0x60)
#define GPGDAT GPIO_REG(0x64)
#define GPGUP GPIO_REG(0x68)

/* External interrupt control */
#define EXTINT2 GPIO_REG(0x90)

/* External interrupt filter */
#define EINTFLT3 GPIO_REG(0xa0)

/* Interrupt mask */
#define EINTMASK GPIO_REG(0xa4)

/* Interrupt pending */
#define EINTPEND GPIO_REG(0xa8)


/* The status of the 2 SHAFT GPIO pins in a more convenient form */
#define SHAFT_PINS reciva_qp_pins()

   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

/* Communication with the application */
static struct input_dev *input_dev;

/* Stores the most recently read status of the GPIO pins
 * bit 0 : SHAFT 0
 * bit 1 : SHAFT 1 */
static int shaft_old; 


   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/

/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init
register_quad (void)
{
  printk("register_quad\n");

  /* Set up GPIO pins
   * SHAFT0 = GPG14 (pin function = EINT22)
   * SHAFT1 = GPG15 (pin function = EINT23) */
  rutl_regwrite(((2 << 30) | (2 << 28)),  // Set
                ((3 << 30) | (3 << 28)),  // Clear
		GPGCON);
  rutl_regwrite(((0 << 15) | (0 << 14)),  // Set
                ((1 << 15) | (1 << 14)),  // Clear 
		GPGUP);
  
  /* Enable interrupt on both pins (both edges)
   * Don't need to clear bits as we're setting all that we're interested in */
  rutl_regwrite(0xff000000,  // Set
                0,           // Clear 
		EXTINT2);

  /* Set the length of filter for external interrupt
   * Filter clock = PCLK
   * Filter width = 0x7f (max) */
  rutl_regwrite(0x7f7f0000,  // Set
                0xffff0000,  // Clear 
		EINTFLT3);

  /* Set up the interrupts */
  request_irq(IRQ_EINT22, reciva_quad_int_handler, 0, "EINT22", (void *)22);
  request_irq(IRQ_EINT23, reciva_quad_int_handler, 0, "EINT23", (void *)23);

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

  free_irq(IRQ_EINT22, (void *)22);
  free_irq(IRQ_EINT23, (void *)23);

  input_unregister_device (input_dev);
  kfree (input_dev);
}

/****************************************************************************
 * Handles an interrupt on GPIO14/15
 * Parameters : Standard interrupt handler params. Not used.
 ****************************************************************************/
static void reciva_quad_int_handler(int irq, void *dev, struct pt_regs *regs)
{
  int shaft_new = SHAFT_PINS;

  /* Work out rotation direction and let application know about it */
  do_quad();
  
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
static void do_quad()
{
  int direction;
  int shaft_new = SHAFT_PINS;
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

    if (iCount > 3)
    {
      iCount = 0;
      switch (direction)
      {
        case 'c':
          /* Clockwise */
          input_report_rel (input_dev, REL_Y, +1);

          /* Tell keypad driver to cancel it's shift status */
          rkg_cancel_shift();
          break;
        case 'a':
          /* Anticlockwise */
          input_report_rel (input_dev, REL_Y, -1);

          /* Tell keypad driver to cancel it's shift status */
          rkg_cancel_shift();
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


      
