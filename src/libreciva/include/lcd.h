/* 
 * Sharpfin project
 * Copyright (C) by Steve Clarke and Ico Doornekamp
 * 2011-11-30 Philipp Schmidt
 *   Added to github 
 *
 * This file is part of the sharpfin project
 * LCD access,control,maintenance
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
 * LCD Access, Control and Maintenance Functions
 */
#ifndef lcdif_h_defined
#define lcdif_h_defined
#include <stdlib.h>
#include <time.h>
#include "lcdhw.h"

/**
 * enum slcd_e_selected
 *
 * Selected / Not Selected state of individual lines
 * in the menu.
 **/
enum slcd_e_select {
	SLCD_SELECTED,
	SLCD_NOTSELECTED
} ;

/**
 * enum slcd_e_type
 *
 * Type of buffers
 **/
enum slcd_e_type {
	SLCD_MENU,
	SLCD_FRAME,
	SLCD_INPUT,
	SLCD_CLOCK,
	SLCD_YESNO
} ;

/**
 * struct lcdif_s_row
 *
 * Line of data for the screen
 **/
struct slcd_s_row {
	struct slcd_s_row *next, *prev ;	/* Linked list pointers */
	char *str ;				/* row text string */
	int idnumber ;				/* row's ID Number */
	int scrollpos ;				/* Scroll pos for row animations */
	int len ;				/* UTF8 aware length of str */
} ;

/**
 * struct lcd_handle
 *
 * The lcd_handle structure is created and managed by
 * the lcd screen functions.  Its purpose is to enable a
 * number of different screens to be managed at the same
 * time, without interfering with each other.  It provides
 * the storage area for screen data much larger than the
 * visible screen, and provides functions like menus,
 * input selection and progress bars.
 **/
typedef struct {
	enum slcd_e_type type ;		/* type = frame, input, menu */
	int numrows ;			/* number of rows of text in the structure */
	struct slcd_s_row *top, *end ;	/* storage buffer pointers for lines data */
	struct slcd_s_row *tsc, *curl ;	/* line  at top of screen, and current selected line */

	char *linebuf ;			/* scratch buffer for line creation (Screen width in UTF8 chars */
	
	/* Frame specific parameters */
	int statusrow ;			/* Frame row to overwrite with 88:88 status */
	enum slcd_e_status showicons ;	/* Add SL AL to status row */
	
	/* Clock Specific Parameters */
	struct tm alm ;			/* Alarm time */
	enum slcd_e_status almon ;	/* Alarm on/off state */
	
	/* Input Specific Parameters */
	int inputselectedopt ;		/* Position of the selected option on input top line */
	int inputcursorpos ;		/* Input char under cursor */
	int inputbolpos ;		/* Input char at beginning of line */
	char *result ;			/* Destination for result (input types) */
	char *selectopts ;		/* Input selection list */
	int maxlen ;			/* Max length of result */
	
	/* YesNo Specific Parameters */
	int yesnoopt ;			/* Index of selected option */
	int yesnomax ;			/* Max number of options */
	char *yesnoopts ;		/* YesNo Options */
	

} lcd_handle ;

/**
 * enum slcd_e_inputctl
 *
 * Control messages for the input  function
 **/
enum slcd_e_inputctl {
	SLCD_LEFT = 0,
	SLCD_RIGHT,
	SLCD_ENTER
} ;

/**
 * enum slcd_e_menuctl
 *
 * Control messages for the menu function
 **/
enum slcd_e_menuctl {
	SLCD_UP = 0,
	SLCD_DOWN
} ;

/**
 * enum slcd_e_bartype
 *
 * Types of progress bar
 **/
enum slcd_e_bartype {
	SLCD_BLINES,
	SLCD_BARROWS
} ;


/*************************
 * Menu Control Functions
 *************************/

/**
 * lcd_menucreate
 *
 * The lcd_menucreate function creates a screen frame,
 * into which any data can be printed / output.  The selected
 * line is scrolled (if required) when the lcdif_tick function is 
  *called and the screen is the active one.
 * This function returns a handle, which is used for future 
 * screen operations, or NULL in the event of an error.
 */
