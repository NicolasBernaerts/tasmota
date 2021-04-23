/*
  xnrg_16_teleinfo.ino - France Teleinfo energy sensor support for Sonoff-Tasmota
  
  Copyright (C) 2020  Nicolas Bernaerts

  Settings are stored using weighting scale parameters :
    - Settings.weight_reference   = TIC message publication policy
    - Settings.weight_calibration = Meter data publication policy
  
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

#ifdef ESP32
#include <HardwareSerial.h>
#endif

/*********************************************************************************************\
 * Teleinfo historical
 * docs https://www.enedis.fr/sites/default/files/Enedis-NOI-CPT_54E.pdf
 * Teleinfo hardware will be enabled if 
 *     Hardware RX = [TinfoRX]
\*********************************************************************************************/

/*************************************************\
 *               Variables
\*************************************************/

// declare teleinfo energy driver and sensor
#define XNRG_15   15
#define XSNS_99   99

// teleinfo constant
#define TELEINFO_VOLTAGE             230        // default voltage provided
#define TELEINFO_VOLTAGE_REF         200        // voltage reference for max power calculation
#define TELEINFO_VOLTAGE_LOW         220        // minimum acceptable voltage
#define TELEINFO_VOLTAGE_HIGH        240        // maximum acceptable voltage
#define TELEINFO_PHASE_MAX           3          // maximum number of phases
#define TELEINFO_INDEX_MAX           10         // maximum number of total power counters
#define TELEINFO_SERIAL_BUFFER       4          // teleinfo serial buffer
#define TELEINFO_STORE_PERIOD        1800       // store energy totals every 30mn

// graph data
#define TELEINFO_GRAPH_SAMPLE        300         // number of samples per period
#define TELEINFO_GRAPH_WIDTH         1200        // graph width
#define TELEINFO_GRAPH_HEIGHT        800         // default graph height
#define TELEINFO_GRAPH_STEP          200         // graph height mofification step
#define TELEINFO_GRAPH_PERCENT_START 10     
#define TELEINFO_GRAPH_PERCENT_STOP  90

// string size
#define TELEINFO_STRING_ID_MAX        16        // max length of TIC contract ID
#define TELEINFO_STRING_VALUE_MAX     12        // max length of numerical value
#define TELEINFO_STRING_PERIOD_MAX    32        // max mength of period literal name
#define TELEINFO_STRING_HEADER_MAX    12        // max length of main screen header
#define TELEINFO_STRING_DISPLAY_MAX   32        // max length of main screen displayed value
#define TELEINFO_PART_MAX             4         // maximum number of parts in a line
#define TELEINFO_LINE_MAX             74        // maximum number of lines handled in a TIC message
#define TELEINFO_STRING_LINE_MAX      128       // maximum size of a received TIC line
#define TELEINFO_STRING_ETIQUETTE_MAX 12        // maximum size of a TIC etiquette
#define TELEINFO_STRING_DONNEE_MAX    32        // maximum size of a TIC donnee

#ifdef ESP32
#undef TELEINFO_STRING_DONNEE_MAX
#define TELEINFO_STRING_DONNEE_MAX    92
#endif

// commands
#define D_CMND_TELEINFO_MODE         "mode"
#define D_CMND_TELEINFO_ETH          "eth"
#define D_CMND_TELEINFO_PHASE        "phase"
#define D_CMND_TELEINFO_PERIOD       "period"
#define D_CMND_TELEINFO_DATA         "data"
#define D_CMND_TELEINFO_HEIGHT       "height"
#define D_CMND_TELEINFO_CFG_RATE     "rate"
#define D_CMND_TELEINFO_CFG_TIC      "tic"
#define D_CMND_TELEINFO_CFG_DATA     "data"

// JSON TIC extensions
#define TELEINFO_JSON_METER          "METER"
#define TELEINFO_JSON_TIC            "TIC"
#define TELEINFO_JSON_PERIOD         "PERIOD"
#define TELEINFO_JSON_PHASE          "PHASE"
#define TELEINFO_JSON_ID             "ID"
#define TELEINFO_JSON_IREF           "IREF"
#define TELEINFO_JSON_PREF           "PREF"

// interface strings
#define D_TELEINFO                   "Teleinfo"
#define D_TELEINFO_TIC               "TIC"
#define D_TELEINFO_MESSAGE           "Messages"
#define D_TELEINFO_CONTRACT          "Contract"
#define D_TELEINFO_HEURES            "Heures"
#define D_TELEINFO_ERROR             "Errors"
#define D_TELEINFO_MODE              "Mode"
#define D_TELEINFO_RESET             "Message reset"
#define D_TELEINFO_PERIOD            "Period"
#define D_TELEINFO_GRAPH             "Graph"
#define D_TELEINFO_CHECKSUM          "Checksum"
#define D_TELEINFO_BAUD              "Bauds"
#define D_TELEINFO_DATA              "Data"
#define D_TELEINFO_DIFFUSION         "Diffusion"

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
const char D_TELEINFO_CONFIG[]   PROGMEM = "Configure Teleinfo";
const char TELEINFO_DISABLED[]   PROGMEM = "Disabled";
const char TELEINFO_APPARENT[]   PROGMEM = "Apparent";
const char TELEINFO_COSPHI[]     PROGMEM = "Cos φ";
const char TELEINFO_TITLE_RATE[] PROGMEM = "TIC Rate";
const char TELEINFO_TITLE_TIC[]  PROGMEM = "Send Teleinfo data";
const char TELEINFO_TITLE_DATA[] PROGMEM = "Send Meter data";

// form strings
const char TELEINFO_INPUT_RADIO[] PROGMEM = "<p><input type='radio' name='%s' value='%d' %s>%s</p>\n";
const char TELEINFO_FORM_START[]  PROGMEM = "<form method='get' action='%s'>\n";
const char TELEINFO_FORM_STOP[]   PROGMEM = "</form>\n";
const char TELEINFO_FIELD_START[] PROGMEM = "<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>\n";
const char TELEINFO_FIELD_STOP[]  PROGMEM = "</fieldset></p><br>\n";
const char TELEINFO_HTML_POWER[]  PROGMEM = "<text class='power' x='%d%%' y='%d%%'>%s</text>\n";
const char TELEINFO_HTML_DASH[]   PROGMEM = "<line class='dash' x1='%d%%' y1='%d%%' x2='%d%%' y2='%d%%' />\n";
const char TELEINFO_HTML_BAR[]    PROGMEM = "<tr><div style='margin:4px;padding:0px;background-color:#ddd;border-radius:4px;'><div style='font-size:0.75rem;font-weight:bold;padding:0px;text-align:center;border:1px solid #bbb;border-radius:4px;color:#444;background-color:%s;width:%s%%;'>%s%%</div></div></tr>\n";

// TIC - specific etiquettes
enum TeleinfoEtiquette { TIC_NONE, TIC_ADCO, TIC_ADSC, TIC_PTEC, TIC_NGTF, TIC_EAIT, TIC_IINST, TIC_IINST1, TIC_IINST2, TIC_IINST3, TIC_ISOUSC, TIC_PS, TIC_PAPP, TIC_SINSTS, TIC_BASE, TIC_EAST, TIC_HCHC, TIC_HCHP, TIC_EJPHN, TIC_EJPHPM, TIC_BBRHCJB, TIC_BBRHPJB, TIC_BBRHCJW, TIC_BBRHPJW, TIC_BBRHCJR, TIC_BBRHPJR, TIC_ADPS, TIC_ADIR1, TIC_ADIR2, TIC_ADIR3, TIC_URMS1, TIC_URMS2, TIC_URMS3, TIC_UMOY1, TIC_UMOY2, TIC_UMOY3, TIC_IRMS1, TIC_IRMS2, TIC_IRMS3, TIC_SINSTS1, TIC_SINSTS2, TIC_SINSTS3, TIC_PTCOUR, TIC_PREF, TIC_PCOUP, TIC_LTARF, TIC_EASF01, TIC_EASF02, TIC_EASF03, TIC_EASF04, TIC_EASF05, TIC_EASF06, TIC_EASF07, TIC_EASF08, TIC_EASF09, TIC_EASF10, TIC_MAX };
const char kTeleinfoEtiquetteName[] PROGMEM = "|ADCO|ADSC|PTEC|NGTF|EAIT|IINST|IINST1|IINST2|IINST3|ISOUSC|PS|PAPP|SINSTS|BASE|EAST|HCHC|HCHP|EJPHN|EJPHPM|BBRHCJB|BBRHPJB|BBRHCJW|BBRHPJW|BBRHCJR|BBRHPJR|ADPS|ADIR1|ADIR2|ADIR3|URMS1|URMS2|URMS3|UMOY1|UMOY2|UMOY3|IRMS1|IRMS2|IRMS3|SINSTS1|SINSTS2|SINSTS3|PTCOUR|PREF|PCOUP|LTARF|EASF01|EASF02|EASF03|EASF04|EASF05|EASF06|EASF07|EASF08|EASF09|EASF10";

// TIC - modes and rates
enum TeleinfoMode { TIC_MODE_UNDEFINED, TIC_MODE_HISTORIC, TIC_MODE_STANDARD };
const char kTeleinfoModeName[] PROGMEM = "|Historique|Standard";
const int ARR_TELEINFO_RATE[] = { 0, 1200, 2400, 4800, 9600, 19200 }; 

// TIC - tarifs
enum TeleinfoPeriod { TIC_HISTO_TH, TIC_HISTO_HC, TIC_HISTO_HP, TIC_HISTO_HN, TIC_HISTO_PM, TIC_HISTO_CB, TIC_HISTO_CW, TIC_HISTO_CR, TIC_HISTO_PB, TIC_HISTO_PW, TIC_HISTO_PR, TIC_STD_P, TIC_STD_HPH, TIC_STD_HCH, TIC_STD_HPD, TIC_STD_HCD, TIC_STD_HPE, TIC_STD_HCE, TIC_STD_JA, TIC_STD_PM, TIC_STD_HH, TIC_STD_HD, TIC_STD_HM, TIC_STD_DSM, TIC_STD_SCM, TIC_STD_1, TIC_STD_2, TIC_STD_3, TIC_STD_4, TIC_STD_5, TIC_STD_BASE, TIC_STD_HP, TIC_STD_HC };
const char kTeleinfoPeriod[] PROGMEM = "TH..|HC..|HP..|HN..|PM..|HCJB|HCJW|HCJR|HPJB|HPJW|HPJR|P|HPH|HCH|HPD|HCD|HPE|HCE|JA|PM|HH|HD|HM|DSM|SCM|1|2|3|4|5|BASE|HEURE PLEINE|HEURE CREUSE";
const char kTeleinfoPeriodName[] PROGMEM = "Toutes|Creuses|Pleines|Normales|Pointe Mobile|Creuses Bleus|Creuses Blancs|Creuses Rouges|Pleines Bleus|Pleines Blancs|Pleines Rouges|Pointe|Pleines Hiver|Creuses Hiver|Pleines Demi-saison|Creuses Demi-saison|Pleines Ete|Creuses Ete|Juillet-Aout|Pointe Mobile|Hiver|Demi-saison|Hiver Mobile|Demi-saison Mobile|Saison Creuse Mobile|Pointe|Pleines Hiver|Creuses Hiver|Pleines Ete|Creuses Ete|Base|Pleines|Creuses";
const int ARR_TELEINFO_PERIOD_INDEX[] { 1, 2, 2, 2, 2, 6, 6, 6, 6, 6, 6, 8, 8, 8, 8, 8, 8, 8, 8, 6, 6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 1, 2, 2 };

// TIC - data diffusion policy
enum TeleinfoConfigDiffusion { TELEINFO_POLICY_NEVER, TELEINFO_POLICY_MESSAGE, TELEINFO_POLICY_PERCENT, TELEINFO_POLICY_TELEMETRY, TELEINFO_POLICY_MAX };
const char TELEINFO_CFG_LABEL0[] PROGMEM = "Never";
const char TELEINFO_CFG_LABEL1[] PROGMEM = "Every TIC message";
const char TELEINFO_CFG_LABEL2[] PROGMEM = "When Power change (± 1%)";
const char TELEINFO_CFG_LABEL3[] PROGMEM = "With Telemetry only";
const char *const ARR_TELEINFO_CFG_LABEL[] PROGMEM = { TELEINFO_CFG_LABEL0, TELEINFO_CFG_LABEL1, TELEINFO_CFG_LABEL2, TELEINFO_CFG_LABEL3 };

