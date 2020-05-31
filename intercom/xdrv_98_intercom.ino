/*
  xdrv_98_motion.ino - Motion detector management with tempo and timers (~16.5 kb)
  
  Copyright (C) 2020  Nicolas Bernaerts
    21/05/2020 - v1.0 - Creation
    26/05/2020 - v1.1 - Add Information JSON page
    28/05/2020 - v1.2 - Define number of rings before opening gate
    30/05/2020 - v1.3 - Separate Intercom and Gate in JSON
                   
  Input devices should be configured as followed :
   - Serial Out = Switch2
   - Serial In  = Relay2

  Settings are stored using weighting scale parameters :
   - Settings.energy_power_calibration   = Global activation timeout (seconds)
   - Settings.energy_kWhtoday            = Number of rings to open
   - Settings.energy_voltage_calibration = Maximum time to pulse the rings (seconds)
   - Settings.energy_current_calibration = Latch opening duration (seconds)

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
 *               Intercom management
\*************************************************/

#ifdef USE_INTERCOM

#define XDRV_98                      98
#define XSNS_98                      98

#define INTERCOM_BUTTON              1       // switch2
#define INTERCOM_RELAY               2       // relay2

#define INTERCOM_MAX_RING            5       // max number of rings

#define INTERCOM_TIMEOUT_ACT         120     // max time value (seconds or minutes)
#define INTERCOM_TIMEOUT_RING        60      // max timeout for rings (seconds)
#define INTERCOM_TIMEOUT_LATCH       10      // max timeout for latch pressed
#define INTERCOM_TIMEOUT_UPDATE      5       // update period if active (seconds) 
#define INTERCOM_DELAY_WILLOPEN      2       // delay once ring is detected
#define INTERCOM_DELAY_OPENED        10      // delay once gate is opened

#define INTERCOM_BUFFER_SIZE         128

#define D_PAGE_INTERCOM_CONFIG       "config"

#define D_CMND_INTERCOM_ACTIVE       "active"
#define D_CMND_INTERCOM_TACT         "tact"
#define D_CMND_INTERCOM_TRING        "tring"
#define D_CMND_INTERCOM_TLATCH       "tlatch"
#define D_CMND_INTERCOM_NRING        "nring"

#define D_JSON_INTERCOM              "Intercom"
#define D_JSON_INTERCOM_ACTIVE       "Active"
#define D_JSON_INTERCOM_TACT         "Timeout"
#define D_JSON_INTERCOM_TRING        "Ringing"
#define D_JSON_INTERCOM_TLATCH       "Latch"
#define D_JSON_INTERCOM_NRING        "NumRing"
#define D_JSON_INTERCOM_ON           "ON"
#define D_JSON_INTERCOM_OFF          "OFF"
#define D_JSON_INTERCOM_GATE         "Gate"
#define D_JSON_INTERCOM_STATE        "State"
#define D_JSON_INTERCOM_LABEL        "Label"
#define D_JSON_INTERCOM_TIMELEFT     "Timeleft"

#define D_INTERCOM                   "Intercom"
#define D_INTERCOM_GATE              "Gate"
#define D_INTERCOM_BELL              "Bell"
#define D_INTERCOM_ACTIVE            "Active"
#define D_INTERCOM_TACT              "Activation timeout (mn)"
#define D_INTERCOM_NRING             "Number of rings"
#define D_INTERCOM_TRING             "Ring timeout (sec)"
#define D_INTERCOM_TLATCH            "Latch open (sec)"
#define D_INTERCOM_STATE             "State"
#define D_INTERCOM_RINGING           "Ringing"
#define D_INTERCOM_TIMELEFT          "Time left"
#define D_INTERCOM_CONFIG            "Configure"

#define D_GATE_DISABLED              "Disabled"
#define D_GATE_WAITING               "Waiting"
#define D_GATE_RINGING               "Ringing"
#define D_GATE_WILLOPEN              "Will open"
#define D_GATE_LATCH                 "Latch On"
#define D_GATE_OPENED                "Opened"

// intercom commands
enum IntercomCommands { CMND_INTERCOM_ACTIVE, CMND_INTERCOM_TACT, CMND_INTERCOM_TRING, CMND_INTERCOM_TLATCH, CMND_INTERCOM_NRING };
const char kIntercomCommands[] PROGMEM = D_CMND_INTERCOM_ACTIVE "|" D_CMND_INTERCOM_TACT "|" D_CMND_INTERCOM_TRING "|" D_CMND_INTERCOM_TLATCH "|" D_CMND_INTERCOM_NRING;

