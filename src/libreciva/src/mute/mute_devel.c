/* 
 * Sharpfin project
 * Copyright (C) by Steve Clarke and Ico Doornekamp
 * 2011-11-30 Philipp Schmidt
 *   Added to github 
 *
 * This file is part of the sharpfin project
 * Mute Control
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
 * Mute Control
 */
#include "mute.h"
static enum smute_e_state saved_state=SMUTE_ON ;

/*
 * ident
 */
char *mute_ident() {
        return "$Id: mute_devel.c 230 2007-10-23 17:42:49Z  $" ;
}

/**
 * mute_set
 * @state: Mute On/Off State
 *
 * This function sets the mute state of the radio
 *
 **/
void mute_set(enum smute_e_state state) {
	saved_state=state ;
}

/**
 * mute_get
 *
 * This function gets the mute state of the radio
 *
 **/
enum smute_e_state mute_get() {
	return saved_state ;
}
