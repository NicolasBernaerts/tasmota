/*
  xdrv_97_humidity.ino - Retreive Humidity thru MQTT
  
  Copyright (C) 2020  Nicolas Bernaerts
    15/03/2020 - v1.0 - Creation
                      
  Settings are stored using weighting scale parameters :
    - Settings.free_f03 = Humidity MQTT topic;Humidity JSON key

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
 *                    Humidity
\*************************************************/

#ifdef USE_HUMIDITY_MQTT

#define XDRV_97                     97
#define XSNS_97                     97

#define HUMIDITY_BUFFER_SIZE     128
#define HUMIDITY_TIMEOUT_5MN     300000

#define D_PAGE_HUMIDITY          "humidity"

#define D_CMND_HUMIDITY_TOPIC    "htopic"
#define D_CMND_HUMIDITY_KEY      "hkey"

#define D_JSON_HUMIDITY_VALUE    "State"
#define D_JSON_HUMIDITY_TOPIC    "Topic"
#define D_JSON_HUMIDITY_KEY      "Key"

#define D_HUMIDITY_CONFIGURE     "Remote Humidity Sensor"
#define D_HUMIDITY_REMOTE        "Humidity remote sensor"
#define D_HUMIDITY_TOPIC         "MQTT Topic"
#define D_HUMIDITY_KEY           "MQTT JSON Key"

// humidity commands
enum HumidityCommands { CMND_HUMIDITY_TOPIC, CMND_HUMIDITY_KEY };
const char kHumidityCommands[] PROGMEM = D_CMND_HUMIDITY_TOPIC "|" D_CMND_HUMIDITY_KEY;

// form topic style
const char HUMIDITY_TOPIC_STYLE[] PROGMEM = "style='float:right;font-size:0.7rem;'";

/*************************************************\
 *               Variables
\*************************************************/

// variables
float    humidity_mqtt_value       = NAN;          // current humidity
bool     humidity_topic_subscribed = false;        // flag for humidity subscription
uint32_t humidity_last_update      = 0;            // last time (in millis) humidity was updated

/**************************************************\
 *                  Accessors
\**************************************************/

// get current humidity MQTT topic (humidity topic;humidity key)
const char* HumidityGetMqttTopic ()
{
  int    index;
  String str_setting, str_result;

  // extract humidity topic from settings
  str_setting = (char*)Settings.free_f03;
  index = str_setting.indexOf (';');
  if (index != -1) str_result = str_setting.substring (0, index);

  return str_result.c_str ();
}

// get current humidity JSON key (humidity topic;humidity key)
const char* HumidityGetMqttKey ()
{
  int    index;
  String str_setting, str_result;

  // extract temperature topic from settings
  str_setting = (char*)Settings.free_f03;
  index = str_setting.indexOf (';');
  if (index != -1) str_result = str_setting.substring (index + 1);

  return str_result.c_str ();
}

// set current humidity MQTT topic (humidity topic;humidity key)
void HumiditySetMqttTopic (char* str_topic)
{
  int    index;
  String str_setting;

  // extract humidity topic from settings
  str_setting = (char*)Settings.free_f03;
  index = str_setting.indexOf (';');
  if (index != -1) str_setting = str_topic + str_setting.substring (index);
  else str_setting = str_topic + String (";");

  // save the full settings
  strncpy ((char*)Settings.free_f03, str_setting.c_str (), 233);
}

// set current humidity JSON key (humidity topic;humidity key)
void HumiditySetMqttKey (char* str_key)
{
  int    index;
  String str_setting;

  // extract humidity topic from settings
  str_setting = (char*)Settings.free_f03;
  index = str_setting.indexOf (';');
  if (index != -1) str_setting = str_setting.substring (0, index + 1) + str_key;
  else str_setting = String (";") + str_key;

  // save the full settings
  strncpy ((char*)Settings.free_f03, str_setting.c_str (), 233);
}

