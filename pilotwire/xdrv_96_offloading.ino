/*
  xdrv_96_offloading.ino - Device offloading thru MQTT instant and max power
  
    23/03/2020 - v1.0   - Creation
    20/07/2020 - v1.1   - Change delays to seconds
    22/07/2020 - v1.2   - Update instant device power in case of Sonoff energy module
    05/08/2020 - v1.4   - Get max power thru MQTT meter
    22/08/2020 - v1.4.1 - Add restart after offload configuration
    05/09/2020 - v1.4.2 - Correct display exception on mainpage
    15/09/2020 - v1.5   - Add OffloadJustSet and OffloadJustRemoved
                   
  Settings are stored using unused KNX parameters :
    - Settings.knx_GA_addr[0]   = Power of plugged appliance (W) 
    - Settings.knx_GA_addr[1]   = Maximum power of contract (W) 
    - Settings.knx_GA_addr[2]   = Delay in seconds before effective offload
    - Settings.knx_GA_addr[3]   = Delay in seconds before removal of offload
    - Settings.knx_GA_addr[4]   = Phase number (1...3)

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

#define XDRV_96                 96
#define XSNS_96                 96

#define OFFLOAD_BUFFER_SIZE     128
#define OFFLOAD_PHASE_MAX       3
#define OFFLOAD_MESSAGE_LEFT    5
#define OFFLOAD_MESSAGE_DELAY   5

#define D_PAGE_OFFLOAD_CONFIG   "offload"
#define D_PAGE_OFFLOAD_CONTROL  "control"

#define D_CMND_OFFLOAD_BEFORE   "before"
#define D_CMND_OFFLOAD_AFTER    "after"
#define D_CMND_OFFLOAD_DEVICE   "device"
#define D_CMND_OFFLOAD_MAX      "max"
#define D_CMND_OFFLOAD_PHASE    "phase"
#define D_CMND_OFFLOAD_TOPIC    "topic"
#define D_CMND_OFFLOAD_KEY_INST "kinst"
#define D_CMND_OFFLOAD_KEY_MAX  "kmax"
#define D_CMND_OFFLOAD_OFF      "off"
#define D_CMND_OFFLOAD_ON       "on"

#define D_JSON_OFFLOAD          "Offload"
#define D_JSON_OFFLOAD_STATE    "State"
#define D_JSON_OFFLOAD_STAGE    "Stage"
#define D_JSON_OFFLOAD_PHASE    "Phase"
#define D_JSON_OFFLOAD_BEFORE   "Before"
#define D_JSON_OFFLOAD_AFTER    "After"
#define D_JSON_OFFLOAD_CONTRACT "Contract"
#define D_JSON_OFFLOAD_DEVICE   "Device"
#define D_JSON_OFFLOAD_TOPIC    "Topic"
#define D_JSON_OFFLOAD_KEY_INST "KeyInst"
#define D_JSON_OFFLOAD_KEY_MAX  "KeyMax"

#define MQTT_OFFLOAD_TOPIC      "compteur/tele/SENSOR"
#define MQTT_OFFLOAD_KEY_INST   "SINSTS"
#define MQTT_OFFLOAD_KEY_MAX    "SSOUSC"

#define D_OFFLOADING            "Offloading"

#define D_OFFLOAD_POWER         "Power"
#define D_OFFLOAD_DELAY         "Delay"
#define D_OFFLOAD_BEFORE        "Before offload"
#define D_OFFLOAD_AFTER         "Before offload removal"

#define D_OFFLOAD_UNIT_W        "W"
#define D_OFFLOAD_UNIT_SEC      "sec."

#define D_OFFLOAD_CONFIGURE     "Configure Offload"
#define D_OFFLOAD_CONTROL       "Control"
#define D_OFFLOAD_DEVICE        "Device"
#define D_OFFLOAD_INSTCONTRACT  "Act/Max"
#define D_OFFLOAD_PHASE         "Phase"
#define D_OFFLOAD_CONTRACT      "Contract"
#define D_OFFLOAD_METER         "Meter (MQTT)"
#define D_OFFLOAD_TOPIC         "Topic"
#define D_OFFLOAD_KEY_INST      "Key : Instant power"
#define D_OFFLOAD_KEY_MAX       "Key : Max power"
#define D_OFFLOAD_ACTIVE        "Délestage en cours"

// list of MQTT extra parameters
enum OffloadParameters { PARAM_OFFLOAD_TOPIC, PARAM_OFFLOAD_KEY_INST, PARAM_OFFLOAD_KEY_MAX };

// offloading commands
enum OffloadCommands { CMND_OFFLOAD_DEVICE, CMND_OFFLOAD_MAX, CMND_OFFLOAD_BEFORE, CMND_OFFLOAD_AFTER, CMND_OFFLOAD_TOPIC, CMND_OFFLOAD_KEY_INST, CMND_OFFLOAD_KEY_MAX };
const char kOffloadCommands[] PROGMEM = D_CMND_OFFLOAD_DEVICE "|" D_CMND_OFFLOAD_MAX "|" D_CMND_OFFLOAD_BEFORE "|" D_CMND_OFFLOAD_AFTER "|" D_CMND_OFFLOAD_TOPIC "|" D_CMND_OFFLOAD_KEY_INST "|" D_CMND_OFFLOAD_KEY_MAX;

// strings
const char OFFLOAD_FIELDSET_START[] PROGMEM = "<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>\n";
const char OFFLOAD_FIELDSET_STOP[] PROGMEM = "</fieldset></p>\n";
const char OFFLOAD_INPUT_NUMBER[] PROGMEM = "<p>%s (%s)<span style='float:right;font-size:0.7rem;'>%s</span><br><input type='number' name='%s' min='0' step='1' value='%d'></p>\n";
const char OFFLOAD_INPUT_TEXT[] PROGMEM = "<p>%s<span style='float:right;font-size:0.7rem;'>%s</span><br><input name='%s' value='%s' placeholder='%s'></p>\n";

/*************************************************\
 *               Variables
\*************************************************/

