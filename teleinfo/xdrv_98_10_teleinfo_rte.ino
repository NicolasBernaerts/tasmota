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
    04/12/2023 v3.0 - Rename ecowatt.cfg to rte.cfg
                      Add Tempo calendar
    07/12/2023 v3.1 - Handle Ecowatt v4 and v5
    19/12/2023 v3.2 - Add Pointe calendar
    05/02/2024 v3.3 - Switch to Ecowatt v5 only
    25/02/2024 v3.4 - Publish topics sequentially to avoid reception errors
    10/11/2024 v3.5 - Rework around HTTPs connexions
    30/04/2025 v3.6 - Set start delay to protect single core ESP boot process 
    10/07/2025 v4.0 - Refactoring based on Tasmota 15
    16/08/2025 v4.1 - Minimizing of ECOWATT hourly status
    07/09/2025 v4.2 - Set https timeout to 3 sec
                      Limit publications to 1 per sec.
    19/09/2025 v4.3 - Hide and Show with click on main page display
    26/10/2025 v4.4 - Adapt to FUNC_JSON_APPEND call every second
    19/11/2025 v4.5 - Add tomorrow's color on main page if minimized
    16/12/2025 v5.0 - Add OpenDPE Tempo forecast
                      Save data to teleinfo-rte.dat instead of rte.cfg to save context
    02/01/2026 v5.1 - Handle new OpenDPE JSON format
    08/01/2026 v5.2 - Add RTE Tempo Light management
    31/01/2026 v5.3 - Correct bug in midnight shift
                      
  This module connects to french RTE server to retrieve Ecowatt, Tempo and Pointe electricity production forecast.

  It publishes ECOWATT prediction under following MQTT topic :
  
    .../tele/RTE
    {"Time":"2022-10-10T23:51:09","ECOWATT":{"dval":2,"hour":14,"now":1,"next":2,
      "day0":{"jour":"06-10-2022","dval":1,"hour":"11110001111..."},
      "day1":{"jour":"07-10-2022","dval":2,"hour":"10001110001111..."},
      "day2":{"jour":"08-10-2022","dval":3,"hour":"00011110001111..."},
      "day3":{"jour":"09-10-2022","dval":2,"hour":"1111222222..."}}}

  It publishes TEMPO production days under following MQTT topic :

    .../tele/RTE
    {"Time":"2022-10-10T23:51:09","TEMPO":{"lv":1,"hp":0,"label":"bleu","icon":"🔵","yesterday":1,"today":1,"tomorrow":2}}


  It publishes POINTE production days under following MQTT topic :

    .../tele/RTE
    {"Time":"2022-10-10T23:51:09","POINTE":{"lv":1,"label":"bleu","icon":"🔵","today":1,"tomorrow":2}}


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
#ifdef USE_TELEINFO_RTE

#include <ArduinoJson.h>

// global constant
#define RTE_HTTPS_TIMEOUT               3000       // HTTPs connexion timeout (ms)
#define RTE_TOKEN_DELAY_RETRY           30         // token retry timeout (sec.)
#define RTE_DELAY_QUERY                 10         // initial query delay
#define RTE_FILE_VERSION_CFG            1          // configuration version
#define RTE_FILE_VERSION_DATA           2          // data version

// tempo open dpe constant
#define RTE_OPENDPE_DELAY_RETRY         1800       // Open DPE retry timeout (30mn)

// tempo light constant
#define RTE_TEMPOLIGHT_DELAY_RETRY      900        // Tempo Light retry timeout (15mn)
#define RTE_TEMPOLIGHT_KNOWN            101        // probability level of known tempo light day

// tempo constant
#define RTE_TEMPO_UPDATE_JSON           900        // publish JSON every 15 mn
#define RTE_TEMPO_DELAY_INITIAL         15         // tempo initial delay (15sec)
#define RTE_TEMPO_DELAY_RETRY           900        // tempo retry timeout (15mn)
#define RTE_TEMPO_DELAY_KNOWN           21600      // tempo renew period when tomorrow is known (6h)
#define RTE_TEMPO_DELAY_UNKNOWN         1800       // tempo renew period when tomorrow is unknown (30mn)
#define RTE_TEMPO_KNOWN                 102        // probability level of known tempo day

// ecowatt constant
#define RTE_ECOWATT_UPDATE_JSON         1800       // publish JSON every 30 mn
#define RTE_ECOWATT_DELAY_INITIAL       15         // ecowatt initial delay (15sec)
#define RTE_ECOWATT_DELAY_RETRY         900        // ecowatt retry timeout (15mn)
#define RTE_ECOWATT_DELAY_RENEW         21600      // ecowatt renew period (6h)

// pointe constant
#define RTE_POINTE_UPDATE_JSON          900        // publish JSON every 15 mn
#define RTE_POINTE_DELAY_INITIAL        15         // pointe initial delay (15sec)
#define RTE_POINTE_DELAY_RETRY          900        // pointe retry timeout (15mn)
#define RTE_POINTE_DELAY_RENEW          21600      // pointe renew period when tomorrow is known (6h)



// configuration file
const char PSTR_RTE_CFG[]            PROGMEM = "/rte.cfg";
const char PSTR_RTE_OLD[]            PROGMEM = "/rte.old";
const char PSTR_RTE_DATA[]           PROGMEM = "/teleinfo-rte.dat";

// commands
#define D_CMND_RTE                  "rte"
#define D_CMND_RTE_ECOWATT          "ecowatt"
#define D_CMND_RTE_TEMPO            "tempo"
#define D_CMND_RTE_POINTE           "pointe"
#define D_CMND_RTE_OPENDPE          "opendpe"
#define D_CMND_RTE_KEY              "key"
#define D_CMND_RTE_TOKEN            "token"
#define D_CMND_RTE_DISPLAY          "display"
#define D_CMND_RTE_SANDBOX          "sandbox"
#define D_CMND_RTE_UPDATE           "update"
#define D_CMND_RTE_PUBLISH          "publish"

// URL
static const char RTE_URL_OAUTH2[]           PROGMEM = "https://digital.iservices.rte-france.com/token/oauth/";
static const char RTE_URL_ECOWATT_DATA[]     PROGMEM = "https://digital.iservices.rte-france.com/open_api/ecowatt/v5/signals";
static const char RTE_URL_ECOWATT_SANDBOX[]  PROGMEM = "https://digital.iservices.rte-france.com/open_api/ecowatt/v5/sandbox/signals";
static const char RTE_URL_TEMPO_DATA[]       PROGMEM = "https://digital.iservices.rte-france.com/open_api/tempo_like_supply_contract/v1/tempo_like_calendars";
static const char RTE_URL_POINTE_DATA[]      PROGMEM = "https://digital.iservices.rte-france.com/open_api/demand_response_signal/v2/signals";
static const char RTE_URL_TEMPOLIGHT_DATA[]  PROGMEM = "https://www.services-rte.com/cms/open_data/v1/tempoLight";
static const char RTE_URL_OPENDPE_DATA[]     PROGMEM = "https://open-dpe.fr/assets/tempo_days.json";

// Commands
static const char kRteCommands[]      PROGMEM = D_CMND_RTE "|"                   "|_" D_CMND_RTE_KEY  "|_" D_CMND_RTE_TOKEN  "|_" D_CMND_RTE_SANDBOX  "|_" D_CMND_RTE_PUBLISH     ;
void (* const RteCommand[])(void)     PROGMEM = {             &CmndTeleinfoRteHelp, &CmndTeleinfoRteKey, &CmndTeleinfoRteToken, &CmndTeleinfoRteSandbox, &CmndTeleinfoRtePublish };

static const char kEcowattCommands[]  PROGMEM =  D_CMND_RTE_ECOWATT "|"                         "|_"   D_CMND_RTE_DISPLAY    "|_"   D_CMND_RTE_UPDATE       ;
void (* const EcowattCommand[])(void) PROGMEM = {                      &CmndTeleinfoEcowattEnable, &CmndTeleinfoEcowattDisplay, &CmndTeleinfoEcowattUpdate };

static const char kTempoCommands[]    PROGMEM = D_CMND_RTE_TEMPO "|"                         "|_"   D_CMND_RTE_DISPLAY    "|_"   D_CMND_RTE_UPDATE     ;
void (* const TempoCommand[])(void)   PROGMEM = {                    &CmndTeleinfoTempoEnable , &CmndTeleinfoTempoDisplay  , &CmndTeleinfoTempoUpdate };

static const char kPointeCommands[]   PROGMEM = D_CMND_RTE_POINTE "|"                          "|_"   D_CMND_RTE_DISPLAY    "|_"    D_CMND_RTE_UPDATE     ;
void (* const PointeCommand[])(void)  PROGMEM = {                     &CmndTeleinfoPointeEnable , &CmndTeleinfoPointeDisplay , &CmndTeleinfoPointeUpdate };

// https stream status
enum RteHttpsUpdate { RTE_UPDATE_NONE, RTE_UPDATE_TOKEN, RTE_UPDATE_TEMPO, RTE_UPDATE_ECOWATT, RTE_UPDATE_POINTE, RTE_UPDATE_TEMPOLIGHT, RTE_UPDATE_OPENDPE, RTE_UPDATE_MAX};

// config
enum RteConfigKey { RTE_CONFIG_KEY, RTE_CONFIG_SANDBOX, RTE_CONFIG_ECOWATT, RTE_CONFIG_ECOWATT_DISPLAY, RTE_CONFIG_TEMPO, RTE_CONFIG_TEMPO_DISPLAY, RTE_CONFIG_POINTE, RTE_CONFIG_POINTE_DISPLAY, RTE_CONFIG_MAX };                   // configuration parameters
static const char kEcowattConfigKey[] PROGMEM = D_CMND_RTE_KEY "|" D_CMND_RTE_SANDBOX "|" D_CMND_RTE_ECOWATT "|" D_CMND_RTE_ECOWATT "_" D_CMND_RTE_DISPLAY "|" D_CMND_RTE_TEMPO "|" D_CMND_RTE_TEMPO "_" D_CMND_RTE_DISPLAY "|" D_CMND_RTE_POINTE "|" D_CMND_RTE_POINTE "_" D_CMND_RTE_DISPLAY;        // configuration keys

enum RteDay { RTE_DAY_YESTERDAY, RTE_DAY_TODAY, RTE_DAY_TOMORROW, RTE_DAY_PLUS2, RTE_DAY_PLUS3, RTE_DAY_PLUS4, RTE_DAY_PLUS5, RTE_DAY_PLUS6, RTE_DAY_PLUS7, RTE_DAY_MAX };
enum RtePeriod { RTE_PERIOD_PLEINE, RTE_PERIOD_CREUSE, RTE_PERIOD_MAX };
enum RteGlobalLevel { RTE_LEVEL_UNKNOWN, RTE_LEVEL_BLUE, RTE_LEVEL_WHITE, RTE_LEVEL_RED, RTE_LEVEL_MAX };

