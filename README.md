## Arma3 Extension DB  C++ (windows / linux)

I got bored waiting on Arma 2017 / Epoch for Arma3.  
So i decided to write up an C++ Extension for Arma3server.

 
#### Known Public Missions / Mods using older extDB  
http://www.altisliferpg.com  
http://a3wasteland.com/  


#### Known Public Missions / Mods using extDB2
None  
  
  
### Features

 - ASync Support
 - Unique ID for Messages  
 - Multi-Part Messages  
 - Rcon Support  
 - Steam VAC + Friends Queries  
 - MySQL + SQLite Support  
 - RemoteTCP Support to send/receive text from extDB2  
 - Arma2 Legacy randomize configfile support  


#### Protocols

 - SQL_CUSTOM (Ability to define sql prepared statements in a .ini file)
 - SQL_RAW
 - LOG (Ability to log info into custom log files)
 - MISC (has beguid, crc32, md4/5, time + time offset)
 - RCon (Send Server Rcon messages + ability to whitelist them aswell)
 - STEAM (Ability to Query Steam for VAC Bans / Friend Info)


#### WIP

 - Improve TCPServer code security wise  
 - Improve RCon Code
 - Redis Support Coming Soon  

  
#### Known Issues
 - https://github.com/Torndeco/extDB2/wiki/Known-Issues
  
  
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

 - bladez- For code to encode BERcon packets from https://github.com/bladez-/bercon for RCON  
 - Fank for his code to convert SteamID to BEGUID https://gist.github.com/Fank/11127158
 - rajkosto for his work on DayZ Hive, using almost the exact same boost parser for sanitize checks for input/output https://github.com/rajkosto/hive   
 - firefly2442 for the CMake Build System & wiki updates https://github.com/firefly2442  
 - MaHuJa for fixing Test Application Input, no longer hardcoded input limit https://github.com/MaHuJa  
 - killzonekid for his blog http://killzonekid.com/  
 - Tonic & Atlis RPG Admins for beening literally beening bleeding edge testers for extDB. https://github.com/TAWTonic  
 - Gabime for Spdlog https://github.com/gabime/spdlog  
 - killerty69 for fix loadbans after AutoBan player  