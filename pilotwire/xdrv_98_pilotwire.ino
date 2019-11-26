/*
  xdrv_98_pilotwire.ino - French Pilot Wire (Fil Pilote) support 
  for Sonoff Basic or Sonoff Dual R2
  
  Copyright (C) 2019  Nicolas Bernaerts

    05/07/2019 - v3.0 - Add max power management with automatic offload
                        Save power settings in Settings.energy... variables
                       
  Settings are stored using weighting scale parameters :
    - Settings.energy_max_power_limit       = Maximum power of the house contract (W) 
    - Settings.energy_max_power             = Heater power (W) 
    - Settings.energy_max_power_limit_hold  = Priority of the heater in the house (1 .. 5)
    - Settings.free_1D5                     = Total power JSON key 
    - Settings.free_73D                     = Total power MQTT topic
 
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

#ifdef USE_PILOTWIRE

/*********************************************************************************************\
 * Fil Pilote
\*********************************************************************************************/

#define XDRV_98    98

/*************************************************\
 *               Variables
\*************************************************/

// variables
bool     pilotwire_topic_subscribed = false;        // flag for power subscription
bool     pilotwire_offloaded        = false;        // flag of offloaded state
uint16_t pilotwire_house_power      = 0;            // last total house power retrieved thru MQTT
uint32_t pilotwire_meter_count      = 0;            // number of house power data reveived where heater is allowed on

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
  if (priority > 5) priority = 5;

  return priority;
}

// set heater priority in the house
void PilotwireMqttSetPriority (uint8_t new_priority)
{
  // if outvalue, set to 1
  if (new_priority > 5) new_priority = 5;

  // write to settings
  Settings.energy_max_power_limit_hold = (uint16_t)new_priority;
}

// get current total power JSON key
char* PilotwireMqttGetJsonKey ()
{
  char* json_key = NULL;

  // if topic is defined, return string
  if (strlen ((char*)Settings.free_1D5) > 0) json_key = (char*)Settings.free_1D5;

  // return the key
  return json_key;
}

// set current total power JSON key
void PilotwireMqttSetJsonKey (char* new_key)
{
  // save the key
  if (new_key == NULL) strcpy ((char*)Settings.free_1D5, "");
  else strncpy ((char*)Settings.free_1D5, new_key, 7);
}

// get current total power MQTT topic
char* PilotwireMqttGetTopic ()
{
  char* power_topic = NULL;

  // if topic is defined, return string
  if (strlen ((char*)Settings.free_73D) > 0) power_topic = (char*)Settings.free_73D;

  return power_topic;
}

// set current total power MQTT topic
void PilotwireMqttSetTopic (char* new_topic)
{
  // save the topic
  if (new_topic == NULL) strcpy ((char*)Settings.free_73D, "");
  else strncpy ((char*)Settings.free_73D, new_topic, 80);
}

/**************************************************\
 *                  Functions
\**************************************************/

void PilotwireMqttUpdateHousePower (uint16_t new_power)
{
  uint16_t house_contract, heater_power;
  
  // update instant power
  pilotwire_house_power = new_power;

  // get house contract and heater power
  house_contract = PilotwireMqttGetContract ();
  heater_power   = PilotwireGetHeaterPower ();

  // if house contract and heater power are defined, heater is offloaded and instant power is low enought, increase meter update counter
  if ((house_contract > 0) && (heater_power > 0) && (pilotwire_offloaded == true) && (pilotwire_house_power < house_contract - heater_power)) pilotwire_meter_count ++;
}

void PilotwireMqttEverySecond ()
{
  bool  is_connected;
  char* power_topic;

  // check MQTT connexion
  is_connected = MqttIsConnected();

  // if connected to MQTT server
  if (is_connected)
  {
    // if still no subsciption to power topic
    if (!pilotwire_topic_subscribed)
    {
      // check power topic availability
      power_topic = PilotwireMqttGetTopic ();
      if (power_topic != NULL) 
      {
        // subscribe to power meter
        MqttSubscribe(power_topic);

        // subscription done
        pilotwire_topic_subscribed = true;

        // log
        AddLog_P2(LOG_LEVEL_INFO, PSTR("MQT: Subscribed to %s"), power_topic);
      }
    }
  }
  else pilotwire_topic_subscribed = false;
}

bool PilotwireMqttData ()
{
  bool     data_handled = false;
  int      topic_found;
  uint8_t  value_length;
  uint16_t actual_power;
  char     power_data[16];
  char*    power_topic;
  char*    json_key;
  char*    pos_key;
  char*    pos_value;

  // check power topic availability
  power_topic = PilotwireMqttGetTopic ();
  if (power_topic != NULL)
  {
    // compare current topic to power topic
    topic_found = strcmp (power_topic, XdrvMailbox.topic);
    if (topic_found == 0)
    {
      // if JSON key is defined
      json_key = PilotwireMqttGetJsonKey ();
      if (json_key != NULL)
      {
        // find the value
        pos_value = NULL;
        pos_key   = strstr (XdrvMailbox.data, json_key);
        if (pos_key != NULL) pos_value = strchr (pos_key, ':');
        if (pos_value != NULL) pos_value++;
      }

      // else value is the data string
      else pos_value = XdrvMailbox.data;

      // get number of digits, extract power value and convert it
      value_length = strspn (pos_value, "0123456789");
      strncpy (power_data, pos_value, value_length);

      // set actual power
      actual_power = atoi (power_data);
      PilotwireMqttUpdateHousePower (actual_power);

      // data from message has been handled
      data_handled = true;

      // log
      AddLog_P2(LOG_LEVEL_INFO, PSTR("Actual power : %d"), pilotwire_house_power);
    }
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
      AddLog_P2(LOG_LEVEL_INFO, PSTR("Xdrv98 - FUNC_MQTT_INIT"));
      PilotwireMqttEverySecond ();
      break;
    case FUNC_MQTT_DATA:
      result = PilotwireMqttData ();
      break;
  }
  
  return result;
}

#endif // USE_PILOTWIRE
