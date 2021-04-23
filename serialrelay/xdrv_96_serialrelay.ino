/*
  xdrv_96_icse.ino - Driver for serial relay boards :
    * ICSE012A, ICSE013A, ICSE014A
    * LC Technology x1, x2 and x4
  
  Copyright (C) 2020  Nicolas Bernaerts

    20/11/2020 - v1.0 - Creation
    05/04/2021 - v2.0 - Rewrite to add LC Technology boards

  Settings are stored using weighting scale parameters :
    - Settings.weight_reference   = Relay board type
 
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

#ifdef USE_SERIALRELAY

/**************************************\
 *               Variables
\**************************************/

// declare teleinfo energy driver and sensor
#define XDRV_96      96

// maximum number of relays
#define SERIALRELAY_MAX_DEVICE     16
#define SERIALRELAY_INIT_TIMEOUT   4        // seconds before init

// web configuration page
#define D_SERIALRELAY_PAGE_CONFIG  "/sr"
#define D_CMND_SERIALRELAY         "board"
#define D_SERIALRELAY              "Relay Board"
#define D_SERIALRELAY_CONFIG       "Select Board Model"

// JSON message
#define SERIALRELAY_JSON         "SerialRelay"
#define SERIALRELAY_JSON_NAME    "Name"
#define SERIALRELAY_JSON_RATE    "Rate"
#define SERIALRELAY_JSON_RELAY   "Relay"
#define SERIALRELAY_JSON_DELAY   "Delay"

// strings
#define D_SERIALRELAY_BOARD      "Board"
#define D_SERIALRELAY_STATUS     "Status"

// form strings
const char SERIALRELAY_FORM_START[]  PROGMEM = "<form method='get' action='%s'>\n";
const char SERIALRELAY_FORM_STOP[]   PROGMEM = "</form>\n";
const char SERIALRELAY_FIELD_START[] PROGMEM = "<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>\n";
const char SERIALRELAY_FIELD_STOP[]  PROGMEM = "</fieldset></p><br>\n";
const char SERIALRELAY_INPUT_TEXT[]  PROGMEM = "<p><input type='radio' name='%s' value='%d' %s>%s</p>\n";

// relay commands
enum SerialRelayPendingSwitches { SERIALRELAY_SWITCH_NONE, SERIALRELAY_SWITCH_OFF, SERIALRELAY_SWITCH_ON };

// Serial board arrays
const char serialrelay_tmpl0[] PROGMEM = "{\"NAME\":\"ICSE-013A x2\",\"GPIO\":[21,148,0,149,22,0,0,0,0,0,0,0,0],\"FLAG\":0,\"BASE\":18}";
const char serialrelay_tmpl1[] PROGMEM = "{\"NAME\":\"ICSE-012A x4\",\"GPIO\":[21,148,0,149,22,23,0,0,24,0,0,0,0],\"FLAG\":0,\"BASE\":18}";
const char serialrelay_tmpl2[] PROGMEM = "{\"NAME\":\"ICSE-014A x8\",\"GPIO\":[21,148,0,149,22,23,0,0,24,25,26,27,28],\"FLAG\":0,\"BASE\":18}";
const char serialrelay_tmpl3[] PROGMEM = "{\"NAME\":\"LC Tech. x1 (old)\",\"GPIO\":[21,148,0,149,0,0,0,0,0,0,0,0,0],\"FLAG\":0,\"BASE\":18}";
const char serialrelay_tmpl4[] PROGMEM = "{\"NAME\":\"LC Tech. x1\",\"GPIO\":[21,148,0,149,0,0,0,0,0,0,0,0,0],\"FLAG\":0,\"BASE\":18}";
const char serialrelay_tmpl5[] PROGMEM = "{\"NAME\":\"LC Tech. x2 (old)\",\"GPIO\":[21,148,0,149,22,0,0,0,0,0,0,0,0],\"FLAG\":0,\"BASE\":18}";
const char serialrelay_tmpl6[] PROGMEM = "{\"NAME\":\"LC Tech. x2\",\"GPIO\":[21,148,0,149,22,0,0,0,0,0,0,0,0],\"FLAG\":0,\"BASE\":18}";
const char serialrelay_tmpl7[] PROGMEM = "{\"NAME\":\"LC Tech. x4 (old)\",\"GPIO\":[21,148,0,149,22,23,0,0,24,0,0,0,0],\"FLAG\":0,\"BASE\":18}";
const char serialrelay_tmpl8[] PROGMEM = "{\"NAME\":\"LC Tech. x4\",\"GPIO\":[21,148,0,149,22,23,0,0,24,0,0,0,0],\"FLAG\":0,\"BASE\":18}";
const char* const arr_serialrelay_tmpl[] PROGMEM = { serialrelay_tmpl0, serialrelay_tmpl1, serialrelay_tmpl2, serialrelay_tmpl3, serialrelay_tmpl4, serialrelay_tmpl5, serialrelay_tmpl6, serialrelay_tmpl7, serialrelay_tmpl8 };

