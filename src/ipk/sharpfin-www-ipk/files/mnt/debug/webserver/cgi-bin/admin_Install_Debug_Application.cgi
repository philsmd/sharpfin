#!/bin/ash

ACTION="`/opt/webserver/cgi-bin/getarg action`"
DATA="`/opt/webserver/cgi-bin/getarg url`"
#
# Show Form
#

if [ "$ACTION" = "" ]; then

  echo -e "Content-type text/html\r\n\r"
  echo "<html><head><title>Debug Application</title>"
  echo "<link rel='STYLESHEET' href='/sharpfin.css' type='text/css' />"
  echo "</head><body>"

  echo "<h1>Running Demonstration / Debug Software</h1>"
  echo "<p>This function is provided so that different applications"
  echo "can be tried / tested without risking corruption of the radio</p>"
  echo "<p>This function downloads the specified .debug bundle from"
  echo "a webserver, unpacks it in the ramdisk /tmp directory, and"
  echo "runs it there.  It avoids the need to write to the permanent"
  echo "memory, and all of the debug program is lost when the radio"
  echo "is rebooted.</p>"
  echo ""
  echo "<form method=\"get\" action=\"/cgi-bin/admin_Install_Debug_Application.cgi\">"
  echo "URL: <input name=\"url\" type=\"text\" id=\"url\" value=\"http://www.website/application.debug\" size=\"80\" /><br/>"
  echo " <input type=\"hidden\" name=\"action\" value=\"url\" />"
  echo " <input type=\"submit\" value=\"Run Debug Application\" />"
  echo "</form>"
  echo "<p>The output from the last debug to take place can be found"
  echo "<a href="/cgi-bin/admin_Install_Debug_Application.cgi?action=show">here</a></p>"
  echo "</body></html>"

fi

#
# Download File, Show Readme and run
#

if [ "$ACTION" = "url" ]; then

  echo -e "Content-type text/html\r\n\r"
  echo "<html><head><title>Running Application</title>"
  echo "<link rel='STYLESHEET' href='/sharpfin.css' type='text/css' />"
  echo "</head><body>"
  echo "<h1>Confirm Install</h1>"

  /bin/rm -f /tmp/patchdownload.log
(
  echo "Downloading Debug Application: $DATA"
  cd /tmp
  /bin/rm -f /tmp/debug.tar.bz2 /tmp/debug/readme.txt
  killall wget
  wget -O /tmp/debug.tar.bz2 "$DATA"
  bunzip2 debug.tar.bz2
  tar xf debug.tar
  /bin/rm -f debug.tar
) > /tmp/patchdownload.log
  
  if [ -f /tmp/debug/readme.txt ]; then
    echo "<h1>Readme</h1><pre>"
    cat /tmp/debug/readme.txt
    echo "</pre>"
    echo "<p><b>Running Application</b></p>"
    echo "<p>Output from the application can be found"
    echo "<a href=\"/cgi-bin/admin_Install_Debug_Application.cgi?action=show\">here</a></p>"
    cd /tmp/debug
    killall debug-me
    killall debugapp
    ./debug-me > /tmp/debug/debug.output 2>1 &
  else
    echo "<h1>Problem</h1>"
    echo "<p>There was a problem downloading the debugfile.  Check the URL was correct.  Are you sure it is a Sharpfin debugfile?</p>"
    echo "<pre>"
    cat /tmp/patchdownload.log
    echo "</pre>"
  fi
  echo "</body></html>"

fi

if [ "$ACTION" = "show" ]; then

  echo -e "Content-type text/html\r\n\r"
  echo ""
  echo "<html><head><title>Debug Output</title>"
  echo "<link rel='STYLESHEET' href='/sharpfin.css' type='text/css' />"
  echo "</head><body><pre>"
  cat /tmp/debug/debug.output
  echo "</pre></body></html>"
  
fi

