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
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "mute.h"
#include "log.h"
#include "reciva_audio_mute.h"

static int smute_hwdoioctl(int iot, void *arg) ;
static enum smute_e_state stored_mute_state ;

/*
 * ident
 */
char *mute_ident() {
        return "$Id: mute_reciva.c 230 2007-10-23 17:42:49Z  $" ;
}

/**
 * mute_set
 * @state: Mute On/Off State
 *
 * This function sets the mute state of the radio
 *
 **/
void mute_set(enum smute_e_state state) {
	stored_mute_state=state ;
	smute_hwdoioctl(IOC_AUDIO_MUTE, &state) ;
}

/**
 * mute_get
 *
 * This function gets the mute state of the radio
 *
 **/
enum smute_e_state mute_get() {
	return stored_mute_state ;
}

/**
 * smute_hwdoioctl
 * @iot: IOCTL
 * @arg: argument
 *
 * this function calls and ioctl, and returns true on success
 * or false on failure.
 **/
int smute_hwdoioctl(int iot, void *arg) {
        int fd;
        int r;

        fd = open("/dev/misc/audio_mute", O_RDWR);
        if(fd == -1) {
                logf(LG_FTL, "IOCTL, unable to open /dev/misc/audio_mute") ;
                return (1==0) ;
        }

	r = ioctl(fd, iot, arg);

        if (r==(-1)) {
                logf(LG_INF, "smute_doioctl failed: dir=%d, type=%d, nr=%d, size=%d, err=%s\n",
			_IOC_DIR(iot), _IOC_TYPE(iot), _IOC_NR(iot), _IOC_SIZE(iot), strerror(errno)) ;
	}

	close(fd) ;
	return (r!=(-1)) ;
}
