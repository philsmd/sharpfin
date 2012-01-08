/*
 * Reciva LCD driver code for Tianma LCD
 * Copyright (c) 2004, 2005, 2006 Reciva Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option,
 */

   
   /*************************************************************************/
   /***                        Include Files                              ***/
   /*************************************************************************/

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/smp_lock.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/string.h>

#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/io.h>

#include "lcd_generic.h"
#include "reciva_gpio.h"
#include "reciva_util.h"
#include "reciva_leds.h"
#include "reciva_backlight.h"
#include "fontdata.h"

#define INITIAL_HEIGHT        2 // Use tianma_driver.get_height()
#define INITIAL_WIDTH        14 // Use tianma_driver.get_width()

   /*************************************************************************/
   /***                        Static function prototypes                 ***/
   /*************************************************************************/

static int  __init tianma_lcd_init(void);
static void __exit tianma_lcd_exit(void);

/* Functions in lcd interface */
static void tianma_init_hardware(void);
static int  tianma_draw_screen(char **acText,
                                int   iX,
                                int   iY,
                                int   iCursorType,
                                int   iCursorWidth,
                                int   *piArrows,
                                int   *piLineContents);
static void tianma_clear_screen(void);
static void tianma_draw_grid(int reverse);
static void tianma_draw_vertical_lines(int reverse);
static void tianma_set_led(unsigned int mask);
static void tianma_power_off(void);

/* Local helper functions */
static void cycle (int device, int a, int d);
static void setup_gpio (void);
static void draw_symbol(int x, int y, const unsigned short *f, int iReverse);
static void lcd_draw_arrow(int iSide, int iLine);
static void lcd_clear_arrow(int iSide, int iLine);
static int  get_offset(int iLength);
static void write_char(int x, int y, const char *pcData, int iReverse);
static void write_hybrid_char(int x, int y,
                              const char *pcData1, const char *pcData2,
                              int iReverse1, int iReverse2);

static void draw_arrows(int iArrowStatus, int line);

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



   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/
   
   /*************************************************************************/
   /***                        Static data                                ***/
   /*************************************************************************/

static const char acModuleName[] = "Reciva LCD TIANMA";
static const unsigned short au16FontData[] =
{
#include "fontdata.c"
};

/* Note that any elements not explicitly initialised here are guaranteed to 
 * be zero initialised */
static const struct reciva_lcd_driver tianma_driver = {
  name:                acModuleName,
  init_hardware:       tianma_init_hardware,
  get_height:          reciva_lcd_get_height,
  get_width:           reciva_lcd_get_width,
  draw_screen:         tianma_draw_screen,
  clear_screen:        tianma_clear_screen,
  draw_grid:           tianma_draw_grid,
  draw_vertical_lines: tianma_draw_vertical_lines,
  set_backlight:       reciva_bl_set_backlight,
  get_max_backlight:   reciva_bl_get_max_backlight,
  set_led:             tianma_set_led,
  power_off:           tianma_power_off,
  charmap:             asCharMap,
  leds_supported:      RLED_MENU,
};

   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/
            
/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init
tianma_lcd_init (void)
{
  return reciva_lcd_init(&tianma_driver, INITIAL_HEIGHT, INITIAL_WIDTH);
}

/****************************************************************************
 * Initialise LCD hardware
 ****************************************************************************/
static void tianma_init_hardware()
{
  int i;
  int iPage;

  /* Set up GPIO pins */
  setup_gpio ();

  /* 1/32 duty */
  cycle (0, 0, 0xa9); // E1
  cycle (1, 0, 0xa9); // E2

  /* Display on */
  cycle (0, 0, 0xaf);
  cycle (1, 0, 0xaf);

  /* Zero start */
  cycle (0, 0, 0xc0);
  cycle (1, 0, 0xc0);

  for (iPage = 0; iPage < 4; iPage++)
  {
    cycle (1, 0, 0xb8 | iPage);
    cycle (0, 0, 0xb8 | iPage);

    cycle (1, 0, 0x00);
    cycle (0, 0, 0x00);

    for (i = 0; i < 0x50; i++)
    {
      cycle (0, 1, 0xff);
      cycle (1, 1, 0);
    }
  }
}  