// graph - periods
enum TeleinfoGraphPeriod { TELEINFO_PERIOD_LIVE, TELEINFO_PERIOD_DAY, TELEINFO_PERIOD_WEEK, TELEINFO_PERIOD_YEAR, TELEINFO_PERIOD_MAX };      // available graph periods
const char kTeleinfoGraphPeriod[] PROGMEM = "Live|Day|Week|Year";                                                                             // period labels
const long ARR_TELEINFO_PERIOD_SAMPLE[] = { 1800/TELEINFO_GRAPH_SAMPLE, 86400/TELEINFO_GRAPH_SAMPLE, 604800/TELEINFO_GRAPH_SAMPLE, 31536000/TELEINFO_GRAPH_SAMPLE };                                                                           // number of seconds between samples

// graph - display
enum TeleinfoGraphDisplay { TELEINFO_DISPLAY_POWER, TELEINFO_DISPLAY_VOLTAGE, TELEINFO_DISPLAY_COSPHI, TELEINFO_DISPLAY_MAX };               // available graph displays
const char kTeleinfoGraphDisplay[] PROGMEM = "Power|Voltage|Cos φ";                                                                                 // data display labels

// graph - phase colors
const char TELEINFO_PHASE_COLOR0[] PROGMEM = "#5dade2";    // blue
const char TELEINFO_PHASE_COLOR1[] PROGMEM = "#f5b041";    // orange
const char TELEINFO_PHASE_COLOR2[] PROGMEM = "#52be80";    // green
const char *const ARR_TELEINFO_PHASE_COLOR[] PROGMEM = { TELEINFO_PHASE_COLOR0, TELEINFO_PHASE_COLOR1, TELEINFO_PHASE_COLOR2 };

// graph - week days name
const char *const arr_week_day[] PROGMEM = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

// teleinfo driver status
bool teleinfo_enabled = false;

// serial port
#ifdef ESP8266
TasmotaSerial *teleinfo_serial = nullptr;
#else  // ESP32
HardwareSerial *teleinfo_serial = nullptr;
#endif // ESP286 & ESP32

// teleinfo data
struct {
  int    phase   = 1;                                 // number of phases
  int    mode    = TIC_MODE_UNDEFINED;                // meter mode
  long   voltage = TELEINFO_VOLTAGE;                  // contract reference voltage
  long   isousc  = 0;                                 // contract max current per phase
  long   ssousc  = 0;                                 // contract max power per phase
  char   str_id[TELEINFO_STRING_ID_MAX];              // contract reference (adco or ads)
  char   str_period[TELEINFO_STRING_PERIOD_MAX];      // current tarif period
} teleinfo_contract;

// teleinfo current line
struct {
  char str_text[TELEINFO_STRING_LINE_MAX];
  char separator;
} teleinfo_line;

// teleinfo power counters
struct {
  long store    = 0;                          // number of second since last energy totals storage
  long papp     = 0;                          // total apparent power
  long total    = 0;                          // total of all indexes for current contract
  int  nb_index = TELEINFO_INDEX_MAX;         // number of indexes in current contract      
  long index[TELEINFO_INDEX_MAX];             // array of indexes of different tarif periods
} teleinfo_counter;

// cos phi data
struct {
  uint32_t start_time  = 0;                   // timestamp of measurement start (ms)
  uint32_t update_time = 0;                   // timestamp of last measurement update (ms)
  uint32_t start_total = 0;                   // total counter at start of measurement (Wh)
  float    delta_va[TELEINFO_PHASE_MAX];      // counter for apparent power since start of measurement (VA)
} teleinfo_cosphi; 

// data per phase
struct tic_phase {
  bool  volt_set;                           // voltage set in current message
  long  voltage;                            // instant voltage
  long  current;                            // instant current
  long  sinsts;                             // instant apparent power (may be a sum)
  long  papp;                               // instant apparent power
  long  pact;                               // instant active power
  long  papp_last;                          // last published apparent power
  float cos_phi;                            // cos phi
}; 
tic_phase teleinfo_phase[TELEINFO_PHASE_MAX];

// TIC message array
struct tic_line {
  char str_etiquette[TELEINFO_STRING_ETIQUETTE_MAX];
  char str_donnee[TELEINFO_STRING_DONNEE_MAX];
  char checksum;
};

struct {
  bool overload   = false;            // overload has been detected
  bool received   = false;            // one full message has been received
  bool percent    = false;            // power has changed of more than 1% on one phase
  long nb_message = 0;                // number of received messages
  long nb_data    = 0;                // number of received data (lines)
  long nb_error   = 0;                // number of checksum errors
  long nb_reset   = 0;                // number of message reset
  int  line_index = 0;                // index of current received message line
  int  line_max   = 0;                // max number of lines in a message
  tic_line line[TELEINFO_LINE_MAX];   // array of message lines
} teleinfo_message;

// graph
struct tic_period {
  bool     updated;                                               // flag to ask for graph update
  int      index;                                                 // current array index per refresh period
  long     counter;                                               // counter in seconds of current refresh period

  // --- limit values for graph display ---
  uint16_t vmin;                                                  // graph minimum voltage
  uint16_t vmax;                                                  // graph maximum voltage
  uint16_t pmax;                                                  // graph maximum power

  // --- arrays for current refresh period (per phase) ---
  uint16_t volt_low[TELEINFO_PHASE_MAX];                          // peak low voltage during refresh period
  uint16_t volt_high[TELEINFO_PHASE_MAX];                         // peak high voltage during refresh period
  uint16_t papp_high[TELEINFO_PHASE_MAX];                         // peak apparent power during refresh period
  uint32_t papp_sum[TELEINFO_PHASE_MAX];                          // sum of apparent power during refresh period
  float    cosphi_sum[TELEINFO_PHASE_MAX];                        // sum of cos phi during refresh period

  // --- arrays with all graph values ---
  uint8_t  arr_papp[TELEINFO_PHASE_MAX][TELEINFO_GRAPH_SAMPLE];   // array of apparent power graph values
  uint8_t  arr_volt[TELEINFO_PHASE_MAX][TELEINFO_GRAPH_SAMPLE];   // array min and max voltage delta
  uint8_t  arr_cosphi[TELEINFO_PHASE_MAX][TELEINFO_GRAPH_SAMPLE]; // array of cos phi
}; 
tic_period teleinfo_graph[TELEINFO_PERIOD_MAX];

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

// get TIC message publication policy
int TeleinfoGetPolicyTIC ()
{
  int policy;

  // read actual teleinfo mode
  policy = (int)Settings.weight_reference;
  if (policy < 0) policy = TELEINFO_POLICY_TELEMETRY;
  if (policy >= TELEINFO_POLICY_MAX) policy = TELEINFO_POLICY_TELEMETRY;
  
  return policy;
}

// set TIC message publication policy
void TeleinfoSetPolicyTIC (int policy)
{
  Settings.weight_reference = (ulong)policy;
}

// get Meter data publication policy
int TeleinfoGetPolicyData ()
{
  int policy;

  // read actual teleinfo mode
  policy = (int)Settings.weight_calibration;
  if ((policy < 0) || (policy >= TELEINFO_POLICY_MAX)) policy = TELEINFO_POLICY_TELEMETRY;
  
  return policy;
}

// set Meter data publication policy
void TeleinfoSetPolicyData (int policy)
{
  if ((policy < 0) || (policy >= TELEINFO_POLICY_MAX)) policy = TELEINFO_POLICY_TELEMETRY;
  Settings.weight_calibration = (ulong)policy;
}

// get teleinfo baud rate
uint16_t TeleinfoGetBaudRate ()
{
  uint16_t actual_rate;

  // read actual teleinfo baud rate
  actual_rate = Settings.sbaudrate;
  if (actual_rate > 19200) actual_rate = 0;
  
  return actual_rate;
}

// set teleinfo baud rate
void TeleinfoSetBaudRate (uint16_t new_rate)
{
  // if within range, set baud rate
  if (new_rate <= 19200)
  {
    // if mode has changed
    if (Settings.sbaudrate != new_rate)
    {
      // save mode
      Settings.sbaudrate = new_rate;

      // ask for restart
      TasmotaGlobal.restart_flag = 2;
    }
  }
}

// validate graph access data
bool TeleinfoValidateGraphAccess (int period, int phase, int index)
{
  bool validation = true;

  // check that values are within range
  validation &= ((period >= 0) && (period < TELEINFO_PERIOD_MAX));
  validation &= ((phase >= 0)  && (phase < TELEINFO_PHASE_MAX));
  validation &= ((index >= 0)  && (index < TELEINFO_GRAPH_SAMPLE));

  return validation;
}

// set graph power value
void TeleinfoSaveGraphPower (int period, int phase, int index)
{
  bool     data_valid, power_valid;
  uint32_t graph_value;

  // check power value
  power_valid = (teleinfo_contract.ssousc > 0);

  // if all provided data are valid, update graph data
  data_valid = TeleinfoValidateGraphAccess (period, phase, index);
  if (data_valid && power_valid)
  {
    // if overload has been detected during period, use overload value, else use average value
    if (teleinfo_graph[period].papp_high[phase] > teleinfo_contract.ssousc) graph_value = (uint32_t)teleinfo_graph[period].papp_high[phase];
    else graph_value = teleinfo_graph[period].papp_sum[phase] / ARR_TELEINFO_PERIOD_SAMPLE[period];

    // calculate percentage of power according to contract (200 = 100% of contract power)
    graph_value = graph_value * 200 / teleinfo_contract.ssousc;
    teleinfo_graph[period].arr_papp[phase][index] = (uint8_t)graph_value;
  }

  // if period and phase are valid, reset them
  if (data_valid)
  {
    // reset period data
    teleinfo_graph[period].papp_high[phase] = 0;
    teleinfo_graph[period].papp_sum[phase]  = 0;
  }
}

// get graph power value
uint16_t TeleinfoGetGraphPower (int period, int phase, int index)
{
  bool     data_valid;
  uint32_t graph_value = UINT16_MAX;

  // if all provided data are valid, get graph data
  data_valid = TeleinfoValidateGraphAccess (period, phase, index);
  if (data_valid)
  {
    // if value is defined
    if (teleinfo_graph[period].arr_papp[phase][index] != UINT8_MAX)
    {
      // calculate percentage of power according to contract
      graph_value = (uint32_t)teleinfo_graph[period].arr_papp[phase][index];
      graph_value = graph_value * teleinfo_contract.ssousc / TELEINFO_VOLTAGE_REF;
    }
  }

  return (uint16_t)graph_value;
}

// set graph voltage value
void TeleinfoSaveGraphVoltage (int period, int phase, int index)
{
  bool     data_valid;
  uint16_t voltage;

  // if all provided data are valid, get graph data
  data_valid = TeleinfoValidateGraphAccess (period, phase, index);
  if (data_valid)
  {
    // if maximum voltage is above maximum acceptable value, store it
    if (teleinfo_graph[period].volt_high[phase] > TELEINFO_VOLTAGE_HIGH) voltage = teleinfo_graph[period].volt_high[phase];

    // else, if minimum voltage is below acceptable value, store it
    else if (teleinfo_graph[period].volt_low[phase] < TELEINFO_VOLTAGE_LOW) voltage = teleinfo_graph[period].volt_low[phase];

    // else, store average value between min and max
    else voltage = (teleinfo_graph[period].volt_high[phase] + teleinfo_graph[period].volt_low[phase]) / 2;

    // calculate value to be stored
    if (voltage > TELEINFO_VOLTAGE) voltage = voltage + 128 - TELEINFO_VOLTAGE;
    else if (voltage < TELEINFO_VOLTAGE) voltage = 128 + voltage - TELEINFO_VOLTAGE;
    else voltage = 128;

    // store value
    teleinfo_graph[period].arr_volt[phase][index] = (uint8_t)voltage;

    // reset period data
    teleinfo_graph[period].volt_low[phase]  = UINT16_MAX;
    teleinfo_graph[period].volt_high[phase] = UINT16_MAX;
  }
}

