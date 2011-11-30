#!/bin/ash

echo -e "Content-Type text/html\r\n\r"

echo "<html><head><title>Radio Addons Info</title>"
echo "<link rel='STYLESHEET' href='/sharpfin.css' type='text/css' />"
echo "</head>"
echo "<body><h1>Addons Information</h1><pre>"
ipkg info
echo "</pre></body></html>"
