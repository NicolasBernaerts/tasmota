/*
  xdrv_98_20_teleinfo.ino - France Teleinfo energy sensor support for Tasmota devices

  Copyright (C) 2019-2025 Nicolas Bernaerts

  Version history :
    24/01/2024 v1.0 - Creation (split from xnrg_15_teleinfo.ino)
    05/02/2024 v1.1 - Use FUNC_SHOW_SENSOR & FUNC_JSON_APPEND for JSON publication
    13/01/2024 v2.0 - Complete rewrite of Contrat and Period management
                      Add Emeraude 2 meter management
                      Add calendar and virtual relay management
                      Activate serial reception when NTP time is ready
                      Change MQTT publication and data reception handling to minimize errors
                      Handle Linky supplied Winky with deep sleep 
                      Support various temperature sensors
                      Add Domoticz topics publication
    03/01/2024 v2.1 - Add alert management thru STGE
    15/01/2024 v2.2 - Add support for Denky (thanks to C. Hallard prototype)
                      Add Emeraude 2 meter management
                      Add calendar and virtual relay management
    25/02/2024 v3.0 - Complete rewrite of Contrat and Period management
                      Activate serial reception when NTP time is ready
                      Change MQTT publication and data reception handling to minimize errors
                      Support various temperature sensors
                      Add Domoticz topics publication (idea from Sebastien)
                      Add support for Wenky with deep sleep (thanks to C. Hallard prototype)
                      Lots of bug fixes (thanks to B. Monot and Sebastien)
    05/03/2024 v14.0 - Removal of all float and double calculations
    27/03/2024 v14.1 - Section COUNTER renamed as CONTRACT with addition of contract data
    21/04/2024 v14.3 - Add Homie integration
    21/05/2024 v14.4 - Group all sensor data in a single frame
                        Publish Teleinfo raw data under /TIC instead of /SENSOR
    01/06/2024 v14.5 - Add contract auto-discovery for unknown Standard contracts
    28/06/2024 v14.6 - Change in calendar JSON (for compliance)
                       Add counter serial number in CONTRACT:serial
                       Add global conso counter in CONTRACT:CONSO
                       Remove all String for ESP8266 stability
    30/06/2024 v14.7 - Add virtual and physical reception status LED (WS2812)
                       Add commands full and noraw (compatibility with official version)
                       Always publish CONTRACT data with METER and PROD
    19/08/2024 v14.8 - Increase ESP32 reception buffer to 8192 bytes
                       Redesign of contract management and auto-discovery
                       Rewrite of periods management by meter type
                       Add contract change detection on main page
                       Optimisation of serial reception to minimise errors
                       Add command 'tic' to publish raw TIC data
                       Correct CONTRACT/tday and tmrw publication bug
    05/01/2025 v14.9 - Add indexes and totals to InfluxDB data
                       Update color from RTE according to contract type (Tempo or EJP)
    30/04/2025 v14.10 - Drop data before wifi connexion to protect single core ESP boot process 
    01/06/2025 v14.11 - Add period profile
    10/07/2025 v15.0  - Switch all module to driver to minimize indexes
                        Refactoring based on Tasmota 15
                        Handle display for generic PME/PMI, Emeraude and Jaune meters
                        Hide and Show with click on main page displays
    01/10/2025 v15.1  - Save data to teleinfo-driver.dat
                        Adapt to FUNC_JSON_APPEND call every second
                        Rework data structure initialisation
                        Rewrite RGB LED management
  
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or ENERGY_WATCHDOG FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef USE_ENERGY_SENSOR
#ifdef USE_TELEINFO

/*********************************************\
 *               Helper functions
\*********************************************/

// Display any value to a string with unit and kilo conversion with number of digits (12000, 2.0k, 12.0k, 2.3M, ...)
void TeleinfoGetFormattedValue (const int unit_type, const long value, char* pstr_result, const int size_result, bool in_kilo) 
{
  char str_text[16];

  // check parameters
  if (pstr_result == nullptr) return;

  // convert cos phi value
  if (unit_type == TELEINFO_UNIT_COS) sprintf_P (pstr_result, PSTR ("%d.%02d"), value / 100, value % 100);

  // else convert values in M with 1 digit
  else if (in_kilo && (value > 999999)) sprintf_P (pstr_result, PSTR ("%d.%dM"), value / 1000000, (value / 100000) % 10);

  // else convert values in k with no digit
  else if (in_kilo && (value > 9999)) sprintf_P (pstr_result, PSTR ("%dk"), value / 1000);

  // else convert values in k
  else if (in_kilo && (value > 999)) sprintf_P (pstr_result, PSTR ("%d.%dk"), value / 1000, (value / 100) % 10);

  // else convert value
  else sprintf_P (pstr_result, PSTR ("%d"), value);

  // append unit if specified
  if (unit_type < TELEINFO_UNIT_MAX) 
  {
    GetTextIndexed (str_text, sizeof (str_text), unit_type, kTeleinfoUnit);
    strlcat (pstr_result, " ", size_result);
    strlcat (pstr_result, str_text, size_result);
  }
}

// Get specific argument as a value with min and max
int TeleinfoGetArgValue (const char* pstr_argument, const int value_min, const int value_max, const int value_default)
{
  int  value = value_default;
  char str_argument[8];

  // check for argument
  if (Webserver->hasArg (pstr_argument))
  {
    WebGetArg (pstr_argument, str_argument, sizeof (str_argument));
    value = atoi (str_argument);

    // check for min and max value
    value = max (value, value_min);
    value = min (value, value_max);
  }

  return value;
}

/*************************************************\
 *                LED light
\*************************************************/

// get LED status
uint8_t TeleinfoDriverLedGetStatus ()
{
  uint8_t result;

  // check wifi connexion
  if (Wifi.status != WL_CONNECTED) result = TIC_LED_STEP_WIFI;

  // else if no meter reception from beginning
  else if (teleinfo_meter.nb_message < TIC_MESSAGE_MIN) result = TIC_LED_STEP_NODATA;

  // else if no meter reception after timeout
  else if (TimeReached (teleinfo_message.timestamp_last + TELEINFO_RECEPTION_TIMEOUT)) result = TIC_LED_STEP_ERR;

  // else reception is ok
  else result = TIC_LED_STEP_OK;

  return result;
}

#ifdef USE_LIGHT

// switch LED
void TeleinfoDriverLedSwitch (const uint8_t status, const bool target)
{
  uint8_t level;
  uint8_t current_red, current_green, current_blue;
  uint8_t target_red,  target_green,  target_blue;

  // if switch OFF
  if (!target) ExecuteCommandPower (Light.device, POWER_OFF_NO_STATE, SRC_LIGHT);

  // else if switch ON
  else
  {
    // get current color
    light_state.getRGB (&current_red, &current_green, &current_blue);

    // set level according to status
    switch (status)
    {
      case TIC_LED_STEP_WIFI:   level = TIC_LEVEL_NONE; break;
      case TIC_LED_STEP_NODATA: level = TIC_LEVEL_NONE; break;
      default: if (teleinfo_prod.papp > 0) level = TIC_LEVEL_PROD; else level = TeleinfoPeriodGetLevel (); break;
    }

    // set target color according to level
    target_red   = arrTeletinfoLedColor[level][0];
    target_green = arrTeletinfoLedColor[level][1];
    target_blue  = arrTeletinfoLedColor[level][2];

    // if color has changed
    if ((current_red != target_red) || (current_green != target_green) || (current_blue != target_blue))
    {
      // log
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Change LED from [%u,%u,%u] to [%u,%u,%u]"), current_red, current_green, current_blue, target_red, target_green, target_blue);

      // change color
      current_red   = target_red;
      current_green = target_green;
      current_blue  = target_blue;
      light_controller.changeRGB (current_red, current_green, current_blue, true);
    }

    // switch ON
    ExecuteCommandPower (Light.device, POWER_ON_NO_STATE, SRC_LIGHT);
  }
}

// update LED status
void TeleinfoDriverLedUpdate ()
{
  uint8_t  status;
  uint32_t timestamp;

  // if not declared, ignore
  if (!teleinfo_led.present) return;
  if (!teleinfo_config.led) return;

  // if needed, init LED timestamp
  if (teleinfo_led.upd_stamp == UINT32_MAX) teleinfo_led.upd_stamp = millis ();

  // get current status
  status = TeleinfoDriverLedGetStatus ();

  // calculate next switching time according to LED state
  if (teleinfo_led.state) timestamp = teleinfo_led.upd_stamp + arrTicLedOn[status]; 
    else timestamp = teleinfo_led.upd_stamp + arrTicLedOff[status];

  // if LED should be toggled
  if (TimeReached (timestamp))
  {
    // set next state
    teleinfo_led.state     = !teleinfo_led.state;
    teleinfo_led.upd_stamp = millis ();

    // update LED
    TeleinfoDriverLedSwitch (status, teleinfo_led.state);
  }
}

#endif    // USE_LIGHT

/*************************************************\
 *                  Commands
\*************************************************/

// send sensor stream
void CmndTeleinfoDriverTIC ()
{
  teleinfo_meter.json.tic = true;
  ResponseCmndDone ();
}

/*************************************************\
 *                  Configuration
\*************************************************/

// Load configuration from Settings or from LittleFS
void TeleinfoDriverLoadSettings ()
{
  uint8_t index;

  // load standard settings
  teleinfo_config.display      = (Settings->teleinfo.display  == 0);
  teleinfo_config.meter        = (Settings->teleinfo.meter    == 0);
  teleinfo_config.relay        = (Settings->teleinfo.relay    != 0);
  teleinfo_config.calendar     = (Settings->teleinfo.calendar != 0);
  teleinfo_config.tic          = (Settings->teleinfo.tic      != 0);
  teleinfo_config.energy       = (Settings->teleinfo.energy   != 0);
  teleinfo_config.live         = (Settings->teleinfo.live     != 0);
  teleinfo_config.cal_hexa     = (Settings->teleinfo.cal_hexa != 0);
  teleinfo_config.led          = (Settings->teleinfo.led      != 0);
  teleinfo_config.percent      = Settings->teleinfo.percent;
  teleinfo_config.skip         = Settings->teleinfo.skip;
  teleinfo_config.policy       = min ((uint)Settings->teleinfo.policy, (uint)(TIC_POLICY_MAX - 1));
  teleinfo_config.prod_max     = (long)Settings->energy_max_power_limit;
  teleinfo_config.prod_trigger = 50 * (long)Settings->rf_code[16][6];
  teleinfo_config.brightness   = max (Settings->rf_code[16][7], (uint8_t)100);

  // load display flags
  for (index = 0; index < 9; index ++) teleinfo_config.arr_main[index] = !(bool)Settings->rf_code[10][index];
  for (index = 0; index < TIC_DISPLAY_MAX - 9; index ++) teleinfo_config.arr_main[index + 9] = !(bool)Settings->rf_code[11][index];
  
  // validate boundaries
  if (teleinfo_config.skip == 0) teleinfo_config.skip = 3;
  if (teleinfo_config.percent < TIC_PERCENT_MIN) teleinfo_config.percent = 100;
  if (teleinfo_config.percent > TIC_PERCENT_MAX) teleinfo_config.percent = 100;
}

// Save configuration to Settings or to LittleFS
void TeleinfoDriverSaveSettings () 
{
  uint8_t index;

  // save standard settings
  Settings->teleinfo.display  = (uint8_t)!teleinfo_config.display;
  Settings->teleinfo.meter    = (uint8_t)!teleinfo_config.meter;
  Settings->teleinfo.energy   = (uint8_t)teleinfo_config.energy;
  Settings->teleinfo.tic      = (uint8_t)teleinfo_config.tic;
  Settings->teleinfo.live     = (uint8_t)teleinfo_config.live;
  Settings->teleinfo.calendar = (uint8_t)teleinfo_config.calendar;
  Settings->teleinfo.relay    = (uint8_t)teleinfo_config.relay;
  Settings->teleinfo.cal_hexa = (uint8_t)teleinfo_config.cal_hexa;
  Settings->teleinfo.led      = (uint8_t)teleinfo_config.led;
  Settings->teleinfo.percent  = teleinfo_config.percent;
  Settings->teleinfo.policy   = teleinfo_config.policy;
  Settings->teleinfo.skip     = teleinfo_config.skip;
  Settings->rf_code[16][6]    = (uint8_t)(teleinfo_config.prod_trigger / 50);
  Settings->energy_max_power_limit = (uint16_t)teleinfo_config.prod_max;

  // save display flags
  for (index = 0; index < 9; index ++) Settings->rf_code[10][index] = (uint8_t)(!teleinfo_config.arr_main[index]);
  for (index = 0; index < TIC_DISPLAY_MAX - 9; index ++) Settings->rf_code[11][index] = (uint8_t)(!teleinfo_config.arr_main[index + 9]);

  // save calendar
  TeleinfoCalendarSaveToSettings ();

  // save settings
  SettingsSave (0);
}

void TeleinfoDriverLoadData ()
{
#ifdef USE_UFILESYS
  uint8_t version;
  char    str_filename[24];
  File    file;

  // open file in read mode
  strcpy_P (str_filename, PSTR_DRIVER_DATA_FILE);
  if (ffsp->exists (str_filename))
  {
    file = ffsp->open (str_filename, "r");
    file.read ((uint8_t*)&version, sizeof (version));
    if (version == TIC_DATA_VERSION)
    {
      file.read ((uint8_t*)&teleinfo_contract_db, sizeof (teleinfo_contract_db));
      file.read ((uint8_t*)&teleinfo_conso_wh,    sizeof (teleinfo_conso_wh));
      file.read ((uint8_t*)&teleinfo_prod_wh,     sizeof (teleinfo_prod_wh));
    }
    file.close ();
  }
#endif    // USE_UFILESYS
}

void TeleinfoDriverSaveData ()
{
#ifdef USE_UFILESYS
  uint8_t version;
  char    str_filename[24];
  File    file;

  // init data
  version = TIC_DATA_VERSION;

  // open file in write mode
  strcpy_P (str_filename, PSTR_DRIVER_DATA_FILE);
  file = ffsp->open (str_filename, "w");
  if (file > 0)
  {
    file.write ((uint8_t*)&version, sizeof (version));
    file.write ((uint8_t*)&teleinfo_contract_db, sizeof (teleinfo_contract_db));
    file.write ((uint8_t*)&teleinfo_conso_wh,    sizeof (teleinfo_conso_wh));
    file.write ((uint8_t*)&teleinfo_prod_wh,     sizeof (teleinfo_prod_wh));
    file.close ();
  }
#endif    // USE_UFILESYS
}

/*************************************************\
 *                  Functions
\*************************************************/

/*
uint32_t TeleinfoDriverWebGetDelay (const uint8_t type)
{
  // check parameter
  if (type >= TIC_WEB_MAX) return 0;

  return arrTeleinfoWebDelay[type];
}
*/

bool TeleinfoDriverWebAllow (const uint8_t type)
{
  uint8_t index;

  // check time and parameter
  if (type >= TIC_WEB_MAX) return false;

  // if needed, init timestamp
  for (index = 0; index < TIC_WEB_MAX; index ++) if (teleinfo_meter.arr_web_ts[index] == 0) teleinfo_meter.arr_web_ts[index] = millis () + arrTeleinfoWebDelay[index];

  // check if timeout has been reached
  return (TimeDifference (teleinfo_meter.arr_web_ts[type], millis()) >= 0);
}

