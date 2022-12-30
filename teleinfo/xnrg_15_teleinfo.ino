/*
  xnrg_15_teleinfo.ino - France Teleinfo energy sensor support for Sonoff-Tasmota

  Copyright (C) 2019-2022  Nicolas Bernaerts

  Connexions :
    * On ESP8266, Teleinfo Rx must be connected to GPIO3 (as it must be forced to 7E1)
    * On ESP32, Teleinfo Rx must NOT be connected to GPIO3 (to avoid nasty ESP32 ESP_Restart bug where ESP32 hangs if restarted when Rx is under heavy load)
    * Teleinfo EN may (or may not) be connected to any GPIO starting from GPIO5 ...
   
  If LittleFS partition is available, config is stored in /teleinfo.cfg
  If no partition is available, settings are stored using Settings->weight parameters :
    - Settings->weight_reference   = Message publication policy
    - Settings->weight_calibration = Type of message publication policy
    - Settings->weight_item        = Energy update to SRAM interval (in mn)
    - Settings->weight_max         = Energy minimum power delta to do calculation (in VA/W)
    - Settings->weight_change      = Energy contract percentage adjustment for maximum acceptable power (in %)

  Version history :
    05/05/2019 - v1.0   - Creation
    16/05/2019 - v1.1   - Add Tempo and EJP contracts
    08/06/2019 - v1.2   - Handle active and apparent power
    05/07/2019 - v2.0   - Rework with selection thru web interface
    02/01/2020 - v3.0   - Functions rewrite for Tasmota 8.x compatibility
    05/02/2020 - v3.1   - Add support for 3 phases meters
    14/03/2020 - v3.2   - Add apparent power graph
    05/04/2020 - v3.3   - Add Timezone management
    13/05/2020 - v3.4   - Add overload management per phase
    19/05/2020 - v3.6   - Add configuration for first NTP server
    26/05/2020 - v3.7   - Add Information JSON page
    07/07/2020 - v3.7.1 - Enable discovery (mDNS)
    29/07/2020 - v3.8   - Add Meter section to JSON
    05/08/2020 - v4.0   - Major code rewrite, JSON section is now TIC, numbered like new official Teleinfo module
    24/08/2020 - v4.0.1 - Web sensor display update
    18/09/2020 - v4.1   - Based on Tasmota 8.4
    07/10/2020 - v5.0   - Handle different graph periods and javascript auto update
    18/10/2020 - v5.1   - Expose icon on web server
    25/10/2020 - v5.2   - Real time graph page update
    30/10/2020 - v5.3   - Add TIC message page
    02/11/2020 - v5.4   - Tasmota 9.0 compatibility
    09/11/2020 - v6.0   - Handle ESP32 ethernet devices with board selection
    11/11/2020 - v6.1   - Add data.json page
    20/11/2020 - v6.2   - Checksum bug
    29/12/2020 - v6.3   - Strengthen message error control
    25/02/2021 - v7.0   - Prepare compatibility with TIC standard
    01/03/2021 - v7.0.1 - Add power status bar
    05/03/2021 - v7.1   - Correct bug on hardware energy counter
    08/03/2021 - v7.2   - Handle voltage and checksum for horodatage
    12/03/2021 - v7.3   - Use average / overload for graph
    15/03/2021 - v7.4   - Change graph period parameter
    21/03/2021 - v7.5   - Support for TIC Standard
    29/03/2021 - v7.6   - Add voltage graph
    04/04/2021 - v7.7   - Change in serial port & graph height selection
    06/04/2021 - v7.7.1 - Handle number of indexes according to contract
    10/04/2021 - v7.7.2 - Remove use of String to avoid heap fragmentation 
    14/04/2021 - v7.8   - Calculate Cos phi and Active power (W)
    21/04/2021 - v8.0   - Fixed IP configuration and change in Cos phi calculation
    29/04/2021 - v8.1   - Bug fix in serial port management and realtime energy totals
    16/05/2021 - v8.1.1 - Control initial baud rate to avoid crash (thanks to Seb)
    26/05/2021 - v8.2   - Add active power (W) graph
    22/06/2021 - v8.3   - Change in serial management for ESP32
    04/08/2021 - v9.0   - Tasmota 9.5 compatibility
                          Add LittleFS historic data record
                          Complete change in VA, W and cos phi measurement based on transmission time
                          Add PME/PMI ACE6000 management
                          Add energy update interval configuration
                          Add TIC to TCP bridge (command 'tcpstart 8888' to publish teleinfo stream on port 8888)
    04/09/2021 - v9.1   - Save settings in LittleFS partition if available
                          Log rotate and old files deletion if space low
    10/10/2021 - v9.2   - Add peak VA and V in history files
    02/11/2021 - v9.3   - Add period and totals in history files
                          Add simple FTP server to access history files
    13/03/2022 - v9.4   - Change keys to ISUB and PSUB in METER section
    20/03/2022 - v9.5   - Change serial init and major rework in active power calculation
    01/04/2022 - v9.6   - Add software watchdog feed to avoid lock
    22/04/2022 - v9.7   - Option to minimise LittleFS writes (day:every 1h and week:every 6h)
    09/06/2022 - v9.7.1 - Correction of EAIT bug
    04/08/2022 - v9.8   - Based on Tasmota 12 , add ESP32S2 support
                          Remove FTP server auto start
    18/08/2022 - v9.9   - Force GPIO_TELEINFO_RX as digital input
    31/08/2022 - v9.9.1 - Bug littlefs config and graph data recording
    01/09/2022 - v9.9.2 - Add Tempo and Production mode (thanks to Sébastien)
    08/09/2022 - v9.9.3 - Correct publication synchronised with teleperiod
    26/10/2022 - v10.0  - Add bar graph monthly (every day) and yearly (every month)
    06/11/2022 - v10.1  - Bug fixes on bar graphs and change in lltoa conversion
    15/11/2022 - v10.2  - Add bar graph daily (every hour)

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

#ifdef USE_ENERGY_SENSOR
#ifdef USE_TELEINFO

/*********************************************************************************************\
 * Teleinfo historical
 * docs https://www.enedis.fr/sites/default/files/Enedis-NOI-CPT_54E.pdf
 * Teleinfo hardware will be enabled if 
 *     Hardware RX = [TinfoRX]
 *     Rx enable   = [TinfoEN] (optional)
\*********************************************************************************************/

#include <TasmotaSerial.h>
TasmotaSerial *teleinfo_serial = nullptr;

/*************************************************\
 *               Variables
\*************************************************/

// declare teleinfo energy driver and sensor
#define XNRG_15   15
#define XSNS_99   99

// teleinfo constant
#define TELEINFO_VOLTAGE                230       // default voltage provided
#define TELEINFO_VOLTAGE_MAXIMUM        300       // maximum voltage for value to be acceptable
#define TELEINFO_VOLTAGE_MINIMUM        100       // minimum voltage for value to be acceptable
#define TELEINFO_VOLTAGE_REF            200       // voltage reference for max power calculation
#define TELEINFO_VOLTAGE_LOW            220       // minimum acceptable voltage
#define TELEINFO_VOLTAGE_HIGH           240       // maximum acceptable voltage
#define TELEINFO_INDEX_MAX              12        // maximum number of total power counters
#define TELEINFO_INTERVAL_MIN           1         // energy calculation minimum interval (1mn)
#define TELEINFO_INTERVAL_MAX           1440      // energy calculation maximum interval (12h)
#define TELEINFO_PERCENT_MIN            1         // minimum acceptable percentage of energy contract
#define TELEINFO_PERCENT_MAX            200       // maximum acceptable percentage of energy contract
#define TELEINFO_PERCENT_CHANGE         5         // 5% of power change to publish JSON
#define TELEINFO_FILE_DAILY             7         // default number of daily files
#define TELEINFO_FILE_WEEKLY            8         // default number of weekly files

// graph data
#define TELEINFO_GRAPH_SAMPLE           300       // number of samples per graph data (should divide 3600)
#define TELEINFO_GRAPH_WIDTH            1200      // graph width
#define TELEINFO_GRAPH_HEIGHT           600       // default graph height
#define TELEINFO_GRAPH_STEP             100       // graph height mofification step
#define TELEINFO_GRAPH_PERCENT_START    5         // start position of graph window
#define TELEINFO_GRAPH_PERCENT_STOP     95        // stop position of graph window
#define TELEINFO_GRAPH_MAX_BARGRAPH     32        // maximum number of bar graph

#define TELEINFO_GRAPH_INC_VOLTAGE      10
#define TELEINFO_GRAPH_MIN_VOLTAGE      240
#define TELEINFO_GRAPH_DEF_VOLTAGE      250       // default voltage maximum in graph
#define TELEINFO_GRAPH_MAX_VOLTAGE      380
#define TELEINFO_GRAPH_INC_POWER        1000
#define TELEINFO_GRAPH_MIN_POWER        2000
#define TELEINFO_GRAPH_DEF_POWER        3000      // default power maximum consumption in graph
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

// string size
#define TELEINFO_CONTRACTID_MAX         16        // max length of TIC contract ID
#define TELEINFO_PART_MAX               4         // maximum number of parts in a line
#define TELEINFO_LINE_QTY               74        // maximum number of lines handled in a TIC message
#define TELEINFO_LINE_MAX               128       // maximum size of a received TIC line
#define TELEINFO_KEY_MAX                12        // maximum size of a TIC etiquette

// ESP8266/ESP32 specificities
#ifdef ESP32
  #define TELEINFO_DATA_MAX             96        // maximum size of a TIC donnee
#else       // ESP8266
  #define TELEINFO_DATA_MAX             32        // maximum size of a TIC donnee
#endif      // ESP32 & ESP8266

// commands : MQTT
#define D_CMND_TELEINFO_HELP            "help"
#define D_CMND_TELEINFO_ENABLE          "enable"
#define D_CMND_TELEINFO_RATE            "rate"
#define D_CMND_TELEINFO_ROM_UPDATE      "romupd"
#define D_CMND_TELEINFO_CONTRACT        "percent"

#define D_CMND_TELEINFO_MSG_POLICY      "msgpol"
#define D_CMND_TELEINFO_MSG_TYPE        "msgtype"

#define D_CMND_TELEINFO_LOG_ROTATE      "logrot"
#define D_CMND_TELEINFO_LOG_POLICY      "logpol"
#define D_CMND_TELEINFO_LOG_DAY         "logday"
#define D_CMND_TELEINFO_LOG_WEEK        "logweek"

#define D_CMND_TELEINFO_MAX_V           "maxv"
#define D_CMND_TELEINFO_MAX_VA          "maxva"
#define D_CMND_TELEINFO_MAX_KWH_MONTH   "maxmonth"
#define D_CMND_TELEINFO_MAX_KWH_DAY     "maxday"
#define D_CMND_TELEINFO_MAX_KWH_HOUR    "maxhour"

// commands : Web
#define D_CMND_TELEINFO_MODE            "mode"
#define D_CMND_TELEINFO_ETH             "eth"
#define D_CMND_TELEINFO_PHASE           "phase"
#define D_CMND_TELEINFO_PERIOD          "period"
#define D_CMND_TELEINFO_HISTO           "histo"
#define D_CMND_TELEINFO_HOUR            "hour"
#define D_CMND_TELEINFO_DAY             "day"
#define D_CMND_TELEINFO_MONTH           "month"
#define D_CMND_TELEINFO_HEIGHT          "height"
#define D_CMND_TELEINFO_DATA            "data"
#define D_CMND_TELEINFO_INCREMENT       "incr"
#define D_CMND_TELEINFO_DECREMENT       "decr"

// JSON TIC extensions
#define TELEINFO_JSON_TIC               "TIC"
#define TELEINFO_JSON_METER             "METER"
#define TELEINFO_JSON_PHASE             "PH"
#define TELEINFO_JSON_ISUB              "ISUB"
#define TELEINFO_JSON_PSUB              "PSUB"
#define TELEINFO_JSON_PMAX              "PMAX"

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

// Historic data files
#define TELEINFO_HISTO_BUFFER_MAX       2048      // log buffer
#define TELEINFO_HISTO_DAY_MAX          31        // max number of daily histotisation files
#define TELEINFO_HISTO_WEEK_MAX         52        // max number of weekly histotisation files
#define TELEINFO_HISTO_LINE_LENGTH      256       // maximum line length in CSV files

#ifdef USE_UFILESYS

// configuration and history files
#define D_TELEINFO_CFG                  "/teleinfo.cfg"
const char D_TELEINFO_HISTO_FILE_DAY[]  PROGMEM = "/teleinfo-day-%u.csv";
const char D_TELEINFO_HISTO_FILE_WEEK[] PROGMEM = "/teleinfo-week-%02u.csv";
const char D_TELEINFO_HISTO_FILE_YEAR[] PROGMEM = "/teleinfo-year-%04d.csv";

#endif    // USE_UFILESYS

// web URL
const char D_TELEINFO_ICON_PNG[]        PROGMEM = "/icon.png";
const char D_TELEINFO_PAGE_CONFIG[]     PROGMEM = "/config";
const char D_TELEINFO_PAGE_TIC[]        PROGMEM = "/tic";
const char D_TELEINFO_PAGE_TIC_UPD[]    PROGMEM = "/tic.upd";
const char D_TELEINFO_PAGE_GRAPH[]      PROGMEM = "/graph";
const char D_TELEINFO_PAGE_GRAPH_UPD[]  PROGMEM = "/graph.upd";

// MQTT commands : tic_help, tic_enable, tic_rate, tic_msgp, tic_msgt, tic_log, tic_ival, tic_adj, tic_rot, tic_mini, tic_daily and tic_weekly
const char kTeleinfoCommands[]          PROGMEM = "tic_" "|" D_CMND_TELEINFO_HELP "|" D_CMND_TELEINFO_ENABLE "|" D_CMND_TELEINFO_RATE "|" D_CMND_TELEINFO_MSG_POLICY "|" D_CMND_TELEINFO_MSG_TYPE "|" D_CMND_TELEINFO_ROM_UPDATE "|" D_CMND_TELEINFO_CONTRACT "|" D_CMND_TELEINFO_LOG_ROTATE "|" D_CMND_TELEINFO_LOG_POLICY "|" D_CMND_TELEINFO_LOG_DAY "|" D_CMND_TELEINFO_LOG_WEEK "|" D_CMND_TELEINFO_MAX_V "|" D_CMND_TELEINFO_MAX_VA "|" D_CMND_TELEINFO_MAX_KWH_HOUR "|" D_CMND_TELEINFO_MAX_KWH_DAY "|" D_CMND_TELEINFO_MAX_KWH_MONTH;
void (* const TeleinfoCommand[])(void)  PROGMEM = { &CmndTeleinfoHelp, &CmndTeleinfoEnable, &CmndTeleinfoRate, &CmndTeleinfoMessagePolicy, &CmndTeleinfoMessageType, &CmndTeleinfoRomUpdate, &CmndTeleinfoContractPercent, &CmndTeleinfoLogRotate, &CmndTeleinfoLogPolicy, &CmndTeleinfoLogNbDay, &CmndTeleinfoLogNbWeek, &CmndTeleinfoGraphMaxV, &CmndTeleinfoGraphMaxVA, &CmndTeleinfoGraphMaxKWhHour, &CmndTeleinfoGraphMaxKWhDay, &CmndTeleinfoGraphMaxKWhMonth};

// Specific etiquettes
enum TeleinfoEtiquette { TIC_NONE, TIC_ADCO, TIC_ADSC, TIC_PTEC, TIC_NGTF, TIC_EAIT, TIC_IINST, TIC_IINST1, TIC_IINST2, TIC_IINST3, TIC_ISOUSC, TIC_PS, TIC_PAPP, TIC_SINSTS, TIC_SINSTI, TIC_BASE, TIC_EAST, TIC_HCHC, TIC_HCHP, TIC_EJPHN, TIC_EJPHPM, TIC_BBRHCJB, TIC_BBRHPJB, TIC_BBRHCJW, TIC_BBRHPJW, TIC_BBRHCJR, TIC_BBRHPJR, TIC_ADPS, TIC_ADIR1, TIC_ADIR2, TIC_ADIR3, TIC_URMS1, TIC_URMS2, TIC_URMS3, TIC_UMOY1, TIC_UMOY2, TIC_UMOY3, TIC_IRMS1, TIC_IRMS2, TIC_IRMS3, TIC_SINSTS1, TIC_SINSTS2, TIC_SINSTS3, TIC_PTCOUR, TIC_PTCOUR1, TIC_PTCOUR2, TIC_PREF, TIC_PCOUP, TIC_LTARF, TIC_EASF01, TIC_EASF02, TIC_EASF03, TIC_EASF04, TIC_EASF05, TIC_EASF06, TIC_EASF07, TIC_EASF08, TIC_EASF09, TIC_EASF10, TIC_ADS, TIC_CONFIG, TIC_EAPS, TIC_EAS, TIC_EAPPS, TIC_PREAVIS, TIC_MAX };
const char kTeleinfoEtiquetteName[]     PROGMEM = "|ADCO|ADSC|PTEC|NGTF|EAIT|IINST|IINST1|IINST2|IINST3|ISOUSC|PS|PAPP|SINSTS|SINSTI|BASE|EAST|HCHC|HCHP|EJPHN|EJPHPM|BBRHCJB|BBRHPJB|BBRHCJW|BBRHPJW|BBRHCJR|BBRHPJR|ADPS|ADIR1|ADIR2|ADIR3|URMS1|URMS2|URMS3|UMOY1|UMOY2|UMOY3|IRMS1|IRMS2|IRMS3|SINSTS1|SINSTS2|SINSTS3|PTCOUR|PTCOUR1|PTCOUR2|PREF|PCOUP|LTARF|EASF01|EASF02|EASF03|EASF04|EASF05|EASF06|EASF07|EASF08|EASF09|EASF10|ADS|CONFIG|EAP_s|EA_s|EAPP_s|PREAVIS";

// TIC - modes and rates
enum TeleinfoType { TIC_TYPE_UNDEFINED, TIC_TYPE_CONSO, TIC_TYPE_PROD, TIC_TYPE_MAX };
const char kTeleinfoTypeName[] PROGMEM = "|Consommateur|Producteur";
enum TeleinfoMode { TIC_MODE_UNDEFINED, TIC_MODE_HISTORIC, TIC_MODE_STANDARD, TIC_MODE_PME, TIC_MODE_MAX };
const char kTeleinfoModeName[] PROGMEM = "Inconnu|Historique|Standard|PME";
const uint16_t ARR_TELEINFO_RATE[] = { 1200, 9600, 19200 }; 

// Tarifs                                  [  Toutes   ]   [ Creuses       Pleines   ] [ Normales   PointeMobile ] [CreusesBleu  CreusesBlanc  CreusesRouge  PleinesBleu   PleinesBlanc  PleinesRouge] [ Pointe   PointeMobile  Hiver      Pleines     Creuses    PleinesHiver CreusesHiver PleinesEte   CreusesEte   Pleines1/2S  Creuses1/2S  JuilletAout] [Pointe PleinesHiver CreusesHiver PleinesEte CreusesEte] [ Base  ]  [ Pleines    Creuses   ]  [ Creuses bleu     Creuse Blanc       Creuse Rouge      Pleine Bleu     Pleine Blanc      Pleine Rouge ]  [ Normale      Pointe  ]  [Production]
enum TeleinfoPeriod                        { TIC_HISTO_TH, TIC_HISTO_HC, TIC_HISTO_HP, TIC_HISTO_HN, TIC_HISTO_PM, TIC_HISTO_CB, TIC_HISTO_CW, TIC_HISTO_CR, TIC_HISTO_PB, TIC_HISTO_PW, TIC_HISTO_PR, TIC_STD_P, TIC_STD_PM, TIC_STD_HH, TIC_STD_HP, TIC_STD_HC, TIC_STD_HPH, TIC_STD_HCH, TIC_STD_HPE, TIC_STD_HCE, TIC_STD_HPD, TIC_STD_HCD, TIC_STD_JA, TIC_STD_1, TIC_STD_2, TIC_STD_3, TIC_STD_4, TIC_STD_5, TIC_STD_BASE, TIC_STD_HPL, TIC_STD_HCR, TIC_STD_HC_BLEU, TIC_STD_HC_BLANC, TIC_STD_HC_ROUGE, TIC_STD_HP_BLEU, TIC_STD_HP_BLANC, TIC_STD_HP_ROUGE, TIC_STD_HNO, TIC_STD_HPT, TIC_STD_PROD, TIC_PERIOD_MAX };
const int ARR_TELEINFO_PERIOD_FIRST[]    = { TIC_HISTO_TH, TIC_HISTO_HC, TIC_HISTO_HC, TIC_HISTO_HN, TIC_HISTO_HN, TIC_HISTO_CB, TIC_HISTO_CB, TIC_HISTO_CB, TIC_HISTO_CB, TIC_HISTO_CB, TIC_HISTO_CB, TIC_STD_P, TIC_STD_P,  TIC_STD_P,  TIC_STD_P,  TIC_STD_P,  TIC_STD_P,   TIC_STD_P,   TIC_STD_P,   TIC_STD_P,   TIC_STD_P,   TIC_STD_P,   TIC_STD_P,  TIC_STD_1, TIC_STD_1, TIC_STD_1, TIC_STD_1, TIC_STD_1, TIC_STD_BASE, TIC_STD_HPL, TIC_STD_HPL, TIC_STD_HC_BLEU, TIC_STD_HC_BLEU,  TIC_STD_HC_BLEU,  TIC_STD_HC_BLEU, TIC_STD_HC_BLEU,  TIC_STD_HC_BLEU,  TIC_STD_HNO, TIC_STD_HNO, TIC_STD_PROD };
const int ARR_TELEINFO_PERIOD_NUMBER[]   = { 1,            2,            2,            2,            2,            6,            6,            6,            6,            6,            6,            12,        12,         12,         12,         12,         12,          12,          12,          12,          12,          12,          12,         5,         5,         5,         5,         5,         1,            2,           2          ,         6      ,       6         ,       6        ,        6        ,       6        ,         6       ,       2     ,      2     ,      1       };
const char kTeleinfoPeriod[] PROGMEM     = "TH..|HC..|HP..|HN..|PM..|HCJB|HCJW|HCJR|HPJB|HPJW|HPJR|P|PM|HH|HP|HC|HPH|HCH|HPE|HCE|HPD|HCD|JA|1|2|3|4|5|BASE|HEURE PLEINE|HEURE CREUSE|HC BLEU|HC BLANC|HC ROUGE|HP BLEU|HP BLANC|HP ROUGE|HEURE NORMALE|HEURE POINTE|INDEX NON CONSO";
const char kTeleinfoPeriodName[] PROGMEM = "Toutes|Creuses|Pleines|Normales|Pointe Mobile|Creuses Bleu|Creuses Blanc|Creuses Rouge|Pleines Bleu|Pleines Blanc|Pleines Rouge|Pointe|Pointe Mobile|Hiver|Pleines|Creuses|Pleines Hiver|Creuses Hiver|Pleines Ete|Creuses Ete|Pleines Demi-saison|Creuses Demi-saison|Juillet-Aout|Pointe|Pleines Hiver|Creuses Hiver|Pleines Ete|Creuses Ete|Base|Pleines|Creuse|Creuses Bleu|Creuses Blanc|Creuses Rouge|Pleines Bleu|Pleines Blanc|Pleines Rouge|Normale|Pointe|Production";

// Data diffusion policy
enum TeleinfoMessagePolicy { TELEINFO_POLICY_NEVER, TELEINFO_POLICY_MESSAGE, TELEINFO_POLICY_PERCENT, TELEINFO_POLICY_TELEMETRY, TELEINFO_POLICY_MAX };
const char kTeleinfoMessagePolicy[] PROGMEM = "Never|Every TIC message|When Power fluctuates (± 5%)|With Telemetry only";
enum TeleinfoMessageType { TELEINFO_MSG_NONE, TELEINFO_MSG_METER, TELEINFO_MSG_TIC, TELEINFO_MSG_BOTH, TELEINFO_MSG_MAX };
const char kTeleinfoMessageType[] PROGMEM = "None|METER only|TIC only|METER and TIC";

