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
 *  Keys are as follows:
 *
 *  Power	= Escape
 *  1		= '1'
 *  2		= '2'
 *  3		= '3'
 *  4		= '4'
 *  5		= '5'
 *  Shift+1	= '6'
 *  Shift+2	= '7'
 *  Shift+3	= '8'
 *  Shift+4	= '9'
 *  Shift+5	= '0'
 *  Browse	= 'B'
 *  Reply	= 'R'
 *  Back	= Backspace
 *  Select	= Enter
 *  Left	= Left Arrow
 *  Right	= Right Arrow
 *  VolumeUp	= Up Arrow
 *  VolumeDown	= Down Arrow
 *
 *  HoldKey	= F1-F10
 */

#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/select.h>
#include <linux/input.h>
#include "log.h"
#include "key.h"
#include "scr_devel.h"
#include <ncurses.h>

static int translate_key(int chr, struct key *ev);
#define KEYPRESSED 1
#define KEY6TO10PRESSED 2
#define KEYHELD 3
#define KEY6TO10HELD 4
#define BADKEY 5

#define HOLDME 0x10000

/*
 * ident
 */

char *key_ident() {
        return "$Id: key_devel.c 230 2007-10-23 17:42:49Z  $" ;
}

struct key_handler *key_init(void) {
	struct key_handler *eh;
	WINDOW *w ;
	/* Initialise the ncurses stuff */
	scr_init_key() ;

       /* Print Help Window */
        w=scr_get_helpw() ;
        wprintw(w, "1-0=Buttons, F1-F10=Hold, Esc=Pwr\n"
                "(B)rowse (M)enu (R)eply Lft/Rt=Nav\n"
                "Up/Dn=Vol Enter=Sel Backspace=Back") ;
        wrefresh(w) ;

	/* We don't need this structure, but allocate anyway */
	eh = calloc(sizeof *eh, 1);

	/* Set up the ncurses keyboard to no-echo & no-block */
	noecho() ;
	nodelay(stdscr,TRUE) ;
	keypad(stdscr, TRUE) ;
	return eh;
}


int key_poll(struct key_handler *eh, struct key *ev)
{
	static int queuesize=0, queuepos=0 ;
	static struct key qk[3]  ;
	int sleepcount=0 ;
	int r ;

	/* Simulate holding a key down */
	if (sleepcount>0) {
		sleepcount-- ;
		return 0 ;
	}

	/* Handle Queued Keystrokes */
	if (queuesize!=0) {

		/* Simulate holding a key down */
		if ( ((qk[queuepos].state)&HOLDME) == HOLDME) {
			sleepcount=25 ;
			qk[queuepos].state^=HOLDME ;
			return 0 ;
		}

		/* Get the next key from the queue */
		ev->id=qk[queuepos].id ;
		ev->state=qk[queuepos].state ;

		queuepos++ ;
		if (queuepos==queuesize) {
			queuepos=0 ;
			queuesize=0 ;
		}

		/* Queued key found */
		return 1 ;

	} else {

		/* Handle input Keystrokes */

		r=getch() ;
		if (r==ERR) return 0 ; /* No data waiting */
	
		/* redraw everything to stop screen corruptions - NASTY HACK*/
		touchwin(scr_get_lcdw()) ;
		touchwin(scr_get_lcdl()) ;
		touchwin(scr_get_logw()) ;
		touchwin(scr_get_helpw()) ;
		wrefresh(scr_get_helpw()) ;
		wrefresh(scr_get_logw()) ;
		wrefresh(scr_get_lcdl()) ;
		wrefresh(scr_get_lcdw()) ;
		refresh() ;

		switch (translate_key(r, ev)) {
		case KEYPRESSED:
			/* Pressed Keys */
			queuesize=1 ;
			qk[0].state=KEY_STATE_RELEASED ;
			qk[0].id=ev->id ;
			return 1 ;
		case KEY6TO10PRESSED:
			/* Shifted Key 1-5 */
			queuesize=3 ;
			qk[0].state=KEY_STATE_PRESSED ;
			qk[0].id=ev->id ;
			qk[1].state=KEY_STATE_RELEASED ;
			qk[1].id=ev->id ;
			qk[2].state=KEY_STATE_RELEASED ;
			qk[2].id=KEY_ID_SHIFT ;
			ev->state=KEY_STATE_PRESSED ;
			ev->id=KEY_ID_SHIFT ;
			return 1 ;
		case KEYHELD:
			/* Held Key 1-5 */
			qk[0].state = KEY_STATE_RELEASED & HOLDME ;
			qk[0].id=ev->id ;
			return 1 ;
		case KEY6TO10HELD:
			/* Held Shifted Key 1-5 */
			qk[0].state=KEY_STATE_PRESSED ;
			qk[0].id=ev->id ;
			qk[1].state=KEY_STATE_RELEASED & HOLDME ;
			qk[1].id=ev->id ;
			qk[2].state=KEY_STATE_RELEASED ;
			qk[2].id=KEY_ID_SHIFT ;
			ev->state=KEY_STATE_PRESSED ;
			ev->id=KEY_ID_SHIFT ;
			return 1 ;
		default:
			return 0 ;
		}

	}

	return -1 ;
}