/**************************************************\
 *                  Functions
\**************************************************/

// get current humidity
float HumidityGetValue ( )
{
  uint32_t time_now = millis ();

  // if no update for 5 minutes, humidity not available
  if (time_now - humidity_last_update > HUMIDITY_TIMEOUT_5MN) humidity_mqtt_value = NAN;

  return humidity_mqtt_value;
}

// update humidity
void HumiditySetValue (float new_humidity)
{
  // update current humidity
  humidity_mqtt_value = new_humidity;

  // humidity updated
  humidity_last_update = millis ();
}

// generate JSON status (for MQTT)
void HumidityShowJSON (bool append)
{
  String str_mqtt, str_json, str_topic, str_key;
  float  humidity;

  // read data
  str_topic = HumidityGetMqttTopic ();
  str_key   = HumidityGetMqttKey ();
  humidity  = HumidityGetValue ();

  // humidity  -->  "Humidity":{"Value":18.5,"Topic":"mqtt/topic/of/temperature","Key":"Temp"}
  str_json =  "\"" + String (D_JSON_HUMIDITY) + "\":{";
  str_json += "\"" + String (D_JSON_HUMIDITY_VALUE) + "\":" + String (humidity, 1) + ",";
  str_json += "\"" + String (D_JSON_HUMIDITY_TOPIC) + "\":\"" + str_topic + "\",";
  str_json += "\"" + String (D_JSON_HUMIDITY_KEY) + "\":\"" + str_key + "\"}";

  // generate MQTT message according to publish state and publish if needed
  if (append == false) 
  {
    str_mqtt = "{" + str_json + "}";
    snprintf_P(mqtt_data, sizeof(mqtt_data), str_mqtt.c_str());
    MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));
  }
  else
  {
    str_mqtt = String (mqtt_data) + "," + str_json;
    snprintf_P(mqtt_data, sizeof(mqtt_data), str_mqtt.c_str());
  }
}

// Handle humidity MQTT commands
bool HumidityMqttCommand ()
{
  bool command_handled = true;
  int  command_code;
  char command [CMDSZ];

  // check MQTT command
  command_code = GetCommandCode (command, sizeof(command), XdrvMailbox.topic, kHumidityCommands);

  // handle command
  switch (command_code)
  {
    case CMND_HUMIDITY_TOPIC:  // set mqtt humidity topic 
      HumiditySetMqttTopic (XdrvMailbox.data);
      break;
    case CMND_HUMIDITY_KEY:  // set mqtt humidity key 
      HumiditySetMqttKey (XdrvMailbox.data);
      break;
    default:
      command_handled = false;
  }

  // if needed, send updated status
  if (command_handled == true) HumidityShowJSON (false);
  
  return command_handled;
}

// MQTT connexion update
void HumidityCheckMqttConnexion ()
{
  bool   is_connected;
  String str_topic;

  // get temperature MQTT topic
  str_topic = HumidityGetMqttTopic ();

  // if topic defined, check MQTT connexion
  if (str_topic.length () > 0)
  {
    // if connected to MQTT server
    is_connected = MqttIsConnected();
    if (is_connected)
    {
      // if still no subsciption to humidity topic
      if (humidity_topic_subscribed == false)
      {
        // subscribe to humidity meter
        MqttSubscribe(str_topic.c_str ());

        // subscription done
        humidity_topic_subscribed = true;

        // log
        AddLog_P2(LOG_LEVEL_INFO, PSTR("MQT: Subscribed to %s"), str_topic.c_str ());
      }
    }

    // else disconnected : topic not subscribed
    else humidity_topic_subscribed = false;
  }
}

