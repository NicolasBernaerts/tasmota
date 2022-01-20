/*
  xsns_98_remote_value.ino - Collect remote temperature, humidity, ... thru MQTT 

  Remote sensor value is accessed thru an MQTT topic/key.
  
  Copyright (C) 2020  Nicolas Bernaerts
    15/01/2020 - v1.0 - Creation with management of remote MQTT sensor
    23/04/2021 - v1.1 - Remove use of String to avoid heap fragmentation
    18/06/2021 - v1.2 - Bug fixes
    15/11/2021 - v1.3 - Add LittleFS management
    19/11/2021 - v1.4 - Handle drift and priority between local and remote sensors
                      
  If LittleFS partition is available, settings are stored in remote.cfg
  Otherwise, settings are stored using Settings Text :
    * SET_REMOTE_TOPIC
    * SET_REMOTE_KEY

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

/*************************************************\
 *              Remote Sensor
\*************************************************/

#ifdef USE_REMOTE_SENSOR

#define XDRV_97                         97

#define REMOTE_SENSOR_TIMEOUT      300000

#define REMOTE_TOPIC_LENGTH        64
#define REMOTE_KEY_LENGTH          16

#define REMOTE_TEMP_DRIFT_MAX           10
#define REMOTE_TEMP_DRIFT_STEP          0.1

#define D_PAGE_REMOTE              "rsensor"

#define D_CMND_REMOTE              "rsensor_"
#define D_CMND_REMOTE_VALUE        "value"
#define D_CMND_REMOTE_DRIFT        "drift"
#define D_CMND_REMOTE_TOPIC        "topic"
#define D_CMND_REMOTE_KEY          "key"

//#define D_JSON_REMOTE              "Remote"
//#define D_JSON_REMOTE_VALUE        "Value"
//#define D_JSON_REMOTE_DRIFT        "Drift"
//#define D_JSON_REMOTE_TOPIC        "Topic"
//#define D_JSON_REMOTE_KEY          "Key"

#define D_REMOTE                   "Remote"
#define D_REMOTE_SENSOR            "Sensor"
#define D_REMOTE_CONFIGURE         "Configure"
#define D_REMOTE_TOPIC             "MQTT Topic"
#define D_REMOTE_KEY               "MQTT JSON Key"
#define D_REMOTE_CORRECTION        "Correction"


#ifdef USE_UFILESYS
#define D_REMOTE_FILE_CFG          "/remote.cfg"
#endif     // USE_UFILESYS

// remote sensor commands
const char kRemoteCommands[] PROGMEM = D_CMND_REMOTE "|" D_CMND_REMOTE_TOPIC "|" D_CMND_REMOTE_KEY "|" D_CMND_REMOTE_DRIFT "|" D_CMND_REMOTE_VALUE;
void (* const RemoteCommand[])(void) PROGMEM = { &CmndRemoteTopic, &CmndRemoteKey, &CmndRemoteDrift, &CmndRemoteValue };

// form topic style
const char REMOTE_FIELDSET_START[] PROGMEM = "<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>\n";
const char REMOTE_FIELDSET_STOP[]  PROGMEM = "</fieldset></p>\n";
const char REMOTE_FIELD_INPUT[]    PROGMEM = "<p>%s<span class='key'>%s</span><br><input name='%s' value='%s'></p>\n";
const char REMOTE_FIELD_CONFIG[]   PROGMEM = "<p>%s (%s)<span class='key'>%s</span><br><input type='number' name='%s' min='%d' max='%d' step='%s' value='%s'></p>\n";

/*************************************************\
 *               Variables
\*************************************************/

struct {
  char str_topic[REMOTE_TOPIC_LENGTH];   // remote sensor topic
  char str_key[REMOTE_KEY_LENGTH];       // remote sensor key
} remote_config;

struct {
  float    mqtt_value = NAN;                  // current remote value
  uint32_t update_ts  = 0;                    // last time (in millis) remote value was updated
  char     str_unit_temp[4];                  // temperature unit
} remote_status;

/**************************************************\
 *                  Accessors
\**************************************************/

void RemoteLoadConfig () 
{
  char temp_unit = TempUnit ();

  // get temperature unit
  strcpy (remote_status.str_unit_temp, "°");
  strncat (remote_status.str_unit_temp, &temp_unit, 1);

#ifdef USE_UFILESYS

  // retrieve saved settings from flash filesystem
  UfsCfgLoadKey (D_REMOTE_FILE_CFG, D_CMND_REMOTE_TOPIC, remote_config.str_topic, sizeof (remote_config.str_topic));
  UfsCfgLoadKey (D_REMOTE_FILE_CFG, D_CMND_REMOTE_KEY, remote_config.str_key, sizeof (remote_config.str_key));

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("RSR: Config from LittleFS"));

