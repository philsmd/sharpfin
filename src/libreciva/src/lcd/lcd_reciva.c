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
 * The LCD library contains interface functions to all of the
 * LCD hardware features.
 *
 * It is essential that lcd_init() is called before any of these
 * functions are used.
 *
 * Note that for the printing, cursor and select functions, 
 * lcd_refresh() must be called to actually see a screen update.
 *
 */

#include "log.h"
#include "lcdhw.h"
#include "reciva_lcd.h"
#include "reciva_leds.h"
#include <errno.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#define SLCD_D_CLK_TITLE_LEN 32

struct {
	int wid ;				/* Screen Width */
	int hei ;				/* Screen Height */
	enum slcd_e_caps cap ;			/* Display Capabilities */
	int icons ;				/* Icons mask */
	int leds ;				/* LEDs mask */
	struct lcd_draw_screen scr ;		/* Screen structure */
	enum slcd_e_clkmode clkmode ;		/* Clock display mode (12/24 hour) */
	char clktitle[SLCD_D_CLK_TITLE_LEN] ;	/* Clock title - default='Sharpfin Radio' */
} lcd ;

/* local function definitions */
static int lcd_hwdoioctl(int iot, void *arg) ;

/*
 * ident
 */

char *lcdhw_ident()
{
        return "$Id: lcd_reciva.c 230 2007-10-23 17:42:49Z  $" ;
}

/**
 * lcd_init
 *
 * lcd_init() initialises the LCD hardware, allocating memory, 
 * clearing the screen, and setting the initial states of the 
 * cursor and backlight.  lcd_init() must be called before
 * any of the lcd functions can be used.
 * Returns true on success, or false on memory allocation failure.
 **/ 
int lcd_init() 
{
	int r, tmp ;

	/* Find out what the radio can actually do */
	
	lcd_hwdoioctl(IOC_LCD_GET_CAPABILITIES, (void *)&lcd.cap) ;
	lcd_hwdoioctl(IOC_LCD_GET_WIDTH, (void *)&lcd.wid) ;
	lcd_hwdoioctl(IOC_LCD_GET_HEIGHT, (void *)&lcd.hei) ;
	lcd_hwdoioctl(IOC_LCD_LEDS_SUPPORTED, (void *)&tmp) ;
	if (tmp!=0) lcd.cap|=SLCD_HAS_LEDS ;

	tmp=0 ;
	if (!(!lcd_hwdoioctl(IOC_LCD_BACKLIGHT, &tmp) && errno==ENOSYS)) lcd.cap|=SLCD_HAS_BRIGHTNESS ;
	if (!(!lcd_hwdoioctl(IOC_LCD_SET_CONTRAST, &tmp) && errno==ENOSYS)) lcd.cap|=SLCD_HAS_CONTRAST ;
	if (!(!lcd_hwdoioctl(IOC_LCD_GRID, &tmp) && errno==ENOSYS)) lcd.cap|=SLCD_HAS_GRID ;
	if (!(!lcd_hwdoioctl(IOC_LCD_INVERSE_GRID, &tmp) && errno==ENOSYS)) lcd.cap|=SLCD_HAS_INVGRID ;
	if (!(!lcd_hwdoioctl(IOC_LCD_DRAW_ICONS, &tmp) && errno==ENOSYS)) lcd.cap|=SLCD_HAS_ICONS ;
	if (!(!lcd_hwdoioctl(IOC_LCD_SET_AMPM_TEXT, &tmp) && errno==ENOSYS)) lcd.cap|=SLCD_HAS_DRIVERCLOCK ;

	/* Set icons, LEDs, Brightness and Contrast */
	
	lcd.icons=0 ;
	lcd.leds=0 ;
	lcd_seticon(SLCD_ICON_SHIFT, SLCD_OFF) ;
	lcd_brightness(15) ;
	lcd_contrast(15) ;
	
	lcd.scr.acText=malloc(sizeof(char *) * lcd.hei) ;
	if (lcd.scr.acText==NULL) return SLCD_FALSE ;
	lcd.scr.piArrows=malloc(sizeof(int) * lcd.hei) ;
	if (lcd.scr.piArrows==NULL) return SLCD_FALSE ;
	lcd.scr.piLineContents=malloc(sizeof(int) * lcd.hei) ;
	if (lcd.scr.piLineContents==NULL) return SLCD_FALSE ;
	for (r=0; r<lcd.hei; r++) {
		/* Allow for each character on a line to be 3 unicode characters long */
		lcd.scr.acText[r]=malloc( sizeof(char) * 3 * lcd.wid + 1 ) ;
		if (lcd.scr.acText[r]==NULL) {
			logf(LG_FTL, "memory allocation failure") ;
			return SLCD_FALSE ;
		}
		lcd.scr.acText[r][0]='\0' ;
		lcd.scr.piArrows[r]=LCD_ARROW_NONE ;
		lcd.scr.piLineContents[r]=LCD_LINE_CONTENTS_TEXT ;
	}
	
	/* Clear and update screen */
	
	lcd_hwclearscr() ;
	lcd_hwrefresh() ;

	return SLCD_TRUE ;
}

