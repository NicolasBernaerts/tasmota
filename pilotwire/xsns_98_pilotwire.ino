/*
  xsns_98_pilotwire.ino - French Pilot Wire (Fil Pilote) support 
  for Sonoff Basic or Sonoff Dual R2
  
  Copyright (C) 2019  Nicolas Bernaerts

    05/04/2019 - v1.0 - Creation
    12/04/2019 - v1.1 - Save settings in Settings.weight... variables
    10/06/2019 - v2.0 - Complete rewrite to add web management
    25/06/2019 - v2.1 - Add DHT temperature sensor and settings validity control
    05/07/2019 - v2.2 - Add embeded icon
    05/07/2019 - v3.0 - Add max power management with automatic offload
                        Save power settings in Settings.energy... variables
                       
  Settings are stored using weighting scale parameters :
    - Settings.weight_reference       = Fil pilote mode
    - Settings.weight_max             = Target temperature x10 (192 = 19.2°C)
    - Settings.weight_calibration     = Temperature correction (0 = -5°C, 50 = 0°C, 100 = +5°C)
    - Settings.energy_max_power       = Heater power (W) 
    
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

#define XSNS_98                         98

/*************************************************\
 *               Variables
\*************************************************/

#define PILOTWIRE_BUFFER_SIZE           128

#define D_PAGE_PILOTWIRE                "pw"
#define D_CMND_PILOTWIRE_MODE           "mode"
#define D_CMND_PILOTWIRE_OFFLOAD        "offload"
#define D_CMND_PILOTWIRE_TARGET         "target"
#define D_CMND_PILOTWIRE_DRIFT          "drift"
#define D_CMND_PILOTWIRE_POWER          "power"
#define D_CMND_PILOTWIRE_PRIORITY       "priority"
#define D_CMND_PILOTWIRE_CONTRACT       "contract"
#define D_CMND_PILOTWIRE_MQTTTOPIC      "topic"
#define D_CMND_PILOTWIRE_JSONKEY        "key"

#define D_JSON_PILOTWIRE                "Pilotwire"
#define D_JSON_PILOTWIRE_MODE           "Mode"
#define D_JSON_PILOTWIRE_LABEL          "Label"
#define D_JSON_PILOTWIRE_OFFLOAD        "Offload"
#define D_JSON_PILOTWIRE_TARGET         "Target"
#define D_JSON_PILOTWIRE_TEMPERATURE    "Temperature"
#define D_JSON_PILOTWIRE_RELAY          "Relay"
#define D_JSON_PILOTWIRE_STATE          "State"
#define D_JSON_PILOTWIRE_DRIFT          "Drift"

#define PILOTWIRE_COLOR_BUFFER_SIZE     8
#define PILOTWIRE_LABEL_BUFFER_SIZE     16
#define PILOTWIRE_MESSAGE_BUFFER_SIZE   64
#define PILOTWIRE_OFFLOAD_TIMEOUT       300000       // 5 mn

#define PILOTWIRE_TEMP_MIN              5
#define PILOTWIRE_TEMP_MAX              30
#define PILOTWIRE_DRIFT_MIN             -5
#define PILOTWIRE_DRIFT_MAX             5
#define PILOTWIRE_THRESHOLD             0.5

