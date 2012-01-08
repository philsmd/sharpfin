/*
 * Reciva LCD driver code for ALTO LCD module
 * Tianma TM13264CBCG (module)
 * Samsung S6B1713 (LCD controller)
 * Copyright (c) 2005-07 Reciva Ltd
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
 * GPIO PIN(S)                    LCD (8080)  (6800)
 * -----------                    ------------------
 * J3-20 GPC0 (+GPB2, GPA12)          /RD     E
 * J3-18 GPC1 (+ GPB3, GPA13)         /WR     RW
 * J3-16 GPC2                         A0/RS   A0/RS (register select)
 * J3-14 GPC3                         /RES    /RES (reset)
 * J3-12 GPC4  (+ GPB1)               /CS1    /CS1
 * J3-10 GPC5                         POWER   POWER
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
#include <linux/version.h>

#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/mach-types.h>

#ifdef CONFIG_ARCH_S3C2410
#include <asm/arch/regs-gpio.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#include <asm/arch/regs-irq.h>
#else
#include <asm/arch-s3c2410/S3C2410-irq.h>
#endif
#endif

#include "reciva_gpio.h"
#include "reciva_util.h"
#include "reciva_leds.h"
#include "reciva_backlight.h"
#include "reciva_lcd.h"
#include "lcd_generic.h"
#include "fontdata.h"
#include "reciva_font.h"
#include "reciva_lcd_tm13264cbcg.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#define RECIVA_MODULE_PARM(x) module_param(x, int, S_IRUGO)
#else
#define RECIVA_MODULE_PARM(x) MODULE_PARM(x, "i")
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#define RECIVA_MODULE_PARM_TYPE(x) module_param(x, uint, S_IRUGO)
#else
#define RECIVA_MODULE_PARM_TYPE(x) MODULE_PARM(x, "i")
#endif

/* This module takes a parameter to define the number of lines etc
 * 0 = 2 lines, with icons on bottom row (used on ALTO)
 * 1 = 3 lines, with icons on bottom row
 * 2 = 4 lines, with no icons
 * 3 = 3 lines, with icons on top row
 * 4 = 2 lines, no icons (95 *17 pixels)
 * 8 = 3 lines, no icons, 11x8 font (132 * 32 pixels) */
static lcd_tm13264cbcg_mode_t lcd_display_mode = LCD_TM13264CBCG_MODE_TM13264CBCG_0;
RECIVA_MODULE_PARM_TYPE(lcd_display_mode);

static lcd_hardware_t lcd_hardware = LCD_TM13264CBCG;
RECIVA_MODULE_PARM_TYPE(lcd_hardware);

// Will only take note of the pins specified in parallel_pin_cfg
// if this is set
static int use_custom_gpio;
RECIVA_MODULE_PARM_TYPE(use_custom_gpio);

static int power_level;
RECIVA_MODULE_PARM_TYPE(power_level);

// MCU paralell interface type - 6800 or 808. 6800 is default
static int interface_8080;
RECIVA_MODULE_PARM_TYPE(interface_8080);

// Rewrite contrast register on every screen redraw
static int rewrite_contrast;
RECIVA_MODULE_PARM_TYPE(rewrite_contrast);

/* Override clock display style */
typedef enum
{
  CLOCK_STYLE0  = 0,
  CLOCK_STYLE1  = 1,
  CLOCK_STYLE2  = 2,
  CLOCK_STYLE3  = 3,
  CLOCK_STYLE4  = 4,
  CLOCK_STYLE5  = 5,

} clock_style_t;
static clock_style_t clock_style = -1;
RECIVA_MODULE_PARM_TYPE(clock_style);

/* Defines the initial contrast level */
static int lcd_contrast_level = -1;
RECIVA_MODULE_PARM(lcd_contrast_level);

// Allow initial reference voltage (contrast) to be changed by module parameter
static int init_reference_v = -1;
RECIVA_MODULE_PARM(init_reference_v);

/* Defines if display is reversed/upside down
 * Bitmask
 * Bit 0 - ADC
 * Bit 1 - SHL */
static int segment_remap = 0;
RECIVA_MODULE_PARM(segment_remap);

/* Defines which bias setting should be used
 * 0 = 1/9, 1 = 1/7 */
static int alternate_bias = 0;
RECIVA_MODULE_PARM(alternate_bias);

/* Defines if the display is positive or negative (invert pixels)
 * 0 = normal, 1 = inverted
 */
static int reversed_pixels = 0;
RECIVA_MODULE_PARM(reversed_pixels);

/* Defines type of display 0=OLED, 1=LCD*/
static int display_type = 1;
RECIVA_MODULE_PARM(display_type);

/* Calgary module - register initialisation values */
static int calgary_contrast_control = 0x6d;
static int calgary_current_range = 0x86;
RECIVA_MODULE_PARM(calgary_contrast_control);
RECIVA_MODULE_PARM(calgary_current_range);

/* Number of blank columns between icons */
static int icon_spacing = 2;
RECIVA_MODULE_PARM(icon_spacing);

/* 0 = use default greyscale backlight config - all non zero levels show something on screen
 * 1 = 0,1,2 are all off (used on cf851) */
static int greyscale_config = 0;
RECIVA_MODULE_PARM(greyscale_config);

// LCD_E -> GPC0 or GPC6 on Stingray
static unsigned long lcd_e = (1 << 0);

   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

typedef enum
{
  NOT_INVERTED = 0,
  INVERTED     = 1,

} InvertStatus_t;

/* Font size */
typedef enum 
{
  FONT_SIZE_16x8,
  FONT_SIZE_32x16,
  FONT_SIZE_8x6,
  FONT_SIZE_11x8,
} FontSize_t;

struct display_mode
{
  FontSize_t tFontSize;
  int iHeight_c;
  int iWidth_c;
  int iTopRowOffset_p;
  void (*pfDrawIcon)(int);
  int iIconPosition;
  int iZoomMode;
  int iInverseBleed;
  void (*pfDrawClock)(struct lcd_draw_clock *, int);
  int (*pfDrawArrow)(int);
  void (*pfDrawSmallClock)(struct lcd_draw_small_clock *);
  int iNoSignalStrength;
  int iIconLinePosition;
};

struct serial_pin_cfg
{
  unsigned int cs;
  unsigned int rs;
  unsigned int scl;
  unsigned int sda;
};

struct parallel_pin_cfg
{
  unsigned int cs;
  unsigned int e;
  unsigned int rs;
  unsigned int rw;
};

struct display_hardware
{
  int iHeight_p;
  int iWidth_p;
  int iXOffset_p;
  int iYOffset_p;

  int regulator_resistor;
  int reference_v;
  int rw_high;
  int parallel;
  int set_contrast;
  int power_off_by_reset;
  int *piRowLookup;
  int iFlip;
  int greyscale;
  int power_fail_present;
  int dont_use_cs; // Don't drive LCD control signal CS (LCDN4)

  union
  {
    struct serial_pin_cfg serial;
    struct parallel_pin_cfg parallel;
  } pin_cfg;

  int reset_pin;
  int power_pin;
};

/* Allow override of all display hardware info via module params */
static int dh_height = -1;
static int dh_width = -1;
static int dh_xoffset = -1;
static int dh_yoffset = -1;
static int dh_regulator_resistor = -1;
static int dh_reference_v = -1;
static int dh_rw_high = -1;
static int dh_parallel = -1;
static int dh_set_contrast = -1;
static int dh_power_off_by_reset = -1;
static int dh_flip = -1;
static int dh_greyscale = -1;
static int dh_power_fail_present = -1;
static int dh_dont_use_cs = -1;

RECIVA_MODULE_PARM(dh_height);
RECIVA_MODULE_PARM(dh_width);
RECIVA_MODULE_PARM(dh_xoffset);
RECIVA_MODULE_PARM(dh_yoffset);
RECIVA_MODULE_PARM(dh_regulator_resistor);
RECIVA_MODULE_PARM(dh_reference_v);
RECIVA_MODULE_PARM(dh_rw_high);
RECIVA_MODULE_PARM(dh_parallel);
RECIVA_MODULE_PARM(dh_set_contrast);
RECIVA_MODULE_PARM(dh_power_off_by_reset);
RECIVA_MODULE_PARM(dh_flip);
RECIVA_MODULE_PARM(dh_greyscale);
RECIVA_MODULE_PARM(dh_power_fail_present);
RECIVA_MODULE_PARM(dh_dont_use_cs);

/* Allow override of all display mode info via module params */
//static int dm_font_size = -1;
static int dm_width_c = -1;
static int dm_height_c = -1;
static int dm_top_row_offset_p = -1;
static int dm_draw_icon = -1;
static int dm_icon_position = -1;
static int dm_zoom_mode = -1;
static int dm_inverse_bleed = -1;
static int dm_row_lookup = -1;
//static int dm_draw_clock = -1;
//static int dm_draw_arrow = -1;
static int dm_icon_line_position = -1;

RECIVA_MODULE_PARM(dm_width_c);
RECIVA_MODULE_PARM(dm_height_c);
RECIVA_MODULE_PARM(dm_top_row_offset_p);
RECIVA_MODULE_PARM(dm_draw_icon);
RECIVA_MODULE_PARM(dm_icon_position);
RECIVA_MODULE_PARM(dm_zoom_mode);
RECIVA_MODULE_PARM(dm_inverse_bleed);
RECIVA_MODULE_PARM(dm_row_lookup);
RECIVA_MODULE_PARM(dm_icon_line_position);

// Force date off in standby clock. 
// Only works for clock config 1
static int clock_disable_date;
RECIVA_MODULE_PARM(clock_disable_date);


// Stored in vertical columns
//
// eg a 128(w) x 40(h) bitmap is stored as follows
// [0] [5] .......[635]
// [1] [6] .......[636]
// [2] [7] .......[637]
// [3] [8] .......[638]
// [4] [9] .......[639]
struct display_bitmap
{
  int iHeight_b;          // Height in bytes (bits * 8)
  unsigned char *buffer;  // Raw bitmap
  int bufsize;            // Size of buffer
};

struct display_position
{
  u32 u32Mask;
  int iByteOffset;
  int iPage;
  int iShift;
  int iHeight_p;
};

   /*************************************************************************/
   /***                        Static Functions                           ***/
   /*************************************************************************/

static int __init tm13264cbcg_lcd_init (void);
static void __exit tm13264cbcg_lcd_exit(void);

/* Functions in lcd interface */
static void tm13264cbcg_init_hardware(void);
static int  tm13264cbcg_draw_screen(char **acText, int iX, int iY,
                                    int iCursorType, int iCursorWidth, int *piArrows, int *piLineContents);
static int tm13264cbcg_get_height(void);
static int tm13264cbcg_get_width(void);
static void tm13264cbcg_clear_screen(void);
static void tm13264cbcg_set_greyscale_backlight(int level);
static void tm13264cbcg_set_led(unsigned int mask);
static void tm13264cbcg_set_contrast(int level);
static void tm13264cbcg_power_off(void);
static int tm13264cbcg_draw_clock(struct lcd_draw_clock *psClockInfo);
static void tm13264cbcg_draw_signal_strength(int iLevel);
static void tm13264cbcg_set_zoom(int iOn, struct lcd_resize *psNewSize);
static void tm13264cbcg_draw_icons(unsigned int iBitmask);
static int tm13264cbcg_get_capabilities(void);
static void tm13264cbcg_set_display_mode(int iMode);
static void tm13264cbcg_draw_bitmap(struct bitmap_data);
static int tm13264cbcg_get_graphics_height(void);
static int tm13264cbcg_get_graphics_width(void);
static struct bitmap_data tm13264cbcg_grab_screen_region(int left, int top, int width, int height);
static void tm13264cbcg_redraw_screen(void);
static int tm13264cbcg_set_ampm_text(struct lcd_ampm_text *ampm_text);
static int tm13264cbcg_set_icon_slots(int);
  
/* Local helper functions */
static void setup_gpio (void);
static void setup_gpio_1bit_interface(void);
static void setup_gpio_8bit_interface(void);
static void init_registers_default(void);
static void init_calgary(void);
static void init_registers_calgary(void);
static int stretch4to8(int data4bit);
static int draw_line(int iLine, const char *pcText, int iX, 
                     int iCursorType, int iCursorWidth, InvertStatus_t tInverted, int iArrows,
                     struct display_mode *psMode);
static void draw_character_8x6(int iData, int iInvert);
static void draw_character_16x8(int iData, int iInvert);
static void draw_character_16x8_shifted(int iData, int iInvert, int iShift);
static void draw_chinese_character_16x16(int iData, int iInvert);
static void draw_character_11x8(int iData, int iInvert);
static void draw_icon_8x8(int iIcon);
static void draw_icon_16x16(int iIcon);
static void draw_character_32x16(int iData, int iInvert);
static void draw_chinese_character_32x32(int iData, int iInvert);
static void draw_zoomed_character(const unsigned short *pFontData, int iWidth,
                                  int iInvert);
static void draw_clock_digit(const u32 *pu32Data, int iWidth, int iDigit);
static int iFontHeight_p(FontSize_t tFontSize);
static int iFontWidth_p(FontSize_t tFontSize);
static void draw_blank_columns(int iCount, InvertStatus_t tInvertStatus);
static int draw_barcode(int iLine, const char *pcText);
static void draw_barcode_char(int iCount);
static struct bitmap_data new_bitmap(int left, int top, int width, int height);
static void test_pattern(void);
static void set_display_position(int iHeight, int iColumn, int iRow);
static void blit_column(u64 u64Data);
static int get_text_from_user(char **ppcDest, char *pcSrc);
static int iDoCharLookupOnString(const char *pcText, int *piLocalRep,
                                 int iWidth, int *piCharLength);
