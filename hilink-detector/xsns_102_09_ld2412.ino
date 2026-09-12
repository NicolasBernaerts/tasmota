/*
  xsns_102_09_ld2412.ino - Driver for Presence and Movement sensor HLK-LD2412

  Copyright (C) 2026  Nicolas Bernaerts

  Version history :
    07/07/2026 - v1.0 - Creation
    17/07/2026 - v1.1 - Set bluetooth flag persistent

  Connexions :
    * GPIO1 should be declared as LD2410 Tx and connected to HLK-LD2412 Rx
    * GPIO3 should be declared as LD2410 Rx and connected to HLK-LD2412 Tx

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
#ifdef USE_HILINK_LD2412

#include <TasmotaSerial.h>

/*************************************************\
 *               Variables
\*************************************************/

// constant
#define LD2412_START_DELAY              5         // sensor startup delay

#define LD2412_GATE_QUANTITY            14        // number of sensor gates
#define LD2412_GATE_WIDTH               75        // one gate handles 70 cm
#define LD2412_DIST_MAX                 1050      // max 10.5 m

#define LD2412_ENERGY_GATE_MOTION       17        // index of motion gate level in energy mode 
#define LD2412_ENERGY_GATE_STATIC       31        // index of static gate level in energy mode 
#define LD2412_ENERGY_LIGHT_LEVEL       45        // index of light level in energy mode 

#define LD2412_DATA_RATE                115200

/*************************************************\
 *               Variables
\*************************************************/

// ----------------
// binary commands
// ----------------

uint8_t ld2412_cmnd_reset[]         PROGMEM = { 0xa2, 0x00 };
uint8_t ld2412_cmnd_restart[]       PROGMEM = { 0xa3, 0x00 };

uint8_t ld2412_cmnd_read_firmware[] PROGMEM = { 0xa0, 0x00 };
uint8_t ld2412_cmnd_read_mac[]      PROGMEM = { 0xa5, 0x00, 0x01, 0x00 };

uint8_t ld2412_cmnd_bluetooth[]     PROGMEM = { 0xa4, 0x00, 0x00, 0x00 };

uint8_t ld2412_cmnd_energy_on[]     PROGMEM = { 0x62, 0x00 };
uint8_t ld2412_cmnd_energy_off[]    PROGMEM = { 0x63, 0x00 };

uint8_t ld2412_cmnd_get_width[]     PROGMEM = { 0x11, 0x00 };
uint8_t ld2412_cmnd_set_width[]     PROGMEM = { 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00 };

uint8_t ld2412_cmnd_get_param[]     PROGMEM = { 0x12, 0x00 };
uint8_t ld2412_cmnd_set_param[]     PROGMEM = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

