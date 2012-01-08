/*
 * Reciva LCD driver code for ST7066 (or compatible) character mapped
 * LCD module
 *
 * Copyright (c) 2004-2007 Reciva Ltd
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

/* There are a lot of LCD modules that are controlled in pretty much the same
 * way. This parameter defines the actual LCD module in use so that any minor
 * differences can be handled. If no module parameter is specified then it will
 * default to MODULE_4BIT
 * 0 = "standard" display, 4 bit interface, 16*2 chars
 * 1 = "standard" display, 8 bit interface, 16*2 chars
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
 * 12 = cf990 - 2 line, 4 bit (driver IC = Sitronix ST7036)
 * 16 = same as MODULE_HIPSHING_LC1641SRNH6_CF997 but only 2 lines
 * 17 = ST7032 driver with specific set-up
 * 18 = cf719 - 4 lines of 20 characters, 4 bits interface
 */
typedef enum
{
  MODULE_4BIT                              = 0,
  MODULE_8BIT                              = 1,
  MODULE_SITRONIX_ST7032                   = 2,
  MODULE_HIPSHING_LC1641SRNH6              = 3,
  MODULE_AMAX_EDC162A40GXBBB               = 4,      /* Driver IC = SPLC780D */
  MODULE_4BIT_1LINE                        = 5,
  MODULE_HEM1601B                          = 6,
  MODULE_8BIT_1LINE                        = 7,
  MODULE_SDEC_S2P16                        = 8,
  MODULE_HIPSHING_LC1641SRNH6_CF997        = 9,
  MODULE_CF979                             = 10,
  MODULE_CF989                             = 11,
  MODULE_CF990                             = 12,     // XXX Same as CF979?     
  MODULE_CF947                             = 13,
  MODULE_CF959                             = 14,
  MODULE_CF936                             = 15,
  MODULE_CF928                             = 16,
  MODULE_CF772                             = 17,
  MODULE_FUTABA_M204_SD02FJ                = 18,
  MODULE_SPLC780D_4BIT                     = 19,
} module_id_t;

static module_id_t lcd_module_id = MODULE_4BIT;

static int pins_shared;
static int use_power_control = 1;
static int power_control_active = 1;
static unsigned long lcd_e1 = (1 << 0);

   /*************************************************************************/

/* Functions in lcd interface */
static void ateam_set_led(unsigned int mask);

/* static functions */
static void ateam_draw_icons_cf772 (unsigned int uiBitMask);
static void ateam_draw_volume_cf772 (int volume);
static void draw_icon_buffer (void);

   /*************************************************************************/
   /***                        Local defines                              ***/
   /*************************************************************************/

#define MAX_SCREEN_HEIGHT			4
#define MAX_EXTRA_CHAR_TABLES			4

#define CMD_CLEAR_DISPLAY		0x01
#define CMD_RETURN_HOME			0x02
#define CMD_ENTRY_MODE_SET		0x04
#define ENTRY_MODE_INCREMENT			0x2
#define ENTRY_MODE_SHIFT			0x1
#define CMD_DISPLAY_MODE_SET		0x08
#define DISPLAY_MODE_DISPLAY_ON			0x4
#define DISPLAY_MODE_CURSOR_ON			0x2
#define DISPLAY_MODE_CURSOR_BLINK		0x1
#define CMD_CURSOR_DISPLAY_SHIFT	0x10
#define CMD_FUNCTION_SET		0x20
#define FUNCTION_SET_DL                         0x10
#define CMD_SET_CGRAM_ADDRESS		0x40
#define CMD_SET_DDRAM_ADDRESS		0x80
#define CMD_SET_IRAM_ADDRESS		0x40

/* ST7070 extended commands */
#define FUNCTION_SET_ST7070_EXT_BIT             0x4
#define CMD_ST7070_COM_SEG_DIRECTION    0x40

#define A_E_SETUP_DELAY			20
#define D_E_SETUP_DELAY			20

#define PREFIX "RLA: "

   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/
   
struct soft_char
{
  unsigned char *utf8_char;
  unsigned char data[8];
};

struct lcm_data
{
  unsigned int bus_width;
  unsigned int display_width, display_height;
  unsigned int legacy_led : 1;
  const struct rutl_unicode_charmap *extra_chars[MAX_EXTRA_CHAR_TABLES];
};

   /*************************************************************************/
   /***                        Static data                                ***/
   /*************************************************************************/

static const char acModuleName[] = "Reciva LCD Ateam Generic";

#include "reciva_lcd_ateam_soft_font.c"

static const struct rutl_unicode_charmap asModule11Charmap[] = 
  {
    { LCD_RIGHT_ARROW_SOLID,	0x10 },
    { LCD_LEFT_ARROW_SOLID,	0x11 },
    { NULL, 0 },    
  };

