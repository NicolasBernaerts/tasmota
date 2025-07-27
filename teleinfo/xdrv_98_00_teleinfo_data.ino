/*
  xdrv_98_00_teleinfo_data.ino - Data structure used by all modules of France Teleinfo energy sensor

  Copyright (C) 2019-2025 Nicolas Bernaerts

  Version history :
    10/07/2025 v1.0 - Refactoring based on Tasmota 15

  RAM : esp8266 ~ 5600 bytes
        esp32   ~ 20000 bytes

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

/****************************************\
 *             Serial port
\****************************************/

// serial port
#include <TasmotaSerial.h>
TasmotaSerial *teleinfo_serial = nullptr;

/****************************************\
 *              Constant
\****************************************/

#define TIC_AVERAGE_PROD_SAMPLE     200       // average production samples

// result of command
enum TeleinfoCommandResult { TIC_COMMAND_NOTHING, TIC_COMMAND_SAVE, TIC_COMMAND_REBOOT, TIC_COMMAND_MAX };

// TIC reception
#define TIC_LINE_SIZE               128       // maximum size of a received TIC line
#define TIC_CODE_SIZE               20        // maximum size of a period code
#define TIC_KEY_MAX                 12        // maximum size of a TIC etiquette

#ifdef ESP32
  #define TIC_BUFFER_MAX            16384     // size of reception buffer (15 sec. at 9600 bps)
  #define TIC_LINE_QTY              74        // maximum number of lines handled in a TIC message
  #define TIC_DATA_MAX              112       // maximum size of a TIC donnee
  #define TIC_RX_BUFFER             1024
#else
  #define TIC_BUFFER_MAX            512       // size of reception buffer (3.5 sec. at 1200 bps)
  #define TIC_LINE_QTY              56        // maximum number of lines handled in a TIC message
  #define TIC_DATA_MAX              28        // maximum size of a TIC donnee
  #define TIC_RX_BUFFER             256
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
#define TIC_COSPHI_MIN              4         // minimum cosphi calculation before publication
#define TIC_COSPHI_SAMPLE           12        // number of samples to calculate cosphi (minimum = 4)
#define TIC_COSPHI_PAGE             20        // number of previous cosphi values

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


// interface strings
#define TIC_MESSAGE                 "Message"

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
const char kTeleinfoEnergyCommands[] PROGMEM =    "historique"   "|"   "standard"   "|"   "noraw"   "|"   "full"   "|"   "period"   "|"   "live"   "|"   "skip"   "|"   "bright"   "|"   "percent"   "|"   "stats"   "|"   "error"   "|"   "reset"   "|"    "calraz"   "|"   "calhexa"   "|"    "trigger"   "|" CMND_TIC_POLICY "|" CMND_TIC_METER "|" CMND_TIC_CALENDAR "|" CMND_TIC_RELAY;
enum TeleinfoEnergyCommand                   { TIC_CMND_HISTORIQUE, TIC_CMND_STANDARD, TIC_CMND_NORAW, TIC_CMND_FULL, TIC_CMND_PERIOD, TIC_CMND_LIVE, TIC_CMND_SKIP, TIC_CMND_BRIGHT, TIC_CMND_PERCENT, TIC_CMND_STATS, TIC_CMND_ERROR, TIC_CMND_RESET,  TIC_CMND_CALRAZ, TIC_CMND_CALHEXA,  TIC_CMND_TRIGGER,  TIC_CMND_POLICY  ,  TIC_CMND_METER  ,  TIC_CMND_CALENDAR  ,  TIC_CMND_RELAY};

// Data publication commands
static const char kTeleinfoDriverCommands[]  PROGMEM =          "|tic";
void (* const TeleinfoDriverCommand[])(void) PROGMEM = { &CmndTeleinfoDriverTIC };

// Data diffusion policy
enum TeleinfoMessagePolicy { TIC_POLICY_TELEMETRY, TIC_POLICY_DELTA, TIC_POLICY_MESSAGE, TIC_POLICY_MAX };
const char kTeleinfoMessagePolicy[] PROGMEM = "A chaque Télémétrie|Evolution de ±|A chaque message reçu";

