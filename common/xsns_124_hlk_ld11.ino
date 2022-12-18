/*
  xsns_124_hlkld11.ino - Driver for Presence and Movement sensor HLK-LD11xx

  Copyright (C) 2022  Nicolas Bernaerts

  Connexions :
    * GPIO1 (Tx) should be declared as Serial Tx and connected to HLK-LDxx Rx
    * GPIO3 (Rx) should be declared as Serial Rx and connected to HLK-LDxx Tx

  Version history :
    22/06/2022 - v1.0   - Creation

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

#ifdef USE_HLKLD11

#include <TasmotaSerial.h>

/*************************************************\
 *               Variables
\*************************************************/

// declare teleinfo energy driver and sensor
#define XSNS_124                   124

// constant
#define HLKLD_DEFAULT_TIMEOUT      5      // inactivity after 5 sec
#define HLKLD_SENSOR_MAX           2      // sensor for mov & occ
#define HLKLD_RANGE_MAX            3      // maximum of 3 ranges for each sensor

// web pages
#define D_HLKLD_PAGE_CONFIG       "ld-conf"
#define D_HLKLD_PAGE_CONFIG_UPD   "ld-upd"

// strings
const char D_HLKLD_NAME[]        PROGMEM = "HLK-LD";
const char D_HLKLD_CONFIGURE[]   PROGMEM = "Configure";


// type of message
enum HLKLDDataType { HLKLD_DATA_OCC, HLKLD_DATA_MOV, HLKLD_DATA_ANY, HLKLD_DATA_MAX };          // sensor types
const char HLKLDDataLabel[] PROGMEM = "occ|mov|any|";                                           // sensor types description
const char HLKLDDataIcon[] PROGMEM = "🧍|🏃|👋|";                                               // sensor types icon

// type of received lines
enum HLKLDLineType  { HLKLD_LINE_OCC, HLKLD_LINE_MOV, HLKLD_LINE_CONFIG, HLKLD_LINE_MAX };      // received line types

// mov and occ parameters available
enum HLKLDParam  { HLKLD_PARAM_TH1, HLKLD_PARAM_TH2, HLKLD_PARAM_TH3, HLKLD_PARAM_MTH1_MOV, HLKLD_PARAM_MTH2_MOV, HLKLD_PARAM_MTH3_MOV, HLKLD_PARAM_MTH1_OCC, HLKLD_PARAM_MTH2_OCC, HLKLD_PARAM_MTH3_OCC, HLKLD_PARAM_MAX };         // device configuration modes
const char HLKLDParamLabel[] PROGMEM = "th1|th2|th3|mth1_mov|mth2_mov|mth3_mov|mth1_occ|mth2_occ|mth3_occ";                                                  // device mode labels

// known models
enum HLKLDModel { HLKLD_MODEL_1115H, HLKLD_MODEL_1125H, HLKLD_MODEL_MAX };              // device reference index
const char HLKLDModelName[] PROGMEM = "LD1115H|LD1125H|Unknown";                        // device reference name
const char HLKLDModelSignature[] PROGMEM = "th1 th2 th3 ind_min ind_max mov_sn occ_sn dtime|test_mode rmax mth1_mov mth2_mov mth3_mov mth1_occ mth2_occ mth3_occ eff_th accu_num|";                                                  // device mode labels
const char HLKLDModelDefault[]   PROGMEM = "th1=120;th2=250;dtime=5;mov_sn=3;occ_sn=5;get_all|test_mode=0;rmax=6;mth1_mov=60;mth2_mov=30;mth3_mov=20;mth1_occ=60;mth2_occ=30;mth3_occ=20;get_all|";                                                  // device mode labels

// MQTT commands : ld_help and ld_send
const char kHLKLDCommands[]         PROGMEM = "ld_|help|cmnd|conf|def";
void (* const HLKLDCommand[])(void) PROGMEM = { &CmndHLKLDHelp, &CmndHLKLDCommand, &CmndHLKLDConfig, &CmndHLKLDDefault };

/****************************************\
 *                 Data
\****************************************/

// HLK-LD sensor general status
static struct {
  bool     get_all     = false;                   // flag to update current config
  bool     display_all = false;                   // display presence and movement or global status
  char     str_buffer[32];                        // buffer of received data
  String   str_command;                           // list of next commands to send (separated by ;)
  String   str_config;                            // returned configuration (param1=value1,param2=value2,...)
  String   str_signature;                         // device signature
  TasmotaSerial *pserial = nullptr;               // pointer to serial port
} hlkld_status; 

// HLK-LD sensor range data
struct hlkld_range {
  uint8_t  type;                                  // type of range parameter
  uint32_t time;                                  // timestamp of last detection
  long     power_def;                             // default reference power for detection
  long     power_ref;                             // actual reference power for detection
  long     power_min;                             // actual minimum power detected
  long     power_last;                            // last power detected
  float    dist_max;                              // maximum distance detection
  float    dist_last;                             // actual distance detection
};

// HLK-LD sensor device data
static struct {
  uint8_t  model   = HLKLD_MODEL_MAX;                                 // device model
  uint16_t timeout = HLKLD_DEFAULT_TIMEOUT;                           // default timeout
  uint32_t index   = UINT32_MAX;                                      // virtual switch index
  struct   hlkld_range sensor[HLKLD_SENSOR_MAX][HLKLD_RANGE_MAX];     // movement and presence sensor data
} hlkld_device;

/**************************************************\
 *                  Commands
\**************************************************/

// hlk-ld sensor help
void CmndHLKLDHelp ()
{
  char str_signature[128];

  // get signature
  if (hlkld_device.model == HLKLD_MODEL_MAX) strlcpy (str_signature, hlkld_status.str_signature.c_str (), sizeof (str_signature));
  else GetTextIndexed (str_signature, sizeof (str_signature), hlkld_device.model, HLKLDModelSignature);

  // help on command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: HLK-LD11xx sensor commands :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ld_conf = get last configuration from sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ld_def  = set all default parameters to sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ld_cmnd = send command to sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR ("      get_all, save or parameter=value"));
  AddLog (LOG_LEVEL_INFO, PSTR ("      parameter can be : %s"), str_signature);

  ResponseCmndDone();
}

