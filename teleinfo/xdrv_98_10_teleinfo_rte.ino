/*
xdrv_98_10_teleinfo_rte.ino - Handle access to RTE server for Tempo, Pointe and Ecowatt

  Only compatible with ESP32

  Copyright (C) 2022-2025  Nicolas Bernaerts
    06/10/2022 v1.0 - Creation 
    21/10/2022 v1.1 - Add sandbox management (with day shift to have green/orange/red on current day)
    09/02/2023 v1.2 - Disable wifi sleep to avoid latency
    15/05/2023 v1.3 - Rewrite CSV file access
    14/10/2023 v1.4 - Rewrite API management and intergrate RTE root certificate
    28/10/2023 v1.5 - Change in ecowatt stream reception to avoid overload
    19/11/2023 v2.0 - Switch to BearSSL::WiFiClientSecure_light
    04/12/2023 v3.0 - Rename to xsns_119_rte_server.ino
                        Rename ecowatt.cfg to rte.cfg
                        Add Tempo calendar
    07/12/2023 v3.1 - Handle Ecowatt v4 and v5
    19/12/2023 v3.2 - Add Pointe calendar
    05/02/2024 v3.3 - Switch to Ecowatt v5 only
    25/02/2024 v3.4 - Publish topics sequentially to avoid reception errors
    10/11/2024 v3.5 - Rework around HTTPs connexions
    30/04/2025 v3.6 - Set start delay to protect single core ESP boot process 
    10/07/2025 v4.0 - Refactoring based on Tasmota 15

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
#ifdef USE_TELEINFO

#include <ArduinoJson.h>

// global constant
#define RTE_HTTP_TIMEOUT            5000       // HTTP connexion timeout (ms)
#define RTE_TOKEN_RETRY             30         // token retry timeout (sec.)

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
#define D_CMND_RTE                  "rte"
#define D_CMND_RTE_ECOWATT          "ecowatt"
#define D_CMND_RTE_TEMPO            "tempo"
#define D_CMND_RTE_POINTE           "pointe"
#define D_CMND_RTE_KEY              "key"
#define D_CMND_RTE_TOKEN            "token"
#define D_CMND_RTE_DISPLAY          "dis"
#define D_CMND_RTE_SANDBOX          "sand"
#define D_CMND_RTE_UPDATE           "upd"
#define D_CMND_RTE_PUBLISH          "pub"

// URL
static const char RTE_URL_OAUTH2[]          PROGMEM = "https://digital.iservices.rte-france.com/token/oauth/";
static const char RTE_URL_ECOWATT_DATA[]    PROGMEM = "https://digital.iservices.rte-france.com/open_api/ecowatt/v5/signals";
static const char RTE_URL_ECOWATT_SANDBOX[] PROGMEM = "https://digital.iservices.rte-france.com/open_api/ecowatt/v5/sandbox/signals";
static const char RTE_URL_TEMPO_DATA[]      PROGMEM = "https://digital.iservices.rte-france.com/open_api/tempo_like_supply_contract/v1/tempo_like_calendars";
static const char RTE_URL_POINTE_DATA[]     PROGMEM = "https://digital.iservices.rte-france.com/open_api/demand_response_signal/v2/signals";

// Commands
static const char kRteCommands[]  PROGMEM = D_CMND_RTE "|"       "|_" D_CMND_RTE_KEY  "|_" D_CMND_RTE_TOKEN  "|_"  D_CMND_RTE_SANDBOX;
void (* const RteCommand[])(void) PROGMEM = { &CmndTeleinfoRteHelp, &CmndTeleinfoRteKey, &CmndTeleinfoRteToken, &CmndTeleinfoRteSandbox };

static const char kEcowattCommands[]  PROGMEM =  D_CMND_RTE_ECOWATT "|"    "|_"    D_CMND_RTE_DISPLAY   "|_"    D_CMND_RTE_UPDATE   "|_"    D_CMND_RTE_PUBLISH;
void (* const EcowattCommand[])(void) PROGMEM = { &CmndTeleinfoEcowattEnable, &CmndTeleinfoEcowattDisplay, &CmndTeleinfoEcowattUpdate, &CmndTeleinfoEcowattPublish };

static const char kTempoCommands[]  PROGMEM = D_CMND_RTE_TEMPO "|"      "|_"   D_CMND_RTE_DISPLAY  "|_"  D_CMND_RTE_UPDATE   "|_"  D_CMND_RTE_PUBLISH;
void (* const TempoCommand[])(void) PROGMEM = {  &CmndTeleinfoTempoEnable, &CmndTeleinfoTempoDisplay, &CmndTeleinfoTempoUpdate, &CmndTeleinfoTempoPublish };

static const char kPointeCommands[]  PROGMEM = D_CMND_RTE_POINTE "|"     "|_"   D_CMND_RTE_DISPLAY   "|_"   D_CMND_RTE_UPDATE   "|_"  D_CMND_RTE_PUBLISH;
void (* const PointeCommand[])(void) PROGMEM = { &CmndTeleinfoPointeEnable, &CmndTeleinfoPointeDisplay, &CmndTeleinfoPointeUpdate, &CmndTeleinfoPointePublish };

// https stream status
enum RteHttpsUpdate { RTE_UPDATE_NONE, RTE_UPDATE_TOKEN, RTE_UPDATE_TEMPO, RTE_UPDATE_ECOWATT, RTE_UPDATE_POINTE, RTE_UPDATE_MAX};

// config
enum RteConfigKey { RTE_CONFIG_KEY, RTE_CONFIG_SANDBOX, RTE_CONFIG_ECOWATT, RTE_CONFIG_ECOWATT_DISPLAY, RTE_CONFIG_TEMPO, RTE_CONFIG_TEMPO_DISPLAY, RTE_CONFIG_POINTE, RTE_CONFIG_POINTE_DISPLAY, RTE_CONFIG_MAX };                   // configuration parameters
static const char kEcowattConfigKey[] PROGMEM = D_CMND_RTE_KEY "|" D_CMND_RTE_SANDBOX "|" D_CMND_RTE_ECOWATT "|" D_CMND_RTE_ECOWATT "_" D_CMND_RTE_DISPLAY "|" D_CMND_RTE_TEMPO "|" D_CMND_RTE_TEMPO "_" D_CMND_RTE_DISPLAY "|" D_CMND_RTE_POINTE "|" D_CMND_RTE_POINTE "_" D_CMND_RTE_DISPLAY;        // configuration keys

enum RteDay { RTE_DAY_YESTERDAY, RTE_DAY_TODAY, RTE_DAY_TOMORROW, RTE_DAY_MAX };
enum RtePeriod { RTE_PERIOD_PLEINE, RTE_PERIOD_CREUSE, RTE_PERIOD_MAX };
enum RteGlobalLevel { RTE_LEVEL_UNKNOWN, RTE_LEVEL_BLUE, RTE_LEVEL_WHITE, RTE_LEVEL_RED, RTE_LEVEL_MAX };

// tempo data
enum TempoLevel  { RTE_TEMPO_LEVEL_UNKNOWN, RTE_TEMPO_LEVEL_BLUE, RTE_TEMPO_LEVEL_WHITE, RTE_TEMPO_LEVEL_RED, RTE_TEMPO_LEVEL_MAX };
static const char kTeleinfoRteTempoStream[] PROGMEM = "|BLUE|WHITE|RED";
static const char kRteTempoJSON[]   PROGMEM = "Inconnu|Bleu|Blanc|Rouge";
static const char kRteTempoColor[]  PROGMEM = "#252525|#06b|#ccc|#b00";
static const char kRteTempoDot[]    PROGMEM = "⚪|⚪|⚫|⚪";
static const char kRteTempoIcon[]   PROGMEM = "❔|🟦|⬜|🟥|❔|🔵|⚪|🔴";

// pointe data
enum PointeLevel  { RTE_POINTE_LEVEL_UNKNOWN, RTE_POINTE_LEVEL_BLUE, RTE_POINTE_LEVEL_RED, RTE_POINTE_LEVEL_MAX };
static const char kRtePointeJSON[]  PROGMEM = "Inconnu|Bleu|Blanc";
static const char kRtePointeColor[] PROGMEM = "#252525|#06b|#900";
static const char kRtePointeIcon[]  PROGMEM = "❔|🟦|🟥";

// ecowatt data
enum EcowattDays { RTE_ECOWATT_DAY_TODAY, RTE_ECOWATT_DAY_TOMORROW, RTE_ECOWATT_DAY_PLUS2, RTE_ECOWATT_DAY_DAYPLUS3, RTE_ECOWATT_DAY_MAX };
enum EcowattLevel { RTE_ECOWATT_LEVEL_CARBONFREE, RTE_ECOWATT_LEVEL_NORMAL, RTE_ECOWATT_LEVEL_WARNING, RTE_ECOWATT_LEVEL_POWERCUT, RTE_ECOWATT_LEVEL_MAX };
static const char kRteEcowattLevelColor[] PROGMEM = "#0a0|#080|#F80|#A00";

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
  uint8_t     hour         = 0;                     // current hour
  uint8_t     step         = RTE_UPDATE_NONE;       // current reception step
  uint32_t    time_token   = UINT32_MAX;            // timestamp of token update
  uint32_t    time_ecowatt = UINT32_MAX;            // timestamp of signal update
  uint32_t    time_tempo   = UINT32_MAX;            // timestamp of tempo update
  uint32_t    time_pointe  = UINT32_MAX;            // timestamp of pointe period update
  char        str_token[128];                       // token value
} rte_update;

// Ecowatt status
struct rte_ecowatt_day {
  uint8_t dvalue;                                   // day global status
  char    str_day_of_week[4];                       // day of week (mon, Tue, ...)
  uint8_t day_of_month;                             // dd
  uint8_t month;                                    // mm
  uint8_t arr_hvalue[24];                           // hourly slots
};
static struct {
  uint8_t  json_publish = 0;                        // flag to publish JSON
  uint32_t json_time    = UINT32_MAX;               // timestamp of next JSON update
  rte_ecowatt_day arr_day[RTE_ECOWATT_DAY_MAX];     // slots for today and next days
} rte_ecowatt_status;

// Tempo status
static struct {
  uint8_t  json_publish = 0;                        // flag to publish JSON
  uint32_t json_time    = UINT32_MAX;               // timestamp of next JSON update
  uint8_t  last_period  = RTE_PERIOD_MAX;           // last period
  uint8_t  last_level   = RTE_TEMPO_LEVEL_MAX;      // last color
  int arr_day[RTE_DAY_MAX];                         // days status
} rte_tempo_status;

// Pointe status
static struct {
  uint8_t  json_publish = 0;                        // flag to publish JSON
  uint32_t json_time    = UINT32_MAX;               // timestamp of next JSON update
  uint8_t  last_level   = RTE_POINTE_LEVEL_MAX;     // last color
  int arr_day[RTE_DAY_MAX];                         // days status
} rte_pointe_status;

/************************************\
 *        RTE global commands
\************************************/

