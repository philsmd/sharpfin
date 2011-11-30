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

#include <windows.h>
#include <winsock.h>
#include <sys/types.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include "launcher.h"

static char **patches ;
void *allocatememory(int size) {
	void *ptr ;
	ptr=malloc(size) ;
	if (ptr==NULL) {
		MessageBox(0,"Out of memory", "Problem ...",MB_OK); 
		exit(1) ; 	
	}
	return ptr ;
}

/*
 * read next character, ignoring any \r chars
 */
int readc(int h, int isnet) {
	int len ;
	char buf[2] ;
	
	do {
		if (isnet) {
			len=recv(h, buf, 1, 0) ;
		} else {
			len=read(h, buf, 1) ;
		}
	} while (len==1 && buf[0]=='\r') ;
	if (len<=0) return -1 ;
	else return buf[0] ;
}

/*
 * read line, removing any leading or trailing spaces
 */
int readln(int handle, int isnet, char *buf, int max)
{
	int c, p ;
	
	p=0 ;
	
	// Skip Leading White Characters
	do {
		c=readc(handle, isnet) ;
	} while (c==' ' || c=='\t') ;

	// Copy the line
	if (c>0 && c!='\n') do {
		buf[p]=c ;
		p++ ;
		c=readc(handle, isnet) ;
	} while (c!='\n' && p<max-1) ;

	// Remove trailing white characters
	while (p>0 && buf[p-1]==' ' || buf[p-1]=='\t') p-- ;

	// terminate and return
	buf[p]='\0' ;
	if (c<0) return -1 ;
	else return p ;
}


//
// The URLs source is in the format:
//
// www.url.com     /path/to/file.tar    port#
//
// or..
//
//  -
// PATCHFILE  URL1 Description of URL1\n
// PATCHFILE  URL2 Description of URL2\n
//
//
// This file, or the contents of the supplied URL is loaded into the urls list, using successive indexes, i.e.
//
// patches[0]="URL1"
// patches[1]="Description of URL1"
// patches[2]="URL2"
// patches[3]="Description of URL2"
//
char **geturls(int *numpatches)
{
	int h ;
	char url[MAX] ;
	char desc[MAX] ;
	int isnet ;
	int errcode ;
	
	// Open local urls file
	h=open("patchfiles.lst", O_RDONLY) ;
	if (h<=0) {
		MessageBox(0, "Unable to find patchfiles.lst.\nThis file tells the patchserver where to get the list of patchfiles from.\n", "Error...", MB_OK) ;
		return NULL ;
	}
	if (readln(h, (1==0), url, MAX)<=0) {
		MessageBox(0, "patchfiles.lst appears to be empty.\n", "Error...", MB_OK) ;
		close(h) ;
		return NULL ;
	}

	if (url[0]=='-') {

		// If the file starts with a '-', this means that the file contains the urls so leave it open
		isnet=(1==0) ;
	} else {
		char server[MAX], path[MAX] ;
		int port ;
		
		// If the file does not start with a '-', assume it is the URL of the real patchlist, so open that instead
		close(h) ;
		if (sscanf(url, "%s %s %d", server, path, &port)!=3) {
			MessageBox(0, "patchfiles.lst appears to be in the wrong format.\n", "Error...", MB_OK) ;
			return NULL ;
		}

		h=openwebpage(server, path, port, &errcode) ;
		isnet=(1==1) ;
		if (h<=0) {
			sprintf(desc, "Unable to connect to server: %s.\n", server) ;
			MessageBox(0, desc, "Error...", MB_OK) ;
			return NULL ;
		}
		if (errcode<200 || errcode>299) {
			sprintf(desc, "Server %s responded with error %d\n", server, errcode) ;
			MessageBox(0, desc, "Error...", MB_OK) ;
			return NULL ;
		}
		
	}
	
	patches=(char **)allocatememory(MAX * sizeof(char *)) ;
	while (getwebpage(h, isnet, url, desc) && (*numpatches)<=MAX-2) {

		if (url[0]!='\0' && desc[0]!='\0') {
			patches[(*numpatches)]=(char *)allocatememory(strlen(url)+1) ;
			strcpy(patches[(*numpatches)++], url) ;
			patches[(*numpatches)]=(char *)allocatememory(strlen(desc)+1) ;
			strcpy(patches[(*numpatches)++], desc) ;
		}
	}

	if (isnet) closesocket(h) ;
	else close(h) ;
	
	return patches ;
}

int openwebpage(char *server, char *file, int port, int *response)
{
	int sockfd ;
	struct sockaddr_in serv_addr ;
	struct  hostent *hp ;
	WSADATA n ;
	char buf[MAX], buf2[MAX] ;
	static char *reqhdr = " HTTP/1.0\r\nAccept: */*\r\nHost: " ;
	static char *ua = "\r\nUser-Agent: Mozilla/4.0 (Compatible - Shaprfin Patch Server)\r\n\r\n" ;
	
	if (WSAStartup(MAKEWORD(2,2),&n)!=0) {
		MessageBox(0, "Unable to initialise Windows Socket System.", "Winsock Error", MB_OK) ;
		return -1 ;
	}
	
	// Create Socket
	if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		MessageBox(0, "Unable to create TCP Socket", "Network Error", MB_OK) ;
		return -1 ;
	}

	// Lookup name
	hp = gethostbyname(server);	
	
	// Connect and Open
	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	memcpy(&(serv_addr.sin_addr.s_addr), hp->h_addr, hp->h_length);
	serv_addr.sin_port = htons(port);

	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))<0) {
		sprintf(buf, "Unable to connect to server %s", server) ;
		MessageBox(0, buf, "Network Error", MB_OK) ;
		closesocket(sockfd) ;
		return -1 ;
	}

	// Send Request
	send(sockfd, "GET ", 4, 0) ; send(sockfd, file, strlen(file), 0) ;
	send(sockfd, reqhdr, strlen(reqhdr), 0) ; send(sockfd, server, strlen(server), 0) ;
	send(sockfd, ua, strlen(ua), 0) ;
	
	if (readln(sockfd, (1==1), buf, MAX)>0)
		sscanf(buf, "%s %d", buf2, response) ;
	else
		*response=901 ;

	return sockfd ;
}

/*
 * getwebpage
 *
 * this function returns the URL and Description for the line if it is in the format
 *   PATCHFILE url description
 */
int getwebpage(int sockfd, int isnet, char *url, char *description)
{
	static char buffer[MAX], hdr[MAX] ;
	int len ;
	
	// Get Next Entry
	
	len=readln(sockfd, isnet, buffer, MAX-1) ;

	if (len<0) return (1==0) ;

	// Extract URL
	sscanf(buffer, "%s %s", hdr, url) ;

	if (strcmp(hdr, "PATCHFILE")!=0) {
		url[0]='\0' ;
		description[0]='\0' ;
	} else {
		// Skip White Spaces Extract Description
		len=strlen(url)+strlen("PATCHFILE ") ;
		while (buffer[len]==' ' || buffer[len]=='\t') len++ ;
		strcpy(description, &buffer[len]) ;
	}
	
	return (1==1) ;
}