void TeleinfoDriverWebDeclare (const uint8_t type)
{
  // check time and parameter
  if (type >= TIC_WEB_MAX) return;

  // set timestamp
  switch (type)
  {
    // new MQTT publication declared
    case TIC_WEB_MQTT:
      teleinfo_meter.arr_web_ts[TIC_WEB_MQTT] = millis () + arrTeleinfoWebDelay[TIC_WEB_MQTT];
      if (TimeDifference (teleinfo_meter.arr_web_ts[TIC_WEB_MQTT], teleinfo_meter.arr_web_ts[TIC_WEB_HTTP])  < 0) teleinfo_meter.arr_web_ts[TIC_WEB_HTTP]  = teleinfo_meter.arr_web_ts[TIC_WEB_MQTT];
      if (TimeDifference (teleinfo_meter.arr_web_ts[TIC_WEB_MQTT], teleinfo_meter.arr_web_ts[TIC_WEB_HTTPS]) < 0) teleinfo_meter.arr_web_ts[TIC_WEB_HTTPS] = teleinfo_meter.arr_web_ts[TIC_WEB_MQTT];
      break;

    // new HTTP access declared
    case TIC_WEB_HTTP:
      teleinfo_meter.arr_web_ts[TIC_WEB_MQTT] = millis () + arrTeleinfoWebDelay[TIC_WEB_MQTT];
      teleinfo_meter.arr_web_ts[TIC_WEB_HTTP] = millis () + arrTeleinfoWebDelay[TIC_WEB_HTTP];
      if (TimeDifference (teleinfo_meter.arr_web_ts[TIC_WEB_HTTP], teleinfo_meter.arr_web_ts[TIC_WEB_HTTPS]) < 0) teleinfo_meter.arr_web_ts[TIC_WEB_HTTPS] = teleinfo_meter.arr_web_ts[TIC_WEB_HTTP];
      break;

    // new HTTPs access declared
    case TIC_WEB_HTTPS:
      teleinfo_meter.arr_web_ts[TIC_WEB_MQTT]  = millis () + arrTeleinfoWebDelay[TIC_WEB_MQTT];
      teleinfo_meter.arr_web_ts[TIC_WEB_HTTPS] = millis () + arrTeleinfoWebDelay[TIC_WEB_HTTPS];
      if (TimeDifference (teleinfo_meter.arr_web_ts[TIC_WEB_HTTPS], teleinfo_meter.arr_web_ts[TIC_WEB_HTTP]) < 0) teleinfo_meter.arr_web_ts[TIC_WEB_HTTP] = teleinfo_meter.arr_web_ts[TIC_WEB_HTTPS];
      break;
  }
}

void TeleinfoDriverRemoveSpace (char* pstr_source)
{
  char *pstr_destination;

  // check parameter
  if (pstr_source == nullptr) return;

  // loop to remove spaces
  pstr_destination = pstr_source;
  while (*pstr_source != 0)
  {
    if (*pstr_source != ' ')
    { 
      *pstr_destination = *pstr_source;
      pstr_destination++;
    }
    pstr_source++;
  }
  *pstr_destination = 0;
}

bool TeleinfoDriverIsPowered ()   { return !teleinfo_config.battery; }
bool TeleinfoDriverIsOnBattery () { return  teleinfo_config.battery; }

void TeleinfoDriverSetBattery (const bool battery) 
{
  teleinfo_config.battery = battery;
}

// data are ready to be published
bool TeleinfoDriverMeterReady () 
{
  return (teleinfo_meter.nb_message >= TIC_MESSAGE_MIN);
}

// get production relay index
bool TeleinfoDriverGetProductionRelay () 
{
  return teleinfo_prod.relay;
}

// get current contract period
uint8_t TeleinfoDriverGetPeriod () 
{
  return teleinfo_contract.period;
}

// get number of periods in contract
uint8_t TeleinfoDriverGetPeriodQuantity () 
{
  return teleinfo_contract_db.period_qty;
}

// get production relay power trigger
long TeleinfoDriverGetProductionRelayTrigger () 
{
  return teleinfo_config.prod_trigger;
}

/*************************************************\
 *                  Calendar
\*************************************************/

// get hour slot hc / hp
uint8_t TeleinfoDriverCalendarGetHP (const uint8_t day, const uint8_t slot)
{
  uint8_t hchp;

  // init to HP
  hchp = 1;

  // check parameters
  if (day >= TIC_DAY_MAX) return hchp;
  if (slot >= TIC_DAY_SLOT_MAX) return hchp;
  
  // if day is valid, get level according to current period
  if (teleinfo_calendar[day].level > TIC_LEVEL_NONE) hchp = TeleinfoPeriodGetHP (teleinfo_calendar[day].arr_slot[slot]);

  return hchp;
}

// get hour slot level
uint8_t TeleinfoDriverCalendarGetLevel (const uint8_t day, uint8_t slot, const bool use_rte)
{
  uint8_t level;

  // init level to unknown
  level = TIC_LEVEL_NONE;

  // check parameters
  if (day >= TIC_DAY_MAX) return level;

  // if level is asked for the day, set slot to 12h
  if (slot == UINT8_MAX) slot = 12 * 2;

  // if level is not set, get level at 12h
  level = TeleinfoPeriodGetLevel (teleinfo_calendar[day].arr_slot[slot]);

#ifdef USE_TELEINFO_RTE
  if (use_rte && TeleinfoRteTempoIsEnabled ()  && ((teleinfo_contract_db.type == TIC_C_HIS_TEMPO) || (teleinfo_contract_db.type == TIC_C_STD_TEMPO))) level = TeleinfoRteTempoGetGlobalLevel  (day + 1, slot / 2);
  if (use_rte && TeleinfoRtePointeIsEnabled () && ((teleinfo_contract_db.type == TIC_C_HIS_EJP)   || (teleinfo_contract_db.type == TIC_C_STD_EJP)))   level = TeleinfoRtePointeGetGlobalLevel (day + 1, slot / 2);
#endif      // USE_TELEINFO_RTE

  return level;
}

// get hour slot level
void TeleinfoDriverCalendarGetDate (const uint32_t delta, char *pstr_date, const size_t size_date)
{
  TIME_T date_dst;

  // check parameters
  if (pstr_date == nullptr) return;
  if (size_date < 8) return;
  
  // handle according to delta
  switch (delta)
  {
    case 0: strcpy_P (pstr_date, PSTR ("Aujourd.")); break;
    case 1: strcpy_P (pstr_date, PSTR ("Demain"));   break;
    default:
      BreakTime (LocalTime () + delta * 86400, date_dst);
      sprintf_P (pstr_date, PSTR ("%02u/%02u"), date_dst.day_of_month, date_dst.month);
      break;
  }
}

/*************************************************\
 *               JSON publication
\*************************************************/

// Trigger publication flags
void TeleinfoDriverJsonPublish ()
{
  // message start
  ResponseClear ();
  if (RtcTime.valid) ResponseAppendTime ();
    else ResponseAppend_P (PSTR ("{"));

  // populate message
  TeleinfoDriverJsonAppend ();

  // message end and publication
  ResponseJsonEnd ();
  MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR),  Settings->flag.mqtt_sensor_retain);
  XdrvRulesProcess (true);

  // reset flag and declare publication
  teleinfo_meter.json.data = false;
  TeleinfoDriverWebDeclare (TIC_WEB_MQTT);
}

// Generate LIVE JSON
void TeleinfoDriverPublishLive ()
{
  // start of message
  ResponseClear ();
  ResponseAppend_P (PSTR ("{"));

  // message content
  TeleinfoDriverAppendMeterSensor ();

  // end of message
  ResponseJsonEnd ();

  // message publication
  MqttPublishPrefixTopicRulesProcess_P (TELE, PSTR ("LIVE"), false);

  // reset JSON flag and declare publication
  teleinfo_meter.json.live = false;
  TeleinfoDriverWebDeclare (TIC_WEB_MQTT);
}

// Generate TIC full JSON
void TeleinfoDriverPublishTic ()
{
  bool    is_first = true;
  uint8_t index;

  // start of message
  ResponseClear ();
  ResponseAppend_P (PSTR ("{"));

  // loop thru TIC message lines to add lines
  for (index = 0; index < TIC_LINE_QTY; index ++)
    if (teleinfo_message.arr_last[index].checksum != 0)
    {
      if (!is_first) ResponseAppend_P (PSTR (",")); else is_first = false;
      ResponseAppend_P (PSTR ("\"%s\":\"%s\""), teleinfo_message.arr_last[index].str_etiquette, teleinfo_message.arr_last[index].str_donnee);
    }

  // end of message
  ResponseJsonEnd ();

  // message publication
  MqttPublishPrefixTopic_P (TELE, PSTR ("TIC"),  false);
  XdrvRulesProcess (true);

  // reset JSON flag and declare publication
  teleinfo_meter.json.tic = false;
  TeleinfoDriverWebDeclare (TIC_WEB_MQTT);
}

// Append ALERT to JSON
void TeleinfoDriverAppendAlert ()
{
  bool    is_active;
  uint8_t index;
  char    str_key[8];
  char    str_source[TIC_ALERT_SRC_SIZE];

  // start of section
  ResponseAppend_P (PSTR (",\"ALERT\":{"));

  // loop thru alerts
  for (index = 0; index < TIC_ALERT_MAX; index ++)
  {
    // get alert data
    GetTextIndexed (str_key, sizeof (str_key), index, kTeleinfoAlert);
    TeleinfoDriverAlertGetSource (index, str_source);

    // append alert
    if (index > 0) ResponseAppend_P (PSTR (","));
    ResponseAppend_P (PSTR ("\"%s\":\"%s\""), str_key, str_source);
  }

  // end of section
  ResponseAppend_P (PSTR ("}"));
}

// Append METER and PROD to JSON
void TeleinfoDriverAppendMeterSensor ()
{
  uint8_t phase, value;
  long    voltage, current, power_app, power_act;

  // METER basic data
  MiscOptionPrepareJsonSection ();
  ResponseAppend_P (PSTR ("\"METER\":{\"PH\":%u,\"ISUB\":%u,\"PSUB\":%u,\"PMAX\":%d"), teleinfo_contract.phase, teleinfo_contract.isousc, teleinfo_contract.ssousc, (long)teleinfo_config.percent * teleinfo_contract.ssousc / 100);

  // conso 
  if (teleinfo_conso.enabled)
  {
    // conso : loop thru phases
    voltage = 0;
    current = 0;
    power_app = 0;
    power_act = 0;
    for (phase = 0; phase < teleinfo_contract.phase; phase++)
    {
      // calculate parameters
      voltage   += teleinfo_conso.phase[phase].voltage;
      current   += teleinfo_conso.phase[phase].current;
      power_app += teleinfo_conso.phase[phase].papp;
      power_act += teleinfo_conso.phase[phase].pact;

      // if needed, publish phase data
      value = phase + 1;
      ResponseAppend_P (PSTR (",\"U%u\":%d,\"I%u\":%d.%02d"), value, teleinfo_conso.phase[phase].voltage, value, teleinfo_conso.phase[phase].current / 1000, teleinfo_conso.phase[phase].current % 1000 / 10);
      ResponseAppend_P (PSTR (",\"P%u\":%d,\"W%u\":%d"), value, teleinfo_conso.phase[phase].papp, value, teleinfo_conso.phase[phase].pact);
    } 

    // conso : values and cosphi
    if (teleinfo_contract.phase > 1) voltage = voltage / (long)teleinfo_contract.phase;
    ResponseAppend_P (PSTR (",\"U\":%d,\"I\":%d.%02d,\"P\":%d,\"W\":%d"), voltage, current / 1000, current % 1000 / 10, power_app, power_act);
    if (teleinfo_conso.cosphi.quantity >= TIC_COSPHI_MIN) ResponseAppend_P (PSTR (",\"C\":%d.%02d"), teleinfo_conso.cosphi.value / 1000, teleinfo_conso.cosphi.value % 1000 / 10);

    // conso : if not on battery, publish total of yesterday and today
    if (TeleinfoDriverIsPowered ())
    {
      ResponseAppend_P (PSTR (",\"YDAY\":%d"), teleinfo_conso_wh.yesterday);
      ResponseAppend_P (PSTR (",\"TDAY\":%d"), teleinfo_conso_wh.today);
    }
  }
  
  // production 
  if (teleinfo_prod.enabled)
  {
    // prod : global values
    ResponseAppend_P (PSTR (",\"PP\":%d,\"PW\":%d"), teleinfo_prod.papp, teleinfo_prod.pact);

    // prod : cosphi
    if (teleinfo_prod.cosphi.quantity >= TIC_COSPHI_MIN) ResponseAppend_P (PSTR (",\"PC\":%d.%02d"), teleinfo_prod.cosphi.value / 1000, teleinfo_prod.cosphi.value % 1000 / 10);

    // prod : average power
    ResponseAppend_P (PSTR (",\"PAVG\":%d"), (long)teleinfo_prod.pact_avg);

    // prod : if not on battery, publish total of yesterday and today
    if (TeleinfoDriverIsPowered ())
    {
      ResponseAppend_P (PSTR (",\"PYDAY\":%d"), teleinfo_prod_wh.yesterday);
      ResponseAppend_P (PSTR (",\"PTDAY\":%d"), teleinfo_prod_wh.today);
    }
  } 
  ResponseJsonEnd ();
}

// Append CONTRACT to JSON
void TeleinfoDriverAppendContractSensor ()
{
  uint8_t index;
  char    str_value[32];
  char    str_period[32];

  // section CONTRACT
  MiscOptionPrepareJsonSection ();
  ResponseAppend_P (PSTR ("\"CONTRACT\":{"));

  // meter serial number
  lltoa (teleinfo_meter.ident, str_value, 10);
  ResponseAppend_P (PSTR ("\"serial\":\"%s\""), str_value);

  // contract name
  TeleinfoContractGetName (str_value, sizeof (str_value));
  ResponseAppend_P (PSTR (",\"name\":\"%s\""), str_value);

  // contract period
  TeleinfoPeriodGetLabel (str_value, sizeof (str_value));
  ResponseAppend_P (PSTR (",\"period\":\"%s\""), str_value);

  // period profile
  TeleinfoPeriodGetProfile (str_value, sizeof (str_value));
  ResponseAppend_P (PSTR (",\"profile\":\"%s\""), str_value);

  // period color
  index = TeleinfoPeriodGetLevel ();
  GetTextIndexed (str_value, sizeof (str_value), index, kTeleinfoLevelLabel);
  ResponseAppend_P (PSTR (",\"color\":\"%s\""), str_value);

  // period type
  index = TeleinfoPeriodGetHP ();
  GetTextIndexed (str_value, sizeof (str_value), index, kTeleinfoHourLabel);
  ResponseAppend_P (PSTR (",\"hour\":\"%s\""), str_value);

  // level today at 12h
  index = TeleinfoDriverCalendarGetLevel (TIC_DAY_TODAY, UINT8_MAX, false);
  GetTextIndexed (str_value, sizeof (str_value), index, kTeleinfoLevelLabel);
  ResponseAppend_P (PSTR (",\"tday\":\"%s\""), str_value);

  // level tomorrow at 12h
  index = TeleinfoDriverCalendarGetLevel (TIC_DAY_TMROW, UINT8_MAX, false);
  GetTextIndexed (str_value, sizeof (str_value), index, kTeleinfoLevelLabel);
  ResponseAppend_P (PSTR (",\"tmrw\":\"%s\""), str_value);

  // total conso counter
  lltoa (teleinfo_conso_wh.total, str_value, 10);
  ResponseAppend_P (PSTR (",\"CONSO\":%s"), str_value);

  // loop to publish conso counters
  for (index = 0; index < teleinfo_contract_db.period_qty; index ++)
  {
    TeleinfoPeriodGetCode (str_period, sizeof (str_period), index);
    lltoa (teleinfo_conso_wh.index[index], str_value, 10);
    ResponseAppend_P (PSTR (",\"%s\":%s"), str_period, str_value);
  }

  // total production counter
  if (teleinfo_prod.enabled)
  {
    lltoa (teleinfo_prod_wh.total, str_value, 10) ;
    ResponseAppend_P (PSTR (",\"PROD\":%s"), str_value);
  }

  ResponseJsonEnd ();
}

