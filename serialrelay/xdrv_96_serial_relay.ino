/*
  xdrv_96_serial_relay.ino - Driver for serial relay boards :
    * ICSE012A, ICSE013A, ICSE014A
    * LC Technology x1, x2 and x4
  
  Copyright (C) 2020  Nicolas Bernaerts

    20/11/2020 - v1.0 - Creation
    05/04/2021 - v2.0 - Rewrite to add LC Technology boards
    01/05/2021 - v2.1 - Remove use of String to avoid heap fragmentation
    18/07/2021 - v2.2 - Tasmota 9.5 compatibility
    03/02/2023 - v2.3 - Tasmota 12.3 compatibility

  Settings are stored using weighting scale parameters :
    - Settings.weight_reference = Relay board type
 
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

#ifdef USE_RELAY_SERIAL

/**************************************\
 *               Variables
\**************************************/

// declare serial relay driver
#define XDRV_96      96

// maximum number of relays
#define RELAYSERIAL_MAX_DEVICE     8
#define RELAYSERIAL_INIT_TIMEOUT   4        // seconds before init

// web configuration page
#define D_RELAYSERIAL_PAGE_CONFIG  "/srelay"
#define D_RELAYSERIAL_CMND         "serialrelay"
#define D_SERIALRELAY              "Relay Board"
#define D_RELAYSERIAL_CONFIG       "Select Board Model"

// JSON message
#define RELAYSERIAL_JSON         "RelaySerial"
#define RELAYSERIAL_JSON_NAME    "Name"
#define RELAYSERIAL_JSON_RATE    "Rate"
#define RELAYSERIAL_JSON_RELAY   "Relay"
#define RELAYSERIAL_JSON_DELAY   "Delay"

// strings
#define D_RELAYSERIAL_BOARD      "Board"
#define D_RELAYSERIAL_STATUS     "Status"

// form strings
const char RELAYSERIAL_FORM_START[]  PROGMEM = "<form method='get' action='%s'>\n";
const char RELAYSERIAL_FORM_STOP[]   PROGMEM = "</form>\n";
const char RELAYSERIAL_FIELD_START[] PROGMEM = "<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>\n";
const char RELAYSERIAL_FIELD_STOP[]  PROGMEM = "</fieldset></p><br>\n";
const char RELAYSERIAL_INPUT_TEXT[]  PROGMEM = "<p><input type='radio' name='%s' value='%d' %s>%s</p>\n";

// relay commands
enum RelaySerialPendingSwitches { RELAYSERIAL_SWITCH_NONE, RELAYSERIAL_SWITCH_OFF, RELAYSERIAL_SWITCH_ON };

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

enum RelaySerialBoards { RELAYSERIAL_ICSE013A, RELAYSERIAL_ICSE012A, RELAYSERIAL_ICSE014A, RELAYSERIAL_LC1_OLD, RELAYSERIAL_LC1, RELAYSERIAL_LC2_OLD, RELAYSERIAL_LC2, RELAYSERIAL_LC4_OLD, RELAYSERIAL_LC4, RELAYSERIAL_BOARD_MAX };
const char kRelaySerialBoard[] PROGMEM = "ICSE-013A 2 relays|ICSE-012A 4 relays (HW-034)|ICSE-014A 8 relays (HW-149)|LC Tech. Relay x1 (old)|LC Tech. Relay x1 v1.2+|LC Tech. Relay x2 (old)|LC Tech. Relay x2|LC Tech. Relay x4 (old)|LC Tech. Relay x4|Unknown board";
const int  arr_serialrelay_device[]    = {2,                 4,                          8,                          1,                      1,                      2,                      2,                4,                      4,                0};                      // number of relays
const int  arr_serialrelay_baud[]      = {9600,              9600,                       9600,                       9600,                   115200,                 9600,                   115200,           9600,                   115200,           9600};                   // board serial speed
const int  arr_serialrelay_delay[]     = {250,               250,                        250,                        400,                    400,                    400,                    400,              400,                    400,              500};                    // delay between commands (in ms)

// enum
enum RelaySerialStatus { RELAYSERIAL_STATUS_SERIAL, RELAYSERIAL_STATUS_INIT, RELAYSERIAL_STATUS_HANDSHAKE, RELAYSERIAL_STATUS_READY };
const char kRelaySerialStatus[] PROGMEM = "Configuration|Initialisation|Handshake|Ready";

