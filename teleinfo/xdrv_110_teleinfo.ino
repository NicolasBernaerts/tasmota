/*
  xdrv_110_teleinfo.ino - France Teleinfo energy sensor support for Tasmota devices

  Copyright (C) 2019-2024  Nicolas Bernaerts

  Version history :
    24/01/2024 - v1.0 - Creation (split from xnrg_15_teleinfo.ino)
    05/02/2024 - v1.1 - Use FUNC_SHOW_SENSOR & FUNC_JSON_APPEND for JSON publication
    13/01/2024 - v2.0 - Complete rewrite of Contrat and Period management
                        Add Emeraude 2 meter management
                        Add calendar and virtual relay management
                        Activate serial reception when NTP time is ready
                        Change MQTT publication and data reception handling to minimize errors
                        Handle Linky supplied Winky with deep sleep 
                        Support various temperature sensors
                        Add Domoticz topics publication
    03/01/2024 - v2.1 - Add alert management thru STGE
    15/01/2024 - v2.2 - Add support for Denky (thanks to C. Hallard prototype)
                        Add Emeraude 2 meter management
                        Add calendar and virtual relay management
    25/02/2024 - v3.0 - Complete rewrite of Contrat and Period management
                        Activate serial reception when NTP time is ready
                        Change MQTT publication and data reception handling to minimize errors
                        Support various temperature sensors
                        Add Domoticz topics publication (idea from Sebastien)
                        Add support for Wenky with deep sleep (thanks to C. Hallard prototype)
                        Lots of bug fixes (thanks to B. Monot and Sebastien)
    05/03/2024 - v14.0 - Removal of all float and double calculations
    27/03/2024 - v14.1 - Section COUNTER renamed as CONTRACT with addition of contract data
    21/04/2024 - v14.3 - Add Homie integration
    21/05/2024 - v14.4 - Group all sensor data in a single frame
                         Publish Teleinfo raw data under /TIC instead of /SENSOR
    01/06/2024 - v14.5 - Add contract auto-discovery for unknown Standard contracts
    28/06/2024 - v14.6 - Change in calendar JSON (for compliance)
                         Add counter serial number in CONTRACT:serial
                         Add global conso counter in CONTRACT:CONSO
                         Remove all String for ESP8266 stability
    30/06/2024 - v14.7 - Add virtual and physical reception status LED (WS2812)
                         Add commands full and noraw (compatibility with official version)
                         Always publish CONTRACT data with METER and PROD
    19/08/2024 - v14.8 - Increase ESP32 reception buffer to 8192 bytes
                         Redesign of contract management and auto-discovery
                         Rewrite of periods management by meter type
                         Add contract change detection on main page
                         Optimisation of serial reception to minimise errors
                         Add command 'tic' to publish raw TIC data
                         Correct CONTRACT/tday and tmrw publication bug
    05/01/2025 - v14.9 - Add indexes and totals to InfluxDB data
                         Update color from RTE according to contract type (Tempo or EJP)
                         
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

// declare teleinfo energy driver
#define XDRV_110                    110

// TIC reception
#define TIC_LINE_SIZE               128       // maximum size of a received TIC line
#define TIC_CODE_SIZE               16        // maximum size of a period code
#define TIC_KEY_MAX                 12        // maximum size of a TIC etiquette

#ifdef ESP32
  #define TIC_BUFFER_MAX            16384     // size of reception buffer (15 sec. at 9600 bps)
  #define TIC_LINE_QTY              74        // maximum number of lines handled in a TIC message
  #define TIC_DATA_MAX              112       // maximum size of a TIC donnee
#else
  #define TIC_BUFFER_MAX            512       // size of reception buffer (3.5 sec. at 1200 bps)
  #define TIC_LINE_QTY              56        // maximum number of lines handled in a TIC message
  #define TIC_DATA_MAX              24        // maximum size of a TIC donnee
#endif    // ESP32

// teleinfo constant
#define TIC_PHASE_MAX               3         // maximum number of phases

#define TIC_POINTE_MAX              3         // maximum number of pointe dates
#define TIC_INDEX_MAX               14        // maximum number of indexes per contract
#define TIC_VOLTAGE                 230       // default voltage provided
#define TIC_VOLTAGE_MAXIMUM         260       // maximum voltage for value to be acceptable
#define TIC_VOLTAGE_MINIMUM         100       // minimum voltage for value to be acceptable
#define TIC_VOLTAGE_REF             200       // voltage reference for max power calculation
#define TIC_PERCENT_MIN             1         // minimum acceptable percentage of energy contract
#define TIC_PERCENT_MAX             200       // maximum acceptable percentage of energy contract
#define TIC_LIVE_DEFAULT            3         // Live update every 3 seconds by default
#define TIC_RTC_TIMEOUT_MAINS       60        // RTC setup timeout before setting from meter when on mains
#define TIC_RTC_TIMEOUT_BATTERY     15        // RTC setup timeout before setting from meter when on battery

// publication constants
#define TIC_MESSAGE_MIN             2         // minimum number of messages to publish data
#define TIC_MESSAGE_DELAY           500       // delay between messages (to avoid serial reception errors)

#define TIC_COSPHI_DEFAULT          850       // default start value for cosphi
#define TIC_COSPHI_SAMPLE           12        // number of samples to calculate cosphi (minimum = 4)
#define TIC_COSPHI_PAGE             20         // number of previous cosphi values

// graph default and boundaries
#define TIC_GRAPH_INC_VOLTAGE       5
#define TIC_GRAPH_MIN_VOLTAGE       235
#define TIC_GRAPH_DEF_VOLTAGE       240       // default voltage maximum in graph
#define TIC_GRAPH_MAX_VOLTAGE       265
#define TIC_GRAPH_INC_POWER         3000
#define TIC_GRAPH_MIN_POWER         3000
#define TIC_GRAPH_DEF_POWER         6000      // default power maximum consumption in graph
#define TIC_GRAPH_MAX_POWER         150000
#define TIC_GRAPH_INC_WH_HOUR       2
#define TIC_GRAPH_MIN_WH_HOUR       2
#define TIC_GRAPH_DEF_WH_HOUR       2         // default max hourly active power consumption in year graph
#define TIC_GRAPH_MAX_WH_HOUR       50
#define TIC_GRAPH_INC_WH_DAY        10
#define TIC_GRAPH_MIN_WH_DAY        10
#define TIC_GRAPH_DEF_WH_DAY        10         // default max daily active power consumption in year graph
#define TIC_GRAPH_MAX_WH_DAY        200
#define TIC_GRAPH_INC_WH_MONTH      100
#define TIC_GRAPH_MIN_WH_MONTH      100
#define TIC_GRAPH_DEF_WH_MONTH      100       // default max monthly active power consumption in year graph
#define TIC_GRAPH_MAX_WH_MONTH      50000

// timeout and delays
#define TELEINFO_RECEPTION_TIMEOUT  3000      // timeout for TIC frame reception (in ms)
#define TELEINFO_PREAVIS_TIMEOUT    15        // timeout to reset preavis flag (in sec.)

// RGB limits
#define TIC_RGB_RED_MAX             208
#define TIC_RGB_GREEN_MAX           176

// colors for conso and prod
#define TIC_COLOR_PROD              "#1c0"
#define TIC_COLOR_PROD_PREV         "#160"
#define TIC_COLOR_CONSO             "#6cf"
#define TIC_COLOR_CONSO_PREV        "#069"

// commands : MQTT
#define CMND_TIC_RATE               "rate"
#define CMND_TIC_POLICY             "policy"
#define CMND_TIC_METER              "meter"
#define CMND_TIC_ENERGY             "energy"
#define CMND_TIC_TIC                "tic"
#define CMND_TIC_LIVE               "live"
#define CMND_TIC_SKIP               "skip"
#define CMND_TIC_SENSOR             "sensor"
#define CMND_TIC_BRIGHT             "bright"
#define CMND_TIC_CALENDAR           "calendar"
#define CMND_TIC_RELAY              "relay"
#define CMND_TIC_DELTA              "delta"
#define CMND_TIC_TRIGGER            "trigger"

#define CMND_TIC_DOMO               "domo"
#define CMND_TIC_HASS               "hass"
#define CMND_TIC_HOMIE              "homie"
#define CMND_TIC_THINGSBOARD        "things"
#define CMND_TIC_INFLUXDB           "influx"

// configuration file
#define CMND_TIC_MAX_V              "maxv"
#define CMND_TIC_MAX_VA             "maxva"
#define CMND_TIC_MAX_KWH_HOUR       "maxhour"
#define CMND_TIC_MAX_KWH_DAY        "maxday"
#define CMND_TIC_MAX_KWH_MONTH      "maxmonth"

#define CMND_TIC_TODAY_CONSO        "tday-conso"
#define CMND_TIC_TODAY_PROD         "tday-prod"
#define CMND_TIC_YESTERDAY_CONSO    "yday-conso"
#define CMND_TIC_YESTERDAY_PROD     "yday-prod"

#define CMND_TIC_CONTRACT_INDEX     "index"
#define CMND_TIC_CONTRACT_NAME      "name"
#define CMND_TIC_CONTRACT_PERIOD    "period"

// commands : Web
#define CMND_TIC_ETH                "eth"
#define CMND_TIC_PHASE              "phase"
#define CMND_TIC_PERIOD             "period"
#define CMND_TIC_HISTO              "histo"
#define CMND_TIC_HOUR               "hour"
#define CMND_TIC_DAY                "day"
#define CMND_TIC_MONTH              "month"
#define CMND_TIC_DATA               "data"
#define CMND_TIC_DISPLAY            "display"
#define CMND_TIC_PREV               "prev"
#define CMND_TIC_NEXT               "next"
#define CMND_TIC_MINUS              "minus"
#define CMND_TIC_PLUS               "plus"

// interface strings
#define TIC_MESSAGE                 "Message"
#define TIC_GRAPH                   "Graph"

// configuration file
#define TIC_FILE_CFG                "/teleinfo.cfg"

// web URL
#define TIC_ICON_LINKY_PNG          "/linky.png"
#define TIC_PAGE_TIC_CFG            "/tic.cfg"
#define TIC_PAGE_TIC_MSG            "/tic"
#define TIC_PAGE_TIC_UPD            "/tic.upd"

// default LED brightness
#define TIC_BRIGHT_DEFAULT          50        // default LED brightness

// EnergyConfig commands
const char kTeleinfoEnergyCommands[] PROGMEM =    "historique"   "|"   "standard"   "|"   "noraw"   "|"   "full"   "|"   "period"   "|"   "live"   "|"   "skip"   "|"   "bright"   "|"   "percent"   "|"   "stats"   "|"   "error"   "|"   "reset"   "|"    "automode"  "|"   "calraz"   "|"   "calhexa"   "|"    "trigger"   "|" CMND_TIC_POLICY "|" CMND_TIC_METER "|" CMND_TIC_CALENDAR "|" CMND_TIC_RELAY "|" CMND_TIC_MAX_V "|" CMND_TIC_MAX_VA "|" CMND_TIC_MAX_KWH_HOUR "|" CMND_TIC_MAX_KWH_DAY "|" CMND_TIC_MAX_KWH_MONTH;
enum TeleinfoEnergyCommand                   { TIC_CMND_HISTORIQUE, TIC_CMND_STANDARD, TIC_CMND_NORAW, TIC_CMND_FULL, TIC_CMND_PERIOD, TIC_CMND_LIVE, TIC_CMND_SKIP, TIC_CMND_BRIGHT, TIC_CMND_PERCENT, TIC_CMND_STATS, TIC_CMND_ERROR, TIC_CMND_RESET, TIC_CMND_AUTOMODE, TIC_CMND_CALRAZ, TIC_CMND_CALHEXA,  TIC_CMND_TRIGGER,  TIC_CMND_POLICY  ,  TIC_CMND_METER  ,  TIC_CMND_CALENDAR  ,  TIC_CMND_RELAY  ,  TIC_CMND_MAX_V  ,  TIC_CMND_MAX_VA  ,  TIC_CMND_MAX_KWH_HOUR  ,  TIC_CMND_MAX_KWH_DAY  ,  TIC_CMND_MAX_KWH_MONTH };

// Data publication commands
static const char kTeleinfoDriverCommands[]  PROGMEM =          "|tic";
void (* const TeleinfoDriverCommand[])(void) PROGMEM = { &CmndTeleinfoDriverTIC };

// Data diffusion policy
enum TeleinfoMessagePolicy { TIC_POLICY_TELEMETRY, TIC_POLICY_DELTA, TIC_POLICY_MESSAGE, TIC_POLICY_MAX };
const char kTeleinfoMessagePolicy[] PROGMEM = "A chaque Télémétrie|Evolution de ±|A chaque message reçu";

// config : param
enum TeleinfoConfigKey                          { TIC_CONFIG_BRIGHT , TIC_CONFIG_MAX_HOUR     , TIC_CONFIG_MAX_DAY     , TIC_CONFIG_MAX_MONTH     , TIC_CONFIG_TODAY_CONSO , TIC_CONFIG_TODAY_PROD , TIC_CONFIG_YESTERDAY_CONSO , TIC_CONFIG_YESTERDAY_PROD, TIC_CONFIG_MAX };    // config parameters
const long arrTeleinfoConfigDefault[] PROGMEM = { TIC_BRIGHT_DEFAULT, TIC_GRAPH_DEF_WH_HOUR   , TIC_GRAPH_DEF_WH_DAY   , TIC_GRAPH_DEF_WH_MONTH   , 0                      , 0                     , 0                          , 0 };                                            // config default values
const char kTeleinfoConfigKey[]       PROGMEM =    CMND_TIC_BRIGHT "|" CMND_TIC_MAX_KWH_HOUR "|" CMND_TIC_MAX_KWH_DAY "|" CMND_TIC_MAX_KWH_MONTH "|" CMND_TIC_TODAY_CONSO "|" CMND_TIC_TODAY_PROD "|" CMND_TIC_YESTERDAY_CONSO "|" CMND_TIC_YESTERDAY_PROD;                       // config keys

// config : contract
enum TeleinfoContractKey { TIC_CONTRACT_INDEX, TIC_CONTRACT_NAME, TIC_CONTRACT_PERIOD, TIC_CONTRACT_MAX };                                                                                                                                 // contract parameters
const char kTeleinfoContractKey[] PROGMEM = CMND_TIC_CONTRACT_INDEX "|" CMND_TIC_CONTRACT_NAME "|" CMND_TIC_CONTRACT_PERIOD;                                                                                       // contract keys

// published data 
enum TeleinfoPublish { TIC_PUB_CONNECT,
                       TIC_PUB_CONTRACT, TIC_PUB_CONTRACT_NAME, TIC_PUB_CONTRACT_SERIAL,
                       TIC_PUB_CALENDAR, TIC_PUB_CALENDAR_PERIOD, TIC_PUB_CALENDAR_COLOR, TIC_PUB_CALENDAR_HOUR, TIC_PUB_CALENDAR_TODAY, TIC_PUB_CALENDAR_TOMRW, 
                       TIC_PUB_PROD, TIC_PUB_PROD_P, TIC_PUB_PROD_W, TIC_PUB_PROD_C, TIC_PUB_PROD_YTDAY, TIC_PUB_PROD_TODAY,  
                       TIC_PUB_CONSO, TIC_PUB_CONSO_P, TIC_PUB_CONSO_W, TIC_PUB_CONSO_C, TIC_PUB_CONSO_U, TIC_PUB_CONSO_I, TIC_PUB_CONSO_YTDAY, TIC_PUB_CONSO_TODAY, 
                       TIC_PUB_PH1, TIC_PUB_PH1_U, TIC_PUB_PH1_I, TIC_PUB_PH1_P, TIC_PUB_PH1_W, 
                       TIC_PUB_PH2, TIC_PUB_PH2_U, TIC_PUB_PH2_I, TIC_PUB_PH2_P, TIC_PUB_PH2_W,
                       TIC_PUB_PH3, TIC_PUB_PH3_U, TIC_PUB_PH3_I, TIC_PUB_PH3_P, TIC_PUB_PH3_W,   
                       TIC_PUB_RELAY, TIC_PUB_RELAY_DATA,
                       TIC_PUB_TOTAL, TIC_PUB_TOTAL_PROD, TIC_PUB_TOTAL_CONSO, TIC_PUB_TOTAL_INDEX, 
#ifdef USE_RTE_SERVER
                       TIC_PUB_RTE_TEMPO_TODAY, TIC_PUB_RTE_TEMPO_TOMRW, TIC_PUB_RTE_POINTE_TODAY, TIC_PUB_RTE_POINTE_TOMRW,
#endif  // RTE_SERVER
                       TIC_PUB_MAX, 
                       TIC_PUB_DISCONNECT };

// power calculation modes
enum TeleinfoContractUnit { TIC_UNIT_NONE, TIC_UNIT_KVA, TIC_UNIT_KW, TIC_UNIT_MAX };

// contract periods
#define TIC_DAY_SLOT_MAX                48
enum TeleinfoPeriodDay                  { TIC_DAY_TODAY  , TIC_DAY_TMROW, TIC_DAY_AFTER, TIC_DAY_MAX};
const char kTeleinfoPeriodDay[] PROGMEM = "Aujourd'hui" "|"  "Demain"  "|" "Après-dem.";

// contract hours
const char kTeleinfoHourShort[] PROGMEM = "HC|HP";
const char kTeleinfoHourLabel[] PROGMEM = "Creuse|Pleine";

// contract periods
enum TeleinfoLevel  { TIC_LEVEL_NONE, TIC_LEVEL_BLUE, TIC_LEVEL_WHITE, TIC_LEVEL_RED, TIC_LEVEL_MAX };
const char kTeleinfoLevelLabel[]  PROGMEM = "Inconnu|Bleu|Blanc|Rouge";
const char kTeleinfoLevelDot[]    PROGMEM = "🟢|🔵|⚪|🔴";
const char kTeleinfoLevelCalDot[] PROGMEM = "⚪|⚪|⚫|⚪";
const char kTeleinfoLevelCalRGB[] PROGMEM = "#252525|#06b|#ccc|#b00";

// preavis levels
enum TeleinfoPreavisLevel {TIC_PREAVIS_NONE, TIC_PREAVIS_WARNING, TIC_PREAVIS_ALERT, TIC_PREAVIS_DANGER, TIC_PREAVIS_MAX};

// serial port management
enum TeleinfoSerialStatus { TIC_SERIAL_INIT, TIC_SERIAL_GPIO, TIC_SERIAL_SPEED, TIC_SERIAL_FAILED, TIC_SERIAL_ACTIVE, TIC_SERIAL_STOPPED, TIC_SERIAL_MAX };

// etapes de reception d'un message TIC
enum TeleinfoReceptionStatus { TIC_RECEPTION_NONE, TIC_RECEPTION_MESSAGE, TIC_RECEPTION_LINE, TIC_RECEPTION_MAX };

// week days name for history
static const char kWeekdayNames[] = D_DAY3LIST;

// Led status                                          not used          red                blue               yellow            no light             green             green
enum TicLedStatus                                   { TIC_LED_STEP_NONE, TIC_LED_STEP_WIFI, TIC_LED_STEP_MQTT, TIC_LED_STEP_TIC, TIC_LED_STEP_NODATA, TIC_LED_STEP_ERR, TIC_LED_STEP_OK, TIC_LED_STEP_MAX };    // LED status
const uint16_t arrTicLedOn[TIC_LED_STEP_MAX]      = { 0,                 1000,              1000,              1000,             0,                   100,              100           };                        // led ON (in ms)
const uint16_t arrTicLedOff[TIC_LED_STEP_MAX]     = { 1000,              0,                 0,                 0,                1000,                900,              2900          };                        // led OFF (in ms)
const uint8_t arrTicLedColor[TIC_LED_STEP_MAX][3] = { {0,0,0},           {255,0,0},         {0,0,255},         {255,255,0},      {0,0,0},             {0,255,0},        {0,255,0}     };                        // led color RED
enum TicLedPower { TIC_LED_PWR_OFF, TIC_LED_PWR_ON, TIC_LED_PWR_SLEEP, TIC_LED_PWR_MAX };       // LED power

// -------------------
//  TELEINFO Protocol
// -------------------

// Meter mode
enum TeleinfoMode                      { TIC_MODE_UNKNOWN, TIC_MODE_HISTORIC, TIC_MODE_STANDARD, TIC_MODE_PMEPMI, TIC_MODE_EMERAUDE, TIC_MODE_JAUNE, TIC_MODE_MAX };
const char kTeleinfoModeName[] PROGMEM =    "Inconnu"   "|"   "Historique" "|"    "Standard"  "|"   "PME-PMI"  "|"    "Emeraude"  "|"   "Jaune";
const char kTeleinfoModeIcon[] PROGMEM =       "❔"     "|"       "🇭"     "|"       "🇸"     "|"      "🇵"    "|"       "🇪"     "|"    "🇯";

// list of etiquettes according to meter mode
enum TicEtiquetteUnknown                   { TIC_UKN_NONE, TIC_UKN_ADCO, TIC_UKN_ADSC, TIC_UKN_ADS, TIC_UKN_CONTRAT, TIC_UKN_JAUNE, TIC_UKN_MAX };
const char kTicEtiquetteUnknown[]  PROGMEM =                 "|ADCO"       "|ADSC"       "|ADS"       "|CONTRAT"       "|JAUNE";

enum TicEtiquetteHisto                     { TIC_HIS_NONE = TIC_UKN_MAX, TIC_HIS_ADCO, TIC_HIS_OPTARIF, TIC_HIS_ISOUSC, TIC_HIS_BASE, TIC_HIS_HCHC, TIC_HIS_HCHP, TIC_HIS_EJPHN, TIC_HIS_EJPHPM, TIC_HIS_BBRHCJB, TIC_HIS_BBRHPJB, TIC_HIS_BBRHCJW, TIC_HIS_BBRHPJW, TIC_HIS_BBRHCJR, TIC_HIS_BBRHPJR,  TIC_HIS_PEJP, TIC_HIS_PTEC, TIC_HIS_IINST, TIC_HIS_IINST1, TIC_HIS_IINST2, TIC_HIS_IINST3, TIC_HIS_ADPS, TIC_HIS_PAPP, TIC_HIS_DEMAIN, TIC_HIS_ADIR1, TIC_HIS_ADIR2, TIC_HIS_ADIR3, TIC_HIS_MAX };
const char kTicEtiquetteHisto[]    PROGMEM =                               "|ADCO"       "|OPTARIF"       "|ISOUSC"       "|BASE"       "|HCHC"       "|HCHP"       "|EJPHN"       "|EJPHPM"      "|BBRHCJB"       "|BBRHPJB"        "|BBRHCJW"       "|BBRHPJW"       "|BBRHCJR"       "|BBRHPJR"       "|PEJP"        "|PTEC"       "|IINST"       "|IINST1"       "|IINST2"       "|IINST3"       "|ADPS"      "|PAPP"         "|DEMAIN"        "|ADIR1"       "|ADIR2"       "|ADIR3";
enum TicEtiquetteStandard                  { TIC_STD_NONE = TIC_HIS_MAX, TIC_STD_ADSC,  TIC_STD_DATE, TIC_STD_NGTF, TIC_STD_LTARF, TIC_STD_EAST, TIC_STD_EASF01, TIC_STD_EASF02, TIC_STD_EASF03, TIC_STD_EASF04, TIC_STD_EASF05, TIC_STD_EASF06, TIC_STD_EASF07, TIC_STD_EASF08, TIC_STD_EASF09, TIC_STD_EASF10, TIC_STD_EAIT, TIC_STD_IRMS1, TIC_STD_IRMS2, TIC_STD_IRMS3, TIC_STD_URMS1, TIC_STD_URMS2, TIC_STD_URMS3, TIC_STD_PREF, TIC_STD_PCOUP, TIC_STD_SINSTS, TIC_STD_SINSTS1, TIC_STD_SINSTS2, TIC_STD_SINSTS3, TIC_STD_SINSTI, TIC_STD_UMOY1, TIC_STD_UMOY2, TIC_STD_UMOY3, TIC_STD_STGE, TIC_STD_DPM1, TIC_STD_DPM2, TIC_STD_DPM3, TIC_STD_FPM1, TIC_STD_FPM2, TIC_STD_FPM3, TIC_STD_RELAIS, TIC_STD_PJOURF1, TIC_STD_PPOINTE, TIC_STD_MAX };
const char kTicEtiquetteStandard[] PROGMEM =                               "|ADSC"        "|DATE"       "|NGTF"       "|LTARF"       "|EAST"       "|EASF01"       "|EASF02"       "|EASF03"       "|EASF04"       "|EASF05"       "|EASF06"       "|EASF07"       "|EASF08"       "|EASF09"       "|EASF10"       "|EAIT"       "|IRMS1"       "|IRMS2"       "|IRMS3"       "|URMS1"        "|URMS2"      "|URMS3"       "|PREF"       "|PCOUP"       "|SINSTS"       "|SINSTS1"       "|SINSTS2"       "|SINSTS3"       "|SINSTI"       "|UMOY1"       "|UMOY2"       "|UMOY3"       "|STGE"       "|DPM1"       "|DPM2"       "|DPM3"       "|FPM1"       "|FPM2"       "|FPM3"       "|RELAIS"        "|PJOURF+1"      "|PPOINTE";

enum TicEtiquettePmePmi                    { TIC_PME_NONE = TIC_STD_MAX, TIC_PME_ADS, TIC_PME_MESURES1, TIC_PME_DATE,  TIC_PME_EAS,  TIC_PME_EAPPS, TIC_PME_PTCOUR1, TIC_PME_EAPS, TIC_PME_PS, TIC_PME_PREAVIS, TIC_PME_MAX };
const char kTicEtiquettePmePmi[]   PROGMEM =                               "|ADS"       "|MESURES1"       "|DATE"        "|EA_s"       "|EAPP_s"      "|PTCOUR1"       "|EAP_s"      "|PS"      "|PREAVIS";

enum TicEtiquetteEmeraude                  { TIC_EME_NONE = TIC_PME_MAX, TIC_EME_CONTRAT, TIC_EME_APPLI, TIC_EME_DATECOUR, TIC_EME_EA, TIC_EME_ERP, TIC_EME_PTCOUR, TIC_EME_PREAVIS, TIC_EME_EAPP, TIC_EME_EAPPM, TIC_EME_EAPHCE, TIC_EME_EAPHCH, TIC_EME_EAPHH, TIC_EME_EAPHCD, TIC_EME_EAPHD, TIC_EME_EAPJA, TIC_EME_EAPHPE, TIC_EME_EAPHPH, TIC_EME_EAPHPD, TIC_EME_EAPSCM, TIC_EME_EAPHM, TIC_EME_EAPDSM, TIC_EME_PSP, TIC_EME_PSPM, TIC_EME_PSHPH, TIC_EME_PSHPD, TIC_EME_PSHCH, TIC_EME_PSHCD, TIC_EME_PSHPE, TIC_EME_PSHCE, TIC_EME_PSJA, TIC_EME_PSHH, TIC_EME_PSHD, TIC_EME_PSHM, TIC_EME_PSDSM, TIC_EME_PSSCM, TIC_EME_U10MN, TIC_EME_MAX };
const char kTicEtiquetteEmeraude[] PROGMEM =                               "|CONTRAT"       "|Appli"       "|DATECOUR"       "|EA"       "|ERP"       "|PTCOUR"       "|PREAVIS"       "|EApP"       "|EApPM"       "|EApHCE"       "|EApHCH"       "|EApHH"       "|EApHCD"       "|EApHD"       "|EApJA"       "|EApHPE"       "|EApHPH"       "|EApHPD"       "|EApSCM"       "|EApHM"       "|EApDSM"       "|PSP"       "|PSPM"       "|PSHPH"       "|PSHPD"       "|PSHCH"       "|PSHCD"       "|PSHPE"       "|PSHCE"       "|PSJA"       "|PSHH"       "|PSHD"       "|PSHM"       "|PSDSM"       "|PSSCM"       "|U10MN";

enum TicEtiquetteJaune                     { TIC_JAU_NONE = TIC_EME_MAX, TIC_JAU_JAUNE, TIC_JAU_ENERG,  TIC_JAU_MAX };
const char kTicEtiquetteJaune[]    PROGMEM =                               "|JAUNE"       "|ENERG";

// arrays of etiquettes and delta in global index
const uint8_t arrTicEtiquetteDelta[]          = { TIC_UKN_NONE,  TIC_HIS_NONE, TIC_STD_NONE, TIC_PME_NONE, TIC_EME_NONE, TIC_JAU_NONE };
const char *const arr_kTicEtiquette[] PROGMEM = { kTicEtiquetteUnknown,     // contract unknown
                                                  kTicEtiquetteHisto,       // Historique
                                                  kTicEtiquetteStandard,    // Standard
                                                  kTicEtiquettePmePmi,      // PME/PMI
                                                  kTicEtiquetteEmeraude,    // Emeraude
                                                  kTicEtiquetteJaune };     // Jaune

// Manufacturers
// -------------<

const char kTicManufacturer00to09[] PROGMEM = "|Crouzet / Monetel|Sagem / Sagemcom|Schlumberger / Actaris / Itron|Landis & Gyr / Siemens Metering|Sauter / Stepper Energie / Zellweger|ITRON|MAEC|Matra-Chauvin Arnoux / Enerdis|Faure-Herman";
const char kTicManufacturer10to19[] PROGMEM = "Sevme / SIS|Magnol / Elster / Honeywell|Gaz Thermique||Ghielmetti / Dialog E.S. / Micronique|MECELEC|Legrand / Baco|SERD-Schlumberger|Schneider / Merlin Gerin / Gardy|General Electric / Power Control / ABB";
const char kTicManufacturer20to29[] PROGMEM = "Nuovo Pignone / Dresser|SCLE|EDF|GDF / GDF-SUEZ|Hager – General Electric|Delta-Dore|RIZ|Iskraemeco|GMT|Analog Device";
const char kTicManufacturer30to39[] PROGMEM = "Michaud|Hexing Electrical Co Ltd|Siame|Larsen & Toubro Ltd|Elster / Honeywell|Electronic Afzar Azma|Advanced Electronic Co Ltd|AEM|Zhejiang Chint Inst. & Meter Co Ltd|ZIV";
const char kTicManufacturer70to79[] PROGMEM = "Landis & Gyr (export)|Stepper Energie France (export)";
const char kTicManufacturer80to89[] PROGMEM = "|Sagem / Sagemcom|Landis & Gyr / Siemens Metering|Elster / Honeywell|Sagem / Sagemcom|ITRON";

// Models
// ------

const char kTicModel00to09[] PROGMEM = "|Bleu mono. BBR,Gen1|Poste HTA/BT,Gen3|||Bleu monophasé,Gen1|Jaune tarif modulable|PRISME ou ICE|Sauter modifié EURIDIS|Bleu triphasé,Gen1";
const char kTicModel10to19[] PROGMEM = "Jaune,Gen2|Bleu mono. FERRARIS|Prisme|||Bleu mono. sans BBR";
const char kTicModel20to29[] PROGMEM = "Bleu mono. ½taux,Gen1|Bleu tri. ½taux,Gen1|Bleu monophasé,Gen2|Bleu mono. ½taux,Gen2||Bleu monophasé,Gen2|Bleu triphasé,Gen2|Bleu tri. ½taux,Gen2|Bleu monophasé,Gen3|Bleu mono. ½taux,Gen3";
const char kTicModel30to39[] PROGMEM = "Bleu triphasé,Gen3|Bleu tri. ½taux,Gen3|Bleu tri. télétotalisation|Jaune branchement direct|ICE 4 quadrants|Trimaran 2P classe 0,2s|PME-PMI BT > 36kva|Prépaiement|HXE34 de HECL tri.";
const char kTicModel40to49[] PROGMEM = "||ACTARIS mono. export|ACTARIS mono. export|ACTARIS tri. export|ACTARIS / AECL tri. export";
const char kTicModel60to69[] PROGMEM = "Linky monophasé 60A,Gen1|Linky monophasé 60A,Gen3|Linky monophasé 90A,Gen1|Linky triphasé 60A,Gen1|Linky monophasé 60A,Gen3|Linky mono. 90A CPL,Gen3||Linky monophasé 90A,Gen1|Linky triphasé 60A,Gen1";
const char kTicModel70to79[] PROGMEM = "Linky monophasé 60A,Gen3|Linky triphasé 60A,Gen3|HXE12K mono. 10-80A 4 tarifs||HXE34K tri. 10-80A 4 tarifs|Linky monophasé 90A,Gen3|Linky triphasé 60A,Gen3";
const char kTicModel80to89[] PROGMEM = "||||||SEI mono. 60A 60Hz,Gen3|SEI tri. 60A 60Hz,Gen3|ACTARIS mono. PLC DSMR2.2|ACTARIS tri. PLC DSMR2.2";
const char kTicModel90to99[] PROGMEM = "Monophasé CPL intégré,Gen1|Triphasé CPL intégré,Gen2|Linky ORES mono. 90A,Gen3|Linky ORES tri. 60A 3fils,Gen3|Linky ORES tri. 60A 4fils,Gen3";

// Contracts & Periods
// --------------------

// Inconnu
const char kTicPeriodUnknown[]    PROGMEM = "";

// Historique Base
const char kTicPeriodHistoBase[]  PROGMEM = "1|1|1|TH..|Toutes Heures";

// Historique Heure Pleine / Heure creuse
const char kTicPeriodHistoHcHp[]  PROGMEM = "1|1|0|HC..|Creuses" "|" "2|1|1|HP..|Pleines";

// Historique EJP
const char kTicPeriodHistoEjp[]   PROGMEM = "1|1|1|HN..|Normale" "|" "2|3|1|PM..|Pointe Mobile";

// Historique Tempo
const char kTicPeriodHistoTempo[] PROGMEM =  "1|1|0|HCJB|Creuses Bleu"  "|" "2|1|1|HPJB|Pleines Bleu" 
                                         "|" "3|2|0|HCJW|Creuses Blanc" "|" "4|2|1|HPJW|Pleines Blanc"
                                         "|" "5|3|0|HCJR|Creuses Rouge" "|" "6|3|1|HPJR|Pleines Rouge";

// Standard Base
const char kTicPeriodStdBase[]    PROGMEM = "1|1|1|BASE|Toutes Heures";

// Standard Heure Pleine / Heure creuse
const char kTicPeriodStdHcHp[]    PROGMEM = "1|1|0|HEURE CREUSE|Creuses" "|" "2|1|1|HEURE PLEINE|Pleines"; 

// Standard Heure Pleine / Heure creuse 12h30
const char kTicPeriodStdHcHp12h[] PROGMEM = "1|1|0|HEURES CREUSES|Creuses" "|" "2|1|1|HEURES PLEINES|Pleines";

// Standard EJP
const char kTicPeriodStdEjp[]     PROGMEM = "1|1|1|HEURE NORMALE|Normale" "|" "2|3|1|HEURE POINTE|Pointe Mobile";

// Standard Tempo 
const char kTicPeriodStdTempo[]   PROGMEM = "1|1|0|HC BLEU|Creuses Bleu"   "|" "2|1|1|HP BLEU|Pleines Bleu"
                                        "|" "3|2|0|HC BLANC|Creuses Blanc" "|" "4|2|1|HP BLANC|Pleines Blanc"
                                        "|" "5|3|0|HC ROUGE|Creuses Rouge" "|" "6|3|1|HP ROUGE|Pleines Rouge";

// PME/PMI 
const char kTicPeriodPmePmi[]    PROGMEM =  "1|3|1|P|Pointe"              "|" "2|3|1|M|Pointe Mobile"
                                        "|" "3|1|0|HH|Hiver"
                                        "|" "4|1|1|HP|Pleines"            "|" "5|1|0|HC|Creuses"
                                        "|" "6|1|1|HPH|Pleines Hiver"     "|" "7|1|0|HCH|Creuses Hiver"
                                        "|" "8|1|1|HPE|Pleines Eté"       "|" "9|1|0|HCE|Creuses Eté"
                                        "|" "10|1|1|HPD|Pleines ½ saison" "|" "11|1|0|HCD|Creuses ½ saison"
                                        "|" "12|1|0|JA|Juillet-Août";

// Emeraude 2 quadrants - A5 Base, A8 Basen A5 EJP, A8 EJP, A8 Modulable
const char kTicPeriodEmeraude[]  PROGMEM = "1|3|1|P|Pointe"          "|" "2|3|1|PM|Pointe Mobile"     
                                       "|" "3|1|1|HPH|Pleines Hiver" "|" "4|1|1|HPD|Pleines ½ saison"
                                       "|" "5|1|0|HCH|Creuses Hiver" "|" "6|1|0|HCD|Creuses ½ saison"
                                       "|" "7|1|1|HPE|Pleines Eté"   "|" "8|1|0|HCE|Creuses Eté"
                                       "|" "9|1|1|JA|Juillet-Aout"   "|" "10|1|1|HH|Hiver" "|" "11|1|1|HD|½ saison"
                                       "|" "12|3|1|HM|Hiver Mobile"  "|" "13|3|1|DSM|½ saison Mobile" "|" "14|3|0|SCM|Creuses Mobile";

// Periods
enum TicContract                      { TIC_C_UNKNOWN, TIC_C_HIS_BASE, TIC_C_HIS_HCHP, TIC_C_HIS_EJP, TIC_C_HIS_TEMPO, TIC_C_STD_BASE,   TIC_C_STD_HCHP    , TIC_C_STD_HCHP12H, TIC_C_STD_EJP, TIC_C_STD_TEMPO, TIC_C_PME_BT4SUP36, TIC_C_PME_BT5SUP36, TIC_C_PME_TVA5_BASE, TIC_C_PME_TVA8_BASE,   TIC_C_PME_TJMU  , TIC_C_PME_TJLU_SD , TIC_C_PME_TJLU_P , TIC_C_PME_TJLU_PH , TIC_C_PME_TJLU_CH , TIC_C_PME_TJEJP, TIC_C_PME_TJEJP_SD, TIC_C_PME_TJEJP_PM, TIC_C_PME_TJEJP_HH, TIC_C_PME_HTA5, TIC_C_PME_HTA8, TIC_C_EME_A5_BASE, TIC_C_EME_A8_BASE, TIC_C_EME_A5_EJP, TIC_C_EME_A8_EJP, TIC_C_EME_A8_MOD, TIC_C_MAX };
const char kTicContractCode[] PROGMEM =             "|"    "TH.."   "|"     "HC.."  "|"    "EJP."  "|"     "BBR"    "|"    "BASE"   "|" "H PLEINE/CREUSE" "|"   "HC-12H30"   "|"     "EJP"  "|"    "TEMPO"   "|"  "BT 4 SUP36"   "|"   "BT 5 SUP36"  "|"   "TV A5 BASE"   "|"   "TV A8 BASE"    "|"     "TJ MU"    "|"    "TJ LU-SD"   "|"    "TJ LU-P"   "|"    "TJ LU-PH"   "|"    "TJ LU-CH"   "|"    "TJ EJP"  "|"    "TJ EJP-SD"  "|"    "TJ EJP-PM"  "|"    "TJ EJP-HH"  "|"   "HTA 5"   "|"   "HTA 8"   "|"    "BASE_A5"   "|"   "BASE_A8"    "|"    "EJP_A5"   "|"   "EJP_A8"    "|"    "MOD";
const char kTicContractName[] PROGMEM =             "|"    "Base"   "|"    "HC/HP"  "|"    "EJP"   "|"    "Tempo"   "|"    "Base"   "|"      "HC/HP"      "|"  "HC/HP 12h30" "|"     "EJP"  "|"    "Tempo"   "|"  "BT>36kVA 4p." "|"  "BT>36kVA 5p." "|"  "Vert A5 Base"  "|"  "Vert A8 Base"   "|"  "Jaune Moyen" "|" "Jaune Long SD" "|" "Jaune Long P" "|" "Jaune Long PH" "|" "Jaune Long CH" "|"  "Jaune EJP" "|"  "Jaune EJP SD" "|"  "Jaune EJP PM" "|"  "Jaune EJP HH" "|"  "HTA 5p."  "|"  "HTA 8p."  "|"    "A5 Base"   "|"   "A8 Base"    "|"    "A5 EJP"   "|"   "A8 EJP"    "|"  "A8 Mod.";

// association between contract index and list of periods
const char *const arr_kTicPeriod[] PROGMEM = { kTicPeriodUnknown,     // TIC_C_UNKNOWN
                                               kTicPeriodHistoBase,   // TIC_C_HIS_BASE
                                               kTicPeriodHistoHcHp,   // TIC_C_HIS_HCHP
                                               kTicPeriodHistoEjp,    // TIC_C_HIS_EJP
                                               kTicPeriodHistoTempo,  // TIC_C_HIS_TEMPO
                                               kTicPeriodStdBase,     // TIC_C_STD_BASE
                                               kTicPeriodStdHcHp,     // TIC_C_STD_HCHP
                                               kTicPeriodStdHcHp12h,  // TIC_C_STD_HCHP12H
                                               kTicPeriodStdEjp,      // TIC_C_STD_EJP
                                               kTicPeriodStdTempo,    // TIC_C_STD_TEMPO
                                               kTicPeriodPmePmi,      // TIC_C_PME_BT4SUP36
                                               kTicPeriodPmePmi,      // TIC_C_PME_BT5SUP36
                                               kTicPeriodPmePmi,      // TIC_C_PME_TVA5_BASE
                                               kTicPeriodPmePmi,      // TIC_C_PME_TVA8_BASE
                                               kTicPeriodPmePmi,      // TIC_C_PME_TJMU
                                               kTicPeriodPmePmi,      // TIC_C_PME_TJLU_SD
                                               kTicPeriodPmePmi,      // TIC_C_PME_TJLU_P
                                               kTicPeriodPmePmi,      // TIC_C_PME_TJLU_PH
                                               kTicPeriodPmePmi,      // TIC_C_PME_TJLU_CH
                                               kTicPeriodPmePmi,      // TIC_C_PME_TJEJP
                                               kTicPeriodPmePmi,      // TIC_C_PME_TJEJP_SD
                                               kTicPeriodPmePmi,      // TIC_C_PME_TJEJP_PM
                                               kTicPeriodPmePmi,      // TIC_C_PME_TJEJP_HH
                                               kTicPeriodPmePmi,      // TIC_C_PME_HTA5
                                               kTicPeriodPmePmi,      // TIC_C_PME_HTA8
                                               kTicPeriodEmeraude,    // TIC_C_EME_A5_BASE
                                               kTicPeriodEmeraude,    // TIC_C_EME_A8_BASE
                                               kTicPeriodEmeraude,    // TIC_C_EME_A5_EJP
                                               kTicPeriodEmeraude,    // TIC_C_EME_A8_EJP
                                               kTicPeriodEmeraude };  // TIC_C_EME_A8_MOD

/****************************************\
 *                 Data
\****************************************/

