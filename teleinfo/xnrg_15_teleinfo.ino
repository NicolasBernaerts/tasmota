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
    20/11/2020 - v6.2   - Correct checksum bug
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
#define TELEINFO_VOLTAGE_MINIMUM        100       // minimum voltage for value to be acceptable
#define TELEINFO_VOLTAGE_REF            200       // voltage reference for max power calculation
#define TELEINFO_VOLTAGE_LOW            220       // minimum acceptable voltage
#define TELEINFO_VOLTAGE_HIGH           240       // maximum acceptable voltage
#define TELEINFO_INDEX_MAX              12        // maximum number of total power counters
#define TELEINFO_INTERVAL_MIN           1         // energy calculation minimum interval (1mn)
#define TELEINFO_INTERVAL_MAX           720       // energy calculation maximum interval (12h)
#define TELEINFO_PERCENT_MIN            1         // minimum acceptable percentage of energy contract
#define TELEINFO_PERCENT_MAX            200       // maximum acceptable percentage of energy contract
#define TELEINFO_PERCENT_CHANGE         5         // 5% of power change to publish JSON
#define TELEINFO_DAILY_DEFAULT          7         // default number of daily files
#define TELEINFO_WEEKLY_DEFAULT         8         // default number of weekly files

// graph data
#define TELEINFO_GRAPH_WIDTH            1200      // graph width
#define TELEINFO_GRAPH_HEIGHT           600       // default graph height
#define TELEINFO_GRAPH_STEP             200       // graph height mofification step
#define TELEINFO_GRAPH_PERCENT_START    5         // start position of graph window
#define TELEINFO_GRAPH_PERCENT_STOP     95        // stop position of graph window
#define TELEINFO_GRAPH_POWER_MAX        3000      // maximum power
#define TELEINFO_GRAPH_VOLTAGE_MIN      200       // minimum voltage
#define TELEINFO_GRAPH_VOLTAGE_MAX      250       // maximum voltage
#define TELEINFO_GRAPH_VOLTAGE_RANGE    40        // graph voltage range
#define TELEINFO_GRAPH_VOLTAGE_STEP     10        // voltage increment/decrement step

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

// LITTLEFS specificities
#ifdef USE_UFILESYS
  #define TELEINFO_MEMORY_PERIOD_MAX    1         // memory graph for LIVE only
  #define TELEINFO_GRAPH_SAMPLE         600       // number of samples per graph data (should divide 3600)
#else
  #define TELEINFO_MEMORY_PERIOD_MAX    3         // memory graph for LIVE, DAY and WEEK
  #define TELEINFO_GRAPH_SAMPLE         300       // number of samples per graph data (should divide 3600)
#endif    // USE_UFILESYS

// commands : MQTT
#define D_CMND_TELEINFO_ENABLE          "enable"
#define D_CMND_TELEINFO_RATE            "rate"
#define D_CMND_TELEINFO_LOG_POLICY      "log"
#define D_CMND_TELEINFO_MSG_POLICY      "msgp"
#define D_CMND_TELEINFO_MSG_TYPE        "msgt"
#define D_CMND_TELEINFO_INTERVAL        "ival"
#define D_CMND_TELEINFO_ADJUST          "adj"
#define D_CMND_TELEINFO_ROTATE          "rot"
#define D_CMND_TELEINFO_MINIMIZE        "mini"

#define D_CMND_TELEINFO_NBDAILY         "daily"
#define D_CMND_TELEINFO_NBWEEKLY        "weekly"

// commands : Web
#define D_CMND_TELEINFO_MODE            "mode"
#define D_CMND_TELEINFO_ETH             "eth"
#define D_CMND_TELEINFO_PHASE           "phase"
#define D_CMND_TELEINFO_PERIOD          "period"
#define D_CMND_TELEINFO_HISTO           "histo"
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

// configuration file
#define D_TELEINFO_CFG                  "teleinfo.cfg"

// history files label and filename
const char D_TELEINFO_HISTO_DAY[]         PROGMEM = "day";
const char D_TELEINFO_HISTO_WEEK[]        PROGMEM = "week";
const char D_TELEINFO_HISTO_FILE_DAILY[]  PROGMEM = "/day-%d.csv";
const char D_TELEINFO_HISTO_FILE_WEEKLY[] PROGMEM = "/week-%d.csv";

#endif    // USE_UFILESYS

// web URL
const char D_TELEINFO_PAGE_CONFIG[]       PROGMEM = "/tic-cfg";
const char D_TELEINFO_PAGE_TIC[]          PROGMEM = "/tic-msg";
const char D_TELEINFO_PAGE_TIC_UPD[]      PROGMEM = "/tic-msg.upd";
const char D_TELEINFO_PAGE_GRAPH[]        PROGMEM = "/tic-graph";
const char D_TELEINFO_PAGE_GRAPH_FRAME[]  PROGMEM = "/tic-base.svg";
const char D_TELEINFO_PAGE_GRAPH_DATA[]   PROGMEM = "/tic-data.svg";
const char D_TELEINFO_PAGE_GRAPH_UPD[]    PROGMEM = "/tic-graph.upd";
const char D_TELEINFO_PAGE_TIC_PNG[]      PROGMEM = "/tic.png";

// web strings
const char D_TELEINFO_CONFIG[]            PROGMEM = "Configure Teleinfo";
const char TELEINFO_INPUT_RADIO[]         PROGMEM = "<p><input type='radio' name='%s' value='%d' %s>%s</p>\n";
const char TELEINFO_INPUT_CHECKBOX[]      PROGMEM = "<p><input type='checkbox' name='%s' %s>%s</p>\n";
const char TELEINFO_INPUT_NUMBER[]        PROGMEM = "<p>%s<br><input type='number' name='%s' min='%d' max='%d' step='1' value='%d'></p>\n";
const char TELEINFO_FIELD_START[]         PROGMEM = "<p><fieldset><legend><b>&nbsp;%s %s&nbsp;</b></legend>\n";
const char TELEINFO_FIELD_STOP[]          PROGMEM = "</fieldset></p>\n<br>\n";
const char TELEINFO_HTML_POWER[]          PROGMEM = "<text class='power' x='%d%%' y='%d%%'>%s</text>\n";
const char TELEINFO_HTML_DASH[]           PROGMEM = "<line class='dash' x1='%d%%' y1='%d%%' x2='%d%%' y2='%d%%' />\n";
const char TELEINFO_HTML_BAR[]            PROGMEM = "<tr><div style='margin:4px;padding:0px;background-color:#ddd;border-radius:4px;'><div style='font-size:0.75rem;font-weight:bold;padding:0px;text-align:center;border:1px solid #bbb;border-radius:4px;color:#444;background-color:%s;width:%d%%;'>%d%%</div></div></tr>\n";

// TIC - MQTT commands : TICenable, TICrate, TICtele, TICmeter, TICival, TICdelta, TICadj and TICtcp
enum TeleinfoCommands { TIC_CMND_NONE, TIC_CMND_ENABLE, TIC_CMND_RATE, TIC_CMND_POLICYTIC, TIC_CMND_POLICYMETER, TIC_CMND_POLICYLOG, TIC_CMND_INTERVAL, TIC_CMND_ADJUST, TIC_CMND_NBDAILY, TIC_CMND_NBWEEKLY };
const char kTeleinfoCommands[]            PROGMEM = "tic_" "|" D_CMND_TELEINFO_ENABLE "|" D_CMND_TELEINFO_RATE "|" D_CMND_TELEINFO_MSG_POLICY "|" D_CMND_TELEINFO_MSG_TYPE "|" D_CMND_TELEINFO_LOG_POLICY "|" D_CMND_TELEINFO_INTERVAL "|" D_CMND_TELEINFO_ADJUST "|" D_CMND_TELEINFO_ROTATE;
void (* const TeleinfoCommand[])(void)    PROGMEM = { &CmndTeleinfoEnable, &CmndTeleinfoRate, &CmndTeleinfoMessagePolicy, &CmndTeleinfoMessageType, &CmndTeleinfoLogPolicy, &CmndTeleinfoInterval, &CmndTeleinfoAdjust, &CmndTeleinfoRotate };

// TIC - specific etiquettes
enum TeleinfoEtiquette { TIC_NONE, TIC_ADCO, TIC_ADSC, TIC_PTEC, TIC_NGTF, TIC_EAIT, TIC_IINST, TIC_IINST1, TIC_IINST2, TIC_IINST3, TIC_ISOUSC, TIC_PS, TIC_PAPP, TIC_SINSTS, TIC_BASE, TIC_EAST, TIC_HCHC, TIC_HCHP, TIC_EJPHN, TIC_EJPHPM, TIC_BBRHCJB, TIC_BBRHPJB, TIC_BBRHCJW, TIC_BBRHPJW, TIC_BBRHCJR, TIC_BBRHPJR, TIC_ADPS, TIC_ADIR1, TIC_ADIR2, TIC_ADIR3, TIC_URMS1, TIC_URMS2, TIC_URMS3, TIC_UMOY1, TIC_UMOY2, TIC_UMOY3, TIC_IRMS1, TIC_IRMS2, TIC_IRMS3, TIC_SINSTS1, TIC_SINSTS2, TIC_SINSTS3, TIC_PTCOUR, TIC_PTCOUR1, TIC_PREF, TIC_PCOUP, TIC_LTARF, TIC_EASF01, TIC_EASF02, TIC_EASF03, TIC_EASF04, TIC_EASF05, TIC_EASF06, TIC_EASF07, TIC_EASF08, TIC_EASF09, TIC_EASF10, TIC_ADS, TIC_CONFIG, TIC_EAPS, TIC_EAS, TIC_EAPPS, TIC_PREAVIS, TIC_MAX };
const char kTeleinfoEtiquetteName[] PROGMEM = "|ADCO|ADSC|PTEC|NGTF|EAIT|IINST|IINST1|IINST2|IINST3|ISOUSC|PS|PAPP|SINSTS|BASE|EAST|HCHC|HCHP|EJPHN|EJPHPM|BBRHCJB|BBRHPJB|BBRHCJW|BBRHPJW|BBRHCJR|BBRHPJR|ADPS|ADIR1|ADIR2|ADIR3|URMS1|URMS2|URMS3|UMOY1|UMOY2|UMOY3|IRMS1|IRMS2|IRMS3|SINSTS1|SINSTS2|SINSTS3|PTCOUR|PTCOUR1|PREF|PCOUP|LTARF|EASF01|EASF02|EASF03|EASF04|EASF05|EASF06|EASF07|EASF08|EASF09|EASF10|ADS|CONFIG|EAP_s|EA_s|EAPP_s|PREAVIS";

// TIC - modes and rates
enum TeleinfoMode { TIC_MODE_UNDEFINED, TIC_MODE_HISTORIC, TIC_MODE_STANDARD, TIC_MODE_CONSO, TIC_MODE_PROD };
const char kTeleinfoModeName[] PROGMEM = "|Historique|Standard|PME Conso|PME Prod";
const uint16_t ARR_TELEINFO_RATE[] = { 1200, 9600, 19200 }; 

// TIC - tarifs                              [  Toutes   ] [ Creuses       Pleines   ] [ Normales   PointeMobile ] [CreusesBleu  CreusesBlanc  CreusesRouge  PleinesBleu   PleinesBlanc  PleinesRouge] [ Pointe   PointeMobile  Hiver      Pleines     Creuses    PleinesHiver CreusesHiver PleinesEte   CreusesEte   Pleines1/2S  Creuses1/2S  JuilletAout] [Pointe PleinesHiver CreusesHiver PleinesEte CreusesEte] [ Base  ]  [ Pleines    Creuses   ]
enum TeleinfoPeriod                        { TIC_HISTO_TH, TIC_HISTO_HC, TIC_HISTO_HP, TIC_HISTO_HN, TIC_HISTO_PM, TIC_HISTO_CB, TIC_HISTO_CW, TIC_HISTO_CR, TIC_HISTO_PB, TIC_HISTO_PW, TIC_HISTO_PR, TIC_STD_P, TIC_STD_PM, TIC_STD_HH, TIC_STD_HP, TIC_STD_HC, TIC_STD_HPH, TIC_STD_HCH, TIC_STD_HPE, TIC_STD_HCE, TIC_STD_HPD, TIC_STD_HCD, TIC_STD_JA, TIC_STD_1, TIC_STD_2, TIC_STD_3, TIC_STD_4, TIC_STD_5, TIC_STD_BASE, TIC_STD_HPL, TIC_STD_HCR, TIC_PERIOD_MAX };
const int ARR_TELEINFO_PERIOD_FIRST[]    = { TIC_HISTO_TH, TIC_HISTO_HC, TIC_HISTO_HC, TIC_HISTO_HN, TIC_HISTO_HN, TIC_HISTO_CB, TIC_HISTO_CB, TIC_HISTO_CB, TIC_HISTO_CB, TIC_HISTO_CB, TIC_HISTO_CB, TIC_STD_P, TIC_STD_P,  TIC_STD_P,  TIC_STD_P,  TIC_STD_P,  TIC_STD_P,   TIC_STD_P,   TIC_STD_P,   TIC_STD_P,   TIC_STD_P,   TIC_STD_P,   TIC_STD_P,  TIC_STD_1, TIC_STD_1, TIC_STD_1, TIC_STD_1, TIC_STD_1, TIC_STD_BASE, TIC_STD_HPL, TIC_STD_HPL };
const int ARR_TELEINFO_PERIOD_NUMBER[]   = { 1,            2,            2,            2,            2,            6,            6,            6,            6,            6,            6,            12,        12,         12,         12,         12,         12,          12,          12,          12,          12,          12,          12,         5,         5,         5,         5,         5,         1,            2,           2           };
const char kTeleinfoPeriod[] PROGMEM     = "TH..|HC..|HP..|HN..|PM..|HCJB|HCJW|HCJR|HPJB|HPJW|HPJR|P|PM|HH|HP|HC|HPH|HCH|HPE|HCE|HPD|HCD|JA|1|2|3|4|5|BASE|HEURE PLEINE|HEURE CREUSE";
const char kTeleinfoPeriodName[] PROGMEM = "Toutes|Creuses|Pleines|Normales|Pointe Mobile|Creuses Bleu|Creuses Blanc|Creuses Rouge|Pleines Bleu|Pleines Blanc|Pleines Rouge|Pointe|Pointe Mobile|Hiver|Pleines|Creuses|Pleines Hiver|Creuses Hiver|Pleines Ete|Creuses Ete|Pleines Demi-saison|Creuses Demi-saison|Juillet-Aout|Pointe|Pleines Hiver|Creuses Hiver|Pleines Ete|Creuses Ete|Base|Pleines|Creuses";

// TIC - data diffusion policy
enum TeleinfoMessagePolicy { TELEINFO_POLICY_NEVER, TELEINFO_POLICY_MESSAGE, TELEINFO_POLICY_PERCENT, TELEINFO_POLICY_TELEMETRY, TELEINFO_POLICY_MAX };
const char kTeleinfoMessagePolicy[] PROGMEM = "Never|Every TIC message|When Power fluctuates (± 5%)|With Telemetry only";
enum TeleinfoMessageType { TELEINFO_TYPE_NONE, TELEINFO_TYPE_METER, TELEINFO_TYPE_TIC, TELEINFO_TYPE_BOTH, TELEINFO_TYPE_MAX };
const char kTeleinfoMessageType[] PROGMEM = "None|METER only|TIC only|METER and TIC";