// board status
struct {
  int      status    = RELAYSERIAL_STATUS_SERIAL;
  int      type      = RELAYSERIAL_BOARD_MAX;
  int      baud_rate = 0;
  int      nb_relay  = 0;
  int      delay_ms  = 0;
  uint32_t last_time = 0;
  uint8_t  last_state[RELAYSERIAL_MAX_DEVICE];
  char     str_received[64];
} serialrelay_board;

/**************************************\
 *               Accessor
\**************************************/

// set relay board type
void RelaySerialSetBoardType (int new_type)
{
  // save board type
  if (new_type < 0) new_type = 0;
  if (new_type > RELAYSERIAL_BOARD_MAX) new_type = RELAYSERIAL_BOARD_MAX;
  Settings->weight_reference = (uint16_t)new_type;
}

// get relay board type
int RelaySerialGetBoardType ()
{
  int board_type;

  // get relay state
  board_type = (int)Settings->weight_reference;
  if (board_type > RELAYSERIAL_BOARD_MAX) board_type = RELAYSERIAL_BOARD_MAX;

  return board_type;
}

// -------------------------------------
//  Functions for ICSE01xA relay boards
// -------------------------------------

void RelaySerialInitialiseICSE01xA ()
{
  // reset received buffer
  strcpy (serialrelay_board.str_received, "");

  // send initilisation caracter
  Serial.write (0x50); 

  // switch to handshake step
  serialrelay_board.status = RELAYSERIAL_STATUS_HANDSHAKE;
  AddLog (LOG_LEVEL_INFO, PSTR ("SRB: Step %d"), serialrelay_board.status);
}

void RelaySerialHandshakeICSE01xA ()
{
  int  board_type = INT_MAX;
  char str_text[32];

  // check for board type caracter
  if (strchr (serialrelay_board.str_received, 0xAB) != nullptr) board_type = RELAYSERIAL_ICSE012A;
  else if (strchr (serialrelay_board.str_received, 0xAC) != nullptr) board_type = RELAYSERIAL_ICSE014A;
  else if (strchr (serialrelay_board.str_received, 0xAD) != nullptr) board_type = RELAYSERIAL_ICSE013A;
  else if (strlen (serialrelay_board.str_received) > 0) board_type = RELAYSERIAL_BOARD_MAX;

  // log
  GetTextIndexed (str_text, sizeof (str_text), board_type, kRelaySerialBoard);
  if (strlen (str_text) > 0) AddLog (LOG_LEVEL_INFO, PSTR("SRB: %s detected"), str_text);

  // activate board
  Serial.write (0x51);

  // reset buffer
  strcpy (serialrelay_board.str_received, "");
  serialrelay_board.last_time = millis ();

  // force relays update by interting state of first relay
  serialrelay_board.last_state[0] = !bitRead (TasmotaGlobal.power, 0);
    
  // relay board is now ready
  serialrelay_board.status = RELAYSERIAL_STATUS_READY;
  AddLog (LOG_LEVEL_INFO, PSTR ("SRB: Step %d"), serialrelay_board.status);
}

void RelaySerialSwitchICSEO1xA (int relay_index)
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

void RelaySerialInitialiseLCTech ()
{
  // switch to handshake step
  serialrelay_board.status = RELAYSERIAL_STATUS_HANDSHAKE;
  AddLog (LOG_LEVEL_INFO, PSTR ("SRB: Step %d"), serialrelay_board.status);
}

void RelaySerialHandshakeLCTech ()
{
  // check for AT+CWMODE=1
  if (strstr (serialrelay_board.str_received, "AT+CWMODE=1") != nullptr)
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
    strcpy (serialrelay_board.str_received, "WAIT");
  }

  // else, check for AT+CIPSTO=360 or for empty buffer (case when ESP8266 reboots)
  else if ((strstr (serialrelay_board.str_received, "AT+CIPSTO=360") != nullptr) || (strlen (serialrelay_board.str_received) == 0))
  {
    // init last command timestamp
    serialrelay_board.last_time = millis ();

    // reset buffer
    strcpy (serialrelay_board.str_received, "");

    // relay board is now ready
    serialrelay_board.status = RELAYSERIAL_STATUS_READY;
    AddLog (LOG_LEVEL_INFO, PSTR ("SRB: Step %d"), serialrelay_board.status);
  }
}

void RelaySerialSwitchLCTech (int index)
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

