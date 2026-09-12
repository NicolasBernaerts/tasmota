/*
  xsns_102_05_ld2420.ino - Driver for Presence and Movement sensor HLK-LD2420

  Copyright (C) 2026  Nicolas Bernaerts

  Version history :
    30/04/2026 - v1.0 - Creation
    06/06/2026 - v2.0 - Complete rewrite to be used as a generic hilink detector

  Connexions :
    * GPIO1 should be declared as LD2410 Tx and connected to HLK-LD2420 Rx
    * GPIO3 should be declared as LD2410 Rx and connected to HLK-LD2420 OT1

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
#ifdef USE_HILINK_LD2420

#include <TasmotaSerial.h>

/*************************************************\
 *               Variables
\*************************************************/

// constant
#define LD2420_START_DELAY              5         // sensor startup delay

#define LD2420_GATE_QUANTITY            12        // number of sensor gates
#define LD2420_GATE_WIDTH               70        // one gate handles 70 cm

#define LD2420_DIST_MAX                 840       // maximum detectable distance

#define LD2420_DATA_RATE                115200

#define LD2420_ENERGY_GATE              9         // index of gate level in energy mode 

// ----------------
// binary commands
// ----------------

uint8_t ld2420_cmnd_version[]     PROGMEM = { 0x00, 0x00 };
uint8_t ld2420_cmnd_get_serial[]  PROGMEM = { 0x11, 0x00 };
uint8_t ld2420_cmnd_set_serial[]  PROGMEM = { 0x10, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

uint8_t ld2420_cmnd_restart[]     PROGMEM = { 0x68, 0x00 };
uint8_t ld2420_cmnd_reset[]       PROGMEM = { 0xa2, 0x00, 0x00, 0x00 };

uint8_t ld2420_cmnd_energy[]      PROGMEM = { 0x12, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00 };

uint8_t ld2420_cmnd_get_param[]   PROGMEM = { 0x08, 0x00, 0x00, 0x00 };
uint8_t ld2420_cmnd_set_param[]   PROGMEM = { 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

// list of commands available
enum LD2420ListCommand { LD2420_CMND_NONE, LD2420_CMND_START, LD2420_CMND_STOP, LD2420_CMND_VERSION, LD2420_CMND_ENERGY, LD2420_CMND_RESTART, LD2420_CMND_RESET, LD2420_CMND_GET_SERIAL, LD2420_CMND_SET_SERIAL, LD2420_CMND_GET_MINIMUM, LD2420_CMND_SET_MINIMUM, LD2420_CMND_GET_MAXIMUM, LD2420_CMND_SET_MAXIMUM, LD2420_CMND_GET_DELAY, LD2420_CMND_SET_DELAY, LD2420_CMND_GET_TRIGGER, LD2420_CMND_SET_TRIGGER, LD2420_CMND_GET_HOLD, LD2420_CMND_SET_HOLD, LD2420_CMND_MAX };

// MQTT commands
const char kHLKLD2420Commands[]         PROGMEM = "hlk" "|" "_serial" "|"    "_restart"   "|"    "_reset"  "|"    "_gate"   "|"    "_energy"      ;
void (* const HLKLD2420Command[])(void) PROGMEM = {   &CmndLD2420Serial, &CmndLD2420Restart,&CmndLD2420Reset, &CmndLD2420Gate, &CmndLD2420Energy };

/******************************\
 *           Commands
\******************************/

void CmndLD2420Restart ()
{
  HilinkAppendCommand (LD2420_CMND_RESTART);
  ResponseCmndDone ();
}

void CmndLD2420Reset ()
{
  HilinkAppendCommand (LD2420_CMND_RESET);
  ResponseCmndDone ();
}

void CmndLD2420Serial ()
{
  int  index, length;
  char str_data[16];

  if (XdrvMailbox.data_len > 0)
  {
    strcpy (str_data, XdrvMailbox.data);
    length = strlen (str_data);
    for (index = length; index < 8; index ++) strcat_P (str_data, PSTR (" "));
    HilinkAppendCommand (LD2420_CMND_SET_SERIAL, str_data);
  }
  else HilinkAppendCommand (LD2420_CMND_GET_SERIAL);

  ResponseCmndDone ();
}

void CmndLD2420Energy ()
{
  int  index, length;
  char str_data[16];

  if (XdrvMailbox.data_len > 0)
  {
    if (XdrvMailbox.payload == 0) HilinkAppendCommand (LD2420_CMND_ENERGY, 0x64);
      else HilinkAppendCommand (LD2420_CMND_ENERGY, 0x04);
    ResponseCmndDone ();
  }
  else ResponseCmndFailed ();
}

// command : hilink_gate gate trigger,hold = gate sensitivity (%)
void CmndLD2420Gate ()
{
  bool  is_ok;
  int   gate;
  long  value, level;
  char *pstr_data;
  char *pstr_hold;
  char *pstr_digit;
  char  str_data[16];

  is_ok = (XdrvMailbox.data_len > 0);
  if (is_ok)
  {
    // get gate number
    strcpy (str_data, XdrvMailbox.data);
    pstr_data = strchr (str_data, ' ');
    if (pstr_data != nullptr) *pstr_data++ = 0;
    
    // get gate target
    gate = atoi (str_data) - 1;
    is_ok = (gate < hilink_status.gate_qty);
  }

  if (is_ok && (pstr_data != nullptr))
  {
    // extract data
    pstr_hold = strchr (pstr_data, ',');
    if (pstr_hold != nullptr) *pstr_hold++ = 0;

    // extract trigger
    pstr_digit = strchr (pstr_data, '.');
    if (pstr_digit != nullptr) *pstr_digit++ = 0;
    level = 100 * atol (pstr_data);
    if (pstr_digit != nullptr) 
      if (strlen (pstr_digit) == 1) level += 10 * atol (pstr_digit);
        else level += atol (pstr_digit);
    hilink_motion.arr_gate[gate].threshold = (uint8_t)(level / 100);

    // update gate trigger
    value = LD2420ConvertLevel2Serial (level);
    HilinkAppendCommand (LD2420_CMND_SET_TRIGGER, gate, value);

    // extract holding
    if (pstr_hold != nullptr)
    {
      pstr_digit = strchr (pstr_hold, '.');
      if (pstr_digit != nullptr) *pstr_digit++ = 0;
      level = 100 * atol (pstr_hold);
      if (pstr_digit != nullptr) 
        if (strlen (pstr_digit) == 1) level += 10 * atol (pstr_digit);
          else level += atol (pstr_digit);
      hilink_static.arr_gate[gate].threshold = (uint8_t)(level / 100);
    }
    else hilink_static.arr_gate[gate].threshold = hilink_motion.arr_gate[gate].threshold;

    // update gate trigger
    value = LD2420ConvertLevel2Serial (level);
    HilinkAppendCommand (LD2420_CMND_SET_HOLD, gate, value);
  }

  // else if gate provided, read gate
  else if (is_ok)
  {
    HilinkAppendCommand (LD2420_CMND_GET_TRIGGER, gate);
    HilinkAppendCommand (LD2420_CMND_GET_HOLD,    gate);
  }

  // else, update all gates
  else for (gate = 0; gate < hilink_status.gate_qty; gate ++)
  {
    HilinkAppendCommand (LD2420_CMND_GET_TRIGGER, gate);
    HilinkAppendCommand (LD2420_CMND_GET_HOLD,    gate);
  }

  // command done
  ResponseCmndDone ();
}

/******************************\
 *       Common commands
\******************************/

void LD2420DeviceCommand (const uint8_t command, const uint8_t context)
{
  uint16_t gate;

  // handle different common commands
  switch (command)
  {
    // display device help
    case HILINK_CMND_HELP:
      AddLog (LOG_LEVEL_INFO, PSTR ("%s specific commands :"), hilink_status.str_model);
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_restart      = restart sensor"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_reset        = reset sensor"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_serial <sn>  = set serial number"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_energy <0/1> = set energy mode"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_gate gate trigger,hold = gate sensitivity (%)"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    gate    : 1 .. %u"), hilink_status.gate_qty);
      AddLog (LOG_LEVEL_INFO, PSTR ("    trigger : 0.00 .. 100"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    hold    : 0.00 .. 100"));
      break;

    // init device parameters
    case HILINK_CMND_INIT:
      // declare device
      hilink_status.baudrate   = LD2420_DATA_RATE;
      hilink_status.gate_qty   = LD2420_GATE_QUANTITY;
      hilink_status.gate_width = LD2420_GATE_WIDTH;
      if (hilink_status.dist_limit > LD2420_DIST_MAX) hilink_status.dist_limit = LD2420_DIST_MAX;

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
      HilinkSetCommandDelay (LD2420_START_DELAY);

      // set parameters
      HilinkAppendCommand (LD2420_CMND_SET_DELAY,   (int)hilink_config.delay);
      HilinkAppendCommand (LD2420_CMND_SET_MINIMUM, (int)HilinkGateMin ());
      HilinkAppendCommand (LD2420_CMND_SET_MAXIMUM, (int)max (HilinkPresenceGateMax (), HilinkMotionGateMax ()));

      // get device general config
      HilinkAppendCommand (LD2420_CMND_VERSION);
      HilinkAppendCommand (LD2420_CMND_GET_DELAY);
      HilinkAppendCommand (LD2420_CMND_GET_MINIMUM);
      HilinkAppendCommand (LD2420_CMND_GET_MAXIMUM);
      for (gate = 0; gate < hilink_status.gate_qty; gate ++)
      {
        HilinkAppendCommand (LD2420_CMND_GET_TRIGGER, gate);
        HilinkAppendCommand (LD2420_CMND_GET_HOLD,    gate);
      }

      // set energy mode
      HilinkAppendCommand (LD2420_CMND_ENERGY, 0x04);
      break;

    // read device data information
    case HILINK_CMND_INFO:
      HilinkAppendCommand (LD2420_CMND_VERSION);
      break;
      
    // read device parameters
    case HILINK_CMND_PARAM:
      HilinkAppendCommand (LD2420_CMND_GET_DELAY);
      HilinkAppendCommand (LD2420_CMND_GET_MINIMUM);
      HilinkAppendCommand (LD2420_CMND_GET_MAXIMUM);
      for (gate = 0; gate < hilink_status.gate_qty; gate ++)
      {
        HilinkAppendCommand (LD2420_CMND_GET_TRIGGER, gate);
        HilinkAppendCommand (LD2420_CMND_GET_HOLD,    gate);
      }
      break;

    // set presence-less delay
    case HILINK_CMND_DELAY:
      HilinkAppendCommand (LD2420_CMND_SET_DELAY, (int)hilink_config.delay);
      break;
      
    // set minimum detection distance
    case HILINK_CMND_DMIN:
      HilinkAppendCommand (LD2420_CMND_SET_MINIMUM, (int)HilinkGateMin ());
      break;

    // set maximum detection distance
    case HILINK_CMND_DMAX:
      HilinkAppendCommand (LD2420_CMND_SET_MAXIMUM, (int)max (HilinkPresenceGateMax (), HilinkMotionGateMax ()));
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

/**************************************************\
 *                  Functions
\**************************************************/

// convert detection level to serial 
long LD2420ConvertLevel2Serial (const long level)
{
  long result;

  result = level * 65535 / 10000;

  return result;
}

// convert energy value to percentage with logarithmic scale
uint8_t LD2420ConvertEnergy2Percent (const uint16_t energy)
{
  uint32_t value;

  value = (uint32_t)energy * 100 / 65535;
  return (uint8_t)value;
}

/*
// driver initialisation
void LD2420AppendInitCommand ()
{
  uint8_t gate;

  // default config at startup
  hilink_reception.data   = true;
  hilink_reception.energy = false;
  hilink_command.mode     = false;

  // no bluetooth available on the device
  hilink_config.param.bluetooth = 0;

  // set initial delay
  HilinkSetCommandDelay (LD2420_START_DELAY);

  // get device data
  LD2420DeviceCommand (HILINK_CMND_INFO, 0);

  // set boundaries
  LD2420DeviceCommand (HILINK_CMND_DMIN, 0);
  LD2420DeviceCommand (HILINK_CMND_DMAX, 0);

  // get parameters
  LD2420DeviceCommand (HILINK_CMND_PARAM, 0);

  // set energy mode
  HilinkAppendCommand (LD2420_CMND_ENERGY, 0x04);
}
*/

/*********************************************\
 *             Communication
\*********************************************/

// send command
void LD2420SendCommand (const uint8_t command, char *pstr_param)
{
  uint16_t param = 0;
  long     value = 0;
  size_t   index, body;
  uint8_t  arr_buffer[16];
  char    *pstr_value;

  // check sensor presence
  if (hilink_status.pserial == nullptr) return;

  // get parameter
  if (pstr_param != nullptr)
  {
    pstr_value = strchr (pstr_param, ',');
    if (pstr_value != nullptr) *pstr_value++ = 0;
    param = (uint16_t)atoi (pstr_param);
    if (pstr_value != nullptr) value = atol (pstr_value);
  }

  // handle command
  hilink_reception.data = false;

  // add command body
  body = 0;
  switch (command)
  {
    case LD2420_CMND_START:
      body = sizeof (hilink_cmnd_start);
      memcpy_P (arr_buffer, hilink_cmnd_start, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set command ON"), hilink_status.str_model);
      break;

    case LD2420_CMND_STOP:
      body = sizeof (hilink_cmnd_stop);
      memcpy_P (arr_buffer, hilink_cmnd_stop, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set command OFF"), hilink_status.str_model);
      break;

    case LD2420_CMND_VERSION:
      body = sizeof (ld2420_cmnd_version);
      memcpy_P (arr_buffer, ld2420_cmnd_version, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s get version"), hilink_status.str_model);
      break;

    case LD2420_CMND_RESTART:
      body = sizeof (ld2420_cmnd_restart);
      memcpy_P (arr_buffer, ld2420_cmnd_restart, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s restart"), hilink_status.str_model);
      
      // prepare init commands after restart
      HilinkDeviceCommand (HILINK_CMND_FIRST);
      break;

    case LD2420_CMND_RESET:
      body = sizeof (ld2420_cmnd_reset);
      memcpy_P (arr_buffer, ld2420_cmnd_reset, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s reset"), hilink_status.str_model);

      // ask for device restart
      HilinkSetCommandDelay (LD2420_START_DELAY);
      HilinkAppendCommand (LD2420_CMND_RESTART);
      break;

    case LD2420_CMND_GET_SERIAL:
      body = sizeof (ld2420_cmnd_get_serial);
      memcpy_P (arr_buffer, ld2420_cmnd_get_serial, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s get serial number"), hilink_status.str_model);
      break;

    case LD2420_CMND_SET_SERIAL:
      body = sizeof (ld2420_cmnd_set_serial);
      memcpy_P (arr_buffer, ld2420_cmnd_set_serial, body);
      for (index = 0; index < 8; index++) arr_buffer[4 + index] = pstr_param[index];
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set serial to %s"), hilink_status.str_model, pstr_param);
      break;

    case LD2420_CMND_ENERGY:
      body = sizeof (ld2420_cmnd_energy);
      memcpy_P (arr_buffer, ld2420_cmnd_energy, body);
      arr_buffer[4] = (uint8_t)param;
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s data in energy mode"), hilink_status.str_model);
      break;

    case LD2420_CMND_GET_MINIMUM:
      body = sizeof (ld2420_cmnd_get_param);
      memcpy_P (arr_buffer, ld2420_cmnd_get_param, body);
      arr_buffer[2] = 0x00;
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s get gate min"), hilink_status.str_model);
      break;

    case LD2420_CMND_SET_MINIMUM:
      body = sizeof (ld2420_cmnd_set_param);
      memcpy_P (arr_buffer, ld2420_cmnd_set_param, body);
      arr_buffer[2] = 0x00;
      arr_buffer[4] = (uint8_t)param;
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set gate min to %u"), hilink_status.str_model, param);
      break;

    case LD2420_CMND_GET_MAXIMUM:
      body = sizeof (ld2420_cmnd_get_param);
      memcpy_P (arr_buffer, ld2420_cmnd_get_param, body);
      arr_buffer[2] = 0x01;
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s get gate max"), hilink_status.str_model);
      break;

    case LD2420_CMND_SET_MAXIMUM:
      body = sizeof (ld2420_cmnd_set_param);
      memcpy_P (arr_buffer, ld2420_cmnd_set_param, body);
      arr_buffer[2] = 0x01;
      arr_buffer[4] = (uint8_t)param;
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set gate max to %u"), hilink_status.str_model, param);
      break;

    case LD2420_CMND_GET_DELAY:
      body = sizeof (ld2420_cmnd_get_param);
      memcpy_P (arr_buffer, ld2420_cmnd_get_param, body);
      arr_buffer[2] = 0x04;
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s get delay"), hilink_status.str_model);
      break;

    case LD2420_CMND_SET_DELAY:
      body = sizeof (ld2420_cmnd_set_param);
      memcpy_P (arr_buffer, ld2420_cmnd_set_param, body);
      arr_buffer[2] = 0x04;
      arr_buffer[4] = (uint8_t)(param % 256);
      arr_buffer[5] = (uint8_t)(param / 256);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set timeout to %u"), hilink_status.str_model, param);
      break;

    case LD2420_CMND_GET_TRIGGER:
      body = sizeof (ld2420_cmnd_get_param);
      memcpy_P (arr_buffer, ld2420_cmnd_get_param, body);
      arr_buffer[2] = 0x10 + (uint8_t)param;
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s get gate %u trigger"), hilink_status.str_model, param);
      break;

    case LD2420_CMND_SET_TRIGGER:
      body = sizeof (ld2420_cmnd_set_param);
      memcpy_P (arr_buffer, ld2420_cmnd_set_param, body);
      arr_buffer[2] = 0x10 + (uint8_t)param;
      arr_buffer[4] = (uint8_t)(value % 256);
      arr_buffer[5] = (uint8_t)(value / 256);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set gate %u trigger"), hilink_status.str_model, param);
      break;

    case LD2420_CMND_GET_HOLD:
      body = sizeof (ld2420_cmnd_get_param);
      memcpy_P (arr_buffer, ld2420_cmnd_get_param, body);
      arr_buffer[2] = 0x20 + (uint8_t)param;
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s get gate %u hold"), hilink_status.str_model, param);
      break;

    case LD2420_CMND_SET_HOLD:
      body = sizeof (ld2420_cmnd_set_param);
      memcpy_P (arr_buffer, ld2420_cmnd_set_param, body);
      arr_buffer[2] = 0x20 + (uint8_t)param;
      arr_buffer[4] = (uint8_t)(value % 256);
      arr_buffer[5] = (uint8_t)(value / 256);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set gate %u hold"), hilink_status.str_model, param);
      break;
  }

  // if command defined, send it
  if (body > 0)
  {
    HilinkCommandStarted (command, (uint8_t)param);
    HilinkSendCommand (arr_buffer, body);
  }
}

// Handling of received command
void LD2420HandleReceivedCommand ()
{
  uint8_t  index, gate;
  uint16_t command, distance, delay;
  uint16_t value;
  char     str_serial[16];

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
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s command ON"), hilink_status.str_model);
        break;
      
      case 0xfe01:    // command stop
        hilink_command.mode = false;
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s command OFF"), hilink_status.str_model);
        break;

      case 0x1201:    // energy mode
        if (hilink_command.param == 0x04) AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s in Energy mode"), hilink_status.str_model);  
          else AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s in Data mode"), hilink_status.str_model); 
        break;

      case 0x0001:    // read firmware version
        for (index = 0; index < 6; index ++) str_serial[index] = hilink_reception.arr_body[12 + index];
        str_serial[7] = 0;
        hilink_status.str_firmware = str_serial;
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s firmware %s"), hilink_status.str_model, str_serial);
        break;

      case 0x1001:    // set serial number
        HilinkAppendCommand (LD2420_CMND_GET_SERIAL);
        break;

      case 0x1101:    // read serial number
        //  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21
        // FD FC FB FA 0C 00 11 01 00 00 06 00 32 35 30 33 31 38 04 03 02 01
        //   header   | len |ty|hd| ack |ftype| V 3  .  0  .  3 |  footer
        memset (str_serial, 0, sizeof (str_serial));
        for (index = 0; index < 8; index ++) str_serial[index] = hilink_reception.arr_body[12 + index];
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s serial is %s"), hilink_status.str_model, str_serial);
        break;

      case 0x0701:    // set parameters
        // handle according to last command
        switch (hilink_command.command)
        {
          case LD2420_CMND_SET_MINIMUM: HilinkAppendCommand (LD2420_CMND_GET_MINIMUM); break;
          case LD2420_CMND_SET_MAXIMUM: HilinkAppendCommand (LD2420_CMND_GET_MAXIMUM); break;
          case LD2420_CMND_SET_DELAY:   HilinkAppendCommand (LD2420_CMND_GET_DELAY);   break;
          case LD2420_CMND_SET_TRIGGER: HilinkAppendCommand (LD2420_CMND_GET_TRIGGER, (int)hilink_command.param); break;
          case LD2420_CMND_SET_HOLD:    HilinkAppendCommand (LD2420_CMND_GET_HOLD,    (int)hilink_command.param); break;
        }
        break;

      case 0x0801:    // get parameters
        //  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17
        // FD FC FB FA 08 00 08 01 00 00 00 00 00 00 04 03 02 01
        // header     | len |     |     |   val     |trailer
        switch (hilink_command.command)
        {
          case LD2420_CMND_GET_MINIMUM:
            distance = (uint16_t)hilink_reception.arr_body[10] * hilink_status.gate_width;
            AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s minimum distance is %u cm"), hilink_status.str_model, distance);
            break;

          case LD2420_CMND_GET_MAXIMUM:
            distance = (uint16_t)(hilink_reception.arr_body[10] + 1) * hilink_status.gate_width;
            AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s maximum distance is %u cm"), hilink_status.str_model, distance);
            break;

          case LD2420_CMND_GET_DELAY:
            delay = HilinkConvertSerialToUint16 (hilink_reception.arr_body[10], hilink_reception.arr_body[11]);
            AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s Delay = %u s"), hilink_status.str_model, delay);
            break;

          case LD2420_CMND_GET_TRIGGER:
            index = hilink_command.param;
            value = HilinkConvertSerialToUint16 (hilink_reception.arr_body[10], hilink_reception.arr_body[11]);
            hilink_motion.arr_gate[index].threshold = LD2420ConvertEnergy2Percent (value);
            AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s Gate %u, trigger %u%% [%u]"), hilink_status.str_model, index + 1, hilink_motion.arr_gate[index].threshold, value);
            break;

          case LD2420_CMND_GET_HOLD:
            index = hilink_command.param;
            value = HilinkConvertSerialToUint16 (hilink_reception.arr_body[10], hilink_reception.arr_body[11]);
            hilink_static.arr_gate[index].threshold = LD2420ConvertEnergy2Percent (value);
            AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s Gate %u, holding %u%% [%u]"), hilink_status.str_model, index + 1, hilink_static.arr_gate[index].threshold, value);
            break;
        }
        break;
    }
  }

  // declare command as finished
  HilinkCommandFinished ();
}

// Handling of received energy
void LD2420HandleReceivedEnergy ()
{
  uint8_t  index, gate, level;
  uint16_t value;

  // log data to console
  HilinkLogHexa (hilink_reception.arr_body, hilink_reception.idx_body, false, false);

  // if enough data received
  if (hilink_reception.idx_body >= 45)
  {
    // distance
    hilink_motion.arr_target[0].dist = HilinkConvertSerialToUint16 (hilink_reception.arr_body[7], hilink_reception.arr_body[8]);

    // set detection status
    hilink_motion.arr_target[0].active = (hilink_motion.arr_target[0].dist > 0);

    // energy per gate
    index = LD2420_ENERGY_GATE;
    hilink_motion.arr_target[0].power = 0;
    for (gate = 0; gate < hilink_status.gate_qty; gate ++ )
    {
      // update gate power
      value = HilinkConvertSerialToUint16 (hilink_reception.arr_body[index++], hilink_reception.arr_body[index++]);
      level = LD2420ConvertEnergy2Percent (value);
      hilink_static.arr_gate[gate].power = level;
      hilink_motion.arr_gate[gate].power = level;

      // update target power (percentage of threshold)
      if (hilink_motion.arr_target[0].power < 100)
      {
        if (hilink_motion.arr_gate[gate].threshold == 0) value = 100;
          else if (hilink_motion.arr_gate[gate].power > hilink_motion.arr_gate[gate].threshold) value = 100;
          else value = 100 * (uint16_t)hilink_motion.arr_gate[gate].power / (uint16_t)hilink_motion.arr_gate[gate].threshold;
        hilink_motion.arr_target[0].power = max (hilink_motion.arr_target[0].power, (uint8_t)value);
      }
    }
  }

  // reset reception buffer
  hilink_reception.energy = false;
  HilinkReceptionEmpty ();
}

// Handling of received data
void LD2420HandleReceivedData ()
{
  uint8_t gate;
  char   *pstr_buffer;
  char   *pstr_range;
  char   *pstr_value;

  // remove \n and set reception buffer
  hilink_reception.arr_body[hilink_reception.idx_body - 2] = 0;
  pstr_buffer = (char*)hilink_reception.arr_body;

  // log data to console
  HilinkLogText (pstr_buffer, false);

  // check for no motion : Data OFF
  if (strstr_P (pstr_buffer, PSTR ("OFF")) != nullptr)
  {
    // detection distance
    hilink_motion.arr_target[0].active = false; 
    hilink_motion.arr_target[0].dist   = 0; 
  }

  // else check for range : Data Range xx
  else
  {
    pstr_value = nullptr;
    pstr_range = strstr_P (pstr_buffer, PSTR ("Range"));
    if (pstr_range != nullptr) pstr_value = strchr (pstr_range, ' ');
    if (pstr_value != nullptr)
    {
      hilink_motion.arr_target[0].dist   = (uint16_t)atoi (pstr_value + 1); 
      hilink_motion.arr_target[0].active = true; 
      hilink_motion.arr_target[0].power  = 100; 
    }    
  }

  // reset unknown energy levels
  for (gate = 0; gate < hilink_status.gate_qty; gate ++ )
  {
    hilink_static.arr_gate[gate].power = 0;
    hilink_motion.arr_gate[gate].power = 0;
  }

  // reset reception buffer
  HilinkReceptionEmpty ();
}

/*********************************************\
 *                   Callback
\*********************************************/

void LD2420ProcessCommandQueue ()
{
  bool    cmnd_mode, cmnd_waiting;
  uint8_t command, param;
  char    str_command[16];
  char    str_param[16];

  // check validity
  if (hilink_config.device != HLK_DEVICE_LD2420) return;
  if (hilink_status.pserial == nullptr) return;
  if (!HilinkCommandPossible ()) return;

  // check if commands are waiting
  cmnd_mode    = HilinkIsInCommandMode ();
  cmnd_waiting = HilinkCommandWaiting ();

  // commands in the pipe and command mode not started, start command mode
  if (cmnd_waiting && !cmnd_mode) LD2420SendCommand (LD2420_CMND_START, nullptr);
    
  // else if no command in the queue, stop command mode
  else if (!cmnd_waiting && cmnd_mode) LD2420SendCommand (LD2420_CMND_STOP, nullptr);

  // else send next command
  else if (cmnd_waiting && cmnd_mode)
  {
    HilinkGetNextCommand (str_command, sizeof (str_command));
    HilinkSplitCommand (str_command, command, str_param, sizeof (str_param));
    LD2420SendCommand (command, str_param);
  }
}

// Handling of serial reception
void LD2420SerialReception ()
{
  uint8_t  index, delta;
  uint32_t value;

  // check if enabled
  if (hilink_config.device != HLK_DEVICE_LD2420) return;
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

      // detect command mode
      if (value == 0xfdfcfbfa) hilink_reception.data = false;

      // detect energy mode
      if (value == 0xf4f3f2f1) hilink_reception.energy = true;

      // if receiving data, not in engineeering mode
      if (hilink_reception.data && !hilink_reception.energy)
      {
        delta = hilink_reception.idx_body - 2;
        value = HilinkConvertSerialToUint32 (hilink_reception.arr_body[delta + 1], hilink_reception.arr_body[delta], 0, 0);
      }

      // check for header or footer
      switch (value)
      {
        case 0xfdfcfbfa:    // command header
        case 0xf4f3f2f1:    // energy header
          // if more than 4 digits received, remove everything before header
          if (hilink_reception.idx_body > 4)
          {
            for (index = 0; index < 4; index ++) hilink_reception.arr_body[index] = hilink_reception.arr_body[delta + index];
            hilink_reception.idx_body = 4;
          }
          break;

        case 0x04030201:    // command footer
          LD2420HandleReceivedCommand ();
          break;

        case 0xf8f7f6f5:    // energy footer
          LD2420HandleReceivedEnergy ();
          break;

        case 0x0d0a:        // data footer
          LD2420HandleReceivedData ();
          break;
      }
    }
  }
}

/***************************************\
 *              Interface
\***************************************/

// LD2420 sensor
bool XsnsHilinkLD2420 (const uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
    case FUNC_COMMAND:
      if (hilink_config.device == HLK_DEVICE_LD2420) result = DecodeCommand (kHLKLD2420Commands, HLKLD2420Command);
      break;

    case FUNC_EVERY_100_MSECOND:
      LD2420SerialReception ();
      LD2420ProcessCommandQueue ();
      break;
  }

  return result;
}

#endif     // USE_HILINK_LD2420
#endif     // USE_HILINK_DETECTOR
