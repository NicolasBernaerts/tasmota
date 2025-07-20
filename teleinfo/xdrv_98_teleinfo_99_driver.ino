/*
  xdrv_98_teleinfo_99_driver.ino - France Teleinfo energy sensor support for Tasmota devices

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

  Integration flags are stored in Settings :
    - Settings->rf_code[16][0]  : Flag en enable Home Assistant integration
    - Settings->rf_code[16][2]  : Flag to enable Domoticz integration
    - Settings->rf_code[16][1]  : Flag en enable Homie integration
    - Settings->rf_code[16][3]  : Flag en enable Thingsboard integration
    - Settings->rf_code[16][5]  : Flag to enable Influxdb extension

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY orENERGY_WATCHDOG FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef USE_ENERGY_SENSOR
#ifdef USE_TELEINFO

// declare teleinfo energy sensor
#define XDRV_98                    98

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

#ifdef USE_LIGHT

// switch LED ON or OFF
void TeleinfoLedSwitch (const uint8_t state)
{
  uint8_t new_state;
  uint8_t new_dimmer;

  // check param
  if (state >= TIC_LED_PWR_MAX) return;

  // if next blink period delay is 0, skip
  if (state == TIC_LED_PWR_SLEEP) new_state = TIC_LED_PWR_SLEEP;
    else if ((state == TIC_LED_PWR_ON) && (arrTicLedOn[teleinfo_led.status] == 0) && (arrTicLedOff[teleinfo_led.status] != 0)) new_state = TIC_LED_PWR_OFF;
    else if ((state == TIC_LED_PWR_OFF) && (arrTicLedOff[teleinfo_led.status] == 0) && (arrTicLedOn[teleinfo_led.status] != 0)) new_state = TIC_LED_PWR_ON;
    else new_state = state;

  // if LED state should change
  if (teleinfo_led.state != new_state)
  {
    // set LED power
    if (new_state == TIC_LED_PWR_ON) new_dimmer = (uint8_t)teleinfo_config.param[TIC_CONFIG_BRIGHT];
      else if (new_state == TIC_LED_PWR_SLEEP) new_dimmer = (uint8_t)(teleinfo_config.param[TIC_CONFIG_BRIGHT] / 2);
      else new_dimmer = 0;

    // set dimmer and update light status
    light_controller.changeDimmer (new_dimmer);
    teleinfo_led.state    = new_state;
    if (new_state == TIC_LED_PWR_SLEEP) teleinfo_led.upd_time = UINT32_MAX;
      else teleinfo_led.upd_time = millis ();
  }
}

void TeleinfoLedSetStatus (const uint8_t status)
{
  bool    changed = false;
  uint8_t level   = TIC_LEVEL_NONE;

  // check param
  if (status >= TIC_LED_STEP_MAX) return;

  // update message reception timestamp
  if (status >= TIC_LED_STEP_ERR) teleinfo_led.msg_time = millis ();

  // detect status or level change
  if (status != teleinfo_led.status) changed = true;
  if ((status == TIC_LED_STEP_OK) && teleinfo_config.led_period)
  {
    level = TeleinfoPeriodGetLevel ();
    if (teleinfo_led.level != level) changed = true;
  }

  // if LED has changed
  if (changed)
  {
    // set color
    switch (level)
    {
      case TIC_LEVEL_NONE:
        light_controller.changeRGB (arrTicLedColor[status][0], arrTicLedColor[status][1], arrTicLedColor[status][2], true);
        break;
      case TIC_LEVEL_BLUE: 
        light_controller.changeRGB (0, 0, 255, true);
        break;
      case TIC_LEVEL_WHITE:
        light_controller.changeRGB (255, 255, 255, true);
        break;
      case TIC_LEVEL_RED:
        light_controller.changeRGB (255, 0, 0, true);
        break;
    }

    // update LED data and log
    teleinfo_led.status = status;
    teleinfo_led.level  = level;

    // switch LED ON
    TeleinfoLedSwitch (TIC_LED_PWR_ON);
  }
}

// update LED status
void TeleinfoLedUpdate ()
{
  // if LED status is enabled
  if (teleinfo_led.status != TIC_LED_STEP_NONE)
  {
    //   define LED state
    // ---------------------

    // check wifi connexion
    if (Wifi.status != WL_CONNECTED) TeleinfoLedSetStatus (TIC_LED_STEP_WIFI);

    // else if no meter reception from beginning
    else if (teleinfo_meter.nb_message < TIC_MESSAGE_MIN) TeleinfoLedSetStatus (TIC_LED_STEP_TIC);

    // else if no meter reception after timeout
    else if (TimeReached (teleinfo_led.msg_time + TELEINFO_RECEPTION_TIMEOUT)) TeleinfoLedSetStatus (TIC_LED_STEP_NODATA);

    //   Handle LED blinking
    // -----------------------

    if (teleinfo_led.upd_time != UINT32_MAX)
    {
      // if led is ON and should be switched OFF
      if ((teleinfo_led.state == TIC_LED_PWR_ON) && TimeReached (teleinfo_led.upd_time + arrTicLedOn[teleinfo_led.status])) TeleinfoLedSwitch (TIC_LED_PWR_OFF);

      // else, if led is OFF and should be switched ON
      else if ((teleinfo_led.state == TIC_LED_PWR_OFF) && TimeReached (teleinfo_led.upd_time + arrTicLedOff[teleinfo_led.status])) TeleinfoLedSwitch (TIC_LED_PWR_ON);
    }
  }
}

#endif    // USE_LIGHT

/*************************************************\
 *                  Commands
\*************************************************/

// send sensor stream
void CmndTeleinfoDriverTIC ()
{
  // get TIC data
  TeleinfoDriverPublishTic (false);

  // publish answer
  Response_P (ResponseData ());
}

/*************************************************\
 *                  Functions
\*************************************************/

bool TeleinfoDriverIsPowered ()   { return (teleinfo_config.battery == 0); }
bool TeleinfoDriverIsOnBattery () { return (teleinfo_config.battery != 0); }

void TeleinfoDriverSetBattery (const bool battery) 
{
  if (battery) teleinfo_config.battery = 1;
    else teleinfo_config.battery = 0;
}

// data are ready to be published
bool TeleinfoDriverMeterReady () 
{
  return (teleinfo_meter.nb_message >= TIC_MESSAGE_MIN);
}

// get production relay index
uint8_t TeleinfoDriverGetProductionRelay () 
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
  return teleinfo_contract.period_qty;
}

// get production relay power trigger
long TeleinfoDriverGetProductionRelayTrigger () 
{
  return teleinfo_config.prod_trigger;
}

// Trigger publication flags
void TeleinfoDriverPublishAllData (const bool append)
{
  int start, stop;

  // message start
  if (!append)
  {
    ResponseClear ();
    ResponseAppendTime ();
  }

  // if ENERGY should not be published
  else if (!teleinfo_config.energy)
  {
    start = TasmotaGlobal.mqtt_data.indexOf (F ("\"ENERGY\"")); 
    if (start > 0)
    {
      stop = TasmotaGlobal.mqtt_data.substring (start).indexOf ("}");
      if (stop > 0) TasmotaGlobal.mqtt_data.remove (start, stop + 2);
    }
  }

  // data
  if (teleinfo_config.meter)
  {
    TeleinfoDriverPublishConsoProd (true);
    TeleinfoDriverPublishContract ();
  }
  if (teleinfo_config.relay)    TeleinfoDriverPublishRelay ();
  if (teleinfo_config.calendar) TeleinfoDriverPublishCalendar (append);
  TeleinfoDriverPublishAlert ();

#ifdef USE_RTE_SERVER
  TeleinfoRtePublishSensor ();
#endif  // USE_RTE_SERVER

#ifdef USE_WINKY
  // winky publication
  TeleinfoWinkyPublish ();
#endif    // USE_WINKY

  // message end and publication
  if (!append)
  {
    ResponseJsonEnd ();
    MqttPublishTeleSensor ();
  }

  // reset flag
  teleinfo_meter.json.data = 0;

  // domoticz publication
  TeleinfoDomoticzData ();

  // homie publication
  TeleinfoHomieDataConsoProd ();
  if (teleinfo_config.calendar) TeleinfoHomieDataCalendar ();
  if (teleinfo_config.relay)    TeleinfoHomieDataRelay ();

  // thingsboard publication
  TeleinfoThingsboardData ();
  if (append) TeleinofThingsboardAttribute ();

  // influxdb publication
#ifdef ESP32
  TeleinfoInfluxDbData ();
#endif  // ESP32
}

// Generate LIVE JSON
void TeleinfoDriverPublishLive ()
{
  // JSON content
  ResponseClear ();
  ResponseAppend_P (PSTR ("{"));
  TeleinfoDriverPublishConsoProd (false);
  ResponseJsonEnd ();

  // message publication
  MqttPublishPrefixTopicRulesProcess_P (TELE, PSTR ("LIVE"), false);

  // reset JSON flag
  teleinfo_meter.json.live = 0;
}