// graph - periods
enum TeleinfoGraphPeriod { TELEINFO_PERIOD_LIVE, TELEINFO_PERIOD_DAY, TELEINFO_PERIOD_WEEK, TELEINFO_PERIOD_MAX };                              // available graph periods
const char kTeleinfoGraphPeriod[] PROGMEM = "Live|Day|Week";                                                                                    // period labels
const long ARR_TELEINFO_PERIOD_WINDOW[] = { 3600 / TELEINFO_GRAPH_SAMPLE, 86400 / TELEINFO_GRAPH_SAMPLE, 604800 / TELEINFO_GRAPH_SAMPLE };      // time window between samples (sec.)
const long ARR_TELEINFO_PERIOD_FLUSH[]  = { 1, 3600, 21600 };                                                                                   // max time between log flush (sec.)

// graph - display
enum TeleinfoGraphDisplay { TELEINFO_UNIT_VA, TELEINFO_UNIT_W, TELEINFO_UNIT_V, TELEINFO_UNIT_COSPHI, TELEINFO_UNIT_MAX };                                                      // available graph displays
const char kTeleinfoGraphDisplay[] PROGMEM = "VA|W|V|cosφ";                                                                                                                     // data display labels
enum TeleinfoHistoDisplay { TELEINFO_HISTO_VA, TELEINFO_HISTO_W, TELEINFO_HISTO_V, TELEINFO_HISTO_COSPHI, TELEINFO_HISTO_PEAK_VA, TELEINFO_HISTO_PEAK_V, TELEINFO_HISTO_MAX };  // available graph displays

// graph - phase colors
enum TeleinfoGraphColor { TELEINFO_COLOR_BLUE, TELEINFO_COLOR_ORANGE, TELEINFO_COLOR_GREEN };        // available graph colors
const char kTeleinfoGraphColorPhase[] PROGMEM = "#6bc4ff|#ffca74|#7a7fb3";                                                                                                                     // data display labels
const char kTeleinfoGraphColorPeak[]  PROGMEM = "#5dade2|#d9ad67|#64d394";                                                                                                                     // data display labels

// week days name for graph
const char kTeleinfoWeekDay[] PROGMEM = "Sun|Mon|Tue|Wed|Thu|Fri|Sat";                                                                                                                     // data display labels

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
  uint16_t baud_rate   = 1200;                      // meter transmission rate (bauds)
  int  log_policy      = TELEINFO_LOG_BUFFERED;     // log writing policy
  int  msg_policy      = TELEINFO_POLICY_TELEMETRY; // publishing policy for data
  int  msg_type        = TELEINFO_TYPE_METER;       // type of data to publish (METER and/or TIC)
  int  update_interval = 720;                       // update interval
  int  calc_delta      = 1;                         // minimum delta for power calculation
  int  percent_adjust  = 100;                       // percentage adjustment to maximum contract power
  int  nb_daily        = TELEINFO_HISTO_DAY_MAX;    // number of daily historisation files
  int  nb_weekly       = TELEINFO_HISTO_WEEK_MAX;   // number of weekly historisation files
  int  height          = TELEINFO_GRAPH_HEIGHT;     // graph height in pixels
} teleinfo_config;

// power calculation structure
static struct {
  uint8_t   method = TIC_METHOD_GLOBAL_COUNTER;         // power calculation method
  float     papp_inc[ENERGY_MAX_PHASES];                // apparent power counter increment per phase (in vah)
  long      papp_current_counter    = LONG_MAX;         // current apparent power counter (in vah)
  long      papp_previous_counter   = LONG_MAX;         // previous apparent power counter (in vah)
  long      papp_previous_increment = 0;                // previous apparent power increment (in vah)
  long      pact_current_counter    = LONG_MAX;         // current active power counter (in wh)
  long      pact_previous_counter   = LONG_MAX;         // previous active power counter (in wh)
  long      pact_previous_increment = 0;                // previous active power increment (in wh)
  long long previous_counter        = LONG_LONG_MAX;    // previous global counter start (in wh)
  uint32_t  previous_time_message   = UINT32_MAX;       // timestamp of previous message
  uint32_t  previous_time_counter   = UINT32_MAX;       // timestamp of previous global counter increase
} teleinfo_calc; 

// TIC : current line being received
static struct {
  char    separator;                                // detected separator
  char    buffer[TELEINFO_LINE_MAX];                // reception buffer
  uint8_t buffer_index = 0;                         // buffer current index
} teleinfo_line;

// TIC : current message
struct tic_line {
  char str_etiquette[TELEINFO_KEY_MAX];             // TIC line etiquette
  char str_donnee[TELEINFO_DATA_MAX];               // TIC line donnee
  char checksum;
};
static struct {
  bool     overload   = false;                      // overload has been detected
  bool     received   = false;                      // one full message has been received
  bool     percent    = false;                      // power has changed of more than 1% on one phase
  bool     send_msg   = false;                      // flag to ask to send TIC JSON
  bool     send_tic   = false;                      // flag to ask to send TIC JSON
  bool     send_meter = false;                      // flag to ask to send Meter JSON
  int      line_index = 0;                          // index of current received message line
  int      line_max   = 0;                          // max number of lines in a message
  int      length     = INT_MAX;                    // length of message     
  uint32_t timestamp  = UINT32_MAX;                 // timestamp of message (ms)
  tic_line line[TELEINFO_LINE_QTY];                 // array of message lines
} teleinfo_message;

// teleinfo : contract data
static struct {
  int  phase      = 1;                              // number of phases
  int  mode       = TIC_MODE_UNDEFINED;             // meter mode
  int  period     = -1;                             // current tarif period
  int  nb_indexes = -1;                             // number of indexes in current contract      
  long voltage    = TELEINFO_VOLTAGE;               // contract reference voltage
  long isousc     = 0;                              // contract max current per phase
  long ssousc     = 0;                              // contract max power per phase
  char str_id[TELEINFO_CONTRACTID_MAX];             // contract reference (adco or ads)
} teleinfo_contract;

// teleinfo : power meter
static struct {
  uint8_t   status_rx      = TIC_SERIAL_INIT;       // Teleinfo Rx initialisation status
  uint8_t   enable_rx      = UINT8_MAX;             // pin used to enable/disable Teleinfo Rx
  int       interval_count = 0;                     // energy publication counter      
  long      papp           = 0;                     // current apparent power 
  long      nb_message     = 0;                     // total number of messages sent by the meter
  long      nb_reset       = 0;                     // total number of message reset sent by the meter
  long      nb_update      = 0;                     // number of cosphi calculation updates
  long long nb_line        = 0;                     // total number of received lines
  long long nb_error       = 0;                     // total number of checksum errors
  long long total_wh       = 0;                     // total of all indexes of active power
  long long index[TELEINFO_INDEX_MAX];              // array of indexes of different tarif periods
} teleinfo_meter;

// teleinfo : actual data per phase
struct tic_phase {
  bool  volt_set;                                    // voltage set in current message
  long  voltage;                                     // instant voltage
  long  current;                                     // instant current
  long  papp;                                        // instant apparent power
  long  pact;                                        // instant active power
  long  papp_last;                                   // last published apparent power
  float cosphi;                                     // cos phi (x100)
}; 
static tic_phase teleinfo_phase[ENERGY_MAX_PHASES];

// teleinfo : calculation periods data
struct tic_period {
  bool updated;                                     // flag to ask for graph update
  int  index;                                       // current array index per refresh period
  int  line_idx;                                    // line index in storage file
  long counter;                                     // counter in seconds of current refresh period

  // --- arrays for current refresh period (per phase) ---
  long  papp_peak[ENERGY_MAX_PHASES];               // peak apparent power during refresh period
  long  volt_low[ENERGY_MAX_PHASES];                // lowest voltage during refresh period
  long  volt_peak[ENERGY_MAX_PHASES];               // peak high voltage during refresh period
  long  long papp_sum[ENERGY_MAX_PHASES];           // sum of apparent power during refresh period
  long  long pact_sum[ENERGY_MAX_PHASES];           // sum of active power during refresh period
  float cosphi_sum[ENERGY_MAX_PHASES];              // sum of cos phi during refresh period
}; 
static tic_period teleinfo_period[TELEINFO_PERIOD_MAX];

// teleinfo : log data
struct tic_log {
  uint32_t time_flush;                              // timestamp of next flush
  String   str_filename;                            // current log filename
  String   str_buffer;                              // pre log buffer
}; 
static tic_log teleinfo_log[TELEINFO_PERIOD_MAX];

// teleinfo : graph data
struct tic_graph {
  uint8_t arr_papp[ENERGY_MAX_PHASES][TELEINFO_GRAPH_SAMPLE];     // array of apparent power graph values
  uint8_t arr_pact[ENERGY_MAX_PHASES][TELEINFO_GRAPH_SAMPLE];     // array of active power graph values
  uint8_t arr_volt[ENERGY_MAX_PHASES][TELEINFO_GRAPH_SAMPLE];     // array min and max voltage delta
  uint8_t arr_cosphi[ENERGY_MAX_PHASES][TELEINFO_GRAPH_SAMPLE];   // array of cos phi
};
static struct {
  long pmax   = TELEINFO_GRAPH_POWER_MAX;           // graph maximum power
  long vmax   = TELEINFO_GRAPH_VOLTAGE_MAX;         // graph maximum voltage
  long papp_peak[ENERGY_MAX_PHASES];                // peak apparent power to save
  long volt_low[ENERGY_MAX_PHASES];                 // voltage to save
  long volt_peak[ENERGY_MAX_PHASES];                // voltage to save
  long papp[ENERGY_MAX_PHASES];                     // apparent power to save
  long pact[ENERGY_MAX_PHASES];                     // active power to save
  int  cosphi[ENERGY_MAX_PHASES];                   // cosphi to save
  tic_graph data[TELEINFO_MEMORY_PERIOD_MAX];       // grpah display value per period
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
  for (index = 0; index < nitems (ARR_TELEINFO_RATE); index ++) if (baud_rate == ARR_TELEINFO_RATE[index]) is_conform = true;
  if (!is_conform) baud_rate = 1200;
  
  return baud_rate;
}

/**************************************************\
 *                  Commands
\**************************************************/

void CmndTeleinfoRate (void)
{
  if (XdrvMailbox.data_len > 0) teleinfo_config.baud_rate = TeleinfoValidateBaudRate ((uint16_t)XdrvMailbox.payload);
  ResponseCmndNumber (teleinfo_config.baud_rate);
}

void CmndTeleinfoMessagePolicy (void)
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload >= 0) && (XdrvMailbox.payload < TELEINFO_POLICY_MAX)) teleinfo_config.msg_policy = XdrvMailbox.payload;
  ResponseCmndNumber (teleinfo_config.msg_policy);
}

void CmndTeleinfoMessageType (void)
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload >= 0) && (XdrvMailbox.payload < TELEINFO_TYPE_MAX)) teleinfo_config.msg_type = XdrvMailbox.payload;
  ResponseCmndNumber (teleinfo_config.msg_type);
}

void CmndTeleinfoLogPolicy (void)
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload < TELEINFO_LOG_MAX)) teleinfo_config.log_policy = XdrvMailbox.payload;
  ResponseCmndNumber (teleinfo_config.log_policy);
}

void CmndTeleinfoInterval (void)
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload >= TELEINFO_INTERVAL_MIN) && (XdrvMailbox.payload <= TELEINFO_INTERVAL_MAX)) teleinfo_config.update_interval = XdrvMailbox.payload;
  ResponseCmndNumber (teleinfo_config.update_interval);
}

void CmndTeleinfoAdjust (void)
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload >= TELEINFO_PERCENT_MIN) && (XdrvMailbox.payload <= TELEINFO_PERCENT_MAX)) teleinfo_config.percent_adjust = XdrvMailbox.payload;
  ResponseCmndNumber (teleinfo_config.percent_adjust);
}

// Start and stop Teleinfo serial reception
void CmndTeleinfoEnable (void)
{
  bool cmd_start  = false;
  bool cmd_stop   = false;
  bool cmd_result = false;

  // if parameter is provided
  if (XdrvMailbox.data_len > 0)
  {
    // determine command
    cmd_start = ((strcasecmp (XdrvMailbox.data, MQTT_STATUS_ON) == 0)  || (XdrvMailbox.payload == 1));
    cmd_stop  = ((strcasecmp (XdrvMailbox.data, MQTT_STATUS_OFF) == 0) || (XdrvMailbox.payload == 0));

    // switch ON or OFF command
    if (cmd_start) cmd_result = TeleinfoEnableSerial ();
    else if (cmd_stop) cmd_result = TeleinfoDisableSerial ();
  } 

  // send response
  if (cmd_result) ResponseCmndDone ();
  else ResponseCmndFailed ();
}

void CmndTeleinfoRotate (void)
{
  // rotate files
  TeleinfoRotateLogs ();

  // send response
  ResponseCmndDone ();
}

/*********************************************\
 *               Functions
\*********************************************/

// Start serial reception
bool TeleinfoEnableSerial ()
{
  bool is_enabled = false;

  // if serial port creation is possible
  if ((TasmotaGlobal.energy_driver == XNRG_15) && PinUsed (GPIO_TELEINFO_RX) && (teleinfo_config.baud_rate > 0))
  { 
 
#ifdef ESP32
    // create serial port
    teleinfo_serial = new TasmotaSerial (Pin (GPIO_TELEINFO_RX), -1, 1);

    // initialise serial port
    is_enabled = teleinfo_serial->begin (teleinfo_config.baud_rate, SERIAL_7E1);

#else       // ESP8266
    // create serial port
    teleinfo_serial = new TasmotaSerial (Pin (GPIO_TELEINFO_RX), -1, 1);

    // initialise serial port
    is_enabled = teleinfo_serial->begin (teleinfo_config.baud_rate, SERIAL_7E1);

    // force configuration on ESP8266
    if (teleinfo_serial->hardwareSerial ()) ClaimSerial ();

#endif      // ESP32 & ESP8266

    // declare serial port enabled
    if (is_enabled)
    {
      teleinfo_meter.status_rx = TIC_SERIAL_ACTIVE; 
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Serial set to 7E1 %d bauds"), teleinfo_config.baud_rate);
    } 

    // else declare serial port initialisation failure
    else
    {
      teleinfo_meter.status_rx = TIC_SERIAL_FAILED; 
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Serial port init failed"));
    } 
  }

  // if needed, enable Teleinfo RX
  if (is_enabled && (teleinfo_meter.enable_rx != UINT8_MAX))
  {
    // set output high
    digitalWrite(teleinfo_meter.enable_rx, HIGH);
    AddLog (LOG_LEVEL_INFO, PSTR("TIC: Teleinfo Rx enabled by GPIO%d"), teleinfo_meter.enable_rx);
  }

  return is_enabled;
}

// Stop serial reception
bool TeleinfoDisableSerial ()
{
  bool control_rx;

  // if needed, disable Teleinfo RX
  control_rx = (teleinfo_meter.enable_rx != UINT8_MAX);
  if (control_rx)
  {
    // set output low
    digitalWrite(teleinfo_meter.enable_rx, LOW);
    AddLog (LOG_LEVEL_INFO, PSTR("TIC: Teleinfo Rx disabled by GPIO%d"), teleinfo_meter.enable_rx);
  }

  return control_rx;
}

// Load configuration from Settings or from LittleFS
void TeleinfoLoadConfig () 
{
#ifdef USE_UFILESYS

  // retrieve saved settings from flash filesystem
  teleinfo_config.baud_rate       = TeleinfoValidateBaudRate (UfsCfgLoadKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_RATE));
  teleinfo_config.log_policy      = UfsCfgLoadKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_LOG_POLICY);
  teleinfo_config.msg_policy      = UfsCfgLoadKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_MSG_POLICY);
  teleinfo_config.msg_type        = UfsCfgLoadKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_MSG_TYPE);
  teleinfo_config.update_interval = UfsCfgLoadKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_INTERVAL);
  teleinfo_config.percent_adjust  = UfsCfgLoadKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_ADJUST);
  teleinfo_config.nb_daily        = UfsCfgLoadKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_NBDAILY, TELEINFO_DAILY_DEFAULT);
  teleinfo_config.nb_weekly       = UfsCfgLoadKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_NBWEEKLY, TELEINFO_WEEKLY_DEFAULT);
  teleinfo_config.height          = UfsCfgLoadKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_HEIGHT, TELEINFO_GRAPH_HEIGHT);

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Config loaded from LittleFS"));

