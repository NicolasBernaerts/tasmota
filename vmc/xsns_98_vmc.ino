/*
  xsns_98_vmc.ino - Ventilation Motor Controled support 
  for Sonoff TH, Sonoff Basic or SonOff Dual
  
  Copyright (C) 2019 Nicolas Bernaerts
    15/03/2019 - v1.0 - Creation
    01/03/2020 - v2.0 - Functions rewrite for Tasmota 8.x compatibility
    07/03/2020 - v2.1 - Add daily humidity / temperature graph
    13/03/2020 - v2.2 - Add time on graph
    17/03/2020 - v2.3 - Handle Sonoff Dual and remote humidity sensor
    05/04/2020 - v2.4 - Add Timezone management
    15/05/2020 - v2.5 - Add /json page
    20/05/2020 - v2.6 - Add configuration for first NTP server

  Settings are stored using weighting scale parameters :
    - Settings.weight_reference   = VMC mode
    - Settings.weight_max         = Target humidity level (%)
    - Settings.weight_calibration = Humidity thresold (%) 
    
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

#ifdef USE_VMC

/*********************************************************************************************\
 * Fil Pilote
\*********************************************************************************************/

#define XSNS_98                  98

// web configuration page
#define D_PAGE_VMC_CONFIG       "vmc"
#define D_PAGE_VMC_GRAPH        "graph"
#define D_PAGE_VMC_JSON         "json"
#define D_CMND_VMC_MODE         "mode"
#define D_CMND_VMC_TARGET       "target"
#define D_CMND_VMC_THRESHOLD    "thres"

// JSON data
#define D_JSON_VMC              "VMC"
#define D_JSON_VMC_MODE         "Mode"
#define D_JSON_VMC_STATE        "State"
#define D_JSON_VMC_LABEL        "Label"
#define D_JSON_VMC_TARGET       "Target"
#define D_JSON_VMC_HUMIDITY     "Humidity"
#define D_JSON_VMC_TEMPERATURE  "Temperature"
#define D_JSON_VMC_THRESHOLD    "Threshold"
#define D_JSON_VMC_RELAY        "Relay"

#define D_VMC_MODE              "VMC Mode"
#define D_VMC_STATE             "VMC State"
#define D_VMC_GRAPH             "Daily Graph"
#define D_VMC_LOCAL             "Local"
#define D_VMC_REMOTE            "Remote"
#define D_VMC_HUMIDITY          "Humidity"
#define D_VMC_SENSOR            "Sensor"
#define D_VMC_DISABLED          "Disabled"
#define D_VMC_LOW               "Low speed"
#define D_VMC_HIGH              "High speed"
#define D_VMC_AUTO              "Automatic"
#define D_VMC_PARAMETERS        "VMC Parameters"
#define D_VMC_TARGET            "Target Humidity"
#define D_VMC_THRESHOLD         "Humidity Threshold"
#define D_VMC_CONFIGURE         "Configure VMC"
#define D_VMC_TIME              "Time"

// graph data
#define VMC_GRAPH_STEP          5           // collect graph data every 5 mn
#define VMC_GRAPH_SAMPLE        288         // 24 hours if data is collected every 5mn
#define VMC_GRAPH_WIDTH         800      
#define VMC_GRAPH_HEIGHT        400 
#define VMC_GRAPH_PERCENT_START 12      
#define VMC_GRAPH_PERCENT_STOP  88
#define VMC_GRAPH_TEMP_MIN      15      
#define VMC_GRAPH_TEMP_MAX      25  
#define VMC_GRAPH_HUMIDITY_MIN  0      
#define VMC_GRAPH_HUMIDITY_MAX  100  

// VMC data
#define VMC_TARGET_MAX          99
#define VMC_TARGET_DEFAULT      50
#define VMC_THRESHOLD_MAX       10
#define VMC_THRESHOLD_DEFAULT   2

// buffer
#define VMC_BUFFER_SIZE         128

// VMC modes
enum VmcModes { VMC_DISABLED, VMC_LOW, VMC_HIGH, VMC_AUTO };

// VMC commands
enum VmcCommands { CMND_VMC_MODE, CMND_VMC_TARGET, CMND_VMC_THRESHOLD };
enum VmcSources { VMC_SOURCE_NONE, VMC_SOURCE_LOCAL, VMC_SOURCE_REMOTE };
const char kVmcCommands[] PROGMEM = D_CMND_VMC_MODE "|" D_CMND_VMC_TARGET "|" D_CMND_VMC_THRESHOLD;

/*************************************************\
 *               Variables
\*************************************************/