// Generate JSON with RELAY (virtual relay mapping)
//   V1..V8 : virtual relay = status
//   C1..C8 : period relay  = status,level,label
//   P1     : prod. relay status
//   W1     : prod. relay trigger
void TeleinfoDriverAppendRelaySensor ()
{
  bool    status, first;
  uint8_t index, level, hchp;
  char    str_name[32];

  // loop to publish virtual relays
  first = true;
  MiscOptionPrepareJsonSection ();
  ResponseAppend_P (PSTR ("\"RELAY\":{"));

  // production relay
  if (teleinfo_prod.enabled)
  {
    ResponseAppend_P (PSTR ("\"P1\":%u,\"W1\":%d"), teleinfo_prod.relay, teleinfo_config.prod_trigger);
    first = false;
  }

  // virtual relays
  if (teleinfo_conso.enabled)
    for (index = 0; index < 8; index ++)
    {
      if (!first) ResponseAppend_P (PSTR (","));
      ResponseAppend_P (PSTR ("\"V%u\":%u"), index + 1, TeleinfoRelayStatus (index));
      first = false;
    }

  // contract period relays
  if (teleinfo_conso.enabled)
    for (index = 0; index < teleinfo_contract_db.period_qty; index ++) 
    {
      // period status
      status = (index == teleinfo_contract.period);
      level  = TeleinfoPeriodGetLevel (index);
      hchp   = TeleinfoPeriodGetHP (index);
      TeleinfoPeriodGetLabel (str_name, sizeof (str_name), index);
      if (!first) ResponseAppend_P (PSTR (","));
      ResponseAppend_P (PSTR ("\"C%u\":\"%u,%u,%u,%s\""), index + 1, status, level, hchp, str_name);
      first = false;
    }

  ResponseJsonEnd ();
}

// Generate JSON with CALENDAR
void TeleinfoDriverAppendCalendarSensor ()
{
  // if on battery, ignore publication
  if (TeleinfoDriverIsOnBattery ()) return;

  // publish CAL current data
  MiscOptionPrepareJsonSection ();
  ResponseAppend_P (PSTR ("\"CAL\":{\"level\":%u,\"hp\":%u}"), TeleinfoPeriodGetLevel (), TeleinfoPeriodGetHP ());
}

/**************************/
/*         Alerts         */
/**************************/

bool TeleinfoDriverAlertGetStatus (const uint8_t type)
{
  bool active;

  // check paremeters
  if (type >= TIC_ALERT_MAX) return false;
  if (teleinfo_meter.arr_alert[type].timeout == UINT32_MAX) return false;

  // check for reset
  active = (LocalTime () <= teleinfo_meter.arr_alert[type].timeout);
  if (!active)
  {
    // reset alert
    teleinfo_meter.arr_alert[type].timeout = UINT32_MAX;
    strcpy_P (teleinfo_meter.arr_alert[type].str_source, PSTR (""));

    // trigger publication
    teleinfo_meter.json.data = true;
  } 

  return active;
}

void TeleinfoDriverAlertGetSource (const uint8_t type, char *pstr_source)
{
  // check paremeters
  if (type >= TIC_ALERT_MAX) return;
  if (pstr_source == nullptr) return;

  // get source
  strlcpy (pstr_source, teleinfo_meter.arr_alert[type].str_source, TIC_ALERT_SRC_SIZE);
}

void TeleinfoDriverAlertTrigger (const uint8_t type, const char* pstr_source)
{
  // check parameters
  if (!RtcTime.valid) return;
  if (!TeleinfoDriverMeterReady ()) return;
  if (type >= TIC_ALERT_MAX) return;
  if (pstr_source == nullptr) return;
  if (strlen_P (pstr_source) >= 6) return;

  // if needed, trigger publication
  if (teleinfo_meter.arr_alert[type].timeout == UINT32_MAX) teleinfo_meter.json.data = true;

  // update alert
  teleinfo_meter.arr_alert[type].timeout = LocalTime () + TELEINFO_ALERT_TIMEOUT;
  strcpy_P (teleinfo_meter.arr_alert[type].str_source, pstr_source);
}

/*********************************************\
 *                Calendar
\*********************************************/

// calculate current slot
uint8_t TeleinfoCalendarGetSlot (const uint8_t hour, const uint8_t minute)
{
  uint8_t slot;

  // check parameter
  if (hour > 23) return UINT8_MAX;
  if (minute > 59) return UINT8_MAX;

  // calculate slot
  slot = hour * 2 + (minute / 30);
 
  return slot;
}

// calculate current slot
uint8_t TeleinfoCalendarGetSlot (const char* pstr_time, const bool is_hexa)
{
  uint8_t slot, hour, minute;
  char    str_value[4];

  // check parameter
  if (pstr_time == nullptr) return UINT8_MAX;
  if (strlen (pstr_time) < 4) return UINT8_MAX;

  // calculate hours
  strlcpy (str_value, pstr_time, 3);
  if (is_hexa) hour = (uint8_t)strtol (str_value, NULL, 16);
    else hour = (uint8_t)atoi (str_value);

  // calculate minutes
  strlcpy (str_value, pstr_time + 2, 3);
  if (is_hexa) minute = (uint8_t)strtol (str_value, NULL, 16);
    else minute = (uint8_t)atoi (str_value);

  // calculate slot
  slot = TeleinfoCalendarGetSlot (hour, minute);

  return slot;
}

// calculate current date (format : YYYYMMDDSS)
uint32_t TeleinfoCalendarGetDay (const char* pstr_date, const bool add_slot)
{
  uint32_t date = 0;
  char     str_text[8];

  // check parameter
  if (pstr_date == nullptr) return 0;

  // calculate date stamp
  if (strlen (pstr_date) >= 6)
  {
    strlcpy (str_text, pstr_date, 7);
    date = 100 * (20000000 + (uint32_t)atoi (str_text));
  }

  // if needed, include day slot
  if (add_slot && (date != 0) && (strlen (pstr_date) >= 10))
  {
    // handle hours
    strlcpy (str_text, pstr_date + 6, 3);
    date += 2 * (uint8_t)atoi (str_text);

    // handle minutes
    strlcpy (str_text, pstr_date + 8, 3);
    date += (uint8_t)atoi (str_text) / 30;
  }

  return date;
}

// calculate current date (format : YYYYMMDDSS)
uint32_t TeleinfoCalendarGetDay (const uint16_t year, const uint8_t month, const uint8_t day_of_month, const uint8_t hour, const uint8_t minute)
{
  uint32_t date;

  // calculate today's date
  date  = 1000000 * (2000 + (uint32_t)year);
  date += 10000 * (uint32_t)month;
  date += 100 * (uint32_t)day_of_month;

  // append daily slot
  date += hour * 2;
  date += minute / 30;

  return date;
}

// set current date (format : YYYYMMDD00)
uint32_t TeleinfoCalendarGetNextDay (const uint32_t date)
{
  uint32_t date_next;
  TIME_T   date_dst;

  // check parameter
  if (date == 0) return 0;

  // set given date
  date_dst.year         = (uint16_t)(date / 1000000 - 1970);
  date_dst.month        = (uint8_t)(date / 10000 % 100);
  date_dst.day_of_month = (uint8_t)(date / 100 % 100);
  date_dst.hour         = 0;
  date_dst.minute       = 0;
  date_dst.second       = 0;

  // calculate next day
  date_next = MakeTime (date_dst) + 86400;
  BreakTime (date_next, date_dst);

  // generate new date timestamp
  date_next  = 1000000 * ((uint32_t)date_dst.year + 1970);
  date_next += 10000 * (uint32_t)date_dst.month;
  date_next += 100 * (uint32_t)date_dst.day_of_month;

  return date_next;
}

// remise a zero des donnees de calendrier
void TeleinfoCalendarReset (const uint8_t day)
{
  uint8_t index;

  // check parameter
  if (day >= TIC_DAY_MAX) return;

  // reset all data
  teleinfo_calendar[day].level = 0;
  for (index = 0; index < TIC_DAY_SLOT_MAX; index ++) teleinfo_calendar[day].arr_slot[index] = 0; 
  teleinfo_calendar[day].date = 0;

  // reset settings
  SettingsUpdateText (SET_TIC_CAL_TODAY + day, "");
}

// save current calendar to settings
void TeleinfoCalendarSaveToSettings ()
{
  uint8_t day, count, slot, period;
  char    str_value[8];
  String  str_setting;

  // loop thru days to save in settings
  for (day = TIC_DAY_TODAY; day < TIC_DAY_MAX; day ++)
  {
    // date
    ultoa ((ulong)teleinfo_calendar[day].date, str_value, 16);
    str_setting = str_value;

    // loop thru slots
    count = 0;
    period = teleinfo_calendar[day].arr_slot[0];
    for (slot = 0; slot < TIC_DAY_SLOT_MAX; slot ++)
    {
      // if period has not changed, increase counter
      if (teleinfo_calendar[day].arr_slot[slot] == period) count ++;

      // else save previous one
      else
      {
        // append previous period
        str_setting += period;
        sprintf_P (str_value, PSTR ("%02u"), count);
        str_setting += str_value;

        // set new period
        period = teleinfo_calendar[day].arr_slot[slot];
        count = 1;
      }
    }

    // append last period
    str_setting += period;
    sprintf_P (str_value, PSTR ("%02u"), count);
    str_setting += str_value;    

    // update string
    SettingsUpdateText (SET_TIC_CAL_TODAY + day, str_setting.c_str ());
  }
}

// load current calendar from settings
bool TeleinfoCalendarLoadFromSettings (const uint8_t day)
{
  bool     done = false;
  uint8_t  index, period, slot, start, stop, length;
  uint16_t value;
  uint32_t date;
  char    *pstr_setting;
  char     str_text[8];

  // check parameters
  if (day >= TIC_DAY_MAX) return done;

  // loop thru setting strings
  for (index = SET_TIC_CAL_TODAY; index <= SET_TIC_CAL_AFTER; index ++)
  {
    // retrieve date of current day setting
    pstr_setting = SettingsText (index);
    strlcpy (str_text, pstr_setting, 5);
    date = (uint32_t)strtoul (str_text, nullptr, 16);

    // if date is matching, populate slots
    if (date == teleinfo_calendar[day].date)
    {
      // init
      done  = true;
      index = UINT8_MAX;
      start = 0;
      pstr_setting += 4;

      // loop thru slots
      while (strlen (pstr_setting) >= 3)
      {
        // get current slot description
        strlcpy (str_text, pstr_setting, 4);
        value = atoi (str_text);
        period = (uint8_t)(value / 100); 
        length = (uint8_t)(value % 100);

        // update daily slot
        stop = min (start + length, TIC_DAY_SLOT_MAX);
        for (slot = start; slot < stop; slot ++) teleinfo_calendar[day].arr_slot[slot] = period;

        // go to next slot
        pstr_setting += 3;
        start += length;
      }
    }
  }

  return done;
}

// set current date (year has offset from 2000 : 24 is 2024)
void TeleinfoCalendarSetDate (const uint16_t year, const uint8_t month, const uint8_t day_of_month, const uint8_t hour, const uint8_t minute)
{
  uint8_t day;

  // update current slot
  teleinfo_meter.date = TeleinfoCalendarGetDay (year, month, day_of_month, hour, minute);
  teleinfo_meter.slot = TeleinfoCalendarGetSlot (hour, minute);
  
  // if today is set, nothing else
  if (teleinfo_calendar[TIC_DAY_TODAY].date > 0) return;
  
  // reset all calendar days
  for (day = TIC_DAY_TODAY; day < TIC_DAY_MAX; day ++) TeleinfoCalendarReset (day);

  // set today, tomorrow and day after tomorrow
  teleinfo_calendar[TIC_DAY_TODAY].date = TeleinfoCalendarGetDay (year, month, day_of_month, 0, 0);
  teleinfo_calendar[TIC_DAY_TMROW].date = TeleinfoCalendarGetNextDay (teleinfo_calendar[TIC_DAY_TODAY].date);
  teleinfo_calendar[TIC_DAY_AFTER].date = TeleinfoCalendarGetNextDay (teleinfo_calendar[TIC_DAY_TMROW].date);
}

