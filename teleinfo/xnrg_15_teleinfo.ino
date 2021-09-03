/*
  xnrg_16_teleinfo.ino - France Teleinfo energy sensor support for Sonoff-Tasmota

  Copyright (C) 2019-2021  Nicolas Bernaerts

  Connexions :
    * On ESP8266, Teleinfo Rx must be connected to GPIO3 (as it must be forced to 7E1)
    * On ESP32, Teleinfo Rx must NOT be connected to GPIO3 (to avoid nasty ESP32 ESP_Restart bug where ESP32 hangs if restarted when Rx is under heavy load)
    * Teleinfo EN may be connected to any GPIO starting from GPIO5 ...
   
  Settings are stored using Settings->weight parameters :
    - Settings->weight_reference   = TIC message publication policy
    - Settings->weight_calibration = Meter data publication policy
    - Settings->weight_item        = Energy update to SRAM interval (in mn)
    - Settings->weight_max         = Energy minimum power delta to do calculation (in VA/W)
    - Settings->weight_change      = Energy cpntract percentage adjustment for maximum acceptable power (in %)

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
                          Complete change in VA, W and cos phi measurement based on transmission time
                          Add PME/PMI ACE6000 management
                          Add energy update interval configuration
                          Add TIC to TCP bridge (command 'TICtcp 8888' to publish teleinfo stream on port 8888)
                          Add LittleFS historic data record

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

#ifdef ESP8266
#include <FS.h>
#include <LittleFS.h>
#endif

#include <TasmotaSerial.h>
TasmotaSerial *teleinfo_serial = nullptr;

/*************************************************\
 *               Variables
\*************************************************/

// declare teleinfo energy driver and sensor
#define XNRG_15   15
#define XSNS_99   99

// teleinfo constant
#define TELEINFO_INIT_DELAY             10        // delay before serial port initialization (sec)
#define TELEINFO_VOLTAGE                230       // default voltage provided
#define TELEINFO_VOLTAGE_MINIMUM        100       // minimum voltage for value to be acceptable
#define TELEINFO_VOLTAGE_REF            200       // voltage reference for max power calculation
#define TELEINFO_VOLTAGE_LOW            220       // minimum acceptable voltage
#define TELEINFO_VOLTAGE_HIGH           240       // maximum acceptable voltage
#define TELEINFO_POWER_COSPHI           200       // minimum instant power to do cosphi calculation
#define TELEINFO_INDEX_MAX              12        // maximum number of total power counters
#define TELEINFO_INTERVAL_MIN           1         // energy calculation minimum interval (1mn)
#define TELEINFO_INTERVAL_MAX           720       // energy calculation maximum interval (12h)
#define TELEINFO_DELTA_MIN              1         // minimum energy delta acceptable to do calculation (1 W/VA)
#define TELEINFO_DELTA_MAX              1000      // maximum energy delta acceptable to do calculation (1 kW/kVA)
#define TELEINFO_PERCENT_MIN            1         // minimum acceptable percentage of energy contract
#define TELEINFO_PERCENT_MAX            200       // maximum acceptable percentage of energy contract
#define TELEINFO_PERCENT_CHANGE         5         // 5% of power change to publish JSON

// graph data
#define TELEINFO_GRAPH_WIDTH            1200      // graph width
#define TELEINFO_GRAPH_HEIGHT           600       // default graph height
#define TELEINFO_GRAPH_STEP             200       // graph height mofification step
#define TELEINFO_GRAPH_PERCENT_START    5         // start position of graph window
#define TELEINFO_GRAPH_PERCENT_STOP     95        // stop position of graph window
#define TELEINFO_GRAPH_VOLTAGE_MIN      200       // minimum voltage
#define TELEINFO_GRAPH_VOLTAGE_MAX      260       // maximum voltage

// string size
#define TELEINFO_CONTRACTID_MAX         16        // max length of TIC contract ID
#define TELEINFO_PART_MAX               4         // maximum number of parts in a line
#define TELEINFO_LINE_QTY               74        // maximum number of lines handled in a TIC message
#define TELEINFO_LINE_MAX               128       // maximum size of a received TIC line
#define TELEINFO_KEY_MAX                12        // maximum size of a TIC etiquette

// embedded TCP server
#define TELEINFO_TCP_CLIENTS            2         // maximum number of TCP connexion

// ESP8266/ESP32 specificities
#ifdef ESP32
  #define TELEINFO_DATA_MAX             96        // maximum size of a TIC donnee
#else       // ESP8266
  #define TELEINFO_DATA_MAX             32        // maximum size of a TIC donnee
  #define LITTLEFS                      LittleFS
#endif      // ESP32 & ESP8266

// LITTLEFS specificities
#ifdef USE_UFILESYS
  #define TELEINFO_GRAPH_PERIOD_MAX     1         // memory graph for LIVE only
  #define TELEINFO_GRAPH_SAMPLE         720       // number of samples per graph data (should divide 3600)
#else
  #define TELEINFO_GRAPH_PERIOD_MAX     3         // memory graph for LIVE, DAY and WEEK
  #define TELEINFO_GRAPH_SAMPLE         300       // number of samples per graph data (should divide 3600)
#endif    // USE_UFILESYS

// commands : MQTT
#define D_CMND_TELEINFO_ENABLE          "enable"
#define D_CMND_TELEINFO_RATE            "rate"
#define D_CMND_TELEINFO_TELE            "tele"
#define D_CMND_TELEINFO_METER           "meter"
#define D_CMND_TELEINFO_INTERVAL        "ival"
#define D_CMND_TELEINFO_DELTA           "delta"
#define D_CMND_TELEINFO_ADJUST          "adj"
#define D_CMND_TELEINFO_TCP             "tcp"

// commands : Web
#define D_CMND_TELEINFO_MODE            "mode"
#define D_CMND_TELEINFO_ETH             "eth"
#define D_CMND_TELEINFO_PHASE           "phase"
#define D_CMND_TELEINFO_PERIOD          "period"
#define D_CMND_TELEINFO_HISTO           "histo"
#define D_CMND_TELEINFO_HEIGHT          "height"
#define D_CMND_TELEINFO_DATA            "data"

// JSON TIC extensions
#define TELEINFO_JSON_METER             "METER"
#define TELEINFO_JSON_TIC               "TIC"
#define TELEINFO_JSON_PERIOD            "PERIOD"
#define TELEINFO_JSON_PHASE             "PHASE"
#define TELEINFO_JSON_ID                "ID"
#define TELEINFO_JSON_IREF              "IREF"
#define TELEINFO_JSON_PREF              "PREF"
#define TELEINFO_JSON_PMAX              "PMAX"

// interface strings
#define D_TELEINFO                      "Teleinfo"
#define D_TELEINFO_MESSAGE              "Message"
#define D_TELEINFO_GRAPH                "Graph"
#define D_TELEINFO_CONTRACT             "Contract"
#define D_TELEINFO_HEURES               "Heures"
#define D_TELEINFO_ERROR                "Errors"
#define D_TELEINFO_MODE                 "Mode"
#define D_TELEINFO_RESET                "Message reset"
#define D_TELEINFO_PERIOD               "Period"
#define D_TELEINFO_CHECKSUM             "Checksum"
#define D_TELEINFO_BAUD                 "Bauds"
#define D_TELEINFO_DATA                 "Data"
#define D_TELEINFO_DIFFUSION            "Diffusion"

// Historic data files
#define TELEINFO_HISTO_DAY_MAX          7
#define TELEINFO_HISTO_WEEK_MAX         4
#define TELEINFO_HISTO_LINE_LENGTH      86
#define TELEINFO_HISTO_COLUMN_MAX       16
#define D_TELEINFO_HISTO_DAY            "day"
#define D_TELEINFO_HISTO_FILE_TODAY     "/today.csv"
#define D_TELEINFO_HISTO_FILE_PASTDAY   "/day-%d.csv"
#define D_TELEINFO_HISTO_WEEK           "week"
#define D_TELEINFO_HISTO_FILE_WEEK      "/week.csv"
#define D_TELEINFO_HISTO_FILE_PASTWEEK  "/week-%d.csv"

// web URL
const char D_TELEINFO_PAGE_CONFIG[]      PROGMEM = "/tic-cfg";
const char D_TELEINFO_PAGE_TIC[]         PROGMEM = "/tic-msg";
const char D_TELEINFO_PAGE_TIC_UPD[]     PROGMEM = "/tic-msg.upd";
const char D_TELEINFO_PAGE_GRAPH[]       PROGMEM = "/tic-graph";
const char D_TELEINFO_PAGE_GRAPH_FRAME[] PROGMEM = "/tic-base.svg";
const char D_TELEINFO_PAGE_GRAPH_DATA[]  PROGMEM = "/tic-data.svg";
const char D_TELEINFO_PAGE_GRAPH_UPD[]   PROGMEM = "/tic-graph.upd";
const char D_TELEINFO_PAGE_DATA_JSON[]   PROGMEM = "/tic-data.json";
const char D_TELEINFO_PAGE_TIC_PNG[]     PROGMEM = "/tic.png";

// web strings
const char D_TELEINFO_CONFIG[]     PROGMEM = "Configure Teleinfo";
const char TELEINFO_INPUT_RADIO[]  PROGMEM = "<p><input type='radio' name='%s' value='%d' %s>%s</p>\n";
const char TELEINFO_INPUT_NUMBER[] PROGMEM = "<span>%s<br><input type='number' name='%s' min='%d' max='%d' step='1' value='%d'></span>";
const char TELEINFO_FIELD_START[]  PROGMEM = "<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>\n";
const char TELEINFO_FIELD_STOP[]   PROGMEM = "</fieldset></p><br>\n";
const char TELEINFO_HTML_POWER[]   PROGMEM = "<text class='power' x='%d%%' y='%d%%'>%s</text>\n";
const char TELEINFO_HTML_DASH[]    PROGMEM = "<line class='dash' x1='%d%%' y1='%d%%' x2='%d%%' y2='%d%%' />\n";
const char TELEINFO_HTML_BAR[]     PROGMEM = "<tr><div style='margin:4px;padding:0px;background-color:#ddd;border-radius:4px;'><div style='font-size:0.75rem;font-weight:bold;padding:0px;text-align:center;border:1px solid #bbb;border-radius:4px;color:#444;background-color:%s;width:%d%%;'>%d%%</div></div></tr>\n";

// TIC - MQTT commands : TICenable, TICrate, TICtele, TICmeter, TICival, TICdelta, TICadj and TICtcp
const char kTeleinfoCommands[] PROGMEM = "TIC" "|" D_CMND_TELEINFO_ENABLE "|" D_CMND_TELEINFO_RATE "|" D_CMND_TELEINFO_TELE "|" D_CMND_TELEINFO_METER "|" D_CMND_TELEINFO_INTERVAL "|" D_CMND_TELEINFO_DELTA "|" D_CMND_TELEINFO_ADJUST "|" D_CMND_TELEINFO_TCP;
void (* const TeleinfoCommand[])(void) PROGMEM = { &CmndTeleinfoEnable, &CmndTeleinfoRate, &CmndTeleinfoTIC, &CmndTeleinfoData, &CmndTeleinfoInterval, &CmndTeleinfoDelta, &CmndTeleinfoAdjust, &CmndTeleinfoTCP };

// TIC - specific etiquettes
enum TeleinfoEtiquette { TIC_NONE, TIC_ADCO, TIC_ADSC, TIC_PTEC, TIC_NGTF, TIC_EAIT, TIC_IINST, TIC_IINST1, TIC_IINST2, TIC_IINST3, TIC_ISOUSC, TIC_PS, TIC_PAPP, TIC_SINSTS, TIC_BASE, TIC_EAST, TIC_HCHC, TIC_HCHP, TIC_EJPHN, TIC_EJPHPM, TIC_BBRHCJB, TIC_BBRHPJB, TIC_BBRHCJW, TIC_BBRHPJW, TIC_BBRHCJR, TIC_BBRHPJR, TIC_ADPS, TIC_ADIR1, TIC_ADIR2, TIC_ADIR3, TIC_URMS1, TIC_URMS2, TIC_URMS3, TIC_UMOY1, TIC_UMOY2, TIC_UMOY3, TIC_IRMS1, TIC_IRMS2, TIC_IRMS3, TIC_SINSTS1, TIC_SINSTS2, TIC_SINSTS3, TIC_PTCOUR, TIC_PTCOUR1, TIC_PREF, TIC_PCOUP, TIC_LTARF, TIC_EASF01, TIC_EASF02, TIC_EASF03, TIC_EASF04, TIC_EASF05, TIC_EASF06, TIC_EASF07, TIC_EASF08, TIC_EASF09, TIC_EASF10, TIC_ADS, TIC_CONFIG, TIC_EAPS, TIC_EAS, TIC_EAPPS, TIC_PREAVIS, TIC_MAX };
const char kTeleinfoEtiquetteName[] PROGMEM = "|ADCO|ADSC|PTEC|NGTF|EAIT|IINST|IINST1|IINST2|IINST3|ISOUSC|PS|PAPP|SINSTS|BASE|EAST|HCHC|HCHP|EJPHN|EJPHPM|BBRHCJB|BBRHPJB|BBRHCJW|BBRHPJW|BBRHCJR|BBRHPJR|ADPS|ADIR1|ADIR2|ADIR3|URMS1|URMS2|URMS3|UMOY1|UMOY2|UMOY3|IRMS1|IRMS2|IRMS3|SINSTS1|SINSTS2|SINSTS3|PTCOUR|PTCOUR1|PREF|PCOUP|LTARF|EASF01|EASF02|EASF03|EASF04|EASF05|EASF06|EASF07|EASF08|EASF09|EASF10|ADS|CONFIG|EAP_s|EA_s|EAPP_s|PREAVIS";

// TIC - modes and rates
enum TeleinfoMode { TIC_MODE_UNDEFINED, TIC_MODE_HISTORIC, TIC_MODE_STANDARD, TIC_MODE_CONSO, TIC_MODE_PROD };
const char kTeleinfoModeName[] PROGMEM = "|Historique|Standard|PME Conso|PME Prod";
const uint16_t ARR_TELEINFO_RATE[] = { 1200, 9600, 19200 }; 