enum SerialRelayBoards { SERIALRELAY_ICSE013A, SERIALRELAY_ICSE012A, SERIALRELAY_ICSE014A, SERIALRELAY_LC1_OLD, SERIALRELAY_LC1, SERIALRELAY_LC2_OLD, SERIALRELAY_LC2, SERIALRELAY_LC4_OLD, SERIALRELAY_LC4, SERIALRELAY_BOARD_MAX };
const char kSerialRelayBoard[] PROGMEM = "ICSE-013A 2 relays|ICSE-012A 4 relays (HW-034)|ICSE-014A 8 relays (HW-149)|LC Tech. Relay x1 (old)|LC Tech. Relay x1 v1.2+|LC Tech. Relay x2 (old)|LC Tech. Relay x2|LC Tech. Relay x4 (old)|LC Tech. Relay x4|Unknown board";
const int  arr_serialrelay_device[]    = { 2,    4,    8,    1,    1,      2,    2,      4,    4,      0};                      // number of relays
const int  arr_serialrelay_baud[]      = { 9600, 9600, 9600, 9600, 115200, 9600, 115200, 9600, 115200, 9600};                   // board serial speed
const int  arr_serialrelay_delay[]     = { 250,  250,  250,  400,  400,    400,  400,    400,  400,    500};                    // delay between commands (in ms)

// enum
enum SerialRelayStatus { SERIALRELAY_STATUS_SERIAL, SERIALRELAY_STATUS_INIT, SERIALRELAY_STATUS_HANDSHAKE, SERIALRELAY_STATUS_READY };
const char kSerialRelayStatus[] PROGMEM = "Configuration|Initialisation|Handshake|Ready";

// board status
struct {
  int      status    = SERIALRELAY_STATUS_SERIAL;
  int      type      = SERIALRELAY_BOARD_MAX;
  int      baud_rate = 0;
  int      nb_relay  = 0;
  int      delay_ms  = 0;
  uint32_t last_time = 0;
  uint8_t  last_state[SERIALRELAY_MAX_DEVICE];
  String   str_received;
} serialrelay_board;

/**************************************\
 *               Accessor
\**************************************/

// set relay board type
void SerialRelaySetBoardType (int new_type)
{
  // save board type
  if (new_type < 0) new_type = 0;
  if (new_type > SERIALRELAY_BOARD_MAX) new_type = SERIALRELAY_BOARD_MAX;
  Settings.weight_reference = (uint16_t)new_type;
}

// get relay board type
int SerialRelayGetBoardType ()
{
  int board_type;

  // get relay state
  board_type = (int)Settings.weight_reference;
  if (board_type > SERIALRELAY_BOARD_MAX) board_type = SERIALRELAY_BOARD_MAX;

  return board_type;
}

