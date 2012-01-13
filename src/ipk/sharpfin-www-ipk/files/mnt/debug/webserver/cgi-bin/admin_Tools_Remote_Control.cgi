#!/bin/ash

COM="`/opt/webserver/cgi-bin/getarg com`"
NAME="`/opt/webserver/cgi-bin/getarg name`"
AJAX="`/opt/webserver/cgi-bin/getarg ajax`"

#
# Remote Control
#
echo -e "Content-type text/html\r\n\r"

# start the daemon if  necessary
ps aux|grep lircd|grep -v grep>/dev/null 2>&1
if [ "$?" -eq 1 -a -f "/usr/sbin/lircd-r" ]
then 
    /usr/sbin/lircd 2>/dev/null
fi

if [ -n "$COM" -a -n "$NAME" ]
then
    sync
    irsend SIMULATE "0000000000000000 00 $COM 00"
    if [ "$?" -eq 0 ]
    then
        STATUS="The command \`$NAME\` was successfully sent to your radio"
	STATUSCOLOR="green"
    else
        STATUS="The command \`$NAME\` could NOT be sent to your radio"
	STATUSCOLOR="green"
    fi
    if [ -n "$AJAX" -a "$AJAX" = 1 ]
    then
        echo $STATUS
        exit 0;
    else
        # continue	
        STATUS="<div id='lircStatus' style='color:$STATUSCOLOR'>$STATUS<br/></div>"
    fi
fi

cat << ..END
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN">
<HTML>
<HEAD>
<TITLE>Sharpfin Remote Control</TITLE>
<link rel="STYLESHEET" href="/sharpfin.css" type="text/css" />
<script type="text/javascript" src="/functions.js"></script>
</HEAD>
<BODY onLoad="MM_preloadImages('/remote_mute.jpg')">
<h1 id='pageTitle'>Sharpfin Radio Control Interface</h1>
$STATUS
..END

