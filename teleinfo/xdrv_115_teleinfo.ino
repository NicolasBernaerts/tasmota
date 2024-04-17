/*
  xdrv_115_teleinfo.ino - France Teleinfo energy sensor support for Tasmota devices

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
    05/03/2024 - v3.1 - Removal of all float and double calculations
    27/03/2024 - v3.2 - Section COUNTER renamed as CONTRACT with addition of contract data

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
#define XDRV_115                        115

// teleinfo constant
#define TELEINFO_VOLTAGE                230       // default voltage provided
#define TELEINFO_VOLTAGE_MAXIMUM        260       // maximum voltage for value to be acceptable
#define TELEINFO_VOLTAGE_MINIMUM        100       // minimum voltage for value to be acceptable
#define TELEINFO_VOLTAGE_REF            200       // voltage reference for max power calculation
#define TELEINFO_INDEX_MAX              18        // maximum number of total power counters
#define TELEINFO_PERCENT_MIN            1         // minimum acceptable percentage of energy contract
#define TELEINFO_PERCENT_MAX            200       // maximum acceptable percentage of energy contract
#define TELEINFO_PERCENT_CHANGE         4         // 5% of power change to publish JSON
#define TELEINFO_FILE_DAILY             7         // default number of daily files
#define TELEINFO_FILE_WEEKLY            8         // default number of weekly files

// publication constants
#define TELEINFO_MESSAGE_MIN            1         // minimum number of messages to publish data
#define TELEINFO_MESSAGE_DELAY          500       // delay between messages (to avoid serial reception errors)
#define TELEINFO_COSPHI_DEFAULT         850       // default start value for cosphi
#define TELEINFO_COSPHI_SAMPLE          8         // number cosphi of samples to calculate cosphi (minimum = 4)

// data default and boundaries
#define TELEINFO_GRAPH_INC_VOLTAGE      5
#define TELEINFO_GRAPH_MIN_VOLTAGE      235
#define TELEINFO_GRAPH_DEF_VOLTAGE      240       // default voltage maximum in graph
#define TELEINFO_GRAPH_MAX_VOLTAGE      265
#define TELEINFO_GRAPH_INC_POWER        3000
#define TELEINFO_GRAPH_MIN_POWER        3000
#define TELEINFO_GRAPH_DEF_POWER        6000      // default power maximum consumption in graph
#define TELEINFO_GRAPH_MAX_POWER        150000
#define TELEINFO_GRAPH_INC_WH_HOUR      2
#define TELEINFO_GRAPH_MIN_WH_HOUR      2
#define TELEINFO_GRAPH_DEF_WH_HOUR      2         // default max hourly active power consumption in year graph
#define TELEINFO_GRAPH_MAX_WH_HOUR      50
#define TELEINFO_GRAPH_INC_WH_DAY       10
#define TELEINFO_GRAPH_MIN_WH_DAY       10
#define TELEINFO_GRAPH_DEF_WH_DAY       10         // default max daily active power consumption in year graph
#define TELEINFO_GRAPH_MAX_WH_DAY       200
#define TELEINFO_GRAPH_INC_WH_MONTH     100
#define TELEINFO_GRAPH_MIN_WH_MONTH     100
#define TELEINFO_GRAPH_DEF_WH_MONTH     100       // default max monthly active power consumption in year graph
#define TELEINFO_GRAPH_MAX_WH_MONTH     50000

// timeout and delays
#define TELEINFO_RECV_TIMEOUT_DATA      95        // message reception timeout during data update (in ms)
#define TELEINFO_RECV_TIMEOUT_LOOP      195       // message reception timeout during loop update (in ms)
#define TELEINFO_PREAVIS_TIMEOUT        15        // timeout to reset preavis flag (in sec.)

// string size
#define TELEINFO_LINE_QTY               74        // maximum number of lines handled in a TIC message
#define TELEINFO_LINE_MAX               128       // maximum size of a received TIC line
#define TELEINFO_KEY_MAX                14        // maximum size of a TIC etiquette
#define TELEINFO_DATA_MAX               112       // maximum size of a TIC donnee

// RGB limits
#define TELEINFO_RGB_RED_MAX            208
#define TELEINFO_RGB_GREEN_MAX          176

// graph data
#define TELEINFO_GRAPH_SAMPLE           300       // number of samples per graph data
#define TELEINFO_GRAPH_WIDTH            1200      // graph width
#define TELEINFO_GRAPH_HEIGHT           600       // default graph height
#define TELEINFO_GRAPH_STEP             100       // graph height mofification step
#define TELEINFO_GRAPH_PERCENT_START    5         // start position of graph window
#define TELEINFO_GRAPH_PERCENT_STOP     95        // stop position of graph window
#define TELEINFO_GRAPH_MAX_BARGRAPH     32        // maximum number of bar graph

// colors for conso and prod
#define TELEINFO_COLOR_PROD             "#1c0"
#define TELEINFO_COLOR_PROD_PREV        "#160"
#define TELEINFO_COLOR_CONSO            "#6cf"
#define TELEINFO_COLOR_CONSO_PREV       "#069"

// commands : MQTT
#define D_CMND_TELEINFO_RATE            "rate"
#define D_CMND_TELEINFO_POLICY          "policy"
#define D_CMND_TELEINFO_METER           "meter"
#define D_CMND_TELEINFO_TIC             "tic"
#define D_CMND_TELEINFO_CALENDAR        "calendar"
#define D_CMND_TELEINFO_RELAY           "relay"
#define D_CMND_TELEINFO_CONTRACT        "contract"

#define D_CMND_TELEINFO_LOG_DAY         "nbday"
#define D_CMND_TELEINFO_LOG_WEEK        "nbweek"

#define D_CMND_TELEINFO_MAX_V           "maxv"
#define D_CMND_TELEINFO_MAX_VA          "maxva"
#define D_CMND_TELEINFO_MAX_KWH_HOUR    "maxhour"
#define D_CMND_TELEINFO_MAX_KWH_DAY     "maxday"
#define D_CMND_TELEINFO_MAX_KWH_MONTH   "maxmonth"

// commands : Web
#define D_CMND_TELEINFO_ETH             "eth"
#define D_CMND_TELEINFO_PHASE           "phase"
#define D_CMND_TELEINFO_PERIOD          "period"
#define D_CMND_TELEINFO_HISTO           "histo"
#define D_CMND_TELEINFO_HOUR            "hour"
#define D_CMND_TELEINFO_DAY             "day"
#define D_CMND_TELEINFO_MONTH           "month"
#define D_CMND_TELEINFO_DATA            "data"
#define D_CMND_TELEINFO_PREV            "prev"
#define D_CMND_TELEINFO_NEXT            "next"
#define D_CMND_TELEINFO_MINUS           "minus"
#define D_CMND_TELEINFO_PLUS            "plus"
#define D_CMND_TELEINFO_TODAY_CONSO     "today-conso"
#define D_CMND_TELEINFO_TODAY_PROD      "today-prod"
#define D_CMND_TELEINFO_YESTERDAY_CONSO "yesterday-conso"
#define D_CMND_TELEINFO_YESTERDAY_PROD  "yesterday-prod"

// JSON TIC extensions
#define TELEINFO_JSON_TIC               "TIC"
#define TELEINFO_JSON_METER             "METER"
#define TELEINFO_JSON_PROD              "PROD"
#define TELEINFO_JSON_ALERT             "ALERT"

// interface strings
#define D_TELEINFO                      "Teleinfo"
#define D_TELEINFO_MESSAGE              "Message"
#define D_TELEINFO_GRAPH                "Graph"
#define D_TELEINFO_CONTRACT             "Contract"
#define D_TELEINFO_HEURES               "Heures"
#define D_TELEINFO_ERROR                "Errors"
#define D_TELEINFO_MODE                 "Mode"
#define D_TELEINFO_UPDATE               "Cosφ updates"
#define D_TELEINFO_RESET                "Message reset"
#define D_TELEINFO_PERIOD               "Period"

// configuration file
#define D_TELEINFO_CFG                  "/teleinfo.cfg"

// web URL
const char D_TELEINFO_PAGE_CONFIG[]     PROGMEM = "/config";
const char D_TELEINFO_ICON_LINKY_PNG[]  PROGMEM = "/linky.png";
const char D_TELEINFO_PAGE_TIC[]        PROGMEM = "/tic";
const char D_TELEINFO_PAGE_TIC_UPD[]    PROGMEM = "/tic.upd";

// Historic data files
#define TELEINFO_HISTO_DAY_DEFAULT      8         // default number of daily historisation files
#define TELEINFO_HISTO_DAY_MAX          31        // max number of daily historisation files
#define TELEINFO_HISTO_WEEK_DEFAULT     4         // default number of weekly historisation files
#define TELEINFO_HISTO_WEEK_MAX         52        // max number of weekly historisation files

// MQTT EnergyConfig commands
const char kTeleinfoCommands[] PROGMEM =    "historique"   "|"   "standard"   "|"   "percent"   "|"   "stats"   "|"   "error"   "|" D_CMND_TELEINFO_POLICY "|" D_CMND_TELEINFO_METER "|" D_CMND_TELEINFO_TIC "|" D_CMND_TELEINFO_CALENDAR "|" D_CMND_TELEINFO_RELAY "|" D_CMND_TELEINFO_CONTRACT "|" D_CMND_TELEINFO_LOG_DAY "|" D_CMND_TELEINFO_LOG_WEEK "|" D_CMND_TELEINFO_MAX_V "|" D_CMND_TELEINFO_MAX_VA "|" D_CMND_TELEINFO_MAX_KWH_HOUR "|" D_CMND_TELEINFO_MAX_KWH_DAY "|" D_CMND_TELEINFO_MAX_KWH_MONTH;
enum TeleinfoCommand                   { TIC_CMND_HISTORIQUE, TIC_CMND_STANDARD, TIC_CMND_PERCENT, TIC_CMND_STATS, TIC_CMND_ERROR,      TIC_CMND_POLICY     ,     TIC_CMND_METER      ,      TIC_CMND_TIC     ,      TIC_CMND_CALENDAR     ,      TIC_CMND_RELAY     ,      TIC_CMND_CONTRACT     ,      TIC_CMND_LOG_DAY     ,      TIC_CMND_LOG_WEEK     ,      TIC_CMND_MAX_V     ,      TIC_CMND_MAX_VA     ,      TIC_CMND_MAX_KWH_HOUR     ,     TIC_CMND_MAX_KWH_DAY      ,     TIC_CMND_MAX_KWH_MONTH };

// Data diffusion policy
enum TeleinfoMessagePolicy { TELEINFO_POLICY_TELEMETRY, TELEINFO_POLICY_PERCENT, TELEINFO_POLICY_MESSAGE, TELEINFO_POLICY_MAX };
const char kTeleinfoMessagePolicy[] PROGMEM = "Telemetrie seulement|± 5% Evolution puissance|Tous les messages TIC";

// config
enum TeleinfoConfigKey { TELEINFO_CONFIG_NBDAY, TELEINFO_CONFIG_NBWEEK, TELEINFO_CONFIG_MAX_HOUR, TELEINFO_CONFIG_MAX_DAY, TELEINFO_CONFIG_MAX_MONTH, TELEINFO_CONFIG_TODAY_CONSO, TELEINFO_CONFIG_TODAY_PROD, TELEINFO_CONFIG_YESTERDAY_CONSO, TELEINFO_CONFIG_YESTERDAY_PROD, TELEINFO_CONFIG_MAX };     // configuration parameters
const char kTeleinfoConfigKey[] PROGMEM = D_CMND_TELEINFO_LOG_DAY "|" D_CMND_TELEINFO_LOG_WEEK "|" D_CMND_TELEINFO_MAX_KWH_HOUR "|" D_CMND_TELEINFO_MAX_KWH_DAY "|" D_CMND_TELEINFO_MAX_KWH_MONTH "|" D_CMND_TELEINFO_TODAY_CONSO "|" D_CMND_TELEINFO_TODAY_PROD "|" D_CMND_TELEINFO_YESTERDAY_CONSO "|" D_CMND_TELEINFO_YESTERDAY_PROD;      // configuration keys

// published data 
enum TeleinfoPublish { TIC_PUB_CONNECT,
                       TIC_PUB_CONTRACT, TIC_PUB_CONTRACT_NAME,
                       TIC_PUB_CALENDAR, TIC_PUB_CALENDAR_PERIOD, TIC_PUB_CALENDAR_COLOR, TIC_PUB_CALENDAR_HOUR, TIC_PUB_CALENDAR_TODAY, TIC_PUB_CALENDAR_TOMRW, 
                       TIC_PUB_PROD, TIC_PUB_PROD_P, TIC_PUB_PROD_W, TIC_PUB_PROD_C, TIC_PUB_PROD_YDAY, TIC_PUB_PROD_2DAY,  
                       TIC_PUB_CONSO, TIC_PUB_CONSO_U, TIC_PUB_CONSO_I, TIC_PUB_CONSO_P, TIC_PUB_CONSO_W, TIC_PUB_CONSO_C, TIC_PUB_CONSO_YDAY, TIC_PUB_CONSO_2DAY, 
                       TIC_PUB_PH1, TIC_PUB_PH1_U, TIC_PUB_PH1_I, TIC_PUB_PH1_P, TIC_PUB_PH1_W, 
                       TIC_PUB_PH2, TIC_PUB_PH2_U, TIC_PUB_PH2_I, TIC_PUB_PH2_P, TIC_PUB_PH2_W,
                       TIC_PUB_PH3, TIC_PUB_PH3_U, TIC_PUB_PH3_I, TIC_PUB_PH3_P, TIC_PUB_PH3_W,   
                       TIC_PUB_RELAY, TIC_PUB_RELAY_DATA,
                       TIC_PUB_TOTAL, TIC_PUB_TOTAL_PROD, TIC_PUB_TOTAL_CONSO, 
                       TIC_PUB_MAX, 
                       TIC_PUB_DISCONNECT };

// power calculation modes
enum TeleinfoContractUnit { TIC_UNIT_NONE, TIC_UNIT_KVA, TIC_UNIT_KW, TIC_UNIT_MAX };

// contract periods
enum TeleinfoPeriodDay    {TIC_DAY_YESTERDAY, TIC_DAY_TODAY, TIC_DAY_TOMORROW, TIC_DAY_MAX};
const char kTeleinfoPeriodDay[] PROGMEM = "Hier|Aujourd'hui|Demain";

// contract hours
const char kTeleinfoPeriodHour[] PROGMEM = "Heure creuse|Heure pleine";

// contract periods
enum TeleinfoPeriodLevel  { TIC_LEVEL_NONE, TIC_LEVEL_BLUE, TIC_LEVEL_WHITE, TIC_LEVEL_RED, TIC_LEVEL_MAX };
const char kTeleinfoPeriodLevel[] PROGMEM = "Inconnu|Bleu|Blanc|Rouge";

// preavis levels
enum TeleinfoPreavisLevel {TIC_PREAVIS_NONE, TIC_PREAVIS_WARNING, TIC_PREAVIS_ALERT, TIC_PREAVIS_DANGER, TIC_PREAVIS_MAX};

// serial port management
enum TeleinfoSerialStatus { TIC_SERIAL_INIT, TIC_SERIAL_ACTIVE, TIC_SERIAL_STOPPED, TIC_SERIAL_FAILED, TIC_SERIAL_MAX };
const char kTeleinfoSerialStatus[] PROGMEM = "Non configuré|Réception en cours|Réception arrêtée|Erreur d'initialisation";
enum TeleinfoReceptionStatus { TIC_RECEPTION_NONE, TIC_RECEPTION_MESSAGE, TIC_RECEPTION_LINE, TIC_RECEPTION_MAX };

// contract colors
const char kTeleinfoCalendarDot[]   PROGMEM = "⚪|⚪|⚫|⚪";
const char kTeleinfoCalendarColor[] PROGMEM = "#252525|#06b|#ccc|#b00";

#ifdef USE_UFILESYS

// graph - periods
enum TeleinfoGraphPeriod { TELEINFO_CURVE_LIVE, TELEINFO_CURVE_DAY, TELEINFO_CURVE_WEEK, TELEINFO_HISTO_CONSO, TELEINFO_HISTO_PROD, TELEINFO_PERIOD_MAX };    // available graph periods

#else

// graph - periods
enum TeleinfoGraphPeriod { TELEINFO_CURVE_LIVE, TELEINFO_PERIOD_MAX };                                           // available graph periods

#endif    // USE_UFILESYS

// teleinfo relays
const char kTeleinfoRelayName[] PROGMEM  = "Eau Chaude Sanitaire|Chauffage Principal|Chauffage Secondaire|Clim / Pompe à Chaleur|Véhicule Electrique|Stockage & Injection|Réservé|Réservé";

// graph - display
enum TeleinfoGraphDisplay { TELEINFO_UNIT_VA, TELEINFO_UNIT_W, TELEINFO_UNIT_V, TELEINFO_UNIT_COSPHI, TELEINFO_UNIT_PEAK_VA, TELEINFO_UNIT_PEAK_V, TELEINFO_UNIT_WH, TELEINFO_UNIT_MAX };       // available graph units
const char kTeleinfoGraphDisplay[] PROGMEM = "VA|W|V|cosφ|VA|V|Wh";                                                                                                                             // units labels

// week days name for history
static const char kWeekdayNames[] = D_DAY3LIST;

// phase colors
const char kTeleinfoColorPhase[] PROGMEM = "#09f|#f90|#093";                   // phase colors (blue, orange, green)
const char kTeleinfoColorPeak[]  PROGMEM = "#5ae|#eb6|#2a6";                   // peak colors (blue, orange, green)

// -------------------
//  TELEINFO Protocol
// -------------------

// Specific etiquettes
enum TeleinfoEtiquette { TIC_NONE, TIC_ADCO, TIC_ADSC, TIC_PTEC, TIC_OPTARIF, TIC_NGTF, TIC_DATE, TIC_DATECOUR, TIC_MESURES1, TIC_EAIT, TIC_IINST, TIC_IINST1, TIC_IINST2, TIC_IINST3, TIC_I1, TIC_I2, TIC_I3, TIC_ISOUSC, TIC_PS, TIC_PAPP, TIC_SINSTS, TIC_SINST1, TIC_SINST2, TIC_SINST3, TIC_SINSTI, TIC_BASE, TIC_EAST, TIC_HCHC, TIC_HCHP, TIC_EJPHN, TIC_EJPHPM, TIC_BBRHCJB, TIC_BBRHPJB, TIC_BBRHCJW, TIC_BBRHPJW, TIC_BBRHCJR, TIC_BBRHPJR, TIC_ADPS, TIC_ADIR1, TIC_ADIR2, TIC_ADIR3, TIC_URMS1, TIC_URMS2, TIC_URMS3, TIC_UMOY1, TIC_UMOY2, TIC_UMOY3, TIC_IRMS1, TIC_IRMS2, TIC_IRMS3, TIC_SINSTS1, TIC_SINSTS2, TIC_SINSTS3, TIC_PTCOUR, TIC_PTCOUR1, TIC_PTCOUR2, TIC_PREF, TIC_PCOUP, TIC_LTARF, TIC_EASF01, TIC_EASF02, TIC_EASF03, TIC_EASF04, TIC_EASF05, TIC_EASF06, TIC_EASF07, TIC_EASF08, TIC_EASF09, TIC_EASF10, TIC_ADS, TIC_CONFIG, TIC_EAPS, TIC_EAS, TIC_EAPPS, TIC_PREAVIS, TIC_PEJP, TIC_STGE, TIC_DPM1, TIC_FPM1, TIC_PJOURF1, TIC_PPOINTE, TIC_RELAIS, TIC_CONTRAT, TIC_EAPP, TIC_EAPPM, TIC_EAPHPH, TIC_EAPHPD, TIC_EAPHCH, TIC_EAPHCD, TIC_EAPHPE, TIC_EAPHCE, TIC_EAPJA, TIC_EAPHH, TIC_EAPHD, TIC_EAPHM, TIC_EAPDSM, TIC_EAPSCM, TIC_U10MN, TIC_EA, TIC_ERP, TIC_PSP, TIC_PSPM, TIC_PSHPH, TIC_PSHPD, TIC_PSHCH, TIC_PSHCD, TIC_PSHPE, TIC_PSHCE, TIC_PSJA, TIC_PSHH, TIC_PSHD, TIC_PSHM, TIC_PSDSM, TIC_PSSCM, TIC_MAX };
const char kTeleinfoEtiquetteName[] PROGMEM = "|ADCO|ADSC|PTEC|OPTARIF|NGTF|DATE|DATECOUR|MESURES1|EAIT|IINST|IINST1|IINST2|IINST3|I1|I2|I3|ISOUSC|PS|PAPP|SINSTS|SINST1|SINST2|SINST3|SINSTI|BASE|EAST|HCHC|HCHP|EJPHN|EJPHPM|BBRHCJB|BBRHPJB|BBRHCJW|BBRHPJW|BBRHCJR|BBRHPJR|ADPS|ADIR1|ADIR2|ADIR3|URMS1|URMS2|URMS3|UMOY1|UMOY2|UMOY3|IRMS1|IRMS2|IRMS3|SINSTS1|SINSTS2|SINSTS3|PTCOUR|PTCOUR1|PTCOUR2|PREF|PCOUP|LTARF|EASF01|EASF02|EASF03|EASF04|EASF05|EASF06|EASF07|EASF08|EASF09|EASF10|ADS|CONFIG|EAP_s|EA_s|EAPP_s|PREAVIS|PEJP|STGE|DPM1|FPM1|PJOURF+1|PPOINTE|RELAIS|CONTRAT|EApP|EApPM|EApHPH|EApHPD|EApHCH|EApHCD|EApHPE|EApHCE|EApJA|EApHH|EApHD|EApHM|EApDSM|EApSCM|U10MN|EA|ERP|PSP|PSPM|PSHPH|PSHPD|PSHCH|PSHCD|PSHPE|PSHCE|PSJA|PSHH|PSHD|PSHM|PSDSM|PSSCM";

// Meter type
enum TeleinfoType { TIC_TYPE_UNDEFINED, TIC_TYPE_CONSO, TIC_TYPE_PROD, TIC_TYPE_MAX };
const char kTeleinfoTypeName[] PROGMEM = "Non déterminé|Consommateur|Producteur";

// Meter mode
enum TeleinfoMode { TIC_MODE_UNDEFINED, TIC_MODE_HISTORIC, TIC_MODE_STANDARD, TIC_MODE_PMEPMI, TIC_MODE_EMERAUDE, TIC_MODE_MAX };
const char kTeleinfoModeName[] PROGMEM = "|Historique|Standard|PME-PMI|Emeraude";
const char kTeleinfoModeIcon[] PROGMEM = "|🇭|🇸|🇵|🇪";

// Type of contracts
enum TicContract  { TIC_C_UNDEFINED, TIC_C_HISTO_BASE, TIC_C_HISTO_HCHP, TIC_C_HISTO_EJP, TIC_C_HISTO_TEMPO, TIC_C_STD_BASE, TIC_C_STD_HCHP, TIC_C_STD_EJP, TIC_C_STD_TEMPO, TIC_C_BT4SUP36, TIC_C_BT5SUP36, TIC_C_TJEJP, TIC_C_HTA5, TIC_C_HTA8, TIC_C_A5_BASE, TIC_C_A8_BASE, TIC_C_A5_EJP, TIC_C_A8_EJP, TIC_C_A8_MOD, TIC_C_JAUNE_BASE, TIC_C_JAUNE_EJP, TIC_C_MAX };
const char kTicContractCode[] PROGMEM = "UNDEF|TH..|HC..|EJP.|BBR|BASE|H PLEINE/CREUSE|EJP|TEMPO|BT 4 SUP36|BT 5 SUP36|TJ EJP|HTA 5|HTA 8|BASE_A5|BASE_A8|EJP_A5|EJP_A8|MOD|JBASE|JEJP";
const char kTicContractName[] PROGMEM = "Indéterminé|Base|HC/HP|EJP|Tempo|Base|HC/HP|EJP|Tempo|BT>36kVA 4p.|BT>36kVA 5p.|Jaune EJP|HTA 5p|HTA 8p|A5 Base|A8 Base|A5 EJP|A8 EJP|A8 Mod.|Jaune Base|Jaune EJP";

// contrat de type inconnu
const char    kTicPeriodUndefCode[] PROGMEM = "PERIOD1|PERIOD2|PERIOD3|PERIOD4|PERIOD5|PERIOD6|PERIOD7|PERIOD8|PERIOD9|PERIOD10";
const char    kTicPeriodUndefName[] PROGMEM = "Période n°1|Période n°2|Période n°3|Période n°4|Période n°5|Période n°6|Période n°7|Période n°8|Période n°9|Période n°10";
const uint8_t arrPeriodUndefLevel[]         = {    1,          1,          1,          1,          1,          1,          1,          1,          1,          1       };
const uint8_t arrPeriodUndefHP[]            = {    1,          1,          1,          1,          1,          1,          1,          1,          1,          1       };

// contrat Historique Base (1 period)
const char    kTicPeriodHistoBaseCode[] PROGMEM = "TH..";
const char    kTicPeriodHistoBaseName[] PROGMEM = "Toutes Heures";
const uint8_t arrPeriodHistoBaseLevel[]         = {      1      };
const uint8_t arrPeriodHistoBaseHP[]            = {      1      };

// contrat Historique Heure Pleine / Heure creuse : 2 periods
const char    kTicPeriodHistoHcHpCode[] PROGMEM = "HC..|HP..";
const char    kTicPeriodHistoHcHpName[] PROGMEM = "Heures Creuses|Heures Pleines";
const uint8_t arrPeriodHistoHcHpLevel[]         = {   1,      1    };
const uint8_t arrPeriodHistoHcHpHP[]            = {   0,      1    };

// contrat Historique EJP : 2 periods
const char    kTicPeriodHistoEjpCode[] PROGMEM = "HN..|PM..";
const char    kTicPeriodHistoEjpName[] PROGMEM = "Normale|Pointe Mobile";
const uint8_t arrPeriodHistoEjpLevel[]         = {   1,          3,     };
const uint8_t arrPeriodHistoEjpHP[]            = {   1,          1,     };

// contrat Historique Tempo : 6 periods
const char    kTicPeriodHistoTempoCode[] PROGMEM = "HCJB|HPJB|HCJW|HPJW|HCJR|HPJR";
const char    kTicPeriodHistoTempoName[] PROGMEM = "Creuses Bleu|Pleines Bleu|Creuses Blanc|Pleines Blanc|Creuses Rouge|Pleines Rouge";
const uint8_t arrPeriodHistoTempoLevel[]         = {      1,           1,           2,            2,            3,            3      };
const uint8_t arrPeriodHistoTempoHP[]            = {      0,           1,           0,            1,            0,            1      };

// contrat Standard Base (1 period)
const char    kTicPeriodStandardBaseCode[] PROGMEM = "BASE";
const char    kTicPeriodStandardBaseName[] PROGMEM = "Toutes Heures";
const uint8_t arrPeriodStandardBaseLevel[]         = {      1      };
const uint8_t arrPeriodStandardBaseHP[]            = {      1      };

// contrat Standard Heure Pleine / Heure creuse : 2 periods
const char    kTicPeriodStandardHcHpCode[] PROGMEM = "HEURE CREUSE|HEURE PLEINE";
const char    kTicPeriodStandardHcHpName[] PROGMEM = "Heures Creuses|Heures Pleines";
const uint8_t arrPeriodStandardHcHpLevel[]         = {      1,             1        };
const uint8_t arrPeriodStandardHcHpHP[]            = {      0,             1        };

// contrat Standard EJP : 2 periods
const char    kTicPeriodStandardEjpCode[] PROGMEM = "HEURE NORMALE|HEURE POINTE";
const char    kTicPeriodStandardEjpName[] PROGMEM = "Normale|Pointe Mobile";
const uint8_t arrPeriodStandardEjpLevel[]         = {   1,          3      };
const uint8_t arrPeriodStandardEjpHP[]            = {   1,          1      };

// contrat Standard Tempo : 6 periods
const char    kTicPeriodStandardTempoCode[] PROGMEM = "HC BLEU|HP BLEU|HC BLANC|HP BLANC|HC ROUGE|HP ROUGE";
const char    kTicPeriodStandardTempoName[] PROGMEM = "Creuses Bleu|Pleines Bleu|Creuses Blanc|Pleines Blanc|Creuses Rouge|Pleines Rouge";
const uint8_t arrPeriodStandardTempoLevel[]         = {      1,           1,           2,            2,            3,            3       };
const uint8_t arrPeriodStandardTempoHP[]            = {      0,           1,           0,            1,            0,            1       };

// contrat PME/PMI : 16 periods
const char    kTicPeriodPmePmiCode[] PROGMEM = "P|PM|HH|HM|HP|HC|HPH|HCH|HPE|HCE|HPD|HCD|HD|JA|DSM|SCM";
const char    kTicPeriodPmePmiName[] PROGMEM = "Pointe|Pointe Mobile|Hiver|Hiver Mobile|Heures Pleines|Heures Creuses|Pleines Hiver|Creuses Hiver|Pleines Eté|Creuses Eté|Pleines 1/2 saison|Creuses 1/2 saison|1/2 saison|Juillet-Août|1/2 saison Mobile|Saison Creuse Mobile";
const uint8_t arrPeriodPmePmiLevel[]         = {  3,         3,        1,       2,         1,      1,         1,            1,            1,          1,            1,                 1,              1,           1,           2,                  2          };
const uint8_t arrPeriodPmePmiHP[]            = {  1,         1,        0,       1,         1,      0,         1,            0,            1,          0,            1,                 0,              0,           0,           1,                  0          };

// contrat Emeraude 2 quadrants - A5 Base, A8 Basen A5 EJP, A8 EJP, A8 Modulable : 14 periods
const char    kTicPeriodEmeraude2Code[] PROGMEM = "P|PM|HPH|HPD|HCH|HCD|HPE|HCE|JA|HH|HD|HM|DSM|SCM";
const char    kTicPeriodEmeraude2Name[] PROGMEM = "Pointe|Pointe Mobile|Pleines Hiver|Pleines 1/2 Saison|Creuses Hiver|Creuses 1/2 Saison|Pleines Eté|Creuses Eté|Juillet-Aout|Hiver|1/2 Saison|Hiver Mobile|1/2 Saison Mobile|Saison Creuse Mobile";
const uint8_t arrPeriodEmeraude2Level[]         = {  3,         3,            1,              1,               1,              1,               1,          1,           1,      1,      1,          3,             3,                  3          };
const uint8_t arrPeriodEmeraude2HP[]            = {  1,         1,            1,              1,               0,              0,               1,          0,           1,      1,      1,          1,             1,                  0          };

// A CODER ! contrat Jaune Base (5 periods)
const char    kTicPeriodJauneBaseCode[] PROGMEM = "11|12|21|22|23";
const char    kTicPeriodJauneBaseName[] PROGMEM = "Pleines Eté|Creuses Eté|Pleines Hiver|Creuses Hiver|Pointe Fixe";
const uint8_t arrPeriodJauneBaseHP[]            = {      1,          0,          1,            0,           1      };

// A CODER ! contrat Jaune EJP (4 periods)
const char    kTicPeriodJauneEjpCode[] PROGMEM = "11|12|21|44";
const char    kTicPeriodJauneEjpName[] PROGMEM = "Pleines Eté|Creuses Eté|Heures Hiver|Pointe Mobile";
const uint8_t arrPeriodJauneEjpHP[]            = {      1,          0,         1,           1        };

// A CODER ! contrat Emeraude (PTCOUR : 5 periods)
const char    kTicPeriodEmeraude4Code[] PROGMEM = "1|2|3|4|5";
const char    kTicPeriodEmeraude4Name[] PROGMEM = "Pointe|Pleines Hiver|Creuses Hiver|Pleines Eté|Creuses Eté";

/****************************************\
 *                 Data
\****************************************/

