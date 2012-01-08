/*
 * linux/reciva/reciva_edge_detector.c
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
 * Edge detector
 * Send events on rising and falling edges on GPIO input pin
 * Currently only supports 1 source
 * Debounce not supported yet
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
#include <asm/io.h>

#include "reciva_util.h"


   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

/* Edge Detector */
typedef struct
{
  /* Indicates if this encoder is in use */
  int active;

  /* GPIO/interrupt details */
  int gpio;                   // GPIO number
  int irq;                    // IRQ number

  /* IRQ name */
  char *irq_name;

  /* Device name */
  char *device_name;

  /* Event ID for reporting events to user spave (via input_report_rel) */
  int rel_event_id;

  /* Use different offset for each GPIO so it can be identified at other end */
  int rel_offset;

  /* Debounce timer (optional) */
  int debounce_required;
  struct timer_list timer;

} detector_t;


   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

static int __init reciva_init (void);
static void __exit reciva_exit(void);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
static irqreturn_t reciva_int_handler(int irq, void *dev);
#else
static void reciva_int_handler(int irq, void *dev, struct pt_regs *regs);
#endif


   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

#define PFX "EDGE:"

   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

/* Edge Detectors */
#define MAX_DETECTORS 1
static detector_t detectors[MAX_DETECTORS];

/* Defines the pins we are using */
static int pin_config;  // 0 = J2-3 (GPG5)

/* Communication with user space */
static struct input_dev *input_dev;



   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/

/****************************************************************************
 * Reports edge detection to user space
 ****************************************************************************/
static void report_edge(detector_t *d)
{
  int level = s3c2410_gpio_getpin (d->gpio);
  int rel;

  // Need to change pin to inout before reading on s3c2412
  if (machine_is_rirm3())
  {
    switch (pin_config)
    {
      case 0:
        /* Set up gpio */
        s3c2410_gpio_cfgpin (detectors[0].gpio, S3C2410_GPG0_INP);
        level = s3c2410_gpio_getpin (d->gpio);
        s3c2410_gpio_cfgpin (detectors[0].gpio, S3C2410_GPG0_EINT8);
        break;
      default:
        break;
    }
  }

  if (level)
    rel = d->rel_offset;
  else
    rel = -d->rel_offset;

  input_report_rel (input_dev, d->rel_event_id, rel);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
  input_sync (input_dev);
#endif
}

/****************************************************************************
 * Handle edge detection
 ****************************************************************************/
static void handle_edge_detect(detector_t *d)
{
  if (d->debounce_required)
  {
    // XXX TODO
  }
  else
  {
    report_edge(d);
  }
}

/****************************************************************************
 * Interrupt handler
 * Parameters : Standard interrupt handler params. Not used.
 ****************************************************************************/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
static irqreturn_t reciva_int_handler(int irq, void *dev)
#else
static void reciva_int_handler(int irq, void *dev, struct pt_regs *regs)
#endif
{
  detector_t *d = NULL;

  int i;
  for (i=0; i<MAX_DETECTORS; i++)
  {
    if (irq == detectors[i].irq)
      d = &detectors[i];
  }

  /* Work out rotation direction and let application know about it */
  if (d)
    handle_edge_detect(d);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
  return IRQ_HANDLED;
#endif
}

/****************************************************************************
 * Debounce function
 ****************************************************************************/
static void debounce_function(unsigned long ignore)
{
  // XXX TODO
}

/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init
reciva_init (void)
{
  printk(PFX "init\n");
  printk(PFX "  pin_config=%d\n", pin_config);

  memset(detectors, 0, sizeof(detectors));

  /* Work out which GPIO pins we're using etc */
  switch (pin_config)
  {
    case 0:
      /* [0] = J2-3 (GPG5) */
      detectors[0].active = 1;
      detectors[0].irq_name = "edge0";
      detectors[0].device_name = "reciva_edge0";
      detectors[0].rel_event_id = REL_MISC;
      detectors[0].rel_offset = 1;
      detectors[0].debounce_required = 0;
      break;
    default:
      return -ENODEV;
  }

  // Some module pins are mapped to different GPIO on stingray
  if (machine_is_rirm3())
  {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    switch (pin_config)
    {
      case 0:
        /* [0] = J2-3 (GPG0) */
        detectors[0].gpio = S3C2410_GPG0;
        detectors[0].irq = IRQ_EINT8;

        /* Set up gpio */
        s3c2410_gpio_cfgpin (detectors[0].gpio, S3C2410_GPG0_EINT8);
        s3c2410_gpio_pullup (detectors[0].gpio, 1); // pulldown off

        /* Enable interrupt on both edges */
        rutl_regwrite((0x0f << (0*4)),  // Set
                      0,                // Clear 
                      (int)S3C2412_EXTINT1);
        break;
      default:
        break;
    }
#endif
  }
  else
  {
    switch (pin_config)
    {
      case 0:
        /* [0] = J2-3 (GPG0) */
        detectors[0].gpio = S3C2410_GPG5;
        detectors[0].irq = IRQ_EINT13;

        /* Set up gpio */
        s3c2410_gpio_cfgpin (detectors[0].gpio, S3C2410_GPG5_EINT13);
        s3c2410_gpio_pullup (detectors[0].gpio, 1); // pullup off

        /* Enable interrupt on both edges */
        rutl_regwrite((0x0f << (5*4)),  // Set
                      0,                // Clear 
                      (int)S3C2410_EXTINT1);
        break;
      default:
        break;
    }
  }

  /* Set up input system */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
  input_dev = input_allocate_device();
#else
  input_dev = kmalloc (sizeof (*input_dev), GFP_KERNEL);
  memset (input_dev, 0, sizeof (*input_dev));
#endif
  input_dev->evbit[0] = BIT(EV_REL);
  set_bit (REL_MISC, input_dev->relbit);
  input_dev->name = "reciva_edge";
  input_register_device (input_dev);

  /* Request IRQs */
  int i;
  for (i=0; i<MAX_DETECTORS; i++)
  {
    if (detectors[i].active)
    {
      /* Set up the interrupts */
      request_irq(detectors[i].irq, reciva_int_handler, 0, detectors[i].irq_name, NULL);

      /* Set up debounce timers */
      if (detectors[i].debounce_required)
      {
        init_timer(&detectors[i].timer);
        detectors[i].timer.function = debounce_function;
      }
    }
  }

  return 0;
}

/****************************************************************************
 * Module cleanup
 ****************************************************************************/
static void __exit
reciva_exit (void)
{
  printk(PFX "exit\n");

  /* Input device */
  input_unregister_device (input_dev);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#else
  kfree (input_dev);
#endif

  /* Free IRQs and debounce timers */
  int i;
  for (i=0; i<MAX_DETECTORS; i++)
  {
    if (detectors[i].active)
    {
      free_irq(detectors[i].irq, NULL);

      if (detectors[i].debounce_required)
        del_timer(&detectors[i].timer);
    }
  }
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
module_param(pin_config, int, S_IRUGO);
#else
MODULE_PARM(pin_config, "i");
#endif

module_init(reciva_init);
module_exit(reciva_exit);

MODULE_DESCRIPTION("Reciva Edge Detector");
MODULE_LICENSE("GPL");