int translate_key(int ch, struct key *k)
{
	k->state=KEY_STATE_PRESSED ;
	switch (ch) {
		case '1': k->id = KEY_ID_1 ; return KEYPRESSED ;
		case '2': k->id = KEY_ID_2 ; return KEYPRESSED ;
		case '3': k->id = KEY_ID_3 ; return KEYPRESSED ;
		case '4': k->id = KEY_ID_4 ; return KEYPRESSED ;
		case '5': k->id = KEY_ID_5 ; return KEYPRESSED ;
		case '6': k->id = KEY_ID_1 ; return KEY6TO10PRESSED ;
		case '7': k->id = KEY_ID_2 ; return KEY6TO10PRESSED ;
		case '8': k->id = KEY_ID_3 ; return KEY6TO10PRESSED ;
		case '9': k->id = KEY_ID_4 ; return KEY6TO10PRESSED ;
		case '0': k->id = KEY_ID_5 ; return KEY6TO10PRESSED ;
		case KEY_F(1): k->id = KEY_ID_1 ; return KEYHELD ;
		case KEY_F(2): k->id = KEY_ID_2 ; return KEYHELD ;
		case KEY_F(3): k->id = KEY_ID_3 ; return KEYHELD ;
		case KEY_F(4): k->id = KEY_ID_4 ; return KEYHELD ;
		case KEY_F(5): k->id = KEY_ID_5 ; return KEYHELD ;
		case KEY_F(6): k->id = KEY_ID_1 ; return KEY6TO10HELD ;
		case KEY_F(7): k->id = KEY_ID_2 ; return KEY6TO10HELD ;
		case KEY_F(8): k->id = KEY_ID_3 ; return KEY6TO10HELD ;
		case KEY_F(9): k->id = KEY_ID_4 ; return KEY6TO10HELD ;
		case KEY_F(10): k->id = KEY_ID_5 ; return KEY6TO10HELD ;
		case 'r':
		case 'R': k->id = KEY_ID_REPLY ; return KEYPRESSED ;
		case 'm':
		case 'M': k->id = KEY_ID_MENU ; return KEYPRESSED ;
		case 'b':
		case 'B': k->id = KEY_ID_BROWSE ; return KEYPRESSED ;
		case '\n':
		case '\r':
		case KEY_ENTER: k->id = KEY_ID_SELECT ; return KEYPRESSED ;
		case KEY_UP: k->id = KEY_ID_VOLUP ; return KEYPRESSED ;
		case KEY_DOWN: k->id = KEY_ID_VOLDN ; return KEYPRESSED ;
		case KEY_LEFT: k->id = KEY_ID_LEFT ; return KEYPRESSED ;
		case KEY_RIGHT: k->id = KEY_ID_RIGHT ; return KEYPRESSED ;
		case '\x7F':
		case KEY_BACKSPACE: k->id = KEY_ID_BACK ; return KEYPRESSED ;
		case 27: k->id=KEY_ID_POWER ; return KEYPRESSED ;
		default:
			logf(LG_INF, "Key %02X Pressed", ch) ;
			return BADKEY ;
	}
}
