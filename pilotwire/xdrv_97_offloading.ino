/*
  xdrv_98_pilotwire.ino - French Pilot Wire (Fil Pilote) support 
  for Sonoff Basic or Sonoff Dual R2
  
  Copyright (C) 2019  Nicolas Bernaerts
    05/07/2019 - v3.0 - Add max power management with automatic offload
                        Save power settings in Settings.energy... variables
    30/12/2019 - v4.0 - Switch settings to free_f03 for Tasmota 8.x compatibility
    06/01/2019 - v4.1 - Handle offloading with finite state machine
    09/01/2019 - v4.2 - Separation between Offloading driver and Pilotwire sensor
                      
  Settings are stored using weighting scale parameters :
    - Settings.energy_max_power              = Heater power (W) 
    - Settings.energy_max_power_limit        = Maximum power of the house contract (W) 
    - Settings.energy_max_power_limit_window = Number of overload messages before offload
    - Settings.energy_max_power_limit_hold   = Number of back to naormal messages before removing offload
    - Settings.free_f03                      = Total power (MQTT topic;JSON key)

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
 *              Fil Pilote
\*************************************************/

#ifdef USE_OFFLOADING

#define XDRV_97               97
#define XSNS_97               97

#define OFFLOADING_BUFFER_SIZE           128

#define D_PAGE_OFFLOADING_METER          "meter"

#define D_CMND_OFFLOADING_BEFORE    "before"
#define D_CMND_OFFLOADING_AFTER     "after"
#define D_CMND_OFFLOADING_POWER     "power"
#define D_CMND_OFFLOADING_CONTRACT  "contract"
#define D_CMND_OFFLOADING_TOPIC     "topic"
#define D_CMND_OFFLOADING_KEY       "key"

#define D_JSON_OFFLOADING           "Offload"
#define D_JSON_OFFLOADING_STATE     "State"
#define D_JSON_OFFLOADING_BEFORE    "Before"
#define D_JSON_OFFLOADING_AFTER     "After"
#define D_JSON_OFFLOADING_CONTRACT  "Contract"
#define D_JSON_OFFLOADING_POWER     "Power"
#define D_JSON_OFFLOADING_TOPIC     "Topic"
#define D_JSON_OFFLOADING_KEY       "Key"

const char OFFLOADING_FORM_START[] PROGMEM = "<form method='get' action='%s'>";
const char OFFLOADING_FORM_STOP[] PROGMEM = "</form>";
const char OFFLOADING_FIELDSET_START[] PROGMEM = "<fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>";
const char OFFLOADING_FIELDSET_STOP[] PROGMEM = "</fieldset><br />";

// offloading commands
enum OffloadingCommands { CMND_OFFLOADING_POWER, CMND_OFFLOADING_CONTRACT, CMND_OFFLOADING_BEFORE, CMND_OFFLOADING_AFTER, CMND_OFFLOADING_TOPIC, CMND_OFFLOADING_KEY };
const char kOffloadingCommands[] PROGMEM = D_CMND_OFFLOADING_POWER "|" D_CMND_OFFLOADING_CONTRACT "|" D_CMND_OFFLOADING_BEFORE "|" D_CMND_OFFLOADING_AFTER "|" D_CMND_OFFLOADING_TOPIC "|" D_CMND_OFFLOADING_KEY;

/*************************************************\
 *               Variables
\*************************************************/

// offloading stages
enum OffloadingStages { OFFLOADING_NONE, OFFLOADING_BEFORE, OFFLOADING_ACTIVE, OFFLOADING_AFTER };

// variables
uint8_t  offloading_state            = OFFLOADING_NONE;    // current offloading state
uint16_t offloading_counter          = 0;                  // message counter before or after offloading
uint16_t offloading_house_power      = 0;                  // last total house power retrieved thru MQTT
bool     offloading_topic_subscribed = false;              // flag for power subscription

/**************************************************\
 *                  Accessors
\**************************************************/

// get offload state
bool OffloadingIsOffloaded ()
{
  return ((offloading_state == OFFLOADING_ACTIVE) || (offloading_state == OFFLOADING_AFTER));
}

// get maximum power limit of house contract
uint16_t OffloadingGetContract ()
{
  return Settings.energy_max_power_limit;
}

// set maximum power limit of house contract
void OffloadingSetContract (uint16_t new_power)
{
  Settings.energy_max_power_limit = new_power;
}

// get power of heater
uint16_t OffloadingGetDevicePower ()
{
  return Settings.energy_max_power;
}

// set power of heater
void OffloadingSetDevicePower (uint16_t new_power)
{
  Settings.energy_max_power = new_power;
}