// offloading stages
enum OffloadStages { OFFLOAD_NONE, OFFLOAD_BEFORE, OFFLOAD_ACTIVE, OFFLOAD_AFTER };

// variables
bool     offload_relay_managed    = true;               // define if relay is managed directly
uint8_t  offload_relay_state      = 0;                  // relay state before offloading
uint8_t  offload_stage            = OFFLOAD_NONE;       // current offloading state
ulong    offload_stage_time       = 0;                  // time of current stage
int      offload_power_inst       = 0;                  // actual phase instant power (retrieved thru MQTT)
bool     offload_topic_subscribed = false;              // flag for power subscription
bool     offload_just_set         = false;              // flag to signal that offload has just been set
bool     offload_just_removed     = false;              // flag to signal that offload has just been removed
uint8_t  offload_message_left     = 0;                  // number of JSON messages to send
uint8_t  offload_message_delay    = 0;                  // delay in seconds before next JSON message
uint32_t offload_message_time     = 0;                  // time of last message

/**************************************************\
 *                  Accessors
\**************************************************/

// get offload state
bool OffloadIsOffloaded ()
{
  return (offload_stage >= OFFLOAD_ACTIVE);
}

// get offload newly set state
bool OffloadJustSet ()
{
  bool result;

  // get the flag and reset it
  result = offload_just_set;
  offload_just_set = false;

  return (result);
}

// get offload newly removed state
bool OffloadJustRemoved ()
{
  bool result;

  // get the flag and reset it
  result = offload_just_removed;
  offload_just_removed = false;

  return (result);
}

// set relay managed mode
void OffloadSetRelayMode (bool is_managed)
{
  offload_relay_managed = is_managed;
}

// activate offload state
bool OffloadActivate ()
{
  // set flag to signal offload has just been set
  offload_just_set     = true;
  offload_just_removed = false;

  // read relay state and switch off if needed
  if (offload_relay_managed == true)
  {
    // save relay state
    offload_relay_state = bitRead (power, 0);

    // if relay is ON, switch off
    if (offload_relay_state == 1) ExecuteCommandPower (1, POWER_OFF, SRC_MAX);
  }

  // get relay state and log
  AddLog_P2(LOG_LEVEL_INFO, PSTR("PWR: Offload starts (relay = %d)"), offload_relay_state);
}

// remove offload state
void OffloadRemove ()
{
  // set flag to signal offload has just been removed
  offload_just_removed = true;
  offload_just_set     = false;

  // switch back relay ON if needed
  if (offload_relay_managed == true)
  {
    // if relay was ON, switch it back
    if (offload_relay_state == 1) ExecuteCommandPower (1, POWER_ON, SRC_MAX);
  }

  // log offloading removal
  AddLog_P2(LOG_LEVEL_INFO, PSTR("PWR: Offload stops (relay = %d)"), offload_relay_state);
}

// get power of device
uint16_t OffloadGetDevicePower ()
{
  return Settings.knx_GA_addr[0];
}

// set power of device
void OffloadSetDevicePower (uint16_t new_power)
{
  Settings.knx_GA_addr[0] = new_power;
}

// get maximum power limit before offload
uint16_t OffloadGetMaxPower ()
{
  return Settings.knx_GA_addr[1];
}

