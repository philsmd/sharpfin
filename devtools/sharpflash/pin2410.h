/* 
 * Sharpfin project
 * 2002-05-16 Copyright (C) Jaewook Cheong
 *   first writing for S3C2410
 * 2007 Steve Clarke
 *   Modified version by Steve Clarke
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

#ifndef __PIN2410_H__
#define __PIN2410_H__
// Boundary Scan Cell number of S3C2410
#define S2410_MAX_CELL_INDEX	426	//0~426
#define DATA0_7_CON	    (99)
#define DATA0_IN	    (100)
#define DATA0_OUT	    (98)
#define DATA1_IN	    (97)
#define DATA1_OUT	    (96)
#define DATA2_IN	    (95)
#define DATA2_OUT	    (94)
#define DATA3_IN	    (93)
#define DATA3_OUT	    (92)
#define DATA4_IN	    (91)
#define DATA4_OUT	    (90)
#define DATA5_IN	    (89)
#define DATA5_OUT	    (88)
#define DATA6_IN	    (87)
#define DATA6_OUT	    (86)
#define DATA7_IN	    (85)
#define DATA7_OUT	    (84)

#define DATA8_15_CON    (82)
#define DATA8_IN	    (83)
#define DATA8_OUT	    (81)
#define DATA9_IN	    (80)
#define DATA9_OUT	    (79)
#define DATA10_IN	    (78)
#define DATA10_OUT	    (77)
#define DATA11_IN	    (76)
#define DATA11_OUT	    (75)
#define DATA12_IN	    (74)
#define DATA12_OUT	    (73)
#define DATA13_IN	    (72)
#define DATA13_OUT	    (71)
#define DATA14_IN	    (70)
#define DATA14_OUT	    (69)
#define DATA15_IN	    (68)
#define DATA15_OUT	    (67)

#define DATA16_23_CON	(65)
#define DATA16_IN	    (66)
#define DATA16_OUT	    (64)
#define DATA17_IN	    (63)
#define DATA17_OUT	    (62)
#define DATA18_IN	    (61)
#define DATA18_OUT	    (60)
#define DATA19_IN	    (59)
#define DATA19_OUT	    (58)
#define DATA20_IN	    (57)
#define DATA20_OUT	    (56)
#define DATA21_IN	    (55)
#define DATA21_OUT	    (54)
#define DATA22_IN	    (53)
#define DATA22_OUT	    (52)
#define DATA23_IN	    (51)
#define DATA23_OUT	    (50)

#define DATA24_31_CON	(48)
#define DATA24_IN	    (49)
#define DATA24_OUT	    (47)
#define DATA25_IN	    (46)
#define DATA25_OUT	    (45)
#define DATA26_IN	    (44)
#define DATA26_OUT	    (43)
#define DATA27_IN	    (42)
#define DATA27_OUT	    (41)
#define DATA28_IN	    (40)
#define DATA28_OUT	    (39)
#define DATA29_IN	    (38)
#define DATA29_OUT	    (37)
#define DATA30_IN	    (36)
#define DATA30_OUT	    (35)
#define DATA31_IN	    (34)
#define DATA31_OUT	    (33)

#define ADDR0_CON	    (140)
#define ADDR0	        (139)
#define ADDR1_15_CON	(138)
#define ADDR1	        (137)
#define ADDR2	        (136)
#define ADDR3	        (135)
#define ADDR4	        (134)
#define ADDR5	        (133)
#define ADDR6	        (132)
#define ADDR7	        (131)
#define ADDR8	        (130)
#define ADDR9	        (129)
#define ADDR10	        (128)
#define ADDR11	        (127)
#define ADDR12	        (126)
#define ADDR13	        (125)
#define ADDR14	        (124)
#define ADDR15	        (123)
#define ADDR16_CON      (122)
#define ADDR16		    (121)
#define ADDR17_CON      (120)
#define ADDR17		    (119)
#define ADDR18_CON      (118)
#define ADDR18		    (117)
#define ADDR19_CON      (116)
#define ADDR19	        (115)
#define ADDR20_CON      (114)
#define ADDR20          (113)
#define ADDR21_CON      (112)
#define ADDR21	        (111)
#define ADDR22_CON      (110)
#define ADDR22		    (109)
#define ADDR23_CON      (108)
#define ADDR23		    (107)
#define ADDR24_CON      (106)
#define ADDR24		    (105)
#define ADDR25_CON      (104)
#define ADDR25		    (103)
#define ADDR26_CON      (102)
#define ADDR26		    (101)


#define CLE		        (168)
#define ALE		        (169)

#define nFWE		    (170)
#define nFRE		    (171)
#define nFCE		    (172)

#define nWE		        (148)
#define nOE		        (147)
#define nBE0		    (146)
#define nBE1		    (145)
#define nBE2		    (144)
#define nBE3		    (143)
#define nSRAS		    (142)
#define nSCAS		    (141)

#define NCON0		    (229)
#define nWAIT		    (167)

#define nGCS67_CON	    (166)
#define nGCS7		    (165)
#define nGCS6		    (164)
#define nGCS5_CON	    (163)
#define nGCS5	        (162)
#define nGCS4_CON	    (161)
#define nGCS4	        (160)
#define nGCS3_CON	    (159)
#define nGCS3	        (158)
#define nGCS2_CON	    (157)
#define nGCS2	        (156)
#define nGCS1_CON	    (155)
#define nGCS1	        (154)
#define nGCS0_ETC_CON	(153)   //nGCS0,nWE,nOE,nBEn,nSRAS,nSCAS
#define nGCS0	        (152)

/*****************************************************************************/
/* Exported Functions                                                        */
/*****************************************************************************/

void S2410_InitCell(void);
void S2410_SetPin(int index, char value);
char S2410_GetPin(int index);
void S2410_SetDataByte( U8 );
void S2410_SetDataHW( U16 );
void S2410_SetDataWord( U32 );
U8  S2410_GetDataByte( void );
U16 S2410_GetDataHW( void );
U32 S2410_GetDataWord( void );
void S2410_SetAddr(U32 addr);
extern char outCellValue[ S2410_MAX_CELL_INDEX + 2 ];
extern char  inCellValue[ S2410_MAX_CELL_INDEX + 2 ];
extern int  dataOutCellIndex[32];
extern int  dataInCellIndex[32];
extern int  addrCellIndex[27];

// MACRO for speed up
//#define S2410_SetPin(index,value)   outCellValue[index] = value
//#define S2410_GetPin(index)	    inCellValue[index]
#endif  //__PIN2410_H__
