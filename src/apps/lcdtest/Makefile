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

BIN   	  := lcdtest
SRC 	  := lcdtest.c
CFLAGS    += -I$(PWD)/../../libreciva/include
LDFLAGS   += -L$(PWD)/../../libreciva -lreciva
VER	:=	"`cat debug/revision.txt | cut -d' ' -f2`"

include ../../Rules.mak

debugclean:
	rm -f debug/*~ *~ debug/lcdtest lcdtest*.debug lcdtest.tar lcdtest.tar.bz2

debug: lcdtest debug/debug-me
	make debugclean
	cp lcdtest debug
	tar cvf lcdtest.tar debug/*
	bzip2 lcdtest.tar
	mv lcdtest.tar.bz2 lcdtest$(VER).debug