// teleinfo : configuration
// ------------------------

static struct {
  uint8_t  battery      = 0;                            // device is running on battery
  uint8_t  percent      = 100;                          // maximum acceptable power in percentage of contract power
  uint8_t  policy       = TIC_POLICY_TELEMETRY;         // data publishing policy
  uint8_t  meter        = 1;                            // publish METER & PROD section
  uint8_t  energy       = 0;                            // publish ENERGY section
  uint8_t  tic          = 0;                            // publish TIC topic
  uint8_t  live         = 0;                            // publish LIVE topic
  uint8_t  skip         = 2;                            // number of frames to skip while publishing TIC or LIVE
  uint8_t  calendar     = 1;                            // publish CALENDAR section
  uint8_t  relay        = 1;                            // publish RELAY section
  uint8_t  contract     = 1;                            // publish CONTRACT section
  uint8_t  error        = 0;                            // force display of errors on home page 
  uint8_t  cal_hexa     = 0;                            // flag to set format of period profiles as hexa
  uint8_t  led_period   = 0;                            // adjust LED color according to period color 
  long     prod_trigger = 0;                            // average production power to trigger production relay 
  long     max_volt     = TIC_GRAPH_DEF_VOLTAGE;        // maximum voltage on graph
  long     max_power    = TIC_GRAPH_DEF_POWER;          // maximum power on graph
  long     param[TIC_CONFIG_MAX];                       // generic params
} teleinfo_config;


