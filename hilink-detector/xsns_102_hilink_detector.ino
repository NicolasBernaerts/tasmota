/*
  xsns_102_102_detector.ino - Hi-Link movement and presence detector driver
    Reading is possible from local or remote (MQTT topic/key) sensors

  Copyright (C) 2020-2025  Nicolas Bernaerts
    26/06/2022 - v1.0 - Management of HLK-LD1115H and HLK-LD1125H sensors
    15/01/2023 - v1.1 - Add HLK-LD2410 sensor
    14/09/2024 - v1.2 - Add HLK-LD2450 sensor
    09/02/2025 - v2.0 - Tasmota 15 and rewrite for Hi-Link driver
    01/05/2026 - v2.1 - Add HLK-LD2420 sensor
    14/05/2026 - v2.2 - Add HLK-LD2410s sensor
    06/06/2026 - v2.3 - Add HLK-LD2401 and HLK-LD2402 sensor
    14/07/2026 - v2.4 - Add HLK-LD2412 sensor
    17/07/2026 - v2.5 - Set bluetooth and light sensitivity flags persistent
                        Separate LD2410B and LD2410C

  Manage sensors connected thru serial port (Rx 2410 and Tx 2410) :
    - HLK-LD1115
    - HLK-LD1125
    - HLK-LD2401
    - HLK-LD2402
    - HLK-LD2410b
    - HLK-LD2410c
    - HLK-LD2410s
    - HLK-LD2412
    - HLK-LD2420
    - HLK-LD2450

  Configuration values are stored in :
    - Settings->rf_code[2][0] : type of Hilink detector
    - Settings->rf_code[2][1] : misc data (bluetooth, output level, light policy and log policy)
    - Settings->rf_code[2][2] : minimum detection distance
    - Settings->rf_code[2][3] : maximum presence detection distance
    - Settings->rf_code[2][4] : maximum motion detection distance
    - Settings->rf_code[2][5] : nobody delay
    - Settings->rf_code[2][6] : light level threshold

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

#define XSNS_102                              102

// colors
constexpr char PSTR_HILINK_COLOR_PRESENCE[]   PROGMEM = "#1fa3ec";
constexpr char PSTR_HILINK_COLOR_MOTION[]     PROGMEM = "#b00";
constexpr char PSTR_HILINK_COLOR_BLUE[]       PROGMEM = "#0082fc";
constexpr char PSTR_HILINK_COLOR_GREEN[]      PROGMEM = "#090";
constexpr char PSTR_HILINK_COLOR_YELLOW[]     PROGMEM = "#b83";
constexpr char PSTR_HILINK_COLOR_WARNING[]    PROGMEM = "#c80";
constexpr char PSTR_HILINK_COLOR_ERROR[]      PROGMEM = "#b00";

// web pages
constexpr char PSTR_HILINK_PAGE_RADAR[]       PROGMEM = "/radar";
constexpr char PSTR_HILINK_PAGE_RADAR_JS[]    PROGMEM = "/radar.js";
constexpr char PSTR_HILINK_PAGE_RADAR_CSS[]   PROGMEM = "/radar.css";
constexpr char PSTR_HILINK_PAGE_RADAR_UPD[]   PROGMEM = "/radar.upd";
constexpr char PSTR_HILINK_PAGE_RADAR2D[]     PROGMEM = "/radar2d";
constexpr char PSTR_HILINK_PAGE_RADAR2D_JS[]  PROGMEM = "/radar2d.js";
constexpr char PSTR_HILINK_PAGE_RADAR2D_CSS[] PROGMEM = "/radar2d.css";
constexpr char PSTR_HILINK_PAGE_RADAR2D_UPD[] PROGMEM = "/radar2d.upd";

// MQTT commands : ld_help and ld_send
const char kHilinkCommands[]          PROGMEM = "hlk" "|"       "|"    "_device"   "|"   "_log"    "|"     "_info"     "|"     "_param"     "|"    "_delay"   "|"      "_min"     "|"      "_max"        ;
void (* const HilinkCommand[])(void)  PROGMEM = { &CmndHilinkHelp, &CmndHilinkDevice, &CmndHilinkLog, &CmndHilinkGetInfo, &CmndHilinkGetParam, &CmndHilinkDelay, &CmndHilinkDistMin, &CmndHilinkDistMax };

/**************************************************\
 *                  Config
\**************************************************/

uint16_t HilinkConfig2Delay (const uint8_t value)
{
  uint16_t result, delay;

  delay = (uint16_t)value;
  if (delay > 156)      result = 7200 + (delay - 156) * 1800;      // every 30 mn after 2h
  else if (delay > 138) result = 1800 + (delay - 138) * 300;       // every 5 mn after 30 mn
  else if (delay > 118) result = 600  + (delay - 118) * 60;        // every 1 mn after 10 mn
  else if (delay > 108) result = 300  + (delay - 108) * 30;        // every 30 s after 5 mn
  else if (delay > 60)  result = 60   + (delay - 60)  * 5;         // every 5 s after 1 mn
  else result = delay;                                             // every second if less than 1 mn

  return result;
}

uint8_t HilinkDelay2Config (const uint16_t delay)
{
  uint16_t result;

  if (delay > 7200) result = 156 + (delay - 7200) / 1800;
  else if (delay > 1800) result = 138 + (delay - 1800) / 300;
  else if (delay > 600) result = 118 + (delay - 600)  / 60;
  else if (delay > 300) result = 108 + (delay - 300)  / 30;
  else if (delay > 60)  result = 60  + (delay - 60)   / 5;
  else result = delay;

  return (uint8_t)result;
}

uint16_t HilinkConfigToDistance (const uint8_t value)
{
  uint16_t result, distance;

  distance = (uint16_t)value;
  if (value > 219)      result = 1200 + (distance - 219) * 50;        // every 50 cm after 1200 cm
  else if (value > 200) result = 1000 + (distance - 200) * 10;        // every 10 cm after 1000 cm
  else result = distance * 5;                                         // every 5 cm before 1000 cm

  return result;
}

uint8_t HilinkDistanceToConfig (const uint16_t distance)
{
  uint16_t result;

  if (distance > 1200) result = 219 + (distance - 1200) / 50;
  else if (distance > 1000) result = 200 + (distance - 1000) / 10;
  else result = distance / 5;

  return (uint8_t)result;
}

// Load configuration from flash memory
void HilinkLoadConfig ()
{
  // load device
  hilink_config.device = Settings->rf_code[2][0];

  // load general data
  hilink_config.param.data = Settings->rf_code[2][1];

  // load distances
  hilink_config.dist_min = HilinkConfigToDistance (Settings->rf_code[2][2]);
  hilink_config.pres_max = HilinkConfigToDistance (Settings->rf_code[2][3]);
  hilink_config.move_max = HilinkConfigToDistance (Settings->rf_code[2][4]);

  // load delay & light sensitivity
  hilink_config.delay       = HilinkConfig2Delay (Settings->rf_code[2][5]);
  hilink_config.light_thres = Settings->rf_code[2][6];
  
  // check boudaries
  if (hilink_config.device >= HLK_DEVICE_MAX) hilink_config.device = HLK_DEVICE_NONE;
  if (hilink_config.delay    == 0) hilink_config.delay    = HILINK_DEFAULT_DELAY;
  if (hilink_config.pres_max == 0) hilink_config.pres_max = HILINK_DEFAULT_DISTANCE;
  if (hilink_config.move_max == 0) hilink_config.move_max = HILINK_DEFAULT_DISTANCE;
  if (hilink_config.dist_min >= hilink_config.pres_max) hilink_config.dist_min = 0;
}

// Save configuration into flash memory
void HilinkSaveConfig ()
{
  // save basic config
  Settings->rf_code[2][0] = hilink_config.device;
  Settings->rf_code[2][1] = hilink_config.param.data;

  // save distances
  Settings->rf_code[2][2] = HilinkDistanceToConfig (hilink_config.dist_min);
  Settings->rf_code[2][3] = HilinkDistanceToConfig (hilink_config.pres_max);
  Settings->rf_code[2][4] = HilinkDistanceToConfig (hilink_config.move_max);

  // save delay & light sensitivity
  Settings->rf_code[2][5] = HilinkDelay2Config (hilink_config.delay);
  Settings->rf_code[2][6] = hilink_config.light_thres;
}

/***********************************************\
 *                  Commands
\***********************************************/

uint8_t HilinkGetDeviceIdent (const uint8_t device)
{
  uint8_t result = UINT8_MAX;

  // devices list
  switch (device)
  {
    case HLK_DEVICE_NONE: result = device; break;

#ifdef USE_HILINK_LD1115
    case HLK_DEVICE_LD1115: result = device; break;
#endif    // USE_HILINK_LD1115

#ifdef USE_HILINK_LD1125
    case HLK_DEVICE_LD1125: result = device; break;
#endif    // USE_HILINK_LD1125

#ifdef USE_HILINK_LD2401
    case HLK_DEVICE_LD2401: result = device; break;
#endif    // USE_HILINK_LD2401

#ifdef USE_HILINK_LD2402
    case HLK_DEVICE_LD2402: result = device; break;
#endif    // USE_HILINK_LD2402

#ifdef USE_HILINK_LD2410
    case HLK_DEVICE_LD2410B: result = device; break;
    case HLK_DEVICE_LD2410C: result = device; break;
#endif    // USE_HILINK_LD2410

#ifdef USE_HILINK_LD2410S
    case HLK_DEVICE_LD2410S: result = device; break;
#endif    // USE_HILINK_LD2410S

#ifdef USE_HILINK_LD2412
    case HLK_DEVICE_LD2412: result = device; break;
#endif    // USE_HILINK_LD2412

#ifdef USE_HILINK_LD2420
    case HLK_DEVICE_LD2420: result = device; break;
#endif    // USE_HILINK_LD2420

#ifdef USE_HILINK_LD2450
    case HLK_DEVICE_LD2450: result = device; break;
#endif    // USE_HILINK_LD2450
  }

  return result;
}

