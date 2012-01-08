/*
 * Reciva LCD driver code for Futaba 162SD013INK VFD
 *
 * Copyright (c) 2008 Reciva Ltd
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
#include <linux/slab.h>
#include <linux/version.h>

#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/mach-types.h>

#include "reciva_lcd.h"
#include "reciva_leds.h"
#include "reciva_lcd_ateam.h"
#include "lcd_generic.h"
#include "reciva_util.h"
#include "reciva_backlight.h"
#include "reciva_keypad_generic.h"

   /*************************************************************************/
   /***                        Local defines                              ***/
   /*************************************************************************/

#define MAX_EXTRA_CHAR_TABLES			4

#define PREFIX "RVF: "

#define WRITE_DCRAM             0x00
#define WRITE_CGRAM             0x40
#define TIMING_SET              0xe0
#define DIMMING_SET             0xe4
#define DISPLAY_ON              0xe8

#define DCRAM_A                 0x00
#define DCRAM_B                 0x20

   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/
   
struct soft_char
{
  unsigned char *utf8_char;
  unsigned char data[8];
};

   /*************************************************************************/
   /***                        Static data                                ***/
   /*************************************************************************/

static const char acModuleName[] = "Reciva Futaba VFD";

#include "reciva_lcd_ateam_soft_font.c"

#define NR_CGRAM_SLOTS	8

static unsigned char soft_char_on_screen[NR_CGRAM_SLOTS];
static unsigned short soft_char_loaded[NR_CGRAM_SLOTS];
static int soft_char_eviction_pointer;

static const struct rutl_unicode_charmap asBaseCharmap[] = {
#include "lcd_simple_charmap.c"
};

#define NR_BASE_CHARS (sizeof (asBaseCharmap) / sizeof (asBaseCharmap[0]))

/* Charmaps 
 * These can be optionally set up via a module parameter
 * String should be a space seperated list of numbers
 * eg 
 * "0 2" would select asModule11Charmap, asST7066U_A_Charmap
 * Note - use "-1" to select no special charmaps */
static char *charmap_string;
static const struct rutl_unicode_charmap *charmap_lookup[MAX_EXTRA_CHAR_TABLES] =
{
};
static const struct rutl_unicode_charmap *extra_chars[MAX_EXTRA_CHAR_TABLES];

/* Screen dimensions */
static int screen_width;            // Screen width excluding arrows
static int screen_width_inc_arrows = -1; // Screen width including arrows
static int screen_height = -1;

static unsigned char *shadow_buffer;

static struct reciva_lcd_driver vfd_driver;

static int forced_redraw_in_progress;
static int icon_showing;
static int icons_enabled;

static void vfd_clear_screen (void);

/* nCS: J3-16 GPC2 LCDCN2 */
static int cs_pin = S3C2410_GPC2;

/* DA: J3-20 GPC0 LCDCN0 */
static int da_pin = S3C2410_GPC0;

/* nRESET: J3-14 GPC3 LCDCN3 */
static int reset_pin = 0;  // S3C2410_GPC3;

/* CP: J3-18 GPC1 LCDCN1 */
static int cp_pin = S3C2410_GPC1;

/****************************************************************************
 * Frob the chip select pin
 ****************************************************************************/
static void set_cs (int cs)
{
  s3c2410_gpio_setpin (cs_pin, cs);
}

/****************************************************************************
 * Clock out one byte
 ****************************************************************************/
static void sendbyte (int data)
{
  int i;

  for (i = 0; i < 8; i++)
  {
    int bit = data & (1 << i);

    s3c2410_gpio_setpin (cp_pin, 0);
    s3c2410_gpio_setpin (da_pin, bit ? 1 : 0);
    udelay (1);
    s3c2410_gpio_setpin (cp_pin, 1);
    udelay (1);
  }
}

static int dc_address;

static void write_dcram (int i)
{
  set_cs (0);
  sendbyte (WRITE_DCRAM | (dc_address++));
  sendbyte (i);
  set_cs (1);
}

static void set_address (int x, int y)
{
  dc_address = x | (y ? DCRAM_B : DCRAM_A);
}

/****************************************************************************
 * Set up GPIO pins
 ****************************************************************************/