// tempo data
enum TempoLevel                                   { RTE_TEMPO_LEVEL_UNKNOWN, RTE_TEMPO_LEVEL_BLUE, RTE_TEMPO_LEVEL_WHITE, RTE_TEMPO_LEVEL_RED, RTE_TEMPO_LEVEL_MAX };
static const char kRteOpenDpeColorJSON[]  PROGMEM =           ""          "|"       "bleu"      "|"      "blanc"       "|"      "rouge"      ;
static const char kTeleinfoRteTempoJSON[] PROGMEM =                       "|"       "BLUE"      "|"      "WHITE"       "|"       "RED"       ;
static const char kRteTempoColor[]        PROGMEM =       "#252525"     "|"      "#059"     "|"      "#bbb"      "|"      "#800"     ;
static const char kRteTempoColorBorder[]  PROGMEM =        "#666"       "|"      "#08d"     "|"      "#ddd"      "|"      "#b00"     ;
static const char kRteTempoColorText[]    PROGMEM =         "white"       "|"       "white"     "|"      "black"       "|"      "white"      ;
static const char kRteTempoColorJSON[]    PROGMEM =        "Inconnu"      "|"       "Bleu"      "|"      "Blanc"       "|"      "Rouge"      ;
static const char kRteTempoIcon[]         PROGMEM =          "❔"         "|"        "🔵"       "|"       "⚪"         "|"       "🔴"        ;


// pointe data
enum PointeLevel                            { RTE_POINTE_LEVEL_UNKNOWN, RTE_POINTE_LEVEL_BLUE, RTE_POINTE_LEVEL_RED, RTE_POINTE_LEVEL_MAX };
static const char kRtePointeJSON[]  PROGMEM =          "Inconnu"     "|"       "Bleu"       "|"       "Rouge";
static const char kRtePointeColor[] PROGMEM =         "#252525"    "|"      "#06b"      "|"       "#800";
static const char kRtePointeIcon[]  PROGMEM =             "❔"       "|"        "🔵"        "|"         "🔴";

// ecowatt data
enum EcowattDays { RTE_ECOWATT_DAY_TODAY, RTE_ECOWATT_DAY_TOMORROW, RTE_ECOWATT_DAY_PLUS2, RTE_ECOWATT_DAY_DAYPLUS3, RTE_ECOWATT_DAY_MAX };
enum EcowattLevel                                 { RTE_ECOWATT_LEVEL_CARBONFREE, RTE_ECOWATT_LEVEL_NORMAL, RTE_ECOWATT_LEVEL_WARNING, RTE_ECOWATT_LEVEL_POWERCUT, RTE_ECOWATT_LEVEL_MAX };
static const char kRteEcowattJSON[]  PROGMEM =         "Décarboné"        "|"         "Normal"      "|"         "Alerte"       "|"         "Coupure";
static const char kRteEcowattColor[] PROGMEM =          "#0a0"          "|"         "#080"      "|"         "#F80"       "|"          "#800";

/***********************************************************\
 *                        Data
\***********************************************************/

// RTE initialisation
static struct {
  bool done = false;                                // flag to handle init process
} rte_init;

// RTE configuration
struct rte_api {
  bool enabled;                                     // flag to enable api access
  bool display;                                     // flag to display api period
};
static struct {
  bool    sandbox;                                  // flag to use RTE sandbox
  rte_api ecowatt;                                  // flags to enable ecowatt api
  rte_api tempo;                                    // flags to enable tempo api
  rte_api pointe;                                   // flags to enable pointe api
  char    str_private_key[128];                     // base 64 private key (provided on RTE site)
} rte_config;

// RTE server updates
static struct {
  bool     publish          = false;                // flag to publish JSON
  uint8_t  hour             = 0;                    // current hour
  uint8_t  step             = RTE_UPDATE_NONE;      // current reception step
  uint32_t time_token       = UINT32_MAX;           // timestamp of next token update
  uint32_t time_ecowatt     = UINT32_MAX;           // timestamp of next ecowatt update
  uint32_t time_pointe      = UINT32_MAX;           // timestamp of next pointe update
  uint32_t time_tempo       = UINT32_MAX;           // timestamp of next tempo update
  uint32_t time_opendpe     = UINT32_MAX;           // timestamp of next open dpe update
  uint32_t time_tempolight  = UINT32_MAX;           // timestamp of next tempo light update
  char     str_token[128];                          // current token value
} rte_update;

// Ecowatt status
struct rte_ecowatt_day {
  uint8_t dvalue;                                   // day global status
  uint8_t day_of_month;                             // dd
  uint8_t month;                                    // mm
  uint8_t arr_hvalue[24];                           // hourly slots
  char    str_day_of_week[4];                       // day of week (mon, Tue, ...)
};
static struct {
  uint32_t time_json = UINT32_MAX;                  // timestamp of next JSON update
  rte_ecowatt_day arr_day[RTE_ECOWATT_DAY_MAX];     // slots for today and next days
} rte_ecowatt_status;

// Tempo status
struct tempo_day {
  uint8_t level;                                    // open dpe level
  uint8_t proba;                                    // open dpe probability
};
struct tempo_left {
  uint8_t white;                                    // stock of white days
  uint8_t red;                                      // stock of red days
};
static struct {
  uint32_t   time_json = UINT32_MAX;                // timestamp of next JSON update
  tempo_day  arr_day[RTE_DAY_MAX];                  // tempo days
  tempo_left left;                                  // tempo days remaining
} rte_tempo_status;

// Pointe status
static struct {
  uint32_t time_json = UINT32_MAX;                  // timestamp of next JSON update
  int      arr_day[RTE_DAY_TOMORROW + 1];           // days status
} rte_pointe_status;

/************************************\
 *        RTE global commands
\************************************/

// RTE server help
void CmndTeleinfoRteHelp ()
{
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Liste des commandes API RTE"));
  AddLog (LOG_LEVEL_INFO, PSTR (" Commandes génériques :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - rte_key <key>         = clé privée RTE en base64"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - rte_token             = token RTE actuel"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - rte_sandbox <0/1>     = mode bac à sable (%u)"), rte_config.sandbox);
  AddLog (LOG_LEVEL_INFO, PSTR (" - rte_publish           = publication des données"));
  AddLog (LOG_LEVEL_INFO, PSTR (" Commandes API Tempo :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - tempo <0/1>           = activation des API Tempo (%u)"),  rte_config.tempo.enabled);
  AddLog (LOG_LEVEL_INFO, PSTR (" - tempo_display <0/1>   = données Tempo sur page d'accueil (%u)"), rte_config.tempo.display);
  AddLog (LOG_LEVEL_INFO, PSTR (" - tempo_update          = mise a jour des données Tempo"));
  AddLog (LOG_LEVEL_INFO, PSTR (" Commandes API Pointe :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - pointe <0/1>          = activation des API pointe (%u)"),  rte_config.pointe.enabled);
  AddLog (LOG_LEVEL_INFO, PSTR (" - pointe_display <0/1>  = données Pointe sur page d'accueil (%u)"), rte_config.pointe.display);
  AddLog (LOG_LEVEL_INFO, PSTR (" - pointe_update         = mise a jour des données Pointe"));
  AddLog (LOG_LEVEL_INFO, PSTR (" Commandes API Ecowatt :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - ecowatt <0/1>         = activation des API Ecowatt (%u)"),  rte_config.ecowatt.enabled);
  AddLog (LOG_LEVEL_INFO, PSTR (" - ecowatt_display <0/1> = données Ecowatt sur page d'accueil (%u)"), rte_config.ecowatt.display);
  AddLog (LOG_LEVEL_INFO, PSTR (" - ecowatt_update        = mise a jour des données Ecowatt"));
  ResponseCmndDone ();
}

// RTE base64 private key
void CmndTeleinfoRteKey ()
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.data_len < 128))
  {
    if (strcmp_P (XdrvMailbox.data, PSTR ("null")) == 0) rte_config.str_private_key[0] = 0;
      else strlcpy (rte_config.str_private_key, XdrvMailbox.data, sizeof (rte_config.str_private_key));
    TeleinfoRteSaveData ();
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
    rte_config.sandbox = (XdrvMailbox.payload != 0);
    TeleinfoRteSaveData ();

    // force update
    rte_update.time_ecowatt = LocalTime ();
    rte_update.time_tempo   = LocalTime ();
  }
  
  ResponseCmndNumber (rte_config.sandbox);
}

// publish data
void CmndTeleinfoRtePublish ()
{
  rte_update.publish = true;
  ResponseCmndDone ();
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
    rte_config.ecowatt.enabled = (XdrvMailbox.payload != 0);
    if (rte_config.ecowatt.enabled) rte_config.ecowatt.display = true;
    rte_update.time_ecowatt = LocalTime ();
    TeleinfoRteSaveData ();
  }

  ResponseCmndNumber (rte_config.ecowatt.enabled);
}

// Display ecowatt server
void CmndTeleinfoEcowattDisplay ()
{
  if (XdrvMailbox.data_len > 0)
  {
    rte_config.ecowatt.display = (XdrvMailbox.payload != 0);
    TeleinfoRteSaveData ();
  }

  ResponseCmndNumber (rte_config.ecowatt.display);
}

// Ecowatt forced update from RTE server
void CmndTeleinfoEcowattUpdate ()
{
  rte_update.time_ecowatt = LocalTime ();
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
    rte_config.tempo.enabled = (XdrvMailbox.payload != 0);
    if (rte_config.tempo.enabled) rte_config.tempo.display = true;
    TeleinfoRteSaveData ();
  }

  ResponseCmndNumber (rte_config.tempo.enabled);
}

// Display tempo server
void CmndTeleinfoTempoDisplay ()
{
  if (XdrvMailbox.data_len > 0)  {
    rte_config.tempo.display = (XdrvMailbox.payload != 0);
    TeleinfoRteSaveData ();
  }

  ResponseCmndNumber (rte_config.tempo.display);
}

// Tempo forced update from RTE server
void CmndTeleinfoTempoUpdate ()
{
  // reset data
  TeleinfoRteTempoReset ();

  // ask for immediate update
  rte_update.time_opendpe    = LocalTime ();
  rte_update.time_tempolight = LocalTime ();
  rte_update.time_tempo      = LocalTime ();

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
    rte_config.pointe.enabled = (XdrvMailbox.payload != 0);
    if (rte_config.pointe.enabled) rte_config.pointe.display = true;
    rte_update.time_pointe = LocalTime ();
    TeleinfoRteSaveData ();
  }

  ResponseCmndNumber (rte_config.pointe.enabled);
}

// Display pointe server
void CmndTeleinfoPointeDisplay ()
{
  if (XdrvMailbox.data_len > 0)
  {
    rte_config.pointe.display = (XdrvMailbox.payload != 0);
    TeleinfoRteSaveData ();
  }

  ResponseCmndNumber (rte_config.pointe.display);
}

// Pointe period forced update from RTE server
void CmndTeleinfoPointeUpdate ()
{
  rte_update.time_pointe = LocalTime ();
  ResponseCmndDone ();
}

/***********************************************************\
 *                      Configuration
\***********************************************************/