/****************************************************************************
 * Tidy up on exit
 ****************************************************************************/
static void __exit
tianma_lcd_exit(void)
{
  reciva_lcd_exit();
}

/****************************************************************************
 * Set up GPIO pins
 ****************************************************************************/
static void setup_gpio (void)
{
  /* Tell GPIO module which GPIO pins we are using */
  rgpio_register("GPB2 GBC0 GPA12 (E1)", acModuleName);
  rgpio_register("GPB3 GBC1 GPA13 (E2)", acModuleName);
  rgpio_register("GPB1 GBC4       (LCD_BRIGHT)", acModuleName);
  rgpio_register("GBC0-GPC5, GPC8-GPC15", acModuleName);

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
  GPCDAT |= ~(1 << 1);  // Output low

  /* LDN2 (A0) - connected to J6 (GPC2) */
  GPCCON &= ~(3 << 4);
  GPCCON |= (1 << 4);
  GPCDAT &= ~(1 << 2);  // Output low
  GPCUP |= (1 << 2);    // disable pullup

  /* LDN3 (RES) - connected to K4 (GPC3) */
  GPCCON &= ~(3 << 6);
  GPCCON |= (1 << 6);
  GPCDAT &= ~(1 << 3);  // Output low
  GPCUP |= (1 << 3);    // disable pullup

  /* LCD Data - LCDD0 - LCDD7 (GPC8 to GPC15) */
  GPCCON &= ~((3 << 16) | (3 << 18) | (3 << 20) | (3 << 22) |
              (3 << 24) | (3 << 26) | (3 << 28) | (3 << 30) );
  GPCCON |=  ((1 << 16) | (1 << 18) | (1 << 20) | (1 << 22) |
              (1 << 24) | (1 << 26) | (1 << 28) | (1 << 30) );
  GPCDAT &= ~((1 << 8) | (1 << 9) | (1 << 10) | (1 << 11)|
              (1 << 12) | (1 << 13) | (1 << 14) | (1 << 15));  // Output low
  GPCUP |= ((1 << 8) | (1 << 9) | (1 << 10) | (1 << 11)|
            (1 << 12) | (1 << 13) | (1 << 14) | (1 << 15));// disable pullups

  /* LDN5 (LDC_PON) - connected to K6 (GPC5) */
  GPCCON &= ~(3 << 10);
  GPCCON |= (1 << 10);
  GPCDAT &= ~(1 << 5);  // Power off
  GPCUP |= (1 << 5); // disable pullup
  /* Hold power low */
  int i;
  for (i=0; i<50; i++)
    udelay(1000);
  GPCDAT &= ~(1 << 3);		// reset low
  GPCDAT |= (1 << 5);  // Power on

  for (i=0; i<50; i++)
    udelay(1000);

  GPCDAT |= (1 << 3);		// reset high
  udelay (1);
}

/****************************************************************************
 **
 ** NAME:              cycle
 **
 ** PARAMETERS:        device - specifies the device (E1 or E2 selects device)
 **                    iA0 - level of A0 signal
 **                    iData - data to be written
 **
 ** RETURN VALUES:     None
 **
 ** DESCRIPTION:       Sends an instruction to the LCD module.
 **                    Note : the fastest you can change the state of a GPIO
 **                    pin is 125ns. Hence the reason there are no additional
 **                    delays needed in the command cycle.
 **
 ****************************************************************************/