// get graph voltage value
uint16_t TeleinfoGetGraphVoltage (int period, int phase, int index)
{
  bool     data_valid;
  uint16_t voltage = UINT16_MAX;

  // if all provided data are valid, get graph data
  data_valid = TeleinfoValidateGraphAccess (period, phase, index);
  if (data_valid)
  {
    // if voltage has been stored
    if (teleinfo_graph[period].arr_volt[phase][index] != UINT8_MAX) voltage = TELEINFO_VOLTAGE - 128 + teleinfo_graph[period].arr_volt[phase][index];
  }

  return voltage;
}

// set graph cos phi value
void TeleinfoSaveGraphCosPhi (int period, int phase, int index)
{
  bool  data_valid;
  float value_cosphi;

  // if all provided data are valid, get graph data
  data_valid = TeleinfoValidateGraphAccess (period, phase, index);
  if (data_valid)
  {
    // if maximum voltage is above maximum acceptable value, store it
    value_cosphi = 100 * teleinfo_graph[period].cosphi_sum[phase] / ARR_TELEINFO_PERIOD_SAMPLE[period];

    // store value
    teleinfo_graph[period].arr_cosphi[phase][index] = (uint8_t)value_cosphi;

    // reset sum
    teleinfo_graph[period].cosphi_sum[phase] = 0;
  }
}

// get graph voltage value
int TeleinfoGetGraphCosPhi (int period, int phase, int index)
{
  bool data_valid;
  int  value = 1;

  // if all provided data are valid, get graph data
  data_valid = TeleinfoValidateGraphAccess (period, phase, index);
  if (data_valid)
  {
    // if voltage has been stored
    value = (int)teleinfo_graph[period].arr_cosphi[phase][index];
  }

  return value;
}

/*********************************************\
 *               Functions
\*********************************************/

char TeleinfoCalculateChecksum (const char* pstr_line) 
{
  int     index, line_size;
  uint8_t line_checksum  = 0;
  uint8_t given_checksum = 0;

  // if given line exists
  if (pstr_line != nullptr)
  {
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
      teleinfo_message.nb_error++;
      AddLog (LOG_LEVEL_INFO, PSTR ("ERR: %s [%c]"), pstr_line, line_checksum);

      // reset checksum
      line_checksum = 0;
    } 
  }

  return line_checksum;
}

void TeleinfoSplitLine (char* pstr_line, char separator, char* pstr_etiquette, int size_etiquette, char* pstr_donnee, int size_donnee)
{
  int   index = 0;
  char  arr_separator[2];
  char* pstr_text;

  // if strings are defined
  if ((pstr_line != nullptr) && (pstr_etiquette != nullptr) && (pstr_donnee != nullptr))
    {
    // init strings
    arr_separator[0] = separator;
    arr_separator[1] = 0;
    strcpy (pstr_etiquette, "");
    strcpy (pstr_donnee, "");

    // split line in parts
    pstr_text = strtok (pstr_line, arr_separator);
    while (pstr_text != nullptr)
    {
      // extract etiquette and donnee
      if (index == 0) strlcpy (pstr_etiquette, pstr_text, size_etiquette);
      else if (strlen (pstr_text) > 1) strlcpy (pstr_donnee, pstr_text, size_donnee);

      // switch to next token
      pstr_text = strtok (nullptr, arr_separator);
      index++;
    }
  }
}

void TeleinfoCleanupString (char* pstr_string, char* pstr_result, int max_size) 
{
  int   index = 0;
  char* pstr_token;

  // if strings are valid
  if ((pstr_string != nullptr) && (pstr_result != nullptr))
  {
    // init
    strcpy (pstr_result, "");

    // split line in parts
    pstr_token = strtok (pstr_string, " ");
    while (pstr_token != nullptr)
    {
      // if current token defined
      if (strlen(pstr_token) > 0)
      {
        // if one token already found, append SPACE
        if (index > 0) strlcat (pstr_result, " ", max_size);

        // append current token
        strlcat (pstr_result, pstr_token, max_size);
      }

      // switch to next token
      pstr_token = strtok (nullptr, " ");
      index++;
    }
  }
}

void TeleinfoUpdateCosPhi ()
{
  int      phase;
  uint32_t time_now;
  long     difference_w;
  float    global_va, global_w;
  float    phase_w;
  float    delta_va, cos_phi;
  float    time_delta;
  float    phase_va[TELEINFO_PHASE_MAX];

  // timestamp
  time_now = millis ();

  // first initialisation
  if (teleinfo_cosphi.start_time  == 0) teleinfo_cosphi.start_time  = time_now;
  if (teleinfo_cosphi.update_time == 0) teleinfo_cosphi.update_time = time_now;
  if (teleinfo_cosphi.start_total == 0) teleinfo_cosphi.start_total = teleinfo_counter.total;

  // calculate time difference since last measurement (in sec.)
  time_delta = (float)(time_now - teleinfo_cosphi.update_time) / 1000;
  teleinfo_cosphi.update_time = time_now;

  // update data since last measurement
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    delta_va = (float)teleinfo_phase[phase].papp * time_delta / 3600;
    teleinfo_cosphi.delta_va[phase] += delta_va;
  }

  // if total power difference has execeeded 10Wn update cos phi
  difference_w = teleinfo_counter.total - teleinfo_cosphi.start_total;
  if (difference_w >= 10)
  {
    // calculate time difference since start time (in sec.)
    time_delta = (float)(time_now - teleinfo_cosphi.start_time) / 1000;

    // loop thru phase to calculate apparent power
    global_va = 0;
    for (phase = 0; phase < teleinfo_contract.phase; phase++)
    {
      // calculate phase apparent power in VA during period
      phase_va[phase] = teleinfo_cosphi.delta_va[phase] / time_delta * 3600;
      global_va += phase_va[phase];

      // reset cos phi phase couter
      teleinfo_cosphi.delta_va[phase] = 0;
    }

    // calculate global active power on the period
    global_w = (float)difference_w / time_delta * 3600;

    // calculate cos phi (similar for all phases as active power is published globally)
    if (global_va > 0) cos_phi = min (global_w / global_va, (float)1);
    else cos_phi = 1;

    // loop thru phase to update cos phi
    for (phase = 0; phase < teleinfo_contract.phase; phase++) teleinfo_phase[phase].cos_phi = cos_phi;

    // reset measurement data
    teleinfo_cosphi.start_time  = time_now;
    teleinfo_cosphi.start_total = teleinfo_counter.total;    
  } 
}

void TeleinfoPreInit ()
{
  // if PIN defined, set energy driver
  if (!TasmotaGlobal.energy_driver && PinUsed (GPIO_TELEINFO_RX))
  {
    // energy driver is teleinfo meter
    TasmotaGlobal.energy_driver = XNRG_15;
  }
}

void TeleinfoInit ()
{
  int      index;
  uint16_t baud_rate;

  // get teleinfo speed
  baud_rate = TeleinfoGetBaudRate ();

  // if sensor has been pre initialised
  teleinfo_enabled = ((TasmotaGlobal.energy_driver == XNRG_15) && (baud_rate > 0));
  if (teleinfo_enabled)
  {

#ifdef ESP8266

    // create serial port with buffer set to 256 (to handle 19200bps with 100ms loop)
    teleinfo_serial = new TasmotaSerial (Pin (GPIO_TELEINFO_RX), -1, 2, 1, 256);

    // if port has been created
    if (teleinfo_serial->begin (baud_rate))
    {
      // check that it's connected to hardware GPIO
      if (teleinfo_serial->hardwareSerial ())
      {
        // init hardware port
        Serial.begin (baud_rate, SERIAL_7E1);
        ClaimSerial ();

        // log
        AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Teleinfo ESP8266 hardware serial initialised"));
      }
    }

#else  // ESP32

    // use UART2 (some board have USB on UART1)
    teleinfo_serial = new HardwareSerial (2);

    // set buffer to 256 (to handle 19200bps with 100ms loop)
    teleinfo_serial->setRxBufferSize (256); 

    // init UART          
    teleinfo_serial->begin (baud_rate, SERIAL_7E1, Pin(GPIO_TELEINFO_RX), -1);

    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Teleinfo ESP32 hardware serial initialised"));

#endif // ESP286 & ESP32

    // init hardware energy counters
    Settings.flag3.hardware_energy_total = true;
    RtcSettings.energy_kWhtotal = 0;
    RtcSettings.energy_kWhtoday = 0;
    Settings.energy_kWhtotal    = 0;

    // set default energy parameters
    Energy.voltage_available = true;
    Energy.current_available = true;
    for (index = 0; index < TELEINFO_PHASE_MAX; index++)
    {
      // ernergy counter
      Energy.voltage[index] = TELEINFO_VOLTAGE;
      Energy.current[index] = 0;
      Energy.apparent_power[index] = 0;

      // cos phi values
      teleinfo_phase[index].cos_phi = 1;
      teleinfo_cosphi.delta_va[index] = 0;
    }

    // init strings
    strcpy (teleinfo_contract.str_id, "");
    strcpy (teleinfo_contract.str_period, "");

    // init all total indexes
    for (index = 0; index < TELEINFO_INDEX_MAX; index ++) teleinfo_counter.index[index] = 0;

    // disable all message lines
    for (index = 0; index < TELEINFO_LINE_MAX; index ++) teleinfo_message.line[index].checksum = 0;
  }

  // else disable energy driver
  else TasmotaGlobal.energy_driver = ENERGY_NONE;
}

void TeleinfoGraphInit ()
{
  int index, phase, period;

  // initialise phase data
  for (phase = 0; phase < TELEINFO_PHASE_MAX; phase++)
  {
    // default values
    teleinfo_phase[phase].voltage   = TELEINFO_VOLTAGE;
    teleinfo_phase[phase].current   = 0;
    teleinfo_phase[phase].papp      = 0;
    teleinfo_phase[phase].papp_last = 0;
  }

  // initialise graph data
  for (period = 0; period < TELEINFO_PERIOD_MAX; period++)
  {
    // init counter
    teleinfo_graph[period].index   = 0;
    teleinfo_graph[period].counter = 0;

    // loop thru phase
    for (phase = 0; phase < TELEINFO_PHASE_MAX; phase++)
    {
      // init max power per period
      teleinfo_graph[period].volt_low[phase]   = UINT16_MAX;
      teleinfo_graph[period].volt_high[phase]  = UINT16_MAX;
      teleinfo_graph[period].papp_high[phase]  = 0;
      teleinfo_graph[period].papp_sum[phase]   = 0;
      teleinfo_graph[period].cosphi_sum[phase] = 100;

      // loop thru graph values
      for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
      {
        teleinfo_graph[period].arr_volt[phase][index]   = UINT8_MAX;
        teleinfo_graph[period].arr_papp[phase][index]   = UINT8_MAX;
        teleinfo_graph[period].arr_cosphi[phase][index] = UINT8_MAX;
      } 
    }
  }
}

