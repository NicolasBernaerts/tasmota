/*
  xdrv_98_motion_light.ino - Corridor light management with motion detection and timers (~16.5 kb)
  
  Copyright (C) 2020  Nicolas Bernaerts
    27/03/2020 - v1.0 - Creation
    10/04/2020 - v1.1 - Add detector configuration for low/high level
    15/04/2020 - v1.2 - Add detection auto rearm flag management
    18/04/2020 - v1.3 - Handle Toggle button and display motion icon
    15/05/2020 - v1.4 - Add /json page to get latest motion JSON
    20/05/2020 - v1.5 - Add configuration for first NTP server
    26/05/2020 - v1.6 - Add Information JSON page and enable discovery (mDNS)
    21/09/2020 - v1.7 - Add switch and icons on control page
                        Based on Tasmota 8.4
    28/10/2020 - v1.8 - Real time graph page update
    06/11/2020 - v1.9 - Tasmota 9 compatibility
    01/05/2021 - v2.0 - Add fixed IP and remove use of String to avoid heap fragmentation
    05/02/2022 - v2.1 - Rewrite and Tasmota 10 compatibility
    09/06/2022 - v3.0 - Switch to counters to handle events
                        Tasmota 12 compatibility
                   
  Input devices should be configured as followed :
   - Counter1 = Light On sensor
   - Counter2 = Movement detection sensor
   - Counter3 = Button to force On/Off

  Settings are stored using weighting scale parameters :
   - Settings.energy_power_calibration   = Light timeout (s)

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

#ifdef USE_LIGHT_MOTION

#define XDRV_98                      98

#define MOTION_DETECT_LIGHT          0           // counter 1 : light ON detection
#define MOTION_DETECT_MOVE           1           // counter 2 : movement detector
#define MOTION_DETECT_BUTTON         2           // counter 3 : On/Off button pressed

#define MOTION_JSON_UPDATE           5           // update JSON every 5 sec
#define MOTION_TIMEOUT_DEFAULT       180         // timeout after 3 mn

/*************************************************\
 *               Variables
\*************************************************/

// motion status
enum MotionStatus { MOTION_STATUS_DISABLED, MOTION_STATUS_ENABLED, MOTION_STATUS_FORCED, MOTION_STATUS_MAX };

// motion states
enum MotionState { MOTION_STATE_INACTIVE, MOTION_STATE_SWITCH_ON, MOTION_STATE_ACTIVE, MOTION_STATE_SWITCH_OFF, MOTION_STATE_MAX };

// motion sources
enum MotionSource { MOTION_SOURCE_NONE, MOTION_SOURCE_DETECTOR, MOTION_SOURCE_BUTTON, MOTION_SOURCE_EXTERNAL, MOTION_SOURCE_MAX };
//const char kMotionSource[] PROGMEM = "None|Detector|Button|External";

// light and motion commands
const char kMotionCommands[] PROGMEM = "lm_" "|" "help" "|" "timeout" "|" "status";
void (* const MotionCommand[])(void) PROGMEM = { &CmndMotionHelp, &CmndMotionTimeout, &CmndMotionStatus };

// configuration
struct {
  uint32_t timeout = UINT32_MAX;              // temporisation after motion detection
} motion_config;

// variables
struct {
  uint8_t  status  = MOTION_STATUS_ENABLED;   // light enabled at boot
  uint8_t  state   = MOTION_STATE_INACTIVE;   // light switched off at boot
  uint8_t  source  = MOTION_SOURCE_NONE;      // light activation source
  uint32_t count_light  = UINT32_MAX;         // light ON detection counter
  uint32_t count_move   = UINT32_MAX;         // movement detection counter
  uint32_t count_button = UINT32_MAX;         // command button counter
  uint32_t time_stop    = UINT32_MAX;         // when light should be stopped
  uint32_t time_json    = UINT32_MAX;         // when JSON should be updated
} motion_status;

/**************************************************\
 *                  Accessors
\**************************************************/