void CmndHLKLDCommand (void)
{
  // if data is valid, add command
  if (XdrvMailbox.data_len > 0)
  {
    // add new command to the queue
    HLKLDAddCommand (XdrvMailbox.data, false);

    // command done
    ResponseCmndDone();
  }

  // else command failed
  else ResponseCmndFailed();
}

void CmndHLKLDConfig (void)
{
  ResponseCmndChar (hlkld_status.str_config.c_str ());
}

void CmndHLKLDDefault (void)
{
  char str_default[64];

  // if data is valid, add command
  if (hlkld_device.model != HLKLD_MODEL_MAX)
  {
    // get default command
    GetTextIndexed (str_default, sizeof (str_default), hlkld_device.model, HLKLDModelDefault);
    HLKLDAddCommand (str_default, true);

    // command done
    ResponseCmndDone();
  }

  // else command failed
  else ResponseCmndFailed();
}

/**************************************************\
 *                  Functions
\**************************************************/

// driver initialisation
bool HLKLDInitDevice (uint32_t index, uint32_t timeout = HLKLD_DEFAULT_TIMEOUT)
{
  bool done;

  // set timeout and switch index
  hlkld_device.timeout = timeout;
  hlkld_device.index   = index;

  // check if already initialised
  done = (hlkld_status.pserial != nullptr);

  // if ports are selected, init sensor state
  if (!done && PinUsed (GPIO_TXD) && PinUsed (GPIO_RXD))
  {
#ifdef ESP32
    // create serial port
    hlkld_status.pserial = new TasmotaSerial (Pin (GPIO_RXD), Pin (GPIO_TXD), 1);

    // initialise serial port
    done = hlkld_status.pserial->begin (115200, SERIAL_8N1);

#else       // ESP8266
    // create serial port
    hlkld_status.pserial = new TasmotaSerial (Pin (GPIO_RXD), Pin (GPIO_TXD), 1);

    // initialise serial port
    done = hlkld_status.pserial->begin (115200, SERIAL_8N1);

    // force hardware configuration on ESP8266
    if (hlkld_status.pserial->hardwareSerial ()) ClaimSerial ();
#endif      // ESP32 & ESP8266

    // flush data
    if (done) hlkld_status.pserial->flush ();
  }

  // init device
  if (done) AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s sensor init succesfull"), D_HLKLD_NAME);
  else AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s sensor init failed"), D_HLKLD_NAME);

  return done;
}

void HLKLDSetSensorType (const uint8_t type)
{
  char str_sensor[32];

  // check parameter
  if (type >= HLKLD_MODEL_MAX) return;

  // set default parameters according to sensor type
  switch (type)
  {
    case HLKLD_MODEL_1115H:
      hlkld_device.model = HLKLD_MODEL_1115H;
      hlkld_device.sensor[HLKLD_DATA_MOV][0].type      = HLKLD_PARAM_TH1;
      hlkld_device.sensor[HLKLD_DATA_MOV][0].power_def = 120;
      hlkld_device.sensor[HLKLD_DATA_MOV][0].dist_max  = 16;
      hlkld_device.sensor[HLKLD_DATA_OCC][0].type      = HLKLD_PARAM_TH2;
      hlkld_device.sensor[HLKLD_DATA_OCC][0].power_def = 250;
      hlkld_device.sensor[HLKLD_DATA_OCC][0].dist_max  = 16;
      break;
      
    case HLKLD_MODEL_1125H:
      hlkld_device.model = HLKLD_MODEL_1125H;
      hlkld_device.sensor[HLKLD_DATA_MOV][0].type      = HLKLD_PARAM_MTH1_MOV;
      hlkld_device.sensor[HLKLD_DATA_MOV][0].power_def = 60;
      hlkld_device.sensor[HLKLD_DATA_MOV][0].dist_max  = 2.8;
      hlkld_device.sensor[HLKLD_DATA_OCC][0].type      = HLKLD_PARAM_MTH1_OCC;
      hlkld_device.sensor[HLKLD_DATA_OCC][0].power_def = 60;
      hlkld_device.sensor[HLKLD_DATA_OCC][0].dist_max  = 2.8;
      hlkld_device.sensor[HLKLD_DATA_MOV][1].type      = HLKLD_PARAM_MTH2_MOV;
      hlkld_device.sensor[HLKLD_DATA_MOV][1].power_def = 30;
      hlkld_device.sensor[HLKLD_DATA_MOV][1].dist_max  = 8;
      hlkld_device.sensor[HLKLD_DATA_OCC][1].type      = HLKLD_PARAM_MTH2_OCC;
      hlkld_device.sensor[HLKLD_DATA_OCC][1].power_def = 30;
      hlkld_device.sensor[HLKLD_DATA_OCC][1].dist_max  = 8;
      hlkld_device.sensor[HLKLD_DATA_MOV][2].type      = HLKLD_PARAM_MTH3_MOV;
      hlkld_device.sensor[HLKLD_DATA_MOV][2].power_def = 20;
      hlkld_device.sensor[HLKLD_DATA_MOV][2].dist_max  = 20;
      hlkld_device.sensor[HLKLD_DATA_OCC][2].type      = HLKLD_PARAM_MTH3_OCC;
      hlkld_device.sensor[HLKLD_DATA_OCC][2].power_def = 20;
      hlkld_device.sensor[HLKLD_DATA_OCC][2].dist_max  = 20;
      break;
  }

  // apply full config
  HLKLDApplyConfig ();

  // log detection
  GetTextIndexed (str_sensor, sizeof (str_sensor), (uint32_t)type, HLKLDModelName);
  AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s sensor detected"), str_sensor);
}