// variables
int      vmc_graph_refresh;
uint32_t vmc_graph_index;
uint32_t vmc_graph_counter;
uint8_t  vmc_humidity_source;         // humidity source
float    vmc_current_temperature, vmc_graph_temperature;
uint8_t  vmc_current_humidity, vmc_graph_humidity;
uint8_t  vmc_current_target, vmc_graph_target;
uint8_t  vmc_graph_state;
float    arr_temperature[VMC_GRAPH_SAMPLE];
uint8_t  arr_humidity[VMC_GRAPH_SAMPLE];
uint8_t  arr_target[VMC_GRAPH_SAMPLE];
uint8_t  arr_state[VMC_GRAPH_SAMPLE];
String   str_vmc_json;

/**************************************************\
 *                  Accessors
\**************************************************/

// get VMC label according to state
const char* VmcGetStateLabel (uint8_t state)
{
  const char* label = NULL;
    
  // get label
  switch (state)
  {
   case VMC_DISABLED:          // Disabled
     label = D_VMC_DISABLED;
     break;
   case VMC_LOW:               // Forced Low speed
     label = D_VMC_LOW;
     break;
   case VMC_HIGH:              // Forced High speed
     label = D_VMC_HIGH;
     break;
   case VMC_AUTO:              // Automatic mode
     label = D_VMC_AUTO;
     break;
  }
  
  return label;
}

// get VMC state from relays state
uint8_t VmcGetRelayState ()
{
  uint8_t relay1 = 0;
  uint8_t relay2 = 1;
  uint8_t state;

  // read relay states
  relay1 = bitRead (power, 0);
  if (devices_present == 2) relay2 = bitRead (power, 1);

  // convert to pilotwire state
  if ((relay1 == 0 ) && (relay2 == 1 )) state = VMC_LOW;
  else if (relay1 == 1) state = VMC_HIGH;
  else state  = VMC_DISABLED;
  
  return state;
}

// set relays state
void VmcSetRelayState (uint8_t new_state)
{
  // set relays
  switch (new_state)
  {
    case VMC_DISABLED:  // VMC disabled
      ExecuteCommandPower (1, POWER_OFF, SRC_IGNORE);
      if (devices_present == 2) ExecuteCommandPower (2, POWER_OFF, SRC_IGNORE);
      break;
    case VMC_LOW:  // VMC low speed
      ExecuteCommandPower (1, POWER_OFF, SRC_IGNORE);
      if (devices_present == 2) ExecuteCommandPower (2, POWER_ON, SRC_IGNORE);
      break;
    case VMC_HIGH:  // VMC high speed
      if (devices_present == 2) ExecuteCommandPower (2, POWER_OFF, SRC_IGNORE);
      ExecuteCommandPower (1, POWER_ON, SRC_IGNORE);
      break;
  }
}

// get vmc actual mode
uint8_t VmcGetMode ()
{
  uint8_t actual_mode;

  // read actual VMC mode
  actual_mode = (uint8_t) Settings.weight_reference;

  // if outvalue, set to disabled
  if (actual_mode > VMC_AUTO) actual_mode = VMC_DISABLED;

  return actual_mode;
}

// set vmc mode
void VmcSetMode (uint8_t new_mode)
{
  // if outvalue, set to disabled
  if (new_mode > VMC_AUTO) new_mode = VMC_DISABLED;

  // if within range, set mode
  Settings.weight_reference = (unsigned long) new_mode;
}

// get current temperature
float VmcGetTemperature ()
{
  float temperature = NAN;

#ifdef USE_DHT
  // if dht sensor present, read it 
  if (Dht[0].t != 0) temperature = Dht[0].t;
#endif

  return temperature;
}

// get current humidity level
uint8_t VmcGetHumidity ()
{
  uint8_t result   = UINT8_MAX;
  float   humidity = NAN;

  // read humidity from local sensor
  vmc_humidity_source = VMC_SOURCE_LOCAL;

#ifdef USE_DHT
  // if dht sensor present, read it 
  if (Dht[0].h != 0) humidity = Dht[0].h;
#endif

  // if not available, read MQTT humidity
  if (isnan (humidity))
  {
    vmc_humidity_source = VMC_SOURCE_REMOTE;
    humidity = HumidityGetValue ();
  }

  // convert to integer
  if (isnan (humidity) == false) result = int (humidity);

  return result;
}

// set target humidity
void VmcSetTargetHumidity (uint8_t new_target)
{
  // if in range, save target humidity level
  if (new_target <= VMC_TARGET_MAX) Settings.weight_max = (uint16_t) new_target;
}

// get target humidity
uint8_t VmcGetTargetHumidity ()
{
  uint8_t target;

  // get target temperature
  target = (uint8_t) Settings.weight_max;
  if (target > VMC_TARGET_MAX) target = VMC_TARGET_DEFAULT;
  
  return target;
}

// set vmc humidity threshold
void VmcSetThreshold (uint8_t new_threshold)
{
  // if within range, save threshold
  if (new_threshold <= VMC_THRESHOLD_MAX) Settings.weight_calibration = (unsigned long) new_threshold;
}