// teleinfo : configuration
static struct {
  bool     restart   = false;                           // flag to ask for restart
  uint8_t  battery   = 0;                               // device is running on battery
  uint8_t  percent   = 100;                             // maximum acceptable power in percentage of contract power
  uint8_t  policy    = TELEINFO_POLICY_TELEMETRY;       // data publishing policy
  uint8_t  meter     = 1;                               // publish METER & PROD section
  uint8_t  tic       = 0;                               // publish TIC section
  uint8_t  calendar  = 1;                               // publish CALENDAR section
  uint8_t  relay     = 1;                               // publish RELAY section
  uint8_t  contract  = 1;                               // publish CONTRACT section
  uint8_t  display   = 0;                               // display errors on home page 
  long     max_volt  = TELEINFO_GRAPH_DEF_VOLTAGE;      // maximum voltage on graph
  long     max_power = TELEINFO_GRAPH_DEF_POWER;        // maximum power on graph
  long     param[TELEINFO_CONFIG_MAX] = { TELEINFO_HISTO_DAY_DEFAULT, TELEINFO_HISTO_WEEK_DEFAULT, TELEINFO_GRAPH_DEF_WH_HOUR, TELEINFO_GRAPH_DEF_WH_DAY, TELEINFO_GRAPH_DEF_WH_MONTH, 0, 0, 0, 0 };      // graph configuration
} teleinfo_config;

