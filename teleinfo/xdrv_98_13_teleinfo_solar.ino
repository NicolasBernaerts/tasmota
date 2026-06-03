/*
xdrv_98_13_teleinfo_solar.ino - Handle access to solar production and forecast

  Only compatible with ESP32

  Copyright (C) 2022-2025  Nicolas Bernaerts
    25/08/2025 v1.0 - Creation
    28/08/2025 v1.1 - Switch API from POST to GET
    07/09/2025 v1.2 - Set https timeout to 3 sec
                      Limit publications to 1 per sec.
    19/09/2025 v1.3 - Hide and Show with click on main page display
                      Add MQTT to solar production update

  solar production API is accessible thru :

    HTTP Get : your.device.ip.addr/solar?power=xxxx&total=yyyyy
    MQTT     : topic and key for instant production (W) and total production (Wh)

  Production forecast is done thru connection to https://api.forecast.solar/

  It publishes FORECAST prediction under following MQTT topic :
  
    .../tele/SOLAR
    {"Time":"2025-08-25T23:51:09","lat":44,"long":5.8,"dec":37,"az":0,"max":5000,"now":1245,"tday":12719,"tmrw":16157}

      See https://github.com/NicolasBernaerts/tasmota/tree/master/teleinfo for instructions

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

/**********************************************************\
 *             Solar production forecast
\**********************************************************/

#ifdef ESP32
#ifdef USE_TELEINFO
#ifdef USE_TELEINFO_SOLAR

#include <ArduinoJson.h>

#define TIC_SOLAR_POWER_TIMEOUT       120000      // 2 mn

#define SOLAR_VERSION                 2           // saved data version


// global constant
#define SOLAR_HTTP_TIMEOUT            3000       // HTTP connexion timeout (ms)
#define SOLAR_DELAY_INITIAL           12         // first update 12s after boot
#define SOLAR_DELAY_TELEPERIOD        5          // publish data 5s after teleperiod
#define SOLAR_DELAY_UPDATE            60         // update after 1mn
#define SOLAR_DELAY_RENEW             10800      // update every 3 hours

// commands
#define D_CMND_SOLAR                  "solar"
#define D_CMND_SOLAR_API              "api"
#define D_CMND_SOLAR_MQTT             "mqtt"
#define D_CMND_SOLAR_TOPIC            "topic"
#define D_CMND_SOLAR_PKEY             "pkey"
#define D_CMND_SOLAR_TTOPIC           "ttopic"
#define D_CMND_SOLAR_TKEY             "tkey"
#define D_CMND_FORECAST               "fcast"
#define D_CMND_FORECAST_POWER         "power"
#define D_CMND_FORECAST_DECLIN        "dec"
#define D_CMND_FORECAST_AZIMUT        "az"
#define D_CMND_FORECAST_KEY           "key"
#define D_CMND_FORECAST_UPDATE        "update"

#define D_SOLAR_CONFIGURE             "Solaire"

// path and url
#define TIC_SOLAR_PAGE_API            "/solar"
#define TIC_SOLAR_PAGE_CONFIG         "/solar-cfg"

static const char PSTR_SOLAR_DATA_FILE[] PROGMEM = "/teleinfo-solar.dat";
static const char PSTR_FORECAST_URL[]    PROGMEM = "https://api.forecast.solar/%sestimate/%d.%06d/%d.%06d/%u/%d/%d.%03d?limit=2&resolution=60";

// Commands
static const char kSolarCommands[]       PROGMEM = D_CMND_SOLAR "|"          "|_" D_CMND_SOLAR_API  "|_" D_CMND_SOLAR_MQTT  "|_"  D_CMND_SOLAR_TOPIC "|_" D_CMND_SOLAR_PKEY  "|_"  D_CMND_SOLAR_TTOPIC "|_" D_CMND_SOLAR_TKEY    ;
void (* const SolarCommand[])(void)      PROGMEM = {        &CmndTeleinfoSolar, &CmndTeleinfoSolarAPI, &CmndTeleinfoSolarMQTT, &CmndTeleinfoSolarTopic, &CmndTeleinfoSolarPKey, &CmndTeleinfoSolarTTopic, &CmndTeleinfoSolarTKey};

static const char kForecastCommands[]    PROGMEM = D_CMND_FORECAST "|"          "|_"    D_CMND_FORECAST_DECLIN    "|_"  D_CMND_FORECAST_AZIMUT  "|_"  D_CMND_FORECAST_KEY "|_"  D_CMND_FORECAST_UPDATE    ;
void (* const ForecastCommand[])(void)   PROGMEM = {        &CmndTeleinfoForecast, &CmndTeleinfoForecastDeclination, &CmndTeleinfoForecastAzimuth, &CmndTeleinfoForecastKey, &CmndTeleinfoForecastUpdate };

/************************************\
 *           Commands
\************************************/