static void setup_gpio (void)
{
  printk(PREFIX "%s\n", __FUNCTION__);

  if (reset_pin) {
          s3c2410_gpio_cfgpin (reset_pin, S3C2410_GPIO_OUTPUT);
          s3c2410_gpio_pullup (reset_pin, 1);
          s3c2410_gpio_setpin (reset_pin, 1);
  }

  s3c2410_gpio_cfgpin (cs_pin, S3C2410_GPIO_OUTPUT);
  s3c2410_gpio_pullup (cs_pin, 1);
  s3c2410_gpio_setpin (cs_pin, 1);

  s3c2410_gpio_cfgpin (da_pin, S3C2410_GPIO_OUTPUT);
  s3c2410_gpio_pullup (da_pin, 1);
  s3c2410_gpio_setpin (da_pin, 0);

  s3c2410_gpio_cfgpin (cp_pin, S3C2410_GPIO_OUTPUT);
  s3c2410_gpio_pullup (cp_pin, 1);
  s3c2410_gpio_setpin (cp_pin, 1);
}

/****************************************************************************
 **************************** low level lcm ops *****************************
 ****************************************************************************/

/****************************************************************************
 * Write a user defined character into CGRAM
 ****************************************************************************/
static void define_char (unsigned int slot, const unsigned char *data)
{
  int i;

  if (slot >= NR_CGRAM_SLOTS)
    BUG ();

  set_cs (0);
  sendbyte (WRITE_CGRAM | slot);
  for (i = 0; i < 5; i++)
  {
    unsigned char c = 0;
    int j;
    for (j = 0; j < 7; j++)
    {
      if (data[j] & (1 << (4 - i)))
        c |= 1 << j;
    }
    sendbyte (c);
  }
  set_cs (1);
}

/****************************************************************************
 * Return a pointer to the soft character data for the specified code.
 ****************************************************************************/
static inline const unsigned char *get_soft_char_data (unsigned short code)
{
  return &soft_chars[code].data[0];
}

/****************************************************************************
 * Load a soft character into CGRAM and return its index.
 * If all soft character slots are in use, returns -1.
 ****************************************************************************/
static int find_soft_char_slot (unsigned short code, int *loaded)
{
  int i;

  /* Check the requested character is in range.  */
  if (code >= NR_SOFT_CHARS)
    BUG ();

  /* Already in CGRAM?  */
  for (i = 0; i < NR_CGRAM_SLOTS; i++)
    {
      if (soft_char_loaded[i] == code)
	  return i;
    }

  int p;

  /* Now look for a free slot.  */
  for (i = 0; i < NR_CGRAM_SLOTS; i++)
    {
      p = (i + soft_char_eviction_pointer) % NR_CGRAM_SLOTS;
      if (soft_char_on_screen[p] == 0)
	break;
    }

  if (i == NR_CGRAM_SLOTS)
    {
      printk(PREFIX "couldn't find soft character slot for code %x\n", code);
      return -1;			// All slots in use
    }

  define_char (p, get_soft_char_data (code));
  soft_char_loaded[p] = code;
  soft_char_eviction_pointer = (soft_char_eviction_pointer + 1) % NR_CGRAM_SLOTS;
  *loaded = 1;

  return p;
}

/****************************************************************************
 * Return something that roughly approximates the soft character CODE
 ****************************************************************************/
static unsigned char get_fallback_code (unsigned short code)
{
  const char *pcUTF8 = soft_chars[code].utf8_char;
  int i;

  for (i = 0; i < NR_BASE_CHARS; i++)
    {
      if (!strcmp (pcUTF8, asBaseCharmap[i].pcUTF8Rep))
	return asBaseCharmap[i].cLocalRep;
    }

  return 'X';		// very rough indeed
}

/****************************************************************************
 * Zap the soft character data
 ****************************************************************************/
static void reset_soft_characters (void)
{
  int i;

  for (i = 0; i < NR_CGRAM_SLOTS; i++)
    soft_char_loaded[i] = 0xffff;
}

/****************************************************************************
 ****************************** lcd_generic API *****************************
 ****************************************************************************/

/****************************************************************************
 * Redraw the screen
 * Also reinit some registers on certain configs
 ****************************************************************************/
static void redraw_screen (void)
{
  reset_soft_characters ();
  forced_redraw_in_progress = 1;
  reciva_lcd_redraw();
  forced_redraw_in_progress = 0;
}

/****************************************************************************
 * Initialises LCD hardware
 ****************************************************************************/
