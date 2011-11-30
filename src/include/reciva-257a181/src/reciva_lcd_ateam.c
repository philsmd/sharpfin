/*
 * Reciva LCD driver code for "A-Team" character mapped LCD module
 * Copyright (c) 2004 Nexus Electronics Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

   /*************************************************************************/
   /***                        Include Files                              ***/
   /*************************************************************************/

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/smp_lock.h>
#include <linux/string.h>
#include <linux/delay.h>

#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/io.h>

#include <asm/arch-s3c2410/S3C2410-timer.h>

#include "reciva_lcd.h"
#include "reciva_leds.h"
#include "reciva_lcd_ateam.h"
#include "lcd_generic.h"
#include "reciva_util.h"
#include "reciva_backlight.h"
#include "reciva_keypad_generic.h"


/* There are a lot of LCD modules that are controlled in pretty much the same
 * way. This parameter defines the actual LCD module in use so that any minor
 * differences can be handled. If no module parameter is specified then it will
 * default to MODULE_ATEAM_4BIT
 * 0 = standard ateam driver chip (used on VPRO units) 
 *     4 bit interface, 16*2 display
 * 1 = standard ateam driver chip (used on VPRO units) 
 *     8 bit interface, 16*2 display
 * 2 = Sitronix ST7032 (as used on PURE Evoke) 
 * 3 = LC1641-SRNH6 (Hipshing)
 *     4 bit interface, 16*4 display 
 * 4 = AMAX_EDC162A40GXBBB on config1007 hardware
 *     (GPC3= nAmpPD, dafault value =1 ie ON)
 *     4 bit interface, 16*2 display 
 * 5 = 4 bit, 1 line mode
 * 6 = HEM1601B
 *     8 bit, 16x1
 * 7 = 8 bit interface, 16x2 screen but second line disabled
 * 8 = ?
 * 9 = same as MODULE_HIPSHING_LC1641SRNH6 but with different LED control
 * 10 = same as MODULE_HIPSHING_LC1641SRNH6 but only 2 lines
 * 11 = cf989 - 2 line, 8bit interface
 * 12 - cf990 - 2 line, 4 bit (driver IC = Sitronix ST7036)
 */
typedef enum
{
  MODULE_ATEAM_4BIT                        = 0,
  MODULE_ATEAM_8BIT                        = 1,
  MODULE_SITRONIX_ST7032                   = 2,
  MODULE_HIPSHING_LC1641SRNH6              = 3,
  MODULE_AMAX_EDC162A40GXBBB               = 4,
  MODULE_4BIT_1LINE                        = 5,
  MODULE_HEM1601B                          = 6,
  MODULE_8BIT_1LINE                        = 7,
  MODULE_SDEC_S2P16                        = 8,
  MODULE_HIPSHING_LC1641SRNH6_CF997        = 9,
  MODULE_CF979                             = 10,
  MODULE_CF989                             = 11,
  MODULE_CF990                             = 12,

} module_id_t;
static module_id_t lcd_module_id = MODULE_ATEAM_4BIT;
MODULE_PARM(lcd_module_id, "i");

static int test_mode;
MODULE_PARM(test_mode, "i");

static int pins_shared;
MODULE_PARM(pins_shared, "i");



   /*************************************************************************/

static int  __init ateam_lcd_init(void);
static void __exit ateam_lcd_exit(void);

/* Functions in lcd interface */
static void ateam_init_hardware(void);
static int  ateam_draw_screen(char **acText, int iX, int iY,
                              int iCursorType, int *piArrows, int *piLineContents);
static void ateam_clear_screen(void);
static void ateam_set_led(unsigned int mask);
static void ateam_power_off(void);

/* Local helper functions */

static void cycle (int a, int d, int iDelay);
static void cycle_4bit (int iA0, int iData, int iDelay);
static void cycle_8bit (int iA0, int iData, int iDelay);
static void setup_gpio (void);
static void init_function_set_register(int reinitialise);
static void setup_user_defined_chars(void);

static void draw_cursor (int x, int y);
static void init_registers(void);
static void redraw_screen(void);

static void setup_data_gpio(void);
static void tristate_data_gpio(void);

   /*************************************************************************/
   /***                        Static data                                ***/
   /*************************************************************************/


   /*************************************************************************/
   /***                        Local defines                              ***/
   /*************************************************************************/

/* GPIO registers */
#define GPIO_REG(x) *((volatile unsigned int *)(S3C2410_VA_GPIO + (x)))

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



/* Control interface width (4bit or 8bit) */
#define INTERFACE_WIDTH_ATEAM_4BIT              4
#define INTERFACE_WIDTH_ATEAM_8BIT              8
#define INTERFACE_WIDTH_SITRONIX_ST7032         4
#define INTERFACE_WIDTH_HIPSHING_LC1641SRNH6    4
#define INTERFACE_WIDTH_AMAX_EDC162A40GXBBB     4