// set maximum power limit before offload
void OffloadSetMaxPower (uint16_t new_power)
{
  Settings.knx_GA_addr[1] = new_power;
}

// get delay in seconds before effective offloading
uint16_t OffloadGetDelayBeforeOffload ()
{
  return Settings.knx_GA_addr[2];
}

// set delay in seconds before effective offloading
void OffloadSetDelayBeforeOffload (uint16_t number)
{
  Settings.knx_GA_addr[2] = number;
}

// get delay in seconds before removing offload
uint16_t OffloadGetDelayBeforeRemoval ()
{
  return Settings.knx_GA_addr[3];
}

// set delay in seconds before removing offload
void OffloadSetDelayBeforeRemoval (uint16_t number)
{
  Settings.knx_GA_addr[3] = number;
}

// get phase number
uint16_t OffloadGetPhase ()
{
  uint16_t number;

  number = Settings.knx_GA_addr[4];
  if ((number == 0) || (number > 3)) number = 1;
  return number;
}

// set phase number
void OffloadSetPhase (uint16_t number)
{
  if ((number > 0) && (number < 4)) Settings.knx_GA_addr[4] = number;
  else Settings.knx_GA_addr[4] = 1;
}

// get instant power MQTT topic
String OffloadGetPowerTopic (bool get_default)
{
  String str_result;

  // get value or default value if not set
  str_result = SettingsText (SET_OFFLOAD_TOPIC);
  if (get_default == true || str_result == "") str_result = MQTT_OFFLOAD_TOPIC;

  return str_result;
}

// set power MQTT topic
void OffloadSetPowerTopic (char* str_topic)
{
  SettingsUpdateText (SET_OFFLOAD_TOPIC, str_topic);
}

// get max power JSON key
String OffloadGetMaxPowerKey (bool get_default)
{
  String str_result;

  // get value or default value if not set
  str_result = SettingsText (SET_OFFLOAD_KEY_MAX);
  if (get_default == true || str_result == "") str_result = MQTT_OFFLOAD_KEY_MAX;

  return str_result;
}

// set max power JSON key
void OffloadSetMaxPowerKey (char* str_key)
{
  SettingsUpdateText (SET_OFFLOAD_KEY_MAX, str_key);
}

// get instant power JSON key
String OffloadGetInstPowerKey (bool get_default)
{
  String str_result;

  // get value or default value if not set
  str_result = SettingsText (SET_OFFLOAD_KEY_INST);
  if (get_default == true || str_result == "") str_result = MQTT_OFFLOAD_KEY_INST + String (OffloadGetPhase ());

  return str_result;
}

// set instant power JSON key
void OffloadSetInstPowerKey (char* str_key)
{
  SettingsUpdateText (SET_OFFLOAD_KEY_INST, str_key);
}

/**************************************************\
 *                  Functions
\**************************************************/

// Show JSON status (for MQTT)
void OffloadShowJSON (bool append)
{
  bool     is_offloaded;
  uint16_t power_max, power_device, delay_before, delay_after;
  String   str_mqtt, str_json, str_status;

  // read data
  is_offloaded = OffloadIsOffloaded ();
  delay_before = OffloadGetDelayBeforeOffload ();
  delay_after  = OffloadGetDelayBeforeRemoval ();
  power_device = OffloadGetDevicePower ();
  power_max    = OffloadGetMaxPower ();

  // "Offload":{
  //   "State":"OFF","Stage":1,
  //   "Phase":2,"Before":1,"After":5,"Device":1000,"Contract":5000,"Topic":"...","KeyInst":"...","KeyMax":"..."  //           
  //   }
  str_json = "\"" + String (D_JSON_OFFLOAD) + "\":{";
  
  // dynamic data
  if (is_offloaded == true) str_status = MQTT_STATUS_ON;
  else str_status = MQTT_STATUS_OFF;
  str_json += "\"" + String (D_JSON_OFFLOAD_STATE) + "\":\"" + str_status + "\"";
  str_json += ",\"" + String (D_JSON_OFFLOAD_STAGE) + "\":" + String (offload_stage);
  
  // static data
  if (append == true) 
  {
    str_json += ",\"" + String (D_JSON_OFFLOAD_PHASE) + "\":" + OffloadGetPhase ();
    str_json += ",\"" + String (D_JSON_OFFLOAD_BEFORE) + "\":" + String (delay_before);
    str_json += ",\"" + String (D_JSON_OFFLOAD_AFTER) + "\":" + String (delay_after);
    str_json += ",\"" + String (D_JSON_OFFLOAD_DEVICE) + "\":" + String (power_device);
    str_json += ",\"" + String (D_JSON_OFFLOAD_CONTRACT) + "\":" + String (power_max);
    str_json += ",\"" + String (D_JSON_OFFLOAD_TOPIC) + "\":\"" + OffloadGetPowerTopic (false) + "\"";
    str_json += ",\"" + String (D_JSON_OFFLOAD_KEY_INST) + "\":\"" + OffloadGetInstPowerKey (false) + "\"";
    str_json += ",\"" + String (D_JSON_OFFLOAD_KEY_MAX) + "\":\"" + OffloadGetMaxPowerKey (false) + "\"";
  }

  str_json += "}";

  // generate MQTT message according to append mode
  if (append == true) str_mqtt = String (mqtt_data) + "," + str_json;
  else str_mqtt = "{" + str_json + "}";

  // place JSON back to MQTT data and publish it if not in append mode
  snprintf_P (mqtt_data, sizeof(mqtt_data), str_mqtt.c_str ());
  if (append == false) MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));
}

