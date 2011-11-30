/* 
 * Sharpfin project
 * Copyright (C) 2002 by Jaewook Cheong
 * Modified version by Steve Clarke
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

#ifndef __K9Fxx08_H__
#define __K9Fxx08_H__

// Return codes from our (bad) block checking routines
#define BAD_BLOCK_OK       0
#define BAD_BLOCK_ERASE    1
#define BAD_BLOCK_FACTORY  2
#define BAD_BLOCK_WRITE    3

// NAND Flash Command Bytes
//
#define	SEQ_DATA_INPUT		(0x80)
#define	READ_ID				(0x90)
#define	RESET				(0xFF)
#define	READ_1_1			(0x00)
#define	READ_1_2			(0x01)
#define	READ_2				(0x50)
#define	PAGE_PROGRAM		(0x10)
#define	BLOCK_ERASE			(0x60)
#define	BLOCK_ERASE_CONFIRM	(0xD0)
#define	READ_STATUS			(0x70)

void K9Fxx08_JtagInit();
void K9Fxx08_badcheck( U16 type);
void K9Fxx08_write( char *file, U32 start, U32 len);
void K9Fxx08_read( char *file, U32 start, U32 len );
U16  K9Fxx08_checkid() ;
void K9Fxx08_init() ;

#endif //__K9Fxx08_H__