void HilinkDeviceCommand (const uint8_t command) { HilinkDeviceCommand (command, 0); }
void HilinkDeviceCommand (const uint8_t command, const uint8_t context)
{
  // devices help
  switch (hilink_config.device)
  {
#ifdef USE_HILINK_LD1115
    case HLK_DEVICE_LD1115: LD1115DeviceCommand (command, context); break;
#endif    // USE_HILINK_LD1115

#ifdef USE_HILINK_LD1125
    case HLK_DEVICE_LD1125: LD1125DeviceCommand (command, context); break;
#endif    // USE_HILINK_LD1125

#ifdef USE_HILINK_LD2401
    case HLK_DEVICE_LD2401: LD2401DeviceCommand (command, context); break;
#endif    // USE_HILINK_LD2401

#ifdef USE_HILINK_LD2402
    case HLK_DEVICE_LD2402: LD2402DeviceCommand (command, context); break;
#endif    // USE_HILINK_LD2402

#ifdef USE_HILINK_LD2410
    case HLK_DEVICE_LD2410B: 
    case HLK_DEVICE_LD2410C: 
      LD2410DeviceCommand (command, context); 
      break;
#endif    // USE_HILINK_LD2410

#ifdef USE_HILINK_LD2410S
    case HLK_DEVICE_LD2410S: LD2410sDeviceCommand (command, context); break;
#endif    // USE_HILINK_LD2410S

#ifdef USE_HILINK_LD2412
    case HLK_DEVICE_LD2412: LD2412DeviceCommand (command, context); break;
#endif    // USE_HILINK_LD2412

#ifdef USE_HILINK_LD2420
    case HLK_DEVICE_LD2420: LD2420DeviceCommand (command, context); break;
#endif    // USE_HILINK_LD2420

#ifdef USE_HILINK_LD2450
    case HLK_DEVICE_LD2450: LD2450DeviceCommand (command, context); break;
#endif    // USE_HILINK_LD2450
  }
}

void HilinkSendCommand (const uint8_t *parr_command, const size_t size_command)
{
  size_t  index, size;
  uint8_t arr_buffer[6];
  char    str_text[4];
  String  str_log;

  // check sensor presence
  if (hilink_status.pserial == nullptr) return;

  // empty send buffer (enable chunk mode introducing possible Hardware Watchdogs)
  hilink_status.pserial->setReadChunkMode (1);
  hilink_status.pserial->flush ();

  // send header
  size = sizeof (hilink_cmnd_header);
  memcpy_P (arr_buffer, hilink_cmnd_header, size);
  hilink_status.pserial->write (arr_buffer, size);
  for (index = 0; index < size; index ++) { sprintf_P (str_text, PSTR ("%02X "), arr_buffer[index]); str_log += str_text; }

  // send command size
  arr_buffer[0] = (uint8_t)size_command;
  arr_buffer[1] = 0;
  hilink_status.pserial->write (arr_buffer, 2);
  for (index = 0; index < 2; index ++) { sprintf_P (str_text, PSTR ("%02X "), arr_buffer[index]); str_log += str_text; }

  // send command
  hilink_status.pserial->write (parr_command, size_command);
  for (index = 0; index < size_command; index ++) { sprintf_P (str_text, PSTR ("%02X "), parr_command[index]); str_log += str_text; }

  // send footer
  size = sizeof (hilink_cmnd_footer);
  memcpy_P (arr_buffer, hilink_cmnd_footer, size);
  hilink_status.pserial->write (arr_buffer, size);
  for (index = 0; index < size; index ++) { sprintf_P (str_text, PSTR ("%02X "), arr_buffer[index]); str_log += str_text; }

  // log command
  HilinkLogText (str_log.c_str (), true);
}

void CmndHilinkHelp ()
{
  uint8_t index, device;
  char    str_text[16];
  String  str_list;

  // display commands
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Hi-Link detector commands :"));

  // device model
  GetTextIndexed (str_text, sizeof (str_text), hilink_config.device, kHilinkModel);
  AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_device <dev>    = set device type [%s]"), str_text);

  // loop to get handled device name
  for (index = 0; index < HLK_DEVICE_MAX; index ++)
  {
    device = HilinkGetDeviceIdent (index);
    if (device != UINT8_MAX) { GetTextIndexed (str_text, sizeof (str_text), device, kHilinkModel); str_list += ' '; str_list += str_text; }
  }
  AddLog (LOG_LEVEL_INFO, PSTR ("    %s"), str_list.c_str ());

  // common commands
  GetTextIndexed (str_text, sizeof (str_text), hilink_config.param.log, kHilinkLogLevel);
  AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_log <val>       = log policy [%s]"), str_text);
  AddLog (LOG_LEVEL_INFO, PSTR ("    off data cmnd all"));
  AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_info            = get sensor info"));
  AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_param           = read sensor parameters"));
  AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_delay <val>     = detection timeout [%u s]"), hilink_config.delay);
  AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_min <dist>      = minimum detection distance [%u cm]"), hilink_config.dist_min);
  AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_max <pres,move> = maximum detection distance"));
  AddLog (LOG_LEVEL_INFO, PSTR ("    pres : 1 .. %u [%u cm]"), hilink_status.dist_limit, hilink_config.pres_max);
  AddLog (LOG_LEVEL_INFO, PSTR ("    move : 1 .. %u [%u cm]"), hilink_status.dist_limit, hilink_config.move_max);

  // devices help
  HilinkDeviceCommand (HILINK_CMND_HELP);

  ResponseCmndDone ();
}

// set detector device
void CmndHilinkDevice ()
{
  bool   restart = false;
  int    device;
  size_t index;
  char   str_name[8];

  if (strlen (XdrvMailbox.data) > 0)
  {
    // convert device name to upper case
    for (index = 0; index < strlen (XdrvMailbox.data); index ++)
      if ((XdrvMailbox.data[index] >= 97) && (XdrvMailbox.data[index] <= 122)) XdrvMailbox.data[index] = XdrvMailbox.data[index] - 32;

    // look for known devices
    device = GetCommandCode (str_name, sizeof (str_name), XdrvMailbox.data, kHilinkModel);
    if ((device >= 0) && (hilink_config.device != device))
    {
      hilink_config.device = device;
      restart = true;
    }
  }

  // answer with current device
  GetTextIndexed (str_name, sizeof (str_name), hilink_config.device, kHilinkModel);
  ResponseCmndChar (str_name);

  // if device has chnaged, restart
  if (restart) TasmotaGlobal.restart_flag = 2;
//  if (restart) WebRestart (1);
   
}

// set detector log
void CmndHilinkLog ()
{
  int  level;
  char str_level[8];
  
  // if parameter given, update
  if (XdrvMailbox.data_len > 0)
  {
    level = GetCommandCode(str_level, sizeof (str_level), XdrvMailbox.data, kHilinkLogLevel);
    if ((level != -1) && (level < HILINK_LOG_MAX)) hilink_config.param.log = (uint8_t)level;
  }
  
  // answer
  GetTextIndexed (str_level, sizeof (str_level), hilink_config.param.log, kHilinkLogLevel);
  ResponseCmndChar (str_level);
}

// get device information
void CmndHilinkGetInfo ()
{
  HilinkDeviceCommand (HILINK_CMND_INFO, 0);
  ResponseCmndDone ();
}

// get device parameters
void CmndHilinkGetParam ()
{
  HilinkDeviceCommand (HILINK_CMND_PARAM, 0);
  ResponseCmndDone ();
}

void CmndHilinkDelay ()
{
  // if value given, save detection delay
  if (XdrvMailbox.payload > 0) hilink_config.delay = (uint16_t)XdrvMailbox.payload;

  // forward to device
  HilinkDeviceCommand (HILINK_CMND_DELAY, 0);

  ResponseCmndNumber (hilink_config.delay);
}

void CmndHilinkDistMin ()
{
  uint16_t dist;

  if (XdrvMailbox.payload >= 0) 
  {
    dist = (uint16_t)XdrvMailbox.payload;
    if ((dist < hilink_config.pres_max) && (dist < hilink_config.move_max)) hilink_config.dist_min = dist;
  }

  // forward to device
  HilinkDeviceCommand (HILINK_CMND_DMIN, 0);

  ResponseCmndNumber (hilink_config.dist_min);
}

void CmndHilinkDistMax ()
{
  uint16_t dist;
  char    *pstr_motion;
  char    str_answer[16];

  if (XdrvMailbox.data_len > 0)
  {
    // get separator
    pstr_motion = strchr (XdrvMailbox.data, ',');
    if (pstr_motion != nullptr) *pstr_motion++ = 0;

    // extract presence detection max distance
    dist = (uint16_t)atoi (XdrvMailbox.data);
    if (dist > hilink_config.dist_min) hilink_config.pres_max = dist;

    // extract motion detection max distance
    if (pstr_motion != nullptr) dist = (uint16_t)atoi (pstr_motion);
      else dist = hilink_config.pres_max;
    if (dist > hilink_config.dist_min) hilink_config.move_max = dist;
  }

  // forward to device
  HilinkDeviceCommand (HILINK_CMND_DMAX, 0);

  sprintf_P (str_answer, PSTR ("%u,%u"), hilink_config.pres_max, hilink_config.move_max);
  ResponseCmndChar (str_answer);
}

/*********************************************\
 *                Conversion
\*********************************************/

uint8_t HilinkGateMin ()
{
  uint16_t gate = 0;

  if (hilink_status.gate_width > 0) gate = hilink_config.dist_min / hilink_status.gate_width;

  return (uint8_t)gate;
}

uint8_t HilinkPresenceGateMax ()
{
  uint16_t gate = 0;

  if (hilink_status.gate_width > 0) gate = (hilink_config.pres_max - 1) / hilink_status.gate_width;

  return (uint8_t)gate;
}

uint8_t HilinkMotionGateMax ()
{
  uint16_t gate = 0;

  if (hilink_status.gate_width > 0) gate = (hilink_config.move_max - 1) / hilink_status.gate_width;
  
  return (uint8_t)gate;
}

