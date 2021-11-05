# FTPServer and FTPClient
Simple FTP Server and Client for the esp8266/esp32 with SPIFFS and LittleFS support.

I've modified a FTP Server from arduino/wifi shield to work with the esp8266 and the esp32. This allows you to FTP into your esp8266/esp32 and access/modify files and directories on the FS. SPIFFS's approach to directories is somewhat limited (everything is a file albeit it's name may contain '/'-es). LittleFS (which is not yet available for the esp32) however has full directory support. 
So with a FTP Server working on SPIFFS there will be no create/modify directory support but with LittleFS there is!
The code ist tested it with command line ftp and Filezilla. 

The FTP Client is pretty much straight forward. It can upload (put, STOR) a file to a FTP Server or download (get, RETR) a file from a FTP Server. Both ways can be done blocking or non-blocking.

## Features
* Server supports both active and passive mode
* Client uses passive mode
* Client/Server both support LittleFS and SPIFFS
* Server (fully) supports directories with LittleFS
* Client supports directories with either filesystem 
  since both FS will just auto-create missing Directories
  when accessing files.

## Limitations
* Server only allows **one** ftp control and **one** data connection at a time. You need to setup Filezilla (or other clients) to respect that, i.e. only allow **1** connection. (In FileZilla go to File/Site Manager then select your site. In Transfer Settings, check "Limit number of simultaneous connections" and set the maximum to 1.) This limitation is also the reason why FuseFS based clients (e.g. curlftpfs) seem to work (i.e. listing directories) but will fail on file operations as they try to open a second control connection for that.

* It does not yet support encryption

## Compatibility
This library was tested against the 2.7.1 version of the esp8266 Arduino core library and the 1.0.4 version of the esp32 Arduino core.

## Server Usage

### Construct an FTPServer
Select the desired FS via the contructor 
```cpp
#include <FTPServer.h>
#include <LittleFS.h>

FTPServer ftpSrv(LittleFS); // construct with LittleFS
// or
FTPServer ftpSrv(SPIFFS);   // construct with SPIFFS if you need to for backward compatibility
```

### Set username/password
```cpp
ftpSrv.begin("username", "password");
```

### Handle by calling frequently
```cpp
ftpSrv.handleFTP(); // place this in e.g. loop()
```

## Client Usage

### Construct an FTPClient
Select the desired FS via the contructor 
```cpp
#include <FTPClient.h>
#include <LittleFS.h>

FTPClient ftpClient(LittleFS); // construct with LittleFS
// or
FTPClient ftpClient(SPIFFS);   // construct with SPIFFS if you need to for backward compatibility
```

### Provide username, password, server, port...
```cpp
// struct ServerInfo
// {
//     String login;
//     String password;
//     String servername;
//     uint16_t port;
// };

ServerInfo ftpServerInfo ("username", "password", "server_name_or_ip", 21);
ftpClient.begin(ftpServerInfo);
```

### Transfer a file
```cpp
ftpClient.transfer("local_file_path", "remote_file_path", FTPClient::FTP_GET);  // get a file blocking
ftpClient.transfer("local_file_path", "remote_file_path", FTPClient::FTP_PUT_NONBLOCKING);  // put a file non-blocking
```
### Handle non-blocking transfers by calling frequently
```cpp
ftpClient.handleFTP(); // place this in e.g. loop()
```

## Notes
* I forked the Server from https://github.com/nailbuster/esp8266FTPServer which itself was forked from: https://github.com/gallegojm/Arduino-Ftp-Server/tree/master/FtpServer
* Inspiration for the Client was taken from https://github.com/danbicks and his code posted in https://github.com/esp8266/Arduino/issues/1183#issuecomment-634556135
