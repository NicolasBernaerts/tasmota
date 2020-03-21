/*
  xsns_98_vmc.ino - Ventilation Motor Controled support 
  for Sonoff TH, Sonoff Basic or SonOff Dual
  
  Copyright (C) 2019 Nicolas Bernaerts
    15/03/2019 - v1.0 - Creation
    01/03/2020 - v2.0 - Functions rewrite for Tasmota 8.x compatibility
    07/03/2020 - v2.1 - Add daily humidity / temperature graph
    13/03/2020 - v2.2 - Add time on graph
    17/03/2020 - v2.3 - Handle Sonoff Dual and remote humidity sensor

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
#define D_PAGE_VMC_CONTROL      "control"
#define D_PAGE_VMC_GRAPH        "graph"
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
float    vmc_current_temperature;
float    vmc_current_humidity;
uint8_t  vmc_current_target;
uint8_t  vmc_current_state;
float    arr_temperature[VMC_GRAPH_SAMPLE];
float    arr_humidity[VMC_GRAPH_SAMPLE];
uint8_t  arr_target[VMC_GRAPH_SAMPLE];
uint8_t  arr_state[VMC_GRAPH_SAMPLE];

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
float VmcGetHumidity ()
{
  float humidity = NAN;

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

  return humidity;
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

  // check if within range
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
  
  // check if within range
   if (threshold > VMC_THRESHOLD_MAX) threshold = VMC_THRESHOLD_DEFAULT;

  return threshold;
}

// Show JSON status (for MQTT)
void VmcShowJSON (bool append)
{
  uint8_t state, mode;
  float   value;
  String  str_json, str_label;

  // get mode and humidity
  mode      = VmcGetMode ();
  str_label = VmcGetStateLabel (mode);

  // start message  -->  {  or message,
  if (append == false) str_json = "{";
  else str_json = String (mqtt_data) + ",";

  // vmc mode  -->  "VMC":{"Relay":2,"Mode":4,"Label":"Automatic","Humidity":70.5,"Target":50,"Temperature":18.4}
  str_json += "\"" + String (D_JSON_VMC) + "\":{";
  str_json += ",\"" + String (D_JSON_VMC_RELAY) + "\":" + String (devices_present);
  str_json += ",\"" + String (D_JSON_VMC_MODE) + "\":" + String (mode);
  str_json += ",\"" + String (D_JSON_VMC_LABEL) + "\":" + str_label + "\"";

  // if temperature is available, add it to JSON
  value = VmcGetTemperature ();
  if (isnan(value) == false) str_json += ",\"" + String (D_JSON_VMC_TEMPERATURE) + "\":" + String (value, 1);

  // if humidity level is available, add it to JSON
  value = VmcGetHumidity ();
  if (isnan(value) == false) str_json += ",\"" + String (D_JSON_VMC_HUMIDITY) + "\":" + String (value, 1);

  // add target and thresold to JSON
  str_json += ",\"" + String (D_JSON_VMC_TARGET) + "\":" + String (VmcGetTargetHumidity ());
  str_json += ",\"" + String (D_JSON_VMC_THRESHOLD) + "\":" + String (VmcGetThreshold ());
  
  // end of section
  str_json += "}";

  // if VMC mode is enabled
  if (mode != VMC_DISABLED)
  {
    // get relay state and label
    state     = VmcGetRelayState ();
    str_label = VmcGetStateLabel (state);

    // relay state  -->  ,"State":{"Mode":1,"Label":"On"}
    str_json += ",\"" + String (D_JSON_VMC_STATE) + "\":{";
    str_json += ",\"" + String (D_JSON_VMC_MODE) + "\":" + String (state);
    str_json += ",\"" + String (D_JSON_VMC_LABEL) + "\":" + str_label + "\"}";
  }

  // if not in append mode, add last bracket 
  if (append == false) str_json += "}";

  // add json string to MQTT message
  snprintf_P (mqtt_data, sizeof(mqtt_data), str_json.c_str ());

  // if not in append mode, publish message 
  if (append == false) MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));
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
      VmcSetTargetHumidity (atof (XdrvMailbox.data));
      break;
    case CMND_VMC_THRESHOLD:  // set humidity threshold 
      VmcSetThreshold (atof (XdrvMailbox.data));
      break;
    default:
      command_serviced = false;
      break;
  }

  // if command processed, update JSON
  if (command_serviced == true) VmcShowJSON (false);

  return command_serviced;
}

// update graph history data
void VmcUpdateHistory ()
{
  // set indexed graph values with current values
  arr_temperature[vmc_graph_index] = vmc_current_temperature;
  arr_humidity[vmc_graph_index] = vmc_current_humidity;
  arr_target[vmc_graph_index] = vmc_current_target;
  arr_state[vmc_graph_index] = vmc_current_state;

  // init current values
  vmc_current_temperature = NAN;
  vmc_current_humidity    = NAN;
  vmc_current_target      = UINT8_MAX;
  vmc_current_state       = VMC_DISABLED;

  // increase temperature data index and reset if max reached
  vmc_graph_index ++;
  vmc_graph_index = vmc_graph_index % VMC_GRAPH_SAMPLE;
}

void VmcEverySecond ()
{
  uint8_t mode, target_value, actual_state, target_state;
  float   temperature, humidity, target, threshold;

  // update current temperature
  temperature = VmcGetTemperature ( );
  if (isnan(temperature) == false)
  {
    if (isnan(vmc_current_temperature) == false) vmc_current_temperature = min (vmc_current_temperature, temperature);
    else vmc_current_temperature = temperature;
  }

  // update current humidity
  humidity = VmcGetHumidity ( );
  if (isnan(humidity) == false)
  {
    if (isnan(vmc_current_humidity) == false) vmc_current_humidity = max (vmc_current_humidity, humidity);
    else vmc_current_humidity = humidity;
  }

  // update target humidity
  target_value = VmcGetTargetHumidity ();
  if (target_value != UINT8_MAX)
  {
    if (vmc_current_target != UINT8_MAX) vmc_current_target = min (vmc_current_target, target_value);
    else vmc_current_target = target_value;
  } 

  // update relay state
  actual_state = VmcGetRelayState ();
  if (vmc_current_state != VMC_HIGH) vmc_current_state = actual_state;

  // increment delay counter and if delay reached, update history data
  if (vmc_graph_counter == 0) VmcUpdateHistory ();
  vmc_graph_counter ++;
  vmc_graph_counter = vmc_graph_counter % vmc_graph_refresh;

  // get VMC mode
  mode = VmcGetMode ();

  // if only one relay and mode is disabled, it is considered as low
  if ((devices_present == 1) && (mode == VMC_DISABLED)) mode = VMC_LOW;

  // if VMC mode is automatic
  if (mode == VMC_AUTO)
  {
    // get current and target humidity
    target    = float (target_value);
    threshold = float (VmcGetThreshold ());

    // if humidity is low enough, target VMC state is low speed
    if (humidity < (target - threshold)) target_state = VMC_LOW;
      
    // else, if humidity is too high, target VMC state is high speed
    else if (humidity > (target + threshold)) target_state = VMC_HIGH;

    // else, keep current state
    else target_state = actual_state;
  }

  // else, set target mode
  else target_state = mode;

  // if VMC state is different than target state, change state
  if (actual_state != target_state)
  {
    // set relays
    VmcSetRelayState (target_state);

    // publish new state
    VmcShowJSON (false);
  }
}

void VmcInit ()
{
  int    index;

  // init default values
  vmc_humidity_source     = VMC_SOURCE_NONE;
  vmc_current_temperature = NAN;
  vmc_current_humidity    = NAN;
  vmc_current_target      = UINT8_MAX;
  vmc_current_state       = VMC_LOW;
  vmc_graph_index         = 0;
  vmc_graph_counter       = 0;
  vmc_graph_refresh       = 60 * VMC_GRAPH_STEP;

  // initialise graph data
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    arr_temperature[index] = NAN;
    arr_humidity[index] = NAN;
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
  uint8_t actual_mode;
  float   actual_humidity;
  char    argument[VMC_BUFFER_SIZE];

  // get mode and humidity
  actual_mode     = VmcGetMode ();
  actual_humidity = VmcGetHumidity ();

  // selection : beginning
  WSContentSend_P (PSTR ("<select name='%s'"), D_CMND_VMC_MODE);
  if (autosubmit == true) WSContentSend_P (PSTR (" onchange='this.form.submit()'"));
  WSContentSend_P (PSTR (">"));
  
  // selection : disabled
  if (actual_mode == VMC_DISABLED) strcpy (argument, "selected"); 
  else strcpy (argument, "");
  WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>"), VMC_DISABLED, argument, D_VMC_DISABLED);

  // selection : low speed
  if (actual_mode == VMC_LOW) strcpy (argument, "selected");
  else strcpy (argument, "");
  WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>"), VMC_LOW, argument, D_VMC_LOW);

  // selection : high speed
  if (actual_mode == VMC_HIGH) strcpy (argument, "selected");
  else strcpy (argument, "");
  WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>"), VMC_HIGH, argument, D_VMC_HIGH);

  // selection : automatic
  if (actual_humidity != 0) 
  {
    if (actual_mode == VMC_AUTO) strcpy (argument, "selected");
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
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_PAGE_VMC_CONTROL, D_VMC_CONTROL);
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
  TIME_T   current_dst;
  uint32_t current_time;
  uint8_t  mode, state;
  float    humidity;
  String   str_title, str_text, str_color;

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
    // read humidity and handle sensor source
    humidity  = VmcGetHumidity ();
    switch (vmc_humidity_source)
    {
      case VMC_SOURCE_NONE:  // no humidity source available 
        str_title = D_VMC_HUMIDITY;
        str_text  = "<b>---</b>";
        break;
      case VMC_SOURCE_LOCAL:  // local humidity source used 
        str_title = D_VMC_LOCAL + String (" ") + D_VMC_SENSOR;
        str_text  = "<b>" + String (humidity, 1) + "</b>";
        break;
      case VMC_SOURCE_REMOTE:  // remote humidity source used 
        str_title = D_VMC_REMOTE + String (" ") + D_VMC_SENSOR;
        str_text  = "<b>" + String (humidity, 1) + "</b>";
        break;
    }

    // add target humidity
    humidity = VmcGetTargetHumidity ();
    str_text += " / " + String (humidity, 0) + "%";

    // display
    WSContentSend_PD (PSTR("{s}%s{m}%s{e}"), str_title.c_str(), str_text.c_str());
  }

  // dislay current DST time
  current_time = LocalTime();
  BreakTime (current_time, current_dst);
  WSContentSend_PD (PSTR("{s}%s{m}%02d:%02d:%02d{e}"), D_VMC_TIME, current_dst.hour, current_dst.minute, current_dst.second);
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
    if (strlen(argument) > 0) VmcSetTargetHumidity (atof (argument));

    // get VMC humidity threshold according to THRESHOLD parameter
    WebGetArg (D_CMND_VMC_THRESHOLD, argument, VMC_BUFFER_SIZE);
    if (strlen(argument) > 0) VmcSetThreshold (atof (argument));
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
  int      graph_x, graph_y, graph_x1, graph_x2, graph_left, graph_right, graph_width, graph_pos;
  uint8_t  target, state_curr, state_prev;
  float    humidity, temperature, temp_min, temp_max, temp_scope;
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
    if (isnan (humidity) == false)
    {
      // adjust current temperature to acceptable range
      humidity = min (max (humidity, float (0)), float (100));

      // calculate current position
      graph_x = graph_left + (graph_width * index / VMC_GRAPH_SAMPLE);
      graph_y = VMC_GRAPH_HEIGHT - int (humidity * VMC_GRAPH_HEIGHT / 100);

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

  // dislay first time mark
  if (current_dst.minute <= 20)
  {
    current_dst.hour = (current_dst.hour + 1) % 24;
    graph_x  = graph_left;
    graph_x += (8 + (20 - current_dst.minute) % VMC_GRAPH_STEP) * graph_width / VMC_GRAPH_SAMPLE;
  }
  else
  {
    current_dst.hour = (current_dst.hour + 2) % 24;
    graph_x  = graph_left; 
    graph_x += (8 + (80 - current_dst.minute) % VMC_GRAPH_STEP) * graph_width / VMC_GRAPH_SAMPLE;
  }
  sprintf(str_hour, "%02d", current_dst.hour);
  WSContentSend_P (PSTR ("<text class='time' x='%d' y='52%%'>%sh</text>\n"), graph_x, str_hour);

  // dislay next 5 time marks (every 4 hours)
  for (index = 0; index < 5; index++)
  {
    current_dst.hour = (current_dst.hour + 4) % 24;
    graph_x += 48 * graph_width / VMC_GRAPH_SAMPLE;
    sprintf(str_hour, "%02d", current_dst.hour);
    WSContentSend_P (PSTR ("<text class='time' x='%d' y='52%%'>%sh</text>\n"), graph_x, str_hour);
  }

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
}

// VMC public web page
void VmcWebPageControl ()
{
  float value;

  // beginning of form without authentification with 60 seconds auto refresh
  WSContentStart_P (D_VMC_CONTROL, false);
  WSContentSend_P (PSTR ("</script>\n"));
  WSContentSend_P (PSTR ("<meta http-equiv='refresh' content='%d;URL=/%s' />\n"), 60, D_PAGE_VMC_CONTROL);
  
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
  WSContentSend_P (PSTR ("<div class='title bold orange'>%s %%</div>\n"), String (VmcGetHumidity (), 1).c_str());

  // display temperature graph
  WSContentSend_P (PSTR ("<div class='graph'>\n"));
  VmcWebDisplayGraph ();
  WSContentSend_P (PSTR ("</div>\n"));

  // end of page
  WSContentStop ();
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
      WebServer->on ("/" D_PAGE_VMC_CONTROL, VmcWebPageControl);
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
