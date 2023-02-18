/*
  xsns_121_ld1125.ino - Driver for Presence and Movement sensor HLK-LD1125

  Copyright (C) 2022  Nicolas Bernaerts

  Connexions :
    * GPIO1 (Tx) should be declared as Serial Tx and connected to HLK-LD1125 Rx
    * GPIO3 (Rx) should be declared as Serial Rx and connected to HLK-LD1125 Tx

  Call LD1125InitDevice (timeout) to declare the device and make it operational

  Version history :
    22/06/2022 - v1.0 - Creation
    14/01/2023 - v2.0 - Complete rewrite

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

#ifdef USE_LD1125

#include <TasmotaSerial.h>

/*************************************************\
 *               Variables
\*************************************************/

// declare teleinfo energy driver and sensor
#define XSNS_121                   121

// constant
#define LD1125_START_DELAY          15          // sensor answers after 15 seconds
#define LD1125_DEFAULT_TIMEOUT      5           // inactivity after 5 sec

#define LD1125_COLOR_TEXT_ON        "white"
#define LD1125_COLOR_TEXT_OFF       "grey"
#define LD1125_COLOR_NONE           "#444"
#define LD1125_COLOR_PRESENCE       "#E80"
#define LD1125_COLOR_MOTION         "#D00"

// strings
const char D_LD1125_NAME[]          PROGMEM = "HLK-LD1125H";

// MQTT commands : ld_help and ld_send
const char kLD1125Commands[]         PROGMEM = "ld1125_|help|move_th|pres_th|max|timeout|update|save|reset";
void (* const LD1125Command[])(void) PROGMEM = { &CmndLD1125Help, &CmndLD1125MotionTh, &CmndLD1125PresenceTh, &CmndLD1125Max, &CmndLD1125Timeout, &CmndLD1125Update, &CmndLD1125Save, &CmndLD1125Reset };

/****************************************\
 *                 Data
\****************************************/

// HLK-LD1125 sensor general status
struct ld1125_sensor
{
  bool     detected;
  int      th[3];
  int      power; 
  float    distance; 
  uint32_t timestamp;
};
static struct {
  bool     enabled  = false;
  bool     detected = false;
  int      max_distance = INT_MAX;
  uint32_t timeout    = UINT32_MAX;               // detection timeout
  char     str_buffer[32];                        // buffer of received data
  String   str_cmnd_list;                         // list of pending commands, separated by ;
  ld1125_sensor mov;
  ld1125_sensor occ;
  TasmotaSerial *pserial = nullptr;               // pointer to serial port
} ld1125_status; 

/**************************************************\
 *                  Commands
\**************************************************/

// hlk-ld sensor help
void CmndLD1125Help ()
{
  // help on command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: HLK-LD1125 sensor commands :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ld1125_move_th <th1>,<th2>,<th3>"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ld1125_pres_th <th1>,<th2>,<th3>"));
  AddLog (LOG_LEVEL_INFO, PSTR ("     set 0-2.8m,2.8-8m,+8m sensitivity threshold"));
  AddLog (LOG_LEVEL_INFO, PSTR ("     keep value empty if not changed"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ld1125_max <value>     = set max detection distance"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ld1125_timeout <value> = set detection timeout"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ld1125_update = read all sensor parameters"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ld1125_save   = save parameters to sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ld1125_reset  = reset to default sensor parameters"));
  ResponseCmndDone ();
}

void CmndLD1125Update (void)
{
  LD1125AppendCommand ("test_mode=1;get_all");
  ResponseCmndDone ();
}

void CmndLD1125Save (void)
{
  LD1125AppendCommand ("save");
  ResponseCmndDone ();
}

void CmndLD1125Reset (void)
{
  LD1125AppendCommand ("rmax=6;mth1_mov=60;mth2_mov=30;mth3_mov=20;mth1_occ=60;mth2_occ=30;mth3_occ=20;test_mode=1;get_all");
  ResponseCmndDone ();
}

