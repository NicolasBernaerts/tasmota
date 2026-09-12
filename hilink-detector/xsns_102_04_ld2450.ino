/*
  xsns_102_04_ld2450.ino - Driver for Movement sensor HLK-LD2450

  Copyright (C) 2023-2026  Nicolas Bernaerts

  Version history :
    03/09/2023 - v1.0 - Creation
    12/09/2023 - v1.1 - Switch to LD2410 Rx & LD2410 Tx
    20/11/2023 - v1.2 - Tasmota 13.2 compatibility
                        Switch parameters to rf_code[2]
    12/12/2023 - v1.3 - Change help command to ld2450_help 
    15/03/2024 - v2.0 - Add commands 
    08/06/2026 - v3.0 - Complete rewrite to be used as a generic hilink detector
    17/07/2026 - v3.1 - Set bluetooth flag persistent
    29/07/2026 - v3.2 - Switch from static to motion

  Connexions :
    * GPIO1 should be declared as LD2410 Tx and connected to HLK-LD2450 Rx
    * GPIO3 should be declared as LD2410 Rx and connected to HLK-LD2450 Tx

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
#ifdef USE_HILINK_LD2450

#include <TasmotaSerial.h>

/*************************************************\
 *               Variables
\*************************************************/

// constant
#define LD2450_START_DELAY              5        // sensor startup delay

#define LD2450_DISTANCE_MIN             50       // minimum detection distance (cm)
#define LD2450_DISTANCE_MAX             600      // default minimum detection distance (cm)

#define LD2450_DATA_RATE                256000

#define LD2450_GATE_QUANTITY            1
#define LD2450_GATE_WIDTH               600

#define LD2450_MAX_ZONE                 3
#define LD2450_MAX_TARGET               3

#define LD2450_MINIMUM_FIRMWARE         0x0214    // minimum firmware version not to be outdated

// ----------------
// binary commands
// ----------------

// command handling steps
enum LD2450StepCommand { LD2450_STEP_DATA, LD2450_STEP_COMMAND_SENT, LD2450_STEP_COMMAND_RECV, LD2450_STEP_MAX };

// list of commands available
enum LD2450ListCommand { LD2450_CMND_NONE, LD2450_CMND_START, LD2450_CMND_STOP, LD2450_CMND_READ_FIRMWARE, LD2450_CMND_RESET, LD2450_CMND_RESTART, LD2450_CMND_BLUETOOTH_ON, LD2450_CMND_BLUETOOTH_OFF, LD2450_CMND_READ_MAC, LD2450_CMND_TARGET_QUERY, LD2450_CMND_TARGET_SINGLE, LD2450_CMND_TARGET_MULTI, LD2450_CMND_ZONE_QUERY, LD2450_CMND_ZONE_SET, LD2450_CMND_MAX };

// target
enum LD2450Target                  { LD2450_TARGET_SINGLE, LD2450_TARGET_MULTI };
const char kLD2450Target[] PROGMEM =       "single"     "|"       "multi"       ;

// ----------------
// binary commands
// ----------------

uint8_t ld2450_cmnd_reset[]         PROGMEM = { 0xa2, 0x00 };
uint8_t ld2450_cmnd_restart[]       PROGMEM = { 0xa3, 0x00 };

uint8_t ld2450_cmnd_read_firmware[] PROGMEM = { 0xa0, 0x00 };
uint8_t ld2450_cmnd_read_mac[]      PROGMEM = { 0xa5, 0x00, 0x01, 0x00 };

uint8_t ld2450_cmnd_target_single[] PROGMEM = { 0x80, 0x00 };
uint8_t ld2450_cmnd_target_multi[]  PROGMEM = { 0x90, 0x00 };

uint8_t ld2450_cmnd_target_query[]  PROGMEM = { 0x91, 0x00 };

uint8_t ld2450_cmnd_bluetooth[]     PROGMEM = { 0xa4, 0x00, 0x00, 0x00 };