// teleinfo : current message
struct tic_line {
  String str_etiquette;                           // line etiquette
  String str_donnee;                              // line donnee
  char checksum;                                  // line checksum
};
static struct {
  int      line_index    = 0;                     // index of current received message line
  int      line_last     = 0;                     // number of lines in last message
  int      line_max      = 0;                     // max number of lines in a message
  uint32_t timestamp     = UINT32_MAX;            // timestamp of message (ms)
  long     duration      = 1000;                  // duration of average message (ms)
  char     str_line[TELEINFO_LINE_MAX];           // reception buffer for current line
  tic_line arr_line[TELEINFO_LINE_QTY];           // array of lines in current message
  tic_line arr_last[TELEINFO_LINE_QTY];           // array of lines in last message received
} teleinfo_message;

// teleinfo : contract data
static struct {
  uint8_t   contract   = TIC_C_UNDEFINED;         // actual contract
  uint8_t   mode       = TIC_MODE_UNDEFINED;      // meter mode (historic, standard)
  uint8_t   unit       = TIC_UNIT_NONE;           // default contract unit
  uint8_t   period_qty = 10;                      // default number of periods
  uint8_t   period     = UINT8_MAX;               // default current period
  uint8_t   phase      = 1;                       // number of phases
  long      voltage    = TELEINFO_VOLTAGE;        // contract reference voltage
  long      isousc     = 0;                       // contract max current per phase
  long      ssousc     = 0;                       // contract max power per phase
  long long ident      = 0;                       // contract identification
} teleinfo_contract;