// TIC - tarifs                              [  Toutes   ] [ Creuses       Pleines   ] [ Normales   Pointe Mobile] [Creuses Bleu  Creuses Blanc  Creuses Rouge  Pleines Bleu  Pleines Blanc  Pleines Rouge] [ Pointe   Pointe Mobile  Hiver        Pleines     Creuses      Pleines Hiver   Creuses Hiver   Pleines Ete   Creuses Ete    Pleines Demi Saison   Creuses Demi Saison   Juillet Aout]  [Pointe    Pleines Hiver  Creuses Hiver  Pleines Ete  Creuses Ete] [   Base    ] [Pleines     Creuses    ]
enum TeleinfoPeriod                        { TIC_HISTO_TH, TIC_HISTO_HC, TIC_HISTO_HP, TIC_HISTO_HN, TIC_HISTO_PM, TIC_HISTO_CB,  TIC_HISTO_CW,  TIC_HISTO_CR,  TIC_HISTO_PB, TIC_HISTO_PW,  TIC_HISTO_PR,  TIC_STD_P, TIC_STD_PM,    TIC_STD_HH,  TIC_STD_HP, TIC_STD_HC,  TIC_STD_HPH,    TIC_STD_HCH,    TIC_STD_HPE,  TIC_STD_HCE,   TIC_STD_HPD,          TIC_STD_HCD,          TIC_STD_JA,    TIC_STD_1, TIC_STD_2,     TIC_STD_3,     TIC_STD_4,   TIC_STD_5,   TIC_STD_BASE, TIC_STD_HPL, TIC_STD_HCR };
const int ARR_TELEINFO_PERIOD_INDEX[]    = { 1,            2,            2,            2,            2,            6,             6,             6,             6,            6,             6,             12,        12,            12,          12,         12,          12,             12,             12,           12,            12,                   12,                   12,            5,         5,             5,             5,           5,           1,            2,           2           };
const char kTeleinfoPeriod[] PROGMEM     = "TH..|HC..|HP..|HN..|PM..|HCJB|HCJW|HCJR|HPJB|HPJW|HPJR|P|PM|HH|HP|HC|HPH|HCH|HPE|HCE|HPD|HCD|JA|1|2|3|4|5|BASE|HEURE PLEINE|HEURE CREUSE";
const char kTeleinfoPeriodName[] PROGMEM = "Toutes|Creuses|Pleines|Normales|Pointe Mobile|Creuses Bleu|Creuses Blanc|Creuses Rouge|Pleines Bleu|Pleines Blanc|Pleines Rouge|Pointe|Pointe Mobile|Hiver|Pleines|Creuses|Pleines Hiver|Creuses Hiver|Pleines Ete|Creuses Ete|Pleines Demi-saison|Creuses Demi-saison|Juillet-Aout|Pointe|Pleines Hiver|Creuses Hiver|Pleines Ete|Creuses Ete|Base|Pleines|Creuses";

// TIC - data diffusion policy
enum TeleinfoConfigDiffusion { TELEINFO_POLICY_NEVER, TELEINFO_POLICY_MESSAGE, TELEINFO_POLICY_PERCENT, TELEINFO_POLICY_TELEMETRY, TELEINFO_POLICY_MAX };
const char TELEINFO_CFG_LABEL0[] PROGMEM = "Never";
const char TELEINFO_CFG_LABEL1[] PROGMEM = "Every TIC message";
const char TELEINFO_CFG_LABEL2[] PROGMEM = "When Power fluctuates (± 2%)";
const char TELEINFO_CFG_LABEL3[] PROGMEM = "With Telemetry only";
const char *const ARR_TELEINFO_CFG_LABEL[] = { TELEINFO_CFG_LABEL0, TELEINFO_CFG_LABEL1, TELEINFO_CFG_LABEL2, TELEINFO_CFG_LABEL3 };

// graph - periods
enum TeleinfoGraphPeriod { TELEINFO_PERIOD_LIVE, TELEINFO_PERIOD_DAY, TELEINFO_PERIOD_WEEK, TELEINFO_PERIOD_MAX };    // available graph periods
const char kTeleinfoGraphPeriod[] PROGMEM = "Live|Day|Week";                                                                           // period labels
const long ARR_TELEINFO_PERIOD_SAMPLE[] = { 3600 / TELEINFO_GRAPH_SAMPLE, 86400 / TELEINFO_GRAPH_SAMPLE, 604800 / TELEINFO_GRAPH_SAMPLE };                                                                           // number of seconds between samples

// graph - display
enum TeleinfoGraphDisplay { TELEINFO_UNIT_VA, TELEINFO_UNIT_W, TELEINFO_UNIT_V, TELEINFO_UNIT_COSPHI, TELEINFO_UNIT_MAX };                  // available graph displays
const char kTeleinfoGraphDisplay[] PROGMEM = "VA|W|V|cosφ";                                                                                 // data display labels

// graph - phase colors
const char TELEINFO_PHASE_COLOR0[] PROGMEM = "#5dade2";    // blue
const char TELEINFO_PHASE_COLOR1[] PROGMEM = "#f5b041";    // orange
const char TELEINFO_PHASE_COLOR2[] PROGMEM = "#52be80";    // green
const char *const ARR_TELEINFO_PHASE_COLOR[] = { TELEINFO_PHASE_COLOR0, TELEINFO_PHASE_COLOR1, TELEINFO_PHASE_COLOR2 };

// graph - week days name
const char TELEINFO_DAY_SUN[] PROGMEM = "Sun";
const char TELEINFO_DAY_MON[] PROGMEM = "Mon";
const char TELEINFO_DAY_TUE[] PROGMEM = "Tue";
const char TELEINFO_DAY_WED[] PROGMEM = "Wed";
const char TELEINFO_DAY_THU[] PROGMEM = "Thu";
const char TELEINFO_DAY_FRI[] PROGMEM = "Fri";
const char TELEINFO_DAY_SAT[] PROGMEM = "Sat";
const char *const arr_week_day[] = { TELEINFO_DAY_SUN, TELEINFO_DAY_MON, TELEINFO_DAY_TUE, TELEINFO_DAY_WED, TELEINFO_DAY_THU, TELEINFO_DAY_FRI, TELEINFO_DAY_SAT };

// power calculation modes
enum TeleinfoPowerCalculation { TIC_POWER_UNDEFINED, TIC_POWER_PROVIDED, TIC_POWER_GLOBAL_COUNTER, TIC_POWER_INCREMENT };
enum TeleinfoPowerTarget { TIC_POWER_UPDATE_COUNTER, TIC_POWER_UPDATE_PAPP, TIC_POWER_UPDATE_PACT };

// serial port management
enum TeleinfoSerialStatus { TIC_SERIAL_INIT, TIC_SERIAL_ACTIVE, TIC_SERIAL_STOPPED, TIC_SERIAL_FAILED };

// teleinfo calculation : apparent power (VA) and Active Power (W) 
struct tic_calc_power {
  uint8_t  type;                                // type of power in the structure
  long     ratio;                               // phase concerned
  long     value;                               // counter used to calculate power
  long     init_value;                          // initial value of counter
  long     comp_value;                          // comparaison value (to calculate cos phi)
  uint32_t init_time;                           // last timestamp where power was read
  uint32_t comp_time;                           // last time comparaison value was provided
};
static tic_calc_power teleinfo_papp[ENERGY_MAX_PHASES]; 
static tic_calc_power teleinfo_pact[ENERGY_MAX_PHASES]; 

// teleinfo : TCP server
static struct {
  WiFiServer *server = nullptr;                 // TCP server pointer
  WiFiClient client[TELEINFO_TCP_CLIENTS];      // TCP clients
  uint8_t    client_next = 0;                   // next client slot
  char       buffer[TELEINFO_DATA_MAX];         // data transfer buffer
  uint8_t    buffer_index = 0;                  // buffer current index
} teleinfo_tcp;

// TIC : current line being received
static struct {
  char    separator;                            // detected separator
  char    buffer[TELEINFO_LINE_MAX];            // reception buffer
  uint8_t buffer_index = 0;                     // buffer current index
} teleinfo_line;

// TIC : current message
struct tic_line {
  char str_etiquette[TELEINFO_KEY_MAX];         // TIC line etiquette
  char str_donnee[TELEINFO_DATA_MAX];           // TIC line donnee
  char checksum;
};
static struct {
  bool overload   = false;                      // overload has been detected
  bool received   = false;                      // one full message has been received
  bool percent    = false;                      // power has changed of more than 1% on one phase
  bool send_tic   = false;                      // flag to ask to send TIC JSON
  bool send_meter = false;                      // flag to ask to send Meter JSON
  int  line_index = 0;                          // index of current received message line
  int  line_max   = 0;                          // max number of lines in a message
  int  length     = INT_MAX;                    // length of message     
  uint32_t timestamp = UINT32_MAX;              // timestamp of message (ms)
  tic_line line[TELEINFO_LINE_QTY];             // array of message lines
} teleinfo_message;

// teleinfo : contract data
static struct {
  int  phase  = 1;                              // number of phases
  int  mode   = TIC_MODE_UNDEFINED;             // meter mode
  int  period = -1;                             // current tarif period
  int  nb_indexes     = -1;                     // number of indexes in current contract      
  int  percent_adjust = 0;                      // percentage adjustment to maximum contract power
  long voltage = TELEINFO_VOLTAGE;              // contract reference voltage
  long isousc  = 0;                             // contract max current per phase
  long ssousc  = 0;                             // contract max power per phase
  char str_id[TELEINFO_CONTRACTID_MAX];         // contract reference (adco or ads)
} teleinfo_contract;

// teleinfo : power meter
static struct {
  uint8_t  status_rx   = TIC_SERIAL_INIT;       // Teleinfo Rx initialisation status
  uint8_t  enable_rx   = UINT8_MAX;             // pin used to enable/disable Teleinfo Rx
  uint16_t baud_rate   = 1200;                  // meter transmission rate (bauds)
  int  interval_count  = 0;                     // energy publication counter      
  int  update_interval = 720;                   // update interval
  int  source_papp = TIC_POWER_UNDEFINED;       // apparent power mode should be defined
  int  source_pact = TIC_POWER_GLOBAL_COUNTER;  // active power is calculated from global counters (not thru delta increments)
  long papp        = 0;                         // current apparent power 
  int  policy_tic  = 0;                         // publishing policy for TIC data
  int  policy_data = 0;                         // publishing policy for meter values
  long power_delta = 1;                         // minimum delta for power calculation
  long long total_wh = 0;                       // total of all indexes of active power
  long long index[TELEINFO_INDEX_MAX];          // array of indexes of different tarif periods
  long long nb_message = 0;                     // total number of messages sent by the meter
  long long nb_data    = 0;                     // total number of received lines
  long long nb_error   = 0;                     // total number of checksum errors
  long nb_reset        = 0;                     // total number of message reset sent by the meter
} teleinfo_meter;

// teleinfo : actual data per phase
struct tic_phase {
  bool volt_set;                                // voltage set in current message
  long voltage;                                 // instant voltage
  long current;                                 // instant current
  long papp;                                    // instant apparent power
  long pact;                                    // instant active power
  long papp_last;                               // last published apparent power
  long cosphi;                                  // cos phi (x1000)
}; 
static tic_phase teleinfo_phase[ENERGY_MAX_PHASES];

// teleinfo : calculation periods data
struct tic_period {
  bool updated;                                 // flag to ask for graph update
  int  index;                                   // current array index per refresh period
  long counter;                                 // counter in seconds of current refresh period

  // --- limit values for graph display ---
  long vmin;                                    // graph minimum voltage
  long vmax;                                    // graph maximum voltage
  long pmax;                                    // graph maximum power

  // --- arrays for current refresh period (per phase) ---
  long volt_low[ENERGY_MAX_PHASES];            // peak low voltage during refresh period
  long volt_high[ENERGY_MAX_PHASES];           // peak high voltage during refresh period
  long pact_high[ENERGY_MAX_PHASES];           // peak active power during refresh period
  long papp_high[ENERGY_MAX_PHASES];           // peak apparent power during refresh period
  long volt_sum[ENERGY_MAX_PHASES];            // sum of voltage during refresh period
  long cosphi_sum[ENERGY_MAX_PHASES];          // sum of cos phi during refresh period
  long long pact_sum[ENERGY_MAX_PHASES];       // sum of active power during refresh period
  long long papp_sum[ENERGY_MAX_PHASES];       // sum of apparent power during refresh period
}; 
static tic_period teleinfo_period[TELEINFO_PERIOD_MAX];

// teleinfo : graph data
struct tic_graph {
  uint8_t arr_pact[ENERGY_MAX_PHASES][TELEINFO_GRAPH_SAMPLE];     // array of active power graph values
  uint8_t arr_papp[ENERGY_MAX_PHASES][TELEINFO_GRAPH_SAMPLE];     // array of apparent power graph values
  uint8_t arr_volt[ENERGY_MAX_PHASES][TELEINFO_GRAPH_SAMPLE];     // array min and max voltage delta
  uint8_t arr_cosphi[ENERGY_MAX_PHASES][TELEINFO_GRAPH_SAMPLE];   // array of cos phi
};
static struct {
  int  height  = TELEINFO_GRAPH_HEIGHT;
  long papp[ENERGY_MAX_PHASES];                 // apparent power to save
  long pact[ENERGY_MAX_PHASES];                 // active power to save
  long volt[ENERGY_MAX_PHASES];                 // voltage to save
  int  cosphi[ENERGY_MAX_PHASES];               // cosphi to save
  tic_graph data[TELEINFO_GRAPH_PERIOD_MAX];
} teleinfo_graph;

#ifdef USE_UFILESYS

// teleinfo : historic records management
static struct {
  bool     mounted   = false;                   // flag to indicate filesystem is mounted
  uint32_t timestamp = 0;                       // last timestamp record for today's data file (HHMM)

  File  file;                                   // opened file
  bool  opened;                                 // opened file : flag
  int   idx_line;                               // opened file : index of current line
  char  str_line[TELEINFO_HISTO_LINE_LENGTH];   // opened file : last read line
  int   nb_value;                               // opened file : number of values in last line
  char* pstr_value[TELEINFO_HISTO_COLUMN_MAX];  // opened file : array of values in last line
} teleinfo_histo;

#endif    // USE_UFILESYS

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

// validate teleinfo baud rate
uint16_t TeleinfoValidateBaudRate (uint16_t baud_rate)
{
  bool is_conform = false;
  int  index;

  // check if rate is within allowed rates
  for (index = 0; index < nitems (ARR_TELEINFO_RATE); index ++) if (baud_rate == ARR_TELEINFO_RATE[index]) is_conform = true;

  // if non standard rate, set to default 1200
  if (!is_conform) baud_rate = 1200;
  
  return baud_rate;
}

