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

BIN       := lcdif_test
BINSRC	  := lcdif_test.c
LIBSRC    := lcdif.c

LIBOBJS   := $(subst .c,.o, $(LIBSRC))
BINOBJS   := $(subst .c,.o, $(BINSRC))
OBJS      := $(LIBOBJS) $(BINOBJS)

CROSS     := arm-unknown-linux-gnu-
CC        := $(CROSS)gcc
LD        := $(CROSS)ld
AR        := $(CROSS)ar
STRIP     := $(CROSS)strip
RM        := /bin/rm

CFLAGS    := -g -Wall -Werror
LDFLAGS   := 

libs: $(LIBOBJS)

all: test

test: $(BIN)

clean:
	$(RM) -f $(OBJS) $(BIN) $(BIN)_notstripped

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


$(BIN): $(OBJS)
	$(CC) $(OBJS) -o $(BIN)_notstripped
	cp $(BIN)_notstripped $(BIN)
	$(STRIP) $(BIN)
