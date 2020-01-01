/*
  xdrv_98_pilotwire.ino - French Pilot Wire (Fil Pilote) support 
  for Sonoff Basic or Sonoff Dual R2
  
  Copyright (C) 2019  Nicolas Bernaerts
    05/07/2019 - v3.0 - Add max power management with automatic offload
                        Save power settings in Settings.energy... variables
    30/12/2019 - v4.0 - Switch settings to free_f03 for Tasmota 8.x compatibility
                      
  Settings are stored using weighting scale parameters :
    - Settings.energy_max_power_limit       = Maximum power of the house contract (W) 
    - Settings.energy_max_power             = Heater power (W) 
    - Settings.energy_max_power_limit_hold  = Priority of the heater in the house (1 .. 10)
    - Settings.free_f03                     = Total power (MQTT topic;JSON key)

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

#ifdef USE_PILOTWIRE
#define XDRV_98               98

#define PILOTWIRE_PRIORITY_MIN          1
#define PILOTWIRE_PRIORITY_MAX          10

/*************************************************\
 *               Variables
\*************************************************/

// variables
bool     pilotwire_topic_subscribed = false;        // flag for power subscription
bool     pilotwire_offloaded        = false;        // flag of offloaded state
uint16_t pilotwire_house_power      = 0;            // last total house power retrieved thru MQTT
uint32_t pilotwire_priority_count   = 0;            // number of power data reveived since offload removed (used by priority)

/**************************************************\
 *                  Accessors
\**************************************************/

// get maximum power limit of house contract
uint16_t PilotwireMqttGetContract ()
{
  // return Settings value
  return Settings.energy_max_power_limit;
}

// set maximum power limit of house contract
void PilotwireMqttSetContract (uint16_t new_power)
{
  // save the value
  Settings.energy_max_power_limit = new_power;
}

// get power of heater
uint16_t PilotwireGetHeaterPower ()
{
  // return Settings value
  return Settings.energy_max_power;
}

// set power of heater
void PilotwireSetHeaterPower (uint16_t new_power)
{
  // save the value
  Settings.energy_max_power = new_power;
}

// get heater priority in the house
uint8_t PilotwireMqttGetPriority ()
{
  uint8_t priority;

  // read actual VMC mode
  priority = (uint8_t)Settings.energy_max_power_limit_hold;

  // if outvalue, set to 1
  if (priority < PILOTWIRE_PRIORITY_MIN) priority = PILOTWIRE_PRIORITY_MIN;
  if (priority > PILOTWIRE_PRIORITY_MAX) priority = PILOTWIRE_PRIORITY_MAX;

  return priority;
}

// set heater priority in the house
void PilotwireMqttSetPriority (uint8_t new_priority)
{
  // if outvalue, set to 1
  if (new_priority < PILOTWIRE_PRIORITY_MIN) new_priority = PILOTWIRE_PRIORITY_MIN;
  if (new_priority > PILOTWIRE_PRIORITY_MAX) new_priority = PILOTWIRE_PRIORITY_MAX;

  // write to settings
  Settings.energy_max_power_limit_hold = (uint16_t)new_priority;
}

// get current total power MQTT topic
void PilotwireMqttGetTopic (String& str_topic)
{
  String str_meter;
  int position;

  str_meter = (char*)Settings.free_f03;
  position = str_meter.indexOf(';');
  if (position == -1) str_topic = str_meter;
  else str_topic = str_meter.substring(0, position);
}

// get current total power JSON key
void PilotwireMqttGetJsonKey (String& str_key)
{
  String str_meter;
  int position;

  str_meter = (char*)Settings.free_f03;
  position = str_meter.indexOf(';');
  if (position == -1) str_key = "";
  else str_key = str_meter.substring(position +1);
}


// set current total power MQTT topic
void PilotwireMqttSetTopic (char* new_topic)
{
  String str_meter, str_key;

  // get key
  PilotwireMqttGetJsonKey (str_key);

  // save the full meter topic and key
  str_meter = new_topic;
  str_meter += ";";
  str_meter += str_key;
  strncpy ((char*)Settings.free_f03, str_meter.c_str (), 100);
}