static void vfd_init_hardware (void)
{
  printk(PREFIX "%s\n", __FUNCTION__);

  /* Set up GPIO pins */
  setup_gpio ();

  if (reset_pin) {
          s3c2410_gpio_setpin (reset_pin, 0);
          mdelay (10);
          s3c2410_gpio_setpin (reset_pin, 1);
          mdelay (100);

          memset (shadow_buffer, ' ', screen_width_inc_arrows * screen_height);
  } else {
          /* Clear the display */
          vfd_clear_screen ();
  }

  set_cs (0);
  sendbyte (TIMING_SET);
  sendbyte (0x0f);
  set_cs (1);

  mdelay (2);

  set_cs (0);
  sendbyte (DIMMING_SET);
  sendbyte (0x7f);
  set_cs (1);

  mdelay (2);

  set_cs (0);
  sendbyte (0xa0);
  sendbyte (0x7f);
  set_cs (1);

  mdelay (2);

  set_cs (0);
  sendbyte (0xa1);
  sendbyte (0x7f);
  set_cs (1);

  mdelay (2);

  set_cs (0);
  sendbyte (DISPLAY_ON);
  set_cs (1);

  mdelay (2);

  reset_soft_characters ();
}  

/****************************************************************************
 * LED control
 * level : 0 = off, non zero = on
 ****************************************************************************/
static void vfd_set_led(unsigned int mask)
{
  reciva_led_set (mask);
}

/****************************************************************************
 * Returns the device capabilities
 ****************************************************************************/
static int vfd_get_capabilities(void)
{
  return LCD_CAPABILITIES_NO_CURSOR;
}

/****************************************************************************
 * Sets the "display mode"
 ****************************************************************************/
static void vfd_set_display_mode(int iMode)
{
  icons_enabled = (iMode & 1) ? 1 : 0;
}

/****************************************************************************
 * Draws the icons
 ****************************************************************************/
