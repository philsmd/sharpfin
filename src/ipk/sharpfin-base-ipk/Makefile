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

include ../../Rules.mak

all: nanddump/files/usr/sbin/nanddump files/bin/sharpfin
	cp nanddump/files/usr/sbin/nanddump files/usr/sbin/
	chmod -R 755 files
	../../../devtools/bin/ipkg-build -o root -g root files

clean:
	/bin/rm -f files/bin/sharpfin files/usr/sbin/nanddump *.ipk 
	/bin/rm -rf busybox/_install/
	(cd nanddump; make clean)
	(cd busybox; make clean)

files/bin/sharpfin: busybox/busybox
	cp busybox/busybox files/bin/sharpfin

busybox/busybox:
	(cd busybox; make CROSS_COMPILE=$(CROSS) ARCH=arm; make CROSS_COMPILE=$(CROSS) ARCH=arm install)

nanddump/files/usr/sbin/nanddump:
	(cd nanddump; make CROSS_COMPILE=$(CROSS) ARCH=arm)