// get number of power update messages to receive before offloading
uint16_t OffloadingGetUpdateBeforeOffload ()
{
  return Settings.energy_max_power_limit_window;;
}

// set number of power update messages to receive before offloading
void OffloadingSetUpdateBeforeOffload (uint16_t number)
{
  Settings.energy_max_power_limit_window = number;
}

// get number of power update messages to receive before removing offload
uint16_t OffloadingGetUpdateAfterOffload ()
{
  return Settings.energy_max_power_limit_hold;;
}

// set number of power update messages to receive before removing offload
void OffloadingSetUpdateAfterOffload (uint16_t number)
{
  Settings.energy_max_power_limit_hold = number;
}

// get current total power MQTT topic
void OffloadingGetMqttHouseTopic (String& str_topic)
{
  String str_meter;
  int position;

  str_meter = (char*)Settings.free_f03;
  position = str_meter.indexOf(';');
  if (position == -1) str_topic = str_meter;
  else str_topic = str_meter.substring(0, position);
}

// get current total power JSON key
void OffloadingGetMqttHouseKey (String& str_key)
{
  String str_meter;
  int position;

  str_meter = (char*)Settings.free_f03;
  position = str_meter.indexOf(';');
  if (position == -1) str_key = "";
  else str_key = str_meter.substring(position +1);
}


// set current total power MQTT topic
void OffloadingSetMqttHouseTopic (char* new_topic)
{
  String str_meter, str_key;

  // get key
  OffloadingGetMqttHouseKey (str_key);

  // save the full meter topic and key
  str_meter = new_topic;
  str_meter += ";";
  str_meter += str_key;
  strncpy ((char*)Settings.free_f03, str_meter.c_str (), 128);
}

// set current total power JSON key
void OffloadingSetMqttHouseKey (char* new_key)
{
  String str_meter, str_topic;

  // get key
  OffloadingGetMqttHouseTopic (str_topic);

  // save the full meter topic and key
  str_meter = str_topic;
  str_meter += ";";
  str_meter += new_key;
  strncpy ((char*)Settings.free_f03, str_meter.c_str (), 128);
}

/**************************************************\
 *                  Functions
\**************************************************/

// Show JSON status (for MQTT)
void OffloadingShowJSON (bool append)
{
  bool     isOffloaded;
  uint16_t contract, power, before, after;
  String   str_json, str_topic, str_key;

  // read data
  isOffloaded = OffloadingIsOffloaded ();
  before   = OffloadingGetUpdateBeforeOffload ();
  after    = OffloadingGetUpdateAfterOffload ();
  power    = OffloadingGetDevicePower ();
  contract = OffloadingGetContract ();
  OffloadingGetMqttHouseTopic (str_topic);
  OffloadingGetMqttHouseKey (str_key);

  // start message  -->  {  or message,
  if (append == false) str_json = "{";
  else str_json = String (mqtt_data) + ",";

  // Offloading  -->  "Offload":{"State":"OFF","Before":1,"After":5,"Power":1000,"Contract":5000,"Topic":"mqtt/topic/of/device","Key":"Power"}
  str_json += "\"" + String (D_JSON_OFFLOADING) + "\":{";
  str_json += "\"" + String (D_JSON_OFFLOADING_STATE) + "\":";
  if (isOffloaded == true) str_json += "\"ON\",";
  else str_json += "\"OFF\",";
  str_json += "\"" + String (D_JSON_OFFLOADING_BEFORE) + "\":" + String (before) + ",";
  str_json += "\"" + String (D_JSON_OFFLOADING_AFTER) + "\":" + String (after) + ",";
  str_json += "\"" + String (D_JSON_OFFLOADING_POWER) + "\":" + String (power) + ",";
  str_json += "\"" + String (D_JSON_OFFLOADING_CONTRACT) + "\":" + String (contract) + ",";
  str_json += "\"" + String (D_JSON_OFFLOADING_TOPIC) + "\":\"" + str_topic + "\",";
  str_json += "\"" + String (D_JSON_OFFLOADING_KEY) + "\":\"" + str_key + "\"}";

  // if not in append mode, add last bracket 
  if (append == false) str_json += "}";

  // add json string to MQTT message
  snprintf_P (mqtt_data, sizeof(mqtt_data), str_json.c_str ());

  // if not in append mode, publish message 
  if (append == false) MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));
}

