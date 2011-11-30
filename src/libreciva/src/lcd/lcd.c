/* 
 * Sharpfin project
 * Copyright (C) by Steve Clarke and Ico Doornekamp
 * 2011-11-30 Philipp Schmidt
 *   Added to github 
 *
 * This file is part of the sharpfin project
 * LCD access,control and maintenance
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
#include "lcd.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <stdio.h>
/*************************
 * Local Variables, defs and types
 *************************/
/**
 * lcd_currentscreen
 *
 * This variable maintains a pointer to the current screen.
 * it is used by the lcdif_tick() function to identify what
 * (if any) scrolling is required.
 **/
static lcd_handle *lcd_currentscreen=NULL ;
static char *lcd_textbuildscrollingline(struct slcd_s_row *row) ;
static char *lcd_inputbuildscrollingline(char *selstr, int selection) ;
static char *lcd_inputbuildinputline(char *src, int bol, int cursor) ;
static int lcd_getnextcharacterpos(char *str, int curpos) ;
static int lcd_getprevcharacterpos(char *str, int curpos)  ;
static int lcd_insertchar(char *str, int pos, char ch, int maxlen) ;
static int lcd_strcatc(char *buf, int maxlen, char c) ;
static int lcd_strlen(char *buf) ;
static char * lcd_getutf8char(char *str, int pos) ;
static char *lcd_parsespecial(char *t) ;
//static void lcd_dumpvars(lcd_handle *handle, char *comment, int n) ;
#define SLCD_PRINTF_BUFSIZE 256
#define SLCD_TEXTBUF_MAXLEN 128
#define SLCD_TABSIZE 4

/*
 * ident
 */
char *lcd_ident() {
        return "$Id: lcd.c 230 2007-10-23 17:42:49Z  $" ;
}

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
lcd_handle *lcd_menucreate() {
	lcd_handle *h ;
	/* Allocate / Create the a lcdif_handle structure */
	h=malloc(sizeof(lcd_handle)) ;
	if (h==NULL) {
		logf(LG_FTL, "memory allocation failure") ;
		return NULL ;
	}
	
	h->type=SLCD_MENU ;
	h->top=NULL ;
	h->end=NULL ;
	h->tsc=NULL ;
	h->curl=NULL ;
	h->numrows=0 ;

	h->linebuf=malloc(sizeof(char)*lcd_width()*3+1) ;
	if (h->linebuf==NULL) {
		logf(LG_FTL, "memory allocation failure") ;
		free(h) ;
		return NULL ;
	}

	/* Frame specific parameters */
	h->statusrow=(-1) ;
	h->showicons=SLCD_FALSE ;
	
	/* Clock specific parameters */
	h->alm.tm_hour=0 ;
	h->alm.tm_min=0 ;
	h->almon=SLCD_FALSE ;

	/* Input specific parameters */
	h->inputselectedopt=0 ;
	h->inputcursorpos=0 ;
	h->inputbolpos=0 ;
	h->result=NULL ;
	h->selectopts=NULL ;
	h->maxlen=0 ;
	
	/* YesNo specific parameters */
	h->yesnoopt=0 ;
	h->yesnomax=0 ;
	h->yesnoopts=NULL ;
	return h ;
}	
	
/**
 * lcd_menuclear
 * @handle: pointer to the menu structure to clear
 *
 * This function clears the menu, removing all of the
 * rows, and putting it back into its original state
 * This function returns true on success.
 *
 **/
int lcd_menuclear(lcd_handle *handle) {
	struct slcd_s_row *p, *q ;
	if (handle==NULL) {
		logf(LG_ERR, "attempt to clear NULL menu") ;
		return SLCD_FALSE ;
	}

	/* Free up linked list memory */
	p=handle->top ;
	handle->tsc=NULL ;
	handle->curl=NULL ;
	handle->top=NULL ;
	handle->numrows=0 ;
	if (handle->end!=NULL) handle->end->next=NULL ;
	handle->end=NULL ;
	while (p!=NULL) {
		if (p->str!=NULL) free(p->str) ;
		q=p ;
		p=p->next ;
		free(q) ;
	}
	return SLCD_TRUE ;
}
 
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
int lcd_menuaddentry(lcd_handle *handle, int idnumber, char *str, enum slcd_e_select selected) {
	struct slcd_s_row *row ;
	if (handle==NULL || str==NULL) return SLCD_FALSE ;
	/* Allocate and fill new row data */
	row=malloc(sizeof(struct slcd_s_row)) ;
	if (row==NULL) {
		logf(LG_FTL, "memory allocation failure") ;
		return SLCD_FALSE ;
	}
	row->idnumber=idnumber ;
	row->scrollpos=0 ;
	row->str=malloc(strlen(str)+1) ;
	if (row->str==NULL) {
		logf(LG_FTL, "memory allocation failure") ;
		free(row) ;
		return SLCD_FALSE ;
	}
	strcpy(row->str, str) ;
	row->len=lcd_strlen(str) ;
	
	/* Create the linked list - note that the linked list is designed to be */
	/* circular, so that the menu entries wrap around with the minimum */
	/* amount of code */
	if (handle->top==NULL) {		/* Add first entry in the list, selecting it */
		handle->top=row ;
		handle->end=row ;
		row->prev=row ;	 /* make it circular */
		row->next=row ;
		handle->tsc=row ; /* select it */
		handle->curl=row ;
	} else {				/* Add more entries */
		row->prev=handle->end ;
		handle->end->next=row ;
		row->next=handle->top ;
		handle->top->prev=row ;
		handle->end=row ;
	}
	
	/* Override row that is at the top of the screen  */
	if (selected==SLCD_SELECTED) {		
		handle->curl=row ;
		handle->tsc=row ;
	}
	
	/* Increment the row count and return */
	handle->numrows++ ;
	return SLCD_TRUE ;
}

