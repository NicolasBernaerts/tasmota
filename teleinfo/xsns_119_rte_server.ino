/*
  xsns_119_rte_server.ino - Handle access to RTE server for Tempo and Ecowatt
  
  Copyright (C) 2022  Nicolas Bernaerts
    06/10/2022 - v1.0 - Creation 
    21/10/2022 - v1.1 - Add sandbox management (with day shift to have green/orange/red on current day)
    09/02/2023 - v1.2 - Disable wifi sleep to avoid latency
    15/05/2023 - v1.3 - Rewrite CSV file access
    14/10/2023 - v1.4 - Rewrite API management and intergrate RTE root certificate
    28/10/2023 - v1.5 - Change in ecowatt stream reception to avoid overload
    19/11/2023 - v2.0 - Switch to BearSSL::WiFiClientSecure_light
    04/12/2023 - v3.0 - Rename to xsns_119_rte_server.ino
                        Rename ecowatt.cfg to rte.cfg
                        Add Tempo calendar
    07/12/2023 - v3.1 - Handle Ecowatt v4 and v5
    19/12/2023 - v3.2 - Add Pointe calendar
    05/02/2024 - v3.3 - Switch to Ecowatt v5 only
    25/02/2024 - v3.4 - Publish topics sequentially to avoid reception errors

  This module connects to french RTE server to retrieve Ecowatt, Tempo and Pointe electricity production forecast.

  It publishes ECOWATT prediction under following MQTT topic :
  
    .../tele/SENSOR
    {"Time":"2022-10-10T23:51:09","ECOWATT":{"dval":2,"hour":14,"now":1,"next":2,
      "day0":{"jour":"06-10-2022","dval":1,"0":1,"1":1,"2":1,"3":1,"4":1,"5":1,"6":1,...,"23":1},
      "day1":{"jour":"07-10-2022","dval":2,"0":1,"1":1,"2":2,"3":1,"4":1,"5":1,"6":1,...,"23":1},
      "day2":{"jour":"08-10-2022","dval":3,"0":1,"1":1,"2":1,"3":1,"4":1,"5":3,"6":1,...,"23":1},
      "day3":{"jour":"09-10-2022","dval":2,"0":1,"1":1,"2":1,"3":2,"4":1,"5":1,"6":1,...,"23":1}}}

  It publishes TEMPO production days under following MQTT topic :

    .../tele/SENSOR
    {"Time":"2022-10-10T23:51:09","TEMPO":{"lv":1,"hp":0,"label":"bleu","icon":"🟦","yesterday":1,"today":1,"tomorrow":2}}


  It publishes POINTE production days under following MQTT topic :

    .../tele/SENSOR
    {"Time":"2022-10-10T23:51:09","POINTE":{"lv":1,"label":"bleu","icon":"🟦","today":1,"tomorrow":2}}


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
 *       France RTE electricity signal management
\**********************************************************/

#ifdef ESP32
#ifdef USE_UFILESYS
#ifdef USE_RTE_SERVER

#define XSNS_119                    119

#include <ArduinoJson.h>
#include <WiFiClientSecureLightBearSSL.h>

// global constant
#define RTE_HTTP_TIMEOUT            5000       // HTTP connexion timeout (ms)
#define RTE_TOKEN_RETRY             300        // token retry timeout (sec.)

// ecowatt constant
#define RTE_ECOWATT_UPDATE_JSON     1800       // publish JSON every 30 mn
#define RTE_ECOWATT_RETRY           900        // ecowatt retry timeout (5mn)
#define RTE_ECOWATT_RENEW           21600      // ecowatt renew period (6h)

// tempo constant
#define RTE_TEMPO_UPDATE_JSON       900        // publish JSON every 15 mn
#define RTE_TEMPO_RETRY             300        // tempo retry timeout (5mn)
#define RTE_TEMPO_RENEW_KNOWN       21600      // tempo renew period when tomorrow is known (6h)
#define RTE_TEMPO_RENEW_UNKNOWN     1800       // tempo renew period when tomorrow is unknown (30mn)

// pointe constant
#define RTE_POINTE_UPDATE_JSON      900        // publish JSON every 15 mn
#define RTE_POINTE_RETRY            300        // pointe retry timeout (5mn)
#define RTE_POINTE_RENEW            21600      // pointe renew period when tomorrow is known (6h)

// configuration file
#define D_RTE_CFG                   "/rte.cfg"

// commands
#define D_CMND_RTE_HELP             "help"
#define D_CMND_RTE_KEY              "key"
#define D_CMND_RTE_TOKEN            "token"
#define D_CMND_RTE_ECOWATT          "ecowatt"
#define D_CMND_RTE_TEMPO            "tempo"
#define D_CMND_RTE_POINTE           "pointe"
#define D_CMND_RTE_ENABLE           "enable"
#define D_CMND_RTE_DISPLAY          "display"
#define D_CMND_RTE_SANDBOX          "sandbox"
#define D_CMND_RTE_UPDATE           "update"
#define D_CMND_RTE_PUBLISH          "publish"

// URL
#define RTE_URL_OAUTH2              "https://digital.iservices.rte-france.com/token/oauth/"
#define RTE_URL_ECOWATT_DATA        "https://digital.iservices.rte-france.com/open_api/ecowatt/v5/signals"
#define RTE_URL_ECOWATT_SANDBOX     "https://digital.iservices.rte-france.com/open_api/ecowatt/v5/sandbox/signals"
#define RTE_URL_TEMPO_DATA          "https://digital.iservices.rte-france.com/open_api/tempo_like_supply_contract/v1/tempo_like_calendars"
#define RTE_URL_POINTE_DATA         "https://digital.iservices.rte-france.com/open_api/demand_response_signal/v2/signals"

// Commands
const char kRteCommands[] PROGMEM = "rte_" "|" D_CMND_RTE_HELP "|" D_CMND_RTE_KEY "|" D_CMND_RTE_TOKEN "|" D_CMND_RTE_SANDBOX;
void (* const RteCommand[])(void) PROGMEM = { &CmndRteHelp, &CmndRteKey, &CmndRteToken, &CmndRteSandbox };

const char kEcowattCommands[] PROGMEM = "eco_" "|" D_CMND_RTE_ENABLE "|" D_CMND_RTE_DISPLAY "|" D_CMND_RTE_UPDATE "|" D_CMND_RTE_PUBLISH;
void (* const EcowattCommand[])(void) PROGMEM = { &CmndEcowattEnable, &CmndEcowattDisplay, &CmndEcowattUpdate, &CmndEcowattPublish };

const char kTempoCommands[] PROGMEM = "tempo_" "|" D_CMND_RTE_ENABLE "|" D_CMND_RTE_DISPLAY "|" D_CMND_RTE_UPDATE "|" D_CMND_RTE_PUBLISH;
void (* const TempoCommand[])(void) PROGMEM = { &CmndTempoEnable, &CmndTempoDisplay, &CmndTempoUpdate, &CmndTempoPublish };

const char kPointeCommands[] PROGMEM = "pointe_" "|" D_CMND_RTE_ENABLE "|" D_CMND_RTE_DISPLAY "|" D_CMND_RTE_UPDATE "|" D_CMND_RTE_PUBLISH;
void (* const PointeCommand[])(void) PROGMEM = { &CmndPointeEnable, &CmndPointeDisplay, &CmndPointeUpdate, &CmndPointePublish };

// https stream status
enum RteHttpsUpdate { RTE_UPDATE_NONE, RTE_UPDATE_TOKEN_BEGIN, RTE_UPDATE_TOKEN_GET, RTE_UPDATE_TOKEN_END, RTE_UPDATE_TEMPO_BEGIN, RTE_UPDATE_TEMPO_GET, RTE_UPDATE_TEMPO_END, RTE_UPDATE_ECOWATT_BEGIN, RTE_UPDATE_ECOWATT_GET, RTE_UPDATE_ECOWATT_END, RTE_UPDATE_POINTE_BEGIN, RTE_UPDATE_POINTE_GET, RTE_UPDATE_POINTE_END, RTE_UPDATE_MAX};

// config
enum RteConfigKey { RTE_CONFIG_KEY, RTE_CONFIG_SANDBOX, RTE_CONFIG_ECOWATT, RTE_CONFIG_ECOWATT_DISPLAY, RTE_CONFIG_TEMPO, RTE_CONFIG_TEMPO_DISPLAY, RTE_CONFIG_POINTE, RTE_CONFIG_POINTE_DISPLAY, RTE_CONFIG_MAX };                   // configuration parameters
const char kEcowattConfigKey[] PROGMEM = D_CMND_RTE_KEY "|" D_CMND_RTE_SANDBOX "|" D_CMND_RTE_ECOWATT "|" D_CMND_RTE_ECOWATT "_" D_CMND_RTE_DISPLAY "|" D_CMND_RTE_TEMPO "|" D_CMND_RTE_TEMPO "_" D_CMND_RTE_DISPLAY "|" D_CMND_RTE_POINTE "|" D_CMND_RTE_POINTE "_" D_CMND_RTE_DISPLAY;        // configuration keys

enum RteDay { RTE_DAY_YESTERDAY, RTE_DAY_TODAY, RTE_DAY_TOMORROW, RTE_DAY_MAX };
enum RtePeriod { RTE_PERIOD_PLEINE, RTE_PERIOD_CREUSE, RTE_PERIOD_MAX };
enum RteGlobalLevel { RTE_LEVEL_UNKNOWN, RTE_LEVEL_BLUE, RTE_LEVEL_WHITE, RTE_LEVEL_RED, RTE_LEVEL_MAX };

// tempo data
enum TempoLevel  { RTE_TEMPO_LEVEL_UNKNOWN, RTE_TEMPO_LEVEL_BLUE, RTE_TEMPO_LEVEL_WHITE, RTE_TEMPO_LEVEL_RED, RTE_TEMPO_LEVEL_MAX };
const char kRteTempoStream[] PROGMEM = "|BLUE|WHITE|RED";
const char kRteTempoJSON[]   PROGMEM = "undef|blue|white|red";
const char kRteTempoColor[]  PROGMEM = "#252525|#06b|#ccc|#b00";
const char kRteTempoDot[]    PROGMEM = "⚪|⚪|⚫|⚪";
const char kRteTempoIcon[]   PROGMEM = "❔|🟦|⬜|🟥|❔|🔵|⚪|🔴";

// pointe data
enum PointeLevel  { RTE_POINTE_LEVEL_UNKNOWN, RTE_POINTE_LEVEL_BLUE, RTE_POINTE_LEVEL_RED, RTE_POINTE_LEVEL_MAX };
const char kRtePointeJSON[]  PROGMEM = "undef|blue|red";
const char kRtePointeColor[] PROGMEM = "#252525|#06b|#900";
const char kRtePointeIcon[]  PROGMEM = "❔|🟦|🟥";

