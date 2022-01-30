/*
  xdrv_98_intercom.ino - Building intercom management to automatically open building door after specific number of rings
  
  Copyright (C) 2020  Nicolas Bernaerts
    21/05/2020 - v1.0 - Creation
    26/05/2020 - v1.1 - Add Information JSON page
    28/05/2020 - v1.2 - Define number of rings before opening the door
    30/05/2020 - v1.3 - Separate Intercom and Door in JSON
    20/11/2020 - v1.4 - Tasmota 9.1 compatibility
    01/05/2021 - v1.5 - Add fixed IP and remove use of String to avoid heap fragmentation
    20/10/2021 - v1.6 - Tasmota 10.1 compatibility, commands start with icom_
                        Add LittleFS support
                        Add Telegram notification support
    22/01/2022 - v1.7 - Merge xdrv and xsns to xdrv
                   
  Input devices should be configured as followed :
   - Switch 1 : connected to the intercom button circuit
   - Relay  1 : connected to external relay in charge of opening the door

  To enable Telegram notification in case of door opening, run these commands once in console :
   # tmtoken yourtelegramtoken
   # tmchatid yourtelegramchatid
   # tmstate 1
  When door opens, you'll receive a Telegram notification "DeviceName : Intercom Opened"

  If LittleFS is enabled, settings are stored in /intercom.cfg
  Else, settings are stored using unused energy parameters :
   - Settings->energy_power_calibration   = Global activation timeout (seconds)
   - Settings->energy_kWhtoday            = Number of rings to open
   - Settings->energy_voltage_calibration = Door activation timeout (seconds)
   - Settings->energy_current_calibration = Door opening duration (seconds)

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

#define XDRV_98                         98

#define INTERCOM_SWITCH                 0       // switch 1
#define INTERCOM_RELAY                  1       // relay 1

#define INTERCOM_RING_MAX               5       // max number of rings
#define INTERCOM_RING_DELAY             500     // delay between 2 rings (in ms)

#define INTERCOM_RING_DURATION_MAX      60      // max duration between first and last ring (in seconds)
#define INTERCOM_LATCH_DURATION_MAX     10      // max latch opening duration (in seconds)
#define INTERCOM_ACTIVE_DURATION_MAX    360     // max duration of active state (in minutes)
#define INTERCOM_ACTIVE_UPDATE          5       // JSON update period when intercom is in active state (in seconds) 
#define INTERCOM_FINISHED_DURATION      10      // duration of door closed state at the end of door opening (in seconds)

#define INTERCOM_LINE_LENGTH            64

#define D_PAGE_INTERCOM_CONFIG          "/cfg"

#define D_CMND_INTERCOM_ACTION          "action"
#define D_CMND_INTERCOM_TACT            "timeout"
#define D_CMND_INTERCOM_DRING           "duration"
#define D_CMND_INTERCOM_DLATCH          "latch"
#define D_CMND_INTERCOM_NRING           "ring"

#define D_JSON_INTERCOM                 "Intercom"
#define D_JSON_INTERCOM_ACTION          "Action"
#define D_JSON_INTERCOM_TACT            "Timeout"
#define D_JSON_INTERCOM_DRING           "Ringing"
#define D_JSON_INTERCOM_DLATCH          "Latch"
#define D_JSON_INTERCOM_NRING           "NumRing"
#define D_JSON_INTERCOM_ON              "ON"
#define D_JSON_INTERCOM_OFF             "OFF"
#define D_JSON_INTERCOM_STATUS          "Status"
#define D_JSON_INTERCOM_STATE           "State"
#define D_JSON_INTERCOM_RING            "Ring"
#define D_JSON_INTERCOM_LABEL           "Label"
#define D_JSON_INTERCOM_TIMELEFT        "TimeLeft"
#define D_JSON_INTERCOM_LASTRING        "LastRing"
#define D_JSON_INTERCOM_LASTOPEN        "LastOpen"

#define D_INTERCOM                      "Intercom"
#define D_INTERCOM_DOOR                 "Door"
#define D_INTERCOM_BELL                 "Bell"
#define D_INTERCOM_TACT                 "Activation timeout (mn)"
#define D_INTERCOM_NRING                "Number of rings"
#define D_INTERCOM_DRING                "Max delay for rings (sec)"
#define D_INTERCOM_DLATCH               "Duration of latch opening (sec)"
#define D_INTERCOM_STATE                "State"
#define D_INTERCOM_RING                 "Ring"
#define D_INTERCOM_RINGING              "Ringing"
#define D_INTERCOM_TIMELEFT             "Time left"
#define D_INTERCOM_CONFIG               "Configure"
#define D_INTERCOM_TIMEOUT              "Timeout"
#define D_INTERCOM_CONFIGG              "Pb config"

#define D_DOOR_DISABLED                 "Disabled"
#define D_DOOR_WAITING                  "Waiting"
#define D_DOOR_RINGING                  "Ringing"
#define D_DOOR_OPENED                   "Opened"
#define D_DOOR_CLOSED                   "Closed"

#ifdef USE_UFILESYS
#define D_INTERCOM_CFG                  "/intercom.cfg"
#define D_INTERCOM_LOG_FILE             "/intercom-%02u.csv"
#endif    // USE_UFILESYS

// intercom commands
enum IntercomCommands { CMND_INTERCOM_NONE, CMND_INTERCOM_ACTION, CMND_INTERCOM_TACT, CMND_INTERCOM_DRING, CMND_INTERCOM_DLATCH, CMND_INTERCOM_NRING };
const char kIntercomCommands[] PROGMEM = "ic_" "|" D_CMND_INTERCOM_ACTION "|" D_CMND_INTERCOM_TACT "|" D_CMND_INTERCOM_DRING "|" D_CMND_INTERCOM_DLATCH "|" D_CMND_INTERCOM_NRING;
void (* const IntercomCommand[])(void) PROGMEM = { &CmndIntercomAction, &CmndIntercomActiveTimeout, &CmndIntercomRingingTimeout, &CmndIntercomLatchDuration, &CmndIntercomRingNumber };

// form topic style
const char INTERCOM_TOPIC_STYLE[] PROGMEM = "style='float:right;font-size:0.7rem;'";

/*************************************************\
 *               Variables
\*************************************************/