/* Screen dimensions for all module IDs */
#define HEIGHT_ATEAM_4BIT                       2
#define HEIGHT_ATEAM_8BIT                       2
#define HEIGHT_SITRONIX_ST7032                  2
#define HEIGHT_HIPSHING_LC1641SRNH6             4
#define HEIGHT_AMAX_EDC162A40GXBBB              2
#define MAX_SCREEN_HEIGHT                       HEIGHT_HIPSHING_LC1641SRNH6

/* note - width doesn't include the 2 arrows */
#define WIDTH_ATEAM_4BIT                        14
#define WIDTH_ATEAM_8BIT                        14
#define WIDTH_SITRONIX_ST7032                   14
#define WIDTH_HIPSHING_LC1641SRNH6              14
#define WIDTH_AMAX_EDC162A40GXBBB               14
#define MAX_SCREEN_WIDTH                        WIDTH_ATEAM_4BIT




   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/
   
   /*************************************************************************/
   /***                        Static data                                ***/
   /*************************************************************************/

static char acModuleName[] = "Reciva LCD Ateam Generic";

static const struct rutl_unicode_charmap asCharMap[] = {
  { LCD_VOLUME_2_BAR,     '\1' },
  { LCD_VOLUME_1_BAR,     '\2' },
  { "\\",                 '\3' },
  { LCD_LEFT_ARROW_SOLID, '\4' },
  { LCD_RIGHT_ARROW_SOLID,'\5' },
#include "lcd_simple_charmap.c"
  { NULL, 0 }
};

/* Note that any elements not explicitly initialised here are guaranteed to 
 * be zero initialised */
static const struct reciva_lcd_driver ateam_driver = {
  name:          acModuleName,
  init_hardware: ateam_init_hardware,
  get_height:    reciva_lcd_get_height,
  get_width:     reciva_lcd_get_width,
  draw_screen:   ateam_draw_screen,
  clear_screen:  ateam_clear_screen,
  set_backlight: reciva_bl_set_backlight,
  set_led:       ateam_set_led,
  power_off:     ateam_power_off,
  redraw_screen: redraw_screen,
  charmap:       asCharMap,
};

/* Screen dimensions */
static int screen_width;            // Screen width excluding arrows
static int screen_width_inc_arrows; // Screen width including arrows
static int screen_height;

/* Control interface width (4bit or 8bit) */
static int interface_width;

/* Protect acces to cycle() function */
static spinlock_t cycle_lock;


/****************************************************************************
 * Get access to the shared GPIO pins
 ****************************************************************************/
static void get_shared_gpio(void)
{
  if (pins_shared)
  {
    rkg_access_request();
    setup_data_gpio();
  }
}

/****************************************************************************
 * Release access to the shared GPIO pins
 ****************************************************************************/
static void release_shared_gpio(void)
{
  if (pins_shared)
  {
    tristate_data_gpio();
    rkg_access_release();
  }
}

/****************************************************************************
 * Redraw the screen
 * Also reinit some registers on certain configs
 ****************************************************************************/
static void redraw_screen(void)
{
  get_shared_gpio();

  init_function_set_register(1);
  setup_user_defined_chars();
  reciva_lcd_redraw();

  release_shared_gpio();
}