void HLKLDSetPowerFromDistance (const uint8_t sensor, const float distance, const long power)
{
  uint8_t index = UINT8_MAX;

  // check parameters
  if (sensor >= HLKLD_DATA_ANY) return;

  // check which range is targeted
  if (isnan (distance)) index = 0;
  else if (isnan (hlkld_device.sensor[sensor][0].dist_max)) index = 0;
  else if (distance <= hlkld_device.sensor[sensor][0].dist_max) index = 0;
  else if (!isnan (hlkld_device.sensor[sensor][1].dist_max) && (distance <= hlkld_device.sensor[sensor][1].dist_max)) index = 1;
  else if (!isnan (hlkld_device.sensor[sensor][2].dist_max) && (distance <= hlkld_device.sensor[sensor][2].dist_max)) index = 2;


  // if sensor has been targeted, update data
  if (index != UINT8_MAX) HLKLDSetPowerAndDistance (sensor, index, power, distance);
}

void HLKLDApplyConfig ()
{
  int idx_start = 0;
  int idx_next  = 0;
  int idx_equal, idx_stop;

  // check parameters
  if (hlkld_status.str_config.length () == 0) return;

  // loop thru config keys
  while (idx_next != -1)
  {
    // get = and , separators
    idx_equal = hlkld_status.str_config.indexOf ('=', idx_start);
    idx_next  = hlkld_status.str_config.indexOf (',', idx_start);

    // if key and value are defined, extract them
    if (idx_next  == -1) idx_stop = hlkld_status.str_config.length (); else idx_stop = idx_next;
    if (idx_equal != -1) HLKLDSetDeviceConfig (hlkld_status.str_config.substring (idx_start, idx_equal).c_str (), hlkld_status.str_config.substring (idx_equal + 1, idx_stop).c_str ());

    // shift to next value
    idx_start = idx_next + 1; 
  }
}

struct hlkld_range* HLKLDGetRangeFromIndex (const uint8_t type)
{
  uint8_t sensor, range;
  struct  hlkld_range *prange = nullptr;

  // if parameter is known, search within existing sensor/range
  if (type < HLKLD_PARAM_MAX)
  {
    for (sensor = 0; sensor < HLKLD_SENSOR_MAX; sensor ++)
      for (range = 0; range < HLKLD_RANGE_MAX; range ++)
        if (hlkld_device.sensor[sensor][range].type == type) prange = &hlkld_device.sensor[sensor][range];
  }

  return prange;
}

struct hlkld_range* HLKLDGetRangeFromKey (const char* pstr_key)
{
  int    type;
  char   str_label[16];
  struct hlkld_range *prange = nullptr;

  // get parameter index and search within existing sensor/range
  type = GetCommandCode (str_label, sizeof (str_label), pstr_key, HLKLDParamLabel);
  if (type != -1) prange = HLKLDGetRangeFromIndex ((uint8_t)type);

  return prange;
}

// Get configuration update
struct hlkld_range* HLKLDGetLastActiveSensor ()
{
  uint8_t  sensor, range;
  uint32_t curr_time = LocalTime ();
  uint32_t last_time = 0;
  struct hlkld_range *psensor = nullptr;

  // loop thru sensors to get last range received
  for (sensor = 0; sensor < HLKLD_DATA_ANY; sensor ++)
    for (range = 0; range < HLKLD_RANGE_MAX; range ++)

      // if sensor has already got detection and is the newest
      if ((hlkld_device.sensor[sensor][range].time != UINT32_MAX) && (hlkld_device.sensor[sensor][range].time > last_time))
        {
          // save last detection time
          last_time = hlkld_device.sensor[sensor][range].time;

          // if timeout has not been reached, set sensor as the latest
          if (curr_time - last_time < hlkld_device.timeout) psensor = &hlkld_device.sensor[sensor][range];
        }

  return psensor;
}

bool HLKLDSetDeviceConfig (const char* pstr_key, const char* pstr_value)
{
  struct hlkld_range *prange = nullptr;


  // search for sensor range according to the key and update if found
  prange = HLKLDGetRangeFromKey (pstr_key);
  if (prange != nullptr)
  {
    prange->power_ref = atol (pstr_value);
    prange->power_min = LONG_MAX;
  }

  // else if HLK_1115H
  else if (hlkld_device.model == HLKLD_MODEL_1115H)
  {
    // if key deals with timeout, update it
    if (strstr (pstr_key, "dtime") != nullptr) 
    {
      hlkld_device.timeout = (uint16_t)atoi (pstr_value);
      if (hlkld_device.timeout >= 1000) hlkld_device.timeout /= 1000;
    }
  }

  return (prange != nullptr);
}

void HLKLDSetPowerAndDistance (const uint8_t sensor, const uint8_t range, long power, const float distance)
{
  // check parameters
  if (sensor >= HLKLD_DATA_ANY) return;
  if (range  >= HLKLD_RANGE_MAX) return;

  // if power not provided, set it equal to reference power
  if (power == LONG_MAX) power = hlkld_device.sensor[sensor][range].power_ref;

  // set distance
  hlkld_device.sensor[sensor][range].dist_last = distance;

  // set power
  hlkld_device.sensor[sensor][range].power_last = power;

  // update minimum power
  if (hlkld_device.sensor[sensor][range].power_min > power) hlkld_device.sensor[sensor][range].power_min = power;

  // set timestamp
  hlkld_device.sensor[sensor][range].time = LocalTime ();

  // log update data
  AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: sensor %u, range %u, power %d"), sensor, range, power);
} 

void HLKLDAddCommand (const char* pstr_command, const bool flush)
{
  // check parameter
  if (pstr_command == nullptr) return;

  // if needed, flushed commands queue
  if (flush) hlkld_status.str_command = "";

  // append command to the pipe
  if (hlkld_status.str_command.length () > 0) hlkld_status.str_command += ";";
  hlkld_status.str_command += pstr_command;

  // if needed, add get_all
  if (!flush && (strstr (pstr_command, "get_all") == nullptr)) hlkld_status.str_command += ";get_all";
}