static void draw_alarm_time(int i12HourFormat, int iColumnOffset,
                            int iAlarmHours, int iAlarmMinutes);
static void draw_clock_icon_16x15(int iIcon);
static void draw_clock_config0and4(struct lcd_draw_clock *psClockInfo, int i12HourFormat);
static void draw_clock_config0(struct lcd_draw_clock *psClockInfo, int i12HourFormat);
static void draw_clock_config1(struct lcd_draw_clock *psClockInfo, int i12HourFormat);
static void draw_clock_config2(struct lcd_draw_clock *psClockInfo, int i12HourFormat);
static void draw_clock_config3(struct lcd_draw_clock *psClockInfo, int i12HourFormat);
static void draw_clock_config4(struct lcd_draw_clock *psClockInfo, int i12HourFormat);
static int draw_arrow_none(int iArrowType);
static int draw_arrow_16xX(int iArrowType);
static int draw_arrow_16x6(int iArrowType);
static int draw_arrow_16x5(int iArrowType);
static int draw_arrow_8x5(int iArrowType);
static int tm13264cbcg_draw_small_clock(struct lcd_draw_small_clock *psClockInfo);
static int tm13264cbcg_display_enable(int enabled);
static void draw_small_clock(struct lcd_draw_small_clock *psClockInfo);
#define SHIFT_OFF 0
#define SHIFT_ON  1

static void draw_8bit(const u8 *pu8Data, int iWidth, int iInvert);
static void draw_16bit(const u16 *pu16Data, int iWidth, int iInvert);
static void draw_16bit_shifted(const u16 *pu16Data, int iWidth, int iInvert, int iShift);
static void draw_32bit(const u32 *pu32Data, int iWidth, int iInvert);

static void draw_bitmap(void);
static void setup_bitmap(struct display_bitmap *sNewBitmap);
static void draw_bitmap_greyscale(void);

#define RS_CONTROL  0
#define RS_DATA     1
static void send_data(int data); 
static void send_command(int data);

#ifdef CONFIG_ARCH_S3C2410
static void (*cycle)(int iRS, int iData, int iDelay);
static void cycle_serial (int iRS, int iData, int iDelay);
static void cycle_parallel (int iRS, int iData, int iDelay);
static void cycle_parallel_8080 (int iRS, int iData, int iDelay);
static void cycle_parallel2 (int iRS, int iData, int iDelay);
#endif

#ifdef CONFIG_ARCH_MV88W8618
static void cycle (int iRS, int iData, int iDelay);
#endif

#ifdef ENABLE_READ
static int read_8bit (int iRS, int iDelay);
#endif

#define AMPM_LENGTH 2
static int aiAM[AMPM_LENGTH] = {'A', 'M'};
static int iAMLen = AMPM_LENGTH;
static int aiPM[AMPM_LENGTH] = {'P', 'M'};
static int iPMLen = AMPM_LENGTH;

#define PFX "tm13264cbcg: "

static char bDrawSeparator = 1;

   /*************************************************************************/
   /***                        Local defines                              ***/
   /*************************************************************************/

/****************************************************************************
 * Number of icons on screen
 * Signal strength starts at 102, which leaves space for (102-2)/14 = 7 icons
 *
 * There are currently 8 icons, but IRADIO and MEDIA are mutally exclusive,
 * so we can display everything at the moment.
 ****************************************************************************/
#define DEFAULT_MAX_ICONS 7
static int max_icons;

/* Size of the graphics surfaces. N.B Graphics
 * Height MUST be a multiple of 8 */



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

/* Maximum value for the backlight when greyscale backlight being used */
#define MAX_GREYSCALE_BACKLIGHT 15

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
  get_max_backlight:    reciva_bl_get_max_backlight,
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
  redraw_screen:        tm13264cbcg_redraw_screen,
  set_ampm_text:        tm13264cbcg_set_ampm_text,
  get_font_buffer:      rfont_get_font_buffer,
  get_font_buffer_size: rfont_get_font_buffer_size,
  set_language:         rfont_set_language,
  draw_small_clock:     tm13264cbcg_draw_small_clock,
  display_enable:       tm13264cbcg_display_enable,
  set_icon_slots:       tm13264cbcg_set_icon_slots,
};

static int greyscale_backlight_level = MAX_GREYSCALE_BACKLIGHT;

/* Zoom related */
static int zoom_enabled = 0;

// Can be selected via mopule param
// 0 = aiRowLookup
// 1 = aiRowLookup_reverse
// 2 = aiRowLookup_split
// 3 = aiRowLookup_tiffany
// 4 = aiRowLookup_split_reverse
static int aiRowLookup[] = {0,1,2,3,4,5,6,7,8};
static int aiRowLookup_reverse[] = {7,6,5,4,3,2,1,0,8};
static int aiRowLookup_split[] = {4,5,6,7,0,1,2,3,8};
static int aiRowLookup_tiffany[] = {5,4,3,2,1,0,8,7,6};
static int aiRowLookup_split_reverse[] = {3,2,1,0,7,6,5,4,8};

static struct display_hardware asLCDHardware[] =
{
#include "tm13264cbcg_hw_variants.c"
};

static struct display_mode asLCDMode[] =
{
#include "tm13264cbcg_display_modes.c"
};

/* Main display variables */
static struct display_mode *psCurrentMode = &asLCDMode[0];
static struct display_position sPosition;
static struct display_hardware *psHardware = NULL;
static struct display_bitmap sBitmap = { .buffer = NULL };

/* This is the main font used for text display - 16 pixels high, 8 bits wide */
static const unsigned short au16FontData[] =
{
#include "fontdata.c"
};

/* 8x6 font */
static const unsigned char aucFontData_8x6[] =
{
#include "reciva_fontdata_8x6.c"
};

static const unsigned short au16FontData_11x8[] =
{
#include "fontdata_11x8.c"
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
  /* 0x0a MONO */         0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
  /* 0x0b STEREO */       0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
  /* 0x0c ALARM 1 */      0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
  /* 0x0d ALARM 2 */      0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
  /* 0x0e THUMBS UP */    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
  /* 0x0f NPR */          0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
  /* 0x10 PANDORA */      0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
  /* 0x11 FAVOURITE */    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
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
  /* 0x0a MONO */         0xfe, 0xc0, 0x60, 0x30, 0x60, 0xc0, 0xfe, 0x00, 
  /* 0x0b STEREO */       0x64, 0x92, 0x92, 0x0c, 0x80, 0xfe, 0x80, 0x00, 
  /* 0x0c ALARM 1 */      0x04, 0x7c, 0xfe, 0x7c, 0x04, 0x40, 0xf0, 0x00, 
  /* 0x0d ALARM 2 */      0x04, 0x7c, 0xfe, 0x7c, 0x04, 0xb0, 0xd0, 0x00, 
  /* 0x0e THUMBS UP */    0x0e, 0x0e, 0x3f, 0xff, 0x1f, 0x1f, 0x1c, 0x00, 
  /* 0x0f NPR */          0x00, 0x7e, 0x20, 0x40, 0x40, 0x20, 0x1e, 0x00, 
  /* 0x10 PANDORA */      0x81, 0xff, 0x89, 0x88, 0x88, 0x70, 0x00, 0x00, 
  /* 0x11 FAVOURITE */    0x49, 0x2a, 0x1c, 0x7f, 0x1c, 0x2a, 0x49, 0x00, 
};

// Map from bitmask used in RLG to array position in IconData
static const int aiIconMap[] =
{
  0x01, // LCD_BITMASK_SHIFT              0x00000001
  0x02, // LCD_BITMASK_IRADIO             0x00000002
  0x03, // LCD_BITMASK_MEDIA              0x00000004
  0x04, // LCD_BITMASK_SHUFFLE            0x00000008
  0x05, // LCD_BITMASK_REPEAT             0x00000010
  0x06, // LCD_BITMASK_SLEEP_TIMER        0x00000020
  0x07, // LCD_BITMASK_MUTE               0x00000040
  0x08, // LCD_BITMASK_ALARM              0x00000080
  0x09, // LCD_BITMASK_SNOOZE             0x00000100
  0x0a, // LCD_BITMASK_MONO               0x00000200
  0x0b, // LCD_BITMASK_STEREO             0x00000400
  0x0c, // LCD_BITMASK_ALARM1             0x00000800
  0x0d, // LCD_BITMASK_ALARM2             0x00001000
  0x00, // LCD_BITMASK_NAP_ALARM          0x00002000
  0x00, // LCD_BITMASK_RF_REMOTE_PAIRED   0x00004000
  0x00, // LCD_BITMASK_RF_REMOTE_UNPAIRED 0x00008000
  0x00, // LCD_BITMASK_FM                 0x00010000
  0x00, // LCD_BITMASK_AUX_IN             0x00020000
  0x00, // LCD_BITMASK_DAB                0x00040000
  0x00, // LCD_BITMASK_SD_CARD            0x00080000
  0x00, // LCD_BITMASK_USB                0x00100000
  0x00, // LCD_BITMASK_MP3                0x00200000
  0x00, // LCD_BITMASK_WMA                0x00400000
  0x0e, // LCD_BITMASK_THUMBS_UP          0x00800000
  0x0f, // LCD_BITMASK_NPR                0x01000000
  0x10, // LCD_BITMASK_PANDORA            0x02000000
  0x11, // LCD_BITMASK_FAVOURITE          0x04000000
};

#define MAX_ICON_ID (sizeof(au8IconData8x8)/8)

/* Data for 32x16 clock display "1234567890:2 */
static const u32 au32Font32x26[] =
{
  #include "clock_fontdata_32x16.c"
};

/*  Signal strength icon and bars - 8 pixels high, 6 pixels wide */
#define SIG_STRENGTH_BARS 4
static const u8 au8SignalStrength8x6[] =
{
/*0*/ 0xc0, 0xa0, 0xff, 0xa0, 0xc0, 0x00, // Signal strength ICON (large)
/*1*/ 0x03, 0x03, 0x03, 0x03, 0x03, 0x00, // Signal strength bar - 2 pixels
/*2*/ 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x00, // Signal strength bar - 4 pixels
/*3*/ 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x00, // Signal strength bar - 6 pixels
/*4*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, // Signal strength bar - 8 pixels
};

/* Left and right arrows - 6 pixels wide */
static unsigned short au16LeftArrowLookup[] =
{ 0x0100, 
  0x0380,
  0x06c0,
  0x0c60,
  0x1830,
  0x0000,
};
static unsigned short au16RightArrowLookup[] =
{
  0x0000,
  0x1830,
  0x0c60,
  0x06c0,
  0x0380,
  0x0100,
};

static u32 au32Font32x22[] =
{
  #include "clock_fontdata_32x22.c"
};

unsigned short au16ClockIcons16x15[] =
{
  #include "clock_fontdata_icons_16x15.c"
};

static int iHardwareInitialised = 0;
  
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
  return psCurrentMode->iHeight_c;
}
static int tm13264cbcg_get_width(void)
{
  return psCurrentMode->iWidth_c;
}

#ifdef CONFIG_ARCH_S3C2410
/****************************************************************************
 * Shut down LCD when power fails
 ****************************************************************************/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
static irqreturn_t tm13264cbcg_power_fail (int irq, void *handle)
#else
static void tm13264cbcg_power_fail (int irq, void *handle, struct pt_regs *regs)
#endif
{
  disable_irq (IRQ_EINT1);

  if (psHardware->power_off_by_reset)
  {
    s3c2410_gpio_setpin (psHardware->reset_pin, 0);         // reset low
  }
  else
  {
    /* Power down Vcc (15V) */
    s3c2410_gpio_setpin (psHardware->power_pin, 1); // High (OFF)
    /* Display OFF */
    cycle (RS_CONTROL, CTL_DISPLAY_OFF, 0);
  }

  printk (PFX "Power off\n");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
  return IRQ_HANDLED;
#endif
}

/****************************************************************************
 * Utility functions to change GPC0 to GPC6 on Stingray
 ****************************************************************************/
static void check_gpc0(int *pin)
{
  if (*pin == S3C2410_GPC0)
  {
    *pin = S3C2410_GPC6;
  }
}

static void check_for_gpc0(void)
{
  if (psHardware->parallel)
  {
    check_gpc0(&(psHardware->pin_cfg.parallel.e));
    check_gpc0(&(psHardware->pin_cfg.parallel.rw));
    check_gpc0(&(psHardware->pin_cfg.parallel.rs));
    check_gpc0(&(psHardware->pin_cfg.parallel.cs));
  }
  else
  {
    check_gpc0(&(psHardware->pin_cfg.serial.sda));
    check_gpc0(&(psHardware->pin_cfg.serial.scl));
    check_gpc0(&(psHardware->pin_cfg.serial.rs));
    check_gpc0(&(psHardware->pin_cfg.serial.cs));
  }
}
#endif

