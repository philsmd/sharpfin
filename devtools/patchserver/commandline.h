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

#ifndef _commandline_defined_
#define _commandline_defined_

#define CONFIG_WEBSERVER_PORT 80
#define CONFIG_DNSSERVER_PORT 53

#define STDIN (int)0
#define STDOUT (int)1
#define STDERR (int)2

int dnsserver_openlistener() ;
struct sockaddr_in *dnsserver_createrelay(char *nameserver) ;
int dnsserver_command(int dnslistener, struct sockaddr_in *dnsserver_address) ;
int dnsserver_closelistener(int dnslistener) ;

#define FAKESERVER "www.sharpfin.fakeserver.com"
#define FAKETARFILE "reciva-upgrade.tar.bz2"
int webserver_openlistener(char *tarfile) ;
int webserver_command(int weblistener) ;
int webserver_closelistener(int weblistener) ;

void net_putc(int fd, unsigned char c) ;
void net_printf(int fd, const char *fmt, ...) ;
int net_getc(int fd) ;
int net_scanf(int fd, const char *fmt, ...) ;

#endif