// icon coded in base64
const char strIcon0[] PROGMEM = "iVBORw0KGgoAAAANSUhEUgAAACUAAAAgCAQAAAA/Wnk7AAAC3npUWHRSYXcgcHJvZmlsZSB0eXBlIGV4aWYAAHja7ZdBkhshDEX3nCJHQBJC4jh0A1W5QY6fD83Y8YwzVZlkkYWhDLRafEAP6HLoP76P8A2JPHJIap5LzhEplVS4ouHxSldNMa1yJdb9jh7tgbc9MkyCWq7H3Ld/hV3vHSxt+/FoD3ZuHd9C+8WboMyR52jbz7eQ8GWn/RzK7lfTL8vZv2FrzEjb6f1zMgSjKfSEA3chiSh9jiKYgRSpqDNKkek0bbNdVunPYxduzXfBa+157GLdHvIYihDzdsjvYrTtpM9jtyL064zorcmPL4bEN9AfYzeaj9Gv1dWUEakc9qLillgtOB4IpaxuGdnwU7Rt5YLsWOIJYg00D+QzUCFGtAclalRpUF/1SSemmLizoWY+WZbNxbjwuQCkmWmwAUML4mB1gprAzLe50Bq3rPFOHIEWG8GTCWKEHh9yeGb8Sr4JjTG3LtEMJtDTBZjnnsY0JrlZwgtAaOyY6orvyuGG9Z5o7cIEtxlmxwJrPC6JQ+m+t2RxnnQ1phCvo0HWtgBChLEVkyEBgZhJlDJFYzYixNHBp2LmLIkPECBVbhQG2IhkwHGeY6OP0fJl5cuMqwUgFIfGgAYHBbBSUuwfS449VFU0BVXNaupatGbJKWvO2fK8o6qJJVPLZuZWrLp4cvXs5u7Fa+EiuMK05GK";
const char strIcon1[] PROGMEM = "heCmlVgxaIV3Ru8Kj1oMPOdKhRz7s8KMc9cT2OdOpZz7t9LOctXGThuPfcrPQvJVWO3VspZ669tytey+9Duy1ISMNHXnY8FFGvVHbVB+p0Ttyn1OjTW0SS8vP7tRgNnuToHmd6GQGYpwIxG0SwIbmySw6pcST3GQWC+NQKIMa6YTTaBIDwdSJddCN3Z3cp9yCpj/ixr8jFya6f0EuTHSb3EduT6i1ur4osgDNUzhjGmXgYoND98pe5zfpy3X4W4GX0EvoJfQSegm9hP4XoTFGaPjLFH4CjgdTc9MWax8AAAACYktHRAD/h4/MvwAAAAlwSFlzAAAX3AAAF9wBGQRXVgAAAAd0SU1FB+MGHRcnJmCWOwMAAAF8SURBVEjH7dYxSFRxHAfwj8+TQ0RDKaihpXKwzRBzC4ISbMtZqCUEHcNBAhHMhiaX0IaWHCTMzUFxiUMiMPAw4sAhsKEWtUHJojyHe955ouD9741+3/D+/z/vffi99/68/59Crnnri5zPevDcmpxVfRiQlZM1gD6rcta8dsUpabUlHx8jWInbk5iJ2zOYLF614fJxJAXGNassVz0z6IYpTfHIuwJ1X+W5g0fuFvu3ItAQQNWj7kg/iiSWlIe6hYCXTLldTk2rDyqiyZPygcgbm0HUvm175dSgi/4HUN+0mCinqsm8nWJ7LlUVldGYVFWSe8Bz6pw6jXovrzbgzuvytszqKFH3qiikWa9P+g+pB17IBzC/vD";
const char strIcon2[] PROGMEM = "Tnr8irwi8wkjFsP4DaNKRXpx01nibx2rNm0ZXMF/yKC6UltZpMu2mpRO0W19iz53d8/uHx0Sm6GFDNh5Nn+7DtCqHvxk7efqzrMKpdyq4VLEirs2cZGW3S/shgWZe0fz4a8fM4dQDPsF31pJ5WwwAAAABJRU5ErkJggg==";
const char *const arrIconBase64[] PROGMEM = {strIcon0, strIcon1, strIcon2};

// fil pilote modes
enum PilotWireModes { PILOTWIRE_DISABLED, PILOTWIRE_OFF, PILOTWIRE_COMFORT, PILOTWIRE_ECO, PILOTWIRE_FROST, PILOTWIRE_THERMOSTAT, PILOTWIRE_OFFLOAD };

// fil pilote commands
enum PilotWireCommands { CMND_PILOTWIRE_MODE, CMND_PILOTWIRE_OFFLOAD, CMND_PILOTWIRE_TARGET, CMND_PILOTWIRE_DRIFT, CMND_PILOTWIRE_POWER, CMND_PILOTWIRE_PRIORITY, CMND_PILOTWIRE_CONTRACT, CMND_PILOTWIRE_MQTTTOPIC, CMND_PILOTWIRE_JSONKEY };
const char kPilotWireCommands[] PROGMEM = D_CMND_PILOTWIRE_MODE "|" D_CMND_PILOTWIRE_OFFLOAD "|" D_CMND_PILOTWIRE_TARGET "|" D_CMND_PILOTWIRE_DRIFT "|" D_CMND_PILOTWIRE_POWER "|" D_CMND_PILOTWIRE_PRIORITY "|" D_CMND_PILOTWIRE_CONTRACT "|" D_CMND_PILOTWIRE_MQTTTOPIC "|" D_CMND_PILOTWIRE_JSONKEY;

// variables
//ulong offload_start = 0;            // time of last offload command

char mqtt_text[MQTT_MAX_PACKET_SIZE];

/**************************************************\
 *                  Accessors
\**************************************************/