// -------------------------------------
//  Functions for ICSE01xA relay boards
// -------------------------------------

void SerialRelayInitialiseICSE01xA ()
{
  // reset received buffer
  serialrelay_board.str_received = "";

  // send initilisation caracter
  Serial.write (0x50); 

  // switch to handshake step
  serialrelay_board.status = SERIALRELAY_STATUS_HANDSHAKE;
  AddLog (LOG_LEVEL_INFO, PSTR ("SRB: Step %d"), serialrelay_board.status);
}

void SerialRelayHandshakeICSE01xA ()
{
  int  board_type = INT_MAX;
  char str_text[32];

  // check for board type caracter
  if (serialrelay_board.str_received.indexOf (0xAB) != -1) board_type = SERIALRELAY_ICSE012A;
  else if (serialrelay_board.str_received.indexOf (0xAC) != -1) board_type = SERIALRELAY_ICSE014A;
  else if (serialrelay_board.str_received.indexOf (0xAD) != -1) board_type = SERIALRELAY_ICSE013A;
  else if (serialrelay_board.str_received.length () > 0) board_type = SERIALRELAY_BOARD_MAX;

  // log
  GetTextIndexed (str_text, sizeof (str_text), board_type, kSerialRelayBoard);
  if (strlen (str_text) > 0) AddLog (LOG_LEVEL_INFO, PSTR("SRB: %s detected"), str_text);

  // activate board
  Serial.write (0x51);

  // reset buffer
  serialrelay_board.str_received = "";
  serialrelay_board.last_time = millis ();

  // force relays update by interting state of first relay
  serialrelay_board.last_state[0] = !bitRead (TasmotaGlobal.power, 0);
    
  // relay board is now ready
  serialrelay_board.status = SERIALRELAY_STATUS_READY;
  AddLog (LOG_LEVEL_INFO, PSTR ("SRB: Step %d"), serialrelay_board.status);
}

void SerialRelaySwitchICSEO1xA (int relay_index)
{
  int      index;
  uint8_t  relay_state;
  uint16_t global_state, relay_target;

  // loop to generate global relay state
  global_state = 0;
  for (index = 0; index < serialrelay_board.nb_relay; index ++)
  {
    // get current relay state
    relay_state = bitRead (TasmotaGlobal.power, index);

    // if current relay is ON, update global state
    if (relay_state != 0)
    {
      // add current relay to global state
      relay_target = 1 << index;
      global_state |= relay_target;
    }

    // if CURRENT relay state has changed
    if (serialrelay_board.last_state[index] != relay_state)
    {
      // log state change
      if (relay_state == 0) AddLog (LOG_LEVEL_INFO, PSTR ("SRB: Relay %d switched OFF"), index + 1);
      else AddLog (LOG_LEVEL_INFO, PSTR ("SRB: Relay %d switched ON"), index + 1);
    }

    // update last state
    serialrelay_board.last_state[index] = relay_state;
  }

  // send current ---inverted--- global state to board
  Serial.write ((uint8_t)~global_state);
}

// ------------------------------------------
//  Functions for LC Technology relay boards
// ------------------------------------------

void SerialRelayInitialiseLCTech ()
{
  // switch to handshake step
  serialrelay_board.status = SERIALRELAY_STATUS_HANDSHAKE;
  AddLog (LOG_LEVEL_INFO, PSTR ("SRB: Step %d"), serialrelay_board.status);
}