/**
 * lcd_menusort
 * @handle: handle of screen buffer
 *
 * This function sorts the menu into alphabetical
 * order.
 **/
void lcd_menusort(lcd_handle *handle) {
	int i, j ;
	struct slcd_s_row *p, *t1, *t2, *t3, *t4 ;
	
	if (handle==NULL) {
		logf(LG_FTL, "NULL handle passed to function") ;
		return ;
	}
	if (handle->top==NULL || handle->numrows<2) {
		logf(LG_INF, "Screen has insufficient lines") ;
		return ;
	}
	
	/* bubble sort */
	for (i=0; i<handle->numrows-1; i++) {
		p=handle->top ;
		for (j=i; j<handle->numrows-1; j++) {
			t1=p->prev ;
			t2=p ;
			t3=p->next ;
			t4=p->next->next ;
			if (strcmp(t1->str, t2->str)>0) {
				/* t1 -> t2 -> t3 -> t4 */
				/* t1 <- t2 <- t3 <- t4 */
				/*       becomes	 */
				/* t1 -> t3 -> t2 -> t4 */
				/* t1 <- t3 <- t2 <- t4 */
				t1->next=t3 ;
				t3->next=t2 ;
				t2->next=t4 ;
				t4->prev=t2 ;
				t2->prev=t3 ;
				t3->prev=t1 ;
				if (handle->top==t2) handle->top=t3 ;
				if (handle->end==t3) handle->end=t2 ;
			} else {
				p=p->next ;
			}
		}
	}
	/* put current line at top of screen */
	handle->tsc=handle->curl ;
}
 
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
int lcd_menucontrol(lcd_handle *handle, enum slcd_e_menuctl cmd) {
	int r ;
	struct slcd_s_row *p ;
	
	/* The screen is empty, so return */
	if (handle==NULL) {
		logf(LG_FTL, "NULL handle passed to function") ;
		return SLCD_FALSE ;
	}
	
	/* The screen has no lines, so return */
	if (handle->curl==NULL) {
		logf(LG_INF, "screen has no lines") ;
		return SLCD_FALSE ;
	}
	
	/* Reset the current line's scroll position */
	handle->curl->scrollpos=0 ;
	
	switch (cmd) {
	case SLCD_UP:
		/* We scroll up if the menu is larger than a screen AND we are at the top */
		if (handle->numrows>lcd_height() && 
				handle->curl==handle->tsc) {
			handle->tsc=handle->tsc->prev ;
		}
		handle->curl=handle->curl->prev ;
		break ;
	case SLCD_DOWN:
		/* We scroll down if the menu is larger than a screen  AND we are at the bottom */
		if (handle->numrows>lcd_height()) {
			for (r=1, p=handle->tsc; r<lcd_height(); r++, p=p->next) ;
			if (handle->curl==p) {
				handle->tsc=handle->tsc->next ;
			}
		}
		handle->curl=handle->curl->next ;
		break ;
	}
	return SLCD_TRUE ;
}

/**
 * lcd_menugetselid
 * @handle: handle of the screen buffer
 *
 * This function returns the selected line's idnumber.
 **/
int lcd_menugetselid(lcd_handle *handle) {
	/* Invalid pointer, so return */
	if (handle==NULL) {
		logf(LG_FTL, "NULL handle passed to function") ;
		return -1 ;
	}
	/* The screen has no lines, so return */
	else if (handle->curl==NULL) {
		logf(LG_INF, "screen has no lines") ;
		return -1 ;
	}
	else return handle->curl->idnumber ;
}

/**
 * lcd_menugetsels
 * @handle: handle of the screen buffer
 *
 * This function returns a pointer to selected line's string
 **/
char *lcd_menugetsels(lcd_handle *handle) {
	/* Invalid pointer, so return */
	if (handle==NULL) {
		logf(LG_FTL, "NULL handle passed to function") ;
		return "" ;
	}
	/* The screen has no lines, so return */
	else if (handle->curl==NULL) {
		logf(LG_INF, "screen has no lines") ;
		return "" ;
	}
	/* Memory pointers all wrong ... */
	else if (handle->curl->str==NULL) {
		logf(LG_FTL, "current line invalid.  Was there a memory allocation error?") ;
		return "" ;
	}
	else return handle->curl->str ;

}

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
lcd_handle *lcd_framecreate() {
	lcd_handle *h ;
	int r ;
	
	/* Allocate / Create the a lcd_handle structure (use menucreate) */
	h=lcd_menucreate() ;
	if (h==NULL) return NULL ;
	h->type=SLCD_FRAME ;

	/* Add blank lines to fill the frame (use menuaddentry) */
	for (r=0; r<lcd_height(); r++) {
		if (!lcd_menuaddentry(h, r, "", SLCD_NOTSELECTED)) {
			lcd_delete(h) ;
			return NULL ;
		}
	}
	return h ;
}

/**
 * lcd_framesetline
 * @handle: handle of screen buffer
 * @line: line number to set
 * @str: string to set line to
 *
 * This function sets the specified line text.
 * The function returns true on success, and false if out of memory.
 **/