// set period on current slot
void TeleinfoCalendarSetDailyCalendar (const uint8_t day, const uint8_t period)
{
  uint8_t index, period_hc, period_hp;

  // check parameters
  if (day >= TIC_DAY_AFTER) return;
  if (period >= TIC_PERIOD_MAX) return;
  
  // if dealing with EJP calendar and announcing pointe
  if (((teleinfo_contract_db.type == TIC_C_HIS_EJP) || (teleinfo_contract_db.type == TIC_C_STD_EJP)) && (period == 1))
  {
    // if today before 1h -> set period today 0h-1h
    if ((day == TIC_DAY_TODAY) && (teleinfo_meter.slot < 2)) for (index = 0; index < 2; index ++) teleinfo_calendar[TIC_DAY_TODAY].arr_slot[index] = period;

    // else if today after 7h -> set period today 7h-24h and tomorrow 0h-1h
    else if ((day == TIC_DAY_TODAY) && (teleinfo_meter.slot >= 14))
    {
      for (index = 14; index < TIC_DAY_SLOT_MAX; index ++) teleinfo_calendar[TIC_DAY_TODAY].arr_slot[index] = period;
      for (index = 0; index < 2; index ++) teleinfo_calendar[TIC_DAY_TMROW].arr_slot[index] = period;
      teleinfo_calendar[TIC_DAY_TODAY].level = teleinfo_contract_db.arr_period[period].level;
    }

    // else if tomorrow before 1h -> set period today 0h-1h
    else if ((day == TIC_DAY_TMROW) && (teleinfo_meter.slot < 2)) for (index = 0; index < 2; index ++) teleinfo_calendar[TIC_DAY_TODAY].arr_slot[index] = period;

    // else if tomorrow after 8h -> set period tomorrow 7h-24h and day after 0h-1h
    else if ((day == TIC_DAY_TMROW) && (teleinfo_meter.slot >= 16))
    {
      for (index = 14; index < TIC_DAY_SLOT_MAX; index ++) teleinfo_calendar[TIC_DAY_TMROW].arr_slot[index] = period;
      for (index = 0; index < 2; index ++) teleinfo_calendar[TIC_DAY_AFTER].arr_slot[index] = period;
      teleinfo_calendar[TIC_DAY_TMROW].level = teleinfo_contract_db.arr_period[period].level;
    }
  }

  // check if default Tempo calendar should be applied
  else if (((teleinfo_contract_db.type == TIC_C_HIS_TEMPO) || (teleinfo_contract_db.type == TIC_C_STD_TEMPO)) && (period >= 2))
  {
    // calculate HC and HP periods
    period_hc = period - (period % 2);
    period_hp = period_hc + 1;

    // if today before 6h -> set period today 0h-6h hc
    if ((day == TIC_DAY_TODAY) &&  (teleinfo_meter.slot < 12)) for (index = 0; index < 12; index ++) teleinfo_calendar[TIC_DAY_TODAY].arr_slot[index] = period_hc;

    // else if today after 6h -> set period today 6h-22h hp, today 22h-24h hc and tomorrow 0h-6h hc
    else if ((day == TIC_DAY_TODAY) &&  (teleinfo_meter.slot >= 12))
    {
      for (index = 12; index < 44; index ++) teleinfo_calendar[TIC_DAY_TODAY].arr_slot[index] = period_hp;
      for (index = 44; index < TIC_DAY_SLOT_MAX; index ++) teleinfo_calendar[TIC_DAY_TODAY].arr_slot[index] = period_hc;
      for (index = 0; index < 12; index ++) teleinfo_calendar[TIC_DAY_TMROW].arr_slot[index] = period_hc;
      teleinfo_calendar[TIC_DAY_TODAY].level = teleinfo_contract_db.arr_period[period].level;
    }

    // else if tomorrow before 6h -> set period today 0h-6h hc
    else if ((day == TIC_DAY_TMROW) &&  (teleinfo_meter.slot < 12)) for (index = 0; index < 12; index ++) teleinfo_calendar[TIC_DAY_TODAY].arr_slot[index] = period_hc;

    // else if tomorrow after 8h -> set period tomorrow 6h-22h hp, tomorrow 22h-24h hc and day after 0h-6h hc
    else if ((day == TIC_DAY_TMROW) &&  (teleinfo_meter.slot >= 16))
    {
      for (index = 12; index < 44; index ++) teleinfo_calendar[TIC_DAY_TMROW].arr_slot[index] = period_hp;
      for (index = 44; index < TIC_DAY_SLOT_MAX; index ++) teleinfo_calendar[TIC_DAY_TMROW].arr_slot[index] = period_hc;
      for (index = 0; index < 12; index ++) teleinfo_calendar[TIC_DAY_AFTER].arr_slot[index] = period_hc;
      teleinfo_calendar[TIC_DAY_TMROW].level = teleinfo_contract_db.arr_period[period].level;
    }
  }
}

void TeleinfoCalendarSetDemain (const char* pstr_color)
{
  uint8_t level, period;

  // check parameters
  if (pstr_color == nullptr) return;
  if (!RtcTime.valid) return;

  // detect color
  level = TeleinfoPeriodDetectLevel (pstr_color);
  if (level == TIC_LEVEL_NONE) return;

  // check if contract is compatible, calculate period accordingly
  if (teleinfo_contract_db.type == TIC_C_HIS_TEMPO) period = 2 * level - 2;
    else if (teleinfo_contract_db.type == TIC_C_HIS_EJP) period = level / 3;
    else return;

  // set period
  TeleinfoCalendarSetDailyCalendar (TIC_DAY_TMROW, period);
}

// calendar midnight shift
void TeleinfoEnergyCalendarMidnight ()
{
  char str_text[16];

  // shift tomorrow and day after tomorrow
  teleinfo_calendar[TIC_DAY_TODAY] = teleinfo_calendar[TIC_DAY_TMROW];
  teleinfo_calendar[TIC_DAY_TMROW] = teleinfo_calendar[TIC_DAY_AFTER];

  // init day after
  TeleinfoCalendarReset (TIC_DAY_AFTER);
  teleinfo_calendar[TIC_DAY_AFTER].date = TeleinfoCalendarGetNextDay (teleinfo_calendar[TIC_DAY_TMROW].date);
  TeleinfoDriverCalendarGetDate (TIC_DAY_AFTER, str_text, sizeof (str_text));
  AddLog (LOG_LEVEL_INFO, PSTR ("CAL: %u %s"), teleinfo_calendar[TIC_DAY_AFTER].date / 100, str_text);
}

// set calendar current pointe start
void TeleinfoCalendarPointeBegin (const uint8_t index, const char *pstr_horodatage)
{
  // check parameter
  if (!RtcTime.valid) return;
  if (index >= TIC_POINTE_MAX) return;
  if (pstr_horodatage == nullptr) return;
  if (strlen (pstr_horodatage) < 12) return;

  // set index, day and slot
  teleinfo_message.arr_pointe[index].start = TeleinfoCalendarGetDay (pstr_horodatage, true);
}

// set calendar current pointe stop
void TeleinfoCalendarPointeEnd (const uint8_t index, const char *pstr_horodatage)
{
  // check parameter
  if (!RtcTime.valid) return;
  if (index >= TIC_POINTE_MAX) return;
  if (pstr_horodatage == nullptr) return;
  if (strlen (pstr_horodatage) < 12) return;

  // check if date is within known days
  teleinfo_message.arr_pointe[index].stop = TeleinfoCalendarGetDay (pstr_horodatage, true);
}

// set default profile if not defined
void TeleinfoCalendarDefaultProfile (char *pstr_donnee)
{
  bool    update;
  uint8_t day, start, slot, period, level;
  uint8_t arr_slot[TIC_DAY_SLOT_MAX];
  char    str_text[16];

  // check parameters
  if (!RtcTime.valid) return;
  if (pstr_donnee == nullptr) return;
  if (strlen (pstr_donnee) < 8) return;

  // check if a day calendar needs to be updated
  update = false;
  for (day = TIC_DAY_TODAY; day < TIC_DAY_MAX; day ++) update |= (teleinfo_calendar[day].level == TIC_LEVEL_NONE);
  if (!update) return;

  // loop thru segments to load daily calendar
  level = TIC_LEVEL_NONE;
  for (slot = 0; slot < TIC_DAY_SLOT_MAX; slot ++) arr_slot[slot] = UINT8_MAX;
  while ((strlen (pstr_donnee) >= 8) && isdigit (pstr_donnee[0]))
  {
    // extract data and populate slot
    strlcpy (str_text, pstr_donnee, 5);
    start = TeleinfoCalendarGetSlot (str_text, teleinfo_config.cal_hexa);
    strlcpy (str_text, pstr_donnee + 7, 2);
    period = (uint8_t)atoi (str_text) - 1;
    level = max (level, teleinfo_contract_db.arr_period[period].level);
    for (slot = start; slot < TIC_DAY_SLOT_MAX; slot ++) arr_slot[slot] = period;

    // increment to next segment
    pstr_donnee += 8;
    if (pstr_donnee[0] == ' ') pstr_donnee ++;
  }

  // loop thru days : if day's calendar not set, set default one
  for (day = TIC_DAY_TODAY; day < TIC_DAY_MAX; day ++)
    if ((teleinfo_calendar[day].level == TIC_LEVEL_NONE) && (level != TIC_LEVEL_NONE))
    {
      teleinfo_calendar[day].level = level;
      for (slot = 0; slot < TIC_DAY_SLOT_MAX; slot ++) 
        if (teleinfo_calendar[day].arr_slot[slot] < arr_slot[slot]) teleinfo_calendar[day].arr_slot[slot] = arr_slot[slot];
      TeleinfoDriverCalendarGetDate (day, str_text, sizeof (str_text));
      AddLog (LOG_LEVEL_INFO, PSTR ("CAL: default calendar applied to %s"), str_text);
    }
}

// set pointe profile
void TeleinfoCalendarPointeProfile (const char *pstr_donnee)
{
  uint8_t  day, start, slot, period, index, pointe;
  uint32_t date;
  uint8_t  arr_slot[TIC_DAY_SLOT_MAX];
  char     str_value[8];

  // check parameters
  if (!RtcTime.valid) return;
  if (pstr_donnee == nullptr) return;
  if (strlen (pstr_donnee) < 8) return;

  // detect pointe to apply
  pointe = UINT8_MAX;
  date   = UINT32_MAX;
  for (index = 0; index < TIC_POINTE_MAX; index ++) 
    if ((teleinfo_message.arr_pointe[index].start > teleinfo_meter.date) && (teleinfo_message.arr_pointe[index].start < date))
    {
      date   = teleinfo_message.arr_pointe[index].start;
      pointe = index;
    }
  if (pointe >= TIC_POINTE_MAX) return;
  if (teleinfo_message.arr_pointe[pointe].stop == 0) return;

  // loop thru segments to load daily calendar
  for (slot = 0; slot < TIC_DAY_SLOT_MAX; slot ++) arr_slot[slot] = UINT8_MAX;
  while ((strlen (pstr_donnee) >= 8) && isdigit (pstr_donnee[0]))
  {
    // extract data and populate slot
    strlcpy (str_value, pstr_donnee, 5);
    start = TeleinfoCalendarGetSlot (str_value, teleinfo_config.cal_hexa);
    strlcpy (str_value, pstr_donnee + 7, 2);
    period = (uint8_t)atoi (str_value) - 1;
    for (slot = start; slot < TIC_DAY_SLOT_MAX; slot ++) arr_slot[slot] = period;

    // increment to next segment
    pstr_donnee += 8;
    if (pstr_donnee[0] == ' ') pstr_donnee ++;
  }

  // loop thru days to update their slots with current pointe period
  for (day = TIC_DAY_TODAY; day < TIC_DAY_MAX; day ++)
    for (slot = 0; slot < TIC_DAY_SLOT_MAX; slot ++)
    {
      date = teleinfo_calendar[day].date + slot;
      if ((date >= teleinfo_message.arr_pointe[pointe].start) && (date < teleinfo_message.arr_pointe[pointe].stop) && (arr_slot[slot] != UINT8_MAX)) teleinfo_calendar[day].arr_slot[slot] = arr_slot[slot];
    }
}

/***************************************\
 *              Callback
\***************************************/

// Teleinfo driver initialisation
void TeleinfoDriverInit ()
{
  uint8_t index;

  // disable wifi sleep mode
  TasmotaGlobal.wifi_stay_asleep = false;

  // set web refresh to 3 sec (linky ready)
  if (Settings->web_refresh == HTTP_REFRESH_TIME) Settings->web_refresh = TIC_WEB_REFRESH;

  // init publication flags
  teleinfo_meter.json.data = false;
  teleinfo_meter.json.live = false;
  teleinfo_meter.json.tic  = false;

  // meter timestamps
  for (index = 0; index < TIC_WEB_MAX; index ++) teleinfo_meter.arr_web_ts[index] = 0;

  // reset contract data
  TeleinfoContractInit ();

  // load configuration and data
  TeleinfoDriverLoadSettings ();
  TeleinfoDriverLoadData ();
  AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: Configuration loaded, contract %s, %u periods"), teleinfo_contract_db.str_code, teleinfo_contract_db.period_qty);

#ifdef USE_LIGHT
  // init LED presence
  teleinfo_led.present = PinUsed (GPIO_WS2812);
#endif    // USE_LIGHT
}

// Save data in case of planned restart
void TeleinfoDriverSaveBeforeRestart ()
{
  // if running on battery, nothing else
  if (TeleinfoDriverIsOnBattery ()) return;

  // if graph unit have been changed, save configuration
  TeleinfoDriverSaveSettings ();
  TeleinfoDriverSaveData ();

  // update energy counters
  EnergyUpdateToday ();
}

// Handling of received teleinfo data, called 20x / second
//     Message :    0x02 = start      Ox03 = stop        0x04 = reset 
//     Line    :    0x0A = start      0x0D = stop        0x09 = separator
void TeleinfoDriverEvery50ms ()
{
  char     character;
  size_t   index, buffer;
  char     str_character[2];
  char     str_buffer[TIC_RX_BUFFER];

  // check serial port
  if (!TeleinfoEnergySerialIsStarted ()) return;

  // read pending data
  buffer = 0;
  if (teleinfo_serial->available ()) buffer = teleinfo_serial->read (str_buffer, TIC_RX_BUFFER);

  // for ESP8266, wait for network to be up before handling reception
  if (TasmotaGlobal.global_state.network_down) return;

  // loop thru reception buffer
  for (index = 0; index < buffer; index++)
  {
    // read character
    character = str_buffer[index];

#ifdef USE_TELEINFO_TCP
    // send character thru TCP stream
    TeleinfoTCPSend (character);
#endif  // USE_TELEINFO_TCP

    // analyse character
    switch (character)
    {
      case 0x04: TeleinfoReceptionMessageReset (); break;       // reset
      case 0x02: TeleinfoReceptionMessageStart (); break;       // message start
      case 0x03: TeleinfoReceptionMessageStop ();  break;       // message stop
      case 0x0A: TeleinfoReceptionLineStart ();    break;       // line start
      case 0x0D: TeleinfoReceptionLineStop ();     break;       // line stop
      default:
        // if needed, set line separator
        if ((teleinfo_meter.nb_message == 0) && (character == 0x09)) teleinfo_meter.sep_line = 0x09;

        // append separator to line
        str_character[0] = character;
        str_character[1] = 0;
        strlcat (teleinfo_message.str_line, str_character, sizeof (teleinfo_message.str_line));
        break;
    }
  }

#ifdef USE_LIGHT
  // update LED status
  TeleinfoDriverLedUpdate ();
#endif    // USE_LIGHT
}

// called 4 times per second
void TeleinfoDriverEvery250ms ()
{
  // if meter not ready, ignore
  if (!TeleinfoDriverMeterReady ()) return;

  // check if TIC or LIVE topic should be published (according to frame skip ratio)
  if ((teleinfo_config.tic || teleinfo_config.live) && (teleinfo_meter.nb_message >= teleinfo_meter.nb_skip + (long)teleinfo_config.skip))
  {
    teleinfo_meter.nb_skip = teleinfo_meter.nb_message;
    if (teleinfo_config.live) teleinfo_meter.json.live = true;
    if (teleinfo_config.tic)  teleinfo_meter.json.tic  = true;
  }

  // if minimum delay between 2 publications is reached
  if (TeleinfoDriverWebAllow (TIC_WEB_MQTT))
  {
    // if something to publish
    if (teleinfo_meter.json.live) TeleinfoDriverPublishLive ();
      else if (teleinfo_meter.json.tic) TeleinfoDriverPublishTic ();
      else if (teleinfo_meter.json.data) 
      {
        TeleinfoDriverTriggerExtension ();
        TeleinfoDriverJsonPublish ();
      }
  }
}

// Handle MQTT teleperiod
void TeleinfoDriverTriggerExtension ()
{
  // trigger domoticz data publication
#ifdef USE_TELEINFO_DOMOTICZ
  TeleinfoDomoticzData ();
#endif  // TELEINFO_DOMOTICZ

  // trigger homie data publication
#ifdef USE_TELEINFO_HOMIE
  TeleinfoHomieData ();
#endif    // USE_TELEINFO_HOMIE

  // trigger thingsboard data publication
#ifdef USE_TELEINFO_THINGSBOARD
  TeleinfoThingsboardData ();
  TeleinofThingsboardAttribute ();
#endif  // USE_TELEINFO_THINGSBOARD

  // trigger influxdb data publication
#ifdef USE_TELEINFO_INFLUXDB
  TeleinfoInfluxDbData ();
#endif  // USE_TELEINFO_INFLUXDB
}

