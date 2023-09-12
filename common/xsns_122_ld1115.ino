/*
  xsns_122_ld1115.ino - Driver for Presence and Movement sensor HLK-LD1115

  Copyright (C) 2022  Nicolas Bernaerts

  Connexions :
    * GPIO1 (Tx) should be declared as Serial Tx and connected to HLK-LD1115 Rx
    * GPIO3 (Rx) should be declared as Serial Rx and connected to HLK-LD1115 Tx

  Call LD1115InitDevice (timeout) to declare the device and make it operational

  Version history :
    22/06/2022 - v1.0 - Creation
    12/01/2023 - v2.0 - Complete rewrite

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

#ifdef USE_LD1115

#include <TasmotaSerial.h>

/*************************************************\
 *               Variables
\*************************************************/

// declare teleinfo energy driver and sensor
#define XSNS_122                   122

// constant
#define LD1115_START_DELAY          15          // sensor answers after 15 seconds
#define LD1115_DEFAULT_TIMEOUT      5           // inactivity after 5 sec

#define LD1115_COLOR_TEXT_ON        "white"
#define LD1115_COLOR_TEXT_OFF       "grey"
#define LD1115_COLOR_BORDER         "#444"
#define LD1115_COLOR_NONE           "#444"
#define LD1115_COLOR_PRESENCE       "#E80"
#define LD1115_COLOR_MOTION         "#D00"

// strings
const char D_LD1115_NAME[]          PROGMEM = "HLK-LD1115H";

// MQTT commands : ld_help and ld_send
const char kLD1115Commands[]         PROGMEM = "ld1115_|help|update|save|reset|move_th|pres_th|move_sn|pres_sn|timeout";
void (* const LD1115Command[])(void) PROGMEM = { &CmndLD1115Help, &CmndLD1115Update, &CmndLD1115Save, &CmndLD1115Reset, &CmndLD1115MotionTh, &CmndLD1115PresenceTh, &CmndLD1115MotionSn, &CmndLD1115PresenceSn, &CmndLD1115Timeout };

/****************************************\
 *                 Data
\****************************************/

// HLK-LD1115 sensor general status
struct ld1115_sensor
{
  bool     detected;
  int      sn;
  long     th;
  long     power;
  uint32_t timestamp;
};
static struct {
  bool     enabled  = false;                      // sensor enabled
  bool     detected = false;                      // detection status
  uint32_t timeout  = UINT32_MAX;                 // detection timeout
  char     str_buffer[32];                        // buffer of received data
  String   str_cmnd_list;                         // list of pending commands, separated by ;
  ld1115_sensor mov;
  ld1115_sensor occ;
  TasmotaSerial *pserial = nullptr;               // pointer to serial port
} ld1115_status; 

/**************************************************\
 *                  Commands
\**************************************************/

// hlk-ld sensor help
void CmndLD1115Help ()
{
  // help on command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: HLK-LD1115 sensor commands :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ld1115_move_th <value> = set motion sensitivity threshold"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ld1115_pres_th <value> = set presence sensitivity threshold"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ld1115_move_sn <value> = set motion sample number to use"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ld1115_pres_sn <value> = set presence sample number to use"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ld1115_timeout <value> = set detection time delay (in sec.)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ld1115_update = read all sensor params"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ld1115_save   = save parameters to sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ld1115_reset  = reset to default sensor parameters"));
  ResponseCmndDone ();
}

void CmndLD1115Update (void)
{
  LD1115AppendCommand ("get_all");
  ResponseCmndDone ();
}

void CmndLD1115Save (void)
{
  LD1115AppendCommand ("save");
  ResponseCmndDone ();
}

void CmndLD1115Reset (void)
{
  LD1115AppendCommand ("th1=120;th2=250;mov_sn=3;occ_sn=5;dtime=5;get_all");
  ResponseCmndDone ();
}

void CmndLD1115MotionTh (void)
{
  char str_command[16];

  // if data is valid, add command
  if (XdrvMailbox.payload > 0)
  {
    sprintf (str_command, "th1=%d", XdrvMailbox.payload);
    LD1115AppendCommand (str_command);
    ResponseCmndDone ();
  }

  // else return value
  else ResponseCmndNumber (ld1115_status.mov.th);
}

void CmndLD1115PresenceTh (void)
{
  char str_command[16];

  // if data is valid, add command
  if (XdrvMailbox.payload > 0)
  {
    sprintf (str_command, "th2=%d", XdrvMailbox.payload);
    LD1115AppendCommand (str_command);
    ResponseCmndDone ();
  }

  // else return value
  else ResponseCmndNumber (ld1115_status.occ.th);
}