// get label according to state
char* PilotwireGetStateLabel (uint8_t state)
{
  char* label = NULL;
    
  // get label
  switch (state)
  {
   case PILOTWIRE_DISABLED:         // Disabled
     label = D_PILOTWIRE_DISABLED;
     break;
   case PILOTWIRE_OFF:              // Off
     label = D_PILOTWIRE_OFF;
     break;
   case PILOTWIRE_COMFORT:          // Comfort
     label = D_PILOTWIRE_COMFORT;
     break;
   case PILOTWIRE_ECO:              // Economy
     label = D_PILOTWIRE_ECO;
     break;
   case PILOTWIRE_FROST:            // No frost
     label = D_PILOTWIRE_FROST;
     break;
   case PILOTWIRE_THERMOSTAT:       // Thermostat
     label = D_PILOTWIRE_THERMOSTAT;
     break;
   case PILOTWIRE_OFFLOAD:          // Offloaded
     label = D_PILOTWIRE_OFFLOAD;
     break;
  }
  
  return label;
}

// get pilot wire state from relays state
uint8_t PilotwireGetRelayState ()
{
  uint8_t relay1 = 0;
  uint8_t relay2 = 0;
  uint8_t state  = 0;
  
  // read relay states
  relay1 = bitRead (power, 0);
  if (devices_present > 1) relay2 = bitRead (power, 1);

  // convert to pilotwire state
  if (relay1 == 0 && relay2 == 0) state = PILOTWIRE_COMFORT;
  else if (relay1 == 0 && relay2 == 1) state = PILOTWIRE_OFF;
  else if (relay1 == 1 && relay2 == 0) state = PILOTWIRE_FROST;
  else if (relay1 == 1 && relay2 == 1) state = PILOTWIRE_ECO;

  // if one relay device, convert no frost to off
  if (devices_present == 1)
  {
    if (state == PILOTWIRE_FROST) state = PILOTWIRE_OFF;
    if (state == PILOTWIRE_ECO)   state = PILOTWIRE_COMFORT;
  }
  
  return state;
}

// set relays state
void PilotwireSetRelayState (uint8_t new_state)
{
  // handle 1 relay device state conversion
  if (devices_present == 1)
  {
    if (new_state == PILOTWIRE_ECO) new_state = PILOTWIRE_COMFORT;
    else if (new_state == PILOTWIRE_OFF) new_state = PILOTWIRE_FROST;
  }

  // pilot relays
  switch (new_state)
  {
    case PILOTWIRE_OFF:  // Set Off
      ExecuteCommandPower (1, POWER_OFF, SRC_IGNORE);
      if (devices_present > 1) ExecuteCommandPower (2, POWER_ON, SRC_IGNORE);
      break;
    case PILOTWIRE_COMFORT:  // Set Comfort
      ExecuteCommandPower (1, POWER_OFF, SRC_IGNORE);
      if (devices_present > 1) ExecuteCommandPower (2, POWER_OFF, SRC_IGNORE);
      break;
    case PILOTWIRE_ECO:  // Set Economy
      ExecuteCommandPower (1, POWER_ON, SRC_IGNORE);
      if (devices_present > 1) ExecuteCommandPower (2, POWER_ON, SRC_IGNORE);
      break;
    case PILOTWIRE_FROST:  // Set No Frost
      ExecuteCommandPower (1, POWER_ON, SRC_IGNORE);
      if (devices_present > 1) ExecuteCommandPower (2, POWER_OFF, SRC_IGNORE);
      break;
  }
}

// get pilot actual mode
uint8_t PilotwireGetMode ()
{
  uint8_t actual_mode;

  // read actual VMC mode
  actual_mode = (uint8_t) Settings.weight_reference;

  // if outvalue, set to disabled
  if (actual_mode > PILOTWIRE_OFFLOAD) actual_mode = PILOTWIRE_DISABLED;

  return actual_mode;
}

// set pilot wire mode
void PilotwireSetMode (uint8_t new_mode)
{
  // reset offloading
  pilotwire_offloaded = false;

  // handle 1 relay device state conversion
  if (devices_present == 1)
  {
    if (new_mode == PILOTWIRE_ECO) new_mode = PILOTWIRE_COMFORT;
    else if (new_mode == PILOTWIRE_FROST) new_mode = PILOTWIRE_OFF;
  }

  // if within range, set mode
  if (new_mode <= PILOTWIRE_OFFLOAD) Settings.weight_reference = (unsigned long) new_mode;
}

// set pilot wire offload mode
void PilotwireSetOffload (char* offload)
{
  // detect offload mode on
  if (strcmp (offload, "1") == 0) pilotwire_offloaded = true;
  else if (strcmp (offload, "ON") == 0) pilotwire_offloaded = true;

  // detect offload mode off
  else if (strcmp (offload, "0") == 0) pilotwire_offloaded = false;
  else if (strcmp (offload, "OFF") == 0) pilotwire_offloaded = false;
}