lcd_handle *lcd_menucreate() ;

/**
 * lcd_menuclear
 *
 * This function clears the menu, removing all of the 
 * rows, and putting it back into its original state
 **/
int lcd_menuclear(lcd_handle *menuh) ;

/**
 * lcd_menuaddentry
 * @handle: handle of screen buffer
 * @idnumber: user's reference number
 * @str: string to set line to
 * @selected: entry is the currently selected line
 *
 * This function adds a menu entry to the bottom of the menu
 * list.  This adds an entry to the list, and does not update the
 * display - lcd_refresh() is required to update the LCD. When
 * selected is set to SLCD_SELECTED, the entry is automatically
 * selected when the screen is displayed.
 * The function returns true on success, and false if out of memory.
 **/
int lcd_menuaddentry(lcd_handle *handle, int idnumber, char *str, enum slcd_e_select selected) ;

/**
 * lcd_menusort
 * @handle: handle of screen buffer
 *
 * This function sorts the menu into alphabetical
 * order.
 **/
void lcd_menusort(lcd_handle *handle) ;
 
/**
 * lcd_menucontrol
 * @handle: handle of screen buffer
 * @cmd: direction to move, or enter
 *
 * lcd_menucontrol is used to scroll through a menu list
 * with SLCD_UP and SLCD_DOWN commands.
 * The function updates the buffer only.  To update the
 * actual LCD, a call to lcd_refresh() is required.
 **/
int lcd_menucontrol(lcd_handle *handle, enum slcd_e_menuctl cmd) ;

/**
 * lcd_menugetsels
 * @handle: handle of the screen buffer
 *
 * This function returns a pointer to selected line's string
 **/
char *lcd_menugetsels(lcd_handle *handle) ;

/**
 * lcd_menugetselid
 * @handle: handle of the screen buffer
 *
 * This function returns the selected line's idnumber.
 **/
int lcd_menugetselid(lcd_handle *handle) ;


/*************************
 * Frame Access / Control Functions
 *************************/
 
/**
 * lcd_framecreate
 *
 * The lcd_framecreate function creates a screen frame,
 * into which any data can be printed / output. Any line longer 
 * than the screen width is scrolled when the lcd_tick function 
  *is called and the screen is the active one.
 * This function returns a handle, which is used for future 
 * screen operations, or NULL in the event of an error.
 */
lcd_handle *lcd_framecreate() ;

/**
 * lcd_framesetline
 * @handle: handle of screen buffer
 * @line: line number to set
 * @str: string to set line to
 *
 * This function sets the specified line text.
 * The function returns true on success, and false if out of memory.
 **/
int lcd_framesetline(lcd_handle *handle, int line, char *str) ;

/**
 * lcd_frameprintf
 * @handle: handle of screen buffer
 * @line: destination line on the screen
 * @fmt: printf format
 *
 * This function sets the specified line text, using a printf structure.
 * The function returns true on success, and false if out of memory.
 **/ 
int lcd_frameprintf(lcd_handle *handle, int line, char *fmt, ...) ;

/**
 * lcd_framebar
 * @handle: handle of the screen buffer
 * @line: line number to set
 * @min: minimum value for bar
 * @max: maximum value for bar
 * @progress: actual value of bar
 * @bartype: type of bar
 *
 * The lcd_framebar function draws a progress bar into the frame
 * buffer using the full screen width at the given line.  The progress
 * value must be between (inclusive) min and max.  The bar type 
 * specifies whether the bar is displayed using lines (SLCD_BLINES)
 * or solid arrows (SLCD_BARROWS) to enable the bar to be used
 * for controls such as volume, or progress displays such as track
 * time played / remaining.
 * The function returns true on success, and false if out of memory,
 * or progress is out of range.
 **/
int lcd_framebar(lcd_handle *handle, int line, int min, int max, int progress, enum slcd_e_bartype type) ;