// graph - periods
enum TeleinfoGraphPeriod { TELEINFO_PERIOD_LIVE, TELEINFO_PERIOD_DAY, TELEINFO_PERIOD_WEEK, TELEINFO_PERIOD_YEAR, TELEINFO_PERIOD_MAX };              // available graph periods
const char kTeleinfoGraphPeriod[] PROGMEM = "Live|Day|Week|Year";                                                                                     // period labels
const long ARR_TELEINFO_PERIOD_WINDOW[] = { 3600 / TELEINFO_GRAPH_SAMPLE, 86400 / TELEINFO_GRAPH_SAMPLE, 604800 / TELEINFO_GRAPH_SAMPLE };            // time window between samples (sec.)
const long ARR_TELEINFO_PERIOD_FLUSH[]  = { 1, 3600, 21600 };                                                                                         // max time between log flush (sec.)

// graph - display
enum TeleinfoGraphDisplay { TELEINFO_UNIT_VA, TELEINFO_UNIT_W, TELEINFO_UNIT_V, TELEINFO_UNIT_COSPHI, TELEINFO_UNIT_PEAK_VA, TELEINFO_UNIT_PEAK_V, TELEINFO_UNIT_WH, TELEINFO_UNIT_MAX };       // available graph units
const char kTeleinfoGraphDisplay[] PROGMEM = "VA|W|V|cosφ|VA|V|Wh";                                                                                                                             // units labels

// graph - phase colors
const char kTeleinfoGraphColorPhase[] PROGMEM = "#6bc4ff|#ffca74|#23bf64";                            // phase colors (blue, orange, green)
const char kTeleinfoGraphColorPeak[]  PROGMEM = "#5dade2|#d9ad67|#20a457";                            // peak colors (blue, orange, green)

// month and week day names
const char kTeleinfoYearMonth[] PROGMEM = "|Jan|Fév|Mar|Avr|Mai|Jun|Jui|Aoû|Sep|Oct|Nov|Déc";         // month name for selection
const char kTeleinfoWeekDay[]   PROGMEM = "Lun|Mar|Mer|Jeu|Ven|Sam|Dim";                              // day name for selection
const char kTeleinfoWeekDay2[]  PROGMEM = "lu|ma|me|je|ve|sa|di";                                     // day name for bar graph

// power calculation modes
enum TeleinfoPowerCalculationMethod { TIC_METHOD_GLOBAL_COUNTER, TIC_METHOD_INCREMENT };
enum TeleinfoPowerTarget { TIC_POWER_UPDATE_COUNTER, TIC_POWER_UPDATE_PAPP, TIC_POWER_UPDATE_PACT };

// serial port management
enum TeleinfoSerialStatus { TIC_SERIAL_INIT, TIC_SERIAL_ACTIVE, TIC_SERIAL_STOPPED, TIC_SERIAL_FAILED };

// log write policy
enum TeleinfoLogPolicy { TELEINFO_LOG_BUFFERED, TELEINFO_LOG_IMMEDIATE, TELEINFO_LOG_MAX};

/****************************************\
 *                 Data
\****************************************/

// teleinfo : configuration
static struct {
  uint16_t baud_rate     = 1200;                            // meter transmission rate (bauds)
  int      msg_policy    = TELEINFO_POLICY_TELEMETRY;       // publishing policy for data
  int      msg_type      = TELEINFO_MSG_METER;              // type of data to publish (METER and/or TIC)
  int      rom_update    = 720;                             // update interval
  int      calc_delta    = 1;                               // minimum delta for power calculation
  int      contract_percent = 100;                          // percentage adjustment to maximum contract power
  int      log_policy    = TELEINFO_LOG_BUFFERED;           // log writing policy
  uint8_t  log_nbday     = TELEINFO_HISTO_DAY_MAX;          // number of daily historisation files
  uint8_t  log_nbweek    = TELEINFO_HISTO_WEEK_MAX;         // number of weekly historisation files
  int      height        = TELEINFO_GRAPH_HEIGHT;           // graph height in pixels
  long     max_va        = TELEINFO_GRAPH_DEF_POWER;        // maximum graph value for power
  int      max_v         = TELEINFO_GRAPH_DEF_VOLTAGE;      // maximum graph value for voltage
  long     max_kwh_hour  = TELEINFO_GRAPH_DEF_WH_HOUR;      // maximum hourly graph value for daily active power total
  long     max_kwh_day   = TELEINFO_GRAPH_DEF_WH_DAY;       // maximum daily graph value for monthly active power total
  long     max_kwh_month = TELEINFO_GRAPH_DEF_WH_MONTH;     // maximum monthly graph value for yearly active power total
} teleinfo_config;

// power calculation structure
static struct {
  uint8_t   method = TIC_METHOD_GLOBAL_COUNTER;         // power calculation method
  float     papp_inc[ENERGY_MAX_PHASES];                // apparent power counter increment per phase (in vah)
  long      papp_current_counter    = LONG_MAX;         // current apparent power counter (in vah)
  long      papp_previous_counter   = LONG_MAX;         // previous apparent power counter (in vah)
  long      pact_current_counter    = LONG_MAX;         // current active power counter (in wh)
  long      pact_previous_counter   = LONG_MAX;         // previous active power counter (in wh)
  uint32_t  previous_time_message   = UINT32_MAX;       // timestamp of previous message
  uint32_t  previous_time_counter   = UINT32_MAX;       // timestamp of previous global counter increase
} teleinfo_calc; 

// TIC : current message
struct tic_line {
  char str_etiquette[TELEINFO_KEY_MAX];             // TIC line etiquette
  char str_donnee[TELEINFO_DATA_MAX];               // TIC line donnee
  char checksum;
};
static struct {
  bool     overload      = false;                   // overload has been detected
  bool     received      = false;                   // one full message has been received
  bool     percent       = false;                   // power has changed of more than 1% on one phase
  bool     publish_msg   = false;                   // flag to ask to publish data
  bool     publish_tic   = false;                   // flag to ask to publish TIC JSON
  bool     publish_meter = false;                   // flag to ask to publish Meter JSON
  int      line_index    = 0;                       // index of current received message line
  int      line_max      = 0;                       // max number of lines in a message
  int      length        = INT_MAX;                 // length of message     
  uint32_t timestamp     = UINT32_MAX;              // timestamp of message (ms)
  tic_line line[TELEINFO_LINE_QTY];                 // array of message lines
} teleinfo_message;

// teleinfo : contract data
static struct {
  uint8_t   phase      = 1;                         // number of phases
  uint8_t   mode       = TIC_MODE_UNDEFINED;        // meter mode (historic, standard)
  uint8_t   type       = TIC_TYPE_CONSO;            // meter running type (conso or prod)
  int       period     = -1;                        // current tarif period
  int       nb_indexes = -1;                        // number of indexes in current contract      
  long      voltage    = TELEINFO_VOLTAGE;          // contract reference voltage
  long      isousc     = 0;                         // contract max current per phase
  long      ssousc     = 0;                         // contract max power per phase
  long long ident      = 0;                         // contract identification
} teleinfo_contract;

// teleinfo : power meter
static struct {
  bool      enabled        = true;                  // reception enabled by default
  uint8_t   status_rx      = TIC_SERIAL_INIT;       // Teleinfo Rx initialisation status
  int       interval_count = 0;                     // energy publication counter      
  long      papp           = 0;                     // current apparent power 
  long      nb_message     = 0;                     // total number of messages sent by the meter
  long      nb_reset       = 0;                     // total number of message reset sent by the meter
  long      nb_update      = 0;                     // number of cosphi calculation updates
  long long nb_line        = 0;                     // total number of received lines
  long long nb_error       = 0;                     // total number of checksum errors

  long long total_wh       = 0;                     // total of all indexes of active powerhisto
  long long index_wh[TELEINFO_INDEX_MAX];           // array of indexes of different tarif periods
  long long day_last_wh    = 0;                     // previous daily total
  long long hour_last_wh   = 0;                     // previous hour total
  long      hour_wh[24];                            // hourly increments

  char      sep_line = 0;                           // detected line separator
  uint8_t   idx_line = 0;                           // caracter index of current line
  char      str_line[TELEINFO_LINE_MAX];            // reception buffer for current line
} teleinfo_meter;

// teleinfo : actual data per phase
struct tic_phase {
  bool  volt_set;                                    // voltage set in current message
  long  voltage;                                     // instant voltage
  long  current;                                     // instant current
  long  papp;                                        // instant apparent power
  long  pact;                                        // instant active power
  long  papp_last;                                   // last published apparent power
  float cosphi;                                      // cos phi (x100)
}; 
static tic_phase teleinfo_phase[ENERGY_MAX_PHASES];

// teleinfo : calculation periods data
struct tic_period {
  bool     updated;                                 // flag to ask for graph update
  int      index;                                   // current array index per refresh period (day of year for yearly period)
  long     counter;                                 // counter in seconds of current refresh period (10k*year + 100*month + day_of_month for yearly period)

  long  papp_peak[ENERGY_MAX_PHASES];               // peak apparent power during refresh period
  long  volt_low[ENERGY_MAX_PHASES];                // lowest voltage during refresh period
  long  volt_peak[ENERGY_MAX_PHASES];               // peak high voltage during refresh period
  long  long papp_sum[ENERGY_MAX_PHASES];           // sum of apparent power during refresh period
  long  long pact_sum[ENERGY_MAX_PHASES];           // sum of active power during refresh period
  float cosphi_sum[ENERGY_MAX_PHASES];              // sum of cos phi during refresh period

  uint32_t time_flush;                              // timestamp of next log writing
  String   str_filename;                            // log filename
  String   str_buffer;                              // buffer of data waiting to be logged
}; 
static tic_period teleinfo_period[TELEINFO_PERIOD_MAX];

// teleinfo : graph data
struct tic_graph {
  uint8_t arr_papp[ENERGY_MAX_PHASES][TELEINFO_GRAPH_SAMPLE];     // array of apparent power graph values
  uint8_t arr_pact[ENERGY_MAX_PHASES][TELEINFO_GRAPH_SAMPLE];     // array of active power graph values
  uint8_t arr_volt[ENERGY_MAX_PHASES][TELEINFO_GRAPH_SAMPLE];     // array min and max voltage delta
  uint8_t arr_cosphi[ENERGY_MAX_PHASES][TELEINFO_GRAPH_SAMPLE];   // array of cos phi
};
static struct {
  long    left;                               // left position of the curve
  long    right;                              // right position of the curve
  long    width;                              // width of the curve (in pixels)
  uint8_t period = TELEINFO_PERIOD_LIVE;      // graph period
  uint8_t data = TELEINFO_UNIT_VA;            // graph default data
  uint8_t histo = 0;                          // graph histotisation index
  uint8_t month = 0;                          // graph current month
  uint8_t day = 0;                            // graph current day
  long papp_peak[ENERGY_MAX_PHASES];          // peak apparent power to save
  long volt_low[ENERGY_MAX_PHASES];           // voltage to save
  long volt_peak[ENERGY_MAX_PHASES];          // voltage to save
  long papp[ENERGY_MAX_PHASES];               // apparent power to save
  long pact[ENERGY_MAX_PHASES];               // active power to save
  int  cosphi[ENERGY_MAX_PHASES];             // cosphi to save
  char str_phase[ENERGY_MAX_PHASES + 1];      // phases to displayed
  tic_graph data_live;                        // grpah display value per period
} teleinfo_graph;

/****************************************\
 *               Icons
 * 
 *      xxd -i -c 256 icon.png
\****************************************/

// icon : teleinfo
const char teleinfo_icon_tic_png[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x04, 0x03, 0x00, 0x00, 0x00, 0x31, 0x10, 0x7c, 0xf8, 0x00, 0x00, 0x00, 0x12, 0x50, 0x4c, 0x54, 0x45, 0x00, 0x6f, 0x6c, 0x00, 0x00, 0x00, 0x4d, 0x82, 0xbd, 0x61, 0x83, 0xb5, 0x67, 0x83, 0xb1, 0xf7, 0xe4, 0xb9, 0xab, 0xda, 0xea, 0xec, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x00, 0x01, 0x62, 0x4b, 0x47, 0x44, 0x00, 0x88, 0x05, 0x1d, 0x48, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x2e, 0x23, 0x00, 0x00, 0x2e, 0x23, 0x01, 0x78, 0xa5, 0x3f, 0x76, 0x00, 0x00, 0x00, 0x07, 0x74, 0x49, 0x4d, 0x45, 0x07, 0xe4, 0x0a, 0x12, 0x17, 0x07, 0x09, 0xde, 0x55, 0xef, 0x17, 0x00, 0x00, 0x02, 0x37, 0x49, 0x44, 0x41, 0x54, 0x68, 0xde, 0xed, 0x99, 0x59, 0x6e, 0xc4, 0x20, 0x0c, 0x86, 0x11, 0x9e, 0x83, 0x58, 0x70, 0x01, 0x64, 0xee, 0x7f, 0xb7, 0x66, 0x99, 0x25, 0xac, 0xc6, 0x38, 0xe9, 0xb4, 0x52, 0xfc, 0x42, 0x35, 0xc4, 0x5f, 0x7e, 0x13, 0xc0, 0x86, 0x1a, 0xf3, 0xf7, 0x0d, 0x9c, 0xf3, 0xb3, 0xbe, 0x71, 0x73, 0x9f, 0x06, 0xac, 0xaf, 0xa6, 0xcd, 0x3f, 0xcc, 0xfa, 0xef, 0x36, 0x1b, 0x00, 0xbd, 0x00, 0x68, 0x74, 0x02, 0x9c, 0x56, 0xc0, 0x2c, 0xc0, 0x29, 0x01, 0x1f, 0x01, 0x5e, 0x0b, 0x40, 0xe5, 0x10, 0x4e, 0x02, 0xec, 0xd7, 0x01, 0xa4, 0x05, 0xb8, 0x13, 0x01, 0x4e, 0x0d, 0xc0, 0xff, 0x09, 0xa0, 0x33, 0x01, 0xe6, 0x2b, 0x00, 0xab, 0x05, 0x80, 0x16, 0x60, 0xd4, 0x00, 0x3b, 0xf1, 0x11, 0x62, 0x5d, 0xc2, 0x38, 0xc0, 0x05, 0xdd, 0xae,
  0x6e, 0x53, 0x00, 0x88, 0x01, 0x90, 0x3d, 0x2a, 0x1e, 0xc3, 0x1c, 0x60, 0xa5, 0x00, 0x9b, 0x3f, 0x4b, 0xd2, 0x31, 0x74, 0x55, 0x09, 0x47, 0x8d, 0x71, 0xcb, 0x13, 0xb1, 0x3d, 0xf7, 0xb0, 0x9c, 0x4c, 0x98, 0xb9, 0xb7, 0xd3, 0xbd, 0x65, 0xe2, 0x8d, 0xc7, 0x2d, 0xa2, 0x24, 0xc0, 0x5e, 0x4a, 0xe0, 0xd0, 0xf2, 0xac, 0x11, 0x88, 0x19, 0xf2, 0xcc, 0xbf, 0x20, 0x40, 0x5b, 0x5b, 0xb1, 0xba, 0xab, 0xd3, 0x8b, 0x29, 0x67, 0xc0, 0x95, 0xd6, 0x52, 0x18, 0x86, 0x02, 0x28, 0x24, 0x50, 0x6f, 0x80, 0xab, 0x02, 0x32, 0xad, 0xc7, 0x9e, 0x18, 0x29, 0xeb, 0xaf, 0x02, 0x5c, 0x13, 0x50, 0x2a, 0xb9, 0x08, 0x80, 0x0c, 0x00, 0x45, 0x00, 0xba, 0x16, 0xb0, 0x76, 0xfb, 0xd8, 0x05, 0x00, 0xf7, 0x9d, 0x2b, 0xcf, 0x21, 0x33, 0x55, 0x90, 0xcb, 0x17, 0xc8, 0xcc, 0x15, 0xe4, 0x32, 0x4e, 0x30, 0xfd, 0x0f, 0xc9, 0x66, 0x1c, 0x66, 0xbd, 0x21, 0x93, 0xf4, 0x3c, 0xb7, 0x62, 0x99, 0xac, 0xe9, 0xb9, 0x25, 0x87, 0x4c, 0xe9, 0x80, 0x4c, 0x42, 0xee, 0x57, 0xe8, 0x80, 0x57, 0x9d, 0x31, 0x5e, 0x22, 0x82, 0x51, 0x58, 0x34, 0xb7, 0x35, 0xc6, 0x16, 0x55, 0xfe, 0x56, 0xf7, 0x69, 0xdf, 0x33, 0x4c, 0x5c, 0x6b, 0x7a, 0xcc, 0x56, 0x89, 0x2c, 0x0e, 0x78, 0xbf, 0xd3, 0xce, 0x4d, 0xd0, 0xc3, 0x2b, 0xa7, 0x8a, 0x56, 0x38, 0x00, 0x66, 0xaa, 0xd6, 0x74, 0xd1, 0xce, 0x1c, 0x3e, 0xd2, 0x55, 0x2d, 0x0f, 0x02, 0xd2, 0xc7, 0xe5, 0x41, 0x50, 0xf6, 0x3e, 0x69, 0x10, 0x90, 0x0b, 0xb6, 0xc2, 0x20, 0xca, 0x9a, 0x4b, 0x26, 0x81, 0xdc, 0x14, 0xe0, 0x11, 0x3a, 0x7b, 0xe3, 0x48, 0x0c, 0xe0, 0x8a, 0xfd, 0x5d, 0x78, 0xa1, 0x40, 0x4b, 0x12, 0x8f, 0xd4, 0x48, 0x2f, 0x23, 0x49, 0x87, 0x7a,
  0x09, 0xf2, 0x4c, 0x00, 0x8e, 0x1c, 0x57, 0xcf, 0x01, 0xa0, 0xf0, 0x4e, 0x43, 0x0d, 0x80, 0xd3, 0x01, 0xd2, 0x3b, 0x0d, 0xe8, 0x08, 0xb0, 0x5a, 0xc0, 0xd8, 0xad, 0x4a, 0xaf, 0xce, 0xb2, 0x72, 0x00, 0x0e, 0x96, 0x88, 0x4d, 0x42, 0xbb, 0x77, 0xf6, 0xf6, 0xc7, 0xf2, 0x80, 0xcf, 0xe9, 0xd4, 0x75, 0x66, 0xda, 0xd0, 0xdd, 0x49, 0xa7, 0x77, 0x28, 0x99, 0x77, 0xfa, 0x94, 0xa5, 0x86, 0xae, 0xd2, 0xb8, 0xed, 0xb6, 0xdb, 0x7e, 0xcd, 0xe2, 0x62, 0xe1, 0xdd, 0x94, 0x3f, 0xae, 0xff, 0x7c, 0xdb, 0x9b, 0xce, 0x5e, 0xe2, 0x9f, 0xf9, 0x21, 0xdf, 0x86, 0x9e, 0x07, 0xba, 0xee, 0xb9, 0xee, 0x79, 0xb2, 0xa8, 0x01, 0x5e, 0x9e, 0x0f, 0x06, 0xe0, 0xcd, 0x06, 0x08, 0xcb, 0xfe, 0xfc, 0x01, 0xf8, 0xa5, 0xee, 0x59, 0x00, 0x1e, 0xf6, 0x26, 0xbf, 0x7d, 0x4d, 0x00, 0xb8, 0x7a, 0xe6, 0x80, 0xf5, 0xc7, 0xb7, 0x27, 0x03, 0x08, 0xbb, 0x82, 0xe5, 0x4f, 0x7f, 0x04, 0x58, 0x97, 0x36, 0x78, 0x25, 0x80, 0xfc, 0x06, 0x20, 0x9f, 0x86, 0x20, 0x00, 0x3c, 0xc7, 0xa0, 0x06, 0x20, 0xad, 0x02, 0x16, 0x40, 0x2f, 0x80, 0x58, 0xc1, 0xe1, 0x6e, 0x87, 0xf6, 0x49, 0x94, 0xe6, 0x69, 0x4a, 0x1a, 0x5b, 0xde, 0x04, 0x0d, 0x02, 0x6c, 0x06, 0xb8, 0xf3, 0xdc, 0x6e, 0x3f, 0x61, 0x43, 0x14, 0x0c, 0xfe, 0x63, 0x6f, 0x4d, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
unsigned int teleinfo_icon_tic_len = 720;

/*******************************************\
 *               Accessor
\*******************************************/

// Validate that teleinfo baud rate is standard
uint16_t TeleinfoValidateBaudRate (uint16_t baud_rate)
{
  bool is_conform = false;
  int  index;

  // check if rate is within allowed rates, else set to 1200
  for (index = 0; index < nitems (ARR_TELEINFO_RATE); index ++) 
    if (baud_rate == ARR_TELEINFO_RATE[index]) is_conform = true;
  if (!is_conform) baud_rate = 1200;
  
  return baud_rate;
}

/**************************************************\
 *                  Commands
\**************************************************/

// teleinfo help
void CmndTeleinfoHelp ()
{
  uint8_t index;
  char    str_text[32];

  AddLog (LOG_LEVEL_INFO,   PSTR ("HLP: TIC commands"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - tic_enable <0/1> = enable teleinfo"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - tic_rate <rate>  = set serial rate"));

  AddLog (LOG_LEVEL_INFO,   PSTR (" Policy :"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - tic_romupd <val>  = ROM update interval (mn)"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - tic_percent <val> = maximum acceptable contract (in %%)"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - tic_msgpol        = message publish policy :"));
  for (index = 0; index < TELEINFO_POLICY_MAX; index++)
  {
    GetTextIndexed (str_text, sizeof (str_text), index, kTeleinfoMessagePolicy);
    AddLog (LOG_LEVEL_INFO, PSTR ("   %u - %s"), index, str_text);
  }
  AddLog (LOG_LEVEL_INFO,   PSTR (" - tic_msgtype       = message type publish policy :"));
  for (index = 0; index < TELEINFO_MSG_MAX; index++)
  {
    GetTextIndexed (str_text, sizeof (str_text), index, kTeleinfoMessageType);
    AddLog (LOG_LEVEL_INFO, PSTR ("   %u - %s"), index, str_text);
  }

#ifdef USE_UFILESYS
  AddLog (LOG_LEVEL_INFO,   PSTR (" Logs [littlefs] :"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - tic_logpol <0/1>  = log policy (0:buffered, 1:immediate)"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - tic_logday <val>  = number of daily logs"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - tic_logweek <val> = number of weekly logs"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - tic_logrot        = force log rotate"));

  AddLog (LOG_LEVEL_INFO,   PSTR (" Graphs :"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - tic_maxv <val>     = maximum voltage (v)"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - tic_maxva <val>    = maximum power (va and w)"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - tic_maxhour <val>  = maximum total per hour (wh)"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - tic_maxday <val>   = maximum total per day (wh)"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - tic_maxmonth <val> = maximum total per month (wh)"));
#endif        // USE_UFILESYS

  ResponseCmndDone();
}

// Start and stop Teleinfo serial reception
void CmndTeleinfoEnable (void)
{
  bool result;

  // if parameter is provided
  result = (XdrvMailbox.data_len > 0);
  if (result)
  {
    // if serial enable
    if ((strcasecmp (XdrvMailbox.data, MQTT_STATUS_ON) == 0)  || (XdrvMailbox.payload == 1)) result = TeleinfoEnableSerial ();

    // else if serial disabled
    else if ((strcasecmp (XdrvMailbox.data, MQTT_STATUS_OFF) == 0) || (XdrvMailbox.payload == 0)) result = TeleinfoDisableSerial ();
  } 

  // send response
  if (result) ResponseCmndDone ();
    else ResponseCmndFailed ();
}

void CmndTeleinfoRate (void)
{
  if (XdrvMailbox.data_len > 0) 
  {
    teleinfo_config.baud_rate = TeleinfoValidateBaudRate ((uint16_t)XdrvMailbox.payload);
    TeleinfoSaveConfig ();
  }
  ResponseCmndNumber (teleinfo_config.baud_rate);
}

void CmndTeleinfoRomUpdate (void)
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload >= TELEINFO_INTERVAL_MIN) && (XdrvMailbox.payload <= TELEINFO_INTERVAL_MAX))
  {
    teleinfo_config.rom_update = XdrvMailbox.payload;
    TeleinfoSaveConfig ();
  }
  ResponseCmndNumber (teleinfo_config.rom_update);
}

void CmndTeleinfoContractPercent (void)
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload >= TELEINFO_PERCENT_MIN) && (XdrvMailbox.payload <= TELEINFO_PERCENT_MAX))
  {
    teleinfo_config.contract_percent = XdrvMailbox.payload;
    TeleinfoSaveConfig ();
  }
  ResponseCmndNumber (teleinfo_config.contract_percent);
}