int lcd_framesetline(lcd_handle *handle, int line, char *str) {
	struct slcd_s_row *p ;
	int len ;
	/* Invalid pointer, so return */
	if (handle==NULL || str==NULL) {
		logf(LG_FTL, "NULL handle / text passed to function") ;
		return SLCD_FALSE ;
	}
	/* The screen has no lines, so return */
	else if (handle->top==NULL) {
		logf(LG_INF, "screen has no lines") ;
		return SLCD_FALSE ;
	}
	/* Scan through list, looking for line (idnumber) */
	for (p=handle->top; p!=handle->end && p->idnumber!=line; p=p->next) ;
	if (p->idnumber!=line) return SLCD_FALSE ;
	/* re-allocate memory */
	if (p->str!=NULL) free(p->str) ;
	len=strlen(str)+1 ;
	p->str=malloc(len) ;
	if (p->str==NULL) {
		logf(LG_FTL, "memory allocation failure") ;
		return SLCD_FALSE ;
	}
	p->str[0]='\0' ;
	/* copy string */
	strcpy(p->str, str) ;
	p->scrollpos=0 ;
	p->len=lcd_strlen(p->str) ;
	return SLCD_TRUE ;
}

/**
 * lcd_frameprintf
 * @handle: handle of screen buffer
 * @line: destination line on the screen
 * @fmt: printf format
 *
 * This function sets the specified line text, using a printf structure.
 * The function returns true on success, and false if out of memory.
 **/ 
int lcd_frameprintf(lcd_handle *handle, int line, char *fmt, ...) {
	va_list va;
	char buf[SLCD_PRINTF_BUFSIZE];
	int r;

	if (handle==NULL || fmt==NULL) return SLCD_FALSE ;
	
	va_start(va, fmt);
	r = vsnprintf(buf, SLCD_PRINTF_BUFSIZE, fmt, va);
	va_end(va);
	if (r>=SLCD_PRINTF_BUFSIZE) return SLCD_FALSE ;

	return lcd_framesetline(handle, line, buf);
}

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
int lcd_framebar(lcd_handle *handle, int line, int min, int max, int progress, enum slcd_e_bartype type)
{
	int i, l ;
	char half[4], whole[4] ;

	/* Invalid pointer, so return */
	if (handle==NULL || handle->linebuf==NULL) {
		logf(LG_FTL, "NULL handle / NULL linebuf passed to function") ;
		return SLCD_FALSE ;
	}
		
	if (type==SLCD_BARROWS) {
		strcpy(half," ") ;
		strcpy(whole, SLCD_UCHR_RIGHT) ;
	} else {
		strcpy(half,SLCD_UCHR_VOL1) ;
		strcpy(whole, SLCD_UCHR_VOL2) ;
	}
	
	if (progress<min) progress=min ;
	if (progress>max) progress=max ;
	
	l= ( ( ( lcd_width()-2 ) * 2 * (progress-min) ) / (max-min) ) ;
	
	strcpy(handle->linebuf,"|") ;
	for (i=0; i<(l-2); i+=2) {
		strcat(handle->linebuf, whole) ;
	}
	
	if (i==0) {
		/* length is 0, append nothing */
	} else if (i==l-1) {
		/* Length is odd, append half character */
		strcat(handle->linebuf, half) ;
		i++ ;
	} else if (i==l-2) {
		/* Length is even, append whole character */
		strcat(handle->linebuf, whole) ;
		i++ ;
	}
	
	while (lcd_strlen(handle->linebuf)<(lcd_width()-1)) strcat(handle->linebuf," ") ;
	strcat(handle->linebuf,"|") ;

	return lcd_framesetline(handle, line, handle->linebuf) ;
}

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
enum slcd_e_status lcd_framestatus(lcd_handle *handle, int line, enum slcd_e_status showicons)
{
	/* Invalid pointer, so return */
	if (handle==NULL) {
		logf(LG_FTL, "NULL handle passed to function") ;
		return SLCD_FALSE ;
	}
	handle->statusrow=line ;
	handle->showicons=showicons ;
	return SLCD_TRUE ;
}


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
lcd_handle *lcd_inputcreate(char *selection, char *result, int maxlen)
{
	lcd_handle *h ;
	
	/* Invalid pointer, so return */
	if (result==NULL || selection==NULL) {
		logf(LG_FTL, "NULL result / selection passed to function") ;
		return NULL ;
	}
	
	else if (maxlen<1) {
		logf(LG_ERR, "maxlen too small") ;
		return NULL ;
	}
	
	/* Check that the selection is not too long */
	if (strlen(selection)>(SLCD_TEXTBUF_MAXLEN-strlen(SLCD_USTR_INPUTMENU)-1)) {
		logf(LG_ERR, "selection too long") ;
		return NULL ;
	}
	
	/* Allocate / Create the a lcdif_handle structure */
	h=lcd_framecreate() ;
	if (h==NULL) return NULL ;
	h->type=SLCD_INPUT ;

	/* Store the selection */
	h->selectopts=malloc(strlen(selection)+strlen(SLCD_USTR_INPUTMENU)+1) ;
	if (h->selectopts==NULL) {
		logf(LG_FTL, "memory allocation failure") ;
		lcd_delete(h) ;
		return NULL ;
	}
	strcpy(h->selectopts, selection) ;
	strcat(h->selectopts, SLCD_USTR_INPUTMENU) ;

	/* Store a pointer to result */
	h->result=result ;
	h->maxlen=maxlen ;
	
	/* Force Calculation of Cursor Position */
	h->inputbolpos=(-1) ;
	h->inputcursorpos=(-1) ;
	
	/* Scroll left on the input selector to END (and force frame to be updated) */
	lcd_inputcontrol(h, SLCD_LEFT) ;
	
	return h ;
}