// intercom main states
enum IntercomAction { INTERCOM_ACTION_DISABLE, INTERCOM_ACTION_OPEN, INTERCOM_ACTION_RING };
enum IntercomState { INTERCOM_STATE_DISABLED, INTERCOM_STATE_WAITING, INTERCOM_STATE_RINGING, INTERCOM_STATE_OPENED, INTERCOM_STATE_CLOSED, INTERCOM_STATE_CONFIG,  INTERCOM_STATE_TIMEOUT, INTERCOM_STATE_MAX };
const char kIntercomState[] PROGMEM = "Disabled|Waiting|Ringing|Opened|Closed|Pb config|Timeout";                               // intercom state labels

// configuration
struct {
  uint8_t  ring_number;             // number of rings before opening
  uint32_t active_timeout;          // timeout when intercom is active (minutes)
  uint32_t ring_timeout;            // timeout of ringing phase (seconds)
  uint8_t  latch_duration;          // door opened latch duration (seconds)
} intercom_config;

// detector status
struct {
  bool          ring_present = false;                     // ring declared as Switch 1
  bool          ring_active  = false;                     // ring is actually ON
  bool          update_json  = false;                     // flag to publish updates
  IntercomState main_state   = INTERCOM_STATE_DISABLED;   // intercom door opening is disabled
  uint8_t       action       = INTERCOM_ACTION_DISABLE;   // last action triggered
  uint8_t       press_count  = UINT8_MAX;                 // number of time ring has been pressed
  uint32_t      press_last   = UINT32_MAX;                // number of time ring has been pressed
  uint32_t      time_enabled = UINT32_MAX;                // time when door opening was enabled (for timeout)
  uint32_t      time_changed = UINT32_MAX;                // time of last state change
  uint32_t      time_json    = UINT32_MAX;                // time of last JSON update
  uint32_t      last_ring    = UINT32_MAX;                // time of last ring
  uint32_t      last_open    = 0;                         // time of last door opening
} intercom_status;

/**************************************************\
 *                  Accessors
\**************************************************/

