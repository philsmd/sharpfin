/* 
 * Sharpfin project
 * Original Copyright (C) 2007 by Robert Fitzsimons, robfitz at 273k.net
 * Modifications/Porting to K9Fxx08UOC Copyright (C) Steve Clarke
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
#include <time.h>
#include <string.h>
#include "def.h"
#include "k9fxx08.h"
#include "pin2410.h"
#include "jtag.h"
#define ECC_CHECK	(0)

//*************** JTAG dependent functions ***************
static void NF_CMD(U8 cmd);
static void NF_ADDR(U32 addr);
static void NF_nFCE_L(void);
static void NF_nFCE_H(void);
static U8   NF_RDDATA(void);
static void NF_WRDATA(U8 data);
static void NF_WAITRB(void);

//*************** H/W dependent functions ***************
static U16  NF_CheckId(void);
static int  NF_EraseBlock(U32 blockNum);
static int  NF_ReadPage(U32 block,U32 page,U8 *buffer,U8 *spareBuf);
static int  NF_WritePage(U32 block,U32 page,U8 *buffer,U8 *spareBuf);
static int  NF_IsBadBlock(U32 block);
static int  NF_MarkBadBlock(U32 block);
static void NF_Reset(void);
static void NF_Init(void);
//*******************************************************

#define     DELAY_CONST     1
static void Delay(int del) {
    int i;
	for (i=0 ; i<10000*del ; i++);
}

void K9Fxx08_init() {
	NF_Init() ;
}

U16 K9Fxx08_checkid() {
	return NF_CheckId() ;
}

void K9Fxx08_badcheck(U16 type) {
    int i,badcount=0,bad,numBlocks;
	if (type == JTAG_ID_K9F2808U0C) numBlocks = 1024;
	if (type == JTAG_ID_K9F5608U0B) numBlocks = 2048;
	printf("Checking NAND Blocks") ;
	for (i=0 ; i<numBlocks ; i++) {
		if (i%64 == 0)
            printf("\n %08X: ", i*0x4000) ;
		bad = NF_IsBadBlock( i );
		if (bad != 0xFF) {
			printf("b");
			badcount++ ;
		} else {
			printf(".");
		}
        fflush(stdout);
    }
	printf("\nTotal Bad Blocks: %d\n", badcount) ;
}

void K9Fxx08_write( char *filename, U32 start, U32 len ) {
    int i, blockwriteerror, bad;
    U32 sourceBlock, numBlocks;
    U32 block, blockcount;
    U8 src[0x4200];
    FILE *fi;
    time_t rawtime, starttime;
    float elapsed, percent, mins ;

	// Calculate blocks
	sourceBlock = start / 0x4000 ;
	numBlocks = len / 0x4000 ;
	block=sourceBlock ;
	printf("Writing NAND Flash:\n") ;
	printf(" source	     = %s\n", filename) ;
	printf(" start addr  = 0x%X\n", start) ;
	printf(" length	     = 0x%X\n", len) ;
	printf("\n") ;
	fi = fopen(filename, "rb");
	if (fi == NULL) {
		printf("error opening file %s\n", filename) ;
		return ;
	}
	printf("\n"
	       "Address  Progress			   Remaining\n"
	       "-------  --------------------------------  ---------\n") ;
	time(&starttime) ;
	blockcount=0 ;
	while (blockcount<numBlocks) {
		printf("%07X  ", block*0x4000) ;
		fflush(stdout) ;
		// Erase the Block
		bad = NF_EraseBlock(block) ;
		switch (bad) {
		    case BAD_BLOCK_OK:
			    memset(src, 0, 0x4200) ;
			    if (fread(src, 0x4200, 1, fi) != 1)
                    printf("fread() error\n");
			    blockwriteerror=0 ;
			    // Fill It
			    for (i=0; i<32; i++) {
				    if (NF_WritePage(block, i, &src[i*528], &src[i*528+512])!=BAD_BLOCK_OK) {
					    // Error Writing
					    printf("B") ;
					    blockwriteerror=1 ;
				    } else {
					    // Write OK
					    printf("w") ;
				    }
				    fflush(stdout) ;
			    }
			    if (blockwriteerror==0) {
				    blockcount++ ;
			    }
			    break ;
		    case BAD_BLOCK_FACTORY:
			    // The block is bad
			    printf("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb") ;
			    break ;
		    case BAD_BLOCK_ERASE:
			    // The block has just become bad
			    printf("BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB") ;
			    break ;
		}
		// Print progress
		time ( &rawtime );
		elapsed = (float)(rawtime - starttime) ;
		percent = (float) ((block-sourceBlock+1)*100)/numBlocks ;
		mins = ( (100*elapsed)/percent - elapsed ) / 60 ;
		printf("   %2dh %02dm\n", ((U32)mins/60), ((U32)mins%60)) ;
		fflush(stdout) ;
		// Next Destination Block
		block++ ;
	}
	// Close File
	fclose( fi ) ;
}

void K9Fxx08_read(char *file, U32 start, U32 len) {
    int sourceBlock,numBlocks;
    FILE *df, *of;
    int block, blockcount, page;
    unsigned char buffer[512];
    unsigned char extra[16] ;
    time_t rawtime, starttime ;
    float elapsed, percent, mins ;
	sourceBlock = start/0x4000 ;
	numBlocks = len/0x4000 ;
	printf("Reading NAND Flash:\n") ;
	printf(" destination   = %s\n", file) ;
	printf(" raw data      = %s\n", "rawdata.bin") ;
	printf(" start addr    = 0x%08X\n", start) ;
	printf(" length	       = 0x%08X\n", len) ;
	printf(" num blocks    = 0x%04X\n", numBlocks) ;
	printf("\n") ;
	of = fopen("rawdata.bin", "wb");
	df = fopen(file, "wb");
	if ((of==NULL) || (df==NULL)) {
		printf("error opening output file(s)");
		return;
	}
	printf("\n") ;
	printf("Address  Progress			   Remaining\n") ;
	printf("-------  --------------------------------  ---------\n") ;

	time(&starttime) ;
	for (blockcount=0, block = sourceBlock; blockcount < numBlocks; block++, blockcount++) {
		printf("%07X  ", block*0x4000) ;
		fflush(stdout) ;
		for (page = 0; page < 32; page++) {
			NF_ReadPage(block, page, buffer, extra);
			fwrite(buffer, 512, 1, of) ;
			fwrite(extra, 16, 1, of) ;
			if (extra[5]==0xFF) {
				printf("r");
				fwrite(buffer, 512, 1, df) ;
				fwrite(extra, 16, 1, df) ;
			} else {
				printf("b") ;
			}
			fflush(stdout);
		}
		// Print Progress
		time ( &rawtime );
		elapsed = (float)(rawtime-starttime);
		percent = (float) ((block-sourceBlock+1)*100)/numBlocks ;
		mins = ( (100*elapsed)/percent - elapsed ) / 60 ;

        if (mins>999.0) mins=999.0;
        printf("   %2dh %02dm\n", ((U32)mins/60), ((U32)mins%60)) ;

		fflush(stdout) ;
		block++ ;
	}
	printf("\n");
	fclose( of );
	fclose( df );
}

/*
 * H/W dependent functions
 */
