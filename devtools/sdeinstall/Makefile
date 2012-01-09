# Sharpfin project
# Copyright (C) by 2012-01-09 Philipp Schmidt
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

# Note: For 7zS.sfx, 7zS2.sfx and/or 7zSD.sfx you may need "7z_extra"
# (see README)

VERSION := 0.2
DIST := dist
DOC  := README
ZIP := 7z
SFXTYPE := S
SFX := 7z$(SFXTYPE).sfx

all: tools.tar sde-install.7z sde-install-$(VERSION).exe
	mkdir -p $(DIST)
	$(ZIP) a $(DIST)/SDE-install-$(VERSION).zip sde-install-$(VERSION).exe sde.txt

clean:
	/bin/rm -f \
	tools.tar \
	sde-install.7z \
	sde-install-*.exe \
	$(DIST)/SDE-install-*.zip

tools.tar:
	tar pcfh tools.tar /opt/crosstool

sde-install.7z:
	$(ZIP) a sde-install.7z run.bat install.sh sde.txt sharpfin.sh tools.tar

sde-install-$(VERSION).exe:
	@(sfxPath=`which $(ZIP) 2>/dev/null`; \
	 sfxPath=`dirname "$$sfxPath" 2>/dev/null`; \
	 sfxPathWin=`cygpath -w "$$sfxPath" 2>/dev/null`; \
	 if [ -n "$$sfxPathWin" ]; \
	 then \
		cmd /C copy /b "$$sfxPathWin\$(SFX)" + config.txt + sde-install.7z sde-install-$(VERSION).exe; \
	 else \
		echo "7z.sfx NOT found. Abort"; \
	 fi)