/**
 * lcd_inputsetline
 * @handle: handle of screen buffer
 * @line: line number to set
 * @str: string to set line to
 *
  * This function sets the specified line text (lines 0 and 1 are reserved)
* The function returns true on success, and false if out of memory.
 **/
int lcd_inputsetline(lcd_handle *handle, int line, char *str) 
{
	if (handle==NULL) {
		logf(LG_ERR, "NULL handle passed to function") ;
		return SLCD_FALSE ;
	}
	if (line<2) {
		logf(LG_ERR, "attempt to set top two lines of input screen") ;
		return SLCD_FALSE ;
	}
	else return lcd_framesetline(handle, line, str) ;
}

/**
 * lcd_inputprintf
 * @handle: handle of screen buffer
 * @line: destination line on the screen
 * @fmt: printf format
 *
 * This function sets the specified line text, using a printf structure.
 * The function returns true on success, and false if out of memory.
 **/ 
int lcd_inputprintf(lcd_handle *handle, int line, char *fmt, ...) 
{
	va_list va;
	char buf[SLCD_PRINTF_BUFSIZE];
	int r;

	if (handle==NULL || fmt==NULL) {
		logf(LG_ERR, "NULL handle / format passed to function") ;
		return SLCD_FALSE ;
	}
	
	va_start(va, fmt);
	r = vsnprintf(buf, SLCD_PRINTF_BUFSIZE, fmt, va);
	va_end(va);
	if (r>=SLCD_PRINTF_BUFSIZE) return SLCD_FALSE ;

	return lcd_inputsetline(handle, line, buf);
}

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
int lcd_inputcontrol(lcd_handle *handle, enum slcd_e_inputctl cmd)
{
	char *u8 ;
		
	if (handle==NULL) {
		logf(LG_ERR, "NULL handle passed to function") ;
		return SLCD_TRUE ; /* Return as if END found */
	}
	
	/* Initialise cursor position */
	if (handle->inputbolpos<0) {
		handle->inputcursorpos=strlen(handle->result) ;
		if (handle->inputcursorpos < lcd_width()) {
			handle->inputbolpos=0 ;
		} else {
			handle->inputbolpos=handle->inputcursorpos-lcd_width()+1 ;
		}
	}
	
	if (cmd==SLCD_LEFT) {
		/* Move MENU Chooser String to the Left */
		handle->inputselectedopt=lcd_getprevcharacterpos(handle->selectopts, 
			handle->inputselectedopt) ;
		/* Skip ND and EL */
		u8=lcd_getutf8char(handle->selectopts, handle->inputselectedopt) ;
		while (strcmp(u8, SLCD_UCHR_END2)==0 || strcmp(u8, SLCD_UCHR_END3)==0 ||
				strcmp(u8, SLCD_UCHR_DEL2)==0 || strcmp(u8, SLCD_UCHR_DEL3)==0) {
			handle->inputselectedopt=lcd_getprevcharacterpos(
				handle->selectopts, handle->inputselectedopt) ;
			u8=lcd_getutf8char(handle->selectopts, handle->inputselectedopt) ;
		}
	}
	
	if (cmd==SLCD_RIGHT) {
		/* Move MENU Chooser String to the Right */
		handle->inputselectedopt=lcd_getnextcharacterpos(handle->selectopts,
			handle->inputselectedopt) ;

		/* Skip ND and EL */
		u8=lcd_getutf8char(handle->selectopts, handle->inputselectedopt) ;
		while (strcmp(u8, SLCD_UCHR_END2)==0 || strcmp(u8, SLCD_UCHR_END3)==0 ||
				strcmp(u8, SLCD_UCHR_DEL2)==0 || strcmp(u8, SLCD_UCHR_DEL3)==0) {
			handle->inputselectedopt=lcd_getnextcharacterpos(
				handle->selectopts, handle->inputselectedopt) ;
			u8=lcd_getutf8char(handle->selectopts, handle->inputselectedopt) ;
		}
	}
	
	if (cmd==SLCD_ENTER) {
		/* Act on selected character in MENU Chooser */
		u8=lcd_getutf8char(handle->selectopts, handle->inputselectedopt) ;
		if (strcmp(u8, SLCD_UCHR_END1)==0) {
			/* END Selected, Return True */
			return SLCD_TRUE ;
		} else if (strcmp(u8, SLCD_UCHR_DEL1)==0) {
			/* DEL char to the left of Selected */
			if (handle->inputcursorpos>=1) {
				handle->inputcursorpos=
					lcd_insertchar(handle->result, 
					handle->inputcursorpos, '\x0',
					handle->maxlen) ;			
			}
		} else if (strcmp(u8, SLCD_UCHR_LEFT)==0) {
			/* Move Cursor Left */
			if (handle->inputcursorpos>0) {
				/* Adjust actual cursor position */
				handle->inputcursorpos-- ;				
				/* If moved off beginning of screen, move BOL back too */
				if (handle->inputcursorpos<handle->inputbolpos)
					handle->inputbolpos=handle->inputcursorpos ;
			}
		} else if (strcmp(u8, SLCD_UCHR_RIGHT)==0) {
			/* Move Cursor Right */
			if (handle->inputcursorpos<strlen(handle->result)) {
				/* Adjust actual cursor position */
				handle->inputcursorpos++ ;
				/* if moved off end of screen, advance BOL to ensure cursor remains viewable */
				if ((handle->inputcursorpos-handle->inputbolpos)>lcd_width())
					handle->inputbolpos=handle->inputcursorpos-lcd_width() ;
			}
		} else {
			/* Insert Character at cursorpos */
			handle->inputcursorpos=lcd_insertchar(handle->result, 
				handle->inputcursorpos, u8[0], handle->maxlen) ;
		}
	}
	
	/* Adjust the beginning of line */
	if (handle->inputcursorpos<handle->inputbolpos)
		handle->inputbolpos=handle->inputcursorpos ;
	if (handle->inputcursorpos>(handle->inputbolpos+lcd_width()))
		handle->inputbolpos=handle->inputcursorpos-lcd_width()+1 ;
	
	return SLCD_FALSE ;
}

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
lcd_handle *lcd_yesnocreate(char *yesno, char *title, int defopt)
{
	lcd_handle *h ;
	int i ;
	
	if (yesno==NULL || title==NULL) {
		logf(LG_ERR, "NULL yesno / title passed to function") ;
		return NULL ;
	}
	
	h=lcd_framecreate() ;
	if (h==NULL) return NULL ;
	h->type=SLCD_YESNO ;

	/* Store options and title */
	h->yesnoopts=malloc(strlen(yesno)+1) ;
	if (h->yesnoopts==NULL) {
		logf(LG_FTL, "memory allocation failure") ;
		lcd_delete(h) ;
		return NULL ;
	}
	strcpy(h->yesnoopts, yesno) ;
	lcd_framesetline(h, 0, title) ;
	
	/* Set parameters */
	h->yesnoopt=defopt ;
	for (h->yesnomax=0, i=0; h->yesnoopts[i]!='\0'; i++) {
		if (h->yesnoopts[i]=='/') h->yesnomax++ ;
	}
	
	return h ;
}