// validate TIC message publication policy
int TeleinfoValidatePolicy (int policy)
{
  // check boundaries
  if ((policy < 0) || (policy >= TELEINFO_POLICY_MAX)) policy = TELEINFO_POLICY_TELEMETRY;
  return policy;
}

// validate energy update interval
int TeleinfoValidateUpdateInterval (int interval)
{
  // check boundaries
  if ((interval < TELEINFO_INTERVAL_MIN) || (interval > TELEINFO_INTERVAL_MAX)) interval = TELEINFO_INTERVAL_MAX;
  return interval;
}

// validate minimum power delta needed to do calculation
int TeleinfoValidatePowerDelta (int delta)
{
  // check boundaries
  if (delta < 1) delta = 1;
  return delta;
}

// validate contract percentage adjustment
int TeleinfoValidateContractAdjustment (int adjust)
{
  // check boundaries
  if ((adjust < TELEINFO_PERCENT_MIN) || (adjust > TELEINFO_PERCENT_MAX)) adjust = 100;
  return adjust;
}

/**************************************************\
 *                  Commands
\**************************************************/

void CmndTeleinfoRate (void)
{
  teleinfo_meter.baud_rate = TeleinfoValidateBaudRate ((uint16_t)XdrvMailbox.payload);
  ResponseCmndNumber (teleinfo_meter.baud_rate);
}

void CmndTeleinfoTIC (void)
{
  teleinfo_meter.policy_tic = TeleinfoValidatePolicy (XdrvMailbox.payload);
  ResponseCmndNumber (teleinfo_meter.policy_tic);
}

void CmndTeleinfoData (void)
{
  teleinfo_meter.policy_data = TeleinfoValidatePolicy (XdrvMailbox.payload);
  ResponseCmndNumber (teleinfo_meter.policy_data);
}

void CmndTeleinfoInterval (void)
{
  teleinfo_meter.update_interval = TeleinfoValidateUpdateInterval (XdrvMailbox.payload);
  ResponseCmndNumber (teleinfo_meter.update_interval);
}

void CmndTeleinfoDelta (void)
{
  teleinfo_meter.power_delta = TeleinfoValidatePowerDelta (XdrvMailbox.payload);
  ResponseCmndNumber (teleinfo_meter.power_delta);
}

void CmndTeleinfoAdjust (void)
{
  teleinfo_contract.percent_adjust = TeleinfoValidateContractAdjustment (XdrvMailbox.payload);
  ResponseCmndNumber (teleinfo_contract.percent_adjust);
}

void CmndTeleinfoEnable (void)
{
  bool result = false;

  // switch ON command
  if ((strcasecmp (XdrvMailbox.data, "on") == 0) || (XdrvMailbox.payload == 1)) result = TeleinfoStartSerial ();

  // switch OFF command
  else if ((strcasecmp (XdrvMailbox.data, "off") == 0) || (XdrvMailbox.payload == 0)) result = TeleinfoStopSerial ();

  // send response
  if (result) ResponseCmndDone ();
  else ResponseCmndFailed ();
}

void CmndTeleinfoTCP (void)
{
  int     index;
  int16_t port;

  // stop previous server if running
  if (teleinfo_tcp.server)
  {
    // kill server
    teleinfo_tcp.server->stop ();
    delete teleinfo_tcp.server;
    teleinfo_tcp.server = nullptr;

    // stop all clients
    for (index = 0; index < TELEINFO_TCP_CLIENTS; index++) teleinfo_tcp.client[index].stop ();

    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Stopping TCP server"));
  }

  // if port is provided, start server
  port = XdrvMailbox.payload;
  if (port > 0)
  {
    // create and start server
    teleinfo_tcp.server = new WiFiServer (port);
    teleinfo_tcp.server->begin ();
    teleinfo_tcp.server->setNoDelay (true);

    // reset buffer index
    teleinfo_tcp.buffer_index = 0;

    // log
    AddLog (LOG_LEVEL_INFO, PSTR("TIC: Starting TCP server on port %d"), port);
  }

  // answer command
  ResponseCmndNumber (port);
}

/*********************************************\
 *               Functions
\*********************************************/

bool TeleinfoStartSerial ()
{
  bool is_created, is_possible;

  // check if serial port already created and if creation is possible
  is_created  = (teleinfo_serial != nullptr);
  is_possible = (TasmotaGlobal.energy_driver == XNRG_15) && PinUsed (GPIO_TELEINFO_RX) && (teleinfo_meter.baud_rate > 0) && (TasmotaGlobal.uptime > TELEINFO_INIT_DELAY);

  // if serial port creation is possible and not alredy done
  if (!is_created && is_possible)
  { 
 
#ifdef ESP32
    // create serial port
    teleinfo_serial = new TasmotaSerial (Pin (GPIO_TELEINFO_RX), -1, 1);

    // initialise serial port
    is_created = teleinfo_serial->begin (teleinfo_meter.baud_rate, SERIAL_7E1);

#else       // ESP8266
    // create serial port
    teleinfo_serial = new TasmotaSerial (Pin (GPIO_TELEINFO_RX), -1, 1);

    // initialise serial port
    is_created = teleinfo_serial->begin (teleinfo_meter.baud_rate, SERIAL_7E1);

    // force configuration on ESP8266
    if (teleinfo_serial->hardwareSerial ()) ClaimSerial ();

#endif      // ESP32 & ESP8266

    // declare serial port enabled
    if (is_created)
    {
      teleinfo_meter.status_rx = TIC_SERIAL_ACTIVE; 
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Serial set to 7E1 %d bauds"), teleinfo_meter.baud_rate);
    } 

    // else declare serial port initialisation failure
    else
    {
      teleinfo_meter.status_rx = TIC_SERIAL_FAILED; 
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Serial port init failed"));
    } 
  }

  // if needed, enable Teleinfo RX
  if (is_created && (teleinfo_meter.enable_rx != UINT8_MAX))
  {
    // set output high
    digitalWrite(teleinfo_meter.enable_rx, HIGH);
    AddLog (LOG_LEVEL_INFO, PSTR("TIC: Teleinfo Rx enabled by GPIO%d"), teleinfo_meter.enable_rx);
  }

  return is_created;
}

bool TeleinfoStopSerial ()
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

// split line into etiquette and donnee
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

// function to display any value to a string with unit and kilo conversion with number of digits (12000, 12000 VA, 12.0 kVA, 12.00 kVA, ...)
void TeleinfoDisplayValue (int unit_type, long value, char* pstr_result, int max_size, int kilo_digit = -1) 
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
      if (value < 10) strcpy (str_text, "000");
      else if (value < 100) strcpy (str_text, "00");
      else if (value < 1000) strcpy (str_text, "0");

      // append value
      strlcat (str_text, pstr_result, sizeof (str_text));

      // generate kilo format
      length = strlen (str_text) - 3;
      strcpy (pstr_result, "");
      strncat (pstr_result, str_text, length);
      if (kilo_digit > 0)
      {
        strcat (pstr_result, ".");
        strncat (pstr_result, str_text + length, kilo_digit);
      }
    }

    // append unit if specified
    if (unit_type < TELEINFO_UNIT_MAX) 
    {
      // add space and k unit, else end of string
      strlcat (pstr_result, " ", max_size);
      if ((kilo_digit > 0) && (unit_type != TELEINFO_UNIT_COSPHI)) strlcat (pstr_result, "k", max_size);

      // append unit label
      GetTextIndexed (str_text, sizeof (str_text), unit_type, kTeleinfoGraphDisplay);
      strlcat (pstr_result, str_text, max_size);
    }
  }
}

// function to display current counter values to a string with unit and kilo conversion
void TeleinfoDisplayCurrentValue (int unit_type, int phase, char* pstr_result, int max_size, int kilo_digit = -1) 
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
      value = teleinfo_phase[phase].cosphi * 10;
      break;
  }

  // convert value for display
  TeleinfoDisplayValue (unit_type, value, pstr_result, max_size, kilo_digit);
}

// Full message received - Update power calculation
void TeleinfoUpdateFromGlobalCounter (long counter_delta) 
{
  uint8_t phase;
  long    power_total, power_delta;

  // calculate apparent power ratio between phases
  power_total = 0;
  for (phase = 0; phase < teleinfo_contract.phase; phase++) power_total += teleinfo_phase[phase].papp;

  // if total power is not O
  if (power_total > 0)
  {
    // loop to prepare active power calculation structure
    for (phase = 0; phase < teleinfo_contract.phase; phase++)
    {
      // if value not initilised (case of a new measure period), set it to 0
      if (teleinfo_pact[phase].value == LONG_MAX) teleinfo_pact[phase].value = 0;

      // calculate phase power increase (with x1000 ratio to increase precision)
      power_delta = 1000 * teleinfo_phase[phase].papp / power_total * counter_delta;

      // prepare active power calculation structure
      teleinfo_pact[phase].ratio = 1000;
      teleinfo_pact[phase].init_value = 0;
      teleinfo_pact[phase].value += power_delta;
    }

    // loop to update active power
    for (phase = 0; phase < teleinfo_contract.phase; phase++) TeleinfoUpdatePower (TIC_POWER_UPDATE_PACT, phase);
  }
}

// Full message received - Update power calculation
void TeleinfoUpdatePower (int power_type, int phase) 
{
  long     power, power_increase, cosphi, cosphi_target;
  uint32_t time_delta = 0;
  struct   tic_calc_power* pcalc_power;
  char     str_unit[8];

  // set working structure
  switch (power_type)
  {
    case TIC_POWER_UPDATE_PAPP:
      pcalc_power = &teleinfo_papp[phase];
      break;
    case TIC_POWER_UPDATE_PACT:
      pcalc_power = &teleinfo_pact[phase];
      break;
    default:
      return;
  }

  // if dealing with active power, update cos phi calculation comparison value
  if (power_type == TIC_POWER_UPDATE_PACT)
  {
    // calculate default active power with current cos phi
    teleinfo_phase[phase].pact = teleinfo_phase[phase].papp * teleinfo_phase[phase].cosphi / 100;

    // if not first call, calculate power increase since last call
    if (pcalc_power->comp_time != UINT32_MAX) 
    {
      // calculate time elapsed since previous call
      time_delta = TimeDifference (pcalc_power->comp_time, teleinfo_message.timestamp);

      // calculate power increase (keeping x1000 ratio)
      power_increase = teleinfo_phase[phase].papp * time_delta / 3600;
      pcalc_power->comp_value += power_increase;
    }

    // set time stamp
    pcalc_power->comp_time = teleinfo_message.timestamp;
  }

  // if value has not been updated
  if (pcalc_power->value == LONG_MAX) return;

  // if initial value not set, set to current value
  if (pcalc_power->init_value == LONG_MAX) pcalc_power->init_value = pcalc_power->value;

  // if last measurement time is not set
  if (pcalc_power->init_time == UINT32_MAX)
  {
    // set comparison values and reset current value
    pcalc_power->init_time  = teleinfo_message.timestamp;
    pcalc_power->init_value = pcalc_power->value;
    pcalc_power->ratio = 1;
    pcalc_power->value = LONG_MAX;
    return;
  }

  // calculate power increase since last measure
  if (pcalc_power->ratio > 0) power_increase = (pcalc_power->value - pcalc_power->init_value) / pcalc_power->ratio;
  else power_increase = 0;

  // if increase is negative
  if (power_increase < 0)
  {
    // set last time and value, reset current value, but do not change last power increase
    pcalc_power->init_time  = teleinfo_message.timestamp;
    pcalc_power->init_value = pcalc_power->value;
    pcalc_power->ratio = 1;
    pcalc_power->value = LONG_MAX;

    // log rollback
    GetTextIndexed (str_unit, sizeof (str_unit), pcalc_power->type, kTeleinfoGraphDisplay);
    AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Counter rollback for %s"), str_unit);

    return;
  }

  // if power delta is not big enough to allow calculation, return
  if (power_increase < teleinfo_meter.power_delta) return;

  // calculate time elapsed since last measure (ms), return if null
  time_delta = TimeDifference (pcalc_power->init_time, teleinfo_message.timestamp);
  if (time_delta == 0) return;

  // calculate instant power over the period
  if (time_delta > 0) power = 3600000 / (long)time_delta * power_increase;
  else power = 0;

  // update power
  switch (power_type)
  {
    // apparent power
    case TIC_POWER_UPDATE_PAPP:

      // average apparent power : 50% of current value + 50% of newly calculated value
      teleinfo_phase[phase].papp = (teleinfo_phase[phase].papp / 2) + (power / 2);
      break;

    // active power
    case TIC_POWER_UPDATE_PACT:

      // if instant power is too small, cos phi calculation not possible
      if (power < TELEINFO_POWER_COSPHI) AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Phase %d, %dW too small for cos phi"), phase, power);

      // if cos phi calculation is possible : some apparent power increase and instant apparent power > 100W
      else if (pcalc_power->comp_value > 0)
      {
        // calculate cos phi
        cosphi_target = 100 * 1000 * power_increase / pcalc_power->comp_value;

        // discard wrong values
        if (cosphi > 100) cosphi = 100;
        else if (cosphi <= 0) cosphi = teleinfo_phase[phase].cosphi;

        // smoothing algo to avoid cos phi spikes
        cosphi = (teleinfo_phase[phase].cosphi * 75 + cosphi_target * 25) / 100;

        // update new cos phi and calculate new active power
        teleinfo_phase[phase].cosphi = cosphi;
        teleinfo_phase[phase].pact   = teleinfo_phase[phase].papp * cosphi / 100;
      }
      break;
  }

  // reset calculation values
  pcalc_power->init_time  = teleinfo_message.timestamp;
  pcalc_power->init_value = pcalc_power->value;
  pcalc_power->value      = LONG_MAX;
  pcalc_power->ratio      = 1;
  pcalc_power->comp_value = 0;
}

/*********************************************\
 *               Historisation
\*********************************************/

#ifdef USE_UFILESYS

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
  if (period == TELEINFO_PERIOD_DAY) sprintf (pstr_text, "%02u/%02u", start_dst.day_of_month, start_dst.month);

  // generate time label for day graph
  else if (period == TELEINFO_PERIOD_WEEK)
  {
    // end date
    calc_time += 518400;
    BreakTime (calc_time, stop_dst);

    if (start_dst.month == stop_dst.month) sprintf (pstr_text, "%02u-%02u/%02u", start_dst.day_of_month, stop_dst.day_of_month, stop_dst.month);
    else sprintf (pstr_text, "%02u/%02u-%02u/%02u", start_dst.day_of_month, start_dst.month, stop_dst.day_of_month, stop_dst.month);
  }

  return (strlen (pstr_text) > 0);
}