static const struct rutl_unicode_charmap asLatin1Charmap[] = 
  {
    { "\xc2\xa1", 0xa1 }, //  INVERTED EXCLAMATION MARK
    { "\xc2\xa2", 0xa2 }, //  CENT SIGN
    { "\xc2\xa3", 0xa3 }, //  POUND SIGN
    { "\xc2\xa4", 0xa4 }, //  CURRENCY SIGN
    { "\xc2\xa5", 0xa5 }, //  YEN SIGN
    { "\xc2\xa6", 0xa6 }, //  BROKEN BAR
    { "\xc2\xa7", 0xa7 }, //  SECTION SIGN
    { "\xc2\xa8", 0xa8 }, //  DIAERESIS
    { "\xc2\xa9", 0xa9 }, //  COPYRIGHT SIGN
    { "\xc2\xaa", 0xaa }, //  FEMININE ORDINAL INDICATOR
    { "\xc2\xab", 0xab }, //  LEFT-POINTING DOUBLE ANGLE QUOTATION MARK
    { "\xc2\xac", 0xac }, //  NOT SIGN
    { "\xc2\xad", 0xad }, //  SOFT HYPHEN
    { "\xc2\xae", 0xae }, //  REGISTERED SIGN
    { "\xc2\xaf", 0xaf }, //  MACRON
    { "\xc2\xb0", 0xb0 }, //  DEGREE SIGN
    { "\xc2\xb1", 0xb1 }, //  PLUS-MINUS SIGN
    { "\xc2\xb2", 0xb2 }, //  SUPERSCRIPT TWO
    { "\xc2\xb3", 0xb3 }, //  SUPERSCRIPT THREE
    { "\xc2\xb4", 0xb4 }, //  ACUTE ACCENT
    { "\xc2\xb5", 0xb5 }, //  MICRO SIGN
    { "\xc2\xb6", 0xb6 }, //  PILCROW SIGN
    { "\xc2\xb7", 0xb7 }, //  MIDDLE DOT
    { "\xc2\xb8", 0xb8 }, //  CEDILLA
    { "\xc2\xb9", 0xb9 }, //  SUPERSCRIPT ONE
    { "\xc2\xba", 0xba }, //  MASCULINE ORDINAL INDICATOR
    { "\xc2\xbb", 0xbb }, //  RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK
    { "\xc2\xbc", 0xbc }, //  VULGAR FRACTION ONE QUARTER
    { "\xc2\xbd", 0xbd }, //  VULGAR FRACTION ONE HALF
    { "\xc2\xbe", 0xbe }, //  VULGAR FRACTION THREE QUARTERS
    { "\xc2\xbf", 0xbf }, //  INVERTED QUESTION MARK
    { "\xc3\x80", 0xc0 }, // �LATIN CAPITAL LETTER A WITH GRAVE
    { "\xc3\x81", 0xc1 }, // �LATIN CAPITAL LETTER A WITH ACUTE
    { "\xc3\x82", 0xc2 }, // �LATIN CAPITAL LETTER A WITH CIRCUMFLEX
    { "\xc3\x83", 0xc3 }, // �LATIN CAPITAL LETTER A WITH TILDE
    { "\xc3\x84", 0xc4 }, // �LATIN CAPITAL LETTER A WITH DIAERESIS
    { "\xc3\x85", 0xc5 }, // �LATIN CAPITAL LETTER A WITH RING ABOVE
    { "\xc3\x86", 0xc6 }, // �LATIN CAPITAL LETTER AE
    { "\xc3\x87", 0xc7 }, // �LATIN CAPITAL LETTER C WITH CEDILLA
    { "\xc3\x88", 0xc8 }, // �LATIN CAPITAL LETTER E WITH GRAVE
    { "\xc3\x89", 0xc9 }, // �LATIN CAPITAL LETTER E WITH ACUTE
    { "\xc3\x8a", 0xca }, // �LATIN CAPITAL LETTER E WITH CIRCUMFLEX
    { "\xc3\x8b", 0xcb }, // �LATIN CAPITAL LETTER E WITH DIAERESIS
    { "\xc3\x8c", 0xcc }, // �LATIN CAPITAL LETTER I WITH GRAVE
    { "\xc3\x8d", 0xcd }, // �LATIN CAPITAL LETTER I WITH ACUTE
    { "\xc3\x8e", 0xce }, // �LATIN CAPITAL LETTER I WITH CIRCUMFLEX
    { "\xc3\x8f", 0xcf }, // �LATIN CAPITAL LETTER I WITH DIAERESIS
    { "\xc3\x90", 0xd0 }, // �LATIN CAPITAL LETTER ETH
    { "\xc3\x91", 0xd1 }, // �LATIN CAPITAL LETTER N WITH TILDE
    { "\xc3\x92", 0xd2 }, // �LATIN CAPITAL LETTER O WITH GRAVE
    { "\xc3\x93", 0xd3 }, // �LATIN CAPITAL LETTER O WITH ACUTE
    { "\xc3\x94", 0xd4 }, // �LATIN CAPITAL LETTER O WITH CIRCUMFLEX
    { "\xc3\x95", 0xd5 }, // �LATIN CAPITAL LETTER O WITH TILDE
    { "\xc3\x96", 0xd6 }, // �LATIN CAPITAL LETTER O WITH DIAERESIS
    { "\xc3\x97", 0xd7 }, // �MULTIPLICATION SIGN
    { "\xc3\x98", 0xd8 }, // �LATIN CAPITAL LETTER O WITH STROKE
    { "\xc3\x99", 0xd9 }, // �LATIN CAPITAL LETTER U WITH GRAVE
    { "\xc3\x9a", 0xda }, // �LATIN CAPITAL LETTER U WITH ACUTE
    { "\xc3\x9b", 0xdb }, // �LATIN CAPITAL LETTER U WITH CIRCUMFLEX
    { "\xc3\x9c", 0xdc }, // �LATIN CAPITAL LETTER U WITH DIAERESIS
    { "\xc3\x9d", 0xdd }, // �LATIN CAPITAL LETTER Y WITH ACUTE
    { "\xc3\x9e", 0xde }, // �LATIN CAPITAL LETTER THORN
    { "\xc3\x9f", 0xdf }, // �LATIN SMALL LETTER SHARP S
    { "\xc3\xa0", 0xe0 }, // �LATIN SMALL LETTER A WITH GRAVE
    { "\xc3\xa1", 0xe1 }, // �LATIN SMALL LETTER A WITH ACUTE
    { "\xc3\xa2", 0xe2 }, // �LATIN SMALL LETTER A WITH CIRCUMFLEX
    { "\xc3\xa3", 0xe3 }, // �LATIN SMALL LETTER A WITH TILDE
    { "\xc3\xa4", 0xe4 }, // �LATIN SMALL LETTER A WITH DIAERESIS
    { "\xc3\xa5", 0xe5 }, // �LATIN SMALL LETTER A WITH RING ABOVE
    { "\xc3\xa6", 0xe6 }, // �LATIN SMALL LETTER AE
    { "\xc3\xa7", 0xe7 }, // �LATIN SMALL LETTER C WITH CEDILLA
    { "\xc3\xa8", 0xe8 }, // �LATIN SMALL LETTER E WITH GRAVE
    { "\xc3\xa9", 0xe9 }, // �LATIN SMALL LETTER E WITH ACUTE
    { "\xc3\xaa", 0xea }, // �LATIN SMALL LETTER E WITH CIRCUMFLEX
    { "\xc3\xab", 0xeb }, // �LATIN SMALL LETTER E WITH DIAERESIS
    { "\xc3\xac", 0xec }, // �LATIN SMALL LETTER I WITH GRAVE
    { "\xc3\xad", 0xed }, // �LATIN SMALL LETTER I WITH ACUTE
    { "\xc3\xae", 0xee }, // �LATIN SMALL LETTER I WITH CIRCUMFLEX
    { "\xc3\xaf", 0xef }, // �LATIN SMALL LETTER I WITH DIAERESIS
    { "\xc3\xb0", 0xf0 }, // �LATIN SMALL LETTER ETH
    { "\xc3\xb1", 0xf1 }, // �LATIN SMALL LETTER N WITH TILDE
    { "\xc3\xb2", 0xf2 }, // �LATIN SMALL LETTER O WITH GRAVE
    { "\xc3\xb3", 0xf3 }, // �LATIN SMALL LETTER O WITH ACUTE
    { "\xc3\xb4", 0xf4 }, // �LATIN SMALL LETTER O WITH CIRCUMFLEX
    { "\xc3\xb5", 0xf5 }, // �LATIN SMALL LETTER O WITH TILDE
    { "\xc3\xb6", 0xf6 }, // �LATIN SMALL LETTER O WITH DIAERESIS
    { "\xc3\xb7", 0xf7 }, // �DIVISION SIGN
    { "\xc3\xb8", 0xf8 }, //  LATIN SMALL LETTER O WITH STROKE
    { "\xc3\xb9", 0xf9 }, //  LATIN SMALL LETTER U WITH GRAVE
    { "\xc3\xba", 0xfa }, //  LATIN SMALL LETTER U WITH ACUTE
    { "\xc3\xbb", 0xfb }, //  LATIN SMALL LETTER U WITH CIRCUMFLEX
    { "\xc3\xbc", 0xfc }, //  LATIN SMALL LETTER U WITH DIAERESIS
    { "\xc3\xbd", 0xfd }, //  LATIN SMALL LETTER Y WITH ACUTE
    { "\xc3\xbe", 0xfe }, //  LATIN SMALL LETTER THORN
    { "\xc3\xbf", 0xff }, //  LATIN SMALL LETTER Y WITH DIAERESIS
    { NULL, 0 },
  };