// block0: reserved for boot strap
// block1~1025: used for OS image
// badblock SE: xx xx xx xx xx 00 ....
// good block SE: ECC0 ECC1 ECC2 FF FF FF ....

#define WRITEVERIFY  (0)  //verifing is enable at writing.
/*
#define NF_CMD(cmd)	{rNFCMD=cmd;}
#define NF_ADDR(addr)	{rNFADDR=addr;}
#define NF_nFCE_L()	{rNFCONF&=~(1<<11);}
#define NF_nFCE_H()	{rNFCONF|=(1<<11);}
#define NF_RSTECC()	{rNFCONF|=(1<<12);}
#define NF_RDDATA() 	(rNFDATA)
#define NF_WRDATA(data) {rNFDATA=data;}
#define NF_WAITRB()	{while(!(rNFSTAT&(1<<0)));}
		//wait tWB and check F_RNB pin.
*/
#define ID_K9Fxx08UOC	0xec75
static U8 seBuf[16]={0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};
// 1block=(512+16)bytes x 32pages
// 1024block
// A[23:14][13:9]
//  block   page

static int NF_EraseBlock(U32 block) {
	U32 blockPage=(block<<5);
	int bad ;
	bad=NF_IsBadBlock(block) ;
	if (block!=0 && bad!=0xFF) {
		return BAD_BLOCK_FACTORY ;
	}

	NF_nFCE_L();
	NF_CMD(0x60);   		// Erase one block 1st command
	NF_ADDR(blockPage&0xff);	// Page number=0
	NF_ADDR((blockPage>>8)&0x7f);
	NF_CMD(0xd0);  			// Erase one blcok 2nd command
	Delay(DELAY_CONST); 			//wait tWB(100ns)
	NF_WAITRB();			// Wait tBERS max 3ms.
	NF_CMD(0x70);   		// Read status command

	if (NF_RDDATA()&0x1) {
		NF_nFCE_H() ;
		NF_MarkBadBlock(block);
		return BAD_BLOCK_ERASE;
	} else {
		NF_nFCE_H() ;
		return BAD_BLOCK_OK;
	}
}