// get vmc humidity threshold
uint8_t VmcGetThreshold ()
{
  uint8_t threshold;

  // get humidity threshold
  threshold = (uint8_t) Settings.weight_calibration;
   if (threshold > VMC_THRESHOLD_MAX) threshold = VMC_THRESHOLD_DEFAULT;

  return threshold;
}

// Show JSON status (for MQTT)
void VmcShowJSON (bool append)
{
  uint8_t humidity, value, mode;
  float   temperature;
  String  str_mqtt, str_text;

  // save MQTT data
  str_mqtt = mqtt_data;

  // get mode and humidity
  mode     = VmcGetMode ();
  str_text = VmcGetStateLabel (mode);

  // vmc mode  -->  "VMC":{"Relay":2,"Mode":4,"Label":"Automatic","Humidity":70.5,"Target":50,"Temperature":18.4}
  str_vmc_json  = "\"" + String (D_JSON_VMC) + "\":{";
  str_vmc_json += "\"" + String (D_JSON_VMC_RELAY) + "\":" + String (devices_present) + ",";
  str_vmc_json += "\"" + String (D_JSON_VMC_MODE) + "\":" + String (mode) + ",";
  str_vmc_json += "\"" + String (D_JSON_VMC_LABEL) + "\":\"" + str_text + "\",";

  // temperature
  temperature = VmcGetTemperature ();
  if (isnan(temperature) == false) str_text = String (temperature, 1);
  else str_text = "n/a";
  str_vmc_json += "\"" + String (D_JSON_VMC_TEMPERATURE) + "\":" + str_text + ",";

  // humidity level
  humidity = VmcGetHumidity ();
  if (humidity != UINT8_MAX) str_text = String (humidity);
  else str_text = "n/a";
  str_vmc_json += "\"" + String (D_JSON_VMC_HUMIDITY) + "\":" + str_text + ",";

  // target humidity
  value = VmcGetTargetHumidity ();
  str_vmc_json += "\"" + String (D_JSON_VMC_TARGET) + "\":" + String (value) + ",";

  // humidity thresold
  value = VmcGetThreshold ();
  str_vmc_json += "\"" + String (D_JSON_VMC_THRESHOLD) + "\":" + String (value) + "}";

  // if VMC mode is enabled
  if (mode != VMC_DISABLED)
  {
    // get relay state and label
    mode     = VmcGetRelayState ();
    str_text = VmcGetStateLabel (mode);

    // relay state  -->  ,"State":{"Mode":1,"Label":"On"}
    str_vmc_json += ",";
    str_vmc_json += "\"" + String (D_JSON_VMC_STATE) + "\":{";
    str_vmc_json += "\"" + String (D_JSON_VMC_MODE) + "\":" + String (mode) + ",";
    str_vmc_json += "\"" + String (D_JSON_VMC_LABEL) + "\":\"" + str_text + "\"}";
  }

  // add remote humidity to JSON
  snprintf_P(mqtt_data, sizeof(mqtt_data), str_vmc_json.c_str());
  HumidityShowJSON (true);
  str_vmc_json = mqtt_data;

  // generate MQTT message and publish if needed
  if (append == false) 
  {
    str_mqtt = "{" + str_vmc_json + "}";
    snprintf_P(mqtt_data, sizeof(mqtt_data), str_mqtt.c_str());
    MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));
  }
  else
  {
    str_mqtt += "," + str_vmc_json;
    snprintf_P(mqtt_data, sizeof(mqtt_data), str_mqtt.c_str());
  }
}

// Handle VMC MQTT commands
bool VmcCommand ()
{
  bool command_serviced = true;
  int  command_code;
  char command [CMDSZ];

  // check MQTT command
  command_code = GetCommandCode (command, sizeof(command), XdrvMailbox.topic, kVmcCommands);

  // handle command
  switch (command_code)
  {
    case CMND_VMC_MODE:        // set mode
      VmcSetMode (XdrvMailbox.payload);
      break;
    case CMND_VMC_TARGET:     // set target humidity 
      VmcSetTargetHumidity ((uint8_t) XdrvMailbox.payload);
      break;
    case CMND_VMC_THRESHOLD:  // set humidity threshold 
      VmcSetThreshold ((uint8_t) XdrvMailbox.payload);
      break;
    default:
      command_serviced = false;
      break;
  }

  // if command processed, publish JSON
  if (command_serviced == true) VmcShowJSON (false);

  return command_serviced;
}

// update graph history data
void VmcUpdateHistory ()
{
  // set indexed graph values with current values
  arr_temperature[vmc_graph_index] = vmc_graph_temperature;
  arr_humidity[vmc_graph_index] = vmc_graph_humidity;
  arr_target[vmc_graph_index] = vmc_graph_target;
  arr_state[vmc_graph_index] = vmc_graph_state;

  // init current values
  vmc_graph_temperature = NAN;
  vmc_graph_humidity    = UINT8_MAX;
  vmc_graph_target      = UINT8_MAX;
  vmc_graph_state       = VMC_DISABLED;

  // increase temperature data index and reset if max reached
  vmc_graph_index ++;
  vmc_graph_index = vmc_graph_index % VMC_GRAPH_SAMPLE;
}