void SerialRelayHandshakeLCTech ()
{
  // check for AT+CWMODE=1
  if (serialrelay_board.str_received.indexOf ("AT+CWMODE=1") != -1)
  {
    // send string to swith to mode 2
    Serial.write ("WIFI CONNECTED");
    Serial.write (0x0A);
    Serial.write ("WIFI GOT IP");
    Serial.write (0x0A);
    Serial.write ("AT+CIPMUX=1");
    Serial.write (0x0A);
    Serial.write ("AT+CIPSERVER=1,8080");
    Serial.write (0x0A);
    Serial.write ("AT+CIPSTO=360");

    // init last command timestamp
    serialrelay_board.last_time = millis ();

    // reset buffer
    serialrelay_board.str_received = "WAIT";
  }

  // else, check for AT+CIPSTO=360 or for empty buffer (case when ESP8266 reboots)
  else if ((serialrelay_board.str_received.indexOf ("AT+CIPSTO=360") != -1) || (serialrelay_board.str_received.length () == 0))
  {
    // init last command timestamp
    serialrelay_board.last_time = millis ();

    // reset buffer
    serialrelay_board.str_received = "";

    // relay board is now ready
    serialrelay_board.status = SERIALRELAY_STATUS_READY;
    AddLog (LOG_LEVEL_INFO, PSTR ("SRB: Step %d"), serialrelay_board.status);
  }
}

void SerialRelaySwitchLCTech (int index)
{
  uint8_t  relay_id, relay_state, relay_checksum;

  // if temporisation is not over
  if (millis () - serialrelay_board.last_time > serialrelay_board.delay_ms)
  {
    // get current relay state
    relay_state = bitRead (TasmotaGlobal.power, index);

    // calculate data for serial command
    relay_id = (uint8_t)index + 1;
    relay_checksum = 0xA0 + relay_id + relay_state;

    // switch command
    Serial.write (0xA0);
    Serial.write (relay_id);
    Serial.write (relay_state);
    Serial.write (relay_checksum);

    // log state change
    if (relay_state == 0) AddLog (LOG_LEVEL_INFO, PSTR ("SRB: Relay %d switched OFF"), relay_id);
    else AddLog (LOG_LEVEL_INFO, PSTR ("SRB: Relay %d switched ON"), relay_id);

    // update last state and last command timestamp
    serialrelay_board.last_state[index] = relay_state;
    serialrelay_board.last_time         = millis ();
  }
}

/***************************************\
 *               Functions
\***************************************/

void SerialRelayInit ()
{
  int  index;
  char str_text[32];

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("SRB: Step %d"), serialrelay_board.status);

  // initialise pending commands
  for (index = 0; index < SERIALRELAY_MAX_DEVICE; index ++) serialrelay_board.last_state[index] = 0;

  // get board type
  serialrelay_board.type      = SerialRelayGetBoardType ();
  serialrelay_board.baud_rate = arr_serialrelay_baud[serialrelay_board.type];
  serialrelay_board.nb_relay  = arr_serialrelay_device[serialrelay_board.type];
  serialrelay_board.delay_ms  = arr_serialrelay_delay[serialrelay_board.type];

  // check if serial ports are selected 
  if (PinUsed (GPIO_TXD) && PinUsed (GPIO_RXD)) 
  {
    // start ESP8266 serial port
    Serial.flush ();
    Serial.begin (serialrelay_board.baud_rate, SERIAL_8N1);
    ClaimSerial ();

    // force GPIO2 as output and set it LOW to enable Tx
    pinMode (2, OUTPUT);
    digitalWrite (2, LOW);

    // next step is initilisation
    serialrelay_board.status = SERIALRELAY_STATUS_INIT;
    AddLog (LOG_LEVEL_INFO, PSTR ("SRB: Step %d"), serialrelay_board.status);
  }
}

void SerialRelayBeforeRestart ()
{
  // set back GPIO2 to input PIN before reboot (to avoid Tx junk)
  digitalWrite (2, HIGH);
  pinMode (2, INPUT);
}

void SerialRelayBoardInitialise ()
{
  // handle action according to relay board type
  switch (serialrelay_board.type) 
  {
    case SERIALRELAY_ICSE013A:
    case SERIALRELAY_ICSE012A:
    case SERIALRELAY_ICSE014A:
      SerialRelayInitialiseICSE01xA ();
      break;

    case SERIALRELAY_LC1:
    case SERIALRELAY_LC2:
    case SERIALRELAY_LC4:
      SerialRelayInitialiseLCTech ();
      break;
  }
}

