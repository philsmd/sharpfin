# Sharpfin project
# Copyright (C) by Philipp Schmidt
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

include ../../../Rules.mak

all:
	mkdir -p files/usr/sbin
	$(CC) -o files/usr/sbin/nanddump nanddump.c	
	chmod -R 755 files
	../../../../devtools/bin/ipkg-build -o root -g root files
clean:
	/bin/rm -f files/usr/sbin/nanddump
	/bin/rm -f *.ipk
