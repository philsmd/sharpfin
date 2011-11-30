# Sharpfin project
# Copyright (C) by Steve Clarke and Ico Doornekamp
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

VER:=\"0.3\"
CC:=gcc -Wall
OBJS:=ppt.o jtag.o pin2410.o k9fxx08.o sharpflash.o
EPOBS:=nand_ecc.o getpart.o

CFLAGS:=-DLINUX_PPDEV
#CFLAGS:=-DCHAMELEON -D__WINDOWS__ 

all: sharpflash getpart

getpart: $(EPOBS)
	$(CC) $(LDFLAGS) -o $@ $^

sharpflash: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -g -o $@ -c $^ -DVER=$(VER)

clean:
	rm -f sharpflash getpart *~ $(OBJS) $(EPOBS)
