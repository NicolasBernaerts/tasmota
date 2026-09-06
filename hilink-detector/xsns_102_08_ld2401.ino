/*
  xsns_102_08_ld2401.ino - Driver for Movement sensor HLK-LD2401

  Copyright (C) 2026  Nicolas Bernaerts

  Version history :
    28/05/2026 - v1.0 - Creation
    17/07/2026 - v1.1 - Set bluetooth flag persistent

  Connexions :
    * GPIO1 should be declared as LD2410 Tx and connected to HLK-LD2401 Rx
    * GPIO3 should be declared as LD2410 Rx and connected to HLK-LD2401 Tx
 
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
#ifdef USE_HILINK_LD2401

#include <TasmotaSerial.h>

/*************************************************\
 *               Variables
\*************************************************/

// constant
#define LD2401_START_DELAY              5             // sensor startup delay

#define LD2401_DATA_RATE                256000

#define LD2401_GATE_QUANTITY            8             // number of sensor gates
#define LD2401_GATE_WIDTH_LOW           20            // one gate handles 20 cm
#define LD2401_GATE_WIDTH_HIGH          75            // one gate handles 75 cm

#define LD2401_DIST_MAX                 600           // maximum detectable distance (cm)

#define LD2401_ENERGY_GATE_MOTION       19            // index of motion gate level in energy mode 
#define LD2401_ENERGY_GATE_STATIC       29            // index of static gate level in energy mode 

#define LD2401_MINIMUM_FIRMWARE         0x0244        // minimum firmware version not to be outdated

// ----------------
// binary commands
// ----------------

uint8_t ld2401_cmnd_version[]     PROGMEM = { 0xa0, 0x00 };
uint8_t ld2401_cmnd_read_mac[]    PROGMEM = { 0xa5, 0x00, 0x01, 0x00 };

uint8_t ld2401_cmnd_reset[]       PROGMEM = { 0xa2, 0x00, 0x00, 0x00 };
uint8_t ld2401_cmnd_restart[]     PROGMEM = { 0xa3, 0x00 };

uint8_t ld2401_cmnd_energy_on[]   PROGMEM = { 0x62, 0x00 };
uint8_t ld2401_cmnd_energy_off[]  PROGMEM = { 0x63, 0x00 };

uint8_t ld2401_cmnd_set_param[]   PROGMEM = { 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 };
uint8_t ld2401_cmnd_get_param[]   PROGMEM = { 0x61, 0x00 };