// Handle offloading MQTT commands
bool OffloadingCommand ()
{
  int  command_code;
  char command [CMDSZ];

  // check MQTT command
  command_code = GetCommandCode (command, sizeof(command), XdrvMailbox.topic, kOffloadingCommands);

  // handle command
  switch (command_code)
  {
    case CMND_OFFLOADING_POWER:  // set device power
      OffloadingSetDevicePower (XdrvMailbox.payload);
      break;
    case CMND_OFFLOADING_CONTRACT:  // set house contract power
      OffloadingSetContract (XdrvMailbox.payload);
      break;
    case CMND_OFFLOADING_TOPIC:  // set mqtt house power topic 
      OffloadingSetMqttHouseTopic (XdrvMailbox.data);
      break;
    case CMND_OFFLOADING_KEY:  // set mqtt house power key 
      OffloadingSetMqttHouseKey (XdrvMailbox.data);
      break;
    case CMND_OFFLOADING_BEFORE:  // set number of updates before offloading 
      OffloadingSetUpdateBeforeOffload (XdrvMailbox.payload);
      break;
    case CMND_OFFLOADING_AFTER:  // set number of updates after offloading 
      OffloadingSetUpdateAfterOffload (XdrvMailbox.payload);
      break;
  }

  // send MQTT status
  OffloadingShowJSON (false);
  
  return true;
}

// update offloading status
void OffloadingUpdateStatus ()
{
  uint8_t  previous_state = offloading_state;
  uint16_t house_contract, heater_power;

  // get MQTT power data
  house_contract = OffloadingGetContract ();
  heater_power   = OffloadingGetDevicePower ();

  // if house contract and heater power are defined
  if ((house_contract > 0) && (heater_power > 0))
  {
    // switch according to current state
    switch (offloading_state)
    { 
      case OFFLOADING_NONE:
        // if overload is detected
        if (offloading_house_power > house_contract)
        {
          // set status to before offloading
          offloading_counter = OffloadingGetUpdateBeforeOffload ();
          if (offloading_counter > 0) offloading_state = OFFLOADING_BEFORE;
          else offloading_state = OFFLOADING_ACTIVE;
        }
        break;
      case OFFLOADING_BEFORE:
          // if house power has gone down, remove offloading countdown
          if (offloading_house_power <= house_contract) offloading_state = OFFLOADING_NONE;
        
          // else, decrement message counter to activate offloading
          else
          {
            offloading_counter--;
            if (offloading_counter == 0) offloading_state = OFFLOADING_ACTIVE;
          }
        break;
      case OFFLOADING_ACTIVE:
        if (offloading_house_power <= house_contract - heater_power)
        {
          // set status to after offloading
          offloading_counter = OffloadingGetUpdateAfterOffload ();
          if (offloading_counter > 0) offloading_state = OFFLOADING_AFTER;
          else offloading_state = OFFLOADING_NONE;
        }
        break;
      case OFFLOADING_AFTER:
        // if house power has gone again too high, offloading back to active state
        if (offloading_house_power > house_contract - heater_power) offloading_state = OFFLOADING_ACTIVE;
        
        // else, decrement message counter to remove offloading
        else
        {
          offloading_counter--;
          if (offloading_counter == 0) offloading_state = OFFLOADING_NONE;
        }
        break;
    }

    // if state change, send MQTT status
    if (previous_state != offloading_state) OffloadingShowJSON (false);
  }

}

// check and update MQTT power subsciption after disconnexion
void OffloadingCheckMqttConnexion ()
{
  bool   is_connected;
  String str_topic;

  // if connected to MQTT server
  is_connected = MqttIsConnected();
  if (is_connected)
  {
    // if still no subsciption to power topic
    if (offloading_topic_subscribed == false)
    {
      // check power topic availability
      OffloadingGetMqttHouseTopic (str_topic);
      if (str_topic.length () > 0) 
      {
        // subscribe to power meter
        MqttSubscribe(str_topic.c_str ());

        // subscription done
        offloading_topic_subscribed = true;

        // log
        AddLog_P2(LOG_LEVEL_INFO, PSTR("MQT: Subscribed to %s"), str_topic.c_str ());
      }
    }
  }
  else offloading_topic_subscribed = false;
}

// update instant house power
void OffloadingUpdateHousePower (uint16_t new_power)
{
  // update instant power
  offloading_house_power = new_power;

  // update offloading status according to new house instant power
  OffloadingUpdateStatus ();
}