/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init
tm13264cbcg_lcd_init (void)
{
  int iErr =0;
  int i;

  printk(PFX "init mode=%d hardware=%d clock=%d\n", lcd_display_mode, lcd_hardware, clock_style);
  printk(PFX "  calgary_contrast_control=%02x\n", calgary_contrast_control);
  printk(PFX "  calgary_current_range=%02x\n", calgary_current_range);
  printk(PFX "  interface_8080=%d\n", interface_8080);
  printk(PFX "  dm_row_lookup=%d\n", dm_row_lookup);
  printk(PFX "  greyscale_config=%d\n", greyscale_config);
  printk(PFX "  clock_disable_date=%d\n", clock_disable_date);

  if (lcd_hardware >= (sizeof(asLCDHardware)/sizeof(asLCDHardware[0])))
  {
    printk(PFX "Illegal hardware mode\n");
    return -1;
  }

  /* Setup clock style if it has been specified as a module parameter
   * Write to all modes in case there might be a zoom option */
  if (clock_style >= 0)
  {
    for (i=0; i<sizeof(asLCDMode)/sizeof(asLCDMode[0]); i++)
    {
      switch (clock_style)
      {
        case CLOCK_STYLE0: 
          asLCDMode[i].pfDrawClock = draw_clock_config0;
          break;
        case CLOCK_STYLE1: 
          asLCDMode[i].pfDrawClock = draw_clock_config1;
          break;
        case CLOCK_STYLE2: 
          asLCDMode[i].pfDrawClock = draw_clock_config2;
          break;
        case CLOCK_STYLE3: 
          asLCDMode[i].pfDrawClock = draw_clock_config3;
          break;
        case CLOCK_STYLE4: 
          asLCDMode[i].pfDrawClock = draw_clock_config4;
          break;
        case CLOCK_STYLE5: 
          asLCDMode[i].pfDrawClock = NULL;
          break;
      }
    }
  }

  psHardware = &(asLCDHardware[lcd_hardware]);
  // Override default contrast if a module parameters has been specified
  if (init_reference_v > -1)
  {
    psHardware->reference_v = init_reference_v & 63;
  }

  /* Override hardware info if specified via module params */
  if (dh_height > -1)
    psHardware->iHeight_p = dh_height;
  if (dh_width > -1)
    psHardware->iWidth_p = dh_width ;
  if (dh_xoffset > -1)
    psHardware->iXOffset_p = dh_xoffset;
  if (dh_yoffset > -1)
    psHardware->iYOffset_p = dh_yoffset;
  if (dh_regulator_resistor > -1)
    psHardware->regulator_resistor = dh_regulator_resistor;
  if (dh_reference_v > -1)
    psHardware->reference_v = dh_reference_v;
  if (dh_rw_high > -1)
    psHardware->rw_high = dh_rw_high;
  if (dh_parallel > -1)
    psHardware->parallel = dh_parallel;
  if (dh_set_contrast > -1)
    psHardware->set_contrast = dh_set_contrast;
  if (dh_power_off_by_reset > -1)
    psHardware->power_off_by_reset = dh_power_off_by_reset;
  if (dh_flip > -1)
    psHardware->iFlip = dh_flip;
  if (dh_greyscale > -1)
    psHardware->greyscale = dh_greyscale;
  if (dh_power_fail_present > -1)
    psHardware->power_fail_present = dh_power_fail_present;
  if (dh_dont_use_cs > -1)
    psHardware->dont_use_cs = dh_dont_use_cs;

#ifdef CONFIG_ARCH_S3C2410
  if (machine_is_rirm3())
  {
    lcd_e = (1 << 6);
    check_for_gpc0();
  }
#endif

  printk(PFX "%s psHardware %p\n", __FUNCTION__, psHardware);
  printk(PFX "\tiHeight_p %d iWidth_p %d iXOffset_p %d iYOffset_p %d\n",
                psHardware->iHeight_p,   psHardware->iWidth_p,   psHardware->iXOffset_p,   psHardware->iYOffset_p);
  printk(PFX "\tregulator_resistor %d reference_v %d rw_high %d parallel %d\n",
                psHardware->regulator_resistor,   psHardware->reference_v,   psHardware->rw_high,   psHardware->parallel);
  printk(PFX "\tset_contrast %d power_off_by_reset %d piRowLookup %p iFlip %d greyscale %d dont_use_cs=%d\n",
                psHardware->set_contrast,   psHardware->power_off_by_reset,   psHardware->piRowLookup,   psHardware->iFlip,   psHardware->greyscale, psHardware->dont_use_cs);

  setup_bitmap(&sBitmap);
  
  /* Set up the screen dimensions and font size etc */
  tm13264cbcg_set_display_mode(lcd_display_mode);

  /* Override display mode info if specified via module params */
  if (dm_width_c > -1)
    psCurrentMode->iWidth_c = dm_width_c;
  if (dm_height_c > -1)
    psCurrentMode->iHeight_c = dm_height_c;
  if (dm_top_row_offset_p > -1)
    psCurrentMode->iTopRowOffset_p = dm_top_row_offset_p;
  if (dm_icon_position > -1)
    psCurrentMode->iIconPosition = dm_icon_position;
  if (dm_icon_line_position > -1)
    psCurrentMode->iIconLinePosition = dm_icon_line_position;
  if (dm_zoom_mode > -1)
    psCurrentMode->iZoomMode = dm_zoom_mode;
  if (dm_inverse_bleed > -1)
    psCurrentMode->iInverseBleed = dm_inverse_bleed;
  if (dm_row_lookup > -1)
  {
    switch (dm_row_lookup)
    {
      case 0: psHardware->piRowLookup = aiRowLookup;                break;
      case 1: psHardware->piRowLookup = aiRowLookup_reverse;        break;
      case 2: psHardware->piRowLookup = aiRowLookup_split;          break;
      case 3: psHardware->piRowLookup = aiRowLookup_tiffany;        break;
      case 4: psHardware->piRowLookup = aiRowLookup_split_reverse;  break;
    }
  }
  if (dm_draw_icon > -1)
  {
    switch (dm_draw_icon)
    {
      case 0: psCurrentMode->pfDrawIcon = NULL;                     break;
      case 8: psCurrentMode->pfDrawIcon = draw_icon_8x8;            break;
      case 16: psCurrentMode->pfDrawIcon = draw_icon_16x16;         break;
    }
  }

  printk(PFX "%s psCurrentMode %p\n", __FUNCTION__, psCurrentMode);
  printk(PFX "\tiWidth_c %d iHeight_c %d iTopRowOffset_p %d iIconPosition %d iIconLinePosition %d dm_draw_icon %d\n",
                psCurrentMode->iWidth_c, psCurrentMode->iHeight_c, psCurrentMode->iTopRowOffset_p, psCurrentMode->iIconPosition, psCurrentMode->iIconLinePosition, dm_draw_icon);
 
#ifdef CONFIG_ARCH_S3C2410
  /* Set up power-fail */
  if (psHardware->power_fail_present)
  {
    /* Double check to see if the power-fail hardware is present */
    /* set GPF1 as input */
    rutl_regwrite (0 << 2, 3 << 2, (int)S3C2410_GPFCON);
    /* enable pullup */
    rutl_regwrite (0 << 1, 1 << 1, (int)S3C2410_GPFUP);
    /* wait a bit */
    mdelay (1);
    if ((__raw_readl (S3C2410_GPFDAT) & (1 << 1)) != 0)
    {
      psHardware->power_fail_present = 0;
    }
    else
    {
      /* set GPF1 as EINT1 */
      rutl_regwrite (2 << 2, 3 << 2, (int)S3C2410_GPFCON);

      /* set EINT1 as rising edge triggered */
      rutl_regwrite (4 << 4, 7 << 4, (int)S3C2410_EXTINT0);

      /* clear up any outstanding interrupts */
      __raw_writel (1 << 1, S3C2410_SRCPND);

      request_irq (IRQ_EINT1, tm13264cbcg_power_fail, SA_INTERRUPT | SA_SHIRQ, "power fail", &tm13264cbcg_power_fail);
    }
  }

  printk (PFX "power failure hardware %s present\n", psHardware->power_fail_present ? "is" : "not");
#endif

  // If greyscale set, backlight is simulated using the greyscale levels
  // To make that work, alternate 'set' and 'get_max' functions are inserted
  if (psHardware->greyscale)
  {
    tm13264cbcg_driver.set_backlight = tm13264cbcg_set_greyscale_backlight;
  }

  /* generic initialisation */
#ifdef CONFIG_ARCH_S3C2410
  tm13264cbcg_driver.leds_supported = reciva_get_leds_supported();
#else
  tm13264cbcg_driver.leds_supported = 0;
#endif
  iErr = reciva_lcd_init(&tm13264cbcg_driver,
                         psCurrentMode->iHeight_c, psCurrentMode->iWidth_c);
  if (iErr < 0)
  {
    printk (KERN_ERR PFX "reciva_lcd_init failure (err %d)\n", iErr);
    tm13264cbcg_lcd_exit();
  }

  return iErr;
}

/****************************************************************************
 * Initialises LCD hardware
 ****************************************************************************/
static void tm13264cbcg_init_hardware()
{
  max_icons = DEFAULT_MAX_ICONS;

  /* Set up GPIO pins */
  setup_gpio ();
  
  /* Initialise the registers */
  if (psHardware->greyscale)
  {
    init_calgary();
  }
  else
  {
#ifdef CONFIG_ARCH_S3C2410
    if (display_type == 0)
    {
      /* Apply Vcc */
      s3c2410_gpio_cfgpin (psHardware->power_pin, S3C2410_GPIO_OUTPUT);
      s3c2410_gpio_pullup (psHardware->power_pin, 1);         // pullup off
      s3c2410_gpio_setpin (psHardware->power_pin, power_level);         // pin low
    }
#endif

    init_registers_default();
  }

  iHardwareInitialised = 1;

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
  if (segment_remap & 1)
    cycle (RS_CONTROL, CTL_ADC_SEG_REVERSE, 0);  /* Reversed */
  else
    cycle (RS_CONTROL, CTL_ADC_SEG_NORMAL, 0);  /* Normal */

  /* SHL Select - reverse direction */
  if (segment_remap & 2)
    cycle (RS_CONTROL, CTL_SHL_SEG_NORMAL, 0);
  else
    cycle (RS_CONTROL, CTL_SHL_SEG_REVERSE, 0);

  if (display_type)
  {
  //ONLY LCD
  
  /* LCD Bias Select - Duty Ratio = 1/65 Bias = 1/9  */
  /*                   Duty Ratio = 1/33 Bias = 1/6  */
  if (alternate_bias)
    cycle (RS_CONTROL, CTL_LCD_BIAS_0, 0);
  else
    cycle (RS_CONTROL, CTL_LCD_BIAS_1, 0);

  /* Set if pixels are reversed or not */
  if (reversed_pixels)
    cycle (RS_CONTROL, CTL_PIX_REVERSED_ON, 0);
  else
    cycle (RS_CONTROL, CTL_PIX_REVERSED_OFF, 0);

  /* Power Control - Power Supply Configuration (V0 = 9.45V) */
  // Voltage Converter ON
  cycle (RS_CONTROL, CTL_POWER | CTL_POWER_VC, 0);
  mdelay(2);          // Wait > 1ms
  // Voltage Converter, Regulator ON
  cycle (RS_CONTROL, CTL_POWER | CTL_POWER_VC | CTL_POWER_VR, 0);
  mdelay(2);          // Wait > 1ms
  // Voltage Converter, Regulator, Follower ON
  cycle (RS_CONTROL, CTL_POWER | CTL_POWER_VC | CTL_POWER_VR | CTL_POWER_VF, 0);

  /* Regulator Resistor Select */
  cycle (RS_CONTROL, psHardware->regulator_resistor, 0);

  /* Reference Voltage Select (2 byte instruction) */
  cycle (RS_CONTROL, CTL_SET_REF_V, 0); // Select mode
  cycle (RS_CONTROL, psHardware->reference_v, 0); // Select voltage

  /* Waiting for LCD Power Level Stabilisation. It doesn't say how long you
   * have to wait. 100 ms should be enough */
  mdelay(100);
  //END ONLY LCD
  }
                                                                
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

  mdelay (10);
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

#ifdef CONFIG_ARCH_S3C2410
  /* Wait 100ms, Apply Vcc */
  mdelay(100);
  s3c2410_gpio_cfgpin (psHardware->power_pin, S3C2410_GPIO_OUTPUT);
  s3c2410_gpio_pullup (psHardware->power_pin, 1);         // pullup off
  s3c2410_gpio_setpin (psHardware->power_pin, power_level);         // pin low
#endif

  /* Display ON */
  cycle (RS_CONTROL, CTL_DISPLAY_ON, 0);
}

static void test_pattern(void)
{
  int i,j,k;
  for (k=0; k<5; k++)
  {
    tm13264cbcg_clear_screen();
    for (j=0; j<3; j++)
    {
      set_display_position(8, 0, j);
      for (i=0; i<psHardware->iWidth_p; i++)
      {
        blit_column(0xff);
        tm13264cbcg_redraw_screen();
        mdelay(20);
      }
    }
  }
}

/****************************************************************************
 * Module cleanup
 ****************************************************************************/
static void __exit
tm13264cbcg_lcd_exit(void)
{
#ifdef CONFIG_ARCH_S3C2410
  if (psHardware && (psHardware->power_fail_present))
    free_irq (IRQ_EINT1, &tm13264cbcg_power_fail);
#endif
  kfree(sBitmap.buffer);
  reciva_lcd_exit();
}

#ifdef CONFIG_ARCH_MV88W8618

#define ISA_DATA_PORT           0x1c0
#define ISA_INSTRUCTION_PORT    0x1bc
#define ISA_AUX_CONTROL         0x1ac

static volatile void *lcd_base;

/****************************************************************************
 * Waits for ISA bus FIFO to be empty
 ****************************************************************************/
static void fifo_drain(void)
{
  int stat;
  do {
    stat = *((volatile unsigned int *)(lcd_base + 0x184));
  } while ((stat & (1 << 22)) == 0);
}

/****************************************************************************
 * Initialise the "GPIO" (actually ISA port) for use
 ****************************************************************************/