// Handle MQTT teleperiod
void TeleinfoDriverRemoveEnergySensor ()
{
  int start, stop;

  // look for energy section
  start = TasmotaGlobal.mqtt_data.indexOf (F ("\"ENERGY\"")); 
  if (start > 0)
  {
    stop = TasmotaGlobal.mqtt_data.substring (start).indexOf ("},");
    if (stop == -1) stop = TasmotaGlobal.mqtt_data.substring (start).indexOf ("}");
    if (stop != -1) TasmotaGlobal.mqtt_data.remove (start, stop + 2);
  }
}

// Handle MQTT teleperiod
void TeleinfoDriverJsonAppend ()
{
  // Populate current message
  // ------------------------

  // if needed, remove ENERGY
  if (!teleinfo_config.energy) TeleinfoDriverRemoveEnergySensor ();

  // data
  if (teleinfo_config.meter)    TeleinfoDriverAppendMeterSensor ();
  if (teleinfo_config.meter)    TeleinfoDriverAppendContractSensor ();
  if (teleinfo_config.relay)    TeleinfoDriverAppendRelaySensor ();
  if (teleinfo_config.calendar) TeleinfoDriverAppendCalendarSensor ();

  // alerts
  TeleinfoDriverAppendAlert ();

  // solar production
#ifdef USE_TELEINFO_SOLAR
  if (teleinfo_solar.enabled) TeleinfoSolarAppendSensor ();
#endif  // USE_TELEINFO_SOLAR

  // RTE main data
#ifdef USE_TELEINFO_RTE
  if (TeleinfoDriverIsPowered ()) TeleinfoRteAppendSensor ();
#endif  // USE_TELEINFO_RTE

  // winky data
#ifdef USE_TELEINFO_WINKY
  if (teleinfo_winky.suspend) TeleinfoWinkyAppendSensor ();
#endif    // USE_TELEINFO_WINKY

  // reset flag and declare publication
  teleinfo_meter.json.data = false;
  TeleinfoDriverWebDeclare (TIC_WEB_MQTT);
}

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

// Append Teleinfo graph button to main page
void TeleinfoDriverWebMainButton ()
{
  uint8_t index;
  char    str_color[8];

  //    style
  // -----------

  WSContentSend_P (PSTR ("\n<style>\n"));
  WSContentSend_P (PSTR ("h2 {margin:20px 0px 5px 0px;}\n"));
  WSContentSend_P (PSTR ("h3 {display:none;}\n"));
  WSContentSend_P (PSTR (".warn{width:80%%;color:red;margin-top:4px;}\n"));
  WSContentSend_P (PSTR (".sec{font-size:12px;text-align:center;padding:4px 6px;margin-bottom:5px;background:#333333;border-radius:12px;}\n"));
  WSContentSend_P (PSTR (".tic{display:flex;padding:2px 0px;font-size:11px;}\n"));
  WSContentSend_P (PSTR (".tic div{padding:0px;}\n"));
  WSContentSend_P (PSTR (".tic span.gen{padding:1px 4px;color:black;background:#aaa;border-radius:4px;font-size:9px;margin-left:4px;}\n"));
  WSContentSend_P (PSTR (".ticb{font-size:14px;padding:0px 0px 4px 5px;}\n"));
  WSContentSend_P (PSTR (".tico75{opacity:75%%;}\n"));
  WSContentSend_P (PSTR (".ticblk{color:black;}\n"));
  WSContentSend_P (PSTR (".tichead{margin-top:4px;}\n"));
  WSContentSend_P (PSTR (".tic02{width:2%%;}\n"));
  WSContentSend_P (PSTR (".tic10{width:10%%;font-size:16px;}\n"));
  WSContentSend_P (PSTR (".tic10l{width:10%%;text-align:left;}\n"));
  WSContentSend_P (PSTR (".tic12l{width:12%%;text-align:left;}\n"));
  WSContentSend_P (PSTR (".tic16{width:16%%;}\n"));
  WSContentSend_P (PSTR (".tic16l{width:16%%;text-align:left;}\n"));
  WSContentSend_P (PSTR (".tic18r{width:18%%;text-align:right;}\n"));
  WSContentSend_P (PSTR (".tic35r{width:35%%;text-align:right;font-weight:bold;}\n"));
  WSContentSend_P (PSTR (".tic36{width:36%%;border-radius:6px;}\n"));
  WSContentSend_P (PSTR (".tic36r{width:36%%;text-align:right;}\n"));
  WSContentSend_P (PSTR (".tic37l{width:37%%;text-align:left;}\n"));
  WSContentSend_P (PSTR (".tic40r{width:40%%;text-align:right;}\n"));
  WSContentSend_P (PSTR (".tic46{width:46%%;}\n"));
  WSContentSend_P (PSTR (".tic48l{width:48%%;text-align:left;font-weight:bold;}\n"));
  WSContentSend_P (PSTR (".tic50r{width:50%%;text-align:right;font-size:10px;font-style:italic;}\n"));
  WSContentSend_P (PSTR (".tic70l{width:70%%;text-align:left;font-weight:bold;}\n"));
  WSContentSend_P (PSTR (".tic80b{width:80%%;font-weight:bold;}\n"));
  WSContentSend_P (PSTR (".tic88l{width:88%%;text-align:left;}\n"));
  WSContentSend_P (PSTR (".tic100{width:100%%;}\n"));

  // value bar
  WSContentSend_P (PSTR (".bar{margin:0px;height:16px;opacity:75%%;}\n"));
  WSContentSend_P (PSTR (".bar72l{width:72%%;text-align:left;background-color:%s;border-radius:6px;}\n"), PSTR (COLOR_BACKGROUND));
  WSContentSend_P (PSTR (".barv{height:16px;padding:0px;text-align:center;font-size:12px;border-radius:6px;}\n"));

  // alert notification
  WSContentSend_P (PSTR (".tic70l span{display:block;float:right;font-weight:normal;border-radius:6px;margin-left:5px;padding:1px 3px;border:none;font-size:12px;}\n"));
  WSContentSend_P (PSTR (".tic70l span.alert{background-color:#d70;}\n"));
  WSContentSend_P (PSTR (".tic70l span.danger{background-color:#900;}\n"));

  // counter styles
  WSContentSend_P (PSTR (".countl{width:47%%;text-align:left;}\n"));
  WSContentSend_P (PSTR (".countr{width:44%%;text-align:right;}\n"));
  WSContentSend_P (PSTR (".error{color:#c00;}\n"));
  WSContentSend_P (PSTR (".reset{color:grey;}\n"));
  WSContentSend_P (PSTR (".light{width:6%%;height:16px;animation:animate %u.%02us linear infinite;}\n"), Settings->web_refresh / 1000, Settings->web_refresh % 1000 / 10);
  WSContentSend_P (PSTR ("@keyframes animate{0%%{ opacity:0;} 49.9%%{ opacity:0;} 50%%{ opacity:1;} 100%%{ opacity:1;}}\n"));

  // phase colors
  for (index = 0; index < TIC_PHASE_END; index ++)
  {
    GetTextIndexed (str_color, sizeof (str_color), index, kTeleinfoPhaseColor);  
    WSContentSend_P (PSTR (".ph%u{background-color:%s;}\n"), index, str_color);
  }

  // calendar styles
  WSContentSend_P (PSTR (".calf{border-top-left-radius:6px;border-bottom-left-radius:6px;}\n"));
  WSContentSend_P (PSTR (".call{border-top-right-radius:6px;border-bottom-right-radius:6px;}\n"));
  WSContentSend_P (PSTR (".cald{font-size:9px;}\n"));
  for (index = 0; index < TIC_LEVEL_END; index ++)
  {
    GetTextIndexed (str_color, sizeof (str_color), index, kTeleinfoLevelCalColor);  
    WSContentSend_P (PSTR (".lv%u{background-color:%s;}\n"), index, str_color);
    WSContentSend_P (PSTR (".cal%u{padding:1px 0px;background-color:%s;}\n"), index, str_color);
  }

  // remove separation lines
  WSContentSend_P (PSTR ("table hr{display:none;}\n"));

  // LED sliders
  WSContentSend_P (PSTR ("#b,#s,#c{height:20px;}\n"));
  WSContentSend_P (PSTR ("#o1{height:28px;line-height:28px;}\n"));
  WSContentSend_P (PSTR ("#sl2,#sl3,#sl4{height:10px;}\n"));

  // if driven by linky, set light as basic, not to be displayed on main page
  if (teleinfo_config.led) WSContentSend_P (PSTR ("#b,#s,#o1{display:none;}\n"));

  WSContentSend_P (PSTR ("</style>\n"));

  //   script
  // -----------
  WSContentSend_P (PSTR ("\n<script>function onClickMain(url){xmlhttp=new XMLHttpRequest();xmlhttp.open('GET',url,false);xmlhttp.send();location.reload();};</script>\n"));

  // Teleinfo message page button
  WSContentSend_P (PSTR ("\n<p><form action='%s' method='get'><button>%s %s</button></form></p>\n"), PSTR (TIC_PAGE_MESSAGE), PSTR (TIC_TELEINFO), PSTR (TIC_MESSAGE));
}

// Append Teleinfo configuration button
void TeleinfoDriverWebConfigButton ()
{
  WSContentSend_P (PSTR ("\n<p><form action='%s' method='get'><button>Teleinfo</button></form></p>\n"), PSTR (TIC_PAGE_CONFIG));
}