void CmndLD1115MotionSn (void)
{
  char str_command[16];

  // if data is valid, add command
  if (XdrvMailbox.payload > 0)
  {
    sprintf (str_command, "mov_sn=%d", XdrvMailbox.payload);
    LD1115AppendCommand (str_command);
    ResponseCmndDone ();
  }

  // else return value
  else ResponseCmndNumber (ld1115_status.mov.sn);
}

void CmndLD1115PresenceSn (void)
{
  char str_command[16];

  // if data is valid, add command
  if (XdrvMailbox.payload > 0)
  {
    sprintf (str_command, "occ_sn=%d", XdrvMailbox.payload);
    LD1115AppendCommand (str_command);
    ResponseCmndDone ();
  }

  // else return value
  else ResponseCmndNumber (ld1115_status.occ.sn);
}

void CmndLD1115Timeout (void)
{
  char str_command[16];

  // if data is valid, add command
  if (XdrvMailbox.payload > 0)
  {
    sprintf (str_command, "dtime=%d", XdrvMailbox.payload);
    LD1115AppendCommand (str_command);
    ResponseCmndDone ();
  }

  // else return value
  else ResponseCmndNumber (ld1115_status.timeout);
}

/**************************************************\
 *                  Functions
\**************************************************/

void LD1115AppendCommand (const char* pstr_command)
{
  // check parameter
  if (pstr_command == nullptr) return;
  if (strlen (pstr_command) == 0) return;

  // add command
  if (ld1115_status.str_cmnd_list.length () > 0) ld1115_status.str_cmnd_list += ";";
  ld1115_status.str_cmnd_list += pstr_command;
}

bool LD1115ExecuteNextCommand ()
{
  bool   result;
  int    index;
  String str_command;

  // if at least one command is pending
  result = (ld1115_status.str_cmnd_list.length () > 0);
  if (result)
  {
    // extract first command
    index = ld1115_status.str_cmnd_list.indexOf (';');

    // if some commands are remaining
    if (index != -1)
    {
      str_command = ld1115_status.str_cmnd_list.substring (0, index);
      ld1115_status.str_cmnd_list = ld1115_status.str_cmnd_list.substring (index + 1);
    }

    // else, last command in the queue
    else
    {
      str_command = ld1115_status.str_cmnd_list;
      ld1115_status.str_cmnd_list = "";
    }

    // log command
    AddLog (LOG_LEVEL_INFO, PSTR ("HLK: cmnd %s"), str_command.c_str ());

    // send command
    ld1115_status.pserial->write (str_command.c_str (), str_command.length ());
  }

  return result;
}

// driver initialisation
bool LD1115InitDevice (uint32_t timeout)
{
  // set timeout
  if ((timeout == 0) || (timeout == UINT32_MAX)) ld1115_status.timeout = LD1115_DEFAULT_TIMEOUT;
    else ld1115_status.timeout = timeout;

  // if ports are selected, init sensor state
  if ((ld1115_status.pserial == nullptr) && PinUsed (GPIO_TXD) && PinUsed (GPIO_RXD))
  {
#ifdef ESP32
    // create serial port
    ld1115_status.pserial = new TasmotaSerial (Pin (GPIO_RXD), Pin (GPIO_TXD), 1);

    // initialise serial port
    ld1115_status.enabled = ld1115_status.pserial->begin (115200, SERIAL_8N1);

#else       // ESP8266
    // create serial port
    ld1115_status.pserial = new TasmotaSerial (Pin (GPIO_RXD), Pin (GPIO_TXD), 1);

    // initialise serial port
    ld1115_status.enabled = ld1115_status.pserial->begin (115200, SERIAL_8N1);

    // force hardware configuration on ESP8266
    if (ld1115_status.pserial->hardwareSerial ()) ClaimSerial ();
#endif      // ESP32 & ESP8266

    // flush data
    if (ld1115_status.enabled) ld1115_status.pserial->flush ();

    // first command will ask configuration
    ld1115_status.str_cmnd_list = "get_all";

    // log
    if (ld1115_status.enabled) AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s sensor init succesfull"), D_LD1115_NAME);
    else AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s sensor init failed"), D_LD1115_NAME);
  }

  return ld1115_status.enabled;
}

uint32_t LD1115GetDelaySinceLastDetection ()
{
  uint32_t timestamp = 0;
  uint32_t delay     = UINT32_MAX;

  // check last detection time
  timestamp = max (timestamp, ld1115_status.mov.timestamp);
  timestamp = max (timestamp, ld1115_status.occ.timestamp);

  // calculate delay
  if (timestamp != 0) delay = LocalTime () - timestamp;

  return delay;
}