#else

  // retrieve saved settings from flash memory
  teleinfo_config.baud_rate       = TeleinfoValidateBaudRate (Settings->sbaudrate * 300);
  teleinfo_config.msg_policy      = (int)Settings->weight_reference;
  teleinfo_config.msg_type        = (int)Settings->weight_calibration;
  teleinfo_config.update_interval = (int)Settings->weight_item;
  teleinfo_config.percent_adjust  = (int)Settings->weight_change;

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Config loaded from flash memory"));

# endif     // USE_UFILESYS

  // validate boundaries
  if (teleinfo_config.log_policy >= TELEINFO_LOG_MAX) teleinfo_config.log_policy = TELEINFO_LOG_BUFFERED;
  if ((teleinfo_config.msg_policy < 0) || (teleinfo_config.msg_policy >= TELEINFO_POLICY_MAX)) teleinfo_config.msg_policy = TELEINFO_POLICY_TELEMETRY;
  if (teleinfo_config.msg_type >= TELEINFO_TYPE_MAX) teleinfo_config.msg_type = TELEINFO_TYPE_METER;
  if ((teleinfo_config.update_interval < TELEINFO_INTERVAL_MIN) || (teleinfo_config.update_interval > TELEINFO_INTERVAL_MAX)) teleinfo_config.update_interval = TELEINFO_INTERVAL_MAX;
  if ((teleinfo_config.percent_adjust < TELEINFO_PERCENT_MIN) || (teleinfo_config.percent_adjust > TELEINFO_PERCENT_MAX)) teleinfo_config.percent_adjust = 100;
}

// Save configuration to Settings or to LittleFS
void TeleinfoSaveConfig () 
{
#ifdef USE_UFILESYS

  // save settings into flash filesystem
  UfsCfgSaveKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_RATE,       teleinfo_config.baud_rate,       true);
  UfsCfgSaveKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_LOG_POLICY, teleinfo_config.log_policy,      false);
  UfsCfgSaveKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_MSG_POLICY, teleinfo_config.msg_policy,      false);
  UfsCfgSaveKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_MSG_TYPE,   teleinfo_config.msg_type,        false);
  UfsCfgSaveKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_INTERVAL,   teleinfo_config.update_interval, false);
  UfsCfgSaveKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_ADJUST,     teleinfo_config.percent_adjust,  false);
  UfsCfgSaveKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_NBDAILY,    teleinfo_config.nb_daily,        false);
  UfsCfgSaveKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_NBWEEKLY,   teleinfo_config.nb_weekly,       false);
  UfsCfgSaveKeyInt (D_TELEINFO_CFG, D_CMND_TELEINFO_HEIGHT,     teleinfo_config.height,          false);

# else

  // save serial port settings
  Settings->sbaudrate = teleinfo_config.baud_rate / 300;

  // save teleinfo settings
  Settings->weight_reference   = (ulong)teleinfo_config.msg_policy;
  Settings->weight_calibration = (ulong)teleinfo_config.msg_type;
  Settings->weight_item        = (ulong)teleinfo_config.update_interval;
  Settings->weight_change      = (uint8_t) teleinfo_config.percent_adjust;

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
  if (teleinfo_line.separator == ' ') line_size--;

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

// Split line into etiquette and donnee
void TeleinfoSplitLine (char* pstr_line, char* pstr_etiquette, int size_etiquette, char* pstr_donnee, int size_donnee)
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

// Display any value to a string with unit and kilo conversion with number of digits (12000, 12000 VA, 12.0 kVA, 12.00 kVA, ...)
void TeleinfoDisplayValue (int unit_type, long value, char* pstr_result, int size_result, int kilo_digit = -1) 
{
  int  length;
  char str_text[16];

  // handle specific cases
  if (kilo_digit > 3) kilo_digit = 3;
  if (value < 1000) kilo_digit = -1;
  if (unit_type == TELEINFO_UNIT_COSPHI) kilo_digit = 2;

  // if strings are valid
  if (pstr_result != nullptr)
  {
    // convert value
    ltoa (value, pstr_result, 10);

    // if kilo format is asked
    if (kilo_digit > -1)
    {
      // prefix 0 if needed
      strcpy (str_text, "");
      if (value < 10) strcpy_P (str_text, PSTR ("000"));
      else if (value < 100) strcpy_P (str_text, PSTR ("00"));
      else if (value < 1000) strcpy_P (str_text, PSTR ("0"));

      // append value
      strlcat (str_text, pstr_result, sizeof (str_text));

      // generate kilo format
      length = strlen (str_text) - 3;
      strcpy (pstr_result, "");
      strncat (pstr_result, str_text, length);
      if (kilo_digit > 0)
      {
        strcat_P (pstr_result, PSTR ("."));
        strncat (pstr_result, str_text + length, kilo_digit);
      }
    }

    // append unit if specified
    if (unit_type < TELEINFO_UNIT_MAX) 
    {
      // add space and k unit, else end of string
      strlcat (pstr_result, " ", size_result);
      if ((kilo_digit > 0) && (unit_type != TELEINFO_UNIT_COSPHI)) strlcat (pstr_result, "k", size_result);

      // append unit label
      GetTextIndexed (str_text, sizeof (str_text), unit_type, kTeleinfoGraphDisplay);
      strlcat (pstr_result, str_text, size_result);
    }
  }
}

// Generate current counter values as a string with unit and kilo conversion
void TeleinfoDisplayCurrentValue (int unit_type, int phase, char* pstr_result, int size_result, int kilo_digit = -1) 
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
      value = (long)teleinfo_phase[phase].cosphi * 10;
      break;
  }

  // convert value for display
  TeleinfoDisplayValue (unit_type, value, pstr_result, size_result, kilo_digit);
}

/*********************************************\
 *               Historisation
\*********************************************/

#ifdef USE_UFILESYS

// Conversion from long long to string (not available in standard libraries)
void lltoa (long long value, char *pstr_result, size_t size_result)
{
  lldiv_t result;
  char    str_value[24];

  // conversion loop
  strcpy (str_value, "");
  result.quot = value;
  do
  {
    // split by chunk of 1 000 000 to be compatible with long conversion
    result = lldiv (result.quot, 1000000);
    if (result.quot > 0) sprintf_P (pstr_result, PSTR ("%06d"), (long)result.rem);
    else sprintf_P (pstr_result, PSTR ("%d"), (long)result.rem);

    // add chunk to conversion result
    strlcat (pstr_result, str_value, size_result);
    strlcpy (str_value, pstr_result, sizeof (str_value));
  } while (result.quot > 0);
}

// Get historisation period start time
uint32_t TeleinfoHistoGetStartTime (int period, int histo)
{
  uint32_t start_time, delta_time;
  TIME_T   start_dst;

  // start date
  start_time = LocalTime ();
  if (period == TELEINFO_PERIOD_DAY) start_time -= histo * 86400;
  else if (period == TELEINFO_PERIOD_WEEK) start_time -= histo * 604800;

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
bool TeleinfoHistoGetDate (int period, int histo, char* pstr_text)
{
  uint32_t calc_time;
  TIME_T   start_dst, stop_dst;

  // init
  strcpy (pstr_text, "");

  // start date
  calc_time = TeleinfoHistoGetStartTime (period, histo);
  BreakTime (calc_time, start_dst);

  // generate time label for day graph
  if (period == TELEINFO_PERIOD_DAY) sprintf_P (pstr_text, PSTR ("%02u/%02u"), start_dst.day_of_month, start_dst.month);

  // generate time label for day graph
  else if (period == TELEINFO_PERIOD_WEEK)
  {
    // end date
    calc_time += 518400;
    BreakTime (calc_time, stop_dst);
    sprintf_P (pstr_text, PSTR ("%02u/%02u - %02u/%02u"), start_dst.day_of_month, start_dst.month, stop_dst.day_of_month, stop_dst.month);
  }

  return (strlen (pstr_text) > 0);
}

// Get historisation filename
bool TeleinfoHistoGetFilename (int period, int histo, char* pstr_filename)
{
  bool exists = false;

  // set filename according to period
  strcpy (pstr_filename, "");
  switch (period)
  {
    case TELEINFO_PERIOD_DAY:
      sprintf_P (pstr_filename, D_TELEINFO_HISTO_FILE_DAILY, histo);
      break;
    case TELEINFO_PERIOD_WEEK:
      sprintf_P (pstr_filename, D_TELEINFO_HISTO_FILE_WEEKLY, histo);
      break;
  }

  // if filename defined, check existence
  if (strlen (pstr_filename) > 0) exists = ffsp->exists (pstr_filename);

  return exists;
}

// Get historisation data
long TeleinfoHistoGetData (int data, int period, int phase, int index, bool increment)
{
  int  column, csv_index;
  long value = LONG_MAX;

  // if index is 0, restart from beginning skipping header
  if (index == 0) UfsCsvSeekToStart ();
  else if (index == TELEINFO_GRAPH_SAMPLE - 1) UfsCsvSeekToEnd ();

  // if index of current line is different, increment or decrement line
  csv_index = UfsCsvGetColumnInt (0);
  if (increment) while ((csv_index < index) && (csv_index < TELEINFO_GRAPH_SAMPLE)) csv_index = UfsCsvGetColumnInt (0, UFS_CSV_NEXT);
  else while ((csv_index > index) && (csv_index > 0)) csv_index = UfsCsvGetColumnInt (0, UFS_CSV_PREVIOUS);

  // if line index is matching, get the value
  if (csv_index == index)
  {
    // calculate column index to read and extract data
    if (data < TELEINFO_HISTO_MAX) column = 3 + (TELEINFO_HISTO_MAX * phase) + data;
    else column = INT_MAX;

    // if ok, read data from column and prepare next line
    value = UfsCsvGetColumnLong (column);
  }

  return value;
}

// Flush log data to littlefs
void TeleinfoHistoFlushData (int period)
{
  // if buffer is filled, save buffer to log
  if (teleinfo_log[period].str_buffer.length () > 0)
  {
    UfsCsvAppend (teleinfo_log[period].str_filename.c_str (), teleinfo_log[period].str_buffer.c_str (), false);
    teleinfo_log[period].str_buffer = "";
  }

  // set next flush time
  teleinfo_log[period].time_flush = millis () + 1000 * ARR_TELEINFO_PERIOD_FLUSH[period];
}

// Save historisation data
void TeleinfoHistoSaveData (int period, uint8_t log_policy)
{
  int    phase, index, line;
  TIME_T now_dst;
  char   str_filename[UFS_FILENAME_SIZE];
  char   str_line[TELEINFO_HISTO_LINE_LENGTH];
  char   str_value[32];

  // check boundaries
  if ((period != TELEINFO_PERIOD_DAY) && (period != TELEINFO_PERIOD_WEEK)) return;

  // extract current time data
  BreakTime (LocalTime (), now_dst);

  // set target is weekly records
  if (period == TELEINFO_PERIOD_WEEK)
  {
    sprintf_P (str_filename, D_TELEINFO_HISTO_FILE_WEEKLY, 0);
    line = (((now_dst.day_of_week + 5) % 7) * 86400 + now_dst.hour * 3600 + now_dst.minute * 60) / ARR_TELEINFO_PERIOD_WINDOW[period];
  }

  // else target is daily records
  else if (period == TELEINFO_PERIOD_DAY)
  {
    sprintf_P (str_filename, D_TELEINFO_HISTO_FILE_DAILY, 0);
    line = (now_dst.hour * 3600 + now_dst.minute * 60) / ARR_TELEINFO_PERIOD_WINDOW[period];
  }

  else return;

  // if log file name has changed
  if (teleinfo_log[period].str_filename != str_filename) TeleinfoHistoFlushData (period);

  // set log filename
  teleinfo_log[period].str_filename = str_filename;

  // if buffer empty and new file, generate header
  if (!ffsp->exists (str_filename) && teleinfo_log[period].str_buffer.length () == 0)
  {
    strcpy_P (str_line, PSTR ("Idx;Date;Time"));
    for (phase = 0; phase < teleinfo_contract.phase; phase++)
    {
      sprintf_P (str_value, PSTR (";VA%d;W%d;V%d;C%d;pVA%d;pV%d"), phase + 1, phase + 1, phase + 1, phase + 1, phase + 1, phase + 1);
      strlcat (str_line, str_value, sizeof (str_line));
    }

    // append contract period and totals
    strcpy_P (str_value, PSTR (";Period"));
    strlcat (str_line, str_value, sizeof (str_line));
    for (index = ARR_TELEINFO_PERIOD_FIRST[teleinfo_contract.period]; index < ARR_TELEINFO_PERIOD_FIRST[teleinfo_contract.period] + teleinfo_contract.nb_indexes; index++)
    {
      GetTextIndexed (str_value, sizeof (str_value), index, kTeleinfoPeriod);
      strlcat (str_line, ";", sizeof (str_line));
      strlcat (str_line, str_value, sizeof (str_line));
    }
    strlcat (str_line, "\n", sizeof (str_line));

    // add header to the buffer
    teleinfo_log[period].str_buffer += str_line;
  }

  // calculate line index (previous line + 1 or calculated line index if difference is more than one index)
  if (abs (line - teleinfo_period[period].line_idx) > 1) teleinfo_period[period].line_idx = line;

  // generate line  : index and date
  sprintf_P (str_line, PSTR ("%d;%02u/%02u;%02u:%02u"), teleinfo_period[period].line_idx++, now_dst.day_of_month, now_dst.month, now_dst.hour, now_dst.minute);

  // generate line  : phase data
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    sprintf_P (str_value, PSTR (";%d;%d;%d;%d;%d;%d"), teleinfo_graph.papp[phase], teleinfo_graph.pact[phase], teleinfo_graph.volt_low[phase], teleinfo_graph.cosphi[phase], teleinfo_graph.papp_peak[phase], teleinfo_graph.volt_peak[phase]);
    strlcat (str_line, str_value, sizeof (str_line));
  }

  // generate line  : totals
  GetTextIndexed (str_value, sizeof (str_value), teleinfo_contract.period, kTeleinfoPeriod);
  strlcat (str_line, ";", sizeof (str_line));
  strlcat (str_line, str_value, sizeof (str_line));
  for (index = 0; index < teleinfo_contract.nb_indexes; index++)
  {
    lltoa (teleinfo_meter.index[index], str_value, sizeof (str_value)); 
    strlcat (str_line, ";", sizeof (str_line));
    strlcat (str_line, str_value, sizeof (str_line));
  }
  strlcat (str_line, "\n", sizeof (str_line));

  // add line to the buffer
  teleinfo_log[period].str_buffer += str_line;

  // if log should be saved now
  if ((log_policy == TELEINFO_LOG_IMMEDIATE) || (teleinfo_log[period].str_buffer.length () > TELEINFO_HISTO_BUFFER_MAX)) TeleinfoHistoFlushData (period);
}

#endif    // USE_UFILESYS

/*********************************************\
 *                   Graph
\*********************************************/

