## Running an Exile Server Looking for Linux Extension ?
Grab v65 at
https://github.com/Torndeco/extDB2/releases/tag/65

## Arma3 Extension DB2
Arma3 Database + Rcon Extension for both Windows + Linux.  

<a href="https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=2SUEFTGABTAM2"><img src="https://www.paypalobjects.com/en_US/i/btn/btn_donate_LG.gif" alt="[paypal]" />

#### Public Missions / Mods using extDB2
http://www.exilemod.com  
http://a3wasteland.com
https://github.com/MrEliasen/Supremacy-Framework
http://www.altisliferpg.com  

#### Features

 - ASync + Sync Support
 - Unique ID for fetching Results
 - Multi-Part Messages
 - Arma2 Legacy randomize configfile support  

 - Commandline Arguments Support
 - Rcon Support
 - Rcon Whitelisting + Kicking for Bad Playernames
 - Steam VAC + Friends Queries  


#### Supported Backends

 - MySQL
 - SQLite

#### Protocols

 - SQL_CUSTOM_V2 (Ability to define sql prepared statements in a .ini file)
 - SQL_RAW
 - LOG (Custom Logfiles)
 - MISC (has beguid, crc32, md4/5, time + time offset)
 - RCON (Ability to whitelist allowed commands)
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


##### Donations
If you link to donate to extDB2 Developement use donate button above.  
Don't forget to leave message if any features you would like to see implemented.  


#### Thanks to

 - [firefly2442](https://github.com/firefly2442) for the CMake Build System.
 - [MaHuJa](https://github.com/MaHuJa) for taking time to look over the code and fixing / improving the code.
 - [bladez-](https://github.com/bladez-) For the original Rcon code, made my life alot easier.
 - [Fank](https://gist.github.com/Fank) for his code to convert SteamID to BEGuid.
 - [Gabime](https://github.com/gabime) for Spdlog Logging Library.
 - [rajkosto](https://github.com/rajkosto) for his work on DayZ Hive, using same code for sanitize checks.
 - [Tonic](https://github.com/TAWTonic) & Altis RPG Admins for initial testing of extDB etc.
