/*
  xdrv_97_temperature.ino - Retreive Temperature thru MQTT (~2.5 kb)
  
  Copyright (C) 2020  Nicolas Bernaerts
    15/01/2020 - v1.0 - Creation with management of remote MQTT sensor
    23/04/2021 - v1.1 - Remove use of String to avoid heap fragmentation
    18/06/2021 - v1.2 - Bug fixes
                      
  Settings are stored using Settings Text :
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

#define TEMPERATURE_TIMEOUT_5MN     300000

#define TEMPERATURE_PARAM_TOPIC     4
#define TEMPERATURE_PARAM_KEY       5

#define D_PAGE_TEMPERATURE          "temp"

#define D_CMND_TEMPERATURE_TOPIC    "ttopic"
#define D_CMND_TEMPERATURE_KEY      "tkey"

#define D_JSON_TEMPERATURE_VALUE    "State"
#define D_JSON_TEMPERATURE_TOPIC    "Topic"
#define D_JSON_TEMPERATURE_KEY      "Key"

#define D_TEMPERATURE_CONFIGURE     "Configure Temperature"
#define D_TEMPERATURE_REMOTE        "Temperature remote sensor"
#define D_TEMPERATURE_TOPIC         "MQTT Topic"
#define D_TEMPERATURE_KEY           "MQTT JSON Key"

// temperature commands
enum TemperatureCommands { CMND_TEMPERATURE_TOPIC, CMND_TEMPERATURE_KEY };
const char kTemperatureCommands[] PROGMEM = D_CMND_TEMPERATURE_TOPIC "|" D_CMND_TEMPERATURE_KEY;

// form topic style
const char TEMPERATURE_FIELDSET_START[] PROGMEM = "<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>\n";
const char TEMPERATURE_FIELDSET_STOP[] PROGMEM = "</fieldset></p>\n";
const char TEMPERATURE_FIELD_INPUT[] PROGMEM = "<p>%s<span style='float:right;font-size:0.7rem;'>%s</span><br><input name='%s' value='%s'></p>\n";

/*************************************************\
 *               Variables
\*************************************************/

// variables
struct {
  bool     subscribed  = false;        // flag for temperature subscription
  uint32_t last_update = 0;            // last time (in millis) temperature was updated
  float    value       = NAN;          // current temperature
} mqtt_temperature;

/**************************************************\
 *                  Accessors
\**************************************************/

// get current temperature MQTT topic ()
const char* TemperatureGetMqttTopic ()
{
  return SettingsText (SET_TEMPERATURE_TOPIC);
}

// get current temperature JSON key
const char* TemperatureGetMqttKey ()
{
  return SettingsText (SET_TEMPERATURE_KEY);
}

// set current temperature MQTT topic
void TemperatureSetMqttTopic (char* str_topic)
{
  SettingsUpdateText (SET_TEMPERATURE_TOPIC, str_topic);
}

// set current temperature JSON key
void TemperatureSetMqttKey (char* str_key)
{
  SettingsUpdateText (SET_TEMPERATURE_KEY, str_key);
}

/**************************************************\
 *                  Functions
\**************************************************/

// get current temperature
float TemperatureGetValue ()
{
  uint32_t time_over;

  // if no update for 5 minutes, temperature not available
  time_over = mqtt_temperature.last_update + TEMPERATURE_TIMEOUT_5MN;
//  if (TimeReached (time_over)) mqtt_temperature.value = NAN;

  return mqtt_temperature.value;
}

// update temperature
void TemperatureSetValue (float new_temperature)
{
  // set current temperature
  mqtt_temperature.value = new_temperature;

  // temperature updated
  mqtt_temperature.last_update = millis ();
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
    if (append) ResponseAppend_P (PSTR (",")); else Response_P (PSTR ("{"));

    // temperature  -->  "Temperature":{"Value":18.5,"Topic":"mqtt/topic/of/temperature","Key":"Temp"}
    ResponseAppend_P (PSTR ("\"%s\":{"), D_JSON_TEMPERATURE);
    ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &temperature);
    ResponseAppend_P (PSTR ("\"%s\":%s,\"%s\":\"%s\",\"%s\":\"%s\""), D_JSON_TEMPERATURE_VALUE, str_value, D_JSON_TEMPERATURE_TOPIC, TemperatureGetMqttTopic (), D_JSON_TEMPERATURE_KEY, TemperatureGetMqttKey ());
    ResponseAppend_P (PSTR ("}"));

    // publish it if not in append mode
    if (!append)
    {
      ResponseAppend_P (PSTR ("}"));
      MqttPublishPrefixTopic_P (TELE, PSTR (D_RSLT_SENSOR));
    } 
  }
}

