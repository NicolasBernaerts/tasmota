#include "FTPClient.h"

FTPClient::FTPClient(FS &_FSImplementation) : FTPCommon(_FSImplementation)
{
  // set aTimeout to never expire, will be used later by ::waitFor(...)
  aTimeout.resetToNeverExpires();
}

void FTPClient::begin(const ServerInfo &theServer)
{
  _server = &theServer;
}

const FTPClient::Status &FTPClient::transfer(const String &localFileName, const String &remoteFileName, TransferType direction)
{
  _serverStatus.result = PROGRESS;
  if (ftpState >= cIdle)
  {
    _remoteFileName = remoteFileName;
    _direction = direction;

    if (direction & FTP_GET)
      file = THEFS.open(localFileName, "w");
    else if (direction & FTP_PUT)
      file = THEFS.open(localFileName, "r");

    if (!file)
    {
      _serverStatus.result = ERROR;
      _serverStatus.code = errorLocalFile;
      _serverStatus.desc = F("Local file error");
    }
    else
    {
      ftpState = cConnect;
      if (direction & 0x80)
      {
        while (ftpState <= cQuit)
        {
          handleFTP();
          delay(25);
        }
      }
    }
  }
  else
  {
    // return error code with status "in PROGRESS"
    _serverStatus.code = errorAlreadyInProgress;
  }
  return _serverStatus;
}

const FTPClient::Status &FTPClient::check()
{
  return _serverStatus;
}

void FTPClient::handleFTP()
{
  if (_server == nullptr)
  {
    _serverStatus.result = TransferResult::ERROR;
    _serverStatus.code = errorUninitialized;
    _serverStatus.desc = F("begin() not called");
  }
  else if (ftpState > cIdle)
  {
    _serverStatus.result = TransferResult::ERROR;
  }
  else if (cConnect == ftpState)
  {
    _serverStatus.code = errorConnectionFailed;
    _serverStatus.desc = F("No connection to FTP server");
    if (controlConnect())
    {
      FTP_DEBUG_MSG("Connection to %s:%u established", control.remoteIP().toString().c_str(), control.remotePort());
      _serverStatus.result = TransferResult::PROGRESS;
      ftpState = cGreet;
    }
    else
    {
      ftpState = cError;
    }
  }
  else if (cGreet == ftpState)
  {
    if (waitFor(220 /* 220 (vsFTPd version) */, F("No server greeting")))
    {
      FTP_DEBUG_MSG(">>> USER %s", _server->login.c_str());
      control.printf_P(PSTR("USER %s\n"), _server->login.c_str());
      ftpState = cUser;
    }
  }
  else if (cUser == ftpState)
  {
    if (waitFor(331 /* 331 Password */))
    {
      FTP_DEBUG_MSG(">>> PASS %s", _server->password.c_str());
      control.printf_P(PSTR("PASS %s\n"), _server->password.c_str());
      ftpState = cPassword;
    }
  }
  else if (cPassword == ftpState)
  {
    if (waitFor(230 /* 230 Login successful*/))
    {
      FTP_DEBUG_MSG(">>> PASV");
      control.printf_P(PSTR("PASV\n"));
      ftpState = cPassive;
    }
  }
  else if (cPassive == ftpState)
  {
    if (waitFor(227 /* 227 Entering Passive Mode (ip,ip,ip,ip,port,port) */))
    {
      bool parseOK = false;
      // find ()
      uint8_t bracketOpen = _serverStatus.desc.indexOf(F("("));
      uint8_t bracketClose = _serverStatus.desc.indexOf(F(")"));
      if (bracketOpen && (bracketClose > bracketOpen))
      {
        FTP_DEBUG_MSG("Parsing PASV response %s", _serverStatus.desc.c_str());
        _serverStatus.desc[bracketClose] = '\0';
        if (parseDataIpPort(_serverStatus.desc.c_str() + bracketOpen + 1))
        {
          // catch ip=0.0.0.0 and replace with the control.remoteIP()
          if (dataIP.toString() == F("0.0.0.0"))
          {
            dataIP = control.remoteIP();
          }
          parseOK = true;
          ftpState = cData;
        }
      }
      if (!parseOK)
      {
        _serverStatus.code = errorServerResponse;
        _serverStatus.desc = F("FTP server response not understood.");
      }
    }
  }
  else if (cData == ftpState)
  {
    // open data connection
    if (dataConnect() < 0)
    {
      _serverStatus.code = errorDataConnectionFailed;
      _serverStatus.desc = F("No data connection to FTP server");
      ftpState = cError;
    }
    else
    {
      FTP_DEBUG_MSG("Data connection to %s:%u established", data.remoteIP().toString().c_str(), data.remotePort());
      millisBeginTrans = millis();
      bytesTransfered = 0;
      ftpState = cTransfer;
      if (allocateBuffer() == 0)
      {
        _serverStatus.code = errorMemory;
        _serverStatus.desc = F("No memory for transfer buffer");
        ftpState = cError;
      }
      if (_direction & FTP_PUT_NONBLOCKING)
      {
        FTP_DEBUG_MSG(">>> STOR %s", _remoteFileName.c_str());
        control.printf_P(PSTR("STOR %s\n"), _remoteFileName.c_str());
      }
      else if (_direction & FTP_GET_NONBLOCKING)
      {
        FTP_DEBUG_MSG(">>> RETR %s", _remoteFileName.c_str());
        control.printf_P(PSTR("RETR %s\n"), _remoteFileName.c_str());
      }
    }
  }
  else if (cTransfer == ftpState)
  {
    bool res = true;
    if (_direction & FTP_PUT_NONBLOCKING)
    {
      res = doFiletoNetwork();
    }
    else
    {
      res = doNetworkToFile();
    }
    if (!res || !data.connected())
    {
      ftpState = cFinish;
    }
  }
  else if (cFinish == ftpState)
  {
    closeTransfer();
    ftpState = cQuit;
  }
  else if (cQuit == ftpState)
  {
    FTP_DEBUG_MSG(">>> QUIT");
    control.printf_P(PSTR("QUIT\n"));
    _serverStatus.result = OK;
    ftpState = cIdle;
  }
  else if (cIdle == ftpState)
  {
    stop();
  }
}

