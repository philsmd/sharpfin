/*
 * Reciva LCD driver code for Everbouqet International MG1203D series
 * Copyright (c) 2004 Reciva Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option,
 */

/*************************************************************************
  Description :

  Driver for Everbouqet Internations MG1203D series LCD

  Display is split into 8 areas :

    aaaaaaaabbbbbbbb
    ccccccccdddddddd
    eeeeeeeeffffffff
    gggggggghhhhhhhh

  To write to left half of display, set row = 0
  To write to right half of LCD set row = 1

  To start writing from the far right of the section set column = 0

  Bottom 3 bits (block) of Page address Command select between the 4 rows.
  eg row = 1, block = 0, col =0 will write to block 'bbbbbbbb'

****************************************************************************/


    

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
#include <linux/version.h>

#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/mach-types.h>
#include <asm/arch/regs-gpio.h>

#include "lcd_generic.h"
#include "reciva_gpio.h"
#include "reciva_util.h"
#include "reciva_leds.h"
#include "reciva_backlight.h"
#include "fontdata.h"

#define INITIAL_HEIGHT        2 // Use mg1203d_driver.get_height()
#define INITIAL_WIDTH        14 // Use mg1203d_driver.get_width()

#define RIRM_LCDCN0_PIN  (1 << 0)
#define RIRM_LCDCN0_PIN_STINGRAY  (1 << 6)
#define RIRM_LCDCN1_PIN  (1 << 1)
#define RIRM_LCDCN2_PIN  (1 << 2)
#define RIRM_LCDCN3_PIN  (1 << 3)
#define RIRM_LCDCN4_PIN  (1 << 4)
#define RIRM_LCDCN5_PIN  (1 << 5)

#define LCD_E1     RIRM_LCDCN0_PIN
#define LCD_E2     RIRM_LCDCN1_PIN
#define LCD_A0     RIRM_LCDCN2_PIN
#define LCD_RESET  RIRM_LCDCN3_PIN
#define LCD_BRIGHT RIRM_LCDCN4_PIN
#define LCD_POWER  RIRM_LCDCN5_PIN

   /*************************************************************************/
   /***                        Static function prototypes                 ***/
   /*************************************************************************/

static int  __init mg1203d_lcd_init(void);
static void __exit mg1203d_lcd_exit(void);

/* Functions in lcd interface */
static void mg1203d_init_hardware(void);
static int  mg1203d_draw_screen(char **acText,
                                int   iX,
                                int   iY,
                                int   iCursorType,
                                int   iCursorWidth,
                                int   *piArrows,
                                int   *piLineContents);
static void mg1203d_clear_screen(void);
static void mg1203d_draw_grid(int reverse);
static void mg1203d_draw_vertical_lines(int reverse);
static void mg1203d_set_led(unsigned int mask);
static void mg1203d_power_off(void);

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

   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/
   
   /*************************************************************************/
   /***                        Static data                                ***/
   /*************************************************************************/

static const char acModuleName[] = "Reciva LCD MG1203D";
static const unsigned short au16FontData[] =
{
#include "fontdata.c"
};

/* Note that any elements not explicitly initialised here are guaranteed to 
 * be zero initialised */
static const struct reciva_lcd_driver mg1203d_driver = {
  name:                acModuleName,
  init_hardware:       mg1203d_init_hardware,
  get_height:          reciva_lcd_get_height,
  get_width:           reciva_lcd_get_width,
  draw_screen:         mg1203d_draw_screen,
  clear_screen:        mg1203d_clear_screen,
  draw_grid:           mg1203d_draw_grid,
  draw_vertical_lines: mg1203d_draw_vertical_lines,
  set_backlight:       reciva_bl_set_backlight,
  get_max_backlight:   reciva_bl_get_max_backlight,
  set_led:             mg1203d_set_led,
  power_off:           mg1203d_power_off,
  charmap:             asCharMap,
  leds_supported:      RLED_MENU,
};

static unsigned long lcd_e1 = LCD_E1;

   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/
            
/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init
mg1203d_lcd_init (void)
{
  if (machine_is_rirm3())
  {
    lcd_e1 = RIRM_LCDCN0_PIN_STINGRAY;
  }
  return reciva_lcd_init(&mg1203d_driver, INITIAL_HEIGHT, INITIAL_WIDTH);
}

/****************************************************************************
 * Initialise LCD hardware
 ****************************************************************************/