/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init
ateam_lcd_init (void)
{
  cycle_lock = SPIN_LOCK_UNLOCKED;

  switch (lcd_module_id)
  {
    case MODULE_ATEAM_4BIT:
      screen_width = WIDTH_ATEAM_4BIT;
      screen_height = HEIGHT_ATEAM_4BIT;
      interface_width = INTERFACE_WIDTH_ATEAM_4BIT;
      printk("RLA: lcd_module_id = MODULE_ATEAM_4BIT\n");
      break;
    case MODULE_ATEAM_8BIT:
      screen_width = WIDTH_ATEAM_8BIT;
      screen_height = HEIGHT_ATEAM_8BIT;
      interface_width = INTERFACE_WIDTH_ATEAM_8BIT;
      printk("RLA: lcd_module_id = MODULE_ATEAM_8BIT\n");
      break;
    case MODULE_CF989:
      screen_width = 14;
      screen_height = 2;
      interface_width = 4;
      printk("RLA: lcd_module_id = MODULE_CF989\n");
      break;
    case MODULE_SITRONIX_ST7032:
      screen_width = WIDTH_SITRONIX_ST7032;
      screen_height = HEIGHT_SITRONIX_ST7032;
      interface_width = INTERFACE_WIDTH_SITRONIX_ST7032;
      printk("RLA: lcd_module_id = MODULE_SITRONIX_ST7032\n");
      break;
    case MODULE_CF990:
      screen_width = 14;
      screen_height = 2;
      interface_width = 4;
      break;
    case MODULE_HIPSHING_LC1641SRNH6_CF997:
      printk("RLA: lcd_module_id = MODULE_HIPSHING_LC1641SRNH6_CF997\n");
    case MODULE_HIPSHING_LC1641SRNH6:
      screen_width = WIDTH_HIPSHING_LC1641SRNH6;
      screen_height = HEIGHT_HIPSHING_LC1641SRNH6;
      interface_width = INTERFACE_WIDTH_HIPSHING_LC1641SRNH6;
      printk("RLA: lcd_module_id = MODULE_HIPSHING_LC1641SRNH6\n");
      break;
    case MODULE_AMAX_EDC162A40GXBBB:
      screen_width = WIDTH_AMAX_EDC162A40GXBBB;
      screen_height = HEIGHT_AMAX_EDC162A40GXBBB;
      interface_width = INTERFACE_WIDTH_AMAX_EDC162A40GXBBB;
      printk("RLA: lcd_module_id = MODULE_AMAX_EDC162A40GXBBB\n");
      break;
    case MODULE_4BIT_1LINE:
      screen_width = 14;
      screen_height = 1;
      interface_width = 4;
      printk("RLA: lcd_module_id = MODULE_4BIT_1LINE\n");
      break;
    case MODULE_8BIT_1LINE:
      screen_width = 14;
      screen_height = 1;
      interface_width = 8;
      printk("RLA: lcd_module_id = MODULE_8BIT_1LINE\n");
      break;
    case MODULE_HEM1601B:
      screen_width = 14;
      screen_height = 1;
      interface_width = 8;
      printk("RLA: lcd_module_id = MODULE_HEM1601B\n");
      break;
    case MODULE_SDEC_S2P16:
      screen_width = 14;
      screen_height = 2;
      interface_width = 8;
      printk("RLA: lcd_module_id = MODULE_SDEC_S2P16\n");
      break;
    case MODULE_CF979:
      screen_width = 14;
      screen_height = 2;
      interface_width = 4;
      printk("RLA: lcd_module_id = MODULE_CF979\n");
      break;
  }
  screen_width_inc_arrows = screen_width + 2;
  printk("RLA: screen_height = %d\n", screen_height);
  printk("RLA: screen_width = %d\n", screen_width);
  printk("RLA: screen_width_inc_arrows = %d\n", screen_width_inc_arrows);
  printk("RLA: interface_width = %d\n", interface_width);
    
  int ret = reciva_lcd_init(&ateam_driver, screen_height, screen_width);

  return ret;
}

/****************************************************************************
 * Sets up the user defined characters
 ****************************************************************************/