// teleinfo : calendar
// -------------------

struct tic_cal_day {                            // 52 bytes
  uint32_t date;                                // date in YYYYMMDD00 format
  uint8_t  level;                               // maximum level of the day
  uint8_t  arr_slot[TIC_DAY_SLOT_MAX];              // period slots within the day
};

tic_cal_day teleinfo_calendar[TIC_DAY_MAX];     // today, tomorrow and day after calendar

// teleinfo : current message
// --------------------------

struct tic_line {
  char str_etiquette[TIC_KEY_MAX];              // line etiquette
  char str_donnee[TIC_DATA_MAX];                // line donnee
  char checksum;                                // line checksum
};

struct tic_pointe {
  uint32_t start;                               // start date with slot
  uint32_t stop;                                // stop date with slot
};

struct tic_stge {
  uint8_t  over_load;                           // currently overloaded
  uint8_t  over_volt;                           // voltage overload on one phase
  uint8_t  pointe;                              // current pointe mobile
  uint8_t  preavis;                             // pointe mobile preavis
};

static struct {
  uint8_t  injection = 0;                       // flag to detect injection part of message (Emeraude 4 quadrand)
  uint8_t  error     = 0;                       // error during current message reception
  uint8_t  period    = UINT8_MAX;               // period index in current message
  uint32_t timestamp_last = UINT32_MAX;         // timestamp of last message (ms)
  int      line_idx  = 0;                       // index of current received message line
  int      line_last = 0;                       // number of lines in last message
  int      line_max  = 0;                       // max number of lines in a message
  long     duration  = 1000;                    // average duration of between 2 message (ms)
  char     str_contract[TIC_CODE_SIZE];         // contract name in current message
  char     str_period[TIC_CODE_SIZE];           // period name in current message
  char     str_line[TIC_LINE_SIZE];             // reception buffer for current line
  tic_stge    stge;                             // STGE data
  tic_pointe  arr_pointe[TIC_POINTE_MAX];       // array of pointe dates
  tic_line    arr_line[TIC_LINE_QTY];           // array of lines in current message
  tic_line    arr_last[TIC_LINE_QTY];           // array of lines in last message received
  tic_cal_day cal_default;                      // default daily profile
  tic_cal_day cal_pointe;                       // pointe daily profile
} teleinfo_message;