static int NF_IsBadBlock(U32 block) {
unsigned int blockPage;
U8 data1, data2;
/*
 * The block status is defined by the 6th byte in the spare area. Samsung makes sure that either the 1st or 2nd page of
 * every initial invalid block has non-FFh data at the column address of 517. So we need to check this offset5 in both
 * pages. If either byte is not equal to 0xFF the block is bad.
 */
	blockPage=(block<<5);           // For 2'nd cycle I/O[7:5]
	NF_nFCE_L();
	NF_CMD(0x50);			        // Spare array read command
	NF_ADDR(517&0xf);		        // Read the mark of bad block in spare array(M addr=5)
	NF_ADDR(blockPage&0xff);	    // The mark of bad block is in 0 page
	NF_ADDR((blockPage>>8)&0x7f);   // For block number A[23:17]
	Delay(DELAY_CONST);			            // wait tWB(100ns)
	NF_WAITRB();			        // Wait tR(max 12us)
	data1=NF_RDDATA();
	NF_nFCE_H();

	NF_nFCE_L();
	NF_CMD(0x50);			        // Spare array read command
	NF_ADDR(517&0xf);		        // Read the mark of bad block in spare array(M addr=5)
	NF_ADDR((blockPage&0xff)+1);	// The mark of bad block is in 1 page
	NF_ADDR((blockPage>>8)&0x7f);   // For block number A[23:17]
	Delay(DELAY_CONST);		                // wait tWB(100ns)
	NF_WAITRB();			        // Wait tR(max 12us)
	data2=NF_RDDATA();
	NF_nFCE_H();

    return (data1 != 0xFF) ? data1 : data2;
}

static int NF_MarkBadBlock(U32 block) {
    int i;
    U32 blockPage=(block<<5);
	seBuf[0]=0xff;
	seBuf[1]=0xff;
	seBuf[2]=0xff;
	seBuf[5]=0x44;  		 // Bad blcok mark=0

	NF_nFCE_L();
	NF_CMD(0x50);
	NF_CMD(0x80);   		// Write 1st command

	NF_ADDR(0x0);			// The mark of bad block is
	NF_ADDR(blockPage&0xff);	// marked 5th spare array
	NF_ADDR((blockPage>>8)&0x7f);   // in the 1st page.

	for(i=0;i<16;i++) {
		NF_WRDATA(seBuf[i]);	// Write spare array
	}
	NF_CMD(0x10);   		// Write 2nd command
	Delay(DELAY_CONST);  			//tWB = 100ns.
	NF_WAITRB();	  		// Wait tPROG(200~500us)
	NF_CMD(0x70);
	Delay(DELAY_CONST);	 		//twhr=60ns//
	NF_nFCE_H() ;
	return 1;
}