void CmndLD1125MotionTh (void)
{
  bool updated = false;
  uint8_t index = 1;
  char str_command[16];
  char str_data[32];
  char *pstr_token;
   
  // get data
  strlcpy (str_data, XdrvMailbox.data, sizeof (str_data));
   
  // loop thru tokens
  pstr_token = strtok (str_data, ",");
  while( pstr_token != nullptr)
  {
    // if value is given, add update command
    if (strlen (pstr_token) > 0)
    {
      sprintf (str_command, "mth%u_mov=%s", index, pstr_token);
      LD1125AppendCommand (str_command);
      updated = true;
    }

    // search for next token    
    pstr_token = strtok (nullptr, ",");
    index ++;
  }

  // answer
  if (updated) ResponseCmndDone ();
  else
  {
    sprintf (str_data, "mth1_mov=%d,mth2_mov=%d,mth3_mov=%d", ld1125_status.mov.th[0], ld1125_status.mov.th[1], ld1125_status.mov.th[2]);
    ResponseCmndChar (str_data);
  }
}

void CmndLD1125PresenceTh (void)
{
  bool updated = false;
  uint8_t index = 1;
  char str_command[16];
  char str_data[32];
  char *pstr_token;
   
  // get data
  strlcpy (str_data, XdrvMailbox.data, sizeof (str_data));
   
  // loop thru tokens
  pstr_token = strtok (str_data, ",");
  while( pstr_token != nullptr)
  {
    // if value is given, add update command
    if (strlen (pstr_token) > 0)
    {
      sprintf (str_command, "mth%u_occ=%s", index, pstr_token);
      LD1125AppendCommand (str_command);
      updated = true;
    }

    // search for next token    
    pstr_token = strtok (nullptr, ",");
    index ++;
  }

  // answer
  if (updated) ResponseCmndDone ();
  else
  {
    sprintf (str_data, "mth1_occ=%d,mth2_occ=%d,mth3_occ=%d", ld1125_status.occ.th[0], ld1125_status.occ.th[1], ld1125_status.occ.th[2]);
    ResponseCmndChar (str_data);
  }
}

void CmndLD1125Max (void)
{
  char str_command[16];

  // if data is valid, add command
  if (XdrvMailbox.payload > 0)
  {
    sprintf (str_command, "rmax=%d", XdrvMailbox.payload);
    LD1125AppendCommand (str_command);
    ResponseCmndDone ();
  }

  // else return value
  else ResponseCmndNumber (ld1125_status.max_distance);
}

void CmndLD1125Timeout (void)
{
  // if data is valid, update timeout
  if (XdrvMailbox.payload > 0)
  {
    ld1125_status.timeout = XdrvMailbox.payload;
    ResponseCmndDone ();
  }

  // else return value
  else ResponseCmndNumber (ld1125_status.timeout);
}

/**************************************************\
 *                  Functions
\**************************************************/

void LD1125AppendCommand (const char* pstr_command)
{
  // check parameter
  if (pstr_command == nullptr) return;
  if (strlen (pstr_command) == 0) return;

  // add command
  if (ld1125_status.str_cmnd_list.length () > 0) ld1125_status.str_cmnd_list += ";";
  ld1125_status.str_cmnd_list += pstr_command;
}

void LD1125ExecuteNextCommand ()
{
  int    index;
  String str_command;

  // if at least one command is pending
  if (ld1125_status.str_cmnd_list.length () > 0)
  {
    // extract first command
    index = ld1125_status.str_cmnd_list.indexOf (';');

    // if some commands are remaining
    if (index != -1)
    {
      str_command = ld1125_status.str_cmnd_list.substring (0, index);
      ld1125_status.str_cmnd_list = ld1125_status.str_cmnd_list.substring (index + 1);
    }

    // else, last command in the queue
    else
    {
      str_command = ld1125_status.str_cmnd_list;
      ld1125_status.str_cmnd_list = "";
    }

    // log command
    AddLog (LOG_LEVEL_INFO, PSTR ("HLK: cmnd %s"), str_command.c_str ());

    // send command
    str_command += "\r\n";
    ld1125_status.pserial->write (str_command.c_str (), str_command.length ());
  }
}

