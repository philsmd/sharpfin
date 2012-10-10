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

#ifndef __PPT_H__
#define __PPT_H__
#define LPT1 0x378
#define LPT2 0x278
#define LPT3 0x3bc

void ConfigureParallelPort( void );
int EnableIO( void );
int SetPrinterNumber( int );
int GetPrinterIndex( int );
void OutputPpt(U8 value);
U8 InputPpt( void );
#endif //__PPT_H__
