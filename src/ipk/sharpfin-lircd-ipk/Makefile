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
	(cd lirc/daemons;$(CC) -I.. -DHAVE_CONFIG_H -O2 -g -Wall lircd.c ir_remote.c config_file.c hw-types.c release.c hw_mplay.c hw_irlink.c hw_i2cuser.c hw_accent.c hw_bte.c hw_creative.c hw_creative_infracd.c hw_default.c hw_devinput.c hw_dsp.c hw_ea65.c hw_hiddev.c hw_livedrive_common.c hw_livedrive_midi.c hw_livedrive_seq.c hw_logitech.c hw_mouseremote.c hw_mp3anywhere.c hw_pcmak.c hw_pinsys.c hw_pixelview.c hw_silitek.c hw_tira.c hw_udp.c hw_uirt2.c hw_uirt2_common.c hw_uirt2_raw.c hw_usbx.c receive.c serial.c transmit.c input_map.c -o ../../files/usr/sbin/lircd-r)

files/usr/sbin/irsend:
	(cd lirc/tools;$(CC) -I.. -DHAVE_CONFIG_H -O2 -g -Wall irsend.c -o ../../files/usr/sbin/irsend)