// check and update MQTT power subsciption after disconnexion
void OffloadEverySecond ()
{
  bool   is_connected;
  String str_topic;

  // check MQTT connexion
  is_connected = MqttIsConnected();
  if (is_connected)
  {
    // if still no subsciption to power topic
    if (offload_topic_subscribed == false)
    {
      // check power topic availability
      str_topic = OffloadGetPowerTopic (false);
      if (str_topic.length () > 0) 
      {
        // subscribe to power meter
        MqttSubscribe(str_topic.c_str ());

        // subscription done
        offload_topic_subscribed = true;

        // log
        AddLog_P2(LOG_LEVEL_INFO, PSTR("MQT: Subscribed to %s"), str_topic.c_str ());
      }
    }
  }
  else offload_topic_subscribed = false;

  // check if offload JSON messages need to be sent
  if (offload_message_left > 0)
  {
    // decrement delay
    offload_message_delay--;

    // if delay is reached
    if (offload_message_delay == 0)
    {
      // send MQTT message
      OffloadShowJSON (false);

      // update counters
      offload_message_left--;
      if (offload_message_left > 0) offload_message_delay = OFFLOAD_MESSAGE_DELAY;
    }
  }
}

// Handle offloading MQTT commands
bool OffloadMqttCommand ()
{
  bool command_handled = true;
  int  command_code;
  char command [CMDSZ];

  // check MQTT command
  command_code = GetCommandCode (command, sizeof(command), XdrvMailbox.topic, kOffloadCommands);

  // handle command
  switch (command_code)
  {
    case CMND_OFFLOAD_DEVICE:  // set device power
      OffloadSetDevicePower (XdrvMailbox.payload);
      break;
    case CMND_OFFLOAD_MAX:  // set maximum power per phase
      OffloadSetMaxPower (XdrvMailbox.payload);
      break;
    case CMND_OFFLOAD_BEFORE:  // set delay before offload (in seconds) 
      OffloadSetDelayBeforeOffload (XdrvMailbox.payload);
      break;
    case CMND_OFFLOAD_AFTER:  // set delay before removing offload (in seconds) 
      OffloadSetDelayBeforeRemoval (XdrvMailbox.payload);
      break;
    default:
      command_handled = false;
  }
  
  return command_handled;
}

// read received MQTT data to retrieve house instant power
bool OffloadMqttData ()
{
  bool    data_handled = false;
  int     idx_value, mqtt_power;
  String  str_data, str_topic, str_key;

  // if topic is the instant house power
  str_topic = OffloadGetPowerTopic (false);
  if (str_topic == XdrvMailbox.topic)
  {
    // log and counter increment
    AddLog_P2(LOG_LEVEL_INFO, PSTR("MQT: Received %s"), str_topic.c_str ());
    offload_message_time = LocalTime ();

    // get message data (removing SPACE and QUOTE)
    str_data  = XdrvMailbox.data;
    str_data.replace (" ", "");
    str_data.replace ("\"", "");

    // if instant power key is present
    str_key = OffloadGetInstPowerKey (false) + ":";
    idx_value = str_data.indexOf (str_key);
    if (idx_value >= 0) idx_value = str_data.indexOf (':', idx_value + 1);
    if (idx_value >= 0)
    {
      offload_power_inst = str_data.substring (idx_value + 1).toInt ();
      data_handled = true;
    }

    // if max power key is present
    str_key = OffloadGetMaxPowerKey (false) + ":";
    idx_value = str_data.indexOf (str_key);
    if (idx_value >= 0) idx_value = str_data.indexOf (':', idx_value + 1);
    if (idx_value >= 0)
    {
      mqtt_power = str_data.substring (idx_value + 1).toInt ();    
      if (mqtt_power > 0) OffloadSetMaxPower ((uint16_t)mqtt_power);
      data_handled = true;
    }
  }

  return data_handled;
}

