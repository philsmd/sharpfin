#!/bin/ash

DESTNAME="`/opt/webserver/cgi-bin/getarg destname`"
PARTITION="`/opt/webserver/cgi-bin/getarg partition`"
FAILURL="http://www.pschmidt.it/sharpfin/images/8/8b/Sharpfin-nanddump_0.1.install"

#
# Show Form
#

echo -e "Content-type text/html\r\n\r"
echo "<html><head><title>Backup Partition</title>"
echo "<link rel='STYLESHEET' href='/sharpfin.css' type='text/css' />"
echo "</head>"
echo "<body>"

if [ ! "$DESTNAME" = "" ]; then

  echo "<h1>Partition Results</h1>"
  /bin/rm -rf /tmp/mtd-backup
  mkdir /tmp/mtd-backup
  cd /tmp/mtd-backup
  echo "<p>Running: nanddump /dev/mtd/$PARTITION $DESTNAME</p>"
  echo "<pre>"
  echo n | nanddump /dev/mtd/$PARTITION $DESTNAME
  echo "</pre>"
  echo "<br/>"
  echo "<p>Backup is now complete.  You can download this partition from "
  echo "<a href=\"/tmp/mtd-backup/$DESTNAME\">here</a></p>"
fi

echo "<h1>Backup a Partition</h1>"
which nanddump >/dev/null 2>&1
if [ "$?" -eq 0 -o -f "/usr/sbin/nanddump" ]
then
  echo "<p>The partitions are laid out on the NAND Flash as follows</p>"
  echo "<pre>"
  cat /proc/mtd
  echo "</pre>"
  echo "<p>Select one of the following links to create a backup file for one"
  echo "of the partitions.  This backup file can be restored using 'sharpflash',"
  echo "which is a JTAG programming utility.</p>"
  
  echo "<ul>"
  echo "<li><a href='/cgi-bin/admin_Tools_Backup.cgi?partition=0&destname=boot-mtd.bin'>boot partition (mtd0)</a></li>"
  echo "<li><a href='/cgi-bin/admin_Tools_Backup.cgi?partition=1&destname=kernel-mtd.bin'>kernel partition (mtd1)</a></li>"
  echo "<li><a href='/cgi-bin/admin_Tools_Backup.cgi?partition=2&destname=root-mtd.bin'>root partition (mtd2)</a></li>"
  echo "<li><a href='/cgi-bin/admin_Tools_Backup.cgi?partition=3&destname=config-mtd.bin'>config partition (mtd3)</a></li>"
  echo "<li><a href=/cgi-bin/admin_Tools_Backup.cgi?partition=4&destname=debug-mtd.bin'>debug partition (mtd4)</a></li>"
  if [ -c "/dev/mtd/5" ]
  then
    echo "<li><a href=/cgi-bin/admin_Tools_Backup.cgi?partition=5&destname=data-mtd.bin'>data partition (mtd5)</a></li>"
  fi

  echo "</ul>"
else
  echo "<h1>WARNING:</h1>"
  echo "To backup a partition the <i>nanddump</i> utility is needed, but it was NOT yet installed on your radio."
  echo "To install the missing utility click <a href='/cgi-bin/admin_Install_Install_Addon.cgi?url="$FAILURL"&action=url'>here</a>"
fi
echo "</body></html>"
