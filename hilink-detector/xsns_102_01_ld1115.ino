/*
  xsns_102_01_ld1115.ino - Driver for Presence and Movement sensor HLK-LD1115

  Copyright (C) 2022-2026  Nicolas Bernaerts

  Version history :
    22/06/2022 - v1.0 - Creation
    12/01/2023 - v2.0 - Complete rewrite
    12/09/2023 - v2.1 - Switch to LD2410 Rx & LD2410 Tx
    25/05/2024 - v2.2 - Change help command to ld1115 
    10/06/2026 - v3.0 - Complete rewrite to be used as a generic hilink detector 

  Connexions :
    * GPIO1 should be declared as LD2410 Tx and connected to HLK-LD1115 Rx
    * GPIO3 should be declared as LD2410 Rx and connected to HLK-LD1115 Tx

  Reception sample :
    occ, 2 23242
    occ, 2 22974
    occ, 2 22974
    occ, 4 22974
    mov, 2 296
    mov, 2 7080

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
#ifdef USE_HILINK_LD1115

#include <TasmotaSerial.h>

/*************************************************\
 *               Variables
\*************************************************/

// constant
#define LD1115_START_DELAY          10              // sensor answers after 10 seconds
#define LD1115_RECEPTION_TIMEOUT    2000            // inactivity after 2 sec

#define LD1115_MOTION_DIST_MAX      1600            // maximum detection distance (cm)
#define LD1115_PRESEN_DIST_MAX      500             // maximum detection distance (cm)
#define LD1115_DEFAULT_DIST         100             // presence detection average distance (cm)

#define LD1115_GATE_QUANTITY        16              // number of gates
#define LD1115_GATE_WIDTH           100             // gate width (cm)

#define LD1115_DATA_RATE            115200

// MQTT commands : ld_help and ld_send
const char kLD1115Commands[]         PROGMEM = "hlk" "|" "_save" "|"    "_reset"   "|"    "_th"   "|"    "_sn"      ;
void (* const LD1115Command[])(void) PROGMEM = {   &CmndLD1115Save, &CmndLD1115Reset, &CmndLD1115Th, &CmndLD1115Sn };

/**************************************************\
 *                  Commands
\**************************************************/

/*
// hlk-ld sensor help
void CmndLD1115Help ()
{
  AddLog (LOG_LEVEL_INFO, PSTR ("%s specific commands :"), hilink_status.str_model);
  AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_reset      = reset parameters to default"));
  AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_save       = save parameters to sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_th idx val = sensor threshold (%%)"));
  AddLog (LOG_LEVEL_INFO, PSTR ("    idx  : 1 .. 3      (sensor index)"));
  AddLog (LOG_LEVEL_INFO, PSTR ("    val  : 1 .. 100000 (sensor threshold)"));
  AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_sn type num = set samples to average"));
  AddLog (LOG_LEVEL_INFO, PSTR ("    type : occ (presence), mov (motion)"));
  AddLog (LOG_LEVEL_INFO, PSTR ("    num  : number of samples"));
  ResponseCmndDone ();
}
*/

void CmndLD1115Save ()
{
  HilinkAppendCommand ("save");
  ResponseCmndDone ();
}

void CmndLD1115Reset ()
{
  HilinkAppendCommand ("th1=120");
  HilinkAppendCommand ("th2=250");
  HilinkAppendCommand ("mov_sn=3");
  HilinkAppendCommand ("occ_sn=5");
  HilinkAppendCommand ("dtime=5");
  HilinkAppendCommand ("ind_min=2");
  HilinkAppendCommand ("ind_max=32");
  HilinkAppendCommand ("get_all");
  ResponseCmndDone ();
}