static void cycle (int device, int iA0, int iData)
{
  unsigned int temp = GPCDAT;

  /* GPC8 - GPC15             : LCD module data D0 - D7
   * GPB2 (and GPC0, GPA12)    : (LDN0) LCD module E1 
   * GPB3 (and GPC1, GPA13)    : (LDN1) LCD module E2 
   * GPC2                     : (LDN2) LCD module A0
   * GPC3                     : (LDN3) LCD module RES
   * GPB1 (and GPC4)          : (LDN4) LCD brightness
   * GPC5                     : (LDN5) LCD power */

  /* Set up A0 (GPC2) */
  if (iA0)
    temp |= (1 << 2);   /* A0 = '1' */
  else
    temp &= ~(1 << 2);  /* A0 = '0' */
  GPCDAT = temp;
    
  udelay(1);
  /* Set E high (GPC0 or GPC1) */
  if (device)
  {
    temp |= (1 << 0);  /* E2 = '1' */
  }
  else
  {
    temp |= (1 << 1);  /* E1 = '1' */
  }
  GPCDAT = temp;

  udelay(1);

  /* Set up data */
  temp &= ~(0x0000ff00);
  temp |= (iData  << 8);
  GPCDAT = temp;

  udelay (1);

  /* Set E1, E2 high */
  temp &= ~(3 << 0);

  GPCDAT = temp;

  udelay(1);
}

#if 0
static void print_one_char(const char *s)
{
  const char *next_char = rutl_find_next_utf8(s);
  char *copied_char;
  int length, i;

  length = next_char - s;
  copied_char = kmalloc(length + 1, GFP_KERNEL);
  strncpy(copied_char, s, length);
  copied_char[length] = '\0';

  printk(KERN_ERR);
  for (i = 0; i < length; ++i)
  {
    printk("0x%02x ", copied_char[i]);
  }
  printk(": \"%s\"\n", copied_char);

  kfree(copied_char);
}
#endif

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
 * Returns 0 on success or negative on error
 ****************************************************************************/
static int  tianma_draw_screen(char **ppcText, int iX, int iY, 
                               int iCursorType, int iCursorWidth, int *piArrows,
                               int *piLineContents)
{
  /* In this function x is measured in units of half a character.  Sorry :) 
   * And for avoidance of doubt it's the position on the display that's
   * currently being drawn, NOT an index into the character array being drawn,
   * which may contain multibyte characters. */
  
  int x, x_offset;

  /* pcCurrentPos is the position we're actually at in the string.  pcCur is
   * the character we're in the process of drawing right now which may be " "
   * or something instead of a pointer into the real string. */
  const char *pcPrev, *pcCur, *pcCurrentPos;
  int iPrevRev, iCurRev;
  int iRow;

  for (iRow = 0; iRow < tianma_driver.get_height(); iRow++)
  {
    if (!ppcText[iRow])
      continue;

    pcPrev = " ";
    pcCur  = " ";
    pcCurrentPos = ppcText[iRow];
    iPrevRev = 0;
    iCurRev  = 0;
    draw_arrows(piArrows[iRow], iRow);

    x_offset = get_offset(rutl_count_utf8_chars(pcCurrentPos));

    for (x = x_offset & 1; x < 2 * tianma_driver.get_width(); x += 2)
    {
      if (x < x_offset || !*pcCurrentPos)
      {
        pcCur    = " ";
        iCurRev  = 0;
      }
      else
      {
        pcCur    = pcCurrentPos;
        iCurRev  = ((x-x_offset)/2 == iX && iRow == iY && iCursorType == LCD_CURSOR_ON);
        pcCurrentPos = rutl_find_next_utf8(pcCurrentPos);
      }

      if (x_offset & 1)
        write_hybrid_char((x-1)/2, iRow, pcPrev, pcCur, iPrevRev, iCurRev);
      else
        write_char(x/2, iRow, pcCur, iCurRev);

      pcPrev   = pcCur;
      iPrevRev = iCurRev;
    }
  }

  return 0;
}

/****************************************************************************
 * Clear the display
 ****************************************************************************/
static void tianma_clear_screen()
{
  int x, y;
  for (x = 0; x < tianma_driver.get_width(); x++)
  {
    for (y = 0; y < tianma_driver.get_height(); y++)
    {
      write_char (x, y, " ", 0);
    }
  }
}

/****************************************************************************
 **
 ** NAME:              draw_arrows
 **
 ** PARAMETERS:        iArrowStatus - which arrows should be shown
 **                    line - the line number to draw on
 **
 ** RETURN VALUES:     None
 **
 ** DESCRIPTION:       Handles drawing or clearing the arrows
 **
 ****************************************************************************/
