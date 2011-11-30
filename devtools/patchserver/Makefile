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

VERSION := 0.7rc1
windoze: patchserver_$(VERSION).zip
DIST := dist
DOC  := PatchingTheRecivaRadio
INC  := 
OPT  := -Wwrite-strings -DVERSION=\"$(VERSION)\" -Wno-deprecated
WINRESINC := --include-dir $(HOME)/.wine/drive_c/MinGW/include/

GCC:= $(shell which x86_64-w64-mingw32-g++)
ifeq ($(GCC),)
GCC:= $(shell which i586-mingw32msvc-g++)
ifeq ($(GCC),)
GCC:=gcc
endif
endif

clean:
	/bin/rm -f \
		*~ \
		launcher.res \
		commandline.res \
		patchserver_$(VERSION).zip \
		$(DIST)/patchserver-commandline.exe \
		$(DIST)/patchserver-launcher.exe \
		$(DIST)/README \
		$(DIST)/patchfiles.lst \
		$(DIST)/$(DOC).pdf \
		dialog.res

DISTFILES := \
	$(DIST)/README \
	$(DIST)/patchfiles.lst \
	$(DIST)/patchserver-launcher.exe \
	$(DIST)/patchserver-commandline.exe \
	$(DIST)/$(DOC).pdf

patchserver_$(VERSION).zip: $(DISTFILES)
	zip patchserver_$(VERSION).zip $(DISTFILES)

dist/README: README
	mkdir -p $(DIST)
	cp README $(DIST)/

dist/patchfiles.lst: patchfiles.lst
	mkdir -p $(DIST)
	cp patchfiles.lst $(DIST)/
	
COMMANDLINESRC := commandline.cpp webserver.cpp dnsserver.cpp netio.cpp

dist/patchserver-commandline.exe:  \
	$(COMMANDLINESRC) \
			commandline.h \
			terms.h \
			commandline.res
	$(GCC) $(OPT) $(COMMANDLINESRC) commandline.res $(INC) \
		-o $(DIST)/patchserver-commandline.exe

LAUNCHERSRC := launcher.cpp dialog.cpp geturls.cpp

dist/patchserver-launcher.exe:  \
	$(LAUNCHERSRC) \
			launcher.h \
			terms.h \
			launcher.res
	$(GCC) -DWINDOWS $(OPT) -mwindows $(LAUNCHERSRC) launcher.res \
		    $(INC) -lws2_32 -o $(DIST)/patchserver-launcher.exe

launcher.res: launcher.rc launcher.ico
	windres $(WINRESINC) $< -O coff -o $@

commandline.res: commandline.rc commandline.ico
	windres $(WINRESINC) $< -O coff -o $@

pdf dist/$(DOC).pdf: $(DOC).odt
	mkdir -p $(DIST)
	unoconv -f pdf -o $(DIST)/ $(DOC).odt
