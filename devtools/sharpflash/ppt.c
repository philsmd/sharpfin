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

#if defined( _WIN32)        // Visual C build
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#elif defined(__CYGWIN__)   // Cygwin build
#include <windows.h>
#include <sys/io.h>
#else                       // Linux build
#include <sys/io.h>
#include <unistd.h>
#endif

#include "def.h"
#include "ppt.h"

// Printer port and bit definitions
#define ECP_ECR_OFFSET      0x402       // Looks wrong but it's actually right
#define ECR_STANDARD	    0x00
#define ECR_DISnERRORINT    0x10
#define ECR_DISDMA	        0x00
#define ECR_DISSVCINT	    0x04

// Default to LPT1 unless over-ridden on the commandline
static U16 base_address = LPT1;
// Permitted printer ports
static U16 printer_ports[] = { LPT1, LPT2, LPT3 };

void OutputPpt(U8 value) {
#if !defined(_WIN32) || defined(__CYGWIN__)
    outb( value & 0xff, base_address );
#else
    _outp( base_address, value & 0xff );
#endif
}

U8 InputPpt( void ) {
#if !defined(_WIN32) || defined(__CYGWIN__)
    return inb( (U16) (base_address + 1) );
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

// Just put the port into standard parallel mode
void ConfigureParallelPort(void) {
    // Configure the parallel port in standard mode
#if !defined(_WIN32) || defined(__CYGWIN__)
	outb( (ECR_STANDARD | ECR_DISnERRORINT | ECR_DISDMA | ECR_DISSVCINT), (base_address + ECP_ECR_OFFSET) );
#else
	_outp( (U16)(base_address + ECP_ECR_OFFSET), (ECR_STANDARD | ECR_DISnERRORINT | ECR_DISDMA | ECR_DISSVCINT) );
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
	    // OS = NT/2000/XP
	    h = CreateFile("\\\\.\\giveio", GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE)
            return 0;
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
