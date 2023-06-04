/*
  xdrv_96_offload.ino - Device offloading thru MQTT instant and max power
  
  Copyright (C) 2020  Nicolas Bernaerts
    23/03/2020 - v1.0  - Creation
    26/05/2020 - v1.1  - Add Information JSON page
    07/07/2020 - v1.2  - Enable discovery (mDNS)
    20/07/2020 - v1.3  - Change offloading delays to seconds
                         Update instant device power in case of Sonoff energy module
    05/08/2020 - v1.4  - Add /control page to have a public switch
                         If available, get max power thru MQTT meter
                         Phase selection and disable mDNS 
                         Add restart after offload configuration
                         Correct display exception on mainpage
    22/08/2020 - v1.5  - Save offload config using new Settings text
                         Add restart after offload configuration
    15/09/2020 - v1.6    Add OffloadJustSet and OffloadJustRemoved
    19/09/2020 - v2.0  - Add Contract power adjustment in %
                         Set offload priorities as standard options
                         Add icons to /control page
    15/10/2020 - v2.1  - Expose icons on web server
    16/10/2020 - v2.2  - Handle priorities as list of device types
                         Add randomisation to reconnexion
    23/10/2020 - v2.3  - Update control page in real time
    05/11/2020 - v2.4  - Tasmota 9.0 compatibility
    11/11/2020 - v2.5  - Add offload history pages (/histo and /histo.json)
    23/04/2021 - v3.0  - Add fixed IP and remove use of String to avoid heap fragmentation
    22/09/2021 - v3.1  - Add LittleFS support to store offload events
    17/01/2022 - v3.2  - Use device type priority to handle delays
    08/04/2022 - v9.1  - Switch from icons to emojis
    08/06/2022 - v9.2  - Add auto-rearm capability
    09/02/2023 - v10.2 - Disable wifi sleep to avoid latency
    12/05/2023 - v10.3 - Save history in Settings strings
    12/05/2023 - v10.4 - Change auto-rearm to auto-on and auto-off 

  Settings are stored using free_f63 parameters :
    - Settings->free_f63[0] = Device type (0 ...)
    - Settings->free_f63[1] = Device power (0 ... 25.5 kVA in 100VA steps)
    - Settings->free_f63[2] = Device priority (0 ... 32)
    - Settings->free_f63[3] = Contract power (0 ... 256kW in 1000W steps)
    - Settings->free_f63[4] = Contract adjustment in % (0 ... 200)
    - Settings->free_f63[5] = Auto-on delay (0 ... 1200s in 5s steps)
                              0 means no auto-on
    - Settings->free_f63[6] = Auto-off delay (0 ... 1200s in 5s steps)
                              0 means no auto-off
  Text settings are stored using new SettingsTextIndex defined in <tasmota.h> :
    * SET_OFFLOAD_TOPIC
    * SET_OFFLOAD_KEY_INST
    * SET_OFFLOAD_KEY_MAX

  If LittleFS partition is available device icon should be stored as logo.png or logo.jpg

  Use ol_help command to list available commands
  
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
 *                  Offload
\*************************************************/

#ifdef USE_OFFLOAD

#define XDRV_96                 96

#include <ArduinoJson.h>

#define OFFLOAD_PHASE_MAX       3
#define OFFLOAD_POWER_MAX       10000
#define OFFLOAD_POWER_MINIMUM   20                // minimum power to consider that device is active
#define OFFLOAD_DELAY_MAX       1000
#define OFFLOAD_PRIORITY_MAX    10
#define OFFLOAD_POWER_UPDATE    300               // update peak power every 5 minutes

#define OFFLOAD_EVENT_MAX       10

#define D_PAGE_OFFLOAD_CONFIG    "offload"
#define D_PAGE_OFFLOAD_CONTROL   "control"
#define D_PAGE_OFFLOAD_HISTORY   "history"
#define D_PAGE_OFFLOAD_UPDATE    "control.upd"

#define D_CMND_OFFLOAD_PREFIX    "ol_"
#define D_CMND_OFFLOAD_HELP      "help"
#define D_CMND_OFFLOAD_POWER     "power"
#define D_CMND_OFFLOAD_TYPE      "type"
#define D_CMND_OFFLOAD_PRIORITY  "prio"
#define D_CMND_OFFLOAD_MAX       "max"
#define D_CMND_OFFLOAD_ADJUST    "adj"
#define D_CMND_OFFLOAD_TOPIC     "topic"
#define D_CMND_OFFLOAD_KINST     "kinst"
#define D_CMND_OFFLOAD_KMAX      "kmax"
#define D_CMND_OFFLOAD_AUTO_ON   "on"
#define D_CMND_OFFLOAD_AUTO_OFF  "off"

#define D_CMND_OFFLOAD_CHOICE    "choice"

#define D_JSON_OFFLOAD           "Offload"
#define D_JSON_OFFLOAD_STATE     "State"
#define D_JSON_OFFLOAD_STAGE     "Stage"
#define D_JSON_OFFLOAD_POWER     "Power"
#define D_JSON_OFFLOAD_CONTRACT  "Contract"
#define D_JSON_OFFLOAD_TOPIC     "Topic"
#define D_JSON_OFFLOAD_KEY_INST  "KeyInst"
#define D_JSON_OFFLOAD_KEY_MAX   "KeyMax"

#define D_OFFLOAD                "Offload"
#define D_OFFLOAD_CONFIGURE      "Configure"
#define D_OFFLOAD_CONTROL        "Control"
#define D_OFFLOAD_HISTORY        "History"
#define D_OFFLOAD_DEVICE         "Device"
#define D_OFFLOAD_TYPE           "Type"
#define D_OFFLOAD_ADJUST         "Adjust"
#define D_OFFLOAD_MAX            "Max"
#define D_OFFLOAD_METER          "Meter"
#define D_OFFLOAD_TOPIC          "MQTT Topic"
#define D_OFFLOAD_KEY_INST       "JSON Power Key"
#define D_OFFLOAD_KEY_MAX        "JSON Contract Key"
#define D_OFFLOAD_POWER          "Power"
#define D_OFFLOAD_PRIORITY       "Priority"
#define D_OFFLOAD_REARM          "Auto rearm"

#ifdef USE_UFILESYS
const char D_OFFLOAD_FILE_YEAR[] PROGMEM = "/offload-year-%04u.log";    // offload history file
#endif    // USE_UFILESYS

#define D_OFFLOAD_ICON_LOGO      "logo"    

// offloading commands
const char kOffloadCommands[] PROGMEM = D_CMND_OFFLOAD_PREFIX "|" D_CMND_OFFLOAD_HELP "|" D_CMND_OFFLOAD_TYPE "|" D_CMND_OFFLOAD_PRIORITY "|" D_CMND_OFFLOAD_POWER  "|" D_CMND_OFFLOAD_MAX "|" D_CMND_OFFLOAD_ADJUST "|" D_CMND_OFFLOAD_TOPIC "|" D_CMND_OFFLOAD_KINST "|" D_CMND_OFFLOAD_KMAX "|" D_CMND_OFFLOAD_AUTO_ON "|" D_CMND_OFFLOAD_AUTO_OFF;
void (* const OffloadCommand[])(void) PROGMEM = { &CmndOffloadHelp, &CmndOffloadType, &CmndOffloadPriority, &CmndOffloadPower, &CmndOffloadMax, &CmndOffloadAdjust, &CmndOffloadPowerTopic, &CmndOffloadPowerKeyInst, &CmndOffloadPowerKeyMax, &CmndOffloadDelayAutoOn, &CmndOffloadDelayAutoOff };
 