// driver initialisation
bool LD1125InitDevice (uint32_t timeout)
{
  // set timeout and switch index
  if ((timeout == 0) || (timeout == UINT32_MAX)) ld1125_status.timeout = LD1125_DEFAULT_TIMEOUT;
    else ld1125_status.timeout = timeout;

  // if ports are selected, init sensor state
  if ((ld1125_status.pserial == nullptr) && PinUsed (GPIO_TXD) && PinUsed (GPIO_RXD))
  {
#ifdef ESP32
    // create serial port
    ld1125_status.pserial = new TasmotaSerial (Pin (GPIO_RXD), Pin (GPIO_TXD), 1);

    // initialise serial port
    ld1125_status.enabled = ld1125_status.pserial->begin (115200, SERIAL_8N1);

#else       // ESP8266
    // create serial port
    ld1125_status.pserial = new TasmotaSerial (Pin (GPIO_RXD), Pin (GPIO_TXD), 1);

    // initialise serial port
    ld1125_status.enabled = ld1125_status.pserial->begin (115200, SERIAL_8N1);

    // force hardware configuration on ESP8266
    if (ld1125_status.pserial->hardwareSerial ()) ClaimSerial ();
#endif      // ESP32 & ESP8266

    // flush data
    if (ld1125_status.enabled) ld1125_status.pserial->flush ();
 
    // first command will ask configuration
    ld1125_status.str_cmnd_list = "test_mode=1;get_all";

    // log
    if (ld1125_status.enabled) AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s sensor init succesfull"), D_LD1125_NAME);
    else AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s sensor init failed"), D_LD1125_NAME);
 }

  return ld1125_status.enabled;
}

uint32_t LD1125GetDelaySinceLastDetection ()
{
  uint32_t timestamp = 0;
  uint32_t delay     = UINT32_MAX;

  // check last detection time
  timestamp = max (timestamp, ld1125_status.mov.timestamp);
  timestamp = max (timestamp, ld1125_status.occ.timestamp);

  // calculate delay
  if (timestamp != 0) delay = LocalTime () - timestamp;

  return delay;
}

void LD1125GetDelayText (char* pstr_result, size_t size_result)
{
  uint32_t delay;

  // check parameters
  if (pstr_result == nullptr) return;
  if (size_result < 12) return;

  // get delay
  delay = LD1125GetDelaySinceLastDetection ();

  // set unit according to value
  if (delay == UINT32_MAX) strlcpy (pstr_result, "---", size_result);
  else if (delay >= 172800) sprintf (pstr_result, "%u days", delay / 86400);
  else if (delay >= 86400) sprintf (pstr_result, "1 day");
  else if (delay >= 3600) sprintf (pstr_result, "%u hr", delay / 3600);
  else if (delay >= 60) sprintf (pstr_result, "%u mn", delay / 60);
  else if (delay > 0) sprintf (pstr_result, "%u sec", delay);
  else strlcpy (pstr_result, "now", size_result);
  
  return;
}

void LD1125GetPowerText (const int power, char* pstr_result)
{
  if (power == INT_MAX) strcpy (pstr_result, "&nbsp;");
  else if (power < 1000) itoa (power, pstr_result, 10);
  else if (power < 10000) sprintf (pstr_result, "%d.%dk", power / 1000, (power % 1000) / 100);
  else sprintf (pstr_result, "%dk", power / 1000);
}

bool LD1125GetMotionDetectionStatus (uint32_t timeout)
{
  // if power or timestamp not defined, no detection
  if (ld1125_status.mov.power == 0) return false;
  if (ld1125_status.mov.timestamp == 0) return false;
  
  // if no timeout given, use default one
  if ((timeout == 0) || (timeout == UINT32_MAX)) timeout = ld1125_status.timeout;

  // return timeout status
  return (ld1125_status.mov.timestamp + timeout >= LocalTime ());
}

