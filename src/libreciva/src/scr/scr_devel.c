/* 
 * Sharpfin project
 * Copyright (C) by Steve Clarke and Ico Doornekamp
 * 2011-11-30 Philipp Schmidt
 *   Added to github 
 *
 * This file is part of the sharpfin project
 * Common library to ncurses (LCD <-> LOG)
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

#include "scr_devel.h"
#include "ncurses.h"
#include "log.h" 

static WINDOW *lcdw ;
static WINDOW *logw ;
static WINDOW *lcdl ;
static WINDOW *helpw ;

/*
 * ident
 */
char *scr_ident() {
        return "$Id: scr_devel.c 230 2007-10-23 17:42:49Z  $" ;
}

static int scr_init_common() {
	int first=(1==1) ;
	if (first) {
		initscr() ;
		start_color() ;
		init_pair(1, COLOR_BLACK,COLOR_GREEN) ;
		init_pair(2, COLOR_WHITE, COLOR_GREEN) ;
		init_pair(3, COLOR_BLACK, COLOR_BLUE) ;
		init_pair(4, COLOR_WHITE, COLOR_BLUE) ;
		first=(1==0) ;
	}
	return (1==1) ;
}

WINDOW * scr_get_lcdw() {
	return lcdw ;
}

WINDOW * scr_get_helpw() {
	return helpw ;
}

WINDOW *scr_get_lcdl() {
	return lcdl ;
}

int scr_init_lcd(int hei, int wid) {
	scr_init_common() ;
	lcdw=newwin(hei, wid+2, 1, 1) ;
	if (lcdw==NULL) {
		logf(LG_FTL, "ncurses screen allocation failure") ;
		return (1==0) ;
	}
	wbkgd(lcdw,COLOR_PAIR(1)) ;
	wrefresh(lcdw) ;

	lcdl=newwin(4, 22, 1, 18) ;
	if (lcdl==NULL) {
		logf(LG_FTL, "ncurses screen allocation failure") ;
	        return (1==0) ;
	}
	wbkgd(lcdl,COLOR_PAIR(2)) ;
	wrefresh(lcdl) ;
	return (1==1) ;
}

WINDOW * scr_get_logw() {
	return logw ;
}

int scr_init_log() {
	scr_init_common() ;
	logw=newwin(15, 80, 6, 0) ;
	if (logw==NULL) {
		logf(LG_FTL, "ncurses screen allocation failure") ;
		return (1==0) ;
	}
	idlok(logw,TRUE) ;
	scrollok(logw,TRUE) ;
	wsetscrreg(logw, 0, 14) ;
	wbkgd(logw,COLOR_PAIR(4)) ;
	wrefresh(logw) ;
	return (1==1) ;
}

int scr_init_key() {
	scr_init_common() ;
	helpw=newwin(4, 38, 1, 41) ;
        if (helpw==NULL) {
                logf(LG_FTL, "ncurses screen allocation failure") ;
                return (1==0) ;
									        }
        idlok(helpw,TRUE) ;
        wbkgd(helpw,COLOR_PAIR(3)) ;
        wrefresh(helpw) ;
        return (1==1) ;
}
