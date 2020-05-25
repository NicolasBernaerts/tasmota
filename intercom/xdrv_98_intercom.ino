/*
  xdrv_98_motion.ino - Motion detector management with tempo and timers (~16.5 kb)
  
  Copyright (C) 2020  Nicolas Bernaerts
    21/05/2020 - v1.0 - Creation
                   
  Input devices should be configured as followed :
   - Serial Out = Switch2
   - Serial In  = Relay2

  Settings are stored using weighting scale parameters :
   - Settings.energy_voltage_calibration = Delay between ring and gate opening
   - Settings.energy_current_calibration = Gate opening signal duration
   - Settings.energy_power_calibration   = Timeout before disabling intercom

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

#define INTERCOM_BUFFER_SIZE         128
#define INTERCOM_MAX_TIME            120     // max time value (seconds or minutes)
#define INTERCOM_UPDATE              5       // update period if active (seconds) 

#define D_PAGE_INTERCOM_CONFIG       "config"

#define D_CMND_INTERCOM_ACTIVE       "active"
#define D_CMND_INTERCOM_STATE        "state"
#define D_CMND_INTERCOM_DELAY        "delay"
#define D_CMND_INTERCOM_GATE         "gate"
#define D_CMND_INTERCOM_TIMEOUT      "timeout"

#define D_JSON_INTERCOM              "Intercom"
#define D_JSON_INTERCOM_ACTIVE       "Active"
#define D_JSON_INTERCOM_STATE        "State"
#define D_JSON_INTERCOM_LABEL        "Label"
#define D_JSON_INTERCOM_DELAY        "Delay"
#define D_JSON_INTERCOM_DURATION     "Duration"
#define D_JSON_INTERCOM_TIMEOUT      "Timeout"
#define D_JSON_INTERCOM_TIMELEFT     "Timeleft"
#define D_JSON_INTERCOM_ON           "ON"
#define D_JSON_INTERCOM_OFF          "OFF"

#define D_INTERCOM                   "Intercom"
#define D_INTERCOM_DISABLED          "Disabled"
#define D_INTERCOM_WAITING           "Waiting"
#define D_INTERCOM_RINGING           "Ringing"
#define D_INTERCOM_GATE              "Gate open"
#define D_INTERCOM_DISABLED          "Disabled"
#define D_INTERCOM_OFF               "Off"
#define D_INTERCOM_DELAY             "Delay after ring (sec)"
#define D_INTERCOM_DURATION          "Duration of gate open (sec)"
#define D_INTERCOM_TIMEOUT           "Ringing timeout  (mn)"
#define D_INTERCOM_TEMPO             "Temporisation"
#define D_INTERCOM_STATE             "State"
#define D_INTERCOM_BELL              "Bell"
#define D_INTERCOM_TIMELEFT          "Time left"
#define D_INTERCOM_CONFIG            "Configure"


// intercom commands
enum IntercomCommands { CMND_INTERCOM_ACTIVE, CMND_INTERCOM_DELAY, CMND_INTERCOM_GATE, CMND_INTERCOM_TIMEOUT };
const char kIntercomCommands[] PROGMEM = D_CMND_INTERCOM_ACTIVE "|" D_CMND_INTERCOM_DELAY "|" D_CMND_INTERCOM_GATE "|" D_CMND_INTERCOM_TIMEOUT;

// form topic style
const char INTERCOM_TOPIC_STYLE[] PROGMEM = "style='float:right;font-size:0.7rem;'";

/*************************************************\
 *               Variables
\*************************************************/

// intercom states
enum IntercomStates { INTERCOM_DISABLED, INTERCOM_WAITING, INTERCOM_RINGING, INTERCOM_GATE };

// variables
bool          intercom_updated    = false;               // intercom state is updated
uint8_t       intercom_state      = INTERCOM_DISABLED;   // intercom state is disabled
unsigned long intercom_start      = 0;
unsigned long intercom_lastupdate = 0;

/**************************************************\
 *                  Accessors
\**************************************************/

// Enable intercom opening
String IntercomGetStateLabel ()
{
  String str_label;

  // handle command
  switch (intercom_state)
  {
    case INTERCOM_DISABLED:  // intercom disabled
      str_label = D_INTERCOM_DISABLED;
      break;
    case INTERCOM_WAITING:   // intercom waiting for ring
      str_label = D_INTERCOM_WAITING;
      break;
    case INTERCOM_RINGING:   // intercom ringing
      str_label = D_INTERCOM_RINGING;
      break;
    case INTERCOM_GATE:      // gate open
      str_label = D_INTERCOM_GATE;
      break;
  }

  return str_label;
}

