#!/bin/ash

VERS="`ipkg info 'sharpfin-www' | grep Version | cut -d: -f2`"

cd /opt/webserver/cgi-bin
menucgi="`ls radio_* | cut -d' ' -f1 | sed -e s/radio_//`"

menuitems="$menucgi $menuhtm"

echo -e "Content-Type text/html\r\n\r"
echo "<html><head><title>Sharpfin Menu</title>"
echo "<style type=\"text/css\"><!--"
echo "a:link { text-decoration: none; color: #FFFFFF; }"
echo "a:visited { text-decoration: none; color: #FFFFFF; }"
echo "a:hover { text-decoration: none; color: #FFFF66; }"
echo "a:active { text-decoration: none; color: #FFFFFF; }"
echo "body { font-size: 14px; color: #FFFFFF; font-family: Arial, Helvetica, sans-serif; background-color: #22CCFF; margin-left: 0px; margin-top: 0px; margin-right: 0px; margin-bottom: 0px; }"
echo "--></style></head>"

echo "<body background=\"/bgtile.jpg\"><center>"
echo "<a title=\"Sharpfin homepage\" target=\"_blank\" href=\"http://www.sharpfin.org/\"><img alt=\"Sharpfin logo\" border=0 src=\"/sharpfin_small.png\" width=145/></a><br/><br/>"
echo "<a href=\"/index.html\" target=\"_top\">Radio Home</a><br/>"
echo "<a href=\"/admin.html\" target=\"_top\">Admin Home</a><br/>"

last=""

for i in $menuitems; do

 sect="`echo $i | cut -d_ -f1`"
 title="`echo $i | cut -d_ -f2- | sed -e 's/_/ /g' | sed -e 's/.cgi//' | sed -e 's/.html//`"
 
  if [ ! "$sect" = "" ]; then
   if [ ! "$last" = "$sect" ]; then
    echo "<br/><b>$sect</b><br/>"
    last="$sect"
   fi
  fi
             
             
 if [ "`echo $i | grep .html`" = "" ]; then
   echo "<a href=\"/cgi-bin/radio_$i\" target=\"right\">$title</a><br/>"
 else
   echo "<a href=\"/admin/radio_$i\" target=\"right\">$title</a><br/>"
 fi

done

echo "<br/>"
echo "<p>v$VERS</p>"

echo "</center></body></html>"