// load old configuration format
bool TeleinfoRteLoadOldConfig () 
{
  bool  result;
  int   index;
  char  str_text[16];
  char  str_line[128];
  char  str_cfg[24];
  char  str_old[24];
  char *pstr_key, *pstr_value;
  File  file;

  // load old file format
  strcpy_P (str_cfg, PSTR_RTE_CFG);
  result = ffsp->exists (str_cfg);
  if (result)
  {
    file = ffsp->open (str_cfg, "r");
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
            rte_config.sandbox = (atoi (pstr_value) != 0);
            break;
         case RTE_CONFIG_ECOWATT:
            rte_config.ecowatt.enabled = (atoi (pstr_value) != 0);
            break;
         case RTE_CONFIG_ECOWATT_DISPLAY:
            rte_config.ecowatt.display = (atoi (pstr_value) != 0);
            break;
         case RTE_CONFIG_TEMPO:
            rte_config.tempo.enabled = (atoi (pstr_value) != 0);
            break;
         case RTE_CONFIG_TEMPO_DISPLAY:
            rte_config.tempo.display = (atoi (pstr_value) != 0);
            break;
         case RTE_CONFIG_POINTE:
            rte_config.pointe.enabled = (atoi (pstr_value) != 0);
            break;
         case RTE_CONFIG_POINTE_DISPLAY:
            rte_config.pointe.display = (atoi (pstr_value) != 0);
            break;
        }
      }
    }

    // close file
    file.close ();

    // rename file
    strcpy_P (str_old, PSTR_RTE_OLD);
    ffsp->rename (str_cfg, str_old);
  }

  return result;
}

// load data file
void TeleinfoRteLoadData () 
{
  uint8_t  version;
  uint32_t time_save;
  char     str_filename[24];
  File     file;

  // if needed, load data from old config file
  if (TeleinfoRteLoadOldConfig ())
  {
    // save data
    TeleinfoRteSaveData ();

    // log
    AddLog (LOG_LEVEL_DEBUG, PSTR ("RTE: Config loaded from %s, renamed to %s"), PSTR_RTE_CFG, PSTR_RTE_OLD);
  }

  // else load data
  else
  {
    // if file exists
    strcpy_P (str_filename, PSTR_RTE_DATA);
    if (ffsp->exists (str_filename))
    {
      // open file in read mode
      file = ffsp->open (str_filename, "r");
      file.read ((uint8_t*)&version, sizeof (version));

      if (version == RTE_FILE_VERSION_CFG)
      // if version matches, load config
      {
        file.read ((uint8_t*)&rte_config, sizeof (rte_config));
        file.read ((uint8_t*)&version,    sizeof (version));

        // if version matches, load data
        if (version == RTE_FILE_VERSION_DATA)
        {
          file.read ((uint8_t*)&rte_update,         sizeof (rte_update));
          file.read ((uint8_t*)&rte_ecowatt_status, sizeof (rte_ecowatt_status));
          file.read ((uint8_t*)&rte_tempo_status,   sizeof (rte_tempo_status));
          file.read ((uint8_t*)&rte_pointe_status,  sizeof (rte_pointe_status));
        }
      }

      // close file
      file.close ();

      // log
      AddLog (LOG_LEVEL_DEBUG, PSTR ("RTE: Config loaded from %s, renamed to %s"), PSTR_RTE_CFG, PSTR_RTE_OLD);
    }
  }

  // if Open DPE forecast not known, reset previous planification
  if (rte_tempo_status.arr_day[RTE_DAY_PLUS2].level == UINT8_MAX) rte_update.time_opendpe = UINT32_MAX;
}

// save data file
void TeleinfoRteSaveData () 
{
  uint8_t version;
  char    str_filename[24];
  File    file;

  // open file in write mode
  strcpy_P (str_filename, PSTR_RTE_DATA);
  file = ffsp->open (str_filename, "w");
  if (file > 0)
  {
    // write configuration
    version  = RTE_FILE_VERSION_CFG;
    file.write ((uint8_t*)&version,    sizeof (version));
    file.write ((uint8_t*)&rte_config, sizeof (rte_config));

    // write 
    version  = RTE_FILE_VERSION_DATA;
    file.write ((uint8_t*)&version,            sizeof (version));
    file.write ((uint8_t*)&rte_update,         sizeof (rte_update));
    file.write ((uint8_t*)&rte_ecowatt_status, sizeof (rte_ecowatt_status));
    file.write ((uint8_t*)&rte_tempo_status,   sizeof (rte_tempo_status));
    file.write ((uint8_t*)&rte_pointe_status,  sizeof (rte_pointe_status));

    // close file
    file.close ();
  }
}

/***************************************\
 *           JSON publication
\***************************************/

// publish RTE data thru MQTT
void TeleinfoRtePublishJson ()
{
  // check need of publication
  rte_update.publish = false;
  if (!rte_config.tempo.enabled && !rte_config.pointe.enabled && !rte_config.ecowatt.enabled) return;

  // init message
  ResponseClear ();
  ResponseAppendTime ();

  // TEMPO, POINTE and ECOWATT
  TeleinfoRteTempoAppendJSON ();
  TeleinfoRtePointeAppendJSON ();
  TeleinfoRteEcowattAppendJSON ();

  // publish message
  ResponseJsonEnd ();
  MqttPublishPrefixTopic_P (TELE, PSTR ("RTE"), Settings->flag.mqtt_sensor_retain);
  XdrvRulesProcess (true);

  // plan next update and declare publication
  rte_update.publish = false;
  TeleinfoDriverWebDeclare (TIC_WEB_MQTT);
}

/***********************************\
 *        Token management
\***********************************/

// next token update calculation
void TeleinfoRteTokenNextUpdate (const uint32_t validity)
{
  // calculate next token update
  rte_update.time_token   = LocalTime () + validity - 10;
  rte_update.time_tempo   = max (rte_update.time_tempo,   LocalTime () + RTE_TEMPO_DELAY_INITIAL);
  rte_update.time_pointe  = max (rte_update.time_pointe,  LocalTime () + RTE_POINTE_DELAY_INITIAL);
  rte_update.time_ecowatt = max (rte_update.time_ecowatt, LocalTime () + RTE_ECOWATT_DELAY_INITIAL);
}

// token connexion
bool TeleinfoRteStreamToken ()
{
  bool     is_ok;
  uint32_t time_start, validity;
  int32_t  time_response;
  int      http_code;
  char     str_auth[128];
  String   str_result;
  JsonDocument     json_result;
  HTTPClientLight *phttp;

  // check private key
  if (strlen (rte_config.str_private_key) == 0) return false;

  // create connexion
  phttp = new HTTPClientLight ();
  is_ok = (phttp != nullptr);
  if (is_ok)
  {
    // connexion
    is_ok = phttp->begin (RTE_URL_OAUTH2);
    if (is_ok) 
    {
      // set authorisation
      strcpy_P (str_auth, PSTR ("Basic "));
      strlcat (str_auth, rte_config.str_private_key, sizeof (str_auth) - 1);

      // set timeout and headers
      phttp->setTimeout (RTE_HTTPS_TIMEOUT);
      phttp->addHeader (F ("Authorization"), str_auth, false, true);
      phttp->addHeader (F ("Accept"), F ("application/json"), false, true);

      // connexion
      time_start = millis ();
      http_code = phttp->POST (nullptr, 0);
      time_response = TimePassedSince (time_start);

      // collect result
      is_ok = ((http_code == HTTP_CODE_OK) || (http_code == HTTP_CODE_MOVED_PERMANENTLY));
      if (is_ok) str_result = phttp->getString ();
      is_ok = !str_result.isEmpty ();

      // end connexion
      phttp->end ();
      
      // log
      AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Token - [%d] %u char in %d ms"), http_code, str_result.length (), time_response);
    }
    else AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Token - Connexion begin failed"));

    // delete connexion
    delete phttp;
  }
  else AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Token - Connexion creation failed"));

  // if data received
  if (is_ok)
  {
    // extract token from JSON
    deserializeJson (json_result, str_result.c_str ());

    // get token value
    strlcpy (rte_update.str_token, json_result[F ("access_token")].as<const char*> (), sizeof (rte_update.str_token));

    // set token validity
    validity = json_result[F ("expires_in")].as<unsigned int> ();
    TeleinfoRteTokenNextUpdate (validity);
    AddLog (LOG_LEVEL_DEBUG, PSTR ("RTE: Token - Valid for %u seconds"), validity);
  }

  // else
  else
  {
    // plan next retry and reset token
    rte_update.time_token = LocalTime () + RTE_TOKEN_DELAY_RETRY;
    rte_update.str_token[0] = 0;
  }

  // declare query
  TeleinfoDriverWebDeclare (TIC_WEB_HTTPS);

  return is_ok;
}

/***********************************\
 *        Ecowatt management
\***********************************/

bool TeleinfoRteEcowattIsEnabled ()
{
  return rte_config.ecowatt.enabled;
}

// next ecowatt update calculation
uint32_t TeleinfoRteEcowattNextUpdate (const bool need_retry)
{
  uint32_t result;

  // init
  result = LocalTime ();

  // if stream reading should be retried, else plan next update
  if (need_retry) result += RTE_ECOWATT_DELAY_RETRY;
    else result += RTE_ECOWATT_DELAY_RENEW;

  return result;
}

// init daily data
void TeleinfoRteEcowattDayInit (const uint8_t day)
{
  uint8_t slot;

  // check parameters
  if (day >= RTE_ECOWATT_DAY_MAX) return;

  // init date, global status and slots for day+3
  strcpy_P (rte_ecowatt_status.arr_day[day].str_day_of_week, PSTR (""));
  rte_ecowatt_status.arr_day[day].day_of_month = 0;
  rte_ecowatt_status.arr_day[day].month        = 0;
  rte_ecowatt_status.arr_day[day].dvalue       = RTE_ECOWATT_LEVEL_NORMAL;
  for (slot = 0; slot < 24; slot ++) rte_ecowatt_status.arr_day[day].arr_hvalue[slot] = RTE_ECOWATT_LEVEL_NORMAL;
}

// shift data from one day to another
void TeleinfoRteEcowattDayShift (const uint8_t origin, const uint8_t target)
{
  uint8_t slot;

  // check parameters
  if (target >= RTE_ECOWATT_DAY_MAX) return;
  if (origin >= RTE_ECOWATT_DAY_MAX) return;
  

  // shift date, global status and slots for day, day+1 and day+2
  strcpy (rte_ecowatt_status.arr_day[target].str_day_of_week, rte_ecowatt_status.arr_day[origin].str_day_of_week);
  rte_ecowatt_status.arr_day[target].day_of_month = rte_ecowatt_status.arr_day[origin].day_of_month;
  rte_ecowatt_status.arr_day[target].month        = rte_ecowatt_status.arr_day[origin].month;
  rte_ecowatt_status.arr_day[target].dvalue       = rte_ecowatt_status.arr_day[origin].dvalue;
  for (slot = 0; slot < 24; slot ++) rte_ecowatt_status.arr_day[target].arr_hvalue[slot] = rte_ecowatt_status.arr_day[origin].arr_hvalue[slot];

}

