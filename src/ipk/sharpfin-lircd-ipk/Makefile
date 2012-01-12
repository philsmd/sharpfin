# Sharpfin project
# Copyright (C) by Philipp Schmidt
# 
# This file is part of the Sharpfin project
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

all: files/usr/sbin/lircd-r files/usr/sbin/irsend
	$(CROSS)strip files/usr/sbin/irsend files/usr/sbin/lircd-r
	chmod -R 755 files
	(daemon_size=`ls -l files/usr/sbin/lircd.sh|awk '{print $$5;}'`;sed -i "s/ -eq [0-9]*[ \t]*\$$/ -eq $$daemon_size/" files/CONTROL/postinst)
	../../../devtools/bin/ipkg-build -o root -g root files

clean:
	/bin/rm -f *.ipk files/usr/sbin/lircd-r files/usr/sbin/irsend

files/usr/sbin/lircd-r:
	(cd lirc/daemons;$(CC) -I.. -DHAVE_CONFIG_H -O2 -g -Wall -o ../../files/usr/sbin/lircd-r lircd.c ir_remote.c config_file.c hw-types.c release.c hw_default.c receive.c transmit.c)

files/usr/sbin/irsend:
	(cd lirc/tools;$(CC) -I.. -DHAVE_CONFIG_H -O2 -g -Wall irsend.c -o ../../files/usr/sbin/irsend)
