/*
  xsns_102_07_ld2402.ino - Driver for Presence and Movement sensor HLK-LD2402

  Copyright (C) 2026  Nicolas Bernaerts

  Version history :
    24/05/2026 - v1.0 - Creation

  Connexions :
    * GPIO1 should be declared as LD2410 Tx and connected to HLK-LD2402 Rx
    * GPIO3 should be declared as LD2410 Rx and connected to HLK-LD2402 Tx

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
#ifdef USE_HILINK_LD2402

#include <TasmotaSerial.h>

/*************************************************\
 *               Variables
\*************************************************/

// constant
#define LD2402_START_DELAY              5         // sensor startup delay

#define LD2402_GATE_QUANTITY            16        // number of sensor gates
#define LD2402_GATE_WIDTH               70        // one gate handles 70 cm
#define LD2402_DIST_MAX                 1120      // max 11.2 m

#define LD2402_DATA_RATE                115200

#define LD2402_AUTO_TRIGGER             2         // auto detection default trigger coefficient
#define LD2402_AUTO_KEEP                3         // auto detection default keep coefficient
#define LD2402_AUTO_MICRO               2         // auto detection default micro movement coefficient

#define LD2402_ENERGY_GATE_MOTION       9         // index of motion gate level in energy mode 
#define LD2402_ENERGY_GATE_STATIC       73        // index of static gate level in energy mode 

#define LD2402_MINIMUM_FIRMWARE         335       // minimum firmware version not to be outdated

// ----------------
// binary commands
// ----------------

uint8_t ld2402_cmnd_read_firmware[] PROGMEM = { 0x00, 0x00 };
uint8_t ld2402_cmnd_read_serial[]   PROGMEM = { 0x11, 0x00 };

uint8_t ld2402_cmnd_save[]          PROGMEM = { 0xfd, 0x00 };

uint8_t ld2402_cmnd_energy[]        PROGMEM = { 0x12, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00 };

