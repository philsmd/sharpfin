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

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <errno.h>

main(int argc, char *argv[])
{
	int sockfd, clilen, err, pid;
	struct sockaddr_in cli_addr, serv_addr;
	int i, len, slen ;
	unsigned char p[1024] ;
	
	if (argc!=2) {
		printf("clienttest ip_address_of_dns_server\n") ;
		return ;
	}
	
	if ( (sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
		fprintf(stderr, "unable to create UDP client\n") ; 
		return ;
	}

	bzero((char *) &serv_addr, sizeof(serv_addr));
	inet_aton(argv[1], &serv_addr.sin_addr);
	serv_addr.sin_family      = AF_INET;
	serv_addr.sin_port        = htons(53);

/* Firmware update check:	
"\xFB\xFE\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00\x03\x77\x77\x77"
"\x06\x72\x65\x63\x69\x76\x61\x03\x63\x6F\x6D\x00\x00\x01\x00\x01"
*/

	/* Output request Message to DNS Server */
	slen=sizeof(serv_addr) ;
	sendto(sockfd, 
		"\xFB\xFE\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00\x03\x77\x77\x77"
		"\x06\x72\x65\x63\x69\x76\x61\x03\x63\x6F\x6D\x00\x00\x01\x00\x01"
		, 32, 0, (struct sockaddr *)&serv_addr, slen) ;
	
	/* Fetch the response */
	slen=sizeof(serv_addr) ;
	len=recvfrom(sockfd, p, 1024, 0, (struct sockaddr *)&serv_addr, &slen) ;
	
	/* Print it out */
	printf("\"") ;
	for (i=0; i<len; i++) {
		printf("\\x%02X", p[i], (p[i]>=32 && p[i]<=126)?p[i]:'.') ;
		if ((i+1)%16==0) printf("\"\n\"") ;
	}
	if ((i+2)%16!=0) printf("\"\n") ;
	printf("len=%d\n", i) ;
	
}

