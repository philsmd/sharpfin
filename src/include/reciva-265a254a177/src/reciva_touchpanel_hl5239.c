/*
 * Reciva Touchpanel - Holylite HL5239
 * Copyright (c) $Date: 2007-07-24 15:41:12 $ Reciva Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
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
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>

#include "reciva_touchpanel_hl5239.h"
#include "reciva_util.h"
#include "reciva_gpio.h"
#include "reciva_keypad_generic.h"


   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

/* Debug prefix */
#define PREFIX "RT_HL5239:"

/* Input detection is reset if we haven't received an interrupt
 * within this period */
#define NEW_KEYPRESS_GAP        ((HZ*20)/1000) /* 20 ms */

/* Max number of touchpanel keys */
#define RT_HL5239_MAX_KEYS 28


   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

static const char acModuleName[] = "Reciva Touchpanel HL5239";

/* Time of last interrupt */
static unsigned long last_interrupt_time;

/* Number of interrupt since new key event detected */
static int clock_irq_count;

/* Converts touchpanel data to key events */
static int cf966_lookup[RT_HL5239_MAX_KEYS] =
{
  /*  0 */  RKD_BACK,
  /*  1 */  RKD_DOWN,
  /*  2 */  RKD_SELECT,
  /*  3 */  RKD_UNUSED,
  /*  4 */  RKD_UP,
  /*  5 */  RKD_REPLY,
  /*  6 */  RKD_SHIFT,
  /*  7 */  RKD_IR_FM_MODE_SWITCH,
  /*  8 */  RKD_PRESET_1,
  /*  9 */  RKD_PRESET_3,
  /* 10 */  RKD_PRESET_4,
  /* 11 */  RKD_PRESET_2,
  /* 12 */  RKD_ALARM,
  /* 13 */  RKD_POWER,
  /* 14 */  RKD_PRESET_5,
  /* 15 */  RKD_BROWSE_QUEUE,
  /* 16 */  RKD_UNUSED,
  /* 17 */  RKD_UNUSED,
  /* 18 */  RKD_UNUSED,
  /* 19 */  RKD_UNUSED,
  /* 20 */  RKD_UNUSED,
  /* 21 */  RKD_UNUSED,
  /* 22 */  RKD_UNUSED,
  /* 23 */  RKD_UNUSED,
  /* 24 */  RKD_UNUSED,
  /* 25 */  RKD_UNUSED,
  /* 26 */  RKD_UNUSED,
  /* 27 */  RKD_UNUSED,
};



   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/

/****************************************************************************
 * Set level of CLOCK, DATA, ABLEN pins
 ****************************************************************************/
static inline void set_CLOCK(int level)
{
  /* GPG11 (CLOCK) */
  if (level)
    rutl_regwrite((1 << 11), (0 << 11), S3C2410_GPGDAT); // Set high
  else
    rutl_regwrite((0 << 11), (1 << 11), S3C2410_GPGDAT); // Set low
}
static inline void set_DATA(int level)
{
  /* GPG10 (DATA) */
  if (level)
    rutl_regwrite((1 << 10), (0 << 10), S3C2410_GPGDAT); // Set high
  else
    rutl_regwrite((0 << 10), (1 << 10), S3C2410_GPGDAT); // Set low
}
static inline void set_ABLEN(int level)
{
  /* GPD10 (ABLEN) */
  if (level)
    rutl_regwrite((1 << 10), (0 << 10), S3C2410_GPDDAT); // Set high
  else
    rutl_regwrite((0 << 10), (1 << 10), S3C2410_GPDDAT); // Set low
}

/****************************************************************************
 * Get level of DATA
 ****************************************************************************/
static inline int get_DATA(void)
{
  int level = 0;
  unsigned long temp;

  temp = __raw_readl(S3C2410_GPGDAT);
  if (temp & (0x01<<10))
    level = 1;

  return level;
}

/****************************************************************************
 * Converts a touchpanel event into a key event
 * data - data from touchpanel (bit 5 = key state, 4:0 = key id)
 ****************************************************************************/