uint8_t ld2402_cmnd_get_param[]     PROGMEM = { 0x08, 0x00, 0x00, 0x00 };
uint8_t ld2402_cmnd_set_param[]     PROGMEM = { 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

uint8_t ld2402_cmnd_auto_start[]    PROGMEM = { 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
uint8_t ld2402_cmnd_auto_progress[] PROGMEM = { 0x0a, 0x00 };

uint8_t ld2402_cmnd_gain[]          PROGMEM = { 0xee, 0x00 };
uint8_t ld2402_cmnd_interference[]  PROGMEM = { 0x14, 0x00 };

// list of commands available
enum LD2402ListCommand { LD2402_CMND_NONE, LD2402_CMND_START, LD2402_CMND_STOP, LD2402_CMND_SAVE, LD2402_CMND_VERSION, LD2402_CMND_SERIAL, LD2402_CMND_GAIN, LD2402_CMND_INTERFERENCE, LD2402_CMND_AUTO_START, LD2402_CMND_AUTO_PROGRESS, LD2402_CMND_ENERGY, LD2402_CMND_GET_PARAM, LD2402_CMND_SET_PARAM, LD2402_CMND_MAX };

// MQTT commands
const char kHLKLD2402Commands[]         PROGMEM = "hlk" "|" "_save" "|"    "_energy"   "|"      "_gain"     "|"        "_auto"        "|"     "_gate"     ;
void (* const HLKLD2402Command[])(void) PROGMEM = {   &CmndLD2402Save, &CmndLD2402Energy, &CmndLD2402AutoGain, &CmndLD2402AutoThreshold, &CmndLD2402Gate };

/*********************************************\
 *                Conversion
\*********************************************/

// convert detection level to serial 
uint32_t LD2402ConvertLevel2Serial (const long level)
{
  uint32_t result;
  float    exponent;
  double   value;

  // calculate exponent
  exponent = (float)level / 100.0 / 10.0;

  // calculate result value
  value = pow (10.0, exponent);

  return (uint32_t)value;
}

long LD2402ConvertSerial2Level (const uint32_t value)
{
  long  level;
  float value_f, level_f;

  value_f = (float)value;
  level_f = 10 * log (value_f) * 0.43429448;     // log10(f) = ln(f) / ln(10)
  level   = (long)level_f;
//  level   = (long)(level_f * 100);

  return level;
}

/******************************\
 *           Commands
\******************************/

void CmndLD2402Save ()
{
  HilinkAppendCommand (LD2402_CMND_SAVE);
  ResponseCmndDone ();
}

void CmndLD2402AutoGain ()
{
  HilinkAppendCommand (LD2402_CMND_GAIN);
  ResponseCmndDone ();
}

void CmndLD2402Energy ()
{
  // if gain data is available
  if (XdrvMailbox.data_len > 0)
  {
    // append command
    if (XdrvMailbox.payload == 0) HilinkAppendCommand (LD2402_CMND_ENERGY, 0x64);
      else HilinkAppendCommand (LD2402_CMND_ENERGY, 0x04);

    // command handled
    ResponseCmndDone ();
  }

  // command handled
  else ResponseCmndFailed ();
}

void CmndLD2402AutoThreshold ()
{
  char str_data[8];

  strlcpy (str_data, XdrvMailbox.data, sizeof (str_data));
  if (strlen (str_data) == 0) sprintf_P (str_data, PSTR ("%u,%u,%u"), LD2402_AUTO_TRIGGER, LD2402_AUTO_KEEP, LD2402_AUTO_MICRO);

  // append command
  HilinkAppendCommand (LD2402_CMND_AUTO_START, str_data);
  ResponseCmndDone ();
}

// command : hilink_gate gate motion,micro = gate sensitivity (dB)
void CmndLD2402Gate ()
{
  int   gate;
  long  value;
  char *pstr_motion;
  char *pstr_micro; 
  char *pstr_digit;
  char  str_data[16];

  // if no gate provided, get all gate params
  if (XdrvMailbox.data_len == 0)
  {
    for (gate = 0; gate < hilink_status.gate_qty; gate ++) HilinkAppendCommand (LD2402_CMND_GET_PARAM, 16 + gate);
    for (gate = 0; gate < hilink_status.gate_qty; gate ++) HilinkAppendCommand (LD2402_CMND_GET_PARAM, 48 + gate);
  }

  // else if gate index is provided
  else
  {
    // get gate number
    strcpy (str_data, XdrvMailbox.data);
    pstr_motion = strchr (str_data, ' ');
    if (pstr_motion != nullptr) *pstr_motion++ = 0;
    gate = atoi (str_data) - 1;
    
    // get gate is valid
    if (gate < hilink_status.gate_qty)
    {
      // if no data provided, read gate params
      if (pstr_motion == nullptr)
      {
        HilinkAppendCommand (LD2402_CMND_GET_PARAM, 16 + gate);
        HilinkAppendCommand (LD2402_CMND_GET_PARAM, 48 + gate);
      }

      // else, set gate params
      else
      {
        // extract data
        pstr_micro = strchr (pstr_motion, ',');
        if (pstr_micro != nullptr) *pstr_micro++ = 0;

        // separate digits
        pstr_digit = strchr (pstr_motion, '.');
        if (pstr_digit != nullptr) *pstr_digit++ = 0;

        // calculate motion
        value = atol (pstr_motion) * 100;
        if (pstr_digit != nullptr) value += atol (pstr_digit);

        // append motion update
        HilinkAppendCommand (LD2402_CMND_SET_PARAM, 16 + gate, value);

        // extract micro
        if (pstr_micro != nullptr)
        {
          // separate digits
          pstr_digit = strchr (pstr_micro, '.');
          if (pstr_digit != nullptr) *pstr_digit++ = 0;

          // calculate micro motion
          value = atol (pstr_micro) * 100;
          if (pstr_digit != nullptr) value += atol (pstr_digit);
        }

        // append micro motion update
        HilinkAppendCommand (LD2402_CMND_SET_PARAM, 48 + gate, value);
      }
    }

    // else error
    else ResponseCmndFailed ();
  }

  // command done
  ResponseCmndDone ();
}

/******************************\
 *       Common commands
\******************************/

void LD2402DeviceCommand (const uint8_t command, const uint8_t context)
{
  uint8_t gate;

  // handle different common commands
  switch (command)
  {
    // display device help
    case HILINK_CMND_HELP:
      AddLog (LOG_LEVEL_INFO, PSTR ("%s specific commands :"), hilink_status.str_model);
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_save         = save parameters"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_energy <0/1> = set energy mode (defaut)"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_gain         = auto gain adjustment"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_auto trig,keep,micr = start auto level detection"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    trig : trigger coefficient    1..20 [%u]"), LD2402_AUTO_TRIGGER);
      AddLog (LOG_LEVEL_INFO, PSTR ("    keep : keep coefficient       1..20 [%u]"), LD2402_AUTO_KEEP);
      AddLog (LOG_LEVEL_INFO, PSTR ("    micr : micro move coefficient 1..20 [%u]"), LD2402_AUTO_MICRO);
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_gate gate move,micr = gate sensitivity (dB)"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    gate : gate number      1..%u"), hilink_status.gate_qty);
      AddLog (LOG_LEVEL_INFO, PSTR ("    move : motion level     0.00..95.00"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    micr : micro move level 0.00..95.00"));
      break;

    // init device parameters
    case HILINK_CMND_INIT:
      // declare device
      hilink_status.baudrate   = LD2402_DATA_RATE;
      hilink_status.gate_qty   = LD2402_GATE_QUANTITY;
      hilink_status.gate_width = LD2402_GATE_WIDTH;
      hilink_status.dist_limit = LD2402_DIST_MAX;

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
      HilinkSetCommandDelay (LD2402_START_DELAY);

      // set parameters
      HilinkAppendCommand (LD2402_CMND_SET_PARAM, 0x04);        // delay
      HilinkAppendCommand (LD2402_CMND_SET_PARAM, 0x01);        // max distance

      // get device general config
      HilinkAppendCommand (LD2402_CMND_VERSION);
      HilinkAppendCommand (LD2402_CMND_SERIAL);
      HilinkAppendCommand (LD2402_CMND_INTERFERENCE);
      for (gate = 0; gate < hilink_status.gate_qty; gate ++) HilinkAppendCommand (LD2402_CMND_GET_PARAM, 16 + gate);
      for (gate = 0; gate < hilink_status.gate_qty; gate ++) HilinkAppendCommand (LD2402_CMND_GET_PARAM, 48 + gate);
      HilinkAppendCommand (LD2402_CMND_GET_PARAM, 0x05);        // power supply interference

      // set energy mode
      HilinkAppendCommand (LD2402_CMND_ENERGY, 0x04);           // data energy mode
      break;

    // read device data information
    case HILINK_CMND_INFO:
      HilinkAppendCommand (LD2402_CMND_VERSION);
      HilinkAppendCommand (LD2402_CMND_SERIAL);
      HilinkAppendCommand (LD2402_CMND_INTERFERENCE);
      break;
      
    // read device parameters
    case HILINK_CMND_PARAM:
      for (gate = 0; gate < hilink_status.gate_qty; gate ++) HilinkAppendCommand (LD2402_CMND_GET_PARAM, 16 + gate);
      for (gate = 0; gate < hilink_status.gate_qty; gate ++) HilinkAppendCommand (LD2402_CMND_GET_PARAM, 48 + gate);
      HilinkAppendCommand (LD2402_CMND_GET_PARAM, 0x04);        // delay
      HilinkAppendCommand (LD2402_CMND_GET_PARAM, 0x05);        // power supply interference
      break;
   
    // set presence-less delay
    case HILINK_CMND_DELAY:
      HilinkAppendCommand (LD2402_CMND_SET_PARAM, 0x04);
      break;
      
    // set minimum detection distance
    case HILINK_CMND_DMIN:
      break;

    // set maximum detection distance
    case HILINK_CMND_DMAX:
      HilinkAppendCommand (LD2402_CMND_SET_PARAM, 0x01);
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

/**************************************************\
 *                  Functions
\**************************************************/
/*
// device initial commands
void LD2402AppendInitCommand ()
{
  uint8_t gate;
  
  // default config at startup
  hilink_reception.data   = true;
  hilink_reception.energy = false;
  hilink_command.mode     = false;

  // no bluetooth available on the device
  hilink_config.param.bluetooth = 0;

  // set initial delay
  HilinkSetCommandDelay (LD2402_START_DELAY);

  // query device data
  LD2402DeviceCommand (HILINK_CMND_INFO, 0);

  // get device parameters
  LD2402DeviceCommand (HILINK_CMND_PARAM, 0);

  // set parameters
  LD2402DeviceCommand (HILINK_CMND_DELAY, 0);
  LD2402DeviceCommand (HILINK_CMND_DMAX, 0);

  // set energy mode
  HilinkAppendCommand (LD2402_CMND_ENERGY, 0x04);             // data energy mode
}
*/

/*********************************************\
 *             Communication
\*********************************************/

// send command
void LD2402SendCommand (const uint8_t command, char *pstr_param)
{
  uint8_t  gate;
  uint8_t  msb, byte3, byte2, lsb;
  uint8_t  param1 = 0;
  long     param2 = 0;
  long     param3 = 0;
  uint32_t value;
  size_t   body;
  uint8_t  arr_buffer[12];
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
    case LD2402_CMND_START:
      body = sizeof (hilink_cmnd_start);
      memcpy_P (arr_buffer, hilink_cmnd_start, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set command on"), hilink_status.str_model);
      break;

    case LD2402_CMND_STOP:
      body = sizeof (hilink_cmnd_stop);
      memcpy_P (arr_buffer, hilink_cmnd_stop, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set command off"), hilink_status.str_model);
      break;

    case LD2402_CMND_SAVE:
      body = sizeof (ld2402_cmnd_save);
      memcpy_P (arr_buffer, ld2402_cmnd_save, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s parameters saved"), hilink_status.str_model);
      break;

    case LD2402_CMND_VERSION:
      body = sizeof (ld2402_cmnd_read_firmware);
      memcpy_P (arr_buffer, ld2402_cmnd_read_firmware, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s read firmware"), hilink_status.str_model);
      break;

    case LD2402_CMND_SERIAL:
      body = sizeof (ld2402_cmnd_read_serial);
      memcpy_P (arr_buffer, ld2402_cmnd_read_serial, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s read serial number"), hilink_status.str_model);
      break;

    case LD2402_CMND_ENERGY:
      body = sizeof (ld2402_cmnd_energy);
      memcpy_P (arr_buffer, ld2402_cmnd_energy, body);
      arr_buffer[4] = param1;
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s energy mode"), hilink_status.str_model);
      break;

    case LD2402_CMND_GET_PARAM:
      body = sizeof (ld2402_cmnd_get_param);
      memcpy_P (arr_buffer, ld2402_cmnd_get_param, body);
      arr_buffer[2] = param1;
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s get param %u"), hilink_status.str_model, param1);
      break;

    case LD2402_CMND_GAIN:
      body = sizeof (ld2402_cmnd_gain);
      memcpy_P (arr_buffer, ld2402_cmnd_gain, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s auto gain started"), hilink_status.str_model);
      break;

    case LD2402_CMND_AUTO_START:
      // set default parameters
      if ((param1 < 1) || (param1 > 20)) param1 = LD2402_AUTO_TRIGGER;
      if ((param2 < 1) || (param2 > 20)) param2 = LD2402_AUTO_KEEP;
      if ((param3 < 1) || (param3 > 20)) param3 = LD2402_AUTO_MICRO;

      // generate command body
      body = sizeof (ld2402_cmnd_auto_start);
      memcpy_P (arr_buffer, ld2402_cmnd_auto_start, body);
      arr_buffer[2] = param1 * 10;
      arr_buffer[4] = (uint8_t)param2 * 10;
      arr_buffer[6] = (uint8_t)param3 * 10;
      AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s auto level param : trigger %u, keep %d, micro %d"), hilink_status.str_model, param1, param2, param3);
      break;

    case LD2402_CMND_AUTO_PROGRESS:
      body = sizeof (ld2402_cmnd_auto_progress);
      memcpy_P (arr_buffer, ld2402_cmnd_auto_progress, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s auto level progress"), hilink_status.str_model);
      break;

    case LD2402_CMND_INTERFERENCE:
      body = sizeof (ld2402_cmnd_interference);
      memcpy_P (arr_buffer, ld2402_cmnd_interference, body);
      AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s report interference"), hilink_status.str_model);
      break;

    case LD2402_CMND_SET_PARAM:
      body = sizeof (ld2402_cmnd_set_param);
      memcpy_P (arr_buffer, ld2402_cmnd_set_param, body);
      arr_buffer[2] = param1;

      // max distance
      if (param1 == 1)
      {
        arr_buffer[4] = (uint8_t)(hilink_config.pres_max / 10);
        AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set max distance"), hilink_status.str_model);
      }

      // detection delay
      else if (param1 == 4)
      {
        arr_buffer[4] = (uint8_t)(hilink_config.delay % 256);
        arr_buffer[5] = (uint8_t)(hilink_config.delay / 256);
        AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set delay"), hilink_status.str_model);        
      }

      // gate motion detection level
      else if ((param1 >= 16) && (param1 < 32))
      {
        // calculate serial value of gate level
        gate  = param1 - 16;
        value = LD2402ConvertLevel2Serial (param2);
        HilinkConvertUint32ToSerial (value, lsb, byte2, byte3, msb);

        // update command
        arr_buffer[4] = lsb;
        arr_buffer[5] = byte2;
        arr_buffer[6] = byte3;
        arr_buffer[7] = msb;
        AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set gate %u motion to %d.%02d (%u)"), hilink_status.str_model, gate, param2 / 100, param2 % 100, value);        
      }

      // gate micro detection level
      else if ((param1 >= 48) && (param1 < 64))
      {
        // calculate serial value of gate level
        gate  = param1 - 48;
        value = LD2402ConvertLevel2Serial (param2);
        HilinkConvertUint32ToSerial (value, lsb, byte2, byte3, msb);

        // update command
        arr_buffer[4] = lsb;
        arr_buffer[5] = byte2;
        arr_buffer[6] = byte3;
        arr_buffer[7] = msb;
        AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s set gate %u micro to %d.%02d (%u)"), hilink_status.str_model, gate, param2 / 100, param2 % 100, value);        
      }
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
void LD2402HandleReceivedCommand ()
{
  uint8_t  index, gate, step;
  uint16_t command, dist_max, delay;
  long     level;
  uint32_t value;
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
        hilink_command.mode = true;
        hilink_status.protocol = hilink_reception.arr_body[10];
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s command ON"), hilink_status.str_model);
        break;
      
      case 0xfe01:    // command stop
        hilink_command.mode = false;
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s command OFF"), hilink_status.str_model);
        break;

      case 0xfd01:    // save config
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s configuration saved"), hilink_status.str_model);
        break;

      case 0x0001:    // read firmware version
        for (index = 0; index < 6; index ++) str_value[index] = hilink_reception.arr_body[12 + index];
        str_value[6] = 0;
        hilink_status.str_firmware = str_value;
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s firmware %s"), hilink_status.str_model, str_value);

        // check for outdated firmware (< 3.3.5)
        str_value[0] = hilink_reception.arr_body[13];
        str_value[1] = hilink_reception.arr_body[15];
        str_value[2] = hilink_reception.arr_body[17];
        str_value[3] = 0;
        hilink_status.outdated = (atoi (str_value) < LD2402_MINIMUM_FIRMWARE);
        if (hilink_status.outdated) AddLog (LOG_LEVEL_INFO, PSTR ("HLK: WARNING your firmware %s is too old ! Update to get full features"), hilink_status.str_firmware.c_str ());
        break;

      case 0x1101:    // read serial number
        //  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21
        // FD FC FB FA 0C 00 11 01 00 00 06 00 32 35 30 33 31 38 04 03 02 01
        //   header   | len |ty|hd| ack |ftype| V 3  .  0  .  3 |  footer
        memset (str_log, 0, sizeof (str_log));
        for (index = 0; index < 6; index ++) str_log[index] = hilink_reception.arr_body[12 + index];
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s serial number %s"), hilink_status.str_model, str_log);
        break;

      case 0x1201:    // data energy mode
        if (hilink_command.param == 0x04) AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s data energy mode"), hilink_status.str_model);
          else AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s data basic mode"), hilink_status.str_model);
        break;

      case 0xee01:    // auto gain start
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s auto gain running"), hilink_status.str_model);
        HilinkSetCommandDelay (5);      // give 5 sec to finish auto gain
        break;

      case 0xf000:    // auto gain finished
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s auto gain finished"), hilink_status.str_model);
        break;

      case 0x1401:    // automatic threshold interference
        if (hilink_reception.arr_body[8] == 0x00) AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s no interference detected"), hilink_status.str_model);
          else AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s interference detected [%u%u]"), hilink_status.str_model, hilink_reception.arr_body[11], hilink_reception.arr_body[10]);
        break;

      case 0x0901:    // start auto threshold
        hilink_status.progress = 0;
        HilinkAppendCommand (LD2402_CMND_AUTO_PROGRESS);
        AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s auto level started"), hilink_status.str_model);
        break;

      case 0x0a01:    // auto threshold progress
        index = hilink_reception.arr_body[10];
        step  = index / 5;
        if (hilink_status.progress != step) AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s auto level running [%u%%]"), hilink_status.str_model, index);
        hilink_status.progress = step;

        // if less than 100%, append new progress reading
        if (index < 100) HilinkAppendCommand (LD2402_CMND_AUTO_PROGRESS);
        else
        {
          hilink_status.progress = UINT8_MAX;
          AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s auto level finished"), hilink_status.str_model);
        } 
        break;

      case 0x0701:    // set config parameters
        HilinkAppendCommand (LD2402_CMND_GET_PARAM, (int)hilink_command.param);
        break;

      case 0x0801:    // get config parameters
        //  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17
        // FD FC FB FA 08 00 08 01 00 00 64 00 00 00 04 03 02 01
        // header     | len |cw cv| ack |   value   |  footer

        // max distance
        if (hilink_command.param == 1)
        {
          // set gate limits
          dist_max = 10 * HilinkConvertSerialToUint16 (hilink_reception.arr_body[10], hilink_reception.arr_body[11]);
          AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s max distance is %u cm"), hilink_status.str_model, dist_max);
        }

        // detection delay
        else if (hilink_command.param == 4)
        {
          delay = HilinkConvertSerialToUint16 (hilink_reception.arr_body[10], hilink_reception.arr_body[11]);
          AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s delay is %u s"), hilink_status.str_model, delay);
        }

        // power supply interference
        else if (hilink_command.param == 5)
          switch (hilink_reception.arr_body[10])
          {
            case 0: AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s interference test not performed"), hilink_status.str_model); break;
            case 1: AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s no interference detected"), hilink_status.str_model); break;
            case 2: AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s interference detected"), hilink_status.str_model); break;
          }

        // gate motion detection level
        else if ((hilink_command.param >= 16) && (hilink_command.param < 32))
        {
          gate  = hilink_command.param - 16;
          value = HilinkConvertSerialToUint32 (hilink_reception.arr_body[10], hilink_reception.arr_body[11], hilink_reception.arr_body[12], hilink_reception.arr_body[13]);
          level = LD2402ConvertSerial2Level (value);
          hilink_motion.arr_gate[gate].threshold = (uint8_t)level;
          AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s gate %u motion at %d dB [%u]"), hilink_status.str_model, gate + 1, level, value);        
        }

        // gate micro detection level
        else if ((hilink_command.param >= 48) && (hilink_command.param < 64))
        {
          gate  = hilink_command.param - 48;
          value = HilinkConvertSerialToUint32 (hilink_reception.arr_body[10], hilink_reception.arr_body[11], hilink_reception.arr_body[12], hilink_reception.arr_body[13]);
          level = LD2402ConvertSerial2Level (value);
          hilink_static.arr_gate[gate].threshold = (uint8_t)level;
          AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s gate %u micro at %d dB [%u]"), hilink_status.str_model, gate + 1, level, value);        
        }
        break;
    }
  }

  // declare command as finished
  HilinkCommandFinished ();
}

// Handling of received data
void LD2402HandleReceivedEnergy ()
{
  uint8_t  index, gate;
  uint16_t level;
  uint32_t value;

  // log data to console
  HilinkLogHexa (hilink_reception.arr_body, hilink_reception.idx_body, false, false);

  // if enough data received
  if (hilink_reception.idx_body >= 137)
  {
    // set targets activity
    hilink_static.arr_target[0].active = (hilink_reception.arr_body[6] == 0x02);
    hilink_motion.arr_target[0].active = (hilink_reception.arr_body[6] == 0x01);

    // set still target distance
    if (hilink_static.arr_target[0].active) hilink_static.arr_target[0].dist = HilinkConvertSerialToUint16 (hilink_reception.arr_body[7], hilink_reception.arr_body[8]);
      else hilink_static.arr_target[0].dist = 0;

    // set motion target distance
    if (hilink_motion.arr_target[0].active) hilink_motion.arr_target[0].dist = HilinkConvertSerialToUint16 (hilink_reception.arr_body[7], hilink_reception.arr_body[8]);
      else hilink_motion.arr_target[0].dist = 0;

    // motion energy
    index = LD2402_ENERGY_GATE_MOTION;
    hilink_motion.arr_target[0].power = 0;
    for (gate = 0; gate < hilink_status.gate_qty; gate ++ )
    {
      // update gate power
      value = HilinkConvertSerialToUint32 (hilink_reception.arr_body[index++], hilink_reception.arr_body[index++], hilink_reception.arr_body[index++], hilink_reception.arr_body[index++]);
      hilink_motion.arr_gate[gate].power = (uint8_t)LD2402ConvertSerial2Level (value);

      // update target power (percentage of threshold)
      if (hilink_motion.arr_target[0].power < 100)
      {
        if (hilink_motion.arr_gate[gate].threshold == 0) level = 100;
          else if (hilink_motion.arr_gate[gate].power > hilink_motion.arr_gate[gate].threshold) level = 100;
          else level = 100 * (uint16_t)hilink_motion.arr_gate[gate].power / (uint16_t)hilink_motion.arr_gate[gate].threshold;
        hilink_motion.arr_target[0].power = max (hilink_motion.arr_target[0].power, (uint8_t)level);
      }
    }

    // presence energy
    index = LD2402_ENERGY_GATE_STATIC;
    hilink_static.arr_target[0].power = 0;
    for (gate = 0; gate < hilink_status.gate_qty; gate ++ )
    {
      // update gate power
      value = HilinkConvertSerialToUint32 (hilink_reception.arr_body[index++], hilink_reception.arr_body[index++], hilink_reception.arr_body[index++], hilink_reception.arr_body[index++]);
      hilink_static.arr_gate[gate].power = (uint8_t)LD2402ConvertSerial2Level (value);

      // update target power (percentage of threshold)
      if (hilink_static.arr_target[0].power < 100)
      {
        if (hilink_static.arr_gate[gate].threshold == 0) level = 100;
          else if (hilink_static.arr_gate[gate].power > hilink_static.arr_gate[gate].threshold) level = 100;
          else level = 100 * (uint16_t)hilink_static.arr_gate[gate].power / (uint16_t)hilink_static.arr_gate[gate].threshold;
        hilink_static.arr_target[0].power = max (hilink_static.arr_target[0].power, (uint8_t)level);
      }
    }
  }

  // reset reception buffer
  HilinkReceptionEmpty ();
  hilink_reception.energy = false;
}

// Handling of received data
void LD2402HandleReceivedData ()
{
  uint8_t gate;
  char   *pstr_data;
  char   *pstr_distance;

  // convert reception buffer to string
  hilink_reception.arr_body[hilink_reception.idx_body - 2] = 0;
  pstr_data = (char*)hilink_reception.arr_body;

  // log data to console
  HilinkLogText (pstr_data, false);

  // look for distance
  pstr_distance = strchr (pstr_data, ':');
  if (pstr_distance != nullptr) pstr_distance++;

  // update motion target
  hilink_static.arr_target[0].active = (pstr_distance != nullptr);
  if (pstr_distance != nullptr) hilink_static.arr_target[0].dist = (uint16_t)atoi (pstr_distance);
    else hilink_static.arr_target[0].dist = 0;
  hilink_static.arr_target[0].power = 100;

  // reset motion target
  hilink_motion.arr_target[0].active = false;
  hilink_motion.arr_target[0].dist   = 0;
  hilink_motion.arr_target[0].power  = 0;

  // reset energy levels
  for (gate = 0; gate < hilink_status.gate_qty; gate ++)
  {
    hilink_static.arr_gate[0].power = 0;
    hilink_motion.arr_gate[0].power = 0;
  }

  // reset reception buffer
  HilinkReceptionEmpty ();
}

/*********************************************\
 *                   Callback
\*********************************************/

// Handling of next command
void LD2402ProcessCommandQueue ()
{
  bool    cmnd_mode, cmnd_waiting;
  uint8_t command, param;
  char    str_command[16];
  char    str_param[12];

  // check validity
  if (hilink_config.device != HLK_DEVICE_LD2402) return;
  if (hilink_status.pserial == nullptr) return;
  if (!HilinkCommandPossible ()) return;

  // check if commands are waiting
  cmnd_mode    = HilinkIsInCommandMode ();
  cmnd_waiting = HilinkCommandWaiting ();

  // commands in the pipe and command mode not started, start command mode
  if (cmnd_waiting && !cmnd_mode) LD2402SendCommand (LD2402_CMND_START, nullptr);
    
  // else if no command in the queue, stop command mode
  else if (!cmnd_waiting && cmnd_mode) LD2402SendCommand (LD2402_CMND_STOP, nullptr);

  // else send next command
  else if (cmnd_waiting && cmnd_mode)
  {
    HilinkGetNextCommand (str_command, sizeof (str_command));
    HilinkSplitCommand (str_command, command, str_param, sizeof (str_param));
    LD2402SendCommand (command, str_param);
  }
}

// Handling of serial reception
void LD2402SerialReception ()
{
  uint8_t  index, delta;
  uint32_t value;

  // check if enabled
  if (hilink_config.device != HLK_DEVICE_LD2402) return;
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

      // detect energy mode
      else if (value == 0xf4f3f2f1) hilink_reception.energy = true;

      // if receiving data and not in engineering mode
      if (hilink_reception.data && !hilink_reception.energy)
      {
        delta = hilink_reception.idx_body - 2;
        value = HilinkConvertSerialToUint32 (hilink_reception.arr_body[delta + 1], hilink_reception.arr_body[delta], 0, 0);
      }

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
          LD2402HandleReceivedCommand ();
          break;

        case 0xf8f7f6f5:    // energy data footer
          LD2402HandleReceivedEnergy ();
          break;

        case 0x0d0a:        // plain data footer
          LD2402HandleReceivedData ();
          break;
      }
    }
  }
}

/***************************************\
 *              Interface
\***************************************/

// LD2402 sensor
bool XsnsHilinkLD2402 (const uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
    case FUNC_COMMAND:
      result = DecodeCommand (kHLKLD2402Commands, HLKLD2402Command);
      break;

    case FUNC_EVERY_250_MSECOND:
      LD2402ProcessCommandQueue ();
      break;

    case FUNC_EVERY_100_MSECOND:
      LD2402SerialReception ();
      break;
  }

  return result;
}

#endif     // USE_HILINK_LD2402
#endif     // USE_HILINK_DETECTOR
