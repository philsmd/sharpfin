/* 
 * Sharpfin project
 * Copyright (C) by Steve Clarke and Ico Doornekamp
 * 2011-11-30 Philipp Schmidt
 *   Added to github 
 *
 * This file is part of the sharpfin project
 * This is an extremely limited/basic FTP server,using a subset of RFC959 Verbs
 * All transfers as in binary, irrespective of requested mode
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
 * Known bugs:
 * ftp server does not interpret %... characters in arguments (filenames / paths)
 * windows explorer gets confused with filenames / dates when softlinks are used
 * fireftp has problem with PASV data connection dropping
 */
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <wait.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <pwd.h>
#include <crypt.h>
#include "ftpd.h"

// FTPD State Variables
int state ;		// Logged in/out
char username[MAX] ;	// Supplied Username
char rnfr[MAX] ;	// Filename specified in first part of rename command
int datal, datap ;	// Socket handles for DataListener and DataPort
char *anonroot ;	// Root directory for anonymous logins
char *anonusername ;	// Username for anonymous logins

// General Network I/O Functions
int net_putc(int fd, int c) {
	int r ;
	r=write(fd, &c, 1) ;
	if (r==0) return (-1) ;
	else return (c) ;
}

int net_printf(int fd, const char *fmt, ...) {
	char buf[MAX] ;
	va_list va ;
	
	va_start(va, fmt) ;
	vsnprintf(buf, MAX-1, fmt, va) ;
	va_end(va) ;
	return write(fd, buf, strlen(buf)) ;
}

int net_getc(int fd)
{
	unsigned char c[2] ;
	int r ;
	r=read(fd, c, 1) ;
	if (r<=0) return (-1) ;
	else return (c[0]) ;
}

int net_getline(int fd, char *buf, int maxlen)
{
	int c, d=0 ;
	do {
		c=net_getc(fd) ;
		if (c<0) return -1 ;
		if (c!='\r' && c!='\n') buf[d++]=c ;
		else if (c=='\n' || d==maxlen-1) buf[d++]='\0' ;
	} while (buf[d-1]!='\0' && d<maxlen) ;
	return d ;
}

// FTPD File transmission functions
int ftpd_stor(int cs, int dp, char *filename)
{
	struct stat sb ;
	int j, totlen ;
	
	if (dp<0) return 1 ;

	// Check target either does not exist, or is a regular file
	if (stat(filename, &sb)==0 && !S_ISREG(sb.st_mode)) {
		net_printf(cs, ERROR_523) ;
		return 1 ;
	} else {
		int fh ;
		int len ;
		unsigned char buf[MAX] ;

		net_printf(cs, STATUS_150, filename) ;

		fh=open(filename, O_WRONLY|O_CREAT, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) ;
		if (fh<0) {
			net_printf(cs, ERROR_523) ;
			return 1 ;
		}
		j=0 ; totlen=0 ;
		while (j>=0 && (len=read(dp, buf, MAX-1))>0) {
			j=write(fh, buf, len) ;
			if (j>=0) totlen+=j ;
		}
		close(fh) ;
		
		if (len<0 || j<0) net_printf(cs, ERROR_524) ;
		else net_printf(cs, SUCCESS_226W, totlen) ;
		return 0 ;
	}
}

