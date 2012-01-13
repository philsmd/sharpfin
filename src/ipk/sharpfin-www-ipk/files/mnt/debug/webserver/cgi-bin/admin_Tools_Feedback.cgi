#!/bin/ash

SHARPFIN="`ipkg list_installed | grep sharpfin | sed -e 's/ - $//g' | sed -e 's/ - /_/g'`"
APPVER="`ipkg list_installed | grep reciva-app | cut -d' ' -f3`"
CONFVAL=`readconfig`
RECIVA_HW_CONFIG=$((0x$CONFVAL & 0x3FF))
. readconfig.sh
VERSION="`echo $RECIVA_HW_CONFIG`;$APPVER;`/usr/bin/get-current-service-pack-version`;`/usr/bin/get-current-kernel-package-version`"

echo -e "Content-type text/html\r\n\r"
cat << ..EOF..
<head>
<link rel='STYLESHEET' href='/sharpfin.css' type='text/css' />
<title>Feedback</title>
</head>

<body>
<h1>Feedback</h1>
<p><b>Privacy Statement: </b>This form collects information so that we may better understand the range of radios out there
and improve our firmware patching mechanism.  The form automatically collects information about the radio operating
system and application versions.  The form does not automatically collect any personal information - that is for you
to decide to provide to us.  We only use information you provide to us for analysis purposes, and never pass it on to any
third party.</p>
<p><i><b>Please note that the data submitted with this form is not checked daily!</b></i></p>
<p><i>If you need feedback or support, please contact us through the <a href="http://groups.google.co.uk/group/sharpfin" target="forum">forum</a> or visit the <a href="http://www.pschmidt.it/sharpfin/">Sharpfin homepage</a></i></p>

<form id="form1" name="form1" method="post" action="http://www.pschmidt.it/sharpfin/feedback.php">
    <table width="800" border="0">
      <tr>
        <td width="150" valign=top>Radio Model No.</td>
        <td width="650"><label>
          <input name="radio" type="text" id="radio" size="60" />
        </label></td>
      </tr>
      <tr>
        <td valign=top>Radio Version </td>
        <td><input name="version" type="hidden" id="version" size="80" value="$VERSION" />$VERSION</td>
      </tr>
      <tr>
        <td valign=top>Sharpfin Version</td>
        <td><input name="osinfo" type="hidden" id="osinfo" value="$SHARPFIN" />$SHARPFIN</td>
      </tr>      
      <tr>
        <td valign=top>Name/Alias (option)</td>
        <td><input name="name" type="text" id="name" size="60" /><br/>
      </tr>
      <tr>
        <td valign=top>Comments<br><font size=1><i>(Remember to use the forum if you want a response!)</i></font></td>
        <td><label>
          <textarea name="feedback" cols="60" rows="10" id="feedback"></textarea>
        </label></td>
      </tr>
      <tr>
        <td colspan="2" style="text-align:center">
          <p>&nbsp;</p>
          <input type="submit" name="Submit" value="Submit" />
        </td>
      </tr>
    </table>
    <input name="success" type="hidden" id="success" value="1" />
  </form>
<p><font size=1><i>We would appreciate it if you could provide a name or alias on this form so we can ignore duplicates
when counting the number of successful patches (if you use this form twice)</i></font></p>
               
</body>