void VmcEverySecond ()
{
  bool    need_update = false;
  uint8_t humidity, threshold, mode, target, actual_state, target_state;
  float   temperature;

  // update current temperature
  temperature = VmcGetTemperature ( );
  if (isnan(temperature) == false)
  {
    // save current value and ask for JSON update if any change
    if (vmc_current_temperature != temperature) need_update = true;
    vmc_current_temperature = temperature;

    // update graph value
    if (isnan(vmc_graph_temperature) == false) vmc_graph_temperature = min (vmc_graph_temperature, temperature);
    else vmc_graph_temperature = temperature;
  }

  // update current humidity
  humidity = VmcGetHumidity ( );
  if (humidity != UINT8_MAX)
  {
    // save current value and ask for JSON update if any change
    if (vmc_current_humidity != humidity) need_update = true;
    vmc_current_humidity = humidity;

    // update graph value
    if (vmc_graph_humidity != UINT8_MAX) vmc_graph_humidity = max (vmc_graph_humidity, humidity);
    else vmc_graph_humidity = humidity;
  }

  // update target humidity
  target = VmcGetTargetHumidity ();
  if (target != UINT8_MAX)
  {
    // save current value and ask for JSON update if any change
    if (vmc_current_target != target) need_update = true;
    vmc_current_target = target;

    // update graph value
    if (vmc_graph_target != UINT8_MAX) vmc_graph_target = min (vmc_graph_target, target);
    else vmc_graph_target = target;
  } 

  // update relay state
  actual_state = VmcGetRelayState ();
  if (vmc_graph_state != VMC_HIGH) vmc_graph_state = actual_state;

  // get VMC mode
  mode = VmcGetMode ();

  // if only one relay and mode is disabled, it is considered as low
  if ((devices_present == 1) && (mode == VMC_DISABLED)) mode = VMC_LOW;

  // if VMC mode is automatic
  if (mode == VMC_AUTO)
  {
    // get current and target humidity
    threshold = VmcGetThreshold ();

    // if humidity is low enough, target VMC state is low speed
    if (humidity < (target - threshold)) target_state = VMC_LOW;
      
    // else, if humidity is too high, target VMC state is high speed
    else if (humidity > (target + threshold)) target_state = VMC_HIGH;

    // else, keep current state
    else target_state = actual_state;
  }

  // else, set target mode
  else target_state = mode;

  // if VMC state is different than target state, set relay
  if (actual_state != target_state)
  {
    VmcSetRelayState (target_state);
    need_update = true;
  }
  
  // if JSON update needed, publish
  if (need_update == true) VmcShowJSON (false);

  // increment delay counter and if delay reached, update history data
  if (vmc_graph_counter == 0) VmcUpdateHistory ();
  vmc_graph_counter ++;
  vmc_graph_counter = vmc_graph_counter % vmc_graph_refresh;
}

void VmcInit ()
{
  int    index;

  // init default values
  vmc_humidity_source     = VMC_SOURCE_NONE;
  vmc_current_temperature = NAN;
  vmc_current_humidity    = UINT8_MAX;
  vmc_current_target      = UINT8_MAX;
  vmc_graph_temperature = NAN;
  vmc_graph_humidity    = UINT8_MAX;
  vmc_graph_target      = UINT8_MAX;
  vmc_graph_state       = VMC_LOW;
  vmc_graph_index       = 0;
  vmc_graph_counter     = 0;
  vmc_graph_refresh     = 60 * VMC_GRAPH_STEP;

  // initialise graph data
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    arr_temperature[index] = NAN;
    arr_humidity[index] = UINT8_MAX;
    arr_target[index] = UINT8_MAX;
    arr_state[index] = VMC_LOW;
  }
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// VMC mode select combo
void VmcWebSelectMode (bool autosubmit)
{
  uint8_t mode, humidity;
  char    argument[VMC_BUFFER_SIZE];

  // get mode and humidity
  mode     = VmcGetMode ();
  humidity = VmcGetHumidity ();

  // selection : beginning
  WSContentSend_P (PSTR ("<select name='%s'"), D_CMND_VMC_MODE);
  if (autosubmit == true) WSContentSend_P (PSTR (" onchange='this.form.submit()'"));
  WSContentSend_P (PSTR (">"));
  
  // selection : disabled
  if (mode == VMC_DISABLED) strcpy (argument, "selected"); 
  else strcpy (argument, "");
  WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>"), VMC_DISABLED, argument, D_VMC_DISABLED);

  // selection : low speed
  if (mode == VMC_LOW) strcpy (argument, "selected");
  else strcpy (argument, "");
  WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>"), VMC_LOW, argument, D_VMC_LOW);

  // selection : high speed
  if (mode == VMC_HIGH) strcpy (argument, "selected");
  else strcpy (argument, "");
  WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>"), VMC_HIGH, argument, D_VMC_HIGH);

  // selection : automatic
  if (humidity != UINT8_MAX) 
  {
    if (mode == VMC_AUTO) strcpy (argument, "selected");
    else strcpy (argument, "");
    WSContentSend_P (PSTR("<option value='%d' %s>%s</option>"), VMC_AUTO, argument, D_VMC_AUTO);
  }

  // selection : end
  WSContentSend_P (PSTR ("</select>"));
}

