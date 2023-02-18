/*
  xsns_102_ld2410.ino - Driver for Presence and Movement sensor HLK-LD2410

  Copyright (C) 2022  Nicolas Bernaerts

  Connexions :
    * GPIO1 (Tx) should be declared as Serial Tx and connected to HLK-LD2410 Rx
    * GPIO3 (Rx) should be declared as Serial Rx and connected to HLK-LD2410 Tx

  Version history :
    28/06/2022 - v1.0 - Creation
    15/01/2023 - v2.0 - Complete rewrite

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
#define LD2410_DEFAULT_TIMEOUT          5        // inactivity after 5 sec

#define LD2410_GATE_MAX                 9        // number of sensor gates

#define LD2410_MSG_SIZE_MAX             64       // maximum message size

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
enum LD2410ListCommand { LD2410_CMND_START, LD2410_CMND_STOP, LD2410_CMND_READ_PARAM, LD2410_CMND_DIST_TIMEOUT, LD2410_CMND_SENSITIVITY, LD2410_CMND_READ_FIRMWARE, LD2410_CMND_SERIAL_RATE, LD2410_CMND_RESET, LD2410_CMND_RESTART, LD2410_CMND_CLOSE_ENGINEER, LD2410_CMND_OPEN_ENGINEER, LD2410_CMND_DATA, LD2410_CMND_MAX };       // list of available commands

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

// MQTT commands : ld_help and ld_send
const char kHLKLD2410Commands[]         PROGMEM = "ld2410_|help|param|eng|rate|reset|restart|timeout|firmware|gate|max";
void (* const HLKLD2410Command[])(void) PROGMEM = { &CmndLD2410Help, &CmndLD2410ReadParam, &CmndLD2410EngineeringMode, &CmndLD2410SerialRate, &CmndLD2410Reset, &CmndLD2410Restart, &CmndLD2410Timeout, &CmndLD2410Firmware, &CmndLD2410SetGateParam, &CmndLD2410SetGateMax };

/****************************************\
 *                 Data
\****************************************/

// HLK-LD2410 sensor command
static struct {
  uint8_t step = LD2410_STEP_NONE;
  uint8_t gate = 0;                                 // gate to handle
  String  str_queue;
} ld2410_command; 

// HLK-LD sensor general status
static struct {    
  uint32_t timestamp = UINT32_MAX;                  // timestamp of last received character
  uint8_t  idx_body  = 0;                           // index of received body
  uint8_t  arr_body[LD2410_MSG_SIZE_MAX];           // body of current received message
  uint8_t  arr_last[4] = {0, 0, 0, 0};              // last received characters
} ld2410_received; 

// HLK-LD sensor general status
struct ld2410_firmware
{
  uint8_t  major    = 0;                            // major version
  uint8_t  minor    = 0;                            // minor version
  uint32_t revision = 0;                            // revision
};
struct ld2410_sensor
{
  bool     detected;                                // activity detected
  uint8_t  max_gate;                                // detection max gate
  uint8_t  power;                                   // detection power
  uint16_t distance;                                // detection distance
  uint32_t timestamp;                               // timestamp of last detection
  uint8_t  arr_sensitivity[LD2410_GATE_MAX];        // gates sensitivity
};
static struct {
  TasmotaSerial  *pserial     = nullptr;            // pointer to serial port
  bool            enabled     = false;              // sensor enabled
  bool            detected    = false;              // sensor global detection state
  bool            engineering = false;              // data in engineering mode
  uint8_t         timeout     = LD2410_DEFAULT_TIMEOUT;          // timeout of detection
  uint32_t        serial_rate = UINT32_MAX;         // target serial rate
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
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ld2410_param       = read sensor parameters"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ld2410_firmware    = get sensor firmware"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ld2410_eng <0/1>   = set engineering mode"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ld2410_rate <rate> = set serial rate"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ld2410_reset       = reset sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ld2410_restart     = restart sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ld2410_gate <gate,pres,motion> = set gate sensitivity"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ld2410_max <pres,motion>       = set detection max gate "));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ld2410_timeout <value>         = set timeout"));
 
  ResponseCmndDone ();
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

void CmndLD2410Timeout ()
{
  if (XdrvMailbox.payload > 0) ld2410_status.timeout = XdrvMailbox.payload;
  LD2410AppendCommand (LD2410_CMND_DIST_TIMEOUT);
  ResponseCmndNumber (ld2410_status.timeout);
}

