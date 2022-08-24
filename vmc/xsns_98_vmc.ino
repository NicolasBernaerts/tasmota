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
    26/05/2020 - v2.7 - Add Information JSON page
    15/09/2020 - v2.8 - Remove /json page, based on Tasmota 8.4
                        Add status icons and mode control
    08/10/2020 - v3.0 - Handle graph with js auto update
    18/10/2020 - v3.1 - Expose icons on web server
    30/10/2020 - v3.2 - Real time graph page update
    05/11/2020 - v3.3 - Tasmota 9.0 compatibility
    11/11/2020 - v3.4 - Add /data.json for history data
    01/05/2021 - v3.5 - Remove use of String to avoid heap fragmentation 
    15/06/2021 - v3.6 - Bug fixes 
    20/06/2021 - v3.7 - Change in remote humidity sensor management (thanks to Bernard Monot) 
    24/02/2022 - v3.8 - Tasmota 10 compatibility
                        Sensor access rewrite
    22/08/2022 - v3.9 - Tasmota 12 & use generic sensor

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

/**************************************************\
 *                   VMC
\**************************************************/

#define XSNS_98                 98

// commands
#define D_CMND_VMC_HELP         "help"
#define D_CMND_VMC_MODE         "mode"
#define D_CMND_VMC_TARGET       "target"
#define D_CMND_VMC_THRESHOLD    "thres"
#define D_CMND_VMC_LOW          "low"
#define D_CMND_VMC_HIGH         "high"
#define D_CMND_VMC_AUTO         "auto"

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
#define D_VMC_CONTROL           "Control"
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
#define VMC_GRAPH_WIDTH         1200      
#define VMC_GRAPH_HEIGHT        400 
#define VMC_GRAPH_SAMPLE        800
#define VMC_GRAPH_REFRESH       108         // collect data every 108 sec to get 24h graph with 800 samples
#define VMC_GRAPH_MODE_LOW      20 
#define VMC_GRAPH_MODE_HIGH     40 
#define VMC_GRAPH_PERCENT_START 12      
#define VMC_GRAPH_PERCENT_STOP  88
#define VMC_GRAPH_TEMP_MIN      15      
#define VMC_GRAPH_TEMP_MAX      25       

// VMC data
#define VMC_HUMIDITY_MAX        100  
#define VMC_TARGET_MAX          99
#define VMC_TARGET_DEFAULT      50
#define VMC_THRESHOLD_MAX       10
#define VMC_THRESHOLD_DEFAULT   2

// vmc humidity source
enum VmcSources { VMC_SOURCE_NONE, VMC_SOURCE_LOCAL, VMC_SOURCE_REMOTE };

// vmc states
enum VmcStates { VMC_STATE_OFF, VMC_STATE_LOW, VMC_STATE_HIGH, VMC_STATE_NONE, VMC_STATE_MAX };

// vmc modes
enum VmcModes { VMC_MODE_DISABLED, VMC_MODE_LOW, VMC_MODE_HIGH, VMC_MODE_AUTO, VMC_MODE_MAX };
const char kVmcMode[] PROGMEM = "⏹️ Disabled|▶️ Low speed|⏩ High speed|🔁 Automatic";
const char kVmcModeGraph[] PROGMEM = "---|Low|High|Auto";

// vmc commands
const char kVmcCommands[] PROGMEM = "vmc_" "|" D_CMND_VMC_HELP "|" D_CMND_VMC_MODE "|" D_CMND_VMC_TARGET "|" D_CMND_VMC_THRESHOLD;
void (* const VmcCommand[])(void) PROGMEM = { &CmndVmcHelp, &CmndVmcMode, &CmndVmcTarget, &CmndVmcThreshold };

// graph units
const int   arr_graph_ypos[]    = {4,   27,   52,  77,   99};
const int   arr_graph_percent[] = {100, 75,   50,  25,   0};
const float arr_graph_scale[]   = {1,   0.75, 0.5, 0.25, 0};

// HTML chains
const char VMC_LINE_DASH[]        PROGMEM = "<line class='dash' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n";
const char VMC_TEXT_TEMPERATURE[] PROGMEM = "<text class='temperature' x='%d%%' y='%d%%'>%s°C</text>\n";
const char VMC_TEXT_HUMIDITY[]    PROGMEM = "<text class='humidity' x='%d%%' y='%d%%'>%d %%</text>\n";
const char VMC_TEXT_TIME[]        PROGMEM = "<text class='time' x='%d' y='%d%%'>%sh</text>\n";
const char VMC_INPUT_BUTTON[]     PROGMEM = "<button name='%s' class='button %s'>%s</button>\n";
const char VMC_INPUT_NUMBER[]     PROGMEM = "<p>%s<br/><input type='number' name='%s' min='0' max='%d' step='1' value='%d'></p>\n";
const char VMC_INPUT_OPTION[]     PROGMEM = "<option value='%d' %s>%s</option>";

#ifdef USE_UFILESYS

#define D_VMC_CFG                  "vmc.cfg"      // configuration file

#endif    // USE_UFILESYS

/*************************************************\
 *               Variables
\*************************************************/

// configuration variables
struct {
  uint8_t mode      = VMC_MODE_MAX;            // default running mode
  uint8_t target    = UINT8_MAX;               // target humidity level
  uint8_t threshold = UINT8_MAX;               // humidity level threshold
} vmc_config;

// status variables
struct {
  uint8_t humidity    = UINT8_MAX;               // last read humidity level
  float   temperature = NAN;                     // last temperature read
} vmc_status;