static void draw_arrows(int iArrowStatus, int line)
{
  switch (iArrowStatus)
  {
    case LCD_ARROW_NONE:
      lcd_clear_arrow(0, line);
      lcd_clear_arrow(1, line);
      break;
    case LCD_ARROW_LEFT:
      lcd_draw_arrow (0, line);
      lcd_clear_arrow(1, line);
      break;
    case LCD_ARROW_RIGHT:
      lcd_clear_arrow(0, line);
      lcd_draw_arrow (1, line);
      break;
    case LCD_ARROW_BOTH:
      lcd_draw_arrow(0, line);
      lcd_draw_arrow(1, line);
      break;
    default:
      printk(KERN_ERR "Unknown arrow status: %d !  ", iArrowStatus);
      break;
  }
}


/****************************************************************************
 * Draw a single width character symbol on the display.
 *
 * x        - x coord
 * y        - y coord
 * f        - pointer to data
 * iReverse - if set, character is drawn white on black
 ****************************************************************************/
static void draw_symbol(int x, int y, const unsigned short *f, int iReverse)
{
  int i;
  int row, col;
  int block;

  if (x < 0 || x >= tianma_driver.get_width())
    return;

  y = (tianma_driver.get_height() - 1) - y;
  x = (tianma_driver.get_width() - 1) - x;

  /* The LCD panel is logically arranged as four rows of seven characters,
   * where each physical row consists of two logical rows side-by side.
   * Figure out which logical row we are in, and adjust x accordingly */
  row = (x / 7) ? 0 : 1;
  x = x % 7;

  /* There are two blocks in a row */
  block = y * 2;

  col = x * 8 + ((row == 1) ? 5 : 0);

  cycle (row, 0, 0xb8 | block);
  cycle (row, 0, col);

  for (i = 0; i < 8; i++)
  {
    int l;
    l = 7 - i;
    if (iReverse)
    {
      cycle (row, 1, ~f[l] & 0xff);
    }
    else
    {
      cycle (row, 1,  f[l] & 0xff);
    }
  }

  cycle (row, 0, 0xb8 | block | 1);
  cycle (row, 0, col);

  for (i = 0; i < 8; i++)
  {
    int l;
    l = 7 - i;
    if (iReverse)
    {
      cycle (row, 1, ~f[l] >> 8);
    }
    else
    {
      cycle (row, 1,  f[l] >> 8);
    }
  }
}

/****************************************************************************
 * Draw the given (possibly multibyte) character on the display.
 * NB this currently only handles characters that are one visual unit wide.
 *
 * x        - x coord
 * y        - y coord
 * pcData   - character to be displayed
 * iReverse - if set, character is drawn white on black
 ****************************************************************************/
static void write_char(int x, int y, const char *pcData, int iReverse)
{
  char cCharToDraw;
  
  cCharToDraw = reciva_lcd_utf8_lookup(pcData);

  draw_symbol(x, y, &au16FontData[cCharToDraw * 8], iReverse);
}

/****************************************************************************
 * This is a fun one.  It draws a portion of the display that is normally
 * occupied by a single character.  In the left half of that portion it puts
 * the right-hand half of cData1, and in the right half it puts the left half
 * of cData2.  It also deals with reversing one or both of those characters.
 ****************************************************************************/
static void write_hybrid_char(int  x, int  y,
                              const char *pcData1, const char *pcData2,
                              int iReverse1, int iReverse2)
{
  unsigned short asData[8];
  int i;
  unsigned short j;
  char cLocalRep, cLocalRep1, cLocalRep2;
  int iReverse;
  
  cLocalRep1 = reciva_lcd_utf8_lookup(pcData1);
  cLocalRep2 = reciva_lcd_utf8_lookup(pcData2);

  for (i = 0; i < 8; ++i)
  {
    if (i < 4)
    {
      cLocalRep = cLocalRep1;
      iReverse  = iReverse1;
    }
    else
    {
      cLocalRep = cLocalRep2;
      iReverse  = iReverse2;
    }

    asData[i] = 0;

    for (j = 1 ; j != 0; j <<= 1)
    {
      if (iReverse ^ ((au16FontData[cLocalRep * 8 + (i<4 ? i+4 : i-4)])/j & 1))
        asData[i] |=  j;
    }
  }

  draw_symbol(x, y, asData, 0);
}