// load configuration
void MotionLoadConfig ()
{
  motion_config.timeout = Settings->energy_power_calibration;
  if (motion_config.timeout == UINT32_MAX) motion_config.timeout = MOTION_TIMEOUT_DEFAULT;
}

// save configuration
void MotionSaveConfig ()
{
  Settings->energy_power_calibration = motion_config.timeout;
}

/***************************************\
 *               Commands
\***************************************/

// motion detection help
void CmndMotionHelp ()
{
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Light & motion commands :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - lm_enable  = disable/enable light (0/1)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - lm_timeout = timeout after movement detection (in sec.)"));
  ResponseCmndDone();
}

// set motion detection temporisation
void CmndMotionTimeout ()
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload > 0))
  {
    // update flag
    motion_config.timeout = (uint32_t)XdrvMailbox.payload;

    // save configuration
    MotionSaveConfig ();
  }

  // ask for JSON update
  motion_status.time_json = millis ();

  ResponseCmndNumber (motion_config.timeout);
}

// set status of light management
void CmndMotionStatus ()
{
  // if new status is valid, set
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload < MOTION_STATUS_MAX)) MotionSetStatus ((uint8_t)XdrvMailbox.payload);

  ResponseCmndNumber (motion_status.status);
}

/***************************************\
 *               Functions
\***************************************/

// set status of light management
void MotionSetStatus (const uint8_t new_status)
{
  // switch according to new status
  switch (new_status)
  {
    case MOTION_STATUS_DISABLED:
      // if light state is ON, switch it OFF
      if (motion_status.state == MOTION_STATE_ACTIVE) MotionChangeState (MOTION_STATE_INACTIVE, MOTION_SOURCE_NONE);
      break;

    case MOTION_STATUS_ENABLED:
      // if light state is ON, set switch OFF timeout
      if (motion_status.state == MOTION_STATE_ACTIVE) motion_status.time_stop = millis () + 1000 * motion_config.timeout;
      break;

    case MOTION_STATUS_FORCED:
      // if light state is OFF, switch it ON
      if (motion_status.state == MOTION_STATE_INACTIVE) MotionChangeState (MOTION_STATE_ACTIVE, MOTION_SOURCE_EXTERNAL);
      break;
  }

  // set new status
  motion_status.status = new_status;
}

// change light state
void MotionChangeState (const uint8_t new_state, const uint8_t new_source)
{
  // switch according to new state
  switch (new_state)
  {
    case MOTION_STATE_INACTIVE:
      // switch relay OFF
      ExecuteCommandPower (1, POWER_OFF, SRC_MAX);

      // reset movement counters
      motion_status.count_light  = RtcSettings.pulse_counter[MOTION_DETECT_LIGHT];
      motion_status.count_move   = RtcSettings.pulse_counter[MOTION_DETECT_MOVE];
      motion_status.count_button = RtcSettings.pulse_counter[MOTION_DETECT_BUTTON];

      // reset timeout
      motion_status.time_stop = UINT32_MAX;
      break;

    case MOTION_STATE_ACTIVE:
      // switch relay ON
      ExecuteCommandPower (1, POWER_ON, SRC_MAX);

      // reset all counters
      motion_status.count_light  = RtcSettings.pulse_counter[MOTION_DETECT_LIGHT];
      motion_status.count_move   = RtcSettings.pulse_counter[MOTION_DETECT_MOVE];
      motion_status.count_button = RtcSettings.pulse_counter[MOTION_DETECT_BUTTON];

      // set timeout
      if (new_source == MOTION_SOURCE_DETECTOR) motion_status.time_stop = millis () + 1000 * motion_config.timeout;
      else motion_status.time_stop = UINT32_MAX;
      break;
  }

  // update state and source
  motion_status.state  = new_state;
  motion_status.source = new_source;

  // ask for JSON update
  motion_status.time_json = millis ();
}

