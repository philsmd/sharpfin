/* 
 * Sharpfin project
 * Copyright (C) by Steve Clarke and Ico Doornekamp
 * 2011-11-30 Philipp Schmidt
 *   Added to github 
 *
 * This file is part of the sharpfin project
 *  
 * This Library is free software: you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *  
 * You should have received a copy of the GNU General Public License
 * along with this source files. If not, see
 * <http://www.gnu.org/licenses/>.
 */

/*
 * This library provides common setup
 * functions for ncurses to be shared between
 * the LCD and LOG functions when in devel
 * mode.
 */

#ifndef _scr_devel_h
#define _scr_devel_h
#include <ncurses.h>

WINDOW * scr_get_lcdw() ;
WINDOW * scr_get_lcdl() ;
int scr_init_lcd(int hei, int wid) ;

WINDOW * scr_get_logw() ;
int scr_init_log() ;

WINDOW *scr_get_helpw() ;
int scr_init_key() ; 
#endif
