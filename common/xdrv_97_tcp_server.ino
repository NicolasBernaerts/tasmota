/*
  xdrv_97_tcp_server.ino - TCP server

  Copyright (C) 2021  Nicolas Bernaerts

  This TCP server sends raw data on a TCP port
  It allows to publish any data stream on you LAN
  You can start and stop the server from the console : 
    - start TCP server on port 8888 : tcp_start 8888
    - stop TCP server               : tcp_stop
    - check TCP server status       : tcp_status

  From any linux or raspberry, you can retrieve the teleinfo stream with
    # nc 192.168.x.x 8888

  Version history :
    04/08/2021 - v1.0 - Creation
    15/09/2022 - v1.1 - Limit connexion to 1 client (latest kills previous one)

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

#ifdef USE_TCPSERVER

#define XDRV_97                   97

#define TCP_DATA_MAX              64             // buffer size

/****************************************\
 *           Class TCPServer
\****************************************/

class TCPServer
{
public:
  TCPServer ();
  bool start (int port);
  bool stop ();
  void send (char to_send);
  void check_for_client ();
  int  get_port ();

private:
  WiFiServer *server;                       // TCP server pointer
  WiFiClient client;                        // TCP client
  int        server_port;                   // TCP server port number
  uint8_t    client_next;                   // next client slot
  uint8_t    buffer_index;                  // buffer current index
  char       buffer[TCP_DATA_MAX];          // data transfer buffer
};

TCPServer::TCPServer ()
{
  server = nullptr;
  server_port  = 0;
  client_next  = 0;
  buffer_index = 0;
}

bool TCPServer::stop ()
{
  int index; 

  // if TCP server is inactive, cancel 
  if (server == nullptr) return false;

  // stop client
  client.stop ();

  // kill server
  server->stop ();
  delete server;
  server = nullptr;
  server_port = 0;

  // validate and log
  AddLog (LOG_LEVEL_INFO, PSTR ("TCP: Stopping TCP server"));

  return true;
}

bool TCPServer::start (int port)
{
  // if port undefined, cancel start
  if (port <= 0) return false;

  // if server already started, stop it
  if (server != nullptr) stop ();

  // create and start server
  server = new WiFiServer (port);

  // if TCP server not created, cancel 
  if (server == nullptr) return false;

  // start server
  server_port = port;
  server->begin ();
  server->setNoDelay (true);

  // reset buffer index
  buffer_index = 0;

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("TCP: Starting TCP server on port %d"), port);

  return true;
}

// TCP server data send
void TCPServer::send (char to_send)
{
  int index;

  // if TCP server is inactive, cancel 
  if (server == nullptr) return;

  // append caracter to buffer
  if (buffer_index < sizeof (buffer)) buffer[buffer_index] = to_send;
  buffer_index++;

  // if end of line or buffer size reached
  if ((to_send == 13) || (buffer_index == sizeof (buffer)))
  {
    // send data to connected clients
    if (client.connected ()) client.write (buffer, buffer_index);

    // reset buffer index
    buffer_index = 0;
  }
}

// TCP server client connexion management
void TCPServer::check_for_client ()
{
  int index;

  // if TCP server is inactive, cancel 
  if (server == nullptr) return;

  // if a new client connection is waitong
  if (server->hasClient ())
  {
    // if needed, disconnect previous client
    if (client.connected ()) client.stop ();

    // connect new client
    client = server->available ();

    // reset buffer
    buffer_index = 0;
  }
}

// TCP server running port number
int TCPServer::get_port ()
{
  return server_port;
}

/***********************************************************\
 *                      Variables
\***********************************************************/

// TCP - MQTT commands
const char kTCPServerCommands[] PROGMEM = "tcp_" "|" "help" "|" "start" "|" "stop" "|" "status";
void (* const TCPServerCommand[])(void) PROGMEM = { &CmndTCPHelp, &CmndTCPStart, &CmndTCPStatus };

// TCP server instance
TCPServer tcp_server;

/***********************************************************\
 *                      Commands
\***********************************************************/

// TCP stream help
void CmndTCPHelp ()
{
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: TCP Server commands :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - tcp_status     = status (running port or 0 if not running)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - tcp_start xxxx = start stream on port xxxx"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - tcp_stop       = stop stream"));
  AddLog (LOG_LEVEL_INFO, PSTR ("   Server allows only 1 concurrent connexion"));
  AddLog (LOG_LEVEL_INFO, PSTR ("   Any new client will kill previous one"));
  ResponseCmndDone();
}

// Start TCP server
void CmndTCPStart (void)
{
  bool result = false;

  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload > 0)) result = tcp_server.start (XdrvMailbox.payload);
 
  if (result) ResponseCmndDone ();
    else ResponseCmndFailed ();
}

// Stop TCP server
void CmndTCPStop (void)
{
  bool result = false;

  result = tcp_server.stop ();

  if (result) ResponseCmndDone (); 
    else ResponseCmndFailed ();
}

// Get TCP server status
void CmndTCPStatus (void)
{
  int port = tcp_server.get_port ();

  ResponseCmndNumber (port); 
}

/***********************************************************\
 *                      Functions
\***********************************************************/

// init main status
void TCPInit ()
{
  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: tcp_help to get help on TCP Server commands"));
}

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xdrv97 (uint32_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
    case FUNC_INIT:
      TCPInit ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kTCPServerCommands, TCPServerCommand);
      break;
    case FUNC_EVERY_100_MSECOND:
      tcp_server.check_for_client ();
      break;
  }
  
  return result;
}

#endif    // USE_TCPSERVER
