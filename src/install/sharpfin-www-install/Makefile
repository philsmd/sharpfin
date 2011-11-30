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

VER := 0.6
APP := www
INS := sharpfin-$(APP)_$(VER).install
IPK := sharpfin-$(APP)_$(VER)_arm.ipk
IPKSRC := ../../ipk/sharpfin-$(APP)-ipk/sharpfin-$(APP)_$(VER)_arm.ipk

$(INS): install/$(IPK) install/install-me install/readme.txt
	/bin/rm -f *~ install/*~
	chmod -R 755 install
	tar cf tar.tar install/*
	bzip2 tar.tar
	mv tar.tar.bz2 $(INS)

install/$(IPK): $(IPKSRC)
	rm -f install/sharpfin-$(APP)_*.install
	cp -f $(IPKSRC) install/$(IPK)

clean:
	/bin/rm -f *~ install/*~ tar.tar tar.tar.bz2 sharpfin-$(APP)_*.install install/$(IPK)