static void setup_gpio (void)
{
  unsigned int reg_data;
       
  lcd_base = (unsigned long)ioremap_nocache(0x9000c000,0x1d0);
  reg_data =  0x00100001;
  *(volatile unsigned long *)(lcd_base + ISA_AUX_CONTROL) = reg_data;       
  reg_data =  0x00100009;
  *(volatile unsigned long *)(lcd_base + ISA_AUX_CONTROL) = reg_data;       
  msleep(10);
  reg_data =  0x00100001;
  msleep(10);
  *(volatile unsigned long *)(lcd_base + ISA_AUX_CONTROL) = reg_data;       
  reg_data =  0x00100000;
  *(volatile unsigned long *)(lcd_base + 0x184) = reg_data;       
  reg_data =  0x1d1d0101;
  *(volatile unsigned long *)(lcd_base + 0x1a8) = reg_data;       
}

/****************************************************************************
 * Sends one instruction to the LCD module.
 * iRS - level of RS signal (register select - RS_DATA or RS_CONTROL)
 * iData - data to be written
 * iDelay - delay in microseconds at end of cycle
 ****************************************************************************/
static void cycle (int iRS, int iData, int iDelay)
{
  if (iRS == RS_DATA)
  {
    *((volatile unsigned long *)(lcd_base + ISA_DATA_PORT)) = iData;

    if (iDelay)
      BUG ();
  }
  else
  {
    *((volatile unsigned long *)(lcd_base + ISA_INSTRUCTION_PORT)) = iData;
    fifo_drain ();
    udelay (iDelay);
  }
}

#endif

#ifdef CONFIG_ARCH_S3C2410

/****************************************************************************
 * Sets up GPIO pins
 ****************************************************************************/
static void setup_gpio (void)
{
  if (psHardware->parallel)
  {
    setup_gpio_8bit_interface();
  }
  else
  {
    setup_gpio_1bit_interface();
  }
}

/****************************************************************************
 * Sets up GPIO pins when using a 1 bit interface
 ****************************************************************************/
static void setup_gpio_1bit_interface(void)
{
  cycle = cycle_serial;

  s3c2410_gpio_cfgpin (psHardware->pin_cfg.serial.rs, S3C2410_GPIO_OUTPUT);
  s3c2410_gpio_setpin (psHardware->pin_cfg.serial.rs, 0);
  s3c2410_gpio_pullup (psHardware->pin_cfg.serial.rs, 1);

  s3c2410_gpio_cfgpin (psHardware->pin_cfg.serial.cs, S3C2410_GPIO_OUTPUT);
  s3c2410_gpio_setpin (psHardware->pin_cfg.serial.cs, 1);
  s3c2410_gpio_pullup (psHardware->pin_cfg.serial.cs, 1);

  s3c2410_gpio_cfgpin (psHardware->pin_cfg.serial.scl, S3C2410_GPIO_OUTPUT);
  s3c2410_gpio_setpin (psHardware->pin_cfg.serial.scl, 0);
  s3c2410_gpio_pullup (psHardware->pin_cfg.serial.scl, 1);

  s3c2410_gpio_cfgpin (psHardware->pin_cfg.serial.sda, S3C2410_GPIO_OUTPUT);
  s3c2410_gpio_setpin (psHardware->pin_cfg.serial.sda, 0);
  s3c2410_gpio_pullup (psHardware->pin_cfg.serial.sda, 1);

  /* BL_ON (GPC2) */
  rutl_regwrite((1 << 2), (0 << 2), (int)S3C2410_GPCDAT);    // Output high
  rutl_regwrite((1 << 2), (0 << 2), (int)S3C2410_GPCUP);     // Disable pullup
  rutl_regwrite((1 << 4), (3 << 4), (int)S3C2410_GPCCON);    // Set as output

  /* nRESET (GPC3) */
  rutl_regwrite((0 << 3), (1 << 3), (int)S3C2410_GPCDAT);    // Output low
  rutl_regwrite((1 << 3), (0 << 3), (int)S3C2410_GPCUP);     // Disable pullup
  rutl_regwrite((1 << 6), (3 << 6), (int)S3C2410_GPCCON);    // Set as output

  /* nReset low */
  mdelay(100);

  rutl_regwrite((1 << 3), (0 << 3), (int)S3C2410_GPCDAT);    // RESETB high (release reset)
}

/****************************************************************************
 * Sets up GPIO pins when using an 8 bit interface
 ****************************************************************************/
static void setup_gpio_8bit_interface(void)
{
  if (interface_8080)
    cycle = cycle_parallel_8080;
  else
    cycle = cycle_parallel;

  /* Tell GPIO module which GPIO pins we are using */
  rgpio_register("GPB2 GBC0 GPA12 (E)", acModuleName);
  rgpio_register("GPB3 GBC1 GPA13 (RW)", acModuleName);
  rgpio_register("GPB1 GBC4       (CS1B)", acModuleName);
  rgpio_register("GBC0-GPC5, GPC8-GPC15", acModuleName);

  /* LED
     Processor pin = K12 (GPH7). Active high. */
  rutl_regwrite((1 << 7),  (0 << 7), (int)S3C2410_GPHUP);   // Disable pullup
  rutl_regwrite((1 << 14), (3 << 14), (int)S3C2410_GPHCON); // Set as ouput
  tm13264cbcg_driver.set_led(RLED_MENU);       // Menu LED

  /* LDN0 (E - enable pulse)
     This connects to 3 processor pins to allow various alternate functions
     to be used.
     The processor pins are : F4 (GPB2), J4 (GPC0), E15 (GPA12)
                              + GPC6 on Stingray
     Using C0 for now. */
  /* Set up all pins as inputs */
  rutl_regwrite((0 << 24), (3 << 24), (int)S3C2410_GPACON);
  rutl_regwrite((0 << 4), (3 << 4), (int)S3C2410_GPBCON);
  rutl_regwrite((0 << 0), (3 << 0), (int)S3C2410_GPCCON);
  rutl_regwrite((0 << 0), (3 << 12), (int)S3C2410_GPCCON);
  /* Disable pullups */
  // GPA doesn't have a pullup -  rutl_regwrite((1 << 12), 0, GPAUP);
  rutl_regwrite((1 << 2), 0, (int)S3C2410_GPBUP);
  rutl_regwrite((1 << 6) | (1 << 0), 0, (int)S3C2410_GPCUP);
  /* Use GPC0/GPC6 */
  if (machine_is_rirm3())
  {
    rutl_regwrite((0 << 6), (1 << 6), (int)S3C2410_GPCDAT); // Output low
    rutl_regwrite((1 << 12), (3 << 12), (int)S3C2410_GPCCON); // Set as output
  }
  else
  {
    rutl_regwrite((0 << 0), (1 << 0), (int)S3C2410_GPCDAT); // Output low
    rutl_regwrite((1 << 0), (3 << 0), (int)S3C2410_GPCCON); // Set as output
  }

  /* LDN1 (RW - read/write)
     This connects to 3 processor pins to allow various alternate functions
     to be used.
     The processor pins are : G3 (GPB3), J2 (GPC1), E16 (GPA13)
     Using G3 for now. */
  /* Set up unused pins as inputs */
  rutl_regwrite((0 << 26), (3 << 26), (int)S3C2410_GPACON);
  rutl_regwrite((0 << 6), (3 << 6), (int)S3C2410_GPBCON);
  rutl_regwrite((0 << 2), (3 << 2), (int)S3C2410_GPCCON);
  /* Disable pullups */
  // GPA doesn't have a pullup - rutl_regwrite((1 << 13), 0, GPAUP);
  rutl_regwrite((1 << 3), 0, (int)S3C2410_GPBUP);
  rutl_regwrite((1 << 1), 0, (int)S3C2410_GPCUP);
  /* Use GPC1 */
  /* Write = high on some modules */
  rutl_regwrite((0 << 1), (1 << 1), (int)S3C2410_GPCDAT);  // low
  rutl_regwrite((1 << 2), (3 << 2), (int)S3C2410_GPCCON);  // Set as output
  
  /* LDN2 (RS) - connected to J6 (GPC2) */
  rutl_regwrite((1 << 2), (0 << 2), (int)S3C2410_GPCUP);   // Disable pullup
  rutl_regwrite((0 << 2), (1 << 2), (int)S3C2410_GPCDAT);  // Output low
  rutl_regwrite((1 << 4), (3 << 4), (int)S3C2410_GPCCON);  // Set as output

  /* LDN3 (RESETB) - connected to K4 (GPC3) */
  rutl_regwrite((1 << 3), (0 << 3), (int)S3C2410_GPCUP);   // Disable pullup
  rutl_regwrite((0 << 3), (1 << 3), (int)S3C2410_GPCDAT);  // Output low
  rutl_regwrite((1 << 6), (3 << 6), (int)S3C2410_GPCCON);  // Set as output

  /* LDN4 (CS1B) - connected to F1 (GPB1) and K2 (GPC4) using K2 */
  /* Set up unused pins as inputs */
  if (psHardware->dont_use_cs)
  {
    // CS tied high/low
  }
  else
  {
    rutl_regwrite((1 << 4), (0 << 4), (int)S3C2410_GPCUP);   // Disable pullup
    rutl_regwrite((1 << 1), (0 << 1), (int)S3C2410_GPBUP);   // Disable pullup
    rutl_regwrite((0 << 2), (3 << 2), (int)S3C2410_GPBCON);  // Set as input
    rutl_regwrite((0 << 4), (1 << 4), (int)S3C2410_GPCDAT);  // Output low
    rutl_regwrite((1 << 8), (3 << 8), (int)S3C2410_GPCCON);  // Set as output
  }

  /* LCD Data - LCDD0 - LCDD7 (GPC8 to GPC15) */
  rutl_regwrite(((1 << 8) | (1 << 9) | (1 << 10) | (1 << 11)|
                 (1 << 12) | (1 << 13) | (1 << 14) | (1 << 15)),
                 0,
                 (int)S3C2410_GPCUP);   // Disable pullups
  rutl_regwrite( 0,
                 ((1 << 8) | (1 << 9) | (1 << 10) | (1 << 11)|
                 (1 << 12) | (1 << 13) | (1 << 14) | (1 << 15)),
                 (int)S3C2410_GPCDAT);   // Set outputs low
  rutl_regwrite( ((1 << 16) | (1 << 18) | (1 << 20) | (1 << 22) |
                 (1 << 24) | (1 << 26) | (1 << 28) | (1 << 30) ),
                 ((3 << 16) | (3 << 18) | (3 << 20) | (3 << 22) |
                 (3 << 24) | (3 << 26) | (3 << 28) | (3 << 30) ),
                 (int)S3C2410_GPCCON);   // Set as outputs

  /* Note that for ALTO hardware all LCD gpio output pins should be set
   * low at this point as they affect the power if set high */

  /* LDN5 (POWER) - connected to K6 (GPC5) */
  if (psHardware->power_off_by_reset)
  {
    s3c2410_gpio_cfgpin (psHardware->power_pin, S3C2410_GPIO_OUTPUT);
    s3c2410_gpio_pullup (psHardware->power_pin, 1);         // pullup off
    s3c2410_gpio_setpin (psHardware->power_pin, !power_level);         // pin low
  }
  else
  {
    /* Need to wait until we have sent a Power OFF command to display
     * before enabling this power (15V) */
  }

  // LDN4 (CS1B)
  s3c2410_gpio_setpin (psHardware->pin_cfg.parallel.cs, 1);     // High

  s3c2410_gpio_setpin (psHardware->pin_cfg.parallel.rw, psHardware->rw_high ? 1 : 0);

  /* Hold RESETB low */
  // XXX how long delay needed we can read a status bit
  mdelay(100);
  s3c2410_gpio_setpin (psHardware->reset_pin, 1);               // release reset
}

/****************************************************************************
 * Clock one byte of data out to LCD module serially.
 * iRS - level of RS signal (register select - RS_DATA or RS_CONTROL)
 * iData - data to be written
 * iDelay - delay in microseconds at end of cycle
 ****************************************************************************/
static void cycle_serial (int iRS, int iData, int iDelay)
{
  /* CS low */
  s3c2410_gpio_setpin (psHardware->pin_cfg.serial.cs, 0);

  int i=0;
  for (i=7; i>=0; i--)
  {
    /* RS/A0 - GPC0
     * CS    - GPG3
     * SDA   - GPG6
     * SCL   - GPG7 */

    /* Set up RS (A0) */
    s3c2410_gpio_setpin (psHardware->pin_cfg.serial.rs, iRS);

    /* Set up data (SDA) */
    s3c2410_gpio_setpin (psHardware->pin_cfg.serial.sda, (iData >> i) & 0x1);

    /* SCL high */
    s3c2410_gpio_setpin (psHardware->pin_cfg.serial.scl, 1);

    /* Wait > 60 ns 
     * Don't need an explicit delay here as we can't toggle gpio pins faster 
     * than 125ns */

    /* SCL low */
    s3c2410_gpio_setpin (psHardware->pin_cfg.serial.scl, 0);
  }
 
  /* CS high */
  s3c2410_gpio_setpin (psHardware->pin_cfg.serial.cs, 1);

  udelay (iDelay);
}

#define RIRM_LCDCN_DAT S3C2410_GPCDAT

#define RIRM_LCDCN0_PIN  (1 << 0)
#define RIRM_LCDCN1_PIN  (1 << 1)
#define RIRM_LCDCN2_PIN  (1 << 2)
#define RIRM_LCDCN3_PIN  (1 << 3)
#define RIRM_LCDCN4_PIN  (1 << 4)
#define RIRM_LCDCN5_PIN  (1 << 5)

# define LCD_RW    RIRM_LCDCN1_PIN
# define LCD_RS    RIRM_LCDCN2_PIN
# define LCD_RESET RIRM_LCDCN3_PIN
# define LCD_CS1   RIRM_LCDCN4_PIN
# define LCD_POWER RIRM_LCDCN5_PIN

