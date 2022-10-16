/*
  xdrv_96_offload.ino - Device offloading thru MQTT instant and max power
  
  Copyright (C) 2020  Nicolas Bernaerts
    23/03/2020 - v1.0   - Creation
    26/05/2020 - v1.1   - Add Information JSON page
    07/07/2020 - v1.2   - Enable discovery (mDNS)
    20/07/2020 - v1.3   - Change offloading delays to seconds
    22/07/2020 - v1.3.1 - Update instant device power in case of Sonoff energy module
    05/08/2020 - v1.4   - Add /control page to have a public switch
                          If available, get max power thru MQTT meter
                          Phase selection and disable mDNS 
    22/08/2020 - v1.4.1 - Add restart after offload configuration
    05/09/2020 - v1.4.2 - Correct display exception on mainpage
    22/08/2020 - v1.5   - Save offload config using new Settings text
                          Add restart after offload configuration
    15/09/2020 - v1.6   - Add OffloadJustSet and OffloadJustRemoved
    19/09/2020 - v2.0   - Add Contract power adjustment in %
                          Set offload priorities as standard options
                          Add icons to /control page
    15/10/2020 - v2.1   - Expose icons on web server
    16/10/2020 - v2.2   - Handle priorities as list of device types
                          Add randomisation to reconnexion
    23/10/2020 - v2.3   - Update control page in real time
    05/11/2020 - v2.4   - Tasmota 9.0 compatibility
    11/11/2020 - v2.5   - Add offload history pages (/histo and /histo.json)
    23/04/2021 - v3.0   - Add fixed IP and remove use of String to avoid heap fragmentation
    22/09/2021 - v3.1   - Add LittleFS support to store offload events
    17/01/2022 - v3.2   - Use device type priority to handle delays
    08/04/2022 - v9.1   - Switch from icons to emojis
    08/06/2022 - v9.2   - Add auto-rearm capability

  If no littleFS partition is available, settings are stored using SettingsTextIndex :
    * SET_OFFLOAD_TOPIC
    * SET_OFFLOAD_KEY_INST
    * SET_OFFLOAD_KEY_MAX

  These keys should be added in tasmota.h at the end of "enum SettingsTextIndex" section just before SET_MAX :

	#ifndef USE_UFILESYS
		SET_OFFLOAD_TOPIC, SET_OFFLOAD_KEY_INST, SET_OFFLOAD_KEY_MAX,
	#endif 	// USE_UFILESYS
  
  If LittleFS partition is available :
    - settings are stored in offload.cfg
    - device icon should be stored as logo.png or logo.jpg

  If there is no LittleFS partition, settings are stored using unused KNX parameters :
    - Settings.knx_GA_addr[0] = Device type
    - Settings.knx_GA_addr[1] = Device power (W)
    - Settings.knx_GA_addr[2] = Device priority
    - Settings.knx_GA_addr[3] = Contract power (W)
    - Settings.knx_GA_addr[4] = Contract adjustment in % (1 = -99%, 100 = 0%, 199 = +99%)
    - Settings.knx_GA_addr[5] = Auto-ream delay 'sec.).
                                0 means no auto-rearm

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

#ifndef FIRMWARE_SAFEBOOT
#ifdef USE_OFFLOAD

#define XDRV_96                 96

#define OFFLOAD_PHASE_MAX       3
#define OFFLOAD_POWER_MAX       10000
#define OFFLOAD_DELAY_MAX       1000
#define OFFLOAD_PRIORITY_MAX    10
#define OFFLOAD_POWER_UPDATE    300              // update peak power every 5 minutes

#define OFFLOAD_EVENT_MAX       10

#define D_PAGE_OFFLOAD_CONFIG    "offload"
#define D_PAGE_OFFLOAD_CONTROL   "control"
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
#define D_CMND_OFFLOAD_STAGE     "stage"
#define D_CMND_OFFLOAD_REARM     "rearm"

#define D_CMND_OFFLOAD_CHOICE    "choice"

#define D_JSON_OFFLOAD           "Offload"
#define D_JSON_OFFLOAD_STATE     "State"
#define D_JSON_OFFLOAD_STAGE     "Stage"
#define D_JSON_OFFLOAD_DEVICE    "Device"
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

#define D_OFFLOAD_UNIT_VA        "VA"
#define D_OFFLOAD_UNIT_W         "W"
#define D_OFFLOAD_UNIT_SEC       "sec."
#define D_OFFLOAD_UNIT_PERCENT   "%"
#define D_OFFLOAD_SELECTED       "selected"

#define D_OFFLOAD_CFG            "/offload.cfg"
#define D_OFFLOAD_ICON_LOGO      "logo"    

// offloading commands
const char kOffloadCommands[] PROGMEM = D_CMND_OFFLOAD_PREFIX "|" D_CMND_OFFLOAD_HELP "|" D_CMND_OFFLOAD_TYPE "|" D_CMND_OFFLOAD_PRIORITY "|" D_CMND_OFFLOAD_POWER  "|" D_CMND_OFFLOAD_MAX "|" D_CMND_OFFLOAD_ADJUST "|" D_CMND_OFFLOAD_TOPIC "|" D_CMND_OFFLOAD_KINST "|" D_CMND_OFFLOAD_KMAX "|" D_CMND_OFFLOAD_STAGE "|" D_CMND_OFFLOAD_REARM;
void (* const OffloadCommand[])(void) PROGMEM = { &CmndOffloadHelp, &CmndOffloadType, &CmndOffloadPriority, &CmndOffloadPower, &CmndOffloadMax, &CmndOffloadAdjust, &CmndOffloadPowerTopic, &CmndOffloadPowerKeyInst, &CmndOffloadPowerKeyMax, &CmndOffloadStage, &CmndOffloadRearm };
 
// strings
const char OFFLOAD_FIELDSET_START[]      PROGMEM = "<fieldset><legend><b>&nbsp;%s %s&nbsp;</b></legend>\n";
const char OFFLOAD_FIELDSET_STOP[]       PROGMEM = "</fieldset>\n";
const char OFFLOAD_INPUT_NUMBER[]        PROGMEM = "<p>%s<span class='key'>%s%s</span><br><input type='number' name='%s' id='%s' min='%d' max='%d' step='%d' value='%d'></p>\n";
const char OFFLOAD_INPUT_NUMBER_HALF[]   PROGMEM = "<p class='half'>%s<span class='key'>%s%s</span><br><input type='number' name='%s' id='%s' min='%d' max='%d' step='%d' value='%d'></p>\n";
const char OFFLOAD_INPUT_NUMBER_SWITCH[] PROGMEM = "<p class='half'>%s<span class='key'>%s%s</span><br><input class='switch' type='number' name='%s' id='%s' min='%d' max='%d' step='%d' value='%d'></p>\n";
const char OFFLOAD_INPUT_TEXT[]          PROGMEM = "<p>%s<span class='key'>%s%s</span><br><input name='%s' value='%s' placeholder='%s'></p>\n";
const char OFFLOAD_BUTTON[]              PROGMEM = "<p><form action='%s' method='get'><button>%s</button></form></p>";

// definition of types of device with associated delays
enum OffloadDevice                     { OFFLOAD_DEVICE_APPLIANCE, OFFLOAD_DEVICE_LIGHT, OFFLOAD_DEVICE_FRIDGE, OFFLOAD_DEVICE_WASHING, OFFLOAD_DEVICE_DISH, OFFLOAD_DEVICE_DRIER, OFFLOAD_DEVICE_CUMULUS, OFFLOAD_DEVICE_IRON, OFFLOAD_DEVICE_BATHROOM, OFFLOAD_DEVICE_OFFICE, OFFLOAD_DEVICE_LIVING, OFFLOAD_DEVICE_ROOM, OFFLOAD_DEVICE_KITCHEN, OFFLOAD_DEVICE_MAX };
const uint8_t arr_offload_priority[] = { 3,            1,    1,     4,              4,          5,    1,      2,   6,       7,     8,          9,            10 };
const char kOffloadDevice[] PROGMEM  = "Misc appliance|Light|Fridge|Washing machine|Dish washer|Drier|Cumulus|Iron|Bathroom|Office|Living room|Sleeping room|Kitchen";               // labels
const char kOffloadIcon[]   PROGMEM  = "🔌|💡|🧊|👕|🍽️|🌀|💧|👕|♨️|♨️|♨️|♨️|♨️";                                                                      // icons

// offloading stages
enum OffloadStages { OFFLOAD_STAGE_NONE, OFFLOAD_STAGE_BEFORE, OFFLOAD_STAGE_ACTIVE, OFFLOAD_STAGE_AFTER,  OFFLOAD_STAGE_MAX };
const char kOffloadStageColor[] PROGMEM  = "|orange|red|white|orange|";                                                                         // colors of offload stages
const char kOffloadStageLabel[] PROGMEM  = "|Starting in <b>%d</b> sec.|<b>Active</b>|Active for <b>%d</b> sec.|Ending in <b>%d</b> sec.|";     // labels of offload stages                                    // color of offload stage labels

/*************************************************\
 *               Variables
\*************************************************/

