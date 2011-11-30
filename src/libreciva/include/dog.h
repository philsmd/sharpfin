/* 
 * Sharpfin project
 * Copyright (C) by 2007 Steve Clarke and Ico Doornekamp
 * 2011-11-30 Philipp Schmidt
 *   Added to github 
 *
 * This file is part of the sharpfin project
 * The watchdog hardware library
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
 * This is the watchdog hardware library
 */
#ifndef _dog_defined_h
#define _dog_defined_h
#define DEV_WATCHDOG "/dev/misc/S3C2410 watchdog"

// Open watchdog device
int dog_init() ;

// Close 
void dog_exit() ;

// Enable / Disable Watchdog
int dog_enable() ;
int dog_disable() ;

// Returns whether dog is enabled or not (true / false)
int dog_isenabled() ;

// Timer callback function to trigger the watchdog
int dog_kick() ;
#endif