// convert ecowatt level to global level
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
  uint32_t    time_start;
  int32_t     time_response;
  int         http_code;
  char        str_day[12];
  char        str_text[256];
  const char *pstr_url;
  String      str_result;
  TIME_T      day_dst;
  JsonArray   arr_day, arr_slot;
  JsonDocument     json_result;
  HTTPClientLight *phttp;

  // check token
  if (strlen (rte_update.str_token) == 0)
  {
    AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Ecowatt - Token missing"));
    return false;
  }

  // create connexion
  phttp = new HTTPClientLight ();
  is_ok = (phttp != nullptr);
  if (is_ok)
  {
    // connexion
    if (rte_config.sandbox) pstr_url = RTE_URL_ECOWATT_SANDBOX;
      else pstr_url = RTE_URL_ECOWATT_DATA;
    AddLog (LOG_LEVEL_DEBUG, PSTR ("RTE: Ecowatt - %s"), pstr_url);

    // connexion
    is_ok = phttp->begin (pstr_url);
    if (is_ok)
    {
      // set authorisation
      strcpy_P (str_text, PSTR ("Bearer "));
      strlcat (str_text, rte_update.str_token, sizeof (str_text) - 1);

      // set timeout and headers
      phttp->setTimeout (RTE_HTTPS_TIMEOUT);
      phttp->addHeader (F ("Authorization"), str_text, false, true);
      phttp->addHeader (F ("Accept"), F ("application/json"), false, true);

      // connexion
      time_start = millis ();
      http_code = phttp->GET ();
      time_response = TimePassedSince (time_start);

      // if success, collect result
      is_ok = ((http_code == HTTP_CODE_OK) || (http_code == HTTP_CODE_MOVED_PERMANENTLY));
      if (is_ok) str_result = phttp->getString ();
      is_ok = !str_result.isEmpty ();

      // end connexion
      phttp->end ();
      
      // log
      AddLog (LOG_LEVEL_INFO, PSTR ("SOL: Ecowatt - [%d] %u char in %d ms"), http_code, str_result.length (), time_response);
    }
    else AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Ecowatt - Connexion begin failed"));

    // delete connexion
    delete phttp;
  }
  
  else AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Ecowatt - Connexion creation failed"));

  // process received data
  if (is_ok)
  {
    // extract token from JSON
    deserializeJson (json_result, str_result.c_str ());

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

    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Ecowatt - Update till %02u/%02u"), rte_ecowatt_status.arr_day[index_array].day_of_month, rte_ecowatt_status.arr_day[index_array].month);
  }

  // declare query
  TeleinfoDriverWebDeclare (TIC_WEB_HTTPS);

  return is_ok;
}

// Append Ecowatt data to SENSOR
void TeleinfoRteEcowattAppendSensor ()
{
  char str_value[8];

  // check data enabled
  if (!rte_config.ecowatt.enabled) return;

  // check need of ',' according to previous data
  MiscOptionPrepareJsonSection ();
  ResponseAppend_P (PSTR ("\"ECOWATT\":{"));

  // ecowatt today
  GetTextIndexed (str_value, sizeof (str_value), rte_ecowatt_status.arr_day[RTE_ECOWATT_DAY_TODAY].dvalue, kRteEcowattJSON);
  ResponseAppend_P (PSTR ("\"tday\":\"%s\""), str_value);

  // ecowatt tomorrow
  GetTextIndexed (str_value, sizeof (str_value), rte_ecowatt_status.arr_day[RTE_ECOWATT_DAY_TOMORROW].dvalue, kRteEcowattJSON);
  ResponseAppend_P (PSTR (",\"tmrw\":\"%s\""), str_value);

  // ecowatt day after
  GetTextIndexed (str_value, sizeof (str_value), rte_ecowatt_status.arr_day[RTE_ECOWATT_DAY_PLUS2].dvalue, kRteEcowattJSON);
  ResponseAppend_P (PSTR (",\"day2\":\"%s\""), str_value);

  ResponseJsonEnd ();
}

// publish Ecowatt full RTE data
void TeleinfoRteEcowattAppendJSON ()
{
  uint8_t  index, slot;
  uint8_t  day_now, day_next, hour_now, hour_next;
  String   str_day;

  // check data flag
  if (!rte_config.ecowatt.enabled) return;

  // current time
  day_now   = RTE_ECOWATT_DAY_TODAY;
  hour_now  = RtcTime.hour;
  day_next  = day_now;
  hour_next = hour_now + 1;
  if (hour_next == 24) { hour_next = 0; day_next++; }

  // ,"ECOWATT":{...}
  MiscOptionPrepareJsonSection ();
  ResponseAppend_P (PSTR ("\"ECOWATT\":{"));

  // publish current status
  ResponseAppend_P (PSTR ("\"dval\":%u,\"now\":%u,\"next\":%u"), rte_ecowatt_status.arr_day[day_now].dvalue,  rte_ecowatt_status.arr_day[day_now].arr_hvalue[hour_now], rte_ecowatt_status.arr_day[day_next].arr_hvalue[hour_next]);

  // loop thru number of slots to publish
  for (index = 0; index < RTE_ECOWATT_DAY_MAX; index ++)
  {
    str_day="";
    for (slot = 0; slot < 24; slot ++) str_day += rte_ecowatt_status.arr_day[index].arr_hvalue[slot];
    ResponseAppend_P (PSTR (",\"day%u\":{\"jour\":\"%s\",\"date\":\"%u:%u\",\"dval\":%u,\"hour\"=\"%s\"}"), index, rte_ecowatt_status.arr_day[index].str_day_of_week, rte_ecowatt_status.arr_day[index].day_of_month, rte_ecowatt_status.arr_day[index].month, rte_ecowatt_status.arr_day[index].dvalue, str_day.c_str ());
  }
  
  // end of section
  ResponseJsonEnd ();

  // plan next update
  rte_ecowatt_status.time_json = LocalTime () + RTE_ECOWATT_UPDATE_JSON;
}

// ecowatt data rotation at midnight
void TeleinfoRteEcowattMidnight ()
{
  uint8_t index;

  // shift day's data from 0 to day+2
  for (index = 0; index < RTE_ECOWATT_DAY_MAX - 1; index ++) TeleinfoRteEcowattDayShift (index + 1, index);

  // init date, global status and slots for day+3
  TeleinfoRteEcowattDayInit (RTE_ECOWATT_DAY_MAX - 1);

  // publish data change
  rte_update.publish |= rte_config.ecowatt.enabled;

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Ecowatt - Midnight rotation"));
}

/***********************************\
 *        Tempo management
\***********************************/

bool TeleinfoRteTempoIsEnabled ()
{
  return rte_config.tempo.enabled;
}

void TeleinfoRteTempoReset ()
{
  uint8_t index;

  // tempo initialisation
  for (index = 0; index < RTE_DAY_MAX; index ++)
  {
    rte_tempo_status.arr_day[index].level = RTE_TEMPO_LEVEL_UNKNOWN;
    rte_tempo_status.arr_day[index].proba = 0;
  }
  rte_tempo_status.left.white = UINT8_MAX;
  rte_tempo_status.left.red   = UINT8_MAX;
}

 
// next tempo update calculation
uint32_t TeleinfoRteTempoNextUpdate (const bool need_retry)
{
  uint32_t result, hour;

  // init
  hour = (uint32_t)RtcTime.hour;
  result = LocalTime ();

  // calculate next update
  if (need_retry) result += RTE_TEMPO_DELAY_RETRY;                                                                      // stream reading should be retried
    else if (hour < 11) result += (11 - hour) * 3600;                                                                   // before 11h, plan next update after 11h
    else if (rte_tempo_status.arr_day[RTE_DAY_TOMORROW].proba != RTE_TEMPO_KNOWN) result += RTE_TEMPO_DELAY_UNKNOWN;    // after 11h and tomorrow unknown, plan next update in 30 mn
    else result += (24 + 11 - hour) * 3600;                                                                             // tomorrow known, plan next update after 11h tomorrow

  return result;
}

uint8_t TeleinfoRteTempoGetLevel (const uint8_t day, const uint8_t hour)
{
  uint8_t level;
  
  // check parameter
  if (day >= RTE_DAY_MAX) return RTE_TEMPO_LEVEL_UNKNOWN;

  // calculate level according to time
  if (hour >= 6) level = rte_tempo_status.arr_day[day].level;
    else if (day > RTE_DAY_YESTERDAY) level = rte_tempo_status.arr_day[day - 1].level;
    else level = RTE_TEMPO_LEVEL_UNKNOWN;

  return level;
}

uint8_t TeleinfoRteTempoGetGlobalLevel (const uint8_t day, const uint8_t hour)
{
  uint8_t level, result;

  // get level
  level = TeleinfoRteTempoGetLevel (day, hour);

  // convert level
  switch (level)
  {
    case RTE_TEMPO_LEVEL_BLUE:  result = RTE_LEVEL_BLUE;    break;
    case RTE_TEMPO_LEVEL_WHITE: result = RTE_LEVEL_WHITE;   break;
    case RTE_TEMPO_LEVEL_RED:   result = RTE_LEVEL_RED;     break;
    default:                    result = RTE_LEVEL_UNKNOWN; break;
  }

  return result;
}

// get Tempo level
uint8_t TeleinfoRteTempoGetDailyLevel (const uint8_t day)
{
  // check bouddaries
  if (day >= RTE_DAY_MAX) return RTE_TEMPO_LEVEL_UNKNOWN;

  return rte_tempo_status.arr_day[day].level;
}

// get RTE or Open DPE probability level
uint8_t TeleinfoRteTempoGetProbability (const uint8_t day)
{
  // check bouddaries
  if (day >= RTE_DAY_MAX) return UINT8_MAX;

  return rte_tempo_status.arr_day[day].proba;;
}

