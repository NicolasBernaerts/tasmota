/*
  xdrv_97_temperature.ino - Retreive Temperature thru MQTT (~2.5 kb)
  
  Copyright (C) 2020  Nicolas Bernaerts
    15/01/2020 - v1.0 - Creation with management of remote MQTT sensor
                      
  Settings are stored using weighting scale parameters :
    - Settings.free_f03  = MQTT Instant temperature (power data|Temperature MQTT topic;Temperature JSON key)

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

#define TEMPERATURE_BUFFER_SIZE     128
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
const char str_temp_fieldset_start[] PROGMEM = "<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>\n";
const char str_temp_fieldset_stop[] PROGMEM = "</fieldset></p>\n";
const char str_temp_input[] PROGMEM = "<p>%s<span style='float:right;font-size:0.7rem;'>%s</span><br><input name='%s' value='%s'></p>\n";

/*************************************************\
 *               Variables
\*************************************************/

// variables
float    temperature_mqtt_value       = NAN;          // current temperature
bool     temperature_topic_subscribed = false;        // flag for temperature subscription
uint32_t temperature_last_update      = 0;            // last time (in millis) temperature was updated

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
float TemperatureGetValue ( )
{
  uint32_t time_now = millis ();

  // if no update for 5 minutes, temperature not available
  if (time_now - temperature_last_update > TEMPERATURE_TIMEOUT_5MN) temperature_mqtt_value = NAN;

  return temperature_mqtt_value;
}

// update temperature
void TemperatureSetValue (float new_temperature)
{
  // set current temperature
  temperature_mqtt_value = new_temperature;

  // temperature updated
  temperature_last_update = millis ();
}

// Show JSON status (for MQTT)
void TemperatureShowJSON (bool append)
{
  String str_json, str_topic, str_key;
  float  temperature;

  // if temperature is available
  temperature = TemperatureGetValue ();
  if (!isnan (temperature))
  {
    // read data
    str_topic = TemperatureGetMqttTopic ();
    str_key   = TemperatureGetMqttKey ();

    // temperature  -->  "Temperature":{"Value":18.5,"Topic":"mqtt/topic/of/temperature","Key":"Temp"}
    str_json  = "\"" + String (D_JSON_TEMPERATURE) + "\":{";
    str_json += "\"" + String (D_JSON_TEMPERATURE_VALUE) + "\":" + String (temperature,1) + ",";
    str_json += "\"" + String (D_JSON_TEMPERATURE_TOPIC) + "\":\"" + str_topic + "\",";
    str_json += "\"" + String (D_JSON_TEMPERATURE_KEY) + "\":\"" + str_key + "\"}";

    // if append mode, add json string to MQTT message
    if (append) ResponseAppend_P (PSTR(",%s"), str_json.c_str ());
    else Response_P (PSTR("{%s}"), str_json.c_str ());

    // if not in append mode, publish message 
    if (!append) MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));
  }
}

// Handle Temperature MQTT commands
bool TemperatureMqttCommand ()
{
  bool command_handled = true;
  int  command_code;
  char command [CMDSZ];

  // check MQTT command
  command_code = GetCommandCode (command, sizeof(command), XdrvMailbox.topic, kTemperatureCommands);

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
  bool   is_connected;
  String str_topic;

  // if topic defined, check MQTT connexion
  str_topic = TemperatureGetMqttTopic ();
  if (str_topic.length () > 0)
  {
    // if connected to MQTT server
    is_connected = MqttIsConnected();
    if (is_connected)
    {
      // if still no subsciption to temperature topic
      if (temperature_topic_subscribed == false)
      {
        // subscribe to temperature meter
        MqttSubscribe(str_topic.c_str ());

        // subscription done
        temperature_topic_subscribed = true;

        // log
        AddLog_P2(LOG_LEVEL_INFO, PSTR("MQT: Subscribed to %s"), str_topic.c_str ());
      }
    }

    // else disconnected : topic not subscribed
    else temperature_topic_subscribed = false;
  }
}

