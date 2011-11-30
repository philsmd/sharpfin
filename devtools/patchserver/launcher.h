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

// Maximum string size
#define MAX 1024

// Icon
#define IDI_APP_ICON 1
#define IDI_APP48_ICON 2

// Input dialog Box
#define IDD_DLGINPUT 101
#define IDC_PROMPT 200
#define IDC_TEXT 201

char *inputstring(HINSTANCE hwnd, char *title, char *prompt) ;
char *getfilename(char *ext, char *title) ;

// Select dialog Box
#define IDD_DLGSELECT 102
#define IDC_LIST 300
#define IDC_DNS 301
int selectpatch(HINSTANCE parent, char *dnsaddress, int entries, char **list) ;

// Registry Keys
#define REGKEY_DNS "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters"
#define REGKEY_DNS_KEY "NameServer"
#define REGKEY_DNS_KEY2 "DhcpNameServer"

char **geturls(int *numpatches) ;

// Open Web Page, Read Patch Line from Page and Close Page
int openwebpage(char *server, char *file, int port, int *errcode) ;
int getwebpage(int sockfd, int isnet, char *url, char *description) ;
int closewebpage(int sockfd) ;
