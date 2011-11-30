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

VER := 0.1
APP := uninstall
INS := sharpfin-$(APP)_$(VER).patch

all: $(INS).zip

$(INS): patch/$(IPK) patch/install-me patch/readme.txt patch/lcdprint
	/bin/rm -f *~ patch/*~
	chmod -R 755 patch
	tar cf tar.tar patch/*
	bzip2 tar.tar
	mv tar.tar.bz2 $(INS)

$(INS).zip: $(INS)
	rm -rf radiopatch $(INS).zip
	mkdir radiopatch
	cp patch/readme.txt radiopatch/$(INS).txt
	cp $(INS) radiopatch
	zip $(INS).zip radiopatch/*
	rm -rf radiopatch

patch/lcdprint: ../../apps/lcdprint/lcdprint
	cp -f ../../apps/lcdprint/lcdprint patch/lcdprint

clean:
	/bin/rm -f *~ */*~ tar.tar tar.tar.bz2 sharpfin-$(APP)_*.patch sharpfin-$(APP)_*.patch.zip patch/lcdprint