bool HLKLDGetCommand (char* pstr_command, size_t size_command)
{
  bool result;
  int  index;

  // check if at least one command in the pipe
  result = (hlkld_status.str_command.length () > 0);
  if (result)
  {
    // get first command in the list (separated by ;)
    index = hlkld_status.str_command.indexOf (';');

    // if ; detected, get command before ; and remove it from the list
    if (index != -1)
    {
      strlcpy (pstr_command, hlkld_status.str_command.substring (0, index).c_str (), size_command);
      hlkld_status.str_command = hlkld_status.str_command.substring (index + 1);
    }

    // else get command and empty list
    else
    {
      strlcpy (pstr_command, hlkld_status.str_command.c_str (), size_command);
      hlkld_status.str_command = "";
    }

    // if command is get_all, reset configuration and signature
    hlkld_status.get_all = (strstr (pstr_command, "get_all") != nullptr);
    if (hlkld_status.get_all) hlkld_status.str_config = "";
    if (hlkld_status.get_all && (hlkld_device.model == HLKLD_MODEL_MAX)) hlkld_status.str_signature = "";
  }

  return result;
}

uint32_t HLKLDGetDelay (const uint8_t type)
{
  uint8_t  index;
  uint32_t time  = 0;
  uint32_t delay = UINT32_MAX;

  // check latest reading time
  if ((type == HLKLD_DATA_OCC) || (type == HLKLD_DATA_ANY)) 
    for (index = 0; index < HLKLD_RANGE_MAX; index ++) 
      time = max (time, hlkld_device.sensor[HLKLD_DATA_OCC][index].time);
      
  if ((type == HLKLD_DATA_MOV) || (type == HLKLD_DATA_ANY))
    for (index = 0; index < HLKLD_RANGE_MAX; index ++) 
      time = max (time, hlkld_device.sensor[HLKLD_DATA_MOV][index].time);

  // if time is valid
  if (time > 0) delay = LocalTime () - time;
  
  return delay;
}

void HLKLDGetDelayText (const uint8_t type, char* pstr_result, size_t size_result)
{
  uint32_t delay = UINT32_MAX;

  // check parameters
  if (pstr_result == nullptr) return;
  if (size_result < 12) return;

  // get delay
  delay = HLKLDGetDelay (type);

  // set unit according to value
  if (delay == UINT32_MAX) strcpy (pstr_result, "---");
  else if (delay >= 172800) sprintf (pstr_result, "%u days", delay / 86400);
  else if (delay >= 86400) sprintf (pstr_result, "1 day");
  else if (delay >= 3600) sprintf (pstr_result, "%u hr", delay / 3600);
  else if (delay >= 60) sprintf (pstr_result, "%u mn", delay / 60);
  else if (delay > 0) sprintf (pstr_result, "%u sec", delay);
  else strcpy (pstr_result, "now");
  
  return;
}

bool HLKLDGetDetectionStatus (uint8_t type, uint32_t timeout)
{
  uint32_t delay;

  // check parameters
  if (type >= HLKLD_DATA_MAX) return false;

  // if no timeout given, use default one
  if (timeout == UINT32_MAX) timeout = hlkld_device.timeout;

  // calculate delays
  delay = HLKLDGetDelay (type);

  if (delay == UINT32_MAX) return false;
  else return (delay <= timeout);
}

/*********************************************\
 *                   Callback
\*********************************************/

// driver initialisation
void HLKLDInit ()
{
  uint8_t sensor, range;

  // init device range arrays
  for (sensor = 0; sensor < HLKLD_SENSOR_MAX; sensor++)
    for (range = 0; range < HLKLD_RANGE_MAX; range++)
    {
      hlkld_device.sensor[sensor][range].type       = HLKLD_PARAM_MAX;
      hlkld_device.sensor[sensor][range].time       = 0;   
      hlkld_device.sensor[sensor][range].power_def  = LONG_MAX;
      hlkld_device.sensor[sensor][range].power_ref  = LONG_MAX;
      hlkld_device.sensor[sensor][range].power_last = LONG_MAX;
      hlkld_device.sensor[sensor][range].power_min  = LONG_MAX;
      hlkld_device.sensor[sensor][range].dist_max   = NAN;
      hlkld_device.sensor[sensor][range].dist_last  = NAN;
    }

  // first command will ask configuration
  strcpy (hlkld_status.str_buffer, "");
  HLKLDAddCommand (" ;get_all", true);

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ld_help to get help on HLK-LD11xx commands"), D_HLKLD_NAME);
}

// loop every second
void HLKLDEverySecond ()
{
  int  index = -1;
  char str_command[32];

  // check sensor presence
  if (hlkld_status.pserial == nullptr) return;

  // if device is initialised and a command is in the pipe
  if ((TasmotaGlobal.uptime > 5) && HLKLDGetCommand (str_command, sizeof (str_command)))
  {
    // log command
    AddLog (LOG_LEVEL_INFO, PSTR ("HLK: Sending command %s"), str_command);

    // add CR + LF and send command
    strlcat (str_command, "\r\n", sizeof (str_command));
    hlkld_status.pserial->write (str_command, strlen (str_command));
  }

  // if needed, update virtual switch status
  if (hlkld_device.index < MAX_SWITCHES) Switch.virtual_state[hlkld_device.index] = HLKLDGetDetectionStatus (HLKLD_DATA_ANY, UINT32_MAX);
}

