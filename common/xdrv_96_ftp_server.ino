/*
  xdrv_96_ftp_server.ino - Simple FTP server
  Heavily based on FTPClientSevrer from dplasa (https://github.com/dplasa/FTPClientServer)
  
  Copyright (C) 2021  Nicolas Bernaerts
    02/11/2021 - v1.0 - Creation 
    19/11/2021 - v1.1 - Tasmota 10 compatibility 

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

FTPServer *ftp_server = nullptr;

// commands : MQTT
#define FTP_SERVER_LOGIN          "teleinfo"
#define FTP_SERVER_PASSWORD       "teleinfo"


// Create FTP server
void FTPServerInit ()
{
  // if FTP server not created
  if (ftp_server == nullptr)
  {
    // create FTP server
    AddLog (LOG_LEVEL_INFO, PSTR ("FTP: Starting server ..."));
    ftp_server = new FTPServer (LittleFS);

    // start server with login/ pwd
    if (ftp_server != nullptr)
    {
      ftp_server->begin (FTP_SERVER_LOGIN, FTP_SERVER_PASSWORD);
      AddLog (LOG_LEVEL_INFO, PSTR ("FTP: Server started on port %u"), FTP_CTRL_PORT);
    }
  }
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
    case FUNC_LOOP:
      FTPServerLoop ();
      break;
  }
  
  return result;
}

#endif    // USE_FTPSERVER
#endif    // USE_UFILESYS
