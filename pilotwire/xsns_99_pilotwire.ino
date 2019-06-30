/*
  xsns_99_pilotwire.ino - French Pilot Wire (Fil Pilote) support 
  for Sonoff Basic or Sonoff Dual R2
  
  Copyright (C) 2019 Nicolas Bernaerts

  Settings are stored using weighting scale parameters :
    - Settings.weight_reference   = Fil pilote mode
    - Settings.weight_max         = Target temperature x10 (192 = 19.2°C)
    - Settings.weight_calibration = Temperature correction (0 = -5°C, 50 = 0°C, 100 = +5°C) 
    
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

#define XSNS_99                         99

#define D_PAGE_PILOTWIRE                "pw"
#define D_CMND_PILOTWIRE_MODE           "mode"
#define D_CMND_PILOTWIRE_OFFLOAD        "offload"
#define D_CMND_PILOTWIRE_TARGET         "target"
#define D_CMND_PILOTWIRE_DRIFT          "drift"

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

// fil pilote modes
enum PilotWireModes { PILOTWIRE_DISABLED, PILOTWIRE_OFF, PILOTWIRE_COMFORT, PILOTWIRE_ECO, PILOTWIRE_FROST, PILOTWIRE_THERMOSTAT, PILOTWIRE_OFFLOAD };

// fil pilote commands
enum PilotWireCommands { CMND_PILOTWIRE_MODE, CMND_PILOTWIRE_OFFLOAD, CMND_PILOTWIRE_TARGET, CMND_PILOTWIRE_DRIFT };
const char kPilotWireCommands[] PROGMEM = D_CMND_PILOTWIRE_MODE "|" D_CMND_PILOTWIRE_OFFLOAD "|" D_CMND_PILOTWIRE_TARGET "|" D_CMND_PILOTWIRE_DRIFT;

// variables
ulong offload_start = 0;            // time of last offload command

/*********************************************************************************************/

// get label according to state
char* PilotWireGetStateLabel (uint8_t state)
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
uint8_t PilotWireGetRelayState ()
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
  if ((devices_present == 1) && (state == PILOTWIRE_FROST)) state = PILOTWIRE_OFF;

  return state;
}

// set relays state
void PilotWireSetRelayState (uint8_t new_state)
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
uint8_t PilotWireGetMode ()
{
  uint8_t actual_mode;

  // read actual VMC mode
  actual_mode = (uint8_t) Settings.weight_reference;

  // if outvalue, set to disabled
  if (actual_mode > PILOTWIRE_OFFLOAD) actual_mode = PILOTWIRE_DISABLED;

  return actual_mode;
}