static const struct rutl_unicode_charmap asST7066U_A_Charmap[] = 
  {
    { "\xc2\xa5", 0x4c }, //  YEN SIGN
    { "\xef\xbd\xa1", 0xa1 }, // HALFWIDTH IDEOGRAPHIC FULL STOP
    { "\xef\xbd\xa2", 0xa2 }, // HALFWIDTH LEFT CORNER BRACKET
    { "\xef\xbd\xa3", 0xa3 }, // HALFWIDTH RIGHT CORNER BRACKET
    { "\xef\xbd\xa4", 0xa4 }, // HALFWIDTH IDEOGRAPHIC COMMA
    { "\xef\xbd\xa5", 0xa5 }, // HALFWIDTH KATAKANA MIDDLE DOT
    { "\xef\xbd\xa6", 0xa6 }, // HALFWIDTH KATAKANA LETTER WO
    { "\xef\xbd\xa7", 0xa7 }, // HALFWIDTH KATAKANA LETTER SMALL A
    { "\xef\xbd\xa8", 0xa8 }, // HALFWIDTH KATAKANA LETTER SMALL I
    { "\xef\xbd\xa9", 0xa9 }, // HALFWIDTH KATAKANA LETTER SMALL U
    { "\xef\xbd\xaa", 0xaa }, // HALFWIDTH KATAKANA LETTER SMALL E
    { "\xef\xbd\xab", 0xab }, // HALFWIDTH KATAKANA LETTER SMALL O
    { "\xef\xbd\xac", 0xac }, // HALFWIDTH KATAKANA LETTER SMALL YA
    { "\xef\xbd\xad", 0xad }, // HALFWIDTH KATAKANA LETTER SMALL YU
    { "\xef\xbd\xae", 0xae }, // HALFWIDTH KATAKANA LETTER SMALL YO
    { "\xef\xbd\xaf", 0xaf }, // HALFWIDTH KATAKANA LETTER SMALL TU
    { "\xef\xbd\xb0", 0xb0 }, // HALFWIDTH KATAKANA-HIRAGANA PROLONGED SOUND MARK
    { "\xef\xbd\xb1", 0xb1 }, // HALFWIDTH KATAKANA LETTER A
    { "\xef\xbd\xb2", 0xb2 }, // HALFWIDTH KATAKANA LETTER I
    { "\xef\xbd\xb3", 0xb3 }, // HALFWIDTH KATAKANA LETTER U
    { "\xef\xbd\xb4", 0xb4 }, // HALFWIDTH KATAKANA LETTER E
    { "\xef\xbd\xb5", 0xb5 }, // HALFWIDTH KATAKANA LETTER O
    { "\xef\xbd\xb6", 0xb6 }, // HALFWIDTH KATAKANA LETTER KA
    { "\xef\xbd\xb7", 0xb7 }, // HALFWIDTH KATAKANA LETTER KI
    { "\xef\xbd\xb8", 0xb8 }, // HALFWIDTH KATAKANA LETTER KU
    { "\xef\xbd\xb9", 0xb9 }, // HALFWIDTH KATAKANA LETTER KE
    { "\xef\xbd\xba", 0xba }, // HALFWIDTH KATAKANA LETTER KO
    { "\xef\xbd\xbb", 0xbb }, // HALFWIDTH KATAKANA LETTER SA
    { "\xef\xbd\xbc", 0xbc }, // HALFWIDTH KATAKANA LETTER SI
    { "\xef\xbd\xbd", 0xbd }, // HALFWIDTH KATAKANA LETTER SU
    { "\xef\xbd\xbe", 0xbe }, // HALFWIDTH KATAKANA LETTER SE
    { "\xef\xbd\xbf", 0xbf }, // HALFWIDTH KATAKANA LETTER SO
    { "\xef\xbe\x80", 0xc0 }, // HALFWIDTH KATAKANA LETTER TA
    { "\xef\xbe\x81", 0xc1 }, // HALFWIDTH KATAKANA LETTER TI
    { "\xef\xbe\x82", 0xc2 }, // HALFWIDTH KATAKANA LETTER TU
    { "\xef\xbe\x83", 0xc3 }, // HALFWIDTH KATAKANA LETTER TE
    { "\xef\xbe\x84", 0xc4 }, // HALFWIDTH KATAKANA LETTER TO
    { "\xef\xbe\x85", 0xc5 }, // HALFWIDTH KATAKANA LETTER NA
    { "\xef\xbe\x86", 0xc6 }, // HALFWIDTH KATAKANA LETTER NI
    { "\xef\xbe\x87", 0xc7 }, // HALFWIDTH KATAKANA LETTER NU
    { "\xef\xbe\x88", 0xc8 }, // HALFWIDTH KATAKANA LETTER NE
    { "\xef\xbe\x89", 0xc9 }, // HALFWIDTH KATAKANA LETTER NO
    { "\xef\xbe\x8a", 0xca }, // HALFWIDTH KATAKANA LETTER HA
    { "\xef\xbe\x8b", 0xcb }, // HALFWIDTH KATAKANA LETTER HI
    { "\xef\xbe\x8c", 0xcc }, // HALFWIDTH KATAKANA LETTER HU
    { "\xef\xbe\x8d", 0xcd }, // HALFWIDTH KATAKANA LETTER HE
    { "\xef\xbe\x8e", 0xce }, // HALFWIDTH KATAKANA LETTER HO
    { "\xef\xbe\x8f", 0xcf }, // HALFWIDTH KATAKANA LETTER MA
    { "\xef\xbe\x90", 0xd0 }, // HALFWIDTH KATAKANA LETTER MI
    { "\xef\xbe\x91", 0xd1 }, // HALFWIDTH KATAKANA LETTER MU
    { "\xef\xbe\x92", 0xd2 }, // HALFWIDTH KATAKANA LETTER ME
    { "\xef\xbe\x93", 0xd3 }, // HALFWIDTH KATAKANA LETTER MO
    { "\xef\xbe\x94", 0xd4 }, // HALFWIDTH KATAKANA LETTER YA
    { "\xef\xbe\x95", 0xd5 }, // HALFWIDTH KATAKANA LETTER YU
    { "\xef\xbe\x96", 0xd6 }, // HALFWIDTH KATAKANA LETTER YO
    { "\xef\xbe\x97", 0xd7 }, // HALFWIDTH KATAKANA LETTER RA
    { "\xef\xbe\x98", 0xd8 }, // HALFWIDTH KATAKANA LETTER RI
    { "\xef\xbe\x99", 0xd9 }, // HALFWIDTH KATAKANA LETTER RU
    { "\xef\xbe\x9a", 0xda }, // HALFWIDTH KATAKANA LETTER RE
    { "\xef\xbe\x9b", 0xdb }, // HALFWIDTH KATAKANA LETTER RO
    { "\xef\xbe\x9c", 0xdc }, // HALFWIDTH KATAKANA LETTER WA
    { "\xef\xbe\x9d", 0xdd }, // HALFWIDTH KATAKANA LETTER N
    { "\xef\xbe\x9e", 0xde }, // HALFWIDTH KATAKANA VOICED SOUND MARK
    { "\xef\xbe\x9f", 0xdf }, // HALFWIDTH KATAKANA SEMI-VOICED SOUND MARK
    { "\xc3\xa4", 0xe1 }, // �LATIN SMALL LETTER A WITH DIAERESIS
    { "\xc3\xb6", 0xef }, // �LATIN SMALL LETTER O WITH DIAERESIS
    { "\xc3\xbc", 0xf5 }, //  LATIN SMALL LETTER U WITH DIAERESIS
    { "\xc3\xb7", 0xfd }, // �DIVISION SIGN
    { NULL, 0 },
  };

static const struct lcm_data lcm_data_array[] = 
  {
    /* MODULE_4BIT */
    { bus_width: 4, display_width: 16, display_height: 2, legacy_led: 1 },
    /* MODULE_8BIT */
    { bus_width: 8, display_width: 16, display_height: 2, legacy_led: 1 },
    /* MODULE_SITRONIX_ST7032 */
    { bus_width: 4, display_width: 16, display_height: 2, legacy_led: 1 },
    /* MODULE_HIPSHING_LC1641SRNH6 */
    { bus_width: 4, display_width: 16, display_height: 4, legacy_led: 1,
      extra_chars: { asST7066U_A_Charmap }, },
    /* MODULE_AMAX_EDC162A40GXBBB */
    { bus_width: 4, display_width: 16, display_height: 2, legacy_led: 1 },
    /* MODULE_4BIT_1LINE */
    { bus_width: 4, display_width: 16, display_height: 1 },
    /* MODULE_HEM1601B */
    { bus_width: 8, display_width: 16, display_height: 1 },
    /* MODULE_8BIT_1LINE */
    { bus_width: 8, display_width: 16, display_height: 1 },
    /* MODULE_SDEC_S2P16 */
    { bus_width: 8, display_width: 16, display_height: 2 },
    /* MODULE_HIPSHING_LC1641SRNH6_CF997 */
    { bus_width: 4, display_width: 16, display_height: 4 },
    /* MODULE_CF979 */
    { bus_width: 4, display_width: 16, display_height: 2, legacy_led: 1 },
    /* MODULE_CF989 */
    { bus_width: 4, display_width: 16, display_height: 2, legacy_led: 1,
      extra_chars: { asLatin1Charmap, asModule11Charmap } },
    /* MODULE_CF990 */
    { bus_width: 4, display_width: 16, display_height: 2, legacy_led: 1 },
    /* MODULE_CF947 */
    { bus_width: 4, display_width: 16, display_height: 2, legacy_led: 0 },
    /* MODULE_CF959 */
    { bus_width: 4, display_width: 16, display_height: 2, 
      extra_chars: { asLatin1Charmap, asModule11Charmap } },
    /* MODULE_CF936 */
    { bus_width: 1, display_width: 16, display_height: 2 },
    /* MODULE_CF928 */
    { bus_width: 4, display_width: 16, display_height: 2 },
    /* MODULE_CF772 */
    { bus_width: 1, display_width: 16, display_height: 2 },
    /* MODULE_FUTABA_M204_SD02FJ */
    { bus_width: 4, display_width: 20, display_height: 4 },
    /* MODULE_SPLC780D_4BIT */
    { bus_width: 4, display_width: 16, display_height: 2 },
  };

#define NR_LCMS ((sizeof (lcm_data_array) / sizeof (lcm_data_array[0])))

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
  asModule11Charmap,            // 0
  asLatin1Charmap,              // 1  
  asST7066U_A_Charmap,          // 2
};
static const struct rutl_unicode_charmap *extra_chars[MAX_EXTRA_CHAR_TABLES];

// Data for icon ram display
static char icon_buf[16];

struct icon
{
  const int addr;
  const int mask;
};

enum icons
{
  ICON_COLON_TOP,
  ICON_COLON_BOT,
  ICON_AM,
  ICON_PM,
  ICON_VOL_8,
  ICON_VOL_7,
  ICON_VOL_6,
  ICON_VOL_5,
  ICON_VOL_4,
  ICON_VOL_3,
  ICON_VOL_2,
  ICON_VOL_1,
  ICON_VOL_SPEAKER,
  ICON_ALARM1,
  ICON_ALARM2,
  ICON_SNOOZE,
  ICON_Z_SMALL,
  ICON_Z_MEDIUM,
  ICON_Z_BIG,
  ICON_CLOCK,
  ICON_FM,
  ICON_AUX,
  ICON_REPEAT,
  ICON_CD,
  ICON_SD,
  ICON_MP3,
  ICON_FOLDER,
  ICON_ALL,
  ICON_WMA,
  ICON_USB,
  ICON_STEREO,
  ICON_BRACKET_RIGHT_BIG,
  ICON_BRACKET_RIGHT_SMALL,
  ICON_BRACKET_LEFT_SMALL,
  ICON_BRACKET_LEFT_BIG,
  ICON_DAB,
};

