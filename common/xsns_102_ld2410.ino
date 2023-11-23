/*
  xsns_102_ld2410.ino - Driver for Presence and Movement sensor HLK-LD2410

  Copyright (C) 2022  Nicolas Bernaerts

  Connexions :
    * GPIO1 should be declared as LD2410 Tx and connected to HLK-LD2410 Rx
    * GPIO3 should be declared as LD2410 Rx and connected to HLK-LD2410 Tx

  Baud rate is forced at 256000.
  
  Call LD2410InitDevice (timeout) to declare the device and make it operational

  Settings are stored using unused parameters :
    - Settings->rf_code[2][2] : Presence detection timeout (sec.)
    - Settings->rf_code[2][3] : Presence detection number of samples to average

  Version history :
    28/06/2022 - v1.0 - Creation
    15/01/2023 - v2.0 - Complete rewrite
    03/04/2023 - v2.1 - Add trigger to avoid false detection
    12/09/2023 - v2.2 - Switch to LD2410 Rx & LD2410 Tx
    20/11/2023 - v2.3 - Tasmota 13.2 compatibility
    23/11/2023 - v2.4 - Add bluetooth command (thanks to protectivedad)

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

#ifdef USE_LD2410

#include <TasmotaSerial.h>

/*************************************************\
 *               Variables
\*************************************************/

// declare teleinfo energy driver and sensor
#define XSNS_102                        102

// constant
#define LD2410_START_DELAY              10       // sensor startup delay
#define LD2410_DEFAULT_TIMEOUT          5        // timeout to trigger inactivity (sec.)
#define LD2410_DEFAULT_SAMPLE           10       // number of samples to average
#define LD2410_GATE_MAX                 9        // number of sensor gates
#define LD2410_MSG_SIZE_MAX             64       // maximum message size

#define LD2410_TRIGGER_MIN              2000     // minimum activity to trigger detection (ms)
#define LD2410_TRIGGER_MAX              4000     // maximum activity to trigger detection (ms)


#define LD2410_COLOR_DISABLED           "none"
#define LD2410_COLOR_NONE               "#555"
#define LD2410_COLOR_PRESENCE           "#E80"
#define LD2410_COLOR_MOTION             "#D00"

// strings
const char D_LD2410_NAME[] PROGMEM =    "HLK-LD2410";

// ----------------
// binary commands
// ----------------

enum LD2410StepCommand { LD2410_STEP_NONE, LD2410_STEP_BEFORE_START, LD2410_STEP_AFTER_START, LD2410_STEP_BEFORE_COMMAND, LD2410_STEP_AFTER_COMMAND, LD2410_STEP_BEFORE_STOP, LD2410_STEP_AFTER_STOP };       // steps to run a command
enum LD2410ListCommand { LD2410_CMND_START, LD2410_CMND_STOP, LD2410_CMND_READ_PARAM, LD2410_CMND_DIST_TIMEOUT, LD2410_CMND_SENSITIVITY, LD2410_CMND_READ_FIRMWARE, LD2410_CMND_RESET, LD2410_CMND_RESTART, LD2410_CMND_CLOSE_ENGINEER, LD2410_CMND_OPEN_ENGINEER, LD2410_CMND_DATA, LD2410_CMND_BLUETOOTH_ON, LD2410_CMND_BLUETOOTH_OFF, LD2410_CMND_MAX };       // list of available commands

// commands
uint8_t ld2410_cmnd_header[]         PROGMEM = { 0xfd, 0xfc, 0xfb, 0xfa };
uint8_t ld2410_cmnd_footer[]         PROGMEM = { 0x04, 0x03, 0x02, 0x01 };
uint8_t ld2410_cmnd_start[]          PROGMEM = { 0x04, 0x00, 0xff, 0x00, 0x01, 0x00 };
uint8_t ld2410_cmnd_stop[]           PROGMEM = { 0x02, 0x00, 0xfe, 0x00 };
uint8_t ld2410_cmnd_read_param[]     PROGMEM = { 0x02, 0x00, 0x61, 0x00 };
uint8_t ld2410_cmnd_dist_timeout[]   PROGMEM = { 0x14, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 };
uint8_t ld2410_cmnd_sensitivity[]    PROGMEM = { 0x14, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 };
uint8_t ld2410_cmnd_read_firmware[]  PROGMEM = { 0x02, 0x00, 0xa0, 0x00 };
uint8_t ld2410_cmnd_serial_rate[]    PROGMEM = { 0x04, 0x00, 0xa1, 0x00, 0x07, 0x00 };
uint8_t ld2410_cmnd_reset[]          PROGMEM = { 0x02, 0x00, 0xa2, 0x00 };
uint8_t ld2410_cmnd_restart[]        PROGMEM = { 0x02, 0x00, 0xa3, 0x00 };
uint8_t ld2410_cmnd_open_engineer[]  PROGMEM = { 0x02, 0x00, 0x62, 0x00 };
uint8_t ld2410_cmnd_close_engineer[] PROGMEM = { 0x02, 0x00, 0x63, 0x00 };
uint8_t ld2410_cmnd_bluetooth_on[]   PROGMEM = { 0x04, 0x00, 0xa4, 0x00, 0x01, 0x00 };
uint8_t ld2410_cmnd_bluetooth_off[]  PROGMEM = { 0x04, 0x00, 0xa4, 0x00, 0x00, 0x00 };

// MQTT commands : ld_help and ld_send
const char kHLKLD2410Commands[]         PROGMEM = "ld2410_|help|timeout|sample|param|eng|reset|restart|firmware|gate|max|bluetooth";
void (* const HLKLD2410Command[])(void) PROGMEM = { &CmndLD2410Help, &CmndLD2410Timeout, &CmndLD2410Sample, &CmndLD2410ReadParam, &CmndLD2410EngineeringMode, &CmndLD2410Reset, &CmndLD2410Restart, &CmndLD2410Firmware, &CmndLD2410SetGateParam, &CmndLD2410SetGateMax, &CmndLD2410Bluetooth };

/****************************************\
 *                 Data
\****************************************/

// LD2410 received message
static struct {    
  uint32_t timestamp = UINT32_MAX;            // timestamp of last received character
  uint8_t  idx_body  = 0;                     // index of received body
  uint8_t  arr_body[LD2410_MSG_SIZE_MAX];     // body of current received message
  uint8_t  arr_last[4] = {0, 0, 0, 0};        // last received characters
} ld2410_received; 

// LD2410 commands queue
static struct {
  uint8_t step = LD2410_STEP_NONE;
  uint8_t gate = 0;                                 // gate to handle
  String  str_queue;
} ld2410_command; 

