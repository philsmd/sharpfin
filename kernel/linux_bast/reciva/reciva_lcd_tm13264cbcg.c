/*
 * Reciva LCD driver code for ALTO LCD module
 * Tianma TM13264CBCG (module)
 * Samsung S6B1713 (LCD controller)
 * Copyright (c) 2005-06 Reciva Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * Notes :
 *
 * This module 'owns' all GPC GPIO pins.
 *
 * GPIO PIN(S)              LCD
 * -----------              ---
 * GPC0 (+GPB2, GPA12)      E
 * GPC1 (+ GPB3, GPA13)     RW
 * GPC2                     RS (D/C on Alto schematics)
 * GPC3                     RESETB
 * GPC4  (+ GPB1)           CS1B
 * GPC5                     POWER
 * GPC6                     (not connected on module)
 * GPC7                     (not connected on module)
 * GPC8-15                  LCDD0 - LCDD7
 *
 *                                              
 * The LCD has 132 columns and 64 rows.
 * We support 2 fonts (32x16 and 16x8)
 * The display is allocated as follows
 *
 * 77777777777777777777777777777777777777777
 * 66666666666666666666666666666666666666666
 * 55555555555555555555555555555555555555555
 * 44444444444444444444444444444444444444444
 * 33333333333333333333333333333333333333333
 * 22222222222222222222222222222222222222222
 * 11111111111111111111111111111111111111111
 * 00000000000000000000000000000000000000000
 *
 * This module can operate in one of 4 display modes.
 * The mode is selected via a module parameter
 * Arrows are not supported - application will just need to send 
 *
 * Mode LCD_TM13264CBCG_MODE_TM13264CBCG_0:
 * ----------------------------------------
 *   2 line display with icons on bottom row.
 *   Can zoom text 
 * 
 *   16x8 Font
 *   ---------
 *   6,5 = Top line of text
 *   4,3 = 2nd line of text
 *   2   = Unused
 *   1   = icons (shift indication, signal strength bars etc)
 *   0   = Unused
 *
 *   32x16 Font
 *   ----------
 *   7,6,5,4 = Top line of text
 *   3,2,1,0 = 2nd line of text
 *   This module takes a parameter to define the number of lines etc
 *   0 = 2 lines, with icons on bottom row (used on ALTO)
 *   1 = 3 lines, with icons on bottom row
 *   2 = 4 lines, with no icons
 *   3 = 3 lines, with icons on top row
 *
 * Mode LCD_TM13264CBCG_MODE_TM13264CBCG_1:
 * ----------------------------------------
 *   3 line display with icons on bottom row.
 *   Can't zoom text 
 *
 *   16x8 Font
 *   ---------
 *   7,6 = Top line of text
 *   5,4 = 2nd line of text
 *   3,2 = 3rd line of text
 *   1,0 = icons + signal strength meter
 *
 * Mode LCD_TM13264CBCG_MODE_TM13264CBCG_2:
 * ----------------------------------------
 *   4 line display with no icons.
 *   Can't zoom text 
 *
 *   16x8 Font
 *   ---------
 *   7,6 = Top line of text
 *   5,4 = 2nd line of text
 *   3,2 = 3rd line of text
 *   1,0 = 4th line of text
 *
 * Mode LCD_TM13264CBCG_MODE_TM13264CBCG_3:
 * ----------------------------------------
 *   4 line display with icons on top row.
 *   Can't zoom text 
 *
 *   16x8 Font
 *   ---------
 *   7,6 = icons + signal strength meter
 *   5,4 = 1st line of text
 *   3,2 = 2nd line of text
 *   1,0 = 3rd line of text
 *
 * Mode LCD_TM13264CBCG_MODE_AMAX_KDC162A28BXXBB_95x17:
 * ----------------------------------------------------
 *   2 line display, no icons
 *   97 * 17 pixel display
 *   Can't zoom text 
 *   8x6 Font
 *
 * Mode LCD_TM13264CBCG_MODE_SSD0323_128x64:
 * ----------------------------------------------------
 *   3 line display OLED, icons on bottom
 *   128 * 64 pixel display (4 bit greyscale)
 *   16x8 Font
 *
 * Mode LCD_TM13264CBCG_MODE_CONFIG1009:
 * ----------------------------------------------------
 *  6 line display
 *  8x6 font
 *  icons on bottom
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
#include <linux/ctype.h>

#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/io.h>

#include <asm/arch-s3c2410/S3C2410-timer.h>
#include <asm/arch-s3c2410/S3C2410-irq.h>

#include "reciva_gpio.h"
#include "reciva_util.h"
#include "reciva_leds.h"
#include "reciva_backlight.h"
#include "reciva_lcd.h"
#include "lcd_generic.h"
#include "fontdata.h"
#include "reciva_font_chinese.h"
#include "reciva_lcd_tm13264cbcg.h"

/* This module takes a parameter to define the number of lines etc
 * 0 = 2 lines, with icons on bottom row (used on ALTO)
 * 1 = 3 lines, with icons on bottom row
 * 2 = 4 lines, with no icons
 * 3 = 3 lines, with icons on top row
 * 4 = 2 lines, no icons (95 *17 pixels)
 * 8 = 3 lines, no icons, 10x8 font (132 * 32 pixels) */
static lcd_tm13264cbcg_mode_t lcd_display_mode = LCD_TM13264CBCG_MODE_TM13264CBCG_0;
MODULE_PARM(lcd_display_mode, "i");

/* Defines the initial contrast level */
static int lcd_contrast_level = -1;
MODULE_PARM(lcd_contrast_level, "i");

/* Defines the initial backlight level */
static int lcd_backlight_level = LCDG_MAX_BACKLIGHT;
MODULE_PARM(lcd_backlight_level, "i");

/* Defines if display is reversed
 * 0 = normal, 1 = reversed */
static int segment_remap = 0;
MODULE_PARM(segment_remap, "i");

/* Define the clock format
 * LCD_CLOCK_CONFIG0 : time (hours, minutes) in big font with alarm time in 
 * brackets underneath
 * LCD_CLOCK_CONFIG1 : time in 32x16 font, date in 16x8 font, alarm icon
 */
typedef enum
{
  LCD_CLOCK_CONFIG0           = 0,
  LCD_CLOCK_CONFIG1           = 1,
} clock_config_t;
static clock_config_t clock_config = LCD_CLOCK_CONFIG0;
MODULE_PARM(clock_config, "i");


   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

typedef struct
{
  unsigned char acData[6];

} FontData8x6_t;

typedef enum
{
  NOT_INVERTED = 0,
  INVERTED     = 1,

} InvertStatus_t;

   /*************************************************************************/
   /***                        Static Functions                           ***/
   /*************************************************************************/

static int __init tm13264cbcg_lcd_init (void);
static void __exit tm13264cbcg_lcd_exit(void);

/* Functions in lcd interface */
static void tm13264cbcg_init_hardware(void);
static int  tm13264cbcg_draw_screen(char **acText, int iX, int iY,
                                    int iCursorType, int *piArrows, int *piLineContents);
static int tm13264cbcg_get_height(void);
static int tm13264cbcg_get_width(void);
static void tm13264cbcg_clear_screen(void);
static void tm13264cbcg_set_led(unsigned int mask);
static void tm13264cbcg_set_contrast(int level);
static void tm13264cbcg_power_off(void);
static void tm13264cbcg_draw_clock(struct lcd_draw_clock *psClockInfo);
static void tm13264cbcg_draw_signal_strength(int iLevel);
static void tm13264cbcg_set_zoom(int iOn, struct lcd_resize *psNewSize);
static void tm13264cbcg_draw_icons(unsigned int iBitmask);
static int tm13264cbcg_get_capabilities(void);
static void tm13264cbcg_set_display_mode(int iMode);
static void tm13264cbcg_draw_bitmap(struct bitmap_data);
static int tm13264cbcg_get_graphics_height(void);
static int tm13264cbcg_get_graphics_width(void);
static struct bitmap_data tm13264cbcg_grab_screen_region(int left, int top, int width, int height);
static void redraw_screen(void);
static int tm13264cbcg_set_ampm_text(struct lcd_ampm_text *ampm_text);
  
/* Local helper functions */
static void setup_gpio (void);
static void setup_gpio_1bit_interface(void);
static void setup_gpio_8bit_interface(void);
static void init_registers_default(void);
static void init_calgary(void);
static void init_registers_calgary(void);
static int stretch4to8(int data4bit);
static int draw_line(int iLine, const char *pcText, int iX, 
                     int iCursorType, InvertStatus_t tInverted, int iArrows);
static void draw_character_8x6(char cData, int iInvert);
static void draw_character_8x5(char cData, int iInvert);
static void draw_character_16x8(char cData, int iInvert, int iBottomHalf);
static void draw_chinese_character_16x16(int iData, int iInvert, int iBottomHalf);
static void draw_character(const unsigned short *pFontData, int iWidth,
                           int iInvert, int iSection);
static void draw_icons_8x8(unsigned int uiBitmask);
static void draw_icon_8x8(int iIcon);
static void draw_icons_16x16(unsigned int uiBitmask);
static void draw_icon_16x16(int iIcon, int iTopHalf);
static void draw_character_32x16(char cData, int iInvert, int iSection);
static void draw_chinese_character_32x32(int iData, int iInvert, int iSection);
static void draw_zoomed_character(const unsigned short *pFontData, int iWidth,
                                  int iInvert, int iSection);
static void draw_8x6_character(const FontData8x6_t *ptData);
static void draw_clock_digit(int iPosition, int iNumber, int iColumnOffset);
static void draw_clock_colon(int iOffset);
static void draw_half_spaces(int iCount, InvertStatus_t tInvertStatus);
static void draw_blank_columns8x1(int iCount, InvertStatus_t tInvertStatus);
static int draw_barcode(int iLine, const char *pcText);
static void draw_barcode_char(int iCount);
static void set_display_byte(int xpos, int row, unsigned char value, struct bitmap_data sBitmap);
static struct bitmap_data new_bitmap(int left, int top, int width, int height);
static void test_pattern(void);
static void set_row(int row);
static void blit_byte(unsigned char data);
static int get_text_from_user(char **ppcDest, char *pcSrc);
static int iDoCharLookupOnString(const char *pcText, int *piLocalRep,
                                 int iWidth, int *piCharLength);
static void draw_alarm_time(int i12HourFormat, int iColumnOffset,
                            int iAlarmHours, int iAlarmMinutes);
#define SHIFT_OFF 0
#define SHIFT_ON  1

#define RS_CONTROL  0
#define RS_DATA     1
static void send_data(int data); 
static void send_command(int data);
static void cycle (int iRS, int iData, int iDelay);
static void cycle_1bit (int iRS, int iData, int iDelay);
static void cycle_8bit (int iRS, int iData, int iDelay);

#if ENABLE_READ
static int read_8bit (int iRS, int iDelay);
#endif

static void update_screen_dimensions(void);
static int allowed_to_draw_icons(void);

static int current_column = 0;
static int current_row = 0;

#define AMPM_LENGTH 2
static int aiAM[AMPM_LENGTH] = {'A', 'M'};
static int iAMLen = AMPM_LENGTH;
static int aiPM[AMPM_LENGTH] = {'P', 'M'};
static int iPMLen = AMPM_LENGTH;

#define PFX "tm13264cbcg: "

   /*************************************************************************/
   /***                        Local defines                              ***/
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
/* GPIO port D */
#define GPDCON GPIO_REG(0x30)
#define GPDDAT GPIO_REG(0x34)
#define GPDUP GPIO_REG(0x38)
/* GPIO port F */
#define GPFCON GPIO_REG(0x50)
#define GPFDAT GPIO_REG(0x54)
#define GPFUP GPIO_REG(0x58)
/* GPIO port G */
#define GPGCON GPIO_REG(0x60)
#define GPGDAT GPIO_REG(0x64)
#define GPGUP GPIO_REG(0x68)
/* GPIO port H */
#define GPHCON GPIO_REG(0x70)
#define GPHDAT GPIO_REG(0x74)
#define GPHUP GPIO_REG(0x78)

#define EXTINT0    GPIO_REG(0x88)

/* Size of the graphics surfaces. N.B Graphics
 * Height MUST be a multiple of 8 */
#define GRAPHICS_WIDTH_ALTO   132
#define GRAPHICS_HEIGHT_ALTO  64
#define GRAPHICS_XOFFSET_ALTO 0
#define GRAPHICS_YOFFSET_ALTO 0

#define GRAPHICS_WIDTH_AMAX   95
#define GRAPHICS_HEIGHT_AMAX  24
#define GRAPHICS_XOFFSET_AMAX 0
#define GRAPHICS_YOFFSET_AMAX 15

/* Screen dimensions for both font sizes and modes */
#define SCREEN_HEIGHT_MODE_0 2
#define SCREEN_HEIGHT_MODE_1 3
#define SCREEN_HEIGHT_MODE_2 4
#define SCREEN_HEIGHT_MODE_3 3
#define SCREEN_HEIGHT_AMAX   2
#define MAX_SCREEN_HEIGHT   SCREEN_HEIGHT_MODE_2

#define SCREEN_WIDTH_16x8   15
#define SCREEN_WIDTH_32x16  8
#define SCREEN_WIDTH_AMAX   14
#define SCREEN_WIDTH_CONFIG1009   20
#define MAX_SCREEN_WIDTH    SCREEN_WIDTH_CONFIG1009

/* Barcode lookup table
 * Bars alternate between black and white
 * 1 indicates thick bar
 * 0 indicates thin bar
 * Ratio 3:1 (minimum 2.2:1) -> codes are at least 15 pixels wide */
static int code39_lookup[] =
{
  /* 0: 0 0011 0100 */ 0x034,
  /* 1: 1 0010 0001 */ 0x121,
  /* 2: 0 0110 0001 */ 0x061,
  /* 3: 1 0110 0000 */ 0x160,
  /* 4: 0 0011 0001 */ 0x031,
  /* 5: 1 0011 0000 */ 0x130,
  /* 6: 0 0111 0000 */ 0x070,
  /* 7: 0 0010 0101 */ 0x025,
  /* 8: 1 0010 0100 */ 0x124,
  /* 9: 0 0110 0100 */ 0x064,
  /* A: 1 0000 1001 */ 0x109,
  /* B: 0 0100 1001 */ 0x049,
  /* C: 1 0100 1000 */ 0x148,
  /* D: 0 0001 1001 */ 0x019,
  /* E: 1 0001 1000 */ 0x118,
  /* F: 0 0101 1000 */ 0x058,
};

/* Start/Stop barcode: 0 1001 0100 */
#define BAR_STARTSTOP_CHAR 0x094

/* Maximum number of barcode characters we can display
 *
 * From above each character is 15 pixels, plus a 1 pixels inter-char gap
 * Thus 132 / 16 = 8 characters. Code requires start and startop char so
 * maximum useful characters is 6 */
#define MAX_BARCODE_LENGTH 6




   /*************************************************************************/
   /***                        Static data                                ***/
   /*************************************************************************/

static const char acModuleName[] = "Reciva LCD TM13264CBCG";

/* Note that any elements not explicitly initialised here are guaranteed to 
 * be zero initialised */
