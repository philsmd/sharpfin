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

/*
 * This file needs re-structuring, to output a copy of the
 * log messages to the ncurses screen identified by scr_get_logw()
 *
 **/
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <syslog.h>
#include <stdlib.h>
#include <regex.h>
#include "scr_devel.h"
#include "log.h"

static char *levelstr[16] = { "ftl", "err", "wrn", "inf", "dbg" };
static char *ansistr[16] = { 	
	"\x1b[7m",
	"\x1b[4m",
	"\x1b[1m",
	"\x1b[22m",
	"\x1b[22m"
};

static int loglevel = LG_WRN;
static int log_to = 0;
static int log_ansicolors = 0;
static char *progname;
static WINDOW *logw ;

/*
 * ident
 */
char *log_ident() {
        return "$Id: log_devel.c 230 2007-10-23 17:42:49Z  $" ;
}

void log_init(char *name, enum log_to to, enum log_level level, int color) {
	scr_init_log() ;
	logw=scr_get_logw() ;
	wprintw(logw," Welcome to the Sharpfin Radio Simulator ...\n") ;
	wrefresh(logw) ;

	progname = name;
	log_to = to;
	loglevel = level;
	log_ansicolors = color;

	if(log_to & LOG_TO_SYSLOG) {
		openlog(progname, 0, LOG_DAEMON);
	}
	
}

void log_entry(enum log_level level, const char *func, char *fmt, ...) {
	va_list va;
	char buf[512] = "";
	int l = 0;
	char *p=NULL;

	/*
	 * Dont show loglines that are lower then logevel
	 */
	
	if(level > loglevel) return;
	l += snprintf(buf+l, sizeof(buf)-l, "[%s] %-10.10s ", levelstr[level], func);
	va_start(va, fmt);
	l += vsnprintf(buf+l, sizeof(buf)-l, fmt, va);
	va_end(va);

	while( (p = strchr(buf, '\n')) ) *p=' ';
	while( (p = strchr(buf, '\r')) ) *p=' ';

	if(log_to & LOG_TO_SYSLOG) {
		syslog(LOG_DAEMON | LOG_INFO, "%s", buf);
	}
	if((log_to & LOG_TO_STDERR) || (level == LG_FTL)) {
		if(log_ansicolors) fprintf(stderr, "%s", ansistr[level]);
		fprintf(stderr, "%s\n", buf);
		if(log_ansicolors) fprintf(stderr, "\x1b[0m");
	}

	if (logw!=NULL) {
		wprintw(logw, buf) ;
		wprintw(logw, "\n") ;
		wrefresh(logw) ;
	}
	if(level == LG_FTL) exit(1);
}

void log_hexdump(char *txt, unsigned char *data, int len) {
	int i,j;
	if(len == 0) return;
	fprintf(stderr, "+-------| %s len=%d |-------\n", txt, len);
	for(i=0; i<len; i+=16) {
		fprintf(stderr, "| %04X  ", i);
		for(j=i; (j<i+16);  j++) {
			if(j<len) {
				fprintf(stderr, "%02X ", (unsigned char)*(data+j));
			} else {
				fprintf(stderr, "   ");
			}
		}
		fprintf(stderr, "  ");
		for(j=i; (j<i+16) && (j<len);  j++) {
			if((*(data+j) >= 32) && (*(data+j)<=127)) {
				fprintf(stderr, "%c", *(data+j));
			} else {
				fprintf(stderr, ".");
			}
		}
		fprintf(stderr, "\n");
	}
}