uint8_t ld2450_cmnd_zone_query[]    PROGMEM = { 0xc1, 0x00 };
uint8_t ld2450_cmnd_zone_set[]      PROGMEM = { 0xc2, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

// MQTT commands
const char kHLKLD2450Commands[]         PROGMEM = "hlk" "|" "_reset" "|"    "_restart"   "|"        "_bt"      "|"        "_mode"     "|"    "_zone"      ;
void (* const HLKLD2450Command[])(void) PROGMEM = {   &CmndLD2450Reset, &CmndLD2450Restart, &CmndLD2450Bluetooth, &CmndLD2450TargetMode, &CmndLD2450Zone };

/**************************************************\
 *                  Commands
\**************************************************/

// sensor detection zone reset
void CmndLD2450Reset ()
{
  HilinkAppendCommand (LD2450_CMND_RESET);
  ResponseCmndDone ();
}

void CmndLD2450Restart ()
{
  HilinkAppendCommand (LD2450_CMND_RESTART);
  ResponseCmndDone ();
}

void CmndLD2450Bluetooth ()
{
  if (XdrvMailbox.payload == 1) HilinkAppendCommand (LD2450_CMND_BLUETOOTH_ON);
    else HilinkAppendCommand (LD2450_CMND_BLUETOOTH_OFF);

  // append device restart
  HilinkAppendCommand (LD2410_CMND_RESTART);

  ResponseCmndDone ();
}

void CmndLD2450TargetMode ()
{
  int  device = -2;
  char str_answer[8];

  // handle parameter
  if (XdrvMailbox.data_len > 0)
  {
    device = GetCommandCode (str_answer, sizeof (str_answer), XdrvMailbox.data, kLD2450Target);
    if (device == LD2450_TARGET_SINGLE) HilinkAppendCommand (LD2450_CMND_TARGET_SINGLE);
      else if (device == LD2450_TARGET_MULTI) HilinkAppendCommand (LD2450_CMND_TARGET_MULTI);
  }

  if (device == -2) ResponseCmndChar (GetTextIndexed (str_answer, sizeof (str_answer), hilink_config.device, kLD2450Target));
    else if (device == -1) ResponseCmndFailed ();
    else ResponseCmndDone ();
}

// sensor detection zone setup
void CmndLD2450Zone ()
{
  bool    handled = false;
  uint8_t index;
  int     command;
  char   *pstr_param;
  char    str_text[24];

  // handle parameter
  if (XdrvMailbox.data_len > 0)
  {
    // look for delimiter
    pstr_param = strchr (XdrvMailbox.data, ' ');
    if (pstr_param != nullptr) *pstr_param++ = 0;

    // look for command
    handled = true;
    command = GetCommandCode (str_text, sizeof (str_text), XdrvMailbox.data, kHilinkZone);
    switch (command)
    {
      case HILINK_ZONE_RESET:       // reset zone detection
        hilink_zone.status = HILINK_ZONE_DISABLE;
        for (index = 0; index < LD2450_MAX_ZONE; index ++)
        {
          hilink_zone.arr_zone[index].x1 = 0;
          hilink_zone.arr_zone[index].y1 = 0;
          hilink_zone.arr_zone[index].x2 = 0;
          hilink_zone.arr_zone[index].y2 = 0;
        }
        break;
        
      case HILINK_ZONE_DISABLE:     // disable zone detection
        hilink_zone.status = HILINK_ZONE_DISABLE;
        break;
        
      case HILINK_ZONE_INCLUDE:     // set inclusion zone
        hilink_zone.status = HILINK_ZONE_INCLUDE;
        break;
        
      case HILINK_ZONE_EXCLUDE:     // set exclusion zone
        hilink_zone.status = HILINK_ZONE_EXCLUDE;
        break;
        
      case HILINK_ZONE_1:           // update zone 1
        LD2450SetDetectionZone (0, pstr_param);
        break;
        
      case HILINK_ZONE_2:           // update zone 2
        LD2450SetDetectionZone (1, pstr_param);
        break;
        
      case HILINK_ZONE_3:           // update zone 3
        LD2450SetDetectionZone (2, pstr_param);
        break;
        
      default:
        handled = false;
        break;
    }

    // if zone modified, send update command
    if (handled) HilinkAppendCommand (LD2450_CMND_ZONE_SET);
  }

  // query detection zone
  else
  {
    handled = true;
    HilinkAppendCommand (LD2450_CMND_ZONE_QUERY);
  }

  if (handled) ResponseCmndDone ();
    else ResponseCmndFailed ();
}

/******************************\
 *       Common commands
\******************************/

void LD2450DeviceCommand (const uint8_t command, const uint8_t context)
{
  uint8_t index;

switch (command)
  {
    // display device help
    case HILINK_CMND_HELP:
      AddLog (LOG_LEVEL_INFO, PSTR ("%s specific commands :"), hilink_status.str_model);
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_reset       = reset detector (will restart)"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_restart     = restart detector"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_bt <0/1>    = set bluetooth"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_zone        = query detection zone"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_mode <mode> = target mode"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    single : single target mode"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    multi  : multi target mode"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_zone <cmnd>     = set zone detection behaviour"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    reset : reset all detection zone"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    off   : disable zones"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    inc   : detection within zones"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    exc   : detection outside zones"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_zone id x1,y1,x2,y2 = set detection zone"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    id    : 1..%u (zone index)"), hilink_zone.max);
      AddLog (LOG_LEVEL_INFO, PSTR ("    x1,x2 : -%u..%u (cm)"),       hilink_status.dist_limit, hilink_status.dist_limit);
      AddLog (LOG_LEVEL_INFO, PSTR ("    y1,y2 : 0..%u (cm)"),         hilink_status.dist_limit);
      break;

    // init device parameters
    case HILINK_CMND_INIT:
      // declare device
      hilink_status.baudrate   = LD2450_DATA_RATE;
      hilink_status.gate_qty   = LD2450_GATE_QUANTITY;
      hilink_status.gate_width = LD2450_GATE_WIDTH;
      if (hilink_config.dist_min   < LD2450_DISTANCE_MIN) hilink_config.dist_min   = LD2450_DISTANCE_MIN;
      if (hilink_status.dist_limit > LD2450_DISTANCE_MAX) hilink_status.dist_limit = LD2450_DISTANCE_MAX;

      // set presence and motion detection status
      hilink_static.enabled = false;
      hilink_motion.enabled = true;

      // set radar and zones
      hilink_status.radar2D = true;
      hilink_zone.max       = LD2450_MAX_ZONE;
      break;

    // device environment initialisation
    case HILINK_CMND_FIRST:
      // default config at startup
      hilink_reception.data   = true;
      hilink_reception.energy = false;
      hilink_command.mode     = false;

      // set initial delay
      HilinkSetCommandDelay (LD2450_START_DELAY);

      // get device general config
      HilinkAppendCommand (LD2450_CMND_READ_MAC);
      HilinkAppendCommand (LD2450_CMND_READ_FIRMWARE);
      HilinkAppendCommand (LD2450_CMND_TARGET_QUERY);
      HilinkAppendCommand (LD2450_CMND_ZONE_QUERY);

      // set bluetooth
      (hilink_config.param.bluetooth == 1) ? HilinkAppendCommand (LD2450_CMND_BLUETOOTH_ON) : HilinkAppendCommand (LD2450_CMND_BLUETOOTH_OFF);
      break;

    // read device data information
    case HILINK_CMND_INFO:
      HilinkAppendCommand (LD2450_CMND_READ_MAC);
      HilinkAppendCommand (LD2450_CMND_READ_FIRMWARE);
      break;
      
    // read device parameters
    case HILINK_CMND_PARAM:
      HilinkAppendCommand (LD2450_CMND_TARGET_QUERY);
      HilinkAppendCommand (LD2450_CMND_ZONE_QUERY);
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
          for (index = 0; index < LD2450_MAX_TARGET; index ++) ResponseAppend_P (PSTR (",\"T%u\":{\"X\":%d,\"Y\":%d,\"Dist\":%u,\"Speed\":%d}"), index + 1, hilink_motion.arr_target[index].x, hilink_motion.arr_target[index].y, hilink_motion.arr_target[index].dist, hilink_motion.arr_target[index].speed);
          break;
      } 
      break;
  }
}

