#!/bin/sh
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

(cd / ; tar xvf - ) < tools.tar
cp -f sde.txt /opt
cp -f sharpfin.sh /etc/profile.d
chmod 555 /opt/sde.txt /etc/profile.d/sharpfin.sh