static void setup_user_defined_chars(void)
{
  // Pattern 2 (use 0x01 to draw this one) - 2 volume bars
  cycle (0, 0x40 | (1 << 3), 39); /* Set address */
  cycle (1, 0x1b, 39);
  cycle (1, 0x1b, 39);
  cycle (1, 0x1b, 39);
  cycle (1, 0x1b, 39);
  cycle (1, 0x1b, 39);
  cycle (1, 0x1b, 39);
  cycle (1, 0x1b, 39);
  switch (lcd_module_id)
  {
    case MODULE_ATEAM_4BIT:
    case MODULE_ATEAM_8BIT:
    case MODULE_CF989:
    case MODULE_SDEC_S2P16:
    case MODULE_SITRONIX_ST7032:
    case MODULE_CF990:
    case MODULE_HIPSHING_LC1641SRNH6:
    case MODULE_HIPSHING_LC1641SRNH6_CF997:
    case MODULE_CF979:
    case MODULE_4BIT_1LINE:
    case MODULE_8BIT_1LINE:
    case MODULE_HEM1601B:
      cycle (1, 0x1b, 39);
      break;
    case MODULE_AMAX_EDC162A40GXBBB:
      cycle (1, 0x00, 39);
      break;
  }

  // Pattern 3 - (use 0x02 to draw this one) 1 volume bar
  cycle (0, 0x40 | (2 << 3), 39); /* Set address */
  cycle (1, 0x18, 39);
  cycle (1, 0x18, 39);
  cycle (1, 0x18, 39);
  cycle (1, 0x18, 39);
  cycle (1, 0x18, 39);
  cycle (1, 0x18, 39);
  cycle (1, 0x18, 39);
  switch (lcd_module_id)
  {
    case MODULE_ATEAM_4BIT:
    case MODULE_ATEAM_8BIT:
    case MODULE_CF989:
    case MODULE_SDEC_S2P16:
    case MODULE_SITRONIX_ST7032:
    case MODULE_CF990:
    case MODULE_HIPSHING_LC1641SRNH6:
    case MODULE_HIPSHING_LC1641SRNH6_CF997:
    case MODULE_CF979:
    case MODULE_4BIT_1LINE:
    case MODULE_8BIT_1LINE:
    case MODULE_HEM1601B:
      cycle (1, 0x18, 39);
      break;
    case MODULE_AMAX_EDC162A40GXBBB:
      cycle (1, 0x00, 39);
      break;
  }
  
  // Pattern 4 - (use 0x03 to draw this one) '\'
  cycle (0, 0x40 | (3 << 3), 39); /* Set address */
  cycle (1, 0x00, 39);
  cycle (1, 0x10, 39);
  cycle (1, 0x08, 39);
  cycle (1, 0x04, 39);
  cycle (1, 0x02, 39);
  cycle (1, 0x01, 39);
  cycle (1, 0x00, 39);
  cycle (1, 0x00, 39);

  // Pattern 5 - (use 0x04 to draw this one) 'SOLID_CURSOR_LEFT'
  cycle (0, 0x40 | (4 << 3), 39); /* Set address */
  cycle (1, 0x01, 39);
  cycle (1, 0x03, 39);
  cycle (1, 0x07, 39);
  cycle (1, 0x0f, 39);
  cycle (1, 0x07, 39);
  cycle (1, 0x03, 39);
  cycle (1, 0x01, 39);
  cycle (1, 0x00, 39);

  // Pattern 6 - (use 0x05 to draw this one) 'SOLID_CURSOR_RIGHT'
  cycle (0, 0x40 | (5 << 3), 39); /* Set address */
  cycle (1, 0x10, 39);
  cycle (1, 0x18, 39);
  cycle (1, 0x1c, 39);
  cycle (1, 0x1e, 39);
  cycle (1, 0x1c, 39);
  cycle (1, 0x18, 39);
  cycle (1, 0x10, 39);
  cycle (1, 0x00, 39);
}

/****************************************************************************
 * Intialise the registers
 ****************************************************************************/
static void init_registers(void)
{
  get_shared_gpio();

  // Function Set
  // Interface width, no of lines, display font
  init_function_set_register(0);

  // Display off; cursor off, blinking off
  cycle (0, 0x8, 39);

  // Display clear
  cycle (0, 0x1, 39);

  // Display on; cursor on
  cycle (0, 0xf, 39);

  // Wait more than 1.53ms
  mdelay (4);
  
  // Entry mode set
  cycle (0, 0x6, 39);

  // Wait more than 1.53ms
  mdelay (4);

  /* Set up user defined characters */
  setup_user_defined_chars();

  release_shared_gpio();
}

/****************************************************************************
 * Initialises LCD hardware
 ****************************************************************************/
static void ateam_init_hardware()
{
  printk("RLA: %s\n", __FUNCTION__);

  /* Set up GPIO pins */
  setup_gpio ();
  
  // Wait 40ms for power on
  mdelay (200);

  /* Initialise LCD registers */
  init_registers();
}  

/****************************************************************************
 * Module cleanup
 ****************************************************************************/
static void __exit
ateam_lcd_exit(void)
{
  reciva_lcd_exit();
}

/****************************************************************************
 * Set up GPIO pins
 ****************************************************************************/