/**
 * lcd_exit
 *
 * This function closes down the LCD library
 * and frees all associated memory.
 **/
void lcd_exit()
{
	int r ;

	/* Free all allocated memory */
	
	if (lcd.scr.piArrows!=NULL) free(lcd.scr.piArrows) ;
	lcd.scr.piArrows=NULL ;

	if (lcd.scr.piLineContents!=NULL) free(lcd.scr.piLineContents) ;
	lcd.scr.piLineContents=NULL ;

	if (lcd.scr.acText!=NULL) for (r=0; r<lcd.hei; r++) {
		if (lcd.scr.acText[r]!=NULL) free(lcd.scr.acText[r]) ;
	}
	free(lcd.scr.acText) ;
	lcd.scr.acText=NULL ;
}

/**
 * lcd_capabilities
 *
 * Returns a bitfield, which represents the capabilities of the LCD display.
 **/
enum slcd_e_caps lcd_capabilities()
{
	return lcd.cap ;
}

/**
 * lcd_hwclearscr
 *
 * Clears the screen's working buffer. lcd_refresh must
 * then be called to cause the LCD display contents to be
 * updated to match. 
 **/
void lcd_hwclearscr()
{
	int r ;
	if (lcd.scr.acText==NULL) return ;
	for (r=0; r<lcd.hei; r++) {
		lcd.scr.acText[r][0]='\0' ;
		lcd.scr.piArrows[r]=LCD_ARROW_NONE ;
	}
	lcd_hwcursor(0, 0, SLCD_OFF) ;
}


/**
 * lcd_hwputline
 * num: line number of destination
 * str: string to print on line (can contain unicode characters)
 * sel: show selection arrows
 *
 * lcd_hwputline updates the given line in the display buffer.
 * The line is left justified.  If sel is SLCD_ARROWS, selection
 * arrows are displayed on the given line.
 * lcd_refresh must be called to update the actual screen.
 * The function returns true on success, or false if the
 * target buffer does not exist.
 **/
int lcd_hwputline(int num, char *str, enum slcd_e_arrows sel)
{
	int i ;
	if (num<0 || num>=lcd.hei) return SLCD_FALSE ;
	
	/* Copy the line */
	for (i=0; i<(lcd.wid*3) && str[i]!='\0'; i++) 
		lcd.scr.acText[num][i]=str[i] ;
	lcd.scr.acText[num][i]='\0' ;
	
	/* Set the arrows requested */
	switch (sel) {
	case SLCD_SEL_ARROWS:
		lcd.scr.piArrows[num]=LCD_ARROW_BOTH ;
		break ;
	case SLCD_SEL_NOARROWS:
		lcd.scr.piArrows[num]=LCD_ARROW_NONE ;
		break ;
	default:
		return SLCD_FALSE ;
	}
	
	return SLCD_TRUE ;
}