// read received MQTT data to retrieve humidity
bool HumidityMqttData ()
{
  bool    data_handled = false;
  int     idx_value;
  String  str_topic, str_key;
  String  str_mailbox_topic, str_mailbox_data, str_mailbox_value;

  // get topics to compare
  str_mailbox_topic = XdrvMailbox.topic;
  str_key   = HumidityGetMqttKey ();
  str_topic = HumidityGetMqttTopic ();

  // get humidity (removing SPACE and QUOTE)
  str_mailbox_data  = XdrvMailbox.data;
  str_mailbox_data.replace (" ", "");
  str_mailbox_data.replace ("\"", "");

  // if topic is the humidity
  if (str_mailbox_topic.compareTo(str_topic) == 0)
  {
    // if a humidity key is defined, find the value in the JSON chain
    if (str_key.length () > 0)
    {
      str_key += ":";
      idx_value = str_mailbox_data.indexOf (str_key);
      if (idx_value >= 0) idx_value = str_mailbox_data.indexOf (':', idx_value + 1);
      if (idx_value >= 0) str_mailbox_value = str_mailbox_data.substring (idx_value + 1);
    }

    // else, no humidity key provided, data holds the value
    else str_mailbox_value = str_mailbox_data;

    // convert and update humidity
    HumiditySetValue (str_mailbox_value.toFloat ());

    // data from message has been handled
    data_handled = true;
  }

  return data_handled;
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// humidity configuration button
void HumidityWebButton ()
{
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>"), D_PAGE_HUMIDITY, D_HUMIDITY_CONFIGURE);
}

// Humidity MQTT setting web page
void HumidityWebPage ()
{
  bool   state_pullup;
  char   argument[HUMIDITY_BUFFER_SIZE];
  String str_topic, str_key, str_pullup;

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // page comes from save button on configuration page
  if (WebServer->hasArg("save"))
  {
    // set MQTT topic according to 'htopic' parameter
    WebGetArg (D_CMND_HUMIDITY_TOPIC, argument, HUMIDITY_BUFFER_SIZE);
    HumiditySetMqttTopic (argument);

    // set JSON key according to 'hkey' parameter
    WebGetArg (D_CMND_HUMIDITY_KEY, argument, HUMIDITY_BUFFER_SIZE);
    HumiditySetMqttKey (argument);
  }

  // beginning of form
  WSContentStart_P (D_HUMIDITY_CONFIGURE);
  WSContentSendStyle ();
  WSContentSend_P (PSTR("<form method='get' action='%s'>\n"), D_PAGE_HUMIDITY);

  // remote sensor section  
  // --------------
  WSContentSend_P (PSTR("<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>\n"), D_HUMIDITY_REMOTE);

  // remote sensor mqtt topic
  str_topic = HumidityGetMqttTopic ();
  WSContentSend_P (PSTR ("<p>%s<span %s>%s</span><br><input name='%s' value='%s'></p>\n"), D_HUMIDITY_TOPIC, HUMIDITY_TOPIC_STYLE, D_CMND_HUMIDITY_TOPIC, D_CMND_HUMIDITY_TOPIC, str_topic.c_str ());

  // remote sensor json key
  str_key = HumidityGetMqttKey ();
  WSContentSend_P (PSTR ("<p>%s<span %s>%s</span><br/><input name='%s' value='%s'><br/>\n"), D_HUMIDITY_KEY, HUMIDITY_TOPIC_STYLE, D_CMND_HUMIDITY_KEY, D_CMND_HUMIDITY_KEY, str_key.c_str ());
  WSContentSend_P (PSTR("</fieldset></p>\n"));

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
      HumidityCheckMqttConnexion ();
      break;
   case FUNC_MQTT_DATA:
      result = HumidityMqttData ();
      break;
    case FUNC_COMMAND:
      result = HumidityMqttCommand ();
      break;
    case FUNC_EVERY_SECOND:
      HumidityCheckMqttConnexion ();
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
      WebServer->on ("/" D_PAGE_HUMIDITY, HumidityWebPage);
      break;
    case FUNC_WEB_ADD_BUTTON:
      HumidityWebButton ();
      break;
#endif  // USE_WEBSERVER
  }
  
  return result;
}

#endif // USE_HUMIDITY