int ftpd_list(int cs, int dp, char *dir)
{
	DIR *d ;
	struct dirent *de ;
	struct stat sb ;
	char type ;
	struct tm *t ;
	char datestring[16] ;
	unsigned int size ;
	char sizet ;
	int totlen ;
	
	if (dp<0) {
		net_printf(cs, ERROR_522) ;
		return 1 ;
	}

	net_printf(cs, STATUS_150, "directory") ;

	totlen=0 ;
	if ((d=opendir(dir))!=NULL) {
	  while ((de=readdir(d))!=NULL) {
		if (stat(de->d_name, &sb)==0) {
			if (S_ISREG(sb.st_mode)) type='-' ;
			else if (S_ISDIR(sb.st_mode)) type='d' ;
			else type='?' ;
				
			t=localtime(&(sb.st_mtime)) ;
			if (((unsigned long int)time(NULL)-(unsigned long int)sb.st_mtime)>31449600)
				strftime(datestring, sizeof(datestring), "%b %d  %Y", t) ;
			else
				strftime(datestring, sizeof(datestring), "%b %d %H:%M", t) ;
	
			if (sb.st_size<99999) { size=(unsigned int)sb.st_size ; sizet=' ' ; }
			else if (sb.st_size<99999000) { size=(unsigned int)(sb.st_size+499)/1E3 ; sizet='K' ; }
			else { size=(unsigned int)(sb.st_size+499999)/1E6 ; sizet='M' ; }
			
			totlen+=net_printf(dp, "%crwxrwxrwx %5d %8d %8d %7u%c %s %s\n", 
				(char)type,
				(int)sb.st_nlink,
				(int)sb.st_uid,
				(int)sb.st_gid,
				(unsigned int)size,
				(char)sizet,
				datestring,
				de->d_name) ;
		}
	  }
	  closedir(d) ;
	}

	net_printf(cs, SUCCESS_226R, totlen) ;
	return 0 ;
}

int ftpd_retr(int cs, int dp, char *filename)
{
	struct stat sb ;
	if (dp<0) return 1 ;

	if (stat(filename, &sb)!=0) {
		net_printf(cs, ERROR_522) ;
		return 1 ;
	} else {
		if (S_ISDIR(sb.st_mode)) { // Mozilla uses RETR to get directory listings
			return ftpd_list(cs, dp, filename) ;
		} else {
			int fh ;
			int len ;
			unsigned char buf[MAX] ;
			int totlen ;

			totlen=0 ;
			net_printf(cs, STATUS_150, filename) ;

			fh=open(filename, O_RDONLY, 0) ;
			if (fh<0) {
				net_printf(cs, ERROR_522) ;
				return 1 ;
			}
			while ((len=read(fh, buf, MAX-1))>0) {
				totlen+=len ;
				write(dp, buf, len) ;
			}
			close(fh) ;

			net_printf(cs, SUCCESS_226R, totlen) ;
			return 0 ;
		}
	}
}

// FTPD Connection Establishment Functions
int ftpd_openlistener(int maxlisteners, int port)
{
	int sockfd, err;
	struct sockaddr_in serv_addr;

	if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("socket") ;
	} else {
		memset((char *) &serv_addr, 0,  sizeof(serv_addr));
		serv_addr.sin_family      = AF_INET;
		serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		serv_addr.sin_port        = htons(port);

		if ((err = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))) < 0) {
			perror("bind") ;
			close(sockfd) ;
			sockfd=-1 ;
		} else if (listen(sockfd, maxlisteners)<0) {
			perror("listen") ;
			close(sockfd) ;
			sockfd=-1 ;
		}
	}
	return sockfd ;
}

int ftpd_accept(int listener)
{
	int fd; 
	int clilen ;
	struct sockaddr_in cli_addr;
	clilen = sizeof(cli_addr);
	fd=accept(listener, (struct sockaddr *) &cli_addr, (socklen_t *) &clilen);
	if (fd<0) perror("accept") ;
	return fd ;
}

int ftpd_pasv(int cs, int dp)
{
		struct sockaddr_in so ;
		unsigned long int add ;
		socklen_t solen ;
		unsigned int port ;
		int a1=0, a2=0, a3=0, a4=0, p1=0, p2=0 ;
		
		if (dp<0 || cs<0) {
			perror("ftpd_pasv") ;
			net_printf(cs, ERROR_521) ;
			return -1 ;
		}
		solen=sizeof(so) ;

		// Get IP address from current port 21 connection
		if (getsockname(cs, (struct sockaddr *)&so, &solen)<0) {
			perror("getsockname(cs)") ;
			net_printf(cs, ERROR_521) ;
			return -1 ;
		}
		add=so.sin_addr.s_addr ;
		a1=(int)(add&0xFF) ; add/=256 ;
		a2=(int)(add&0xFF) ; add/=256 ;
		a3=(int)(add&0xFF) ; add/=256 ;
		a4=(int)(add&0xFF) ; add/=256 ;	

		// Get port number from datap handle
		if (getsockname(dp, (struct sockaddr *)&so, &solen)<0) {
			perror("getsockname(datap)") ;
			net_printf(cs, ERROR_521) ;
			return -1 ;
		}
		port=ntohs(so.sin_port) ;
		p2=(int)(port&0xFF) ; port/=256 ;
		p1=(int)(port&0xFF) ; port/=256 ;

		net_printf(cs, SUCCESS_227, a1, a2, a3, a4, p1, p2) ;
		return 0 ;
}

