/*
  xdrv_96_ftp_server.ino - Simple FTP server
  Heavily based on FTPClientSevrer from dplasa (https://github.com/dplasa/FTPClientServer)
  
  This FTP micro FTP server only accepts 1 concurrent connexion.
  Make sure ton configure your FTP client according to that limit or your connexion will fail.
   
  Default login / paswword is ftp / password
  For Tasmota Teleinfo version it is teleinfo / teleinfo
 
  Copyright (C) 2021  Nicolas Bernaerts
    02/11/2021 - v1.0 - Creation 
    19/11/2021 - v1.1 - Tasmota 10 compatibility 
    04/08/2022 - v1.2 - Manual start thru ftp_start
    16/09/2022 - v1.3 - Add ftp_status

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

#ifdef USE_UFILESYS
#ifdef USE_FTPSERVER

#define XDRV_96                   96

#include <FTPServer.h>

// commands : MQTT
#ifndef FTP_SERVER_LOGIN
#define FTP_SERVER_LOGIN          "ftp"
#endif

#ifndef FTP_SERVER_PASSWORD
#define FTP_SERVER_PASSWORD       "password"
#endif

// FTP - MQTT commands
const char kFTPServerCommands[] PROGMEM = "ftp_" "|" "help" "|" "start" "|" "stop" "|" "status";
void (* const FTPServerCommand[])(void) PROGMEM = { &CmndFTPServerHelp, &CmndFTPServerStart, &CmndFTPServerStop, &CmndFTPServerStatus };

// FTP server instance
FTPServer *ftp_server = nullptr;

/***********************************************************\
 *                      Commands
\***********************************************************/

// FTP server help
void CmndFTPServerHelp ()
{
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: FTP server commands :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ftp_status = status (running port or 0 if not running)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ftp_start  = start FTP server on port %u"), FTP_CTRL_PORT);
  AddLog (LOG_LEVEL_INFO, PSTR (" - ftp_stop   = stop FTP server"));
  AddLog (LOG_LEVEL_INFO, PSTR ("   Server allows only 1 concurrent connexion"));
  AddLog (LOG_LEVEL_INFO, PSTR ("   Please configure your FTP client accordingly to connect"));
  ResponseCmndDone ();
}

// Start FTP server
void CmndFTPServerStart ()
{
  bool done = false;

  // if FTP server not created, create it
  if (ftp_server == nullptr)
  {
    // create server
    ftp_server = new FTPServer (LittleFS);

    // if server created, start it with login/ pwd
    if (ftp_server != nullptr) 
    {
      ftp_server->begin (FTP_SERVER_LOGIN, FTP_SERVER_PASSWORD);
      done = true;
      AddLog (LOG_LEVEL_INFO, PSTR ("FTP: Server started on port %u"), FTP_CTRL_PORT);
    }
  }

  // answer
  if (done) ResponseCmndDone ();
    else ResponseCmndFailed ();
}

// Start FTP server
void CmndFTPServerStop ()
{
  bool done = false;

  // if server exists, stop it
  if (ftp_server != nullptr) 
  {
    ftp_server->stop ();
    delete (ftp_server);
    ftp_server = nullptr;
    done = true;
    AddLog (LOG_LEVEL_INFO, PSTR ("FTP: Server stopped"));
  }

  // answer
  if (done) ResponseCmndDone ();
    else ResponseCmndFailed ();
}

// Status of FTP server
void CmndFTPServerStatus ()
{
  if (ftp_server != nullptr) ResponseCmndNumber (FTP_CTRL_PORT);
    else ResponseCmndNumber (0);
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

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xdrv96 (uint32_t function)
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
      if (ftp_server != nullptr) ftp_server->handleFTP ();
      break;
  }
  
  return result;
}

#endif    // USE_FTPSERVER
#endif    // USE_UFILESYS