if [ -f "/usr/sbin/lircd" ]
then
cat << ..END
<center>
  <table width="99%" border="0" cellspacing="0" cellpadding="0">
    <tr>
      <td width="45%"><table width="300" height="620" border="0" align="center" cellpadding="0" cellspacing="0" id="Tabelle_01">
        <tr>
          <td width="300" height="359" colspan="3"><img src="../remote_01.jpg" alt="" width="300" height="359" border="0" usemap="#remoteMap"></td>
        </tr>
        <tr>
          <td width="105" height="51"><img src="../remote_02.jpg" alt="" width="105" height="51" border="0" usemap="#remote2Map"></td>
          <td><img src="../remote_no_mute.jpg" alt="" name="speaker" width="77" height="51" border="0" usemap="#remote3Map" id="speaker"></td>
          <td width="118" height="51"><img src="../remote_03.jpg" alt="" width="118" height="51" border="0" usemap="#remote4Map"></td>
        </tr>
        <tr>
          <td width="300" height="210" colspan="3"><img src="../remote_04.jpg" alt="" width="300" height="210" border="0" usemap="#remote5Map"></td>
        </tr>
      </table>
     </p></td>
      <td width="55%" valign="top"><p style="font-weight: bold">
      </p>
        <p>This is the Remote Control designed and developed by gforums.</p>
        <p>Within this interface, you can control your RECIVA&copy;-based radio in many ways.<br/>
	This page lets you control everything you can realize with the normal remote that possibly comes with your radio.
    </tr>
  </table>
        <map name="remoteMap">
            <area shape="rect" coords="35,95,100,125" href="?com=v&name=Settings" alt="Settings">
            <area shape="rect" coords="120,81,177,132" href="?com=O&name=Power" alt="On / Off">
            <area shape="rect" coords="204,87,267,135" href="?com=4&name=Equalizer" alt="Equalizer">
            <area shape="rect" coords="130,186,166,226" href="?com=p&name=Up" alt="Up">
            <area shape="rect" coords="133,271,166,319" href="?com=q&name=Down" alt="Down">
            <area shape="rect" coords="125,235,172,270" href="?com=R&name=Select" alt="Select">
            <area shape="rect" coords="218,187,267,215" href="?com=c&name=Playmode" alt="Playmode">
            <area shape="rect" coords="221,222,269,255" href="?com=T&name=Shift" alt="Shift">
            <area shape="rect" coords="219,259,269,294" href="?com=f&name=Sleep-Timer" alt="Sleep Timer">
            <area shape="rect" coords="27,219,70,239" href="?com=o&name=Aux-In" alt="Aux In">
            <area shape="rect" coords="29,240,76,262" href="?com=n&name=Mediaplayer" alt="Media Player">
            <area shape="rect" coords="30,262,77,278" href="?com=m&name=Channellist" alt="Channel List">
            <area shape="rect" coords="28,362,107,383" href="?com=5&name=Menu" alt="Menu">
            <area shape="rect" coords="195,361,277,382" href="?com=N&name=Back" alt="Back">
            <area shape="rect" coords="230,388,276,408" href="?com=Y&name=Next" alt="Next">
            <area shape="rect" coords="170,384,213,411" href="?com=a&name=Play-Pause" alt="Play / Pause">
            <area shape="rect" coords="93,382,129,413" href="?com=b&name=Stop" alt="Stop">
            <area shape="rect" coords="29,387,76,410" href="?com=Z&name=Previous" alt="Previous">
            <area shape="rect" coords="32,184,72,208" href="?com=M&name=Reply" alt="Reply">
            <area shape="rect" coords="27,306,91,324" href="?com=5&name=Menu"  alt="Menu">
            <area shape="rect" coords="212,303,269,325" href="?com=N&name=Back" alt="Back">
            <area shape="rect" coords="119,149,182,172" href="?com=d&name=Browse" alt="Browse">
            <area shape="rect" coords="89,238,124,267" href="?com=Z&name=Left" alt="Left">
            <area shape="rect" coords="176,239,212,267" href="?com=Y&name=Right" alt="Right">
            <area shape="rect" coords="29,328,88,356" href="?com=Z&name=Previous" alt="Previous">
            <area shape="rect" coords="91,327,136,357" href="?com=b&name=Stop" alt="Stop">
            <area shape="rect" coords="163,326,213,356" href="?com=a&name=Play-Pause" alt="Play / Pause">
            <area shape="rect" coords="223,325,279,355" href="?com=Y&name=Next" alt="Next">
          </map>
          <map name="remote2Map">
            <area shape="rect" coords="31,8,66,45" href="?com=V&name=Vol Down" alt="Volume Decrease">
          </map>
          <map name="remote3Map">
            <area shape="rect" coords="17,12,81,52" href="?com=W&name=Mute" alt="Mute " 
		onMouseOver="MM_swapImage('speaker','','../remote_mute.jpg',1)" onMouseOut="MM_swapImgRestore()">
          </map>
          <map name="remote4Map">
            <area shape="rect" coords="40,11,89,50" href="?com=U&name=Vol Up"  alt="Volume Increase">
          </map> <map name="remote5Map">
          <area shape="rect" coords="30,11,84,38" href="?com=C&name=P1" alt="Preset 1">
          <area shape="rect" coords="110,12,181,41" href="?com=D&name=P2" alt="Preset 2">
          <area shape="rect" coords="212,12,271,41" href="?com=E&name=P3" alt="Preset 3">
          <area shape="rect" coords="29,62,86,94" href="?com=F&name=P4" alt="Preset 4">
          <area shape="rect" coords="114,66,187,92" href="?com=G&name=P5" alt="Preset 5">
          <area shape="rect" coords="213,67,277,91" href="?com=H&name=P6" alt="Preset 6">
          <area shape="rect" coords="33,118,86,144" href="?com=I&name=P7" alt="Preset 7">
          <area shape="rect" coords="116,120,194,146" href="?com=J&name=P8" alt="Preset 8">
          <area shape="rect" coords="216,120,276,145" href="?com=K&name=P9" alt="Preset 9">
          <area shape="rect" coords="171,164,228,187" href="?com=j&name=Call..." alt="Call Preset">
          <area shape="rect" coords="26,159,90,188" href="?com=k&name=-/--" alt="-/--">
          <area shape="rect" coords="112,163,167,187" href="?com=L&name=P10" alt="Preset 10">
          <area shape="rect" coords="230,161,277,186" href="?com=i&name=Store" alt="Store">
      </map> 
      <script type="text/javascript">setTimeout(initAjaxControls,200);</script>
..END
else
    echo "<div>Sharpfin-lircd seems to be NOT installed, therefore it is not possible to use the the Remote Control Interface designed by gforums.<br>Please get the newest version of Sharpfin-lircd from <a href=\"http://www.pschmidt.it/sharpfin/index.php?title=Releases#Lirc\">here</a>.</div>";
fi
</BODY></HTML>