static void convert_to_key_event(int data, int *event_id, int *press)
{
  /* On the 7x4 sample touchpanel the key codes are 
   * Note : (0xaa) = release value
   * ---------------------------------------------------------------
   * | 0x38  |  0x34  |  0x30  |  0x2c  |  0x28  |  0x24  |  0x20  |
   * |(0x18) | (0x14) | (0x10) | (0x0c) | (0x08) | (0x04) | (0x00) |
   * ---------------------------------------------------------------
   * | 0x39  |  0x35  |  0x31  |  0x2d  |  0x29  |  0x25  |  0x21  |
   * |(0x19) | (0x15) | (0x11) | (0x0d) | (0x09) | (0x05) | (0x01) |
   * ---------------------------------------------------------------
   * | 0x3a  |  0x36  |  0x32  |  0x2e  |  0x2a  |  0x26  |  0x22  |
   * |(0x1a) | (0x16) | (0x12) | (0x0e) | (0x0a) | (0x06) | (0x02) |
   * ---------------------------------------------------------------
   * | 0x3b  |  0x37  |  0x33  |  0x2f  |  0x2b  |  0x27  |  0x23  |
   * |(0x1b) | (0x17) | (0x13) | (0x0f) | (0x0b) | (0x07) | (0x03) |
   * ---------------------------------------------------------------
   */

  *event_id = RKD_UNUSED;
  *press = (data & 0x20) >> 5;

  int key_number = data & 0x1f;

  if (key_number < RT_HL5239_MAX_KEYS)
    *event_id = cf966_lookup[key_number];
}

/****************************************************************************
 * Handles an interrupt on GPIO2/3/4/5
 * Parameters - Standard interrupt handler params. Not used.
 ****************************************************************************/
static void touchpanel_int_handler(int irq, void *dev, struct pt_regs *regs)
{
  int level = get_DATA();
  static int data;

  if ((jiffies - last_interrupt_time) > NEW_KEYPRESS_GAP)
  {
    clock_irq_count = 1;
    data = 0;
  }
  else
  {
    clock_irq_count++;

    if (clock_irq_count <= 7)
    {
      data <<= 1;
      data |= level;
    }

    if (clock_irq_count == 7)
    {
      /* Report key event to user space */
      int event;
      int press;       
      convert_to_key_event(data, &event, &press);
      printk(PREFIX "KEY CHANGE d=%08x e=%d p=%d\n", data, event, press);
      rkg_report_key(event, press);

      /* Set up ready for next 7 cycle key event */
      clock_irq_count = 1;
      data = 0;
    }
  }

  last_interrupt_time = jiffies;
}

/****************************************************************************
 * Setup gpio for writing to the device
 ****************************************************************************/
static void setup_gpio_for_writing(void)
{
  /* Set ABLEN high - write enable device */
  set_ABLEN(1);

  /* GPD10 (ABLEN) */
  rutl_regwrite((1 << 10), (0 << 10), S3C2410_GPDUP) ; // Disable pullup
  rutl_regwrite((1 << 20), (3 << 20), S3C2410_GPDCON); // Set as output
  udelay(1);    /* Might not need this delay */

  /* GPG11 (CLOCK) */
  rutl_regwrite((1 << 11), (0 << 11), S3C2410_GPGUP) ; // Disable pullup
  rutl_regwrite((0 << 11), (1 << 11), S3C2410_GPGDAT); // Set data low
  rutl_regwrite((1 << 22), (3 << 22), S3C2410_GPGCON); // Set as output

  /* GPG10 (DATA) */
  rutl_regwrite((1 << 10), (0 << 10), S3C2410_GPGUP) ; // Disable pullup
  rutl_regwrite((0 << 10), (1 << 10), S3C2410_GPGDAT); // Set data low
  rutl_regwrite((1 << 20), (3 << 20), S3C2410_GPGCON); // Set as output
}

/****************************************************************************
 * Setup gpio for reading from the device
 ****************************************************************************/
static void setup_gpio_for_reading(void)
{
  /* GPG10 (DATA) */
  rutl_regwrite((1 << 10), (0 << 10), S3C2410_GPGUP) ; // Disable pullup
  rutl_regwrite((0 << 20), (3 << 20), S3C2410_GPGCON); // Set as input

  /* GPG11 (CLOCK) */
  rutl_regwrite((1 << 11), (0 << 11), S3C2410_GPGUP) ; // Disable pullup
  rutl_regwrite((2 << 22), (3 << 22), S3C2410_GPGCON); // EINT19

  /* Set the length of filter for external interrupt
   * Filter clock = PCLK
   * Filter width = 0x7f (max) */
  rutl_regwrite(0x7f000000, 0xff000000, S3C2410_EINFLT2);

  /* EINT19 - falling edge triggerred interrupts - filter on */
  rutl_regwrite(0x0000a000, 0x0000f000, S3C2410_EXTINT2);

  /* Set ABLEN low - write protect device */
  udelay(1);    /* Might not need this delay */
  set_ABLEN(0);
}

/****************************************************************************
 * Send one bit of data top the module
 ****************************************************************************/
static void send_bit(int level)
{
  set_CLOCK(0);    /* Drive CLOCK low */
  set_DATA(level); /* Set up data */
  udelay(1);       /* Might not need this delay */
  set_CLOCK(1);    /* Drive CLOCK high */
  udelay(1);       /* Might not need this delay */
}