// strings
const char OFFLOAD_FIELDSET_START[]      PROGMEM = "<fieldset><legend><b>&nbsp;%s %s&nbsp;</b></legend>\n";
const char OFFLOAD_FIELDSET_STOP[]       PROGMEM = "</fieldset>\n";
const char OFFLOAD_INPUT_NUMBER[]        PROGMEM = "<p>%s<span class='key'>%s%s</span><br><input type='number' name='%s' id='%s' min='%d' max='%d' step='%d' value='%d'></p>\n";
const char OFFLOAD_INPUT_NUMBER_HALF[]   PROGMEM = "<p class='half'>%s<span class='key'>%s%s</span><br><input type='number' name='%s' id='%s' min='%d' max='%d' step='%d' value='%d'></p>\n";
const char OFFLOAD_INPUT_NUMBER_SWITCH[] PROGMEM = "<p class='half'>%s<span class='key'>%s%s</span><br><input class='switch' type='number' name='%s' id='%s' min='%d' max='%d' step='%d' value='%d'></p>\n";
const char OFFLOAD_INPUT_TEXT[]          PROGMEM = "<p>%s<span class='key'>%s%s</span><br><input name='%s' value='%s' placeholder='%s'></p>\n";
//const char OFFLOAD_BUTTON[]              PROGMEM = "<p><form action='%s' method='get'><button>%s</button></form></p>";

// definition of types of device with associated delays
enum OffloadDevice                     { OFFLOAD_DEVICE_APPLIANCE, OFFLOAD_DEVICE_LIGHT, OFFLOAD_DEVICE_FRIDGE, OFFLOAD_DEVICE_WASHING, OFFLOAD_DEVICE_DISH, OFFLOAD_DEVICE_DRIER, OFFLOAD_DEVICE_CUMULUS, OFFLOAD_DEVICE_IRON, OFFLOAD_DEVICE_BATHROOM, OFFLOAD_DEVICE_OFFICE, OFFLOAD_DEVICE_LIVING, OFFLOAD_DEVICE_ROOM, OFFLOAD_DEVICE_KITCHEN, OFFLOAD_DEVICE_MAX };
const uint8_t arr_offload_priority[] = { 3,                        1,                    1,                     4,                      4,                   5,                    5,                      2,                   6,                       7,                     8,                     9,                   10  };
const uint16_t arr_offload_after[]   = { 10,                       5,                    30,                    60,                     60,                  60,                   120,                    5,                   120,                     180,                   240,                   300,                 360 };
const char kOffloadDevice[] PROGMEM  = "Misc appliance|Light|Fridge|Washing machine|Dish washer|Drier|Cumulus|Iron|Bathroom|Office|Living room|Sleeping room|Kitchen";               // labels
const char kOffloadIcon[]   PROGMEM  = "🔌|💡|❄️|👕|🍽️|🌀|💧|🧺|🔥|🔥|🔥|🔥|🔥";                                                                      // icons

// offloading stages
enum OffloadStages { OFFLOAD_STAGE_NONE, OFFLOAD_STAGE_BEFORE, OFFLOAD_STAGE_ACTIVE, OFFLOAD_STAGE_AFTER,  OFFLOAD_STAGE_MAX };
const char kOffloadStageColor[] PROGMEM  = "#2ae|#fa0|#d11|#fa0|";                                                                       // colors of offload stages
const char kOffloadStageLabel[] PROGMEM  = "|Offload starts in %d sec.|Offload Active|Offload ends in %d sec.|";                       // labels of offload stages

/*************************************************\
 *               Variables
\*************************************************/

// variables
struct {
  uint8_t  type            = OFFLOAD_DEVICE_APPLIANCE;    // device type
  uint8_t  arr_type[OFFLOAD_DEVICE_MAX];                  // array of possible device types
  uint8_t  nbr_type        = 0;                           // number of possible device types
  uint8_t  priority        = 0;                           // device priority
  uint8_t  contract_adjust = 0;                           // adjustement of maximum power in %
  uint16_t contract_power  = 0;                           // maximum power limit before offload
  uint16_t device_power    = 0;                           // maximum power of device
  uint16_t delay_auto_on   = 0;                           // delay before automatically switching ON the relay (in sec.)
  uint16_t delay_auto_off  = 0;                           // delay before automatically switching OFF the relay (in sec.)
  String   str_topic;                                     // mqtt topic to be used for meter
  String   str_kinst;                                     // mqtt instant apparent power key
  String   str_kmax;                                      // mqtt maximum apparent power key
} offload_config;

struct {
  bool     sensor           = false;                      // flag to define if power sensor is available
  bool     managed          = true;                       // flag to define if relay is managed directly
  bool     changed          = false;                      // flag to signal that offload state has changed
  uint8_t  event_idx        = UINT8_MAX;                  // index of current event in settings  
  uint8_t  stage            = OFFLOAD_STAGE_NONE;         // current offloading state
  uint8_t  relay            = 0;                          // relay state before offloading
  uint16_t pinst            = 0;                          // device instant power (mesured thru sensor)
  uint16_t total_pinst      = 0;                          // actual phase instant power (retrieved thru MQTT)
  uint32_t time_message     = 0;                          // time of last power message
  uint32_t time_next        = UINT32_MAX;                 // time of next stage
  uint32_t time_json        = UINT32_MAX;                 // time of JSON update
  uint32_t time_auto_on     = UINT32_MAX;                 // time when relay should be automatically switched ON
  uint32_t time_auto_off    = UINT32_MAX;                 // time when relay should be automatically switched OFF
  uint16_t delay_before     = 0;                          // delay in seconds before effective offloading
  uint16_t delay_after      = 0;                          // delay in seconds before removing offload
} offload_status;

/**************************************************\
 *                  Accessors
\**************************************************/

// load config parameters
void OffloadLoadConfig ()
{
  // retrieve saved settings from flash memory
  OffloadValidateDeviceType (Settings->free_f63[0]);
  offload_config.device_power    = (uint16_t)Settings->free_f63[1] * 50;
  offload_config.priority        = Settings->free_f63[2];
  offload_config.contract_power  = (uint16_t)Settings->free_f63[3] * 1000;
  offload_config.contract_adjust = Settings->free_f63[4];
  offload_config.delay_auto_on   = (uint16_t)Settings->free_f63[5] * 5;
  offload_config.delay_auto_off  = (uint16_t)Settings->free_f63[6] * 5;

  // mqtt config
  offload_config.str_topic = SettingsText (SET_OFFLOAD_TOPIC);
  offload_config.str_kinst = SettingsText (SET_OFFLOAD_KEY_INST);
  offload_config.str_kmax  = SettingsText (SET_OFFLOAD_KEY_MAX);

  // check for out of range values
  if (offload_config.device_power > OFFLOAD_POWER_MAX) offload_config.device_power = 0;
  if (offload_config.contract_power > OFFLOAD_POWER_MAX) offload_config.contract_power = 0;
  if (offload_config.priority > OFFLOAD_PRIORITY_MAX) offload_config.priority = OFFLOAD_PRIORITY_MAX;
  if (offload_config.contract_adjust == 0)  offload_config.contract_adjust = 100;
  if (offload_config.contract_adjust > 200) offload_config.contract_adjust = 100;

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("OFL: Loaded configuration"));
}

// save config parameters
void OffloadSaveConfig ()
{
  // save settings to flash memory
  Settings->free_f63[0] = offload_config.type;
  Settings->free_f63[1] = (uint8_t)(offload_config.device_power / 50);
  Settings->free_f63[2] = offload_config.priority;
  Settings->free_f63[3] = (uint8_t)(offload_config.contract_power / 1000);
  Settings->free_f63[4] = offload_config.contract_adjust;
  Settings->free_f63[5] = (uint8_t)(offload_config.delay_auto_on / 5);
  Settings->free_f63[6] = (uint8_t)(offload_config.delay_auto_off / 5);

  // mqtt config
  SettingsUpdateText (SET_OFFLOAD_TOPIC,    offload_config.str_topic.c_str ());
  SettingsUpdateText (SET_OFFLOAD_KEY_INST, offload_config.str_kinst.c_str ());
  SettingsUpdateText (SET_OFFLOAD_KEY_MAX,  offload_config.str_kmax.c_str ());
}

/**************************************************\
 *                  Commands
\**************************************************/