static const struct icon icon_table[] =
{
  /* COLON TOP */ {0x6, 0x02},
  /* COLON BOT */ {0x6, 0x01},
  /* AM */        {0x3, 0x04},
  /* PM */        {0x3, 0x08},
  /* 8volume */   {0x0, 0x10},
  /* 7volume */   {0x0, 0x08},
  /* 6volume */   {0x0, 0x04},
  /* 5volume */   {0x0, 0x02},
  /* 4volume */   {0x0, 0x01},
  /* 3volume */   {0x1, 0x10},
  /* 2volume */   {0x1, 0x08},
  /* 1volume */   {0x1, 0x04},
  /* volumepic */ {0x1, 0x02},
  /* Alarm1 */    {0x1, 0x01},
  /* Alarm 2 */   {0x2, 0x01},
  /* snooze */    {0x2, 0x10},
  /* smallz */    {0x2, 0x08},
  /* mediumz */   {0x2, 0x04},
  /* bigz */      {0x2, 0x02},
  /* clockpic */  {0x3, 0x10},
  /* FM */        {0xC, 0x01},
  /* Aux */       {0xD, 0x10},
  /* Repeat */    {0xD, 0x08},
  /* CD */        {0xD, 0x04},
  /* SD */        {0xD, 0x02},
  /* MP3 */       {0xD, 0x01},
  /* Folder */    {0xE, 0x10},
  /* All */       {0xE, 0x08},
  /* WMA */       {0xE, 0x04},
  /* USB */       {0xE, 0x02},
  /* Stereo */    {0xE, 0x01},
  /* Big) */      {0xF, 0x10},
  /* Small) */    {0xF, 0x08},
  /* Small( */    {0xF, 0x04},
  /* Big( */      {0xF, 0x02},
  /* DAB */       {0xF, 0x01},
};

#define CLEAR_ICON(index) icon_buf[icon_table[index].addr] &= ~icon_table[index].mask
#define SET_ICON(index) icon_buf[icon_table[index].addr] |= icon_table[index].mask

struct icon_digit
{
  const int addr;
  const char data[10][3];
};

static const struct icon_digit icon_digit_data[] =
{
  // Digit 1
  {
    3,
    {
      /* 0    */ {3, 0x1a, 0x10},
      /* 1    */ {0, 2, 0x10},
      /* 2    */ {2, 0x19, 0x10},
      /* 3    */ {2, 0xB, 0x10},
      /* 4    */ {1, 3, 0x10},
      /* 5    */ {3, 0xB, 0},
      /* 6    */ {3, 0x1B, 0},
      /* 7    */ {3, 2, 0x10},
      /* 8    */ {3, 0x1B, 0x10},
      /* 9    */ {3, 0xB, 0x10},
    },
  },
  // Digit 2
  {
    5,
    {
      /* 0    */ {0xF, 0x14, 0},
      /* 1    */ {0, 0x14, 0},
      /* 2    */ {0xB, 0xC, 0},
      /* 3    */ {9, 0x1C, 0},
      /* 4    */ {4, 0x1C, 0},
      /* 5    */ {0xD, 0x18, 0},
      /* 6    */ {0xF, 0x18, 0},
      /* 7    */ {0xC, 0x14, 0},
      /* 8    */ {0xF, 0x1C, 0},
      /* 9    */ {0xD, 0x1C, 0},
    },
  },
  // Digit 3
  {
    0xA,
    {
      /* 0    */ {0x1F, 8, 0},
      /* 1    */ {1, 8, 0},
      /* 2    */ {0x16, 0x18, 0},
      /* 3    */ {0x13, 0x18, 0},
      /* 4    */ {9, 0x18, 0},
      /* 5    */ {0x1B, 0x10, 0},
      /* 6    */ {0x1F, 0x10, 0},
      /* 7    */ {0x19, 0, 0},
      /* 8    */ {0x1F, 0x18, 0},
      /* 9    */ {0x1B, 0x18, 0},
    },
  },
  // Digit 4
  {
    0xB,
    {
      /* 0    */ {7, 0x1A, 0},
      /* 1    */ {0, 0xA, 0},
      /* 2    */ {5, 0x16, 0},
      /* 3    */ {4, 0x1e, 0},
      /* 4    */ {2, 0xE, 0},
      /* 5    */ {6, 0x1C, 0},
      /* 6    */ {7, 0x1C, 0},
      /* 7    */ {6, 0xA, 0},
      /* 8    */ {7, 0x1e, 0},
      /* 9    */ {6, 0x1E, 0},
    },
  },
};

/* Screen dimensions */
static int screen_width;            // Screen width excluding arrows
static int screen_width_inc_arrows = -1; // Screen width including arrows
static int screen_height = -1;

static unsigned char *shadow_buffer;

/* Control interface width (4bit or 8bit) */
static int interface_width = -1;

/* Protect acces to cycle() function */
static spinlock_t cycle_lock = SPIN_LOCK_UNLOCKED;

static struct reciva_lcd_driver ateam_driver;
static int using_legacy_led = -1;

static int forced_redraw_in_progress;
static int icon_showing;
static int icons_enabled;
static int arrows_enabled = 1;
static int arrows_left_only;
static int cursor_blink;
static int really_no_arrows;

/****************************************************************************
 * Sends one 4 bit instruction to the LCD module.
 * iA0 - level of A0 signal
 * iData - data to be written - bits 3:0
 * iDelay - delay in microseconds at end of cycle
 ****************************************************************************/
static void cycle_4bit (int iA0, int iData, int iDelay)
{
  unsigned int temp = __raw_readl (S3C2410_GPCDAT);
  unsigned int a0_bit;

  if (lcd_module_id == MODULE_AMAX_EDC162A40GXBBB)
    a0_bit = 1 << 1;	// A0=GPC1
  else
    a0_bit = 1 << 2;	// A0=GPC2

  /* Set up A0 */
  if (iA0)
    temp |= a0_bit;
  else
    temp &= ~a0_bit;
  __raw_writel (temp, S3C2410_GPCDAT);
    
  udelay (A_E_SETUP_DELAY);

  /* Set E high (GPC0 */
  //temp |= (1 << 0);  /* E1 = '1' */
  temp |= lcd_e1;  /* E1 = '1' */
  __raw_writel (temp, S3C2410_GPCDAT);

  /* Set up data */
  temp &= ~(0x0000f000);
  temp |= (iData  << 12);
  __raw_writel (temp, S3C2410_GPCDAT);

  udelay (D_E_SETUP_DELAY);
  
  /* Set E low */
  //temp &= ~(1 << 0);  /* E1 = '1' */
  temp &= ~lcd_e1;  /* E1 = '1' */
  __raw_writel (temp, S3C2410_GPCDAT);

  udelay (20 + iDelay);
}

/****************************************************************************
 * Sends one 8 bit instruction to the LCD module.
 * iA0 - level of A0 signal
 * iData - data to be written - bits 7:0
 * iDelay - delay in microseconds at end of cycle
 ****************************************************************************/
static void cycle_8bit (int iA0, int iData, int iDelay)
{
  unsigned int temp = __raw_readl (S3C2410_GPCDAT);

  /* GPC8 - GPC15             : LCD module data D0 - D7
   * GPB2 (and GPC0, GPA12)   : (LDN0) LCD module E1 (E)
   * GPB3 (and GPC1, GPA13)   : (LDN1) LCD module E2  not used
   * GPC2                     : (LDN2) LCD module A0 (RS)
   * GPC3                     : (LDN3) RES
   * GPB1 (and GPC4)          : (LDN4) LCD brightness
   * GPC5                     : (LDN5) LCD power */

  /* Set up A0 (GPC2) */
  if (iA0)
    temp |= (1 << 2);   /* A0 = '1' */
  else
    temp &= ~(1 << 2);  /* A0 = '0' */

  __raw_writel (temp, S3C2410_GPCDAT);

  udelay (A_E_SETUP_DELAY);

  /* Set E high (GPC0 or GPC1) */
  //temp |= (1 << 0);  /* E1 = '1' */
  temp |= lcd_e1;  /* E1 = '1' */
  __raw_writel (temp, S3C2410_GPCDAT);

  /* Set up data */
  temp &= ~(0x0000ff00);
  temp |= (iData  << 8);
  __raw_writel (temp, S3C2410_GPCDAT);

  udelay (D_E_SETUP_DELAY);

  /* Set E low */
  //temp &= ~(1 << 0);  /* E1 = '1' */
  temp &= ~lcd_e1;  /* E1 = '1' */
  __raw_writel (temp, S3C2410_GPCDAT);

  udelay (20 + iDelay);
}

/****************************************************************************
 * Clock one byte of data out to LCD module serially.
 * iRS - level of RS signal (register select - RS_DATA or RS_CONTROL)
 * iData - data to be written
 * iDelay - delay in microseconds at end of cycle
 ****************************************************************************/