static void setup_gpio (void)
{
  printk("RLA: %s\n", __FUNCTION__);

  switch (lcd_module_id)
  {
    case MODULE_ATEAM_4BIT:
    case MODULE_ATEAM_8BIT:
    case MODULE_CF989:
    case MODULE_SITRONIX_ST7032:
    case MODULE_CF990:
    case MODULE_HIPSHING_LC1641SRNH6:
    case MODULE_CF979:
    case MODULE_AMAX_EDC162A40GXBBB:
      /* LED
         Processor pin = K12 (GPH7). Active high. */
      GPHCON |= (4 << 14);
      GPHCON &= ~(2 << 14); // Set pin as an output
      GPHUP &= ~(1 << 7); // disable pullup
      GPHDAT |= (1 << 7); // output on
      break;
    case MODULE_HIPSHING_LC1641SRNH6_CF997:
    case MODULE_SDEC_S2P16:
    case MODULE_4BIT_1LINE:
    case MODULE_8BIT_1LINE:
    case MODULE_HEM1601B:
      break;
  }
  ateam_set_led(1);     // on

  /* LDN0 (E1)
     This connects to 3 processor pins to allow various alternate functions
     to be used.
     The processor pins are : F4 (GPB2), J4 (GPC0), E15 (GPA12)
     Using F4 for now. */
  /* Set up unused pins as inputs */
  GPACON &= ~(3 << 24);
  GPBCON &= ~(3 << 4);
  GPCCON &= ~(3 << 0);
  GPAUP |= (1 << 12);  // disable pullup
  GPBUP |= (1 << 2);  // disable pullup
  GPCUP |= (1 << 0);  // disable pullup
  /* Set up GPC0 as standard GPIO (might change this later) */
  GPCCON |= (1 << 0);   // Set to output
  GPCDAT &= ~(1 << 0);  // Output low

  /* LDN1 (E2)
     This connects to 3 processor pins to allow various alternate functions
     to be used.
     The processor pins are : G3 (GPB3), J2 (GPC1), E16 (GPA13)
     Using G3 for now. */
  /* Set up unused pins as inputs */
  GPACON &= ~(3 << 26);
  GPBCON &= ~(3 << 6);
  GPCCON &= ~(3 << 2);
  GPAUP |= (1 << 13);  // disable pullup
  GPBUP |= (1 << 3);  // disable pullup
  GPCUP |= (1 << 1);    // disable pullup
  /* Set up GPC1 as standard GPIO (might change this later) */
  GPCCON |= (1 << 2);   // Set to output
  GPCDAT &= ~(1 << 1);  // Output low

  /* LDN2 (A0) - connected to J6 (GPC2) */
  GPCCON &= ~(3 << 4);
  GPCCON |= (1 << 4);
  GPCDAT &= ~(1 << 2);  // Output low
  GPCUP |= (1 << 2);    // disable pullup

  /* LDN3 (RES) - connected to K4 (GPC3) */
  GPCCON &= ~(3 << 6);
  GPCCON |= (1 << 6);
  switch (lcd_module_id)
  {
    case MODULE_ATEAM_4BIT:
    case MODULE_ATEAM_8BIT:
    case MODULE_CF989:
    case MODULE_SDEC_S2P16:
    case MODULE_SITRONIX_ST7032:
    case MODULE_CF990:
    case MODULE_HIPSHING_LC1641SRNH6:
    case MODULE_HIPSHING_LC1641SRNH6_CF997:
    case MODULE_CF979:
    case MODULE_4BIT_1LINE:
    case MODULE_8BIT_1LINE:
    case MODULE_HEM1601B:
      GPCDAT &= ~(1 << 3);  // Output low
      break;
    case MODULE_AMAX_EDC162A40GXBBB:
      GPCDAT |= (1 << 3);  // Output high
      break;
  }
  GPCUP |= (1 << 3);    // disable pullup

  /* LDN4 (LCDBRIGHT) - connected to F1 (GPB1) and K2 (GPC4) using F1 
   * Now done via reciva_backligh*.o */
  
  if (pins_shared)
  {
    tristate_data_gpio();
  }
  else
  {
    setup_data_gpio();
  }

  /* LDN5 (LDC_PON) - connected to K6 (GPC5) */
  GPCCON &= ~(3 << 10);
  GPCCON |= (1 << 10);
  GPCDAT &= ~(1 << 5);  // Power off
  GPCUP |= (1 << 5); // disable pullup
  /* Hold power low */
  int i;
  for (i=0; i<50; i++)
    udelay(1000);
  GPCDAT |= (1 << 5);  // Power on
}

static void setup_data_gpio(void)
{
  //printk("RLA: %s\n", __FUNCTION__);

  /* LCD Data - LCDD0 - LCDD7 (GPC8 to GPC15) */
  GPCCON &= ~((3 << 16) | (3 << 18) | (3 << 20) | (3 << 22) |
              (3 << 24) | (3 << 26) | (3 << 28) | (3 << 30) );
  GPCCON |=  ((1 << 16) | (1 << 18) | (1 << 20) | (1 << 22) |
              (1 << 24) | (1 << 26) | (1 << 28) | (1 << 30) );
  GPCDAT &= ~((1 << 8) | (1 << 9) | (1 << 10) | (1 << 11)|
              (1 << 12) | (1 << 13) | (1 << 14) | (1 << 15));  // Output low
  GPCUP |= ((1 << 8) | (1 << 9) | (1 << 10) | (1 << 11)|
            (1 << 12) | (1 << 13) | (1 << 14) | (1 << 15));// disable pullups
}

static void tristate_data_gpio(void)
{
  //printk("RLA: %s\n", __FUNCTION__);

  // Put GPC8-11 into tristate as they are shared with the
  // ROW inputs of the keypad (GPG8-11)
  // GPC12-15 are shared with the COL outputs, so don't need changing
  /* LCD Data - LCDD0 - LCDD7 (GPC8 to GPC15) */
  GPCCON &= ~( (3 << 16) | (3 << 18) | (3 << 20) | (3 << 22) );
  GPCUP |= ( (1 << 8) | (1 << 9) | (1 << 10) | (1 << 11) );// disable pullups
}

