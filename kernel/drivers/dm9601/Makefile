# Sharpfin project
# Copyright (C) by Davicom Semiconductor Inc.  	For DM9601 V0.00
# 2000-09-05 Fixed SMPFALGS wrong on smp & smp_mod
# 2000-08-02 Changed some description string & include file path
# 2000-07-25 Append smp_mod and changed some descriptions
# 2000-01-25 by Sten Wang
# 2011-11-30 Philipp Schmidt
#   Added to github 
# 
# This file is part of the sharpfin project
#  
# This Library is free software: you can redistribute it and/or modify 
# it under the terms of the GNU General Public License as published by 
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#  
# You should have received a copy of the GNU General Public License
# along with this source files. If not, see
# <http://www.gnu.org/licenses/>.

CC	= arm-9tdmi-linux-gnu-gcc
CFLAGS  = -DMODULE -D__KERNEL__ -I../../linux_bast/include/linux -Wall -Wstrict-prototypes -O6 -c
MFLAGS  = -DMODVERSIONS
SMPFLAGS = -D__SMP__

##=============================================
## Default Compiler: make all
##=============================================
all:		org

##=============================================
## One processor and normal kernel: make org
##=============================================
org:	dm9601.c \
	Makefile
	$(CC) $(CFLAGS) dm9601.c

##=============================================
## Set version info. on all module symbol
##     : make mod
##=============================================
mod:	dm9601.c \
	Makefile
	$(CC) $(MFLAGS) $(CFLAGS) dm9601.c

##=============================================
## Symmetric Multi Processor(SMP)
##     : make smp
##=============================================
smp:	dm9601.c \
	Makefile
	$(CC) $(SMPFLAGS) $(CFLAGS) dm9601.c

##=============================================
## SMP & Set all version info. on all module symbols
##     : make smp_mod
##=============================================
smp_mod:dm9601.c \
	Makefile
	$(CC) $(SMPFLAGS) $(MFLAGS) $(CFLAGS) dm9601.c

clean:			
	rm *o