// teleinfo : meter
struct tic_flag {
  uint8_t  overload;                              // currently overloaded
  uint8_t  overvolt;                              // voltage overload on one phase
};
struct tic_preavis {
  uint8_t  level;                                 // level of current preavis
  uint32_t timeout;                               // timeout of current preavis
  char     str_label[8];                          // label of current preavis
};
struct tic_pointe {
  uint8_t  day;                                   // day index of pointe period
  uint8_t  hour;                                  // hour slot when pointe period starts
  uint16_t crc_start;                             // CRC of pointe start date
  uint16_t crc_profile;                           // CRC of pointe profile
  uint32_t start;                                 // start time of pointe period
};
struct tic_day {
  uint8_t  valid;                                 // day has been initialised
  uint8_t  arr_period[24];                        // hourly slots periods
};
struct tic_json {
  uint8_t   published;                            // flag to first publication
  uint8_t   meter;                                // flag to publish METER
  uint8_t   alert;                                // flag to publish ALERT
  uint8_t   contract;                             // flag to publish CONTRACT
  uint8_t   relay;                                // flag to publish RELAY
  uint8_t   tic;                                  // flag to publish TIC
  uint8_t   calendar;                             // flag to publish CAL
};
static struct {
  uint8_t     serial     = TIC_SERIAL_INIT;       // serial port status
  uint8_t     reception  = TIC_RECEPTION_NONE;    // reception phase 
  uint8_t     use_sinsts = 0;                     // flag to use sinsts etiquette for papp
  uint32_t    days       = 0;                     // current forever days index
  uint32_t    last_msg   = 0;                     // timestamp of last trasnmitted message
  long        nb_message = 0;                     // total number of messages sent by the meter
  long        nb_reset   = 0;                     // total number of message reset sent by the meter
  long long   nb_line    = 0;                     // total number of received lines
  long long   nb_error   = 0;                     // total number of checksum errors
  char        sep_line   = 0;                     // detected line separator
  uint8_t     relay      = 0;                     // global relays status
  tic_day     arr_day[TIC_DAY_MAX];               // today, tomorrow and day after tomorrow hourly status
  tic_json    json;                               // JSON publication flags
  tic_flag    flag;                               // STGE data
  tic_preavis preavis;                            // Current preavis
  tic_pointe  pointe;                             // next pointe period
} teleinfo_meter;

struct tic_cosphi {
  long nb_measure;                                // number of measure done 
  long last_papp;                                 // last papp value
  long value;                                     // current value of cosphi
  long arr_value[TELEINFO_COSPHI_SAMPLE];         // array of cosphi values
  
}; 

// teleinfo : conso mode
struct tic_phase {
  bool  volt_set;                                 // voltage set in current message
  long  voltage;                                  // instant voltage (V)
  long  current;                                  // instant current (mA)
  long  papp;                                     // instant apparent power (VA)
  long  sinsts;                                   // instant apparent power (VA)
  long  pact;                                     // instant active power (W)
  long  preact;                                   // instant reactive power (VAr)
  long  papp_last;                                // last published apparent power (VA)
  long  cosphi;                                   // current cos phi (x1000)
}; 
static struct {
  long      papp         = 0;                     // current conso apparent power (VA)
  long      pact         = 0;                     // current conso active power (W)
  long      delta_mwh    = 0;                     // active conso delta since last total (milli Wh)
  long      delta_mvah   = 0;                     // apparent power counter increment (milli VAh)
  long      today_wh     = 0;                     // active power conso today (Wh)
  long      yesterday_wh = 0;                     // active power conso testerday (Wh)
  long long midnight_wh  = 0;                     // global active conso total at previous midnight (Wh)
  long long total_wh     = 0;                     // global active conso total (Wh)
  long long index_wh[TELEINFO_INDEX_MAX];         // array of conso total of different tarif periods (Wh)
  tic_cosphi cosphi;
  tic_phase  phase[ENERGY_MAX_PHASES];
  
  long papp_now     = LONG_MAX;                   // apparent power current counter (in vah)
  long papp_prev    = LONG_MAX;                   // apparent power previous counter (in vah)
  long pact_now     = LONG_MAX;                   // active power current counter (in wh)
  long pact_prev    = LONG_MAX;                   // active power previous counter (in wh)
  long preact_now   = LONG_MAX;                   // current reactive power counter (in wh)
  long preact_prev  = LONG_MAX;                   // previous reactive power counter (in wh)

  long last_stamp   = LONG_MAX;                   // timestamp of current measure
  long papp_stamp   = LONG_MAX;                   // timestamp of previous apparent power measure
  long pact_stamp   = LONG_MAX;                   // timestamp of previous active power measure
  long preact_stamp = LONG_MAX;                   // timestamp of previous reactive power measure
  long cosphi_stamp = LONG_MAX;                   // timestamp of last cos phi calculation
} teleinfo_conso;

// teleinfo : production mode
static struct {
  long      papp         = 0;                     // production instant apparent power 
  long      pact         = 0;                     // production instant active power
  long      papp_last    = 0;                     // last published apparent power
  long      delta_mwh    = 0;                     // active conso delta since last total (milli Wh)
  long      delta_mvah   = 0;                     // apparent power counter increment (milli VAh)
  long      today_wh     = 0;                     // active power produced today (Wh)
  long      yesterday_wh = 0;                     // active power produced yesterday (Wh)
  long long midnight_wh  = 0;                     // active power total last midnight (Wh)
  long long total_wh     = 0;                     // active power total
  tic_cosphi cosphi;
} teleinfo_prod;

static struct {
  bool    serving = false;                        // flag set when serving graph page

  uint8_t period  = TELEINFO_CURVE_LIVE;          // graph period
  uint8_t data    = TELEINFO_UNIT_VA;             // graph default data
  uint8_t histo   = 0;                            // graph histotisation index
  uint8_t month   = 0;                            // graph current month
  uint8_t day     = 0;                            // graph current day

  long    left;                                   // left position of the curve
  long    right;                                  // right position of the curve
  long    width;                                  // width of the curve (in pixels)
} teleinfo_graph;

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
  int  arg_value = value_default;
  char str_argument[8];

  // check for argument
  if (Webserver->hasArg (pstr_argument))
  {
    WebGetArg (pstr_argument, str_argument, sizeof (str_argument));
    arg_value = atoi (str_argument);
  }

  // check for min and max value
  if ((value_min > 0) && (arg_value < value_min)) arg_value = value_min;
  if ((value_max > 0) && (arg_value > value_max)) arg_value = value_max;

  return arg_value;
}

#endif      // USE_WEBSERVER

/*************************************************\
 *                  Functions
\*************************************************/

// data are ready to be published
bool TeleinfoDriverMeterReady () 
{
  return (teleinfo_meter.nb_message >= TELEINFO_MESSAGE_MIN);
}

// Display any value to a string with unit and kilo conversion with number of digits (12000, 2.0k, 12.0k, 2.3M, ...)
void TeleinfoDriverGetDataDisplay (const int unit_type, const long value, char* pstr_result, const int size_result, bool in_kilo) 
{
  char str_text[16];

  // check parameters
  if (pstr_result == nullptr) return;

  // convert cos phi value
  if (unit_type == TELEINFO_UNIT_COSPHI) sprintf (pstr_result, "%d.%02d", value / 100, value % 100);

  // else convert values in M with 1 digit
  else if (in_kilo && (value > 999999)) sprintf (pstr_result, "%d.%dM", value / 1000000, (value / 100000) % 10);

  // else convert values in k with no digit
  else if (in_kilo && (value > 9999)) sprintf (pstr_result, "%dk", value / 1000);

  // else convert values in k
  else if (in_kilo && (value > 999)) sprintf (pstr_result, "%d.%dk", value / 1000, (value / 100) % 10);

  // else convert value
  else sprintf (pstr_result, "%d", value);

  // append unit if specified
  if (unit_type < TELEINFO_UNIT_MAX) 
  {
    // append unit label
    strcpy (str_text, "");
    GetTextIndexed (str_text, sizeof (str_text), unit_type, kTeleinfoGraphDisplay);
    strlcat (pstr_result, " ", size_result);
    strlcat (pstr_result, str_text, size_result);
  }
}

// Trigger publication flags
void TeleinfoDriverPublishTrigger ()
{
  // set data publication flags
  if (teleinfo_config.relay) teleinfo_meter.json.relay = 1;
  if (teleinfo_config.meter) teleinfo_meter.json.meter = 1;
  if (teleinfo_config.contract) teleinfo_meter.json.contract = 1;
  if (teleinfo_config.calendar) teleinfo_meter.json.calendar = 1;
  if (teleinfo_config.tic) teleinfo_meter.json.tic = 1;

#ifdef USE_TELEINFO_DOMOTICZ
  DomoticzIntegrationPublishTrigger ();
#endif    // USE_TELEINFO_DOMOTICZ
}

// Generate JSON alert data
void TeleinfoDriverPublishAlert ()
{
  // message start
  ResponseClear ();
  ResponseAppendTime ();

  // alert
  ResponseAppend_P (PSTR (",\"ALERT\":{\"Load\":%u,\"Volt\":%u,\"Preavis\":%u,\"Label\":\"%s\"}"), teleinfo_meter.flag.overload, teleinfo_meter.flag.overvolt, teleinfo_meter.preavis.level, teleinfo_meter.preavis.str_label);

  // message end and publication
  ResponseJsonEnd ();
  MqttPublishTeleSensor ();

  // reset JSON flag
  teleinfo_meter.json.alert = 0;

  // update data reception
  TeleinfoProcessRealTime ();
}

// Generate JSON with TIC informations
void TeleinfoDriverPublishTic ()
{
  bool    is_first = true;
  uint8_t index;

  // start of message
  ResponseClear ();
  ResponseAppendTime ();
  ResponseAppend_P (PSTR (",\"TIC\":{"));

  // loop thru TIC message lines to add lines
  for (index = 0; index < TELEINFO_LINE_QTY; index ++)
    if (teleinfo_message.arr_last[index].checksum != 0)
    {
      if (!is_first) ResponseAppend_P (PSTR (",")); else is_first = false;
      ResponseAppend_P (PSTR ("\"%s\":\"%s\""), teleinfo_message.arr_last[index].str_etiquette.c_str (), teleinfo_message.arr_last[index].str_donnee.c_str ());
    }

  // end of message
  ResponseJsonEndEnd ();
  MqttPublishTeleSensor ();

  // reset JSON flag
  teleinfo_meter.json.tic = 0;

  // update data reception
  TeleinfoProcessRealTime ();
}

