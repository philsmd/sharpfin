/****************************************************************************
 ** hw_mp3anywhere.c ********************************************************
 ****************************************************************************
 *
 * routines for X10 MP3 Anywhere receiver 
 * 
 * Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
 * 	modified for logitech receiver by Isaac Lauer <inl101@alumni.psu.edu>
 *	modified for X10 receiver by Shawn Nycz <dscordia@eden.rutgers.edu>
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
#include "hw_mp3anywhere.h"

#define NUMBYTES 16
#define TIMEOUT 50000

extern struct ir_remote *repeat_remote, *last_remote;

static unsigned char b[NUMBYTES];
static struct timeval start, end, last;
static lirc_t signal_length;
static ir_code pre, code;

struct hardware hw_mp3anywhere = {
	LIRC_IRTTY,		/* default device */
	-1,			/* fd */
	LIRC_CAN_REC_LIRCCODE,	/* features */
	0,			/* send_mode */
	LIRC_MODE_LIRCCODE,	/* rec_mode */
	8,			/* code_length */
	mp3anywhere_init,	/* init_func */
	mp3anywhere_deinit,	/* deinit_func */
	NULL,			/* send_func */
	mp3anywhere_rec,	/* rec_func */
	mp3anywhere_decode,	/* decode_func */
	NULL,			/* ioctl_func */
	NULL,			/* readdata */
	"mp3anywhere"
};

int mp3anywhere_decode(struct ir_remote *remote, ir_code * prep, ir_code * codep, ir_code * postp, int *repeat_flagp,
		       lirc_t * min_remaining_gapp, lirc_t * max_remaining_gapp)
{
	if (!map_code(remote, prep, codep, postp, 24, pre, 8, code, 0, 0)) {
		return (0);
	}

	map_gap(remote, &start, &last, signal_length, repeat_flagp, min_remaining_gapp, max_remaining_gapp);

	return (1);
}

int mp3anywhere_init(void)
{
	signal_length = hw.code_length * 1000000 / 9600;

	if (!tty_create_lock(hw.device)) {
		logprintf(LOG_ERR, "could not create lock files");
		return (0);
	}
	if ((hw.fd = open(hw.device, O_RDWR | O_NONBLOCK | O_NOCTTY)) < 0) {
		logprintf(LOG_ERR, "could not open %s", hw.device);
		logperror(LOG_ERR, "mp3anywhere_init()");
		tty_delete_lock();
		return (0);
	}
	if (!tty_reset(hw.fd)) {
		logprintf(LOG_ERR, "could not reset tty");
		mp3anywhere_deinit();
		return (0);
	}
	if (!tty_setbaud(hw.fd, 9600)) {
		logprintf(LOG_ERR, "could not set baud rate");
		mp3anywhere_deinit();
		return (0);
	}
	return (1);
}

int mp3anywhere_deinit(void)
{
	close(hw.fd);
	tty_delete_lock();
	return (1);
}

char *mp3anywhere_rec(struct ir_remote *remotes)
{
	char *m;
	int i = 0;
	b[0] = 0x00;
	b[1] = 0xd5;
	b[2] = 0xaa;
	b[3] = 0xee;
	b[5] = 0xad;

	last = end;
	gettimeofday(&start, NULL);
	while (b[i] != 0xAD) {
		i++;
		if (i >= NUMBYTES) {
			LOGPRINTF(0, "buffer overflow at byte %d", i);
			break;
		}
		if (i > 0) {
			if (!waitfordata(TIMEOUT)) {
				LOGPRINTF(0, "timeout reading byte %d", i);
				return (NULL);
			}
		}
		if (read(hw.fd, &b[i], 1) != 1) {
			logprintf(LOG_ERR, "reading of byte %d failed", i);
			logperror(LOG_ERR, NULL);
			return (NULL);
		}
		if (b[1] != 0xd5 || b[2] != 0xaa || b[3] != 0xee || b[5] != 0xad) {
			logprintf(LOG_ERR, "bad envelope");
			return (NULL);
		}
		LOGPRINTF(1, "byte %d: %02x", i, b[i]);
	}
	gettimeofday(&end, NULL);

	pre = 0xD5AAEE;
	code = (ir_code) b[4];

	m = decode_all(remotes);
	return (m);
}
