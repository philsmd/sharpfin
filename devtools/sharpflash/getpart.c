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
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "def.h"
#include "nand_ecc.h"

int main(int argc, char *argv[]) {
	int x ;
	int fi, fo ;
	int start, len ;
	U8 buf[512], crc[16] ;

	if (argc!=5) {
		printf("\ngetpart source.mtd start len dest.bin\n\n") ;
		printf(" source.mtd   - Reciva flash image\n") ;
		printf(" start	      - Start position of partition (hex)\n") ;
		printf(" len	      - Length of partition (hex)\n") ;
		printf(" dest.bin     - Destination of partition\n") ;
		printf(" (start and len must be multiples of 4000(hex))\n\n") ;
		printf("v%s, http://www.sharpfin.zevv.nl/\n\n",VER) ;
		return 0 ;
	}

	fi=open(argv[1],O_RDONLY) ;
	if ( fi < 0 ) {
		printf("extractpartition: error opening %s\n", argv[1]) ;
		return 0 ;
	}

	sscanf(argv[2], "%X", &start) ;
	sscanf(argv[3], "%X", &len) ;

	if ((start%0x4000)!=0) {
		printf("extractpartition: start must be a multiple of 04000(hex)\n") ;
		return 0 ;
	}

	if ((len%0x4000)!=0) {
		printf("extractpartition: len must be a multiple of 04000(hex)\n") ;
		return 0 ;
	}

	fo=creat(argv[4],664) ;
	if ( fo < 0 ) {
		printf("extractpartition: error opening %s\n", argv[4]) ;
		return 0 ;
	}

	printf("\n") ;
	printf("Source File      = %s\n", argv[1]) ;
	printf("Start Address    = %07X\n", start) ;
	printf("Length	   = %07X\n", len) ;
	printf("Destination File = %s\n", argv[4]) ;
	printf("\n") ;

	lseek(fi, (off_t)start, SEEK_SET) ;
	for (x=0; x<len; x+=512) {
		memset(crc, 0xFF, 16) ;
		memset(buf, 0xFF, 512) ;
		read(fi, buf, 512) ;
		nand_calculate_ecc(buf, crc) ;
		write(fo, buf, 512) ;
		write(fo, crc, 16) ;
	}

	close(fi) ;
	close(fo) ;

	return 0 ;
}

