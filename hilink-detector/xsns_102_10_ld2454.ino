/*
  xsns_102_10_ld2454.ino - Driver for Movement sensor HLK-LD2454

  Copyright (C) 2023-2026  Nicolas Bernaerts

  Version history :
    12/09/2026 - v1.0 - Creation

  Connexions :
    * GPIO1 should be declared as LD2410 Tx and connected to HLK-LD2454 Rx
    * GPIO3 should be declared as LD2410 Rx and connected to HLK-LD2454 Tx

  No settings stored. Everything is read from device.

  Detection sensor zones :

            x1,y1 ..........x2,y1
              .               .
              .    detection  .
              .      zone     .
              .               .
            x1,y2 ..........x2,y2
  
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

#ifdef USE_HILINK_DETECTOR
#ifdef USE_HILINK_LD2454

#include <TasmotaSerial.h>

/*************************************************\
 *               Variables
\*************************************************/

// constant
#define LD2454_START_DELAY              5        // sensor startup delay

#define LD2454_DISTANCE_MIN             0        // minimum detection distance (cm)
#define LD2454_DISTANCE_MAX             600      // default minimum detection distance (cm)

#define LD2454_DATA_RATE                256000

#define LD2454_GATE_QUANTITY            1
#define LD2454_GATE_WIDTH               600

#define LD2454_MAX_TARGET               3

// ----------------
// binary commands
// ----------------

// command handling steps
enum LD2454StepCommand { LD2454_STEP_DATA, LD2454_STEP_COMMAND_SENT, LD2454_STEP_COMMAND_RECV, LD2454_STEP_MAX };

// list of commands available
enum LD2454ListCommand { LD2454_CMND_NONE, LD2454_CMND_START, LD2454_CMND_STOP, LD2454_CMND_READ_FIRMWARE, LD2454_CMND_RESET, LD2454_CMND_RESTART, LD2454_CMND_TARGET_SINGLE, LD2454_CMND_TARGET_MULTI, LD2454_CMND_MAX };

// target
enum LD2454Target                  { LD2454_TARGET_SINGLE, LD2454_TARGET_MULTI };
const char kLD2454Target[] PROGMEM =       "single"     "|"       "multi"       ;

// ----------------
// binary commands
// ----------------

uint8_t LD2454_cmnd_reset[]         PROGMEM = { 0xa2, 0x00 };
uint8_t LD2454_cmnd_restart[]       PROGMEM = { 0xa3, 0x00 };

uint8_t LD2454_cmnd_read_firmware[] PROGMEM = { 0xa0, 0x00 };

uint8_t LD2454_cmnd_target_single[] PROGMEM = { 0x80, 0x00 };
uint8_t LD2454_cmnd_target_multi[]  PROGMEM = { 0x90, 0x00 };

// MQTT commands
const char kHLKLD2454Commands[]         PROGMEM = "hlk" "|" "_reset" "|"    "_restart"   "|"      "_mode"         ;
void (* const HLKLD2454Command[])(void) PROGMEM = {   &CmndLD2454Reset, &CmndLD2454Restart, &CmndLD2454TargetMode };

/**************************************************\
 *                  Commands
\**************************************************/

// sensor detection zone reset
void CmndLD2454Reset ()
{
  HilinkAppendCommand (LD2454_CMND_RESET);
  ResponseCmndDone ();
}

void CmndLD2454Restart ()
{
  HilinkAppendCommand (LD2454_CMND_RESTART);
  ResponseCmndDone ();
}

void CmndLD2454TargetMode ()
{
  int  device = -2;
  char str_answer[8];

  // handle parameter
  if (XdrvMailbox.data_len > 0)
  {
    device = GetCommandCode (str_answer, sizeof (str_answer), XdrvMailbox.data, kLD2454Target);
    if (device == LD2454_TARGET_SINGLE) HilinkAppendCommand (LD2454_CMND_TARGET_SINGLE);
      else if (device == LD2454_TARGET_MULTI) HilinkAppendCommand (LD2454_CMND_TARGET_MULTI);
  }

  if (device == -2) ResponseCmndChar (GetTextIndexed (str_answer, sizeof (str_answer), hilink_config.device, kLD2454Target));
    else if (device == -1) ResponseCmndFailed ();
    else ResponseCmndDone ();
}

/******************************\
 *       Common commands
\******************************/