bool TeleinfoHistoGetFilename (int period, int histo, char* pstr_filename)
{
  bool file_exist = false;

  // set target is weekly records
  switch (period)
  {
    case TELEINFO_PERIOD_DAY:
      if (histo == 0) strcpy (pstr_filename, D_TELEINFO_HISTO_FILE_TODAY);
      else sprintf (pstr_filename, D_TELEINFO_HISTO_FILE_PASTDAY, histo);
      break;
    case TELEINFO_PERIOD_WEEK:
      if (histo == 0) strcpy (pstr_filename, D_TELEINFO_HISTO_FILE_WEEK);
      else sprintf (pstr_filename, D_TELEINFO_HISTO_FILE_PASTWEEK, histo);
      break;
    default:
      strcpy (pstr_filename, "");
      break;
  }

  // check if file exists
  file_exist = LITTLEFS.exists (pstr_filename);

  return file_exist;
}

int TeleinfoHistoNextLine ()
{
  bool  read_next;
  int   index;
  char* pstr_token;
  uint32_t pos_start, pos_delta;


  // read next line
  pos_start = teleinfo_histo.file.position ();
  teleinfo_histo.file.readBytes (teleinfo_histo.str_line, sizeof (teleinfo_histo.str_line) - 1);

  // align string and file position on end of line
  pstr_token = strchr (teleinfo_histo.str_line, '\n');
  if (pstr_token != nullptr)
  {
    pos_delta = pstr_token - teleinfo_histo.str_line;
    teleinfo_histo.file.seek (pos_start + pos_delta + 1);
    pstr_token = 0;
  } 

  // loop to populate array of values
  index = 0;
  pstr_token = strtok (teleinfo_histo.str_line, ";");
  while (pstr_token != nullptr)
  {
    if (index < TELEINFO_HISTO_COLUMN_MAX) teleinfo_histo.pstr_value[index++] = pstr_token;
    pstr_token = strtok (nullptr, ";");
  }
  teleinfo_histo.nb_value = index;

  // get line index
  if (teleinfo_histo.nb_value > 0) teleinfo_histo.idx_line = atoi (teleinfo_histo.pstr_value[0]);
  else teleinfo_histo.idx_line = INT_MAX;

  return teleinfo_histo.idx_line;
}

void TeleinfoHistoClose ()
{
  if (teleinfo_histo.opened)
  {
    // close current file
    teleinfo_histo.file.close ();

    // init file caracteristics
    teleinfo_histo.opened = false;
  }
}

bool TeleinfoHistoOpen (int period, int histo)
{
  char str_filename[16];

  // close file if already opened
  teleinfo_histo.file.close ();

  // init file caracteristics
  teleinfo_histo.opened   = false;
  teleinfo_histo.nb_value = 0;

  // open file in read mode
  teleinfo_histo.opened = TeleinfoHistoGetFilename (period, histo, str_filename);
  if (teleinfo_histo.opened) teleinfo_histo.file = LITTLEFS.open (str_filename, "r");

  // if opened, read first data line, skipping header
  if (teleinfo_histo.opened)
  {
    TeleinfoHistoNextLine ();
    TeleinfoHistoNextLine ();
  } 

  return teleinfo_histo.opened;
}

long TeleinfoHistoGetData (int data, int period, int phase, int index, int histo)
{
  int  row_index;
  long value = LONG_MAX;

  // if file is opened
  if (teleinfo_histo.opened)
  {
    // if index is 0, restart from beginning
    if (index == 0)
    {
      // set to start of file
      teleinfo_histo.file.seek (0);

     // read first data line, skipping header
      TeleinfoHistoNextLine ();
      TeleinfoHistoNextLine ();
    }

    // if line index smaller than given index, read next line dans get its index
    if (teleinfo_histo.idx_line < index) TeleinfoHistoNextLine ();

    // if line index is matching, get the value
    if (teleinfo_histo.idx_line == index)
    {
      // calculate column index to read and extract data
      row_index = 3 + (4 * phase) + data;
      if (row_index < teleinfo_histo.nb_value) value = atol (teleinfo_histo.pstr_value[row_index]);
    }
  }

  // if value undefined, set set out or display
  if (value == LONG_MAX)
  {
    if ((data == TELEINFO_UNIT_VA) || (data == TELEINFO_UNIT_W)) value = -100;
    else if (data == TELEINFO_UNIT_COSPHI) value = 110;
  }

  return value;
}

void TeleinfoHistoSaveData (int period)
{
  bool file_exist;
  int  phase;
  long index;
  uint32_t now_time, timestamp;
  TIME_T   now_dst;
  char str_original[16];
  char str_target[16];
  char str_value[32];
  char str_line[TELEINFO_HISTO_LINE_LENGTH];
  File file_histo;

  // calculate current timestamp
  now_time = LocalTime ();
  BreakTime (now_time, now_dst);
  timestamp = now_dst.hour * 100 + now_dst.minute;

  // -------------------
  //  Shift histo files
  // -------------------

  // if timestamp is smaller than previous one, new day is coming
  if (timestamp < teleinfo_histo.timestamp)
  {
    // rotate previous daily files
    for (index = TELEINFO_HISTO_DAY_MAX; index > 1; index--)
    {
      sprintf (str_original, D_TELEINFO_HISTO_FILE_PASTDAY, index - 1);
      sprintf (str_target, D_TELEINFO_HISTO_FILE_PASTDAY, index);
      if (LITTLEFS.exists (str_original)) LITTLEFS.rename (str_original, str_target);
    }

    // rename today's file
    sprintf (str_target, D_TELEINFO_HISTO_FILE_PASTDAY, 1);
    if (LITTLEFS.exists (D_TELEINFO_HISTO_FILE_TODAY)) LITTLEFS.rename (D_TELEINFO_HISTO_FILE_TODAY, str_target);

    // if we are monday, week has changed
    if (now_dst.day_of_week == 2)
    {
      // rotate previous weekly files
      for (index = TELEINFO_HISTO_WEEK_MAX; index > 1; index--)
      {
        sprintf (str_original, D_TELEINFO_HISTO_FILE_PASTWEEK, index - 1);
        sprintf (str_target, D_TELEINFO_HISTO_FILE_PASTWEEK, index);
        if (LITTLEFS.exists (str_original)) LITTLEFS.rename (str_original, str_target);
      }

      // rename this weeks's file
      sprintf (str_target, D_TELEINFO_HISTO_FILE_PASTWEEK, 1);
      if (LITTLEFS.exists (D_TELEINFO_HISTO_FILE_WEEK)) LITTLEFS.rename (D_TELEINFO_HISTO_FILE_WEEK, str_target);
    }
  }

  // update timestamp
  teleinfo_histo.timestamp = timestamp;

  // -------------------------
  //  Save data to histo file
  // -------------------------

  // set target is weekly records
  if (period == TELEINFO_PERIOD_WEEK)
  {
    strcpy (str_target, D_TELEINFO_HISTO_FILE_WEEK);
    index = (((now_dst.day_of_week + 5) % 7) * 86400 + now_dst.hour * 3600 + now_dst.minute * 60) / ARR_TELEINFO_PERIOD_SAMPLE[period];
  }

  // else target is daily records
  else if (period == TELEINFO_PERIOD_DAY)
  {
    strcpy (str_target, D_TELEINFO_HISTO_FILE_TODAY);
    index = (now_dst.hour * 3600 + now_dst.minute * 60) / ARR_TELEINFO_PERIOD_SAMPLE[period];
  }

  else return;

  // check if today's file already exists and open in append mode
  file_exist = LITTLEFS.exists (str_target);
  file_histo = LITTLEFS.open (str_target, "a");

  // if file doens't exists, add header
  if (!file_exist)
  {
    // create header
    strcpy (str_line, "Idx;Date;Time");
    for (phase = 0; phase < ENERGY_MAX_PHASES; phase++)
    {
      sprintf (str_value, ";VA%d;W%d;V%d;C%d", phase + 1, phase + 1, phase + 1, phase + 1);
      strlcat (str_line, str_value, sizeof (str_line));
    }
    strlcat (str_line, "\n", sizeof (str_line));

    // write header
    file_histo.print (str_line);
  }

  // generate current line
  sprintf (str_line, "%d;%02u/%02u;%02u:%02u", index, now_dst.day_of_month, now_dst.month, now_dst.hour, now_dst.minute);
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    sprintf (str_value, ";%d;%d;%d;%d", teleinfo_graph.papp[phase], teleinfo_graph.pact[phase], teleinfo_graph.volt[phase], teleinfo_graph.cosphi[phase]);
    strlcat (str_line, str_value, sizeof (str_line));
  }
  strlcat (str_line, "\n", sizeof (str_line));

  // append current line and close file
  file_histo.print (str_line);
  file_histo.close ();
}

#endif    // USE_UFILESYS

/*********************************************\
 *                   Graph
\*********************************************/

// save current values to graph
void TeleinfoGraphSaveData (int period)
{
  bool histo_update;
  int  index, phase;
  long power, voltage, value;

  // historic will be updated if period is beyond dynamic graph period
  histo_update = (period >= TELEINFO_GRAPH_PERIOD_MAX);

  // set indexed graph values with current values
  index = teleinfo_period[period].index;
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    //   Apparent power
    // -----------------
    power = LONG_MAX;
    value = UINT8_MAX;

    // if power contract is defined
    if (teleinfo_contract.ssousc > 0)
    {
      // calculate average value over the period
      power = (long)(teleinfo_period[period].papp_sum[phase] / ARR_TELEINFO_PERIOD_SAMPLE[period]);

      // calculate percentage of power according to contract (200 = 100% of contract power)
      value = power * 200 / teleinfo_contract.ssousc;
    }

    // save graph current value
    teleinfo_graph.papp[phase] = power;

    // if needed, save dynmic memory graph data
    if (period < TELEINFO_GRAPH_PERIOD_MAX) teleinfo_graph.data[period].arr_papp[phase][index] = (uint8_t)value;

    // if power not defined, no historic graph saving 
    histo_update &= (power != LONG_MAX);

    // reset period data
    teleinfo_period[period].papp_high[phase] = 0;
    teleinfo_period[period].papp_sum[phase]  = 0;

    //   Active power
    // -----------------
    power = LONG_MAX;
    value = UINT8_MAX;

    // check power contract validity
    if (teleinfo_contract.ssousc > 0)
    {
      // calculate average value over the period
      power = (long)(teleinfo_period[period].pact_sum[phase] / ARR_TELEINFO_PERIOD_SAMPLE[period]);
      if (power > teleinfo_graph.papp[phase]) power = teleinfo_graph.papp[phase];

      // calculate percentage of power according to contract (200 = 100% of contract power)
      value = power * 200 / teleinfo_contract.ssousc;
    }

    // save graph current value
    teleinfo_graph.pact[phase] = power;

    // if needed, save dynmic memory graph data
    if (period < TELEINFO_GRAPH_PERIOD_MAX) teleinfo_graph.data[period].arr_pact[phase][index] = (uint8_t)value;

    // if power not defined, no historic graph saving 
    histo_update &= (power != LONG_MAX);

    // reset period data
    teleinfo_period[period].pact_high[phase] = 0;
    teleinfo_period[period].pact_sum[phase]  = 0;

    //   Voltage
    // -----------

    // if maximum voltage is above maximum acceptable value, store it
    if (teleinfo_period[period].volt_high[phase] > TELEINFO_VOLTAGE_HIGH) voltage = teleinfo_period[period].volt_high[phase];

    // else, if minimum voltage is below acceptable value, store it
    else if (teleinfo_period[period].volt_low[phase] < TELEINFO_VOLTAGE_LOW) voltage = teleinfo_period[period].volt_low[phase];

    // else, store average value between min and max
    else voltage = teleinfo_period[period].volt_sum[phase] / ARR_TELEINFO_PERIOD_SAMPLE[period];

    // calculate value to be stored
    value = 128 + voltage - TELEINFO_VOLTAGE;

    // save graph current value
    teleinfo_graph.volt[phase] = voltage;

    // if needed, save dynmic memory graph data
    if (period < TELEINFO_GRAPH_PERIOD_MAX) teleinfo_graph.data[period].arr_volt[phase][index] = (uint8_t)value;

    // reset period data
    teleinfo_period[period].volt_low[phase]  = TELEINFO_VOLTAGE;
    teleinfo_period[period].volt_high[phase] = TELEINFO_VOLTAGE;
    teleinfo_period[period].volt_sum[phase]  = 0;

    //   CosPhi
    // -----------

    // calculate average value over the period
    value = (long)(teleinfo_period[period].cosphi_sum[phase] / ARR_TELEINFO_PERIOD_SAMPLE[period]);

    // save graph current value
    teleinfo_graph.cosphi[phase] = (int)value;

    // if needed, save dynmic memory graph data
    if (period < TELEINFO_GRAPH_PERIOD_MAX) teleinfo_graph.data[period].arr_cosphi[phase][index] = (uint8_t)value;

    // reset period data
    teleinfo_period[period].cosphi_sum[phase] = 0;
  }

  // increase data index in the graph and set update flag
  teleinfo_period[period].index++;
  teleinfo_period[period].index = teleinfo_period[period].index % TELEINFO_GRAPH_SAMPLE;
  teleinfo_period[period].updated = true;

#ifdef USE_UFILESYS
    // if needed, save data to history file
    if (teleinfo_histo.mounted && histo_update) TeleinfoHistoSaveData (period);
#endif    // USE_UFILESYS
}

