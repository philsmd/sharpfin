#!/bin/ash

QUERY="`echo $QUERY_STRING | cut -d= -f1`"
DATA="`echo $QUERY_STRING | cut -d= -f2 | cut -d\& -f1`"

# Remove the http encoding
DATA="`httpd -d \"$DATA\"`"

#
# Show Form
#

echo -e "Content-type text/html\r\n\r"
echo "<html><head><title>Run Program</title>"
echo "<link rel='STYLESHEET' href='/sharpfin.css' type='text/css' />"
echo "</head>"
echo "<body>"

if [ ! "$QUERY" = "" ]; then

  echo "<h1>Program Output</h1>"
  echo "<p> Output of <i>$DATA</i></p>"
  echo "<pre>"
  cd /tmp
  $DATA 2>&1
  echo "</pre>"
  echo "<br/>"
fi

echo "<h1>Run Program</h1>"
echo "<p>Enter the program you wish to run.  e.g. \"ls /tmp\"</p>"
echo ""
echo "<form method=\"get\" action=\"/cgi-bin/admin_Tools_Run_Program.cgi\">"
echo "Program: <input name=\"program\" type=\"text\" id=\"program\" value=\"ls\" size=\"80\" /><br/>"
echo " <input type=\"submit\" value=\"Run Program\" />"
echo "</form>"
echo "</body></html>"

