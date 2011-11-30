/* 
 * Sharpfin project
 * Copyright (C) 2007 by Steve Clarke and Ico Doornekamp
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

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/watchdog.h>
#include "dog.h"
#include "log.h"

static int wd_fd=-1 ;
static int wd_enabled=0 ;

/*
 * ident
 */

char *dog_ident() {
        return "$Id: dog_reciva.c 230 2007-10-23 17:42:49Z  $" ;
}

/*
 * Open watchdog device
 */
int dog_init() {
	int fd;
	fd = open(DEV_WATCHDOG, O_WRONLY);
	if(fd == -1) {
		logf(LG_FTL, "Can't open watchdog device %s: %s\n", DEV_WATCHDOG, strerror(errno));
		return (1==0) ;
	}
	wd_fd = fd;
	return (1==1) ;
}

/*
 * Close 
 */

void dog_exit() {
	if (wd_fd != -1) close(wd_fd);
}

/*
 * Enable / Disable Watchdog
 */

int dog_enable() {
	int r;
	if (wd_fd==(-1)) {
		logf(LG_ERR, "Unable to enable the dog, bad handle") ;
		return -1 ;
	} else {
		wd_enabled=1 ;
		r = ioctl(wd_fd, WDIOS_ENABLECARD);
		if(r != 0) {
			logf(LG_WRN, "Error enabling watchdog: %s\n", strerror(errno));
			return -1;
		}
		return 0;
	}
}

/*
 * Disable watchdog
 */
int dog_disable() {
	int r;
	if (wd_fd==(-1)) {
		logf(LG_ERR, "Unable to disable the dog, bad handle") ;
		return -1 ;
	} else {
		wd_enabled=0 ;
		r = ioctl(wd_fd, WDIOS_DISABLECARD);
		if(r != 0) {
			logf(LG_WRN, "Error disabling watchdog: %s\n", strerror(errno));
			return -1;
		}
		return 0;
	}
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
	char kick = '1';
	if (wd_fd==(-1)) {
		logf(LG_ERR, "Enable to kick the dog, bad handle") ;
		return -1 ;
	} else {
		write(wd_fd, &kick, sizeof(kick));
		return 0 ;
	}
}