// read received MQTT data to retrieve house instant power
bool OffloadingMqttData ()
{
  bool    data_handled = false;
  int     idx_value;
  String  str_power_topic, str_power_key;
  String  str_mailbox_topic, str_mailbox_data, str_mailbox_value;

  // get topics to compare
  str_mailbox_topic = XdrvMailbox.topic;
  OffloadingGetMqttHouseKey (str_power_key);
  OffloadingGetMqttHouseTopic (str_power_topic);

  // get power data (removing SPACE and QUOTE)
  str_mailbox_data  = XdrvMailbox.data;
  str_mailbox_data.replace (" ", "");
  str_mailbox_data.replace ("\"", "");

  // if topic is the instant house power
  if (str_mailbox_topic.compareTo(str_power_topic) == 0)
  {
    // if a power key is defined, find the value in the JSON chain
    if (str_power_key.length () > 0)
    {
      str_power_key += ":";
      idx_value = str_mailbox_data.indexOf (str_power_key);
      if (idx_value >= 0) idx_value = str_mailbox_data.indexOf (':', idx_value + 1);
      if (idx_value >= 0) str_mailbox_value = str_mailbox_data.substring (idx_value + 1);
    }

    // else, no power key provided, data holds the value
    else str_mailbox_value = str_mailbox_data;

    // convert and update instant power
    OffloadingUpdateHousePower (str_mailbox_value.toInt ());

    // data from message has been handled
    data_handled = true;
  }

  return data_handled;
}

/***********************************************\
 *                    Web
\***********************************************/

// Offloading configuration button
void OffloadingWebButton ()
{
  // meter configuration button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>"), D_PAGE_OFFLOADING_METER, D_OFFLOADING_CONF_METER);
}

// append offloading state to main page
bool OffloadingWebSensor ()
{
  uint16_t contract_power, num_message;
  String   str_color, str_label;

  // if house power is subscribed, display power
  if (offloading_topic_subscribed == true)
  {
    // switch according to current state
    switch (offloading_state)
    { 
      case OFFLOADING_BEFORE:
        num_message = OffloadingGetUpdateBeforeOffload ();
        str_label = String (D_OFFLOADING_SENSOR_BEFORE) + "<br/><small>" + String (D_OFFLOADING_IN) + String (offloading_counter) + String (D_OFFLOADING_UPDATE) + "</small>";
        str_color = "orange";
        break;
      case OFFLOADING_AFTER:
        num_message = OffloadingGetUpdateAfterOffload ();
        str_label = String (D_OFFLOADING_SENSOR_AFTER) + "<br/><small>" + String (D_OFFLOADING_IN) + String (offloading_counter) + String (D_OFFLOADING_UPDATE) + "</small>";
        str_color = "orange";
        break;
      case OFFLOADING_ACTIVE:
        str_label = String (D_OFFLOADING_OFFLOAD);
        str_color = "red";
        break;
    }
    
    // display current state
    if (str_label.length () > 0) WSContentSend_PD (PSTR("<tr><th colspan=2 style='font-size:1.6em; color:%s; text-align:center;'>%s</th></tr>"), str_color.c_str (), str_label.c_str ());

    contract_power = OffloadingGetContract ();
    if (contract_power > 0) WSContentSend_PD (PSTR("<tr><th>%s</th><td><b>%d</b> / %dW</td></tr>"), D_OFFLOADING_HOUSE, offloading_house_power, contract_power);
  }
}