/**
 * lcd_yesnocontrol
 * @handle: handle of screen buffer
 * @cmd: direction to move, or enter
 *
 * lcd_yesnocontrol is used to scroll through the yesno 
 * options with the SLCD_LEFT and SLCD_RIGHT commands.
 * The function returns true on success.
 **/
int lcd_yesnocontrol(lcd_handle *handle, enum slcd_e_inputctl cmd)
{
	if (handle==NULL) return SLCD_FALSE ;
	
	switch (cmd) {
	case SLCD_LEFT:
		if (handle->yesnoopt>0) handle->yesnoopt-- ;
		break ;
	case SLCD_RIGHT:
		if (handle->yesnoopt<handle->yesnomax) handle->yesnoopt++ ;
		break ;
	default:
		logf(LG_ERR, "incorrect command passed to function") ;
		return SLCD_FALSE ;
		break ;
	}
	return SLCD_TRUE ;
}

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
int lcd_yesnoresult(lcd_handle *handle)
{
	if (handle==NULL) {
		logf(LG_ERR, "NULL handle passed to function") ;
		return 0 ;
	}
	return handle->yesnoopt ;
}
/*************************
 *Clock Functions
 *************************/

/**
 *lcd_clockcreate()
 * @title: screen title
 * 
 * This function creates a clock buffer type.
 **/
lcd_handle *lcd_clockcreate(char *title)
{
	lcd_handle *h ;

	if (title==NULL) {
		logf(LG_ERR, "NULL title passed to function") ;
		return NULL ;
	}
	
	/* Allocate / Create the a lcdif_handle structure (use framecreate) */
	h=lcd_framecreate() ;
	if (h==NULL) return NULL ;
	h->type=SLCD_CLOCK ;

	/* framebuffer's top line used to store title, all others are automagically re-generated in lcd_refresh() */
	if (!lcd_framesetline(h, 0, title)) {
		free(h) ;
		return NULL ;
	} else {
		return h ;
	}
}

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
void lcd_clocksetalarm(lcd_handle *handle, struct tm *alm, enum slcd_e_status alarmon)
{
	if (handle==NULL || alm==NULL) {
		logf(LG_ERR, "NULL handle / alm passed to function") ;
		return ;
	}
	handle->almon=alarmon ;
	memcpy((void *)&(handle->alm), (void *)alm, sizeof(struct tm)) ;
}


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
void lcd_delete(lcd_handle *handle)
{
/* FIXME: Double check and Make sure that ALL possible allocations are deleted */
	if (handle==NULL) {
		logf(LG_ERR, "attempt to delete NULL structure") ;
		return ;
	}	

	/* Remove any lines in the screen */
	lcd_menuclear(handle) ;

	/* Release memory */
	if (handle->linebuf!=NULL) free(handle->linebuf) ;
	if (handle->selectopts!=NULL) free(handle->selectopts) ;
	if (handle->yesnoopts!=NULL) free(handle->yesnoopts) ;
	free(handle) ;
}

/**
 * lcd_refresh
 * @handle: handle for the screen buffer
 *
 * This function transfers the contents of
 * the screen buffer identified by the provided
 * handle to the LCD.
 **/
