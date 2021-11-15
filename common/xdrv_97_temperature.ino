/*
  xdrv_97_temperature.ino - Retreive Temperature thru MQTT (~2.5 kb)
  
  Copyright (C) 2020  Nicolas Bernaerts
    15/01/2020 - v1.0 - Creation with management of remote MQTT sensor
    23/04/2021 - v1.1 - Remove use of String to avoid heap fragmentation
    18/06/2021 - v1.2 - Bug fixes
    15/11/2021 - v1.3 - Add LittleFS management
                      
  If LittleFS partition is available, settings are stored in temperature.cfg
  Otherwise, settings are stored using Settings Text :
    * SET_TEMPERATURE_TOPIC
    * SET_TEMPERATURE_KEY

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
 *              Temperature
\*************************************************/

#ifdef USE_TEMPERATURE_MQTT

#define XDRV_97                     97
#define XSNS_97                     97

#define TEMPERATURE_UPDATE_TIMEOUT  300000

#define TEMPERATURE_TOPIC_LENGTH    64
#define TEMPERATURE_KEY_LENGTH      16

#define D_PAGE_TEMPERATURE          "temp"

#define D_CMND_TEMPERATURE_TOPIC    "topic"
#define D_CMND_TEMPERATURE_KEY      "key"

#define D_JSON_TEMPERATURE_VALUE    "State"
#define D_JSON_TEMPERATURE_TOPIC    "Topic"
#define D_JSON_TEMPERATURE_KEY      "Key"

#define D_TEMPERATURE_CONFIGURE     "Configure Temperature"
#define D_TEMPERATURE_REMOTE        "Temperature remote sensor"
#define D_TEMPERATURE_TOPIC         "MQTT Topic"
#define D_TEMPERATURE_KEY           "MQTT JSON Key"

#ifdef USE_UFILESYS
#define D_TEMPERATURE_FILE_CFG            "/temperature.cfg"
#endif     // USE_UFILESYS

// temperature commands
const char kTemperatureCommands[] PROGMEM = "temp_" "|" D_CMND_TEMPERATURE_TOPIC "|" D_CMND_TEMPERATURE_KEY;
void (* const TemperatureCommand[])(void) PROGMEM = { &CmndTemperatureTopic, &CmndTemperatureKey };

// form topic style
const char TEMPERATURE_FIELDSET_START[] PROGMEM = "<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>\n";
const char TEMPERATURE_FIELDSET_STOP[] PROGMEM = "</fieldset></p>\n";
const char TEMPERATURE_FIELD_INPUT[] PROGMEM = "<p>%s<span style='float:right;font-size:0.7rem;'>%s</span><br><input name='%s' value='%s'></p>\n";

/*************************************************\
 *               Variables
\*************************************************/

struct {
  char str_topic[TEMPERATURE_TOPIC_LENGTH];   // remote temperature topic
  char str_key[TEMPERATURE_KEY_LENGTH];       // remote temperature key
} mqtt_temperature_config;

struct {
  bool     subscribed  = false;               // flag for temperature subscription
  uint32_t last_update = 0;                   // last time (in millis) temperature was updated
  float    value       = NAN;                 // current temperature
} mqtt_temperature_status;

/**************************************************\
 *                  Accessors
\**************************************************/

void TemperatureLoadConfig () 
{
#ifdef USE_UFILESYS

  // retrieve saved settings from flash filesystem
  UfsCfgLoadKey (D_TEMPERATURE_FILE_CFG, D_CMND_TEMPERATURE_TOPIC, mqtt_temperature_config.str_topic, sizeof (mqtt_temperature_config.str_topic));
  UfsCfgLoadKey (D_TEMPERATURE_FILE_CFG, D_CMND_TEMPERATURE_KEY, mqtt_temperature_config.str_key, sizeof (mqtt_temperature_config.str_key));

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("RTM : Config from LittleFS"));

#else       // No LittleFS

  // retrieve saved settings from flash memory
  strlcpy (mqtt_temperature_config.str_topic, SettingsText (SET_TEMPERATURE_TOPIC), sizeof (mqtt_temperature_config.str_topic));
  strlcpy (mqtt_temperature_config.str_key, SettingsText (SET_TEMPERATURE_KEY), sizeof (mqtt_temperature_config.str_key));

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("RTM : Config from Settings"));