/**************************************************\
 *                  Functions
\**************************************************/

// convert LD2450 coordinate format to MSB and LSB
void LD2450ConvertCoordinate2LsbMsb (const int16_t coordinate, uint8_t &lsb, uint8_t &msb)
{
  int32_t value;

  // convert according to positive or negative value
  if (coordinate >= 0) value = 32768 + (int32_t)coordinate;
    else value = - (int32_t)coordinate;

  msb = (uint8_t)(value / 256);
  lsb = (uint8_t)(value % 256);
}

void LD2450SetDetectionZone (const uint8_t zone, const char* pstr_param)
{
  uint8_t index;
  char   *pstr_token;
  char    str_data[32];

  // check parameters
  if (zone >= LD2450_MAX_ZONE) return;
  if (pstr_param == nullptr) return;

  // loop thru tokens
  index = 0;
  strcpy (str_data, pstr_param);
  pstr_token = strtok (str_data, ",");
  while( pstr_token != nullptr)
  {
    // if value is given, add update command
    if (strlen (pstr_token) > 0) switch (index)
    {
      case 0: hilink_zone.arr_zone[zone].x1 = (int16_t)atoi (pstr_token); break;
      case 1: hilink_zone.arr_zone[zone].y1 = (int16_t)atoi (pstr_token); break;
      case 2: hilink_zone.arr_zone[zone].x2 = (int16_t)atoi (pstr_token); break;
      case 3: hilink_zone.arr_zone[zone].y2 = (int16_t)atoi (pstr_token); break;
    }

    // search for next token    
    pstr_token = strtok (nullptr, ",");
    index ++;
  }

AddLog (LOG_LEVEL_INFO, PSTR ("HLK: Set zone %u : %d %d %d %d"), zone, hilink_zone.arr_zone[zone].x1, hilink_zone.arr_zone[zone].y1, hilink_zone.arr_zone[zone].x2, hilink_zone.arr_zone[zone].y2); 
}