// LD2410 configuration
struct {
  uint8_t sample  = LD2410_DEFAULT_SAMPLE;          // default running mode
} ld2410_config;

// LD2410 status
struct ld2410_firmware
{
  uint8_t  major    = 0;          // major version
  uint8_t  minor    = 0;          // minor version
  uint32_t revision = 0;          // revision
};
struct ld2410_sensor
{
  bool     active;
  uint8_t  max_gate;                                    // detection max gate
  uint8_t  power;                                       // detection power
  uint8_t  avg_count;
  uint16_t distance;                                    // detection distance
  uint16_t sum_power;
  uint16_t sum_distance;
  uint8_t  arr_sensitivity[LD2410_GATE_MAX];            // gates sensitivity
};
static struct {
  TasmotaSerial  *pserial   = nullptr;                  // pointer to serial port
  bool            enabled   = false;                    // sensor enabled
  uint32_t        timestamp = 0;                        // timestamp of last detection
  uint16_t        timeout = LD2410_DEFAULT_TIMEOUT;     // target humidity level
  ld2410_firmware firmware; 
  ld2410_sensor   motion; 
  ld2410_sensor   presence; 
} ld2410_status; 

/**************************************************\
 *                  Commands
\**************************************************/

// hlk-ld sensor help
void CmndLD2410Help ()
{
  // help on command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ld2410_timeout <value> = set timeout (sec.)"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ld2410_sample <value>  = set number of sample"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ld2410_param      = read sensor parameters"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ld2410_firmware   = get sensor firmware"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ld2410_eng <0/1>  = set engineering mode"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ld2410_reset      = reset sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ld2410_restart    = restart sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ld2410_gate <gate,pres,motion> = set gate sensitivity"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ld2410_max <pres,motion>       = set detection max gate "));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ld2410_bluetooth <0/1>         = set bluetooth"));
 
  ResponseCmndDone ();
}

void CmndLD2410Timeout ()
{
  if (XdrvMailbox.payload > 0)
  {
    ld2410_status.timeout = XdrvMailbox.payload;
    LD2410SaveConfig ();
    LD2410AppendCommand (LD2410_CMND_DIST_TIMEOUT);
  }
  ResponseCmndNumber (ld2410_status.timeout);
}

void CmndLD2410Sample ()
{
  if (XdrvMailbox.payload > 0)
  {
    ld2410_config.sample = XdrvMailbox.payload;
    LD2410SaveConfig ();
  }
  ResponseCmndNumber (ld2410_config.sample);
}

void CmndLD2410ReadParam ()
{
  LD2410AppendCommand (LD2410_CMND_READ_PARAM);
  ResponseCmndDone ();
}

void CmndLD2410EngineeringMode ()
{
  if (XdrvMailbox.payload == 1) LD2410AppendCommand (LD2410_CMND_OPEN_ENGINEER);
    else LD2410AppendCommand (LD2410_CMND_CLOSE_ENGINEER);
  ResponseCmndDone ();
}

void CmndLD2410Firmware ()
{
  LD2410AppendCommand (LD2410_CMND_READ_FIRMWARE);
  ResponseCmndDone ();
}

void CmndLD2410Reset ()
{
  LD2410AppendCommand (LD2410_CMND_RESET);
  ResponseCmndDone ();
}

void CmndLD2410Restart ()
{
  LD2410AppendCommand (LD2410_CMND_RESTART);
  ResponseCmndDone ();
}

void CmndLD2410SetGateParam ()
{
  bool    result = false;
  uint8_t index = 0;
  uint8_t gate = UINT8_MAX;
  uint8_t presence = UINT8_MAX;
  uint8_t motion = UINT8_MAX;
  char *pstr_token;
  char str_data[16];

  if (XdrvMailbox.data_len > 0)
  {
    // loop thru tokens
    strcpy (str_data, XdrvMailbox.data);
    pstr_token = strtok (str_data, ",");
    while( pstr_token != nullptr)
    {
      // if value is given, add update command
      if (strlen (pstr_token) > 0)
      {
        if (index == 0) gate = (uint8_t)atoi (pstr_token);
        else if (index == 1) presence = (uint8_t)atoi (pstr_token);
        else if (index == 2) motion = (uint8_t)atoi (pstr_token);
      }

      // search for next token    
      pstr_token = strtok (nullptr, ",");
      index ++;
    }
  }

  // if parameters are provided, update gate
  result = ((gate < LD2410_GATE_MAX) && (presence <= 100) && (motion <= 100));
  if (result)
  {
    ld2410_command.gate = gate;
    ld2410_status.presence.arr_sensitivity[gate] = presence;
    ld2410_status.motion.arr_sensitivity[gate] = motion;
    LD2410AppendCommand (LD2410_CMND_SENSITIVITY);
  }

  // if gate is provided, display param
  if (gate < LD2410_GATE_MAX)
  {
    sprintf (str_data, "%u,%u,%u", gate, ld2410_status.presence.arr_sensitivity[gate], ld2410_status.motion.arr_sensitivity[gate]);
    ResponseCmndChar (str_data);
  }

  // else command failed
  else ResponseCmndFailed ();
}

void CmndLD2410SetGateMax ()
{
  bool    result = false;
  uint8_t index = 0;
  uint8_t max_presence = UINT8_MAX;
  uint8_t max_motion = UINT8_MAX;
  char *pstr_token;
  char str_data[16];

  if (XdrvMailbox.data_len > 0)
  {
    // loop thru tokens
    strcpy (str_data, XdrvMailbox.data);
    pstr_token = strtok (str_data, ",");
    while( pstr_token != nullptr)
    {
      // if value is given, add update command
      if (strlen (pstr_token) > 0)
      {
        if (index == 0) max_presence = (uint8_t)atoi (pstr_token);
        else if (index == 1) max_motion = (uint8_t)atoi (pstr_token);
      }

      // search for next token    
      pstr_token = strtok (nullptr, ",");
      index ++;
    }
  }

  // if parameters are provided, update gate
  if ((max_presence <= LD2410_GATE_MAX) && (max_motion <= LD2410_GATE_MAX))
  {
    ld2410_status.presence.max_gate = max_presence;
    ld2410_status.motion.max_gate   = max_motion;
    LD2410AppendCommand (LD2410_CMND_DIST_TIMEOUT);
    LD2410AppendCommand (LD2410_CMND_READ_PARAM);
  }

  // display status
  sprintf (str_data, "%u,%u", ld2410_status.presence.max_gate, ld2410_status.motion.max_gate);
  ResponseCmndChar (str_data);
}

