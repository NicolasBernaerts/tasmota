/*
  xsns_102_02_ld1125.ino - Driver for Presence and Movement sensor HLK-LD1125

  Copyright (C) 2022-2026  Nicolas Bernaerts

  Version history :
    22/06/2022 - v1.0 - Creation
    14/01/2023 - v2.0 - Complete rewrite
    12/09/2023 - v2.1 - Switch to LD2410 Rx & LD2410 Tx
    25/05/2024 - v2.2 - Change help command to ld1125 
    07/06/2026 - v3.0 - Complete rewrite to be used as a generic hilink detector

  Connexions :
    * GPIO1 should be declared as LD2410 Tx and connected to HLK-LD1125 Rx
    * GPIO3 should be declared as LD2410 Rx and connected to HLK-LD1125 Tx

  LD1125 has 3 gates inequal gates but is handled as 4 equal gates :
    1 - between 0 and 2.8m
    2 - between 2.8 and 6m
    3 - between 6 and 9m
    3 - beyond 9m

  Reception sample :
    mov, dis=0.70
    occ, dis=0.65
    occ, dis=0.65
    mov, dis=0.70

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
#ifdef USE_HILINK_LD1125

#include <TasmotaSerial.h>

/*************************************************\
 *               Variables
\*************************************************/

// constant
#define LD1125_START_DELAY          10              // sensor answers after 10 seconds

#define LD1125_RECEPTION_TIMEOUT    5               // reception timeout (sec)

#define LD1125_DIST_MAX             1200            // maximum distance (cm)
#define LD1125_GATE_QUANTITY        12
#define LD1125_GATE_WIDTH           100
#define LD1125_GATE_TH1             3
#define LD1125_GATE_TH2             8

#define LD1125_DATA_RATE            115200

// MQTT commands : ld_help and ld_send
const char kLD1125Commands[]         PROGMEM = "hlk" "|" "_save" "|"    "_reset"   "|"    "_th"      ;
void (* const LD1125Command[])(void) PROGMEM = {   &CmndLD1125Save, &CmndLD1125Reset, &CmndLD1125Th };

/**************************************************\
 *                  Commands
\**************************************************/

void CmndLD1125Save (void)
{
  HilinkAppendCommand ("save");
  ResponseCmndDone ();
}

void CmndLD1125Reset (void)
{
  HilinkAppendCommand ("rmax=6");
  HilinkAppendCommand ("mth1_mov=60");
  HilinkAppendCommand ("mth2_mov=30");
  HilinkAppendCommand ("mth3_mov=20");
  HilinkAppendCommand ("mth1_occ=60");
  HilinkAppendCommand ("mth2_occ=30");
  HilinkAppendCommand ("mth3_occ=20");
  HilinkAppendCommand ("test_mode=1");
  HilinkAppendCommand ("get_all");
  ResponseCmndDone ();
}