// append VMC control button to main page
void VmcWebMainButton ()
{
    // VMC control page button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_PAGE_VMC_GRAPH, D_VMC_GRAPH);
}

// append VMC configuration button to configuration page
void VmcWebConfigButton ()
{
  // VMC configuration button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>"), D_PAGE_VMC_CONFIG, D_VMC_CONFIGURE);
}

// append VMC state to main page
bool VmcWebSensor ()
{
  uint8_t  mode, state, humidity, target;
  String   str_title, str_text, str_value, str_source, str_color;

  // display mode
  mode     = VmcGetMode ();
  str_text = VmcGetStateLabel (mode);
  WSContentSend_PD (PSTR("{s}%s{m}%s{e}"), D_VMC_MODE, str_text.c_str());

  // get state and label
  state    = VmcGetRelayState ();
  str_text = VmcGetStateLabel (state);
    
  // set color according to state
  switch (state)
  {
    case VMC_HIGH:
      str_color = "red";
      break;
    case VMC_LOW:
      str_color = "green";
      break;
    default:
      str_color = "white";
  }
 
  // display state
  WSContentSend_PD (PSTR("{s}%s{m}<span style='font-weight:bold; color:%s;'>%s</span>{e}"), D_VMC_STATE, str_color.c_str(), str_text.c_str());

  // if automatic mode, display humidity and target humidity
  if (mode == VMC_AUTO)
  {
    // read current and target humidity
    humidity = VmcGetHumidity ();
    target   = VmcGetTargetHumidity ();

    // handle sensor source
    switch (vmc_humidity_source)
    {
      case VMC_SOURCE_NONE:  // no humidity source available 
        str_value  = "--";
        break;
      case VMC_SOURCE_LOCAL:  // local humidity source used 
        str_source = D_VMC_LOCAL;
        str_value  = String (humidity);
        break;
      case VMC_SOURCE_REMOTE:  // remote humidity source used 
        str_source = D_VMC_REMOTE;
        str_value  = String (humidity);
        break;
    }

    // set title and text
    str_title = D_VMC_HUMIDITY;
    if (str_source.length() > 0) str_title += " (" + str_source + ")";
    str_text  = "<b>" + str_value + "</b> / " + String (target) + "%";

    // display
    WSContentSend_PD (PSTR("{s}%s{m}%s{e}"), str_title.c_str(), str_text.c_str());
  }
}