// get delay between ring and gate opening (seconds)
int IntercomGetDelay ()
{
  return min (INTERCOM_MAX_TIME, int (Settings.energy_voltage_calibration));
}

// set delay between ring and gate opening (seconds)
void IntercomSetDelay (int delay)
{
  Settings.energy_voltage_calibration = (unsigned long) min (INTERCOM_MAX_TIME, delay);
}

// get gate opening signal duration (seconds)
int IntercomGetGateDuration ()
{
  return min (INTERCOM_MAX_TIME, int (Settings.energy_current_calibration));
}

// set gate opening signal duration (seconds)
void IntercomSetGateDuration (int duration)
{
  Settings.energy_current_calibration = (unsigned long) min (INTERCOM_MAX_TIME, duration);
}

// get timeout after intercom enabling (minutes)
int IntercomGetTimeout ()
{
  return min (INTERCOM_MAX_TIME, int (Settings.energy_power_calibration));
}

// set timeout after intercom enabling (seconds)
void IntercomSetTimeout (int timeout)
{
  Settings.energy_power_calibration = (unsigned long) min (INTERCOM_MAX_TIME, timeout);
}

/**************************************************\
 *                  Functions
\**************************************************/

// Enable or disable intercom gate opening
void IntercomSetState (const char* str_string)
{
  String str_command = str_string;

  // if intercom should be enabled
  if (str_command.equals ("1") || str_command.equalsIgnoreCase (D_JSON_INTERCOM_ON))
  {
    if (intercom_state == INTERCOM_DISABLED)
    {
      // switch to waiting state
      intercom_start   = millis ();
      intercom_state   = INTERCOM_WAITING;
      intercom_updated = true;
    }
  }

  // else if intercom should be disabled
  else if (str_command.equals ("0") || str_command.equalsIgnoreCase (D_JSON_INTERCOM_OFF))
  {
    // close gate
    IntercomGateClose ();

    // switch to waiting state
    intercom_state   = INTERCOM_DISABLED;
    intercom_updated = true;
  }
}

// Toggle intercom gate opening
void IntercomToggleState ()
{
  // toggle intercom state
  if (intercom_state == INTERCOM_DISABLED) IntercomSetState (D_JSON_INTERCOM_ON);
  else IntercomSetState (D_JSON_INTERCOM_OFF);
}

// Open gate
void IntercomGateOpen ()
{
  // open gate
  ExecuteCommandPower (INTERCOM_RELAY, POWER_ON, SRC_MAX);
}

// Close gate
void IntercomGateClose ()
{
  // close gate
  ExecuteCommandPower (INTERCOM_RELAY, POWER_OFF, SRC_MAX);
}

// get temporisation left in readable format
String IntercomGetTimeLeft ()
{
  TIME_T        time_dst;
  unsigned long time_now, time_out, time_left;
  char          str_number[8];
  String        str_timeleft;

  // if timeout has started
  if (intercom_state != INTERCOM_DISABLED)
  {
    // get current time
    time_now = millis ();

    // get temporisation according to gate status
    if (intercom_state == INTERCOM_GATE) time_out = 1000 * IntercomGetGateDuration ();
    else time_out = 60 * 1000 * IntercomGetTimeout ();

    // if temporisation is not over
    if (time_now - intercom_start < time_out)
    {
      // convert to readable format
      time_left = (intercom_start + time_out - time_now) / 1000;
      BreakTime ((uint32_t) time_left, time_dst);
      sprintf (str_number, "%02d", time_dst.minute);
      str_timeleft = str_number + String (":");
      sprintf (str_number, "%02d", time_dst.second);
      str_timeleft += str_number;
    }
    else str_timeleft = "---";
  }

  return str_timeleft;
}