// set current total power JSON key
void PilotwireMqttSetJsonKey (char* new_key)
{
  String str_meter, str_topic;

  // get key
  PilotwireMqttGetTopic (str_topic);

  // save the full meter topic and key
  str_meter = str_topic;
  str_meter += ";";
  str_meter += new_key;
  strncpy ((char*)Settings.free_f03, str_meter.c_str (), 100);
}

/**************************************************\
 *                  Functions
\**************************************************/

// check and update MQTT power subsciption after disconnexion
void PilotwireMqttCheckConnexion ()
{
  bool  is_connected;
  String str_topic;

  // check MQTT connexion
  is_connected = MqttIsConnected();

  // if connected to MQTT server
  if (is_connected)
  {
    // if still no subsciption to power topic
    if (!pilotwire_topic_subscribed)
    {
      // check power topic availability
      PilotwireMqttGetTopic (str_topic);
      if (str_topic.length () > 0) 
      {
        // subscribe to power meter
        MqttSubscribe(str_topic.c_str ());

        // subscription done
        pilotwire_topic_subscribed = true;

        // log
        AddLog_P2(LOG_LEVEL_INFO, PSTR("MQT: Subscribed to %s"), str_topic.c_str ());
      }
    }
  }
  else pilotwire_topic_subscribed = false;
}

// update instant house power
void PilotwireMqttUpdateHousePower (uint16_t new_power)
{
  uint16_t heater_power, house_contract;
  uint8_t  heater_priority;

  // update instant power
  pilotwire_house_power = new_power;

  // get MQTT power data
  house_contract = PilotwireMqttGetContract ();
  heater_power   = PilotwireGetHeaterPower ();
  heater_priority = PilotwireMqttGetPriority ();

  // if house contract and heater power are defined
  if ((house_contract > 0) && (heater_power > 0))
  {
    // if power consumption exceeds house contract, offload heater
    if (pilotwire_house_power > house_contract)
    {
      pilotwire_offloaded      = true;
      pilotwire_priority_count = 0;
    }

    // else, if instant power is low enough, increase counter used for restart priority
    else if (pilotwire_house_power <= house_contract - heater_power) pilotwire_priority_count ++;

    // if heater offloaded and restart counter high enought, remove offload
    if ((pilotwire_offloaded == true) && (pilotwire_priority_count >= heater_priority)) pilotwire_offloaded = false;
  }
}

// read received MQTT data to retrieve house instant power
bool PilotwireMqttData ()
{
  bool    data_handled = false;
  int     idx_value;
  String  str_power_topic, str_power_key;
  String  str_mailbox_topic, str_mailbox_data, str_mailbox_value;

  // get topics to compare
  str_mailbox_topic = XdrvMailbox.topic;
  PilotwireMqttGetJsonKey (str_power_key);
  PilotwireMqttGetTopic (str_power_topic);

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
      // 
       str_power_key += ":";
      idx_value = str_mailbox_data.indexOf (str_power_key);
      if (idx_value >= 0) idx_value = str_mailbox_data.indexOf (':', idx_value + 1);
      if (idx_value >= 0) str_mailbox_value = str_mailbox_data.substring (idx_value + 1);
    }

    // else, no power key provided, data holds the value
    else str_mailbox_value = str_mailbox_data;

    // convert and update instant power
    PilotwireMqttUpdateHousePower (str_mailbox_value.toInt ());

    // data from message has been handled
    data_handled = true;
  }

  return data_handled;
}

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xdrv98 (uint8_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
    case FUNC_MQTT_INIT:
      PilotwireMqttCheckConnexion ();
      break;
    case FUNC_MQTT_DATA:
      result = PilotwireMqttData ();
      break;
    case FUNC_EVERY_SECOND:
      PilotwireMqttCheckConnexion ();
      break;
  }
  
  return result;
}

#endif // USE_PILOTWIRE
