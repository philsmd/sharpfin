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

#define LCDG_MAX_BACKLIGHT 31

int __init reciva_lcd_init(const struct reciva_lcd_driver *d, int h, int w);
void __exit reciva_lcd_exit(void);

char reciva_lcd_utf8_lookup(const char *utf8rep);
char reciva_lcd_unicode_lookup(const rutl_utf8_seq_info *unicode);

int reciva_lcd_get_height(void);
int reciva_lcd_get_width(void);
int reciva_lcd_set_backlight (int level);

#endif