void CmndTeleinfoMessagePolicy (void)
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload >= 0) && (XdrvMailbox.payload < TELEINFO_POLICY_MAX))
  {
    teleinfo_config.msg_policy = XdrvMailbox.payload;
    TeleinfoSaveConfig ();
  }
  ResponseCmndNumber (teleinfo_config.msg_policy);
}

void CmndTeleinfoMessageType (void)
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload >= 0) && (XdrvMailbox.payload < TELEINFO_MSG_MAX))
  {
    teleinfo_config.msg_type = XdrvMailbox.payload;
    TeleinfoSaveConfig ();
  }
  ResponseCmndNumber (teleinfo_config.msg_type);
}

void CmndTeleinfoLogRotate (void)
{
#ifdef USE_UFILESYS
  TeleinfoRotateLog ();
#endif      // USE_UFILESYS
  // send response
  ResponseCmndDone ();
}

void CmndTeleinfoLogPolicy (void)
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload < TELEINFO_LOG_MAX))
  {
    teleinfo_config.log_policy = XdrvMailbox.payload;
    TeleinfoSaveConfig ();
  }
  ResponseCmndNumber (teleinfo_config.log_policy);
}

void CmndTeleinfoLogNbDay (void)
{
  if ((XdrvMailbox.payload > 0) && (XdrvMailbox.payload < 31))
  {
    teleinfo_config.log_nbday = XdrvMailbox.payload;
    TeleinfoSaveConfig ();
  }
  ResponseCmndNumber (teleinfo_config.log_nbday);
}

void CmndTeleinfoLogNbWeek (void)
{
  if ((XdrvMailbox.payload > 0) && (XdrvMailbox.payload < 52))
  {
    teleinfo_config.log_nbweek = XdrvMailbox.payload;
    TeleinfoSaveConfig ();
  }
  ResponseCmndNumber (teleinfo_config.log_nbweek);
}

void CmndTeleinfoGraphMaxV (void)
{
  if ((XdrvMailbox.payload >= TELEINFO_GRAPH_MIN_VOLTAGE) && (XdrvMailbox.payload <= TELEINFO_GRAPH_MAX_VOLTAGE))
  {
    teleinfo_config.max_v = XdrvMailbox.payload;
    TeleinfoSaveConfig ();
  }
  ResponseCmndNumber (teleinfo_config.max_v);
}

void CmndTeleinfoGraphMaxVA (void)
{
  if ((XdrvMailbox.payload >= TELEINFO_GRAPH_MIN_POWER) && (XdrvMailbox.payload <= TELEINFO_GRAPH_MAX_POWER))
  {
    teleinfo_config.max_va = XdrvMailbox.payload;
    TeleinfoSaveConfig ();
  }
  ResponseCmndNumber (teleinfo_config.max_va);
}

void CmndTeleinfoGraphMaxKWhHour (void)
{
  if ((XdrvMailbox.payload >= TELEINFO_GRAPH_MIN_WH_HOUR) && (XdrvMailbox.payload <= TELEINFO_GRAPH_MAX_WH_HOUR))
  {
    teleinfo_config.max_kwh_hour = XdrvMailbox.payload;
    TeleinfoSaveConfig ();
  }
  ResponseCmndNumber (teleinfo_config.max_kwh_hour);
}

void CmndTeleinfoGraphMaxKWhDay (void)
{
  if ((XdrvMailbox.payload >= TELEINFO_GRAPH_MIN_WH_DAY) && (XdrvMailbox.payload <= TELEINFO_GRAPH_MAX_WH_DAY))
  {
    teleinfo_config.max_kwh_day = XdrvMailbox.payload;
    TeleinfoSaveConfig ();
  }
  ResponseCmndNumber (teleinfo_config.max_kwh_day);
}

void CmndTeleinfoGraphMaxKWhMonth (void)
{
  if ((XdrvMailbox.payload >= TELEINFO_GRAPH_MIN_WH_MONTH) && (XdrvMailbox.payload <= TELEINFO_GRAPH_MAX_WH_MONTH))
  {
    teleinfo_config.max_kwh_month = XdrvMailbox.payload;
    TeleinfoSaveConfig ();
  }
  ResponseCmndNumber (teleinfo_config.max_kwh_month);
}

/*********************************************\
 *               Functions
\*********************************************/

#ifndef ESP32

// conversion from long long to string (not available in standard libraries)
char* lltoa (const long long value, char *pstr_result, const int base)
{
  lldiv_t result;
  char    str_value[12];

  // check parameters
  if (pstr_result == nullptr) return nullptr;
  if (base != 10) return nullptr;

  // if needed convert upper digits
  result = lldiv (value, 10000000000000000LL);
  if (result.quot != 0) ltoa ((long)result.quot, pstr_result, 10);
    else strcpy (pstr_result, "");

  // convert middle digits
  result = lldiv (result.rem, 100000000LL);
  if (result.quot != 0)
  {
    if (strlen (pstr_result) == 0) ltoa ((long)result.quot, str_value, 10);
      else sprintf_P (str_value, PSTR ("%08d"), (long)result.quot);
    strcat (pstr_result, str_value);
  }

  // convert lower digits
  if (strlen (pstr_result) == 0) ltoa ((long)result.rem, str_value, 10);
    else sprintf_P (str_value, PSTR ("%08d"), (long)result.rem);
  strcat (pstr_result, str_value);

  return pstr_result;
}

#endif      // ESP32

// Start serial reception
bool TeleinfoEnableSerial ()
{
  bool is_ready = (teleinfo_serial != nullptr);

  // if serial port is not already created
  if (!is_ready)
  { 
    // check if environment is ok
    is_ready = ((TasmotaGlobal.energy_driver == XNRG_15) && PinUsed (GPIO_TELEINFO_RX) && (teleinfo_config.baud_rate > 0));
    if (is_ready)
    {
#ifdef ESP32
      // create and initialise serial port
      teleinfo_serial = new TasmotaSerial (Pin (GPIO_TELEINFO_RX), -1, 1);
      is_ready = teleinfo_serial->begin (teleinfo_config.baud_rate, SERIAL_7E1);

#else       // ESP8266
      // create and initialise serial port
      teleinfo_serial = new TasmotaSerial (Pin (GPIO_TELEINFO_RX), -1, 1);
      is_ready = teleinfo_serial->begin (teleinfo_config.baud_rate, SERIAL_7E1);

      // force configuration on ESP8266
      if (teleinfo_serial->hardwareSerial ()) ClaimSerial ();
#endif      // ESP32 & ESP8266

      // flush transmit and receive buffer
      if (is_ready)
      {
        teleinfo_serial->flush ();
        while (teleinfo_serial->available()) teleinfo_serial->read (); 
      }

      // log action
      if (is_ready) AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Serial set to 7E1 %d bauds"), teleinfo_config.baud_rate);
      else AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Serial port init failed"));
    }

    // set serial port status
    if (is_ready) teleinfo_meter.status_rx = TIC_SERIAL_ACTIVE; 
    else teleinfo_meter.status_rx = TIC_SERIAL_FAILED;
  }

  // if Teleinfo En declared, enable it
  if (is_ready && PinUsed (GPIO_TELEINFO_ENABLE)) digitalWrite(Pin (GPIO_TELEINFO_ENABLE), HIGH);

  return is_ready;
}

// Stop serial reception
bool TeleinfoDisableSerial ()
{
  // if teleinfo enabled, set it low
  if (PinUsed (GPIO_TELEINFO_ENABLE)) digitalWrite(Pin (GPIO_TELEINFO_ENABLE), LOW);

  // declare serial as stopped
  teleinfo_meter.status_rx = TIC_SERIAL_STOPPED;

  return true;
}

// Load configuration from Settings or from LittleFS
void TeleinfoLoadConfig () 
{
#ifdef USE_UFILESYS

  // retrieve saved settings from flash filesystem
  teleinfo_config.baud_rate        = TeleinfoValidateBaudRate (UfsCfgLoadKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_RATE, 1200));
  teleinfo_config.msg_policy       = UfsCfgLoadKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_MSG_POLICY, TELEINFO_POLICY_TELEMETRY);
  teleinfo_config.msg_type         = UfsCfgLoadKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_MSG_TYPE,   TELEINFO_MSG_METER);
  teleinfo_config.rom_update       = UfsCfgLoadKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_ROM_UPDATE, TELEINFO_INTERVAL_MAX);
  teleinfo_config.contract_percent = UfsCfgLoadKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_CONTRACT,    100);

  teleinfo_config.log_policy = UfsCfgLoadKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_LOG_POLICY, TELEINFO_LOG_BUFFERED);
  teleinfo_config.log_nbday  = UfsCfgLoadKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_LOG_DAY,    TELEINFO_FILE_DAILY);
  teleinfo_config.log_nbweek = UfsCfgLoadKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_LOG_WEEK,   TELEINFO_FILE_WEEKLY);

  teleinfo_config.max_va        = UfsCfgLoadKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_MAX_VA,        TELEINFO_GRAPH_DEF_POWER);
  teleinfo_config.max_v         = UfsCfgLoadKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_MAX_V,         TELEINFO_GRAPH_DEF_VOLTAGE);
  teleinfo_config.max_kwh_hour  = UfsCfgLoadKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_MAX_KWH_HOUR,  TELEINFO_GRAPH_DEF_WH_HOUR);
  teleinfo_config.max_kwh_day   = UfsCfgLoadKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_MAX_KWH_DAY,   TELEINFO_GRAPH_DEF_WH_DAY);
  teleinfo_config.max_kwh_month = UfsCfgLoadKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_MAX_KWH_MONTH, TELEINFO_GRAPH_DEF_WH_MONTH);

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Config loaded from LittleFS"));

#else

  // retrieve saved settings from flash memory
  teleinfo_config.baud_rate       = TeleinfoValidateBaudRate (Settings->sbaudrate * 300);
  teleinfo_config.msg_policy      = (int)Settings->weight_reference;
  teleinfo_config.msg_type        = (int)Settings->weight_calibration;
  teleinfo_config.rom_update      = (int)Settings->weight_item;
  teleinfo_config.contract_percent = (int)Settings->weight_change;

  teleinfo_config.max_va        = TELEINFO_GRAPH_DEF_POWER;
  teleinfo_config.max_v         = TELEINFO_GRAPH_DEF_VOLTAGE;
  teleinfo_config.max_kwh_hour  = TELEINFO_GRAPH_DEF_WH_HOUR;
  teleinfo_config.max_kwh_day   = TELEINFO_GRAPH_DEF_WH_DAY;
  teleinfo_config.max_kwh_month = TELEINFO_GRAPH_DEF_WH_MONTH;

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Config loaded from flash memory"));

# endif     // USE_UFILESYS

  // validate boundaries
  if (teleinfo_config.log_policy >= TELEINFO_LOG_MAX) teleinfo_config.log_policy = TELEINFO_LOG_BUFFERED;
  if ((teleinfo_config.msg_policy < 0) || (teleinfo_config.msg_policy >= TELEINFO_POLICY_MAX)) teleinfo_config.msg_policy = TELEINFO_POLICY_TELEMETRY;
  if (teleinfo_config.msg_type >= TELEINFO_MSG_MAX) teleinfo_config.msg_type = TELEINFO_MSG_METER;
  if ((teleinfo_config.rom_update < TELEINFO_INTERVAL_MIN) || (teleinfo_config.rom_update > TELEINFO_INTERVAL_MAX)) teleinfo_config.rom_update = TELEINFO_INTERVAL_MAX;
  if ((teleinfo_config.contract_percent < TELEINFO_PERCENT_MIN) || (teleinfo_config.contract_percent > TELEINFO_PERCENT_MAX)) teleinfo_config.contract_percent = 100;
}

// Save configuration to Settings or to LittleFS
void TeleinfoSaveConfig () 
{
#ifdef USE_UFILESYS

  // save settings into flash filesystem
  UfsCfgSaveKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_RATE,       teleinfo_config.baud_rate,  true);
  UfsCfgSaveKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_MSG_POLICY, teleinfo_config.msg_policy, false);
  UfsCfgSaveKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_MSG_TYPE,   teleinfo_config.msg_type,   false);
  UfsCfgSaveKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_ROM_UPDATE, teleinfo_config.rom_update, false);
  UfsCfgSaveKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_CONTRACT,    teleinfo_config.contract_percent, false);

  UfsCfgSaveKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_LOG_POLICY, teleinfo_config.log_policy, false);
  UfsCfgSaveKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_LOG_DAY,    teleinfo_config.log_nbday,  false);
  UfsCfgSaveKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_LOG_WEEK,   teleinfo_config.log_nbweek, false);

  UfsCfgSaveKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_MAX_VA,        teleinfo_config.max_va,        false);
  UfsCfgSaveKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_MAX_V,         teleinfo_config.max_v,         false);
  UfsCfgSaveKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_MAX_KWH_HOUR,  teleinfo_config.max_kwh_hour,  false);
  UfsCfgSaveKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_MAX_KWH_DAY,   teleinfo_config.max_kwh_day,   false);
  UfsCfgSaveKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_MAX_KWH_MONTH, teleinfo_config.max_kwh_month, false);

# else

  // save serial port settings
  Settings->sbaudrate = teleinfo_config.baud_rate / 300;

  // save teleinfo settings
  Settings->weight_reference   = (ulong)teleinfo_config.msg_policy;
  Settings->weight_calibration = (ulong)teleinfo_config.msg_type;
  Settings->weight_item        = (ulong)teleinfo_config.rom_update;
  Settings->weight_change      = (uint8_t) teleinfo_config.contract_percent;

# endif     // USE_UFILESYS
}

// calculate line checksum
char TeleinfoCalculateChecksum (const char* pstr_line) 
{
  int     index, line_size;
  uint8_t line_checksum  = 0;
  uint8_t given_checksum = 0;

  // if given line exists
  if (pstr_line == nullptr) return 0;

  // get given checksum
  line_size = strlen (pstr_line) - 1;
  if (line_size > 0) given_checksum = (uint8_t)pstr_line[line_size];

  // adjust checksum calculation according to mode
  if (teleinfo_meter.sep_line == ' ') line_size--;

  // loop to calculate checksum
  for (index = 0; index < line_size; index ++) line_checksum += (uint8_t)pstr_line[index];

  // keep 6 lower bits and add Ox20 and compare to given checksum
  line_checksum = (line_checksum & 0x3F) + 0x20;

  // reset if different than given checksum
  if (line_checksum != given_checksum)
  {
    // increase checksum error counter and log
    teleinfo_meter.nb_error++;
    AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: Error %s [%c]"), pstr_line, line_checksum);

    // reset checksum
    line_checksum = 0;
  } 

  return line_checksum;
}

// update global counter
void TeleinfoUpdateGlobalCounter (const char* str_value, uint8_t index)
{
  long long value;

  // check parameter
  if (str_value == nullptr) return;

  // calculate and update
  value = atoll (str_value);
  if ((value < LONG_LONG_MAX) && (value > teleinfo_meter.index_wh[index])) teleinfo_meter.index_wh[index] = value;
}

// update phase voltage
void TeleinfoUpdateVoltage (const char* str_value, const uint8_t phase, const bool is_rms)
{
  long value;

  // check parameter
  if (str_value == nullptr) return;

  // calculate and update
  value = atol (str_value);
  if (value > TELEINFO_VOLTAGE_MAXIMUM) value = LONG_MAX;

  // if value is valid
  if ((value > 0) && (value != LONG_MAX))
  {
    // rms voltage
    if (is_rms)
    {
      teleinfo_phase[phase].volt_set = true;
      teleinfo_phase[phase].voltage = value;
    }

    // average voltage
    else if (!teleinfo_phase[phase].volt_set) teleinfo_phase[phase].voltage = value;
  }
}

// update phase current
void TeleinfoUpdateCurrent (const char* str_value, const uint8_t phase)
{
  long value;

  // check parameter
  if (str_value == nullptr) return;

  // calculate and update
  value = atol (str_value);
  if ((value >= 0) && (value != LONG_MAX)) teleinfo_phase[phase].current = value; 
}

void TeleinfoUpdateApparentPower (const char* str_value)
{
  long value;

  value = atol (str_value);
  if ((value >= 0) && (value != LONG_MAX)) teleinfo_meter.papp = value;
}

// Split line into etiquette and donnee
void TeleinfoSplitLine (char* pstr_line, char* pstr_etiquette, const int size_etiquette, char* pstr_donnee, const int size_donnee)
{
  int  index, length;
  char *pstr_token;

  // if strings are not given
  if ((pstr_line == nullptr) || (pstr_etiquette == nullptr) || (pstr_donnee == nullptr)) return;

  // check line minimum size and remove checksum
  length = strlen (pstr_line) - 2;
  if (length < 0) return;
  *(pstr_line + length) = 0;

  // relace all TABS with SPACE
  pstr_token = strchr (pstr_line, 9);
  while (pstr_token != nullptr)
  {
    *pstr_token = ' ';
    pstr_token = strchr (pstr_line, 9);
  }

  // init strings
  strcpy (pstr_etiquette, "");
  strcpy (pstr_donnee, "");

  // extract line parts with separator
  index = 0;
  pstr_token = strtok (pstr_line, " ");
  while (pstr_token != nullptr)
  {
    // extract etiquette
    if (index == 0) strlcpy (pstr_etiquette, pstr_token, size_etiquette);

    // else append to donnee
    else
    {
      if (pstr_donnee[0] != 0) strlcat (pstr_donnee, " ", size_donnee);
      strlcat (pstr_donnee, pstr_token, size_donnee);
    }

    // switch to next token
    pstr_token = strtok (nullptr, " ");
    index++;
  }
}