void CmndLD1115Th ()
{
  uint8_t gate, percent;
  int     index = 0;
  long    level = 0;
  char   *pstr_level;
  char    str_data[16];
  char    str_command[16];

  if (XdrvMailbox.data_len > 0)
  {
    // get gate number
    strcpy (str_data, XdrvMailbox.data);
    pstr_level = strchr (str_data, ' ');
    if (pstr_level != nullptr) *pstr_level++ = 0;
    
    // get gate target
    index = atoi (str_data);
    if (index > 3) index = 0;

    // get level
    if (pstr_level != nullptr) level = atol (pstr_level);
  }

  if ((index > 0) && (level > 0))
  {
    // append command
    sprintf_P (str_command, PSTR ("th%d=%d"), index, level);
    HilinkAppendCommand (str_command);
    HilinkAppendCommand ("get_all");

    // set all gates threshold
    percent = LD1115ConvertLevel2Percentage (level);
    switch (index)
    {
      case 1:
        for (gate = 0; gate < LD1115_GATE_QUANTITY; gate ++) hilink_motion.arr_gate[gate].threshold = percent;
        break;

      case 2:
        for (gate = 0; gate < LD1115_GATE_QUANTITY; gate ++) hilink_static.arr_gate[gate].threshold = percent;
        break;
    }

    // command done
    ResponseCmndDone ();
  }

  // command failed
  else ResponseCmndFailed ();
}

void CmndLD1115Sn ()
{
  int   sample;
  char *pstr_type;
  char *pstr_sample;
  char  str_data[16];
  char  str_command[16];

  if (XdrvMailbox.data_len > 0)
  {
    // get gate number
    strcpy (str_data, XdrvMailbox.data);
    pstr_sample = strchr (str_data, ' ');
    if (pstr_sample != nullptr) *pstr_sample++ = 0;

    // get level
    if (pstr_sample != nullptr) sample = atoi (pstr_sample);
  }

  if (sample > 0)
  {
    // append command
    sprintf_P (str_command, PSTR ("%s_sn=%d"), str_data, sample);
    HilinkAppendCommand (str_command);
    HilinkAppendCommand ("get_all");

    // command done
    ResponseCmndDone ();
  }

  // command failed
  else ResponseCmndFailed ();
}

/******************************\
 *       Common commands
\******************************/

void LD1115DeviceCommand (const uint8_t command, const uint8_t context)
{
  uint16_t distance;
  char     str_text[16];

  // handle different common commands
  switch (command)
  {
    // display device help
    case HILINK_CMND_HELP:
      AddLog (LOG_LEVEL_INFO, PSTR ("%s specific commands :"), hilink_status.str_model);
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_reset      = reset parameters to default"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_save       = save parameters to sensor"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_th idx val = sensor threshold (%%)"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    idx  : 1 .. 3      (sensor index)"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    val  : 1 .. 100000 (sensor threshold)"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_sn type num = set samples to average"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    type : occ (presence), mov (motion)"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    num  : number of samples"));
      break;
      
    // init device parameters
    case HILINK_CMND_INIT:
      // declare device
      hilink_status.baudrate   = LD1115_DATA_RATE;
      hilink_status.gate_qty   = LD1115_GATE_QUANTITY;
      hilink_status.gate_width = LD1115_GATE_WIDTH;
      hilink_status.dist_limit = LD1115_MOTION_DIST_MAX;

      // set presence and motion detection status
      hilink_static.enabled = true;
      hilink_motion.enabled = true;

      // no bluetooth available on the device
      hilink_config.param.bluetooth = 0;
      break;

    // device environment initialisation
    case HILINK_CMND_FIRST:
      // set initial delay
      HilinkSetCommandDelay (LD1115_START_DELAY);

      // add startup commands
      sprintf_P (str_text, PSTR ("dtime=%u"), hilink_config.delay);
      HilinkAppendCommand (str_text);

      sprintf_P (str_text, PSTR ("ind_min=%u"), hilink_config.dist_min / LD1115_GATE_WIDTH * 2);
      HilinkAppendCommand (str_text);

      sprintf_P (str_text, PSTR ("ind_max=%u"), max (hilink_config.pres_max, hilink_config.move_max) / LD1115_GATE_WIDTH * 2);
      HilinkAppendCommand (str_text);

      // read all parameters
      HilinkAppendCommand ("get_all");
      break;
      
    // read device data information
    case HILINK_CMND_INFO:
      break;
      
    // read device parameters
    case HILINK_CMND_PARAM:
      HilinkAppendCommand ("get_all");
      break;
       
    // set presence-less delay
    case HILINK_CMND_DELAY:
      sprintf_P (str_text, PSTR ("dtime=%u"), hilink_config.delay);
      HilinkAppendCommand (str_text);
      break;
      
    // set minimum detection distance
    case HILINK_CMND_DMIN:
      distance = hilink_config.dist_min;
      if (distance < LD1115_GATE_WIDTH) distance = LD1115_GATE_WIDTH;
      sprintf_P (str_text, PSTR ("ind_min=%u"), distance / LD1115_GATE_WIDTH * 2);
      HilinkAppendCommand (str_text);
      break;

    // set maximum detection distance
    case HILINK_CMND_DMAX:
      distance = max (hilink_config.pres_max, hilink_config.move_max);
      if (distance > LD1115_MOTION_DIST_MAX) distance = LD1115_MOTION_DIST_MAX;
      sprintf_P (str_text, PSTR ("ind_max=%u"), distance / LD1115_GATE_WIDTH * 2);
      HilinkAppendCommand (str_text);
      break;
    
    // append JSON according to context
    case HILINK_CMND_JSON:
      switch (context)
      {
        case HILINK_JSON_PRESENCE: ResponseAppend_P (PSTR (",\"Power\":%u"), HilinkGetDistance (HILINK_JSON_PRESENCE), HilinkGetPower (HILINK_JSON_PRESENCE)); break;
        case HILINK_JSON_MOTION:   ResponseAppend_P (PSTR ("\"Power\":%u"),  HilinkGetDistance (HILINK_JSON_MOTION),   HilinkGetPower (HILINK_JSON_MOTION));   break;
      } 
      break;
  }
}