// function to handle received teleinfo data
void TeleinfoReceiveData ()
{
  bool  overload = false;
  char  recv_serial, checksum;
  int   index, phase, max_size;
  long  current_total, power_total, counter_total;
  float active_power;
  char  str_etiquette[TELEINFO_STRING_ETIQUETTE_MAX];
  char  str_donnee[TELEINFO_STRING_LINE_MAX];
  char  str_result[TELEINFO_STRING_LINE_MAX];

  while (teleinfo_serial->available() > TELEINFO_SERIAL_BUFFER) 
  {
    // read caracter
    recv_serial = teleinfo_serial->read(); 

    switch (recv_serial)
    {
      // ---------------------------
      // 0x02 : Beginning of message 
      // ---------------------------
      case 2:
        // reset current message line index
        teleinfo_message.line_index = 0;

        // reset voltage flags
        for (phase = 0; phase < teleinfo_contract.phase; phase++) teleinfo_phase[phase].volt_set = false;
        break;
          
      // ---------------------
      // Ox03 : End of message
      // ---------------------
      case 3:
        // increment message counter
        teleinfo_message.nb_message++;

        // loop to remove unused message lines
        for (index = teleinfo_message.line_index; index < TELEINFO_LINE_MAX; index++)
        {
          strcpy (teleinfo_message.line[index].str_etiquette, "");
          strcpy (teleinfo_message.line[index].str_donnee, "");
          teleinfo_message.line[index].checksum = 0;
        }

        // save number of lines and reset index
        if (teleinfo_message.line_index < teleinfo_message.line_max - 10) teleinfo_message.line_max = teleinfo_message.line_index;
        else teleinfo_message.line_max = max (teleinfo_message.line_max, teleinfo_message.line_index);
        teleinfo_message.line_index = 0;

        // if defined, defined maximum power per phase
        if ((teleinfo_contract.ssousc == 0) && (teleinfo_contract.isousc != 0)) teleinfo_contract.ssousc = teleinfo_contract.isousc * TELEINFO_VOLTAGE_REF;

        // if needed, declare voltage and number of phases
        if (Energy.phase_count != teleinfo_contract.phase) Energy.phase_count = teleinfo_contract.phase;

        // loop to calculate total current and power
        current_total = 0;
        power_total = 0;
        for (phase = 0; phase < teleinfo_contract.phase; phase++)
        {
          current_total += teleinfo_phase[phase].current;
          power_total += teleinfo_phase[phase].sinsts;
        }

        // loop to update current and power
        for (phase = 0; phase < teleinfo_contract.phase; phase++)
        {
          // calculate phase apparent power
          if (abs (teleinfo_counter.papp - power_total) < 10) teleinfo_phase[phase].papp = teleinfo_phase[phase].sinsts;
          else if (current_total == 0) teleinfo_phase[phase].papp = teleinfo_counter.papp / teleinfo_contract.phase;
          else teleinfo_phase[phase].papp = (teleinfo_counter.papp * teleinfo_phase[phase].current) / current_total;

          // calculate active power
          active_power = (float)teleinfo_phase[phase].papp * teleinfo_phase[phase].cos_phi;
          teleinfo_phase[phase].pact = (uint16_t)active_power; 

          // update phase active power and instant current
          Energy.voltage[phase]        = (float)teleinfo_phase[phase].voltage;
          Energy.apparent_power[phase] = (float)teleinfo_phase[phase].papp;
          Energy.current[phase]        = Energy.apparent_power[phase] / Energy.voltage[phase];
          Energy.active_power[phase]   = active_power;

          // detect power overload
          if (teleinfo_phase[phase].papp > teleinfo_contract.ssousc) teleinfo_message.overload = true;

          // detect more than 1% power change
          if (abs (teleinfo_phase[phase].papp_last - teleinfo_phase[phase].papp) > (teleinfo_contract.ssousc / 100))
          {
            teleinfo_message.percent = true;
            teleinfo_phase[phase].papp_last = teleinfo_phase[phase].papp;
          }
        } 

        // if not already done, determine number of indexes according to contract
        if (teleinfo_counter.nb_index == TELEINFO_INDEX_MAX)
        {
          index = GetCommandCode (str_result, sizeof (str_result), teleinfo_contract.str_period, kTeleinfoPeriod);
          if ((index >= 0) && (index < TELEINFO_INDEX_MAX)) teleinfo_counter.nb_index = ARR_TELEINFO_PERIOD_INDEX[index];
        }

        // update total energy counter
        counter_total = 0;
        for (index = 0; index < teleinfo_counter.nb_index; index ++) counter_total += teleinfo_counter.index[index];

        // if total power counter has changed
        if (teleinfo_counter.total != counter_total)
        {
          // if first reading, update energy totals
          if (teleinfo_counter.total == 0) EnergyUpdateTotal((float) counter_total, false);

          // update totals
          teleinfo_counter.total = counter_total;

          // update cos phi calculation
          TeleinfoUpdateCosPhi ();
        }

        // declare received message
        teleinfo_message.received = true;
        break;

      // ---------------------------
      // 0x04 : Reset of message 
      // ---------------------------
      case 4:
        // increment reset counter and reset message line index
        teleinfo_message.nb_reset++;
        teleinfo_message.line_index = 0;
        break;

      // ------------------------
      // 0x0A : Beginning of line
      // ------------------------
      case 10:
        // init current line
        teleinfo_line.separator   = 0;
        strcpy (teleinfo_line.str_text, "");
        break;

      // ------------------
      // 0x0D : End of line
      // ------------------
      case 13:
        // increment counter
        teleinfo_message.nb_data++;

        // drop first message
        if (teleinfo_message.nb_data > 1)
        {
          // if checksum is ok, handle the line
          checksum = TeleinfoCalculateChecksum (teleinfo_line.str_text);
          if (checksum != 0)
          {
            // extract etiquette and donnee from current line
            TeleinfoSplitLine (teleinfo_line.str_text, teleinfo_line.separator, str_etiquette, sizeof (str_etiquette), str_result, sizeof (str_result));

            // cleanup data part to remove leading, trailing and multiple SPACE
            TeleinfoCleanupString (str_result, str_donnee, sizeof (str_donnee));

            // handle specific etiquette index
            index = GetCommandCode (str_result, sizeof (str_result), str_etiquette, kTeleinfoEtiquetteName);
            switch (index)
            {
              // contract reference
              case TIC_ADCO:
                teleinfo_message.line_index = 0;
                teleinfo_contract.mode = TIC_MODE_HISTORIC;
                strlcpy (teleinfo_contract.str_id, str_donnee, sizeof (teleinfo_contract.str_id));
                break;
              case TIC_ADSC:
                teleinfo_message.line_index = 0;
                teleinfo_contract.mode = TIC_MODE_STANDARD;
                strlcpy (teleinfo_contract.str_id, str_donnee, sizeof (teleinfo_contract.str_id));
                break;
              // period name
              case TIC_PTEC:
              case TIC_LTARF:
              case TIC_PTCOUR:
                strlcpy (teleinfo_contract.str_period, str_donnee, sizeof(teleinfo_contract.str_period));
                break;
              // instant current
              case TIC_EAIT:
              case TIC_IINST:
              case TIC_IRMS1:
              case TIC_IINST1:
                teleinfo_phase[0].current = atoi (str_donnee);
                break;
              case TIC_IRMS2:
              case TIC_IINST2:
                teleinfo_phase[1].current = atoi (str_donnee);
                break;
              case TIC_IRMS3:
              case TIC_IINST3:
                teleinfo_phase[2].current = atoi (str_donnee); 
                teleinfo_contract.phase = 3; 
                break;
              // instant power
              case TIC_PAPP:
              case TIC_SINSTS:
                teleinfo_counter.papp = atoi (str_donnee);
                break;
              case TIC_SINSTS1:
                teleinfo_phase[0].sinsts = atoi (str_donnee);
                break;
              case TIC_SINSTS2:
                teleinfo_phase[1].sinsts = atoi (str_donnee);
                break;
              case TIC_SINSTS3:
                teleinfo_phase[2].sinsts = atoi (str_donnee);
                teleinfo_contract.phase = 3; 
                break;
              // voltage
              case TIC_URMS1:
                teleinfo_phase[0].voltage  = atoi (str_donnee);
                teleinfo_phase[0].volt_set = true;
                break;
              case TIC_UMOY1:
                if (!teleinfo_phase[0].volt_set) teleinfo_phase[0].voltage = atoi (str_donnee);
                break;
              case TIC_URMS2:
                teleinfo_phase[1].voltage = atoi (str_donnee);
                teleinfo_phase[1].volt_set = true;
                break;
              case TIC_UMOY2:
                if (!teleinfo_phase[1].volt_set) teleinfo_phase[1].voltage = atoi (str_donnee);
                break;
              case TIC_URMS3:
                teleinfo_phase[2].voltage = atoi (str_donnee);
                teleinfo_phase[2].volt_set = true;
                teleinfo_contract.phase = 3; 
                break;
              case TIC_UMOY3:
                if (!teleinfo_phase[2].volt_set) teleinfo_phase[2].voltage = atoi (str_donnee);
                teleinfo_contract.phase = 3; 
                break;
              // contract max current or power
              case TIC_ISOUSC:
                teleinfo_contract.isousc = atoi (str_donnee);
                break;
              case TIC_PS:
                teleinfo_contract.ssousc = atoi (str_donnee);
                break;
              // contract maximum power
              case TIC_PREF:
              case TIC_PCOUP:
                teleinfo_contract.ssousc = 1000 * atoi (str_donnee) / teleinfo_contract.phase;
                teleinfo_contract.isousc = teleinfo_contract.ssousc / TELEINFO_VOLTAGE_REF;
                break;
              // index suivant type de contrat
              case TIC_BASE:
              case TIC_HCHC:
              case TIC_EJPHN:
              case TIC_BBRHCJB:
              case TIC_EASF01:
                teleinfo_counter.index[0] = atoi (str_donnee);
                break;
              case TIC_HCHP:
              case TIC_EJPHPM:
              case TIC_BBRHPJB:
              case TIC_EASF02:
                teleinfo_counter.index[1] = atoi (str_donnee);
                break;
              case TIC_BBRHCJW:
              case TIC_EASF03:
                teleinfo_counter.index[2] = atoi (str_donnee);
                break;
              case TIC_BBRHPJW:
              case TIC_EASF04:
                teleinfo_counter.index[3] = atoi (str_donnee);
                break;
              case TIC_BBRHCJR:
              case TIC_EASF05:
                teleinfo_counter.index[4] = atoi (str_donnee);
                break;
              case TIC_BBRHPJR:
              case TIC_EASF06:
                teleinfo_counter.index[5] = atoi (str_donnee);
                break;
              case TIC_EASF07:
                teleinfo_counter.index[6] = atoi (str_donnee);
                break;
              case TIC_EASF08:
                teleinfo_counter.index[7] = atoi (str_donnee);
                break;
              case TIC_EASF09:
                teleinfo_counter.index[8] = atoi (str_donnee);
                break;
              case TIC_EASF10:
                teleinfo_counter.index[9] = atoi (str_donnee);
                break;
              // overload flags
              case TIC_ADPS:
              case TIC_ADIR1:
              case TIC_ADIR2:
              case TIC_ADIR3:
                teleinfo_message.overload = true;
                break;
            }

            // if maximum number of lines no reached
            if (teleinfo_message.line_index < TELEINFO_LINE_MAX)
            {
              // get index and size
              index = teleinfo_message.line_index;
              max_size = sizeof (teleinfo_message.line[index].str_donnee);

              // save new line
              strlcpy (teleinfo_message.line[index].str_etiquette, str_etiquette, sizeof(teleinfo_message.line[index].str_etiquette));
              if (strlen (str_donnee) < max_size) strlcpy (teleinfo_message.line[index].str_donnee, str_donnee, max_size);
              else
              {
                strlcpy (teleinfo_message.line[index].str_donnee, str_donnee, max_size - 5);
                strlcat (teleinfo_message.line[index].str_donnee, " ...", max_size);
              }
              teleinfo_message.line[index].checksum = checksum;

              // increment line index
              teleinfo_message.line_index++;
            }
          }
        }
        break;

      // ---------------------------
      // \t or SPACE : new line part
      // ---------------------------
      case 9:
      case ' ':
        if (teleinfo_line.separator == 0) teleinfo_line.separator = (char)recv_serial;
        if (strlen (teleinfo_line.str_text) < TELEINFO_STRING_LINE_MAX - 1) strncat (teleinfo_line.str_text, &recv_serial, 1);
        break;
        
      // add other caracters to current line
      default:
        if (strlen (teleinfo_line.str_text) < TELEINFO_STRING_LINE_MAX - 1) strncat (teleinfo_line.str_text, &recv_serial, 1);
        break;
    }
  }
}

