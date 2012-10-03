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

VERSION := 0.7rc3
TARGET  := dist
DIST := radiopatch
DOC  := PatchingTheRecivaRadio
OPT  := -Wwrite-strings -DVERSION=\"$(VERSION)\" -Wno-deprecated
LAUNCHERINC := -I C:/cygwin/lib/gcc/i686-pc-cygwin/3.4.4/include/c++/backward
CROSS :=
GCC:=$(CROSS)gcc

all: linux windoze
linux: $(TARGET)/patchserver_$(VERSION).tar.bz2
windoze: $(TARGET)/patchserver_$(VERSION).zip

clean:
	/bin/rm -f \
	*~ \
	*.o \
	dialog.res  \
	launcher.res  \
	commandline.res \
	$(DIST)/README  \
	$(TARGET)/patchserver_*.zip \
	$(TARGET)/patchserver_*.tar.bz2 \
	$(DIST)/sharpfin-test*   \
	$(DIST)/patchfiles.lst   \
	$(DIST)/patchserver-launcher.exe \
	$(DIST)/$(DOC).pdf \
	$(DIST)/$(DOC).odt \
	$(DIST)/patchserver-commandline \
	$(DIST)/patchserver-commandline.exe

DISTFILESWIN := \
	$(DIST)/README \
	$(DIST)/patchfiles.lst \
	$(DIST)/patchserver-launcher.exe \
	$(DIST)/patchserver-commandline.exe

TESTPATHFILES := \
	$(DIST)/sharpfin-test.patch \
	$(DIST)/sharpfin-test.patch.txt

$(TARGET)/patchserver_$(VERSION).zip: pdf testpatch $(DISTFILESWIN)
	@(if [ -f "$(DIST)/$(DOC).pdf" ]; \
	  then \
		docfile=$(DIST)/$(DOC).pdf; \
	  else \
		docfile=$(DIST)/$(DOC).odt; \
	  fi; \
	  which zip>/dev/null 2>&1; \
	  if [ "$$?" -eq 0 ]; \
	  then \
	      zip $(TARGET)/patchserver_$(VERSION).zip $(DISTFILESWIN) $(TESTPATHFILES) $$docfile; \
	  else \
	  	which 7z>/dev/null 2>&1; \
		if [ "$$?" -eq 0 ]; \
		then \
			7z a $(TARGET)/patchserver_$(VERSION).zip $(DISTFILESWIN) $(TESTPATHFILES) $$docfile; \
		else \
			echo "[-] Could NOT find any zip tool"; \
		fi \
	  fi)

DISTFILESLINUX := \
	$(DIST)/README \
	$(DIST)/patchfiles.lst \
	$(DIST)/patchserver-commandline

$(TARGET)/patchserver_$(VERSION).tar.bz2: pdf testpatch $(DISTFILESLINUX)
	chmod +x $(DIST)/patchserver-commandline
	@(if [ -f "$(DIST)/$(DOC).pdf" ]; \
	  then \
	        docfile=$(DIST)/$(DOC).pdf; \
	  else \
	        docfile=$(DIST)/$(DOC).odt; \
	  fi; \
	  tar pcjf $(TARGET)/patchserver_$(VERSION).tar.bz2 $(DISTFILESLINUX) $(TESTPATHFILES) $$docfile)

testpatch:
	@(testPatchFolder="../../src/install/sharpfin-test-patch"; \
	 if [ -d "$$testPatchFolder" ]; \
	 then \
		 if [ ! -f "$$testPatchFolder/sharpfin-test.patch" ]; \
		 then \
			cd $$testPatchFolder; \
			make; \
			cd -; \
		 fi; \
		 cp $$testPatchFolder/sharpfin-test.patch $(DIST)/; \
		 cp $$testPatchFolder/patch/readme.txt $(DIST)/sharpfin-test.patch.txt; \
	 fi)

$(DIST)/README: README
	cp README $(DIST)/

$(DIST)/patchfiles.lst: patchfiles.lst
	cp patchfiles.lst $(DIST)/
	
COMMANDLINESRC := commandline.cpp webserver.cpp dnsserver.cpp netio.cpp

$(DIST)/patchserver-commandline: \
	$(COMMANDLINESRC) \
	commandline.h \
	terms.h
	$(GCC) -fpermissive $(OPT) $(COMMANDLINESRC) -o $(DIST)/patchserver-commandline.o -lstdc++
	mv $(DIST)/patchserver-commandline.o $(DIST)/patchserver-commandline

LAUNCHERSRC := launcher.cpp dialog.cpp geturls.cpp

$(DIST)/patchserver-commandline.exe: \
	$(COMMANDLINESRC) \
	commandline.h \
	terms.h \
	commandline.res
	$(GCC) -DWINDOWS -fpermissive $(OPT) $(COMMANDLINESRC) commandline.res -o $(DIST)/patchserver-commandline.exe -lws2_32

$(DIST)/patchserver-launcher.exe:  \
	$(LAUNCHERSRC) \
	launcher.h \
	terms.h \
	launcher.res
	$(GCC) -DWINDOWS $(OPT) -mwindows $(LAUNCHERSRC) launcher.res \
		$(LAUNCHERINC) -lws2_32 -o $(DIST)/patchserver-launcher.exe

launcher.res: launcher.rc launcher.ico
	windres $< -O coff -o $@

commandline.res: commandline.rc commandline.ico
	windres $< -O coff -o $@

pdf $(DIST)/$(DOC).pdf: $(DOC).odt
	mkdir -p $(DIST)
	mkdir -p $(TARGET)
ifeq ($(shell which unoconv 2>/dev/null),)
ifeq ($(shell which soffice 2>/dev/null),)
	@echo "[-] Could NOT find any tool to convert ODT files to PDF"
	cp $(DOC).odt $(DIST)/
else
	soffice --invisible --convert-to pdf:writer_pdf_Export $(DOC).odt
	@(if [ -f "$(DOC).pdf" ]; \
	then \
		mv $(DOC).pdf $(DIST)/; \
	fi)
endif
else
	@(ver=`unoconv --version|head -1|egrep -o '[.,0-9]*'`; \
	ver=`echo "$$ver*10"|bc -l|sed 's/\([0-9]*\).*/\1/g'`; \
	if [ "$$ver" -ge 5 ]; \
	then \
		unoconv -f pdf -o $(DIST)/ $(DOC).odt; \
	else \
		unoconv -f pdf $(DOC).odt; \
		mv $(DOC).pdf $(DIST)/$(DOC).pdf; \
	fi)
endif