// RTE server help
void CmndTeleinfoRteHelp ()
{
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: commands about RTE server"));
  AddLog (LOG_LEVEL_INFO, PSTR (" RTE global commands :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - rte_key <key>     = set RTE base64 private key"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - rte_token         = display current token"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - rte_sand <0/1>    = set sandbox mode (%u)"), rte_config.sandbox);
  AddLog (LOG_LEVEL_INFO, PSTR (" Tempo commands :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - tempo <0/1>       = enable tempo server (%u)"), rte_config.tempo_enable);
  AddLog (LOG_LEVEL_INFO, PSTR (" - tempo_dis <0/1>   = display tempo calendar (%u)"), rte_config.tempo_display);
  AddLog (LOG_LEVEL_INFO, PSTR (" - tempo_upd         = update tempo from RTE server"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - tempo_pub         = publish tempo data"));
  AddLog (LOG_LEVEL_INFO, PSTR (" Pointe commands :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - pointe <0/1>      = enable pointe period server (%u)"), rte_config.pointe_enable);
  AddLog (LOG_LEVEL_INFO, PSTR (" - pointe_dis <0/1>  = display pointe calendar (%u)"), rte_config.pointe_display);
  AddLog (LOG_LEVEL_INFO, PSTR (" - pointe_upd        = update period from RTE server"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - pointe_pub        = publish pointe period data"));
  AddLog (LOG_LEVEL_INFO, PSTR (" Ecowatt commands :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ecowatt <0/1>     = enable ecowatt server (%u)"), rte_config.ecowatt_enable);
  AddLog (LOG_LEVEL_INFO, PSTR (" - ecowatt_dis <0/1> = display ecowatt calendar (%u)"), rte_config.ecowatt_display);
  AddLog (LOG_LEVEL_INFO, PSTR (" - ecowatt_upd       = update ecowatt from RTE server"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ecowatt_pub       = publish ecowatt data"));
  ResponseCmndDone ();
}

// RTE base64 private key
void CmndTeleinfoRteKey ()
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.data_len < 128))
  {
    strlcpy (rte_config.str_private_key, XdrvMailbox.data, sizeof (rte_config.str_private_key));
    TeleinfoRteSaveConfig ();
  }
  
  ResponseCmndChar (rte_config.str_private_key);
}

// RTE server token
void CmndTeleinfoRteToken ()
{
  ResponseCmndChar (rte_update.str_token);
}

// Set RTE sandbox mode
void CmndTeleinfoRteSandbox ()
{
  if (XdrvMailbox.data_len > 0)
  {
    // set sandbox mode
    rte_config.sandbox = (uint8_t)XdrvMailbox.payload;
    TeleinfoRteSaveConfig ();

    // force update
    rte_update.time_ecowatt = rte_update.time_tempo = LocalTime ();
  }
  
  ResponseCmndNumber (rte_config.sandbox);
}

/**********************************\
 *        Ecowatt commands
\**********************************/

// Enable ecowatt server
void CmndTeleinfoEcowattEnable ()
{
  if (XdrvMailbox.data_len > 0)
  {
    // update status
    rte_config.ecowatt_enable = (XdrvMailbox.payload != 0);
    if (rte_config.ecowatt_enable) rte_config.ecowatt_display = 1;
    rte_update.time_ecowatt = LocalTime ();
    TeleinfoRteSaveConfig ();
  }

  ResponseCmndNumber (rte_config.ecowatt_enable);
}

// Display ecowatt server
void CmndTeleinfoEcowattDisplay ()
{
  if (XdrvMailbox.data_len > 0)
  {
    rte_config.ecowatt_display = (XdrvMailbox.payload != 0);
    TeleinfoRteSaveConfig ();
  }

  ResponseCmndNumber (rte_config.ecowatt_display);
}

// Ecowatt forced update from RTE server
void CmndTeleinfoEcowattUpdate ()
{
  rte_update.time_ecowatt = LocalTime ();
  ResponseCmndDone ();
}

// Ecowatt publish data
void CmndTeleinfoEcowattPublish ()
{
  rte_ecowatt_status.json_time = LocalTime ();
  ResponseCmndDone ();
}

/**********************************\
 *         Tempo commands
\**********************************/

// Enable ecowatt server
void CmndTeleinfoTempoEnable ()
{
  if (XdrvMailbox.data_len > 0)
  {
    // update status
    rte_config.tempo_enable = (XdrvMailbox.payload != 0);
    if (rte_config.tempo_enable) rte_config.tempo_display = 1;
    rte_update.time_tempo = LocalTime ();
    TeleinfoRteSaveConfig ();
  }

  ResponseCmndNumber (rte_config.tempo_enable);
}

// Display tempo server
void CmndTeleinfoTempoDisplay ()
{
  if (XdrvMailbox.data_len > 0)
  {
    rte_config.tempo_display = (XdrvMailbox.payload != 0);
    TeleinfoRteSaveConfig ();
  }

  ResponseCmndNumber (rte_config.tempo_display);
}

// Tempo forced update from RTE server
void CmndTeleinfoTempoUpdate ()
{
  rte_update.time_tempo = LocalTime ();
  ResponseCmndDone ();
}

// Tempo publish data
void CmndTeleinfoTempoPublish ()
{
  rte_tempo_status.json_time = LocalTime ();
  ResponseCmndDone ();
}

/**********************************\
 *      Pointe Period commands
\**********************************/

// Enable pointe period server
void CmndTeleinfoPointeEnable ()
{
  bool enable;

  if (XdrvMailbox.data_len > 0)
  {
    // update status
    rte_config.pointe_enable = (XdrvMailbox.payload != 0);
    if (rte_config.pointe_enable) rte_config.pointe_display = 1;
    rte_update.time_pointe = LocalTime ();
    TeleinfoRteSaveConfig ();
  }

  ResponseCmndNumber (rte_config.pointe_enable);
}

// Display pointe server
void CmndTeleinfoPointeDisplay ()
{
  if (XdrvMailbox.data_len > 0)
  {
    rte_config.pointe_display = (XdrvMailbox.payload != 0);
    TeleinfoRteSaveConfig ();
  }

  ResponseCmndNumber (rte_config.pointe_display);
}

// Pointe period forced update from RTE server
void CmndTeleinfoPointeUpdate ()
{
  rte_update.time_pointe = LocalTime ();
  ResponseCmndDone ();
}

// Pointe period publish data
void CmndTeleinfoPointePublish ()
{
  rte_pointe_status.json_time = LocalTime ();
  ResponseCmndDone ();
}

/***********************************************************\
 *                      Configuration
\***********************************************************/

// load configuration
void TeleinfoRteLoadConfig () 
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
      pstr_value = strtok (nullptr, "\n");

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
void TeleinfoRteSaveConfig () 
{
  uint8_t index, value;
  char    str_number[4];
  char    str_line[128];
  File    file;

  // open file
  file = ffsp->open (D_RTE_CFG, "w");

  // loop to write config
  for (index = 0; index < RTE_CONFIG_MAX; index ++)
  {
    value = UINT8_MAX;
    if (GetTextIndexed (str_line, sizeof (str_line), index, kEcowattConfigKey) != nullptr)
    {
      // generate key=value
      strcat_P (str_line, PSTR ("="));
      switch (index)
        {
          case RTE_CONFIG_KEY:             strcat (str_line, rte_config.str_private_key); break;
          case RTE_CONFIG_SANDBOX:         value = rte_config.sandbox;                    break;
          case RTE_CONFIG_ECOWATT:         value = rte_config.ecowatt_enable;             break;
          case RTE_CONFIG_ECOWATT_DISPLAY: value = rte_config.ecowatt_display;            break;
          case RTE_CONFIG_TEMPO:           value = rte_config.tempo_enable;               break;
          case RTE_CONFIG_TEMPO_DISPLAY:   value = rte_config.tempo_display;              break;
          case RTE_CONFIG_POINTE:          value = rte_config.pointe_enable;              break;
          case RTE_CONFIG_POINTE_DISPLAY:  value = rte_config.pointe_display;             break;
        }

      // if needed, append value
      if (value != UINT8_MAX) sprintf_P (str_number, PSTR ("%u\n"), value);
        else strcpy_P (str_number, PSTR ("\n"));
      strcat (str_line, str_number); 

      // write to file
      file.print (str_line);
    }
  }

  // close file
  file.close ();
}

/*
bool RteIsEnabled ()
{
  return (rte_config.tempo_enable || rte_config.pointe_enable || rte_config.ecowatt_enable);
}
*/

/***************************************\
 *           JSON publication
\***************************************/

// publish RTE Tempo data thru MQTT
void TeleinfoRtePublishTempoJson ()
{
  uint8_t level, period;
  TIME_T  time_dst;
  char    str_label[8];
  char    str_icon[8];

  // init message
  ResponseClear ();
  ResponseAppendTime ();

  // "TEMPO":{...}}
  ResponseAppend_P (PSTR (",\"TEMPO\":{"));

  // publish current status
  level  = TeleinfoRteTempoGetLevel (RTE_DAY_TODAY, RtcTime.hour);
  period = TeleinfoRteTempoGetPeriod (RtcTime.hour);
  TeleinfoRteTempoGetIcon (RTE_DAY_TODAY, RtcTime.hour, str_icon, sizeof (str_icon));
  GetTextIndexed (str_label, sizeof (str_label), level, kRteTempoJSON);
  ResponseAppend_P (PSTR ("\"lv\":%u,\"hp\":%u,\"label\":\"%s\",\"icon\":\"%s\""), level, period, str_label, str_icon);

  // publish days
  ResponseAppend_P (PSTR (",\"yesterday\":%u,\"today\":%u,\"tomorrow\":%u"), rte_tempo_status.arr_day[RTE_DAY_YESTERDAY], rte_tempo_status.arr_day[RTE_DAY_TODAY], rte_tempo_status.arr_day[RTE_DAY_TOMORROW]);

  // publish message
  ResponseJsonEndEnd ();
  MqttPublishPrefixTopicRulesProcess_P (TELE, PSTR ("RTE"), false);

  // plan next update
  rte_tempo_status.json_publish = 0;
  rte_tempo_status.json_time    = LocalTime () + RTE_TEMPO_UPDATE_JSON;
}

// publish RTE pointe data thru MQTT
void TeleinfoRtePublishPointeJson ()
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
  level  = TeleinfoRtePointeGetLevel (RTE_DAY_TODAY, RtcTime.hour);
  GetTextIndexed (str_label, sizeof (str_label), level, kRtePointeJSON);
  GetTextIndexed (str_icon, sizeof (str_icon), level, kRtePointeIcon);
  ResponseAppend_P (PSTR ("\"lv\":%u,\"label\":\"%s\",\"icon\":\"%s\""), level, str_label, str_icon);

  // publish days
  ResponseAppend_P (PSTR (",\"today\":%u,\"tomorrow\":%u"), rte_pointe_status.arr_day[RTE_DAY_TODAY], rte_pointe_status.arr_day[RTE_DAY_TOMORROW]);

  // publish message
  ResponseJsonEndEnd ();
  MqttPublishPrefixTopicRulesProcess_P (TELE, PSTR ("RTE"), false);

  // plan next update
  rte_pointe_status.json_publish = 0;
  rte_pointe_status.json_time    = LocalTime () + RTE_POINTE_UPDATE_JSON;
}

// publish RTE data thru MQTT
void TeleinfoRtePublishEcowattJson ()
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
  ResponseAppend_P (PSTR ("\"dval\":%u,\"now\":%u,\"next\":%u"), rte_ecowatt_status.arr_day[day_now].dvalue,  rte_ecowatt_status.arr_day[day_now].arr_hvalue[hour_now], rte_ecowatt_status.arr_day[day_next].arr_hvalue[hour_next]);

  // loop thru number of slots to publish
  for (index = 0; index < RTE_ECOWATT_DAY_MAX; index ++)
  {
    ResponseAppend_P (PSTR (",\"day%u\":{\"jour\":\"%s\",\"date\":\"%u:%u\",\"dval\":%u"), index, rte_ecowatt_status.arr_day[index].str_day_of_week, rte_ecowatt_status.arr_day[index].day_of_month, rte_ecowatt_status.arr_day[index].month, rte_ecowatt_status.arr_day[index].dvalue);
    for (slot = 0; slot < 24; slot ++) ResponseAppend_P (PSTR (",\"%u\":%u"), slot, rte_ecowatt_status.arr_day[index].arr_hvalue[slot]);
    ResponseAppend_P (PSTR ("}"));
  }
  
  // publish message
  ResponseJsonEndEnd ();
  MqttPublishPrefixTopicRulesProcess_P (TELE, PSTR ("RTE"), false);

  // plan next update
  rte_ecowatt_status.json_publish = 0;
  rte_ecowatt_status.json_time    = LocalTime () + RTE_ECOWATT_UPDATE_JSON;
}

/***********************************\
 *        Token management
\***********************************/

// token connexion
bool TeleinfoRteStreamToken ()
{
  bool     is_ok;
  uint32_t time_now, token_validity;
  int      http_code;
  char     str_auth[128];
  DynamicJsonDocument json_result(192);
  HTTPClientLight*    phttp;

  // check private key
  is_ok = (strlen (rte_config.str_private_key) > 0);
  if (!is_ok)
  {
    AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Token - Private key missing"));
    return false;
  }

  // create connexion
  phttp = new HTTPClientLight ();

  // start connexion
  is_ok = (phttp != nullptr);
  if (is_ok)
  {
    // declare connexion
    is_ok = phttp->begin (RTE_URL_OAUTH2);
    if (is_ok) AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Token - Connexion accepted to %s"), RTE_URL_OAUTH2);
      else AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Token - Connexion failed to %s"), RTE_URL_OAUTH2);
  }
  else AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Token - Connexion creation failed"));

  // set connexion
  if (is_ok)
  {
    // set authorisation
    strcpy_P (str_auth, PSTR ("Basic "));
    strlcat (str_auth, rte_config.str_private_key, sizeof (str_auth) - 1);

    // set timeout and headers
    phttp->setTimeout (RTE_HTTP_TIMEOUT);
    phttp->addHeader (F ("Authorization"), str_auth, false, true);
    phttp->addHeader (F ("Accept"), F ("application/json"), false, true);

    // connexion
    http_code = phttp->POST (nullptr, 0);
    yield ();
    
    is_ok = ((http_code == HTTP_CODE_OK) || (http_code == HTTP_CODE_MOVED_PERMANENTLY));
    if (is_ok) AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Token - Success [%d]"), http_code);
      else AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Token - Error %s (%d)"), phttp->errorToString (http_code).c_str (), http_code);
  }

  // process received data
  if (is_ok)
  {
    // current time
    time_now = LocalTime ();

    // extract token from JSON
    deserializeJson (json_result, phttp->getString ().c_str ());

    // get token value
    strlcpy (rte_update.str_token, json_result[F ("access_token")].as<const char*> (), sizeof (rte_update.str_token));

    // get token validity
    token_validity = json_result[F ("expires_in")].as<unsigned int> ();

    // set next token and next signal update
    rte_update.time_token   = time_now + token_validity - 10;
    rte_update.time_tempo   = max (rte_update.time_tempo, time_now + 15);
    rte_update.time_pointe  = max (rte_update.time_pointe, time_now + 30);
    rte_update.time_ecowatt = max (rte_update.time_ecowatt, time_now + 30);

    // log
    AddLog (LOG_LEVEL_DEBUG, PSTR ("RTE: Token valid for %u seconds [%s]"), token_validity, rte_update.str_token);
  }

  // end connexion
  if (phttp)
  {
    phttp->end ();
    delete phttp;
  }

  return (strlen (rte_update.str_token) > 0);
}

/***********************************\
 *        Ecowatt management
\***********************************/

bool TeleinfoRteEcowattIsEnabled ()
{
  return (rte_config.ecowatt_enable == 1);
}

uint8_t TeleinfoRteEcowattGetGlobalLevel (const uint8_t day, const uint8_t slot)
{
  uint8_t level, result;

  // convert day and get level
  if (day == RTE_DAY_TODAY) level = rte_ecowatt_status.arr_day[RTE_ECOWATT_DAY_TODAY].arr_hvalue[slot];
  else if (day == RTE_DAY_TOMORROW) level = rte_ecowatt_status.arr_day[RTE_ECOWATT_DAY_TOMORROW].arr_hvalue[slot];
  else level = RTE_ECOWATT_LEVEL_MAX;

  // convert level
  switch (level)
  {
    case RTE_ECOWATT_LEVEL_CARBONFREE: result = RTE_LEVEL_BLUE;    break;
    case RTE_ECOWATT_LEVEL_NORMAL:     result = RTE_LEVEL_BLUE;    break;
    case RTE_ECOWATT_LEVEL_WARNING:    result = RTE_LEVEL_WHITE;   break;
    case RTE_ECOWATT_LEVEL_POWERCUT:   result = RTE_LEVEL_RED;     break;
    default:                           result = RTE_LEVEL_UNKNOWN; break;
  }

  return result;
}

// start of signal reception
bool TeleinfoRteEcowattStream ()
{
  bool        is_ok;
  uint8_t     slot, value;
  uint16_t    index, index_array, size;
  uint32_t    day_time;
  int         http_code;
  char        str_day[12];
  char        str_text[256];
  const char* pstr_url;
  TIME_T      day_dst;
  JsonArray           arr_day, arr_slot;
  DynamicJsonDocument json_result(8192);
  HTTPClientLight*    phttp;

  // check token
  is_ok = (strlen (rte_update.str_token) > 0);
  if (!is_ok)
  {
    AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Ecowatt - Token missing"));
    return false;
  }

  // create connexion
  phttp = new HTTPClientLight ();

  // start connexion
  is_ok = (phttp != nullptr);
  if (is_ok)
  {
    // connexion
    if (rte_config.sandbox) pstr_url = RTE_URL_ECOWATT_SANDBOX;
      else pstr_url = RTE_URL_ECOWATT_DATA;
    is_ok = phttp->begin (pstr_url);

    // connexion report
    if (is_ok) AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Ecowatt - Connexion done [%s]"), pstr_url);
      else AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Ecowatt - Connexion refused [%s]"), pstr_url);
  }
  else AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Ecowatt - Connexion creation failed"));

  // connexion
  if (is_ok)
  {
    // set authorisation
    strcpy_P (str_text, PSTR ("Bearer "));
    strlcat (str_text, rte_update.str_token, sizeof (str_text) - 1);

    // set timeout and headers
    phttp->setTimeout (RTE_HTTP_TIMEOUT);
    phttp->addHeader (F ("Authorization"), str_text, false, true);
    phttp->addHeader (F ("Accept"), F ("application/json"), false, true);

    // connexion
    http_code = phttp->GET ();
    yield ();

    // check if connexion failed
    is_ok = ((http_code == HTTP_CODE_OK) || (http_code == HTTP_CODE_MOVED_PERMANENTLY));
    if (is_ok) AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Ecowatt - Success [%d]"), http_code);
      else AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Ecowatt - Error %s [%d]"), phttp->errorToString (http_code).c_str(), http_code);
  }

  // process received data
  if (is_ok)
  {
    // extract token from JSON
    deserializeJson (json_result, phttp->getString ().c_str ());

    // check array Size
    arr_day = json_result[F ("signals")].as<JsonArray>();
    size = arr_day.size ();

    // loop thru all 4 days to get today's and tomorrow's index
    if (size <= RTE_ECOWATT_DAY_MAX) for (index = 0; index < size; index ++)
    {
      // if sandbox, shift array index to avoid current day fully ok
      if (rte_config.sandbox) index_array = (index + RTE_ECOWATT_DAY_MAX - 1) % RTE_ECOWATT_DAY_MAX;
        else index_array = index;

      // get global status
      rte_ecowatt_status.arr_day[index_array].dvalue = arr_day[index][F ("dvalue")].as<unsigned int>();

      // get date and convert from yyyy-mm-dd to dd.mm.yyyy
      strcpy (str_text, D_DAY3LIST);
      strlcpy (str_day, arr_day[index][F ("jour")], sizeof (str_day));
      str_day[4] = str_day[7] = str_day[10] = 0;
      day_dst.year = atoi (str_day) - 1970;
      day_dst.month = atoi (str_day + 5);
      day_dst.day_of_month = atoi (str_day + 8);
      day_dst.day_of_week = 0;
      day_time = MakeTime (day_dst);
      BreakTime (day_time, day_dst);
      if (day_dst.day_of_week < 8) strlcpy (str_day, str_text + 3 * (day_dst.day_of_week - 1), 4);
        else strcpy (str_day, "");

      // update current slot
      strlcpy (rte_ecowatt_status.arr_day[index_array].str_day_of_week, str_day, sizeof (rte_ecowatt_status.arr_day[index_array].str_day_of_week));
      rte_ecowatt_status.arr_day[index_array].day_of_month = day_dst.day_of_month;
      rte_ecowatt_status.arr_day[index_array].month        = day_dst.month;

      // loop to populate the slots
      arr_slot = arr_day[index][F ("values")].as<JsonArray>();
      for (slot = 0; slot < 24; slot ++)
      {
        value = arr_slot[slot][F ("hvalue")].as<unsigned int>();
        if (value >= RTE_ECOWATT_LEVEL_MAX) value = RTE_ECOWATT_LEVEL_NORMAL;
        rte_ecowatt_status.arr_day[index_array].arr_hvalue[slot] = value;
      }
    }

    // set signal next update
    rte_update.time_ecowatt = LocalTime () + RTE_ECOWATT_RENEW;

    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Ecowatt - Update till %02u/%02u"), rte_ecowatt_status.arr_day[index_array].day_of_month, rte_ecowatt_status.arr_day[index_array].month);
  }

  // end connexion
  if (phttp)
  {
    phttp->end ();
    delete phttp;
  }

  return is_ok;
}

/***********************************\
 *        Tempo management
\***********************************/

bool TeleinfoRteTempoIsEnabled ()
{
  return (rte_config.tempo_enable == 1);
}

uint8_t TeleinfoRteTempoGetPeriod (const uint8_t hour)
{
  uint8_t period = RTE_PERIOD_PLEINE;

  if ((hour < 6) || (hour > 21)) period = RTE_PERIOD_CREUSE;
  
  return period;
}

uint8_t TeleinfoRteTempoGetGlobalLevel (const uint8_t day, const uint8_t hour)
{
  uint8_t level, result;

  // get level
  level = TeleinfoRteTempoGetLevel (day, hour);

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

uint8_t TeleinfoRteTempoGetLevel (const uint8_t day, const uint8_t hour)
{
  uint8_t level = RTE_TEMPO_LEVEL_UNKNOWN;
  
  // check parameter
  if (day >= RTE_DAY_MAX) return level;

  // calculate level according to time
  if (hour >= 6) level = rte_tempo_status.arr_day[day];
    else if (day > RTE_DAY_YESTERDAY) level = rte_tempo_status.arr_day[day - 1];

  return level;
}

void TeleinfoRteTempoGetIcon (const uint8_t day, const uint8_t hour, char* pstr_icon, const size_t size_icon)
{
  uint8_t period, level, index;

  // get period and color
  period = TeleinfoRteTempoGetPeriod (hour);
  level  = TeleinfoRteTempoGetLevel (day, hour);

  // calculate index and get icon
  index = period * RTE_TEMPO_LEVEL_MAX + level;
  GetTextIndexed (pstr_icon, size_icon, index, kRteTempoIcon); 
}

// start of signal reception
bool TeleinfoRteTempoStream ()
{
  bool     is_ok;
  int      http_code;
  int32_t  tz_offset;
  uint16_t size;
  uint32_t current_time;
  char     str_text[256];
  TIME_T   start_dst, stop_dst;
  JsonArray           arr_day;
  DynamicJsonDocument json_result(1024);
  HTTPClientLight*    phttp;

  // check token
  is_ok = (strlen (rte_update.str_token) > 0);
  if (!is_ok)
  {
    AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Tempo - Token missing"));
    return false;
  }

  // create connexion
  phttp = new HTTPClientLight ();

  // start connexion
  is_ok = (phttp != nullptr);
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
    AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Tempo - URL %s"), str_text);

    // connexion
    is_ok = phttp->begin (str_text);

    // connexion report
    if (is_ok) AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Tempo - Connexion done [%s]"), str_text);
      else AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Tempo - Connexion refused [%s]"), str_text);
  }
  else AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Tempo - Connexion creation failed"));

  // set headers and call URL
  if (is_ok)
  {
    // set authorisation
    strcpy_P (str_text, PSTR ("Bearer "));
    strlcat (str_text, rte_update.str_token, sizeof (str_text) - 1);

    // set timeout and headers
    phttp->setTimeout (RTE_HTTP_TIMEOUT);
    phttp->addHeader (F ("Authorization"), str_text, false, true);
    phttp->addHeader (F ("Accept"), F ("application/json"), false, true);

    // connexion
    http_code = phttp->GET ();
    yield ();

    // check if connexion failed
    is_ok = ((http_code == HTTP_CODE_OK) || (http_code == HTTP_CODE_MOVED_PERMANENTLY));
    if (is_ok) AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Tempo - Success [%d]"), http_code);
    else AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Tempo - Error %s [%d]"), phttp->errorToString (http_code).c_str (), http_code);
  }

  // process received data
  if (is_ok)
  {
    // extract token from JSON
    deserializeJson (json_result, phttp->getString ().c_str ());

    // check array Size
    arr_day = json_result["tempo_like_calendars"]["values"].as<JsonArray>();
    size = arr_day.size ();

    // if tomorrow is not available
    if (size == 2)
    {
      rte_tempo_status.arr_day[RTE_DAY_TOMORROW]  = RTE_TEMPO_LEVEL_UNKNOWN;
      rte_tempo_status.arr_day[RTE_DAY_TODAY]     = GetCommandCode (str_text, sizeof (str_text), arr_day[0][F ("value")], kTeleinfoRteTempoStream);
      rte_tempo_status.arr_day[RTE_DAY_YESTERDAY] = GetCommandCode (str_text, sizeof (str_text), arr_day[1][F ("value")], kTeleinfoRteTempoStream);
    }

    // else tomorrow is available
    else if (size == 3)
    {
      rte_tempo_status.arr_day[RTE_DAY_TOMORROW]  = GetCommandCode (str_text, sizeof (str_text), arr_day[0][F ("value")], kTeleinfoRteTempoStream);
      rte_tempo_status.arr_day[RTE_DAY_TODAY]     = GetCommandCode (str_text, sizeof (str_text), arr_day[1][F ("value")], kTeleinfoRteTempoStream);
      rte_tempo_status.arr_day[RTE_DAY_YESTERDAY] = GetCommandCode (str_text, sizeof (str_text), arr_day[2][F ("value")], kTeleinfoRteTempoStream);
    }

    // set signal next update
    if (rte_tempo_status.arr_day[RTE_DAY_TOMORROW] == RTE_TEMPO_LEVEL_UNKNOWN) rte_update.time_tempo = LocalTime () + RTE_TEMPO_RENEW_UNKNOWN;
      else rte_update.time_tempo = LocalTime () + RTE_TEMPO_RENEW_KNOWN;

    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Tempo - Update done (%d-%d-%d)"), rte_tempo_status.arr_day[RTE_DAY_YESTERDAY], rte_tempo_status.arr_day[RTE_DAY_TODAY], rte_tempo_status.arr_day[RTE_DAY_TOMORROW]);
  }

  // end connexion
  if (phttp)
  {
    phttp->end ();
    delete phttp;
  }

  return is_ok;
}


/***********************************\
 *          Pointe management
\***********************************/

bool TeleinfoRtePointeIsEnabled ()
{
  return (rte_config.pointe_enable == 1);
}

uint8_t TeleinfoRtePointeGetGlobalLevel (const uint8_t day, const uint8_t slot)
{
  uint8_t level, result;

  // get level
  level = TeleinfoRtePointeGetLevel (day, slot);

  // convert level
  switch (level)
  {
    case RTE_POINTE_LEVEL_BLUE: result = RTE_LEVEL_BLUE;    break;
    case RTE_POINTE_LEVEL_RED:  result = RTE_LEVEL_RED;     break;
    default:                    result = RTE_LEVEL_UNKNOWN; break;
  }

  return result;
}

uint8_t TeleinfoRtePointeGetLevel (const uint8_t day, const uint8_t slot)
{
  uint8_t level = RTE_POINTE_LEVEL_UNKNOWN;

  // check parameter
  if ((day != RTE_DAY_TODAY) && (day != RTE_DAY_TOMORROW)) return level;

  // calculate level
  if ((slot >= 1) && (slot < 7)) level = RTE_POINTE_LEVEL_BLUE;
    else if ((day == RTE_DAY_TODAY) && (slot < 1)) level = rte_pointe_status.arr_day[RTE_DAY_YESTERDAY];
    else if (day == RTE_DAY_TODAY) level = rte_pointe_status.arr_day[RTE_DAY_TODAY];
    else if ((day == RTE_DAY_TOMORROW) && (slot < 1)) level = rte_pointe_status.arr_day[RTE_DAY_TODAY];
    else level = rte_pointe_status.arr_day[RTE_DAY_TOMORROW];

  return level;
}

// start of pointe period reception
bool TeleinfoRtePointeStream ()
{
  bool     is_ok;
  int      http_code;
  uint16_t size;
  char     str_text[256];
  JsonArray           arr_day;
  DynamicJsonDocument json_result(2048);
  HTTPClientLight*    phttp;

  // check token
  is_ok = (strlen (rte_update.str_token) > 0);
  if (!is_ok)
  {
    AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Pointe - Token missing"));
    return false;
  }

  // create connexion
  phttp = new HTTPClientLight ();

  // start connexion
  is_ok = (phttp != nullptr);
  if (is_ok) 
  {
    // connexion
    sprintf (str_text, "%s", RTE_URL_POINTE_DATA);
    is_ok = phttp->begin (str_text);

    // connexion report
    if (is_ok) AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Pointe - Connexion done [%s]"), str_text);
    else AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Pointe - Connexion refused [%s]"), str_text);
  }
  else AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Pointe - Connexion creation failed"));

  // set headers
  if (is_ok)
  {
    // set authorisation
    strcpy_P (str_text, PSTR ("Bearer "));
    strlcat (str_text, rte_update.str_token, sizeof (str_text) - 1);

    // set timeout and headers
    phttp->setTimeout (RTE_HTTP_TIMEOUT);
    phttp->addHeader (F ("Authorization"), str_text, false, true);
    phttp->addHeader (F ("Accept"), F ("application/json"), false, true);

    // connexion
    http_code = phttp->GET ();
    yield ();

    // check if connexion failed
    is_ok = ((http_code == HTTP_CODE_OK) || (http_code == HTTP_CODE_MOVED_PERMANENTLY));
    if (is_ok) AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Pointe - Success [%d]"), http_code);
    else AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Pointe - Error %s [%d]"), phttp->errorToString (http_code).c_str (), http_code);
  }

  // process received data
  if (is_ok)
  {
    // extract token from JSON
    deserializeJson (json_result, phttp->getString ().c_str ());

    // check array size
    arr_day = json_result[F ("signals")][0][F ("signaled_dates")].as<JsonArray>();
    size = arr_day.size ();

    // if both days are available
    if (size == 2)
    {
      // set tomorrow
      rte_pointe_status.arr_day[RTE_DAY_TOMORROW] = 1 + arr_day[0][F ("aoe_signals")].as<unsigned int>();
      if (rte_pointe_status.arr_day[RTE_DAY_TOMORROW] > RTE_POINTE_LEVEL_RED) rte_pointe_status.arr_day[RTE_DAY_TOMORROW] = RTE_POINTE_LEVEL_RED;

      // set today
      rte_pointe_status.arr_day[RTE_DAY_TODAY] = 1 + arr_day[1][F ("aoe_signals")].as<unsigned int>();
      if (rte_pointe_status.arr_day[RTE_DAY_TODAY] > RTE_POINTE_LEVEL_RED) rte_pointe_status.arr_day[RTE_DAY_TODAY] = RTE_POINTE_LEVEL_RED;
    }

    // set signal next update
    rte_update.time_pointe = LocalTime () + RTE_POINTE_RENEW;

    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Pointe - Update done (%d-%d-%d)"), rte_pointe_status.arr_day[RTE_DAY_YESTERDAY], rte_pointe_status.arr_day[RTE_DAY_TODAY], rte_pointe_status.arr_day[RTE_DAY_TOMORROW]);
  }

  // end connexion
  if (phttp)
  {
    phttp->end ();
    delete phttp;
  }

  return is_ok;
}

// called every day at midnight
void TeleinfoRteMidnight ()
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
    strcpy (rte_ecowatt_status.arr_day[index].str_day_of_week, rte_ecowatt_status.arr_day[index + 1].str_day_of_week);
    rte_ecowatt_status.arr_day[index].day_of_month = rte_ecowatt_status.arr_day[index + 1].day_of_month;
    rte_ecowatt_status.arr_day[index].month        = rte_ecowatt_status.arr_day[index + 1].month;
    rte_ecowatt_status.arr_day[index].dvalue       = rte_ecowatt_status.arr_day[index + 1].dvalue;
    for (slot = 0; slot < 24; slot ++) rte_ecowatt_status.arr_day[index].arr_hvalue[slot] = rte_ecowatt_status.arr_day[index + 1].arr_hvalue[slot];
  }

  // init date, global status and slots for day+3
  index = RTE_ECOWATT_DAY_MAX - 1;
  strcpy (rte_ecowatt_status.arr_day[index].str_day_of_week, "");
  rte_ecowatt_status.arr_day[index].day_of_month = 0;
  rte_ecowatt_status.arr_day[index].month        = 0;
  rte_ecowatt_status.arr_day[index].dvalue       = RTE_ECOWATT_LEVEL_NORMAL;
  for (slot = 0; slot < 24; slot ++) rte_ecowatt_status.arr_day[index].arr_hvalue[slot] = RTE_ECOWATT_LEVEL_NORMAL;

  // publish data change
  if (rte_config.ecowatt_enable) rte_ecowatt_status.json_publish = 1;

  // handle tempo data rotation
  // --------------------------
  
  // rotate days
  rte_tempo_status.arr_day[RTE_DAY_YESTERDAY] = rte_tempo_status.arr_day[RTE_DAY_TODAY];
  rte_tempo_status.arr_day[RTE_DAY_TODAY]     = rte_tempo_status.arr_day[RTE_DAY_TOMORROW];
  rte_tempo_status.arr_day[RTE_DAY_TOMORROW]  = RTE_TEMPO_LEVEL_UNKNOWN;

  // publish data change
  if (rte_config.tempo_enable) rte_tempo_status.json_publish = 1;

  // handle pointe data rotation
  // ---------------------------
  
  // rotate days
  rte_pointe_status.arr_day[RTE_DAY_YESTERDAY] = rte_pointe_status.arr_day[RTE_DAY_TODAY];
  rte_pointe_status.arr_day[RTE_DAY_TODAY]     = rte_pointe_status.arr_day[RTE_DAY_TOMORROW];
  rte_pointe_status.arr_day[RTE_DAY_TOMORROW]  = RTE_POINTE_LEVEL_UNKNOWN;

  // publish data change
  if (rte_config.pointe_enable) rte_pointe_status.json_publish = 1;
}