/****************************************************************************
 * Sends one instruction to the LCD module.
 * iRS - level of RS signal (register select - RS_DATA or RS_CONTROL)
 * iData - data to be written
 * iDelay - delay in microseconds at end of cycle
 ****************************************************************************/
static void cycle_parallel (int iRS, int iData, int iDelay)
{
  // FIXME The plan was to always use cycle_parallel2, but it broke some configs 
  // (eg cf870). Fee free to work out what's wrong
  if (use_custom_gpio)
  {
    cycle_parallel2 (iRS, iData, iDelay);
    return;
  }

  unsigned int temp = __raw_readl(S3C2410_GPCDAT);

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
  if (!psHardware->greyscale)
  {
    udelay(3);
  }
  
  /* Set E high and set up data */
  temp |= lcd_e;  /* E = '1' */
  __raw_writel (temp, RIRM_LCDCN_DAT);

  /* Wait 250 ns (data sheet specifies 55 ns, ALTO hardware needs 250ns) */
  int delay_ns = 250;
  if (psHardware->greyscale)
  {
    delay_ns = 50;
  }
  ndelay(delay_ns);

  /* Set E low, CS1B high */
  temp &= ~lcd_e;  /* E = '0' */
  temp |= LCD_CS1;  /* CS1B = '1' */
  __raw_writel (temp, RIRM_LCDCN_DAT);

  /* Data must still be valid for 13ns after setting E low
   * Again, don't need an explicit delay here as next GPIO write will
   * take longer than this */

  udelay (iDelay);
}

/****************************************************************************
 * Sends one instruction to the LCD module - 8080-series
 * iRS - level of RS signal (register select - RS_DATA or RS_CONTROL)
 * iData - data to be written
 * iDelay - delay in microseconds at end of cycle
 ****************************************************************************/
static void cycle_parallel_8080 (int iRS, int iData, int iDelay)
{
  unsigned int temp = __raw_readl(S3C2410_GPCDAT);

  /* GPC8 - GPC15             : LCD module data D0 - D7
   * GPC0 (and GPB2, GPA12)   : (LCDCN0) E (RD) (read strobe)
   * GPC1 (and GPB3, GPA13)   : (LCDCN1) RW (WR) (write strobe)
   * GPC2                     : (LCDCN2) RS (== D/C) (Register Select)
   * GPC3                     : (LCDCN3) RESETB
   * GPC4 (and GPB1)          : (LCDCN4) CS1B
   * GPC5                     : (LCDCN5) POWER */

  // Set up data
  // D/C (data command)
  temp &= ~(0x0000ff00);
  temp |= (iData  << 8);
  if (iRS == RS_DATA)
    temp |= LCD_RS;   /* RS = '1' */
  else
    temp &= ~LCD_RS;  /* RS = '0' */
  temp &= ~LCD_CS1;    /* CS1B = '0' */
  // RD = 1
  // WR = 1
  // CS = 1
  temp |= lcd_e;  /* E = '1' */
  temp |= LCD_RW;
  temp |= LCD_CS1;
  __raw_writel (temp, RIRM_LCDCN_DAT);

  // RD = 1
  // WR = 0
  // CS = 0
  temp |= lcd_e;  /* E = '1' */
  temp &= ~LCD_RW;
  temp &= ~LCD_CS1;
  __raw_writel (temp, RIRM_LCDCN_DAT);

  /* Wait 250 ns (data sheet specifies 55 ns, ALTO hardware needs 250ns) */
  int delay_ns = 250;
  ndelay(delay_ns);

  // RD = 1
  // WR = 1
  // CS = 1
  temp |= lcd_e;  /* E = '1' */
  temp |= LCD_RW;
  temp |= LCD_CS1;
  __raw_writel (temp, RIRM_LCDCN_DAT);

  udelay (iDelay);
}

/****************************************************************************
 * Sends one instruction to the LCD module.
 * iRS - level of RS signal (register select - RS_DATA or RS_CONTROL)
 * iData - data to be written
 * iDelay - delay in microseconds at end of cycle
 ****************************************************************************/
