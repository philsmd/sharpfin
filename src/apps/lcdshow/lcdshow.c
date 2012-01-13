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
#include "reciva_lcd.h"

int main(int argc,char**argv) {
	lcd_handle *h ;
	lcd_init();
	lcd_brightness(100) ;
	h=lcd_framecreate() ;
	lcd_framesetline(h,1,"TEST") ;
	lcd_refresh(h) ;
	int top=0,left=0,width=-1,height=-1;
	if (argc>1) {
		top=atoi(argv[1]);
		if (argc>2) {
			left=atoi(argv[2]);
		}
		if (argc>3) {
			width=atoi(argv[3]);
		}
		if (argc>4) {
			height=atoi(argv[4]);
		}
	}
	struct bitmap_data*bitmap=lcd_grab_region(top,left,width,height);
	if (bitmap!=NULL) {
		printf("top: %i, left: %i, width: %i, height: %i\n",bitmap->top,bitmap->left,bitmap->width,bitmap->height);
	 	printf("data %p\n",bitmap->data);
	} else {
		perror("Bitmap data was NULL");
		lcd_exit() ;
		exit(1);
	} 
	lcd_exit() ;
	return 0;
}