/***********************************************************\
 *                      Callback
\***********************************************************/

// init main status
void TeleinfoRteInit ()
{
  uint8_t index, slot;

  // init data*
  strcpy (rte_update.str_token, "");

  // load configuration file
  TeleinfoRteLoadConfig ();

  // tempo initialisation
  for (index = 0; index < RTE_DAY_MAX; index ++) rte_tempo_status.arr_day[index] = RTE_TEMPO_LEVEL_UNKNOWN;

  // pointe initialisation
  for (index = 0; index < RTE_DAY_MAX; index ++) rte_pointe_status.arr_day[index] = RTE_POINTE_LEVEL_UNKNOWN;

  // ecowatt initialisation
  for (index = 0; index < RTE_ECOWATT_DAY_MAX; index ++)
  {
    strcpy (rte_ecowatt_status.arr_day[index].str_day_of_week, "");
    rte_ecowatt_status.arr_day[index].day_of_month = 0;
    rte_ecowatt_status.arr_day[index].month        = 0;
    rte_ecowatt_status.arr_day[index].dvalue       = RTE_ECOWATT_LEVEL_NORMAL;
    for (slot = 0; slot < 24; slot ++) rte_ecowatt_status.arr_day[index].arr_hvalue[slot] = RTE_ECOWATT_LEVEL_NORMAL;
  }

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Run rte to get help on RTE modules"));
}