// Display any value to a string with unit and kilo conversion with number of digits (12000, 2.0k, 12.0k, 2.3M, ...)
void TeleinfoDisplayValue (const int unit_type, const long value, char* pstr_result, const int size_result, bool in_kilo) 
{
  float result;
  char  str_text[16];

  // check parameters
  if (pstr_result == nullptr) return;

  // convert cos phi value
  if (unit_type == TELEINFO_UNIT_COSPHI)
  {
    result = (float)value / 100;
    ext_snprintf_P (pstr_result, size_result, PSTR ("%2_f"), &result);
  }

  // else convert values in M with 1 digit
  else if (in_kilo && (value > 999999))
  {
    result = (float)value / 1000000;
    ext_snprintf_P (pstr_result, size_result, PSTR ("%1_fM"), &result);
  }

  // else convert values in k with no digit
  else if (in_kilo && (value > 9999))
  {
    result = (float)value / 1000;
    ext_snprintf_P (pstr_result, size_result, PSTR ("%0_fk"), &result);
  }

  // else convert values in k
  else if (in_kilo && (value > 999))
  {
    result = (float)value / 1000;
    ext_snprintf_P (pstr_result, size_result, PSTR ("%1_fk"), &result);
  }

  // else convert value
  else sprintf_P (pstr_result, PSTR ("%d"), value);

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

// Generate current counter values as a string with unit and kilo conversion
void TeleinfoDisplayCurrentValue (const int unit_type, const int phase, char* pstr_result, const int size_result) 
{
  long value = LONG_MAX;

  // set curve value according to displayed data
  switch (unit_type) 
  {
    case TELEINFO_UNIT_W:
      value = teleinfo_phase[phase].pact;
      break;
    case TELEINFO_UNIT_VA:
      value = teleinfo_phase[phase].papp;
      break;
    case TELEINFO_UNIT_V:
      value = teleinfo_phase[phase].voltage;
      break;
    case TELEINFO_UNIT_COSPHI:
      value = (long)teleinfo_phase[phase].cosphi;
      break;
  }

  // convert value for display
  TeleinfoDisplayValue (unit_type, value, pstr_result, size_result, false);
}

/*********************************************\
 *               Historisation
\*********************************************/

#ifdef USE_UFILESYS

// Get historisation period start time
uint32_t TeleinfoHistoGetStartTime (const uint8_t period, const uint8_t histo)
{
  uint32_t start_time, delta_time;
  TIME_T   start_dst;

  // start date
  start_time = LocalTime ();
  if (period == TELEINFO_PERIOD_DAY) start_time -= 86400 * (uint32_t)histo;
  else if (period == TELEINFO_PERIOD_WEEK) start_time -= 604800 * (uint32_t)histo;

  // set to beginning of current day
  BreakTime (start_time, start_dst);
  start_dst.hour   = 0;
  start_dst.minute = 0;
  start_dst.second = 0;

  // convert back to localtime
  start_time = MakeTime (start_dst);

  // if weekly period, start from monday
  if (period == TELEINFO_PERIOD_WEEK)
  {
    delta_time = ((start_dst.day_of_week + 7) - 2) % 7;
    start_time -= delta_time * 86400;
  }

  return start_time;
}

// Get historisation period literal date
bool TeleinfoHistoGetDate (const int period, const int histo, char* pstr_text)
{
  uint32_t calc_time, year;
  TIME_T   start_dst, stop_dst;

  // check parameters and init
  if (pstr_text == nullptr) return false;
  strcpy (pstr_text, "");

  // start date
  calc_time = TeleinfoHistoGetStartTime (period, histo);
  BreakTime (calc_time, start_dst);

  // generate time label for day graph
  if (period == TELEINFO_PERIOD_DAY) sprintf_P (pstr_text, PSTR ("%02u/%02u"), start_dst.day_of_month, start_dst.month);

  // generate time label for week graph
  else if (period == TELEINFO_PERIOD_WEEK)
  {
    BreakTime (calc_time + 518400, stop_dst);        // end time
    sprintf_P (pstr_text, PSTR ("%02u-%02u/%02u"), start_dst.day_of_month, stop_dst.day_of_month, stop_dst.month);
  }

  // generate time label for year graph
  else if (period == TELEINFO_PERIOD_YEAR)
  {
    // end date
    year = 1970 + start_dst.year - histo;
    sprintf_P (pstr_text, PSTR ("%04u"), year);
  }

  return (strlen (pstr_text) > 0);
}

// Get historisation filename
bool TeleinfoHistoGetFilename (const uint8_t period, const uint8_t histo, char* pstr_filename)
{
  bool exists = false;

  // check parameters
  if (pstr_filename == nullptr) return false;

  // generate filename according to period
  strcpy (pstr_filename, "");
  if (period == TELEINFO_PERIOD_DAY) sprintf_P (pstr_filename, D_TELEINFO_HISTO_FILE_DAY, histo);
  else if (period == TELEINFO_PERIOD_WEEK) sprintf_P (pstr_filename, D_TELEINFO_HISTO_FILE_WEEK, histo);
  else if (period == TELEINFO_PERIOD_YEAR) sprintf_P (pstr_filename, D_TELEINFO_HISTO_FILE_YEAR, RtcTime.year - histo);

  // if filename defined, check existence
  if (strlen (pstr_filename) > 0) exists = ffsp->exists (pstr_filename);

  return exists;
}

// Flush log data to littlefs
void TeleinfoFlushHistoData (const uint8_t period)
{
  uint8_t phase, index;
  char    str_value[32];
  String  str_header;
  File    file;

  // validate parameters
  if ((period != TELEINFO_PERIOD_DAY) && (period != TELEINFO_PERIOD_WEEK)) return;
  if ((teleinfo_contract.period < 0) || (teleinfo_contract.period >= TIC_PERIOD_MAX)) return;

  // if buffer is filled, save buffer to log
  if (teleinfo_period[period].str_buffer.length () > 0)
  {
    // if file doesn't exist, create it and append header
    if (!ffsp->exists (teleinfo_period[period].str_filename.c_str ()))
    {
      // create file
      file = ffsp->open (teleinfo_period[period].str_filename.c_str (), "w");

      // create header
      str_header = "Idx;Date;Time";
      for (phase = 0; phase < teleinfo_contract.phase; phase++)
      {
        sprintf_P (str_value, PSTR (";VA%d;W%d;V%d;C%d;pVA%d;pV%d"), phase + 1, phase + 1, phase + 1, phase + 1, phase + 1, phase + 1);
        str_header += str_value;
      }

      // append contract period and totals
      str_header += ";Period";
      for (index = ARR_TELEINFO_PERIOD_FIRST[teleinfo_contract.period]; index < ARR_TELEINFO_PERIOD_FIRST[teleinfo_contract.period] + teleinfo_contract.nb_indexes; index++)
      {
        GetTextIndexed (str_value, sizeof (str_value), index, kTeleinfoPeriod);
        str_header += ";";
        str_header += str_value;
      }
 
      // write header
      str_header += "\n";
      file.print (str_header.c_str ());
    }

    // else, file exists, open in append mode
    else file = ffsp->open (teleinfo_period[period].str_filename.c_str (), "a");

    // write data in buffer and empty buffer
    file.print (teleinfo_period[period].str_buffer.c_str ());
    teleinfo_period[period].str_buffer = "";

    // close file
    file.close ();
  }

  // set next flush time
  teleinfo_period[period].time_flush = LocalTime () + ARR_TELEINFO_PERIOD_FLUSH[period];
}

// Save historisation data
void TeleinfoSaveHistoData (const uint8_t period, const uint8_t log_policy)
{
  uint8_t  phase;
  long     year, month, remain;
  uint32_t start_time, current_time, day_of_week, index;
  TIME_T   time_dst;
  char     str_value[32];
  char     str_filename[UFS_FILENAME_SIZE];

  // check boundaries
  if ((period != TELEINFO_PERIOD_DAY) && (period != TELEINFO_PERIOD_WEEK)) return;

  // extract current time data
  current_time = LocalTime ();
  BreakTime (current_time, time_dst);

  // if saving daily record
  if (period == TELEINFO_PERIOD_DAY)
  {
    // set current weekly file name
    sprintf_P (str_filename, D_TELEINFO_HISTO_FILE_DAY, 0);

    // calculate index of daily line
    index = (time_dst.hour * 3600 + time_dst.minute * 60) * TELEINFO_GRAPH_SAMPLE / 86400;
  }

  // else if saving weekly record
  else if (period == TELEINFO_PERIOD_WEEK)
  {
    // set current weekly file name
    sprintf_P (str_filename, D_TELEINFO_HISTO_FILE_WEEK, 0);

    // calculate start of current week
    time_dst.second = 1;
    time_dst.minute = 0;
    time_dst.hour   = 0;  
    if (time_dst.day_of_week == 1) day_of_week = 8; else day_of_week = time_dst.day_of_week;
    start_time = MakeTime (time_dst) - 86400 * (day_of_week - 2);

    // calculate index of weekly line
    index = (current_time - start_time) * TELEINFO_GRAPH_SAMPLE / 604800;
    if (index >= TELEINFO_GRAPH_SAMPLE) index = TELEINFO_GRAPH_SAMPLE - 1;
  }

  // if log file name has changed, flush data of previous file
  if ((teleinfo_period[period].str_filename != "") && (teleinfo_period[period].str_filename != str_filename)) TeleinfoFlushHistoData (period);

  // set new log filename
  teleinfo_period[period].str_filename = str_filename;

  // line : index and date
  teleinfo_period[period].str_buffer += index;
  sprintf_P (str_value, PSTR (";%02u/%02u;%02u:%02u"), time_dst.day_of_month, time_dst.month, time_dst.hour, time_dst.minute);
  teleinfo_period[period].str_buffer += str_value;

  // line : phase data
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    // apparent power
    teleinfo_period[period].str_buffer += ";";
    if (teleinfo_graph.papp[phase] != LONG_MAX) teleinfo_period[period].str_buffer += teleinfo_graph.papp[phase];

    // active power
    teleinfo_period[period].str_buffer += ";";
    if (teleinfo_graph.pact[phase] != LONG_MAX) teleinfo_period[period].str_buffer += teleinfo_graph.pact[phase];

    // lower voltage
    teleinfo_period[period].str_buffer += ";";
    teleinfo_period[period].str_buffer += teleinfo_graph.volt_low[phase];

    // cos phi
    teleinfo_period[period].str_buffer += ";";
    teleinfo_period[period].str_buffer += teleinfo_graph.cosphi[phase];

    // peak apparent power
    teleinfo_period[period].str_buffer += ";";
    if (teleinfo_graph.papp_peak[phase] != LONG_MAX) teleinfo_period[period].str_buffer += teleinfo_graph.papp_peak[phase];

    // peak voltage
    teleinfo_period[period].str_buffer += ";";
    teleinfo_period[period].str_buffer += teleinfo_graph.volt_peak[phase];
  }

  // line : totals
  GetTextIndexed (str_value, sizeof (str_value), teleinfo_contract.period, kTeleinfoPeriod);
  teleinfo_period[period].str_buffer += ";";
  teleinfo_period[period].str_buffer += str_value;
  for (index = 0; index < teleinfo_contract.nb_indexes; index++)
  {
    lltoa (teleinfo_meter.index_wh[index], str_value, 10); 
    teleinfo_period[period].str_buffer += ";";
    teleinfo_period[period].str_buffer += str_value;
  }

  // line : end
  teleinfo_period[period].str_buffer += "\n";

  // if log should be saved now
  if ((log_policy == TELEINFO_LOG_IMMEDIATE) || (teleinfo_period[period].str_buffer.length () > TELEINFO_HISTO_BUFFER_MAX)) TeleinfoFlushHistoData (period);
}


// Rotation of log files
void TeleinfoRotateLog ()
{
  TIME_T time_dst;

  // log default method
  AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Rotate log files"));

  // flush daily and weekly records
  TeleinfoFlushHistoData ( TELEINFO_PERIOD_DAY);
  TeleinfoFlushHistoData ( TELEINFO_PERIOD_WEEK);

  // rotate daily files
  UfsFileRotate (D_TELEINFO_HISTO_FILE_DAY, 0, teleinfo_config.log_nbday);

  // if we are monday, week has changed, rotate previous weekly files
  BreakTime (LocalTime () - 60, time_dst);
  if (time_dst.day_of_week == 1) UfsFileRotate (D_TELEINFO_HISTO_FILE_WEEK, 0, teleinfo_config.log_nbweek);
}

// Save historisation data
void TeleinfoSaveDailyTotal ()
{
  uint8_t   index;
  uint32_t  current_time;
  TIME_T    today_dst;
  long long delta;
  char      str_value[32];
  char      str_filename[UFS_FILENAME_SIZE];
  String    str_line;
  File      file;

  // calculate today's filename (shift 5 sec in case of sligth midnight call delay)
  current_time = LocalTime () - 5;
  BreakTime (current_time, today_dst);

  // if daily total has been updated and date is defined
  if ((teleinfo_meter.total_wh != 0) && (today_dst.year != 0))
  {
    // calculate daily ahd hourly delta
    delta = teleinfo_meter.total_wh - teleinfo_meter.day_last_wh;
    teleinfo_meter.hour_wh[RtcTime.hour] += (long)(teleinfo_meter.total_wh - teleinfo_meter.hour_last_wh);

    // update last day and hour total
    teleinfo_meter.day_last_wh = teleinfo_meter.total_wh;
    teleinfo_meter.hour_last_wh = teleinfo_meter.total_wh;

    // get filename
    sprintf_P (str_filename, D_TELEINFO_HISTO_FILE_YEAR, 1970 + today_dst.year);

    // if file exists, open in append mode
    if (ffsp->exists (str_filename)) file = ffsp->open (str_filename, "a");

    // else open in creation mode
    else
    {
      file = ffsp->open (str_filename, "w");

      // generate header for daily sum
      str_line = "Idx;Month;Day;Global;Daily";
      for (index = 0; index < 24; index ++)
      {
        sprintf_P (str_value, ";%02uh", index);
        str_line += str_value;
      }
      str_line += "\n";
      file.print (str_line.c_str ());
    }

    // generate today's line
    sprintf (str_value, "%u;%u;%u", today_dst.day_of_year, today_dst.month, today_dst.day_of_month);
    str_line = str_value;
    lltoa (teleinfo_meter.total_wh, str_value, 10);
    str_line += ";";
    str_line += str_value;
    if (delta != LONG_LONG_MAX) lltoa (delta, str_value, 10);
      else strcpy (str_value, "0");
    str_line += ";";
    str_line += str_value;

    // loop to add hourly totals
    for (index = 0; index < 24; index ++)
    {
      // append hourly increment to line
      str_line += ";";
      str_line += teleinfo_meter.hour_wh[index];

      // reset hourly increment
      teleinfo_meter.hour_wh[index] = 0;
    }

    // write line and close file
    str_line += "\n";
    file.print (str_line.c_str ());
    file.close ();
  }
}
#endif    // USE_UFILESYS

/*********************************************\
 *                   Graph
\*********************************************/

// Save current values to graph data
void TeleinfoSavePeriodData (const uint8_t period)
{
  uint8_t phase;
  long    power;

  // if period out of range, return
  if (period >= TELEINFO_PERIOD_MAX) return;

  // if period other than yearly, loop thru phases
  if (period < TELEINFO_PERIOD_YEAR)
    for (phase = 0; phase < teleinfo_contract.phase; phase++)
    {
      //   Apparent power
      // -----------------

      // save average and peak value over the period
      power = LONG_MAX;
      if (teleinfo_contract.ssousc > 0) power = (long)(teleinfo_period[period].papp_sum[phase] / ARR_TELEINFO_PERIOD_WINDOW[period]);
      teleinfo_graph.papp[phase] = power;
      teleinfo_graph.papp_peak[phase] = teleinfo_period[period].papp_peak[phase];

      // reset period data
      teleinfo_period[period].papp_sum[phase]  = 0;
      teleinfo_period[period].papp_peak[phase] = 0;

      //   Active power
      // -----------------

      // save average value over the period
      power = LONG_MAX;
      if (teleinfo_contract.ssousc > 0) power = (long)(teleinfo_period[period].pact_sum[phase] / ARR_TELEINFO_PERIOD_WINDOW[period]);
      if (power > teleinfo_graph.papp[phase]) power = teleinfo_graph.papp[phase];
      teleinfo_graph.pact[phase] = power;

      // reset period data
      teleinfo_period[period].pact_sum[phase] = 0;

      //   Voltage
      // -----------

      // save graph current value
      teleinfo_graph.volt_low[phase]  = teleinfo_period[period].volt_low[phase];
      teleinfo_graph.volt_peak[phase] = teleinfo_period[period].volt_peak[phase];

      // reset period data
      teleinfo_period[period].volt_low[phase]  = LONG_MAX;
      teleinfo_period[period].volt_peak[phase] = 0;

      //   CosPhi
      // -----------

      // save average value over the period
      teleinfo_graph.cosphi[phase] = (int)(teleinfo_period[period].cosphi_sum[phase] / ARR_TELEINFO_PERIOD_WINDOW[period]);

      // reset period _
      teleinfo_period[period].cosphi_sum[phase] = 0;
    }

  // save to memory array
  TeleinfoSaveMemoryData (period);

#ifdef USE_UFILESYS
  // save to historisation file
  TeleinfoSaveHistoData (period, teleinfo_config.log_policy);
#endif    // USE_UFILESYS

  // data updated for period
  teleinfo_period[period].updated = true;
}

// Save current values to graph data
void TeleinfoSaveMemoryData (const uint8_t period)
{
  uint8_t  phase;
  uint16_t cell_index;
  long     value;

  // check parameters
  if (period != TELEINFO_PERIOD_LIVE) return;

  // set array index and current index of cell in memory array
  cell_index = teleinfo_period[period].index;

  // loop thru phases
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    // apparent power
    value = UINT8_MAX;
    if (teleinfo_contract.ssousc > 0) value = teleinfo_graph.papp[phase] * 200 / teleinfo_contract.ssousc;
    teleinfo_graph.data_live.arr_papp[phase][cell_index] = (uint8_t)value;

    // active power
    value = UINT8_MAX;
    if (teleinfo_contract.ssousc > 0) value = teleinfo_graph.pact[phase] * 200 / teleinfo_contract.ssousc;
    teleinfo_graph.data_live.arr_pact[phase][cell_index] = (uint8_t)value;

    // voltage
    value = 128 + teleinfo_graph.volt_low[phase] - TELEINFO_VOLTAGE;
    teleinfo_graph.data_live.arr_volt[phase][cell_index] = (uint8_t)value;

    // cos phi
    teleinfo_graph.data_live.arr_cosphi[phase][cell_index] = (uint8_t)teleinfo_graph.cosphi[phase];
  }

  // increase data index in the graph and set update flag
  cell_index++;
  teleinfo_period[period].index = cell_index % TELEINFO_GRAPH_SAMPLE;
}

/*********************************************\
 *                   Callback
\*********************************************/

// Teleinfo GPIO initilisation
void TeleinfoPreInit ()
{
  // if TeleinfoEN pin defined, switch it OFF
  if (PinUsed (GPIO_TELEINFO_ENABLE))
  {
    // switch off GPIO_TELEINFO_ENABLE to avoid ESP32 Rx heavy load restart bug
    pinMode (Pin (GPIO_TELEINFO_ENABLE, 0), OUTPUT);

    // disable serial input
    TeleinfoDisableSerial ();
  }

  // if TeleinfoRX pin defined, set energy driver as teleinfo
  if (!TasmotaGlobal.energy_driver && PinUsed (GPIO_TELEINFO_RX))
  {
    // set GPIO_TELEINFO_RX as digital input
    pinMode (Pin (GPIO_TELEINFO_RX, 0), INPUT);

    // declare energy driver
    TasmotaGlobal.energy_driver = XNRG_15;

    // log
    AddLog (LOG_LEVEL_INFO, PSTR("TIC: Teleinfo driver enabled"));
  }
}

// Teleinfo driver initialisation
void TeleinfoInit ()
{
  int index, phase;

#ifdef USE_UFILESYS
  // log result
  if (ufs_type) AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Partition mounted"));
  else AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Partition could not be mounted"));
#endif  // USE_UFILESYS

  // disable fast cycle power recovery (SetOption65)
  Settings->flag3.fast_power_cycle_disable = true;

  // init hardware energy counter
  Settings->flag3.hardware_energy_total = true;

  // set default energy parameters
  Energy.voltage_available = true;
  Energy.current_available = true;

  // load configuration
  TeleinfoLoadConfig ();

  // initialise message timestamp
  teleinfo_message.timestamp = UINT32_MAX;

  // loop thru all possible phases
  for (phase = 0; phase < ENERGY_MAX_PHASES; phase++)
  {
    // reset total counter
    Settings->energy_kWhtotal_ph[phase] = 0;

    // tasmota energy counters
    Energy.voltage[phase]           = TELEINFO_VOLTAGE;
    Energy.current[phase]           = 0;
    Energy.active_power[phase]      = 0;
    Energy.apparent_power[phase]    = 0;

    // initialise phase data
    teleinfo_phase[phase].volt_set  = false;
    teleinfo_phase[phase].voltage   = TELEINFO_VOLTAGE;
    teleinfo_phase[phase].current   = 0;
    teleinfo_phase[phase].papp      = 0;
    teleinfo_phase[phase].pact      = 0;
    teleinfo_phase[phase].papp_last = 0;
    teleinfo_phase[phase].cosphi    = 100;

    // initialise power calculation structure
    teleinfo_calc.papp_inc[phase]   = 0;

    // data of historisation files
    teleinfo_graph.papp[phase]      = 0;
    teleinfo_graph.pact[phase]      = 0;
    teleinfo_graph.volt_low[phase]  = LONG_MAX;
    teleinfo_graph.volt_peak[phase] = 0;
    teleinfo_graph.cosphi[phase]    = 100;
  }

  // init all meter indexes
  for (index = 0; index < TELEINFO_INDEX_MAX; index ++) teleinfo_meter.index_wh[index] = 0;

  // init hourly data
  for (index = 0; index < 24; index ++) teleinfo_meter.hour_wh[index] = 0;

  // disable all message lines
  for (index = 0; index < TELEINFO_LINE_QTY; index ++) teleinfo_message.line[index].checksum = 0;

  // log default method
  AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Using default Global Counter method"));

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: tic_help to get help on Teleinfo TIC commands"));
}

// Teleinfo graph data initialisation
void TeleinfoGraphInit ()
{
  uint32_t period, phase, index;

  // boundaries of SVG graph
  teleinfo_graph.left  = TELEINFO_GRAPH_PERCENT_START * TELEINFO_GRAPH_WIDTH / 100;
  teleinfo_graph.right = TELEINFO_GRAPH_PERCENT_STOP  * TELEINFO_GRAPH_WIDTH / 100;
  teleinfo_graph.width = teleinfo_graph.right - teleinfo_graph.left;

  // initialise period data
  for (period = 0; period < TELEINFO_PERIOD_MAX; period++)
  {
    // init counter
    teleinfo_period[period].index    = 0;
    teleinfo_period[period].counter  = 0;

    // loop thru phase
    for (phase = 0; phase < ENERGY_MAX_PHASES; phase++)
    {
      // init max power per period
      teleinfo_period[period].papp_peak[phase]  = 0;
      teleinfo_period[period].volt_low[phase]   = LONG_MAX;
      teleinfo_period[period].volt_peak[phase]  = 0;
      teleinfo_period[period].papp_sum[phase]   = 0;
      teleinfo_period[period].pact_sum[phase]   = 0;
      teleinfo_period[period].cosphi_sum[phase] = 0;
    }

    // reset initial log flush time
    teleinfo_period[period].time_flush = UINT32_MAX;
  }

  // initialise graph data
  strcpy (teleinfo_graph.str_phase, "");
  for (phase = 0; phase < ENERGY_MAX_PHASES; phase++)
  {
    // loop thru graph values
    for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
    {
      teleinfo_graph.data_live.arr_pact[phase][index]   = UINT8_MAX;
      teleinfo_graph.data_live.arr_papp[phase][index]   = UINT8_MAX;
      teleinfo_graph.data_live.arr_volt[phase][index]   = UINT8_MAX;
      teleinfo_graph.data_live.arr_cosphi[phase][index] = UINT8_MAX;
    } 
  }
}

// Save data before ESP restart
void TeleinfoSaveBeforeRestart ()
{
  // stop serial reception and disable Teleinfo Rx
  TeleinfoDisableSerial ();

  // update energy total (in kwh)
  EnergyUpdateTotal ();

#ifdef USE_UFILESYS
  // save configuration
  TeleinfoSaveConfig ();
  
  // flush logs
  TeleinfoFlushHistoData ( TELEINFO_PERIOD_DAY);
  TeleinfoFlushHistoData ( TELEINFO_PERIOD_WEEK);

  // save daily total
  TeleinfoSaveDailyTotal ();
#endif      // USE_UFILESYS
}


