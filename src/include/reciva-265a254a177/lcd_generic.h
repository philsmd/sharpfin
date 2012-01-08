/*
 * Reciva LCD driver - non-device-specific functions
 * Copyright 2004-2006 John Stirling <js@reciva.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef __RECIVA_LCD_H
#define __RECIVA_LCD_H

#include "reciva_lcd.h"
#include "reciva_util.h"

extern int reciva_lcd_init(const struct reciva_lcd_driver *d, int h, int w);
extern void reciva_lcd_exit(void);

extern unsigned short reciva_lcd_utf8_lookup(const char *utf8rep);
extern char reciva_lcd_unicode_lookup(const rutl_utf8_seq_info *unicode);

extern int reciva_lcd_get_height(void);
extern int reciva_lcd_get_width(void);
extern int reciva_lcd_set_backlight (int level);
extern void reciva_lcd_redraw (void);

// XXX This looks like it's unused, but have a feeling it's used by firmware
// upgrade system
extern char *dup_get_progress_bar(int iProgress, int iRange, int iWidth);

#endif