// Handle Temperature MQTT commands
bool TemperatureMqttCommand ()
{
  bool command_handled = true;
  int  command_code;
  char command [CMDSZ];

  // check MQTT command
  command_code = GetCommandCode (command, sizeof (command), XdrvMailbox.topic, kTemperatureCommands);

  // handle command
  switch (command_code)
  {
    case CMND_TEMPERATURE_TOPIC:  // set mqtt temperature topic 
      TemperatureSetMqttTopic (XdrvMailbox.data);
      break;
    case CMND_TEMPERATURE_KEY:  // set mqtt temperature key 
      TemperatureSetMqttKey (XdrvMailbox.data);
      break;
    default:
      command_handled = false;
  }

  // if needed, send updated status
  if (command_handled == true) TemperatureShowJSON (false);
  
  return command_handled;
}

// MQTT connexion update
void TemperatureCheckMqttConnexion ()
{
  bool        is_connected;
  const char* pstr_topic;

  // if topic defined, check MQTT connexion
  pstr_topic = TemperatureGetMqttTopic ();
  if (strlen (pstr_topic) > 0)
  {
    // if connected to MQTT server
    is_connected = MqttIsConnected();
    if (is_connected)
    {
      // if still no subsciption to temperature topic
      if (mqtt_temperature.subscribed == false)
      {
        // subscribe to temperature meter
        MqttSubscribe (pstr_topic);

        // subscription done
        mqtt_temperature.subscribed = true;

        // log
        AddLog (LOG_LEVEL_INFO, PSTR ("MQT: Subscribed to %s"), pstr_topic);
      }
    }

    // else disconnected : topic not subscribed
    else mqtt_temperature.subscribed = false;
  }
}

// read received MQTT data to retrieve temperature
bool TemperatureMqttData ()
{
  bool        data_handled = false;
  float       temperature = -100;
  const char* pstr_topic;
  const char* pstr_key;
  char*       pstr_result = nullptr;
  char*       pstr_value  = nullptr;
  char        str_buffer[64];

  // if MQTT subscription is active
  if (mqtt_temperature.subscribed)
  {
    // if topic is the right one
    pstr_topic = TemperatureGetMqttTopic ();
    if (strcmp (pstr_topic, XdrvMailbox.topic) == 0)
    {
      // look for temperature key
      pstr_key = TemperatureGetMqttKey ();
      sprintf (str_buffer, "\"%s\"", pstr_key);
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
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// Temperature MQTT setting web page
void TemperatureWebPage ()
{
  const char* pstr_text;
  char        str_argument[64];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess ()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg ("save"))
  {
    // set MQTT topic according to 'ttopic' parameter
    WebGetArg (D_CMND_TEMPERATURE_TOPIC, str_argument, sizeof (str_argument));
    TemperatureSetMqttTopic (str_argument);

    // set JSON key according to 'tkey' parameter
    WebGetArg (D_CMND_TEMPERATURE_KEY, str_argument, sizeof (str_argument));
    TemperatureSetMqttKey (str_argument);
  }

  // beginning of form
  WSContentStart_P (D_TEMPERATURE_CONFIGURE);
  WSContentSendStyle ();
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_PAGE_TEMPERATURE);

  // remote sensor section  
  // --------------
  WSContentSend_P (TEMPERATURE_FIELDSET_START, D_TEMPERATURE_REMOTE);

  // remote sensor mqtt topic
  pstr_text = TemperatureGetMqttTopic ();
  WSContentSend_P (TEMPERATURE_FIELD_INPUT, D_TEMPERATURE_TOPIC, D_CMND_TEMPERATURE_TOPIC, D_CMND_TEMPERATURE_TOPIC, pstr_text);

  // remote sensor json key
  pstr_text = TemperatureGetMqttKey ();
  WSContentSend_P (TEMPERATURE_FIELD_INPUT, D_TEMPERATURE_KEY, D_CMND_TEMPERATURE_KEY, D_CMND_TEMPERATURE_KEY, pstr_text);
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
   case FUNC_MQTT_INIT:
      TemperatureCheckMqttConnexion ();
      break;
   case FUNC_MQTT_DATA:
      result = TemperatureMqttData ();
      break;
    case FUNC_COMMAND:
      result = TemperatureMqttCommand ();
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