void lcd_refresh(lcd_handle *handle)
{
	int r=0, i, j ;
	struct slcd_s_row *p=NULL ;
	time_t rawtime ;
	struct tm *timeinfo ;

	/* There is nothing in the buffers, so clear the screen and leave */
	if (handle==NULL) {
		logf(LG_ERR, "NULL handle passed to function") ;
		lcd_hwclearscr() ;
		return ;
	}
	if (handle->tsc==NULL) {
		lcd_hwclearscr() ;
		return ;
	}

	switch (handle->type) {
	case SLCD_MENU:
		lcd_hwcursor(0, 0, SLCD_OFF) ;
		for (r=0, p=handle->tsc; r<handle->numrows && r<lcd_height(); r++, p=p->next) {
			if (p==handle->curl) {
				lcd_hwputline(r, lcd_textbuildscrollingline(p), SLCD_SEL_ARROWS) ;
			} else {
				lcd_hwputline(r, p->str, SLCD_SEL_NOARROWS) ;
			}
		}
		for (; r<lcd_height(); r++) {
			lcd_hwputline(r, "", SLCD_SEL_NOARROWS) ;
		}
		
		break ;

	case SLCD_CLOCK:
		lcd_hwcursor(0, 0, SLCD_OFF) ;
		time ( &rawtime );
		timeinfo = localtime ( &rawtime );
		if ((lcd_capabilities()&SLCD_HAS_DRIVERCLOCK)!=0) {
			logf(LG_DBG, "refreshing hardware clock, %02d:%02d", timeinfo->tm_hour, timeinfo->tm_min) ;
			lcd_hwclock(timeinfo, handle->almon, &(handle->alm), 
			SLCD_CLK_24HR, "AM", "PM", handle->top->str) ;
		} else {
			char buf[15] ;
			logf(LG_DBG, "refreshing software clock, %02d:%02d", timeinfo->tm_hour, timeinfo->tm_min) ;
			lcd_hwclearscr() ;
			lcd_hwputline(0, handle->top->str, SLCD_SEL_NOARROWS) ;
			strftime(buf, 14, "%H:%M", timeinfo) ;
			lcd_hwputline(1, buf, SLCD_SEL_NOARROWS) ;
			strftime(buf, 14, "%a, %d-%b ", timeinfo) ;
			lcd_hwputline(2, buf, SLCD_SEL_NOARROWS) ;
			if (handle->almon) {
				strftime(buf, 14, "(al %H:%M)", &(handle->alm)) ;
				lcd_hwputline(3, buf, SLCD_SEL_NOARROWS) ;
			}
		}
		break ;

	case SLCD_FRAME:
		/* Override Status Row with time and Icons */
		if (handle->statusrow>=0) {
			time ( &rawtime );
			timeinfo = localtime ( &rawtime );
			if ((lcd_capabilities(handle)&SLCD_HAS_ICONS)==0 && handle->showicons) {
				lcd_frameprintf(handle, handle->statusrow, "%s%s   %02d:%02d %s%s",
					lcd_geticon(SLCD_ICON_REWIND)?SLCD_UCHR_REW:
					lcd_geticon(SLCD_ICON_PAUSE)?SLCD_UCHR_PAUSE:
					lcd_geticon(SLCD_ICON_STOP)?SLCD_UCHR_STOP:
					lcd_geticon(SLCD_ICON_FF)?SLCD_UCHR_FF:" ",
					lcd_geticon(SLCD_ICON_MUTE)?SLCD_UCHR_MUTE:" ",
					timeinfo->tm_hour, timeinfo->tm_min, 
					lcd_geticon(SLCD_ICON_SLEEP)?"Zzz":"   ",
					lcd_geticon(SLCD_ICON_ALARM)?"A":" ") ;
				
			} else {
				lcd_frameprintf(handle, handle->statusrow, "%2d:%2d",
					timeinfo->tm_hour, timeinfo->tm_min) ;
			}
		}
		/* Update the frame */
		lcd_hwcursor(0, 0, SLCD_OFF) ;
		for (r=0, p=handle->tsc; r<lcd_height(); r++, p=p->next) { 		
			lcd_hwputline(r, lcd_textbuildscrollingline(p), SLCD_SEL_NOARROWS) ;
		}
		break ;

	case SLCD_INPUT:
		lcd_hwputline(0, lcd_inputbuildscrollingline(handle->selectopts, handle->inputselectedopt), SLCD_SEL_ARROWS) ;
		lcd_hwcursor(handle->inputcursorpos-handle->inputbolpos-1, 1, SLCD_ON) ;
		lcd_hwputline(1, lcd_inputbuildinputline(handle->result, handle->inputbolpos, handle->inputcursorpos), SLCD_SEL_NOARROWS) ;
		break ;

	case SLCD_YESNO:

		/* build the second line (yes/no text line) */
		handle->linebuf[0]='\0' ;
		for (i=0, j=0; i<lcd_width() && handle->yesnoopts[i]!='\0'; i++) {

		if (handle->yesnoopts[i]=='/') {
				/* Close previous */
				if (handle->yesnoopt==j-1) {
					strcat(handle->linebuf,">") ;
				}
				strcat(handle->linebuf,"/") ;
			}
			

			if (i==0 || handle->yesnoopts[i]=='/') {
				/* Open next */
				if (handle->yesnoopt==j) {
					strcat(handle->linebuf, "<") ;
				}
				j++ ;
			}

			if (handle->yesnoopts[i]!='/') {
				lcd_strcatc(handle->linebuf, lcd_width(), handle->yesnoopts[i]) ;
			}
			
		}
		
		/* Close the line */
		if (handle->yesnoopt==handle->yesnomax) {
			strcat(handle->linebuf,">") ;
		}

		/* Set the display text, overriding row 1 */
		lcd_hwcursor(0, 0, SLCD_OFF) ;
		for (r=0, p=handle->top; r<lcd_height(); r++, p=p->next) { 
			if (r==1) {
				lcd_hwputline(r, handle->linebuf, SLCD_SEL_NOARROWS) ;
			} else {
				lcd_hwputline(r, lcd_textbuildscrollingline(p), SLCD_SEL_NOARROWS) ;
			}
		}
		break ;

	}
	lcd_hwrefresh() ;
	
	/* Update screen that will be automatically refreshed by lcd_tick */
	lcd_currentscreen=handle ;
}