void SerialRelayBoardHandshake ()
{
  // handle action according to relay board type
  switch (serialrelay_board.type) 
  {
    case SERIALRELAY_ICSE013A:
    case SERIALRELAY_ICSE012A:
    case SERIALRELAY_ICSE014A:
      SerialRelayHandshakeICSE01xA ();
      break;

    case SERIALRELAY_LC1:
    case SERIALRELAY_LC2:
    case SERIALRELAY_LC4:
      SerialRelayHandshakeLCTech ();
      break;
  }
}

void SerialRelayBoardSynchronise ()
{
  int index;

  // loop thru all relays
  for (index = 0; index < serialrelay_board.nb_relay; index ++)
  {
    // if current relay state is different from last state
    if (serialrelay_board.last_state[index] != bitRead (TasmotaGlobal.power, index))
    {
      // handle switch command according to relay board type
      switch (serialrelay_board.type) 
      {
        case SERIALRELAY_ICSE013A:
        case SERIALRELAY_ICSE012A:
        case SERIALRELAY_ICSE014A:
          SerialRelaySwitchICSEO1xA (index);
          break;

        case SERIALRELAY_LC1:
        case SERIALRELAY_LC2:
        case SERIALRELAY_LC4:
          SerialRelaySwitchLCTech (index);
          break;
      }
    }
  }

  // empty reception buffer
  serialrelay_board.str_received = "";
}

void SerialRelayBoardUpdate ()
{
  // receive serial buffer
  if (serialrelay_board.status != SERIALRELAY_STATUS_SERIAL) 
    while (Serial.available() > 0) serialrelay_board.str_received += (char)Serial.read();

  switch (serialrelay_board.status) 
  {
    case SERIALRELAY_STATUS_INIT:
      SerialRelayBoardInitialise ();
      break;

    case SERIALRELAY_STATUS_HANDSHAKE:
      SerialRelayBoardHandshake ();
      break;

    case SERIALRELAY_STATUS_READY:
      SerialRelayBoardSynchronise ();
      break;
  }
}

// Append board JSON status
void SerialRelayAppendJSON ()
{
  char   str_text[32];
  String str_json;

  // get board name
  GetTextIndexed (str_text, sizeof (str_text), serialrelay_board.type, kSerialRelayBoard);

  // generate JSON string
  str_json  = "\"";
  str_json += SERIALRELAY_JSON;
  str_json += "\":{\"";
  str_json += SERIALRELAY_JSON_NAME;
  str_json += "\":\"";
  str_json += str_text;
  str_json += "\",\"";
  str_json += SERIALRELAY_JSON_RATE;
  str_json += "\":";
  str_json += serialrelay_board.baud_rate;
  str_json += ",\"";
  str_json += "\",\"";
  str_json += SERIALRELAY_JSON_RELAY;
  str_json += "\":";
  str_json += serialrelay_board.nb_relay;
  str_json += ",\"";
  str_json += SERIALRELAY_JSON_DELAY;
  str_json += "\":";
  str_json += serialrelay_board.delay_ms;
  str_json += "}";

  // append string to MQTT message
  ResponseAppend_P (PSTR(",%s"), str_json.c_str ());
}

/**************************************\
 *                   Web
\**************************************/

#ifdef USE_WEBSERVER

// append relay status to main page
void SerialRelayWebSensor ()
{
  uint32_t duration;
  char     str_text[32];
  String   str_status;

  // get board status
  GetTextIndexed (str_text, sizeof (str_text), serialrelay_board.status, kSerialRelayStatus);
  str_status = str_text;

  // if hanshake, calculate delay
  if (serialrelay_board.status == SERIALRELAY_STATUS_HANDSHAKE)
  {
    duration = (millis () - serialrelay_board.last_time) / 1000;
    str_status += " (";
    str_status += duration
    ;
    str_status += ")";
  }

  // display status
  WSContentSend_PD (PSTR ("{s}%s %s{m}%s{e}"), D_SERIALRELAY_BOARD, D_SERIALRELAY_STATUS, str_status.c_str ());
}