// Generate JSON data with METER & PROD
void TeleinfoDriverPublishMeter ()
{
  uint8_t phase, value;
  long    voltage, current, power_app, power_act;

  // start of message
  ResponseClear ();
  ResponseAppendTime ();
  ResponseAppend_P (PSTR (",\"METER\":{"));

  // METER basic data
  ResponseAppend_P (PSTR ("\"PH\":%u,\"ISUB\":%u,\"PSUB\":%u"), teleinfo_contract.phase, teleinfo_contract.isousc, teleinfo_contract.ssousc);

  // METER adjusted maximum power
  ResponseAppend_P (PSTR (",\"PMAX\":%d"), (long)teleinfo_config.percent * teleinfo_contract.ssousc / 100);

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

      // cos phi
      if (teleinfo_conso.cosphi.nb_measure > 1) ResponseAppend_P (PSTR (",\"C%u\":%d.%02d"), value, teleinfo_conso.phase[phase].cosphi / 1000, teleinfo_conso.phase[phase].cosphi % 1000 / 10);
    } 

    // conso : global values
    ResponseAppend_P (PSTR (",\"U\":%d,\"I\":%d.%02d,\"P\":%d,\"W\":%d"), voltage / (long)teleinfo_contract.phase, current / 1000, current % 1000 / 10, power_app, power_act);

    // conso : cosphi
    if (teleinfo_conso.cosphi.nb_measure > 1) ResponseAppend_P (PSTR (",\"C\":%d.%02d"), teleinfo_conso.cosphi.value / 1000, teleinfo_conso.cosphi.value % 1000 / 10);

    // conso : yesterday
    ResponseAppend_P (PSTR (",\"YDAY\":%d"), teleinfo_conso.yesterday_wh);

    // conso : today
    if (teleinfo_conso.today_wh > 0) ResponseAppend_P (PSTR (",\"2DAY\":%d"), teleinfo_conso.today_wh);
  }
  
  // production 
  if (teleinfo_prod.total_wh != 0)
  {
    // prod : global values
    ResponseAppend_P (PSTR (",\"PP\":%d,\"PW\":%d"), teleinfo_prod.papp, teleinfo_prod.pact);

    // prod : cosphi
    if (teleinfo_prod.cosphi.nb_measure > 1) ResponseAppend_P (PSTR (",\"PC\":%d.%02d"), teleinfo_prod.cosphi.value / 1000, teleinfo_prod.cosphi.value % 1000 / 10);

    // prod : yesterday
    ResponseAppend_P (PSTR (",\"PYDAY\":%d"), teleinfo_prod.yesterday_wh);

    // prod : today
    if (teleinfo_prod.today_wh > 0) ResponseAppend_P (PSTR (",\"P2DAY\":%d"), teleinfo_prod.today_wh);
  } 

  // end of message
  ResponseJsonEndEnd ();
  MqttPublishTeleSensor ();

  // reset JSON flag
  teleinfo_meter.json.meter = 0;

#ifdef USE_TELEINFO_HOMIE
  HomieIntegrationPublishMeter ();
#endif    // USE_TELEINFO_HOMIE

  // update data reception
  TeleinfoProcessRealTime ();
}

// Generate JSON with RELAY (virtual relay mapping)
void TeleinfoDriverPublishRelay ()
{
  uint8_t index;

  // start of message
  ResponseClear ();
  ResponseAppendTime ();
  ResponseAppend_P (PSTR (",\"RELAY\":{"));

  // loop to publish virtual relays
  for (index = 0; index < 8; index ++)
  {
    if (index > 0) ResponseAppend_P (PSTR (","));
    ResponseAppend_P (PSTR ("\"R%u\":%u"), index + 1, TeleinfoRelayStatus (index));
  }

  // end of message
  ResponseJsonEndEnd ();
  MqttPublishTeleSensor ();

  // reset JSON flag
  teleinfo_meter.json.relay = 0;

#ifdef USE_TELEINFO_HOMIE
  // ask to publish relay data with Homie protocol
  HomieIntegrationPublishRelay ();
#endif    // USE_TELEINFO_HOMIE

  // update data reception
  TeleinfoProcessRealTime ();
}

// Generate JSON with Contract and Totals (global counters in wh)
void TeleinfoDriverPublishContract ()
{
  uint8_t index;
  char    str_value[32];
  char    str_period[32];

  // start of message
  ResponseClear ();
  ResponseAppendTime ();
  ResponseAppend_P (PSTR (",\"CONTRACT\":{"));

  // contract name
  TeleinfoContractGetName (str_value, sizeof (str_value));
  ResponseAppend_P (PSTR ("\"name\":\"%s\""), str_value);

  // contract period
  TeleinfoPeriodGetName (str_value, sizeof (str_value));
  ResponseAppend_P (PSTR (",\"period\":\"%s\""), str_value);

  // contract color
  index = TeleinfoPeriodGetLevel ();
  GetTextIndexed (str_value, sizeof (str_value), index, kTeleinfoPeriodLevel);
  ResponseAppend_P (PSTR (",\"color\":\"%s\""), str_value);

  // contract hour type
  index = TeleinfoPeriodGetHP ();
  GetTextIndexed (str_value, sizeof (str_value), index, kTeleinfoPeriodHour);
  ResponseAppend_P (PSTR (",\"hour\":\"%s\""), str_value);

  // contract today
  index = TeleinfoDriverCalendarGetLevel (TIC_DAY_TODAY, 12, true);
  GetTextIndexed (str_value, sizeof (str_value), index, kTeleinfoPeriodLevel);
  ResponseAppend_P (PSTR (",\"today\":\"%s\""), str_value);

  // contract tomorrow
  index = TeleinfoDriverCalendarGetLevel (TIC_DAY_TOMORROW, 12, true);
  GetTextIndexed (str_value, sizeof (str_value), index, kTeleinfoPeriodLevel);
  ResponseAppend_P (PSTR (",\"tomorrow\":\"%s\""), str_value);

  // loop to publish conso counters
  for (index = 0; index < teleinfo_contract.period_qty; index ++)
  {
    if (teleinfo_conso.index_wh[index] > 0)
    {
      TeleinfoPeriodGetCode (index, str_period, sizeof (str_period));
      lltoa (teleinfo_conso.index_wh[index], str_value, 10);
      ResponseAppend_P (PSTR (",\"%s\":%s"), str_period, str_value);
    }
  }

  // ,"PROD":xxx
  // -------------
  if (teleinfo_prod.total_wh != 0)
  {
    lltoa (teleinfo_prod.total_wh, str_value, 10) ;
    ResponseAppend_P (PSTR (",\"PROD\":%s"), str_value);
  }

  // end of message
  ResponseJsonEndEnd ();
  MqttPublishTeleSensor ();

  // reset JSON flag
  teleinfo_meter.json.contract = 0;

#ifdef USE_TELEINFO_HOMIE
  HomieIntegrationPublishTotal ();
#endif    // USE_TELEINFO_HOMIE

  // update data reception
  TeleinfoProcessRealTime ();
}

// get hour slot hc / hp
uint8_t TeleinfoDriverCalendarGetHP (const uint8_t day, const uint8_t hour)
{
  uint8_t period, result;

  // check parameters
  if (day >= TIC_DAY_MAX) return 0;
  if (hour >= 24) return 0;
  
  // get level according to current period
  period = teleinfo_meter.arr_day[day].arr_period[hour];
  result = TeleinfoPeriodGetHP (period);

  return result;
}

// get hour slot level
uint8_t TeleinfoDriverCalendarGetLevel (const uint8_t day, const uint8_t hour, const bool use_rte)
{
  uint8_t period, level, rte_level;

  // check parameters
  level = TIC_LEVEL_NONE;
  if (day >= TIC_DAY_MAX) return level;
  if (hour >= 24) return level;
  
#ifdef USE_RTE_SERVER
  if (use_rte && RteTempoIsEnabled ()) level = RteTempoGetGlobalLevel (day, hour);
  else if (use_rte && RtePointeIsEnabled ()) level = RtePointeGetGlobalLevel (day, hour);
  else
  {
#endif      // USE_RTE_SERVER

    // get  level according to current period
    period = teleinfo_meter.arr_day[day].arr_period[hour];
    level  = TeleinfoPeriodGetLevel (period);

#ifdef USE_RTE_SERVER
  }

  // if needed, add ecowatt alert
  if (use_rte && RteEcowattIsEnabled ())
  {
    rte_level = RteEcowattGetGlobalLevel (day, hour);
    if (rte_level != TIC_LEVEL_BLUE) level = max (level, rte_level);
  }
#endif      // USE_RTE_SERVER

  return level;
}

// Generate JSON with CALENDAR
void TeleinfoDriverPublishCalendar ()
{
  uint8_t day, slot, level, peak;

  // start of message
  ResponseClear ();
  ResponseAppendTime ();
  ResponseAppend_P (PSTR (",\"CAL\":{"));

  // get and publish current data
  level = TeleinfoPeriodGetLevel ();
  peak  = TeleinfoPeriodGetHP ();
  ResponseAppend_P (PSTR ("\"lv\":%u,\"hp\":%u"), level, peak);

  // loop thru days
  for (day = TIC_DAY_TODAY; day <= TIC_DAY_TOMORROW; day ++)
  {
    // day header
    if (day == TIC_DAY_TODAY) ResponseAppend_P (PSTR (",\"today\":{"));
      else ResponseAppend_P (PSTR (",\"tomorrow\":{"));

    // day data
    for (slot = 0; slot < 24; slot ++)
    {
      // get data
      level = TeleinfoDriverCalendarGetLevel (day, slot, true);
      peak  = TeleinfoDriverCalendarGetHP    (day, slot);

      // add current slot
      if (slot > 0) ResponseAppend_P (PSTR (","));
      ResponseAppend_P (PSTR ("\"lv%u\":%u,\"hp%u\":%u"), slot, level, slot, peak);
    }
    ResponseJsonEnd ();
  }

  // publish message
  ResponseJsonEndEnd ();
  MqttPublishTeleSensor ();

  // reset JSON flag
  teleinfo_meter.json.calendar = 0;

#ifdef USE_TELEINFO_HOMIE
  HomieIntegrationPublishCalendar ();
#endif    // USE_TELEINFO_HOMIE

  // update data reception
  TeleinfoProcessRealTime ();
}

/***************************************\
 *              Callback
\***************************************/

// Teleinfo module initialisation
void TeleinfoModuleInit ()
{
  // disable wifi sleep mode
  TasmotaGlobal.wifi_stay_asleep = false;

  // boundaries of SVG graph
  teleinfo_graph.left  = TELEINFO_GRAPH_PERCENT_START * TELEINFO_GRAPH_WIDTH / 100;
  teleinfo_graph.right = TELEINFO_GRAPH_PERCENT_STOP  * TELEINFO_GRAPH_WIDTH / 100;
  teleinfo_graph.width = teleinfo_graph.right - teleinfo_graph.left;
}