void CmndLD2410Bluetooth ()
{
  if (XdrvMailbox.payload == 1) LD2410AppendCommand (LD2410_CMND_BLUETOOTH_ON);
    else LD2410AppendCommand (LD2410_CMND_BLUETOOTH_OFF);
  ResponseCmndDone ();
}

/**************************************************\
 *                  Config
\**************************************************/

// Load configuration from flash memory
void LD2410LoadConfig ()
{
  // read parameters
//  ld2410_status.timeout = Settings->rf_code[2][2];
  ld2410_config.sample  = Settings->rf_code[2][3];

  // check parameters
  if (ld2410_config.sample == 0) ld2410_config.sample = LD2410_DEFAULT_SAMPLE;
}

// Save configuration into flash memory
void LD2410SaveConfig ()
{
//  Settings->rf_code[2][2] = ld2410_status.timeout;
  Settings->rf_code[2][3] = ld2410_config.sample;
}

/**************************************************\
 *                  Functions
\**************************************************/

bool LD2410GetDetectionStatus (const uint32_t delay)
{
  uint32_t timeout = delay;

  // if timestamp not defined, no detection
  if (ld2410_status.timestamp == 0) return false;
  
  // if no timeout given, use default one
  if ((timeout == 0) || (timeout == UINT32_MAX)) timeout = ld2410_status.timeout;

  // return timeout status
  return (ld2410_status.timestamp + timeout > LocalTime ());
}

// driver initialisation
bool LD2410InitDevice ()
{
  // if not done, init sensor state
  if ((ld2410_status.pserial == nullptr) && PinUsed (GPIO_LD2410_RX) && PinUsed (GPIO_LD2410_TX))
  {
    // force baud rate to 256000
    Settings->baudrate = 853;

    // create serial port
    ld2410_status.pserial = new TasmotaSerial (Pin (GPIO_LD2410_RX), Pin (GPIO_LD2410_TX), 2);

    // initialise serial port
    ld2410_status.enabled = ld2410_status.pserial->begin (256000, SERIAL_8N1);

#ifdef ESP8266
    // force hardware configuration on ESP8266
    if (ld2410_status.enabled && ld2410_status.pserial->hardwareSerial ()) ClaimSerial ();
#endif      // ESP8266

    // log
    if (ld2410_status.enabled) AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s sensor init at %u"), D_LD2410_NAME, 256000);
      else AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s sensor init failed"), D_LD2410_NAME);
  }

  // first commands
  LD2410AppendCommand (LD2410_CMND_CLOSE_ENGINEER);
  LD2410AppendCommand (LD2410_CMND_READ_PARAM);
  LD2410AppendCommand (LD2410_CMND_READ_FIRMWARE);

  return ld2410_status.enabled;
}

void LD2410AppendCommand (const uint8_t command)
{
  uint8_t index, actual;

  // check parameter
  if (command >= LD2410_CMND_MAX) return;

  // add command
  if (ld2410_command.str_queue.length () > 0) ld2410_command.str_queue += ";";
  ld2410_command.str_queue += command;
}

// send command
void LD2410SendCommand (const uint8_t command)
{
  uint8_t  size_buffer = 0;
  uint8_t  arr_buffer[24];
  uint16_t value;

  // empty send buffer
  ld2410_status.pserial->setReadChunkMode (1);                            // Enable chunk mode introducing possible Hardware Watchdogs
  ld2410_status.pserial->flush ();

  switch (command)
  {
    case LD2410_CMND_START:
      size_buffer = sizeof (ld2410_cmnd_start);
      memcpy_P (arr_buffer, ld2410_cmnd_start, size_buffer);
      AddLog (LOG_LEVEL_INFO, PSTR ("HLK: cmnd enable"));
      break;

    case LD2410_CMND_STOP:
      size_buffer = sizeof (ld2410_cmnd_stop);
      memcpy_P (arr_buffer, ld2410_cmnd_stop, size_buffer);
      AddLog (LOG_LEVEL_INFO, PSTR ("HLK: cmnd disable"));
      break;

    case LD2410_CMND_READ_PARAM:
      size_buffer = sizeof (ld2410_cmnd_read_param);
      memcpy_P (arr_buffer, ld2410_cmnd_read_param, size_buffer);
      AddLog (LOG_LEVEL_INFO, PSTR ("HLK: cmnd read param"));
      break;

    case LD2410_CMND_DIST_TIMEOUT:
      size_buffer = sizeof (ld2410_cmnd_dist_timeout);
      memcpy_P (arr_buffer, ld2410_cmnd_dist_timeout, size_buffer);
      arr_buffer[6]  = ld2410_status.motion.max_gate;
      arr_buffer[12] = ld2410_status.presence.max_gate;
      ((uint16_t*)(arr_buffer + 18))[0] = ld2410_status.timeout;
      AddLog (LOG_LEVEL_INFO, PSTR ("HLK: cmnd set max gate pres %u, motion %u & timeout %u"), ld2410_status.presence.max_gate, ld2410_status.motion.max_gate, ld2410_status.timeout);
      break;

    case LD2410_CMND_SENSITIVITY:
      size_buffer = sizeof (ld2410_cmnd_sensitivity);
      memcpy_P (arr_buffer, ld2410_cmnd_sensitivity, size_buffer);
      arr_buffer[6] = ld2410_command.gate;
      arr_buffer[12] = ld2410_status.motion.arr_sensitivity[ld2410_command.gate];
      arr_buffer[18] = ld2410_status.presence.arr_sensitivity[ld2410_command.gate];
      AddLog (LOG_LEVEL_INFO, PSTR ("HLK: cmnd set for gate %u : pres %u & motion %u"), ld2410_command.gate, ld2410_status.presence.arr_sensitivity[ld2410_command.gate], ld2410_status.motion.arr_sensitivity[ld2410_command.gate]);
      break;

    case LD2410_CMND_READ_FIRMWARE:
      size_buffer = sizeof (ld2410_cmnd_read_firmware);
      memcpy_P (arr_buffer, ld2410_cmnd_read_firmware, size_buffer);
      AddLog (LOG_LEVEL_INFO, PSTR ("HLK: cmnd read firmware"));
      break;

    case LD2410_CMND_RESET:
      size_buffer = sizeof (ld2410_cmnd_reset);
      memcpy_P (arr_buffer, ld2410_cmnd_reset, size_buffer);
      AddLog (LOG_LEVEL_INFO, PSTR ("HLK: cmnd reset"));
      break;

    case LD2410_CMND_RESTART:
      size_buffer = sizeof (ld2410_cmnd_restart);
      memcpy_P (arr_buffer, ld2410_cmnd_restart, size_buffer);
      AddLog (LOG_LEVEL_INFO, PSTR ("HLK: cmnd restart"));
      break;

    case LD2410_CMND_OPEN_ENGINEER:
      size_buffer = sizeof (ld2410_cmnd_open_engineer);
      memcpy_P (arr_buffer, ld2410_cmnd_open_engineer, size_buffer);
      AddLog (LOG_LEVEL_INFO, PSTR ("HLK: cmnd engineering ON"));
      break;

    case LD2410_CMND_CLOSE_ENGINEER:
      size_buffer = sizeof (ld2410_cmnd_close_engineer);
      memcpy_P (arr_buffer, ld2410_cmnd_close_engineer, size_buffer);
      AddLog (LOG_LEVEL_INFO, PSTR ("HLK: cmnd engineering OFF"));
      break;

    case LD2410_CMND_BLUETOOTH_ON:
      size_buffer = sizeof (ld2410_cmnd_bluetooth_on);
      memcpy_P (arr_buffer, ld2410_cmnd_bluetooth_on, size_buffer);
      AddLog (LOG_LEVEL_INFO, PSTR ("HLK: cmnd bluetooth ON"));
      break;

    case LD2410_CMND_BLUETOOTH_OFF:
      size_buffer = sizeof (ld2410_cmnd_bluetooth_off);
      memcpy_P (arr_buffer, ld2410_cmnd_bluetooth_off, size_buffer);
      AddLog (LOG_LEVEL_INFO, PSTR ("HLK: cmnd bluetooth OFF"));
      break;
  }

  // if command defined
  if (size_buffer > 0)
  {
    // send command
    ld2410_status.pserial->write (ld2410_cmnd_header, 4);
    ld2410_status.pserial->write (arr_buffer, size_buffer);
    ld2410_status.pserial->write (ld2410_cmnd_footer, 4);

    // if in debug mode, log command to console
    LD2410LogMessage (arr_buffer, size_buffer, true);
  }
}