// Save current values to graph data
void TeleinfoGraphSaveData (int period)
{
  bool histo_update;
  int  index, phase;
  long power, voltage, value;

  // if period out of range, return
  if ((period < 0) || (period >= TELEINFO_PERIOD_MAX)) return;

  // historic will be updated if period is beyond dynamic graph period
  histo_update = (period >= TELEINFO_MEMORY_PERIOD_MAX);

  // set indexed graph values with current values
  index = teleinfo_period[period].index;
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    //   Apparent power
    // -----------------
    power = LONG_MAX;
    value = UINT8_MAX;

    // save average and peak value over the period
    if (teleinfo_contract.ssousc > 0) power = (long)(teleinfo_period[period].papp_sum[phase] / ARR_TELEINFO_PERIOD_WINDOW[period]);
    teleinfo_graph.papp[phase] = power;
    teleinfo_graph.papp_peak[phase] = teleinfo_period[period].papp_peak[phase];

    // if needed, save dynmic memory graph data
    if (period < TELEINFO_MEMORY_PERIOD_MAX)
    {
      // calculate percentage of power according to contract (200 = 100% of contract power)
      if (teleinfo_contract.ssousc > 0) value = power * 200 / teleinfo_contract.ssousc;
      teleinfo_graph.data[period].arr_papp[phase][index] = (uint8_t)value;
    }

    // if power not defined, no historic graph saving 
    histo_update &= (power != LONG_MAX);

    // reset period data
    teleinfo_period[period].papp_peak[phase] = 0;
    teleinfo_period[period].papp_sum[phase]  = 0;

    //   Active power
    // -----------------
    power = LONG_MAX;
    value = UINT8_MAX;

    // save average value over the period
    if (teleinfo_contract.ssousc > 0) power = (long)(teleinfo_period[period].pact_sum[phase] / ARR_TELEINFO_PERIOD_WINDOW[period]);
    if (power > teleinfo_graph.papp[phase]) power = teleinfo_graph.papp[phase];
    teleinfo_graph.pact[phase] = power;

    // if needed, save dynmic memory graph data
    if (period < TELEINFO_MEMORY_PERIOD_MAX)
    {
      // calculate percentage of power according to contract (200 = 100% of contract power)
      if (teleinfo_contract.ssousc > 0) value = power * 200 / teleinfo_contract.ssousc;
      teleinfo_graph.data[period].arr_pact[phase][index] = (uint8_t)value;
    }

    // if power not defined, no historic graph saving 
    histo_update &= (power != LONG_MAX);

    // reset period data
    teleinfo_period[period].pact_sum[phase] = 0;

    //   Voltage
    // -----------

    // save graph current value
    teleinfo_graph.volt_low[phase]  = teleinfo_period[period].volt_low[phase];
    teleinfo_graph.volt_peak[phase] = teleinfo_period[period].volt_peak[phase];

    // if needed, save dynmic memory graph data
    if (period < TELEINFO_MEMORY_PERIOD_MAX)
    {
      value = 128 + teleinfo_graph.volt_low[phase] - TELEINFO_VOLTAGE;
      teleinfo_graph.data[period].arr_volt[phase][index] = (uint8_t)value;
    }

    // reset period data
    teleinfo_period[period].volt_low[phase]  = LONG_MAX;
    teleinfo_period[period].volt_peak[phase] = 0;

    //   CosPhi
    // -----------

    // save average value over the period
    teleinfo_graph.cosphi[phase] = (int)(teleinfo_period[period].cosphi_sum[phase] / ARR_TELEINFO_PERIOD_WINDOW[period]);

    // if needed, save dynamic memory graph data
    if (period < TELEINFO_MEMORY_PERIOD_MAX) teleinfo_graph.data[period].arr_cosphi[phase][index] = (uint8_t)teleinfo_graph.cosphi[phase];

    // reset period _
    teleinfo_period[period].cosphi_sum[phase] = 0;
  }

  // increase data index in the graph and set update flag
  teleinfo_period[period].index++;
  teleinfo_period[period].index   = teleinfo_period[period].index % TELEINFO_GRAPH_SAMPLE;
  teleinfo_period[period].updated = true;

#ifdef USE_UFILESYS
    // if needed, save data to history file
    if (ufs_type && histo_update) TeleinfoHistoSaveData (period, teleinfo_config.log_policy);
#endif    // USE_UFILESYS
}

// Get a specific graph data value
long TeleinfoGetGraphData (int data, int period, int phase, int index, bool increment)
{
  int  index_array;
  long value = LONG_MAX;

  // if data is stored in memory
  if (period < TELEINFO_MEMORY_PERIOD_MAX)
  {
    // get current array index if in live memory mode
    index_array = (index + teleinfo_period[period].index) % TELEINFO_GRAPH_SAMPLE;

    // get memory data according to data type
    switch (data)
    {
      case TELEINFO_UNIT_VA:
        // if value is defined, calculate percentage of power according to contract
        if (teleinfo_graph.data[period].arr_papp[phase][index_array] != UINT8_MAX)
          value = (long)teleinfo_graph.data[period].arr_papp[phase][index_array] * teleinfo_contract.ssousc / 200;
        break;

      case TELEINFO_UNIT_W:
        // if value is defined, calculate percentage of power according to contract
        if (teleinfo_graph.data[period].arr_pact[phase][index_array] != UINT8_MAX)
          value = (long)teleinfo_graph.data[period].arr_pact[phase][index_array] * teleinfo_contract.ssousc / 200;
        break;

      case TELEINFO_UNIT_V:
        // if voltage has been stored
        if (teleinfo_graph.data[period].arr_volt[phase][index_array] != UINT8_MAX)
          value = TELEINFO_VOLTAGE - 128 + teleinfo_graph.data[period].arr_volt[phase][index_array];
        break;

      case TELEINFO_UNIT_COSPHI:
        // if voltage has been stored
        if (teleinfo_graph.data[period].arr_cosphi[phase][index_array] != UINT8_MAX) value = (long)teleinfo_graph.data[period].arr_cosphi[phase][index_array];
        break;
    }

    // if value undefined, set set out or display
    if (value == LONG_MAX)
    {
      if ((data == TELEINFO_UNIT_VA) || (data == TELEINFO_UNIT_W)) value = -100;
      else if (data == TELEINFO_UNIT_COSPHI) value = 110;
    }
  }

#ifdef USE_UFILESYS
  // else if histo value is asked
  else value = TeleinfoHistoGetData (data, period, phase, index, increment);
#endif    // USE_UFILESYS

  return value;
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
    // switch it off to avoid ESP32 Rx heavy load restart bug
    teleinfo_meter.enable_rx = Pin (GPIO_TELEINFO_ENABLE);
    pinMode (teleinfo_meter.enable_rx, OUTPUT);

    // disable serial input
    TeleinfoDisableSerial ();
  }

  // if TeleinfoRX pin defined, set energy driver as teleinfo
  if (!TasmotaGlobal.energy_driver && PinUsed (GPIO_TELEINFO_RX))
  {
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

  // init hardware energy counters
  Settings->flag3.hardware_energy_total = true;
  Settings->energy_kWhtotal = 0;

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

  // init strings
  strcpy (teleinfo_contract.str_id, "");

  // init all total indexes
  for (index = 0; index < TELEINFO_INDEX_MAX; index ++) teleinfo_meter.index[index] = 0;

  // disable all message lines
  for (index = 0; index < TELEINFO_LINE_QTY; index ++) teleinfo_message.line[index].checksum = 0;

  // log default method
  AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Using default Global Counter method"));
}

// Teleinfo graph data initialisation
void TeleinfoGraphInit ()
{
  int      index, phase, period;
  uint32_t time_now = millis ();

  // initialise period data
  for (period = 0; period < TELEINFO_PERIOD_MAX; period++)
  {
    // init counter
    teleinfo_period[period].index    = 0;
    teleinfo_period[period].counter  = 0;
    teleinfo_period[period].line_idx = 0;

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

    // set initial log flush time
    teleinfo_log[period].time_flush = time_now + 1000 * ARR_TELEINFO_PERIOD_FLUSH[period];
  }

  // initialise graph data
  for (period = 0; period < TELEINFO_MEMORY_PERIOD_MAX; period++)
  {
    // loop thru phase
    for (phase = 0; phase < ENERGY_MAX_PHASES; phase++)
    {
      // loop thru graph values
      for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
      {
        teleinfo_graph.data[period].arr_pact[phase][index]   = UINT8_MAX;
        teleinfo_graph.data[period].arr_papp[phase][index]   = UINT8_MAX;
        teleinfo_graph.data[period].arr_volt[phase][index]   = UINT8_MAX;
        teleinfo_graph.data[period].arr_cosphi[phase][index] = UINT8_MAX;
      } 
    }
  }
}

// Save data before ESP restart
void TeleinfoSaveBeforeRestart ()
{
  // stop serail reception and disable Teleinfo Rx
  teleinfo_meter.status_rx = TIC_SERIAL_STOPPED;
  TeleinfoDisableSerial ();

  // update energy total (in kwh)
  EnergyUpdateTotal ();

#ifdef USE_UFILESYS
  // flush logs
  TeleinfoHistoFlushData ( TELEINFO_PERIOD_DAY);
  TeleinfoHistoFlushData ( TELEINFO_PERIOD_WEEK);
#endif      // USE_UFILESYS
}

// Rotation of log files
void TeleinfoRotateLogs ()
{
#ifdef USE_UFILESYS
  uint32_t min_size;
  TIME_T   now_dst;
  char     str_filename[UFS_FILENAME_SIZE];

  // log default method
  AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Rotate log files"));

  // flush logs
  TeleinfoHistoFlushData ( TELEINFO_PERIOD_DAY);
  TeleinfoHistoFlushData ( TELEINFO_PERIOD_WEEK);

  // calculate minimum file size from lastest daily file
  sprintf_P (str_filename, D_TELEINFO_HISTO_FILE_DAILY, 0);
  min_size = UfsCsvGetFileSizeKb (str_filename) * 2;

  // rotate daily files
  UfsCsvFileRotate (D_TELEINFO_HISTO_FILE_DAILY, 0, teleinfo_config.nb_daily);

  // if we are monday, week has changed, rotate previous weekly files
  BreakTime (LocalTime (), now_dst);
  if (now_dst.day_of_week == 2) UfsCsvFileRotate (D_TELEINFO_HISTO_FILE_WEEKLY, 0, teleinfo_config.nb_weekly);

  // clean old CSV files get get minimum size left
  UfsCsvCleanupFileSystem ((uint32_t)min_size);
#endif      // USE_UFILESYS
}