// get current temperature
float PilotwireGetTemperature ()
{
  float temperature;
  
  // get global temperature
  temperature = global_temperature;

  // if global temperature not defined and ds18b20 sensor present, read it 
#ifdef USE_DS18B20
  if ((temperature == 0) && (ds18b20_temperature != 0)) temperature = ds18b20_temperature;
#endif

  // if global temperature not defined and dht sensor present, read it 
#ifdef USE_DHT
  if ((temperature == 0) && (Dht[0].t != 0)) temperature = Dht[0].t;
#endif

  return temperature;
}

// get current temperature with drift correction
float PilotwireGetTemperatureWithDrift ()
{
  float temperature;
  
  // get current temperature adding drift correction
  temperature = PilotwireGetTemperature () + PilotwireGetDrift ();

  return temperature;
}

// set pilot wire in thermostat mode
void PilotwireSetThermostat (float new_thermostat)
{
  // save target temperature
  if ((new_thermostat >= PILOTWIRE_TEMP_MIN) && (new_thermostat <= PILOTWIRE_TEMP_MAX)) Settings.weight_max = (uint16_t) (new_thermostat * 10);
}

// get target temperature
float PilotwireGetTargetTemperature ()
{
  float temperature;

  // get target temperature (/10)
  temperature = (float) Settings.weight_max;
  temperature = temperature / 10;
  
  // check if within range
  if (temperature < PILOTWIRE_TEMP_MIN) temperature = PILOTWIRE_TEMP_MIN;
  if (temperature > PILOTWIRE_TEMP_MAX) temperature = PILOTWIRE_TEMP_MAX;

  return temperature;
}


// set pilot wire drift temperature
void PilotwireSetDrift (float new_drift)
{
  // if within range, save temperature correction
  if ((new_drift >= PILOTWIRE_DRIFT_MIN) && (new_drift <= PILOTWIRE_DRIFT_MAX)) Settings.weight_calibration = (unsigned long) (50 + (new_drift * 10));
}

// get pilot wire drift temperature
float PilotwireGetDrift ()
{
  float drift;

  // get drift temperature (/10)
  drift = (float) Settings.weight_calibration;
  drift = ((drift - 50) / 10);
  
  // check if within range
  if (drift < PILOTWIRE_DRIFT_MIN) drift = PILOTWIRE_DRIFT_MIN;
  if (drift > PILOTWIRE_DRIFT_MAX) drift = PILOTWIRE_DRIFT_MAX;

  return drift;
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

/******************************************************\
 *                         Functions
\******************************************************/

// Show JSON status (for MQTT)
void PilotwireShowJSON (bool append)
{
  float   drift_temperature;
  float   actual_temperature;
  float   target_temperature;
  uint8_t actual_mode;
  char*   actual_label;

  // start message  -->  {  or  ,
  if (append == false) snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("{"));
  else snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,"), mqtt_data);

  // get mode and temperature
  actual_mode  = PilotwireGetMode ();
  actual_label = PilotwireGetStateLabel (actual_mode);
  drift_temperature  = PilotwireGetDrift ();
  actual_temperature = PilotwireGetTemperatureWithDrift ();
  target_temperature = PilotwireGetTargetTemperature ();

  // pilotwire mode  -->  "PilotWire":{"Relay":2,"Mode":5,"Label":"Thermostat",Offload:"ON","Temperature":20.5,"Target":21}
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_PILOTWIRE "\":{"), mqtt_data);
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_PILOTWIRE_RELAY "\":%d"), mqtt_data, devices_present);
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_PILOTWIRE_MODE "\":%d"), mqtt_data, actual_mode);
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_PILOTWIRE_LABEL "\":\"%s\""), mqtt_data, actual_label);
  if (pilotwire_offloaded == true) snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_PILOTWIRE_OFFLOAD "\":\"%s\""), mqtt_data, "ON");
  else snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_PILOTWIRE_OFFLOAD "\":\"%s\""), mqtt_data, "OFF");
  if (actual_temperature != 0) snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_PILOTWIRE_TEMPERATURE "\":%.1f"), mqtt_data, actual_temperature);
  if (actual_mode == PILOTWIRE_THERMOSTAT)
  {
    snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_PILOTWIRE_TARGET "\":%.1f"), mqtt_data, target_temperature);
    snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_PILOTWIRE_DRIFT "\":%.1f"), mqtt_data, drift_temperature);
  }
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s}"), mqtt_data);

  // if pilot wire mode is enabled
  if (actual_mode != PILOTWIRE_DISABLED)
  {
    // relay state  -->  ,"Relay":{"Mode":4,"Label":"Comfort","Number":number}
    actual_mode  = PilotwireGetRelayState ();
    actual_label = PilotwireGetStateLabel (actual_mode);
    snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_PILOTWIRE_STATE "\":{"), mqtt_data);
    snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_PILOTWIRE_MODE "\":%d"), mqtt_data, actual_mode);
    snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_PILOTWIRE_LABEL "\":\"%s\""), mqtt_data, actual_label);
    snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s}"), mqtt_data);
  }
  
  // if not in append mode, publish message 
  if (append == false)
  { 
    // end of message   ->  }
    snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s}"), mqtt_data);

    // publish full sensor state
    MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));
  }
}