# endif     // USE_UFILESYS
}

void TemperatureSaveConfig () 
{
#ifdef USE_UFILESYS

  // save settings into flash filesystem
  UfsCfgSaveKey (D_TEMPERATURE_FILE_CFG, D_CMND_TEMPERATURE_TOPIC, mqtt_temperature_config.str_topic, true);
  UfsCfgSaveKey (D_TEMPERATURE_FILE_CFG, D_CMND_TEMPERATURE_KEY, mqtt_temperature_config.str_key, false);

# else       // No LittleFS

  // save settings into flash memory
  SettingsUpdateText (SET_TEMPERATURE_TOPIC, mqtt_temperature_config.str_topic);
  SettingsUpdateText (SET_TEMPERATURE_KEY, mqtt_temperature_config.str_key);

# endif     // USE_UFILESYS
}

/**************************************************\
 *                  Functions
\**************************************************/

// get current temperature
float TemperatureGetValue ()
{
  uint32_t time_over;

  // if no update for 5 minutes, temperature not available
  time_over = mqtt_temperature_status.last_update + TEMPERATURE_UPDATE_TIMEOUT;
  if (TimeReached (time_over)) mqtt_temperature_status.value = NAN;

  return mqtt_temperature_status.value;
}

// update temperature
void TemperatureSetValue (float new_temperature)
{
  // set current temperature and reset update trigger
  mqtt_temperature_status.value = new_temperature;
  mqtt_temperature_status.last_update = millis ();
}

// Show JSON status (for MQTT)
void TemperatureShowJSON (bool append)
{
  float temperature;
  char  str_value[8];

  // if temperature is available
  temperature = TemperatureGetValue ();
  if (!isnan (temperature))
  {
    // add , in append mode or { in publish mode
    if (append) ResponseAppend_P (PSTR (",")); 
    else Response_P (PSTR ("{"));

    // temperature  -->  "Temperature":{"Value":18.5,"Topic":"mqtt/topic/of/temperature","Key":"Temp"}
    ResponseAppend_P (PSTR ("\"%s\":{"), D_JSON_TEMPERATURE);
    ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &temperature);
    ResponseAppend_P (PSTR ("\"%s\":%s,\"%s\":\"%s\",\"%s\":\"%s\""), D_JSON_TEMPERATURE_VALUE, str_value, D_JSON_TEMPERATURE_TOPIC, mqtt_temperature_config.str_topic, D_JSON_TEMPERATURE_KEY, mqtt_temperature_config.str_key);
    ResponseAppend_P (PSTR ("}"));

    // publish it if not in append mode
    if (!append)
    {
      ResponseAppend_P (PSTR ("}"));
      MqttPublishPrefixTopic_P (TELE, PSTR (D_RSLT_SENSOR));
    } 
  }
}

// MQTT connexion update
void TemperatureCheckMqttConnexion ()
{
  // if topic defined, check MQTT connexion
  if (strlen (mqtt_temperature_config.str_topic) > 0)
  {
    // if connected to MQTT server and no subsciption to temperature topic
    if (MqttIsConnected () && (mqtt_temperature_status.subscribed == false))
    {
      // subscribe to temperature meter
      MqttSubscribe (mqtt_temperature_config.str_topic);
      mqtt_temperature_status.subscribed = true;

      // log
      AddLog (LOG_LEVEL_INFO, PSTR ("MQT: Subscribed to %s"), mqtt_temperature_config.str_topic);
    }

    // else disconnected : topic not subscribed
    else mqtt_temperature_status.subscribed = false;
  }
}