/****************************************************************************
 * Sends one 8 bit (or two 4 bit) instructions to the LCD module.
 * iA0 - level of A0 signal
 * iData - data to be written
 * iDelay - delay in microseconds at end of 8 bit cycle
 ****************************************************************************/
static void cycle (int iA0, int iData, int iDelay)
{
  //printk("RLA: %s\n", __FUNCTION__);
  unsigned long flags;
  spin_lock_irqsave(&cycle_lock, flags);

  if (interface_width == 4)
  {
    cycle_4bit (iA0, (iData>>4), 0);    // Top 4 bits
    cycle_4bit (iA0, iData, iDelay);    // Lower 4 bits
  }
  else
  {
    cycle_8bit (iA0, iData, iDelay);    
  }  

  spin_unlock_irqrestore(&cycle_lock, flags);
}

/****************************************************************************
 * LED control
 * level : 0 = off, non zero = on
 ****************************************************************************/
static void ateam_set_led(unsigned int mask)
{
  // XXX should really use reciva_set_led for all but trying not to break
  // other hwconfigs for now
  switch (lcd_module_id)
  {
    case MODULE_ATEAM_4BIT:
    case MODULE_ATEAM_8BIT:
    case MODULE_CF989:
    case MODULE_SITRONIX_ST7032:
    case MODULE_CF990:
    case MODULE_HIPSHING_LC1641SRNH6:
    case MODULE_CF979:
    case MODULE_AMAX_EDC162A40GXBBB:
      GPHUP |= (1 << 7);
      if (mask & RLED_VOLUME)
        GPHDAT |= (1 << 7);  // led on
      else
        GPHDAT &= ~(1 << 7);  // led off
      break;
    case MODULE_HIPSHING_LC1641SRNH6_CF997:
      reciva_led_set(mask);
      break;
    case MODULE_SDEC_S2P16:
    case MODULE_4BIT_1LINE:
    case MODULE_8BIT_1LINE:
    case MODULE_HEM1601B:
      break;
  }
}

/****************************************************************************
 * Turns power off
 ****************************************************************************/
static void ateam_power_off()
{
  GPCDAT &= ~(1 << 5);
}

/****************************************************************************
 * Set address pointer for LCM write
 * x - x coord
 * y - y coord
 ****************************************************************************/
static void set_address (int x, int y)
{
  int base_addr = x; // Address of leftmost column on specified row
  int addr;          // Address taking x coord into account
  
  switch (lcd_module_id)
  {
    case MODULE_ATEAM_4BIT:
    case MODULE_ATEAM_8BIT:
    case MODULE_CF989:
    case MODULE_SDEC_S2P16:
    case MODULE_SITRONIX_ST7032:
    case MODULE_CF990:
    case MODULE_AMAX_EDC162A40GXBBB:
    case MODULE_4BIT_1LINE:
    case MODULE_8BIT_1LINE:
    case MODULE_HEM1601B:
      if (y==0)
        base_addr = x;
      else
        base_addr = 0x40;
      break;
    case MODULE_HIPSHING_LC1641SRNH6:
    case MODULE_CF979:
    case MODULE_HIPSHING_LC1641SRNH6_CF997:
      switch (y)
      {
        case 0:
        default:
          base_addr = 0x00;
          break;
        case 1:
          base_addr = 0x40;
          break;
        case 2:
          base_addr = 0x10;
          break;
        case 3:  
          base_addr = 0x50;
          break;
      }
      break;
  }
  addr = base_addr + x;
          
  cycle (0, 0x80 | addr, 40);	/* Set address */
}

/****************************************************************************
 * Draw the given text on the screen, with optional cursor and arrows.
 *
 * acText         - Two dimensional array of strings
 * iX             - X coordinate of cursor
 * iY             - Y coordinate of cursor
 * iCursorType    - The cursor type
 * piArrows       - Arrow status for each row
 * piLineContents - Ignored (display text/barcode for each line)
 *
 * Returns 0 on success or negative on failure
 ****************************************************************************/
