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
 * DNS Server
 */
#include "commandline.h"
#ifdef WINDOWS
#include <winsock.h>
int inet_aton(const char *cp, struct in_addr *addr) {
  addr->s_addr = inet_addr(cp);
  return (addr->s_addr == INADDR_NONE) ? 0 : 1;
}
#else
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include <unistd.h>

#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>

const char *dnsserver_getdnsrequestname(const char *dump, int len) ;
int dnsserver_dumpcontains(const char *test, const char *dump, int len) ;
void dnsserver_getmyipaddress(struct sockaddr_in *mydnsserver, struct sockaddr_in *thisend) ;

#define RESPONSE_LEN_MAX 65536
int ip0, ip1, ip2, ip3 ;


/*
 * dnsserver_openlistener
 *
 * Opens a new listener on the DNS server port (53)
 */
struct sockaddr_in *dnsserver_createrelay(char *nameserver)
{
	static struct sockaddr_in dnsserver_address ;
	struct sockaddr_in serv_addr, mysocket ;
	int clientfd ;
	unsigned long int add ;
	
	/* Create socket connection for our client connection to the real DNS server */
	if ( (clientfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
	
		printf("dnsserver: unable to connect to create UDP client - %s\n", strerror(errno)) ; 
		return NULL ;
		
	} else {

		/* Fill dnsserver_address with addres and port of real DNS server */
		memset((char *) &dnsserver_address, 0, sizeof(dnsserver_address));
		dnsserver_address.sin_family      = AF_INET;
		dnsserver_address.sin_port        = htons(53);
		if (inet_aton(nameserver, &dnsserver_address.sin_addr)==0) {
			printf("dnsserver: error, invalid nameserver ip address: %s\n", nameserver) ;
			close(clientfd) ;
			return NULL ;
		}
		
		/* Get my IP Address */
		dnsserver_getmyipaddress(&dnsserver_address, &mysocket) ;
		add=mysocket.sin_addr.s_addr ;
		ip0=add&0xFF ; add/=256 ;
		ip1=add&0xFF ; add/=256 ;
		ip2=add&0xFF ; add/=256 ;
		ip3=add&0xFF ; add/=256 ;

		close(clientfd) ;

		return &dnsserver_address ;
	}
	
}

/*
 * dnsserver_openlistener
 *
 * Opens a new listener on the DNS server port (53)
 */
int dnsserver_openlistener()
{
	struct sockaddr_in serv_addr ;
	int sockfd ;
	int err ;
	
	/* Create UDP Socket for our server */
        if ( (sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
       	        fprintf(stderr, "dnsserver: unable to create UDP server - \n", strerror(errno)) ;
		sockfd=(-1) ;
	}

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family      = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port        = htons(CONFIG_DNSSERVER_PORT);

	if (sockfd>=0 && (err = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))) < 0) {
		fprintf(stderr, "dnsserver: unable to bind to socket - %s\n", 
			strerror(errno)) ;
	        close(sockfd) ;
		sockfd=(-1) ;
	}

	if (sockfd>=0) printf("dnssever: waiting for connections ...\n") ;
	return sockfd ;
}

/*
 * dump_dns_response
 *
 * This function is used during development, to 
 * capture a DNS response to generate the code
 * to insert as a fake entry
 */
void dump_dns_response(unsigned char *p, int len)
{
	int i ;
	
	printf("\nresplen=%d ;\n", len) ;
	printf("ipaddrpos=xx ;\n") ;
	printf("memcpy(&p[2],\n\"") ;
	for (i=2; i<len; i++) {
		if (i%16==0) printf("\"\n\"") ;
		printf("\\x%02X", p[i]) ;
	}
	printf("\", resplen) ;\n") ;
}


/*
 * dnsserver_command
 *
 * Fetch Request from socket
 * If Server has Closed, Re-open it, and reject this particular request
 * If there is an error in the request, reject it
 * Else
 *   If request contains www.reciva.com, forge response
 *   Else
 *      Forward request to real DNS
 *      If response is valid, return it
 *      Else timeout
 *
 */
