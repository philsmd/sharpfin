/* 
 * Sharpfin project
 * Copyright (C) 2000-2004 Steven J. Hill <sjhill@realitydiluted.com>
 *   Toshiba America Electronics Components, Inc.
 * 2007 Copyright (C) by Steve Clarke
 * 2011-11-30 Philipp Schmidt
 *   Added to github 
 *
 * This file is part of the sharpfin project
 * S3C2410 NAND Flash ECC Calculator
 *  nand_ecc contains an ECC algorithm, heavily based on the 256 byte
 *  algorithm developed by Steven J Hill, Toshiba.
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

#include "def.h"
int nand_calculate_ecc(const U8 *dat, U8 *ecc_code) ;