// Handle pilot wire MQTT commands
bool PilotwireCommand ()
{
  int  command_code;
  char command [CMDSZ];

  // check MQTT command
  command_code = GetCommandCode (command, sizeof(command), XdrvMailbox.topic, kPilotWireCommands);

  // handle command
  switch (command_code)
  {
    case CMND_PILOTWIRE_MODE:  // set mode
      PilotwireSetMode (XdrvMailbox.payload);
      break;
    case CMND_PILOTWIRE_OFFLOAD:  // set offloading
      PilotwireSetOffload (XdrvMailbox.data);
      break;
    case CMND_PILOTWIRE_TARGET:  // set target temperature 
      PilotwireSetThermostat (atof (XdrvMailbox.data));
      break;
    case CMND_PILOTWIRE_DRIFT:  // set temperature drift correction 
      PilotwireSetDrift (atof (XdrvMailbox.data));
      break;
  }

  // send MQTT status
  PilotwireShowJSON (false);
  
  return true;
}

// update pilot wire relay states according to current status
void PilotwireEvery250MSecond ()
{
  float    actual_temperature, target_temperature;
  uint8_t  target_mode, heater_state, target_state;
  uint8_t  heater_priority;
  uint16_t house_contract, heater_power;
  ulong    time_now;

  // get house contract heater power and heater priority
  heater_priority = PilotwireMqttGetPriority ();
  house_contract = PilotwireMqttGetContract ();
  heater_power   = PilotwireGetHeaterPower ();

  // if contract and heater power are defined
  if ((house_contract > 0) && (heater_power > 0))
  {
    // if house power is too high, heater should be offloaded
    if (pilotwire_house_power > house_contract)
    {
      // set offload status
      pilotwire_offloaded   = true;
      pilotwire_meter_count = 0;
    }

    // else if heater is candidate to remove offload
    else if (pilotwire_meter_count >= heater_priority) pilotwire_offloaded = false;
  }

  // get target mode
  target_mode = PilotwireGetMode ();

  // if pilotwire mode is enabled
  if (target_mode != PILOTWIRE_DISABLED)
  {
    // if offload mode, target state is off
    if (pilotwire_offloaded == true) target_state = PILOTWIRE_OFF;
 
    // else if thermostat mode
    else if (target_mode == PILOTWIRE_THERMOSTAT)
    {
      // get current and target temperature
      actual_temperature = PilotwireGetTemperatureWithDrift ();
      target_temperature = PilotwireGetTargetTemperature ();

      // if temperature is too low, target state is on
      // else, if too high, target state is off
      target_state = target_mode;
      if (actual_temperature < (target_temperature - PILOTWIRE_THRESHOLD)) target_state = PILOTWIRE_COMFORT;
      else if (actual_temperature > (target_temperature + PILOTWIRE_THRESHOLD)) target_state = PILOTWIRE_OFF;
    }

    // else set mode if needed
    else target_state = target_mode;

    // get heater status
    heater_state = PilotwireGetRelayState ();

    // if heater state different than target state, change state
    if (heater_state != target_state)
    {
      // set relays
      PilotwireSetRelayState (target_state);

      // publish new state
      PilotwireShowJSON (false);
    }
  }
}

/*******************************************************\
 *                      Web server
\*******************************************************/

#ifdef USE_WEBSERVER

// Pilot Wire icon
void PilotwireWebDisplayIcon (uint8_t height)
{
  uint8_t nbrItem, index;

  // display img according to height
  WSContentSend_P ("<img height=%d src='data:image/png;base64,", height);

  // loop to display base64 segments
  nbrItem = sizeof (arrIconBase64) / sizeof (char*);
  for (index=0; index<nbrItem; index++) { WSContentSend_P (arrIconBase64[index]); }

  WSContentSend_P ("'/>");
}