/**
 * lcd_tick
 *
 * lcd_tick animates the currently displayed
 * screen.
 **/
void lcd_tick()
{
	static int lastmins=-1 ;
	static time_t rawtime ;
	static struct tm *timeinfo ;
	
	if (lcd_currentscreen==NULL) return ;
	switch (lcd_currentscreen->type) {
	case SLCD_MENU:
	case SLCD_FRAME:
		lcd_refresh(lcd_currentscreen) ;
		lastmins=-1 ;
		break ;
	case SLCD_CLOCK:
		/* Update screen if time has changed */
		time (&rawtime) ;
		timeinfo=localtime(&rawtime) ;
		if (lastmins!=timeinfo->tm_min) {
			lastmins=timeinfo->tm_min ;
			lcd_refresh(lcd_currentscreen) ;
		}
		break ;
	default:
		break ;
	}
}

/**
 * lcd_fetch
 * buf: buffer for current screen output
 * max: maximum buffer size
 *
 * lcd_fetch copies to buf a text string which represents the 
 * current screen buffer contents.
 * The function returns true on success, or false if the buffer is
 * too small, or there are memory allocation problems.
 **/
int lcd_fetch(lcd_handle *handle, char *buf, int max) 
{
	return SLCD_TRUE ;
}

/**
 * Local support functions
 **/

/* Appends character to a string and returns length */
int lcd_strcatc(char *buf, int maxlen, char c)
{
	int i ;
	if (buf==NULL) return 0 ;
	i=strlen(buf) ;
	if (i>=maxlen) return i ;
	buf[i++]=c ;
	buf[i]='\0' ;
	return i ;
}
 
/* Calculate UTF8 string length */
int lcd_strlen(char *buf)
{
	int l, i ;
	if (buf==NULL) return 0 ;
	for (l=0,i=0; buf[i]!='\0';) {
		if ((buf[i]&'\xC0')=='\xC0')
			for (i++; (buf[i]&'\xC0')=='\x80'; i++) ;
		else
			i++ ;
		l++ ;
	}
	return l ;
}
		
/* Looks to the right of curpos, and returns the position of the next character (UTF8 aware) */
int lcd_getnextcharacterpos(char *str, int curpos)
{
	curpos++ ;
	while ((str[curpos]&'\xC0')=='\x80') curpos++ ;
	if (str[curpos]=='\0') curpos=0 ;
	return curpos ;
}

/* Looks to the left of curpos, and returns the position of the prev character (UTF8 aware) */
int lcd_getprevcharacterpos(char *str, int curpos) 
{
	if (curpos==0) curpos=strlen(str)-1 ;
	else curpos-- ;
	while (curpos>0 && (str[curpos]&'\xC0')=='\x80') curpos-- ;
	return curpos ;
}

/* scrolls along, and returns string */
char *lcd_textbuildscrollingline(struct slcd_s_row *row)
{
	int p ;
	static char linebuf[SLCD_TEXTBUF_MAXLEN] ;
	char *utf8char ;

/*FIXME: function assumes that SLCD_TEXTBUF_MAXLEN will always be larger than lcd_width() */
	
	if (row==NULL) return "" ;	/* paranoia check */

	/* short line - no scrolling required */
	if (row->len<=lcd_width()) {
		strcpy(linebuf, row->str) ;
		return linebuf ;
	}

	 /* Copy the string at the given offset */
	p=row->scrollpos ;
	strcpy(linebuf,"") ;
	while (lcd_strlen(linebuf)<lcd_width()) {
		utf8char=lcd_getutf8char(row->str, p) ;
		p=lcd_getnextcharacterpos(row->str, p) ;
		strcat(linebuf, utf8char) ;
	}

	row->scrollpos=lcd_getnextcharacterpos(row->str, row->scrollpos) ;

	return linebuf ;
}

/* returns input formatted string (doesn't scroll, that's someone elses job */
char *lcd_inputbuildscrollingline(char *selstr, int selection)
{
	int i, p ;
	static char linebuf[SLCD_TEXTBUF_MAXLEN] ;
	char *utf8char ;
	int preview ;
	
/*FIXME: function assumes that SLCD_TEXTBUF_MAXLEN will always be larger than lcd_width() */
	
	if (selstr==NULL) return "" ;	/* paranoia check */

	/* Clear the destination */
	strcpy(linebuf,"") ;
	
	/* Calculate how many preview characters there will be */
	preview=(lcd_width()-5)/2 ;

	/* Work back along the line to identify screen-left */
	for (p=selection, i=0; i<preview; i++) p=lcd_getprevcharacterpos(selstr, p) ;

	/* Copy the pre-centre string at the given offset */
	for (i=0; i<preview; i++) {
		utf8char=lcd_getutf8char(selstr, p) ;
		strcat(linebuf, lcd_parsespecial(utf8char)) ;
		p=lcd_getnextcharacterpos(selstr, p) ;
	}

	/* Now do the selected character */
	utf8char=lcd_getutf8char(selstr, p) ;
	if (strcmp(utf8char, SLCD_UCHR_END1)==0) {
		strcat(linebuf, " END ") ;
		p=lcd_getnextcharacterpos(selstr, p) ;
		p=lcd_getnextcharacterpos(selstr, p) ;
		p=lcd_getnextcharacterpos(selstr, p) ;
	} else if (strcmp(utf8char, SLCD_UCHR_DEL1)==0) {
		strcat(linebuf, " DEL ") ;
		p=lcd_getnextcharacterpos(selstr, p) ;
		p=lcd_getnextcharacterpos(selstr, p) ;
		p=lcd_getnextcharacterpos(selstr, p) ;
	} else {
		strcat(linebuf, " <") ;
		strcat(linebuf, utf8char) ;
		strcat(linebuf, "> ") ;
		p=lcd_getnextcharacterpos(selstr, p) ;
	}

	/* Now copy the post-centre string */
	for (i=0; i<preview; i++) {
		utf8char=lcd_getutf8char(selstr, p) ;
		strcat(linebuf, lcd_parsespecial(utf8char)) ;
		p=lcd_getnextcharacterpos(selstr, p) ;
	}

	/* pad with space if needed (most likely) */
	if (lcd_width() != (preview+5+preview)) {
		strcat(linebuf," ") ;
	}
	
	return linebuf ;
}

