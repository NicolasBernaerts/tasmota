/*
 * FTP Server for ESP8266/ESP32
 * based on FTP Serveur for Arduino Due and Ethernet shield (W5100) or WIZ820io (W5200)
 * based on Jean-Michel Gallego's work
 * modified to work with esp8266 SPIFFS by David Paiva david@nailbuster.com
 * modified to work with esp8266 LitteFS by Daniel Plasa dplasa@gmail.com
 * Also done some code reworks and all string contants are now in flash memory 
 * by using F(), PSTR() ... on the string literals.  
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "FTPServer.h"

#ifdef ESP8266
#include <ESP8266WiFi.h>
#elif defined ESP32
#include <WiFi.h>
#endif

#include "FTPCommon.h"
#include <stdlib.h>
#include <stdarg.h>

WiFiServer controlServer(FTP_CTRL_PORT);
WiFiServer dataServer(FTP_DATA_PORT_PASV);

// some constants
static const char aSpace[] PROGMEM = " ";
static const char aSlash[] PROGMEM = "/";

// constructor
FTPServer::FTPServer(FS &_FSImplementation) : FTPCommon(_FSImplementation)
{
  aTimeout.resetToNeverExpires();
}

void FTPServer::begin(const String &uname, const String &pword)
{
  _FTP_USER = uname;
  _FTP_PASS = pword;

  iniVariables();

  // Tells the ftp server to begin listening for incoming connections
  controlServer.begin();
  dataServer.begin();
}

void FTPServer::stop()
{
  abortTransfer();
  disconnectClient(false);
  controlServer.stop();
  dataServer.stop();

  FTPCommon::stop();
}

void FTPServer::iniVariables()
{
  // Default Data connection is Active
  dataPassiveConn = true;

  // Set the root directory
  cwd = FPSTR(aSlash);

  // init internal status vars
  cmdState = cInit;
  transferState = tIdle;
  rnFrom.clear();

  // reset control connection input buffer, clear previous command
  cmdLine.clear();
  cmdString.clear();
  parameters.clear();
  command = 0;

  // free any used fileBuffer
  freeBuffer();
}

void FTPServer::handleFTP()
{
  //
  // control connection state sequence is
  //  cInit
  //    |
  //    V
  //  cWait
  //    |
  //    V
  //  cCheck -----------+
  //    |               | (no username but password set)
  //    V               |
  //  cUserId ----------+---+
  //    |               |   |
  //    +<--------------+   |
  //    V                   | (no password set)
  //  cPassword             |
  //    |                   |
  //    +<------------------+
  //    V
  //  cLoginOk
  //    |
  //    V
  //  cProcess
  //
  if (cmdState == cInit)
  {
    if (control.connected())
    {
      abortTransfer();
      disconnectClient(false);
    }
    iniVariables();
    cmdState = cWait;
  }
  else if (cmdState == cWait) // FTP control server waiting for connection
  {
    if (controlServer.hasClient())
    {
      control = controlServer.available();

      // wait 10s for login command
      aTimeout.reset(10 * 1000);
      cmdState = cCheck;
    }
  }

  else if (cmdState == cCheck) // FTP control server check/setup control connection
  {
    if (control.connected()) // A client connected, say "220 Hello client!"
    {
      FTP_DEBUG_MSG("control server got connection from %s:%d",
                    control.remoteIP().toString().c_str(), control.remotePort());

      sendMessage_P(220, PSTR("(espFTP " FTP_SERVER_VERSION ")"));

      if (_FTP_USER.length())
      {
        cmdState = cUserId;
      }
      else if (_FTP_PASS.length())
      {
        cmdState = cPassword;
      }
      else
      {
        cmdState = cLoginOk;
      }
    }
  }

  else if (cmdState == cLoginOk) // tell client "Login ok!"
  {
    sendMessage_P(230, PSTR("Login successful."));
    aTimeout.reset(sTimeOutMs);
    cmdState = cProcess;
  }

  //
  // all other command states need to process commands froms control connection
  //
  else if (readChar() > 0)
  {
    // enforce USER than PASS commands before anything else except the FEAT command
    // that should be supported to indicate server features even before login
    if ((FTP_CMD(FEAT) != command) && (((cmdState == cUserId) && (FTP_CMD(USER) != command)) ||
                                       ((cmdState == cPassword) && (FTP_CMD(PASS) != command))))
    {
      sendMessage_P(530, PSTR("Please login with USER and PASS."));
      FTP_DEBUG_MSG("ignoring before login: command %s [%x], params='%s'", cmdString.c_str(), command, parameters.c_str());
      command = 0;
      return;
    }

    // process the command
    int8_t rc = processCommand();
    // returns
    // -1 : command processing indicates, we have to close control (e.g. QUIT)
    //  0 : not yet finished, just call processCommend() again
    //  1 : finished
    if (rc < 0)
    {
      cmdState = cInit;
    }
    if (rc > 0)
    {
      // clear current command, so readChar() can fetch the next command
      command = 0;

      // command was successful, update command state
      if (cmdState == cUserId)
      {
        if (_FTP_PASS.length())
        {
          // wait 10s for PASS command
          aTimeout.reset(10 * 1000);
          sendMessage_P(331, PSTR("Please specify the password."));
          cmdState = cPassword;
        }
        else
        {
          cmdState = cLoginOk;
        }
      }
      else if (cmdState == cPassword)
      {
        cmdState = cLoginOk;
      }
      else
      {
        aTimeout.reset(sTimeOutMs);
      }
    }
  }

  //
  // general connection handling
  // (if we have an established control connection)
  //
  if (cmdState >= cCheck)
  {
    // detect lost/closed by remote connections
    if (!control.connected() || !control)
    {
      cmdState = cInit;
      FTP_DEBUG_MSG("client lost or disconnected");
    }

    // check for timeout
    if (aTimeout.expired())
    {
      sendMessage_P(530, PSTR("Timeout."));
      cmdState = cInit;
    }

    // handle data file transfer
    if (transferState == tRetrieve) // Retrieve data
    {
      if (!doFiletoNetwork())
      {
        closeTransfer();
        transferState = tIdle;
      }
    }
    else if (transferState == tStore) // Store data
    {
      if (!doNetworkToFile())
      {
        closeTransfer();
        transferState = tIdle;
      }
    }
  }
}

void FTPServer::disconnectClient(bool gracious)
{
  FTP_DEBUG_MSG("Disconnecting client");
  abortTransfer();
  if (gracious)
  {
    sendMessage_P(221, PSTR("Goodbye."));
  }
  else
  {
    sendMessage_P(231, PSTR("Service terminated."));
  }
  control.stop();
}

int8_t FTPServer::processCommand()
{
  // assume successful operation by default
  int8_t rc = 1;

  // make the full path of parameters (even if this makes no sense for all commands)
  String path = getFileName(parameters, true);
  FTP_DEBUG_MSG("processing: command %s [%x], params='%s' (cwd='%s')", cmdString.c_str(), command, parameters.c_str(), cwd.c_str());

  ///////////////////////////////////////
  //                                   //
  //      ACCESS CONTROL COMMANDS      //
  //                                   //
  ///////////////////////////////////////

  //
  //  USER - Provide username
  //
  if (FTP_CMD(USER) == command)
  {
    if (_FTP_USER.length() && (_FTP_USER != parameters))
    {
      sendMessage_P(430, PSTR("User not found."));
      command = 0;
      rc = 0;
    }
    else
    {
      FTP_DEBUG_MSG("USER ok");
    }
  }

  //
  //  PASS - Provide password
  //
  else if (FTP_CMD(PASS) == command)
  {
    if (_FTP_PASS.length() && (_FTP_PASS != parameters))
    {
      sendMessage_P(430, PSTR("Password invalid."));
      command = 0;
      rc = 0;
    }
    else
    {
      FTP_DEBUG_MSG("PASS ok");
    }
  }

  //
  //  QUIT
  //
  else if (FTP_CMD(QUIT) == command)
  {
    disconnectClient();
    rc = -1;
  }

  //
  //  NOOP
  //
  else if (FTP_CMD(NOOP) == command)
  {
    sendMessage_P(200, PSTR("Zzz..."));
  }

  //
  //  CDUP - Change to Parent Directory
  //
  else if (FTP_CMD(CDUP) == command)
  {
    // up one level
    cwd = getPathName("", false);
    sendMessage_P(250, PSTR("Directory successfully changed to \"%s\"."), cwd.c_str());
  }

  //
  //  CWD - Change Working Directory
  //
  else if (FTP_CMD(CWD) == command)
  {
    if (parameters == F(".")) // 'CWD .' is the same as PWD command
    {
      command = FTP_CMD(PWD); // make CWD a PWD command ;-)
      rc = 0;                 // indicate we need another processCommand() call
    }
    else if (parameters == F("..")) // 'CWD ..' is the same as CDUP command
    {
      command = FTP_CMD(CDUP); // make CWD a CDUP command ;-)
      rc = 0;                  // indicate we need another processCommand() call
    }
    else
    {
#if (defined esp8266FTPServer_SPIFFS)
      // SPIFFS has no directories, it's always ok
      cwd = path;
      sendMessage_P(250, PSTR("Directory successfully changed."));
#else
      // check if directory exists
      file = THEFS.open(path, "r");
      if (file.isDirectory())
      {
        cwd = path;
        sendMessage_P(250, PSTR("Directory successfully changed."));
      }
      else
      {
        sendMessage_P(550, PSTR("Failed to change directory."));
      }
      file.close();
#endif
    }
  }

  //
  //  PWD - Print Directory
  //
  else if (FTP_CMD(PWD) == command)
  {
    sendMessage_P(257, PSTR("\"%s\" is the current directory."), cwd.c_str());
  }

  ///////////////////////////////////////
  //                                   //
  //    TRANSFER PARAMETER COMMANDS    //
  //                                   //
  ///////////////////////////////////////

  //
  //  MODE - Transfer Mode
  //
  else if (FTP_CMD(MODE) == command)
  {
    if (parameters == F("S"))
      sendMessage_P(504, PSTR("Only S(tream) mode is suported"));
    else
      sendMessage_P(200, PSTR("Mode set to S."));
  }

  //
  //  PASV - Passive data connection management
  //
  else if (FTP_CMD(PASV) == command)
  {
    // stop a possible previous data connection
    data.stop();
    // tell client to open data connection to our ip:dataPort
    dataPort = FTP_DATA_PORT_PASV;
    dataPassiveConn = true;
    String ip = control.localIP().toString();
    ip.replace(".", ",");
    sendMessage_P(227, PSTR("Entering Passive Mode (%s,%d,%d)."), ip.c_str(), dataPort >> 8, dataPort & 255);
    //sendMessage_P(227, PSTR("Entering Passive Mode (0,0,0,0,%d,%d)."), dataPort >> 8, dataPort & 255);
  }

  //
  //  PORT - Data Port, Active data connection management
  //
  else if (FTP_CMD(PORT) == command)
  {
    if (data)
      data.stop();

    if (parseDataIpPort(parameters.c_str()))
    {
      dataPassiveConn = false;
      sendMessage_P(200, PSTR("PORT command successful"));
      FTP_DEBUG_MSG("Data connection management Active, using %s:%u", dataIP.toString().c_str(), dataPort);
    }
    else
    {
      sendMessage_P(501, PSTR("Cannot interpret parameters."));
    }
  }

  //
  //  STRU - File Structure
  //
  else if (FTP_CMD(STRU) == command)
  {
    if (parameters == F("F"))
      sendMessage_P(504, PSTR("Only F(ile) is suported"));
    else
      sendMessage_P(200, PSTR("Structure set to F."));
  }

  //
  //  TYPE - Data Type
  //
  else if (FTP_CMD(TYPE) == command)
  {
    if (parameters == F("A"))
      sendMessage_P(200, PSTR("TYPE is now ASII."));
    else if (parameters == F("I"))
      sendMessage_P(200, PSTR("TYPE is now 8-bit Binary."));
    else
      sendMessage_P(504, PSTR("Unrecognised TYPE."));
  }

  ///////////////////////////////////////
  //                                   //
  //        FTP SERVICE COMMANDS       //
  //                                   //
  ///////////////////////////////////////

  //
  //  ABOR - Abort
  //
  else if (FTP_CMD(ABOR) == command)
  {
    abortTransfer();
    sendMessage_P(226, PSTR("Data connection closed"));
  }

  //
  //  DELE - Delete a File
  //
  else if (FTP_CMD(DELE) == command)
  {
    if (parameters.length() == 0)
      sendMessage_P(501, PSTR("No file name"));
    else
    {
      if (!THEFS.exists(path))
      {
        sendMessage_P(550, PSTR("Delete operation failed, file '%s' not found."), path.c_str());
      }
      else if (THEFS.remove(path))
      {
        sendMessage_P(250, PSTR("Delete operation successful."));
      }
      else
      {
        sendMessage_P(450, PSTR("Delete operation failed."));
      }
    }
  }

  //
  //  LIST - List directory contents
  //  MLSD - Listing for Machine Processing (see RFC 3659)
  //  NLST - Name List
  //
  else if ((FTP_CMD(LIST) == command) || (FTP_CMD(MLSD) == command) || (FTP_CMD(NLST) == command))
  {
    rc = dataConnect(); // returns -1: no data connection, 0: need more time, 1: data ok
    if (rc < 0)
    {
      sendMessage_P(425, PSTR("No data connection"));
      rc = 1; // mark command as processed
    }
    else if (rc > 0)
    {
      sendMessage_P(150, PSTR("Accepted data connection"));
      uint16_t dirCount = 0;

      // filter out possible command parameters like "-a", given by some clients
      // like FuseFS
      int8_t dashPos = path.lastIndexOf(F("-"));
      if (dashPos > 0)
      {
        path.remove(dashPos);
      }
      FTP_DEBUG_MSG("Listing content of '%s'", path.c_str());
#if (defined ESP8266)
      Dir dir = THEFS.openDir(path);
      while (dir.next())
      {
        file = dir.openFile("r");
#elif (defined ESP32)
      File dir = THEFS.open(path);
      file = dir.openNextFile();
      while (file)
      {
#endif
        bool isDir = file.isDirectory();
        String fn = file.name();
        uint32_t fs = file.size();
        String fileTime = makeDateTimeStr(file.getLastWrite());
        file.close();
        if (cwd == FPSTR(aSlash) && fn[0] == '/')
          fn.remove(0, 1);

        if (FTP_CMD(LIST) == command)
        {
          // unixperms  type userid   groupid      size time & date  name
          // drwxrwsr-x    2 111      117          4096 Apr 01 12:45 aDirectory
          // -rw-rw-r--    1 111      117        875315 Mar 23 17:29 aFile
          data.printf_P(PSTR("%crw%cr-%cr-%c    %c    0    0  %8" PRINTu32 " %s %s\r\n"),
                        isDir ? 'd' : '-',
                        isDir ? 'x' : '-',
                        isDir ? 'x' : '-',
                        isDir ? 'x' : '-',
                        isDir ? '2' : '1',
                        isDir ? 0 : fs,
                        fileTime.c_str(),
                        fn.c_str());
          //data.printf_P(PSTR("+r,s%lu\r\n,\t%s\r\n"), (uint32_t)dir.fileSize(), fn.c_str());
        }
        else if (FTP_CMD(MLSD) == command)
        {
          // "modify=20170122163911;type=dir;UNIX.group=0;UNIX.mode=0775;UNIX.owner=0; dirname"
          // "modify=20170121000817;size=12;type=file;UNIX.group=0;UNIX.mode=0644;UNIX.owner=0; filename"
          data.printf_P(PSTR("modify=%s;UNIX.group=0;UNIX.owner=0;UNIX.mode="), fileTime.c_str());
          if (isDir)
          {
            data.printf_P(PSTR("0755;type=dir; "));
          }
          else
          {
            data.printf_P(PSTR("0644;size=%" PRINTu32 ";type=file; "), fs);
          }
          data.printf_P(PSTR("%s\r\n"), fn.c_str());
        }
        else if (FTP_CMD(NLST) == command)
        {
          data.println(fn);
        }
        ++dirCount;
#if (defined ESP32)
        file = dir.openNextFile();
#endif
      }

      if (FTP_CMD(MLSD) == command)
      {
        control.println(F("226-options: -a -l\r\n"));
      }
      sendMessage_P(226, PSTR("%d matches total"), dirCount);
    }
    data.stop();
  }

  //
  //  RETR - Retrieve
  //
  else if (FTP_CMD(RETR) == command)
  {
    if (parameters.length() == 0)
    {
      sendMessage_P(501, PSTR("No file name"));
    }
    else
    {
      // open the file if not opened before (when re-running processCommand() since data connetion needs time)
      if (!file)
        file = THEFS.open(path, "r");
      if (!file)
      {
        sendMessage_P(550, PSTR("File \"%s\" not found."), parameters.c_str());
      }
      else if (file.isDirectory())
      {
        sendMessage_P(450, PSTR("Cannot open file \"%s\"."), parameters.c_str());
      }
      else
      {
        rc = dataConnect(); // returns -1: no data connection, 0: need more time, 1: data ok
        if (rc < 0)
        {
          sendMessage_P(425, PSTR("No data connection"));
          rc = 1; // mark command as processed
        }
        else if (rc > 0)
        {
          transferState = tRetrieve;
          millisBeginTrans = millis();
          bytesTransfered = 0;
          uint32_t fs = file.size();
          if (allocateBuffer())
          {
            FTP_DEBUG_MSG("Sending file '%s' (%lu bytes)", path.c_str(), fs);
            sendMessage_P(150, PSTR("%lu bytes to download"), fs);
          }
          else
          {
            closeTransfer();
            sendMessage_P(451, PSTR("Internal error. Not enough memory."));
          }
        }
      }
    }
  }

  //
  //  STOR - Store
  //
  else if (FTP_CMD(STOR) == command)
  {
    if (parameters.length() == 0)
    {
      sendMessage_P(501, PSTR("No file name."));
    }
    else
    {
      FTP_DEBUG_MSG("STOR '%s'", path.c_str());
      if (!file)
      {
        file = THEFS.open(path, "w"); // open file, truncate it if already exists
        file.close();                    // this performs a sync on LittleFS so that the actual
                                      // space used by the file in FS gets released
        file = THEFS.open(path, "w"); // re-open file for writing
      }
      if (!file)
      {
        sendMessage_P(451, PSTR("Cannot open/create \"%s\""), path.c_str());
      }
      else
      {
        rc = dataConnect(); // returns -1: no data connection, 0: need more time, 1: data ok
        if (rc < 0)
        {
          sendMessage_P(425, PSTR("No data connection"));
          file.close();
          rc = 1; // mark command as processed
        }
        else if (rc > 0)
        {
          transferState = tStore;
          millisBeginTrans = millis();
          bytesTransfered = 0;
          if (allocateBuffer())
          {
            FTP_DEBUG_MSG("Receiving file '%s' => %s", parameters.c_str(), path.c_str());
            sendMessage_P(150, PSTR("Connected to port %d"), dataPort);
          }
          else
          {
            closeTransfer();
            sendMessage_P(451, PSTR("Internal error. Not enough memory."));
          }
        }
      }
    }
  }

  //
  //  MKD - Make Directory
  //
  else if (FTP_CMD(MKD) == command)
  {
#if (defined esp8266FTPServer_SPIFFS)
    sendMessage_P(550, "Create directory operation failed."); //not support on SPIFFS
#else
    FTP_DEBUG_MSG("mkdir(%s)", path.c_str());
    if (THEFS.mkdir(path))
    {
      sendMessage_P(257, PSTR("\"%s\" created."), path.c_str());
    }
    else
    {
      sendMessage_P(550, PSTR("Create directory operation failed."));
    }
#endif
  }

  //
  //  RMD - Remove a Directory
  //
  else if (FTP_CMD(RMD) == command)
  {
#if (defined esp8266FTPServer_SPIFFS)
    sendMessage_P(550, "Remove directory operation failed."); //not support on SPIFFS
#else
    // check directory for files
#if (defined ESP8266)
    Dir dir = THEFS.openDir(path);
    if (dir.next())
    {
#elif (defined ESP32)
    File dir = THEFS.open(path);
    file = dir.openNextFile();
    if (file)
    {
      file.close();
#endif
      //only delete if dir is empty!
      sendMessage_P(550, PSTR("Remove directory operation failed, directory is not empty."));
    }
    else
    {
      THEFS.rmdir(path);
      sendMessage_P(250, PSTR("Remove directory operation successful."));
    }
#endif
  }
  //
  //  RNFR - Rename From
  //
  else if (FTP_CMD(RNFR) == command)
  {
    if (parameters.length() == 0)
      sendMessage_P(501, PSTR("No file name"));
    else
    {
      if (!THEFS.exists(path))
        sendMessage_P(550, PSTR("File \"%s\" not found."), path.c_str());
      else
      {
        sendMessage_P(350, PSTR("RNFR accepted - file \"%s\" exists, ready for destination"), path.c_str());
        rnFrom = path;
      }
    }
  }
  //
  //  RNTO - Rename To
  //
  else if (FTP_CMD(RNTO) == command)
  {
    if (rnFrom.length() == 0)
      sendMessage_P(503, PSTR("Need RNFR before RNTO"));
    else if (parameters.length() == 0)
      sendMessage_P(501, PSTR("No file name"));
    else if (THEFS.exists(path))
      sendMessage_P(553, PSTR("\"%s\" already exists."), parameters.c_str());
    else
    {
      FTP_DEBUG_MSG("Renaming '%s' to '%s'", rnFrom.c_str(), path.c_str());
      if (THEFS.rename(rnFrom, path))
        sendMessage_P(250, PSTR("File successfully renamed or moved"));
      else
        sendMessage_P(451, PSTR("Rename/move failure."));
    }
    rnFrom.clear();
  }

  ///////////////////////////////////////
  //                                   //
  //   EXTENSIONS COMMANDS (RFC 3659)  //
  //                                   //
  ///////////////////////////////////////

  //
  //  FEAT - New Features
  //
  else if (FTP_CMD(FEAT) == command)
  {
    control.print(F("211-Features:\r\n  MLSD\r\n  MDTM\r\n  SITE\r\n  SIZE\r\n211 End.\r\n"));
    command = 0; // clear command code and
    rc = 0;      // return 0 to prevent progression of state machine in case FEAT was a command before login
  }

  //
  //  MDTM - File Modification Time (see RFC 3659)
  //
  else if (FTP_CMD(MDTM) == command)
  {
    file = THEFS.open(path, "r");
    if ((!file) || (0 == parameters.length()))
    {
      sendMessage_P(550, PSTR("Unable to retrieve time"));
    }
    else
    {
      sendMessage_P(213, PSTR("%s"), makeDateTimeStr(file.getLastWrite()).c_str());
    }
    file.close();
  }

  //
  //  SIZE - Size of the file
  //
  else if (FTP_CMD(SIZE) == command)
  {
    file = THEFS.open(path, "r");
    if ((!file) || (0 == parameters.length()))
    {
      sendMessage_P(450, PSTR("Cannot open file."));
    }
    else
    {
      sendMessage_P(213, PSTR("%lu"), (uint32_t)file.size());
    }
    file.close();
  }

  //
  //  SITE - System command
  //
  else if (FTP_CMD(SITE) == command)
  {
    sendMessage_P(550, PSTR("SITE %s command not implemented."), parameters.c_str());
  }

  //
  //  SYST - System information
  //
  else if (FTP_CMD(SYST) == command)
  {
    sendMessage_P(215, PSTR("UNIX Type: L8"));
  }

  //
  //  Unrecognized commands ...
  //
  else
  {
    FTP_DEBUG_MSG("Unknown command: %s, params: '%s')", cmdString.c_str(), parameters.c_str());
    sendMessage_P(500, PSTR("unknown command \"%s\""), cmdString.c_str());
  }

  return rc;
}

int8_t FTPServer::dataConnect()
{
  int8_t rc = 1; // assume success

  if (!dataPassiveConn)
  {
    // active mode
    // open our own data connection
    return FTPCommon::dataConnect();
  }
  else
  {
    // passive mode
    // wait for data connection from the client
    if (!data.connected())
    {
      if (dataServer.hasClient())
      {
        data.stop();
        data = dataServer.available();
        FTP_DEBUG_MSG("Got incoming (passive) data connection from %s:%u", data.remoteIP().toString().c_str(), data.remotePort());
      }
      else
      {
        // give me more time waiting for a data connection
        rc = 0;
      }
    }
  }
  return rc;
}

void FTPServer::closeTransfer()
{
  uint32_t deltaT = (int32_t)(millis() - millisBeginTrans);
  if (deltaT > 0 && bytesTransfered > 0)
  {
    sendMessage_P(226, PSTR("File successfully transferred, %lu ms, %f kB/s."), deltaT, float(bytesTransfered) / deltaT);
  }
  else
    sendMessage_P(226, PSTR("File successfully transferred"));

  FTPCommon::closeTransfer();
}

void FTPServer::abortTransfer()
{
  if (transferState > tIdle)
  {
    file.close();
    data.stop();
    sendMessage_P(426, PSTR("Transfer aborted"));
  }
  freeBuffer();
  transferState = tIdle;
}

// Read a char from client connected to ftp server
//
//  returns:
//    -1 if cmdLine too long
//     0 cmdLine still incomplete (no \r or \n received yet)
//     1 cmdLine processed, command and parameters available

int8_t FTPServer::readChar()
{
  // only read/parse, if the previous command has been fully processed!
  if (command)
    return 1;

  while (control.available())
  {
    char c = control.read();
    // FTP_DEBUG_MSG("readChar() cmdLine='%s' <= %c", cmdLine.c_str(), c);

    // substitute '\' with '/'
    if (c == '\\')
      c = '/';

    // nl detected? then process line
    if (c == '\n' || c == '\r')
    {
      cmdLine.trim();

      // but only if we got at least chars in the line!
      if (0 == cmdLine.length())
        break;

      // search for space between command and parameters
      int pos = cmdLine.indexOf(FPSTR(aSpace));
      if (pos > 0)
      {
        parameters = cmdLine.substring(pos + 1);
        parameters.trim();
        cmdLine.remove(pos);
      }
      else
      {
        parameters.remove(0);
      }
      cmdString = cmdLine;

      // convert command to upper case
      cmdString.toUpperCase();

      // convert the (up to 4 command chars to numerical value)
      command = *(const uint32_t *)cmdString.c_str();

      // clear cmdline
      cmdLine.clear();
      // FTP_DEBUG_MSG("readChar() success, cmdString='%s' [%x], params='%s'", cmdString.c_str(), command, parameters.c_str());
      return 1;
    }
    else
    {
      // just add char
      cmdLine += c;
      if (cmdLine.length() > FTP_CMD_SIZE)
      {
        cmdLine.clear();
        sendMessage_P(500, PSTR("Line too long"));
      }
    }
  }
  return 0;
}

// Get the complete path from cwd + parameters or complete filename from cwd + parameters
//
// 3 possible cases: parameters can be absolute path, relative path or only the name
//
// returns:
//    path WITHOUT file-/dirname (fullname=false)
//    full path WITH file-/dirname (fullname=true)
String FTPServer::getPathName(const String &param, bool fullname)
{
  String tmp;

  // is param an absoulte path?
  if (param[0] == '/')
  {
    tmp = param;
  }
  else
  {
    // start with cwd
    tmp = cwd;

    // if param != "" then add param
    if (param.length())
    {
      if (!tmp.endsWith(FPSTR(aSlash)))
        tmp += '/';
      tmp += param;
    }
    // => tmp becomes cdw [ + '/' + param ]
  }

  // strip filename
  if (!fullname)
  {
    // search rightmost '/'
    int lastslash = tmp.lastIndexOf(FPSTR(aSlash));
    if (lastslash >= 0)
    {
      tmp.remove(lastslash);
    }
  }
  // sanetize:
  // "" -> "/"
  // "/some/path/" => "/some/path"
  while (tmp.length() > 1 && tmp.endsWith(FPSTR(aSlash)))
    tmp.remove(cwd.length() - 1);
  if (tmp.length() == 0)
    tmp += '/';
  return tmp;
}

// Get the [complete] file name from cwd + parameters
//
// 3 possible cases: parameters can be absolute path, relative path or only the filename
//
// returns:
//    filename or filename with complete path
String FTPServer::getFileName(const String &param, bool fullFilePath)
{
  // build the filename with full path
  String tmp = getPathName(param, true);

  if (!fullFilePath)
  {
    // strip path
    // search rightmost '/'
    int lastslash = tmp.lastIndexOf(FPSTR(aSlash));
    if (lastslash > 0)
    {
      tmp.remove(0, lastslash);
    }
  }

  return tmp;
}

//
// Formats printable String from a time_t timestamp
//
String FTPServer::makeDateTimeStr(time_t ft)
{
  String tmp;
  // a buffer with enough space for the formats
  char buf[25];
  char *b = buf;

  // break down the provided file time
  struct tm _tm;
  gmtime_r(&ft, &_tm);

  if (FTP_CMD(MLSD) == command)
  {
    // "%Y%m%d%H%M%S", e.g. "20200517123400"
    strftime(b, sizeof(buf), "%Y%m%d%H%M%S", &_tm);
  }
  else if (FTP_CMD(LIST) == command)
  {
    // "%h %d %H:%M", e.g. "May 17 12:34" for file dates of the current year
    // "%h %d  %Y"  , e.g. "May 17  2019" for file dates of any other years

    // just convert both ways, select later what's to be shown
    // buf becomes "May 17  2019May 17 12:34"
    strftime(b, sizeof(buf), "%h %d %H:%M%h %d  %Y", &_tm);

    // check for a year != year from now
    int fileYear = _tm.tm_year;
    time_t nowTime = time(NULL);
    gmtime_r(&nowTime, &_tm);
    if (fileYear == _tm.tm_year)
    {
      // cut off 2nd half - year variant
      b[12] = '\0';
    }
    else
    {
      // skip 1st half - time variant
      b += 12;
    }
  }
  tmp = b;
  return tmp;
}

//
//    send "code formatted string" + CR-LF
//
void FTPServer::sendMessage_P(int16_t code, PGM_P fmt, ...)
{
  FTP_DEBUG_MSG(">>> %d %s", code, fmt);

  int size = 0;
  char *p = NULL;
  va_list ap;

  /* Determine required size for a string buffer */
  va_start(ap, fmt);
  size = vsnprintf(p, size, fmt, ap);
  va_end(ap);

  if (size > 0)
  {
    p = (char *)malloc(++size); // increase +1 for '\0'
    if (p)
    {
      va_start(ap, fmt);
      size = vsnprintf(p, size, fmt, ap);
      va_end(ap);

      if (size > 0)
      {
        control.printf_P(PSTR("%d %s\r\n"), code, p);
      }
      free(p);
    }
  }
}
