/* 
 * Sharpfin project
 * Copyright (C) 2007 by Steve Clarke
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
#include <stdio.h>
#include <stdlib.h>
#if defined( _WIN32)        // Visual C build
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#elif defined(__CYGWIN__)   // Cygwin build
#include <windows.h>
#include <sys/io.h>
#include <sys/perm.h>
#else                       // Linux build
#include <sys/io.h>
#include <unistd.h>
#include <linux/parport.h>
#include <linux/ppdev.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#endif

#include "def.h"
#include "ppt.h"

// Printer port and bit definitions
#define ECP_ECR_OFFSET		0x402       // Looks wrong but it's actually right
#define ECR_STANDARD		0x00
#define ECR_DISnERRORINT	0x10
#define ECR_DISDMA		0x00
#define ECR_DISSVCINT		0x04

int validPpt;
int usingGiveio;
// Default to LPT1 unless over-ridden on the commandline
static U16 base_address = LPT1;
// Permitted printer ports
static U16 printer_ports[] = { LPT1, LPT2, LPT3 };

void OutputPpt(U8 value) {
#if !defined(_WIN32) || defined(__CYGWIN__)
#if defined(LINUX_PPDEV) && ! defined(__CYGWIN__)
    int i = value;
    ioctl(validPpt, PPWDATA, &i);
#elif defined(__CYGWIN__)
    outb( value, base_address );
#else
    outb( base_address, value );
#endif
#else
    _outp( value & 0xff, base_address );
#endif
}


U8 InputPpt( void ) {
#if !defined(_WIN32) || defined(__CYGWIN__)
#if defined(LINUX_PPDEV) && ! defined(__CYGWIN__)
    int i;
    ioctl(validPpt, PPRSTATUS, &i);
    return (i & 0xff);
#else
    int j=inb( base_address + 1 );
    return j;
#endif
#else
    return _inp( (U16) (base_address + 1) );
#endif
}



/*
 * Return 0 (fail) if num<1 or num>2
 * Otherwise return non-zero
 */
int SetPrinterNumber( int num ) {
    if (num<1 || num>3)
        return 0;
    base_address = printer_ports[ num-1 ];
    return 1;
}

/*
 * Return -1 (fail)
 * Otherwise return non-zero printer index
 */
int GetPrinterIndex( int base_address ) {
    int i;
    for (i=0;i<sizeof(printer_ports)/sizeof(U16);i++) {
        if (printer_ports[i]==base_address) {
            return i;
        }
    }
    return -1;
}

// Just put the port into standard parallel mode
void ConfigureParallelPort(void) {
    // Configure the parallel port in standard mode
#if !defined(_WIN32) || defined(__CYGWIN__)
#if defined(LINUX_PPDEV) && ! defined(__CYGWIN__)
        int j;
	int ppt; 
        int k=GetPrinterIndex(base_address);
        if (k<0) {
            fprintf(stderr,"Error: Printer port index not found");
            exit(1);
        }
	char linux_device[512];
        sprintf(linux_device,"/dev/parport%d",k);
        ppt = open(linux_device, O_WRONLY);
        if (ppt < 0) {
                fprintf(stderr, "can't open %s\n",linux_device);
                exit(1);
        }

        j = ioctl(ppt, PPCLAIM);
        if (j < 0) {
                fprintf(stderr, "can't claim device\n");
                close(ppt);
                exit(1);
        }
	int i;
	i = PARPORT_MODE_COMPAT;
	i = ioctl(ppt, PPSETMODE, &i);
	if (i < 0) {
		fprintf(stderr, "can't set compatible mode\n");
		close(ppt);
		exit(1);
	}
	i = IEEE1284_MODE_COMPAT;
	i = ioctl(ppt, PPNEGOT, &i);
	if (i < 0) {
		fprintf(stderr, "can't set compatible 1284 mode\n");
		close(ppt);
		exit(1);
	}
	validPpt=ppt;
#else
        // PPCLAIM, PPSETMODE    , PPNEGOT
        // *      , PARPOR COMPAT, IEEE1284 COMPAT
        if (ioperm(base_address,3,1)) {
   	    printf("Sorry, you were not able to gain access to the ports\n");
   	    printf("You must be root to run this program\n");
   	    exit(1);
   	}

#if defined(__CYGWIN__)
	outb( ECR_STANDARD | ECR_DISnERRORINT | ECR_DISDMA | ECR_DISSVCINT, (U16)(base_address + ECP_ECR_OFFSET) );
#else
	outb( (U16)(base_address + ECP_ECR_OFFSET),  ECR_STANDARD | ECR_DISnERRORINT | ECR_DISDMA | ECR_DISSVCINT );
#endif // CYGWIN OR NOT
#endif
#else
	_outp( (ECR_STANDARD | ECR_DISnERRORINT | ECR_DISDMA | ECR_DISSVCINT), (U16)(base_address + ECP_ECR_OFFSET) );
#endif
}

/*
 * To enable I/O port access, under Linux we use the iopl() library call and must therefore run in a root shell.
 * Under windows (XP/NT), use the handy giveio kernel driver written by Dale Roberts, which I downloaded from 
 * http://mitglied.lycos.de/mgrafe/treiber.htm but it's dotted around on the net. (I haven't tried this under
 * Vista by the way.)
 *
 *
 * Returns   0, install error
 *         !=0, installed OK
 */
int EnableIO( void ) {
#if defined(_WIN32) || defined(__CYGWIN__)
    HANDLE h = NULL;
    OSVERSIONINFO osvi;
    osvi.dwOSVersionInfoSize = sizeof( OSVERSIONINFO );
    GetVersionEx( &osvi );
    if(osvi.dwPlatformId == VER_PLATFORM_WIN32_NT) {
        // OS = NT/2000/XP/Vista/7
        h = CreateFile("\\\\.\\giveio", GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE) {
            fprintf(stderr,"ERROR: Could NOT access giveio file. Please check if you have the giveio.sys driver installed and loaded \n");
            fprintf(stderr,"       or consider fetching and installing the drivers from online resources (e.g. from here: http://www.cs.ucr.edu/~eblock/pages/pictools/giveio.html\n");
            fprintf(stderr,"       or http://mitglied.multimania.de/mgrafe/treiber.htm)\n");
            fprintf(stderr,"       (Credits: Dale Roberts and Paula Tomlinson)\n");
            return 0;
        } else {
            usingGiveio=1;
	}
	if (h) CloseHandle(h);
    }
    return (int) h;
#else
    if (iopl(3) == 0) {
        // Success
        return 1;
    }
    return 0;
#endif
}