// Handling of received data
void HLKLDReceiveData ()
{
  uint8_t  type;
  int      index;
  long     power;
  float    distance;
  char     *pstr_tag;
  char     str_recv[2];
  char     str_value[8];
  char     str_key[128];

  // check sensor presence
  if (hlkld_status.pserial == nullptr) return;

  // init buffer and set receive loop timeout to 40ms
  strcpy (str_recv, " ");

  // run serial receive loop
  while (hlkld_status.pserial->available ()) 
  {
    // handle received character
    str_recv[0] = (char)hlkld_status.pserial->read (); 
    switch (str_recv[0])
    {
      // CR is ignored
      case 0x0D:
        break;
          
      // LF needs line analysis
      case 0x0A:
        //log received data
        AddLog (LOG_LEVEL_DEBUG, PSTR ("HLK: %s"), hlkld_status.str_buffer);

        // determine line type
        type  = HLKLD_LINE_MAX;
        if (strstr (hlkld_status.str_buffer, "occ,") == hlkld_status.str_buffer) type = HLKLD_LINE_OCC;
        else if (strstr (hlkld_status.str_buffer, "mov,") == hlkld_status.str_buffer) type = HLKLD_LINE_MOV;
        else if (strstr (hlkld_status.str_buffer, " is ") != nullptr) type = HLKLD_LINE_CONFIG;

        // handle according to line type
        switch (type)
        {
          // handle parameter line
          case HLKLD_LINE_CONFIG:
            pstr_tag = strstr (hlkld_status.str_buffer, " ");
            if (pstr_tag != nullptr)
            {
              // extract key and value
              *pstr_tag = 0;
              strlcpy (str_key,   hlkld_status.str_buffer, sizeof (str_key));
              strlcpy (str_value, pstr_tag + 4, sizeof (str_value));
            
              // if we are dealing with get_all result
              if (hlkld_status.get_all)
              {
                // add to parameters
                if (hlkld_status.str_config.length () > 0) hlkld_status.str_config += ",";
                hlkld_status.str_config += str_key;
                hlkld_status.str_config += "=";
                hlkld_status.str_config += str_value;

                // if model unknown, check signature
                if (hlkld_device.model == HLKLD_MODEL_MAX)
                {
                  // append key to signature
                  if (hlkld_status.str_signature.length () > 0) hlkld_status.str_signature += " ";
                  hlkld_status.str_signature += str_key;

                  // check current signature
                  index = GetCommandCode (str_key, sizeof (str_key), hlkld_status.str_signature.c_str (), HLKLDModelSignature);
                  if (index != -1) HLKLDSetSensorType ((uint8_t) index);
                }
              }

              // else update power trigger values
              else HLKLDSetDeviceConfig (str_key, str_value);
            }
          break;

          // handle presence (occ) or movement (mov) line
          case HLKLD_LINE_OCC:
          case HLKLD_LINE_MOV:
            // init
            distance = NAN;
            power    = LONG_MAX;

            // if dis= available, get distance
            pstr_tag = strstr (hlkld_status.str_buffer, "dis=");
            if (pstr_tag != nullptr) distance = atof (pstr_tag + 4);

            // if str= available, get power
            pstr_tag = strstr (hlkld_status.str_buffer, "str=");
            if (pstr_tag != nullptr) power = atol (pstr_tag + 4);

            // if distance and power not available, get last value with space separation
            pstr_tag = nullptr;
            if (isnan (distance) && (power == LONG_MAX)) pstr_tag = strrchr (hlkld_status.str_buffer, ' ');
            if (pstr_tag != nullptr) power = atol (pstr_tag + 1);

            // set current value
            HLKLDSetPowerFromDistance (type, distance, power);
            break;
        }

        // empty reception buffer
        strcpy (hlkld_status.str_buffer, "");
        break;

      // default : add current caracter to buffer
      default:
        strlcat (hlkld_status.str_buffer, str_recv, sizeof (hlkld_status.str_buffer));
        break;
    }
  }
}

// Show JSON status (for MQTT)
void HLKLDShowJSON (bool append)
{
  uint32_t delay;
  char     str_text[32];
  String   str_config;

  // check sensor presence
  if (hlkld_status.pserial == nullptr) return;

  // --------------------
  //   HLK-LD11 section
  //
  //   normal : "HLK-LD":{"timeout"=5,"occ":0,"mov":1,"any":1,"last"="2 s"}
  //   append : "HLK-LD":{"timeout"=5,"occ":0,"mov":1,"any":1,"last"="2 s","Model":"HLK-LD1125H","Config":{"test_mode":0,"rmax":6.00,"mth1_mov":80,"mth2_mov":50, ... }}
  // --------------------

  // add , in append mode or { in direct publish mode
  if (append) ResponseAppend_P (PSTR (",")); else Response_P (PSTR ("{"));

  // start of sensor section
  ResponseAppend_P (PSTR ("\"%s\":{"), D_HLKLD_NAME);

  // timeout
  ResponseAppend_P (PSTR ("\"timeout\":%u"), hlkld_device.timeout);

  // presence (occ)
  ResponseAppend_P (PSTR (",\"occ\":%d"), HLKLDGetDetectionStatus (HLKLD_DATA_OCC, UINT32_MAX));

  // movement (mov)
  ResponseAppend_P (PSTR (",\"mov\":%d"), HLKLDGetDetectionStatus (HLKLD_DATA_MOV, UINT32_MAX));

  // presence or movement (any)
  ResponseAppend_P (PSTR (",\"any\":%d"), HLKLDGetDetectionStatus (HLKLD_DATA_ANY, UINT32_MAX));

  // last detection in human reading format
  HLKLDGetDelayText (HLKLD_DATA_ANY, str_text, sizeof (str_text));
  ResponseAppend_P (PSTR (",\"last\":\"%s\""), str_text);

  // in append mode, add model and all config parameters
  if (append)
  {
    // model
    GetTextIndexed (str_text, sizeof (str_text), hlkld_device.model, HLKLDModelName);
    ResponseAppend_P (PSTR (",\"Model\":\"%s\""), str_text);

    // convert config as JSON
    if (hlkld_status.str_config.length () > 0)
    {
      // trasnform config as JSON string
      str_config = hlkld_status.str_config;
      str_config.replace (",", ",\"");
      str_config.replace ("=", "\":");

      // start of sensor section
      ResponseAppend_P (PSTR (",\"Config\":{"));
      ResponseAppend_P (PSTR ("\"%s"), str_config.c_str ());
      ResponseAppend_P (PSTR ("}"));
    }
  }

  // end of section
  ResponseAppend_P (PSTR ("}"));

  // publish it if not in append mode
  if (!append)
  {
    ResponseAppend_P (PSTR ("}"));
    MqttPublishTeleSensor ();
  } 
}

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

