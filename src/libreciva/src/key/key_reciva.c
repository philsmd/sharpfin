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

#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/select.h>
#include <linux/input.h>
#include "../../include/key.h"
#define PATH_EVDEV "/dev/input/event%d"

static int translate_key(struct input_event *ie, struct key *ev);

/*
 * ident
 */
char *key_ident() {
        return "$Id: key_reciva.c 230 2007-10-23 17:42:49Z  $" ;
}

struct key_handler *key_init(void) {
	struct key_handler *eh;
	int i;
	char fname[64];

	eh = calloc(sizeof *eh, 1);

	/* Try to open all the HID key devices. All devices that are opened
	 * successfully have a proper file descriptor, devices that don't exist
	 * will get '-1' because of the failing open() call */

	for(i=0; i<EVENT_FD_COUNT; i++) {
		snprintf(fname, sizeof(fname), PATH_EVDEV, i);
		eh->fd[i] = open(fname, O_RDONLY);
	}

	return eh;
}

int key_poll(struct key_handler *eh, struct key *ev) {
	fd_set fds;
	int i;
	int fd_max = 0;
	struct timeval tv;
	int r;
	struct input_event ie;

	/*
	 * Add all key file descriptors to the select's fd_set 
	 */
	FD_ZERO(&fds);
	for(i=0; i<EVENT_FD_COUNT; i++) {
		if(eh->fd[i] != -1) {
			FD_SET(eh->fd[i], &fds);
			if(eh->fd[i] > fd_max) fd_max = eh->fd[i];
		}
	}
	/* Wait 0 seconds: return right away */
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	/* Check if any data is available on the file descriptors */
	r = select(fd_max+1, &fds, NULL, NULL, &tv);
	/* Error or no data: return right away */
	if(r == -1) return -1;	/* Select returned error */
	if(r == 0) return 0;	/* Timeout, no data waiting */

	/* One or more keys are waiting: read from key device and translate to
	 * key struct */
	for(i=0; i<EVENT_FD_COUNT; i++) {
		if(eh->fd[i] != -1) {
			if(FD_ISSET(eh->fd[i], &fds)) {
				r = read(eh->fd[i], &ie, sizeof(ie));
				if(r < sizeof(ie)) return -1;
				return translate_key(&ie, ev);

			}
		}
	}
	return 0;
}

/*
 * Translate a linux HID input key to a reciva-key
 */
#define IE_TYPE_BUTTON 1
#define IE_TYPE_WHEEL 2
#define IE_CODE_SHIFT 309
#define IE_CODE_BACK 265
#define IE_CODE_SELECT 263
#define IE_CODE_REPLY 264
#define IE_CODE_POWER 256
#define IE_CODE_1 257
#define IE_CODE_2 258
#define IE_CODE_3 259
#define IE_CODE_4 260
#define IE_CODE_5 261

static int translate_key(struct input_event *ie, struct key *key) {
	switch(ie->type) {
		case IE_TYPE_BUTTON:
			switch(ie->code) {
				case IE_CODE_SHIFT: key->id = KEY_ID_SHIFT; break;
				case IE_CODE_BACK: key->id = KEY_ID_BACK; break;
				case IE_CODE_SELECT: key->id = KEY_ID_SELECT; break;
				case IE_CODE_REPLY: key->id = KEY_ID_REPLY; break;
				case IE_CODE_POWER: key->id = KEY_ID_POWER; break;
				case IE_CODE_1: key->id = KEY_ID_1; break;
				case IE_CODE_2: key->id = KEY_ID_2; break;
				case IE_CODE_3: key->id = KEY_ID_3; break;
				case IE_CODE_4: key->id = KEY_ID_4; break;
				case IE_CODE_5: key->id = KEY_ID_5; break;
				default: return 0; break;
			}
			key->state = ie->value;
			return 1;
		case IE_TYPE_WHEEL:

			if(ie->value == -1) {
				key->id = KEY_ID_LEFT;
			} else {
				key->id = KEY_ID_RIGHT;
			}
			key->state = KEY_STATE_PRESSED;
			return 1;
		default:
			return 0;
	}
	return 0;
}