// set pilot wire mode
void PilotWireSetMode (uint8_t new_mode)
{
  // reset offloading
  offload_start = 0;

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
void PilotWireSetOffload (char* offload)
{
  bool    offload_status = false;
  uint8_t device_mode;
  
  // detect offload mode on
  if (strcmp (offload, "1") == 0) offload_status = true;
  else if (strcmp (offload, "ON") == 0) offload_status = true;

  // if offload is set on
  if (offload_status == true)
  {
    // if first trigger, set pilot wire off
    if (offload_start == 0) PilotWireSetRelayState (PILOTWIRE_OFF);
    
    // set start time
    offload_start = millis ();
  }

  // else if set off if needed
  else if (offload_start != 0)
  {
    // get target mode
    device_mode = PilotWireGetMode ();
    
    // if first trigger, set pilot wire off
    PilotWireSetRelayState (device_mode);
    
    // set start time
    offload_start = 0;    
  }
}

// get current temperature
float PilotWireGetTemperature ()
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
float PilotWireGetTemperatureWithDrift ()
{
  float temperature;
  
  // get current temperature adding drift correction
  temperature = PilotWireGetTemperature () + PilotWireGetDrift ();

  return temperature;
}

// set pilot wire in thermostat mode
void PilotWireSetThermostat (float new_thermostat)
{
  // save target temperature
  if ((new_thermostat > 0) && (new_thermostat <= 30)) Settings.weight_max = (uint16_t) (new_thermostat * 10);
}

// get target temperature
float PilotWireGetTargetTemperature ()
{
  float temperature;

  // get target temperature (/10)
  temperature = (float) Settings.weight_max;
  temperature = temperature / 10;
  
  // check if within range
  if (temperature < 0)  temperature = 0;
  if (temperature > 30) temperature = 30;

  return temperature;
}


// set pilot wire drift temperature
void PilotWireSetDrift (float new_drift)
{
  // if within range, save temperature correction
  if ((new_drift >= -5) && (new_drift <= 5)) Settings.weight_calibration = (unsigned long) (50 + (new_drift * 10));
}

// get pilot wire drift temperature
float PilotWireGetDrift ()
{
  float drift;

  // get drift temperature (/10)
  drift = (float) Settings.weight_calibration;
  drift = ((drift - 50) / 10);
  
  // check if within range
  if (drift < -5) drift = -5;
  if (drift > 5)  drift = 5;

  return drift;
}

// Show JSON status (for MQTT)
void PilotWireShowJSON (bool append)
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
  actual_mode  = PilotWireGetMode ();
  actual_label = PilotWireGetStateLabel (actual_mode);
  drift_temperature  = PilotWireGetDrift ();
  actual_temperature = PilotWireGetTemperatureWithDrift ();
  target_temperature = PilotWireGetTargetTemperature ();

  // pilotwire mode  -->  "PilotWire":{"Relay":2,"Mode":5,"Label":"Thermostat",Offload:"ON","Temperature":20.5,"Target":21}
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_PILOTWIRE "\":{"), mqtt_data);
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_PILOTWIRE_RELAY "\":%d"), mqtt_data, devices_present);
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_PILOTWIRE_MODE "\":%d"), mqtt_data, actual_mode);
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_PILOTWIRE_LABEL "\":\"%s\""), mqtt_data, actual_label);
  if (offload_start != 0) snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,\"" D_JSON_PILOTWIRE_OFFLOAD "\":\"%s\""), mqtt_data, "ON");
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
    actual_mode  = PilotWireGetRelayState ();
    actual_label = PilotWireGetStateLabel (actual_mode);
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
bool PilotWireCommand ()
{
  bool serviced = true;
  int  command_code;
  char command [CMDSZ];

  // check MQTT command
  command_code = GetCommandCode (command, sizeof(command), XdrvMailbox.topic, kPilotWireCommands);

  // handle command
  switch (command_code)
  {
    case CMND_PILOTWIRE_MODE:  // set mode
      PilotWireSetMode (XdrvMailbox.payload);
      break;
    case CMND_PILOTWIRE_OFFLOAD:  // set offloading
      PilotWireSetOffload (XdrvMailbox.data);
      break;
    case CMND_PILOTWIRE_TARGET:  // set target temperature 
      PilotWireSetThermostat (atof (XdrvMailbox.data));
      break;
    case CMND_PILOTWIRE_DRIFT:  // set temperature drift correction 
      PilotWireSetDrift (atof (XdrvMailbox.data));
      break;
    default:
      serviced = false;
  }

  // send MQTT status
  if (serviced == true) PilotWireShowJSON (false);
  
  return serviced;
}

// update pilot wire relay states according to current status
void PilotWireEvery250MSecond ()
{
  float   actual_temperature;
  float   target_temperature;
  uint8_t target_mode;
  uint8_t heater_state;
  uint8_t target_state;
  ulong   time_now;

  // get target mode
  target_mode = PilotWireGetMode ();

  // if pilotwire mode is enabled
  if (target_mode != PILOTWIRE_DISABLED)
  {
    // check if offload should be removed
    time_now = millis ();
    if (time_now - offload_start > PILOTWIRE_OFFLOAD_TIMEOUT) offload_start = 0;

    // if offload mode, target state is off
    if (offload_start != 0) target_state = PILOTWIRE_OFF;
 
    // else if thermostat mode
    else if (target_mode == PILOTWIRE_THERMOSTAT)
    {
      // get current and target temperature
      actual_temperature = PilotWireGetTemperatureWithDrift ();
      target_temperature = PilotWireGetTargetTemperature ();

      // if temperature is too low, target state is on
      // else, if too high, target state is off
      target_state = target_mode;
      if (actual_temperature < (target_temperature - 0.5)) target_state = PILOTWIRE_COMFORT;
      else if (actual_temperature > (target_temperature + 0.5)) target_state = PILOTWIRE_OFF;
    }

    // else set mode if needed
    else target_state = target_mode;

    // get heater status
    heater_state = PilotWireGetRelayState ();

    // if heater state different than target state, change state
    if (heater_state != target_state)
    {
      // set relays
      PilotWireSetRelayState (target_state);

      // publish new state
      PilotWireShowJSON (false);
    }
  }
}