// tempo stream reception
bool TeleinfoRteTempoStream ()
{
  bool      is_ok;
  int       http_code;
  int32_t   tz_offset;
  uint16_t  size;
  uint32_t  time_start;
  int32_t   time_response;
  char      str_text[256];
  TIME_T    start_dst, stop_dst;
  String    str_result;
  JsonArray arr_day;
  JsonDocument     json_result;
  HTTPClientLight *phttp;

  // check token
  if (strlen (rte_update.str_token) == 0)
  {
    AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Tempo - Token missing"));
    return false;
  }

  // create connexion
  phttp = new HTTPClientLight ();
  is_ok = (phttp != nullptr);
  if (is_ok)
  {
    // calculate time window
    time_start = LocalTime ();
    BreakTime (time_start - 86400, start_dst);
    BreakTime (time_start + 172800, stop_dst);

    // calculate timezone offset
    tz_offset = Rtc.time_timezone / 60;

    // generate URL
    sprintf_P (str_text, PSTR ("%s?start_date=%04u-%02u-%02uT00:00:00+%02d:00&end_date=%04u-%02u-%02uT00:00:00+%02d:00"), RTE_URL_TEMPO_DATA, 1970 + start_dst.year, start_dst.month, start_dst.day_of_month, tz_offset, 1970 + stop_dst.year, stop_dst.month, stop_dst.day_of_month, tz_offset);
    AddLog (LOG_LEVEL_DEBUG, PSTR ("RTE: Tempo - %s"), str_text);

    // connexion
    is_ok = phttp->begin (str_text);
    if (is_ok)
    {
      // set authorisation
      strcpy_P (str_text, PSTR ("Bearer "));
      strlcat (str_text, rte_update.str_token, sizeof (str_text) - 1);

      // set timeout and headers
      phttp->setTimeout (RTE_HTTPS_TIMEOUT);
      phttp->addHeader (F ("Authorization"), str_text, false, true);
      phttp->addHeader (F ("Accept"), F ("application/json"), false, true);

      // connexion
      time_start = millis ();
      http_code = phttp->GET ();
      time_response = TimePassedSince (time_start);

      // if success, collect result
      is_ok = ((http_code == HTTP_CODE_OK) || (http_code == HTTP_CODE_MOVED_PERMANENTLY));
      if (is_ok) str_result = phttp->getString ();
      is_ok = !str_result.isEmpty ();

      // end connexion
      phttp->end ();
      
      // log
      AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Tempo - [%d] %u char in %d ms"), http_code, str_result.length (), time_response);
    }

    else AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Tempo - Connexion begin failed"));

    // delete connexion
    delete phttp;
  }
  else AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Tempo - Connexion creation failed"));

  // process received data
  if (is_ok)
  {
    // extract token from JSON
    deserializeJson (json_result, str_result.c_str ());

    // check array Size
    arr_day = json_result["tempo_like_calendars"]["values"].as<JsonArray>();
    size = arr_day.size ();

    // if tomorrow is not available
    if (size == 2)
    {
      rte_tempo_status.arr_day[RTE_DAY_TODAY].level = GetCommandCode (str_text, sizeof (str_text), arr_day[0][F ("value")], kTeleinfoRteTempoJSON);
      rte_tempo_status.arr_day[RTE_DAY_TODAY].proba = RTE_TEMPO_KNOWN;
      rte_tempo_status.arr_day[RTE_DAY_YESTERDAY].level = GetCommandCode (str_text, sizeof (str_text), arr_day[1][F ("value")], kTeleinfoRteTempoJSON);
      rte_tempo_status.arr_day[RTE_DAY_YESTERDAY].proba = RTE_TEMPO_KNOWN;
    }

    // else tomorrow is available
    else if (size == 3)
    {
      rte_tempo_status.arr_day[RTE_DAY_TOMORROW].level = GetCommandCode (str_text, sizeof (str_text), arr_day[0][F ("value")], kTeleinfoRteTempoJSON);
      rte_tempo_status.arr_day[RTE_DAY_TOMORROW].proba = RTE_TEMPO_KNOWN;
      rte_tempo_status.arr_day[RTE_DAY_TODAY].level = GetCommandCode (str_text, sizeof (str_text), arr_day[1][F ("value")], kTeleinfoRteTempoJSON);
      rte_tempo_status.arr_day[RTE_DAY_TODAY].proba = RTE_TEMPO_KNOWN;
      rte_tempo_status.arr_day[RTE_DAY_YESTERDAY].level = GetCommandCode (str_text, sizeof (str_text), arr_day[2][F ("value")], kTeleinfoRteTempoJSON);
      rte_tempo_status.arr_day[RTE_DAY_YESTERDAY].proba = RTE_TEMPO_KNOWN;
    }

    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Tempo - Update done (%d-%d-%d)"), rte_tempo_status.arr_day[RTE_DAY_YESTERDAY].level, rte_tempo_status.arr_day[RTE_DAY_TODAY].level, rte_tempo_status.arr_day[RTE_DAY_TOMORROW].level);
  }

  // declare query
  TeleinfoDriverWebDeclare (TIC_WEB_HTTPS);

  return is_ok;
}

// Append Tempo data to SENSOR
void TeleinfoRteTempoAppendSensor ()
{
  uint8_t index;
  char    str_value[8];

  // check data enabled
  if (!rte_config.tempo.enabled) return;

  // check need of ',' according to previous data
  MiscOptionPrepareJsonSection ();
  ResponseAppend_P (PSTR ("\"TEMPO\":{"));

  // today
  GetTextIndexed (str_value, sizeof (str_value), rte_tempo_status.arr_day[RTE_DAY_TODAY].level, kRteTempoColorJSON);
  ResponseAppend_P (PSTR ("\"tday\":\"%s\""), str_value);

  // tomorrow
  GetTextIndexed (str_value, sizeof (str_value), rte_tempo_status.arr_day[RTE_DAY_TOMORROW].level, kRteTempoColorJSON);
  ResponseAppend_P (PSTR (",\"tmrw\":\"%s\""), str_value);

  // next days
  for (index = RTE_DAY_PLUS2; index < RTE_DAY_PLUS7; index ++)
  {
    GetTextIndexed (str_value, sizeof (str_value), rte_tempo_status.arr_day[index].level, kRteTempoColorJSON);
    ResponseAppend_P (PSTR (",\"day%u\":\"%s\""), index, str_value);
  }

  ResponseJsonEnd ();
}

// publish full RTE Tempo data
void TeleinfoRteTempoAppendJSON ()
{
  uint8_t level, day;
  char    str_label[8];
  char    str_icon[8];

  // check data flag
  if (!rte_config.tempo.enabled) return;

  // "TEMPO":{...}}
  MiscOptionPrepareJsonSection ();
  ResponseAppend_P (PSTR ("\"TEMPO\":{"));

  // publish current status
  level = TeleinfoRteTempoGetLevel (RTE_DAY_TODAY, RtcTime.hour);
  GetTextIndexed (str_icon,  sizeof (str_icon),  level, kRteTempoIcon); 
  GetTextIndexed (str_label, sizeof (str_label), level, kRteTempoColorJSON);
  ResponseAppend_P (PSTR ("\"lv\":%u,\"label\":\"%s\",\"icon\":\"%s\""), level, str_label, str_icon);

  // publish days
  ResponseAppend_P (PSTR (",\"tday\":%u,\"tmrw\":%u"), rte_tempo_status.arr_day[RTE_DAY_TODAY].level, rte_tempo_status.arr_day[RTE_DAY_TOMORROW].level);
  for (day = RTE_DAY_PLUS2; day < RTE_DAY_PLUS7; day ++) ResponseAppend_P (PSTR (",\"day%u\":%u"), day - 1, rte_tempo_status.arr_day[day].level);

  // end of section
  ResponseJsonEnd ();

  // plan next update
  rte_tempo_status.time_json = LocalTime () + RTE_TEMPO_UPDATE_JSON;
}

// tempo data rotation at midnight
void TeleinfoRteTempoMidnight ()
{
  uint8_t index;
  
  // rotate tempo days
  for (index = 1; index < RTE_DAY_MAX; index ++)
  {
    rte_tempo_status.arr_day[index - 1].level = rte_tempo_status.arr_day[index].level;
    rte_tempo_status.arr_day[index - 1].proba = rte_tempo_status.arr_day[index].proba;
  }
  rte_tempo_status.arr_day[RTE_DAY_MAX - 1].level = RTE_TEMPO_LEVEL_UNKNOWN;
  rte_tempo_status.arr_day[RTE_DAY_MAX - 1].proba = UINT8_MAX;

  // publish data change
  rte_update.publish |= rte_config.tempo.enabled;

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Tempo - Midnight rotation"));
}

/***********************************\
 *       Tempo light management
\***********************************/

// tempo light next update calculation (after 6h till 12h)
uint32_t TeleinfoRteTempoLightNextUpdate (const bool need_retry)
{
  uint32_t result, hour;

  // init values
  hour = (uint32_t)RtcTime.hour;
  result = LocalTime ();

  // calculate next update slot
  if (need_retry) result += RTE_TEMPOLIGHT_DELAY_RETRY;       // reading should be retried
  else if (hour < 6)  result += 3600 * (6 - hour);            // before 6h, plan after 6h
  else if (hour < 12) result += 3600;                         // before 12h, plan every hour
  else                result += 3600 * (24 + 6 - hour);       // after 12h, plan tomorrow after 6h

  return result;
}

// tempo loight stream reception
bool TeleinfoRteTempoLightStream ()
{
  bool        is_ok, daysleft, found;
  int         index, position, size, http_code;
  uint32_t    time_start;
  int32_t     time_response;
  TIME_T      time_day;
  const char *pstr_color;
  char        str_color[8];
  char        str_text[64];
  String      str_result;
  JsonDocument     json_result;
  HTTPClientLight *phttp;

  // create connexion
  phttp = new HTTPClientLight ();
  is_ok = (phttp != nullptr);
  if (is_ok)
  {
    // connexion
    strcpy_P (str_text, RTE_URL_TEMPOLIGHT_DATA);
    is_ok = phttp->begin (str_text);
    if (is_ok)
    {
      // set timeout and headers
      phttp->setTimeout (RTE_HTTPS_TIMEOUT);
      phttp->addHeader (F ("Accept"), F ("application/json"), false, true);

      // connexion
      time_start = millis ();
      http_code = phttp->GET ();
      time_response = TimePassedSince (time_start);

      // if success, collect result
      is_ok = ((http_code == HTTP_CODE_OK) || (http_code == HTTP_CODE_MOVED_PERMANENTLY));
      if (is_ok) str_result = phttp->getString ();
      is_ok = !str_result.isEmpty ();
    
      // end connexion
      phttp->end ();
      
      // log
      AddLog (LOG_LEVEL_INFO, PSTR ("RTE: TempoLight - [%d] %u char in %d ms"), http_code, str_result.length (), time_response);
    }

    else AddLog (LOG_LEVEL_INFO, PSTR ("RTE: TempoLight - Connexion begin failed"));

    // delete connexion
    delete phttp;
  }
  else AddLog (LOG_LEVEL_INFO, PSTR ("RTE: TempoLight - Connexion creation failed"));

  // process received data
  if (is_ok)
  {
    // extract token from JSON
    deserializeJson (json_result, str_result.c_str ());

    // loop thru days
    time_start = LocalTime ();
    for (index = RTE_DAY_TODAY; index <= RTE_DAY_TOMORROW; index ++)
    {
      // if day is not a known RTE day
      if (rte_tempo_status.arr_day[index].proba < RTE_TEMPO_KNOWN)
      {
        // calculate current day
        BreakTime (time_start, time_day);
        sprintf_P (str_text, PSTR ("%04u-%02u-%02u"), time_day.year + 1970, time_day.month, time_day.day_of_month);

        // detect stream section
        pstr_color = json_result[F ("values")][str_text].as<const char*>();
        if (pstr_color != nullptr)
        {
          // update day
          rte_tempo_status.arr_day[index].level = max (0, GetCommandCode (str_color, sizeof (str_color), pstr_color, kTeleinfoRteTempoJSON));
          rte_tempo_status.arr_day[index].proba = RTE_TEMPOLIGHT_KNOWN;

          // log update
          AddLog (LOG_LEVEL_INFO, PSTR ("RTE: TempoLight - %s = %s)"), str_text, str_color);
        }
      }

      // set next day
      time_start += 86400;
    }
  }

  // declare query
  TeleinfoDriverWebDeclare (TIC_WEB_HTTPS);

  return is_ok;
}

/***********************************\
 *        open DPE management
\***********************************/

// open dpe next update calculation (after 8h or 16h)
uint32_t TeleinfoRteOpenDpeNextUpdate (const bool need_retry)
{
  uint32_t result, hour;

  // init values
  hour = (uint32_t)RtcTime.hour;
  result = LocalTime ();

  // calculate next update slot
  if (need_retry) result += RTE_OPENDPE_DELAY_RETRY;          // reading should be retried
  else if (hour < 8)  result += 3600 * (8 - hour);            // before 8h, plan after 8h
  else if (hour < 16) result += 3600 * (16 - hour);           // before 16h, plan after 16h
  else                result += 3600 * (24 + 8 - hour);       // after 16h, plan tomorrow after 8h

  return result;
}