// graph variables
struct {
  uint32_t index    = 0;
  uint32_t counter  = 0;
  uint8_t  humidity = UINT8_MAX;
  uint8_t  target   = UINT8_MAX;
  uint8_t  state    = VMC_STATE_NONE;
  float    temperature = NAN;
  float    temp_min, temp_max;
  uint8_t  arr_state[VMC_GRAPH_SAMPLE];
  uint8_t  arr_humidity[VMC_GRAPH_SAMPLE];
  uint8_t  arr_target[VMC_GRAPH_SAMPLE];
  float    arr_temperature[VMC_GRAPH_SAMPLE];
} vmc_graph;

/**************************************************\
 *                  Accessors
\**************************************************/

// get VMC state from relays state
uint8_t VmcGetRelayState ()
{
  uint8_t vmc_relay1 = 0;
  uint8_t vmc_relay2 = 1;
  uint8_t relay_state = VMC_STATE_OFF;

  // read relay states
  vmc_relay1 = bitRead (TasmotaGlobal.power, 0);
  if (TasmotaGlobal.devices_present == 2) vmc_relay2 = bitRead (TasmotaGlobal.power, 1);

  // convert to vmc state
  if ((vmc_relay1 == 0) && (vmc_relay2 == 1)) relay_state = VMC_STATE_LOW;
  else if (vmc_relay1 == 1) relay_state = VMC_STATE_HIGH;

  return relay_state;
}

// set relays state
void VmcSetRelayState (uint8_t new_state)
{
  // set relays
  switch (new_state)
  {
    case VMC_STATE_OFF:  // VMC disabled
      ExecuteCommandPower (1, POWER_OFF, SRC_MAX);
      if (TasmotaGlobal.devices_present == 2) ExecuteCommandPower (2, POWER_OFF, SRC_MAX);
      break;
    case VMC_STATE_LOW:  // VMC low speed
      ExecuteCommandPower (1, POWER_OFF, SRC_MAX);
      if (TasmotaGlobal.devices_present == 2) ExecuteCommandPower (2, POWER_ON, SRC_MAX);
      break;
    case VMC_STATE_HIGH:  // VMC high speed
      if (TasmotaGlobal.devices_present == 2) ExecuteCommandPower (2, POWER_OFF, SRC_MAX);
      ExecuteCommandPower (1, POWER_ON, SRC_MAX);
      break;
  }
}

// Load configuration
void VmcLoadConfig ()
{
#ifdef USE_UFILESYS

  // retrieve saved settings from flash filesystem
  vmc_config.mode      = UfsCfgLoadKeyInt (D_VMC_CFG, D_CMND_VMC_MODE, VMC_MODE_AUTO);
  vmc_config.target    = UfsCfgLoadKeyInt (D_VMC_CFG, D_CMND_VMC_TARGET, VMC_TARGET_DEFAULT);
  vmc_config.threshold = UfsCfgLoadKeyInt (D_VMC_CFG, D_CMND_VMC_THRESHOLD, VMC_THRESHOLD_DEFAULT);

#else

  // retrieve saved settings from flash memory
  vmc_config.mode      = (uint8_t)Settings->weight_reference;
  vmc_config.target    = (uint8_t)Settings->weight_max;
  vmc_config.threshold = (uint8_t)Settings->weight_calibration;

# endif     // USE_UFILESYS

  // check out of range values
  if (vmc_config.mode >= VMC_MODE_MAX) vmc_config.mode = VMC_MODE_AUTO;
  if ((vmc_config.target == 0) || (vmc_config.target >= 100)) vmc_config.target = VMC_TARGET_DEFAULT;
  if ((vmc_config.threshold == 0) || (vmc_config.threshold >= 25)) vmc_config.target = VMC_THRESHOLD_DEFAULT;
}

// Save configuration
void VmcSaveConfig ()
{
#ifdef USE_UFILESYS

  // save settings into flash filesystem
  UfsCfgSaveKeyInt (D_VMC_CFG, D_CMND_VMC_MODE, vmc_config.mode, true);
  UfsCfgSaveKeyInt (D_VMC_CFG, D_CMND_VMC_TARGET, vmc_config.target, false);
  UfsCfgSaveKeyInt (D_VMC_CFG, D_CMND_VMC_THRESHOLD, vmc_config.threshold, false);

#else

  // save settings into flash memory
  Settings->weight_reference   = (uint32_t)vmc_config.mode;
  Settings->weight_max         = (uint16_t)vmc_config.target;
  Settings->weight_calibration = (uint32_t)vmc_config.threshold;

# endif     // USE_UFILESYS
}