static struct reciva_lcd_driver tm13264cbcg_driver = {
  name:                 acModuleName,
  init_hardware:        tm13264cbcg_init_hardware,
  get_height:           tm13264cbcg_get_height,
  get_width:            tm13264cbcg_get_width,
  set_zoom:             tm13264cbcg_set_zoom,
  draw_screen:          tm13264cbcg_draw_screen,
  clear_screen:         tm13264cbcg_clear_screen,
  set_backlight:        reciva_bl_set_backlight,
  set_led:              tm13264cbcg_set_led,
  power_off:            tm13264cbcg_power_off,
  draw_clock:           tm13264cbcg_draw_clock,
  draw_signal_strength: tm13264cbcg_draw_signal_strength,
  draw_icons:           tm13264cbcg_draw_icons,
  charmap:              asCharMap,
  leds_supported:       RLED_MENU | RLED_VOLUME,
  get_capabilities:     tm13264cbcg_get_capabilities,
  set_display_mode:     tm13264cbcg_set_display_mode,
  draw_bitmap:          tm13264cbcg_draw_bitmap,
  get_graphics_width:   tm13264cbcg_get_graphics_width,
  get_graphics_height:  tm13264cbcg_get_graphics_height,
  grab_screen_region:   tm13264cbcg_grab_screen_region,
  set_contrast:         tm13264cbcg_set_contrast,
  test_pattern:         test_pattern,
  redraw_screen:        redraw_screen,
  set_ampm_text:        tm13264cbcg_set_ampm_text,
};

/* Font size */
typedef enum 
{
  FONT_SIZE_16x8,
  FONT_SIZE_32x16,
  FONT_SIZE_8x6,
} FontSize_t;
static FontSize_t font_size;

/* Top row of text for given display mode */
static int iTopRow;

/* Screen dimensions */
static int screen_width = MAX_SCREEN_WIDTH;
static int screen_height = MAX_SCREEN_HEIGHT;
static int graphics_width = -1;
static int graphics_height = -1;
static int graphics_xoffset = -1;
static int graphics_yoffset = -1;

/* Zoom related */
static int zoom_enabled = 0;
static int zoom_allowed = 0;

/* Icon size and placement */
static int icon_size;
#define ICON_POSITION_TOP      0
#define ICON_POSITION_BOTTOM   1
static int icon_position;

                                                     
/* This is the main font used for text display - 16 pixels high, 8 bits wide */
static const unsigned short au16FontData[] =
{
#include "fontdata.c"
};

static struct bitmap_data sDisplayBuffer = { .data = NULL };

/* 8x6 font */
static const unsigned char aucFontData_8x6[] =
{
#include "reciva_fontdata_8x6.c"
};