// Handling of received data
void LD2410HandleReceivedMessage ()
{
  uint8_t  index;
  uint16_t gate;
  uint16_t *pcommand;
  uint16_t *pvalue;
  uint32_t *pheader;
  uint32_t *prevision;

  // if in debug mode, log data to console
  LD2410LogMessage (ld2410_received.arr_body, ld2410_received.idx_body, false);

  // handle header
  pheader = (uint32_t*)&ld2410_received.arr_body;
  switch (*pheader)
  {
    // ---------------------
    //   command reception
    // ---------------------
    case 0xfafbfcfd:
    
      // step is after command reception
      ld2410_command.step = LD2410_STEP_AFTER_COMMAND;

      // handle command
      pcommand = (uint16_t*)(ld2410_received.arr_body + 6);
      switch (*pcommand)
      {
        // command start
        case 0x01ff:
          ld2410_command.step = LD2410_STEP_AFTER_START;
          break;

        // command end
        case 0x01fe:
          ld2410_command.step = LD2410_STEP_AFTER_STOP;
          break;
        
        // read radar parameters
        //  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37
        // FD FC FB FA 1C 00 61 01 00 00 AA 08 08 08 32 32 28 1E 14 0F 0F 0F 0F 00 00 28 28 1E 1E 14 14 14 05 00 04 03 02 01
        // header     | len |cw cv| ack |hd|dd|md|sd| moving sensitivity 0..8  | static sensitivity 0..8  |timed|trailer
        //            | 28  |     |  0  |  | 8| 8| 8|50 50 40 30 20 15 15 15 15| 0  0 40 40 30 30 20 20 20|  5  |
        case 0x0161:
          ld2410_status.motion.max_gate   = ld2410_received.arr_body[12];
          ld2410_status.presence.max_gate = ld2410_received.arr_body[13];
          for (index = 0; index < LD2410_GATE_MAX; index ++) ld2410_status.motion.arr_sensitivity[index] = ld2410_received.arr_body[14 + index];
          for (index = 0; index < LD2410_GATE_MAX; index ++) ld2410_status.presence.arr_sensitivity[index] = ld2410_received.arr_body[23 + index];
          pvalue = (uint16_t*)(ld2410_received.arr_body + 32);
          ld2410_status.timeout = *pvalue;
          break;

        // set max gate and timeout
        case 0x0160:
          LD2410AppendCommand (LD2410_CMND_READ_PARAM);
          break;

        // set gate sensitivity
        case 0x0164:
          LD2410AppendCommand (LD2410_CMND_READ_PARAM);
          break;

        // read firmware
        //  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21
        // FD FC FB FA 0C 00 A0 01 00 00 00 01 07 01 16 15 09 22 04 03 02 01
        // header     | len |ty|hd| ack |ftype| mum | revision  |trailer
        //            | 12  |  | 1|  0  |  256| 1.7 | 22091516  |
        case 0x01a0:
          ld2410_status.firmware.minor = ld2410_received.arr_body[12];
          ld2410_status.firmware.major = ld2410_received.arr_body[13];
          prevision = (uint32_t*)(ld2410_received.arr_body + 14);
          ld2410_status.firmware.revision = *prevision;
          break;

        // serial rate
        case 0x01a1:
          break;

        // reset device
        case 0x01a2:
          AddLog (LOG_LEVEL_INFO, PSTR ("HLK: device got reset"));
          break;

        // restart device
        case 0x01a3:
          AddLog (LOG_LEVEL_INFO, PSTR ("HLK: device restarted"));
          break;

        // open engineering mode
        case 0x0162:
          AddLog (LOG_LEVEL_INFO, PSTR ("HLK: engineering mode is ON"));
          break;

        // close engineering mode
        case 0x0163:
          AddLog (LOG_LEVEL_INFO, PSTR ("HLK: engineering mode is OFF"));
          break;

        // bluetooth
        case 0x01a4:
          AddLog (LOG_LEVEL_INFO, PSTR ("HLK: bluetooth command sent"));
          break;
      }
      break;

    // ---------------------
    //    data reception
    // ---------------------
    case 0xf1f2f3f4:

      // motion : update average data
      pvalue = (uint16_t*)(ld2410_received.arr_body + 9);
      ld2410_status.motion.sum_distance += *pvalue;
      ld2410_status.motion.sum_power    += (uint16_t)ld2410_received.arr_body[11];
      ld2410_status.motion.avg_count++;

      // motion : number of samples reached
      if (ld2410_status.motion.avg_count >= ld2410_config.sample)
      {
        // calculate distance and power
        ld2410_status.motion.distance = ld2410_status.motion.sum_distance / ld2410_status.motion.avg_count;
        ld2410_status.motion.power    = ld2410_status.motion.sum_power / ld2410_status.motion.avg_count;

        // update detection status
        gate = ld2410_status.motion.distance / 75;
        if (gate < ld2410_status.motion.max_gate) ld2410_status.motion.active = (ld2410_status.motion.power >= ld2410_status.motion.arr_sensitivity[gate]);
          else ld2410_status.motion.active = false;
        if (ld2410_status.motion.active) ld2410_status.timestamp = LocalTime ();
        
        // reset average counters
        ld2410_status.motion.avg_count    = 0;
        ld2410_status.motion.sum_distance = 0;
        ld2410_status.motion.sum_power    = 0;
      }

      // presence : update average data
      pvalue   = (uint16_t*)(ld2410_received.arr_body + 12);
      ld2410_status.presence.sum_distance += *pvalue;
      ld2410_status.presence.sum_power    += (uint16_t)ld2410_received.arr_body[14];
      ld2410_status.presence.avg_count++;

      // presence : number of samples reached
      if (ld2410_status.presence.avg_count >= ld2410_config.sample)
      {
        // calculate distance and power
        ld2410_status.presence.distance = ld2410_status.presence.sum_distance / ld2410_status.presence.avg_count;
        ld2410_status.presence.power    = ld2410_status.presence.sum_power / ld2410_status.presence.avg_count;

        // update detection status
        gate = ld2410_status.presence.distance / 75;
        if (gate < ld2410_status.presence.max_gate) ld2410_status.presence.active = (ld2410_status.presence.power >= ld2410_status.presence.arr_sensitivity[gate]);
          else ld2410_status.presence.active = false;
        if (ld2410_status.presence.active) ld2410_status.timestamp = LocalTime ();

        // reset average counters
        ld2410_status.presence.avg_count    = 0;
        ld2410_status.presence.sum_distance = 0;
        ld2410_status.presence.sum_power    = 0;
      }
      break;
  }
}