// read received MQTT data to retrieve temperature
bool TemperatureMqttData ()
{
  bool    data_handled = false;
  int     idx_value;
  String  str_topic, str_key;
  String  str_mailbox_topic, str_mailbox_data, str_mailbox_value;

  // if MQTT subscription is active
  if (temperature_topic_subscribed == true)
  {
    // get topics to compare
    str_mailbox_topic = XdrvMailbox.topic;
    str_key   = TemperatureGetMqttKey ();
    str_topic = TemperatureGetMqttTopic ();

    // get temperature (removing SPACE and QUOTE)
    str_mailbox_data  = XdrvMailbox.data;
    str_mailbox_data.replace (" ", "");
    str_mailbox_data.replace ("\"", "");

    // if topic is the temperature
    if (str_mailbox_topic.compareTo(str_topic) == 0)
    {
      // if a temperature key is defined, find the value in the JSON chain
      if (str_key.length () > 0)
      {
        str_key += ":";
      idx_value = str_mailbox_data.indexOf (str_key);
        if (idx_value >= 0) idx_value = str_mailbox_data.indexOf (':', idx_value + 1);
        if (idx_value >= 0) str_mailbox_value = str_mailbox_data.substring (idx_value + 1);
      }

      // else, no temperature key provided, data holds the value
      else str_mailbox_value = str_mailbox_data;

      // convert and update temperature
      TemperatureSetValue (str_mailbox_value.toFloat ());

      // data from message has been handled
      data_handled = true;
    }
  }

  return data_handled;
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// Temperature configuration button
void TemperatureWebButton ()
{
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>"), D_PAGE_TEMPERATURE, D_TEMPERATURE_CONFIGURE);
}

// Temperature MQTT setting web page
void TemperatureWebPage ()
{
  bool   state_pullup;
  char   argument[TEMPERATURE_BUFFER_SIZE];
  String str_topic, str_key, str_pullup;

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg("save"))
  {
    // set MQTT topic according to 'ttopic' parameter
    WebGetArg (D_CMND_TEMPERATURE_TOPIC, argument, TEMPERATURE_BUFFER_SIZE);
    TemperatureSetMqttTopic (argument);

    // set JSON key according to 'tkey' parameter
    WebGetArg (D_CMND_TEMPERATURE_KEY, argument, TEMPERATURE_BUFFER_SIZE);
    TemperatureSetMqttKey (argument);
  }

  // beginning of form
  WSContentStart_P (D_TEMPERATURE_CONFIGURE);
  WSContentSendStyle ();
  WSContentSend_P (PSTR("<form method='get' action='%s'>\n"), D_PAGE_TEMPERATURE);

  // remote sensor section  
  // --------------
  WSContentSend_P (str_temp_fieldset_start, D_TEMPERATURE_REMOTE);

  // remote sensor mqtt topic
  str_topic = TemperatureGetMqttTopic ();
  WSContentSend_P (str_temp_input, D_TEMPERATURE_TOPIC, D_CMND_TEMPERATURE_TOPIC, D_CMND_TEMPERATURE_TOPIC, str_topic.c_str ());

  // remote sensor json key
  str_key = TemperatureGetMqttKey ();
  WSContentSend_P (str_temp_input, D_TEMPERATURE_KEY, D_CMND_TEMPERATURE_KEY, D_CMND_TEMPERATURE_KEY, str_key.c_str ());
  WSContentSend_P (str_temp_fieldset_stop);

  // end of form and save button
  WSContentSend_P (PSTR ("<p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), D_SAVE);
  WSContentSend_P (PSTR("</form>\n"));

  // configuration button
  WSContentSpaceButton(BUTTON_CONFIGURATION);

  // end of page
  WSContentStop();
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
      TemperatureWebButton ();
      break;
#endif  // USE_WEBSERVER
  }
  
  return result;
}

#endif // USE_TEMPERATURE