// Save intercom configuration
void IntercomLoadConfig () 
{
#ifdef USE_UFILESYS
  // retrieve configuration from flash filesystem
  intercom_config.ring_number    = IntercomValidateRingNumber    ((uint8_t)UfsCfgLoadKeyInt  (D_INTERCOM_CFG, D_CMND_INTERCOM_NRING));
  intercom_config.ring_timeout   = IntercomValidateRingTimeout   ((uint32_t)UfsCfgLoadKeyInt (D_INTERCOM_CFG, D_CMND_INTERCOM_DRING));
  intercom_config.latch_duration = IntercomValidateLatchDuration ((uint8_t)UfsCfgLoadKeyInt  (D_INTERCOM_CFG, D_CMND_INTERCOM_DLATCH));
  intercom_config.active_timeout = IntercomValidateActiveTimeout ((uint32_t)UfsCfgLoadKeyInt (D_INTERCOM_CFG, D_CMND_INTERCOM_TACT));
#else
  // retrieve configuration from Settings
  intercom_config.ring_number    = IntercomValidateRingNumber    ((uint8_t)Settings->energy_kWhtoday);
  intercom_config.active_timeout = IntercomValidateActiveTimeout ((uint32_t)Settings->energy_power_calibration);
  intercom_config.ring_timeout   = IntercomValidateRingTimeout  ((uint32_t)Settings->energy_voltage_calibration);
  intercom_config.latch_duration = IntercomValidateLatchDuration ((uint8_t)Settings->energy_current_calibration);
#endif
}

// Load intercom configuration 
void IntercomSaveConfig () 
{
#ifdef USE_UFILESYS
  // save configuration into flash filesystem
  UfsCfgSaveKeyInt (D_INTERCOM_CFG, D_CMND_INTERCOM_NRING,  (int)intercom_config.ring_number,    true);
  UfsCfgSaveKeyInt (D_INTERCOM_CFG, D_CMND_INTERCOM_DRING,  (int)intercom_config.ring_timeout,   false);
  UfsCfgSaveKeyInt (D_INTERCOM_CFG, D_CMND_INTERCOM_DLATCH, (int)intercom_config.latch_duration, false);
  UfsCfgSaveKeyInt (D_INTERCOM_CFG, D_CMND_INTERCOM_TACT,   (int)intercom_config.active_timeout, false);
 #else
  // save configuration into Settings
  Settings->energy_kWhtoday            = (ulong)intercom_config.ring_number;
  Settings->energy_power_calibration   = (ulong)intercom_config.active_timeout;
  Settings->energy_voltage_calibration = (ulong)intercom_config.ring_timeout;
  Settings->energy_current_calibration = (ulong)intercom_config.latch_duration;
#endif
}

// validate number of rings to open the door
uint8_t IntercomValidateRingNumber (uint8_t count)
{
  if ((count == 0) || (count > INTERCOM_RING_MAX)) count = INTERCOM_RING_MAX;
  return count;
}

// validate timeout after intercom enabling (minutes)
uint32_t IntercomValidateActiveTimeout (uint32_t timeout)
{
  if ((timeout == 0) || (timeout > INTERCOM_ACTIVE_DURATION_MAX)) timeout = INTERCOM_ACTIVE_DURATION_MAX;
  return timeout;
}

// validate delay between for door opening timeout (seconds)
uint32_t IntercomValidateRingTimeout (uint32_t timeout)
{
  if ((timeout == 0) || (timeout > INTERCOM_RING_DURATION_MAX)) timeout = INTERCOM_RING_DURATION_MAX;
  return timeout;
}

// validate door opening command duration (seconds)
uint8_t IntercomValidateLatchDuration (uint8_t duration)
{
  if ((duration == 0) || (duration > INTERCOM_LATCH_DURATION_MAX)) duration = INTERCOM_LATCH_DURATION_MAX;
  return duration;
}

// Enable intercom opening
void IntercomMainStateLabel (char* pstr_label, size_t size_label)
{
  char str_state[16];
  char str_text[16];

  // handle command
  GetTextIndexed (str_state, sizeof (str_state), intercom_status.main_state, kIntercomState);
  switch (intercom_status.main_state)
  {
    case INTERCOM_STATE_DISABLED:     // door opening disabled
      strlcpy (pstr_label, str_state, size_label);
      break;
    case INTERCOM_STATE_WAITING:      // waiting for ring
      GetTextIndexed (str_state, sizeof (str_state), intercom_status.main_state, kIntercomState);
      IntercomGetTimeLeft (intercom_status.time_enabled, intercom_config.active_timeout * 60000, str_text, sizeof (str_text));
      sprintf_P (pstr_label, PSTR ("%s (%s)"), str_state, str_text);
      break;
    case INTERCOM_STATE_RINGING:      // actually ringing
      sprintf_P (pstr_label, PSTR ("%s (%u/%u)"), str_state, intercom_status.press_count, intercom_config.ring_number);
      break;
    case INTERCOM_STATE_OPENED:      // during the door opening command
      strlcpy (pstr_label, str_state, size_label);
      break;
    case INTERCOM_STATE_CLOSED:     // door has just been opened and is now closed
      strlcpy (pstr_label, str_state, size_label);
      break;
    default:
      strcpy (pstr_label, "");
      break;
  }
}

