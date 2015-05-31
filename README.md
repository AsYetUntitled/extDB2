## Arma3 Extension DB2
Arma3 Database Extension for Windows + Linux.

#### Donations
If you link to donate to extDB2 Developement use donate button below.
Don't forget to leave message if any features you would like to see implemented.  

<a href="https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=2SUEFTGABTAM2"><img src="https://www.paypalobjects.com/en_US/i/btn/btn_donate_LG.gif" alt="[paypal]" />

 
#### Known Public Missions / Mods using extDB2
http://www.exilemod.com   (Coming Soon)
  

#### Known Public Missions / Mods using older extDB  
http://www.altisliferpg.com  
http://a3wasteland.com  


### Features

 - ASync + Sync Support
 - Unique ID for fetching Results
 - Multi-Part Messages
 - Arma2 Legacy randomize configfile support  

 - Commandline Arguments Support
 - Rcon Support
 - Rcon Whitelisting + Kicking for Bad Playernames
 - RemoteTCP Support to send/receive text from extDB2  
 - Steam VAC + Friends Queries  


### Supported Backends

 - MySQL
 - SQLite
 - HTTP
 

 ### No Longer Supported Backends

 - Redis
 

#### Protocols

 - HTTP_RAW (Support for Auth, GET, POST)
 - SQL_CUSTOM    (Ability to define sql prepared statements in a .ini file)
 - SQL_CUSTOM_V2 (Ability to define sql prepared statements in a .ini file)
 - SQL_RAW
 - LOG (Custom Logfiles)
 - MISC (has beguid, crc32, md4/5, time + time offset)
 - RCON (Ability to whitelist allowed commands)
 - STEAM    (Ability to Query Steam for VAC Bans / Friend Info)
 - STEAM_V2 (Ability to Query Steam for VAC Bans / Friend Info)


#### Documentation @  
https://github.com/Torndeco/extDB2/wiki
  
  
#### Linux Requirements  
Linux Distro with Glibc 2.17 or higher  
Debian 8 / Centos 7 / Ubuntu 14.10  

#### Windows Requirements  
Windows Server 2008 + Later  
Windows 7 + Later  

Install vcredist_x86.exe  
http://www.microsoft.com/en-ie/download/details.aspx?id=40784  

#### Donation Link @  

https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=2SUEFTGABTAM2

 

#### Thanks to

 - [firefly2442](https://github.com/firefly2442) for the CMake Build System.
 - [MaHuJa](https://github.com/MaHuJa) for taking time to look over the code and fixing / improving the code.
 - [bladez-](https://github.com/bladez-) For code to encode BERcon packets from bercon.
 - [Fank](https://gist.github.com/Fank) for his code to convert SteamID to BEGUID. 
 - [Gabime](https://github.com/gabime) for Spdlog.
 - [rajkosto](https://github.com/rajkosto) for his work on DayZ Hive, using same code for parsering arma values.
 - [killzonekid](http://killzonekid.com) for his blog.
 - [Tonic](https://github.com/TAWTonic) & Atlis RPG Admins for beening literally beening bleeding edge testers for extDB.   