// Handling of received teleinfo data
void TeleinfoReceiveData ()
{
  bool      overload, tcp_send, is_valid;
  int       index, phase, period;
  long      value, total_current, increment;
  long long counter, counter_wh, delta_wh;
  uint32_t  timeout, timestamp, message_ms;
  float     cosphi, papp_inc;
  char*     pstr_match;
  char      checksum, byte_recv;
  char      str_etiquette[TELEINFO_KEY_MAX];
  char      str_donnee[TELEINFO_DATA_MAX];
  char      str_text[TELEINFO_DATA_MAX];

  // init and set receive loop timeout to 40ms
  overload  = false;
  tcp_send  = false;
  timestamp = millis ();
  timeout   = timestamp + 40;

  // serial receive loop
  while (!TimeReached (timeout) && teleinfo_serial->available()) 
  {
    // read caracter
    byte_recv = (char)teleinfo_serial->read (); 
    teleinfo_message.length++;

    // if TCP server is active, append character to TCP buffer
    tcp_server.send (byte_recv); 

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
        teleinfo_line.separator = 0;
        teleinfo_line.buffer_index = 0;
        break;

      // ------------------------------
      // \t or SPACE : Line - New part
      // ------------------------------

      case 0x09:
      case ' ':
        if (teleinfo_line.separator == 0) teleinfo_line.separator = byte_recv;
        if (teleinfo_line.buffer_index < sizeof (teleinfo_line.buffer) - 1)
        {
          teleinfo_line.buffer[teleinfo_line.buffer_index] = byte_recv;
          teleinfo_line.buffer_index++;
        }
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
          teleinfo_line.buffer[teleinfo_line.buffer_index] = 0;
          checksum = TeleinfoCalculateChecksum (teleinfo_line.buffer);
          if (checksum != 0)
          {
            // init
            strcpy (str_etiquette, "");
            strcpy (str_donnee, "");
            
            // extract etiquette and donnee from current line
            TeleinfoSplitLine (teleinfo_line.buffer, str_etiquette, sizeof (str_etiquette), str_donnee, sizeof (str_donnee));

            // get etiquette type
            index = GetCommandCode (str_text, sizeof (str_text), str_etiquette, kTeleinfoEtiquetteName);

            // update data according to etiquette
            switch (index)
            {
              // Contract : Reference
              case TIC_ADCO:
                teleinfo_contract.mode = TIC_MODE_HISTORIC;
                strlcpy (teleinfo_contract.str_id, str_donnee, sizeof (teleinfo_contract.str_id));
                break;
              case TIC_ADSC:
                teleinfo_contract.mode = TIC_MODE_STANDARD;
                strlcpy (teleinfo_contract.str_id, str_donnee, sizeof (teleinfo_contract.str_id));
                break;
              case TIC_ADS:
                strlcpy (teleinfo_contract.str_id, str_donnee, sizeof (teleinfo_contract.str_id));
                break;

              // Contract : Mode
              case TIC_CONFIG:
                if (strcmp (str_donnee, "CONSO") == 0) teleinfo_contract.mode = TIC_MODE_CONSO;
                else if (strcmp (str_donnee, "PROD") == 0) teleinfo_contract.mode = TIC_MODE_PROD;
                break;

              // Period Name
              case TIC_PTEC:
              case TIC_LTARF:
              case TIC_PTCOUR:
              case TIC_PTCOUR1:
                period = GetCommandCode (str_text, sizeof (str_text), str_donnee, kTeleinfoPeriod);
                if ((period >= 0) && (period < TIC_PERIOD_MAX)) teleinfo_contract.period = period;
                break;

              // Instant Current
              case TIC_EAIT:
              case TIC_IINST:
              case TIC_IRMS1:
              case TIC_IINST1:
                value = atol (str_donnee);
                is_valid = ((value >= 0) && (value < LONG_MAX));
                if (is_valid) teleinfo_phase[0].current = value;
                break;
              case TIC_IRMS2:
              case TIC_IINST2:
                value = atol (str_donnee);
                is_valid = ((value >= 0) && (value < LONG_MAX));
                if (is_valid) teleinfo_phase[1].current = value;
                break;
              case TIC_IRMS3:
              case TIC_IINST3:
                value = atol (str_donnee);
                is_valid = ((value >= 0) && (value < LONG_MAX));
                if (is_valid) teleinfo_phase[2].current = value; 
                teleinfo_contract.phase = 3; 
                break;

              // Instant Apparent Power
              case TIC_PAPP:
              case TIC_SINSTS:
                value = atol (str_donnee);
                is_valid = ((value >= 0) && (value < LONG_MAX));
                if (is_valid) teleinfo_meter.papp = value;

                break;

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

              // Active Power Counter
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
                value = atol (str_donnee);
                teleinfo_phase[0].volt_set = ((value > 0) && (value < LONG_MAX));
                if (teleinfo_phase[0].volt_set) teleinfo_phase[0].voltage = value;
                break;
              case TIC_UMOY1:
                value = atol (str_donnee);
                is_valid = ((value > 0) && (value < LONG_MAX));
                if (!teleinfo_phase[0].volt_set && is_valid) teleinfo_phase[0].voltage = value;
                break;
              case TIC_URMS2:
                value = atol (str_donnee);
                teleinfo_phase[1].volt_set = ((value > 0) && (value < LONG_MAX));
                if (teleinfo_phase[1].volt_set) teleinfo_phase[1].voltage = value;
                break;
              case TIC_UMOY2:
                value = atol (str_donnee);
                is_valid = ((value > 0) && (value < LONG_MAX));
                if (!teleinfo_phase[1].volt_set && is_valid) teleinfo_phase[1].voltage = value;
                break;
              case TIC_URMS3:
                value = atol (str_donnee);
                teleinfo_phase[2].volt_set = ((value > 0) && (value < LONG_MAX));
                if (teleinfo_phase[2].volt_set) teleinfo_phase[2].voltage = value;
                teleinfo_contract.phase = 3; 
                break;
              case TIC_UMOY3:
                value = atol (str_donnee);
                is_valid = ((value > 0) && (value < LONG_MAX));
                if (!teleinfo_phase[2].volt_set && is_valid) teleinfo_phase[2].voltage = value;
                teleinfo_contract.phase = 3; 
                break;

              // Contract : Maximum Current
              case TIC_ISOUSC:
                value = atol (str_donnee);
                is_valid = ((value > 0) && (value < LONG_MAX));
                if (is_valid) 
                {
                  value *= TELEINFO_VOLTAGE_REF;
                  if (teleinfo_contract.ssousc != value)
                  {
                    // update max power
                    teleinfo_contract.ssousc = value;
                    teleinfo_graph.pmax = teleinfo_contract.ssousc;

                    // update max current
                    teleinfo_contract.isousc = value;
                  }                   
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
                    // update max power
                    teleinfo_contract.ssousc = value;
                    teleinfo_graph.pmax = value;

                    // update max current
                    teleinfo_contract.isousc = value / TELEINFO_VOLTAGE_REF;
                  }                   

                }
                break;
              case TIC_PS:
                strlcpy (str_text, str_donnee, sizeof (str_text));
                pstr_match = strchr (str_text, 'k');
                if (pstr_match != nullptr) strcpy_P (pstr_match, PSTR ("000"));
                value = atol (str_text);
                is_valid = ((value > 0) && (value < LONG_MAX));
                if (is_valid)
                {
                  if (teleinfo_contract.ssousc != value)
                  {
                    // update max power
                    teleinfo_contract.ssousc = value;
                    teleinfo_graph.pmax = value;

                    // update max current
                    teleinfo_contract.isousc = value / TELEINFO_VOLTAGE_REF;
                  }                   
                }
                break;

              // Contract : Index suivant type de contrat
              case TIC_EAPS:
                strlcpy (str_text, str_donnee, sizeof (str_text));
                pstr_match = strchr (str_text, 'k');
                if (pstr_match != nullptr) strcpy_P (pstr_match, PSTR ("000"));
                counter = atoll (str_text);
                is_valid = (counter != LONG_LONG_MAX);
                if (is_valid) teleinfo_meter.index[0] = counter;
                break;
              case TIC_BASE:
              case TIC_HCHC:
              case TIC_EJPHN:
              case TIC_BBRHCJB:
              case TIC_EASF01:
                counter = atoll (str_donnee);
                is_valid = ((counter < LONG_LONG_MAX) && (counter > teleinfo_meter.index[0]));
                if (is_valid) teleinfo_meter.index[0] = counter;
                break;
              case TIC_HCHP:
              case TIC_EJPHPM:
              case TIC_BBRHPJB:
              case TIC_EASF02:
                counter = atoll (str_donnee);
                is_valid = ((counter < LONG_LONG_MAX) && (counter > teleinfo_meter.index[1]));
                if (is_valid) teleinfo_meter.index[1] = counter;
                break;
              case TIC_BBRHCJW:
              case TIC_EASF03:
                counter = atoll (str_donnee);
                is_valid = ((counter < LONG_LONG_MAX) && (counter > teleinfo_meter.index[2]));
                if (is_valid) teleinfo_meter.index[2] = counter;
                break;
              case TIC_BBRHPJW:
              case TIC_EASF04:
                counter = atoll (str_donnee);
                is_valid = ((counter < LONG_LONG_MAX) && (counter > teleinfo_meter.index[3]));
                if (is_valid) teleinfo_meter.index[3] = counter;
                break;
              case TIC_BBRHCJR:
              case TIC_EASF05:
                counter = atoll (str_donnee);
                is_valid = ((counter < LONG_LONG_MAX) && (counter > teleinfo_meter.index[4]));
                if (is_valid) teleinfo_meter.index[4] = counter;
                break;
              case TIC_BBRHPJR:
              case TIC_EASF06:
                counter = atoll (str_donnee);
                is_valid = ((counter < LONG_LONG_MAX) && (counter > teleinfo_meter.index[5]));
                if (is_valid) teleinfo_meter.index[5] = counter;
                break;
              case TIC_EASF07:
                counter = atoll (str_donnee);
                is_valid = ((counter < LONG_LONG_MAX) && (counter > teleinfo_meter.index[6]));
                if (is_valid) teleinfo_meter.index[6] = counter;
                break;
              case TIC_EASF08:
                counter = atoll (str_donnee);
                is_valid = ((counter < LONG_LONG_MAX) && (counter > teleinfo_meter.index[7]));
                if (is_valid) teleinfo_meter.index[7] = counter;
                break;
              case TIC_EASF09:
                counter = atoll (str_donnee);
                is_valid = ((counter < LONG_LONG_MAX) && (counter > teleinfo_meter.index[8]));
                if (is_valid) teleinfo_meter.index[8] = counter;
                break;
              case TIC_EASF10:
                counter = atoll (str_donnee);
                is_valid = ((counter < LONG_LONG_MAX) && (counter > teleinfo_meter.index[9]));
                if (is_valid) teleinfo_meter.index[9] = counter;
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

        // give control back to system to avoid watchdog
        yield ();

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
            for (index = 0; index < teleinfo_contract.nb_indexes; index ++) counter_wh += teleinfo_meter.index[index];
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

              // else, if apparent power is above 1, apply reduction to cosphi that will be reduced
              else if (papp_inc > 1)
              {
                cosphi = 0.5 * cosphi + 0.5 * 100 * 1 / papp_inc;
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
            else if (teleinfo_calc.papp_current_counter <= 2 * teleinfo_calc.papp_previous_increment)
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
              teleinfo_phase[0].papp = teleinfo_phase[0].papp / 2 + 3600000 / (long)message_ms * increment / 2;
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
            else if (teleinfo_calc.pact_current_counter <= 2 * teleinfo_calc.pact_previous_increment)
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
              teleinfo_phase[0].pact = teleinfo_phase[0].pact / 2 + 3600000 / (long)message_ms * increment / 2;
              teleinfo_calc.pact_previous_counter = teleinfo_calc.pact_current_counter;
              teleinfo_meter.nb_update++;
            }

            // avoid active power greater than apparent power
            if (teleinfo_phase[0].pact > teleinfo_phase[0].papp) teleinfo_phase[0].pact = teleinfo_phase[0].papp;

            // calculate cos phi
            teleinfo_phase[0].cosphi = 100 * (float)teleinfo_phase[0].pact / (float)teleinfo_phase[0].papp;

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
          if (teleinfo_phase[phase].papp > teleinfo_contract.ssousc * (long)teleinfo_config.percent_adjust / 100) teleinfo_message.overload = true;

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

        // update global active power counters
        Energy.total[0] = (float)teleinfo_meter.total_wh / 1000;
        Energy.import_active[0] = Energy.total[0];

        // declare received message
        teleinfo_message.received = true;
        break;

      // add other caracters to current line, removing all control caracters
      default:
        if (teleinfo_line.buffer_index < sizeof (teleinfo_line.buffer) - 1)
        {
          teleinfo_line.buffer[teleinfo_line.buffer_index] = byte_recv;
          teleinfo_line.buffer_index++;
        }
        break;
    }
  }
}

// Publish JSON if candidate (called 4 times per second)
void TeleinfoEvery250ms ()
{
  // if message should be sent, check which type to send
  if (teleinfo_message.send_msg)
  {
    teleinfo_message.send_meter = ((teleinfo_config.msg_type == TELEINFO_TYPE_BOTH) || (teleinfo_config.msg_type == TELEINFO_TYPE_METER));
    teleinfo_message.send_tic = ((teleinfo_config.msg_type == TELEINFO_TYPE_BOTH) || (teleinfo_config.msg_type == TELEINFO_TYPE_TIC));
    teleinfo_message.send_msg = false;
  }

  // if needed, publish meter or TIC JSON
  if (teleinfo_message.send_meter) TeleinfoPublishMeterJSON ();
  else if (teleinfo_message.send_tic) TeleinfoPublishTicJSON ();
}

// Calculate if some JSON should be published (called every second)
void TeleinfoEverySecond ()
{
  int period, phase;

  // increment update interval counter
  teleinfo_meter.interval_count++;
  teleinfo_meter.interval_count = teleinfo_meter.interval_count % (60 * teleinfo_config.update_interval);

  // if needed, update energy total (in kwh)
  if ((teleinfo_meter.status_rx == TIC_SERIAL_ACTIVE) && (teleinfo_meter.interval_count == 0))
  {
    EnergyUpdateTotal ();
    AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Total counter updated to %u Wh"), teleinfo_meter.total_wh);
  }

  // loop thru the periods and the phases, to update apparent power to the max on the period
  for (period = 0; period < TELEINFO_PERIOD_MAX; period++)
  {
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

#ifdef USE_UFILESYS
      // if max buffer time reached, flush to littlefs
      if (TimeReached (teleinfo_log[period].time_flush)) 
      {
        // save buffer to log file
        UfsCsvAppend (teleinfo_log[period].str_filename.c_str (), teleinfo_log[period].str_buffer.c_str (), false);
        teleinfo_log[period].str_buffer = "";

        // set next flush time
        teleinfo_log[period].time_flush = millis () + 1000 * ARR_TELEINFO_PERIOD_FLUSH[period];
      }
#endif    // USE_UFILESYS

    }

    // increment graph period counter and update graph data if needed
    if (teleinfo_period[period].counter == 0) TeleinfoGraphSaveData (period);
    teleinfo_period[period].counter ++;
    teleinfo_period[period].counter = teleinfo_period[period].counter % ARR_TELEINFO_PERIOD_WINDOW[period];
  }

  // check if message should be published
  if (teleinfo_message.overload && (teleinfo_config.msg_policy != TELEINFO_POLICY_NEVER))   teleinfo_message.send_msg = true;
  if (teleinfo_message.received && (teleinfo_config.msg_policy == TELEINFO_POLICY_MESSAGE)) teleinfo_message.send_msg = true;
  if (teleinfo_message.percent  && (teleinfo_config.msg_policy == TELEINFO_POLICY_PERCENT)) teleinfo_message.send_msg = true;
  
  // reset message flags
  teleinfo_message.overload = false;
  teleinfo_message.received = false;
  teleinfo_message.percent  = false;
}

// Generate JSON with TIC informations
void TeleinfoPublishTicJSON ()
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
  teleinfo_message.send_tic = false;
}

// Generate JSON with Meter informations
void TeleinfoPublishMeterJSON ()
{
  int   phase, value;
  long  power_max, power_app, power_act;
  float current;
  char  str_text[16];

  // Start METER section    {"Time":"xxxxxxxx","METER":{
  Response_P (PSTR ("{\"%s\":\"%s\",\"%s\":{"), D_JSON_TIME, GetDateAndTime (DT_LOCAL).c_str (), TELEINFO_JSON_METER);

  // METER basic data
  ResponseAppend_P (PSTR ("\"%s\":%d"),  TELEINFO_JSON_PHASE, teleinfo_contract.phase);
  ResponseAppend_P (PSTR (",\"%s\":%d"), TELEINFO_JSON_ISUB, teleinfo_contract.isousc);
  ResponseAppend_P (PSTR (",\"%s\":%d"), TELEINFO_JSON_PSUB, teleinfo_contract.ssousc);

  // METER adjusted maximum power
  power_max = teleinfo_contract.ssousc * (long)teleinfo_config.percent_adjust / 100;
  ResponseAppend_P (PSTR (",\"%s\":%d"), TELEINFO_JSON_PMAX, power_max);

  // loop to calculate apparent and active power
  power_app = 0;
  power_act = 0;
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    value = phase + 1;
    power_app += teleinfo_phase[phase].papp;
    power_act += teleinfo_phase[phase].pact;

    // U, Papp and Pact
    ResponseAppend_P (PSTR (",\"U%d\":%d"), value, teleinfo_phase[phase].voltage);
    ResponseAppend_P (PSTR (",\"P%d\":%d"), value, teleinfo_phase[phase].papp);
    ResponseAppend_P (PSTR (",\"W%d\":%d"), value, teleinfo_phase[phase].pact);

    // I
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
  if (teleinfo_contract.phase > 1)
  {
    ResponseAppend_P (PSTR (",\"P\":%d"), power_app);
    ResponseAppend_P (PSTR (",\"W\":%d"), power_act);
  }

  // end of METER section, publish JSON and process rules
  ResponseAppend_P (PSTR ("}}"));
  MqttPublishTeleSensor ();

  // Meter has been published
  teleinfo_message.send_meter = false;
}

// Show JSON status (for MQTT)
void TeleinfoShowJSON (bool append)
{
  // if telemetry call, check for JSON update according to update policy
  if (append && (teleinfo_config.msg_policy != TELEINFO_POLICY_NEVER)) teleinfo_message.send_msg = true;
}

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

// Display offload icons
void TeleinfoWebIconTic () { Webserver->send_P (200, PSTR ("image/png"), teleinfo_icon_tic_png, teleinfo_icon_tic_len); }

// Get specific argument as a value with min and max
int TeleinfoWebGetArgValue (const char* pstr_argument, int value_default, int value_min = 0, int value_max = 0)
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
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_TELEINFO_PAGE_TIC, D_TELEINFO_MESSAGE);

  // Teleinfo graph page button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_TELEINFO_PAGE_GRAPH, D_TELEINFO_GRAPH);
}

// Append Teleinfo configuration button to configuration page
void TeleinfoWebConfigButton ()
{
  // heater and meter configuration button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_TELEINFO_PAGE_CONFIG, D_TELEINFO_CONFIG);
}