static const cmdtypes cmdlist[] = {
	{(char *)"noop", CMD_NOOP}, {(char *)"user", CMD_USER}, {(char *)"pass", CMD_PASS}, {(char *)"quit", CMD_QUIT},
	{(char *)"pasv", CMD_PASV}, {(char *)"port", CMD_PORT}, {(char *)"list", CMD_LIST}, {(char *)"retr", CMD_RETR}, 
	{(char *)"stor", CMD_STOR}, {(char *)"dele", CMD_DELE}, {(char *)"rmd", CMD_RMD}, {(char *)"feat", CMD_FEAT},
	{(char *)"cwd", CMD_CWD}, {(char *)"pwd", CMD_PWD}, {(char *)"type", CMD_TYPE}, {(char *)"mkd", CMD_MKD},
	{(char *)"rnfr", CMD_RNFR}, {(char *)"rnto", CMD_RNTO}, {(char *)"", CMD_ERR} } ;

// Command parsing functions
int ftpd_cmd(int cs)
{
	int cmdi ;	// command code
	int c ;		// I/O error Code / argument count
	int i,j ;	// General loop counters
	char line[MAX], cmd[MAX], arg[MAX] ;

	// Parse Line
	c=net_getline(cs, line, MAX) ;
	if (c<0) { net_printf(cs, ERROR_520) ; return 1 ; }
	if (c==0) line[0]='\0' ; // Blank Line
	for (i=0; line[i]!='\0' && line[i]!=' '; i++)
		cmd[i]=tolower(line[i]) ;
	cmd[i]='\0' ;
	while (line[i]==' ') i++ ;
	for (j=0; line[i+j]!='\0'; j++) 
		arg[j]=line[i+j] ;
	arg[j]='\0' ;
	
	// Decode the command Verb
	for (i=0, cmdi=CMD_ERR; cmdlist[i].cmdi!=CMD_ERR; i++)
		if (strcmp(cmd, cmdlist[i].cmd)==0) cmdi=cmdlist[i].cmdi ;
	if (cmdi==CMD_PASS) debug("cmd=\"%s\", arg=\"********\"\n", cmd) ;	
	else debug("cmd=\"%s\", arg=\"%s\"\n", cmd, arg) ;	

	switch (cmdi) {	// valid commands in all states :-
	
		case CMD_ERR: 
			net_printf(cs, ERROR_500) ; 
			break ;
			
		case CMD_QUIT:
			net_printf(cs, SUCCESS_221) ; 
			return 1 ; // 1=Exit
			break ;
			
		case CMD_NOOP: 
			net_printf(cs, SUCCESS_200) ; 
			break ;
			
		case CMD_FEAT:
			net_printf(cs, SUCCESS_211) ;
			break ;
			
		default:
			if (state==STATE_LOGOUT) { 
			
			    switch(cmdi) { // Logout State Commands :-
					
			    case CMD_USER:
			    	strcpy(username, arg) ;
			    	net_printf(cs, STATUS_331) ;
				debug("login - username=%s\n", username) ;
			    	break ;
				
			    case CMD_PASS:
				if (username[0]=='\0') {
			    		net_printf(cs, ERROR_503) ;
				} else if (strcmp(username, "anonymous")==0 || strcmp(username,"ftp")==0) {
					// anonymous login (become the 'user' account irrespective of password) 
					const struct passwd *pw ;
					if (anonusername==NULL || anonroot==NULL) {
						debug("ANON LOGIN - ftp user/root params not specified\n") ;
						net_printf(cs, ERROR_506) ;
					} else {
						pw = getpwnam(anonusername) ;
						debug("ANON LOGIN - setting root dir: %s\n", anonroot) ;
						debug("ANON LOGIN - setting username: %s\n", anonusername) ;
						if (chroot(anonroot)!=0) {
							debug("ANON LOGIN - Set root dir FAILED\n") ;
							net_printf(cs, ERROR_505) ;
						} else if (pw==NULL || pw->pw_uid<100 || setuid(pw->pw_uid)!=0) {
							debug("ANON LOGIN - Set username FAILED\n") ;
							net_printf(cs, ERROR_506) ;
						} else {
							debug("ANON LOGIN - SUCCESS\n") ;
							chdir("/") ;
							net_printf(cs, SUCCESS_230A) ;
							state=STATE_LOGIN ;
						}
					}
				} else { 
					// password login (change to the requested user)
					const struct passwd *pw ;
					debug("USER LOGIN - Checking password\n") ;
					pw = getpwnam(username) ;
					if (pw==NULL) {
						debug("USER LOGIN - Username check FAILED\n") ;
						net_printf(cs, ERROR_503) ;
					} else if (strcmp(pw->pw_passwd, crypt(arg, pw->pw_passwd))!=0 || setuid(pw->pw_uid)!=0) {
						debug("USER LOGIN - Password check FAILED\n") ;
						net_printf(cs, ERROR_503) ;

					} else {	
						debug("USER LOGIN - SUCCESS\n") ;
						chdir("/") ;
						net_printf(cs, SUCCESS_230N) ;
						state=STATE_LOGIN ;
					}
			    	}
				break ;
				
			    default:
			    	net_printf(cs, ERROR_500) ;
			    	break ;
				}
				
			} else if (state==STATE_LOGIN) {
				
			    switch (cmdi) { // Login State Commands ;
			
			    case CMD_PORT:
				net_printf(cs, ERROR_500_PORT) ;
				break ;

			    case CMD_PASV:
				if (datal>0) close(datal) ;
				if (datap>0) close(datap) ;
				datap=(-1) ;
				datal=ftpd_openlistener(1, CONFIG_ANY_PORT) ;
				usleep(500) ;
				ftpd_pasv(cs, datal) ;
				break ;
				
			    case CMD_LIST:
				ftpd_list(cs, datap, (char *)".") ;
				if (datap>=0) {
					usleep(500) ;
					close(datap) ;
					datap=(-1) ;
				}
				break ;
				
			    case CMD_RETR:
				ftpd_retr(cs, datap, arg) ;
				if (datap>=0) {
					usleep(500) ;
					close(datap) ;
					datap=(-1) ;
				}
				break ;				
				
			    case CMD_STOR:
				ftpd_stor(cs, datap, arg) ;
				if (datap>=0) {
					usleep(500) ;
					close(datap) ;
					datap=(-1) ;
				}
				break ;				
				
			    case CMD_PWD:
				{ char p[MAX] ;
				getcwd(p, MAX-1) ;
				net_printf(cs, SUCCESS_257, p) ; }
				break ;				
				
			    case CMD_CWD:
				if (arg[0]=='\0') {
					net_printf(cs, ERROR_504) ;
				} else {
					if (chdir(arg)==0) 
						net_printf(cs, SUCCESS_250) ;
					else
						net_printf(cs, ERROR_550) ;
				}
				break ;	
				
			    case CMD_TYPE:
				net_printf(cs, SUCCESS_200) ;
				break ;
				
			    case CMD_MKD:
				if (arg[0]=='\0' || mkdir(arg, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)<0)
					net_printf(cs, ERROR_550) ;
				else
					net_printf(cs, SUCCESS_200) ;
				break ;
				
			    case CMD_RMD:
				if (arg[0]=='\0' || rmdir(arg)<0)
					net_printf(cs, ERROR_550) ;
				else
					net_printf(cs, SUCCESS_200) ;
				break ;

			    case CMD_RNFR:
				if (arg[0]=='\0') {
					net_printf(cs, ERROR_550) ;
				} else {
					strcpy(rnfr, arg) ;
					net_printf(cs, STATUS_350) ;
				}
				break ;
				
			    case CMD_RNTO:
				if (arg[0]=='\0' || rnfr[0]=='\0' || rename(rnfr, arg)!=0) {
					net_printf(cs, ERROR_550) ;
				} else {
					net_printf(cs, SUCCESS_250) ;
				}
				break ;
				
			    case CMD_DELE:
				if (arg[0]=='\0' || unlink(arg)<0)
					net_printf(cs, ERROR_550) ;
				else
					net_printf(cs, SUCCESS_200) ;
				break ;
				
			    default:
				net_printf(cs, ERROR_500) ;
				break ;

			    }
			}
	}	
	return 0 ; // 0=Do Not Exit
}