/**
 * lcd_brightness
 * @level: Brightness level (0-100)
 *
 * lcd_brightness changes the screen's backlight level
 * to the requested brightness.  This happens immediately
 * and lcd_refresh is not required.
 */
void lcd_brightness(int level)
{
	if ((lcd.cap&SLCD_HAS_BRIGHTNESS)==0) return ;
	if (level<0) level=0 ;
	if (level>100) level=100 ;
	level *=31 ;
	level /= 100 ;
	lcd_hwdoioctl(IOC_LCD_BACKLIGHT, (void *)&level) ; 
}


/**
 * lcd_contrast
 * @level: Contrast level (0-100)
 *
 * lcd_contrast changes the screen's contrast level
 * to the requested value.  This happens immediately
 * and lcd_refresh is not required.
 */
void lcd_contrast(int level)
{
	if ((lcd.cap&SLCD_HAS_CONTRAST)==0) return ;
	if (level<0) level=0 ;
	if (level>100) level=100 ;
	lcd_hwdoioctl(IOC_LCD_SET_CONTRAST, (void *)&level) ; 
}


/**
 * lcd_hwcursor
 * @x: Sets X position
 * @y: Sets Y position
 * @status: Defines whether cursor is on or off
 *
 * lcd_cursor moves the cursor to the specified X,Y
 * position in the buffer.  It is turned on by setting
 * the status to SLCD_ON and off with a status of SLCD_OFF.
 * lcd_refresh must be called to see the cursor actually move.
 * The function returns true on success, and false if
 * the coordinates are out of range.
 **/
int lcd_hwcursor(int x,int y,enum slcd_e_status status)
{
	/* It appears that the drivers only let the cursor work on the first two rows */
	if (x>=0 && x<lcd.wid && y>=0 && y<2 && y<lcd.hei) {
		lcd.scr.iX=x ;
		lcd.scr.iY=y ;
		switch (status) {
		case SLCD_ON:
			lcd.scr.iCursorType=LCD_CURSOR_ON ;
			break ;
		case SLCD_OFF:
			lcd.scr.iCursorType=LCD_CURSOR_OFF ;
			break ;
		}
		return SLCD_TRUE ;
	} else {
		return SLCD_FALSE ;
	}
}	


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
int lcd_seticon(enum slcd_e_icons icon, enum slcd_e_status state)
{
	int mask ;
	
	/* Turn an LED On or Off */
	switch (icon) {
	case SLCD_ICON_MENU:
		mask=RLED_MENU ;
		break ;
	case SLCD_ICON_VOLUME:
		mask=RLED_VOLUME ;
		break ;
	default:
		mask=0 ;
		break ;
	}
	if (mask!=0) {
		if (state==SLCD_ON) {
			lcd.leds |= mask ;
		} else {
			lcd.leds |= mask ;
			lcd.leds ^= mask ;
		}

		if ((lcd.cap&SLCD_HAS_LEDS)!=0) {
			lcd_hwdoioctl(IOC_LCD_LED, (void *)&lcd.leds) ;
		}
		
		return SLCD_TRUE ;
	}

	/* Turn an Icon On or Off */
	mask=0 ;
	switch (icon) {
	case SLCD_ICON_SHIFT:
		mask=LCD_BITMASK_SHIFT ;
		break ;
	case SLCD_ICON_IRADIO:
		mask=LCD_BITMASK_IRADIO ;
		break ;
	case SLCD_ICON_MEDIA:
		mask=LCD_BITMASK_MEDIA ;
		break ;
	case SLCD_ICON_SHUFFLE:
		mask=LCD_BITMASK_SHUFFLE ;
		break ;
	case SLCD_ICON_REPEAT:
		mask=LCD_BITMASK_REPEAT ;
		break ;
	case SLCD_ICON_SLEEP:
		mask=LCD_BITMASK_SLEEP_TIMER ;
		break ;
	case SLCD_ICON_MUTE:
		mask=LCD_BITMASK_MUTE ;
		break ;
	case SLCD_ICON_ALARM:
		mask=LCD_BITMASK_ALARM ;
		break ;
	case SLCD_ICON_SNOOZE:
		mask=LCD_BITMASK_SNOOZE ;
		break ;
	default:
		break ;
	}
	if (mask!=0) {
		if (state==SLCD_ON) {
			lcd.icons |= mask ;
		} else {
			lcd.icons |= mask ;
			lcd.icons ^= mask ;
		}
		if ((lcd.cap&SLCD_HAS_ICONS)!=0) {
			lcd_hwdoioctl(IOC_LCD_DRAW_ICONS, (void *)&lcd.icons) ;			
		}
		return SLCD_TRUE ;
	}
	
	return SLCD_FALSE ;
}