// Show JSON status (for MQTT)
void IntercomShowJSON (bool append)
{
  TIME_T  tempo_dst;
  uint8_t  motion_level;
  unsigned long value, time_total;
  String   str_json, str_mqtt, str_text;

  // Intercom section  -->  "Intercom":{"Enabled":0,"State":1,"Label":"Disabled","Delay":3,"Duration":5,"Timeout":5,"Timeleft":"2:15"}
  str_json = "\"" + String (D_JSON_INTERCOM) + "\":{";

  // enabled
  if (intercom_state == INTERCOM_DISABLED) str_text = D_JSON_INTERCOM_OFF;
  else str_text = D_JSON_INTERCOM_ON;
  str_json += "\"" + String (D_JSON_INTERCOM_ACTIVE) + "\":\"" + str_text + "\",";

  // state
  str_text = IntercomGetStateLabel ();
  str_json += "\"" + String (D_JSON_INTERCOM_STATE) + "\":" + String (intercom_state) + ",";
  str_json += "\"" + String (D_JSON_INTERCOM_LABEL) + "\":\"" + str_text + "\",";
  
  // delay
  value = IntercomGetDelay ();
  str_json += "\"" + String (D_JSON_INTERCOM_DELAY) + "\":" + String (value) + ",";

  // duration
  value = IntercomGetGateDuration ();
  str_json += "\"" + String (D_JSON_INTERCOM_DURATION) + "\":" + String (value) + ",";

  // timeout
  value = IntercomGetTimeout ();
  str_json += "\"" + String (D_JSON_INTERCOM_TIMEOUT) + "\":" + String (value);

  // time left
  if (intercom_state != INTERCOM_DISABLED)
  {
    str_text = IntercomGetTimeLeft ();
    str_json += ",\"" + String (D_JSON_INTERCOM_TIMELEFT) + "\":\"" + str_text + "\"";
  }
  str_json += "}";

  // generate MQTT message according to append mode
  if (append == true) str_mqtt = mqtt_data + String (",") + str_json;
  else str_mqtt = "{" + str_json + "}";

  // place JSON back to MQTT data and publish it if not in append mode
  snprintf_P (mqtt_data, sizeof(mqtt_data), str_mqtt.c_str ());
  if (append == false) MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));
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
    case CMND_INTERCOM_ACTIVE:    // enable/disable intercom
      IntercomSetState (XdrvMailbox.data);
      break;
    case CMND_INTERCOM_DELAY:       // set delay after ring
      IntercomSetDelay (XdrvMailbox.payload);
      break;
    case CMND_INTERCOM_GATE:    // set gate open duration
      IntercomSetGateDuration (XdrvMailbox.payload);
      break;
    case CMND_INTERCOM_TIMEOUT:     // timeout to disable intercom
      IntercomSetTimeout (XdrvMailbox.payload);
      break;
    default:
      command_handled = false;
  }

  // if needed, send updated status
  if (command_handled == true) IntercomShowJSON (false);
  
  return command_handled;
}

// update intercom state
void IntercomEvery250ms ()
{
  uint8_t  prev_state;
  unsigned long time_now, time_over;

  // save current state and get current time
  prev_state = intercom_state;
  time_now   = millis ();

  // if intercom gate opening is disabled, reset update time reference
  if ((intercom_state == INTERCOM_DISABLED) && (intercom_lastupdate != 0)) intercom_lastupdate = 0;

  // if gate opening is enabled
  if (intercom_state != INTERCOM_DISABLED)
  {
    // check if update timeout is reached
    if (intercom_lastupdate == 0) intercom_lastupdate = time_now;
    time_over = INTERCOM_UPDATE * 1000;
    if (time_now - intercom_lastupdate > time_over)
    {
      // reset update tiem reference and ask for update
      intercom_lastupdate = time_now;
      intercom_updated    = true;
    }

    // check if timeout is reached
    time_over = 60 * 1000 * IntercomGetTimeout ();
    if (time_now - intercom_start > time_over)
    {
      // if gate is opened, close it
      if (intercom_state == INTERCOM_GATE) IntercomGateClose ();

      // intercom is disabled
      intercom_state   = INTERCOM_DISABLED;
      intercom_updated = true;
    }
  }

  // handle action according to intercom state
  switch (intercom_state)
  {
    // intercom is ringing
    case INTERCOM_RINGING:       
      // check if delay is reached
      time_over = 1000 * IntercomGetDelay ();
      if (time_now - intercom_start > time_over)
      {
        // open gate
        IntercomGateOpen ();

        // switch to gate open state
        intercom_start = time_now;
        intercom_state = INTERCOM_GATE;
      }
      break;

    // gate is open
    case INTERCOM_GATE:    
      // check if duration is reached
      time_over = 1000 * IntercomGetGateDuration ();
      if (time_now - intercom_start > time_over)
      {
        // close gate
        IntercomGateClose ();

        // switch to disabled state
        intercom_state = INTERCOM_DISABLED;
      }
      break;
  }

  // if state changed, intercom updated
  if (prev_state != intercom_state) intercom_updated = true;

  // if some important data have been updated, publish JSON
  if (intercom_updated == true)
  {
    IntercomShowJSON (false);
    intercom_updated = false;
  }
}

