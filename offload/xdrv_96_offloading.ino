/*
  xdrv_96_offloading.ino - Device offloading thru MQTT instant house power
  
    23/03/2020 - v1.0 - Creation
    20/07/2020 - v1.1 - Change delays to seconds
    22/07/2020 - v1.2 - Update instant device power in case of Sonoff energy module
                   
  Settings are stored using weighting scale parameters :
    - Settings.energy_max_power              = Power of plugged appliance (W) 
    - Settings.energy_max_power_limit        = Maximum power of contract (W) 
    - Settings.energy_max_power_limit_window = Delay in seconds before effective offload
    - Settings.energy_max_power_limit_hold   = Delay in seconds before removal of offload
    - Settings.free_f03                      = MQTT Instant house power (Power MQTT topic;Power JSON key)

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
 *                Offloading
\*************************************************/

#ifdef USE_OFFLOADING

#define XDRV_96                     96
#define XSNS_96                     96

#define OFFLOADING_BUFFER_SIZE      128

#define D_PAGE_OFFLOADING_METER     "meter"

#define D_CMND_OFFLOADING_BEFORE    "before"
#define D_CMND_OFFLOADING_AFTER     "after"
#define D_CMND_OFFLOADING_DEVICE    "device"
#define D_CMND_OFFLOADING_CONTRACT  "contract"
#define D_CMND_OFFLOADING_TOPIC     "ptopic"
#define D_CMND_OFFLOADING_KEY       "pkey"

#define D_JSON_OFFLOADING           "Offload"
#define D_JSON_OFFLOADING_STATE     "State"
#define D_JSON_OFFLOADING_STAGE     "Stage"
#define D_JSON_OFFLOADING_BEFORE    "Before"
#define D_JSON_OFFLOADING_AFTER     "After"
#define D_JSON_OFFLOADING_CONTRACT  "Contract"
#define D_JSON_OFFLOADING_DEVICE    "Device"
#define D_JSON_OFFLOADING_TOPIC     "Topic"
#define D_JSON_OFFLOADING_KEY       "Key"

#define D_OFFLOADING                "Offloading"

#define D_OFFLOADING_POWER          "Power"
#define D_OFFLOADING_DELAY          "Delay"
#define D_OFFLOADING_STATUS         "Status"
#define D_OFFLOADING_BEFORE         "Before offloading"
#define D_OFFLOADING_AFTER          "Before offloading removal"

#define D_OFFLOADING_UNIT_W         "W"
#define D_OFFLOADING_UNIT_SEC       "sec."

#define D_OFFLOADING_CONFIGURE      "Configure Offloading"
#define D_OFFLOADING_DEVICE         "Device"
#define D_OFFLOADING_INSTCONTRACT   "Act/Max"
#define D_OFFLOADING_CONTRACT       "Contract"
#define D_OFFLOADING_ACTIVE         "Active"
#define D_OFFLOADING_TOPIC          "MQTT Topic"
#define D_OFFLOADING_KEY            "MQTT JSON Key"

// offloading commands
enum OffloadingCommands { CMND_OFFLOADING_DEVICE, CMND_OFFLOADING_CONTRACT, CMND_OFFLOADING_BEFORE, CMND_OFFLOADING_AFTER, CMND_OFFLOADING_TOPIC, CMND_OFFLOADING_KEY };
const char kOffloadingCommands[] PROGMEM = D_CMND_OFFLOADING_DEVICE "|" D_CMND_OFFLOADING_CONTRACT "|" D_CMND_OFFLOADING_BEFORE "|" D_CMND_OFFLOADING_AFTER "|" D_CMND_OFFLOADING_TOPIC "|" D_CMND_OFFLOADING_KEY;

// constant chains
const char str_conf_fieldset_start[] PROGMEM = "<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>\n";
const char str_conf_fieldset_stop[] PROGMEM = "</fieldset></p>\n";
const char str_conf_input_number[] PROGMEM = "<p>%s (%s)<span style='float:right;font-size:0.7rem;'>%s</span><br><input type='number' name='%s' min='0' step='1' value='%d'></p>\n";
const char str_conf_input_text[] PROGMEM = "<p>%s<span style='float:right;font-size:0.7rem;'>%s</span><br><input name='%s' value='%s'></p>\n";