// form topic style
const char INTERCOM_TOPIC_STYLE[] PROGMEM = "style='float:right;font-size:0.7rem;'";

/*************************************************\
 *               Variables
\*************************************************/

// intercom gate states
enum IntercomGateStates { GATE_DISABLED, GATE_WAITING, GATE_RINGING, GATE_WILLOPEN, GATE_LATCH, GATE_OPENED };

// variables
bool     intercom_updated = false;               // intercom data has been updated
uint8_t  gate_state       = GATE_DISABLED;       // gate opening is disabled
uint8_t  gate_last_state  = GATE_DISABLED;       // previous recorded state
uint8_t  gate_rings       = 0;                   // number of time ring has been pressed
unsigned long gate_time_enabled = 0;             // time when gate was enabled (for timeout)
unsigned long gate_time_changed = 0;             // time of last state change
unsigned long gate_time_updated = 0;             // time of last JSON update
unsigned long gate_time_timeout = 0;             // timeout of current status

/**************************************************\
 *                  Accessors
\**************************************************/

// Enable intercom opening
String IntercomGateStateLabel ()
{
  String str_label;

  // handle command
  switch (gate_state)
  {
    case GATE_DISABLED:  // gate disabled
      str_label = D_GATE_DISABLED;
      break;
    case GATE_WAITING:   // waiting for ring
      str_label = D_GATE_WAITING + String (" (") + IntercomGetTimeLeft (gate_time_enabled, 60000 * IntercomGetActiveTimeout ()) + ")";
      break;
    case GATE_RINGING:   // actually ringing
      str_label = D_GATE_RINGING + String (" (") + String (gate_rings) + "/" + String (IntercomGetNumberRing ()) + ")";
      break;
    case GATE_WILLOPEN:  // rings detected, will open latch
      str_label = D_GATE_WILLOPEN;
      break;
    case GATE_LATCH:     // opening latch
      str_label = D_GATE_LATCH;
      break;
    case GATE_OPENED:     // gate opened
      str_label = D_GATE_OPENED;
      break;
  }

  return str_label;
}

// get number of rings to open gate
int IntercomGetNumberRing ()
{
  return max (1, min (INTERCOM_MAX_RING, int (Settings.energy_kWhtoday)));
}

// set number of rings to open gate
void IntercomSetNumberRing (int count)
{
  Settings.energy_kWhtoday = max (1, min (INTERCOM_MAX_RING, count));
}

// get timeout after intercom enabling (minutes)
int IntercomGetActiveTimeout ()
{
  return min (INTERCOM_TIMEOUT_ACT, int (Settings.energy_power_calibration));
}

// set timeout after intercom enabling (seconds)
void IntercomSetActiveTimeout (int timeout)
{
  Settings.energy_power_calibration = (unsigned long) min (INTERCOM_TIMEOUT_ACT, timeout);
}

// get timeout between first and last ring (seconds)
int IntercomGetRingingTimeout ()
{
  return min (INTERCOM_TIMEOUT_RING, int (Settings.energy_voltage_calibration));
}

// set delay between ring and gate opening (seconds)
void IntercomSetRingingTimeout (int timeout)
{
  Settings.energy_voltage_calibration = (unsigned long) min (INTERCOM_TIMEOUT_RING, timeout);
}

// get gate opening signal duration (seconds)
int IntercomGetLatchTimeout ()
{
  return min (INTERCOM_TIMEOUT_LATCH, int (Settings.energy_current_calibration));
}

// set gate opening signal duration (seconds)
void IntercomSetLatchTimeout (int timeout)
{
  Settings.energy_current_calibration = (unsigned long) min (INTERCOM_TIMEOUT_LATCH, timeout);
}

/**************************************************\
 *                  Functions
\**************************************************/

// get intercom status
bool IntercomIsEnabled ()
{
  return (gate_state != GATE_DISABLED);
}

// Activate / deactivate intercom gate opening
void IntercomSetActive (const char* str_string)
{
  bool   is_enabled, ask_enable, ask_disable;
  String str_command = str_string;

  // check for actual state and asked state
  is_enabled  = IntercomIsEnabled ();
  ask_enable  =  (str_command.equals ("1") || str_command.equalsIgnoreCase (D_JSON_INTERCOM_ON));
  ask_disable =  (str_command.equals ("0") || str_command.equalsIgnoreCase (D_JSON_INTERCOM_OFF));

  // enable or disable intercom
  if ((ask_enable == true) && (is_enabled == false))
  {
    gate_state = GATE_WAITING;
    gate_time_enabled = millis ();
  }
  else if ((ask_disable == true) && (is_enabled == true)) gate_state = GATE_DISABLED;

  // ask for update
  intercom_updated = true;
}

