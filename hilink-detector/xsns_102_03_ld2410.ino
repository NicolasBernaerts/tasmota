/*
  xsns_102_03_ld2410.ino - Driver for Presence and Movement sensor HLK-LD2410

  Copyright (C) 2022-2026  Nicolas Bernaerts

  Version history :
    28/06/2022 - v1.0 - Creation
    15/01/2023 - v2.0 - Complete rewrite
    03/04/2023 - v2.1 - Add trigger to avoid false detection
    12/09/2023 - v2.2 - Switch to LD2410 Rx & LD2410 Tx
    20/11/2023 - v2.3 - Tasmota 13.2 compatibility
                        Switch parameters to rf_code[2]
    23/11/2023 - v2.4 - Add bluetooth command (thanks to protectivedad)
    25/05/2024 - v2.5 - Change help command to ld2410 
    08/06/2026 - v3.0 - Complete rewrite to be used as a generic hilink detector
    17/07/2026 - v3.1 - Set bluetooth flag persistent
                        Separate LD2410B and LD2410C

  Connexions :
    * GPIO1 should be declared as LD2410 Tx and connected to HLK-LD2410 Rx
    * GPIO3 should be declared as LD2410 Rx and connected to HLK-LD2410 Tx

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
#ifdef USE_HILINK_LD2410

#include <TasmotaSerial.h>

/*************************************************\
 *               Variables
\*************************************************/

// constant
#define LD2410_START_DELAY              5         // sensor startup delay

#define LD2410_GATE_QUANTITY            8         // number of sensor gates
#define LD2410_GATE_WIDTH               75        // one gate handles 75 cm
#define LD2410_DIST_MAX                 600       // maximum detectable distance (cm)

#define LD2410_DATA_RATE                256000

#define LD2410_ENERGY_GATE_MOTION       19        // index of motion gate level in energy mode 
#define LD2410_ENERGY_GATE_STATIC       28        // index of static gate level in energy mode 
#define LD2410_ENERGY_LIGHT             37        // index of light level in energy mode 

#define LD2410_MINIMUM_FIRMWARE         0x0244    // minimum firmware version not to be outdated

// ----------------
// binary commands
// ----------------

uint8_t ld2410_cmnd_reset[]       PROGMEM = { 0xa2, 0x00 };
uint8_t ld2410_cmnd_restart[]     PROGMEM = { 0xa3, 0x00 };

uint8_t ld2410_cmnd_mac[]         PROGMEM = { 0xa5, 0x00, 0x01, 0x00 };
uint8_t ld2410_cmnd_sensitivity[] PROGMEM = { 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 };
uint8_t ld2410_cmnd_firmware[]    PROGMEM = { 0xa0, 0x00 };