#else       // No LittleFS

  // retrieve saved settings from flash memory
  strlcpy (remote_config.str_topic, SettingsText (SET_REMOTE_TOPIC), sizeof (remote_config.str_topic));
  strlcpy (remote_config.str_key, SettingsText (SET_REMOTE_KEY), sizeof (remote_config.str_key));

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("RSR: Config from Settings"));

# endif     // USE_UFILESYS
}

void RemoteSaveConfig () 
{
#ifdef USE_UFILESYS

  // save settings into flash filesystem
  UfsCfgSaveKey (D_REMOTE_FILE_CFG, D_CMND_REMOTE_TOPIC, remote_config.str_topic, true);
  UfsCfgSaveKey (D_REMOTE_FILE_CFG, D_CMND_REMOTE_KEY, remote_config.str_key, false);

# else       // No LittleFS

  // save settings into flash memory
  SettingsUpdateText (SET_REMOTE_TOPIC, remote_config.str_topic);
  SettingsUpdateText (SET_REMOTE_KEY, remote_config.str_key);

# endif     // USE_UFILESYS
}

/**************************************************\
 *                  Functions
\**************************************************/

// get current remote sensor value
float RemoteGetValue ()
{
  float value;

  // get remote sensor value with correction
  value = remote_status.mqtt_value;
  if (!isnan (value)) value += (float)Settings->temp_comp / 10;

  return value;
}

// check and update MQTT power subsciption after disconnexion
void RemoteMqttSubscribe ()
{
  // if topic is defined, subscribe
  if (strlen (remote_config.str_topic) > 0)
  {
    // subscribe to sensor topic
    MqttSubscribe (remote_config.str_topic);

    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("RSR: Subscribed to %s"), remote_config.str_topic);
  }
}

// read received MQTT data to retrieve sensor value
bool RemoteMqttData ()
{
  bool        data_handled = false;
  float       value;
  char*       pstr_result = nullptr;
  char*       pstr_value  = nullptr;
  char        str_buffer[REMOTE_TOPIC_LENGTH];

  // if topic is the right one
  if (strcmp (remote_config.str_topic, XdrvMailbox.topic) == 0)
  {
    // log
    AddLog (LOG_LEVEL_DEBUG, PSTR ("RSR Received %s"), remote_config.str_topic);

    // look for sensor key
    sprintf_P (str_buffer, PSTR ("\"%s\""), remote_config.str_key);
    pstr_result = strstr (XdrvMailbox.data, str_buffer);
    if (pstr_result != nullptr) pstr_value = strchr (pstr_result, ':');

    // if key is found,
    if (pstr_value != nullptr)
    {
      // extract value
      pstr_value++;
      value = strtof (pstr_value, &pstr_result);

      // if value has been read
      data_handled = (pstr_value != pstr_result);
      if (data_handled)
      {
        // save remote value
        remote_status.update_ts  = millis ();
        remote_status.mqtt_value = value;

        // log and counter increment
        ext_snprintf_P (str_buffer, sizeof (str_buffer), PSTR ("%01_f"), &value);
        AddLog (LOG_LEVEL_INFO, PSTR ("RSR: Remote value is %s"), str_buffer);
      }
    }
  }

  return data_handled;
}

// reset sensor data in case of timeout
void RemoteEverySecond ()
{
  uint32_t time_over = remote_status.update_ts + REMOTE_SENSOR_TIMEOUT;

  // if no update for 5 minutes, remote sensor reset
  if (TimeReached (time_over)) remote_status.mqtt_value = NAN;
}

/***********************************************\
 *                  Commands
\***********************************************/

void CmndRemoteTopic ()
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.data_len < sizeof (remote_config.str_topic)))
  {
    strncpy (remote_config.str_topic, XdrvMailbox.data, XdrvMailbox.data_len);
    RemoteSaveConfig ();
  }
  ResponseCmndChar (remote_config.str_topic);
}

void CmndRemoteKey ()
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.data_len < sizeof (remote_config.str_topic)))
  {
    strncpy (remote_config.str_key, XdrvMailbox.data, XdrvMailbox.data_len);
    RemoteSaveConfig ();
  }
  ResponseCmndChar (remote_config.str_key);
}