void RelaySerialInit ()
{
  int  index;
  char str_text[32];

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("SRB: Step %d"), serialrelay_board.status);

  // disable fast cycle power recovery (SetOption65)
  Settings->flag3.fast_power_cycle_disable = true;

  // initialise pending commands
  for (index = 0; index < RELAYSERIAL_MAX_DEVICE; index ++) serialrelay_board.last_state[index] = 0;

  // get board type
  serialrelay_board.type      = RelaySerialGetBoardType ();
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
    serialrelay_board.status = RELAYSERIAL_STATUS_INIT;
    AddLog (LOG_LEVEL_INFO, PSTR ("SRB: Step %d"), serialrelay_board.status);
  }
}

void RelaySerialBeforeRestart ()
{
  // set back GPIO2 to input PIN before reboot (to avoid Tx junk)
  digitalWrite (2, HIGH);
  pinMode (2, INPUT);
}

// send serial board init sequence
void RelaySerialBoardInitialise ()
{
  // handle action according to relay board type
  switch (serialrelay_board.type) 
  {
    case RELAYSERIAL_ICSE013A:
    case RELAYSERIAL_ICSE012A:
    case RELAYSERIAL_ICSE014A:
      RelaySerialInitialiseICSE01xA ();
      break;

    case RELAYSERIAL_LC1:
    case RELAYSERIAL_LC2:
    case RELAYSERIAL_LC4:
      RelaySerialInitialiseLCTech ();
      break;
  }
}

// set the handshake to finalise serial board init
void RelaySerialBoardHandshake ()
{
  // handle action according to relay board type
  switch (serialrelay_board.type) 
  {
    case RELAYSERIAL_ICSE013A:
    case RELAYSERIAL_ICSE012A:
    case RELAYSERIAL_ICSE014A:
      RelaySerialHandshakeICSE01xA ();
      break;

    case RELAYSERIAL_LC1:
    case RELAYSERIAL_LC2:
    case RELAYSERIAL_LC4:
      RelaySerialHandshakeLCTech ();
      break;
  }
}

// synchronise current serial relay state with tasmota relay state
void RelaySerialBoardSynchronise ()
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
        case RELAYSERIAL_ICSE013A:
        case RELAYSERIAL_ICSE012A:
        case RELAYSERIAL_ICSE014A:
          RelaySerialSwitchICSEO1xA (index);
          break;

        case RELAYSERIAL_LC1:
        case RELAYSERIAL_LC2:
        case RELAYSERIAL_LC4:
          RelaySerialSwitchLCTech (index);
          break;
      }
    }
  }

  // empty reception buffer
  strcpy (serialrelay_board.str_received, "");
}

// function called 10 times per second
void RelaySerialEvery100ms ()
{
  char str_received[2] = {0, 0};

  // reception of serial buffer till empty
  if (serialrelay_board.status != RELAYSERIAL_STATUS_SERIAL) 
    while (Serial.available() > 0)
    {
      str_received[0] = (char)Serial.read();
      strlcat (serialrelay_board.str_received, str_received, sizeof (serialrelay_board.str_received)); 
    }

  // action according to serial relay init stage
  switch (serialrelay_board.status) 
  {
    // send serial board init sequence
    case RELAYSERIAL_STATUS_INIT:
      RelaySerialBoardInitialise ();
      break;

    // set the handshake to finalise serial board init
    case RELAYSERIAL_STATUS_HANDSHAKE:
      RelaySerialBoardHandshake ();
      break;

    // synchronise current serial relay state with tasmota relay state
    case RELAYSERIAL_STATUS_READY:
      RelaySerialBoardSynchronise ();
      break;
  }
}

// Append board JSON status
void RelaySerialAppendJSON ()
{
  char str_text[32];

  // append RelaySerial section
  ResponseAppend_P (PSTR (",\"%s\":{"), RELAYSERIAL_JSON);

  // append board name
  GetTextIndexed (str_text, sizeof (str_text), serialrelay_board.type, kRelaySerialBoard);
  ResponseAppend_P (PSTR ("\"%s\":\"%s\""), RELAYSERIAL_JSON_NAME, str_text);

  // append baud rate
  ResponseAppend_P (PSTR (",\"%s\":%d"), RELAYSERIAL_JSON_RATE, serialrelay_board.baud_rate);

  // append number of relays
  ResponseAppend_P (PSTR (",\"%s\":%d"), RELAYSERIAL_JSON_RELAY, serialrelay_board.nb_relay);

  // append command delay
  ResponseAppend_P (PSTR (",\"%s\":%d"), RELAYSERIAL_JSON_DELAY, serialrelay_board.delay_ms);

  // append end of section
  ResponseAppend_P (PSTR ("}"));
}