// called 10 times per second
void TeleinfoDriverEvery100ms ()
{
  // if no valid time, ignore
  if (!RtcTime.valid) return;
  if (!TeleinfoDriverMeterReady ()) return;

  // check for last message publication
  if (teleinfo_meter.last_msg == 0) teleinfo_meter.last_msg = millis ();
  if (!TimeReached (teleinfo_meter.last_msg + TELEINFO_MESSAGE_DELAY)) return;

  // update next publication timestamp
  teleinfo_meter.last_msg = millis ();

  // alert
  if (teleinfo_meter.json.alert) TeleinfoDriverPublishAlert ();
  else if (teleinfo_meter.json.tic) TeleinfoDriverPublishTic ();
  else if (teleinfo_meter.json.relay) TeleinfoDriverPublishRelay ();
  else if (teleinfo_meter.json.calendar) TeleinfoDriverPublishCalendar ();
  else if (teleinfo_meter.json.contract) TeleinfoDriverPublishContract ();
  else if (teleinfo_meter.json.meter) TeleinfoDriverPublishMeter ();
}

// Handle MQTT teleperiod
void TeleinfoDriverTeleperiod ()
{
  // check if real teleperiod
  if (TasmotaGlobal.tele_period > 0) return;

  // trigger flags for full topic publication
  teleinfo_meter.json.alert = 1;
  TeleinfoDriverPublishTrigger ();
}

// Save data in case of planned restart
void TeleinfoDriverSaveBeforeRestart ()
{
#ifndef USE_WINKY 
  // save configuration
  TeleinfoSaveConfig ();

  // update energy counters
  EnergyUpdateToday ();
#endif    // USE_WINKY 
}

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

// Append Teleinfo graph button to main page
void TeleinfoDriverWebMainButton ()
{
  // Teleinfo message page button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>Messages</button></form></p>\n"), D_TELEINFO_PAGE_TIC);
}

// Append Teleinfo configuration button to configuration page
void TeleinfoDriverWebConfigButton ()
{
  // heater and meter configuration button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>Configure Teleinfo</button></form></p>\n"), D_TELEINFO_PAGE_CONFIG);
}

// Teleinfo web page
void TeleinfoDriverWebPageConfigure ()
{
  int  index;
  char str_text[32];
  char str_select[16];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess ()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg ("save"))
  {
    // parameter 'rate' : set teleinfo rate
    WebGetArg (D_CMND_TELEINFO_RATE, str_text, sizeof (str_text));
    if (strlen (str_text) > 0) SetSerialBaudrate ((uint32_t)atoi (str_text));

    // parameter 'policy' : set energy messages diffusion policy
    WebGetArg (D_CMND_TELEINFO_POLICY, str_text, sizeof (str_text));
    if (strlen (str_text) > 0) teleinfo_config.policy = atoi (str_text);

    // parameter 'meter' : set METER & PROD section diffusion flag
    WebGetArg (D_CMND_TELEINFO_METER, str_text, sizeof (str_text));
    if (strlen (str_text) > 0) teleinfo_config.meter = 1;
      else teleinfo_config.meter = 0;

    // parameter 'tic' : set TIC section diffusion flag
    WebGetArg (D_CMND_TELEINFO_TIC, str_text, sizeof (str_text));
    if (strlen (str_text) > 0) teleinfo_config.tic = 1;
      else teleinfo_config.tic = 0;

    // parameter 'calendar' : set CALENDAR section diffusion flag
    WebGetArg (D_CMND_TELEINFO_CALENDAR, str_text, sizeof (str_text));
    if (strlen (str_text) > 0) teleinfo_config.calendar = 1;
      else teleinfo_config.calendar = 0;

    // parameter 'relay' : set RELAY section diffusion flag
    WebGetArg (D_CMND_TELEINFO_RELAY, str_text, sizeof (str_text));
    if (strlen (str_text) > 0) teleinfo_config.relay = 1;
      else teleinfo_config.relay = 0;

    // parameter 'contract' : set CONTRACT section diffusion flag
    WebGetArg (D_CMND_TELEINFO_CONTRACT, str_text, sizeof (str_text));
    if (strlen (str_text) > 0) teleinfo_config.contract = 1;
      else teleinfo_config.contract = 0;

    // save configuration
    TeleinfoSaveConfig ();
  }

  // beginning of form
  WSContentStart_P (PSTR ("Configure Teleinfo"));
  WSContentSendStyle ();
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_TELEINFO_PAGE_CONFIG);

  // speed selection form
  WSContentSend_P (PSTR ("<p><fieldset><legend><b>&nbsp;%s %s&nbsp;</b></legend>\n"), "📨", PSTR ("Teleinfo"));
  if ((TasmotaGlobal.baudrate != 1200) && (TasmotaGlobal.baudrate != 9600)) strcpy_P (str_select, PSTR ("checked")); else strcpy (str_select, "");
  WSContentSend_P (PSTR ("<p><input type='radio' name='%s' value='%d' %s>%s</p>\n"), D_CMND_TELEINFO_RATE, 115200, str_select, "Désactivé");
  if (TasmotaGlobal.baudrate == 1200) strcpy_P (str_select, PSTR ("checked")); else strcpy (str_select, "");
  WSContentSend_P (PSTR ("<p><input type='radio' name='%s' value='%d' %s>%s</p>\n"), D_CMND_TELEINFO_RATE, 1200, str_select, "Historique (1200 bauds)");
  if (TasmotaGlobal.baudrate == 9600) strcpy_P (str_select, PSTR ("checked")); else strcpy (str_select, "");
  WSContentSend_P (PSTR ("<p><input type='radio' name='%s' value='%d' %s>%s</p>\n"), D_CMND_TELEINFO_RATE, 9600, str_select, "Standard (9600 bauds)");
  WSContentSend_P (PSTR ("<p style='text-align:center;font-style:italic;font-size:90%;'>Redémarrage requis</p>\n"));
  WSContentSend_P (PSTR ("</fieldset></p>\n"));

  // teleinfo message diffusion policy
  WSContentSend_P (PSTR ("<p><fieldset><legend><b>&nbsp;%s %s&nbsp;</b></legend>\n"), "〽️", PSTR ("Fréquence de publication"));
  for (index = 0; index < TELEINFO_POLICY_MAX; index++)
  {
    if (index == teleinfo_config.policy) strcpy_P (str_select, PSTR ("checked")); else strcpy (str_select, "");
    GetTextIndexed (str_text, sizeof (str_text), index, kTeleinfoMessagePolicy);
    WSContentSend_P (PSTR ("<p><input type='radio' name='%s' value='%d' %s>%s</p>\n"), D_CMND_TELEINFO_POLICY, index, str_select, str_text);
  }
  WSContentSend_P (PSTR ("</fieldset></p>\n"));

  // teleinfo message diffusion type
  WSContentSend_P (PSTR ("<p><fieldset><legend><b>&nbsp;%s %s&nbsp;</b></legend>\n"), "📊", PSTR ("Données publiées"));
  if (teleinfo_config.meter) strcpy_P (str_select, PSTR ("checked")); else strcpy (str_select, "");
  WSContentSend_P (PSTR ("<p><input type='checkbox' id='%s' name='%s' %s><label for='%s'>Consommation & Production</label></p>\n"), D_CMND_TELEINFO_METER, D_CMND_TELEINFO_METER, str_select, D_CMND_TELEINFO_METER);
  if (teleinfo_config.contract) strcpy_P (str_select, PSTR ("checked")); else strcpy (str_select, "");
  WSContentSend_P (PSTR ("<p><input type='checkbox' id='%s' name='%s' %s><label for='%s'>Contrat & Compteurs</label></p>\n"), D_CMND_TELEINFO_CONTRACT, D_CMND_TELEINFO_CONTRACT, str_select, D_CMND_TELEINFO_CONTRACT);
  if (teleinfo_config.calendar) strcpy_P (str_select, PSTR ("checked")); else strcpy (str_select, "");
  WSContentSend_P (PSTR ("<p><input type='checkbox' id='%s' name='%s' %s><label for='%s'>Calendrier</label></p>\n"), D_CMND_TELEINFO_CALENDAR, D_CMND_TELEINFO_CALENDAR, str_select, D_CMND_TELEINFO_CALENDAR);
  if (teleinfo_config.relay) strcpy_P (str_select, PSTR ("checked")); else strcpy (str_select, "");
  WSContentSend_P (PSTR ("<p><input type='checkbox' id='%s' name='%s' %s><label for='%s'>Relais virtuels</label></p>\n"), D_CMND_TELEINFO_RELAY, D_CMND_TELEINFO_RELAY, str_select, D_CMND_TELEINFO_RELAY);
  if (teleinfo_config.tic) strcpy_P (str_select, PSTR ("checked")); else strcpy (str_select, "");
  WSContentSend_P (PSTR ("<p><input type='checkbox' id='%s' name='%s' %s><label for='%s'>Données Teleinfo brutes</label></p>\n"), D_CMND_TELEINFO_TIC, D_CMND_TELEINFO_TIC, str_select, D_CMND_TELEINFO_TIC);
  WSContentSend_P (PSTR ("</fieldset></p>\n"));

  // save button
  WSContentSend_P (PSTR ("<br><button name='save' type='submit' class='button bgrn'>%s</button>"), D_SAVE);
  WSContentSend_P (PSTR ("</form>\n"));

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

