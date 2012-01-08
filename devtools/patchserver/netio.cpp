/* 
 * Sharpfin project
 * Copyright (C) by Steve Clarke and 
 *   Ico Doornekamp
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

// Socket i/o functions

#ifdef WINDOWS
#include <winsock.h>
#else
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

// vsnprintf()
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "commandline.h"
 
void net_putc(int fd, unsigned char c)
{
	write(fd, &c, 1) ;
}

void net_printf(int fd, const char *fmt, ...) 
{
	char buf[1024] ;
	va_list va ;
	int r ;
	
	va_start(va, fmt) ;
	vsnprintf(buf, 1023, fmt, va) ;
	va_end(va) ;
	write(fd, buf, strlen(buf)) ;
}

int net_getc(int fd)
{
	unsigned char c[2] ;
	int r ;
	r=read(fd, c, 1) ;
	if (r==0) return (-1) ;
	else return (c[0]) ;
}

int net_scanf(int fd, const char *fmt, ...) 
{
	char buf[1024] ;   
	va_list va ;
	int r ;
	int d, c ;

	/* fill buffer */
	d=0 ;
	do {
		c=net_getc(fd) ;
		if (c<0) return 0 ;
		if (c!='\r' && c!='\n') buf[d++]=c ;
		else if (c=='\n' || d==1023) buf[d++]='\0' ;
	} while (buf[d-1]!='\0' && d<1024) ;

	/* parse buffer */
	if (d==0) return 0 ;
	va_start(va, fmt) ;
	r=vsscanf(buf, fmt, va) ;
	va_end(va) ;
	
	/* return count */
	return r ;
}
