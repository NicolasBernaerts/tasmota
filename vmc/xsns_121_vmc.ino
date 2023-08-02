/*
  xsns_121_vmc.ino - Ventilation Motor Controled support 
    for Sonoff TH, Sonoff Basic or SonOff Dual
  
  Copyright (C) 2019-2023 Nicolas Bernaerts
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
    03/02/2023 - v4.0 - Tasmota 12.3 compatibility
                        Control page redesign with auto update
   
  Settings are stored using unused parameters :
    - Settings->free_f63[0] : VMC mode
    - Settings->free_f63[1] : Target humidity level (%)
    - Settings->free_f63[2] : Humidity thresold (%)


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

#define XSNS_121                 121

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
#define D_JSON_VMC_HUMIDITY     "Humi"
#define D_JSON_VMC_TEMPERATURE  "Temp"
#define D_JSON_VMC_THRESHOLD    "Thres"
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
#define D_VMC_TARGET            "VMC Target"
#define D_VMC_THRESHOLD         "Humidity Threshold"
#define D_VMC_CONFIGURE         "Configure VMC"
#define D_VMC_TIME              "Time"

// graph colors
#define VMC_COLOR_HUMI          "#6bc4ff"
#define VMC_COLOR_TEMP          "#f39c12"
#define VMC_COLOR_FAST          "#a00"
#define VMC_COLOR_FAST2         "#f00"
#define VMC_COLOR_TIME          "#aaa"
#define VMC_COLOR_LINE          "#888"

// graph data
#define VMC_GRAPH_WIDTH         1200      
#define VMC_GRAPH_HEIGHT        600 
#define VMC_GRAPH_BARGRAPH      50 
#define VMC_GRAPH_SAMPLE        300
#define VMC_GRAPH_REFRESH       288             // collect data every 288 sec to get 24h graph with 300 samples
#define VMC_GRAPH_PERCENT_START 6      
#define VMC_GRAPH_PERCENT_STOP  94
#define VMC_GRAPH_TEMP_MIN      18      
#define VMC_GRAPH_TEMP_MAX      22       

// VMC data
#define VMC_DATA_TIMEOUT        1800            // 30 mn  
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
const char kVmcModeIcon[]  PROGMEM = "❌|💤|🌀|♻️";
const char kVmcModeLabel[] PROGMEM = "❌ Disabled|💤 Low speed|🌀 High speed|♻️ Automatic";
const char kVmcModeName[]  PROGMEM = "---|Slow|Fast|Auto";
const char kVmcModeId[]    PROGMEM = "-|slow|fast|auto";

// vmc commands
const char kVmcCommands[] PROGMEM = "vmc_" "|" D_CMND_VMC_HELP "|" D_CMND_VMC_MODE "|" D_CMND_VMC_TARGET "|" D_CMND_VMC_THRESHOLD;
void (* const VmcCommand[])(void) PROGMEM = { &CmndVmcHelp, &CmndVmcMode, &CmndVmcTarget, &CmndVmcThreshold };

// graph units
const uint8_t arr_graph_percent[] = {100, 75,   50,  25,   0};
const float   arr_graph_scale[]   = {1,   0.75, 0.5, 0.25, 0};
const uint8_t arr_graph_text_y[]  = {4,   27,   52,  77,   99};

// HTML chains
const char VMC_TEXT_TIME[]    PROGMEM = "<text class='time' x='%d' y='%d%%'>%sh</text>\n";
const char VMC_INPUT_BUTTON[] PROGMEM = "<button name='%s' class='button %s'>%s</button>\n";
const char VMC_INPUT_NUMBER[] PROGMEM = "<p>%s<br/><input type='number' name='%s' min='0' max='%d' step='1' value='%d'></p>\n";
const char VMC_INPUT_OPTION[] PROGMEM = "<option value='%d' %s>%s</option>";

#ifdef USE_UFILESYS

#define D_VMC_CFG             "vmc.cfg"      // configuration file

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
void VmcSetRelayState (const uint8_t new_state)
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
  // retrieve settings from flash memory
  vmc_config.mode      = Settings->free_f63[0];
  vmc_config.target    = Settings->free_f63[1];
  vmc_config.threshold = Settings->free_f63[2];

  // check out of range values
  if (vmc_config.mode >= VMC_MODE_MAX) vmc_config.mode = VMC_MODE_AUTO;
  if ((vmc_config.target == 0) || (vmc_config.target >= 100)) vmc_config.target = VMC_TARGET_DEFAULT;
  if ((vmc_config.threshold == 0) || (vmc_config.threshold >= 25)) vmc_config.threshold = VMC_THRESHOLD_DEFAULT;
}

// Save configuration
void VmcSaveConfig ()
{
  // save settings into flash memory
  Settings->free_f63[0] = vmc_config.mode;
  Settings->free_f63[1] = vmc_config.target;
  Settings->free_f63[2] = vmc_config.threshold;
}

// Show JSON status (for MQTT)
void VmcShowJSON (bool append)
{
  uint8_t status;
  float   value;
  char    str_text[32];

  // get mode and humidity
  GetTextIndexed (str_text, sizeof (str_text), vmc_config.mode, kVmcModeLabel);
  
  // add , in append mode or { in direct publish mode
  if (append) ResponseAppend_P (PSTR (",")); else Response_P (PSTR ("{"));

  // vmc mode  -->  "VMC":{"Mode":4,"Label":"Automatic","Temperature":18.4,"Humidity":70.5,"Target":50}
  ResponseAppend_P (PSTR ("\"%s\":{"), D_JSON_VMC);
  ResponseAppend_P (PSTR ("\"%s\":%d"), D_JSON_VMC_MODE, vmc_config.mode);
  ResponseAppend_P (PSTR (",\"%s\":\"%s\""), D_JSON_VMC_LABEL, str_text);

  // temperature
  value = SensorTemperatureGet ();
  ext_snprintf_P (str_text, sizeof(str_text), PSTR ("%1_f"), &value);
  ResponseAppend_P (PSTR (",\"%s\":%s"), D_JSON_VMC_TEMPERATURE, str_text);

  // humidity level
  value = SensorHumidityGet ();
  ext_snprintf_P (str_text, sizeof(str_text), PSTR ("%1_f"), &value);
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
    status = VmcGetRelayState ();
    GetTextIndexed (str_text, sizeof (str_text), status, kVmcModeLabel);

    // relay state  -->  ,"State":{"Mode":1,"Label":"On"}
    ResponseAppend_P (PSTR (",\"%s\":{"), D_JSON_VMC_STATE);
    ResponseAppend_P (PSTR ("\"%s\":%d"), D_JSON_VMC_MODE, status);
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
    GetTextIndexed (str_text, sizeof (str_text), index, kVmcModeLabel);
    AddLog (LOG_LEVEL_INFO, PSTR ("HLP:   %u - %s"), index, str_text);
  }
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: vmc_target = target humidity level (%u %%)"), vmc_config.target);
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: vmc_thres  = humidity threshold (%u %%)"), vmc_config.threshold);

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
      vmc_config.target = (uint8_t)XdrvMailbox.payload;
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

// update sensors and adjust VMC mode
void VmcEverySecond ()
{
  uint8_t humidity, actual_state, target_state;
  float   value;

  // update relay state
  actual_state = VmcGetRelayState ();

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
      // update current humidity
      humidity = (uint8_t)SensorHumidityGet ();

      // if humidity is low enough, target VMC state is low speed
      // else, if humidity is too high, target VMC state is high speed
      if (humidity < vmc_config.target - vmc_config.threshold) target_state = VMC_STATE_LOW;
      else if (humidity > vmc_config.target + vmc_config.threshold) target_state = VMC_STATE_HIGH;
      break;
  }

  // if VMC state is different than target state, set relay
  if (actual_state != target_state) VmcSetRelayState (target_state);
  
  // update history state
  if (target_state == VMC_STATE_HIGH) SensorActivitySet ();

  // if JSON update needed, publish
  if (actual_state != target_state) VmcShowJSON (false);
}

// initialisation
void VmcInit ()
{
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
  WSContentSend_P (PSTR ("<p><form action='control' method='get'><button>VMC Control</button></form></p>\n"));

  // status mode options
  WSContentSend_P (PSTR ("<p><form action='/mode.upd' method='get'>\n"));
  WSContentSend_P (PSTR ("<select style='width:100%%;text-align:center;padding:8px;border-radius:0.3rem;' name='%s' onchange='this.form.submit();'>\n"), D_CMND_VMC_MODE);
  WSContentSend_P (PSTR ("<option value='%u'>%s</option>\n"), UINT8_MAX, PSTR ("-- Select mode --"));
  for (index = VMC_MODE_DISABLED; index < VMC_MODE_MAX; index ++)
  {
    // display mode
    GetTextIndexed (str_text, sizeof (str_text), index, kVmcModeLabel);
    WSContentSend_P (PSTR ("<option value='%u'>%s</option>\n"), index, str_text);
  }
  WSContentSend_P (PSTR ("</select>\n"));
  WSContentSend_P (PSTR ("</form></p>\n"));
}

// append VMC state to main page
void VmcWebSensor ()
{
  char str_text[32];

  // display vmc icon status
  GetTextIndexed (str_text, sizeof (str_text), vmc_config.mode, kVmcModeLabel);
  WSContentSend_PD (PSTR ("{s}%s{m}%s{e}"), D_VMC_MODE, str_text);

  // if automatic mode, add target humidity
  if (vmc_config.mode == VMC_MODE_AUTO) WSContentSend_PD (PSTR ("{s}%s{m}%u %%{e}"), D_VMC_TARGET, vmc_config.target);
}

// update status for web client
// format is humi;temp;slow;fast;flag
//   humi : humidity value
//   temp : temperature value
//   slow : emoji of slow button
//   fast : emoji of fast button
//   flag : page reload flag
void VmcWebUpdateData ()
{
  uint8_t relay;
  float   value;
  char    str_humi[8];
  char    str_temp[16];
  char    str_slow[8];
  char    str_fast[8];

  // humidity value
  value = SensorHumidityGet ();
  if (isnan (value)) strcpy (str_humi, "---"); 
    else ext_snprintf_P (str_humi, sizeof(str_humi), PSTR ("%1_f%%"), &value);

  // temperature value
  value = SensorTemperatureGet ();
  if (isnan (value)) strcpy (str_temp, "---"); 
    else ext_snprintf_P (str_temp, sizeof(str_temp), PSTR ("%1_f°C"), &value);

  // get slow and fast emoji
  relay = VmcGetRelayState ();
  if (relay == VMC_STATE_HIGH) { strcpy (str_slow, "⚫"); strcpy (str_fast, "🔴"); }
    else { strcpy (str_slow, "⚪"); strcpy (str_fast, "⚫"); }

  // serve update page
  WSContentBegin (200, CT_PLAIN);
  WSContentSend_P (PSTR ("%s;%s;%u %%;%s;%s;%d;"), str_humi, str_temp, vmc_config.target, str_slow, str_fast, true);
  WSContentEnd ();

  // log
  AddLog (LOG_LEVEL_DEBUG, PSTR ("VMC: Upd %s;%s;%u %%;%s;%d;"), str_humi, str_temp, vmc_config.target, str_slow, str_fast);
}

// VMC control public web page
void VmcWebPageControl ()
{
  uint16_t index;
  float    temperature, humidity;
  char     str_argument[16];
  char     str_text[16];
  char     str_id[16];

  // check if vmc state has changed
  if (Webserver->hasArg (D_CMND_VMC_MODE))
  {
    WebGetArg (D_CMND_VMC_MODE, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) vmc_config.mode = (uint8_t)atoi (str_argument);
    VmcSaveConfig ();
  }

  // check if vmc state has changed
  if (Webserver->hasArg (D_CMND_VMC_TARGET))
  {
    WebGetArg (D_CMND_VMC_TARGET, str_argument, sizeof (str_argument));
    if (strlen(str_argument) > 0) vmc_config.target = (uint8_t)max (5, min (100, atoi (str_argument)));
    VmcSaveConfig ();
  }

  // beginning page without authentification
  WSContentStart_P (D_VMC_CONTROL, false);

  // graph swipe section
  SensorWebGraphSwipe ();
  WSContentSend_P (PSTR ("</script>\n"));

  // page data refresh script
  WSContentSend_P (PSTR ("<script type='text/javascript'>\n"));

  WSContentSend_P (PSTR ("function updateData()\n"));
  WSContentSend_P (PSTR ("{\n"));
  WSContentSend_P (PSTR (" httpData=new XMLHttpRequest();\n"));
  WSContentSend_P (PSTR (" httpData.onreadystatechange=function()\n"));
  WSContentSend_P (PSTR (" {\n"));
  WSContentSend_P (PSTR ("  if (httpData.readyState==4)\n"));
  WSContentSend_P (PSTR ("  {\n"));
  WSContentSend_P (PSTR ("   setTimeout(function() {updateData();},%u);\n"), 1000);                  // ask for next update 
  WSContentSend_P (PSTR ("   arr_param=httpData.responseText.split(';');\n"));
  WSContentSend_P (PSTR ("   document.getElementById('humi').innerHTML=arr_param[0];\n"));           // humidity value
  WSContentSend_P (PSTR ("   document.getElementById('temp').innerHTML=arr_param[1];\n"));           // temperature value
  WSContentSend_P (PSTR ("   document.getElementById('goal').innerHTML=arr_param[2];\n"));           // temperature value
  WSContentSend_P (PSTR ("   document.getElementById('slow').innerHTML=arr_param[3];\n"));           // slow button mark
  WSContentSend_P (PSTR ("   document.getElementById('fast').innerHTML=arr_param[4];\n"));           // fast button mark
  WSContentSend_P (PSTR ("  }\n"));
  WSContentSend_P (PSTR (" }\n"));
  WSContentSend_P (PSTR (" httpData.open('GET','/data.upd',true);\n"));
  WSContentSend_P (PSTR (" httpData.send();\n"));
  WSContentSend_P (PSTR ("}\n"));

  // refresh data every 5 sec
  WSContentSend_P (PSTR ("setTimeout(function() {updateData();},250);\n"));                         // ask for first update

  WSContentSend_P (PSTR ("</script>\n"));

  // set page as scalable
  WSContentSend_P (PSTR ("<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=yes'/>\n"));

  // refresh full page every 5 mn
  WSContentSend_P (PSTR ("<meta http-equiv='refresh' content='%u'/>\n"), 300);

  // page style
  WSContentSend_P (PSTR ("<style>\n"));

  // graph section style
  SensorWebGraphWeeklyMeasureStyle ();

  WSContentSend_P (PSTR ("div.choice {font-size:2.5vh;color:#bbb;margin:2vh 0px;border-radius:6px;}\n"));
  WSContentSend_P (PSTR ("div.choice a {color:white;}\n"));
  WSContentSend_P (PSTR ("div.choice div {display:inline-block;border:#444 1px solid;border-radius:4px;}\n"));
  WSContentSend_P (PSTR ("div.choice div.mode {width:90px;}\n"));
  WSContentSend_P (PSTR ("div.choice div.humi {width:60px;}\n"));
  WSContentSend_P (PSTR ("div.choice div.act {background:#666;}\n"));
  WSContentSend_P (PSTR ("div.choice div:hover {background:#aaa;}\n"));
  WSContentSend_P (PSTR ("div.choice span {margin:0px 15px;}\n"));

  WSContentSend_P (PSTR ("div.mode {margin:0px 10px;}\n"));
  WSContentSend_P (PSTR ("div.mode span {font-size:2vh;padding-right:10px;margin:0px;}\n"));

  WSContentSend_P (PSTR ("</style>\n"));

  WSContentSend_P (PSTR ("</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  // room name
  WSContentSend_P (PSTR ("<div class='title'><a href='/'>%s</a></div>\n"), SettingsText(SET_DEVICENAME));

  // values
  WSContentSend_P (PSTR ("<div class='value'>"));

  // temperature
  temperature = SensorTemperatureGet ();
  if (!isnan (temperature))
  {
    ext_snprintf_P (str_text, sizeof (str_text), PSTR ("%1_f"), &temperature);
    WSContentSend_P (PSTR ("<span id='temp'>%s°C</span>"), str_text);
  }

  // humidity
  humidity = SensorHumidityGet ();
  if (!isnan (humidity)) ext_snprintf_P (str_text, sizeof (str_text), PSTR ("%1_f%%"), &humidity);
    else strcpy (str_text, "---");
  WSContentSend_P (PSTR ("<span id='humi'>%s</span>"), str_text);

  WSContentSend_P (PSTR ("</div>\n"));

  // vmc mode selector
  WSContentSend_P (PSTR ("<div><div class='choice'>\n"));
  for (index = VMC_MODE_LOW; index < VMC_MODE_MAX; index++)
  {
    GetTextIndexed (str_id, sizeof (str_id), index, kVmcModeId);
    GetTextIndexed (str_text, sizeof (str_text), index, kVmcModeName);
    if (vmc_config.mode == index) strcpy (str_argument, " act"); else strcpy (str_argument, "");
    if (index != VMC_MODE_AUTO) WSContentSend_P (PSTR ("<a href='control?%s=%d'><div class='mode%s'><span id='%s'>⚫</span><span>%s</span></div></a>\n"), D_CMND_VMC_MODE, index, str_argument, str_id, str_text);
      else WSContentSend_P (PSTR ("<a href='control?%s=%d'><div class='mode%s'><span>♻️</span><span>%s</span></div></a>\n"), D_CMND_VMC_MODE, index, str_argument, str_text);
  }
  WSContentSend_P (PSTR ("</div></div>\n"));

  // humidity target selection
  WSContentSend_P (PSTR ("<div class='choice'>\n"));
  WSContentSend_P (PSTR ("<a href='/control?target=%u'><div class='humi'>&lt;&lt;</div></a>\n"), (vmc_config.target - 1) / 5 * 5);
  WSContentSend_P (PSTR ("<a href='/control?target=%u'><div class='humi'>&lt;</div></a>\n"), vmc_config.target - 1);
  WSContentSend_P (PSTR ("<span id='goal'>%u %%</span>\n"), vmc_config.target);
  WSContentSend_P (PSTR ("<a href='/control?target=%u'><div class='humi'>&gt;</div></a>\n"), vmc_config.target + 1);
  WSContentSend_P (PSTR ("<a href='/control?target=%u'><div class='humi'>&gt;&gt;</div></a>\n"), vmc_config.target / 5 * 5 + 5);
  WSContentSend_P (PSTR ("</div>\n"));

  // Graph
  SensorWebGraphWeeklyMeasureCurve (0, D_VMC_CONTROL);

  // end of page
  WSContentStop ();
}

#endif  // USE_WEBSERVER

/*******************************************************\
 *                      Interface
\*******************************************************/

bool Xsns121 (uint32_t function)
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
    case FUNC_SAVE_BEFORE_RESTART:
      VmcSaveConfig ();
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
      Webserver->on ("/control",  VmcWebPageControl);
      Webserver->on ("/mode.upd", VmcWebUpdateMode);
      Webserver->on ("/data.upd", VmcWebUpdateData);
      break;
    case FUNC_WEB_SENSOR:
      VmcWebSensor ();
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      VmcWebMainButton ();
      break;

#endif  // USE_WEBSERVER

  }
  return result;
}

#endif // USE_VMC