// Pilot Wire mode selection 
void PilotwireWebSelectMode (bool autosubmit)
{
  uint8_t actual_mode;
  float   actual_temperature;
  char    argument[PILOTWIRE_LABEL_BUFFER_SIZE];

  // get mode and temperature
  actual_mode  = PilotwireGetMode ();
  actual_temperature = PilotwireGetTemperature ();

  // selection : beginning
  WSContentSend_P (PSTR ("<select name='%s'"), D_CMND_PILOTWIRE_MODE);
  if (autosubmit == true) WSContentSend_P (PSTR (" onchange='this.form.submit()'"));
  WSContentSend_P (PSTR (">"));
  
  // selection : disabled
  if (actual_mode == PILOTWIRE_DISABLED) strcpy (argument, "selected"); 
  else strcpy (argument, "");
  WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>"), PILOTWIRE_DISABLED, argument, D_PILOTWIRE_DISABLED);

  // selection : comfort
  if (actual_mode == PILOTWIRE_COMFORT) strcpy (argument, "selected");
  else strcpy (argument, "");
  WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>"), PILOTWIRE_COMFORT, argument, D_PILOTWIRE_COMFORT);

  // if dual relay device
  if (devices_present > 1)
  {
    // selection : eco
    if (actual_mode == PILOTWIRE_ECO) strcpy (argument, "selected");
    else strcpy (argument, "");
    WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>"), PILOTWIRE_ECO, argument, D_PILOTWIRE_ECO);
  
    // selection : no frost
    if (actual_mode == PILOTWIRE_FROST) strcpy (argument, "selected");
    else strcpy (argument, "");
    WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>"), PILOTWIRE_FROST, argument, D_PILOTWIRE_FROST);
  }

  // selection : off
  if (actual_mode == PILOTWIRE_OFF) strcpy (argument, "selected");
  else strcpy (argument, "");
  WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>"), PILOTWIRE_OFF, argument, D_PILOTWIRE_OFF);

  // selection : thermostat
  if (actual_temperature != 0) 
  {
    if (actual_mode == PILOTWIRE_THERMOSTAT) strcpy (argument, "selected");
    else strcpy (argument, "");
    WSContentSend_P (PSTR("<option value='%d' %s>%s</option>"), PILOTWIRE_THERMOSTAT, argument, D_PILOTWIRE_THERMOSTAT);
  }

  // selection : end
  WSContentSend_P (PSTR ("</select>"));
}

// Pilot Wire configuration button
void PilotwireWebButton ()
{
  // beginning
  WSContentSend_P (PSTR ("<table style='width:100%%;'><tr>"));

  // vmc icon
  WSContentSend_P (PSTR ("<td align='center'>"));
  PilotwireWebDisplayIcon (32);
  WSContentSend_P (PSTR ("</td>"));

  // button
  WSContentSend_P (PSTR ("<td><form action='%s' method='get'><button>%s</button></form></td>"), D_PAGE_PILOTWIRE, D_PILOTWIRE_CONFIGURE);

  // end
  WSContentSend_P (PSTR ("</tr></table>"));
}

// append pilot wire buttons to main page
void PilotwireWebMainButton ()
{
  // beginning
  WSContentSend_P (PSTR ("<table style='width:100%%;padding:5px 10px;'><tr>"));

  // vmc icon
  WSContentSend_P (PSTR ("<td>"));
  PilotwireWebDisplayIcon (32);
  WSContentSend_P (PSTR ("</td>"));

  // select mode
  WSContentSend_P (PSTR ("<td><form action='%s' method='get'>"), D_PAGE_PILOTWIRE);
  PilotwireWebSelectMode (true);
  WSContentSend_P (PSTR ("</form></td>"));

  // end
  WSContentSend_P (PSTR ("</tr></table>"));
}