uint8_t ld2412_cmnd_get_motion[]    PROGMEM = { 0x13, 0x00 };
uint8_t ld2412_cmnd_set_motion[]    PROGMEM = { 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

uint8_t ld2412_cmnd_get_static[]    PROGMEM = { 0x14, 0x00 };
uint8_t ld2412_cmnd_set_static[]    PROGMEM = { 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

uint8_t ld2412_cmnd_get_light[]     PROGMEM = { 0x1c, 0x00, 0x01, 0x00 };
uint8_t ld2412_cmnd_set_light[]     PROGMEM = { 0x0c, 0x00, 0x00, 0x00 };

uint8_t ld2412_cmnd_auto_start[]    PROGMEM = { 0x0b, 0x00 };
uint8_t ld2412_cmnd_auto_progress[] PROGMEM = { 0x1b, 0x00 };


// list of commands available
enum LD2412ListCommand { LD2412_CMND_NONE, LD2412_CMND_START, LD2412_CMND_STOP, LD2412_CMND_RESTART, LD2412_CMND_RESET, LD2412_CMND_VERSION, LD2412_CMND_MAC, LD2412_CMND_BLUETOOTH, LD2412_CMND_ENERGY_ON, LD2412_CMND_ENERGY_OFF, LD2412_CMND_GET_WIDTH, LD2412_CMND_SET_WIDTH, LD2412_CMND_GET_PARAM, LD2412_CMND_SET_PARAM, LD2412_CMND_GET_MOTION, LD2412_CMND_SET_MOTION, LD2412_CMND_GET_STATIC, LD2412_CMND_SET_STATIC, LD2412_CMND_GET_LIGHT, LD2412_CMND_SET_LIGHT, LD2412_CMND_AUTO_START, LD2412_CMND_AUTO_PROGRESS, LD2412_CMND_MAX };

// MQTT commands
const char kHLKLD2412Commands[]         PROGMEM = "hlk" "|" "_reset" "|"    "_restart"   "|"    "_auto"   "|"   "_energy"    "|"        "_bt"      "|"    "_width"    "|"    "_gate"   "|"   "_light"    "|"      "_out"       ;
void (* const HLKLD2412Command[])(void) PROGMEM = {   &CmndLD2412Reset, &CmndLD2412Restart, &CmndLD2412Auto, &CmndLD2412Energy, &CmndLD2412Bluetooth,  &CmndLD2412Width, &CmndLD2412Gate, &CmndLD2412Light, &CmndLD2412Output };

/******************************\
 *           Commands
\******************************/

void CmndLD2412Reset ()
{
  HilinkAppendCommand (LD2412_CMND_RESET);
  ResponseCmndDone ();
}

void CmndLD2412Restart ()
{
  HilinkAppendCommand (LD2412_CMND_RESTART);
  ResponseCmndDone ();
}

void CmndLD2412Auto ()
{
  HilinkAppendCommand (LD2412_CMND_AUTO_START);
  ResponseCmndDone ();
}

void CmndLD2412Energy ()
{
  // if gain data is available
  if (XdrvMailbox.data_len > 0)
  {
    // append command
    if (XdrvMailbox.payload == 0) HilinkAppendCommand (LD2412_CMND_ENERGY_OFF);
      else HilinkAppendCommand (LD2412_CMND_ENERGY_ON);

    // command handled
    ResponseCmndDone ();
  }

  // command handled
  else ResponseCmndFailed ();
}

void CmndLD2412Bluetooth ()
{
  // if gain data is available
  if (XdrvMailbox.data_len > 0)
  {
    // append command
    HilinkAppendCommand (LD2412_CMND_BLUETOOTH, (uint8_t)XdrvMailbox.payload);

    // append device restart
    HilinkAppendCommand (LD2410_CMND_RESTART);

    // command handled
    ResponseCmndDone ();
  }

  // command handled
  else ResponseCmndFailed ();
}

void CmndLD2412Output ()
{
  // if output level is provided
  if (XdrvMailbox.data_len > 0)
  {
    // append command
    (XdrvMailbox.payload == 1) ? hilink_config.param.output = 0 : hilink_config.param.output = 1; 
    HilinkAppendCommand (LD2412_CMND_SET_PARAM);

    // command handled
    ResponseCmndDone ();
  }

  // command handled
  else ResponseCmndFailed ();
}

void CmndLD2412Width ()
{
  bool    is_ok = true;
  uint8_t param = UINT8_MAX;

  // if gain data is available
  if (XdrvMailbox.data_len > 0)
  {
    switch (XdrvMailbox.payload)
    {
      case 20: param = 3;     break;
      case 50: param = 1;     break;
      case 75: param = 0;     break;
      default: is_ok = false; break;
    }

    if (is_ok) HilinkAppendCommand (LD2412_CMND_SET_WIDTH, param);
  }

  // else command failed
  else HilinkAppendCommand (LD2412_CMND_GET_WIDTH);

  if (is_ok) ResponseCmndDone ();
    else ResponseCmndFailed ();
}

// command : hlk_gate gate motion,still
void CmndLD2412Gate ()
{
  bool    is_ok = true; 
  uint8_t gate;
  int     motion, still;
  char   *pstr_motion;
  char   *pstr_still; 
  char    str_data[16];

  // if no gate provided, get all gate params
  if (XdrvMailbox.data_len == 0) HilinkAppendCommand (LD2412_CMND_GET_PARAM);

  // else if gate index is provided
  else
  {
    // get gate number
    strcpy (str_data, XdrvMailbox.data);
    pstr_motion = strchr (str_data, ' ');
    if (pstr_motion != nullptr) *pstr_motion++ = 0;
    gate = (uint8_t)atoi (str_data) - 1;

    // if data are available
    is_ok = ((gate < hilink_status.gate_qty) && (pstr_motion != nullptr));
    if (is_ok)
    {
      // extract data
      pstr_still = strchr (pstr_motion, ',');
      if (pstr_still != nullptr) *pstr_still++ = 0;
      motion = atoi (pstr_motion);
      (pstr_still != nullptr) ? still = atoi (pstr_still) : still = motion;

      // generate command
      hilink_motion.arr_gate[gate].threshold = motion;
      hilink_static.arr_gate[gate].threshold = still;
      HilinkAppendCommand (LD2412_CMND_SET_STATIC, gate + 1);
      HilinkAppendCommand (LD2412_CMND_SET_MOTION, gate + 1);
    }
  }

  // command done
  if (is_ok) ResponseCmndDone ();
    else ResponseCmndFailed ();
}

// command : hlk_light state threshold
void CmndLD2412Light ()
{
  bool  is_ok     = true;
  int   threshold = INT_MAX;
  int   action    = -1;
  char *pstr_thres;
  char  str_data[16];
  char  str_action[8];

  // if no parameter provided, get light parameters
  if (XdrvMailbox.data_len == 0) HilinkAppendCommand (LD2412_CMND_GET_LIGHT);

  // else if parameters are provided
  else
  {
    // get gate number
    strcpy (str_data, XdrvMailbox.data);

    // look for threshold value
    pstr_thres = strchr (str_data, ' ');
    if (pstr_thres != nullptr) *pstr_thres++ = 0;

    // collect for action and threshold
    action = GetCommandCode (str_action, sizeof (str_action), str_data, kHilinkLights);
    is_ok  = (action >= 0);
    if (is_ok) 
    {
      hilink_config.param.light = (uint8_t)action;
      if (pstr_thres != nullptr) hilink_config.light_thres = (uint8_t)(atoi (pstr_thres) * 255 / 100);

      // append command
      HilinkAppendCommand (LD2412_CMND_SET_LIGHT);
    }
  }

  // command done
  (is_ok) ? ResponseCmndDone () : ResponseCmndFailed ();
}

/******************************\
 *       Common commands
\******************************/

void LD2412DeviceCommand (const uint8_t command, const uint8_t context)
{
  // handle different common commands
  switch (command)
  {
    // display device help
    case HILINK_CMND_HELP:
      AddLog (LOG_LEVEL_INFO, PSTR ("%s specific commands :"), hilink_status.str_model);
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_reset        = reset device"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_restart      = restart device"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_auto         = start auto level detection"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_energy <0/1> = set energy mode (defaut)"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_bt <0/1>     = enable bluetooth"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_width <val>  = set gate width cm (20, 50 or 75)"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_out <0/1>    = output level on detection"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_gate <gate> <motion,static> = gate sensitivity (%)"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    gate   : gate number            1..%u"), hilink_status.gate_qty);
      AddLog (LOG_LEVEL_INFO, PSTR ("    motion : motion detection level 0..100%%"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    still  : still detection level  0..100%%"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_light <conf> <thres> = light sensitivity control"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    conf  : off   = disabled"));
      AddLog (LOG_LEVEL_INFO, PSTR ("            below = detection on low light"));
      AddLog (LOG_LEVEL_INFO, PSTR ("            above = detection on high light"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    thres : light trigger level   0..100%%"));
      break;
      
    // init device parameters
    case HILINK_CMND_INIT:
      // declare device
      hilink_status.baudrate   = LD2412_DATA_RATE;
      hilink_status.gate_qty   = LD2412_GATE_QUANTITY;
      hilink_status.gate_width = LD2412_GATE_WIDTH;
      if (hilink_status.dist_limit > LD2412_DIST_MAX) hilink_status.dist_limit = LD2412_DIST_MAX;

      // set presence and motion detection status
      hilink_static.enabled = true;
      hilink_motion.enabled = true;
      break;

    // device environment initialisation
    case HILINK_CMND_FIRST:
      // default config at startup
      hilink_reception.data   = true;
      hilink_reception.energy = false;
      hilink_command.mode     = false;

      // set initial delay
      HilinkSetCommandDelay (LD2412_START_DELAY);

      // query device
      HilinkAppendCommand (LD2412_CMND_VERSION);                  // firmware version
      HilinkAppendCommand (LD2412_CMND_MAC);                      // MAC address
      HilinkAppendCommand (LD2412_CMND_GET_WIDTH);                // gate width
      HilinkAppendCommand (LD2412_CMND_GET_MOTION);               // gates motion threshold
      HilinkAppendCommand (LD2412_CMND_GET_STATIC);               // gates static threshold

      // set parameters and light sensitivity
      HilinkAppendCommand (LD2412_CMND_SET_PARAM);                // main parameters
      HilinkAppendCommand (LD2412_CMND_SET_LIGHT);                // light sensitivity

      // set bluetooth and data energy mode
      HilinkAppendCommand (LD2412_CMND_BLUETOOTH, hilink_config.param.bluetooth);
      HilinkAppendCommand (LD2412_CMND_ENERGY_ON);                
      break;

    // read device data information
    case HILINK_CMND_INFO:
      HilinkAppendCommand (LD2412_CMND_VERSION);                  // firmware version
      HilinkAppendCommand (LD2412_CMND_MAC);                      // MAC address
      break;
      
    // read device parameters
    case HILINK_CMND_PARAM:
      HilinkAppendCommand (LD2412_CMND_GET_PARAM);
      HilinkAppendCommand (LD2412_CMND_GET_WIDTH);                // gate width
      HilinkAppendCommand (LD2412_CMND_GET_MOTION);               // gates motion threshold
      HilinkAppendCommand (LD2412_CMND_GET_STATIC);               // gates static threshold
      HilinkAppendCommand (LD2412_CMND_GET_LIGHT);                // light sensitivity config
      break;
    
    // set presence-less delay
    case HILINK_CMND_DELAY:
      HilinkAppendCommand (LD2412_CMND_SET_PARAM);
      break;
      
    // set minimum detection distance
    case HILINK_CMND_DMIN:
      break;

    // set maximum detection distance
    case HILINK_CMND_DMAX:
      HilinkAppendCommand (LD2412_CMND_SET_PARAM);
      break;
    
    // append JSON according to context
    case HILINK_CMND_JSON:
      switch (context)
      {
        case HILINK_JSON_GENERAL:  ResponseAppend_P (PSTR (",\"Dist\":%u,\"Light\":%u"), HilinkGetDistance (HILINK_JSON_GENERAL), hilink_status.light_level); break;
        case HILINK_JSON_PRESENCE: ResponseAppend_P (PSTR (",\"Dist\":%u,\"Power\":%u"), HilinkGetDistance (HILINK_JSON_PRESENCE), HilinkGetPower (HILINK_JSON_PRESENCE)); break;
        case HILINK_JSON_MOTION:   ResponseAppend_P (PSTR (",\"Dist\":%u,\"Power\":%u"), HilinkGetDistance (HILINK_JSON_MOTION), HilinkGetPower (HILINK_JSON_MOTION)); break;
      } 
      break;
  }
}

/**************************************************\
 *                  Functions
\**************************************************/
/*
// device initial commands
void LD2412AppendInitCommand ()
{
  // default config at startup
  hilink_reception.data   = true;
  hilink_reception.energy = false;
  hilink_command.mode     = false;

  // set initial delay
  HilinkSetCommandDelay (LD2412_START_DELAY);

  // query device
  LD2412DeviceCommand (HILINK_CMND_INFO, 0);
  LD2412DeviceCommand (HILINK_CMND_PARAM, 0);

  // set parameters
  HilinkAppendCommand (LD2412_CMND_SET_PARAM);                // main parameters

  // set light sensitivity
  HilinkAppendCommand (LD2412_CMND_SET_LIGHT);                // light sensitivity

  // set bluetooth
  HilinkAppendCommand (LD2412_CMND_BLUETOOTH, hilink_config.param.bluetooth);

  // set data energy mode
  HilinkAppendCommand (LD2412_CMND_ENERGY_ON);                
}
*/

/*********************************************\
 *             Communication
\*********************************************/

// send command
void LD2412SendCommand (const uint8_t command, char *pstr_param)
{
  uint8_t  gate;
  uint8_t  param1 = 0;
  long     param2 = 0;
  long     param3 = 0;
  uint32_t value;
  size_t   body;
  uint8_t  arr_buffer[20];
  char    *pstr_param2 = nullptr;
  char    *pstr_param3 = nullptr;

  // check sensor presence
  if (hilink_status.pserial == nullptr) return;

  // extract parameter 1
  if (pstr_param != nullptr)
  {
    pstr_param2 = strchr (pstr_param, ',');
    if (pstr_param2 != nullptr) *pstr_param2++ = 0;
    param1 = (uint8_t)atoi (pstr_param);
  }

  // extract parameter 2
  if (pstr_param2 != nullptr)
  {
    pstr_param3 = strchr (pstr_param2, ',');
    if (pstr_param3 != nullptr) *pstr_param3++ = 0;
    param2 = atol (pstr_param2);
  }

  // extract parameter 3
  if (pstr_param3 != nullptr) param3 = atol (pstr_param3);

  // set command reception
  hilink_reception.data = false;

  // handle command
  body = 0;
  switch (command)
  {
    case LD2412_CMND_START:
      body = sizeof (hilink_cmnd_start);
      memcpy_P (arr_buffer, hilink_cmnd_start, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s command start"), hilink_status.str_model);
      break;

    case LD2412_CMND_STOP:
      body = sizeof (hilink_cmnd_stop);
      memcpy_P (arr_buffer, hilink_cmnd_stop, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s command stop"), hilink_status.str_model);
      break;

    case LD2412_CMND_RESTART:
      body = sizeof (ld2412_cmnd_restart);
      memcpy_P (arr_buffer, ld2412_cmnd_restart, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s restart"), hilink_status.str_model);

      // prepare init commands after restart
      HilinkDeviceCommand (HILINK_CMND_FIRST);
      break;

    case LD2412_CMND_RESET:
      body = sizeof (ld2412_cmnd_reset);
      memcpy_P (arr_buffer, ld2412_cmnd_reset, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s reset"), hilink_status.str_model);

      // ask for device restart
      HilinkSetCommandDelay (LD2412_START_DELAY);
      HilinkAppendCommand (LD2412_CMND_RESTART);
      break;

    case LD2412_CMND_VERSION:
      body = sizeof (ld2412_cmnd_read_firmware);
      memcpy_P (arr_buffer, ld2412_cmnd_read_firmware, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s read firmware"), hilink_status.str_model);
      break;

    case LD2412_CMND_MAC:
      body = sizeof (ld2412_cmnd_read_mac);
      memcpy_P (arr_buffer, ld2412_cmnd_read_mac, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s read MAC address"), hilink_status.str_model);
      break;

    case LD2412_CMND_BLUETOOTH:
      body = sizeof (ld2412_cmnd_bluetooth);
      memcpy_P (arr_buffer, ld2412_cmnd_bluetooth, body);
      arr_buffer[2] = param1;
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set Bluetooth [%u]"), hilink_status.str_model, param1);
      break;

    case LD2412_CMND_ENERGY_ON:
      body = sizeof (ld2412_cmnd_energy_on);
      memcpy_P (arr_buffer, ld2412_cmnd_energy_on, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s energy on"), hilink_status.str_model);
      break;

    case LD2412_CMND_ENERGY_OFF:
      body = sizeof (ld2412_cmnd_energy_off);
      memcpy_P (arr_buffer, ld2412_cmnd_energy_off, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s energy off"), hilink_status.str_model);
      break;

    case LD2412_CMND_GET_WIDTH:
      body = sizeof (ld2412_cmnd_get_width);
      memcpy_P (arr_buffer, ld2412_cmnd_get_width, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s get gate width"), hilink_status.str_model);
      break;

    case LD2412_CMND_SET_WIDTH:
      body = sizeof (ld2412_cmnd_set_width);
      memcpy_P (arr_buffer, ld2412_cmnd_set_width, body);
      arr_buffer[2] = param1;
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set gate width [%u]"), hilink_status.str_model, param1);
      break;

    case LD2412_CMND_GET_PARAM:
      body = sizeof (ld2412_cmnd_get_param);
      memcpy_P (arr_buffer, ld2412_cmnd_get_param, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s get param"), hilink_status.str_model);
      break;

    case LD2412_CMND_SET_PARAM:
      body = sizeof (ld2412_cmnd_set_param);
      memcpy_P (arr_buffer, ld2412_cmnd_set_param, body);
      arr_buffer[2] = HilinkGateMin ();
      arr_buffer[3] = HilinkPresenceGateMax ();
      arr_buffer[4] = (uint8_t)(hilink_config.delay % 256);
      arr_buffer[5] = (uint8_t)(hilink_config.delay / 256);
      arr_buffer[6] = hilink_config.param.output;
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set param"), hilink_status.str_model);
      break;

    case LD2412_CMND_GET_MOTION:
      body = sizeof (ld2412_cmnd_get_motion);
      memcpy_P (arr_buffer, ld2412_cmnd_get_motion, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s get motion threshold"), hilink_status.str_model);
      break;

    case LD2412_CMND_SET_MOTION:
      body = sizeof (ld2412_cmnd_set_motion);
      memcpy_P (arr_buffer, ld2412_cmnd_set_motion, body);
      for (gate = 0; gate < hilink_status.gate_qty; gate ++) arr_buffer[2 + gate] = hilink_motion.arr_gate[gate].threshold;
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set gate %u motion threshold"), hilink_status.str_model, param1);
      break;

    case LD2412_CMND_GET_STATIC:
      body = sizeof (ld2412_cmnd_get_static);
      memcpy_P (arr_buffer, ld2412_cmnd_get_static, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s get static threshold"), hilink_status.str_model);
      break;

    case LD2412_CMND_SET_STATIC:
      body = sizeof (ld2412_cmnd_set_static);
      memcpy_P (arr_buffer, ld2412_cmnd_set_static, body);
      for (gate = 0; gate < hilink_status.gate_qty; gate ++) arr_buffer[2 + gate] = hilink_static.arr_gate[gate].threshold;
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set gate %u static threshold"), hilink_status.str_model, param1);
      break;

    case LD2412_CMND_GET_LIGHT:
      body = sizeof (ld2412_cmnd_get_light);
      memcpy_P (arr_buffer, ld2412_cmnd_get_light, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s get light sensitivity config"), hilink_status.str_model);
      break;

    case LD2412_CMND_SET_LIGHT:
      body = sizeof (ld2412_cmnd_set_light);
      memcpy_P (arr_buffer, ld2412_cmnd_set_light, body);
      arr_buffer[2] = hilink_config.param.light;
      arr_buffer[3] = hilink_config.light_thres;
      AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s set light config %u, threshold %d"), hilink_status.str_model, hilink_config.param.light, hilink_config.light_thres);
      break;

    case LD2412_CMND_AUTO_START:
      body = sizeof (ld2412_cmnd_auto_start);
      memcpy_P (arr_buffer, ld2412_cmnd_auto_start, body);
      AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s auto level started"), hilink_status.str_model);
      break;

    case LD2412_CMND_AUTO_PROGRESS:
      body = sizeof (ld2412_cmnd_auto_progress);
      memcpy_P (arr_buffer, ld2412_cmnd_auto_progress, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s auto level progress"), hilink_status.str_model);
      break;
  }

  // if command defined, send it
  if (body > 0)
  {
    HilinkCommandStarted (command, param1);
    HilinkSendCommand (arr_buffer, body);
  }
}

// Handling of received data
void LD2412HandleReceivedCommand ()
{
  uint8_t  index, gate;
  uint16_t command, value;
  char     str_state[16];
  String   str_level;

  // log data to console
  HilinkLogHexa (hilink_reception.arr_body, hilink_reception.idx_body, true, false);

  // if enough data received
  if (hilink_reception.idx_body >= 14)
  {
    // handle according to command index
    //  0  1  2  3  4  5  6  7 ...
    // FD FC FB FA yy 00 xx xx ... 04 03 02 01
    command = HilinkConvertSerialToUint16 (hilink_reception.arr_body[7], hilink_reception.arr_body[6]);
    switch (command)
    {
      case 0xff01:    // command start
        hilink_command.mode = true;
        hilink_status.protocol = hilink_reception.arr_body[10];
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s command ON"), hilink_status.str_model);
        break;
      
      case 0xfe01:    // command stop
        hilink_command.mode = false;
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s command OFF"), hilink_status.str_model);
        break;

      case 0xa201:    // reset to factory
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s reset to factory"), hilink_status.str_model);
        break;

      case 0xa301:    // restart module
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s restart"), hilink_status.str_model);
        break;

      case 0xa001:    // read firmware
        sprintf_P (str_state, PSTR ("%x.%02x.%02x%02x%02x%02x"), hilink_reception.arr_body[13], hilink_reception.arr_body[12], hilink_reception.arr_body[17], hilink_reception.arr_body[16], hilink_reception.arr_body[15], hilink_reception.arr_body[14]);
        hilink_status.str_firmware = str_state;
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s firmware %s"), hilink_status.str_model, str_state);
        break;

      case 0xa501:  // read MAC address
        //  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17
        // FD FC FB FA A0 00 A5 01 aa bb cc dd ee ff 04 03 02 01
        // header     | len |     |   mac address   |   footer
        sprintf_P (hilink_status.str_mac, PSTR ("%02X:%02X:%02X:%02X:%02X:%02X"), hilink_reception.arr_body[10], hilink_reception.arr_body[11], hilink_reception.arr_body[12], hilink_reception.arr_body[13], hilink_reception.arr_body[14], hilink_reception.arr_body[15]);
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s MAC is %s"), hilink_status.str_model, hilink_status.str_mac);
        break;

      case 0xa401:    // bluetooth activation
        if (hilink_reception.arr_body[8] == 1) AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s bluetooth command failed"), hilink_status.str_model);
        else
        {
          hilink_config.param.bluetooth = hilink_command.param;
          (hilink_config.param.bluetooth == 0) ? strcpy_P (str_state, PSTR ("OFF")) : strcpy_P (str_state, PSTR ("ON"));
          AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s bluetooth %s success"), hilink_status.str_model, str_state);
        }
        break;

      case 0x6201:    // engineering mode on
        hilink_reception.energy = true;
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s data in energy mode"), hilink_status.str_model);
        break;

      case 0x6301:    // engineering mode off
        hilink_reception.energy = false;
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s data in basic mode"), hilink_status.str_model);
        break;

      case 0x0101:    // set gate width
        HilinkAppendCommand (LD2412_CMND_RESTART);
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s gate width set [%u]"), hilink_status.str_model, hilink_command.param);
        break;

      case 0x1101:    // get gate width
        switch (hilink_reception.arr_body[10])
        {
          case 0: hilink_status.gate_width = 75; break;
          case 1: hilink_status.gate_width = 50; break;
          case 3: hilink_status.gate_width = 20; break;
        }
        hilink_status.dist_limit = hilink_status.gate_width * LD2412_GATE_QUANTITY;
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s gate width is %u cm"), hilink_status.str_model, hilink_status.gate_width);
        break;

      case 0x0201:    // set main param
        HilinkAppendCommand (LD2412_CMND_GET_PARAM);
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s main param set"), hilink_status.str_model);
        break;

      case 0x1201:    // get main param
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s minimum gate is %u"),  hilink_status.str_model, hilink_reception.arr_body[10]);
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s maximum gate is %u"),  hilink_status.str_model, hilink_reception.arr_body[11]);
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s delay is %u sec"),     hilink_status.str_model, HilinkConvertSerialToUint16 (hilink_reception.arr_body[12], hilink_reception.arr_body[13]));
        (hilink_reception.arr_body[14] == 0) ? strcpy_P (str_state, PSTR ("HIGH")) : strcpy_P (str_state, PSTR ("LOW")); 
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s pin %s on detection"), hilink_status.str_model, str_state);
        break;

      case 0x0301:    // set motion threshold
        HilinkAppendCommand (LD2412_CMND_GET_MOTION);
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s motion levels set"), hilink_status.str_model);
        break;

      case 0x1301:    // get motion threshold
        str_level = "";
        for (gate = 0; gate < hilink_status.gate_qty; gate ++)
        {
          hilink_motion.arr_gate[gate].threshold = hilink_reception.arr_body[10 + gate];
          str_level += " ";
          str_level += hilink_motion.arr_gate[gate].threshold;
        }
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s motion levels are %s"), hilink_status.str_model, str_level.c_str ());
        break;

      case 0x0401:    // set static threshold
        HilinkAppendCommand (LD2412_CMND_GET_STATIC);
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s static levels set"), hilink_status.str_model);
        break;

      case 0x1401:    // get static threshold
        str_level = "";
        for (gate = 0; gate < hilink_status.gate_qty; gate ++)
        {
          hilink_static.arr_gate[gate].threshold = hilink_reception.arr_body[10 + gate];
          str_level += " ";
          str_level += hilink_static.arr_gate[gate].threshold;
        }
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s static levels are %s"), hilink_status.str_model, str_level.c_str ());
        break;

      case 0x0c01:    // set light sensitivity
        HilinkAppendCommand (LD2412_CMND_GET_LIGHT);
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s light sensitivity set"), hilink_status.str_model);
        break;

      case 0x1c01:    // get light sensitivity config
        index = hilink_reception.arr_body[10];
        value = (uint16_t)hilink_reception.arr_body[11] * 100 / 255;
        if (index == HILINK_LIGHT_OFF) AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s light sensitivity disabled"), hilink_status.str_model);
          else if (index == HILINK_LIGHT_LOW) AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s detection when light lower than %u%%"),  hilink_status.str_model, value);
          else if (index == HILINK_LIGHT_HIGH) AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s detection when light higher than %u%%"), hilink_status.str_model, value);
        break;

      case 0x0b01:    // start auto threshold
        hilink_status.progress = 0;
        HilinkSetCommandDelay (10);
        HilinkAppendCommand (LD2412_CMND_AUTO_PROGRESS);
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s auto threshold started"), hilink_status.str_model);
        break;

      case 0x1b01:    // auto threshold progress
        if (hilink_reception.arr_body[10] == 0) 
        {
          hilink_status.progress = UINT8_MAX;
          AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s auto threshold finished"), hilink_status.str_model);
        }
        else
        {
          HilinkSetCommandDelay (2);
          HilinkAppendCommand (LD2412_CMND_AUTO_PROGRESS);
          AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s auto threshold running (%u sec.)"), hilink_status.str_model, 10 + hilink_status.progress * 2);
          hilink_status.progress++;
        }
        break;
    }
  }

  // declare command as finished
  HilinkCommandFinished ();
}

// Handling of received data
void LD2412HandleReceivedData ()
{
  uint8_t  gate, activity;

  // log data to console
  HilinkLogHexa (hilink_reception.arr_body, hilink_reception.idx_body, false, false);

  // target activity
  activity = hilink_reception.arr_body[8];

  // motion target
  hilink_motion.arr_target[0].dist   = HilinkConvertSerialToUint16 (hilink_reception.arr_body[9],  hilink_reception.arr_body[10]);
  hilink_motion.arr_target[0].power  = hilink_reception.arr_body[11];
  hilink_motion.arr_target[0].active = ((hilink_motion.arr_target[0].power > 0) && ((activity & 0x01) == 0x01));

  // static target
  hilink_static.arr_target[0].dist   = HilinkConvertSerialToUint16 (hilink_reception.arr_body[12], hilink_reception.arr_body[13]);
  hilink_static.arr_target[0].power  = hilink_reception.arr_body[14];
  hilink_static.arr_target[0].active = ((hilink_static.arr_target[0].power > 0) && ((activity & 0x02) == 0x02));

  // if energy mode, get ambient light level and retreive gate energy
  if (hilink_reception.arr_body[6] == 0x01)
  {
    hilink_status.light_level = hilink_reception.arr_body[LD2412_ENERGY_LIGHT_LEVEL];
    for (gate = 0; gate < hilink_status.gate_qty; gate ++) hilink_motion.arr_gate[gate].power = hilink_reception.arr_body[LD2412_ENERGY_GATE_MOTION + gate];
    for (gate = 0; gate < hilink_status.gate_qty; gate ++) hilink_static.arr_gate[gate].power = hilink_reception.arr_body[LD2412_ENERGY_GATE_STATIC + gate];      
  }

  // else, reset ambient light level and gate energy
  else 
  {
    hilink_status.light_level = 0;
    for (gate = 0; gate < hilink_status.gate_qty; gate ++) hilink_motion.arr_gate[gate].power = 0;
    for (gate = 0; gate < hilink_status.gate_qty; gate ++) hilink_static.arr_gate[gate].power = 0;
  }

  // update light level
  if (hilink_status.light_level >= hilink_config.light_thres) hilink_status.char_yellow = 'L';
    else hilink_status.char_yellow = 0;

  // reset reception buffer
  HilinkReceptionEmpty ();
}

/*********************************************\
 *                   Callback
\*********************************************/

// Handling of next command
void LD2412ProcessCommandQueue ()
{
  bool    cmnd_mode, cmnd_waiting;
  uint8_t command, param;
  char    str_command[16];
  char    str_param[12];

  // check validity
  if (hilink_config.device != HLK_DEVICE_LD2412) return;
  if (hilink_status.pserial == nullptr) return;
  if (!HilinkCommandPossible ()) return;

  // check if commands are waiting
  cmnd_mode    = HilinkIsInCommandMode ();
  cmnd_waiting = HilinkCommandWaiting ();

  // commands in the pipe and command mode not started, start command mode
  if (cmnd_waiting && !cmnd_mode) LD2412SendCommand (LD2412_CMND_START, nullptr);
    
  // else if no command in the queue, stop command mode
  else if (!cmnd_waiting && cmnd_mode) LD2412SendCommand (LD2412_CMND_STOP, nullptr);

  // else send next command
  else if (cmnd_waiting && cmnd_mode)
  {
    HilinkGetNextCommand (str_command, sizeof (str_command));
    HilinkSplitCommand (str_command, command, str_param, sizeof (str_param));
    LD2412SendCommand (command, str_param);
  }
}

// Handling of serial reception
void LD2412SerialReception ()
{
  uint8_t  index, delta;
  uint32_t value;

  // check if enabled
  if (hilink_config.device != HLK_DEVICE_LD2412) return;
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
      // extract last 4 bytes
      delta = hilink_reception.idx_body - 4;
      value = HilinkConvertSerialToUint32 (hilink_reception.arr_body[delta + 3], hilink_reception.arr_body[delta + 2], hilink_reception.arr_body[delta + 1], hilink_reception.arr_body[delta]);

      // detect command mode
      if (value == 0xfdfcfbfa) hilink_reception.data = false;

      // check for header or footer
      switch (value)
      {
        case 0xfdfcfbfa:    // command header
        case 0xf4f3f2f1:    // energy data header
          // if more than 4 digits received, remove everything before header
          if (hilink_reception.idx_body > 4)
          {
            for (index = 0; index < 4; index ++) hilink_reception.arr_body[index] = hilink_reception.arr_body[delta + index];
            hilink_reception.idx_body = 4;
          }
          break;

        case 0x04030201:    // command footer
          LD2412HandleReceivedCommand ();
          break;

        case 0xf8f7f6f5:    // data footer
          LD2412HandleReceivedData ();
          break;
      }
    }
  }
}

/***************************************\
 *              Interface
\***************************************/

// LD2412 sensor
bool XsnsHilinkLD2412 (const uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
    case FUNC_COMMAND:
      if (hilink_config.device == HLK_DEVICE_LD2412) result = DecodeCommand (kHLKLD2412Commands, HLKLD2412Command);
      break;

    case FUNC_EVERY_250_MSECOND:
      LD2412ProcessCommandQueue ();
      break;

    case FUNC_EVERY_100_MSECOND:
      LD2412SerialReception ();
      break;
  }

  return result;
}

#endif     // USE_HILINK_LD2412
#endif     // USE_HILINK_DETECTOR