/**************************************************\
 *                  Commands
\**************************************************/

// activate / deactivate intercom
void CmndIntercomAction ()
{
  // set activation state
  if (XdrvMailbox.data_len > 0) IntercomAction (XdrvMailbox.payload);

  ResponseCmndNumber (intercom_status.action);
}

// set activation timeout
void CmndIntercomActiveTimeout ()
{ 
  // set value and save config
  if (XdrvMailbox.data_len > 0) intercom_config.active_timeout = IntercomValidateActiveTimeout ((uint32_t)XdrvMailbox.payload);
  IntercomSaveConfig ();

  ResponseCmndNumber (intercom_config.active_timeout);
}

// set ringing timeout
void CmndIntercomRingingTimeout ()
{ 
  // set value and save config
  if (XdrvMailbox.data_len > 0) intercom_config.ring_timeout = IntercomValidateRingTimeout ((uint32_t)XdrvMailbox.payload);
  IntercomSaveConfig ();

  ResponseCmndNumber (intercom_config.ring_timeout);
}

// set door latch opening duration
void CmndIntercomLatchDuration ()
{ 
  // set value and save config
  if (XdrvMailbox.data_len > 0) intercom_config.latch_duration = IntercomValidateLatchDuration ((uint8_t)XdrvMailbox.payload);
  IntercomSaveConfig ();
  
  ResponseCmndNumber (intercom_config.latch_duration);
}

// set ring number before door opening
void CmndIntercomRingNumber ()
{ 
  // set value and save config
  if (XdrvMailbox.data_len > 0) intercom_config.ring_number = IntercomValidateRingNumber ((uint8_t)XdrvMailbox.payload);
  IntercomSaveConfig ();
    
  ResponseCmndNumber (intercom_config.ring_number);
}

/**************************************************\
 *                  Functions
\**************************************************/

// Log event
void IntercomLogEvent (uint8_t state)
{
  char str_state[16];
  char str_line[INTERCOM_LINE_LENGTH];

  // get state label
  GetTextIndexed (str_state, sizeof (str_state), state, kIntercomState);

#ifdef USE_UFILESYS
  uint8_t month;
  TIME_T  event_dst;
  char    str_filename[UFS_FILENAME_SIZE];

  // handle states transition
  switch (intercom_status.main_state)
  {
    // waiting for ring
    case INTERCOM_STATE_DISABLED: 
    case INTERCOM_STATE_WAITING: 
    case INTERCOM_STATE_OPENED:    
    case INTERCOM_STATE_CLOSED:    
      // extract current time data
      BreakTime (LocalTime (), event_dst);

      // if needed, remove file of next month (to keep 11 months maximum)
      if (event_dst.month == 12) month = 1; else month = event_dst.month + 1;
      sprintf_P (str_filename, PSTR (D_INTERCOM_LOG_FILE), month);
      if (!ffsp->exists (str_filename)) ffsp->remove (str_filename);

      // if needed, generate header of current file
      sprintf_P (str_filename, PSTR (D_INTERCOM_LOG_FILE), event_dst.month);
      if (!ffsp->exists (str_filename)) UfsCsvAppend (str_filename, "Month;Day;Time;Event", true);

      // log to littleFS
      sprintf_P (str_line, PSTR ("%02u;%02u;%02u:%02u:%02u;%s"), event_dst.month, event_dst.day_of_month, event_dst.hour, event_dst.minute, event_dst.second, str_state);
      UfsCsvAppend (str_filename, str_line, false);
      break;
  }
#endif

#ifdef USE_TELEGRAM_EXTENSION
  // handle states transition
  switch (intercom_status.main_state)
  {
    // waiting for ring
    case INTERCOM_STATE_DISABLED: 
    case INTERCOM_STATE_WAITING: 
    case INTERCOM_STATE_OPENED:    
      // generate telegram message
      sprintf_P (str_line, PSTR ("*%s*\nIntercom %s"), SettingsText (SET_DEVICENAME), str_state);
      TelegramSendMessage (str_line);
      break;
  }
#endif

  // log event
  AddLog (LOG_LEVEL_INFO, str_state);
}