// get a specific graph data value
long TeleinfoGetGraphData (int data, int period, int phase, int index, int histo)
{
  int  index_array;
  long value = LONG_MAX;

  // if data is stored in memory
  if (period < TELEINFO_GRAPH_PERIOD_MAX)
  {
    // get current array index if in live memory mode
    index_array = (index + teleinfo_period[period].index) % TELEINFO_GRAPH_SAMPLE;

    // get memory data according to data type
    switch (data)
    {
      case TELEINFO_UNIT_VA:
        // if value is defined, calculate percentage of power according to contract
        if (teleinfo_graph.data[period].arr_papp[phase][index] != UINT8_MAX)
          value = (long)teleinfo_graph.data[period].arr_papp[phase][index] * teleinfo_contract.ssousc / 200;
        break;

      case TELEINFO_UNIT_W:
        // if value is defined, calculate percentage of power according to contract
        if (teleinfo_graph.data[period].arr_pact[phase][index] != UINT8_MAX)
          value = (long)teleinfo_graph.data[period].arr_pact[phase][index] * teleinfo_contract.ssousc / 200;
        break;

      case TELEINFO_UNIT_V:
        // if voltage has been stored
        if (teleinfo_graph.data[period].arr_volt[phase][index] != UINT8_MAX)
          value = TELEINFO_VOLTAGE - 128 + teleinfo_graph.data[period].arr_volt[phase][index];
        break;

      case TELEINFO_UNIT_COSPHI:
        // if voltage has been stored
        if (teleinfo_graph.data[period].arr_cosphi[phase][index] != UINT8_MAX) value = (long)teleinfo_graph.data[period].arr_cosphi[phase][index];
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
  else value = TeleinfoHistoGetData (data, period, phase, index, histo);
#endif    // USE_UFILESYS

  return value;
}

/*********************************************\
 *                   Callback
\*********************************************/

void TeleinfoPreInit ()
{
  // if TeleinfoEN pin defined, switch it OFF
  if (PinUsed (GPIO_TELEINFO_ENABLE))
  {
    // switch it off to avoid ESP32 Rx heavy load restart bug
    teleinfo_meter.enable_rx = Pin (GPIO_TELEINFO_ENABLE);
    pinMode (teleinfo_meter.enable_rx, OUTPUT);

    // disable serial input
    TeleinfoStopSerial ();
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

void TeleinfoInit ()
{
  int index, phase;

  // init hardware energy counters
  Settings->flag3.hardware_energy_total = true;
  Settings->energy_kWhtotal = 0;

  // get saved settings
  teleinfo_meter.baud_rate = TeleinfoValidateBaudRate (Settings->sbaudrate * 300);
  teleinfo_meter.policy_tic = TeleinfoValidatePolicy ((int)Settings->weight_reference);
  teleinfo_meter.policy_data = TeleinfoValidatePolicy ((int)Settings->weight_calibration);
  teleinfo_meter.power_delta = TeleinfoValidatePowerDelta ((int)Settings->weight_max);
  teleinfo_meter.update_interval = TeleinfoValidateUpdateInterval ((int)Settings->weight_item);
  teleinfo_contract.percent_adjust = TeleinfoValidateContractAdjustment ((int)Settings->weight_change);

  // set default energy parameters
  Energy.voltage_available = true;
  Energy.current_available = true;

  // initialise message timestamp
  teleinfo_message.timestamp = UINT32_MAX;

  // loop thru all possible phases
  for (phase = 0; phase < ENERGY_MAX_PHASES; phase++)
  {
    // tasmota energy counters
    Energy.voltage[phase]        = TELEINFO_VOLTAGE;
    Energy.current[phase]        = 0;
    Energy.active_power[phase]   = 0;
    Energy.apparent_power[phase] = 0;

    // initialise phase data
    teleinfo_phase[phase].volt_set  = false;
    teleinfo_phase[phase].voltage   = TELEINFO_VOLTAGE;
    teleinfo_phase[phase].current   = 0;
    teleinfo_phase[phase].papp      = 0;
    teleinfo_phase[phase].pact      = 0;
    teleinfo_phase[phase].papp_last = 0;
    teleinfo_phase[phase].cosphi    = 100;

    // initialise apparent power calculation structure
    teleinfo_papp[phase].type       = TELEINFO_UNIT_VA;
    teleinfo_papp[phase].ratio      = 1;
    teleinfo_papp[phase].value      = LONG_MAX;
    teleinfo_papp[phase].init_value = LONG_MAX;
    teleinfo_papp[phase].init_time  = UINT32_MAX;
    teleinfo_papp[phase].comp_value = 0;
    teleinfo_papp[phase].comp_time  = UINT32_MAX;

    // initialise active power calculation structure
    teleinfo_pact[phase].type       = TELEINFO_UNIT_W;
    teleinfo_pact[phase].ratio      = 1;
    teleinfo_pact[phase].value      = LONG_MAX;
    teleinfo_pact[phase].init_value = LONG_MAX;
    teleinfo_pact[phase].init_time  = UINT32_MAX;
    teleinfo_pact[phase].comp_value = 0;
    teleinfo_pact[phase].comp_time  = UINT32_MAX;

    // data of historisation files
    teleinfo_graph.papp[phase]   = 0;
    teleinfo_graph.pact[phase]   = 0;
    teleinfo_graph.volt[phase]   = TELEINFO_VOLTAGE;
    teleinfo_graph.cosphi[phase] = 100;
  }

  // init strings
  strcpy (teleinfo_contract.str_id, "");

  // init all total indexes
  for (index = 0; index < TELEINFO_INDEX_MAX; index ++) teleinfo_meter.index[index] = 0;

  // disable all message lines
  for (index = 0; index < TELEINFO_LINE_QTY; index ++) teleinfo_message.line[index].checksum = 0;

#ifdef USE_UFILESYS

  // initialise historic file environment
  teleinfo_histo.nb_value = 0;

  // mount filesystem with auto-format option
  teleinfo_histo.mounted = LITTLEFS.begin ();
  if (!teleinfo_histo.mounted)
  {
    // format partition
    LITTLEFS.format ();
    AddLog (LOG_LEVEL_INFO, PSTR ("TIC: LittleFS partition formatted"));

    // remount the partition
    teleinfo_histo.mounted = LITTLEFS.begin ();
  }
  
  // log result
  if (teleinfo_histo.mounted) AddLog (LOG_LEVEL_INFO, PSTR ("TIC: LittleFS partition mounted"));
  else AddLog (LOG_LEVEL_INFO, PSTR ("TIC: LittleFS could not be mounted"));

#endif  // USE_UFILESYS
}

void TeleinfoGraphInit ()
{
  int index, phase, period;

  // initialise period data
  for (period = 0; period < TELEINFO_PERIOD_MAX; period++)
  {
    // init counter
    teleinfo_period[period].index   = 0;
    teleinfo_period[period].counter = 0;

    // loop thru phase
    for (phase = 0; phase < ENERGY_MAX_PHASES; phase++)
    {
      // init max power per period
      teleinfo_period[period].volt_low[phase]   = TELEINFO_VOLTAGE;
      teleinfo_period[period].volt_high[phase]  = TELEINFO_VOLTAGE;
      teleinfo_period[period].volt_sum[phase]   = 0;
      teleinfo_period[period].pact_high[phase]  = 0;
      teleinfo_period[period].pact_sum[phase]   = 0;
      teleinfo_period[period].papp_high[phase]  = 0;
      teleinfo_period[period].papp_sum[phase]   = 0;
    }
  }

  // initialise graph data
  for (period = 0; period < TELEINFO_GRAPH_PERIOD_MAX; period++)
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

// function to save data before ESP restart
void TeleinfoSaveBeforeRestart ()
{
  // stop serail reception and disable Teleinfo Rx
  teleinfo_meter.status_rx = TIC_SERIAL_STOPPED;
  TeleinfoStopSerial ();

  // save serial port settings
  Settings->sbaudrate = teleinfo_meter.baud_rate / 300;

  // save teleinfo settings
  Settings->weight_reference = (ulong)teleinfo_meter.policy_tic;
  Settings->weight_calibration = (ulong)teleinfo_meter.policy_data;
  Settings->weight_item = (ulong)teleinfo_meter.update_interval;
  Settings->weight_max = (uint16_t)teleinfo_meter.power_delta;
  Settings->weight_change = (uint8_t) teleinfo_contract.percent_adjust;

#ifdef USE_UFILESYS

  // unmout littlefs filesystem
  if (teleinfo_histo.mounted)
  {
    LITTLEFS.end ();
    teleinfo_histo.mounted = false;
    AddLog (LOG_LEVEL_INFO, PSTR ("TIC: LittleFS partition unmounted"));
  }

#endif  // USE_UFILESYS

  // update energy total
  EnergyUpdateTotal ((float)teleinfo_meter.total_wh, false);
}

// function to check for TCP clients
void TeleinfoCheckTCP ()
{
  int index;

  // check for a new client connection
  if (teleinfo_tcp.server && teleinfo_tcp.server->hasClient ())
  {
    // find an empty slot
    for (index = 0; index < TELEINFO_TCP_CLIENTS; index++)
      if (!teleinfo_tcp.client[index]) { teleinfo_tcp.client[index] = teleinfo_tcp.server->available ();  break; }

    // if no empty slot, kill oldest one
    if (index >= TELEINFO_TCP_CLIENTS)
    {
      index = teleinfo_tcp.client_next++ % TELEINFO_TCP_CLIENTS;
      teleinfo_tcp.client[index].stop ();
      teleinfo_tcp.client[index] = teleinfo_tcp.server->available ();
    }
  }
}

// function to handle received teleinfo data
void TeleinfoReceiveData ()
{
  bool overload = false;
  bool tcp_send = false;
  bool is_valid;
  int  index, phase, period;
  long value;
  long total_current = 0;
  long total_papp = 0;
  long long counter, counter_wh, delta_wh;
  uint32_t  timeout, timestamp;
  uint32_t  time_measured, time_maximum;
  char* pstr_match;
  char  checksum;
  char  byte_recv;
  char  str_etiquette[TELEINFO_KEY_MAX];
  char  str_donnee[TELEINFO_DATA_MAX];
  char  str_text[TELEINFO_DATA_MAX];

  // set receive loop timeout
  timestamp = millis ();
  timeout   = timestamp + 10;

  // serial receive loop
  while (!TimeReached (timeout) && teleinfo_serial->available()) 
  {
    // read caracter
    byte_recv = (char)teleinfo_serial->read (); 
    teleinfo_message.length++;

    // if TCP server is active, append caracter to TCP buffer
    if (teleinfo_tcp.server)
    {
      // append caracter to buffer
      teleinfo_tcp.buffer[teleinfo_tcp.buffer_index++] = byte_recv;

      // if end of line or buffer size reached
      if ((teleinfo_tcp.buffer_index == sizeof (teleinfo_tcp.buffer)) || (byte_recv == 13))
      {
        // send data to connected clients
        for (index = 0; index < TELEINFO_TCP_CLIENTS; index++)
          if (teleinfo_tcp.client[index]) teleinfo_tcp.client[index].write (teleinfo_tcp.buffer, teleinfo_tcp.buffer_index);

        // reset buffer index
        teleinfo_tcp.buffer_index = 0;
      }
    } 

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
        if (teleinfo_line.buffer_index < sizeof (teleinfo_line.buffer) - 1) teleinfo_line.buffer[teleinfo_line.buffer_index++] = byte_recv;
        break;
        
      // ------------------
      // 0x0D : Line - End
      // ------------------
      case 0x0D:
        // increment counter
        teleinfo_meter.nb_data++;

        // drop first message
        if (teleinfo_meter.nb_data > 1)
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
              // Contract : reference
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

              // Contract : mode
              case TIC_CONFIG:
                if (strcmp (str_donnee, "CONSO") == 0) teleinfo_contract.mode = TIC_MODE_CONSO;
                else if (strcmp (str_donnee, "PROD") == 0) teleinfo_contract.mode = TIC_MODE_PROD;
                break;

              // period name
              case TIC_PTEC:
              case TIC_LTARF:
              case TIC_PTCOUR:
              case TIC_PTCOUR1:
                period = GetCommandCode (str_text, sizeof (str_text), str_donnee, kTeleinfoPeriod);
                if (period != -1) teleinfo_contract.period = period;
                break;

              // instant current
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
                if (teleinfo_meter.source_papp == TIC_POWER_UNDEFINED) teleinfo_meter.source_papp = TIC_POWER_PROVIDED;
                if (teleinfo_meter.source_papp == TIC_POWER_PROVIDED)
                {
                  value = atol (str_donnee);
                  is_valid = ((value >= 0) && (value < LONG_MAX));
                  if (is_valid) teleinfo_meter.papp = value;
                }
                break;

              case TIC_EAPPS:
                if (teleinfo_meter.source_papp == TIC_POWER_UNDEFINED) teleinfo_meter.source_papp = TIC_POWER_INCREMENT;
                if (teleinfo_meter.source_papp == TIC_POWER_INCREMENT)
                {
                  strlcpy (str_text, str_donnee, sizeof (str_text));
                  pstr_match = strchr (str_text, 'V');
                  if (pstr_match != nullptr) *pstr_match = 0;
                  value = atol (str_text);
                  is_valid = ((value >= 0) && (value < LONG_MAX));
                  if (is_valid) teleinfo_papp[0].value = value;
                }
                break;

              // Active Power Counter
              case TIC_EAS:
                strlcpy (str_text, str_donnee, sizeof (str_text));
                pstr_match = strchr (str_text, 'W');
                if (pstr_match != nullptr) *pstr_match = 0;
                value = atol (str_text);
                is_valid = (value < LONG_MAX);
                if (is_valid) teleinfo_pact[0].value = value;
                teleinfo_meter.source_pact = TIC_POWER_INCREMENT;
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

              // Contract : maximum current
              case TIC_ISOUSC:
                value = atol (str_donnee);
                is_valid = ((value > 0) && (value < LONG_MAX));
                if (is_valid) 
                {
                  teleinfo_contract.isousc = value;
                  teleinfo_contract.ssousc = teleinfo_contract.isousc * TELEINFO_VOLTAGE_REF;
                }
                break;

              // Contract : maximum power (kVA)
              case TIC_PREF:
              case TIC_PCOUP:
                value = atol (str_donnee);
                is_valid = ((value > 0) && (value < LONG_MAX) && (teleinfo_contract.phase > 0));
                if (is_valid)
                {
                  teleinfo_contract.ssousc = 1000 * value / teleinfo_contract.phase;
                  teleinfo_contract.isousc = teleinfo_contract.ssousc / TELEINFO_VOLTAGE_REF;
                }
                break;

              case TIC_PS:
                strlcpy (str_text, str_donnee, sizeof (str_text));
                pstr_match = strchr (str_text, 'k');
                if (pstr_match != nullptr) strcpy (pstr_match, "000");
                value = atol (str_text);
                is_valid = ((value > 0) && (value < LONG_MAX));
                if (is_valid)
                {
                  teleinfo_contract.ssousc = value;
                  teleinfo_contract.isousc = teleinfo_contract.ssousc / TELEINFO_VOLTAGE_REF;
                }
                break;

              // Contract : index suivant type de contrat
              case TIC_EAPS:
                strlcpy (str_text, str_donnee, sizeof (str_text));
                pstr_match = strchr (str_text, 'k');
                if (pstr_match != nullptr) strcpy (pstr_match, "000");
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
        // increment message counter
        teleinfo_meter.nb_message++;

        // reset message timing data
        teleinfo_message.timestamp = timestamp;
        
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
        if ((teleinfo_contract.nb_indexes == -1) && (teleinfo_contract.period != -1))
        {
          // update number of indexes
          teleinfo_contract.nb_indexes = ARR_TELEINFO_PERIOD_INDEX[teleinfo_contract.period];

          // log
          GetTextIndexed (str_text, sizeof (str_text), teleinfo_contract.period, kTeleinfoPeriod);
          AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Period %s detected, total power based on %d indexe(s)"), str_text, teleinfo_contract.nb_indexes);
        }

        // update total energy counter
        counter_wh = 0;
        if (teleinfo_contract.nb_indexes != -1)
          for (index = 0; index < teleinfo_contract.nb_indexes; index ++) counter_wh += teleinfo_meter.index[index];

        // if needed, adjust number of phases
        if (Energy.phase_count < teleinfo_contract.phase) Energy.phase_count = teleinfo_contract.phase;

        // calculate total current
        for (phase = 0; phase < teleinfo_contract.phase; phase++) total_current += teleinfo_phase[phase].current;

        // -----------------------
        //  update apparent power

        for (phase = 0; phase < teleinfo_contract.phase; phase++)
        {
          // swtich according to calculation method
          switch (teleinfo_meter.source_papp) 
          {
            // apparent power has been directly provided
            case TIC_POWER_PROVIDED:
              // if apparent power is given by phase, use direct value
              if (teleinfo_papp[phase].value != LONG_MAX) teleinfo_phase[phase].papp = teleinfo_papp[phase].value;

              // else, current is given, apparent power is split between phase according to current ratio
              else if (total_current > 0) teleinfo_phase[phase].papp = teleinfo_meter.papp * teleinfo_phase[phase].current / total_current;

              // else if current is 0, apparent power is equally split between phases
              else if (teleinfo_contract.phase > 0) teleinfo_phase[phase].papp = teleinfo_meter.papp / teleinfo_contract.phase;

              break;

            // apparent power is provided as individual counters par phase
            case TIC_POWER_INCREMENT:
              // calculate apparent power with average
              TeleinfoUpdatePower (TIC_POWER_UPDATE_PAPP, phase);
              break;
          }

          // reset current value
          teleinfo_papp[phase].value = LONG_MAX;
        }

        // ---------------------
        //  update active power

        // swtich according to calculation method
        switch (teleinfo_meter.source_pact) 
        {
          // active power is provided by global counters
          case TIC_POWER_GLOBAL_COUNTER:

            // first global counter update
            if (teleinfo_meter.total_wh == 0) teleinfo_meter.total_wh = counter_wh;

            // calculate global counter delta and update calculation if needed
            delta_wh = counter_wh - teleinfo_meter.total_wh;
            if (delta_wh > 0) 
            {
              // update active power from global counter increase
              TeleinfoUpdateFromGlobalCounter ((long)delta_wh);

              // update global counter
              teleinfo_meter.total_wh = counter_wh;
            }
            break;

          // active power is provided as individual counters par phase
          case TIC_POWER_INCREMENT:
            // loop to calculate active power per phase
            for (phase = 0; phase < teleinfo_contract.phase; phase++)
              if (teleinfo_phase[phase].papp != LONG_MAX) TeleinfoUpdatePower (TIC_POWER_UPDATE_PACT, phase);
            break;
        }

        // --------------
        //  update flags

        for (phase = 0; phase < teleinfo_contract.phase; phase++)
        {
          // detect apparent power overload
          if (teleinfo_phase[phase].papp > teleinfo_contract.ssousc) teleinfo_message.overload = true;

          // detect more than x % power change
          value = abs (teleinfo_phase[phase].papp_last - teleinfo_phase[phase].papp);
          if (value > (teleinfo_contract.ssousc * TELEINFO_PERCENT_CHANGE / 100)) teleinfo_message.percent = true;
          teleinfo_phase[phase].papp_last = teleinfo_phase[phase].papp;
        }

        // ------------------------
        //  update energy counters

        for (phase = 0; phase < teleinfo_contract.phase; phase++)
        {
          if (teleinfo_phase[phase].voltage > 0) Energy.voltage[phase] = (float)teleinfo_phase[phase].voltage;
          if (teleinfo_phase[phase].papp != LONG_MAX) Energy.apparent_power[phase] = (float)teleinfo_phase[phase].papp;
          if (teleinfo_phase[phase].pact != LONG_MAX) Energy.active_power[phase] = (float)teleinfo_phase[phase].pact;
          if (teleinfo_phase[phase].cosphi != INT_MAX) Energy.power_factor[phase] = (float)teleinfo_phase[phase].cosphi / 100;
          if ((teleinfo_phase[phase].voltage > 0) && (teleinfo_phase[phase].papp != LONG_MAX))
            Energy.current[phase] = (float)teleinfo_phase[phase].papp / (float)teleinfo_phase[phase].voltage;
        } 

        // update global active power counter
        Energy.total = (float)counter_wh / 1000;

        // declare received message
        teleinfo_message.received = true;
        break;

      // add other caracters to current line, removing all control caracters
      default:
          if (teleinfo_line.buffer_index < sizeof (teleinfo_line.buffer) - 1) teleinfo_line.buffer[teleinfo_line.buffer_index++] = byte_recv;
        break;
    }
  }
}

// if needed, update graph display values and check if data should be published
void TeleinfoEverySecond ()
{
  int  publish_policy;
  int  period, phase;

  // increment update interval counter
  teleinfo_meter.interval_count++;
  teleinfo_meter.interval_count = teleinfo_meter.interval_count % (60 * teleinfo_meter.update_interval);

  // if needed, update energy counters
  if (teleinfo_meter.interval_count == 0)
  {
    // update energy total
    EnergyUpdateTotal ((float)teleinfo_meter.total_wh, false);
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
        teleinfo_period[period].volt_sum[phase]   += teleinfo_phase[phase].voltage;
        teleinfo_period[period].cosphi_sum[phase] += teleinfo_phase[phase].cosphi;

        // update lowest voltage level
        if (teleinfo_phase[phase].voltage < teleinfo_period[period].volt_low[phase]) teleinfo_period[period].volt_low[phase] = teleinfo_phase[phase].voltage;

        // update highest voltage level         
        if (teleinfo_phase[phase].voltage > teleinfo_period[period].volt_high[phase]) teleinfo_period[period].volt_high[phase] = teleinfo_phase[phase].voltage;

        // update highest apparent power level
        if (teleinfo_phase[phase].papp > teleinfo_period[period].papp_high[phase]) teleinfo_period[period].papp_high[phase] = teleinfo_phase[phase].papp;

        // update highest active power level
        if (teleinfo_phase[phase].pact > teleinfo_period[period].pact_high[phase]) teleinfo_period[period].pact_high[phase] = teleinfo_phase[phase].pact;
      } 
    }

    // increment graph period counter and update graph data if needed
    if (teleinfo_period[period].counter == 0) TeleinfoGraphSaveData (period);
    teleinfo_period[period].counter ++;
    teleinfo_period[period].counter = teleinfo_period[period].counter % ARR_TELEINFO_PERIOD_SAMPLE[period];
  }

  // check if TIC should be published
  publish_policy = teleinfo_meter.policy_tic;
  if (teleinfo_message.overload && (publish_policy != TELEINFO_POLICY_NEVER))   teleinfo_message.send_tic = true;
  if (teleinfo_message.received && (publish_policy == TELEINFO_POLICY_MESSAGE)) teleinfo_message.send_tic = true;
  if (teleinfo_message.percent  && (publish_policy == TELEINFO_POLICY_PERCENT)) teleinfo_message.send_tic = true;
  
  // check if Meter data should be published
  publish_policy = teleinfo_meter.policy_data;
  if (teleinfo_message.overload && (publish_policy != TELEINFO_POLICY_NEVER))   teleinfo_message.send_meter = true;
  if (teleinfo_message.received && (publish_policy == TELEINFO_POLICY_MESSAGE)) teleinfo_message.send_meter = true;
  if (teleinfo_message.percent  && (publish_policy == TELEINFO_POLICY_PERCENT)) teleinfo_message.send_meter = true;

  // reset message flags
  teleinfo_message.overload = false;
  teleinfo_message.received = false;
  teleinfo_message.percent  = false;

  // if current or overload has been updated, publish teleinfo data
  if (teleinfo_message.send_meter) TeleinfoShowJSON (false, false, true);
  if (teleinfo_message.send_tic)   TeleinfoShowJSON (false, true, false);
}

// Generate JSON with TIC informations
//   "TIC":{ "ADCO":"1234567890","ISOUSC":30,"SSOUSC":6000,"SINSTS":2567,"SINSTS1":1243,"IINST1":14.7,"ADIR1":0,"SINSTS2":290,"IINST2":4.4, ... }
void TeleinfoGenerateTicJSON ()
{
  int index;

  // start of TIC section
  ResponseAppend_P (PSTR (",\"%s\":{"), TELEINFO_JSON_TIC);

  // loop thru TIC message lines
  for (index = 0; index < TELEINFO_LINE_QTY; index ++)
    if (teleinfo_message.line[index].checksum != 0)
    {
      // add current line
      if (index != 0) ResponseAppend_P (PSTR (","));
      ResponseAppend_P (PSTR ("\"%s\":\"%s\""), teleinfo_message.line[index].str_etiquette, teleinfo_message.line[index].str_donnee);
    }

  // end of TIC section
  ResponseAppend_P (PSTR ("}"));

  // TIC has been published
  teleinfo_message.send_tic = false;
}

// Generate JSON with Meter informations
//   "METER":{ "PHASE":3,"PREF":6000,"IREF":30,"U1":233,"I1":10,"P1":2020,"U2":231,"I2":5,"P1":990,"U3":230,"I3":2,"P3":410}
void TeleinfoGenerateMeterJSON ()
{
  int   phase, value;
  long  pmax;
  float current;
  char str_text[16];

  // start METER section
  ResponseAppend_P (PSTR (",\"%s\":{"), TELEINFO_JSON_METER);

  // METER basic data
  ResponseAppend_P (PSTR ("\"%s\":%d"),  TELEINFO_JSON_PHASE, teleinfo_contract.phase);
  ResponseAppend_P (PSTR (",\"%s\":%d"), TELEINFO_JSON_IREF, teleinfo_contract.isousc);
  ResponseAppend_P (PSTR (",\"%s\":%d"), TELEINFO_JSON_PREF, teleinfo_contract.ssousc);

  // METER adjusted maximum power
  pmax = teleinfo_contract.ssousc * (long)teleinfo_contract.percent_adjust / 100;
  ResponseAppend_P (PSTR (",\"%s\":%d"), TELEINFO_JSON_PMAX, pmax);

  // loop to update voltage, current and power
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    value = phase + 1;

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

  // end of Meter section
  ResponseAppend_P (PSTR ("}"));

  // Meter has been published
  teleinfo_message.send_meter = false;
}

// Show JSON status (for MQTT)
void TeleinfoShowJSON (bool append, bool allow_tic, bool allow_meter)
{
  // if teleperiod call,
  if (append)
  {
    // if telemetry call, check for JSON update according to update policy
    if (TELEINFO_POLICY_NEVER != teleinfo_meter.policy_tic) teleinfo_message.send_tic = true;
    if (TELEINFO_POLICY_NEVER != teleinfo_meter.policy_data) teleinfo_message.send_meter = true;
  }

  // if not in append mode, start with current time   {"Time":"xxxxxxxx"
  if (!append) Response_P (PSTR ("{\"%s\":\"%s\""), D_JSON_TIME, GetDateAndTime (DT_LOCAL).c_str ());

  // if TIC section should be published
  if (allow_tic && teleinfo_message.send_tic) TeleinfoGenerateTicJSON ();

  // if Meter section should be published
  if (allow_meter && teleinfo_message.send_meter) TeleinfoGenerateMeterJSON ();

  // generate MQTT message according to append mode
  if (!append)
  {
    ResponseAppend_P (PSTR ("}"));
    MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));
  } 
}

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

