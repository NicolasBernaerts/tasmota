/*
  xdrv_98_motion.ino - Motion detector management with tempo and timers (~6.3 kb)
  
  Copyright (C) 2020  Nicolas Bernaerts
    27/03/2020 - v1.0 - Creation
    10/04/2020 - v1.1 - Add detector configuration for low/high level
                   
  Input devices should be configured as followed :
   - Switch2 = Motion detector

  Settings are stored using weighting scale parameters :
   - Settings.energy_power_calibration   = Motion detection tempo (s)
   - Settings.energy_voltage_calibration = Moton detection status (0 = low, 1 = high)

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
 *                Motion detector
\*************************************************/

#ifdef USE_MOTION

#define XDRV_98                   98
#define XSNS_98                   98

#define MOTION_BUTTON             1       // switch2

#define MOTION_BUFFER_SIZE        128

#define D_PAGE_MOTION_CONFIG      "config"
#define D_PAGE_MOTION_TOGGLE      "toggle"
#define D_PAGE_MOTION_GRAPH       "graph"

#define D_CMND_MOTION_ENABLE      "enable"
#define D_CMND_MOTION_FORCE       "force"
#define D_CMND_MOTION_LEVEL       "level"
#define D_CMND_MOTION_TEMPO       "tempo"
#define D_CMND_MOTION_MN          "mn"
#define D_CMND_MOTION_SEC         "sec"

#define D_JSON_MOTION             "Motion"
#define D_JSON_MOTION_STATUS      "Status"
#define D_JSON_MOTION_ENABLED     "Enabled"
#define D_JSON_MOTION_LEVEL       "Level"
#define D_JSON_MOTION_HIGH        "High"
#define D_JSON_MOTION_LOW         "Low"
#define D_JSON_MOTION_ON          "ON"
#define D_JSON_MOTION_OFF         "OFF"
#define D_JSON_MOTION_FORCED      "Forced"
#define D_JSON_MOTION_TIMELEFT    "Timeleft"
#define D_JSON_MOTION_DETECTED    "Detected"
#define D_JSON_MOTION_ACTIVE      "Active"
#define D_JSON_MOTION_TEMPO       "Tempo"

#define MOTION_GRAPH_STEP            2           // collect motion status every 2mn
#define MOTION_GRAPH_SAMPLE          720         // 24 hours display with collect every 2 mn
#define MOTION_GRAPH_WIDTH           800      
#define MOTION_GRAPH_HEIGHT          400 
#define MOTION_GRAPH_PERCENT_START   10      
#define MOTION_GRAPH_PERCENT_STOP    90      

// xdrv_98_motion.ino
#define D_MOTION              "Motion Detector"
#define D_MOTION_CONFIG       "Configure"
#define D_MOTION_DETECTION    "Detection"
#define D_MOTION_GRAPH        "Graph"
#define D_MOTION_TEMPO        "Temporisation"
#define D_MOTION_MOTION       "Motion"
#define D_MOTION_DETECTOR     "Detector"
#define D_MOTION_COMMAND      "Light"
#define D_MOTION_ENABLE       "Enable"
#define D_MOTION_ENABLED      "Enabled"
#define D_MOTION_DISABLE      "Disable"
#define D_MOTION_DISABLED     "Disabled"
#define D_MOTION_ON           "On"
#define D_MOTION_OFF          "Off"
#define D_MOTION_LEVEL        "Level"
#define D_MOTION_HIGH         "High"
#define D_MOTION_LOW          "Low"
#define D_MOTION_FORCED       "Forced"

// offloading commands
enum MotionCommands { CMND_MOTION_TEMPO, CMND_MOTION_LEVEL, CMND_MOTION_ENABLE, CMND_MOTION_FORCE };
const char kMotionCommands[] PROGMEM = D_CMND_MOTION_TEMPO "|" D_CMND_MOTION_LEVEL "|" D_CMND_MOTION_ENABLE "|" D_CMND_MOTION_FORCE;

