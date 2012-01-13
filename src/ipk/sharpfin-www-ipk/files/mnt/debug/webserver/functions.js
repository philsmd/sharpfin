/*
 *  Sharpfin project
 *  Copyright (C) by Steve Clarke and Ico Doornekamp
 *  2011-11-30 Philipp Schmidt
 *    Added to github
 * 
 *  This file is part of the sharpfin project
 * 
 *  This Library is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 * 
 *  This Library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this source files. If not, see
 *  <http://www.gnu.org/licenses/>.
 */

// utility function to insert a DOM child after a specific child
function insertAfter(target,reference) { 
	reference.parentNode.insertBefore(target,reference.nextSibling); 
} 

// functions to preload images
function MM_preloadImages() {
	var d=document; if(d.images){ if(!d.MM_p) d.MM_p=new Array();
	var i,j=d.MM_p.length,a=MM_preloadImages.arguments; for(i=0; i<a.length; i++)
	if (a[i].indexOf("#")!=0){ d.MM_p[j]=new Image; d.MM_p[j++].src=a[i];}}
}

function MM_swapImgRestore() {
	var i,x,a=document.MM_sr; for(i=0;a&&i<a.length&&(x=a[i])&&x.oSrc;i++) x.src=x.oSrc;
}

function MM_findObj(n, d) {
	var p,i,x; if(!d) d=document; if((p=n.indexOf("?"))>0&&parent.frames.length) {
	d=parent.frames[n.substring(p+1)].document; n=n.substring(0,p);}
	if(!(x=d[n])&&d.all) x=d.all[n]; for (i=0;!x&&i<d.forms.length;i++) x=d.forms[i][n];
	for(i=0;!x&&d.layers&&i<d.layers.length;i++) x=MM_findObj(n,d.layers[i].document);
	if(!x && d.getElementById) x=d.getElementById(n); return x;
}

function MM_swapImage() { //v3.0
	var i,j=0,x,a=MM_swapImage.arguments; document.MM_sr=new Array; for(i=0;i<(a.length-2);i+=3)
	if ((x=MM_findObj(a[i]))!=null){document.MM_sr[j++]=x; if(!x.oSrc) x.oSrc=x.src; x.src=a[i+2];}
}

// Creates a XhttpRequest object (compatible w/ most of the browsers)
function xhttp() {
    var xmlhttp=false;
    if (typeof XMLHttpRequest!='undefined') {
            xmlhttp=new XMLHttpRequest();
    }
    if (!xmlhttp) {
        if (window.ActiveXObject){
            try {
                xmlhttp=new ActiveXObject("Msxml2.XMLHTTP");
                xmlhttp.setRequestHeader("If-Modified-Since","Sat, 12 Jan 2008 00:00:00 GMT");
            } catch (e1){
                try{
                    xmlhttp=new ActiveXObject("Microsoft.XMLHTTP");
                    xmlhttp.setRequestHeader("If-Modified-Since","Sat, 12 Jan 2008 00:00:00 GMT");
                } catch(e2) {
                    xmlhttp=new Object();
                }
            }   
        } else {
            xmlhttp = new Object();
        }
    }
    return xmlhttp;
}

// Helper function to make a XhttpRequest (Ajax)
function sendXhttpRequest(method,targetURL,callback) {
	var xh=xhttp();
	var symbol="?";
	if (targetURL.indexOf(symbol)!=-1) {
	    symbol="&";
	}
	xh.open(method,targetURL+symbol+((new Date()).getTime()),true);
	xh.onreadystatechange=function() {
	    if (xh.readyState==4) {
	        if (xh.status==200) {
		    callback(true,xh.responseText);
	        } else {
                    callback(false,null);
	        }
	    }
	};
	xh.send(undefined);
}

// send IR SIMULATE COMMAND
function sendLircSimulateCmd(e) {
	var targ;
	if (!e) {
		e=window.event;
	}
	if (e.target) {
		targ=e.target;
	} else if (e.srcElement) {
		targ=e.srcElement;
	}
	if (targ.nodeType==3) {  // for Safari
		targ=targ.parentNode;
	}
	if (typeof targ=="undefined"||targ==null) {
		return;
	}
	sendXhttpRequest("GET","/cgi-bin/admin_Tools_Remote_Control.cgi"+targ.getAttribute("href")+"&ajax=1",getLircServerResponse)
	return false;
}

// handle the response from the Sharpfin-lircd daemon
function getLircServerResponse(success,responseText) {
	var statusDiv=document.getElementById('lircStatus');
	var statusDivSuffix="<br/>";
	if (typeof statusDiv=="undefined"||statusDiv==null) {
		statusDiv=document.createElement("div");
		statusDiv.id="lircStatus";
		var refChild=document.getElementById('pageTitle');
		if (typeof refChild=="undefined"||refChild==null) {
			refChild=parent.frames[1].document.getElementById("pageTitle");
		}
		insertAfter(statusDiv,refChild);
	}
	if (!success) {
		statusDiv.style.color="red";
		statusDiv.innerHTML="Error while sending the request to the web/lirc server, no meaningfull response received"+statusDivSuffix;
	} else {
		if (responseText.indexOf("successfully")!=-1) {
			statusDiv.style.color="green";
		} else {
			statusDiv.style.color="red";
		}
		statusDiv.innerHTML=responseText+statusDivSuffix;
	}
}

function initAjaxControls() {
	var elements=document.getElementsByTagName("area");
	if (typeof elements=="undefined"||elements==null) {
		return;
	}
	for (var i=0;i<elements.length;i++) {
		elements[i].onclick=sendLircSimulateCmd;
	}
}