// Show JSON status (for MQTT)
void VmcShowJSON (bool append)
{
  uint8_t humidity, value;
  float   temperature;
  char    str_text[16];

  // get mode and humidity
  GetTextIndexed (str_text, sizeof (str_text), vmc_config.mode, kVmcMode);
  
  // add , in append mode or { in direct publish mode
  if (append) ResponseAppend_P (PSTR (",")); else Response_P (PSTR ("{"));

  // vmc mode  -->  "VMC":{"Mode":4,"Label":"Automatic","Temperature":18.4,"Humidity":70.5,"Target":50}
  ResponseAppend_P (PSTR ("\"%s\":{"), D_JSON_VMC);
  ResponseAppend_P (PSTR (",\"%s\":%d"), D_JSON_VMC_MODE, vmc_config.mode);
  ResponseAppend_P (PSTR (",\"%s\":\"%s\""), D_JSON_VMC_LABEL, str_text);

  // temperature
  temperature = SensorReadTemperature ();
  ext_snprintf_P (str_text, sizeof(str_text), PSTR ("%1_f"), &temperature);
  ResponseAppend_P (PSTR (",\"%s\":%s"), D_JSON_VMC_TEMPERATURE, str_text);

  // humidity level
  humidity = (uint8_t)SensorReadHumidity ();
  if (humidity != UINT8_MAX) sprintf (str_text, "%d", humidity); else strcpy (str_text, "n/a");
  ResponseAppend_P (PSTR (",\"%s\":%s"), D_JSON_VMC_HUMIDITY, str_text);

  // target humidity
  ResponseAppend_P (PSTR (",\"%s\":%u"), D_JSON_VMC_TARGET, vmc_config.target);

  // humidity thresold
  ResponseAppend_P (PSTR (",\"%s\":%u"), D_JSON_VMC_THRESHOLD, vmc_config.threshold);

  ResponseAppend_P (PSTR ("}"));

  // if VMC mode is enabled
  if (vmc_config.mode != VMC_MODE_DISABLED)
  {
    // get relay state and label
    value = VmcGetRelayState ();
    GetTextIndexed (str_text, sizeof (str_text), value, kVmcMode);

    // relay state  -->  ,"State":{"Mode":1,"Label":"On"}
    ResponseAppend_P (PSTR (",\"%s\":{"), D_JSON_VMC_STATE);
    ResponseAppend_P (PSTR ("\"%s\":%d"), D_JSON_VMC_MODE, value);
    ResponseAppend_P (PSTR (",\"%s\":\"%s\""), D_JSON_VMC_LABEL, str_text);
    ResponseAppend_P (PSTR ("}"));
  }

  // publish it if not in append mode
  if (!append)
  {
    ResponseAppend_P (PSTR ("}"));
    MqttPublishTeleSensor ();
  }
}

/**************************************************\
 *                  Commands
\**************************************************/

// vmc help
void CmndVmcHelp ()
{
  uint8_t index;
  char    str_text[32];

  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: vmc_mode = running mode"));
  for (index = 0; index < VMC_MODE_MAX; index++)
  {
    GetTextIndexed (str_text, sizeof (str_text), index, kVmcMode);
    AddLog (LOG_LEVEL_INFO, PSTR ("HLP:   %u - %s"), index, str_text);
  }
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: vmc_target = target humidity level (%%)"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: vmc_thres  = humidity threshold (%%)"));

  ResponseCmndDone();
}

// set vmc mode
void CmndVmcMode ()
{
  // set activation state
  if (XdrvMailbox.data_len > 0)
  {
    if (XdrvMailbox.payload < VMC_MODE_MAX) 
    {
      vmc_config.mode = XdrvMailbox.payload;
      VmcSaveConfig ();
    }
  }

  ResponseCmndNumber (vmc_config.mode);
}

// set vmc target humidity level
void CmndVmcTarget ()
{
  // set activation state
  if (XdrvMailbox.data_len > 0)
  {
    if ((XdrvMailbox.payload > 0) && (XdrvMailbox.payload < 100)) 
    {
      vmc_config.target = XdrvMailbox.payload;
      VmcSaveConfig ();
    }
  }

  ResponseCmndNumber (vmc_config.target);
}

// set vmc humidity level threshold
void CmndVmcThreshold ()
{
  // set activation state
  if (XdrvMailbox.data_len > 0)
  {
    if ((XdrvMailbox.payload > 0) && (XdrvMailbox.payload < 50)) 
    {
      vmc_config.threshold = XdrvMailbox.payload;
      VmcSaveConfig ();
    }
  }

  ResponseCmndNumber (vmc_config.threshold);
}

// update graph history data
void VmcUpdateGraphData ()
{
  // set indexed graph values with current values
  vmc_graph.arr_temperature[vmc_graph.index] = vmc_graph.temperature;
  vmc_graph.arr_humidity[vmc_graph.index] = vmc_graph.humidity;
  vmc_graph.arr_target[vmc_graph.index] = vmc_graph.target;
  vmc_graph.arr_state[vmc_graph.index] = vmc_graph.state;

  // init current values
  vmc_graph.temperature = NAN;
  vmc_graph.humidity = UINT8_MAX;
  vmc_graph.target = UINT8_MAX;
  vmc_graph.state = VMC_STATE_OFF;

  // increase graph data index and reset if max reached
  vmc_graph.index ++;
  vmc_graph.index = vmc_graph.index % VMC_GRAPH_SAMPLE;
}

