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
    02/04/2022 - v1.8 - Simplify finite state machine
    05/05/2022 - v1.9 - Switch input to counter
                   
  Input devices should be configured as followed :
   - Counter 1 : connected to the intercom button circuit
   - Relay  1  : connected to external relay in charge of opening the door

  You should setup counter debounce as follow to be on the safe side :
    - CounterDebounce 1000
    - CounterDebounceHigh 50
    - CounterDebounceLow 50
  You may have to adjust settings according to your ring

  To enable Telegram notification in case of door opening, run these commands once in console :
   # tmtoken yourtelegramtoken
   # tmchatid yourtelegramchatid
   # tmstate 1
  When door opens, you'll receive a Telegram notification "DeviceName : Intercom Opened"

  If LittleFS is enabled, settings are stored in /intercom.cfg
  Else, settings are stored using unused energy parameters :
   - Settings->energy_power_calibration   = Global activation timeout (seconds)
   - Settings->energy_kWhtoday            = Number of rings to open
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

#define INTERCOM_LATCH_DURATION_MAX     10      // max latch opening duration (in seconds)
#define INTERCOM_ACTIVE_DURATION_MAX    360     // max duration of active state (in minutes)
#define INTERCOM_JSON_REFRESH           30      // JSON update period when intercom is in active state (in seconds) 

#define INTERCOM_LINE_LENGTH            64

#define D_PAGE_INTERCOM_CONFIG          "/cfg"

#define D_CMND_INTERCOM_STATE           "state"
#define D_CMND_INTERCOM_TIMEOUT         "timeout"
#define D_CMND_INTERCOM_LATCH           "latch"
#define D_CMND_INTERCOM_RING            "ring"

#define D_JSON_INTERCOM                 "Intercom"
#define D_JSON_INTERCOM_TIMEOUT         "Timeout"
#define D_JSON_INTERCOM_LATCH           "Latch"
#define D_JSON_INTERCOM_RING            "Ring"
#define D_JSON_INTERCOM_STATUS          "Status"
#define D_JSON_INTERCOM_STATE           "State"
#define D_JSON_INTERCOM_RING            "Ring"
#define D_JSON_INTERCOM_LABEL           "Label"
#define D_JSON_INTERCOM_TIMELEFT        "TimeLeft"
#define D_JSON_INTERCOM_LASTOPEN        "LastOpen"

#define D_INTERCOM                      "Intercom"
#define D_INTERCOM_DOOR                 "Door"
#define D_INTERCOM_BELL                 "Bell"
#define D_INTERCOM_TACT                 "Activation timeout (mn)"
#define D_INTERCOM_NRING                "Number of rings"
#define D_INTERCOM_LATCH                "Latch opening (sec)"
#define D_INTERCOM_STATE                "State"
#define D_INTERCOM_RING                 "Ring"
#define D_INTERCOM_RINGING              "Ringing"
#define D_INTERCOM_TIMELEFT             "Time left"
#define D_INTERCOM_CONFIG               "Configure"
#define D_INTERCOM_TIMEOUT              "Timeout"
#define D_INTERCOM_ACTIVITY             "Activity log"

#define D_DOOR_DISABLED                 "Disabled"
#define D_DOOR_RINGING                  "Ringing"
#define D_DOOR_OPENED                   "Opened"

#ifdef USE_UFILESYS
#define D_INTERCOM_CFG                  "/intercom.cfg"
#endif    // USE_UFILESYS

// intercom commands
const char kIntercomCommands[] PROGMEM = "ic_" "|" D_CMND_INTERCOM_STATE "|" D_CMND_INTERCOM_TIMEOUT  "|" D_CMND_INTERCOM_LATCH "|" D_CMND_INTERCOM_RING;
void (* const IntercomCommand[])(void) PROGMEM = { &CmndIntercomState, &CmndIntercomActiveTimeout, &CmndIntercomLatchDuration, &CmndIntercomRingNumber };