/****************************************************************************
 * Send data to the module. Data is clocked out msb first
 * data - up to 32 bits worth of data
 * count - number of bits to send (1 to 32)
 ****************************************************************************/
static void send_data(int data, int count)
{
  while (count)
  {
    send_bit((data >> (count -1)) & 0x01);
    count--;
  }
}

/****************************************************************************
 * Initialise device
 ****************************************************************************/
static void init_device(void)
{
  int initialisation_data = 0x00;

  /* This array is to make it more convenient to experiment with the 
   * initialisation values */
  static const int init_values[] =
  {
    /* 000 = 0.26 sec */
    0,  // Bit 28  T2         Delay - key stable                           
    0,  // Bit 27  T1         Delay - key stable                           
    0,  // Bit 26  T0         Delay - key stable                           

    /* 000 = 0.26 sec */
    0,  // Bit 25  DEL2       Delay - power on to stable                   
    0,  // Bit 24  DEL1       Delay - power on to stable                   
    0,  // Bit 23  DEL0       Delay - power on to stable                   

    /* 100 = 16 */
    1,  // Bit 22  R2         Number of keys                               
    0,  // Bit 21  R1         Number of keys                               
    0,  // Bit 20  R0         Number of keys                               

    /* 0111 = medium sensitivity */
#ifdef CF966_FIRST_SAMPLE
    /* Values for the first cf966 sample (28 keys with paper) */
    1,  // Bit 19  SENSE3     Sensitivity                            
    1,  // Bit 18  SENSE2     Sensitivity                            
    1,  // Bit 17  SENSE1     Sensitivity                            
    1,  // Bit 16  SENSE0     Sensitivity                            
#else
    /* Values for the real cf966 sample (16 keys with plastic front) */
    0,  // Bit 19  SENSE3     Sensitivity                            
    0,  // Bit 18  SENSE2     Sensitivity                            
    0,  // Bit 17  SENSE1     Sensitivity                            
    1,  // Bit 16  SENSE0     Sensitivity                            
#endif

    /* 011 = mid stability (least likely to detect a key press) */
    0,  // Bit 15  STAB2      Stability                              
    1,  // Bit 14  STAB1      Stability                              
    1,  // Bit 13  STAB0      Stability                              

    /* 11 = max reduction */
    1,  // Bit 12  DW1        Sensitivity reduction during scan      
    1,  // Bit 11  DW0        Sensitivity reduction during scan      

    /* 11 = max transmission rate */
    0,  // Bit 10  TRATE1     Transmission rate                      
    0,  // Bit 9   TRATE0     Transmission rate                      

    /* 00 = don't multiplex */
    0,  // Bit 8   MUXPOL     Multiplexing mode                      
    0,  // Bit 7   MULTIPLEX  Multiplexing mode                      

    /* 0 = max operating frequency */
    0,  // Bit 6   FRA        Operating frequency                    

    /* 1 = max checking that a key has been pressed */
    1,  // Bit 5   DCHECK     Double checking of key presses         

    /* 0 = use noise detection pin */
    1,  // Bit 4   DISNOISE   Disable NOISE1 pin                     

    /* 0= 3v */
    0,  // Bit 3   VSL        Operating voltage                      

    /* 1 = takes more time for noise to affect system */    
    1,  // Bit 2   SLOWREF    Noise interference                     

    /* 1 =  prevent interference from other keys */
    0,  // Bit 1   DEINTER    Prevent interference from other keys   

    /* Output sent only when state of key changes */
    0,  // Bit 0   LEVEL      Output mode                            
  };

  int i;
  for (i=0; i<29; i++)
  {
    initialisation_data <<= 1;
    initialisation_data |= init_values[i];
  }

  /* Initialise device */
  printk(PREFIX "init_device d=%08x\n", initialisation_data);
  setup_gpio_for_writing();
  send_data(initialisation_data, 29);

  /* Set up to receive key events from device */
  setup_gpio_for_reading();
  request_irq(IRQ_EINT19, touchpanel_int_handler, 0, "EINT19 (hl5239)", (void *)11);
}


   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/


/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
reciva_touchpanel_hl5239_init(void)
{
  printk(PREFIX "%s module: loaded\n", acModuleName);
  init_device();
  return 0;
}

/****************************************************************************
 * Module tidy up
 ****************************************************************************/
static void __exit 
reciva_touchpanel_hl5239_exit(void)
{
  printk(PREFIX "%s module: unloaded\n", acModuleName);
  free_irq(IRQ_EINT19, (void *)11);
}


module_init(reciva_touchpanel_hl5239_init);
module_exit(reciva_touchpanel_hl5239_exit);

MODULE_LICENSE("GPL");