// teleinfo : contract
// -------------------

struct tic_period {
  uint8_t valid;                                // period validity
  uint8_t level;                                // period level
  uint8_t hchp;                                 // period hp flag
  String  str_code;                             // period code
  String  str_label;                            // period label
};

static struct {
  uint8_t   changed    = 0;                     // flag to indicate that contract has changed
  uint8_t   unit       = TIC_UNIT_NONE;         // default contract unit
  uint8_t   index      = UINT8_MAX;             // actual contract index
  uint8_t   mode       = TIC_MODE_UNKNOWN;      // meter mode (historic, standard)

  uint8_t   period     = UINT8_MAX;             // period - current index
  uint8_t   period_qty = 0;                     // period - number of indexes

  uint8_t   phase      = 1;                     // number of phases
  long      isousc     = 0;                     // contract max current per phase
  long      ssousc     = 0;                     // contract max power per phase

  char       str_code[18];                      // code of current contract
  char       str_period[18];                    // code of current period
  tic_period arr_period[TIC_INDEX_MAX];         // periods in the contract
} teleinfo_contract;

// teleinfo : meter
// ----------------

struct tic_preavis {
  uint8_t  level;                               // level of current preavis
  uint32_t timeout;                             // timeout of current preavis
  char     str_label[8];                        // label of current preavis
};