void CmndLD2410SerialRate ()
{
  bool valid = false;

  valid |= (XdrvMailbox.payload == 9600);
  valid |= (XdrvMailbox.payload == 19200);
  valid |= (XdrvMailbox.payload == 38400);
  valid |= (XdrvMailbox.payload == 57600);
  valid |= (XdrvMailbox.payload == 115200);
  valid |= (XdrvMailbox.payload == 256000);

  if (valid)
  {
    Settings->baudrate = XdrvMailbox.payload / 300;
    LD2410AppendCommand (LD2410_CMND_SERIAL_RATE);
  }

  ResponseCmndNumber (Settings->baudrate * 300);
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
    ResponseCmndDone ();
  }

  // else if gate is provided, send param
  else if (gate < LD2410_GATE_MAX)
  {
    sprintf (str_data, "%u : %u & %u", gate, ld2410_status.presence.arr_sensitivity[gate], ld2410_status.motion.arr_sensitivity[gate]);
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
  result = ((max_presence <= LD2410_GATE_MAX) && (max_motion <= LD2410_GATE_MAX));
  if (result)
  {
    ld2410_status.presence.max_gate = max_presence;
    ld2410_status.motion.max_gate   = max_motion;
    LD2410AppendCommand (LD2410_CMND_DIST_TIMEOUT);
    LD2410AppendCommand (LD2410_CMND_READ_PARAM);
    ResponseCmndDone ();
  }

  // if no command, display status
  else
  {
    sprintf (str_data, "max gate : %u & %u", ld2410_status.presence.max_gate, ld2410_status.motion.max_gate);
    ResponseCmndChar (str_data);
  }
}

/**************************************************\
 *                  Functions
\**************************************************/

bool LD2410GetMotionDetectionStatus (uint32_t timeout)
{
  // if timestamp not defined, no detection
  if (ld2410_status.motion.timestamp == 0) return false;
  
  // if no timeout given, use default one
  if ((timeout == 0) || (timeout == UINT32_MAX)) timeout = ld2410_status.timeout;

  // return timeout status
  return (LocalTime () - ld2410_status.motion.timestamp <= timeout);
}

bool LD2410GetPresenceDetectionStatus (uint32_t timeout)
{
  // if timestamp not defined, no detection
  if (ld2410_status.presence.timestamp == 0) return false;
  
  // if no timeout given, use default one
  if ((timeout == 0) || (timeout == UINT32_MAX)) timeout = ld2410_status.timeout;

  // return timeout status
  return (LocalTime () - ld2410_status.presence.timestamp <= timeout);
}

bool LD2410GetGlobalDetectionStatus (uint32_t timeout)
{
   return (LD2410GetMotionDetectionStatus (timeout) || LD2410GetPresenceDetectionStatus (timeout));
}