// Toggle intercom gate opening
void IntercomToggleState ()
{
  // toggle intercom state
  if (IntercomIsEnabled () == false) IntercomSetActive (D_JSON_INTERCOM_ON);
  else IntercomSetActive (D_JSON_INTERCOM_OFF);
}

// Open gate
void IntercomGateOpen ()
{
  ExecuteCommandPower (INTERCOM_RELAY, POWER_ON, SRC_MAX);
}

// Close gate
void IntercomGateClose ()
{
  ExecuteCommandPower (INTERCOM_RELAY, POWER_OFF, SRC_MAX);
}

// get temporisation left in readable format
String IntercomGetTimeLeft (unsigned long time_start, unsigned long timeout)
{
  TIME_T        time_dst;
  unsigned long time_now, time_left;
  char          str_number[8];
  String        str_timeleft;

  // get current time
  time_now = millis ();

  // if temporisation is not over
  if (time_now - time_start < timeout)
  {
    // convert to readable format
    time_left = (time_start + timeout - time_now) / 1000;
    BreakTime ((uint32_t) time_left, time_dst);
    sprintf (str_number, "%02d", time_dst.minute);
    str_timeleft = str_number + String (":");
    sprintf (str_number, "%02d", time_dst.second);
    str_timeleft += str_number;
  }
  else str_timeleft = "---";

  return str_timeleft;
}

// Show JSON status (for MQTT)
void IntercomShowJSON (bool append)
{
  bool     show_intercom = false;
  bool     show_gate = false;
  TIME_T   tempo_dst;
  uint8_t  motion_level;
  unsigned long value, time_total;
  String   str_json, str_mqtt, str_text;


  // Intercom section
  //   "Intercom":{"Active":"OFF","ActiveTimeout":5,"RingTimeout":3,"LatchTimeout":2,"NumRing":3}
  // ----------------
  str_json = "\"" + String (D_JSON_INTERCOM) + "\":{";

  // enabled
  if (IntercomIsEnabled ()) str_text = D_JSON_INTERCOM_ON;
  else str_text = D_JSON_INTERCOM_OFF;
  str_json += "\"" + String (D_JSON_INTERCOM_ACTIVE) + "\":\"" + str_text + "\",";

  // global timeout
  value = IntercomGetActiveTimeout ();
  str_json += "\"" + String (D_JSON_INTERCOM_TACT) + "\":" + String (value) + ",";

  // ringing timeout
  value = IntercomGetRingingTimeout ();
  str_json += "\"" + String (D_JSON_INTERCOM_TRING) + "\":" + String (value) + ",";

  // timeout
  value = IntercomGetLatchTimeout ();
  str_json += "\"" + String (D_JSON_INTERCOM_TLATCH) + "\":" + String (value) + ",";

  // number of rings
  value = IntercomGetNumberRing ();
  str_json += "\"" + String (D_JSON_INTERCOM_NRING) + "\":" + String (value) + "}";


  // Gate section
  //   "Gate":{"State":1,"Label":"Waiting","Timeleft":"02:18"}
  // ----------------
  str_json += ",\"" + String (D_JSON_INTERCOM_GATE) + "\":{";

  // status
  str_json += "\"" + String (D_JSON_INTERCOM_STATE) + "\":" + String (gate_state) + ",";

  // label
  str_text = IntercomGateStateLabel ();
  str_json += "\"" + String (D_JSON_INTERCOM_LABEL) + "\":\"" + str_text + "\"}";

  // Publish message
  // ----------------

  // generate MQTT message according to append mode
  if (append == true) str_mqtt = mqtt_data + String (",") + str_json;
  else str_mqtt = "{" + str_json + "}";

  // place JSON back to MQTT data and publish it if not in append mode
  snprintf_P (mqtt_data, sizeof(mqtt_data), str_mqtt.c_str ());
  if (append == false) MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));

  // reset updated flag
  intercom_updated  = false;
  gate_time_updated = millis ();
}

