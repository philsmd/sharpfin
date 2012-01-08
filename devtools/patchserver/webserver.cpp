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

// The webserver

#include "commandline.h"
#ifdef WINDOWS
#include <winsock.h>
#define socklen_t int
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
char _webserver_tarfile[1024] ;
char _webserver_tarfile_isurl ;

int webserver_openlistener(char *tarfile) {
	int sockfd, err;
	struct sockaddr_in serv_addr;

	strncpy(_webserver_tarfile, tarfile, 1023) ;
	_webserver_tarfile_isurl = (strncmp(tarfile, "http://",7)==0) || (strncmp(tarfile, "reciva://", 9)==0) || 
					(strncmp(tarfile, "ftp://", 6)==0) || (strncmp(tarfile, "https://", 8)==0) ;
	

	if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){

		fprintf(stderr, "webserver: unable to create TCP server - \n", strerror(errno)) ; 

	} else {

		memset((char *) &serv_addr, 0,  sizeof(serv_addr));
		serv_addr.sin_family      = AF_INET;
		serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		serv_addr.sin_port        = htons(CONFIG_WEBSERVER_PORT);

		if ((err = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))) < 0) {

			fprintf(stderr, "webserver: unable to bind socket - %s\n", strerror(errno)) ;
			close(sockfd) ;
			sockfd=-1 ;

		} else {

			listen(sockfd, 1);
			printf("webserver: waiting for connections ...\n") ;

		}
		
	}
	
	return sockfd ;
}

int webserver_command(int weblistener) {
	int sockfd, fd, clilen, err, pid;
	const char * opt="1";
	struct sockaddr_in cli_addr, serv_addr;
	clilen = sizeof(cli_addr);
	fd = accept(weblistener, (struct sockaddr *) &cli_addr, (socklen_t *) &clilen);
	if (fd<0) {
		fprintf(stderr, "webserver: %s\n", strerror(errno)) ;
	} else {
	
		/* I have no idea which sockopts to set.  This is from busybox httpd */
		setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, opt, sizeof(opt));
		setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, opt, sizeof(opt));

		printf("webserver: %s: ", inet_ntoa(cli_addr.sin_addr)) ; fflush(stdout) ;

		/* handle connection  */
		char cmd[1024], arg[1024] ;
		if (net_scanf(fd, "%s %s", cmd, arg)==2) {
			if (strcasecmp(cmd, "get")==0) {

				if (strncasecmp(arg,"/cgi-local", 10)==0) {

					printf("fetching info: ... ") ;
					fflush(stdout) ;

					net_printf(fd, "HTTP/1.0 200 OK\r\nContent-type: text/plain\r\n\r\n") ;
					if (_webserver_tarfile_isurl)
						net_printf(fd, "%s%c%c", _webserver_tarfile, 0x0a, 0x0a) ;
					else
						net_printf(fd, "http://%s/%s%c%c", FAKESERVER, FAKETARFILE, 0x0a, 0x0a) ;

					printf("OK") ;
					
				} else if (_webserver_tarfile_isurl) {
					// Request has come to us.  There is probably a DNS error
					printf("error - radio has come back for update patch rather than going to %s\n", _webserver_tarfile) ;
				
				} else {
					// Return the contents of the identified file
					FILE *fp ;
					int c, j, len ;
					fp=fopen(_webserver_tarfile,"rb") ;
					if (fp==NULL) {
						printf("unable to open %s", _webserver_tarfile) ;
					} else {
						unsigned char buffer[1024] ;
						int fs ;

						printf("transferring patchfile ... ") ;
						fflush(stdout) ;

						for (len=0; fgetc(fp)!=EOF; len++) ;
						fclose(fp) ;
														
						net_printf(fd,
							"HTTP/1.0 200 OK\r\n"
							"Content-Type: binary/octet-stream\r\n"
							"Content-Length: %d\r\n"
							"Content-Disposition: attachment; filename=%s; size=%d\r\n"
							"\r\n", len, FAKETARFILE, len) ;

						fs=open(_webserver_tarfile,O_RDONLY) ;
						while( (j=read(fs,buffer,1024)) > 0)
							write(fd,buffer,j);
						close(fs) ;
							
						/* wait for buffers to empty */
						// FIXME - shouldn't need a sleep
						sleep(2);
						printf("OK") ;
					}
				}
			}
		}
#ifdef WINDOWS
		closesocket(fd) ;
#else
		close(fd) ;
#endif
		printf("\n") ;
	}
}

/*
 * webserver_closelistener
 *
 * shuts down web server socket
 */
int webserver_closelistener(int weblistener) {
	if (weblistener>=0) 
#ifdef WINDOWS
		closesocket(weblistener) ;
#else
		close(weblistener) ;
#endif
	return 0 ;
}