// Pilot Wire web page
void PilotwireWebPage ()
{
  uint8_t  target_mode;
  uint8_t  priority_heater;
  uint16_t power_heater, power_limit;
  float    drift_temperature, actual_temperature, target_temperature;
  char     argument[PILOTWIRE_BUFFER_SIZE];
  char*    power_topic;
  char*    json_key;

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // page comes from save button on configuration page
  if (WebServer->hasArg("save"))
  {
    // get pilot wire mode according to 'mode' parameter
    WebGetArg (D_CMND_PILOTWIRE_MODE, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetMode ((uint8_t)atoi (argument)); 

    // get target temperature according to 'target' parameter
    WebGetArg (D_CMND_PILOTWIRE_TARGET, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetThermostat (atof (argument));

    // get temperature drift according to 'drift' parameter
    WebGetArg (D_CMND_PILOTWIRE_DRIFT, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetDrift (atof (argument));

    // get power of heater according to 'power' parameter
    WebGetArg (D_CMND_PILOTWIRE_POWER, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetHeaterPower ((uint16_t)atoi (argument));

    // get priority of heater according to 'priority' parameter
    WebGetArg (D_CMND_PILOTWIRE_PRIORITY, argument, PILOTWIRE_BUFFER_SIZE);
    PilotwireMqttSetPriority ((uint8_t)atoi (argument));

    // get maximum power limit according to 'contract' parameter
    WebGetArg (D_CMND_PILOTWIRE_CONTRACT, argument, PILOTWIRE_BUFFER_SIZE);
    PilotwireMqttSetContract ((uint16_t)atoi (argument));

    // get MQTT topic according to 'topic' parameter
    WebGetArg (D_CMND_PILOTWIRE_MQTTTOPIC, argument, PILOTWIRE_BUFFER_SIZE);
    PilotwireMqttSetTopic (argument);

    // get JSON key according to 'key' parameter
    WebGetArg (D_CMND_PILOTWIRE_JSONKEY, argument, PILOTWIRE_BUFFER_SIZE);
    PilotwireMqttSetJsonKey (argument);

    // back to configuration screen
    HandleConfiguration ();
    return;
  }

  // else, if page comes from mode selection in main page
  else if (WebServer->hasArg(D_CMND_PILOTWIRE_MODE))
  {
    // get pilot wire mode according to 'MODE' parameter
    WebGetArg (D_CMND_PILOTWIRE_MODE, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetMode ((uint8_t)atoi (argument)); 

    // back to previous page
    return;
  }
  
  // else, display configuration page
  else
  {
    // get temperature and target mode
    actual_temperature = PilotwireGetTemperature ();
    target_mode = PilotwireGetMode ();
  
    // beginning of form
    WSContentStart_P (D_PILOTWIRE_CONFIGURE);
    WSContentSendStyle ();

    // form
    WSContentSend_P (PSTR ("<fieldset><legend><b>&nbsp;%s&nbsp;</b></legend><form method='get' action='%s'>"), D_PILOTWIRE_PARAMETERS, D_PAGE_PILOTWIRE);
 
    // temperature section
    WSContentSend_P (PSTR ("<p><b>%s</b></p>"), D_PILOTWIRE_TEMPERATURE);

    // mode selection
    WSContentSend_P (PSTR ("<p>%s<br/>"), D_PILOTWIRE_MODE);
    PilotwireWebSelectMode (false);
    WSContentSend_P (PSTR ("</p>"));

    // if temperature sensor is present
    if (actual_temperature != 0) 
    {
      // target temperature label and input
      target_temperature = PilotwireGetTargetTemperature ();
      WSContentSend_P (PSTR ("<p>%s<br/><input type='number' name='%s' min='%d' max='%d' step='0.5' value='%.2f'></p>"), D_PILOTWIRE_TARGET, D_CMND_PILOTWIRE_TARGET, PILOTWIRE_TEMP_MIN, PILOTWIRE_TEMP_MAX, target_temperature);

      // temperature correction label and input
      drift_temperature  = PilotwireGetDrift ();
      WSContentSend_P (PSTR ("<p>%s<br/><input type='number' name='%s' min='%d' max='%d' step='0.1' value='%.2f'></p>"), D_PILOTWIRE_DRIFT, D_CMND_PILOTWIRE_DRIFT, PILOTWIRE_DRIFT_MIN, PILOTWIRE_DRIFT_MAX, drift_temperature);
    }

    // heater section
    WSContentSend_P (PSTR ("<hr><p><b>%s</b></p>"), D_PILOTWIRE_HEATER);

    // power of heater label and input
    power_heater = PilotwireGetHeaterPower ();
    WSContentSend_P (PSTR ("<p>%s<br/><input type='number' name='%s' value='%d'></p>"), D_PILOTWIRE_POWER, D_CMND_PILOTWIRE_POWER, power_heater);

    // priority of heater label and input
    priority_heater  = PilotwireMqttGetPriority ();
    WSContentSend_P (PSTR ("<p>%s<br/><input type='number' name='%s' min='1' max='5' step='1' value='%d'></p>"), D_PILOTWIRE_PRIORITY, D_CMND_PILOTWIRE_PRIORITY, priority_heater);

    // house section
    WSContentSend_P (PSTR ("<hr><p><b>%s</b></p>"), D_PILOTWIRE_HOUSE);

    // contract power limit label and input
    power_limit = PilotwireMqttGetContract ();
    WSContentSend_P (PSTR ("<p>%s<br/><input type='number' name='%s' value='%d'></p>"), D_PILOTWIRE_CONTRACT, D_CMND_PILOTWIRE_CONTRACT, power_limit);

    // power mqtt topic label and input
    power_topic = PilotwireMqttGetTopic ();
    if (power_topic == NULL) strcpy (argument, "");
    else strcpy (argument, power_topic);
    WSContentSend_P (PSTR ("<p>%s<br/><input name='%s' value='%s'></p>"), D_PILOTWIRE_MQTTTOPIC, D_CMND_PILOTWIRE_MQTTTOPIC, argument);

    // power json key label and input
    json_key = PilotwireMqttGetJsonKey ();
    if (json_key == NULL) strcpy (argument, "");
    else strcpy (argument, json_key);
    WSContentSend_P (PSTR ("<p>%s<br/><input name='%s' value='%s'></p>"), D_PILOTWIRE_JSONKEY, D_CMND_PILOTWIRE_JSONKEY, argument);

    // end of form
    WSContentSend_P(HTTP_FORM_END);

    // configuration button
    WSContentSpaceButton(BUTTON_CONFIGURATION);

    // end of page
    WSContentStop();
  }
}

// append pilot wire state to main page
bool PilotwireWebState ()
{
  uint8_t  actual_mode, actual_state;
  uint16_t contract_power;
  uint32_t current_time;
  float    corrected_temperature, target_temperature;
  char     state_color[PILOTWIRE_COLOR_BUFFER_SIZE];
  char*    actual_label;
  TIME_T   current_dst;

  // if pilot wire is in thermostat mode, display target temperature
  actual_mode  = PilotwireGetMode ();
  if (actual_mode == PILOTWIRE_THERMOSTAT)
  {
    // read temperature
    corrected_temperature = PilotwireGetTemperatureWithDrift ();
    target_temperature = PilotwireGetTargetTemperature ();

    // add it to JSON
    snprintf_P (mqtt_data, sizeof(mqtt_data), "%s<tr><th>%s</th><td><b>%.1f</b> / %.1f°C</td></tr>", mqtt_data, D_PILOTWIRE_BOTHTEMP, corrected_temperature, target_temperature);
  }

  // display power
  contract_power = PilotwireMqttGetContract ();
  snprintf_P (mqtt_data, sizeof(mqtt_data), "%s<tr><td><b>%s</b></td><td><b>%d</b> / %dW</td></tr>", mqtt_data, D_PILOTWIRE_WATT, pilotwire_house_power, contract_power);

  // if pilot wire mode is enabled
  if (actual_mode != PILOTWIRE_DISABLED)
  {
    // get state and label
    if (pilotwire_offloaded == true) actual_state = PILOTWIRE_OFFLOAD;
    else actual_state = PilotwireGetRelayState ();
    actual_label = PilotwireGetStateLabel (actual_state);
    
    // set color according to state
    switch (actual_state)
    {
      case PILOTWIRE_COMFORT:
        strcpy (state_color, "green");
        break;
      case PILOTWIRE_FROST:
        strcpy (state_color, "grey");
        break;
      case PILOTWIRE_ECO:
        strcpy (state_color, "blue");
        break;
      case PILOTWIRE_OFF:
      case PILOTWIRE_OFFLOAD:
        strcpy (state_color, "red");
        break;
      default:
        strcpy (state_color, "black");
    }
 
    // if pilotwire is not disabled, display current state
    if (actual_mode != PILOTWIRE_DISABLED) snprintf_P (mqtt_data, sizeof(mqtt_data), "%s<tr><td colspan=2 style='font-weight:bold; color:%s; text-align: center;'>%s</td></tr>", mqtt_data, state_color, actual_label);
  }

  // dislay current DST time
//  current_time = LocalTime();
//  BreakTime (current_time, current_dst);
//  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR ("%s<tr><th>%s</th><td style='font-weight:bold;'>%02d:%02d:%02d</td></tr>"), mqtt_data, D_PILOTWIRE_TIME, current_dst.hour, current_dst.minute, current_dst.second);
}

#endif  // USE_WEBSERVER

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xsns98 (uint8_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  {
    case FUNC_COMMAND:
      result = PilotwireCommand ();
      break;
    case FUNC_EVERY_250_MSECOND:
      PilotwireEvery250MSecond ();
      break;
    case FUNC_JSON_APPEND:
      PilotwireShowJSON (true);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      WebServer->on ("/" D_PAGE_PILOTWIRE, PilotwireWebPage);
      break;
    case FUNC_WEB_APPEND:
      PilotwireWebState ();
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      PilotwireWebMainButton ();
      break;
    case FUNC_WEB_ADD_BUTTON:
      PilotwireWebButton ();
      break;
#endif  // USE_WEBSERVER

  }
  return result;
}

#endif // USE_PILOTWIRE