uint8_t ld2401_cmnd_set_gate[]    PROGMEM = { 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

uint8_t ld2401_cmnd_bluetooth[]   PROGMEM = { 0xa4, 0x00, 0x00, 0x00 };
uint8_t ld2401_cmnd_password[]    PROGMEM = { 0xa9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

uint8_t ld2401_cmnd_set_width[]    PROGMEM = { 0xaa, 0x00, 0x00, 0x00 };
uint8_t ld2401_cmnd_get_width[]    PROGMEM = { 0xab, 0x00 };

uint8_t ld2401_cmnd_get_light[]   PROGMEM = { 0xae, 0x00 };
uint8_t ld2401_cmnd_set_light[]   PROGMEM = { 0xad, 0x00, 0x00, 0x00, 0x00, 0x00 };

uint8_t ld2401_cmnd_auto_start[]  PROGMEM = { 0x0b, 0x00, 0x0a, 0x00 };
uint8_t ld2401_cmnd_auto_query[]  PROGMEM = { 0x1b, 0x00 };

// list of commands available
enum LD2401ListCommand { LD2401_CMND_NONE, LD2401_CMND_START, LD2401_CMND_STOP, LD2401_CMND_VERSION, LD2401_CMND_RESTART, LD2401_CMND_RESET, LD2401_CMND_GET_PARAM, LD2401_CMND_SET_PARAM, LD2401_CMND_ENERGY_ON, LD2401_CMND_ENERGY_OFF, LD2401_CMND_SET_GATE, LD2401_CMND_READ_MAC, LD2401_CMND_BLUETOOTH, LD2401_CMND_PASSWORD, LD2401_CMND_SET_WIDTH, LD2401_CMND_GET_WIDTH, LD2401_CMND_SET_LIGHT, LD2401_CMND_GET_LIGHT, LD2401_CMND_AUTO_START, LD2401_CMND_AUTO_QUERY, LD2401_CMND_MAX };

// MQTT commands
const char kHLKLD2401Commands[]         PROGMEM = "hlk" "|" "_restart" "|"    "_reset"   "|"    "_energy"   "|"       "_bt"       "|"    "_gate"   "|"    "_auto"   "|"    "_width"   "|"    "_light"   "|"      "_out"       ;
void (* const HLKLD2401Command[])(void) PROGMEM = {   &CmndLD2401Restart, &CmndLD2401Reset, &CmndLD2401Energy, &CmndLD2401Bluetooth, &CmndLD2401Gate, &CmndLD2401Auto, &CmndLD2401Width, &CmndLD2401Light, &CmndLD2401Output };

/*****************************\
 *          Commands
\*****************************/

void CmndLD2401Restart ()
{
  HilinkAppendCommand (LD2401_CMND_RESTART);
  ResponseCmndDone ();
}

void CmndLD2401Reset ()
{
  HilinkAppendCommand (LD2401_CMND_RESET);
  ResponseCmndDone ();
}

void CmndLD2401Auto ()
{
  HilinkAppendCommand (LD2401_CMND_AUTO_START);
  ResponseCmndDone ();
}

void CmndLD2401Energy ()
{
  // if gain data is available
  if (XdrvMailbox.data_len > 0)
  {
    // append command
    if (XdrvMailbox.payload == 0) HilinkAppendCommand (LD2401_CMND_ENERGY_OFF);
      else HilinkAppendCommand (LD2401_CMND_ENERGY_ON);

    // command handled
    ResponseCmndDone ();
  }

  // command handled
  else ResponseCmndFailed ();
}

void CmndLD2401Bluetooth ()
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
    HilinkAppendCommand (LD2401_CMND_BLUETOOTH, action);

    // if bluetooth password is provided and 6 digits long
    if (pstr_pwd != nullptr)
    {
      if (strlen (pstr_pwd) == 6) HilinkAppendCommand (LD2401_CMND_PASSWORD, pstr_pwd);
    }

    // append device restart
    HilinkAppendCommand (LD2401_CMND_RESTART);

    ResponseCmndDone ();
  }
  else ResponseCmndFailed ();
}

void CmndLD2401Width ()
{
  bool    is_ok = true;
  uint8_t width;

  if (XdrvMailbox.data_len > 0)
  {
    // get gate width
    width = (uint8_t)atoi (XdrvMailbox.data);
    is_ok = ((width == LD2401_GATE_WIDTH_LOW) || (width == LD2401_GATE_WIDTH_HIGH));

    if (is_ok) HilinkAppendCommand (LD2401_CMND_SET_WIDTH, width);
  }
  else HilinkAppendCommand (LD2401_CMND_GET_WIDTH);

  // response
  (is_ok) ? ResponseCmndDone () : ResponseCmndFailed ();
}