// called every second
void TeleinfoRteEverySecond ()
{
  bool     result;
  uint8_t  period, level;
  uint32_t time_now;

  // check parameters
  if (!RtcTime.valid) return;
  if (TasmotaGlobal.uptime < RTE_TOKEN_RETRY) return;
  if (TasmotaGlobal.global_state.network_down) return;

  // check for midnight switch
  if (RtcTime.hour < rte_update.hour) TeleinfoRteMidnight ();
  rte_update.hour = RtcTime.hour;

  // if first update, ask for token update
  time_now = LocalTime ();
  if ((rte_update.time_token == UINT32_MAX) && (rte_config.tempo_enable || rte_config.pointe_enable || rte_config.ecowatt_enable)) rte_update.time_token = time_now;

  // if needed, plan initial ecowatt and tempo updates
  if (rte_config.tempo_enable && (rte_update.time_tempo == UINT32_MAX)) rte_update.time_tempo = time_now + 5;
  if (rte_config.pointe_enable && (rte_update.time_pointe == UINT32_MAX)) rte_update.time_pointe = time_now + 10;
  if (rte_config.ecowatt_enable && (rte_update.time_ecowatt == UINT32_MAX)) rte_update.time_ecowatt = time_now + 15;

  //   Stream recetion
  // --------------------

  switch (rte_update.step)
  {
    // no current stream operation
    case RTE_UPDATE_NONE:
      // priority 1 : check for need of token update
      if (rte_update.time_token <= time_now) rte_update.step = RTE_UPDATE_TOKEN;

      // else, priority 2 : check for need of tempo update
      else if (rte_config.tempo_enable && (rte_update.time_tempo <= time_now)) rte_update.step = RTE_UPDATE_TEMPO;

      // else, priority 3 : check for need of pointe update
      else if (rte_config.pointe_enable && (rte_update.time_pointe <= time_now)) rte_update.step = RTE_UPDATE_POINTE;

      // else, priority 4 : check for need of ecowatt update
      else if (rte_config.ecowatt_enable && (rte_update.time_ecowatt <= time_now)) rte_update.step = RTE_UPDATE_ECOWATT;
      break;

    // begin token reception
    case RTE_UPDATE_TOKEN:
      // init token update
      result = TeleinfoRteStreamToken ();
      rte_update.step = RTE_UPDATE_NONE; 
      
      // if failure, plan retry
      if (!result)
      {
        rte_update.time_token   = LocalTime () + RTE_TOKEN_RETRY;
        rte_update.time_tempo   = max (rte_update.time_tempo, rte_update.time_token + 5);
        rte_update.time_pointe  = max (rte_update.time_pointe, rte_update.time_token + 10);
        rte_update.time_ecowatt = max (rte_update.time_ecowatt, rte_update.time_token + 15);
      }
      break;

    // init ecowatt reception
    case RTE_UPDATE_ECOWATT:
      // init ecowatt update
      result = TeleinfoRteEcowattStream ();
      rte_update.step = RTE_UPDATE_NONE; 
      
      // if failure, plan retry
      if (!result) rte_update.time_ecowatt = LocalTime () + RTE_ECOWATT_RETRY;
      break;


    // init tempo reception
    case RTE_UPDATE_TEMPO:
      // init tempo update
      result = TeleinfoRteTempoStream ();
      rte_update.step = RTE_UPDATE_NONE; 
      
      // if failure, plan retry
      if (!result) rte_update.time_tempo = LocalTime () + RTE_TEMPO_RETRY;
      break;
      
    // init pointe period reception
    case RTE_UPDATE_POINTE:
      // init tempo update
      result = TeleinfoRtePointeStream ();
      rte_update.step = RTE_UPDATE_NONE; 
      
      // if failure, plan retry
      if (!result) rte_update.time_tempo = LocalTime () + RTE_POINTE_RETRY;
      break;
  }

  //   Tempo status
  // -----------------
  if (rte_config.tempo_enable)
  {
    // get current tempo period and color
    period = TeleinfoRteTempoGetPeriod (RtcTime.hour);
    level  = TeleinfoRteTempoGetLevel (RTE_DAY_TODAY, RtcTime.hour);

    // check for tempo JSON update
    if (rte_tempo_status.json_time == UINT32_MAX) rte_tempo_status.json_publish = 1;
      else if (rte_tempo_status.json_time < time_now) rte_tempo_status.json_publish = 1;
      else if (rte_tempo_status.last_period != period) rte_tempo_status.json_publish = 1;
      else if (rte_tempo_status.last_level != level) rte_tempo_status.json_publish = 1;

    // update tempo period and color
    rte_tempo_status.last_period = period;
    rte_tempo_status.last_level  = level;
  }

  //   Pointe status
  // -----------------
  if (rte_config.pointe_enable)
  {
    // check for tempo JSON update
    if (rte_pointe_status.json_time == UINT32_MAX) rte_pointe_status.json_publish = 1;
      else if (rte_pointe_status.json_time < time_now) rte_pointe_status.json_publish = 1;
      else if (rte_pointe_status.last_level != rte_pointe_status.arr_day[RTE_DAY_TOMORROW]) rte_pointe_status.json_publish = 1;

    // update pointe color
    rte_pointe_status.last_level = rte_pointe_status.arr_day[RTE_DAY_TOMORROW];
  }

  //   Ecowatt status
  // -------------------
  if (rte_config.ecowatt_enable)
  {
    // check for ecowatt JSON update
    if (rte_ecowatt_status.json_time == UINT32_MAX) rte_ecowatt_status.json_publish = 1;
      else if (rte_ecowatt_status.json_time < time_now) rte_ecowatt_status.json_publish = 1;
  }

  // if needed, publish TEMPO, POINTE or ECOWATT
  if (rte_tempo_status.json_publish) TeleinfoRtePublishTempoJson ();
  else if (rte_pointe_status.json_publish) TeleinfoRtePublishPointeJson ();
  else if (rte_ecowatt_status.json_publish) TeleinfoRtePublishEcowattJson ();
}