// get temporisation left in readable format
void MotionGetTempoLeft (char* pstr_timeleft, size_t size_timeleft)
{
  TIME_T   tempo_dst;
  uint32_t time_left;

  // parameter control
  if (pstr_timeleft == nullptr) return;

  // init
  if (motion_status.state == MOTION_STATE_ACTIVE) strcpy (pstr_timeleft, "Forced");
    else strcpy (pstr_timeleft, "---");

  // if tempo has been started
  if (motion_status.time_stop != UINT32_MAX)
  {
    // if timeout is over
    if (TimeReached (motion_status.time_stop)) strcpy (pstr_timeleft, "Over");

    // else calculate time left
    else
    {
      time_left = (motion_status.time_stop - millis ()) / 1000;
      BreakTime ((uint32_t) time_left, tempo_dst);
      sprintf (pstr_timeleft, "%02d:%02d", tempo_dst.minute, tempo_dst.second);
    }
  }
}

// Show JSON status (for MQTT)
void MotionShowJSON (bool append)
{
  char str_text[32];

  // reset need for update
  motion_status.time_json = UINT32_MAX;

  // add , in append mode or { in direct publish mode
  if (append) ResponseAppend_P (PSTR (",")); else Response_P (PSTR ("{"));

  // start of corridor section
  ResponseAppend_P (PSTR ("\"Light\":{"));

  // motion timeout
  ResponseAppend_P (PSTR ("\"Timeout\":%u"), motion_config.timeout);

  // enabled or disabled
  ResponseAppend_P (PSTR (",\"Status\":%u"), motion_status.status);

  // current state
  ResponseAppend_P (PSTR (",\"State\":%u"), motion_status.state);

  // current source
  ResponseAppend_P (PSTR (",\"Source\":%u"), motion_status.source);

  // time left
  MotionGetTempoLeft (str_text, sizeof (str_text));
  ResponseAppend_P (PSTR (",\"Timeleft\":\"%s\""), str_text);

  // end of corridor section
  ResponseAppend_P (PSTR ("}"));

  // publish it if not in append mode
  if (!append)
  {
    ResponseAppend_P (PSTR ("}"));
    MqttPublishPrefixTopic_P (TELE, PSTR (D_RSLT_SENSOR));
  } 
}

// update motion and relay state according to status
void MotionEvery100ms ()
{
  uint8_t previous_state = motion_status.state;
  
  // switch according to light current state
  switch (motion_status.state)
  {
    case MOTION_STATE_INACTIVE:
      // if movement allowed and light detector has been triggered
      if ((motion_status.status == MOTION_STATUS_ENABLED) && (RtcSettings.pulse_counter[MOTION_DETECT_LIGHT] > motion_status.count_light)) MotionChangeState (MOTION_STATE_ACTIVE, MOTION_SOURCE_DETECTOR);

      // else if movement allowed and movement detected
      else if ((motion_status.status == MOTION_STATUS_ENABLED) && (RtcSettings.pulse_counter[MOTION_DETECT_MOVE] > motion_status.count_move)) MotionChangeState (MOTION_STATE_ACTIVE, MOTION_SOURCE_DETECTOR);

      // else if button pressed
      else if (RtcSettings.pulse_counter[MOTION_DETECT_BUTTON] > motion_status.count_button) MotionChangeState (MOTION_STATE_SWITCH_ON, MOTION_SOURCE_BUTTON);
      break;
    
    case MOTION_STATE_ACTIVE:
      // if no JSON update planned, plan it
      if (motion_status.time_json == UINT32_MAX) motion_status.time_json = millis () + 1000 * MOTION_JSON_UPDATE;

      // if timeout is active and move is detected, update counter and restart timeout
      if ((motion_status.time_stop != UINT32_MAX) && (motion_status.count_move < RtcSettings.pulse_counter[MOTION_DETECT_MOVE]))
      {
        motion_status.count_move = RtcSettings.pulse_counter[MOTION_DETECT_MOVE];
        motion_status.time_stop  = millis () + 1000 * motion_config.timeout;
      } 

      // if timeout is reached, ask for switch off
      if ((motion_status.time_stop != UINT32_MAX) && TimeReached (motion_status.time_stop)) MotionChangeState (MOTION_STATE_INACTIVE, MOTION_SOURCE_NONE);

      // if button has been pressed, ask to switch OFF
      if (motion_status.count_button < RtcSettings.pulse_counter[MOTION_DETECT_BUTTON]) MotionChangeState (MOTION_STATE_INACTIVE, MOTION_SOURCE_NONE);
      break;
  }

  // check for JSON update
  if ((motion_status.time_json != UINT32_MAX) && TimeReached (motion_status.time_json)) MotionShowJSON (false);
}