// config : param
enum TeleinfoConfigKey                          { TIC_CONFIG_BRIGHT , TIC_CONFIG_TODAY_CONSO , TIC_CONFIG_TODAY_PROD , TIC_CONFIG_YESTERDAY_CONSO , TIC_CONFIG_YESTERDAY_PROD, TIC_CONFIG_MAX };    // config parameters
const long arrTeleinfoConfigDefault[] PROGMEM = { TIC_BRIGHT_DEFAULT, 0                      , 0                     , 0                          , 0 };                                            // config default values
const char kTeleinfoConfigKey[]       PROGMEM =    CMND_TIC_BRIGHT "|" CMND_TIC_TODAY_CONSO "|" CMND_TIC_TODAY_PROD "|" CMND_TIC_YESTERDAY_CONSO "|" CMND_TIC_YESTERDAY_PROD;                       // config keys

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
const char kTeleinfoLevelShort[]       PROGMEM = "ND|JB|JW|JR";
const char kTeleinfoLevelLabel[]       PROGMEM = "Inconnu|Bleu|Blanc|Rouge";
const char kTeleinfoLevelDot[]         PROGMEM = "🟢|🔵|⚪|🔴";
const char kTeleinfoLevelCalDot[]      PROGMEM = "⚪|⚪|⚫|⚪";
const char kTeleinfoLevelCalRGB[]      PROGMEM = "#946|#06b|#ddd|#b00";         // calendar color for period levels
const char kTeleinfoLevelCalText[]     PROGMEM = "#fff|#fff|#000|#fff";         // calendar color for period text
const char kTeleinfoLevelCalProd[]     PROGMEM = "#dd3";                        // production color (yellow)
const char kTeleinfoLevelCalProdText[] PROGMEM = "#000";                        // production color text

// preavis levels
enum TeleinfoPreavisLevel {TIC_PREAVIS_NONE, TIC_PREAVIS_WARNING, TIC_PREAVIS_ALERT, TIC_PREAVIS_DANGER, TIC_PREAVIS_MAX};

// serial port management
enum TeleinfoSerialStatus { TIC_SERIAL_INIT, TIC_SERIAL_GPIO, TIC_SERIAL_SPEED, TIC_SERIAL_FAILED, TIC_SERIAL_ACTIVE, TIC_SERIAL_STOPPED, TIC_SERIAL_MAX };

// etapes de reception d'un message TIC
enum TeleinfoReceptionStatus { TIC_RECEPTION_NONE, TIC_RECEPTION_MESSAGE, TIC_RECEPTION_LINE, TIC_RECEPTION_MAX };

// graph - display
enum TeleinfoUnit                  { TELEINFO_UNIT_VA, TELEINFO_UNIT_VAMAX, TELEINFO_UNIT_W, TELEINFO_UNIT_V, TELEINFO_UNIT_VMAX, TELEINFO_UNIT_COS, TELEINFO_UNIT_MAX };       // available graph units
const char kTeleinfoUnit[] PROGMEM =      "VA"      "|"        "VA"      "|"      "W"     "|"      "V"     "|"        "V"      "|"       "cosφ";                                                                                                                             // units labels

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