// Handle MQTT teleperiod
void TeleinfoRtePublishSensor ()
{
  int   index;
  int   length = 0;
  char  last   = 0;
  const char *pstr_response;
  char  str_value[8];

  // tempo
  if (rte_config.tempo_enable)
  {
    // check need of ',' according to previous data
    pstr_response = ResponseData ();
    if (pstr_response != nullptr) length = strlen (pstr_response) - 1;
    if (length >= 0) last = pstr_response[length];
    if ((last != 0) && (last != '{') && (last != ',')) ResponseAppend_P (PSTR (","));

    ResponseAppend_P (PSTR ("\"TEMPO\":{"));

    // tempo today
    index = TeleinfoRteTempoGetGlobalLevel (RTE_DAY_TODAY, 12);
    GetTextIndexed (str_value, sizeof (str_value), index, kRteTempoJSON);
    ResponseAppend_P (PSTR ("\"tday\":\"%s\""), str_value);

    // tempo tomorrow
    index = TeleinfoRteTempoGetGlobalLevel (RTE_DAY_TOMORROW, 12);
    GetTextIndexed (str_value, sizeof (str_value), index, kRteTempoJSON);
    ResponseAppend_P (PSTR (",\"tmrw\":\"%s\""), str_value);

    ResponseJsonEnd ();
  }

  // pointe
  if (rte_config.pointe_enable)
  {
    // check need of ',' according to previous data
    pstr_response = ResponseData ();
    if (pstr_response != nullptr) length = strlen (pstr_response) - 1;
    if (length >= 0) last = pstr_response[length];
    if ((last != 0) && (last != '{') && (last != ',')) ResponseAppend_P (PSTR (","));

    ResponseAppend_P (PSTR ("\"POINTE\":{"));

    // pointe today
    index = TeleinfoRtePointeGetGlobalLevel (RTE_DAY_TODAY, 12);
    GetTextIndexed (str_value, sizeof (str_value), index, kRtePointeJSON);
    ResponseAppend_P (PSTR ("\"tday\":\"%s\""), str_value);

    // pointe tomorrow
    index = TeleinfoRtePointeGetGlobalLevel (RTE_DAY_TOMORROW, 12);
    GetTextIndexed (str_value, sizeof (str_value), index, kRtePointeJSON);
    ResponseAppend_P (PSTR (",\"tmrw\":\"%s\""), str_value);

    ResponseJsonEnd ();
  }
}