// convert int16_t to command LSB and MSB
void HilinkConvertUint32ToSerial (const uint32_t value, uint8_t &lsb, uint8_t &byte2, uint8_t &byte3, uint8_t &msb)
{
  msb    = (uint8_t)(value  >> 24);
  byte3  = (uint8_t)((value >> 16) & 0xff);
  byte2  = (uint8_t)((value >> 8) & 0xff);
  lsb    = (uint8_t)(value & 0xff);
}

// convert serial LSB ... MSB to uint32 value
uint32_t HilinkConvertSerialToUint32 (const uint8_t lsb, const uint8_t byte2, const uint8_t byte3, const uint8_t msb)
{
  return ((uint32_t)msb * 0x1000000 + (uint32_t)byte3 * 0x10000 + (uint32_t)byte2 * 0x100 + (uint32_t)lsb);
}

// convert data LSB and MSB to uint16 value
uint16_t HilinkConvertSerialToUint16 (const uint8_t lsb, const uint8_t msb)
{
  return ((uint16_t)msb * 0x100 + (uint16_t)lsb);
}

/**************************************\
 *              Functions
\**************************************/

bool HilinkIsDeclared ()
{
  return (Settings->rf_code[2][0] != HLK_DEVICE_NONE);
}

bool HilinkGetDevice ()
{
  return hilink_config.device;
}

bool HilinkIsActive () { return HilinkIsActive (HILINK_JSON_GENERAL); }
bool HilinkIsActive (const uint8_t context)
{
  bool    result   = false;
  bool    presence = false;
  bool    motion   = false;
  uint8_t index;

  // check for presence
  for (index = 0; index < HILINK_MAX_PRESENCE; index ++)
    if (hilink_static.arr_target[index].active && (hilink_static.arr_target[index].dist >= hilink_config.dist_min) && (hilink_static.arr_target[index].dist <= hilink_config.pres_max)) presence = true;

  // check for motion
  for (index = 0; index < HILINK_MAX_MOTION; index ++)
    if (hilink_motion.arr_target[index].active && (hilink_motion.arr_target[index].dist >= hilink_config.dist_min) && (hilink_motion.arr_target[index].dist <= hilink_config.move_max)) motion = true;

  // set result according to context
  switch (context)
  {
    case HILINK_JSON_PRESENCE: result = presence;           break;
    case HILINK_JSON_MOTION:   result = motion;             break;
    case HILINK_JSON_GENERAL:  result = presence || motion; break;
  }
  
  return result;
}

uint16_t HilinkGetDistance () { return HilinkGetDistance (HILINK_JSON_GENERAL); }
uint16_t HilinkGetDistance (const uint8_t context)
{
  uint16_t result, presence, motion;
  uint8_t  index;

  // check for presence distance
  presence = UINT16_MAX;
  for (index = 0; index < HILINK_MAX_PRESENCE; index ++)
    if (hilink_static.arr_target[index].active) presence = min (presence, hilink_static.arr_target[index].dist);

  // check for motion distance
  motion = UINT16_MAX;
  for (index = 0; index < HILINK_MAX_MOTION; index ++)
    if (hilink_motion.arr_target[index].active) motion = min (motion, hilink_motion.arr_target[index].dist);

  // set result according to context
  result = UINT16_MAX;
  switch (context)
  {
    case HILINK_JSON_PRESENCE: result = presence;               break;
    case HILINK_JSON_MOTION:   result = motion;                 break;
    case HILINK_JSON_GENERAL:  result = min (presence, motion); break;
  }
  if (result == UINT16_MAX) result = 0;
  
  return result;
}

uint8_t HilinkGetPower () { return HilinkGetPower (HILINK_JSON_GENERAL); }
uint8_t HilinkGetPower (const uint8_t context)
{
  uint8_t result, presence, motion;
  uint8_t index;

  // check for presence distance
  presence = 0;
  for (index = 0; index < HILINK_MAX_PRESENCE; index ++)
    if (hilink_static.arr_target[index].active) presence = max (presence, hilink_static.arr_target[index].power);

  // check for motion distance
  motion = 0;
  for (index = 0; index < HILINK_MAX_MOTION; index ++)
    if (hilink_motion.arr_target[index].active) motion = max (motion, hilink_motion.arr_target[index].power);

  // set result according to context
  result = 0;
  switch (context)
  {
    case HILINK_JSON_PRESENCE: result = presence;               break;
    case HILINK_JSON_MOTION:   result = motion;                 break;
    case HILINK_JSON_GENERAL:  result = max (presence, motion); break;
  }
  
  return result;
}