// update sensors and adjust VMC mode
void VmcEverySecond ()
{
  bool    json_update = false;
  uint8_t humidity, actual_state, target_state;
  float   temperature, value;
  float   compar_current, compar_read, compar_diff;

  // update current temperature
  temperature = SensorReadTemperature ();
  if (!isnan(temperature))
  {
    // if temperature was previously mesured, compare
    if (!isnan(vmc_status.temperature))
    {
      // update JSN if temperature is at least 0.2°C different
      compar_current = floor (10 * vmc_status.temperature);
      compar_read    = floor (10 * temperature);
      compar_diff    = abs (compar_current - compar_read);
      if (compar_diff >= 2) json_update = true;

      // update temperature
      vmc_status.temperature = temperature;
    }

    // update graph value
    if (isnan(vmc_graph.temperature)) vmc_graph.temperature = temperature;
      else vmc_graph.temperature = min (vmc_graph.temperature, temperature);
  }

  // update current humidity
  humidity = (uint8_t)SensorReadHumidity ();
  if (humidity != UINT8_MAX)
  {
    // save current value and ask for JSON update if any change
    if (vmc_status.humidity != humidity) json_update = true;
    vmc_status.humidity = humidity;

    // update graph value
    if (vmc_graph.humidity == UINT8_MAX) vmc_graph.humidity = humidity;
      else vmc_graph.humidity = max (vmc_graph.humidity, humidity);
  }

  // update graph value
  if (vmc_graph.target == UINT8_MAX) vmc_graph.target = vmc_config.target;
    else vmc_graph.target = min (vmc_graph.target, vmc_config.target);

  // update relay state
  actual_state = VmcGetRelayState ();
  if (vmc_graph.state != VMC_STATE_HIGH) vmc_graph.state = actual_state;

  // get VMC mode and consider as low if mode disabled and single relay
  if ((vmc_config.mode == VMC_MODE_DISABLED) && (TasmotaGlobal.devices_present == 1)) vmc_config.mode = VMC_MODE_LOW;

  // determine relay target state according to vmc mode
  target_state = actual_state;
  switch (vmc_config.mode)
  {
    case VMC_MODE_LOW: 
      target_state = VMC_STATE_LOW;
      break;
    case VMC_MODE_HIGH:
      target_state = VMC_STATE_HIGH;
      break;
    case VMC_MODE_AUTO: 
      // if humidity is low enough, target VMC state is low speed
      if (humidity + vmc_config.threshold < vmc_config.target) target_state = VMC_STATE_LOW;
      
      // else, if humidity is too high, target VMC state is high speed
      else if (humidity > vmc_config.target + vmc_config.threshold) target_state = VMC_STATE_HIGH;
      break;
  }

  // if VMC state is different than target state, set relay
  if (actual_state != target_state)
  {
    VmcSetRelayState (target_state);
    json_update = true;
  }
  
  // if JSON update needed, publish
  if (json_update) VmcShowJSON (false);

  // increment delay counter and if delay reached, update history data
  if (vmc_graph.counter == 0) VmcUpdateGraphData ();
  vmc_graph.counter ++;
  vmc_graph.counter = vmc_graph.counter % VMC_GRAPH_REFRESH;
}

// initialisation
void VmcInit ()
{
  int index;

  // initialise graph data
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    vmc_graph.arr_state[index]       = VMC_STATE_NONE;
    vmc_graph.arr_humidity[index]    = UINT8_MAX;
    vmc_graph.arr_target[index]      = UINT8_MAX;
    vmc_graph.arr_temperature[index] = NAN;
  }

  // load configuration
  VmcLoadConfig ();

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: vmc_help to get help on vmc commands"));
}