/*********************************************\
 *                   Callback
\*********************************************/

// driver initialisation
void LD2410Init ()
{
  uint8_t index;

  // init motion sensor
  ld2410_status.motion.max_gate     = LD2410_GATE_MAX;
  ld2410_status.motion.distance     = UINT16_MAX;
  ld2410_status.motion.active       = false;
  ld2410_status.motion.power        = 0;
  ld2410_status.motion.avg_count    = 0;
  ld2410_status.motion.sum_distance = 0;
  ld2410_status.motion.sum_power    = 0;
  for (index = 0; index < LD2410_GATE_MAX; index ++) ld2410_status.motion.arr_sensitivity[index] = UINT8_MAX;

  // init presence sensor
  ld2410_status.presence.max_gate     = LD2410_GATE_MAX;
  ld2410_status.presence.distance     = UINT16_MAX;
  ld2410_status.presence.active       = false;
  ld2410_status.presence.power        = 0;
  ld2410_status.presence.avg_count    = 0;
  ld2410_status.presence.sum_distance = 0;
  ld2410_status.presence.sum_power    = 0;
  for (index = 0; index < LD2410_GATE_MAX; index ++) ld2410_status.presence.arr_sensitivity[index] = UINT8_MAX;

  // load configuration
  LD2410LoadConfig ();

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ld2410_help to get help on %s commands"), D_LD2410_NAME);
}

void LD2410Every100ms ()
{
  int index;
  uint32_t time_now;

  // check validity
  if (!ld2410_status.enabled) return;
  if (TasmotaGlobal.uptime < LD2410_START_DELAY) return;

  // check if in command queue is not empty
  if (ld2410_command.str_queue.length () > 0)
  {
    switch (ld2410_command.step)
    {
      // send start command
      case LD2410_STEP_NONE:
        ld2410_command.step = LD2410_STEP_BEFORE_START;
        LD2410SendCommand (LD2410_CMND_START);
        break;

      // send command
      case LD2410_STEP_AFTER_START:
        ld2410_command.step = LD2410_STEP_BEFORE_COMMAND;
        LD2410SendCommand ((uint8_t)atoi (ld2410_command.str_queue.c_str ()));
        break;

      // send stop command
      case LD2410_STEP_AFTER_COMMAND:
        ld2410_command.step = LD2410_STEP_BEFORE_STOP;
        LD2410SendCommand (LD2410_CMND_STOP);
        break;

      // shift command queue to next one
      case LD2410_STEP_AFTER_STOP:
        ld2410_command.step = LD2410_STEP_NONE;
        index = ld2410_command.str_queue.indexOf (';');
        if (index != -1) ld2410_command.str_queue = ld2410_command.str_queue.substring (index + 1);
          else ld2410_command.str_queue = "";
    }
  }
}

void LD2410LogMessage (const uint8_t *parr_data, const size_t size_data, const bool send)
{
  uint8_t index;
  char    str_text[8];
  String  str_log;

  // check parameters
  if (parr_data == nullptr) return;

  // log type
  if (send) str_log = "send"; else str_log = "recv";

  // loop to generate string
  for (index = 0; index < size_data; index ++)
  {
    sprintf(str_text, " %02X", parr_data[index]);
    str_log += str_text;
  }

  // log message
  AddLog (LOG_LEVEL_DEBUG_MORE, PSTR ("HLK: %s"), str_log.c_str ());
}

// Handling of received data
void LD2410ReceiveData ()
{
  uint8_t  recv_data;
  uint32_t *plast;
    
  // check sensor presence
  if (ld2410_status.pserial == nullptr) return;

  // run serial receive loop
  while (ld2410_status.pserial->available ()) 
  {
    // receive character
    recv_data = (uint8_t)ld2410_status.pserial->read ();

    // update last received characters
    ld2410_received.arr_last[0] = ld2410_received.arr_last[1];
    ld2410_received.arr_last[1] = ld2410_received.arr_last[2];
    ld2410_received.arr_last[2] = ld2410_received.arr_last[3];
    ld2410_received.arr_last[3] = recv_data;

    // append character to received message body
    if (ld2410_received.idx_body < LD2410_MSG_SIZE_MAX) ld2410_received.arr_body[ld2410_received.idx_body++] = recv_data;

    // update reception timestamp
    ld2410_received.timestamp = millis ();

    // look for header of footer to detect message 
    plast = (uint32_t *)&ld2410_received.arr_last;
    switch (*plast)
    {
      // command and data header
      case 0xfafbfcfd:
      case 0xf1f2f3f4:
        // copy header
        memcpy (ld2410_received.arr_body, ld2410_received.arr_last, 4);
        ld2410_received.idx_body = 4;
        break;

      // command and data footer
      case 0x01020304:
      case 0xf5f6f7f8:
        // handle received message and reset message body
        LD2410HandleReceivedMessage ();
        ld2410_received.idx_body  = 0;
        break;
    }

    // give control back to system
    yield ();
  }
}