// form topic style
const char MOTION_TOPIC_STYLE[] PROGMEM = "style='float:right;font-size:0.7rem;'";

// graph data structure
struct graph_value {
    uint8_t is_enabled : 1;
    uint8_t is_detected : 1;
    uint8_t is_active : 1;
}; 

/*************************************************\
 *               Variables
\*************************************************/

// detector states
enum MotionStates { MOTION_OFF, MOTION_ON };

// variables
bool motion_updated      = false;             // some important data have been updated
bool motion_enabled      = true;              // is motion detection enabled
bool motion_detected     = false;             // is motion currently detected
bool motion_active       = false;             // is relay currently active
bool motion_relay_forced = false;             // is relay forced
unsigned long motion_tempo_start = 0;         // when tempo started

// graph data
int  motion_graph_index;
int  motion_graph_counter;
int  motion_graph_refresh;
bool motion_graph_enabled;
bool motion_graph_detected;
bool motion_graph_active;
graph_value arr_graph_data[MOTION_GRAPH_SAMPLE];

/**************************************************\
 *                  Accessors
\**************************************************/

// get motion detection level (0 or 1)
uint8_t MotionGetDetectionLevel ()
{
  return (uint8_t)Settings.energy_voltage_calibration;
}

// set motion detection level (0 or 1)
void MotionSetDetectionLevel (uint8_t level)
{
  Settings.energy_voltage_calibration= (unsigned long)level;
  motion_updated = true;
}

// check if motion is detected
bool MotionDetected ()
{
  uint8_t actual_status, detection_status;
  
  // check if motion is detected according to detection level
  actual_status    = SwitchGetVirtual (MOTION_BUTTON);
  detection_status = MotionGetDetectionLevel ();

  return (actual_status == detection_status);
}

// get relay state
bool MotionRelayActive ()
{
  uint8_t relay_condition;
  
  // read relay state
  relay_condition = bitRead (power, 0);

  return (relay_condition == 1);
}

// set relay state
void MotionSetRelay (bool new_state)
{
  // set relay state
  if (new_state == false) ExecuteCommandPower (1, POWER_OFF, SRC_IGNORE);
  else ExecuteCommandPower (1, POWER_ON, SRC_IGNORE);
  motion_updated = true;
}

// get detection tempo (in s)
unsigned long MotionGetTempo ()
{
  return Settings.energy_power_calibration;
}
 
// set detection tempo (in s)
void MotionSetTempo (unsigned long new_tempo)
{
  Settings.energy_power_calibration = new_tempo;
  motion_updated = true;
}

// enable or disable motion detector (POWER_OFF = disable, POWER_ON = enable)
void MotionEnable (uint32_t state)
{
  if (state == POWER_ON) motion_enabled = true;
  else if (state == POWER_OFF) motion_enabled = false;
  motion_updated = true;
}

// enable or disable motion detector
void MotionEnable (const char* str_string)
{
  String str_command = str_string;

  // check if state in ON
  if (str_command.equalsIgnoreCase (D_JSON_MOTION_ON)) motion_enabled = true;
  else if (str_command.equalsIgnoreCase (D_JSON_MOTION_OFF)) motion_enabled = false;
  motion_updated = true;
}

// force relay switch ON or OFF
void MotionRelayForce (const char* str_string)
{
  String str_command = str_string;

  // check if state is forced ON
  if (str_command.equals ("1") || str_command.equalsIgnoreCase (D_JSON_MOTION_ON))
  {
    motion_relay_forced = true;
    motion_tempo_start  = 0;
    motion_updated = true;
  }

  // else if state is forced OFF
  else if (str_command.equals ("0") || str_command.equalsIgnoreCase (D_JSON_MOTION_OFF))
  {
    motion_relay_forced = false;
    motion_tempo_start  = 0;
    motion_updated = true;
  } 
}