void CmndLD2401Gate ()
{
  bool     is_ok = false;
  int      gate = INT_MAX;
  uint8_t  level;
  uint32_t value;
  char    *pstr_presence;
  char    *pstr_motion;
  char     str_data[16];

  if (XdrvMailbox.data_len > 0)
  {
    // get gate number
    strcpy (str_data, XdrvMailbox.data);
    pstr_presence = strchr (str_data, ' ');
    if (pstr_presence != nullptr) *pstr_presence++ = 0;
    
    // get gate target
    gate = atoi (str_data) - 1;
    is_ok = ((gate < hilink_status.gate_qty) && (pstr_presence != nullptr));
  }

  // if parameters are provided
  if (is_ok)
  {
      // extract data
      pstr_motion = strchr (pstr_presence, ',');
      if (pstr_motion != nullptr) *pstr_motion++ = 0;

      // set presence threshold
      level = (uint8_t)atoi (pstr_presence);
      hilink_static.arr_gate[gate].threshold = level;

      // set motion threshold
      if (pstr_motion != nullptr) level = (uint8_t)atoi (pstr_motion);
      hilink_motion.arr_gate[gate].threshold = level;

      // update gate parameters
      HilinkAppendCommand (LD2401_CMND_SET_GATE, gate);
  }

  // response
  (is_ok) ? ResponseCmndDone () : ResponseCmndFailed ();
}

// command : hlk_light state threshold
void CmndLD2401Light ()
{
  bool  is_ok     = true;
  int   threshold = INT_MAX;
  int   action    = -1;
  char *pstr_thres;
  char  str_data[16];
  char  str_action[8];

  // if no parameter provided, get light parameters
  if (XdrvMailbox.data_len == 0) HilinkAppendCommand (LD2401_CMND_GET_LIGHT);

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
      HilinkAppendCommand (LD2401_CMND_SET_LIGHT);
    }
  }

  // command done
  (is_ok) ? ResponseCmndDone () : ResponseCmndFailed ();
}

void CmndLD2401Output ()
{
  // if output level is provided
  if (XdrvMailbox.data_len > 0)
  {
    // append command
    (XdrvMailbox.payload == 1) ? hilink_config.param.output = 0 : hilink_config.param.output = 1; 
    HilinkAppendCommand (LD2401_CMND_SET_LIGHT);

    // command handled
    ResponseCmndDone ();
  }

  // command handled
  else ResponseCmndFailed ();
}

/******************************\
 *       Common commands
\******************************/

