# About

This project aims to revive the Sharpfin project (now hosted at http://www.sharpfin.org).  

Please help me to revive this great project. We need to adapt the code to new firmwares and test it.  
Did you also like this project and didn't believe that this project quit so immediately? Then you may be interested in helping me to improve and extend this code.

This is for developers only. No warranty!
 
Thx for all contributors  
Happy Hacking!  

** WIKI ** [Sharpfin Wiki](http://www.sharpfin.org "Sharpfin Wiki")


# Features  
* Web frontend
* Web-server
* FTP-server
* Firmware upload
* Firmware patching
* Some Custom Apps

# Requirements

Software:  
- Cygwin/windres to build the patchserver  
- gcc / crosstools
- reciva sources from http://copper.reciva.com/sources/ 


Hardware:  
- it should work on most reciva (internet) radios

# Installation and First Steps
* Clone this repository:  
    git clone https://github.com/philsmd/sharpfin.git  
* Build patchserver:   
    cd patchserver  
    make
* Build sharpflash:  
    cd sharpflash    
    make  

# Hacking

* Add more features to the web-frontend
* Adapt to new firmwares
* Improve the security of the webserver (e.g. user-management or at least password-management)
* and,and,and (add your suggestions here, but please help also to implement them)

# Credits and Contributors 
Credits go to the main developers of the former Sharpfin project, among others:
  
* Steve Clarke
* Ico Doornekamp

(Note: there are many, many developers that are involved in some libraries and other tools used by Sharpfin, they can be found in the according header/source files, if I missed somebody there please notify me)

# License

This project is lincensed under the **GNU GENERAL PUBLIC LICENSE version 3**.  
If you find some code/headers within this repository NOT compatible w/ this license, please let me know immediately. I didn't write all of this code, but had a deep look at all libraries and source files contained.

Thanks.