/*************************************************\
 *               Variables
\*************************************************/

// offloading stages
enum OffloadingStages { OFFLOADING_NONE, OFFLOADING_BEFORE, OFFLOADING_ACTIVE, OFFLOADING_AFTER };

// variables
bool    offloading_relay_managed    = true;               // define if relay is managed directly
uint8_t offloading_relay_state      = 0;                  // relay state before offloading
uint8_t offloading_stage            = OFFLOADING_NONE;    // current offloading state
ulong   offloading_stage_time       = 0;                  // time of current stage
int     offloading_power_actual     = 0;                  // actual total power (retrieved thru MQTT)
bool    offloading_topic_subscribed = false;              // flag for power subscription

/**************************************************\
 *                  Accessors
\**************************************************/

// get offload state
bool OffloadingIsOffloaded ()
{
  return (offloading_stage >= OFFLOADING_ACTIVE);
}

// get maximum power limit before offload
uint16_t OffloadingGetContractPower ()
{
  return Settings.energy_max_power_limit;
}

// set maximum power limit before offload
void OffloadingSetContractPower (uint16_t new_power)
{
  Settings.energy_max_power_limit = new_power;
}

// get power of device
uint16_t OffloadingGetDevicePower ()
{
  return Settings.energy_max_power;
}

// set power of device
void OffloadingSetDevicePower (uint16_t new_power)
{
  Settings.energy_max_power = new_power;
}

// get delay in seconds before effective offloading
uint16_t OffloadingGetDelayBeforeOffload ()
{
  return Settings.energy_max_power_limit_window;;
}

// set delay in seconds before effective offloading
void OffloadingSetDelayBeforeOffload (uint16_t number)
{
  Settings.energy_max_power_limit_window = number;
}

// get delay in seconds before removing offload
uint16_t OffloadingGetDelayBeforeRemoval ()
{
  return Settings.energy_max_power_limit_hold;;
}

// set delay in seconds before removing offload
void OffloadingSetDelayBeforeRemoval (uint16_t number)
{
  Settings.energy_max_power_limit_hold = number;
}

// get instant power MQTT topic
void OffloadingGetMqttPowerTopic (String& str_topic)
{
  int    index;
  String str_setting;

  // extract power topic from settings (power topic;power key)
  str_setting = (char*)Settings.free_f03;
  index  = str_setting.indexOf (';');
  if (index != -1) str_topic = str_setting.substring (0, index);
  else str_topic = "";
}

// get instant power JSON key (power topic;power key)
void OffloadingGetMqttPowerKey (String& str_key)
{
  int    index;
  String str_setting;

  // extract power data from settings
  str_setting = (char*)Settings.free_f03;
  index = str_setting.indexOf (';');
  if (index != -1) str_key = str_setting.substring (index + 1);
  else str_key = "";
}

// set instant power MQTT topic (power topic;power key)
void OffloadingSetMqttPowerTopic (char* str_topic)
{
  int    index;
  String str_setting;

  // extract data other than power topic
  str_setting = (char*)Settings.free_f03;
  index  = str_setting.indexOf (';');
  if (index != -1) str_setting = str_topic + str_setting.substring (index);
  else str_setting = String (str_topic) + ";";

  // save the full settings
  strncpy ((char*)Settings.free_f03, str_setting.c_str (), 233);
}

// set instant power JSON key (power topic;power key)
void OffloadingSetMqttPowerKey (char* str_key)
{
  int    index;
  String str_setting;

  // extract data other than power key
  str_setting = (char*)Settings.free_f03;
  index = str_setting.indexOf (';');
  if (index != -1) str_setting = str_setting.substring (0, index + 1) + str_key;
  else str_setting = ";" + String (str_key);

  // save the full settings
  strncpy ((char*)Settings.free_f03, str_setting.c_str (), 233);
}