// get motion status in readable format
void MotionGetStatus (String& str_status)
{
  bool relay_on;

  // if relay is OFF
  relay_on = MotionRelayActive ();
  if (relay_on == false) str_status = D_JSON_MOTION_OFF;

  // if tempo is forced ON
  else if (motion_relay_forced == true) str_status = D_JSON_MOTION_FORCED;

  // if tempo is forced ON
  else if (motion_detected == true) str_status = D_JSON_MOTION;

  // else, if temporisation is runnning, update remaining time
  else str_status = D_JSON_MOTION_TEMPO;
}

// get timeleft in readable format
void MotionGetTimeleft (String& str_timeleft)
{
  TIME_T   tempo_dst;
  uint8_t  tempo_mn;
  unsigned long time_tempo, time_now, time_left;
  char     str_number[8];

  // if tempo is forced ON
  if (motion_relay_forced == true) str_timeleft = D_MOTION_FORCED;

  // else, if temporisation is runnning, update remaining time
  else if (motion_tempo_start != 0)
  {
    // get current time and current temporisation (convert from s to ms)
    time_now   = millis ();
    time_tempo = 1000 * MotionGetTempo ();
 
    // if temporisation is not over
    if (time_now - motion_tempo_start < time_tempo)
    {
      // convert to readable format
      time_left = (motion_tempo_start + time_tempo - time_now) / 1000;
      BreakTime ((uint32_t) time_left, tempo_dst);
      sprintf (str_number, "%02d", tempo_dst.minute);
      str_timeleft = str_number + String (":");
      sprintf (str_number, "%02d", tempo_dst.second);
      str_timeleft += str_number;
    }
  }

  // else, tempo is OFF
  else str_timeleft = "---";
}

// Save data for graph use
void MotionSetGraphData (int index, bool set_enabled, bool set_detected, bool set_active)
{
  // force index in graph window
  index = index % MOTION_GRAPH_SAMPLE;

  // generate stored value
  if (set_enabled == true) arr_graph_data[index].is_enabled = 1;
  else arr_graph_data[index].is_enabled = 0;
  if (set_detected == true) arr_graph_data[index].is_detected = 1;
  else arr_graph_data[index].is_detected = 0;
  if (set_active == true) arr_graph_data[index].is_active = 1;
  else arr_graph_data[index].is_active = 0;
}

// Retrieve enabled state from graph data
bool MotionGetGraphEnabled (int index)
{
  // force index in graph window and return result
  index = index % MOTION_GRAPH_SAMPLE;
  return (arr_graph_data[index].is_enabled == 1);
}

// Retrieve detected state from graph data
bool MotionGetGraphDetected (int index)
{
  // force index in graph window and return result
  index = index % MOTION_GRAPH_SAMPLE;
  return (arr_graph_data[index].is_detected == 1);
}

// Retrieve active state from graph data
bool MotionGetGraphActive (int index)
{
  // force index in graph window and return result
  index = index % MOTION_GRAPH_SAMPLE;
  return (arr_graph_data[index].is_active == 1);
}

/**************************************************\
 *                  Functions
\**************************************************/