// Help
void CmndTeleinfoSolar ()
{
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: commands about Solar Production"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - solar_api 0/1      = enable API [%u]"),                     teleinfo_solar.use_api);
  AddLog (LOG_LEVEL_INFO, PSTR (" - solar_mqtt 0/1     = enable MQTT [%u]"),                    teleinfo_solar.use_mqtt);
  AddLog (LOG_LEVEL_INFO, PSTR (" - solar_topic <val>  = MQTT W topic (null to remove) [%s]"),  teleinfo_solar.str_topic);
  AddLog (LOG_LEVEL_INFO, PSTR (" - solar_pkey <val>   = MQTT W key (null to remove) [%s]"),    teleinfo_solar.str_pkey);
  AddLog (LOG_LEVEL_INFO, PSTR (" - solar_ttopic <val> = MQTT Wh topic (null to remove) [%s]"), teleinfo_solar.str_ttopic);
  AddLog (LOG_LEVEL_INFO, PSTR (" - solar_tkey <val>   = MQTT Wh key (null to remove) [%s]"),   teleinfo_solar.str_tkey);
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: commands about Solar Forecast"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - fcast 0/1          = enable forecast [%u]"),                teleinfo_forecast.enabled);
  AddLog (LOG_LEVEL_INFO, PSTR (" - fcast_dec <val>    = solar panel declination [%d]"),        teleinfo_forecast.declination);
  AddLog (LOG_LEVEL_INFO, PSTR ("        0 : horizontal"));
  AddLog (LOG_LEVEL_INFO, PSTR ("       90 : vertical"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - fcast_az <val>     = solar panel azimuth [%d]"),            teleinfo_forecast.azimuth);
  AddLog (LOG_LEVEL_INFO, PSTR ("     -180 : north"));
  AddLog (LOG_LEVEL_INFO, PSTR ("      -90 : east"));
  AddLog (LOG_LEVEL_INFO, PSTR ("        0 : south"));
  AddLog (LOG_LEVEL_INFO, PSTR ("       90 : west"));
  AddLog (LOG_LEVEL_INFO, PSTR ("      180: north"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - fcast_key <key>    = forecast API key (null to remove) [%s]"), teleinfo_forecast.str_apikey);
  AddLog (LOG_LEVEL_INFO, PSTR (" - fcast_update       = force update"));
  ResponseCmndDone ();
}

// Enable solar production API
void CmndTeleinfoSolarAPI ()
{
  if (MiscCommandUpdateFlag (XdrvMailbox.data, teleinfo_solar.use_api)) TeleinfoSolarSaveConfig ();
  ResponseCmndNumber (teleinfo_solar.use_api);
}

// Enable solar production MQTT
void CmndTeleinfoSolarMQTT ()
{
  if (MiscCommandUpdateFlag (XdrvMailbox.data, teleinfo_solar.use_mqtt)) TeleinfoSolarSaveConfig ();
  ResponseCmndNumber (teleinfo_solar.use_mqtt);
}

// MQTT topics and keys
void CmndTeleinfoSolarTopic ()
{
  if (MiscCommandUpdateString (XdrvMailbox.data, teleinfo_solar.str_topic, sizeof (teleinfo_solar.str_topic))) TeleinfoSolarSaveConfig ();
  ResponseCmndChar (teleinfo_solar.str_topic);
}

void CmndTeleinfoSolarPKey ()
{
  if (MiscCommandUpdateString (XdrvMailbox.data, teleinfo_solar.str_pkey, sizeof (teleinfo_solar.str_pkey))) TeleinfoSolarSaveConfig ();
  ResponseCmndChar (teleinfo_solar.str_pkey);
}

void CmndTeleinfoSolarTTopic ()
{
  if (MiscCommandUpdateString (XdrvMailbox.data, teleinfo_solar.str_ttopic, sizeof (teleinfo_solar.str_ttopic))) TeleinfoSolarSaveConfig ();
  ResponseCmndChar (teleinfo_solar.str_ttopic);
}

void CmndTeleinfoSolarTKey ()
{
  if (MiscCommandUpdateString (XdrvMailbox.data, teleinfo_solar.str_tkey, sizeof (teleinfo_solar.str_tkey))) TeleinfoSolarSaveConfig ();
  ResponseCmndChar (teleinfo_solar.str_tkey);
}

// Enable solar forecast
void CmndTeleinfoForecast ()
{
  if (MiscCommandUpdateFlag (XdrvMailbox.data, teleinfo_forecast.enabled)) TeleinfoSolarSaveConfig ();
  ResponseCmndNumber (teleinfo_forecast.enabled);
}

// set forecast solar panel declination
void CmndTeleinfoForecastDeclination ()
{
  if (MiscCommandUpdateInteger (XdrvMailbox.data, teleinfo_forecast.declination, 0, 90)) TeleinfoSolarSaveConfig ();
  ResponseCmndNumber (teleinfo_forecast.declination);
}

// set forecast solar panel declination
void CmndTeleinfoForecastAzimuth ()
{
  if (MiscCommandUpdateInteger (XdrvMailbox.data, teleinfo_forecast.azimuth, -180, 180)) TeleinfoSolarSaveConfig ();
  ResponseCmndNumber (teleinfo_forecast.azimuth);
}

// set forecast API key
void CmndTeleinfoForecastKey ()
{
  if (MiscCommandUpdateString (XdrvMailbox.data, teleinfo_forecast.str_apikey, sizeof (teleinfo_forecast.str_apikey))) TeleinfoSolarSaveConfig ();
  ResponseCmndChar (teleinfo_forecast.str_apikey);
}

// Update forecast data
void CmndTeleinfoForecastUpdate ()
{
  teleinfo_forecast.time_update = LocalTime ();
  ResponseCmndDone ();
}

/****************************************\
 *              Functions
\****************************************/

// Handle MQTT teleperiod
void TeleinfoSolarAppendSensor ()
{
  char str_value[16];

  // if solar production is enabled
  if (teleinfo_solar.enabled)
  {
    // convert total
    lltoa (teleinfo_solar.total_wh, str_value, 10);

    // append SOLAR section
    MiscOptionPrepareJsonSection ();
    ResponseAppend_P (PSTR ("\"SOLAR\":{\"W\":\"%d\",\"Wh\":\"%s\"}"), teleinfo_solar.pact, str_value);
  }

  // if solar production forecast is enabled
  if (teleinfo_forecast.enabled)
  {
    // append FORECAST section
    MiscOptionPrepareJsonSection ();
    ResponseAppend_P (PSTR ("\"FORECAST\":{\"W\":\"%u\",\"tday\":\"%u\",\"tmrw\":\"%u\"}"), teleinfo_forecast.pact, teleinfo_forecast.today.total, teleinfo_forecast.tomorrow.total);
  }
}

/************************************\
 *          Configuration
\************************************/

void TeleinfoSolarLoadConfig ()
{
  uint8_t  version;
  uint32_t time_save;
  char     str_filename[24];
  File     file;

  // open file in read mode
  strcpy_P (str_filename, PSTR_SOLAR_DATA_FILE);
  if (ffsp->exists(str_filename))
  {
    file = ffsp->open (str_filename, "r");
    file.read ((uint8_t*)&version, sizeof (version));
    if (version == SOLAR_VERSION)
    {
      file.read ((uint8_t*)&time_save,          sizeof (time_save));
      file.read ((uint8_t*)&teleinfo_solar,     sizeof (teleinfo_solar));
      file.read ((uint8_t*)&teleinfo_forecast,  sizeof (teleinfo_forecast));
    }
    else AddLog (LOG_LEVEL_INFO, PSTR ("SOL: Attention, format de stockage different. Configuration Solaire réinitialisée !"));

    file.close ();
  }
}

void TeleinfoSolarSaveConfig ()
{
  uint8_t  version;
  uint32_t time_now;
  char     str_filename[24];
  File     file;

  // update production enabled flag
  teleinfo_solar.enabled = (teleinfo_solar.use_api || teleinfo_solar.use_mqtt);

  // init data
  version  = SOLAR_VERSION;
  time_now = LocalTime ();

  // open file in write mode
  strcpy_P (str_filename, PSTR_SOLAR_DATA_FILE);
  file = ffsp->open (str_filename, "w");
  if (file > 0)
  {
    file.write ((uint8_t*)&version,            sizeof (version));
    file.write ((uint8_t*)&time_now,           sizeof (time_now));
    file.write ((uint8_t*)&teleinfo_solar,     sizeof (teleinfo_solar));
    file.write ((uint8_t*)&teleinfo_forecast,  sizeof (teleinfo_forecast));
    file.close ();
  }

  // plan update with new configuration
  teleinfo_forecast.time_update = LocalTime () + SOLAR_DELAY_UPDATE;
}

/***************************************\
 *           JSON publication
\***************************************/

// publish RTE Tempo data thru MQTT
void TeleinfoSolarPublishJson ()
{
  int latitude_int, longitude_int, latitude_dec, longitude_dec;

  // if RTC not valid, ignore
  if (!RtcTime.valid) return;

  // get latitude and longitude
  latitude_int  = Settings->latitude / 1000000;
  latitude_dec  = abs (Settings->latitude) % 1000000;
  longitude_int = Settings->longitude / 1000000;
  longitude_dec = abs (Settings->longitude) % 1000000;

  // init message
  ResponseClear ();
  ResponseAppendTime ();

  // publish data
  ResponseAppend_P (PSTR (",\"lat\":%d.%06d,\"long\":%d.%06d"), latitude_int, latitude_dec, longitude_int, longitude_dec );
  ResponseAppend_P (PSTR (",\"dec\":%u,\"az\":%d,\"max\":%d"), teleinfo_forecast.declination, teleinfo_forecast.azimuth, teleinfo_config.prod_max);
  ResponseAppend_P (PSTR (",\"now\":%u,\"tday\":%u,\"tmrw\":%u"), teleinfo_forecast.pact, teleinfo_forecast.today.total, teleinfo_forecast.tomorrow.total);

  // publish message
  ResponseJsonEnd ();
  MqttPublishPrefixTopic_P (TELE, PSTR("SOLAR"), Settings->flag.mqtt_sensor_retain);
  XdrvRulesProcess (true);

  // declare publication
  teleinfo_forecast.time_publish = UINT32_MAX;
  TeleinfoDriverWebDeclare (TIC_WEB_MQTT);
}

/***********************************\
 *            API call
\***********************************/

bool TeleinfoSolarForecastUpdate ()
{
  bool     is_ok;
  uint8_t  index;
  uint32_t time_start;
  int32_t  time_response;
  int      latitude_int, longitude_int, latitude_dec, longitude_dec;
  int      http_code;
  TIME_T   tomorrow;
  char     str_today[12];
  char     str_tomorrow[12];
  char     str_key[28];
  char     str_text[128];
  String   str_result;
  JsonDocument     json_result;
  HTTPClientLight *phttp = nullptr;

  // check time
  if (!RtcTime.valid) return false;

  // check query timestamp
  if (!TeleinfoDriverWebAllow (TIC_WEB_HTTP)) return false;

  // check compulsory data
  is_ok = true;
  if (teleinfo_config.prod_max == 0) { is_ok = false; AddLog (LOG_LEVEL_INFO, PSTR ("SOL: Forecast: No peak production defined [energyconfig prod=xxxxx]")); }

  // construct URL
  if (is_ok)
  {
    // get latitude and longitude
    latitude_int  = Settings->latitude / 1000000;
    latitude_dec  = abs (Settings->latitude) % 1000000;
    longitude_int = Settings->longitude / 1000000;
    longitude_dec = abs (Settings->longitude) % 1000000;

    // generate URL
    if (strlen (teleinfo_forecast.str_apikey) > 0) sprintf_P (str_key, PSTR ("%s/"), teleinfo_forecast.str_apikey); else str_key[0] = 0;
    sprintf_P (str_text, PSTR_FORECAST_URL, str_key, latitude_int, latitude_dec, longitude_int, longitude_dec, teleinfo_forecast.declination, teleinfo_forecast.azimuth, teleinfo_config.prod_max / 1000, teleinfo_config.prod_max % 1000); 
    AddLog (LOG_LEVEL_DEBUG, PSTR ("SOL: Forecast: %s"), str_text);

    // start connexion
    phttp = new HTTPClientLight ();
    is_ok = (phttp != nullptr);
  }

  if (is_ok)
  {
    // connexion
    is_ok = phttp->begin (str_text);
    if (is_ok)
    {
      // set timeout and headers
      phttp->setTimeout (SOLAR_HTTP_TIMEOUT);
      phttp->addHeader (F ("Accept"), F ("application/json"), false, true);

      // check connexion status
      time_start = millis ();
      http_code  = phttp->GET ();
      time_response = TimePassedSince (time_start);

      // if success, collect result
      is_ok = ((http_code == HTTP_CODE_OK) || (http_code == HTTP_CODE_MOVED_PERMANENTLY));
      if (is_ok) str_result = phttp->getString ();
      is_ok = (!str_result.isEmpty ());

      // end connexion
      phttp->end ();
      
      // declare query
      TeleinfoDriverWebDeclare (TIC_WEB_HTTP);

      // log
      AddLog (LOG_LEVEL_INFO, PSTR ("SOL: Forecast: [%d] %u char in %d ms"), http_code, str_result.length (), time_response);
    }

    else AddLog (LOG_LEVEL_INFO, PSTR ("SOL: Forecast: Connexion begin failed"));

    // delete connexion
    delete phttp;
  }
  else AddLog (LOG_LEVEL_INFO, PSTR ("SOL: Forecast: Connexion creation failed"));

  // analyse received stream
  if (is_ok)
  {
    // init data
    memset (&teleinfo_forecast.today,    0, sizeof (solar_day));
    memset (&teleinfo_forecast.tomorrow, 0, sizeof (solar_day));

    // generate JSON
    deserializeJson (json_result, str_result.c_str ());

    // calculate tomorrow
    BreakTime (LocalTime () + 86400, tomorrow);

    // gerenerate today and tomorrow strings
    sprintf_P (str_today,    PSTR ("%u-%02u-%02u"), RtcTime.year,         RtcTime.month,  RtcTime.day_of_month);
    sprintf_P (str_tomorrow, PSTR ("%u-%02u-%02u"), tomorrow.year + 1970, tomorrow.month, tomorrow.day_of_month);
    
    // retrieve place
    if (!json_result[F ("message")][F ("info")][F ("place")].isNull ()) strlcpy (teleinfo_forecast.str_place, json_result[F ("message")][F ("info")][F ("place")].as<const char*>(), sizeof (teleinfo_forecast.str_place));

    // update credits
    if (!json_result[F ("message")][F ("ratelimit")][F ("remaining")].isNull ()) teleinfo_forecast.credit_left  = json_result[F ("message")][F ("ratelimit")][F ("remaining")].as<unsigned int>();
    if (!json_result[F ("message")][F ("ratelimit")][F ("limit")].isNull ())     teleinfo_forecast.credit_total = json_result[F ("message")][F ("ratelimit")][F ("limit")].as<unsigned int>();

    // recover today's forecast
    if (!json_result[F ("result")][F ("watt_hours_day")][str_today].isNull ()) teleinfo_forecast.today.total = json_result[F ("result")][F ("watt_hours_day")][str_today].as<unsigned int>();
    for (index = 0; index < 24; index ++)
    {
      sprintf_P (str_text, PSTR ("%s %02u:00:00"), str_today, index);
      if (!json_result[F ("result")][F ("watts")][str_text].isNull ()) teleinfo_forecast.today.arr_pact[index] = json_result[F ("result")][F ("watts")][str_text].as<unsigned int>();
      if (!json_result[F ("result")][F ("watt_hours_period")][str_text].isNull ()) teleinfo_forecast.today.arr_wh[index] = json_result[F ("result")][F ("watt_hours_period")][str_text].as<unsigned int>();
    }

    // recover tomorrow hourly forecast
    if (!json_result[F ("result")][F ("watt_hours_day")][str_tomorrow].isNull ()) teleinfo_forecast.tomorrow.total = json_result[F ("result")][F ("watt_hours_day")][str_tomorrow].as<unsigned int>();
    for (index = 0; index < 24; index ++)
    {
      sprintf_P (str_text, PSTR ("%s %02u:00:00"), str_tomorrow, index);
      if (!json_result[F ("result")][F ("watts")][str_text].isNull ()) teleinfo_forecast.tomorrow.arr_pact[index] = json_result[F ("result")][F ("watts")][str_text].as<unsigned int>();
      if (!json_result[F ("result")][F ("watt_hours_period")][str_text].isNull ()) teleinfo_forecast.tomorrow.arr_wh[index] = json_result[F ("result")][F ("watt_hours_period")][str_text].as<unsigned int>();
    }

    // set new update time and log
    teleinfo_forecast.time_update = LocalTime () + SOLAR_DELAY_RENEW;
    AddLog (LOG_LEVEL_INFO, PSTR ("SOL: Solar forecast updated, credit %u/%u, production is now %u W, will be %u Wh today and %u Wh tomorrow"), teleinfo_forecast.credit_left, teleinfo_forecast.credit_total, teleinfo_forecast.today.arr_pact[RtcTime.hour], teleinfo_forecast.today.total, teleinfo_forecast.tomorrow.total );
  }

  // set renew timestamp
  teleinfo_forecast.time_update = LocalTime () + SOLAR_DELAY_RENEW;

  return is_ok;
}

// called every day at midnight
void TeleinfoSolarMidnight ()
{
  // update counters
  teleinfo_solar.yesterday_wh = teleinfo_solar.today_wh;
  teleinfo_solar.today_wh     = 0;
  teleinfo_solar.midnight_wh  = teleinfo_solar.total_wh;

  // shift forecast tomorrow's data as today's data
  memcpy (&teleinfo_forecast.today, &teleinfo_forecast.tomorrow, sizeof (solar_day));
  memset (&teleinfo_forecast.tomorrow, 0, sizeof (solar_day));

  // plan publication
  teleinfo_forecast.time_publish = LocalTime () + SOLAR_DELAY_INITIAL;
}

/***********************************************************\
 *                      Callback
\***********************************************************/

// init main status
void TeleinfoSolarInit ()
{
  // init data
  memset (&teleinfo_forecast.today,    0, sizeof (solar_day));
  memset (&teleinfo_forecast.tomorrow, 0, sizeof (solar_day));
  teleinfo_forecast.str_apikey[0] = 0;
  teleinfo_forecast.str_place[0]  = 0;
  teleinfo_solar.str_topic[0]  = 0;
  teleinfo_solar.str_ttopic[0] = 0;
  teleinfo_solar.str_pkey[0]   = 0;
  teleinfo_solar.str_tkey[0]   = 0;

  // load configuration file
  TeleinfoSolarLoadConfig ();

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Run forecast to get help on Solar Production Forecast"));
}

// called every second
void TeleinfoSolarEverySecond ()
{
  uint32_t time_now;

  // check parameters
  if (!RtcTime.valid) return;
  if (TasmotaGlobal.global_state.network_down) return;

  // if needed, init current hour
  if (teleinfo_solar.hour == UINT8_MAX) teleinfo_solar.hour = RtcTime.hour;

  // if day change, update daily counters
  if (teleinfo_solar.hour > RtcTime.hour) TeleinfoSolarMidnight ();

  // handle solar production
  if (teleinfo_solar.enabled)
  {
    // check instant power validity
    if ((teleinfo_solar.pact_time != UINT32_MAX) && (TimeDifference (teleinfo_solar.pact_time, millis ()) > TIC_SOLAR_POWER_TIMEOUT))
    {
      teleinfo_solar.pact      = 0;
      teleinfo_solar.pact_time = UINT32_MAX;
    }
  }

  // handle solar forecast production
  if (teleinfo_forecast.enabled)
  {
    time_now = LocalTime ();

    // if new hour, add hourly production
    if (teleinfo_solar.hour != RtcTime.hour) teleinfo_forecast.total_wh += (long long)teleinfo_forecast.today.arr_wh[RtcTime.hour];

    // set current forecast active power
    teleinfo_forecast.pact = teleinfo_forecast.today.arr_pact[RtcTime.hour];

    // if needed, plan first update
    if (teleinfo_forecast.time_update == UINT32_MAX) teleinfo_forecast.time_update = time_now + SOLAR_DELAY_INITIAL;

    // if needed, publish data
    if (teleinfo_forecast.time_publish < time_now) TeleinfoSolarPublishJson ();

    // else if needed, ask for update
    else if (teleinfo_forecast.time_update < time_now) TeleinfoSolarForecastUpdate ();
  }

  // set current hour
  teleinfo_solar.hour = RtcTime.hour;
}

// Handle MQTT teleperiod
void TeleinfoSolarTeleperiod ()
{
  if (teleinfo_forecast.enabled) teleinfo_forecast.time_publish = LocalTime () + SOLAR_DELAY_TELEPERIOD;
}

// check and update MQTT power subsciption after disconnexion
void TeleinfoSolarMqttSubscribe ()
{
  // if not enabled, ignore
  if (!teleinfo_solar.use_mqtt) return;

  // if generic topic is defined
  if (strlen (teleinfo_solar.str_topic) > 0)
  {
    MqttSubscribe (teleinfo_solar.str_topic);
    AddLog (LOG_LEVEL_INFO, PSTR ("SOL: Subscribed to %s"), teleinfo_solar.str_topic);
  }

  // if total topic is defined and different from generic topic
  if ((strlen (teleinfo_solar.str_ttopic) > 0) && (strcmp (teleinfo_solar.str_topic, teleinfo_solar.str_ttopic) != 0))
  {
    MqttSubscribe (teleinfo_solar.str_ttopic);
    AddLog (LOG_LEVEL_INFO, PSTR ("SOL: Subscribed to %s"), teleinfo_solar.str_ttopic);
  }
}

// read received MQTT data to instant power (w) and total power (wh)
bool TeleinfoSolarMqttData ()
{
  bool found = false;
  JsonDocument json_result;

  // if not enabled, ignore
  if (!teleinfo_solar.use_mqtt) return false;

  // if dealing with generic topic
  if (strcmp (XdrvMailbox.topic, teleinfo_solar.str_topic) == 0)
  {
    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("SOL: Received %s : %s"), XdrvMailbox.topic, XdrvMailbox.data);
    found = true;

    // if instant key defined, deserialize and analyse
    if (strlen (teleinfo_solar.str_pkey) > 0) 
    {
      // deserialize
      deserializeJson (json_result, (const char*)XdrvMailbox.data);

      // look for power instant key
      if (!json_result[teleinfo_solar.str_pkey].isNull ()) teleinfo_solar.pact = json_result[teleinfo_solar.str_pkey].as<const long>();

      // look for power total key
      if (strlen (teleinfo_solar.str_tkey) > 0)
        if (!json_result[teleinfo_solar.str_tkey].isNull ()) teleinfo_solar.total_wh = (long long)json_result[teleinfo_solar.str_tkey].as<const long>();
    }

    // else, get raw value 
    else teleinfo_solar.pact = atol (XdrvMailbox.data);
  }

  // check for total topic
  else if (strcmp (XdrvMailbox.topic, teleinfo_solar.str_ttopic) == 0)
  {
    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("SOL: Received %s : %s"), XdrvMailbox.topic, XdrvMailbox.data);
    found = true;

    // if instant key defined, deserialize and analyse
    if (strlen (teleinfo_solar.str_tkey) > 0) 
    {
      // deserialize
      deserializeJson (json_result, (const char*)XdrvMailbox.data);

      // look for total key
      if (!json_result[teleinfo_solar.str_tkey].isNull ()) teleinfo_solar.total_wh = (long long)json_result[teleinfo_solar.str_tkey].as<const long>();
    }

    // else get raw value
    else teleinfo_solar.total_wh = atoll (XdrvMailbox.data);
  }

  return found;
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// TIC raw message data
void TeleinfoSolarWebAPI ()
{
  bool result = false;
  char str_value[16];

  // solar production instant power 
  if (Webserver->hasArg (F ("power")))
  {
    WebGetArg (PSTR ("power"), str_value, sizeof (str_value));
    teleinfo_solar.pact = atol (str_value);
    teleinfo_solar.pact_time = millis ();
    result = true;
  }

  // solar production glogal power 
  if (Webserver->hasArg (F ("total")))
  {
    WebGetArg (PSTR ("total"), str_value, sizeof (str_value));
    teleinfo_solar.total_wh = atoll (str_value);
    if (teleinfo_solar.midnight_wh <= 0) teleinfo_solar.midnight_wh = teleinfo_solar.total_wh;
    teleinfo_solar.today_wh = (long)(teleinfo_solar.total_wh - teleinfo_solar.midnight_wh);
    result = true;
  }

  // answer
  WSContentBegin (200, CT_PLAIN);
  if (result) WSContentSend_P (PSTR ("success"));
    else WSContentSend_P (PSTR ("error"));
  WSContentEnd ();
}

// append solar production forecast to main page
void TeleinfoSolarWebSensor ()
{
  long percentage;

  // solar production
  if (teleinfo_solar.enabled)
  {
    // instant solar production
    percentage = 0;
    if (teleinfo_config.prod_max > 0) percentage = min (100L, 100L * teleinfo_solar.pact / teleinfo_config.prod_max);

    // section
    WSContentSend_P (PSTR ("<div class='sec' onclick=\"onClickMain(\'%s?%s=%u\')\">\n"), PSTR (TIC_PAGE_DISPLAY), PSTR (CMND_TIC_MAIN), TIC_DISPLAY_SOLAR);

    // header
    WSContentSend_P (PSTR ("<div class='tic ticb'><div class='tic70l'>Production ☀️</div>"));
    if (teleinfo_config.arr_main[TIC_DISPLAY_SOLAR]) WSContentSend_P (PSTR ("<div class='tic18r'>%d.%d</div><div class='tic02'></div><div class='tic10l'>kW</div>"), teleinfo_config.prod_max / 1000, teleinfo_config.prod_max % 1000 / 100);
      else WSContentSend_P (PSTR ("<div class='tic18r'>%d</div><div class='tic02'></div><div class='tic10l'>W</div>"), teleinfo_solar.pact);
    WSContentSend_P (PSTR ("</div>\n"));


    if (teleinfo_config.arr_main[TIC_DISPLAY_SOLAR])
    {
      // bar graph percentage
      WSContentSend_P (PSTR ("<div class='tic bar'><div class='tic16l'></div><div class='bar72l'><div class='barv ph%u' style='width:%d%%;'>%d</div></div><div class='tic02'></div><div class='tic10l'>W</div></div>\n"), TIC_PHASE_SOLAR, percentage, teleinfo_solar.pact);

      // yesterday, today and total
      if (teleinfo_solar.yesterday_wh > 0) WSContentSend_P (PSTR ("<div class='tic'><div class='tic16'></div><div class='tic37l'>%s</div><div class='tic35r'>%d,%03d</div><div class='tic02'></div><div class='tic10l'>kWh</div></div>\n"), PSTR ("Hier"),        teleinfo_solar.yesterday_wh / 1000,     teleinfo_solar.yesterday_wh % 1000);
      if (teleinfo_solar.today_wh     > 0) WSContentSend_P (PSTR ("<div class='tic'><div class='tic16'></div><div class='tic37l'>%s</div><div class='tic35r'>%d,%03d</div><div class='tic02'></div><div class='tic10l'>kWh</div></div>\n"), PSTR ("Aujourd'hui"), teleinfo_solar.today_wh / 1000,         teleinfo_solar.today_wh % 1000);
      if (teleinfo_solar.total_wh     > 0) WSContentSend_P (PSTR ("<div class='tic'><div class='tic16'></div><div class='tic37l'>%s</div><div class='tic35r'>%d,%03d</div><div class='tic02'></div><div class='tic10l'>kWh</div></div>\n"), PSTR ("Total"),       (long)(teleinfo_solar.total_wh / 1000), (long)(teleinfo_solar.total_wh % 1000));
    }

    WSContentSend_P (PSTR ("</div>\n"));    // sec
  }

  // solar forecast
  if (teleinfo_forecast.enabled)
  {
    // instant solar production
    percentage = 0;
    if (teleinfo_config.prod_max > 0) percentage = min (100L, 100L * teleinfo_forecast.pact / teleinfo_config.prod_max);

    // section
    WSContentSend_P (PSTR ("<div class='sec' onclick=\"onClickMain(\'%s?%s=%u\')\">\n"), PSTR (TIC_PAGE_DISPLAY), PSTR (CMND_TIC_MAIN), TIC_DISPLAY_FORECAST);

    // header
    WSContentSend_P (PSTR ("<div class='tic ticb'><div class='tic70l'>Prévision ☀️</div>"));
    if (teleinfo_config.arr_main[TIC_DISPLAY_FORECAST]) WSContentSend_P (PSTR ("<div class='tic18r'>%d.%d</div><div class='tic02'></div><div class='tic10l'>kW</div>"), teleinfo_config.prod_max / 1000, teleinfo_config.prod_max % 1000 / 100);
      else WSContentSend_P (PSTR ("<div class='tic18r'>%d</div><div class='tic02'></div><div class='tic10l'>W</div>"), teleinfo_forecast.pact);
    WSContentSend_P (PSTR ("</div>\n"));    // tic ticb

    if (teleinfo_config.arr_main[TIC_DISPLAY_FORECAST])
    {
      // bar graph percentage
      WSContentSend_P (PSTR ("<div class='tic bar'><div class='tic16l'></div><div class='bar72l'><div class='barv ph%u' style='width:%d%%;'>%d</div></div><div class='tic02'></div><div class='tic10l'>W</div></div>\n"), TIC_PHASE_FORECAST, percentage, teleinfo_forecast.pact);

      // today and tomorrow
      if (teleinfo_forecast.today.total    > 0) WSContentSend_P (PSTR ("<div class='tic'><div class='tic16'></div><div class='tic37l'>%s</div><div class='tic35r'>%d,%03d</div><div class='tic02'></div><div class='tic10l'>kWh</div></div>\n"), PSTR ("Aujourd'hui"), teleinfo_forecast.today.total / 1000, teleinfo_forecast.today.total % 1000);
      if (teleinfo_forecast.tomorrow.total > 0) WSContentSend_P (PSTR ("<div class='tic'><div class='tic16'></div><div class='tic37l'>%s</div><div class='tic35r'>%d,%03d</div><div class='tic02'></div><div class='tic10l'>kWh</div></div>\n"), PSTR ("Demain"), (long)(teleinfo_forecast.tomorrow.total / 1000), (long)(teleinfo_forecast.tomorrow.total % 1000));

      // location
      if (teleinfo_forecast.str_place[0] != 0) WSContentSend_P (PSTR ("<hr><div class='tic'><div class='tic100'>%s</div></div>\n"), teleinfo_forecast.str_place);
    }

    WSContentSend_P (PSTR ("</div>\n"));    // sec
  }
}

// Append Teleinfo solar configuration button
void TeleinfoSolarWebConfigButton ()
{
  WSContentSend_P (PSTR ("\n<p><form action='%s' method='get'><button>Production solaire</button></form></p>\n"), PSTR (TIC_SOLAR_PAGE_CONFIG));
}

// Solar production configuration web page
void TeleinfoSolarWebPageConfigure ()
{
  bool  changed = false;
  bool  reboot  = false;
  float value;
  char  str_text[16];
  String str_value;

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess ()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg (F ("save")))
  {
    // get production api and mqtt usage
    changed |= MiscWebGetArgFlag (PSTR ("api"), teleinfo_solar.use_api);
    reboot  |= MiscWebGetArgFlag (PSTR ("mqtt"), teleinfo_solar.use_mqtt);

    // get production MQTT topics and keys
    reboot |= MiscWebGetArgString (PSTR (D_CMND_SOLAR_TOPIC),  teleinfo_solar.str_topic,  sizeof (teleinfo_solar.str_topic));
    reboot |= MiscWebGetArgString (PSTR (D_CMND_SOLAR_TTOPIC), teleinfo_solar.str_ttopic, sizeof (teleinfo_solar.str_ttopic));
    reboot |= MiscWebGetArgString (PSTR (D_CMND_SOLAR_PKEY),   teleinfo_solar.str_pkey,   sizeof (teleinfo_solar.str_pkey));
    reboot |= MiscWebGetArgString (PSTR (D_CMND_SOLAR_TKEY),   teleinfo_solar.str_tkey,   sizeof (teleinfo_solar.str_tkey));

    // production forecast
    changed |= MiscWebGetArgFlag    (PSTR (D_CMND_FORECAST),        teleinfo_forecast.enabled);
    changed |= MiscWebGetArgInteger (PSTR (D_CMND_FORECAST_POWER),  teleinfo_config.prod_max, 0L, 50000L);
    changed |= MiscWebGetArgInteger (PSTR (D_CMND_FORECAST_DECLIN), teleinfo_forecast.declination, 0, 90);
    changed |= MiscWebGetArgInteger (PSTR (D_CMND_FORECAST_AZIMUT), teleinfo_forecast.azimuth, -180, 180);
    changed |= MiscWebGetArgString  (PSTR (D_CMND_FORECAST_KEY),    teleinfo_forecast.str_apikey, sizeof (teleinfo_forecast.str_apikey));

    // latitude
    value = (float)Settings->latitude / 1000000;
    changed |= MiscWebGetArgFloat (PSTR ("lat"), value, 0,  90);
    Settings->latitude = (int)(value * 1000000);

    // longitude
    value = (float)Settings->longitude / 1000000;
    changed |= MiscWebGetArgFloat (PSTR ("long"), value, -180, 180);
    Settings->longitude = (int)(value * 1000000);

    // save configuration and reboot if needed
    if (changed || reboot) TeleinfoSolarSaveConfig ();
    if (reboot) WebRestart (1);
  }

  // beginning of form
  WSContentStart_P (PSTR (D_SOLAR_CONFIGURE));
  WSContentSendStyle ();

  // specific style
  WSContentSend_P (PSTR ("\n<style>\n"));
  WSContentSend_P (PSTR ("p,br,hr {clear:both;}\n")); 
  WSContentSend_P (PSTR ("fieldset {background:#333;margin:15px 0px;border:none;border-radius:8px;}\n")); 
  WSContentSend_P (PSTR ("fieldset.config {border-color:#888;background:%s;margin-left:12px;margin-top:4px;padding:8px 4px 0px 4px;}\n"), PSTR (COLOR_BACKGROUND)); 
  WSContentSend_P (PSTR ("fieldset.config legend {font-weight:normal;}\n"));
  WSContentSend_P (PSTR ("legend {font-weight:bold;margin-bottom:-30px;padding:5px;color:#888;background:transparent;}\n")); 
  WSContentSend_P (PSTR ("legend:after {content:'';display:block;height:1px;margin:15px 0px;}\n"));
  WSContentSend_P (PSTR ("input,select {margin-bottom:10px;text-align:center;border:none;border-radius:4px;}\n")); 
  WSContentSend_P (PSTR ("input[type='checkbox'] {margin:0px 15px;}\n")); 
  WSContentSend_P (PSTR ("input[type='text'] {text-align:left;}\n"));
  WSContentSend_P (PSTR ("p.dat {padding:0px 2px;margin:0px;}\n")); 
  WSContentSend_P (PSTR ("span {float:left;}\n"));
  WSContentSend_P (PSTR ("span.short {width:20%%;}\n")); 
  WSContentSend_P (PSTR ("span.head {width:50%%;}\n")); 
  WSContentSend_P (PSTR ("span.val {width:35%%;padding:0px;}\n")); 
  WSContentSend_P (PSTR ("span.unit {width:15%%;text-align:center;}\n"));
  WSContentSend_P (PSTR ("span.text {width:75%%;text-align:left;}\n")); 
  WSContentSend_P (PSTR ("</style>\n"));

  // form
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), PSTR (TIC_SOLAR_PAGE_CONFIG));

  // ------------------------
  //       Production  
  // ------------------------

  WSContentSend_P (PSTR ("<fieldset><legend>%s</legend>\n"), PSTR ("☀️ Production"));

  if (teleinfo_solar.use_api) strcpy_P (str_text, PSTR ("checked")); else str_text[0] = 0;
  WSContentSend_P (PSTR ("<p class='dat'><input type='checkbox' id='%s' name='%s' %s><label for='%s'>%s</label></p><br>\n"), PSTR (D_CMND_SOLAR_API), PSTR (D_CMND_SOLAR_API), str_text, PSTR (D_CMND_SOLAR_API), PSTR ("Utilisation API"));

  if (teleinfo_solar.use_mqtt) strcpy_P (str_text, PSTR ("checked")); else str_text[0] = 0;
  WSContentSend_P (PSTR ("<p class='dat'><input type='checkbox' id='%s' name='%s' onchange='onChange()' %s><label for='%s'>%s</label></p><br>\n"), PSTR (D_CMND_SOLAR_MQTT), PSTR (D_CMND_SOLAR_MQTT), str_text, PSTR (D_CMND_SOLAR_MQTT), PSTR ("Utilisation MQTT"));

  WSContentSend_P (PSTR ("<fieldset class='config' id='%s'><legend>%s</legend>\n"), PSTR ("icfg"), PSTR ("Production instantanée (W)"));
  WSContentSend_P (PSTR ("<p class='dat'><span class='short'>%s</span><span class='text'><input type='text' name='%s' value='%s'></span></p>\n"), PSTR ("Topic"), PSTR (D_CMND_SOLAR_TOPIC), teleinfo_solar.str_topic);
  WSContentSend_P (PSTR ("<p class='dat'><span class='short'>%s</span><span class='val'><input type='text' name='%s' value='%s' title='%s'></span></p>\n"), PSTR ("Clé"), PSTR (D_CMND_SOLAR_PKEY), teleinfo_solar.str_pkey, PSTR ("Nécessaire uniquement si le topic publie un JSON"));
  WSContentSend_P (PSTR ("</fieldset>\n"));

  WSContentSend_P (PSTR ("<fieldset class='config' id='%s'><legend>%s</legend>\n"), PSTR ("tcfg"), PSTR ("Production totale (Wh)"));
  WSContentSend_P (PSTR ("<p class='dat'><span class='short'>%s</span><span class='text'><input type='text' name='%s' value='%s' title='%s'></span></p>\n"), PSTR ("Topic"), PSTR (D_CMND_SOLAR_TTOPIC), teleinfo_solar.str_ttopic, PSTR ("Ne pas saisir si identique au topic précédent"));
  WSContentSend_P (PSTR ("<p class='dat'><span class='short'>%s</span><span class='val'><input type='text' name='%s' value='%s' title='%s'></span></p>\n"), PSTR ("Clé"), PSTR (D_CMND_SOLAR_TKEY), teleinfo_solar.str_tkey, PSTR ("Nécessaire uniquement si le topic publie un JSON"));
  WSContentSend_P (PSTR ("</fieldset>\n"));

  WSContentSend_P (PSTR ("</fieldset>\n"));

  // ------------------------
  //       Forecast  
  // ------------------------


  WSContentSend_P (PSTR ("<fieldset><legend>%s</legend>\n"), PSTR ("🌤️ Prévision"));

  if (teleinfo_forecast.enabled) strcpy_P (str_text, PSTR ("checked")); else str_text[0] = 0;
  WSContentSend_P (PSTR ("<p class='dat'><input type='checkbox' id='%s' name='%s' onchange='onChange()' %s><label for='%s'>%s</label></p><br>\n"), PSTR ("fcast"), PSTR ("fcast"), str_text, PSTR ("fcast"), PSTR ("Activation"));

  WSContentSend_P (PSTR ("<fieldset class='config' id='%s'>\n"), PSTR ("fcfg"));
  WSContentSend_P (PSTR ("<p class='dat'><span class='head'>%s</span><span class='val'><input type='number' name='%s' min=%d max=%d step=%d value=%d></span><span class='unit'>%s</span></p>\n"), PSTR ("Puissance max."), PSTR (D_CMND_FORECAST_POWER), 0, 50000, 50, teleinfo_config.prod_max, PSTR ("W"));
  value = (float)Settings->latitude / 1000000;
  str_value = String (value, 6);
  WSContentSend_P (PSTR ("<p class='dat'><span class='head'>%s</span><span class='val'><input type='number' name='%s' min=%d max=%d step=0.000001 value=%s></span><span class='unit'>%s</span></p>\n"), PSTR ("Latitude"), PSTR ("lat"), -90, 90, str_value.c_str (), PSTR ("°"));
  value = (float)Settings->longitude / 1000000;
  str_value = String (value, 6);
  WSContentSend_P (PSTR ("<p class='dat'><span class='head'>%s</span><span class='val'><input type='number' name='%s' min=%d max=%d step=0.000001 value=%s></span><span class='unit'>%s</span></p>\n"), PSTR ("Longitude"), PSTR ("long"), -180, 180, str_value.c_str (), PSTR ("°"));
  WSContentSend_P (PSTR ("<p class='dat'><span class='head'>%s</span><span class='val'><input type='number' name='%s' min=%d max=%d step=%d value=%d></span><span class='unit'>%s</span></p>\n"), PSTR ("Déclinaison"), PSTR (D_CMND_FORECAST_DECLIN), 0, 90, 1, teleinfo_forecast.declination, PSTR ("°"));
  WSContentSend_P (PSTR ("<p class='dat'><span class='head'>%s</span><span class='val'><input type='number' name='%s' min=%d max=%d step=%d value=%d></span><span class='unit'>%s</span></p>\n"), PSTR ("Azimuth"), PSTR (D_CMND_FORECAST_AZIMUT), -180, 180, 1, teleinfo_forecast.azimuth, PSTR ("°"));
  WSContentSend_P (PSTR ("<p class='dat'><span class='short'>%s</span><span class='text'><input type='text' name='%s' value='%s'></span></p>\n"), PSTR ("Clé"), PSTR (D_CMND_FORECAST_KEY), teleinfo_forecast.str_apikey);
  WSContentSend_P (PSTR ("</fieldset>\n"));
  WSContentSend_P (PSTR ("</fieldset>\n"));

  // display script
  WSContentSend_P (PSTR ("<script type='text/javascript'>\n"));
  WSContentSend_P (PSTR ("function onChange() {\n"));
  WSContentSend_P (PSTR ("document.getElementById('icfg').style.display=document.getElementById('prod').checked?'block':'none';\n"));
  WSContentSend_P (PSTR ("document.getElementById('tcfg').style.display=document.getElementById('prod').checked?'block':'none';\n"));
  WSContentSend_P (PSTR ("document.getElementById('fcfg').style.display=document.getElementById('fcast').checked?'block':'none';\n"));
  WSContentSend_P (PSTR ("}\n"));
  WSContentSend_P (PSTR ("onChange();\n"));
  WSContentSend_P (PSTR ("</script>\n"));

  // save button  
  // -----------
  WSContentSend_P (PSTR ("<br><p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), D_SAVE);
  WSContentSend_P (PSTR ("</form>\n"));

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

#endif  // USE_WEBSERVER

/***************************************\
 *              Interface
\***************************************/

bool XdrvTeleinfoSolar (const uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
    case FUNC_COMMAND:
      result = DecodeCommand (kSolarCommands, SolarCommand);
      break;

    case FUNC_INIT:
      TeleinfoSolarInit ();
      break;

    case FUNC_EVERY_SECOND:
      TeleinfoSolarEverySecond ();
      break;

    case FUNC_SAVE_BEFORE_RESTART:
      TeleinfoSolarSaveConfig ();
      break;

    case FUNC_JSON_APPEND:
      if (TasmotaGlobal.tele_period == 0) TeleinfoSolarTeleperiod ();
      break;

    case FUNC_MQTT_SUBSCRIBE:
      TeleinfoSolarMqttSubscribe ();
      break;

    case FUNC_MQTT_DATA:
      result = TeleinfoSolarMqttData ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_BUTTON:
      TeleinfoSolarWebConfigButton ();
      break;

      case FUNC_WEB_ADD_HANDLER:
      Webserver->on (F (TIC_SOLAR_PAGE_CONFIG), TeleinfoSolarWebPageConfigure);
      if (teleinfo_solar.use_api) Webserver->on (F (TIC_SOLAR_PAGE_API), TeleinfoSolarWebAPI);
      break;

    case FUNC_WEB_SENSOR:
      TeleinfoSolarWebSensor ();
      break;
#endif    // USE_WEBSERVER
  }

  return result;
}

#endif    // USE_TELEINFO_SOLAR
#endif    // USE_TELEINFO
#endif    // ESP32