uint8_t ld2410_cmnd_bluetooth[]   PROGMEM = { 0xa4, 0x00, 0x00, 0x00 };
uint8_t ld2410_cmnd_password[]    PROGMEM = { 0xa9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

uint8_t ld2410_cmnd_get_width[]   PROGMEM = { 0xab, 0x00 };
uint8_t ld2410_cmnd_set_width[]   PROGMEM = { 0xaa, 0x00, 0x00, 0x00 };

uint8_t ld2410_cmnd_get_param[]   PROGMEM = { 0x61, 0x00 };
uint8_t ld2410_cmnd_set_param[]   PROGMEM = { 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 };

uint8_t ld2410_cmnd_get_light[]   PROGMEM = { 0xae, 0x00 };
uint8_t ld2410_cmnd_set_light[]   PROGMEM = { 0xad, 0x00, 0x00, 0x00, 0x00, 0x00 };

uint8_t ld2410_cmnd_energy_on[]   PROGMEM = { 0x62, 0x00 };
uint8_t ld2410_cmnd_energy_off[]  PROGMEM = { 0x63, 0x00 };

uint8_t ld2410_cmnd_auto[]        PROGMEM = { 0x0b, 0x00, 0x0a, 0x00 };

// list of commands available
enum LD2410ListCommand { LD2410_CMND_NONE, LD2410_CMND_START, LD2410_CMND_STOP, LD2410_CMND_RESET, LD2410_CMND_RESTART, LD2410_CMND_FIRMWARE, LD2410_CMND_BLUETOOTH, LD2410_CMND_PASSWORD, LD2410_CMND_MAC, LD2410_CMND_SENSITIVITY, LD2410_CMND_GET_PARAM, LD2410_CMND_SET_PARAM, LD2410_CMND_ENERGY_OFF, LD2410_CMND_ENERGY_ON, LD2410_CMND_GET_WIDTH, LD2410_CMND_SET_WIDTH, LD2410_CMND_GET_LIGHT, LD2410_CMND_SET_LIGHT, LD2410_CMND_AUTO, LD2410_CMND_MAX };

// MQTT commands : ld_help and ld_send
const char kHLKLD2410Commands[]         PROGMEM = "hlk" "|" "_energy" "|"    "_reset"   "|"    "_restart"   "|"    "_auto"   "|"    "_width"   "|"    "_gate"   "|"   "_light"    "|"    "_out"      "|"       "_bt"          ;
void (* const HLKLD2410Command[])(void) PROGMEM = {   &CmndLD2410Energy, &CmndLD2410Reset, &CmndLD2410Restart, &CmndLD2410Auto, &CmndLD2410Width, &CmndLD2410Gate, &CmndLD2410Light, &CmndLD2410Output, &CmndLD2410Bluetooth };

/******************************\
 *           Commands
\******************************/

void CmndLD2410Reset ()
{
  HilinkAppendCommand (LD2410_CMND_RESET);
  ResponseCmndDone ();
}

void CmndLD2410Restart ()
{
  HilinkAppendCommand (LD2410_CMND_RESTART);
  ResponseCmndDone ();
}

void CmndLD2410Auto ()
{
  HilinkAppendCommand (LD2410_CMND_AUTO);
  ResponseCmndDone ();
}

void CmndLD2410Bluetooth ()
{
  bool    is_ok;
  uint8_t action;
  char   *pstr_pwd;
  char    str_data[12];

  if (XdrvMailbox.data_len > 0)
  {
    // get gate number
    strcpy (str_data, XdrvMailbox.data);
    pstr_pwd = strchr (str_data, ' ');
    if (pstr_pwd != nullptr) *pstr_pwd++ = 0;
    
    // get bluetooth action
    action = (uint8_t)atoi (str_data);
    HilinkAppendCommand (LD2410_CMND_BLUETOOTH, action);

    // if bluetooth password is provided and 6 digits long
    if (pstr_pwd != nullptr)
    {
      if (strlen (pstr_pwd) == 6) HilinkAppendCommand (LD2410_CMND_PASSWORD, pstr_pwd);
    }

    // append device restart
    HilinkAppendCommand (LD2410_CMND_RESTART);

    ResponseCmndDone ();
  }
  else ResponseCmndFailed ();
}

void CmndLD2410Energy ()
{
  if (XdrvMailbox.payload == 1) HilinkAppendCommand (LD2410_CMND_ENERGY_ON);
    else HilinkAppendCommand (LD2410_CMND_ENERGY_OFF);
  ResponseCmndDone ();
}

void CmndLD2410Width ()
{
  bool    is_ok = true;
  uint8_t param = UINT8_MAX;

  // if gain data is available
  if (XdrvMailbox.data_len > 0)
  {
    switch (XdrvMailbox.payload)
    {
      case 20: param = 1;     break;
      case 75: param = 0;     break;
      default: is_ok = false; break;
    }

    if (is_ok) HilinkAppendCommand (LD2410_CMND_SET_WIDTH, param);
  }

  // else command failed
  else HilinkAppendCommand (LD2410_CMND_GET_WIDTH);

  if (is_ok) ResponseCmndDone ();
    else ResponseCmndFailed ();
}

// set gate sensitivity
void CmndLD2410Gate ()
{
  bool    is_ok;
  uint8_t gate;
  char   *pstr_data;
  char   *pstr_sep;
  char    str_data[16];

  is_ok = (XdrvMailbox.data_len > 0);
  if (is_ok)
  {
    // get gate number
    strcpy (str_data, XdrvMailbox.data);
    pstr_data = strchr (str_data, ' ');
    if (pstr_data != nullptr)
    {
      *pstr_data = 0;
      pstr_data++;
    }
    
    // get gate target
    gate = (uint8_t)(atoi (str_data) - 1);
    is_ok = (gate < hilink_status.gate_qty);
  }

  if (is_ok && (pstr_data != nullptr))
  {
    // separate data
    pstr_sep = strchr (pstr_data, ',');
    if (pstr_sep != nullptr) *pstr_sep++ = 0;

    // extract gate threshold
    hilink_static.arr_gate[gate].threshold = (uint8_t)atoi (pstr_data);
    if (pstr_sep != nullptr) hilink_motion.arr_gate[gate].threshold = (uint8_t)atoi (pstr_sep);
      else hilink_motion.arr_gate[gate].threshold = hilink_static.arr_gate[gate].threshold;

    // update gate parameters
    HilinkAppendCommand (LD2410_CMND_SENSITIVITY, (int)gate);

    // answer
    sprintf_P (str_data, PSTR ("%u : %u,%u"), gate + 1, hilink_static.arr_gate[gate].threshold, hilink_motion.arr_gate[gate].threshold);
    ResponseCmndChar (str_data);
  }
  
  // else command failed
  else ResponseCmndFailed ();
}

// command : hlk_light state threshold
void CmndLD2410Light ()
{
  bool  is_ok     = true;
  int   threshold = INT_MAX;
  int   action    = -1;
  char *pstr_thres;
  char  str_data[16];
  char  str_action[8];

  // if no parameter provided, get light parameters
  if (XdrvMailbox.data_len == 0) HilinkAppendCommand (LD2410_CMND_GET_LIGHT);

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
      HilinkAppendCommand (LD2410_CMND_SET_LIGHT);
    }
  }

  // command done
  (is_ok) ? ResponseCmndDone () : ResponseCmndFailed ();
}

