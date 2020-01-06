/*
  xdrv_98_pilotwire.ino - French Pilot Wire (Fil Pilote) support 
  for Sonoff Basic or Sonoff Dual R2
  
  Copyright (C) 2019  Nicolas Bernaerts
    05/07/2019 - v3.0 - Add max power management with automatic offload
                        Save power settings in Settings.energy... variables
    30/12/2019 - v4.0 - Switch settings to free_f03 for Tasmota 8.x compatibility
    06/01/2019 - v4.1 - Handle offloading with finite state machine
                      
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

#ifdef USE_PILOTWIRE
#define XDRV_98               98

/*************************************************\
 *               Variables
\*************************************************/

// offloading stages
enum OffloadingStages { OFFLOADING_NONE, OFFLOADING_BEFORE, OFFLOADING_ACTIVE, OFFLOADING_AFTER };

// variables
uint8_t  offloading_state           = OFFLOADING_NONE;    // current offloading state
uint16_t offloading_counter         = 0;                  // message counter before or after offloading
uint16_t pilotwire_house_power      = 0;                  // last total house power retrieved thru MQTT
bool     pilotwire_topic_subscribed = false;              // flag for power subscription

/**************************************************\
 *                  Accessors
\**************************************************/

// get maximum power limit of house contract
uint16_t PilotwireMqttGetContract ()
{
  return Settings.energy_max_power_limit;
}

// set maximum power limit of house contract
void PilotwireMqttSetContract (uint16_t new_power)
{
  Settings.energy_max_power_limit = new_power;
}

// get power of heater
uint16_t PilotwireGetHeaterPower ()
{
  return Settings.energy_max_power;
}

// set power of heater
void PilotwireSetHeaterPower (uint16_t new_power)
{
  Settings.energy_max_power = new_power;
}

// get number of power messages to receive before offloading
uint16_t PilotwireMqttGetPriorityBeforeOffload ()
{
  return Settings.energy_max_power_limit_window;;
}

// set number of power messages to receive before offloading
void PilotwireMqttSetPriorityBeforeOffload (uint16_t new_priority)
{
  Settings.energy_max_power_limit_window = new_priority;
}

// get number of power messages to receive before removing offload
uint16_t PilotwireMqttGetPriorityAfterOffload ()
{
  return Settings.energy_max_power_limit_hold;;
}

// set number of power messages to receive before removing offload
void PilotwireMqttSetPriorityAfterOffload (uint16_t new_priority)
{
  Settings.energy_max_power_limit_hold = new_priority;
}

// get current total power MQTT topic
void PilotwireMqttGetHouseTopic (String& str_topic)
{
  String str_meter;
  int position;

  str_meter = (char*)Settings.free_f03;
  position = str_meter.indexOf(';');
  if (position == -1) str_topic = str_meter;
  else str_topic = str_meter.substring(0, position);
}

// get current total power JSON key
void PilotwireMqttGetHouseKey (String& str_key)
{
  String str_meter;
  int position;

  str_meter = (char*)Settings.free_f03;
  position = str_meter.indexOf(';');
  if (position == -1) str_key = "";
  else str_key = str_meter.substring(position +1);
}


// set current total power MQTT topic
void PilotwireMqttSetHouseTopic (char* new_topic)
{
  String str_meter, str_key;

  // get key
  PilotwireMqttGetHouseKey (str_key);

  // save the full meter topic and key
  str_meter = new_topic;
  str_meter += ";";
  str_meter += str_key;
  strncpy ((char*)Settings.free_f03, str_meter.c_str (), 128);
}

// set current total power JSON key
void PilotwireMqttSetHouseKey (char* new_key)
{
  String str_meter, str_topic;

  // get key
  PilotwireMqttGetHouseTopic (str_topic);

  // save the full meter topic and key
  str_meter = str_topic;
  str_meter += ";";
  str_meter += new_key;
  strncpy ((char*)Settings.free_f03, str_meter.c_str (), 128);
}

/**************************************************\
 *                  Functions
\**************************************************/

// update offloading status
void PilotwireUpdateOffloadingStatus ()
{
  uint16_t house_contract, heater_power;

  // get MQTT power data
  house_contract = PilotwireMqttGetContract ();
  heater_power   = PilotwireGetHeaterPower ();

  // if house contract and heater power are defined
  if ((house_contract > 0) && (heater_power > 0))
  {
    // switch according to current state
    switch (offloading_state)
    { 
      case OFFLOADING_NONE:
        // if overload is detected
        if (pilotwire_house_power > house_contract)
        {
          // set status to before offloading
          offloading_counter = PilotwireMqttGetPriorityBeforeOffload ();
          if (offloading_counter > 0) offloading_state = OFFLOADING_BEFORE;
          else offloading_state = OFFLOADING_ACTIVE;
        }
        break;
      case OFFLOADING_BEFORE:
          // if house power has gone down, remove offloading countdown
          if (pilotwire_house_power > house_contract) offloading_state = OFFLOADING_NONE;
        
          // else, decrement message counter to activate offloading
          else
          {
            offloading_counter--;
            if (offloading_counter == 0) offloading_state = OFFLOADING_ACTIVE;
          }
        break;
      case OFFLOADING_ACTIVE:
        if (pilotwire_house_power <= house_contract - heater_power)
        {
          // set status to after offloading
          offloading_counter = PilotwireMqttGetPriorityAfterOffload ();
          if (offloading_counter > 0) offloading_state = OFFLOADING_AFTER;
          else offloading_state = OFFLOADING_NONE;
        }
        break;
      case OFFLOADING_AFTER:
        // if house power has gone again too high, offloading back to active state
        if (pilotwire_house_power > house_contract - heater_power) offloading_state = OFFLOADING_ACTIVE;
        
        // else, decrement message counter to remove offloading
        else
        {
          offloading_counter--;
          if (offloading_counter == 0) offloading_state = OFFLOADING_NONE;
        }
        break;
    }
  }
}

// check and update MQTT power subsciption after disconnexion
void PilotwireMqttCheckConnexion ()
{
  bool   is_connected;
  String str_topic;

  // if connected to MQTT server
  is_connected = MqttIsConnected();
  if (is_connected)
  {
    // if still no subsciption to power topic
    if (!pilotwire_topic_subscribed)
    {
      // check power topic availability
      PilotwireMqttGetHouseTopic (str_topic);
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
  // update instant power
  pilotwire_house_power = new_power;

  // update offloading status according to new house instant power
  PilotwireUpdateOffloadingStatus ();
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
  PilotwireMqttGetHouseKey (str_power_key);
  PilotwireMqttGetHouseTopic (str_power_topic);

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