void LD1115GetDelayText (char* pstr_result, size_t size_result)
{
  uint32_t delay;

  // check parameters
  if (pstr_result == nullptr) return;
  if (size_result < 12) return;

  // get delay
  delay = LD1115GetDelaySinceLastDetection ();

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

bool LD1115GetMotionDetectionStatus (uint32_t timeout)
{
  // if timestamp not defined, no detection
  if (ld1115_status.mov.timestamp == 0) return false;
  
  // if no timeout given, use default one
  if ((timeout == 0) || (timeout == UINT32_MAX)) timeout = ld1115_status.timeout;

  // return timeout status
  return ((LocalTime () - ld1115_status.mov.timestamp <= timeout) && (ld1115_status.mov.power > 0));
}

bool LD1115GetPresenceDetectionStatus (uint32_t timeout)
{
  // if timestamp not defined, no detection
  if (ld1115_status.occ.timestamp == 0) return false;
  
  // if no timeout given, use default one
  if ((timeout == 0) || (timeout == UINT32_MAX)) timeout = ld1115_status.timeout;

  // return timeout status
  return ((LocalTime () - ld1115_status.occ.timestamp <= timeout) && (ld1115_status.occ.power > 0));
}

bool LD1115GetGlobalDetectionStatus (uint32_t timeout)
{
   return (LD1115GetMotionDetectionStatus (timeout) || LD1115GetPresenceDetectionStatus (timeout));
}

/*********************************************\
 *                   Callback
\*********************************************/

// driver initialisation
void LD1115Init ()
{
  // init reception buffer
  strcpy (ld1115_status.str_buffer, "");

  // init motion sensor
  ld1115_status.mov.detected  = false;
  ld1115_status.mov.sn        = INT_MAX;
  ld1115_status.mov.th        = LONG_MAX;
  ld1115_status.mov.power     = 0;
  ld1115_status.mov.timestamp = 0;

  // init static sensor
  ld1115_status.occ.detected  = false;
  ld1115_status.occ.sn        = INT_MAX;
  ld1115_status.occ.th        = LONG_MAX;
  ld1115_status.occ.power     = 0;
  ld1115_status.occ.timestamp = 0;

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ld1115_help to get help on %s commands"), D_LD1115_NAME);
}

// loop every 250 msecond
void LD1115Every250ms ()
{
  // check status
  if (!ld1115_status.enabled) return;

  // execute any command in the pipe
  LD1115ExecuteNextCommand ();
}

void LD1115EverySecond ()
{
  bool     detected;
  uint32_t time_now = LocalTime ();

  // if presence timeout is reached, reset presence detection
  ld1115_status.mov.detected = (time_now <= ld1115_status.mov.timestamp + ld1115_status.timeout);
  ld1115_status.occ.detected = (time_now <= ld1115_status.occ.timestamp + ld1115_status.timeout);

  // check for MQTT update
  detected = (ld1115_status.mov.detected || ld1115_status.occ.detected);
  if (ld1115_status.detected != detected)
  {
    ld1115_status.detected = detected;
    LD1115ShowJSON (false);
  }
}

// Handling of received data
void LD1115ReceiveData ()
{
  bool detected;
  long value;
  char *pstr_key;
  char *pstr_value;
  char str_recv[2];
  char str_key[16];

  // check sensor presence
  if (ld1115_status.pserial == nullptr) return;
  if (!ld1115_status.enabled) return;

  // init strings
  strcpy (str_key, "");
  strcpy (str_recv, " ");

  // run serial receive loop
  while (ld1115_status.pserial->available ()) 
  {
    // handle received character
    str_recv[0] = (char)ld1115_status.pserial->read (); 
    switch (str_recv[0])
    {
      // CR is ignored
      case 0x0D:
        break;
          
      // LF needs line analysis
      case 0x0A:
        //log received data
        AddLog (LOG_LEVEL_DEBUG_MORE, PSTR ("HLK: %s"), ld1115_status.str_buffer);

        // if received static level
        if (strstr (ld1115_status.str_buffer, "occ,") == ld1115_status.str_buffer)
        {
          pstr_value = strrchr (ld1115_status.str_buffer, ' ');
          if (pstr_value != nullptr)
          {
            ld1115_status.occ.power = atol (pstr_value + 1);
            if (ld1115_status.occ.power > 0)
            {
              ld1115_status.occ.detected  = true;
              ld1115_status.occ.timestamp = LocalTime ();
            }
          }
        }

        // else if received motion level
        else if (strstr (ld1115_status.str_buffer, "mov,") == ld1115_status.str_buffer)
        {
          pstr_value = strrchr (ld1115_status.str_buffer, ' ');
          if (pstr_value != nullptr)
          {
            ld1115_status.mov.power = atol (pstr_value + 1);
            if (ld1115_status.mov.power > 0)
            {
              ld1115_status.mov.detected  = true;
              ld1115_status.mov.timestamp = LocalTime ();
            }
          }
        }

        // else if received configuration value
        else if (strstr (ld1115_status.str_buffer, " is ") != nullptr)
        {
          // init
          value = LONG_MAX;
          strcpy (str_key, "");
          pstr_value = nullptr;

          // look for key and value
          pstr_key = strchr (ld1115_status.str_buffer, ' ');
          if (pstr_key != nullptr) pstr_value = strchr (pstr_key + 1, ' ');

          // extract key
          if (pstr_key != nullptr)
          {
            *pstr_key = 0;
            strlcpy (str_key, ld1115_status.str_buffer, sizeof (str_key));
          }

          // extract value
          if (pstr_value != nullptr) value = atol (pstr_value + 1);

          // update value according to key
          if ((strlen (str_key) > 0) && (value != LONG_MAX))
          {
            // assign value
            if (strstr (str_key, "th1") != nullptr) ld1115_status.mov.th = value;
            else if (strstr (str_key, "th2") != nullptr) ld1115_status.occ.th = value;
            else if (strstr (str_key, "mov_sn") != nullptr) ld1115_status.mov.sn = (int)value;
            else if (strstr (str_key, "occ_sn") != nullptr) ld1115_status.occ.sn = (int)value;
            else if (strstr (str_key, "dtime") != nullptr) ld1115_status.timeout = (uint16_t)value / 1000;

            // log
            AddLog (LOG_LEVEL_INFO, PSTR ("HLK: recv %s=%d"), str_key, value);
          }
        }

        // empty reception buffer
        strcpy (ld1115_status.str_buffer, "");
        break;

      // default : add current caracter to buffer
      default:
        strlcat (ld1115_status.str_buffer, str_recv, sizeof (ld1115_status.str_buffer));
        break;
    }

    // give control back to the system to avoid watchdog
    yield ();
  }

  // check for MQTT update
  detected = (ld1115_status.mov.detected || ld1115_status.occ.detected);
  if (ld1115_status.detected != detected)
  {
    ld1115_status.detected = detected;
    LD1115ShowJSON (false);
  }
}

// Show JSON status (for MQTT)
//  false ->  "HLK-LD1115":{"detect":1}
//  true  ->  "HLK-LD1115":{"detect":1,"timeout":5,"occ":{"odetect":1,"opower":176},"mov":{"mdetect":0,"mpower":127},"delay":{"dval":4,"dtext"="2 s"}}
void LD1115ShowJSON (bool append)
{
  bool     result;
  uint32_t delay;
  char     str_text[32];
  String   str_config;

  // check sensor presence
  if (ld1115_status.pserial == nullptr) return;
  if (!ld1115_status.enabled) return;

  // add , in append mode or { in direct publish mode
  if (append) ResponseAppend_P (PSTR (",")); else Response_P (PSTR ("{"));

  // start of sensor section
  ResponseAppend_P (PSTR ("\"%s\":{\"detect\":%u"), D_LD1115_NAME, ld1115_status.detected);

  if (append)
  {
    // timeout
    ResponseAppend_P (PSTR (",\"timeout\":%u"), ld1115_status.timeout);

    // presence (occ)
    result = LD1115GetPresenceDetectionStatus (UINT32_MAX);
    ResponseAppend_P (PSTR (",\"occ\":{\"ostate\":%u"), result);
    if (result) ResponseAppend_P (PSTR (",\"opower\":%d"), ld1115_status.occ.power);
    ResponseAppend_P (PSTR ("}"));

    // motion (mov)
    result = LD1115GetMotionDetectionStatus (UINT32_MAX);
    ResponseAppend_P (PSTR (",\"mov\":{\"mstate\":%u"), result);
    if (result) ResponseAppend_P (PSTR (",\"mpower\":%d"), ld1115_status.mov.power);
    ResponseAppend_P (PSTR ("}"));

    // delay since last detection
    delay = LD1115GetDelaySinceLastDetection ();
    LD1115GetDelayText (str_text, sizeof (str_text));
    if (delay != UINT32_MAX)   ResponseAppend_P (PSTR (",\"delay\":{\"dval\":%u,\"dtext\":\"%s\"}"), delay, str_text);
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

// Append HLK-LD1115 sensor data to main page
void LD1115WebSensor ()
{
  bool detected;
  int  index;
  char str_value[16];

  // check status
  if (!ld1115_status.enabled) return;

  WSContentSend_P (PSTR ("<div style='font-size:10px;text-align:center;margin:0px;padding:1px;border:1px solid #666;border-radius:6px;'>\n"));

  // presence sensor
  WSContentSend_P (PSTR ("<div style='display:flex;margin-top:-2px;'>\n"));
  WSContentSend_P (PSTR ("<div style='width:20%%;padding:0px;text-align:left;'>Presence</div>\n"));
  detected = LD1115GetPresenceDetectionStatus (0);
  if (ld1115_status.occ.th != LONG_MAX) ltoa (ld1115_status.occ.th, str_value, 10); else strcpy (str_value, "---");
  if (detected) WSContentSend_P (PSTR ("<div style='width:20%%;padding:0px;border:1px solid %s;border-radius:4px;background:%s;color:%s;'>%d</div>\n"), LD1115_COLOR_PRESENCE, LD1115_COLOR_PRESENCE, LD1115_COLOR_TEXT_ON, ld1115_status.occ.power);
    else WSContentSend_P (PSTR ("<div style='width:20%%;padding:0px;border:1px solid %s;border-radius:4px;background:%s;color:%s;'>%s</div>\n"), LD1115_COLOR_BORDER, LD1115_COLOR_NONE, LD1115_COLOR_TEXT_OFF, str_value);
  WSContentSend_P (PSTR ("<div style='width:60%%;padding:0px;background:none;'>&nbsp;</div>\n"));
  WSContentSend_P (PSTR ("</div>\n"));

  // motion sensor
  WSContentSend_P (PSTR ("<div style='display:flex;margin-top:-8px;'>\n"));
  WSContentSend_P (PSTR ("<div style='width:20%%;padding:0px;text-align:left;'>Motion</div>\n"));
  detected = LD1115GetMotionDetectionStatus (0);
  if (ld1115_status.mov.th != LONG_MAX) ltoa (ld1115_status.mov.th, str_value, 10); else strcpy (str_value, "---");
  if (detected) WSContentSend_P (PSTR ("<div style='width:80%%;padding:0px;border:1px solid %s;border-radius:4px;background:%s;color:%s;'>%d</div>\n"), LD1115_COLOR_MOTION, LD1115_COLOR_MOTION, LD1115_COLOR_TEXT_ON, ld1115_status.mov.power);
    else WSContentSend_P (PSTR ("<div style='width:80%%;padding:0px;border:1px solid %s;border-radius:4px;background:%s;color:%s;'>%s</div>\n"), LD1115_COLOR_BORDER, LD1115_COLOR_NONE, LD1115_COLOR_TEXT_OFF, str_value);
  WSContentSend_P (PSTR ("</div>\n"));

  // scale
  WSContentSend_P (PSTR ("<div style='display:flex;margin-top:-10px;padding:0px;'>\n"));
  WSContentSend_P (PSTR ("<div style='width:20%%;padding-bottom:2px;'>&nbsp;</div>\n"));
  WSContentSend_P (PSTR ("<div style='width:10%%;padding-bottom:2px;text-align:left;'>%um</div>\n"), 0);
  for (index = 1; index < 4; index ++) WSContentSend_P (PSTR ("<div style='width:20%%;padding-bottom:2px;'>%um</div>\n"), index * 4);
  WSContentSend_P (PSTR ("<div style='width:10%%;padding-bottom:2px;text-align:right;'>%um</div>\n"), 16);
  WSContentSend_P (PSTR ("</div>\n"));

  WSContentSend_P (PSTR ("</div>\n"));
}

#endif  // USE_WEBSERVER

/***************************************\
 *              Interface
\***************************************/

// Teleinfo sensor
bool Xsns122 (uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function) 
  {
    case FUNC_INIT:
      LD1115Init ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kLD1115Commands, LD1115Command);
      break;
    case FUNC_EVERY_250_MSECOND:
      if (RtcTime.valid) LD1115Every250ms ();
      break;
    case FUNC_EVERY_SECOND:
      if (RtcTime.valid) LD1115EverySecond ();
      break;
    case FUNC_JSON_APPEND:
      LD1115ShowJSON (true);
      break;
    case FUNC_LOOP:
      LD1115ReceiveData ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      LD1115WebSensor ();
      break;
#endif  // USE_WEBSERVER
  }

  return result;
}

#endif      // USE_LD1115

