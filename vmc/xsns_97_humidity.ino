/*
  xdrv_95_humidity.ino - Retreive Humidity thru MQTT
  
  Copyright (C) 2020  Nicolas Bernaerts
    15/03/2020 - v1.0 - Creation
    17/09/2020 - v1.1 - Adaptation for Tasmota 8.4
    19/09/2020 - v1.2 - Switch to sensor only
    01/05/2021 - v1.3 - Remove use of String to avoid heap fragmentation 
                      
  Settings are stored using Settings Text

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

#define XSNS_97                  97

#define HUMIDITY_TIMEOUT_5MN     300000

#define D_PAGE_HUMIDITY          "humidity"

#define D_CMND_HUMIDITY_TOPIC    "htopic"
#define D_CMND_HUMIDITY_KEY      "hkey"

#define D_JSON_HUMIDITY_VALUE    "State"
#define D_JSON_HUMIDITY_TOPIC    "Topic"
#define D_JSON_HUMIDITY_KEY      "Key"

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
struct {
  bool     subscribed  = false;        // flag for humidity subscription
  uint32_t last_update = 0;            // last time (in millis) humidity was updated
  float    value       = NAN;          // current humidity
} mqtt_humidity;


/**************************************************\
 *                  Accessors
\**************************************************/

// get current humidity MQTT topic (humidity topic;humidity key)
char* HumidityGetMqttTopic ()
{
  return SettingsText (SET_HUMIDITY_TOPIC);
}

// get current humidity JSON key (humidity topic;humidity key)
char* HumidityGetMqttKey ()
{
  return SettingsText (SET_HUMIDITY_KEY);
}

// set current humidity MQTT topic (humidity topic;humidity key)
void HumiditySetMqttTopic (char *str_topic)
{
  SettingsUpdateText (SET_HUMIDITY_TOPIC, str_topic);
}

// set current humidity JSON key (humidity topic;humidity key)
void HumiditySetMqttKey (char *str_key)
{
  SettingsUpdateText (SET_HUMIDITY_KEY, str_key);
}

/**************************************************\
 *                  Functions
\**************************************************/

// get current humidity
float HumidityGetValue ( )
{
  uint32_t time_now = millis ();

  // if no update for 5 minutes, humidity not available
  if (time_now - mqtt_humidity.last_update > HUMIDITY_TIMEOUT_5MN) mqtt_humidity.value = NAN;

  return mqtt_humidity.value;
}

// update humidity
void HumiditySetValue (float new_humidity)
{
  // update current humidity
  mqtt_humidity.value = new_humidity;

  // humidity updated
  mqtt_humidity.last_update = millis ();
}