/**
 * lcd_geticon
 * @icon: Icon
 * 
 * This function is used to query the current state of the specified
 * icon.  This function reports the would-be state of the icon, in
 * the event that the icon does not actually exist.
 * This function returns SLCD_ON or SLCD_OFF.
 **/
enum slcd_e_icons lcd_geticon(enum slcd_e_icons icon)
{
	int mask ;
	
	/* Check LEDs */
	switch (icon) {
	case SLCD_ICON_MENU:
		mask=RLED_MENU ;
		break ;
	case SLCD_ICON_VOLUME:
		mask=RLED_VOLUME ;
		break ;
	default:
		mask=0 ;
		break ;
	}
	if (mask!=0) return (lcd.leds&mask)!=0 ;

	/* Check Icons  */
	mask=0 ;
	switch (icon) {
	case SLCD_ICON_SHIFT:
		mask=LCD_BITMASK_SHIFT ;
		break ;
	case SLCD_ICON_IRADIO:
		mask=LCD_BITMASK_IRADIO ;
		break ;
	case SLCD_ICON_MEDIA:
		mask=LCD_BITMASK_MEDIA ;
		break ;
	case SLCD_ICON_SHUFFLE:
		mask=LCD_BITMASK_SHUFFLE ;
		break ;
	case SLCD_ICON_REPEAT:
		mask=LCD_BITMASK_REPEAT ;
		break ;
	case SLCD_ICON_SLEEP:
		mask=LCD_BITMASK_SLEEP_TIMER ;
		break ;
	case SLCD_ICON_MUTE:
		mask=LCD_BITMASK_MUTE ;
		break ;
	case SLCD_ICON_ALARM:
		mask=LCD_BITMASK_ALARM ;
		break ;
	case SLCD_ICON_SNOOZE:
		mask=LCD_BITMASK_SNOOZE ;
		break ;
	default:
		break ;
	}
	if (mask!=0) return (lcd.icons&mask)!=0 ;
	
	return SLCD_FALSE ;
}

/**
 * lcd_width
*
* Returns the LCD display width
**/
int lcd_width()
{
	return lcd.wid ;
}

/**
 * lcd_height
*
* Returns the LCD display height
**/
int lcd_height()
{
	return lcd.hei ;
}

/**
 * lcd_hwrefresh
 *
 * lcd_hwrefresh function causes the contents of the screen
 * buffer to be written to the actual display.  this affects
 * textual content, selection arrows and the cursor.
 **/
void lcd_hwrefresh()
{
	lcd_hwdoioctl(IOC_LCD_DRAW_SCREEN, &lcd.scr) ; 
}

/**
 * lcd_driverclock
 * @clk: current date / time
 * @almon: identifies whether alarm is on or off
 * @alm: alarm date / time
 * @clkmode: display mode for clock (12hr,24hr)
 * @am: AM text
 * @pm: PM text
 * @title: clock title
 * 
 * lcd_driverclock function sets the clock display parameters,
 * and then it will display the clock on the LCD screen.
 * optionally, the alarm time can also be displayed - for this
 * the alm function has to point to a time_t structure, and not
 * be NULL, and the almon status must be SLCD_ON.
 * This function will overwrite whatever has been displayed with the 
 * lcd_refresh function.
 **/
int lcd_hwclock(struct tm *clk, enum slcd_e_status almon, struct tm *alm,
	enum slcd_e_clkmode clkmode, char *am, char *pm, char *title)
{
	struct lcd_draw_clock tmp ;
	struct lcd_ampm_text ampm ;
  