// Show JSON status (for MQTT)
void MotionShowJSON (bool append)
{
  TIME_T  tempo_dst;
  uint8_t  motion_level;
  unsigned long tempo_active, time_tempo, time_now, time_left;
  String   str_json, str_text;

  // start message  -->  {  or message,
  if (append == false) str_json = "{";
  else str_json = ",";

  // Motion detection section  -->  "Motion":{"Level":"High","Enabled":"ON","Detected":"ON","Active":"ON","Tempo":120,"Status":"Timer","Timeleft":"2:15"}
  str_json += "\"" + String (D_JSON_MOTION) + "\":{";

  motion_level = MotionGetDetectionLevel ();
  str_json += "\"" + String (D_JSON_MOTION_LEVEL) + "\":\"";
  if (motion_level == 0) str_json += D_JSON_MOTION_LOW;
  else str_json += D_JSON_MOTION_HIGH;
  str_json += "\",";

  str_json += "\"" + String (D_JSON_MOTION_ENABLED) + "\":\"";
  if (motion_enabled == true) str_json += D_JSON_MOTION_ON;
  else str_json += D_JSON_MOTION_OFF;
  str_json += "\",";

  str_json += "\"" + String (D_JSON_MOTION_DETECTED) + "\":\"";
  if (motion_detected == true) str_json += D_JSON_MOTION_ON;
  else str_json += D_JSON_MOTION_OFF;
  str_json += "\",";

  str_json += "\"" + String (D_JSON_MOTION_ACTIVE) + "\":\"";
  if (motion_active == true) str_json += D_JSON_MOTION_ON;
  else str_json += D_JSON_MOTION_OFF;
  str_json += "\",";

  tempo_active = MotionGetTempo ();
  str_json += "\"" + String (D_JSON_MOTION_TEMPO) + "\":" + String (tempo_active) + ",";

  MotionGetStatus (str_text);
  str_json += "\"" + String (D_JSON_MOTION_STATUS) + "\":\"" + str_text + "\",";

  MotionGetTimeleft (str_text);
  str_json += "\"" + String (D_JSON_MOTION_TIMELEFT) + "\":\"" + str_text + "\"}";

  // if append mode, add json string to MQTT message
  if (append == true) ResponseAppend_P (str_json.c_str ());

  // else, add last bracket and directly publish message
  else 
  {
    str_json += "}";
    Response_P (str_json.c_str ());
    MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));
  } 
}

// Handle detector MQTT commands
bool MotionMqttCommand ()
{
  bool command_handled = true;
  int  command_code;
  char command [CMDSZ];

  // check MQTT command
  command_code = GetCommandCode (command, sizeof(command), XdrvMailbox.topic, kMotionCommands);

  // handle command
  switch (command_code)
  {
    case CMND_MOTION_TEMPO:  // set detector tempo
      MotionSetTempo (XdrvMailbox.payload);
      break;
    case CMND_MOTION_LEVEL:  // set detector level (high or low)
      MotionSetDetectionLevel (XdrvMailbox.payload);
      break;
    case CMND_MOTION_ENABLE:  // enable or disable detector
      MotionEnable (XdrvMailbox.data);
      break;
    case CMND_MOTION_FORCE:  // force detector state 
      MotionRelayForce (XdrvMailbox.data);
      break;
    default:
      command_handled = false;
  }

  // if needed, send updated status
  if (command_handled == true) MotionShowJSON (false);
  
  return command_handled;
}

void MotionUpdateGraph ()
{
  // set current graph value
  MotionSetGraphData (motion_graph_index, motion_graph_enabled, motion_graph_detected, motion_graph_active);

  // init current values
  motion_graph_enabled  = false;
  motion_graph_detected = false;
  motion_graph_active   = false;

  // increase temperature data index and reset if max reached
  motion_graph_index ++;
  motion_graph_index = motion_graph_index % MOTION_GRAPH_SAMPLE;
}

// update motion and relay state according to status
void MotionEvery250ms ()
{
  bool mustbe_active;
  unsigned long time_tempo, time_now, time_end;

  // update current status of motion detection and relay
  motion_active   = MotionRelayActive ();
  motion_detected = MotionDetected ();

  // if relay is forced ON
  if (motion_relay_forced == true)
  {
    motion_tempo_start = 0;
    mustbe_active = true;
  }

  // else, if motion is enabled and detected
  else if ((motion_enabled == true) && (motion_detected == true))
  {
    motion_tempo_start = millis ();
    mustbe_active = true;
  }

  // else, if temporisation is running
  else if (motion_tempo_start != 0)
  {
    // relay should be ON
    mustbe_active = true;

    // get current time and tempo (convert mn in ms)
    time_now   = millis ();
    time_tempo = 1000 * MotionGetTempo (); 

    // if temporisation is over
    if (time_now - motion_tempo_start >= time_tempo)
    {
      motion_tempo_start = 0;
      mustbe_active = false;
    }
  }

  // else, relay should be off
  else mustbe_active = false;

  // if relay needs to change
  if (mustbe_active != motion_active)
  {
    MotionSetRelay (mustbe_active);
    motion_updated = true;
  }

  // if some important data have been updated
  if (motion_updated == true)
  {
    // publish JSON
    MotionShowJSON (false);
    motion_updated = false;
  }
}