// Append Teleinfo state to main page
void TeleinfoWebSensor ()
{
  int  index, phase, period;
  long percentage;
  char str_text[32];
  char str_header[32];
  char str_data[128];

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
          WSContentSend_P (TELEINFO_HTML_BAR, str_text, percentage, percentage);
        }

      // Teleinfo mode
      strcpy_P (str_header, PSTR (D_TELEINFO_MODE));
      GetTextIndexed (str_data, sizeof (str_data), teleinfo_contract.mode, kTeleinfoModeName);

      // Teleinfo contract power
      if (teleinfo_contract.ssousc > 0)
      {
        // header
        strlcat (str_header, "<br>", sizeof (str_header));
        strlcat (str_header, D_TELEINFO_CONTRACT, sizeof (str_header));

        // number of phases
        strcpy_P (str_text, PSTR ("<br>"));
        if (teleinfo_contract.phase > 1) sprintf_P (str_text, PSTR ("<br>%d x "), teleinfo_contract.phase);
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

      // get number of TIC messages received
      strcpy_P (str_header, PSTR (D_TELEINFO_MESSAGE));
      sprintf_P (str_data, PSTR ("%d"), teleinfo_meter.nb_message);

      // calculate TIC checksum errors percentage
      if ((teleinfo_meter.nb_error > 0) && (teleinfo_meter.nb_line > 0))
      {
        // calculate error percentage
        percentage = (long)(teleinfo_meter.nb_error * 100 / teleinfo_meter.nb_line);

        // append data
        strcpy (str_text, "");
        if (percentage > 0) sprintf_P (str_text, PSTR (" <small>(%d%%)</small>"), percentage);
        strlcat (str_data, str_text, sizeof (str_data));
      }

      // active power updates
      strlcat (str_header, "<br>", sizeof (str_header));
      strlcat (str_header, D_TELEINFO_UPDATE, sizeof (str_header));
      sprintf_P (str_text, PSTR ("<br>%d"), teleinfo_meter.nb_update);
      strlcat (str_data, str_text, sizeof (str_data));

      // get TIC reset
      if (teleinfo_meter.nb_reset > 0)
      {
        strlcat (str_header, "<br>", sizeof (str_header));
        strlcat (str_header, D_TELEINFO_RESET, sizeof (str_header));

        sprintf_P (str_text, PSTR ("<br>%d"), teleinfo_meter.nb_reset);
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
  int  index, value, rate_mode;
  uint16_t rate_baud;
  char str_text[32];
  char str_select[16];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess ()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg (F ("save")))
  {
    // parameter 'sendp' : set TIC messages diffusion policy
    WebGetArg (D_CMND_TELEINFO_MSG_POLICY, str_text, sizeof (str_text));
    if (strlen (str_text) > 0) teleinfo_config.msg_policy = atoi (str_text);

    // parameter 'sendt' : set data messages diffusion policy
    WebGetArg (D_CMND_TELEINFO_MSG_TYPE, str_text, sizeof (str_text));
    if (strlen (str_text) > 0) teleinfo_config.msg_type = atoi (str_text);

    // parameter 'ival' : set energy publication interval
    WebGetArg (D_CMND_TELEINFO_INTERVAL, str_text, sizeof (str_text));
    if (strlen (str_text) > 0) teleinfo_config.update_interval = atoi (str_text);

    // parameter 'adjust' : set contract power limit adjustment in %
    WebGetArg (D_CMND_TELEINFO_ADJUST, str_text, sizeof (str_text));
    if (strlen (str_text) > 0) teleinfo_config.percent_adjust = atoi (str_text);

    // get current and new teleinfo rate
    WebGetArg (D_CMND_TELEINFO_RATE, str_text, sizeof (str_text));
    if (strlen (str_text) > 0) teleinfo_config.baud_rate = TeleinfoValidateBaudRate (atoi (str_text));

#ifdef USE_UFILESYS

    // parameter 'nbday' : set number of daily historisation files
    WebGetArg (D_CMND_TELEINFO_MINIMIZE, str_text, sizeof (str_text));
    if (strlen (str_text) > 0) teleinfo_config.log_policy = TELEINFO_LOG_BUFFERED;
      else teleinfo_config.log_policy = TELEINFO_LOG_IMMEDIATE;

    // parameter 'nbday' : set number of daily historisation files
    WebGetArg (D_CMND_TELEINFO_NBDAILY, str_text, sizeof (str_text));
    if (strlen (str_text) > 0) teleinfo_config.nb_daily = max (atoi (str_text), 1);

    // parameter 'nbweek' : set number of weekly historisation files
    WebGetArg (D_CMND_TELEINFO_NBWEEKLY, str_text, sizeof (str_text));
    if (strlen (str_text) > 0) teleinfo_config.nb_weekly = max (atoi (str_text), 1);

#endif    // USE_UFILESYS

    // save configuration
    TeleinfoSaveConfig ();

    // ask for meter update
    teleinfo_message.send_msg = true;
  }

  // beginning of form
  WSContentStart_P (D_TELEINFO_CONFIG);
  WSContentSendStyle ();
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_TELEINFO_PAGE_CONFIG);

  // speed selection form
  WSContentSend_P (TELEINFO_FIELD_START, "📨", PSTR ("TIC Rate"));
  for (index = 0; index < nitems (ARR_TELEINFO_RATE); index++)
  {
    if (ARR_TELEINFO_RATE[index] == teleinfo_config.baud_rate) strcpy_P (str_select, PSTR ("checked")); else strcpy (str_select, "");
    itoa (ARR_TELEINFO_RATE[index], str_text, 10);
    WSContentSend_P (TELEINFO_INPUT_RADIO, D_CMND_TELEINFO_RATE, ARR_TELEINFO_RATE[index], str_select, str_text);
  }
  WSContentSend_P (TELEINFO_FIELD_STOP);

  // teleinfo message diffusion policy
  WSContentSend_P (TELEINFO_FIELD_START, "🧾", PSTR ("Message policy"));
  for (index = 0; index < TELEINFO_POLICY_MAX; index++)
  {
    if (index == teleinfo_config.msg_policy) strcpy_P (str_select, PSTR ("checked")); else strcpy (str_select, "");
    GetTextIndexed (str_text, sizeof (str_text), index, kTeleinfoMessagePolicy);
    WSContentSend_P (TELEINFO_INPUT_RADIO, D_CMND_TELEINFO_MSG_POLICY, index, str_select, str_text);
  }
  WSContentSend_P (TELEINFO_FIELD_STOP);

  // teleinfo message diffusion type
  WSContentSend_P (TELEINFO_FIELD_START, "📑", PSTR ("Message data"));
  for (index = 0; index < TELEINFO_TYPE_MAX; index++)
  {
    if (index == teleinfo_config.msg_type) strcpy_P (str_select, PSTR ("checked")); else strcpy (str_select, "");
    GetTextIndexed (str_text, sizeof (str_text), index, kTeleinfoMessageType);
    WSContentSend_P (TELEINFO_INPUT_RADIO, D_CMND_TELEINFO_MSG_TYPE, index, str_select, str_text);
  }
  WSContentSend_P (TELEINFO_FIELD_STOP);

  // teleinfo energy calculation parameters
  WSContentSend_P (TELEINFO_FIELD_START,  "⚡", PSTR ("Energy calculation"));
  WSContentSend_P (TELEINFO_INPUT_NUMBER, PSTR ("Contract acceptable overload (%)"), D_CMND_TELEINFO_ADJUST, TELEINFO_PERCENT_MIN, TELEINFO_PERCENT_MAX, teleinfo_config.percent_adjust);
  WSContentSend_P (TELEINFO_INPUT_NUMBER, PSTR ("Power saving interval to SRAM (mn)"), D_CMND_TELEINFO_INTERVAL, TELEINFO_INTERVAL_MIN, TELEINFO_INTERVAL_MAX, teleinfo_config.update_interval);
  WSContentSend_P (TELEINFO_FIELD_STOP);

#ifdef USE_UFILESYS

  // teleinfo historisation parameters
  WSContentSend_P (TELEINFO_FIELD_START,  "🗂️", PSTR ("Historisation"));
  if (teleinfo_config.log_policy == TELEINFO_LOG_BUFFERED) strcpy (str_select, "checked");
    else strcpy (str_select, "");
  WSContentSend_P (TELEINFO_INPUT_CHECKBOX, D_CMND_TELEINFO_MINIMIZE, str_select, PSTR ("Minimize write on filesystem"));
  WSContentSend_P (TELEINFO_INPUT_NUMBER, PSTR ("Number of daily files"), D_CMND_TELEINFO_NBDAILY, 1, 365, teleinfo_config.nb_daily);
  WSContentSend_P (TELEINFO_INPUT_NUMBER, PSTR ("Number of weekly files"), D_CMND_TELEINFO_NBWEEKLY, 0, 52, teleinfo_config.nb_weekly);
  WSContentSend_P (TELEINFO_FIELD_STOP);

#endif    // USE_UFILESYS

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
  WSContentSend_P (PSTR ("<div><a href='/'><img height=64 src='%s'></a></div>\n"), D_TELEINFO_PAGE_TIC_PNG);

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

// Get status update
void TeleinfoWebGraphUpdate ()
{
  int    phase, update;
  int    period, data, histo;
  char   str_text[16];

  // check graph parameters
  period = TeleinfoWebGetArgValue (D_CMND_TELEINFO_PERIOD, TELEINFO_PERIOD_LIVE,  0, TELEINFO_PERIOD_MAX - 1);
  data   = TeleinfoWebGetArgValue (D_CMND_TELEINFO_DATA,   TELEINFO_UNIT_VA, 0, TELEINFO_UNIT_MAX - 1);
  histo  = TeleinfoWebGetArgValue (D_CMND_TELEINFO_HISTO,  0, 0, 0);

  // start of data page
  WSContentBegin (200, CT_PLAIN);

  // chech for graph update (no updata if past historic file)
  if (histo > 0) teleinfo_period[period].updated = false;
  WSContentSend_P ("%d\n", teleinfo_period[period].updated);

  // check power and power difference for each phase
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    TeleinfoDisplayCurrentValue (data, phase, str_text, sizeof (str_text), 1); 
    WSContentSend_P ("%s\n", str_text);
  }

  // end of data page
  WSContentEnd ();

  // reset update flags
  teleinfo_period[period].updated = false;
}

// Display graph frame and time marks
void TeleinfoWebGraphTime (int period, int histo)
{
  int  index;
  long graph_left, graph_right, graph_width;  
  long unit_width, shift_unit, shift_width;  
  long graph_x;  
  uint32_t current_time;
  TIME_T   time_dst;
  char     str_text[8];

  // extract time data
  current_time = LocalTime ();

#ifdef USE_UFILESYS
  if (period >= TELEINFO_MEMORY_PERIOD_MAX) current_time = TeleinfoHistoGetStartTime (period, histo);
#endif    // USE_UFILESYS

  BreakTime (current_time, time_dst);

  // boundaries of SVG graph
  graph_left  = TELEINFO_GRAPH_PERCENT_START * TELEINFO_GRAPH_WIDTH / 100;
  graph_right = TELEINFO_GRAPH_PERCENT_STOP  * TELEINFO_GRAPH_WIDTH / 100;
  graph_width = graph_right - graph_left;

  // handle graph units according to period
  switch (period) 
  {
    case TELEINFO_PERIOD_LIVE:
      // calculate horizontal shift
      unit_width  = graph_width / 6;
      shift_unit  = time_dst.minute % 10;
      shift_width = unit_width - (unit_width * shift_unit / 10) - (unit_width * time_dst.second / 600);

      // calculate first time displayed by substracting (5 * 10mn + shift) to current time
      current_time -= 3000;
      current_time -= (shift_unit * 60); 

      // display 5 mn separation lines with time
      for (index = 0; index < 6; index++)
      {
        // convert back to date and increase time of 5mn
        BreakTime (current_time, time_dst);
        current_time += 600;

        // display separation line and time
        graph_x = graph_left + shift_width + (index * unit_width);
        WSContentSend_P (PSTR ("<line class='time' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n"), graph_x, 49, graph_x, 51);
        WSContentSend_P (PSTR ("<text class='time' x='%d' y='%d%%'>%02dh%02d</text>\n"), graph_x - 25, 55, time_dst.hour, time_dst.minute);
      }
      break;

    case TELEINFO_PERIOD_DAY:
      // calculate horizontal shift
      unit_width  = graph_width / 6;
      shift_unit  = time_dst.hour % 4;
      shift_width = unit_width - (unit_width * shift_unit / 4) - (unit_width * time_dst.minute / 240);

      // calculate first time displayed by substracting (5 * 4h + shift) to current time
      current_time -= 72000;
      current_time -= (shift_unit * 3600); 

      // display 4 hours separation lines with hour
      for (index = 0; index < 6; index++)
      {
        // convert back to date and increase time of 4h
        BreakTime (current_time, time_dst);
        current_time += 14400;

        // display separation line and time
        graph_x = graph_left + shift_width + (index * unit_width);
        WSContentSend_P (PSTR ("<line class='time' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n"), graph_x, 49, graph_x, 51);
        WSContentSend_P (PSTR ("<text class='time' x='%d' y='%d%%'>%02dh</text>\n"), graph_x - 15, 55, time_dst.hour);
      }
      break;

    case TELEINFO_PERIOD_WEEK:
      // calculate horizontal shift
      unit_width = graph_width / 7;
      shift_width = unit_width - (unit_width * time_dst.hour / 24) - (unit_width * time_dst.minute / 1440);

      // display day lines with day name
      time_dst.day_of_week --;
      for (index = 0; index < 7; index++)
      {
        // calculate next week day
        time_dst.day_of_week ++;
        time_dst.day_of_week = time_dst.day_of_week % 7;

        // display month separation line and week day (first days or current day after 6pm)
        graph_x = graph_left + shift_width + (index * unit_width);
        WSContentSend_P (PSTR ("<line class='time' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n"), graph_x, 49, graph_x, 51);
        if ((index < 6) || (time_dst.hour >= 18))
        {
          GetTextIndexed (str_text, sizeof (str_text), time_dst.day_of_week, kTeleinfoWeekDay);
          WSContentSend_P (PSTR ("<text class='time' x='%d' y='%d%%'>%s</text>\n"), graph_x + 50, 53, str_text);
        }
      }
      break;
  }
}