// if needed, update graph display values and check if data should be published
void TeleinfoEverySecond ()
{
  bool     publish_tic  = false;
  bool     publish_data = false;
  int      publish_policy;
  int      period, phase;
  uint16_t current_volt;
  uint16_t current_papp;

#ifdef SIMULATION
  Energy.phase_count = 3;
  teleinfo_contract.phase = 3;
  teleinfo_contract.ssousc = 6000;
  teleinfo_phase[0].papp = 1000 + random (-1000, 1000);
  teleinfo_phase[1].papp = 3000 + random (-500, 500);
  teleinfo_phase[2].papp = 4000 + random (-500, 500);
  teleinfo_phase[0].voltage = 230 + random (-5, 5);
  teleinfo_phase[1].voltage = 228 + random (-6, 6);
  teleinfo_phase[2].voltage = 235 + random (-6, 6);
#endif

  // increment energy totals storage counter
  teleinfo_counter.store ++;
  teleinfo_counter.store = teleinfo_counter.store % TELEINFO_STORE_PERIOD;

  // if energy totals should be stored
  if ((teleinfo_counter.store == 0) && (teleinfo_counter.total > 0)) EnergyUpdateTotal((float) teleinfo_counter.total, false);

  // loop thru the periods and the phases, to update apparent power to the max on the period
  for (period = 0; period < TELEINFO_PERIOD_MAX; period++)
  {
    // loop thru phases to update max value
    for (phase = 0; phase < teleinfo_contract.phase; phase++)
    {
      // get current voltage and apparent power
      current_volt = (uint16_t)teleinfo_phase[phase].voltage;
      current_papp = (uint16_t)teleinfo_phase[phase].papp;

      // update sum of cos phi (for average calculation)
      teleinfo_graph[period].cosphi_sum[phase] += teleinfo_phase[phase].cos_phi;

      // if within range, update phase apparent power
      if (current_papp != UINT16_MAX)
      {
        // add power to period total (for average calculation)
        teleinfo_graph[period].papp_sum[phase] += current_papp;

        // update low voltage level
        if (teleinfo_graph[period].volt_low[phase] == UINT16_MAX) teleinfo_graph[period].volt_low[phase] = current_volt;
        else if (current_volt < teleinfo_graph[period].volt_low[phase])  teleinfo_graph[period].volt_low[phase] = current_volt;

        // update high voltage level
        if (teleinfo_graph[period].volt_high[phase] == UINT16_MAX) teleinfo_graph[period].volt_high[phase] = current_volt;
        else if (current_volt > teleinfo_graph[period].volt_high[phase]) teleinfo_graph[period].volt_high[phase] = current_volt;

        // update high power level
        if (teleinfo_graph[period].papp_high[phase] == UINT16_MAX) teleinfo_graph[period].papp_high[phase] = current_papp;
        else if (current_papp > teleinfo_graph[period].papp_high[phase]) teleinfo_graph[period].papp_high[phase] = current_papp;
      } 
    }

    // increment graph period counter and update graph data if needed
    if (teleinfo_graph[period].counter == 0) TeleinfoUpdateGraphData (period);
    teleinfo_graph[period].counter ++;
    teleinfo_graph[period].counter = teleinfo_graph[period].counter % ARR_TELEINFO_PERIOD_SAMPLE[period];
  }

  // check if TIC should be published
  publish_policy = TeleinfoGetPolicyTIC ();
  if (teleinfo_message.overload && (publish_policy != TELEINFO_POLICY_NEVER))   publish_tic = true;
  if (teleinfo_message.received && (publish_policy == TELEINFO_POLICY_MESSAGE)) publish_tic = true;
  if (teleinfo_message.percent  && (publish_policy == TELEINFO_POLICY_PERCENT)) publish_tic = true;
  
  // check if Meter data should be published
  publish_policy = TeleinfoGetPolicyData ();
  if (teleinfo_message.overload && (publish_policy != TELEINFO_POLICY_NEVER))   publish_data = true;
  if (teleinfo_message.received && (publish_policy == TELEINFO_POLICY_MESSAGE)) publish_data = true;
  if (teleinfo_message.percent  && (publish_policy == TELEINFO_POLICY_PERCENT)) publish_data = true;

  // reset message flags
  teleinfo_message.overload = false;
  teleinfo_message.received = false;
  teleinfo_message.percent  = false;

  // if current or overload has been updated, publish teleinfo data
  if (publish_tic || publish_data) TeleinfoShowJSON (false, publish_tic, publish_data);
}

// update graph history data
void TeleinfoUpdateGraphData (uint8_t period)
{
  int index, phase;

  // if period is within the range
  if (period < TELEINFO_PERIOD_MAX)
  {
    // set indexed graph values with current values
    index = teleinfo_graph[period].index;
    for (phase = 0; phase < teleinfo_contract.phase; phase++)
    {
      TeleinfoSaveGraphPower (period, phase, index);
      TeleinfoSaveGraphVoltage (period, phase, index);
      TeleinfoSaveGraphCosPhi (period, phase, index);
    }

    // increase data index in the graph
    teleinfo_graph[period].index++;
    teleinfo_graph[period].index = teleinfo_graph[period].index % TELEINFO_GRAPH_SAMPLE;

    // set update flag
    teleinfo_graph[period].updated = true;
  }
}

// Generate JSON with TIC informations
//  "TIC":{ "ADCO":"1234567890","ISOUSC":30,"SSOUSC":6000,"SINSTS":2567,"SINSTS1":1243,"IINST1":14.7,"ADIR1":0,"SINSTS2":290,"IINST2":4.4, ... }
void TeleinfoGenerateTicJSON ()
{
  int index;

  // start of TIC section
  ResponseAppend_P (PSTR (",\"%s\":{"), TELEINFO_JSON_TIC);

  // loop thru TIC message lines
  for (index = 0; index < TELEINFO_LINE_MAX; index ++)
    if (teleinfo_message.line[index].checksum != 0)
    {
      // add current line
      if (index != 0) ResponseAppend_P (PSTR (","));
      ResponseAppend_P (PSTR ("\"%s\":\"%s\""), teleinfo_message.line[index].str_etiquette, teleinfo_message.line[index].str_donnee);
    }

  // end of TIC section
  ResponseAppend_P (PSTR ("}"));
}

// Generate JSON with Meter informations
//  "METER":{ "PHASE":3,"PREF":6000,"IREF":30,"U1":233,"I1":10,"P1":2020,"U2":231,"I2":5,"P1":990,"U3":230,"I3":2,"P3":410}
void TeleinfoGenerateMeterJSON ()
{
  int   phase, value;
  float current;
  char str_text[TELEINFO_STRING_VALUE_MAX];

  // start METER section
  ResponseAppend_P (PSTR (",\"%s\":{"), TELEINFO_JSON_METER);
  ResponseAppend_P (PSTR ("\"%s\":%d"), TELEINFO_JSON_PHASE, teleinfo_contract.phase);
  ResponseAppend_P (PSTR (",\"%s\":%d"), TELEINFO_JSON_PREF, teleinfo_contract.ssousc);
  ResponseAppend_P (PSTR (",\"%s\":%d"), TELEINFO_JSON_IREF, teleinfo_contract.isousc);

  // loop to update voltage, current and power
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    value = phase + 1;

    // U, Papp and Pact
    ResponseAppend_P (PSTR (",\"U%d\":%d"), value, teleinfo_phase[phase].voltage);
    ResponseAppend_P (PSTR (",\"P%d\":%d"), value, teleinfo_phase[phase].papp);
    ResponseAppend_P (PSTR (",\"W%d\":%d"), value, teleinfo_phase[phase].pact);

    // I
    current = teleinfo_phase[phase].papp / teleinfo_phase[phase].voltage;
    ext_snprintf_P (str_text, sizeof(str_text), PSTR ("%1_f"), &current);
    ResponseAppend_P (PSTR (",\"I%d\":%s"), value, str_text);

    // cos phi
    ext_snprintf_P (str_text, sizeof(str_text), PSTR ("%2_f"), &teleinfo_phase[phase].cos_phi);
    ResponseAppend_P (PSTR (",\"C%d\":%s"), value, str_text);
  } 

  // end of Meter section
  ResponseAppend_P (PSTR ("}"));
}

// Show JSON status (for MQTT)
void TeleinfoShowJSON (bool append, bool publish_tic, bool publish_data)
{
  // if not in append mode, start with current time   {"Time":"xxxxxxxx"
  if (!append) Response_P (PSTR ("{\"%s\":\"%s\""), D_JSON_TIME, GetDateAndTime (DT_LOCAL).c_str ());

  // if Meter section should be published
  if (publish_data) TeleinfoGenerateMeterJSON ();

  // if TIC section should be published
  if (publish_tic) TeleinfoGenerateTicJSON ();

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
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s %s</button></form></p>\n"), D_TELEINFO_PAGE_TIC, D_TELEINFO_TIC, D_TELEINFO_MESSAGE);

  // Teleinfo graph page button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s %s</button></form></p>\n"), D_TELEINFO_PAGE_GRAPH, D_TELEINFO_TIC, D_TELEINFO_GRAPH);
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
  int    index, phase, period;
  float  percentage;
  char   str_text[TELEINFO_STRING_DISPLAY_MAX];
  char   str_header[3][TELEINFO_STRING_HEADER_MAX];
  char   str_display[3][TELEINFO_STRING_DISPLAY_MAX];
  char   str_lf[2][8];

  // phase graph bar
  if (teleinfo_contract.ssousc > 0)
    for (phase = 0; phase < teleinfo_contract.phase; phase++)
    {
      // calculate phase percentage
      percentage  = (float)teleinfo_phase[phase].papp * 100 / (float)teleinfo_contract.ssousc;
      ext_snprintf_P (str_text, sizeof(str_text), PSTR ("%0_f"), &percentage);

      // display graph bar
      WSContentSend_PD (TELEINFO_HTML_BAR, ARR_TELEINFO_PHASE_COLOR[phase], str_text, str_text);
    }

  // init strings
  for (index = 0; index < 2; index ++) strcpy (str_lf[index], "");
  for (index = 0; index < 3; index ++) strcpy (str_header[index], "");
  for (index = 0; index < 3; index ++) strcpy (str_display[index], "");
  
  // Teleinfo mode
  strcpy (str_header[0], D_TELEINFO_MODE);
  GetTextIndexed (str_text, sizeof (str_text), teleinfo_contract.mode, kTeleinfoModeName);
  strlcpy (str_display[0], str_text, sizeof (str_display[0]));

  // Teleinfo tarif period (with conversion if historic short version)
  strcpy (str_lf[0], "<br>");
  strcpy (str_header[1], D_TELEINFO_PERIOD);
  period = GetCommandCode (str_text, TELEINFO_STRING_DISPLAY_MAX, teleinfo_contract.str_period, kTeleinfoPeriod);
  if (period != -1) GetTextIndexed (str_display[1], TELEINFO_STRING_DISPLAY_MAX, period, kTeleinfoPeriodName);
  else strlcpy (str_display[1], teleinfo_contract.str_period, sizeof (str_display[1]));

  // Teleinfo contract power
  strcpy (str_text, "");
  strcpy (str_lf[1], "<br>");
  strcpy (str_header[2], D_TELEINFO_CONTRACT);
  if (teleinfo_contract.phase > 1) sprintf (str_text, "%d x ", teleinfo_contract.phase);
  if (teleinfo_contract.ssousc > 0) sprintf (str_display[2], "%s%d kVA", str_text, teleinfo_contract.ssousc / 1000);

  // display data
  WSContentSend_PD (PSTR ("{s}%s%s%s%s%s{m}%s%s%s%s%s{e}"), str_header[0], str_lf[0], str_header[1], str_lf[1], str_header[2], str_display[0], str_lf[0], str_display[1], str_lf[1], str_display[2]);

  // init strings
  for (index = 0; index < 2; index ++) strcpy (str_lf[index], "");
  for (index = 0; index < 3; index ++) strcpy (str_header[index], "");
  for (index = 0; index < 3; index ++) strcpy (str_display[index], "");

  // get number of TIC messages received
  strcpy (str_header[0], D_TELEINFO_MESSAGE);
  sprintf (str_display[0], "%d", teleinfo_message.nb_message);

  // get TIC checksum errors
  if ((teleinfo_message.nb_data > 0) && ( teleinfo_message.nb_error > 0 ))
  {
    // calculate error percentage
    if (teleinfo_message.nb_data == 0) percentage = 0;
    else percentage = (float)teleinfo_message.nb_error * 100 / teleinfo_message.nb_data;

    // append data
    strcpy (str_lf[0], "<br>");
    strcpy (str_header[1], D_TELEINFO_ERROR);
    ext_snprintf_P (str_text, sizeof(str_text), PSTR ("%2_f"), &percentage);
    snprintf (str_display[1], sizeof (str_display[1]), "%d <small>(%s%%)</small>", teleinfo_message.nb_error, str_text);
  }

  // get TIC reset
  if (teleinfo_message.nb_reset > 0)
  {
    strcpy (str_lf[1], "<br>");
    strcpy (str_header[2], D_TELEINFO_RESET);
    sprintf (str_display[2], "%d", teleinfo_message.nb_reset);
  }

  // display data
  WSContentSend_PD (PSTR ("{s}%s%s%s%s%s{m}%s%s%s%s%s{e}"), str_header[0], str_lf[0], str_header[1], str_lf[1], str_header[2], str_display[0], str_lf[0], str_display[1], str_lf[1], str_display[2]);
}

