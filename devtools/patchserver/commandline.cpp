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
 * INSTRUCTIONS:
 * Patchserver is a single application that can be run under Linux or Cygwin,
 * and provides both DNS and HTTP server applications to support reciva radio
 * patching.
 *
 * Usage:
 *   patchserver [-accept] [ dnsserveripaddress [ patchfile  / url] ]
 * 
 * The DNS server is very limited, and returns the local machine's ip address
 * in response to all lookup queries.
 *
 * The HTTP server is also very limited, and either:
 *
 *  returns a fixed reference file in response to the initial reciva query, 
 *  and then whatever file is provided on the command line in response to 
 *  the upgrade fetch query.
 * or
 *  if the supplied patchfile actually looks like a URL, returns the URL
 *  in response to the initial query.  Following this, the client will go to
 *  that URL ro get the actual file contents.
 *
 *
 * The upgrade process goes as follows:
 * Radio connects to "http://www.reciva.com/" port 80, and gets the file
 *  http://www.reciva.com/cgi-local/service-pack.pl?serial=xxx.....
 * Server responds with a plaintext file containing the following
 *  http://www.reciva.com/xxxxxxx.tar.bz2
 *
 * Where xxxxxx.tar is the file specified on the command line
 * The radio then re-connects and downloads the file.
 * The file is structured as follows:
 *
 * xxxxxxx.tar.bz2
 *  + upgrade
 *     - install-me         (executable script -> chmod 555 install-me)
 *     - install-file.ipk   (install pkg file) 
 *
 * The radio unpacks the file in /tmp, looks for an install-me file in a
 * subdirectory, cds into the subdirectory and executes ./install-me as
 * root.  Note that the / partitions is writeable when this occurs.
 *
 * The radio then re-boots, and the upgrade is complete.
 *
 **/
 
#include "commandline.h"
#include "terms.h"

#ifdef WINDOWS
#include <winsock.h>
#endif

#include <sys/types.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>

int mainloop(char *nameserver, char *tarfile) ;

int main(int argc, char *argv[]) {
	int dnsserver_ps, webserver_ps, i ;
	char reply[1024], nameserver[1024] ;
	char tarfile[1024] ;
	int sa ;
	
#ifdef WINDOWS
	WSADATA wda ;
	if (WSAStartup(MAKEWORD(2,2), &wda)!=0) {
		MessageBox(0, "Unable to start Networking", "Winsock Error", MB_OK) ;
		exit(1) ;
	}
#endif
	
	// first possible option is -accept override
	sa=1 ;
	if (argc>sa && strcmp(argv[sa], "-accept")==0) {
		sa++ ;
		
	} else {
	
		printf("\n\n\n\n\n\n\n\n\n"
			"                  ***********************************\n"
			"                  *   SHARPFIN RADIO PATCH SERVER   *\n"
			"                  ***********************************\n"
			"\n"
			TERMS
			" [yN] "
			"\n") ;
			
		fgets(reply, 32, stdin) ;
		if (reply[0]!='y' && reply[0]!='Y') {
			printf("OK, Goodbye\n") ;
			return 1 ;
		}
		fflush(stdin) ;
	}
	
	/* Welcome */
	printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
		"                  ***********************************\n"
		"                  *   SHARPFIN RADIO PATCH SERVER   *\n"
		"                  * Press ENTER to exit this server *\n"
		"                  ***********************************\n"
		"\n"
		" 1. Stop any Web or DNS server on this machine\n"
		" 2. Configure your radio to use this machine as the DNS server\n"
		" 3. Connect your radio to the network\n"
		" 4. Perform radio upgrade\n"
		" 5. On completion, radio will reboot\n"
		"\n"
		"Version: " VERSION "\n\n") ;

	// Next Option = get the IP Address for the real DNS
	if (argc>sa) {
		strncpy(nameserver, argv[sa++], 1023) ; nameserver[1023]='\0' ;
	} else {
		printf("Please enter the IP address of your real DNS server:\n") ;
		fgets(reply, 1023, stdin) ;
		sscanf(reply, "%s", nameserver) ;
		fflush(stdin) ;
	}
	
	// Remaining options are the path to the tarfile.
	// It is done this way to accomodate Windows spaces in filenames
	// It will still fall over if there are any double (sequential) spaces in filenames, however
	if (argc>sa) {
		strncpy(tarfile, argv[sa++], 1023) ; tarfile[1023]='\0' ;		
		while (sa<argc) {
			if ((strlen(tarfile)+strlen(argv[sa])+1)<1023) {
				strcat(tarfile, " ") ;
				strcat(tarfile, argv[sa]) ;
			}
			sa++ ;
		}
	} else {
		printf("Please enter the URL of the patchfile (e.g. http://server.com/patchfile.tar) :\n") ;
		fgets(tarfile, 1023, stdin) ;
		if (tarfile[strlen(tarfile)-1]=='\n' || tarfile[strlen(tarfile)-1]=='\r') tarfile[strlen(tarfile)-1]='\0' ;
		fflush(stdin) ;	
	}

	if (mainloop(nameserver, tarfile)!=1) getchar() ;
	