// display offload icons
void TeleinfoWebIconTic () { Webserver->send_P (200, PSTR ("image/png"), teleinfo_icon_tic_png, teleinfo_icon_tic_len); }

// get specific argument as a value with min and max
int TeleinfoWebGetArgValue (const char* pstr_argument, int value_default, int value_min, int value_max)
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
  if (arg_value < value_min) arg_value = value_min;
  if (arg_value > value_max) arg_value = value_max;

  return arg_value;
}

// append Teleinfo graph button to main page
void TeleinfoWebMainButton ()
{
  // Teleinfo message page button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_TELEINFO_PAGE_TIC, D_TELEINFO_MESSAGE);

  // Teleinfo graph page button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_TELEINFO_PAGE_GRAPH, D_TELEINFO_GRAPH);
}

// append Teleinfo configuration button to configuration page
void TeleinfoWebConfigButton ()
{
  // heater and meter configuration button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>"), D_TELEINFO_PAGE_CONFIG, D_TELEINFO_CONFIG);
}

// append Teleinfo state to main page
void TeleinfoWebSensor ()
{
  int  index, phase, period;
  long percentage;
  char str_text[32];
  char str_header[32];
  char str_data[96];


  switch (teleinfo_meter.status_rx)
  {
    case TIC_SERIAL_INIT:
      if (TasmotaGlobal.uptime < TELEINFO_INIT_DELAY) WSContentSend_P (PSTR ("{s}%s{m}Initialisation{e}"), D_TELEINFO);
      else WSContentSend_P (PSTR ("{s}%s{m}Check configuration{e}"), D_TELEINFO);
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
          WSContentSend_P (TELEINFO_HTML_BAR, ARR_TELEINFO_PHASE_COLOR[phase], percentage, percentage);
        }

      // Teleinfo mode
      strcpy (str_header, D_TELEINFO_MODE);
      GetTextIndexed (str_data, sizeof (str_data), teleinfo_contract.mode, kTeleinfoModeName);

      // Teleinfo contract power
      if (teleinfo_contract.ssousc > 0)
      {
        // header
        strlcat (str_header, "<br>", sizeof (str_header));
        strlcat (str_header, D_TELEINFO_CONTRACT, sizeof (str_header));

        // number of phases
        strcpy (str_text, "<br>");
        if (teleinfo_contract.phase > 1) sprintf (str_text, "<br>%d x ", teleinfo_contract.phase);
        strlcat (str_data, str_text, sizeof (str_data));

        // power
        sprintf (str_text, "%d kVA", teleinfo_contract.ssousc / 1000);
        strlcat (str_data, str_text, sizeof (str_data));
      }

      // Teleinfo period
      if (teleinfo_contract.period != -1)
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
      strcpy (str_header, D_TELEINFO_MESSAGE);
      sprintf (str_data, "%d", teleinfo_meter.nb_message);

      // calculate TIC checksum errors percentage
      if ((teleinfo_meter.nb_error > 0) && (teleinfo_meter.nb_data > 0))
      {
        // calculate error percentage
        percentage = 100 * teleinfo_meter.nb_error / teleinfo_meter.nb_data;

        // append data
        strcpy (str_text, "");
        if (percentage > 0) sprintf (str_text, " <small>(%d%%)</small>", percentage);
        strlcat (str_data, str_text, sizeof (str_data));
      }

      // get TIC reset
      if (teleinfo_meter.nb_reset > 0)
      {
        strlcat (str_header, "<br>", sizeof (str_header));
        strlcat (str_header, D_TELEINFO_RESET, sizeof (str_header));

        sprintf (str_text, "<br>%d", teleinfo_meter.nb_reset);
        strlcat (str_data, str_text, sizeof (str_data));
      }

      // display data
      WSContentSend_P (PSTR ("{s}%s{m}%s{e}"), str_header, str_data);
      break;
  }
}