// open dpe stream reception
bool TeleinfoRteOpenDpeStream ()
{
  bool        is_ok, daysleft, found;
  int         index, position, size, http_code;
  uint32_t    time_start;
  int32_t     time_response;
  TIME_T      time_day;
  const char *pstr_date;
  char        str_color[8];
  char        str_text[64];
  String      str_result;
  JsonDocument     json_result;
  HTTPClientLight *phttp;

  // create connexion
  phttp = new HTTPClientLight ();
  is_ok = (phttp != nullptr);
  if (is_ok)
  {
    // connexion
    strcpy_P (str_text, RTE_URL_OPENDPE_DATA);
    is_ok = phttp->begin (str_text);
    if (is_ok)
    {
      // set timeout and headers
      phttp->setTimeout (RTE_HTTPS_TIMEOUT);
      phttp->addHeader (F ("Accept"), F ("application/json"), false, true);

      // connexion
      time_start = millis ();
      http_code = phttp->GET ();
      time_response = TimePassedSince (time_start);

      // if success, collect result
      is_ok = ((http_code == HTTP_CODE_OK) || (http_code == HTTP_CODE_MOVED_PERMANENTLY));
      if (is_ok) str_result = phttp->getString ();
      is_ok = !str_result.isEmpty ();
    
      // end connexion
      phttp->end ();
      
      // log
      AddLog (LOG_LEVEL_INFO, PSTR ("RTE: OpenDPE - [%d] %u char in %d ms"), http_code, str_result.length (), time_response);
    }

    else AddLog (LOG_LEVEL_INFO, PSTR ("RTE: OpenDPE - Connexion begin failed"));

    // delete connexion
    delete phttp;
  }
  else AddLog (LOG_LEVEL_INFO, PSTR ("RTE: OpenDPE - Connexion creation failed"));

  // process received data
  if (is_ok)
  {
    // extract token from JSON
    deserializeJson (json_result, str_result.c_str ());

    // loop thru days
    daysleft   = false;
    time_start = LocalTime ();
    for (index = RTE_DAY_TODAY; index < RTE_DAY_MAX; index ++)
    {
      // if day is not a known RTE day
      if ((rte_tempo_status.arr_day[index].proba != RTE_TEMPO_KNOWN) && (rte_tempo_status.arr_day[index].proba != RTE_TEMPOLIGHT_KNOWN))
      {
        // calculate current day
        BreakTime (time_start, time_day);
        sprintf_P (str_text, PSTR ("%04u-%02u-%02u"), time_day.year + 1970, time_day.month, time_day.day_of_month);

        // detect stream section
        found = false;
        for (position = 0; position < 9; position ++)
        {
          pstr_date = json_result[position][F ("date")].as<const char*>();
          if (pstr_date != nullptr) found = (strcmp (str_text, pstr_date) == 0);
          if (found) break;
        }

        // if section found
        if (found)
        {
          // set color and probability
          rte_tempo_status.arr_day[index].level = max (0, GetCommandCode (str_color, sizeof (str_color), json_result[position][F ("tempo_color")].as<const char*>(), kRteOpenDpeColorJSON));
          rte_tempo_status.arr_day[index].proba = (uint8_t)(100 * json_result[position][F ("probability")].as<float>());

          // log update
          AddLog (LOG_LEVEL_INFO, PSTR ("RTE: OpenDPE - %s = %s, %u%%)"), str_text, str_color, rte_tempo_status.arr_day[index].proba);

          // if needed, update tempo days left
          if (!daysleft)
          {
            rte_tempo_status.left.white = (uint8_t)(json_result[position][F ("stock_blanc")].as<unsigned short>());
            rte_tempo_status.left.red   = (uint8_t)(json_result[position][F ("stock_rouge")].as<unsigned short>());
            daysleft = true;
          }
        }
      }

      // set next day
      time_start += 86400;
    }
  }

  // declare query
  TeleinfoDriverWebDeclare (TIC_WEB_HTTPS);

  return is_ok;
}

/***********************************\
 *        Pointe management
\***********************************/

bool TeleinfoRtePointeIsEnabled ()
{
  return (rte_config.pointe.enabled);
}

// next pointe update calculation
uint32_t TeleinfoRtePointeNextUpdate (const bool need_retry)
{
  uint32_t result;
  
  // init
  result = LocalTime ();

  // if stream reading should be retried, else plan next update
  if (need_retry) result += RTE_POINTE_DELAY_RETRY;
    else result += RTE_POINTE_DELAY_RENEW;

  return result;
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
  bool      is_ok;
  int       http_code;
  uint16_t  size;
  uint32_t  time_start;
  int32_t   time_response;
  char      str_text[256];
  String    str_result;
  JsonArray arr_day;
  JsonDocument     json_result;
  HTTPClientLight *phttp;

  // check token
  if (strlen (rte_update.str_token) == 0)
  {
    AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Pointe - Token missing"));
    return false;
  }

  // create connexion
  phttp = new HTTPClientLight ();
  is_ok = (phttp != nullptr);
  if (is_ok)
  {
    // connexion
    sprintf (str_text, "%s", RTE_URL_POINTE_DATA);
    AddLog (LOG_LEVEL_DEBUG, PSTR ("RTE: Pointe - %s"), str_text);

    // connexion
    is_ok = phttp->begin (str_text);
    if (is_ok)
    {
      // set authorisation
      strcpy_P (str_text, PSTR ("Bearer "));
      strlcat (str_text, rte_update.str_token, sizeof (str_text) - 1);

      // set timeout and headers
      phttp->setTimeout (RTE_HTTPS_TIMEOUT);
      phttp->addHeader (F ("Authorization"), str_text, false, true);
      phttp->addHeader (F ("Accept"), F ("application/json"), false, true);

      // check connexion status
      time_start = millis ();
      http_code = phttp->GET ();
      time_response = TimePassedSince (time_start);

      // if success, collect result
      is_ok = ((http_code == HTTP_CODE_OK) || (http_code == HTTP_CODE_MOVED_PERMANENTLY));
      if (is_ok) str_result = phttp->getString ();
      is_ok = !str_result.isEmpty ();

      // end connexion
      phttp->end ();
      
      // log
      AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Pointe - [%d] %u char in %d ms"), http_code, str_result.length (), time_response);
    }

    else AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Pointe - Connexion begin failed"));
  }

  else AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Pointe - Connexion creation failed"));

  // process received data
  if (is_ok)
  {
    // extract token from JSON
    deserializeJson (json_result, str_result.c_str ());

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

    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Pointe - Update done (%d-%d-%d)"), rte_pointe_status.arr_day[RTE_DAY_YESTERDAY], rte_pointe_status.arr_day[RTE_DAY_TODAY], rte_pointe_status.arr_day[RTE_DAY_TOMORROW]);
  }

  // declare query
  TeleinfoDriverWebDeclare (TIC_WEB_HTTPS);

  return is_ok;
}

