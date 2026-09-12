/*
  xsns_102_06_ld2410s.ino - Driver for Presence and Movement sensor HLK-LD2410S

  Copyright (C) 2026  Nicolas Bernaerts

  Version history :
    15/05/2026 - v1.0 - Creation

  Connexions :
    * GPIO1 should be declared as LD2410 Tx and connected to HLK-LD2410S Rx
    * GPIO3 should be declared as LD2410 Rx and connected to HLK-LD2410S Tx

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
#ifdef USE_HILINK_LD2410S

#include <TasmotaSerial.h>

/*************************************************\
 *               Variables
\*************************************************/

// constant
#define LD2410S_START_DELAY             5        // sensor startup delay


#define LD2410S_GATE_QUANTITY           16        // number of sensor gates
#define LD2410S_GATE_WIDTH              70        // one gate handles 70 cm

#define LD2410S_DIST_MAX                1120      // maximum detectable distance (cm)

#define LD2410S_DATA_RATE               115200

#define LD2410S_AUTO_TRIGGER            2         // auto detection default trigger
#define LD2410S_AUTO_RETENTION          1         // auto detection default retention
#define LD2410S_AUTO_SCANNING           60        // auto detection default scanning time

#define LD2410S_ENERGY_GATE             12        // index of gate level in energy mode 

// ----------------
// binary commands
// ----------------

uint8_t ld2410s_cmnd_reset[]         PROGMEM = { 0xa2, 0x00, 0x00, 0x00 };
uint8_t ld2410s_cmnd_restart[]       PROGMEM = { 0xa3, 0x00 };