// Teleinfo web page
void TeleinfoWebPageConfig ()
{
  int  index, value, rate_mode;
  uint16_t rate_baud;
  char str_text[16];
  char str_select[16];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess ()) return;

  // parameter 'tic' : set TIC messages diffusion policy
  WebGetArg (D_CMND_TELEINFO_TELE, str_text, sizeof (str_text));
  if (strlen (str_text) > 0) teleinfo_meter.policy_tic = TeleinfoValidatePolicy (atoi (str_text));

  // parameter 'data' : set data messages diffusion policy
  WebGetArg (D_CMND_TELEINFO_METER, str_text, sizeof (str_text));
  if (strlen (str_text) > 0) teleinfo_meter.policy_data = TeleinfoValidatePolicy (atoi (str_text));

  // parameter 'ival' : set energy publication interval
  WebGetArg (D_CMND_TELEINFO_INTERVAL, str_text, sizeof (str_text));
  if (strlen (str_text) > 0) teleinfo_meter.update_interval = TeleinfoValidateUpdateInterval (atoi (str_text));

  // parameter 'delta' : set energy power delta to start calculation (in VA or W)
  WebGetArg (D_CMND_TELEINFO_DELTA, str_text, sizeof (str_text));
  if (strlen (str_text) > 0) teleinfo_meter.power_delta = TeleinfoValidatePowerDelta (atoi (str_text));

  // parameter 'adjust' : set contract power limit adjustment in %
  WebGetArg (D_CMND_TELEINFO_ADJUST, str_text, sizeof (str_text));
  if (strlen (str_text) > 0) teleinfo_contract.percent_adjust = TeleinfoValidatePowerDelta (atoi (str_text));

  // get current and new teleinfo rate
  WebGetArg (D_CMND_TELEINFO_RATE, str_text, sizeof (str_text));
  if (strlen (str_text) > 0) teleinfo_meter.baud_rate = TeleinfoValidateBaudRate (atoi (str_text));

  // beginning of form
  WSContentStart_P (D_TELEINFO_CONFIG);
  WSContentSendStyle ();
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_TELEINFO_PAGE_CONFIG);

  // speed selection form
  WSContentSend_P (TELEINFO_FIELD_START, PSTR ("TIC Rate"));
  for (index = 0; index < nitems (ARR_TELEINFO_RATE); index++)
  {
    if (ARR_TELEINFO_RATE[index] == teleinfo_meter.baud_rate) strcpy (str_select, "checked"); else strcpy (str_select, "");
    itoa (ARR_TELEINFO_RATE[index], str_text, 10);
    WSContentSend_P (TELEINFO_INPUT_RADIO, D_CMND_TELEINFO_RATE, ARR_TELEINFO_RATE[index], str_select, str_text);
  }
  WSContentSend_P (TELEINFO_FIELD_STOP);

  // teleinfo energy calculation parameters
  WSContentSend_P (TELEINFO_FIELD_START, PSTR ("Energy calculation"));
  WSContentSend_P (TELEINFO_INPUT_NUMBER, PSTR ("Contract acceptable overload (%)"), D_CMND_TELEINFO_ADJUST, TELEINFO_PERCENT_MIN, TELEINFO_PERCENT_MAX, teleinfo_contract.percent_adjust);
  WSContentSend_P (TELEINFO_INPUT_NUMBER, PSTR ("Minimum power delta for calculation (VA/W)"), D_CMND_TELEINFO_DELTA, TELEINFO_DELTA_MIN, TELEINFO_DELTA_MAX, teleinfo_meter.power_delta);
  WSContentSend_P (TELEINFO_INPUT_NUMBER, PSTR ("Power saving interval to SRAM (mn)"), D_CMND_TELEINFO_INTERVAL, TELEINFO_INTERVAL_MIN, TELEINFO_INTERVAL_MAX, teleinfo_meter.update_interval);
  WSContentSend_P (TELEINFO_FIELD_STOP);

  // teleinfo meter tic diffusion selection
  WSContentSend_P (TELEINFO_FIELD_START, PSTR ("Send Teleinfo data"));
  for (index = 0; index < TELEINFO_POLICY_MAX; index++)
  {
    if (index == teleinfo_meter.policy_tic) strcpy (str_select, "checked"); else strcpy (str_select, "");
    WSContentSend_P (TELEINFO_INPUT_RADIO, D_CMND_TELEINFO_TELE, index, str_select, ARR_TELEINFO_CFG_LABEL[index]);
  }
  WSContentSend_P (TELEINFO_FIELD_STOP);

  // teleinfo meter data diffusion selection
  WSContentSend_P (TELEINFO_FIELD_START, PSTR ("Send Meter data"));
  for (index = 0; index < TELEINFO_POLICY_MAX; index++)
  {
    if (index == teleinfo_meter.policy_data) strcpy (str_select, "checked"); else strcpy (str_select, "");
    WSContentSend_P (TELEINFO_INPUT_RADIO, D_CMND_TELEINFO_METER, index, str_select, ARR_TELEINFO_CFG_LABEL[index]);
  }
  WSContentSend_P (TELEINFO_FIELD_STOP);

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
  WSContentSend_P (PSTR ("<tr><th>Etiquette</th><th class='value'>Valeur</th><th>Checksum</th></tr>\n"));

  // loop to display TIC messsage lines
  for (index = 0; index < teleinfo_message.line_max; index ++)
    WSContentSend_P (PSTR ("<tr id='l%d'><td id='e%d'>%s</td><td id='d%d'>%s</td><td id='c%d'>%c</td></tr>\n"), index + 1, index + 1, teleinfo_message.line[index].str_etiquette, index + 1, teleinfo_message.line[index].str_donnee, index + 1, teleinfo_message.line[index].checksum);

  // end of table and end of page
  WSContentSend_P (PSTR ("</table></div>\n"));
  WSContentSend_P (PSTR ("<div>\n"));
  WSContentStop ();
}

