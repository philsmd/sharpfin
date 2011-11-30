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

#include "dog.h"
static int wd_enabled=0;

/*
 * ident
 */
char *dog_ident() {
	return "$Id: dog_devel.c 230 2007-10-23 17:42:49Z  $" ;
}

/*
 * Open watchdog device
 */
int dog_init() {
	return (1==1) ;
}


/*
 * Close 
 */
void dog_exit() {}


/*
 * Enable / Disable Watchdog
 */
int dog_enable() {
	wd_enabled=1 ;
	return 0 ;
}

/*
 * Disable watchdog
 */
int dog_disable() {
	wd_enabled=0 ;
	return 0 ;
}

/*
 * Returns whether dog is enabled or not (true / false)
 */
int dog_isenabled() {
	return (wd_enabled==1) ;
}

/*
 * Timer callback function to trigger the watchdog
 */
int dog_kick() {
	return 0 ;
}
