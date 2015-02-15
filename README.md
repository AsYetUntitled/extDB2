## Arma3 Extension DB  C++ (windows / linux)
  
Code is currently getting reviewed all over again + rewriting parts.  
New version will pretty much break all existing SQF Code, but it should only require some minor changes to SQF Code.  

Goal is cleanup code and improve performance, already got some minor improvements done.  
New features i.e INSERT_ID for DB_CUSTOM_V5 & support for Dynamic SQF (A3Wasteland).    
Also remote Server SQF Execution via Apache Thrift.  


#### Documentation @  
https://github.com/Torndeco/extDB/wiki

#### Linux Requirements  
Linux Distro with Glibc 2.17 or higher  
Debian 8 / Centos 7 / Ubuntu 14.10  

#### Windows Requirements  
Windows Server 2008 + Later  
Windows 7 + Later  

Install vcredist_x86.exe @ http://www.microsoft.com/en-ie/download/details.aspx?id=40784  



#### Thanks to

 - bladez- Using modified code from https://github.com/bladez-/bercon for RCON  
 - Fank for his code to convert SteamID to BEGUID https://gist.github.com/Fank/11127158
 - rajkosto for his work on DayZ Hive, using almost the exact same boost parser for sanitize checks for input/output https://github.com/rajkosto/hive  
 - firefly2442 for the CMake Build System & wiki updates https://github.com/firefly2442
 - MaHuJa for fixing Test Application Input, no longer hardcoded input limit https://github.com/MaHuJa
 - killzonekid for his blog http://killzonekid.com/
 - Tonic & Atlis RPG Admins for beening literally beening bleeding edge testers for extDB. https://github.com/TAWTonic
 - Gabime for Spdlog https://github.com/gabime/spdlog



##### Donate
[PayPal Donate Link](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=2SUEFTGABTAM2)  
If you like to donate, all development is done on a Dedicated Linux Server. Server cost is currently 50 Euro a month