// Generate TIC full JSON
void TeleinfoDriverPublishTic (const bool mqtt)
{
  bool    is_first = true;
  uint8_t index;

  // start of message
  ResponseClear ();

  // loop thru TIC message lines to add lines
  ResponseAppend_P (PSTR ("{"));
  for (index = 0; index < TIC_LINE_QTY; index ++)
    if (teleinfo_message.arr_last[index].checksum != 0)
    {
      if (!is_first) ResponseAppend_P (PSTR (",")); else is_first = false;
      ResponseAppend_P (PSTR ("\"%s\":\"%s\""), teleinfo_message.arr_last[index].str_etiquette, teleinfo_message.arr_last[index].str_donnee);
    }
  ResponseJsonEnd ();

  // if dealing with MQTT publication
  if (mqtt)
  {
    // message publication
    MqttPublishPrefixTopicRulesProcess_P (TELE, PSTR ("TIC"), false);

    // reset JSON flag
    teleinfo_meter.json.tic = 0;
  }
}

// Append ALERT to JSON
void TeleinfoDriverPublishAlert ()
{
  // alert
  ResponseAppend_P (PSTR (",\"ALERT\":{\"Load\":%u,\"Volt\":%u,\"Preavis\":%u,\"Label\":\"%s\"}"), teleinfo_message.stge.over_load, teleinfo_message.stge.over_volt, teleinfo_meter.preavis.level, teleinfo_meter.preavis.str_label);
}

// Append METER and PROD to JSON
void TeleinfoDriverPublishConsoProd (const bool append)
{
  uint8_t     phase, value;
  int         length = 0;
  long        voltage, current, power_app, power_act;
  char        last = 0;
  const char *pstr_response;

  // check need of ',' according to previous data
  pstr_response = ResponseData ();
  if (pstr_response != nullptr) length = strlen (pstr_response) - 1;
  if (length >= 0) last = pstr_response[length];
  if ((last != 0) && (last != '{') && (last != ',')) ResponseAppend_P (PSTR (","));

  // METER basic data
  ResponseAppend_P (PSTR ("\"METER\":{\"PH\":%u,\"ISUB\":%u,\"PSUB\":%u,\"PMAX\":%d"), teleinfo_contract.phase, teleinfo_contract.isousc, teleinfo_contract.ssousc, (long)teleinfo_config.percent * teleinfo_contract.ssousc / 100);

  // conso 
  if (teleinfo_conso.total_wh > 0)
  {
    // conso : loop thru phases
    voltage = current = power_app = power_act = 0;
    for (phase = 0; phase < teleinfo_contract.phase; phase++)
    {
      // calculate parameters
      value = phase + 1;
      voltage   += teleinfo_conso.phase[phase].voltage;
      current   += teleinfo_conso.phase[phase].current;
      power_app += teleinfo_conso.phase[phase].papp;
      power_act += teleinfo_conso.phase[phase].pact;

      // voltage
      ResponseAppend_P (PSTR (",\"U%u\":%d"), value, teleinfo_conso.phase[phase].voltage);

      // current
      ResponseAppend_P (PSTR (",\"I%u\":%d.%02d"), value, teleinfo_conso.phase[phase].current / 1000, teleinfo_conso.phase[phase].current % 1000 / 10);

      // apparent and active power
      ResponseAppend_P (PSTR (",\"P%u\":%d,\"W%u\":%d"), value, teleinfo_conso.phase[phase].papp, value, teleinfo_conso.phase[phase].pact);
    } 

    // conso : global values
    ResponseAppend_P (PSTR (",\"U\":%d,\"I\":%d.%02d,\"P\":%d,\"W\":%d"), voltage / (long)teleinfo_contract.phase, current / 1000, current % 1000 / 10, power_app, power_act);

    // conso : cosphi
    if (teleinfo_conso.cosphi.quantity >= TIC_COSPHI_MIN) ResponseAppend_P (PSTR (",\"C\":%d.%02d"), teleinfo_conso.cosphi.value / 1000, teleinfo_conso.cosphi.value % 1000 / 10);

    // conso : if not on battery, publish total of yesterday and today
    if (TeleinfoDriverIsPowered ())
    {
      ResponseAppend_P (PSTR (",\"YDAY\":%d"), teleinfo_conso.yesterday_wh);
      ResponseAppend_P (PSTR (",\"TDAY\":%d"), teleinfo_conso.today_wh);
    }
  }
  
  // production 
  if (teleinfo_prod.total_wh != 0)
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
      ResponseAppend_P (PSTR (",\"PYDAY\":%d"), teleinfo_prod.yesterday_wh);
      ResponseAppend_P (PSTR (",\"PTDAY\":%d"), teleinfo_prod.today_wh);
    }
  } 
  ResponseJsonEnd ();
}

// Append CONTRACT to JSON
void TeleinfoDriverPublishContract ()
{
  uint8_t     index;
  int         length = 0;
  char        last = 0;
  const char *pstr_response;
  char        str_value[32];
  char        str_period[32];

  // check need of ',' according to previous data
  pstr_response = ResponseData ();
  if (pstr_response != nullptr) length = strlen (pstr_response) - 1;
  if (length >= 0) last = pstr_response[length];
  if ((last != 0) && (last != '{') && (last != ',')) ResponseAppend_P (PSTR (","));

  // section CONTRACT
  ResponseAppend_P (PSTR ("\"CONTRACT\":{"));

  // meter serial number
  lltoa (teleinfo_meter.ident, str_value, 10);
  ResponseAppend_P (PSTR ("\"serial\":%s"), str_value);

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
  lltoa (teleinfo_conso.total_wh, str_value, 10);
  ResponseAppend_P (PSTR (",\"CONSO\":%s"), str_value);

  // loop to publish conso counters
  for (index = 0; index < teleinfo_contract.period_qty; index ++)
  {
    TeleinfoPeriodGetCode (str_period, sizeof (str_period), index);
    lltoa (teleinfo_conso.index_wh[index], str_value, 10);
    ResponseAppend_P (PSTR (",\"%s\":%s"), str_period, str_value);
  }

  // total production counter
  if (teleinfo_prod.total_wh != 0)
  {
    lltoa (teleinfo_prod.total_wh, str_value, 10) ;
    ResponseAppend_P (PSTR (",\"PROD\":%s"), str_value);
  }

  ResponseJsonEnd ();
}

// Generate JSON with RELAY (virtual relay mapping)
//   V1..V8 : virtual relay
//   C1..C8 : contract period relay
//   N1..N8 : contract period name
//   P1     : production relay
//   W1     : production relay power trigger
void TeleinfoDriverPublishRelay ()
{
  uint8_t     index, value;
  int         length = 0;
  char        last = 0;
  const char *pstr_response;
  char        str_text[32];

  // check need of ',' according to previous data
  pstr_response = ResponseData ();
  if (pstr_response != nullptr) length = strlen (pstr_response) - 1;
  if (length >= 0) last = pstr_response[length];
  if ((last != 0) && (last != '{') && (last != ',')) ResponseAppend_P (PSTR (","));

  // loop to publish virtual relays
  ResponseAppend_P (PSTR ("\"RELAY\":{"));

  // virtual relays
  for (index = 0; index < 8; index ++) ResponseAppend_P (PSTR ("\"V%u\":%u,"), index + 1, TeleinfoRelayStatus (index));

  // contract period relays
  for (index = 0; index < teleinfo_contract.period_qty; index ++) 
  {
    // period status
    if (index == teleinfo_contract.period) value = 1;
      else value = 0;
    ResponseAppend_P (PSTR ("\"C%u\":%u,"), index + 1, value);

    // period name
    TeleinfoPeriodGetLabel (str_text, sizeof (str_text), index);
    ResponseAppend_P (PSTR ("\"N%u\":\"%s\","), index + 1, str_text);
  }

  // production relay
  ResponseAppend_P (PSTR ("\"P%u\":%u,\"W%u\":%d"), 1, teleinfo_prod.relay, 1, teleinfo_config.prod_trigger);

  ResponseJsonEnd ();
}

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

#ifdef USE_RTE_SERVER
  if (use_rte && TeleinfoRteTempoIsEnabled ()  && ((teleinfo_contract.index == TIC_C_HIS_TEMPO) || (teleinfo_contract.index == TIC_C_STD_TEMPO))) level = TeleinfoRteTempoGetGlobalLevel  (day + 1, slot / 2);
  if (use_rte && TeleinfoRtePointeIsEnabled () && ((teleinfo_contract.index == TIC_C_HIS_EJP)   || (teleinfo_contract.index == TIC_C_STD_EJP)))   level = TeleinfoRtePointeGetGlobalLevel (day + 1, slot / 2);
#endif      // USE_RTE_SERVER

  return level;
}

// Generate JSON with CALENDAR
void TeleinfoDriverPublishCalendar (const bool with_days)
{
  uint8_t     day, slot, profile;
  int         length = 0;
  char        last = 0;
  const char *pstr_response;

  // if on battery, ignore publication
  if (TeleinfoDriverIsOnBattery ()) return;

  // check need of ',' according to previous data
  pstr_response = ResponseData ();
  if (pstr_response != nullptr) length = strlen (pstr_response) - 1;
  if (length >= 0) last = pstr_response[length];
  if ((last != 0) && (last != '{') && (last != ',')) ResponseAppend_P (PSTR (","));

  // publish CAL current data
  ResponseAppend_P (PSTR ("\"CAL\":{\"level\":%u,\"hp\":%u"), TeleinfoPeriodGetLevel (), TeleinfoPeriodGetHP ());

  // if full publication, append 2 days calendar
  if (with_days) for (day = TIC_DAY_TODAY; day <= TIC_DAY_TMROW; day ++)
  {
    // day header
    if (day == TIC_DAY_TODAY) ResponseAppend_P (PSTR (",\"tday\":{"));
    if (day == TIC_DAY_TMROW) ResponseAppend_P (PSTR (",\"tmrw\":{"));

    // hour slots
    for (slot = 0; slot < TIC_DAY_SLOT_MAX; slot ++)
    {
      if (slot > 0) ResponseAppend_P (PSTR (","));
      profile = 10 *  TeleinfoDriverCalendarGetLevel (day, slot, false) + TeleinfoDriverCalendarGetHP (day, slot);
      ResponseAppend_P (PSTR ("\"%u\":%u"), slot, profile);
    } 
    ResponseJsonEnd ();
  }

  ResponseJsonEnd ();
}