// intercept relay command, if from anything else than SRC_MAX, ignore it
bool VmcSetDevicePower ()
{
  bool result = false;

  // if command is not from the module, ignore it
  if (XdrvMailbox.payload != SRC_MAX)
  {
    // ignore action and log it
    result = true;
    AddLog (LOG_LEVEL_INFO, PSTR ("VMC: Relay order ignored from %u"), XdrvMailbox.payload);
  }

  return result;
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// get status emoji
void VmcWebGetStatusEmoji (char* pstr_emoji, size_t size_emoji)
{
  uint8_t device_state = VmcGetRelayState ();

  if (device_state == VMC_STATE_HIGH) strlcpy (pstr_emoji, "⏩", size_emoji);
  else if (device_state == VMC_STATE_LOW) strlcpy (pstr_emoji, "▶️", size_emoji);
  else strlcpy (pstr_emoji, "⏹️", size_emoji);
}

// intermediate page to update running mode from main page
void VmcWebUpdateMode ()
{
  uint8_t mode = UINT8_MAX;
  char    str_argument[8];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // check for 'mode' parameter
  WebGetArg (D_CMND_VMC_MODE, str_argument, sizeof (str_argument));
  if (strlen (str_argument) > 0) mode = atoi (str_argument);
  if (mode < VMC_MODE_MAX)
  {
      vmc_config.mode = mode;
      VmcSaveConfig ();
  }

  // auto reload root page with dark background
  WSContentStart_P ("", false);
  WSContentSend_P (PSTR ("</script>\n"));
  WSContentSend_P (PSTR ("<meta http-equiv='refresh' content='0;URL=/' />\n"));
  WSContentSend_P (PSTR ("</head>\n"));
  WSContentSend_P (PSTR ("<body bgcolor='#303030'></body>\n"));
  WSContentSend_P (PSTR ("</html>\n"));
  WSContentEnd ();
}

// add buttons on main page
void VmcWebMainButton ()
{
  uint8_t index;
  char    str_text[32];

  // control button
  WSContentSend_P (PSTR ("<p><form action='control' method='get'><button>%s</button></form></p>\n"), D_VMC_CONTROL);

  // status mode options
  WSContentSend_P (PSTR ("<p><form action='/mode.upd' method='get'>\n"));
  WSContentSend_P (PSTR ("<select style='width:100%%;text-align:center;padding:8px;border-radius:0.3rem;' name='%s' onchange='this.form.submit();'>\n"), D_CMND_VMC_MODE);
  WSContentSend_P (PSTR ("<option value='%u'>%s</option>\n"), UINT8_MAX, PSTR ("-- Select mode --"));
  for (index = VMC_MODE_DISABLED; index < VMC_MODE_MAX; index ++)
  {
    // display mode
    GetTextIndexed (str_text, sizeof (str_text), index, kVmcMode);
    WSContentSend_P (PSTR ("<option value='%u'>%s</option>\n"), index, str_text);
  }
  WSContentSend_P (PSTR ("</select>\n"));
  WSContentSend_P (PSTR ("</form></p>\n"));
}

// add buttons on configuration page
void VmcWebConfigButton ()
{
  // config button
  WSContentSend_P (PSTR ("<p><form action='vmc' method='get'><button>%s</button></form></p>\n"), D_VMC_CONFIGURE);
}

// append VMC state to main page
void VmcWebSensor ()
{
  uint8_t humidity;
  char str_text[16];

  // if automatic mode, add target humidity
  humidity = (uint8_t)SensorReadHumidity ();
  if (vmc_config.mode == VMC_MODE_AUTO)
  {
    if (humidity != UINT8_MAX) WSContentSend_PD (PSTR ("{s}%s{m}%u / %u %%{e}"), D_VMC_TARGET, humidity, vmc_config.target);
      else WSContentSend_PD (PSTR ("{s}%s{m}No data{e}"), D_VMC_TARGET);
  }

  // display vmc icon status
  VmcWebGetStatusEmoji (str_text, sizeof (str_text));
  WSContentSend_PD (PSTR ("{s}%s{m}%s{e}"), D_VMC_MODE, str_text);
}

// VMC web page
void VmcWebPageConfig ()
{
  uint8_t index, value;
  char    str_argument[16];
  char    str_text[32];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess ()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg ("save"))
  {
    // get VMC mode according to MODE parameter
    WebGetArg (D_CMND_VMC_MODE, str_argument, sizeof (str_argument));
    if (strlen(str_argument) > 0) vmc_config.mode = (uint8_t)atoi (str_argument); 

    // get VMC target humidity according to TARGET parameter
    WebGetArg (D_CMND_VMC_TARGET, str_argument, sizeof (str_argument));
    if (strlen(str_argument) > 0) vmc_config.target = (uint8_t)atoi (str_argument);

    // get VMC humidity threshold according to THRESHOLD parameter
    WebGetArg (D_CMND_VMC_THRESHOLD, str_argument, sizeof (str_argument));
    if (strlen(str_argument) > 0) vmc_config.threshold = (uint8_t) atoi (str_argument);

    // save configuration
    VmcSaveConfig ();
  }

  // beginning of form
  WSContentStart_P (D_VMC_CONFIGURE);
  WSContentSendStyle ();
  WSContentSend_P (PSTR ("<form method='get' action='vmc'>\n"));

  WSContentSend_P (PSTR ("<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>\n"), D_VMC_PARAMETERS);

  // selection : start
  WSContentSend_P (PSTR ("<p>%s<br/><select name='%s'>"), D_VMC_MODE, D_CMND_VMC_MODE);

  for (index = VMC_MODE_DISABLED; index < VMC_MODE_MAX; index++)
  {
    if (vmc_config.mode == index) strcpy (str_argument, "selected"); else strcpy (str_argument, "");
    GetTextIndexed (str_text, sizeof (str_text), index, kVmcMode);
    WSContentSend_P (VMC_INPUT_OPTION, index, str_argument, str_text);
  }

  // selection : end
  WSContentSend_P (PSTR ("</select></p>\n"));

  // target humidity label and input
  WSContentSend_P (VMC_INPUT_NUMBER, D_VMC_TARGET, D_CMND_VMC_TARGET, VMC_TARGET_MAX, vmc_config.target);

  // humidity threshold label and input
  WSContentSend_P (VMC_INPUT_NUMBER, D_VMC_THRESHOLD, D_CMND_VMC_THRESHOLD, VMC_THRESHOLD_MAX, vmc_config.threshold);

  WSContentSend_P (PSTR ("</fieldset></p>\n"));

  // save button
  WSContentSend_P (PSTR ("<p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), D_SAVE);
  WSContentSend_P (PSTR ("</form>"));

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

// Temperature & humidity graph data
void VmcWebGraphData ()
{
  bool     draw;
  int      index, array_idx;
  int      graph_left, graph_right, graph_width;
  int      graph_x, graph_y, graph_off, graph_low, graph_high;
  int      unit_width, shift_unit, shift_width;
  uint8_t  humidity;
  float    temperature, temp_min, temp_max, temp_range;
  TIME_T   current_dst;
  uint32_t current_time;

  // vmc state graph position
  graph_off  = VMC_GRAPH_HEIGHT;
  graph_low  = VMC_GRAPH_HEIGHT - VMC_GRAPH_MODE_LOW;
  graph_high = VMC_GRAPH_HEIGHT - VMC_GRAPH_MODE_HIGH;
  temp_range = vmc_graph.temp_max - vmc_graph.temp_min;

  // boundaries of SVG graph
  graph_left  = VMC_GRAPH_PERCENT_START * VMC_GRAPH_WIDTH / 100;
  graph_right = VMC_GRAPH_PERCENT_STOP * VMC_GRAPH_WIDTH / 100;
  graph_width = graph_right - graph_left;

  // start of SVG graph
  WSContentBegin (200, CT_HTML);
  WSContentSend_P (PSTR ("<svg viewBox='%d %d %d %d' preserveAspectRatio='xMinYMinmeet'>\n"), 0, 0, VMC_GRAPH_WIDTH, VMC_GRAPH_HEIGHT);

  // SVG style 
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));
  WSContentSend_P (PSTR ("polyline {fill:none;stroke-width:1;}\n"));
  WSContentSend_P (PSTR ("polyline.vmc {stroke:white;stroke-width:2;}\n"));
  WSContentSend_P (PSTR ("polyline.target {stroke:orange;stroke-dasharray:4;}\n"));
  WSContentSend_P (PSTR ("polyline.temperature {stroke:yellow;}\n"));
  WSContentSend_P (PSTR ("polyline.humidity {stroke:orange;}\n"));
  WSContentSend_P (PSTR ("line.time {stroke:white;stroke-width:1;}\n"));
  WSContentSend_P (PSTR ("text.time {font-size:16px;fill:white;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // ---------------
  //     Curves
  // ---------------

  // loop to display vmc state curve
  WSContentSend_P (PSTR ("<polyline class='vmc' points='"));
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    // get array index and read vmc state
    array_idx  = (index + vmc_graph.index) % VMC_GRAPH_SAMPLE;
    if (vmc_graph.arr_state[array_idx] != VMC_STATE_NONE)
    {
      // calculate current position and add the point to the line
      graph_x = graph_left + (graph_width * index / VMC_GRAPH_SAMPLE);
      if (vmc_graph.arr_state[array_idx] == VMC_STATE_HIGH) graph_y = graph_high;
      else if (vmc_graph.arr_state[array_idx] == VMC_STATE_LOW) graph_y = graph_low;
      else graph_y = graph_off;
      WSContentSend_P (PSTR ("%d,%d "), graph_x, graph_y);
    }
  }
  WSContentSend_P (PSTR ("'/>\n"));

  // loop to display target humidity curve
  WSContentSend_P (PSTR ("<polyline class='target' points='"));
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    // if humidity has been mesured
    array_idx = (index + vmc_graph.index) % VMC_GRAPH_SAMPLE;
    if (vmc_graph.arr_target[array_idx] != UINT8_MAX)
    {
      // calculate current position and add the point to the line
      graph_x = graph_left + (graph_width * index / VMC_GRAPH_SAMPLE);
      graph_y = VMC_GRAPH_HEIGHT - (vmc_graph.arr_target[array_idx] * VMC_GRAPH_HEIGHT / 100);
      WSContentSend_P (PSTR ("%d,%d "), graph_x, graph_y);
    }
  }
  WSContentSend_P (PSTR ("'/>\n"));

  // loop to display temperature curve
  WSContentSend_P (PSTR ("<polyline class='temperature' points='"));
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    // if temperature has been mesured
    array_idx   = (index + vmc_graph.index) % VMC_GRAPH_SAMPLE;
    if (!isnan (vmc_graph.arr_temperature[array_idx]))
    {
      // calculate current position and add the point to the line
      graph_x = graph_left + (graph_width * index / VMC_GRAPH_SAMPLE);
      graph_y = VMC_GRAPH_HEIGHT - int (((vmc_graph.arr_temperature[array_idx] - vmc_graph.temp_min) / temp_range) * VMC_GRAPH_HEIGHT);
      WSContentSend_P (PSTR ("%d,%d "), graph_x, graph_y);
    }
  }
  WSContentSend_P (PSTR ("'/>\n"));

  // loop to display humidity curve
  WSContentSend_P (PSTR ("<polyline class='humidity' points='"));
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    // if temperature value is defined
    array_idx = (index + vmc_graph.index) % VMC_GRAPH_SAMPLE;
    if (vmc_graph.arr_humidity[array_idx] != UINT8_MAX)
    {
      // calculate current position and add the point to the line
      graph_x = graph_left + (graph_width * index / VMC_GRAPH_SAMPLE);
      graph_y = VMC_GRAPH_HEIGHT - (vmc_graph.arr_humidity[array_idx] * VMC_GRAPH_HEIGHT / 100);
      WSContentSend_P (PSTR ("%d,%d "), graph_x, graph_y);
    }
  }
  WSContentSend_P (PSTR ("'/>\n"));

  // ---------------
  //   Time line
  // ---------------

  // get current time
  current_time = LocalTime ();
  BreakTime (current_time, current_dst);

  // calculate horizontal shift
  unit_width  = graph_width / 6;
  shift_unit  = current_dst.hour % 4;
  shift_width = unit_width - (unit_width * shift_unit / 4) - (unit_width * current_dst.minute / 240);

  // calculate first time displayed by substracting (5 * 4h + shift) to current time
  current_time -= (5 * 14400) + (shift_unit * 3600); 

  // display 4 hours separation lines with hour
  for (index = 0; index < 6; index++)
  {
    // convert back to date and increase time of 4h
    BreakTime (current_time, current_dst);
    current_time += 14400;

    // display separation line and time
    graph_x = graph_left + shift_width + (index * unit_width);
    WSContentSend_P (PSTR ("<line class='time' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n"), graph_x, 49, graph_x, 51);
    WSContentSend_P (PSTR ("<text class='time' x='%d' y='%d%%'>%02dh</text>\n"), graph_x - 15, 55, current_dst.hour);
  }

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentEnd ();
}