// ecowatt data
enum EcowattDays { RTE_ECOWATT_DAY_TODAY, RTE_ECOWATT_DAY_TOMORROW, RTE_ECOWATT_DAY_PLUS2, RTE_ECOWATT_DAY_DAYPLUS3, RTE_ECOWATT_DAY_MAX };
enum EcowattLevel { RTE_ECOWATT_LEVEL_CARBONFREE, RTE_ECOWATT_LEVEL_NORMAL, RTE_ECOWATT_LEVEL_WARNING, RTE_ECOWATT_LEVEL_POWERCUT, RTE_ECOWATT_LEVEL_MAX };
const char kRteEcowattLevelColor[] PROGMEM = "#0a0|#080|#F80|#A00";

/***********************************************************\
 *                        Data
\***********************************************************/

// RTE configuration
static struct {
  uint8_t tempo_enable    = 0;                      // flag to enable tempo period server
  uint8_t tempo_display   = 0;                      // flag to display tempo period
  uint8_t pointe_enable   = 0;                      // flag to enable pointe period server
  uint8_t pointe_display  = 0;                      // flag to display pointe period
  uint8_t ecowatt_enable  = 0;                      // flag to enable ecowatt server
  uint8_t ecowatt_display = 0;                      // flag to display ecowatt
  uint8_t sandbox         = 0;                      // flag to use RTE sandbox
  char    str_private_key[128];                     // base 64 private key (provided on RTE site)
} rte_config;

// RTE server updates
static struct {
  uint8_t     step         = RTE_UPDATE_NONE;       // current reception step
  uint32_t    time_token   = UINT32_MAX;            // timestamp of token update
  uint32_t    time_ecowatt = UINT32_MAX;            // timestamp of signal update
  uint32_t    time_tempo   = UINT32_MAX;            // timestamp of tempo update
  uint32_t    time_pointe  = UINT32_MAX;            // timestamp of pointe period update
  char        str_token[128];                       // token value
} rte_update;

// Ecowatt status
struct ecowatt_day {
  uint8_t dvalue;                                   // day global status
  char    str_day_of_week[4];                       // day of week (mon, Tue, ...)
  uint8_t day_of_month;                             // dd
  uint8_t month;                                    // mm
  uint8_t arr_hvalue[24];                           // hourly slots
};
static struct {
  uint8_t  json_publish = 0;                        // flag to publish JSON
  uint32_t json_time    = UINT32_MAX;               // timestamp of next JSON update
  ecowatt_day arr_day[RTE_ECOWATT_DAY_MAX];         // slots for today and next days
} ecowatt_status;

// Tempo status
static struct {
  uint8_t  json_publish = 0;                        // flag to publish JSON
  uint32_t json_time    = UINT32_MAX;               // timestamp of next JSON update
  uint8_t  last_period  = RTE_PERIOD_MAX;           // last period
  uint8_t  last_level   = RTE_TEMPO_LEVEL_MAX;      // last color
  int arr_day[RTE_DAY_MAX];                         // days status
} tempo_status;

// Pointe status
static struct {
  uint8_t  json_publish = 0;                        // flag to publish JSON
  uint32_t json_time    = UINT32_MAX;               // timestamp of next JSON update
  uint8_t  last_level   = RTE_POINTE_LEVEL_MAX;     // last color
  int arr_day[RTE_DAY_MAX];                         // days status
} pointe_status;

/***********************************************************\
 *                       SSL client
\***********************************************************/

/*
// RTE API root certificate
const char ecowatt_root_certificate[] PROGMEM = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDXzCCAkegAwIBAgILBAAAAAABIVhTCKIwDQYJKoZIhvcNAQELBQAwTDEgMB4G\n" \
"A1UECxMXR2xvYmFsU2lnbiBSb290IENBIC0gUjMxEzARBgNVBAoTCkdsb2JhbFNp\n" \
"Z24xEzARBgNVBAMTCkdsb2JhbFNpZ24wHhcNMDkwMzE4MTAwMDAwWhcNMjkwMzE4\n" \
"MTAwMDAwWjBMMSAwHgYDVQQLExdHbG9iYWxTaWduIFJvb3QgQ0EgLSBSMzETMBEG\n" \
"A1UEChMKR2xvYmFsU2lnbjETMBEGA1UEAxMKR2xvYmFsU2lnbjCCASIwDQYJKoZI\n" \
"hvcNAQEBBQADggEPADCCAQoCggEBAMwldpB5BngiFvXAg7aEyiie/QV2EcWtiHL8\n" \
"RgJDx7KKnQRfJMsuS+FggkbhUqsMgUdwbN1k0ev1LKMPgj0MK66X17YUhhB5uzsT\n" \
"gHeMCOFJ0mpiLx9e+pZo34knlTifBtc+ycsmWQ1z3rDI6SYOgxXG71uL0gRgykmm\n" \
"KPZpO/bLyCiR5Z2KYVc3rHQU3HTgOu5yLy6c+9C7v/U9AOEGM+iCK65TpjoWc4zd\n" \
"QQ4gOsC0p6Hpsk+QLjJg6VfLuQSSaGjlOCZgdbKfd/+RFO+uIEn8rUAVSNECMWEZ\n" \
"XriX7613t2Saer9fwRPvm2L7DWzgVGkWqQPabumDk3F2xmmFghcCAwEAAaNCMEAw\n" \
"DgYDVR0PAQH/BAQDAgEGMA8GA1UdEwEB/wQFMAMBAf8wHQYDVR0OBBYEFI/wS3+o\n" \
"LkUkrk1Q+mOai97i3Ru8MA0GCSqGSIb3DQEBCwUAA4IBAQBLQNvAUKr+yAzv95ZU\n" \
"RUm7lgAJQayzE4aGKAczymvmdLm6AC2upArT9fHxD4q/c2dKg8dEe3jgr25sbwMp\n" \
"jjM5RcOO5LlXbKr8EpbsU8Yt5CRsuZRj+9xTaGdWPoO4zzUhw8lo/s7awlOqzJCK\n" \
"6fBdRoyV3XpYKBovHd7NADdBj+1EbddTKJd+82cEHhXXipa0095MJ6RMG3NzdvQX\n" \
"mcIfeg7jLQitChws/zyrVQ4PkX4268NXSb7hLi18YIvDQVETI53O9zJrlAGomecs\n" \
"Mx86OyXShkDOOyyGeMlhLxS67ttVb9+E7gUJTb0o2HLO02JQZR7rkpeDMdmztcpH\n" \
"WD9f\n" \
"-----END CERTIFICATE-----\n";
*/

BearSSL::WiFiClientSecure_light  rte_wifi (4096, 1024);         // bear ssl client
HTTPClient                       rte_http;                      // https client

/************************************\
 *        RTE global commands
\************************************/

