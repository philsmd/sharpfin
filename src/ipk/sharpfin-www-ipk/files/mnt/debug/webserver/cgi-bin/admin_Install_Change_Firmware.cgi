#!/bin/ash

ACTION="`/opt/webserver/cgi-bin/getarg action`"
DATA="`/opt/webserver/cgi-bin/getarg version`"

#
# Show Form
#

echo -e "Content-type text/html\r\n\r"
echo "<html><head><title>Change Firmware</title>"
echo "<link rel='STYLESHEET' href='/sharpfin.css' type='text/css' />"
echo "</head>"
echo "<body>"

if [ ! "$ACTION" = "" ]; then

  cd /opt/webserver/tmp
  /bin/rm -f /tmp/*.patch *.patch.txt
  echo "<h1>Firmware Patchfiles</h1>"
  echo "<h2>Downloading From Reciva ...</h2>"
  echo "<pre>"
  URL="reciva://copper.reciva.com:6000/service-pack/sp-wrap-cache/sp.$DATA.tar"
  curl -o $DATA.patch $URL
  echo "$DATA.patch, downloaded from Reciva on `date`.  INSTALLATION IS AT YOUR OWN RISK, AND WILL INVALIDATE YOUR WARRANTY" > $DATA.patch.txt
  echo "</pre>"
  if [ -f $DATA.patch ]; then
    echo "<p><i>... done ... </i></p>"
    echo "<h2>Files</h2>"
    echo "<p>Save these two files, and then install them to your radio using the patchserver</p>"
    echo "<ul><li><a href=\"/tmp/$DATA.patch\">$DATA.patch</a></li>"
    echo "<li><a href=\"/tmp/$DATA.patch.txt\">$DATA.patch.txt</a></li></ul>"
    echo "<p>The patchserver (and instructions) can be found on the"
    echo "<a href=\"http://www.sharpfin.zevv.nl/\" target=\"sharpfin\">Sharpfin Website</a></p>"
    echo "<p><i>be aware that this patchfile contains the instructions to download the actual"
    echo "firmware.  Please don't keep a backup of this patchfile, thinking you have the whole"
    echo "firmware.  The firmware is downloaded from the Reciva servers as part of the upgrade"
    echo "process</i></p>"
  else
    echo "<p><i>... error ... </i></p>"
    echo "<p>The patchfile could not be found on the Reciva server.  The path used was: $URL</p>"
  fi
  
else

  echo "<h1>Change Firmware</h1>"
  echo "<p>This option allows you to obtain a different firmware release"
  echo "for your radio to the one you are currently running, by downloading"
  echo "new patchfiles from Reciva.</p>"
  echo "<p>You can also use this option to re-install your current firmware"
  echo "release on your radio, which will effectively remove any modifications"
  echo "you may have made</p>"
  echo ""
  echo "<p>Once you find a patch with this form, download it, and install it"
  echo "using the patchserver</p>"
  echo "" 
  echo "<p><i>Some firmware releases are complete flash distributions, whereas"
  echo "others are delta patches.  Only install complete flash distributions. "
  echo "It is our understanding that the firmware patches do "
  echo "work on different radio models.  Please check the Sharpfin website"
  echo "for details of which firmwares have been tried on which radios</i></p>"
  echo ""
  echo "<form method=\"get\" action=\"/cgi-bin/admin_Install_Change_Firmware.cgi\">"
  echo "Firmware Version: <input name=\"version\" type=\"text\" value=\"257-a-297-a-027\" size=\"40\" /><br/>"
  echo " <input type=\"hidden\" name=\"action\" value=\"program\" />"
  echo " <input type=\"submit\" value=\"Find Patch\" />"
  echo "</form>"
  
fi

echo "</body></html>"