// variables
struct {
  uint8_t   arr_device_type[OFFLOAD_DEVICE_MAX];
  uint8_t   nbr_device_type = 0;
  uint16_t  device_type     = OFFLOAD_DEVICE_APPLIANCE;   // device type
  uint16_t  device_priority = 0;                          // device priority
  uint16_t  device_power    = 0;                          // power of device
  uint16_t  contract_power  = 0;                          // maximum power limit before offload
  int       contract_adjust = 0;                          // adjustement of maximum power in %
  uint16_t  rearm_delay     = 0;                          // delay before automatically rearming the relay (in sec.)
  String    str_topic;                                    // mqtt topic to be used for meter
  String    str_kinst;                                    // mqtt instant apparent power key
  String    str_kmax;                                     // mqtt maximum apparent power key
} offload_config;

struct {
  uint8_t  stage         = OFFLOAD_STAGE_NONE;    // current offloading state
  uint8_t  relay_state   = 0;                     // relay state before offloading
  uint16_t power_inst    = 0;                     // actual phase instant power (retrieved thru MQTT)
  uint32_t time_message  = 0;                     // time of last power message
  uint32_t time_stage    = UINT32_MAX;            // time of next stage
  uint32_t time_json     = UINT32_MAX;            // time of next JSON update
  uint32_t time_update   = UINT32_MAX;            // time of last peak power update
  uint32_t time_rearm    = UINT32_MAX;            // time when relay should be automatically rearmed
  uint16_t delay_before  = 0;                     // delay in seconds before effective offloading
  uint16_t delay_after   = 0;                     // delay in seconds before removing offload
  bool     power_sensor  = false;                 // flag to define if power sensor is available
  bool     relay_managed = true;                  // flag to define if relay is managed directly
  bool     state_changed = false;                 // flag to signal that offload state has changed
  time_t   event_start   = UINT32_MAX;            // offload start time
  time_t   event_stop    = UINT32_MAX;            // offload release time
  uint16_t event_power   = 0;                     // power overload when offload triggered
} offload_status;