#ifdef WINDOWS
	WSACleanup() ;
#endif
	return 0 ;
}

/*
 * Very crude mainloop.
 *
 * This program can handle only one DNS and one Web connection at any one time
 *
 * Note that we poll the exit keypress rather than add it to the select() list.  This is
 * because for Windows, select() is for network sockets only
 *
 */
 
int mainloop(char *nameserver, char *tarfile) {
	int r ;
	int maxfd ;
	int weblistener ;
	int dnslistener ;
	struct sockaddr_in *dnsserver_address ;
	fd_set fds_read ;
	int exit_mainloop ;
	struct timeval tv ;
	unsigned long state ;
	
	/* Initialise */

	exit_mainloop=0 ;
	tv.tv_sec=0 ;
	tv.tv_usec=300 ;

	/* Open network sockets to listen on */
	
	weblistener=webserver_openlistener(tarfile) ;
	dnslistener=dnsserver_openlistener() ;

	/* process/convert the supplied ASCII nameserver address */
	
	dnsserver_address=dnsserver_createrelay(nameserver) ;
	
	if (weblistener>0 && dnslistener>0 && dnsserver_address!=NULL) {
	
		/* Set STDIN to be non-blocking */
		state=1 ; ioctl(STDIN, FIONBIO, &state) ;
	
		do {	

			/* Build list of handles / sockets to monitor */
			FD_ZERO(&fds_read) ;
			maxfd=0 ;
			FD_SET(weblistener, &fds_read) ;
			if (weblistener>maxfd) maxfd=weblistener ;
			FD_SET(dnslistener, &fds_read) ;
			if (dnslistener>maxfd) maxfd=dnslistener ;

			/* wait for something to happen */		
			r=select(maxfd+1, &fds_read, NULL, NULL, &tv) ;		

			/* Process the received data */
			if (r<0) {
				perror("select()") ;
				exit_mainloop=-1 ;
			} else if (r==0) {
				/* Tick - check if enter key has been pressed */
				if (getchar()>0) exit_mainloop=1 ;
			} else {
				if (FD_ISSET(weblistener, &fds_read)) webserver_command(weblistener) ;
				if (FD_ISSET(dnslistener, &fds_read)) dnslistener=dnsserver_command(dnslistener, dnsserver_address) ;
			}
		
		} while (exit_mainloop==0 && dnslistener>0 && weblistener>0) ;

		/* Set STDIN to be blocking */
		state=0 ; ioctl(STDIN, FIONBIO, &state) ;
	
	}
	
	/* Tidy Up and Close Down */
	webserver_closelistener(weblistener) ;
	dnsserver_closelistener(dnslistener) ;
	
	return exit_mainloop ;
}	
