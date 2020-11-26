/*
  xdrv_96_icse.ino - Driver for relay boards based on ICSE012A, ICSE013A, ICSE014A
  
  Copyright (C) 2020  Nicolas Bernaerts

    20/11/2020 - v1.0 - Creation

  Settings are stored using weighting scale parameters :
    - Settings.weight_reference = Last state of relays
 
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

#ifdef USE_ICSE

/**************************************************\
 * ICSE012A, ICSE013A, ICSE014A boards
 * To enable ICSE board management, select :
     GPIO1 = Serial TX
     GPIO3 = Serial RX
\**************************************************/

/**************************************\
 *               Variables
\**************************************/

// declare teleinfo energy driver and sensor
#define XDRV_96    96

// JSON message
#define ICSE_JSON         "ICSE"
#define ICSE_JSON_CODE    "Code"
#define ICSE_JSON_NAME    "Name"
#define ICSE_JSON_RELAY   "Relay"

// strings
#define D_ICSE_BOARD      "Board"

// board arrays
const char *const arr_icse_board[] PROGMEM = { "ICSE012A", "ICSE014A", "ICSE013A", "Unknown" };      // board reference
const uint8_t     arr_icse_relay[]         = { 4, 8, 2, 0};                                           // number of relays

// enum
enum IcseBoards { ICSE_BOARD_ICSE012A, ICSE_BOARD_ICSE014A, ICSE_BOARD_ICSE013A, ICSE_BOARD_MAX };
enum IcseInitSteps { ICSE_STEP_SERIAL, ICSE_STEP_HANDSHAKE, ICSE_STEP_ACTIVATE, ICSE_STEP_RESTORE, ICSE_STEP_READY };

// board status
struct {
  uint8_t init_step   = ICSE_STEP_SERIAL;
  uint8_t board_code  = 0;
  uint8_t board_index = ICSE_BOARD_MAX;
  uint8_t board_relay = 8;
} icse_state;


/**************************************\
 *               Accessor
\**************************************/

// set relay state
void IcseSetRelayState (uint8_t new_state)
{
  bool     relay_state, target_state;
  uint8_t  index, target_select;
  uint32_t device_state;

  // save relay state
  Settings.weight_reference = new_state;

  // if board initialised
  if (icse_state.init_step >= ICSE_STEP_RESTORE)
  {

    // send command to board (inverted state)
    Serial.write (~new_state);

    // log
    AddLog_P2 (LOG_LEVEL_INFO, PSTR("COM: Board %s set to %d"), arr_icse_board[icse_state.board_index], new_state);
  }

  // loop to set internal relay state accordingly
  for (index = 0; index < icse_state.board_relay; index ++)
  {
    // read relay state
    relay_state = (bool)bitRead (TasmotaGlobal.power, index);

    // check target relay state
    target_select = (uint8_t)pow (2, index) & new_state;
    target_state  = (target_select != 0);

    // if state and target are different
    if (relay_state != target_state)
    {
      if (target_state) ExecuteCommandPower(index + 1, POWER_ON, SRC_MAX);
      else ExecuteCommandPower(index + 1, POWER_OFF, SRC_MAX);
    }
  }
}

// get relay state
uint8_t IcseGetRelayState ()
{
  uint8_t relay_state;

  // get relay state
  relay_state = Settings.weight_reference;

  return relay_state;
}

/***************************************\
 *               Functions
\***************************************/

void IcseInit ()
{
  // check if serial ports are selected 
  if (PinUsed(GPIO_TXD) && PinUsed(GPIO_RXD))
  {
    // start ESP8266 serial port
    Serial.flush ();
    Serial.begin (9600, SERIAL_8N1);
    ClaimSerial ();

    // next step and log
    icse_state.init_step++;
    AddLog_P2 (LOG_LEVEL_INFO, PSTR("COM: Init serial at 9600"));
  }
}

void IcseEverySecond ()
{
  uint8_t relay_state;

  // swtich according to context
  switch (icse_state.init_step) 
  {
    case ICSE_STEP_HANDSHAKE:
      // send handshake and next step
      Serial.write (0x50);
      icse_state.init_step++;
      break;

    case ICSE_STEP_ACTIVATE:
      // if board type has been received
      if (Serial.available() > 0)
      {
        icse_state.board_code = Serial.read ();
        if ((icse_state.board_code >= 0xAB) && (icse_state.board_code <= 0xAD))
        {
          icse_state.board_index = icse_state.board_code - 0xAB;
          icse_state.board_relay = arr_icse_relay[icse_state.board_index];
        }
        else icse_state.board_code = 0;
      } 

      // activate relays and next step
      Serial.write (0x51);
      icse_state.init_step++;

      // set number of relays
      TasmotaGlobal.devices_present = icse_state.board_relay;
      break;

    case ICSE_STEP_RESTORE:
      // restore previous relay state and next step
      relay_state = IcseGetRelayState ();
      IcseSetRelayState (relay_state);
      icse_state.init_step++;
      break;
  }
}

// Handle relay commands
void IcseHandleCommand (uint32_t device, uint32_t state)
{
  uint8_t  relay_state, device_select, device_invert, device_result;
  uint32_t new_state;

  // get last relays state
  relay_state = IcseGetRelayState ();

  // prepare specific relay state
  device_select = (uint8_t)pow (2, device - 1);
  device_invert = UINT8_MAX - device_select;

  // calculate relay states according to ON, OFF or TOGGLE
  if (state == POWER_OFF) relay_state &= device_invert;
  else if (state == POWER_ON) relay_state |= device_select;
  else if (state == POWER_TOGGLE) relay_state ^= device_select;

  // save and send new relays state to board
  IcseSetRelayState (relay_state);
}

// Append board JSON status
void IcseAppendJSON ()
{
  String str_json;

  // generate JSON string
  str_json  = "\"" + String (ICSE_JSON) + "\":{";
  str_json += "\"" + String (ICSE_JSON_CODE)  + "\":"   + String (icse_state.board_code) + ",";
  str_json += "\"" + String (ICSE_JSON_NAME)  + "\":\"" + String (arr_icse_board[icse_state.board_index]) + "\",";
  str_json += "\"" + String (ICSE_JSON_RELAY) + "\":"   + String (icse_state.board_relay) + "}";

  // append string to MQTT message
  ResponseAppend_P (PSTR(",%s"), str_json.c_str ());
}

/**************************************\
 *                   Web
\**************************************/

#ifdef USE_WEBSERVER

// append relay status to main page
bool IcseWebSensor ()
{
  // display device type
  WSContentSend_PD (PSTR("{s}%s{m}%s (%d relays){e}"), D_ICSE_BOARD, arr_icse_board[icse_state.board_index], icse_state.board_relay);
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
    case FUNC_INIT:
      IcseInit ();
      break;
    case FUNC_EVERY_SECOND:
      if (TasmotaGlobal.uptime > 4) IcseEverySecond ();
      break;
    case FUNC_JSON_APPEND:
      IcseAppendJSON ();
      break;
#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      IcseWebSensor ();
      break;
#endif  // USE_WEBSERVER
  }
  return false;
}

#endif   // USE_ICSE