/*********************************************\
 *              Communication
\*********************************************/

// send command
void LD2450SendCommand (const uint8_t command)
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
    case LD2450_CMND_START:
      body = sizeof (hilink_cmnd_start);
      memcpy_P (arr_buffer, hilink_cmnd_start, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s command ON"), hilink_status.str_model);
      break;

    case LD2450_CMND_STOP:
      body = sizeof (hilink_cmnd_stop);
      memcpy_P (arr_buffer, hilink_cmnd_stop, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s command OFF"), hilink_status.str_model);
      break;

    case LD2450_CMND_READ_FIRMWARE:
      body = sizeof (ld2450_cmnd_read_firmware);
      memcpy_P (arr_buffer, ld2450_cmnd_read_firmware, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s read firmware"), hilink_status.str_model);
      break;

    case LD2450_CMND_READ_MAC:
      body = sizeof (ld2450_cmnd_read_mac);
      memcpy_P (arr_buffer, ld2450_cmnd_read_mac, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s read MAC address"), hilink_status.str_model);
      break;

    case LD2450_CMND_RESET:
      body = sizeof (ld2450_cmnd_reset);
      memcpy_P (arr_buffer, ld2450_cmnd_reset, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s reset"), hilink_status.str_model);
      break;

    case LD2450_CMND_RESTART:
      body = sizeof (ld2450_cmnd_restart);
      memcpy_P (arr_buffer, ld2450_cmnd_restart, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s restart"), hilink_status.str_model);

      // prepare init commands after restart
      HilinkDeviceCommand (HILINK_CMND_FIRST);
      break;

    case LD2450_CMND_TARGET_QUERY:
      body = sizeof (ld2450_cmnd_target_query);
      memcpy_P (arr_buffer, ld2450_cmnd_target_query, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s query target mode"), hilink_status.str_model);
      break;

    case LD2450_CMND_TARGET_SINGLE:
      body = sizeof (ld2450_cmnd_target_single);
      memcpy_P (arr_buffer, ld2450_cmnd_target_single, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s single target mode"), hilink_status.str_model);
      break;

    case LD2450_CMND_TARGET_MULTI:
      body = sizeof (ld2450_cmnd_target_multi);
      memcpy_P (arr_buffer, ld2450_cmnd_target_multi, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s multi target mode"), hilink_status.str_model);
      break;

    case LD2450_CMND_ZONE_QUERY:
      body = sizeof (ld2450_cmnd_zone_query);
      memcpy_P (arr_buffer, ld2450_cmnd_zone_query, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s query detection zone"), hilink_status.str_model);
      break;

    case LD2450_CMND_ZONE_SET:
      body = sizeof (ld2450_cmnd_zone_set);
      memcpy_P (arr_buffer, ld2450_cmnd_zone_set, body);

      // set detection zone method
      arr_buffer[2] = hilink_zone.status;

      // set 3 detection zones
      for (index = 0; index < 3; index ++)
      {
        start = 4 + index * 8;
        LD2450ConvertCoordinate2LsbMsb (10 * hilink_zone.arr_zone[index].x1, lsb, msb);
        arr_buffer[start++] = lsb;
        arr_buffer[start++] = msb;
        LD2450ConvertCoordinate2LsbMsb (10 * hilink_zone.arr_zone[index].y1, lsb, msb);
        arr_buffer[start++] = lsb;
        arr_buffer[start++] = msb;
        LD2450ConvertCoordinate2LsbMsb (10 * hilink_zone.arr_zone[index].x2, lsb, msb);
        arr_buffer[start++] = lsb;
        arr_buffer[start++] = msb;
        LD2450ConvertCoordinate2LsbMsb (10 * hilink_zone.arr_zone[index].y2, lsb, msb);
        arr_buffer[start++] = lsb;
        arr_buffer[start++] = msb;
      }
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set detection zone"), hilink_status.str_model);
      break;

    case LD2450_CMND_BLUETOOTH_OFF:
      hilink_config.param.bluetooth = 0;
      body = sizeof (ld2450_cmnd_bluetooth);
      memcpy_P (arr_buffer, ld2450_cmnd_bluetooth, body);
      arr_buffer[2] = hilink_config.param.bluetooth;
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s bluetooth OFF"), hilink_status.str_model);
      break;

    case LD2450_CMND_BLUETOOTH_ON:
      hilink_config.param.bluetooth = 1;
      body = sizeof (ld2450_cmnd_bluetooth);
      memcpy_P (arr_buffer, ld2450_cmnd_bluetooth, body);
      arr_buffer[2] = hilink_config.param.bluetooth;
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s bluetooth ON"), hilink_status.str_model);
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
void LD2450HandleReceivedCommand ()
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
        HilinkAppendCommand (LD2450_CMND_TARGET_QUERY);
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s set to Single target mode"), hilink_status.str_model);
        break;
      
      case 0x9001:    // set multi target tracking
        HilinkAppendCommand (LD2450_CMND_TARGET_QUERY);
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s set to Multi target mode"), hilink_status.str_model);
        break;
      
      case 0x9101:    // query target tracking
        //  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
        // FD FC FB FA 06 00 91 01 00 00 02 00 04 03 02 01
        // header     | len |     |     |xx   |  footer
        if (hilink_reception.arr_body[10] == 2)
        {
          hilink_config.param.multi = 1;
          hilink_status.char_green  = 'M';
          AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s is in Multi target mode"), hilink_status.str_model);
        }
        else
        {
          hilink_config.param.multi = 0;
          hilink_status.char_green  = 'S';
          AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s is in Single target mode"), hilink_status.str_model);
        }
        break;
      
      case 0xa001:    // read firmware
        sprintf_P (str_text, PSTR ("%x.%02x.%02x%02x%02x%02x"), hilink_reception.arr_body[13], hilink_reception.arr_body[12], hilink_reception.arr_body[17], hilink_reception.arr_body[16], hilink_reception.arr_body[15], hilink_reception.arr_body[14]);
        hilink_status.str_firmware = str_text;
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s firmware %s"), hilink_status.str_model, str_text);

        // check for outdated firmware
        value = 256 * (uint16_t)hilink_reception.arr_body[13] + (uint16_t)hilink_reception.arr_body[12];
        hilink_status.outdated = (value < LD2450_MINIMUM_FIRMWARE);
        if (hilink_status.outdated) AddLog (LOG_LEVEL_INFO, PSTR ("HLK: WARNING your firmware %s is too old ! Update to get full features"), hilink_status.str_firmware.c_str ());
        break;

      case 0xa201:  // reset device
        HilinkAppendCommand (LD2450_CMND_RESTART);
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s has been reset"), hilink_status.str_model);
        break;

      case 0xa301:  // restart device
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s will restart"), hilink_status.str_model);
        break;

      case 0xa401:  // set bluetooth
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s bluetooth changed"), hilink_status.str_model);
        break;

      case 0xa501:  // read MAC address
        //  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17
        // FD FC FB FA A0 00 A5 01 aa bb cc dd ee ff 04 03 02 01
        // header     | len |     |   mac address   |   footer
        sprintf_P (hilink_status.str_mac, PSTR ("%02X:%02X:%02X:%02X:%02X:%02X"), hilink_reception.arr_body[10], hilink_reception.arr_body[11], hilink_reception.arr_body[12], hilink_reception.arr_body[13], hilink_reception.arr_body[14], hilink_reception.arr_body[15]);
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s MAC is %s"), hilink_status.str_model, hilink_status.str_mac);
        break;

      case 0xc101:  // query zones
        //  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39
        // FD FC FB FA 1E 00 C1 01 00 00 xx 00 x1 x1 y1 y1 x2 x2 y2 y2 x1 x1 y1 y1 x2 x2 y2 y2 x1 x1 y1 y1 x2 x2 y2 y2 04 03 02 01
        // header     | len |           |mode |   x1,y1,x2,y2 zone 1  |   x1,y1,x2,y2 zone 2  |   x1,y1,x2,y2 zone 3  |   footer

        // get detection mode
        hilink_zone.status = hilink_reception.arr_body[10];
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s zone mode is %s"), hilink_status.str_model, GetTextIndexed (str_text, sizeof (str_text), hilink_zone.status, kHilinkZoneLabel));

        // loop to collect zone
        for (index = 0; index < LD2450_MAX_ZONE; index ++)
        {
          shift = 12 + index * 8;
          hilink_zone.arr_zone[index].x1 = HilinkConvertLsbMsb2Value (hilink_reception.arr_body[shift++], hilink_reception.arr_body[shift++]) / 10;
          hilink_zone.arr_zone[index].y1 = HilinkConvertLsbMsb2Value (hilink_reception.arr_body[shift++], hilink_reception.arr_body[shift++]) / 10;
          hilink_zone.arr_zone[index].x2 = HilinkConvertLsbMsb2Value (hilink_reception.arr_body[shift++], hilink_reception.arr_body[shift++]) / 10;
          hilink_zone.arr_zone[index].y2 = HilinkConvertLsbMsb2Value (hilink_reception.arr_body[shift++], hilink_reception.arr_body[shift++]) / 10;
          AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s zone %u is [%d,%d,%d,%d]"), hilink_status.str_model, index + 1, hilink_zone.arr_zone[index].x1, hilink_zone.arr_zone[index].y1, hilink_zone.arr_zone[index].x2, hilink_zone.arr_zone[index].y2);
        }
        break;

      case 0xc201:  // set zones
        HilinkAppendCommand (LD2450_CMND_ZONE_QUERY);
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s zone set"), hilink_status.str_model);
        break;
    }
  }

  // declare command as finished
  HilinkCommandFinished ();
}

// Handling of received data
void LD2450HandleReceivedData ()
{
  bool     active;
  size_t   index, start;
  uint16_t dist;
  int16_t  speed;
  int32_t  x, y;

  // log received message
  HilinkLogHexa (hilink_reception.arr_body, hilink_reception.idx_body, false, false);

  // loop thru targets
  for (index = 0; index < LD2450_MAX_TARGET; index ++)
  {
    // set target coordonnates
    start = 4 + index * 8;

    // read x, y
    x     = (int32_t)HilinkConvertLsbMsb2Value (hilink_reception.arr_body[start++], hilink_reception.arr_body[start++]) / 10;
    y     = (int32_t)HilinkConvertLsbMsb2Value (hilink_reception.arr_body[start++], hilink_reception.arr_body[start++]) / 10;
    speed = HilinkConvertLsbMsb2Value (hilink_reception.arr_body[start++], hilink_reception.arr_body[start++]);
    dist  = (uint16_t)sqrt ((float)(x * x + y * y));
    hilink_motion.arr_target[index].active = (dist >= hilink_config.dist_min);

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
void LD2450ProcessCommandQueue ()
{
  bool cmnd_mode, cmnd_waiting;

  // check validity
  if (hilink_config.device != HLK_DEVICE_LD2450) return;
  if (hilink_status.pserial == nullptr) return;
  if (!HilinkCommandPossible ()) return;

  // check if commands are waiting
  cmnd_mode    = HilinkIsInCommandMode ();
  cmnd_waiting = HilinkCommandWaiting ();

  // commands in the pipe and command mode not started, start command mode
  if (cmnd_waiting && !cmnd_mode) LD2450SendCommand (LD2450_CMND_START);
    
  // else if no command in the queue, stop command mode
  else if (!cmnd_waiting && cmnd_mode) LD2450SendCommand (LD2450_CMND_STOP);

  // else send next command
  else if (cmnd_waiting && cmnd_mode) LD2450SendCommand (HilinkGetNextCommand ());
}

// Handling serial recpetion
void LD2450SerialReception ()
{
  uint8_t  delta, index;
  uint32_t value;
  
  // check sensor presence
  if (hilink_config.device != HLK_DEVICE_LD2450) return;
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
 
      // if receiving data
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
          LD2450HandleReceivedCommand ();
          break;

        case 0x55cc:        // radar data footer
          LD2450HandleReceivedData ();
          break;
      }
    }
  }
}

/***************************************\
 *              Interface
\***************************************/

// LD2450 sensor
bool XsnsHilinkLD2450 (const uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
    case FUNC_COMMAND:
      if (hilink_config.device == HLK_DEVICE_LD2450) result = DecodeCommand (kHLKLD2450Commands, HLKLD2450Command);
      break;

    case FUNC_EVERY_100_MSECOND:
      LD2450SerialReception ();
      LD2450ProcessCommandQueue ();
      break;
  }

  return result;
}

#endif     // USE_HILINK_LD2450
#endif     // USE_HILINK_DETECTOR