// offload help
void CmndOffloadHelp ()
{
  uint8_t index;
  char    str_text[32];

  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Offload commands :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ol_power = device power (W)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ol_type  = device type"));
  for (index = 0; index < OFFLOAD_DEVICE_MAX; index++)
  {
    GetTextIndexed (str_text, sizeof (str_text), index, kOffloadDevice);
    AddLog (LOG_LEVEL_INFO, PSTR ("     %u - %s"), index, str_text);
  }
  AddLog (LOG_LEVEL_INFO, PSTR (" - ol_prio  = device priority (1 max ... 10 min)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ol_max   = maximum acceptable contract power (W)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ol_adj   = contract maximum power adjustment (100% = contract)"));

  AddLog (LOG_LEVEL_INFO, PSTR (" - ol_topic = MQTT topic of the meter"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ol_kmax  = MQTT key for contract power"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ol_kinst = MQTT key for current meter instant power"));

  AddLog (LOG_LEVEL_INFO, PSTR (" - ol_on = Set auto switch ON delay (in sec., 0 no rearm)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ol_off = Set auto switch OFF delay (in sec., 0 no rearm)"));

  ResponseCmndDone();
}

void CmndOffloadType ()
{
  if (XdrvMailbox.data_len > 0)
  {
    OffloadValidateDeviceType ((uint8_t)XdrvMailbox.payload);
    OffloadSaveConfig ();
  }
  ResponseCmndNumber (offload_config.type);
}

void CmndOffloadPower ()
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload <= OFFLOAD_POWER_MAX))
  {
    offload_config.device_power = (uint16_t)XdrvMailbox.payload;
    OffloadSaveConfig ();
  }
  ResponseCmndNumber (offload_config.device_power);
}

void CmndOffloadPriority ()
{
  if (XdrvMailbox.data_len > 0)
  {
    offload_config.priority = (uint8_t)XdrvMailbox.payload;
    OffloadSaveConfig ();
  }
  ResponseCmndNumber (offload_config.priority);
}

void CmndOffloadMax ()
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload <= OFFLOAD_POWER_MAX)) offload_config.contract_power = (uint16_t)XdrvMailbox.payload;
  ResponseCmndNumber (offload_config.contract_power);
}

void CmndOffloadAdjust ()
{
  if ((XdrvMailbox.data_len > 0) && (abs (XdrvMailbox.payload) <= 200))
  {
    offload_config.contract_adjust = (uint8_t)XdrvMailbox.payload;
    OffloadSaveConfig ();
  }
  ResponseCmndNumber (offload_config.contract_adjust);
}

void CmndOffloadPowerTopic ()
{
  if (XdrvMailbox.data_len > 0)
  {
    offload_config.str_topic = XdrvMailbox.data;
    OffloadSaveConfig ();
  }
  ResponseCmndChar (offload_config.str_topic.c_str ());
}

void CmndOffloadPowerKeyInst ()
{
  if (XdrvMailbox.data_len > 0)
  {
    offload_config.str_kinst = XdrvMailbox.data;
    OffloadSaveConfig ();
  }
  ResponseCmndChar (offload_config.str_kinst.c_str ());
}

void CmndOffloadPowerKeyMax ()
{
  if (XdrvMailbox.data_len > 0)
  {
    offload_config.str_kmax = XdrvMailbox.data;
    OffloadSaveConfig ();
  }
  ResponseCmndChar (offload_config.str_kmax.c_str ());
}

void CmndOffloadDelayAutoOn ()
{
  if (XdrvMailbox.data_len > 0)
  {
    offload_config.delay_auto_on = (uint16_t)XdrvMailbox.payload;
    OffloadSaveConfig ();
  }
  ResponseCmndNumber (offload_config.delay_auto_on);
}

void CmndOffloadDelayAutoOff ()
{
  if (XdrvMailbox.data_len > 0)
  {
    offload_config.delay_auto_off = (uint16_t)XdrvMailbox.payload;
    OffloadSaveConfig ();
  }
  ResponseCmndNumber (offload_config.delay_auto_off);
}

/**************************************************\
 *                  Functions
\**************************************************/

// declare device type in the available list
void OffloadResetAvailableType ()
{
  offload_config.nbr_type = 0;
}

// declare device type in the available list
void OffloadAddAvailableType (uint8_t device_type)
{
  if ((offload_config.nbr_type < OFFLOAD_DEVICE_MAX) && (device_type < OFFLOAD_DEVICE_MAX))
  {
    offload_config.arr_type[offload_config.nbr_type] = device_type;
    offload_config.nbr_type++;
  }
}

// validate device type selection
uint8_t OffloadValidateDeviceType (uint8_t new_type)
{
  bool    is_ok = false;
  uint8_t index;

  // loop to check if device is in the availability list
  for (index = 0; index < offload_config.nbr_type; index ++) if (offload_config.arr_type[index] == new_type) is_ok = true;
 
  // if device is available, save appliance type and associatd delays
  if (is_ok)
  {
    offload_config.type = new_type;
    if (offload_config.priority == 0) offload_config.priority = arr_offload_priority[new_type];

    // calculate delay before offloading (10 - priority in sec.)
    offload_status.delay_before = 2 * (OFFLOAD_PRIORITY_MAX - offload_config.priority);

    // calculate delay after offloading (priority x 6 sec. + 5 sec. random)
    offload_status.delay_after  = arr_offload_after[new_type] + random (0, arr_offload_after[new_type] / 2);
  }
  else new_type = UINT8_MAX;

  return new_type;
}

// get maximum power limit before offload
uint16_t OffloadGetMaxPower ()
{
  uint32_t power_max = 0;

  // calculate maximum power including extra overload
  if (offload_config.contract_power != 0) power_max = offload_config.contract_power * (uint16_t)offload_config.contract_adjust / 100;

  return (uint16_t)power_max;
}

// get offload state
bool OffloadIsOffloaded ()
{
  return (offload_status.stage >= OFFLOAD_STAGE_ACTIVE);
}

// get offload newly set state
bool OffloadJustSet ()
{
  bool result;
  
  // calculate and reset state changed flag
  result = (offload_status.changed && (offload_status.event_idx != UINT8_MAX));
  offload_status.changed = false;

  return result;
}

// get offload newly removed state
bool OffloadJustRemoved ()
{
  bool result;

  // calculate and reset state changed flag
  result  = (offload_status.changed && (offload_status.event_idx == UINT8_MAX));
//  result |= (offload_status.changed && (offload_status.event_start == UINT32_MAX));
  offload_status.changed = false;

  return result;
}

// set status flags
void OffloadSetManagedMode (bool is_managed)
{
  offload_status.managed = is_managed;
}

// generate time string : 12d 03h 22m 05s
void OffloadGenerateTime (char *pstr_time, size_t size, uint32_t local_time)
{
  bool   set_date, set_hour, set_minute;
  TIME_T time_dst;
  char   str_part[8];

  // init
  strcpy (pstr_time, "");

  // generate time structure
  BreakTime (local_time, time_dst);

  // set flags
  set_date = (time_dst.days > 0);
  set_hour = (set_date || (time_dst.hour > 0));
  set_minute = (set_date || set_hour || (time_dst.minute > 0));

  // generate string
  if (set_date) sprintf_P (pstr_time, PSTR ("%ud "), time_dst.days); 
  if (set_hour)
  {
    sprintf_P (str_part, PSTR ("%uh "), time_dst.hour);
    strlcat (pstr_time, str_part, size);
  }
  if (set_minute)
  {
    sprintf_P (str_part, PSTR ("%um "), time_dst.minute);
    strlcat (pstr_time, str_part, size);
  }
  sprintf_P (str_part, PSTR ("%us"), time_dst.second);
  strlcat (pstr_time, str_part, size);
}

