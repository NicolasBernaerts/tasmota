/*
  xdrv_98_teleinfo_01_tcp.ino - TCP server for Teleinfo data

  Copyright (C) 2021-2025  Nicolas Bernaerts

  This TCP server sends raw data on a TCP port
  It allows to publish any data stream on you LAN
  You can start and stop the server from the console : 
    - start TCP server on port 8888 : tcp_start 8888
    - stop TCP server               : tcp_stop
    - check TCP server status       : tcp_status

  From any linux or raspberry, you can retrieve the teleinfo stream with
    # nc 192.168.x.x 8888

  Version history :
    04/08/2021 v1.0 - Creation
    15/09/2022 v1.1 - Limit connexion to 1 client (latest kills previous one)
    03/01/2024 v2.0 - tcp_status bug correction
                      Dynamic loading thru tcp_start
    10/07/2025 v3.0 - Refactoring based on Tasmota 15

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

#ifdef USE_TELEINFO

#ifdef ESP32
  #define TCP_DATA_MAX                256
#else
  #define TCP_DATA_MAX                64
#endif    // ESP32

/****************************************\
 *           Class TCPServer
\****************************************/

class TCPServer
{
public:
  TCPServer ();
  bool start (const int port);
  bool stop ();
  void send (const char to_send);
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
  server       = nullptr;
  server_port  = 0;
  client_next  = 0;
  buffer_index = 0;
}

bool TCPServer::stop ()
{
  // if TCP server is inactive, cancel 
  if (server == nullptr) return false;

  // stop client
  client.stop ();

  // kill server
  server->stop ();
  delete server;
  server = nullptr;
  server_port = 0;

  return true;
}

bool TCPServer::start (const int port)
{
  // if port undefined, cancel start
  if (port == 0) return false;

  // if server already started, stop it
  if (server != nullptr) stop ();

  // create and start server
  server = new WiFiServer ((uint16_t)port);

  // if TCP server not created, cancel 
  if (server == nullptr) return false;

  // start server
  server_port = port;
  server->begin ();

  // reset buffer index
  buffer_index = 0;

  return true;
}

// TCP server data send
void TCPServer::send (const char to_send)
{
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
const char kTCPServerCommands[]         PROGMEM = "tcp" "|"            "|"       "_start"     "|"      "_stop"      "|"     "_status";
void (* const TCPServerCommand[])(void) PROGMEM = { &CmndTeleinfoTCPHelp, &CmndTeleinfoTCPStart, &CmndTeleinfoTCPStop, &CmndTeleinfoTCPStatus };

// TCP server instance
TCPServer *ptcp_server = nullptr;

/***********************************************************\
 *                      Commands
\***********************************************************/

// TCP stream help
void CmndTeleinfoTCPHelp ()
{
  int tcp_port = 0;

  // if server active, get listening port
  if (ptcp_server != nullptr) tcp_port = ptcp_server->get_port ();

  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: TCP Server commands :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - tcp_status       = server listening port, 0 if stopped (%d)"), tcp_port);
  AddLog (LOG_LEVEL_INFO, PSTR (" - tcp_start <port> = start server on specified port"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - tcp_stop         = stop stream"));
  AddLog (LOG_LEVEL_INFO, PSTR ("   Server allows only 1 concurrent connexion"));
  AddLog (LOG_LEVEL_INFO, PSTR ("   Any new client will kill previous one"));
  ResponseCmndDone();
}

// Start TCP server
void CmndTeleinfoTCPStart (void)
{
  bool done = false;
  int  tcp_port = 0;

  // if port is provided
  if (XdrvMailbox.data_len > 0) tcp_port = XdrvMailbox.payload;
  if (tcp_port > 0)
  {
    // if TCP server not created, create it
    if (ptcp_server == nullptr)
    {
      // create server
      ptcp_server = new TCPServer ();

      // if server created
      if (ptcp_server != nullptr) 
      {
        // start it on specified port
        ptcp_server->start (tcp_port);

        // log
        AddLog (LOG_LEVEL_INFO, PSTR ("TCP: Server started on port %d"), tcp_port);
        done = true;
      }
    }
  }

  // answer
  if (done) ResponseCmndDone ();
    else ResponseCmndFailed ();
}

// Stop TCP server
void CmndTeleinfoTCPStop (void)
{
  bool done = false;

  // if server exists
  if (ptcp_server != nullptr) 
  {
    // stop it
    ptcp_server->stop ();
    delete (ptcp_server);
    ptcp_server = nullptr;

    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("TCP: Server stopped"));
    done = true;
  }

  // answer
  if (done) ResponseCmndDone (); 
    else ResponseCmndFailed ();
}

// Get TCP server status
void CmndTeleinfoTCPStatus (void)
{
  int tcp_port = 0;

  // if server active, get listening port
  if (ptcp_server != nullptr) tcp_port = ptcp_server->get_port ();
  
  ResponseCmndNumber (tcp_port);
}

/***********************************************************\
 *                      Functions
\***********************************************************/

// init main status
void TeleinfoTCPInit ()
{
  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Run tcp to get help on TCP Server commands"));
}

// check for client connexion
void TeleinfoTCPEvery100ms ()
{
  // if server is active, check client connexion
  if (ptcp_server != nullptr) ptcp_server->check_for_client ();
}

// send caracter
void TeleinfoTCPSend (const char to_send)
{
  // if server is active, send caracter
  if (ptcp_server != nullptr) ptcp_server->send (to_send);
}

#endif  // USE_TELEINFO