// Handle detector MQTT commands
bool IntercomMqttCommand ()
{
  bool command_handled = true;
  int  command_code;
  char command [CMDSZ];

  // check MQTT command
  command_code = GetCommandCode (command, sizeof(command), XdrvMailbox.topic, kIntercomCommands);

  // handle command
  switch (command_code)
  {
    case CMND_INTERCOM_ACTIVE:       // enable/disable intercom
      IntercomSetActive (XdrvMailbox.data);
      break;
    case CMND_INTERCOM_TACT:        // timeout to disable intercom
      IntercomSetActiveTimeout (XdrvMailbox.payload);
      break;
    case CMND_INTERCOM_TRING:       // set delay after ring
      IntercomSetRingingTimeout (XdrvMailbox.payload);
      break;
    case CMND_INTERCOM_TLATCH:      // set gate open duration
      IntercomSetLatchTimeout (XdrvMailbox.payload);
      break;
    case CMND_INTERCOM_NRING:       // number of rings before opening gate
      IntercomSetNumberRing (XdrvMailbox.payload);
      break;
    default:
      command_handled = false;
  }

  // if needed, send updated status
  if (command_handled == true) IntercomShowJSON (false);
  
  return command_handled;
}

// called when bell button is pressed
bool IntercomBellPressed ()
{
  bool served = false;

  if ((XdrvMailbox.index == INTERCOM_BUTTON) && (XdrvMailbox.payload == PRESSED) && (Button.last_state[INTERCOM_BUTTON] == NOT_PRESSED))
  {
    // log
    AddLog_P (LOG_LEVEL_INFO, D_INTERCOM_RINGING);

    // if intercom is enabled, increment rings number
    if (IntercomIsEnabled ()) gate_rings ++;

    // notification served
    served = true;
  }

  return served;
}

// called when bell button is pressed
bool IntercomBellIsPressed ()
{
  return (Button.last_state[INTERCOM_BUTTON] == PRESSED);
}