struct tic_json {
  uint8_t data;                                 // flag to publish ALERT, METER, RELAY, CONTRACT or CAL
  uint8_t tic;                                  // flag to publish TIC
  uint8_t live;                                 // flag to publish LIVE
};

static struct {
  char      sep_line   = 0;                     // detected line separator
  uint8_t   company    = 0;                     // manufacturer of the meter
  uint8_t   model      = 0;                     // model of the meter
  uint8_t   serial     = TIC_SERIAL_INIT;       // serial port status
  uint8_t   reception  = TIC_RECEPTION_NONE;    // reception phase
  uint8_t   suspend    = 0;                     // flag set if suspending soon
  uint8_t   use_sinsts = 0;                     // flag to use sinsts etiquette for papp
  uint8_t   new_speed  = 0;                     // flag set if reception speed will change
  uint8_t   day        = 0;                     // current day of month
  uint8_t   slot       = UINT8_MAX;             // current slot
  uint16_t  year       = 0;                     // year of manufacturing of the meter
  uint32_t  date       = 0;                     // current date with slot
  uint32_t  last_msg   = 0;                     // timestamp of last trasnmitted message

  long      nb_message = 0;                     // total number of messages sent by the meter
  long      nb_reset   = 0;                     // total number of message reset sent by the meter
  long      nb_skip    = 0;                     // index of last non skipped live message
  long long nb_line    = 0;                     // total number of received lines
  long long nb_error   = 0;                     // total number of checksum errors
  long long ident      = 0;                     // meter identification number
  tic_json    json;                             // JSON publication flags
  tic_preavis preavis;                          // Current preavis
} teleinfo_meter;

// teleinfo : LED management
// -------------------------

static struct {
  uint8_t  state     = TIC_LED_PWR_OFF;         // current LED state
  uint8_t  status    = TIC_LED_STEP_NONE;       // meter LED status
  uint8_t  level     = TIC_LEVEL_NONE;          // period level
  uint32_t msg_time  = UINT32_MAX;              // timestamp of last reception 
  uint32_t upd_time  = UINT32_MAX;              // timestamp of last LED update 
} teleinfo_led;


// teleinfo : conso mode
// ---------------------

struct tic_cosphi {
  uint8_t index;                                // index of cosphy array current value
  uint8_t page;                                 // index of current power page
  long value;                                   // current value of cosphi
  long quantity;                                // number of measure done 
  long nb_message;                              // number of messages with last cosphi calculation 
  long arr_value[TIC_COSPHI_SAMPLE];            // array of cosphi values
  long arr_page[TIC_COSPHI_PAGE];               // array of average cosphi for power pages 
}; 

struct tic_phase {
  bool  volt_set;                               // voltage set in current message
  long  voltage;                                // instant voltage (V)
  long  current;                                // instant current (mA)
  long  papp;                                   // instant apparent power (VA)
  long  sinsts;                                 // instant apparent power (VA)
  long  pact;                                   // instant active power (W)
  long  preact;                                 // instant reactive power (VAr)
  long  pact_last;                              // last published active power (VA)
  long  cosphi;                                 // current cos phi (x1000)
};

static struct {
  uint8_t relay      = 0;                       // consoi virtual relays status

  long  papp         = 0;                       // current conso apparent power (VA)
  long  pact         = 0;                       // current conso active power (W)
  long  delta_mwh    = 0;                       // active conso delta since last total (milli Wh)
  long  delta_mvah   = 0;                       // apparent power counter increment (milli VAh)
  long  today_wh     = 0;                       // active power conso today (Wh)
  long  yesterday_wh = 0;                       // active power conso testerday (Wh)
  
  long  papp_now     = LONG_MAX;                // apparent power current counter (in vah)
  long  papp_prev    = LONG_MAX;                // apparent power previous counter (in vah)
  long  pact_now     = LONG_MAX;                // active power current counter (in wh)
  long  pact_prev    = LONG_MAX;                // active power previous counter (in wh)
  long  preact_now   = LONG_MAX;                // current reactive power counter (in wh)
  long  preact_prev  = LONG_MAX;                // previous reactive power counter (in wh)

  long  last_stamp   = LONG_MAX;                // timestamp of current measure
  long  papp_stamp   = LONG_MAX;                // timestamp of previous apparent power measure
  long  pact_stamp   = LONG_MAX;                // timestamp of previous active power measure
  long  preact_stamp = LONG_MAX;                // timestamp of previous reactive power measure
  long  cosphi_stamp = LONG_MAX;                // timestamp of last cos phi calculation

  long long midnight_wh  = 0;                   // global active conso total at previous midnight (Wh)
  long long total_wh     = 0;                   // global active conso total (Wh)
  long long index_wh[TIC_INDEX_MAX];            // array of conso total of different tarif periods (Wh)

  tic_cosphi cosphi;                            // global cosphi data 
  tic_phase  phase[TIC_PHASE_MAX];              // phases data
} teleinfo_conso;

// teleinfo : production mode
// --------------------------

static struct {
  uint8_t relay         = 0;                   // production relay status

  long    papp          = 0;                   // production instant apparent power 
  long    papp_last     = 0;                   // last published apparent power
  long    pact          = 0;                   // production instant active power
  long    delta_mwh     = 0;                   // active conso delta since last total (milli Wh)
  long    delta_mvah    = 0;                   // apparent power counter increment (milli VAh)
  long    today_wh      = 0;                   // active power produced today (Wh)
  long    yesterday_wh  = 0;                   // active power produced yesterday (Wh)

  float   pact_avg      = 0;                   // average produced active power

  long long midnight_wh = 0;                   // active power total last midnight (Wh)
  long long total_wh    = 0;                   // active power total

  tic_cosphi cosphi;                            // global cosphi data 
} teleinfo_prod;

/****************************************\
 *               Icons
 *
 *      xxd -i -c 256 icon.png
\****************************************/

#ifdef USE_WEBSERVER