void CmndLD1125Th ()
{
  uint8_t percent = 0;
  int     index = 0;
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
    if (pstr_level != nullptr) percent = (uint8_t)atoi (pstr_level);
  }

  if ((index > 0) && (percent > 0))
  {
    // append command
    sprintf_P (str_command, PSTR ("mth%d_occ=%u"), index, percent);
    HilinkAppendCommand (str_command);
    sprintf_P (str_command, PSTR ("mth%d_mov=%u"), index, percent);
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

void LD1125DeviceCommand (const uint8_t command, const uint8_t context)
{
  uint16_t distance;
  char     str_command[16];

  switch (command)
  {
    // display device help
    case HILINK_CMND_HELP:
      AddLog (LOG_LEVEL_INFO, PSTR ("%s specific commands :"), hilink_status.str_model);
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_reset      = reset to default parameters"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_save       = save parameters"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  hlk_th idx val = sensor threshold (%%)"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    idx  : 1 .. 3      (sensor index)"));
      AddLog (LOG_LEVEL_INFO, PSTR ("    val  : 1 .. 100    (sensor threshold)"));
      break;

    // init device parameters
    case HILINK_CMND_INIT:
      // declare device
      hilink_status.baudrate   = LD1125_DATA_RATE;
      hilink_status.gate_qty   = LD1125_GATE_QUANTITY;
      hilink_status.gate_width = LD1125_GATE_WIDTH;
      if (hilink_status.dist_limit > LD1125_DIST_MAX) hilink_status.dist_limit = LD1125_DIST_MAX;

      // set presence and motion detection status
      hilink_static.enabled = true;
      hilink_motion.enabled = true;

      // no bluetooth available on the device
      hilink_config.param.bluetooth = 0;
      break;

    // device environment initialisation
    case HILINK_CMND_FIRST:
      // set initial delay
      HilinkSetCommandDelay (LD1125_START_DELAY);

      // init distance
      sprintf_P (str_command, PSTR ("rmax=%u"), max (hilink_config.pres_max, hilink_config.move_max) / 100);
      HilinkAppendCommand (str_command);

      // init commands
      HilinkAppendCommand ("test_mode=1");
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
      break;
      
    // set minimum detection distance
    case HILINK_CMND_DMIN:
      break;

    // set maximum detection distance
    case HILINK_CMND_DMAX:
      // check distance validity
      distance = max (hilink_config.pres_max, hilink_config.move_max);
      if (distance < 100) return;

      // add command
      sprintf_P (str_command, PSTR ("rmax=%u"), distance / 100);
      HilinkAppendCommand (str_command);
      break;
    
    // append JSON according to context
    case HILINK_CMND_JSON:
      switch (context)
      {
        case HILINK_JSON_GENERAL:  ResponseAppend_P (PSTR (",\"Dist\":%u"),              HilinkGetDistance (HILINK_JSON_GENERAL));                                         break;
        case HILINK_JSON_PRESENCE: ResponseAppend_P (PSTR (",\"Dist\":%u,\"Power\":%u"), HilinkGetDistance (HILINK_JSON_PRESENCE), HilinkGetPower (HILINK_JSON_PRESENCE)); break;
        case HILINK_JSON_MOTION:   ResponseAppend_P (PSTR (",\"Dist\":%u,\"Power\":%u"), HilinkGetDistance (HILINK_JSON_MOTION),   HilinkGetPower (HILINK_JSON_MOTION));   break;
      } 
      break;
  }
}

/**************************************************\
 *                  Functions
\**************************************************/

void LD1125ExecuteNextCommand ()
{
  char str_command[64];

  // if at least one command is pending
  if (HilinkCommandPossible () && HilinkCommandWaiting ())
  {
    // get command
    HilinkGetNextCommand (str_command, sizeof (str_command));

    // log command
    HilinkLogText (str_command, true);

    // send command
    strcat_P (str_command, PSTR ("\r\n"));
    hilink_status.pserial->write (str_command, strlen (str_command));
  }
}

/*********************************************\
 *                   Callback
\*********************************************/

// loop every 250 msecond
void LD1125ProcessCommandQueue ()
{
  char str_command[64];
    
  // if not enabled, ignore
  if (hilink_config.device != HLK_DEVICE_LD1125) return;
  if (hilink_status.pserial == nullptr) return;
  if (!HilinkCommandPossible ()) return;

  // if at least one command is pending
  if (HilinkCommandWaiting ())
  {
    // get command
    HilinkGetNextCommand (str_command, sizeof (str_command));

    // log command
    HilinkLogText (str_command, true);

    // send command
    strcat_P (str_command, PSTR ("\r\n"));
    hilink_status.pserial->write (str_command, strlen (str_command));
  }

  // check for reception timeout
  if (TimeReached (hilink_static.timestamp)) hilink_static.arr_target[0].active = false;
  if (TimeReached (hilink_motion.timestamp)) hilink_motion.arr_target[0].active = false;
}