#ifdef USE_WEBSERVER

// Pilot Wire icon
void PilotWireWebDisplayIcon (uint8_t height)
{
  WSContentSend_P ("<img height=%d src='data:image/png;base64,", height);

  WSContentSend_P ("iVBORw0KGgoAAAANSUhEUgAAACUAAAAgCAQAAAA/Wnk7AAAC3npUWHRSYXcgcHJvZmlsZSB0eXBlIGV4aWYAAHja7ZdBkhshDEX3nCJHQBJC4jh0A1W5QY6fD83Y8YwzVZlkkYWhDLRafEAP6HLoP76P8A2JPHJIap5LzhEplVS4ouHxSldNMa1yJdb9jh7tgbc9MkyCWq7H3Ld/hV3vHSxt+/FoD3ZuHd9C+8WboMyR52jbz7eQ8GWn/RzK7lfTL8vZv2FrzEjb6f1zMgSjKfSEA3chiSh9jiKYgRSpqDNKkek0bbNdVunPYxduzXfBa+157GLdHvIYihDzdsjvYrTtpM9jtyL064zorcmPL4bEN9AfYzeaj9Gv1dWUEakc9qLillgtOB4IpaxuGdnwU7Rt5YLsWOIJYg00D+QzUCFGtAclalRpUF/1SSemmLizoWY+WZbNxbjwuQCkmWmwAUML4mB1gprAzLe50Bq3rPFOHIEWG8GTCWKEHh9yeGb8Sr4JjTG3LtEMJtDTBZjnnsY0JrlZwgtAaOyY6orvyuGG9Z5o7cIEtxlmxwJrPC6JQ+m+t2RxnnQ1phCvo0HWtgBChLEVkyEBgZhJlDJFYzYixNHBp2LmLIkPECBVbhQG2IhkwHGeY6OP0fJl5cuMqwUgFIfGgAYHBbBSUuwfS449VFU0BVXNaupatGbJKWvO2fK8o6qJJVPLZuZWrLp4cvXs5u7Fa+EiuMK05GK");
  WSContentSend_P ("heCmlVgxaIV3Ru8Kj1oMPOdKhRz7s8KMc9cT2OdOpZz7t9LOctXGThuPfcrPQvJVWO3VspZ669tytey+9Duy1ISMNHXnY8FFGvVHbVB+p0Ttyn1OjTW0SS8vP7tRgNnuToHmd6GQGYpwIxG0SwIbmySw6pcST3GQWC+NQKIMa6YTTaBIDwdSJddCN3Z3cp9yCpj/ixr8jFya6f0EuTHSb3EduT6i1ur4osgDNUzhjGmXgYoND98pe5zfpy3X4W4GX0EvoJfQSegm9hP4XoTFGaPjLFH4CjgdTc9MWax8AAAACYktHRAD/h4/MvwAAAAlwSFlzAAAX3AAAF9wBGQRXVgAAAAd0SU1FB+MGHRcnJmCWOwMAAAF8SURBVEjH7dYxSFRxHAfwj8+TQ0RDKaihpXKwzRBzC4ISbMtZqCUEHcNBAhHMhiaX0IaWHCTMzUFxiUMiMPAw4sAhsKEWtUHJojyHe955ouD9741+3/D+/z/vffi99/68/59Crnnri5zPevDcmpxVfRiQlZM1gD6rcta8dsUpabUlHx8jWInbk5iJ2zOYLF614fJxJAXGNassVz0z6IYpTfHIuwJ1X+W5g0fuFvu3ItAQQNWj7kg/iiSWlIe6hYCXTLldTk2rDyqiyZPygcgbm0HUvm175dSgi/4HUN+0mCinqsm8nWJ7LlUVldGYVFWSe8Bz6pw6jXovrzbgzuvytszqKFH3qiikWa9P+g+pB17IBzC/vD");
  WSContentSend_P ("Tnr8irwi8wkjFsP4DaNKRXpx01nibx2rNm0ZXMF/yKC6UltZpMu2mpRO0W19iz53d8/uHx0Sm6GFDNh5Nn+7DtCqHvxk7efqzrMKpdyq4VLEirs2cZGW3S/shgWZe0fz4a8fM4dQDPsF31pJ5WwwAAAABJRU5ErkJggg==");

  WSContentSend_P ("'/>");
}