// Append Teleinfo state to main page
void TeleinfoDriverWebSensor ()
{
  bool      display_mode = true;
  uint8_t   index, phase, slot, level, hp;
  long      value, red, green;
  long long total;
  char      str_color[24];
  char      str_text[64];

  // set reception status
  if (teleinfo_meter.serial == TIC_SERIAL_ACTIVE)
  {
    TeleinfoContractGetName (str_text, sizeof (str_text));
    GetTextIndexed (str_color, sizeof (str_color), teleinfo_contract.mode, kTeleinfoModeIcon);
  }
  else
  {
    GetTextIndexed (str_text, sizeof (str_text), teleinfo_meter.serial, kTeleinfoSerialStatus);
    strcpy (str_color, "");
  }

  //   Start
  // ----------
  WSContentSend_P (PSTR ("<div style='font-size:13px;text-align:center;padding:4px 6px;margin-bottom:5px;background:#333333;border-radius:12px;'>\n"));
  WSContentSend_P (PSTR ("<div style='display:flex;margin:2px 0px 6px 0px;padding:0px;font-size:16px;'>\n"));

  // if reception has not started
  if (!TeleinfoSerialIsActive ())
  {
    WSContentSend_P (PSTR ("<div style='width:100%%;padding:0px;text-align:left;font-weight:bold;'>%s</div>\n"), str_text);
    WSContentSend_P (PSTR ("</div>\n"));
  }

  // else receiving teleinfo data
  else
  {
    // header
    WSContentSend_P (PSTR ("<div style='width:16%%;padding:0px;'>%s</div>\n"), str_color);
    WSContentSend_P (PSTR ("<div style='width:44%%;padding:0px;text-align:left;font-weight:bold;'>%s</div>\n"), str_text);
    if (teleinfo_contract.phase > 1) sprintf (str_text, "%ux", teleinfo_contract.phase); else strcpy (str_text, ""); 
    WSContentSend_P (PSTR ("<div style='width:28%%;padding:0px;text-align:right;font-weight:bold;'>%s%d</div>\n"), str_text, teleinfo_contract.ssousc / 1000);
    if (teleinfo_contract.unit == TIC_UNIT_KVA) strcpy (str_text, "kVA"); 
      else if (teleinfo_contract.unit == TIC_UNIT_KW) strcpy (str_text, "kW");
      else strcpy (str_text, "");
    WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div><div style='width:10%%;padding:0px;text-align:left;'>%s</div>\n"), str_text);
    WSContentSend_P (PSTR ("</div>\n"));

    // update data reception
    TeleinfoProcessRealTime ();

    // contract
    for (index = 0; index < teleinfo_contract.period_qty; index++)
      if (teleinfo_conso.index_wh[index] > 0)
      {
        WSContentSend_P (PSTR ("<div style='display:flex;padding:2px 0px;'>\n"));

        // period name
        WSContentSend_P (PSTR ("<div style='width:16%%;padding:0px;'></div>\n"));
        TeleinfoPeriodGetName (index, str_text, sizeof (str_text));
        if (teleinfo_contract.period == index) WSContentSend_P (PSTR ("<div style='width:31%%;padding:1px 0px;font-size:12px;background-color:#09f;border-radius:6px;'>%s</div>\n"), str_text);
          else WSContentSend_P (PSTR ("<div style='width:31%%;padding:0px;font-size:12px;'>%s</div>\n"), str_text);

        // counter value
        lltoa (teleinfo_conso.index_wh[index] / 1000, str_text, 10);
        value = (long)(teleinfo_conso.index_wh[index] % 1000);
        WSContentSend_P (PSTR ("<div style='width:41%%;padding:0px;text-align:right;'>%s.%03d</div>\n"), str_text, value);
        WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div><div style='width:10%%;padding:0px;text-align:left;'>kWh</div>\n"));
        WSContentSend_P (PSTR ("</div>\n"));
      }

    // update data reception
    TeleinfoProcessRealTime ();

    // production total
    if (teleinfo_prod.total_wh != 0)
    {
      WSContentSend_P (PSTR ("<div style='display:flex;padding:2px 0px;'>\n"));

      // period name
      WSContentSend_P (PSTR ("<div style='width:16%%;padding:0px;'></div>\n"));
      if (teleinfo_prod.papp == 0) WSContentSend_P (PSTR ("<div style='width:31%%;padding:0px;'>Production</div>\n"));
        else WSContentSend_P (PSTR ("<div style='width:31%%;padding:0px;background-color:#080;border-radius:6px;'>Production</div>\n"));

      // counter value
      lltoa (teleinfo_prod.total_wh / 1000, str_text, 10);
      value = (long)(teleinfo_prod.total_wh % 1000);
      WSContentSend_P (PSTR ("<div style='width:41%%;padding:0px;text-align:right;'>%s.%03d</div>\n"), str_text, value);
      WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div><div style='width:10%%;padding:0px;text-align:left;'>kWh</div>\n"));
      WSContentSend_P (PSTR ("</div>\n"));

      // update data reception
      TeleinfoProcessRealTime ();
    }

    //   consommation
    // ----------------

    if (teleinfo_conso.total_wh > 0)
    {
      // consumption : separator and header
      WSContentSend_P (PSTR ("<hr>\n"));
      WSContentSend_P (PSTR ("<div style='display:flex;padding:2px 0px;'>\n"));
      WSContentSend_P (PSTR ("<div style='width:48%%;padding:0px;text-align:left;font-weight:bold;'>Consommation</div>\n"));

      // over voltage
      if (teleinfo_meter.flag.overvolt != 0) { strcpy (str_color, "background:#900;"); strcpy (str_text, "V"); }
        else { strcpy (str_color, ""); strcpy (str_text, ""); }
      WSContentSend_P (PSTR ("<div style='width:10%%;padding:2px 0px;border-radius:12px;font-size:12px;%s'>%s</div>\n"), str_color, str_text);
      WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div>\n"));

      // over load
      if (teleinfo_meter.flag.overvolt != 0) { strcpy (str_color, "background:#900;"); strcpy (str_text, "VA"); }
        else { strcpy (str_color, ""); strcpy (str_text, ""); }
      WSContentSend_P (PSTR ("<div style='width:12%%;padding:2px 0px;border-radius:12px;font-size:12px;%s'>%s</div>\n"), str_color, str_text);
      WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div>\n"));

      // preavis
      if (teleinfo_meter.preavis.level == TIC_PREAVIS_NONE) { strcpy (str_color, ""); strcpy (str_text, ""); }
        else if (teleinfo_meter.preavis.level == TIC_PREAVIS_WARNING) { strcpy (str_color, "background:#d70;"); strcpy (str_text, teleinfo_meter.preavis.str_label); }
        else { strcpy (str_color, "background:#900;"); strcpy (str_text, teleinfo_meter.preavis.str_label); }
      WSContentSend_P (PSTR ("<div style='width:14%%;padding:2px 0px;border-radius:12px;font-size:12px;%s'>%s</div>\n"), str_color, str_text);

      WSContentSend_P (PSTR ("<div style='width:12%%;padding:0px;'></div>\n"));
      WSContentSend_P (PSTR ("</div>\n"));

      // update data reception
      TeleinfoProcessRealTime ();

      // consumption : bar graph per phase
      if (teleinfo_contract.ssousc > 0)
        for (phase = 0; phase < teleinfo_contract.phase; phase++)
        {
          WSContentSend_P (PSTR ("<div style='display:flex;margin:2px 0px;padding:1px;height:16px;opacity:75%%;'>\n"));

          // display voltage
          if (teleinfo_meter.flag.overvolt == 1) strcpy (str_text, "font-weight:bold;color:red;"); else strcpy (str_text, "");
          WSContentSend_P (PSTR ("<div style='width:16%%;padding:0px;text-align:left;%s'>%d V</div>\n"), str_text, teleinfo_conso.phase[phase].voltage);

          // calculate percentage and value
          value = 100 * teleinfo_conso.phase[phase].papp / teleinfo_contract.ssousc;
          if (value > 100) value = 100;
          if (teleinfo_conso.phase[phase].papp > 0) ltoa (teleinfo_conso.phase[phase].papp, str_text, 10); 
            else strcpy (str_text, "");

          // calculate color
          if (value < 50) green = TELEINFO_RGB_GREEN_MAX; else green = TELEINFO_RGB_GREEN_MAX * 2 * (100 - value) / 100;
          if (value > 50) red = TELEINFO_RGB_RED_MAX; else red = TELEINFO_RGB_RED_MAX * 2 * value / 100;
          sprintf (str_color, "#%02x%02x00", (ulong)red, (ulong)green);

          // display bar graph percentage
          WSContentSend_P (PSTR ("<div style='width:72%%;padding:0px;text-align:left;background-color:%s;border-radius:6px;'><div style='width:%d%%;height:16px;padding:0px;text-align:center;font-size:12px;background-color:%s;border-radius:6px;'>%s</div></div>\n"), COLOR_BACKGROUND, value, str_color, str_text);
          WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div><div style='width:10%%;padding:0px;text-align:left;'>VA</div>\n"));
          WSContentSend_P (PSTR ("</div>\n"));
        }

      // update data reception
      TeleinfoProcessRealTime ();

      // consumption : active power
      WSContentSend_P (PSTR ("<div style='display:flex;padding:2px 0px;'>\n"));
      WSContentSend_P (PSTR ("<div style='width:16%%;padding:0px;'></div>\n"));
      WSContentSend_P (PSTR ("<div style='width:47%%;padding:0px;text-align:left;'>cosφ <b>%d.%02d</b></div>\n"), teleinfo_conso.cosphi.value / 1000, teleinfo_conso.cosphi.value % 1000 / 10);
      value = 0; 
      for (phase = 0; phase < teleinfo_contract.phase; phase++) value += teleinfo_conso.phase[phase].pact;
      WSContentSend_P (PSTR ("<div style='width:25%%;padding:0px;text-align:right;font-weight:bold;'>%d</div>\n"), value);
      WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div><div style='width:10%%;padding:0px;text-align:left;'>W</div>\n"));
      WSContentSend_P (PSTR ("</div>\n"));

      // consumption : today's total
      if (teleinfo_conso.today_wh > 0)
      {
        WSContentSend_P (PSTR ("<div style='display:flex;padding:2px 0px;'>\n"));
        WSContentSend_P (PSTR ("<div style='width:16%%;padding:0px;'></div>\n"));
        WSContentSend_P (PSTR ("<div style='width:37%%;padding:0px;text-align:left;'>Aujourd'hui</div>\n"));
        WSContentSend_P (PSTR ("<div style='width:35%%;padding:0px;text-align:right;font-weight:bold;'>%d</div>\n"), teleinfo_conso.today_wh);
        WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div><div style='width:10%%;padding:0px;text-align:left;'>Wh</div>\n"));
        WSContentSend_P (PSTR ("</div>\n"));
      }

      // consumption : yesterday's total
      if (teleinfo_conso.yesterday_wh > 0)
      {
        WSContentSend_P (PSTR ("<div style='display:flex;padding:2px 0px;'>\n"));
        WSContentSend_P (PSTR ("<div style='width:16%%;padding:0px;'></div>\n"));
        WSContentSend_P (PSTR ("<div style='width:37%%;padding:0px;text-align:left;'>Hier</div>\n"));
        WSContentSend_P (PSTR ("<div style='width:35%%;padding:0px;text-align:right;font-weight:bold;'>%d</div>\n"), teleinfo_conso.yesterday_wh);
        WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div><div style='width:10%%;padding:0px;text-align:left;'>Wh</div>\n"));
        WSContentSend_P (PSTR ("</div>\n"));
      }

      // update data reception
      TeleinfoProcessRealTime ();
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
            else strcpy (str_text, "");

        // calculate color
        if (value < 50) red = TELEINFO_RGB_GREEN_MAX; else red = TELEINFO_RGB_GREEN_MAX * 2 * (100 - value) / 100;
        if (value > 50) green = TELEINFO_RGB_RED_MAX; else green = TELEINFO_RGB_RED_MAX * 2 * value / 100;
        sprintf (str_color, "#%02x%02x%02x", red, green, 0);

        // display bar graph percentage
        WSContentSend_P (PSTR ("<div style='display:flex;margin:2px 0px;padding:1px;height:16px;opacity:75%%;'>\n"));
        WSContentSend_P (PSTR ("<div style='width:16%%;padding:0px;text-align:left;'></div>\n"));
        WSContentSend_P (PSTR ("<div style='width:72%%;padding:0px;text-align:left;background-color:%s;border-radius:6px;'><div style='width:%d%%;height:16px;padding:0px;text-align:center;font-size:12px;background-color:%s;border-radius:6px;'>%s</div></div>\n"), COLOR_BACKGROUND, value, str_color, str_text);
        WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div><div style='width:10%%;padding:0px;text-align:left;'>VA</div>\n"));
        WSContentSend_P (PSTR ("</div>\n"));
      }

      // update data reception
      TeleinfoProcessRealTime ();

      // production : active power
      WSContentSend_P (PSTR ("<div style='display:flex;padding:2px 0px;'>\n"));
      WSContentSend_P (PSTR ("<div style='width:16%%;padding:0px;'></div>\n"));
      WSContentSend_P (PSTR ("<div style='width:47%%;padding:0px;text-align:left;'>cosφ <b>%d.%02d</b></div>\n"), teleinfo_prod.cosphi.value / 1000, teleinfo_prod.cosphi.value % 1000 / 10);
      WSContentSend_P (PSTR ("<div style='width:25%%;padding:0px;text-align:right;font-weight:bold;'>%d</div>\n"), teleinfo_prod.pact);
      WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div><div style='width:10%%;padding:0px;text-align:left;'>W</div>\n"));
      WSContentSend_P (PSTR ("</div>\n"));

      // production : today's total
      if (teleinfo_prod.today_wh > 0)
      {
        WSContentSend_P (PSTR ("<div style='display:flex;padding:2px 0px;'>\n"));
        WSContentSend_P (PSTR ("<div style='width:16%%;padding:0px;'></div>\n"));
        WSContentSend_P (PSTR ("<div style='width:37%%;padding:0px;text-align:left;'>Aujourd'hui</div>\n"));
        WSContentSend_P (PSTR ("<div style='width:35%%;padding:0px;text-align:right;font-weight:bold;'>%d</div>\n"), teleinfo_prod.today_wh);
        WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div><div style='width:10%%;padding:0px;text-align:left;'>Wh</div>\n"));
        WSContentSend_P (PSTR ("</div>\n"));
      }

      // production : yesterday's total
      if (teleinfo_prod.yesterday_wh > 0)
      {
        WSContentSend_P (PSTR ("<div style='display:flex;padding:2px 0px;'>\n"));
        WSContentSend_P (PSTR ("<div style='width:16%%;padding:0px;'></div>\n"));
        WSContentSend_P (PSTR ("<div style='width:37%%;padding:0px;text-align:left;'>Hier</div>\n"));
        WSContentSend_P (PSTR ("<div style='width:35%%;padding:0px;text-align:right;font-weight:bold;'>%d</div>\n"), teleinfo_prod.yesterday_wh);
        WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div><div style='width:10%%;padding:0px;text-align:left;'>Wh</div>\n"));
        WSContentSend_P (PSTR ("</div>\n"));
      }

      // update data reception
      TeleinfoProcessRealTime ();
    }

    //   calendar
    // ------------

    if (teleinfo_config.calendar)
    {
      // separator and header
      WSContentSend_P (PSTR ("<hr>\n"));

      // hour scale
      WSContentSend_P (PSTR ("<div style='display:flex;margin:0px;padding:0px;font-size:10px;'>\n"));
      WSContentSend_P (PSTR ("<div style='width:20.8%%;padding:0px;text-align:left;font-weight:bold;font-size:12px;'>Période</div>\n"), 0);
      WSContentSend_P (PSTR ("<div style='width:13.2%%;padding:0px;text-align:left;'>%uh</div>\n"), 0);
      WSContentSend_P (PSTR ("<div style='width:13.2%%;padding:0px;'>%uh</div>\n"), 6);
      WSContentSend_P (PSTR ("<div style='width:26.4%%;padding:0px;'>%uh</div>\n"), 12);
      WSContentSend_P (PSTR ("<div style='width:13.2%%;padding:0px;'>%uh</div>\n"), 18);
      WSContentSend_P (PSTR ("<div style='width:13.2%%;padding:0px;text-align:right;'>%uh</div>\n"), 24);
      WSContentSend_P (PSTR ("</div>\n"));

      // loop thru days
      for (index = TIC_DAY_TODAY; index <= TIC_DAY_TOMORROW; index ++)
      {
        WSContentSend_P (PSTR ("<div style='display:flex;margin:2px 0px;padding:1px;'>\n"));

        // display day
        if (index == TIC_DAY_TODAY) strcpy (str_text, "Aujourd'hui"); else strcpy (str_text, "Demain");
        WSContentSend_P (PSTR ("<div style='width:20.8%%;padding:0px;font-size:10px;text-align:left;'>%s</div>\n"), str_text);

        // display hourly slots
        for (slot = 0; slot < 24; slot ++)
        {
          // slot display beginning
          level  = TeleinfoDriverCalendarGetLevel (index, slot, false);
          GetTextIndexed (str_color, sizeof (str_color), level, kTeleinfoCalendarColor);  
          WSContentSend_P (PSTR ("<div style='width:3.3%%;padding:1px 0px;background-color:%s;"), str_color);

          // set specific opacity for HC
          hp = TeleinfoDriverCalendarGetHP (index, slot);
          if (hp == 0) WSContentSend_P (PSTR ("opacity:75%%;"));

          // first and last segment specificities
          if (slot == 0) WSContentSend_P (PSTR ("border-top-left-radius:6px;border-bottom-left-radius:6px;"));
            else if (slot == 23) WSContentSend_P (PSTR ("border-top-right-radius:6px;border-bottom-right-radius:6px;"));

          // segment end
          if ((index == TIC_DAY_TODAY) && (slot == RtcTime.hour))
          {
            GetTextIndexed (str_text, sizeof (str_text), level, kTeleinfoCalendarDot);  
            WSContentSend_P (PSTR ("font-size:9px;'>%s</div>\n"), str_text);
          }
          else WSContentSend_P (PSTR ("'></div>\n"));
        }
        WSContentSend_P (PSTR ("</div>\n"));
      }

      // update data reception
      TeleinfoProcessRealTime ();
    }

    //   relays
    // ------------

    if (teleinfo_config.relay)
    {
      // separator
      WSContentSend_P (PSTR ("<hr>\n"));

      // relay state
      WSContentSend_P (PSTR ("<div style='display:flex;padding:0px;font-size:16px;'>\n"));
      WSContentSend_P (PSTR ("<div style='width:20%%;padding:0px;text-align:left;font-weight:bold;font-size:12px;'>Relais</div>\n"));
      for (index = 0; index < 8; index ++) 
      {
        if (TeleinfoRelayStatus (index) == 1) strcpy (str_text, "🟢"); else strcpy (str_text, "🔴");
        WSContentSend_P (PSTR ("<div style='width:10%%;padding:0px;'>%s</div>\n"), str_text);
      }
      WSContentSend_P (PSTR ("</div>\n"));

      // relay number
      WSContentSend_P (PSTR ("<div style='display:flex;padding:0px;font-size:10px;margin:-17px 0px 10px 0px;'>\n"));
      WSContentSend_P (PSTR ("<div style='width:20%%;padding:0px;'></div>\n"));
      for (index = 0; index < 8; index ++) WSContentSend_P (PSTR ("<div style='width:10%%;padding:0px;'>%u</div>\n"), index + 1);
      WSContentSend_P (PSTR ("</div>\n"));

      // update data reception
      TeleinfoProcessRealTime ();
    }

    //   counters
    // ------------

    // separator
    WSContentSend_P (PSTR ("<hr>\n"));

    // counters
    WSContentSend_P (PSTR ("<div style='display:flex;padding:2px 0px;'>\n"));
    WSContentSend_P (PSTR ("<div style='width:50%%;padding:0px;'>%d trames</div>\n"), teleinfo_meter.nb_message);
    WSContentSend_P (PSTR ("<div style='width:50%%;padding:0px;'>%d cosφ</div>\n"), teleinfo_conso.cosphi.nb_measure + teleinfo_prod.cosphi.nb_measure);
    WSContentSend_P (PSTR ("</div>\n"));

    // update data reception
    TeleinfoProcessRealTime ();

    // if reset or more than 1% errors, display counters
    if (teleinfo_meter.nb_line > 0) value = (long)(teleinfo_meter.nb_error * 10000 / teleinfo_meter.nb_line);
      else value = 0;
    if (teleinfo_config.display || (teleinfo_meter.nb_reset > 0) || (value >= 100))
    {
      WSContentSend_P (PSTR ("<div style='display:flex;padding:2px 0px;'>\n"));
      if (teleinfo_meter.nb_error == 0) strcpy (str_text, "white"); else strcpy (str_text, "red");
      WSContentSend_P (PSTR ("<div style='width:50%%;padding:0px;color:%s;'>%d erreurs (%d.%02d%%)</div>\n"), str_text, (long)teleinfo_meter.nb_error, value / 100, value % 100);
      if (teleinfo_meter.nb_reset == 0) strcpy (str_text, "white"); else strcpy (str_text, "orange");
      WSContentSend_P (PSTR ("<div style='width:50%%;padding:0px;color:%s;'>%d reset</div>\n"), str_text, teleinfo_meter.nb_reset);
      WSContentSend_P (PSTR ("</div>\n"));
    }
  }

  // end
  WSContentSend_P (PSTR ("</div>\n"));

  // update data reception
  TeleinfoProcessRealTime ();
}