// Handling of received data
void LD1125SerialReception ()
{
  uint8_t  gate, recv_data;
  uint16_t distance;
  int      value;
  char    *pstr_key;
  char    *pstr_value;
  char    *pstr_power;
  char    *pstr_buffer;
  char     str_key[16];

  // check sensor presence
  if (hilink_config.device != HLK_DEVICE_LD1125) return;
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
        if (strstr_P (pstr_buffer, PSTR ("occ,")) == pstr_buffer)
        {
          // log received data
          HilinkLogText (pstr_buffer, false);

          // reset motion data
          hilink_motion.timestamp = 0;
          hilink_motion.arr_target[0].active = false;
          hilink_motion.arr_target[0].dist   = 0;
          hilink_motion.arr_target[0].power  = 0;

          // update presence timeout
          hilink_static.timestamp = millis () + LD1125_RECEPTION_TIMEOUT * 1000;
          
          // if distance is available
          pstr_value = strstr_P (pstr_buffer, PSTR ("dis="));
          if (pstr_value != nullptr)
          {
            hilink_static.arr_target[0].active = true;
            hilink_static.arr_target[0].dist   = (uint16_t)(100 * atof (pstr_value + 4));
          }

          // else distance not available
          else
          {
            hilink_static.arr_target[0].active = false;
            hilink_static.arr_target[0].dist   = 0;
          }

          // check power
          pstr_power = strstr_P (pstr_buffer, PSTR ("str="));
          if (pstr_power != nullptr) hilink_static.arr_target[0].power = (uint8_t)atoi (pstr_power + 4);
        }

        // if received motion level
        else if (strstr_P (pstr_buffer, PSTR ("mov,")) == pstr_buffer)
        {
          // log received data
          HilinkLogText (pstr_buffer, false);

          // reset presence data
          hilink_static.timestamp = 0;
          hilink_static.arr_target[0].active = false;
          hilink_static.arr_target[0].dist   = 0;
          hilink_static.arr_target[0].power  = 0;

          // update motion timeout
          hilink_motion.timestamp = millis () + LD1125_RECEPTION_TIMEOUT * 1000;

          // if distance is available
          pstr_value = strstr_P (pstr_buffer, PSTR ("dis="));
          if (pstr_value != nullptr)
          {
            hilink_motion.arr_target[0].active = true;
            hilink_motion.arr_target[0].dist   = (uint16_t)(100 * atof (pstr_value + 4));
          }

          // else distance not available
          else
          {
            hilink_motion.arr_target[0].active = false;
            hilink_motion.arr_target[0].dist   = 0;
          }

          // check power
          pstr_power = strstr_P (pstr_buffer, PSTR ("str="));
          if (pstr_power != nullptr) hilink_motion.arr_target[0].power = (uint8_t)atoi (pstr_power + 4); 
        }

        // else if received configuration value
        else if (strstr_P (pstr_buffer, PSTR (" is ")) != nullptr)
        {
          // log received data
          AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s"), pstr_buffer);

          // init
          value = INT_MAX;
          strcpy (str_key, "");
          pstr_value = nullptr;

          // look for key and value
          pstr_key = strchr (pstr_buffer, ' ');
          if (pstr_key != nullptr) pstr_value = strchr (pstr_key + 1, ' ');

          // extract key
          if (pstr_key != nullptr)
          {
            *pstr_key = 0;
            strlcpy (str_key, pstr_buffer, sizeof (str_key));
          }

          // extract value
          if (pstr_value != nullptr) value = atoi (pstr_value + 1);

          // update value according to key
          if ((strlen (str_key) > 0) && (value != INT_MAX))
          {
            // presence
            if (strstr_P (str_key, PSTR ("mth1_occ")) != nullptr) for (gate = 0; gate < LD1125_GATE_TH1; gate ++)                         hilink_static.arr_gate[gate].threshold = (uint8_t)value;
            else if (strstr_P (str_key, PSTR ("mth2_occ")) != nullptr) for (gate = LD1125_GATE_TH1; gate < LD1125_GATE_TH2; gate ++)      hilink_static.arr_gate[gate].threshold = (uint8_t)value;
            else if (strstr_P (str_key, PSTR ("mth3_occ")) != nullptr) for (gate = LD1125_GATE_TH2; gate < LD1125_GATE_QUANTITY; gate ++) hilink_static.arr_gate[gate].threshold = (uint8_t)value;

            // motion
            else if (strstr_P (str_key, PSTR ("mth1_mov")) != nullptr) for (gate = 0; gate < LD1125_GATE_TH1; gate ++)                    hilink_motion.arr_gate[gate].threshold = (uint8_t)value;
            else if (strstr_P (str_key, PSTR ("mth2_mov")) != nullptr) for (gate = LD1125_GATE_TH1; gate < LD1125_GATE_TH2; gate ++)      hilink_motion.arr_gate[gate].threshold = (uint8_t)value;
            else if (strstr_P (str_key, PSTR ("mth3_mov")) != nullptr) for (gate = LD1125_GATE_TH2; gate < LD1125_GATE_QUANTITY; gate ++) hilink_motion.arr_gate[gate].threshold = (uint8_t)value;
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
bool XsnsHilinkLD1125 (const uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function) 
  {
    case FUNC_COMMAND:
      if (hilink_config.device == HLK_DEVICE_LD1125) result = DecodeCommand (kLD1125Commands, LD1125Command);
      break;

    case FUNC_EVERY_250_MSECOND:
      LD1125ProcessCommandQueue ();
      break;

    case FUNC_EVERY_100_MSECOND:
      LD1125SerialReception ();
      break;
  }

  return result;
}

#endif      // USE_HILINK_LD1125
#endif      // USE_HILINK_DETECTOR