// Handle MQTT teleperiod
void TeleinfoRteTeleperiod ()
{
  // check parameter
  if (TasmotaGlobal.tele_period > 0) return;

  // ecowatt, ask to publish full topic
  if (rte_config.ecowatt_enable) rte_ecowatt_status.json_publish = true;

  // tempo, ask to publish full topic
  if (rte_config.tempo_enable) rte_tempo_status.json_publish = true;

  // pointe, ask to publish full topic
  if (rte_config.pointe_enable) rte_pointe_status.json_publish = true;
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// append presence forecast to main page
void TeleinfoRteWebSensor ()
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

  WSContentSend_P (PSTR ("<style>table hr{display:none;}</style>\n"));

  // tempo display
  // -------------
  if (rte_config.tempo_enable && rte_config.tempo_display)
  {
    // start of tempo data
    WSContentSend_P (PSTR ("<div style='font-size:10px;text-align:center;padding:2px 6px 6px 6px;margin-bottom:5px;background:#333333;border-radius:12px;'>\n"));

    // title display
    WSContentSend_P (PSTR ("<div style='display:flex;margin:2px 0px 0px 0px;padding:0px;font-size:16px;'>\n"));
    WSContentSend_P (PSTR ("<div style='width:25%%;padding:0px;text-align:left;font-weight:bold;'>Tempo</div>\n"));
    WSContentSend_P (PSTR ("<div style='width:13%%;padding:4px 0px;text-align:left;font-size:10px;font-style:italic;'>v1</div>\n"));
    WSContentSend_P (PSTR ("<div style='width:60%%;padding:4px 0px 8px 0px;text-align:right;font-size:10px;font-style:italic;'>"));

    // set display message
    if (strlen (rte_config.str_private_key) == 0)   WSContentSend_P (PSTR ("clé privée RTE manquante"));
    else if (rte_update.time_token == UINT32_MAX)   WSContentSend_P (PSTR ("initialisation"));
    else if (rte_update.time_tempo < time_now + 10) WSContentSend_P (PSTR ("maj imminente"));
    else if (rte_update.time_tempo != UINT32_MAX)   WSContentSend_P (PSTR ("maj dans %d mn"), 1 + (rte_update.time_tempo - time_now) / 60);

    // end of title display
    WSContentSend_P (PSTR ("</div>\n"));
    WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div>\n"));
    WSContentSend_P (PSTR ("</div>\n"));

    // if data are available
    if (rte_tempo_status.arr_day[RTE_DAY_TODAY] != RTE_TEMPO_LEVEL_UNKNOWN)
    {
      // ecowatt styles
      WSContentSend_P (PSTR ("<style>\n"));
      WSContentSend_P (PSTR (".tempod{display:flex;margin:0px;padding:1px;height:14px;}\n"));
      WSContentSend_P (PSTR (".tempoh{width:20.8%%;padding:0px;text-align:left;}\n"));
      for (index = 0; index < RTE_TEMPO_LEVEL_MAX; index ++)
      {
        GetTextIndexed (str_color, sizeof (str_color), index, kRteTempoColor);
        WSContentSend_P (PSTR (".tempo%u{width:3.3%%;padding:1px 0px;background-color:%s;}\n"), index, str_color);
      }
      WSContentSend_P (PSTR ("</style>\n"));

      // loop thru days
      for (index = RTE_DAY_TODAY; index < RTE_DAY_MAX; index ++)
      {
        WSContentSend_P (PSTR ("<div class='tempod'>\n"));

        // display day
        if (index == RTE_DAY_TODAY) strcpy (str_text, "Aujourd'hui"); else strcpy (str_text, "Demain");
        WSContentSend_P (PSTR ("<div class='tempoh'>%s</div>\n"), str_text);

        // display hourly slots
        for (slot = 0; slot < 24; slot ++)
        {
          // calculate color of current slot
          level = TeleinfoRteTempoGetLevel (index, slot);
          WSContentSend_P (PSTR ("<div class='tempo%u' style='"), level);

          // set opacity for HC
          if ((slot < 6) || (slot >= 22)) WSContentSend_P (PSTR ("opacity:75%%;"));

          // first and last segment specificities
          if (slot == 0) WSContentSend_P (PSTR ("border-top-left-radius:6px;border-bottom-left-radius:6px;"));
            else if (slot == 23) WSContentSend_P (PSTR ("border-top-right-radius:6px;border-bottom-right-radius:6px;"));

          // current hour dot and hourly segment end
          if ((index == RTE_DAY_TODAY) && (slot == dst_now.hour))
          {
            GetTextIndexed (str_text, sizeof (str_text), level, kRteTempoDot);  
            WSContentSend_P (PSTR ("font-size:9px;'>%s</div>\n"), str_text);
          }
          else WSContentSend_P (PSTR ("'></div>\n"));
        }
        WSContentSend_P (PSTR ("</div>\n"));
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
    WSContentSend_P (PSTR ("<div style='width:13%%;padding:4px 0px;text-align:left;font-size:10px;font-style:italic;'>v2</div>\n"));
    WSContentSend_P (PSTR ("<div style='width:60%%;padding:4px 0px 8px 0px;text-align:right;font-size:10px;font-style:italic;'>"));

    // set display message
    if (strlen (rte_config.str_private_key) == 0)    WSContentSend_P (PSTR ("clé privée RTE manquante"));
    else if (rte_update.time_token == UINT32_MAX)    WSContentSend_P (PSTR ("initialisation"));
    else if (rte_update.time_pointe < time_now + 10) WSContentSend_P (PSTR ("maj imminente"));
    else if (rte_update.time_pointe != UINT32_MAX)   WSContentSend_P (PSTR ("maj dans %d mn"), 1 + (rte_update.time_pointe - time_now) / 60);

    // end of title display
    WSContentSend_P (PSTR ("</div>\n"));
    WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div>\n"));
    WSContentSend_P (PSTR ("</div>\n"));

    // if data are available
    if (rte_pointe_status.arr_day[RTE_DAY_TODAY] != RTE_POINTE_LEVEL_UNKNOWN)
    {
      // pointe styles
      WSContentSend_P (PSTR ("<style>\n"));
      WSContentSend_P (PSTR (".pointed{display:flex;margin:0px;padding:1px;height:14px;}\n"));
      WSContentSend_P (PSTR (".pointeh{width:20.8%%;padding:0px;text-align:left;}\n"));
      for (index = 0; index < RTE_POINTE_LEVEL_MAX; index ++)
      {
        GetTextIndexed (str_color, sizeof (str_color), index, kRtePointeColor);  
        WSContentSend_P (PSTR (".pointe%u{width:3.3%%;padding:1px 0px;background-color:%s;}\n"), index, str_color);
      }
      WSContentSend_P (PSTR ("</style>\n"));

      // loop thru days
      for (index = RTE_DAY_TODAY; index < RTE_DAY_MAX; index ++)
      {
        WSContentSend_P (PSTR ("<div class='pointed'>\n"));

        // display day
        if (index == RTE_DAY_TODAY) strcpy (str_text, "Aujourd'hui"); else strcpy (str_text, "Demain");
        WSContentSend_P (PSTR ("<div class='pointeh'>%s</div>\n"), str_text);

        // display hourly slots
        for (slot = 0; slot < 24; slot ++)
        {
          // calculate color of current slot
          level = TeleinfoRtePointeGetLevel (index, slot);
          WSContentSend_P (PSTR ("<div class='pointe%u' style='"), level);

          // first and last segment specificities
          if (slot == 0) WSContentSend_P (PSTR ("border-top-left-radius:6px;border-bottom-left-radius:6px;"));
            else if (slot == 23) WSContentSend_P (PSTR ("border-top-right-radius:6px;border-bottom-right-radius:6px;"));

          // segment end
          if ((index == RTE_DAY_TODAY) && (slot == dst_now.hour)) WSContentSend_P (PSTR ("font-size:9px;'>⚪</div>\n"));
            else WSContentSend_P (PSTR ("'></div>\n"));
        }
        WSContentSend_P (PSTR ("</div>\n"));
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
    WSContentSend_P (PSTR ("<div style='width:13%%;padding:4px 0px;text-align:left;font-size:10px;font-style:italic;'>v5</div>\n"));
    WSContentSend_P (PSTR ("<div style='width:60%%;padding:4px 0px 8px 0px;text-align:right;font-size:10px;font-style:italic;'>"));

    // set display message
    if (strlen (rte_config.str_private_key) == 0)     WSContentSend_P (PSTR ("clé privée RTE manquante"));
    else if (rte_update.time_token == UINT32_MAX)     WSContentSend_P (PSTR ("initialisation"));
    else if (rte_update.time_ecowatt < time_now + 10) WSContentSend_P (PSTR ("maj imminente"));
    else if (rte_update.time_ecowatt != UINT32_MAX)   WSContentSend_P (PSTR ("maj dans %d mn"), 1 + (rte_update.time_ecowatt - time_now) / 60);

    // end of title display
    WSContentSend_P (PSTR ("</div>\n"));
    WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div>\n"));
    WSContentSend_P (PSTR ("</div>\n"));

    // if ecowatt signal has been received previously
    if (rte_ecowatt_status.arr_day[0].month != 0)
    {
      // ecowatt styles
      WSContentSend_P (PSTR ("<style>\n"));
      WSContentSend_P (PSTR (".ecod{display:flex;margin:0px;padding:1px;height:14px;}\n"));
      WSContentSend_P (PSTR (".ecoh{width:20.8%%;padding:0px;text-align:left;}\n"));
      for (index = 0; index < RTE_ECOWATT_LEVEL_MAX; index ++)
      {
        GetTextIndexed (str_color, sizeof (str_color), index, kRteEcowattLevelColor);
        WSContentSend_P (PSTR (".eco%u{width:3.3%%;padding:1px 0px;background-color:%s;}\n"), index, str_color);
      }
      WSContentSend_P (PSTR ("</style>\n"));

      // loop thru days
      for (index = 0; index < RTE_ECOWATT_DAY_MAX; index ++)
      {
        WSContentSend_P (PSTR ("<div class='ecod'>\n"));

        // display day
        if (index == 0) WSContentSend_P (PSTR ("<div class='ecoh'>%s</div>\n"), "Aujourd'hui");
          else if (index == 1) WSContentSend_P (PSTR ("<div class='ecoh'>%s</div>\n"), "Demain");
          else if (strlen (rte_ecowatt_status.arr_day[index].str_day_of_week) == 0) WSContentSend_P (PSTR ("<div class='ecoh'>%s</div>\n"), "");
          else WSContentSend_P (PSTR ("<div style='width:8.4%%;padding:0px;text-align:left;'>%s</div><div style='width:12.4%%;padding:0px;text-align:left;'>%02u/%02u</div>\n"), rte_ecowatt_status.arr_day[index].str_day_of_week, rte_ecowatt_status.arr_day[index].day_of_month, rte_ecowatt_status.arr_day[index].month);

        // display hourly slots
        for (slot = 0; slot < 24; slot ++)
        {
          // segment beginning
          level = rte_ecowatt_status.arr_day[index].arr_hvalue[slot];
          WSContentSend_P (PSTR ("<div class='eco%u' style='"), level);

          // first and last segment specificities
          if (slot == 0) WSContentSend_P (PSTR ("border-top-left-radius:6px;border-bottom-left-radius:6px;"));
            else if (slot == 23) WSContentSend_P (PSTR ("border-top-right-radius:6px;border-bottom-right-radius:6px;"));

          // segment end
          if ((index == RTE_ECOWATT_DAY_TODAY) && (slot == dst_now.hour)) WSContentSend_P (PSTR ("font-size:9px;'>⚪</div>\n"));
            else WSContentSend_P (PSTR ("'></div>\n"));
        }
        WSContentSend_P (PSTR ("</div>\n"));
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
  }
}

#endif  // USE_WEBSERVER

/***************************************\
 *              Interface
\***************************************/

bool XdrvTeleinfoRte (const uint32_t function)
{
  bool result = false;

  // swtich according to context
  if (TeleinfoDriverIsPowered ()) switch (function)
  {
    case FUNC_COMMAND:
      result = DecodeCommand (kRteCommands, RteCommand);
      if (!result) result = DecodeCommand (kEcowattCommands, EcowattCommand);
      if (!result) result = DecodeCommand (kTempoCommands,   TempoCommand);
      if (!result) result = DecodeCommand (kPointeCommands,  PointeCommand);
      break;

    case FUNC_INIT:
      TeleinfoRteInit ();
      break;

    case FUNC_EVERY_SECOND:
      TeleinfoRteEverySecond ();
      break;

    case FUNC_JSON_APPEND:
      TeleinfoRteTeleperiod ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      TeleinfoRteWebSensor ();
      break;
#endif    // USE_WEBSERVER
  }

  return result;
}

#endif    // USE_TELEINFO
#endif    // ESP32