void CmndRemoteDrift ()
{
  float drift;

  if (XdrvMailbox.data_len > 0)
  {
    drift = atof (XdrvMailbox.data) * 10;
    Settings->temp_comp = (int8_t)drift;
  }
  drift = (float)Settings->temp_comp / 10;
  ResponseCmndFloat (drift, 1);
}

void CmndRemoteValue ()
{
  ResponseCmndFloat (RemoteGetValue (), 1);
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// remote sensor configuration web page
void RemoteWebPage ()
{
  float drift, step;
  char  str_value[8];
  char  str_step[8];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess ()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg ("save"))
  {
    // set MQTT topic according to 'sensor_topic' parameter
    WebGetArg (D_CMND_REMOTE_TOPIC, remote_config.str_topic, sizeof (remote_config.str_topic));

    // set JSON key according to 'sensor_key' parameter
    WebGetArg (D_CMND_REMOTE_KEY, remote_config.str_key, sizeof (remote_config.str_key));

    // get sensor drift according to 'drift' parameter
    WebGetArg (D_CMND_REMOTE_DRIFT, str_value, sizeof (str_value));
    if (strlen (str_value) > 0)
    {
      drift = atof (str_value) * 10;
      Settings->temp_comp = (int8_t)drift;
    }

    // save config
    RemoteSaveConfig ();

    // restart device
    WebRestart (1);
  }

  // beginning of form
  WSContentStart_P (D_REMOTE_CONFIGURE);
  WSContentSendStyle ();
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("span.key {float:right;font-size:0.7rem;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_PAGE_REMOTE);

  //   Correction 
  // ---------------

  WSContentSend_P (REMOTE_FIELDSET_START, D_REMOTE_CORRECTION);
  WSContentSend_P (PSTR ("<p>\n"));

  // sensor correction label and input
  drift = (float)Settings->temp_comp / 10;
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &drift);
  step = REMOTE_TEMP_DRIFT_STEP;
  ext_snprintf_P (str_step, sizeof (str_step), PSTR ("%1_f"), &step);
  WSContentSend_P (REMOTE_FIELD_CONFIG, D_REMOTE_CORRECTION, remote_status.str_unit_temp, D_CMND_REMOTE_DRIFT, D_CMND_REMOTE_DRIFT, - REMOTE_TEMP_DRIFT_MAX, REMOTE_TEMP_DRIFT_MAX, str_step, str_value);

  WSContentSend_P (PSTR ("</p>\n"));
  WSContentSend_P (REMOTE_FIELDSET_STOP);

  //   remote sensor  
  // -----------------

  WSContentSend_P (REMOTE_FIELDSET_START, D_REMOTE " " D_REMOTE_SENSOR);
  WSContentSend_P (PSTR ("<p>\n"));

  WSContentSend_P (REMOTE_FIELD_INPUT, D_REMOTE_TOPIC, D_CMND_REMOTE_TOPIC, D_CMND_REMOTE_TOPIC, remote_config.str_topic);
  WSContentSend_P (REMOTE_FIELD_INPUT, D_REMOTE_KEY, D_CMND_REMOTE_KEY, D_CMND_REMOTE_KEY, remote_config.str_key);

  WSContentSend_P (PSTR ("</p>\n"));
  WSContentSend_P (REMOTE_FIELDSET_STOP);


  // end of form and save button
  WSContentSend_P (PSTR ("<p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), D_SAVE);
  WSContentSend_P (PSTR ("</form>\n"));

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

#endif  // USE_WEBSERVER

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xdrv97 (uint8_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
   case FUNC_INIT:
      RemoteLoadConfig ();
      break;
    case FUNC_EVERY_SECOND:
      RemoteEverySecond ();
      break;
    case FUNC_MQTT_SUBSCRIBE:
      RemoteMqttSubscribe ();
      break;
    case FUNC_MQTT_DATA:
      result = RemoteMqttData ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kRemoteCommands, RemoteCommand);
      break;
#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      Webserver->on ("/" D_PAGE_REMOTE, RemoteWebPage);
      break;
    case FUNC_WEB_ADD_BUTTON:
      WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s %s</button></form></p>"), D_PAGE_REMOTE, D_REMOTE_CONFIGURE, D_REMOTE_SENSOR);
      break;
#endif  // USE_WEBSERVER
  }
  
  return result;
}

#endif // USE_REMOTE_SENSOR