// activate offload state
void OffloadActivate ()
{
  uint8_t index, position;

  // find first available slot
  for (index = 0; index < MAX_SHUTTERS; index ++) if (Settings->shutter_button[index] == 0) break;
  position = index;

  // if not slot available, shift previous one
  if (position == MAX_SHUTTERS)
  {
    for (index = MAX_SHUTTERS - 1; index > 0; index --)
    {
      Settings->shutter_button[index] = Settings->shutter_button[index - 1];
      Settings->shutter_closetime[index] = Settings->shutter_closetime[index - 1];
    }
    position = 0;
  }

  // init offload event
  Settings->shutter_button[position] = LocalTime ();
  Settings->shutter_closetime[position] = 0;
  offload_status.time_next = UINT32_MAX;
  offload_status.event_idx = position;
  offload_status.changed   = true;

  // set current event
//  offload_status.event_power   = offload_status.total_pinst;
//  offload_status.event_start   = LocalTime ();

  // read relay state and switch off if needed
  if (offload_status.managed)
  {
    // save relay state
    offload_status.relay = bitRead (TasmotaGlobal.power, 0);

    // if relay is ON, switch off
    if (offload_status.relay == 1) ExecuteCommandPower (1, POWER_OFF, SRC_MAX);
  }

  // log event
//  sprintf_P (str_value, PSTR ("%u"), offload_status.event_power);
//  sprintf_P (str_value, PSTR ("%u"), offload_status.total_pinst);
//  LogSaveEvent (LOG_EVENT_NEW, str_value);

  // get relay state and log
  AddLog (LOG_LEVEL_INFO, PSTR ("OFL: Offload starts, relay was %u"), offload_status.relay);
}

// remove offload state
void OffloadRemove ()
{
  // save event duration
  if (offload_status.event_idx < MAX_SHUTTERS) Settings->shutter_closetime[offload_status.event_idx] = (uint16_t)(LocalTime () - Settings->shutter_button[offload_status.event_idx]);

  // if relay is managed and it was ON, switch it back
  if (offload_status.managed && (offload_status.relay == 1)) ExecuteCommandPower (1, POWER_ON, SRC_MAX);

  // reset current offload data
  offload_status.time_next = UINT32_MAX;
  offload_status.event_idx = UINT8_MAX;
  offload_status.changed   = true;

  // log offloading removal
  AddLog (LOG_LEVEL_INFO, PSTR ("OFL: Offload stops, relay is %u"), offload_status.relay);
}

// Called just before setting relays
bool OffloadSetDevicePower ()
{
  bool     result = false;
  uint32_t time_now;

  // log
  AddLog (LOG_LEVEL_DEBUG, PSTR ("OFF: Before - managed %u, state %u, index %d, payload %d,"), offload_status.managed, offload_status.relay, XdrvMailbox.index, XdrvMailbox.payload);

  // if relay is managed, 
  if (offload_status.managed)
  {
    // get current time
    time_now = LocalTime ();

    // if relay command is not coming from offload module
    if (XdrvMailbox.payload != SRC_MAX)
    {
      // save target state of first relay
      offload_status.relay = (uint8_t)XdrvMailbox.index;

      // if offload is active
      if (OffloadIsOffloaded ())
      {
        // log and ignore action
        AddLog (LOG_LEVEL_INFO, PSTR ("OFF: Offload active, relay order %u blocked"), offload_status.relay);
        result = true;
      }
    }

    // else command is coming from offload (SRC_MAX), update relay state according to the command
    else offload_status.relay = (XdrvMailbox.index == 1);

    // if relay will be switched OFF
    if (XdrvMailbox.index == 0)
    {
      // if auto switch ON is enabled, set time when relay will be back ON
      if (offload_config.delay_auto_on > 0) offload_status.time_auto_on = time_now + offload_config.delay_auto_on;
        else offload_status.time_auto_on = UINT32_MAX;

      // disable auto switch OFF
      offload_status.time_auto_off = UINT32_MAX;
    } 

    // else, if relay will be switched ON,
    else if (XdrvMailbox.index == 1)
    {
      // if auto switch OFF is enabled, set time when relay will be back OFF
      if (offload_config.delay_auto_off > 0) offload_status.time_auto_off = time_now + offload_config.delay_auto_off;
        else offload_status.time_auto_off = UINT32_MAX;

      // disable auto switch ON
      offload_status.time_auto_on = UINT32_MAX;
    } 
  }

#ifdef USE_PILOTWIRE
  else result = PilotwireSetDevicePower ();
#endif // USE_PILOTWIRE

  // log
  AddLog (LOG_LEVEL_DEBUG, PSTR ("OFF: After - managed %u, state %u, index %d, payload %d,"), offload_status.managed, offload_status.relay, XdrvMailbox.index, XdrvMailbox.payload);

  return result;
}

// Show JSON status (for MQTT)
// Format is : "Offload":{"State":0,"Stage":1,"Power":1000}
void OffloadShowJSON (bool is_autonomous)
{
  // add , in append mode
  if (is_autonomous) Response_P (PSTR ("{"));
    else ResponseAppend_P (PSTR (","));

  // generate JSON
  ResponseAppend_P (PSTR ("\"Offload\":{\"State\":%u,\"Stage\":%u,\"Power\":%u}"), OffloadIsOffloaded (), offload_status.stage, offload_config.device_power);

  // publish it if message is autonomous
  if (is_autonomous)
  {
    // publish message
    ResponseAppend_P (PSTR ("}"));
    MqttPublishTeleSensor ();

    // set next JSON update trigger
    if (offload_status.stage == OFFLOAD_STAGE_NONE) offload_status.time_json = UINT32_MAX;
      else offload_status.time_json = LocalTime () + OFFLOAD_POWER_UPDATE;
  } 
}

// check and update MQTT power subsciption after disconnexion
void OffloadMqttSubscribe ()
{
  // if subsciption topic defined
  if (offload_config.str_topic.length () > 0)
  {
    // subscribe to power meter and log
    MqttSubscribe (offload_config.str_topic.c_str ());
    AddLog (LOG_LEVEL_INFO, PSTR ("OFL: Subscribed to %s"), offload_config.str_topic.c_str ());
  }
}

// read received MQTT data to retrieve house instant power
bool OffloadMqttData ()
{
  bool is_topic, is_key;
  bool is_found = false;
  long power;
  char str_value[16];

  // check for meter topic
  is_topic = (strcmp (offload_config.str_topic.c_str (), XdrvMailbox.topic) == 0);

  if (is_topic)
  {
    // log
    AddLog (LOG_LEVEL_DEBUG, PSTR ("OFL: Received %s"), XdrvMailbox.topic);

    // look for max power key
    is_key = SensorGetJsonKey (XdrvMailbox.data, offload_config.str_kmax.c_str (), str_value, sizeof (str_value));
    if (is_key)
    {
      is_found = true;
      power = atol (str_value);
      if (power > 0) offload_config.contract_power = (uint16_t)power;
    }

    // look for instant power key
    is_key = SensorGetJsonKey (XdrvMailbox.data, offload_config.str_kinst.c_str (), str_value, sizeof (str_value));
    if (is_key)
    {
      is_found = true;
      power = atol (str_value);
      offload_status.total_pinst = (uint16_t)power;
    }
  }

  // if meter value have been found
  if (is_found)
  {
    // set message timestamp
    offload_status.time_message = LocalTime ();

    // log
    AddLog (LOG_LEVEL_DEBUG, PSTR ("OFL: %u / %u VA"), offload_status.total_pinst, offload_config.contract_power);
  }

  return is_topic;
}