// Show JSON status (for MQTT)
//   "HLK-LD2410":{"detect"=1,"firmware"="04.02.26727272","timeout"=5,"motion":{"detect":1,"gate":8,"dist":126,"power":0},"presence":{"detect":1,"gate":8,"dist":120,"power":38}}
void LD2410ShowJSON (bool append)
{
  bool detected;

  // check sensor presence
  if (ld2410_status.pserial == nullptr) return;

  // send data in append mode
  if (append)
  {
    // start of ld2410 section
    detected = (ld2410_status.timestamp != 0);
    ResponseAppend_P (PSTR (","));
    ResponseAppend_P (PSTR ("\"ld2410\":{\"detect\":%u,"), detected);

    // firmware
    ResponseAppend_P (PSTR ("\"firm\":\"%02u.%02u.%u\""), ld2410_status.firmware.major, ld2410_status.firmware.minor, ld2410_status.firmware.revision);
    ResponseAppend_P (PSTR (",\"timeout\":%u"), ld2410_status.timeout);

    // motion
    ResponseAppend_P (PSTR (",\"move\":{"));
    ResponseAppend_P (PSTR ("\"mgate\":%u,"), ld2410_status.motion.max_gate);
    ResponseAppend_P (PSTR ("\"mdist\":%u,"), ld2410_status.motion.distance);
    ResponseAppend_P (PSTR ("\"mpower\":%u}"), ld2410_status.motion.power);

    // presence
    ResponseAppend_P (PSTR (",\"pres\":{"));
    ResponseAppend_P (PSTR ("\"pgate\":%u,"), ld2410_status.presence.max_gate);
    ResponseAppend_P (PSTR ("\"pdist\":%u,"), ld2410_status.presence.distance);
    ResponseAppend_P (PSTR ("\"ppower\":%u}"), ld2410_status.presence.power);

    // end of ld2410 section
    ResponseAppend_P (PSTR ("}"));
  }
}

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

// Append HLK-LD2410 sensor data to main page
void LD2410WebSensor ()
{
  uint8_t  index;
  uint16_t gate_index;
  char     str_background[8];
  char     str_color[8];
  char     str_value[8];

  // check if enabled
  if (!ld2410_status.enabled) return;

  WSContentSend_PD (PSTR ("<div style='font-size:10px;text-align:center;margin-top:4px;padding:2px 6px;background:#333333;border-radius:8px;'>\n"));

  // scale
  WSContentSend_PD (PSTR ("<div style='display:flex;padding:0px;'>\n"));
  WSContentSend_PD (PSTR ("<div style='width:28%%;padding:0px;text-align:left;font-size:12px;font-weight:bold;' title='firmware %u.%u.%u'>LD2410</div>\n"), ld2410_status.firmware.major, ld2410_status.firmware.minor, ld2410_status.firmware.revision);

  WSContentSend_PD (PSTR ("<div style='width:6%%;padding:0px;text-align:left;'>%um</div>\n"), 0);
  for (index = 1; index < 6; index ++) WSContentSend_PD (PSTR ("<div style='width:12%%;padding:0px;'>%um</div>\n"), index);
  WSContentSend_PD (PSTR ("<div style='width:6%%;padding:0px;text-align:right;'>%um</div>\n"), 6);
  WSContentSend_PD (PSTR ("</div>\n"));

  // presence sensor line
  gate_index = ld2410_status.presence.distance / 75;
  WSContentSend_P (PSTR ("<div style='display:flex;padding:1px 0px;color:grey;'>\n"));

  // presence title
  WSContentSend_P (PSTR ("<div style='width:28%%;padding:0px;text-align:left;"));
  if (ld2410_status.presence.active) WSContentSend_PD (PSTR ("color:%s;font-weight:bold;"), LD2410_COLOR_PRESENCE); else WSContentSend_P (PSTR ("color:white;"));
  WSContentSend_P (PSTR ("'>&nbsp;&nbsp;Presence</div>\n"));

  // loop thru gates
  for (index = 0; index < LD2410_GATE_MAX; index ++)
  {
    // cell : start
    WSContentSend_PD (PSTR ("<div style='width:8%%;padding:0px;"));

    // cell : first and last
    if (index == 0) WSContentSend_PD (PSTR ("border-top-left-radius:4px;border-bottom-left-radius:4px;"));
      else if (index == ld2410_status.presence.max_gate - 1) WSContentSend_PD (PSTR ("border-top-right-radius:4px;border-bottom-right-radius:4px;"));

    // cell : value
    if (index >= ld2410_status.presence.max_gate) strcpy (str_value, "&nbsp;");
      else if (gate_index == index) sprintf (str_value, "%u", ld2410_status.presence.power);
      else sprintf (str_value, "%u", ld2410_status.presence.arr_sensitivity[index]);

    // cell : value color
    if (gate_index == index) strcpy (str_color, "white");
      else strcpy (str_color, LD2410_COLOR_DISABLED);

    // cell : background
    if (index >= ld2410_status.presence.max_gate) strcpy (str_background, LD2410_COLOR_DISABLED);
      else if ((gate_index == index) && (ld2410_status.presence.power >= ld2410_status.presence.arr_sensitivity[index])) strcpy (str_background, LD2410_COLOR_PRESENCE);
      else strcpy (str_background, LD2410_COLOR_NONE);

    // display
    WSContentSend_PD (PSTR ("background:%s;color:%s;'>%s</div>\n"), str_background, str_color, str_value);
  }

  // end of presence
  WSContentSend_PD (PSTR ("</div>\n"));

  // motion sensor line
  gate_index = ld2410_status.motion.distance / 75;
  WSContentSend_PD (PSTR ("<div style='display:flex;padding:1px 0px;color:grey;'>\n"));

  // motion title
  WSContentSend_PD (PSTR ("<div style='width:28%%;padding:0px;text-align:left;"));
  if (ld2410_status.motion.active) WSContentSend_PD (PSTR ("color:%s;font-weight:bold;"), LD2410_COLOR_MOTION); else WSContentSend_PD (PSTR ("color:white;"));
  WSContentSend_PD (PSTR ("'>&nbsp;&nbsp;Motion</div>\n"));

  // loop thru gates
  for (index = 0; index < LD2410_GATE_MAX; index ++)
  {
    // cell : start
    WSContentSend_PD (PSTR ("<div style='width:8%%;padding:0px;"));

    // cell : first and last
    if (index == 0) WSContentSend_PD (PSTR ("border-top-left-radius:4px;border-bottom-left-radius:4px;"));
      else if (index == ld2410_status.motion.max_gate - 1) WSContentSend_PD (PSTR ("border-top-right-radius:4px;border-bottom-right-radius:4px;"));

    // cell : value
    if (index >= ld2410_status.motion.max_gate) strcpy (str_value, "&nbsp;");
      else if (gate_index == index) sprintf (str_value, "%u", ld2410_status.motion.power);
      else sprintf (str_value, "%u", ld2410_status.motion.arr_sensitivity[index]);

    // cell : value color
    if (gate_index == index) strcpy (str_color, "white");
      else strcpy (str_color, LD2410_COLOR_DISABLED);

    // cell : background
    if (index >= ld2410_status.motion.max_gate) strcpy (str_background, LD2410_COLOR_DISABLED);
      else if ((gate_index == index) && (ld2410_status.motion.power >= ld2410_status.motion.arr_sensitivity[index])) strcpy (str_background, LD2410_COLOR_MOTION);
      else strcpy (str_background, LD2410_COLOR_NONE);

    // display
    WSContentSend_PD (PSTR ("background:%s;color:%s;'>%s</div>\n"), str_background, str_color, str_value);
  }

  // end of motion
  WSContentSend_PD (PSTR ("</div>\n"));

  WSContentSend_PD (PSTR ("</div>\n"));
}