void LD2454DeviceCommand (const uint8_t command, const uint8_t context)
{
  uint8_t index;

switch (command)
  {
    // display device help
    case HILINK_CMND_HELP:
      AddLog (LOG_LEVEL_INFO, PSTR ("%s specific commands :"), hilink_status.str_model);
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_reset       = reset detector (will restart)"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_restart     = restart detector"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_mode <mode> = target mode"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    single : single target mode"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    multi  : multi target mode"));
      break;

    // init device parameters
    case HILINK_CMND_INIT:
      // declare device
      hilink_status.baudrate   = LD2454_DATA_RATE;
      hilink_status.gate_qty   = LD2454_GATE_QUANTITY;
      hilink_status.gate_width = LD2454_GATE_WIDTH;
      hilink_status.dist_limit = LD2454_DISTANCE_MAX;
      hilink_config.dist_min   = LD2454_DISTANCE_MIN;

      // set presence and motion detection status
      hilink_static.enabled = false;
      hilink_motion.enabled = true;

      // set radar and zones
      hilink_status.radar2D = true;
      hilink_zone.max       = 0;
      break;

    // device environment initialisation
    case HILINK_CMND_FIRST:
      // default config at startup
      hilink_reception.data   = true;
      hilink_reception.energy = false;
      hilink_command.mode     = false;

      // set initial delay
      HilinkSetCommandDelay (LD2454_START_DELAY);

      // get device general config
      HilinkAppendCommand (LD2454_CMND_READ_FIRMWARE);

      // set tracking mode
      if (hilink_config.param.multi == 0) HilinkAppendCommand (LD2454_CMND_TARGET_SINGLE);
        else HilinkAppendCommand (LD2454_CMND_TARGET_MULTI);
      break;

    // read device data information
    case HILINK_CMND_INFO:
      HilinkAppendCommand (LD2454_CMND_READ_FIRMWARE);
      break;
      
    // read device parameters
    case HILINK_CMND_PARAM:
      break;
      
    // set presence-less delay
    case HILINK_CMND_DELAY:
      break;
      
    // set minimum detection distance
    case HILINK_CMND_DMIN:
      break;

    // set maximum detection distance
    case HILINK_CMND_DMAX:
      break;
    
    // append JSON according to context
    case HILINK_CMND_JSON:
      switch (context)
      {
        case HILINK_JSON_GENERAL:
          ResponseAppend_P (PSTR (",\"Dist\":%u"), HilinkGetDistance (HILINK_JSON_GENERAL));
          break;

        case HILINK_JSON_MOTION:
          ResponseAppend_P (PSTR (",\"Dist\":%u"), HilinkGetDistance (HILINK_JSON_MOTION));
          for (index = 0; index < LD2454_MAX_TARGET; index ++) ResponseAppend_P (PSTR (",\"T%u\":{\"X\":%d,\"Y\":%d,\"Dist\":%u,\"Speed\":%d}"), index + 1, hilink_motion.arr_target[index].x, hilink_motion.arr_target[index].y, hilink_motion.arr_target[index].dist, hilink_motion.arr_target[index].speed);
          break;
      } 
      break;
  }
}

/**************************************************\
 *                  Functions
\**************************************************/

// convert LD2454 coordinate format to MSB and LSB
void LD2454ConvertCoordinate2LsbMsb (const int16_t coordinate, uint8_t &lsb, uint8_t &msb)
{
  int32_t value;

  // convert according to positive or negative value
  if (coordinate >= 0) value = 32768 + (int32_t)coordinate;
    else value = - (int32_t)coordinate;

  msb = (uint8_t)(value / 256);
  lsb = (uint8_t)(value % 256);
}

// convert data LSB and MSB according to LD2454 signed value format
int16_t LD2454ConvertLsbMsb2Value (const uint8_t lsb, const uint8_t msb)
{
  int32_t value, coordinate;

  // calculate value from MSB and LSB
  value  = 256 * (int32_t)msb + (int32_t)lsb;

  // convert according to higher bit
  if (value >= 32768) coordinate = value - 32768;
    else coordinate = - value;

  return (int16_t)coordinate;
}

/*********************************************\
 *              Communication
\*********************************************/