/****************************************************************************
 **
 ** NAME:              tianma_draw_grid
 **
 ** PARAMETERS:        reverse - if set, display is inverted
 **
 ** RETURN VALUES:     None
 **
 ** DESCRIPTION:       Draws a grid on the LCD (test pattern)
 **
 ****************************************************************************/
static void tianma_draw_grid(int reverse)
{
  int row, col;
  int block;
  int i;
  
  /* Right half of display */
  row = 1;
  col = 0;
  for (block = 0; block<4; block++)
  {
    /* Page Address Set */
    cycle (row, 0, 0xb8 | block);
    /* Column Address Set */
    cycle (row, 0, col);
    /* Write some data */
    for (i=0; i<31; i++)
    {
      if (reverse)
      {
        cycle (row, 1,  0xaa);
        cycle (row, 1,  0x55);
      }
      else
      {
        cycle (row, 1,  0x55);
        cycle (row, 1,  0xaa);
      }
    }
  }

  /* Left half of display */
  row = 0;
  for (block = 0; block<4; block++)
  {
    /* Page Address Set */
    cycle (row, 0, 0xb8 | block);
    /* Column Address Set */
    cycle (row, 0, col);
    /* Write some data */
    for (i=0; i<31; i++)
    {
      if (reverse)
      {
        cycle (row, 1,  0x55);
        cycle (row, 1,  0xaa);
      }
      else
      {
        cycle (row, 1,  0xaa);
        cycle (row, 1,  0x55);
      }
    }
  }
}  

/****************************************************************************
 **
 ** NAME:              tianma_draw_vertical_lines
 **
 ** PARAMETERS:        reverse - if set, display is inverted
 **
 ** RETURN VALUES:     None
 **
 ** DESCRIPTION:       Draws vertical lines of varying width on the LCD
 **                    (test pattern)
 **
 ****************************************************************************/
static void tianma_draw_vertical_lines(int reverse)
{
  int row, col;
  int block;
  int i;
  int width;
  unsigned char line;
  unsigned char no_line;

  /* Handle inverse display */
  if (reverse)
  {
    line = 0x00;
    no_line = 0xff;
  }
  else
  {
    line = 0xff;
    no_line = 0x00;
  }    
  
  /* Draw identical pattern on left and right of display
   * row = 0 (left), row = 1 (right) */
  for (row=0; row<2; row++)
  {
    col = 0;
    for (block = 0; block<4; block++)
    {
      /* Page Address Set */
      cycle (row, 0, 0xb8 | block);
      /* Column Address Set */
      cycle (row, 0, col);
      /* Draw the lines */

      /* First line 17 pixels */
      for (i=0; i<17; i++)
      {
        cycle (row, 1, line);
      }
      cycle (row, 1, no_line);
        
      /* 8 pixel to 1 pixel line widths separated by 1 pixel */
      for (width=8; width>0; width--)
      {
        for (i=0; i<width; i++)
          cycle (row, 1, line);
        cycle (row, 1, no_line);
      }
    }
  }
}  

/****************************************************************************
 **
 ** NAME:              lcd_draw_arrow
 **
 ** PARAMETERS:        iSide - 0 for left, 1 for right
 **                    iLine - line number on the display
 **
 ** RETURN VALUES:     None
 **
 ** DESCRIPTION:       Draws an arrow at the extreme edge of the display
 **                    on the given line
 **
 ****************************************************************************/
