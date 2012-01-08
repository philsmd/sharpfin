/*
 * linux/reciva/reciva_quad_piher_20pulse.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2007 Reciva Ltd. All Rights Reserved
 * 
 */

/*********************************************************************
 *
 * Description :
 *
 * Rotary Encoder driver 
 * 2 encoders supported
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
#include <linux/version.h>
#include <linux/interrupt.h>

#include <asm/delay.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/arch/regs-gpio.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
# include <asm/arch/regs-irq.h>
#endif

#include <asm/io.h>

#include "reciva_util.h"

   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

/* Encoder */
typedef struct
{
  /* Indicates if this encoder is in use */
  int active;

  /* Communication with the application */
  struct input_dev *input_dev;

  /* Current and previous status of the GPIO pins
   * bit 0 : SHAFT 0
   * bit 1 : SHAFT 1 */
  unsigned int shaft_old; 
  unsigned int shaft_new; 

  /* Number of transitions per click */
  int pulse_threshold;  

  /* 1 = invert the pins */
  int pins_reversed;

  /* GPIO/interrupt details */
  unsigned long pin0, pin1;             // GPIO numbers
  unsigned long eint0, eint1;           // GPIO EINT function
  unsigned long irq_pin0, irq_pin1;     // IRQ numbers

  /* Direction of last detected click */
  int direction_old;

  /* Transitions in one direction */
  int transition_count;

  /* IRQ names */
  char *pin0_irq_name;
  char *pin1_irq_name;

  /* Device name */
  char *device_name;

  /* Event ID for reporting events to user spave (via input_report_rel) */
  int rel_event_id;

} encoder_t;


   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

static int __init register_quad (void);
static void __exit unregister_quad(void);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
static irqreturn_t reciva_quad_int_handler(int irq, void *dev);
#else
static void reciva_quad_int_handler(int irq, void *dev, struct pt_regs *regs);
#endif

static void do_quad(encoder_t *e);

   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

#define PFX "QUAD:"

   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

/* Encoders */
#define MAX_ENCODERS 2
static encoder_t encoders[MAX_ENCODERS];

/* SHAFT0/1 configuration */
static int pulse_threshold = 4;  // Number of transitions per click
static int pin_config;           // 0 - use GPG14/15
                                 // 1 - use GPG3/5
                                 // 2 - use GPG5/7
static int pins_reversed;        // set to 1 to invert the pins

/* SHAFT2/3 configuration */
static int pulse_threshold_23 = 4;  // Number of transitions per click
static int pin_config_23;           // 0 - none
                                    // 1 - SHAFT2=J2-11(GPG11) SHAFT3=J2-13(GPG13) XXX think this one doesn't work
                                    // 2 - SHAFT2=J2-1(GPG3) SHAFT3=J2-25(GPF4)
                                    // 3 - SHAFT2=J2-1(GPG3) SHAFT3=J2-3(GPG5/0)
static int pins_reversed_23;        // set to 1 to invert the pins


   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/

/****************************************************************************
 * Returns status of shaft pins in bits [0:1]
 ****************************************************************************/
static int 
reciva_qp_pins_read (encoder_t *e)
{
  // S3C2412/13 insists we change pin config to input before reading them
  s3c2410_gpio_cfgpin(e->pin0, S3C2410_GPIO_INPUT);
  s3c2410_gpio_cfgpin(e->pin1, S3C2410_GPIO_INPUT);
  unsigned long pin_statF = readl(S3C2410_GPFDAT);
  unsigned long pin_statG = readl(S3C2410_GPGDAT);
  s3c2410_gpio_cfgpin(e->pin0, e->eint0);
  s3c2410_gpio_cfgpin(e->pin1, e->eint1);

  int pin0 = (((e->pin0 < S3C2410_GPIO_BANKG) ? pin_statF : pin_statG) >> S3C2410_GPIO_OFFSET(e->pin0)) & 1;
  int pin1 = (((e->pin1 < S3C2410_GPIO_BANKG) ? pin_statF : pin_statG) >> S3C2410_GPIO_OFFSET(e->pin1)) & 1;
  int iPins = e->pins_reversed ? ((pin0 << 1) | pin1)
                               : ((pin1 << 1) | pin0);

  return iPins;
}