	/* If the driver does not support the clock display, return failed */
	if ((lcd.cap&SLCD_HAS_DRIVERCLOCK)==0) return SLCD_FALSE ;
  
	/* Store the clock title */
	strncpy(lcd.clktitle, title, SLCD_D_CLK_TITLE_LEN-1) ;
	lcd.clktitle[SLCD_D_CLK_TITLE_LEN-1]='\0' ;
  
	/* Store the mode (12hr / 24hr clock */
	if (clkmode==SLCD_CLK_12HR) lcd.clkmode=LCD_DRAW_CLOCK_12HR ;
	else lcd.clkmode=LCD_DRAW_CLOCK_24HR ;
  
  
	/* Store the AM/PM text */
	ampm.pcAM=am ;
	ampm.pcPM=pm ;
	lcd_hwdoioctl(IOC_LCD_SET_AMPM_TEXT, &ampm) ;

	/* Set up the time and alarm structures */
	tmp.iHours=clk->tm_hour ;
	tmp.iMinutes=clk->tm_min ;
	tmp.iSeconds=clk->tm_sec ;
	tmp.iMode=lcd.clkmode ;
	if (almon && alm!=NULL) {
		tmp.iAlarmOn=SLCD_TRUE ;
		tmp.iAlarmHours=alm->tm_hour ;
		tmp.iAlarmMinutes=alm->tm_min ;
	} else {
		tmp.iAlarmOn=SLCD_FALSE ;
		tmp.iAlarmHours=0 ;
		tmp.iAlarmMinutes=0 ;
	}
	tmp.pcDateString=lcd.clktitle ;
	return (lcd_hwdoioctl(IOC_LCD_DRAW_CLOCK, &tmp)!=(-1)) ;
}

/**
 * lcd_hwdoioctl
 * @iot: IOCTL
 * @arg: argument
 *
 * this function calls and ioctl, and returns true on success
 * or false on failure.
 **/
int lcd_hwdoioctl(int iot, void *arg) {
	int fd;
	int r;

	fd = open("/dev/misc/lcd", O_RDWR);
	if(fd == -1) {
		logf(LG_FTL, "IOCTL, unable to open /dev/misc/lcd") ;
		return (1==0) ;	
	}

	r = ioctl(fd, iot, arg);

	if (r==(-1)) {
		logf(LG_INF, "SLCD_doioctl failed: dir=%d, type=%d, nr=%d, size=%d, err=%s\n", 
			_IOC_DIR(iot), _IOC_TYPE(iot), _IOC_NR(iot), _IOC_SIZE(iot), strerror(errno)) ;
	}

	close(fd) ;
	return (r!=(-1)) ;
}

/**
 * lcd_hwgetscreen
 *
 * This function returns a pointer to a
 * character array, with the current
 * screen contents that will be shown on
 * the next lcd_hwrefresh()
 **/
char ** lcd_hwgetscreen() {
	return lcd.scr.acText ;
}

/**
 * lcd_hwgetselrows
 *
 * This function returns a pointer to an
 * integer array, identifying if the 
 * corresponding row has <> selection
 * arrows or not.  The s that will be shown on
 * the next lcd_hwrefresh()
 **/
enum slcd_e_arrows * lcd_hwgetselrows() {
	return (enum slcd_e_arrows *) lcd.scr.piArrows ;
}

/**
 * lcd_grab_region
 *
 * lcd_grab_region function grabs the contents of the screen
 * buffer 
 **/
struct bitmap_data*lcd_grab_region(int top,int left,int width,int height)
{
	struct bitmap_data*bitmap=(struct bitmap_data*)malloc(sizeof(struct bitmap_data));
	bitmap->top=top;
	bitmap->left=left;
	bitmap->width=width;
	bitmap->height=height;
	lcd_hwdoioctl(IOC_LCD_GRAB_SCREEN_REGION,bitmap); 
	return bitmap;
}