// Append HLK-LD11xx sensor data to main page
void HLKLDWebSensor ()
{
  uint8_t info_type;
  char    str_icon[8];
  char    str_text[32];

  // check sensor presence
  if (hlkld_status.pserial == nullptr) return;

  // model name
  GetTextIndexed (str_text, sizeof (str_text), hlkld_device.model, HLKLDModelName);
  WSContentSend_P (PSTR ("{s}%s Detector{m}"), str_text);

  // if full display, display occ
  if (hlkld_status.display_all)
  {
    GetTextIndexed (str_icon, sizeof (str_icon), HLKLD_DATA_OCC, HLKLDDataIcon);
    HLKLDGetDelayText (HLKLD_DATA_OCC, str_text, sizeof (str_text));
    WSContentSend_P (PSTR ("%s %s<br>"), str_icon, str_text);
  }

  // display mov or global status
  if (hlkld_status.display_all) info_type = HLKLD_DATA_MOV;
    else info_type = HLKLD_DATA_ANY;
  GetTextIndexed (str_icon, sizeof (str_icon), info_type, HLKLDDataIcon);
  HLKLDGetDelayText (info_type, str_text, sizeof (str_text));
  WSContentSend_P (PSTR ("%s %s{e}"), str_icon, str_text);
}

#ifdef USE_HLKLD11_WEB_CONFIG

void HLKLDWebConfigButton ()
{
  char str_model[32];

  // if model is detected, display configuration button
  if (hlkld_device.model != HLKLD_MODEL_MAX)
  {
    GetTextIndexed (str_model, sizeof (str_model), hlkld_device.model, HLKLDModelName);
    WSContentSend_P (PSTR ("<p><form action='/%s' method='get'><button>Configure %s</button></form></p>"), D_HLKLD_PAGE_CONFIG, str_model);
  }
}