static int  ateam_draw_screen(char **ppcText,
                              int    iX,
                              int    iY,
                              int    iCursorType,
                              int   *piArrows,
                              int   *piLineContents)
{
  int x, iLine, iCharCount;
  char cLocalRep;
  const char *pcCurrentChar;
  int aiOffset[ateam_driver.get_height()];

  get_shared_gpio();

  /* Turn cursor off - this stops it flickering */
  cycle (0, 0x0c, 40);
    
  for (iLine = 0; iLine < ateam_driver.get_height(); iLine++)
  {
    pcCurrentChar = ppcText[iLine];
    iCharCount = rutl_count_utf8_chars(pcCurrentChar);

    aiOffset[iLine] = (ateam_driver.get_width() - iCharCount) / 2;
    if (aiOffset[iLine] < 0) 
      aiOffset[iLine] = 0;

    /* Write the whole line */
    set_address (0, iLine);
    for (x=0; x<screen_width_inc_arrows; x++)
    {
      if (x == 0)
      {
        if ((piArrows[iLine] == LCD_ARROW_LEFT) || 
            (piArrows[iLine] == LCD_ARROW_BOTH))
          cLocalRep = '<';
        else
          cLocalRep = ' ';
      }
      else if (x == screen_width_inc_arrows-1)
      {
        if ((piArrows[iLine] == LCD_ARROW_RIGHT) || 
            (piArrows[iLine] == LCD_ARROW_BOTH))
          cLocalRep = '>';
        else
          cLocalRep = ' ';
      }
      else
      {
        if (x > aiOffset[iLine] && *pcCurrentChar)
        {
          cLocalRep = reciva_lcd_utf8_lookup(pcCurrentChar);
          pcCurrentChar = rutl_find_next_utf8(pcCurrentChar);
        }
        else
        {
          cLocalRep = ' ';
        }
      }
      
      cycle (1, cLocalRep, 43);

      if (lcd_module_id == MODULE_HEM1601B && x == 7)
        cycle (0, 0x80 | 0x40, 40);	/* Set address */
    }  
  }

  /* Turn cursor on if appropriate (only handles line 0) */
  if (iCursorType == LCD_CURSOR_ON)
    draw_cursor (iX+1+aiOffset[iY], iY);

  release_shared_gpio();

  return 0;
}

/****************************************************************************
 * Clears the display
 ****************************************************************************/
static void ateam_clear_screen()
{
  get_shared_gpio();

  // Clear display
  cycle (0, 0x1, 40);

  release_shared_gpio();

  mdelay (2);
}

/****************************************************************************
 * Draws cursor at specified coords
 * x,y - cursor position
 ****************************************************************************/
static void draw_cursor (int x, int y)
{
  int iCount;
  int i;
  
  /* Set cursor to original position */ 
  cycle (0, 0x02, 0);  
  mdelay(2);

  /* Right shift cursor the appropriate amount */
  iCount = x + (y*40);
  for (i=0; i< iCount; i++)
    cycle (0, 0x14, 40);  

  /* Cursor on, no blink */
  cycle (0, 0x0e, 40);  
}  

/****************************************************************************
 * Set up the Function Set register
 * reinitialise - reinitialise
 ****************************************************************************/
static void init_function_set_register(int reinitialise)
{
  switch (lcd_module_id)
  {
    case MODULE_ATEAM_4BIT:
    case MODULE_4BIT_1LINE:
    case MODULE_SDEC_S2P16:
    case MODULE_ATEAM_8BIT:
    case MODULE_CF989:
    case MODULE_8BIT_1LINE:
    case MODULE_SITRONIX_ST7032:
    case MODULE_CF990:
    case MODULE_HIPSHING_LC1641SRNH6:
    case MODULE_HIPSHING_LC1641SRNH6_CF997:
    case MODULE_CF979:
    case MODULE_HEM1601B:
      break;
    case MODULE_AMAX_EDC162A40GXBBB:
      if (reinitialise == 0)
      {
        cycle_4bit (0, 0x03, 0);
        mdelay(20);
        cycle_4bit (0, 0x03, 0);
        mdelay(5);
        cycle_4bit (0, 0x03, 0);
        mdelay(1);
      }
      break;
  }

  if (interface_width == 4)
  {
    /* 4 bit i/f 
     * Don't need to set this up again when recovering from ESD shock */
    if (reinitialise == 0)
      cycle_4bit (0, 0x02, 0);
  }
  
  switch (lcd_module_id)
  {
    case MODULE_AMAX_EDC162A40GXBBB:
      cycle (0, 0x2c, 39);
      if (reinitialise)
        cycle (0, 0x02, 2000);   // Return home
      break;
    case MODULE_ATEAM_4BIT:
    case MODULE_CF989:
    case MODULE_4BIT_1LINE:
      /* 4bit i/f, 2 line, 5*11 font */
      cycle (0, 0x2c, 39);
      break;
    case MODULE_SDEC_S2P16:
      cycle (0, 0x30, 39);
      mdelay (8);
      cycle (0, 0x30, 200);
      cycle (0, 0x30, 200);
      cycle (0, 0x38, 39);
      if (test_mode)
      {
        for (;;)
        cycle (0, 0x38, 39);
      }
      break;
    case MODULE_ATEAM_8BIT:
    case MODULE_8BIT_1LINE:
      /* 8bit i/f, 2 line, 5*11 font */
      cycle (0, 0x3c, 39);
      break;
    case MODULE_SITRONIX_ST7032:
      /* 4bit i/f, 2 line, 5*11 font */
      cycle (0, 0x29, 39);
      break;
    case MODULE_CF990:
      cycle (0, 0x28, 39);
      break;
    case MODULE_HIPSHING_LC1641SRNH6:
    case MODULE_HIPSHING_LC1641SRNH6_CF997:
    case MODULE_CF979:
      /* 4bit i/f, 2 line, 5*11 font */
      cycle (0, 0x28, 39);
      break;
    case MODULE_HEM1601B:
      /* 8bit i/f, 2 lines, 5*11 font */
      cycle (0, 0x3c, 39);
      break;
  }
}