/**
 * lcd_framestatus
 * @handle: handle of the screen buffer
 * @line: line number to set
  * @showicons: shows icons
 *
 * The lcd_framestatus function shows a representation of the
 * radio's icons on the given line.  Note that this output will wither
 * look like:
 *
 *	88:88  SL  AL	If showicons=SLCD_TRUE
 *			AND icons are not supported
 *			in hardware.
 * or
 *
 *	     88:88		If showicons=SLCD_FALSE
 *			OR icons are supported in
 *			hardware.
 *
 * Depending on whether icons are supported on the radio or not.
 * The current time will be displayed in both cases, and if icons are
 * not supported, the Sleep and Alarm icons will be represented.
 * This line will automatically be re-generated and updated on an
 * lcd_tick call.
 * This function returns true (SLCD_TRUE) on success.
 **/
enum slcd_e_status lcd_framestatus(lcd_handle *handle, int line, enum slcd_e_status showicons) ;

/*************************
 * Input Access / control Functions
 *************************/

/**
 * lcd_inputcreate
 * @selection: characters to choose from
 * @result: string buffer for result
 * @maxlen: max length of string buffer (in bytes)
 *
 * The lcd_inputcreate function creates an input screen
 * with the top line having a scrolling selection e.g. "abcd <e> fghi".
 * The function automatically adds DEL LEFT RIGHT and END
 * strings to the selection, and populates the result line with
 * the contents of result as a preset. result is kept up-to-date
 * with the current contents of the display.
 * This function returns a handle, which is used for future 
 * screen operations, or NULL in the case of an error.
 */
lcd_handle *lcd_inputcreate(char *selection, char *result, int maxlen) ;

/**
 * lcd_inputcontrol
 * @handle: handle of screen buffer
 * @cmd: direction to move, or enter
 *
 * lcd_inputcontrol is used to scroll through an input line
 * with SLCD_LEFT and SLCD_RIGHT commands.  enter/select
 * can be performed with the SLCD_ENTER command.
 * The function updates the buffer only.  To update the
 * actual LCD, a call to lcd_refresh() is required.
 * This function is appropriate tothe INPUT buffer type.
 * This function returns true if END has been selected and
 * the update is complete, or false if not complete.
 **/
int lcd_inputcontrol(lcd_handle *handle, enum slcd_e_inputctl cmd) ;

/**
 * lcd_inputsetline
 * @handle: handle of screen buffer
 * @line: line number to set
 * @str: string to set line to
 *
 * This function sets the specified line text (lines 0 and 1 are reserved)
 * The function returns true on success, and false if out of memory.
 **/
int lcd_inputsetline(lcd_handle *handle, int line, char *str) ;
/**
 * lcd_inputprintf
 * @handle: handle of screen buffer
 * @line: destination line on the screen
 * @fmt: printf format
 *
 * This function sets the specified line text, using a printf structure.
 * The function returns true on success, and false if out of memory.
 **/ 
int lcd_inputprintf(lcd_handle *handle, int line, char *fmt, ...) ;


/*************************
 * YesNo Access / control Functions
 *************************/

/**
 * lcd_yesnocreate
 * @yesno: Yes string
 * @title: title for top line of screen
 * @defopt: Default option to start
 *
 * The lcd_yesnocreate function creates an input screen
 * With a prompt on the top line, and the next line shows
 * "<Yes>/ No ".
 * yesno contains the text for Yes and No, in the format "Yes/No"
 * The function also supports more options, which are
 * supplied in the format "Abort/Save/Ignore".  The yesno functions
 * do not perform any scrolling of the input line
 *. It should be noted that each option is padded (before and after) 
 * with a single space. This function returns a handle, which is used 
 * for future screen operations, or NULL in the case of an error.
 */
lcd_handle *lcd_yesnocreate(char *yesno, char *title, int defopt) ;

/**
 * lcd_yesnocontrol
 * @handle: handle of screen buffer
 * @cmd: direction to move, or enter
 *
 * lcd_yesnocontrol is used to scroll through the yesno 
 * options with the SLCD_LEFT and SLCD_RIGHT commands.
 * The function returns true on success.
 **/
int lcd_yesnocontrol(lcd_handle *handle, enum slcd_e_inputctl cmd) ;