/**************************************************\
 *                  Accessors
\**************************************************/

// load config parameters
void OffloadLoadConfig ()
{
  uint16_t device_type;

#ifdef USE_UFILESYS

  // retrieve saved settings from flash filesystem
  device_type = UfsCfgLoadKeyInt (D_OFFLOAD_CFG, D_CMND_OFFLOAD_TYPE, OFFLOAD_DEVICE_APPLIANCE);
  offload_config.device_power    = (uint16_t)UfsCfgLoadKeyInt (D_OFFLOAD_CFG, D_CMND_OFFLOAD_POWER, 0);
  offload_config.contract_power  = (uint16_t)UfsCfgLoadKeyInt (D_OFFLOAD_CFG, D_CMND_OFFLOAD_MAX, 0);
  offload_config.device_priority = (uint16_t)UfsCfgLoadKeyInt (D_OFFLOAD_CFG, D_CMND_OFFLOAD_PRIORITY, OFFLOAD_PRIORITY_MAX);
  offload_config.contract_adjust = UfsCfgLoadKeyInt (D_OFFLOAD_CFG, D_CMND_OFFLOAD_ADJUST, 0);

  // get periodicity from flash filesystem
  offload_config.rearm_delay   = (uint16_t)UfsCfgLoadKeyInt (D_OFFLOAD_CFG, D_CMND_OFFLOAD_REARM, 0);

  // mqtt config
  offload_config.str_topic = UfsCfgLoadKey (D_OFFLOAD_CFG, D_CMND_OFFLOAD_TOPIC);
  offload_config.str_kinst = UfsCfgLoadKey (D_OFFLOAD_CFG, D_CMND_OFFLOAD_KINST);
  offload_config.str_kmax  = UfsCfgLoadKey (D_OFFLOAD_CFG, D_CMND_OFFLOAD_KMAX);

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("OFL: Config from LittleFS"));

#else       // No LittleFS

  // retrieve saved settings from flash memory
  device_type = Settings->knx_GA_addr[0];
  offload_config.device_power    = Settings->knx_GA_addr[1];
  offload_config.device_priority = Settings->knx_GA_addr[2];
  offload_config.contract_power  = Settings->knx_GA_addr[3];
  offload_config.contract_adjust = (int)Settings->knx_GA_addr[4] - 100;

  // periodicity
  offload_config.rearm_delay   = (uint16_t)Settings->knx_GA_addr[5];

  // mqtt config
  offload_config.str_topic = SettingsText (SET_OFFLOAD_TOPIC);
  offload_config.str_kinst = SettingsText (SET_OFFLOAD_KEY_INST);
  offload_config.str_kmax  = SettingsText (SET_OFFLOAD_KEY_MAX);

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("OFL: Config from Settings"));

# endif     // USE_UFILESYS

  // check for out of range values
  if (offload_config.device_power > OFFLOAD_POWER_MAX) offload_config.device_power = 0;
  if (offload_config.contract_power > OFFLOAD_POWER_MAX) offload_config.contract_power = 0;
  if (offload_config.device_priority > OFFLOAD_PRIORITY_MAX) offload_config.device_priority = OFFLOAD_PRIORITY_MAX;
  if (offload_config.contract_adjust > 100) offload_config.contract_adjust = 0;
  if (offload_config.contract_adjust < -99) offload_config.contract_adjust = 0;
  if (offload_config.rearm_delay == UINT16_MAX) offload_config.rearm_delay = 0;

  // validate device type
  OffloadValidateDeviceType (device_type);
}

