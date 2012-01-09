REM Sharpfin project
REM Copyright (C) by 2012-01-09 Philipp Schmidt
REM
REM This file is part of the sharpfin project
REM
REM This Library is free software: you can redistribute it and/or modify
REM it under the terms of the GNU General Public License as published by
REM the Free Software Foundation, either version 3 of the License, or
REM (at your option) any later version.
REM
REM This Library is distributed in the hope that it will be useful,
REM but WITHOUT ANY WARRANTY; without even the implied warranty of
REM MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
REM GNU General Public License for more details.
REM
REM You should have received a copy of the GNU General Public License
REM along with this source files. If not, see
REM <http://www.gnu.org/licenses/>.

%~d0
cd %~p0
@echo off
c:\cygwin\bin\bash.exe install.sh
notepad c:\cygwin\opt\sde.txt