void LD2401DeviceCommand (const uint8_t command, const uint8_t context)
{
  // handle different common commands
  switch (command)
  {
    // display device help
    case HILINK_CMND_HELP:
      AddLog (LOG_LEVEL_INFO, PSTR ("%s specific commands :"), hilink_status.str_model);
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_restart        = restart sensor"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_reset          = reset sensor"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_auto           = auto calibrate sensitivity"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_energy <0/1>   = set energy mode (default)"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_delay <val>    = detection delay (sec.)"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_width <20/75>  = gate width (cm)"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_bt <0/1> <pwd> = bluetooth status and password"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_out <0/1>      = output level on detection"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_gate gate pres,motion = gate sensitivity (%)"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    gate   : 1 .. %u"), hilink_status.gate_qty);
      AddLog (LOG_LEVEL_INFO, PSTR ("    pres   : 0 .. 100"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    motion : 0 .. 100"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_light <conf> <thres>  = light sensitivity control"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    conf  : off             disabled"));
      AddLog (LOG_LEVEL_INFO, PSTR ("            below           detection on low light"));
      AddLog (LOG_LEVEL_INFO, PSTR ("            above           detection on high light"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    thres : trigger level   0..100%%"));
      break;

    // init device parameters
    case HILINK_CMND_INIT:
      // declare device
      hilink_status.baudrate   = LD2401_DATA_RATE;
      hilink_status.gate_qty   = LD2401_GATE_QUANTITY;
      hilink_status.gate_width = LD2401_GATE_WIDTH_HIGH;
      hilink_status.dist_limit = LD2401_DIST_MAX;

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
      HilinkSetCommandDelay (LD2401_START_DELAY);

      // get device general config
      HilinkAppendCommand (LD2401_CMND_VERSION);
      HilinkAppendCommand (LD2401_CMND_READ_MAC);
      HilinkAppendCommand (LD2401_CMND_GET_WIDTH);

      // set parameters
      HilinkAppendCommand (LD2401_CMND_SET_PARAM);

      // set bluetooth and energy mode
      HilinkAppendCommand (LD2401_CMND_BLUETOOTH, hilink_config.param.bluetooth);
      HilinkAppendCommand (LD2401_CMND_ENERGY_ON);
      break;

    // read device data information
    case HILINK_CMND_INFO:
      HilinkAppendCommand (LD2401_CMND_VERSION);
      HilinkAppendCommand (LD2401_CMND_READ_MAC);
      break;
      
    // read device parameters
    case HILINK_CMND_PARAM:
      HilinkAppendCommand (LD2401_CMND_GET_PARAM);
      HilinkAppendCommand (LD2401_CMND_GET_WIDTH);
      break;
      
    // set presence-less delay
    case HILINK_CMND_DELAY:
      HilinkAppendCommand (LD2401_CMND_SET_PARAM);
      break;
      
    // set minimum detection distance
    case HILINK_CMND_DMIN:
      break;

    // set maximum detection distance
    case HILINK_CMND_DMAX:
      HilinkAppendCommand (LD2401_CMND_SET_PARAM);
      break;
    
    // append JSON according to context
    case HILINK_CMND_JSON:
      switch (context)
      {
        case HILINK_JSON_GENERAL:  ResponseAppend_P (PSTR (",\"Dist\":%u"), HilinkGetDistance (HILINK_JSON_GENERAL)); break;
        case HILINK_JSON_PRESENCE: ResponseAppend_P (PSTR (",\"Dist\":%u,\"Power\":%u"), HilinkGetDistance (HILINK_JSON_PRESENCE), HilinkGetPower (HILINK_JSON_PRESENCE)); break;
        case HILINK_JSON_MOTION:   ResponseAppend_P (PSTR (",\"Dist\":%u,\"Power\":%u"), HilinkGetDistance (HILINK_JSON_MOTION), HilinkGetPower (HILINK_JSON_MOTION)); break;
      } 
      break;
  }
}

/*********************************\
 *            Functions
\*********************************/

/*
// device initial commands
void LD2401AppendInitCommand ()
{
  // default config at startup
  hilink_reception.data   = true;
  hilink_reception.energy = false;
  hilink_command.mode     = false;

  // set initial delay
  HilinkSetCommandDelay (LD2401_START_DELAY);

  // query device data
  LD2401DeviceCommand (HILINK_CMND_INFO, 0);

  // get gate width
  HilinkAppendCommand (LD2401_CMND_GET_WIDTH);

  // add init commands
  HilinkAppendCommand (LD2401_CMND_BLUETOOTH, hilink_config.param.bluetooth);

  // set boundaries
  HilinkAppendCommand (LD2401_CMND_SET_PARAM);
  
  // set engineering mode output
  HilinkAppendCommand (LD2401_CMND_ENERGY_ON);
}
  */

/*********************************************\
 *             Communication
\*********************************************/

// send command
void LD2401SendCommand (const uint8_t command, char *pstr_param)
{
  uint8_t param;
  size_t  index, limit;
  size_t  body;
  uint8_t arr_buffer[24];

  // check sensor presence
  if (hilink_status.pserial == nullptr) return;

  // get parameter
  if (pstr_param == nullptr) param = 0;
    else param = (uint8_t)atoi (pstr_param);

  // add command body
  body = 0;
  switch (command)
  {
    case LD2401_CMND_START:
      body = sizeof (hilink_cmnd_start);
      memcpy_P (arr_buffer, hilink_cmnd_start, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set command ON"), hilink_status.str_model);
      break;

    case LD2401_CMND_STOP:
      body = sizeof (hilink_cmnd_stop);
      memcpy_P (arr_buffer, hilink_cmnd_stop, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set command OFF"), hilink_status.str_model);
      break;

    case LD2401_CMND_VERSION:
      body = sizeof (ld2401_cmnd_version);
      memcpy_P (arr_buffer, ld2401_cmnd_version, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s get version"), hilink_status.str_model);
      break;

    case LD2401_CMND_RESTART:
      body = sizeof (ld2401_cmnd_restart);
      memcpy_P (arr_buffer, ld2401_cmnd_restart, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s restart"), hilink_status.str_model);

      // prepare init commands after restart
      HilinkDeviceCommand (HILINK_CMND_FIRST);
      break;

    case LD2401_CMND_RESET:
      body = sizeof (ld2401_cmnd_reset);
      memcpy_P (arr_buffer, ld2401_cmnd_reset, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s reset"), hilink_status.str_model);

      // ask for device restart
      HilinkSetCommandDelay (LD2401_START_DELAY);
      HilinkAppendCommand (LD2401_CMND_RESTART);
      break;

    case LD2401_CMND_ENERGY_ON:
      body = sizeof (ld2401_cmnd_energy_on);
      memcpy_P (arr_buffer, ld2401_cmnd_energy_on, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s engineering mode ON"), hilink_status.str_model);
      break;

    case LD2401_CMND_ENERGY_OFF:
      body = sizeof (ld2401_cmnd_energy_off);
      memcpy_P (arr_buffer, ld2401_cmnd_energy_off, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s engineering mode OFF"), hilink_status.str_model);
      break;

    case LD2401_CMND_GET_PARAM:
      body = sizeof (ld2401_cmnd_get_param);
      memcpy_P (arr_buffer, ld2401_cmnd_get_param, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s get parameters"), hilink_status.str_model);
      break;

    case LD2401_CMND_SET_PARAM:
      body = sizeof (ld2401_cmnd_set_param);
      memcpy_P (arr_buffer, ld2401_cmnd_set_param, body);
      arr_buffer[4]  = 1 + HilinkMotionGateMax ();
      arr_buffer[10] = 1 + HilinkPresenceGateMax ();
      arr_buffer[16] = (uint8_t)(hilink_config.delay % 256);
      arr_buffer[17] = (uint8_t)(hilink_config.delay / 256);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set parameters"), hilink_status.str_model);
      break;

    case LD2401_CMND_SET_GATE:
      body = sizeof (ld2401_cmnd_set_gate);
      memcpy_P (arr_buffer, ld2401_cmnd_set_gate, body);
      arr_buffer[2]  = 0x00; arr_buffer[4]  = param;
      arr_buffer[8]  = 0x01; arr_buffer[10] = hilink_motion.arr_gate[param].threshold;
      arr_buffer[14] = 0x02; arr_buffer[16] = hilink_static.arr_gate[param].threshold;
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set gate %u parameters"), hilink_status.str_model, param + 1);
      break;

    case LD2401_CMND_READ_MAC:
      body = sizeof (ld2401_cmnd_read_mac);
      memcpy_P (arr_buffer, ld2401_cmnd_read_mac, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s read MAC address"), hilink_status.str_model);
      break;

    case LD2401_CMND_BLUETOOTH:
      body = sizeof (ld2401_cmnd_bluetooth);
      memcpy_P (arr_buffer, ld2401_cmnd_bluetooth, body);
      arr_buffer[2] = param;
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set bluetooth"), hilink_status.str_model);
      break;

    case LD2401_CMND_PASSWORD:
      body = sizeof (ld2401_cmnd_password);
      memcpy_P (arr_buffer, ld2401_cmnd_password, body);
      limit = (size_t)min (strlen (pstr_param), (size_t)6);
      for (index = 0; index < limit; index ++) arr_buffer[2 + index] = pstr_param[index];
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set password to %s"), hilink_status.str_model, pstr_param);
      break;

    case LD2401_CMND_GET_WIDTH:
      body = sizeof (ld2401_cmnd_get_width);
      memcpy_P (arr_buffer, ld2401_cmnd_get_width, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s get gate width"), hilink_status.str_model);
      break;

    case LD2401_CMND_SET_WIDTH:
      body = sizeof (ld2401_cmnd_set_width);
      memcpy_P (arr_buffer, ld2401_cmnd_set_width, body);
      if (param == LD2401_GATE_WIDTH_LOW) arr_buffer[2] = 0x01;
        else arr_buffer[2] = 0x00;
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set gate width to %u cm"), hilink_status.str_model, param);
      break;

    case LD2401_CMND_GET_LIGHT:
      body = sizeof (ld2401_cmnd_get_light);
      memcpy_P (arr_buffer, ld2401_cmnd_get_light, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s get light sensitivity config"), hilink_status.str_model);
      break;

    case LD2401_CMND_SET_LIGHT:
      body = sizeof (ld2401_cmnd_set_light);
      memcpy_P (arr_buffer, ld2401_cmnd_set_light, body);
      arr_buffer[2] = hilink_config.param.light;
      arr_buffer[3] = hilink_config.light_thres;
      arr_buffer[4] = hilink_config.param.output;
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set light config"), hilink_status.str_model);
      break;


    case LD2401_CMND_AUTO_START:
      body = sizeof (ld2401_cmnd_auto_start);
      memcpy_P (arr_buffer, ld2401_cmnd_auto_start, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s start auto gain"), hilink_status.str_model);
      break;

    case LD2401_CMND_AUTO_QUERY:
      body = sizeof (ld2401_cmnd_auto_query);
      memcpy_P (arr_buffer, ld2401_cmnd_auto_query, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s auto gain progress"), hilink_status.str_model);
      break;
  }

  // if command defined, send it
  if (body > 0)
  {
    HilinkCommandStarted (command, param);
    HilinkSendCommand (arr_buffer, body);
  }
}

// Handling of received command
void LD2401HandleReceivedCommand ()
{
  uint8_t  index, gate;
  uint16_t dist_pres, dist_move, delay, value;
  uint16_t command;
  char     str_text[16];

  // log data to console
  HilinkLogHexa (hilink_reception.arr_body, hilink_reception.idx_body, true, false);

  // if enough data received
  if (hilink_reception.idx_body >= 12)
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

      case 0x6001:    // set param
        HilinkAppendCommand (LD2401_CMND_GET_PARAM);
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s parameters set"), hilink_status.str_model);
        break;

      case 0x6101:    // get param
        // get max motion distance
        dist_move = (uint16_t)hilink_reception.arr_body[12] * hilink_status.gate_width;

        // get max presence distance
        dist_pres = (uint16_t)hilink_reception.arr_body[13] * hilink_status.gate_width;

        // get motion gates threshold
        for (gate = 0; gate < hilink_status.gate_qty; gate ++) hilink_motion.arr_gate[gate].threshold = hilink_reception.arr_body[14 + gate];

        // get static gates threshold
        for (gate = 0; gate < hilink_status.gate_qty; gate ++) hilink_static.arr_gate[gate].threshold = hilink_reception.arr_body[23 + gate];

        // get delay
        delay = HilinkConvertSerialToUint16 (hilink_reception.arr_body[32], hilink_reception.arr_body[33]);

        // log
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s max static distance is %u cm"), hilink_status.str_model, dist_pres);
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s max motion distance is %u cm"), hilink_status.str_model, dist_move);
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s delay is %u sec"), hilink_status.str_model, delay);
        break;

      case 0x6201:    // engineering mode on
        hilink_reception.energy = true;
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s data in energy mode"), hilink_status.str_model);
        break;

     case 0x6301:    // engineering mode off
        hilink_reception.energy = false;
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s data in basic mode"), hilink_status.str_model);
        break;

     case 0x6401:    // set gate sensitivity
        HilinkAppendCommand (LD2401_CMND_GET_PARAM);
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s Gate sensitivity set"), hilink_status.str_model);
        break;

      case 0xa001:    // read firmware version
        sprintf_P (str_text, PSTR ("%x.%02x.%02x%02x%02x%02x"), hilink_reception.arr_body[13], hilink_reception.arr_body[12], hilink_reception.arr_body[17], hilink_reception.arr_body[16], hilink_reception.arr_body[15], hilink_reception.arr_body[14]);
        hilink_status.str_firmware = str_text;
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s firmware %s"), hilink_status.str_model, str_text);

        // check for outdated firmware
        value = 256 * (uint16_t)hilink_reception.arr_body[13] + (uint16_t)hilink_reception.arr_body[12];
        hilink_status.outdated = (value < LD2401_MINIMUM_FIRMWARE);
        if (hilink_status.outdated) AddLog (LOG_LEVEL_INFO, PSTR ("HLK: WARNING your firmware %s is too old ! Update to get full features"), hilink_status.str_firmware.c_str ());
        break;

      case 0xa201:    // reset to factory
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s reset to factory"), hilink_status.str_model);
        break;

      case 0xa301:    // restart module
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s restart"), hilink_status.str_model);
        break;

      case 0xa401:    // set bluetooth
        hilink_config.param.bluetooth = hilink_command.param;
        if (hilink_config.param.bluetooth == 0) AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s bluetooth is %s"), hilink_status.str_model, PSTR ("OFF"));
          else AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s bluetooth is %s"), hilink_status.str_model, PSTR ("ON"));
        break;

      case 0xa901:    // set bluetooth password
        HilinkAppendCommand (LD2401_CMND_RESTART);
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s bluetooth password changed"), hilink_status.str_model);
        break;

      case 0xa501:  // read MAC address
        //  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17
        // FD FC FB FA A0 00 A5 01 aa bb cc dd ee ff 04 03 02 01
        // header     | len |     |   mac address   |   footer
        sprintf_P (hilink_status.str_mac, PSTR ("%02X:%02X:%02X:%02X:%02X:%02X"), hilink_reception.arr_body[10], hilink_reception.arr_body[11], hilink_reception.arr_body[12], hilink_reception.arr_body[13], hilink_reception.arr_body[14], hilink_reception.arr_body[15]);
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s MAC is %s"), hilink_status.str_model, hilink_status.str_mac);
        break;

      case 0xaa01:    // set gate width
        HilinkAppendCommand (LD2401_CMND_GET_WIDTH);
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s gate width set"), hilink_status.str_model);
        break;

      case 0xab01:    // get gate width
        if (hilink_reception.arr_body[10] == 0x00) hilink_status.gate_width = LD2401_GATE_WIDTH_HIGH;
          else if (hilink_reception.arr_body[10] == 0x01) hilink_status.gate_width = LD2401_GATE_WIDTH_LOW;
        hilink_status.dist_limit = hilink_status.gate_width * LD2401_GATE_QUANTITY;
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s gate width is %u cm"), hilink_status.str_model, hilink_status.gate_width);
        break;

      case 0xad01:    // set light config
        HilinkAppendCommand (LD2401_CMND_GET_LIGHT);
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

      case 0x0b01:    // start auto noise adustment
        hilink_status.progress = 0;
        HilinkSetCommandDelay (1);
        HilinkAppendCommand (LD2401_CMND_AUTO_QUERY);
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s auto level started"), hilink_status.str_model);
        break;

      case 0x1b01:    // query auto noise adustment progress
        if (hilink_reception.arr_body[10] == 0x01)
        {
          hilink_status.progress++;
          HilinkSetCommandDelay (1);
          HilinkAppendCommand (LD2401_CMND_AUTO_QUERY);
          AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s auto level running [%u s]"), hilink_status.str_model, hilink_status.progress);
        }
        else
        {
          hilink_status.progress = UINT8_MAX;
          AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s auto level finished"), hilink_status.str_model);
        }
        break;
      }
  }

  // declare command as finished
  HilinkCommandFinished ();
}

// Handling of received data
void LD2401HandleReceivedData ()
{
  uint8_t target, gate;

  // log data to console
  HilinkLogHexa (hilink_reception.arr_body, hilink_reception.idx_body, false, false);

  // detect data mode
  hilink_reception.energy = (hilink_reception.arr_body[6] == 0x01);

  // handle target status
  target = hilink_reception.arr_body[8];
  hilink_static.arr_target[0].active = ((target == 0x02) || (target == 0x03));
  hilink_motion.arr_target[0].active = ((target == 0x01) || (target == 0x03));

  // motion
  hilink_motion.arr_target[0].dist  = HilinkConvertSerialToUint16 (hilink_reception.arr_body[9],  hilink_reception.arr_body[10]);
  hilink_motion.arr_target[0].power = hilink_reception.arr_body[11];

  // presence
  hilink_static.arr_target[0].dist  = HilinkConvertSerialToUint16 (hilink_reception.arr_body[12], hilink_reception.arr_body[13]);
  hilink_static.arr_target[0].power = hilink_reception.arr_body[14];

  // if in engineering mode, get energy levels
  if (hilink_reception.energy)
  {
    for (gate = 0; gate < 7; gate ++) hilink_motion.arr_gate[gate].power = hilink_reception.arr_body[LD2401_ENERGY_GATE_MOTION + gate];
    for (gate = 0; gate < 7; gate ++) hilink_static.arr_gate[gate].power = hilink_reception.arr_body[LD2401_ENERGY_GATE_STATIC + gate];
  }

  // reset reception buffer
  HilinkReceptionEmpty ();
}

/*********************************************\
 *                   Callback
\*********************************************/

void LD2401ProcessCommandQueue ()
{
  bool    cmnd_mode, cmnd_waiting;
  uint8_t command, param;
  char    str_command[16];
  char    str_param[12];

  // check validity
  if (hilink_config.device != HLK_DEVICE_LD2401) return;
  if (hilink_status.pserial == nullptr) return;
  if (!HilinkCommandPossible ()) return;

  // check if commands are waiting
  cmnd_mode    = HilinkIsInCommandMode ();
  cmnd_waiting = HilinkCommandWaiting ();

  // commands in the pipe and command mode not started, start command mode
  if (cmnd_waiting && !cmnd_mode) LD2401SendCommand (LD2401_CMND_START, nullptr);
    
  // else if no command in the queue, stop command mode
  else if (!cmnd_waiting && cmnd_mode) LD2401SendCommand (LD2401_CMND_STOP, nullptr);

  // else send next command
  else if (cmnd_waiting && cmnd_mode)
  {
    HilinkGetNextCommand (str_command, sizeof (str_command));
    HilinkSplitCommand (str_command, command, str_param, sizeof (str_param));
    LD2401SendCommand (command, str_param);
  }
}

// Handling of serial reception
void LD2401SerialReception ()
{
  uint8_t  index, delta;
  uint32_t value;
    
  // check if enabled
  if (hilink_config.device != HLK_DEVICE_LD2401) return;
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

      // check for header or footer
      switch (value)
      {
        case 0xfdfcfbfa:    // command header
        case 0xf4f3f2f1:    // data header
          // if more than 4 digits received, remove everything before header
          if (hilink_reception.idx_body > 4)
          {
            for (index = 0; index < 4; index ++) hilink_reception.arr_body[index] = hilink_reception.arr_body[delta + index];
            hilink_reception.idx_body = 4;
          }
          break;

        case 0x04030201:    // command footer
          LD2401HandleReceivedCommand ();
          break;

        case 0xf8f7f6f5:    // data footer
          LD2401HandleReceivedData ();
          break;
      }
    }
  }
}

/***************************************\
 *              Interface
\***************************************/

// LD2401 sensor
bool XsnsHilinkLD2401 (const uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
    case FUNC_COMMAND:
      if (hilink_config.device == HLK_DEVICE_LD2401) result = DecodeCommand (kHLKLD2401Commands, HLKLD2401Command);
      break;

    case FUNC_EVERY_100_MSECOND:
      LD2401SerialReception ();
      LD2401ProcessCommandQueue ();
      break;
  }

  return result;
}

#endif     // USE_HILINK_LD2401
#endif     // USE_HILINK_DETECTOR