static void cycle_serial (int iRS, int iData, int iDelay)
{
  /* RS/A0 - LCDCN2 - GPC2
   * CS    - LCDD5  - GPC13
   * SCL   - LCDD6  - GPC14 
   * SI    - LCDD7  - GPC15 */
#define LCDD7 15
#define LCDD6 14
#define LCDD5 13
#define LCDD4 12

  /* CS low */
  rutl_regwrite((0 << LCDD5), (1 << LCDD5), (int)S3C2410_GPCDAT);

  /* Set up RS (A0) */
  if (iRS)
    rutl_regwrite((1 << 2), (0 << 2), (int)S3C2410_GPCDAT); // Set
  else
    rutl_regwrite((0 << 2), (1 << 2), (int)S3C2410_GPCDAT); // Clear

  int i=0;
  for (i=7; i>=0; i--)
  {
    /* SCL low */
    rutl_regwrite((0 << LCDD6), (1 << LCDD6), (int)S3C2410_GPCDAT); // Clear

    /* Set up data (SDA) */
    if ((iData>>i) & 0x01)
      rutl_regwrite((1 << LCDD7), (0 << LCDD7), (int)S3C2410_GPCDAT); // Set
    else
      rutl_regwrite((0 << LCDD7), (1 << LCDD7), (int)S3C2410_GPCDAT); // Clear

    udelay(1);

    /* SCL high */
    rutl_regwrite((1 << LCDD6), (0 << LCDD6), (int)S3C2410_GPCDAT); // Set

    udelay(1);
  }

  /* CS high */
  rutl_regwrite((1 << LCDD5), (0 << LCDD5), (int)S3C2410_GPCDAT);

  udelay (iDelay);
}

/****************************************************************************
 * Sends one 8 bit (or two 4 bit) instructions to the LCD module.
 * iA0 - level of A0 signal
 * iData - data to be written
 * iDelay - delay in microseconds at end of 8 bit cycle
 ****************************************************************************/
static void cycle (int iA0, int iData, int iDelay)
{
  unsigned long flags;

  spin_lock_irqsave (&cycle_lock, flags);

  switch (interface_width)
  {
    case 1:
      cycle_serial(iA0, iData, iDelay);
      break;
    case 4:
      cycle_4bit (iA0, (iData>>4), 0);    // Top 4 bits
      cycle_4bit (iA0, iData, iDelay);    // Lower 4 bits
      break;
    case 8:
      cycle_8bit (iA0, iData, iDelay);    
      break;
  }  

  spin_unlock_irqrestore (&cycle_lock, flags);
}

/****************************************************************************
 * Configure the shared GPIO pins ready for use
 ****************************************************************************/
static inline void setup_data_gpio(void)
{
  int con, data, pullup;
  int con_mask, data_mask;

  switch (interface_width)
    {
    case 8:
      con = (1 << 16) | (1 << 18) | (1 << 20) | (1 << 22) |
        (1 << 24) | (1 << 26) | (1 << 28) | (1 << 30);
      data = 0;  
      pullup = (1 << 8)  | (1 << 9)  | (1 << 10) | (1 << 11) |
        (1 << 12) | (1 << 13) | (1 << 14) | (1 << 15);
      
      con_mask = 0xffff;
      data_mask = 0xff;
      break;

    case 4:
      con = (1 << 24) | (1 << 26) | (1 << 28) | (1 << 30);
      data = 0;  
      pullup = (1 << 12) | (1 << 13) | (1 << 14) | (1 << 15);
      con_mask = 0xff00;
      data_mask = 0xf0;
      break;

    case 1:
      con =    (1 << 26) | (1 << 28) | (1 << 30);
      data =   (1 << 13) | (1 << 14) | (1 << 15);
      pullup = (1 << 13) | (1 << 14) | (1 << 15);
      con_mask = 0xffff;                // XXX
      data_mask = 0xff;                 // XXX
      break;

    default:
      BUG();
      return;
  }

  /* LCD Data - LCDD0 - LCDD7 (GPC8 to GPC15) */
  rutl_regwrite ( con, (con_mask << 16), (int)S3C2410_GPCCON);

  /* Output low */
  rutl_regwrite (data, (data_mask << 8), (int)S3C2410_GPCDAT);

  /* Disable pullups */
  rutl_regwrite (pullup, (data_mask << 8), (int)S3C2410_GPCUP);
}

/****************************************************************************
 * Put the shared GPIO pins into a safe configuration
 ****************************************************************************/
static inline void tristate_data_gpio(void)
{
  // Put GPC8-11 into tristate as they are shared with the
  // ROW inputs of the keypad (GPG8-11)
  // GPC12-15 are shared with the COL outputs, so don't need changing
  /* LCD Data - LCDD0 - LCDD7 (GPC8 to GPC15) */
  rutl_regwrite (0, (3 << 16) | (3 << 18) | (3 << 20) | (3 << 22), (int)S3C2410_GPCCON);
  rutl_regwrite (0, (1 << 8) | (1 << 9) | (1 << 10) | (1 << 11), (int)S3C2410_GPCUP); // enable pullups
}

/****************************************************************************
 * Get access to the shared GPIO pins
 ****************************************************************************/
static inline void get_shared_gpio(void)
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
static inline void release_shared_gpio(void)
{
  if (pins_shared)
  {
    tristate_data_gpio();
    rkg_access_release();
  }
}

/****************************************************************************
 * Set up GPIO pins
 ****************************************************************************/