// Temperature & humidity graph frame
void VmcWebGraphFrame ()
{
  int   index;
  int   graph_left, graph_right, graph_width;
  float temperature, temp_range;
  char  str_value[8];

  // start of SVG graph
  WSContentBegin (200, CT_HTML);
  WSContentSend_P (PSTR ("<svg viewBox='%d %d %d %d' preserveAspectRatio='xMinYMinmeet'>\n"), 0, 0, VMC_GRAPH_WIDTH, VMC_GRAPH_HEIGHT);

  // SVG style 
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));
  WSContentSend_P (PSTR ("rect {stroke:grey;fill:none;}\n"));
  WSContentSend_P (PSTR ("line.dash {stroke:grey;stroke-width:1;stroke-dasharray:8;}\n"));
  WSContentSend_P (PSTR ("text.temperature {font-size:18px;fill:yellow;}\n"));
  WSContentSend_P (PSTR ("text.humidity {font-size:18px;fill:orange;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // loop to adjust min and max temperature
  vmc_graph.temp_min = VMC_GRAPH_TEMP_MIN;
  vmc_graph.temp_max = VMC_GRAPH_TEMP_MAX;
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    // if needed, adjust minimum and maximum temperature
    if (!isnan (vmc_graph.arr_temperature[index]))
    {
      if (vmc_graph.temp_min > vmc_graph.arr_temperature[index]) vmc_graph.temp_min = floor (vmc_graph.arr_temperature[index]);
      if (vmc_graph.temp_max < vmc_graph.arr_temperature[index]) vmc_graph.temp_max = ceil (vmc_graph.arr_temperature[index]);
    } 
  }
  temp_range = vmc_graph.temp_max - vmc_graph.temp_min;

  // boundaries of SVG graph
  graph_left  = VMC_GRAPH_PERCENT_START * VMC_GRAPH_WIDTH / 100;
  graph_right = VMC_GRAPH_PERCENT_STOP * VMC_GRAPH_WIDTH / 100;
  graph_width = graph_right - graph_left;

  // graph curve zone
  WSContentSend_P (PSTR ("<rect x='%d%%' y='%d%%' width='%d%%' height='%d%%' rx='10' />\n"), VMC_GRAPH_PERCENT_START, 0, VMC_GRAPH_PERCENT_STOP - VMC_GRAPH_PERCENT_START, 100);

  // graph separation lines (75, 50 and 25)
  for (index = 1; index < 4; index ++) WSContentSend_P (VMC_LINE_DASH, graph_left, arr_graph_percent[index], graph_right, arr_graph_percent[index]);

  // temperature an humidity units
  for (index = 0; index < 5; index ++)
  {
    // temperature
    temperature = vmc_graph.temp_min + temp_range * arr_graph_scale[index];
    ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &temperature);
    WSContentSend_P (VMC_TEXT_TEMPERATURE, 2, arr_graph_ypos[index], str_value);

    // humidity
    WSContentSend_P (VMC_TEXT_HUMIDITY, VMC_GRAPH_PERCENT_STOP + 2, arr_graph_ypos[index], arr_graph_percent[index]);
  }

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentEnd ();
}

