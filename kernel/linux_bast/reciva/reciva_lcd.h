#ifndef LINUX_RECIVA_LCD_H
#define LINUX_RECIVA_LCD_H

#include <linux/ioctl.h>

struct lcd_draw_screen
{
  char **acText;  
  int  iX;
  int  iY;
  int  iCursorType;
  int  *piArrows;
  int  *piLineContents;
};

/* XXX Please never change this struct or the associated ioctl, 
 * IOC_LCD_DRAW_PROGRESS_SIMPLE, as it's meant to be simple and backwards
 * compatible for ever more.  (This gets used in the firmware upgrade system
 * which ships binaries to the radio and therefore needs the interface not to
 * change.)
 */
struct lcd_draw_progress_simple
{
  /* Some text to explain what the progress bar is */
  char *pcHeading;

  /* The highest and lowest values the progress bar can take, eg 0 and 100 */
  int iLowest;
  int iHighest;

  /* The value the progress bar has, between iLowest and iHighest (inclusive)
   * If it's not within that range we don't draw any kind of progress bar.
   */
  int iProgress; 
};

struct lcd_get_volume
{
  int  iVolume;
  char *pcStringToWriteTo;
  int  iStringSize;
};

#define LCD_DRAW_CLOCK_12HR 0
#define LCD_DRAW_CLOCK_24HR 1
struct lcd_draw_clock
{
  int iHours;
  int iMinutes;
  int iSeconds;
  int iMode;         // 12/24Hr mode
  int iAlarmOn;      // Indicates if alarm is enabled
  int iAlarmHours;   // Alarm Time (hours) 
  int iAlarmMinutes; // Alarm Time (minutes)   
  char *pcDateString;
};

struct lcd_ampm_text
{
  char *pcAM;
  char *pcPM;
};
  
struct lcd_resize
{
  int iHeight;
  int iWidth;
};

struct bitmap_data
{
  int left;
  int top;  
  int width;
  int height;
  void *data;
};

struct reciva_lcd_driver
{
  const char *name;
  void (*init_hardware)(void);
  int  (*get_height)(void);
  int  (*get_width)(void);
  void (*set_zoom)(int, struct lcd_resize *);
  int  (*draw_screen)(char **, int, int, int, int *, int *);
  void (*clear_screen)(void);
  void (*draw_grid)(int);
  void (*draw_vertical_lines)(int);
  void (*set_backlight)(int);
  void (*set_led)(unsigned int);
  void (*power_off)(void);
  void (*draw_clock)(struct lcd_draw_clock *clock_info);
  void (*draw_signal_strength)(int);
  void (*draw_icons)(unsigned int);
  const struct rutl_unicode_charmap *charmap;
  int leds_supported;
  int  (*get_capabilities)(void);
  void (*set_display_mode)(int);
  void (*draw_bitmap)(struct bitmap_data);
  int (*get_graphics_height)(void);
  int (*get_graphics_width)(void);
  struct bitmap_data (*grab_screen_region)(int, int, int, int);
  void (*set_contrast)(int);
  void (*test_pattern)(void);
  void (*redraw_screen)(void);
  int (*set_ampm_text)(struct lcd_ampm_text *ampm_text);
};


#define LCD_IOCTL_BASE  'L'
#define IOC_LCD_DRAW_SCREEN         _IOW(LCD_IOCTL_BASE, 0, struct lcd_draw_screen)
#define IOC_LCD_CLEAR               _IOW(LCD_IOCTL_BASE, 1, int)
#define IOC_LCD_ENDSYMWIDTH         _IOR(LCD_IOCTL_BASE, 3, int)
#define IOC_LCD_GETVOLUMESTRING     _IOWR(LCD_IOCTL_BASE, 6, struct lcd_get_volume)
#define IOC_LCD_GET_HEIGHT          _IOR(LCD_IOCTL_BASE, 7, int)
#define IOC_LCD_GET_WIDTH           _IOR(LCD_IOCTL_BASE, 8, int)
#define IOC_LCD_GRID                _IOR(LCD_IOCTL_BASE, 9, int)
#define IOC_LCD_INVERSE_GRID        _IOR(LCD_IOCTL_BASE, 10, int)
#define IOC_LCD_VERT_LINES          _IOR(LCD_IOCTL_BASE, 11, int)
#define IOC_LCD_INVERSE_VERT_LINES  _IOR(LCD_IOCTL_BASE, 12, int)
#define IOC_LCD_BACKLIGHT           _IOR(LCD_IOCTL_BASE, 13, int)
#define IOC_LCD_LED                 _IOR(LCD_IOCTL_BASE, 14, int)
#define IOC_LCD_POWER_OFF           _IOR(LCD_IOCTL_BASE, 15, int)
#define IOC_LCD_DRAW_CLOCK          _IOW(LCD_IOCTL_BASE, 16, struct lcd_draw_clock)
#define IOC_LCD_SIGNAL_STRENGTH     _IOW(LCD_IOCTL_BASE, 17, int)
#define IOC_LCD_DRAW_ICONS          _IOW(LCD_IOCTL_BASE, 18, int)
#define IOC_LCD_ZOOM                _IOW(LCD_IOCTL_BASE, 19, int)
#define IOC_LCD_LEDS_SUPPORTED      _IOR(LCD_IOCTL_BASE, 20, int)
#define IOC_LCD_GET_CAPABILITIES    _IOR(LCD_IOCTL_BASE, 21, int)
#define IOC_LCD_SET_DISPLAY_MODE    _IOW(LCD_IOCTL_BASE, 22, int)
#define IOC_LCD_DRAW_BITMAP         _IOW(LCD_IOCTL_BASE, 23, struct bitmap_data)
#define IOC_LCD_GET_GRAPHICS_HEIGHT _IOR(LCD_IOCTL_BASE, 24, int)
#define IOC_LCD_GET_GRAPHICS_WIDTH  _IOR(LCD_IOCTL_BASE, 25, int)
#define IOC_LCD_GRAB_SCREEN_REGION  _IOWR(LCD_IOCTL_BASE, 26, struct bitmap_data)
#define IOC_LCD_SET_CONTRAST        _IOW(LCD_IOCTL_BASE, 27, int)
#define IOC_LCD_REDRAW_SCREEN       _IOW(LCD_IOCTL_BASE, 28, int)
#define IOC_LCD_SET_AMPM_TEXT       _IOW(LCD_IOCTL_BASE, 29, struct lcd_ampm_text)
#define IOC_LCD_DRAW_PROGRESS_SIMPLE _IOW(LCD_IOCTL_BASE, 30, struct lcd_draw_progress_simple) // XXX Do not change me.