int ftpd_session(int cs)
{
	int session_exit=0 ;
	struct timeval tv ;
	int maxfd, r ;
	fd_set fds_read ;
	
	state=STATE_LOGOUT ;		// Default State
	datal=-1 ;			// Data listener handle
	datap=-1 ;			// Data traffic handle
	username[0]='\0' ;		// No username supplied
	rnfr[0]='\0' ;			// Rename-From store reset
	tv.tv_sec=30 ; tv.tv_usec=0 ;	// timeout is 30 seconds
	
	net_printf(cs, SUCCESS_220, "$Id: ftpd.c 277 2008-05-09 23:40:08Z steve $") ;
	
	do {
		FD_ZERO(&fds_read) ; maxfd=0 ;
		if (cs>0) FD_SET(cs, &fds_read) ;
		if (cs>maxfd) maxfd=cs ;
		if (datal>0) FD_SET(datal, &fds_read) ;
		if (datal>maxfd) maxfd=datal ;
		r=select(maxfd+1, &fds_read, NULL, NULL, &tv) ;		

		if (r<0) { // Error encountered
			perror("select()") ;
			session_exit=1 ;			
		} else if (r==0) { // Timeout Occurred
			net_printf(cs, ERROR_421) ;
			session_exit=1 ;
		} else {
			if (datal>0 && FD_ISSET(datal, &fds_read)) { // Request to establish data connection received
				if (datap>0) {
					close(datap) ;
					usleep(500) ;
				}
				datap=ftpd_accept(datal) ;
				
			} else if (FD_ISSET(cs, &fds_read)) { // Command Received
				session_exit=ftpd_cmd(cs) ;
			}
		}
	} while (session_exit==0) ;

	// Close ports down
	if (datal>0) close(datal) ;
	if (datap>0) close(datap) ;
	
	return 0 ;
}