/**************************************\
 *                   Web
\**************************************/

#ifdef USE_WEBSERVER

// append relay status to main page
void RelaySerialWebSensor ()
{
  uint32_t duration;
  char     str_status[16];
  char     str_duration[16];

  // get board status
  GetTextIndexed (str_status, sizeof (str_status), serialrelay_board.status, kRelaySerialStatus);

  // if hanshake, calculate delay
  strcpy (str_duration, "");
  if (serialrelay_board.status == RELAYSERIAL_STATUS_HANDSHAKE)
  {
    duration = millis () - serialrelay_board.last_time;
    sprintf (str_duration, " (%d)", duration / 1000);
  }

  // display status
  WSContentSend_PD (PSTR ("{s}%s %s{m}%s%s{e}"), D_RELAYSERIAL_BOARD, D_RELAYSERIAL_STATUS, str_status, str_duration);
}

// append Serial Relay configuration button
void RelaySerialWebConfigButton ()
{
  // heater and meter configuration button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>"), D_RELAYSERIAL_PAGE_CONFIG, D_RELAYSERIAL_CONFIG);
}

// board selection page
void RelaySerialWebPageConfig ()
{
  int  index, board_type;
  char str_text[256];
  char str_checked[16];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess ()) return;

  // get teleinfo mode according to ETH parameter
  if (Webserver->hasArg (D_RELAYSERIAL_CMND))
  {
    WebGetArg (D_RELAYSERIAL_CMND, str_text, sizeof (str_text));
    index = atoi (str_text);

    // if board is referenced
    if (index < RELAYSERIAL_BOARD_MAX)
    {
      // save board selection
      RelaySerialSetBoardType (index);

      // apply template as default one
      snprintf_P (str_text, sizeof (str_text), PSTR(D_CMND_BACKLOG " " D_CMND_TEMPLATE " %s; %s 0"), arr_serialrelay_tmpl[index], D_CMND_MODULE);
      ExecuteWebCommand (str_text, SRC_WEBGUI);

      // if some data has been updated, ask for reboot
      WebRestart(1);
    }
  }

  // get current board type
  board_type = RelaySerialGetBoardType ();

  // beginning of form
  WSContentStart_P (D_RELAYSERIAL_CONFIG);
  WSContentSendStyle ();
  WSContentSend_P (RELAYSERIAL_FORM_START, D_RELAYSERIAL_PAGE_CONFIG);

  // board selection
  WSContentSend_P (RELAYSERIAL_FIELD_START, D_SERIALRELAY);
  for (index = 0; index < RELAYSERIAL_BOARD_MAX; index ++) 
  {
    GetTextIndexed (str_text, sizeof (str_text), index, kRelaySerialBoard);
    if (index == board_type) strcpy (str_checked, "checked"); else strcpy (str_checked, "");
    WSContentSend_P (RELAYSERIAL_INPUT_TEXT, D_RELAYSERIAL_CMND, index, str_checked, str_text);
  }
  WSContentSend_P (RELAYSERIAL_FIELD_STOP);
   
  // save button
  WSContentSend_P (PSTR ("<button name='save' type='submit' class='button bgrn'>%s</button>"), D_SAVE);
  WSContentSend_P (RELAYSERIAL_FORM_STOP);

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

#endif  // USE_WEBSERVER

/************************************\
 *              Interface
\************************************/

// serial relay board driver
bool Xdrv96 (uint32_t function)
{
  // swtich according to context
  switch (function) 
  {
    case FUNC_PRE_INIT:
      RelaySerialInit ();
      break;
    case FUNC_EVERY_100_MSECOND:
      if (TasmotaGlobal.uptime > RELAYSERIAL_INIT_TIMEOUT) RelaySerialEvery100ms ();
      break;
    case FUNC_JSON_APPEND:
      RelaySerialAppendJSON ();
      break;
    case FUNC_SAVE_BEFORE_RESTART:
      RelaySerialBeforeRestart ();
      break;
#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      RelaySerialWebSensor ();
      break;
    case FUNC_WEB_ADD_HANDLER:
      // pages
      Webserver->on (D_RELAYSERIAL_PAGE_CONFIG, RelaySerialWebPageConfig);
      break;
    case FUNC_WEB_ADD_BUTTON:
    case FUNC_WEB_ADD_MAIN_BUTTON:
      RelaySerialWebConfigButton ();
      break;
#endif  // USE_WEBSERVER
  }
  return false;
}

#endif   // USE_RELAY_SERIAL