// update status for web client
// format is A1;A2;A3
//   A1 : status icon (off.png, low.png, high.png)
//   A2 : temperature value
//   A3 : humidity value
void VmcWebUpdateData ()
{
  float   temperature;
  uint8_t device_state;
  uint8_t humidity;
  char  str_text[16];
  char  str_value[8];

  // start of data page
  WSContentBegin (200, CT_PLAIN);

  // A1 : icon state update
  VmcWebGetStatusEmoji (str_text, sizeof (str_text));
  WSContentSend_P (PSTR ("%s;"), str_text);

  // A2 : temperature value
  temperature = SensorReadTemperature ();
  if (!isnan (temperature)) ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%1_f"), &temperature); else strcpy (str_value, "---");
  WSContentSend_P (PSTR ("%s;"), str_value);

  // A3 : humidity value
  humidity = (uint8_t)SensorReadHumidity ();
  if (humidity != UINT8_MAX) WSContentSend_P (PSTR ("%d"), humidity); else WSContentSend_P (PSTR ("---"));

  // end of data page
  WSContentEnd ();
}

// VMC control public web page
void VmcWebPageControl ()
{
//  bool    updated = false;
  int     index;
  float   value;
  long    percentage;
  char    str_argument[8];
  char    str_text[16];

  // check if vmc state has changed
  if (Webserver->hasArg (D_CMND_VMC_MODE))
  {
    // get VMC mode according to MODE parameter
    WebGetArg (D_CMND_VMC_MODE, str_argument, sizeof (str_argument));
    if (strlen(str_argument) > 0) vmc_config.mode = (uint8_t)atoi (str_argument); 
  }

  // beginning page without authentification
  WSContentStart_P (D_VMC_CONTROL, false);
  WSContentSend_P (PSTR ("</script>\n"));

  // page data refresh script
  WSContentSend_P (PSTR ("<script type='text/javascript'>\n"));

  WSContentSend_P (PSTR ("function updateData() {\n"));
  WSContentSend_P (PSTR ("httpUpd=new XMLHttpRequest();\n"));
  WSContentSend_P (PSTR ("httpUpd.onreadystatechange=function() {\n"));
  WSContentSend_P (PSTR (" if (httpUpd.responseText.length>0) {\n"));
  WSContentSend_P (PSTR ("  arr_param=httpUpd.responseText.split(';');\n"));
  WSContentSend_P (PSTR ("  document.getElementById('mode').innerHTML=arr_param[0];\n"));            // fan mode emoji
  WSContentSend_P (PSTR ("  document.getElementById('temp').innerHTML=arr_param[1];\n"));            // temperature value
  WSContentSend_P (PSTR ("  document.getElementById('humi').innerHTML=arr_param[2];\n"));            // humidity value
  WSContentSend_P (PSTR (" }\n"));
  WSContentSend_P (PSTR ("}\n"));
  WSContentSend_P (PSTR ("httpUpd.open('GET','data.upd',true);\n"));
  WSContentSend_P (PSTR ("httpUpd.send();\n"));
  WSContentSend_P (PSTR ("}\n"));

  WSContentSend_P (PSTR ("function updateGraph()\n"));
  WSContentSend_P (PSTR ("{\n"));
  WSContentSend_P (PSTR (" str_random=Math.floor(Math.random()*100000);\n"));
  WSContentSend_P (PSTR (" document.getElementById('data').data='data.svg?rnd='+str_random;\n"));
  WSContentSend_P (PSTR ("}\n"));

  WSContentSend_P (PSTR ("setInterval(function() {updateData();},1000);\n"));
  WSContentSend_P (PSTR ("setInterval(function() {updateGraph();},10000);\n"));

  WSContentSend_P (PSTR ("</script>\n"));

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div.main {margin:10px auto;padding:10px;text-align:center;vertical-align:middle;}\n"));

  WSContentSend_P (PSTR ("div.title {font-size:2rem;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.title a {color:white;}\n"));

  WSContentSend_P (PSTR ("div.value {font-size:3rem;padding:0px;}\n"));
  WSContentSend_P (PSTR ("div span {font-size:2.5rem;padding-left:1rem;}\n"));
  WSContentSend_P (PSTR (".temp {color:yellow;}\n"));
  WSContentSend_P (PSTR (".humi {color:orange;}\n"));
  WSContentSend_P (PSTR ("div.mode {font-size:5rem;padding:15px;}\n"));

  WSContentSend_P (PSTR ("div.choice {display:inline-block;font-size:1.5rem;margin:0.5rem;border:1px #666 solid;background:none;color:#fff;border-radius:6px;margin-bottom:40px;}\n"));
  WSContentSend_P (PSTR ("div.choice a {color:white;}\n"));
  WSContentSend_P (PSTR ("div.item {display:inline-block;width:80px;padding:0.2rem auto;margin:1px;border:none;border-radius:4px;background:none;}\n"));
  WSContentSend_P (PSTR ("div.item:hover {background:#aaa;}\n"));
  WSContentSend_P (PSTR ("div.active {background:#666;}\n"));

  WSContentSend_P (PSTR ("a:link {text-decoration:none;}\n"));

  percentage = (100 * VMC_GRAPH_HEIGHT / VMC_GRAPH_WIDTH) + 2; 
  WSContentSend_P (PSTR (".svg-container {position:relative;width:100%%;max-width:%dpx;padding-top:%d%%;margin:auto;}\n"), VMC_GRAPH_WIDTH, percentage);
  WSContentSend_P (PSTR (".svg-content {display:inline-block;position:absolute;top:0;left:0;}\n"));

  WSContentSend_P (PSTR ("</style>\n"));

  WSContentSend_P (PSTR ("</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  // room name
  WSContentSend_P (PSTR ("<div class='title'><a href='/'>%s</a></div>\n"), SettingsText(SET_DEVICENAME));

  // temperature
  value = SensorReadTemperature ();
  ext_snprintf_P (str_text, sizeof (str_text), PSTR ("%1_f"), &value);
  WSContentSend_P (PSTR ("<div class='value temp' id='temp'>%s<span class='unit'>°C</span></div>\n"), str_text);

  // humidity
  WSContentSend_P (PSTR ("<div class='value humi' id='humi'>%u<span class='unit'>%%"), (uint8_t)SensorReadHumidity ());
  if (vmc_config.mode == VMC_MODE_AUTO) WSContentSend_P (PSTR ("<small> (%u%%)</small>"), vmc_config.target);
  WSContentSend_P (PSTR ("</span></div>\n"));

  // vmc icon status
  VmcWebGetStatusEmoji (str_text, sizeof (str_text));
  WSContentSend_PD (PSTR ("<div class='mode' id='mode'>%s</div>\n"), str_text);

  // vmc mode selector
  WSContentSend_P (PSTR ("<div class='choice'>\n"));
  for (index = VMC_MODE_LOW; index < VMC_MODE_MAX; index++)
  {
    // get button command and label
    GetTextIndexed (str_text, sizeof (str_text), index, kVmcModeGraph);

    // display mode button
    if (vmc_config.mode == index) strcpy (str_argument, "active"); else strcpy (str_argument, "");
    WSContentSend_P (PSTR ("<a href='control?%s=%d'><div class='item %s'>%s</div></a>\n"), D_CMND_VMC_MODE, index, str_argument, str_text);
  }
  WSContentSend_P (PSTR ("</div>\n"));

  // graph base and data
  WSContentSend_P (PSTR ("<div class='svg-container'>\n"));
  WSContentSend_P (PSTR ("<object class='svg-content' id='base' type='image/svg+xml' width='%d%%' height='%d%%' data='base.svg'></object>\n"), 100, 100);
  WSContentSend_P (PSTR ("<object class='svg-content' id='data' type='image/svg+xml' width='%d%%' height='%d%%' data='data.svg?rnd=0'></object>\n"), 100, 100);
  WSContentSend_P (PSTR ("</div>\n"));

  WSContentSend_P (PSTR ("</div>\n"));

  // end of page
  WSContentStop ();
}

#endif  // USE_WEBSERVER

/*******************************************************\
 *                      Interface
\*******************************************************/

bool Xsns98 (uint8_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  {
    case FUNC_INIT:
      VmcInit ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kVmcCommands, VmcCommand);
      break;
    case FUNC_EVERY_SECOND:
      VmcEverySecond ();
      break;
    case FUNC_JSON_APPEND:
      VmcShowJSON (true);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      // pages
      Webserver->on ("/vmc",      VmcWebPageConfig);
      Webserver->on ("/control",  VmcWebPageControl);
      Webserver->on ("/base.svg", VmcWebGraphFrame);
      Webserver->on ("/data.svg", VmcWebGraphData);

      // update status
      Webserver->on ("/mode.upd", VmcWebUpdateMode);
      Webserver->on ("/data.upd", VmcWebUpdateData);

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