// Handling of received teleinfo data
void TeleinfoReceiveData ()
{
  bool      is_valid;
  uint8_t   phase;
  int       period, index;
  long      value, total_current, increment;
  long long counter, counter_wh, delta_wh;
  uint32_t  timeout, timestamp, message_ms;
  float     cosphi, papp_inc;
  char*     pstr_match;
  char      checksum, byte_recv;
  char      str_etiquette[TELEINFO_KEY_MAX];
  char      str_donnee[TELEINFO_DATA_MAX];
  char      str_text[TELEINFO_DATA_MAX];

  // serial receive loop
  while (teleinfo_serial->available()) 
//  if (teleinfo_serial->available()) 
  {
    // set timestamp
    timestamp = millis ();

    // read caracter
    byte_recv = (char)teleinfo_serial->read (); 
    teleinfo_message.length++;

    // handle caracter
    switch (byte_recv)
    {
      // ---------------------------
      // 0x02 : Beginning of message 
      // ---------------------------

      case 0x02:
        // reset current message line index
        teleinfo_message.line_index = 0;

        // initialise message length
        teleinfo_message.length = 1;

        // reset voltage flags
        for (phase = 0; phase < teleinfo_contract.phase; phase++) teleinfo_phase[phase].volt_set = false;
        break;
          
      // ---------------------------
      // 0x04 : Reset of message 
      // ---------------------------

      case 0x04:
        // increment reset counter and reset message line index
        teleinfo_meter.nb_reset++;
        teleinfo_message.line_index = 0;

        // log
        AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Message reset"));
        break;

      // -----------------------------
      // 0x0A : Line - Beginning
      // 0x0E : On some faulty meters
      // -----------------------------

      case 0x0A:
      case 0x0E:
        // init current line
        teleinfo_meter.idx_line = 0;
        break;

      // ------------------------------
      // \t or SPACE : Line - New part
      // ------------------------------

      case 0x09:
      case ' ':
        if (teleinfo_meter.sep_line == 0) teleinfo_meter.sep_line = byte_recv;
        if (teleinfo_meter.idx_line < sizeof (teleinfo_meter.str_line) - 1) teleinfo_meter.str_line[teleinfo_meter.idx_line++] = byte_recv;
        break;
        
      // ------------------
      // 0x0D : Line - End
      // ------------------

      case 0x0D:
        // increment counter
        teleinfo_meter.nb_line++;

        // drop first message
        if (teleinfo_meter.nb_line > 1)
        {
          // if checksum is ok, handle the line
          teleinfo_meter.str_line[teleinfo_meter.idx_line] = 0;
          checksum = TeleinfoCalculateChecksum (teleinfo_meter.str_line);
          if (checksum != 0)
          {
            // init
            strcpy (str_etiquette, "");
            strcpy (str_donnee, "");
            
            // extract etiquette and donnee from current line
            TeleinfoSplitLine (teleinfo_meter.str_line, str_etiquette, sizeof (str_etiquette), str_donnee, sizeof (str_donnee));

            // get etiquette type
            index = GetCommandCode (str_text, sizeof (str_text), str_etiquette, kTeleinfoEtiquetteName);

            // update data according to etiquette
            switch (index)
            {
              // contract : reference
              case TIC_ADCO:
                teleinfo_contract.mode = TIC_MODE_HISTORIC;
                  if (teleinfo_contract.ident == 0) teleinfo_contract.ident = atoll (str_donnee);
                break;
              case TIC_ADSC:
                teleinfo_contract.mode = TIC_MODE_STANDARD;
                  if (teleinfo_contract.ident == 0) teleinfo_contract.ident = atoll (str_donnee);
                break;
              case TIC_ADS:
                  if (teleinfo_contract.ident == 0) teleinfo_contract.ident = atoll (str_donnee);
                break;

              // contract : mode and type
              case TIC_CONFIG:
                if (strstr (str_donnee, "CONSO") != nullptr)
                {
                  teleinfo_contract.mode = TIC_MODE_PME;
                  teleinfo_contract.type = TIC_TYPE_CONSO;
                }
                else if (strstr (str_donnee, "PROD") != nullptr)
                {
                  teleinfo_contract.mode = TIC_MODE_PME;
                  teleinfo_contract.type = TIC_TYPE_PROD;
                }
                break;
              case TIC_NGTF:
                if (strstr (str_donnee, "PRODUCTEUR") != nullptr) teleinfo_contract.type = TIC_TYPE_PROD;
                break;

              // period name
              case TIC_PTEC:
              case TIC_LTARF:
              case TIC_PTCOUR:
              case TIC_PTCOUR1:
              case TIC_PTCOUR2:
                period = GetCommandCode (str_text, sizeof (str_text), str_donnee, kTeleinfoPeriod);
                if ((period >= 0) && (period < TIC_PERIOD_MAX)) teleinfo_contract.period = period;
                break;

              // instant current
              case TIC_IINST:
              case TIC_IRMS1:
              case TIC_IINST1:
                TeleinfoUpdateCurrent (str_donnee,0);
                break;
              case TIC_IRMS2:
              case TIC_IINST2:
                TeleinfoUpdateCurrent (str_donnee,1);
                break;
              case TIC_IRMS3:
              case TIC_IINST3:
                TeleinfoUpdateCurrent (str_donnee,2);
                teleinfo_contract.phase = 3; 
                break;

              // if in conso mode, instant apparent power, 
              case TIC_PAPP:
              case TIC_SINSTS:
                if (teleinfo_contract.type == TIC_TYPE_CONSO) TeleinfoUpdateApparentPower (str_donnee);
                break;

              // if in prod mode, instant apparent power, 
              case TIC_SINSTI:
                if (teleinfo_contract.type == TIC_TYPE_PROD) TeleinfoUpdateApparentPower (str_donnee);
                break;

              // apparent power counter
              case TIC_EAPPS:
                if (teleinfo_calc.method != TIC_METHOD_INCREMENT) AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Switching to VA/W increment method"));
                teleinfo_calc.method = TIC_METHOD_INCREMENT;
                strlcpy (str_text, str_donnee, sizeof (str_text));
                pstr_match = strchr (str_text, 'V');
                if (pstr_match != nullptr) *pstr_match = 0;
                value = atol (str_text);
                is_valid = ((value >= 0) && (value < LONG_MAX));
                if (is_valid) teleinfo_calc.papp_current_counter = value;
                break;

              // active Power Counter
              case TIC_EAS:
                if (teleinfo_calc.method != TIC_METHOD_INCREMENT) AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Switching to VA/W increment method"));
                teleinfo_calc.method = TIC_METHOD_INCREMENT;
                strlcpy (str_text, str_donnee, sizeof (str_text));
                pstr_match = strchr (str_text, 'W');
                if (pstr_match != nullptr) *pstr_match = 0;
                value = atol (str_text);
                is_valid = (value < LONG_MAX);
                if (is_valid) teleinfo_calc.pact_current_counter = value;
                break;

              // Voltage
              case TIC_URMS1:
                TeleinfoUpdateVoltage (str_donnee, 0, true);
                break;
              case TIC_UMOY1:
                TeleinfoUpdateVoltage (str_donnee, 0, false);
                break;
              case TIC_URMS2:
                TeleinfoUpdateVoltage (str_donnee, 1, true);
                break;
              case TIC_UMOY2:
                TeleinfoUpdateVoltage (str_donnee, 1, false);
                break;
              case TIC_URMS3:
                TeleinfoUpdateVoltage (str_donnee, 2, true);
                teleinfo_contract.phase = 3; 
                break;
              case TIC_UMOY3:
                TeleinfoUpdateVoltage (str_donnee, 2, false);
                teleinfo_contract.phase = 3; 
                break;

              // Contract : Maximum Current
              case TIC_ISOUSC:
                value = atol (str_donnee);
                is_valid = ((value > 0) && (value < LONG_MAX));
                if (is_valid && (teleinfo_contract.isousc != value))
                {
                  // update max current and power
                  teleinfo_contract.isousc = value;
                  teleinfo_contract.ssousc = value * TELEINFO_VOLTAGE_REF;
                  teleinfo_config.max_va   = teleinfo_contract.ssousc;
                }
                break;

              // Contract : Maximum Power (kVA)
              case TIC_PREF:
              case TIC_PCOUP:
                value = atol (str_donnee);
                is_valid = ((value > 0) && (value < LONG_MAX) && (teleinfo_contract.phase > 0));
                if (is_valid)
                {
                  value = value * 1000;
                  if (teleinfo_contract.phase > 1) value = value / teleinfo_contract.phase;
                  if (teleinfo_contract.ssousc != value)
                  {
                    // update max current and power
                    teleinfo_contract.isousc = value / TELEINFO_VOLTAGE_REF;
                    teleinfo_contract.ssousc = value;
                    teleinfo_config.max_va   = value;
                  }                   
                }
                break;
              case TIC_PS:
                strlcpy (str_text, str_donnee, sizeof (str_text));

                // replace xxxk by xxx000
                pstr_match = strchr (str_text, 'k');
                if (pstr_match != nullptr) 
                {
                  *pstr_match = 0;
                  strlcat (str_text, "000", sizeof (str_text));
                }
                value = atol (str_text);
                is_valid = ((value > 0) && (value < LONG_MAX));
                if (is_valid)
                {
                  if (teleinfo_contract.ssousc != value)
                  {
                    // update max current and power
                    teleinfo_contract.isousc = value / TELEINFO_VOLTAGE_REF;
                    teleinfo_contract.ssousc = value;
                    teleinfo_config.max_va   = value;
                  }                   
                }
                break;

              // Contract : Index suivant type de contrat
              case TIC_EAPS:
                strlcpy (str_text, str_donnee, sizeof (str_text));
                pstr_match = strchr (str_text, 'k');
                if (pstr_match != nullptr)
                {
                  *pstr_match = 0;
                  strlcat (str_text, "000", sizeof (str_text));
                }
                if (teleinfo_contract.type == TIC_TYPE_CONSO) TeleinfoUpdateGlobalCounter (str_text, 0);
                break;
              case TIC_BASE:
              case TIC_HCHC:
              case TIC_EJPHN:
              case TIC_BBRHCJB:
              case TIC_EASF01:
                if (teleinfo_contract.type == TIC_TYPE_CONSO) TeleinfoUpdateGlobalCounter (str_donnee, 0);
                break;

              case TIC_EAIT:
                if (teleinfo_contract.type == TIC_TYPE_PROD) TeleinfoUpdateGlobalCounter (str_donnee, 0);
                break;

              case TIC_HCHP:
              case TIC_EJPHPM:
              case TIC_BBRHPJB:
              case TIC_EASF02:
               if (teleinfo_contract.type == TIC_TYPE_CONSO) TeleinfoUpdateGlobalCounter (str_donnee, 1);
                break;
              case TIC_BBRHCJW:
              case TIC_EASF03:
                if (teleinfo_contract.type == TIC_TYPE_CONSO) TeleinfoUpdateGlobalCounter (str_donnee, 2);
                break;
              case TIC_BBRHPJW:
              case TIC_EASF04:
                if (teleinfo_contract.type == TIC_TYPE_CONSO) TeleinfoUpdateGlobalCounter (str_donnee, 3);
                break;
              case TIC_BBRHCJR:
              case TIC_EASF05:
                if (teleinfo_contract.type == TIC_TYPE_CONSO) TeleinfoUpdateGlobalCounter (str_donnee, 4);
                break;
              case TIC_BBRHPJR:
              case TIC_EASF06:
                if (teleinfo_contract.type == TIC_TYPE_CONSO) TeleinfoUpdateGlobalCounter (str_donnee, 5);
                break;
              case TIC_EASF07:
                if (teleinfo_contract.type == TIC_TYPE_CONSO) TeleinfoUpdateGlobalCounter (str_donnee, 6);
                break;
              case TIC_EASF08:
                if (teleinfo_contract.type == TIC_TYPE_CONSO) TeleinfoUpdateGlobalCounter (str_donnee, 7);
                break;
              case TIC_EASF09:
                if (teleinfo_contract.type == TIC_TYPE_CONSO) TeleinfoUpdateGlobalCounter (str_donnee, 8);
               break;
              case TIC_EASF10:
                if (teleinfo_contract.type == TIC_TYPE_CONSO) TeleinfoUpdateGlobalCounter (str_donnee, 9);
                break;

              // Overload
              case TIC_ADPS:
              case TIC_ADIR1:
              case TIC_ADIR2:
              case TIC_ADIR3:
              case TIC_PREAVIS:
                teleinfo_message.overload = true;
                break;
            }

            // if maximum number of lines not reached
            if (teleinfo_message.line_index < TELEINFO_LINE_QTY)
            {
              // get index
              index = teleinfo_message.line_index;

              // save new line
              strlcpy (teleinfo_message.line[index].str_etiquette, str_etiquette, sizeof(teleinfo_message.line[index].str_etiquette));
              strlcpy (teleinfo_message.line[index].str_donnee, str_donnee, sizeof(teleinfo_message.line[index].str_donnee));
              teleinfo_message.line[index].checksum = checksum;

              // increment line index
              teleinfo_message.line_index++;
            }
          }
        }
        break;

      // ---------------------
      // Ox03 : End of message
      // ---------------------

      case 0x03:
        // set message timestamp
        teleinfo_message.timestamp = timestamp;

        // increment message counter
        teleinfo_meter.nb_message++;

        // loop to remove unused message lines
        for (index = teleinfo_message.line_index; index < TELEINFO_LINE_QTY; index++)
        {
          teleinfo_message.line[index].str_etiquette[0] = 0;
          teleinfo_message.line[index].str_donnee[0] = 0;
          teleinfo_message.line[index].checksum = 0;
        }

        // save number of lines and reset index
        teleinfo_message.line_max   = teleinfo_message.line_index;
        teleinfo_message.line_index = 0;

        // if needed, calculate max contract power from max phase current
        if ((teleinfo_contract.ssousc == 0) && (teleinfo_contract.isousc != 0)) teleinfo_contract.ssousc = teleinfo_contract.isousc * TELEINFO_VOLTAGE_REF;

        // ----------------------
        // counter global indexes

        // if not already done, determine number of indexes according to contract
        if ((teleinfo_contract.nb_indexes == -1) && (teleinfo_contract.period >= 0) && (teleinfo_contract.period < TIC_PERIOD_MAX))
        {
          // update number of indexes
          teleinfo_contract.nb_indexes = ARR_TELEINFO_PERIOD_NUMBER[teleinfo_contract.period];

          // log
          GetTextIndexed (str_text, sizeof (str_text), teleinfo_contract.period, kTeleinfoPeriod);
          AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Period %s detected, using %d indexe(s)"), str_text, teleinfo_contract.nb_indexes);
        }

        // if needed, adjust number of phases
        if (Energy.phase_count < teleinfo_contract.phase) Energy.phase_count = teleinfo_contract.phase;

        // calculate total current
        total_current = 0;
        for (phase = 0; phase < teleinfo_contract.phase; phase++) total_current += teleinfo_phase[phase].current;

        // if first measure, init timing
        if (teleinfo_calc.previous_time_message == UINT32_MAX) teleinfo_calc.previous_time_message = teleinfo_message.timestamp;

        // calculate time window
        message_ms = TimeDifference (teleinfo_calc.previous_time_message, teleinfo_message.timestamp);

        // swtich according to calculation method
        switch (teleinfo_calc.method) 
        {
          // -----------------------------------------------
          //   global apparent power is provided
          //   active power calculated from global counter
          // -----------------------------------------------

          case TIC_METHOD_GLOBAL_COUNTER:

            // update total energy counter and set first global counter
            counter_wh = 0;
            for (index = 0; index < teleinfo_contract.nb_indexes; index ++) counter_wh += teleinfo_meter.index_wh[index];
            if (teleinfo_meter.total_wh == 0) teleinfo_meter.total_wh = counter_wh;
            delta_wh = counter_wh - teleinfo_meter.total_wh;

            // slit apparent power between phases, based on current per phase
            papp_inc = 0;
            for (phase = 0; phase < teleinfo_contract.phase; phase++)
            {
              // if current is given, apparent power is split between phase according to current ratio
              if (total_current > 0) teleinfo_phase[phase].papp = teleinfo_meter.papp * teleinfo_phase[phase].current / total_current;

              // else if current is 0, apparent power is equally split between phases
              else if (teleinfo_contract.phase > 0) teleinfo_phase[phase].papp = teleinfo_meter.papp / teleinfo_contract.phase;

              // add apparent power for message time window
              teleinfo_calc.papp_inc[phase] += (float)teleinfo_phase[phase].papp * (float)message_ms / 3600000;
              papp_inc += teleinfo_calc.papp_inc[phase];
            }

            // if global counter increment start is known
            if (teleinfo_calc.previous_time_counter != UINT32_MAX)
            {
              // if global counter has got incremented, calculate new cos phi, averaging with previous one to avoid peaks
              cosphi = teleinfo_phase[0].cosphi;
              if (delta_wh > 0)
              {
                // new cosphi = 3/4 previous one + 1/4 calculated one, if new cosphi > 1, new cosphi = 1
                if (papp_inc > 0) cosphi = 0.75 * cosphi + 0.25 * 100 * (float)delta_wh / papp_inc;
                if (cosphi > 100) cosphi = 100;
                teleinfo_meter.nb_update++;
              }

              // else, if apparent power is above 1, cosphi will be drastically reduced when global counter will be increased
              // so lets apply slow reduction to cosphi according to apparent power current increment
              else if (papp_inc > 1)
              {
                cosphi = 0.5 * cosphi + 0.5 * 100 / papp_inc;
                if (cosphi > teleinfo_phase[0].cosphi) cosphi = teleinfo_phase[0].cosphi;
              }

              // apply cos phi to each phase
              for (phase = 0; phase < teleinfo_contract.phase; phase++) teleinfo_phase[phase].cosphi = cosphi;
            }

            // calculate active power per phase
            for (phase = 0; phase < teleinfo_contract.phase; phase++) teleinfo_phase[phase].pact = teleinfo_phase[phase].papp * (long)teleinfo_phase[phase].cosphi / 100;

            // update global counter
            teleinfo_meter.total_wh = counter_wh;

            // update calculation timestamp
            teleinfo_calc.previous_time_message = teleinfo_message.timestamp;
            if (delta_wh > 0)
            {
              // update calculation timestamp
              teleinfo_calc.previous_time_counter = teleinfo_message.timestamp;

              // reset apparent power sum for the period
              for (phase = 0; phase < teleinfo_contract.phase; phase++) teleinfo_calc.papp_inc[phase] = 0;
            }
          break;

          // -----------------------------------
          //   apparent power and active power
          //   are provided as increment
          // -----------------------------------

          case TIC_METHOD_INCREMENT:
            // apparent power : no increment available
            if (teleinfo_calc.papp_current_counter == LONG_MAX)
            {
              AddLog (LOG_LEVEL_INFO, PSTR ("TIC: %d - VA counter unavailable"), teleinfo_meter.nb_message);
              teleinfo_calc.papp_previous_counter = LONG_MAX;
            }

            // apparent power : no comparison possible
            else if (teleinfo_calc.papp_previous_counter == LONG_MAX)
            {
              teleinfo_calc.papp_previous_counter = teleinfo_calc.papp_current_counter;
            }

            // apparent power : rollback
            else if (teleinfo_calc.papp_current_counter <= teleinfo_calc.papp_previous_counter / 100)
            {
              AddLog (LOG_LEVEL_INFO, PSTR ("TIC: %d - VA counter rollback, start from %d"), teleinfo_meter.nb_message, teleinfo_calc.papp_current_counter);
              teleinfo_calc.papp_previous_counter = teleinfo_calc.papp_current_counter;
            }

            // apparent power : abnormal increment
            else if (teleinfo_calc.papp_current_counter < teleinfo_calc.papp_previous_counter)
            {
              AddLog (LOG_LEVEL_INFO, PSTR ("TIC: %d - VA abnormal value, %d after %d"), teleinfo_meter.nb_message, teleinfo_calc.papp_current_counter, teleinfo_calc.papp_previous_counter);
              teleinfo_calc.papp_previous_counter = LONG_MAX;
            }

            // apparent power : do calculation
            else if (message_ms > 0)
            {
              increment = teleinfo_calc.papp_current_counter - teleinfo_calc.papp_previous_counter;
              teleinfo_phase[0].papp = teleinfo_phase[0].papp / 2 + 3600000 * increment / 2 / (long)message_ms;
              teleinfo_calc.papp_previous_counter = teleinfo_calc.papp_current_counter;
            }

            // active power : no increment available
            if (teleinfo_calc.pact_current_counter == LONG_MAX)
            {
              AddLog (LOG_LEVEL_INFO, PSTR ("TIC: %d - W counter unavailable"), teleinfo_meter.nb_message);
              teleinfo_calc.pact_previous_counter = LONG_MAX;
            }

            // active power : no comparison possible
            else if (teleinfo_calc.pact_previous_counter == LONG_MAX)
            {
              teleinfo_calc.pact_previous_counter = teleinfo_calc.pact_current_counter;
            }

            // active power : detect rollback
            else if (teleinfo_calc.pact_current_counter <= teleinfo_calc.pact_previous_counter / 100)
            {
              AddLog (LOG_LEVEL_INFO, PSTR ("TIC: %d - W counter rollback, start from %d"), teleinfo_meter.nb_message, teleinfo_calc.pact_current_counter);
              teleinfo_calc.pact_previous_counter = teleinfo_calc.pact_current_counter;
            }

            // active power : detect abnormal increment
            else if (teleinfo_calc.pact_current_counter < teleinfo_calc.pact_previous_counter)
            {
              AddLog (LOG_LEVEL_INFO, PSTR ("TIC: %d - W abnormal value, %d after %d"), teleinfo_meter.nb_message, teleinfo_calc.pact_current_counter, teleinfo_calc.pact_previous_counter);
              teleinfo_calc.pact_previous_counter = LONG_MAX;
            }

            // active power : do calculation
            else if (message_ms > 0)
            {
              increment = teleinfo_calc.pact_current_counter - teleinfo_calc.pact_previous_counter;
              teleinfo_phase[0].pact = teleinfo_phase[0].pact / 2 + 3600000 * increment / 2 / (long)message_ms;
              teleinfo_calc.pact_previous_counter = teleinfo_calc.pact_current_counter;
              teleinfo_meter.nb_update++;
            }

            // avoid active power greater than apparent power
            if (teleinfo_phase[0].pact > teleinfo_phase[0].papp) teleinfo_phase[0].pact = teleinfo_phase[0].papp;

            // calculate cos phi
            if (teleinfo_phase[0].papp > 0) teleinfo_phase[0].cosphi = 100 * (float)teleinfo_phase[0].pact / (float)teleinfo_phase[0].papp;

            // update previous message timestamp
            teleinfo_calc.previous_time_message = teleinfo_message.timestamp;

            // reset values
            teleinfo_calc.papp_current_counter = LONG_MAX;
            teleinfo_calc.pact_current_counter = LONG_MAX;
            break;
        }

        // --------------
        //  update flags

        // loop thru phases
        for (phase = 0; phase < teleinfo_contract.phase; phase++)
        {
          // detect apparent power overload
          if (teleinfo_phase[phase].papp > teleinfo_contract.ssousc * (long)teleinfo_config.contract_percent / 100) teleinfo_message.overload = true;

          // detect more than x % power change
          value = abs (teleinfo_phase[phase].papp_last - teleinfo_phase[phase].papp);
          if (value > (teleinfo_contract.ssousc * TELEINFO_PERCENT_CHANGE / 100)) teleinfo_message.percent = true;
          teleinfo_phase[phase].papp_last = teleinfo_phase[phase].papp;
        }

        // ------------------------
        //  update energy counters

        // loop thru phases
        for (phase = 0; phase < teleinfo_contract.phase; phase++)
        {
          // set voltage
          Energy.voltage[phase] = (float)teleinfo_phase[phase].voltage;

          // set apparent and active power
          Energy.apparent_power[phase] = (float)teleinfo_phase[phase].papp;
          Energy.active_power[phase]   = (float)teleinfo_phase[phase].pact;
          Energy.power_factor[phase]   = teleinfo_phase[phase].cosphi / 100;

          // set current
          if (Energy.voltage[phase] > 0) Energy.current[phase] = Energy.apparent_power[phase] / Energy.voltage[phase];
            else Energy.current[phase] = 0;
        } 

        // update global active power counter
        Energy.total[0] = (float)teleinfo_meter.total_wh / 1000;
        Energy.import_active[0] = Energy.total[0];

        // declare received message
        teleinfo_message.received = true;
        break;

      // add other caracters to current line
      default:
        if (teleinfo_meter.idx_line < sizeof (teleinfo_meter.str_line) - 1) teleinfo_meter.str_line[teleinfo_meter.idx_line++] = byte_recv;
        break;
    }

    // if TCP server is active, append character to TCP buffer
    tcp_server.send (byte_recv); 

    // give control back to system to avoid watchdog
    yield ();
  }
}

// Publish JSON if candidate (called 4 times per second)
void TeleinfoEvery250ms ()
{
  // if message should be sent, check which type to send
  if (teleinfo_message.publish_msg)
  {
    teleinfo_message.publish_meter = ((teleinfo_config.msg_type == TELEINFO_MSG_BOTH) || (teleinfo_config.msg_type == TELEINFO_MSG_METER));
    teleinfo_message.publish_tic   = ((teleinfo_config.msg_type == TELEINFO_MSG_BOTH) || (teleinfo_config.msg_type == TELEINFO_MSG_TIC));
    teleinfo_message.publish_msg   = false;
  }

  // if needed, publish meter or TIC JSON
  if (teleinfo_message.publish_meter) TeleinfoPublishJsonMeter ();
  else if (teleinfo_message.publish_tic) TeleinfoPublishJsonTic ();
}

// Calculate if some JSON should be published (called every second)
void TeleinfoEverySecond ()
{
  uint8_t  period, phase;
  uint32_t time_now;
  char     str_value[24];

  // current time
  time_now = LocalTime ();

  // if needed, update energy total (in kwh)
  teleinfo_meter.interval_count++;
  teleinfo_meter.interval_count = teleinfo_meter.interval_count % (60 * teleinfo_config.rom_update);
  if ((teleinfo_meter.status_rx == TIC_SERIAL_ACTIVE) && (teleinfo_meter.interval_count == 0))
  {
    EnergyUpdateTotal ();
    lltoa (teleinfo_meter.total_wh, str_value, 10);
    AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Total counter updated to %s Wh"), str_value);
  }

  // loop thru the periods and the phases, to update all values over the period
  for (period = 0; period <= TELEINFO_PERIOD_WEEK; period++)
  {
    // if needed, set initial log flush time
    if (teleinfo_period[period].time_flush == UINT32_MAX) teleinfo_period[period].time_flush = time_now + ARR_TELEINFO_PERIOD_FLUSH[period];

    // loop thru phases to update max value
    for (phase = 0; phase < teleinfo_contract.phase; phase++)
    {
      // if within range, update phase apparent power
      if (teleinfo_phase[phase].papp != LONG_MAX)
      {
        // add power to period total (for average calculation)
        teleinfo_period[period].pact_sum[phase]   += (long long)teleinfo_phase[phase].pact;
        teleinfo_period[period].papp_sum[phase]   += (long long)teleinfo_phase[phase].papp;
        teleinfo_period[period].cosphi_sum[phase] += teleinfo_phase[phase].cosphi;

        // if voltage defined, update lowest and highest voltage level
        if (teleinfo_phase[phase].voltage > 0)
        {
          if (teleinfo_phase[phase].voltage < teleinfo_period[period].volt_low[phase])  teleinfo_period[period].volt_low[phase]  = teleinfo_phase[phase].voltage;
          if (teleinfo_phase[phase].voltage > teleinfo_period[period].volt_peak[phase]) teleinfo_period[period].volt_peak[phase] = teleinfo_phase[phase].voltage;
        }       

        // update highest apparent power level
        if (teleinfo_phase[phase].papp > teleinfo_period[period].papp_peak[phase]) teleinfo_period[period].papp_peak[phase] = teleinfo_phase[phase].papp;
      }
    }

    // increment graph period counter and update graph data if needed
    teleinfo_period[period].counter ++;
    teleinfo_period[period].counter = teleinfo_period[period].counter % ARR_TELEINFO_PERIOD_WINDOW[period];
    if (teleinfo_period[period].counter == 0) TeleinfoSavePeriodData (period);
  }

  // if needed, init first meter total
  if (teleinfo_meter.day_last_wh  == 0) teleinfo_meter.day_last_wh  = teleinfo_meter.total_wh;
  if (teleinfo_meter.hour_last_wh == 0) teleinfo_meter.hour_last_wh = teleinfo_meter.total_wh;

  // if hour change, save hourly increment
  if ((RtcTime.minute == 59) && (RtcTime.second == 59))
  {
    teleinfo_meter.hour_wh[RtcTime.hour] += (long)(teleinfo_meter.total_wh - teleinfo_meter.hour_last_wh);
    teleinfo_meter.hour_last_wh = teleinfo_meter.total_wh;
  }

  // check if message should be published for overload
  if (teleinfo_message.overload && (teleinfo_config.msg_policy != TELEINFO_POLICY_NEVER))   teleinfo_message.publish_msg = true;
  teleinfo_message.overload = false;

  // check if message should be published after message was received
  if (teleinfo_message.received && (teleinfo_config.msg_policy == TELEINFO_POLICY_MESSAGE)) teleinfo_message.publish_msg = true;
  teleinfo_message.received = false;

  // check if message should be published after percentage chnage
  if (teleinfo_message.percent  && (teleinfo_config.msg_policy == TELEINFO_POLICY_PERCENT)) teleinfo_message.publish_msg = true;
  teleinfo_message.percent  = false;
}