// Enable intercom door opening
void IntercomAction (uint8_t action)
{
  uint32_t timestamp  = millis ();

  switch (action)
  {
    // disable intercom opening
    case INTERCOM_ACTION_DISABLE: 
      // disable door opening
      intercom_status.action       = action;
      intercom_status.main_state   = INTERCOM_STATE_DISABLED;
      intercom_status.press_count  = UINT8_MAX;
      intercom_status.press_last   = UINT32_MAX;
      intercom_status.time_enabled = UINT32_MAX;
      intercom_status.time_changed = timestamp;
      intercom_status.update_json  = true;

      // force gate closed
      ExecuteCommandPower (INTERCOM_RELAY, POWER_OFF, SRC_MAX);

      // log
      IntercomLogEvent (INTERCOM_STATE_DISABLED);
      break;

    // force intercom opening
    case INTERCOM_ACTION_OPEN: 
      intercom_status.action       = action;
      intercom_status.main_state   = INTERCOM_STATE_RINGING; 
      intercom_status.press_count  = intercom_config.ring_number;
      intercom_status.press_last   = timestamp;
      intercom_status.time_enabled = timestamp;
      intercom_status.time_changed = timestamp;
      intercom_status.update_json  = true;
      break;

    // enable intercom on rings
    case INTERCOM_ACTION_RING: 
      if (intercom_status.ring_present)
      {
        intercom_status.action       = action;
        intercom_status.main_state   = INTERCOM_STATE_WAITING; 
        intercom_status.press_count  = 0;
        intercom_status.press_last   = UINT32_MAX;
        intercom_status.time_enabled = timestamp;
        intercom_status.time_changed = timestamp;
        intercom_status.update_json  = true;

        // log
        IntercomLogEvent (INTERCOM_STATE_WAITING);
      }

      // else no ring declared
      else IntercomLogEvent (INTERCOM_STATE_CONFIG);
      break;
  }
}

// Toggle intercom door opening
void IntercomToggleState ()
{
  if (intercom_status.main_state == INTERCOM_STATE_DISABLED) IntercomAction (INTERCOM_ACTION_RING);
  else IntercomAction (INTERCOM_ACTION_DISABLE);
}

// get temporisation left in readable format
void IntercomGetTimeLeft (uint32_t time_start, uint32_t timeout, char* pstr_timeleft, size_t size_timeleft)
{
  TIME_T   time_dst;
  uint32_t time_left;

  // control
  if (pstr_timeleft == nullptr) return;
  if (size_timeleft < 8) return;
  
  // if temporisation is not over
  time_left = time_start + timeout;
  if (!TimeReached (time_left))
  {
    // convert to readable format
    time_left = (time_start + timeout - millis ()) / 1000;
    BreakTime ((uint32_t) time_left, time_dst);

    if (size_timeleft >= 8) sprintf (pstr_timeleft, "%02d:%02d", time_dst.minute, time_dst.second);
  }
  else strcpy (pstr_timeleft, "---");
}

