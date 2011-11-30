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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "lcd.h"

int main(int argc, char **argv) {
	lcd_handle *h ;
	int r;
	lcd_init() ;
	lcd_brightness(100) ;
	//lcd_contrast(50) ;
	h=lcd_framecreate() ;
	if (h==NULL) return 1 ;
	if (argc>1 && strcmp(argv[1],"-v")==0) {
		char revb[32] ;
		printf("LCD Print Version Details: $Id: lcdprint.c 192 2007-10-01 10:16:58Z  $\n") ;
		lcd_framesetline(h, 0, "SVN Revision:") ;
		strcpy(revb,"$Revision: 192 $") ;
		revb[strlen(revb)-2]='\0' ;
		lcd_framesetline(h, 1, &revb[11]) ;
	} else {
		for (r=1; r<argc; r++) {
			lcd_framesetline(h, r-1, argv[r]) ;
		}
	}
	lcd_refresh(h) ;
	lcd_exit() ;
	return 0;
}