int8_t FTPClient::controlConnect()
{
  if (_server->validateCA)
  {
    FTP_DEBUG_MSG("Ignoring CA verification - FTP only");
  }
  control.connect(_server->servername.c_str(), _server->port);
  FTP_DEBUG_MSG("Connection to %s:%d ... %s", _server->servername.c_str(), _server->port, control.connected() ? PSTR("OK") : PSTR("failed"));
  if (control.connected())
    return 1;
  return -1;
}

bool FTPClient::waitFor(const int16_t respCode, const __FlashStringHelper *errorString, uint32_t timeOutMs)
{
  // initalize waiting
  if (!aTimeout.canExpire())
  {
    aTimeout.reset(timeOutMs);
    _serverStatus.desc.clear();
  }
  else
  {
    // timeout
    if (aTimeout.expired())
    {
      aTimeout.resetToNeverExpires();
      FTP_DEBUG_MSG("Waiting for code %u - timeout!", respCode);
      _serverStatus.code = errorTimeout;
      if (errorString)
      {
        _serverStatus.desc = errorString;
      }
      else
      {
        _serverStatus.desc = F("timeout");
      }
      ftpState = cTimeout;
      return false;
    }

    // check for bytes from the client
    while (control.available())
    {
      char c = control.read();
      //FTP_DEBUG_MSG("readChar() line='%s' <= %c", _serverStatus.desc.c_str(), c);
      if (c == '\n' || c == '\r')
      {
        // filter out empty lines
        _serverStatus.desc.trim();
        if (0 == _serverStatus.desc.length())
          continue;

        // line complete, evaluate code
        _serverStatus.code = atoi(_serverStatus.desc.c_str());
        if (respCode != _serverStatus.code)
        {
          ftpState = cError;
          FTP_DEBUG_MSG("Waiting for code %u but SMTP server replies: %s", respCode, _serverStatus.desc.c_str());
        }
        else
        {
          FTP_DEBUG_MSG("Waiting for code %u success, SMTP server replies: %s", respCode, _serverStatus.desc.c_str());
        }

        aTimeout.resetToNeverExpires();
        return (respCode == _serverStatus.code);
      }
      else
      {
        // just add the char
        _serverStatus.desc += c;
      }
    }
  }
  return false;
}
