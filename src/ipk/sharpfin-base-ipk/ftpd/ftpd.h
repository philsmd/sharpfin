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

#define CONFIG_FTPD_PORT 21
#define CONFIG_ANY_PORT 0
#define MAX 1024

#define CMD_NOOP 0
#define CMD_USER 1
#define CMD_PASS 2
#define CMD_QUIT 3
#define CMD_PASV 4
#define CMD_LIST 5
#define CMD_RETR 6
#define CMD_STOR 7
#define CMD_CWD 8
#define CMD_PWD 9
#define CMD_TYPE 10
#define CMD_PORT 11
#define CMD_DELE 12
#define CMD_RMD 13
#define CMD_FEAT 14
#define CMD_MKD 15
#define CMD_RNFR 16
#define CMD_RNTO 17
#define CMD_ERR 99

#define STATE_LOGOUT 0
#define STATE_LOGIN 1

#define STATUS_150 "150 Ready to download %s\n"
#define STATUS_151 "151 Ready to upload %s\n"

#define SUCCESS_200 "200 OK\n"
#define SUCCESS_211 "211-Features\n PASV\n211 End\n"
#define SUCCESS_220 "220 Sharpfin FTP Server - %s\n"
#define SUCCESS_221 "221 QUIT OK\n"
#define SUCCESS_226R "226 Transfer OK - %d bytes received\n"
#define SUCCESS_226W "226 Transfer OK - %d bytes sent\n"
#define SUCCESS_227 "227 PASV OK (%d,%d,%d,%d,%d,%d)\n"
#define SUCCESS_230N "230 Login OK\n" 
#define SUCCESS_230A "230 Anonymous Login OK\n" 
#define SUCCESS_257 "257 \"%s\"\n"
#define SUCCESS_250 "250 OK\n"

#define STATUS_331 "331 Enter Password\n"
#define STATUS_350 "350 RNFR OK\n"

#define ERROR_421 "421 Timeout\n"

#define ERROR_500 "500 Unknown Command\n"
#define ERROR_500_PORT "500 PORT Command not supported, use PASV Mode\n"
#define ERROR_503 "503 Bad username or password\n"
#define ERROR_504 "504 Bad Argument\n"
#define ERROR_505 "505 Unable to find anonymous 'user' account on system\n"
#define ERROR_506 "506 Unable to change to anonymous home directory\n"
#define ERROR_520 "520 Connection Problem\n"
#define ERROR_521 "521 PASV Connection Failed\n"
#define ERROR_522 "522 Data Connection Problem, did you use PASV?\n"
#define ERROR_523 "523 Error Creating Destination File - does it already exist read-only?\n"
#define ERROR_524 "524 Download interrupted, created file may be incomplete\n"
#define ERROR_530 "530 Login Incorrect\n"
#define ERROR_550 "550 Permission Denied\n"

typedef struct {
	char *cmd;
	int cmdi;
} cmdtypes;

#define debug printf