// Pilot Wire web page
void OffloadingWebPage ()
{
  uint16_t num_message, power_heater, power_limit;
  char     argument[OFFLOADING_BUFFER_SIZE];
  String   str_topic, str_key;

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // page comes from save button on configuration page
  if (WebServer->hasArg("save"))
  {
    // get power of heater according to 'power' parameter
    WebGetArg (D_CMND_OFFLOADING_POWER, argument, OFFLOADING_BUFFER_SIZE);
    if (strlen(argument) > 0) OffloadingSetDevicePower ((uint16_t)atoi (argument));

    // get maximum power limit according to 'contract' parameter
    WebGetArg (D_CMND_OFFLOADING_CONTRACT, argument, OFFLOADING_BUFFER_SIZE);
    OffloadingSetContract ((uint16_t)atoi (argument));

    // get MQTT topic according to 'topic' parameter
    WebGetArg (D_CMND_OFFLOADING_TOPIC, argument, OFFLOADING_BUFFER_SIZE);
    OffloadingSetMqttHouseTopic (argument);

    // get JSON key according to 'key' parameter
    WebGetArg (D_CMND_OFFLOADING_KEY, argument, OFFLOADING_BUFFER_SIZE);
    OffloadingSetMqttHouseKey (argument);

    // get number of overload messages before offloading heater according to 'before' parameter
    WebGetArg (D_CMND_OFFLOADING_BEFORE, argument, OFFLOADING_BUFFER_SIZE);
    OffloadingSetUpdateBeforeOffload ((uint8_t)atoi (argument));

    // get number of correct load messages before removing offload of heater according to 'after' parameter
    WebGetArg (D_CMND_OFFLOADING_AFTER, argument, OFFLOADING_BUFFER_SIZE);
    OffloadingSetUpdateAfterOffload ((uint8_t)atoi (argument));
  }

  // beginning of form
  WSContentStart_P (D_OFFLOADING_CONF_METER);
  WSContentSendStyle ();
  WSContentSend_P (OFFLOADING_FORM_START, D_PAGE_OFFLOADING_METER);

  // house section  
  // --------------
  WSContentSend_P (OFFLOADING_FIELDSET_START, D_OFFLOADING_HOUSE);

  // house contract power limit
  power_limit = OffloadingGetContract ();
  WSContentSend_P (PSTR ("%s (W)<br/>"), D_OFFLOADING_CONTRACT);
  WSContentSend_P (PSTR ("<input type='number' name='%s' value='%d'>"), D_CMND_OFFLOADING_CONTRACT, power_limit);
  WSContentSend_P (PSTR ("<br/>"));

  // house power mqtt topic
  OffloadingGetMqttHouseTopic (str_topic);
  WSContentSend_P (PSTR ("<br/>%s<br/>"), D_OFFLOADING_TOPIC);
  WSContentSend_P (PSTR ("<input name='%s' value='%s'>"), D_CMND_OFFLOADING_TOPIC, str_topic.c_str ());
  WSContentSend_P (PSTR ("<br/>"));

  // house power json key
  OffloadingGetMqttHouseKey (str_key);
  WSContentSend_P (PSTR ("<br/>%s<br/>"), D_OFFLOADING_KEY);
  WSContentSend_P (PSTR ("<input name='%s' value='%s'>"), D_CMND_OFFLOADING_KEY, str_key.c_str ());
  WSContentSend_P (PSTR ("<br/>"));

  WSContentSend_P (OFFLOADING_FIELDSET_STOP);

  // heater section  
  // --------------
  WSContentSend_P (OFFLOADING_FIELDSET_START, D_OFFLOADING_DEVICE);

  // heater power
  power_heater = OffloadingGetDevicePower ();
  WSContentSend_P (PSTR ("%s (W)<br/>"), D_OFFLOADING_POWER);
  WSContentSend_P (PSTR ("<input type='number' name='%s' value='%d'><br/>"), D_CMND_OFFLOADING_POWER, power_heater);
  WSContentSend_P (PSTR ("<br/>"));

  // number of overload messages before offloading heater
  num_message  = OffloadingGetUpdateBeforeOffload ();
  WSContentSend_P (PSTR ("<br/>%s<br/>"), D_OFFLOADING_UPDATE_BEFORE);
  WSContentSend_P (PSTR ("<input type='number' name='%s' min='0' step='1' value='%d'>"), D_CMND_OFFLOADING_BEFORE, num_message);
  WSContentSend_P (PSTR ("<br/>"));

  // number of correct load messages before removing offload of heater
  num_message  = OffloadingGetUpdateAfterOffload ();
  WSContentSend_P (PSTR ("<br/>%s<br/>"), D_OFFLOADING_UPDATE_AFTER);
  WSContentSend_P (PSTR ("<input type='number' name='%s' min='0' step='1' value='%d'>"), D_CMND_OFFLOADING_AFTER, num_message);
  WSContentSend_P (PSTR ("<br/>"));

  WSContentSend_P (OFFLOADING_FIELDSET_STOP);

  // save button
  WSContentSend_P (PSTR ("<button name='save' type='submit' class='button bgrn'>%s</button>"), D_SAVE);
  WSContentSend_P (OFFLOADING_FORM_STOP);

  // configuration button
  WSContentSpaceButton(BUTTON_CONFIGURATION);

  // end of page
  WSContentStop();
}

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
      OffloadingCheckMqttConnexion ();
      break;
    case FUNC_MQTT_DATA:
      result = OffloadingMqttData ();
      break;
    case FUNC_EVERY_SECOND:
      OffloadingCheckMqttConnexion ();
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
    case FUNC_JSON_APPEND:
      OffloadingShowJSON (true);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      WebServer->on ("/" D_PAGE_OFFLOADING_METER, OffloadingWebPage);
      break;
    case FUNC_WEB_ADD_BUTTON:
      OffloadingWebButton ();
      break;
    case FUNC_WEB_SENSOR:
      OffloadingWebSensor ();
      break;
#endif  // USE_WEBSERVER

  }
  
  return result;
}

#endif // USE_OFFLOADING