/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init
register_quad (void)
{
  printk(PFX "register_quad\n");
  printk(PFX "  pin_config=%d\n", pin_config);
  printk(PFX "  pin_config_23=%d\n", pin_config_23);
  printk(PFX "  [0].pulse_threshold=%d\n", pulse_threshold);
  printk(PFX "  [0].pins_reversed=%d\n", pins_reversed);
  printk(PFX "  [1].pulse_threshold=%d\n", pulse_threshold_23);
  printk(PFX "  [1].pins_reversed=%d\n", pins_reversed_23);

  memset(encoders, 0, sizeof(encoders));

  encoders[0].pin0_irq_name = "shaft0";
  encoders[0].pin1_irq_name = "shaft1";
  encoders[0].device_name = "reciva_quad";
  encoders[0].pulse_threshold = pulse_threshold;
  encoders[0].pins_reversed = pins_reversed;
  encoders[0].direction_old = 'x';
  encoders[0].rel_event_id = REL_Y;

  encoders[1].pin0_irq_name = "shaft2";
  encoders[1].pin1_irq_name = "shaft3";
  encoders[1].device_name = "reciva_quad2";
  encoders[1].pulse_threshold = pulse_threshold_23;
  encoders[1].pins_reversed = pins_reversed_23;
  encoders[1].direction_old = 'x';
  encoders[1].rel_event_id = REL_Z;

  /* Work out which GPIO pins we're using (SHAFT0/1) */
  switch (pin_config)
  {
    case 0:
      /* SHAFT0 = GPG14 (pin function = EINT22)
       * SHAFT1 = GPG15 (pin function = EINT23) */
      encoders[0].active = 1;
      encoders[0].pin0 = S3C2410_GPG14;
      encoders[0].pin1 = S3C2410_GPG15;
      encoders[0].eint0 = S3C2410_GPG14_EINT22;
      encoders[0].eint1 = S3C2410_GPG15_EINT23;
      encoders[0].irq_pin0 = IRQ_EINT22;
      encoders[0].irq_pin1 = IRQ_EINT23;
      break;
    case 1:  
      /* SHAFT0 = GPG3 (pin function = EINT11)
       * SHAFT1 = GPG5 (pin function = EINT13) */
      encoders[0].active = 1;
      encoders[0].pin0 = S3C2410_GPG3;
      encoders[0].pin1 = S3C2410_GPG5;
      encoders[0].eint0 = S3C2410_GPG3_EINT11;
      encoders[0].eint1 = S3C2410_GPG5_EINT13;
      encoders[0].irq_pin0 = IRQ_EINT11;
      encoders[0].irq_pin1 = IRQ_EINT13;
      break;
    case 2:  
      /* SHAFT0 = GPG5 (pin function = EINT13)
       * SHAFT1 = GPG7 (pin function = EINT15) */
      encoders[0].active = 1;
      encoders[0].pin0 = S3C2410_GPG5;
      encoders[0].pin1 = S3C2410_GPG7;
      encoders[0].eint0 = S3C2410_GPG5_EINT13;
      encoders[0].eint1 = S3C2410_GPG7_EINT15;
      encoders[0].irq_pin0 = IRQ_EINT13;
      encoders[0].irq_pin1 = IRQ_EINT15;
      break;

    default:
      return -ENODEV;
  }

  /* Work out which GPIO pins we're using (SHAFT2/3) */
  switch (pin_config_23)
  {
    case 0:
      break;
    case 1:
      encoders[1].active = 1;
      encoders[1].pin0 = S3C2410_GPG11;
      encoders[1].pin1 = S3C2410_GPG10;
      encoders[1].eint0 = S3C2410_GPG11_EINT19;
      encoders[1].eint1 = S3C2410_GPG10_EINT18;
      encoders[1].irq_pin0 = IRQ_EINT19;
      encoders[1].irq_pin1 = IRQ_EINT18;
      break;
    case 2:
      encoders[1].active = 1;
      encoders[1].pin0 = S3C2410_GPG3;
      encoders[1].pin1 = S3C2410_GPF4;
      encoders[1].eint0 = S3C2410_GPG3_EINT11;
      encoders[1].eint1 = S3C2410_GPF4_EINT4;
      encoders[1].irq_pin0 = IRQ_EINT11;
      encoders[1].irq_pin1 = IRQ_EINT4;
      break;
    case 3:
      encoders[1].active = 1;

      encoders[1].pin0 = S3C2410_GPG3;
      encoders[1].eint0 = S3C2410_GPG3_EINT11;
      encoders[1].irq_pin0 = IRQ_EINT11;

      if (machine_is_rirm3())
      {
        encoders[1].pin1 = S3C2410_GPG0;
        encoders[1].eint1 = S3C2410_GPG0_EINT8;
        encoders[1].irq_pin1 = IRQ_EINT8;
      }
      else
      {
        encoders[1].pin1 = S3C2410_GPG5;
        encoders[1].eint1 = S3C2410_GPG5_EINT13;
        encoders[1].irq_pin1 = IRQ_EINT13;
      }
      break;
    default:
      return -ENODEV;
  }

  /* Configure GPIO */
  int i;
  for (i=0; i<MAX_ENCODERS; i++)  
  {
    if (encoders[i].active)
    {
      s3c2410_gpio_cfgpin(encoders[i].pin0, encoders[i].eint0);
      s3c2410_gpio_cfgpin(encoders[i].pin1, encoders[i].eint1);
      // Enable pullups on Barracuda, disable pulldowns on Stingray
      int pull = machine_is_rirm2() ? 0 : 1;
      s3c2410_gpio_pullup(encoders[i].pin0, pull);
      s3c2410_gpio_pullup(encoders[i].pin1, pull);
    }
  }

  if ((pin_config==0) && machine_is_rirm3())
  {
    // Enable external pullups on Stingray
    s3c2410_gpio_cfgpin(S3C2410_GPA5, S3C2410_GPA5_OUT);
    s3c2410_gpio_cfgpin(S3C2410_GPA7, S3C2410_GPA7_OUT);
    s3c2410_gpio_setpin(S3C2410_GPA5, 1);
    s3c2410_gpio_setpin(S3C2410_GPA7, 1);
  }

  /* Enable interrupts - SHAFT0/1 */
  // Stingray fiddle for Samsung changing the external interrupt addresses
  int eint_adjust = machine_is_rirm2() ? 0 : 0x10;
  switch (pin_config)
  {
    case 0:
      /* Enable interrupt on both pins (both edges)
       * Don't need to clear bits as we're setting all that we're interested in */
      rutl_regwrite(0xff000000,  // Set
                    0,           // Clear 
                    (int)S3C2410_EXTINT2 + eint_adjust);

      /* Set the length of filter for external interrupt
       * Filter clock = PCLK
       * Filter width = 0x7f (max) */
      rutl_regwrite(0x7f7f0000,  // Set
                    0xffff0000,  // Clear 
                    (int)S3C2410_EINFLT3 + eint_adjust);
      break;
    case 1:
      /* Enable interrupt on both pins (both edges)
       * Don't need to clear bits as we're setting all that we're interested in */
      rutl_regwrite((7 << 20) | (7 << 12),  // Set
                    0,                      // Clear 
                    (int)S3C2410_EXTINT1 + eint_adjust);
      break;
    case 2:
      /* Enable interrupt on both pins (both edges)
       * Don't need to clear bits as we're setting all that we're interested in */
      rutl_regwrite((7 << 28) | (7 << 20),  // Set
                    0,           // Clear 
                    (int)S3C2410_EXTINT1 + eint_adjust);
      break;
    default:
      BUG ();
  }

  /* Enable interrupts - SHAFT2/3 */
  switch (pin_config_23)
  {
    case 0:
      break;
    case 1:
      /* Enable interrupt on both pins (both edges)
       * Don't need to clear bits as we're setting all that we're interested in */
      rutl_regwrite((7 << 8) | (7 << 12),   // Set
                    0,                      // Clear 
                    (int)S3C2410_EXTINT2 + eint_adjust);
      break;
    case 2:
      // GPF4 - EINT4  
      rutl_regwrite((7 << 16),              // Set
                    0,                      // Clear 
                    (int)S3C2410_EXTINT0 + eint_adjust);
      // GPG3 - EINT11
      rutl_regwrite((7 << 12),   // Set
                    0,                      // Clear 
                    (int)S3C2410_EXTINT1 + eint_adjust);
      break;
    case 3:
      if (machine_is_rirm3())
      {
        // GPG0 - EINT8
        rutl_regwrite((7 << 0),   // Set
                      0,                      // Clear 
                      (int)S3C2410_EXTINT1 + eint_adjust);
      }
      else
      {
        // GPG5 - EINT13
        rutl_regwrite((7 << 20),   // Set
                      0,                      // Clear 
                      (int)S3C2410_EXTINT1 + eint_adjust);
      }

      // GPG3 - EINT11
      rutl_regwrite((7 << 12),   // Set
                    0,                      // Clear 
                    (int)S3C2410_EXTINT1 + eint_adjust);
      break;
    default:
      BUG ();
  }

  /* Request IRQs and set up input system */
  for (i=0; i<MAX_ENCODERS; i++)
  {
    if (encoders[i].active)
    {
      /* Note initial state of shaft encoder gpio pins */
      encoders[i].shaft_old = reciva_qp_pins_read(&encoders[i]);

      /* Set up input system */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
      encoders[i].input_dev = input_allocate_device();
#else
      encoders[i].input_dev = kmalloc (sizeof (*encoders[i].input_dev), GFP_KERNEL);
      memset (encoders[i].input_dev, 0, sizeof (*encoders[i].input_dev));
#endif
      encoders[i].input_dev->evbit[0] = BIT(EV_REL);
      set_bit (encoders[i].rel_event_id, encoders[i].input_dev->relbit);
      encoders[i].input_dev->name = encoders[i].device_name;
      input_register_device (encoders[i].input_dev);

      /* Set up the interrupts */
      request_irq(encoders[i].irq_pin0, reciva_quad_int_handler, 0, encoders[i].pin0_irq_name, NULL);
      request_irq(encoders[i].irq_pin1, reciva_quad_int_handler, 0, encoders[i].pin1_irq_name, NULL);
    }
  }
  return 0;
}