// update graph data
void MotionEverySecond ()
{
  // check if motion is enabled
  if (motion_graph_enabled == false) motion_graph_enabled = motion_enabled;

  // check if motion is detected
  if (motion_graph_detected == false) motion_graph_detected = motion_detected;

  // check if relay is active
  if (motion_graph_active == false) motion_graph_active = motion_active;

  // increment delay counter and if delay reached, update history data
  if (motion_graph_counter == 0) MotionUpdateGraph ();
  motion_graph_counter ++;
  motion_graph_counter = motion_graph_counter % motion_graph_refresh;
}

// pre init main status
void MotionPreInit ()
{
  int index;

  // set switch mode
  Settings.switchmode[MOTION_BUTTON] = FOLLOW;

  // disable serial log
  Settings.seriallog_level = 0;
  
  // initialise graph data
  motion_graph_index   = 0;
  motion_graph_counter = 0;
  motion_graph_refresh = 60 * MOTION_GRAPH_STEP;
  motion_graph_enabled  = false;  
  motion_graph_detected = false;  
  motion_graph_active   = false;  
  for (index = 0; index < MOTION_GRAPH_SAMPLE; index++) MotionSetGraphData (index, false, false, false);
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// detector main page switch button
void MotionWebMainButton ()
{
  String str_state;

  if (motion_enabled == false) str_state = D_MOTION_ENABLE;
  else str_state = D_MOTION_DISABLE;
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s %s</button></form></p>"), D_PAGE_MOTION_TOGGLE, str_state.c_str (), D_MOTION_MOTION);

  // Motion control page button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_PAGE_MOTION_GRAPH, D_MOTION_GRAPH);
}


// detector configuration page button
void MotionWebConfigButton ()
{
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s %s</button></form></p>"), D_PAGE_MOTION_CONFIG, D_MOTION_CONFIG, D_MOTION_MOTION);
}

// append detector state to main page
bool MotionWebSensor ()
{
  String str_motion, str_time;

  // get tempo timeleft
  MotionGetTimeleft (str_time);

  // determine motion detector state
  if (motion_enabled == false) str_motion = D_MOTION_DISABLED;
  else if ( motion_detected == true) str_motion = D_MOTION_ON;
  else str_motion = D_MOTION_OFF;

  // display result
  WSContentSend_PD (PSTR("{s}%s{m}%s{e}"), D_MOTION_MOTION, str_motion.c_str ());
  WSContentSend_PD (PSTR("{s}%s{m}%s{e}"), D_MOTION_TEMPO, str_time.c_str ());
}

// Movement detector mode toggle 
void MotionWebPageToggle ()
{
  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // invert mode
  motion_enabled = !motion_enabled;

  // refresh immediatly on main page
  WSContentStart_P (D_MOTION_DETECTION, false);
  WSContentSend_P (PSTR ("</script>\n"));
  WSContentSend_P (PSTR ("<meta http-equiv='refresh' content='0;URL=/' />\n"));
  WSContentSend_P (PSTR ("</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body bgcolor='#303030' >\n"));
  WSContentStop ();
}

// Motion config web page
void MotionWebPageConfigure ()
{
  uint8_t  tempo_mn  = 0;
  uint8_t  tempo_sec = 0;
  uint8_t  level;
  unsigned long tempo;
  char     argument[MOTION_BUFFER_SIZE];
  String   str_checked;
  
  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // page comes from save button on configuration page
  if (WebServer->hasArg("save"))
  {
    // get detection level,according to 'level' parameter
    WebGetArg (D_CMND_MOTION_LEVEL, argument, MOTION_BUFFER_SIZE);
    if (strlen(argument) > 0) MotionSetDetectionLevel (atoi (argument));
    
    // get number of minutes according to 'mn' parameter
    WebGetArg (D_CMND_MOTION_MN, argument, MOTION_BUFFER_SIZE);
    if (strlen(argument) > 0) tempo_mn = atoi (argument);
    
    // get number of seconds according to 'sec' parameter
    WebGetArg (D_CMND_MOTION_SEC, argument, MOTION_BUFFER_SIZE);
    if (strlen(argument) > 0) tempo_sec = atoi (argument);

    // save total tempo
    tempo = 60 * tempo_mn + tempo_sec;
    MotionSetTempo (tempo);
  }

  // beginning of form
  WSContentStart_P (D_MOTION_CONFIG);
  WSContentSendStyle ();
  WSContentSend_P (PSTR("<form method='get' action='%s'>\n"), D_PAGE_MOTION_CONFIG);

  // motion detector section  
  // -----------------------
  level     = MotionGetDetectionLevel ();
  tempo     = MotionGetTempo ();
  tempo_mn  = tempo / 60;
  tempo_sec = tempo % 60;

  // level (high or low)
  WSContentSend_P (PSTR("<p><fieldset><legend><b>&nbsp;%s %s&nbsp;</b></legend>"), D_MOTION_DETECTION, D_MOTION_LEVEL);
  if (level == 0) str_checked = "checked";
  else str_checked = "";
  WSContentSend_P (PSTR ("<p><input type='radio' id='low' name='level' value=0 %s><label for='low'>Low<span %s>%s</span></label></p>\n"), str_checked.c_str (), MOTION_TOPIC_STYLE, D_CMND_MOTION_LEVEL);
  if (level == 1) str_checked = "checked";
  else str_checked = "";
  WSContentSend_P (PSTR ("<p><input type='radio' id='high' name='level' value=1 %s><label for='high'>High</label></p>\n"), str_checked.c_str ());
  WSContentSend_P (PSTR("</fieldset></p>\n"));

  // temporisation
  WSContentSend_P (PSTR("<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>"), D_MOTION_TEMPO);
  WSContentSend_P (PSTR ("<p>minutes<span %s>%s</span><br><input type='number' name='%s' min='0' max='120' step='1' value='%d'></p>\n"), MOTION_TOPIC_STYLE, D_CMND_MOTION_TEMPO, D_CMND_MOTION_MN, tempo_mn);
  WSContentSend_P (PSTR ("<p>seconds<br><input type='number' name='%s' min='0' max='59' step='1' value='%d'></p>\n"), D_CMND_MOTION_SEC, tempo_sec);
  WSContentSend_P (PSTR("</fieldset></p>\n"));

  // save button  
  // --------------
  WSContentSend_P (PSTR ("<p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), D_SAVE);
  WSContentSend_P (PSTR("</form>\n"));

  // configuration button and end of page
  WSContentSpaceButton(BUTTON_CONFIGURATION);
  WSContentStop();
}

// Motion status graph display
void MotionWebDisplayGraph ()
{
  TIME_T   current_dst;
  uint32_t current_time;
  int      index, array_idx;
  int      graph_x, graph_y, graph_left, graph_right, graph_width, graph_low, graph_high, graph_hour;
  bool     state_curr;
  char     str_hour[4];

  // boundaries of SVG graph
  graph_left  = MOTION_GRAPH_PERCENT_START * MOTION_GRAPH_WIDTH / 100;
  graph_right = MOTION_GRAPH_PERCENT_STOP * MOTION_GRAPH_WIDTH / 100;
  graph_width = graph_right - graph_left;

  // start of SVG graph
  WSContentSend_P (PSTR ("<svg viewBox='0 0 %d %d'>\n"), MOTION_GRAPH_WIDTH, MOTION_GRAPH_HEIGHT);

  // graph curve zone
  WSContentSend_P (PSTR ("<rect x='%d%%' y='0%%' width='%d%%' height='100%%' rx='10' />\n"), MOTION_GRAPH_PERCENT_START, MOTION_GRAPH_PERCENT_STOP - MOTION_GRAPH_PERCENT_START);

  // graph label
  WSContentSend_P (PSTR ("<text class='active' x='%d%%' y='%d%%'>%s</text>\n"), 0, 27, D_MOTION_COMMAND);
  WSContentSend_P (PSTR ("<text class='enabled' x='%d%%' y='%d%%'>%s</text>\n"), 0, 59, D_MOTION_ENABLED);
  WSContentSend_P (PSTR ("<text class='detected' x='%d%%' y='%d%%'>%s</text>\n"), 0, 92, D_MOTION_DETECTOR);

  // ------------------
  //   Detector state
  // ------------------

  // loop for the sensor state graph
  graph_high = MOTION_GRAPH_HEIGHT * 83 / 100;
  graph_low  = MOTION_GRAPH_HEIGHT * 99 / 100;
  WSContentSend_P (PSTR ("<polyline class='detected' points='"));
  for (index = 0; index < MOTION_GRAPH_SAMPLE; index++)
  {
    // get current detection status
    array_idx  = (index + motion_graph_index) % MOTION_GRAPH_SAMPLE;
    state_curr = MotionGetGraphDetected (array_idx);

    // calculate current position
    graph_x = graph_left + (graph_width * index / MOTION_GRAPH_SAMPLE);
    if (state_curr == true) graph_y = graph_high;
    else graph_y = graph_low;

    // add the point to the line
    WSContentSend_P (PSTR("%d,%d "), graph_x, graph_y);
  }
  WSContentSend_P (PSTR("'/>\n"));

  // -----------------
  //   Enabled state
  // -----------------

  // loop for the sensor state graph
  graph_high = MOTION_GRAPH_HEIGHT * 50 / 100;
  graph_low  = MOTION_GRAPH_HEIGHT * 66 / 100;
  WSContentSend_P (PSTR ("<polyline class='enabled' points='"));
  for (index = 0; index < MOTION_GRAPH_SAMPLE; index++)
  {
    // get current detection status
    array_idx  = (index + motion_graph_index) % MOTION_GRAPH_SAMPLE;
    state_curr = MotionGetGraphEnabled (array_idx);

    // calculate current position
    graph_x = graph_left + (graph_width * index / MOTION_GRAPH_SAMPLE);
    if (state_curr == true) graph_y = graph_high;
    else graph_y = graph_low;

    // add the point to the line
    WSContentSend_P (PSTR("%d,%d "), graph_x, graph_y);
  }
  WSContentSend_P (PSTR("'/>\n"));

  // ------------------
  //    Light state
  // ------------------

  // loop for the relay state graph
  graph_high = MOTION_GRAPH_HEIGHT * 17 / 100;
  graph_low  = MOTION_GRAPH_HEIGHT * 33 / 100;
  WSContentSend_P (PSTR ("<polyline class='active' points='"));
  for (index = 0; index < MOTION_GRAPH_SAMPLE; index++)
  {
    // get current detection status
    array_idx  = (index + motion_graph_index) % MOTION_GRAPH_SAMPLE;
    state_curr = MotionGetGraphActive (array_idx);

    // calculate current position
    graph_x = graph_left + (graph_width * index / MOTION_GRAPH_SAMPLE);
    if (state_curr == true) graph_y = graph_high;
    else graph_y = graph_low;

    // add the point to the line
    WSContentSend_P (PSTR("%d,%d "), graph_x, graph_y);
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
  WSContentSend_P (PSTR ("<text class='time' x='%d' y='%d%%'>%sh</text>\n"), graph_x, 75, str_hour);

  // dislay next 5 time marks (every 4 hours)
  for (index = 0; index < 5; index++)
  {
    current_dst.hour = (current_dst.hour + 4) % 24;
    graph_x += graph_width / 6;
    sprintf(str_hour, "%02d", current_dst.hour);
    WSContentSend_P (PSTR ("<text class='time' x='%d' y='%d%%'>%sh</text>\n"), graph_x, 75, str_hour);
  }

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
}

// Motion graph web page
void MotionWebPageGraph ()
{
  // beginning of form without authentification with 60 seconds auto refresh
  WSContentStart_P (D_MOTION, false);
  WSContentSend_P (PSTR ("</script>\n"));
  WSContentSend_P (PSTR ("<meta http-equiv='refresh' content='%d;URL=/%s' />\n"), 60, D_PAGE_MOTION_GRAPH);
  
  // page style
  WSContentSend_P (PSTR ("<style>\n"));

  WSContentSend_P (PSTR ("body {color:white;background-color:#303030;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {width:100%%;margin:auto;padding:3px 0px;text-align:center;vertical-align:middle;}\n"));

  WSContentSend_P (PSTR (".title {font-size:5vh;}\n"));
  WSContentSend_P (PSTR (".time {font-size:20px;}\n"));
  WSContentSend_P (PSTR (".bold {font-weight:bold;}\n"));

  WSContentSend_P (PSTR (".graph {max-width:%dpx;}\n"), MOTION_GRAPH_WIDTH);

  WSContentSend_P (PSTR ("rect.graph {stroke:grey;stroke-dasharray:1;fill:#101010;}\n"));
  WSContentSend_P (PSTR ("line.dash {stroke:grey;stroke-width:1;stroke-dasharray:8;}\n"));
  WSContentSend_P (PSTR ("polyline.active {fill:none;stroke:yellow;stroke-width:2;}\n"));
  WSContentSend_P (PSTR ("polyline.detected {fill:none;stroke:orange;stroke-width:2;}\n"));
  WSContentSend_P (PSTR ("polyline.enabled {fill:none;stroke:red;stroke-width:2;}\n"));
  WSContentSend_P (PSTR ("text.active {stroke:yellow;fill:yellow;}\n"));
  WSContentSend_P (PSTR ("text.detected {stroke:orange;fill:orange;}\n"));
  WSContentSend_P (PSTR ("text.enabled {stroke:red;fill:red;}\n"));
  WSContentSend_P (PSTR ("text.time {stroke:white;fill:white;}\n"));

  WSContentSend_P (PSTR ("</style>\n</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body>\n"));

  // room name
  WSContentSend_P (PSTR ("<div class='title bold'>%s</div>\n"), SettingsText(SET_FRIENDLYNAME1));

  // display graph
  WSContentSend_P (PSTR ("<div class='graph'>\n"));
  MotionWebDisplayGraph ();
  WSContentSend_P (PSTR ("</div>\n"));

  // end of page
  WSContentStop ();
}

#endif  // USE_WEBSERVER

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xdrv98 (uint8_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
    case FUNC_PRE_INIT:
      MotionPreInit ();
      break;
    case FUNC_COMMAND:
      result = MotionMqttCommand ();
      break;
    case FUNC_EVERY_250_MSECOND:
      MotionEvery250ms ();
      break;
  }
  
  return result;
}

bool Xsns98 (uint8_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
    case FUNC_EVERY_SECOND:
      MotionEverySecond ();
      break;
    case FUNC_JSON_APPEND:
      MotionShowJSON (true);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      WebServer->on ("/" D_PAGE_MOTION_CONFIG, MotionWebPageConfigure);
      WebServer->on ("/" D_PAGE_MOTION_TOGGLE, MotionWebPageToggle);
      WebServer->on ("/" D_PAGE_MOTION_GRAPH, MotionWebPageGraph);
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      MotionWebMainButton ();
      break;
    case FUNC_WEB_ADD_BUTTON:
      MotionWebConfigButton ();
      break;
    case FUNC_WEB_SENSOR:
      MotionWebSensor ();
      break;
#endif  // USE_WEBSERVER

  }
  
  return result;
}

#endif // USE_MOTION