/**************************************************\
 *                  Functions
\**************************************************/

// Show JSON status (for MQTT)
void OffloadingShowJSON (bool append)
{
  bool     is_offloaded;
  uint16_t power_contract, power_device, delay_before, delay_after;
  String   str_json, str_topic, str_key;

  // read data
  is_offloaded   = OffloadingIsOffloaded ();
  delay_before   = OffloadingGetDelayBeforeOffload ();
  delay_after    = OffloadingGetDelayBeforeRemoval ();
  power_device   = OffloadingGetDevicePower ();
  power_contract = OffloadingGetContractPower ();
  OffloadingGetMqttPowerTopic (str_topic);
  OffloadingGetMqttPowerKey (str_key);

  // start message  -->  {  or message,
  if (append == false) str_json = "{";
  else str_json = String (mqtt_data) + ",";

  // Offloading  -->  "Offload":{"State":"OFF","Stage":1,"Before":1,"After":5,"Device":1000,"Contract":5000,"Topic":"mqtt/topic/of/device","Key":"Power"}
  str_json += "\"" + String (D_JSON_OFFLOADING) + "\":{";
  str_json += "\"" + String (D_JSON_OFFLOADING_STATE) + "\":";
  if (is_offloaded == true) str_json += "\"ON\",";
  else str_json += "\"OFF\",";
  str_json += "\"" + String (D_JSON_OFFLOADING_STAGE) + "\":" + String (offloading_stage) + ",";
  str_json += "\"" + String (D_JSON_OFFLOADING_BEFORE) + "\":" + String (delay_before) + ",";
  str_json += "\"" + String (D_JSON_OFFLOADING_AFTER) + "\":" + String (delay_after) + ",";
  str_json += "\"" + String (D_JSON_OFFLOADING_DEVICE) + "\":" + String (power_device) + ",";
  str_json += "\"" + String (D_JSON_OFFLOADING_CONTRACT) + "\":" + String (power_contract) + ",";
  str_json += "\"" + String (D_JSON_OFFLOADING_TOPIC) + "\":\"" + str_topic + "\",";
  str_json += "\"" + String (D_JSON_OFFLOADING_KEY) + "\":\"" + str_key + "\"}";

  // if not in append mode, add last bracket 
  if (append == false) str_json += "}";

  // add json string to MQTT message
  snprintf_P (mqtt_data, sizeof(mqtt_data), str_json.c_str ());

  // if not in append mode, publish message 
  if (append == false) MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));
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
      OffloadingGetMqttPowerTopic (str_topic);
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

// Handle offloading MQTT commands
bool OffloadingMqttCommand ()
{
  bool command_handled = true;
  int  command_code;
  char command [CMDSZ];

  // check MQTT command
  command_code = GetCommandCode (command, sizeof(command), XdrvMailbox.topic, kOffloadingCommands);

  // handle command
  switch (command_code)
  {
    case CMND_OFFLOADING_DEVICE:  // set device power
      OffloadingSetDevicePower (XdrvMailbox.payload);
      break;
    case CMND_OFFLOADING_CONTRACT:  // set house contract power
      OffloadingSetContractPower (XdrvMailbox.payload);
      break;
    case CMND_OFFLOADING_TOPIC:  // set mqtt house power topic 
      OffloadingSetMqttPowerTopic (XdrvMailbox.data);
      break;
    case CMND_OFFLOADING_KEY:  // set mqtt house power key 
      OffloadingSetMqttPowerKey (XdrvMailbox.data);
      break;
    case CMND_OFFLOADING_BEFORE:  // set delay before offload (in seconds) 
      OffloadingSetDelayBeforeOffload (XdrvMailbox.payload);
      break;
    case CMND_OFFLOADING_AFTER:  // set delay before removing offload (in seconds) 
      OffloadingSetDelayBeforeRemoval (XdrvMailbox.payload);
      break;
    default:
      command_handled = false;
  }

  // if needed, send updated status
  if (command_handled == true) OffloadingShowJSON (false);
  
  return command_handled;
}