void CmndLD2410Output ()
{
  // if output level is provided
  if (XdrvMailbox.data_len > 0)
  {
    // append command
    (XdrvMailbox.payload == 1) ? hilink_config.param.output = 0 : hilink_config.param.output = 1; 
    HilinkAppendCommand (LD2410_CMND_SET_LIGHT);

    // command handled
    ResponseCmndDone ();
  }

  // command handled
  else ResponseCmndFailed ();
}

/******************************\
 *       Common commands
\******************************/

// set general parameters : delay, presence & motion max
void LD2410DeviceSetParam ()
{
  uint16_t pres_max, move_max;
  char     str_command[16];

  // check boundaries
  pres_max = hilink_config.pres_max;
  move_max = hilink_config.move_max;
  if (pres_max > LD2410_DIST_MAX) pres_max = LD2410_DIST_MAX;
  if (move_max > LD2410_DIST_MAX) move_max = LD2410_DIST_MAX;

  // update gate
  sprintf_P (str_command, PSTR ("%u,%u"), (move_max - 1) / LD2410_GATE_WIDTH, (pres_max - 1) / LD2410_GATE_WIDTH);
  HilinkAppendCommand (LD2410_CMND_SET_PARAM, str_command);
}

void LD2410DeviceCommand (const uint8_t command, const uint8_t context)
{
  // handle different common commands
  switch (command)
  {
    // display device help
    case HILINK_CMND_HELP:
      // general help
      AddLog (LOG_LEVEL_INFO, PSTR ("%s specific commands :"), hilink_status.str_model);
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_reset          = reset sensor"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_restart        = restart sensor"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_auto           = start auto level detection"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_energy <0/1>   = set data energy (default)"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_bt <0/1> <pwd> = bluetooth status and password"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_width <val>    = set gate width cm (20 or 75)"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_out <0/1>      = output level on detection"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_gate gate presence,motion = gate sensitivity (%%)"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    gate     : 1..%u"), hilink_status.gate_qty);
      AddLog (LOG_LEVEL_INFO, PSTR ("    presence : 1..100"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    motion   : 1..100"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_light <conf> <thres> = light sensitivity control"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    conf  : off             disabled"));
      AddLog (LOG_LEVEL_INFO, PSTR ("            below           detection on low light"));
      AddLog (LOG_LEVEL_INFO, PSTR ("            above           detection on high light"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    thres : trigger level   0..100%%"));
      break;

    // init device parameters
    case HILINK_CMND_INIT:
      // declare device
      hilink_status.baudrate   = LD2410_DATA_RATE;
      hilink_status.gate_qty   = LD2410_GATE_QUANTITY;
      hilink_status.gate_width = LD2410_GATE_WIDTH;
      if (hilink_status.dist_limit > LD2410_DIST_MAX) hilink_status.dist_limit = LD2410_DIST_MAX;

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
      HilinkSetCommandDelay (LD2410_START_DELAY);

      // set default parameters
      LD2410DeviceSetParam ();                                                      // set general parameters

      // device config
      HilinkAppendCommand (LD2410_CMND_MAC);                                        // read MAC address
      HilinkAppendCommand (LD2410_CMND_FIRMWARE);                                   // read firmware version
      HilinkAppendCommand (LD2410_CMND_GET_WIDTH);                                  // gate width
      HilinkAppendCommand (LD2410_CMND_GET_LIGHT);                                  // light sensitivity

      // set bluetooth and energy mode
      HilinkAppendCommand (LD2410_CMND_BLUETOOTH, hilink_config.param.bluetooth);   // set bluetooth
      HilinkAppendCommand (LD2410_CMND_ENERGY_ON);                                  // enter energy mode
      break;
      
    // read device data information
    case HILINK_CMND_INFO:
      HilinkAppendCommand (LD2410_CMND_MAC);                                        // read MAC address
      HilinkAppendCommand (LD2410_CMND_FIRMWARE);                                   // read firmware version
      break;
      
    // read device parameters
    case HILINK_CMND_PARAM:
      // append startup commands
      HilinkAppendCommand (LD2410_CMND_GET_WIDTH);                                  // gate width
      HilinkAppendCommand (LD2410_CMND_GET_LIGHT);                                  // light sensitivity

      // general parameters
      HilinkAppendCommand (LD2410_CMND_GET_PARAM);
      break;
    
    // set presence-less delay
    case HILINK_CMND_DELAY:
      LD2410DeviceSetParam ();
      break;
      
    // set minimum detection distance
    case HILINK_CMND_DMIN:
      break;

    // set maximum detection distance
    case HILINK_CMND_DMAX:
      LD2410DeviceSetParam ();
      break;
    
    // append JSON according to context
    case HILINK_CMND_JSON:
      switch (context)
      {
        case HILINK_JSON_GENERAL:  ResponseAppend_P (PSTR (",\"Dist\":%u,\"Light\":%u"), HilinkGetDistance (HILINK_JSON_GENERAL),  hilink_status.light_level);             break;
        case HILINK_JSON_PRESENCE: ResponseAppend_P (PSTR (",\"Dist\":%u,\"Power\":%u"), HilinkGetDistance (HILINK_JSON_PRESENCE), HilinkGetPower (HILINK_JSON_PRESENCE)); break;
        case HILINK_JSON_MOTION:   ResponseAppend_P (PSTR (",\"Dist\":%u,\"Power\":%u"), HilinkGetDistance (HILINK_JSON_MOTION),   HilinkGetPower (HILINK_JSON_MOTION));   break;
      } 
      break;
  }
}

/*********************************************\
 *             Communication
\*********************************************/

// send command
void LD2410SendCommand (const uint8_t command, char *pstr_param)
{
  uint8_t param = 0;
  int     value = 0;
  size_t  body, index, limit;
  uint8_t arr_buffer[24];
  char   *pstr_value;

  // check sensor presence
  if (hilink_status.pserial == nullptr) return;

  // get parameter
  if (pstr_param != nullptr)
  {
    pstr_value = strchr (pstr_param, ',');
    if (pstr_value != nullptr) *pstr_value++ = 0;
    param = (uint8_t)atoi (pstr_param);
    if (pstr_value != nullptr) value = atoi (pstr_value);
  }

  // handle command
  body = 0;
  switch (command)
  {
    case LD2410_CMND_START:
      body = sizeof (hilink_cmnd_start);
      memcpy_P (arr_buffer, hilink_cmnd_start, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set command on"), hilink_status.str_model);
      break;

    case LD2410_CMND_STOP:
      body = sizeof (hilink_cmnd_stop);
      memcpy_P (arr_buffer, hilink_cmnd_stop, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set command off"), hilink_status.str_model);
      break;

    case LD2410_CMND_GET_WIDTH:
      body = sizeof (ld2410_cmnd_get_width);
      memcpy_P (arr_buffer, ld2410_cmnd_get_width, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s get gate width"), hilink_status.str_model);
      break;

    case LD2410_CMND_SET_WIDTH:
      body = sizeof (ld2410_cmnd_set_width);
      memcpy_P (arr_buffer, ld2410_cmnd_set_width, body);
      arr_buffer[2] = param;
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set gate width [%u]"), hilink_status.str_model, param);
      break;

    case LD2410_CMND_GET_PARAM:
      body = sizeof (ld2410_cmnd_get_param);
      memcpy_P (arr_buffer, ld2410_cmnd_get_param, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s read radar parameters"), hilink_status.str_model);
      break;

    case LD2410_CMND_SET_PARAM:
      body = sizeof (ld2410_cmnd_set_param);
      memcpy_P (arr_buffer, ld2410_cmnd_set_param, body);
      arr_buffer[4]  = param;
      arr_buffer[10] = (uint8_t)value;
      arr_buffer[16] = (uint8_t)(hilink_config.delay % 256);
      arr_buffer[17] = (uint8_t)(hilink_config.delay / 256);
      AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s set delay and max gate"), hilink_status.str_model);
      break;

    case LD2410_CMND_SENSITIVITY:
      body = sizeof (ld2410_cmnd_sensitivity);
      memcpy_P (arr_buffer, ld2410_cmnd_sensitivity, body);
      arr_buffer[4]  = param;
      arr_buffer[10] = hilink_motion.arr_gate[param].threshold;
      arr_buffer[16] = hilink_static.arr_gate[param].threshold;
      AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s set gate %u sensitivity"), hilink_status.str_model, param);
      break;

    case LD2410_CMND_FIRMWARE:
      body = sizeof (ld2410_cmnd_firmware);
      memcpy_P (arr_buffer, ld2410_cmnd_firmware, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s read firmware"), hilink_status.str_model);
      break;

    case LD2410_CMND_MAC:
      body = sizeof (ld2410_cmnd_mac);
      memcpy_P (arr_buffer, ld2410_cmnd_mac, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s read MAC address"), hilink_status.str_model);
      break;

    case LD2410_CMND_RESTART:
      body = sizeof (ld2410_cmnd_restart);
      memcpy_P (arr_buffer, ld2410_cmnd_restart, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s restart"), hilink_status.str_model);

      // init device environment for restart
      HilinkDeviceCommand (HILINK_CMND_FIRST);
      break;

    case LD2410_CMND_RESET:
      body = sizeof (ld2410_cmnd_reset);
      memcpy_P (arr_buffer, ld2410_cmnd_reset, body);
      AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s reset"), hilink_status.str_model);

      // ask for device restart
      HilinkSetCommandDelay (LD2410_START_DELAY);
      HilinkAppendCommand (LD2410_CMND_RESTART);
      break;

    case LD2410_CMND_ENERGY_ON:
      body = sizeof (ld2410_cmnd_energy_on);
      memcpy_P (arr_buffer, ld2410_cmnd_energy_on, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set energy data"), hilink_status.str_model);
      break;

    case LD2410_CMND_ENERGY_OFF:
      body = sizeof (ld2410_cmnd_energy_off);
      memcpy_P (arr_buffer, ld2410_cmnd_energy_off, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set basic data"), hilink_status.str_model);
      break;

    case LD2410_CMND_BLUETOOTH:
      body = sizeof (ld2410_cmnd_bluetooth);
      memcpy_P (arr_buffer, ld2410_cmnd_bluetooth, body);
      arr_buffer[2] = param;
      AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s set bluetooth to %u"), hilink_status.str_model, param);
      break;

    case LD2410_CMND_PASSWORD:
      body = sizeof (ld2410_cmnd_password);
      memcpy_P (arr_buffer, ld2410_cmnd_password, body);
      limit = (size_t)min (strlen (pstr_param), (size_t)6);
      for (index = 0; index < limit; index ++) arr_buffer[2 + index] = pstr_param[index];
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set password to %s"), hilink_status.str_model, pstr_param);
      break;

    case LD2410_CMND_GET_LIGHT:
      body = sizeof (ld2410_cmnd_get_light);
      memcpy_P (arr_buffer, ld2410_cmnd_get_light, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s get light sensitivity config"), hilink_status.str_model);
      break;

    case LD2410_CMND_SET_LIGHT:
      body = sizeof (ld2410_cmnd_set_light);
      memcpy_P (arr_buffer, ld2410_cmnd_set_light, body);
      arr_buffer[2] = hilink_config.param.light;
      arr_buffer[3] = hilink_config.light_thres;
      arr_buffer[4] = hilink_config.param.output;
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set light config"), hilink_status.str_model);
      break;

    case LD2410_CMND_AUTO:
      body = sizeof (ld2410_cmnd_auto);
      memcpy_P (arr_buffer, ld2410_cmnd_auto, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s auto level started"), hilink_status.str_model);
      break;
    }

  // if command defined, send it
  if (body > 0)
  {
    HilinkCommandStarted (command, param);
    HilinkSendCommand (arr_buffer, body);
  }
}

// Handling of received data
void LD2410HandleReceivedCommand ()
{
  bool     changed;
  uint8_t  index;
  uint16_t command, dist_pres, dist_move, value;
  char     str_text[16];

  // log data to console
  HilinkLogHexa (hilink_reception.arr_body, hilink_reception.idx_body, true, false);

  // if not enough data received
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
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s command ON"), hilink_status.str_model);
        break;
      
      case 0xfe01:    // command stop
        hilink_command.mode = false;
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s command OFF"), hilink_status.str_model);
        break;

      case 0x6101:    // read radar parameters
        //  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37
        // FD FC FB FA 1C 00 61 01 00 00 AA 08 08 08 32 32 28 1E 14 0F 0F 0F 0F 00 00 28 28 1E 1E 14 14 14 05 00 04 03 02 01
        // header     | len |cw cv| ack |hd|dd|md|sd| moving sensitivity 0..8  | static sensitivity 0..8  |timed|trailer
        //            | 28  |     |  0  |  | 8| 8| 8|50 50 40 30 20 15 15 15 15| 0  0 40 40 30 30 20 20 20|  5  |

        // motion and presence distance limit
        dist_move = (uint16_t)(hilink_reception.arr_body[12] + 1) * hilink_status.gate_width;
        dist_pres = (uint16_t)(hilink_reception.arr_body[13] + 1) * hilink_status.gate_width;

        // set gates threshold
        for (index = 0; index < hilink_status.gate_qty; index ++) hilink_motion.arr_gate[index].threshold = hilink_reception.arr_body[14 + index];
        for (index = 0; index < hilink_status.gate_qty; index ++) hilink_static.arr_gate[index].threshold = hilink_reception.arr_body[23 + index];

        // delay
        value = HilinkConvertSerialToUint16 (hilink_reception.arr_body[32], hilink_reception.arr_body[33]);

        // log
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s delay is %u sec"), hilink_status.str_model, value);
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s max presence detection is %u cm"), hilink_status.str_model, dist_pres);
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s max motion detection is %u cm"),   hilink_status.str_model, dist_move);
        break;

      case 0x6001:    // set max gate and timeout
        HilinkAppendCommand (LD2410_CMND_GET_PARAM);
        break;

      case 0x6401:    // set gate sensitivity
        HilinkAppendCommand (LD2410_CMND_GET_PARAM);
        break;

      case 0xa001:    // read firmware
        sprintf_P (str_text, PSTR ("%x.%02x.%02x%02x%02x%02x"), hilink_reception.arr_body[13], hilink_reception.arr_body[12], hilink_reception.arr_body[17], hilink_reception.arr_body[16], hilink_reception.arr_body[15], hilink_reception.arr_body[14]);
        hilink_status.str_firmware = str_text;
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s firmware %s"), hilink_status.str_model, str_text);

        // check for outdated firmware
        value = 256 * (uint16_t)hilink_reception.arr_body[13] + (uint16_t)hilink_reception.arr_body[12];
        hilink_status.outdated = (value < LD2410_MINIMUM_FIRMWARE);
        if (hilink_status.outdated) AddLog (LOG_LEVEL_INFO, PSTR ("HLK: WARNING your firmware %s is too old ! Update to get full features"), hilink_status.str_firmware.c_str ());
        break;

      case 0xa201:  // reset device
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s reset to factory"), hilink_status.str_model);
        break;

      case 0xa301:  // restart device
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s restart"), hilink_status.str_model);
        break;

      case 0x6201:  // data in energy mode
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s data in energy mode"), hilink_status.str_model);
        break;

      case 0x6301:  // data in basic mode
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %S data in basic mode"), hilink_status.str_model);
        break;

      case 0xa401:  // set bluetooth
        if (hilink_config.param.bluetooth != hilink_command.param)
        {
          hilink_config.param.bluetooth = hilink_command.param;
          AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s bluetooth changed to %u"), hilink_status.str_model, hilink_config.param.bluetooth);
        }
        break;

      case 0xa901:    // get bluetooth password
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s bluetooth password changed"), hilink_status.str_model);
        break;

      case 0xaa01:    // set gate width
        HilinkAppendCommand (LD2410_CMND_RESTART);
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s gate width set [%u]"), hilink_status.str_model, hilink_command.param);
        break;

      case 0xab01:    // get gate width
        switch (hilink_reception.arr_body[10])
        {
          case 0: hilink_status.gate_width = 75; break;
          case 1: hilink_status.gate_width = 20; break;
        }
        hilink_status.dist_limit = hilink_status.gate_width * LD2410_GATE_QUANTITY;
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s gate width is %u cm"), hilink_status.str_model, hilink_status.gate_width);
        break;

      case 0xad01:    // set light config
        HilinkAppendCommand (LD2410_CMND_GET_LIGHT);
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s light config set"), hilink_status.str_model);
        break;

      case 0xae01:    // get light config
        index = hilink_reception.arr_body[10];
        value = (uint16_t)hilink_reception.arr_body[11] * 100 / 255;
        if (index == HILINK_LIGHT_OFF) AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s light sensitivity disabled"), hilink_status.str_model);
          else if (index == HILINK_LIGHT_LOW) AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s detection when light lower than %u%%"),  hilink_status.str_model, value);
          else if (index == HILINK_LIGHT_HIGH) AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s detection when light higher than %u%%"), hilink_status.str_model, value);
        (hilink_reception.arr_body[12] == 0) ? strcpy_P (str_text, PSTR ("HIGH")) : strcpy_P (str_text, PSTR ("LOW"));
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s output is %s on detection"), hilink_status.str_model, str_text);
        break;

      case 0xa501:  // read MAC address
        //  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17
        // FD FC FB FA A0 00 A5 01 aa bb cc dd ee ff 04 03 02 01
        // header     | len |     |   mac address   |   footer
        sprintf_P (hilink_status.str_mac, PSTR ("%02X:%02X:%02X:%02X:%02X:%02X"), hilink_reception.arr_body[10], hilink_reception.arr_body[11], hilink_reception.arr_body[12], hilink_reception.arr_body[13], hilink_reception.arr_body[14], hilink_reception.arr_body[15]);
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s MAC is %s"), hilink_status.str_model, hilink_status.str_mac);
        break;

      case 0x0b01:    // start auto threshold
        hilink_status.progress = 0;
        if (hilink_reception.arr_body[8] == 0x00) AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s auto level will start"), hilink_status.str_model);
          else AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s auto level error"), hilink_status.str_model);
        break;
    }
  }

  // declare command as finished
  HilinkCommandFinished ();
}

// Handling of received data (normal mode or engineering mode)
void LD2410HandleReceivedData ()
{
  uint8_t index;

  // if in debug mode, log data to console
  HilinkLogHexa (hilink_reception.arr_body, hilink_reception.idx_body, false, false);

  // analyse data content
  switch (hilink_reception.arr_body[8])
  {
    case 0x00:      // no detection
      hilink_motion.arr_target[0].active = false;
      hilink_static.arr_target[0].active = false;
      break;

    case 0x01:      // motion detected
      hilink_motion.arr_target[0].active = true;
      hilink_static.arr_target[0].active = false;
      break;

    case 0x02:      // static person detected
      hilink_motion.arr_target[0].active = false;
      hilink_static.arr_target[0].active = true;
      break;

    case 0x03:      // motion and static person detected
      hilink_motion.arr_target[0].active = true;
      hilink_static.arr_target[0].active = true;
      break;

    case 0x04:      // background noise detection running
      hilink_status.progress++;
      if (hilink_status.progress % 10 == 0) AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s auto level running [%u]"), hilink_status.str_model, hilink_status.progress / 10);
      break;

    case 0x05:      // background noise detection successful
      if (hilink_status.progress != UINT8_MAX) AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s auto level successful"), hilink_status.str_model);
      hilink_status.progress = UINT8_MAX;
      break;

    case 0x06:      // background noise detection failed
      if (hilink_status.progress != UINT8_MAX) AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s auto level failed"), hilink_status.str_model);
      hilink_status.progress = UINT8_MAX;
      break;
  }

  // motion
  hilink_motion.arr_target[0].dist  = HilinkConvertSerialToUint16 (hilink_reception.arr_body[9], hilink_reception.arr_body[10]);
  hilink_motion.arr_target[0].power = hilink_reception.arr_body[11];

  // presence
  hilink_static.arr_target[0].dist  = HilinkConvertSerialToUint16 (hilink_reception.arr_body[12], hilink_reception.arr_body[13]);
  hilink_static.arr_target[0].power = hilink_reception.arr_body[14];

  // if energy mode, get motion and presence gate energy
  if (hilink_reception.arr_body[6] == 1)
  {
    for (index = 0; index < hilink_status.gate_qty; index ++) hilink_motion.arr_gate[index].power = hilink_reception.arr_body[LD2410_ENERGY_GATE_MOTION + index];
    for (index = 0; index < hilink_status.gate_qty; index ++) hilink_static.arr_gate[index].power = hilink_reception.arr_body[LD2410_ENERGY_GATE_STATIC + index];
    if (!hilink_status.outdated) hilink_status.light_level = hilink_reception.arr_body[LD2410_ENERGY_LIGHT];
      else hilink_status.light_level = 0;
  }

  // else normal data mode, reset gates energy
  else
  {
    for (index = 0; index < hilink_status.gate_qty; index ++) hilink_motion.arr_gate[index].power = 0;
    for (index = 0; index < hilink_status.gate_qty; index ++) hilink_static.arr_gate[index].power = 0;
    hilink_status.light_level = 0;
  }

  // reset reception buffer
  HilinkReceptionEmpty ();
}

/*********************************************\
 *                   Callback
\*********************************************/

void LD2410ProcessCommandQueue ()
{
  bool    cmnd_mode, cmnd_waiting;
  uint8_t command;
  char    str_command[16];
  char    str_param[16];

  // check validity
  if (hilink_status.pserial == nullptr) return;
  if ((hilink_config.device != HLK_DEVICE_LD2410B) && (hilink_config.device != HLK_DEVICE_LD2410C)) return;
  if (!HilinkCommandPossible ()) return;

  // check if commands are waiting
  cmnd_mode    = HilinkIsInCommandMode ();
  cmnd_waiting = HilinkCommandWaiting ();

  // commands in the pipe and command mode not started, start command mode
  if (cmnd_waiting && !cmnd_mode) LD2410SendCommand (LD2410_CMND_START, nullptr);
    
  // else if no command in the queue, stop command mode
  else if (!cmnd_waiting && cmnd_mode) LD2410SendCommand (LD2410_CMND_STOP, nullptr);

  // else send next command
  else if (cmnd_waiting && cmnd_mode)
  {
    HilinkGetNextCommand (str_command, sizeof (str_command));
    HilinkSplitCommand (str_command, command, str_param, sizeof (str_param));
    LD2410SendCommand (command, str_param);
  }
}

// Handling of serial reception
void LD2410SerialReception ()
{
  uint8_t  index, delta;
  uint32_t value;
    
  // check if enabled
  if (hilink_status.pserial == nullptr) return;
  if ((hilink_config.device != HLK_DEVICE_LD2410B) && (hilink_config.device != HLK_DEVICE_LD2410C)) return;

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
      // calculate value of last 4 digits received
      delta = hilink_reception.idx_body - 4;
      value = 0x1000000 * (uint32_t)hilink_reception.arr_body[delta] + 0x10000 * (uint32_t)hilink_reception.arr_body[delta + 1] + 0x100 * (uint32_t)hilink_reception.arr_body[delta + 2] + (uint32_t)hilink_reception.arr_body[delta + 3];

      // check for header or footer
      switch (value)
      {
        case 0xf4f3f2f1:    // data header
          // if more than 4 digits received, remove everything before header
          if (hilink_reception.idx_body > 4)
          {
            for (index = 0; index < 4; index ++) hilink_reception.arr_body[index] = hilink_reception.arr_body[delta + index];
            hilink_reception.idx_body = 4;
          }
          break;

        case 0xfdfcfbfa:    // command header
          // if more than 4 digits received, remove everything before header
          if (hilink_reception.idx_body > 4)
          {
            for (index = 0; index < 4; index ++) hilink_reception.arr_body[index] = hilink_reception.arr_body[delta + index];
            hilink_reception.idx_body = 4;
          }
          break;

        case 0x04030201:    // command footer
          // handle command result
          LD2410HandleReceivedCommand ();
          break;

        case 0xf8f7f6f5:    // data footer
          // handle data reception
          LD2410HandleReceivedData ();
          break;
      }
    }
  }
}

/***************************************\
 *              Interface
\***************************************/

// LD2410 sensor
bool XsnsHilinkLD2410 (const uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
    case FUNC_COMMAND:
      if ((hilink_config.device == HLK_DEVICE_LD2410B) || (hilink_config.device == HLK_DEVICE_LD2410C)) result = DecodeCommand (kHLKLD2410Commands, HLKLD2410Command);
      break;

    case FUNC_EVERY_250_MSECOND:
      LD2410ProcessCommandQueue ();
      break;

    case FUNC_EVERY_100_MSECOND:
      LD2410SerialReception ();
      break;
  }

  return result;
}

#endif     // USE_HILINK_LD2410
#endif     // USE_HILINK_DETECTOR