// save config parameters
void OffloadSaveConfig ()
{
#ifdef USE_UFILESYS

  // save settings to flash filesystem
  UfsCfgSaveKeyInt (D_OFFLOAD_CFG, D_CMND_OFFLOAD_TYPE,     (int)offload_config.device_type,     true);
  UfsCfgSaveKeyInt (D_OFFLOAD_CFG, D_CMND_OFFLOAD_PRIORITY, (int)offload_config.device_priority, false);
  UfsCfgSaveKeyInt (D_OFFLOAD_CFG, D_CMND_OFFLOAD_POWER,    (int)offload_config.device_power,    false);
  UfsCfgSaveKeyInt (D_OFFLOAD_CFG, D_CMND_OFFLOAD_MAX,      (int)offload_config.contract_power,  false);
  UfsCfgSaveKeyInt (D_OFFLOAD_CFG, D_CMND_OFFLOAD_ADJUST,   offload_config.contract_adjust,      false);

  // periodicity of power cut
  UfsCfgSaveKeyInt (D_OFFLOAD_CFG, D_CMND_OFFLOAD_REARM,  (int)offload_config.rearm_delay, false);

  // mqtt config
  UfsCfgSaveKey (D_OFFLOAD_CFG, D_CMND_OFFLOAD_TOPIC, offload_config.str_topic.c_str (), false);
  UfsCfgSaveKey (D_OFFLOAD_CFG, D_CMND_OFFLOAD_KINST, offload_config.str_kinst.c_str (), false);
  UfsCfgSaveKey (D_OFFLOAD_CFG, D_CMND_OFFLOAD_KMAX,  offload_config.str_kmax.c_str (),  false);

# else      // No LittleFS

  // save settings to flash memory
  Settings->knx_GA_addr[0] = offload_config.device_type;
  Settings->knx_GA_addr[1] = offload_config.device_power;
  Settings->knx_GA_addr[2] = offload_config.device_priority;
  Settings->knx_GA_addr[3] = offload_config.contract_power;
  Settings->knx_GA_addr[4] = (uint16_t)(offload_config.contract_adjust + 100);

  // periodicity
  Settings->knx_GA_addr[5] = offload_config.rearm_delay;

  // mqtt config
  SettingsUpdateText (SET_OFFLOAD_TOPIC,    offload_config.str_topic.c_str ());
  SettingsUpdateText (SET_OFFLOAD_KEY_INST, offload_config.str_kinst.c_str ());
  SettingsUpdateText (SET_OFFLOAD_KEY_MAX,  offload_config.str_kmax.c_str ());

# endif     // USE_UFILESYS
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

  AddLog (LOG_LEVEL_INFO, PSTR (" - ol_topic = Topic for contract power"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ol_kinst = Key for current instant power"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ol_kmax  = Key for contract maximum power"));

  AddLog (LOG_LEVEL_INFO, PSTR (" - ol_stage = Offload stage (%u...%u)"), 0, OFFLOAD_STAGE_MAX - 1);
  AddLog (LOG_LEVEL_INFO, PSTR (" - ol_rearm = Switch rearm delay (0 no rearm or timeout in sec.)"));

  ResponseCmndDone();
}

void CmndOffloadType ()
{
  if (XdrvMailbox.data_len > 0)
  {
    OffloadValidateDeviceType ((uint16_t)XdrvMailbox.payload);
    OffloadSaveConfig ();
  }
  ResponseCmndNumber (offload_config.device_type);
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
    offload_config.device_priority = (uint16_t)XdrvMailbox.payload;
    OffloadSaveConfig ();
  }
  ResponseCmndNumber (offload_config.device_priority);
}

void CmndOffloadMax ()
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload <= OFFLOAD_POWER_MAX)) offload_config.contract_power = (uint16_t)XdrvMailbox.payload;
  ResponseCmndNumber (offload_config.contract_power);
}

void CmndOffloadAdjust ()
{
  if ((XdrvMailbox.data_len > 0) && (abs (XdrvMailbox.payload) <= 100))
  {
    offload_config.contract_adjust = XdrvMailbox.payload;
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

void CmndOffloadStage ()
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload >= 0) && (XdrvMailbox.payload < OFFLOAD_STAGE_MAX))
  {
    offload_status.time_stage = UINT32_MAX;
    offload_status.stage      = (uint8_t)XdrvMailbox.payload;
  }

  ResponseCmndNumber (offload_status.stage);
}

void CmndOffloadRearm ()
{
  if (XdrvMailbox.data_len > 0)
  {
    offload_config.rearm_delay = (uint16_t)XdrvMailbox.payload;
    OffloadSaveConfig ();
  }
  ResponseCmndNumber (offload_config.rearm_delay);
}

/**************************************************\
 *                  Functions
\**************************************************/

// declare device type in the available list
void OffloadResetAvailableType ()
{
  offload_config.nbr_device_type = 0;
}

// declare device type in the available list
void OffloadAddAvailableType (uint8_t device_type)
{
  if ((offload_config.nbr_device_type < OFFLOAD_DEVICE_MAX) && (device_type < OFFLOAD_DEVICE_MAX))
  {
    offload_config.arr_device_type[offload_config.nbr_device_type] = device_type;
    offload_config.nbr_device_type++;
  }
}

// validate device type selection
uint16_t OffloadValidateDeviceType (uint16_t new_type)
{
  bool     is_ok = false;
  uint16_t index;

  // loop to check if device is in the availability list
  for (index = 0; index < offload_config.nbr_device_type; index ++) if (offload_config.arr_device_type[index] == new_type) is_ok = true;
 
  // if device is available, save appliance type and associatd delays
  if (is_ok)
  {
    offload_config.device_type = new_type;
    if (offload_config.device_priority == 0) offload_config.device_priority = arr_offload_priority[new_type];

    // calculate delay before offloading (10 - priority in sec.)
    offload_status.delay_before = OFFLOAD_PRIORITY_MAX - offload_config.device_priority;

    // calculate delay after offloading (priority x 6 sec. + 5 sec. random)
    offload_status.delay_after  = 6 * offload_config.device_priority + random (0, 5);
  }
  else new_type = UINT16_MAX;

  return new_type;
}

// get maximum power limit before offload
uint16_t OffloadGetMaxPower ()
{
  uint32_t power_max = 0;

  // calculate maximum power including extra overload
  if (offload_config.contract_power != UINT32_MAX) power_max = offload_config.contract_power * (100 + offload_config.contract_adjust) / 100;

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
  result = (offload_status.state_changed && (offload_status.event_start != UINT32_MAX));
  offload_status.state_changed = false;

  return result;
}

// get offload newly removed state
bool OffloadJustRemoved ()
{
  bool result;

  // calculate and reset state changed flag
  result  = (offload_status.state_changed && (offload_status.event_start != UINT32_MAX) && (offload_status.event_stop != UINT32_MAX));
  result |= (offload_status.state_changed && (offload_status.event_start == UINT32_MAX));
  offload_status.state_changed = false;

  return result;
}