uint8_t ld2410s_cmnd_read_firmware[] PROGMEM = { 0x00, 0x00 };
uint8_t ld2410s_cmnd_energy[]        PROGMEM = { 0x7a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

uint8_t ld2410s_cmnd_get_serial[]    PROGMEM = { 0x11, 0x00 };
uint8_t ld2410s_cmnd_set_serial[]    PROGMEM = { 0x10, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

uint8_t ld2410s_cmnd_set_param[]     PROGMEM = { 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
uint8_t ld2410s_cmnd_get_param[]     PROGMEM = { 0x71, 0x00, 0x00, 0x00 };
uint8_t ld2410s_cmnd_set_trigger[]   PROGMEM = { 0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
uint8_t ld2410s_cmnd_get_trigger[]   PROGMEM = { 0x73, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00, 0x05, 0x00, 0x06, 0x00, 0x07, 0x00, 0x08, 0x00, 0x09, 0x00, 0x0a, 0x00, 0x0b, 0x00, 0x0c, 0x00, 0x0d, 0x00, 0x0e, 0x00, 0x0f, 0x00 };
uint8_t ld2410s_cmnd_set_hold[]      PROGMEM = { 0x76, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
uint8_t ld2410s_cmnd_get_hold[]      PROGMEM = { 0x77, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00, 0x05, 0x00, 0x06, 0x00, 0x07, 0x00, 0x08, 0x00, 0x09, 0x00, 0x0a, 0x00, 0x0b, 0x00, 0x0c, 0x00, 0x0d, 0x00, 0x0e, 0x00, 0x0f, 0x00 };

uint8_t ld2410s_cmnd_auto_start[]    PROGMEM = { 0x09, 0x00, 0x02, 0x00, 0x01, 0x00, 0x78, 0x00 };

// list of commands available
enum LD2410sListCommand { LD2410S_CMND_NONE, LD2410S_CMND_START, LD2410S_CMND_STOP, LD2410S_CMND_VERSION, LD2410S_CMND_RESET, LD2410S_CMND_RESTART, LD2410S_CMND_ENERGY, LD2410S_CMND_GET_SERIAL, LD2410S_CMND_SET_SERIAL, LD2410S_CMND_GET_PARAM, LD2410S_CMND_SET_PARAM, LD2410S_CMND_GET_TRIGGER, LD2410S_CMND_SET_TRIGGER, LD2410S_CMND_GET_HOLD, LD2410S_CMND_SET_HOLD, LD2410S_CMND_AUTO_START, LD2410S_CMND_MAX };

// MQTT commands
const char kHLKLD2410sCommands[]         PROGMEM = "hlk" "|" "_reset" "|"    "_restart"    "|"    "_serial"    "|"    "_freq"    "|"    "_gate"    "|"     "_energy"   "|"    "_auto"    ;
void (* const HLKLD2410sCommand[])(void) PROGMEM = {  &CmndLD2410sReset, &CmndLD2410sRestart , &CmndLD2410sSerial, &CmndLD2410sFreq, &CmndLD2410sGate, &CmndLD2410sEnergy, &CmndLD2410sAuto};

/*****************************\
 *          Commands
\*****************************/

void CmndLD2410sVersion ()
{
  HilinkAppendCommand (LD2410S_CMND_VERSION);
  ResponseCmndDone ();
}

void CmndLD2410sReset ()
{
  HilinkAppendCommand (LD2410S_CMND_RESET);
  ResponseCmndDone ();
}

void CmndLD2410sRestart ()
{
  HilinkAppendCommand (LD2410S_CMND_RESTART);
  ResponseCmndDone ();
}

void CmndLD2410sAuto ()
{
  // send command with or without parameters
  if (XdrvMailbox.data_len > 0) HilinkAppendCommand (LD2410S_CMND_AUTO_START, XdrvMailbox.data);
    else HilinkAppendCommand (LD2410S_CMND_AUTO_START);

  // command done
  ResponseCmndDone ();
}

void CmndLD2410sSerial ()
{
  int  index;
  char str_serial[10];

  if (XdrvMailbox.data_len > 0)
  {
    memcpy (str_serial, 0, sizeof (str_serial));
    strlcpy (str_serial, XdrvMailbox.data, 9);
    for (index = XdrvMailbox.data_len; index < 8; index ++) str_serial[index] = ' ';
    HilinkAppendCommand (LD2410S_CMND_SET_SERIAL, str_serial);
  }
  else HilinkAppendCommand (LD2410S_CMND_GET_SERIAL);

  ResponseCmndDone ();
}

void CmndLD2410sEnergy ()
{
  int  index, length;
  char str_data[16];

  if (XdrvMailbox.data_len > 0)
  {
    if (XdrvMailbox.payload == 0) HilinkAppendCommand (LD2410S_CMND_ENERGY, 0x00);
      else HilinkAppendCommand (LD2410S_CMND_ENERGY, 0x01);
    ResponseCmndDone ();
  }
  else ResponseCmndFailed ();
}

// command : hilink_gate gate trigger,hold = gate sensitivity (%)
void CmndLD2410sFreq ()
{
  bool     is_ok;
  uint8_t  gate, frequency;
  uint32_t value;
  char    *pstr_digit;
  char    *pstr_hold;
  char     str_data[8];

  is_ok = (XdrvMailbox.data_len > 0);
  if (is_ok)
  {
    // get gate number
    strcpy (str_data, XdrvMailbox.data);
    pstr_digit = strchr (str_data, '.');
    if (pstr_digit != nullptr) *pstr_digit++ = 0;
    
    // extract trigger
    frequency = 10 * (uint8_t)atoi (str_data);

    // extract holding
    if (pstr_digit != nullptr) frequency += (uint8_t)atoi (pstr_digit);

    // update gate parameters
    HilinkAppendCommand (LD2410S_CMND_SET_PARAM, 0x02, frequency);
    HilinkAppendCommand (LD2410S_CMND_SET_PARAM, 0x0c, frequency);
  }

  // else read params
  else
  {
    HilinkAppendCommand (LD2410S_CMND_GET_PARAM, 0x02);
    HilinkAppendCommand (LD2410S_CMND_GET_PARAM, 0x0c);
  }

  // command done
  ResponseCmndDone ();
}

// command : hilink_gate gate trigger,hold = gate sensitivity (%)
void CmndLD2410sGate ()
{
  bool     is_ok;
  uint8_t  gate, level;
  uint32_t value;
  char    *pstr_data;
  char    *pstr_hold;
  char     str_data[16];

  is_ok = (XdrvMailbox.data_len > 0);
  if (is_ok)
  {
    // get gate number
    strcpy (str_data, XdrvMailbox.data);
    pstr_data = strchr (str_data, ' ');
    if (pstr_data != nullptr) *pstr_data++ = 0;
    
    // get gate target
    gate = (uint8_t)(atoi (str_data) - 1);
    is_ok = (gate < hilink_status.gate_qty);
  }

  if (is_ok && (pstr_data != nullptr))
  {
    // extract data
    pstr_hold = strchr (pstr_data, ',');
    if (pstr_hold != nullptr) *pstr_hold++ = 0;

    // extract trigger threshold
    level = (uint8_t)atoi (pstr_data);
    HilinkAppendCommand (LD2410S_CMND_SET_TRIGGER, gate, level);

    // extract hold threshold
    if (pstr_hold != nullptr) level = (uint8_t)atoi (pstr_hold);
    HilinkAppendCommand (LD2410S_CMND_SET_HOLD, gate, level);
  }

  // else read gates
  else
  {
    HilinkAppendCommand (LD2410S_CMND_GET_TRIGGER);
    HilinkAppendCommand (LD2410S_CMND_GET_HOLD);
  }

  // command done
  ResponseCmndDone ();
}

/******************************\
 *       Common commands
\******************************/

void LD2410sDeviceCommand (const uint8_t command, const uint8_t context)
{
   // handle different common commands
  switch (command)
  {
    // display device help
    case HILINK_CMND_HELP:
      AddLog (LOG_LEVEL_INFO, PSTR ("%s specific commands :"), hilink_status.str_model);
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_reset        = reset coefficients to factory"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_restart      = restart device"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_serial <sn>  = set serial number"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_energy <0/1> = set energy mode (default)"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_freq <val>   = update data frequency (Hz)"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    val     : 0.5 .. 8"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_auto time,trig,ret = start auto level detection"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    time : scanning time in sec. [%u]"), LD2410S_AUTO_SCANNING);
      AddLog (LOG_LEVEL_INFO, PSTR ("    trig : trigger factor [%u]"),        LD2410S_AUTO_TRIGGER);
      AddLog (LOG_LEVEL_INFO, PSTR ("    ret  : retention factor [%u]"),      LD2410S_AUTO_RETENTION);
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_gate gate trig,hold = gate sensitivity (%)"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    gate : gate number   1 .. %u"), hilink_status.gate_qty);
      AddLog (LOG_LEVEL_INFO, PSTR ("    trig : trigger level 0 .. 100"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    hold : holding level 0 .. 100"));
      break;

    // init device parameters
    case HILINK_CMND_INIT:
      // declare device
      hilink_status.baudrate   = LD2410S_DATA_RATE;
      hilink_status.gate_qty   = LD2410S_GATE_QUANTITY;
      hilink_status.gate_width = LD2410S_GATE_WIDTH;
      if (hilink_status.dist_limit > LD2410S_DIST_MAX) hilink_status.dist_limit = LD2410S_DIST_MAX;

      // set presence and motion detection status
      hilink_static.enabled = true;
      hilink_motion.enabled = true;

      // no bluetooth available on the device
      hilink_config.param.bluetooth = 0;
      break;

    // device environment initialisation
    case HILINK_CMND_FIRST:
      // default config at startup
      hilink_reception.data   = true;
      hilink_reception.energy = false;
      hilink_command.mode     = false;

      // set initial delay
      HilinkSetCommandDelay (LD2410S_START_DELAY);

      // device data
      HilinkAppendCommand (LD2410S_CMND_VERSION);
      HilinkAppendCommand (LD2410S_CMND_GET_SERIAL);

      // set parameters
      HilinkAppendCommand (LD2410S_CMND_SET_PARAM, 0x06, (long)hilink_config.delay);
      HilinkAppendCommand (LD2410S_CMND_SET_PARAM, 0x0a, (long)HilinkGateMin ());
      HilinkAppendCommand (LD2410S_CMND_SET_PARAM, 0x05, (long)HilinkPresenceGateMax ());

      // general parameters
      HilinkAppendCommand (LD2410S_CMND_GET_PARAM, 0x0a);     // minimum gate
      HilinkAppendCommand (LD2410S_CMND_GET_PARAM, 0x05);     // maximum gate
      HilinkAppendCommand (LD2410S_CMND_GET_PARAM, 0x06);     // nobody delay
      HilinkAppendCommand (LD2410S_CMND_GET_PARAM, 0x02);     // status report frequency
      HilinkAppendCommand (LD2410S_CMND_GET_PARAM, 0x0c);     // distance report frequency
      HilinkAppendCommand (LD2410S_CMND_GET_PARAM, 0x0b);     // response speed

      // gate levels parameters
      HilinkAppendCommand (LD2410S_CMND_GET_TRIGGER);
      HilinkAppendCommand (LD2410S_CMND_GET_HOLD);

      // set energy mode
      HilinkAppendCommand (LD2410S_CMND_ENERGY, 0x01);
      break;

    // read device data information
    case HILINK_CMND_INFO:
      HilinkAppendCommand (LD2410S_CMND_VERSION);
      HilinkAppendCommand (LD2410S_CMND_GET_SERIAL);
      break;
      
    // read device parameters
    case HILINK_CMND_PARAM:
      // general parameters
      HilinkAppendCommand (LD2410S_CMND_GET_PARAM, 0x0a);     // minimum gate
      HilinkAppendCommand (LD2410S_CMND_GET_PARAM, 0x05);     // maximum gate
      HilinkAppendCommand (LD2410S_CMND_GET_PARAM, 0x06);     // nobody delay
      HilinkAppendCommand (LD2410S_CMND_GET_PARAM, 0x02);     // status report frequency
      HilinkAppendCommand (LD2410S_CMND_GET_PARAM, 0x0c);     // distance report frequency
      HilinkAppendCommand (LD2410S_CMND_GET_PARAM, 0x0b);     // response speed

      // gate levels parameters
      HilinkAppendCommand (LD2410S_CMND_GET_TRIGGER);
      HilinkAppendCommand (LD2410S_CMND_GET_HOLD);
      break;
      
    // set presence-less delay
    case HILINK_CMND_DELAY:
      // set acceptable margins
      hilink_config.delay = max (hilink_config.delay, (uint16_t)10);
      hilink_config.delay = min (hilink_config.delay, (uint16_t)120);

      // send command
      HilinkAppendCommand (LD2410S_CMND_SET_PARAM, 0x06, (long)hilink_config.delay);
      break;
      
    // set minimum detection distance
    case HILINK_CMND_DMIN:
      HilinkAppendCommand (LD2410S_CMND_SET_PARAM, 0x0a, (long)HilinkGateMin ());
      break;

    // set maximum detection distance
    case HILINK_CMND_DMAX:
      HilinkAppendCommand (LD2410S_CMND_SET_PARAM, 0x05, (long)HilinkPresenceGateMax ());
      break;
    
    // append JSON according to context
    case HILINK_CMND_JSON:
      switch (context)
      {
        case HILINK_JSON_GENERAL: ResponseAppend_P (PSTR (",\"Dist\":%u"), HilinkGetDistance (HILINK_JSON_GENERAL)); break;
        case HILINK_JSON_MOTION:  ResponseAppend_P (PSTR (",\"Dist\":%u,\"Power\":%u"), HilinkGetDistance (HILINK_JSON_MOTION), HilinkGetPower (HILINK_JSON_MOTION)); break;
      } 
      break;
  }
}

/*********************************\
 *           Functions
\*********************************/

// convert detection level to serial 
long LD2410sConvertLevel2Serial (const long level)
{
  long result;

  result = level * 65535 / 10000;

  return result;
}

// convert energy value to percentage with logarithmic scale
uint8_t LD2410sConvertEnergy2Percent (const uint16_t energy)
{
  uint32_t value;

  value = (uint32_t)energy * 100 / 65535;
  return (uint8_t)value;
}

/*
// device initial commands
void LD2410sAppendInitCommand ()
{
  // default config at startup
  hilink_reception.data   = true;
  hilink_reception.energy = false;
  hilink_command.mode     = false;

  // no bluetooth available on the device
  hilink_config.param.bluetooth = 0;

  // set initial delay
  HilinkSetCommandDelay (LD2410S_START_DELAY);

  // get version
  LD2410sDeviceCommand (HILINK_CMND_INFO, 0);

  // set parameters
  LD2410sDeviceCommand (HILINK_CMND_DMIN, 0);
  LD2410sDeviceCommand (HILINK_CMND_DMAX, 0);
  LD2410sDeviceCommand (HILINK_CMND_DELAY, 0);

  // general parameters
  LD2410sDeviceCommand (HILINK_CMND_PARAM, 0);

  // set energy mode
  HilinkAppendCommand (LD2410S_CMND_ENERGY, 0x01);
}
*/

/*********************************************\
 *             Communication
\*********************************************/

// send command
void LD2410sSendCommand (const uint8_t command, char *pstr_param)
{
//  uint8_t  gate, delay;
  uint8_t  param1 = 0;
  long     param2 = 0;
  long     param3 = 0;
  size_t   index, body;
  uint8_t  arr_buffer[40];
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

  // handle command
  hilink_reception.data = false;

  // handle command
  body = 0;
  switch (command)
  {
    case LD2410S_CMND_START:
      body = sizeof (hilink_cmnd_start);
      memcpy_P (arr_buffer, hilink_cmnd_start, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: LD2410s set command on"));
      break;

    case LD2410S_CMND_STOP:
      body = sizeof (hilink_cmnd_stop);
      memcpy_P (arr_buffer, hilink_cmnd_stop, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: LD2410s set command off"));
      break;

    case LD2410S_CMND_VERSION:
      body = sizeof (ld2410s_cmnd_read_firmware);
      memcpy_P (arr_buffer, ld2410s_cmnd_read_firmware, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: LD2410s get firmware"));
      break;

    case LD2410S_CMND_RESTART:
      body = sizeof (ld2410s_cmnd_restart);
      memcpy_P (arr_buffer, ld2410s_cmnd_restart, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s restart"), hilink_status.str_model);

      // prepare init commands after restart
      HilinkDeviceCommand (HILINK_CMND_FIRST);
      break;

    case LD2410S_CMND_RESET:
      body = sizeof (ld2410s_cmnd_reset);
      memcpy_P (arr_buffer, ld2410s_cmnd_reset, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: LD2410s reset device"));

      // ask for device restart
      HilinkSetCommandDelay (LD2410S_START_DELAY);
      HilinkAppendCommand (LD2410S_CMND_RESTART);
      break;

    case LD2410S_CMND_ENERGY:
      body = sizeof (ld2410s_cmnd_energy);
      memcpy_P (arr_buffer, ld2410s_cmnd_energy, body);
      arr_buffer[4] = param1;
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set data mode to %u"), hilink_status.str_model, param1);
      break;

    case LD2410S_CMND_GET_SERIAL:
      body = sizeof (ld2410s_cmnd_get_serial);
      memcpy_P (arr_buffer, ld2410s_cmnd_get_serial, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s get serial number"), hilink_status.str_model);
      break;

    case LD2410S_CMND_SET_SERIAL:
      body = sizeof (ld2410s_cmnd_set_serial);
      memcpy_P (arr_buffer, ld2410s_cmnd_set_serial, body);
      for (index = 0; index < 8; index++) arr_buffer[4 + index] = pstr_param[index];
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set serial to %s"), hilink_status.str_model, pstr_param);
      break;

    case LD2410S_CMND_GET_PARAM:
      body = sizeof (ld2410s_cmnd_get_param);
      memcpy_P (arr_buffer, ld2410s_cmnd_get_param, body);
      arr_buffer[2] = param1;
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s get param %u"), hilink_status.str_model, param1);
      break;

    case LD2410S_CMND_SET_PARAM:
      // generate command
      body = sizeof (ld2410s_cmnd_set_param);
      memcpy_P (arr_buffer, ld2410s_cmnd_set_param, body);
      arr_buffer[2] = param1;
      arr_buffer[4] = (uint8_t)param2;
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set param %u to %d"), hilink_status.str_model, param1, param2);
      break;

    case LD2410S_CMND_GET_TRIGGER:
      body = sizeof (ld2410s_cmnd_get_trigger);
      memcpy_P (arr_buffer, ld2410s_cmnd_get_trigger, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s get gates trigger level"), hilink_status.str_model);
      break;

    case LD2410S_CMND_SET_TRIGGER:
      body = sizeof (ld2410s_cmnd_set_trigger);
      memcpy_P (arr_buffer, ld2410s_cmnd_set_trigger, body);
      arr_buffer[2] = param1;
      arr_buffer[4] = (uint8_t)param2;
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s gate %u trig set to %d"), hilink_status.str_model, param1, param2);
      break;

    case LD2410S_CMND_GET_HOLD:
      body = sizeof (ld2410s_cmnd_get_hold);
      memcpy_P (arr_buffer, ld2410s_cmnd_get_hold, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s get gates hold level"), hilink_status.str_model);
      break;

    case LD2410S_CMND_SET_HOLD:
      body = sizeof (ld2410s_cmnd_set_hold);
      memcpy_P (arr_buffer, ld2410s_cmnd_set_hold, body);
      arr_buffer[2] = param1;
      arr_buffer[4] = (uint8_t)param2;
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s gate %u hold set to %d"), hilink_status.str_model, param1, param2);
      break;

    case LD2410S_CMND_AUTO_START:
      // set default parameters
      if (param1 == 0) param1 = LD2410S_AUTO_SCANNING;
      if (param2 == 0) param2 = LD2410S_AUTO_TRIGGER;
      if (param3 == 0) param3 = LD2410S_AUTO_RETENTION;

      // prepare command
      body = sizeof (ld2410s_cmnd_auto_start);
      memcpy_P (arr_buffer, ld2410s_cmnd_auto_start, body);
      arr_buffer[2] = (uint8_t)param2;     // trigger
      arr_buffer[4] = (uint8_t)param3;     // retention
      arr_buffer[6] = param1;              // scanning time
      AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s auto level started for %d sec. (trigger %u, retention %d)"), hilink_status.str_model, param1, param2, param3);
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
void LD2410sHandleReceivedCommand ()
{
  uint8_t  index, gate, value;
  uint16_t command, major, minor, patch;
  char     str_value[16];
  char     str_log[68];

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
        hilink_command.mode  = true;
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
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s restarted"), hilink_status.str_model);
        break;

      case 0x0001:    // read firmware
        //  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21
        // FD FC FB FA 08 00 00 01 00 00 00 01 07 01 16 15 09 22 04 03 02 01
        //   header   | len |ty|hd| ack |ftype|mi ma| revision  |  footer
        major = HilinkConvertSerialToUint16 (hilink_reception.arr_body[8],  hilink_reception.arr_body[9]);
        minor = HilinkConvertSerialToUint16 (hilink_reception.arr_body[10], hilink_reception.arr_body[11]);
        patch = HilinkConvertSerialToUint16 (hilink_reception.arr_body[12], hilink_reception.arr_body[13]);
        sprintf_P (str_value, PSTR ("%u.%u.%u"), major, minor, patch);
        hilink_status.str_firmware = str_value;
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s firmware %s"), hilink_status.str_model, str_value);
        break;

      case 0x1001:    // set serial number
        HilinkAppendCommand (LD2410S_CMND_GET_SERIAL);
        break;

      case 0x1101:    // read serial number
        //  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21
        // FD FC FB FA 0C 00 11 01 00 00 06 00 32 35 30 33 31 38 04 03 02 01
        //   header   | len |ty|hd| ack |ftype| V 3  .  0  .  3 |  footer
        memset (str_value, 0, sizeof (str_value));
        for (index = 0; index < 8; index ++) str_value[index] = hilink_reception.arr_body[12 + index];
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s serial is %s"), hilink_status.str_model, str_value);
        break;

      case 0x7001:    // set radar parameters
        HilinkAppendCommand (LD2410S_CMND_GET_PARAM, hilink_command.param);
        break;

      case 0x7101:    // read radar parameters
        //  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17
        // FD FC FB FA 1A 00 71 01 00 00 0C 00 00 00 04 03 02 01
        // header     | len |cw cv| ack |   value   |trailer
        value = hilink_reception.arr_body[10];
        switch (hilink_command.param)
        {
          case 0x0a: AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s min gate is %u"),        hilink_status.str_model, value + 1); break;
          case 0x05: AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s max gate is %u"),        hilink_status.str_model, value + 1); break;
          case 0x06: AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s delay is %u sec."),      hilink_status.str_model, value);     break;
          case 0x0b: AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s response speed is %u"),  hilink_status.str_model, value);     break;
          case 0x02: AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s stat freq is %u.%u Hz"), hilink_status.str_model, value / 10, value % 10); break;
          case 0x0c: AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s dist freq is %u.%u Hz"), hilink_status.str_model, value / 10, value % 10); break;
        }
        break;

      case 0x7201:    // set gate triggers
        HilinkAppendCommand (LD2410S_CMND_GET_TRIGGER);
        break;

      case 0x7301:    // read gate trigger levels
        //  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17          66 67 68 69 70 71 72 73 74 75 76 77
        // FD FC FB FA 44 00 73 01 00 00 32 00 00 00 2E 00 00 00    ...  |1E 00 00 00 1E 00 00 00 04 03 02 01
        // header     | len |cw cv| ack |   gate 0  |   gate 1  |   ...  |   gate 14 |  gate 15  |  trailer
        str_log[0] = 0;
        for (gate = 0; gate < hilink_status.gate_qty; gate ++)
        {
          index = 10 + gate * 4;
          hilink_motion.arr_gate[gate].threshold = hilink_reception.arr_body[index];
          sprintf_P (str_value, PSTR ("%u "), hilink_motion.arr_gate[gate].threshold);
          strcat (str_log, str_value);
        }
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s trig levels = %s"), hilink_status.str_model, str_log);
        break;

      case 0x7601:    // set gate holds
        HilinkAppendCommand (LD2410S_CMND_GET_HOLD);
        break;

      case 0x7701:    // read gate hold levels
        //  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17          66 67 68 69 70 71 72 73 74 75 76 77
        // FD FC FB FA 44 00 77 01 00 00 0F 00 00 00 0F 00 00 00    ...  |09 00 00 00 09 00 00 00 04 03 02 01
        // header     | len |cw cv| ack |   gate 0  |   gate 1  |   ...  |   gate 14 |  gate 15  |  trailer
        str_log[0] = 0;
        for (gate = 0; gate < hilink_status.gate_qty; gate ++)
        {
          index = 10 + gate * 4;
          hilink_static.arr_gate[gate].threshold = hilink_reception.arr_body[index];
          sprintf_P (str_value, PSTR ("%u "), hilink_static.arr_gate[gate].threshold);
          strcat (str_log, str_value);
        }
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s hold levels = %s"), hilink_status.str_model, str_log);
        break;

      case 0x7a01:    // data reporting mode
        if (hilink_command.param == 0) AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s minimal data mode"), hilink_status.str_model);
          else AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s energy data mode"), hilink_status.str_model);
        break;

      case 0x0901:    // auto threshold detection start
        hilink_status.progress = 0;
        break;
      }
  }

  // declare command as finished
  HilinkCommandFinished ();
}

// Handling of received data
void LD2410sHandleReceivedData ()
{
  uint8_t  index, gate, step;
  uint32_t energy;

  // log data to console
  HilinkLogHexa (hilink_reception.arr_body, hilink_reception.idx_body, false, false);

  // if enough data received for energy or auto level progress
  if (hilink_reception.idx_body >= 10)
  {
    // handle message according to type
    switch (hilink_reception.arr_body[6])
    {
      // energy data
      case 0x01:
        // set detection status
        hilink_motion.arr_target[0].active = (hilink_reception.arr_body[7] > 1);

        // distance
        hilink_motion.arr_target[0].dist = HilinkConvertSerialToUint16 (hilink_reception.arr_body[8], hilink_reception.arr_body[9]);

        // energy per gate
        for (gate = 0; gate < hilink_status.gate_qty; gate ++ )
        {
          index = LD2410S_ENERGY_GATE + gate * 4;
          energy = HilinkConvertSerialToUint16 (hilink_reception.arr_body[index], hilink_reception.arr_body[index + 1]);
          hilink_static.arr_gate[gate].power = LD2410sConvertEnergy2Percent (energy);
          hilink_motion.arr_gate[gate].power = hilink_static.arr_gate[gate].power;
        }
        break;

      // auto threshold progress
      case 0x03:
        // get progress level
        index = hilink_reception.arr_body[7];
        step  = index / 5;

        // if auto threshold is over, set back to energy mode
        if (index == 100)
        {
          AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s auto detection finished"), hilink_status.str_model);
          hilink_status.progress = UINT8_MAX;

          // send energy mode after delay
          HilinkSetCommandDelay (LD2410S_START_DELAY);
          HilinkAppendCommand (LD2410S_CMND_ENERGY, 0x01);
          HilinkAppendCommand (LD2410S_CMND_ENERGY, 0x01);      // only second command will be accepted after auto level detection
        }

        // else display progress if % changed
        else if (step != hilink_status.progress) 
        {
          hilink_status.progress = step;
          AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s auto detection %u%%"), hilink_status.str_model, index);
        }

        // reset target and energy data
        hilink_motion.arr_target[0].active = false;
        hilink_motion.arr_target[0].dist   = 0;
        for (gate = 0; gate < hilink_status.gate_qty; gate ++ )
        {
          hilink_static.arr_gate[gate].power = 0;
          hilink_motion.arr_gate[gate].power = 0;
        }
        break;
    }

    // reset reception buffer
    HilinkReceptionEmpty ();
  }

  // else if enough data received for minimal data
  else if (hilink_reception.idx_body == 5)
  {
    // set detection status
    hilink_motion.arr_target[0].active = (hilink_reception.arr_body[1] > 1);

    // distance
    hilink_motion.arr_target[0].dist = HilinkConvertSerialToUint16 (hilink_reception.arr_body[2], hilink_reception.arr_body[3]);

    // reset energy data
    for (gate = 0; gate < hilink_status.gate_qty; gate ++ )
    {
      hilink_static.arr_gate[gate].power = 0;
      hilink_motion.arr_gate[gate].power = 0;
    }

    // reset reception buffer
    HilinkReceptionEmpty ();
  }
}

/*********************************************\
 *                   Callback
\*********************************************/

void LD2410sProcessCommandQueue ()
{
  bool    cmnd_mode, cmnd_waiting;
  uint8_t command, param;
  char    str_command[16];
  char    str_param[16];

  // check validity
  if (hilink_config.device != HLK_DEVICE_LD2410S) return;
  if (hilink_status.pserial == nullptr) return;
  if (!HilinkCommandPossible ()) return;

  // check if commands are waiting
  cmnd_mode    = HilinkIsInCommandMode ();
  cmnd_waiting = HilinkCommandWaiting ();

  // commands in the pipe and command mode not started, start command mode
  if (cmnd_waiting && !cmnd_mode) LD2410sSendCommand (LD2410S_CMND_START, nullptr);
    
  // else if no command in the queue, stop command mode
  else if (!cmnd_waiting && cmnd_mode) LD2410sSendCommand (LD2410S_CMND_STOP, nullptr);

  // else send next command
  else if (cmnd_waiting && cmnd_mode)
  {
    HilinkGetNextCommand (str_command, sizeof (str_command));
    HilinkSplitCommand (str_command, command, str_param, sizeof (str_param));
    LD2410sSendCommand (command, str_param);
  }
}

// Handling of serial reception
void LD2410sSerialReception ()
{
  uint8_t  index, delta;
  uint32_t value;

  // check if enabled
  if (hilink_config.device != HLK_DEVICE_LD2410S) return;
  if (hilink_status.pserial == nullptr) return;

  // run serial receive loop
  while (hilink_status.pserial->available ()) 
  {
    // init
    value = 0;
    
    // receive character
    HilinkReceptionAppend ((uint8_t)hilink_status.pserial->read ());

    // if header is 6E, we are receiving minimal data
    if (hilink_reception.arr_body[0] == 0x6e)
    {
      // if we received 5 bytes, message is complete
      if (hilink_reception.idx_body == 5) value = (uint32_t)hilink_reception.arr_body[4];
    }

    // else, if buffer size allows, calculate value of last 4 digits received
    else if (hilink_reception.idx_body >= 4)
    {
      delta = hilink_reception.idx_body - 4;
      value = 0x1000000 * (uint32_t)hilink_reception.arr_body[delta] + 0x10000 * (uint32_t)hilink_reception.arr_body[delta + 1] + 0x100 * (uint32_t)hilink_reception.arr_body[delta + 2] + (uint32_t)hilink_reception.arr_body[delta + 3];
    }

    // check for header or footer
    switch (value)
    {
      case 0xf4f3f2f1:    // data header
      case 0xfdfcfbfa:    // command header
        // if more than 4 digits received, remove everything before header
        if (hilink_reception.idx_body > 4)
        {
          for (index = 0; index < 4; index ++) hilink_reception.arr_body[index] = hilink_reception.arr_body[delta + index];
          hilink_reception.idx_body = 4;
        }
        break;

      case 0x04030201:    // command footer
        LD2410sHandleReceivedCommand ();
        break;

      case 0xf8f7f6f5:    // data footer
      case 0x62:          // minimal data footer
        LD2410sHandleReceivedData ();
        break;
    }
  }
}

/***************************************\
 *              Interface
\***************************************/

// LD2410 sensor
bool XsnsHilinkLD2410s (const uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
    case FUNC_COMMAND:
      if (hilink_config.device == HLK_DEVICE_LD2410S) result = DecodeCommand (kHLKLD2410sCommands, HLKLD2410sCommand);
      break;

    case FUNC_EVERY_250_MSECOND:
      LD2410sProcessCommandQueue ();
      break;

    case FUNC_EVERY_100_MSECOND:
      LD2410sSerialReception ();
      break;
  }

  return result;
}

#endif     // USE_HILINK_LD2410S
#endif     // USE_HILINK_DETECTOR
