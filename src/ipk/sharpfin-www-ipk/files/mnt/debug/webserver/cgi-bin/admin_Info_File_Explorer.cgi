#!/bin/ash

# Get HTTP ACTION
FILEPATH="`/opt/webserver/cgi-bin/getarg path`"

#Default is root directory
if [ "x$FILEPATH" = "x" ]; then
  FILEPATH="/"
fi

# Work out what we are looking at (can't cd into a file)
cd $FILEPATH
if [ "$FILEPATH" = "`pwd`" ]; then
  type="dir" 
  DIR="`pwd`" 
else
  type="file" 
fi

# Handle File download
if [ "$type" = "file" ]; then
  len="`ls -l $FILEPATH | cut -c30-42 | sed -e 's/ //g'`"
  echo -e "Content-Type binary/octet-stream\r\n\r"
  echo "Content-Length $length"
  echo "Content-Disposition attachment; filename=$FILEPATH; size=$len"
  echo ""
  cat "$FILEPATH"
  exit
fi

# Handle directory ACTION
if [ "$type" = "dir" ]; then

  ENTRIES="`ls`"

  echo -e "Content-Type text/html\r\n\r"
  echo "<html><head><title>File Explorer</title>"
  echo "<link rel='STYLESHEET' href='/sharpfin.css' type='text/css' />"
  echo "</head><body><h1>$DIR</h1>"

  echo "<table border=0 cellspacing=0 cellpadding=4>"

  if [ "$DIR" = "/" ]; then
    DIR=""
  else
    PARENT="`(cd .. ; pwd)`"
    iinfo="<a href=\"/cgi-bin/admin_Info_File_Explorer.cgi?path=$PARENT\"><span style=\"font-family: monospace\">..</span></a>"
    echo "<tr><td valign=top>$iinfo</td><td></td></tr>"
  fi

  for i in $ENTRIES; do
    iinfo="`/bin/ls -lad $i | cut -c1-55`"
    ilink="<a href=\"/cgi-bin/admin_Info_File_Explorer.cgi?path=$DIR/$i\"><span style=\"font-family: monospace\">$i</span></a>"
    echo "<tr><td valign=top>$ilink</td>"
    echo "<td valign=top><span style=\"font-family: monospace\">$iinfo</span></td></tr>"
  done
  
  echo "</table>"
  echo "</body></html>"
fi