// set status flags
void OffloadSetManagedMode (bool is_managed)
{
  offload_status.relay_managed = is_managed;
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
  char str_value[8];

  // set current event
  offload_status.event_power   = offload_status.power_inst;
  offload_status.event_start   = LocalTime ();
  offload_status.state_changed = true;

  // read relay state and switch off if needed
  if (offload_status.relay_managed)
  {
    // save relay state
    offload_status.relay_state = bitRead (TasmotaGlobal.power, 0);

    // if relay is ON, switch off
    if (offload_status.relay_state == 1) ExecuteCommandPower (1, POWER_OFF, SRC_MAX);
  }

  // log event
  sprintf_P (str_value, PSTR ("%u"), offload_status.event_power);
  LogSaveEvent (LOG_EVENT_NEW, str_value);

  // get relay state and log
  AddLog (LOG_LEVEL_INFO, PSTR ("OFL: Offload starts, relay was %u"), offload_status.relay_state);
}

// remove offload state
void OffloadRemove ()
{
  char str_value[8];

  // set release time for current event
  offload_status.event_stop    = LocalTime ();
  offload_status.state_changed = true;

  // log event
  sprintf_P (str_value, PSTR ("%u"), offload_status.event_power);
  LogSaveEvent (LOG_EVENT_UPDATE, str_value);

  // if relay is managed and it was ON, switch it back
  if (offload_status.relay_managed && (offload_status.relay_state == 1)) ExecuteCommandPower (1, POWER_ON, SRC_MAX);

  // reset current offload data
  offload_status.event_start = UINT32_MAX;
  offload_status.event_stop  = UINT32_MAX;
  offload_status.event_power = 0;

  // log offloading removal
  AddLog (LOG_LEVEL_INFO, PSTR ("OFL: Offload stops, relay is %u"), offload_status.relay_state);
}

// Called just before setting relays
bool OffloadSetDevicePower ()
{
  bool result = false;

  // log
  AddLog (LOG_LEVEL_DEBUG, PSTR ("OFF: Before - managed %u, state %u, index %d, payload %d,"), offload_status.relay_managed, offload_status.relay_state, XdrvMailbox.index, XdrvMailbox.payload);

  // if relay is managed, 
  if (offload_status.relay_managed)
  {
    // if relay command is not coming from offload module
    if (XdrvMailbox.payload != SRC_MAX)
    {
      // save target state of first relay
      offload_status.relay_state = (uint8_t)XdrvMailbox.index;

      // if offload is active
      if (OffloadIsOffloaded ())
      {
        // log and ignore action
        AddLog (LOG_LEVEL_INFO, PSTR ("OFF: Offload active, relay order %u blocked"), offload_status.relay_state);
        result = true;
      }
    }

    // else command is coming from offload (SRC_MAX), update relay state according to the command
    else offload_status.relay_state = (XdrvMailbox.index == 1);

    // if relay will be switched OFF and auto-rearm is enabled, set time when relay will be back ON
    if ((XdrvMailbox.index == 0) && (offload_config.rearm_delay > 0)) offload_status.time_rearm = LocalTime () + offload_config.rearm_delay;

    // else, if relay will be switched ON, disable auto-rearm
    else if (XdrvMailbox.index == 1) offload_status.time_rearm = UINT32_MAX;
  }

#ifdef USE_PILOTWIRE
  else result = PilotwireSetDevicePower ();
#endif // USE_PILOTWIRE

  // log
  AddLog (LOG_LEVEL_DEBUG, PSTR ("OFF: After - managed %u, state %u, index %d, payload %d,"), offload_status.relay_managed, offload_status.relay_state, XdrvMailbox.index, XdrvMailbox.payload);

  return result;
}

// Show JSON status (for MQTT)
void OffloadShowJSON (bool is_autonomous)
{
  // add , in append mode
  if (is_autonomous) Response_P (PSTR ("{")); else ResponseAppend_P (PSTR (","));

  // "Offload":{"State":"OFF","Stage":1,"Device":1000}
  ResponseAppend_P (PSTR ("\"%s\":{"), D_JSON_OFFLOAD);
  ResponseAppend_P (PSTR ("\"%s\":%u"), D_JSON_OFFLOAD_STATE, OffloadIsOffloaded ());
  ResponseAppend_P (PSTR (",\"%s\":%d"), D_JSON_OFFLOAD_STAGE, offload_status.stage);
  ResponseAppend_P (PSTR (",\"DeviceVA\":%u"), offload_config.device_power);
  ResponseAppend_P (PSTR ("}"));

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
  bool data_handled;
  long power;
  char str_value[16];

  // if topic is the instant house power
  data_handled = (strcmp (offload_config.str_topic.c_str (), XdrvMailbox.topic) == 0);
  if (data_handled)
  {
    // set message timestamp
    offload_status.time_message = LocalTime ();

    // look for max power key
    if (SensorGetJsonKey (XdrvMailbox.data, offload_config.str_kmax.c_str (), str_value, sizeof (str_value)))
    {
      power = atol (str_value);
      if (power > 0) offload_config.contract_power = (uint16_t)power;
    }

    // look for instant power key
    if (SensorGetJsonKey (XdrvMailbox.data, offload_config.str_kinst.c_str (), str_value, sizeof (str_value)))
    {
      power = atol (str_value);
      offload_status.power_inst = (uint16_t)power;
    }

    // log
    AddLog (LOG_LEVEL_DEBUG, PSTR ("OFL: Power is %u/%u"), offload_status.power_inst, offload_config.contract_power);
  }

  return data_handled;
}