// Show JSON status (for MQTT)
void IntercomShowJSON (bool append)
{
  uint32_t value;
  char     str_text[32];
  TIME_T   event_dst;

  // add , in append mode or { in direct publish mode
  if (append) ResponseAppend_P (PSTR (",")); else Response_P (PSTR ("{"));

  // --------------------
  //   Intercom section    =>   "Intercom":{"Timeout":60,"Ringing":3,"NumRing":3,"Latch":5}
  // --------------------

  ResponseAppend_P (PSTR ("\"%s\":{"), D_JSON_INTERCOM);

  // parameters
  ResponseAppend_P (PSTR (",\"%s\":%u"), D_JSON_INTERCOM_TACT,   intercom_config.active_timeout);
  ResponseAppend_P (PSTR (",\"%s\":%u"), D_JSON_INTERCOM_DRING,  intercom_config.ring_timeout);
  ResponseAppend_P (PSTR (",\"%s\":%u"), D_JSON_INTERCOM_NRING,  intercom_config.ring_number);
  ResponseAppend_P (PSTR (",\"%s\":%d"), D_JSON_INTERCOM_DLATCH, intercom_config.latch_duration);

  ResponseAppend_P (PSTR ("}"));

  // ----------------
  //   Status section     =>    "Status":{"Action":0;"Ring":0;"State":1,"Label":"Waiting","TimeLeft":"02:18","LastRing":"10/12/2021 10:27","LastOpen":"10/12/2021 10:28"}
  // -----------------

  ResponseAppend_P (PSTR (",\"%s\":{"), D_JSON_INTERCOM_STATUS);

  // action 
  ResponseAppend_P (PSTR ("\"%s\":%d"), D_JSON_INTERCOM_ACTION, intercom_status.action);

  // ring 
  ResponseAppend_P (PSTR (",\"%s\":%d"), D_JSON_INTERCOM_RING, intercom_status.ring_active);

  // state and label
  ResponseAppend_P (PSTR (",\"%s\":%d"), D_JSON_INTERCOM_STATE, intercom_status.main_state);
  IntercomMainStateLabel (str_text, sizeof (str_text));
  ResponseAppend_P (PSTR (",\"%s\":\"%s\""), D_JSON_INTERCOM_LABEL, str_text);

  //  ,"TimeLeft":"58:12"
  if (intercom_status.action == INTERCOM_ACTION_RING)
  {
    IntercomGetTimeLeft (intercom_status.time_enabled, intercom_config.active_timeout * 60000, str_text, sizeof (str_text));
    ResponseAppend_P (PSTR (",\"%s\":\"%s\""), D_JSON_INTERCOM_TIMELEFT, str_text);
  }

  //  ,"LastRing":"10/12/2021 10:27:14"
  if (intercom_status.last_ring != UINT32_MAX)
  {
    BreakTime (intercom_status.last_ring, event_dst);
    ResponseAppend_P (PSTR (",\"%s\":\"%02u/%02u/%04u %02u:%02u:%02u\""), D_JSON_INTERCOM_LASTRING, event_dst.day_of_month, event_dst.month, event_dst.year + 1970, event_dst.hour, event_dst.minute, event_dst.second);
  }

  //  ,"LastOpen":"10/12/2021 10:28:04"
  if (intercom_status.last_open != UINT32_MAX)
  {
    BreakTime (intercom_status.last_open, event_dst);
    ResponseAppend_P (PSTR (",\"%s\":\"%02u/%02u/%04u %02u:%02u:%02u\""), D_JSON_INTERCOM_LASTOPEN, event_dst.day_of_month, event_dst.month, event_dst.year + 1970, event_dst.hour, event_dst.minute, event_dst.second);
  }

  ResponseAppend_P (PSTR ("}"));

  // publish it if not in append mode
  if (!append)
  {
    ResponseAppend_P (PSTR ("}"));
    MqttPublishPrefixTopic_P (TELE, PSTR (D_RSLT_SENSOR));
  } 

  // reset updated flag
  intercom_status.update_json = false;
  intercom_status.time_json   = millis ();
}

// init function
void IntercomInit ()
{
  // disable JSON for Timezone module
  TimezoneEnableJSON (false);

  // disable JSON for IP address module
  IPAddressEnableJSON (false);

  // set MQTT message only for switch 1 (intercom ring)
  Settings->switchmode[INTERCOM_SWITCH] = 15;

  // disable reset 1 with button multi-press (SetOption1)
  Settings->flag.button_restrict = 1;

  // init ring declaration
  intercom_status.ring_present = PinUsed (GPIO_SWT1, 0);

  // load configuration
  IntercomLoadConfig ();

#ifdef USE_TELEGRAM_EXTENSION
  // init telegram notification
  TelegramExtensionInit ();
#endif
}