static void setup_gpio (void)
{
  printk(PREFIX "%s\n", __FUNCTION__);

  if (using_legacy_led)
    {
      /* LED
         Processor pin = K12 (GPH7). Active high. */
      rutl_regwrite (4 << 14, 2 << 14, (int)S3C2410_GPHCON);
      rutl_regwrite (1 << 7, 0, (int)S3C2410_GPHUP);	// disable pullup
      rutl_regwrite (1 << 7, 0, (int)S3C2410_GPHDAT); // output on
      
      printk(PREFIX "using obsolescent lcd control code\n");
    }

  /* LDN0 (E1)
     This connects to 3 processor pins to allow various alternate functions
     to be used.
     The processor pins are : F4 (GPB2), J4 (GPC0), E15 (GPA12)
                              + GPC6 on Stingray
     Using F4 for now. */
  /* Set up unused pins as inputs */
  rutl_regwrite (0, 3 << 4, (int)S3C2410_GPBCON);
  rutl_regwrite (0, 3 << 0, (int)S3C2410_GPCCON);
  rutl_regwrite (0, 3 << 12, (int)S3C2410_GPCCON);

  /* no pullup disable for GPA */
  rutl_regwrite (1 << 2, 0, (int)S3C2410_GPBUP);
  if (interface_width != 1)
  {
    rutl_regwrite (1 << 0, 0, (int)S3C2410_GPCUP);
    /* Set up GPC0 as standard GPIO (might change this later) */
    rutl_regwrite (1 << 0, 0, (int)S3C2410_GPCCON);  // Set to output
    rutl_regwrite (0, 1 << 0, (int)S3C2410_GPCDAT);  // Output low

    rutl_regwrite (1 << 6, 0, (int)S3C2410_GPCUP);
    rutl_regwrite (1 << 12, 0, (int)S3C2410_GPCCON);  // Set to output
    rutl_regwrite (0, 1 << 6, (int)S3C2410_GPCDAT);  // Output low
  }

  /* LDN1 (E2)
     This connects to 3 processor pins to allow various alternate functions
     to be used.
     The processor pins are : G3 (GPB3), J2 (GPC1), E16 (GPA13)
     Using G3 for now. */
  /* Set up unused pins as inputs */
  rutl_regwrite (0, 3 << 6, (int)S3C2410_GPBCON);
  rutl_regwrite (0, 3 << 2, (int)S3C2410_GPCCON);
  /* disable pullups */
  rutl_regwrite (1 << 3, 0, (int)S3C2410_GPBUP);
  rutl_regwrite (1 << 1, 0, (int)S3C2410_GPCUP);
  /* Set up GPC1 as standard GPIO (might change this later) */
  rutl_regwrite (1 << 2, 0, (int)S3C2410_GPCCON);	// Set to output
  rutl_regwrite (0, 1 << 1, (int)S3C2410_GPCDAT);	// Output low

  /* LDN2 (A0) - connected to J6 (GPC2) */
  rutl_regwrite (1 << 4, 2 << 4, (int)S3C2410_GPCCON);	// output
  rutl_regwrite (0, 1 << 2, (int)S3C2410_GPCDAT);		// low
  rutl_regwrite (1 << 2, 0, (int)S3C2410_GPCUP);		// disable pullup

  if (use_power_control)        // XXX not really, but happens to be true in the same cases
    {
      /* LDN3 (RES) - connected to K4 (GPC3) */
      rutl_regwrite (1 << 6, 2 << 6, (int)S3C2410_GPCCON);
      if (   (lcd_module_id == MODULE_AMAX_EDC162A40GXBBB)
          || (lcd_module_id == MODULE_CF772))
      {
        /* reset is the other way up for some odd reason */
        rutl_regwrite (1 << 3, 0, (int)S3C2410_GPCDAT);	// output high
      }
      else
      {
        rutl_regwrite (0, 1 << 3, (int)S3C2410_GPCDAT);	// output low
      }
      rutl_regwrite (1 << 3, 0, (int)S3C2410_GPCUP);		// disable pullup
    }

  /* LDN4 (LCDBRIGHT) - connected to F1 (GPB1) and K2 (GPC4) using F1 
   * Now done via reciva_backligh*.o */

  /* If we aren't sharing the data pins, set them up now.  */
  if (! pins_shared)
    setup_data_gpio ();

  if (use_power_control)
  {
    /* LDN5 (LDC_PON) - connected to K6 (GPC5) */


    
    rutl_regwrite (1 << 10, 2 << 10, (int)S3C2410_GPCCON);
    rutl_regwrite (1 << 5, 0, (int)S3C2410_GPCUP);		// disable pullup
  }
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

  cycle (0, CMD_SET_CGRAM_ADDRESS | (slot << 3), 39); /* Set address */

  for (i = 0; i < 8; i++)
    cycle (1, data[i], 43);
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
 * Set address pointer for LCM write
 * x - x coord
 * y - y coord
 ****************************************************************************/
static inline void set_address (int x, int y)
{
  static const unsigned char line_start_16[] = { 0x00, 0x40, 0x10, 0x50 };
  static const unsigned char line_start_20[] = { 0x00, 0x40, 0x14, 0x54 };
  unsigned char addr;
  
  if (lcd_module_id == MODULE_HEM1601B)
    addr = line_start_16[y] + (x & 7) + ((x & 8) ? 0x40 : 0);
  else if (screen_width_inc_arrows == 20)
    addr = line_start_20[y] + x;
  else
    addr = line_start_16[y] + x;
         
  cycle (0, CMD_SET_DDRAM_ADDRESS | addr, 40);	/* Set address */
}

/****************************************************************************
 * Draws cursor at specified coords
 * x,y - cursor position
 ****************************************************************************/
static void draw_cursor (int x, int y)
{
  set_address (x, y);

  /* Cursor on */
  cycle (0, CMD_DISPLAY_MODE_SET | DISPLAY_MODE_DISPLAY_ON | DISPLAY_MODE_CURSOR_ON
         | (cursor_blink ? DISPLAY_MODE_CURSOR_BLINK : 0), 40);  
}  

/****************************************************************************
 * Set up the Function Set register
 * reinitialise - reinitialise
 ****************************************************************************/
static void init_function_set_register(int reinitialise)
{
  int dl_bit = FUNCTION_SET_DL;

  if (lcd_module_id == MODULE_AMAX_EDC162A40GXBBB && reinitialise == 0)
    {
      cycle_4bit (0, 0x03, 0);
      mdelay(20);
      cycle_4bit (0, 0x03, 0);
      mdelay(5);
      cycle_4bit (0, 0x03, 0);
      mdelay(1);
    }

  if (interface_width == 4)
  {
    /* 4 bit i/f 
     * Don't need to set this up again when recovering from ESD shock */
    if (reinitialise == 0)
    {
      cycle_4bit (0, 0x02, 0);
      if (lcd_module_id == MODULE_SPLC780D_4BIT)
        cycle_4bit (0, 0x02, 0);                // Datasheet says send this twice
    }

    dl_bit = 0;
  }
  
  switch (lcd_module_id)
  {
    case MODULE_8BIT:                 /* 8bit i/f, 2 line, 5*11 font */
    case MODULE_8BIT_1LINE:
    case MODULE_HEM1601B:
    case MODULE_CF947:                /* 4bit i/f, 2 lines, 5*11 font */
    case MODULE_AMAX_EDC162A40GXBBB:
    case MODULE_4BIT:
    case MODULE_SPLC780D_4BIT:
    case MODULE_CF989:
    case MODULE_CF959:
    case MODULE_4BIT_1LINE:
      cycle (0, CMD_FUNCTION_SET | dl_bit | 0xc, 39);
      break;

    case MODULE_HIPSHING_LC1641SRNH6:
    case MODULE_HIPSHING_LC1641SRNH6_CF997:
    case MODULE_CF979:
    case MODULE_CF990:
    case MODULE_CF928:
    case MODULE_FUTABA_M204_SD02FJ:
      /* 4bit i/f, 2 line, 5*8 font */
      cycle (0, CMD_FUNCTION_SET | dl_bit | 0x8, 39);
      /* ST7066 datasheet says send this twice.  */
      cycle (0, CMD_FUNCTION_SET | dl_bit | 0x8, 39);
      break;

    case MODULE_SDEC_S2P16:
      cycle (0, CMD_FUNCTION_SET | dl_bit, 39);
      mdelay (8);
      cycle (0, CMD_FUNCTION_SET | dl_bit, 200);
      cycle (0, CMD_FUNCTION_SET | dl_bit, 200);
      cycle (0, CMD_FUNCTION_SET | 0x8 | dl_bit, 39);
      break;

    case MODULE_SITRONIX_ST7032:
      /* 4bit i/f, 2 line, 5*11 font */
      cycle (0, CMD_FUNCTION_SET | dl_bit | 0x9, 39);
      break;

    case MODULE_CF936:
      cycle (0, CMD_FUNCTION_SET | dl_bit | 0x8 | FUNCTION_SET_ST7070_EXT_BIT, 39);
      cycle (0, CMD_ST7070_COM_SEG_DIRECTION | 0x3, 39);
      cycle (0, CMD_FUNCTION_SET | dl_bit | 0x8, 39);
      break;

    case MODULE_CF772:
      if (reinitialise == 0)
      {
        cycle (0, CMD_FUNCTION_SET | dl_bit | 0x9, 39);
        cycle (0, CMD_FUNCTION_SET | dl_bit | 0x9, 39);
        //cycle (0, 0x14, 39);
        cycle (0, 0x77, 39);
        cycle (0, 0x5c, 39);
        cycle (0, 0x6f, 39);
        mdelay(300);
        cycle (0, 0x0f, 39);
      }
      cycle (0, CMD_FUNCTION_SET | dl_bit | 0x8, 39);
  }
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
  cycle (0, CMD_DISPLAY_MODE_SET, 39);

  // Display clear
  cycle (0, CMD_CLEAR_DISPLAY, 39);
  memset (shadow_buffer, ' ', screen_width_inc_arrows * screen_height);

  // Wait more than 1.53ms
  mdelay (4);
  
  // Entry mode set
  cycle (0, CMD_ENTRY_MODE_SET | ENTRY_MODE_INCREMENT, 39);

  // Wait more than 1.53ms
  mdelay (4);

  // Display on; cursor on
  // XXX Why is the cursor on here?
  cycle (0, CMD_DISPLAY_MODE_SET | DISPLAY_MODE_DISPLAY_ON | DISPLAY_MODE_CURSOR_ON
         | (cursor_blink ? DISPLAY_MODE_CURSOR_BLINK : 0), 39);

  release_shared_gpio();
}

/****************************************************************************
 ****************************** lcd_generic API *****************************
 ****************************************************************************/

/****************************************************************************
 * Turns power off
 ****************************************************************************/
static void ateam_power_off (void)
{
  if (use_power_control)
  {
    if (power_control_active)
    {
      /* Active High */
      rutl_regwrite (0, 1 << 5, (int)S3C2410_GPCDAT);   // power off
    }
    else
    {
      /* Active Low */
      rutl_regwrite (1 << 5, 0, (int)S3C2410_GPCDAT);     // power off
    }
  }
}

/****************************************************************************
 * Redraw the screen
 * Also reinit some registers on certain configs
 ****************************************************************************/
static void redraw_screen (void)
{
  get_shared_gpio();

  init_function_set_register(1); // XXX Seems to clobber DDRAM on type-4 screen
  reset_soft_characters ();
  forced_redraw_in_progress = 1;
  reciva_lcd_redraw();
  forced_redraw_in_progress = 0;

  release_shared_gpio();
}

/****************************************************************************
 * Initialises LCD hardware
 ****************************************************************************/
static void ateam_init_hardware (void)
{
  printk(PREFIX "%s\n", __FUNCTION__);

  /* Set up GPIO pins */
  setup_gpio ();

  get_shared_gpio ();

  if (use_power_control)
  {
    ateam_power_off ();

    /* Hold power low 100ms */
    mdelay (100);

    if (power_control_active)
    {
      /* Active High */
      rutl_regwrite (1 << 5, 0, (int)S3C2410_GPCDAT);	// power on
    }
    else
    {
      /* Active Low */
      rutl_regwrite (0, 1 << 5, (int)S3C2410_GPCDAT);     // power on
    }   
    // Wait 40ms for power on
    mdelay (200);
  }
 
  release_shared_gpio ();

  /* Initialise LCD registers */
  init_registers ();

  reset_soft_characters ();
}  

/****************************************************************************
 * LED control
 * level : 0 = off, non zero = on
 ****************************************************************************/
static void ateam_set_led(unsigned int mask)
{
  // XXX should really use reciva_set_led for all but trying not to break
  // other hwconfigs for now
  if (using_legacy_led)
    {
      if (mask & RLED_VOLUME)
	rutl_regwrite (1 << 7, 0, (int)S3C2410_GPHDAT);  // led on
      else
        rutl_regwrite (0, 1 << 7, (int)S3C2410_GPHDAT);  // led off
    }
  else
    {
      reciva_led_set (mask);
    }
}

/****************************************************************************
 * Returns the device capabilities
 ****************************************************************************/
static int ateam_get_capabilities(void)
{
  return really_no_arrows ? 0 : LCD_CAPABILITIES_ARROWS;
}

/****************************************************************************
 * Sets the "display mode"
 ****************************************************************************/
static void ateam_set_display_mode(int iMode)
{
  icons_enabled = (iMode & 1) ? 1 : 0;
  arrows_enabled = (iMode & 2) ? 0 : 1;
  cursor_blink = (iMode & 4) ? 1 : 0;
  arrows_left_only = (iMode & 8) ? 1 : 0;
  really_no_arrows = (iMode & 16) ? 1 : 0;
 
  if (really_no_arrows)
    arrows_enabled = 0;

  printk(PREFIX "icons %sabled, arrows %sabled\n", icons_enabled ? "en" : "dis", really_no_arrows ? "dis" : (arrows_enabled ? "en" : "sort-of en"));

  screen_width = screen_width_inc_arrows;
  if (arrows_enabled)
    screen_width -= 2;
  else if (icons_enabled)
    screen_width -= 1;
}

/****************************************************************************
 * Draws the icons
 ****************************************************************************/
static void ateam_draw_icons(unsigned int uiBitmask)
{
  int icon_was_showing = icon_showing;

  if (icons_enabled)
  {
    icon_showing = uiBitmask &
      (LCD_BITMASK_ALARM | LCD_BITMASK_SLEEP_TIMER | LCD_BITMASK_SHIFT);

    if (icon_showing != icon_was_showing)
    {
      redraw_screen ();
    }
  }
}

/****************************************************************************
 * Set or clear icon flag if it has changed
 ****************************************************************************/
static void update_icon(unsigned int uiBitMask, unsigned int uiChange,
                        unsigned int uiIcon)
{
  if (uiBitMask & uiChange)
    SET_ICON(uiIcon);
  else if (uiChange)
    CLEAR_ICON(uiIcon);
}

/****************************************************************************
 * Specific function for the MODULE_CF772 config
 ****************************************************************************/
static void ateam_draw_icons_cf772 (unsigned int uiBitMask)
{
  static unsigned int uiPreviousBitMask;
  unsigned int uiChange = uiBitMask ^ uiPreviousBitMask;

  uiPreviousBitMask = uiBitMask;

  if (uiChange)
  {
    update_icon(uiBitMask, uiChange & LCD_BITMASK_REPEAT,      ICON_REPEAT  );
    update_icon(uiBitMask, uiChange & LCD_BITMASK_SNOOZE,      ICON_SNOOZE  );
    update_icon(uiBitMask, uiChange & LCD_BITMASK_ALARM1,      ICON_ALARM1  );
    update_icon(uiBitMask, uiChange & LCD_BITMASK_ALARM2,      ICON_ALARM2  );
    update_icon(uiBitMask, uiChange & LCD_BITMASK_NAP_ALARM,   ICON_CLOCK   );
    update_icon(uiBitMask, uiChange & LCD_BITMASK_SLEEP_TIMER, ICON_Z_SMALL );
    update_icon(uiBitMask, uiChange & LCD_BITMASK_SLEEP_TIMER, ICON_Z_MEDIUM);
    update_icon(uiBitMask, uiChange & LCD_BITMASK_SLEEP_TIMER, ICON_Z_BIG   );
    update_icon(uiBitMask, uiChange & LCD_BITMASK_FM,          ICON_FM      );
    update_icon(uiBitMask, uiChange & LCD_BITMASK_AUX_IN,      ICON_AUX     );
    update_icon(uiBitMask, uiChange & LCD_BITMASK_DAB,         ICON_DAB     );
    update_icon(uiBitMask, uiChange & LCD_BITMASK_SD_CARD,     ICON_SD      );
    update_icon(uiBitMask, uiChange & LCD_BITMASK_USB,         ICON_USB     );
    update_icon(uiBitMask, uiChange & LCD_BITMASK_MP3,         ICON_MP3     );
    update_icon(uiBitMask, uiChange & LCD_BITMASK_WMA,         ICON_WMA     );
    update_icon(uiBitMask, uiChange & LCD_BITMASK_STEREO,      ICON_STEREO  );
    update_icon(uiBitMask, uiChange & LCD_BITMASK_STEREO,      ICON_BRACKET_RIGHT_BIG  );
    update_icon(uiBitMask, uiChange & LCD_BITMASK_STEREO,      ICON_BRACKET_RIGHT_SMALL);
    update_icon(uiBitMask, uiChange & LCD_BITMASK_STEREO,      ICON_BRACKET_LEFT_BIG   );
    update_icon(uiBitMask, uiChange & LCD_BITMASK_STEREO,      ICON_BRACKET_LEFT_SMALL );
  }

  draw_icon_buffer ();
}

/****************************************************************************
 * Draw the volume level using icons (not the text bar)
 ****************************************************************************/
static void ateam_draw_volume_cf772 (int volume)
{
#define VOL_ICON_MASK 0x1e

  if (volume >= 0)
  {
    unsigned int bars = volume * 8 / 100;
    unsigned int bits_00 = 0;
    unsigned int bits_01 = (icon_buf[1] & ~VOL_ICON_MASK) | 0x02; // speaker always ON

    if (bars > 0)
      bits_01 |= 0x04; // volume 1
    if (bars > 1)
      bits_01 |= 0x08; // volume 2
    if (bars > 2)
      bits_01 |= 0x10; // volume 3
    if (bars > 3)
      bits_00 |= 0x01; // volume 4
    if (bars > 4)
      bits_00 |= 0x02; // volume 5
    if (bars > 5)
      bits_00 |= 0x04; // volume 6
    if (bars > 6)
      bits_00 |= 0x08; // volume 7
    if (bars > 7)
      bits_00 |= 0x10; // volume 8

    icon_buf[0] = bits_00;
    icon_buf[1] = bits_01;
  }
  else
  {
    icon_buf[0] = 0;
    icon_buf[1] &= ~VOL_ICON_MASK;
  }
  draw_icon_buffer ();
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
                              int    iCursorWidth,
                              int   *piArrows,
                              int   *piLineContents)
{
  int x, iLine, iCharCount;
  unsigned short cLocalRep;
  const char *pcCurrentChar;
  int aiOffset[ateam_driver.get_height()];

  get_shared_gpio();

  /* Turn cursor off - this stops it flickering */
  cycle (0, CMD_DISPLAY_MODE_SET | DISPLAY_MODE_DISPLAY_ON, 40);

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
    cycle (1, iSlot, 43);
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
    cycle (1, iSlot, 43);
  }
  else if (icon_showing & LCD_BITMASK_SLEEP_TIMER)
  {
    set_address (screen_width_inc_arrows - 1, 0);
    cycle (1, 'Z', 43);
  }

  for (iLine = 0; iLine < ateam_driver.get_height(); iLine++)
  {
    unsigned char *shadow_pointer;
    pcCurrentChar = ppcText[iLine];
    iCharCount = rutl_count_utf8_chars(pcCurrentChar);
    int skipped = 0;

    aiOffset[iLine] = (ateam_driver.get_width() - iCharCount) / 2;
    if (aiOffset[iLine] < 0) 
      aiOffset[iLine] = 0;

    if (arrows_enabled)
      aiOffset[iLine]++;                // Shove everything over

    /* Write the whole line */
    set_address (0, iLine);
    shadow_pointer = shadow_buffer + (iLine * screen_width_inc_arrows);

    for (x=0; x<screen_width_inc_arrows; x++)
    {
      if (arrows_enabled && x == 0)
      {
        if (piArrows[iLine] == LCD_ARROW_LEFT || piArrows[iLine] == LCD_ARROW_BOTH)
          cLocalRep = arrows_left_only ? '>' : '<';
        else
          cLocalRep = ' ';
      }
      else if (x == screen_width_inc_arrows-1 && (arrows_enabled || icons_enabled))
      {
        if (iLine == 0 && (icon_showing & (LCD_BITMASK_ALARM | LCD_BITMASK_SLEEP_TIMER | LCD_BITMASK_SHIFT)))
          continue;             // The alarm icon overwrites this arrow.

        if (arrows_enabled && !arrows_left_only && ((piArrows[iLine] == LCD_ARROW_RIGHT) || 
                                                    (piArrows[iLine] == LCD_ARROW_BOTH)))
          cLocalRep = '>';
        else
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
	  
	  if (bResetAddress)
	    {
	      // define_soft_char() clobbered the LCM address pointer.  Put it back.
	      set_address (x, iLine);
	    }
	}

      if (cLocalRep & 0xff00)
	BUG ();

      if (forced_redraw_in_progress || cLocalRep != *shadow_pointer)
      {
        if (skipped || (x == 8 && lcd_module_id == MODULE_HEM1601B))
        {
          set_address (x, iLine);
          skipped = 0;
        }

        cycle (1, cLocalRep, 43);
        *shadow_pointer = cLocalRep;
      }
      else
        skipped = 1;

      shadow_pointer++;
    }
  }

  /* Turn cursor on if appropriate (only handles line 0) */
  if (iCursorType == LCD_CURSOR_ON)
    draw_cursor (iX+aiOffset[iY], iY);

  release_shared_gpio();

  return 0;
}