// update offloading status according to all parameters
void OffloadEvery250ms ()
{
  bool     json_update = false;
  uint8_t  prev_stage, next_stage;
  uint16_t power;
  uint32_t time_now;

  // if contract power and device power are defined
  power = OffloadGetMaxPower ();
  if ((power > 0) && (offload_config.device_power > 0))
  {
    // get current time
    time_now = LocalTime ();

    // set previous and next state to current state
    prev_stage = offload_status.stage;
    next_stage = prev_stage;
  
    // switch according to current state
    switch (offload_status.stage)
    { 
      // actually not offloaded
      case OFFLOAD_STAGE_NONE:
        // if needed, reset end of stage time
        if (offload_status.time_stage != UINT32_MAX)
        {
          offload_status.time_stage = UINT32_MAX;
          OffloadRemove ();
          json_update = true;
        }

        // save relay state
        offload_status.relay_state = bitRead (TasmotaGlobal.power, 0);

        // if overload is detected, start offload process
        if (offload_status.power_inst > power) next_stage = OFFLOAD_STAGE_BEFORE;
        break;

      // pending offloading
      case OFFLOAD_STAGE_BEFORE:
        // if needed, calculate end of stage time
        if (offload_status.time_stage == UINT32_MAX)
        {
          offload_status.time_stage = time_now + (uint32_t)offload_status.delay_before;
          json_update = true;
        }

        // save relay state
        offload_status.relay_state = bitRead (TasmotaGlobal.power, 0);

        // if house power has gone down, remove pending offloading
        if (offload_status.power_inst <= power) next_stage = OFFLOAD_STAGE_NONE;

        // else if delay is reached, set active offloading
        else if (offload_status.time_stage <= time_now) next_stage = OFFLOAD_STAGE_ACTIVE;
        break;

      // offloading is active
      case OFFLOAD_STAGE_ACTIVE:
        // if just started, reset timer and set relay offload
        if (offload_status.time_stage != UINT32_MAX)
        {
          offload_status.time_stage = UINT32_MAX;
          OffloadActivate ();
          json_update = true;
        }

        // calculate maximum power allowed when substracting device power
        if (power > offload_config.device_power) power -= offload_config.device_power;
        else power = 0;

        // if instant power is under this value, prepare to remove offload
        if (offload_status.power_inst <= power) next_stage = OFFLOAD_STAGE_AFTER;
        break;

      // actually just after offloading should stop
      case OFFLOAD_STAGE_AFTER:
        // if needed, calculate end of stage time
        if (offload_status.time_stage == UINT32_MAX)
        {
          offload_status.time_stage = time_now + (uint32_t)offload_status.delay_after;
          json_update = true;
        }

        // calculate maximum power allowed when substracting device power
        if (power > offload_config.device_power) power -= offload_config.device_power;
        else power = 0;

        // if house power has gone again too high, offloading back again
        if (offload_status.power_inst > power) next_stage = OFFLOAD_STAGE_ACTIVE;
        
        // else if delay is reached, set remove offloading
        else if (offload_status.time_stage <= time_now) next_stage = OFFLOAD_STAGE_NONE;
        break;
    }

    // update offloading state and send MQTT status if needed
    offload_status.stage = next_stage;
    if (next_stage != prev_stage) json_update = true;
  }
}

// check if MQTT message should be sent
void OffloadEverySecond ()
{
  uint32_t time_now;

  // get current time
  time_now = LocalTime ();

#ifdef USE_ENERGY_SENSOR
  // if current is detected (more than eq 20w), set power sensor flag
  if (Energy.current[0] > 0.1) offload_status.power_sensor = true;

  // if power sensor present
  if (offload_status.power_sensor)
  {
    float    power;
    uint16_t apparent_power;
    uint16_t new_power = UINT16_MAX;

    // record apparent power if greater than previous reference
    power = Energy.voltage[0] * Energy.current[0];
    apparent_power = (uint16_t)power;

    // calculate power evolution
    if (offload_config.device_power < apparent_power) new_power = apparent_power;
      else if (apparent_power < offload_config.device_power / 2) new_power = apparent_power;

    // if power has changed
    if (new_power != UINT16_MAX)
    {
      // update device power value
      offload_config.device_power = new_power;

      // update
      offload_status.time_update = time_now + OFFLOAD_POWER_UPDATE;
      offload_status.time_json   = time_now; 
    }

    // else, if needed, update power value
    else if (offload_status.time_update == UINT32_MAX) offload_status.time_update = time_now + OFFLOAD_POWER_UPDATE;

    // else, if timer is reached, update power
    else if (offload_status.time_update < time_now)
    {
      // update max value
      offload_config.device_power = apparent_power;

      // update
      offload_status.time_update = time_now + OFFLOAD_POWER_UPDATE;
      offload_status.time_json   = time_now; 
    }
  }
#endif

  // check if auto-rearm is active
  if (offload_status.relay_managed)
  {
    // if auto-rearm timeout is reached, switch back relay
    if (!OffloadIsOffloaded () && (offload_status.time_rearm != UINT32_MAX) && (offload_status.time_rearm < time_now))
    {
      offload_status.time_rearm = UINT32_MAX;
      ExecuteCommandPower (1, POWER_ON, SRC_MAX);
    }
  } 

  // if JSON needs to be updated
  if ((offload_status.time_json != UINT32_MAX) && (offload_status.time_json < time_now)) OffloadShowJSON (true);
}

