/* 
 * Sharpfin project
 * Copyright (C) 2002 by Jaewook Cheong
 * 2002-05-16 Jaewook Cheong
 *   first writing for S3C2410
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

#ifndef __JTAG_H__
#define __JTAG_H__
#include "ppt.h"
/*****************************************************************************\
 *	                  [[    JTAG PIN assignment    ]]                        *
 *****************************************************************************
 *                                                                           *
 * If like me you found a non-standard JTAG card on eBay, use the            *
 * CUSTOM_JTAG #define at build time and set your pin numbers here.          *
 *                                                                           *
 * Sample pinout                                                             *
 * ------------------------------------------------------------------------- *
 *  JTAG Pin          LPT Signal  DB25 LPT Pin #                             *
 * ------------------------------------------------------------------------- *
 *   TMS <------------ DATA0       (2)                                       *
 *   TCK <------------ DATA1       (3)                                       *
 *   TDI <------------ DATA2       (4)                                       *
 *   TDO ------------> STATUS      (11)                                      *
\*****************************************************************************/
#define b_TDO       4           /* bit 4 in printer STATUS register (SELECT) is connected to TDO on this JTAG board */
#if defined(CHAMELEON)
#define b_TCK		0
#define b_TDI		1
#define b_TMS		2
#elif defined(CUSTOM_JTAG)
#define b_TCK		1			/* bit 1 in printer DATA register is connected to TCK on this JTAG board */
#define b_TDI		2			/* bit 2 in printer DATA register is connected to TDI on this JTAG board */
#define b_TMS		0			/* bit 0 in printer DATA register is connected to TMS on this JTAG board */
#else                   /* Default to a WIGGLER pinout */
#define b_TCK		2
#define b_TDI		3
#define b_TMS		1
#endif
#define LOW 			 '0'
#define HIGH			 '1'

// Boundary scan chip IDs
#define     JTAG_ID_CPU                 0x0032409d
#define     JTAG_ID_K9F2808U0C          0xec73        /* 16 Mbyte x 8, 2.7-3.6 V Vcc = 128 Mbit */
#define     JTAG_ID_K9F5608U0B          0xec75        /* 32 Mbyte x 8, 2.7-3.6 V Vcc = 256 Mbit */

// Define some handy masks from the bit numbers
#define TMS_H		(1 << b_TMS)
#define TCK_H		(1 << b_TCK)
#define TDI_H		(1 << b_TDI)
#define TDO_H       (1 << b_TDO)
#define TCK_L		0
#define TDI_L       0
#define TMS_L		0

// JTAG Instruction opcodes for S3C2410
#define EXTEST		    "0000"  		/* LSB...MSB */
#define BYPASS		    "1111"
#define IDCODE		    "0111"
#define SAMPLE_PRELOAD	"1100"

/* 
 * This delay really is stunningly nasty! Calibrate a timing loop perhaps? It all depends
 * on the TAP timing requirements & how fast you can crank your CPU.
 */
INLINE static void JTAG_DELAY( void ) {int _d;for(_d=0 ; _d<1 ; _d++);}
INLINE static void JTAG_SET(U32 value)	{ OutputPpt(value); }
INLINE static char JTAG_GET_TDO() { return (InputPpt()&(1<<7)) ? LOW:HIGH; }

// Local function prototypes
void JTAG_Reset( void );
void JTAG_RunTestldleState( void );
U32  JTAG_ReadId( void );
void JTAG_ShiftIRState( char *wrIR );
void JTAG_ShiftDRState( char *wrDR, char *rdDR );
void JTAG_ShiftDRStateNoTDO( char *wrDR );
#endif