static void mg1203d_init_hardware()
{
  int i;
  int iPage;

  /* Set up GPIO pins */
  setup_gpio ();

  // Reset (E1)
  cycle (0, 1, 0xff);
  cycle (0, 1, 0xff);
  cycle (0, 1, 0xff);
  cycle (0, 0, 0xe2);
  // Reset (E2)
  cycle (1, 1, 0xff);
  cycle (1, 1, 0xff);
  cycle (1, 1, 0xff);
  cycle (1, 0, 0xe2);

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
      cycle (0, 1, 0);
      cycle (1, 1, 0);
    }
  }
}  

/****************************************************************************
 * Tidy up on exit
 ****************************************************************************/
static void __exit
mg1203d_lcd_exit(void)
{
  reciva_lcd_exit();
}

/****************************************************************************
 * Set up GPIO pins
 ****************************************************************************/
static void setup_gpio (void)
{
  /* Tell GPIO module which GPIO pins we are using */
  rgpio_register("GBC0-GPC5, GPC8-GPC15", acModuleName);

  /* LED
     Processor pin = K12 (GPH7). Active high. */
  rutl_regwrite((1 << 14), (3 << 14), (int)S3C2410_GPHCON); // Set to output
  rutl_regwrite((1 << 7), 0, (int)S3C2410_GPHUP); // disable pullup
  rutl_regwrite((1 << 7), 0, (int)S3C2410_GPHDAT); // ouput high
  mg1203d_set_led(RLED_VOLUME);

  // Unused LCD control lines GPB1-3, GPA12-13
  rutl_regwrite(0, (0x3f << 2), (int)S3C2410_GPBCON); // set to input
  rutl_regwrite((7 << 1), 0, (int)S3C2410_GPBUP);       // disable pullup

  /* LDN0 (E1) - connected to J4 (GPC0) or GPC6 on Stingray */
  /* LDN1 (E2) - connected to J2 (GPC1) */
  /* LDN2 (A0) - connected to J6 (GPC2) */
  /* LDN3 (RES) - connected to K4 (GPC3) */
  /* LDN4 (LCDBRIGHT) - connected to K2 (GPC4) */
  /* LDN5 (LDC_PON) - connected to K6 (GPC5) */
  rutl_regwrite(0x1555, 0x3fff, (int)S3C2410_GPCCON); // set to output
  rutl_regwrite(0, 0x7f, (int)S3C2410_GPCDAT);      // output low
  rutl_regwrite(0x7f, 0, (int)S3C2410_GPCUP);       // disable pullup

  /* LCD Data - LCDD0 - LCDD7 (GPC8 to GPC15) */
  rutl_regwrite((0x5555 << 16), (0xffff << 16), (int)S3C2410_GPCCON); // set to output
  rutl_regwrite(0, (0xff << 8), (int)S3C2410_GPCDAT);      // output low
  rutl_regwrite((0xff << 8), 0, (int)S3C2410_GPCUP);       // disable pullup

  /* Hold power low */
  int i;
  for (i=0; i<50; i++)
    udelay(1000);
  rutl_regwrite(LCD_POWER, 0, (int)S3C2410_GPCDAT);  // Power on
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
  /* GPC8 - GPC15             : LCD module data D0 - D7
   * GPB2 (and GPC0, GPA12)    : (LDN0) LCD module E1 
   * GPB3 (and GPC1, GPA13)    : (LDN1) LCD module E2 
   * GPC2                     : (LDN2) LCD module A0
   * GPC3                     : (LDN3)
   * GPB1 (and GPC4)          : (LDN4) LCD brightness
   * GPC5                     : (LDN5) LCD power */

  /* Set up A0 (GPC2) */
  if (iA0)
    rutl_regwrite(LCD_A0, 0, (int)S3C2410_GPCDAT); /* A0 = '1' */
  else
    rutl_regwrite(0, LCD_A0, (int)S3C2410_GPCDAT); /* A0 = '0' */
    
  /* Set E high (GPC0 or GPC1) */
  if (device)
  {
    rutl_regwrite(LCD_E2, 0, (int)S3C2410_GPCDAT); /* E2 = '1' */
  }
  else
  {
    rutl_regwrite(lcd_e1, 0, (int)S3C2410_GPCDAT); /* E1 = '1' */
  }

  /* Set up data */
  unsigned int temp = 0;
  if (iA0)
  {
    /* Reverse bits */
    temp |= ((iData & 0x00000001) << 15) |
            ((iData & 0x00000002) << 13) |
            ((iData & 0x00000004) << 11) |
            ((iData & 0x00000008) << 9)  |
            ((iData & 0x00000010) << 7)  |
            ((iData & 0x00000020) << 5)  |
            ((iData & 0x00000040) << 3)  |
            ((iData & 0x00000080) << 1);
  }
  else
  {
    temp |= (iData  << 8);
  }  
  rutl_regwrite(temp, (0xff << 8), (int)S3C2410_GPCDAT);
  
  udelay(1);

  /* Set E low */
  if (device)
  {
    rutl_regwrite(0, LCD_E2, (int)S3C2410_GPCDAT); /* E2 = '0' */
  }
  else
  {
    rutl_regwrite(0, lcd_e1, (int)S3C2410_GPCDAT); /* E1 = '0' */
  }

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
 * piArrows       - Arrow status for each row (optional)
 * piLineContents - Ignored (display text/barcode for each line)
 *
 *
 * Returns 0 on success or negative on error
 ****************************************************************************/
static int  mg1203d_draw_screen(char **ppcText, int iX, int iY, 
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

  for (iRow = 0; iRow < mg1203d_driver.get_height(); iRow++)
  {
    if (!ppcText[iRow])
      continue;

    pcPrev = " ";
    pcCur  = " ";
    pcCurrentPos = ppcText[iRow];
    iPrevRev = 0;
    iCurRev  = 0;
    if (piArrows)
      draw_arrows(piArrows[iRow], iRow);

    x_offset = get_offset(rutl_count_utf8_chars(pcCurrentPos));

    for (x = x_offset & 1; x < 2 * mg1203d_driver.get_width(); x += 2)
    {
      if (x < x_offset || !*pcCurrentPos)
      {
        pcCur    = " ";
        iCurRev  = 0;
      }
      else
      {
        int iXOffsetChars = (x-x_offset)/2;
        pcCur    = pcCurrentPos;
        iCurRev  = ((iXOffsetChars >= iX && iXOffsetChars < (iX + iCursorWidth)) && iRow == iY && iCursorType == LCD_CURSOR_ON);
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
static void mg1203d_clear_screen()
{
  int x, y;
  for (x = 0; x < mg1203d_driver.get_width(); x++)
  {
    for (y = 0; y < mg1203d_driver.get_height(); y++)
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

  if (x < 0 || x >= mg1203d_driver.get_width())
    return;

  /* For some reason the LCD is mounted upside down.
   * We also draw each character upside down using l below. */
  x = (mg1203d_driver.get_width() - 1) - x;

  /* The LCD panel is logically arranged as four rows of seven characters,
   * where each physical row consists of two logical rows side-by side.
   * Figure out which logical row we are in, and adjust x accordingly */
  row = (x / 7) ? 0 : 1;
  x = x % 7;

  /* There are two blocks in a row */
  block = y * 2;

  col = x * 8 + ((row == 1) ? 5 : 0);

  cycle (row, 0, 0xb8 | block | 1);
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

  cycle (row, 0, 0xb8 | block);
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
 ** NAME:              mg1203d_draw_grid
 **
 ** PARAMETERS:        reverse - if set, display is inverted
 **
 ** RETURN VALUES:     None
 **
 ** DESCRIPTION:       Draws a grid on the LCD (test pattern)
 **
 ****************************************************************************/
static void mg1203d_draw_grid(int reverse)
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
 ** NAME:              mg1203d_draw_vertical_lines
 **
 ** PARAMETERS:        reverse - if set, display is inverted
 **
 ** RETURN VALUES:     None
 **
 ** DESCRIPTION:       Draws vertical lines of varying width on the LCD
 **                    (test pattern)
 **
 ****************************************************************************/
static void mg1203d_draw_vertical_lines(int reverse)
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

  block = iLine * 2;
      
  cycle (row, 0, 0xb8 | block | 1);
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

  cycle (row, 0, 0xb8 | block);
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

  block = iLine * 2;
      
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

  if (iLength <= mg1203d_driver.get_width() - (iLength & 1))
  {
    /* Text will all fit on the display so centre the text and
     * don't use scrolling */
    x_offset = mg1203d_driver.get_width() - iLength;
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
static void mg1203d_set_led(unsigned int mask)
{
  if (mask & RLED_MENU)
    rutl_regwrite((1 << 7), 0, (int)S3C2410_GPHDAT);  // led on
  else
    rutl_regwrite(0, (1 << 7), (int)S3C2410_GPHDAT);  // led off
}

/****************************************************************************
 * Power off the LCD
 ****************************************************************************/
static void mg1203d_power_off()
{
  rutl_regwrite(0, LCD_POWER, (int)S3C2410_GPCDAT);  // Power off
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
EXPORT_NO_SYMBOLS;
#endif

MODULE_AUTHOR("John Stirling <js@reciva.com>");
MODULE_DESCRIPTION("Reciva LCD driver");
MODULE_LICENSE("GPL");

module_init(mg1203d_lcd_init);
module_exit(mg1203d_lcd_exit);

