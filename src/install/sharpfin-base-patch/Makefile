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

PATCHVER := 0.5-alpha20120101
WWWVER := 0.9
TDVER := 0.1
HDVER := 0.1
BSVER := 0.3-alpha20120101
FDVER := 0.1

WWWIPK := sharpfin-www_$(WWWVER)_arm.ipk
TDIPK := sharpfin-telnetd_$(TDVER)_arm.ipk
BSIPK := sharpfin-base_$(BSVER)_arm.ipk
HDIPK := sharpfin-httpd_$(HDVER)_arm.ipk
FDIPK := sharpfin-ftpd_$(FDVER)_arm.ipk

sharpfin-base_$(PATCHVER).zip: sharpfin-base_$(PATCHVER).patch
	/bin/rm -rf radiopatch
	mkdir radiopatch
	cp patch/readme.txt radiopatch/sharpfin-base_$(PATCHVER).patch.txt
	cp sharpfin-base_$(PATCHVER).patch radiopatch/sharpfin-base_$(PATCHVER).patch
	zip sharpfin-base_$(PATCHVER).zip radiopatch/*
	rm -rf radiopatch

sharpfin-base_$(PATCHVER).patch: patch/$(FDIPK) patch/$(HDIPK) patch/$(WWWIPK) patch/$(TDIPK) patch/$(BSIPK) patch/install-me patch/lcdprint
	chmod 755 patch/install-me
	tar cf patch.tar patch/*
	bzip2 patch.tar
	mv patch.tar.bz2 sharpfin-base_$(PATCHVER).patch

patch/lcdprint: ../../apps/lcdprint/lcdprint
	cp -f ../../apps/lcdprint/lcdprint patch/lcdprint
		
patch/$(WWWIPK): ../../ipk/sharpfin-www-ipk/$(WWWIPK)
	cp ../../ipk/sharpfin-www-ipk/$(WWWIPK) patch/$(WWWIPK)

patch/$(TDIPK): ../../ipk/sharpfin-telnetd-ipk/$(TDIPK)
	cp ../../ipk/sharpfin-telnetd-ipk/$(TDIPK) patch/$(TDIPK)

patch/$(BSIPK): ../../ipk/sharpfin-base-ipk/$(BSIPK)
	cp ../../ipk/sharpfin-base-ipk/$(BSIPK) patch/$(BSIPK)

patch/$(HDIPK): ../../ipk/sharpfin-httpd-ipk/$(HDIPK)
	cp ../../ipk/sharpfin-httpd-ipk/$(HDIPK) patch/$(HDIPK)

patch/$(FDIPK): ../../ipk/sharpfin-ftpd-ipk/$(DDIPK)
	cp ../../ipk/sharpfin-ftpd-ipk/$(FDIPK) patch/$(DDIPK)

clean:
	/bin/rm -f *~ patch/*~ patch/*.ipk tar.tar tar.tar.bz2 sharpfin-base_*.zip sharpfin-base_*.patch patch/lcdprint