// Append Pointe data to SENSOR
void TeleinfoRtePointeAppendSensor ()
{
  uint8_t index;
  char    str_value[8];

  // check data enabled
  if (!rte_config.pointe.enabled) return;

  // check need of ',' according to previous data
  MiscOptionPrepareJsonSection ();
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

// publish full RTE Pointe data
void TeleinfoRtePointeAppendJSON ()
{
  uint8_t level;
  char    str_label[8];
  char    str_icon[8];

  // check data flag
  if (!rte_config.pointe.enabled) return;

  // ,"POINTE":{...}
  MiscOptionPrepareJsonSection ();
  ResponseAppend_P (PSTR ("\"POINTE\":{"));

  // publish current status
  level  = TeleinfoRtePointeGetLevel (RTE_DAY_TODAY, RtcTime.hour);
  GetTextIndexed (str_label, sizeof (str_label), level, kRtePointeJSON);
  GetTextIndexed (str_icon, sizeof (str_icon), level, kRtePointeIcon);
  ResponseAppend_P (PSTR ("\"lv\":%u,\"label\":\"%s\",\"icon\":\"%s\""), level, str_label, str_icon);

  // publish days
  ResponseAppend_P (PSTR (",\"tday\":%u,\"tmrw\":%u"), rte_pointe_status.arr_day[RTE_DAY_TODAY], rte_pointe_status.arr_day[RTE_DAY_TOMORROW]);

  // end of section
  ResponseJsonEnd ();

  // plan next update
  rte_pointe_status.time_json = LocalTime () + RTE_POINTE_UPDATE_JSON;
}

// pointe data rotation at midnight
void TeleinfoRtePointeMidnight ()
{
  // rotate days
  rte_pointe_status.arr_day[RTE_DAY_YESTERDAY] = rte_pointe_status.arr_day[RTE_DAY_TODAY];
  rte_pointe_status.arr_day[RTE_DAY_TODAY]     = rte_pointe_status.arr_day[RTE_DAY_TOMORROW];
  rte_pointe_status.arr_day[RTE_DAY_TOMORROW]  = RTE_POINTE_LEVEL_UNKNOWN;

  // publish data change
  rte_update.publish |= rte_config.pointe.enabled;

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Pointe - Midnight rotation"));
}

/***********************************************************\
 *                      Callback
\***********************************************************/

// init main status
void TeleinfoRteInit ()
{
  uint8_t index;

  // init data
  rte_config.ecowatt.enabled = false;
  rte_config.ecowatt.display = false;
  rte_config.tempo.enabled   = false;
  rte_config.tempo.display   = false;
  rte_config.pointe.enabled  = false;
  rte_config.pointe.display  = false;
  rte_config.sandbox         = false;
  strcpy_P (rte_config.str_private_key, PSTR (""));
  strcpy_P (rte_update.str_token,       PSTR (""));

  // tempo initialisation
  TeleinfoRteTempoReset ();

  // pointe initialisation
  for (index = 0; index <= RTE_DAY_TOMORROW; index ++) rte_pointe_status.arr_day[index] = RTE_POINTE_LEVEL_UNKNOWN;

  // ecowatt initialisation
  for (index = 0; index < RTE_ECOWATT_DAY_MAX; index ++) TeleinfoRteEcowattDayInit (index);

  // load data file
  TeleinfoRteLoadData ();
  
  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: tapez rte pour avoir de l'aide sur le module RTE"));

  // log RTE key missing
  if (strlen (rte_config.str_private_key) == 0) AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Clé privée Base64 manquante"));
}

// called every second
void TeleinfoRteEverySecond ()
{
  bool     result;
//  uint8_t  period, level;
  uint8_t  level;
  uint32_t time_now, time_next;
  char     str_text[32];

  // check parameters
  if (!RtcTime.valid) return;
  if (TasmotaGlobal.global_state.network_down) return;

  // init
  time_now = LocalTime ();

  // check for midnight switch
  if (RtcTime.hour < rte_update.hour) TeleinfoRteMidnight ();
  rte_update.hour = RtcTime.hour;

  // if needed, calculate Open DPE update (no need of RTE token)
  if (!rte_config.tempo.enabled) rte_update.time_opendpe = UINT32_MAX;
  else if (rte_update.time_opendpe == UINT32_MAX) rte_update.time_opendpe = time_now + RTE_DELAY_QUERY;

  // if needed, calculate Tempo Light update (no need of RTE token)
  if (!rte_config.tempo.enabled) rte_update.time_tempolight = UINT32_MAX;
  else if (rte_update.time_tempolight == UINT32_MAX) rte_update.time_tempolight = time_now + RTE_DELAY_QUERY;

  // if needed, calculate token update
  result = rte_config.tempo.enabled || rte_config.pointe.enabled || rte_config.ecowatt.enabled;
  result &= (rte_config.str_private_key[0] != 0);
  if (!result) rte_update.time_token = UINT32_MAX;
  else if (rte_update.time_token == UINT32_MAX) rte_update.time_token = time_now + RTE_DELAY_QUERY;

  // if needed, calculate Tempo update
  if (!rte_config.tempo.enabled || (rte_update.time_token == UINT32_MAX)) rte_update.time_tempo = UINT32_MAX;
  else if (rte_update.time_tempo == UINT32_MAX) rte_update.time_tempo = time_now + RTE_DELAY_QUERY;

  // if needed, calculate Ecowatt update
  if (!rte_config.ecowatt.enabled || (rte_update.time_token == UINT32_MAX)) rte_update.time_ecowatt = UINT32_MAX;
  else if (rte_update.time_ecowatt == UINT32_MAX) rte_update.time_ecowatt = time_now + RTE_DELAY_QUERY;

  // if needed, calculate Pointe update
  if (!rte_config.pointe.enabled || (rte_update.time_token == UINT32_MAX)) rte_update.time_pointe = UINT32_MAX;
  else if (rte_update.time_pointe == UINT32_MAX) rte_update.time_pointe = time_now + RTE_DELAY_QUERY;

  // after boot, log all next updates
  if (!rte_init.done)
  {
    if (rte_update.time_opendpe    != UINT32_MAX) AddLog (LOG_LEVEL_INFO, PSTR ("RTE: OpenDPE - %s"),    TeleinfoRteGetDelay (rte_update.time_opendpe,    str_text, sizeof (str_text)));
    if (rte_update.time_tempolight != UINT32_MAX) AddLog (LOG_LEVEL_INFO, PSTR ("RTE: TempoLight - %s"), TeleinfoRteGetDelay (rte_update.time_tempolight, str_text, sizeof (str_text)));
    if (rte_update.time_token      != UINT32_MAX) AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Token - %s"),      TeleinfoRteGetDelay (rte_update.time_token,      str_text, sizeof (str_text)));
    if (rte_update.time_tempo      != UINT32_MAX) AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Tempo - %s"),      TeleinfoRteGetDelay (rte_update.time_tempo,      str_text, sizeof (str_text)));
    if (rte_update.time_ecowatt    != UINT32_MAX) AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Ecowatt - %s"),    TeleinfoRteGetDelay (rte_update.time_ecowatt,    str_text, sizeof (str_text)));
    if (rte_update.time_pointe     != UINT32_MAX) AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Pointe - %s"),     TeleinfoRteGetDelay (rte_update.time_pointe,     str_text, sizeof (str_text)));
    rte_init.done = true;
  }

  //   Stream reception
  // --------------------

  switch (rte_update.step)
  {
    // no current stream operation
    case RTE_UPDATE_NONE:
      if      (rte_update.time_opendpe    <= time_now) rte_update.step = RTE_UPDATE_OPENDPE;      // priority 1 : check for Open DPE update
      else if (rte_update.time_tempolight <= time_now) rte_update.step = RTE_UPDATE_TEMPOLIGHT;   // priority 2 : check for Tempo Light update
      else if (rte_update.time_token      <= time_now) rte_update.step = RTE_UPDATE_TOKEN;        // priority 3 : check for need of RTE token update
      else if (rte_update.time_tempo      <= time_now) rte_update.step = RTE_UPDATE_TEMPO;        // priority 4 : check for Tempo update
      else if (rte_update.time_ecowatt    <= time_now) rte_update.step = RTE_UPDATE_ECOWATT;      // priority 5 : check for Ecowatt update
      else if (rte_update.time_pointe     <= time_now) rte_update.step = RTE_UPDATE_POINTE;       // priority 6 : check for Pointe update
      break;

    // Open DPE reception
    case RTE_UPDATE_OPENDPE:
      if (TeleinfoDriverWebAllow (TIC_WEB_HTTPS))
      {
        // update data and calculate next update slot
        result = TeleinfoRteOpenDpeStream ();
        rte_update.time_opendpe = TeleinfoRteOpenDpeNextUpdate (!result);
        AddLog (LOG_LEVEL_INFO, PSTR ("RTE: OpenDPE - %s"), TeleinfoRteGetDelay (rte_update.time_opendpe, str_text, sizeof (str_text)));

        // init next step
        rte_update.step = RTE_UPDATE_NONE; 
      }
      break;

    // Tempo Light reception
    case RTE_UPDATE_TEMPOLIGHT:
      if (TeleinfoDriverWebAllow (TIC_WEB_HTTPS))
      {
        // update data and calculate next update slot
        result = TeleinfoRteTempoLightStream ();
        rte_update.time_tempolight = TeleinfoRteTempoLightNextUpdate (!result);
        AddLog (LOG_LEVEL_INFO, PSTR ("RTE: TempoLight - %s"), TeleinfoRteGetDelay (rte_update.time_tempolight, str_text, sizeof (str_text)));

        // init next step
        rte_update.step = RTE_UPDATE_NONE; 
      }
      break;

    // token reception
    case RTE_UPDATE_TOKEN:
      if (TeleinfoDriverWebAllow (TIC_WEB_HTTPS))
      {
        // init token update
        result = TeleinfoRteStreamToken ();
        AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Token - %s"), TeleinfoRteGetDelay (rte_update.time_token, str_text, sizeof (str_text)));

        // if failure
        if (!result)
        {
          // update RTE stream planification
          rte_update.time_tempo   = max (rte_update.time_token, rte_update.time_tempo);
          rte_update.time_pointe  = max (rte_update.time_token, rte_update.time_pointe);
          rte_update.time_ecowatt = max (rte_update.time_token, rte_update.time_ecowatt);
        }

        // init next step
        rte_update.step = RTE_UPDATE_NONE; 
      }
      break;

    // ecowatt reception
    case RTE_UPDATE_ECOWATT:
      if (TeleinfoDriverWebAllow (TIC_WEB_HTTPS))
      {
        // update data and calculate next update slot
        result = TeleinfoRteEcowattStream ();
        rte_update.time_ecowatt = TeleinfoRteEcowattNextUpdate (!result);
        AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Ecowatt - %s"), TeleinfoRteGetDelay (rte_update.time_ecowatt, str_text, sizeof (str_text)));

        // init next step
        rte_update.step = RTE_UPDATE_NONE; 
      }
      break;

    // tempo reception
    case RTE_UPDATE_TEMPO:
      if (TeleinfoDriverWebAllow (TIC_WEB_HTTPS))
      {
        // update data and calculate next update slot
        result = TeleinfoRteTempoStream ();
        rte_update.time_tempo = TeleinfoRteTempoNextUpdate (!result);
        AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Tempo - %s"), TeleinfoRteGetDelay (rte_update.time_tempo, str_text, sizeof (str_text)));

        // init next step
        rte_update.step = RTE_UPDATE_NONE; 
      }
      break;
      
    // pointe reception
    case RTE_UPDATE_POINTE:
      if (TeleinfoDriverWebAllow (TIC_WEB_HTTPS))
      {
        // update data and calculate next update slot
        result = TeleinfoRtePointeStream ();
        rte_update.time_pointe = TeleinfoRtePointeNextUpdate (!result);
        AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Pointe - %s"), TeleinfoRteGetDelay (rte_update.time_pointe, str_text, sizeof (str_text)));

        // init next step
        rte_update.step = RTE_UPDATE_NONE;
      }
      break;
    }

  // check for tempo JSON update
  if (rte_config.tempo.enabled)
  {
    if (rte_tempo_status.time_json == UINT32_MAX) rte_tempo_status.time_json = time_now + RTE_TEMPO_DELAY_INITIAL;
    rte_update.publish |= (rte_tempo_status.time_json <= time_now);
  }

  // check for pointe JSON update
  if (rte_config.pointe.enabled)
  {
    if (rte_pointe_status.time_json == UINT32_MAX) rte_pointe_status.time_json = time_now + RTE_POINTE_DELAY_INITIAL;
    rte_update.publish |= (rte_pointe_status.time_json <= time_now);
  }

  // check for ecowatt JSON update
  if (rte_config.ecowatt.enabled)
  {
    if (rte_ecowatt_status.time_json == UINT32_MAX) rte_ecowatt_status.time_json = time_now + RTE_ECOWATT_DELAY_INITIAL;
    rte_update.publish |= (rte_ecowatt_status.time_json <= time_now);
  }

  // if needed, publish TEMPO, POINTE and/or ECOWATT
  if (rte_update.publish && TeleinfoDriverWebAllow (TIC_WEB_MQTT)) TeleinfoRtePublishJson ();
}

// Append RTE data to SENSOR
void TeleinfoRteAppendSensor ()
{
  // append data
  TeleinfoRteEcowattAppendSensor ();
  TeleinfoRteTempoAppendSensor ();
  TeleinfoRtePointeAppendSensor ();
}

// Handle MQTT teleperiod
void TeleinfoRteTeleperiod ()
{
  // ask for update if any RTE data is enabled
  rte_update.publish |= rte_config.ecowatt.enabled || rte_config.tempo.enabled || rte_config.pointe.enabled;
}

// called every day at midnight for data rotation
void TeleinfoRteMidnight ()
{
  TeleinfoRteEcowattMidnight ();
  TeleinfoRteTempoMidnight ();
  TeleinfoRtePointeMidnight ();
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

void TeleinfoRteWebMainButton ()
{
  uint8_t index;
  char    str_color[8];
  char    str_border[8];
  char    str_text[16];

  WSContentSend_P (PSTR ("<style>\n"));

  // ecowatt styles
  for (index = 0; index < RTE_ECOWATT_LEVEL_MAX; index ++)
  {
    GetTextIndexed (str_color, sizeof (str_color), index, kRteEcowattColor);
    WSContentSend_P (PSTR (".eco%u{width:3.4%%;padding:1px 0px;background-color:%s;}\n"), index, str_color);
  }

  // ecowatt levels
  for (index = 0; index < RTE_ECOWATT_LEVEL_MAX; index ++)
  {
    GetTextIndexed (str_color, sizeof (str_color), index, kRteEcowattColor);
    GetTextIndexed (str_text, sizeof (str_text), index, kRteEcowattJSON);
    WSContentSend_P (PSTR (".tic span.e%u {background:%s;border-color:%s;color:white;}\n"), index, str_color, str_color, str_text);
  }

  // tempo styles
  WSContentSend_P (PSTR (".tic span.dots {width:10%%;padding:0px 10px;margin-left:10px;border:1px solid transparent;border-radius:5px;}\n"));
  WSContentSend_P (PSTR (".tic span.dotn {width:10%%;font-size:10px;margin:3px 5px;height:22px;padding-top:6px;border:1px solid #666;border-radius:8px;}\n"));
  WSContentSend_P (PSTR (".tic span.day {width:10%%;margin:-6px 5px 0px 5px;border:1px solid #333333;}\n"));
  WSContentSend_P (PSTR (".tic span.now {border-width:4px;margin-top:0px;}\n"));
  WSContentSend_P (PSTR (".tic span.rte {font-weight:bold;}\n"));

  // tempo levels
  for (index = 0; index < RTE_TEMPO_LEVEL_MAX; index ++)
  {
    GetTextIndexed (str_color, sizeof (str_color), index, kRteTempoColor);
    GetTextIndexed (str_border, sizeof (str_border), index, kRteTempoColorBorder);
    GetTextIndexed (str_text, sizeof (str_text), index, kRteTempoColorText);
    WSContentSend_P (PSTR (".tic span.l%u {background:%s;border-color:%s;color:%s;}\n"), index, str_color, str_border, str_text);
  }

  WSContentSend_P (PSTR ("</style>\n"));
}

char* TeleinfoRteGetDelay (const uint32_t timestamp, char *pstr_delay, const size_t size_delay)
{
  TIME_T time_dst;

  // check parameters
  if (pstr_delay == nullptr) return nullptr;
  strcpy_P (pstr_delay, PSTR (""));
  if (timestamp == UINT32_MAX) return pstr_delay;
  if (size_delay < 32) return pstr_delay;

  // set status
  BreakTime (timestamp, time_dst);
  sprintf_P (pstr_delay, PSTR ("Prochaine maj à %02u:%02u"), time_dst.hour, time_dst.minute);

  return pstr_delay;
}

/*
void TeleinfoRteGetStatus (const uint32_t timestamp, char *pstr_status, const size_t size_status)
{
  uint32_t time_now;

  // check parameters
  if (pstr_status == nullptr) return;
  if (size_status < 32) return;
  
  // init
  time_now = LocalTime ();
  strcpy_P (pstr_status, PSTR (""));

  // set status
  if (strlen (rte_config.str_private_key) == 0)   strcpy_P (pstr_status, PSTR ("Clé privée RTE manquante"));
    else if (rte_update.time_token == UINT32_MAX) strcpy_P (pstr_status, PSTR ("Initialisation"));
    else if (timestamp < time_now + 10)           strcpy_P (pstr_status, PSTR ("Maj imminente"));
    else if (timestamp != UINT32_MAX) TeleinfoRteGetDelay (timestamp, pstr_status, size_status);
}
*/

// append tempo forecast to main page
void TeleinfoRteWebSensorTempo ()
{
  uint8_t  day, level, proba;
  uint32_t time_update;
  char     str_value[8];
  char     str_text[64];

  // check display parameters
  if (!rte_config.tempo.enabled) return;
  if (!rte_config.tempo.display) return;

  // header and title display
  time_update = min (rte_update.time_opendpe, rte_update.time_tempolight);
  time_update = min (time_update, rte_update.time_tempo);
  TeleinfoRteGetDelay (time_update, str_text, sizeof (str_text));
  WSContentSend_P (PSTR ("<div class='sec' title='%s' onclick=\"onClickMain(\'%s?%s=%u\')\">\n"), str_text, PSTR (TIC_PAGE_DISPLAY), PSTR (CMND_TIC_MAIN), TIC_DISPLAY_TEMPO);
  WSContentSend_P (PSTR ("<div class='tic ticb'><div class='tic48l'>Tempo</div>"));

  // first line display (minimized or not)
  WSContentSend_P (PSTR ("<div class='tic50r tichead'>"));
  if (!teleinfo_config.arr_main[TIC_DISPLAY_TEMPO]) WSContentSend_P (PSTR ("<span class='dots l%u'>Aujourd.</span><span class='dots l%u'>Demain</span>"), TeleinfoRteTempoGetDailyLevel (RTE_DAY_TODAY), TeleinfoRteTempoGetDailyLevel (RTE_DAY_TOMORROW));
    else if (rte_tempo_status.left.white != UINT8_MAX) WSContentSend_P (PSTR ("Reste <span class='dots l2'>%u</span><span class='dots l3'>%u</span>"), rte_tempo_status.left.white, rte_tempo_status.left.red);
  WSContentSend_P (PSTR ("</div>\n"));    // tic50r

  WSContentSend_P (PSTR ("</div>\n"));    // tic

  // if not minimized
  if (teleinfo_config.arr_main[TIC_DISPLAY_TEMPO])
  {
    // daily forecast color
    WSContentSend_P (PSTR ("<div class='tic'><div class='tic02'></div>"));
    for (day = RTE_DAY_TODAY; day < RTE_DAY_PLUS7; day ++)
    {
      // prepare dot frame
      sprintf_P (str_text, PSTR ("dotn l%u"), rte_tempo_status.arr_day[day].level);
      if (day == RTE_DAY_TODAY) strcat_P (str_text, PSTR (" now"));
      if (rte_tempo_status.arr_day[day].proba == RTE_TEMPO_KNOWN) strcat_P (str_text, PSTR (" rte"));

      // prepare dot text (RTE or probability)
      if (rte_tempo_status.arr_day[day].proba == 0) strcpy_P (str_value, PSTR (""));
      else if (rte_tempo_status.arr_day[day].proba > 100) strcpy_P (str_value, PSTR ("RTE"));
        else sprintf_P (str_value, PSTR ("%u%%"), rte_tempo_status.arr_day[day].proba);

      // display dot
      WSContentSend_P (PSTR ("<span class='%s'>%s</span>"), str_text, str_value);
    }
    WSContentSend_P (PSTR ("</div>\n"));    // tic

    // day name
    WSContentSend_P (PSTR ("<div class='tic'><div class='tic02'></div>"));
    for (day = RTE_DAY_TODAY; day < RTE_DAY_PLUS7; day ++)
    {
      level = (RtcTime.day_of_week + day + 5) % 7;

      strcpy_P (str_text, PSTR (D_DAY3LIST));
      if (day == RTE_DAY_TODAY) strcpy_P (str_value, PSTR (""));
        else strlcpy (str_value, str_text + level * 3, 4);

      // display day
      sprintf_P (str_text, PSTR ("day"), level);
      if (day == RTE_DAY_TODAY) strcat_P (str_text, PSTR (" now"));

      WSContentSend_P (PSTR ("<span class='%s'>%s</span>"), str_text, str_value);
    }
    WSContentSend_P (PSTR ("</div>\n"));    // tic
  }

  // end of tempo data
  WSContentSend_P (PSTR ("</div>\n"));    // sec
}

void TeleinfoRteWebSensorPointe ()
{
  char str_text[64];

  // check display parameters
  if (!rte_config.pointe.enabled) return;
  if (!rte_config.pointe.display) return;
 
  // header of pointe data
  TeleinfoRteGetDelay (rte_update.time_pointe, str_text, sizeof (str_text));
  WSContentSend_P (PSTR ("<div class='sec' title='%s'>\n"), str_text);

  // main display
  WSContentSend_P (PSTR ("<div class='tic ticb'><div class='tic48l'>Pointe</div><div class='tic50r tichead'><span class='dots l%u'>Aujourd.</span><span class='dots l%u'>Demain</span></div></div>\n"), rte_pointe_status.arr_day[RTE_DAY_TODAY], rte_pointe_status.arr_day[RTE_DAY_TOMORROW]);

  // end of pointe data
  WSContentSend_P (PSTR ("</div>\n")); 
}

void TeleinfoRteWebSensorEcowatt ()
{
  bool     display, slot_now;
  uint8_t  hour, day, level, last_level;
  uint16_t width;
  char     str_color[8];
  char     str_text[64];

  // check display parameters
  if (!rte_config.ecowatt.enabled) return;
  if (!rte_config.ecowatt.display) return;

  // header of ecowatt data
  TeleinfoRteGetDelay (rte_update.time_ecowatt, str_text, sizeof (str_text));
  WSContentSend_P (PSTR ("<div class='sec' title='%s' onclick=\"onClickMain(\'%s?%s=%u\')\">\n"), str_text, PSTR (TIC_PAGE_DISPLAY), PSTR (CMND_TIC_MAIN), TIC_DISPLAY_ECOWATT);

  // title display
  level = rte_ecowatt_status.arr_day[RTE_ECOWATT_DAY_TODAY].arr_hvalue[RtcTime.hour];
  GetTextIndexed (str_text, sizeof (str_text), level, kRteEcowattJSON);
  WSContentSend_P (PSTR ("<div class='tic ticb'><div class='tic48l'>Ecowatt</div><div class='tic50r tichead'><span class='dots e%u'>%s</span></div></div>\n"), level, str_text);

  // if ecowatt signal has been received previously
  if (teleinfo_config.arr_main[TIC_DISPLAY_ECOWATT] && (rte_ecowatt_status.arr_day[0].month != 0))
  {
    // loop thru days
    for (day = RTE_ECOWATT_DAY_TODAY; day < RTE_ECOWATT_DAY_MAX; day ++)
    {
      // display day
      TeleinfoDriverCalendarGetDate (day, str_text, sizeof (str_text));
      WSContentSend_P (PSTR ("<div class='tic'><div class='tic16l'>%s</div>"), str_text);

      // init daily data
      width = 0;
      last_level = RTE_ECOWATT_LEVEL_CARBONFREE;
      str_text[0] = 0;
      str_color[0] = 0;

      // display hourly slots
      for (hour = 0; hour < 24; hour ++)
      {
        // get current slot
        slot_now = ((day == RTE_ECOWATT_DAY_TODAY) && (hour == RtcTime.hour));

        // get slot data
        level = rte_ecowatt_status.arr_day[day].arr_hvalue[hour];

        // check if previous segment should be displayed
        display = ((hour < 2) || (hour == 23));
        if (!display) display = (last_level != level);
        if (!display) display = (slot_now || ((day == RTE_ECOWATT_DAY_TODAY) && (hour == RtcTime.hour + 1)));
        if (display)
        {
          // display previous slot
          if (hour > 0) WSContentSend_P (PSTR ("<div class='%s' style='width:%u.%u%%;'>%s</div>"), str_text, width * TIC_CALENDAR_SLOT / 10, width * TIC_CALENDAR_SLOT % 10, str_color);

          // set current slot
          sprintf_P (str_text, PSTR ("eco%u"), level);
          if (slot_now) strcat_P (str_text, PSTR (" cald"));
          if (hour == 0) strcat_P (str_text, PSTR (" calf"));
            else if (hour == 23) strcat_P (str_text, PSTR (" call"));
          if (slot_now) strcpy_P (str_color, PSTR ("⚪"));
            else str_color[0] = 0;

          // reset width and save last data
          width = 0;
          last_level = level;
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

  // end of ecowatt data
  WSContentSend_P (PSTR ("</div>\n")); 
}

// append presence forecast to main page
void TeleinfoRteWebSensor ()
{
  // check validity
  if (!RtcTime.valid) return;

  // tempo
  TeleinfoRteWebSensorTempo ();

  // pointe
  TeleinfoRteWebSensorPointe ();

  // ecowatt 
  TeleinfoRteWebSensorEcowatt ();
}

#endif  // USE_WEBSERVER

/***************************************\
 *              Interface
\***************************************/

bool XdrvTeleinfoRte (const uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
    case FUNC_COMMAND:
      result = DecodeCommand (kRteCommands, RteCommand);
      if (!result) result = DecodeCommand (kEcowattCommands, EcowattCommand);
      if (!result) result = DecodeCommand (kPointeCommands,  PointeCommand );
      if (!result) result = DecodeCommand (kTempoCommands,   TempoCommand  );
      break;

    case FUNC_INIT:
      TeleinfoRteInit ();
      break;

    case FUNC_SAVE_BEFORE_RESTART:
      TeleinfoRteSaveData ();
      break;

    case FUNC_EVERY_SECOND:
      TeleinfoRteEverySecond ();
      break;

    case FUNC_JSON_APPEND:
      if (TasmotaGlobal.tele_period == 0) TeleinfoRteTeleperiod ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      TeleinfoRteWebSensor ();
      break;

    case FUNC_WEB_ADD_MAIN_BUTTON:
      TeleinfoRteWebMainButton ();
      break;
#endif    // USE_WEBSERVER
  }

  return result;
}

#endif    // USE_TELEINFO_RTE
#endif    // USE_TELEINFO
#endif    // ESP32