// driver initialisation
bool LD2410InitDevice (uint32_t timeout)
{
  uint32_t serial_rate;

  // set timeout
  if ((timeout == 0) || (timeout == UINT32_MAX)) ld2410_status.timeout = LD2410_DEFAULT_TIMEOUT;
    else ld2410_status.timeout = timeout;

  // if not done, init sensor state
  if (ld2410_status.pserial == nullptr)
  {
    // calculate serial rate
    if (Settings->baudrate > 384) serial_rate = 256000;
    else serial_rate = (uint32_t)Settings->baudrate * 300;

#ifdef ESP32
    // create serial port
    ld2410_status.pserial = new TasmotaSerial (Pin (GPIO_RXD), Pin (GPIO_TXD), 2);

    // initialise serial port
    ld2410_status.enabled = ld2410_status.pserial->begin (serial_rate, SERIAL_8N1);

#else       // ESP8266
    // create serial port
    ld2410_status.pserial = new TasmotaSerial (Pin (GPIO_RXD), Pin (GPIO_TXD), 2);

    // initialise serial port
    ld2410_status.enabled = ld2410_status.pserial->begin (serial_rate, SERIAL_8N1);

    // force hardware configuration on ESP8266
    if (ld2410_status.enabled && ld2410_status.pserial->hardwareSerial ()) ClaimSerial ();
#endif      // ESP32 & ESP8266

    // log
    if (ld2410_status.enabled) AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s sensor init at %u"), D_LD2410_NAME, serial_rate);
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
      arr_buffer[18] = ld2410_status.timeout;
      AddLog (LOG_LEVEL_INFO, PSTR ("HLK: cmnd set max gate pres %u, motion %u & timeout %u"), ld2410_status.presence.max_gate, ld2410_status.motion.max_gate, ld2410_status.timeout);
      break;

    case LD2410_CMND_SENSITIVITY:
      size_buffer = sizeof (ld2410_cmnd_sensitivity);
      memcpy_P (arr_buffer, ld2410_cmnd_sensitivity, size_buffer);
      arr_buffer[6] = ld2410_command.gate;
      arr_buffer[12] = ld2410_status.motion.arr_sensitivity[ld2410_command.gate];
      arr_buffer[18] = ld2410_status.presence.arr_sensitivity[ld2410_command.gate];
      AddLog (LOG_LEVEL_INFO, PSTR ("HLK: cmnd set gate %u : pres %u / motion %u"), ld2410_command.gate, ld2410_status.presence.arr_sensitivity[ld2410_command.gate], ld2410_status.motion.arr_sensitivity[ld2410_command.gate]);
      break;

    case LD2410_CMND_READ_FIRMWARE:
      size_buffer = sizeof (ld2410_cmnd_read_firmware);
      memcpy_P (arr_buffer, ld2410_cmnd_read_firmware, size_buffer);
      AddLog (LOG_LEVEL_INFO, PSTR ("HLK: cmnd read firmware"));
      break;

    case LD2410_CMND_SERIAL_RATE:
      size_buffer = sizeof (ld2410_cmnd_serial_rate);
      memcpy_P (arr_buffer, ld2410_cmnd_serial_rate, size_buffer);
      if (Settings->baudrate == 32) value = 0x0001;                     // 9600
      else if (Settings->baudrate == 64) value = 0x0002;                // 19200
      else if (Settings->baudrate == 128) value = 0x0003;               // 38400
      else if (Settings->baudrate == 192) value = 0x0004;               // 57600
      else if (Settings->baudrate == 384) value = 0x0005;               // 115200
      else value = 0x0007;                                              // 256000
      arr_buffer[4] = value;
      AddLog (LOG_LEVEL_INFO, PSTR ("HLK: cmnd serial rate %u"), value);
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
  bool     detected;
  uint8_t  index;
  uint16_t *pcommand;
  uint16_t *pvalue;
  uint32_t *pheader;
  uint32_t *prevision;

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

      // if in debug mode, log data to console
      LD2410LogMessage (ld2410_received.arr_body, ld2410_received.idx_body, false);

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
      }
      break;

    // ---------------------
    //    data reception
    // ---------------------
    case 0xf1f2f3f4:

      // get data mode
      ld2410_status.engineering = (ld2410_received.arr_body[6] == 0x01);

      // read motion and presence detection
      ld2410_status.motion.detected   = (ld2410_received.arr_body[8] && 0x01);
      ld2410_status.presence.detected = (ld2410_received.arr_body[8] && 0x02);

      // read motion and presence power
      ld2410_status.motion.power   = ld2410_received.arr_body[11];
      ld2410_status.presence.power = ld2410_received.arr_body[14];

      // read data in engineering mode
      if (ld2410_status.engineering)
      {
        // retrieve motion distance
        pvalue = (uint16_t*)(ld2410_received.arr_body + 9);
        ld2410_status.motion.distance   = *pvalue;
        ld2410_status.presence.distance = *pvalue;
      }

      // read data in normal mode
      else
      {
        // retrieve motion distance
        pvalue = (uint16_t*)(ld2410_received.arr_body + 9);
        ld2410_status.motion.distance = *pvalue;

        // retrieve static distance
        pvalue = (uint16_t*)(ld2410_received.arr_body + 12);
        ld2410_status.presence.distance = *pvalue;
      }

      // adjust detection according to power
      if (ld2410_status.motion.power == 0) ld2410_status.motion.detected = false;
      if (ld2410_status.presence.power == 0) ld2410_status.presence.detected = false;

      // update timestamp
      if (ld2410_status.motion.detected) ld2410_status.motion.timestamp = LocalTime ();
      if (ld2410_status.presence.detected) ld2410_status.presence.timestamp = LocalTime ();

      // check for MQTT update
      detected = (ld2410_status.motion.detected || ld2410_status.presence.detected);
      if (ld2410_status.detected != detected)
      {
        ld2410_status.detected = detected;
        LD2410ShowJSON (false);
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

  // init gates power
  for (index = 0; index < LD2410_GATE_MAX; index ++)
  {
    ld2410_status.motion.arr_sensitivity[index]   = UINT8_MAX;
    ld2410_status.presence.arr_sensitivity[index] = UINT8_MAX;
  }

  // init sensors
  ld2410_status.motion.detected    = false;
  ld2410_status.presence.detected  = false;
  ld2410_status.motion.max_gate    = LD2410_GATE_MAX;
  ld2410_status.presence.max_gate  = LD2410_GATE_MAX;
  ld2410_status.motion.distance    = UINT16_MAX;
  ld2410_status.presence.distance  = UINT16_MAX;
  ld2410_status.motion.power       = UINT8_MAX;
  ld2410_status.presence.power     = UINT8_MAX;
  ld2410_status.motion.timestamp   = 0;
  ld2410_status.presence.timestamp = 0;

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ld2410_help to get help on %s commands"), D_LD2410_NAME);
}

void LD2410Every250ms ()
{
  int index;

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

void LD2410EverySecond ()
{
  bool     detected;
  uint32_t time_now = LocalTime ();

  // if presence timeout is reached, reset presence detection
  ld2410_status.presence.detected = (time_now <= ld2410_status.presence.timestamp + ld2410_status.timeout);

  // if motion timeout is reached, reset motion detection
  ld2410_status.motion.detected = (time_now <= ld2410_status.motion.timestamp + ld2410_status.timeout);

  // check for MQTT update
  detected = (ld2410_status.motion.detected || ld2410_status.presence.detected);
  if (ld2410_status.detected != detected)
  {
    ld2410_status.detected = detected;
    LD2410ShowJSON (false);
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
//   false : "HLK-LD2410":{"detect"=1}
//   true  : "HLK-LD2410":{"detect"=1,"firmware"="04.02.26727272","timeout"=5,"motion":{"detect":1,"gate":8,"dist":126,"power":0},"presence":{"detect":1,"gate":8,"dist":120,"power":38}}
void LD2410ShowJSON (bool append)
{
  // check sensor presence
  if (ld2410_status.pserial == nullptr) return;

  // add , in append mode or { in direct publish mode
  if (append) ResponseAppend_P (PSTR (",")); else Response_P (PSTR ("{"));

  // start of ld2410 section
  ResponseAppend_P (PSTR ("\"%s\":{\"detect\":%u"), D_LD2410_NAME, ld2410_status.detected);

  if (append)
  {
    // firmware
    ResponseAppend_P (PSTR ("\"firmware\":\"%02u.%02u.%u\""), ld2410_status.firmware.major, ld2410_status.firmware.minor, ld2410_status.firmware.revision);
    ResponseAppend_P (PSTR (",\"timeout\":%u"), ld2410_status.timeout);

    // motion
    ResponseAppend_P (PSTR (",\"motion\":{"));
    ResponseAppend_P (PSTR ("\"mdetect\":%u,"), ld2410_status.motion.detected);
    ResponseAppend_P (PSTR ("\"mgate\":%u,"), ld2410_status.motion.max_gate);
    ResponseAppend_P (PSTR ("\"mdist\":%u,"), ld2410_status.motion.distance);
    ResponseAppend_P (PSTR ("\"mpower\":%u}"), ld2410_status.motion.power);

    // presence
    ResponseAppend_P (PSTR (",\"presence\":{"));
    ResponseAppend_P (PSTR ("\"pdetect\":%u,"), ld2410_status.presence.detected);
    ResponseAppend_P (PSTR ("\"pgate\":%u,"), ld2410_status.presence.max_gate);
    ResponseAppend_P (PSTR ("\"pdist\":%u,"), ld2410_status.presence.distance);
    ResponseAppend_P (PSTR ("\"ppower\":%u}"), ld2410_status.presence.power);
  }

  // end of ld2410 section
  ResponseAppend_P (PSTR ("}"));

  // publish it if not in append mode
  if (!append)
  {
    ResponseAppend_P (PSTR ("}"));
    MqttPublishTeleSensor ();
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

  WSContentSend_PD (PSTR ("<div style='font-size:10px;text-align:center;margin-top:4px;padding:2px 6px;background:#333333;border-radius:8px;'>\n"));

  // scale
  WSContentSend_PD (PSTR ("<div style='display:flex;padding:0px;'>\n"));
  WSContentSend_PD (PSTR ("<div style='width:28%%;padding:0px;text-align:left;font-size:12px;font-weight:bold;' title='firmware %u.%u.%u'>%s</div>\n"), ld2410_status.firmware.major, ld2410_status.firmware.minor, ld2410_status.firmware.revision, "LD2410");

  WSContentSend_PD (PSTR ("<div style='width:6%%;padding:0px;text-align:left;'>%um</div>\n"), 0);
  for (index = 1; index < 6; index ++) WSContentSend_PD (PSTR ("<div style='width:12%%;padding:0px;'>%um</div>\n"), index);
  WSContentSend_PD (PSTR ("<div style='width:6%%;padding:0px;text-align:right;'>%um</div>\n"), 6);
  WSContentSend_PD (PSTR ("</div>\n"));

  // loop to display presence sensor
  gate_index = ld2410_status.presence.distance / 75;
  WSContentSend_PD (PSTR ("<div style='display:flex;padding:1px 0px;color:grey;'>\n"));
  WSContentSend_PD (PSTR ("<div style='width:28%%;padding:0px;text-align:left;color:white;'>&nbsp;&nbsp;Presence</div>\n"));
  for (index = 0; index < LD2410_GATE_MAX; index ++)
  {
    // cell : start
    WSContentSend_PD (PSTR ("<div style='width:8%%;padding:0px;"));

    // cell : first and last
    if (index == 0) WSContentSend_PD (PSTR ("border-top-left-radius:4px;border-bottom-left-radius:4px;"));
    else if (index == ld2410_status.presence.max_gate - 1) WSContentSend_PD (PSTR ("border-top-right-radius:4px;border-bottom-right-radius:4px;"));

    // cell : value and end
    if (ld2410_status.presence.detected && (gate_index == index)) WSContentSend_PD (PSTR ("background:%s;color:white;'>%u</div>\n"), LD2410_COLOR_PRESENCE, ld2410_status.presence.power);
      else if (index >= ld2410_status.presence.max_gate) WSContentSend_PD (PSTR ("background:%s;'>&nbsp;</div>\n"), LD2410_COLOR_DISABLED);
      else if (ld2410_status.presence.arr_sensitivity[index] == UINT8_MAX) WSContentSend_PD (PSTR ("background:%s;'>&nbsp;</div>\n"), LD2410_COLOR_NONE);
      else WSContentSend_PD (PSTR ("background:%s;'>%u</div>\n"), LD2410_COLOR_NONE, ld2410_status.presence.arr_sensitivity[index]);
  }
  WSContentSend_PD (PSTR ("</div>\n"));

  // loop to display motion sensor
  gate_index = ld2410_status.motion.distance / 75;
  WSContentSend_PD (PSTR ("<div style='display:flex;padding:1px 0px;color:grey;'>\n"));
  WSContentSend_PD (PSTR ("<div style='width:28%%;padding:0px;text-align:left;color:white;'>&nbsp;&nbsp;Motion</div>\n"));
  for (index = 0; index < LD2410_GATE_MAX; index ++)
  {
    // cell : start
    WSContentSend_PD (PSTR ("<div style='width:8%%;padding:0px;"));

    // cell : first and last
    if (index == 0) WSContentSend_PD (PSTR ("border-top-left-radius:4px;border-bottom-left-radius:4px;"));
    else if (index == ld2410_status.motion.max_gate - 1) WSContentSend_PD (PSTR ("border-top-right-radius:4px;border-bottom-right-radius:4px;"));

    // cell : value and end
    if (ld2410_status.motion.detected && (gate_index == index)) WSContentSend_PD (PSTR ("background:%s;color:white;'>%u</div>\n"), LD2410_COLOR_MOTION, ld2410_status.motion.power);
      else if (index >= ld2410_status.motion.max_gate) WSContentSend_PD (PSTR ("background:%s;'>&nbsp;</div>\n"), LD2410_COLOR_DISABLED);
      else if (ld2410_status.motion.arr_sensitivity[index] == UINT8_MAX) WSContentSend_PD (PSTR ("background:%s;'>&nbsp;</div>\n"), LD2410_COLOR_NONE);
      else WSContentSend_PD (PSTR ("background:%s;'>%u</div>\n"), LD2410_COLOR_NONE, ld2410_status.motion.arr_sensitivity[index]);
  }
  WSContentSend_PD (PSTR ("</div>\n"));

  WSContentSend_PD (PSTR ("</div>\n"));
}

#endif  // USE_WEBSERVER

/***************************************\
 *              Interface
\***************************************/

// Teleinfo sensor
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
    case FUNC_EVERY_100_MSECOND:
      if (ld2410_status.enabled && (TasmotaGlobal.uptime > LD2410_START_DELAY)) LD2410Every250ms ();
      break;
    case FUNC_EVERY_SECOND:
      LD2410EverySecond ();
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
#endif  // USE_WEBSERVER
  }

  return result;
}

#endif     // USE_LD2410