// offload initialisation
void OffloadInit ()
{
  uint32_t index;

  // disable fast cycle power recovery
  Settings->flag3.fast_power_cycle_disable = true;

  // init available devices list
  for (index = 0; index < OFFLOAD_DEVICE_MAX; index++) OffloadAddAvailableType (index);

  // load configuration
  OffloadLoadConfig ();

  // init device current
  Energy.current[0] = 0;

  // set log history parameters
  LogFileSetFilename ("/offload-%u.log", UFS_LOG_PERIOD_YEAR);
  LogHistoSetDescription ("Offload Events", "Date;Time;Duration;Power", 4);
  LogHistoSetDateColumn (true, true, false, false, true);

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
  // if in managed mode, append control button
  if (offload_status.relay_managed) WSContentSend_P (OFFLOAD_BUTTON, D_PAGE_OFFLOAD_CONTROL, D_OFFLOAD_CONTROL);
}

// append offloading state to main page
void OffloadWebSensor ()
{
  uint16_t power;
  uint32_t time_now;
  uint32_t time_left = 0;
  TIME_T   time_dst;
  char     str_value[12];
  char     str_label[40];
  char     str_text[40];

  // current time
  time_now = LocalTime ();

  // display device type
  GetTextIndexed (str_text, sizeof (str_text), offload_config.device_type, kOffloadDevice);
  GetTextIndexed (str_value, sizeof (str_value), offload_config.device_type, kOffloadIcon);
  WSContentSend_PD (PSTR ("{s}%s %s{m}%u VA{e}"), str_text, str_value, offload_config.device_power);

  // if house power is subscribed, display power
  if (offload_config.str_topic.length () > 0)
  {
    // calculate delay since last power message
    strcpy_P (str_text, PSTR ("..."));
    if (offload_status.time_message > 0)
    {
      // calculate delay
      time_left = time_now - offload_status.time_message;
      BreakTime (time_left, time_dst);

      // generate readable format
      strcpy (str_text, "");
      if (time_dst.hour > 0)
      {
        sprintf_P (str_value, PSTR ("%dh"), time_dst.hour);
        strlcat (str_text, str_value, sizeof (str_text));
      }
      if (time_dst.hour > 0 || time_dst.minute > 0)
      {
        sprintf_P (str_value, PSTR ("%dm"), time_dst.minute);
        strlcat (str_text, str_value, sizeof (str_text));
      }
      sprintf_P (str_value, PSTR ("%ds"), time_dst.second);
      strlcat (str_text, str_value, sizeof (str_text));
    }

    // display current power and max power limit
    power = OffloadGetMaxPower ();
    if (power > 0) WSContentSend_PD (PSTR ("{s}%s <small><i>[%s]</i></small>{m}<b>%u</b> / %u VA{e}"), D_OFFLOAD_POWER, str_text, offload_status.power_inst, power);

    // get offload color and label
    GetTextIndexed (str_value, sizeof (str_value), offload_status.stage, kOffloadStageColor);
    GetTextIndexed (str_label, sizeof (str_label), offload_status.stage, kOffloadStageLabel);
    if (offload_status.time_stage != UINT32_MAX) time_left = offload_status.time_stage - time_now;
    sprintf_P (str_text, str_label , time_left);
    if (strlen (str_text) > 0) WSContentSend_PD (PSTR ("{s}%s{m}<span style='color:%s;'>%s</span>{e}"), D_OFFLOAD, str_value, str_text);

    // if auto rearm is active
  if (offload_status.relay_managed && !OffloadIsOffloaded () && (offload_status.time_rearm != UINT32_MAX))
    {
      if (offload_status.time_rearm < time_now) time_left = 0;
        else time_left = time_now - offload_status.time_rearm;
      WSContentSend_PD (PSTR ("{s}Auto rearm{m}%u sec.{e}"), time_left);
    }
  }
}

