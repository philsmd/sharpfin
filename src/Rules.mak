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

##############################################################################
# Toolchain configuration
##############################################################################
export CROSS     := arm-9tdmi-linux-gnu-
#export HARDWARE  := reciva   # Defined in src/Makefile
export ARFLAGS   += 
export CFLAGS    += -Wall -Werror -I./include -I../include/reciva
export LDFLAGS   += -g $(LDLIBS)
export CC        := $(CROSS)gcc
export LD        := $(CROSS)gcc
export AR        := $(CROSS)ar
export MAKE      := make
export DIRTOP	 := $(shell cd $(dir $(lastword $(MAKEFILE_LIST)));pwd)

##############################################################################
# Stuff for quiet/verbose
##############################################################################
ifdef verbose
  P := @true
  E :=
  MAKE := make
else
  P := @echo
  E := @
  MAKE := make -s
endif

##############################################################################
# Rules
##############################################################################
OBJS      := $(subst .c,.o, $(SRC))

.PHONY: $(SUBDIRS) $(SUBMAKES) $(LIB) $(BIN)

all: $(DIRTOP)/libreciva/libreciva.a $(SUBDIRS) $(SUBMAKES) $(LIB) $(BIN)

$(SUBDIRS):
	$(P) " [SUBDIR] $(shell pwd)/$@"
	$(E) $(MAKE) -C $@

$(SUBMAKES):
	$(P) " [SUBMAK] $(shell pwd)/$@"
	$(E) $(MAKE) -f $@

%.o: %.c
	$(P) " [CC    ] $<"
	$(E) $(CC) $(CFLAGS) -c $< -o $@ 

$(LIB): $(OBJS)
	$(P) " [AR    ] $<"
	$(E) $(AR) $(ARFLAGS) $@ $?

$(BIN): $(OBJS)
	$(P) " [LINK  ] $@"
	$(E) $(LD) -o $@ $^ $(LDFLAGS)

$(DIRTOP)/libreciva/libreciva.a:
	echo $(dir $(CURDIR))
	echo $(DIRTOP)
ifneq ($(dir $(CURDIR)),$(DIRTOP)/)
	(cd $(DIRTOP)/libreciva/;make)
#else
# fake the building, it will be builded anyway right now
#	(touch $(DIRTOP)/libreciva/libreciva.a)
endif

clean:	
	$(P) " [CLEAN ] $(shell pwd)"
	$(E) for d in $(SUBDIRS); do $(MAKE) -C $$d clean; done
	$(E) for d in $(SUBMAKE); do $(MAKE) -f $$d clean; done
	$(E) rm -f $(OBJS) $(LIB) $(BIN) core
