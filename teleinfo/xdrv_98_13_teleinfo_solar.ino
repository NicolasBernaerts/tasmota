/*
xdrv_98_13_teleinfo_solar.ino - Handle access to solar production forecast

  Only compatible with ESP32

  Copyright (C) 2022-2025  Nicolas Bernaerts
    25/08/2025 v1.0 - Creation
    28/08/2025 v1.1 - Switch API from POST to GET
    07/09/2025 v1.2 - Set https timeout to 3 sec
                      Limit publications to 1 per sec.
    19/09/2025 v1.3 - Hide and Show with click on main page display

  solar production API is accessible thru :

    your.device.ip.addr/solar?power=xxxx&total=yyyyy

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
#define D_CMND_SOLAR_ENABLE           "enable"
#define D_CMND_SOLAR_FORECAST         "forecast"
#define D_CMND_SOLAR_UPDATE           "update"
#define D_CMND_SOLAR_KEY              "key"
#define D_CMND_SOLAR_DECLINATION      "dec"
#define D_CMND_SOLAR_AZIMUTH          "az"

// path and url
#define TIC_SOLAR_PAGE_API            "/solar"

static const char PSTR_SOLAR_DATA_FILE[] PROGMEM = "/teleinfo-solar.dat";
static const char PSTR_SOLAR_URL[]       PROGMEM = "https://api.forecast.solar/%sestimate/%d.%06d/%d.%06d/%u/%d/%d.%03d?limit=2&resolution=60";

// Commands
static const char kSolarCommands[]       PROGMEM = D_CMND_SOLAR "|"   "|_" D_CMND_SOLAR_ENABLE  "|_" D_CMND_SOLAR_FORECAST  "|_" D_CMND_SOLAR_UPDATE  "|_" D_CMND_SOLAR_KEY  "|_" D_CMND_SOLAR_DECLINATION  "|_"  D_CMND_SOLAR_AZIMUTH    ;
void (* const SolarCommand[])(void)      PROGMEM = { &CmndTeleinfoSolar, &CmndTeleinfoSolarEnable, &CmndTeleinfoSolarForecast, &CmndTeleinfoSolarUpdate, &CmndTeleinfoSolarKey, &CmndTeleinfoSolarDeclination, &CmndTeleinfoSolarAzimuth };

/************************************\
 *           Commands
\************************************/

// Help
void CmndTeleinfoSolar ()
{
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: commands about Solar Production Forecast"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - solar_enable <%u> = enable solar production"), teleinfo_solar.enabled);
  AddLog (LOG_LEVEL_INFO, PSTR (" - solar_forecast <%u> = enable solar forecast"), teleinfo_forecast.enabled);
  AddLog (LOG_LEVEL_INFO, PSTR (" - solar_key <%s> = API key (null to remove)"), teleinfo_forecast.str_apikey);
  AddLog (LOG_LEVEL_INFO, PSTR (" - solar_dec <%u> = solar panel declination"), teleinfo_forecast.declination);
  AddLog (LOG_LEVEL_INFO, PSTR ("        0 : horizontal"));
  AddLog (LOG_LEVEL_INFO, PSTR ("       90 : vertical"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - solar_az <%d> = solar panel azimuth"), teleinfo_forecast.azimuth);
  AddLog (LOG_LEVEL_INFO, PSTR ("     -180 : north"));
  AddLog (LOG_LEVEL_INFO, PSTR ("      -90 : east"));
  AddLog (LOG_LEVEL_INFO, PSTR ("        0 : south"));
  AddLog (LOG_LEVEL_INFO, PSTR ("       90 : west"));
  AddLog (LOG_LEVEL_INFO, PSTR ("      180: north"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - solar_update = force forecast data update"));
  ResponseCmndDone ();
}

// Enable solar production API
void CmndTeleinfoSolarEnable ()
{
  if (XdrvMailbox.data_len > 0)
  {
    teleinfo_solar.enabled = (XdrvMailbox.payload == 1);
    TeleinfoSolarSaveConfig ();
  }

  ResponseCmndNumber (teleinfo_solar.enabled);
}

// Enable solar forecast
void CmndTeleinfoSolarForecast ()
{
  if (XdrvMailbox.data_len > 0)
  {
    teleinfo_forecast.enabled = (XdrvMailbox.payload == 1);
    TeleinfoSolarSaveConfig ();
  }

  ResponseCmndNumber (teleinfo_forecast.enabled);
}

// Update data
void CmndTeleinfoSolarUpdate ()
{
  teleinfo_forecast.time_update = LocalTime ();
  ResponseCmndDone ();
}

// Forecast API key
void CmndTeleinfoSolarKey ()
{
  if (XdrvMailbox.data_len > 0)
  {
    if (strcmp_P (XdrvMailbox.data, PSTR ("null")) == 0) teleinfo_forecast.str_apikey[0] = 0;
      else strlcpy (teleinfo_forecast.str_apikey, XdrvMailbox.data, sizeof (teleinfo_forecast.str_apikey));
    TeleinfoSolarSaveConfig ();
  }
  
  ResponseCmndChar (teleinfo_forecast.str_apikey);
}

// Forecast solar panel declination
void CmndTeleinfoSolarDeclination ()
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload >= 0) && (XdrvMailbox.payload <= 90))
  {
    teleinfo_forecast.declination = (uint8_t)XdrvMailbox.payload;
    TeleinfoSolarSaveConfig ();
  }
  
  ResponseCmndNumber ((int)teleinfo_forecast.declination);
}