/****************************************************************************
 * Module cleanup
 ****************************************************************************/
static void __exit
unregister_quad (void)
{
  printk(PFX "unregister_quad\n");

  int i;
  for (i=0; i<MAX_ENCODERS; i++)
  {
    if (encoders[i].active)
    {
      free_irq(encoders[i].irq_pin0, NULL);
      free_irq(encoders[i].irq_pin1, NULL);

      input_unregister_device (encoders[i].input_dev);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#else
      kfree (encoders[i].input_dev);
#endif
    }
  }
}

/****************************************************************************
 * Handles an interrupt on GPIO14/15
 * Parameters : Standard interrupt handler params. Not used.
 ****************************************************************************/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
static irqreturn_t reciva_quad_int_handler(int irq, void *dev)
#else
static void reciva_quad_int_handler(int irq, void *dev, struct pt_regs *regs)
#endif
{
  encoder_t *e = NULL;

  int i;
  for (i=0; i<MAX_ENCODERS; i++)
  {
    if (irq == encoders[i].irq_pin0 || irq == encoders[i].irq_pin1)
      e = &encoders[i];
  }

  /* Work out rotation direction and let application know about it */
  if (e)
    do_quad(e);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
  return IRQ_HANDLED;
#endif
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
static void do_quad(encoder_t *e)
{
  int direction;
  /* Use this to determine the direction of rotation */
  static int lookup[4][4] =
  {
    {'x', 'a', 'c', 'x'},
    {'c', 'x', 'x', 'a'},
    {'a', 'x', 'x', 'c'},
    {'x', 'c', 'a', 'x'}
  };

  /* Read current status of shaft pins */
  e->shaft_new = reciva_qp_pins_read(e);

  /* Work out direction of rotation */
  direction = lookup[e->shaft_old][e->shaft_new];

  /* Need to have 4 in a row to register as a rotation */
  if (e->direction_old == direction)
  {
    e->transition_count++;

    if (e->transition_count >= e->pulse_threshold)
    {
      e->transition_count = 0;
      switch (direction)
      {
        case 'c':
          /* Clockwise */
          input_report_rel (e->input_dev, e->rel_event_id, +1);
          break;
        case 'a':
          /* Anticlockwise */
          input_report_rel (e->input_dev, e->rel_event_id, -1);
          break;
        default:
          /* Unexpected transition - ignore */
          break;
      }
    }
  }
  else
  {
    e->transition_count = 1;
    e->direction_old = direction;
  }

  /* Note old value for next time round */
  e->shaft_old = e->shaft_new;
}


module_init(register_quad);
module_exit(unregister_quad);

MODULE_DESCRIPTION("Reciva Quad");
MODULE_LICENSE("GPL");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
module_param(pulse_threshold, int, S_IRUGO);
module_param(pulse_threshold_23, int, S_IRUGO);
module_param(pin_config, int, S_IRUGO);
module_param(pin_config_23, int, S_IRUGO);
module_param(pins_reversed, int, S_IRUGO);
module_param(pins_reversed_23, int, S_IRUGO);
#else
MODULE_PARM(pulse_threshold, "i");
MODULE_PARM(pulse_threshold_23, "i");
MODULE_PARM(pin_config, "i");
MODULE_PARM(pin_config_23, "i");
MODULE_PARM(pins_reversed, "i");
MODULE_PARM(pins_reversed_23, "i");
#endif

