#!/bin/ash

sync
echo -e "Content-Type text/html\r\n\r"

echo "<html><head><title>Rebooting Radio</title>"
echo "<link rel='STYLESHEET' href='/sharpfin.css' type='text/css' />"
echo "</head>"
echo "<body><h1>Rebooting Radio</h1>"
echo "<p>Please be patient ....</p>"
echo "<p>And click <a href='/index.html' target='_top'>here</a> when complete.</p>"
echo "</body></html>"
sleep 1
shutdown -r -n now