// linky icon
char teleinfo_icon_linky_png[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x43, 0x00, 0x00, 0x00, 0x64, 0x08, 0x03, 0x00, 0x00, 0x00, 0xea, 0xbd, 0x7d, 0xcf, 0x00, 0x00, 0x00, 0xc6, 0x50, 0x4c, 0x54, 0x45, 0x00, 0x00, 0x00, 0xcc, 0xe8, 0x21, 0xd6, 0xd6, 0xd6, 0x58, 0x59, 0x58, 0x96, 0xab, 0x13, 0xff, 0xff, 0xff, 0xd7, 0xd5, 0xde, 0x4e, 0x4f, 0x4e, 0xcc, 0xe8, 0x18, 0xce, 0xe5, 0x46, 0xcf, 0xe3, 0x62, 0xd6, 0xd5, 0xdb, 0xcb, 0xe9, 0x00, 0xcc, 0xe9, 0x11, 0xd1, 0xe0, 0x87, 0xd6, 0xd5, 0xdd, 0xdb, 0xdb, 0xdb, 0x53, 0x54, 0x53, 0x8e, 0x8f, 0x8e, 0xc1, 0xc1, 0xc1, 0xad, 0xad, 0xad, 0xc9, 0xe5, 0x20, 0x9c, 0xb2, 0x15, 0x92, 0xa6, 0x12, 0xa1, 0xbb, 0x20, 0xba, 0xd4, 0x1d, 0xb4, 0xcd, 0x1b, 0xc5, 0xe0, 0x1f, 0x96, 0xac, 0x15, 0xb6, 0xd2, 0x25, 0xef, 0xf8, 0xcb, 0xfb, 0xfd, 0xf3, 0xa1, 0xbc, 0x20, 0xe4, 0xf3, 0xa2, 0xd0, 0xea, 0x3e, 0xad, 0xc7, 0x23, 0xc0, 0xde, 0x26, 0xd0, 0xe0, 0x7c, 0xd2, 0xdd, 0x9f, 0xce, 0xce, 0xce, 0xdd, 0xf0, 0x84, 0xeb, 0xf6, 0xb9, 0xd8, 0xed, 0x6c, 0xe0, 0xf1, 0x8e, 0xec, 0xf6, 0xbf, 0xe2, 0xf2, 0x96, 0xa9, 0xc1, 0x1a, 0xcd, 0xe7, 0x32, 0xd4, 0xd9, 0xb6, 0xd2, 0xde, 0x96, 0xd5, 0xd7, 0xcb, 0xd4, 0xd9, 0xb9, 0xf5, 0xfb, 0xdf, 0xda, 0xee, 0x76, 0xba, 0xdc, 0x33, 0xd0, 0xe1, 0x73, 0xd3, 0xdc, 0xa5, 0xcf, 0xe3, 0x5e, 0xd5, 0xd8, 0xc6, 0x84, 0x84, 0x84, 0x60, 0x61, 0x60, 0x9b, 0x9b, 0x9b, 0x77, 0x78, 0x77, 0x6b, 0x6c, 0x6b, 0xc3, 0xc4, 0xc3, 0xe8, 0xf4, 0xaf, 0x8d, 0x9e, 0xc9, 0x15, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66,
  0x00, 0x00, 0x00, 0x01, 0x62, 0x4b, 0x47, 0x44, 0x00, 0x88, 0x05, 0x1d, 0x48, 0x00, 0x00, 0x01, 0xb5, 0x49, 0x44, 0x41, 0x54, 0x58, 0xc3, 0xed, 0xd9, 0x4d, 0x6f, 0x82, 0x30, 0x18, 0x07, 0x70, 0x8a, 0xcf, 0xa5, 0x3b, 0x70, 0xc1, 0x36, 0x7d, 0xe1, 0xb0, 0xb8, 0x09, 0x09, 0x87, 0x32, 0xe7, 0x61, 0x92, 0xb9, 0x97, 0xef, 0xff, 0xa9, 0xf6, 0xa0, 0x5e, 0x8c, 0xa5, 0x2d, 0x94, 0xb9, 0x64, 0xe9, 0x3f, 0x31, 0x68, 0x22, 0x3f, 0xfa, 0x96, 0x5a, 0xdb, 0x2c, 0xc3, 0xf4, 0x3d, 0x25, 0x73, 0x42, 0xdb, 0x3e, 0xbb, 0xa4, 0x23, 0x11, 0x39, 0x13, 0x24, 0x2e, 0xd1, 0xa5, 0x38, 0x23, 0x7d, 0x2c, 0x41, 0x68, 0xd6, 0x46, 0x1b, 0xbd, 0xb7, 0x2a, 0xcc, 0x70, 0x2e, 0xb4, 0xbb, 0x32, 0xbe, 0xa7, 0xb0, 0x0a, 0x00, 0xd4, 0x44, 0x43, 0x33, 0xab, 0xc1, 0x26, 0x18, 0xaa, 0xe2, 0xd7, 0x06, 0x47, 0x43, 0x4e, 0x2b, 0x87, 0x82, 0x93, 0xa1, 0xb9, 0xb8, 0x3c, 0x59, 0x63, 0xc8, 0x2c, 0x83, 0x29, 0xc5, 0x82, 0x07, 0xc8, 0x98, 0x61, 0x0c, 0xd3, 0x46, 0x08, 0x61, 0x34, 0x51, 0x02, 0xeb, 0x22, 0x8d, 0x94, 0xc3, 0xf5, 0xf4, 0x21, 0xcc, 0x78, 0x86, 0x4a, 0x1b, 0xe0, 0x12, 0x5f, 0x4c, 0x80, 0xc2, 0xf7, 0x60, 0x14, 0x54, 0x52, 0x59, 0x1a, 0xc7, 0x65, 0x3c, 0x82, 0x21, 0x12, 0xaf, 0x02, 0x78, 0x85, 0x1c, 0x21, 0x06, 0x2a, 0x5b, 0x3f, 0x5b, 0x8c, 0x8a, 0xdd, 0x18, 0x78, 0xab, 0x19, 0x1a, 0xd6, 0x58, 0x87, 0xca, 0x8d, 0xa1, 0x39, 0x7e, 0x5d, 0x6b, 0x79, 0x65, 0x18, 0xc9, 0xb1, 0x1e, 0xd8, 0x28, 0x00, 0x2c, 0x68, 0x8c, 0xe1, 0xe0, 0xe6, 0x5c, 0x72, 0xce, 0x14, 0x96, 0x5f, 0x73, 0x83, 0xa3, 0x5d, 0x0e, 0x43, 0x5e, 0x9d, 0x7a, 0x7c, 0xce, 0x58, 0x9f, 0xd3, 0xb7,
  0xff, 0xcb, 0xa0, 0x45, 0xed, 0xc8, 0xb6, 0xf3, 0x1b, 0x5d, 0x9d, 0xbb, 0xf3, 0x5a, 0xf8, 0x8c, 0xee, 0x25, 0xf7, 0x66, 0xe7, 0x31, 0xea, 0x3c, 0x20, 0x6f, 0x4e, 0x63, 0x13, 0x42, 0xe4, 0x7b, 0xa7, 0x51, 0x04, 0x19, 0x39, 0x75, 0x19, 0x75, 0x98, 0xf1, 0xe0, 0x32, 0x0e, 0xc9, 0x48, 0xc6, 0x9d, 0x8c, 0x72, 0xfd, 0x61, 0xc9, 0xba, 0x99, 0x60, 0xac, 0x57, 0xf6, 0x1c, 0xcb, 0x60, 0xe3, 0xfb, 0x38, 0x62, 0xac, 0x3e, 0x83, 0x8d, 0xb1, 0x62, 0x60, 0x9a, 0x64, 0x24, 0x23, 0x19, 0xc9, 0x70, 0x19, 0xe5, 0x02, 0x46, 0x33, 0x3a, 0x8f, 0xbd, 0x87, 0xcf, 0xa7, 0xe5, 0xd7, 0xc8, 0x54, 0xd8, 0x4c, 0x98, 0xd7, 0x9b, 0xd2, 0x9a, 0xbb, 0xfd, 0xbe, 0x2c, 0xb1, 0x0e, 0x5a, 0x62, 0x3d, 0x46, 0x17, 0x58, 0x17, 0x86, 0x35, 0xc8, 0xd6, 0xb3, 0x4e, 0xde, 0xfb, 0x89, 0xda, 0xb7, 0x5e, 0xdf, 0x1c, 0xa2, 0xd7, 0xeb, 0xc3, 0x5e, 0xc4, 0xee, 0xe0, 0x48, 0x41, 0xd3, 0x7f, 0xb1, 0x64, 0x24, 0xe3, 0x17, 0x0d, 0x65, 0x86, 0xc8, 0x28, 0x63, 0xd8, 0x61, 0x03, 0x78, 0x8a, 0x32, 0xf4, 0x39, 0xa9, 0x5f, 0x92, 0xf1, 0xb7, 0x46, 0xb7, 0x80, 0x11, 0xbf, 0x47, 0x4f, 0x17, 0x39, 0x2b, 0x88, 0x6f, 0x90, 0x05, 0xce, 0x4e, 0xda, 0xf8, 0x33, 0x9c, 0xf6, 0x72, 0x10, 0x44, 0xe7, 0x36, 0x6c, 0x4f, 0x87, 0xdb, 0x7f, 0x00, 0x63, 0xa4, 0x5e, 0x2f, 0x79, 0x81, 0xc2, 0xec, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
const unsigned int teleinfo_icon_linky_png_size = 730;

// icons
void TeleinfoDriverLinkyIcon ()
{ 
  Webserver->send_P (200, PSTR ("image/png"), teleinfo_icon_linky_png, teleinfo_icon_linky_png_size);
}

// Get specific argument as a value with min and max
int TeleinfoDriverGetArgValue (const char* pstr_argument, const int value_min, const int value_max, const int value_default)
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

#endif      // USE_WEBSERVER

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

    // else check MQTT connexion
//    else if (!MqttIsConnected ()) TeleinfoLedSetStatus (TIC_LED_STEP_MQTT);

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

// data are ready to be published
bool TeleinfoDriverMeterReady () 
{
  return (teleinfo_meter.nb_message >= TIC_MESSAGE_MIN);
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
  if (teleinfo_config.meter)    TeleinfoDriverPublishConsoProd (true);
  if (teleinfo_config.meter)    TeleinfoDriverPublishContract ();
  if (teleinfo_config.relay)    TeleinfoDriverPublishRelay ();
  if (teleinfo_config.calendar) TeleinfoDriverPublishCalendar (append);
  TeleinfoDriverPublishAlert ();

#ifdef USE_RTE_SERVER
  RtePublishSensor ();
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
#ifdef USE_TELEINFO_DOMOTICZ
  DomoticzIntegrationData ();
#endif    // USE_TELEINFO_DOMOTICZ

  // homie publication
#ifdef USE_TELEINFO_HOMIE
  HomieIntegrationDataConsoProd ();
  if (teleinfo_config.calendar) HomieIntegrationDataCalendar ();
  if (teleinfo_config.relay)    HomieIntegrationDataRelay ();
#endif    // USE_TELEINFO_HOMIE

  // thingsboard publication
#ifdef USE_TELEINFO_THINGSBOARD
  ThingsboardIntegrationData ();
  if (append) ThingsboardIntegrationAttribute ();
#endif    // USE_TELEINFO_THINGSBOARD

  // influxdb publication
#ifdef USE_INFLUXDB
  InfluxDbExtensionData ();
#endif    // USE_INFLUXDB
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
  bool        publish;
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
    publish = (teleinfo_meter.suspend || (teleinfo_conso.cosphi.quantity >= TIC_COSPHI_SAMPLE));
    if (publish) ResponseAppend_P (PSTR (",\"C\":%d.%02d"), teleinfo_conso.cosphi.value / 1000, teleinfo_conso.cosphi.value % 1000 / 10);

    // conso : if not on battery, publish total of yesterday and today
    if (!teleinfo_config.battery)
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
    publish = (teleinfo_meter.suspend || (teleinfo_prod.cosphi.quantity >= TIC_COSPHI_SAMPLE));
    if (publish) ResponseAppend_P (PSTR (",\"PC\":%d.%02d"), teleinfo_prod.cosphi.value / 1000, teleinfo_prod.cosphi.value % 1000 / 10);

    // prod : average power
    ResponseAppend_P (PSTR (",\"PAVG\":%d"), (long)teleinfo_prod.pact_avg);

    // prod : if not on battery, publish total of yesterday and today
    if (!teleinfo_config.battery)
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

  // contract color
  index = TeleinfoPeriodGetLevel ();
  GetTextIndexed (str_value, sizeof (str_value), index, kTeleinfoLevelLabel);
  ResponseAppend_P (PSTR (",\"color\":\"%s\""), str_value);

  // contract hour type
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
  if (use_rte && RteTempoIsEnabled ()  && ((teleinfo_contract.index == TIC_C_HIS_TEMPO) || (teleinfo_contract.index == TIC_C_STD_TEMPO))) level = RteTempoGetGlobalLevel  (day + 1, slot / 2);
  if (use_rte && RtePointeIsEnabled () && ((teleinfo_contract.index == TIC_C_HIS_EJP)   || (teleinfo_contract.index == TIC_C_STD_EJP)))   level = RtePointeGetGlobalLevel (day + 1, slot / 2);
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
  if (teleinfo_config.battery) return;

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
  if (teleinfo_config.battery) return;

  // if graph unit have been changed, save configuration
  TeleinfoConfigSave (false);

  // update energy counters
  EnergyUpdateToday ();
}

/*************************************************\
 *               InfluxDB API
\*************************************************/

#ifdef USE_INFLUXDB

// called to generate InfluxDB data
void TeleinfoDriverApiInfluxDb ()
{
  uint8_t phase, index;
  char    str_value[16];
  char    str_line[96];

  // if nothing to publish, ignore
  if ((teleinfo_conso.total_wh == 0) && (teleinfo_prod.total_wh == 0)) return;

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

# endif     // USE_INFLUXDB

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
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>Configure Teleinfo</button></form></p>\n"), PSTR (TIC_PAGE_TIC_CFG));
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
#ifdef USE_TELEINFO_HASS
    WebGetArg (PSTR (CMND_TIC_HASS), str_text, sizeof (str_text));
    status = (strlen (str_text) > 0);
    actual = HassIntegrationGet ();
    if (actual != status) HassIntegrationSet (status);
    if (status) teleinfo_config.meter = 1;
#endif    // USE_TELEINFO_HASS

    // parameter 'homie' : set homie integration
#ifdef USE_TELEINFO_HOMIE
    WebGetArg (PSTR (CMND_TIC_HOMIE), str_text, sizeof (str_text));
    status = (strlen (str_text) > 0);
    actual = HomieIntegrationGet ();
    if (actual != status) HomieIntegrationSet (status);
#endif    // USE_TELEINFO_HOMIE

    // parameter 'domo' : set domoticz integration
#ifdef USE_TELEINFO_DOMOTICZ
    WebGetArg (PSTR (CMND_TIC_DOMO), str_text, sizeof (str_text));
    status = (strlen (str_text) > 0);
    actual = DomoticzIntegrationGet ();
    if (actual != status) DomoticzIntegrationSet (status);
    DomoticzIntegrationRetrieveParameters ();
#endif    // USE_TELEINFO_DOMOTICZ

    // parameter 'things' : set thingsboard integration
#ifdef USE_TELEINFO_THINGSBOARD
    WebGetArg (PSTR (CMND_TIC_THINGSBOARD), str_text, sizeof (str_text));
    status = (strlen (str_text) > 0);
    actual = ThingsboardIntegrationGet ();
    if (actual != status) ThingsboardIntegrationSet (status);
#endif    // USE_TELEINFO_THINGSBOARD

    // parameter 'influx' : set influxdb integration
#ifdef USE_INFLUXDB
    WebGetArg (PSTR (CMND_TIC_INFLUXDB), str_text, sizeof (str_text));
    status = (strlen (str_text) > 0);
    actual = InfluxDbExtensionGet (); 
    if (actual != status) InfluxDbExtensionSet (status);
//    InfluxDbExtensionRetrieveParameters ();
#endif    // USE_INFLUXDB

    // save configuration
    TeleinfoConfigSave (true);

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
  WSContentSend_P (PSTR ("<fieldset><legend>📨 Teleinfo <small><i>(redémarrage)</i></small></legend>\n"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  if (!Settings->teleinfo.autodetect && (TasmotaGlobal.baudrate > 19200)) strcpy_P (str_select, PSTR ("checked")); else str_select[0] = 0;
  WSContentSend_P (PSTR ("<p class='dat'><input type='radio' name='%s' value='%d' %s>%s</p>\n"), PSTR (CMND_TIC_RATE), 115200, str_select, PSTR ("Désactivé"));

  if (!Settings->teleinfo.autodetect && (TasmotaGlobal.baudrate == 1200)) strcpy_P (str_select, PSTR ("checked")); else str_select[0] = 0;
  pstr_title = PSTR ("Compteur configuré en mode Historique émettant à 1200 bauds");
  WSContentSend_P (PSTR ("<p class='dat' title='%s'><input type='radio' name='%s' value='%d' %s>%s</p>\n"), pstr_title, PSTR (CMND_TIC_RATE), 1200, str_select, PSTR ("Historique"));

  if (!Settings->teleinfo.autodetect && (TasmotaGlobal.baudrate == 9600)) strcpy_P (str_select, PSTR ("checked")); else str_select[0] = 0;
  pstr_title = PSTR ("Compteur configuré en mode Standard émettant à 9600 bauds");
  WSContentSend_P (PSTR ("<p class='dat' title='%s'><input type='radio' name='%s' value='%d' %s>%s</p>\n"), pstr_title, PSTR (CMND_TIC_RATE), 9600, str_select, PSTR ("Standard"));

  WSContentSend_P (PSTR ("</div>\n"));
  WSContentSend_P (PSTR ("</fieldset>\n"));

  // data published
  // --------------

  WSContentSend_P (PSTR ("<fieldset><legend>📊 Données publiées</legend>\n"));
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
  WSContentSend_P (PSTR ("<fieldset><legend>〽️ Politique de publication</legend>\n"));
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

#ifdef USE_TELEINFO_HASS
  // Home Assistant auto-discovery
  actual = HassIntegrationGet ();
  if (actual) strcpy_P (str_select, PSTR ("checked")); else str_select[0] = 0;
  pstr_title = PSTR ("Topic hass/... pour l auto-découverte de toutes les données par Home Assistant. Home Assistant exploitera alors les données du topic SENSOR");
  WSContentSend_P (PSTR ("<p class='dat' title='%s'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), pstr_title, PSTR (CMND_TIC_HASS), str_select, PSTR (CMND_TIC_HASS), PSTR ("Home Assistant"));
#endif    // USE_TELEINFO_HASS

#ifdef USE_TELEINFO_HOMIE
  // Homie auto-discovery
  actual = HomieIntegrationGet ();
  if (actual) strcpy_P (str_select, PSTR ("checked")); else str_select[0] = 0;
  pstr_title = PSTR ("Topic homie/... pour l auto-découverte et la publication de toutes les données suivant le protocole Homie (OpenHab)");
  WSContentSend_P (PSTR ("<p class='dat' title='%s'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), pstr_title, PSTR (CMND_TIC_HOMIE), str_select, PSTR (CMND_TIC_HOMIE), PSTR ("Homie"));
#endif    // USE_TELEINFO_HOMIE

#ifdef USE_TELEINFO_DOMOTICZ
  // Domoticz integration
  actual = DomoticzIntegrationGet ();
  if (actual) strcpy_P (str_select, PSTR ("checked")); else str_select[0] = 0;
  pstr_title = PSTR ("Topic domoticz/in pour la publication des données principales vers Domoticz");
  WSContentSend_P (PSTR ("<p class='dat' title='%s'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), pstr_title, PSTR (CMND_TIC_DOMO), str_select, PSTR (CMND_TIC_DOMO), PSTR ("Domoticz"));
  if (actual) DomoticzIntegrationDisplayParameters ();
#endif    // USE_TELEINFO_DOMOTICZ

#ifdef USE_TELEINFO_THINGSBOARD
  // Thingsboard auto-discovery
  actual = ThingsboardIntegrationGet ();
  if (actual) strcpy_P (str_select, PSTR ("checked")); else str_select[0] = 0;
  pstr_title = PSTR ("Topic v1/telemetry/device/me pour la publication des données principales vers Thingsboard");
  WSContentSend_P (PSTR ("<p class='dat' title='%s'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), pstr_title, PSTR (CMND_TIC_THINGSBOARD), str_select, PSTR (CMND_TIC_THINGSBOARD), PSTR ("Thingsboard"));
#endif    // USE_TELEINFO_THINGSBOARD

#ifdef USE_INFLUXDB
  // InfluxDB integration
  actual = InfluxDbExtensionGet ();
  if (actual) strcpy_P (str_select, PSTR ("checked")); else str_select[0] = 0;
  pstr_title = PSTR ("Publication des données principales vers un serveur InfluxDB");
  WSContentSend_P (PSTR ("<p class='dat' title='%s'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), pstr_title, PSTR (CMND_TIC_INFLUXDB), str_select, PSTR (CMND_TIC_INFLUXDB), PSTR ("InfluxDB"));
//  if (actual) InfluxDbExtensionDisplayParameters ();
#endif    // USE_INFLUXDB

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
  WSContentSend_P (PSTR ("div.tic{display:flex;padding:2px 0px;}\n"));
  WSContentSend_P (PSTR ("div.tic div{padding:0px;}\n"));
  WSContentSend_P (PSTR ("div.tich{width:16%%;}\n"));
  WSContentSend_P (PSTR ("div.tict{width:37%%;text-align:left;}\n"));
  WSContentSend_P (PSTR ("div.ticv{width:35%%;text-align:right;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.tics{width:2%%;}\n"));
  WSContentSend_P (PSTR ("div.ticu{width:10%%;text-align:left;}\n"));

  WSContentSend_P (PSTR ("div.bar{margin:0px;height:16px;opacity:75%%;}\n"));
  WSContentSend_P (PSTR ("div.barh{width:16%%;text-align:left;}\n"));
  WSContentSend_P (PSTR ("div.barm{width:72%%;text-align:left;background-color:%s;border-radius:6px;}\n"), PSTR (COLOR_BACKGROUND));
  WSContentSend_P (PSTR ("div.barv{height:16px;padding:0px;text-align:center;font-size:12px;border-radius:6px;}\n"));

  WSContentSend_P (PSTR ("div.warn{width:68%%;text-align:center;margin-top:2px;font-size:12px;font-weight:bold;border-radius:8px;}\n"));
  WSContentSend_P (PSTR ("div.tic span{font-size:10px;font-weight:normal;margin-left:10px;padding:0px 4px;color:black;background:#aaa;border-radius:4px;}\n"));
  
  WSContentSend_P (PSTR ("table hr{display:none;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // set reception status
  if (teleinfo_meter.serial == TIC_SERIAL_GPIO) pstr_alert = PSTR ("TInfo Rx non configuré");
    else if (teleinfo_meter.serial == TIC_SERIAL_SPEED) pstr_alert = PSTR ("Vitesse non configurée");
    else if (teleinfo_meter.serial == TIC_SERIAL_FAILED) pstr_alert = PSTR ("Problème d'initialisation");
    else if (!teleinfo_config.energy && !teleinfo_config.meter && !teleinfo_config.relay && !teleinfo_config.calendar) pstr_alert = PSTR ("Aucune donnée publiée");
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
    // if aloert is published, add separator
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
      if (strlen (str_color) > 0) WSContentSend_P (PSTR ("<span>%s</span>"), str_color);
      WSContentSend_P (PSTR ("</div><div style='width:15%%;text-align:right;font-size:12px;'>%u</div></div>\n"), teleinfo_meter.year);

      // separator
      WSContentSend_P (PSTR ("<hr>\n"));
    }

    // get contract data
    TeleinfoContractGetName (str_text, sizeof (str_text));
    GetTextIndexed (str_color, sizeof (str_color), teleinfo_contract.mode, kTeleinfoModeIcon);

    // header
    WSContentSend_P (PSTR ("<div class='tic' style='margin:-2px 0px 4px 0px;font-size:16px;'>\n"));
    WSContentSend_P (PSTR ("<div class='tich'>%s</div>\n"), str_color);
    WSContentSend_P (PSTR ("<div style='width:44%%;text-align:left;font-weight:bold;'>%s</div>\n"), str_text);
    if (teleinfo_contract.phase > 1) sprintf_P (str_text, PSTR ("%ux"), teleinfo_contract.phase);
      else str_text[0] = 0;
    if (teleinfo_contract.index != UINT8_MAX) WSContentSend_P (PSTR ("<div style='width:28%%;text-align:right;font-weight:bold;'>%s%d</div>\n"), str_text, teleinfo_contract.ssousc / 1000);
    if (teleinfo_contract.unit == TIC_UNIT_KVA) strcpy_P (str_text, PSTR ("kVA")); 
      else if (teleinfo_contract.unit == TIC_UNIT_KW) strcpy_P (str_text, PSTR ("kW"));
      else str_text[0] = 0;
    WSContentSend_P (PSTR ("<div class='tics'></div><div style='width:10%%;text-align:left;'>%s</div>\n"), str_text);
    WSContentSend_P (PSTR ("</div>\n"));

    // contract
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
        WSContentSend_P (PSTR ("<div style='width:36%%;text-align:right;'>%s.%03d</div>"), str_text, value);
        WSContentSend_P (PSTR ("<div class='tics'></div><div class='ticu'>kWh</div>"));
        WSContentSend_P (PSTR ("</div>\n"));
      }
    }

    // production total
    if (teleinfo_prod.total_wh != 0)
    {
      WSContentSend_P (PSTR ("<div class='tic'>"));

      // period name
      WSContentSend_P (PSTR ("<div class='tich'></div>"));
      if (teleinfo_prod.papp == 0) WSContentSend_P (PSTR ("<div style='width:36%%;'>Production</div>"));
        else WSContentSend_P (PSTR ("<div style='width:36%%;background-color:#080;border-radius:6px;'>Production</div>"));

      // counter value
      lltoa (teleinfo_prod.total_wh / 1000, str_text, 10);
      value = (long)(teleinfo_prod.total_wh % 1000);
      WSContentSend_P (PSTR ("<div style='width:36%%;text-align:right;'>%s.%03d</div>"), str_text, value);
      WSContentSend_P (PSTR ("<div class='tics'></div><div class='ticu'>kWh</div>"));
      WSContentSend_P (PSTR ("</div>\n"));
     }

    //   consommation
    // ----------------

    if (teleinfo_conso.total_wh > 0)
    {
      // consumption : separator and header
      WSContentSend_P (PSTR ("<hr>\n"));
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

      // consumption : bar graph per phase
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

      // consumption : active power
      value = 0; 
      for (phase = 0; phase < teleinfo_contract.phase; phase++) value += teleinfo_conso.phase[phase].pact;
      WSContentSend_P (PSTR ("<div class='tic'><div class='tich'></div>"));
      WSContentSend_P (PSTR ("<div class='tict'>cosφ <b>%d.%02d</b></div><div class='ticv'>%d</div>"), teleinfo_conso.cosphi.value / 1000, teleinfo_conso.cosphi.value % 1000 / 10, value);
      WSContentSend_P (PSTR ("<div class='tics'></div><div class='ticu'>W</div></div>\n"));

      // consumption : today's total
      if (teleinfo_conso.today_wh > 0)
      {
        WSContentSend_P (PSTR ("<div class='tic'><div class='tich'></div>"));
        WSContentSend_P (PSTR ("<div class='tict'>Aujourd'hui</div><div class='ticv'>%d</div>"), teleinfo_conso.today_wh);
        WSContentSend_P (PSTR ("<div class='tics'></div><div class='ticu'>Wh</div></div>\n"));
      }

      // consumption : yesterday's total
      if (teleinfo_conso.yesterday_wh > 0)
      {
        WSContentSend_P (PSTR ("<div class='tic'><div class='tich'></div>"));
        WSContentSend_P (PSTR ("<div class='tict'>Hier</div><div class='ticv'>%d</div>"), teleinfo_conso.yesterday_wh);
        WSContentSend_P (PSTR ("<div class='tics'></div><div class='ticu'>Wh</div></div>\n"));
      }
    }

    //   production
    // --------------

    if (teleinfo_prod.total_wh > 0)
    {
      // production : separator and header
      WSContentSend_P (PSTR ("<hr>\n"));
      WSContentSend_P (PSTR ("<div style='padding:0px 0px 5px 0px;text-align:left;font-weight:bold;'>Production</div>\n"));

      // production : bar graph percentage
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

      // production : active power
      WSContentSend_P (PSTR ("<div class='tic'><div class='tich'></div>"));
      WSContentSend_P (PSTR ("<div class='tict'>cosφ <b>%d.%02d</b></div><div class='ticv'>%d</div>"), teleinfo_prod.cosphi.value / 1000, teleinfo_prod.cosphi.value % 1000 / 10, teleinfo_prod.pact);
      WSContentSend_P (PSTR ("<div class='tics'></div><div class='ticu'>W</div></div>\n"));

      // production : today's total
      if (teleinfo_prod.today_wh > 0)
      {
        WSContentSend_P (PSTR ("<div class='tic'><div class='tich'></div>"));
        WSContentSend_P (PSTR ("<div class='tict'>Aujourd'hui</div><div class='ticv'>%d</div>"), teleinfo_prod.today_wh);
        WSContentSend_P (PSTR ("<div class='tics'></div><div class='ticu'>Wh</div></div>\n"));
      }

      // production : yesterday's total
      if (teleinfo_prod.yesterday_wh > 0)
      {
        WSContentSend_P (PSTR ("<div class='tic'><div class='tich'></div>"));
        WSContentSend_P (PSTR ("<div class='tict'>Hier</div><div class='ticv'>%d</div>\n"), teleinfo_prod.yesterday_wh);
        WSContentSend_P (PSTR ("<div class='tics'></div><div class='ticu'>Wh</div></div>\n"));
      }
    }

    //   calendar
    // ------------

    if (teleinfo_config.calendar)
    {
      // separator and header
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

    //   relays
    // ------------

    if (teleinfo_config.relay)
    {
      // separator
      WSContentSend_P (PSTR ("<hr>\n"));

      // relay styles
      WSContentSend_P (PSTR ("<style>\n"));
      WSContentSend_P (PSTR ("div.rel{display:flex;font-size:16px;}\n"));
      WSContentSend_P (PSTR ("div.rel, div.rel div{padding:0px;}\n"));
      WSContentSend_P (PSTR ("div.relh{width:20%%;}\n"));
      WSContentSend_P (PSTR ("div.relv{width:10%%;}\n"));
      WSContentSend_P (PSTR ("</style>\n"));

      // relay state
      WSContentSend_P (PSTR ("<div class='rel'>\n"));
      WSContentSend_P (PSTR ("<div class='relh' style='text-align:left;font-weight:bold;font-size:12px;'>Relais</div>\n"));
      for (index = 0; index < 8; index ++) 
      {
        if (TeleinfoRelayStatus (index) == 1) strcpy_P (str_text, PSTR ("🟢")); else strcpy_P (str_text, PSTR ("🔴"));
        WSContentSend_P (PSTR ("<div class='relv'>%s</div>\n"), str_text);
      }
      WSContentSend_P (PSTR ("</div>\n"));

      // relay number
      WSContentSend_P (PSTR ("<div class='rel' style='font-size:10px;margin:-17px 0px 10px 0px;'>\n"));
      WSContentSend_P (PSTR ("<div class='relh'></div>\n"));
      for (index = 0; index < 8; index ++) WSContentSend_P (PSTR ("<div class='relv'>%u</div>\n"), index + 1);
      WSContentSend_P (PSTR ("</div>\n"));
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
    WSContentSend_P (PSTR (".count{width:47%%;}\n"));
    WSContentSend_P (PSTR (".error{color:#c00;}\n"));
    WSContentSend_P (PSTR (".reset{color:grey;}\n"));
    WSContentSend_P (PSTR (".light{width:6%%;font-size:14px;animation:animate %us linear infinite;}\n"), period / 1000);
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

  WSContentSend_P (PSTR ("body {color:white;background-color:%s;font-family:Arial, Helvetica, sans-serif;}\n"), COLOR_BACKGROUND);
  WSContentSend_P (PSTR ("div {width:100%%;margin:4px auto;padding:2px 0px;text-align:center;vertical-align:middle;}\n"));

  WSContentSend_P (PSTR ("a {text-decoration:none;}\n"));

  WSContentSend_P (PSTR ("div.title {font-size:28px;}\n"));
  WSContentSend_P (PSTR ("div.count {position:relative;top:-36px;}\n"));
  WSContentSend_P (PSTR ("div.count span {font-size:12px;background:#4d82bd;color:white;padding:0px 6px;border-radius:6px;}\n"));

  WSContentSend_P (PSTR ("div.table {margin-top:-24px;}\n"));
  WSContentSend_P (PSTR ("table {width:100%%;max-width:400px;margin:0px auto;border:none;background:none;color:#fff;border-collapse:collapse;}\n"));
  WSContentSend_P (PSTR ("th,td {font-size:1rem;padding:0.2rem 0.1rem;border:1px #666 solid;}\n"));
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

  // title
  WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText (SET_DEVICENAME));

  // icon and counter
  WSContentSend_P (PSTR ("<div><img src='%s'><div class='count'><span id='msg'>%u</span></div></div>\n"), PSTR (TIC_ICON_LINKY_PNG), teleinfo_meter.nb_message);

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

// Teleinfo energy driver
bool Xdrv110 (uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
    case FUNC_INIT:
      TeleinfoDriverInit ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kTeleinfoDriverCommands, TeleinfoDriverCommand);
      break;
    case FUNC_SAVE_BEFORE_RESTART:
      TeleinfoDriverSaveBeforeRestart ();
      break;
   case FUNC_EVERY_50_MSECOND:
      TeleinfoReceptionProcess ();
      break;
   case FUNC_EVERY_250_MSECOND:
      TeleinfoDriverEvery250ms ();
      break;
    case FUNC_JSON_APPEND:
      TeleinfoDriverTeleperiod ();
      break;

#ifdef USE_LIGHT
   case FUNC_EVERY_100_MSECOND:
      TeleinfoLedUpdate ();
      break;
#endif    // USE_LIGHT

#ifdef USE_INFLUXDB
   case FUNC_API_INFLUXDB:
      TeleinfoDriverApiInfluxDb ();
      break;
#endif    // USE_INFLUXDB

#ifdef USE_WEBSERVER
     case FUNC_WEB_ADD_MAIN_BUTTON:
      TeleinfoDriverWebMainButton ();
      break;
    case FUNC_WEB_ADD_BUTTON:
      TeleinfoDriverWebConfigButton ();
      break;
    case FUNC_WEB_SENSOR:
      TeleinfoDriverWebSensor ();
      break;
    case FUNC_WEB_ADD_HANDLER:
      Webserver->on (F (TIC_ICON_LINKY_PNG), TeleinfoDriverLinkyIcon       );
      Webserver->on (F (TIC_PAGE_TIC_CFG),   TeleinfoDriverWebPageConfigure);
      Webserver->on (F (TIC_PAGE_TIC_MSG),   TeleinfoDriverWebPageMessage  );
      Webserver->on (F (TIC_PAGE_TIC_UPD),   TeleinfoDriverWebTicUpdate    );
    break;
#endif    // USE_WEBSERVER
  }

  return result;
}

#endif      // USE_TELEINFO
#endif      // USE_ENERGY_SENSOR