enum TicEtiquetteStandard                  { TIC_STD_NONE = TIC_HIS_MAX, TIC_STD_ADSC,  TIC_STD_DATE, TIC_STD_NGTF, TIC_STD_LTARF, TIC_STD_NTARF, TIC_STD_EAST, TIC_STD_EASF01, TIC_STD_EASF02, TIC_STD_EASF03, TIC_STD_EASF04, TIC_STD_EASF05, TIC_STD_EASF06, TIC_STD_EASF07, TIC_STD_EASF08, TIC_STD_EASF09, TIC_STD_EASF10, TIC_STD_EAIT, TIC_STD_IRMS1, TIC_STD_IRMS2, TIC_STD_IRMS3, TIC_STD_URMS1, TIC_STD_URMS2, TIC_STD_URMS3, TIC_STD_PREF, TIC_STD_PCOUP, TIC_STD_SINSTS, TIC_STD_SINSTS1, TIC_STD_SINSTS2, TIC_STD_SINSTS3, TIC_STD_SINSTI, TIC_STD_UMOY1, TIC_STD_UMOY2, TIC_STD_UMOY3, TIC_STD_STGE, TIC_STD_DPM1, TIC_STD_DPM2, TIC_STD_DPM3, TIC_STD_FPM1, TIC_STD_FPM2, TIC_STD_FPM3, TIC_STD_RELAIS, TIC_STD_PJOURF1, TIC_STD_PPOINTE, TIC_STD_MAX };
const char kTicEtiquetteStandard[] PROGMEM =                               "|ADSC"        "|DATE"       "|NGTF"       "|LTARF"       "|NTARF"       "|EAST"       "|EASF01"       "|EASF02"       "|EASF03"       "|EASF04"       "|EASF05"       "|EASF06"       "|EASF07"       "|EASF08"       "|EASF09"       "|EASF10"       "|EAIT"       "|IRMS1"       "|IRMS2"       "|IRMS3"       "|URMS1"        "|URMS2"      "|URMS3"       "|PREF"       "|PCOUP"       "|SINSTS"       "|SINSTS1"       "|SINSTS2"       "|SINSTS3"       "|SINSTI"       "|UMOY1"       "|UMOY2"       "|UMOY3"       "|STGE"       "|DPM1"       "|DPM2"       "|DPM3"       "|FPM1"       "|FPM2"       "|FPM3"       "|RELAIS"        "|PJOURF+1"      "|PPOINTE";

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

static struct {                   // 38 bytes
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
  uint8_t  cal_hexa     = 1;                            // flag to set format of period profiles as hexa
  uint8_t  led_period   = 0;                            // adjust LED color according to period color 
  long     prod_trigger = 0;                            // average production power to trigger production relay 
  long     param[TIC_CONFIG_MAX];                       // generic params
} teleinfo_config;

// teleinfo : calendar
// -------------------

struct tic_cal_day {               // 53 bytes
  uint8_t  level;                                       // maximum level of the day
  uint8_t  arr_slot[TIC_DAY_SLOT_MAX];                  // period slots within the day
  uint32_t date;                                        // date in YYYYMMDD00 format
};

tic_cal_day teleinfo_calendar[TIC_DAY_MAX];             // today, tomorrow and day after calendar, 169 bytes

// teleinfo : current message
// --------------------------

struct tic_line {                 // esp8266 37 bytes, esp32 125 bytes
  char str_etiquette[TIC_KEY_MAX];                      // line etiquette
  char str_donnee[TIC_DATA_MAX];                        // line donnee
  char checksum;                                        // line checksum
};

struct tic_pointe {               // 8 bytes
  uint32_t start;                               // start date with slot
  uint32_t stop;                                // stop date with slot
};

struct tic_stge {                 // 4 bytes
  uint8_t  over_load;                           // currently overloaded
  uint8_t  over_volt;                           // voltage overload on one phase
  uint8_t  pointe;                              // current pointe mobile
  uint8_t  preavis;                             // pointe mobile preavis
};

static struct {                   // esp8266 4469 bytes, esp32 18825 bytes
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
  tic_pointe  arr_pointe[TIC_POINTE_MAX];       // array of pointe dates, 24 bytes
  tic_line    arr_line[TIC_LINE_QTY];           // array of lines in current message, esp8266 2072 bytes, esp32 9250 bytes
  tic_line    arr_last[TIC_LINE_QTY];           // array of lines in last message received, esp8266 2072 bytes, esp32 9250 bytes
  tic_cal_day cal_default;                      // default daily profile
  tic_cal_day cal_pointe;                       // pointe daily profile
} teleinfo_message;

// teleinfo : contract
// -------------------

struct tic_period {               // 11 bytes
  uint8_t valid;                                // period validity
  uint8_t level;                                // period level
  uint8_t hchp;                                 // period hp flag
  String  str_code;                             // period code
  String  str_label;                            // period label
};

