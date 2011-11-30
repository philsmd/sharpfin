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

#include <windows.h> 
#include <stdlib.h>
#include <string.h>
#include "alloc.h"
#include <stdio.h>
#include <process.h>
#include "launcher.h"
#include "terms.h"

int getregistry(HKEY section, char *subkey, char *value, char *response, int maxlen)
{
	HKEY s ;
	DWORD len ;
	len=maxlen-1 ;
	
	if (RegOpenKey(section, (LPCSTR)subkey, &s)!=ERROR_SUCCESS) {
		// subkey not found, fail
		response[0]='\0' ;
		return (1==0) ;
	}
	
	if (RegQueryValueEx((HKEY)s, (LPCSTR)value, NULL, NULL, (LPBYTE)response, (LPDWORD)&len) != ERROR_SUCCESS) {
		// value not found, fail
		response[0]='\0' ;
		RegCloseKey(s) ;
		return (1==0) ;
	}

	
	if (response[0]=='\0') {
		// Empty key Found - return fail
		RegCloseKey(s) ;
		return (1==0) ;
	} else {
		// valid key found, return it!
		RegCloseKey(s) ;
		return (1==1) ;
	}
}

int cygwincheck()
{
	char *pathname=NULL ;
	char *path ;
	
	/* look for cygwin in the path */
	if (getenv("PATH")!=NULL) pathname="PATH" ;
	if (getenv("Path")!=NULL) pathname="Path" ;
	if (getenv("path")!=NULL) pathname="path" ;
	if (pathname==NULL) {
		MessageBox(0,"You don't seem to have a PATH set","Problem ...",MB_OK); 
		return (1==0); 	
	}
	
	path=getenv(pathname) ;
	if (strstr(path, "ygwin")==NULL && strstr(path,"YGWIN")==NULL) {
			
		FILE *fp=fopen("c:\\cygwin\\bin\\cygwin1.dll","r") ;
		fclose(fp) ;
		
		if (fp==NULL) {
			MessageBox(0,"Unable to find Cygwin (from www.cygwin.com) in the path.\nIs it installed in C:\\CYGWIN ?","Problem ...",MB_OK); 
			return (1==0); 
		} else {
			char *b ;
			/* Pre-pend c:\cygwin\bin to the path */
			b=(char *)malloc(strlen(path)+256) ;
			if (b==NULL) {
				MessageBox(0,"Memory allocation problems","Problem ...",MB_OK); 
				return (1==0); 
			}
			strcpy(b, pathname) ;
			strcat(b, "=c:\\cygwin\\bin;") ;
			strcat(b, path) ;
			putenv(b) ;
			free(b) ;
		}
	}
	return (1==1) ;
}


int WinMain(HINSTANCE hwnd,HINSTANCE,LPSTR orgargv,int) 
{ 
	char *args[5] ;
	char **patches ;
	char **patchurl ;
	char dnsserver[MAX], *tarfile ;
	int numpatches, selectedpatch ;
	
	/* Check cygwin is installed */
	if (!cygwincheck()) return 1 ;
	
	/* Get the Ts&Cs Agreement */
	if (MessageBox(0, TERMS, "Terms and Conditions", MB_YESNO)!=IDYES) return 0 ;

	/* Get the list of patchfiles */
	patches=geturls(&numpatches) ;
	if (numpatches<=0) {
		MessageBox(0, "Unable to download list of patches from server.\n"
			" * Please check your network connection.\n"
			" * Please check you have the latest version of patchserver.\n"
			" * If you know the patch URL, or have a local patchfile, You still have\n"
			"   the option of running the patchserver-commandline version directly.\n", "Error", MB_OK) ;
		exit(1) ;
	}
	
	/* Try to read the DNS server from the registry */
	if (!getregistry(HKEY_LOCAL_MACHINE, REGKEY_DNS, REGKEY_DNS_KEY, dnsserver, MAX) &&
			!getregistry(HKEY_LOCAL_MACHINE, REGKEY_DNS, REGKEY_DNS_KEY2, dnsserver, MAX))
		strcpy(dnsserver, "x.x.x.x") ;
	
	/* Ask the user for the patch file and DNS server */
	do {
		selectedpatch=selectpatch(0, dnsserver, numpatches, (char **)patches) ;
		// Cancel was selected
		if (selectedpatch==(-2)) return 0 ;

		// Get the tarfile name.  If it is '-', it is not a valid selection
		if (selectedpatch>=0) {
			tarfile=patches[selectedpatch] ;
			if (tarfile[0]=='-') selectedpatch=(-1) ;
		}

		// Check the selections
		if (!((dnsserver[0]>='0') && (dnsserver[0]<='9'))) MessageBox(0, "Please enter a valid DNS server", "Ooops!", MB_OK) ;
		else if (selectedpatch==(-1)) MessageBox(0, "Please select a patch", "Ooops!", MB_OK) ;

	} while (selectedpatch==(-1) || !( (dnsserver[0]>='0') && (dnsserver[0]<='9') )) ;
	
	
	/* All OK, run the real program */
	args[0]="patchserver-commandline.exe" ;
	args[1]="-accept" ;
	args[2]=dnsserver;
	args[3]=tarfile ;
	args[4]=NULL ;
	execvp(args[0], args) ;
	
	/* Error! */
	MessageBox(0,"Unable to locate patchserver-commandline.exe", "Problem ...",MB_OK); 
	return 1; 	
} 