// Offload configuration web page
void OffloadWebPageConfig ()
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
    if (strlen (str_argument) > 0) OffloadValidateDeviceType ((uint16_t)atoi (str_argument));

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
      if (abs (value) < 100) offload_config.contract_adjust = value;
    }

    // set offloading device priority according to 'priority' parameter
    WebGetArg (D_CMND_OFFLOAD_PRIORITY, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0)
    {
      value = atoi (str_argument);
      if (value <= OFFLOAD_PRIORITY_MAX) offload_config.device_priority = (uint16_t)value;
    }

    // set MQTT topic according to 'topic' parameter
    WebGetArg (D_CMND_OFFLOAD_TOPIC, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) offload_config.str_topic = str_argument;

    // set JSON key according to 'inst' parameter
    WebGetArg (D_CMND_OFFLOAD_KINST, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) offload_config.str_kinst = str_argument;

    // set JSON key according to 'max' parameter
    WebGetArg (D_CMND_OFFLOAD_KMAX, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) offload_config.str_kmax = str_argument;

    // set shut-off duration according to 'rearm' parameter
    WebGetArg (D_CMND_OFFLOAD_REARM, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) offload_config.rearm_delay = (uint16_t)atoi (str_argument);

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
  for (index = 0; index < offload_config.nbr_device_type; index ++)
  {
    device = offload_config.arr_device_type[index];
    GetTextIndexed (str_default, sizeof (str_default), device, kOffloadDevice);
    GetTextIndexed (str_icon, sizeof (str_icon), device, kOffloadIcon);
    if (device == offload_config.device_type) strcpy_P (str_argument, PSTR (D_OFFLOAD_SELECTED)); else strcpy (str_argument, "");
    WSContentSend_P (PSTR ("<option value='%d' %s>%s %s</option>\n"), device, str_argument, str_icon, str_default);
  }
  WSContentSend_P (PSTR ("</select>\n"));
  WSContentSend_P (PSTR ("</p>\n"));

  // device power
  sprintf_P (str_argument, PSTR ("%s (%s)"), D_OFFLOAD_POWER, D_OFFLOAD_UNIT_VA);
  WSContentSend_P (OFFLOAD_INPUT_NUMBER_HALF, str_argument, D_CMND_OFFLOAD_PREFIX, D_CMND_OFFLOAD_POWER, D_CMND_OFFLOAD_POWER, D_CMND_OFFLOAD_POWER, 0, 65000, 10, offload_config.device_power);

  // priority
  sprintf_P (str_argument, PSTR ("%s"), D_OFFLOAD_PRIORITY);
  WSContentSend_P (OFFLOAD_INPUT_NUMBER_SWITCH, str_argument, D_CMND_OFFLOAD_PREFIX, D_CMND_OFFLOAD_PRIORITY, D_CMND_OFFLOAD_PRIORITY, D_CMND_OFFLOAD_PRIORITY, 1, OFFLOAD_PRIORITY_MAX, 1, offload_config.device_priority);

  WSContentSend_P (OFFLOAD_FIELDSET_STOP);

  // ------------
  //    Meter  
  // ------------

  WSContentSend_P (OFFLOAD_FIELDSET_START, "⚡", D_OFFLOAD_METER);

  // contract power
  sprintf_P (str_argument, PSTR ("%s (%s)"), D_OFFLOAD_MAX, D_OFFLOAD_UNIT_VA);
  WSContentSend_P (OFFLOAD_INPUT_NUMBER_HALF, str_argument, D_CMND_OFFLOAD_PREFIX, D_CMND_OFFLOAD_MAX, D_CMND_OFFLOAD_MAX, D_CMND_OFFLOAD_MAX, 0, 65000, 1, offload_config.contract_power);

  // contract adjustment
  sprintf_P (str_argument, PSTR ("%s (%s)"), D_OFFLOAD_ADJUST, D_OFFLOAD_UNIT_PERCENT);
  WSContentSend_P (OFFLOAD_INPUT_NUMBER_HALF, str_argument, D_CMND_OFFLOAD_PREFIX, D_CMND_OFFLOAD_ADJUST, D_CMND_OFFLOAD_ADJUST, D_CMND_OFFLOAD_ADJUST, -99, 100, 1, offload_config.contract_adjust);

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
  WSContentSend_P (OFFLOAD_INPUT_NUMBER, PSTR ("Auto Rearm delay (s.)"), D_CMND_OFFLOAD_PREFIX, D_CMND_OFFLOAD_REARM, D_CMND_OFFLOAD_REARM, D_CMND_OFFLOAD_REARM, 0, 900, 5, (int)offload_config.rearm_delay);
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

#ifdef USE_OFFLOAD_WEB

// get status update
void OffloadWebUpdate ()
{
  char str_text[16];

  // update switch status
  if (offload_status.relay_state == 1) strcpy (str_text, "true");
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
    GetTextIndexed (str_text, sizeof (str_text), offload_config.device_type, kOffloadIcon);
    WSContentSend_P (PSTR ("%s"), str_text);
  }

  // appliance logo : end
  if (is_logged) WSContentSend_P (PSTR ("</a>"));
  WSContentSend_P (PSTR ("</div>\n"));

  // display switch button
  if (offload_status.relay_state == 1) strcpy (str_text, "checked"); else strcpy (str_text, "");
  WSContentSend_P (PSTR ("<div class='section'><input class='toggle' type='checkbox' id='switch' name='switch' onclick='document.forms.control.submit();' %s /></div>\n"), str_text);

  // hidden new state
  WSContentSend_P (PSTR ("<input type='hidden' name='choice' value='%u' />\n"), !offload_status.relay_state);

  // end of page
  WSContentSend_P (PSTR ("</form>\n"));
  WSContentStop ();
}

#endif  // USE_OFFLOAD_WEB

#endif  // USE_WEBSERVER

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xdrv96 (uint8_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
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
      if (TasmotaGlobal.uptime > 5) OffloadEverySecond ();
      break;
    case FUNC_MQTT_SUBSCRIBE:
      OffloadMqttSubscribe ();
      SensorMqttSubscribe ();
      break;
    case FUNC_MQTT_DATA:
      result = OffloadMqttData ();
      result |= SensorMqttData ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      //pages
      Webserver->on ("/" D_PAGE_OFFLOAD_CONFIG, OffloadWebPageConfig);

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
      WSContentSend_P (OFFLOAD_BUTTON, D_PAGE_OFFLOAD_CONFIG, D_OFFLOAD_CONFIGURE " " D_OFFLOAD);
      break;
    case FUNC_WEB_SENSOR:
      OffloadWebSensor ();
      break;
#endif  // USE_WEBSERVER
  }
  
  return result;
}

#endif    // USE_OFFLOAD
#endif    // FIRMWARE_SAFEBOOT