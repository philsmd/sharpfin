/* 
 * Sharpfin project
 * Copyright (C) by Steve Clarke and Ico Doornekamp
 * 2011-11-30 Philipp Schmidt
 *   Added to github 
 *
 * This file is part of the sharpfin project
 * The LCD library
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
 * The LCD library contains interface functions to all of the
 * LCD hardware features.
 *
 * It is essential that lcd_init() is called before any of these
 * functions are used.
 *
 * Note that for the printing, cursor and select functions, 
 * lcd_refresh() must be called to actually see a screen update.
 */

#ifndef lcd_h_defined
#define lcd_h_defined
#include <time.h>
/**
 * enum slcd_e_status
 *
 * Simple status for on/off/true/false etc..
 **/
enum slcd_e_status {
	SLCD_ON=(1==1),
	SLCD_OFF=(1==0),
	SLCD_YES=(1==1),
	SLCD_NO=(1==0),
	SLCD_TRUE=(1==1),
	SLCD_FALSE=(1==0),
	SLCD_PASS=(1==1),
	SLCD_FAIL=(1==0)
} ;

/**
 * enum slcd_e_arrows
 *
 * Identifies if arrows are selected for given line, or not
 **/
enum slcd_e_arrows {
	SLCD_SEL_ARROWS=13,	/* LCD_ARROW_BOTH, from reciva_lcd.h  */
	SLCD_SEL_NOARROWS=10	/* LCD_ARROW_NONE, from reciva_lcd.h */
} ;

/**
 * enum slcd_e_caps
 *
 * LCD Interface Capabilities
 **/
enum slcd_e_caps {
	SLCD_HAS_ARROWS=1,
	SLCD_HAS_INVERTED_TEXT=2,
	SLCD_HAS_GRAPHICS=4,
	SLCD_HAS_MENULED=8,
	SLCD_HAS_VOLUMELED=16,
	SLCD_HAS_BRIGHTNESS=32,
	SLCD_HAS_CONTRAST=64,
	SLCD_HAS_GRID=128,
	SLCD_HAS_INVGRID=256,
	SLCD_HAS_ICONS=512,
	SLCD_HAS_LEDS=1024,
	SLCD_HAS_DRIVERCLOCK=2048
} ;

/**
 * enum slcd_e_icons
 *
 * List of all of the supported icons and LEDs the radio has
 **/
enum slcd_e_icons {
	SLCD_ICON_SHIFT=1,
	SLCD_ICON_IRADIO=2,
	SLCD_ICON_MEDIA=3,
	SLCD_ICON_SHUFFLE=4,
	SLCD_ICON_REPEAT=5,
	SLCD_ICON_SLEEP=6,
	SLCD_ICON_MUTE=7,
	SLCD_ICON_ALARM=8,
	SLCD_ICON_SNOOZE=9,
	SLCD_ICON_MENU=10,
	SLCD_ICON_VOLUME=11,
	SLCD_ICON_REWIND=12,
	SLCD_ICON_PAUSE=13,
	SLCD_ICON_STOP=14,
	SLCD_ICON_FF=15
} ;

/**
 * enume slcd_e_clockmode
 *
 * Modes the clock can be displayed in
 **/
enum slcd_e_clkmode {
	SLCD_CLK_12HR=	0x00,	/* 12 Hour Clock */
	SLCD_CLK_24HR=	0x01	/* 24 Hour Clock */
				/* ANALOG=0x02 (not currently supported) */
				/* SDATE=0x04 Show date  (not currently supported ) */
} ;

/**
 * lcd_init
 *
 * lcd_init() initialises the LCD hardware, allocating memory, 
 * clearing the screen, and setting the initial states of the 
 * cursor and backlight.  lcd_init() must be called before
 * any of the lcd functions can be used.
 * Returns true on success, or false on memory allocation failure.
 **/ 
int lcd_init() ;

/**
 * lcd_exit
 *
 * This function closes down the LCD library
 * and frees all associated memory.
 **/
void lcd_exit() ;

/**
 * lcd_capabilities
 *
 * Returns a bitfield, which represents the capabilities of the LCD display.
 **/
enum slcd_e_caps lcd_capabilities() ;

/**
 * lcd_hwclearscr
 *
 * Clears the screen's working buffer. lcd_refresh must
 * then be called to cause the LCD display contents to be
 * updated to match. 
 **/