// intercom states
enum IntercomState { INTERCOM_STATUS_NONE, INTERCOM_STATUS_DISABLE, INTERCOM_STATUS_RING, INTERCOM_STATUS_OPEN, INTERCOM_STATUS_CLOSE, INTERCOM_STATUS_TIMEOUT, INTERCOM_STATUS_MAX };
const char kIntercomStatus[] PROGMEM = "|Disabled|Ring|Opening|Opened|Timeout";              // intercom actions labels

// form topic style
const char INTERCOM_TOPIC_STYLE[] PROGMEM = "style='float:right;font-size:0.7rem;'";

/*************************************************\
 *               Variables
\*************************************************/

// configuration
struct {
  uint8_t  ring_number;             // number of rings before opening
  uint32_t active_timeout;          // timeout when intercom is active (minutes)
  uint8_t  latch_duration;          // door opened latch duration (seconds)
} intercom_config;

// detector status
struct {
  uint8_t  main_state   = INTERCOM_STATUS_DISABLE;    // intercom door opening is disabled
  uint32_t ring_number  = UINT32_MAX;                 // rings counter
  uint32_t time_timeout = UINT32_MAX;                 // time when door opening was enabled (for timeout)
  uint32_t time_changed = UINT32_MAX;                 // time of last state change
  uint32_t time_json    = UINT32_MAX;                 // time of last JSON update
  uint32_t last_open    = 0;                          // time of last door opening
  long     telegram_id  = LONG_MAX;                   // telegram message id
} intercom_status;

/**************************************************\
 *                  Accessors
\**************************************************/

// Validate intercom configuration
void IntercomValidateConfig () 
{
  if ((intercom_config.ring_number  < 1)   || (intercom_config.ring_number    > INTERCOM_RING_MAX))            intercom_config.ring_number    = INTERCOM_RING_MAX;
  if ((intercom_config.active_timeout < 1) || (intercom_config.active_timeout > INTERCOM_ACTIVE_DURATION_MAX)) intercom_config.active_timeout = INTERCOM_ACTIVE_DURATION_MAX;
  if ((intercom_config.latch_duration < 1) || (intercom_config.latch_duration > INTERCOM_LATCH_DURATION_MAX))  intercom_config.latch_duration = INTERCOM_LATCH_DURATION_MAX;
}

// Load intercom configuration
void IntercomLoadConfig () 
{
#ifdef USE_UFILESYS
  // retrieve configuration from flash filesystem
  intercom_config.ring_number    = (uint8_t)UfsCfgLoadKeyInt  (D_INTERCOM_CFG, D_CMND_INTERCOM_RING);
  intercom_config.latch_duration = (uint8_t)UfsCfgLoadKeyInt  (D_INTERCOM_CFG, D_CMND_INTERCOM_LATCH);
  intercom_config.active_timeout = (uint32_t)UfsCfgLoadKeyInt (D_INTERCOM_CFG, D_CMND_INTERCOM_TIMEOUT);

#else
  // retrieve configuration from Settings
  intercom_config.ring_number    = (uint8_t)Settings->energy_kWhtoday;
  intercom_config.active_timeout = (uint32_t)Settings->energy_power_calibration;
  intercom_config.latch_duration = (uint8_t)Settings->energy_current_calibration;
#endif

  // validate configuration values
  IntercomValidateConfig ();
}

// Save intercom configuration 
void IntercomSaveConfig () 
{
  // validate configuration values
  IntercomValidateConfig ();

#ifdef USE_UFILESYS
  // save configuration into flash filesystem
  UfsCfgSaveKeyInt (D_INTERCOM_CFG, D_CMND_INTERCOM_RING,    (int)intercom_config.ring_number,    true);
  UfsCfgSaveKeyInt (D_INTERCOM_CFG, D_CMND_INTERCOM_LATCH,   (int)intercom_config.latch_duration, false);
  UfsCfgSaveKeyInt (D_INTERCOM_CFG, D_CMND_INTERCOM_TIMEOUT, (int)intercom_config.active_timeout, false);

#else
  // save configuration into Settings
  Settings->energy_kWhtoday            = (ulong)intercom_config.ring_number;
  Settings->energy_power_calibration   = (ulong)intercom_config.active_timeout;
  Settings->energy_current_calibration = (ulong)intercom_config.latch_duration;
#endif
}

