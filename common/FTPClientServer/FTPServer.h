/*
 * FTP SERVER FOR ESP8266/ESP32
 * based on FTP Serveur for Arduino Due and Ethernet shield (W5100) or WIZ820io (W5200)
 * based on Jean-Michel Gallego's work
 * modified to work with esp8266 SPIFFS by David Paiva (david@nailbuster.com)
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

#ifndef FTP_SERVER_H
#define FTP_SERVER_H

/*******************************************************************************
 **                                                                            **
 **                       DEFINITIONS FOR FTP SERVER/CLIENT                    **
 **                                                                            **
 *******************************************************************************/
#include "FTPCommon.h"

class FTPServer : public FTPCommon
{
public:
  // contruct an instance of the FTP server using a
  // given FS object, e.g. SPIFFS or LittleFS
  FTPServer(FS &_FSImplementation);

  // starts the FTP server with username and password,
  // either one can be empty to enable anonymous ftp
  void begin(const String &uname, const String &pword);

  // stops the FTP server
  void stop();

  // needs to be called frequently (e.g. in loop() )
  // to process ftp requests
  void handleFTP();

private:
  enum internalState
  {
    cInit = 0,
    cWait,
    cCheck,
    cUserId,
    cPassword,
    cLoginOk,
    cProcess,

    tIdle,
    tRetrieve,
    tStore
  };

  void iniVariables();
  void disconnectClient(bool gracious = true);
  int8_t processCommand();
  virtual void closeTransfer();
  void abortTransfer();

  virtual int8_t dataConnect();

  void sendMessage_P(int16_t code, PGM_P fmt, ...);
  String getPathName(const String &param, bool includeLast = false);
  String getFileName(const String &param, bool fullFilePath = false);
  String makeDateTimeStr(time_t fileTime);
  int8_t readChar();

  // server specific
  bool dataPassiveConn = true; // PASV (passive) mode is our default
  String _FTP_USER;            // usename
  String _FTP_PASS;            // password
  uint32_t command;            // numeric command code of command sent by the client
  String cmdLine;              // command line as read from client
  String cmdString;            // command as textual representation
  String parameters;           // parameters sent by client
  String cwd;                  // the current directory
  String rnFrom;               // previous command was RNFR, this is the source file name

  internalState cmdState,      // state of ftp control connection
  transferState;               // state of ftp data connection
};

#endif // FTP_SERVER_H