int dnsserver_command(int sockfd, struct sockaddr_in *dnsserver_address)
{
	int i, len, sum ;
	char p[RESPONSE_LEN_MAX] ;
	int s, clilen, pid;
	int resplen, ipaddrpos ;
	unsigned char *req ;
	struct sockaddr_in otherend ;
			
	/* Get Request */
	i=sizeof(struct sockaddr_in) ;
	len=recvfrom(sockfd, p, RESPONSE_LEN_MAX, 0, (struct sockaddr *)&otherend, &i) ;
	
	printf("dnsserver: %s: ", inet_ntoa(otherend.sin_addr)) ;
	
	if (len<0 && errno==ECONNRESET) {

		/* Especially for Winsock.  If Windows closes connection, re-open it */
		dnsserver_closelistener(sockfd) ;
		sockfd=dnsserver_openlistener() ;
		
		printf("%s .... IGNORED\n", strerror(errno)) ;

	} else if (len<0) {

		/* A Socket Error of some sort */
		fprintf(stderr, "%s\n", strerror(errno)) ;
		return sockfd ;

	} else {

		printf("lookup %s ...", dnsserver_getdnsrequestname((const char *)p, len)) ;
		fflush(stdout) ;

		/* Handle a Request */
		if (dnsserver_dumpcontains("www.reciva.com", (const char *)p, len)) {

			/* Forge the response for the www.reciva.com */

			resplen=127 ;
			ipaddrpos=44 ;
			memcpy(&p[2], 
			        "\x81\x80\x00\x01\x00\x01\x00\x02\x00\x02\x03\x77\x77\x77"
				"\x06\x72\x65\x63\x69\x76\x61\x03\x63\x6F\x6D\x00\x00\x01\x00\x01"
				"\xC0\x0C\x00\x01\x00\x01\x00\x00\x0D\x67\x00\x04"          "IPAD"
				"\xC0\x10\x00\x02\x00\x01\x00\x00\x6F\xD7\x00\x11\x03\x6E\x73\x31"
				"\x07\x63\x61\x6D\x6D\x61\x69\x6C\x03\x6E\x65\x74\x00\xC0\x10\x00"
				"\x02\x00\x01\x00\x00\x6F\xD7\x00\x06\x03\x6E\x73\x32\xC0\x40\xC0"
				"\x3C\x00\x01\x00\x01\x00\x02\xA2\x57\x00\x04\x50\xF8\xB2\x4B\xC0"
				"\x59\x00\x01\x00\x01\x00\x02\xA2\x57\x00\x04\x50\xF8\xB4\x55",
				resplen) ;	
			p[ipaddrpos]=ip0 ;
			p[ipaddrpos+1]=ip1 ;
			p[ipaddrpos+2]=ip2 ;
			p[ipaddrpos+3]=ip3 ;

			sendto(sockfd, p, resplen, 0, (struct sockaddr *)&otherend, sizeof(struct sockaddr_in)) ;
			printf(". OK\n") ;			

		} else if (dnsserver_dumpcontains(FAKESERVER, (const char *)p, len)) {

			/* Forge the response for the FAKESERVER */

			resplen=148 ;
			ipaddrpos=57 ;
			memcpy(&p[2],
				"\x85\x80\x00\x01\x00\x01\x00\x02\x00\x02\x03\x77\x77\x77"
				"\x08\x73\x68\x61\x72\x70\x66\x69\x6E\x0A\x66\x61\x6B\x65\x73\x65"
				"\x72\x76\x65\x72\x03\x63\x6F\x6D\x00\x00\x01\x00\x01\xC0\x0C\x00"
				"\x01\x00\x01\x00\x01\x51\x80\x00\x04"    "IPAD"    "\xC0\x10\x00"
				"\x02\x00\x01\x00\x01\x51\x80\x00\x13\x0B\x6D\x65\x64\x69\x61\x73"
				"\x65\x72\x76\x65\x72\x05\x6C\x6F\x63\x61\x6C\x00\xC0\x10\x00\x02"
				"\x00\x01\x00\x01\x51\x80\x00\x0C\x09\x77\x65\x62\x73\x65\x72\x76"
				"\x65\x72\xC0\x55\xC0\x68\x00\x01\x00\x01\x00\x01\x51\x80\x00\x04"
				"\xC0\xA8\x02\x12\xC0\x49\x00\x01\x00\x01\x00\x01\x51\x80\x00\x04"
				"\xC0\xA8\x02\x0D", resplen) ;

			p[ipaddrpos]=ip0 ;
			p[ipaddrpos+1]=ip1 ;
			p[ipaddrpos+2]=ip2 ;
			p[ipaddrpos+3]=ip3 ;

			sendto(sockfd, p, resplen, 0, (struct sockaddr *)&otherend, sizeof(struct sockaddr_in)) ;
			printf(". OK\n") ;			

		} else {
		
			/* Connect to the real DNS, and relay request */
			
			fd_set fds;
			struct timeval timeout;
			int dnsrelay ;
						
			if ( (dnsrelay = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
				printf("SOCKET ERROR - %s\n", strerror(errno)) ; 
			} else {
				fcntl(dnsrelay, F_SETFL, fcntl(dnsrelay, F_GETFL, 0) | O_NONBLOCK); 
	
				timeout.tv_sec = 1 ;
				timeout.tv_usec = 700;
				FD_ZERO(&fds);
				FD_SET(dnsrelay, &fds);

				sendto(dnsrelay, p, len, 0, (struct sockaddr *)dnsserver_address, sizeof(struct sockaddr_in)) ;
	
				i=select(dnsrelay+1, &fds, NULL, NULL, &timeout) ;
				if (i<=0) {
					// No Response 
					printf(" TIMEOUT\n") ;
				}  else {

					// Response
					i=sizeof(struct sockaddr_in) ;
					len=recvfrom(dnsrelay, p, RESPONSE_LEN_MAX, 0,  (struct sockaddr *)dnsserver_address, &i) ;
					// Dump the DNS response to stdout, so that fake entries can be created - used for code development
					// dump_dns_response(p, len) ;

					sendto(sockfd, p, len, 0, (struct sockaddr *)&otherend, sizeof(struct sockaddr_in)) ;	
					printf(" OK\n") ;
				}
				close(dnsrelay) ;
			}
		}
	}

	return sockfd ;
}

/*
 * dnsserver_closelistener
 *
 * Closes down the DNS server
 */
int dnsserver_closelistener(int dnslistener)
{
	if (dnslistener>=0) 
#ifdef WINDOWS
	closesocket(dnslistener) ;
#else
	close(dnslistener) ;
#endif
	return 0 ;
}

/*
 * dnsserver_closerelay
 *
 * Closes down the DNS server
 */
int dnsserver_closerelay(int dnsrelay)
{
	if (dnsrelay>=0) 
#ifdef WINDOWS
	closesocket(dnsrelay) ;
#else
	close(dnsrelay) ;
#endif
	return 0 ;
}

/*
 * crudely hack out the DNS request name from the UDP packet
 */
const char *dnsserver_getdnsrequestname(const char *dump, int len)
{
	static unsigned char r[1024] ;
	int l, i, d ;
	int end=0 ;
	
	i=12 ; d=0 ;
	do {
		l=dump[i++] ;
		if (l>0) {
			if (d!=0) r[d++]='.' ;
			while (l>0) {
				r[d]=dump[i] ;
				d++ ;
				i++ ;
				l-- ;
			}
		} else {
			end=1 ;
		}
	} while (end==0) ;
	r[d]='\0' ;

	return (const char *)r ;
}

void dnsserver_getmyipaddress(struct sockaddr_in *mydnsserver, struct sockaddr_in *thisend)
{
	int s, l ;
	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP) ;
	connect(s, (struct sockaddr *)mydnsserver, sizeof(struct sockaddr_in)) ;
	l=sizeof(struct sockaddr_in) ;
	getsockname(s, (struct sockaddr *)thisend, &l) ;
	close(s) ;
}

int dnsserver_dumpcontains(const char *test, const char *dump, int len)
{
	return (strcmp(dnsserver_getdnsrequestname(dump, len), test)==0) ;
}