/* parse special UTF8 chars into English */
char *lcd_parsespecial(char *t)
{
	if (strcmp(t, SLCD_UCHR_END1)==0) return "E" ;
	if (strcmp(t, SLCD_UCHR_END2)==0) return "N" ;
	if (strcmp(t, SLCD_UCHR_END3)==0) return "D" ;
	if (strcmp(t, SLCD_UCHR_DEL1)==0) return "D" ;
	if (strcmp(t, SLCD_UCHR_DEL2)==0) return "E" ;
	if (strcmp(t, SLCD_UCHR_DEL3)==0) return "L" ;
	return t ;
}

/* returns an input edit line  */
char *lcd_inputbuildinputline(char *src, int bol, int cursor)
{
/*FIXME:  Need to pad line out with spaces if necessary */
	return &src[bol] ;
}

/* Extracts and returns a UTF8 Character from the given string position */
char * lcd_getutf8char(char *str, int pos)
{
	static char utf8[4] ;
	int x ;

	x=0 ;
	utf8[x++]=str[pos] ;
	if ((utf8[0]&'\xC0')=='\xC0') {
		while ((str[pos+x]&'\xC0')=='\x80' && x<3) {
			utf8[x]=str[pos+x] ;
			x++ ;
		}
	}
	utf8[x]='\0' ;
	return (char *)utf8 ;
}

  
/* Insert character at the given position */
/* Note this function does not handle editing of UTF8 strings */
/* Returns the new pos */
int lcd_insertchar(char *str, int pos, char ch, int maxlen)
{
	int i ;
	int newlen ;

	/* Calculate length of new row */
	newlen=strlen(str) ;
	if (ch=='\0') newlen-- ;
	else newlen++ ;

	/* Check that we have space to do insertion */
	if (newlen>=SLCD_TEXTBUF_MAXLEN || newlen>=maxlen) return pos ;

	if (ch=='\0') {
		/* Check we are not at the beginning of the line */
		if (pos==0) return pos ;

		/* Delete character by copying second half of string */
		for (i=pos-1; str[i]!='\0'; i++)
			str[i]=str[i+1] ;
		str[i]='\0' ;
		pos-- ;
	} else {
		/* Insert character and copy second half */
		str[strlen(str)+1]='\0' ;
		for (i=strlen(str); i>pos; i--)
			str[i]=str[i-1] ;
		str[pos]=ch ;
		pos++ ;
	}
return pos ;

}

void lcd_dumpscreen(char *buf, int maxlen)
{
	int r, c ;			/* row and column */
	char **str, *u8 ;		/* line source and utf8 character */
	int cc ;			/* character count */
	enum slcd_e_arrows *selr ;	/* select status for each row */

/* FIXME: Need to do this properly, and centre each line as is done on the radio */

	str=lcd_hwgetscreen() ;
	selr=lcd_hwgetselrows() ;
	
	buf[0]='\0' ;
	for (r=0; r<lcd_height() && strlen(buf)<maxlen-4; r++) {
	
		/* Add < Arrow */
		if (selr[r]==SLCD_SEL_ARROWS) strcat(buf,"<") ;
		else strcat(buf," ") ;
		
		/* Copy Line */
		for (c=0, cc=0; !(cc==0 && c>0) && c<lcd_width() && strlen(buf)<maxlen-3; c++) {
			u8=lcd_getutf8char(str[r], cc) ;
			cc=lcd_getnextcharacterpos(str[r], cc) ;
			strcat(buf, lcd_parsespecial(u8)) ;
		}
		
		/* Pad with spaces */
		for (; c<lcd_width() && strlen(buf)<maxlen-2; c++)
			strcat(buf," ") ;
			
		/* Add > Arrow */
		if (selr[r]==SLCD_SEL_ARROWS) strcat(buf,">\n") ;
		else strcat(buf," \n") ;
	}
}

/* Dumps the entire screen structure to stdout */
void lcd_dumpvars(lcd_handle *handle, char *comment, int n) {
	struct slcd_s_row *row ;
	int i ;
	
	if (handle==NULL) return  ;
	
	printf("DEBUG DUMP LCD VARS - ") ;
	printf(comment, n) ;
	printf("\n") ;
	
	if (handle->top==NULL) {
		printf (" - EMPTY\n") ;
		return ;
	}
	
	for (i=0, row=handle->top; i<handle->numrows; row=row->next, i++) {
		printf(" %02d: id=%02d, len=%02d, pos=%02d",
			i, row->idnumber, row->len, row->scrollpos) ;
		if (row==handle->curl) printf(", Selected") ;
		if (row==handle->tsc) printf(", Top Line") ;
		printf(", str=%s\n", row->str) ;
	}
}