// VMC web page
void VmcWebPageConfig ()
{
  uint8_t value;
  char    argument[VMC_BUFFER_SIZE];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // page comes from save button on configuration page
  if (WebServer->hasArg("save"))
  {
    // get VMC mode according to MODE parameter
    WebGetArg (D_CMND_VMC_MODE, argument, VMC_BUFFER_SIZE);
    if (strlen(argument) > 0) VmcSetMode ((uint8_t) atoi (argument)); 

    // get VMC target humidity according to TARGET parameter
    WebGetArg (D_CMND_VMC_TARGET, argument, VMC_BUFFER_SIZE);
    if (strlen(argument) > 0) VmcSetTargetHumidity ((uint8_t) atoi (argument));

    // get VMC humidity threshold according to THRESHOLD parameter
    WebGetArg (D_CMND_VMC_THRESHOLD, argument, VMC_BUFFER_SIZE);
    if (strlen(argument) > 0) VmcSetThreshold ((uint8_t) atoi (argument));
  }

  // beginning of form
  WSContentStart_P (D_VMC_CONFIGURE);
  WSContentSendStyle ();
  WSContentSend_P (PSTR("<form method='get' action='%s'>\n"), D_PAGE_VMC_CONFIG);

  WSContentSend_P (PSTR ("<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>\n"), D_VMC_PARAMETERS);

  // select mode
  WSContentSend_P (PSTR ("<p>%s<br/>"), D_VMC_MODE);
  VmcWebSelectMode (false);
  WSContentSend_P (PSTR ("</p>\n"));

  // target humidity label and input
  value = VmcGetTargetHumidity ();
  WSContentSend_P (PSTR ("<p>%s<br/><input type='number' name='%s' min='0' max='%d' step='1' value='%d'></p>\n"), D_VMC_TARGET, D_CMND_VMC_TARGET, VMC_TARGET_MAX, value);

  // humidity threshold label and input
  value = VmcGetThreshold ();
  WSContentSend_P (PSTR ("<p>%s<br/><input type='number' name='%s' min='0' max='%d' step='1' value='%d'></p>\n"), D_VMC_THRESHOLD, D_CMND_VMC_THRESHOLD, VMC_THRESHOLD_MAX, value);

  WSContentSend_P (PSTR("</fieldset></p>\n"));

  // save button
  WSContentSend_P (PSTR ("<p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), D_SAVE);
  WSContentSend_P (PSTR ("</form>"));

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

// Humidity and temperature graph
void VmcWebDisplayGraph ()
{
  TIME_T   current_dst;
  uint32_t current_time;
  int      index, array_idx;
  int      graph_x, graph_y, graph_x1, graph_x2, graph_left, graph_right, graph_width, graph_pos, graph_hour;
  uint8_t  humidity, target, state_curr, state_prev;
  float    temperature, temp_min, temp_max, temp_scope;
  char     str_hour[4];
  String   str_value;

// A RECODER

  // loop to adjust min and max temperature
  temp_min   = VMC_GRAPH_TEMP_MIN;
  temp_max   = VMC_GRAPH_TEMP_MAX;
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    // if needed, adjust minimum and maximum temperature
    temperature = arr_temperature[index];
    if (isnan (temperature) == false)
    {
      if (temperature < temp_min) temp_min = floor (temperature);
      if (temperature > temp_max) temp_max = ceil (temperature);
    }
  }
  temp_scope = temp_max - temp_min;

  // boundaries of SVG graph
  graph_left  = VMC_GRAPH_PERCENT_START * VMC_GRAPH_WIDTH / 100;
  graph_right = VMC_GRAPH_PERCENT_STOP * VMC_GRAPH_WIDTH / 100;
  graph_width = graph_right - graph_left;

  // start of SVG graph
  WSContentSend_P (PSTR ("<svg viewBox='0 0 %d %d'>\n"), VMC_GRAPH_WIDTH, VMC_GRAPH_HEIGHT);

  // graph curve zone
  WSContentSend_P (PSTR ("<rect x='%d%%' y='0%%' width='%d%%' height='100%%' rx='10' />\n"), VMC_GRAPH_PERCENT_START, VMC_GRAPH_PERCENT_STOP - VMC_GRAPH_PERCENT_START);

  // graph separation lines
  WSContentSend_P (PSTR ("<line class='dash' x1='%d' y1='25%%' x2='%d' y2='25%%' />\n"), graph_left, graph_right);
  WSContentSend_P (PSTR ("<line class='dash' x1='%d' y1='50%%' x2='%d' y2='50%%' />\n"), graph_left, graph_right);
  WSContentSend_P (PSTR ("<line class='dash' x1='%d' y1='75%%' x2='%d' y2='75%%' />\n"), graph_left, graph_right);

  // temperature units
  str_value = String (temp_max, 1);
  WSContentSend_P (PSTR ("<text class='temperature' x='%d%%' y='%d%%'>%s°C</text>\n"), 1, 5, str_value.c_str ());
  str_value = String (temp_min + temp_scope * 0.75, 1);
  WSContentSend_P (PSTR ("<text class='temperature' x='%d%%' y='%d%%'>%s°C</text>\n"), 1, 27, str_value.c_str ());
  str_value = String (temp_min + temp_scope * 0.50, 1);
  WSContentSend_P (PSTR ("<text class='temperature' x='%d%%' y='%d%%'>%s°C</text>\n"), 1, 52, str_value.c_str ());
  str_value = String (temp_min + temp_scope * 0.25, 1);
  WSContentSend_P (PSTR ("<text class='temperature' x='%d%%' y='%d%%'>%s°C</text>\n"), 1, 77, str_value.c_str ());
  str_value = String (temp_min, 1);
  WSContentSend_P (PSTR ("<text class='temperature' x='%d%%' y='%d%%'>%s°C</text>\n"), 1, 99, str_value.c_str ());

  // humidity units
  WSContentSend_P (PSTR ("<text class='humidity' x='%d%%' y='%d%%'>%d %%</text>\n"), VMC_GRAPH_PERCENT_STOP + 2, 5, 100);
  WSContentSend_P (PSTR ("<text class='humidity' x='%d%%' y='%d%%'>%d %%</text>\n"), VMC_GRAPH_PERCENT_STOP + 2, 27, 75);
  WSContentSend_P (PSTR ("<text class='humidity' x='%d%%' y='%d%%'>%d %%</text>\n"), VMC_GRAPH_PERCENT_STOP + 2, 52, 50);
  WSContentSend_P (PSTR ("<text class='humidity' x='%d%%' y='%d%%'>%d %%</text>\n"), VMC_GRAPH_PERCENT_STOP + 2, 77, 25);
  WSContentSend_P (PSTR ("<text class='humidity' x='%d%%' y='%d%%'>%d %%</text>\n"), VMC_GRAPH_PERCENT_STOP + 2, 99, 0);

  // ---------------
  //   Relay state
  // ---------------

  // loop for the relay state as background red color
  state_prev = VMC_LOW;
  graph_x1 = 0;
  graph_x2 = 0;
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    // get target temperature value and set to minimum if not defined
    array_idx  = (index + vmc_graph_index) % VMC_GRAPH_SAMPLE;
    state_curr = arr_state[array_idx];

    // last graph point, force draw
    if ((index == VMC_GRAPH_SAMPLE - 1) && (state_prev == VMC_HIGH)) state_curr = VMC_LOW;

    // if relay just switched on, record start point x1
    if ((state_prev != VMC_HIGH) && (state_curr == VMC_HIGH)) graph_x1  = graph_left + (graph_width * index / VMC_GRAPH_SAMPLE);
    
    // esle, if relay just switched off, record end point x2
    else if ((state_prev == VMC_HIGH) && (state_curr != VMC_HIGH)) graph_x2  = graph_left + (graph_width * index / VMC_GRAPH_SAMPLE);

    // if both point recorded,
    if ((graph_x1 != 0) && (graph_x2 != 0))
    {
      // display graph
      WSContentSend_P (PSTR ("<rect class='relay' x='%d' y='95%%' width='%d' height='5%%' />\n"), graph_x1, graph_x2 - graph_x1);

      // reset records
      graph_x1 = 0;
      graph_x2 = 0;
    }

    // update previous state
    state_prev = state_curr;
  }

  // --------------------
  //   Target Humidity
  // --------------------

  // loop for the target humidity graph
  WSContentSend_P (PSTR ("<polyline class='target' points='"));
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    // get target temperature value and set to minimum if not defined
    array_idx = (index + vmc_graph_index) % VMC_GRAPH_SAMPLE;
    target    = arr_target[array_idx];

    // if target value is defined,
    if (target != UINT8_MAX)
    {
      // calculate current position
      graph_x = graph_left + (graph_width * index / VMC_GRAPH_SAMPLE);
      graph_y = VMC_GRAPH_HEIGHT - (target * VMC_GRAPH_HEIGHT / 100);

      // add the point to the line
      WSContentSend_P (PSTR("%d,%d "), graph_x, graph_y);
    }
  }
  WSContentSend_P (PSTR("'/>\n"));

  // ----------------
  //   Temperature
  // ----------------

  // loop for the temperature graph
  WSContentSend_P (PSTR ("<polyline class='temperature' points='"));
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    // if temperature value is defined
    array_idx   = (index + vmc_graph_index) % VMC_GRAPH_SAMPLE;
    temperature = arr_temperature[array_idx];
    if (isnan (temperature) == false)
    {
      // calculate current position
      graph_x = graph_left + (graph_width * index / VMC_GRAPH_SAMPLE);
      graph_y = VMC_GRAPH_HEIGHT - int (((temperature - temp_min) / temp_scope) * VMC_GRAPH_HEIGHT);

      // add the point to the line
      WSContentSend_P (PSTR("%d,%d "), graph_x, graph_y);
    }
  }
  WSContentSend_P (PSTR("'/>\n"));

  // ---------------
  //    Humidity
  // ---------------

  // loop for the humidity graph
  WSContentSend_P (PSTR ("<polyline class='humidity' points='"));
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    // if temperature value is defined
    array_idx = (index + vmc_graph_index) % VMC_GRAPH_SAMPLE;
    humidity  = arr_humidity[array_idx];
    if (humidity != UINT8_MAX)
    {
      // adjust current temperature to acceptable range
      humidity = min (max ((int) humidity, 0), 100);

      // calculate current position
      graph_x = graph_left + (graph_width * index / VMC_GRAPH_SAMPLE);
      graph_y = VMC_GRAPH_HEIGHT - (humidity * VMC_GRAPH_HEIGHT / 100);

      // add the point to the line
      WSContentSend_P (PSTR("%d,%d "), graph_x, graph_y);
    }
  }
  WSContentSend_P (PSTR("'/>\n"));

  // ------------
  //     Time
  // ------------

  // get current time
  current_time = LocalTime();
  BreakTime (current_time, current_dst);

  // calculate width of remaining (minutes) till next hour
  current_dst.hour = (current_dst.hour + 1) % 24;
  graph_hour = ((60 - current_dst.minute) * graph_width / 1440) - 15; 

  // if shift is too small, shift to next hour
  if (graph_hour < 0)
  {
    current_dst.hour = (current_dst.hour + 1) % 24;
    graph_hour += graph_width / 24; 
  }

  // dislay first time mark
  graph_x = graph_left + graph_hour;
  sprintf(str_hour, "%02d", current_dst.hour);
  WSContentSend_P (PSTR ("<text class='time' x='%d' y='%d%%'>%sh</text>\n"), graph_x, 52, str_hour);

  // dislay next 5 time marks (every 4 hours)
  for (index = 0; index < 5; index++)
  {
    current_dst.hour = (current_dst.hour + 4) % 24;
    graph_x += graph_width / 6;
    sprintf(str_hour, "%02d", current_dst.hour);
    WSContentSend_P (PSTR ("<text class='time' x='%d' y='%d%%'>%sh</text>\n"), graph_x, 52, str_hour);
  }

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
}