void zombie_reaper(int sig) 
{
	int status;
	debug("Connection closed\n") ;
	while (wait4(-1, &status, WNOHANG, NULL) > 0);
}

/* NOTE - Probably need a zombie reaper as we are using fork() */
#ifndef STANDALONE
int ftpd_main(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
	int listener, fd ;
	int exit_mainloop=0 ;

	printf("Sharpfin FTPD - $Id: ftpd.c 277 2008-05-09 23:40:08Z steve $\n") ;
	printf("  ftpd [anonusername anonrootdirectory]\n") ;
	
	if (argc==3 && chdir(argv[2])==0) { anonusername=argv[1] ; anonroot=argv[2] ; }
	else { anonusername=NULL ; anonroot=NULL ; }
	
	listener=ftpd_openlistener(10, CONFIG_FTPD_PORT) ;
	if (listener<0) return 1 ;

	printf("  running in background ...\n") ;
	if (fork()!=0) return 0 ; // Run in background

	signal(SIGCHLD, zombie_reaper) ;

	do {	

	fd = ftpd_accept(listener) ;

		if (fd>0 && fork()==0) {
			ftpd_session(fd) ;
			close(fd) ;
			return 0 ;
		}
	
	} while (exit_mainloop==0) ;

	close(listener) ;
	return 0 ;
}