bool LD1125GetPresenceDetectionStatus (uint32_t timeout)
{
  // if power or timestamp not defined, no detection
  if (ld1125_status.occ.power == 0) return false;
  if (ld1125_status.occ.timestamp == 0) return false;
  
  // if no timeout given, use default one
  if ((timeout == 0) || (timeout == UINT32_MAX)) timeout = ld1125_status.timeout;

  // return timeout status
  return (ld1125_status.occ.timestamp + timeout >= LocalTime ());
}

bool LD1125GetGlobalDetectionStatus (uint32_t timeout)
{
   return (LD1125GetMotionDetectionStatus (timeout) || LD1125GetPresenceDetectionStatus (timeout));
}

/*********************************************\
 *                   Callback
\*********************************************/

// driver initialisation
void LD1125Init ()
{
  uint8_t index;

  // init reception buffer
  strcpy (ld1125_status.str_buffer, "");

  // init sensors
  ld1125_status.mov.power     = 0; 
  ld1125_status.mov.distance  = NAN; 
  ld1125_status.mov.timestamp = 0;
  ld1125_status.occ.power     = 0; 
  ld1125_status.occ.distance  = NAN; 
  ld1125_status.occ.timestamp = 0;
  for (index = 0; index < 3; index ++) ld1125_status.occ.th[index] = 0;
  for (index = 0; index < 3; index ++) ld1125_status.mov.th[index] = 0;

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ld1125_help to get help on %s commands"), D_LD1125_NAME);
}

// loop every 250 msecond
void LD1125Every250ms ()
{
  // execute any command in the pipe
  LD1125ExecuteNextCommand ();
}

// loop every second
void LD1125EverySecond ()
{
  bool     detected;
  uint32_t time_now = LocalTime ();

  // if presence timeout is reached, reset presence detection
  ld1125_status.mov.detected = (time_now <= ld1125_status.mov.timestamp + ld1125_status.timeout);
  ld1125_status.occ.detected = (time_now <= ld1125_status.occ.timestamp + ld1125_status.timeout);

  // check for MQTT update
  detected = (ld1125_status.mov.detected || ld1125_status.occ.detected);
  if (ld1125_status.detected != detected)
  {
    ld1125_status.detected = detected;
    LD1125ShowJSON (false);
  }
}