// Forecast solar panel declination
void CmndTeleinfoSolarAzimuth ()
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload >= -180) && (XdrvMailbox.payload <= 180))
  {
    teleinfo_forecast.azimuth = (int16_t)XdrvMailbox.payload;
    TeleinfoSolarSaveConfig ();
  }
  
  ResponseCmndNumber (teleinfo_forecast.azimuth);
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
      file.read ((uint8_t*)&time_save, sizeof (time_save));
      file.read ((uint8_t*)&teleinfo_solar, sizeof (teleinfo_solar));
      file.read ((uint8_t*)&teleinfo_forecast, sizeof (teleinfo_forecast));
    }
    file.close ();
  }
}

void TeleinfoSolarSaveConfig ()
{
  uint8_t  version;
  uint32_t time_now;
  char     str_filename[24];
  File     file;

  // init data
  version  = SOLAR_VERSION;
  time_now = LocalTime ();

  // open file in write mode
  strcpy_P (str_filename, PSTR_SOLAR_DATA_FILE);
  file = ffsp->open (str_filename, "w");
  if (file > 0)
  {
    file.write ((uint8_t*)&version, sizeof (version));
    file.write ((uint8_t*)&time_now, sizeof (time_now));
    file.write ((uint8_t*)&teleinfo_solar, sizeof (teleinfo_solar));
    file.write ((uint8_t*)&teleinfo_forecast, sizeof (teleinfo_forecast));
    file.close ();
  }

  // plan update in 1 mn
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
//  DynamicJsonDocument json_result(3072);
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
    sprintf_P (str_text, PSTR_SOLAR_URL, str_key, latitude_int, latitude_dec, longitude_int, longitude_dec, teleinfo_forecast.declination, teleinfo_forecast.azimuth, teleinfo_config.prod_max / 1000, teleinfo_config.prod_max % 1000); 
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
  teleinfo_forecast.str_apikey[0] = 0;
  teleinfo_forecast.str_place[0]  = 0;
  memset (&teleinfo_forecast.today,    0, sizeof (solar_day));
  memset (&teleinfo_forecast.tomorrow, 0, sizeof (solar_day));

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

  // set solar production status
  if (result) teleinfo_solar.enabled = true;

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

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      Webserver->on (F (TIC_SOLAR_PAGE_API), TeleinfoSolarWebAPI);
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