// VMC public web page
void VmcWebPageGraph ()
{
  float value;

  // beginning of form without authentification with 60 seconds auto refresh
  WSContentStart_P (D_VMC_GRAPH, false);
  WSContentSend_P (PSTR ("</script>\n"));
  WSContentSend_P (PSTR ("<meta http-equiv='refresh' content='%d;URL=/%s' />\n"), 60, D_PAGE_VMC_GRAPH);
  
  // page style
  WSContentSend_P (PSTR ("<style>\n"));

  WSContentSend_P (PSTR ("body {color:white;background-color:#303030;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {width:100%%;margin:auto;padding:3px 0px;text-align:center;vertical-align:middle;}\n"));

  WSContentSend_P (PSTR (".title {font-size:5vh;}\n"));
  WSContentSend_P (PSTR (".temperature {font-size:24px;}\n"));
  WSContentSend_P (PSTR (".humidity {font-size:24px;}\n"));
  WSContentSend_P (PSTR (".time {font-size:20px;}\n"));
  WSContentSend_P (PSTR (".bold {font-weight:bold;}\n"));

  WSContentSend_P (PSTR (".graph {max-width:%dpx;}\n"), VMC_GRAPH_WIDTH);
  WSContentSend_P (PSTR (".yellow {color:#FFFF33;}\n"));
  WSContentSend_P (PSTR (".orange {color:#FF8C00;}\n"));

  WSContentSend_P (PSTR ("rect.graph {stroke:grey;stroke-dasharray:1;fill:#101010;}\n"));
  WSContentSend_P (PSTR ("rect.relay {fill:red;fill-opacity:50%%;}\n"));
  WSContentSend_P (PSTR ("polyline.temperature {fill:none;stroke:yellow;stroke-width:2;}\n"));
  WSContentSend_P (PSTR ("polyline.humidity {fill:none;stroke:orange;stroke-width:4;}\n"));
  WSContentSend_P (PSTR ("polyline.target {fill:none;stroke:orange;stroke-width:2;stroke-dasharray:4;}\n"));
  WSContentSend_P (PSTR ("line.dash {stroke:grey;stroke-width:1;stroke-dasharray:8;}\n"));
  WSContentSend_P (PSTR ("text.temperature {stroke:yellow;fill:yellow;}\n"));
  WSContentSend_P (PSTR ("text.humidity {stroke:orange;fill:orange;}\n"));
  WSContentSend_P (PSTR ("text.time {stroke:white;fill:white;}\n"));

  WSContentSend_P (PSTR ("</style>\n</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body>\n"));

  // room name
  WSContentSend_P (PSTR ("<div class='title bold'>%s</div>\n"), SettingsText(SET_FRIENDLYNAME1));

  // temperature
  value = VmcGetTemperature ();
  if (isnan (value) == false) WSContentSend_P (PSTR ("<div class='title bold yellow'>%s °C</div>\n"), String (VmcGetTemperature (), 1).c_str());

  // humidity
  WSContentSend_P (PSTR ("<div class='title bold orange'>%d %%</div>\n"), VmcGetHumidity ());

  // display temperature graph
  WSContentSend_P (PSTR ("<div class='graph'>\n"));
  VmcWebDisplayGraph ();
  WSContentSend_P (PSTR ("</div>\n"));

  // end of page
  WSContentStop ();
}