// Teleinfo web page
void TeleinfoWebPageConfig ()
{
  int  index, tic_baud, rate_baud, rate_mode, policy_mode;
  char str_text[16];
  char str_select[16];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess ()) return;

  // if parameter 'tic' is provided
  if (Webserver->hasArg (D_CMND_TELEINFO_CFG_TIC))
  {
    // set TIC messages diffusion policy
    WebGetArg (D_CMND_TELEINFO_CFG_TIC, str_text, sizeof (str_text));
    policy_mode = atol (str_text);
    TeleinfoSetPolicyTIC (policy_mode);
  }

  // if parameter 'data' is provided
  if (Webserver->hasArg (D_CMND_TELEINFO_CFG_DATA))
  {
    // set data messages diffusion policy
    WebGetArg (D_CMND_TELEINFO_CFG_DATA, str_text, sizeof (str_text));
    policy_mode = atol (str_text);
    TeleinfoSetPolicyData (policy_mode);
  }

  // get teleinfo serial rate according to 'rate' parameter
  if (Webserver->hasArg (D_CMND_TELEINFO_CFG_RATE))
  {
    // get current and new teleinfo rate
    WebGetArg (D_CMND_TELEINFO_CFG_RATE, str_text, sizeof (str_text));
    rate_baud = atoi (str_text);
    tic_baud = (int)TeleinfoGetBaudRate ();

    // if different, update and restart
    if (tic_baud != rate_baud)
    {
      TeleinfoSetBaudRate ((uint16_t) atoi (str_text));
      WebRestart (1);
    }
  }

  // beginning of form
  WSContentStart_P (D_TELEINFO_CONFIG);
  WSContentSendStyle ();
  WSContentSend_P (TELEINFO_FORM_START, D_TELEINFO_PAGE_CONFIG);

  // speed selection form
  tic_baud = (int)TeleinfoGetBaudRate ();
  WSContentSend_P (TELEINFO_FIELD_START, TELEINFO_TITLE_RATE);
  for (index = 0; index < 6; index++)
  {
    rate_baud = ARR_TELEINFO_RATE[index];
    if (rate_baud == tic_baud) strcpy (str_select, "checked"); else strcpy (str_select, "");
    if (rate_baud == 0) strcpy (str_text, TELEINFO_DISABLED); else itoa (rate_baud, str_text, 10);
    WSContentSend_P (TELEINFO_INPUT_RADIO, D_CMND_TELEINFO_CFG_RATE, ARR_TELEINFO_RATE[index], str_select, str_text);
  }
  WSContentSend_P (TELEINFO_FIELD_STOP);

  // teleinfo meter tic diffusion selection
  policy_mode = TeleinfoGetPolicyTIC ();
  WSContentSend_P (TELEINFO_FIELD_START, TELEINFO_TITLE_TIC);
  for (index = 0; index < TELEINFO_POLICY_MAX; index++)
  {
    if (index == policy_mode) strcpy (str_select, "checked"); else strcpy (str_select, "");
    WSContentSend_P (TELEINFO_INPUT_RADIO, D_CMND_TELEINFO_CFG_TIC, index, str_select, ARR_TELEINFO_CFG_LABEL[index]);
  }
  WSContentSend_P (TELEINFO_FIELD_STOP);

  // teleinfo meter data diffusion selection
  policy_mode = TeleinfoGetPolicyData ();
  WSContentSend_P (TELEINFO_FIELD_START, TELEINFO_TITLE_DATA);
  for (index = 0; index < TELEINFO_POLICY_MAX; index++)
  {
    if (index == policy_mode) strcpy (str_select, "checked"); else strcpy (str_select, "");
    WSContentSend_P (TELEINFO_INPUT_RADIO, D_CMND_TELEINFO_CFG_DATA, index, str_select, ARR_TELEINFO_CFG_LABEL[index]);
  }
  WSContentSend_P (TELEINFO_FIELD_STOP);

  // save button
  WSContentSend_P (PSTR ("<button name='save' type='submit' class='button bgrn'>%s</button>"), D_SAVE);
  WSContentSend_P (TELEINFO_FORM_STOP);

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

// JSON data page
void TeleinfoWebJsonData ()
{
  bool first_value;
  int  index, phase,index_array;
  int  graph_period = TELEINFO_PERIOD_LIVE;
  long power, voltage;
  char str_argument[TELEINFO_STRING_DISPLAY_MAX];

  // check graph period to be displayed
  if (Webserver->hasArg (D_CMND_TELEINFO_PERIOD))
  {
    // set graph period
    WebGetArg (D_CMND_TELEINFO_PERIOD, str_argument, sizeof (str_argument));
    graph_period = atoi (str_argument);
    if ((graph_period < 0) || (graph_period >= TELEINFO_PERIOD_MAX)) graph_period = TELEINFO_PERIOD_LIVE;
  }
  GetTextIndexed (str_argument, sizeof (str_argument), graph_period, kTeleinfoGraphPeriod);

  // start of data page
  WSContentBegin (200, CT_HTML);
  WSContentSend_P (PSTR ("{\"%s\":\"%s\""), TELEINFO_JSON_PERIOD, str_argument);
  WSContentSend_P (PSTR (",\"%s\":%d"),     TELEINFO_JSON_PHASE,  teleinfo_contract.phase);
  WSContentSend_P (PSTR (",\"%s\":\"%s\""), TELEINFO_JSON_ID,     teleinfo_contract.str_id);
  WSContentSend_P (PSTR (",\"%s\":%d"),     TELEINFO_JSON_PREF,   teleinfo_contract.ssousc);
  WSContentSend_P (PSTR (",\"%s\":%d"),     TELEINFO_JSON_IREF,   teleinfo_contract.isousc);

  // loop thru phasis
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    // loop for the apparent power array
    WSContentSend_P (PSTR (",\"P%d\":["), phase + 1);
    for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
    {
      // get target power array position and add value if defined
      index_array = (teleinfo_graph[graph_period].index + index) % TELEINFO_GRAPH_SAMPLE;
      power = TeleinfoGetGraphPower (graph_period, phase, index_array);
      if (power == UINT16_MAX) power = 0;

      // add value to JSON array
      if (index == 0) WSContentSend_P (PSTR ("%d"), power);
      else WSContentSend_P (PSTR (",%d"), power);
    }
    WSContentSend_P (PSTR ("]"));

    // if voltage is available
    if (Energy.voltage_available == true)
    {
      // loop for the minimum voltage
      WSContentSend_P (PSTR (",\"U%d\":["), phase + 1);
      for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
      {
        // get target power array position and add value if defined
        index_array = (teleinfo_graph[graph_period].index + index) % TELEINFO_GRAPH_SAMPLE;
        voltage = TeleinfoGetGraphVoltage (graph_period, phase, index_array);
        if (voltage == UINT16_MAX) voltage = 0;

        // add value to JSON array
        if (index == 0) WSContentSend_P (PSTR ("%d"), voltage);
        else WSContentSend_P (PSTR(",%d"), voltage);
      }
      WSContentSend_P (PSTR ("]"));
    }
  }

  // end of page
  WSContentSend_P (PSTR ("}"));
  WSContentEnd ();
}

// TIC raw message data
void TeleinfoWebTicUpdate ()
{
  int index;

  // start of data page
  WSContentBegin (200, CT_PLAIN);

  // send line number
  WSContentSend_P ("COUNTER|%d\n", teleinfo_message.nb_message);

  // loop thru TIC message lines to publish if defined
  for (index = 0; index < teleinfo_message.line_max; index ++)
    WSContentSend_P (PSTR ("%s|%s|%c\n"), teleinfo_message.line[index].str_etiquette, teleinfo_message.line[index].str_donnee, teleinfo_message.line[index].checksum);

  // end of data page
  WSContentEnd ();
}

// TIC message page
void TeleinfoWebPageTic ()
{
  int  index;
  char str_text[TELEINFO_STRING_DISPLAY_MAX];

  // beginning of form without authentification
  sprintf (str_text, "%s %s", D_TELEINFO_TIC, D_TELEINFO_MESSAGE);
  WSContentStart_P (str_text, false);
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
  WSContentSend_P (PSTR ("<div><span id='count'>%d</span></div>\n"), teleinfo_message.nb_message);

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
  int    graph_period = TELEINFO_PERIOD_LIVE;
  int    index, phase;
  char   str_text[TELEINFO_STRING_VALUE_MAX];

  // check graph period to be displayed
  if (Webserver->hasArg (D_CMND_TELEINFO_PERIOD))
  {
    // set graph period
    WebGetArg (D_CMND_TELEINFO_PERIOD, str_text, sizeof (str_text));
    graph_period = atoi (str_text);
    if ((graph_period < 0) || (graph_period >= TELEINFO_PERIOD_MAX)) graph_period = TELEINFO_PERIOD_LIVE;
  }

  // start of data page
  WSContentBegin (200, CT_PLAIN);

  // chech for graph update
  WSContentSend_P ("%d\n", teleinfo_graph[graph_period].updated);

  // check power and power difference for each phase
  for (phase = 0; phase < teleinfo_contract.phase; phase++) WSContentSend_P ("%d;%d\n", teleinfo_phase[phase].papp, teleinfo_phase[phase].voltage);

  // end of data page
  WSContentEnd ();

  // reset update flags
  teleinfo_graph[graph_period].updated = 0;
}

