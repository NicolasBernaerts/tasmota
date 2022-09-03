/*
  xdrv_96_ftp_server.ino - Simple FTP server
  Heavily based on FTPClientSevrer from dplasa (https://github.com/dplasa/FTPClientServer)
  
  Copyright (C) 2021  Nicolas Bernaerts
    02/11/2021 - v1.0 - Creation 
    19/11/2021 - v1.1 - Tasmota 10 compatibility 
    0'/08/2022 - v1.2 - Manual start thru ftp_start

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*************************************************\
 *                FTP Server
\*************************************************/

#ifndef FIRMWARE_SAFEBOOT
#ifdef USE_UFILESYS
#ifdef USE_FTPSERVER

#define XDRV_96                   96

#include <FTPServer.h>

FTPServer *ftp_server = nullptr;

// commands : MQTT
#define FTP_SERVER_LOGIN          "teleinfo"
#define FTP_SERVER_PASSWORD       "teleinfo"

// FTP - MQTT commands
const char kFTPServerCommands[] PROGMEM = "ftp_" "|" "help" "|" "start" "|" "stop";
void (* const FTPServerCommand[])(void) PROGMEM = { &CmndFTPServerHelp, &CmndFTPServerStart, &CmndFTPServerStop };

/***********************************************************\
 *                      Commands
\***********************************************************/

// FTP server help
void CmndFTPServerHelp ()
{
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: FTP server commands :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ftp_start = start FTP server on port %u"), FTP_CTRL_PORT);
  AddLog (LOG_LEVEL_INFO, PSTR (" - ftp_stop  = stop FTP server"));
  AddLog (LOG_LEVEL_INFO, PSTR ("   Server allows only 1 concurrent connexion"));
  AddLog (LOG_LEVEL_INFO, PSTR ("   Please configure your FTP client accordingly to connect"));
  ResponseCmndDone();
}

// Start FTP server
void CmndFTPServerStart ()
{
  // if FTP server not created, create it
  if (ftp_server == nullptr) ftp_server = new FTPServer (LittleFS);

  // if server exists, start it with login/ pwd
  if (ftp_server != nullptr) 
  {
    ftp_server->begin (FTP_SERVER_LOGIN, FTP_SERVER_PASSWORD);
    AddLog (LOG_LEVEL_INFO, PSTR ("FTP: Server started on port %u"), FTP_CTRL_PORT);
  }

  // answer
  if (ftp_server != nullptr) ResponseCmndDone (); else ResponseCmndFailed ();
}

// Start FTP server
void CmndFTPServerStop ()
{
  // if server exists, stop it
  if (ftp_server != nullptr) 
  {
    ftp_server->stop ();
    AddLog (LOG_LEVEL_INFO, PSTR ("FTP: Server stopped"));
  }

  // answer
  if (ftp_server != nullptr) ResponseCmndDone (); else ResponseCmndFailed ();
}

/***********************************************************\
 *                      Functions
\***********************************************************/

// init main status
void FTPServerInit ()
{
  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ftp_help to get help on FTP Server commands"));
}

// FTP server connexion management
void FTPServerLoop ()
{
  // if FTP server not created
  if (ftp_server != nullptr) ftp_server->handleFTP ();
}

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xdrv96 (uint8_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
    case FUNC_INIT:
      FTPServerInit ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kFTPServerCommands, FTPServerCommand);
      break;
    case FUNC_EVERY_50_MSECOND:
      FTPServerLoop ();
      break;
  }
  
  return result;
}

#endif    // USE_FTPSERVER
#endif    // USE_UFILESYS
#endif    // FIRMWARE_SAFEBOOT