static int NF_ReadPage(U32 block,U32 page,U8 *buffer,U8 *spareBuf) {
	int i;
	unsigned int blockPage;
	U8 *bufPt=buffer;
	int ret ;
	page=page&0x1f;
	blockPage=(block<<5)+page;
	NF_nFCE_L();
	NF_CMD(0x00);   		// Read command
	NF_ADDR(0);			// Column = 0
	NF_ADDR(blockPage&0xff);	//
	NF_ADDR((blockPage>>8)&0x7f);   // Block & Page num.
	Delay(DELAY_CONST);			//wait tWB(100ns)/////??????
	NF_WAITRB();			// Wait tR(max 12us)

	for(i=0;i<(512);i++) {
		*bufPt++=NF_RDDATA();	// Read one page
	}
	if(spareBuf!=NULL) {
		for(i=0;i<16;i++) spareBuf[i]=NF_RDDATA(); // Read spare array
		ret=spareBuf[5] ;
	} else {
		NF_CMD(0x50);   			// Read
		NF_ADDR(5);   				// Column = 517 (Spare)
		NF_ADDR(blockPage&0xff);		//
		NF_ADDR((blockPage>>8)&0x7f);   	// Block & Page num.
		Delay(DELAY_CONST);	   			//wait tWB(100ns)/////??????
		NF_WAITRB();				// Wait tR(max 12us)
		ret=NF_RDDATA() ;
	}
	NF_nFCE_H();
	return ret;
}

static int NF_WritePage(U32 block,U32 page,U8 *buffer,U8 *spareBuf)
{
	int i;
	U32 blockPage=(block<<5)+page;
	U8 *bufPt=buffer;

	NF_nFCE_L();
	NF_CMD(0x0);
	NF_CMD(0x80);			// Write 1st command
	NF_ADDR(0);			// Column 0
	NF_ADDR(blockPage&0xff);	//
	NF_ADDR((blockPage>>8)&0x7f);   // Block & page num.

	for(i=0;i<512;i++) {
		NF_WRDATA(*bufPt++);	// Write one page to NFM from buffer
	}

	if(spareBuf!=NULL) {
		for(i=0;i<16;i++) {
			NF_WRDATA(spareBuf[i]);	// Write spare array(ECC and Mark)
		}
	}

	NF_CMD(0x10);   	// Write 2nd command
	Delay(DELAY_CONST);		//tWB = 100ns.
	NF_WAITRB();		//wait tPROG 200~500us;
	NF_CMD(0x70);   	// Read status command
	Delay(DELAY_CONST);		//twhr=60ns

	if (NF_RDDATA()&0x1) { // Page write error
		NF_nFCE_H();
		NF_MarkBadBlock(block);
		return BAD_BLOCK_WRITE;
	} else {
		NF_nFCE_H();
		return BAD_BLOCK_OK;
	}
}



static U16 NF_CheckId(void)
{
	U16 id;

	NF_nFCE_L();

	NF_CMD(0x90);
	NF_ADDR(0x0);

	Delay(DELAY_CONST);		//wait tWB(100ns)

	id=NF_RDDATA()<<8;	// Maker code(K9Fxx08UOC:0xec)
	id|=NF_RDDATA();	// Devide code(K9Fxx08UOC:0x75)

	NF_nFCE_H();
	return id;
}


static void NF_Reset(void)
{
	NF_nFCE_L();

	NF_CMD(0xFF);   	//reset command

	Delay(DELAY_CONST);		    //tWB = 100ns.

	NF_WAITRB();		//wait 200~500us;

	NF_nFCE_H();
}