/**
* lcd_yesnoresult
* @handle: handle of screen buffer
*
* This function returns the number of the currently
* selected option (first supplied is 0).
* e.g. If the input is "Yes/No/Ignore", the
* possible responses from this function are
* 0, 1 and 2.
**/
int lcd_yesnoresult(lcd_handle *handle) ;

/*************************
 *Clock Functions
 *************************/

/**
 *lcd_clockcreate()
 * @title: screen title
 * 
 * This function creates a clock buffer type.
 **/
lcd_handle *lcd_clockcreate(char *title) ;

/**
 * lcd_clocksetalarm
 * @handle: pointer to screen handle structure
 * @alm: alarm time (or NULL)
 * @almon: true if alarm is enabled
 *
 *This function sets the title, and the alarm  
 * (alarmon=SLCD_ON)  in the clock buffer.
 * Note that the actual displayed time is taken
 * from the system clock's time() function. 
 * If the alarm time is NULL, or alarmon status is 
 * off (SLCD_OFF), the alarm is not shown. Note 
 * that this updates the buffer, and a call to lcd_refresh() 
 * is required to update the actual screen. 
 **/
void lcd_clocksetalarm(lcd_handle *handle, struct tm *alm, enum slcd_e_status alarmon) ;
 
/*************************
 * Functions appropriate to all screen types
 *************************/
 
/**
 * lcd_delete
 * @handle: handle of the screen buffer
 *
 * This function deletes the screen buffer,
 * and frees all of its contents.
 **/
void lcd_delete(lcd_handle *handle) ;

/**
 * lcd_refresh
 * @handle: handle for the screen buffer
 *
 * This function transfers the contents of
 * the screen buffer identified by the provided
 * handle to the LCD.
 **/
void lcd_refresh(lcd_handle *handle) ;

/**
 * lcd_tick
 *
 * lcd_tick animates the currently displayed
 * screen.
 **/
void lcd_tick() ;

/**
 * lcd_dumpscreen
 **/
void lcd_dumpscreen(char *dest, int maxlen) ;

/**
 * lcd_dumpvars
 **/
void lcd_dumpvars(lcd_handle *h, char *str, int n) ;

/**
 * Special Characters
 *
 * These unicode character sequences are interpreted in the
 * low level LCD driver.
 */
#define SLCD_UCHR_END1		"\xee\x80\x80"  /* E Character */
#define SLCD_UCHR_END2		"\xee\x80\x81"  /* N Character */
#define SLCD_UCHR_END3		"\xee\x80\x82"  /* D Character */
#define SLCD_UCHR_DEL1		"\xee\x80\x83"  /* D Character */
#define SLCD_UCHR_DEL2		"\xee\x80\x84"  /* E Character */
#define SLCD_UCHR_DEL3		"\xee\x80\x85"  /* L Character */
#define SLCD_UCHR_VOL1		"\xee\x80\x86"  /* 0xe006 */
#define SLCD_UCHR_VOL2		"\xee\x80\x87"  /* 0xe007 */
#define SLCD_UCHR_LEFT		"\xee\x80\x88"  /* 0xe008 */
#define SLCD_UCHR_RIGHT		"\xee\x80\x89"  /* 0xe009 */
#define SLCD_UCHR_PAUSE		"\xee\x80\x8a"  /* 0xe00a */
#define SLCD_UCHR_EJECT		"\xee\x80\x8b"  /* 0xe00b */
#define SLCD_UCHR_FF		"\xee\x80\x8c"  /* 0xe00c */
#define SLCD_UCHR_REW		"\xee\x80\x8d"  /* 0xe00d */
#define SLCD_UCHR_STOP		"\xee\x80\x8e"  /* 0xe00e */
#define SLCD_UCHR_LOCK		"\xee\x80\x8f"  /* 0xe00f */
#define SLCD_UCHR_MUTE		"M"
#define SLCD_USTR_INPUTMENU	SLCD_UCHR_DEL1 SLCD_UCHR_DEL2 SLCD_UCHR_DEL3 SLCD_UCHR_LEFT SLCD_UCHR_RIGHT SLCD_UCHR_END1 SLCD_UCHR_END2 SLCD_UCHR_END3
#endif
