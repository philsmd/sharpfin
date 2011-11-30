/* 
 * Sharpfin project
 * Copyright (C) 2002 by Jaewook Cheong
 * 2002-05-16 Jaewook Cheong
 *   first writing for S3C2410
 * 2011-11-30 Philipp Schmidt
 *   Added to github 
 *
 * This file is part of the sharpfin project
 * This program extracts the requested partition data from a Reciva mtd.bin
 * file, and saves the output in nanddump format, ready to be used with 
 * sharpflash.
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

#include <string.h>
#include "def.h"
#include "jtag.h"
/*
 * Holding TMS=1 and giving 5 rising clock edges guarantees to get you to the "Test-Logic Reset" state
 * from any other state in JTAG land.
 *
 */
void JTAG_Reset(void) {
	JTAG_SET( TDI_H | TMS_H | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_H | TCK_H ); JTAG_DELAY();

	JTAG_SET( TDI_H | TMS_H | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_H | TCK_H ); JTAG_DELAY();

	JTAG_SET( TDI_H | TMS_H | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_H | TCK_H ); JTAG_DELAY();

	JTAG_SET( TDI_H | TMS_H | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_H | TCK_H ); JTAG_DELAY();

	JTAG_SET( TDI_H | TMS_H | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_H | TCK_H ); JTAG_DELAY();

	JTAG_SET( TDI_H | TMS_H | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_H | TCK_H ); JTAG_DELAY();
}

void JTAG_RunTestldleState( void ) {
    // Go to Test-Logic-Reset state
    JTAG_Reset();

    // Go to Run Test-Idle state
    JTAG_SET( TDI_H | TMS_L | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_L | TCK_H ); JTAG_DELAY();

    // Shouldn't need these additional cycles ?!?!?!? (Makes absolutely sure perhaps?)
    JTAG_SET( TDI_H | TMS_L | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_L | TCK_H ); JTAG_DELAY();    // Run-Test/Idle Status
	JTAG_SET( TDI_H | TMS_L | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_L | TCK_H ); JTAG_DELAY();    // Run-Test/Idle Status

}

void  JTAG_ShiftIRState(char *wrIR) {
int size, i, tdi;
    /*
     * Go to Shift-IR from Run-Test-Idle
     */
	JTAG_SET( TDI_H | TMS_H | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_H | TCK_H ); JTAG_DELAY(); 	// Change to Select-DR-Scan

	JTAG_SET( TDI_H | TMS_H | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_H | TCK_H ); JTAG_DELAY(); 	// Change to Select-IR-Scan

	JTAG_SET( TDI_H | TMS_L | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_L | TCK_H ); JTAG_DELAY(); 	// Change to Capture-IR

	JTAG_SET( TDI_H | TMS_L | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_L | TCK_H ); JTAG_DELAY(); 	// Change to Change-IR

	size = strlen( wrIR );

	for( i=0 ; i<(size-1) ; i++)
	{
	    tdi = (wrIR[i] == HIGH) ? TDI_H : TDI_L;

        JTAG_SET( tdi | TMS_L | TCK_L ); JTAG_DELAY();
	    JTAG_SET( tdi | TMS_L | TCK_H ); JTAG_DELAY(); 	// Shift-IR
	}

    tdi = (wrIR[i] == HIGH) ? TDI_H : TDI_L; //i=3

    JTAG_SET( tdi   | TMS_H | TCK_L ); JTAG_DELAY();
	JTAG_SET( tdi   | TMS_H | TCK_H ); JTAG_DELAY(); 	// Change to Exit1-IR

	JTAG_SET( TDI_H | TMS_H | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_H | TCK_H ); JTAG_DELAY(); 	// Change to Update-IR

	JTAG_SET( TDI_H | TMS_L | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_L | TCK_H ); JTAG_DELAY(); 	// Change to Run-Test/Idle
}

void JTAG_ShiftDRState( char *wrDR, char *rdDR ) {
    int size, i, tdi;
	JTAG_SET( TDI_H | TMS_H | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_H | TCK_H ); JTAG_DELAY(); 	// Select-DR-Scan
	JTAG_SET( TDI_H | TMS_L | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_L | TCK_H ); JTAG_DELAY(); 	// Capture-DR
	JTAG_SET( TDI_H | TMS_L | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_L | TCK_H ); JTAG_DELAY(); 	// Shift-DR
	size = strlen( wrDR );
	for( i=0 ; i<(size-1) ; i++ ) {
	    tdi = (wrDR[i] == HIGH) ? TDI_H : TDI_L;
	    JTAG_SET( tdi | TMS_L | TCK_L ); JTAG_DELAY();
	    JTAG_SET( tdi | TMS_L | TCK_H ); JTAG_DELAY(); 	// Shift-DR
	    rdDR[i] = JTAG_GET_TDO();
	}

	tdi = (wrDR[i] == HIGH) ? TDI_H : TDI_L;	        // i = S3C2410_MAX_CELL_INDEX
	JTAG_SET( tdi | TMS_H | TCK_L ); JTAG_DELAY();
	JTAG_SET( tdi | TMS_H | TCK_H ); JTAG_DELAY(); 	    // Exit1-DR
	rdDR[i] = JTAG_GET_TDO();

	JTAG_SET( TDI_H | TMS_H | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_H | TCK_H ); JTAG_DELAY(); 	// Update-DR

	//Run-Test/Idle
	JTAG_SET( TDI_H | TMS_L | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_L | TCK_H ); JTAG_DELAY();    // Update-DR
}