static void cycle_parallel2 (int iRS, int iData, int iDelay)
{
  /* Note that LCD module 'owns' all GPC GPIO pins so we don't need to
   * protect against anyone else modifying them under our feet */

  /* GPC8 - GPC15             : LCD module data D0 - D7
   * GPC0 (and GPB2, GPA12)   : (LCDCN0) E (enable pulse)
   * GPC1 (and GPB3, GPA13)   : (LCDCN1) RW (Read/Write)
   * GPC2                     : (LCDCN2) RS (== D/C) (Register Select)
   * GPC3                     : (LCDCN3) RESETB
   * GPC4 (and GPB1)          : (LCDCN4) CS1B
   * GPC5                     : (LCDCN5) POWER */

  /* Set up data */
  unsigned int temp = __raw_readl(S3C2410_GPCDAT);
  temp &= ~(0x0000ff00);
  temp |= (iData  << 8);
  __raw_writel (temp, RIRM_LCDCN_DAT);

  /* Set up RS (GPC2) and CS1B (GPC4) low */
  s3c2410_gpio_setpin (psHardware->pin_cfg.parallel.rs, (iRS == RS_DATA) ? 1 : 0);
  s3c2410_gpio_setpin (psHardware->pin_cfg.parallel.cs, 0);

  /* Wait 3us (data sheet specifies 17 ns, ALTO hardware need 3 us)
   * Note : the fastest we can toggle GPIO bits is approx 125ns 
   * (measured on scope 20050305) */
  if (!psHardware->greyscale)
  {
    udelay(3);
  }
  
  /* Set E high and set up data */
  s3c2410_gpio_setpin (psHardware->pin_cfg.parallel.e, 1);

  /* Wait 250 ns (data sheet specifies 55 ns, ALTO hardware needs 250ns) */
  int delay_ns = 250;
  if (psHardware->greyscale)
  {
    delay_ns = 50;
  }
  ndelay(delay_ns);

  /* Set E low, CS1B high */
  s3c2410_gpio_setpin (psHardware->pin_cfg.parallel.e, 0);
  s3c2410_gpio_setpin (psHardware->pin_cfg.parallel.cs, 1);

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
#ifdef ENABLE_READ
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
  temp |= lcd_e;  /* E = '1' */
  __raw_writel (temp, RIRM_LCDCN_DAT);

  /* Wait 250 ns (data sheet specifies 55 ns, ALTO hardware needs 250ns) */
  ndelay(250);

  /* Read the data */
  unsigned int data = __raw_readl(GPCDAT);
  data >>= 8;
  data &= 0xff;

  /* Set E low, CS1B high */
  temp &= ~lcd_e;  /* E = '0' */
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
#endif

/****************************************************************************
 * Turns power off
 ****************************************************************************/
static void tm13264cbcg_power_off()
{
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

// Put data in so that shift >> up, shift << down
// This assumes we never want to write more than 32 bits (or 24 bits shifted 7)
void blit_column(u64 u64Data)
{
  int iOffset = sPosition.iByteOffset;
  int iBits = sPosition.iHeight_p;
  u64 u64Mask = sPosition.u32Mask;
  sPosition.iByteOffset += sBitmap.iHeight_b;

  if (sPosition.iShift)
  {
    u64Data <<= sPosition.iShift;
    u64Mask <<= sPosition.iShift;
  }

  if (u64Mask & (0xffffffff00000000LL))
  {
    printk("warning, mask overflowed beyond 32 bits (%16llx %16llx)\n",
           u64Mask, u64Data);
  }

  u32 u32Data = (u32)u64Data;
  u32 u32Mask = (u32)u64Mask;

  while (iBits > 0)
  {
    iBits -= 8;
    u32 u32ByteMask = (u32Mask >> iBits) & 0xff;
    if (iOffset >= (psHardware->iWidth_p * sBitmap.iHeight_b))
    { 
      // Tried to write past end of bitmap buffer
      printk("BUG: %s : Tried to write past end of bitmap buffer\n", __FUNCTION__);
      printk("  sPosition.iByteOffset = %d\n", sPosition.iByteOffset); // 1052
      printk("  sPosition.iHeight_p = %d\n", sPosition.iHeight_p);     // 16
      printk("  sPosition.u32Mask = %08x\n", sPosition.u32Mask);       // 0x0000ffff
      printk("  sBitmap.iHeight_b = %d\n", sBitmap.iHeight_b);         // 8
      printk("  sPosition.iShift = %d\n", sPosition.iShift);           // 0
      printk("  u32Data = %08x\n", u32Data);                           // 0x00000000
      printk("  u32Mask = %d\n", u32Mask);                             // 0x0000ffff
      printk("  u32ByteMask = %08x\n", u32ByteMask);                   // 0x000000ff
      printk("  iOffset = %d\n", iOffset);                             // 1044 
      printk("  psHardware->iWidth_p = %d\n", psHardware->iWidth_p);   // 130
      printk("  sBitmap.iHeight_b = %d\n", sBitmap.iHeight_b);         // 8
      printk("  iBits = %d\n", iBits);         // 8
      break;
    }
    sBitmap.buffer[iOffset] &= ~u32ByteMask;
    sBitmap.buffer[iOffset] |= (u32Data >> iBits) & u32ByteMask;
    iOffset++;
  }
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
                                   int iCursorType, int iCursorWidth, 
                                   int *piArrows, int *piLineContents)
{
  int iLine;
  int ret = 0;
  InvertStatus_t tInverted = NOT_INVERTED;
  
  bDrawSeparator = 1;

  //printk(PFX "%s\n", __FUNCTION__);
  
  for (iLine = 0; iLine < psCurrentMode->iHeight_c; iLine++)
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
                          iCursorWidth,
                          tInverted, 
                          piArrows[iLine],
                          psCurrentMode);
        }
        else
        {
          ret = draw_line(iLine, 
                          acText[iLine], 
                          iX,
                          LCD_CURSOR_OFF,
                          iCursorWidth,
                          tInverted, 
                          piArrows[iLine],
                          psCurrentMode);
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

  tm13264cbcg_redraw_screen();
  
  return ret;
}

/****************************************************************************
 * Set the position
 ****************************************************************************/
static void set_display_position(int iHeight, int iColumn, int iRow)
{
  //printk(PFX "%s iHeight %d iColumn %d iRow %d\n", __FUNCTION__, iHeight, iColumn, iRow);

  sPosition.u32Mask = (1 << iHeight) - 1;
  int iPage = (iRow + psHardware->iYOffset_p) / 8;
  sPosition.iShift = -1 * ((iRow + psHardware->iYOffset_p) % 8);
  sPosition.iByteOffset = ((iColumn + psHardware->iXOffset_p) * sBitmap.iHeight_b) + iPage;
  sPosition.iHeight_p = ((iHeight + 7) / 8) * 8;
  sPosition.iShift += sPosition.iHeight_p - iHeight;
  if (sPosition.iShift < 0)
  {
    sPosition.iHeight_p += 8;
    sPosition.iShift += 8;
  }

#if 0
  printk(PFX "\tu32Mask %08x iShift %d iByteOffset %d iHeight_p %d\n",
                sPosition.u32Mask,
                sPosition.iShift,
                sPosition.iByteOffset,
                sPosition.iHeight_p);
#endif
}

/****************************************************************************
 * Clears the screen
 ****************************************************************************/
static void tm13264cbcg_clear_screen(void)
{
  printk(PFX "%s\n", __FUNCTION__);
  memset(sBitmap.buffer, '\0', (sBitmap.iHeight_b * psHardware->iWidth_p));
  tm13264cbcg_redraw_screen();  
}  

/****************************************************************************
 * Set the 'backlight' level
 * This function is only used if the backlight level is being used to set
 * the brightness of the 'black-and-white' bitmap on the greyscale display
 * see draw_bitmap_greyscale()
 * Also calls the standard backlight function which sets the PWM backlight
 ****************************************************************************/
static void tm13264cbcg_set_greyscale_backlight(int level)
{
  greyscale_backlight_level = level * MAX_GREYSCALE_BACKLIGHT / BL_DEFAULT_MAX_BACKLIGHT;
  if (greyscale_config == 0)
  {
    if (level > 0 && greyscale_backlight_level == 0)
      greyscale_backlight_level = 1;
  }

  if (greyscale_backlight_level > MAX_GREYSCALE_BACKLIGHT)
    greyscale_backlight_level = MAX_GREYSCALE_BACKLIGHT;
  else if (greyscale_backlight_level < 0)
    greyscale_backlight_level = 0;

  tm13264cbcg_redraw_screen();

  reciva_bl_set_backlight(level); // Standard PWM backlight
}

/****************************************************************************
 * Set the led state
 * XXX this wrapper function could probably be eliminated if the led code was
 * rewritten more sensibly.
 ****************************************************************************/
static void tm13264cbcg_set_led(unsigned int mask)
{
#ifdef CONFIG_ARCH_S3C2410
  reciva_led_set(mask);
#endif
}

/****************************************************************************
 * Set the LCD contrast
 ****************************************************************************/
static void tm13264cbcg_set_contrast(int level)
{
  if (!psHardware->set_contrast || level < 0)
  {
    return;
  }

  if (level < 0)
    level = 0;
  else if (level > 100)
    level = 100;

  /* Range is 0x00 to 0x3f */
  int temp = (level * 0x3f)/ 100;

  /* Reference Voltage Select (2 byte instruction) */
  cycle (RS_CONTROL, CTL_SET_REF_V, 0); // Select mode
  cycle (RS_CONTROL, temp, 0); 

  if (level != lcd_contrast_level)
    printk(PFX "CONTRAST l=%d t=%d\n", level, temp);

  lcd_contrast_level = level;
}

/****************************************************************************
 * Set lcd zoom status.
 ****************************************************************************/
static void tm13264cbcg_set_zoom(int iOn, struct lcd_resize *psNewSize)
{
  if (psCurrentMode->iZoomMode > -1)
  {
    if (iOn)
    {
      zoom_enabled = 1;
    }
    else  
    {
      zoom_enabled = 0;
    }
    psCurrentMode = &asLCDMode[psCurrentMode->iZoomMode];
    tm13264cbcg_clear_screen();
  }
    
  psNewSize->iHeight = psCurrentMode->iHeight_c;
  psNewSize->iWidth  = psCurrentMode->iWidth_c;
}

/****************************************************************************
 * Draws a clock digit
 ****************************************************************************/
static void draw_clock_digit(const u32 *pu32Data, int iWidth, int iDigit)
{
  if (iDigit < 0 || iDigit > 9)
  {
    iDigit = 0;
  }

  pu32Data += iDigit * iWidth;
  draw_32bit(pu32Data, iWidth, 0);
}  

/****************************************************************************
 * Functions to draw the AM/PM text
 ****************************************************************************/
#define COL_OFFSET_AMPM 112
static void draw_clock_am_pm(int iDrawAM, int iHeight)
{
  int *piAMPM = (iDrawAM) ? aiAM : aiPM;
  int iAMPMLen = (iDrawAM) ? iAMLen : iPMLen;

  if (iHeight == 16)
  {
    set_display_position(16, COL_OFFSET_AMPM, 8);
  }
  else
  {
    set_display_position(16, COL_OFFSET_AMPM, 24);
  }

  int i;
  for (i=0; i<iAMPMLen; i++)
  {
    if (iHeight == 16)
    {
      if (piAMPM[i] & (1 << 31))
      {
        draw_chinese_character_16x16(piAMPM[i] & ~(1 << 31), 0 /*no invert*/);
      }
      else
      {
        draw_character_16x8(piAMPM[i], 0 /*no invert*/);
      }
    }
    else
    {
      draw_character_8x6(piAMPM[i], 0);
    }
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
 * Draws the clock (CONFIG0 or CONFIG4). Time (hours, minutes) in big font 
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
 ****************************************************************************/
#define COL_OFFSET_24HR 12
#define COL_OFFSET_12HR 0
static void draw_clock_config0and4(struct lcd_draw_clock *psClockInfo,
                                  int i12HourFormat)
{
  int iHours = i12HourFormat ? iMake12Hour(psClockInfo->iHours) : psClockInfo->iHours;
  int iMinutes = psClockInfo->iMinutes;
  int iColumnOffset = i12HourFormat ? COL_OFFSET_12HR : COL_OFFSET_24HR;

  set_display_position(32, iColumnOffset, 8);

  if (psClockInfo->iMode & LCD_DRAW_CLOCK_BLANK)
  {
    draw_blank_columns(CLOCK_DIGIT_WIDTH, 0);
    draw_blank_columns(CLOCK_DIGIT_WIDTH, 0);
    draw_clock_digit(au32Font32x26+(CLOCK_DIGIT_WIDTH*10), CLOCK_COLON_WIDTH, 0);
    draw_blank_columns(CLOCK_DIGIT_WIDTH, 0);
    draw_blank_columns(CLOCK_DIGIT_WIDTH, 0);
  }
  else
  {
    // Draw time
    if (!i12HourFormat || (iHours > 9))
    {
      draw_clock_digit(au32Font32x26, CLOCK_DIGIT_WIDTH, iHours/10);
    }
    else
    {
      draw_blank_columns(CLOCK_DIGIT_WIDTH, 0);
    }
    draw_clock_digit(au32Font32x26, CLOCK_DIGIT_WIDTH, iHours%10);
    draw_clock_digit(au32Font32x26+(CLOCK_DIGIT_WIDTH*10), CLOCK_COLON_WIDTH, 0);
    draw_clock_digit(au32Font32x26, CLOCK_DIGIT_WIDTH, iMinutes/10);
    draw_clock_digit(au32Font32x26, CLOCK_DIGIT_WIDTH, iMinutes%10);
  }

  /* Draw the AM/PM text */
  if (i12HourFormat)
  {
    draw_clock_am_pm((psClockInfo->iHours < 12), 16);  
  }
}

/****************************************************************************
 * Draws the clock (CONFIG0). Time (hours, minutes) in big font with alarm
 * time in brackets.
 * 
 * For alarm clock:
 *   Ensure that the colon lines up with that of the main clock
 *   (Therfore need to adjust position when not drawing the first digit in
 *   12 hour mode)
 ****************************************************************************/
#define COL_OFFSET_ALARM 26
static void draw_clock_config0(struct lcd_draw_clock *psClockInfo,
                               int i12HourFormat)
{
  draw_clock_config0and4(psClockInfo, i12HourFormat);

  /* Draw alarm info (if required) */
  if (psClockInfo->iAlarmOn)
  {
    int iColumnOffset = i12HourFormat ? COL_OFFSET_12HR : COL_OFFSET_24HR;
    draw_alarm_time(i12HourFormat, iColumnOffset,
                    psClockInfo->iAlarmHours, psClockInfo->iAlarmMinutes);
  }

  tm13264cbcg_redraw_screen();
}

/****************************************************************************
 * Draws the clock (CONFIG4). Time (hours, minutes) in big font with icons
 * for alarm/snooze.
 ****************************************************************************/
static void draw_clock_config4(struct lcd_draw_clock *psClockInfo,
                               int i12HourFormat)
{
  draw_clock_config0and4(psClockInfo, i12HourFormat);

  /* Draw alarm info (if required) */
  unsigned int bitmask = psClockInfo->iAlarmOn;
  tm13264cbcg_draw_icons(bitmask); // also calls draw_bitmap
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
   * different size fonts
  int iTopRowOld = iTopRow;
  int icon_size_old = icon_size;
  int screen_height_old = screen_height;
  int screen_width_old = screen_width;
  int font_size_old = font_size;
  */

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
  static struct display_mode sTimeMode =
    {
      .iHeight_c = 4,
      .iWidth_c = 8,
      .tFontSize = FONT_SIZE_32x16,
      .iTopRowOffset_p = 8,
      .pfDrawArrow = draw_arrow_none,
    };

  draw_line(0,                 // iLine
            time_string,       // pcText
            0,                 // iX
            LCD_CURSOR_OFF,    // iCursorType
            0,                 // iCursorWidth
            NOT_INVERTED,      // tInverted
            LCD_ARROW_NONE,    // iArrows
            &sTimeMode);

  /* Draw AM/PM text */
  if (i12HourFormat)
  {
    draw_clock_am_pm((psClockInfo->iHours < 12), 8);  
  }

  /* Draw date string */
  if (clock_disable_date == 0)
  {
  static struct display_mode sDateMode =
    {
      .iHeight_c = 3,
      .iWidth_c = 15,
      .tFontSize = FONT_SIZE_16x8,
      .iTopRowOffset_p = 32,
      .pfDrawIcon = draw_icon_8x8,
      .iIconPosition = 56,
      .pfDrawArrow = draw_arrow_none,
    };
  draw_line(0,                          // iLine
            psClockInfo->pcDateString,  // pcText
            0,                          // iX
            LCD_CURSOR_OFF,             // iCursorType
            0,                          // iCursorWidth,
            NOT_INVERTED,               // tInverted
            LCD_ARROW_NONE,             // iArrows
            &sDateMode);
  }

  /* Draw alarm info (if required) */
  unsigned int bitmask = psClockInfo->iAlarmOn;
  tm13264cbcg_draw_icons(bitmask); // also calls draw_bitmap

  /* Restore screen dimensions
  screen_height = screen_height_old;
  iTopRow = iTopRowOld;
  icon_size = icon_size_old;
  screen_height = screen_height_old;
  screen_width = screen_width_old;
  font_size = font_size_old;
  */
}

/****************************************************************************
 * Draws the clock (CONFIG2). Time (hours, minutes) in big font with
 * AM/PM/alarm icons on the right
 ****************************************************************************/
static void draw_clock_config2(struct lcd_draw_clock *psClockInfo,
                               int i12HourFormat)
{
  int iHours = psClockInfo->iHours;
  int iMinutes = psClockInfo->iMinutes;
  if (i12HourFormat)
  {
    iHours = iMake12Hour(iHours);
  }

  set_display_position(32, 0, 0);

  /* Blank initial column */
  draw_blank_columns(1, 0);

  /* Hours */
  if (!i12HourFormat || (iHours > 9))
  {
    draw_clock_digit(au32Font32x22, 22, iHours/10);
  }
  else
  {
    draw_blank_columns(22, 0);
  }
  draw_blank_columns(4, 0);
  draw_clock_digit(au32Font32x22, 22, iHours%10);
  draw_clock_digit(au32Font32x22+(22*10), 15, 0);
  draw_clock_digit(au32Font32x22, 22, iMinutes/10);
  draw_blank_columns(4, 0);
  draw_clock_digit(au32Font32x22, 22, iMinutes%10);

  /* Blank rest of display before drawing other stuff */
  draw_blank_columns(20, 0);

  /* Draw the AM/PM text */
  if (i12HourFormat)
  {
    set_display_position(16, 116, 16);
    draw_clock_icon_16x15((psClockInfo->iHours < 12) ? CLOCK_ICON_AM
                                                     : CLOCK_ICON_PM);  
  }

  /* Draw alarm info (if required) */
  if (psClockInfo->iAlarmOn)
  {
    set_display_position(16, 116, 0);
    draw_clock_icon_16x15(CLOCK_ICON_ALARM);
  }

  tm13264cbcg_redraw_screen();
}  

static void draw_clock_icon_16x15(int iIcon)
{
  u16 *u16Data = &au16ClockIcons16x15[iIcon*15];
  draw_16bit(u16Data, 15, 0);
}

/****************************************************************************
 * Draws the clock (CONFIG3).
 * LCD_CLOCK_CONFIG3 : time in 32x16 font
 *                     date in 16x8 font
 *                     alarm time in 16x8 font
 ****************************************************************************/
static void draw_clock_config3(struct lcd_draw_clock *psClockInfo,
                               int i12HourFormat)
{
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
  static struct display_mode sTimeMode =
    {
      .iHeight_c = 4,
      .iWidth_c = 8,
      .tFontSize = FONT_SIZE_32x16,
      .iTopRowOffset_p = 0,                 // GT was 8
      .pfDrawArrow = draw_arrow_none,
    };

  draw_line(0,                 // iLine
            time_string,       // pcText
            0,                 // iX
            LCD_CURSOR_OFF,    // iCursorType
            0,                 // iCursorWidth
            NOT_INVERTED,      // tInverted
            LCD_ARROW_NONE,    // iArrows
            &sTimeMode);

  /* Draw AM/PM text */
  if (i12HourFormat)
  {
    draw_clock_am_pm((psClockInfo->iHours < 12), 8);  
  }

  /* Draw date string */
  static struct display_mode sDateMode =
    {
      .iHeight_c = 3,
      .iWidth_c = 15,
      .tFontSize = FONT_SIZE_16x8,
      .iTopRowOffset_p = 28,               // GT was 32
      .pfDrawIcon = draw_icon_8x8,
      .iIconPosition = 56,
      .pfDrawArrow = draw_arrow_none,
    };
  draw_line(0,                          // iLine
            psClockInfo->pcDateString,  // pcText
            0,                          // iX
            LCD_CURSOR_OFF,             // iCursorType
            0,                          // iCursorWidth
            NOT_INVERTED,               // tInverted
            LCD_ARROW_NONE,             // iArrows
            &sDateMode);

  /* Draw alarm info (if required) */
  if (psClockInfo->iAlarmOn)
  {
    draw_alarm_time(i12HourFormat, 12,
                    psClockInfo->iAlarmHours, psClockInfo->iAlarmMinutes);
  }

  tm13264cbcg_redraw_screen();
}


/****************************************************************************
 * Draws the time on the screen in big font
 ****************************************************************************/
static int tm13264cbcg_draw_clock(struct lcd_draw_clock *psClockInfo)
{
  int i12HourFormat = ((psClockInfo->iMode & LCD_DRAW_CLOCK_HOURS_MASK) == LCD_DRAW_CLOCK_12HR)
                      && iAMLen && iPMLen;
  int iReturn = -ENOSYS;

  if (psCurrentMode->pfDrawClock != NULL)
  {
    psCurrentMode->pfDrawClock(psClockInfo, i12HourFormat);
    iReturn = 0;
  }

  return iReturn;
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
  int j;

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
  int iTimeLen = strlen(acTemp);
  set_display_position(16,
                       COL_OFFSET_ALARM+iColumnOffset, // ensure colons are alligned
                       48);
  
  /* UTF8-clean as long as numbers and : are in the same place! */
  for (j=0;j<iTimeLen; j++)
  {
    draw_character_16x8(acTemp[j], 0);
  }
  if (i12HourFormat)
  {
    for (j=0; j<iAMPMLen; j++)
    {
      if (piAMPM[j] & (1 << 31))
      {
        draw_chinese_character_16x16(piAMPM[j] & ~(1 << 31), 0);
      }
      else
      {
        draw_character_16x8(piAMPM[j], 0);
      }
    }
    draw_character_16x8(')', 0);
  }
}

/****************************************************************************
 * Draws a small clock in the status line area
 ****************************************************************************/
static void draw_small_clock(struct lcd_draw_small_clock *psClockInfo)
{
  char acTemp[20];
  int *piAMPM = NULL;
  int iAMPMLen = 0;
  int i,j;
  int iColumnOffset = 0;

  static int drew_12_hours;

  /* Set up the alarm string */
  if ((psClockInfo->iMode & LCD_DRAW_CLOCK_HOURS_MASK) != LCD_DRAW_CLOCK_12HR)
  {
    if (drew_12_hours)
    {
      // Make sure any remnants of the old time are erased

      set_display_position(12,
                           psClockInfo->iPosition+iColumnOffset - 16,
                           52);

      for (i=0; i<2; i++)
        draw_character_16x8_shifted (' ', 0, 3);
    }
    sprintf(acTemp, "%02d:%02d", psClockInfo->iHours, psClockInfo->iMinutes);
    drew_12_hours = 0;
  }
  else
  {
    piAMPM = (psClockInfo->iHours < 12) ? aiAM : aiPM;
    iAMPMLen = (psClockInfo->iHours < 12) ? iAMLen : iPMLen;
    int iHours_12 = iMake12Hour(psClockInfo->iHours);
    sprintf(acTemp, "%2d:%02d  ", iHours_12, psClockInfo->iMinutes);
    iColumnOffset -= 16;  // Make space for am/pm
    drew_12_hours = 1;
  }

  /* .. and draw it */
  int iTimeLen = strlen(acTemp);
  set_display_position(12,
                       psClockInfo->iPosition+iColumnOffset, // ensure colons are alligned
                       52);

  if (psClockInfo->iMode & LCD_DRAW_CLOCK_BLANK)
  {
    for (j=0;j<iTimeLen; j++)
    {
      draw_character_16x8_shifted (' ', 0, 3);
    }
    if ((psClockInfo->iMode & LCD_DRAW_CLOCK_HOURS_MASK) == LCD_DRAW_CLOCK_12HR)
    {
      for (j=0; j<iAMPMLen; j++)
      {
        draw_character_16x8_shifted (' ', 0, 3);
      }
    }
  }
  else
  {
    /* UTF8-clean as long as numbers and : are in the same place! */
    for (j=0;j<iTimeLen; j++)
    {
      draw_character_16x8_shifted(acTemp[j], 0, 3);
    }

    // Set position ready to draw AM/PM text 
    set_display_position(12,
                         psClockInfo->iPosition+iColumnOffset + (5*8), // ensure colons are alligned
                         52);

    if ((psClockInfo->iMode & LCD_DRAW_CLOCK_HOURS_MASK) == LCD_DRAW_CLOCK_12HR)
    {
      for (j=0; j<iAMPMLen; j++)
      {
        if (piAMPM[j] & (1 << 31))
        {
          if (j == 0)     // Only room for one Chinese character
          {
            int iOldByteOffset = sPosition.iByteOffset;
            set_display_position (16, 0, 48);
            sPosition.iByteOffset = iOldByteOffset;
            draw_chinese_character_16x16(piAMPM[j] & ~(1 << 31), 0);
          }
        }
        else
        {
          int iOldByteOffset = sPosition.iByteOffset;
          set_display_position (8, 0, 61);
          sPosition.iByteOffset = iOldByteOffset;
          draw_character_8x6(piAMPM[j], 0);
        }
      }
    }
  }

  tm13264cbcg_redraw_screen();
}

static int tm13264cbcg_draw_small_clock(struct lcd_draw_small_clock *psClockInfo)
{
  if (psCurrentMode->pfDrawSmallClock)
    {
      psCurrentMode->pfDrawSmallClock (psClockInfo);
      return 0;
    }

  return -ENOSYS;
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

static int tm13264cbcg_set_icon_slots(int nr)
{
  if (nr < 0 || nr > DEFAULT_MAX_ICONS)
    return -EINVAL;

  max_icons = nr;

  return 0;
}

static int tm13264cbcg_display_enable(int enabled)
{
  cycle (RS_CONTROL, enabled ? CTL_DISPLAY_ON : CTL_DISPLAY_OFF, 0);
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

    if (rfont_is_16x16(sUnicodeChar.iUnicode))
    {
      // Chinese character - check we have enough room for it
      if ((iScreenLength + 2) > iWidth)
      {
        break;
      }

      // Add character marking to use the double width font
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
                     int iCursorWidth,
                     InvertStatus_t tInverted, 
                     int iArrows,
                     struct display_mode *psMode)
{
#define MAX_SCREEN_WIDTH    256
  int aiLocalRep[MAX_SCREEN_WIDTH];
  int iCharLength;
  int iCurrentChar;

  /* Do UTF-8 lookup, string length and centre text */
  int iScreenLength = iDoCharLookupOnString(pcText, aiLocalRep,
                                        psMode->iWidth_c, &iCharLength);

  int iArrowWidth = psMode->pfDrawArrow(LCD_ARROW_NONE);
  int iBlankSpaces =   psHardware->iWidth_p - psHardware->iXOffset_p
                     - (iScreenLength * iFontWidth_p(psMode->tFontSize))
                     - (iArrowWidth * 2);
 
  /* Prepare draw functions for the font */
  void (*pfDrawChar)(int, int) = NULL;
  void (*pfDrawChineseChar)(int, int) = NULL;
  switch (psMode->tFontSize)
  {
    case FONT_SIZE_16x8:
      pfDrawChar = draw_character_16x8;
      pfDrawChineseChar = draw_chinese_character_16x16;
      break;
    case FONT_SIZE_32x16:
      pfDrawChar = draw_character_32x16;
      pfDrawChineseChar = draw_chinese_character_32x32;
      break;
    case FONT_SIZE_8x6:
      pfDrawChar = draw_character_8x6;
      pfDrawChineseChar = NULL;
      break;
    case FONT_SIZE_11x8:
      pfDrawChar = draw_character_11x8;
      pfDrawChineseChar = NULL;
      break;
  }

  // Prepare screen buffer
  int iHeight_p = iFontHeight_p(psMode->tFontSize);
  int iYPos_p = (iLine * iHeight_p) + psMode->iTopRowOffset_p;
  if (iYPos_p && psMode->iInverseBleed)
  {
    // Adjust height and position so that inverse will use the final
    // row of the line above (which should be blank)
    iYPos_p--;
    iHeight_p++;
  }
  set_display_position(iHeight_p, 0, iYPos_p);

  // Left arrow
	if ((iArrows == LCD_ARROW_LEFT || iArrows == LCD_ARROW_BOTH) && 
			 tInverted == NOT_INVERTED)
	{
		psMode->pfDrawArrow(LCD_ARROW_LEFT);
	}
	else
	{
		draw_blank_columns(iArrowWidth, tInverted);
	}

  /* Now draw the text, padded with spaces on either side if necessary */
  draw_blank_columns(iBlankSpaces/2, tInverted);
  for (iCurrentChar = 0; iCurrentChar < iCharLength; ++iCurrentChar)
  {
    /* Work out if we need to invert the character */
    int iInvert = 0;
    if (tInverted == INVERTED)
      iInvert = 1;
    if (iCursorType == LCD_CURSOR_ON)
    {
      if (iCurrentChar >= iX && iCurrentChar < (iX + iCursorWidth))
        iInvert ^= 1;
    }
    
    if (aiLocalRep[iCurrentChar] & (1 << 31))
    {
      if (pfDrawChineseChar != NULL)
      {
        pfDrawChineseChar(aiLocalRep[iCurrentChar] & ~(1 << 31), iInvert);
      }
    }
    else
    {
      pfDrawChar(aiLocalRep[iCurrentChar], iInvert);
    }
  }
  draw_blank_columns(iBlankSpaces - (iBlankSpaces/2), tInverted);

  // Right arrow
	if ((iArrows == LCD_ARROW_RIGHT || iArrows == LCD_ARROW_BOTH) && 
			 tInverted == NOT_INVERTED)
	{
		psMode->pfDrawArrow(LCD_ARROW_RIGHT);
	}
	else
	{
		draw_blank_columns(iArrowWidth, tInverted);
	}

  return 0;
}

static int draw_arrow_8x5(int iArrowType)
{
  if (iArrowType != LCD_ARROW_NONE)
  {
		char cData = (iArrowType == LCD_ARROW_LEFT) ? '<' : '>';
		const unsigned char *pcFontData = &aucFontData_8x6[(cData*6)+1];
		draw_8bit(pcFontData, 5, 0);
  }
  return 5;
}

/****************************************************************************
 * Draw a 16 pixel high arrow. Autodetect width
 ****************************************************************************/
static int draw_arrow_16xX(int iArrowType)
{
  int iFontWidth = iFontWidth_p(psCurrentMode->tFontSize);
  int iWidth = (psHardware->iWidth_p -psHardware->iXOffset_p - (psCurrentMode->iWidth_c * iFontWidth))/2;
  if (iWidth < 0)
    iWidth = 0;
  else if (iWidth > 6) 
    iWidth = 6;

  if (iArrowType != LCD_ARROW_NONE)
  {
    draw_16bit((iArrowType == LCD_ARROW_LEFT) ? au16LeftArrowLookup : au16RightArrowLookup+(6-iWidth), iWidth, 0);
  }
  return iWidth;
}

static int draw_arrow_16x6(int iArrowType)
{
  return draw_arrow_16xX(iArrowType);
}

static int draw_arrow_16x5(int iArrowType)
{
  return draw_arrow_16xX(iArrowType);
}

static int draw_arrow_none(int iArrowType)
{
  return 0;
}

/****************************************************************************
 * Draws a character from the 8x6 font.
 * cData - character to be drawn
 * iInvert - invert character
 ****************************************************************************/
static void draw_character_8x6(int iData, int iInvert)
{
  const unsigned char *pcFontData = &aucFontData_8x6[iData*6];
  draw_8bit(pcFontData, 6, iInvert);
}

/****************************************************************************
 * Draws a character from the main font.
 * cData - character to be drawn
 * See draw_character for more details
 ****************************************************************************/
static void draw_character_16x8(int iData, int iInvert)
{
  draw_16bit(&au16FontData[iData * 8], 8, iInvert);
}

static void draw_character_16x8_shifted(int iData, int iInvert, int iShift)
{
  draw_16bit_shifted(&au16FontData[iData * 8], 8, iInvert, iShift);
}

/****************************************************************************
 * Draws a character character from the Chinese font.
 * iData - character to be drawn
 * See draw_character for more details
 ****************************************************************************/
static void draw_chinese_character_16x16(int iData, int iInvert)
{
  draw_16bit((const u16 *)rfont_lookup_16x16(iData), 16, iInvert);
}

static void draw_character_11x8(int iData, int iInvert)
{
  draw_16bit(&au16FontData_11x8[iData * 8], 8, iInvert);
}

/****************************************************************************
 * Draws the icons
 ****************************************************************************/
static void tm13264cbcg_draw_icons(unsigned int uiBitmask)
{
  /* Not allowed to draw the icons in certain modes */
  if (psCurrentMode->pfDrawIcon == NULL)
  {
    return;
  }

  int iIconCount = 0;
  int iIconID = 0;
  int iHeight = (psCurrentMode->pfDrawIcon == draw_icon_8x8) ? 8 : 16;
      
  set_display_position(iHeight, 2, psCurrentMode->iIconPosition);
        
  while (   (iIconCount < max_icons)
         && uiBitmask
         && (iIconID < sizeof(aiIconMap)/sizeof(aiIconMap[0])))
  {
    if ((uiBitmask & 1) && aiIconMap[iIconID])
    {  
      psCurrentMode->pfDrawIcon(aiIconMap[iIconID]);
      iIconCount++;
    }
    iIconID++;
    uiBitmask >>= 1;
  }

  /* Make sure we blank any remaining Icon positions */
  for (; iIconCount<max_icons; iIconCount++)
  {
    psCurrentMode->pfDrawIcon(0);
  }

  /* For 8 bit icons draw a horizontal line to separate icons from the menu */
  if (iHeight == 8 && bDrawSeparator)
  {
    int iLinePosition = psCurrentMode->iIconPosition - 5;
    if (iLinePosition < 8)
    {
      iLinePosition = psCurrentMode->iIconPosition + 12;
    }
    if (psCurrentMode->iIconLinePosition >= 0)
      iLinePosition = psCurrentMode->iIconLinePosition;

    set_display_position(1, 0, iLinePosition);
  
    int i;
    for (i=0; i<psHardware->iWidth_p; i+=2)  
    {  
      blit_column(1);
      blit_column(0);
    }
  }

  tm13264cbcg_redraw_screen();
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
  printk(PFX "%s lcd_display_mode old mode %d new mode %d\n",
              __FUNCTION__, lcd_display_mode, iMode);

  if (iMode >= (sizeof(asLCDMode)/sizeof(asLCDMode[0])))
  {
    printk(PFX "%s illegal display mode\n", __FUNCTION__);
    return;
  }

  lcd_display_mode = iMode;
  psCurrentMode = &asLCDMode[lcd_display_mode];
  tm13264cbcg_clear_screen();
}


/****************************************************************************
 * Draws an Icon. The page and column must have been set up correctly
 * before calling this.
 ****************************************************************************/
static void draw_icon_16x16(int iIcon)
{
  if (iIcon >= MAX_ICON_ID)
    return;

  draw_16bit(&au16IconData[iIcon*16], 14, 0);
}

/****************************************************************************
 * Draws an Icon (8x8). The page and column must have been set up correctly
 * before calling this.
 ****************************************************************************/
static void draw_icon_8x8(int iIcon)
{
  if (iIcon >= MAX_ICON_ID)
    return;

  draw_8bit(&au8IconData8x8[iIcon*8], 8, 0);

  /* Draw 2 (typically) blank columns just to space them out a bit */
  int i;
  for (i = 0; i < icon_spacing; i++)
    blit_column(0);
}

static void draw_8bit(const u8 *pu8Data, int iWidth, int iInvert)
{
  int i;
  u32 iInvertMask = iInvert ? -1 : 0;
  for (i=0; i<iWidth; i++)
  {
    blit_column(pu8Data[i] ^ iInvertMask);
  }
}

static void draw_16bit(const u16 *pu16Data, int iWidth, int iInvert)
{
  int i;
  u32 iInvertMask = iInvert ? -1 : 0;
  for (i=0; i<iWidth; i++)
  {
    blit_column(pu16Data[i] ^ iInvertMask);
  }
}

static void draw_16bit_shifted(const u16 *pu16Data, int iWidth, int iInvert, int iShift)
{
  int i;
  u32 iInvertMask = iInvert ? -1 : 0;
  for (i=0; i<iWidth; i++)
  {
    blit_column((pu16Data[i] ^ iInvertMask) >> iShift);
  }
}

static void draw_32bit(const u32 *pu32Data, int iWidth, int iInvert)
{
  int i;
  u32 iInvertMask = iInvert ? -1 : 0;
  for (i=0; i<iWidth; i++)
  {
    blit_column(pu32Data[i] ^ iInvertMask);
  }
}

/****************************************************************************
 * Draws a zoomed (32x16) character from the main font.
 * cData - character to be drawn
 * See draw_zoomed_character for more details
 ****************************************************************************/
static void draw_character_32x16(int iData, int iInvert)
{
  draw_zoomed_character(&au16FontData[iData * 8], 8, iInvert);
}

/****************************************************************************
 * Draws a zoomed (32x32) character from the chinese font.
 * iData - character to be drawn
 * See draw_zoomed_character for more details
 ****************************************************************************/
static void draw_chinese_character_32x32(int iData, int iInvert)
{
  draw_zoomed_character((const u16 *)rfont_lookup_16x16(iData), 16, iInvert);
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
                                  int iInvert)
{
  int i, j;

  for (i=0; i < iWidth; i++)
  {
    u32 u32StretchedData = 0;
    for (j=0; j<16; j+=4)
    {
      /* We need to stretch the 16x8 font data so we can use it for 32x16 font */
      u32StretchedData |= stretch4to8((pFontData[i] >> j) & 0xf) << (j*2);
    }

    if (iInvert)
      u32StretchedData = ~u32StretchedData;

    /* We're stretching the width as well - draw 2 identical columns */
    blit_column(u32StretchedData);
    blit_column(u32StretchedData);
  }
}

/****************************************************************************
 * Draws specified number of half spaces. The number of pixels to draw is
 * calculated based on the current font size
 * The page and column must have been set up correctly before calling this.
 * iCount - number of half spaces to draw
 * tInverted - indicates if it should be inverted
 ****************************************************************************/
static int iFontWidth_p(FontSize_t tFontSize)
{
  switch (tFontSize)
  {
    case FONT_SIZE_16x8:  return 8;  break;
    case FONT_SIZE_32x16: return 16; break;
    case FONT_SIZE_8x6:   return 6;  break;
    case FONT_SIZE_11x8:  return 8;  break;
  }
        
  return 0;
}

static int iFontHeight_p(FontSize_t tFontSize)
{
  switch (tFontSize)
  {
    case FONT_SIZE_16x8:  return 16;  break;
    case FONT_SIZE_32x16: return 32; break;
    case FONT_SIZE_8x6:   return 8;  break;
    case FONT_SIZE_11x8:  return 11;  break;
  }
        
  return 0;
}

/****************************************************************************
 * Draws the specified number of blank columns
 * iCount = number of columns to draw
 * tInvertStatus - indicates if column should be inverted
 ****************************************************************************/
static void draw_blank_columns(int iCount, InvertStatus_t tInvertStatus)
{
  u32 u32Data = (tInvertStatus == INVERTED) ? sPosition.u32Mask : 0;
  
  while (iCount > 0)
  {
    blit_column(u32Data);
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
  if (psCurrentMode->pfDrawIcon == NULL
      || psCurrentMode->iNoSignalStrength)
  {
    return;
  }
  
  if (iLevel < 0)
    iOn = 0;

  if (iLevel > 100)
    iLevel = 100;
  iBars = (iLevel * (SIG_STRENGTH_BARS+1))/100;
  if (iBars > SIG_STRENGTH_BARS)
    iBars = SIG_STRENGTH_BARS;
  
  // Adjust position if using large icons
  int iBarYPos = psCurrentMode->iIconPosition;
  if (psCurrentMode->pfDrawIcon == draw_icon_16x16)
  {
    iBarYPos += 8;
  }

  set_display_position(8,
                       psHardware->iWidth_p-((SIG_STRENGTH_BARS+1)*6),
                       iBarYPos);

  for (i=0; i<=SIG_STRENGTH_BARS; i++)
  {
    if (iOn)
    {
      if (iBars >= i)
        draw_8bit(&au8SignalStrength8x6[i*6], 6, 0);
      else
        draw_blank_columns(6, 0);
    }
    else
    {
      draw_blank_columns(6, 0);
    }
  }
  tm13264cbcg_redraw_screen();
}  

/* Grab a region of the screen
 * Note that this routine hasn't been tested since the re-write, so there
 * is a high probability that it doesn't work, or will return the data in
 * an unexpected format */
static struct bitmap_data tm13264cbcg_grab_screen_region(int left, int
top, int width, int height)
{
  int graphics_width = psHardware->iWidth_p;
  int graphics_height = sBitmap.iHeight_b * 8;
  struct bitmap_data result = new_bitmap(0, 0, graphics_width, graphics_height);
  memcpy(result.data, sBitmap.buffer, (graphics_width * graphics_height)/8 + 1);
  return result;
}

/* Draw a bitmap on the screen
 * Note that this routine hasn't been tested since the re-write, so there
 * is a high probability that it doesn't work, or will the data input data
 * is in the wrong format */
static void tm13264cbcg_draw_bitmap(struct bitmap_data sBitmapInfo)
{
  int iHeight_b = (sBitmapInfo.height + 7) / 8;
  int iHeight_p = sBitmapInfo.height;
  int iRow = 0;
  while (iHeight_p > 0)
  {
    int iOffset = iRow;
    int iColumn;
    set_display_position((iHeight_p > 8) ? 8 : iHeight_p, sBitmapInfo.left, sBitmapInfo.top);
    for (iColumn=sBitmapInfo.left; iColumn<sBitmapInfo.left+sBitmapInfo.width; iColumn++)
    {
      blit_column(((char *)sBitmapInfo.data)[iOffset]);
      iOffset += iHeight_b;
    }
    iRow++;
    iHeight_p -= 8;
    sBitmapInfo.top += 8;
  }
  bDrawSeparator = 0;
  tm13264cbcg_redraw_screen();
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
static void tm13264cbcg_redraw_screen(void)
{
  //printk(PFX "%s\n", __FUNCTION__);
  if (!iHardwareInitialised)
  {
    return;
  }

  // Some units (eg cf752) seem to lose their contrast setting after
  // an overnight test
  // Attempt to avoid this
  if (rewrite_contrast)
    tm13264cbcg_set_contrast(lcd_contrast_level);

  if (psHardware->greyscale)
  {
    draw_bitmap_greyscale();
  }
  else
  {
    draw_bitmap();
  }
}

static void draw_bitmap(void)
{
  //printk(PFX "%s\n", __FUNCTION__);

  int rowmin = psHardware->iYOffset_p / 8;
  int rowmax = sBitmap.iHeight_b;
  int k,j;
  unsigned char data;
  int iHardwareRow;

  for (j=rowmin; j<rowmax; j++)
  {
    // Set hardware row
    iHardwareRow = psHardware->piRowLookup[j];
    cycle (RS_CONTROL, (CTL_SET_PAGE | (iHardwareRow & 0x0f)), 0);
    //printk( PFX "iHardwareRow %d, cycle %p\n", iHardwareRow, cycle);
  
    // Set hardware column
    cycle (RS_CONTROL, CTL_SET_COL_MSB | ((psHardware->iXOffset_p >> 4) & 0x0f), 0);
    cycle (RS_CONTROL, CTL_SET_COL_LSB | (psHardware->iXOffset_p & 0x0f), 0);
      
    int iOffset = (psHardware->iXOffset_p * sBitmap.iHeight_b) + j;
    for(k=0; k<psHardware->iWidth_p; k++)
    {
      data = sBitmap.buffer[iOffset];
      if (psHardware->iFlip)
         data = flip(data);
      cycle(RS_DATA, data, 0);
      iOffset += sBitmap.iHeight_b;
    }
  }
}

/****************************************************************************
 * Get pixel at specified coords
 * 0,0 = top left
 * Returns non zero if pixel is set
 ****************************************************************************/
// static int get_pixel(int x, int y)
// {
//   int x_byte_offset = (psHardware->iXOffset_p + x) * sBitmap.iHeight_b;
//   int y_byte_offset = y/8;
//   int offset = x_byte_offset + y_byte_offset;
//   int y_bit_offset = y % 8;
//   int pixel = 0;
//   unsigned char data;
// 
//   if (offset < sBitmap.bufsize)
//   {
//     data = sBitmap.buffer[offset];
//     if (data & (1 << (7 - y_bit_offset)))
//       pixel = 1;
//   }
// 
//   return pixel;
// }

/* case LCD_TM13264CBCG_MODE_SSD0323_128x64:
 * 4 bit greyscale so it's organised differently.
 * Row 0: top row of pixels
 * Row 64: bottom row of pixels 
 * An 8 bit data write will write 2 pixels from left to right */
static void draw_bitmap_greyscale(void)
{
  int rowmin = psHardware->iYOffset_p / 8;
  int rowmax = sBitmap.iHeight_b;
  int i,k;

  for (i=rowmin; i<(rowmax*8); i++)
  {
    /* Set column address */
    cycle(RS_CONTROL, 0x15, 0);
    cycle(RS_CONTROL, (psHardware->iXOffset_p / 2), 0);
    cycle(RS_CONTROL, 0x3F, 0);

    /* Set row address */
    cycle(RS_CONTROL, 0x75, 0);
    cycle(RS_CONTROL, i, 0);
    cycle(RS_CONTROL, 0x3F, 0);

    int iOffset = (psHardware->iXOffset_p * sBitmap.iHeight_b) + (i/8);
    int iShift = i % 8;
    if (psHardware->iFlip)
    {
      iShift = 7 - iShift;
    }
    //printk(PFX "i %d iShift %d iOffset %d\n", i, iShift, iOffset);
    for (k=0; k<psHardware->iWidth_p; k+=2)
    {
      unsigned char iData = 0;
      if (sBitmap.buffer[iOffset] & (1 << iShift))
      {
        iData = (greyscale_backlight_level << 4) & 0xF0;
      }
      iOffset += sBitmap.iHeight_b;
      if (sBitmap.buffer[iOffset] & (1 << iShift))
      {
        iData |= greyscale_backlight_level & 0x0F;
      }
      iOffset += sBitmap.iHeight_b;

      cycle(RS_DATA, iData, 0); // 2 pixels
    }
  }
}

static int tm13264cbcg_get_graphics_height(void)
{
  return psHardware->iHeight_p;
}

static int tm13264cbcg_get_graphics_width(void)
{
  return psHardware->iWidth_p;
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

static void setup_bitmap(struct display_bitmap *psNewBitmap)
{
  if (psNewBitmap->buffer)
  {
    kfree(psNewBitmap->buffer);
  }

  psNewBitmap->iHeight_b = (psHardware->iHeight_p + psHardware->iYOffset_p + 7) / 8;
  psNewBitmap->bufsize = psHardware->iWidth_p * psNewBitmap->iHeight_b;
  psNewBitmap->buffer = kmalloc(psNewBitmap->bufsize, GFP_KERNEL);

  printk(PFX "%s iHeight_b %d buffer %p\n", __FUNCTION__,
              psNewBitmap->iHeight_b, psNewBitmap->buffer);
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
  int iBlankCols;
  int iFontHeight = iFontHeight_p(psCurrentMode->tFontSize);

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

  /* Calculate unused columns
   * Barcode: 16 pixels/char plus start (16) and stop (15 without space) */
  iBlankCols = psHardware->iWidth_p - ((iCodeLength * 16) + 31);
  if (iBlankCols < 0)
  {
    return -EINVAL;
  }

  /* Display the bar code */
  set_display_position(iFontHeight, 0, (iLine * iFontHeight) + psCurrentMode->iTopRowOffset_p);

  draw_blank_columns(iBlankCols >> 1, NOT_INVERTED);

  draw_barcode_char(BAR_STARTSTOP_CHAR);
  draw_blank_columns(1, NOT_INVERTED);

  for (iCodeIndex=0; iCodeIndex < iCodeLength; iCodeIndex++)
  {
    draw_barcode_char(aiCode[iCodeIndex]);
    draw_blank_columns(1, NOT_INVERTED);
  }

  draw_barcode_char(BAR_STARTSTOP_CHAR);

  draw_blank_columns(iBlankCols - (iBlankCols >> 1), NOT_INVERTED);

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
  u32 u32Colour = sPosition.u32Mask; /* start with a black bar */
  while (iMask != 0)
  {
    blit_column(u32Colour);
    if (iMask & iCode)
    {
      blit_column(u32Colour);
      blit_column(u32Colour);
    }
    u32Colour ^= sPosition.u32Mask;
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
  send_command (calgary_contrast_control);

  send_command (calgary_current_range);

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

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
EXPORT_NO_SYMBOLS;
#endif

MODULE_AUTHOR("John Stirling <js@reciva.com>");
MODULE_DESCRIPTION("Reciva TM13264CBCG LCD driver");
MODULE_LICENSE("GPL");
