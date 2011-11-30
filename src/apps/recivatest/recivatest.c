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
#include "lcd.h"
#include "key.h"
int main(int argc, char **argv) {
	struct key_handler *eh;
	struct key ev;
	lcd_handle *h ;
	int r;
	lcd_init() ;
	eh = key_init();
	h=lcd_framecreate() ;
	while(1) {
		r = key_poll(eh, &ev);
		if(r == 1) {
			lcd_frameprintf(h, 0, "Key: %d", ev.id) ;
			lcd_frameprintf(h, 1, "State: %d", ev.state) ;
			lcd_refresh(h) ;
			fprintf(stderr,"key %d state %d\n", ev.id, ev.state);
		}
		usleep(10000);
	}

	lcd_exit() ;
	return 0;
}