void lcd_hwclearscr() ;

/**
 * lcd_putline
 * num: line number of destination
 * str: string to print on line (can contain unicode characters)
 * sel: show selection arrows
 *
 * lcd_putline updates the given line in the display buffer.
 * The line is left justified.  If sel is SLCD_ARROWS, selection
 * arrows are displayed on the given line.
 * lcd_refresh must be called to update the actual screen.
 * The function returns true on success, or false if the
 * target buffer does not exist.
 **/
int lcd_hwputline(int num, char *str, enum slcd_e_arrows sel) ;

/**
 * lcd_brightness
 * @level: Brightness level (0-31)
 *
 * lcd_brightness changes the screen's backlight level
 * to the requested brightness.  This happens immediately
 * and lcd_refresh is not required.
 */
void lcd_brightness(int level) ;

/**
 * lcd_contrast
 * @level: Contrast level (0-31)
 *
 * lcd_contrast changes the screen's contrast level
 * to the requested value.  This happens immediately
 * and lcd_refresh is not required.
 */
void lcd_contrast(int level) ;

/**
 * lcd_hwcursor
 * @x: Sets X position
 * @y: Sets Y position
 * @status: Defines whether cursor is on or off
 *
 * lcd_hwcursor moves the cursor to the specified X,Y
 * position in the buffer.  It is turned on by setting
 * the status to SLCD_ON and off with a status of SLCD_OFF.
 * lcd_refresh must be called to see the cursor actually move.
 * The function returns true on success, and false if
 * the coordinates are out of range.
 **/
int lcd_hwcursor(int x,int y,enum slcd_e_status status) ;

/**
 * lcd_seticon
 * @icon: Icon to set or clear
 * @state: State to switch the icon to
 * 
 * This function is used to set or clear any of the icons or LEDs
 * on the radio.  The icon is specified along with a state of SLCD_ON
 * or SLCD_OFF.
 * If the icons do not actually exist on the radio, this function stores the
 * would-be state, which can be retrieved with the lcd_geticon function.
 * The function returns true on success, and false in the case of mis-use.
 **/
int lcd_seticon(enum slcd_e_icons icon, enum slcd_e_status state) ;

/**
 * lcd_geticon
 * @icon: Icon
 * 
 * This function is used to query the current state of the specified
 * icon.  This function reports the would-be state of the icon, in
 * the event that the icon does not actually exist.
 * This function returns SLCD_ON or SLCD_OFF.
 **/
enum slcd_e_icons lcd_geticon(enum slcd_e_icons icon) ;

/**
 * lcd_width
*
* Returns the LCD display width
**/
int lcd_width() ;

/**
 * lcd_height
*
* Returns the LCD display height
**/
int lcd_height() ;

/**
 * lcd_hwrefresh
 *
 * lcd_hwrefresh function causes the contents of the screen
 * buffer to be written to the actual display.  this affects
 * textual content, selection arrows and the cursor.
 **/
void lcd_hwrefresh() ;

/**
 * lcd_hwclock
 * @clk: current date / time
 * @almon: identifies whether alarm is on or off
 * @alm: alarm date / time
 * @clkmode: display mode for clock (12hr,24hr)
 * @am: AM text
 * @pm: PM text
 * @title: clock title
 * 
 * lcd_hwclock function sets the clock display parameters,
 * and then it will display the clock on the LCD screen.
 * optionally, the alarm time can also be displayed - for this
 * the alm function has to point to a time_t structure, and not
 * be NULL, and the almon status must be SLCD_ON.
 * This function will overwrite whatever has been displayed with the 
 * lcd_refresh function.
 **/
int lcd_hwclock(struct tm *clk, enum slcd_e_status almon, struct tm *alm,
	enum slcd_e_clkmode clkmode, char *am, char *pm, char *title) ;

/**
 * lcd_hwgetscreen
 *
 * This function returns a pointer to a
 * character array, with the current
 * screen contents that will be shown on
 * the next lcd_hwrefresh()
 **/
char ** lcd_hwgetscreen() ;

/**
 * lcd_hwgetselrows
 *
 * This function returns a pointer to an
 * integer array, identifying if the 
 * corresponding row has <> selection
 * arrows or not.  The s that will be shown on
 * the next lcd_hwrefresh()
 **/
enum slcd_e_arrows * lcd_hwgetselrows() ;
#endif