static void vfd_draw_icons(unsigned int uiBitmask)
{
  int icon_was_showing = icon_showing;

  if (icons_enabled)
  {
    icon_showing = uiBitmask & (LCD_BITMASK_ALARM | LCD_BITMASK_SLEEP_TIMER | LCD_BITMASK_SHIFT);

    if (icon_showing != icon_was_showing)
      redraw_screen ();
  }
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
static int  vfd_draw_screen(char **ppcText,
                              int    iX,
                              int    iY,
                              int    iCursorType,
                              int    iCursorWidth,
                              int   *piArrows,
                              int   *piLineContents)
{
  int x, iLine, iCharCount;
  unsigned short cLocalRep;
  const char *pcCurrentChar;
  int aiOffset[vfd_driver.get_height()];

  memset (soft_char_on_screen, 0, sizeof (soft_char_on_screen));

  if (icon_showing & LCD_BITMASK_SHIFT)
  {
    // Draw this first so that it can't be displaced by other soft chars.
    unsigned short shift_icon_char = reciva_lcd_utf8_lookup ("\xe2\x87\xa7");   // UPWARDS WHITE ARROW
    int iSlot;
    int bResetAddress = 0;

    iSlot = find_soft_char_slot (shift_icon_char & 0x7fff, &bResetAddress);
    if (iSlot == -1)
      {
        printk(PREFIX "couldn't allocate soft character for shift icon\n");
        iSlot = 'S';
      }
    else
      {
        soft_char_on_screen[iSlot] = 1;
      }

    set_address (screen_width_inc_arrows - 1, 0);
    write_dcram (iSlot);
  }
  else if (icon_showing & LCD_BITMASK_ALARM)
  {
    // Draw this first so that it can't be displaced by other soft chars.
    unsigned short alarm_icon_char = reciva_lcd_utf8_lookup (LCD_ALARM_ICON);
    int iSlot;
    int bResetAddress = 0;

    iSlot = find_soft_char_slot (alarm_icon_char & 0x7fff, &bResetAddress);
    if (iSlot == -1)
      {
        printk(PREFIX "couldn't allocate soft character for alarm icon\n");
        iSlot = 'A';
      }
    else
      {
        soft_char_on_screen[iSlot] = 1;
      }
    set_address (screen_width_inc_arrows - 1, 0);
    write_dcram (iSlot);
  }
  else if (icon_showing & LCD_BITMASK_SLEEP_TIMER)
  {
    set_address (screen_width_inc_arrows - 1, 0);
    write_dcram ('Z');
  }
    
  for (iLine = 0; iLine < vfd_driver.get_height(); iLine++)
  {
    unsigned char *shadow_pointer;
    pcCurrentChar = ppcText[iLine];
    iCharCount = rutl_count_utf8_chars(pcCurrentChar);
    int skipped = 0;

    aiOffset[iLine] = (vfd_driver.get_width() - iCharCount) / 2;
    if (aiOffset[iLine] < 0) 
      aiOffset[iLine] = 0;

    /* Write the whole line */
    set_address (0, iLine);
    shadow_pointer = shadow_buffer + (iLine * screen_width_inc_arrows);

    for (x=0; x<screen_width_inc_arrows; x++)
    {
      if (x == screen_width_inc_arrows-1 && icons_enabled)
      {
        if (iLine == 0 && (icon_showing & (LCD_BITMASK_ALARM | LCD_BITMASK_SLEEP_TIMER | LCD_BITMASK_SHIFT)))
          continue;             // The alarm icon overwrites this arrow.

        cLocalRep = ' ';
      }
      else
      {
        if (x >= aiOffset[iLine] && *pcCurrentChar)
        {
          cLocalRep = reciva_lcd_utf8_lookup(pcCurrentChar);
          pcCurrentChar = rutl_find_next_utf8(pcCurrentChar);
        }
        else
        {
          cLocalRep = ' ';
        }
      }

      if (cLocalRep & 0x8000)
	{
	  /* This is a soft character.  Load it into RAM. */
	  int bResetAddress = 0;
	  int iSlot = find_soft_char_slot (cLocalRep & 0x7fff, &bResetAddress);
	  if (iSlot == -1)
	    {
	      cLocalRep = get_fallback_code (cLocalRep & 0x7fff);
	    }
	  else
	    {
	      soft_char_on_screen[iSlot] = 1;
	      cLocalRep = (unsigned short)iSlot;
	    }
	}

      if (cLocalRep & 0xff00)
	BUG ();

      if (forced_redraw_in_progress || cLocalRep != *shadow_pointer)
      {
        if (skipped)
        {
          set_address (x, iLine);
          skipped = 0;
        }

        write_dcram (cLocalRep);
        *shadow_pointer = cLocalRep;
      }
      else
        skipped = 1;

      shadow_pointer++;
    }
  }

  return 0;
}

/****************************************************************************
 * Clears the display
 ****************************************************************************/
static void vfd_clear_screen (void)
{
        int i;
        set_address (0, 0);
        for (i = 0; i < screen_width_inc_arrows; i++)
                write_dcram (' ');
        set_address (0, 1);
        for (i = 0; i < screen_width_inc_arrows; i++)
                write_dcram (' ');

        memset (shadow_buffer, ' ', screen_width_inc_arrows * screen_height);
}

/****************************************************************************
 * Returns 1 if pcString already has an entry in ptMap
 ****************************************************************************/
static int mapping_exists (struct rutl_unicode_charmap *ptMap, const char *pcString)
{
  while (ptMap->pcUTF8Rep != NULL)
    {
      if (!strcmp (ptMap->pcUTF8Rep, pcString))
	return 1;
      ptMap ++;
    }

  return 0;
}

/****************************************************************************
 * Returns the current width of the display in characters
 ****************************************************************************/
static int vfd_get_width (void)
{
  return screen_width;
}

static void vfd_power_off (void)
{
}

/* Note that any elements not explicitly initialised here are guaranteed to 
 * be zero initialised */
static struct reciva_lcd_driver vfd_driver = {
  name:              acModuleName,
  init_hardware:     vfd_init_hardware,
  power_off:         vfd_power_off,
  get_height:        reciva_lcd_get_height,
  get_width:         vfd_get_width,
  draw_screen:       vfd_draw_screen,
  clear_screen:      vfd_clear_screen,
  set_backlight:     reciva_bl_set_backlight,
  get_max_backlight: reciva_bl_get_max_backlight,
  set_led:           vfd_set_led,
  redraw_screen:     redraw_screen,
  draw_icons:        vfd_draw_icons,
  set_display_mode:  vfd_set_display_mode,
  get_capabilities:  vfd_get_capabilities,
};

/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init
vfd_lcd_init (void)
{
  char *s;
  int i;

  // Stingray doesn't have GPC0, use GPC6 instead
  if (machine_is_rirm3())
    da_pin = S3C2410_GPC6;

  screen_width_inc_arrows = 16;
  screen_width = 16;
  screen_height = 2;

  if (charmap_string)
  {
    printk(PREFIX "charmap_string = %s\n", charmap_string);
    s = charmap_string;
    while (*s)
    {
      i = simple_strtol(s, NULL, 10);
      printk(PREFIX "  index = %d\n", i);

      if (i < MAX_EXTRA_CHAR_TABLES && i >= 0)
        extra_chars[i] = charmap_lookup[i];

      s = strchr(s, ' ');
      if (s)
        s++;
      else
        break;
    }
  }
#if 0
  else
  {
    for (i = 0; i < MAX_EXTRA_CHAR_TABLES; i++)
      extra_chars[i] = lcm->extra_chars[i];
  }
#endif

  shadow_buffer = kmalloc (screen_width_inc_arrows * screen_height, GFP_KERNEL);
  if (!shadow_buffer)
    return -ENOMEM;

  printk(PREFIX "screen_height = %d\n", screen_height);
  printk(PREFIX "screen_width = %d\n", screen_width);
  printk(PREFIX "screen_width_inc_arrows = %d\n", screen_width_inc_arrows);

  /* Build the character mapping table. */
  int iMapSize = NR_SOFT_CHARS + NR_BASE_CHARS;

  for (i = 0; i < MAX_EXTRA_CHAR_TABLES; i++)
    {
      const struct rutl_unicode_charmap *ptTable = extra_chars[i];
      if (ptTable)
	{
	  int j;

	  for (j = 0; ptTable[j].pcUTF8Rep != NULL; j++)
	    ;

	  iMapSize += j;
	}
    }

  int p = 0;
  struct rutl_unicode_charmap *asCharmap = kmalloc ((iMapSize + 1) * sizeof (struct rutl_unicode_charmap), 
						    GFP_KERNEL);
  if (asCharmap == NULL)
    {
      printk (PREFIX "no memory for character map\n");
      return -ENOMEM;
    }

  memset (asCharmap, 0, (iMapSize + 1) * sizeof (struct rutl_unicode_charmap));

  /* Load the extra mapping tables */
  for (i = 0; i < MAX_EXTRA_CHAR_TABLES; i++)
    {
      const struct rutl_unicode_charmap *ptTable = extra_chars[i];
      if (ptTable)
	{
	  int j;

	  for (j = 0; ptTable[j].pcUTF8Rep != NULL; j++)
	    {
	      asCharmap[p].pcUTF8Rep = ptTable[j].pcUTF8Rep;
	      asCharmap[p].cLocalRep = ptTable[j].cLocalRep;
	      p++;
	      if (p > iMapSize)
		BUG ();
	    }
	}
    }

  /* Load the soft character map */
  for (i = 0; i < NR_SOFT_CHARS; i++)
    {
      if (!mapping_exists (asCharmap, soft_chars[i].utf8_char))
	{
	  asCharmap[p].pcUTF8Rep = soft_chars[i].utf8_char;
	  asCharmap[p].cLocalRep = i | 0x8000;
	  p++;
	  if (p > iMapSize)
	    BUG ();
	}
    }

  /* Load the fallback character map */
  for (i = 0; i < NR_BASE_CHARS; i++)
    {
      if (!mapping_exists (asCharmap, asBaseCharmap[i].pcUTF8Rep))
	{
	  asCharmap[p].pcUTF8Rep = asBaseCharmap[i].pcUTF8Rep;
	  asCharmap[p].cLocalRep = asBaseCharmap[i].cLocalRep;
	  p++;
	  if (p > iMapSize)
	    BUG ();
	}
    }

  vfd_driver.charmap = asCharmap;

  return reciva_lcd_init(&vfd_driver, screen_height, screen_width);
}

/****************************************************************************
 * Module cleanup
 ****************************************************************************/
static void __exit
vfd_lcd_exit(void)
{
  kfree (shadow_buffer);
  reciva_lcd_exit ();
  kfree (vfd_driver.charmap);
}

MODULE_AUTHOR("John Stirling <js@reciva.com>");
MODULE_DESCRIPTION("Reciva LCD driver");
MODULE_LICENSE("GPL");

module_init(vfd_lcd_init);
module_exit(vfd_lcd_exit);