#ifdef USE_LD2410_RADAR

// Radar page
void LD2410GraphRadarUpdate ()
{
  long index, distance;
  long cpres, cmove;
  long x, y, r;
  long sin60 = 866;
  long cos60 = 500;

  // calculate environment
  if (ld2410_status.presence.max_gate > ld2410_status.motion.max_gate) { cpres = 1; cmove = 2; }
    else { cpres = 2; cmove = 1; }

  // start of update page
  WSContentBegin (200, CT_PLAIN);

  // presence detection zone
  distance = (max (ld2410_status.presence.max_gate, ld2410_status.motion.max_gate) - 1) * 75;
  x = distance * sin60 * 400 / 1000 / 600 - 1;
  y = distance * cos60 * 400 / 1000 / 600 - 1;
  r = distance * 400 / 600;
  WSContentSend_P (PSTR ("zone;M %d %d L %d %d A %d %d 0 0 1 %d %d Z\n"), 400, 50, 400 + x, 50 + y, r, r, 400 - x, 50 + y);

  // presence detection limit
  distance = (long)(ld2410_status.presence.max_gate - 1) * 75;
  x = distance * sin60 * 400 / 1000 / 600 - 1;
  y = distance * cos60 * 400 / 1000 / 600 - 1;
  r = distance * 400 / 600;
  WSContentSend_P (PSTR ("pres-max;M %d %d L %d %d A %d %d 0 0 1 %d %d Z\n"), 400, 50 + cpres * 2, 400 + x - cpres * 3, 50 + y + cpres, r - cpres * 2, r - cpres * 2, 400 - x + cpres * 3, 50 + y + cpres);

  // motion detection limit
  distance = (long)(ld2410_status.motion.max_gate - 1) * 75;
  x = distance * sin60 * 400 / 1000 / 600;
  y = distance * cos60 * 400 / 1000 / 600;
  r = distance * 400 / 600;
  WSContentSend_P (PSTR ("move-max;M %d %d L %d %d A %d %d 0 0 1 %d %d Z\n"), 400, 50 + cmove * 2, 400 + x - cmove * 3, 50 + y + cmove, r - cmove * 2, r - cmove * 2, 400 - x + cmove * 3, 50 + y + cmove);

  // presence detection
  x = y = r = 0;
  if (ld2410_status.presence.active)
  {
    if (ld2410_status.presence.distance < 10) distance = 10;
      else distance = ld2410_status.presence.distance;
    x = distance * sin60 * 400 / 1000 / 600;
    y = distance * cos60 * 400 / 1000 / 600;
    r = distance * 400 / 600;
  }
  WSContentSend_P (PSTR ("pres;M %d %d A %d %d 0 0 1 %d %d\n"), 400 + x, 50 + y, r, r, 400 - x, 50 + y);

  // motion detection
  x = y = r = 0;
  if (ld2410_status.motion.active)
  {
    if (ld2410_status.motion.distance < 10) distance = 10;
      else distance = ld2410_status.motion.distance;
    x = distance * sin60 * 400 / 1000 / 600;
    y = distance * cos60 * 400 / 1000 / 600;
    r = distance * 400 / 600;
  }
  WSContentSend_P (PSTR ("move;M %d %d A %d %d 0 0 1 %d %d\n"), 400 + x, 50 + y, r, r, 400 - x, 50 + y);

  // end of update page
  WSContentEnd ();
}