// called when bell button is pressed
bool IntercomBellPressed(void)
{
  bool served = false;

  if ((XdrvMailbox.index == INTERCOM_BUTTON) && (XdrvMailbox.payload == PRESSED) && (Button.last_state[INTERCOM_BUTTON] == NOT_PRESSED))
  {
    // if intercom is in waiting state
    if (intercom_state == INTERCOM_WAITING)
      {
        // switch to ringing state
        intercom_start   = millis ();
        intercom_state   = INTERCOM_RINGING;
        intercom_updated = true;
      }

    // log 
    AddLog_P(LOG_LEVEL_INFO, PSTR("Bell ringing"));
    served = true;
  }

  return served;
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
  String str_bell, str_state, str_timeleft;

  // get ring status
  if (Button.last_state[INTERCOM_BUTTON] == PRESSED) str_bell = D_INTERCOM_RINGING;
  else str_bell = D_INTERCOM_OFF;

  // read status and timeleft
  str_state    = IntercomGetStateLabel ();
  str_timeleft = IntercomGetTimeLeft ();

  // display result
  WSContentSend_PD (PSTR ("{s}%s{m}%s{e}"), D_INTERCOM_BELL, str_bell.c_str ());
  WSContentSend_PD (PSTR ("{s}%s{m}%s{e}"), D_INTERCOM_STATE, str_state.c_str ());
  if (str_timeleft.length() > 0) WSContentSend_PD (PSTR ("{s}%s{m}%s{e}"), D_INTERCOM_TIMELEFT, str_timeleft.c_str ());
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
    // set delay according to 'delay' parameter
    WebGetArg (D_CMND_INTERCOM_DELAY, argument, INTERCOM_BUFFER_SIZE);
    if (strlen(argument) > 0) IntercomSetDelay (atoi (argument));
    
    // set gate open duration according to 'gate' parameter
    WebGetArg (D_CMND_INTERCOM_GATE, argument, INTERCOM_BUFFER_SIZE);
    if (strlen(argument) > 0) IntercomSetGateDuration (atoi (argument));
    
    // set gate open duration according to 'gate' parameter
    WebGetArg (D_CMND_INTERCOM_TIMEOUT, argument, INTERCOM_BUFFER_SIZE);
    if (strlen(argument) > 0) IntercomSetTimeout (atoi (argument));
  }

  // beginning of form
  WSContentStart_P (D_INTERCOM_CONFIG);
  WSContentSendStyle ();
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_PAGE_INTERCOM_CONFIG);

  // temporisation
  WSContentSend_P (PSTR ("<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>"), D_INTERCOM_TEMPO);
  value = IntercomGetDelay ();
  WSContentSend_P (PSTR ("<p>%s<span %s>%s</span><br><input type='number' name='%s' min='0' max='%d' step='1' value='%d'></p>\n"), D_INTERCOM_DELAY, INTERCOM_TOPIC_STYLE, D_CMND_INTERCOM_DELAY, D_CMND_INTERCOM_DELAY, INTERCOM_MAX_TIME, value);
  value = IntercomGetGateDuration ();
  WSContentSend_P (PSTR ("<p>%s<span %s>%s</span><br><input type='number' name='%s' min='0' max='%d' step='1' value='%d'></p>\n"), D_INTERCOM_DURATION, INTERCOM_TOPIC_STYLE, D_CMND_INTERCOM_GATE, D_CMND_INTERCOM_GATE, INTERCOM_MAX_TIME, value);
  value = IntercomGetTimeout ();
  WSContentSend_P (PSTR ("<p>%s<span %s>%s</span><br><input type='number' name='%s' min='0' max='%d' step='1' value='%d'></p>\n"), D_INTERCOM_TIMEOUT, INTERCOM_TOPIC_STYLE, D_CMND_INTERCOM_TIMEOUT, D_CMND_INTERCOM_TIMEOUT, INTERCOM_MAX_TIME, value);
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