// read received MQTT data to retrieve temperature
bool TemperatureMqttData ()
{
  bool        data_handled = false;
  float       temperature;
  char*       pstr_result = nullptr;
  char*       pstr_value  = nullptr;
  char        str_buffer[TEMPERATURE_TOPIC_LENGTH];

  // if MQTT subscription is active
  if (mqtt_temperature_status.subscribed)
  {
    // if topic is the right one
    if (strcmp (mqtt_temperature_config.str_topic, XdrvMailbox.topic) == 0)
    {
      // look for temperature key
      sprintf (str_buffer, "\"%s\"", mqtt_temperature_config.str_key);
      pstr_result = strstr (XdrvMailbox.data, str_buffer);
      if (pstr_result != nullptr) pstr_value = strchr (pstr_result, ':');

      // if key is found,
      if (pstr_value != nullptr)
      {
        // extract temperature
        pstr_value++;
        temperature = strtof (pstr_value, &pstr_result);

        // if temperature has been read
        data_handled = (pstr_value != pstr_result);
        if (data_handled)
        {
          // save temperature
          TemperatureSetValue (temperature);

          // log and counter increment
          ext_snprintf_P (str_buffer, sizeof (str_buffer), PSTR ("%02_f"), &temperature);
          AddLog (LOG_LEVEL_INFO, PSTR ("MQT: Received temperature as %s °C"), str_buffer);
        }
      }
    }
  }

  return data_handled;
}

/***********************************************\
 *                  Commands
\***********************************************/

void CmndTemperatureTopic ()
{
  // update and save topic
  strlcpy (mqtt_temperature_config.str_topic, XdrvMailbox.data, sizeof (mqtt_temperature_config.str_topic));
  TemperatureSaveConfig ();
  ResponseCmndChar (XdrvMailbox.data);
}

void CmndTemperatureKey ()
{
  // update and save key
  strlcpy (mqtt_temperature_config.str_key, XdrvMailbox.data, sizeof (mqtt_temperature_config.str_key));
  TemperatureSaveConfig ();
  ResponseCmndChar (XdrvMailbox.data);
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// Temperature MQTT setting web page
void TemperatureWebPage ()
{
  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess ()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg ("save"))
  {
    // set MQTT topic according to 'ttopic' parameter
    WebGetArg (D_CMND_TEMPERATURE_TOPIC, mqtt_temperature_config.str_topic, sizeof (mqtt_temperature_config.str_topic));

    // set JSON key according to 'tkey' parameter
    WebGetArg (D_CMND_TEMPERATURE_KEY, mqtt_temperature_config.str_key, sizeof (mqtt_temperature_config.str_key));

    // save config
    TemperatureSaveConfig ();
  }

  // beginning of form
  WSContentStart_P (D_TEMPERATURE_CONFIGURE);
  WSContentSendStyle ();
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_PAGE_TEMPERATURE);

  // remote sensor section  
  // --------------
  WSContentSend_P (TEMPERATURE_FIELDSET_START, D_TEMPERATURE_REMOTE);

  // remote sensor mqtt topic
  WSContentSend_P (TEMPERATURE_FIELD_INPUT, D_TEMPERATURE_TOPIC, D_CMND_TEMPERATURE_TOPIC, D_CMND_TEMPERATURE_TOPIC, mqtt_temperature_config.str_topic);

  // remote sensor json key
  WSContentSend_P (TEMPERATURE_FIELD_INPUT, D_TEMPERATURE_KEY, D_CMND_TEMPERATURE_KEY, D_CMND_TEMPERATURE_KEY, mqtt_temperature_config.str_key);
  WSContentSend_P (TEMPERATURE_FIELDSET_STOP);

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
      TemperatureLoadConfig ();
      break;
   case FUNC_MQTT_DATA:
      result = TemperatureMqttData ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kTemperatureCommands, TemperatureCommand);
      break;
    case FUNC_EVERY_SECOND:
      TemperatureCheckMqttConnexion ();
      break;
  }
  
  return result;
}

bool Xsns97 (uint8_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      Webserver->on ("/" D_PAGE_TEMPERATURE, TemperatureWebPage);
      break;
    case FUNC_WEB_ADD_BUTTON:
      WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>Configure Remote Temperature</button></form></p>"), D_PAGE_TEMPERATURE);
      break;
#endif  // USE_WEBSERVER
  }
  
  return result;
}

#endif // USE_TEMPERATURE