// Pilot Wire web page
void PilotWireWebSelectMode (bool autosubmit)
{
  uint8_t actual_mode;
  float   actual_temperature;
  char    argument[PILOTWIRE_LABEL_BUFFER_SIZE];

  // get mode and temperature
  actual_mode  = PilotWireGetMode ();
  actual_temperature = PilotWireGetTemperature ();

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
void PilotWireWebButton ()
{
  // beginning
  WSContentSend_P (PSTR ("<table style='width:100%%;'><tr>"));

  // vmc icon
  WSContentSend_P (PSTR ("<td align='center'>"));
  PilotWireWebDisplayIcon (32);
  WSContentSend_P (PSTR ("</td>"));

  // button
  WSContentSend_P (PSTR ("<td><form action='%s' method='get'><button>%s</button></form></td>"), D_PAGE_PILOTWIRE, D_PILOTWIRE_CONFIGURE);

  // end
  WSContentSend_P (PSTR ("</tr></table>"));
}


// append pilot wire buttons to main page
void PilotWireWebMainButton ()
{
  // beginning
  WSContentSend_P (PSTR ("<table style='width:100%%;padding:5px 10px;'><tr>"));

  // vmc icon
  WSContentSend_P (PSTR ("<td>"));
  PilotWireWebDisplayIcon (32);
  WSContentSend_P (PSTR ("</td>"));

  // select mode
  WSContentSend_P (PSTR ("<td><form action='%s' method='get'>"), D_PAGE_PILOTWIRE);
  PilotWireWebSelectMode (true);
  WSContentSend_P (PSTR ("</form></td>"));

  // end
  WSContentSend_P (PSTR ("</tr></table>"));
}

// Pilot Wire web page
void PilotWireWebPage ()
{
  bool    updated = false;
  uint8_t target_mode;
  float   drift_temperature;
  float   actual_temperature;
  float   target_temperature;
  char    argument[PILOTWIRE_LABEL_BUFFER_SIZE];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // get pilot wire mode according to 'MODE' parameter
  if (WebServer->hasArg(D_CMND_PILOTWIRE_MODE))
  {
    WebGetArg (D_CMND_PILOTWIRE_MODE, argument, PILOTWIRE_LABEL_BUFFER_SIZE);
    PilotWireSetMode ((uint8_t)atoi (argument)); 
    updated = true;
  }

  // get pilot wire target temperature according to 'THERMOSTAT' parameter
  if (WebServer->hasArg(D_CMND_PILOTWIRE_TARGET))
  {
    WebGetArg (D_CMND_PILOTWIRE_TARGET, argument, PILOTWIRE_LABEL_BUFFER_SIZE);
    PilotWireSetThermostat (atof (argument));
    updated = true;
  }

  // get pilot wire temperature drift according to 'DRIFT' parameter
  if (WebServer->hasArg(D_CMND_PILOTWIRE_DRIFT))
  {
    WebGetArg (D_CMND_PILOTWIRE_DRIFT, argument, PILOTWIRE_LABEL_BUFFER_SIZE);
    PilotWireSetDrift (atof (argument));
    updated = true;
  }

  // if parameters updated, back to main page
  if (updated == true)
  {
    WebServer->sendHeader ("Location", "/", true);
    WebServer->send ( 302, "text/plain", "");
  }
  
  // get temperature and target mode
  actual_temperature = PilotWireGetTemperature ();
  target_mode = PilotWireGetMode ();

  // beginning of form
  WSContentStart_P (D_PILOTWIRE_CONFIGURE);
  WSContentSendStyle ();

  // form
  WSContentSend_P (PSTR ("<fieldset><legend><b>&nbsp;%s&nbsp;</b></legend><form method='get' action='%s'>"), D_PILOTWIRE_PARAMETERS, D_PAGE_PILOTWIRE);

  // selection : beginning
  WSContentSend_P (PSTR ("<p><b>%s</b><br/>"), D_PILOTWIRE_MODE);

  // select mode
  PilotWireWebSelectMode (false);

  // selection : end
  WSContentSend_P (PSTR ("</p>"));

  // if temperature sensor is present
  if (actual_temperature != 0) 
  {
    // read target and drift temperature
    target_temperature = PilotWireGetTargetTemperature ();
    drift_temperature  = PilotWireGetDrift ();

    // target temperature label and input
    WSContentSend_P (PSTR ("<p><b>%s</b><br/><input type='number' name='%s' min='5' max='30' step='0.5' value='%.2f'></p>"), D_PILOTWIRE_TARGET, D_CMND_PILOTWIRE_TARGET, target_temperature);

    // temperature correction label and input
    WSContentSend_P (PSTR ("<p><b>%s</b><br/><input type='number' name='%s' min='-5' max='5' step='0.1' value='%.2f'></p>"), D_PILOTWIRE_DRIFT, D_CMND_PILOTWIRE_DRIFT, drift_temperature);
  }

  // end of form
  WSContentSend_P(HTTP_FORM_END);
  WSContentSpaceButton(BUTTON_CONFIGURATION);
  WSContentStop();
}

// append pilot wire state to main page
bool PilotWireWebState ()
{
  float   corrected_temperature;
  float   target_temperature;
  uint8_t actual_mode;
  uint8_t actual_state;
  char*   actual_label;
  char    state_color[PILOTWIRE_COLOR_BUFFER_SIZE];

  // get mode
  actual_mode  = PilotWireGetMode ();
  actual_label = PilotWireGetStateLabel (actual_mode);
  
  // if pilot wire is in thermostat mode, display target temperature
  if (actual_mode == PILOTWIRE_THERMOSTAT)
  {
    // read temperature
    corrected_temperature = PilotWireGetTemperatureWithDrift ();
    target_temperature = PilotWireGetTargetTemperature ();

    // add it to JSON
    snprintf_P (mqtt_data, sizeof(mqtt_data), "%s<tr><th>%s</th><td>%.1f°C</td></tr>", mqtt_data, D_PILOTWIRE_CORRECTED, corrected_temperature);

    // add it to JSON
    snprintf_P (mqtt_data, sizeof(mqtt_data), "%s<tr><th>%s</th><td>%.1f°C</td></tr>", mqtt_data, D_PILOTWIRE_TARGET, target_temperature);
  }

  // if pilot wire mode is enabled
  if (actual_mode != PILOTWIRE_DISABLED)
  {
    // get state and label
    if (offload_start != 0) actual_state = PILOTWIRE_OFFLOAD;
    else actual_state = PilotWireGetRelayState ();
    actual_label = PilotWireGetStateLabel (actual_state);
    
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
 
    // display state
    snprintf_P (mqtt_data, sizeof(mqtt_data), "%s<tr><tr><td><b>%s</b></td><td style='font-weight:bold; color:%s;'>%s</td></tr>", mqtt_data, D_PILOTWIRE_STATE, state_color, actual_label);
  }
}

#endif  // USE_WEBSERVER

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xsns99 (byte callback_id)
{
  bool result = false;

  // main callback switch
  switch (callback_id)
  {
    case FUNC_COMMAND:
      result = PilotWireCommand ();
      break;
    case FUNC_EVERY_250_MSECOND:
      PilotWireEvery250MSecond ();
      break;
    case FUNC_JSON_APPEND:
      PilotWireShowJSON (true);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      WebServer->on ("/" D_PAGE_PILOTWIRE, PilotWireWebPage);
      break;
    case FUNC_WEB_APPEND:
      PilotWireWebState ();
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      PilotWireWebMainButton ();
      break;
    case FUNC_WEB_ADD_BUTTON:
      PilotWireWebButton ();
      break;
#endif  // USE_WEBSERVER

  }
  return result;
}

#endif // USE_PILOTWIRE