// read received MQTT data to retrieve house instant power
bool OffloadingMqttData ()
{
  bool    data_handled = false;
  int     idx_value;
  String  str_power_topic, str_power_key;
  String  str_mailbox_topic, str_mailbox_data, str_mailbox_value;

  // get topic and topics to compare
  OffloadingGetMqttPowerKey (str_power_key);
  OffloadingGetMqttPowerTopic (str_power_topic);
  str_mailbox_topic = XdrvMailbox.topic;

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
    offloading_power_actual = str_mailbox_value.toInt ();

    // data from message has been handled
    data_handled = true;
  }

  return data_handled;
}

// update offloading status according to all parameters
void OffloadingUpdateStatus ()
{
  uint8_t  prev_stage, next_stage;
  uint16_t power_mesured, power_device, power_contract;
  ulong    time_now, time_delay;

  // get device power and global power limit
  power_device   = OffloadingGetDevicePower ();
  power_contract = OffloadingGetContractPower ();
  power_mesured  = (uint16_t)Energy.active_power[0];

  // check if device instant power is beyond defined power
  if (power_mesured > power_device)
  {
    power_device = power_mesured;
    OffloadingSetDevicePower (power_mesured);
  }

  // get current time
  time_now = millis ();

  // if contract power and device power are defined
  if ((power_contract > 0) && (power_device > 0))
  {
    // set previous and next state to current state
    prev_stage = offloading_stage;
    next_stage = offloading_stage;
  
    // switch according to current state
    switch (offloading_stage)
    { 
      // actually not offloaded
      case OFFLOADING_NONE:
        // save relay state
        offloading_relay_state = bitRead (power, 0);

        // if overload is detected
        if (offloading_power_actual > power_contract)
        { 
          // set time for effective offloading calculation
          offloading_stage_time = time_now;

          // next state is before offloading
          next_stage = OFFLOADING_BEFORE;
        }
        break;

      // pending offloading
      case OFFLOADING_BEFORE:
        // save relay state
        offloading_relay_state = bitRead (power, 0);

        // if house power has gone down, remove pending offloading
        if (offloading_power_actual <= power_contract) next_stage = OFFLOADING_NONE;

        // else if delay is reached, set active offloading
        else
        {
          time_delay = 1000 * OffloadingGetDelayBeforeOffload ();
          if (time_now - offloading_stage_time > time_delay)
          {
            // set next stage as offloading
            next_stage = OFFLOADING_ACTIVE;

            // get relay state and log
            offloading_relay_state = bitRead (power, 0);
            AddLog_P2(LOG_LEVEL_INFO, PSTR("PWR: Offloading start (relay = %d)"), offloading_relay_state);

            // read relay state and switch off if needed
            if ((offloading_relay_managed == true) && (offloading_relay_state == 1)) ExecuteCommandPower (1, POWER_OFF, SRC_MAX);
          }
        } 
        break;

      // offloading is active
      case OFFLOADING_ACTIVE:
        // to remove offload, power limit is current limit minus device power
        power_contract -= power_device;

        if (offloading_power_actual <= power_contract)
        {
          // set time for removing offloading calculation
          offloading_stage_time = time_now;

          // set stage to after offloading
          next_stage = OFFLOADING_AFTER;
        }
        break;

      // actually just after offloading should stop
      case OFFLOADING_AFTER:
        // if house power has gone again too high, offloading back to active state
        if (offloading_power_actual > power_contract - power_device) next_stage = OFFLOADING_ACTIVE;
        
        // else if delay is reached, set active offloading
        else
        {
          time_delay = 1000 * OffloadingGetDelayBeforeRemoval ();
          if (time_now - offloading_stage_time > time_delay)
          {
            // set stage to after offloading
            next_stage = OFFLOADING_NONE;

            // log offloading removal
            AddLog_P2(LOG_LEVEL_INFO, PSTR("PWR: Offloading stop (relay = %d)"), offloading_relay_state);

            // switch back relay ON if needed
            if ((offloading_relay_managed == true) && (offloading_relay_state == 1)) ExecuteCommandPower (1, POWER_ON, SRC_MAX);
          } 
        } 
        break;
    }

    // update offloading state
    offloading_stage = next_stage;

    // if state has changed, send MQTT status
    if (next_stage != prev_stage) OffloadingShowJSON (false);
  }
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// Offloading configuration button
void OffloadingWebButton ()
{
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>"), D_PAGE_OFFLOADING_METER, D_OFFLOADING_CONFIGURE);
}

// append offloading state to main page
bool OffloadingWebSensor ()
{
  uint16_t contract_power, num_message;
  String   str_title, str_text;
  ulong    time_now, time_left, time_delay;
  
  // device power
  contract_power = OffloadingGetDevicePower ();
  WSContentSend_PD (PSTR("{s}%s{m}<b>%d</b> W{e}"), D_OFFLOADING_DEVICE, contract_power);

  // if house power is subscribed, display power
  if (offloading_topic_subscribed == true)
  {
    // get current time
    time_now = millis ();

    // display current power and contract power limit
    contract_power = OffloadingGetContractPower ();
    str_title = D_OFFLOADING_POWER + String (" (") + D_OFFLOADING_INSTCONTRACT + String (")");
    if (contract_power > 0) WSContentSend_PD (PSTR("{s}%s{m}<b>%d</b> / %d W{e}"), str_title.c_str (), offloading_power_actual, contract_power);

    // switch according to current state
    time_left = 0;
    str_title = D_OFFLOADING;
    switch (offloading_stage)
    { 
      // calculate number of ms left before offloading
      case OFFLOADING_BEFORE:
        time_delay = 1000 * OffloadingGetDelayBeforeOffload ();
        if (time_now - offloading_stage_time < time_delay) time_left = (offloading_stage_time + time_delay - time_now) / 1000;
        str_text = PSTR("<span style='color:orange;'>Starting in <b>") + String (time_left) + PSTR(" sec.</b></span>");
        break;

      // calculate number of ms left before offload removal
      case OFFLOADING_AFTER:
        time_delay = 1000 * OffloadingGetDelayBeforeRemoval ();
        if (time_now - offloading_stage_time < time_delay) time_left = (offloading_stage_time + time_delay - time_now) / 1000;
        str_text = PSTR("<span style='color:red;'>Ending in <b>") + String (time_left) + PSTR(" sec.</b></span>");
        break;

      // device is offloaded
      case OFFLOADING_ACTIVE:
        str_text = PSTR("<span style='color:red;'><b>Active</b></span>");
        break;
    }
    
    // display current state
    if (str_text.length () > 0) WSContentSend_PD (PSTR("{s}%s{m}%s{e}"), str_title.c_str (), str_text.c_str ());
  }
}

// Pilot Wire web page
void OffloadingWebPage ()
{
  uint16_t delay_offload, power_device, power_limit;
  char     argument[OFFLOADING_BUFFER_SIZE];
  String   str_topic, str_key;

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // page comes from save button on configuration page
  if (WebServer->hasArg("save"))
  {
    // get power of heater according to 'power' parameter
    WebGetArg (D_CMND_OFFLOADING_DEVICE, argument, OFFLOADING_BUFFER_SIZE);
    if (strlen(argument) > 0) OffloadingSetDevicePower ((uint16_t)atoi (argument));

    // get maximum power limit according to 'contract' parameter
    WebGetArg (D_CMND_OFFLOADING_CONTRACT, argument, OFFLOADING_BUFFER_SIZE);
    OffloadingSetContractPower ((uint16_t)atoi (argument));

    // get MQTT topic according to 'topic' parameter
    WebGetArg (D_CMND_OFFLOADING_TOPIC, argument, OFFLOADING_BUFFER_SIZE);
    OffloadingSetMqttPowerTopic (argument);

    // get JSON key according to 'key' parameter
    WebGetArg (D_CMND_OFFLOADING_KEY, argument, OFFLOADING_BUFFER_SIZE);
    OffloadingSetMqttPowerKey (argument);

    // get delay in sec. before offloading device according to 'before' parameter
    WebGetArg (D_CMND_OFFLOADING_BEFORE, argument, OFFLOADING_BUFFER_SIZE);
    OffloadingSetDelayBeforeOffload ((uint16_t)atoi (argument));

    // get delay in sec. after offloading device according to 'after' parameter
    WebGetArg (D_CMND_OFFLOADING_AFTER, argument, OFFLOADING_BUFFER_SIZE);
    OffloadingSetDelayBeforeRemoval ((uint16_t)atoi (argument));
  }

  // beginning of form
  WSContentStart_P (D_OFFLOADING_CONFIGURE);
  WSContentSendStyle ();
  WSContentSend_P (PSTR("<form method='get' action='%s'>\n"), D_PAGE_OFFLOADING_METER);

  // contract power limit section  
  // ----------------------------
  WSContentSend_P (str_conf_fieldset_start, D_OFFLOADING_CONTRACT, D_OFFLOADING_POWER);

  // contract power limit
  power_limit = OffloadingGetContractPower ();
  WSContentSend_P (str_conf_input_number, D_OFFLOADING_POWER, D_OFFLOADING_UNIT_W, D_CMND_OFFLOADING_CONTRACT, D_CMND_OFFLOADING_CONTRACT, power_limit);

  // house power mqtt topic
  OffloadingGetMqttPowerTopic (str_topic);
  WSContentSend_P (str_conf_input_text, D_OFFLOADING_TOPIC, D_CMND_OFFLOADING_TOPIC, D_CMND_OFFLOADING_TOPIC, str_topic.c_str ());

  // house power json key
  OffloadingGetMqttPowerKey (str_key);
  WSContentSend_P (str_conf_input_text, D_OFFLOADING_KEY, D_CMND_OFFLOADING_KEY, D_CMND_OFFLOADING_KEY, str_key.c_str ());

  WSContentSend_P (str_conf_fieldset_stop);

  // device section  
  // --------------
  WSContentSend_P (str_conf_fieldset_start, D_OFFLOADING_DEVICE);

  // device power
  power_device = OffloadingGetDevicePower ();
  WSContentSend_P (str_conf_input_number, D_OFFLOADING_POWER, D_OFFLOADING_UNIT_W, D_CMND_OFFLOADING_DEVICE, D_CMND_OFFLOADING_DEVICE, power_device);

  WSContentSend_P (str_conf_fieldset_stop);

  // delay section  
  // -------------
  WSContentSend_P (str_conf_fieldset_start, D_OFFLOADING_DELAY);

  // delay in seconds before offloading the device
  delay_offload = OffloadingGetDelayBeforeOffload ();
  WSContentSend_P (str_conf_input_number, D_OFFLOADING_BEFORE, D_OFFLOADING_UNIT_SEC, D_CMND_OFFLOADING_BEFORE, D_CMND_OFFLOADING_BEFORE, delay_offload);

  // delay in seconds before removing offload of the device
  delay_offload = OffloadingGetDelayBeforeRemoval ();
  WSContentSend_P (str_conf_input_number, D_OFFLOADING_AFTER, D_OFFLOADING_UNIT_SEC, D_CMND_OFFLOADING_AFTER, D_CMND_OFFLOADING_AFTER, delay_offload);

  WSContentSend_P (str_conf_fieldset_stop);

  // save button  
  // -----------
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

bool Xdrv96 (uint8_t function)
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
    case FUNC_COMMAND:
      result = OffloadingMqttCommand ();
      break;
    case FUNC_EVERY_250_MSECOND:
      OffloadingUpdateStatus ();
      break;
    case FUNC_EVERY_SECOND:
      OffloadingCheckMqttConnexion ();
      break;
  }
  
  return result;
}

bool Xsns96 (uint8_t function)
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
