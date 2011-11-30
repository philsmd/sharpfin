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

#include "log.h"
#include "lcd.h"
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#define MAXBUF 32

/**
 * testtitle
 *
 * displays test title on the screen
 **/
void testtitle(int test, char *title) {
	char buf[32] ;
	
	sprintf(buf, "Test %2d", test) ;
	lcd_hwclearscr() ;
	lcd_hwrefresh() ;
	lcd_hwputline(0, buf, SLCD_SEL_NOARROWS) ;
	lcd_hwputline(1, title, SLCD_SEL_NOARROWS) ;
	lcd_hwrefresh() ;
	logf(LG_INF, "%s: %s\n", buf, title) ;
	usleep(1500000) ;
}

/**
 * lcd_test1
 *
 * Screen Printing Test
 **/
void lcd_test1() {
	int i, j ;
	char buf[MAXBUF] ;
	char *scr ;
	
	testtitle(1, "HW Screen Test") ;
	
	/* Fill screen with XXX */
	
	for (i=0; i<lcd_height(); i++) {
		lcd_hwputline(i, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", SLCD_SEL_NOARROWS) ;
	}
	lcd_hwrefresh() ;

	usleep(1000000) ;
	lcd_hwclearscr() ;
	lcd_hwrefresh() ;
	usleep(1000000) ;
	
	/* Fill screen with YYY */

	for (i=0; i<lcd_height(); i++) {
		lcd_hwputline(i, "YYYYYYYYYYYYYYYYYYYYYYYYYYYY", SLCD_SEL_ARROWS) ;
	}
	lcd_hwrefresh() ;
	
	usleep(1000000) ;
	lcd_hwclearscr() ;
	lcd_hwrefresh() ;
	usleep(1000000) ;
	
	scr=malloc( (lcd_width()+3)*lcd_height()+1 ) ;
	if (scr==NULL) {
		logf(LG_INF, "Memory allocation failed - exiting\n") ;
		exit(1) ;
	}
	
	/* Walk Arrows Down Screen */
	for (j=0; j<lcd_height(); j++) {
		for (i=0; i<lcd_height(); i++) {
			strncpy(buf, "Line=X, Sel=Y     ", MAXBUF-1) ;
			buf[MAXBUF-1]='\0' ;
			buf[5]='0'+i ;
			buf[12]='0'+j ;
			if (i==j) lcd_hwputline(i, buf, SLCD_SEL_ARROWS) ;
			else lcd_hwputline(i, buf, SLCD_SEL_NOARROWS) ;
		}
		lcd_hwrefresh() ;
		lcd_dumpscreen(scr, (lcd_width()+3)*lcd_height()) ;
		logf(LG_INF, "Screen=>\n%s", scr) ;
		usleep(1000000) ;
	}
	
	usleep(2000000) ;
	lcd_hwclearscr() ;
	lcd_hwrefresh() ;
	usleep(1000000) ;
}

/**
 * lcd_test2
 *
 * Driver clock display test.  Sets the clock hours and minutes, then
 * changes minutes, every second for 20 seconds.
 **/
void lcd_test2() {
	int i ;
	struct tm clk, alm, *tmp ;
	time_t t ;
	
	testtitle(2, "HW Clock Test") ;

	if ((lcd_capabilities()&SLCD_HAS_DRIVERCLOCK)==0) {
		testtitle(2, " * No Clock") ;
		return ;
	}

	/* Calculate Alarm Time */
	time(&t) ; tmp=gmtime(&t) ;
	alm.tm_hour=tmp->tm_min ;
	alm.tm_min=tmp->tm_sec ;
	
	/* Now show the clock ticking for 20 seconds */
	for (i=0; i<20; i++) {
		time(&t) ; tmp=gmtime(&t) ;
		clk.tm_hour=tmp->tm_min ;
		clk.tm_min=tmp->tm_sec ;
		clk.tm_sec=0 ;
		lcd_hwclock(&clk, SLCD_ON, &alm, SLCD_CLK_24HR, "AM", "PM", "Clock Test") ;
		usleep(1000000) ;
	}

	usleep(2000000) ;
	lcd_hwclearscr() ;
	lcd_hwrefresh() ;
	usleep(1000000) ;
}

void lcd_test3_icons(int icon, char *title) {
	logf(LG_INF, " * Switching %s ON\n", title) ;
	lcd_hwputline(0, title, SLCD_SEL_NOARROWS) ;
	lcd_hwputline(1, "  ON  ", SLCD_SEL_NOARROWS) ;
	lcd_seticon(icon, SLCD_ON) ;
	lcd_hwrefresh() ;
	usleep(700000) ;
	logf(LG_INF, " * Switching %s OFF\n", title) ;
	lcd_hwputline(1, "  OFF ", SLCD_SEL_NOARROWS) ;
	lcd_seticon(icon, SLCD_OFF) ;
	lcd_hwrefresh() ;
	usleep(700000) ;
}
	
/**
 * lcd_test3
 *
 * turns on and off icons and leds
 **/
void lcd_test3() {
	testtitle(3, "HW Icons Test") ;
	if ((lcd_capabilities()&SLCD_HAS_ICONS)==0) {
		testtitle(3, " * No Icons") ;
	} else {
		lcd_test3_icons(SLCD_ICON_SHIFT, "Shift Icon") ;
		lcd_test3_icons(SLCD_ICON_IRADIO, "iRadio Icon") ;
		lcd_test3_icons(SLCD_ICON_MEDIA, "Media Icon") ;
		lcd_test3_icons(SLCD_ICON_SHUFFLE, "Shuffle Icon") ;
		lcd_test3_icons(SLCD_ICON_REPEAT, "Repeat Icon") ;
		lcd_test3_icons(SLCD_ICON_SLEEP, "Sleep Icon") ;
		lcd_test3_icons(SLCD_ICON_MUTE, "Mute Icon") ;
		lcd_test3_icons(SLCD_ICON_ALARM, "Alarm Icon") ;
		lcd_test3_icons(SLCD_ICON_SNOOZE, "Snooze Icon") ;
	}

	if ((lcd_capabilities()&SLCD_HAS_LEDS)==0) {
		testtitle(3, " * No LEDS") ;
	} else {
		lcd_test3_icons(SLCD_ICON_MENU, "Menu LED") ;
		lcd_test3_icons(SLCD_ICON_VOLUME, "Volume LED") ;	
	}

	usleep(1000000) ;
	lcd_hwclearscr() ;
	lcd_hwrefresh() ;
	usleep(1000000) ;
}

/**
 * lcd_test4
 *
 * moves the cursor about the screen
 **/
void lcd_test4() {
	char buf[32] ;
	int x, y ;
	testtitle(4, "HW Cursors") ;
	lcd_hwputline(0, "Cursor Pos", SLCD_SEL_NOARROWS) ;
	for (y=0; y<2; y++) {
		for (x=0; x<lcd_width(); x++) {
			sprintf(buf, "X=%02d, Y=%02d", x, y) ;
			lcd_hwputline(1, buf, SLCD_SEL_NOARROWS) ;
			lcd_hwcursor(x, y, SLCD_ON) ;
			lcd_hwrefresh() ;
			usleep(550000) ;
		}
	}

	usleep(1000000) ;
	lcd_hwclearscr() ;
	lcd_hwrefresh() ;
	usleep(1000000) ;
}

/**
 * lcd_test5
 *
 * Brightness / Contrast tests
 **/
void lcd_test5() {
	char buf[32] ;
	int x ;
	testtitle(5, "HW Brt/Ctrst") ;
	lcd_hwputline(0, "Brightness", SLCD_SEL_NOARROWS) ;
	lcd_hwputline(1, "Level=00", SLCD_SEL_NOARROWS) ;
	lcd_brightness(0) ;
	lcd_hwrefresh() ;
	usleep(1000000) ;

	for (x=1; x<=100; x++) {
			sprintf(buf, "Level=%02d", x) ;
			lcd_hwputline(1, buf, SLCD_SEL_NOARROWS) ;
			lcd_brightness(x) ;
			lcd_hwrefresh() ;
			usleep(50000) ;
	}

	usleep(1000000) ;
	lcd_brightness(50) ;
	usleep(1000000) ;
	
	lcd_hwputline(0, "Contrast", SLCD_SEL_NOARROWS) ;
	for (x=0; x<=100; x++) {
			sprintf(buf, "Level=%02d", x) ;
			lcd_hwputline(1, buf, SLCD_SEL_NOARROWS) ;
			lcd_contrast(x) ;
			lcd_hwrefresh() ;
			usleep(50000) ;
	}

	usleep(1000000) ;
	lcd_hwclearscr() ;
	lcd_hwrefresh() ;
	usleep(1000000) ;
}

/**
 * lcd_test6
 *
 * frame test functions
 **/
void lcd_test6() {
	lcd_handle *h, *h2, *h3 ;
	int i ;
	testtitle(6, "Frame Test") ;
	logf(LG_INF, " * Displaying whole page frame\n") ;
	h=lcd_framecreate() ;
	lcd_framesetline(h, 0, "Top Row") ;
	for (i=1; i<lcd_height(); i++) {
		lcd_frameprintf(h, i, "Line %02d", i) ;
	}
	lcd_refresh(h) ;
	usleep(2000000) ;
	lcd_delete(h) ;
	
	logf(LG_INF, " * Demonstrating frame buffering\n") ;
	
	h2=lcd_framecreate() ;
	lcd_framesetline(h2, 0, "Frame A") ;
	h3=lcd_framecreate() ;
	lcd_framesetline(h3, 0, "Frame B") ;

	lcd_refresh(h2) ;
	usleep(2000000) ;
	lcd_refresh(h3) ;
	usleep(2000000) ;
	lcd_refresh(h2) ;
	usleep(2000000) ;
	
	lcd_delete(h2) ;
	lcd_delete(h3) ;
	
	logf(LG_INF, " * Demonstrating bar\n") ;
	h=lcd_framecreate() ;
	for (i=0; i<=100; i+=5) {
		lcd_framesetline(h, 0, "Bar Tests") ;
		lcd_framebar(h, 1, 0, 100, i, SLCD_BLINES) ;
		lcd_framebar(h, 2, 0, 100, 100-i, SLCD_BARROWS) ;
		lcd_refresh(h) ;
		usleep(100000) ;
	}
	usleep(1000000) ;
	lcd_delete(h) ;
	
	logf(LG_INF, " * Demonstrating ticks\n") ;
	h=lcd_framecreate() ;
	lcd_framesetline(h, 0, "This test demonstrates the tick function.  Line 1 (this) scrolls. Line 2 is static.  Line 3 should not scroll.  Line 4 should display a status line, including live clock.") ;
	lcd_framesetline(h, 1, "0123456789ABCD") ;
	lcd_framesetline(h, 2, "bottom=status                         ") ;
	lcd_seticon(SLCD_ICON_ALARM, SLCD_ON) ;
	lcd_seticon(SLCD_ICON_SLEEP, SLCD_ON) ;
	lcd_framestatus(h, 3, SLCD_TRUE) ;
	lcd_refresh(h) ;

	for (i=0; i<140; i++) {
		lcd_tick() ;
		usleep(500000) ;
		if (i==70) {
			lcd_seticon(SLCD_ICON_ALARM, SLCD_OFF) ;
			lcd_seticon(SLCD_ICON_SLEEP, SLCD_OFF) ;
		}
	}
	lcd_delete(h) ;
	
	lcd_tick() ; /* There's no harm in lcd_tick() being called after lcd_delete(h) as delete de-registers handle */

	usleep(1000000) ;
	lcd_hwclearscr() ;
	lcd_hwrefresh() ;
	usleep(1000000) ;
}

/**
 * lcd_test7
 *
 * menu test functions
 **/
void lcd_test7() {
	lcd_handle *h ;
	int i, j ;
	
	testtitle(7, "Menu Test") ;
	
	logf(LG_INF, " * Demonstrating Menu\n") ;
	
	h=lcd_menucreate() ;
	lcd_menuaddentry(h, 1, "1 Row Menu", SLCD_NOTSELECTED) ;
	lcd_refresh(h) ;

	for (i=0; i<3; i++) {
		logf(LG_INF, " * Down - Selected Row is %2d: %s\n", lcd_menugetselid(h), lcd_menugetsels(h)) ;
		for (j=0; j<60; j++) {
			lcd_tick() ;
			usleep(10000) ;
		}
		lcd_menucontrol(h, SLCD_DOWN) ;
		lcd_refresh(h) ;
	}
	for (i=0; i<3; i++) {
		logf(LG_INF, " * Up - Selected Row is %2d: %s\n", lcd_menugetselid(h), lcd_menugetsels(h)) ;
		for (j=0; j<60; j++) {
			lcd_tick() ;
			usleep(10000) ;
		}
		lcd_menucontrol(h, SLCD_UP) ;
		lcd_refresh(h) ;
	}

	usleep(2000000) ;
	lcd_delete(h) ;

	h=lcd_menucreate() ;
	lcd_menuaddentry(h, 1, "3 Row top", SLCD_NOTSELECTED) ;
	lcd_menuaddentry(h, 2, "3 Row mid", SLCD_NOTSELECTED) ;
	lcd_menuaddentry(h, 3, "3 Row bot", SLCD_NOTSELECTED) ;
	lcd_refresh(h) ;

	for (i=0; i<4; i++) {
		logf(LG_INF, " * Down - Selected Row is %2d: %s\n", lcd_menugetselid(h), lcd_menugetsels(h)) ;
		for (j=0; j<60; j++) {
			lcd_tick() ;
			usleep(10000) ;
		}
		lcd_menucontrol(h, SLCD_DOWN) ;
		lcd_refresh(h) ;
	}
	for (i=0; i<4; i++) {
		logf(LG_INF, " * Up - Selected Row is %2d: %s\n", lcd_menugetselid(h), lcd_menugetsels(h)) ;
		for (j=0; j<60; j++) {
			lcd_tick() ;
			usleep(10000) ;
		}
		lcd_menucontrol(h, SLCD_UP) ;
		lcd_refresh(h) ;
	}

	usleep(2000000) ;
	lcd_delete(h) ;
	
	h=lcd_menucreate() ;
	lcd_menuaddentry(h, 1, "C Menu Row 1", SLCD_NOTSELECTED) ;
	lcd_menuaddentry(h, 2, "E Menu Row 2", SLCD_NOTSELECTED) ;
	lcd_menuaddentry(h, 3, "A Menu Long Row 3.  This line should scroll", SLCD_NOTSELECTED) ;
	lcd_menuaddentry(h, 4, "B Menu Row 4", SLCD_NOTSELECTED) ;
	lcd_menuaddentry(h, 5, "D Menu Row 5", SLCD_NOTSELECTED) ;
	lcd_refresh(h) ;

	for (i=0; i<6; i++) {
		logf(LG_INF, " * Down - Selected Row is %2d: %s\n", lcd_menugetselid(h), lcd_menugetsels(h)) ;
		for (j=0; j<60; j++) {
			lcd_tick() ;
			usleep(10000) ;
		}
		lcd_menucontrol(h, SLCD_DOWN) ;
		lcd_refresh(h) ;
	}
	for (i=0; i<7; i++) {
		logf(LG_INF, " * Up - Selected Row is %2d: %s\n", lcd_menugetselid(h), lcd_menugetsels(h)) ;
		for (j=0; j<60; j++) {
			lcd_tick() ;
			usleep(10000) ;
		}
		lcd_menucontrol(h, SLCD_UP) ;
		lcd_refresh(h) ;
	}
	
	usleep(2000000) ;

	logf(LG_INF, " * Sorting Menu\n") ;
	lcd_menusort(h) ;
	lcd_refresh(h) ;

	for (i=0; i<6; i++) {
		logf(LG_INF, " * Down - Selected Row is %2d: %s\n", lcd_menugetselid(h), lcd_menugetsels(h)) ;
		for (j=0; j<60; j++) {
			lcd_tick() ;
			usleep(10000) ;
		}
		lcd_menucontrol(h, SLCD_DOWN) ;
		lcd_refresh(h) ;
	}
	for (i=0; i<7; i++) {
		logf(LG_INF, " * Up - Selected Row is %2d: %s\n", lcd_menugetselid(h), lcd_menugetsels(h)) ;
		for (j=0; j<60; j++) {
			lcd_tick() ;
			usleep(10000) ;
		}
		lcd_menucontrol(h, SLCD_UP) ;
		lcd_refresh(h) ;
	}
	
	usleep(2000000) ;

	lcd_delete(h) ;
	
	usleep(1000000) ;
	lcd_hwclearscr() ;
	lcd_hwrefresh() ;
	usleep(1000000) ;
}

/**
 * lcd_test8
 *
 * clock test functions
 **/
void lcd_test8() {
	lcd_handle *h ;
	int i ;
	struct tm alarm ;
	char *scr ;
	
	scr=malloc( (lcd_width()+3)*lcd_height()+1 ) ;
	if (scr==NULL) {
		logf(LG_INF, "Memory allocation failed - exiting\n") ;
		exit(1) ;
	}
	
	testtitle(8, "Clock Test") ;
	logf(LG_INF, " * Demonstrating Clock (No Alarm)\n") ;

	h=lcd_clockcreate("Sharpfin Radio") ;
	lcd_clocksetalarm(h, NULL, SLCD_FALSE) ;
	lcd_refresh(h) ;

	lcd_dumpscreen(scr, (lcd_width()+3)*lcd_height()) ;
	logf(LG_INF, "Clock Screen=>\n%s", scr) ;
	
	for (i=0; i<20; i++) {
		lcd_tick() ;
		usleep(1000000) ;
	}
	
	usleep(1000000) ;

	logf(LG_INF, " * Demonstrating Clock (10:30 Alarm)\n") ;

	alarm.tm_hour=10 ;
	alarm.tm_min=30 ;
	
	lcd_clocksetalarm(h, &alarm, SLCD_TRUE) ;
	lcd_refresh(h) ;
	
	for (i=0; i<50; i++) {
		lcd_tick() ;
		usleep(1000000) ;
	}

	lcd_delete(h) ;
	
	usleep(1000000) ;
	lcd_hwclearscr() ;
	lcd_hwrefresh() ;
	usleep(1000000) ;
}

/**
 * lcd_test9
 *
 * input test functions
 **/
void lcd_test9() {
	lcd_handle *h ;
	char result[32] ;
	int i, j ;
	
	testtitle(9, "Input Test") ;
	logf(LG_INF, " * Demonstrating Input\n") ;

	result[0]='\0' ;
	h=lcd_inputcreate("ABCDEFGHIJKLMNOPQRSTUVWXYZ", result, 31) ;

	/* Enter ABCDEFGHIJ (note input selection always stats on END - last entry added to inputline */
	for (i=0; i<10; i++) {
		lcd_inputcontrol(h, SLCD_RIGHT) ;
		lcd_refresh(h) ;
		usleep(300000) ;
		lcd_inputcontrol(h, SLCD_ENTER) ;
		lcd_refresh(h) ;
		usleep(300000) ;
	}
	
	/* Enter IHGFEDCBA<END> */
	for (i=0, j=SLCD_FALSE; i<10 && j!=SLCD_TRUE; i++) {
		lcd_inputcontrol(h, SLCD_LEFT) ;
		lcd_refresh(h) ;
		usleep(300000) ;
		j=lcd_inputcontrol(h, SLCD_ENTER) ;
		lcd_refresh(h) ;
		usleep(300000) ;
	}
	
	logf(LG_INF, " * Input text should be ABCDEFGHIJIHGFEDCBA => %s\n", result) ;
	if (strcmp(result, "ABCDEFGHIJIHGFEDCBA")!=0) {
		logf(LG_INF, " * INPUT TEST FAILED\n") ;
	}
	lcd_delete(h) ;
	
	h=lcd_inputcreate("0123456789", result, 31) ;
	
	/* Select Delete (left 3 chars) */
	for (i=0; i<3; i++) {
		lcd_inputcontrol(h, SLCD_LEFT) ;
		lcd_refresh(h) ;
		usleep(300000) ;
	}
	
	/* Delete 8 Characters */
	for (i=0; i<8; i++) {
		lcd_inputcontrol(h, SLCD_ENTER) ;
		lcd_refresh(h) ;
		usleep(300000) ;
	}
	
	/* Select '9' (left one) */
	lcd_inputcontrol(h, SLCD_LEFT) ;
	lcd_refresh(h) ;
	usleep(300000) ;
	
	/* Insert 999999 */
	for (i=0; i<6; i++) {
		lcd_inputcontrol(h, SLCD_ENTER) ;
		lcd_refresh(h) ;
		usleep(300000) ;
	}

	/* Select Delete (Right One) */
	lcd_inputcontrol(h, SLCD_RIGHT) ;
	lcd_refresh(h) ;
	usleep(300000) ;
	
	/* Try and delete 20 Chars (buf has  ony 16 chars) */
	for (i=0; i<20; i++) {
		lcd_inputcontrol(h, SLCD_ENTER) ;
		lcd_refresh(h) ;
		usleep(300000) ;
	}

	/* Enter '98765' => 98765_ */
	for (i=0; i<5; i++) {
		
		lcd_inputcontrol(h, SLCD_LEFT) ;
		lcd_refresh(h) ;
		usleep(300000) ;
	
		lcd_inputcontrol(h, SLCD_ENTER) ;
		lcd_refresh(h) ;
		usleep(300000) ;
	}
	
	/* Select LEFT */
	for (i=0; i<6; i++) {
		lcd_inputcontrol(h, SLCD_RIGHT) ;
		lcd_refresh(h) ;
		usleep(300000) ;
	}

	/* Move cursor left One => 987_6_5 */
	lcd_inputcontrol(h, SLCD_ENTER) ;
	lcd_refresh(h) ;
	usleep(300000) ;
	
	/* Select DEL */
	lcd_inputcontrol(h, SLCD_LEFT) ;
	lcd_refresh(h) ;
	usleep(300000) ;

	/* Delete one char  => 98_6_5 */
	lcd_inputcontrol(h, SLCD_ENTER) ;
	lcd_refresh(h) ;
	usleep(300000) ;
	
	/* Move cursor left One */
	lcd_inputcontrol(h, SLCD_LEFT) ;
	lcd_refresh(h) ;
	usleep(300000) ;
	
	/* Insert 9999 => 9899_9_65 */
	for (i=0; i<4; i++) {
		lcd_inputcontrol(h, SLCD_ENTER) ;
		lcd_refresh(h) ;
		usleep(300000) ;
	}

	/* Select Enter (Right four) */
	for (i=0; i<4; i++) {
		lcd_inputcontrol(h, SLCD_RIGHT) ;
		lcd_refresh(h) ;
		usleep(300000) ;
	}
	
	lcd_inputcontrol(h, SLCD_ENTER) ;
	lcd_refresh(h) ;
	usleep(300000) ;

	logf(LG_INF, " * Input text should be 98799995 => %s\n", result) ;
	
	lcd_delete(h) ;
	
	usleep(1000000) ;
	lcd_hwclearscr() ;
	lcd_hwrefresh() ;
	usleep(1000000) ;
}

/**
 * lcd_test10
 *
 * yesno test functions
 **/
void lcd_test10() {
	lcd_handle *h ;
	int i ;
	
	testtitle(10, "YesNo Test") ;
	logf(LG_INF, " * Demonstrating Yes/No Function\n") ;

	h=lcd_yesnocreate("Yes/No/Maybe", "Are You Sure?", 0) ;
	if (h==NULL) {
		logf(LG_INF, "Error creating\n") ;
	}
	
	/* Move option right */
	for (i=0; i<5; i++) {
		logf(LG_INF, " * Current Option is Number %d\n", lcd_yesnoresult(h)) ;
		lcd_refresh(h) ;
		usleep(1000000) ;
		lcd_yesnocontrol(h, SLCD_RIGHT) ;
	}
	
	/* Move option left */
	for (i=0; i<5; i++) {
		logf(LG_INF, " * Current Option is Number %d\n", lcd_yesnoresult(h)) ;
		lcd_refresh(h) ;
		usleep(1000000) ;
		lcd_yesnocontrol(h, SLCD_LEFT) ;
	}
	lcd_delete(h) ;
	
	usleep(1000000) ;
	lcd_hwclearscr() ;
	lcd_hwrefresh() ;
	usleep(1000000) ;
}

int main(int argc, char *argv[]) {
	lcd_init() ;
	log_init(argv[0], LOG_TO_STDERR, LG_INF, 127) ;
	lcd_test1() ;
	lcd_test2() ;
	lcd_test3() ;
	lcd_test4() ;
	lcd_test5() ;
	lcd_test6() ;
	lcd_test7() ;
	lcd_test8() ;	
	lcd_test9();
	lcd_test10() ;
	testtitle(0, "Test Complete") ;
	lcd_exit() ;
	return 0 ;
}