/***************************************\
 *              Callback
\***************************************/

// Teleinfo driver initialisation
void TeleinfoDriverInit ()
{
  // disable wifi sleep mode
  TasmotaGlobal.wifi_stay_asleep = false;

#ifdef USE_LIGHT
  if (PinUsed (GPIO_WS2812))
  {
    teleinfo_led.status = TIC_LED_STEP_WIFI;
    AddLog (LOG_LEVEL_INFO, PSTR ("TIC: RGB LED is used for status"));
  } 
#endif    // USE_LIGHT
}


// Handling of received teleinfo data, called 20x / second
//     Message :    0x02 = start      Ox03 = stop        0x04 = reset 
//     Line    :    0x0A = start      0x0D = stop        0x09 = separator
void TeleinfoDriverEvery50ms ()
{
  char   character;
  size_t index, buffer;
  char   str_character[2];
  char   str_buffer[TIC_RX_BUFFER];

  // check serial port
  if (teleinfo_serial == nullptr) return;

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

    // send character thru TCP stream
    TeleinfoTCPSend (character); 

    // handle according to reception stage
    switch (teleinfo_meter.reception)
    {
      case TIC_RECEPTION_NONE:
        // reset
        if (character == 0x04) TeleinfoReceptionMessageReset ();

        // message start
        else if (character == 0x02) TeleinfoReceptionMessageStart ();
        break;

      case TIC_RECEPTION_MESSAGE:
        // reset
        if (character == 0x04) TeleinfoReceptionMessageReset ();

        // line start
        else if (character == 0x0A) TeleinfoReceptionLineStart ();

        // message stop
        else if (character == 0x03) TeleinfoReceptionMessageStop ();
        break;

      case TIC_RECEPTION_LINE:
        // reset
        if (character == 0x04) TeleinfoReceptionMessageReset ();

        // line stop
        else if (character == 0x0D) TeleinfoReceptionLineStop ();

        // message stop
        else if (character == 0x03) TeleinfoReceptionMessageStop ();

        // append character to line
        else
        {
          // set line separator
          if ((teleinfo_meter.nb_message == 0) && (character == 0x09)) teleinfo_meter.sep_line = 0x09;

          // append separator to line
          str_character[0] = character;
          str_character[1] = 0;
          strlcat (teleinfo_message.str_line, str_character, sizeof (teleinfo_message.str_line));
        }
        break;

      default:
        break;
    }
  }
}

// called 4 times per second
void TeleinfoDriverEvery250ms ()
{
  // if no valid time, ignore
  if (!RtcTime.valid) return;
  if (!TeleinfoDriverMeterReady ()) return;

  // check for last message publication
  if (teleinfo_meter.last_msg == 0) teleinfo_meter.last_msg = millis ();

  // check if LIVE topic should be published (according to frame skip ratio)
  if ((teleinfo_config.tic || teleinfo_config.live) && (teleinfo_meter.nb_message >= teleinfo_meter.nb_skip + (long)teleinfo_config.skip))
  {
    teleinfo_meter.nb_skip = teleinfo_meter.nb_message;
    if (teleinfo_config.live) teleinfo_meter.json.live = 1;
    if (teleinfo_config.tic)  teleinfo_meter.json.tic = 1;
  }

  // if minimum delay between 2 publications is reached
  if (TimeReached (teleinfo_meter.last_msg + TIC_MESSAGE_DELAY))
  {
    // update next publication timestamp
    teleinfo_meter.last_msg = millis ();

    // if something to publish
    if (teleinfo_meter.json.live) TeleinfoDriverPublishLive ();
      else if (teleinfo_meter.json.tic) TeleinfoDriverPublishTic (true);
      else if (teleinfo_meter.json.data) TeleinfoDriverPublishAllData (false);
  }
}

// Handle MQTT teleperiod
void TeleinfoDriverTeleperiod ()
{
  // check if real teleperiod
  if (TasmotaGlobal.tele_period > 0) return;

  // trigger flags for full topic publication
  TeleinfoDriverPublishAllData (true);
}

// Save data in case of planned restart
void TeleinfoDriverSaveBeforeRestart ()
{
  // if running on battery, nothing else
  if (TeleinfoDriverIsOnBattery ()) return;

  // if graph unit have been changed, save configuration
  TeleinfoEnergyConfigSave (false);

  // update energy counters
  EnergyUpdateToday ();
}

/*************************************************\
 *               InfluxDB API
\*************************************************/

#ifdef ESP32

// called to generate InfluxDB data
void TeleinfoDriverApiInfluxDb ()
{
  uint8_t phase, index;
  char    str_value[16];
  char    str_line[96];

  // if nothing to publish, ignore
  if ((teleinfo_conso.total_wh == 0) && (teleinfo_prod.total_wh == 0)) return;

  // wifi RSSI
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%d\n"), PSTR ("wifi"), TasmotaGlobal.hostname, PSTR ("rssi"), WiFi.RSSI ());
  TasmotaGlobal.mqtt_data += str_line;

  // contract mode
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u\n"), PSTR ("contract"), TasmotaGlobal.hostname, PSTR ("mode"), teleinfo_contract.mode);
  TasmotaGlobal.mqtt_data += str_line;

  // number of periods
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u\n"), PSTR ("contract"), TasmotaGlobal.hostname, PSTR ("periods"), teleinfo_contract.period_qty);
  TasmotaGlobal.mqtt_data += str_line;

  // current period
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u\n"), PSTR ("contract"), TasmotaGlobal.hostname, PSTR ("period"), teleinfo_contract.period);
  TasmotaGlobal.mqtt_data += str_line;

  // color
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u\n"), PSTR ("contract"), TasmotaGlobal.hostname, PSTR ("color"), TeleinfoPeriodGetLevel ());
  TasmotaGlobal.mqtt_data += str_line;

  // hc/hp
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u\n"), PSTR ("contract"), TasmotaGlobal.hostname, PSTR ("hp"), TeleinfoPeriodGetHP ());
  TasmotaGlobal.mqtt_data += str_line;

  // if needed, add conso data
  if (teleinfo_conso.total_wh > 0)
  {
    // conso global total
    lltoa (teleinfo_conso.total_wh, str_value, 10);
    snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%s\n"), PSTR ("conso"), TasmotaGlobal.hostname, PSTR ("total"), str_value);
    TasmotaGlobal.mqtt_data += str_line;
    
    // conso indexes total
    for (index = 0; index < teleinfo_contract.period_qty; index++)
    {
      lltoa (teleinfo_conso.index_wh[index], str_value, 10);
      snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s%u value=%s\n"), PSTR ("conso"), TasmotaGlobal.hostname, PSTR ("index"), index + 1, str_value);
      TasmotaGlobal.mqtt_data += str_line;
    }

    // loop thru phases
    for (phase = 0; phase < teleinfo_contract.phase; phase++)
    {
      // set phase index
      if ((phase == 0) && (teleinfo_contract.phase == 1)) str_value[0] = 0;
        else sprintf_P (str_value, PSTR ("%u"), phase + 1);

      // current
      snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s%s value=%d.%03d\n"), PSTR ("conso"), TasmotaGlobal.hostname, PSTR ("I"), str_value, teleinfo_conso.phase[phase].current / 1000, teleinfo_conso.phase[phase].current % 1000);
      TasmotaGlobal.mqtt_data += str_line;

      // voltage
      snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s%s value=%d\n"), PSTR ("conso"), TasmotaGlobal.hostname, PSTR ("U"), str_value, teleinfo_conso.phase[phase].voltage);
      TasmotaGlobal.mqtt_data += str_line;

      // apparent power
      snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s%s value=%d\n"), PSTR ("conso"), TasmotaGlobal.hostname, PSTR ("VA"), str_value, teleinfo_conso.phase[phase].papp);
      TasmotaGlobal.mqtt_data += str_line;

      // active power
      snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s%s value=%d\n"), PSTR ("conso"), TasmotaGlobal.hostname, PSTR ("W"), str_value, teleinfo_conso.phase[phase].pact);
      TasmotaGlobal.mqtt_data += str_line;
    }

    // cos phi
    snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%d.%02d\n"), PSTR ("conso"), TasmotaGlobal.hostname, PSTR ("cosphi"), teleinfo_conso.cosphi.value / 1000, teleinfo_conso.cosphi.value % 1000 / 10);
    TasmotaGlobal.mqtt_data += str_line;
  }

  // if needed, add prod data
  if (teleinfo_prod.total_wh > 0)
  {
    // prod global total
    lltoa (teleinfo_prod.total_wh, str_value, 10);
    snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%s\n"), PSTR ("prod"), TasmotaGlobal.hostname, PSTR ("total"), str_value);
    TasmotaGlobal.mqtt_data += str_line;

    // apparent power
    snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%d\n"), PSTR ("prod"), TasmotaGlobal.hostname, PSTR ("VA"), teleinfo_prod.papp);
    TasmotaGlobal.mqtt_data += str_line;

    // active power
    snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%d\n"), PSTR ("prod"), TasmotaGlobal.hostname, PSTR ("W"),  teleinfo_prod.pact);
    TasmotaGlobal.mqtt_data += str_line;

    // cos phi
    snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%d.%02d\n"), PSTR ("prod"), TasmotaGlobal.hostname, PSTR ("cosphi"), teleinfo_prod.cosphi.value / 1000, teleinfo_prod.cosphi.value % 1000 / 10);
    TasmotaGlobal.mqtt_data += str_line;
  }
}

