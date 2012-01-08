/*      $Id: hw_uirt2.c,v 5.6 2010/04/11 18:50:38 lirc Exp $      */

/****************************************************************************
 ** hw_uirt2.c **************************************************************
 ****************************************************************************
 *
 * routines for UIRT2 receiver using the UIR mode
 * 
 * Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
 * 	modified for logitech receiver by Isaac Lauer <inl101@alumni.psu.edu>
 *      modified for UIRT2 receiver by 
 *      Mikael Magnusson <mikma@users.sourceforge.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifndef LIRC_IRTTY
#define LIRC_IRTTY "/dev/ttyS0"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include "hardware.h"
#include "serial.h"
#include "ir_remote.h"
#include "lircd.h"

#define NUMBYTES 6
#define TIMEOUT 20000

static unsigned char b[NUMBYTES];
static struct timeval start, end, last;
static ir_code code;

static int uirt2_decode(struct ir_remote *remote, ir_code * prep, ir_code * codep, ir_code * postp, int *repeat_flagp,
			lirc_t * min_remaining_gapp, lirc_t * max_remaining_gapp);
static int uirt2_init(void);
static int uirt2_deinit(void);
static char *uirt2_rec(struct ir_remote *remotes);

struct hardware hw_uirt2 = {
	LIRC_IRTTY,		/* default device */
	-1,			/* fd */
	LIRC_CAN_REC_LIRCCODE,	/* features */
	0,			/* send_mode */
	LIRC_MODE_LIRCCODE,	/* rec_mode */
	8 * NUMBYTES,		/* code_length */
	uirt2_init,		/* init_func */
	uirt2_deinit,		/* deinit_func */
	NULL,			/* send_func */
	uirt2_rec,		/* rec_func */
	uirt2_decode,		/* decode_func */
	NULL,			/* ioctl_func */
	NULL,			/* readdata */
	"uirt2"
};

static int uirt2_decode(struct ir_remote *remote, ir_code * prep, ir_code * codep, ir_code * postp, int *repeat_flagp,
			lirc_t * min_remaining_gapp, lirc_t * max_remaining_gapp)
{
	if (!map_code(remote, prep, codep, postp, 0, 0, 8 * NUMBYTES, code, 0, 0)) {
		return (0);
	}

	map_gap(remote, &start, &last, 0, repeat_flagp, min_remaining_gapp, max_remaining_gapp);

	return (1);
}

static int uirt2_init(void)
{
	if (!tty_create_lock(hw.device)) {
		logprintf(LOG_ERR, "uirt2: could not create lock files");
		return (0);
	}
	if ((hw.fd = open(hw.device, O_RDWR | O_NONBLOCK | O_NOCTTY)) < 0) {
		logprintf(LOG_ERR, "uirt2: could not open %s", hw.device);
		logperror(LOG_ERR, "uirt2: ");
		tty_delete_lock();
		return (0);
	}
	if (!tty_reset(hw.fd)) {
		logprintf(LOG_ERR, "uirt2: could not reset tty");
		uirt2_deinit();
		return (0);
	}
	if (!tty_setbaud(hw.fd, 115200)) {
		logprintf(LOG_ERR, "uirt2: could not set baud rate");
		uirt2_deinit();
		return (0);
	}
	if (!tty_setcsize(hw.fd, 8)) {
		logprintf(LOG_ERR, "uirt2: could not set csize");
		uirt2_deinit();
		return (0);
	}
	if (!tty_setrtscts(hw.fd, 1)) {
		logprintf(LOG_ERR, "uirt2: could not enable hardware flow");
		uirt2_deinit();
		return (0);
	}
	return (1);
}

static int uirt2_deinit(void)
{
	close(hw.fd);
	tty_delete_lock();
	return (1);
}

static char *uirt2_rec(struct ir_remote *remotes)
{
	char *m;
	int i;

	last = end;
	gettimeofday(&start, NULL);
	for (i = 0; i < NUMBYTES; i++) {
		if (i > 0) {
			if (!waitfordata(TIMEOUT)) {
				logprintf(LOG_ERR, "uirt2: timeout reading byte %d", i);
				return (NULL);
			}
		}
		if (read(hw.fd, &b[i], 1) != 1) {
			logprintf(LOG_ERR, "uirt2: reading of byte %d failed", i);
			logperror(LOG_ERR, NULL);
			return (NULL);
		}
		LOGPRINTF(1, "byte %d: %02x", i, b[i]);
	}
	gettimeofday(&end, NULL);

	/* mark as Irman */
	code = 0xffff;
	code <<= 16;

	code = ((ir_code) b[0]);
	code = code << 8;
	code |= ((ir_code) b[1]);
	code = code << 8;
	code |= ((ir_code) b[2]);
	code = code << 8;
	code |= ((ir_code) b[3]);
	code = code << 8;
	code |= ((ir_code) b[4]);
	code = code << 8;
	code |= ((ir_code) b[5]);

	LOGPRINTF(1, "code: %llx", (__u64) code);

	m = decode_all(remotes);
	return (m);
}