/* Bitmasks for LCD_GET_CAPABILITIES */
#define LCD_CAPABILITIES_ARROWS         0x00000001
#define LCD_CAPABILITIES_INVERTED_TEXT  0x00000002
#define LCD_CAPABILITIES_GRAPHICS       0x00000004

/* Bitmasks for LCD_DRAW_ICONS */
#define LCD_BITMASK_SHIFT       0x00000001
#define LCD_BITMASK_IRADIO      0x00000002
#define LCD_BITMASK_MEDIA       0x00000004
#define LCD_BITMASK_SHUFFLE     0x00000008
#define LCD_BITMASK_REPEAT      0x00000010
#define LCD_BITMASK_SLEEP_TIMER 0x00000020
#define LCD_BITMASK_MUTE        0x00000040
#define LCD_BITMASK_ALARM       0x00000080
#define LCD_BITMASK_SNOOZE      0x00000100

/* For lcd_draw_screen.piLineContents */
#define LCD_LINE_CONTENTS_TEXT           0
#define LCD_LINE_CONTENTS_BARCODE        1
#define LCD_LINE_CONTENTS_INVERTED_TEXT  2

/* For lcd_draw_screen.piArrows */
#define LCD_ARROW_NONE  10
#define LCD_ARROW_LEFT  11
#define LCD_ARROW_RIGHT 12
#define LCD_ARROW_BOTH  13

/* For set_display_mode */
typedef enum
{
  LCD_TM13264CBCG_MODE_TM13264CBCG_0                        = 0,
  LCD_TM13264CBCG_MODE_TM13264CBCG_2                        = 1,
  LCD_TM13264CBCG_MODE_TM13264CBCG_1                        = 2,
  LCD_TM13264CBCG_MODE_TM13264CBCG_3                        = 3,
  LCD_TM13264CBCG_MODE_AMAX_KDC162A28BXXBB_95x17            = 4,
  LCD_TM13264CBCG_MODE_SSD0323_128x64                       = 5,
  LCD_TM13264CBCG_MODE_CONFIG1009                           = 6,
  LCD_TM13264CBCG_MODE_CONFIG985                            = 7,
  LCD_TM13264CBCG_MODE_CONFIG983                            = 8,

} lcd_tm13264cbcg_mode_t;


#define LCD_CURSOR_ON   11
#define LCD_CURSOR_OFF  10

/* New UTF-8 special characters in the private use range */

/* You're welcome to define new characters here in the range 0xe000 to 0xf8ff
 * There should be an identical copy of this list in lcd.h in the app. */

/* For now the end symbol is always three characters long, so we can get away
 * with hardcoding it as three single-space-width unicode characters.  If that
 * stops being the case we should definitely represent it as a single unicode
 * character with width defined by the lcd driver */
#define LCD_END_SYMBOL_PART1      "\xee\x80\x80"  /* 0xe000 */
#define LCD_END_SYMBOL_PART2      "\xee\x80\x81"  /* 0xe001 */
#define LCD_END_SYMBOL_PART3      "\xee\x80\x82"  /* 0xe002 */
#define LCD_END_SYMBOL_INV_PART1  "\xee\x80\x83"  /* 0xe003 */
#define LCD_END_SYMBOL_INV_PART2  "\xee\x80\x84"  /* 0xe004 */
#define LCD_END_SYMBOL_INV_PART3  "\xee\x80\x85"  /* 0xe005 */

#define LCD_VOLUME_1_BAR          "\xee\x80\x86"  /* 0xe006 */
#define LCD_VOLUME_2_BAR          "\xee\x80\x87"  /* 0xe007 */
#define LCD_LEFT_ARROW_SOLID      "\xee\x80\x88"  /* 0xe008 */
#define LCD_RIGHT_ARROW_SOLID     "\xee\x80\x89"  /* 0xe009 */
#define LCD_PAUSE_ICON            "\xee\x80\x8a"  /* 0xe00a */
#define LCD_EJECT_ICON            "\xee\x80\x8b"  /* 0xe00b */
#define LCD_BROWSE_ICON           LCD_EJECT_ICON
#define LCD_FAST_FORWARD_ICON     "\xee\x80\x8c"  /* 0xe00c */
#define LCD_REWIND_ICON           "\xee\x80\x8d"  /* 0xe00d */
#define LCD_STOP_ICON             "\xee\x80\x8e"  /* 0xe00e */
#define LCD_PADLOCK_ICON          "\xee\x80\x8f"  /* 0xe00f */



#endif