// Radar page
void LD2410GraphRadar ()
{
  long index, distance;
  long x, y, r;
  long cos[7] = {-1000, -866, -500, 0, 500, 866, 1000};
  long sin[7] = {0, 500, 866, 1000, 866, 500, 0};

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess ()) return;
  
  // set page label
  WSContentStart_P ("LD2410 Radar", true);
  WSContentSend_P (PSTR ("\n</script>\n"));

  // page data refresh script
  WSContentSend_P (PSTR ("<script type='text/javascript'>\n\n"));

  WSContentSend_P (PSTR ("function updateData(){\n"));

  WSContentSend_P (PSTR (" httpData=new XMLHttpRequest();\n"));
  WSContentSend_P (PSTR (" httpData.open('GET','/ld2410.upd',true);\n"));

  WSContentSend_P (PSTR (" httpData.onreadystatechange=function(){\n"));
  WSContentSend_P (PSTR ("  if (httpData.readyState===XMLHttpRequest.DONE){\n"));
  WSContentSend_P (PSTR ("   if (httpData.status===0 || (httpData.status>=200 && httpData.status<400)){\n"));
  WSContentSend_P (PSTR ("    arr_param=httpData.responseText.split('\\n');\n"));
  WSContentSend_P (PSTR ("    num_param=arr_param.length;\n"));
  WSContentSend_P (PSTR ("    for (i=0;i<num_param;i++){\n"));
  WSContentSend_P (PSTR ("     arr_value=arr_param[i].split(';');\n"));
  WSContentSend_P (PSTR ("     if (document.getElementById(arr_value[0])!=null) document.getElementById(arr_value[0]).setAttributeNS(null,'d',arr_value[1]);\n"));
  WSContentSend_P (PSTR ("    }\n"));
  WSContentSend_P (PSTR ("   }\n"));
  WSContentSend_P (PSTR ("   setTimeout(updateData,%u);\n"), 1000);               // ask for next update in 1 sec
  WSContentSend_P (PSTR ("  }\n"));
  WSContentSend_P (PSTR (" }\n"));

  WSContentSend_P (PSTR (" httpData.send();\n"));
  WSContentSend_P (PSTR ("}\n"));

  WSContentSend_P (PSTR ("setTimeout(updateData,%u);\n\n"), 100);                   // ask for first update after 100ms

  WSContentSend_P (PSTR ("</script>\n\n"));

  // set page as scalable
  WSContentSend_P (PSTR ("<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=yes'/>\n"));

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));

  WSContentSend_P (PSTR ("a {color:white;}\n"));
  WSContentSend_P (PSTR ("a:link {text-decoration:none;}\n"));

  WSContentSend_P (PSTR ("div {padding:0px;margin:0px;text-align:center;}\n"));
  WSContentSend_P (PSTR ("div.title {font-size:4vh;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.header {font-size:3vh;margin:1vh auto;}\n"));

  WSContentSend_P (PSTR ("div.graph {width:100%%;margin:2vh auto;}\n"));
  WSContentSend_P (PSTR ("svg.graph {width:100%%;height:80vh;}\n"));

  WSContentSend_P (PSTR ("</style>\n"));

  // page body
  WSContentSend_P (PSTR ("</head>\n"));
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  // room name
  WSContentSend_P (PSTR ("<div class='title'><a href='/'>%s</a></div>\n"), SettingsText(SET_DEVICENAME));

  // header
  WSContentSend_P (PSTR ("<div class='header'>Live Radar</div>\n"));

  // ------- Graph --------

  // start of radar
  WSContentSend_P (PSTR ("<div class='graph'>\n"));
  WSContentSend_P (PSTR ("<svg class='graph' viewBox='0 0 %u %u'>\n"), 800, 400 + 50 + 4);

  // style
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));
  WSContentSend_P (PSTR ("text {font-size:16px;fill:#aaa;text-anchor:middle;}\n"));
  WSContentSend_P (PSTR ("text.sensor {font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("text.pres {fill:#fa0;font-size:12px;}\n"));
  WSContentSend_P (PSTR ("text.move {fill:#f00;font-size:12px;}\n"));

  WSContentSend_P (PSTR ("path {stroke-opacity:0.5;fill:none;}\n"));
  WSContentSend_P (PSTR ("path.pres {stroke:#fa0;stroke-width:12;}\n"));
  WSContentSend_P (PSTR ("path.move {stroke:#f00;stroke-width:12;}\n"));
  WSContentSend_P (PSTR ("path.zone {stroke:none;fill:#1c1c1c;}\n"));
  WSContentSend_P (PSTR ("path.radar {stroke:#0a0;stroke-dasharray:1 1;}\n"));
  WSContentSend_P (PSTR ("path.pres-max {stroke:#fa0;stroke-dasharray:1 1;}\n"));
  WSContentSend_P (PSTR ("path.move-max {stroke:#f00;stroke-dasharray:1 1;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // legend
  WSContentSend_P (PSTR ("<path class='pres-max' d='M 170 15 L 230 15 L 230 35 L 170 35 Z'></path>\n"));
  WSContentSend_P (PSTR ("<text class='pres' x=200 y=30>Presence</text>\n"));
  WSContentSend_P (PSTR ("<path class='move-max' d='M 570 15 L 630 15 L 630 35 L 570 35 Z'></path>\n"));
  WSContentSend_P (PSTR ("<text class='move' x=600 y=30>Motion</text>\n"));

  // detection zone
  WSContentSend_P (PSTR ("<path class='zone' id='zone' d='' />\n"));

  // radar frame lines
  for (index = 1; index < 6; index++) WSContentSend_P (PSTR ("<path class='radar' d='M 400 50 L %d %d' />\n"), 400 + 400 * cos[index] / 1000, 50 + 400 * sin[index] / 1000);

  // radar frame circles and distance
  WSContentSend_P (PSTR ("<text x=400 y=30>LD2410</text>\n"));
  for (index = 1; index < 7; index++) 
  {
    x = index * sin[4] * 400 / 1000 / 6;
    y = index * cos[4] * 400 / 1000 / 6;
    r = index * 400 / 6;
    WSContentSend_P (PSTR ("<text x=%d y=%d>%dm</text>\n"), 400 + x, 40 + y, index);
    WSContentSend_P (PSTR ("<text x=%d y=%d>%dm</text>\n"), 400 - 5 - x, 40 + y, index);
    WSContentSend_P (PSTR ("<path class='radar'tasmota-flash --querytasmota-flash --query d='M %d %d A %d %d 0 0 1 %d %d' />\n"), 400 + x, 50 + y, r, r, 400 - x, 50 + y);
  }

  // presence detection limit
  WSContentSend_P (PSTR ("<path class='pres-max' id='pres-max' d='' />\n"));

  // motion detection limit
  WSContentSend_P (PSTR ("<path class='move-max' id='move-max' d='' />\n"));

  // presence detection
  WSContentSend_P (PSTR ("<path class='pres' id='pres' d='' />\n"));

  // motion detection
  WSContentSend_P (PSTR ("<path class='move' id='move' d='' />\n"));

  // end of radar
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentSend_P (PSTR ("</div>\n"));

  // end of page
  WSContentStop ();
}

#endif    // USE_LD2410_RADAR

#endif  // USE_WEBSERVER

/***************************************\
 *              Interface
\***************************************/

// LD2410 sensor
bool Xsns102 (uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
    case FUNC_INIT:
      LD2410Init ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kHLKLD2410Commands, HLKLD2410Command);
      break;
    case FUNC_SAVE_BEFORE_RESTART:
      LD2410SaveConfig ();
      break;
    case FUNC_EVERY_100_MSECOND:
      if (RtcTime.valid) LD2410Every100ms ();
      break;
    case FUNC_JSON_APPEND:
      if (ld2410_status.enabled) LD2410ShowJSON (true);
      break;
    case FUNC_LOOP:
      if (ld2410_status.enabled) LD2410ReceiveData ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      if (ld2410_status.enabled) LD2410WebSensor ();
      break;

#ifdef USE_LD2410_RADAR
    case FUNC_WEB_ADD_MAIN_BUTTON:
      if (ld2410_status.enabled) WSContentSend_P (PSTR ("<p><form action='ld2410' method='get'><button>LD2410 Radar</button></form></p>\n"));
      break;
    case FUNC_WEB_ADD_HANDLER:
      if (ld2410_status.enabled) Webserver->on ("/ld2410", LD2410GraphRadar);
      if (ld2410_status.enabled) Webserver->on ("/ld2410.upd", LD2410GraphRadarUpdate);
      break;
#endif    // USE_LD2410_RADAR

#endif  // USE_WEBSERVER
  }

  return result;
}

#endif     // USE_LD2410