static void NF_Init(void)
{
	NF_Reset();

	//NF_nFCE_L();
	NF_CMD(READ_1_1);
	//NF_nFCE_H();
}



//*************************************************
//*************************************************
//**	 JTAG dependent primitive functions	  **
//*************************************************
//*************************************************

void K9Fxx08_JtagInit(void)
{
	JTAG_RunTestldleState();
	JTAG_ShiftIRState( EXTEST );

	S2410_SetPin(CLE, LOW);
	S2410_SetPin(ALE, LOW);
}



static void NF_CMD(U8 cmd)
{

	//Command Latch Cycle
	S2410_SetPin( DATA0_7_CON, LOW );      // D[7:0]=output
	S2410_SetPin( nFCE, LOW );
	S2410_SetPin( nFRE, HIGH );
	S2410_SetPin( nFWE, LOW );             // Because tCLS=0, CLE & nFWE can be changed simultaneously.
	S2410_SetPin( ALE, LOW );
	S2410_SetPin( CLE, HIGH );

    S2410_SetDataByte( cmd );
	JTAG_ShiftDRStateNoTDO( outCellValue );

	S2410_SetPin(nFWE,HIGH);
	JTAG_ShiftDRStateNoTDO( outCellValue );

#if 1
	S2410_SetPin(CLE,LOW);
	S2410_SetPin(DATA0_7_CON,HIGH);        // D[7:0]=input
	JTAG_ShiftDRStateNoTDO( outCellValue );
#endif
}


static void NF_ADDR(U32 addr)
{
U8 b = (U8) addr & 0xff;


    //rNFADDR=addr;
	S2410_SetPin(DATA0_7_CON ,LOW); //D[7:0]=output
	S2410_SetPin(nFCE,LOW);
	S2410_SetPin(nFRE,HIGH);
	S2410_SetPin(nFWE,LOW);
	S2410_SetPin(ALE,HIGH);
	S2410_SetPin(CLE,LOW);
	S2410_SetDataByte( b );
	JTAG_ShiftDRStateNoTDO(outCellValue);

	S2410_SetPin(nFWE,HIGH);
	JTAG_ShiftDRStateNoTDO(outCellValue);

#if 1
	S2410_SetPin(ALE,LOW);
	S2410_SetPin(DATA0_7_CON,HIGH); //D[7:0]=input
	JTAG_ShiftDRStateNoTDO(outCellValue);
#endif
}


static void NF_nFCE_L(void) {
	S2410_SetPin( nFCE,LOW );
	JTAG_ShiftDRStateNoTDO( outCellValue );
}


static void NF_nFCE_H(void) {
	S2410_SetPin( nFCE,HIGH );
	JTAG_ShiftDRStateNoTDO( outCellValue );
}


static U8 NF_RDDATA(void) {
	S2410_SetPin( DATA0_7_CON ,HIGH );                  //D[7:0]=input
	S2410_SetPin( nFRE, LOW );
	JTAG_ShiftDRStateNoTDO( outCellValue );

	S2410_SetPin( nFRE, HIGH );
	JTAG_ShiftDRState( outCellValue, inCellValue);

    return S2410_GetDataByte();
}

static void NF_WRDATA(U8 data) {
	S2410_SetPin(DATA0_7_CON ,LOW); //D[7:0]=output
	S2410_SetPin(nFWE,LOW);
	S2410_SetDataByte(data);
	JTAG_ShiftDRStateNoTDO(outCellValue);

	S2410_SetPin(nFWE,HIGH);
	JTAG_ShiftDRStateNoTDO(outCellValue);
}

static void NF_WAITRB(void) {
char state_nWAIT;
char state_NCON0;
    while (1) {
		JTAG_ShiftDRState(outCellValue,inCellValue);
        state_nWAIT = S2410_GetPin(nWAIT);
        state_NCON0 = S2410_GetPin(NCON0);
        if( (state_nWAIT==HIGH) && (state_NCON0==HIGH))
            break;
    }
}