// Apparent power graph curve
void TeleinfoWebGraphData ()
{
  bool     graph_valid, phase_display;
  int      index, phase, index_array;
  int      graph_period, graph_display, graph_height;  
  long     graph_left, graph_right, graph_width;  
  long     unit_width, shift_unit, shift_width;  
  long     graph_value, graph_delta, graph_x, graph_y;  
  TIME_T   current_dst;
  uint32_t current_time;
  float    font_size;
  char     str_data[16];
  char     str_phase[8];

  // get numerical argument values
  graph_period  = TeleinfoWebGetArgValue (D_CMND_TELEINFO_PERIOD, TELEINFO_PERIOD_LIVE,   0,   TELEINFO_PERIOD_MAX - 1);
  graph_display = TeleinfoWebGetArgValue (D_CMND_TELEINFO_DATA,   TELEINFO_DISPLAY_POWER, 0,   TELEINFO_DISPLAY_MAX - 1);
  graph_height  = TeleinfoWebGetArgValue (D_CMND_TELEINFO_HEIGHT, TELEINFO_GRAPH_HEIGHT,  100, INT_MAX);

  // set font size
  font_size = (float)graph_height / TELEINFO_GRAPH_HEIGHT * 1.5;

  // check phase display argument
  if (Webserver->hasArg (D_CMND_TELEINFO_PHASE))
  {
    WebGetArg (D_CMND_TELEINFO_PHASE, str_data, sizeof (str_data));
    strncpy (str_phase, str_data, 7);
    str_phase[7] = 0;
  }
  for (phase = strlen (str_phase); phase < teleinfo_contract.phase; phase++) strcat (str_phase, "1");

  // boundaries of SVG graph
  graph_left  = TELEINFO_GRAPH_PERCENT_START * TELEINFO_GRAPH_WIDTH / 100;
  graph_right = TELEINFO_GRAPH_PERCENT_STOP * TELEINFO_GRAPH_WIDTH / 100;
  graph_width = graph_right - graph_left;

  // start of SVG graph
  WSContentBegin (200, CT_HTML);
  WSContentSend_P (PSTR ("<svg viewBox='%d %d %d %d'>\n"), 0, 0, TELEINFO_GRAPH_WIDTH, graph_height);

  // SVG style 
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));
  WSContentSend_P (PSTR ("polyline {fill:none;stroke-width:2;}\n"));
  for (phase = 0; phase < teleinfo_contract.phase; phase++) 
    WSContentSend_P (PSTR ("polyline.ph%d {stroke:%s;}\n"), phase, ARR_TELEINFO_PHASE_COLOR[phase]);
  WSContentSend_P (PSTR ("line.time {stroke:white;stroke-width:1;}\n"));
  ext_snprintf_P (str_data, sizeof(str_data), PSTR ("%2_f"), &font_size);
  WSContentSend_P (PSTR ("text.time {font-size:%srem;fill:grey;}\n"), str_data);
  WSContentSend_P (PSTR ("</style>\n"));

  // -----------------
  //   data curves
  // -----------------

  for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    // if phase graph should be displayed
    phase_display = (str_phase[phase] != '0');
    if (phase_display)
    {
      // start of polyline
      WSContentSend_P (PSTR ("<polyline class='ph%d' points='"), phase);

      // loop for the apparent power graph
      for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
      {
        // get current array index
        index_array = (index + teleinfo_graph[graph_period].index) % TELEINFO_GRAPH_SAMPLE;

        // set curve value according to displayed data
        switch (graph_display) 
        {
          case TELEINFO_DISPLAY_POWER:
            // if power is defined, calculate graph y position
            graph_value = (long)TeleinfoGetGraphPower (graph_period, phase, index_array);
            graph_valid = ((graph_value != UINT16_MAX) && (teleinfo_graph[graph_period].pmax != 0));
            if (graph_valid) graph_y = graph_height - (graph_value * graph_height / teleinfo_graph[graph_period].pmax);
            break;

          case TELEINFO_DISPLAY_VOLTAGE:
            // if voltage is defined, calculate graph y position
            graph_value = TeleinfoGetGraphVoltage (graph_period, phase, index_array);
            graph_valid = (graph_value != UINT16_MAX);
            if (graph_valid)
            {
              // calculate graph value as delta from min & max range
              graph_delta = teleinfo_graph[graph_period].vmax - teleinfo_graph[graph_period].vmin;
              graph_value = graph_value - teleinfo_graph[graph_period].vmin;

              // calculate graph y position
              graph_y = graph_height;
              if (graph_delta != 0) graph_y -= (graph_value * graph_height / graph_delta);
            }
            break;
          
          case TELEINFO_DISPLAY_COSPHI:
            // if power is defined, calculate graph y position
            graph_value = (uint8_t)TeleinfoGetGraphCosPhi (graph_period, phase, index_array);
            graph_valid = (graph_value != UINT8_MAX);
            if (graph_valid) graph_y = graph_height - (graph_value * graph_height / 100);
            break;
            
          default:
            graph_valid = false;
            break;
        }

        // display current point
        if (graph_valid)
        {
          // calculate x position and display value
          graph_x = graph_left + (graph_width * index / TELEINFO_GRAPH_SAMPLE);
          WSContentSend_P (PSTR("%d,%d "), graph_x, graph_y);
        }
      }

      // end of polyline
      WSContentSend_P (PSTR("'/>\n"));
    }
  }

  // ------------
  //     Time
  // ------------

  // get current time
  current_time = LocalTime ();
  BreakTime (current_time, current_dst);

  // handle graph units according to period
  switch (graph_period) 
  {
    case TELEINFO_PERIOD_LIVE:
      // calculate horizontal shift
      unit_width  = graph_width / 6;
      shift_unit  = current_dst.minute % 5;
      shift_width = unit_width - (unit_width * shift_unit / 5) - (unit_width * current_dst.second / 300);

      // calculate first time displayed by substracting (5 * 5mn + shift) to current time
      current_time -= 1500;
      current_time -= (shift_unit * 60); 

      // display 5 mn separation lines with time
      for (index = 0; index < 6; index++)
      {
        // convert back to date and increase time of 5mn
        BreakTime (current_time, current_dst);
        current_time += 300;

        // display separation line and time
        graph_x = graph_left + shift_width + (index * unit_width);
        WSContentSend_P (PSTR ("<line class='time' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n"), graph_x, 49, graph_x, 51);
        WSContentSend_P (PSTR ("<text class='time' x='%d' y='%d%%'>%02dh%02d</text>\n"), graph_x - 35, 55, current_dst.hour, current_dst.minute);
      }
      break;

    case TELEINFO_PERIOD_DAY:
      // calculate horizontal shift
      unit_width  = graph_width / 6;
      shift_unit  = current_dst.hour % 4;
      shift_width = unit_width - (unit_width * shift_unit / 4) - (unit_width * current_dst.minute / 240);

      // calculate first time displayed by substracting (5 * 4h + shift) to current time
      current_time -= 72000;
      current_time -= (shift_unit * 3600); 

      // display 4 hours separation lines with hour
      for (index = 0; index < 6; index++)
      {
        // convert back to date and increase time of 4h
        BreakTime (current_time, current_dst);
        current_time += 14400;

        // display separation line and time
        graph_x = graph_left + shift_width + (index * unit_width);
        WSContentSend_P (PSTR ("<line class='time' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n"), graph_x, 49, graph_x, 51);
        WSContentSend_P (PSTR ("<text class='time' x='%d' y='%d%%'>%02dh</text>\n"), graph_x - 15, 55, current_dst.hour);
      }
      break;

    case TELEINFO_PERIOD_WEEK:
      // calculate horizontal shift
      unit_width = graph_width / 7;
      shift_width = unit_width - (unit_width * current_dst.hour / 24) - (unit_width * current_dst.minute / 1440);

      // display day lines with day name
      current_dst.day_of_week --;
      for (index = 0; index < 7; index++)
      {
        // calculate next week day
        current_dst.day_of_week ++;
        current_dst.day_of_week = current_dst.day_of_week % 7;

        // display month separation line and week day (first days or current day after 6pm)
        graph_x = graph_left + shift_width + (index * unit_width);
        WSContentSend_P (PSTR ("<line class='time' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n"), graph_x, 49, graph_x, 51);
        if ((index < 6) || (current_dst.hour >= 18)) WSContentSend_P (PSTR ("<text class='time' x='%d' y='%d%%'>%s</text>\n"), graph_x + 30, 53, arr_week_day[current_dst.day_of_week]);
      }
      break;

    case TELEINFO_PERIOD_YEAR:
      // calculate horizontal shift
      unit_width = graph_width / 12;
      shift_width = unit_width - (unit_width * current_dst.day_of_month / 30);

      // display month separation lines with month name
      for (index = 0; index < 12; index++)
      {
        // calculate next month value
        current_dst.month = (current_dst.month % 12);
        current_dst.month++;

        // convert back to date to get month name
        current_time = MakeTime (current_dst);
        BreakTime (current_time, current_dst);

        // display month separation line and month name (if previous month or current month after 20th)
        graph_x = graph_left + shift_width + (index * unit_width);
        WSContentSend_P (PSTR ("<line class='time' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n"), graph_x, 49, graph_x, 51);
        if ((index < 11) || (current_dst.day_of_month >= 24)) WSContentSend_P (PSTR ("<text class='time' x='%d' y='%d%%'>%s</text>\n"), graph_x + 12, 53, current_dst.name_of_month);
      }
      break;
  }

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentEnd ();
}