// update intercom state every 250 ms
void IntercomEvery250ms ()
{
  bool          ring_active = false;      // ring currently active
  bool          ring_new    = false;      // ring newly pressed
  IntercomState prev_state;               // intercom previous state

  // save previous state
  prev_state = intercom_status.main_state;

  // detect ring status change
  if (intercom_status.ring_present)
  {
    ring_active = !Switch.virtual_state[INTERCOM_SWITCH];
    if (ring_active != intercom_status.ring_active)
    {
      if (ring_active) ring_new = true;
      intercom_status.ring_active = ring_active;
      intercom_status.update_json = true;
    }
  }

  // if rings detection is active
  if (intercom_status.press_count != UINT8_MAX)
  {
    // if timeout reached between rings, reset number of rings
    if (TimeReached (intercom_status.press_last + INTERCOM_RING_DELAY)) intercom_status.press_count = 0;

    // if new ring detected
    if (ring_new)
    {
      // increment ring count
      intercom_status.press_count++;
       
      // update last button pressed timestamp
      intercom_status.press_last = millis ();
      intercom_status.last_ring  = LocalTime ();

      // log event
      IntercomLogEvent (INTERCOM_STATE_RINGING);
    }
  }

  // if intercom is enabled, check for global timeout
  if (intercom_status.time_enabled != UINT32_MAX)
  {
    // if global timeout is reached, disable intercom
    if (TimeReached (intercom_status.time_enabled + (intercom_config.active_timeout * 60000)))
    {
      // disable intercom and log event
      IntercomAction (INTERCOM_ACTION_DISABLE);
      IntercomLogEvent (INTERCOM_STATE_TIMEOUT);
    }

    // if JSON update timeout is reached, force update
    if (TimeReached (intercom_status.time_json + (1000 * INTERCOM_ACTIVE_UPDATE)))
      intercom_status.update_json = true;
  }

  // handle states transition
  switch (intercom_status.main_state)
  {
    // waiting for ring
    case INTERCOM_STATE_WAITING: 
      // intercom button has been pressed once
      if (intercom_status.press_count > 0) intercom_status.main_state = INTERCOM_STATE_RINGING;
      break;

    // ringing
    case INTERCOM_STATE_RINGING:       
      // if timeout is reached, switch back to waiting state
      if (TimeReached (intercom_status.time_changed + (1000 * intercom_config.ring_timeout))) intercom_status.press_count = 0;

      // else, if number of rings reached, open the gate
      else if (intercom_status.press_count >= intercom_config.ring_number)
      {
        // update status
        intercom_status.main_state = INTERCOM_STATE_OPENED;

        // open door latch and log event
        ExecuteCommandPower (INTERCOM_RELAY, POWER_ON, SRC_MAX);
        IntercomLogEvent (INTERCOM_STATE_OPENED);
      }
      break;

    // latch is opened
    case INTERCOM_STATE_OPENED:    
      // wait for door opening timeout to release the command
      if (TimeReached (intercom_status.time_changed + (1000 * intercom_config.latch_duration)))
      {
        // release door latch
        ExecuteCommandPower (INTERCOM_RELAY, POWER_OFF, SRC_MAX);

        // update status and log event
        intercom_status.last_open  = LocalTime ();
        intercom_status.main_state = INTERCOM_STATE_CLOSED;
        IntercomLogEvent (INTERCOM_STATE_CLOSED);
      }
      break;

    // door opening is over, disable intercom
    case INTERCOM_STATE_CLOSED:    
      // if end of opening delay is reached, disable intercom
      if (TimeReached (intercom_status.time_changed + (1000 * INTERCOM_FINISHED_DURATION))) IntercomAction (INTERCOM_ACTION_DISABLE);
      break;
  }

  // if state changed, reset current state
  if (prev_state != intercom_status.main_state)
  {
    intercom_status.time_changed = millis ();
    intercom_status.update_json  = true;
  }

  // if some important data have been updated, publish JSON
  if (intercom_status.update_json) IntercomShowJSON (false);
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
void IntercomWebSensor ()
{
  char str_label[32];

  // ring status
  if (intercom_status.ring_active) strcpy (str_label, MQTT_STATUS_ON);
  else strcpy (str_label, MQTT_STATUS_OFF);
  WSContentSend_PD (PSTR ("{s}%s{m}%s{e}"), D_INTERCOM_RING, str_label);

  // intercom status
  IntercomMainStateLabel (str_label, sizeof (str_label));
  WSContentSend_PD (PSTR ("{s}%s{m}%s{e}"), D_INTERCOM_STATE, str_label);
}

// Motion config web page
void IntercomWebPageConfigure ()
{
  uint32_t value;
  char     str_argument[CMDSZ];
  
  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess ()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg ("save"))
  {
    // set activation timeout according to 'tact' parameter
    WebGetArg (D_CMND_INTERCOM_TACT, str_argument, CMDSZ);
    if (strlen (str_argument) > 0) intercom_config.active_timeout = IntercomValidateActiveTimeout ((uint32_t)atoi (str_argument));

    // set delay between 2 rings according to 'dring' parameter
    WebGetArg (D_CMND_INTERCOM_DRING, str_argument, CMDSZ);
    if (strlen (str_argument) > 0) intercom_config.ring_timeout = IntercomValidateRingTimeout ((uint32_t)atoi (str_argument));
    
    // set door latch opening duration according to 'dlatch' parameter
    WebGetArg (D_CMND_INTERCOM_DLATCH, str_argument, CMDSZ);
    if (strlen (str_argument) > 0) intercom_config.latch_duration = IntercomValidateLatchDuration ((uint8_t)atoi (str_argument));

    // set number of rings according to 'nring' parameter
    WebGetArg (D_CMND_INTERCOM_NRING, str_argument, CMDSZ);
    if (strlen (str_argument) > 0) intercom_config.ring_number = IntercomValidateRingNumber ((uint8_t)atoi (str_argument));

    // save configuration
    IntercomSaveConfig ();
  }

  // beginning of form
  WSContentStart_P (D_INTERCOM_CONFIG);
  WSContentSendStyle ();
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_PAGE_INTERCOM_CONFIG);

  // door
  WSContentSend_P (PSTR ("<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>"), D_INTERCOM_DOOR);
  WSContentSend_P (PSTR ("<p>%s<span %s>%s</span><br><input type='number' name='%s' min='%d' max='%d' step='%d' value='%u'></p>\n"), D_INTERCOM_TACT,   INTERCOM_TOPIC_STYLE, D_CMND_INTERCOM_TACT,   D_CMND_INTERCOM_TACT,   5, INTERCOM_ACTIVE_DURATION_MAX, 5, intercom_config.active_timeout);
  WSContentSend_P (PSTR ("<p>%s<span %s>%s</span><br><input type='number' name='%s' min='%d' max='%d' step='%d' value='%u'></p>\n"), D_INTERCOM_DLATCH, INTERCOM_TOPIC_STYLE, D_CMND_INTERCOM_DLATCH, D_CMND_INTERCOM_DLATCH, 1, INTERCOM_LATCH_DURATION_MAX,  1, intercom_config.latch_duration);
  WSContentSend_P (PSTR ("</fieldset></p>\n"));

  // bell
  WSContentSend_P (PSTR ("<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>"), D_INTERCOM_BELL);
  WSContentSend_P (PSTR ("<p>%s<span %s>%s</span><br><input type='number' name='%s' min='%d' max='%d' step='%d' value='%u'></p>\n"), D_INTERCOM_DRING, INTERCOM_TOPIC_STYLE, D_CMND_INTERCOM_DRING, D_CMND_INTERCOM_DRING, 1, INTERCOM_RING_DURATION_MAX, 1, intercom_config.ring_timeout);
  WSContentSend_P (PSTR ("<p>%s<span %s>%s</span><br><input type='number' name='%s' min='%d' max='%d' step='%d' value='%u'></p>\n"), D_INTERCOM_NRING, INTERCOM_TOPIC_STYLE, D_CMND_INTERCOM_NRING, D_CMND_INTERCOM_NRING, 1, INTERCOM_RING_MAX,          1, intercom_config.ring_number);
  WSContentSend_P (PSTR ("</fieldset></p>\n"));

  // save button  
  // --------------
  WSContentSend_P (PSTR ("<p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), D_SAVE);
  WSContentSend_P (PSTR ("</form>\n"));

  // configuration button and end of page
  WSContentSpaceButton (BUTTON_CONFIGURATION);
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
    case FUNC_INIT:
      IntercomInit ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kIntercomCommands, IntercomCommand);
      break;
    case FUNC_EVERY_250_MSECOND:
      IntercomEvery250ms ();
      break;
    case FUNC_JSON_APPEND:
      IntercomShowJSON (true);
      break;
#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      Webserver->on (D_PAGE_INTERCOM_CONFIG, IntercomWebPageConfigure);
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