/****************************************************************************
 * Clears the display
 ****************************************************************************/
static void ateam_clear_screen (void)
{
  get_shared_gpio();

  // Clear display
  cycle (0, CMD_CLEAR_DISPLAY, 40);

  memset (shadow_buffer, ' ', screen_width_inc_arrows * screen_height);

  release_shared_gpio();

  mdelay (2);
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
static int ateam_get_width (void)
{
  return screen_width;
}

/****************************************************************************
 * Set icon ram
 ****************************************************************************/
static void draw_icon_buffer(void)
{
  // Select extened instruction set
  cycle(0, CMD_FUNCTION_SET | FUNCTION_SET_DL | 0x9, 39);
  cycle(0, CMD_SET_IRAM_ADDRESS, 39);
  int i=0;
  for ( ; i<16; i++)
  {
    cycle(1, icon_buf[i], 43);
  }
  // Back to default instrcution set
  cycle(0, CMD_FUNCTION_SET | FUNCTION_SET_DL | 0x8, 39);
}

/****************************************************************************
 * Clock drawing routines for display with ICON ram (CF772)
 ****************************************************************************/
static void icon_draw_digit(const struct icon_digit *digit_data, int digit)
{
  int i;
  int addr = digit_data->addr;
  for (i=0; i<3; i++, addr++)
  {
    // Use digit 8 as the mask, all segments are on in a 7-segment display
    icon_buf[addr] &= ~(digit_data->data[8][i]);
    if ((digit >= 0) && (digit < 10))
      icon_buf[addr] |= digit_data->data[digit][i];
  }
}

static int icon_draw_small_clock(struct lcd_draw_small_clock *psClockInfo)
{
  int iHours = psClockInfo->iHours;

  CLEAR_ICON(ICON_AM);
  CLEAR_ICON(ICON_PM);
  if (psClockInfo->iMode == LCD_DRAW_CLOCK_12HR)
  {
    if (iHours < 12)
    {
      SET_ICON(ICON_AM);
    }
    else
    {
      SET_ICON(ICON_PM);
    }
    iHours = psClockInfo->iHours % 12;
    if (iHours == 0)
      iHours = 12;
  }
  
  if ((psClockInfo->iMode != LCD_DRAW_CLOCK_12HR) || (iHours > 9))
  {
    icon_draw_digit(&icon_digit_data[0], iHours / 10);
  }
  else
  {
    icon_draw_digit(&icon_digit_data[0], -1);
  }
  
  icon_draw_digit(&icon_digit_data[1], iHours % 10);
  icon_draw_digit(&icon_digit_data[2], psClockInfo->iMinutes / 10);
  icon_draw_digit(&icon_digit_data[3], psClockInfo->iMinutes % 10);
  
  SET_ICON(ICON_COLON_TOP);
  SET_ICON(ICON_COLON_BOT);

  draw_icon_buffer();

  return 0;
}

#if 0
static int icon_draw_clock(struct lcd_draw_clock *psClockInfo)
{
  struct lcd_draw_small_clock small_clock =
  {
    .iHours = psClockInfo->iHours,
    .iMinutes = psClockInfo->iMinutes,
    .iSeconds = psClockInfo->iSeconds,
    .iMode = psClockInfo->iMode,
  };
  return icon_draw_small_clock(&small_clock);
}
#endif

/* Note that any elements not explicitly initialised here are guaranteed to 
 * be zero initialised */
static struct reciva_lcd_driver ateam_driver = {
  name:              acModuleName,
  init_hardware:     ateam_init_hardware,
  get_height:        reciva_lcd_get_height,
  get_width:         ateam_get_width,
  draw_screen:       ateam_draw_screen,
  clear_screen:      ateam_clear_screen,
  set_backlight:     reciva_bl_set_backlight,
  get_max_backlight: reciva_bl_get_max_backlight,
  set_led:           ateam_set_led,
  power_off:         ateam_power_off,
  redraw_screen:     redraw_screen,
  draw_icons:        ateam_draw_icons,
  set_display_mode:  ateam_set_display_mode,
  get_capabilities:  ateam_get_capabilities,
};

/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init
ateam_lcd_init (void)
{
  const struct lcm_data *lcm;
  char *s;
  int i;

  if (lcd_module_id >= NR_LCMS)
    {
      printk (PREFIX "bad value %d for lcd_module_id\n", lcd_module_id);
      return -EINVAL;
    }

  if (machine_is_rirm3())
  {
    lcd_e1 = (1 << 6);
  }


  /* Set up lcd data 
   * Note: some data might have been set up via module parameters */
  lcm = &lcm_data_array[lcd_module_id];
  if (screen_width_inc_arrows < 0)
    screen_width_inc_arrows = lcm->display_width;

  if (screen_height < 0)
    screen_height = lcm->display_height;

  if (interface_width < 0)
    interface_width = lcm->bus_width;

  if (using_legacy_led < 0)
    using_legacy_led = lcm->legacy_led;

  /* Set default display mode; this initialises screen_width */
  ateam_set_display_mode (16);

  /* Set up charmap info */
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
  else
  {
    for (i = 0; i < MAX_EXTRA_CHAR_TABLES; i++)
      extra_chars[i] = lcm->extra_chars[i];
  }

  shadow_buffer = kmalloc (screen_width_inc_arrows * screen_height, GFP_KERNEL);
  if (!shadow_buffer)
    return -ENOMEM;

  printk(PREFIX "screen_height = %d\n", screen_height);
  printk(PREFIX "screen_width = %d\n", screen_width);
  printk(PREFIX "screen_width_inc_arrows = %d\n", screen_width_inc_arrows);
  printk(PREFIX "interface_width = %d\n", interface_width);
  printk(PREFIX "using_legacy_led = %d\n", using_legacy_led);

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

  ateam_driver.charmap = asCharmap;

  if (lcd_module_id == MODULE_CF772)
  {
    ateam_driver.draw_small_clock = icon_draw_small_clock;
    ateam_driver.draw_icons = ateam_draw_icons_cf772;
    ateam_driver.draw_volume = ateam_draw_volume_cf772;
  }

  return reciva_lcd_init(&ateam_driver, screen_height, screen_width);
}

/****************************************************************************
 * Module cleanup
 ****************************************************************************/
static void __exit
ateam_lcd_exit(void)
{
  kfree (shadow_buffer);
  reciva_lcd_exit ();
  kfree (ateam_driver.charmap);
}

MODULE_AUTHOR("John Stirling <js@reciva.com>");
MODULE_DESCRIPTION("Reciva LCD driver");
MODULE_LICENSE("GPL");

module_init(ateam_lcd_init);
module_exit(ateam_lcd_exit);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
  module_param(lcd_module_id, uint, S_IRUGO);
  module_param(pins_shared, int, S_IRUGO);
  module_param(use_power_control, int, S_IRUGO);
  module_param(power_control_active, int, S_IRUGO);
  module_param(screen_width_inc_arrows, int, S_IRUGO);
  module_param(screen_height, int, S_IRUGO);
  module_param(interface_width, int, S_IRUGO);
  module_param(using_legacy_led, int, S_IRUGO);
  module_param(charmap_string, charp, S_IRUGO);
#else
  MODULE_PARM(lcd_module_id, "i");
  MODULE_PARM(pins_shared, "i");
  MODULE_PARM(use_power_control, "i");
  MODULE_PARM(power_control_active, "i");
  MODULE_PARM(screen_width_inc_arrows, "i");
  MODULE_PARM(screen_height, "i");
  MODULE_PARM(interface_width, "i");
  MODULE_PARM(using_legacy_led, "i");
  MODULE_PARM(charmap_string, "s");
#endif