// send command
void LD2454SendCommand (const uint8_t command)
{
  uint8_t  lsb, msb;
  size_t   index, start; 
  size_t   body;
  uint8_t  arr_buffer[32];

  // check sensor presence
  if (hilink_status.pserial == nullptr) return;

  // set command flag
  hilink_reception.data = false;

  // handle command
  body = 0;
  switch (command)
  {
    case LD2454_CMND_START:
      body = sizeof (hilink_cmnd_start);
      memcpy_P (arr_buffer, hilink_cmnd_start, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s command ON"), hilink_status.str_model);
      break;

    case LD2454_CMND_STOP:
      body = sizeof (hilink_cmnd_stop);
      memcpy_P (arr_buffer, hilink_cmnd_stop, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s command OFF"), hilink_status.str_model);
      break;

    case LD2454_CMND_READ_FIRMWARE:
      body = sizeof (LD2454_cmnd_read_firmware);
      memcpy_P (arr_buffer, LD2454_cmnd_read_firmware, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s read firmware"), hilink_status.str_model);
      break;

    case LD2454_CMND_RESET:
      body = sizeof (LD2454_cmnd_reset);
      memcpy_P (arr_buffer, LD2454_cmnd_reset, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s reset"), hilink_status.str_model);
      break;

    case LD2454_CMND_RESTART:
      body = sizeof (LD2454_cmnd_restart);
      memcpy_P (arr_buffer, LD2454_cmnd_restart, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s restart"), hilink_status.str_model);

      // prepare init commands after restart
      HilinkDeviceCommand (HILINK_CMND_FIRST);
      break;

    case LD2454_CMND_TARGET_SINGLE:
      body = sizeof (LD2454_cmnd_target_single);
      memcpy_P (arr_buffer, LD2454_cmnd_target_single, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s single target mode"), hilink_status.str_model);
      break;

    case LD2454_CMND_TARGET_MULTI:
      body = sizeof (LD2454_cmnd_target_multi);
      memcpy_P (arr_buffer, LD2454_cmnd_target_multi, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s multi target mode"), hilink_status.str_model);
      break;
  }

  // if command defined, send it
  if (body > 0)
  {
    HilinkCommandStarted (command, 0);
    HilinkSendCommand (arr_buffer, body);
  }
}

// Handling of received data
void LD2454HandleReceivedCommand ()
{
  uint8_t  index, shift;
  uint16_t command, value;
  char     str_text[16];

  // log data to console
  HilinkLogHexa (hilink_reception.arr_body, hilink_reception.idx_body, true, false);

  // if enough data received
  if (hilink_reception.idx_body >= 8)
  {
    // handle according to command index
    //  0  1  2  3  4  5  6  7 ...
    // FD FC FB FA yy 00 xx xx ... 04 03 02 01
    command = 0x100 * (uint16_t)hilink_reception.arr_body[6] + (uint16_t)hilink_reception.arr_body[7]; 
    switch (command)
    {
      case 0xff01:    // start command mode
        hilink_command.mode = true;
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s command ON"), hilink_status.str_model);
        break;
      
      case 0xfe01:    // stop command mode
        hilink_command.mode = false;
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s command OFF"), hilink_status.str_model);
        break;

      case 0x8001:    // set single target tracking
        hilink_config.param.multi = 0;
        hilink_status.char_green  = 'S';
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s set to Single target mode"), hilink_status.str_model);
        break;
      
      case 0x9001:    // set multi target tracking
        hilink_config.param.multi = 1;
        hilink_status.char_green  = 'M';
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s set to Multi target mode"), hilink_status.str_model);
        break;
           
      case 0xa001:    // read firmware
        sprintf_P (str_text, PSTR ("%u.%u.%u"), hilink_reception.arr_body[14], hilink_reception.arr_body[16], hilink_reception.arr_body[18]);
        hilink_status.str_firmware = str_text;
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s firmware %s"), hilink_status.str_model, str_text);
        break;

      case 0xa201:  // reset device
        HilinkAppendCommand (LD2454_CMND_RESTART);
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s has been reset"), hilink_status.str_model);
        break;

      case 0xa301:  // restart device
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s will restart"), hilink_status.str_model);
        break;
    }
  }

  // declare command as finished
  HilinkCommandFinished ();
}

// Handling of received data
void LD2454HandleReceivedData ()
{
  bool     active;
  size_t   index, start;
  uint16_t dist;
  int16_t  speed;
  int32_t  x, y;

  // log received message
  HilinkLogHexa (hilink_reception.arr_body, hilink_reception.idx_body, false, false);

  // loop thru targets
  for (index = 0; index < LD2454_MAX_TARGET; index ++)
  {
    // set target coordonnates
    start = 4 + index * 8;

    // read x, y
    x     = (int32_t)LD2454ConvertLsbMsb2Value (hilink_reception.arr_body[start++], hilink_reception.arr_body[start++]) / 10;
    y     = (int32_t)LD2454ConvertLsbMsb2Value (hilink_reception.arr_body[start++], hilink_reception.arr_body[start++]) / 10;
    speed = LD2454ConvertLsbMsb2Value (hilink_reception.arr_body[start++], hilink_reception.arr_body[start++]);
    dist  = (uint16_t)sqrt ((float)(x * x + y * y));
    hilink_motion.arr_target[index].active = (dist > 0);

    // set data
    if (hilink_motion.arr_target[index].active)
    {
      hilink_motion.arr_target[index].x     = (int16_t)x;
      hilink_motion.arr_target[index].y     = (int16_t)y;
      hilink_motion.arr_target[index].speed = speed;
      hilink_motion.arr_target[index].dist  = dist;
    }
    else
    {
      hilink_motion.arr_target[index].x     = 0;
      hilink_motion.arr_target[index].y     = 0;
      hilink_motion.arr_target[index].speed = 0;
      hilink_motion.arr_target[index].dist  = 0;
    }
  }

  // reset reception buffer
  HilinkReceptionEmpty ();
}

/*********************************************\
 *                   Callback
\*********************************************/

// handle command queue
void LD2454ProcessCommandQueue ()
{
  bool cmnd_mode, cmnd_waiting;

  // check validity
  if (hilink_config.device != HLK_DEVICE_LD2454) return;
  if (hilink_status.pserial == nullptr) return;
  if (!HilinkCommandPossible ()) return;

  // check if commands are waiting
  cmnd_mode    = HilinkIsInCommandMode ();
  cmnd_waiting = HilinkCommandWaiting ();

  // commands in the pipe and command mode not started, start command mode
  if (cmnd_waiting && !cmnd_mode) LD2454SendCommand (LD2454_CMND_START);
    
  // else if no command in the queue, stop command mode
  else if (!cmnd_waiting && cmnd_mode) LD2454SendCommand (LD2454_CMND_STOP);

  // else send next command
  else if (cmnd_waiting && cmnd_mode) LD2454SendCommand (HilinkGetNextCommand ());
}

// Handling serial recpetion
void LD2454SerialReception ()
{
  uint8_t  delta, index;
  uint32_t value;
  
  // check sensor presence
  if (hilink_config.device != HLK_DEVICE_LD2454) return;
  if (hilink_status.pserial == nullptr) return;

  // run serial receive loop
  while (hilink_status.pserial->available ()) 
  {
    // init
    value = 0;
    
    // receive character
    HilinkReceptionAppend ((uint8_t)hilink_status.pserial->read ());

    // if enough data received for header
    if (hilink_reception.idx_body >= 4)
    {
      // look for command or energy header
      delta = hilink_reception.idx_body - 4;
      value = HilinkConvertSerialToUint32 (hilink_reception.arr_body[delta + 3], hilink_reception.arr_body[delta + 2], hilink_reception.arr_body[delta + 1], hilink_reception.arr_body[delta]);

      // check if we are in command mode
      if ((value == 0xfdfcfbfa) || (value == 0xf4f3f2f1)) hilink_reception.data = false;

      // check for radar data reception
      else if (value == 0xaaff0300) hilink_reception.data = true;
 
      // if receiving data, frame end is 2 bytes only (55 CC)
      if (hilink_reception.data)
      {
        delta = hilink_reception.idx_body - 2;
        value = HilinkConvertSerialToUint32 (hilink_reception.arr_body[delta + 1], hilink_reception.arr_body[delta], 0, 0);
      }

      // check for header or footer
      switch (value)
      {
        case 0xfdfcfbfa:    // command header
        case 0xaaff0300:    // radar data header
          // if more than 4 digits received, remove everything before header
          if (hilink_reception.idx_body > 4)
          {
            for (index = 0; index < 4; index ++) hilink_reception.arr_body[index] = hilink_reception.arr_body[delta + index];
            hilink_reception.idx_body = 4;
          }
          break;

        case 0x04030201:    // command footer
          LD2454HandleReceivedCommand ();
          break;

        case 0x55cc:        // radar data footer
          LD2454HandleReceivedData ();
          break;
      }
    }
  }
}

/***************************************\
 *              Interface
\***************************************/

// LD2454 sensor
bool XsnsHilinkLD2454 (const uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
    case FUNC_COMMAND:
      if (hilink_config.device == HLK_DEVICE_LD2454) result = DecodeCommand (kHLKLD2454Commands, HLKLD2454Command);
      break;

    case FUNC_EVERY_100_MSECOND:
      LD2454SerialReception ();
      LD2454ProcessCommandQueue ();
      break;
  }

  return result;
}

#endif     // USE_HILINK_LD2454
#endif     // USE_HILINK_DETECTOR
