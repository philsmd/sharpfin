Sharpfin Radio Webserver Files

Overview
--------
These files provide admin management of the radio via
the webserver, which is installed as part of the patch
process.

You will be able to connect with a web browser, to your
radio and access the admin pages, which can be found at

  http://...yourradio.../admin

If you have modified your webserver's index.html page
in the top directory, and are performing an upgrade,
this will be detected, and your file will not be touched.

Prerequisites
-------------
This release of the webserver files no longer includes 
the startup scripts for the webserver, and consequently
you must have the sharpfin-httpd package installed, either
through installing the sharpfin base patchfile (0.2 or 
greater)