// update offloading status according to all parameters
void OffloadEvery250ms ()
{
  uint16_t power_max;
  uint32_t time_next, time_now;

  // if contract power and device power are defined
  power_max = OffloadGetMaxPower ();
  if ((power_max > 0) && (offload_config.device_power > 0))
  {
    // get current time
    time_next = offload_status.time_next;
    time_now  = LocalTime ();

    // switch according to current state
    switch (offload_status.stage)
    { 
      // actually not offloaded
      case OFFLOAD_STAGE_NONE:
        // if needed, reset end of stage time
        if (offload_status.time_next != UINT32_MAX) OffloadRemove ();

        // save relay state
        offload_status.relay = bitRead (TasmotaGlobal.power, 0);

        // if overload is detected, start offload process
        if (offload_status.total_pinst > power_max) offload_status.stage = OFFLOAD_STAGE_BEFORE;
        break;

      // pending offloading
      case OFFLOAD_STAGE_BEFORE:
        // if needed, calculate time for before offload stage
        if (offload_status.time_next == UINT32_MAX) offload_status.time_next = time_now + (uint32_t)offload_status.delay_before;

        // save relay state
        offload_status.relay = bitRead (TasmotaGlobal.power, 0);

        // if house power has gone down, remove pending offloading
        if (offload_status.total_pinst <= power_max) offload_status.stage = OFFLOAD_STAGE_NONE;

        // else if delay is reached, set active offloading
        else if (offload_status.time_next <= time_now) offload_status.stage = OFFLOAD_STAGE_ACTIVE;
        break;

      // offloading is active
      case OFFLOAD_STAGE_ACTIVE:
        // if just started, reset timer and set relay offload
        if (offload_status.time_next != UINT32_MAX) OffloadActivate ();

        // calculate maximum power allowed when substracting device power
        if (power_max > offload_config.device_power) power_max = power_max - offload_config.device_power;
          else power_max = 0;

        // if instant power is under this value, prepare to remove offload
        if (offload_status.total_pinst <= power_max) offload_status.stage = OFFLOAD_STAGE_AFTER;
        break;

      // actually just after offloading should stop
      case OFFLOAD_STAGE_AFTER:
        // if needed, calculate end of stage time
        if (offload_status.time_next == UINT32_MAX) offload_status.time_next = time_now + (uint32_t)offload_status.delay_after;

        // calculate maximum power allowed when substracting device power
        if (power_max > offload_config.device_power) power_max = power_max - offload_config.device_power;
          else power_max = 0;

        // if house power has gone again too high, offloading back again
        if (offload_status.total_pinst > power_max) offload_status.stage = OFFLOAD_STAGE_ACTIVE;
        
        // else if delay is reached, set remove offloading
        else if (offload_status.time_next <= time_now) offload_status.stage = OFFLOAD_STAGE_NONE;
        break;
    }

    // update MQTT status if needed
    if (time_next != offload_status.time_next) offload_status.time_json = time_now;
  }

   // if JSON needs to be updated
  if ((offload_status.time_json != UINT32_MAX) && (offload_status.time_json <= time_now)) OffloadShowJSON (true);

}

// check if MQTT message should be sent
void OffloadEverySecond ()
{
  // nothing to do before initial delay
  if (TasmotaGlobal.uptime < 5) return;

#ifdef USE_ENERGY_SENSOR
  float    power;
  uint16_t new_power = UINT16_MAX;

  // if current is detected (more than eq 20w), set power sensor flag
  if (Energy->current[0] > 0.1) offload_status.sensor = true;

  // if power sensor present
  if (offload_status.sensor)
  {
    // record apparent power if greater than previous reference
    power = Energy->voltage[0] * Energy->current[0];
    offload_status.pinst = (uint16_t)power;

    // detect new peak power
    if (offload_config.device_power < offload_status.pinst)
    {
      offload_config.device_power = offload_status.pinst;
      offload_status.time_json    = LocalTime (); 
    }
  }
#endif

  // update history slot
  if (offload_status.stage == OFFLOAD_STAGE_ACTIVE) SensorInactivitySet ();

  // check if auto-rearm is active
  if (offload_status.managed && !OffloadIsOffloaded ())
  {
    // if auto switch ON timeout is reached
    if ((offload_status.time_auto_on != UINT32_MAX) && (offload_status.time_auto_on < LocalTime ()))
    {
      offload_status.time_auto_on = UINT32_MAX;
      ExecuteCommandPower (1, POWER_ON, SRC_MAX);
    }

    // else if auto switch OFF timeout is reached
    else if ((offload_status.time_auto_off != UINT32_MAX) && (offload_status.time_auto_off < LocalTime ()))
    {
      offload_status.time_auto_off = UINT32_MAX;
      ExecuteCommandPower (1, POWER_OFF, SRC_MAX);
    }
  }
}

// offload module initialisation
void OffloadModuleInit ()
{
  // disable wifi sleep mode
  Settings->flag5.wifi_no_sleep = true;
  TasmotaGlobal.wifi_stay_asleep = false;
}