// append Serial Relay configuration button
void SerialRelayWebConfigButton ()
{
  // heater and meter configuration button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>"), D_SERIALRELAY_PAGE_CONFIG, D_SERIALRELAY_CONFIG);
}

// board selection page
void SerialRelayWebPageConfig ()
{
  int    index, board_type;
  char   str_text[MAX_LOGSZ];
  String str_board, str_active;

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // get teleinfo mode according to ETH parameter
  if (Webserver->hasArg(D_CMND_SERIALRELAY))
  {
    WebGetArg (D_CMND_SERIALRELAY, str_text, sizeof(str_text));
    index = atoi (str_text);

    // if board is referenced
    if (index < SERIALRELAY_BOARD_MAX)
    {
      // save board selection
      SerialRelaySetBoardType (index);

      // apply template as default one
      snprintf_P (str_text, sizeof(str_text), PSTR(D_CMND_BACKLOG " " D_CMND_TEMPLATE " %s; %s 0"), arr_serialrelay_tmpl[index], D_CMND_MODULE);
      ExecuteWebCommand (str_text, SRC_WEBGUI);

      // if some data has been updated, ask for reboot
      WebRestart(1);
    }
  }

  // get current board type
  board_type = SerialRelayGetBoardType ();

  // beginning of form
  WSContentStart_P (D_SERIALRELAY_CONFIG);
  WSContentSendStyle ();
  WSContentSend_P (SERIALRELAY_FORM_START, D_SERIALRELAY_PAGE_CONFIG);

  // board selection
  WSContentSend_P (SERIALRELAY_FIELD_START, D_SERIALRELAY);
  for (index = 0; index < SERIALRELAY_BOARD_MAX; index ++) 
  {
    GetTextIndexed (str_text, sizeof(str_text), index, kSerialRelayBoard);
    if (index == board_type) str_active = "checked"; else str_active = "";
    WSContentSend_P (SERIALRELAY_INPUT_TEXT, D_CMND_SERIALRELAY, index, str_active.c_str (), str_text);
  }
  WSContentSend_P (SERIALRELAY_FIELD_STOP);
   
  // save button
  WSContentSend_P (PSTR ("<button name='save' type='submit' class='button bgrn'>%s</button>"), D_SAVE);
  WSContentSend_P (SERIALRELAY_FORM_STOP);

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

#endif  // USE_WEBSERVER

/************************************\
 *              Interface
\************************************/

// Icse relay board driver
bool Xdrv96 (uint8_t function)
{
  // swtich according to context
  switch (function) 
  {
    case FUNC_PRE_INIT:
      SerialRelayInit ();
      break;
    case FUNC_EVERY_100_MSECOND:
      if (TasmotaGlobal.uptime > SERIALRELAY_INIT_TIMEOUT) SerialRelayBoardUpdate ();
      break;
    case FUNC_JSON_APPEND:
      SerialRelayAppendJSON ();
      break;
    case FUNC_SAVE_BEFORE_RESTART:
      SerialRelayBeforeRestart ();
      break;
#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      SerialRelayWebSensor ();
      break;
    case FUNC_WEB_ADD_HANDLER:
      // pages
      Webserver->on (D_SERIALRELAY_PAGE_CONFIG, SerialRelayWebPageConfig);
      break;
    case FUNC_WEB_ADD_BUTTON:
    case FUNC_WEB_ADD_MAIN_BUTTON:
      SerialRelayWebConfigButton ();
      break;
#endif  // USE_WEBSERVER
  }
  return false;
}

#endif   // USE_SERIALRELAY