/*********************************************\
 *                Conversion
\*********************************************/

uint8_t LD1115ConvertLevel2Percentage (const long level)
{
  float base, exponent, percent;

  base     = (float)level / 100000;
  exponent = 0.258976;
  percent  = 100 * pow (base, exponent);
  if (percent > 100) percent = 100;

  return (uint8_t)percent;
}

/*********************************************\
 *                   Callback
\*********************************************/

// loop every 250 msecond
void LD1115ProcessCommandQueue ()
{
  char str_command[64];

    // if not valid, ignore
  if (hilink_config.device != HLK_DEVICE_LD1115) return;
  if (hilink_status.pserial == nullptr) return;
  if (!HilinkCommandPossible ()) return;

  // if at least one command is pending
  if (HilinkCommandWaiting ())
  {
    // send command
    HilinkGetNextCommand (str_command, sizeof (str_command));
    hilink_status.pserial->write (str_command, strlen (str_command));

    // log command
    HilinkLogText (str_command, true);
  }

  // check for reception timeout
  if (TimeReached(hilink_static.timestamp)) hilink_static.arr_target[0].active = false;
  if (TimeReached(hilink_motion.timestamp)) hilink_motion.arr_target[0].active = false;
}

// Handling of received data
void LD1115SerialReception ()
{
  uint8_t  gate, limit, level, recv_data;
  uint16_t distance;
  long     value;
  char    *pstr_key;
  char    *pstr_value;
  char    *pstr_buffer;
  char     str_key[16];

  // check sensor presence
  if (hilink_config.device != HLK_DEVICE_LD1115) return;
  if (hilink_status.pserial == nullptr) return;

  // init strings
  str_key[0]  = 0;
  pstr_buffer = (char*)hilink_reception.arr_body;

  // run serial receive loop
  while (hilink_status.pserial->available ()) 
  {
    // handle received character
    recv_data = hilink_status.pserial->read (); 
    switch (recv_data)
    {
      // CR is ignored
      case 0x0D:
        break;
          
      // LF needs line analysis
      case 0x0A:
        // if received static level
        if (strstr_P (pstr_buffer, PSTR ("occ")) == pstr_buffer)
        {
          // log received data
          HilinkLogText (pstr_buffer, false);

          // update presence timeout
          hilink_static.timestamp = millis () + LD1115_RECEPTION_TIMEOUT * 1000;

          // reset motion data
          hilink_motion.timestamp            = 0;
          hilink_motion.arr_target[0].active = false;
          hilink_motion.arr_target[0].dist   = 0;
          hilink_motion.arr_target[0].power  = 0;
          for (gate = 0; gate < LD1115_GATE_QUANTITY; gate ++) hilink_motion.arr_gate[gate].power = 0;

          // extract values
          pstr_value = strrchr (pstr_buffer, ' ');
          if (pstr_value != nullptr) *pstr_value++ = 0;
          if (pstr_value != nullptr)
          {
            level = LD1115ConvertLevel2Percentage (atol (pstr_value));
            hilink_static.arr_target[0].active = true;
            hilink_static.arr_target[0].dist   = LD1115_DEFAULT_DIST;
            hilink_static.arr_target[0].power  = level;
            for (gate = 0; gate < LD1115_GATE_QUANTITY; gate ++) hilink_static.arr_gate[gate].power = level;
          }
        }

        // else if received motion level
        else if (strstr_P (pstr_buffer, PSTR ("mov")) == pstr_buffer)
        {
          // log received data
          HilinkLogText (pstr_buffer, false);

          // reset presence data
          hilink_static.timestamp            = 0;
          hilink_static.arr_target[0].active = false;
          hilink_static.arr_target[0].dist   = 0;
          hilink_static.arr_target[0].power  = 0;
          for (gate = 0; gate < LD1115_GATE_QUANTITY; gate ++) hilink_static.arr_gate[gate].power = 0;

          // update motion timeout
          hilink_motion.timestamp = millis () + LD1115_RECEPTION_TIMEOUT * 1000;

          // extract values
          pstr_value = strrchr (pstr_buffer, ' ');
          if (pstr_value != nullptr) *pstr_value++ = 0;
          if (pstr_value != nullptr)
          {
            level = LD1115ConvertLevel2Percentage (atol (pstr_value));
            hilink_motion.arr_target[0].active = true;
            hilink_motion.arr_target[0].dist   = LD1115_DEFAULT_DIST;
            hilink_motion.arr_target[0].power  = level;
            for (gate = 0; gate < LD1115_GATE_QUANTITY; gate ++) hilink_motion.arr_gate[gate].power = level;
          }
        }

        // else if received configuration value
        else if (strstr_P (pstr_buffer, PSTR (" is ")) != nullptr)
        {
          // log command data
          AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s"), pstr_buffer);

          // init
          value = LONG_MAX;
          str_key[0] = 0;
          pstr_value = nullptr;

          // look for key and value
          pstr_key = strchr (pstr_buffer, ' ');
          if (pstr_key != nullptr) *pstr_key++ = 0;
          if (pstr_key != nullptr) pstr_value = strchr (pstr_key, ' ');

          // extract key
          if (pstr_key != nullptr)
          {
            *pstr_key = 0;
            strlcpy (str_key, pstr_buffer, sizeof (str_key));
          }

          // extract value
          if (pstr_value != nullptr) pstr_value++;
          if (pstr_value != nullptr) value = atol (pstr_value);

          // update value according to key
          if ((strlen (str_key) > 0) && (value != LONG_MAX))
          {
            // calculate threshold percentage
            level = LD1115ConvertLevel2Percentage (value);

            // get th1 value
            if (strstr_P (str_key, PSTR ("th1")) != nullptr)
              for (gate = 0; gate < LD1115_GATE_QUANTITY; gate ++) hilink_motion.arr_gate[gate].threshold = level;

            else if (strstr_P (str_key, PSTR ("th2")) != nullptr)
              for (gate = 0; gate < LD1115_GATE_QUANTITY; gate ++) hilink_static.arr_gate[gate].threshold = level;
          }
        }

        // declare command as finished
        HilinkCommandFinished ();
        break;

      // default : add current caracter to buffer
      default:
        HilinkReceptionAppend (recv_data);
        break;
    }
  }
}

/***************************************\
 *              Interface
\***************************************/

// Teleinfo sensor
bool XsnsHilinkLD1115 (const uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function) 
  {
    case FUNC_COMMAND:
      if (hilink_config.device == HLK_DEVICE_LD1115) result = DecodeCommand (kLD1115Commands, LD1115Command);
      break;

    case FUNC_EVERY_250_MSECOND:
      LD1115ProcessCommandQueue ();
      break;

    case FUNC_EVERY_100_MSECOND:
      LD1115SerialReception ();
      break;
  }

  return result;
}

#endif      // USE_HILINK_LD1115
#endif      // USE_HILINK_DETECTOR