// Teleinfo web page
void TeleinfoDriverWebPageConfigure ()
{
  bool        status, actual;
  bool        restart = false;
  uint32_t    baudrate;
  char        str_select[16];
  char        str_title[40];
  char        str_text[64];
  const char *pstr_title;

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess ()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg (F ("save")))
  {
    // parameter 'rate' : set teleinfo rate
    baudrate = TasmotaGlobal.baudrate;
    WebGetArg (PSTR (CMND_TIC_RATE), str_text, sizeof (str_text));
    if (strlen (str_text) > 0) baudrate = (uint32_t)atoi (str_text);
    if (TasmotaGlobal.baudrate != baudrate)
    {
      restart = true;
      TasmotaGlobal.baudrate = baudrate;
      Settings->baudrate     = baudrate / 300;
    }

    // parameter 'policy' : set energy messages diffusion policy
    WebGetArg (PSTR (CMND_TIC_POLICY), str_text, sizeof (str_text));
    if (strlen (str_text) > 0) teleinfo_config.policy = atoi (str_text);

    // parameter 'delta' : power delta for dynamic publication
    WebGetArg (PSTR (CMND_TIC_DELTA), str_text, sizeof (str_text));
    if (strlen (str_text) > 0) Settings->energy_power_delta[0] = (uint16_t)atoi (str_text);

    // parameter 'energy' : set ENERGY section diffusion flag
    teleinfo_config.energy = Webserver->hasArg (F (CMND_TIC_ENERGY));

    // parameter 'meter' : set METER, PROD & CONTRACT section diffusion flag
    teleinfo_config.meter = Webserver->hasArg (F (CMND_TIC_METER));

    // parameter 'calendar' : set CALENDAR section diffusion flag
    teleinfo_config.calendar = Webserver->hasArg (F (CMND_TIC_CALENDAR));

    // parameter 'relay' : set RELAY section diffusion flag
    teleinfo_config.relay = Webserver->hasArg (F (CMND_TIC_RELAY));

    // parameter 'period' : set led color according to period
    teleinfo_config.led = Webserver->hasArg (F (CMND_TIC_LED));

#ifdef USE_TELEINFO_HOMIE
    // parameter 'homie' : set homie integration
    WebGetArg (PSTR (CMND_TIC_HOMIE), str_text, sizeof (str_text));
    status = (strlen (str_text) > 0);
    actual = TeleinfoHomieGet ();
    if (actual != status) TeleinfoHomieSet (status);
#endif    // TELEINFO_HOMIE

#ifdef USE_TELEINFO_HASS
    // parameter 'hass' : set home assistant integration
    WebGetArg (PSTR (CMND_TIC_HASS), str_text, sizeof (str_text));
    status = (strlen (str_text) > 0);
    actual = TeleinfoHomeAssistantGet ();
    if (actual != status) TeleinfoHomeAssistantSet (status);
    if (status) teleinfo_config.meter = true;
#endif    // TELEINFO_HASS

#ifdef USE_TELEINFO_THINGSBOARD
    // parameter 'things' : set thingsboard integration
    WebGetArg (PSTR (CMND_TIC_THINGSBOARD), str_text, sizeof (str_text));
    status = (strlen (str_text) > 0);
    actual = TeleinfoThingsboardGet ();
    if (actual != status) TeleinfoThingsboardSet (status);
#endif    // USE_TELEINFO_THINGSBOARD

#ifdef USE_TELEINFO_DOMOTICZ
    // parameter 'domo' : set domoticz integration
    WebGetArg (PSTR (CMND_TIC_DOMO), str_text, sizeof (str_text));
    status = (strlen (str_text) > 0);
    actual = TeleinfoDomoticzGet ();
    if (actual != status) TeleinfoDomoticzSet (status);
    TeleinfoDomoticzRetrieveParameters ();
#endif    // USE_TELEINFO_DOMOTICZ

#ifdef USE_TELEINFO_INFLUXDB
    // parameter 'influx' : set influxdb integration
    WebGetArg (PSTR (CMND_TIC_INFLUXDB), str_text, sizeof (str_text));
    status = (strlen (str_text) > 0);
    actual = TeleinfoInfluxDbGet (); 
    if (actual != status) TeleinfoInfluxDbSet (status);
    TeleinfoInfluxDbRetrieveParameters ();
#endif    // USE_TELEINFO_INFLUXDB

    // if needed, ask for restart
    if (restart) WebRestart (1);

    // save configuration
    else TeleinfoDriverSaveSettings ();
  }

  // beginning of form
  WSContentStart_P (PSTR ("Configure Teleinfo"));

    // page style
  WSContentSendStyle ();
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("p,br,hr {clear:both;}\n"));
  WSContentSend_P (PSTR ("p.dat {margin:2px 10px;}\n"));
  WSContentSend_P (PSTR ("fieldset {background:#333;margin:15px 0px;border:none;border-radius:8px;}\n")); 
  WSContentSend_P (PSTR ("fieldset.config {border-color:#888;background:%s;margin-left:12px;margin-top:4px;padding:8px 0px 0px 0px;}\n"), PSTR (COLOR_BACKGROUND));
  WSContentSend_P (PSTR ("legend {font-weight:bold;margin:0px;padding:5px;color:#888;background:transparent;}\n")); 
  WSContentSend_P (PSTR ("legend:after {content:'';display:block;height:1px;margin:15px 0px;}\n"));
  WSContentSend_P (PSTR ("div.main {padding:0px;margin-top:-25px;}\n"));
  WSContentSend_P (PSTR ("input,select {margin-bottom:10px;text-align:center;border:none;border-radius:4px;}\n"));
  WSContentSend_P (PSTR ("input[type='text'] {text-align:left;}\n"));
  WSContentSend_P (PSTR ("span {float:left;padding-top:4px;}\n"));
  WSContentSend_P (PSTR ("span.hval {width:70%%;}\n"));
  WSContentSend_P (PSTR ("span.htxt {width:35%%;}\n"));
  WSContentSend_P (PSTR ("span.hea {width:55%%;}\n"));
  WSContentSend_P (PSTR ("span.val {width:30%%;padding:0px;}\n"));
  WSContentSend_P (PSTR ("span.uni {width:15%%;text-align:center;}\n"));
  WSContentSend_P (PSTR ("span.sel {width:65%%;padding:0px;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // form
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), PSTR (TIC_PAGE_CONFIG));

  // speed selection
  // ---------------
  WSContentSend_P (PSTR ("<fieldset><legend>🧮 Compteur</legend>\n"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  // auto-detection
  if (TasmotaGlobal.baudrate == 115200) strcpy_P (str_select, PSTR ("checked")); else str_select[0] = 0;
  WSContentSend_P (PSTR ("<p class='dat'><input type='radio' name='%s' value='%d' %s>%s</p>\n"), PSTR (CMND_TIC_RATE), 115200, str_select, PSTR ("Auto-détection"));

  // 1200 bauds
  if (TasmotaGlobal.baudrate == 1200) strcpy_P (str_select, PSTR ("checked")); else str_select[0] = 0;
  WSContentSend_P (PSTR ("<p class='dat'><input type='radio' name='%s' value='%d' %s>%s</p>\n"), PSTR (CMND_TIC_RATE), 1200, str_select, PSTR ("Mode Historique"));

  // 9600 bauds
  if (TasmotaGlobal.baudrate == 9600) strcpy_P (str_select, PSTR ("checked")); else str_select[0] = 0;
  WSContentSend_P (PSTR ("<p class='dat'><input type='radio' name='%s' value='%d' %s>%s</p>\n"), PSTR (CMND_TIC_RATE), 9600, str_select, PSTR ("Mode Standard"));

  // other specific rate
  if ((TasmotaGlobal.baudrate != 1200) && (TasmotaGlobal.baudrate != 9600) && (TasmotaGlobal.baudrate != 115200))
  {
    strcpy_P (str_select, PSTR ("checked"));
    WSContentSend_P (PSTR ("<p class='dat'><input type='radio' name='%s' value='%d' %s>%s <small>(%u bauds)</small></p>\n"), PSTR (CMND_TIC_RATE), TasmotaGlobal.baudrate, str_select, PSTR ("Spécifique"), TasmotaGlobal.baudrate);
  }

  WSContentSend_P (PSTR ("</div>\n"));
  WSContentSend_P (PSTR ("</fieldset>\n"));

  // data published
  // --------------

  WSContentSend_P (PSTR ("<fieldset><legend>📈 Données publiées</legend>\n"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  if (teleinfo_config.energy) strcpy_P (str_select, PSTR ("checked")); else str_select[0] = 0;
  pstr_title = PSTR ("Topic SENSOR : section ENERGY publiée en standard par Tasmota");
  WSContentSend_P (PSTR ("<p class='dat' title='%s'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), pstr_title, PSTR (CMND_TIC_ENERGY), str_select, PSTR (CMND_TIC_ENERGY), PSTR ("Energie Tasmota"));

  if (teleinfo_config.meter) strcpy_P (str_select, PSTR ("checked")); else str_select[0] = 0;
  pstr_title = PSTR ("Topic SENSOR : sections contenant les données de consommation et de production pour chaque phase : METER (V, A, VA, W, Cosphi), PROD (V, A, VA, W, Cosphi) & CONTRACT (Wh total et par période, période courante, couleur courante)");
  WSContentSend_P (PSTR ("<p class='dat' title='%s'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), pstr_title, PSTR (CMND_TIC_METER), str_select, PSTR (CMND_TIC_METER), PSTR ("Consommation & Production"));

  if (teleinfo_config.relay) strcpy_P (str_select, PSTR ("checked")); else str_select[0] = 0;  
  pstr_title = PSTR ("Topic SENSOR : section RELAY destinée à piloter des relais grace aux périodes et aux relais virtuels");
  WSContentSend_P (PSTR ("<p class='dat' title='%s'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), pstr_title, PSTR (CMND_TIC_RELAY), str_select, PSTR (CMND_TIC_RELAY), PSTR ("Relais virtuels"));

  if (teleinfo_config.calendar) strcpy_P (str_select, PSTR ("checked")); else str_select[0] = 0;
  pstr_title = PSTR ("Topic SENSOR : section CAL annoncant l état HC/HP et la couleur de chaque heure pour aujourd hui et demain");
  WSContentSend_P (PSTR ("<p class='dat' title='%s'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), pstr_title, PSTR (CMND_TIC_CALENDAR), str_select, PSTR (CMND_TIC_CALENDAR), PSTR ("Calendrier"));

  WSContentSend_P (PSTR ("</div>\n"));
  WSContentSend_P (PSTR ("</fieldset>\n"));

  // publication policy
  // ------------------
  WSContentSend_P (PSTR ("<fieldset><legend>⏳ Fréquence de publication</legend>\n"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  GetTextIndexed (str_text, sizeof (str_text), TIC_POLICY_TELEMETRY, kTeleinfoMessagePolicy);
  if (teleinfo_config.policy == TIC_POLICY_TELEMETRY) strcpy_P (str_select, PSTR ("checked")); else str_select[0] = 0;
  sprintf_P (str_title, PSTR ("Publication toutes les %u sec."), Settings->tele_period);
  WSContentSend_P (PSTR ("<p class='dat' title='%s'><input type='radio' name='%s' value='%u' %s>%s</p>\n"), str_title, PSTR (CMND_TIC_POLICY), TIC_POLICY_TELEMETRY, str_select, str_text);

  GetTextIndexed (str_text, sizeof (str_text), TIC_POLICY_DELTA, kTeleinfoMessagePolicy);
  if (teleinfo_config.policy == TIC_POLICY_DELTA) strcpy_P (str_select, PSTR ("checked")); else str_select[0] = 0;
  pstr_title = PSTR ("Publication dès que la puissance d une phase évolue de la valeur définie");
  WSContentSend_P (PSTR ("<p class='dat' title='%s'><span class='hea'><input type='radio' name='%s' value='%u' %s>%s</span>"), pstr_title, PSTR (CMND_TIC_POLICY), TIC_POLICY_DELTA, str_select, str_text);
  WSContentSend_P (PSTR ("<span class='val'><input type='number' name='%s' min=10 max=10000 step=10 value=%u></span><span class='uni'>%s</span></p>\n"), PSTR (CMND_TIC_DELTA), Settings->energy_power_delta[0],  PSTR ("W"));

  GetTextIndexed (str_text, sizeof (str_text), TIC_POLICY_MESSAGE, kTeleinfoMessagePolicy);
  if (teleinfo_config.policy == TIC_POLICY_MESSAGE) strcpy_P (str_select, PSTR ("checked")); else str_select[0] = 0;
  pstr_title = PSTR ("Publication à chaque trame reçue du compteur. A éviter car cela stresse l ESP");
  WSContentSend_P (PSTR ("<p class='dat' title='%s'><input type='radio' name='%s' value='%u' %s>%s</p>\n"), pstr_title, PSTR (CMND_TIC_POLICY), TIC_POLICY_MESSAGE, str_select, str_text);

  WSContentSend_P (PSTR ("</div>\n"));
  WSContentSend_P (PSTR ("</fieldset>\n"));

  // integration
  // -----------
  WSContentSend_P (PSTR ("<fieldset><legend>🏠 Intégration</legend>\n"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

#ifdef USE_TELEINFO_HOMIE
  // Homie integration (auto-discovery)
  actual = TeleinfoHomieGet ();
  if (actual) strcpy_P (str_select, PSTR ("checked")); else str_select[0] = 0;
  pstr_title = PSTR ("Topic homie/... pour l auto-découverte et la publication de toutes les données suivant le protocole Homie (OpenHab)");
  WSContentSend_P (PSTR ("<p class='dat' title='%s'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), pstr_title, PSTR (CMND_TIC_HOMIE), str_select, PSTR (CMND_TIC_HOMIE), PSTR ("Homie"));
#endif    // TELEINFO_HOMIE

#ifdef USE_TELEINFO_HASS
  // Home Assistant integration (auto-discovery)
  actual = TeleinfoHomeAssistantGet ();
  if (actual) strcpy_P (str_select, PSTR ("checked")); else str_select[0] = 0;
  pstr_title = PSTR ("Topic hass/... pour l auto-découverte de toutes les données par Home Assistant. Home Assistant exploitera alors les données du topic SENSOR");
  WSContentSend_P (PSTR ("<p class='dat' title='%s'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), pstr_title, PSTR (CMND_TIC_HASS), str_select, PSTR (CMND_TIC_HASS), PSTR ("Home Assistant"));
#endif    // TELEINFO_HASS

#ifdef USE_TELEINFO_THINGSBOARD
  // Thingsboard integration (auto-discovery)
  actual = TeleinfoThingsboardGet ();
  if (actual) strcpy_P (str_select, PSTR ("checked")); else str_select[0] = 0;
  pstr_title = PSTR ("Topic v1/telemetry/device/me pour la publication des données principales vers Thingsboard");
  WSContentSend_P (PSTR ("<p class='dat' title='%s'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), pstr_title, PSTR (CMND_TIC_THINGSBOARD), str_select, PSTR (CMND_TIC_THINGSBOARD), PSTR ("Thingsboard"));
#endif    // TELEINFO_THINGSBOARD

#ifdef USE_TELEINFO_DOMOTICZ
  // Domoticz integration
  actual = TeleinfoDomoticzGet ();
  if (actual) strcpy_P (str_select, PSTR ("checked")); else str_select[0] = 0;
  pstr_title = PSTR ("Topic domoticz/in pour la publication des données principales vers Domoticz");
  WSContentSend_P (PSTR ("<p class='dat' title='%s'><input type='checkbox' id='%s' name='%s' onchange='onChangeDomo()' %s><label for='%s'>%s</label></p>\n"), pstr_title, PSTR (CMND_TIC_DOMO), PSTR (CMND_TIC_DOMO), str_select, PSTR (CMND_TIC_DOMO), PSTR ("Domoticz"));

  // Domoticz specific parameters
  TeleinfoDomoticzDisplayParameters ();
#endif    // USE_TELEINFO_DOMOTICZ

#ifdef USE_TELEINFO_INFLUXDB
  // InfluxDB integration
  actual = TeleinfoInfluxDbGet ();
  if (actual) strcpy_P (str_select, PSTR ("checked")); else str_select[0] = 0;
  pstr_title = PSTR ("Publication des données principales vers un serveur InfluxDB");
  WSContentSend_P (PSTR ("<p class='dat' title='%s'><input type='checkbox' id='%s' name='%s' onchange='onChangeInflux()' %s><label for='%s'>%s</label></p>\n"), pstr_title, PSTR (CMND_TIC_INFLUXDB), PSTR (CMND_TIC_INFLUXDB), str_select, PSTR (CMND_TIC_INFLUXDB), PSTR ("InfluxDB"));

  // InfluxDB specific parameters
  TeleinfoInfluxDbDisplayParameters ();
#endif    // USE_TELEINFO_INFLUXDB

  WSContentSend_P (PSTR ("</div>\n"));
  WSContentSend_P (PSTR ("</fieldset>\n"));

  // LED management
  // --------------
  WSContentSend_P (PSTR ("<fieldset><legend>🚦 Affichage</legend>\n"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  if (teleinfo_config.led) strcpy_P (str_select, PSTR ("checked")); else str_select[0] = 0;
  WSContentSend_P (PSTR ("<p class='dat'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), PSTR (CMND_TIC_LED), str_select, PSTR (CMND_TIC_LED), PSTR ("Pilotage de la LED"));

  WSContentSend_P (PSTR ("</div>\n"));
  WSContentSend_P (PSTR ("</fieldset>\n"));

  // End of page
  // -----------

  // save button
  WSContentSend_P (PSTR ("<br><button name='save' type='submit' class='button bgrn'>%s</button>"), PSTR (D_SAVE));
  WSContentSend_P (PSTR ("</form>\n"));

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

// Append Teleinfo state to main page
void TeleinfoDriverWebCalendarHourScale ()
{
  WSContentSend_P (PSTR ("<div class='tic'><div class='tic16'></div><div style='width:13.6%%;text-align:left;'>0h</div>"));
  WSContentSend_P (PSTR ("<div style='width:13.6%%;'>6h</div><div style='width:27.2%%;'>12h</div><div style='width:13.6%%;'>18h</div>"));
  WSContentSend_P (PSTR ("<div style='width:13.6%%;text-align:right;'>24h</div></div>\n"));
}

// prepare web page
void TeleinfoDriverWebGetArg ()
{
  uint8_t index;

  // if managed by firmware, disable LED sliders
  if (teleinfo_config.led) for (index = 0; index < LST_MAX; index++) Web.slider[index] = -1;
}

// Append Teleinfo state to main page
void TeleinfoDriverWebSensor ()
{
  bool        display, slot_now;
  uint8_t     index, phase, day, hour, slot;
  uint8_t     level, last_level, hchp, last_hchp;
  uint16_t    width;
  long        value;
  char        str_color[16];
  char        str_text[64];
  const char *pstr_alert;

  // if managed by firmware, re-enable LED sliders
//  if (teleinfo_config.led) for (index = 0; index < LST_MAX; index++) Web.slider[index] = teleinfo_led.arr_slider[index];

  // check if disabled
  if (!teleinfo_config.display) return;

  //   meter and contract
  // ----------------------

  WSContentSend_P (PSTR ("<div class='sec' onclick=\"onClickMain(\'%s?%s=%u\')\">\n"), PSTR (TIC_PAGE_DISPLAY), PSTR (CMND_TIC_MAIN), TIC_DISPLAY_CONTRACT);

  // set reception status
  pstr_alert = nullptr;
  if (teleinfo_meter.serial < TIC_SERIAL_GPIO) pstr_alert = PSTR ("TInfo Rx non configuré");
    else if (teleinfo_meter.serial < TIC_SERIAL_SPEED) pstr_alert = PSTR ("Vitesse non configurée");
    else if (teleinfo_meter.serial == TIC_SERIAL_FAILED) pstr_alert = PSTR ("Problème d'initialisation");
    else if (TasmotaGlobal.global_state.network_down) pstr_alert = PSTR ("Connexion réseau en cours");
    else if (teleinfo_meter.nb_message == 0) pstr_alert = PSTR ("Aucune donnée publiée");
    else if (teleinfo_sleep.nb_change > 0) pstr_alert = PSTR ("Nouveau contrat détecté");

  // if needed, display warning
  if (pstr_alert != nullptr) WSContentSend_P (PSTR ("<div class='tic'><div class='tic10'>⚠️</div><div class='warn'>%s</div><div class='tic10'>⚠️</div></div>\n"), pstr_alert);

  // if reception is active
  if (teleinfo_meter.nb_message > 0)
  {
    // if alert is published, add separator
    if (pstr_alert != nullptr) WSContentSend_P (PSTR ("<hr>\n"));

    // meter model, manufacturer and year and contract
    if (teleinfo_contract.mode != 0)
    {
      // mode
      GetTextIndexed (str_color, sizeof (str_color), teleinfo_contract.mode, kTeleinfoModeIcon);
      WSContentSend_P (PSTR ("<div class='tic ticb'><div class='tic10l'>%s</div>"), str_color);

      // model
      TeleinfoEnergyMeterGetModel (str_text, sizeof (str_text), str_color, sizeof (str_color));
      WSContentSend_P (PSTR ("<div class='tic80b'>%s</div></div>\n"), str_text);
    }

    // manufacturer and year
    if (teleinfo_meter.company != 0)
    {
      TeleinfoEnergyMeterGetManufacturer (str_text, sizeof (str_text));
      WSContentSend_P (PSTR ("<div class='tic'><div class='tic88l'>%s"), str_text);
      if (strlen (str_color) > 0) WSContentSend_P (PSTR ("&nbsp;<span class='gen'>%s</span>"), str_color);
      WSContentSend_P (PSTR ("</div>"));
      if (teleinfo_meter.year != 0) WSContentSend_P (PSTR ("<div class='tic12l'>%u</div>"), teleinfo_meter.year);
      WSContentSend_P (PSTR ("</div>\n"));
    }

    //   counters
    // ------------

    // set reception LED
    level = TeleinfoDriverLedGetStatus ();
    if (level == TIC_LED_STEP_OK) GetTextIndexed (str_text, sizeof (str_text), TeleinfoPeriodGetLevel (), kTeleinfoLevelIcon);
      else str_text[0] = 0;

    // display counters
    WSContentSend_P (PSTR ("<div class='tic'><div class='countl'>%d trames</div><div class='light'>%s</div><div class='countr'>%d cosφ</div></div>\n"), teleinfo_meter.nb_message, str_text, teleinfo_conso.cosphi.quantity + teleinfo_prod.cosphi.quantity);

    // if reset or more than 1% errors, display counters
    value = 0;
    if (teleinfo_meter.nb_line > 0) value = (long)(teleinfo_meter.nb_error * 10000 / teleinfo_meter.nb_line);
    if (teleinfo_config.error || (value >= 100))
    {
      WSContentSend_P (PSTR ("<div class='tic'>"));
      WSContentSend_P (PSTR ("<div class='countl error'>%d erreurs<small> (%d.%02d%%)</small></div>"), (long)teleinfo_meter.nb_error, value / 100, value % 100);
      WSContentSend_P (PSTR ("<div class='light'></div>"));
      if (teleinfo_meter.nb_reset == 0) strcpy_P (str_text, PSTR ("reset")); else strcpy_P (str_text, PSTR ("error"));
      WSContentSend_P (PSTR ("<div class='countr %s'>%d reset</div></div>\n"), str_text, teleinfo_meter.nb_reset);
    }

    //   contract
    // --------------

    if (teleinfo_config.arr_main[TIC_DISPLAY_CONTRACT])
    {
      // separator
      WSContentSend_P (PSTR ("<hr>\n"));

      // contrat name and power
      TeleinfoContractGetName (str_text, sizeof (str_text));
      WSContentSend_P (PSTR ("<div class='tic ticb'><div class='tic70l'>%s</div>"), str_text);
      if (teleinfo_contract_db.type != UINT8_MAX)
      {
        if (teleinfo_contract.phase > 1) sprintf_P (str_text, PSTR ("%ux"), teleinfo_contract.phase);
          else str_text[0] = 0;
        WSContentSend_P (PSTR ("<div class='tic18r'>%s%d</div>"), str_text, teleinfo_contract.ssousc / 1000);
      }
      if (teleinfo_contract.unit == TIC_UNIT_KVA) strcpy_P (str_text, PSTR ("kVA")); 
        else if (teleinfo_contract.unit == TIC_UNIT_KW) strcpy_P (str_text, PSTR ("kW"));
        else str_text[0] = 0;
      WSContentSend_P (PSTR ("<div class='tic02'></div><div class='tic10l'>%s</div></div>\n"), str_text);

      // contract production period
      if (teleinfo_prod.enabled)
      {
        // period name and counter value
        str_color[0] = 0;
        if (teleinfo_prod.papp > 0) sprintf_P (str_color, PSTR (" ph%u"), TIC_PHASE_PROD);
        lltoa (teleinfo_prod_wh.total / 1000, str_text, 10);
        value = (long)(teleinfo_prod_wh.total % 1000);
        WSContentSend_P (PSTR ("<div class='tic'><div class='tic16'></div><div class='tic36%s'>Production</div><div class='tic36r'>%s,%03d</div><div class='tic02'></div><div class='tic10l'>kWh</div></div>\n"), str_color, str_text, value);
      }
      
      // contract conso periods
      for (index = 0; index < TIC_PERIOD_MAX; index++)
      {
        if (teleinfo_contract_db.arr_period[index].valid || (teleinfo_conso_wh.index[index] > 0))
        {
          // period indicator
          strcpy_P (str_text, PSTR ("tic36"));
          if (TeleinfoPeriodGetHP () == 0) strcat_P (str_text, PSTR (" tico75"));

          // color if current period
          if (teleinfo_contract.period == index)
          {
            // set level color and dot
            level = TeleinfoPeriodGetLevel ();
            sprintf_P (str_color, PSTR (" lv%u"), level);
            strcat (str_text, str_color);
            if (level == TIC_LEVEL_WHITE) strcat_P (str_text, PSTR (" ticblk"));
          }
          WSContentSend_P (PSTR ("<div class='tic'><div class='tic16'></div><div class='%s'>"), str_text);

          // period label and value
          TeleinfoPeriodGetLabel (str_text, sizeof (str_text), index);
          lltoa (teleinfo_conso_wh.index[index] / 1000, str_color, 10);
          value = (long)(teleinfo_conso_wh.index[index] % 1000);
          WSContentSend_P (PSTR ("%s</div><div class='tic36r'>%s,%03d</div><div class='tic02'></div><div class='tic10l'>kWh</div></div>\n"), str_text, str_color, value);
        }
      }
    }
  }

  WSContentSend_P (PSTR ("</div>\n"));      // sec

  //   consommation
  // ----------------

  if (teleinfo_conso.enabled)
  {
    // section
    WSContentSend_P (PSTR ("<div class='sec' onclick=\"onClickMain(\'%s?%s=%u\')\">\n"), PSTR (TIC_PAGE_DISPLAY), PSTR (CMND_TIC_MAIN), TIC_DISPLAY_CONSO);

    // header
    WSContentSend_P (PSTR ("<div class='tic ticb'><div class='tic70l'>Consommation"));
    if (TeleinfoDriverAlertGetStatus (TIC_ALERT_OVERVOLT)) WSContentSend_P (PSTR ("<span class='danger'>%s</span>"), PSTR ("V"));         // over volatge                                                  // overvoltage
    if (TeleinfoDriverAlertGetStatus (TIC_ALERT_OVERLOAD)) WSContentSend_P (PSTR ("<span class='danger'>%s</span>"), PSTR ("VA"));        // over load                                            // overload
    if (TeleinfoDriverAlertGetStatus (TIC_ALERT_PERIOD))
    {
      TeleinfoDriverAlertGetSource (TIC_ALERT_PERIOD, str_text);
      WSContentSend_P (PSTR ("<span class='alert'>%s</span>"), str_text);                                                               // pointe period warning
    }
    WSContentSend_P (PSTR ("</div>"));    // tic70l

    // full display or minimize
    if (teleinfo_config.arr_main[TIC_DISPLAY_CONSO]) WSContentSend_P (PSTR ("<div class='tic18r'>%d</div><div class='tic02'></div><div class='tic10l'>kVA</div>"), teleinfo_contract.phase * teleinfo_contract.ssousc / 1000);
    else
    {
      value = 0; 
      for (phase = 0; phase < teleinfo_contract.phase; phase++) value += teleinfo_conso.phase[phase].pact;
      WSContentSend_P (PSTR ("<div class='tic18r'>%d</div><div class='tic02'></div><div class='tic10l'>W</div>"), value); 
    }

    WSContentSend_P (PSTR ("</div>\n"));

    if (teleinfo_config.arr_main[TIC_DISPLAY_CONSO])
    {
      // bar graph per phase
      if (teleinfo_contract.ssousc > 0)
        for (phase = 0; phase < teleinfo_contract.phase; phase++)
        {
          // display voltage
          if (TeleinfoDriverAlertGetStatus (TIC_ALERT_OVERVOLT)) strcpy_P (str_text, PSTR ("style='font-weight:bold;color:red;'"));
            else str_text[0] = 0;
          WSContentSend_P (PSTR ("<div class='tic bar'><div class='tic16l' %s>%d V</div>"), str_text, teleinfo_conso.phase[phase].voltage);

          // calculate percentage and value
          if (teleinfo_conso.phase[phase].papp > 0) ltoa (teleinfo_conso.phase[phase].papp, str_text, 10); 
            else str_text[0] = 0;
          value = min (100L, 100L * teleinfo_conso.phase[phase].papp / teleinfo_contract.ssousc);

          // display bar graph percentage
          WSContentSend_P (PSTR ("<div class='bar72l'><div class='barv ph%u' style='width:%d%%;'>%s</div></div><div class='tic02'></div><div class='tic10l'>VA</div></div>\n"), phase, value, str_text);
        }

      // active power
      value = 0; 
      for (phase = 0; phase < teleinfo_contract.phase; phase++) value += teleinfo_conso.phase[phase].pact;
      WSContentSend_P (PSTR ("<div class='tic'><div class='tic16'></div><div class='tic37l'>cosφ <b>%d,%02d</b></div><div class='tic35r'>%d</div><div class='tic02'></div><div class='tic10l'>W</div></div>\n"), teleinfo_conso.cosphi.value / 1000, teleinfo_conso.cosphi.value % 1000 / 10, value);

      // today's and yesterday's total
      if (teleinfo_conso_wh.yesterday > 0) WSContentSend_P (PSTR ("<div class='tic'><div class='tic16'></div><div class='tic37l'>%s</div><div class='tic35r'>%d,%03d</div><div class='tic02'></div><div class='tic10l'>kWh</div></div>\n"), PSTR ("Hier"),        teleinfo_conso_wh.yesterday / 1000, abs (teleinfo_conso_wh.yesterday % 1000));
      if (teleinfo_conso_wh.total > 0)     WSContentSend_P (PSTR ("<div class='tic'><div class='tic16'></div><div class='tic37l'>%s</div><div class='tic35r'>%d,%03d</div><div class='tic02'></div><div class='tic10l'>kWh</div></div>\n"), PSTR ("Aujourd'hui"), teleinfo_conso_wh.today / 1000,     abs (teleinfo_conso_wh.today % 1000));
    }

    WSContentSend_P (PSTR ("</div>\n"));    // sec
  }

  
  //   production
  // --------------

  if (teleinfo_prod.enabled)
  {
    // separator and header
    WSContentSend_P (PSTR ("<div class='sec' onclick=\"onClickMain(\'%s?%s=%u\')\">\n"), PSTR (TIC_PAGE_DISPLAY), PSTR (CMND_TIC_MAIN), TIC_DISPLAY_PRODUCTION);
    WSContentSend_P (PSTR ("<div class='tic ticb'><div class='tic48l'>Revente</div>"));
    if (teleinfo_config.arr_main[TIC_DISPLAY_PRODUCTION]) WSContentSend_P (PSTR ("<div class='tic40r'>%d.%d</div><div class='tic02'></div><div class='tic10l'>kW</div>"), teleinfo_config.prod_max / 1000, teleinfo_config.prod_max % 1000 / 100);
      else WSContentSend_P (PSTR ("<div class='tic40r'>%d</div><div class='tic02'></div><div class='tic10l'>W</div>"), teleinfo_prod.pact); 
    WSContentSend_P (PSTR ("</div>\n"));

    if (teleinfo_config.arr_main[TIC_DISPLAY_PRODUCTION])
    {
      // bar graph percentage
      if (teleinfo_config.prod_max > 0)
      {            
        // calculate percentage and value
        str_text[0] = 0;
        if (teleinfo_prod.papp > 0) ltoa (teleinfo_prod.papp, str_text, 10); 
        value = min (100L, 100 * teleinfo_prod.papp / teleinfo_config.prod_max);

        // display bar graph percentage
        WSContentSend_P (PSTR ("<div class='tic bar'><div class='tic16'></div><div class='bar72l'><div class='barv ph%u' style='width:%d%%;'>%s</div></div><div class='tic02'></div><div class='tic10l'>VA</div></div>\n"), TIC_PHASE_PROD, value, str_text);
      }

      // active power
      WSContentSend_P (PSTR ("<div class='tic'><div class='tic16'></div><div class='tic37l'>cosφ <b>%d,%02d</b></div><div class='tic35r'>%d</div><div class='tic02'></div><div class='tic10l'>W</div></div>\n"), teleinfo_prod.cosphi.value / 1000, teleinfo_prod.cosphi.value % 1000 / 10, teleinfo_prod.pact);

      // today's and yesterday's total
      if (teleinfo_prod_wh.yesterday > 0) WSContentSend_P (PSTR ("<div class='tic'><div class='tic16'></div><div class='tic37l'>%s</div><div class='tic35r'>%d,%03d</div><div class='tic02'></div><div class='tic10l'>kWh</div></div>\n"), PSTR ("Hier"), teleinfo_prod_wh.yesterday / 1000, abs (teleinfo_prod_wh.yesterday % 1000));
      if (teleinfo_prod_wh.total     > 0) WSContentSend_P (PSTR ("<div class='tic'><div class='tic16'></div><div class='tic37l'>%s</div><div class='tic35r'>%d,%03d</div><div class='tic02'></div><div class='tic10l'>kWh</div></div>\n"), PSTR ("Aujourd'hui"), teleinfo_prod_wh.today / 1000, abs (teleinfo_prod_wh.today % 1000));
    }

    WSContentSend_P (PSTR ("</div>\n"));    // sec
  }

  //   calendar
  // ------------

  if (teleinfo_config.calendar)
  {
    // separator and header
    GetTextIndexed (str_color, sizeof (str_color), TeleinfoPeriodGetLevel (), kTeleinfoLevelIcon);  
    WSContentSend_P (PSTR ("<div class='sec' onclick=\"onClickMain(\'%s?%s=%u\')\">\n"), PSTR (TIC_PAGE_DISPLAY), PSTR (CMND_TIC_MAIN), TIC_DISPLAY_CALENDAR);
    WSContentSend_P (PSTR ("<div class='tic ticb'><div class='tic48l'>Calendrier</div>"));
    if (!teleinfo_config.arr_main[TIC_DISPLAY_CALENDAR]) WSContentSend_P (PSTR ("<div class='tic40r'></div><div class='tic02'></div><div class='tic10l'>%s</div>"), str_color);
    WSContentSend_P (PSTR ("</div>\n"));

    if (teleinfo_config.arr_main[TIC_DISPLAY_CALENDAR])
    {
      // loop thru days
      for (day = TIC_DAY_TODAY; day < TIC_DAY_MAX; day ++)
      {
        // display day
        TeleinfoDriverCalendarGetDate (day, str_text, sizeof (str_text));
        WSContentSend_P (PSTR ("<div class='tic'><div class='tic16l'>%s</div>"), str_text);

        // init daily data
        width = 0;
        last_hchp = 0;
        last_level = TIC_LEVEL_NONE;
        str_text[0] = 0;
        str_color[0] = 0;

        // display hourly slots
        for (hour = 0; hour < 24; hour ++)
        {
          // get current slot data
          slot = hour * 2;
          slot_now = ((day == TIC_DAY_TODAY) && (hour == RtcTime.hour));

          // get slot data
          level = TeleinfoDriverCalendarGetLevel (day, slot, false);
          hchp  = TeleinfoDriverCalendarGetHP (day, slot);

          // check if previous segment should be displayed
          display = ((hour < 2) || (hour == 23));
          if (!display) display = ((last_level != level) || (last_hchp != hchp));
          if (!display) display = (slot_now || ((day == TIC_DAY_TODAY) && (hour == RtcTime.hour + 1)));
          if (display)
          {
            // display previous slot
            if (hour > 0) WSContentSend_P (PSTR ("<div class='%s' style='width:%u.%u%%;'>%s</div>"), str_text, width * TIC_CALENDAR_SLOT / 10, width * TIC_CALENDAR_SLOT % 10, str_color);

            // set current slot
            sprintf_P (str_text, PSTR ("cal%u"), level);
            if (slot_now) strcat_P (str_text, PSTR (" cald"));
            if (hour == 0) strcat_P (str_text, PSTR (" calf"));
              else if (hour == 23) strcat_P (str_text, PSTR (" call"));
            if (hchp == 0) strcat_P (str_text, PSTR (" tico75"));
            if (slot_now) GetTextIndexed (str_color, sizeof (str_color), level, kTeleinfoLevelCalDot);
              else str_color[0] = 0;

            // reset width and save last data
            width = 0;
            last_level = level;
            last_hchp  = hchp;
          }

          // increase width
          width++;
        }

        // display last slot and end of line
        WSContentSend_P (PSTR ("<div class='%s' style='width:%u.%u%%;'>%s</div></div>\n"), str_text, TIC_CALENDAR_SLOT / 10, TIC_CALENDAR_SLOT % 10, str_color);
     }

      // hour scale
      TeleinfoDriverWebCalendarHourScale ();
    }

    WSContentSend_P (PSTR ("</div>\n"));    // sec
  }

  // declare sensor publication
//  TeleinfoDriverWebDeclare (TIC_WEB_HTTP);
}

// TIC raw message data
void TeleinfoDriverWebTicUpdate ()
{
  int  index;
  char checksum;
  char str_class[4];
  char str_line[TIC_LINE_SIZE];

  // start of data page
  WSContentBegin (200, CT_PLAIN);

  // send line number
  sprintf_P (str_line, PSTR ("%d\n"), teleinfo_meter.nb_message);
  WSContentSend_P (PSTR ("%s"), str_line); 

  // loop thru TIC message lines to publish if defined
  for (index = 0; index < teleinfo_message.index_last; index ++)
  {
    checksum = teleinfo_message.arr_last[index].checksum;
    if (checksum == 0) strcpy_P (str_class, PSTR ("ko")); else strcpy_P (str_class, PSTR ("ok"));
    if (checksum == 0) checksum = 0x20;
    sprintf_P (str_line, PSTR ("%s|%s|%s|%c\n"), str_class, teleinfo_message.arr_last[index].str_etiquette, teleinfo_message.arr_last[index].str_donnee, checksum);
    WSContentSend_P (PSTR ("%s"), str_line); 
  }

  // end of data page
  WSContentEnd ();
}

void TeleinfoDriverWebTicDisplay ()
{
  uint8_t target = UINT8_MAX;
  char    str_text[4];

  // parameter 'disp'
  WebGetArg (PSTR (CMND_TIC_MAIN), str_text, sizeof (str_text));
  if (strlen (str_text) > 0) target = (uint8_t)atoi (str_text);

  // answer and revert flag
  WSContentBegin (200, CT_PLAIN);
  if (target < TIC_DISPLAY_MAX)
  {
    teleinfo_config.arr_main[target] = !teleinfo_config.arr_main[target];
    WSContentSend_P (PSTR ("%u"), teleinfo_config.arr_main[target]); 
  }
  else WSContentSend_P (PSTR ("error")); 
  WSContentEnd ();
}

// TIC message page
void TeleinfoDriverWebPageMessage ()
{
  int index;

  // beginning of form without authentification
  WSContentStart_P (PSTR (TIC_MESSAGE), false);
  WSContentSend_P (PSTR ("</script>\n\n"));

  // page data refresh script
  WSContentSend_P (PSTR ("<script type='text/javascript'>\n\n"));

  WSContentSend_P (PSTR ("function updateData(){\n"));

  WSContentSend_P (PSTR (" httpData=new XMLHttpRequest();\n"));
  WSContentSend_P (PSTR (" httpData.open('GET','%s',true);\n"), PSTR (TIC_PAGE_UPDATE));

  WSContentSend_P (PSTR (" httpData.onreadystatechange=function(){\n"));
  WSContentSend_P (PSTR ("  if (httpData.readyState===XMLHttpRequest.DONE){\n"));
  WSContentSend_P (PSTR ("   if (httpData.status===0 || (httpData.status>=200 && httpData.status<400)){\n"));
  WSContentSend_P (PSTR ("    arr_param=httpData.responseText.split('\\n');\n"));
  WSContentSend_P (PSTR ("    num_param=arr_param.length-2;\n"));
  WSContentSend_P (PSTR ("    if (document.getElementById('msg')!=null) document.getElementById('msg').textContent=arr_param[0];\n"));
  WSContentSend_P (PSTR ("    for (i=0;i<num_param;i++){\n"));
  WSContentSend_P (PSTR ("     arr_value=arr_param[i+1].split('|');\n"));
  WSContentSend_P (PSTR ("     if (document.getElementById('l'+i)!=null) document.getElementById('l'+i).style.display='table-row';\n"));
  WSContentSend_P (PSTR ("     if (document.getElementById('l'+i)!=null) document.getElementById('l'+i).className=arr_value[0];\n"));
  WSContentSend_P (PSTR ("     if (document.getElementById('e'+i)!=null) document.getElementById('e'+i).textContent=arr_value[1];\n"));
  WSContentSend_P (PSTR ("     if (document.getElementById('d'+i)!=null) document.getElementById('d'+i).textContent=arr_value[2];\n"));
  WSContentSend_P (PSTR ("     if (document.getElementById('c'+i)!=null) document.getElementById('c'+i).textContent=arr_value[3];\n"));
  WSContentSend_P (PSTR ("    }\n"));
  WSContentSend_P (PSTR ("    for (i=num_param;i<%d;i++){\n"), teleinfo_message.index_max);
  WSContentSend_P (PSTR ("     if (document.getElementById('l'+i)!=null) document.getElementById('l'+i).style.display='none';\n"));
  WSContentSend_P (PSTR ("    }\n"));
  WSContentSend_P (PSTR ("   }\n"));
  WSContentSend_P (PSTR ("   setTimeout(updateData,%u);\n"), 1000);               // ask for next update in 1 sec
  WSContentSend_P (PSTR ("  }\n"));
  WSContentSend_P (PSTR (" }\n"));

  WSContentSend_P (PSTR (" httpData.send();\n"));
  WSContentSend_P (PSTR ("}\n"));

  WSContentSend_P (PSTR ("setTimeout(updateData,%u);\n\n"), 100);                   // ask for first update after 100ms

  WSContentSend_P (PSTR ("</script>\n\n"));

  // page style
  WSContentSend_P (PSTR ("<style>\n"));

  WSContentSend_P (PSTR ("body {color:white;background-color:%s;font-size:2.2vmin;font-family:Arial, Helvetica, sans-serif;}\n"), COLOR_BACKGROUND);
  WSContentSend_P (PSTR ("div {margin:0.3vh auto;padding:2px 0px;text-align:center;vertical-align:middle;}\n"));
  WSContentSend_P (PSTR ("a {text-decoration:none;}\n"));

  WSContentSend_P (PSTR ("div.head {margin-bottom:1.5vh;font-size:3vmin;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.count {position:absolute;left:60%%;padding:4px 10px;margin-top:0.2vh;border-radius:6px;background:#333;}\n"));

  WSContentSend_P (PSTR ("table {width:100%%;max-width:400px;margin:0px auto;border:none;background:none;color:#fff;border-collapse:collapse;}\n"));
  WSContentSend_P (PSTR ("th,td {padding:0.2vh;border:1px #666 solid;}\n"));
  WSContentSend_P (PSTR ("th {font-style:bold;background:#555;}\n"));
  WSContentSend_P (PSTR ("th.label {width:30%%;}\n"));
  WSContentSend_P (PSTR ("th.value {width:60%%;}\n"));
  WSContentSend_P (PSTR ("tr.ko {color:red;}\n"));

  WSContentSend_P (PSTR ("button.menu {background:%s;color:%s;line-height:2rem;font-size:1.2rem;cursor:pointer;border:none;border-radius:0.3rem;width:150px;margin:2vh;}\n"), COLOR_BUTTON, COLOR_BUTTON_TEXT);

  WSContentSend_P (PSTR ("</style>\n\n"));

  // set cache policy, no cache for 12 hours
  WSContentSend_P (PSTR ("<meta http-equiv='Cache-control' content='public,max-age=720000'/>\n"));

  // page body
  WSContentSend_P (PSTR ("</head>\n"));
  WSContentSend_P (PSTR ("<body>\n"));

  // title and counter
  WSContentSend_P (PSTR ("<div>\n"));
  WSContentSend_P (PSTR ("<div class='count'>✉️ <span id='msg'>%d</span></div>\n"), teleinfo_meter.nb_message);
  WSContentSend_P (PSTR ("<div class='head'>Messages</div>\n"));
  WSContentSend_P (PSTR ("</div>\n"));

  // display table with header
  WSContentSend_P (PSTR ("<div class='table'><table>\n"));
  WSContentSend_P (PSTR ("<tr><th class='label'>🏷️</th><th class='value'>📄</th><th>✅</th></tr>\n"));

  // loop to display TIC messsage lines
  for (index = 0; index < teleinfo_message.index_max; index ++)
    WSContentSend_P (PSTR ("<tr id='l%d'><td id='e%d'>&nbsp;</td><td id='d%d'>&nbsp;</td><td id='c%d'>&nbsp;</td></tr>\n"), index, index, index, index);

  // end of table and end of page
  WSContentSend_P (PSTR ("</table></div>\n"));
  WSContentSend_P (PSTR ("<div>\n"));

  // main menu button
  WSContentSend_P (PSTR ("<div><form action='/' method='get'><button class='menu'>%s</button></form></div>\n"), D_MAIN_MENU);
  
  // page end
  WSContentStop ();
}

#endif    // USE_WEBSERVER

/***************************************\
 *              Interface
\***************************************/

#define XDRV_98            98

bool Xdrv98 (const uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
    case FUNC_COMMAND:
      result = DecodeCommand (kTeleinfoDriverCommands, TeleinfoDriverCommand);
      break;

    case FUNC_INIT:
      TeleinfoDriverInit ();
      break;

    case FUNC_ACTIVE:
      result = true;
      break;

    case FUNC_EVERY_50_MSECOND:
      TeleinfoDriverEvery50ms ();
      break;

      /*/
#ifdef USE_LIGHT
    case FUNC_EVERY_100_MSECOND:
      TeleinfoDriverLedEvery100ms ();
      break;
#endif    // USE_LIGHT
*/

    case FUNC_EVERY_250_MSECOND:
      TeleinfoDriverEvery250ms ();
      break;

    case FUNC_SAVE_BEFORE_RESTART:
      TeleinfoDriverSaveBeforeRestart ();
      break;

    case FUNC_JSON_APPEND:
      if (TasmotaGlobal.tele_period == 0)
      {
        TeleinfoDriverTriggerExtension ();
        TeleinfoDriverJsonAppend ();
      }
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_GET_ARG:
      TeleinfoDriverWebGetArg ();
      break;

      case FUNC_WEB_SENSOR:
      TeleinfoDriverWebSensor ();
      break;

    case FUNC_WEB_ADD_MAIN_BUTTON:
      TeleinfoDriverWebMainButton ();
      break;

    case FUNC_WEB_ADD_BUTTON:
      TeleinfoDriverWebConfigButton ();
      break;

      case FUNC_WEB_ADD_HANDLER:
      Webserver->on (F (TIC_PAGE_CONFIG),  TeleinfoDriverWebPageConfigure);
      Webserver->on (F (TIC_PAGE_MESSAGE), TeleinfoDriverWebPageMessage  );
      Webserver->on (F (TIC_PAGE_UPDATE),  TeleinfoDriverWebTicUpdate    );
      Webserver->on (F (TIC_PAGE_DISPLAY), TeleinfoDriverWebTicDisplay   );
      break;
#endif    // USE_WEBSERVER
  }

  // call extension modules
  // ----------------------

#ifdef USE_TELEINFO_HOMIE
  if (!result) result = XdrvTeleinfoHomie (function);
#endif  // USE_TELEINFO_HOMIE

#ifdef USE_TELEINFO_DOMOTICZ
  if (!result) result = XdrvTeleinfoDomoticz (function);
#endif  // USE_TELEINFO_DOMOTICZ

#ifdef USE_TELEINFO_HASS
  if (!result) result = XdrvTeleinfoHomeAssistant (function);
#endif  // USE_TELEINFO_HASS

#ifdef USE_TELEINFO_THINGSBOARD
  if (!result) result = XdrvTeleinfoThingsboard (function);
#endif  // USE_TELEINFO_THINGSBOARD

#ifdef USE_TELEINFO_TCP
  if (!result) result = XdrvTeleinfoTCP (function);
#endif  // USE_TELEINFO_TCP

#ifdef USE_TELEINFO_PROMETHEUS
  if (!result && TeleinfoDriverIsPowered ()) result = XdrvTeleinfoPrometheus (function);
#endif  // USE_TELEINFO_PROMETHEUS

#ifdef USE_TELEINFO_GRAPH
  if (!result && TeleinfoDriverIsPowered ()) result = XdrvTeleinfoGraph (function);
#endif  // USE_TELEINFO_GRAPH

#ifdef USE_UFILESYS
  if (!result && TeleinfoDriverIsPowered ()) result = XdrvTeleinfoHisto (function);
#endif  //  USE_UFILESYS

#ifdef USE_TELEINFO_SOLAR
  if (!result && TeleinfoDriverIsPowered ()) result = XdrvTeleinfoSolar (function);
#endif  // TELEINFO_SOLAR

#ifdef USE_TELEINFO_INFLUXDB
  if (!result) result = XdrvTeleinfoInfluxDB (function);
#endif  // USE_TELEINFO_INFLUXDB

#ifdef USE_TELEINFO_RTE
  if (!result && TeleinfoDriverIsPowered ()) result = XdrvTeleinfoRte (function);
#endif    // USE_TELEINFO_RTE

#ifdef USE_TELEINFO_AWTRIX
if (!result) result = XdrvTeleinfoAwtrix (function);
#endif  // USE_TELEINFO_AWTRIX

#ifdef USE_TELEINFO_WINKY
  if (!result) result = XdrvTeleinfoWinky (function);
#endif  // USE_TELEINFO_WINKY

#ifdef USE_TELEINFO_RELAY
  if (!result) result = XdrvTeleinfoRelay (function);
#endif  // TELEINFO_RELAY

  return result;
}

#endif      // USE_TELEINFO
#endif      // USE_ENERGY_SENSOR