// TIC raw message data
void TeleinfoDriverWebTicUpdate ()
{
  int      index;
  uint32_t timestart;
  char     checksum;
  char     str_class[4];
  char     str_line[TELEINFO_LINE_MAX];

  // start timestamp
  timestart = millis ();

  // start of data page
  WSContentBegin (200, CT_PLAIN);

  // send line number
  sprintf (str_line, "%d\n", teleinfo_meter.nb_message);
  WSContentSend_P (PSTR ("%s"), str_line); 


  // loop thru TIC message lines to publish if defined
  for (index = 0; index < teleinfo_message.line_last; index ++)
  {
//    if (teleinfo_message.arr_last[index].checksum != 0)
    checksum = teleinfo_message.arr_last[index].checksum;
    if (checksum == 0) strcpy (str_class, "ko"); else strcpy (str_class, "ok");
    if (checksum == 0) checksum = 0x20;
      sprintf (str_line, "%s|%s|%s|%c\n", str_class, teleinfo_message.arr_last[index].str_etiquette.c_str (), teleinfo_message.arr_last[index].str_donnee.c_str (), checksum);
//    else strcpy (str_line, " | | \n");
    WSContentSend_P (PSTR ("%s"), str_line); 
  }

  // end of data page
  WSContentEnd ();

  // log data serving time
  AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: Update Message in %ums"), millis () - timestart);
}

// TIC message page
void TeleinfoDriverWebPageTic ()
{
  int index;

  // beginning of form without authentification
  WSContentStart_P (D_TELEINFO_MESSAGE, false);
  WSContentSend_P (PSTR ("</script>\n\n"));

  // page data refresh script
  WSContentSend_P (PSTR ("<script type='text/javascript'>\n\n"));

  WSContentSend_P (PSTR ("function updateData(){\n"));

  WSContentSend_P (PSTR (" httpData=new XMLHttpRequest();\n"));
  WSContentSend_P (PSTR (" httpData.open('GET','%s',true);\n"), D_TELEINFO_PAGE_TIC_UPD);

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
  WSContentSend_P (PSTR ("<div><img src='%s'><div class='count'><span id='msg'>%u</span></div></div>\n"), D_TELEINFO_ICON_LINKY_PNG, teleinfo_meter.nb_message);

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
bool Xdrv115 (uint32_t function)
{
  bool result = false;

  // swtich according to contextdomoticz
  switch (function)
  {
    case FUNC_MODULE_INIT:
      TeleinfoModuleInit ();
      break;
    case FUNC_SAVE_BEFORE_RESTART:
      TeleinfoDriverSaveBeforeRestart ();
      break;
   case FUNC_EVERY_100_MSECOND:
      TeleinfoDriverEvery100ms ();
      break;
    case FUNC_JSON_APPEND:
      TeleinfoDriverTeleperiod ();
      break;      
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
      Webserver->on (FPSTR (D_TELEINFO_PAGE_CONFIG),    TeleinfoDriverWebPageConfigure);
      Webserver->on (FPSTR (D_TELEINFO_ICON_LINKY_PNG), TeleinfoDriverLinkyIcon);
      Webserver->on (FPSTR (D_TELEINFO_PAGE_TIC),       TeleinfoDriverWebPageTic);
      Webserver->on (FPSTR (D_TELEINFO_PAGE_TIC_UPD),   TeleinfoDriverWebTicUpdate);
    break;
#endif    // USE_WEBSERVER
  }

  return result;
}

#endif      // USE_TELEINFO
#endif      // USE_ENERGY_SENSOR