static void lcd_draw_arrow(int iSide, int iLine)
{
  int row, col;
  int block;

  switch (iSide)
  {
    case 0:
      row = 0;
      col = 53;
      break;

    case 1:
      row = 7;
      col = 0;
      break;

    default:
      return;
  }

  block = (1 - iLine) * 2;
      
  cycle (row, 0, 0xb8 | block);
  cycle (row, 0, col);

  /* Draw the bottom half of the character */
  if (iSide)
  {
    cycle (row, 1,  0x00);
    cycle (row, 1,  0x00);
    cycle (row, 1,  0x80);
    cycle (row, 1,  0xc0);
    cycle (row, 1,  0x00);
    cycle (row, 1,  0x00);
    cycle (row, 1,  0x00);
    cycle (row, 1,  0x00);
  }
  else
  {
    cycle (row, 1,  0x00);
    cycle (row, 1,  0x00);
    cycle (row, 1,  0x00);
    cycle (row, 1,  0x00);
    cycle (row, 1,  0x00);
    cycle (row, 1,  0xc0);
    cycle (row, 1,  0x80);
    cycle (row, 1,  0x00);
  }

  cycle (row, 0, 0xb8 | block | 1);
  cycle (row, 0, col);

  /* Draw the top half of the character */
  if (iSide)
  {
    cycle (row, 1,  0x00);
    cycle (row, 1,  0x01);
    cycle (row, 1,  0x03);
    cycle (row, 1,  0x07);
    cycle (row, 1,  0x00);
    cycle (row, 1,  0x00);
    cycle (row, 1,  0x00);
    cycle (row, 1,  0x00);
  }
  else
  {
    cycle (row, 1,  0x00);
    cycle (row, 1,  0x00);
    cycle (row, 1,  0x00);
    cycle (row, 1,  0x00);
    cycle (row, 1,  0x00);
    cycle (row, 1,  0x07);
    cycle (row, 1,  0x03);
    cycle (row, 1,  0x01);
  }
}

/****************************************************************************
 **
 ** NAME:              lcd_clear_arrow
 **
 ** PARAMETERS:        iSide - 0 for left, 1 for right
 **                    iLine - line number on the display
 **
 ** RETURN VALUES:     None
 **
 ** DESCRIPTION:       Clears the space where an arrow may previously have
 **                    been drawn.
 **
 ****************************************************************************/
static void lcd_clear_arrow(int iSide, int iLine)
{
  int row, col;
  int block;

  int i;

  switch (iSide)
  {
    case 0:
      row = 0;
      col = 53;
      break;

    case 1:
      row = 7;
      col = 0;
      break;

    default:
      return;
  }

  block = (1 - iLine) * 2;
      
  for (i = 0; i < 16; ++i)
  {
    if (i % 8 == 0)
    {
      cycle (row, 0, 0xb8 | block | i/8);
      cycle (row, 0, col);
    }

    cycle (row, 1,  0x00);
  }
}

/****************************************************************************
 **
 ** NAME:              get_offset
 **
 ** PARAMETERS:        iLength - the visual length of the text to be displayed
 **
 ** RETURN VALUES:     The offset (in half characters) that should be used
 **                    for displaying the text
 **
 ** DESCRIPTION:       Computes the offset that should be used in
 **                    displaying text.
 **
 ****************************************************************************/
static int get_offset(int iLength)
{
  int x_offset; /* Measured in units of half-character again */

  if (iLength <= tianma_driver.get_width() - (iLength & 1))
  {
    /* Text will all fit on the display so centre the text and
     * don't use scrolling */
    x_offset = tianma_driver.get_width() - iLength;
  }
  else
  {
    /* Text might wrap (but hopefully won't as this should be dealt with by
     * vLCD_DrawText) so left-justify it */
    x_offset = 0;
  }

  return x_offset;
}

/****************************************************************************
 * Set LED status
 ****************************************************************************/
static void tianma_set_led(unsigned int mask)
{
  reciva_led_set(mask);
}

/****************************************************************************
 * Power off the LCD
 ****************************************************************************/
static void tianma_power_off()
{
  GPCDAT &= ~(1 << 5);  // Power off
  GPCUP |= (1 << 5);    // disable pullup
}

EXPORT_NO_SYMBOLS;

MODULE_AUTHOR("John Stirling <js@reciva.com>");
MODULE_DESCRIPTION("Reciva LCD driver");
MODULE_LICENSE("GPL");

module_init(tianma_lcd_init);
module_exit(tianma_lcd_exit);