// get status update
void TeleinfoWebGraphUpdate ()
{
  int    phase, update;
  int    period, data, histo;
  char   str_text[16];

  // check graph parameters
  period = TeleinfoWebGetArgValue (D_CMND_TELEINFO_PERIOD, TELEINFO_PERIOD_LIVE,  0, TELEINFO_PERIOD_MAX - 1);
  data   = TeleinfoWebGetArgValue (D_CMND_TELEINFO_DATA,   TELEINFO_UNIT_VA, 0, TELEINFO_UNIT_MAX - 1);
  histo  = TeleinfoWebGetArgValue (D_CMND_TELEINFO_HISTO,  0, 0, TELEINFO_HISTO_DAY_MAX);

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

void TeleinfoWebGraphTime (int period, int histo)
{
  int    index;
  long   graph_left, graph_right, graph_width;  
  long   unit_width, shift_unit, shift_width;  
  long   graph_x;  
  uint32_t current_time;
  TIME_T   time_dst;

  // extract time data
  current_time = LocalTime ();
#ifdef USE_UFILESYS
  if (period >= TELEINFO_GRAPH_PERIOD_MAX) current_time = TeleinfoHistoGetStartTime (period, histo);
#endif    // USE_UFILESYS
  BreakTime (current_time, time_dst);

  // boundaries of SVG graph
  graph_left  = TELEINFO_GRAPH_PERCENT_START * TELEINFO_GRAPH_WIDTH / 100;
  graph_right = TELEINFO_GRAPH_PERCENT_STOP * TELEINFO_GRAPH_WIDTH / 100;
  graph_width = graph_right - graph_left;

  // handle graph units according to period
  switch (period) 
  {
    case TELEINFO_PERIOD_LIVE:
      // calculate horizontal shift
      unit_width  = graph_width / 6;
      shift_unit  = time_dst.minute % 5;
      shift_width = unit_width - (unit_width * shift_unit / 5) - (unit_width * time_dst.second / 300);

      // calculate first time displayed by substracting (5 * 5mn + shift) to current time
      current_time -= 1500;
      current_time -= (shift_unit * 60); 

      // display 5 mn separation lines with time
      for (index = 0; index < 6; index++)
      {
        // convert back to date and increase time of 5mn
        BreakTime (current_time, time_dst);
        current_time += 300;

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
        if ((index < 6) || (time_dst.hour >= 18)) WSContentSend_P (PSTR ("<text class='time' x='%d' y='%d%%'>%s</text>\n"), graph_x + 50, 53, arr_week_day[time_dst.day_of_week]);
      }
      break;
  }
}

void TeleinfoWebGraphCurve (int period, int data, int phase, int histo)
{
  bool is_valid;
  int  index;
  long value;
  long graph_left, graph_right, graph_width;  
  long graph_delta, graph_x, graph_y;
  char str_result[128];
  char str_point[12];

  // boundaries of SVG graph
  graph_left  = TELEINFO_GRAPH_PERCENT_START * TELEINFO_GRAPH_WIDTH / 100;
  graph_right = TELEINFO_GRAPH_PERCENT_STOP * TELEINFO_GRAPH_WIDTH / 100;
  graph_width = graph_right - graph_left;

  // start of polyline
  WSContentSend_P ("<polyline class='ph%d' points='", phase); 

  // loop for the graph display
  strcpy (str_result, "");
  for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
  {
    // set curve value according to displayed data
    is_valid = false;
    switch (data) 
    {
      case TELEINFO_UNIT_VA:
        // if power is defined, calculate graph y position
        value = TeleinfoGetGraphData (TELEINFO_UNIT_VA, period, phase, index, histo);
        is_valid = (teleinfo_period[period].pmax > 0);
        if (is_valid) graph_y = teleinfo_graph.height - (value * teleinfo_graph.height / teleinfo_period[period].pmax);
        break;

      case TELEINFO_UNIT_W:
        // if power is defined, calculate graph y position
        value = TeleinfoGetGraphData (TELEINFO_UNIT_W, period, phase, index, histo);
        is_valid = (teleinfo_period[period].pmax > 0);
        if (is_valid) graph_y = teleinfo_graph.height - (value * teleinfo_graph.height / teleinfo_period[period].pmax);
        break;

      case TELEINFO_UNIT_V:
        // if voltage is defined, calculate graph y position
        value = TeleinfoGetGraphData (TELEINFO_UNIT_V, period, phase, index, histo);
        is_valid = ((value != LONG_MAX) && (teleinfo_period[period].pmax != 0));
        if (is_valid)
        {
          // calculate graph value as delta from min & max range
          graph_delta = teleinfo_period[period].vmax - teleinfo_period[period].vmin;
          value = value - teleinfo_period[period].vmin;

          // calculate graph y position
          graph_y = teleinfo_graph.height;
          if (graph_delta > 0) graph_y -= (value * teleinfo_graph.height / graph_delta);
        }
        break;
      
      case TELEINFO_UNIT_COSPHI:
        // if power is defined, calculate graph y position
        value = TeleinfoGetGraphData (TELEINFO_UNIT_COSPHI, period, phase, index, histo);
        is_valid = ((value != LONG_MAX) && (teleinfo_period[period].pmax != 0));
        if (is_valid) graph_y = teleinfo_graph.height - (value * teleinfo_graph.height / 100);
        break;
    }

    // if current point is valid and should be displayed
    if (is_valid)
    {
      // calculate x position and display value
      graph_x = graph_left + (graph_width * index / TELEINFO_GRAPH_SAMPLE);
      sprintf (str_point, "%d,%d ", graph_x, graph_y);
      strlcat (str_result, str_point, sizeof (str_result));

      // if result string is long enough, send it
      if (strlen (str_result) > sizeof (str_result) - sizeof (str_point))
      {
        WSContentSend_P (str_result); 
        strcpy (str_result, "");
      }
    }
  }

  // end of polyline
  WSContentSend_P (str_result);
  WSContentSend_P (PSTR("'/>\n"));
}

// Graph curves data update
void TeleinfoWebGraphData ()
{
  int  phase, period, data, histo;
  char str_phase[8];

  // get numerical argument values
  period = TeleinfoWebGetArgValue (D_CMND_TELEINFO_PERIOD, TELEINFO_PERIOD_LIVE,  0, TELEINFO_PERIOD_MAX - 1);
  data   = TeleinfoWebGetArgValue (D_CMND_TELEINFO_DATA,   TELEINFO_UNIT_VA, 0, TELEINFO_UNIT_MAX - 1);
  histo  = TeleinfoWebGetArgValue (D_CMND_TELEINFO_HISTO,  0, 0, TELEINFO_HISTO_DAY_MAX);

  // check phase display argument
  strcpy (str_phase, "");
  if (Webserver->hasArg (D_CMND_TELEINFO_PHASE)) WebGetArg (D_CMND_TELEINFO_PHASE, str_phase, sizeof (str_phase));
  for (phase = strlen (str_phase); phase < teleinfo_contract.phase; phase++) strcat (str_phase, "1");

  // start of SVG graph
  WSContentBegin (200, CT_HTML);
  WSContentSend_P (PSTR ("<svg viewBox='%d %d %d %d'>\n"), 0, 0, TELEINFO_GRAPH_WIDTH, teleinfo_graph.height);

  // SVG style 
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));
  WSContentSend_P (PSTR ("polyline {fill:none;stroke-width:2;}\n"));
  for (phase = 0; phase < teleinfo_contract.phase; phase++) WSContentSend_P (PSTR ("polyline.ph%d {stroke:%s;}\n"), phase, ARR_TELEINFO_PHASE_COLOR[phase]);
  WSContentSend_P (PSTR ("line.time {stroke:white;stroke-width:1;}\n"));
  WSContentSend_P (PSTR ("text.time {font-size:1.2rem;fill:grey;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // ------------
  //     Time
  // ------------

  // get current time
  TeleinfoWebGraphTime (period, histo);

  // -----------------
  //   data curves
  // -----------------

#ifdef USE_UFILESYS

  // open historisation data file
  if (period != TELEINFO_PERIOD_LIVE) TeleinfoHistoOpen (period, histo);

#endif    // USE_UFILESYS

  // loop thru phases to display them if asked
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
    if (str_phase[phase] != '0') TeleinfoWebGraphCurve (period, data, phase, histo);

#ifdef USE_UFILESYS

  // close historisation data file
  if (period != TELEINFO_PERIOD_LIVE) TeleinfoHistoClose ();

#endif    // USE_UFILESYS

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
  long  value, value_min, value_max;
  float font_size, unit, unit_min, unit_max;
  char  str_unit[8];
  char  str_text[8];
  char  arr_label[5][8];

  // get parameters
  period  = TeleinfoWebGetArgValue (D_CMND_TELEINFO_PERIOD, TELEINFO_PERIOD_LIVE,  0, TELEINFO_PERIOD_MAX  - 1);
  data = TeleinfoWebGetArgValue (D_CMND_TELEINFO_DATA,   TELEINFO_UNIT_W, 0, TELEINFO_UNIT_MAX - 1);

  // set labels according to type of data
  switch (data) 
  {
    case TELEINFO_UNIT_VA:
      // init power range
      if (teleinfo_contract.ssousc > 0) teleinfo_period[period].pmax = teleinfo_contract.ssousc;
      else teleinfo_period[period].pmax = 1000;

      // set number of digits according to maximum power
      if (teleinfo_period[period].pmax >= 10000) nb_digit = 0;
      else if (teleinfo_period[period].pmax >= 1000) nb_digit = 1;

      // set values label
      itoa (0, arr_label[0], 10);
      TeleinfoDisplayValue (TELEINFO_UNIT_MAX, teleinfo_period[period].pmax / 4,     str_text, sizeof (str_text), nb_digit);
      strcpy (arr_label[1], str_text);
      TeleinfoDisplayValue (TELEINFO_UNIT_MAX, teleinfo_period[period].pmax / 2,     str_text, sizeof (str_text), nb_digit);
      strcpy (arr_label[2], str_text);
      TeleinfoDisplayValue (TELEINFO_UNIT_MAX, teleinfo_period[period].pmax * 3 / 4, str_text, sizeof (str_text), nb_digit);
      strcpy (arr_label[3], str_text);
      TeleinfoDisplayValue (TELEINFO_UNIT_MAX, teleinfo_period[period].pmax,         str_text, sizeof (str_text), nb_digit);
      strcpy (arr_label[4], str_text);
      break;

    case TELEINFO_UNIT_W:
      // init power range
      if (teleinfo_contract.ssousc > 0) teleinfo_period[period].pmax = teleinfo_contract.ssousc;
      else teleinfo_period[period].pmax = 1000;

      // set number of digits according to maximum power
      if (teleinfo_period[period].pmax >= 10000) nb_digit = 0;
      else if (teleinfo_period[period].pmax >= 1000) nb_digit = 1;

      // set values label
      itoa (0, arr_label[0], 10);
      TeleinfoDisplayValue (TELEINFO_UNIT_MAX, teleinfo_period[period].pmax / 4,     str_text, sizeof (str_text), nb_digit);
      strcpy (arr_label[1], str_text);
      TeleinfoDisplayValue (TELEINFO_UNIT_MAX, teleinfo_period[period].pmax / 2,     str_text, sizeof (str_text), nb_digit);
      strcpy (arr_label[2], str_text);
      TeleinfoDisplayValue (TELEINFO_UNIT_MAX, teleinfo_period[period].pmax * 3 / 4, str_text, sizeof (str_text), nb_digit);
      strcpy (arr_label[3], str_text);
      TeleinfoDisplayValue (TELEINFO_UNIT_MAX, teleinfo_period[period].pmax,         str_text, sizeof (str_text), nb_digit);
      strcpy (arr_label[4], str_text);
      break;

    case TELEINFO_UNIT_V:
      // init voltage range
      teleinfo_period[period].vmin = TELEINFO_GRAPH_VOLTAGE_MIN;
      teleinfo_period[period].vmax = TELEINFO_GRAPH_VOLTAGE_MAX;

      // calculate range values
      value_min = teleinfo_period[period].vmin;
      value_max = teleinfo_period[period].vmax;
      value     = value_max - value_min;

      // set values label
      itoa (value_min, arr_label[0], 10);
      itoa (value_min + value / 4, arr_label[1], 10);
      itoa (value_min + value / 2, arr_label[2], 10);
      itoa (value_min + value * 3 / 4, arr_label[3], 10);
      itoa (value_max, arr_label[4], 10);
      break;

    case TELEINFO_UNIT_COSPHI:
      // set values label
      strcpy (arr_label[0], "0");
      strcpy (arr_label[1], "0.25");
      strcpy (arr_label[2], "0.50");
      strcpy (arr_label[3], "0.75");
      strcpy (arr_label[4], "1");
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
  WSContentSend_P (PSTR ("<svg viewBox='%d %d %d %d'>\n"), 0, 0, TELEINFO_GRAPH_WIDTH, teleinfo_graph.height);

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
  if (nb_digit != -1) strcpy (str_text, "k");
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
void TeleinfoWebPageGraph ()
{
  bool display;
  int  index, phase, period, data, histo, choice;  
  long percentage;
  char str_phase[8];
  char str_type[8];
  char str_text[16];
  char str_date[16];

  // get numerical argument values
  period = TeleinfoWebGetArgValue (D_CMND_TELEINFO_PERIOD, TELEINFO_PERIOD_LIVE,  0, TELEINFO_PERIOD_MAX  - 1);
  data   = TeleinfoWebGetArgValue (D_CMND_TELEINFO_DATA,   TELEINFO_UNIT_VA, 0, TELEINFO_UNIT_MAX - 1);
  histo  = TeleinfoWebGetArgValue (D_CMND_TELEINFO_HISTO,  0, 0, TELEINFO_HISTO_DAY_MAX);
  teleinfo_graph.height = TeleinfoWebGetArgValue (D_CMND_TELEINFO_HEIGHT, teleinfo_graph.height, 100, INT_MAX);

  // check phase display argument
  strcpy (str_phase, "");
  if (Webserver->hasArg (D_CMND_TELEINFO_PHASE)) WebGetArg (D_CMND_TELEINFO_PHASE, str_phase, sizeof (str_phase));
  for (phase = strlen (str_phase); phase < teleinfo_contract.phase; phase++) strcat (str_phase, "1");

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
  WSContentSend_P (PSTR (".title {font-size:2rem;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.phase {display:inline-block;width:90px;padding:0.2rem;margin:0.2rem;border-radius:8px;}\n"));
  WSContentSend_P (PSTR ("div.phase span.power {font-size:1rem;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.phase span.volt {font-size:0.8rem;font-style:italic;}\n"));
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
    WSContentSend_P (PSTR ("div.ph%d {color:%s;border:1px %s solid;}\n"), phase, ARR_TELEINFO_PHASE_COLOR[phase], ARR_TELEINFO_PHASE_COLOR[phase]);
  WSContentSend_P (PSTR ("div.disabled {color:#444;}\n"));
  WSContentSend_P (PSTR ("div.choice {display:inline-block;font-size:1rem;padding:0px;margin:auto 5px;border:1px #666 solid;background:none;color:#fff;border-radius:6px;}\n"));
  WSContentSend_P (PSTR ("div.choice a {color:white;}\n"));
  WSContentSend_P (PSTR ("div.choice div {background:#666;}\n"));
  WSContentSend_P (PSTR ("div.choice div.disabled {background:none;}\n"));
  WSContentSend_P (PSTR ("div.choice a div {background:none;}\n"));
  WSContentSend_P (PSTR ("div.item {display:inline-block;padding:0.2rem auto;margin:1px;border:none;border-radius:4px;}\n"));
  WSContentSend_P (PSTR ("div.histo {font-size:0.9rem;}\n"));
  WSContentSend_P (PSTR ("div.day {width:50px;}\n"));
  WSContentSend_P (PSTR ("div.week {width:90px;}\n"));
  WSContentSend_P (PSTR ("div.choice a div.item:hover {background:#aaa;}\n"));
  WSContentSend_P (PSTR ("div.period {width:70px;}\n"));
  WSContentSend_P (PSTR ("div.data {width:50px;}\n"));
  WSContentSend_P (PSTR ("div.size {width:30px;}\n"));
  WSContentSend_P (PSTR ("hr {margin:0px;border:1px #666 dashed;}\n"));
  percentage = (100 * teleinfo_graph.height / TELEINFO_GRAPH_WIDTH) + 2; 
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
    if (display) strcpy (str_text, ""); else strcpy (str_text, "disabled");
    WSContentSend_P (PSTR ("<div class='phase ph%d %s'>"), phase, str_text);    
    TeleinfoDisplayCurrentValue (data, phase, str_text, sizeof (str_text), 1);
    WSContentSend_P (PSTR ("<span class='power' id='p%d'>%s</span>"), phase + 1, str_text);
    WSContentSend_P (PSTR ("</div></a>\n"));
  }
  WSContentSend_P (PSTR ("</div>\n"));

  // -----------------
  //     Data type
  // -----------------

  WSContentSend_P (PSTR ("<div>\n"));     // line

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

  // -----------------
  //      Height
  // -----------------

  WSContentSend_P (PSTR ("<div class='choice'>\n"));
  index = teleinfo_graph.height - TELEINFO_GRAPH_STEP;
  WSContentSend_P (PSTR ("<a href='%s?period=%d&data=%d&histo=%d&height=%d&phase=%s'><div class='item size'>-</div></a>"), D_TELEINFO_PAGE_GRAPH, period, data, histo, index, str_phase);
  index = teleinfo_graph.height + TELEINFO_GRAPH_STEP;
  WSContentSend_P (PSTR ("<a href='%s?period=%d&data=%d&histo=%d&height=%d&phase=%s'><div class='item size'>+</div></a>"), D_TELEINFO_PAGE_GRAPH, period, data, histo, index, str_phase);
  WSContentSend_P (PSTR ("</div>\n"));      // choice

  // start second line of parameters
  WSContentSend_P (PSTR ("</div>\n"));      // line

  // -----------------
  //      Period 
  // -----------------

  WSContentSend_P (PSTR ("<div>\n"));       // line

  WSContentSend_P (PSTR ("<div class='choice'>\n"));
  for (index = 0; index < TELEINFO_PERIOD_MAX; index++)
  {
    // get period label
    GetTextIndexed (str_text, sizeof (str_text), index, kTeleinfoGraphPeriod);

    // set button according to active state
    if ((period != index) || (histo > 0)) WSContentSend_P (PSTR ("<a href='%s?period=%d&data=%d&phase=%s'>"), D_TELEINFO_PAGE_GRAPH, index, data, str_phase);
    WSContentSend_P (PSTR ("<div class='item period'>%s</div>"), str_text);
    if ((period != index) || (histo > 0)) WSContentSend_P (PSTR ("</a>"));
    WSContentSend_P (PSTR ("\n"));
  }

  // -----------------
  //      Period 
  // -----------------

#ifdef USE_UFILESYS

  // set parameters according to period
  switch (period)
  {
    case TELEINFO_PERIOD_DAY:
      choice = TELEINFO_HISTO_DAY_MAX;
      strcpy (str_type, D_TELEINFO_HISTO_DAY);
      break;
    case TELEINFO_PERIOD_WEEK:
      choice = TELEINFO_HISTO_WEEK_MAX;
      strcpy (str_type, D_TELEINFO_HISTO_WEEK);
      break;
    default:
      choice = 0;
      break;
  }

  // if period is havong historic data
  if (choice > 0)
  {
    WSContentSend_P (PSTR ("<hr>\n"));
    for (index = choice; index >= 0; index--)
    {
      // display button if label is defined
      if (TeleinfoHistoGetDate (period, index, str_date))
      {
        // check if file exists
        display = TeleinfoHistoGetFilename (period, index, str_text);
        if (display) strcpy (str_text, ""); else strcpy (str_text, "disabled");

        // display file selection
        if (display && (index != histo)) WSContentSend_P (PSTR ("<a href='%s?period=%d&data=%d&histo=%d&phase=%s'>"), D_TELEINFO_PAGE_GRAPH, period, data, index, str_phase);
        WSContentSend_P (PSTR ("<div class='item histo %s %s'>%s</div>"), str_type, str_text, str_date);
        if (display && (index != histo)) WSContentSend_P (PSTR ("</a>"));
        WSContentSend_P (PSTR ("\n"));
      }
    }
  }

# endif     // USE_UFILESYS

  WSContentSend_P (PSTR ("</div>\n"));        // choice
  WSContentSend_P (PSTR ("</div>\n"));        // line

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

// energy driver
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
      break;
    case FUNC_LOOP:
      if (teleinfo_meter.status_rx == TIC_SERIAL_ACTIVE)
      {
        TeleinfoReceiveData ();
        TeleinfoCheckTCP ();
      }
      break;
  }

  return result;
}

// teleinfo sensor
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
    case FUNC_COMMAND:
      result = DecodeCommand (kTeleinfoCommands, TeleinfoCommand);
      break;
    case FUNC_EVERY_SECOND:
      if (teleinfo_meter.status_rx == TIC_SERIAL_INIT) TeleinfoStartSerial ();
      if (teleinfo_meter.status_rx == TIC_SERIAL_ACTIVE) TeleinfoEverySecond ();
      break;
    case FUNC_JSON_APPEND:
      if (teleinfo_meter.status_rx == TIC_SERIAL_ACTIVE) TeleinfoShowJSON (true, false, true);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      // config
      Webserver->on (FPSTR (D_TELEINFO_PAGE_CONFIG), TeleinfoWebPageConfig);

      // TIC message
      Webserver->on (FPSTR (D_TELEINFO_PAGE_TIC), TeleinfoWebPageTic);
      Webserver->on (FPSTR (D_TELEINFO_PAGE_TIC_UPD), TeleinfoWebTicUpdate);

      // graph
      Webserver->on (FPSTR (D_TELEINFO_PAGE_GRAPH),TeleinfoWebPageGraph);
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