// RTE server help
void CmndRteHelp ()
{
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: commands about RTE server"));
  AddLog (LOG_LEVEL_INFO, PSTR (" RTE global commands :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - rte_key <key>        = set RTE base64 private key"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - rte_token            = display current token"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - rte_sandbox <0/1>    = set sandbox mode <%u>"), rte_config.sandbox);
  AddLog (LOG_LEVEL_INFO, PSTR (" Tempo commands :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - tempo_enable <0/1>   = enable tempo server <%u>"), rte_config.tempo_enable);
  AddLog (LOG_LEVEL_INFO, PSTR (" - tempo_display <0/1>  = display tempo calendar <%u>"), rte_config.tempo_display);
  AddLog (LOG_LEVEL_INFO, PSTR (" - tempo_update         = update tempo from RTE server"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - tempo_publish        = publish tempo data"));
  AddLog (LOG_LEVEL_INFO, PSTR (" Pointe commands :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - pointe_enable <0/1>  = enable pointe period server <%u>"), rte_config.pointe_enable);
  AddLog (LOG_LEVEL_INFO, PSTR (" - pointe_display <0/1> = display pointe calendar <%u>"), rte_config.pointe_display);
  AddLog (LOG_LEVEL_INFO, PSTR (" - pointe_update        = update period from RTE server"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - pointe_publish       = publish pointe period data"));
  AddLog (LOG_LEVEL_INFO, PSTR (" Ecowatt commands :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - eco_enable <0/1>     = enable ecowatt server <%u>"), rte_config.ecowatt_enable);
  AddLog (LOG_LEVEL_INFO, PSTR (" - eco_display <0/1>    = display ecowatt calendar <%u>"), rte_config.ecowatt_display);
  AddLog (LOG_LEVEL_INFO, PSTR (" - eco_update           = update ecowatt from RTE server"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - eco_publish          = publish ecowatt data"));
  ResponseCmndDone ();
}

// RTE base64 private key
void CmndRteKey ()
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.data_len < 128))
  {
    strlcpy (rte_config.str_private_key, XdrvMailbox.data, sizeof (rte_config.str_private_key));
    RteSaveConfig ();
  }
  
  ResponseCmndChar (rte_config.str_private_key);
}

// RTE server token
void CmndRteToken ()
{
  ResponseCmndChar (rte_update.str_token);
}

// Set RTE sandbox mode
void CmndRteSandbox ()
{
  if (XdrvMailbox.data_len > 0)
  {
    // set sandbox mode
    rte_config.sandbox = (uint8_t)XdrvMailbox.payload;
    RteSaveConfig ();

    // force update
    rte_update.time_ecowatt = rte_update.time_tempo = LocalTime ();
  }
  
  ResponseCmndNumber (rte_config.sandbox);
}

/**********************************\
 *        Ecowatt commands
\**********************************/

// Enable ecowatt server
void CmndEcowattEnable ()
{
  if (XdrvMailbox.data_len > 0)
  {
    // update status
    rte_config.ecowatt_enable = (XdrvMailbox.payload != 0);
    if (rte_config.ecowatt_enable) rte_config.ecowatt_display = 1;
    rte_update.time_ecowatt = LocalTime ();
    RteSaveConfig ();
  }

  ResponseCmndNumber (rte_config.ecowatt_enable);
}

// Display ecowatt server
void CmndEcowattDisplay ()
{
  if (XdrvMailbox.data_len > 0)
  {
    rte_config.ecowatt_display = (XdrvMailbox.payload != 0);
    RteSaveConfig ();
  }

  ResponseCmndNumber (rte_config.ecowatt_display);
}

// Ecowatt forced update from RTE server
void CmndEcowattUpdate ()
{
  rte_update.time_ecowatt = LocalTime ();
  ResponseCmndDone ();
}

// Ecowatt publish data
void CmndEcowattPublish ()
{
  ecowatt_status.json_time = LocalTime ();
  ResponseCmndDone ();
}

/**********************************\
 *         Tempo commands
\**********************************/

// Enable ecowatt server
void CmndTempoEnable ()
{
  if (XdrvMailbox.data_len > 0)
  {
    // update status
    rte_config.tempo_enable = (XdrvMailbox.payload != 0);
    if (rte_config.tempo_enable) rte_config.tempo_display = 1;
    rte_update.time_tempo = LocalTime ();
    RteSaveConfig ();
  }

  ResponseCmndNumber (rte_config.tempo_enable);
}

// Display tempo server
void CmndTempoDisplay ()
{
  if (XdrvMailbox.data_len > 0)
  {
    rte_config.tempo_display = (XdrvMailbox.payload != 0);
    RteSaveConfig ();
  }

  ResponseCmndNumber (rte_config.tempo_display);
}

// Tempo forced update from RTE server
void CmndTempoUpdate ()
{
  rte_update.time_tempo = LocalTime ();
  ResponseCmndDone ();
}

// Tempo publish data
void CmndTempoPublish ()
{
  tempo_status.json_time = LocalTime ();
  ResponseCmndDone ();
}

/**********************************\
 *      Pointe Period commands
\**********************************/

// Enable pointe period server
void CmndPointeEnable ()
{
  bool enable;

  if (XdrvMailbox.data_len > 0)
  {
    // update status
    rte_config.pointe_enable = (XdrvMailbox.payload != 0);
    if (rte_config.pointe_enable) rte_config.pointe_display = 1;
    rte_update.time_pointe = LocalTime ();
    RteSaveConfig ();
  }

  ResponseCmndNumber (rte_config.pointe_enable);
}

// Display pointe server
void CmndPointeDisplay ()
{
  if (XdrvMailbox.data_len > 0)
  {
    rte_config.pointe_display = (XdrvMailbox.payload != 0);
    RteSaveConfig ();
  }

  ResponseCmndNumber (rte_config.pointe_display);
}

// Pointe period forced update from RTE server
void CmndPointeUpdate ()
{
  rte_update.time_pointe = LocalTime ();
  ResponseCmndDone ();
}

// Pointe period publish data
void CmndPointePublish ()
{
  pointe_status.json_time = LocalTime ();
  ResponseCmndDone ();
}

/***********************************************************\
 *                      Configuration
\***********************************************************/

// process realtime data
void RteProcessRealTime () 
{
#ifdef USE_TELEINFO
    // update teleinfo data reception
    TeleinfoProcessRealTime ();
#endif    // USE_TELEINFO
}

// load configuration
void RteLoadConfig () 
{
  int    index;
  char   str_text[16];
  char   str_line[128];
  char   *pstr_key, *pstr_value;
  File   file;

  // if file exists, open and read each line
  if (ffsp->exists (D_RTE_CFG))
  {
    file = ffsp->open (D_RTE_CFG, "r");
    while (file.available ())
    {
      // read current line and extract key and value
      index = file.readBytesUntil ('\n', str_line, sizeof (str_line) - 1);
      if (index >= 0) str_line[index] = 0;
      pstr_key   = strtok (str_line, "=");
      pstr_value = strtok (nullptr,  "\n");

      // if key and value are defined, look for config keys
      if ((pstr_key != nullptr) && (pstr_value != nullptr))
      {
        index = GetCommandCode (str_text, sizeof (str_text), pstr_key, kEcowattConfigKey);
        switch (index)
        {
          case RTE_CONFIG_KEY:
            strlcpy (rte_config.str_private_key, pstr_value, sizeof (rte_config.str_private_key));
            break;
         case RTE_CONFIG_SANDBOX:
            rte_config.sandbox = (uint8_t)atoi (pstr_value);
            break;
         case RTE_CONFIG_ECOWATT:
            rte_config.ecowatt_enable = (uint8_t)atoi (pstr_value);
            break;
         case RTE_CONFIG_ECOWATT_DISPLAY:
            rte_config.ecowatt_display = (uint8_t)atoi (pstr_value);
            break;
         case RTE_CONFIG_TEMPO:
            rte_config.tempo_enable = (uint8_t)atoi (pstr_value);
            break;
         case RTE_CONFIG_TEMPO_DISPLAY:
            rte_config.tempo_display = (uint8_t)atoi (pstr_value);
            break;
         case RTE_CONFIG_POINTE:
            rte_config.pointe_enable = (uint8_t)atoi (pstr_value);
            break;
         case RTE_CONFIG_POINTE_DISPLAY:
            rte_config.pointe_display = (uint8_t)atoi (pstr_value);
            break;
        }
      }
    }

    // close file
    file.close ();

    // log
    AddLog (LOG_LEVEL_DEBUG, PSTR ("RTE: Config loaded from %s"), D_RTE_CFG);
  }
}

// save configuration
void RteSaveConfig () 
{
  uint8_t index;
  char    str_number[4];
  char    str_line[128];
  File    file;

  // open file
  file = ffsp->open (D_RTE_CFG, "w");

  // loop to write config
  for (index = 0; index < RTE_CONFIG_MAX; index ++)
  {
    if (GetTextIndexed (str_line, sizeof (str_line), index, kEcowattConfigKey) != nullptr)
    {
      // generate key=value
      strcat (str_line, "=");
      switch (index)
        {
          case RTE_CONFIG_KEY:
            strcat (str_line, rte_config.str_private_key);
            break;
         case RTE_CONFIG_SANDBOX:
            if (rte_config.sandbox) strcat (str_line, "1");
              else strcat (str_line, "0");
            break;
         case RTE_CONFIG_ECOWATT:
            if (rte_config.ecowatt_enable) strcat (str_line, "1");
              else strcat (str_line, "0");
            break;
         case RTE_CONFIG_ECOWATT_DISPLAY:
            if (rte_config.ecowatt_display) strcat (str_line, "1");
              else strcat (str_line, "0");
            break;
         case RTE_CONFIG_TEMPO:
            if (rte_config.tempo_enable) strcat (str_line, "1");
              else strcat (str_line, "0");
            break;
         case RTE_CONFIG_TEMPO_DISPLAY:
            if (rte_config.tempo_display) strcat (str_line, "1");
              else strcat (str_line, "0");
            break;
         case RTE_CONFIG_POINTE:
            if (rte_config.pointe_enable) strcat (str_line, "1");
              else strcat (str_line, "0");
            break;
         case RTE_CONFIG_POINTE_DISPLAY:
            if (rte_config.pointe_display) strcat (str_line, "1");
              else strcat (str_line, "0");
            break;
        }

      // write to file
      strcat (str_line, "\n");
      file.print (str_line);
    }
  }

  // close file
  file.close ();
}

/***************************************\
 *           JSON publication
\***************************************/

// publish RTE Tempo data thru MQTT
void RtePublishTempoJson ()
{
  uint8_t level, period;
  char    str_label[8];
  char    str_icon[8];

  // init message
  ResponseClear ();
  ResponseAppendTime ();

  // "TEMPO":{...}}
  ResponseAppend_P (PSTR (",\"TEMPO\":{"));

  // publish current status
  level  = RteTempoGetLevel (RTE_DAY_TODAY, RtcTime.hour);
  period = RteTempoGetPeriod (RtcTime.hour);
  RteTempoGetIcon (RTE_DAY_TODAY, RtcTime.hour, str_icon, sizeof (str_icon));
  GetTextIndexed (str_label, sizeof (str_label), level, kRteTempoJSON);
  ResponseAppend_P (PSTR ("\"lv\":%u,\"hp\":%u,\"label\":\"%s\",\"icon\":\"%s\""), level, period, str_label, str_icon);

  // publish days
  ResponseAppend_P (PSTR (",\"yesterday\":%u,\"today\":%u,\"tomorrow\":%u"), tempo_status.arr_day[RTE_DAY_YESTERDAY], tempo_status.arr_day[RTE_DAY_TODAY], tempo_status.arr_day[RTE_DAY_TOMORROW]);

  // publish message
  ResponseJsonEndEnd ();
  MqttPublishTeleSensor ();

  // plan next update
  tempo_status.json_publish = 0;
  tempo_status.json_time    = LocalTime () + RTE_TEMPO_UPDATE_JSON;
}

// publish RTE pointe data thru MQTT
void RtePublishPointeJson ()
{
  uint8_t level;
  char    str_label[8];
  char    str_icon[8];

  // init message
  ResponseClear ();
  ResponseAppendTime ();
  
  // ,"POINTE":{...}
  ResponseAppend_P (PSTR (",\"POINTE\":{"));

  // publish current status
  level  = RtePointeGetLevel (RTE_DAY_TODAY, RtcTime.hour);
  GetTextIndexed (str_label, sizeof (str_label), level, kRtePointeJSON);
  GetTextIndexed (str_icon, sizeof (str_icon), level, kRtePointeIcon);
  ResponseAppend_P (PSTR ("\"lv\":%u,\"label\":\"%s\",\"icon\":\"%s\""), level, str_label, str_icon);

  // publish days
  ResponseAppend_P (PSTR (",\"today\":%u,\"tomorrow\":%u"), pointe_status.arr_day[RTE_DAY_TODAY], pointe_status.arr_day[RTE_DAY_TOMORROW]);

  // publish message
  ResponseJsonEndEnd ();
  MqttPublishTeleSensor ();

  // plan next update
  pointe_status.json_publish = 0;
  pointe_status.json_time    = LocalTime () + RTE_POINTE_UPDATE_JSON;
}

// publish RTE data thru MQTT
void RtePublishEcowattJson ()
{
  uint8_t  index, slot;
  uint8_t  day_now, day_next, hour_now, hour_next;

  // current time
  day_now   = RTE_ECOWATT_DAY_TODAY;
  hour_now  = RtcTime.hour;
  day_next  = day_now;
  hour_next = hour_now + 1;
  if (hour_next == 24) { hour_next = 0; day_next++; }

  // init message
  ResponseClear ();
  ResponseAppendTime ();

  // ,"ECOWATT":{...}
  ResponseAppend_P (PSTR (",\"ECOWATT\":{"));

  // publish current status
  ResponseAppend_P (PSTR ("\"dval\":%u,\"now\":%u,\"next\":%u"), ecowatt_status.arr_day[day_now].dvalue,  ecowatt_status.arr_day[day_now].arr_hvalue[hour_now], ecowatt_status.arr_day[day_next].arr_hvalue[hour_next]);

  // loop thru number of slots to publish
  for (index = 0; index < RTE_ECOWATT_DAY_MAX; index ++)
  {
    ResponseAppend_P (PSTR (",\"day%u\":{\"jour\":\"%s\",\"date\":\"%u:%u\",\"dval\":%u"), index, ecowatt_status.arr_day[index].str_day_of_week, ecowatt_status.arr_day[index].day_of_month, ecowatt_status.arr_day[index].month, ecowatt_status.arr_day[index].dvalue);
    for (slot = 0; slot < 24; slot ++) ResponseAppend_P (PSTR (",\"%u\":%u"), slot, ecowatt_status.arr_day[index].arr_hvalue[slot]);
    ResponseAppend_P (PSTR ("}"));
  }
  
  // publish message
  ResponseJsonEndEnd ();
  MqttPublishTeleSensor ();

  // plan next update
  ecowatt_status.json_publish = 0;
  ecowatt_status.json_time    = LocalTime () + RTE_ECOWATT_UPDATE_JSON;
}

/***********************************\
 *        Token management
\***********************************/

// plan token retry
void RteStreamTokenRetry ()
{
  rte_update.step         = RTE_UPDATE_NONE;
  rte_update.time_token   = LocalTime () + RTE_TOKEN_RETRY;
  rte_update.time_tempo   = max (rte_update.time_tempo, rte_update.time_token + 15);
  rte_update.time_pointe  = max (rte_update.time_pointe, rte_update.time_token + 30);
  rte_update.time_ecowatt = max (rte_update.time_ecowatt, rte_update.time_token + 30);
}

// token connexion
bool RteStreamTokenBegin ()
{
  bool is_ok;
  char str_auth[128];

  // check private key
  is_ok = (strlen (rte_config.str_private_key) > 0);
  if (!is_ok) AddLog (LOG_LEVEL_DEBUG, PSTR ("RTE: Token - Private key missing"));

  // prepare connexion
  if (is_ok)
  {
    // declare connexion
    is_ok = rte_http.begin (rte_wifi, RTE_URL_OAUTH2);

   // connexion report
    if (is_ok) AddLog (LOG_LEVEL_DEBUG, PSTR ("RTE: Token - Connexion done [%s]"), RTE_URL_OAUTH2);
    else
    {
      AddLog (LOG_LEVEL_DEBUG, PSTR ("RTE: Token - Connexion refused [%s]"), RTE_URL_OAUTH2);
      rte_http.end ();
      rte_wifi.stop ();
    }
  }

  // set headers
  if (is_ok)
  {
    // set authorisation
    strcpy (str_auth, "Basic ");
    strlcat (str_auth, rte_config.str_private_key, sizeof (str_auth) - 1);

    // set headers
    rte_http.addHeader ("Authorization", str_auth, false, true);
    rte_http.addHeader ("Content-Type", "application/x-www-form-urlencoded", false, true);

    // set http timeout
    rte_http.setTimeout (RTE_HTTP_TIMEOUT);
  }

  return is_ok;
}

// start token reception
bool RteStreamTokenGet ()
{
  bool is_ok;
  int  http_code;

  // connexion
  http_code = rte_http.POST (nullptr, 0);

  // handle realtime data reception
  RteProcessRealTime ();

  // check if connexion failed
  is_ok = ((http_code == HTTP_CODE_OK) || (http_code == HTTP_CODE_MOVED_PERMANENTLY));
  if (is_ok) AddLog (LOG_LEVEL_DEBUG, PSTR ("RTE: Token - Success [%d]"), http_code);
  else
  {
    AddLog (LOG_LEVEL_DEBUG, PSTR ("RTE: Token - Error %s [%d]"), rte_http.errorToString (http_code).c_str(), http_code);
    rte_http.end ();
    rte_wifi.stop ();
  }

  return is_ok;
}

// end of token reception
bool RteStreamTokenEnd ()
{
  uint32_t time_now, token_validity;
  DynamicJsonDocument json_result(192);

  // current time
  time_now = LocalTime ();

  // extract token from JSON
  deserializeJson (json_result, rte_http.getString ().c_str ());

  // get token value
  strlcpy (rte_update.str_token, json_result["access_token"].as<const char*> (), sizeof (rte_update.str_token));

  // get token validity
  token_validity = json_result["expires_in"].as<unsigned int> ();

  // set next token and next signal update
  rte_update.time_token  = time_now + token_validity - 10;
  rte_update.time_tempo   = max (rte_update.time_tempo, time_now + 15);
  rte_update.time_pointe  = max (rte_update.time_pointe, time_now + 30);
  rte_update.time_ecowatt = max (rte_update.time_ecowatt, time_now + 30);

  // log
  AddLog (LOG_LEVEL_DEBUG, PSTR ("RTE: Token valid for %u seconds [%s]"), token_validity, rte_update.str_token);

  // end connexion
  rte_http.end ();
  rte_wifi.stop ();

  return (strlen (rte_update.str_token) > 0);
}

/***********************************\
 *        Ecowatt management
\***********************************/

bool RteEcowattIsEnabled ()
{
  return (rte_config.ecowatt_enable == 1);
}

uint8_t RteEcowattGetGlobalLevel (const uint8_t day, const uint8_t slot)
{
  uint8_t level, result;

  // convert day and get level
  if (day == RTE_DAY_TODAY) level = ecowatt_status.arr_day[RTE_ECOWATT_DAY_TODAY].arr_hvalue[slot];
  else if (day == RTE_DAY_TOMORROW) level = ecowatt_status.arr_day[RTE_ECOWATT_DAY_TOMORROW].arr_hvalue[slot];
  else level = RTE_ECOWATT_LEVEL_MAX;

  // convert level
  switch (level)
  {
    case RTE_ECOWATT_LEVEL_CARBONFREE: result = RTE_LEVEL_BLUE; break;
    case RTE_ECOWATT_LEVEL_NORMAL:     result = RTE_LEVEL_BLUE; break;
    case RTE_ECOWATT_LEVEL_WARNING:    result = RTE_LEVEL_WHITE; break;
    case RTE_ECOWATT_LEVEL_POWERCUT:   result = RTE_LEVEL_RED; break;
    default:                           result = RTE_LEVEL_UNKNOWN; break;
  }

  return result;
}

// plan ecowatt retry
void RteEcowattStreamRetry ()
{
  rte_update.step        = RTE_UPDATE_NONE;
  rte_update.time_ecowatt = LocalTime () + RTE_ECOWATT_RETRY;
}

// start of signal reception
bool RteEcowattStreamBegin ()
{
  bool is_ok;
  char str_text[256];

  // check token
  is_ok = (strlen (rte_update.str_token) > 0);
  if (!is_ok) AddLog (LOG_LEVEL_ERROR, PSTR ("RTE: Ecowatt - Token missing"));

  // prepare connexion
  if (is_ok) 
  {
    // connexion
    if (rte_config.sandbox) strcpy (str_text, RTE_URL_ECOWATT_SANDBOX);
      else strcpy (str_text, RTE_URL_ECOWATT_DATA);
    is_ok = rte_http.begin (rte_wifi, str_text);

   // connexion report
    if (is_ok) AddLog (LOG_LEVEL_DEBUG, PSTR ("RTE: Ecowatt - Connexion done [%s]"), str_text);
    else
    {
      AddLog (LOG_LEVEL_DEBUG, PSTR ("RTE: Ecowatt - Connexion refused [%s]"), str_text);
      rte_http.end ();
      rte_wifi.stop ();
    }
  }

  // set headers
  if (is_ok)
  {
    // set authorisation
    strcpy (str_text, "Bearer ");
    strlcat (str_text, rte_update.str_token, sizeof (str_text) - 1);

    // set headers
    rte_http.addHeader ("Authorization", str_text, false, true);
    rte_http.addHeader ("Content-Type", "application/x-www-form-urlencoded", false, true);

    // set http timeout
    rte_http.setTimeout (RTE_HTTP_TIMEOUT);
  }

  return is_ok;
}

// start of signal reception
bool RteEcowattStreamGet ()
{
  bool is_ok;
  int  http_code;

  // connexion
  http_code = rte_http.GET ();

  // handle realtime data reception
  RteProcessRealTime ();

  // check if connexion failed
  is_ok = ((http_code == HTTP_CODE_OK) || (http_code == HTTP_CODE_MOVED_PERMANENTLY));
  if (is_ok) AddLog (LOG_LEVEL_DEBUG, PSTR ("RTE: Ecowatt - Success [%d]"), http_code);
  else
  {
    AddLog (LOG_LEVEL_DEBUG, PSTR ("RTE: Ecowatt - Error %s [%d]"), rte_http.errorToString (http_code).c_str(), http_code);
    rte_http.end ();
    rte_wifi.stop ();
  }

  return is_ok;
}

// handle end of signals page reception
bool RteEcowattStreamEnd ()
{
  uint8_t  slot, value;
  uint16_t index, index_array, size;
  char     str_day[12];
  char     str_list[24];
  uint32_t day_time;
  TIME_T   day_dst;
  JsonArray           arr_day, arr_slot;
  DynamicJsonDocument json_result(8192);

  // get week days list
  strcpy (str_list, D_DAY3LIST);

  // extract token from JSON
  deserializeJson (json_result, rte_http.getString ().c_str ());

  // check array Size
  arr_day = json_result["signals"].as<JsonArray>();
  size = arr_day.size ();

  // loop thru all 4 days to get today's and tomorrow's index
  if (size <= RTE_ECOWATT_DAY_MAX) for (index = 0; index < size; index ++)
  {
    // if sandbox, shift array index to avoid current day fully ok
    if (rte_config.sandbox) index_array = (index + RTE_ECOWATT_DAY_MAX - 1) % RTE_ECOWATT_DAY_MAX;
      else index_array = index;

    // get global status
    ecowatt_status.arr_day[index_array].dvalue = arr_day[index]["dvalue"].as<unsigned int>();

    // get date and convert from yyyy-mm-dd to dd.mm.yyyy
    strlcpy (str_day, arr_day[index]["jour"], sizeof (str_day));
    str_day[4] = str_day[7] = str_day[10] = 0;
    day_dst.year = atoi (str_day) - 1970;
    day_dst.month = atoi (str_day + 5);
    day_dst.day_of_month = atoi (str_day + 8);
    day_dst.day_of_week = 0;
    day_time = MakeTime (day_dst);
    BreakTime (day_time, day_dst);
    if (day_dst.day_of_week < 8) strlcpy (str_day, str_list + 3 * (day_dst.day_of_week - 1), 4);
      else strcpy (str_day, "");

    // update current slot
    strlcpy (ecowatt_status.arr_day[index_array].str_day_of_week, str_day, sizeof (ecowatt_status.arr_day[index_array].str_day_of_week));
    ecowatt_status.arr_day[index_array].day_of_month = day_dst.day_of_month;
    ecowatt_status.arr_day[index_array].month        = day_dst.month;

    // loop to populate the slots
    arr_slot = arr_day[index]["values"].as<JsonArray>();
    for (slot = 0; slot < 24; slot ++)
    {
      value = arr_slot[slot]["hvalue"].as<unsigned int>();
      if (value >= RTE_ECOWATT_LEVEL_MAX) value = RTE_ECOWATT_LEVEL_NORMAL;
      ecowatt_status.arr_day[index_array].arr_hvalue[slot] = value;
    }
  }

  // set signal next update
  rte_update.time_ecowatt = LocalTime () + RTE_ECOWATT_RENEW;

  // log
  AddLog (LOG_LEVEL_DEBUG, PSTR ("RTE: Ecowatt - Update till %02u/%02u"), ecowatt_status.arr_day[index_array].day_of_month, ecowatt_status.arr_day[index_array].month);

  // end connexion
  rte_http.end ();
  rte_wifi.stop ();

  return true;
}

/***********************************\
 *        Tempo management
\***********************************/

bool RteTempoIsEnabled ()
{
  return (rte_config.tempo_enable == 1);
}

uint8_t RteTempoGetPeriod (const uint8_t hour)
{
  uint8_t period = RTE_PERIOD_PLEINE;

  if ((hour < 6) || (hour > 21)) period = RTE_PERIOD_CREUSE;
  
  return period;
}

uint8_t RteTempoGetGlobalLevel (const uint8_t day, const uint8_t hour)
{
  uint8_t level, result;

  // get level
  level = RteTempoGetLevel (day, hour);

  // convert level
  switch (level)
  {
    case RTE_TEMPO_LEVEL_BLUE:  result = RTE_LEVEL_BLUE; break;
    case RTE_TEMPO_LEVEL_WHITE: result = RTE_LEVEL_WHITE; break;
    case RTE_TEMPO_LEVEL_RED:   result = RTE_LEVEL_RED; break;
    default:                    result = RTE_LEVEL_UNKNOWN; break;
  }

  return result;
}

uint8_t RteTempoGetLevel (const uint8_t day, const uint8_t hour)
{
  uint8_t level = RTE_TEMPO_LEVEL_UNKNOWN;
  
  // check parameter
  if (day >= RTE_DAY_MAX) return level;

  // calculate level according to time
  if (hour >= 6) level = tempo_status.arr_day[day];
    else if (day > RTE_DAY_YESTERDAY) level = tempo_status.arr_day[day - 1];

  return level;
}

void RteTempoGetIcon (const uint8_t day, const uint8_t hour, char* pstr_icon, const size_t size_icon)
{
  uint8_t period, level, index;

  // get period and color
  period = RteTempoGetPeriod (hour);
  level  = RteTempoGetLevel (day, hour);

  // calculate index and get icon
  index = period * RTE_TEMPO_LEVEL_MAX + level;
  GetTextIndexed (pstr_icon, size_icon, index, kRteTempoIcon); 
}

// plan ecowatt retry
void RteTempoStreamRetry ()
{
  rte_update.step       = RTE_UPDATE_NONE;
  rte_update.time_tempo = LocalTime () + RTE_TEMPO_RETRY;
}

// start of signal reception
bool RteTempoStreamBegin ()
{
  bool     is_ok;
  int32_t  tz_offset;
  uint32_t current_time;
  TIME_T   start_dst, stop_dst;
//  char     str_offset[8];
  char     str_text[256];

  // check token
  is_ok = (strlen (rte_update.str_token) > 0);
  if (!is_ok) AddLog (LOG_LEVEL_DEBUG, PSTR ("RTE: Tempo - Token missing"));

  // prepare connexion
  if (is_ok) 
  {
    // calculate time window
    current_time = LocalTime ();
    BreakTime (current_time - 86400, start_dst);
    BreakTime (current_time + 172800, stop_dst);

    // calculate timezone offset
    tz_offset = Rtc.time_timezone / 60;

    // generate URL
    sprintf_P (str_text, PSTR ("%s?start_date=%04u-%02u-%02uT00:00:00+%02d:00&end_date=%04u-%02u-%02uT00:00:00+%02d:00"), RTE_URL_TEMPO_DATA, 1970 + start_dst.year, start_dst.month, start_dst.day_of_month, tz_offset, 1970 + stop_dst.year, stop_dst.month, stop_dst.day_of_month, tz_offset);
    AddLog (LOG_LEVEL_DEBUG, PSTR ("RTE: Tempo - URL %s"), str_text);

    // connexion
    is_ok = rte_http.begin (rte_wifi, str_text);

    // connexion report
    if (is_ok) AddLog (LOG_LEVEL_DEBUG, PSTR ("RTE: Tempo - Connexion done [%s]"), str_text);
    else
    {
      AddLog (LOG_LEVEL_DEBUG, PSTR ("RTE: Tempo - Connexion refused [%s]"), str_text);
      rte_http.end ();
      rte_wifi.stop ();
    }
  }

  // set headers
  if (is_ok)
  {
    // set authorisation
    strcpy (str_text, "Bearer ");
    strlcat (str_text, rte_update.str_token, sizeof (str_text) - 1);

    // set headers
    rte_http.addHeader ("Authorization", str_text, false, true);
    rte_http.addHeader ("Content-Type", "application/x-www-form-urlencoded", false, true);

    // set http timeout
    rte_http.setTimeout (RTE_HTTP_TIMEOUT);
  }

  return is_ok;
}

// start of signal reception
bool RteTempoStreamGet ()
{
  bool is_ok;
  int  http_code;

  // connexion
  http_code = rte_http.GET ();

  // handle realtime data reception
  RteProcessRealTime ();

  // check if connexion failed
  is_ok = ((http_code == HTTP_CODE_OK) || (http_code == HTTP_CODE_MOVED_PERMANENTLY));
  if (is_ok) AddLog (LOG_LEVEL_DEBUG, PSTR ("RTE: Tempo - Success [%d]"), http_code);
  else
  {
    AddLog (LOG_LEVEL_DEBUG, PSTR ("RTE: Tempo - Error %s [%d]"), rte_http.errorToString (http_code).c_str(), http_code);
    rte_http.end ();
    rte_wifi.stop ();
  }

  return is_ok;
}

// handle end of signals page reception
bool RteTempoStreamEnd ()
{
  uint16_t  size;
  char      str_color[16];
  JsonArray arr_day;
  DynamicJsonDocument json_result(1024);

  // extract token from JSON
  deserializeJson (json_result, rte_http.getString ().c_str ());

  // check array Size
  arr_day = json_result["tempo_like_calendars"]["values"].as<JsonArray>();
  size = arr_day.size ();

  // if tomorrow is not available
  if (size == 2)
  {
    tempo_status.arr_day[RTE_DAY_TOMORROW]  = RTE_TEMPO_LEVEL_UNKNOWN;
    tempo_status.arr_day[RTE_DAY_TODAY]     = GetCommandCode (str_color, sizeof (str_color), arr_day[0]["value"], kRteTempoStream);
    tempo_status.arr_day[RTE_DAY_YESTERDAY] = GetCommandCode (str_color, sizeof (str_color), arr_day[1]["value"], kRteTempoStream);
  }

  // else tomorrow is available
  else if (size == 3)
  {
    tempo_status.arr_day[RTE_DAY_TOMORROW]  = GetCommandCode (str_color, sizeof (str_color), arr_day[0]["value"], kRteTempoStream);
    tempo_status.arr_day[RTE_DAY_TODAY]     = GetCommandCode (str_color, sizeof (str_color), arr_day[1]["value"], kRteTempoStream);
    tempo_status.arr_day[RTE_DAY_YESTERDAY] = GetCommandCode (str_color, sizeof (str_color), arr_day[2]["value"], kRteTempoStream);
  }

  // set signal next update
  if (tempo_status.arr_day[RTE_DAY_TOMORROW] == RTE_TEMPO_LEVEL_UNKNOWN) rte_update.time_tempo = LocalTime () + RTE_TEMPO_RENEW_UNKNOWN;
    else rte_update.time_tempo = LocalTime () + RTE_TEMPO_RENEW_KNOWN;

  // log
  AddLog (LOG_LEVEL_DEBUG, PSTR ("RTE: Tempo - Update done (%d-%d-%d)"), tempo_status.arr_day[RTE_DAY_YESTERDAY], tempo_status.arr_day[RTE_DAY_TODAY], tempo_status.arr_day[RTE_DAY_TOMORROW]);

  // end connexion
  rte_http.end ();
  rte_wifi.stop ();

  return true;
}

/***********************************\
 *          Pointe management
\***********************************/

bool RtePointeIsEnabled ()
{
  return (rte_config.pointe_enable == 1);
}

uint8_t RtePointeGetGlobalLevel (const uint8_t day, const uint8_t slot)
{
  uint8_t level, result;

  // get level
  level = RtePointeGetLevel (day, slot);

  // convert level
  switch (level)
  {
    case RTE_POINTE_LEVEL_BLUE: result = RTE_LEVEL_BLUE;    break;
    case RTE_POINTE_LEVEL_RED:  result = RTE_LEVEL_RED;     break;
    default:                    result = RTE_LEVEL_UNKNOWN; break;
  }

  return result;
}

uint8_t RtePointeGetLevel (const uint8_t day, const uint8_t slot)
{
  uint8_t level = RTE_POINTE_LEVEL_UNKNOWN;

  // check parameter
  if ((day != RTE_DAY_TODAY) && (day != RTE_DAY_TOMORROW)) return level;

  // calculate level
  if ((slot >= 1) && (slot < 7)) level = RTE_POINTE_LEVEL_BLUE;
    else if ((day == RTE_DAY_TODAY) && (slot < 1)) level = pointe_status.arr_day[RTE_DAY_YESTERDAY];
    else if (day == RTE_DAY_TODAY) level = pointe_status.arr_day[RTE_DAY_TODAY];
    else if ((day == RTE_DAY_TOMORROW) && (slot < 1)) level = pointe_status.arr_day[RTE_DAY_TODAY];
    else level = pointe_status.arr_day[RTE_DAY_TOMORROW];

  return level;
}

// plan pointe retry
void RtePointeStreamRetry ()
{
  rte_update.step       = RTE_UPDATE_NONE;
  rte_update.time_tempo = LocalTime () + RTE_POINTE_RETRY;
}

// start of pointe period reception
bool RtePointeStreamBegin ()
{
  bool is_ok;
  char str_text[256];

  // check token
  is_ok = (strlen (rte_update.str_token) > 0);
  if (!is_ok) AddLog (LOG_LEVEL_ERROR, PSTR ("RTE: Pointe - Token missing"));

  // prepare connexion
  if (is_ok) 
  {
    // connexion
    sprintf (str_text, "%s", RTE_URL_POINTE_DATA);
    is_ok = rte_http.begin (rte_wifi, str_text);

    // connexion report
    if (is_ok) AddLog (LOG_LEVEL_DEBUG, PSTR ("RTE: Pointe - Connexion done [%s]"), str_text);
    else
    {
      AddLog (LOG_LEVEL_DEBUG, PSTR ("RTE: Pointe - Connexion refused [%s]"), str_text);
      rte_http.end ();
      rte_wifi.stop ();
    }
  }

  // set headers
  if (is_ok)
  {
    // set authorisation
    strcpy (str_text, "Bearer ");
    strlcat (str_text, rte_update.str_token, sizeof (str_text) - 1);

    // set headers
    rte_http.addHeader ("Authorization", str_text, false, true);
    rte_http.addHeader ("Content-Type", "application/x-www-form-urlencoded", false, true);

    // set http timeout
    rte_http.setTimeout (RTE_HTTP_TIMEOUT);
  }

  return is_ok;
}

// start of pointe period data reception
bool RtePointeStreamGet ()
{
  bool is_ok;
  int  http_code;

  // connexion
  http_code = rte_http.GET ();

  // handle realtime data reception
  RteProcessRealTime ();

  // check if connexion failed
  is_ok = ((http_code == HTTP_CODE_OK) || (http_code == HTTP_CODE_MOVED_PERMANENTLY));
  if (is_ok) AddLog (LOG_LEVEL_DEBUG, PSTR ("RTE: Pointe - Success [%d]"), http_code);
  else
  {
    AddLog (LOG_LEVEL_DEBUG, PSTR ("RTE: Pointe - Error %s [%d]"), rte_http.errorToString (http_code).c_str(), http_code);
    rte_http.end ();
    rte_wifi.stop ();
  }

  return is_ok;
}

// handle end of signals page reception
bool RtePointeStreamEnd ()
{
  uint8_t   index;
  uint16_t  size;
  char      str_color[16];
  JsonArray arr_day;
  DynamicJsonDocument json_result(2048);

  // extract token from JSON
  deserializeJson (json_result, rte_http.getString ().c_str ());

  // check array size
  arr_day = json_result["signals"][0]["signaled_dates"].as<JsonArray>();
  size = arr_day.size ();

  // if both days are available
  if (size == 2)
  {
    // set tomorrow
    pointe_status.arr_day[RTE_DAY_TOMORROW] = 1 + arr_day[0]["aoe_signals"].as<unsigned int>();
    if (pointe_status.arr_day[RTE_DAY_TOMORROW] > RTE_POINTE_LEVEL_RED) pointe_status.arr_day[RTE_DAY_TOMORROW] = RTE_POINTE_LEVEL_RED;

    // set today
    pointe_status.arr_day[RTE_DAY_TODAY] = 1 + arr_day[1]["aoe_signals"].as<unsigned int>();
    if (pointe_status.arr_day[RTE_DAY_TODAY] > RTE_POINTE_LEVEL_RED) pointe_status.arr_day[RTE_DAY_TODAY] = RTE_POINTE_LEVEL_RED;
  }

  // set signal next update
  rte_update.time_pointe = LocalTime () + RTE_POINTE_RENEW;

  // log
  AddLog (LOG_LEVEL_DEBUG, PSTR ("RTE: Pointe - Update done (%d-%d-%d)"), pointe_status.arr_day[RTE_DAY_YESTERDAY], pointe_status.arr_day[RTE_DAY_TODAY], pointe_status.arr_day[RTE_DAY_TOMORROW]);

  // end connexion
  rte_http.end ();
  rte_wifi.stop ();

  return true;
}

/***********************************************************\
 *                      Callback
\***********************************************************/

// module init
void RteModuleInit ()
{
  // disable wifi sleep mode
  Settings->flag5.wifi_no_sleep = true;
  TasmotaGlobal.wifi_stay_asleep = false;
}

// init main status
void RteInit ()
{
  uint8_t index, slot;

  // init data*
  strcpy (rte_update.str_token, "");

  // load configuration file
  RteLoadConfig ();

  // tempo initialisation
  for (index = 0; index < RTE_DAY_MAX; index ++) tempo_status.arr_day[index] = RTE_TEMPO_LEVEL_UNKNOWN;

  // pointe initialisation
  for (index = 0; index < RTE_DAY_MAX; index ++) pointe_status.arr_day[index] = RTE_POINTE_LEVEL_UNKNOWN;

  // ecowatt initialisation
  for (index = 0; index < RTE_ECOWATT_DAY_MAX; index ++)
  {
    strcpy (ecowatt_status.arr_day[index].str_day_of_week, "");
    ecowatt_status.arr_day[index].day_of_month = 0;
    ecowatt_status.arr_day[index].month        = 0;
    ecowatt_status.arr_day[index].dvalue       = RTE_ECOWATT_LEVEL_NORMAL;
    for (slot = 0; slot < 24; slot ++) ecowatt_status.arr_day[index].arr_hvalue[slot] = RTE_ECOWATT_LEVEL_NORMAL;
  }

  // set insecure mode to access RTE server
  rte_wifi.setInsecure ();

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: rte_help to get help on RTE modules"));
}

// called every second
void RteEverySecond ()
{
  bool     result;
  uint8_t  period, level;
  uint32_t time_now;

  // check parameters
  if (!RtcTime.valid) return;

  // if needed, plan initial ecowatt and tempo updates
  if (rte_config.tempo_enable && (rte_update.time_tempo == UINT32_MAX)) rte_update.time_tempo = time_now + 15;
  if (rte_config.pointe_enable && (rte_update.time_pointe == UINT32_MAX)) rte_update.time_pointe = time_now + 30;
  if (rte_config.ecowatt_enable && (rte_update.time_ecowatt == UINT32_MAX)) rte_update.time_ecowatt = time_now + 45;

  //   Stream recetion
  // --------------------
  time_now = LocalTime ();
  switch (rte_update.step)
  {
    // no current stream operation
    case RTE_UPDATE_NONE:
      // if first update, ask for token update
      if ((rte_update.time_token == UINT32_MAX) && (rte_config.tempo_enable || rte_config.pointe_enable || rte_config.ecowatt_enable)) rte_update.time_token = time_now;

      // priority 1 : check for need of token update
      if (rte_update.time_token <= time_now) rte_update.step = RTE_UPDATE_TOKEN_BEGIN;

      // else, priority 2 : check for need of tempo update
      else if (rte_config.tempo_enable && (rte_update.time_tempo <= time_now)) rte_update.step = RTE_UPDATE_TEMPO_BEGIN;

      // else, priority 3 : check for need of pointe update
      else if (rte_config.pointe_enable && (rte_update.time_pointe <= time_now)) rte_update.step = RTE_UPDATE_POINTE_BEGIN;

      // else, priority 4 : check for need of ecowatt update
      else if (rte_config.ecowatt_enable && (rte_update.time_ecowatt <= time_now)) rte_update.step = RTE_UPDATE_ECOWATT_BEGIN;
      break;

    // begin token reception
    case RTE_UPDATE_TOKEN_BEGIN:
      // init token update
      result = RteStreamTokenBegin ();
      
      // if success, set next step else plan retry
      if (result) rte_update.step = RTE_UPDATE_TOKEN_GET; 
        else RteStreamTokenRetry ();
      break;

    // start token reception
    case RTE_UPDATE_TOKEN_GET:
      // init token update
      result = RteStreamTokenGet ();
      
      // if success, set next step else plan retry
      if (result) rte_update.step = RTE_UPDATE_TOKEN_END;
        else RteStreamTokenRetry ();
      break;

    // handle end of token reception
    case RTE_UPDATE_TOKEN_END:
      // manage received data
      result = RteStreamTokenEnd ();

      // if token is received, reset operation and plan next signal update else plan retry
      if (result) rte_update.step = RTE_UPDATE_NONE;
        else RteStreamTokenRetry ();
      break;

    // init ecowatt reception
    case RTE_UPDATE_ECOWATT_BEGIN:
      // init ecowatt update
      result = RteEcowattStreamBegin ();
      
      // if success, set next step else plan retry
      if (result) rte_update.step = RTE_UPDATE_ECOWATT_GET; 
        else RteEcowattStreamRetry ();
      break;

    // start ecowatt reception
    case RTE_UPDATE_ECOWATT_GET:
      // ecowatt reception
      result = RteEcowattStreamGet ();
      
      // if success, set next step else plan retry
      if (result) rte_update.step = RTE_UPDATE_ECOWATT_END;
        else RteEcowattStreamRetry ();
      break;

    // end of ecowatt reception
    case RTE_UPDATE_ECOWATT_END:
      // manage received data
      result = RteEcowattStreamEnd ();

      // if ecowatt not received, plan retry
      if (result) rte_update.step = RTE_UPDATE_NONE;
        else RteEcowattStreamRetry ();
      break;

    // init tempo reception
    case RTE_UPDATE_TEMPO_BEGIN:
      // init tempo update
      result = RteTempoStreamBegin ();
      
      // if success, set next step else plan retry
      if (result) rte_update.step = RTE_UPDATE_TEMPO_GET; 
        else RteTempoStreamRetry ();
      break;

    // start tempo reception
    case RTE_UPDATE_TEMPO_GET:
      // init tempo reception
      result = RteTempoStreamGet ();
      
      // if success, set next step else plan retry
      if (result) rte_update.step = RTE_UPDATE_TEMPO_END;
        else RteTempoStreamRetry ();
      break;

    // end of tempo reception
    case RTE_UPDATE_TEMPO_END:
      // manage received data
      result = RteTempoStreamEnd ();

      // if tempo not received, plan retry
      if (result) rte_update.step = RTE_UPDATE_NONE;
        else RteTempoStreamRetry ();
      break;

    // init pointe period reception
    case RTE_UPDATE_POINTE_BEGIN:
      // init tempo update
      result = RtePointeStreamBegin ();
      
      // if success, set next step else plan retry
      if (result) rte_update.step = RTE_UPDATE_POINTE_GET; 
        else RtePointeStreamRetry ();
      break;

    // start pointe period reception
    case RTE_UPDATE_POINTE_GET:
      // init tempo reception
      result = RtePointeStreamGet ();
      
      // if success, set next step else plan retry
      if (result) rte_update.step = RTE_UPDATE_POINTE_END;
        else RtePointeStreamRetry ();
      break;

    // end of pointe period reception
    case RTE_UPDATE_POINTE_END:
      // manage received data
      result = RtePointeStreamEnd ();

      // if tempo not received, plan retry
      if (result) rte_update.step = RTE_UPDATE_NONE;
        else RtePointeStreamRetry ();
      break;
  }

  //   Tempo status
  // -----------------
  if (rte_config.tempo_enable)
  {
    // get current tempo period and color
    period = RteTempoGetPeriod (RtcTime.hour);
    level  = RteTempoGetLevel (RTE_DAY_TODAY, RtcTime.hour);

    // check for tempo JSON update
    if (tempo_status.json_time == UINT32_MAX) tempo_status.json_publish = 1;
      else if (tempo_status.json_time < time_now) tempo_status.json_publish = 1;
      else if (tempo_status.last_period != period) tempo_status.json_publish = 1;
      else if (tempo_status.last_level != level) tempo_status.json_publish = 1;

    // update tempo period and color
    tempo_status.last_period = period;
    tempo_status.last_level  = level;
  }

  //   Pointe status
  // -----------------
  if (rte_config.pointe_enable)
  {
    // check for tempo JSON update
    if (pointe_status.json_time == UINT32_MAX) pointe_status.json_publish = 1;
      else if (pointe_status.json_time < time_now) pointe_status.json_publish = 1;
      else if (pointe_status.last_level != pointe_status.arr_day[RTE_DAY_TOMORROW]) pointe_status.json_publish = 1;

    // update pointe color
    pointe_status.last_level = pointe_status.arr_day[RTE_DAY_TOMORROW];
  }

  //   Ecowatt status
  // -------------------
  if (rte_config.ecowatt_enable)
  {
    // check for ecowatt JSON update
    if (ecowatt_status.json_time == UINT32_MAX) ecowatt_status.json_publish = 1;
      else if (ecowatt_status.json_time < time_now) ecowatt_status.json_publish = 1;
  }

  // if needed, publish TEMPO, POINTE or ECOWATT
  if (tempo_status.json_publish) RtePublishTempoJson ();
  else if (pointe_status.json_publish) RtePublishPointeJson ();
  else if (ecowatt_status.json_publish) RtePublishEcowattJson ();
}

// called every day at midnight
void RteMidnigth ()
{
  uint8_t index, slot;

  // check validity
  if (!RtcTime.valid) return;

  // handle ecowatt data rotation
  // ----------------------------

  // shift day's data from 0 to day+2
  for (index = 0; index < RTE_ECOWATT_DAY_MAX - 1; index ++)
  {
    // shift date, global status and slots for day, day+1 and day+2
    strcpy (ecowatt_status.arr_day[index].str_day_of_week, ecowatt_status.arr_day[index + 1].str_day_of_week);
    ecowatt_status.arr_day[index].day_of_month = ecowatt_status.arr_day[index + 1].day_of_month;
    ecowatt_status.arr_day[index].month        = ecowatt_status.arr_day[index + 1].month;
    ecowatt_status.arr_day[index].dvalue       = ecowatt_status.arr_day[index + 1].dvalue;
    for (slot = 0; slot < 24; slot ++) ecowatt_status.arr_day[index].arr_hvalue[slot] = ecowatt_status.arr_day[index + 1].arr_hvalue[slot];
  }

  // init date, global status and slots for day+3
  index = RTE_ECOWATT_DAY_MAX - 1;
  strcpy (ecowatt_status.arr_day[index].str_day_of_week, "");
  ecowatt_status.arr_day[index].day_of_month = 0;
  ecowatt_status.arr_day[index].month        = 0;
  ecowatt_status.arr_day[index].dvalue       = RTE_ECOWATT_LEVEL_NORMAL;
  for (slot = 0; slot < 24; slot ++) ecowatt_status.arr_day[index].arr_hvalue[slot] = RTE_ECOWATT_LEVEL_NORMAL;

  // publish data change
  if (rte_config.ecowatt_enable) ecowatt_status.json_publish = 1;

  // handle tempo data rotation
  // --------------------------
  
  // rotate days
  tempo_status.arr_day[RTE_DAY_YESTERDAY] = tempo_status.arr_day[RTE_DAY_TODAY];
  tempo_status.arr_day[RTE_DAY_TODAY]     = tempo_status.arr_day[RTE_DAY_TOMORROW];
  tempo_status.arr_day[RTE_DAY_TOMORROW]  = RTE_TEMPO_LEVEL_UNKNOWN;

  // publish data change
  if (rte_config.tempo_enable) tempo_status.json_publish = 1;

  // handle pointe data rotation
  // ---------------------------
  
  // rotate days
  pointe_status.arr_day[RTE_DAY_YESTERDAY] = pointe_status.arr_day[RTE_DAY_TODAY];
  pointe_status.arr_day[RTE_DAY_TODAY]     = pointe_status.arr_day[RTE_DAY_TOMORROW];
  pointe_status.arr_day[RTE_DAY_TOMORROW]  = RTE_POINTE_LEVEL_UNKNOWN;

  // publish data change
  if (rte_config.pointe_enable) pointe_status.json_publish = 1;
}

// Handle MQTT teleperiod
void RteTeleperiod ()
{
  // check parameter
  if (TasmotaGlobal.tele_period > 0) return;

  // publish JSON
  if (rte_config.tempo_enable) tempo_status.json_publish = true;
  if (rte_config.pointe_enable) pointe_status.json_publish = true;
  if (rte_config.ecowatt_enable) ecowatt_status.json_publish = true;
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// append presence forecast to main page
void RteWebSensor ()
{
  bool     ca_ready, pk_ready;
  uint8_t  index, slot, level, day;
  uint32_t time_now;
  TIME_T   dst_now;
  char     str_color[8];
  char     str_text[16];

  // check validity
  if (!RtcTime.valid) return;

  // init time
  time_now = LocalTime ();
  BreakTime (time_now, dst_now);

  // tempo display
  // -------------
  if (rte_config.tempo_enable && rte_config.tempo_display)
  {
    // start of tempo data
    WSContentSend_P (PSTR ("<div style='font-size:10px;text-align:center;padding:2px 6px 6px 6px;margin-bottom:5px;background:#333333;border-radius:12px;'>\n"));

    // title display
    WSContentSend_P (PSTR ("<div style='display:flex;margin:2px 0px 0px 0px;padding:0px;font-size:16px;'>\n"));
    WSContentSend_P (PSTR ("<div style='width:25%%;padding:0px;text-align:left;font-weight:bold;'>Tempo</div>\n"));
    WSContentSend_P (PSTR ("<div style='width:13%%;padding:4px 0px;text-align:left;font-size:10px;font-style:italic;'>RTE v1</div>\n"));
    WSContentSend_P (PSTR ("<div style='width:60%%;padding:4px 0px 8px 0px;text-align:right;font-size:10px;font-style:italic;'>"));

    // set display message
    if (strlen (rte_config.str_private_key) == 0) WSContentSend_P (PSTR ("clé privée RTE manquante"));
    else if (rte_update.time_token == UINT32_MAX) WSContentSend_P (PSTR ("initialisation"));
    else if (rte_update.time_tempo < time_now + 10) WSContentSend_P (PSTR ("maj imminente"));
    else if (rte_update.time_tempo != UINT32_MAX) WSContentSend_P (PSTR ("maj dans %d mn"), 1 + (rte_update.time_tempo - time_now) / 60);

    // end of title display
    WSContentSend_P (PSTR ("</div>\n"));
    WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div>\n"));
    WSContentSend_P (PSTR ("</div>\n"));

    // if data are available
    if (tempo_status.arr_day[RTE_DAY_TODAY] != RTE_TEMPO_LEVEL_UNKNOWN)
    {
      // loop thru days
      for (index = RTE_DAY_TODAY; index < RTE_DAY_MAX; index ++)
      {
        WSContentSend_P (PSTR ("<div style='display:flex;margin:0px;padding:1px;height:14px;'>\n"));

        // display day
        if (index == RTE_DAY_TODAY) strcpy (str_text, "Aujourd'hui"); else strcpy (str_text, "Demain");
        WSContentSend_P (PSTR ("<div style='width:20.8%%;padding:0px;text-align:left;'>%s</div>\n"), str_text);

        // display hourly slots
        for (slot = 0; slot < 24; slot ++)
        {
          // calculate color of current slot
          level = RteTempoGetLevel (index, slot);
          GetTextIndexed (str_color, sizeof (str_color), level, kRteTempoColor);  

          // segment beginning
          WSContentSend_P (PSTR ("<div style='width:3.3%%;padding:1px 0px;background-color:%s;"), str_color);

          // set opacity for HC
          if ((slot < 6) || (slot >= 22)) WSContentSend_P (PSTR ("opacity:75%%;"));

          // first and last segment specificities
          if (slot == 0) WSContentSend_P (PSTR ("border-top-left-radius:6px;border-bottom-left-radius:6px;"));
            else if (slot == 23) WSContentSend_P (PSTR ("border-top-right-radius:6px;border-bottom-right-radius:6px;"));

          // current hour dot and hourly segment end
          if ((index == RTE_DAY_TODAY) && (slot == dst_now.hour))
          {
            GetTextIndexed (str_text, sizeof (str_text), tempo_status.arr_day[day], kRteTempoDot);  
            WSContentSend_P (PSTR ("font-size:9px;'>%s</div>\n"), str_text);
          }
          else WSContentSend_P (PSTR ("'></div>\n"));
        }
        WSContentSend_P (PSTR ("</div>\n"));

        // handle realtime data reception
        RteProcessRealTime ();
      }

      // hour scale
      WSContentSend_P (PSTR ("<div style='display:flex;margin:0px;padding:0px;'>\n"));
      WSContentSend_P (PSTR ("<div style='width:20.8%%;padding:0px;'></div>\n"), 0);
      WSContentSend_P (PSTR ("<div style='width:13.2%%;padding:0px;text-align:left;'>%uh</div>\n"), 0);
      WSContentSend_P (PSTR ("<div style='width:13.2%%;padding:0px;'>%uh</div>\n"), 6);
      WSContentSend_P (PSTR ("<div style='width:26.4%%;padding:0px;'>%uh</div>\n"), 12);
      WSContentSend_P (PSTR ("<div style='width:13.2%%;padding:0px;'>%uh</div>\n"), 18);
      WSContentSend_P (PSTR ("<div style='width:13.2%%;padding:0px;'>%uh</div>\n"), 22);
      WSContentSend_P (PSTR ("</div>\n"));
    }

    // end of tempo data
    WSContentSend_P (PSTR ("</div>\n")); 

    // handle realtime data reception
    RteProcessRealTime ();
  }

  // pointe period display
  // ---------------------
  if (rte_config.pointe_enable && rte_config.pointe_display)
  {
    // start of pointe data
    WSContentSend_P (PSTR ("<div style='font-size:10px;text-align:center;padding:2px 6px 6px 6px;margin-bottom:5px;background:#333333;border-radius:12px;'>\n"));

    // title display
    WSContentSend_P (PSTR ("<div style='display:flex;margin:2px 0px 0px 0px;padding:0px;font-size:16px;'>\n"));
    WSContentSend_P (PSTR ("<div style='width:25%%;padding:0px;text-align:left;font-weight:bold;'>Pointe</div>\n"));
    WSContentSend_P (PSTR ("<div style='width:13%%;padding:4px 0px;text-align:left;font-size:10px;font-style:italic;'>RTE v2</div>\n"));
    WSContentSend_P (PSTR ("<div style='width:60%%;padding:4px 0px 8px 0px;text-align:right;font-size:10px;font-style:italic;'>"));

    // set display message
    if (strlen (rte_config.str_private_key) == 0) WSContentSend_P (PSTR ("clé privée RTE manquante"));
    else if (rte_update.time_token == UINT32_MAX) WSContentSend_P (PSTR ("initialisation"));
    else if (rte_update.time_pointe < time_now + 10) WSContentSend_P (PSTR ("maj imminente"));
    else if (rte_update.time_pointe != UINT32_MAX) WSContentSend_P (PSTR ("maj dans %d mn"), 1 + (rte_update.time_pointe - time_now) / 60);

    // end of title display
    WSContentSend_P (PSTR ("</div>\n"));
    WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div>\n"));
    WSContentSend_P (PSTR ("</div>\n"));

    // if data are available
    if (pointe_status.arr_day[RTE_DAY_TODAY] != RTE_POINTE_LEVEL_UNKNOWN)
    {
      // loop thru days
      for (index = RTE_DAY_TODAY; index < RTE_DAY_MAX; index ++)
      {
        WSContentSend_P (PSTR ("<div style='display:flex;margin:0px;padding:1px;height:14px;'>\n"));

        // display day
        if (index == RTE_DAY_TODAY) strcpy (str_text, "Aujourd'hui"); else strcpy (str_text, "Demain");
        WSContentSend_P (PSTR ("<div style='width:20.8%%;padding:0px;text-align:left;'>%s</div>\n"), str_text);

        // display hourly slots
        for (slot = 0; slot < 24; slot ++)
        {
          // calculate color of current slot
          level = RtePointeGetLevel (index, slot);
          GetTextIndexed (str_color, sizeof (str_color), level, kRtePointeColor);  

          // segment beginning
          WSContentSend_P (PSTR ("<div style='width:3.3%%;padding:1px 0px;background-color:%s;"), str_color);

          // first and last segment specificities
          if (slot == 0) WSContentSend_P (PSTR ("border-top-left-radius:6px;border-bottom-left-radius:6px;"));
            else if (slot == 23) WSContentSend_P (PSTR ("border-top-right-radius:6px;border-bottom-right-radius:6px;"));

          // segment end
          if ((index == RTE_DAY_TODAY) && (slot == dst_now.hour)) WSContentSend_P (PSTR ("font-size:9px;'>⚪</div>\n"));
            else WSContentSend_P (PSTR ("'></div>\n"));
        }
        WSContentSend_P (PSTR ("</div>\n"));

        // handle realtime data reception
        RteProcessRealTime ();
      }

      // hour scale
      WSContentSend_P (PSTR ("<div style='display:flex;margin:0px;padding:0px;'>\n"));
      WSContentSend_P (PSTR ("<div style='width:20.8%%;padding:0px;'></div>\n"), 0);
      WSContentSend_P (PSTR ("<div style='width:6.6%%;padding:0px;'>%uh</div>\n"), 1);
      WSContentSend_P (PSTR ("<div style='width:13.2%%;padding:0px;'></div>\n"));
      WSContentSend_P (PSTR ("<div style='width:6.6%%;padding:0px;'>%uh</div>\n"), 7);
      WSContentSend_P (PSTR ("<div style='width:26.4%%;padding:0px;'>%uh</div>\n"), 12);
      WSContentSend_P (PSTR ("<div style='width:13.2%%;padding:0px;'>%uh</div>\n"), 18);
      WSContentSend_P (PSTR ("<div style='width:13.2%%;padding:0px;text-align:right;'>%uh</div>\n"), 24);
      WSContentSend_P (PSTR ("</div>\n"));
    }

    // end of pointe data
    WSContentSend_P (PSTR ("</div>\n")); 

    // handle realtime data reception
    RteProcessRealTime ();
  }

  // ecowatt display
  // ---------------
  if (rte_config.ecowatt_enable && rte_config.ecowatt_display)
  {
    // start of ecowatt data
    WSContentSend_P (PSTR ("<div style='font-size:10px;text-align:center;padding:2px 6px 6px 6px;margin-bottom:5px;background:#333333;border-radius:12px;'>\n"));

    // title display
    WSContentSend_P (PSTR ("<div style='display:flex;margin:2px 0px 0px 0px;padding:0px;font-size:16px;'>\n"));
    WSContentSend_P (PSTR ("<div style='width:25%%;padding:0px;text-align:left;font-weight:bold;'>Ecowatt</div>\n"));
    WSContentSend_P (PSTR ("<div style='width:13%%;padding:4px 0px;text-align:left;font-size:10px;font-style:italic;'>RTE v5</div>\n"));
    WSContentSend_P (PSTR ("<div style='width:60%%;padding:4px 0px 8px 0px;text-align:right;font-size:10px;font-style:italic;'>"));

    // set display message
    if (strlen (rte_config.str_private_key) == 0) WSContentSend_P (PSTR ("clé privée RTE manquante"));
    else if (rte_update.time_token == UINT32_MAX) WSContentSend_P (PSTR ("initialisation"));
    else if (rte_update.time_ecowatt < time_now + 10) WSContentSend_P (PSTR ("maj imminente"));
    else if (rte_update.time_ecowatt != UINT32_MAX) WSContentSend_P (PSTR ("maj dans %d mn"), 1 + (rte_update.time_ecowatt - time_now) / 60);

    // end of title display
    WSContentSend_P (PSTR ("</div>\n"));
    WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div>\n"));
    WSContentSend_P (PSTR ("</div>\n"));

    // handle realtime data reception
    RteProcessRealTime ();

    // if ecowatt signal has been received previously
    if (ecowatt_status.arr_day[0].month != 0)
    {
      // loop thru days
      for (index = 0; index < RTE_ECOWATT_DAY_MAX; index ++)
      {
        WSContentSend_P (PSTR ("<div style='display:flex;margin:0px;padding:1px;height:14px;opacity:%u%%;'>\n"), 100);

        // display day
        if (index == 0) WSContentSend_P (PSTR ("<div style='width:20.8%%;padding:0px;text-align:left;'>%s</div>\n"), "Aujourd'hui");
          else if (index == 1) WSContentSend_P (PSTR ("<div style='width:20.8%%;padding:0px;text-align:left;'>%s</div>\n"), "Demain");
          else if (strlen (ecowatt_status.arr_day[index].str_day_of_week) == 0) WSContentSend_P (PSTR ("<div style='width:20.8%%;padding:0px;text-align:left;'>%s</div>\n"), "");
          else WSContentSend_P (PSTR ("<div style='width:8.4%%;padding:0px;text-align:left;'>%s</div><div style='width:12.4%%;padding:0px;text-align:left;'>%02u/%02u</div>\n"), ecowatt_status.arr_day[index].str_day_of_week, ecowatt_status.arr_day[index].day_of_month, ecowatt_status.arr_day[index].month);

        // display hourly slots
        for (slot = 0; slot < 24; slot ++)
        {
          // segment beginning
          level = ecowatt_status.arr_day[index].arr_hvalue[slot];
          GetTextIndexed (str_color, sizeof (str_color), level, kRteEcowattLevelColor);
          WSContentSend_P (PSTR ("<div style='width:3.3%%;padding:1px 0px;background-color:%s;"), str_color);

          // first and last segment specificities
          if (slot == 0) WSContentSend_P (PSTR ("border-top-left-radius:6px;border-bottom-left-radius:6px;"));
            else if (slot == 23) WSContentSend_P (PSTR ("border-top-right-radius:6px;border-bottom-right-radius:6px;"));

          // segment end
          if ((index == RTE_ECOWATT_DAY_TODAY) && (slot == dst_now.hour)) WSContentSend_P (PSTR ("font-size:9px;'>⚪</div>\n"));
            else WSContentSend_P (PSTR ("'></div>\n"));
        }
        WSContentSend_P (PSTR ("</div>\n"));

        // handle realtime data reception
        RteProcessRealTime ();
      }

      // hour scale
      WSContentSend_P (PSTR ("<div style='display:flex;margin:0px;padding:0px;'>\n"));
      WSContentSend_P (PSTR ("<div style='width:20.8%%;padding:0px;'></div>\n"), 0);
      WSContentSend_P (PSTR ("<div style='width:13.2%%;padding:0px;text-align:left;'>%uh</div>\n"), 0);
      WSContentSend_P (PSTR ("<div style='width:13.2%%;padding:0px;'>%uh</div>\n"), 6);
      WSContentSend_P (PSTR ("<div style='width:26.4%%;padding:0px;'>%uh</div>\n"), 12);
      WSContentSend_P (PSTR ("<div style='width:13.2%%;padding:0px;'>%uh</div>\n"), 18);
      WSContentSend_P (PSTR ("<div style='width:13.2%%;padding:0px;text-align:right;'>%uh</div>\n"), 24);
      WSContentSend_P (PSTR ("</div>\n"));
    }  

    // end of ecowatt data
    WSContentSend_P (PSTR ("</div>\n")); 

    // handle realtime data reception
    RteProcessRealTime ();
  }
}

#endif  // USE_WEBSERVER

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xsns119 (uint32_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
    case FUNC_MODULE_INIT:
      RteModuleInit ();
      break;
    case FUNC_INIT:
      RteInit ();
      break;
    case FUNC_COMMAND:
      result  = DecodeCommand (kRteCommands,     RteCommand);
      result |= DecodeCommand (kEcowattCommands, EcowattCommand);
      result |= DecodeCommand (kTempoCommands,   TempoCommand);
      result |= DecodeCommand (kPointeCommands,  PointeCommand);
      break;
    case FUNC_EVERY_SECOND:
      RteEverySecond ();
      break;
    case FUNC_SAVE_AT_MIDNIGHT:
      RteMidnigth ();
      break;
    case FUNC_JSON_APPEND:
      RteTeleperiod ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      RteWebSensor ();
      break;
#endif  // USE_WEBSERVER
  }
  
  return result;
}

#endif    // USE_RTE_SERVER
#endif    // USE_UFILESYS
#endif    // ESP32