// generate JSON status (for MQTT)
void HumidityShowJSON (bool append)
{
  float value;
  char  *pstr_topic, *pstr_key;
  char  str_text[8];

  // add , in append mode or { in direct publish mode
  if (append) ResponseAppend_P (PSTR (",")); else Response_P (PSTR ("{"));

  // generate string :   "Humidity":{"Value":18.5,"Topic":"mqtt/topic/of/humidity","Key":"mqtt key"}
  ResponseAppend_P (PSTR ("\"%s\":{"), D_JSON_HUMIDITY);

  // humidity value
  value = HumidityGetValue ();
  ext_snprintf_P (str_text, sizeof(str_text), PSTR ("%1_f"), &value);
  ResponseAppend_P (PSTR ("\"%s\":%s"), D_JSON_HUMIDITY_VALUE, str_text);

  // humidity MQTT topic
  pstr_topic = HumidityGetMqttTopic ();
  ResponseAppend_P (PSTR ("\"%s\":\"%s\""), D_JSON_HUMIDITY_TOPIC, pstr_topic);

  // humidity MQTT key
  pstr_key = HumidityGetMqttKey ();
  ResponseAppend_P (PSTR ("\"%s\":\"%s\""), D_JSON_HUMIDITY_KEY, pstr_key);

  ResponseAppend_P (PSTR ("}"));

  // publish it if not in append mode
  if (!append)
  {
    ResponseAppend_P (PSTR ("}"));
    MqttPublishPrefixTopic_P (TELE, PSTR (D_RSLT_SENSOR));
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
  char *pstr_topic;

  // if topic defined, check MQTT connexion
  pstr_topic = HumidityGetMqttTopic ();
  if (strlen (pstr_topic) > 0)
  {
    // if connected to MQTT server
    if (MqttIsConnected ())
    {
      // if still no subsciption to humidity topic
      if (mqtt_humidity.subscribed == false)
      {
        // subscribe to humidity meter
        mqtt_humidity.subscribed = true;
        MqttSubscribe (pstr_topic);

        // log
        AddLog (LOG_LEVEL_INFO, PSTR ("MQT: Subscribed to %s"), pstr_topic);
      }
    }

    // else disconnected : topic not subscribed
    else mqtt_humidity.subscribed = false;
  }
}

// read received MQTT data to retrieve humidity
bool HumidityMqttData ()
{
  bool       data_handled = false;
  float      humidity;
  const char *pstr_topic, *pstr_key;
  char       *pstr_result;
  char       str_buffer[64];

  // if MQTT subscription is active
  if (mqtt_humidity.subscribed)
  {
    // look for humidity topic
    pstr_topic = HumidityGetMqttTopic ();
    if (strcmp (pstr_topic, XdrvMailbox.topic) == 0)
    {
      // log and counter increment
      AddLog (LOG_LEVEL_INFO, PSTR ("MQT: Received %s"), pstr_topic);

      // look for humidity key
      pstr_key = HumidityGetMqttKey ();
      sprintf (str_buffer, "\"%s\":", pstr_key);
      pstr_result = strstr (XdrvMailbox.data, str_buffer);
      if (pstr_result != nullptr) humidity = atof (pstr_result + strlen (str_buffer));
      data_handled |= (pstr_result != nullptr);

      // update current humidity
      HumiditySetValue (humidity);
    }
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
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>"), D_PAGE_HUMIDITY, D_HUMIDITY_REMOTE);
}

// append Humidity sensor to main page
void HumidityWebSensor ()
{
  float humidity;
  char  str_humidity[8];

  // read humidity
  humidity = HumidityGetValue ();
  if (!isnan(humidity))
  {
    ext_snprintf_P (str_humidity, sizeof (str_humidity), PSTR ("%1_f"), &humidity);
    WSContentSend_PD (PSTR("{s}%s{m}%s{e}"), D_HUMIDITY_REMOTE, str_humidity);
  }
}

// Humidity MQTT setting web page
void HumidityWebPage ()
{
  char  str_argument[64];
  char  *pstr_topic, *pstr_key;

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg("save"))
  {
    // set MQTT topic according to 'htopic' parameter
    WebGetArg (D_CMND_HUMIDITY_TOPIC, str_argument, sizeof (str_argument));
    HumiditySetMqttTopic (str_argument);

    // set JSON key according to 'hkey' parameter
    WebGetArg (D_CMND_HUMIDITY_KEY, str_argument, sizeof (str_argument));
    HumiditySetMqttKey (str_argument);
  }

  // beginning of form
  WSContentStart_P (D_HUMIDITY);
  WSContentSendStyle ();
  WSContentSend_P (PSTR("<form method='get' action='%s'>\n"), D_PAGE_HUMIDITY);

  // remote sensor section  
  // --------------
  WSContentSend_P (PSTR("<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>\n"), D_HUMIDITY_REMOTE);

  // remote sensor mqtt topic
  pstr_topic = HumidityGetMqttTopic ();
  WSContentSend_P (PSTR ("<p>%s<span %s>%s</span><br><input name='%s' value='%s'></p>\n"), D_HUMIDITY_TOPIC, HUMIDITY_TOPIC_STYLE, D_CMND_HUMIDITY_TOPIC, D_CMND_HUMIDITY_TOPIC, pstr_topic);

  // remote sensor json key
  pstr_key = HumidityGetMqttKey ();
  WSContentSend_P (PSTR ("<p>%s<span %s>%s</span><br/><input name='%s' value='%s'><br/>\n"), D_HUMIDITY_KEY, HUMIDITY_TOPIC_STYLE, D_CMND_HUMIDITY_KEY, D_CMND_HUMIDITY_KEY, pstr_key);
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

bool Xsns97 (uint8_t function)
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

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      Webserver->on ("/" D_PAGE_HUMIDITY, HumidityWebPage);
      break;
    case FUNC_WEB_ADD_BUTTON:
      HumidityWebButton ();
      break;
#endif  // USE_WEBSERVER
  }
  
  return result;
}

#endif // USE_HUMIDITY