/* 16x16 and Icon data */
static const unsigned short au16IconData[] =
{
  /* 0x00 Blank */        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  /* 0x01 SHIFT */        0x0040, 0x00c0, 0x0140, 0x0240, 0x047f, 0x0801, 0x0801, 0x047f, 0x0240, 0x0140, 0x00c0, 0x0040, 0x0000, 0x0000, 0x0000, 0x0000, 
  /* 0x02 IRADIO */       0x0600, 0x0f49, 0x0f49, 0x0649, 0x0089, 0x0711, 0x0022, 0x0044, 0x0788, 0x0010, 0x0020, 0x07c0, 0x0000, 0x0000, 0x0000, 0x0000,
  /* 0x03 MEDIA */        0x0000, 0x0000, 0x0000, 0x0007, 0x0007, 0x0007, 0x03ff, 0x0300, 0x0180, 0x00e0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  /* 0x04 SHUFFLE */      0x06c3, 0x06c3, 0x06c3, 0x06c3, 0x06db, 0x06db, 0x06db, 0x06db, 0x0018, 0x0018, 0x0018, 0x0018, 0x0000, 0x0000, 0x0000, 0x0000,
  /* 0x05 REPEAT */       0x0000, 0x0000, 0x0020, 0x0060, 0x00e0, 0x07ff, 0x04e1, 0x0461, 0x0421, 0x0401, 0x0401, 0x07ff, 0x0000, 0x0000, 0x0000, 0x0000,
  /* 0x06 SLEEP TIMER */  0x0000, 0x01fc, 0x0202, 0x0401, 0x0401, 0x07e1, 0x0421, 0x0421, 0x0222, 0x01fc, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  /* 0x07 MUTE */         0x00f0, 0x08f0, 0x04f0, 0x02f0, 0x01f8, 0x03fc, 0x07fe, 0x0fff, 0x0010, 0x0008, 0x0004, 0x0002, 0x0000, 0x0000, 0x0000, 0x0000,
  /* 0x08 ALARM */        0x0000, 0x0004, 0x000c, 0x03f4, 0x0404, 0x0806, 0x0807, 0x0806, 0x0404, 0x03f4, 0x000c, 0x0004, 0x0000, 0x0000, 0x0000, 0x0000,
  /* 0x09 SNOOZE */       0x0430, 0x0450, 0x0490, 0x0510, 0x0610, 0x0410, 0x0046, 0x004a, 0x0052, 0x0062, 0x0042, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

/* 8x8 and Icon data (can be used instead of 16x16) */
static const unsigned char au8IconData8x8[] =
{                                                                                         
  /* 0x00 Blank */        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* 0x01 SHIFT */        0x10, 0x30, 0x5f, 0x81, 0x5f, 0x30, 0x10, 0x00,
  /* 0x02 IRADIO */       0xea, 0xea, 0xca, 0x12, 0xe4, 0x08, 0xf0, 0x00,
  /* 0x03 MEDIA */        0x03, 0x03, 0x03, 0xff, 0xc0, 0x60, 0x38, 0x00,
  /* 0x04 SHUFFLE */      0xc3, 0xc3, 0xdb, 0xdb, 0xdb, 0x18, 0x18, 0x00,
  /* 0x05 REPEAT */       0x08, 0x18, 0xff, 0x99, 0x89, 0x81, 0xff, 0x00,
  /* 0x06 SLEEP TIMER */  0x3c, 0x42, 0x81, 0xf1, 0x91, 0x52, 0x3c, 0x00,
  /* 0x07 MUTE */         0x98, 0x58, 0x3c, 0x7e, 0xff, 0x04, 0x02, 0x00,
  /* 0x08 ALARM */        0x04, 0x0c, 0x7c, 0xfe, 0x7c, 0x0c, 0x04, 0x00,
  /* 0x09 SNOOZE */       0x98, 0xa8, 0xc8, 0x88, 0x00, 0x16, 0x1a, 0x12,
};

/* Data for 32x16 clock display "1234567890:2 */
static const unsigned long au32Font32x26[] =
{
  #include "clock_fontdata_32x16.c"
};

/*  Signal strength icon and bars - 8 pixels high, 6 pixels wide */
#define SIG_STRENGTH_BARS 4
static const FontData8x6_t au8SignalStrength8x6[SIG_STRENGTH_BARS+1] =
{
/*0*/ {{0xc0, 0xa0, 0xff, 0xa0, 0xc0, 0x00}}, // Signal strength ICON (large)
/*1*/ {{0x03, 0x03, 0x03, 0x03, 0x03, 0x00}}, // Signal strength bar - 2 pixels
/*2*/ {{0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x00}}, // Signal strength bar - 4 pixels
/*3*/ {{0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x00}}, // Signal strength bar - 6 pixels
/*4*/ {{0xff, 0xff, 0xff, 0xff, 0xff, 0x00}}, // Signal strength bar - 8 pixels
};

/*  Blank character - 8 pixels high, 6 pixels wide */
static const FontData8x6_t au8Space8x6[] =
{
  /* ' ' */ {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
};

/* Left and right arrows - 6 pixels wide */
#define ARROW_WIDTH 6
static unsigned short au16LeftArrowLookup[ARROW_WIDTH] =
{ 0x0100, 
  0x0380,
  0x06c0,
  0x0c60,
  0x1830,
  0x0000,
};
static unsigned short au16RightArrowLookup[ARROW_WIDTH] =
{
  0x1830,
  0x0c60,
  0x06c0,
  0x0380,
  0x0100,
  0x0000,
};



/* Indicates if power fail hardware is present */
static int power_fail_present;
  
/* Device capabilities */  
static int iCapabilities = LCD_CAPABILITIES_INVERTED_TEXT |
                           LCD_CAPABILITIES_ARROWS |
                           LCD_CAPABILITIES_GRAPHICS;



   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/

                           
/****************************************************************************
 * Return display dimensions
 ****************************************************************************/
static int tm13264cbcg_get_height(void)
{
  return screen_height;
}
static int tm13264cbcg_get_width(void)
{
  return screen_width;
}

/****************************************************************************
 * Shut down LCD when power fails
 ****************************************************************************/
static void
tm13264cbcg_power_fail (int irq, void *handle, struct pt_regs *regs)
{
  disable_irq (IRQ_EINT1);

  switch (lcd_display_mode)
  {
    case LCD_TM13264CBCG_MODE_TM13264CBCG_0:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_1:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_2:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_3:
    case LCD_TM13264CBCG_MODE_AMAX_KDC162A28BXXBB_95x17:
    case LCD_TM13264CBCG_MODE_CONFIG1009:
    case LCD_TM13264CBCG_MODE_CONFIG985:
      rutl_regwrite(0, LCD_RESET, RIRM_LCDCN_DAT);    // set RESETB low
      break;
    case LCD_TM13264CBCG_MODE_SSD0323_128x64:
      /* Power down Vcc (15V) */
      rutl_regwrite(LCD_POWER, 0, RIRM_LCDCN_DAT);    // High (OFF)
      /* Display OFF */
      cycle (RS_CONTROL, CTL_DISPLAY_OFF, 0);
      break;
    case LCD_TM13264CBCG_MODE_CONFIG983:
      break;
  }

  printk (PFX "Power off\n");
}

/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init
tm13264cbcg_lcd_init (void)
{
  int iErr =0;

  printk(PFX "init mode=%d\n", lcd_display_mode);
  
  /* Set up the screen dimensions and font size etc */
  tm13264cbcg_set_display_mode(lcd_display_mode);
  
  /* set GPF1 as input */
  rutl_regwrite (0 << 2, 3 << 2, GPFCON);
  /* enable pullup */
  rutl_regwrite (0 << 1, 1 << 1, GPFUP);
  /* wait a bit */
  mdelay (1);
  if ((__raw_readl (GPFDAT) & (1 << 1)) == 0)
    power_fail_present = 1;

  printk (PFX "power failure hardware %s present\n", power_fail_present ? "is" : "not");

  if (power_fail_present) {
    /* set GPF1 as EINT1 */
    rutl_regwrite (2 << 2, 1 << 2, GPFCON);

    /* set EINT1 as rising edge triggered */
    rutl_regwrite (4 << 4, 3 << 4, EXTINT0);

    /* clear up any outstanding interrupts */
    __raw_writel (1 << 1, S3C2410_SRCPND);

    request_irq (IRQ_EINT1, tm13264cbcg_power_fail, SA_INTERRUPT | SA_SHIRQ, "power fail", &tm13264cbcg_power_fail);
  }
    
  /* generic initialisation */
  tm13264cbcg_driver.leds_supported = reciva_get_leds_supported();
  iErr = reciva_lcd_init(&tm13264cbcg_driver, screen_height, screen_width);
  if (iErr < 0)
  {
    printk (KERN_ERR PFX "reciva_lcd_init failure (err %d)\n", iErr);
    tm13264cbcg_lcd_exit();
  }

  /* Backlight */
  if (lcd_backlight_level < 0)
    lcd_backlight_level = 0;
  else if (lcd_backlight_level > LCDG_MAX_BACKLIGHT)
    lcd_backlight_level = 31;
  reciva_lcd_set_backlight (lcd_backlight_level);

  return iErr;
}

/****************************************************************************
 * Initialises LCD hardware
 ****************************************************************************/
static void tm13264cbcg_init_hardware()
{
  /* Set up GPIO pins */
  setup_gpio ();

  /* Initialise the registers */
  switch (lcd_display_mode)
  {
    case LCD_TM13264CBCG_MODE_TM13264CBCG_0:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_1:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_2:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_3:
    case LCD_TM13264CBCG_MODE_AMAX_KDC162A28BXXBB_95x17:
    case LCD_TM13264CBCG_MODE_CONFIG1009:
    case LCD_TM13264CBCG_MODE_CONFIG985:
    case LCD_TM13264CBCG_MODE_CONFIG983:
      init_registers_default();
      break;
    case LCD_TM13264CBCG_MODE_SSD0323_128x64:
      init_calgary();
      break;
  }

  /* Set contrast */
  if (lcd_contrast_level >= 0)
    tm13264cbcg_set_contrast(lcd_contrast_level);
}

/****************************************************************************
 * Initialise the registers - default
 ****************************************************************************/
static void init_registers_default(void)
{
  /* Segment mapping */
  if (segment_remap)
    cycle (RS_CONTROL, CTL_ADC_SEG_REVERSE, 0);  /* Reversed */
  else
    cycle (RS_CONTROL, CTL_ADC_SEG_NORMAL, 0);  /* Normal */

  /* SHL Select - reverse direction */
  cycle (RS_CONTROL, CTL_SHL_SEG_REVERSE, 0);

  /* LCD Bias Select - Duty Ratio = 1/65 Bias = 1/9  */
  /*                   Duty Ratio = 1/33 Bias = 1/6  */
  cycle (RS_CONTROL, CTL_LCD_BIAS_1, 0);

  /* Power Control - Power Supply Configuration (V0 = 9.45V) */
  cycle (RS_CONTROL, CTL_POWER | CTL_POWER_VC, 0); // Voltage Converter ON
  mdelay(2);          // Wait > 1ms
  cycle (RS_CONTROL, CTL_POWER | CTL_POWER_VC | CTL_POWER_VR, 0); // Voltage Converter, Regulator ON
  mdelay(2);          // Wait > 1ms
  cycle (RS_CONTROL, CTL_POWER | CTL_POWER_VC | CTL_POWER_VR | CTL_POWER_VF, 0); // Voltage Converter, Regulator, Follower ON

  /* Regulator Resistor Select */
  switch (lcd_display_mode)
  {
    case LCD_TM13264CBCG_MODE_TM13264CBCG_0:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_1:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_2:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_3:
    case LCD_TM13264CBCG_MODE_SSD0323_128x64:
    case LCD_TM13264CBCG_MODE_CONFIG985:
    case LCD_TM13264CBCG_MODE_CONFIG983:
      cycle (RS_CONTROL, CTL_REGRES_5_29, 0); // Rb/Ra = 5.29
      break;
    case LCD_TM13264CBCG_MODE_CONFIG1009:
    case LCD_TM13264CBCG_MODE_AMAX_KDC162A28BXXBB_95x17:
      cycle (RS_CONTROL, CTL_REGRES_3_02, 0);
      break;
  }

  /* Reference Voltage Select (2 byte instruction) */
  cycle (RS_CONTROL, CTL_SET_REF_V, 0); // Select mode
  switch (lcd_display_mode)
  {
    case LCD_TM13264CBCG_MODE_TM13264CBCG_0:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_1:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_2:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_3:
    case LCD_TM13264CBCG_MODE_SSD0323_128x64:
      cycle (RS_CONTROL, 0x12, 0); 
      break;
    case LCD_TM13264CBCG_MODE_CONFIG985:
      cycle (RS_CONTROL, 0x08, 0); 
      break;
    case LCD_TM13264CBCG_MODE_CONFIG983:
      cycle (RS_CONTROL, 46, 0); 
      break;
    case LCD_TM13264CBCG_MODE_CONFIG1009:
      cycle (RS_CONTROL, 12, 0); 
      break;
    case LCD_TM13264CBCG_MODE_AMAX_KDC162A28BXXBB_95x17:
      cycle (RS_CONTROL, 31, 0); 
      break;
  }

  /* Waiting for LCD Power Level Stabilisation. It doesn't say how long you
   * have to wait. 100 ms should be enough */
  mdelay(100);
                                                                
  /* Initial Display Line (Line Address = 0) */
  cycle (RS_CONTROL, CTL_SET_INIT_LINE | 0, 0);

  /* Set Page Address
   * There are 8 pages (rows). Page 0 is the top row on the display, page 7
   * is bottom row. Each page is 8 bits high.  */
  cycle (RS_CONTROL, CTL_SET_PAGE | 0, 0);   // Page = 0 (Page == Row)

  /* Set Column Address (2 byte instrcution)
   * There are 132 columns. Column 0 is the far left on the display, column 131
   * is the far right. */
  cycle (RS_CONTROL, CTL_SET_COL_MSB | 0, 0);   // bits 7:4
  cycle (RS_CONTROL, CTL_SET_COL_LSB | 0, 0);   // bits 3:0

  /* Clear the screen */
  tm13264cbcg_clear_screen();

  /* Display On/Off */
  cycle (RS_CONTROL, CTL_DISPLAY_ON, 0);   // Display On
}

/****************************************************************************
 * Initialise the calgary OLED module
 ****************************************************************************/
static void init_calgary(void)
{
  /* Reigister initialisation as supplied by Intelligent Displays */
  init_registers_calgary(); // supplied by Intelligent Displays

  /* Data sheet says initialisation sequence should be :
   * 1. Apply Vdd (digital voltage rail)
   * 2. Send command: Display OFF
   * 3. Wait > 100 ms
   * 4. Apply Vcc (display 15V rail)
   */

  /* Display OFF */
  cycle (RS_CONTROL, CTL_DISPLAY_OFF, 0);

  /* Wait 100ms, Apply Vcc */
  mdelay(100);
  rutl_regwrite((1 << 5), (0 << 5), GPCUP);     // Disable pullup
  rutl_regwrite((0 << 5), (1 << 5), GPCDAT);    // Low (ON)
  rutl_regwrite((1 << 10), (3 << 10), GPCCON);  // Set as output

  /* Display ON */
  cycle (RS_CONTROL, CTL_DISPLAY_ON, 0);
}

/****************************************************************************
 * Redraw the screen
 ****************************************************************************/
static void redraw_screen(void)
{
  tm13264cbcg_draw_bitmap(sDisplayBuffer);
}

static void test_pattern(void)
{
  int i,j,k;
  for (k=0; k<5; k++)
  {
    tm13264cbcg_clear_screen();
    for (j=0; j<3; j++)
    {
      set_row(j);
      for (i=0; i<graphics_width; i++)
      {
        blit_byte(0xff);
        tm13264cbcg_draw_bitmap(sDisplayBuffer);
        udelay(20000);
      }
    }
  }
  printk(KERN_ERR PFX "NEW Height: %d, Width %d, Top: %d, Left: %d\n", sDisplayBuffer.height,
      sDisplayBuffer.width, sDisplayBuffer.top, sDisplayBuffer.left);
}

/****************************************************************************
 * Module cleanup
 ****************************************************************************/
static void __exit
tm13264cbcg_lcd_exit(void)
{
  if (power_fail_present)
    free_irq (IRQ_EINT1, &tm13264cbcg_power_fail);
  reciva_lcd_exit();
}

/****************************************************************************
 * Sets up GPIO pins
 ****************************************************************************/
static void setup_gpio (void)
{
  switch (lcd_display_mode)
  {
    case LCD_TM13264CBCG_MODE_TM13264CBCG_0:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_1:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_2:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_3:
    case LCD_TM13264CBCG_MODE_SSD0323_128x64:
    case LCD_TM13264CBCG_MODE_CONFIG1009:
    case LCD_TM13264CBCG_MODE_CONFIG985:
    case LCD_TM13264CBCG_MODE_CONFIG983:
      setup_gpio_8bit_interface();
      break;
    case LCD_TM13264CBCG_MODE_AMAX_KDC162A28BXXBB_95x17:
      setup_gpio_1bit_interface();
      break;
  }
}

/****************************************************************************
 * Sets up GPIO pins when using a 1 bit interface
 ****************************************************************************/
static void setup_gpio_1bit_interface(void)
{
  /* RS/A0 - J1-20 - GPB9
   * CS    - J2-1  - GPG3
   * SDA   - J2-5  - GPG6
   * SCL   - J2-7  - GPG7 */

  /* Tell GPIO module which GPIO pins we are using */
  rgpio_register("GPC0 (A0/RS)", acModuleName);
  rgpio_register("GPC2 (BL_ON)", acModuleName);
  rgpio_register("GPC3 (nRESLCD)", acModuleName);
  rgpio_register("GPG3 (CS)", acModuleName);
  rgpio_register("GPG6 (SDA)", acModuleName);
  rgpio_register("GPG7 (SCL)", acModuleName);

  /* RS/A0 (GPC0) */
  rutl_regwrite((0 << 0), (1 << 0), GPCDAT);    // Output low
  rutl_regwrite((1 << 0), (0 << 0), GPCUP);     // Disable pullup
  rutl_regwrite((1 << 0), (3 << 0), GPCCON);    // Set as output

  /* BL_ON (GPC2) */
  rutl_regwrite((1 << 2), (0 << 2), GPCDAT);    // Output high
  rutl_regwrite((1 << 2), (0 << 2), GPCUP);     // Disable pullup
  rutl_regwrite((1 << 4), (3 << 4), GPCCON);    // Set as output

  /* CS (GPG3) */
  rutl_regwrite((1 << 3), (0 << 3), GPGDAT);    // Output high
  rutl_regwrite((1 << 3), (0 << 3), GPGUP);     // Disable pullup
  rutl_regwrite((1 << 6), (3 << 6), GPGCON);    // Set as output

  /* SDA (GPG6) */
  rutl_regwrite((0 << 6), (1 << 6), GPGDAT);    // Output low
  rutl_regwrite((1 << 6), (0 << 6), GPGUP);     // Disable pullup
  rutl_regwrite((1 << 12), (3 << 12), GPGCON);  // Set as output

  /* SCL (GPG7) */
  rutl_regwrite((0 << 7), (1 << 7), GPGDAT);    // Output low
  rutl_regwrite((1 << 7), (0 << 7), GPGUP);     // Disable pullup
  rutl_regwrite((1 << 14), (3 << 14), GPGCON);  // Set as output

  /* nRESET (GPC3) */
  rutl_regwrite((0 << 3), (1 << 3), GPCDAT);    // Output low
  rutl_regwrite((1 << 3), (0 << 3), GPCUP);     // Disable pullup
  rutl_regwrite((1 << 6), (3 << 6), GPCCON);    // Set as output

  /* nReset low */
  mdelay(100);

  rutl_regwrite((1 << 3), (0 << 3), GPCDAT);    // RESETB high (release reset)
}

/****************************************************************************
 * Sets up GPIO pins when using an 8 bit interface
 ****************************************************************************/
static void setup_gpio_8bit_interface(void)
{
  /* Tell GPIO module which GPIO pins we are using */
  rgpio_register("GPB2 GBC0 GPA12 (E)", acModuleName);
  rgpio_register("GPB3 GBC1 GPA13 (RW)", acModuleName);
  rgpio_register("GPB1 GBC4       (CS1B)", acModuleName);
  rgpio_register("GBC0-GPC5, GPC8-GPC15", acModuleName);

  /* LED
     Processor pin = K12 (GPH7). Active high. */
  rutl_regwrite((1 << 7),  (0 << 7), GPHUP);   // Disable pullup
  rutl_regwrite((1 << 14), (3 << 14), GPHCON); // Set as ouput
  tm13264cbcg_driver.set_led(RLED_MENU);       // Menu LED

  /* LDN0 (E - enable pulse)
     This connects to 3 processor pins to allow various alternate functions
     to be used.
     The processor pins are : F4 (GPB2), J4 (GPC0), E15 (GPA12)
     Using C0 for now. */
  /* Set up all pins as inputs */
  rutl_regwrite((0 << 24), (3 << 24), GPACON);
  rutl_regwrite((0 << 4), (3 << 4), GPBCON);
  rutl_regwrite((0 << 0), (3 << 0), GPCCON);
  /* Disable pullups */
  rutl_regwrite((1 << 12), 0, GPAUP);
  rutl_regwrite((1 << 2), 0, GPBUP);
  rutl_regwrite((1 << 0), 0, GPCUP);
  /* Use GPC0 */
  rutl_regwrite((0 << 0), (1 << 0), GPCDAT); // Output low
  rutl_regwrite((1 << 0), (3 << 0), GPCCON); // Set as output

  /* LDN1 (RW - read/write)
     This connects to 3 processor pins to allow various alternate functions
     to be used.
     The processor pins are : G3 (GPB3), J2 (GPC1), E16 (GPA13)
     Using G3 for now. */
  /* Set up unused pins as inputs */
  rutl_regwrite((0 << 26), (3 << 26), GPACON);
  rutl_regwrite((0 << 6), (3 << 6), GPBCON);
  rutl_regwrite((0 << 2), (3 << 2), GPCCON);
  /* Disable pullups */
  rutl_regwrite((1 << 13), 0, GPAUP);
  rutl_regwrite((1 << 3), 0, GPBUP);
  rutl_regwrite((1 << 1), 0, GPCUP);
  /* Use GPC1 */
  /* Write = high on some modules */
  switch (lcd_display_mode)
  {
    /* 8 bit interface */
    case LCD_TM13264CBCG_MODE_TM13264CBCG_0:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_1:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_2:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_3:
    case LCD_TM13264CBCG_MODE_SSD0323_128x64:
    case LCD_TM13264CBCG_MODE_CONFIG1009:
    case LCD_TM13264CBCG_MODE_AMAX_KDC162A28BXXBB_95x17:
    case LCD_TM13264CBCG_MODE_CONFIG983:
      rutl_regwrite((0 << 1), (1 << 1), GPCDAT);  // Output low
      break;
    case LCD_TM13264CBCG_MODE_CONFIG985:
      rutl_regwrite((1 << 1), (0 << 1), GPCDAT);  // Output high
      break;
  }
  rutl_regwrite((1 << 2), (3 << 2), GPCCON);  // Set as output
  
  /* LDN2 (RS) - connected to J6 (GPC2) */
  rutl_regwrite((1 << 2), (0 << 2), GPCUP);   // Disable pullup
  rutl_regwrite((0 << 2), (1 << 2), GPCDAT);  // Output low
  rutl_regwrite((1 << 4), (3 << 4), GPCCON);  // Set as output

  /* LDN3 (RESETB) - connected to K4 (GPC3) */
  rutl_regwrite((1 << 3), (0 << 3), GPCUP);   // Disable pullup
  rutl_regwrite((0 << 3), (1 << 3), GPCDAT);  // Output low
  rutl_regwrite((1 << 6), (3 << 6), GPCCON);  // Set as output

  /* LDN4 (CS1B) - connected to F1 (GPB1) and K2 (GPC4) using F1 */
  /* Set up unused pins as inputs */
  rutl_regwrite((1 << 4), (0 << 4), GPCUP);   // Disable pullup
  rutl_regwrite((1 << 1), (0 << 1), GPBUP);   // Disable pullup
  rutl_regwrite((0 << 2), (3 << 2), GPBCON);  // Set as input
  rutl_regwrite((0 << 4), (1 << 4), GPCDAT);  // Output low
  rutl_regwrite((1 << 8), (3 << 8), GPCCON);  // Set as output

  /* LCD Data - LCDD0 - LCDD7 (GPC8 to GPC15) */
  rutl_regwrite(((1 << 8) | (1 << 9) | (1 << 10) | (1 << 11)|
                 (1 << 12) | (1 << 13) | (1 << 14) | (1 << 15)),
                 0,
                 GPCUP);   // Disable pullups
  rutl_regwrite( 0,
                 ((1 << 8) | (1 << 9) | (1 << 10) | (1 << 11)|
                 (1 << 12) | (1 << 13) | (1 << 14) | (1 << 15)),
                 GPCDAT);   // Set outputs low
  rutl_regwrite( ((1 << 16) | (1 << 18) | (1 << 20) | (1 << 22) |
                 (1 << 24) | (1 << 26) | (1 << 28) | (1 << 30) ),
                 ((3 << 16) | (3 << 18) | (3 << 20) | (3 << 22) |
                 (3 << 24) | (3 << 26) | (3 << 28) | (3 << 30) ),
                 GPCCON);   // Set as outputs

  /* Note that for ALTO hardware all LCD gpio output pins should be set
   * low at this point as they affect the power if set high */

  /* LDN5 (POWER) - connected to K6 (GPC5) */
  switch (lcd_display_mode)
  {
    case LCD_TM13264CBCG_MODE_TM13264CBCG_0:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_1:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_2:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_3:
    case LCD_TM13264CBCG_MODE_CONFIG1009:
    case LCD_TM13264CBCG_MODE_CONFIG985:
    case LCD_TM13264CBCG_MODE_AMAX_KDC162A28BXXBB_95x17:
    case LCD_TM13264CBCG_MODE_CONFIG983:
      rutl_regwrite((1 << 5), (0 << 5), GPCUP);     // Disable pullup
      rutl_regwrite((0 << 5), (1 << 5), GPCDAT);    // Output low (active low)
      rutl_regwrite((1 << 10), (3 << 10), GPCCON);  // Set as output
      break;
    case LCD_TM13264CBCG_MODE_SSD0323_128x64:
      /* Need to wait until we have sent a Power OFF command to display
       * before enabling this power (15V) */
      break;
  }

  // LDN4 (CS1B)
  rutl_regwrite(LCD_CS1, 0, RIRM_LCDCN_DAT);  // Output high

  /* Hold RESETB low */
  // XXX how long delay needed we can read a status bit
  mdelay(100);
  rutl_regwrite(LCD_RESET, 0, RIRM_LCDCN_DAT);    // RESETB high (release reset)
}

static void blit_byte(unsigned char data)
{
  set_display_byte(current_column++, current_row, data, sDisplayBuffer);
}

/****************************************************************************
 * Sends data to the module
 ****************************************************************************/
static void send_data(int data)
{
  cycle (RS_DATA, data, 0);
}

/****************************************************************************
 * Sends a command to the module
 ****************************************************************************/
static void send_command(int data)
{
  cycle (RS_CONTROL, data, 0);
}

/****************************************************************************
 * Sends one instruction to the LCD module.
 * iRS - level of RS signal (register select - RS_DATA or RS_CONTROL)
 * iData - data to be written
 * iDelay - delay in microseconds at end of cycle
 ****************************************************************************/
static void cycle (int iRS, int iData, int iDelay)
{
  int i;

  switch (lcd_display_mode)
  {
    /* 8 bit interface */
    case LCD_TM13264CBCG_MODE_TM13264CBCG_0:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_1:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_2:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_3:
    case LCD_TM13264CBCG_MODE_SSD0323_128x64:
    case LCD_TM13264CBCG_MODE_CONFIG1009:
    case LCD_TM13264CBCG_MODE_CONFIG985:
    case LCD_TM13264CBCG_MODE_CONFIG983:
      cycle_8bit (iRS, iData, iDelay); 
      break;

    /* 1 bit interface */
    case LCD_TM13264CBCG_MODE_AMAX_KDC162A28BXXBB_95x17:
      /* CS low */
      rutl_regwrite((0 << 3), (1 << 3), GPGDAT);

      for (i=7; i>=0; i--)
        cycle_1bit (iRS, (iData>>i & 0x01), 0);    // MSB first
    
      /* CS high */
      rutl_regwrite((1 << 3), (0 << 3), GPGDAT);

      udelay (iDelay);
      break;
  }
}

/****************************************************************************
 * Clock one bit of data out to LCD module.
 * iRS - level of RS signal (register select - RS_DATA or RS_CONTROL)
 * iData - data to be written
 * iDelay - delay in microseconds at end of cycle
 ****************************************************************************/
static void cycle_1bit (int iRS, int iData, int iDelay)
{
  /* RS/A0 - GPC0
   * CS    - GPG3
   * SDA   - GPG6
   * SCL   - GPG7 */

  /* Set up RS (A0) */
  if (iRS)
    rutl_regwrite((1 << 0), (0 << 0), GPCDAT); // Set
  else
    rutl_regwrite((0 << 0), (1 << 0), GPCDAT); // Clear

  /* Set up data (SDA) */
  if (iData & 0x01)
    rutl_regwrite((1 << 6), (0 << 6), GPGDAT); // Set
  else
    rutl_regwrite((0 << 6), (1 << 6), GPGDAT); // Clear

  /* SCL high */
  rutl_regwrite((1 << 7), (0 << 7), GPGDAT); // Set

  /* Wait > 60 ns 
   * Don't need an explicit delay here as we can't toggle gpio pins faster 
   * than 125ns */

  /* SCL low */
  rutl_regwrite((0 << 7), (1 << 7), GPGDAT); // Clear
}

/****************************************************************************
 * Sends one instruction to the LCD module.
 * iRS - level of RS signal (register select - RS_DATA or RS_CONTROL)
 * iData - data to be written
 * iDelay - delay in microseconds at end of cycle
 ****************************************************************************/
static void cycle_8bit (int iRS, int iData, int iDelay)
{
  unsigned int temp = __raw_readl(GPCDAT);

  /* Note that LCD module 'owns' all GPC GPIO pins so we don't need to
   * protect against anyone else modifying them under our feet */

  /* GPC8 - GPC15             : LCD module data D0 - D7
   * GPC0 (and GPB2, GPA12)   : (LCDCN0) E (enable pulse)
   * GPC1 (and GPB3, GPA13)   : (LCDCN1) RW (Read/Write)
   * GPC2                     : (LCDCN2) RS (== D/C) (Register Select)
   * GPC3                     : (LCDCN3) RESETB
   * GPC4 (and GPB1)          : (LCDCN4) CS1B
   * GPC5                     : (LCDCN5) POWER */

  /* Set up RS (GPC2) and CS1B (GPC4) low, and set up data */
  temp &= ~(0x0000ff00);
  temp |= (iData  << 8);
  if (iRS == RS_DATA)
    temp |= LCD_RS;   /* RS = '1' */
  else
    temp &= ~LCD_RS;  /* RS = '0' */
  temp &= ~LCD_CS1;    /* CS1B = '0' */
  __raw_writel (temp, RIRM_LCDCN_DAT);

  /* Wait 3us (data sheet specifies 17 ns, ALTO hardware need 3 us)
   * Note : the fastest we can toggle GPIO bits is approx 125ns 
   * (measured on scope 20050305) */
  switch (lcd_display_mode)
  {
    case LCD_TM13264CBCG_MODE_TM13264CBCG_0:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_1:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_2:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_3:
    case LCD_TM13264CBCG_MODE_CONFIG1009:
    case LCD_TM13264CBCG_MODE_CONFIG985:
    case LCD_TM13264CBCG_MODE_AMAX_KDC162A28BXXBB_95x17:
    case LCD_TM13264CBCG_MODE_CONFIG983:
      udelay(3);
      break;
    case LCD_TM13264CBCG_MODE_SSD0323_128x64:
      break;
  }
  
  /* Set E high and set up data */
  temp |= LCD_E;  /* E = '1' */
  __raw_writel (temp, RIRM_LCDCN_DAT);

  /* Wait 250 ns (data sheet specifies 55 ns, ALTO hardware needs 250ns) */
  switch (lcd_display_mode)
  {
    case LCD_TM13264CBCG_MODE_TM13264CBCG_0:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_1:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_2:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_3:
    case LCD_TM13264CBCG_MODE_CONFIG1009:
    case LCD_TM13264CBCG_MODE_CONFIG985:
    case LCD_TM13264CBCG_MODE_AMAX_KDC162A28BXXBB_95x17:
    case LCD_TM13264CBCG_MODE_CONFIG983:
      ndelay(250);
      break;
    case LCD_TM13264CBCG_MODE_SSD0323_128x64:
      ndelay(50);
      break;
  }

  /* Set E low, CS1B high */
  temp &= ~LCD_E;  /* E = '0' */
  temp |= LCD_CS1;  /* CS1B = '1' */
  __raw_writel (temp, RIRM_LCDCN_DAT);

  /* Data must still be valid for 13ns after setting E low
   * Again, don't need an explicit delay here as next GPIO write will
   * take longer than this */

  udelay (iDelay);
}

/****************************************************************************
 * Read from LCD
 * iRS - level of RS signal (register select - RS_DATA or RS_CONTROL)
 * iData - data to be written
 * iDelay - delay in microseconds at end of cycle
 ****************************************************************************/
#if ENABLE_READ
static int read_8bit (int iRS, int iDelay)
{
  /* LCD Data - LCDD0 - LCDD7 (GPC8 to GPC15) */

  /* Note that LCD module 'owns' all GPC GPIO pins so we don't need to
   * protect against anyone else modifying them under our feet */

  /* GPC8 - GPC15             : LCD module data D0 - D7
   * GPC0 (and GPB2, GPA12)   : (LCDCN0) E (enable pulse)
   * GPC1 (and GPB3, GPA13)   : (LCDCN1) RW (Read/Write)
   * GPC2                     : (LCDCN2) RS (== D/C) (Register Select)
   * GPC3                     : (LCDCN3) RESETB
   * GPC4 (and GPB1)          : (LCDCN4) CS1B
   * GPC5                     : (LCDCN5) POWER */


  unsigned int temp = __raw_readl(GPCDAT);

  /* Set D0-D7 as inputs */
  rutl_regwrite( ((0 << 16) | (0 << 18) | (0 << 20) | (0 << 22) |
                 (0 << 24) | (0 << 26) | (0 << 28) | (0 << 30) ),
                 ((3 << 16) | (3 << 18) | (3 << 20) | (3 << 22) |
                 (3 << 24) | (3 << 26) | (3 << 28) | (3 << 30) ),
                 GPCCON);

  /* Set up RS (GPC2) and CS1B (GPC4) low */
  if (iRS == RS_DATA)
    temp |= LCD_RS;   /* RS = '1' */
  else
    temp &= ~LCD_RS;  /* RS = '0' */
  temp &= ~LCD_CS1;    /* CS1B = '0' */
  temp |= LCD_RW;   /* RW = '1' (READ) */
  __raw_writel (temp, RIRM_LCDCN_DAT);

  /* Wait 3us (data sheet specifies 17 ns, ALTO hardware need 3 us)
   * Note : the fastest we can toggle GPIO bits is approx 125ns 
   * (measured on scope 20050305) */
  udelay(3);

  /* Set E high and set up data */
  temp |= LCD_E;  /* E = '1' */
  __raw_writel (temp, RIRM_LCDCN_DAT);

  /* Wait 250 ns (data sheet specifies 55 ns, ALTO hardware needs 250ns) */
  ndelay(250);

  /* Read the data */
  unsigned int data = __raw_readl(GPCDAT);
  data >>= 8;
  data &= 0xff;

  /* Set E low, CS1B high */
  temp &= ~LCD_E;  /* E = '0' */
  temp |= LCD_CS1;  /* CS1B = '1' */
  __raw_writel (temp, RIRM_LCDCN_DAT);

  /* GPC1 (RW) */
  rutl_regwrite(0, LCD_RW, RIRM_LCDCN_DAT);  // Output low

  /* Set D0-D7 as outputs */
  rutl_regwrite( ((1 << 16) | (1 << 18) | (1 << 20) | (1 << 22) |
                 (1 << 24) | (1 << 26) | (1 << 28) | (1 << 30) ),
                 ((3 << 16) | (3 << 18) | (3 << 20) | (3 << 22) |
                 (3 << 24) | (3 << 26) | (3 << 28) | (3 << 30) ),
                 GPCCON);

  udelay (iDelay);

  return data;
}
#endif

/****************************************************************************
 * Turns power off
 ****************************************************************************/
static void tm13264cbcg_power_off()
{
}

/****************************************************************************
 * Draws a screens worth of text of the display.
 * acText  - text to be displayed
 * iX,iY - cursor position
 * iCursorType - indicate if cursor should be drawn
 * piArrows - which arrows should be shown
 * piLineContents - indicates text/barcode for each line
 *
 * Returns 0 on success or < 0 on failure
 ****************************************************************************/
static int tm13264cbcg_draw_screen(char **acText, int iX, int iY,
                                   int iCursorType, int *piArrows,
                                   int *piLineContents)
{
  int iLine;
  int ret = 0;
  InvertStatus_t tInverted = NOT_INVERTED;
  
  for (iLine = 0; iLine < screen_height; iLine++)
  {
    tInverted = NOT_INVERTED;
    switch (piLineContents[iLine])
    {
      case LCD_LINE_CONTENTS_INVERTED_TEXT:
        tInverted = INVERTED;
        /* Intentional fall through */
      case LCD_LINE_CONTENTS_TEXT:
        if ((iCursorType == LCD_CURSOR_ON) && (iLine == iY))
        {
          ret = draw_line(iLine, 
                          acText[iLine], 
                          iX, 
                          LCD_CURSOR_ON, 
                          tInverted, 
                          piArrows[iLine]);
        }
        else
        {
          ret = draw_line(iLine, 
                          acText[iLine], 
                          iX, 
                          LCD_CURSOR_OFF, 
                          tInverted, 
                          piArrows[iLine]);
        }
        break;
      case LCD_LINE_CONTENTS_BARCODE:
        ret = draw_barcode(iLine, acText[iLine]);
        break;
      default:
        ret = -EFAULT;
        break;
    }
  }

  tm13264cbcg_draw_bitmap(sDisplayBuffer);
  
  return ret;
}

/****************************************************************************
 * Sets the row
 ****************************************************************************/
static void set_row(int row)
{
  current_row = row;
}
/****************************************************************************
 * Set the column
 ****************************************************************************/
static void set_column(int col)
{
  current_column = col;
}

/****************************************************************************
 * Clears the screen
 ****************************************************************************/
static void tm13264cbcg_clear_screen(void)
{
  memset(sDisplayBuffer.data, '\0', (sDisplayBuffer.height * sDisplayBuffer.width) / 8 + 1);
  tm13264cbcg_draw_bitmap(sDisplayBuffer);  
}  

/****************************************************************************
 * Set the led state
 * XXX this wrapper function could probably be eliminated if the led code was
 * rewritten more sensibly.
 ****************************************************************************/
static void tm13264cbcg_set_led(unsigned int mask)
{
  reciva_led_set(mask);
}

/****************************************************************************
 * Set the LCD contrast
 ****************************************************************************/
static void tm13264cbcg_set_contrast(int level)
{
  int temp;

  if (level < 0)
    level = 0;
  else if (level > 100)
    level = 100;

  switch (lcd_display_mode)
  {
    case LCD_TM13264CBCG_MODE_TM13264CBCG_0:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_1:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_2:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_3:
    case LCD_TM13264CBCG_MODE_SSD0323_128x64:
    case LCD_TM13264CBCG_MODE_CONFIG1009:
    case LCD_TM13264CBCG_MODE_CONFIG985:
    case LCD_TM13264CBCG_MODE_CONFIG983:
      break;
    case LCD_TM13264CBCG_MODE_AMAX_KDC162A28BXXBB_95x17:
      /* Range is 0x00 to 0x3f */
      temp = (level * 0x3f)/ 100;
      printk(PFX "CONTRAST l=%d t=%d\n", level, temp);
  
      /* Reference Voltage Select (2 byte instruction) */
      cycle (RS_CONTROL, CTL_SET_REF_V, 0); // Select mode
      cycle (RS_CONTROL, temp, 0); 
      break;
  }
}

/****************************************************************************
 * Set lcd zoom status.
 ****************************************************************************/
static void tm13264cbcg_set_zoom(int iOn, struct lcd_resize *psNewSize)
{
  if (zoom_allowed)
  {
    if (iOn)
      zoom_enabled = 1;
    else  
      zoom_enabled = 0;

    update_screen_dimensions();    
  }
    
  psNewSize->iHeight = screen_height;
  psNewSize->iWidth  = screen_width;
  tm13264cbcg_draw_bitmap(sDisplayBuffer);
}

/****************************************************************************
 * Draws a clock digit at the specified position
 ****************************************************************************/
static void draw_clock_digit(int iPosition, int iDigit, int iColumnOffset)
{
  int iRow = 6;
  int iColumn = 0;
  int iInitColumn = iColumnOffset + (iPosition * CLOCK_DIGIT_WIDTH);
  int iData = 0xff;
  int iBlank = (iDigit == -1);

  if (iDigit < 0 || iDigit > 9)
    iDigit = 0;

  if (iPosition > 1)	
    iInitColumn += CLOCK_COLON_WIDTH;

  for (iRow=6; iRow>=3; iRow--)
  {
    set_row(iRow);
    set_column(iInitColumn);

    for (iColumn=0; iColumn<CLOCK_DIGIT_WIDTH; iColumn++)
    {
     iData = iBlank ? 0 : au32Font32x26[iColumn + (iDigit * CLOCK_DIGIT_WIDTH)];
              
      switch (iRow)
      {
        case 6:
          iData >>= 24;
          break;
        case 5:
          iData >>= 16;
          break;
        case 4:
          iData >>= 8;
          break;
        default:  
          break;
      }

      blit_byte( iData);
    }
  }
}  

/****************************************************************************
 * Functions to draw the AM/PM text
 ****************************************************************************/
#define COL_OFFSET_AMPM 112
static void draw_clock_am_pm_16x8(int iDrawAM)
{
  int *piAMPM = (iDrawAM) ? aiAM : aiPM;
  int iAMPMLen = (iDrawAM) ? iAMLen : iPMLen;

  int i,j;
  for (i=0; i<=1; i++)
  {
    set_row(6-i);
    set_column(COL_OFFSET_AMPM);
    
    for (j=0; j<iAMPMLen; j++)
    {
      if (piAMPM[j] & (1 << 31))
      {
        draw_chinese_character_16x16(piAMPM[j] & ~(1 << 31), 0, i);
      }
      else
      {
        draw_character_16x8(piAMPM[j], 0, i);
      }
    }
  }
}  
static void draw_clock_am_pm_8x6(int iDrawAM)
{
  int *piAMPM = (iDrawAM) ? aiAM : aiPM;
  int iAMPMLen = (iDrawAM) ? iAMLen : iPMLen;

  int j;
  set_row(4);
  set_column(COL_OFFSET_AMPM);
    
  for (j=0; j<iAMPMLen; j++)
  {
    draw_character_8x6(piAMPM[j], 0);
  }
}  

/****************************************************************************
 * Utility function to convert to 12 hour clock
 ****************************************************************************/
int iMake12Hour(int iHours)
{
  iHours = iHours % 12;
  if (iHours == 0)
  {
    iHours = 12;
  }
  return iHours;
}

/****************************************************************************
 * Draws the clock (CONFIG0). Time (hours, minutes) in big font with alarm 
 * time in brackets underneath
 * 
 * Alignment of time
 *
 * For 24 hour clock:
 *   4 digits + colon - average gap at end of digit
 *     4*26   +  10   -          6                  = 108
 *   To centre in 24hr format start at line (132-108)/2 = 12
 *
 * For 12 hour clock:
 *   4 digits + colon + AM/PM 
 *     4*26   +  10   +  2*8   = 130
 *
 *   Need to shave off 2 columns so that it fits in 128 lines, so start at
 *   first column and move AM/PM in two cols. (We can do this because the
 *   final columns of the digits are always blank)
 *     Start time at column 0
 *     Start AM/PM at column 112
 *
 * For alarm clock:
 *   Ensure that the colon lines up with that of the main clock
 *   (Therfore need to adjust position when not drawing the first digit in
 *   12 hour mode)
 ****************************************************************************/
#define COL_OFFSET_24HR 12
#define COL_OFFSET_12HR 0
#define COL_OFFSET_ALARM 26
static void draw_clock_config0(struct lcd_draw_clock *psClockInfo,
                               int i12HourFormat)
{
  int iHours = psClockInfo->iHours;
  int iMinutes = psClockInfo->iMinutes;
  int iColumnOffset = COL_OFFSET_24HR;

  if (i12HourFormat)
  {
    iHours = iMake12Hour(iHours);
    iColumnOffset = COL_OFFSET_12HR;
  }

  /* Most significant hours digit */
  if (!i12HourFormat || (iHours > 9))
  {
    draw_clock_digit(0, iHours/10, iColumnOffset);
  }
  else
  {
    draw_clock_digit(0, -1 /*blank digit*/, iColumnOffset);
  }

  /* Least significant hours digit */  
  draw_clock_digit(1, iHours%10, iColumnOffset);

  /* Draw the ':' */
  draw_clock_colon(iColumnOffset);
    
  /* Most significant minutes digit */
  draw_clock_digit(2, iMinutes/10, iColumnOffset);

  /* Least significant hours digit */
  draw_clock_digit(3, iMinutes%10, iColumnOffset);

  /* Draw the AM/PM text */
  if (i12HourFormat)
  {
    draw_clock_am_pm_16x8((psClockInfo->iHours < 12));  
  }

  /* Draw alarm info (if required) */
  if (psClockInfo->iAlarmOn)
  {
    draw_alarm_time(i12HourFormat, iColumnOffset,
                    psClockInfo->iAlarmHours, psClockInfo->iAlarmMinutes);
  }

  tm13264cbcg_draw_bitmap(sDisplayBuffer);
}  

/****************************************************************************
 * Draws the clock (CONFIG1).
 * LCD_CLOCK_CONFIG1 : time in 32x16 font, date in 16x8 font, alarm icon
 ****************************************************************************/
static void draw_clock_config1(struct lcd_draw_clock *psClockInfo,
                               int i12HourFormat)
{
  /* Save screen dimensions.
   * They are modified within the function to make it easier to draw
   * different size fonts */
  int iTopRowOld = iTopRow;
  int icon_size_old = icon_size;
  int screen_height_old = screen_height;
  int screen_width_old = screen_width;
  int font_size_old = font_size;

  /* Set up time string */
  #define TIME_STRING_LENGTH 20
  int iHours = psClockInfo->iHours;
  if (i12HourFormat)
  {
    iHours = iMake12Hour(iHours);
  }
  char time_string[TIME_STRING_LENGTH];
  snprintf(time_string, TIME_STRING_LENGTH, "%2d:%02d", 
                                            iHours, 
                                            psClockInfo->iMinutes);

  /* .. and draw it */
  screen_height = 2;
  iTopRow = 6;
  screen_height = 4;
  screen_width = 8;
  font_size = FONT_SIZE_32x16;
  draw_line(0,                 // iLine
            time_string,       // pcText
            0,                 // iX
            LCD_CURSOR_OFF,    // iCursorType
            NOT_INVERTED,      // tInverted
            LCD_ARROW_NONE);   // iArrows

  /* Draw AM/PM text */
  if (i12HourFormat)
  {
    draw_clock_am_pm_8x6((psClockInfo->iHours < 12));  
  }

  /* Draw date string */
  screen_height = SCREEN_HEIGHT_MODE_1;
  iTopRow = 7;
  screen_height = 3;
  screen_width = 15;
  font_size = FONT_SIZE_16x8;
  draw_line(4,                          // iLine
            psClockInfo->pcDateString,  // pcText
            0,                          // iX
            LCD_CURSOR_OFF,             // iCursorType
            NOT_INVERTED,               // tInverted
            LCD_ARROW_NONE);            // iArrows

  /* Draw alarm info (if required) */
  unsigned int bitmask = psClockInfo->iAlarmOn;
  tm13264cbcg_draw_icons(bitmask); // also calls draw_bitmap

  /* Restore screen dimensions */
  screen_height = screen_height_old;
  iTopRow = iTopRowOld;
  icon_size = icon_size_old;
  screen_height = screen_height_old;
  screen_width = screen_width_old;
  font_size = font_size_old;
}

/****************************************************************************
 * Draws the time on the screen in big font
 ****************************************************************************/
static void tm13264cbcg_draw_clock(struct lcd_draw_clock *psClockInfo)
{
  int i12HourFormat = (psClockInfo->iMode == LCD_DRAW_CLOCK_12HR)
                      && iAMLen && iPMLen;
  switch (clock_config)
  {
    case LCD_CLOCK_CONFIG0:
      draw_clock_config0(psClockInfo, i12HourFormat);
      break;
    case LCD_CLOCK_CONFIG1:
      draw_clock_config1(psClockInfo, i12HourFormat);
      break;
  };
}  

// TODO Could really put this function into it's own IOCTL
//      as the alarm info does not need to be updated as often
//      as the clock
/****************************************************************************
 * Draws the alarm time at the bottom of the screen
 ****************************************************************************/
static void draw_alarm_time(int i12HourFormat, int iColumnOffset,
                            int iAlarmHours, int iAlarmMinutes)
{
  char acTemp[20];
  int *piAMPM = NULL;
  int iAMPMLen = 0;
  int i,j;

  /* Set up the alarm string */
  if (!i12HourFormat)
  {
    sprintf(acTemp, "(%02d:%02d)", iAlarmHours, iAlarmMinutes);
  }
  else
  {
    piAMPM = (iAlarmHours < 12) ? aiAM : aiPM;
    iAMPMLen = (iAlarmHours < 12) ? iAMLen : iPMLen;
    int iAlarmHours_12 = iMake12Hour(iAlarmHours);
    if (iAlarmHours_12 < 10)
    {
      iColumnOffset += 8; // adjust positioning so colons are aligned
    }
    sprintf(acTemp, "(%d:%02d ", iAlarmHours_12, iAlarmMinutes);
  }

  /* .. and draw it */
  for (i=0; i<=1; i++)
  {
    int iTimeLen = strlen(acTemp);
    set_row(1-i);
    set_column(COL_OFFSET_ALARM+iColumnOffset); // ensure colons are alligned
    
    /* UTF8-clean as long as numbers and : are in the same place! */
    for (j=0;j<iTimeLen; j++)
    {
      draw_character_16x8(acTemp[j], 0, i);
    }
    if (i12HourFormat)
    {
      for (j=0; j<iAMPMLen; j++)
      {
        if (piAMPM[j] & (1 << 31))
        {
          draw_chinese_character_16x16(piAMPM[j] & ~(1 << 31), 0, i);
        }
        else
        {
          draw_character_16x8(piAMPM[j], 0, i);
        }
      }
      draw_character_16x8(')', 0, i);
    }
  }
}

static int tm13264cbcg_set_ampm_text(struct lcd_ampm_text *ampm_text)
{
  char *pcText = NULL;
  int ret = get_text_from_user(&pcText, ampm_text->pcAM);
  if (ret)
  {
    printk(KERN_ERR PFX "Failed to get AM text - %d\n", ret);
    return ret;
  }
  printk("AM text - %s\n", pcText);
  (void)iDoCharLookupOnString(pcText, aiAM, AMPM_LENGTH, &iAMLen);

  ret = get_text_from_user(&pcText, ampm_text->pcPM);
  if (ret)
  {
    printk(KERN_ERR PFX "Failed to get PM text - %d\n", ret);
    return ret;
  }
  printk(PFX "PM text - %s\n", pcText);
  (void)iDoCharLookupOnString(pcText, aiPM, AMPM_LENGTH, &iPMLen);
  kfree(pcText);

  return 0;
}

/****************************************************************************
 * Does the lookup on the input UTF8 string and converts in into characters
 * in the current font.
 * This function will return once it runs out of screen width
 * 
 * Input:
 *  pcText       - input string
 *  piLocalRep   - resultant array of characters in local font
 *  iWidth       - screen width in characters
 *  piCharLength - set to the number of characters in the local string
 * Returns:
 *  Number of screen characters required by string
 ****************************************************************************/
static int iDoCharLookupOnString(const char *pcText, int *piLocalRep,
                                 int iWidth, int *piCharLength)
{
  rutl_utf8_seq_info sUnicodeChar;
  int iScreenLength = 0;  // length in terms of displayed characters on screen
  *piCharLength = 0;      // length in characters (i.e. in array)
  sUnicodeChar.pcSeq = pcText;

  while ((*(sUnicodeChar.pcSeq) != '\0') && (iScreenLength < iWidth))
  {
    if (rutl_utf8_to_unicode(&sUnicodeChar) < 0)
    {
      /* If we get an invalid UTF-8 string, give up and just display
       * what we've got so far */
      printk(KERN_ERR PFX "Unicode lookup failed\n");
      break;
    }

    if (rfont_ischinese(sUnicodeChar.iUnicode))
    {
      // Chinese character - check we have enough room for it
      if ((iScreenLength + 2) > iWidth)
      {
        break;
      }

      // Add character marking to use the Chinese font
      piLocalRep[(*piCharLength)++] = (sUnicodeChar.iUnicode | (1 << 31));
      iScreenLength += 2; /* Chinese characters are double-width */
    }
    else
    {
      // Regular lookup
      piLocalRep[(*piCharLength)++] = reciva_lcd_unicode_lookup(&sUnicodeChar);
      iScreenLength++;
    }

    sUnicodeChar.pcSeq = sUnicodeChar.pcNextSeq;
  }

  return iScreenLength;
}

/****************************************************************************
 * Allocates a buffer for a user-land string and copies it over
 * Input:
 *  ppcDest - Pointer to pointer to allocate to string
 *  pcSrc   - Pointer to user-land string
 *
 * Note that this function will try and free *ppcDest if it is non-NULL
 ****************************************************************************/
static int get_text_from_user(char **ppcDest, char *pcSrc)
{
  int iStringLength = strlen_user(pcSrc);
  if (*ppcDest)
  {
    kfree(*ppcDest);
  }

  if (!(*ppcDest = kmalloc(iStringLength + 1, GFP_KERNEL)))
  {
    return -ENOMEM;
  }

  if (copy_from_user(*ppcDest, pcSrc, sizeof(char) * (iStringLength+1)))
  {
    return -EFAULT;
  }

  return 0;
}

/****************************************************************************
 * Draws the colon that seperates the hours and minutes on the clock display
 ****************************************************************************/
static void draw_clock_colon(int iOffset)
{
  int iRow = 6;
  int iColumn = 0;
  int iInitColumn = iOffset + (CLOCK_DIGIT_WIDTH * 2);
  int iData = 0xff;

  for (iRow=6; iRow>=3; iRow--)
  {
    set_row(iRow);
    set_column(iInitColumn);

    for (iColumn=0; iColumn<CLOCK_COLON_WIDTH; iColumn++)
    {
     iData = au32Font32x26[iColumn + (10*CLOCK_DIGIT_WIDTH)+0];
              
      switch (iRow)
      {
        case 6:
          iData >>= 24;
          break;
        case 5:
          iData >>= 16;
          break;
        case 4:
          iData >>= 8;
          break;
        default:  
          break;
      }

      blit_byte( iData);
      
    }
  }
}  

/****************************************************************************
 * Draws one line of text with the current font size
 * iLine - line number
 * pcText - text to draw
 * iX - x coord of cursor
 * iCursorType - indicates if cursor is on or off
 * tInverted - indicates if text should be drawn inverted
 * iArrows - arrow status
 *
 * Returns 0 on success or a negative number on failure.
 * 
 * This will need fixing to handle multi-visual-width characters, but should
 * deal correctly with multibyte UTF-8 characters.
 ****************************************************************************/
static int draw_line(int iLine, 
                     const char *pcText, 
                     int iX, 
                     int iCursorType, 
                     InvertStatus_t tInverted, 
                     int iArrows)
{
  int iRow;
  int aiLocalRep[MAX_SCREEN_WIDTH];
  int iFontHeight;
  int iScreenLength, iCharLength;
  int iBlankHalfSpaces;
  int iCurRow, iCurCol;

  /* Work out the row that we should write to */
  switch (font_size)
  {
    case FONT_SIZE_16x8:
      iFontHeight = 16;
      iRow = iTopRow - (iLine * 2);
      break;
    case FONT_SIZE_32x16:
      iFontHeight = 32;
      iRow = iTopRow - (iLine * 4);
      break;
    case FONT_SIZE_8x6:
      iFontHeight = 8;
      iRow = iLine;
      break;
    default:
      return -EINVAL;
  }

  switch (lcd_display_mode)
  {
    case LCD_TM13264CBCG_MODE_TM13264CBCG_0:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_1:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_2:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_3:
    case LCD_TM13264CBCG_MODE_AMAX_KDC162A28BXXBB_95x17:
    case LCD_TM13264CBCG_MODE_SSD0323_128x64:
    case LCD_TM13264CBCG_MODE_CONFIG985:
    case LCD_TM13264CBCG_MODE_CONFIG983:
      break;
    case LCD_TM13264CBCG_MODE_CONFIG1009:
      iRow = iTopRow - iLine;
      break;
  }
  
  /* Line is split up into either 1, 2 or 4 rows.
   * Each row is 8 pixels high.
   * Draw one complete row 8 pixels high at a time.
   * Start from top row and work down towards bottom row. */

  /* Looking up characters in the utf8 table is moderately expensive, so do
   * that and all our other one-off computations once here rather than for
   * each row */
  iScreenLength = iDoCharLookupOnString(pcText, aiLocalRep,
                                        screen_width, &iCharLength);

  /* Work out how many blank spaces are required to centre the text */
  if (iScreenLength < screen_width)
    iBlankHalfSpaces = screen_width - iScreenLength;
  else
    iBlankHalfSpaces = 0;

  /* Don't have enough space for 6 pixel wide arrows on 95*17 displays */
  int iArrowWidth = ARROW_WIDTH;
  switch (lcd_display_mode)
  {
    case LCD_TM13264CBCG_MODE_TM13264CBCG_0:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_1:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_2:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_3:
    case LCD_TM13264CBCG_MODE_CONFIG1009:
    case LCD_TM13264CBCG_MODE_CONFIG985:
    case LCD_TM13264CBCG_MODE_CONFIG983:
      break;
    case LCD_TM13264CBCG_MODE_AMAX_KDC162A28BXXBB_95x17:
      iArrowWidth = 5;
      break;
    case LCD_TM13264CBCG_MODE_SSD0323_128x64:
      iArrowWidth = 4;
      break;
  }

  for (iCurRow = 0; iCurRow < iFontHeight/8; iCurRow++)
  {
    /* Set up initial row and column */
    set_row(iRow - iCurRow);
    set_column(0);

    /* Draw the Left arrow */
    switch (lcd_display_mode)
    {
      case LCD_TM13264CBCG_MODE_TM13264CBCG_0:
      case LCD_TM13264CBCG_MODE_TM13264CBCG_1:
      case LCD_TM13264CBCG_MODE_TM13264CBCG_2:
      case LCD_TM13264CBCG_MODE_TM13264CBCG_3:
      case LCD_TM13264CBCG_MODE_CONFIG1009:
      case LCD_TM13264CBCG_MODE_CONFIG985:
      case LCD_TM13264CBCG_MODE_CONFIG983:
      case LCD_TM13264CBCG_MODE_AMAX_KDC162A28BXXBB_95x17:
        if (iFontHeight == 16 || iFontHeight == 8)
        {  
          /* On 132 pixel wide displays : first 6 columns reserved for arrows */
          /* On 95 pixel wide displays : first 5 columns reserved for arrows */
          if ((iArrows == LCD_ARROW_LEFT || iArrows == LCD_ARROW_BOTH) && 
              tInverted == NOT_INVERTED)
          {  
            if (iFontHeight == 16)
            {
              draw_character(au16LeftArrowLookup, iArrowWidth, tInverted, iCurRow);
            }
            else
            {
              /* This assumes were on a 95*17 display - don't have enough room 
               * for 6 pixels */
              draw_character_8x5('<', tInverted);
            }
          }
          else
          {  
            draw_blank_columns8x1(iArrowWidth, tInverted);
          }
        }
        break;
      case LCD_TM13264CBCG_MODE_SSD0323_128x64:
        draw_blank_columns8x1(iArrowWidth, tInverted);
        break;
    }

    /* Now draw the text, padded with spaces on either side if necessary */
    draw_half_spaces(iBlankHalfSpaces, tInverted);
    for (iCurCol = 0; iCurCol < iCharLength; ++iCurCol)
    {
      /* Work out if we need to invert the character */
      int iInvert = 0;
      if (tInverted == INVERTED)
        iInvert = 1;
      if (iCursorType == LCD_CURSOR_ON)
      {
        if (iCurCol == iX)
          iInvert ^= 1;
      }
      
      if (aiLocalRep[iCurCol] & (1 << 31))
      {
        /* It's chinese text */
        if (iFontHeight == 8)
        {
          // Chinese text not supported
        }
        else if (iFontHeight == 16)
        {
          draw_chinese_character_16x16(aiLocalRep[iCurCol] & ~(1 << 31),
                                       iInvert, iCurRow);
        }
        else
        {
          draw_chinese_character_32x32(aiLocalRep[iCurCol] & ~(1 << 31), 
                                       iInvert, iCurRow);
        }
      }
      else
      {
        if (iFontHeight == 8)
          draw_character_8x6 (aiLocalRep[iCurCol], iInvert);
        else if (iFontHeight == 16)
          draw_character_16x8 (aiLocalRep[iCurCol], iInvert, iCurRow);
        else
          draw_character_32x16(aiLocalRep[iCurCol], iInvert, iCurRow);
      }
    }
    draw_half_spaces(iBlankHalfSpaces, tInverted);

    /* Draw the Right arrow */
    switch (lcd_display_mode)
    {
      case LCD_TM13264CBCG_MODE_CONFIG1009:
        draw_blank_columns8x1(2, tInverted);
        /* Intentional fall through */
      case LCD_TM13264CBCG_MODE_TM13264CBCG_0:
      case LCD_TM13264CBCG_MODE_TM13264CBCG_1:
      case LCD_TM13264CBCG_MODE_TM13264CBCG_2:
      case LCD_TM13264CBCG_MODE_TM13264CBCG_3:
      case LCD_TM13264CBCG_MODE_AMAX_KDC162A28BXXBB_95x17:
      case LCD_TM13264CBCG_MODE_CONFIG985:
      case LCD_TM13264CBCG_MODE_CONFIG983:
        if (iFontHeight == 16 || iFontHeight == 8)
        {  
          /* On 132 pixel wide displays: last 6 columns reserved for arrows */
          /* On 95 pixel wide displays : last 5 columns reserved for arrows */
          if ((iArrows == LCD_ARROW_RIGHT || iArrows == LCD_ARROW_BOTH) && 
              tInverted == NOT_INVERTED)
          {    
            if (iFontHeight == 16)
            {  
              draw_character(au16RightArrowLookup, iArrowWidth, tInverted, iCurRow);
            }
            else
            {
              /* This assumes were on a 95*17 display - don't have enough room 
               * for 6 pixels */
              draw_character_8x5('>', tInverted);
            }
          }
          else
          {  
            draw_blank_columns8x1(iArrowWidth, tInverted);
          }
        }
        break;
      case LCD_TM13264CBCG_MODE_SSD0323_128x64:
        draw_blank_columns8x1(iArrowWidth, tInverted);
        break;
    }

    /* Draw any blank columns */
    switch (lcd_display_mode)
    {
      case LCD_TM13264CBCG_MODE_TM13264CBCG_0:
      case LCD_TM13264CBCG_MODE_TM13264CBCG_1:
      case LCD_TM13264CBCG_MODE_TM13264CBCG_2:
      case LCD_TM13264CBCG_MODE_TM13264CBCG_3:
      case LCD_TM13264CBCG_MODE_SSD0323_128x64:
      case LCD_TM13264CBCG_MODE_CONFIG1009:
      case LCD_TM13264CBCG_MODE_CONFIG985:
      case LCD_TM13264CBCG_MODE_CONFIG983:
        break;
      case LCD_TM13264CBCG_MODE_AMAX_KDC162A28BXXBB_95x17:
        draw_blank_columns8x1(1, tInverted);
        break;
    }

  }

  return 0;
}

/****************************************************************************
 * Draws a character from the 8x6 font.
 * cData - character to be drawn
 * iInvert - invert character
 ****************************************************************************/
static void draw_character_8x6(char cData, int iInvert)
{
  const unsigned char *pcFontData = &aucFontData_8x6[cData*6];

  int i;
  for (i=0; i < 6; i++)
  {
    int iData = pcFontData[i];
    if (iInvert)
      iData = ~iData;

    /* Get it right way up */    
    switch (lcd_display_mode)
    {
      case LCD_TM13264CBCG_MODE_TM13264CBCG_0:
      case LCD_TM13264CBCG_MODE_SSD0323_128x64:
      case LCD_TM13264CBCG_MODE_TM13264CBCG_1:
      case LCD_TM13264CBCG_MODE_TM13264CBCG_2:
      case LCD_TM13264CBCG_MODE_TM13264CBCG_3:
      case LCD_TM13264CBCG_MODE_CONFIG1009:
      case LCD_TM13264CBCG_MODE_CONFIG985:
      case LCD_TM13264CBCG_MODE_CONFIG983:
        break;
      case LCD_TM13264CBCG_MODE_AMAX_KDC162A28BXXBB_95x17:
        iData = ((iData >> 7) & 0x01) |
                ((iData >> 5) & 0x02) |
                ((iData >> 3) & 0x04) |
                ((iData >> 1) & 0x08) |
                ((iData << 7) & 0x80) |
                ((iData << 5) & 0x40) |
                ((iData << 3) & 0x20) |
                ((iData << 1) & 0x10);
        break;
    }

    blit_byte(iData);
  }
}

/****************************************************************************
 * Draws a character from the 8x6 font, but skips the first line, only 
 * drawing 5 pixels wide
 * cData - character to be drawn
 * iInvert - invert character
 ****************************************************************************/
static void draw_character_8x5(char cData, int iInvert)
{
  const unsigned char *pcFontData = &aucFontData_8x6[cData*6];

  int i;
  for (i=1; i < 6; i++)
  {
    int iData = pcFontData[i];
    if (iInvert)
      iData = ~iData;

    blit_byte( iData);
  }
}

/****************************************************************************
 * Draws a character from the main font.
 * cData - character to be drawn
 * See draw_character for more details
 ****************************************************************************/
static void draw_character_16x8(char cData, int iInvert, int iBottomHalf)
{
  draw_character(&au16FontData[cData * 8], 8, iInvert, iBottomHalf);
}

/****************************************************************************
 * Draws a character character from the Chinese font.
 * iData - character to be drawn
 * See draw_character for more details
 ****************************************************************************/
static void draw_chinese_character_16x16(int iData, int iInvert, int iBottomHalf)
{
  draw_character(rfont_lookup_chinese(iData), 16, iInvert, iBottomHalf);
}

/****************************************************************************
 * Draws a character. The page and column must have been set up correctly
 * before calling this.
 * pFontData - pointer to data for character to be drawn
 * iWidth - character width
 * iInvert - invert character
 * iBottomHalf - 1 == draw the bottom half of the character, 0 == top half
 ****************************************************************************/
static void draw_character(const unsigned short *pFontData, int iWidth,
                           int iInvert, int iBottomHalf)
{
  int i;

  for (i=0; i < iWidth; i++)
  {
    int iData;
    if (iBottomHalf)
    {
      iData = pFontData[i] & 0xff;
    }
    else
    {
      iData = pFontData[i] >> 8;
    }

    if (iInvert)
      iData = ~iData;

    blit_byte( iData);
  }
}

/****************************************************************************
 * Draws the icons
 ****************************************************************************/
static void tm13264cbcg_draw_icons(unsigned int uiBitmask)
{
  /* Not allowed to draw the icons in certain modes */
  if (allowed_to_draw_icons())  
  {  
    if (icon_size == 16)  
      draw_icons_16x16(uiBitmask);
    else      
      draw_icons_8x8(uiBitmask);
  }
  tm13264cbcg_draw_bitmap(sDisplayBuffer);
}

/****************************************************************************
 * Returns the device capabilities
 ****************************************************************************/
static int tm13264cbcg_get_capabilities(void)
{
  return iCapabilities;
}

/****************************************************************************
 * Set the display mode
 ****************************************************************************/
static void tm13264cbcg_set_display_mode(int iMode)
{
  printk(PFX "%s lcd_display_mode old mode%d new mode %d\n",
              __FUNCTION__, lcd_display_mode, iMode);
  lcd_display_mode = iMode;

  /* Determine 
   * 1. if zoom is allowed
   * 2. if the clock can be displayed */
  zoom_allowed = 0;
  switch (lcd_display_mode)
  {
    case LCD_TM13264CBCG_MODE_TM13264CBCG_0:
      zoom_allowed = 1;
      break;
    case LCD_TM13264CBCG_MODE_SSD0323_128x64:
      zoom_allowed = 1;
      break;
    case LCD_TM13264CBCG_MODE_TM13264CBCG_1:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_2:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_3:
    case LCD_TM13264CBCG_MODE_CONFIG1009:
    case LCD_TM13264CBCG_MODE_CONFIG985:
    case LCD_TM13264CBCG_MODE_CONFIG983:
      break;
    case LCD_TM13264CBCG_MODE_AMAX_KDC162A28BXXBB_95x17:
      /* We can only display the big clock on 132 * 65 displays */
      tm13264cbcg_driver.draw_clock = NULL;
      break;
  }

  update_screen_dimensions();
  tm13264cbcg_draw_bitmap(sDisplayBuffer);
}

/****************************************************************************
 * Number of icons on screen
 * Signal strength starts at 102, which leaves space for (102-2)/14 = 7 icons
 *
 * There are currently 8 icons, but IRADIO and MEDIA are mutally exclusive,
 * so we can display everything at the moment.
 ****************************************************************************/
#define MAX_ICONS 7

/****************************************************************************
 * Draws the 16x16 icons on the bottom left of the display
 ****************************************************************************/
static void draw_icons_16x16(unsigned int uiBitmask)
{
  int iRow;
  int iTopHalf;
  int iIconCount = 0;
      
  for (iRow=0; iRow<=1; iRow++)
  {
    set_row(iRow);
    set_column(2);
    iTopHalf = iRow;
    iIconCount = 0;
        
    if (uiBitmask & LCD_BITMASK_SHIFT)
    {  
      draw_icon_16x16(1, iTopHalf);
      iIconCount++;
    }
    if (uiBitmask & LCD_BITMASK_IRADIO)
    {  
      draw_icon_16x16(2, iTopHalf);
      iIconCount++;
    }
    if (uiBitmask & LCD_BITMASK_MEDIA)
    {  
      draw_icon_16x16(3, iTopHalf);
      iIconCount++;
    }
    if (uiBitmask & LCD_BITMASK_SHUFFLE)
    {  
      draw_icon_16x16(4, iTopHalf);
      iIconCount++;
    }
    if (uiBitmask & LCD_BITMASK_REPEAT)
    {  
      draw_icon_16x16(5, iTopHalf);
      iIconCount++;
    }
    if (uiBitmask & LCD_BITMASK_SLEEP_TIMER)
    {  
      draw_icon_16x16(6, iTopHalf);
      iIconCount++;
    }
    if (uiBitmask & LCD_BITMASK_MUTE)
    {  
      draw_icon_16x16(7, iTopHalf);
      iIconCount++;
    }
    if (uiBitmask & LCD_BITMASK_ALARM)
    {  
      draw_icon_16x16(8, iTopHalf);
      iIconCount++;
    }
    if (uiBitmask & LCD_BITMASK_SNOOZE)
    {  
      draw_icon_16x16(9, iTopHalf);
      iIconCount++;
    }
  
    /* Make sure we blank any remaining Icon positions */
    for (; iIconCount<MAX_ICONS; iIconCount++)
      draw_icon_16x16(0, iTopHalf);
  }           
}

/****************************************************************************
 * Draws an Icon. The page and column must have been set up correctly
 * before calling this.
 ****************************************************************************/
static void draw_icon_16x16(int iIcon, int iTopHalf)
{
  const unsigned short *pFontData = &au16IconData[iIcon*16];
  int i;

  /* Only draw 14 pixels wide */
  for (i=0; i<14; i++)
  {
    int iData;
    if (iTopHalf)
    {
      iData = pFontData[i] >> 8;
    }
    else
    {
      iData = pFontData[i] & 0xff;
    }

    blit_byte( iData);
  }
}

/****************************************************************************
 * Draws the 8x8 icons
 ****************************************************************************/
static void draw_icons_8x8(unsigned int uiBitmask)
{
  int iIconCount = 0;
      
  set_column(2);
  if (icon_position == ICON_POSITION_BOTTOM)
    set_row(0);
  else
    set_row(7);
  iIconCount = 0;
      
  if (uiBitmask & LCD_BITMASK_SHIFT)
  {  
    draw_icon_8x8(1);
    iIconCount++;
  }
  if (uiBitmask & LCD_BITMASK_IRADIO)
  {  
    draw_icon_8x8(2);
    iIconCount++;
  }
  if (uiBitmask & LCD_BITMASK_MEDIA)
  {  
    draw_icon_8x8(3);
    iIconCount++;
  }
  if (uiBitmask & LCD_BITMASK_SHUFFLE)
  {  
    draw_icon_8x8(4);
    iIconCount++;
  }
  if (uiBitmask & LCD_BITMASK_REPEAT)
  {  
    draw_icon_8x8(5);
    iIconCount++;
  }
  if (uiBitmask & LCD_BITMASK_SLEEP_TIMER)
  {  
    draw_icon_8x8(6);
    iIconCount++;
  }
  if (uiBitmask & LCD_BITMASK_MUTE)
  {  
    draw_icon_8x8(7);
    iIconCount++;
  }
  if (uiBitmask & LCD_BITMASK_ALARM)
  {  
    draw_icon_8x8(8);
    iIconCount++;
  }
  if (uiBitmask & LCD_BITMASK_SNOOZE)
  {  
    draw_icon_8x8(9);
    iIconCount++;
  }

  /* Make sure we blank any remaining Icon positions */
  for (; iIconCount<MAX_ICONS; iIconCount++)
    draw_icon_8x8(0);

  /* Now draw the a horizontal line to separate the icons from the menu */
  set_column(0);
  if (icon_position == ICON_POSITION_BOTTOM)
    set_row(1);
  else
    set_row(6);
  
  int i;
  for (i=0; i<132; i+=2)  
  {  
    blit_byte( 0x10);
    blit_byte( 0x00);
  }
}

/****************************************************************************
 * Draws an Icon (8x8). The page and column must have been set up correctly
 * before calling this.
 ****************************************************************************/
static void draw_icon_8x8(int iIcon)
{
  const unsigned char *pFontData = &au8IconData8x8[iIcon*8];
  int i;

  for (i=0; i<8; i++)
  {
    int iData = pFontData[i];
    blit_byte( iData);
  }

  /* Draw 2 blank columns just to space them out a bit */
  for (i=0; i<2; i++)
    blit_byte( 0);
}

/****************************************************************************
 * Draws a zoomed (32x16) character from the main font.
 * cData - character to be drawn
 * See draw_zoomed_character for more details
 ****************************************************************************/
static void draw_character_32x16(char cData, int iInvert, int iSection)
{
  draw_zoomed_character(&au16FontData[cData * 8], 8, iInvert, iSection);
}

/****************************************************************************
 * Draws a zoomed (32x32) character from the chinese font.
 * iData - character to be drawn
 * See draw_zoomed_character for more details
 ****************************************************************************/
static void draw_chinese_character_32x32(int iData, int iInvert, int iSection)
{
  draw_zoomed_character(rfont_lookup_chinese(iData), 16, iInvert, iSection);
}

/****************************************************************************
 * Draws a character. The page and column must have been set up correctly
 * before calling this.
 * pFontData - pointer to data for character to be drawn
 * iWidth - character width
 * iInvert - invert character
 * iSection - indicates the section of the character that needs to be drawn
 * for 32x16 font there are 4 sections
 ****************************************************************************/
static void draw_zoomed_character(const unsigned short *pFontData, int iWidth,
                                  int iInvert, int iSection)
{
  int i;
  int iData4Bit;
  int iData8Bit;

  for (i=0; i < iWidth; i++)
  {
    /* We need to stretch the 16x8 font data so we can use it for 32x16 font */
    switch (iSection)
    {
      case 0:
        iData4Bit = (pFontData[i] >> 12) & 0x000f;
        break;
      case 1:
        iData4Bit = (pFontData[i] >> 8) & 0x000f;
        break;
      case 2:
        iData4Bit = (pFontData[i] >> 4) & 0x000f;
        break;
      case 3:
      default:
        iData4Bit = pFontData[i] & 0x000f;
        break;
    }
    iData8Bit = stretch4to8(iData4Bit);

    if (iInvert)
      iData8Bit = ~iData8Bit;

    /* We're stretching the width as well - draw 2 identical columns */
    blit_byte( iData8Bit);
    blit_byte( iData8Bit);
  }
}

/****************************************************************************
 * Draws specified number of half spaces. The number of pixels to draw is
 * calculated based on the current font size
 * The page and column must have been set up correctly before calling this.
 * iCount - number of half spaces to draw
 * tInverted - indicates if it should be inverted
 ****************************************************************************/
static void draw_half_spaces(int iCount, InvertStatus_t tInverted)
{
  int iWidth = 8;
  switch (font_size)
  {
    case FONT_SIZE_16x8:  iWidth = 4; break;
    case FONT_SIZE_32x16: iWidth = 8; break;
    case FONT_SIZE_8x6:   iWidth = 3; break;
  }
        
  while (iCount > 0)
  {
    draw_blank_columns8x1(iWidth, tInverted);
    iCount--;
  }
}

/****************************************************************************
 * Draws the specified number of blank columns (8 pixels high, 1 pixel wide)
 * iCount = number of columns to draw
 * tInvertStatus - indicates if column should be inverted
 ****************************************************************************/
static void draw_blank_columns8x1(int iCount, InvertStatus_t tInvertStatus)
{
  int iData = 0;  
  if (tInvertStatus == INVERTED) 
    iData = 0xff;
  
  while (iCount > 0)
  {
    blit_byte(iData);
    iCount--;
  }
}

/****************************************************************************
 * Draws a signal strength meter
 * iLevel - signal strength 0 to 100. -1 == don't display bar
 ****************************************************************************/
static void tm13264cbcg_draw_signal_strength(int iLevel)
{
  int i;
  int iBars;
  int iOn = 1;
  
  /* No icons == no signal strength meter */
  if (allowed_to_draw_icons() == 0)  
    return;  
  
  if (iLevel < 0)
    iOn = 0;

  if (iLevel > 100)
    iLevel = 100;
  iBars = (iLevel * (SIG_STRENGTH_BARS+1))/100;
  if (iBars > SIG_STRENGTH_BARS)
    iBars = SIG_STRENGTH_BARS;
  
  set_column(graphics_width-30);
  if (icon_position == ICON_POSITION_BOTTOM)
    set_row(0);
  else
    set_row(7);

  for (i=0; i<=SIG_STRENGTH_BARS; i++)
  {
    if (iOn)
    {
      if (iBars >= i)
        draw_8x6_character(&au8SignalStrength8x6[i]);
      else
        draw_8x6_character(&au8Space8x6[0]);
    }
    else
      draw_8x6_character(&au8Space8x6[0]);
  }
  tm13264cbcg_draw_bitmap(sDisplayBuffer);
}  

static unsigned char read_byte(void *data, int data_len_bytes, int offset, int count)
{
  if (count > 8)
  {
    return 0x00;
  }

  if (offset % 8 == 0)
  {
    unsigned char result = ((unsigned char *)data)[offset/8] & (0xFF >> (8 - count));
    return result;
  }

  int byteoffset = offset / 8;

  unsigned char startbyte = ((unsigned char *)data)[byteoffset];
  unsigned char nextbyte = (byteoffset < data_len_bytes ? ((unsigned char *)data)[byteoffset + 1] : 0x00);
  unsigned short halfword = nextbyte << 8 | startbyte;
  unsigned short mask = (0xFF >> (8 - count)) << (offset % 8);
  unsigned char result = (unsigned char)((halfword & mask) >> (offset % 8));
  return result;
}

static struct bitmap_data pad_bitmap(struct bitmap_data info)
{
  void *paddedbuffer;
  struct bitmap_data result;
  int newtop = info.top;
  int newheight = info.height;
  int inputdatalen = ((info.height * info.width)/8) + 2;
  int shift, x;
  if (info.top % 8 == 0 && info.height % 8 == 0)
  {
    // Already aligned
    return info;
  }
  newtop = info.top - (info.top % 8);
  shift = info.top - newtop;
  newheight = info.height + shift;
  if ((info.height + shift) % 8 != 0)
  {
    newheight = newheight + (8 - ((info.height + shift) % 8));
  }
  paddedbuffer = kmalloc(((info.width * newheight) / 8) + 1, GFP_KERNEL);
  memset(paddedbuffer, '\0', ((info.width * newheight) / 8 ) + 1);
  result.left = info.left;
  result.width = info.width;
  result.height = newheight;
  result.top = newtop;
  result.data = paddedbuffer;
  for (x = 0; x<info.width; x++)
  {
    int offset = x * info.height;
    int targetbyteoffset = (x * result.height) / 8;
    if (info.height + shift < 8)
    {
      ((unsigned char *)result.data)[targetbyteoffset] = read_byte(info.data, inputdatalen, offset, info.height);
      ((unsigned char *)result.data)[targetbyteoffset] <<= (8 - (info.top % 8));
    }
    else
    {
      int bitsread = 0;
      ((unsigned char *)result.data)[targetbyteoffset] = read_byte(info.data, inputdatalen, offset, 8 - shift);
      ((unsigned char *)result.data)[targetbyteoffset] <<= shift;
      targetbyteoffset++;
      bitsread = 8 - shift;
      while (bitsread < (info.height - 8))
      {
        ((unsigned char *)result.data)[targetbyteoffset++] = read_byte(info.data, inputdatalen, offset + bitsread, 8);
        bitsread += 8;
      }
      ((unsigned char *)result.data)[targetbyteoffset] = read_byte(info.data, inputdatalen, offset + bitsread, info.height - bitsread);
    }
  }
  return result;
}

static struct bitmap_data tm13264cbcg_grab_screen_region(int left, int top, int width, int height)
{
  struct bitmap_data result = new_bitmap(0, 0, graphics_width, graphics_height);
  memcpy(result.data, sDisplayBuffer.data, (graphics_width * graphics_height)/8 + 1);
  return result;
}

/****************************************************************************
 * Mirror flip data
 ****************************************************************************/
static unsigned char flip(unsigned char data)
{
  unsigned char flipped;

  flipped = 
            ((data & 0x01) << 7) |  
            ((data & 0x02) << 5) |  
            ((data & 0x04) << 3) |  
            ((data & 0x08) << 1) |  
            ((data & 0x10) >> 1) |  
            ((data & 0x20) >> 3) |  
            ((data & 0x40) >> 5) |  
            ((data & 0x80) >> 7);

  return flipped;
}

/****************************************************************************
 * Draws a rectangle of data on the screen
 ****************************************************************************/
static void tm13264cbcg_draw_bitmap(struct bitmap_data info)
{
  int rowmin = info.top / 8;
  int rowmax = (info.top + info.height + 7) / 8;
  int rowheight = rowmax - rowmin;
  int k,j,i;
  struct bitmap_data padded = pad_bitmap(info);
  unsigned char data;
  unsigned char temp;
  unsigned char temp2;
  unsigned char temp3;
  char row_lookup1[] = {3,2,1,0,7,6,5,4};
  int q;

  for (j=rowmin; j<rowmax; j++)
  {
    switch (lcd_display_mode)
    {
      case LCD_TM13264CBCG_MODE_TM13264CBCG_0:
      case LCD_TM13264CBCG_MODE_TM13264CBCG_1:
      case LCD_TM13264CBCG_MODE_TM13264CBCG_2:
      case LCD_TM13264CBCG_MODE_TM13264CBCG_3:
      case LCD_TM13264CBCG_MODE_AMAX_KDC162A28BXXBB_95x17:
      case LCD_TM13264CBCG_MODE_CONFIG1009:
        // The rows are numbered the wrong way up conpared to expectations //
        // Set the row here
        cycle (RS_CONTROL, (CTL_SET_PAGE | (j & 0x0f)), 0);   // Page = 0 (Page == Row)
      
        // Set the column here
        cycle (RS_CONTROL, CTL_SET_COL_MSB | ((padded.left >> 4) & 0x0f), 0);   // bits 7:4
        cycle (RS_CONTROL, CTL_SET_COL_LSB | (padded.left & 0x0f), 0);          // bits 3:0
          
        for(k=0; k<info.width; k++)
        {
          data = ((unsigned char *)padded.data)[(k * rowheight) + (j-rowmin)];
          // ... and the pixels are also upsidedown compared to the way they're 
          // loaded in to the bitmap.
          cycle(RS_DATA, data, 0);
        }
        break;
      case LCD_TM13264CBCG_MODE_CONFIG985:
        /* From top of display rows are: 4,5,6,7,0,1,2,3 */
        q = row_lookup1[j];

        // The rows are numbered the wrong way up conpared to expectations //
        // Set the row here
        cycle (RS_CONTROL, (0xb0 | (q & 0x0f)), 0);   // Page = 0 (Page == Row)
      
        // Set the column here
        cycle (RS_CONTROL, 0x10, 0);   // bits 7:4
        cycle (RS_CONTROL, 0x00, 0);          // bits 3:0
          
        for(k=0; k<info.width; k++)
        {
          data = ((unsigned char *)padded.data)[(k * rowheight) + (j-rowmin)];
          // ... and the pixels are also upsidedown compared to the way they're 
          // loaded in to the bitmap.
          data = flip(data);
          cycle(RS_DATA, data, 0);
        }
        break;
      case LCD_TM13264CBCG_MODE_CONFIG983:
        // The rows are numbered the wrong way up conpared to expectations //
        // Set the row here
        cycle (RS_CONTROL, (CTL_SET_PAGE | ((rowmax-1-j) & 0x0f)), 0);   // Page = 0 (Page == Row)
      
        // Set the column here
        cycle (RS_CONTROL, CTL_SET_COL_MSB | ((padded.left >> 4) & 0x0f), 0);   // bits 7:4
        cycle (RS_CONTROL, CTL_SET_COL_LSB | (padded.left & 0x0f), 0);          // bits 3:0
          
        for(k=0; k<info.width; k++)
        {
          data = ((unsigned char *)padded.data)[(k * rowheight) + (j-rowmin)];
          // ... and the pixels are also upsidedown compared to the way they're 
          // loaded in to the bitmap.
          cycle(RS_DATA, flip(data), 0);
        }
        break;
      case LCD_TM13264CBCG_MODE_SSD0323_128x64:
        /* 4 bit greyscale so it's organised differently.
         * Row 0: top row of pixels
         * Row 64: bottom row of pixels 
         * An 8 bit data write will write 2 pixels from left to right */         
        temp = 0x00;
        for (i=0; i<info.height; i++)
        {
          /* Set colum address */
          cycle(RS_CONTROL, 0x15, 0);
          cycle(RS_CONTROL, 0x00, 0);
          cycle(RS_CONTROL, 0x3F, 0);
        
          /* Set row address */
          cycle(RS_CONTROL, 0x75, 0);
          cycle(RS_CONTROL, info.height-i-1, 0);
          cycle(RS_CONTROL, 0x3F, 0);
        
          temp = i & 0x0f;
          temp = temp | (temp << 4);

          for (k=0; k<info.width; k+=2)
          {
            temp = ((unsigned char *)padded.data)[(k * rowheight) + i/8];
            temp2 = ((unsigned char *)padded.data)[((k+1) * rowheight) + i/8];

            temp3 = i % 8;
            data = 0x00;
            if (temp & (1 << temp3))
              data |= 0xf0;
            if (temp2 & (1 << temp3))
              data |= 0x0f;

            cycle(RS_DATA, data, 0); // 2 pixels
          }
        }
        break;
    }
  }

  if (padded.data != info.data)
    kfree(padded.data);
}


static int tm13264cbcg_get_graphics_height(void)
{
  return graphics_height;
}

static int tm13264cbcg_get_graphics_width(void)
{
  return graphics_width;
}

/****************************************************************************
 * Draws a character 8 pixels high, 6 pixels wide
 ****************************************************************************/
static void draw_8x6_character(const FontData8x6_t *ptData)
{
  int i;
  for (i=0; i<6; i++)
  {
    blit_byte(ptData->acData[i]);
  }
}  

/****************************************************************************
 * 'Stretches' a 4 bit value into an 8 bit value
 ****************************************************************************/
static int stretch4to8(int data4bit)
{
  static int lookup[16] =
  {
    /* 0000 -> 00000000 */ 0x00,
    /* 0001 -> 00000011 */ 0x03,
    /* 0010 -> 00001100 */ 0x0c,
    /* 0011 -> 00001111 */ 0x0f,
    /* 0100 -> 00110000 */ 0x30,
    /* 0101 -> 00110011 */ 0x33,
    /* 0110 -> 00111100 */ 0x3c,
    /* 0111 -> 00111111 */ 0x3f,
    /* 1000 -> 11000000 */ 0xc0,
    /* 1001 -> 11000011 */ 0xc3,
    /* 1010 -> 11001100 */ 0xcc,
    /* 1011 -> 11001111 */ 0xcf,
    /* 1100 -> 11110000 */ 0xf0,
    /* 1101 -> 11110011 */ 0xf3,
    /* 1110 -> 11111100 */ 0xfc,
    /* 1111 -> 11111111 */ 0xff
  };  

  return lookup[data4bit & 0x0f];
}  

static struct bitmap_data new_bitmap(int left, int top, int width, int height)
{
  struct bitmap_data sResult = { .left = left, .top = top, .width = width, .height = height };
  sResult.data = kmalloc((width * height)/8 + 1, GFP_KERNEL);
  printk(KERN_ERR PFX "Created bitmap with top, left: %d, %d\n", sResult.top, sResult.left);
  return sResult;
}

static void free_bitmap(struct bitmap_data bitmap)
{
  if (bitmap.data)
  {
    kfree(bitmap.data); 
  }
}

static void set_display_byte(int xpos, int row, unsigned char value, struct bitmap_data bitmap)
{
  xpos = (xpos + bitmap.width) % bitmap.width;
  int offset = (xpos * bitmap.height) / 8 + row;
  ((unsigned char *)bitmap.data)[offset] = value;
}

/****************************************************************************
 * Sets the screen dimensions and font size based on
 * lcd_display_mode and zoom_enabled
 ****************************************************************************/
static void update_screen_dimensions(void)
{
  free_bitmap(sDisplayBuffer); 
  font_size = FONT_SIZE_16x8;
  screen_width = SCREEN_WIDTH_16x8;
  icon_size = 16;
  icon_position = ICON_POSITION_BOTTOM;

  /* Default values (the non-AMAX case) */
  graphics_height = GRAPHICS_HEIGHT_ALTO;
  graphics_width = GRAPHICS_WIDTH_ALTO;
  graphics_xoffset = GRAPHICS_XOFFSET_ALTO;
  graphics_yoffset = GRAPHICS_YOFFSET_ALTO;

  switch (lcd_display_mode)
  {
    /* 2 lines, with icons on bottom row (used on ALTO) */
    case LCD_TM13264CBCG_MODE_TM13264CBCG_0:
      if (zoom_enabled)
      {
        screen_width = SCREEN_WIDTH_32x16;
        screen_height = SCREEN_HEIGHT_MODE_0;
        font_size = FONT_SIZE_32x16;
        iTopRow = 7;
      }
      else
      {
        screen_height = SCREEN_HEIGHT_MODE_0;
        iTopRow = 6;  
      }
      break;
    
    /* 3 lines, with icons on bottom row */
    case LCD_TM13264CBCG_MODE_TM13264CBCG_1:
    case LCD_TM13264CBCG_MODE_CONFIG985:
      screen_height = SCREEN_HEIGHT_MODE_1;
      iTopRow = 7;
      icon_size = 8;
      break;
    
    /* 4 lines, with no icons */
    case LCD_TM13264CBCG_MODE_TM13264CBCG_2:
      screen_height = SCREEN_HEIGHT_MODE_2;
      iTopRow = 7;
      icon_size = 8;
      break;
    
    /* 3 lines, with icons on top row */
    case LCD_TM13264CBCG_MODE_TM13264CBCG_3:
      screen_height = SCREEN_HEIGHT_MODE_3;
      iTopRow = 5;
      icon_size = 8;
      icon_position = ICON_POSITION_TOP;
      break;

    /* 6 lines, with icons on bottom row */
    case LCD_TM13264CBCG_MODE_CONFIG1009:
      screen_height = SCREEN_HEIGHT_MODE_1;
      iTopRow = 7;
      icon_size = 8;
      screen_height = 6;
      screen_width = 20;
      font_size = FONT_SIZE_8x6;
      break;
    
    /* AMAX - 95 * 17 pixels */
    case LCD_TM13264CBCG_MODE_AMAX_KDC162A28BXXBB_95x17:
      iTopRow = 7;
      screen_height = SCREEN_HEIGHT_AMAX;
      screen_width = SCREEN_WIDTH_AMAX;
      graphics_height = GRAPHICS_HEIGHT_AMAX;
      graphics_width = GRAPHICS_WIDTH_AMAX;
      graphics_xoffset = GRAPHICS_XOFFSET_AMAX;
      graphics_yoffset = GRAPHICS_YOFFSET_AMAX;
      font_size = FONT_SIZE_8x6;
      break;    

    case LCD_TM13264CBCG_MODE_SSD0323_128x64:
      iTopRow = 7;
      icon_size = 8;
      screen_height = 3;
      screen_width = 15;
      graphics_height = 64;
      graphics_width = 128;
      graphics_xoffset = 0;
      graphics_yoffset = 0;
      font_size = FONT_SIZE_16x8;
      break;

    case LCD_TM13264CBCG_MODE_CONFIG983:
      iTopRow = 3;
      screen_height = 2;
      screen_width = 15;
      graphics_height = 32;
      graphics_width = 132;
      graphics_xoffset = 0;
      graphics_yoffset = 0;
      font_size = FONT_SIZE_16x8;
      break;
  }
  sDisplayBuffer = new_bitmap(graphics_xoffset, graphics_yoffset, graphics_width, graphics_height);
  tm13264cbcg_clear_screen();
}

/****************************************************************************
 * Indicates if we are allowed to draw the icons
 ****************************************************************************/
static int allowed_to_draw_icons(void)
{
  int iAllowedToDraw = 1;

  switch (lcd_display_mode)
  {
    case LCD_TM13264CBCG_MODE_TM13264CBCG_0:
    case LCD_TM13264CBCG_MODE_SSD0323_128x64:
      if (zoom_enabled)
        iAllowedToDraw = 0;
      break;
    
    case LCD_TM13264CBCG_MODE_TM13264CBCG_2:
    case LCD_TM13264CBCG_MODE_AMAX_KDC162A28BXXBB_95x17:
    case LCD_TM13264CBCG_MODE_CONFIG983:
      iAllowedToDraw = 0;
      break;
    
    case LCD_TM13264CBCG_MODE_TM13264CBCG_1:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_3:
    case LCD_TM13264CBCG_MODE_CONFIG1009:
    case LCD_TM13264CBCG_MODE_CONFIG985:
      break;
  }
  
  return iAllowedToDraw;
}

/****************************************************************************
 * Draws one line of barcode with the current font size
 * iLine - line number
 * pcText - hexadecimal string to draw
 *
 * Returns 0 on success or a negative number on failure.
 * 
 * This will need fixing to handle multi-visual-width characters, but should
 * deal correctly with multibyte UTF-8 characters.
 *
 * Note that the string represent a hexadecimal number. If the string is
 * longer than the display (MAX_BARCODE_LENGTH) it will ignore the left-most
 * characters as long as they are 0.
 ****************************************************************************/
static int draw_barcode(int iLine, const char *pcText)
{
  char cDigit;
  const char *pcCurrentChar = pcText;
  int aiCode[MAX_BARCODE_LENGTH];
  int i, iStringLength, iCodeIndex, iCodeLength;
  int iStartRow, iRows, iBlankCols;

  switch (lcd_display_mode)
  {
    case LCD_TM13264CBCG_MODE_TM13264CBCG_0:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_1:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_2:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_3:
    case LCD_TM13264CBCG_MODE_SSD0323_128x64:
    case LCD_TM13264CBCG_MODE_CONFIG1009:
    case LCD_TM13264CBCG_MODE_CONFIG985:
    case LCD_TM13264CBCG_MODE_CONFIG983:
      break;
    case LCD_TM13264CBCG_MODE_AMAX_KDC162A28BXXBB_95x17:
      return 0;
  }

  if (pcText == NULL)
  {
    return -EINVAL;
  }
  
  /* Build barcode */
  iStringLength = rutl_count_utf8_chars(pcText);
  for (i=0, iCodeIndex=0 ; i < iStringLength; i++)
  {
     cDigit = reciva_lcd_utf8_lookup(pcCurrentChar);
     pcCurrentChar = rutl_find_next_utf8(pcCurrentChar);

    /* If the string is too long for the display complain if the characters
     * are non-zero */
    if (i < (iStringLength - MAX_BARCODE_LENGTH))
    {
      if (cDigit != '0')
      {
        return -EMSGSIZE;
      }
    }
    /* Displayable characters */
    else
    {
      if (!isxdigit(cDigit))
      {
        return -EILSEQ;
      }

      cDigit = tolower(cDigit); /* ensure it is all lower case */
      aiCode[iCodeIndex++] =  (cDigit < 'a') ? code39_lookup[cDigit - '0']
                                             : code39_lookup[cDigit - 'a' + 10];
    }
  }
  iCodeLength = iCodeIndex;

  /* Calculate screen position */
  switch (font_size)
  {
    case FONT_SIZE_16x8:
    case FONT_SIZE_8x6: // Barcode not supported in 8x6 mode
      iRows = 2;
      iStartRow = iTopRow - (iLine * 2);
      break;
    case FONT_SIZE_32x16:
      iRows = 4;
      iStartRow = iTopRow - (iLine * 4);
      break;
    default:
      return -EINVAL;
  }

  switch (lcd_display_mode)
  {
    case LCD_TM13264CBCG_MODE_TM13264CBCG_0:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_1:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_2:
    case LCD_TM13264CBCG_MODE_TM13264CBCG_3:
    case LCD_TM13264CBCG_MODE_SSD0323_128x64:
    case LCD_TM13264CBCG_MODE_AMAX_KDC162A28BXXBB_95x17:
    case LCD_TM13264CBCG_MODE_CONFIG985:
    case LCD_TM13264CBCG_MODE_CONFIG983:
      break;
    case LCD_TM13264CBCG_MODE_CONFIG1009:
      iRows = 1;
      iStartRow = iTopRow - (iLine);
      break;
  }

  /* Calculate unused columns
   * Barcode: 16 pixels/char plus start (16) and stop (15 without space) */
  iBlankCols = 132 - ((iCodeLength * 16) + 31);
  BUG_ON(iBlankCols < 0);

  /* Display the bar code one row at a time */
  for (i = 0; i < iRows; i++)
  {
    set_row(iStartRow - i);
    set_column(0);

    draw_blank_columns8x1(iBlankCols >> 1, NOT_INVERTED);

    draw_barcode_char(BAR_STARTSTOP_CHAR);
    draw_blank_columns8x1(1, NOT_INVERTED);

    for (iCodeIndex=0; iCodeIndex < iCodeLength; iCodeIndex++)
    {
      draw_barcode_char(aiCode[iCodeIndex]);
      draw_blank_columns8x1(1, NOT_INVERTED);
    }

    draw_barcode_char(BAR_STARTSTOP_CHAR);

    draw_blank_columns8x1(iBlankCols - (iBlankCols >> 1), NOT_INVERTED);
  }

  return 0;
}

/****************************************************************************
 * Draws barcode character in code 39 (12x8)
 * iCode - 9-bit representation of the code to draw
 * 
 * Each bit of the code alternates between black and white (MSB is black).
 * Bit '1' indicates a wide bar.
 ****************************************************************************/
static void draw_barcode_char(int iCode)
{
  int iMask = 0x100; /* move from bit 9 rightwards */
  int iColour = 0xff; /* start with a black bar */
  while (iMask != 0)
  {
    blit_byte( iColour);
    if (iMask & iCode)
    {
      blit_byte( iColour);
      blit_byte( iColour);
    }
    iColour ^= 0xff;
    iMask >>= 1;
  }
}

/****************************************************************************
 * Initialise calgary OLED module (Intelligent Displays)
 * This is a copy of intialisation code suplied by Intelligent Displays
 ****************************************************************************/
static void init_registers_calgary()
{
  /* Clear RAM */
  int i;
  send_command (0x15); //COLUMN ADDRESS
  send_command (0x00); //START COL ADDRESS
  send_command (0x3F); //END COL ADDRESS
  
  send_command (0x75); //ROW ADDRESS
  send_command (0x00); //START ROW ADDRESS
  send_command (0x4F); //END ROW ADDRESS
  
  for (i=0;i<64*80;i++)  //128 col x 80 row
  {
    send_data(0x00);
  }

  send_command (0x15); //COLUMN ADDRESS
  send_command (0x00); //START COL ADDRESS
  send_command (0x3F); //END COL ADDRESS

  send_command (0x75); //ROW ADDRESS
  send_command (0x00); //START ROW ADDRESS
  send_command (0x3F); //END ROW ADDRESS

  send_command (0x81); //CONTRASTSETTING
  send_command (0x6d); //100%

  send_command (0x86); //FULL CURRENT

  send_command (0xA0); //Remap Command
  send_command (0x52); //Enable COM Split Odd Even,
                       //COM Re-map & Nibble Remap

  send_command (0xA1); //START LINE
  send_command (0x00); //START LINE 0

  send_command (0xA2); //OFFSET
  send_command (0x4C); //OFFSET 76 ROW

  send_command (0xA4); //NORMAL DISPLAY


  send_command (0xA8); //SET MULTIPLEX RATIO
  send_command (0x3F); //64 MUX

  send_command (0xAF); //DISPLAY ON

  send_command (0xB2);
  send_command (0x46);

  send_command (0xAD);
  send_command (0x02);

  send_command (0xCF);
  send_command (0xF0);

  send_command (0xB0);
  send_command (0x28);

  send_command (0xB4);
  send_command (0x07);

  send_command (0xB3);
  send_command (0x91);

  send_command (0xB1);
  send_command (0x22);

  send_command (0xBF);
  send_command (0x0D);

  send_command (0xBE);
  send_command (0x02);

  send_command (0xBC);
  send_command (0x10);

  send_command (0xB8); //Grayscale setting
  send_command (0x01);
  send_command (0x11);
  send_command (0x22);
  send_command (0x32);
  send_command (0x43);
  send_command (0x54);
  send_command (0x65);
  send_command (0x76);
}



module_init(tm13264cbcg_lcd_init);
module_exit(tm13264cbcg_lcd_exit);

EXPORT_NO_SYMBOLS;

MODULE_AUTHOR("John Stirling <js@reciva.com>");
MODULE_DESCRIPTION("Reciva TM13264CBCG LCD driver");
MODULE_LICENSE("GPL");