void  JTAG_ShiftDRStateNoTDO(char *wrDR) {
    int size, i, tdi;
	JTAG_SET( TDI_H | TMS_H | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_H | TCK_H ); JTAG_DELAY(); 	// Select-DR-Scan
	JTAG_SET( TDI_H | TMS_L | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_L | TCK_H ); JTAG_DELAY(); 	// Capture-DR
	JTAG_SET( TDI_H | TMS_L | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_L | TCK_H ); JTAG_DELAY(); 	// Shift-DR
	size = strlen( wrDR );
	for( i=0 ; i<size-1 ; i++ ) {
	    tdi = (wrDR[i] == HIGH) ? TDI_H : TDI_L;
	    JTAG_SET( tdi | TMS_L | TCK_L ); JTAG_DELAY();
	    JTAG_SET( tdi | TMS_L | TCK_H ); JTAG_DELAY(); 	// Shift-DR
	    //rdDR[i]=JTAG_GET_TDO();
	}
	tdi = (wrDR[i]==HIGH) ? TDI_H:TDI_L;	            // i = S3C2410_MAX_CELL_INDEX
	JTAG_SET( tdi | TMS_H | TCK_L ); JTAG_DELAY();
	JTAG_SET( tdi | TMS_H | TCK_H ); JTAG_DELAY(); 	    // Exit1-DR
	//rdDR[i] = JTAG_GET_TDO();
	JTAG_SET( TDI_H | TMS_H | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_H | TCK_H ); JTAG_DELAY(); 	// Update-DR
	//Run-Test/Idle
	JTAG_SET( TDI_H | TMS_L | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_L | TCK_H ); JTAG_DELAY(); 	// Update-DR
}

U32 JTAG_ReadId(void) {
    int i;
    char id[32];
    U32 id32;
	JTAG_Reset();
	JTAG_SET( TDI_H | TMS_L | TCK_L ); JTAG_DELAY(); // Why 4 times? (tstst)
	JTAG_SET( TDI_H | TMS_L | TCK_H ); JTAG_DELAY(); // Run-Test/Idle Status
	JTAG_SET( TDI_H | TMS_L | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_L | TCK_H ); JTAG_DELAY(); // Run-Test/Idle Status
	JTAG_SET( TDI_H | TMS_L | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_L | TCK_H ); JTAG_DELAY(); // Run-Test/Idle Status
	JTAG_SET( TDI_H | TMS_L | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_L | TCK_H ); JTAG_DELAY(); // Run-Test/Idle Status
	JTAG_SET( TDI_H | TMS_H | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_H | TCK_H ); JTAG_DELAY(); // Select-DR Scan Status
	JTAG_SET( TDI_H | TMS_H | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_H | TCK_H ); JTAG_DELAY(); // Select-IR Scan Status
	JTAG_SET( TDI_H | TMS_L | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_L | TCK_H ); JTAG_DELAY(); // Capture-IR Status
	JTAG_SET( TDI_H | TMS_L | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_L | TCK_H ); JTAG_DELAY(); // Shift-IR Status

	// S3C2410 IDCODE Instruction "1110"
	JTAG_SET( TDI_L | TMS_L | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_L | TMS_L | TCK_H ); JTAG_DELAY(); // '0'
	JTAG_SET( TDI_H | TMS_L | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_L | TCK_H ); JTAG_DELAY(); // '1'
	JTAG_SET( TDI_H | TMS_L | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_L | TCK_H ); JTAG_DELAY(); // '1'
	JTAG_SET( TDI_H | TMS_H | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_H | TCK_H ); JTAG_DELAY(); // '1', //Exit1-IR
	JTAG_SET( TDI_H | TMS_H | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_H | TCK_H ); JTAG_DELAY(); // Update_IR
	JTAG_SET( TDI_H | TMS_H | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_H | TCK_H ); JTAG_DELAY(); // Select-DR-Scan
	JTAG_SET( TDI_H | TMS_L | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_L | TCK_H ); JTAG_DELAY(); // Capture-DR
	JTAG_SET( TDI_H | TMS_L | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_L | TCK_H ); JTAG_DELAY(); // Shift-DR

    // 	Read IDcode..
	for( i=0 ; i<=30 ; i++ ) {
	    JTAG_SET( TDI_H | TMS_L | TCK_L ); JTAG_DELAY();
	    JTAG_SET( TDI_H | TMS_L | TCK_H ); JTAG_DELAY(); //Shift-DR
	    id[i] = JTAG_GET_TDO();
	}
	JTAG_SET( TDI_H | TMS_H | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_H | TCK_H ); JTAG_DELAY(); // Exit1_DR
	id[i] = JTAG_GET_TDO();
	JTAG_SET( TDI_H | TMS_H | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_H | TCK_H ); JTAG_DELAY(); // Update_DR
	JTAG_SET( TDI_H | TMS_L | TCK_L ); JTAG_DELAY(); // Why 3 times?
	JTAG_SET( TDI_H | TMS_L | TCK_H ); JTAG_DELAY(); // Run-Test/Idle
	JTAG_SET( TDI_H | TMS_L | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_L | TCK_H ); JTAG_DELAY(); // Run-Test/Idle
	JTAG_SET( TDI_H | TMS_L | TCK_L ); JTAG_DELAY();
	JTAG_SET( TDI_H | TMS_L | TCK_H ); JTAG_DELAY(); // Run-Test/Idle

	id32 = 0;
	for( i=31 ; i>=0 ; i-- ) {
	    if( id[i] == HIGH )
		id32 |= (1<<i);
	}
	return id32;
}