// Display graph curve
void TeleinfoWebGraphCurve (int period, int data, int phase, int histo)
{
  bool is_valid, display_peak, first_point;
  long index, position, value;
  long graph_left, graph_right, graph_width;  
  long graph_delta, graph_x, graph_y, graph_last_x;

  // check if peak values should be displayed
  display_peak = (period >=TELEINFO_MEMORY_PERIOD_MAX);

  // boundaries of SVG graph
  graph_left  = TELEINFO_GRAPH_PERCENT_START * TELEINFO_GRAPH_WIDTH / 100 + 1;
  graph_right = TELEINFO_GRAPH_PERCENT_STOP  * TELEINFO_GRAPH_WIDTH / 100;
  graph_width = graph_right - graph_left;

  // display according to data type
  switch (data) 
  {
    // ------------------
    //   Apparent Power
    // ------------------

    case TELEINFO_UNIT_VA:
      // if needed, display peak values
      if (display_peak)
      {
        // display peak values graph
        WSContentSend_P ("<polyline class='pk%d' points='", phase); 
        graph_last_x = graph_left;
        for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
        {
          // calculate x position and get curve value
          graph_x = graph_left + (index * graph_width / TELEINFO_GRAPH_SAMPLE);

          // if power is defined, calculate and display point position
          value = TeleinfoGetGraphData (TELEINFO_HISTO_PEAK_VA, period, phase, index, true);
          if ((value != LONG_MAX) && (teleinfo_graph.pmax > 0))
          {
            graph_last_x = graph_x;
            graph_y = teleinfo_config.height - (value * teleinfo_config.height / teleinfo_graph.pmax);
            WSContentSend_P ("%d,%d ", graph_x, graph_y);
          }
        }
        WSContentSend_P (PSTR("'/>\n"), graph_last_x, teleinfo_config.height);
      }

      // display apparent power graph
      first_point = false;
      WSContentSend_P ("<path class='ph%d' ", phase); 
      graph_last_x = graph_left;
      for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
      {
        // calculate x position
        graph_x = graph_left + (index * graph_width / TELEINFO_GRAPH_SAMPLE);

        // if power is defined, calculate and display point position
        value = TeleinfoGetGraphData (TELEINFO_HISTO_VA, period, phase, index, true);
        if ((value != LONG_MAX) && (teleinfo_graph.pmax > 0))
        {
          // first point
          if (!first_point)
          {
            first_point = true;
            WSContentSend_P ("d='M%d %d ", graph_x, teleinfo_config.height); 
          }

          // current point
          graph_last_x = graph_x;
          graph_y = teleinfo_config.height - (value * teleinfo_config.height / teleinfo_graph.pmax);
          WSContentSend_P ("L%d %d ", graph_x, graph_y);
        }
      }
      WSContentSend_P (PSTR("L%d %d Z'/>\n"), graph_last_x, teleinfo_config.height);

      break;

    // ------------------
    //    Active Power
    // ------------------

    case TELEINFO_UNIT_W:
      // display active power graph
      WSContentSend_P ("<path class='ph%d' ", phase); 
      graph_last_x = graph_left;
      first_point = false;
      for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
      {
        // calculate x position
        graph_x = graph_left + (index * graph_width / TELEINFO_GRAPH_SAMPLE);

        // if power is defined, calculate and display point position
        value = TeleinfoGetGraphData (TELEINFO_HISTO_W, period, phase, index, true);
        if ((value != LONG_MAX) && (teleinfo_graph.pmax > 0))
        {
          // first point
          if (!first_point)
          {
            first_point = true;
            WSContentSend_P ("d='M%d %d ", graph_x, teleinfo_config.height); 
          }

          // current point
          graph_last_x = graph_x;
          graph_y = teleinfo_config.height - (value * teleinfo_config.height / teleinfo_graph.pmax);
          WSContentSend_P ("L%d %d ", graph_x, graph_y);
        }

      }
      WSContentSend_P (PSTR(" L%d %d Z'/>\n"), graph_last_x, teleinfo_config.height);

      break;

    // --------------
    //     Voltage
    // --------------

    case TELEINFO_UNIT_V:
      // if needed, display curve with peak voltage
      if (display_peak)
      {
        // loop to display voltage peak
        WSContentSend_P ("<polyline class='ph%d' points='", phase); 
        graph_last_x = graph_left;
        for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
        {
          // calculate x position
          graph_x = graph_left + (index * graph_width / TELEINFO_GRAPH_SAMPLE);

          // if max voltage is defined, calculate and display point position as delta from min & max range
          value = TeleinfoGetGraphData (TELEINFO_HISTO_PEAK_V, period, phase, index, true);
          if (value != LONG_MAX)
          {
            graph_last_x = graph_x;
            value   = value + TELEINFO_GRAPH_VOLTAGE_RANGE - teleinfo_graph.vmax;
            graph_y = teleinfo_config.height - (value * teleinfo_config.height / TELEINFO_GRAPH_VOLTAGE_RANGE);
            WSContentSend_P ("%d,%d ", graph_x, graph_y);
          }
        }
        WSContentSend_P (PSTR("'/>\n"), graph_last_x, teleinfo_config.height);
      }

      // display minimum voltage graph
      WSContentSend_P ("<path class='ph%d' ", phase);
      first_point = false;
      for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
      {
        // calculate x position
        graph_x = graph_left + (index * graph_width / TELEINFO_GRAPH_SAMPLE);

        // if voltage is defined, calculate and display point position as delta from min & max range
        value = TeleinfoGetGraphData (TELEINFO_HISTO_V, period, phase, index, true);
        if (value != LONG_MAX)
        {
          // first point
          if (!first_point)
          {
            first_point = true;
            WSContentSend_P ("d='M%d %d ", graph_x, teleinfo_config.height); 
          }

          // current point
          graph_last_x = graph_x;
          value   = value + TELEINFO_GRAPH_VOLTAGE_RANGE - teleinfo_graph.vmax;
          graph_y = teleinfo_config.height - (value * teleinfo_config.height / TELEINFO_GRAPH_VOLTAGE_RANGE);
          WSContentSend_P ("L%d %d ", graph_x, graph_y);
        }
      }
      WSContentSend_P (PSTR(" L%d %d Z'/>\n"), graph_last_x, teleinfo_config.height);

      break;
    
    // --------------
    //     Cos Phi
    // --------------

    case TELEINFO_UNIT_COSPHI:
      // display cos phi graph
      WSContentSend_P ("<polyline class='ph%d' points='", phase); 
      for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
      {
        // calculate x position
        graph_x = graph_left + (graph_width * index / TELEINFO_GRAPH_SAMPLE);

        // if data is defined, calculate and display point position
        value = TeleinfoGetGraphData (TELEINFO_HISTO_COSPHI, period, phase, index, true);
        if ((value != LONG_MAX) && (teleinfo_graph.pmax > 0))
        {
          graph_y = teleinfo_config.height - (value * teleinfo_config.height / 100);
          WSContentSend_P ("%d,%d ", graph_x, graph_y);
        }
      }
      WSContentSend_P (PSTR("'/>\n"));

      break;
  }
}