// Handling of received data
void LD1125ReceiveData ()
{
  bool     detected;
  int      value;
  uint32_t time_now;
  char     *pstr_key;
  char     *pstr_value;
  char     *pstr_power;
  char     str_recv[2];
  char     str_key[16];

  // check sensor presence
  if (ld1125_status.pserial == nullptr) return;

  // init
  strcpy (str_key, "");
  strcpy (str_recv, " ");
  time_now = LocalTime ();

  // run serial receive loop
  while (ld1125_status.pserial->available ()) 
  {
    // handle received character
    str_recv[0] = (char)ld1125_status.pserial->read (); 
    switch (str_recv[0])
    {
      // CR is ignored
      case 0x0D:
        break;
          
      // LF needs line analysis
      case 0x0A:
        // log received data
        AddLog (LOG_LEVEL_DEBUG_MORE, PSTR ("HLK: %s"), ld1125_status.str_buffer);

        // if received static level
        if (strstr (ld1125_status.str_buffer, "occ,") == ld1125_status.str_buffer)
        {
          // check distance
          pstr_value = strstr (ld1125_status.str_buffer, "dis=");
          if (pstr_value != nullptr)
          {
            ld1125_status.occ.distance  = atof (pstr_value + 4);
            ld1125_status.occ.detected  = true;
            ld1125_status.occ.timestamp = time_now;
          }

          // check power
          pstr_power = strstr (ld1125_status.str_buffer, "str=");
          if (pstr_power != nullptr) ld1125_status.occ.power = atoi (pstr_power + 4);
            else ld1125_status.occ.power = INT_MAX;
        }

        // if received motion level
        else if (strstr (ld1125_status.str_buffer, "mov,") == ld1125_status.str_buffer)
        {
          // check distance
          pstr_value = strstr (ld1125_status.str_buffer, "dis=");
          if (pstr_value != nullptr)
          {
            ld1125_status.mov.distance  = atof (pstr_value + 4);
            ld1125_status.mov.detected  = true;
            ld1125_status.mov.timestamp = time_now;
          }

          // check power
          pstr_power = strstr (ld1125_status.str_buffer, "str=");
          if (pstr_power != nullptr) ld1125_status.mov.power = atoi (pstr_power + 4); 
            else ld1125_status.mov.power = INT_MAX;
         }

        // else if received configuration value
        else if (strstr (ld1125_status.str_buffer, " is ") != nullptr)
        {
          // init
          value = INT_MAX;
          strcpy (str_key, "");
          pstr_value = nullptr;

          // look for key and value
          pstr_key = strchr (ld1125_status.str_buffer, ' ');
          if (pstr_key != nullptr) pstr_value = strchr (pstr_key + 1, ' ');

          // extract key
          if (pstr_key != nullptr)
          {
            *pstr_key = 0;
            strlcpy (str_key, ld1125_status.str_buffer, sizeof (str_key));
          }

          // extract value
          if (pstr_value != nullptr) value = atoi (pstr_value + 1);

          // update value according to key
          if ((strlen (str_key) > 0) && (value != INT_MAX))
          {
            // assign value
            if (strstr (str_key, "rmax") != nullptr) ld1125_status.max_distance = value;
            else if (strstr (str_key, "mth1_mov") != nullptr) ld1125_status.mov.th[0] = value;
            else if (strstr (str_key, "mth2_mov") != nullptr) ld1125_status.mov.th[1] = value;
            else if (strstr (str_key, "mth3_mov") != nullptr) ld1125_status.mov.th[2] = value;
            else if (strstr (str_key, "mth1_occ") != nullptr) ld1125_status.occ.th[0] = value;
            else if (strstr (str_key, "mth2_occ") != nullptr) ld1125_status.occ.th[1] = value;
            else if (strstr (str_key, "mth3_occ") != nullptr) ld1125_status.occ.th[2] = value;

            // log
            AddLog (LOG_LEVEL_INFO, PSTR ("HLK: recv %s=%d"), str_key, value);
          }
        }

        // empty reception buffer
        strcpy (ld1125_status.str_buffer, "");
        break;

      // default : add current caracter to buffer
      default:
        strlcat (ld1125_status.str_buffer, str_recv, sizeof (ld1125_status.str_buffer));
        break;
    }

    // give control back to the system to avoid watchdog
    yield ();
  }

  // check for MQTT update
  detected = (ld1125_status.mov.detected || ld1125_status.occ.detected);
  if (ld1125_status.detected != detected)
  {
    ld1125_status.detected = detected;
    LD1125ShowJSON (false);
  }
}