// update intercom state
void IntercomEvery250ms ()
{
  unsigned long time_now, time_over;

  // get current time
  time_now   = millis ();

  // check for timeout (GATE_WAITING till GATE_WILLOPEN)
  if ((gate_state > GATE_DISABLED) && (gate_state < GATE_LATCH))
  {
    // if timeout is reached, disable intercom
    time_over = 60000 * IntercomGetActiveTimeout ();
    if (time_now - gate_time_enabled > time_over) gate_state = GATE_DISABLED;

    // if JSON update timeout is reached, force update
    time_over = 1000 * INTERCOM_TIMEOUT_UPDATE;
    if (time_now - gate_time_updated > time_over) intercom_updated = true;
  }

  // handle states transition
  switch (gate_state)
  {
    // waiting for ring
    case GATE_WAITING: 
       // if at least one ring 
      if (gate_rings > 0) gate_state = GATE_RINGING;
      break;

    // ringing
    case GATE_RINGING:       
      // if number of rings reached 
      if (gate_rings >= IntercomGetNumberRing ())
      {
        gate_state = GATE_WILLOPEN;
        gate_rings = 0;
      }

      // else, check for ringing timeout
      else
      {
        // if timeout is reached, switch back to waiting state
        time_over = 1000 * IntercomGetRingingTimeout ();
        if (time_now - gate_time_changed > time_over)
        {
          gate_state = GATE_WAITING;
          gate_rings = 0;
        }
      }
      break;

    // time between ring and gate open
    case GATE_WILLOPEN:       
      // check for delay before opening latch
      time_over = 1000 * INTERCOM_DELAY_WILLOPEN;
      if (time_now - gate_time_changed > time_over)
      {
        gate_state = GATE_LATCH;
        IntercomGateOpen ();
      }
      break;

    // latch is opened
    case GATE_LATCH:    
      // check for latch opened timeout
      time_over = 1000 * IntercomGetLatchTimeout ();
      if (time_now - gate_time_changed > time_over)
      {
        gate_state = GATE_OPENED;
        IntercomGateClose ();
      }
      break;

    // latch has been opened
    case GATE_OPENED:    
      // check for post latch delay
      time_over = 1000 * INTERCOM_DELAY_OPENED;
      if (time_now - gate_time_changed > time_over) gate_state = GATE_DISABLED;
      break;
  }

  // if state changed, reset current state
  if (gate_state != gate_last_state)
  {
    gate_last_state   = gate_state;
    gate_time_changed = time_now;
    intercom_updated  = true;
  }

  // if some important data have been updated, publish JSON
  if (intercom_updated == true) IntercomShowJSON (false);
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// detector configuration page button
void IntercomWebConfigButton ()
{
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s %s</button></form></p>"), D_PAGE_INTERCOM_CONFIG, D_INTERCOM_CONFIG, D_INTERCOM);
}

// append detector state to main page
bool IntercomWebSensor ()
{
  WSContentSend_PD (PSTR ("{s}%s{m}%s{e}"), D_INTERCOM_STATE, IntercomGateStateLabel ().c_str ());
}

// Motion config web page
void IntercomWebPageConfigure ()
{
  unsigned long value;
  char     argument[INTERCOM_BUFFER_SIZE];
  
  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // page comes from save button on configuration page
  if (WebServer->hasArg ("save"))
  {
    // set gate timeout according to 'tact' parameter
    WebGetArg (D_CMND_INTERCOM_TACT, argument, INTERCOM_BUFFER_SIZE);
    if (strlen(argument) > 0) IntercomSetActiveTimeout (atoi (argument));

    // set ringing timeout according to 'tring' parameter
    WebGetArg (D_CMND_INTERCOM_TRING, argument, INTERCOM_BUFFER_SIZE);
    if (strlen(argument) > 0) IntercomSetRingingTimeout (atoi (argument));
    
    // set gate open timeout according to 'tlatch' parameter
    WebGetArg (D_CMND_INTERCOM_TLATCH, argument, INTERCOM_BUFFER_SIZE);
    if (strlen(argument) > 0) IntercomSetLatchTimeout (atoi (argument));

    // set number of rings according to 'nring' parameter
    WebGetArg (D_CMND_INTERCOM_NRING, argument, INTERCOM_BUFFER_SIZE);
    if (strlen(argument) > 0) IntercomSetNumberRing (atoi (argument));
  }

  // beginning of form
  WSContentStart_P (D_INTERCOM_CONFIG);
  WSContentSendStyle ();
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_PAGE_INTERCOM_CONFIG);

  // gate
  WSContentSend_P (PSTR ("<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>"), D_INTERCOM_GATE);
  value = IntercomGetActiveTimeout ();
  WSContentSend_P (PSTR ("<p>%s<span %s>%s</span><br><input type='number' name='%s' min='0' max='%d' step='1' value='%d'></p>\n"), D_INTERCOM_TACT, INTERCOM_TOPIC_STYLE, D_CMND_INTERCOM_TACT, D_CMND_INTERCOM_TACT, INTERCOM_TIMEOUT_ACT, value);
  value = IntercomGetLatchTimeout ();
  WSContentSend_P (PSTR ("<p>%s<span %s>%s</span><br><input type='number' name='%s' min='0' max='%d' step='1' value='%d'></p>\n"), D_INTERCOM_TLATCH, INTERCOM_TOPIC_STYLE, D_CMND_INTERCOM_TLATCH, D_CMND_INTERCOM_TLATCH, INTERCOM_TIMEOUT_LATCH, value);
  WSContentSend_P (PSTR ("</fieldset></p>\n"));

  // bell
  WSContentSend_P (PSTR ("<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>"), D_INTERCOM_BELL);
  value = IntercomGetRingingTimeout ();
  WSContentSend_P (PSTR ("<p>%s<span %s>%s</span><br><input type='number' name='%s' min='0' max='%d' step='1' value='%d'></p>\n"), D_INTERCOM_TRING, INTERCOM_TOPIC_STYLE, D_CMND_INTERCOM_TRING, D_CMND_INTERCOM_TRING, INTERCOM_TIMEOUT_RING, value);
  value = IntercomGetNumberRing ();
  WSContentSend_P (PSTR ("<p>%s<span %s>%s</span><br><input type='number' name='%s' min='0' max='%d' step='1' value='%d'></p>\n"), D_INTERCOM_NRING, INTERCOM_TOPIC_STYLE, D_CMND_INTERCOM_NRING, D_CMND_INTERCOM_NRING, INTERCOM_MAX_RING, value);
  WSContentSend_P (PSTR ("</fieldset></p>\n"));

  // save button  
  // --------------
  WSContentSend_P (PSTR ("<p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), D_SAVE);
  WSContentSend_P (PSTR ("</form>\n"));

  // configuration button and end of page
  WSContentSpaceButton(BUTTON_CONFIGURATION);
  WSContentStop();
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
    case FUNC_COMMAND:
      result = IntercomMqttCommand ();
      break;
    case FUNC_EVERY_250_MSECOND:
      IntercomEvery250ms ();
      break;
    case FUNC_BUTTON_PRESSED:
      result = IntercomBellPressed();
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
    case FUNC_JSON_APPEND:
      IntercomShowJSON (true);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      WebServer->on ("/" D_PAGE_INTERCOM_CONFIG, IntercomWebPageConfigure);
      break;
    case FUNC_WEB_ADD_BUTTON:
      IntercomWebConfigButton ();
      break;
    case FUNC_WEB_SENSOR:
      IntercomWebSensor ();
      break;
#endif  // USE_WEBSERVER

  }
  
  return result;
}

#endif // USE_INTERCOM