void HLKLDWebPageConfigure ()
{
  uint8_t  sensor = UINT8_MAX;
  uint8_t  range  = UINT8_MAX;
  long     value  = LONG_MAX;
  float    dist_min = 0;
  char     str_min[8];
  char     str_max[8];
  char     str_param[16];
  char     str_command[24];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess ()) return;

  // parameter 'sensor'
  WebGetArg ("sensor", str_param, sizeof (str_param));
  if (strlen (str_param) > 0) sensor = (uint8_t)atoi (str_param);

  // parameter 'range'
  WebGetArg ("range", str_param, sizeof (str_param));
  if (strlen (str_param) > 0) range = (uint8_t)atoi (str_param);

  // parameter 'value'
  WebGetArg ("value", str_param, sizeof (str_param));
  if (strlen (str_param) > 0) value = atol (str_param);

  // if sensor type and range are defined, save value
  if ((sensor < HLKLD_DATA_ANY) && (range < HLKLD_RANGE_MAX) && (value != LONG_MAX))
  {
    // if value = 0 set reference as minimum power, else set value
    if (value == 0) hlkld_device.sensor[sensor][range].power_ref = hlkld_device.sensor[sensor][range].power_min;
    else hlkld_device.sensor[sensor][range].power_ref = value;

    // reset minimum power
    hlkld_device.sensor[sensor][range].power_min = LONG_MAX;

    // add command to the queue
    GetTextIndexed (str_param, sizeof (str_param), hlkld_device.sensor[sensor][range].type, HLKLDParamLabel);
    sprintf (str_command, "%s=%d", str_param, hlkld_device.sensor[sensor][range].power_ref);
    HLKLDAddCommand (str_command, true);
  }

  // else first display
  else
  {
    // loop to reset all minimum values
    for (sensor = 0; sensor < HLKLD_DATA_ANY; sensor ++)
      for (range = 0; range < HLKLD_RANGE_MAX; range ++)
        hlkld_device.sensor[sensor][range].power_min = LONG_MAX;
  }

  // beginning of form without authentification
  WSContentStart_P (D_HLKLD_CONFIGURE, false);
  WSContentSend_P (PSTR ("</script>\n"));

  // page data refresh script
  WSContentSend_P (PSTR ("<script type='text/javascript'>\n"));
  WSContentSend_P (PSTR ("function update()\n"));
  WSContentSend_P (PSTR ("{\n"));
  WSContentSend_P (PSTR (" httpUpd=new XMLHttpRequest();\n"));
  WSContentSend_P (PSTR (" httpUpd.onreadystatechange=function()\n"));
  WSContentSend_P (PSTR (" {\n"));
  WSContentSend_P (PSTR ("  if (httpUpd.responseText.length>0)\n"));
  WSContentSend_P (PSTR ("  {\n"));
  WSContentSend_P (PSTR ("   arr_update=httpUpd.responseText.split('\\n');\n"));
  WSContentSend_P (PSTR ("   num_update=arr_update.length;\n"));
  WSContentSend_P (PSTR ("   for (i=0;i<num_update;i++)\n"));
  WSContentSend_P (PSTR ("   {\n"));
  WSContentSend_P (PSTR ("    arr_param=arr_update[i].split(';');\n"));
  WSContentSend_P (PSTR ("    if (arr_param.length>1)\n"));
  WSContentSend_P (PSTR ("    {\n"));
  WSContentSend_P (PSTR ("     if (arr_param[1].length>0) document.getElementById(arr_param[0]).textContent=arr_param[1];\n"));
  WSContentSend_P (PSTR ("     if (arr_param[2].length>0) document.getElementById(arr_param[0]).className=arr_param[2];\n"));
  WSContentSend_P (PSTR ("     if (arr_param[3].length>0) document.getElementById(arr_param[0]).style.visibility=arr_param[3];\n"));
  WSContentSend_P (PSTR ("     if (arr_param[4].length>0) document.getElementById(arr_param[0]).style.top=arr_param[4];\n"));
  WSContentSend_P (PSTR ("    }\n"));
  WSContentSend_P (PSTR ("   }\n"));
  WSContentSend_P (PSTR ("  }\n"));
  WSContentSend_P (PSTR (" }\n"));
  WSContentSend_P (PSTR (" httpUpd.open('GET','%s',true);\n"), D_HLKLD_PAGE_CONFIG_UPD);
  WSContentSend_P (PSTR (" httpUpd.send();\n"));
  WSContentSend_P (PSTR ("}\n"));
  WSContentSend_P (PSTR ("setInterval(function() {update();},1000);\n"));
  WSContentSend_P (PSTR ("</script>\n"));

  // page style
  WSContentSend_P (PSTR ("<style>\n"));

  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div a {color:white;}\n"));

  WSContentSend_P (PSTR ("div.title {font-size:2rem;font-weight:bold;margin-bottom:30px;text-align:center;}\n"));

  WSContentSend_P (PSTR ("div.grid {width:90%%;max-width:800px;margin-bottom:20px;margin:0.25rem auto;padding:0.1rem 0px;text-align:center;}\n"));
  WSContentSend_P (PSTR ("div.pad {width:46%%;min-width:320px;display:inline-block;font-size:40px;padding:0px;margin:20px 1%%;background:transparent;}\n"));
  WSContentSend_P (PSTR ("div.cell {position:relative;padding:0px 4px;margin:0px;border:none;border-radius:12px;}\n"));

  WSContentSend_P (PSTR ("div.occ div.cell {background:#131;border:1px solid #252;}\n"));
  WSContentSend_P (PSTR ("div.mov div.cell {background:#225;border:1px solid #338;}\n"));
  WSContentSend_P (PSTR ("div.occ div span {background:#252;}\n"));
  WSContentSend_P (PSTR ("div.mov div span {background:#338;}\n"));

  WSContentSend_P (PSTR ("div.line {content:'';position:absolute;top:0%%;width:70%%;left:15%%;border-top:1px dashed yellow;visibility:hidden;}\n"));

  WSContentSend_P (PSTR ("div span {margin:2px 2px;padding:1px 5px;float:none;border:1px solid transparent;border-radius:4px;}\n"));
  WSContentSend_P (PSTR ("div span.left {float:left;}\n"));
  WSContentSend_P (PSTR ("div span.right {float:right;}\n"));

  WSContentSend_P (PSTR ("div.top {font-size:20px;padding:0px 2px;margin:2px 0px 15px 0px;}\n"));
  WSContentSend_P (PSTR ("div.top span {font-size:14px;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.top span.left {color:red;}\n"));

  WSContentSend_P (PSTR ("div.old {height:48px;color:grey;font-size:32px;}\n"));
  WSContentSend_P (PSTR ("div.last {height:48px;color:yellow;font-size:40px;font-weight:bold;}\n"));

  WSContentSend_P (PSTR ("div.down {margin:15px 0px 4px 0px;font-size:18px;}\n"));
  WSContentSend_P (PSTR ("div.down span {font-size:12px;}\n"));

  WSContentSend_P (PSTR ("</style>\n"));

  // page body
  WSContentSend_P (PSTR ("</head>\n"));
  WSContentSend_P (PSTR ("<body>\n"));

  // device name
  WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText(SET_DEVICENAME));
  WSContentSend_P (PSTR ("<div class='grid'>\n"));

  // loop thru sensors
  for (sensor = 0; sensor < HLKLD_DATA_ANY; sensor ++)
  {
    // get sensor label and icon
    GetTextIndexed (str_param,   sizeof (str_param),   sensor, HLKLDDataIcon);
    GetTextIndexed (str_command, sizeof (str_command), sensor, HLKLDDataLabel);
    WSContentSend_P (PSTR ("<div class='pad %s'>%s\n"), str_command, str_param);

    // loop thru sensors
    dist_min = 0;
    for (range = 0; range < HLKLD_RANGE_MAX; range ++)
    {
      if (!isnan (hlkld_device.sensor[sensor][range].dist_max) && (hlkld_device.sensor[sensor][range].power_ref != LONG_MAX))
      {
        WSContentSend_P (PSTR ("<div class='cell'>\n"));

        // distance line (hidden by default)
        WSContentSend_P (PSTR ("<div class='line' id='s%u-r%u-line'></div>\n"), sensor, range);

        // distance
        ext_snprintf_P (str_min, sizeof(str_min), PSTR ("%1_f"), &dist_min);
        ext_snprintf_P (str_max, sizeof(str_max), PSTR ("%1_f"), &hlkld_device.sensor[sensor][range].dist_max);
        dist_min = hlkld_device.sensor[sensor][range].dist_max;
        WSContentSend_P (PSTR ("<div class='top'>%s - %sm\n"), str_min, str_max);

        // lowest value
        WSContentSend_P (PSTR ("<a href='/%s?sensor=%u&range=%u&value=low'><span title='Lowest' class='left' id='s%u-r%u-low'>---</span></a>\n"), D_HLKLD_PAGE_CONFIG, sensor, range, sensor, range);

        // default value
        value = hlkld_device.sensor[sensor][range].power_def; 
        WSContentSend_P (PSTR ("<a href='/%s?sensor=%u&range=%u&value=%d'><span title='Default' class='right'>%d</span></a>\n"), D_HLKLD_PAGE_CONFIG, sensor, range, value, value);

        WSContentSend_P (PSTR ("</div>\n"));        // top

        // sensor power
        WSContentSend_P (PSTR ("<div class='old' id='s%u-r%u-last'>---</div>\n"), sensor, range);

        // sensor range adjustments
        WSContentSend_P (PSTR ("<div class='down'>%u\n"), hlkld_device.sensor[sensor][range].power_ref);

        // value - 1000
        value = LONG_MAX;
        if (hlkld_device.sensor[sensor][range].power_ref > 1000) value = hlkld_device.sensor[sensor][range].power_ref - 1000; 
        if (value != LONG_MAX) WSContentSend_P (PSTR ("<a href='/%s?sensor=%u&range=%u&value=%d'><span class='left'>-1000</span></a>\n"), D_HLKLD_PAGE_CONFIG, sensor, range, value);
        else WSContentSend_P (PSTR ("<span class='left'>---</span>\n"));

        // value - 100
        value = LONG_MAX;
        if (hlkld_device.sensor[sensor][range].power_ref > 100) value = hlkld_device.sensor[sensor][range].power_ref - 100; 
        if (value != LONG_MAX) WSContentSend_P (PSTR ("<a href='/%s?sensor=%u&range=%u&value=%d'><span class='left'>-100</span></a>\n"), D_HLKLD_PAGE_CONFIG, sensor, range, value);
        else WSContentSend_P (PSTR ("<span class='left'>---</span>\n"));

        // value - 10
        value = LONG_MAX;
        if (hlkld_device.sensor[sensor][range].power_ref > 10) value = hlkld_device.sensor[sensor][range].power_ref - 10; 
        if (value != LONG_MAX) WSContentSend_P (PSTR ("<a href='/%s?sensor=%u&range=%u&value=%d'><span class='left'>-10</span></a>\n"), D_HLKLD_PAGE_CONFIG, sensor, range, value);
        else WSContentSend_P (PSTR ("<span class='left'>---</span>\n"));

        // value + 1000
        value = hlkld_device.sensor[sensor][range].power_ref + 1000;
        WSContentSend_P (PSTR ("<a href='/%s?sensor=%u&range=%u&value=%d'><span class='right'>+1000</span></a>\n"), D_HLKLD_PAGE_CONFIG, sensor, range, value);

        // value + 100
        value = hlkld_device.sensor[sensor][range].power_ref + 100;
        WSContentSend_P (PSTR ("<a href='/%s?sensor=%u&range=%u&value=%d'><span class='right'>+100</span></a>\n"), D_HLKLD_PAGE_CONFIG, sensor, range, value);

        // value + 10
        value = hlkld_device.sensor[sensor][range].power_ref + 10;
        WSContentSend_P (PSTR ("<a href='/%s?sensor=%u&range=%u&value=%d'><span class='right'>+10</span></a>\n"), D_HLKLD_PAGE_CONFIG, sensor, range, value);

        WSContentSend_P (PSTR ("</div>\n"));      // down
        WSContentSend_P (PSTR ("</div>\n"));      // cell
      }
    }
    WSContentSend_P (PSTR ("</div>\n"));          // pad
  }

  // specific LD1125H message
  if (hlkld_device.model == HLKLD_MODEL_1125H) WSContentSend_P (PSTR ("<p><i>To get power level, run following command in console</i> :<br><b>ld_cmnd test_mode=1</b></p>\n"));

  // end of page
  WSContentSend_P (PSTR ("</div>\n"));            // grid
  WSContentStop ();
}

