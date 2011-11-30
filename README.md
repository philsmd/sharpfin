# About

This project aims to revive the sharpfin project (http://reciva-users.wikispot.org/Sharpfinquickguide at its time hosted on http://zevv.nl - sharpfin.zevv.nl).
This is a slightly modified version of the full SVN repository (at least this is what the main authors told me).
Please help as much as you can to revive this great project.
We need to adapt the code to new firmwares and test it.
Afterwards we could try to create new apps and other cool features (e.g. improve and add features to the web-frontend).

Did you also like this project and didn't believe that this project quit so immediately? Then you maybe are interested in helping me to improve and extend this code.

This is for developers only. No warranty!
I'm NOT one of the main developers but I was kindly provided by the whole code of the old sharpfin project (so don't ask difficult questions, I'm NOT yet expert of the code).

Thx for all contributors
Happy Hacking!

P.S. Please also help to clear copyright and licensing issues


# Features  
* Web frontend
* Web-server
* FTP-server
* Firmware upload
* Firmware patching
* Some Apps
* ... (please help to complete this list)

# Requirements

Software:  
- windres  
- gcc/mingw  
- reciva sources from http://copper.reciva.com/sources/ 


Hardware:  
- it should work on most reciva (internet) radios but also on others (please help test) ?

# Installation and First Steps
* Clone this repository:  
    git clone https://github.com/philsmd/sharpfin.git  
* Build patchserver:   
    cd patchserver  
    make
* Build sharpflash:  
    cd sharpflash    
    make  

(Please help to write full instructions to build and use the Web-server and build patch files)

# Hacking

* Add more features to the web-frontend
* Adapt to new firmwares
* Improve the security of the webserver (e.g. user-management or at least password-management)
* and,and,and (add your suggestions here, but please help also to implement them)

# Credits and Contributors 
Credits go to all developers of the following libraries:
  
* Steve Clarke
* Ico Doornekamp

(Note: there are many, many developers that are involved in some libraries and other tools used by sharpfin, they can be found in the according header/source files, if I missed somebody there please notify me)

# License

This project is lincensed under the **GNU GENERAL PUBLIC LICENSE version 3**.  
If you find some code/headers within this repository NOT compatible w/ this license, please let me know immediately. I didn't write all of this code, but had a quick (no it was deep) to all libraries (and most source files) contained, and also the main authors were not 100% sure about all parts. Help me verify it's compatibility or incompatibility and feel free to fork and contribute. 

Thanks.