// JSON public page
void VmcPageJson ()
{
  WSContentBegin(200, CT_HTML);
  WSContentSend_P (PSTR ("{%s}\n"), str_vmc_json.c_str ());
  WSContentEnd();
}

#endif  // USE_WEBSERVER

/*******************************************************\
 *                      Interface
\*******************************************************/

bool Xsns98 (byte callback_id)
{
  bool result = false;

  // main callback switch
  switch (callback_id)
  {
    case FUNC_INIT:
      VmcInit ();
      break;
    case FUNC_COMMAND:
      result = VmcCommand ();
      break;
    case FUNC_EVERY_SECOND:
      VmcEverySecond ();
      break;
    case FUNC_JSON_APPEND:
      VmcShowJSON (true);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      WebServer->on ("/" D_PAGE_VMC_CONFIG,  VmcWebPageConfig);
      WebServer->on ("/" D_PAGE_VMC_GRAPH, VmcWebPageGraph);
      WebServer->on ("/" D_PAGE_VMC_JSON, VmcPageJson);
      break;
    case FUNC_WEB_SENSOR:
      VmcWebSensor ();
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      VmcWebMainButton ();
      break;
    case FUNC_WEB_ADD_BUTTON:
      VmcWebConfigButton ();
      break;
#endif  // USE_WEBSERVER

  }
  return result;
}

#endif // USE_VMC