// Graph curves data update
void TeleinfoWebGraphData ()
{
  int  phase, period, data, histo;
  char str_phase[8];
  char str_text[32];

  // get numerical argument values
  period = TeleinfoWebGetArgValue (D_CMND_TELEINFO_PERIOD, TELEINFO_PERIOD_LIVE,  0, TELEINFO_PERIOD_MAX - 1);
  data   = TeleinfoWebGetArgValue (D_CMND_TELEINFO_DATA,   TELEINFO_UNIT_VA, 0, TELEINFO_UNIT_MAX - 1);
  histo  = TeleinfoWebGetArgValue (D_CMND_TELEINFO_HISTO,  0, 0, 0);

  // check phase display argument
  strcpy (str_phase, "");
  if (Webserver->hasArg (D_CMND_TELEINFO_PHASE)) WebGetArg (D_CMND_TELEINFO_PHASE, str_phase, sizeof (str_phase));
  for (phase = strlen (str_phase); phase < teleinfo_contract.phase; phase++) strcat_P (str_phase, PSTR ("1"));

  // start of SVG graph
  WSContentBegin (200, CT_HTML);
  WSContentSend_P (PSTR ("<svg viewBox='%d %d %d %d'>\n"), 0, 0, TELEINFO_GRAPH_WIDTH, teleinfo_config.height);

  // SVG style 
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));
  WSContentSend_P (PSTR ("polyline {stroke-width:1.5;opacity:0.8;fill:none;}\n"));
  WSContentSend_P (PSTR ("path {stroke-width:1.5;opacity:0.5;}\n"));

  for (phase = 0; phase < teleinfo_contract.phase; phase++) 
  {
    // phase colors
    GetTextIndexed (str_text, sizeof (str_text), phase, kTeleinfoGraphColorPhase);
    WSContentSend_P (PSTR ("path.ph%d {stroke:%s;fill:%s;}\n"), phase, str_text, str_text);
    WSContentSend_P (PSTR ("polyline.ph%d {stroke:%s;}\n"), phase, str_text);

    // peak colors
    GetTextIndexed (str_text, sizeof (str_text), phase, kTeleinfoGraphColorPeak);
    WSContentSend_P (PSTR ("polyline.pk%d {stroke:%s;stroke-dasharray:1 4;}\n"), phase, str_text);
  }

  WSContentSend_P (PSTR ("line.time {stroke-width:1;stroke:white;}\n"));
  WSContentSend_P (PSTR ("text.time {font-size:1.2rem;fill:grey;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // -----------------
  //   data curves
  // -----------------

#ifdef USE_UFILESYS

  bool file_exists;
  char str_filename[UFS_FILENAME_SIZE];

  // if data is for current period file, force flush for the period
  if (histo == 0) TeleinfoHistoFlushData (period);

  // if not live data, open data file
  file_exists = TeleinfoHistoGetFilename (period, histo, str_filename);
  if (file_exists) UfsCsvOpen (str_filename, true);

  // loop thru phases to display them if asked
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
    if (str_phase[phase] != '0') TeleinfoWebGraphCurve (period, data, phase, histo);

  // close historisation data file
  if (file_exists) UfsCsvClose (UFS_CSV_ACCESS_READ);

#else

  // loop thru phases to display them if asked
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
    if (str_phase[phase] != '0') TeleinfoWebGraphCurve (period, data, phase, 0);

#endif    // USE_UFILESYS

  // ------------
  //     Time
  // ------------

  // display time units
  TeleinfoWebGraphTime (period, histo);

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentEnd ();
}

// Graph frame
void TeleinfoWebGraphFrame ()
{
  int   nb_digit = -1;
  int   index, phase, period, data;  
  int   graph_left, graph_right, graph_width;
  long  value;
  float font_size, unit, unit_min, unit_max;
  char  str_unit[8];
  char  str_text[8];
  char  arr_label[5][8];

  // get parameters
  period = TeleinfoWebGetArgValue (D_CMND_TELEINFO_PERIOD, TELEINFO_PERIOD_LIVE,  0, TELEINFO_PERIOD_MAX  - 1);
  data   = TeleinfoWebGetArgValue (D_CMND_TELEINFO_DATA,   TELEINFO_UNIT_W,       0, TELEINFO_UNIT_MAX - 1);

  // set labels according to type of data
  switch (data) 
  {
    case TELEINFO_UNIT_VA:
      // set number of digits according to maximum power
      if (teleinfo_graph.pmax >= 10000) nb_digit = 0;
      else if (teleinfo_graph.pmax >= 1000) nb_digit = 1;

      // set values label
      itoa (0, arr_label[0], 10);
      TeleinfoDisplayValue (TELEINFO_UNIT_MAX, teleinfo_graph.pmax / 4,     str_text, sizeof (str_text), nb_digit);
      strcpy (arr_label[1], str_text);
      TeleinfoDisplayValue (TELEINFO_UNIT_MAX, teleinfo_graph.pmax / 2,     str_text, sizeof (str_text), nb_digit);
      strcpy (arr_label[2], str_text);
      TeleinfoDisplayValue (TELEINFO_UNIT_MAX, teleinfo_graph.pmax * 3 / 4, str_text, sizeof (str_text), nb_digit);
      strcpy (arr_label[3], str_text);
      TeleinfoDisplayValue (TELEINFO_UNIT_MAX, teleinfo_graph.pmax,         str_text, sizeof (str_text), nb_digit);
      strcpy (arr_label[4], str_text);
      break;

    case TELEINFO_UNIT_W:
      // set number of digits according to maximum power
      if (teleinfo_graph.pmax >= 10000) nb_digit = 0;
      else if (teleinfo_graph.pmax >= 1000) nb_digit = 1;

      // set values label
      itoa (0, arr_label[0], 10);
      TeleinfoDisplayValue (TELEINFO_UNIT_MAX, teleinfo_graph.pmax / 4,     str_text, sizeof (str_text), nb_digit);
      strcpy (arr_label[1], str_text);
      TeleinfoDisplayValue (TELEINFO_UNIT_MAX, teleinfo_graph.pmax / 2,     str_text, sizeof (str_text), nb_digit);
      strcpy (arr_label[2], str_text);
      TeleinfoDisplayValue (TELEINFO_UNIT_MAX, teleinfo_graph.pmax * 3 / 4, str_text, sizeof (str_text), nb_digit);
      strcpy (arr_label[3], str_text);
      TeleinfoDisplayValue (TELEINFO_UNIT_MAX, teleinfo_graph.pmax,         str_text, sizeof (str_text), nb_digit);
      strcpy (arr_label[4], str_text);
      break;

    case TELEINFO_UNIT_V:
      // set values label
      itoa (teleinfo_graph.vmax - TELEINFO_GRAPH_VOLTAGE_RANGE,         arr_label[0], 10);
      itoa (teleinfo_graph.vmax - TELEINFO_GRAPH_VOLTAGE_RANGE * 3 / 4, arr_label[1], 10);
      itoa (teleinfo_graph.vmax - TELEINFO_GRAPH_VOLTAGE_RANGE / 2,     arr_label[2], 10);
      itoa (teleinfo_graph.vmax - TELEINFO_GRAPH_VOLTAGE_RANGE / 4,     arr_label[3], 10);
      itoa (teleinfo_graph.vmax, arr_label[4], 10);
      break;

    case TELEINFO_UNIT_COSPHI:
      // set values label
      strcpy_P (arr_label[0], PSTR ("0"));
      strcpy_P (arr_label[1], PSTR ("0.25"));
      strcpy_P (arr_label[2], PSTR ("0.50"));
      strcpy_P (arr_label[3], PSTR ("0.75"));
      strcpy_P (arr_label[4], PSTR ("1"));
      break;

    default:
      for (index = 0; index < 5; index ++) strcpy (arr_label[index], "");
      break;
  }

  // boundaries of SVG graph
  graph_left  = TELEINFO_GRAPH_PERCENT_START * TELEINFO_GRAPH_WIDTH / 100;
  graph_right = TELEINFO_GRAPH_PERCENT_STOP * TELEINFO_GRAPH_WIDTH / 100;
  graph_width = graph_right - graph_left;

  // start of SVG graph
  WSContentBegin (200, CT_HTML);
  WSContentSend_P (PSTR ("<svg viewBox='%d %d %d %d'>\n"), 0, 0, TELEINFO_GRAPH_WIDTH, teleinfo_config.height);

  // SVG style 
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));
  WSContentSend_P (PSTR ("rect {stroke:grey;fill:none;}\n"));
  WSContentSend_P (PSTR ("line.dash {stroke:grey;stroke-width:1;stroke-dasharray:8;}\n"));
  WSContentSend_P (PSTR ("line.time {stroke:white;stroke-width:1;}\n"));
  WSContentSend_P (PSTR ("text.power {font-size:1.1rem;stroke:white;fill:white;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // graph frame
  WSContentSend_P (PSTR ("<rect x='%d%%' y='%d%%' width='%d%%' height='%d%%' rx='10' />\n"), TELEINFO_GRAPH_PERCENT_START, 0, TELEINFO_GRAPH_PERCENT_STOP - TELEINFO_GRAPH_PERCENT_START, 100);

  // graph separation lines
  WSContentSend_P (TELEINFO_HTML_DASH, TELEINFO_GRAPH_PERCENT_START, 25, TELEINFO_GRAPH_PERCENT_STOP, 25);
  WSContentSend_P (TELEINFO_HTML_DASH, TELEINFO_GRAPH_PERCENT_START, 50, TELEINFO_GRAPH_PERCENT_STOP, 50);
  WSContentSend_P (TELEINFO_HTML_DASH, TELEINFO_GRAPH_PERCENT_START, 75, TELEINFO_GRAPH_PERCENT_STOP, 75);

  // get unit label
  strcpy (str_text, "");
  if (nb_digit != -1) strcpy_P (str_text, PSTR ("k"));
  GetTextIndexed (str_unit, sizeof (str_unit), data, kTeleinfoGraphDisplay);
  strlcat (str_text, str_unit, sizeof (str_text));

  // units graduation
  WSContentSend_P (PSTR ("<text class='power' x='%d%%' y='%d%%'>%s</text>\n"), TELEINFO_GRAPH_PERCENT_STOP + 1, 4, str_text);
  WSContentSend_P (TELEINFO_HTML_POWER, 1, 4,  arr_label[4]);
  WSContentSend_P (TELEINFO_HTML_POWER, 1, 26, arr_label[3]);
  WSContentSend_P (TELEINFO_HTML_POWER, 1, 51, arr_label[2]);
  WSContentSend_P (TELEINFO_HTML_POWER, 1, 76, arr_label[1]);
  WSContentSend_P (TELEINFO_HTML_POWER, 1, 99, arr_label[0]);

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentEnd ();
}

// Graph public page
void TeleinfoWebGraphPage ()
{
  bool display;
  int  index, phase, period, data, histo, choice, counter;  
  long percentage;
  char str_phase[8];
  char str_type[8];
  char str_text[16];
  char str_date[16];

  // get numerical argument values
  period = TeleinfoWebGetArgValue (D_CMND_TELEINFO_PERIOD, TELEINFO_PERIOD_LIVE,  0, TELEINFO_PERIOD_MAX  - 1);
  data   = TeleinfoWebGetArgValue (D_CMND_TELEINFO_DATA,   TELEINFO_UNIT_VA, 0, TELEINFO_UNIT_MAX - 1);
  histo  = TeleinfoWebGetArgValue (D_CMND_TELEINFO_HISTO,  0, 0, 0);
  teleinfo_config.height = TeleinfoWebGetArgValue (D_CMND_TELEINFO_HEIGHT, teleinfo_config.height, 100, INT_MAX);

  // check phase display argument
  strcpy (str_phase, "");
  if (Webserver->hasArg (D_CMND_TELEINFO_PHASE)) WebGetArg (D_CMND_TELEINFO_PHASE, str_phase, sizeof (str_phase));
  for (phase = strlen (str_phase); phase < teleinfo_contract.phase; phase++) strcat_P (str_phase, PSTR ("1"));

  // check unit increment
  if (Webserver->hasArg (D_CMND_TELEINFO_INCREMENT)) 
  {
    if ((data == TELEINFO_UNIT_VA) || (data == TELEINFO_UNIT_W)) teleinfo_graph.pmax += teleinfo_contract.ssousc / 4;
    else if (data == TELEINFO_UNIT_V) teleinfo_graph.vmax += TELEINFO_GRAPH_VOLTAGE_STEP;
  }

  // check unit decrement
  if (Webserver->hasArg (D_CMND_TELEINFO_DECREMENT)) 
  {
    if ((data == TELEINFO_UNIT_VA) || (data == TELEINFO_UNIT_W)) teleinfo_graph.pmax -= teleinfo_contract.ssousc / 4;
    else if (data == TELEINFO_UNIT_V) teleinfo_graph.vmax -= TELEINFO_GRAPH_VOLTAGE_STEP;

    teleinfo_graph.pmax = max (teleinfo_graph.pmax, teleinfo_contract.ssousc / 4);
    teleinfo_graph.vmax = max (teleinfo_graph.vmax, (long)TELEINFO_GRAPH_VOLTAGE_RANGE);
  }

  // beginning of form without authentification
  WSContentStart_P (D_TELEINFO_GRAPH, false);
  WSContentSend_P (PSTR ("</script>\n"));

  // page data refresh script
  WSContentSend_P (PSTR ("<script type='text/javascript'>\n"));
  WSContentSend_P (PSTR ("function update()\n"));
  WSContentSend_P (PSTR ("{\n"));
  WSContentSend_P (PSTR (" httpUpd=new XMLHttpRequest();\n"));
  WSContentSend_P (PSTR (" httpUpd.open('GET','%s?period=%d&data=%d&histo=%d',true);\n"), D_TELEINFO_PAGE_GRAPH_UPD, period, data, histo);
  WSContentSend_P (PSTR (" httpUpd.send();\n"));
  WSContentSend_P (PSTR (" httpUpd.onreadystatechange=function()\n"));
  WSContentSend_P (PSTR (" {\n"));
  WSContentSend_P (PSTR ("  if (httpUpd.responseText.length>0)\n"));
  WSContentSend_P (PSTR ("  {\n"));
  WSContentSend_P (PSTR ("   arr_param=httpUpd.responseText.split('\\n');\n"));
  WSContentSend_P (PSTR ("   if (arr_param[0]==1)\n"));
  WSContentSend_P (PSTR ("   {\n"));
  WSContentSend_P (PSTR ("    str_random=Math.floor(Math.random()*100000);\n"));
  WSContentSend_P (PSTR ("    document.getElementById('data').data='%s?period=%d&data=%d&phase=%s&rnd='+str_random;\n"), D_TELEINFO_PAGE_GRAPH_DATA, period, data, str_phase);
  WSContentSend_P (PSTR ("   }\n"));
  WSContentSend_P (PSTR ("   num_param=arr_param.length;\n"));
  WSContentSend_P (PSTR ("   for (i=1;i<num_param;i++)\n"));
  WSContentSend_P (PSTR ("   {\n"));
  WSContentSend_P (PSTR ("    document.getElementById('p'+i).textContent=arr_param[i];\n"));
  WSContentSend_P (PSTR ("   }\n"));
  WSContentSend_P (PSTR ("  }\n"));
  WSContentSend_P (PSTR (" }\n"));
  WSContentSend_P (PSTR ("}\n"));
  WSContentSend_P (PSTR ("setInterval(function() {update();},5000);\n"));
  WSContentSend_P (PSTR ("</script>\n"));

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {margin:0.25rem auto;padding:0.1rem 0px;text-align:center;vertical-align:middle;}\n"));
  WSContentSend_P (PSTR ("div a {color:white;}\n"));

  WSContentSend_P (PSTR (".title {font-size:2rem;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.phase {display:inline-block;width:90px;padding:0.2rem;margin:0.2rem;border-radius:8px;}\n"));
  WSContentSend_P (PSTR ("div.phase span.power {font-size:1rem;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.phase span.volt {font-size:0.8rem;font-style:italic;}\n"));
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    GetTextIndexed (str_text, sizeof (str_text), phase, kTeleinfoGraphColorPhase);
    WSContentSend_P (PSTR ("div.ph%d {color:%s;border:1px %s solid;}\n"), phase, str_text, str_text);    
  }

  WSContentSend_P (PSTR ("div.choice {display:inline-block;font-size:1rem;padding:0px;margin:auto 5px;border:1px #666 solid;background:none;color:#fff;border-radius:6px;}\n"));
  WSContentSend_P (PSTR ("div.choice div {background:#666;}\n"));
  WSContentSend_P (PSTR ("div.choice a div {background:none;}\n"));

  WSContentSend_P (PSTR ("div.item {display:inline-block;padding:0.2rem auto;margin:1px;border:none;border-radius:4px;}\n"));
  WSContentSend_P (PSTR ("div.histo {font-size:0.9rem;padding:0px;margin-top:0px;}\n"));
  WSContentSend_P (PSTR ("div.day {width:50px;}\n"));
  WSContentSend_P (PSTR ("div.week {width:90px;}\n"));
  WSContentSend_P (PSTR ("div a div.item:hover {background:#aaa;}\n"));

  WSContentSend_P (PSTR ("div.incr {padding:0px;color:white;border:1px #666 solid;border-radius:6px;}\n"));
  WSContentSend_P (PSTR ("div.left {float:left;margin-left:6%%;}\n"));
  WSContentSend_P (PSTR ("div.right {float:right;margin-right:6%%;}\n"));
  
  WSContentSend_P (PSTR ("div.period {width:70px;}\n"));
  WSContentSend_P (PSTR ("div.data {width:50px;}\n"));
  WSContentSend_P (PSTR ("div.size {width:25px;}\n"));

  WSContentSend_P (PSTR ("select {background:#666;color:white;font-size:0.8rem;margin:1px;border:solid #666 2px;border-radius:4px;}\n"));

  percentage = (100 * teleinfo_config.height / TELEINFO_GRAPH_WIDTH) + 2; 
  WSContentSend_P (PSTR (".svg-container {position:relative;width:100%%;max-width:%dpx;padding-top:%d%%;margin:auto;}\n"), TELEINFO_GRAPH_WIDTH, percentage);
  WSContentSend_P (PSTR (".svg-content {display:inline-block;position:absolute;top:0;left:0;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // page body
  WSContentSend_P (PSTR ("</head>\n"));
  WSContentSend_P (PSTR ("<body>\n"));

  // device name
  WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText(SET_DEVICENAME));

  // display icon
  WSContentSend_P (PSTR ("<div><a href='/'><img height=64 src='%s'></a></div>\n"), D_TELEINFO_PAGE_TIC_PNG);

  // -----------------
  //      Values
  // -----------------
  WSContentSend_P (PSTR ("<div>\n"));
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    // display phase inverted link
    strcpy (str_text, str_phase);
    display = (str_text[phase] != '0');
    if (display) str_text[phase] = '0'; else str_text[phase] = '1';
    WSContentSend_P (PSTR ("<a href='%s?period=%d&data=%d&histo=%d&phase=%s'>"), D_TELEINFO_PAGE_GRAPH, period, data, histo, str_text);

    // display phase data
    if (display) strcpy (str_text, ""); else strcpy_P (str_text, PSTR ("disabled"));
    WSContentSend_P (PSTR ("<div class='phase ph%d %s'>"), phase, str_text);    
    TeleinfoDisplayCurrentValue (data, phase, str_text, sizeof (str_text), 1);
    WSContentSend_P (PSTR ("<span class='power' id='p%d'>%s</span>"), phase + 1, str_text);
    WSContentSend_P (PSTR ("</div></a>\n"));
  }
  WSContentSend_P (PSTR ("</div>\n"));

  WSContentSend_P (PSTR ("<div>\n"));     // line

  // -------------------
  //      Unit range
  // -------------------

  WSContentSend_P (PSTR ("<div class='incr left'>"));
  WSContentSend_P (PSTR ("<a href='%s?period=%d&data=%d&histo=%d&phase=%s&incr'><div class='item size'>%s</div></a>"), D_TELEINFO_PAGE_GRAPH, period, data, histo, str_phase, "+");
  WSContentSend_P (PSTR ("<br>"));
  WSContentSend_P (PSTR ("<a href='%s?period=%d&data=%d&histo=%d&phase=%s&decr'><div class='item size'>%s</div></a>"), D_TELEINFO_PAGE_GRAPH, period, data, histo, str_phase, "-");
  WSContentSend_P (PSTR ("</div>\n"));      // choice

  // -----------------
  //      Height
  // -----------------

  WSContentSend_P (PSTR ("<div class='incr right'>\n"));
  index = teleinfo_config.height + TELEINFO_GRAPH_STEP;
  WSContentSend_P (PSTR ("<a href='%s?period=%d&data=%d&histo=%d&height=%d&phase=%s'><div class='item size'>%s</div></a>"), D_TELEINFO_PAGE_GRAPH, period, data, histo, index, str_phase, "+");
  WSContentSend_P (PSTR ("<br>"));
  index = teleinfo_config.height - TELEINFO_GRAPH_STEP;
  WSContentSend_P (PSTR ("<a href='%s?period=%d&data=%d&histo=%d&height=%d&phase=%s'><div class='item size'>%s</div></a>"), D_TELEINFO_PAGE_GRAPH, period, data, histo, index, str_phase, "-");
  WSContentSend_P (PSTR ("</div>\n"));      // choice

  // -----------------
  //     Data type
  // -----------------


  WSContentSend_P (PSTR ("<div class='choice'>\n"));
  for (index = 0; index < TELEINFO_UNIT_MAX; index++)
  {
    GetTextIndexed (str_text, sizeof (str_text), index, kTeleinfoGraphDisplay);
    if (data != index) WSContentSend_P (PSTR ("<a href='%s?period=%d&data=%d&histo=%d&phase=%s'>"), D_TELEINFO_PAGE_GRAPH, period, index, histo, str_phase);
    WSContentSend_P (PSTR ("<div class='item data'>%s</div>"), str_text);
    if (data != index) WSContentSend_P (PSTR ("</a>"));
    WSContentSend_P (PSTR ("\n"));
  }
  WSContentSend_P (PSTR ("</div>\n"));      // choice

  // start second line of parameters
  WSContentSend_P (PSTR ("</div>\n"));      // line

  // -----------------
  //      Period 
  // -----------------

  WSContentSend_P (PSTR ("<form method='post' action='/tic-graph?period=%d&data=%d&phase=%s'>\n"), period, data, str_phase);
  WSContentSend_P (PSTR ("<div>\n"));       // line

  WSContentSend_P (PSTR ("<div class='choice'>\n"));
  for (index = 0; index < TELEINFO_MEMORY_PERIOD_MAX; index++)
  {
    // get period label
    GetTextIndexed (str_text, sizeof (str_text), index, kTeleinfoGraphPeriod);

    // set button according to active state
    if (period != index) WSContentSend_P (PSTR ("<a href='%s?period=%d&data=%d&phase=%s'>"), D_TELEINFO_PAGE_GRAPH, index, data, str_phase);
    WSContentSend_P (PSTR ("<div class='item period'>%s</div>"), str_text);
    if (period != index) WSContentSend_P (PSTR ("</a>"));
    WSContentSend_P (PSTR ("\n"));
  }

#ifdef USE_UFILESYS

  for (index = TELEINFO_MEMORY_PERIOD_MAX; index < TELEINFO_PERIOD_MAX; index++)
  {
    // get period label
    GetTextIndexed (str_text, sizeof (str_text), index, kTeleinfoGraphPeriod);

    // set button according to active state
    if (period == index)
    {
      // get number of saved periods
      if (period == TELEINFO_PERIOD_DAY) choice = teleinfo_config.nb_daily;
      else if (period == TELEINFO_PERIOD_WEEK) choice = teleinfo_config.nb_weekly;
      else choice = 0;

      WSContentSend_P (PSTR ("<select name='histo' id='histo' onchange='this.form.submit();'>\n"));

      for (counter = 0; counter < choice; counter++)
      {
        // check if file exists
        if (TeleinfoHistoGetFilename (period, counter, str_text))
        {
          TeleinfoHistoGetDate (period, counter, str_date);
          if (counter == histo) strcpy_P (str_text, PSTR (" selected")); else strcpy (str_text, "");
          WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>\n"), counter, str_text, str_date);
        }
      }

      WSContentSend_P (PSTR ("</select>\n"));      
    }
    else WSContentSend_P (PSTR ("<a href='%s?period=%d&data=%d&phase=%s'><div class='item period'>%s</div></a>\n"), D_TELEINFO_PAGE_GRAPH, index, data, str_phase, str_text);
  }

# endif     // USE_UFILESYS

  WSContentSend_P (PSTR ("</div>\n"));        // choice
  WSContentSend_P (PSTR ("</div>\n"));        // line
  WSContentSend_P (PSTR ("</form>\n"));

  // -----------------
  //      Graph 
  // -----------------

  // display graph base and data
  WSContentSend_P (PSTR ("<div class='svg-container'>\n"));
  WSContentSend_P (PSTR ("<object class='svg-content' id='base' type='image/svg+xml' width='100%%' height='100%%' data='%s?period=%d&data=%d'></object>\n"), D_TELEINFO_PAGE_GRAPH_FRAME, period, data);
  WSContentSend_P (PSTR ("<object class='svg-content' id='data' type='image/svg+xml' width='100%%' height='100%%' data='%s?period=%d&data=%d&histo=%d&phase=%s&ts=0'></object>\n"), D_TELEINFO_PAGE_GRAPH_DATA, period, data, histo, str_phase);
  WSContentSend_P (PSTR ("</div>\n"));

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
    case FUNC_SAVE_AT_MIDNIGHT:
      TeleinfoRotateLogs ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kTeleinfoCommands, TeleinfoCommand);
      break;
    case FUNC_EVERY_250_MSECOND:
      TeleinfoEvery250ms ();
      break;
    case FUNC_EVERY_SECOND:
      TeleinfoEverySecond ();
      break;
    case FUNC_JSON_APPEND:
      TeleinfoShowJSON (true);
      break;
    case FUNC_LOOP:
      if (teleinfo_meter.status_rx == TIC_SERIAL_ACTIVE) TeleinfoReceiveData ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      // config
      Webserver->on (FPSTR (D_TELEINFO_PAGE_CONFIG), TeleinfoWebPageConfigure);

      // TIC message
      Webserver->on (FPSTR (D_TELEINFO_PAGE_TIC), TeleinfoWebPageTic);
      Webserver->on (FPSTR (D_TELEINFO_PAGE_TIC_UPD), TeleinfoWebTicUpdate);

      // graph
      Webserver->on (FPSTR (D_TELEINFO_PAGE_GRAPH),TeleinfoWebGraphPage);
      Webserver->on (FPSTR (D_TELEINFO_PAGE_GRAPH_FRAME), TeleinfoWebGraphFrame);
      Webserver->on (FPSTR (D_TELEINFO_PAGE_GRAPH_DATA), TeleinfoWebGraphData);
      Webserver->on (FPSTR (D_TELEINFO_PAGE_GRAPH_UPD), TeleinfoWebGraphUpdate);

      // icons
      Webserver->on (FPSTR (D_TELEINFO_PAGE_TIC_PNG), TeleinfoWebIconTic);
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

#endif   // USE_TELEINFO
#endif   // USE_ENERGY_SENSOR