/****************************************************************************
 * Sends one 4 bit instruction to the LCD module.
 * iA0 - level of A0 signal
 * iData - data to be written - bits 3:0
 * iDelay - delay in microseconds at end of cycle
 ****************************************************************************/
static void cycle_4bit (int iA0, int iData, int iDelay)
{
  unsigned int temp = GPCDAT;
  
  /* Set up A0 (GPC2) */
  switch (lcd_module_id)
  {
    case MODULE_ATEAM_4BIT:
    case MODULE_ATEAM_8BIT:
    case MODULE_CF989:
    case MODULE_SITRONIX_ST7032:
    case MODULE_CF990:
    case MODULE_HIPSHING_LC1641SRNH6:
    case MODULE_HIPSHING_LC1641SRNH6_CF997:
    case MODULE_CF979:
    case MODULE_4BIT_1LINE:
    case MODULE_8BIT_1LINE:
    case MODULE_HEM1601B:
    case MODULE_SDEC_S2P16:
      /* GPC2 */
      if (iA0)
        temp |= (1 << 2);   /* A0 = '1' */
      else
        temp &= ~(1 << 2);  /* A0 = '0' */
      break;
    case MODULE_AMAX_EDC162A40GXBBB:
      /* GPC1 */
      if (iA0)
        temp |= (1 << 1);   /* A0 = '1' */
      else
        temp &= ~(1 << 1);  /* A0 = '0' */
      break;
  }
  GPCDAT = temp;
    
  udelay (20);

  /* Set E high (GPC0 */
  temp |= (1 << 0);  /* E1 = '1' */
  GPCDAT = temp;

  /* Set up data */
  temp &= ~(0x0000ff00);
  temp |= (iData  << 12);
  GPCDAT = temp;

  udelay (20);
  
  /* Set E low */
  temp &= ~(1 << 0);  /* E1 = '1' */
  GPCDAT = temp;

  udelay (20);
  udelay (iDelay);
}

/****************************************************************************
 * Sends one 8 bit instruction to the LCD module.
 * iA0 - level of A0 signal
 * iData - data to be written - bits 7:0
 * iDelay - delay in microseconds at end of cycle
 ****************************************************************************/
static void cycle_8bit (int iA0, int iData, int iDelay)
{
  unsigned int temp = GPCDAT;

  /* GPC8 - GPC15             : LCD module data D0 - D7
   * GPB2 (and GPC0, GPA12)    : (LDN0) LCD module E1
   * GPB3 (and GPC1, GPA13)    : (LDN1) LCD module E2  not used
   * GPC2                     : (LDN2) LCD module A0
   * GPC3                     : (LDN3)
   * GPB1 (and GPC4)          : (LDN4) LCD brightness
   * GPC5                     : (LDN5) LCD power */

  /* Set up A0 (GPC2) */
  if (iA0)
    temp |= (1 << 2);   /* A0 = '1' */
  else
    temp &= ~(1 << 2);  /* A0 = '0' */
  GPCDAT = temp;

  udelay (20);

  /* Set E high (GPC0 or GPC1) */
  temp |= (1 << 0);  /* E1 = '1' */
  GPCDAT = temp;

  /* Set up data */
  temp &= ~(0x0000ff00);
  temp |= (iData  << 8);
  GPCDAT = temp;

  udelay (20);

  /* Set E low */
  temp &= ~(1 << 0);  /* E1 = '1' */
  GPCDAT = temp;

  udelay (20);
  udelay (iDelay);
}




EXPORT_NO_SYMBOLS;

MODULE_AUTHOR("John Stirling <js@reciva.com>");
MODULE_DESCRIPTION("Reciva LCD driver");
MODULE_LICENSE("GPL");

module_init(ateam_lcd_init);
module_exit(ateam_lcd_exit);

