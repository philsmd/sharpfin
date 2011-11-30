/* 
 * Sharpfin project
 * Copyright (C) 2007 by Steve Clarke
 * 2011-11-30 Philipp Schmidt
 *   Added to github 
 *
 * This file is part of the sharpfin project
 * This program is very slow, as it uses the boundary scan path
 * to directly drive the nand flash. What it really needs to do
 * is to use mem-read and mem-write and make use of the ARM NAND
 * Flash Controller.
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
#include <string.h>
#include <stdlib.h>
#include "def.h"
#include "jtag.h"
#include "ppt.h"
#include "pin2410.h"
#include "k9fxx08.h"

typedef enum {
    GET_JTAG_IDS,
    FLASH_READ_MODE,
    FLASH_WRITE_MODE,
    BLOCK_CHECK_MODE
} eCommandMode;
eCommandMode mode = GET_JTAG_IDS;

U32 start_address = 0;
U32 length = 0;
char filename[ 256 ];

void usage() {
	printf("\n"
           "sharpflash [-p 1|2|3] [-r|-w filename start length ]\n"
	       "sharpflash [-p 1|2|3] [-b]\n\n"
	       "  -r -w      Read flash to file, or write file to flash\n"
           "  -b         Check flash for bad blocks\n"
	       "  -p <n>     n=1,2 or 3. Use LPT1 (default) LPT2 or LPT3 parallel port\n"
	       "  filename   Destination / source filename, the file must be in nanddump format\n"
	       "  start      Hex start address in NAND for read/write, must be a multiple of 0x4000\n"
	       "  length     Hex length to read/write. if file is too short,\n"
	       "             NAND will be filled with 0xFF. Must be a multiple of 0x4000\n\n"
           " If no r,w or b argument is supplied, the program just tries to read the\n"
           " JTAG device ID of the flash and CPU using the configured parallel port,\n\n"
	       " For r,w and b commands, the output data contains:\n\n"
	       "   w - page written OK\n"
	       "   r - page read OK\n"
	       "   . - page check OK\n"
	       "   b - page/block identified as bad\n"
	       "   B - page/block has just been marked bad\n\n"
	       "v%s, http://www.sharpfin.zevv.nl/\n\n", VER );
}

/*
 * Basic command line parsing. Doesn't catch all syntax erros but gets enough of them.
 */
void ParseCommandLine(int argc, char *argv[] ) {
    int paramnum = 1;
    int lptnumber;
    while (paramnum < argc) {
        if (strcmp(argv[ paramnum ], "-p") == 0) {
            if (paramnum+1>=argc) {
                printf("Missing printer port number\n");
                exit(1);
            }
            lptnumber = atoi(argv[ paramnum+1 ]);
            if (SetPrinterNumber(lptnumber)==0) {
                printf("\nInvalid printer port number!\n");
                usage();
                exit(1);
            }
            paramnum +=2;
        } else if (strcmp(argv[ paramnum ], "-b")==0) {
            mode = BLOCK_CHECK_MODE;
            paramnum++;
        } else if (strcmp(argv[ paramnum ], "-r")==0) {
            if ((paramnum+3) >= argc) {
                printf("Must specify filname, start and length with -r\n");
                exit(1);
            }
            if ((sscanf(argv[paramnum+1], "%s", filename ) != 1) ||
                (sscanf(argv[paramnum+2], "%X", &start_address)    != 1) ||
		        (sscanf(argv[paramnum+3], "%X", &length)      != 1)) {
                printf("Parameter error\n");
                usage();
                exit(1);
            }
            mode = FLASH_READ_MODE;
            paramnum += 4;
        } else if (strcmp(argv[ paramnum ], "-w")==0) {
            if ((paramnum+3) >= argc) {
                printf("Must specify a filname start and length with -w\n");
                exit(1);
            }
            if ((sscanf(argv[paramnum+1], "%s", filename ) != 1) ||
                (sscanf(argv[paramnum+2], "%X", &start_address) != 1) ||
		        (sscanf(argv[paramnum+3], "%X", &length) != 1)) {
                printf("Parameter error\n");
                usage();
                exit(1);
            }
            mode = FLASH_WRITE_MODE;
            paramnum += 4;
        } else if ((strcmp(argv[ paramnum ],"-h")==0) || (strcmp(argv[ paramnum ],"--help")==0) || (strcmp(argv[ paramnum ],"/?")==0)) {
            usage();
            exit(0);
        } else {
            printf("Unrecognised command line argument\n");
            exit(1);
        }
    }
}

int main(int argc, char *argv[]) {
    U32 cpu_id;
    U16 flash_id;

    // What are we
	printf("\nSharpfin Flash Programmer. http://www.sharpfin.zevv.nl/\n\n");
    ParseCommandLine(argc, argv);
    // Quick sanity check on any command line parameters
	if ( ((start_address/0x4000)*0x4000) != start_address ) {
		printf("Start address must be a multiple of 0x4000\n") ;
		return 1;
	}
	if ( ((length/0x4000)*0x4000) != length ) {
		printf("length must be a multiple of 0x4000\n") ;
		return 1;
	}
    // In both Windows and Linux we must enable access to the I/O ports
    if (EnableIO() == 0) {
    	printf("Unable to access I/O ports.\n");
        return 1;
    }
	// Configure the parallel port
    ConfigureParallelPort();
    // Connect to the Processor
	cpu_id = JTAG_ReadId();
	if (cpu_id != JTAG_ID_CPU) {
		printf("Unable to find S3C2410 processor on JTAG Cable, found %08X\n", cpu_id);
		return 0 ;
	}
	printf("Detected S3C2410 processor on JTAG Cable\n");

    S2410_InitCell();
	K9Fxx08_JtagInit();
	K9Fxx08_init();

    /*
     * Look for a known NAND Flash
     */
	flash_id = K9Fxx08_checkid();
	if (flash_id == JTAG_ID_K9F2808U0C) {
		printf("Found K9F2808UOC flash on processor, id=0x%04X\n", flash_id );
	} else if (flash_id == JTAG_ID_K9F5608U0B) {
		printf("Found K9F5608UOC flash on processor, id=0x%04X\n", flash_id );
	} else {
		printf("Unknown flash id on processor bus, found 0x%04X\n", flash_id );
        return 1;
	}

    // If that's all we need to do then exit
    if (mode == GET_JTAG_IDS)
        exit(0);

    if (mode == FLASH_READ_MODE) {
        // Read data
        K9Fxx08_read(filename, start_address, length );
    } else if (mode == FLASH_WRITE_MODE ) {
        // Write data
        K9Fxx08_write( filename, start_address, length );
    } else {
		// Check Bad Blocks
		K9Fxx08_badcheck( flash_id ) ;
    }
	printf("\n") ;
	return 0 ;
}
