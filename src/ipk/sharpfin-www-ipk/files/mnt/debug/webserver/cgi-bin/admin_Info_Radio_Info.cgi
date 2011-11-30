#!/bin/ash

. readconfig.sh
RECIVA_APPV="`ipkg list_installed | grep reciva-app | cut -d' ' -f3`"
SERVPACK="`/usr/bin/get-current-service-pack-version`"
KERNPACK="`/usr/bin/get-current-kernel-package-version`"
PROC="`cat /proc/version`"
CPU="`cat /proc/cpuinfo`"
BUSYBOX="`busybox | head -1`"
SHARPFIN="`ipkg list_installed | grep sharpfin | sed -e 's/ - $//g' | sed -e 's/ - /_/g'`"

echo -e "Content-Type text/html\r\n\r"
echo "<html><head><title>Radio Info</title>"
echo "<link rel='STYLESHEET' href='/sharpfin.css' type='text/css' />"
echo "</head>"
echo "<body><h1>Radio Information</h1>"
echo "<table border=1 cellspacing=0 cellpadding=5>"
echo "<tr><td>Reciva Hardware Config</td><td>$RECIVA_HW_CONFIG</td></tr>"
echo "<tr><td>Reciva Radio App Version</td><td>$RECIVA_APPV</td></tr>"
echo "<tr><td>Reciva Service Pack No.</td><td>$SERVPACK</td></tr>"
echo "<tr><td>Reciva Kernel Package Version.</td><td>$KERNPACK</td></tr>"
echo "<tr><td>Sharpfin Versions</td><td>$SHARPFIN</td></tr>"
echo "<tr><td>Processor Information</td><td>$PROC</td></tr>"
echo "<tr><td>CPU Information</td><td>$CPU</td></tr>"
echo "<tr><td>Busybox Version</td><td>$BUSYBOX</td></tr>"

echo "<tr><td>Disk Usage</td><td><pre>"
df -k
echo "</pre></td></tr>"

echo "<tr><td>Memory Info</td><td><pre>"
cat /proc/meminfo
echo "</pre></td></tr>"

echo "<tr><td>Modules</td><td><pre>"
cat /proc/modules
echo "</pre></td></tr>"

echo "<tr><td>Processes</td><td><pre>"
/bin/ps 
echo "</pre></td></tr>"

echo "<tr><td>Messages</td><td><pre>"
dmesg
echo "</pre></td></tr>"

echo "</table>"
echo "</body></html>"