// Generate JSON with TIC informations
void TeleinfoPublishJsonTic ()
{
  int index;

  // Start TIC section    {"Time":"xxxxxxxx","TIC":{ 
  Response_P (PSTR ("{\"%s\":\"%s\",\"%s\":{"), D_JSON_TIME, GetDateAndTime (DT_LOCAL).c_str (), TELEINFO_JSON_TIC);

  // loop thru TIC message lines
  for (index = 0; index < TELEINFO_LINE_QTY; index ++)
    if (teleinfo_message.line[index].checksum != 0)
    {
      // add current line
      if (index != 0) ResponseAppend_P (PSTR (","));
      ResponseAppend_P (PSTR ("\"%s\":\"%s\""), teleinfo_message.line[index].str_etiquette, teleinfo_message.line[index].str_donnee);
    }

  // end of TIC section, publish JSON and process rules
  ResponseAppend_P (PSTR ("}}"));
  MqttPublishTeleSensor ();

  // TIC has been published
  teleinfo_message.publish_tic = false;
}

// Generate JSON with Meter informations
void TeleinfoPublishJsonMeter ()
{
  uint8_t phase;
  int   value;
  long  power_max, power_app, power_act;
  float current;
  char  str_text[16];

  // Start METER section    {"Time":"xxxxxxxx","METER":{
  Response_P (PSTR ("{\"%s\":\"%s\",\"%s\":{"), D_JSON_TIME, GetDateAndTime (DT_LOCAL).c_str (), TELEINFO_JSON_METER);

  // METER basic data
  ResponseAppend_P (PSTR ("\"%s\":%u"),  TELEINFO_JSON_PHASE, teleinfo_contract.phase);
  ResponseAppend_P (PSTR (",\"%s\":%d"), TELEINFO_JSON_ISUB,  teleinfo_contract.isousc);
  ResponseAppend_P (PSTR (",\"%s\":%d"), TELEINFO_JSON_PSUB,  teleinfo_contract.ssousc);

  // METER adjusted maximum power
  power_max = teleinfo_contract.ssousc * (long)teleinfo_config.contract_percent / 100;
  ResponseAppend_P (PSTR (",\"%s\":%d"), TELEINFO_JSON_PMAX, power_max);

  // loop to calculate apparent and active power
  power_app = 0;
  power_act = 0;
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    value = phase + 1;
    power_app += teleinfo_phase[phase].papp;
    power_act += teleinfo_phase[phase].pact;

    // voltage
    ResponseAppend_P (PSTR (",\"U%d\":%d"), value, teleinfo_phase[phase].voltage);

    // apparent and active power
    ResponseAppend_P (PSTR (",\"P%d\":%d,\"W%d\":%d"), value, teleinfo_phase[phase].papp, value, teleinfo_phase[phase].pact);

    // current
    if (teleinfo_phase[phase].voltage > 0)
    {
      current = teleinfo_phase[phase].papp / teleinfo_phase[phase].voltage;
      ext_snprintf_P (str_text, sizeof(str_text), PSTR ("%1_f"), &current);
      ResponseAppend_P (PSTR (",\"I%d\":%s"), value, str_text);
    }

    // cos phi
    ext_snprintf_P (str_text, sizeof(str_text), PSTR ("%2_f"), &Energy.power_factor[phase]);
    ResponseAppend_P (PSTR (",\"C%d\":%s"), value, str_text);
  } 

  // Total Papp and Pact
  if (teleinfo_contract.phase > 1) ResponseAppend_P (PSTR (",\"P\":%d,\"W\":%d"), power_app, power_act);

  // end of METER section, publish JSON and process rules
  ResponseAppend_P (PSTR ("}}"));
  MqttPublishTeleSensor ();

  // Meter has been published
  teleinfo_message.publish_meter = false;
}

// Show JSON status (for MQTT)
void TeleinfoAfterTeleperiod ()
{
  // if telemetry call, check for JSON update according to update policy
  if (teleinfo_config.msg_policy == TELEINFO_POLICY_TELEMETRY) teleinfo_message.publish_msg = true;
}

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

// Display offload icons
void TeleinfoWebIconTic () { Webserver->send_P (200, PSTR ("image/png"), teleinfo_icon_tic_png, teleinfo_icon_tic_len); }

// Get specific argument as a value with min and max
int TeleinfoWebGetArgValue (const char* pstr_argument, const int value_min, const int value_max, const int value_default)
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

// Append Teleinfo graph button to main page
void TeleinfoWebMainButton ()
{
  // Teleinfo message page button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_TELEINFO_PAGE_TIC, "TIC Message");

  // Teleinfo graph page button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_TELEINFO_PAGE_GRAPH, "TIC Graph");
}

// Append Teleinfo configuration button to configuration page
void TeleinfoWebConfigButton ()
{
  // heater and meter configuration button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_TELEINFO_PAGE_CONFIG, PSTR ("Configure Teleinfo"));
}

// Append Teleinfo state to main page
void TeleinfoWebSensor ()
{
  uint8_t phase;
  long    percentage = 0;
  char    str_text[32];
  char    str_header[64];
  char    str_data[128];

  // display according to serial receiver status
  switch (teleinfo_meter.status_rx)
  {
    case TIC_SERIAL_INIT:
      WSContentSend_P (PSTR ("{s}%s{m}Check configuration{e}"), D_TELEINFO);
      break;

    case TIC_SERIAL_FAILED:
      WSContentSend_P (PSTR ("{s}%s{m}Init failed{e}"), D_TELEINFO);
      break;

    case TIC_SERIAL_STOPPED:
      WSContentSend_P (PSTR ("{s}%s{m}Reception stopped{e}"), D_TELEINFO);
      break;

    case TIC_SERIAL_ACTIVE:
      // One bar graph per phase
      if (teleinfo_contract.ssousc > 0)
        for (phase = 0; phase < teleinfo_contract.phase; phase++)
        {
          // calculate and display bar graph percentage
          percentage = 100 * teleinfo_phase[phase].papp / teleinfo_contract.ssousc;
          GetTextIndexed (str_text, sizeof (str_text), phase, kTeleinfoGraphColorPhase);
          WSContentSend_P (PSTR ("<div style='font-size:0.9rem;text-align:center;color:white;padding:1px;margin-bottom:4px;border-radius:4px;background-color:%s;width:%d%%;'>%d%%</div>\n"), str_text, percentage, percentage);
        }

      // Teleinfo mode 
      //   Mode Historique / Standard       Conso / Prod
      GetTextIndexed (str_text, sizeof (str_text), teleinfo_contract.mode, kTeleinfoModeName);
      sprintf (str_header, "%s %s", D_TELEINFO_MODE, str_text);
      GetTextIndexed (str_data, sizeof (str_data), teleinfo_contract.type, kTeleinfoTypeName);

      // Teleinfo contract power
      if (teleinfo_contract.ssousc > 0)
      {
        // header
        strlcat (str_header, "<br>", sizeof (str_header));
        strlcat (str_header, D_TELEINFO_CONTRACT, sizeof (str_header));

        // number of phases
        strcpy_P (str_text, PSTR ("<br>"));
        if (teleinfo_contract.phase > 1) sprintf_P (str_text, PSTR ("<br>%u x "), teleinfo_contract.phase);
        strlcat (str_data, str_text, sizeof (str_data));

        // power
        sprintf_P (str_text, PSTR ("%d kVA"), teleinfo_contract.ssousc / 1000);
        strlcat (str_data, str_text, sizeof (str_data));
      }

      // Teleinfo period
      if ((teleinfo_contract.period >= 0) && (teleinfo_contract.period < TIC_PERIOD_MAX))
      {
        // header
        strlcat (str_header, "<br>", sizeof (str_header));
        strlcat (str_header, D_TELEINFO_PERIOD, sizeof (str_header));

        // period label
        GetTextIndexed (str_text, sizeof (str_text), teleinfo_contract.period, kTeleinfoPeriodName);
        strlcat (str_data, "<br>", sizeof (str_data));
        strlcat (str_data, D_TELEINFO_HEURES, sizeof (str_data));
        strlcat (str_data, " ", sizeof (str_data));
        strlcat (str_data, str_text, sizeof (str_data));
      }

      // display data
      WSContentSend_P (PSTR ("{s}%s{m}%s{e}"), str_header, str_data);

      // display number of messages and cos phi
      strcpy_P (str_header, PSTR ("Messages / Cosφ"));
      sprintf_P (str_data, PSTR ("%d / %d"), teleinfo_meter.nb_message, teleinfo_meter.nb_update);

      // calculate error percentage
      percentage = 0;
      if (teleinfo_meter.nb_line > 0) percentage = (long)(teleinfo_meter.nb_error * 100 / teleinfo_meter.nb_line);

      // if needed, display reset and error percentage
      if ((teleinfo_meter.nb_reset > 0) || (percentage > 0))
      {
        strlcat (str_header, "<br>Reset / Errors", sizeof (str_header));
        sprintf_P (str_text, PSTR ("<br>%d / %d%%"), teleinfo_meter.nb_reset, percentage);
        strlcat (str_data, str_text, sizeof (str_data));
      }

      // display data
      WSContentSend_P (PSTR ("{s}%s{m}%s{e}"), str_header, str_data);
      break;
  }
}

// Teleinfo web page
void TeleinfoWebPageConfigure ()
{
  int  index;
  char str_text[32];
  char str_select[16];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess ()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg (F ("save")))
  {
    // parameter 'rate' : set teleinfo rate
    WebGetArg (D_CMND_TELEINFO_RATE, str_text, sizeof (str_text));
    if (strlen (str_text) > 0) teleinfo_config.baud_rate = TeleinfoValidateBaudRate (atoi (str_text));

    // parameter 'msgp' : set TIC messages diffusion policy
    WebGetArg (D_CMND_TELEINFO_MSG_POLICY, str_text, sizeof (str_text));
    if (strlen (str_text) > 0) teleinfo_config.msg_policy = atoi (str_text);

    // parameter 'msgt' : set TIC messages type diffusion policy
    WebGetArg (D_CMND_TELEINFO_MSG_TYPE, str_text, sizeof (str_text));
    if (strlen (str_text) > 0) teleinfo_config.msg_type = atoi (str_text);

    // save configuration
    TeleinfoSaveConfig ();

    // ask for meter update
    teleinfo_message.publish_msg = true;
  }

  // beginning of form
  WSContentStart_P (PSTR ("Configure Teleinfo"));
  WSContentSendStyle ();
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_TELEINFO_PAGE_CONFIG);

  // speed selection form
  WSContentSend_P (PSTR ("<p><fieldset><legend><b>&nbsp;%s %s&nbsp;</b></legend>\n"), "📨", PSTR ("TIC Rate"));
  for (index = 0; index < nitems (ARR_TELEINFO_RATE); index++)
  {
    if (ARR_TELEINFO_RATE[index] == teleinfo_config.baud_rate) strcpy_P (str_select, PSTR ("checked")); else strcpy (str_select, "");
    itoa (ARR_TELEINFO_RATE[index], str_text, 10);
    WSContentSend_P (PSTR ("<p><input type='radio' name='%s' value='%d' %s>%s</p>\n"), D_CMND_TELEINFO_RATE, ARR_TELEINFO_RATE[index], str_select, str_text);
  }
  WSContentSend_P (PSTR ("</fieldset></p>\n"));

  // teleinfo message diffusion policy
  WSContentSend_P (PSTR ("<p><fieldset><legend><b>&nbsp;%s %s&nbsp;</b></legend>\n"), "🧾", PSTR ("Message policy"));
  for (index = 0; index < TELEINFO_POLICY_MAX; index++)
  {
    if (index == teleinfo_config.msg_policy) strcpy_P (str_select, PSTR ("checked")); else strcpy (str_select, "");
    GetTextIndexed (str_text, sizeof (str_text), index, kTeleinfoMessagePolicy);
    WSContentSend_P (PSTR ("<p><input type='radio' name='%s' value='%d' %s>%s</p>\n"), D_CMND_TELEINFO_MSG_POLICY, index, str_select, str_text);
  }
  WSContentSend_P (PSTR ("</fieldset></p>\n"));

  // teleinfo message diffusion type
  WSContentSend_P (PSTR ("<p><fieldset><legend><b>&nbsp;%s %s&nbsp;</b></legend>\n"), "📑", PSTR ("Message data"));
  for (index = 0; index < TELEINFO_MSG_MAX; index++)
  {
    if (index == teleinfo_config.msg_type) strcpy_P (str_select, PSTR ("checked")); else strcpy (str_select, "");
    GetTextIndexed (str_text, sizeof (str_text), index, kTeleinfoMessageType);
    WSContentSend_P (PSTR ("<p><input type='radio' name='%s' value='%d' %s>%s</p>\n"), D_CMND_TELEINFO_MSG_TYPE, index, str_select, str_text);
  }
  WSContentSend_P (PSTR ("</fieldset></p>\n"));

  // save button
  WSContentSend_P (PSTR ("<button name='save' type='submit' class='button bgrn'>%s</button>"), D_SAVE);
  WSContentSend_P (PSTR ("</form>\n"));

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

// TIC raw message data
void TeleinfoWebTicUpdate ()
{
  int index;

  // start of data page
  WSContentBegin (200, CT_PLAIN);

  // send line number
  WSContentSend_P ("COUNTER|%d\n", teleinfo_meter.nb_message);

  // loop thru TIC message lines to publish if defined
  for (index = 0; index < teleinfo_message.line_max; index ++)
  {
    if (teleinfo_message.line[index].checksum != 0) WSContentSend_P (PSTR ("%s|%s|%c\n"), teleinfo_message.line[index].str_etiquette, teleinfo_message.line[index].str_donnee, teleinfo_message.line[index].checksum);
    else WSContentSend_P (PSTR (" | | \n"));
  }

  // end of data page
  WSContentEnd ();
}

