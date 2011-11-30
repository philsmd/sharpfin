#!/bin/ash

ACTION="`/opt/webserver/cgi-bin/getarg action`"
DATA="`/opt/webserver/cgi-bin/getarg url`"

#
# Show Form
#

if [ "$ACTION" = "" ]; then

  echo -e "Content-type text/html\r\n\r"
  echo "<html><head><title>Installing Addon</title>"
  echo "<link rel='STYLESHEET' href='/sharpfin.css' type='text/css' />"
  echo "</head>"
  echo "<body>"

  echo "<h1>Installing Software on the Radio</h1>"
  echo "<p>Be careful.  Install files are scripts that modify your radio. "
  echo "There is only limited space on the radio, and you can over-do it! "
  echo "You will be installing these files at your own risk.</p>"
  echo "<p>To protect you, installing is a 2-step process.  Once you submit a URL"
  echo "of a .install file, the file will be downloaded, and you will be"
  echo "automatically shown its readme file.  You will then be given the"
  echo "option of actually installing</p>"
  echo "<p><i>It is recommended that you reboot your radio following any"
  echo "installations, to guarantee that the disk is re-set to read-only</i></p>"
  echo ""
  echo "<form method=\"get\" action=\"/cgi-bin/admin_Install_Install_Addon.cgi\">"
  echo "URL: <input name=\"url\" type=\"text\" id=\"url\" value=\"http://www.website/upgrade.install\" size=\"80\" /><br/>"
  echo " <input type=\"hidden\" name=\"action\" value=\"url\" />"
  echo " <input type=\"submit\" value=\"Download Install Script\" />"
  echo "</form>"
  echo "</body></html>"

fi

#
# Download File and Show Readme
#

if [ "$ACTION" = "url" ]; then

  echo -e "Content-type text/html\r\n\r"
  echo "<html><head><title>OK To Proceed</title>"
  echo "<link rel='STYLESHEET' href='/sharpfin.css' type='text/css' />"
  echo "</head>"
  echo "<body>"
  echo "<h1>Confirm Install</h1>"

  /bin/rm -f /tmp/patchdownload.log
(
  echo "Downloading Installation File: $DATA"
  cd /tmp
  /bin/rm -rf /tmp/install /tmp/install.tar.bz2 /tmp/install.tar
  killall wget
  wget -O /tmp/install.tar.bz2 "$DATA"
  bunzip2 install.tar.bz2
  tar xf install.tar
  /bin/rm -f /tmp/install /tmp/install.tar.bz2 /tmp/install.tar
) > /tmp/patchdownload.log 2>&1
  
  if [ -f /tmp/install/readme.txt ]; then
    echo "<h1>Readme</h1><pre>"
    cat /tmp/install/readme.txt
    echo "</pre>"
    echo "<center><a href=\"/cgi-bin/admin_Install_Install_Addon.cgi?action=doit\">Click Here to Do the Install</a></center>"
  else
    echo "<h1>Problem</h1>"
    echo "<p>There was a problem downloading the installfile.  Check the URL was correct.  Are you sure it is a Sharpfin installfile?</p>"
    echo "<pre>"
    cat /tmp/patchdownload.log
    echo "</pre>"
  fi
  echo "</body></html>"

fi

#
# Do The Patching
#
if [ "$ACTION" = "doit" ]; then

  echo -e "Content-type text/html\r\n\r"
  echo "<html><head><title>Installing</title>"
  echo "<link rel='STYLESHEET' href='/sharpfin.css' type='text/css' />"
  echo "</head>"
  echo "<body>"
  echo "<h1>Installation Results</h1>"
  echo "<pre>"

cd /tmp/install
./install-me

cd /
sync
mount / -oro,remount

  echo "</pre>"
  echo "</body></html>"

fi