// pre init main status
void MotionInit ()
{
  uint16_t index;

  // disable serial log
  Settings->seriallog_level = 0;

  // init counters
  motion_status.count_light  = RtcSettings.pulse_counter[MOTION_DETECT_LIGHT];
  motion_status.count_move   = RtcSettings.pulse_counter[MOTION_DETECT_MOVE];
  motion_status.count_button = RtcSettings.pulse_counter[MOTION_DETECT_BUTTON];

  // disable fast cycle power recovery (SetOption65)
  Settings->flag3.fast_power_cycle_disable = true;

  // load config
  MotionLoadConfig ();

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: lm_help to get help on light and motion commands"));
}

// called just before setting relays
bool MotionSetDevicePower ()
{
  bool result = true;      // by default, do not handle relay commands

  // if device has finished boot process
  if (TasmotaGlobal.uptime > 2)
  {
    // if command comes from timer
    if (XdrvMailbox.payload == SRC_TIMER)
    {
      // if light is enabled and should be disabled
      if ((motion_status.status == MOTION_STATUS_ENABLED) && (XdrvMailbox.index == 0)) MotionSetStatus (MOTION_STATUS_DISABLED);

      // else if light is disabled and should be enabled
      else if ((motion_status.status == MOTION_STATUS_DISABLED) && (XdrvMailbox.index != 0)) MotionSetStatus (MOTION_STATUS_ENABLED);
    } 

    // else if command comes from external trigger
    else if (XdrvMailbox.payload != SRC_MAX)
    {
      // if light should be switched ON
      if ((XdrvMailbox.index != 0) && (motion_status.state == MOTION_STATE_INACTIVE)) MotionChangeState (MOTION_STATE_ACTIVE, MOTION_SOURCE_EXTERNAL);

      // else if light should be switched OFF
      else if ((XdrvMailbox.index == 0) && (motion_status.state == MOTION_STATE_ACTIVE)) MotionChangeState (MOTION_STATE_INACTIVE, MOTION_SOURCE_NONE);
    }

    // set result
    result = (XdrvMailbox.payload != SRC_MAX);
  }

  return result;
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// append detector state to main page
void MotionWebSensor ()
{
  char str_text[16];

  // get time left
  MotionGetTempoLeft (str_text, sizeof (str_text));

  // light is currently disabled
  if (motion_status.status == MOTION_STATUS_DISABLED) WSContentSend_PD (PSTR ("{s}Light{m}💤{e}"));

  // else light is inactive
  else if (motion_status.state != MOTION_STATE_ACTIVE) WSContentSend_PD (PSTR ("{s}Light{m}❌{e}"));

  // else if light has been forced
  else if (motion_status.time_stop == UINT32_MAX) WSContentSend_PD (PSTR ("{s}Light{m}💡{e}"));

  // else display running time left
  else WSContentSend_PD (PSTR ("{s}Light{m}⏳ %s{e}"), str_text);
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
    case FUNC_INIT:
      MotionInit ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kMotionCommands, MotionCommand);
      break;
   case FUNC_SET_DEVICE_POWER:
      result = MotionSetDevicePower ();
      break;
    case FUNC_EVERY_100_MSECOND:
      MotionEvery100ms ();
      break;
    case FUNC_JSON_APPEND:
      MotionShowJSON (true);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      break;
    case FUNC_WEB_SENSOR:
      MotionWebSensor ();
      break;
#endif  // USE_WEBSERVER
  }
  
  return result;
}

#endif // USE_LIGHT_MOTION