static struct {                   // 209 bytes
  uint8_t   changed    = 0;                     // flag to indicate that contract has changed
  uint8_t   unit       = TIC_UNIT_NONE;         // default contract unit
  uint8_t   index      = UINT8_MAX;             // actual contract index
  uint8_t   mode       = TIC_MODE_UNKNOWN;      // meter mode (historic, standard)

  uint8_t   period     = UINT8_MAX;             // period - current index
  uint8_t   period_qty = 0;                     // period - number of indexes

  uint8_t   phase      = 1;                     // number of phases
  long      isousc     = 0;                     // contract max current per phase
  long      ssousc     = 0;                     // contract max power per phase

  char       str_code[TIC_CODE_SIZE];           // code of current contract
  char       str_period[TIC_CODE_SIZE];         // code of current period
  tic_period arr_period[TIC_INDEX_MAX];         // periods in the contract, 154 bytes
} teleinfo_contract;

// teleinfo : meter
// ----------------

struct tic_preavis {                // 13 bytes
  uint8_t  level;                               // level of current preavis
  uint32_t timeout;                             // timeout of current preavis
  char     str_label[8];                        // label of current preavis
};

struct tic_json {                   // 3 bytes
  uint8_t data;                                 // flag to publish ALERT, METER, RELAY, CONTRACT or CAL
  uint8_t tic;                                  // flag to publish TIC
  uint8_t live;                                 // flag to publish LIVE
};

static struct {                     // 71 bytes
  char      sep_line   = 0;                     // detected line separator
  uint8_t   company    = 0;                     // manufacturer of the meter
  uint8_t   model      = 0;                     // model of the meter
  uint8_t   serial     = TIC_SERIAL_INIT;       // serial port status
  uint8_t   reception  = TIC_RECEPTION_NONE;    // reception phase
  uint8_t   use_sinsts = 0;                     // flag to use sinsts etiquette for papp
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

static struct {                     // 11 bytes
  uint8_t  state     = TIC_LED_PWR_OFF;         // current LED state
  uint8_t  status    = TIC_LED_STEP_NONE;       // meter LED status
  uint8_t  level     = TIC_LEVEL_NONE;          // period level
  uint32_t msg_time  = UINT32_MAX;              // timestamp of last reception 
  uint32_t upd_time  = UINT32_MAX;              // timestamp of last LED update 
} teleinfo_led;

// teleinfo : conso mode
// ---------------------

struct tic_cosphi {                 // 142 bytes
  uint8_t index;                                // index of cosphy array current value
  uint8_t page;                                 // index of current power page
  long value;                                   // current value of cosphi
  long quantity;                                // number of measure done 
  long nb_message;                              // number of messages with last cosphi calculation 
  long arr_value[TIC_COSPHI_SAMPLE];            // array of cosphi values
  long arr_page[TIC_COSPHI_PAGE];               // array of average cosphi for power pages 
}; 

struct tic_phase {                  // 34 bytes
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

static struct {                     // 441 bytes
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
  tic_phase  phase[TIC_PHASE_MAX];              // phases data, 102 bytes
} teleinfo_conso;

// teleinfo : production mode
// --------------------------

static struct {                     // 191 bytes
  uint8_t relay         = 0;                   // production relay status
  float   pact_avg      = 0;                   // average produced active power

  long    papp          = 0;                   // production instant apparent power 
  long    papp_last     = 0;                   // last published apparent power
  long    pact          = 0;                   // production instant active power
  long    delta_mwh     = 0;                   // active conso delta since last total (milli Wh)
  long    delta_mvah    = 0;                   // apparent power counter increment (milli VAh)
  long    today_wh      = 0;                   // active power produced today (Wh)
  long    yesterday_wh  = 0;                   // active power produced yesterday (Wh)

  long long midnight_wh = 0;                   // active power total last midnight (Wh)
  long long total_wh    = 0;                   // active power total

  tic_cosphi cosphi;                            // global cosphi data 
} teleinfo_prod;

#endif      // USE_TELEINFO
#endif      // USE_ENERGY_SENSOR