// update offloading status according to all parameters
void OffloadUpdateStatus ()
{
  uint8_t  prev_stage, next_stage;
  uint16_t power_mesured, power_device, power_max;
  ulong    time_end, time_now;

  // get device power and global power limit
  power_device  = OffloadGetDevicePower ();
  power_max     = OffloadGetMaxPower ();

  #ifdef USE_ENERGY_SENSOR
  power_mesured = (uint16_t)Energy.active_power[0];
  #else
  power_mesured = 0;
  #endif

  // check if device instant power is beyond defined power
  if (power_mesured > power_device)
  {
    power_device = power_mesured;
    OffloadSetDevicePower (power_mesured);
  }

  // get current time
  time_now = millis () / 1000;

  // if contract power and device power are defined
  if ((power_max > 0) && (power_device > 0))
  {
    // set previous and next state to current state
    prev_stage = offload_stage;
    next_stage = offload_stage;
  
    // switch according to current state
    switch (offload_stage)
    { 
      // actually not offloaded
      case OFFLOAD_NONE:
        // save relay state
        offload_relay_state = bitRead (power, 0);

        // if overload is detected
        if (offload_power_inst > power_max)
        { 
          // set time for effective offloading calculation
          offload_stage_time = time_now;

          // next state is before offloading
          next_stage = OFFLOAD_BEFORE;
        }
        break;

      // pending offloading
      case OFFLOAD_BEFORE:
        // save relay state
        offload_relay_state = bitRead (power, 0);

        // if house power has gone down, remove pending offloading
        if (offload_power_inst <= power_max) next_stage = OFFLOAD_NONE;

        // else if delay is reached, set active offloading
        else
        {
          time_end = offload_stage_time + (ulong) OffloadGetDelayBeforeOffload ();
          if (time_end < time_now)
          {
            // set next stage as offloading
            next_stage = OFFLOAD_ACTIVE;

            // set offload
            OffloadActivate ();
          }
        } 
        break;

      // offloading is active
      case OFFLOAD_ACTIVE:
        // calculate maximum power allowed when substracting device power
        if (power_max > power_device) power_max -= power_device;
        else power_max = 0;

        // if instant power is under this value, prepare to remove offload
        if (offload_power_inst <= power_max)
        {
          // set time for removing offloading calculation
          offload_stage_time = time_now;

          // set stage to after offloading
          next_stage = OFFLOAD_AFTER;
        }
        break;

      // actually just after offloading should stop
      case OFFLOAD_AFTER:
        // calculate maximum power allowed when substracting device power
        if (power_max > power_device) power_max -= power_device;
        else power_max = 0;

        // if house power has gone again too high, offloading back again
        if (offload_power_inst > power_max) next_stage = OFFLOAD_ACTIVE;
        
        // else if delay is reached, set active offloading
        else
        {
          time_end = offload_stage_time + (ulong) OffloadGetDelayBeforeRemoval ();
          if (time_end < time_now)
          {
            // set stage to after offloading
            next_stage = OFFLOAD_NONE;

            // remove offloading state
            OffloadRemove ();
          } 
        } 
        break;
    }

    // update offloading state
    offload_stage = next_stage;

    // if state has changed,
    if (next_stage != prev_stage)
    {
      // send MQTT status
      OffloadShowJSON (false);

      // set counters of JSON message update
      offload_message_left  = OFFLOAD_MESSAGE_LEFT;
      offload_message_delay = OFFLOAD_MESSAGE_DELAY;
    } 
  }
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// append offloading control button to main page
void OffloadWebControlButton ()
{
  // control page button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_PAGE_OFFLOAD_CONTROL, D_OFFLOAD_CONTROL);
}

// Offloading configuration button
void OffloadWebConfigButton ()
{
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>"), D_PAGE_OFFLOAD_CONFIG, D_OFFLOAD_CONFIGURE);
}

// append offloading state to main page
bool OffloadWebSensor ()
{
  uint16_t contract_power, num_message;
  String   str_title, str_text;
  ulong    time_now, time_left, time_end;
  uint32_t message_delay;
  TIME_T   message_dst;

  // device power
  contract_power = OffloadGetDevicePower ();
  WSContentSend_PD (PSTR("{s}%s{m}<b>%d</b> W{e}"), D_OFFLOAD_DEVICE, contract_power);

  // if house power is subscribed, display power
  if (offload_topic_subscribed == true)
  {
    // calculate delay since last power message
    str_text = "...";
    if (offload_message_time > 0)
    {
      // calculate delay
      message_delay = LocalTime() - offload_message_time;
      BreakTime (message_delay, message_dst);

      // generate readable format
      str_text = "";
      if (message_dst.hour > 0) str_text += String (message_dst.hour) + "h";
      if (message_dst.hour > 0 || message_dst.minute > 0) str_text += String (message_dst.minute) + "m";
      str_text += String (message_dst.second) + "s";
    }

    // display current power and contract power limit
    contract_power = OffloadGetMaxPower ();
    if (contract_power > 0) WSContentSend_PD (PSTR("{s}%s <small><i>[%s]</i></small>{m}<b>%d</b> / %d W{e}"), D_OFFLOAD_POWER, str_text.c_str (), offload_power_inst, contract_power);

    // switch according to current state
    time_now  = millis () / 1000;
    time_left = 0;
    str_text  = "";
    str_title = D_OFFLOADING;
    
    switch (offload_stage)
    { 
      // calculate number of ms left before offloading
      case OFFLOAD_BEFORE:
        time_end = offload_stage_time + (ulong) OffloadGetDelayBeforeOffload ();
        if (time_end > time_now) time_left = time_end - time_now;
        str_text = "<span style='color:orange;'>Starting in <b>" + String (time_left) + " sec.</b></span>";
        break;

      // calculate number of ms left before offload removal
      case OFFLOAD_AFTER:
        time_end = offload_stage_time + (ulong) OffloadGetDelayBeforeRemoval ();
        if (time_end > time_now) time_left = time_end - time_now;
        str_text = "<span style='color:red;'>Ending in <b>" + String (time_left) + " sec.</b></span>";
        break;

      // device is offloaded
      case OFFLOAD_ACTIVE:
        str_text = "<span style='color:red;'><b>Active</b></span>";
        break;
    }
    
    // display current state
    if (str_text.length () > 0) WSContentSend_PD (PSTR("{s}%s{m}%s{e}"), str_title.c_str (), str_text.c_str ());
  }
}

// Offload configuration web page
void OffloadWebPageConfig ()
{
  int      index;
  uint16_t current_value;
  char     argument[OFFLOAD_BUFFER_SIZE];
  String   str_text, str_default;

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg("save"))
  {
    // get power of heater according to 'power' parameter
    WebGetArg (D_CMND_OFFLOAD_DEVICE, argument, OFFLOAD_BUFFER_SIZE);
    if (strlen(argument) > 0) OffloadSetDevicePower ((uint16_t)atoi (argument));

    // get maximum power limit according to 'max' parameter
    WebGetArg (D_CMND_OFFLOAD_MAX, argument, OFFLOAD_BUFFER_SIZE);
    if (strlen(argument) > 0) OffloadSetMaxPower ((uint16_t)atoi (argument));

    // get phase number according to 'phase' parameter
    WebGetArg (D_CMND_OFFLOAD_PHASE, argument, OFFLOAD_BUFFER_SIZE);
    if (strlen(argument) > 0) OffloadSetPhase ((uint16_t)atoi (argument));

    // get delay in sec. before offloading device according to 'before' parameter
    WebGetArg (D_CMND_OFFLOAD_BEFORE, argument, OFFLOAD_BUFFER_SIZE);
    if (strlen(argument) > 0) OffloadSetDelayBeforeOffload ((uint16_t)atoi (argument));

    // get delay in sec. after offloading device according to 'after' parameter
    WebGetArg (D_CMND_OFFLOAD_AFTER, argument, OFFLOAD_BUFFER_SIZE);
    if (strlen(argument) > 0) OffloadSetDelayBeforeRemoval ((uint16_t)atoi (argument));

    // get MQTT topic according to 'topic' parameter
    WebGetArg (D_CMND_OFFLOAD_TOPIC, argument, OFFLOAD_BUFFER_SIZE);
    if (strlen(argument) > 0) OffloadSetPowerTopic (argument);

    // get JSON key according to 'inst' parameter
    WebGetArg (D_CMND_OFFLOAD_KEY_INST, argument, OFFLOAD_BUFFER_SIZE);
    if (strlen(argument) > 0) OffloadSetInstPowerKey (argument);

    // get JSON key according to 'max' parameter
    WebGetArg (D_CMND_OFFLOAD_KEY_MAX, argument, OFFLOAD_BUFFER_SIZE);
    if (strlen(argument) > 0) OffloadSetMaxPowerKey (argument);

    // restart device
    WebRestart (1);
  }

  // beginning of form
  WSContentStart_P (D_OFFLOAD_CONFIGURE);
  WSContentSendStyle ();
  WSContentSend_P (PSTR("<form method='get' action='%s'>\n"), D_PAGE_OFFLOAD_CONFIG);

  // Power section  
  // ----------------------------
  WSContentSend_P (OFFLOAD_FIELDSET_START, D_OFFLOAD_POWER);

  // contract power limit
  current_value = OffloadGetMaxPower ();
  WSContentSend_P (OFFLOAD_INPUT_NUMBER, D_OFFLOAD_CONTRACT, D_OFFLOAD_UNIT_W, D_CMND_OFFLOAD_MAX, D_CMND_OFFLOAD_MAX, current_value);

  // phase number
  current_value = OffloadGetPhase ();
  WSContentSend_P ("<label for='phase'>%s</label>\n", "Phase");
  WSContentSend_P ("<select name='phase' id='phase'>\n");
  for (index = 1; index <= OFFLOAD_PHASE_MAX; index ++)
  {
    if (index == current_value) str_text = "selected";
    else str_text = "";
    WSContentSend_P ("<option value='%d' %s>%d</option>\n", index, str_text.c_str (), index);
  }
  WSContentSend_P ("</select>\n");

  // device power
  current_value = OffloadGetDevicePower ();
  WSContentSend_P (OFFLOAD_INPUT_NUMBER, D_OFFLOAD_POWER, D_OFFLOAD_UNIT_W, D_CMND_OFFLOAD_DEVICE, D_CMND_OFFLOAD_DEVICE, current_value);

  WSContentSend_P (OFFLOAD_FIELDSET_STOP);

  // MQTT section  
  // ----------------------------
  WSContentSend_P (OFFLOAD_FIELDSET_START, D_OFFLOAD_METER);

  // instant power mqtt topic
  str_text    = OffloadGetPowerTopic (false);
  str_default = OffloadGetPowerTopic (true);
  if (str_text == str_default) str_text = "";
  WSContentSend_P (OFFLOAD_INPUT_TEXT, D_OFFLOAD_TOPIC, D_CMND_OFFLOAD_TOPIC, D_CMND_OFFLOAD_TOPIC, str_text.c_str (), str_default.c_str ());

  // max power json key
  str_text    = OffloadGetMaxPowerKey (false);
  str_default = OffloadGetMaxPowerKey (true);
  if (str_text == str_default) str_text = "";
  WSContentSend_P (OFFLOAD_INPUT_TEXT, D_OFFLOAD_KEY_MAX, D_CMND_OFFLOAD_KEY_MAX, D_CMND_OFFLOAD_KEY_MAX, str_text.c_str (), str_default.c_str ());

  // instant power json key
  str_text    = OffloadGetInstPowerKey (false);
  str_default = OffloadGetInstPowerKey (true);
  if (str_text == str_default) str_text = "";
  WSContentSend_P (OFFLOAD_INPUT_TEXT, D_OFFLOAD_KEY_INST, D_CMND_OFFLOAD_KEY_INST, D_CMND_OFFLOAD_KEY_INST, str_text.c_str (), str_default.c_str ());

  WSContentSend_P (OFFLOAD_FIELDSET_STOP);

  // delay section  
  // -------------
  WSContentSend_P (OFFLOAD_FIELDSET_START, D_OFFLOAD_DELAY);

  // delay in seconds before offloading the device
  current_value = OffloadGetDelayBeforeOffload ();
  WSContentSend_P (OFFLOAD_INPUT_NUMBER, D_OFFLOAD_BEFORE, D_OFFLOAD_UNIT_SEC, D_CMND_OFFLOAD_BEFORE, D_CMND_OFFLOAD_BEFORE, current_value);

  // delay in seconds before removing offload of the device
  current_value = OffloadGetDelayBeforeRemoval ();
  WSContentSend_P (OFFLOAD_INPUT_NUMBER, D_OFFLOAD_AFTER, D_OFFLOAD_UNIT_SEC, D_CMND_OFFLOAD_AFTER, D_CMND_OFFLOAD_AFTER, current_value);

  WSContentSend_P (OFFLOAD_FIELDSET_STOP);

  // save button  
  // -----------
  WSContentSend_P (PSTR ("<p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), D_SAVE);
  WSContentSend_P (PSTR("</form>\n"));

  // configuration button
  WSContentSpaceButton(BUTTON_CONFIGURATION);

  // end of page
  WSContentStop();
}

// Offloading public configuration page
void OffloadWebPageControl ()
{
  bool    is_offloaded;
  uint8_t relay_state;
  String  str_info, str_name, str_style;

  // if switch has been switched OFF
  if (Webserver->hasArg(D_CMND_OFFLOAD_OFF))
  {
    if (is_offloaded == true) offload_relay_state = 0;
    else ExecuteCommandPower (1, POWER_OFF, SRC_MAX); 
  }

  // else, if switch has been switched ON
  else if (Webserver->hasArg(D_CMND_OFFLOAD_ON))
  {
    if (is_offloaded == true) offload_relay_state = 1;
    else ExecuteCommandPower (1, POWER_ON, SRC_MAX);
  }

  // get offloaded status
  is_offloaded = OffloadIsOffloaded ();

  // read real relay state or saved one if currently offloaded
  if (is_offloaded == true) relay_state = offload_relay_state;
  else relay_state = bitRead (power, 0);

  // set switch parameters
  if (relay_state == 0) { str_name = "on"; str_style = "slide-off"; }
  else { str_name = "off"; str_style = "slide-on"; }
  if (is_offloaded == true) { str_info = D_OFFLOAD_ACTIVE; str_style += " offload"; }
  else str_info = "&nbsp;"; 

  // beginning of form without authentification
  WSContentStart_P (D_OFFLOAD_CONTROL, false);
  WSContentSend_P (PSTR ("</script>\n"));
  
  // page style
  WSContentSend_P (PSTR ("<style>\n"));

  WSContentSend_P (PSTR ("body {color:white;background-color:#303030;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {width:100%%;margin:auto;padding:3px 0px;text-align:center;vertical-align:middle;}\n"));
  
  WSContentSend_P (PSTR (".title {font-size:5vh;}\n"));
  WSContentSend_P (PSTR (".info {margin:50px auto;color:#ffbe34;font-size:4vh;font-weight:bold;font-style:italic;}\n"));
  WSContentSend_P (PSTR (".switch {text-align:left;font-size:5vh;}\n"));
  WSContentSend_P (PSTR (".toggle input {display:none;}\n"));
  WSContentSend_P (PSTR (".toggle {position:relative;display:inline-block;width:160px;height:60px;margin:50px auto;}\n"));
  WSContentSend_P (PSTR (".slide-off {position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background-color:#bc2612;border:1px solid #aaa;border-radius:10px;}\n"));
  WSContentSend_P (PSTR (".slide-off:before {position:absolute;content:'';width:70px;height:48px;left:4px;top:5px;background-color:#eee;border-radius:6px;}\n"));
  WSContentSend_P (PSTR (".slide-off:after {position:absolute;content:'Off';top:6px;right:15px;color:#fff;}\n"));
  WSContentSend_P (PSTR (".slide-on {position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background-color:#8cbc13;border:1px solid #aaa;border-radius:10px;}\n"));
  WSContentSend_P (PSTR (".slide-on:before {position:absolute;content:'';width:70px;height:48px;left:84px;top:5px;background-color:#eee;border-radius:6px;}\n"));
  WSContentSend_P (PSTR (".slide-on:after {position:absolute;content:'On';top:6px;left:15px;color:#fff;}\n"));
  WSContentSend_P (PSTR (".offload {background-color:#ffbe34;}\n"));

  WSContentSend_P (PSTR ("</style>\n</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_PAGE_OFFLOAD_CONTROL);

  // room name and current temperature
  WSContentSend_P (PSTR ("<div class='title bold'>%s</div>\n"), SettingsText(SET_FRIENDLYNAME1));

  // information about current offloading
  WSContentSend_P (PSTR ("<div class='info'>%s</div>\n"), str_info.c_str ());

  // if heater is in thermostat mode, button to switch off heater, else button to switch on thermostat
  WSContentSend_P (PSTR ("<div class='centered'><label class='toggle'><input name='%s' type='submit'/><span class='switch %s'></span></label></div>"), str_name.c_str (), str_style.c_str ());

  // end of page
  WSContentSend_P (PSTR ("</form>\n"));
  WSContentStop ();
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
      OffloadEverySecond ();
      break;
    case FUNC_MQTT_DATA:
      result = OffloadMqttData ();
      break;
    case FUNC_COMMAND:
      result = OffloadMqttCommand ();
      break;
    case FUNC_EVERY_250_MSECOND:
      OffloadUpdateStatus ();
      break;
    case FUNC_EVERY_SECOND:
      OffloadEverySecond ();
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
      OffloadShowJSON (true);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      Webserver->on ("/" D_PAGE_OFFLOAD_CONFIG, OffloadWebPageConfig);
      if (offload_relay_managed == true) Webserver->on ("/" D_PAGE_OFFLOAD_CONTROL, OffloadWebPageControl);
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      if (offload_relay_managed == true) OffloadWebControlButton ();
      break;
    case FUNC_WEB_ADD_BUTTON:
      OffloadWebConfigButton ();
      break;
    case FUNC_WEB_SENSOR:
      OffloadWebSensor ();
      break;
#endif  // USE_WEBSERVER

  }
  
  return result;
}

#endif // USE_OFFLOADING