// offload initialisation
void OffloadInit ()
{
  uint32_t index;

  // disable fast cycle power recovery
  Settings->flag3.fast_power_cycle_disable = true;

  // force tele on power (to update switch state thru TELE)
  Settings->flag3.hass_tele_on_power = true;

  // init available devices list
  for (index = 0; index < OFFLOAD_DEVICE_MAX; index++) OffloadAddAvailableType (index);

  // load configuration
  OffloadLoadConfig ();

  // init device current
  Energy->current[0] = 0;

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ol_help to get help on Offload commands"));
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// icon : appliance logo
#ifdef USE_UFILESYS

bool OffloadWebHasLogo ()
{
  bool has_logo;
  char str_text[32];

  // check for logo.png
  sprintf_P (str_text, PSTR("/%s.png"), D_OFFLOAD_ICON_LOGO);
  has_logo = ffsp->exists (str_text);

  // check for logo.jpg
  if (!has_logo)
  {
    sprintf_P (str_text, PSTR("/%s.jpg"), D_OFFLOAD_ICON_LOGO);
    has_logo = ffsp->exists (str_text);
  }

  return has_logo;
}

void OffloadWebIconLogo ()
{
  bool is_present;
  char str_mime[8];
  char str_text[32];
  File file;

  // check for logo.png
  strcpy (str_mime, "png");
  sprintf_P (str_text, PSTR("/%s.png"), D_OFFLOAD_ICON_LOGO);
  is_present = ffsp->exists (str_text);

  // check for logo.jpg
  if (!is_present)
  {
    strcpy (str_mime, "jpeg");
    sprintf_P (str_text, PSTR("/%s.jpg"), D_OFFLOAD_ICON_LOGO);
    is_present = ffsp->exists (str_text);
  }

  // if present, stream image
  if (is_present)
  {
    file = ffsp->open (str_text, "r");
    sprintf_P (str_text, PSTR("image/%s"), str_mime);
    Webserver->streamFile (file, str_text);
    file.close ();
  }
}
#endif    // USE_UFILESYS

// append offloading buttons to main page
void OffloadWebMainButton ()
{
  // offload history button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>⚡ Offload events</button></form></p>"), D_PAGE_OFFLOAD_HISTORY);

  // if in managed mode, append control button
  if (offload_status.managed) WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>"), D_PAGE_OFFLOAD_CONTROL, D_OFFLOAD_CONTROL);
}

// append offloading state to main page
void OffloadWebSensor ()
{
  uint32_t percent, graph_max, power_max;
  uint32_t value, time_now;
  char     str_value[12];
  char     str_text[32];
  char     str_label[32];

  // current time
  time_now = LocalTime ();

  // if relay is directly managed
  if (offload_status.managed && !OffloadIsOffloaded ())
  {
    // if auto switch ON is active
    if (offload_status.time_auto_on != UINT32_MAX)
    {
      value = 0;
      if (offload_status.time_auto_on >= time_now) value = offload_status.time_auto_on - time_now;
      WSContentSend_PD (PSTR ("{s}Switch %s{m}%u sec.{e}"), "ON", value);
    }

    // if auto switch OFF is active
    if (offload_status.time_auto_off != UINT32_MAX)
    {
      value = 0;
      if (offload_status.time_auto_off >= time_now) value = offload_status.time_auto_off - time_now;
      WSContentSend_PD (PSTR ("{s}Switch %s{m}%u sec.{e}"), "OFF", value);
    }
  } 

  // if house power is subscribed, display offload status
  power_max = OffloadGetMaxPower ();
  if (power_max > 0)
  {
    // calculate power percentage
    percent   = 100 * offload_status.total_pinst / power_max;
    graph_max = min ((uint32_t)100, percent);

    // display start
    WSContentSend_PD (PSTR ("<div style='font-size:10px;text-align:center;margin-top:4px;padding:2px 6px;background:#333333;border-radius:8px;'>\n"));

    // get color
    if (offload_status.total_pinst >= power_max) GetTextIndexed (str_value, sizeof (str_value), OFFLOAD_STAGE_ACTIVE, kOffloadStageColor);
      else if (offload_status.total_pinst >= power_max - offload_config.device_power) GetTextIndexed (str_value, sizeof (str_value), OFFLOAD_STAGE_BEFORE, kOffloadStageColor);
      else GetTextIndexed (str_value, sizeof (str_value), OFFLOAD_STAGE_NONE, kOffloadStageColor);

    // display bar graph
    WSContentSend_PD (PSTR ("<div style='display:flex;padding:0px;'>\n"));
    WSContentSend_PD (PSTR ("<div style='width:28%%;padding:0px;text-align:left;font-size:16px;font-weight:bold;'>Offload</div>\n"));
    WSContentSend_PD (PSTR ("<div style='width:47%%;padding:0px;'><span style='float:left;width:%u%%;margin-top:3px;font-size:12px;border-radius:6px;background:%s;'>%u%%</span></div>\n"), graph_max, str_value, percent);
    WSContentSend_PD (PSTR ("<div style='width:25%%;padding:0px;text-align:right;font-size:16px;'>%u VA</div>\n"), offload_status.total_pinst);
    WSContentSend_PD (PSTR ("</div>\n"));

    // display managed device data
    if (offload_status.managed) 
    {
      // get device type
      GetTextIndexed (str_text, sizeof (str_text), offload_config.type, kOffloadDevice);
      GetTextIndexed (str_value, sizeof (str_value), offload_config.type, kOffloadIcon);

      // display device
      WSContentSend_PD (PSTR ("<div style='display:flex;padding:0px;font-size:14px;'>\n"));
      WSContentSend_PD (PSTR ("<div style='width:28%%;padding:0px;'>&nbsp;</div>\n"));
      WSContentSend_PD (PSTR ("<div style='width:47%%;padding:0px;'>%s</div>\n"), str_text);
      WSContentSend_PD (PSTR ("<div style='width:25%%;padding:0px;text-align:right;'>%s</div>\n"), str_value);
      WSContentSend_PD (PSTR ("</div>\n"));
    }

    // if offload process
    if ((offload_status.stage == OFFLOAD_STAGE_ACTIVE) || (offload_status.time_next != UINT32_MAX))
    {
      // get color and label
      GetTextIndexed (str_value, sizeof (str_value), offload_status.stage, kOffloadStageColor);
      GetTextIndexed (str_text,  sizeof (str_text),  offload_status.stage, kOffloadStageLabel);
      sprintf (str_label, str_text, offload_status.time_next - time_now);

      // display warning
      WSContentSend_PD (PSTR ("<div style='display:flex;padding:0px;font-size:14px;'>\n"));
      WSContentSend_PD (PSTR ("<div style='width:100%%;padding:0px;font-weight:bold;color:%s;'>%s</div>\n"), str_value, str_label);
      WSContentSend_PD (PSTR ("</div>\n"));
    }

    // display end
    WSContentSend_PD (PSTR ("</div>\n"));
  }
}

// Offload configuration web page
void OffloadWebConfig ()
{
  int      index, value, result, device;
  uint16_t power;
  char     str_icon[8];
  char     str_default[64];
  char     str_argument[64];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg("save"))
  {
    // set contract power limit according to 'contract' parameter
    WebGetArg (D_CMND_OFFLOAD_TYPE, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) OffloadValidateDeviceType ((uint8_t)atoi (str_argument));

    // set power of heater according to 'power' parameter
    WebGetArg (D_CMND_OFFLOAD_POWER, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0)
    {
      value = atoi (str_argument);
      if (value <= OFFLOAD_POWER_MAX) offload_config.device_power = (uint16_t)value;
    }

    // set contract power limit according to 'max' parameter
    WebGetArg (D_CMND_OFFLOAD_MAX, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0)
    {
      value = atoi (str_argument);
      if (value <= OFFLOAD_POWER_MAX) offload_config.contract_power = (uint16_t)value;
    }

    // set contract power limit according to 'adjust' parameter
    WebGetArg (D_CMND_OFFLOAD_ADJUST, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0)
    {
      value = atoi (str_argument);
      if ((value > 0) && (value <= 200)) offload_config.contract_adjust = (uint8_t)value;
    }

    // set offloading device priority according to 'priority' parameter
    WebGetArg (D_CMND_OFFLOAD_PRIORITY, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0)
    {
      value = atoi (str_argument);
      if (value <= OFFLOAD_PRIORITY_MAX) offload_config.priority = (uint8_t)value;
    }

    // set MQTT topic according to 'topic' parameter
    WebGetArg (D_CMND_OFFLOAD_TOPIC, str_argument, sizeof (str_argument));
    offload_config.str_topic = str_argument;

    // set JSON key according to 'inst' parameter
    WebGetArg (D_CMND_OFFLOAD_KINST, str_argument, sizeof (str_argument));
    offload_config.str_kinst = str_argument;

    // set JSON key according to 'max' parameter
    WebGetArg (D_CMND_OFFLOAD_KMAX, str_argument, sizeof (str_argument));
    offload_config.str_kmax = str_argument;

    // set auto switch ON duration according to 'on' parameter
    WebGetArg (D_CMND_OFFLOAD_AUTO_ON, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) offload_config.delay_auto_on = (uint16_t)atoi (str_argument);

    // set auto switch OFF duration according to 'off' parameter
    WebGetArg (D_CMND_OFFLOAD_AUTO_OFF, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) offload_config.delay_auto_off = (uint16_t)atoi (str_argument);

    // save configuration
    OffloadSaveConfig ();
  }

  // beginning of form
  WSContentStart_P (D_OFFLOAD_CONFIGURE);
  WSContentSendStyle ();

  // specific style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("p.half {display:inline-block;width:160px;}\n"));
  WSContentSend_P (PSTR ("p.third {display:inline-block;width:100px;}\n"));
  WSContentSend_P (PSTR ("span.key {float:right;font-size:0.7rem;}\n"));
  WSContentSend_P (PSTR ("input.switch {background:#aaa;}\n"));
  WSContentSend_P (PSTR ("fieldset {margin:16px auto;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_PAGE_OFFLOAD_CONFIG);

  // --------------
  //     Device  
  // --------------

  WSContentSend_P (OFFLOAD_FIELDSET_START, "⚙️", D_OFFLOAD_DEVICE);

  // appliance type
  WSContentSend_P (PSTR ("<p>%s<span class='key'>%s%s</span>\n"), D_OFFLOAD_TYPE, D_CMND_OFFLOAD_PREFIX, D_CMND_OFFLOAD_TYPE);
  WSContentSend_P (PSTR ("<select name='%s' id='%s'>\n"), D_CMND_OFFLOAD_TYPE, D_CMND_OFFLOAD_TYPE);
  for (index = 0; index < offload_config.nbr_type; index ++)
  {
    device = offload_config.arr_type[index];
    GetTextIndexed (str_default, sizeof (str_default), device, kOffloadDevice);
    GetTextIndexed (str_icon, sizeof (str_icon), device, kOffloadIcon);
    if (device == offload_config.type) strcpy (str_argument, "selected"); else strcpy (str_argument, "");
    WSContentSend_P (PSTR ("<option value='%d' %s>%s %s</option>\n"), device, str_argument, str_icon, str_default);
  }
  WSContentSend_P (PSTR ("</select>\n"));
  WSContentSend_P (PSTR ("</p>\n"));

  // device power
  sprintf_P (str_argument, PSTR ("%s (VA)"), D_OFFLOAD_POWER);
  WSContentSend_P (OFFLOAD_INPUT_NUMBER_HALF, str_argument, D_CMND_OFFLOAD_PREFIX, D_CMND_OFFLOAD_POWER, D_CMND_OFFLOAD_POWER, D_CMND_OFFLOAD_POWER, 0, 65000, 10, offload_config.device_power);

  // priority
  WSContentSend_P (OFFLOAD_INPUT_NUMBER_SWITCH, D_OFFLOAD_PRIORITY, D_CMND_OFFLOAD_PREFIX, D_CMND_OFFLOAD_PRIORITY, D_CMND_OFFLOAD_PRIORITY, D_CMND_OFFLOAD_PRIORITY, 1, OFFLOAD_PRIORITY_MAX, 1, offload_config.priority);

  WSContentSend_P (OFFLOAD_FIELDSET_STOP);

  // ------------
  //    Meter  
  // ------------

  WSContentSend_P (OFFLOAD_FIELDSET_START, "⚡", D_OFFLOAD_METER);

  // contract power
  sprintf_P (str_argument, PSTR ("%s (VA)"), D_OFFLOAD_MAX);
  WSContentSend_P (OFFLOAD_INPUT_NUMBER_HALF, str_argument, D_CMND_OFFLOAD_PREFIX, D_CMND_OFFLOAD_MAX, D_CMND_OFFLOAD_MAX, D_CMND_OFFLOAD_MAX, 0, 65000, 1, offload_config.contract_power);

  // contract adjustment
  sprintf_P (str_argument, PSTR ("%s (%%)"), D_OFFLOAD_ADJUST);
  WSContentSend_P (OFFLOAD_INPUT_NUMBER_HALF, str_argument, D_CMND_OFFLOAD_PREFIX, D_CMND_OFFLOAD_ADJUST, D_CMND_OFFLOAD_ADJUST, D_CMND_OFFLOAD_ADJUST, 1, 200, 1, offload_config.contract_adjust);

  // line break
  WSContentSend_P ("<hr>\n");

  // instant power mqtt topic
  WSContentSend_P (OFFLOAD_INPUT_TEXT, D_OFFLOAD_TOPIC, D_CMND_OFFLOAD_PREFIX, D_CMND_OFFLOAD_TOPIC, D_CMND_OFFLOAD_TOPIC, offload_config.str_topic.c_str (), "");

  // instant power json key
  WSContentSend_P (OFFLOAD_INPUT_TEXT, D_OFFLOAD_KEY_INST, D_CMND_OFFLOAD_PREFIX, D_CMND_OFFLOAD_KINST, D_CMND_OFFLOAD_KINST, offload_config.str_kinst.c_str (), "");

  // max power json key
  WSContentSend_P (OFFLOAD_INPUT_TEXT, D_OFFLOAD_KEY_MAX, D_CMND_OFFLOAD_PREFIX, D_CMND_OFFLOAD_KMAX, D_CMND_OFFLOAD_KMAX, offload_config.str_kmax.c_str (), "");


  WSContentSend_P (OFFLOAD_FIELDSET_STOP);

  // --------------
  //    Shut-off  
  // --------------

  WSContentSend_P (OFFLOAD_FIELDSET_START, "🔄", D_OFFLOAD_REARM);

  WSContentSend_P (OFFLOAD_INPUT_NUMBER, PSTR ("Auto switch ON delay (s.)"), D_CMND_OFFLOAD_PREFIX, D_CMND_OFFLOAD_AUTO_ON, D_CMND_OFFLOAD_AUTO_ON, D_CMND_OFFLOAD_AUTO_ON, 0, 900, 5, (int)offload_config.delay_auto_on);
  WSContentSend_P (OFFLOAD_INPUT_NUMBER, PSTR ("Auto switch OFF delay (s.)"), D_CMND_OFFLOAD_PREFIX, D_CMND_OFFLOAD_AUTO_OFF, D_CMND_OFFLOAD_AUTO_OFF, D_CMND_OFFLOAD_AUTO_OFF, 0, 900, 5, (int)offload_config.delay_auto_off);

  WSContentSend_P (OFFLOAD_FIELDSET_STOP);

  // ------------
  //    Script  
  // ------------

  WSContentSend_P (PSTR ("<script type='text/javascript'>\n"));

  WSContentSend_P (PSTR ("var arr_priority = [%d"), arr_offload_priority[0]);
  for (index = 1; index < OFFLOAD_DEVICE_MAX; index ++) WSContentSend_P (PSTR (",%d"), arr_offload_priority[index]);
  WSContentSend_P (PSTR ("];\n"));

  WSContentSend_P (PSTR ("var device_type = document.getElementById ('type');\n"));
  WSContentSend_P (PSTR ("var device_priority = document.getElementById ('prio');\n"));
  WSContentSend_P (PSTR ("device_type.onchange = function () {\n"));
  WSContentSend_P (PSTR ("device_priority.value = arr_priority[this.value];\n"));
  WSContentSend_P (PSTR ("}\n"));

  WSContentSend_P (PSTR ("</script>\n"));

  // save button  
  // -----------
  WSContentSend_P (PSTR ("<p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), D_SAVE);
  WSContentSend_P (PSTR ("</form>\n"));

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

// Offload history page
void OffloadWebHistory ()
{
  bool     event = false;
  uint8_t  index;
  TIME_T   event_dst;
  char str_duration[16];

  // beginning of page without authentification
  WSContentStart_P (PSTR ("Offload History"), false);
  WSContentSend_P (PSTR ("</script>\n"));

  // refresh every 10 sec
  WSContentSend_P (PSTR ("<meta http-equiv='refresh' content='%u'/>\n"), 10);

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {width:100%%;margin:12px auto;padding:3px 0px;text-align:center;vertical-align:middle;}\n"));
  WSContentSend_P (PSTR ("div.title {font-size:1.5rem;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.title a {color:white;}\n"));
  WSContentSend_P (PSTR ("div.header {font-size:1.8rem;color:yellow;}\n"));
  WSContentSend_P (PSTR ("table {width:100%%;max-width:600px;margin:0px auto;border:none;background:none;color:#fff;border-collapse:collapse;}\n"));
  WSContentSend_P (PSTR ("th,td {font-size:1rem;padding:0.5rem 1rem;border:1px #666 solid;}\n"));
  WSContentSend_P (PSTR ("th {font-weight:bold;background:#555;}\n"));
  WSContentSend_P (PSTR ("td.date {font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("td.text {font-style:italic;}\n"));
  WSContentSend_P (PSTR ("a:link {text-decoration:none;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // page body
  WSContentSend_P (PSTR ("</head>\n"));
  WSContentSend_P (PSTR ("<body>\n"));

  // device name, icon and title
  WSContentSend_P (PSTR ("<div class='title'><a href='/'>%s</a></div>\n"), SettingsText(SET_DEVICENAME));
  WSContentSend_P (PSTR ("<div class='header'>Offload History</div>\n"));

  // display table with header
  WSContentSend_P (PSTR ("<div><table>\n"));

  // display header
  WSContentSend_P (PSTR ("<tr><th>Date</th><th>Time</th><th>Duration</th></tr>\n"));

  // loop thru offload events array
  for (index = 0; index < MAX_SHUTTERS; index ++)
  {
    if (Settings->shutter_button[index] > 0)
    {
      event = true;

      // generate time structure
      BreakTime (Settings->shutter_button[index], event_dst);
      event_dst.year += 1970;

      // calculate event duration
      if (Settings->shutter_closetime[index] > 0) OffloadGenerateTime (str_duration, sizeof (str_duration), Settings->shutter_closetime[index]);
        else if ((index == 0) && (offload_status.stage > OFFLOAD_STAGE_BEFORE)) strcpy (str_duration, "<i>Active</i>");
        else strcpy (str_duration, "");

      // display event
      WSContentSend_P (PSTR ("<tr><td class='date'>%u-%02u-%02u</td><td class='time'>%02u:%02u</td><td class='duration'>%s</td></tr>\n"), event_dst.year, event_dst.month, event_dst.day_of_month, event_dst.hour, event_dst.minute, str_duration);
    }
  }

  // if no event
  if (!event) WSContentSend_P (PSTR ("<tr><td class='text' colspan=3>No event available</td></tr>\n"));

  // end of table and end of page
  WSContentSend_P (PSTR ("</table></div>\n"));
  WSContentStop ();
}

#ifdef USE_OFFLOAD_WEB

// get status update
void OffloadWebUpdate ()
{
  char str_text[16];

  // update switch status
  if (offload_status.relay == 1) strcpy (str_text, "true");
  else strcpy (str_text, "false");

  // update offload status
  if (OffloadIsOffloaded ()) strcat (str_text, ";⚡");
  else strcat (str_text, ";&nbsp;");

  // send result
  Webserver->send_P (200, "text/plain", str_text);
}

// Offloading public configuration page
void OffloadWebPageControl ()
{
  bool is_logged, has_logo;
  char str_text[8];

  // check if access is allowed
  is_logged = WebAuthenticate();

  // handle choice change
  if (Webserver->hasArg (D_CMND_OFFLOAD_CHOICE))
  {
    WebGetArg (D_CMND_OFFLOAD_CHOICE, str_text, sizeof (str_text));
    if (strcmp (str_text, "1") == 0) ExecuteCommandPower (1, POWER_ON, SRC_WEBGUI);
    else  ExecuteCommandPower (1, POWER_OFF, SRC_WEBGUI);
  }

  // beginning of form without authentification
  WSContentStart_P (D_OFFLOAD_CONTROL, false);
  WSContentSend_P (PSTR ("</script>\n"));

  // page data refresh script
  WSContentSend_P (PSTR ("<script type='text/javascript'>\n"));
  WSContentSend_P (PSTR ("function update() {\n"));
  WSContentSend_P (PSTR ("httpUpd=new XMLHttpRequest();\n"));
  WSContentSend_P (PSTR ("httpUpd.onreadystatechange=function() {\n"));
  WSContentSend_P (PSTR (" if (httpUpd.responseText.length>0) {\n"));
  WSContentSend_P (PSTR ("  arr_param=httpUpd.responseText.split(';');\n"));
  WSContentSend_P (PSTR ("  if (arr_param[0]!='') {status=document.getElementById('switch').checked;if (status != arr_param[0]){document.getElementById('switch').click();}}\n"));
  WSContentSend_P (PSTR ("  if (arr_param[1]!='') {document.getElementById('offload').innerHTML=arr_param[1];}\n"));
  WSContentSend_P (PSTR (" }\n"));
  WSContentSend_P (PSTR ("}\n"));
  WSContentSend_P (PSTR ("httpUpd.open('GET','%s',true);\n"), D_PAGE_OFFLOAD_UPDATE);
  WSContentSend_P (PSTR ("httpUpd.send();\n"));
  WSContentSend_P (PSTR ("}\n"));
  WSContentSend_P (PSTR ("setInterval(function() {update();},1000);\n"));
  WSContentSend_P (PSTR ("</script>\n"));

  // page style : main
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {width:100%%;font-size:1.5rem;padding:3px 0px;text-align:center;vertical-align:middle;}\n"));
  WSContentSend_P (PSTR ("div.section {margin:30px auto;}\n"));
  WSContentSend_P (PSTR ("div.title {font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.logo {font-size:9rem;}\n"));
  WSContentSend_P (PSTR ("img {height:128px;border-radius:12px;}\n"));

  // page style : logo
  WSContentSend_P (PSTR ("div.offload {position:absolute;margin-left:30px;margin-top:60px;font-size:6rem;}\n"));
  WSContentSend_P (PSTR ("a:link {text-decoration:none;}\n"));

  // page style : switch
  WSContentSend_P (PSTR (".toggle {width:120px;height:50px;position:relative;border-radius:50px;overflow:hidden;background-color:#c00;appearance:none;}\n"));
  WSContentSend_P (PSTR (".toggle:before {content:'ON OFF';display:block;position:absolute;width:46px;height:46px;background:#ccc;left:2px;top:2px;border-radius:50%%;font:20px/46px Helvetica;font-weight:bold;text-indent:-48px;word-spacing:62px;color:#fff;white-space:nowrap;}\n"));
  WSContentSend_P (PSTR (".toggle:checked {background-color:#0c0;}\n"));
  WSContentSend_P (PSTR (".toggle:checked:before {left:72px;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));
  WSContentSend_P (PSTR ("</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (PSTR ("<form method='get' action='%s' id='%s'>\n"), D_PAGE_OFFLOAD_CONTROL, D_PAGE_OFFLOAD_CONTROL);

  // device name
  WSContentSend_P (PSTR ("<div class='section title bold'>%s</div>\n"), SettingsText(SET_DEVICENAME));

  // if needed, add offload icon
  if (OffloadIsOffloaded ()) strcpy (str_text, "⚡"); else strcpy (str_text, "");
  WSContentSend_P (PSTR ("<div class='offload' id='offload'>%s</div>\n"), str_text);


  // appliance logo : start
  has_logo = false;
  WSContentSend_P (PSTR ("<div class='section logo'>"));
  if (is_logged) WSContentSend_P (PSTR ("<a href='/'>"));

#ifdef USE_UFILESYS
  // appliance logo : icon
  has_logo = OffloadWebHasLogo ();
#endif    // USE_UFILESYS

  // appliance logo : icon or default emoji logo
  if (has_logo) WSContentSend_P (PSTR ("<img src='/%s' >"), D_OFFLOAD_ICON_LOGO);
  else
  {
    GetTextIndexed (str_text, sizeof (str_text), offload_config.type, kOffloadIcon);
    WSContentSend_P (PSTR ("%s"), str_text);
  }

  // appliance logo : end
  if (is_logged) WSContentSend_P (PSTR ("</a>"));
  WSContentSend_P (PSTR ("</div>\n"));

  // display switch button
  if (offload_status.relay == 1) strcpy (str_text, "checked"); else strcpy (str_text, "");
  WSContentSend_P (PSTR ("<div class='section'><input class='toggle' type='checkbox' id='switch' name='switch' onclick='document.forms.control.submit();' %s /></div>\n"), str_text);

  // hidden new state
  WSContentSend_P (PSTR ("<input type='hidden' name='choice' value='%u' />\n"), !offload_status.relay);

  // end of page
  WSContentSend_P (PSTR ("</form>\n"));
  WSContentStop ();
}

#endif  // USE_OFFLOAD_WEB

#endif  // USE_WEBSERVER

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xdrv96 (uint32_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
    case FUNC_MODULE_INIT:
      OffloadModuleInit ();
      break;
    case FUNC_INIT:
      OffloadInit ();
      break;
   case FUNC_SET_DEVICE_POWER:
      result = OffloadSetDevicePower ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kOffloadCommands, OffloadCommand);
      break;
    case FUNC_EVERY_250_MSECOND:
      OffloadEvery250ms ();
      break;
    case FUNC_EVERY_SECOND:
      OffloadEverySecond ();
      break;
    case FUNC_MQTT_SUBSCRIBE:
      OffloadMqttSubscribe ();
      break;
    case FUNC_MQTT_DATA:
      result = OffloadMqttData ();
      break;
    case FUNC_JSON_APPEND:
      OffloadShowJSON (false);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      //pages
      Webserver->on ("/" D_PAGE_OFFLOAD_CONFIG, OffloadWebConfig);
      Webserver->on ("/" D_PAGE_OFFLOAD_HISTORY, OffloadWebHistory);

#ifdef USE_OFFLOAD_WEB
      // if relay is managed, relay switch page
      Webserver->on ("/" D_PAGE_OFFLOAD_CONTROL, OffloadWebPageControl);
      Webserver->on ("/" D_PAGE_OFFLOAD_UPDATE,  OffloadWebUpdate);

#ifdef USE_UFILESYS
      // appliance icon
      Webserver->on ("/" D_OFFLOAD_ICON_LOGO, OffloadWebIconLogo);
#endif    // USE_UFILESYS

#endif  // USE_OFFLOAD_WEB
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      OffloadWebMainButton ();
      break;
    case FUNC_WEB_ADD_BUTTON:
      WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>Configure Offload</button></form></p>"), D_PAGE_OFFLOAD_CONFIG);
      break;
    case FUNC_WEB_SENSOR:
      OffloadWebSensor ();
      break;
#endif  // USE_WEBSERVER
  }
  
  return result;
}

#endif    // USE_OFFLOAD