// Send configuration update
//   Each line must be : ObjectID;Content;ClassName;Visibility;TopPositionValue
//   If a value is empty, it won't be updated
void HLKLDWebPageConfigUpdate ()
{
  bool    is_active;
  uint8_t sensor, range;
  float   dist_min, dist_percent;
  struct  hlkld_range *psensor;
  char    str_text[8];

  // get last active sensor
  psensor = HLKLDGetLastActiveSensor ();

  // start of data page
  WSContentBegin (200, CT_PLAIN);

  // loop thru sensors to send latest values
  for (sensor = 0; sensor < HLKLD_DATA_ANY; sensor ++)
    for (range = 0; range < HLKLD_RANGE_MAX; range ++)
    {
      // check if active sensor
      is_active = (psensor == &hlkld_device.sensor[sensor][range]);

      // if last received power defined, send it
      if (hlkld_device.sensor[sensor][range].power_last != LONG_MAX)
      {
        if (is_active) strcpy (str_text, "last"); else strcpy (str_text, "old");
        WSContentSend_P ("s%u-r%u-last;%d;%s;;\n", sensor, range, hlkld_device.sensor[sensor][range].power_last, str_text);
      }

      // minimum received power
      if (hlkld_device.sensor[sensor][range].power_min != LONG_MAX) WSContentSend_P ("s%u-r%u-low;%d;;;\n", sensor, range, hlkld_device.sensor[sensor][range].power_min, str_text);

      // if sensor is active and distance defined, show the line
      if (is_active)
      {
        // calculate minimum distance of current range
        if (range == 0) dist_min = 0;
          else dist_min = hlkld_device.sensor[sensor][range - 1].dist_max;
        
        // if distance not defined, set it to 50% of range, else calculate percentage within the range
        if (isnan (hlkld_device.sensor[sensor][range].dist_last)) dist_percent = 50;
          else dist_percent = 100 * (hlkld_device.sensor[sensor][range].dist_last - dist_min) / (hlkld_device.sensor[sensor][range].dist_max - dist_min);

        // display line
        WSContentSend_P ("s%u-r%u-line;;;visible;%u%%\n", sensor, range, (uint8_t)dist_percent);
      }

      // else hide line
      else WSContentSend_P ("s%u-r%u-line;;;hidden;\n", sensor, range);
    }

  // end of data page
  WSContentEnd ();
}

#endif  // USE_HLKLD11_WEB_CONFIG

#endif  // USE_WEBSERVER

/***************************************\
 *              Interface
\***************************************/

// Teleinfo sensor
bool Xsns124 (uint8_t function)
{
  bool result = false;

  // swtich according to context
  switch (function) 
  {
    case FUNC_INIT:
      HLKLDInit ();
//      HLKLDInitDevice (0, HLKLD_DEFAULT_TIMEOUT);
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kHLKLDCommands, HLKLDCommand);
      break;
    case FUNC_EVERY_SECOND:
      HLKLDEverySecond ();
      break;
    case FUNC_JSON_APPEND:
      HLKLDShowJSON (true);
      break;
    case FUNC_EVERY_100_MSECOND:
      HLKLDReceiveData ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      HLKLDWebSensor ();
      break;

#ifdef USE_HLKLD11_WEB_CONFIG
    case FUNC_WEB_ADD_BUTTON:
      HLKLDWebConfigButton ();
      break;
    case FUNC_WEB_ADD_HANDLER:
      Webserver->on ("/" D_HLKLD_PAGE_CONFIG, HLKLDWebPageConfigure);
      Webserver->on ("/" D_HLKLD_PAGE_CONFIG_UPD, HLKLDWebPageConfigUpdate);
      break;
#endif  // USE_HLKLD11_WEB_CONFIG

#endif  // USE_WEBSERVER
  }

  return result;
}

#endif      // USE_HLKLD11