// Enable intercom opening
void IntercomMainStateLabel (char* pstr_label, size_t size_label)
{
  char str_text[16];
  char str_state[32];

  // handle command
  GetTextIndexed (str_state, sizeof (str_state), intercom_status.main_state, kIntercomStatus);
  switch (intercom_status.main_state)
  {
    case INTERCOM_STATUS_DISABLE:     // door opening disabled
      strlcpy (pstr_label, str_state, size_label);
      break;
    case INTERCOM_STATUS_RING:      // waiting for rings
      IntercomGetTimeLeft (intercom_status.time_timeout, str_text, sizeof (str_text));
      sprintf_P (pstr_label, PSTR ("%s %u/%u (%s)"), str_state, RtcSettings.pulse_counter[0], intercom_config.ring_number, str_text);
      break;
    case INTERCOM_STATUS_OPEN:      // during the door opening command
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
void CmndIntercomState ()
{
  uint8_t action = UINT8_MAX;

  // set activation state
  if (XdrvMailbox.data_len > 0) action = IntercomSetState (XdrvMailbox.payload);

  ResponseCmndNumber (action);
}

// set activation timeout
void CmndIntercomActiveTimeout ()
{ 
  // set value and save config
  if (XdrvMailbox.data_len > 0)
  {
    intercom_config.active_timeout = (uint32_t)XdrvMailbox.payload;
    IntercomSaveConfig ();
  }

  ResponseCmndNumber (intercom_config.active_timeout);
}

// set door latch opening duration
void CmndIntercomLatchDuration ()
{ 
  // set value and save config
  if (XdrvMailbox.data_len > 0)
  {
    intercom_config.latch_duration = (uint8_t)XdrvMailbox.payload;
    IntercomSaveConfig ();
  }
  
  ResponseCmndNumber (intercom_config.latch_duration);
}

// set ring number before door opening
void CmndIntercomRingNumber ()
{ 
  // set value and save config
  if (XdrvMailbox.data_len > 0)
  {
    intercom_config.ring_number = (uint8_t)XdrvMailbox.payload;
    IntercomSaveConfig ();
  }
    
  ResponseCmndNumber (intercom_config.ring_number);
}

/**************************************************\
 *                  Functions
\**************************************************/

// Log event
void IntercomLogEvent (uint8_t action)
{
  char str_text[32];

  // get state label
  GetTextIndexed (str_text, sizeof (str_text), action, kIntercomStatus);

  // handle states transition
  switch (action)
  {
    // waiting for ring
    case INTERCOM_STATUS_DISABLE:
      LogSaveEvent (LOG_EVENT_END, str_text);
      break;

    case INTERCOM_STATUS_RING:
      LogSaveEvent (LOG_EVENT_NEW, str_text);
      break;

    case INTERCOM_STATUS_OPEN:    
      LogSaveEvent (LOG_EVENT_UPDATE, str_text);
      break;

    case INTERCOM_STATUS_CLOSE:    
      LogSaveEvent (LOG_EVENT_END, str_text);
      break;
  }

  // if action performed, log event and ask for JSON update
  if (strlen (str_text) > 0) AddLog (LOG_LEVEL_INFO, str_text);

#ifdef USE_TELEGRAM_EXTENSION
  char str_label[64];

  // send telegram message according to events
  IntercomMainStateLabel (str_text, sizeof (str_text));
  sprintf_P (str_label, PSTR ("*%s*\n%s"), SettingsText (SET_DEVICENAME), str_text);
  switch (action)
  {
    // waiting for ring
    case INTERCOM_STATUS_DISABLE:
      if (intercom_status.telegram_id != LONG_MAX)
      {
        TelegramUpdateMessage (str_label, intercom_status.telegram_id);
        intercom_status.telegram_id = LONG_MAX;
      }
      break;

    case INTERCOM_STATUS_RING:
      if (intercom_status.telegram_id == LONG_MAX) intercom_status.telegram_id = TelegramSendMessage (str_label);
      else TelegramUpdateMessage (str_label, intercom_status.telegram_id);
      break;

    case INTERCOM_STATUS_OPEN:    
      if (intercom_status.telegram_id != LONG_MAX)
      {
        TelegramUpdateMessage (str_label, intercom_status.telegram_id);
        intercom_status.telegram_id = LONG_MAX;
      }
      break;
  }
#endif
}

// Enable intercom door opening
uint8_t IntercomSetState (uint8_t action)
{
  uint32_t timestamp = millis ();

  // handle actions
  switch (action)
  {
    // disable intercom opening
    case INTERCOM_STATUS_DISABLE: 
      // force gate closed
      ExecuteCommandPower (INTERCOM_RELAY, POWER_OFF, SRC_MAX);

      // disable door opening
      intercom_status.main_state   = INTERCOM_STATUS_DISABLE;
      RtcSettings.pulse_counter[0] = 0;
      intercom_status.ring_number  = UINT32_MAX;
      intercom_status.time_changed = timestamp;
      intercom_status.time_timeout = UINT32_MAX;

      // log
      IntercomLogEvent (action);
      break;

    // enable intercom on rings
    case INTERCOM_STATUS_RING: 
      intercom_status.main_state   = INTERCOM_STATUS_RING; 
      RtcSettings.pulse_counter[0] = 0;
      intercom_status.ring_number  = 0;
      intercom_status.time_changed = timestamp;
      intercom_status.time_timeout = timestamp + 60 * 1000 * (uint32_t)intercom_config.active_timeout;

      // log
      IntercomLogEvent (action);
      break;

    // open the gate
    case INTERCOM_STATUS_OPEN:
      // open door latch
      intercom_status.last_open = LocalTime ();
      ExecuteCommandPower (INTERCOM_RELAY, POWER_ON, SRC_MAX);

      // calculate time to release the latch
      intercom_status.main_state   = INTERCOM_STATUS_OPEN; 
      intercom_status.time_changed = timestamp;
      intercom_status.time_timeout = timestamp + 1000 * (uint32_t)intercom_config.latch_duration;

      // log
      IntercomLogEvent (action);
      break;

    // close the gate
    case INTERCOM_STATUS_CLOSE:
      // release door latch
      ExecuteCommandPower (INTERCOM_RELAY, POWER_OFF, SRC_MAX);

      // set to disabled and reset timer
      intercom_status.main_state   = INTERCOM_STATUS_DISABLE; 
      RtcSettings.pulse_counter[0] = 0;
      intercom_status.ring_number  = UINT32_MAX;
      intercom_status.time_changed = timestamp;
      intercom_status.time_timeout = UINT32_MAX;

      // log
      IntercomLogEvent (action);
      break;

    // unknown action
    default:
      action = UINT8_MAX;
      break;
  }

  // update json
  intercom_status.time_json = timestamp;

  return action;
}

// Called just before setting relays
bool IntercomSetDevicePower ()
{
  bool result;

  // if relay command is not coming from intercom module
  result = (XdrvMailbox.payload != SRC_MAX);
  if (result)
  {
    // if system is up and running
    if (TasmotaGlobal.uptime > 5)
    {
      // if relay ON command, enable rings
      if (XdrvMailbox.index == POWER_ON) IntercomSetState (INTERCOM_STATUS_RING);

      // else if relay OFF command, disable rings
      else if (XdrvMailbox.index == POWER_OFF) IntercomSetState (INTERCOM_STATUS_DISABLE);
    }
  }

  return result;
}

// get temporisation left in readable format
void IntercomGetTimeLeft (uint32_t timeout, char* pstr_timeleft, size_t size_timeleft)
{
  uint32_t time_left;
  TIME_T   time_dst;

  // control
  if (pstr_timeleft == nullptr) return;
  if (size_timeleft < 12) return;
  
  // if temporisation is not over
  if (!TimeReached (timeout))
  {
    // convert to readable format
    time_left = (timeout - millis ()) / 1000;
    BreakTime ((uint32_t) time_left, time_dst);

    if (time_dst.hour > 0) sprintf (pstr_timeleft, "%02u:%02u:%02u", time_dst.hour, time_dst.minute, time_dst.second);
    else sprintf (pstr_timeleft, "%02d:%02d", time_dst.minute, time_dst.second);
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
  //   Intercom section    =>   "Intercom":{"Timeout":60,"Ring":3,"Latch":5,"State":1,"Label":"Ring 1/3 (01:28)","LastOpen":"10/12/2021 10:28"}
  // --------------------

  ResponseAppend_P (PSTR ("\"%s\":{"), D_JSON_INTERCOM);

  // parameters
  ResponseAppend_P (PSTR ("\"%s\":%u"),  D_JSON_INTERCOM_TIMEOUT, intercom_config.active_timeout);
  ResponseAppend_P (PSTR (",\"%s\":%u"), D_JSON_INTERCOM_RING,    intercom_config.ring_number);
  ResponseAppend_P (PSTR (",\"%s\":%d"), D_JSON_INTERCOM_LATCH,   intercom_config.latch_duration);

  // state and label
  ResponseAppend_P (PSTR (",\"%s\":%d"), D_JSON_INTERCOM_STATE, intercom_status.main_state);
  IntercomMainStateLabel (str_text, sizeof (str_text));
  ResponseAppend_P (PSTR (",\"%s\":\"%s\""), D_JSON_INTERCOM_LABEL, str_text);

  //  ,"LastOpen":"10/12/2021 10:28:04"
  if (intercom_status.last_open != 0)
  {
    BreakTime (intercom_status.last_open, event_dst);
    ResponseAppend_P (PSTR (",\"%s\":\"%02u/%02u/%04u %02u:%02u:%02u\""), D_JSON_INTERCOM_LASTOPEN, event_dst.day_of_month, event_dst.month, event_dst.year + 1970, event_dst.hour, event_dst.minute, event_dst.second);
  }

  ResponseAppend_P (PSTR ("}"));

  // publish it if not in append mode
  if (!append)
  {
    ResponseAppend_P (PSTR ("}"));
    MqttPublishTeleSensor ();
  } 

#ifdef USE_TELEGRAM_EXTENSION
  char str_label[64];

  // if ringing, send message update
  if (intercom_status.main_state == INTERCOM_STATUS_RING)
  {
    // get context text
    IntercomMainStateLabel (str_text, sizeof (str_label));
    sprintf_P (str_label, PSTR ("*%s*\n%s"), SettingsText (SET_DEVICENAME), str_text);

    // send first message or message update
    if (intercom_status.telegram_id == LONG_MAX) intercom_status.telegram_id = TelegramSendMessage (str_label);
      else TelegramUpdateMessage (str_label, intercom_status.telegram_id);
  }
#endif

  // reset update flag
  intercom_status.time_json = UINT32_MAX;
}

// init function
void IntercomInit ()
{
  // disable fast cycle power recovery (SetOption65)
  Settings->flag3.fast_power_cycle_disable = true;

  // set MQTT message only for switch 1 (intercom ring)
  Settings->switchmode[INTERCOM_SWITCH] = 15;

  // disable reset 1 with button multi-press (SetOption1)
  Settings->flag.button_restrict = 1;

  // load configuration
  IntercomLoadConfig ();

  // set log history parameters
  LogFileSetUnit (UFS_LOG_PERIOD_MONTH);
  LogHistoSetDescription (D_INTERCOM_ACTIVITY, "Date;Time;Duration;Action", 4);
  LogHistoSetDateColumn (true, true, false, false, true);			// start date;start time;stop date;stop time;duration

#ifdef USE_TELEGRAM_EXTENSION
  // init telegram notification
  TelegramExtensionInit ();
#endif
}

// update intercom state every 250 ms
void IntercomEvery250ms ()
{
  // if number of rings increased, publish JSON
  if ((intercom_status.ring_number != UINT32_MAX) && (intercom_status.ring_number != RtcSettings.pulse_counter[0]))
  {
    intercom_status.ring_number = RtcSettings.pulse_counter[0];
    intercom_status.time_json = millis ();
  }

  // handle states transition
  switch (intercom_status.main_state)
  {
    // waiting for rings
    case INTERCOM_STATUS_RING:
      // if number of rings reached, open the gate
      if (RtcSettings.pulse_counter[0] >= intercom_config.ring_number) IntercomSetState (INTERCOM_STATUS_OPEN);
      break;

    // latch is opened
    case INTERCOM_STATUS_OPEN:
      // wait for timeout to release the latch
      if (TimeReached (intercom_status.time_timeout)) IntercomSetState (INTERCOM_STATUS_CLOSE);
      break;
  }

  // if intercom is enabled and global timeout reached, disable intercom
  if ((intercom_status.time_timeout != UINT32_MAX) && TimeReached (intercom_status.time_timeout)) IntercomSetState (INTERCOM_STATUS_DISABLE);

  // if intercom is active and JSON update not planned, set it
  if ((intercom_status.main_state != INTERCOM_STATUS_DISABLE) && (intercom_status.time_json == UINT32_MAX)) intercom_status.time_json = millis () + 1000 * INTERCOM_JSON_REFRESH;

  // if JSON update timeout is reached, publish JSON
  if ((intercom_status.time_json != UINT32_MAX) && TimeReached (intercom_status.time_json)) IntercomShowJSON (false);
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
    WebGetArg (D_CMND_INTERCOM_TIMEOUT, str_argument, CMDSZ);
    if (strlen (str_argument) > 0) intercom_config.active_timeout = (uint32_t)atoi (str_argument);

    // set door latch opening duration according to 'dlatch' parameter
    WebGetArg (D_CMND_INTERCOM_LATCH, str_argument, CMDSZ);
    if (strlen (str_argument) > 0) intercom_config.latch_duration = (uint8_t)atoi (str_argument);

    // set number of rings according to 'nring' parameter
    WebGetArg (D_CMND_INTERCOM_RING, str_argument, CMDSZ);
    if (strlen (str_argument) > 0) intercom_config.ring_number = (uint8_t)atoi (str_argument);

    // save configuration
    IntercomSaveConfig ();
  }

  // beginning of form
  WSContentStart_P (D_INTERCOM_CONFIG);
  WSContentSendStyle ();
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_PAGE_INTERCOM_CONFIG);

  // door
  WSContentSend_P (PSTR ("<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>"), D_INTERCOM_DOOR);
  WSContentSend_P (PSTR ("<p>%s<span %s>%s</span><br><input type='number' name='%s' min='%d' max='%d' step='%d' value='%u'></p>\n"), D_INTERCOM_TACT,   INTERCOM_TOPIC_STYLE, D_CMND_INTERCOM_TIMEOUT,   D_CMND_INTERCOM_TIMEOUT,   5, INTERCOM_ACTIVE_DURATION_MAX, 5, intercom_config.active_timeout);
  WSContentSend_P (PSTR ("<p>%s<span %s>%s</span><br><input type='number' name='%s' min='%d' max='%d' step='%d' value='%u'></p>\n"), D_INTERCOM_LATCH, INTERCOM_TOPIC_STYLE, D_CMND_INTERCOM_LATCH, D_CMND_INTERCOM_LATCH, 1, INTERCOM_LATCH_DURATION_MAX,  1, intercom_config.latch_duration);
  WSContentSend_P (PSTR ("</fieldset></p>\n"));

  // bell
  WSContentSend_P (PSTR ("<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>"), D_INTERCOM_BELL);
  WSContentSend_P (PSTR ("<p>%s<span %s>%s</span><br><input type='number' name='%s' min='%d' max='%d' step='%d' value='%u'></p>\n"), D_INTERCOM_NRING, INTERCOM_TOPIC_STYLE, D_CMND_INTERCOM_RING, D_CMND_INTERCOM_RING, 1, INTERCOM_RING_MAX,          1, intercom_config.ring_number);
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
   case FUNC_SET_DEVICE_POWER:
      result = IntercomSetDevicePower ();
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