void HilinkLogHexa (const uint8_t *parr_hexa, const size_t size_hexa, const bool is_cmnd, const bool is_sent)
{
  size_t index;
  char   str_text[8];
  String str_log;

  // check parameters
  if ((parr_hexa == nullptr) || (size_hexa == 0)) return;

  // if not enabled, ignore
  if (hilink_config.param.log == HILINK_LOG_OFF) return;
  if (is_cmnd  && (hilink_config.param.log != HILINK_LOG_SENT) && (hilink_config.param.log != HILINK_LOG_ALL)) return;
  if (!is_cmnd && (hilink_config.param.log != HILINK_LOG_RECV) && (hilink_config.param.log != HILINK_LOG_ALL)) return;

  // loop to generate string
  str_log = "";
  for (index = 0; index < size_hexa; index ++)
  {
    sprintf_P (str_text, PSTR ("%02X "), parr_hexa[index]);
    str_log += str_text;
  }

  // log message
  if (!is_cmnd) strcpy_P (str_text, PSTR ("Data"));
    else if (is_sent) strcpy_P (str_text, PSTR ("Cmnd"));
    else strcpy_P (str_text, PSTR ("Answ"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s %s"), str_text, str_log.c_str ());
}

void HilinkLogText (const char *pstr_text, const bool is_sent)
{
  size_t index;
  char   str_text[8];

  // check parameters
  if (pstr_text == nullptr) return;

  // if not enabled, ignore
  if (hilink_config.param.log == HILINK_LOG_OFF) return;
  if (is_sent  && (hilink_config.param.log != HILINK_LOG_SENT) && (hilink_config.param.log != HILINK_LOG_ALL)) return;
  if (!is_sent && (hilink_config.param.log != HILINK_LOG_RECV) && (hilink_config.param.log != HILINK_LOG_ALL)) return;

  // log message
  if (is_sent) strcpy_P (str_text, PSTR ("Cmnd"));
    else strcpy_P (str_text, PSTR ("Data"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s %s"), str_text, pstr_text);
}

/****************************************\
 *         Commands management
\****************************************/

// set delay before next command (sec)
void HilinkSetCommandDelay (const uint32_t delay)
{
  // set next command timestamps
  hilink_command.ts_next = millis () + 1000 * delay;
}

// check if a command is possible
bool HilinkCommandPossible ()
{
  return (hilink_command.ts_next == UINT32_MAX);
}

// check if a command is waiting
bool HilinkCommandWaiting ()
{
  return (hilink_command.str_queue.length () > 0);
}

// declare current command as started
void HilinkCommandStarted (const uint8_t command, const uint8_t param)
{
  hilink_command.command = command;
  hilink_command.param   = param;
}

// declare current command as finished
void HilinkCommandFinished ()
{
  // reset reception buffer
  HilinkReceptionEmpty ();

  // set back data reception
  hilink_reception.data = true;

  // reset last command type and param
  hilink_command.command = UINT8_MAX;
  hilink_command.param   = 0;
}

// check if device is in command mode
bool HilinkIsInCommandMode ()
{
  return (hilink_command.mode);
}

// add text command
void HilinkAppendCommand (const char* pstr_command)
{
  if (pstr_command == nullptr) return;
  if (hilink_command.str_queue.length () > 0) hilink_command.str_queue += ';';
  hilink_command.str_queue += pstr_command;
}

// add basic numerical command
void HilinkAppendCommand (const uint8_t command)
{
  // add command
  if (hilink_command.str_queue.length () > 0) hilink_command.str_queue += ';';
  hilink_command.str_queue += command;
}

// add numerical command with numerical parameter
void HilinkAppendCommand (const uint8_t command, const int param)
{
  // append command
  if (hilink_command.str_queue.length () > 0) hilink_command.str_queue += ';';
  hilink_command.str_queue += command;
  hilink_command.str_queue += '.';
  hilink_command.str_queue += param;
}

// add numerical command with numerical parameter and complementary value
void HilinkAppendCommand (const uint8_t command, const uint8_t param, const long value)
{
  // append command
  if (hilink_command.str_queue.length () > 0) hilink_command.str_queue += ';';
  hilink_command.str_queue += command;
  hilink_command.str_queue += '.';
  hilink_command.str_queue += param;
  hilink_command.str_queue += ',';
  hilink_command.str_queue += value;
}

// add numerical command with text parameter
void HilinkAppendCommand (const uint8_t command, const char* pstr_param)
{
  if (pstr_param == nullptr) return;
  if (hilink_command.str_queue.length () > 0) hilink_command.str_queue += ';';
  hilink_command.str_queue += command;
  hilink_command.str_queue += '.';
  hilink_command.str_queue += pstr_param;
}

bool HilinkGetNextCommand (char* pstr_command, const size_t size_command)
{
  bool   result;
  size_t index;

  // check parameters
  if (pstr_command == nullptr) return false;
  if (hilink_command.ts_next != UINT32_MAX) return false;

  // init
  result = false;
  *pstr_command = 0;

  // if no command in the pipe
  result = (hilink_command.str_queue.length () > 0);
  if (result) 
  {
    // look for next separator
    index = hilink_command.str_queue.indexOf (';');

    // if last command
    if (index == -1)
    {
      strlcpy (pstr_command, hilink_command.str_queue.c_str (), size_command);
      hilink_command.str_queue = "";
    }

    // else, some more commands to come
    else
    {
      strlcpy (pstr_command, hilink_command.str_queue.c_str (), min (index + 1, size_command));
      hilink_command.str_queue = hilink_command.str_queue.substring (index + 1);
    }
  }

  return result;
}

uint8_t HilinkGetNextCommand ()
{
  bool    present;
  uint8_t result = UINT8_MAX;
  char    str_value[8];

  // get next command
  present = HilinkGetNextCommand (str_value, sizeof (str_value));
  if (present) result = (uint8_t)atoi (str_value);

  return result;
}

void HilinkSplitCommand (char* pstr_command, uint8_t &command, char* pstr_param, size_t size_param)
{
  char *pstr_digit;

  // check parameters
  if (pstr_command == nullptr) return;
  if (pstr_param == nullptr) return;

  // look for digit separator
  pstr_digit = strchr (pstr_command, '.');
  if (pstr_digit != nullptr) *pstr_digit++ = 0;

  // extract command and param
  command = (uint8_t)atoi (pstr_command);
  if (pstr_digit != nullptr) strlcpy (pstr_param, pstr_digit, size_param);
    else pstr_param[0] = 0;
}

void HilinkSplitCommand (char* pstr_command, uint8_t &command, uint8_t &param)
{
  char *pstr_digit;

  // check parameters
  if (pstr_command == nullptr) return;

  // look for digit separator
  pstr_digit = strchr (pstr_command, '.');
  if (pstr_digit != nullptr) *pstr_digit++ = 0;

  // extract command and param
  command = (uint8_t)atoi (pstr_command);
  if (pstr_digit != nullptr) param = (uint8_t)atoi (pstr_digit);
    else param = 0;
}

void HilinkReceptionEmpty ()
{
  hilink_reception.since    = 0;
  hilink_reception.idx_body = 0;
  memset (hilink_reception.arr_body, 0, HILINK_MSG_SIZE_MAX);
}

void HilinkReceptionAppend (const uint8_t caracter)
{
  // fix watchdogs
  if (hilink_reception.idx_body % 20 == 0) yield ();

  // if buffer is full, empty it and then append character
  if (hilink_reception.idx_body >= HILINK_MSG_SIZE_MAX) HilinkReceptionEmpty ();
  hilink_reception.arr_body[hilink_reception.idx_body++] = caracter;
}

/**************************************************\
 *                  Callback
\**************************************************/

void HilinkInit () 
{
  bool    result = false;
  uint8_t index;

  // init MAC
  hilink_status.str_mac[0]   = 0;
  hilink_status.str_model[0] = 0;

  // init gate threshold and power
  for (index = 0; index < HILINK_MAX_GATE; index ++)
  {
    hilink_static.arr_gate[index].threshold = 0;
    hilink_static.arr_gate[index].power     = 0;
    hilink_motion.arr_gate[index].threshold = 0;
    hilink_motion.arr_gate[index].power     = 0;
  }

  // init presence targets
  for (index = 0; index < HILINK_MAX_PRESENCE; index ++)
  {
    hilink_static.arr_target[index].active = false;
    hilink_static.arr_target[index].power  = 100;         // max power if not handled by device
    hilink_static.arr_target[index].dist   = 0;
    hilink_static.arr_target[index].speed  = 0;
    hilink_static.arr_target[index].x      = 0;
    hilink_static.arr_target[index].y      = 0;
  }

  // init motion targets
  for (index = 0; index < HILINK_MAX_MOTION; index ++)
  {
    hilink_motion.arr_target[index].active = false;
    hilink_motion.arr_target[index].power  = 100;         // max power if not handled by device
    hilink_motion.arr_target[index].dist   = 0;
    hilink_motion.arr_target[index].speed  = 0;
    hilink_motion.arr_target[index].x      = 0;
    hilink_motion.arr_target[index].y      = 0;
  }

  // init detection zones
  for (index = 0; index < HILINK_MAX_ZONE; index ++)
  {
    hilink_zone.arr_zone[index].x1 = 0;
    hilink_zone.arr_zone[index].y1 = 0;
    hilink_zone.arr_zone[index].x2 = 0;
    hilink_zone.arr_zone[index].y2 = 0;
  }

  // empty command and reception buffer
  hilink_command.str_queue = "";
  HilinkReceptionEmpty ();

  // load config
  HilinkLoadConfig ();

  // log device type
  GetTextIndexed (hilink_status.str_model, sizeof (hilink_status.str_model), hilink_config.device, kHilinkModel);
  AddLog (LOG_LEVEL_INFO, PSTR ("HLK: Detector is %s"), hilink_status.str_model);

  // log help
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Run hlk to get help on hilink detector commands"));

  // check Rx2410/Tx2410 and init according to device configured
  if ((hilink_config.device != HLK_DEVICE_NONE) && PinUsed (HILINK_GPIO_RX) && PinUsed (HILINK_GPIO_TX))
  {
    // init device data
    HilinkDeviceCommand (HILINK_CMND_INIT, 0);

    // create serial port
    hilink_status.pserial = new TasmotaSerial (Pin (HILINK_GPIO_RX), Pin (HILINK_GPIO_TX), TasmotaGlobal.seriallog_level ? 1 : 2);

    // initialise serial port
    result = hilink_status.pserial->begin (hilink_status.baudrate, hilink_status.baudmode);

#ifdef ESP8266
    // force hardware configuration on ESP8266
    if (hilink_status.pserial->hardwareSerial ()) ClaimSerial ();
#endif      // ESP8266

    // send intial commands
    if (result)
    {
      // flush serial port
      hilink_status.pserial->flush ();

      // append initialisation commands
      HilinkDeviceCommand (HILINK_CMND_FIRST, 0);
    }
  }

  // log
  if (result) AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s init at %u"), hilink_status.str_model,hilink_status.baudrate);
    else AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s init failed"), hilink_status.str_model);
}

void HilinkEvery100ms ()
{
  // if needed, check for next command timeout
  if ((hilink_command.ts_next != UINT32_MAX) && (TimeDifference (hilink_command.ts_next, millis ()) >= 0)) hilink_command.ts_next = UINT32_MAX;
}

void HilinkEverySecond ()
{
  // increment delay since last reception
  hilink_reception.since++;
}

// Show JSON status (for MQTT)
//   "Hlk":{"Model":"LD2450","Detect":1,"Target":3,"Static":{"Detect":1,"Delay":"now"},"Motion":{"Detect":0,"Delay":"now"},...}
void HilinkShowJSON ()
{
  bool status;

  // start of hilink section
  MiscOptionPrepareJsonSection ();
  ResponseAppend_P (PSTR ("\"Hlk\":{"));

  // detector model
  ResponseAppend_P (PSTR ("\"Model\":\"%s\",\"Detect\":%u"), hilink_status.str_model, HilinkIsActive (HILINK_JSON_GENERAL));
  HilinkDeviceCommand (HILINK_CMND_JSON, HILINK_JSON_GENERAL);

  // presence
  ResponseAppend_P (PSTR (",\"Static\":{\"Detect\":%u"), HilinkIsActive (HILINK_JSON_PRESENCE));
  HilinkDeviceCommand (HILINK_CMND_JSON, HILINK_JSON_PRESENCE);
  ResponseAppend_P (PSTR ("}"));

  // motion
  ResponseAppend_P (PSTR (",\"Motion\":{\"Detect\":%u"), HilinkIsActive (HILINK_JSON_MOTION));
  HilinkDeviceCommand (HILINK_CMND_JSON, HILINK_JSON_MOTION);
  ResponseAppend_P (PSTR ("}"));

  // end of general section
  ResponseAppend_P (PSTR ("}"));
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// Append Hilink sensor data to main page
void HilinkWebSensor ()
{
  uint32_t index, value, limit, unit, width, total;
  char     str_text[2] = {0, 0};

  // check if enabled
  if (hilink_config.device == HLK_DEVICE_NONE) return;
  if (hilink_status.pserial == nullptr) return;

  // section display
  WSContentSend_P (PSTR ("<div style='font-size:10px;text-align:center;margin:4px 0px;padding:2px 6px 8px 6px;background:#333333;border-radius:8px;'>\n"));

  // -----------------
  //  model & targets
  // -----------------

  WSContentSend_P (PSTR ("<div class='hlk'>\n"));
  WSContentSend_P (PSTR ("<div class='head' title='firmware %s'>%s</div>"), hilink_status.str_firmware.c_str (), hilink_status.str_model);
  WSContentSend_P (PSTR ("<div class='zone'>\n"));

  // presence targets
  for (index = 0; index < HILINK_MAX_PRESENCE; index ++)
    if (hilink_static.arr_target[index].active)
    {
      value = 23 + 70 * (uint32_t)hilink_static.arr_target[index].dist / (uint32_t)hilink_status.dist_limit;
      WSContentSend_P (PSTR ("<div class='dot pres' style='left:%u%%;'>%u</div>\n"), value, index + 1);
    }

  // motion targets
  for (index = 0; index < HILINK_MAX_MOTION; index ++)
    if (hilink_motion.arr_target[index].active)
    {
      value = 23 + 70 * (uint32_t)hilink_motion.arr_target[index].dist / (uint32_t)hilink_status.dist_limit;
      WSContentSend_P (PSTR ("<div class='dot move' style='left:%u%%;'>%u</div>\n"), value, index + 1);
    }

  WSContentSend_P (PSTR ("</div>\n"));
  WSContentSend_P (PSTR ("</div>\n"));

  // -----------------
  //       info
  // -----------------
  
  WSContentSend_P (PSTR ("<div class='hlk hlkl'>\n"));

  // head
  WSContentSend_P (PSTR ("<div class='head'>"));
  if (hilink_config.param.bluetooth == 1) WSContentSend_P (PSTR ("<span class='bt' title='MAC %s'>B</span>"), hilink_status.str_mac);
  if (hilink_status.char_green  != 0) { str_text[0] = hilink_status.char_green;  WSContentSend_P (PSTR ("<span class='green'>%s</span>"),  str_text); }
  if (hilink_status.char_yellow != 0) { str_text[0] = hilink_status.char_yellow; WSContentSend_P (PSTR ("<span class='yellow'>%s</span>"), str_text); }
  if (hilink_status.outdated) WSContentSend_P (PSTR ("<span class='err' title='Firmware %s is old ! You should update it'>F</span>"), hilink_status.str_firmware.c_str ());
  if (hilink_reception.since > 5) WSContentSend_P (PSTR ("<span class='err' title='No reception from sensor'>!</span>"));
  WSContentSend_P (PSTR ("</div>\n"));

  // -----------------
  //      range
  // -----------------

  // calculate unit size, number of values to display and display width
  limit = (uint32_t)hilink_status.dist_limit;
  if (limit >= 1200) unit = 3;
    else if (limit >= 800) unit = 2;
    else unit = 1;
  value = limit / (unit * 100);
  width = unit * 78 * 100 * 100 / limit;
  total = 22 * 100;

  // range
  WSContentSend_P (PSTR ("<div class='range first' style='width:%u.%02u%%;'>|</div>\n"), width / 2 / 100, width / 2 % 100);
  total += width / 2;
  for (index = 1; index < value; index ++) 
  {
    WSContentSend_P (PSTR ("<div class='range' style='width:%u.%02u%%;'>%um</div>\n"), width / 100, width % 100, index * unit);
    total += width;
  }
  width = 100 * 100 - total;
  WSContentSend_P (PSTR ("<div class='range last' style='width:%u.%02u%%;'>|</div>\n"), width / 100, width % 100);
  
  WSContentSend_P (PSTR ("</div>\n"));

  // end of display
  WSContentSend_P (PSTR ("</div>\n"));
}

void HilinkWebMainButton ()
{
  char str_link[12];

  // check if enabled
  if (hilink_config.device == HLK_DEVICE_NONE) return;
  if (hilink_status.pserial == nullptr) return;

  // display style
  WSContentSend_P (PSTR ("<style>\n"));

  WSContentSend_P (PSTR (".hlk{display:flex;padding:0px;margin:0px;}\n"));
  WSContentSend_P (PSTR (".hlk div{padding:0px;text-align:center;margin:0px;}\n"));
  WSContentSend_P (PSTR (".hlkl{margin-top:2px;margin-bottom:-4px;}\n"));

  WSContentSend_P (PSTR (".hlk .head{width:22%%;text-align:left;font-size:12px;font-weight:bold;}\n"));
  WSContentSend_P (PSTR (".hlk .zone{width:78%%;background:#252525;margin-top:3px;border-radius:4px;}\n"));
  WSContentSend_P (PSTR (".hlk .dot{position:absolute;width:16px;padding:1px;font-weight:bold;border-radius:50%%;}\n"));
  WSContentSend_P (PSTR (".hlk .pres{background:%s;}\n"), PSTR_HILINK_COLOR_PRESENCE);
  WSContentSend_P (PSTR (".hlk .move{background:%s;}\n"), PSTR_HILINK_COLOR_MOTION);
  WSContentSend_P (PSTR (".hlk .head span{margin-left:4px;padding:0px 4px;border-radius:4px;font-size:10px;font-weight:normal;}\n"));
  WSContentSend_P (PSTR (".hlk .head span.yellow{background:%s;}\n"), PSTR_HILINK_COLOR_YELLOW);
  WSContentSend_P (PSTR (".hlk .head span.green{background:%s;}\n"),  PSTR_HILINK_COLOR_GREEN);
  WSContentSend_P (PSTR (".hlk .head span.bt{background:%s;}\n"),     PSTR_HILINK_COLOR_BLUE);
  WSContentSend_P (PSTR (".hlk .head span.warn{background:%s;}\n"),   PSTR_HILINK_COLOR_WARNING);
  WSContentSend_P (PSTR (".hlk .head span.err{background:%s;}\n"),    PSTR_HILINK_COLOR_ERROR);
  WSContentSend_P (PSTR (".hlk .range{text-align:center;color:#aaa;margin-top:3px;}\n"));
  WSContentSend_P (PSTR (".hlk .first{text-align:left;}\n"));
  WSContentSend_P (PSTR (".hlk .last{text-align:right;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // display radar button
  (hilink_status.radar2D) ? strcpy_P (str_link, PSTR_HILINK_PAGE_RADAR2D) : strcpy_P (str_link, PSTR_HILINK_PAGE_RADAR);
  WSContentSend_P (PSTR ("<p><a href='%s'><button>Detector Map</button></a></p>\n"), str_link);
}

bool HilinkPresenceGetTarget (const uint8_t index, uint16_t &distance, uint8_t &power)
{
  bool is_valid = false;

  // init
  distance = 0;
  power    = 0;

  // if boundaries are ok, check for validity
  if (index < HILINK_MAX_PRESENCE)
  {
    is_valid  = hilink_static.arr_target[index].active;
    is_valid &= (hilink_static.arr_target[index].dist >= hilink_config.dist_min);
    is_valid &= (hilink_static.arr_target[index].dist <= hilink_config.pres_max);
  }

  // if valid, set data
  if (is_valid)
  {
    distance = hilink_static.arr_target[index].dist;
    power    = hilink_static.arr_target[index].power;
  }

  return is_valid;
}

bool HilinkMotionGetTarget (const uint8_t index, uint16_t &distance, uint8_t &power)
{
  bool is_valid = false;

  // init
  distance = 0;
  power    = 0;

  // if boundaries are ok, check for validity
  if (index < HILINK_MAX_MOTION)
  {
    is_valid  = hilink_motion.arr_target[index].active;
    is_valid &= (hilink_motion.arr_target[index].dist >= hilink_config.dist_min);
    is_valid &= (hilink_motion.arr_target[index].dist <= hilink_config.move_max);
  }

  // if valid, set data
  if (is_valid)
  {
    distance = hilink_motion.arr_target[index].dist;
    power    = hilink_motion.arr_target[index].power;
  }

  return is_valid;
}

/**********************\
 *     Linear Radar
\**********************/

void HilinkGraphRadarJS ()
{
  // start page as Javascript with cache enabled
  WSContentBeginJavascript (200, 0);

  WSContentSend_P (PSTR ("function updateData(){\n"));

  WSContentSend_P (PSTR (" httpData=new XMLHttpRequest();\n"));
  WSContentSend_P (PSTR (" httpData.open('GET','%s',true);\n"), PSTR_HILINK_PAGE_RADAR_UPD);

  WSContentSend_P (PSTR (" httpData.onreadystatechange=function(){\n"));
  WSContentSend_P (PSTR ("  if (httpData.readyState===XMLHttpRequest.DONE){\n"));
  WSContentSend_P (PSTR ("   if (httpData.status===0 || (httpData.status>=200 && httpData.status<400)){\n"));
  WSContentSend_P (PSTR ("    let arr_param=httpData.responseText.split('\\n');\n"));

  WSContentSend_P (PSTR ("    for (i=0; i<arr_param.length; i++) {\n"));
  WSContentSend_P (PSTR ("     arr_value=arr_param[i].split(';');\n"));

  WSContentSend_P (PSTR ("     if (arr_value[0]==='act'){\n"));
  WSContentSend_P (PSTR ("      document.getElementById(arr_value[1]).setAttribute('y',arr_value[2]);\n"));
  WSContentSend_P (PSTR ("      document.getElementById(arr_value[1]).setAttribute('height',arr_value[3]);\n"));
  WSContentSend_P (PSTR ("     }\n"));

  WSContentSend_P (PSTR ("     else if (arr_value[0]==='dot'){\n"));
  WSContentSend_P (PSTR ("      document.getElementById(arr_value[1]).classList.remove('off','pr','mv');\n"));
  WSContentSend_P (PSTR ("      document.getElementById(arr_value[1]).classList.add(arr_value[2]);\n"));
  WSContentSend_P (PSTR ("      document.getElementById(arr_value[1]).setAttribute('cy',arr_value[3]);\n"));
  WSContentSend_P (PSTR ("      document.getElementById(arr_value[1]).setAttribute('r',arr_value[4]);\n"));
  WSContentSend_P (PSTR ("     }\n"));
 
  WSContentSend_P (PSTR ("     else if (arr_value[0]==='txt'){\n"));
  WSContentSend_P (PSTR ("      document.getElementById(arr_value[1]).setAttribute('y',arr_value[2]);\n"));
  WSContentSend_P (PSTR ("      document.getElementById(arr_value[1]).textContent=arr_value[3];\n"));
  WSContentSend_P (PSTR ("     }\n"));

  WSContentSend_P (PSTR ("     else if (arr_value[0]==='gate'){\n"));
  WSContentSend_P (PSTR ("      document.getElementById(arr_value[1]).classList.remove('off','pgt','mgt');\n"));
  WSContentSend_P (PSTR ("      document.getElementById(arr_value[1]).classList.add(arr_value[2]);\n"));
  WSContentSend_P (PSTR ("     }\n"));

  WSContentSend_P (PSTR ("     else if (arr_value[0]==='label'){\n"));
  WSContentSend_P (PSTR ("      document.getElementById(arr_value[1]).textContent=arr_value[2];\n"));
  WSContentSend_P (PSTR ("     }\n"));

  WSContentSend_P (PSTR ("     else if (arr_value[0]==='power'){\n"));
  WSContentSend_P (PSTR ("      document.getElementById(arr_value[1]).classList.remove('off','ppw','mpw');\n"));
  WSContentSend_P (PSTR ("      document.getElementById(arr_value[1]).classList.add(arr_value[2]);\n"));
  WSContentSend_P (PSTR ("      document.getElementById(arr_value[1]).setAttribute('x',arr_value[3]);\n"));
  WSContentSend_P (PSTR ("      document.getElementById(arr_value[1]).setAttribute('width',arr_value[4]);\n"));
  WSContentSend_P (PSTR ("     }\n"));

  WSContentSend_P (PSTR ("     else if (arr_value[0]==='thres'){\n"));
  WSContentSend_P (PSTR ("      document.getElementById(arr_value[1]).classList.remove('off','pth','mth');\n"));
  WSContentSend_P (PSTR ("      document.getElementById(arr_value[1]).classList.add(arr_value[2]);\n"));
  WSContentSend_P (PSTR ("      document.getElementById(arr_value[1]).setAttribute('x',arr_value[3]);\n"));
  WSContentSend_P (PSTR ("     }\n"));

  WSContentSend_P (PSTR ("    }\n"));

  WSContentSend_P (PSTR ("   }\n"));
  WSContentSend_P (PSTR ("   setTimeout(updateData,%u);\n"), 500);               // ask for update twice every sec
  WSContentSend_P (PSTR ("  }\n"));
  WSContentSend_P (PSTR (" }\n"));

  WSContentSend_P (PSTR (" httpData.send();\n"));
  WSContentSend_P (PSTR ("}\n"));

  WSContentSend_P (PSTR ("setTimeout(updateData,%u);\n"), 100);                 // ask for first update after 100ms

  WSContentEnd ();
}

void HilinkGraphRadarCSS ()
{
  // start page as CSS with cache enabled
  WSContentBeginCSS (200, 0);

  // --------
  //   Page
  // --------

  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));

  WSContentSend_P (PSTR ("a {color:white;}\n"));
  WSContentSend_P (PSTR ("a:link {text-decoration:none;}\n"));

  WSContentSend_P (PSTR ("div {padding:0px;margin:0px;text-align:center;}\n"));
  WSContentSend_P (PSTR ("div.title {font-size:4vh;font-weight:bold;}\n"));

  WSContentSend_P (PSTR ("div.graph {width:100%%;margin:4vh auto;}\n"));
  WSContentSend_P (PSTR ("svg.graph {max-width:600px;}\n"));

  WSContentSend_P (PSTR ("button.menu {background:%s;color:%s;line-height:2rem;font-size:1.2rem;cursor:pointer;border:none;border-radius:0.3rem;width:150px;margin:2vh;}\n"), COLOR_BUTTON, COLOR_BUTTON_TEXT);

  // --------
  //   SVG
  // --------

  WSContentSend_P (PSTR ("svg rect.dot {fill:none;stroke:#aaa;stroke-dasharray:2;}\n"));
  WSContentSend_P (PSTR ("svg rect.act {fill:#040;stroke:none;opacity:0.6;}\n"));

  WSContentSend_P (PSTR ("svg rect.off {fill:none;stroke:none;}\n"));
  WSContentSend_P (PSTR ("svg rect.out {fill:%s;}\n"),                         PSTR (COLOR_BACKGROUND));
  WSContentSend_P (PSTR ("svg rect.pgt {fill:%s;opacity:0.2;}\n"),             PSTR_HILINK_COLOR_PRESENCE);
  WSContentSend_P (PSTR ("svg rect.pth {fill:%s;stroke:none;}\n"),             PSTR_HILINK_COLOR_PRESENCE);
  WSContentSend_P (PSTR ("svg rect.ppw {fill:%s;stroke:none;opacity:0.6;}\n"), PSTR_HILINK_COLOR_PRESENCE);
  WSContentSend_P (PSTR ("svg rect.mgt {fill:%s;opacity:0.2;}\n"),             PSTR_HILINK_COLOR_MOTION);
  WSContentSend_P (PSTR ("svg rect.mth {fill:%s;stroke:none;}\n"),             PSTR_HILINK_COLOR_MOTION);
  WSContentSend_P (PSTR ("svg rect.mpw {fill:%s;stroke:none;opacity:0.6;}\n"), PSTR_HILINK_COLOR_MOTION);

  WSContentSend_P (PSTR ("svg circle {opacity:0.8;}\n"));
  WSContentSend_P (PSTR ("svg circle.off {fill:none;}\n"));
  WSContentSend_P (PSTR ("svg circle.pr {fill:%s;}\n"), PSTR_HILINK_COLOR_PRESENCE);
  WSContentSend_P (PSTR ("svg circle.mv {fill:%s;}\n"), PSTR_HILINK_COLOR_MOTION);

  WSContentSend_P (PSTR ("svg text {font-size:10px;fill:#888;text-anchor:middle;}\n"));
  WSContentSend_P (PSTR ("svg text.scale {fill:#aaa;dominant-baseline:middle;}\n"));
  WSContentSend_P (PSTR ("svg text.dot {fill:#fff;}\n"));

  WSContentEnd ();
}

// Radar page
void HilinkGraphRadar ()
{
  uint32_t index, gate, quantity;
  uint32_t x, y, y_text, w, h, h_gate;

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess ()) return;
  
  // set page label
  WSContentStart_P (PSTR ("Radar"), true);
  WSContentSend_P (PSTR ("\n</script>\n"));

  // set page as scalable
  WSContentSend_P (PSTR ("<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=yes'/>\n"));

  // load javascript
  WSContentSend_P (PSTR ("<script type='text/javascript' src='%s?ts=%u'></script>\n"), PSTR_HILINK_PAGE_RADAR_JS, Rtc.restart_time);

  // load style sheet
  WSContentSend_P (PSTR ("<link rel='stylesheet' type='text/css' href='%s?ts=%u'>\n"), PSTR_HILINK_PAGE_RADAR_CSS, Rtc.restart_time);

  // page body
  WSContentSend_P (PSTR ("</head>\n"));
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  // room name
  WSContentSend_P (PSTR ("<div class='title'><a href='/'>%s</a></div>\n"), SettingsText(SET_DEVICENAME));

  // ------- Graph --------

  // start of radar
  WSContentSend_P (PSTR ("<div class='graph'>\n"));
  WSContentSend_P (PSTR ("<svg class='graph' viewBox='0 0 %u %u'>\n"), 400, 400);

  // -------------
  //     gates
  // -------------

  // gate height
  quantity = (uint32_t)hilink_status.gate_qty;
  h_gate   = 400 / quantity;

  // detection zone
  WSContentSend_P (PSTR ("<rect class='dot' x=160 y=0 width=80 height=400 rx=6 ry=6></rect>\n"));
  WSContentSend_P (PSTR ("<rect class='act' id='act' x=160 y=0 width=80 height=0></rect>\n"));

  // loop thru gates
  for (gate = 0; gate < quantity; gate ++)
  {
    // vertical start
    y      = gate * h_gate;
    y_text = y + 17 + (h_gate - 25) / 2;

    // presence gates
    WSContentSend_P (PSTR ("<rect class='off' id='pgt%u' x=%u y=%u width=%u height=%u rx=6 ry=6 />\n"), gate, 0, y + 2, 150, h_gate - 4);
    WSContentSend_P (PSTR ("<text id='pla%u' x=%u y=%u></text>\n"), gate, 125, y_text);
    WSContentSend_P (PSTR ("<rect class='%s' x=%u y=%u width=%u height=%u rx=4 ry=4></rect>\n"), PSTR ("out"), 2, y + 4, 100, h_gate - 8);
    WSContentSend_P (PSTR ("<rect class='off' id='ppw%u' x=%u y=%u width=%u height=%u rx=4 ry=4></rect>\n"), gate, 102 - hilink_static.arr_gate[gate].power, y + 4, hilink_static.arr_gate[gate].power, h_gate - 8);
    WSContentSend_P (PSTR ("<rect class='off' id='pth%u' x=%u y=%u width=%u height=%u rx=2 ry=2></rect>\n"), gate, 102 - hilink_static.arr_gate[gate].threshold, y + 8, 2, h_gate - 16);

    // motion gates
    WSContentSend_P (PSTR ("<rect class='off' id='mgt%u' x=%u y=%u width=%u height=%u rx=6 ry=6 />\n"), gate, 250, y + 2, 150, h_gate - 4);
    WSContentSend_P (PSTR ("<text id='mla%u' x=%u y=%u></text>\n"), gate, 275, y_text);
    WSContentSend_P (PSTR ("<rect class='out' x=%u y=%u width=%u height=%u rx=4 ry=4></rect>\n"), 298, y + 4, 100, h_gate - 8);
    WSContentSend_P (PSTR ("<rect class='off' id='mpw%u' x=%u y=%u width=%u height=%u rx=4 ry=4></rect>\n"), gate, 298, y + 4, hilink_motion.arr_gate[gate].power, h_gate - 8);
    WSContentSend_P (PSTR ("<rect class='off' id='mth%u' x=%u y=%u width=%u height=%u rx=2 ry=2></rect>\n"), gate, 298 + hilink_motion.arr_gate[gate].threshold, y + 8, 2, h_gate - 16);
  }

  // ------------------
  //   distance scale
  // ------------------

  // loop 1m gates
  gate = (uint32_t)hilink_status.dist_limit / 100;
  for (index = 1; index <= gate; index ++) 
    if ((gate <= 8) || (index % 2 == 0))
    {
      y = 400 * 100 * (uint32_t)index / (uint32_t)hilink_status.dist_limit;
      WSContentSend_P (PSTR ("<text class='scale' x=%u y=%u>%u m</text>\n"), 200, y, index);
    }

  // -------------------------------
  //   presence and motion circles
  // -------------------------------

  for (index = 0; index < HILINK_MAX_PRESENCE; index ++)
  {
    WSContentSend_P (PSTR ("<circle id='pd%u' class='off' cx=%u cy=%u r=6></circle>\n"), index, 200, 0);
    WSContentSend_P (PSTR ("<text id='pt%u' class='dot' x=%u y=%u></text>\n"), index, 200, 0);
  }

  for (index = 0; index < HILINK_MAX_MOTION; index ++)
  {
    WSContentSend_P (PSTR ("<circle id='md%u' class='off' cx=%u cy=%u r=6></circle>\n"), index, 200, 0);
    WSContentSend_P (PSTR ("<text id='mt%u' class='dot' x=%u y=%u></text>\n"), index, 200, 0);
  }

  // end of radar
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentSend_P (PSTR ("</div>\n"));

  // main menu button
  WSContentSend_P (PSTR ("<div><form action='/' method='get'><button class='menu'>%s</button></form></div>\n"), D_MAIN_MENU);

  // end of page
  WSContentStop ();
}

// Radar update data
void HilinkGraphRadarUpdate ()
{
  uint8_t  index, power, r;
  uint16_t distance;
  uint32_t y, height;

  // start of update page
  WSContentBegin (200, CT_PLAIN);

  // ------------------
  //   detection zone
  // ------------------

  y = 400 * (uint32_t)hilink_config.dist_min / (uint32_t)hilink_status.dist_limit;
  height = 400 * (uint32_t)max (hilink_config.pres_max, hilink_config.move_max) / (uint32_t)hilink_status.dist_limit - y;
  WSContentSend_P (PSTR ("act;act;%u;%u\n"), y, height);

  // ------------
  //   targets
  // ------------

  // presence target
  for (index = 0; index < HILINK_MAX_PRESENCE; index ++)
  {
    // if target active, display target
    if (HilinkPresenceGetTarget (index, distance, power))
    {
      y = (uint32_t)distance * 400 / (uint32_t)hilink_status.dist_limit;
      r = 10 + power / 10;
      WSContentSend_P (PSTR ("dot;pd%u;%s;%u;%u\n"), index, PSTR ("pr"), y, r);
      WSContentSend_P (PSTR ("txt;pt%u;%u;%u%%\n"),  index, y + 3, power);
    }

    // else, hide target
    else 
    {
      WSContentSend_P (PSTR ("dot;pd%u;%s;%u;%u\n"), index, PSTR ("off"), 0, 1);
      WSContentSend_P (PSTR ("txt;pt%u;%u; \n"),     index, 0);
    }
  }

  // motion target
  for (index = 0; index < HILINK_MAX_MOTION; index ++)
  {
    // if target active, display target
    if (HilinkMotionGetTarget (index, distance, power))
    {
      y = (uint32_t)distance * 400 / (uint32_t)hilink_status.dist_limit;
      r = 10 + power / 10;
      WSContentSend_P (PSTR ("dot;md%u;%s;%u;%u\n"), index, PSTR ("mv"), y, r);
      WSContentSend_P (PSTR ("txt;mt%u;%u;%u%%\n"),  index, y + 3, power);
    }

    // else, hide target
    else 
    {
      WSContentSend_P (PSTR ("dot;md%u;%s;%u;%u\n"), index, PSTR ("off"), 0, 1);
      WSContentSend_P (PSTR ("txt;mt%u;%u; \n"),     index, 0);
    }
  }

  // ------------
  //    gates
  // ------------

  for (index = 0; index < hilink_status.gate_qty; index ++)
  {
    // presence gate
    if (hilink_static.enabled)
    {
      WSContentSend_P (PSTR ("gate;%s%u;%s\n"),        PSTR ("pgt"), index, PSTR ("pgt"));
      WSContentSend_P (PSTR ("label;%s%u;%s %u\n"),    PSTR ("pla"), index, PSTR ("Still"), index + 1);
      WSContentSend_P (PSTR ("thres;%s%u;%s;%u\n"),    PSTR ("pth"), index, PSTR ("pth"), 102 - hilink_static.arr_gate[index].threshold);
      WSContentSend_P (PSTR ("power;%s%u;%s;%u;%u\n"), PSTR ("ppw"), index, PSTR ("ppw"), 102 - hilink_static.arr_gate[index].power, hilink_static.arr_gate[index].power);
    }
    else 
    {
      WSContentSend_P (PSTR ("gate;%s%u;%s\n"),        PSTR ("pgt"), index, PSTR ("off"));
      WSContentSend_P (PSTR ("label;%s%u; \n"),        PSTR ("pla"), index);
      WSContentSend_P (PSTR ("thres;%s%u;%s;%u\n"),    PSTR ("pth"), index, PSTR ("off"), 102);
      WSContentSend_P (PSTR ("power;%s%u;%s;%u;%u\n"), PSTR ("ppw"), index, PSTR ("off"), 102, 0);
    }

    // motion gate
    if (hilink_motion.enabled)
    {
      WSContentSend_P (PSTR ("gate;%s%u;%s\n"),        PSTR ("mgt"), index, PSTR ("mgt"));
      WSContentSend_P (PSTR ("label;%s%u;%s %u\n"),    PSTR ("mla"), index, PSTR ("Move"), index + 1);
      WSContentSend_P (PSTR ("thres;%s%u;%s;%u\n"),    PSTR ("mth"), index, PSTR ("mth"), 298 + hilink_motion.arr_gate[index].threshold);
      WSContentSend_P (PSTR ("power;%s%u;%s;%u;%u\n"), PSTR ("mpw"), index, PSTR ("mpw"), 298, hilink_motion.arr_gate[index].power);
    }
    else 
    {
      WSContentSend_P (PSTR ("gate;%s%u;%s\n"),        PSTR ("mgt"), index, PSTR ("off"));
      WSContentSend_P (PSTR ("label;%s%u; \n"),        PSTR ("mla"), index);
      WSContentSend_P (PSTR ("thres;%s%u;%s;%u\n"),    PSTR ("mth"), index, PSTR ("off"), 298);
      WSContentSend_P (PSTR ("power;%s%u;%s;%u;%u\n"), PSTR ("mpw"), index, PSTR ("off"), 298, 0);
    }
  }
  
  // end of update page
  WSContentEnd ();
}

/********************\
 *     2D Radar
\********************/

void HilinkGraphRadar2dJS ()
{
  // start page as Javascript with cache enabled
  WSContentBeginJavascript (200, 0);

  WSContentSend_P (PSTR ("function updateData(){\n"));

  WSContentSend_P (PSTR (" httpData=new XMLHttpRequest();\n"));
  WSContentSend_P (PSTR (" httpData.open('GET','%s',true);\n"), PSTR_HILINK_PAGE_RADAR2D_UPD);

  WSContentSend_P (PSTR (" httpData.onreadystatechange=function(){\n"));
  WSContentSend_P (PSTR ("  if (httpData.readyState===XMLHttpRequest.DONE){\n"));
  WSContentSend_P (PSTR ("   if (httpData.status===0 || (httpData.status>=200 && httpData.status<400)){\n"));
  WSContentSend_P (PSTR ("    arr_param=httpData.responseText.split('\\n');\n"));
  WSContentSend_P (PSTR ("    for (i=0;i<%u;i++){\n"), hilink_zone.max);
  WSContentSend_P (PSTR ("     arr_value=arr_param[i].split(';');\n"));
  WSContentSend_P (PSTR ("     document.getElementById('z'+i).classList.remove('off','inc','exc');\n"));
  WSContentSend_P (PSTR ("     document.getElementById('z'+i).classList.add(arr_value[0]);\n"));
  WSContentSend_P (PSTR ("     document.getElementById('z'+i).setAttribute('x',arr_value[1]);\n"));
  WSContentSend_P (PSTR ("     document.getElementById('z'+i).setAttribute('y',arr_value[2]);\n"));
  WSContentSend_P (PSTR ("     document.getElementById('z'+i).setAttribute('width',arr_value[3]);\n"));
  WSContentSend_P (PSTR ("     document.getElementById('z'+i).setAttribute('height',arr_value[4]);\n"));
  WSContentSend_P (PSTR ("    }\n"));
  WSContentSend_P (PSTR ("    for (i=%u;i<%u;i++){\n"), hilink_zone.max, hilink_zone.max + HILINK_MAX_PRESENCE);
  WSContentSend_P (PSTR ("     arr_value=arr_param[i].split(';');\n"));
  WSContentSend_P (PSTR ("     document.getElementById('c'+i).classList.remove('off','on');\n"));
  WSContentSend_P (PSTR ("     document.getElementById('c'+i).classList.add(arr_value[0]);\n"));
  WSContentSend_P (PSTR ("     document.getElementById('c'+i).setAttribute('cx',arr_value[1]);\n"));
  WSContentSend_P (PSTR ("     document.getElementById('c'+i).setAttribute('cy',arr_value[2]);\n"));
  WSContentSend_P (PSTR ("     document.getElementById('t'+i).setAttribute('x',arr_value[1]);\n"));
  WSContentSend_P (PSTR ("     document.getElementById('t'+i).setAttribute('y',arr_value[3]);\n"));
  WSContentSend_P (PSTR ("     document.getElementById('t'+i).textContent=arr_value[4];\n"));
  WSContentSend_P (PSTR ("    }\n"));
  WSContentSend_P (PSTR ("   }\n"));
  WSContentSend_P (PSTR ("   setTimeout(updateData,%u);\n"), 1000);               // ask for update every 1 sec
  WSContentSend_P (PSTR ("  }\n"));
  WSContentSend_P (PSTR (" }\n"));

  WSContentSend_P (PSTR (" httpData.send();\n"));
  WSContentSend_P (PSTR ("}\n"));

  WSContentSend_P (PSTR ("setTimeout(updateData,%u);\n"), 100);                 // ask for first update after 100ms

  WSContentEnd ();
}

void HilinkGraphRadar2dCSS ()
{
  // start page as CSS with cache enabled
  WSContentBeginCSS (200, 0);

  // --------
  //   Page
  // --------

  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));

  WSContentSend_P (PSTR ("a {color:white;}\n"));
  WSContentSend_P (PSTR ("a:link {text-decoration:none;}\n"));

  WSContentSend_P (PSTR ("div {padding:0px;margin:0px;text-align:center;}\n"));
  WSContentSend_P (PSTR ("div.title {font-size:4vh;font-weight:bold;}\n"));

  WSContentSend_P (PSTR ("div.graph {width:100%%;margin:4vh auto;}\n"));
  WSContentSend_P (PSTR ("svg.graph {max-width:800px;}\n"));

  WSContentSend_P (PSTR ("button.menu {background:%s;color:%s;line-height:2rem;font-size:1.2rem;cursor:pointer;border:none;border-radius:0.3rem;width:150px;margin:2vh;}\n"), COLOR_BUTTON, COLOR_BUTTON_TEXT);

  // --------
  //   SVG
  // --------

  WSContentSend_P (PSTR ("svg path {stroke:green;stroke-dasharray:2 2;fill:none;}\n"));
  WSContentSend_P (PSTR ("svg circle {opacity:0.8;}\n"));
  WSContentSend_P (PSTR ("svg circle.off {fill:none;}\n"));
  WSContentSend_P (PSTR ("svg circle.on {fill:%s;}\n"), PSTR_HILINK_COLOR_MOTION);
  WSContentSend_P (PSTR ("svg text {font-size:16px;fill:#aaa;text-anchor:middle;}\n"));
  WSContentSend_P (PSTR ("svg rect {opacity:0.5;}\n"));
  WSContentSend_P (PSTR ("svg rect.off {fill:none;}\n"));
  WSContentSend_P (PSTR ("svg rect.inc {fill:#040;}\n"));
  WSContentSend_P (PSTR ("svg rect.exc {fill:#600;}\n"));

  WSContentEnd ();
}

// Radar page
void HilinkGraphRadar2d ()
{
  long index;
  long x, y, r;
  long cos[7] = {-1000, -866, -500, 0, 500, 866, 1000};
  long sin[7] = {0, 500, 866, 1000, 866, 500, 0};

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess ()) return;
  
  // set page label
  WSContentStart_P (PSTR ("Radar"), true);

  // page data refresh script
  WSContentSend_P (PSTR ("\n</script>\n"));

  // set page as scalable
  WSContentSend_P (PSTR ("<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=yes'/>\n"));

  // load javascript
  WSContentSend_P (PSTR ("<script type='text/javascript' src='%s?ts=%u'></script>\n"), PSTR_HILINK_PAGE_RADAR2D_JS, Rtc.restart_time);

  // load style sheet
  WSContentSend_P (PSTR ("<link rel='stylesheet' type='text/css' href='%s?ts=%u'>\n"), PSTR_HILINK_PAGE_RADAR2D_CSS, Rtc.restart_time);

  // page body
  WSContentSend_P (PSTR ("</head>\n"));
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  // room name
  WSContentSend_P (PSTR ("<div class='title'><a href='/'>%s</a></div>\n"), SettingsText(SET_DEVICENAME));

  // ------- Graph --------

  // start of radar
  WSContentSend_P (PSTR ("<div class='graph'>\n"));
  WSContentSend_P (PSTR ("<svg class='graph' viewBox='0 0 %u %u'>\n"), 800, 400 + 40);

  // detection zone
  for (index = 0; index < hilink_zone.max; index++) 
    WSContentSend_P (PSTR ("<rect id='z%d' class='off' x=%u y=%u width=%u height=%u rx=10 ry=10 />\n"), index, 0, 0, 0, 0);

  // radar frame lines (x5)
  for (index = 1; index < 6; index++) 
    WSContentSend_P (PSTR ("<path d='M %d %d L %d %d' />\n"), 400, 20, 400 + 400 * cos[index] / 1000, 20 + 400 * sin[index] / 1000);

  // radar frame circles and distance
  for (index = 1; index < 7; index++) 
  {
    x = index * sin[4] * 400 / 1000 / 6;
    y = index * cos[4] * 400 / 1000 / 6;
    r = index * 400 / 6;
    WSContentSend_P (PSTR ("<text x=%d y=%d>%dm</text>\n"),               405 + x, 7  + y, index);
    WSContentSend_P (PSTR ("<text x=%d y=%d>%dm</text>\n"),               395 - x, 7  + y, index);
    WSContentSend_P (PSTR ("<path d='M %d %d A %d %d 0 0 1 %d %d' />\n"), 400 + x, 20 + y, r, r, 400 - x, 20 + y);
  }

  // display targets
  for (index = 0; index < LD2450_MAX_TARGET; index++)
  {
    WSContentSend_P (PSTR ("<circle id='c%d' class='abs' cx=%u cy=%u r=14 />\n"), index + hilink_zone.max, 0, 20);
    WSContentSend_P (PSTR ("<text id='t%d' x=%u y=%u></text>\n"),                 index + hilink_zone.max, 0, 20);
  }

  // end of radar
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentSend_P (PSTR ("</div>\n"));

  // main menu button
  WSContentSend_P (PSTR ("<div><form action='/' method='get'><button class='menu'>%s</button></form></div>\n"), D_MAIN_MENU);

  // end of page
  WSContentStop ();
}

// Radar update
void HilinkGraphRadar2dUpdate ()
{
  uint8_t index;
  int32_t x, y, cx, cy, width, height;
  char    str_class[12];
  char    str_label[4];

  // start of update page
  WSContentBegin (200, CT_PLAIN);

  // loop thru zones
  GetTextIndexed (str_class, sizeof (str_class), hilink_zone.status, kHilinkZone);
  for (index = 0; index < hilink_zone.max; index++)
  {
    // calculate x and y coordonates
    x = 400 + (int32_t)hilink_zone.arr_zone[index].x1 * 400 / hilink_status.dist_limit;
    y = 20  + (int32_t)hilink_zone.arr_zone[index].y1 * 400 / hilink_status.dist_limit;

    // calculate width and height
    width  = (int32_t)(hilink_zone.arr_zone[index].x2 - hilink_zone.arr_zone[index].x1) * 400 / hilink_status.dist_limit;
    height = (int32_t)(hilink_zone.arr_zone[index].y2 - hilink_zone.arr_zone[index].y1) * 400 / hilink_status.dist_limit;

    // display target
    WSContentSend_P (PSTR ("%s;%d;%d;%d;%d\n"), str_class, x, y, width, height);
  }

  // loop thru targets
  for (index = 0; index < HILINK_MAX_MOTION; index++)
  {
    // calculate button style
    if (hilink_motion.arr_target[index].active)
    {
      cx = 400 - hilink_motion.arr_target[index].x * 400 / hilink_status.dist_limit;
      cy = 20  + hilink_motion.arr_target[index].y * 400 / hilink_status.dist_limit;
      itoa (index + 1, str_label, 10);
      strcpy_P (str_class, PSTR ("on"));
    }
    else 
    {
      cx = 0;
      cy = 0;
      str_label[0] = 0;
      strcpy_P (str_class, PSTR ("off"));
    }

    // display target
    WSContentSend_P (PSTR ("%s;%d;%d;%d;%s\n"), str_class, cx, cy, cy + 5, str_label);
  }

  // end of update page
  WSContentEnd ();
}

#endif      // USE_WEBSERVER

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xsns102 (const uint32_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
    case FUNC_INIT:
      HilinkInit ();
      break;

    case FUNC_COMMAND:
      result = DecodeCommand (kHilinkCommands, HilinkCommand);
      break;

    case FUNC_SAVE_SETTINGS:
      HilinkSaveConfig ();
      break;

    case FUNC_EVERY_100_MSECOND:
      HilinkEvery100ms ();
      break;

    case FUNC_EVERY_SECOND:
      HilinkEverySecond ();
      break;

    case FUNC_JSON_APPEND:
      if (TasmotaGlobal.tele_period == 0) HilinkShowJSON ();
      break;

#ifdef USE_WEBSERVER

    case FUNC_WEB_SENSOR:
      HilinkWebSensor ();
      break;

    case FUNC_WEB_ADD_MAIN_BUTTON:
      HilinkWebMainButton ();
      break;

    case FUNC_WEB_ADD_HANDLER:
      Webserver->on (FPSTR (PSTR_HILINK_PAGE_RADAR),       HilinkGraphRadar);
      Webserver->on (FPSTR (PSTR_HILINK_PAGE_RADAR_JS),    HilinkGraphRadarJS);
      Webserver->on (FPSTR (PSTR_HILINK_PAGE_RADAR_CSS),   HilinkGraphRadarCSS);
      Webserver->on (FPSTR (PSTR_HILINK_PAGE_RADAR_UPD),   HilinkGraphRadarUpdate);
      Webserver->on (FPSTR (PSTR_HILINK_PAGE_RADAR2D),     HilinkGraphRadar2d);
      Webserver->on (FPSTR (PSTR_HILINK_PAGE_RADAR2D_JS),  HilinkGraphRadar2dJS);
      Webserver->on (FPSTR (PSTR_HILINK_PAGE_RADAR2D_CSS), HilinkGraphRadar2dCSS);
      Webserver->on (FPSTR (PSTR_HILINK_PAGE_RADAR2D_UPD), HilinkGraphRadar2dUpdate);
      break;

#endif    // USE_WEBSERVER
  }

  if (!result)
  {
#ifdef USE_HILINK_LD1115
    if (hilink_config.device == HLK_DEVICE_LD1115) result = XsnsHilinkLD1115 (function);
#endif  // USE_HILINK_LD1115

#ifdef USE_HILINK_LD1125
    if (hilink_config.device == HLK_DEVICE_LD1125) result = XsnsHilinkLD1125 (function);
#endif  // USE_HILINK_LD1125

#ifdef USE_HILINK_LD2401
    if (hilink_config.device == HLK_DEVICE_LD2401) result = XsnsHilinkLD2401 (function);
#endif  // USE_HILINK_LD2401

#ifdef USE_HILINK_LD2402
    if (hilink_config.device == HLK_DEVICE_LD2402) result = XsnsHilinkLD2402 (function);
#endif  // USE_HILINK_LD2402

#ifdef USE_HILINK_LD2410
    if ((hilink_config.device == HLK_DEVICE_LD2410B) || (hilink_config.device == HLK_DEVICE_LD2410C)) result = XsnsHilinkLD2410 (function);
#endif  // USE_HILINK_LD2410

#ifdef USE_HILINK_LD2410S
    if (hilink_config.device == HLK_DEVICE_LD2410S) result = XsnsHilinkLD2410s (function);
#endif  // USE_HILINK_LD2410S

#ifdef USE_HILINK_LD2412
    if (hilink_config.device == HLK_DEVICE_LD2412) result = XsnsHilinkLD2412 (function);
#endif  // USE_HILINK_LD2412

#ifdef USE_HILINK_LD2420
    if (hilink_config.device == HLK_DEVICE_LD2420) result = XsnsHilinkLD2420 (function);
#endif  // USE_HILINK_LD2420

#ifdef USE_HILINK_LD2450
    if (hilink_config.device == HLK_DEVICE_LD2450) result = XsnsHilinkLD2450 (function);
#endif  // USE_HILINK_LD2450
  }

  return result;
}

#endif      // USE_HILINK_DETECTOR