# endif     // ESP32

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

// Append Teleinfo graph button to main page
void TeleinfoDriverWebMainButton ()
{
  // remove LED sliders
  WSContentSend_P (PSTR ("<style>#b,#c,#s{display:none;}</style>\n"));

  // Teleinfo message page button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>Messages</button></form></p>\n"), PSTR (TIC_PAGE_TIC_MSG));
}

// Append Teleinfo configuration button
void TeleinfoDriverWebConfigButton ()
{
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>Teleinfo</button></form></p>\n"), PSTR (TIC_PAGE_TIC_CFG));
}

// Teleinfo web page
void TeleinfoDriverWebPageConfigure ()
{
  bool        status, actual;
  bool        restart = false;
  uint8_t     index;
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
    if (Webserver->hasArg (F (CMND_TIC_ENERGY))) teleinfo_config.energy = 1; else teleinfo_config.energy = 0;

    // parameter 'meter' : set METER, PROD & CONTRACT section diffusion flag
    if (Webserver->hasArg (F (CMND_TIC_METER))) teleinfo_config.meter = 1; else teleinfo_config.meter = 0;

    // parameter 'calendar' : set CALENDAR section diffusion flag
    if (Webserver->hasArg (F (CMND_TIC_CALENDAR))) teleinfo_config.calendar = 1; else teleinfo_config.calendar = 0;

    // parameter 'relay' : set RELAY section diffusion flag
    if (Webserver->hasArg (F (CMND_TIC_RELAY))) teleinfo_config.relay = 1; else teleinfo_config.relay = 0;

    // parameter 'live' : set LIVE topic diffusion flag
    if (Webserver->hasArg (F (CMND_TIC_LIVE))) teleinfo_config.live = 1; else teleinfo_config.live = 0;

    // parameter 'tic' : set TIC topic diffusion flag
    if (Webserver->hasArg (F (CMND_TIC_TIC))) teleinfo_config.tic = 1; else teleinfo_config.tic = 0;

    // parameter 'skip' : set frame skip parameter
    WebGetArg (PSTR (CMND_TIC_SKIP), str_text, sizeof (str_text));
    if (strlen (str_text) > 0) teleinfo_config.skip = atoi (str_text);

    // parameter 'period' : set led color according to period
    if (Webserver->hasArg (F (CMND_TIC_PERIOD))) teleinfo_config.led_period = 1; else teleinfo_config.led_period = 0;

    // parameter 'bright' : set led brightness
    WebGetArg (PSTR (CMND_TIC_BRIGHT), str_text, sizeof (str_text));
    if (strlen (str_text) > 0) teleinfo_config.param[TIC_CONFIG_BRIGHT] = atol (str_text);

    // parameter 'hass' : set home assistant integration
    WebGetArg (PSTR (CMND_TIC_HASS), str_text, sizeof (str_text));
    status = (strlen (str_text) > 0);
    actual = TeleinfoHassGet ();
    if (actual != status) TeleinfoHassSet (status);
    if (status) teleinfo_config.meter = 1;

    // parameter 'homie' : set homie integration
    WebGetArg (PSTR (CMND_TIC_HOMIE), str_text, sizeof (str_text));
    status = (strlen (str_text) > 0);
    actual = TeleinfoHomieGet ();
    if (actual != status) TeleinfoHomieSet (status);

    // parameter 'domo' : set domoticz integration
    WebGetArg (PSTR (CMND_TIC_DOMO), str_text, sizeof (str_text));
    status = (strlen (str_text) > 0);
    actual = TeleinfoDomoticzGet ();
    if (actual != status) TeleinfoDomoticzSet (status);
    TeleinfoDomoticzRetrieveParameters ();

    // parameter 'things' : set thingsboard integration
    WebGetArg (PSTR (CMND_TIC_THINGSBOARD), str_text, sizeof (str_text));
    status = (strlen (str_text) > 0);
    actual = TeleinfoThingsboardGet ();
    if (actual != status) TeleinfoThingsboardSet (status);

#ifdef ESP32
    // parameter 'influx' : set influxdb integration
    WebGetArg (PSTR (CMND_TIC_INFLUXDB), str_text, sizeof (str_text));
    status = (strlen (str_text) > 0);
    actual = TeleinfoInfluxDbGet (); 
    if (actual != status) TeleinfoInfluxDbSet (status);
#endif    // ESP32

    // save configuration
    TeleinfoEnergyConfigSave (true);

    // if needed, ask for restart
    if (restart) WebRestart (1);
  }

  // beginning of form
  WSContentStart_P (PSTR ("Configure Teleinfo"));

    // page style
  WSContentSendStyle ();
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("p,br,hr {clear:both;}\n"));
  WSContentSend_P (PSTR ("p.dat {margin:0px 10px;}\n"));
  WSContentSend_P (PSTR ("fieldset {background:#333;margin:15px 0px;border:none;border-radius:8px;}\n")); 
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
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), PSTR (TIC_PAGE_TIC_CFG));

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

  // Home Assistant auto-discovery
  actual = TeleinfoHassGet ();
  if (actual) strcpy_P (str_select, PSTR ("checked")); else str_select[0] = 0;
  pstr_title = PSTR ("Topic hass/... pour l auto-découverte de toutes les données par Home Assistant. Home Assistant exploitera alors les données du topic SENSOR");
  WSContentSend_P (PSTR ("<p class='dat' title='%s'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), pstr_title, PSTR (CMND_TIC_HASS), str_select, PSTR (CMND_TIC_HASS), PSTR ("Home Assistant"));

  // Homie auto-discovery
  actual = TeleinfoHomieGet ();
  if (actual) strcpy_P (str_select, PSTR ("checked")); else str_select[0] = 0;
  pstr_title = PSTR ("Topic homie/... pour l auto-découverte et la publication de toutes les données suivant le protocole Homie (OpenHab)");
  WSContentSend_P (PSTR ("<p class='dat' title='%s'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), pstr_title, PSTR (CMND_TIC_HOMIE), str_select, PSTR (CMND_TIC_HOMIE), PSTR ("Homie"));

  // Domoticz integration
  actual = TeleinfoDomoticzGet ();
  if (actual) strcpy_P (str_select, PSTR ("checked")); else str_select[0] = 0;
  pstr_title = PSTR ("Topic domoticz/in pour la publication des données principales vers Domoticz");
  WSContentSend_P (PSTR ("<p class='dat' title='%s'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), pstr_title, PSTR (CMND_TIC_DOMO), str_select, PSTR (CMND_TIC_DOMO), PSTR ("Domoticz"));
  if (actual) TeleinfoDomoticzDisplayParameters ();

  // Thingsboard auto-discovery
  actual = TeleinfoThingsboardGet ();
  if (actual) strcpy_P (str_select, PSTR ("checked")); else str_select[0] = 0;
  pstr_title = PSTR ("Topic v1/telemetry/device/me pour la publication des données principales vers Thingsboard");
  WSContentSend_P (PSTR ("<p class='dat' title='%s'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), pstr_title, PSTR (CMND_TIC_THINGSBOARD), str_select, PSTR (CMND_TIC_THINGSBOARD), PSTR ("Thingsboard"));

#ifdef ESP32
  // InfluxDB integration
  actual = TeleinfoInfluxDbGet ();
  if (actual) strcpy_P (str_select, PSTR ("checked")); else str_select[0] = 0;
  pstr_title = PSTR ("Publication des données principales vers un serveur InfluxDB");
  WSContentSend_P (PSTR ("<p class='dat' title='%s'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), pstr_title, PSTR (CMND_TIC_INFLUXDB), str_select, PSTR (CMND_TIC_INFLUXDB), PSTR ("InfluxDB"));
//  if (actual) InfluxDbExtensionDisplayParameters ();
#endif    // ESP32

  WSContentSend_P (PSTR ("</div>\n"));
  WSContentSend_P (PSTR ("</fieldset>\n"));

  // publication type
  // ----------------
  WSContentSend_P (PSTR ("<fieldset><legend>♻️ Données spécifiques</legend>\n"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  // LIVE data
  if (teleinfo_config.live) strcpy_P (str_select, PSTR ("checked")); else str_select[0] = 0;
  pstr_title = PSTR ("Topic LIVE avec les données de consommation et/ou de production en quasi temps réel, ajustable via energyconfig skip=xxx");
  WSContentSend_P (PSTR ("<p class='dat' title='%s'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), pstr_title, PSTR (CMND_TIC_LIVE), str_select, PSTR (CMND_TIC_LIVE), PSTR ("Données Temps réel"));

  // TIC data
  if (teleinfo_config.tic) strcpy_P (str_select, PSTR ("checked")); else str_select[0] = 0;
  pstr_title = PSTR ("Topic TIC contenant les données telles que publiées par le compteur");
  WSContentSend_P (PSTR ("<p class='dat' title='%s'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), pstr_title, PSTR (CMND_TIC_TIC), str_select, PSTR (CMND_TIC_TIC), PSTR ("Données Teleinfo brutes"));

  // skip value
  WSContentSend_P (PSTR ("<p class='dat'><span class='hea'>&nbsp;Publier 1 trame /</span><span class='val'><input type='number' name='%s' min=1 max=7 step=1 value=%u></span><span class='uni'></span></p>\n"), PSTR (CMND_TIC_SKIP), teleinfo_config.skip);

  WSContentSend_P (PSTR ("</div>\n"));
  WSContentSend_P (PSTR ("</fieldset>\n"));

  // LED management
  // --------------
  WSContentSend_P (PSTR ("<fieldset><legend>🚦 Affichage LED</legend>\n"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  WSContentSend_P (PSTR ("<p class='dat'><span class='hea'>&nbsp;Luminosité</span><span class='val'><input type='number' name='%s' min=0 max=100 step=1 value=%d></span><span class='uni'>%%</span></p>\n"), PSTR (CMND_TIC_BRIGHT), teleinfo_config.param[TIC_CONFIG_BRIGHT]);

  if (teleinfo_config.led_period) strcpy_P (str_select, PSTR ("checked"));
    else str_select[0] = 0;
  WSContentSend_P (PSTR ("<p class='dat'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), PSTR (CMND_TIC_PERIOD), str_select, PSTR (CMND_TIC_PERIOD), PSTR ("Couleur de la période"));

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
void TeleinfoDriverWebSensor ()
{
  uint8_t    index, phase, day, hour, slot, level, hchp;
  uint32_t   period, percent;
  long       value, red, green;
  char       str_color[16];
  char       str_text[64];
  const char *pstr_alert;

  //   Start
  // ----------

  // display start
  WSContentSend_P (PSTR ("<div style='font-size:13px;text-align:center;padding:4px 6px;margin-bottom:5px;background:#333333;border-radius:12px;'>\n"));

  // tic style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("h2 {margin:20px 0px 5px 0px;}\n"));
  WSContentSend_P (PSTR ("h3 {display:none;}\n"));
  WSContentSend_P (PSTR ("div.tic{display:flex;padding:2px 0px;}\n"));
  WSContentSend_P (PSTR ("div.tic div{padding:0px;}\n"));
  WSContentSend_P (PSTR ("div.tic span{padding:0px 4px;color:black;background:#aaa;border-radius:4px;}\n"));
  WSContentSend_P (PSTR ("div.tich{width:16%%;}\n"));
  WSContentSend_P (PSTR ("div.tict{width:37%%;text-align:left;}\n"));
  WSContentSend_P (PSTR ("div.ticv{width:35%%;text-align:right;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.tics{width:2%%;}\n"));
  WSContentSend_P (PSTR ("div.ticu{width:10%%;text-align:left;}\n"));

  WSContentSend_P (PSTR ("div.bar{margin:0px;height:16px;opacity:75%%;}\n"));
  WSContentSend_P (PSTR ("div.barh{width:16%%;text-align:left;}\n"));
  WSContentSend_P (PSTR ("div.barm{width:72%%;text-align:left;background-color:%s;border-radius:6px;}\n"), PSTR (COLOR_BACKGROUND));
  WSContentSend_P (PSTR ("div.barv{height:16px;padding:0px;text-align:center;font-size:12px;border-radius:6px;}\n"));

  WSContentSend_P (PSTR ("div.warn{width:68%%;text-align:center;margin-top:2px;font-size:12px;font-weight:bold;border-radius:8px;opacity:0.6;}\n"));
  
  WSContentSend_P (PSTR ("table hr{display:none;}\n"));
  WSContentSend_P (PSTR ("button#o1{display:none;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // relay style
  if (teleinfo_config.relay)
  {
    WSContentSend_P (PSTR ("<style>\n"));
    WSContentSend_P (PSTR ("div.rel{display:flex;margin-top:4px;}\n"));
    WSContentSend_P (PSTR ("div.rel, div.rel div{padding:0px;}\n"));
    WSContentSend_P (PSTR ("div.relh{width:16%%;text-align:left;}\n"));
    WSContentSend_P (PSTR ("div.relr{width:9.5%%;text-align:left;font-size:16px;}\n"));
    WSContentSend_P (PSTR ("div.reli{width:7.5%%;text-align:left;margin-left:2%%;font-size:10px;}\n"));
    WSContentSend_P (PSTR ("div.relv{width:63.5%%;text-align:right;font-weight:bold;}\n"));
    WSContentSend_P (PSTR ("div.rels{width:2%%;}\n"));
    WSContentSend_P (PSTR ("div.relu{width:10%%;text-align:left;}\n"));
    WSContentSend_P (PSTR ("</style>\n"));
  }

  // set reception status
  if (teleinfo_meter.serial == TIC_SERIAL_GPIO) pstr_alert = PSTR ("TInfo Rx non configuré");
    else if (teleinfo_meter.serial == TIC_SERIAL_SPEED) pstr_alert = PSTR ("Vitesse non configurée");
    else if (teleinfo_meter.serial == TIC_SERIAL_FAILED) pstr_alert = PSTR ("Problème d'initialisation");
    else if (TasmotaGlobal.global_state.network_down) pstr_alert = PSTR ("Connexion réseau en cours");
    else if (teleinfo_meter.nb_message == 0) pstr_alert = PSTR ("Aucune donnée publiée");
    else if (teleinfo_contract.changed) pstr_alert = PSTR ("Nouveau contrat détecté");
    else pstr_alert = nullptr;

  // if needed, display warning
  if (pstr_alert != nullptr) 
  {
    WSContentSend_P (PSTR ("<div class='tic' style='margin:-2px 0px 4px 0px;font-size:16px;'>"));
    WSContentSend_P (PSTR ("<div class='tich' style='font-size:20px;'>⚠️</div><div class='warn'>%s</div><div class='tich'></div>"), pstr_alert);
    WSContentSend_P (PSTR ("</div>\n"));
  }

  // if reception is active
  if (teleinfo_meter.nb_message > 0)
  {
    // if alert is published, add separator
    if (pstr_alert != nullptr) WSContentSend_P (PSTR ("<hr>\n"));

    // meter model, manufacturer and year
    if (teleinfo_meter.year != 0)
    {
      // model
      TeleinfoMeterGetModel (str_text, sizeof (str_text), str_color, sizeof (str_color));
      WSContentSend_P (PSTR ("<div class='tic' style='margin-bottom:4px;'><div style='width:100%%;text-align:center;font-size:14px;font-weight:bold;'>%s</div></div>\n"), str_text);

      // manufacturer and year
      TeleinfoMeterGetManufacturer (str_text, sizeof (str_text));
      WSContentSend_P (PSTR ("<div class='tic' style='margin:0px 4px;'><div style='width:85%%;text-align:left;font-size:12px;'>%s"), str_text);
      if (strlen (str_color) > 0) WSContentSend_P (PSTR ("&nbsp;<span>%s</span>"), str_color);
      WSContentSend_P (PSTR ("</div><div style='width:15%%;text-align:right;font-size:12px;'>%u</div></div>\n"), teleinfo_meter.year);
    }

    //   consommation
    // ----------------

    if (teleinfo_conso.total_wh > 0)
    {
      // separator
      WSContentSend_P (PSTR ("<hr>\n"));

      // header
      WSContentSend_P (PSTR ("<div class='tic' style='margin:-6px 0px 2px 0px;'>\n"));
      WSContentSend_P (PSTR ("<div style='width:48%%;text-align:left;font-weight:bold;'>Consommation</div>\n"));

      // over voltage
      if (teleinfo_message.stge.over_volt == 0)  { strcpy_P (str_color, PSTR ("none")); str_text[0] = 0; }
        else { strcpy_P (str_color, PSTR ("#900")); strcpy_P (str_text, PSTR ("V")); }
      WSContentSend_P (PSTR ("<div class='warn' style='width:10%%;background:%s;'>%s</div>\n"), str_color, str_text);
      WSContentSend_P (PSTR ("<div class='tics'></div>\n"));

      // over load
      if (teleinfo_message.stge.over_volt == 0) { strcpy_P (str_color, PSTR ("none")); str_text[0] = 0; }
        else { strcpy_P (str_color, PSTR ("#900")); strcpy_P (str_text, PSTR ("VA")); }
      WSContentSend_P (PSTR ("<div class='warn' style='width:12%%;background:%s;'>%s</div>\n"), str_color, str_text);
      WSContentSend_P (PSTR ("<div class='tics'></div>\n"));

      // preavis
      if (teleinfo_meter.preavis.level == TIC_PREAVIS_NONE) { strcpy_P (str_color, PSTR ("none")); str_text[0] = 0; }
        else if (teleinfo_meter.preavis.level == TIC_PREAVIS_WARNING) { strcpy_P (str_color, PSTR ("#d70")); strcpy (str_text, teleinfo_meter.preavis.str_label); }
        else { strcpy_P (str_color, PSTR ("#900")); strcpy (str_text, teleinfo_meter.preavis.str_label); }
      WSContentSend_P (PSTR ("<div class='warn' style='width:14%%;background:%s;'>%s</div>\n"), str_color, str_text);

      WSContentSend_P (PSTR ("<div style='width:12%%;'></div>\n"));
      WSContentSend_P (PSTR ("</div>\n"));

      // bar graph per phase
      if (teleinfo_contract.ssousc > 0)
        for (phase = 0; phase < teleinfo_contract.phase; phase++)
        {
          WSContentSend_P (PSTR ("<div class='tic bar'>\n"));

          // display voltage
          if (teleinfo_message.stge.over_volt == 1) strcpy_P (str_text, PSTR ("font-weight:bold;color:red;"));
            else str_text[0] = 0;
          WSContentSend_P (PSTR ("<div class='barh' style='%s'>%d V</div>\n"), str_text, teleinfo_conso.phase[phase].voltage);

          // calculate percentage and value
          value = 100 * teleinfo_conso.phase[phase].papp / teleinfo_contract.ssousc;
          if (value > 100) value = 100;
          if (teleinfo_conso.phase[phase].papp > 0) ltoa (teleinfo_conso.phase[phase].papp, str_text, 10); 
            else str_text[0] = 0;

          // calculate color
          if (value < 50) green = TIC_RGB_GREEN_MAX; else green = TIC_RGB_GREEN_MAX * 2 * (100 - value) / 100;
          if (value > 50) red = TIC_RGB_RED_MAX; else red = TIC_RGB_RED_MAX * 2 * value / 100;
          sprintf_P (str_color, PSTR ("#%02x%02x00"), (ulong)red, (ulong)green);

          // display bar graph percentage
          WSContentSend_P (PSTR ("<div class='barm'><div class='barv' style='width:%d%%;background-color:%s;'>%s</div></div>\n"), value, str_color, str_text);
          WSContentSend_P (PSTR ("<div class='tics'></div><div class='ticu'>VA</div>\n"));
          WSContentSend_P (PSTR ("</div>\n"));
        }

      // active power
      value = 0; 
      for (phase = 0; phase < teleinfo_contract.phase; phase++) value += teleinfo_conso.phase[phase].pact;
      WSContentSend_P (PSTR ("<div class='tic'><div class='tich'></div>"));
      WSContentSend_P (PSTR ("<div class='tict'>cosφ <b>%d,%02d</b></div><div class='ticv'>%d</div>"), teleinfo_conso.cosphi.value / 1000, teleinfo_conso.cosphi.value % 1000 / 10, value);
      WSContentSend_P (PSTR ("<div class='tics'></div><div class='ticu'>W</div></div>\n"));

      // today's total
      WSContentSend_P (PSTR ("<div class='tic'><div class='tich'></div>"));
      WSContentSend_P (PSTR ("<div class='tict'>Aujourd'hui</div><div class='ticv'>%d,%03d</div>"), teleinfo_conso.today_wh / 1000, teleinfo_conso.today_wh % 1000);
      WSContentSend_P (PSTR ("<div class='tics'></div><div class='ticu'>kWh</div></div>\n"));

      // yesterday's total
      WSContentSend_P (PSTR ("<div class='tic'><div class='tich'></div>"));
      WSContentSend_P (PSTR ("<div class='tict'>Hier</div><div class='ticv'>%d,%03d</div>"), teleinfo_conso.yesterday_wh / 1000, teleinfo_conso.yesterday_wh % 1000);
      WSContentSend_P (PSTR ("<div class='tics'></div><div class='ticu'>kWh</div></div>\n"));

      // display virtual relay state
      if (teleinfo_config.relay)
      {
        // virtual relay state
        WSContentSend_P (PSTR ("<div class='rel'>\n"));
        WSContentSend_P (PSTR ("<div class='relh'>Relais</div>\n"));
        for (index = 0; index < 8; index ++) 
        {
          if (TeleinfoRelayStatus (index) == 1) strcpy_P (str_text, PSTR ("🟢"));
            else strcpy_P (str_text, PSTR ("🔴"));
          WSContentSend_P (PSTR ("<div class='relr'>%s</div>\n"), str_text);
        }
        WSContentSend_P (PSTR ("</div>\n"));

        // virtual relay label
        WSContentSend_P (PSTR ("<div class='rel' style='margin:-17px 0px 10px 0px;'>\n"));
        WSContentSend_P (PSTR ("<div class='relh'></div>\n"));
        for (index = 0; index < 8; index ++) WSContentSend_P (PSTR ("<div class='reli'>%u</div>\n"), index + 1);
        WSContentSend_P (PSTR ("</div>\n"));
      }
    }

    //   production
    // --------------

    if (teleinfo_prod.total_wh > 0)
    {
      // separator
      WSContentSend_P (PSTR ("<hr>\n"));

      // header
      WSContentSend_P (PSTR ("<div style='padding:0px 0px 5px 0px;text-align:left;font-weight:bold;'>Production</div>\n"));

      // bar graph percentage
      if (teleinfo_contract.ssousc > 0)
      {            
        // calculate percentage and value
        value = 100 * teleinfo_prod.papp / teleinfo_contract.ssousc;
        if (value > 100) value = 100;
        if (teleinfo_prod.papp > 0) ltoa (teleinfo_prod.papp, str_text, 10); 
            else str_text[0] = 0;

        // calculate color
        if (value < 50) red = TIC_RGB_GREEN_MAX; else red = TIC_RGB_GREEN_MAX * 2 * (100 - value) / 100;
        if (value > 50) green = TIC_RGB_RED_MAX; else green = TIC_RGB_RED_MAX * 2 * value / 100;
        sprintf_P (str_color, PSTR ("#%02x%02x%02x"), red, green, 0);

        // display bar graph percentage
        WSContentSend_P (PSTR ("<div class='tic bar'>\n"));
        WSContentSend_P (PSTR ("<div class='tich'></div>\n"));
        WSContentSend_P (PSTR ("<div class='barm'><div class='barv' style='width:%d%%;background-color:%s;'>%s</div></div>\n"), value, str_color, str_text);
        WSContentSend_P (PSTR ("<div class='tics'></div><div class='ticu'>VA</div>\n"));
        WSContentSend_P (PSTR ("</div>\n"));
      }

      // active power
      WSContentSend_P (PSTR ("<div class='tic'><div class='tich'></div>"));
      WSContentSend_P (PSTR ("<div class='tict'>cosφ <b>%d,%02d</b></div><div class='ticv'>%d</div>"), teleinfo_prod.cosphi.value / 1000, teleinfo_prod.cosphi.value % 1000 / 10, teleinfo_prod.pact);
      WSContentSend_P (PSTR ("<div class='tics'></div><div class='ticu'>W</div></div>\n"));

      // today's total
      WSContentSend_P (PSTR ("<div class='tic'><div class='tich'></div>"));
      WSContentSend_P (PSTR ("<div class='tict'>Aujourd'hui</div><div class='ticv'>%d,%03d</div>"), teleinfo_prod.today_wh / 1000, teleinfo_prod.today_wh % 1000);
      WSContentSend_P (PSTR ("<div class='tics'></div><div class='ticu'>kWh</div></div>\n"));

      // yesterday's total
      WSContentSend_P (PSTR ("<div class='tic'><div class='tich'></div>"));
      WSContentSend_P (PSTR ("<div class='tict'>Hier</div><div class='ticv'>%d,%03d</div>\n"), teleinfo_prod.yesterday_wh / 1000, teleinfo_prod.yesterday_wh % 1000);
      WSContentSend_P (PSTR ("<div class='tics'></div><div class='ticu'>kWh</div></div>\n"));

      // display production relay and average production value
      if (teleinfo_config.relay)
      {
        // production relay state
        WSContentSend_P (PSTR ("<div class='rel'>\n"));
        WSContentSend_P (PSTR ("<div class='relh'>Relai</div>\n"));
        if (teleinfo_prod.relay == 1) strcpy_P (str_text, PSTR ("🟢"));
          else strcpy_P (str_text, PSTR ("🔴"));
        WSContentSend_P (PSTR ("<div class='relr'>%s</div>\n"), str_text);
        WSContentSend_P (PSTR ("<div class='relv'>%u</div>\n"), (uint32_t)teleinfo_prod.pact_avg);
        WSContentSend_P (PSTR ("<div class='rels'></div>\n"));
        WSContentSend_P (PSTR ("<div class='relu'>W</div>\n"));
        WSContentSend_P (PSTR ("</div>\n"));

        // production relay label
        WSContentSend_P (PSTR ("<div class='rel' style='margin:-17px 0px 10px 0px;'>\n"));
        WSContentSend_P (PSTR ("<div class='relh'></div>\n"));
        WSContentSend_P (PSTR ("<div class='reli'>P</div>\n"));

        WSContentSend_P (PSTR ("</div>\n"));
      }
    }

    //   contract
    // --------------

    // separator
    WSContentSend_P (PSTR ("<hr>\n"));

    // get contract data
    TeleinfoContractGetName (str_text, sizeof (str_text));
    GetTextIndexed (str_color, sizeof (str_color), teleinfo_contract.mode, kTeleinfoModeIcon);

    // header
    WSContentSend_P (PSTR ("<div class='tic' style='margin:-2px 0px 4px 0px;font-size:15px;'>\n"));
    WSContentSend_P (PSTR ("<div class='tich'>%s</div>\n"), str_color);
    WSContentSend_P (PSTR ("<div style='width:54%%;text-align:left;font-weight:bold;'>%s</div>\n"), str_text);
    if (teleinfo_contract.phase > 1) sprintf_P (str_text, PSTR ("%ux"), teleinfo_contract.phase);
      else str_text[0] = 0;
    if (teleinfo_contract.index != UINT8_MAX) WSContentSend_P (PSTR ("<div style='width:18%%;text-align:right;font-weight:bold;'>%s%d</div>\n"), str_text, teleinfo_contract.ssousc / 1000);
    if (teleinfo_contract.unit == TIC_UNIT_KVA) strcpy_P (str_text, PSTR ("kVA")); 
      else if (teleinfo_contract.unit == TIC_UNIT_KW) strcpy_P (str_text, PSTR ("kW"));
      else str_text[0] = 0;
    WSContentSend_P (PSTR ("<div class='tics'></div><div style='width:10%%;text-align:left;'>%s</div>\n"), str_text);
    WSContentSend_P (PSTR ("</div>\n"));

    // production
    if (teleinfo_prod.total_wh != 0)
    {
      WSContentSend_P (PSTR ("<div class='tic'>"));

      // period name
      WSContentSend_P (PSTR ("<div class='tich'></div>"));
      if (teleinfo_prod.papp == 0) WSContentSend_P (PSTR ("<div style='width:36%%;'>Production</div>"));
        else WSContentSend_P (PSTR ("<div style='width:36%%;color:%s;background-color:%s;border-radius:6px;'>Production</div>"), kTeleinfoLevelCalProdText, kTeleinfoLevelCalProd);

      // counter value
      lltoa (teleinfo_prod.total_wh / 1000, str_text, 10);
      value = (long)(teleinfo_prod.total_wh % 1000);
      WSContentSend_P (PSTR ("<div style='width:36%%;text-align:right;'>%s,%03d</div>"), str_text, value);
      WSContentSend_P (PSTR ("<div class='tics'></div><div class='ticu'>kWh</div>"));
      WSContentSend_P (PSTR ("</div>\n"));
      }
    
    // conso periods
    for (index = 0; index < teleinfo_contract.period_qty; index++)
    {
      if (teleinfo_conso.index_wh[index] > 0)
      {
        WSContentSend_P (PSTR ("<div class='tic'>"));

        // period name
        WSContentSend_P (PSTR ("<div class='tich'></div>"));
        WSContentSend_P (PSTR ("<div style='width:36%%;"));
        if (teleinfo_contract.period == index)
        {
          // color level
          level = TeleinfoPeriodGetLevel ();
          GetTextIndexed (str_color, sizeof (str_color), level, kTeleinfoLevelCalRGB);
          if (level == TIC_LEVEL_WHITE) WSContentSend_P (PSTR ("color:black;"));
          WSContentSend_P (PSTR ("background-color:%s;border-radius:6px;"), str_color);

          // hp / hc opacity
          hchp = TeleinfoPeriodGetHP ();
          if (hchp == 0) WSContentSend_P (PSTR ("opacity:75%%;"));
        } 
        TeleinfoPeriodGetLabel (str_text, sizeof (str_text), index);
        WSContentSend_P (PSTR ("'>%s</div>"), str_text);

        // counter value
        lltoa (teleinfo_conso.index_wh[index] / 1000, str_text, 10);
        value = (long)(teleinfo_conso.index_wh[index] % 1000);
        WSContentSend_P (PSTR ("<div style='width:36%%;text-align:right;'>%s,%03d</div>"), str_text, value);
        WSContentSend_P (PSTR ("<div class='tics'></div><div class='ticu'>kWh</div>"));
        WSContentSend_P (PSTR ("</div>\n"));
      }
    }

    //   calendar
    // ------------

    if (teleinfo_config.calendar)
    {
      // separator
      WSContentSend_P (PSTR ("<hr>\n"));

      // calendar styles
      WSContentSend_P (PSTR ("<style>\n"));
      WSContentSend_P (PSTR ("div.cal{display:flex;margin:2px 0px;padding:1px;}\n"));
      WSContentSend_P (PSTR ("div.cal div{padding:0px;}\n"));
      WSContentSend_P (PSTR ("div.calh{width:20.8%%;padding:0px;font-size:10px;text-align:left;}\n"));
      for (index = 0; index < TIC_LEVEL_MAX; index ++)
      {
        GetTextIndexed (str_color, sizeof (str_color), index, kTeleinfoLevelCalRGB);  
        WSContentSend_P (PSTR ("div.cal%u{width:3.3%%;padding:1px 0px;background-color:%s;}\n"), index, str_color);
      }
      WSContentSend_P (PSTR ("</style>\n"));

      // hour scale
      WSContentSend_P (PSTR ("<div class='cal' style='padding:0px;font-size:10px;'>\n"));
      WSContentSend_P (PSTR ("<div style='width:20.8%%;text-align:left;font-weight:bold;font-size:12px;'>Période</div>\n"));
      WSContentSend_P (PSTR ("<div style='width:13.2%%;text-align:left;'>%uh</div>\n"), 0);
      WSContentSend_P (PSTR ("<div style='width:13.2%%;'>%uh</div>\n"), 6);
      WSContentSend_P (PSTR ("<div style='width:26.4%%;'>%uh</div>\n"), 12);
      WSContentSend_P (PSTR ("<div style='width:13.2%%;'>%uh</div>\n"), 18);
      WSContentSend_P (PSTR ("<div style='width:13.2%%;text-align:right;'>%uh</div>\n"), 24);
      WSContentSend_P (PSTR ("</div>\n"));

      // loop thru days
      for (day = TIC_DAY_TODAY; day < TIC_DAY_MAX; day ++)
      {
        WSContentSend_P (PSTR ("<div class='cal'>\n"));

        // display day
        GetTextIndexed (str_text, sizeof (str_text), day, kTeleinfoPeriodDay);
        WSContentSend_P (PSTR ("<div class='calh'>%s</div>\n"), str_text);

        // display hourly slots
        for (hour = 0; hour < 24; hour ++)
        {
          // slot display beginning
          slot  = hour * 2;
          level = TeleinfoDriverCalendarGetLevel (day, slot, false);
          WSContentSend_P (PSTR ("<div class='cal%u' style='"), level);

          // set specific opacity for HC
          hchp = TeleinfoDriverCalendarGetHP (day, slot);
          if (hchp == 0) WSContentSend_P (PSTR ("opacity:75%%;"));

          // first and last segment specificities
          if (hour == 0) WSContentSend_P (PSTR ("border-top-left-radius:6px;border-bottom-left-radius:6px;"));
            else if (hour == 23) WSContentSend_P (PSTR ("border-top-right-radius:6px;border-bottom-right-radius:6px;"));

          // if dealing with current hour
          if ((day == TIC_DAY_TODAY) && (hour == (teleinfo_meter.slot / 2)))
          {
            GetTextIndexed (str_text, sizeof (str_text), level, kTeleinfoLevelCalDot);  
            WSContentSend_P (PSTR ("font-size:9px;'>%s</div>\n"), str_text);
          }
          else WSContentSend_P (PSTR ("'></div>\n"));
        }
        WSContentSend_P (PSTR ("</div>\n"));
      }
    }

    //   counters
    // ------------

    // separator
    WSContentSend_P (PSTR ("<hr>\n"));

    // counter styles
    period  = 0;
    percent = 100;
    if (teleinfo_led.status >= TIC_LED_STEP_NODATA) period = arrTicLedOn[teleinfo_led.status] + arrTicLedOff[teleinfo_led.status];
    if (period > 0) percent = 100 - (100 * arrTicLedOn[teleinfo_led.status] / period);
    WSContentSend_P (PSTR ("<style>\n"));
    WSContentSend_P (PSTR ("div.count{width:47%%;}\n"));
    WSContentSend_P (PSTR ("div.error{color:#c00;}\n"));
    WSContentSend_P (PSTR ("div.reset{color:grey;}\n"));
    WSContentSend_P (PSTR ("div.light{width:6%%;font-size:14px;animation:animate %us linear infinite;}\n"), period / 1000);
    WSContentSend_P (PSTR ("@keyframes animate{ 0%%{ opacity:0;} %u%%{ opacity:0;} %u%%{ opacity:1;} 100%%{ opacity:1;}}\n"), percent - 1, percent);
    WSContentSend_P (PSTR ("</style>\n"));

    // check contract level
    level = TIC_LEVEL_NONE;
    if (teleinfo_config.led_period == 1) level = TeleinfoPeriodGetLevel ();

    // set reception LED color
    if (teleinfo_led.status < TIC_LED_STEP_ERR) str_text[0] = 0;
      else GetTextIndexed (str_text, sizeof (str_text), level, kTeleinfoLevelDot);

    // counters and status LED
    WSContentSend_P (PSTR ("<div class='tic'>\n"));
    WSContentSend_P (PSTR ("<div class='count'>%d trames</div>\n"), teleinfo_meter.nb_message);
    WSContentSend_P (PSTR ("<div class='light'>%s</div>\n"), str_text);
    WSContentSend_P (PSTR ("<div class='count'>%d cosφ</div>\n"), teleinfo_conso.cosphi.quantity + teleinfo_prod.cosphi.quantity);
    WSContentSend_P (PSTR ("</div>\n"));

    // if reset or more than 1% errors, display counters
    if (teleinfo_meter.nb_line > 0) value = (long)(teleinfo_meter.nb_error * 10000 / teleinfo_meter.nb_line);
      else value = 0;
    if (teleinfo_config.error || (value >= 100))
    {
      WSContentSend_P (PSTR ("<div class='tic'>\n"));
      if (teleinfo_meter.nb_error == 0) strcpy_P (str_text, PSTR ("white")); else strcpy_P (str_text, PSTR ("red"));
      WSContentSend_P (PSTR ("<div class='count error'>%d erreurs<small> (%d.%02d%%)</small></div>\n"), (long)teleinfo_meter.nb_error, value / 100, value % 100);
      WSContentSend_P (PSTR ("<div class='light'></div>\n"));
      if (teleinfo_meter.nb_reset == 0) strcpy_P (str_text, PSTR ("reset")); else strcpy_P (str_text, PSTR ("error"));
      WSContentSend_P (PSTR ("<div class='count %s'>%d reset</div>\n"), str_text, teleinfo_meter.nb_reset);
      WSContentSend_P (PSTR ("</div>\n"));
    }
  }

  // end
  WSContentSend_P (PSTR ("</div>\n"));
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
  for (index = 0; index < teleinfo_message.line_last; index ++)
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
  WSContentSend_P (PSTR (" httpData.open('GET','%s',true);\n"), PSTR (TIC_PAGE_TIC_UPD));

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
  WSContentSend_P (PSTR ("    for (i=num_param;i<%d;i++){\n"), teleinfo_message.line_max);
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
  WSContentSend_P (PSTR ("<div class='count'>✉️ <span id='msg'>%u</span></div>\n"), teleinfo_meter.nb_message);
  WSContentSend_P (PSTR ("<div class='head'>Messages</div>\n"));
  WSContentSend_P (PSTR ("</div>\n"));

  // display table with header
  WSContentSend_P (PSTR ("<div class='table'><table>\n"));
  WSContentSend_P (PSTR ("<tr><th class='label'>🏷️</th><th class='value'>📄</th><th>✅</th></tr>\n"));

  // loop to display TIC messsage lines
  for (index = 0; index < teleinfo_message.line_max; index ++)
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

// Teleinfo energy sensor
bool Xdrv98 (uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
    case FUNC_INIT:
      TeleinfoDriverInit ();
      TeleinfoTCPInit ();
      TeleinfoHassInit ();
      TeleinfoHomieInit ();
      TeleinfoDomoticzInit ();
      TeleinfoThingsboardInit ();
      TeleinfoRelayInit ();
      if (TeleinfoDriverIsPowered ()) TeleinfoGraphInit ();
#ifdef USE_UFILESYS
      if (TeleinfoDriverIsPowered ()) TeleinfoHistoInit ();
#endif
#ifdef ESP32
      TeleinfoInfluxDbInit ();
      if (TeleinfoDriverIsPowered ())
      {
        TeleinfoRteInit ();
        TeleinfoAwtrixLoadConfig ();
      }
#endif  // ESP32
#ifdef USE_WINKY
      TeleinfoWinkyInit ();
#endif  // USE_WINKY
      break;

    case FUNC_COMMAND:
      result = DecodeCommand (kTeleinfoDriverCommands, TeleinfoDriverCommand);
      if (!result) result = DecodeCommand (kTCPServerCommands, TCPServerCommand);
      if (!result) result = DecodeCommand (kTeleinfoHassCommands, TeleinfoHassCommand);
      if (!result) result = DecodeCommand (kTeleinfoHomieCommands, TeleinfoHomieCommand);
      if (!result) result = DecodeCommand (kTeleinfoDomoticzCommands, TeleinfoDomoticzCommand);
      if (!result) result = DecodeCommand (kTeleinfoThingsboardCommands, TeleinfoThingsboardCommand);
      if (!result) result = DecodeCommand (kTeleinfoRelayCommands, TeleinfoRelayCommand);
#ifdef ESP32
      if (!result) result = DecodeCommand (kTeleinfoInfluxDbCommands, TeleinfoInfluxDbCommand);
      if (!result) result = DecodeCommand (kRteCommands,     RteCommand);
      if (!result) result = DecodeCommand (kEcowattCommands, EcowattCommand);
      if (!result) result = DecodeCommand (kTempoCommands,   TempoCommand);
      if (!result) result = DecodeCommand (kPointeCommands,  PointeCommand);
      if (!result && TeleinfoDriverIsPowered ()) result = DecodeCommand (kTeleinfoAwtrixCommands, TeleinfoAwtrixCommand);
#endif  // ESP32
#ifdef USE_WINKY
      if (!result) result = DecodeCommand (kTeleinfoWinkyCommands, TeleinfoWinkyCommand);
#endif  // USE_WINKY
      break;

    case FUNC_SAVE_BEFORE_RESTART:
      TeleinfoDriverSaveBeforeRestart ();
      if (TeleinfoDriverIsPowered ()) TeleinfoGraphSaveBeforeRestart ();
#ifdef USE_UFILESYS
      if (TeleinfoDriverIsPowered ()) TeleinfoHistoSaveBeforeRestart ();
#endif
#ifdef USE_WINKY
      TeleinfoWinkySaveBeforeRestart ();
#endif  // USE_WINKY
      break;

    case FUNC_EVERY_50_MSECOND:
      TeleinfoDriverEvery50ms ();
      break;

    case FUNC_EVERY_100_MSECOND:
      TeleinfoTCPEvery100ms ();
      TeleinfoHomieEvery100ms ();
#ifdef USE_LIGHT
      TeleinfoLedUpdate ();
#endif
#ifdef USE_WINKY
      TeleinfoWinkyEvery100ms ();
#endif  // USE_WINKY
      break;

    case FUNC_EVERY_250_MSECOND:
      TeleinfoDriverEvery250ms ();
      TeleinfoHassEvery250ms ();
      TeleinfoHomieEvery250ms ();
      TeleinfoDomoticzEvery250ms ();
      TeleinfoThingsboardEvery250ms ();
#ifdef USE_WINKY
      TeleinfoWinkyEvery250ms ();
#endif  // USE_WINKY
      break;

    case FUNC_EVERY_SECOND:
      TeleinfoRelayEverySecond ();
      if (TeleinfoDriverIsPowered ())
      {
        TeleinfoGraphEverySecond ();
#ifdef USE_UFILESYS
        TeleinfoHistoEverySecond ();
#endif  // USE_UFILESYS
#ifdef ESP32
        TeleinfoAwtrixEverySecond ();
#endif  // ESP32
      }
#ifdef ESP32
      TeleinfoInfluxDbEverySecond ();
      TeleinfoRteEverySecond ();
#endif  // ESP32
#ifdef USE_WINKY
      TeleinfoWinkyEverySecond ();
#endif  // USE_WINKY
      break;

    case FUNC_JSON_APPEND:
      TeleinfoDriverTeleperiod ();
#ifdef ESP32
      TeleinfoRteTeleperiod ();
#endif  // ESP32
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_MAIN_BUTTON:
      TeleinfoDriverWebMainButton ();
      if (TeleinfoDriverIsPowered ())
      {
        TeleinfoGraphWebMainButton ();
#ifdef USE_UFILESYS
        TeleinfoHistoWebMainButton ();
#endif  // USE_UFILESYS
      }
      break;

    case FUNC_WEB_ADD_BUTTON:
      TeleinfoDriverWebConfigButton ();
      TeleinfoRelayWebAddButton ();
#ifdef ESP32
      TeleinfoInfluxDbWebConfigButton ();
      if (TeleinfoDriverIsPowered ()) TeleinfoAwtrixWebConfigButton ();
#endif  // ESP32
      break;

    case FUNC_WEB_SENSOR:
      TeleinfoDriverWebSensor ();
#ifdef ESP32
      TeleinfoRteWebSensor ();
#endif  // ESP32
#ifdef USE_WINKY
      TeleinfoWinkyWebSensor ();
#endif  // USE_WINKY
      break;

    case FUNC_WEB_ADD_HANDLER:
      Webserver->on (F (TIC_PAGE_TIC_CFG),  TeleinfoDriverWebPageConfigure);
      Webserver->on (F (TIC_PAGE_TIC_MSG),  TeleinfoDriverWebPageMessage  );
      Webserver->on (F (TIC_PAGE_TIC_UPD),  TeleinfoDriverWebTicUpdate    );
      Webserver->on (F ("/" D_PAGE_TELEINFO_RELAY_CONFIG), TeleinfoRelayWebConfigure);
      if (TeleinfoDriverIsPowered ()) 
      {
        Webserver->on (FPSTR (PSTR_GRAPH_PAGE),       TeleinfoGraphWebDisplayPage);
        Webserver->on (FPSTR (PSTR_GRAPH_PAGE_DATA),  TeleinfoGraphWebUpdateData);
        Webserver->on (FPSTR (PSTR_GRAPH_PAGE_CURVE), TeleinfoGraphWebUpdateCurve);
#ifdef USE_UFILESYS
        Webserver->on (FPSTR (PSTR_PAGE_HISTO),       TeleinfoHistoWebPage);
#endif
      }
#ifdef ESP32
      Webserver->on (F (INFLUXDB_PAGE_CFG), TeleinfoInfluxDbWebPageConfigure);
      Webserver->on (F (AWTRIX_PAGE_CFG),   TeleinfoAwtrixWebPageConfigure);
#endif  // ESP32
    break;

#endif    // USE_WEBSERVER
  }

  return result;
}

#endif      // USE_TELEINFO
#endif      // USE_ENERGY_SENSOR