// TIC message page
void TeleinfoWebPageTic ()
{
  int index;

  // beginning of form without authentification
  WSContentStart_P (D_TELEINFO_MESSAGE, false);
  WSContentSend_P (PSTR ("</script>\n"));

  // page data refresh script
  WSContentSend_P (PSTR ("<script type='text/javascript'>\n"));
  WSContentSend_P (PSTR ("function update() {\n"));
  WSContentSend_P (PSTR (" httpUpd=new XMLHttpRequest();\n"));
  WSContentSend_P (PSTR (" httpUpd.onreadystatechange=function()\n"));
  WSContentSend_P (PSTR (" {\n"));
  WSContentSend_P (PSTR ("  if (httpUpd.responseText.length>0)\n"));
  WSContentSend_P (PSTR ("  {\n"));
  WSContentSend_P (PSTR ("   arr_param=httpUpd.responseText.split('\\n');\n"));
  WSContentSend_P (PSTR ("   num_param=arr_param.length;\n"));
  WSContentSend_P (PSTR ("   arr_value=arr_param[0].split('|');\n"));
  WSContentSend_P (PSTR ("   document.getElementById('count').textContent=arr_value[1];\n"));
  WSContentSend_P (PSTR ("   for (i=1;i<num_param;i++)\n"));
  WSContentSend_P (PSTR ("   {\n"));
  WSContentSend_P (PSTR ("    arr_value=arr_param[i].split('|');\n"));
  WSContentSend_P (PSTR ("    document.getElementById('e'+i).textContent=arr_value[0];\n"));
  WSContentSend_P (PSTR ("    document.getElementById('d'+i).textContent=arr_value[1];\n"));
  WSContentSend_P (PSTR ("    document.getElementById('c'+i).textContent=arr_value[2];\n"));
  WSContentSend_P (PSTR ("   }\n"));
  WSContentSend_P (PSTR ("   for (i=num_param;i<=%d;i++)\n"), teleinfo_message.line_max);
  WSContentSend_P (PSTR ("   {\n"));
  WSContentSend_P (PSTR ("    document.getElementById('e'+i).textContent='';\n"));
  WSContentSend_P (PSTR ("    document.getElementById('d'+i).textContent='';\n"));
  WSContentSend_P (PSTR ("    document.getElementById('c'+i).textContent='';\n"));
  WSContentSend_P (PSTR ("   }\n"));
  WSContentSend_P (PSTR ("  }\n"));
  WSContentSend_P (PSTR (" }\n"));
  WSContentSend_P (PSTR (" httpUpd.open('GET','%s',true);\n"), D_TELEINFO_PAGE_TIC_UPD);
  WSContentSend_P (PSTR (" httpUpd.send();\n"));
  WSContentSend_P (PSTR ("}\n"));
  WSContentSend_P (PSTR ("setInterval(function() {update();},1000);\n"));
  WSContentSend_P (PSTR ("</script>\n"));

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {width:100%%;margin:12px auto;padding:3px 0px;text-align:center;vertical-align:middle;}\n"));
  WSContentSend_P (PSTR ("div.title {font-size:2rem;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("span {font-size:1.5rem;padding:0.5rem 1rem;border-radius:1rem;background:#4d82bd;}\n"));
  WSContentSend_P (PSTR ("table {width:100%%;max-width:400px;margin:0px auto;border:none;background:none;color:#fff;border-collapse:collapse;}\n"));
  WSContentSend_P (PSTR ("th,td {font-size:1rem;padding:0.2rem 0.1rem;border:1px #666 solid;}\n"));
  WSContentSend_P (PSTR ("th {font-style:bold;background:#555;}\n"));
  WSContentSend_P (PSTR ("th.label {width:30%%;}\n"));
  WSContentSend_P (PSTR ("th.value {width:60%%;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // page body
  WSContentSend_P (PSTR ("</head>\n"));
  WSContentSend_P (PSTR ("<body>\n"));

  // meter name and teleinfo icon
  WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText (SET_DEVICENAME));
  WSContentSend_P (PSTR ("<div><a href='/'><img height=64 src='%s'></a></div>\n"), D_TELEINFO_ICON_PNG);

  // display counter
  WSContentSend_P (PSTR ("<div><span id='count'>%d</span></div>\n"), teleinfo_meter.nb_message);

  // display table with header
  WSContentSend_P (PSTR ("<div><table>\n"));
  WSContentSend_P (PSTR ("<tr><th class='label'>🏷️</th><th class='value'>📄</th><th>✅</th></tr>\n"));

  // loop to display TIC messsage lines
  for (index = 0; index < teleinfo_message.line_max; index ++)
    WSContentSend_P (PSTR ("<tr id='l%d'><td id='e%d'>%s</td><td id='d%d'>%s</td><td id='c%d'>%c</td></tr>\n"), index + 1, index + 1, teleinfo_message.line[index].str_etiquette, index + 1, teleinfo_message.line[index].str_donnee, index + 1, teleinfo_message.line[index].checksum);

  // end of table and end of page
  WSContentSend_P (PSTR ("</table></div>\n"));
  WSContentSend_P (PSTR ("<div>\n"));
  WSContentStop ();
}

// Display graph frame and time marks
void TeleinfoWebGraphTime (const uint8_t period, const uint8_t histo)
{
  uint32_t index;
  uint32_t unit_width, shift_unit, shift_width;  
  uint32_t graph_x;  
  uint32_t current_time;
  TIME_T   time_dst;
  char     str_text[8];

  // handle graph units according to period
  switch (period) 
  {
    case TELEINFO_PERIOD_LIVE:
      // extract time data
      current_time = LocalTime ();
      BreakTime (current_time, time_dst);

      // calculate horizontal shift
      unit_width  = teleinfo_graph.width / 6;
      shift_unit  = time_dst.minute % 10;
      shift_width = unit_width - (unit_width * shift_unit / 10) - (unit_width * time_dst.second / 600);

      // calculate first time displayed by substracting (5 * 10mn + shift) to current time
      current_time = current_time - 3000 - (60 * shift_unit); 

      // display 5 mn separation lines with time
      for (index = 0; index < 6; index++)
      {
        // convert back to date and increase time of 5mn
        BreakTime (current_time, time_dst);
        current_time += 600;

        // display separation line and time
        graph_x = teleinfo_graph.left + shift_width + (index * unit_width);
        WSContentSend_P (PSTR ("<line class='time' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n"), graph_x, 1, graph_x, 99);
        WSContentSend_P (PSTR ("<text class='time base' x='%d' y='%u%%'>%02uh%0u</text>\n"), graph_x, 3, time_dst.hour, time_dst.minute);
      }
      break;

    case TELEINFO_PERIOD_DAY:
      // calculate separator width
      unit_width  = teleinfo_graph.width / 6;

      // display 4 hours separation lines with hour
      for (index = 1; index < 6; index++)
      {
        // display separation line and time
        graph_x = teleinfo_graph.left + (index * unit_width);
        WSContentSend_P (PSTR ("<line class='time' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n"), graph_x, 1, graph_x, 99);
        WSContentSend_P (PSTR ("<text class='time' x='%d' y='%u%%'>%02dh</text>\n"), graph_x, 3, index * 4);
      }
      break;

    case TELEINFO_PERIOD_WEEK:
      // calculate separator width
      unit_width = teleinfo_graph.width / 7;

      // display day lines with day name
      for (index = 0; index < 7; index++)
      {
        // display days and separation separation lines
        graph_x = teleinfo_graph.left + index * unit_width;
        if (index > 0) WSContentSend_P (PSTR ("<line class='time' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n"), graph_x, 1, graph_x, 99);
        GetTextIndexed (str_text, sizeof (str_text), index, kTeleinfoWeekDay);
        WSContentSend_P (PSTR ("<text class='time' x='%d' y='%u%%'>%s</text>\n"), graph_x + (unit_width / 2), 3, str_text);
      }
      break;
  }
}

// Display data curve
void TeleinfoWebDisplayCurve (const uint8_t phase, const uint8_t data)
{
//  bool first_point = true;
  int  index, index_array;
  long graph_x, graph_y, graph_range, graph_delta;
  long prev_x, prev_y, bezier_y1, bezier_y2; 
  long value, prev_value;
  long arr_value[TELEINFO_GRAPH_SAMPLE];
  char str_value[36];
  char str_line[256];

  // check parameters
  if (teleinfo_graph.period > TELEINFO_PERIOD_WEEK) return;
  if (teleinfo_config.max_va == 0) return;

  // init array
  for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index ++) arr_value[index] = LONG_MAX;

  // collect data from memory
  if (teleinfo_graph.period == TELEINFO_PERIOD_LIVE) 
    for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
    {
      // get current array index if in live memory mode
      index_array = (teleinfo_period[teleinfo_graph.period].index + index) % TELEINFO_GRAPH_SAMPLE;

      // display according to data type
      switch (data)
      {
        case TELEINFO_UNIT_VA:
          if (teleinfo_graph.data_live.arr_papp[phase][index_array] != UINT8_MAX) arr_value[index] = (long)teleinfo_graph.data_live.arr_papp[phase][index_array] * teleinfo_contract.ssousc / 200;
          break;

        case TELEINFO_UNIT_W:
          if (teleinfo_graph.data_live.arr_pact[phase][index_array] != UINT8_MAX) arr_value[index] = (long)teleinfo_graph.data_live.arr_pact[phase][index_array] * teleinfo_contract.ssousc / 200;
          break;

        case TELEINFO_UNIT_V:
          if (teleinfo_graph.data_live.arr_volt[phase][index_array] != UINT8_MAX) arr_value[index] = (long)TELEINFO_VOLTAGE - 128 + teleinfo_graph.data_live.arr_volt[phase][index_array];
          break;

        case TELEINFO_UNIT_COSPHI:
          if (teleinfo_graph.data_live.arr_cosphi[phase][index_array] != UINT8_MAX) arr_value[index] = (long)teleinfo_graph.data_live.arr_cosphi[phase][index_array];
          break;
      }
    }

#ifdef USE_UFILESYS
  // else collect data from file
  else
  {
    uint8_t column;
    size_t  size_line, size_value;
    char    str_filename[UFS_FILENAME_SIZE];
    File    file;

    // if data file exists
    if (TeleinfoHistoGetFilename (teleinfo_graph.period, teleinfo_graph.histo, str_filename))
    {
      //open file and skip header
      file = ffsp->open (str_filename, "r");
      UfsReadNextLine (file, str_line, sizeof (str_line));

      // loop to read lines
      do
      {
        // read line
        size_line = UfsReadNextLine (file, str_line, sizeof (str_line));
        if (size_line > 0)
        {
          // init
          index = INT_MAX;
          value = LONG_MAX;

          // set column with data
          column = 4 + (phase * 6) + data;

          // extract index from line
          size_value = UfsExtractCsvColumn (str_line, ';', 1, str_value, sizeof (str_value), false);
          if (size_value > 0) index = atoi (str_value);

          // extract value from line
          size_value = UfsExtractCsvColumn (str_line, ';', column, str_value, sizeof (str_value), false);
          if (size_value > 0) value = atol (str_value);

          // if index and value are valid, update value to be displayed
          if ((index < TELEINFO_GRAPH_SAMPLE) && (value != LONG_MAX)) arr_value[index] = value;
        }
      }
      while (size_line > 0);

      // close file
      file.close ();

      // give control back to system to avoid watchdog
      yield ();
    }
  }
#endif    // USE_UFILESYS

  // start of curve
  switch (data)
  {
    case TELEINFO_UNIT_VA:
    case TELEINFO_UNIT_W:
      WSContentSend_P ("<path class='ph%u' ", phase);
      break;

    case TELEINFO_UNIT_PEAK_VA:
    case TELEINFO_UNIT_PEAK_V:
      WSContentSend_P ("<path class='pk%u' ", phase);
      break;

    case TELEINFO_UNIT_V:
    case TELEINFO_UNIT_COSPHI:
      WSContentSend_P ("<path class='ln%u' ", phase);
      break;

  }

  // display values
  strcpy (str_line, "");
  prev_x = LONG_MAX;
  for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
  {
    // if value is valid
    if (arr_value[index] != LONG_MAX) 
    {
      // get current, previous and next values
      value = arr_value[index];
      if (index == 0) prev_value = arr_value[0];
        else prev_value = arr_value[index - 1];

      // calculate x position
      graph_x = teleinfo_graph.left + (index * teleinfo_graph.width / TELEINFO_GRAPH_SAMPLE);

      // calculate y position according to data
      switch (data)
      {
        case TELEINFO_UNIT_VA:
        case TELEINFO_UNIT_PEAK_VA:
        case TELEINFO_UNIT_W:
          graph_y = teleinfo_config.height - (value * teleinfo_config.height / teleinfo_config.max_va);
          break;

        case TELEINFO_UNIT_V:
        case TELEINFO_UNIT_PEAK_V:
          graph_range = abs (teleinfo_config.max_v - TELEINFO_VOLTAGE);
          graph_delta = (TELEINFO_VOLTAGE - value) * teleinfo_config.height / 2 / graph_range;
          graph_y = (teleinfo_config.height / 2) + graph_delta;
          if (graph_y < 0) graph_y = 0;
          if (graph_y > teleinfo_config.height) graph_y = teleinfo_config.height;
          break;

        case TELEINFO_UNIT_COSPHI:
          graph_y = teleinfo_config.height - (value * teleinfo_config.height / 100);
          break;
      }

      // display curve point
      switch (data)
      {
        case TELEINFO_UNIT_VA:
        case TELEINFO_UNIT_W:
          // if first point
          if (prev_x == LONG_MAX)
          {
            // start point
            sprintf (str_value, "d='M%d %d ", graph_x, teleinfo_config.height);
            strcat (str_line, str_value);

            // first point
            sprintf (str_value, "L %d %d ", graph_x, graph_y);
            strcat (str_line, str_value);
          }

          // else, other point
          else
          {
            // calculate bezier curve value
            if ((graph_x - prev_x > 10) || (prev_value == value))
            {
              bezier_y1 = graph_y;
              bezier_y2 = prev_y;
            }
            else
            {
              bezier_y1 = (prev_y + graph_y) / 2;
              bezier_y2 = bezier_y1;
            }

            // display point
            sprintf (str_value, "C %d %d, %d %d, %d %d ", graph_x, bezier_y1, prev_x, bezier_y2, graph_x, graph_y);
            strcat (str_line, str_value);
          }
          break;

        case TELEINFO_UNIT_PEAK_VA:
        case TELEINFO_UNIT_PEAK_V:
        case TELEINFO_UNIT_V:
        case TELEINFO_UNIT_COSPHI:
          // if first point
          if (prev_x == LONG_MAX)
          {
            sprintf (str_value, "d='M%d %d ", graph_x, graph_y);
            strcat (str_line, str_value);
          }

          // else, other point
          else
          {
            // calculate bezier curve value
            if ((graph_x - prev_x > 10) || (prev_value == value))
            {
              bezier_y1 = graph_y;
              bezier_y2 = prev_y;
            }
            else
            {
              bezier_y1 = (prev_y + graph_y) / 2;
              bezier_y2 = bezier_y1;
            }

            // display point
            sprintf (str_value, "C %d %d, %d %d, %d %d ", graph_x, bezier_y1, prev_x, bezier_y2, graph_x, graph_y);
            strcat (str_line, str_value);
          }
          break;
      }

      // save previous y position
      prev_x = graph_x;
      prev_y = graph_y;
    }

    // if needed, flush buffer
    if ((strlen (str_line) > 220) || (index == TELEINFO_GRAPH_SAMPLE - 1))
    {
      WSContentSend_P (str_line);
      strcpy (str_line, "");
    }
  }

  // end data value curve
  switch (data)
  {
    case TELEINFO_UNIT_VA:
    case TELEINFO_UNIT_W:
      WSContentSend_P ("L%d,%d Z'/>\n", graph_x, teleinfo_config.height);
      break;

    case TELEINFO_UNIT_PEAK_VA:
    case TELEINFO_UNIT_PEAK_V:
    case TELEINFO_UNIT_V:
    case TELEINFO_UNIT_COSPHI:
      WSContentSend_P ("'/>\n");
      break;
  }
}

#ifdef USE_UFILESYS

// Display bar graph
void TeleinfoWebDisplayBarGraph (const uint8_t histo, const bool current)
{
  int      index;
  long     value, value_x, value_y;
  long     graph_x, graph_y, graph_range, graph_delta, graph_width, graph_height, graph_x_end, graph_max;    
  long     arr_value[TELEINFO_GRAPH_MAX_BARGRAPH];
  uint8_t  day_of_week;
  uint32_t time_bar;
  size_t   size_line, size_value;
  TIME_T   time_dst;
  char     str_type[8];
  char     str_value[16];
  char     str_line[256];
  char     str_filename[UFS_FILENAME_SIZE];
  File     file;

  // init array
  for (index = 0; index < TELEINFO_GRAPH_MAX_BARGRAPH; index ++) arr_value[index] = LONG_MAX;

  // if full month view, calculate first day of month
  if (teleinfo_graph.month != 0)
  {
    BreakTime (LocalTime (), time_dst);
    time_dst.year -= histo;
    time_dst.month = teleinfo_graph.month;
    time_dst.day_of_week = 0;
    time_dst.day_of_year = 0;
  }

  // init graph units for full year display (month bars)
  if (teleinfo_graph.month == 0)
  {
    graph_width = 90;             // width of graph bar area
    graph_range = 12;             // number of graph bars (months per year)
    graph_delta = 20;             // separation between bars (pixels)
    graph_max   = teleinfo_config.max_kwh_month;
    strcpy (str_type, "month");
  }

  // else init graph units for full month display (day bars)
  else if (teleinfo_graph.day == 0)
  {
    graph_width = 35;             // width of graph bar area
    graph_range = 31;             // number of graph bars (days per month)
    graph_delta = 4;              // separation between bars (pixels)
    graph_max   = teleinfo_config.max_kwh_day;
    strcpy (str_type, "day");
  }

  // else init graph units for full day display (hour bars)
  else
  {
    graph_width = 45;             // width of graph bar area
    graph_range = 24;             // number of graph bars (hours per day)
    graph_delta = 10;             // separation between bars (pixels)
    graph_max   = teleinfo_config.max_kwh_hour;
    strcpy (str_type, "hour");
  }

  // if current day, collect live values
  if ((histo == 0) && (teleinfo_graph.month == RtcTime.month) && (teleinfo_graph.day == RtcTime.day_of_month))
  {
    // update last hour increment
    teleinfo_meter.hour_wh[RtcTime.hour] += (long)(teleinfo_meter.total_wh - teleinfo_meter.hour_last_wh);
    teleinfo_meter.hour_last_wh = teleinfo_meter.total_wh;

    // init hour slots from live values
    for (index = 0; index < 24; index ++) if (teleinfo_meter.hour_wh[index] > 0) arr_value[index] = teleinfo_meter.hour_wh[index];
  }

  // calculate graph height and graph start
  graph_height = teleinfo_config.height;
  graph_x      = teleinfo_graph.left + graph_delta / 2;
  graph_x_end  = graph_x + graph_width - graph_delta;
  if (!current) { graph_x +=4; graph_x_end -= 4; }

  // if data file exists
  if (TeleinfoHistoGetFilename (TELEINFO_PERIOD_YEAR, histo, str_filename))
  {
    //open file and skip header
    file = ffsp->open (str_filename, "r");
    UfsReadNextLine (file, str_line, sizeof (str_line));

    // loop to read lines and load array
    do
    {
      // read line
      size_line = UfsReadNextLine (file, str_line, sizeof (str_line));
      if (size_line > 0)
      {
        // init
        index = INT_MAX;
        value = LONG_MAX;

        // handle values for a full year
        if (teleinfo_graph.month == 0)
        {
          // extract month index from line
          size_value = UfsExtractCsvColumn (str_line, ';', 2, str_value, sizeof (str_value), false);
          if (size_value > 0) index = atoi (str_value);

          // extract value from line
          size_value = UfsExtractCsvColumn (str_line, ';', 5, str_value, sizeof (str_value), false);
          if (size_value > 0) value = atol (str_value);

          // if index and value are valid, add value to month of year
          if ((index <= 12) && (value != LONG_MAX))
            if (arr_value[index] == LONG_MAX) arr_value[index] = value; 
              else arr_value[index] += value;
        }

        // else check if line deals with target month
        else
        {
          // extract month index from line
          size_value = UfsExtractCsvColumn (str_line, ';', 2, str_value, sizeof (str_value), false);
          if (size_value > 0) index = atoi (str_value);
          if (teleinfo_graph.month == index)
          {
            // if display by days of selected month / year
            if (teleinfo_graph.day == 0)
            {
              // extract day index from line
              index = INT_MAX;
              size_value = UfsExtractCsvColumn (str_line, ';', 3, str_value, sizeof (str_value), false);
              if (size_value > 0) index = atoi (str_value);

              // extract value from line
              size_value = UfsExtractCsvColumn (str_line, ';', 5, str_value, sizeof (str_value), false);
              if (size_value > 0) value = atol (str_value);

              // if index and value are valid, add value to day of month
              if ((index <= 31) && (value != LONG_MAX))
                if (arr_value[index] == LONG_MAX) arr_value[index] = value;
                  else arr_value[index] += value;
            }

            // else display by hours selected day / month / year
            else
            {
              // extract day index from line
              index = INT_MAX;
              size_value = UfsExtractCsvColumn (str_line, ';', 3, str_value, sizeof (str_value), false);
              if (size_value > 0) index = atoi (str_value);
              if (teleinfo_graph.day == index)
              {
                // loop to extract hours increments
                for (index = 1; index <= 24; index ++)
                {
                  // extract value from line
                  size_value = UfsExtractCsvColumn (str_line, ';', index + 5, str_value, sizeof (str_value), false);
                  if (size_value > 0) value = atol (str_value);

                  // if value is valid, add value to hour slot
                  if (arr_value[index] == LONG_MAX) arr_value[index] = value;
                   else arr_value[index] += value;
                }
              }
            }
          }
        }
      }
    }
    while (size_line > 0);

    // close file
    file.close ();

    // give control back to system to avoid watchdog
    yield ();
  }

  // loop to display bar graphs
  for (index = 1; index <= graph_range; index ++)
  {
    // if value is defined, display bar and value
    if (arr_value[index] != LONG_MAX)
    {

      // bar graph
      // ---------

      // display
      graph_y = graph_height - (arr_value[index] * graph_height / graph_max / 1000);
      if (graph_y < 0) graph_y = 0;

      // display link
      if (current && (teleinfo_graph.month == 0)) WSContentSend_P (PSTR("<a href='%s?month=%d&day=0'>"), D_TELEINFO_PAGE_GRAPH, index);
      else if (current && (teleinfo_graph.day == 0)) WSContentSend_P (PSTR("<a href='%s?day=%d'>"), D_TELEINFO_PAGE_GRAPH, index);

      // display bar
      if (current) strcpy (str_value, "now"); else strcpy (str_value, "prev");
      WSContentSend_P (PSTR("<path class='%s' d='M%d %d L%d %d L%d %d L%d %d L%d %d L%d %d Z'></path>"), str_value, graph_x, graph_height, graph_x, graph_y + 2, graph_x + 2, graph_y, graph_x_end - 2, graph_y, graph_x_end, graph_y + 2, graph_x_end, graph_height);

      // end of link 
      if (current && ((teleinfo_graph.month == 0) || (teleinfo_graph.day == 0))) WSContentSend_P (PSTR("</a>\n"));
        else WSContentSend_P (PSTR("\n"));

      // if main graph
      if (current)
      {
        // value on top of bar
        // -------------------

        if (arr_value[index] > 0)
        {
          // calculate bar graph value position
          value_x = (graph_x + graph_x_end) / 2;
          value_y = graph_y - 15;
          if (value_y < 15) value_y = 15;
          if (value_y > graph_height - 50) value_y = graph_height - 50;

          TeleinfoDisplayValue (TELEINFO_UNIT_MAX, arr_value[index], str_value, sizeof (str_value), true);
          WSContentSend_P (PSTR("<text class='%s value' x='%d' y='%d'>%s</text>\n"), str_type, value_x, value_y, str_value);
        }

        // month name or day / hour number
        // ----------------

        // if full year, get month name else get day of month
        if (teleinfo_graph.month == 0) GetTextIndexed (str_value, sizeof (str_value), index, kTeleinfoYearMonth);
          else if (teleinfo_graph.day == 0) sprintf (str_value, "%02d", index);
            else sprintf (str_value, "%dh", index - 1);

        // display
        value_y = graph_height - 10;
        WSContentSend_P (PSTR("<text class='%s' x='%d' y='%d'>%s</text>\n"), str_type, value_x, value_y, str_value);

        // week day name
        // -------------

        if ((teleinfo_graph.month != 0) && (teleinfo_graph.day == 0))
        {
          // calculate day name
          time_dst.day_of_month = index;
          time_bar = MakeTime (time_dst);
          BreakTime (time_bar, time_dst);
          day_of_week = (time_dst.day_of_week + 5) % 7;
          GetTextIndexed (str_value, sizeof (str_value), day_of_week, kTeleinfoWeekDay2);

          // display
          value_y = graph_height - 30;
          WSContentSend_P (PSTR("<text class='%s' x='%d' y='%d'>%s</text>\n"), str_type, value_x, value_y, str_value);
        }
      }
    }

    // increment bar position
    graph_x     += graph_width;
    graph_x_end += graph_width;
  }
}

#endif    // USE_UFILESYS

// Graph frame
void TeleinfoWebGraphFrame ()
{
  int     nb_digit = -1;
  long    index, unit_min, unit_max, unit_range;
  float   value;
  char    str_unit[8];
  char    str_text[8];
  char    arr_label[5][12];
  uint8_t arr_pos[5] = {98,76,51,26,2};  

  // set labels according to data type
  switch (teleinfo_graph.data) 
  {
    // power
    case TELEINFO_UNIT_VA:
    case TELEINFO_UNIT_W:
      for (index = 0; index < 5; index ++) TeleinfoDisplayValue (TELEINFO_UNIT_MAX, index * teleinfo_config.max_va / 4, arr_label[index], sizeof (arr_label[index]), true);
      break;

    // voltage
    case TELEINFO_UNIT_V:
      unit_max   = teleinfo_config.max_v;
      unit_min   = 2 * TELEINFO_VOLTAGE - teleinfo_config.max_v;
      unit_range = unit_max - unit_min;
      for (index = 0; index < 5; index ++) ltoa (unit_min + index * unit_range / 4, arr_label[index], 10);
      break;

    // cos phi
    case TELEINFO_UNIT_COSPHI:
      for (index = 0; index < 5; index ++)
      {
        value = (float)index / 4;
        ext_snprintf_P (arr_label[index], sizeof (arr_label[index]), PSTR ("%02_f"), &value);
      }
      break;

    // watt per hour
    case TELEINFO_UNIT_WH:
      if (teleinfo_graph.month == 0) unit_max = teleinfo_config.max_kwh_month;
        else if (teleinfo_graph.day == 0) unit_max = teleinfo_config.max_kwh_day;
          else unit_max = teleinfo_config.max_kwh_hour;
      unit_max = unit_max * 1000;
      for (index = 0; index < 5; index ++) TeleinfoDisplayValue (TELEINFO_UNIT_MAX, index * unit_max / 4, arr_label[index], sizeof (arr_label[index]), true);
      break;

    default:
      for (index = 0; index < 5; index ++) strcpy (arr_label[index], "");
      break;
  }

  // graph frame
  WSContentSend_P (PSTR ("<rect x='%d%%' y='%d%%' width='%d%%' height='99.9%%' rx='10' />\n"), TELEINFO_GRAPH_PERCENT_START, 0, TELEINFO_GRAPH_PERCENT_STOP - TELEINFO_GRAPH_PERCENT_START);

  // graph separation lines
  WSContentSend_P (PSTR ("<line class='dash' x1='%d%%' y1='%d%%' x2='%d%%' y2='%d%%' />\n"), TELEINFO_GRAPH_PERCENT_START, 25, TELEINFO_GRAPH_PERCENT_STOP, 25);
  WSContentSend_P (PSTR ("<line class='dash' x1='%d%%' y1='%d%%' x2='%d%%' y2='%d%%' />\n"), TELEINFO_GRAPH_PERCENT_START, 50, TELEINFO_GRAPH_PERCENT_STOP, 50);
  WSContentSend_P (PSTR ("<line class='dash' x1='%d%%' y1='%d%%' x2='%d%%' y2='%d%%' />\n"), TELEINFO_GRAPH_PERCENT_START, 75, TELEINFO_GRAPH_PERCENT_STOP, 75);

  // get unit label
  strcpy (str_text, "");
  if (nb_digit != -1) strcpy (str_text, "k");
  if (teleinfo_graph.period == TELEINFO_PERIOD_YEAR) strcpy (str_unit, "Wh");
  else GetTextIndexed (str_unit, sizeof (str_unit), teleinfo_graph.data, kTeleinfoGraphDisplay);
  strlcat (str_text, str_unit, sizeof (str_text));

  // units graduation
  WSContentSend_P (PSTR ("<text class='power' x='%d%%' y='%d%%'>%s</text>\n"), TELEINFO_GRAPH_PERCENT_STOP + 3, 2, str_text);
  for (index = 0; index < 5; index ++) WSContentSend_P (PSTR ("<text class='power' x='%u%%' y='%u%%'>%s</text>\n"), 2, arr_pos[index],  arr_label[index]);
}

// Graph public page
void TeleinfoWebGraphPage ()
{
  bool     display;
  uint8_t  phase, choice, counter;  
  uint16_t index;
  uint32_t year; 
  long     percentage;
  TIME_T   time_dst;
  char     str_type[8];
  char     str_text[16];
  char     str_date[16];

  // get numerical argument values
  teleinfo_graph.data  = (uint8_t)TeleinfoWebGetArgValue (D_CMND_TELEINFO_DATA,  0, TELEINFO_UNIT_MAX - 1, teleinfo_graph.data);
  teleinfo_graph.day   = (uint8_t)TeleinfoWebGetArgValue (D_CMND_TELEINFO_DAY,   0, 31, teleinfo_graph.day);
  teleinfo_graph.month = (uint8_t)TeleinfoWebGetArgValue (D_CMND_TELEINFO_MONTH, 0, 12, teleinfo_graph.month);
  teleinfo_graph.histo = (uint8_t)TeleinfoWebGetArgValue (D_CMND_TELEINFO_HISTO, 0, 52, teleinfo_graph.histo);
  choice = (uint8_t)TeleinfoWebGetArgValue (D_CMND_TELEINFO_PERIOD, 0, TELEINFO_PERIOD_MAX - 1, teleinfo_graph.period);
  if (choice != teleinfo_graph.period) teleinfo_graph.histo = 0;
  teleinfo_graph.period = choice;  

  // if period is yearly, force data to Wh
  if (teleinfo_graph.period == TELEINFO_PERIOD_YEAR) teleinfo_graph.data = TELEINFO_UNIT_WH;

  // else if back to other curves, switch to W
  else if (teleinfo_graph.data == TELEINFO_UNIT_WH) teleinfo_graph.data = TELEINFO_UNIT_W;

  // check phase display argument
  if (Webserver->hasArg (D_CMND_TELEINFO_PHASE)) WebGetArg (D_CMND_TELEINFO_PHASE, teleinfo_graph.str_phase, sizeof (teleinfo_graph.str_phase));
  while (strlen (teleinfo_graph.str_phase) < teleinfo_contract.phase) strlcat (teleinfo_graph.str_phase, "1", sizeof (teleinfo_graph.str_phase));

  // get graph limits
  teleinfo_config.max_v         = TeleinfoWebGetArgValue (D_CMND_TELEINFO_MAX_V, TELEINFO_GRAPH_MIN_VOLTAGE, TELEINFO_GRAPH_MAX_VOLTAGE, teleinfo_config.max_v);
  teleinfo_config.max_va        = TeleinfoWebGetArgValue (D_CMND_TELEINFO_MAX_VA, TELEINFO_GRAPH_MIN_POWER, TELEINFO_GRAPH_MAX_POWER, teleinfo_config.max_va);
  teleinfo_config.max_kwh_hour  = TeleinfoWebGetArgValue (D_CMND_TELEINFO_MAX_KWH_HOUR, TELEINFO_GRAPH_MIN_WH_HOUR, TELEINFO_GRAPH_MAX_WH_HOUR, teleinfo_config.max_kwh_hour);
  teleinfo_config.max_kwh_day   = TeleinfoWebGetArgValue (D_CMND_TELEINFO_MAX_KWH_DAY, TELEINFO_GRAPH_MIN_WH_DAY, TELEINFO_GRAPH_MAX_WH_DAY, teleinfo_config.max_kwh_day);
  teleinfo_config.max_kwh_month = TeleinfoWebGetArgValue (D_CMND_TELEINFO_MAX_KWH_MONTH, TELEINFO_GRAPH_MIN_WH_MONTH, TELEINFO_GRAPH_MAX_WH_MONTH, teleinfo_config.max_kwh_month);

  // beginning of form without authentification
  WSContentStart_P (D_TELEINFO_GRAPH, false);
  WSContentSend_P (PSTR ("</script>\n"));

  // set page as scalable
  WSContentSend_P (PSTR ("<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=yes'/>\n"));

  // set auto refresh for live data
  if (teleinfo_graph.period == TELEINFO_PERIOD_LIVE) WSContentSend_P (PSTR ("<meta http-equiv='refresh' content='%d'/>\n"), ARR_TELEINFO_PERIOD_WINDOW[teleinfo_graph.period]);

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {margin:0.25rem auto;padding:0.1rem 0px;text-align:center;vertical-align:top;}\n"));
  WSContentSend_P (PSTR ("div a {color:white;}\n"));

  WSContentSend_P (PSTR ("div.live {height:32px;}\n"));
  WSContentSend_P (PSTR ("div.phase {display:inline-block;width:90px;padding:0.2rem;margin:0.2rem;border-radius:8px;}\n"));
  WSContentSend_P (PSTR ("div.phase span.power {font-size:1rem;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.phase span.volt {font-size:0.8rem;font-style:italic;}\n"));
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    GetTextIndexed (str_text, sizeof (str_text), phase, kTeleinfoGraphColorPhase);
    WSContentSend_P (PSTR ("div.ph%d {color:%s;border:1px %s solid;}\n"), phase, str_text, str_text);    
  }
  WSContentSend_P (PSTR ("div.disabled {color:#666;border-color:#666;}\n"));

  WSContentSend_P (PSTR ("div.choice {display:inline-block;padding:0px 2px;margin:0px;border:1px #666 solid;background:none;color:#fff;border-radius:6px;}\n"));
  WSContentSend_P (PSTR ("div.choice div {background:#666;}\n"));
  WSContentSend_P (PSTR ("div.choice a div {background:none;}\n"));

  WSContentSend_P (PSTR ("div.item {display:inline-block;margin:1px 0px;border-radius:4px;}\n"));
  WSContentSend_P (PSTR ("div a div.item:hover {background:#aaa;}\n"));
  WSContentSend_P (PSTR ("div.histo {font-size:0.9rem;padding:0px;margin-top:0px;}\n"));

  WSContentSend_P (PSTR ("div.incr {position:absolute;top:5vh;left:2%%;padding:0px;color:white;border:1px #666 solid;border-radius:6px;}\n"));
  
  WSContentSend_P (PSTR ("div.period {width:60px;margin-top:2px;}\n"));
  WSContentSend_P (PSTR ("div.month {width:28px;font-size:0.85rem;}\n"));
  WSContentSend_P (PSTR ("div.day {width:16px;font-size:0.8rem;margin-top:2px;}\n"));
  
  WSContentSend_P (PSTR ("div.data {width:50px;}\n"));
  WSContentSend_P (PSTR ("div.size {width:25px;}\n"));
  WSContentSend_P (PSTR ("div a div.active {background:#666;}\n"));

  WSContentSend_P (PSTR ("select {background:#666;color:white;font-size:0.8rem;margin:1px;border:solid #666 2px;border-radius:4px;}\n"));

  WSContentSend_P (PSTR ("div.graph {width:100%%;margin:auto;margin-top:2vh;}\n"));
  WSContentSend_P (PSTR ("svg.graph {width:100%%;height:60vh;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // page body
  WSContentSend_P (PSTR ("</head>\n"));
  WSContentSend_P (PSTR ("<body>\n"));

  // -------------------
  //      Unit range
  // -------------------

  WSContentSend_P (PSTR ("<div class='incr'>\n"));

  // graph increment
  if ((teleinfo_graph.period == TELEINFO_PERIOD_YEAR) && (teleinfo_graph.month == 0)) WSContentSend_P (PSTR ("<a href='%s?%s=%d'><div class='item size'>+</div></a><br>\n"), D_TELEINFO_PAGE_GRAPH, D_CMND_TELEINFO_MAX_KWH_MONTH, teleinfo_config.max_kwh_month + TELEINFO_GRAPH_INC_WH_MONTH);
  else if ((teleinfo_graph.period == TELEINFO_PERIOD_YEAR) && (teleinfo_graph.day == 0)) WSContentSend_P (PSTR ("<a href='%s?%s=%d'><div class='item size'>+</div></a><br>\n"), D_TELEINFO_PAGE_GRAPH, D_CMND_TELEINFO_MAX_KWH_DAY, teleinfo_config.max_kwh_day + TELEINFO_GRAPH_INC_WH_DAY);
  else if (teleinfo_graph.period == TELEINFO_PERIOD_YEAR) WSContentSend_P (PSTR ("<a href='%s?%s=%d'><div class='item size'>+</div></a><br>\n"), D_TELEINFO_PAGE_GRAPH, D_CMND_TELEINFO_MAX_KWH_HOUR, teleinfo_config.max_kwh_hour + TELEINFO_GRAPH_INC_WH_HOUR);
  else if ((teleinfo_graph.data == TELEINFO_UNIT_VA) || (teleinfo_graph.data == TELEINFO_UNIT_W)) WSContentSend_P (PSTR ("<a href='%s?%s=%d'><div class='item size'>+</div></a><br>\n"), D_TELEINFO_PAGE_GRAPH, D_CMND_TELEINFO_MAX_VA, teleinfo_config.max_va + TELEINFO_GRAPH_INC_POWER);
  else if (teleinfo_graph.data == TELEINFO_UNIT_V) WSContentSend_P (PSTR ("<a href='%s?%s=%d'><div class='item size'>+</div></a><br>\n"), D_TELEINFO_PAGE_GRAPH, D_CMND_TELEINFO_MAX_V, teleinfo_config.max_v + TELEINFO_GRAPH_INC_VOLTAGE);

  // graph decrement
  if ((teleinfo_graph.period == TELEINFO_PERIOD_YEAR) && (teleinfo_graph.month == 0)) WSContentSend_P (PSTR ("<a href='%s?%s=%d'><div class='item size'>-</div></a><br>\n"), D_TELEINFO_PAGE_GRAPH, D_CMND_TELEINFO_MAX_KWH_MONTH, teleinfo_config.max_kwh_month - TELEINFO_GRAPH_INC_WH_MONTH);
  else if ((teleinfo_graph.period == TELEINFO_PERIOD_YEAR) && (teleinfo_graph.day == 0)) WSContentSend_P (PSTR ("<a href='%s?%s=%d'><div class='item size'>-</div></a><br>\n"), D_TELEINFO_PAGE_GRAPH, D_CMND_TELEINFO_MAX_KWH_DAY, teleinfo_config.max_kwh_day - TELEINFO_GRAPH_INC_WH_DAY);
  else if (teleinfo_graph.period == TELEINFO_PERIOD_YEAR) WSContentSend_P (PSTR ("<a href='%s?%s=%d'><div class='item size'>-</div></a><br>\n"), D_TELEINFO_PAGE_GRAPH, D_CMND_TELEINFO_MAX_KWH_HOUR, teleinfo_config.max_kwh_hour - TELEINFO_GRAPH_INC_WH_HOUR);
  else if ((teleinfo_graph.data == TELEINFO_UNIT_VA) || (teleinfo_graph.data == TELEINFO_UNIT_W)) WSContentSend_P (PSTR ("<a href='%s?%s=%d'><div class='item size'>-</div></a><br>\n"), D_TELEINFO_PAGE_GRAPH, D_CMND_TELEINFO_MAX_VA, teleinfo_config.max_va - TELEINFO_GRAPH_INC_POWER);
  else if (teleinfo_graph.data == TELEINFO_UNIT_V) WSContentSend_P (PSTR ("<a href='%s?%s=%d'><div class='item size'>-</div></a><br>\n"), D_TELEINFO_PAGE_GRAPH, D_CMND_TELEINFO_MAX_V, teleinfo_config.max_v - TELEINFO_GRAPH_INC_VOLTAGE);

  WSContentSend_P (PSTR ("</div>\n"));      // choice

  // -----------------
  //      Icon
  // -----------------

  WSContentSend_P (PSTR ("<div><a href='/'><img height=64 src='%s'></a></div>\n"), D_TELEINFO_ICON_PNG);

  // ---------------------
  //    Level 1 - Period 
  // ---------------------

  WSContentSend_P (PSTR ("<div><div class='choice'>\n"));
  WSContentSend_P (PSTR ("<form method='post' action='%s'>\n"), D_TELEINFO_PAGE_GRAPH);
  for (index = 0; index < TELEINFO_PERIOD_DAY; index++)
  {
    // get period label
    GetTextIndexed (str_text, sizeof (str_text), index, kTeleinfoGraphPeriod);

    // set button according to active state
    if (teleinfo_graph.period != index) WSContentSend_P (PSTR ("<a href='%s?period=%d'>"), D_TELEINFO_PAGE_GRAPH, index);
    WSContentSend_P (PSTR ("<div class='item period'>%s</div>"), str_text);
    if (teleinfo_graph.period != index) WSContentSend_P (PSTR ("</a>"));
    WSContentSend_P (PSTR ("\n"));
  }

#ifdef USE_UFILESYS
  char str_filename[UFS_FILENAME_SIZE];

  for (index = TELEINFO_PERIOD_DAY; index < TELEINFO_PERIOD_MAX; index++)
  {
    // get period label
    GetTextIndexed (str_text, sizeof (str_text), index, kTeleinfoGraphPeriod);

    // set button according to active state
    if (teleinfo_graph.period == index)
    {
      // get number of saved periods
      if (teleinfo_graph.period == TELEINFO_PERIOD_DAY) choice = teleinfo_config.log_nbday;
      else if (teleinfo_graph.period == TELEINFO_PERIOD_WEEK) choice = teleinfo_config.log_nbweek;
      else if (teleinfo_graph.period == TELEINFO_PERIOD_YEAR) choice = 20;
      else choice = 0;

      WSContentSend_P (PSTR ("<select name='histo' id='histo' onchange='this.form.submit();'>\n"));

      for (counter = 0; counter < choice; counter++)
      {
        // check if file exists
        if (TeleinfoHistoGetFilename (teleinfo_graph.period, counter, str_filename))
        {
          TeleinfoHistoGetDate (teleinfo_graph.period, counter, str_date);
          if (counter == teleinfo_graph.histo) strcpy_P (str_text, PSTR (" selected")); 
            else strcpy (str_text, "");
          WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>\n"), counter, str_text, str_date);
        }
      }

      WSContentSend_P (PSTR ("</select>\n"));      
    }
    else WSContentSend_P (PSTR ("<a href='%s?period=%d&histo=%d'><div class='item period'>%s</div></a>\n"), D_TELEINFO_PAGE_GRAPH, index, teleinfo_graph.histo, str_text);
  }

# endif     // USE_UFILESYS

  WSContentSend_P (PSTR ("</form>\n"));
  WSContentSend_P (PSTR ("</div></div>\n"));        // choice


  WSContentSend_P (PSTR ("<div><div class='choice'>\n"));

  // ------------------------
  //    Level 2 : Data type
  // ------------------------

  if (teleinfo_graph.period <= TELEINFO_PERIOD_WEEK)
  {
    for (index = 0; index <= TELEINFO_UNIT_COSPHI; index++)
    {
      // get unit label
      GetTextIndexed (str_text, sizeof (str_text), index, kTeleinfoGraphDisplay);

      // display selection
      if (teleinfo_graph.data != index) WSContentSend_P (PSTR ("<a href='%s?data=%d'>"), D_TELEINFO_PAGE_GRAPH, index);
      WSContentSend_P (PSTR ("<div class='item data'>%s</div>"), str_text);
      if (teleinfo_graph.data != index) WSContentSend_P (PSTR ("</a>"));
      WSContentSend_P (PSTR ("\n"));
    }
  }

  // ---------------------
  //    Level 2 : Months 
  // ---------------------

  else if (teleinfo_graph.period == TELEINFO_PERIOD_YEAR)
  {
    for (counter = 1; counter <= 12; counter++)
    {
      // get month name
      GetTextIndexed (str_date, sizeof (str_date), counter, kTeleinfoYearMonth);

      // handle selected month
      strcpy (str_text, "");
      index = counter;
      if (teleinfo_graph.month == counter)
      {
        strcpy_P (str_text, PSTR (" active"));
        if ((teleinfo_graph.month != 0) && (teleinfo_graph.day == 0)) index = 0;
      }

      // display month selection
      WSContentSend_P (PSTR ("<a href='%s?month=%u&day=0'><div class='item month%s'>%s</div></a>\n"), D_TELEINFO_PAGE_GRAPH, index, str_text, str_date);       
    }
  }

  WSContentSend_P (PSTR ("</div></div>\n"));      // choice

  WSContentSend_P (PSTR ("<div class='live'>\n"));

  // --------------------------
  //    Level 3 : Live Values 
  // --------------------------

  if (teleinfo_graph.period != TELEINFO_PERIOD_YEAR)
  {
    for (phase = 0; phase < teleinfo_contract.phase; phase++)
    {
      // display phase inverted link
      strlcpy (str_text, teleinfo_graph.str_phase, sizeof (str_text));
      display = (str_text[phase] != '0');
      if (display) str_text[phase] = '0'; 
        else str_text[phase] = '1';
      WSContentSend_P (PSTR ("<a href='%s?phase=%s'>"), D_TELEINFO_PAGE_GRAPH, str_text);

      // display phase data
      if (display) strcpy (str_text, ""); 
        else strcpy_P (str_text, PSTR (" disabled"));
      WSContentSend_P (PSTR ("<div class='phase ph%u%s'>"), phase, str_text);    
      TeleinfoDisplayCurrentValue (teleinfo_graph.data, phase, str_text, sizeof (str_text));
      WSContentSend_P (PSTR ("<span class='power' id='p%u'>%s</span>"), phase + 1, str_text);
      WSContentSend_P (PSTR ("</div></a>\n"));
    }
  }

  // --------------------
  //    Level 3 : Days 
  // --------------------

  else if ((teleinfo_graph.period == TELEINFO_PERIOD_YEAR) && (teleinfo_graph.month != 0))
  {
    // calculate current year
    BreakTime (LocalTime (), time_dst);
    year = 1970 + (uint32_t)time_dst.year - teleinfo_graph.histo;

    // calculate number of days in current month
    if ((teleinfo_graph.month == 4) || (teleinfo_graph.month == 11) || (teleinfo_graph.month == 9) || (teleinfo_graph.month == 6)) choice = 30;     // months with 30 days  
    else if (teleinfo_graph.month != 2) choice = 31;                                                                                                // months with 31 days
    else if ((year % 400) == 0) choice = 29;                                                                                                        // leap year
    else if ((year % 100) == 0) choice = 28;                                                                                                        // not a leap year
    else if ((year % 4) == 0) choice = 29;                                                                                                          // leap year
    else choice = 28;                                                                                                                               // not a leap year 

    WSContentSend_P (PSTR ("<div class='choice'>\n"));

    // loop thru days in the month
    for (counter = 1; counter <= choice; counter++)
    {
      // handle selected day
      strcpy (str_text, "");
      index = counter;
      if (teleinfo_graph.day == counter) strcpy_P (str_text, PSTR (" active"));
      if ((teleinfo_graph.day == counter) && (teleinfo_graph.day != 0)) index = 0;

      // display day selection
      WSContentSend_P (PSTR ("<a href='%s?day=%u'><div class='item day%s'>%u</div></a>\n"), D_TELEINFO_PAGE_GRAPH, index, str_text, counter);
    }

    WSContentSend_P (PSTR ("</div>\n"));      // choice
  }

  WSContentSend_P (PSTR ("</div>\n"));        // live

  // ---------------
  //   SVG : Start 
  // ---------------

  WSContentSend_P (PSTR ("<div class='graph'>\n<svg class='graph' viewBox='0 0 1200 %d' preserveAspectRatio='none'>\n"), teleinfo_config.height);

  // ---------------
  //   SVG : Style 
  // ---------------

  WSContentSend_P (PSTR ("<style type='text/css'>\n"));

  WSContentSend_P (PSTR ("rect {stroke:grey;fill:none;}\n"));
  WSContentSend_P (PSTR ("line.dash {stroke:grey;stroke-width:1;stroke-dasharray:2 8;}\n"));
  WSContentSend_P (PSTR ("text.power {font-size:1.2rem;fill:white;}\n"));

  // phase colors
  WSContentSend_P (PSTR ("path {stroke-width:1.5;opacity:0.8;fill-opacity:0.6;}\n"));
  for (phase = 0; phase < teleinfo_contract.phase; phase++) 
  {
    // phase colors
    GetTextIndexed (str_text, sizeof (str_text), phase, kTeleinfoGraphColorPhase);
    WSContentSend_P (PSTR ("path.ph%d {stroke:%s;fill:%s;}\n"), phase, str_text, str_text);
    WSContentSend_P (PSTR ("path.ln%d {stroke:%s;fill:none;}\n"), phase, str_text);

    // peak colors
    GetTextIndexed (str_text, sizeof (str_text), phase, kTeleinfoGraphColorPeak);
    WSContentSend_P (PSTR ("path.pk%d {stroke:%s;fill:none;stroke-dasharray:1 3;}\n"), phase, str_text);
  }

  // bar graph
  WSContentSend_P (PSTR ("path.now {stroke:#6cf;fill:#6cf;}\n"));
  if (teleinfo_graph.day == 0) WSContentSend_P (PSTR ("path.now:hover {fill-opacity:0.8;}\n"));
  WSContentSend_P (PSTR ("path.prev {stroke:#069;fill:#069;fill-opacity:1;}\n"));

  // text
  WSContentSend_P (PSTR ("text {fill:white;text-anchor:middle;dominant-baseline:middle;}\n"));
  WSContentSend_P (PSTR ("text.value {font-style:italic;}}\n"));
  WSContentSend_P (PSTR ("text.time {font-size:1.2rem;}\n"));
  WSContentSend_P (PSTR ("text.month {font-size:1.2rem;}\n"));
  WSContentSend_P (PSTR ("text.day {font-size:0.9rem;}\n"));
  WSContentSend_P (PSTR ("text.hour {font-size:1rem;}\n"));

  // time line
  WSContentSend_P (PSTR ("line.time {stroke-width:1;stroke:white;stroke-dasharray:1 8;}\n"));

  WSContentSend_P (PSTR ("</style>\n"));

  // ----------------
  //   SVG : Curves
  // ----------------

#ifdef USE_UFILESYS
  // if data is for current period file, force flush for the period
  if ((teleinfo_graph.histo == 0) && (teleinfo_graph.period != TELEINFO_PERIOD_LIVE)) TeleinfoFlushHistoData (teleinfo_graph.period);

  // if dealing with yearly data, display bar graph
  if (teleinfo_graph.period == TELEINFO_PERIOD_YEAR)
  {
    TeleinfoWebDisplayBarGraph (teleinfo_graph.histo + 1, false);       // previous period
    TeleinfoWebDisplayBarGraph (teleinfo_graph.histo,     true);        // current period
  }

  else
#endif    // USE_UFILESYS

  // loop thru phases to display curves
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
    if (teleinfo_graph.str_phase[phase] == '1') 
    {
      // display according to data type
      switch (teleinfo_graph.data) 
      {
        // Apparent Power
        case TELEINFO_UNIT_VA:
          TeleinfoWebDisplayCurve (phase, TELEINFO_UNIT_PEAK_VA);
          TeleinfoWebDisplayCurve (phase, TELEINFO_UNIT_VA);
          break;
          
        // Active Power
        case TELEINFO_UNIT_W:
          TeleinfoWebDisplayCurve (phase, TELEINFO_UNIT_W);
          break;

        // Voltage
        case TELEINFO_UNIT_V:
          TeleinfoWebDisplayCurve (phase, TELEINFO_UNIT_PEAK_V);
          TeleinfoWebDisplayCurve (phase, TELEINFO_UNIT_V);
          break;
        
        // Cos Phi
        case TELEINFO_UNIT_COSPHI:
          TeleinfoWebDisplayCurve (phase, TELEINFO_UNIT_COSPHI);
          break;
      }
    }

  // ---------------
  //   SVG : Frame
  // ---------------

  TeleinfoWebGraphFrame ();

  // --------------
  //   SVG : Time 
  // --------------

  TeleinfoWebGraphTime (teleinfo_graph.period, teleinfo_graph.histo);

  // -----------------
  //   SVG : End 
  // -----------------

  WSContentSend_P (PSTR ("</svg>\n</div>\n"));

  // end of page
  WSContentStop ();
}

#endif  // USE_WEBSERVER

/***************************************\
 *              Interface
\***************************************/

// Teleinfo energy driver
bool Xnrg15 (uint8_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
    case FUNC_PRE_INIT:
      TeleinfoPreInit ();
      break;
    case FUNC_INIT:
      TeleinfoInit ();
      TeleinfoEnableSerial ();
      break;
  }

  return result;
}

// Teleinfo sensor
bool Xsns99 (uint8_t function)
{
  bool result = false;

  // swtich according to context
  switch (function) 
  {
    case FUNC_INIT:
      TeleinfoGraphInit ();
      break;
    case FUNC_SAVE_BEFORE_RESTART:
      TeleinfoSaveBeforeRestart ();
      break;
    case FUNC_AFTER_TELEPERIOD:
      if (teleinfo_meter.status_rx == TIC_SERIAL_ACTIVE) TeleinfoAfterTeleperiod ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kTeleinfoCommands, TeleinfoCommand);
      break;
    case FUNC_LOOP:
      if (teleinfo_meter.status_rx == TIC_SERIAL_ACTIVE) TeleinfoReceiveData ();
      break;
    case FUNC_EVERY_250_MSECOND:
      if (TasmotaGlobal.uptime > 5) TeleinfoEvery250ms ();
      break;
    case FUNC_EVERY_SECOND:
      if (TasmotaGlobal.uptime > 5) TeleinfoEverySecond ();
      break;

#ifdef USE_UFILESYS
    case FUNC_SAVE_AT_MIDNIGHT:
      TeleinfoSaveDailyTotal ();
      TeleinfoRotateLog ();
      break;
#endif      // USE_UFILESYS

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      // config
      Webserver->on (FPSTR (D_TELEINFO_PAGE_CONFIG), TeleinfoWebPageConfigure);

      // TIC message
      Webserver->on (FPSTR (D_TELEINFO_PAGE_TIC), TeleinfoWebPageTic);
      Webserver->on (FPSTR (D_TELEINFO_PAGE_TIC_UPD), TeleinfoWebTicUpdate);

      // graph
      Webserver->on (FPSTR (D_TELEINFO_PAGE_GRAPH),TeleinfoWebGraphPage);

      // icons
      Webserver->on (FPSTR (D_TELEINFO_ICON_PNG), TeleinfoWebIconTic);
      break;
    case FUNC_WEB_ADD_BUTTON:
      TeleinfoWebConfigButton ();
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      TeleinfoWebMainButton ();
      break;
    case FUNC_WEB_SENSOR:
      TeleinfoWebSensor ();
      break;
#endif  // USE_WEBSERVER

  }

  return result;
}

#endif      // USE_TELEINFO
#endif      // USE_ENERGY_SENSOR
