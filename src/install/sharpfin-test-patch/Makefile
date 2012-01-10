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

sharpfin-test-patch.zip: sharpfin-test.patch
	cp patch/readme.txt sharpfin-test.patch.txt
	zip sharpfin-test-patch.zip sharpfin-test.patch sharpfin-test.patch.txt
	rm sharpfin-test.patch.txt

sharpfin-test.patch: patch/install-me patch/lcdprint patch/readme.txt patch/lcdprint
	chmod -R 755 patch
	tar cf patch.tar patch/*
	bzip2 patch.tar
	mv patch.tar.bz2 sharpfin-test.patch

patch/lcdprint: 
	(cd ../../apps/lcdprint/;make)
	cp -f ../../apps/lcdprint/lcdprint patch/lcdprint
	
clean:
	/bin/rm -f *~ */*~ tar.tar tar.tar.bz2 sharpfin-test.patch sharpfin-test-patch.zip patch/lcdprint