// Show JSON status (for MQTT)
//   false ->   "HLK-LD1125":{"detect":1}
//   true  ->   "HLK-LD1125":{"detect":1,"timeout":5,"occ":{"odetect":1,"odist":2.78,"opower":176},"mov":{"mdetect":0},"delay":{"dval":4,"dtext"="2 s"}}
void LD1125ShowJSON (bool append)
{
  bool result;
  uint32_t delay;
  char     str_text[32];
  String   str_config;

  // check sensor presence
  if (ld1125_status.pserial == nullptr) return;

  // add , in append mode or { in direct publish mode
  if (append) ResponseAppend_P (PSTR (",")); else Response_P (PSTR ("{"));

  // start of sensor section
  ResponseAppend_P (PSTR ("\"%s\":{\"detect\":%u"), D_LD1125_NAME, ld1125_status.detected);

  if (append) 
  {
    // timeout
    ResponseAppend_P (PSTR (",\"timeout\":%u"), ld1125_status.timeout);

    // presence (occ)
    result = LD1125GetPresenceDetectionStatus (UINT32_MAX);
    ResponseAppend_P (PSTR (",\"occ\":{\"odetect\":%u"), result);
    if (result)
    {
      ext_snprintf_P (str_text, sizeof (str_text), PSTR ("%2_f"), &ld1125_status.occ.distance);
      ResponseAppend_P (PSTR (",\"odist\":%s,\"opower\":%d"), str_text, ld1125_status.occ.power);
    }
    ResponseAppend_P (PSTR ("}"));

    // motion (mov)
    result = LD1125GetMotionDetectionStatus (UINT32_MAX);
    ResponseAppend_P (PSTR (",\"mov\":{\"mdetect\":%u"), result);
    if (result)
    {
      ext_snprintf_P (str_text, sizeof (str_text), PSTR ("%2_f"), &ld1125_status.mov.distance);
      ResponseAppend_P (PSTR (",\"mdist\":%s,\"mpower\":%d"), str_text, ld1125_status.mov.power);
    }
    ResponseAppend_P (PSTR ("}"));

    // delay since last detection
    delay = LD1125GetDelaySinceLastDetection ();
    LD1125GetDelayText (str_text, sizeof (str_text));
    if (delay != UINT32_MAX)   ResponseAppend_P (PSTR (",\"delay\":{\"dval\":%u,\"dtext\":\"%s\"}"), delay, str_text);

    // end of section
    ResponseAppend_P (PSTR ("}"));
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

// Append HLK-LD1125 sensor data to main page
void LD1125WebSensor ()
{
  bool detected;
  int  index, width, range, distance;
  char str_color[8];
  char str_value[32];
  char str_style[64];

  // calculate maximum range
  range = min (9, ld1125_status.max_distance);

  // start display
  WSContentSend_P (PSTR ("<div style='font-size:10px;text-align:center;margin-top:4px;padding:2px 6px;background:#333333;border-radius:8px;'>\n"));

  // scale
  WSContentSend_P (PSTR ("<div style='display:flex;padding:0px;'>\n"));
  WSContentSend_P (PSTR ("<div style='width:28%%;padding:0px;text-align:left;font-size:12px;font-weight:bold;'>LD1125</div>\n"));
  WSContentSend_P (PSTR ("<div style='width:7.2%%;padding:0px;text-align:left;'>0m</div>\n"));
  for (index = 1; index < 5; index ++) WSContentSend_P (PSTR ("<div style='width:14.4%%;padding:0px;'>%dm</div>\n"), index * 2);
  WSContentSend_P (PSTR ("<div style='width:7.2%%;padding:0px;text-align:right;'>+</div>\n"));
  WSContentSend_P (PSTR ("</div>\n"));

  // presence sensor
  detected = LD1125GetPresenceDetectionStatus (0);
  distance = min (8, (int)ld1125_status.occ.distance);
  WSContentSend_P (PSTR ("<div style='display:flex;padding:1px 0px;color:%s;'>\n"), LD1125_COLOR_TEXT_OFF);
  WSContentSend_P (PSTR ("<div style='width:28%%;padding:0px;text-align:left;color:white;'>&nbsp;&nbsp;Presence</div>\n"));
  for (index = 0; index < range; index ++)
  {
    // display start
    if (index == 8) WSContentSend_P (PSTR ("<div style='width:14.4%%;padding:0px;"));
      else WSContentSend_P (PSTR ("<div style='width:7.2%%;padding:0px;"));

    // cell style
    if (detected && (index == distance)) WSContentSend_P (PSTR ("background:%s;color:%s;"), LD1125_COLOR_PRESENCE, LD1125_COLOR_TEXT_ON);
      else WSContentSend_P (PSTR ("background:%s;"), LD1125_COLOR_NONE);

    // cell border
    if ((index == 2) || (index == range - 1)) WSContentSend_P (PSTR ("border-top-right-radius:4px;border-bottom-right-radius:4px;margin-right:1px;"));
    if ((index == 0) || (index == 3)) WSContentSend_P (PSTR ("border-top-left-radius:4px;border-bottom-left-radius:4px;"));
    if (index == 8) WSContentSend_P (PSTR ("border-radius:4px;"));

    // cell value
    LD1125GetPowerText (ld1125_status.occ.power, str_value);
    if (detected && (index == distance)) WSContentSend_P (PSTR ("'>%s</div>\n"), str_value);
      else if ((index == 1) || (range == 1)) WSContentSend_P (PSTR ("'>%d</div>\n"), ld1125_status.occ.th[0]);
      else if ((index == 5) || ((index > 2) && (index < 5) && (index == range - 1))) WSContentSend_P (PSTR ("'>%d</div>\n"), ld1125_status.occ.th[1]);
      else if (index == 8) WSContentSend_P (PSTR ("'>%d</div>\n"), ld1125_status.occ.th[2]);
      else WSContentSend_P (PSTR ("'>&nbsp;</div>\n"));
  }
  WSContentSend_P (PSTR ("</div>\n"));

  // motion sensor
  detected = LD1125GetMotionDetectionStatus (0);
  distance = min (8, (int)ld1125_status.mov.distance);
  WSContentSend_P (PSTR ("<div style='display:flex;padding:1px 0px;color:%s;'>\n"), LD1125_COLOR_TEXT_OFF);
  WSContentSend_P (PSTR ("<div style='width:28%%;padding:0px;text-align:left;color:white;'>&nbsp;&nbsp;Motion</div>\n"));
  for (index = 0; index < range; index ++)
  {
    // display start
    if (index == 8) WSContentSend_P (PSTR ("<div style='width:14.4%%;padding:0px;"));
      else WSContentSend_P (PSTR ("<div style='width:7.2%%;padding:0px;"));

    // cell style
    if (detected && (index == distance)) WSContentSend_P (PSTR ("background:%s;color:%s;"), LD1125_COLOR_MOTION, LD1125_COLOR_TEXT_ON);
      else WSContentSend_P (PSTR ("background:%s;"), LD1125_COLOR_NONE);

    // cell border
    if ((index == 2) || (index == range - 1)) WSContentSend_P (PSTR ("border-top-right-radius:4px;border-bottom-right-radius:4px;margin-right:1px;"));
    if ((index == 0) || (index == 3)) WSContentSend_P (PSTR ("border-top-left-radius:4px;border-bottom-left-radius:4px;"));
    if (index == 8) WSContentSend_P (PSTR ("border-radius:4px;"));

    // cell value
    LD1125GetPowerText (ld1125_status.mov.power, str_value);
    if (detected && (index == distance)) WSContentSend_P (PSTR ("'>%s</div>\n"), str_value);
      else if ((index == 1) || (range == 1)) WSContentSend_P (PSTR ("'>%d</div>\n"), ld1125_status.mov.th[0]);
      else if ((index == 5) || ((index > 2) && (index < 5) && (index == range - 1))) WSContentSend_P (PSTR ("'>%d</div>\n"), ld1125_status.mov.th[1]);
      else if (index == 8) WSContentSend_P (PSTR ("'>%d</div>\n"), ld1125_status.occ.th[2]);
      else WSContentSend_P (PSTR ("'>&nbsp;</div>\n"));
  }
  WSContentSend_P (PSTR ("</div>\n"));

  // end display
  WSContentSend_P (PSTR ("</div>\n"));
}

#endif  // USE_WEBSERVER

/***************************************\
 *              Interface
\***************************************/

// Teleinfo sensor
bool Xsns121 (uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function) 
  {
    case FUNC_INIT:
      LD1125Init ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kLD1125Commands, LD1125Command);
      break;
    case FUNC_EVERY_250_MSECOND:
      if (ld1125_status.enabled && (TasmotaGlobal.uptime > LD1125_START_DELAY)) LD1125Every250ms ();
      break;
    case FUNC_EVERY_SECOND:
      if (ld1125_status.enabled && (TasmotaGlobal.uptime > LD1125_START_DELAY)) LD1125EverySecond ();
      break;
    case FUNC_JSON_APPEND:
      if (ld1125_status.enabled) LD1125ShowJSON (true);
      break;
    case FUNC_LOOP:
      if (ld1125_status.enabled) LD1125ReceiveData ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      if (ld1125_status.enabled) LD1125WebSensor ();
      break;
#endif  // USE_WEBSERVER
  }

  return result;
}

#endif      // USE_LD1125