// Graph frame
void TeleinfoWebGraphFrame ()
{
  int      index, phase;
  int      graph_period, graph_display, graph_height;  
  int      graph_left, graph_right, graph_width;
  uint16_t value, value_min, value_max;
  float    font_size, unit, unit_min, unit_max;
  char     str_unit[4];
  char     str_size[8];
  char     arr_label[5][8];

  // get numerical argument values
  graph_period  = TeleinfoWebGetArgValue (D_CMND_TELEINFO_PERIOD, TELEINFO_PERIOD_LIVE,   0,   TELEINFO_PERIOD_MAX - 1);
  graph_display = TeleinfoWebGetArgValue (D_CMND_TELEINFO_DATA,   TELEINFO_DISPLAY_POWER, 0,   TELEINFO_DISPLAY_MAX - 1);
  graph_height  = TeleinfoWebGetArgValue (D_CMND_TELEINFO_HEIGHT, TELEINFO_GRAPH_HEIGHT,  100, INT_MAX);

  // set font size
  font_size = (float)graph_height / TELEINFO_GRAPH_HEIGHT * 2;

  // set labels according to type of data
  switch (graph_display) 
  {
    case TELEINFO_DISPLAY_POWER:
      // init power range
      strcpy (str_unit, "VA");
      if (teleinfo_contract.ssousc > 1000) teleinfo_graph[graph_period].pmax = (uint16_t)teleinfo_contract.ssousc;
      else teleinfo_graph[graph_period].pmax = 1000;

      // loop thru phasis and graph records to calculate max power
      for (phase = 0; phase < teleinfo_contract.phase; phase++)
        for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
        {
          // update max power during the period
          value = TeleinfoGetGraphPower (graph_period, phase, index);
          if ((value != UINT16_MAX) && (value > teleinfo_graph[graph_period].pmax )) teleinfo_graph[graph_period].pmax = value;
        }

      // set values label
      itoa (0, arr_label[0], 10);
      itoa (teleinfo_graph[graph_period].pmax / 4, arr_label[1], 10);
      itoa (teleinfo_graph[graph_period].pmax / 2, arr_label[2], 10);
      itoa (teleinfo_graph[graph_period].pmax * 3 / 4, arr_label[3], 10);
      itoa (teleinfo_graph[graph_period].pmax, arr_label[4], 10);
      break;

    case TELEINFO_DISPLAY_VOLTAGE:
      // init voltage range
      strcpy (str_unit, "V");
      teleinfo_graph[graph_period].vmin = TELEINFO_VOLTAGE_LOW;
      teleinfo_graph[graph_period].vmax = TELEINFO_VOLTAGE_HIGH;
      
      // loop thru phasis and graph records to calculate voltage range
      for (phase = 0; phase < teleinfo_contract.phase; phase++)
        for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
        {
          // update max power during the period
          value = TeleinfoGetGraphVoltage (graph_period, phase, index);
          if (value != UINT16_MAX)
          {
            if (value <= teleinfo_graph[graph_period].vmin ) teleinfo_graph[graph_period].vmin = value - 1;
            if (value >= teleinfo_graph[graph_period].vmax ) teleinfo_graph[graph_period].vmax = value + 1;
          } 
        }

      // calculate range values
      value_min = teleinfo_graph[graph_period].vmin;
      value_max = teleinfo_graph[graph_period].vmax;
      value     = value_max - value_min;

      // set values label
      itoa (value_min, arr_label[0], 10);
      itoa (value_min + value / 4, arr_label[1], 10);
      itoa (value_min + value / 2, arr_label[2], 10);
      itoa (value_min + value * 3 / 4, arr_label[3], 10);
      itoa (value_max, arr_label[4], 10);
      break;

    case TELEINFO_DISPLAY_COSPHI:
      // init cos phi range
      strcpy (str_unit, "");

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
  WSContentSend_P (PSTR ("<svg viewBox='%d %d %d %d'>\n"), 0, 0, TELEINFO_GRAPH_WIDTH, graph_height);

  // SVG style 
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));
  WSContentSend_P (PSTR ("rect {stroke:grey;fill:none;}\n"));
  WSContentSend_P (PSTR ("line.dash {stroke:grey;stroke-width:1;stroke-dasharray:8;}\n"));
  WSContentSend_P (PSTR ("line.time {stroke:white;stroke-width:1;}\n"));
  ext_snprintf_P (str_size, sizeof(str_size), PSTR ("%1_f"), &font_size);
  WSContentSend_P (PSTR ("text.power {font-size:%srem;stroke:white;fill:white;}\n"), str_size);
  WSContentSend_P (PSTR ("</style>\n"));

  // graph frame
  WSContentSend_P (PSTR ("<rect x='%d%%' y='%d%%' width='%d%%' height='%d%%' rx='10' />\n"), TELEINFO_GRAPH_PERCENT_START, 0, TELEINFO_GRAPH_PERCENT_STOP - TELEINFO_GRAPH_PERCENT_START, 100);

  // graph separation lines
  WSContentSend_P (TELEINFO_HTML_DASH, TELEINFO_GRAPH_PERCENT_START, 25, TELEINFO_GRAPH_PERCENT_STOP, 25);
  WSContentSend_P (TELEINFO_HTML_DASH, TELEINFO_GRAPH_PERCENT_START, 50, TELEINFO_GRAPH_PERCENT_STOP, 50);
  WSContentSend_P (TELEINFO_HTML_DASH, TELEINFO_GRAPH_PERCENT_START, 75, TELEINFO_GRAPH_PERCENT_STOP, 75);

  // units graduation
  WSContentSend_P (PSTR ("<text class='power' x='%d%%' y='%d%%'>%s</text>\n"), TELEINFO_GRAPH_PERCENT_STOP + 2, 4, str_unit);
  WSContentSend_P (TELEINFO_HTML_POWER, 1, 3,  arr_label[4]);
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
  bool phase_display;
  int  index, phase;
  int  graph_period, graph_display, graph_height, graph_bottom;  
  char str_data[16];
  char str_phase[16];
  char str_text[16];

  // get numerical argument values
  graph_period  = TeleinfoWebGetArgValue (D_CMND_TELEINFO_PERIOD, TELEINFO_PERIOD_LIVE,   0,   TELEINFO_PERIOD_MAX - 1);
  graph_display = TeleinfoWebGetArgValue (D_CMND_TELEINFO_DATA,   TELEINFO_DISPLAY_POWER, 0,   TELEINFO_DISPLAY_MAX - 1);
  graph_height  = TeleinfoWebGetArgValue (D_CMND_TELEINFO_HEIGHT, TELEINFO_GRAPH_HEIGHT,  100, INT_MAX);

  // calculate graph bottom padding
  graph_bottom = 68 * graph_height / TELEINFO_GRAPH_HEIGHT;

  // check phase display argument
  if (Webserver->hasArg (D_CMND_TELEINFO_PHASE))
  {
    WebGetArg (D_CMND_TELEINFO_PHASE, str_data, sizeof (str_data)); 
    strlcpy (str_phase, str_data, sizeof (str_phase)); 
  }
  for (phase = strlen (str_phase); phase < teleinfo_contract.phase; phase++) strcat (str_phase, "1");

  // beginning of form without authentification
  WSContentStart_P (D_TELEINFO_GRAPH, false);
  WSContentSend_P (PSTR ("</script>\n"));

  // page data refresh script
  WSContentSend_P (PSTR ("<script type='text/javascript'>\n"));
  WSContentSend_P (PSTR ("function update()\n"));
  WSContentSend_P (PSTR ("{\n"));
  WSContentSend_P (PSTR (" httpUpd=new XMLHttpRequest();\n"));
  WSContentSend_P (PSTR (" httpUpd.onreadystatechange=function()\n"));
  WSContentSend_P (PSTR (" {\n"));
  WSContentSend_P (PSTR ("  if (httpUpd.responseText.length>0)\n"));
  WSContentSend_P (PSTR ("  {\n"));
  WSContentSend_P (PSTR ("   arr_param=httpUpd.responseText.split('\\n');\n"));
  WSContentSend_P (PSTR ("   if (arr_param[0]==1)\n"));
  WSContentSend_P (PSTR ("   {\n"));
  WSContentSend_P (PSTR ("    str_random=Math.floor(Math.random()*100000);\n"));
  WSContentSend_P (PSTR ("    document.getElementById('data').data='%s?period=%d&data=%d&height=%d&phase=%s&rnd='+str_random;\n"), D_TELEINFO_PAGE_GRAPH_DATA, graph_period, graph_display, graph_height, str_phase);
  WSContentSend_P (PSTR ("   }\n"));
  WSContentSend_P (PSTR ("   num_param=arr_param.length;\n"));
  WSContentSend_P (PSTR ("   for (i=1;i<num_param;i++)\n"));
  WSContentSend_P (PSTR ("   {\n"));
  WSContentSend_P (PSTR ("    arr_value=arr_param[i].split(';');\n"));
  WSContentSend_P (PSTR ("    document.getElementById('p'+i).textContent=arr_value[0]+' VA';\n"));
  WSContentSend_P (PSTR ("    document.getElementById('v'+i).textContent=arr_value[1]+' V';\n"));
  WSContentSend_P (PSTR ("   }\n"));
  WSContentSend_P (PSTR ("  }\n"));
  WSContentSend_P (PSTR (" }\n"));
  WSContentSend_P (PSTR (" httpUpd.open('GET','%s?%s=%d',true);\n"), D_TELEINFO_PAGE_GRAPH_UPD, D_CMND_TELEINFO_PERIOD, graph_period);
  WSContentSend_P (PSTR (" httpUpd.send();\n"));
  WSContentSend_P (PSTR ("}\n"));
  WSContentSend_P (PSTR ("setInterval(function() {update();},5000);\n"));
  WSContentSend_P (PSTR ("</script>\n"));

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {margin:0.5rem auto;padding:0.1rem 0px;text-align:center;vertical-align:middle;}\n"));
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
  WSContentSend_P (PSTR ("div.choice a div {background:none;}\n"));
  WSContentSend_P (PSTR ("div.item {display:inline-block;padding:0.2rem auto;margin:1px;border:none;border-radius:4px;}\n"));
  WSContentSend_P (PSTR ("div.choice a div.item:hover {background:#aaa;}\n"));
  WSContentSend_P (PSTR ("div.period {width:70px;}\n"));
  WSContentSend_P (PSTR ("div.data {width:70px;}\n"));
  WSContentSend_P (PSTR ("div.size {width:30px;}\n"));
  WSContentSend_P (PSTR (".svg-container {position:relative;vertical-align:middle;overflow:hidden;width:100%%;max-width:%dpx;padding-bottom:%dvw;}\n"), TELEINFO_GRAPH_WIDTH, graph_bottom);
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
    phase_display = (str_text[phase] != '0');
    if (phase_display) str_text[phase] = '0'; else str_text[phase] = '1';
    WSContentSend_P (PSTR ("<a href='%s?period=%d&data=%d&height=%d&phase=%s'>"), D_TELEINFO_PAGE_GRAPH, graph_period, graph_display, graph_height, str_text);

    // display phase data
    if (phase_display) strcpy (str_text, ""); else strcpy (str_text, "disabled");
    WSContentSend_P (PSTR ("<div class='phase ph%d %s'>"), phase, str_text);
    WSContentSend_P (PSTR ("<span class='power' id='p%d'>%d VA</span>"), phase + 1, teleinfo_phase[phase].papp);
    if (Energy.voltage_available) WSContentSend_P (PSTR ("<br><span class='volt' id='v%d'>%d V</span>"), phase + 1, teleinfo_phase[phase].voltage);
    WSContentSend_P (PSTR ("</div></a>\n"));
  }
  WSContentSend_P (PSTR ("</div>\n"));

  // -----------------
  //      Period 
  // -----------------
  WSContentSend_P (PSTR ("<div>\n"));
  WSContentSend_P (PSTR ("<div class='choice'>\n"));
  for (index = 0; index < TELEINFO_PERIOD_MAX; index++)
  {
    // get period label
    GetTextIndexed (str_data, sizeof (str_data), index, kTeleinfoGraphPeriod);

    // set button according to active state
    if (graph_period != index) WSContentSend_P (PSTR ("<a href='%s?period=%d&data=%d&height=%d&phase=%s'>"), D_TELEINFO_PAGE_GRAPH, index, graph_display, graph_height, str_phase);
    WSContentSend_P (PSTR ("<div class='item period'>%s</div>"), str_data);
    if (graph_period != index) WSContentSend_P (PSTR ("</a>"));
    WSContentSend_P (PSTR ("\n"));
  }
  WSContentSend_P (PSTR ("</div>\n"));
  WSContentSend_P (PSTR ("</div>\n"));

  // start second line of parameters
  WSContentSend_P (PSTR ("<div>\n"));

  // -----------------
  //     Data type
  // -----------------
  WSContentSend_P (PSTR ("<div class='choice'>\n"));
  for (index = 0; index < TELEINFO_DISPLAY_MAX; index++)
  {
    GetTextIndexed (str_data, sizeof (str_data), index, kTeleinfoGraphDisplay);
    if (graph_display != index) WSContentSend_P (PSTR ("<a href='%s?period=%d&data=%d&height=%d&phase=%s'>"), D_TELEINFO_PAGE_GRAPH, graph_period, index, graph_height, str_phase);
    WSContentSend_P (PSTR ("<div class='item data'>%s</div>"), str_data);
    if (graph_display != index) WSContentSend_P (PSTR ("</a>"));
    WSContentSend_P (PSTR ("\n"));
  }
  WSContentSend_P (PSTR ("</div>\n"));

  // -----------------
  //      Height
  // -----------------
  WSContentSend_P (PSTR ("<div class='choice'>\n"));
  index = graph_height - TELEINFO_GRAPH_STEP;
  WSContentSend_P (PSTR ("<a href='%s?period=%d&data=%d&height=%d&phase=%s'><div class='item size'>-</div></a>"), D_TELEINFO_PAGE_GRAPH, graph_period, graph_display, index, str_phase);
  index = graph_height + TELEINFO_GRAPH_STEP;
  WSContentSend_P (PSTR ("<a href='%s?period=%d&data=%d&height=%d&phase=%s'><div class='item size'>+</div></a>"), D_TELEINFO_PAGE_GRAPH, graph_period, graph_display, index, str_phase);
  WSContentSend_P (PSTR ("</div>\n"));

  // start second line of parameters
  WSContentSend_P (PSTR ("</div>\n"));

  // display graph base and data
  WSContentSend_P (PSTR ("<div class='svg-container'>\n"));
  WSContentSend_P (PSTR ("<object class='svg-content' id='base' type='image/svg+xml' width='100%%' height='100%%' data='%s?period=%d&data=%d&height=%d'></object>\n"), D_TELEINFO_PAGE_GRAPH_FRAME, graph_period, graph_display, graph_height);
  WSContentSend_P (PSTR ("<object class='svg-content' id='data' type='image/svg+xml' width='100%%' height='100%%' data='%s?period=%d&data=%d&height=%d&phase=%s&ts=0'></object>\n"), D_TELEINFO_PAGE_GRAPH_DATA, graph_period, graph_display, graph_height, str_phase);
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
  // swtich according to context
  switch (function)
  {
    case FUNC_PRE_INIT:
      TeleinfoPreInit ();
      break;
    case FUNC_INIT:
      TeleinfoInit ();
      break;
  }
  return false;
}

// teleinfo sensor
bool Xsns99 (uint8_t function)
{
  bool publish_tic, publish_data;

  // swtich according to context
  switch (function) 
  {
    case FUNC_INIT:
      TeleinfoGraphInit ();
      break;
    case FUNC_EVERY_100_MSECOND:
      if (teleinfo_enabled && (TasmotaGlobal.uptime > 4)) TeleinfoReceiveData ();
      break;
    case FUNC_EVERY_SECOND:
      TeleinfoEverySecond ();
      break;
    case FUNC_JSON_APPEND:
      if (teleinfo_enabled)
      {
        // get publication policy
        publish_tic  = (TeleinfoGetPolicyTIC ()  != TELEINFO_POLICY_NEVER);
        publish_data = (TeleinfoGetPolicyData () != TELEINFO_POLICY_NEVER);
        TeleinfoShowJSON (true, publish_tic, publish_data);
      }
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      // pages
      Webserver->on (FPSTR (D_TELEINFO_PAGE_CONFIG),    TeleinfoWebPageConfig);
      Webserver->on (FPSTR (D_TELEINFO_PAGE_TIC),       TeleinfoWebPageTic);
      Webserver->on (FPSTR (D_TELEINFO_PAGE_DATA_JSON), TeleinfoWebJsonData);

      // graph
      Webserver->on (FPSTR (D_TELEINFO_PAGE_GRAPH),       TeleinfoWebPageGraph);
      Webserver->on (FPSTR (D_TELEINFO_PAGE_GRAPH_FRAME), TeleinfoWebGraphFrame);
      Webserver->on (FPSTR (D_TELEINFO_PAGE_GRAPH_DATA),  TeleinfoWebGraphData);
      
      // update status
      Webserver->on (FPSTR (D_TELEINFO_PAGE_GRAPH_UPD), TeleinfoWebGraphUpdate);
      Webserver->on (FPSTR (D_TELEINFO_PAGE_TIC_UPD),   TeleinfoWebTicUpdate);

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
  return false;
}

#endif   // USE_TELEINFO
#endif   // USE_ENERGY_SENSOR
