/*
  xnrg_16_teleinfo.ino - France Teleinfo energy sensor support for Sonoff-Tasmota
  
  Copyright (C) 2020  Nicolas Bernaerts

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
#define XSNS_15   15

// teleinfo constant
#define TELEINFO_VOLTAGE             230        // default contract voltage is 200V
#define TELEINFO_LINE_MAX            71         // maximum number of lines in a TIC message (71 lines)
#define TELEINFO_STRING_MAX          30         // max length of etiquette or donnee (28 char)
#define TELEINFO_PHASE_MAX           3          // maximum number of phases
#define TELEINFO_SERIAL_BUFFER       4          // teleinfo serial buffer

// graph data
#define TELEINFO_GRAPH_SAMPLE        365         // 1 day per year
#define TELEINFO_GRAPH_WIDTH         800      
#define TELEINFO_GRAPH_HEIGHT        500 
#define TELEINFO_GRAPH_PERCENT_START 10     
#define TELEINFO_GRAPH_PERCENT_STOP  90

// commands
#define D_CMND_TELEINFO_MODE         "mode"
#define D_CMND_TELEINFO_ETH          "eth"
#define D_CMND_TELEINFO_PERIOD       "period"

// JSON TIC extensions
#define TELEINFO_JSON_PERIOD         "PERIOD"
#define TELEINFO_JSON_PHASE          "PHASE"
#define TELEINFO_JSON_ADCO           "ADCO"
#define TELEINFO_JSON_SSOUSC         "SSOUSC"
#define TELEINFO_JSON_IINST          "IINST"
#define TELEINFO_JSON_SINSTS         "SINSTS"

// interface strings
#define D_TELEINFO                   "Teleinfo"
#define D_TELEINFO_TIC               "TIC"
#define D_TELEINFO_MESSAGE           "Messages"
#define D_TELEINFO_HEURES            "Heures"

// web URL
const char D_TELEINFO_PAGE_CONFIG[]    PROGMEM = "/tic-cfg";
const char D_TELEINFO_PAGE_TIC[]       PROGMEM = "/tic-msg";
const char D_TELEINFO_PAGE_TIC_UPD[]   PROGMEM = "/tic-msg.upd";
const char D_TELEINFO_PAGE_GRAPH[]     PROGMEM = "/tic-graph";
const char D_TELEINFO_PAGE_GRAPH_UPD[] PROGMEM = "/tic-graph.upd";
const char D_TELEINFO_PAGE_BASE_SVG[]  PROGMEM = "/tic-base.svg";
const char D_TELEINFO_PAGE_DATA_SVG[]  PROGMEM = "/tic-data.svg";
const char D_TELEINFO_PAGE_DATA[]      PROGMEM = "/tic-data.json";
const char D_TELEINFO_PAGE_TIC_PNG[]   PROGMEM = "/tic.png";

// web strings
const char D_TELEINFO_GRAPH[]         PROGMEM = "Graph";
const char D_TELEINFO_CONFIG[]        PROGMEM = "Configure Teleinfo";
const char D_TELEINFO_RESET[]         PROGMEM = "Message reset";
const char D_TELEINFO_ERROR[]         PROGMEM = "Errors";
const char D_TELEINFO_CHECKSUM[]      PROGMEM = "Checksum";
const char D_TELEINFO_DISABLED[]      PROGMEM = "Disabled";
const char D_TELEINFO_SPEED_DEFAULT[] PROGMEM = "bauds";
const char D_TELEINFO_SPEED_HISTO[]   PROGMEM = "bauds (Historique)";
const char D_TELEINFO_SPEED_STD[]     PROGMEM = "bauds (Standard)";
const char D_TELEINFO_VOLTAGE[]       PROGMEM = "Voltage";
const char D_TELEINFO_MODE[]          PROGMEM = "Mode";
const char D_TELEINFO_PERIOD[]        PROGMEM = "Period";

// form strings
const char TELEINFO_INPUT_TEXT[]  PROGMEM = "<p><input type='radio' name='%s' value='%d' %s>%s</p>\n";
const char TELEINFO_INPUT_VALUE[] PROGMEM = "<p><input type='radio' name='%s' value='%d' %s>%d %s</p>\n";
const char TELEINFO_FORM_START[]  PROGMEM = "<form method='get' action='%s'>\n";
const char TELEINFO_FORM_STOP[]   PROGMEM = "</form>\n";
const char TELEINFO_FIELD_START[] PROGMEM = "<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>\n";
const char TELEINFO_FIELD_STOP[]  PROGMEM = "</fieldset></p><br>\n";
const char TELEINFO_HTML_POWER[]  PROGMEM = "<text class='power' x='%d%%' y='%d%%'>%d</text>\n";
const char TELEINFO_HTML_DASH[]   PROGMEM = "<line class='dash' x1='%d%%' y1='%d%%' x2='%d%%' y2='%d%%' />\n";
const char TELEINFO_HTML_BAR[]    PROGMEM = "<tr><div style='margin:4px;padding:0px;background-color:#ddd;border-radius:4px;'><div style='font-size:0.75rem;padding:0px;text-align:center;border:1px solid #bbb;border-radius:4px;color:#444;background-color:%s;width:%s%%;'>%s%%</div></div></tr>\n";

// TIC - specific etiquettes
enum TeleinfoEtiquette { TIC_NONE, TIC_ADCO, TIC_ADSC, TIC_PTEC, TIC_NGTF, TIC_EAIT, TIC_IINST, TIC_IINST1, TIC_IINST2, TIC_IINST3, TIC_ISOUSC, TIC_PS, TIC_PAPP, TIC_SINSTS, TIC_BASE, TIC_EAST, TIC_HCHC, TIC_HCHP, TIC_EJPHN, TIC_EJPHPM, TIC_BBRHCJB, TIC_BBRHPJB, TIC_BBRHCJW, TIC_BBRHPJW, TIC_BBRHCJR, TIC_BBRHPJR, TIC_ADPS, TIC_ADIR1, TIC_ADIR2, TIC_ADIR3, TIC_URMS1, TIC_URMS2, TIC_URMS3, TIC_UMOY1, TIC_UMOY2, TIC_UMOY3, TIC_IRMS1, TIC_IRMS2, TIC_IRMS3, TIC_SINSTS1, TIC_SINSTS2, TIC_SINSTS3, TIC_PTCOUR, TIC_PREF, TIC_PCOUP, TIC_MAX };
const char kTeleinfoEtiquetteName[] PROGMEM = "|ADCO|ADSC|PTEC|NGTF|EAIT|IINST|IINST1|IINST2|IINST3|ISOUSC|PS|PAPP|SINSTS|BASE|EAST|HCHC|HCHP|EJPHN|EJPHPM|BBRHCJB|BBRHPJB|BBRHCJW|BBRHPJW|BBRHCJR|BBRHPJR|ADPS|ADIR1|ADIR2|ADIR3|URMS1|URMS2|URMS3|UMOY1|UMOY2|UMOY3|IRMS1|IRMS2|IRMS3|SINSTS1|SINSTS2|SINSTS3|PTCOUR|PREF|PCOUP";

// TIC - modes
enum TeleinfoMode { TIC_MODE_UNDEFINED, TIC_MODE_HISTORIC, TIC_MODE_STANDARD };
const char kTeleinfoModeName[] PROGMEM = "|Historique|Standard";

// TIC - tarifs
enum TeleinfoPeriod { TIC_HISTO_TH, TIC_HISTO_HC, TIC_HISTO_HP, TIC_HISTO_HN, TIC_HISTO_PM, TIC_HISTO_CB, TIC_HISTO_CW, TIC_HISTO_CR, TIC_HISTO_PB, TIC_HISTO_PW, TIC_HISTO_PR, TIC_HISTO_MAX, TIC_STD_P, TIC_STD_HPH, TIC_STD_HCH, TIC_STD_HPD, TIC_STD_HCD, TIC_STD_HPE, TIC_STD_HCE, TIC_STD_JA, TIC_STD_PM, TIC_STD_HH, TIC_STD_HD, TIC_STD_HM, TIC_STD_DSM, TIC_STD_SCM, TIC_STD_1, TIC_STD_2, TIC_STD_3, TIC_STD_4, TIC_STD_5 };
const char kTeleinfoPeriod[] PROGMEM = "TH..|HC..|HP..|HN..|PM..|HCJB|HCJW|HCJR|HPJB|HPJW|HPJR|P|HPH|HCH|HPD|HCD|HPE|HCE|JA|PM|HH|HD|HM|DSM|SCM|1|2|3|4|5|BASE";
const char kTeleinfoPeriodName[] PROGMEM = "Toutes|Creuses|Pleines|Normales|Pointe Mobile|Creuses Bleus|Creuses Blancs|Creuses Rouges|Pleines Bleus|Pleines Blancs|Pleines Rouges|Pointe|Pleines Hiver|Creuses Hiver|Pleines Demi-saison|Creuses Demi-saison|Pleines Ete|Creuses Ete|Juillet-Aout|Pointe Mobile|Hiver|Demi-saison|Hiver Mobile|Demi-saison Mobile|Saison Creuse Mobile|Pointe|Pleines Hiver|Creuses Hiver|Pleines Ete|Creuses Ete|Base";

// graph - periods
enum TeleinfoGraphPeriod { TELEINFO_PERIOD_LIVE, TELEINFO_PERIOD_DAY, TELEINFO_PERIOD_WEEK, TELEINFO_PERIOD_YEAR, TELEINFO_PERIOD_MAX };      // available graph periods
const char kTeleinfoGraphPeriod[] PROGMEM = "Live|Day|Week|Year";                                                                             // period labels
const long arr_period_sample[] = { 5, 236, 1657, 86400 };                                                                                     // number of seconds between samples

// graph - phase colors
const char TELEINFO_PHASE_ID0[]     PROGMEM = "ph0";
const char TELEINFO_PHASE_ID1[]     PROGMEM = "ph1";
const char TELEINFO_PHASE_ID2[]     PROGMEM = "ph2";
const char TELEINFO_PHASE_COLOR0[]  PROGMEM = "#FFA500";         // orange
const char TELEINFO_PHASE_COLOR1[]  PROGMEM = "#FF6347";         // tomato
const char TELEINFO_PHASE_COLOR2[]  PROGMEM = "#D2691E";         // chocolate
const char *const arr_phase_id[]    PROGMEM = { TELEINFO_PHASE_ID0, TELEINFO_PHASE_ID1, TELEINFO_PHASE_ID2 };
const char *const arr_phase_color[] PROGMEM = { TELEINFO_PHASE_COLOR0, TELEINFO_PHASE_COLOR1, TELEINFO_PHASE_COLOR2 };

// graph - week days name
const char *const arr_week_day[] PROGMEM = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

// teleinfo driver status
bool teleinfo_enabled = false;

// teleinfo data
struct {
  int     phase  = 1;                         // number of phases
  int     mode   = TIC_MODE_UNDEFINED;        // meter mode
  long    isousc = 0;                         // contract max current per phase
  long    ssousc = 0;                         // contract max power per phase
  String  id;                                 // contract reference (adco or ads)
  String  period;                             // current tarif period
} teleinfo_contract;

// teleinfo current line
struct {
  String str_text;
  char   separator;
} teleinfo_line;

// teleinfo update trigger
struct {
bool updated    = false;        // data have been updated
long papp_last  = 0;            // last published apparent power
long papp_delta = 0;            // apparent power delta to publish
} teleinfo_json;

// teleinfo power counters
struct {
  long papp   = 0;             // total apparent power
  long total  = 0;             // total of all indexes
  long index1 = 0;             // index of different tarif periods
  long index2 = 0;
  long index3 = 0;
  long index4 = 0;
  long index5 = 0;
  long index6 = 0;
} teleinfo_counter;

// data per phase
struct tic_phase {
  long voltage;                // voltage
  long iinst;                  // instant current
  long papp;                   // instant apparent power
  long last_papp;              // last published apparent power
  uint32_t last_diff;          // last time a power difference was published
}; 
tic_phase teleinfo_phase[TELEINFO_PHASE_MAX];

// TIC message array
struct tic_line {
  String etiquette;
  String donnee;
  char   checksum;
};

struct {
  bool overload   = false;            // overload has been detected
  long nb_message = 0;                // number of received messages
  long nb_data    = 0;                // number of received data (lines)
  long nb_error   = 0;                // number of checksum errors
  long nb_reset   = 0;                // number of message reset
  int  line_index = 0;                // index of current received message line
  int  line_max   = 0;                // max number of lines in a message
  tic_line line[TELEINFO_LINE_MAX];       // array of message lines
} teleinfo_message;

// graph
struct tic_period {
  bool     updated;                                               // flag to ask for graph update
  int      index;                                                 // current array index per refresh period
  long     counter;                                               // counter in seconds of current refresh period
  uint16_t pmax;                                                  // graph maximum power
  uint16_t papp_peak[TELEINFO_PHASE_MAX];                         // peak apparent power during refresh period (per phase)
  unsigned long papp_sum[TELEINFO_PHASE_MAX];                     // sum of apparent power during refresh period (per phase)
  uint16_t arr_papp[TELEINFO_PHASE_MAX][TELEINFO_GRAPH_SAMPLE];   // array of graph values
}; 
tic_period teleinfo_graph[TELEINFO_PERIOD_MAX];

#ifdef ESP32
// serial port
HardwareSerial *teleinfo_serial = nullptr;
#endif

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

// get teleinfo mode (baud rate)
uint16_t TeleinfoGetMode ()
{
  uint16_t actual_mode;

  // read actual teleinfo mode
  actual_mode = Settings.sbaudrate;

  // if outvalue, set to disabled
  if (actual_mode > 19200) actual_mode = 0;
  
  return actual_mode;
}

// set teleinfo mode (baud rate)
void TeleinfoSetMode (uint16_t new_mode)
{
  // if within range, set baud rate
  if (new_mode <= 19200)
  {
    // if mode has changed
    if (Settings.sbaudrate != new_mode)
    {
      // save mode
      Settings.sbaudrate = new_mode;

      // ask for restart
      TasmotaGlobal.restart_flag = 2;
    }
  }
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
      AddLog (LOG_LEVEL_INFO, PSTR ("ERR: %c not %c, sep %d, line %s"), line_checksum, given_checksum, teleinfo_line.separator, teleinfo_line.str_text.c_str ());

      // reset checksum
      line_checksum = 0;
    } 

  }

  return line_checksum;
}

String TeleinfoCleanupDonnee (const char* pstr_string) 
{
  char   previous_car = 0;
  char   current_car  = 0;
  char*  pstr_text;
  String str_cleanup;

  // if string is defined
  pstr_text = (char*) pstr_string;
  if (pstr_text != nullptr)
  {
    // loop thru string
    while (*pstr_text)
    {
      current_car = (char)*pstr_text;
      if ((previous_car != ' ') || (current_car != ' ')) str_cleanup += current_car;
      previous_car = current_car;
      pstr_text++;
    }
 
    // trim string
    str_cleanup.trim ();
  }

  return str_cleanup;
}

void TeleinfoPreInit ()
{
  // if PIN defined, set energy driver
  if (!TasmotaGlobal.energy_driver && PinUsed(GPIO_TELEINFO_RX))
  {
    // energy driver is teleinfo meter
    TasmotaGlobal.energy_driver = XNRG_15;

    // voltage not available in teleinfo
    Energy.voltage_available = false;
  }
}

void TeleinfoInit ()
{
  int      index;
  uint16_t teleinfo_mode;

  // get teleinfo speed
  teleinfo_mode = TeleinfoGetMode ();

  // if sensor has been pre initialised
  teleinfo_enabled = ((TasmotaGlobal.energy_driver == XNRG_15) && (teleinfo_mode > 0));
  if (teleinfo_enabled)
  {
#ifdef ESP8266
    // start ESP8266 serial port
    Serial.flush ();
    Serial.begin (teleinfo_mode, SERIAL_7E1);
    ClaimSerial ();
#else  // ESP32
    // start ESP32 serial port
    teleinfo_serial = new HardwareSerial(2);
    teleinfo_serial->begin(teleinfo_mode, SERIAL_7E1, Pin(GPIO_TELEINFO_RX), -1);
#endif // ESP286 & ESP32

    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Teleinfo Serial ready"));

    // init hardware energy counters
    Settings.flag3.hardware_energy_total = true;
    RtcSettings.energy_kWhtotal = 0;
    RtcSettings.energy_kWhtoday = 0;
    Settings.energy_kWhtotal    = 0;

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
    // set default voltage
    Energy.voltage[index] = TELEINFO_VOLTAGE;

    // default values
    teleinfo_phase[phase].voltage   = 0;
    teleinfo_phase[phase].iinst     = 0;
    teleinfo_phase[phase].papp      = 0;
    teleinfo_phase[phase].last_papp = 0;
    teleinfo_phase[phase].last_diff = UINT32_MAX;
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
      teleinfo_graph[period].papp_peak[phase] = 0;
      teleinfo_graph[period].papp_sum[phase]  = 0;

      // loop thru graph values
      for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++) teleinfo_graph[period].arr_papp[phase][index] = UINT16_MAX;
    }
  }
}

void TeleinfoEvery250ms ()
{
  char   recv_serial, checksum;
  int    index, phase, mode;
  int    index_start, index_stop, index_last;
  long   current_total;
  float  power_apparent;
  char   str_text[16];
  String str_etiquette;
  String str_donnee;

#ifdef ESP8266
  // loop as long as serial port buffer is not almost empty
  while (Serial.available() > TELEINFO_SERIAL_BUFFER) 
  {
    // read caracter
    recv_serial = Serial.read ();
#else  // ESP32
  // loop as long as serial port buffer is not almost empty
  while (teleinfo_serial->available() > TELEINFO_SERIAL_BUFFER) 
  {
    // read caracter
    recv_serial = teleinfo_serial->read(); 
#endif // ESP286 & ESP32

    switch (recv_serial)
    {
      // ---------------------------
      // 0x02 : Beginning of message 
      // ---------------------------
      case 2:
        // reset current message line index
        teleinfo_message.line_index = 0;
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
          teleinfo_message.line[index].etiquette = "";
          teleinfo_message.line[index].donnee    = "";
          teleinfo_message.line[index].checksum  = 0;
        }

        // save number of lines and reset index
        if (teleinfo_message.line_index < teleinfo_message.line_max - 10) teleinfo_message.line_max = teleinfo_message.line_index;
        else teleinfo_message.line_max = max (teleinfo_message.line_max, teleinfo_message.line_index);
        teleinfo_message.line_index = 0;

        // if defined, defined maximum power per phase
        if (teleinfo_contract.ssousc == 0) teleinfo_contract.ssousc = teleinfo_contract.isousc * 200;

        // if needed, declare 3 phases
        if (Energy.phase_count != teleinfo_contract.phase) Energy.phase_count = teleinfo_contract.phase;

        // loop to calculate total current
        current_total = 0;
        for (phase = 0; phase < teleinfo_contract.phase; phase++) current_total += teleinfo_phase[phase].iinst;

        // loop to update current and power
        for (phase = 0; phase < teleinfo_contract.phase; phase++)
        {
          // calculate phase apparent power
          if (current_total == 0) teleinfo_phase[phase].papp = teleinfo_counter.papp / teleinfo_contract.phase;
          else teleinfo_phase[phase].papp = (teleinfo_counter.papp * teleinfo_phase[phase].iinst) / current_total;

          // update phase active power and instant current
          power_apparent = (float)teleinfo_phase[phase].papp;
          Energy.current[phase]        = (float)teleinfo_phase[phase].iinst;
          Energy.apparent_power[phase] = power_apparent;
          Energy.active_power[phase]   = power_apparent;
        } 

        // update total energy counter
        teleinfo_counter.total = teleinfo_counter.index1 + teleinfo_counter.index2 + teleinfo_counter.index3 + teleinfo_counter.index4 + teleinfo_counter.index5 + teleinfo_counter.index6;

        // message update : if papp above ssousc
        if (teleinfo_counter.papp > teleinfo_contract.ssousc) teleinfo_json.updated = true;

        // message update : if more than 1% power change
        teleinfo_json.papp_delta = teleinfo_contract.ssousc / 100;
        if (abs (teleinfo_json.papp_last - teleinfo_counter.papp) > teleinfo_json.papp_delta) teleinfo_json.updated = true;
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
        teleinfo_line.separator = 0;
        teleinfo_line.str_text  = "";
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
          checksum = TeleinfoCalculateChecksum (teleinfo_line.str_text.c_str ());
          if (checksum != 0)
          {
            // init
            str_etiquette = "";
            str_donnee = "";

            // extract etiquette
            index_start = 0;
            index_stop  = teleinfo_line.str_text.indexOf (teleinfo_line.separator);
            if (index_stop > -1) str_etiquette = teleinfo_line.str_text.substring (index_start, index_stop);

            // extract donnee
            index_start = index_stop + 1;
            index_stop  = teleinfo_line.str_text.indexOf (teleinfo_line.separator, index_start);
            index_last  = teleinfo_line.str_text.lastIndexOf (teleinfo_line.separator);
            if (index_stop == index_last) str_donnee = teleinfo_line.str_text.substring (index_start, index_stop);
            else str_donnee = teleinfo_line.str_text.substring (index_stop + 1, index_last);
            str_donnee = TeleinfoCleanupDonnee (str_donnee.c_str ());

            // handle specific etiquette index
            index = GetCommandCode (str_text, sizeof(str_text), str_etiquette.c_str (), kTeleinfoEtiquetteName);
            switch (index)
            {
              // contract reference
              case TIC_ADCO:
                teleinfo_message.line_index = 0;
                teleinfo_contract.mode = TIC_MODE_HISTORIC;
                teleinfo_contract.id = str_donnee;
                break;
              case TIC_ADSC:
                teleinfo_message.line_index = 0;
                teleinfo_contract.mode = TIC_MODE_STANDARD;
                teleinfo_contract.id = str_donnee;
                break;
              // period name
              case TIC_PTEC:
              case TIC_NGTF:
              case TIC_PTCOUR:
                teleinfo_contract.period = str_donnee;
                break;
              // instant current (also used to set triphase)
              case TIC_EAIT:
              case TIC_IINST:
              case TIC_IRMS1:
              case TIC_IINST1:
                teleinfo_phase[0].iinst = str_donnee.toInt ();
                break;
              case TIC_IRMS2:
              case TIC_IINST2:
                teleinfo_phase[1].iinst = str_donnee.toInt ();
                break;
              case TIC_IRMS3:
              case TIC_IINST3:
                teleinfo_phase[2].iinst = str_donnee.toInt (); 
                teleinfo_contract.phase = 3; 
                break;
              // contract max current or power
              case TIC_ISOUSC:
                teleinfo_contract.isousc = str_donnee.toInt ();
                break;
              case TIC_PS:
                teleinfo_contract.ssousc = str_donnee.toInt ();
                break;
              // instant power
              case TIC_PAPP:
              case TIC_SINSTS:
                teleinfo_counter.papp = str_donnee.toInt ();
                break;
              // option base or standard
              case TIC_BASE:
              case TIC_EAST:
                teleinfo_counter.index1 = str_donnee.toInt ();
                break;
              // option heures creuses
              case TIC_HCHC:
                teleinfo_counter.index1 = str_donnee.toInt ();
                break;
              case TIC_HCHP:
                teleinfo_counter.index2 = str_donnee.toInt ();
                break;
              // option EJP
              case TIC_EJPHN:
                teleinfo_counter.index1 = str_donnee.toInt ();
                break;
              case TIC_EJPHPM:
                teleinfo_counter.index2 = str_donnee.toInt ();
                break;
              // option tempo
              case TIC_BBRHCJB:
                teleinfo_counter.index1 = str_donnee.toInt ();
                break;
              case TIC_BBRHPJB:
                teleinfo_counter.index2 = str_donnee.toInt ();
                break;
              case TIC_BBRHCJW:
                teleinfo_counter.index3 = str_donnee.toInt ();
                break;
              case TIC_BBRHPJW:
                teleinfo_counter.index4 = str_donnee.toInt ();
                break;
              case TIC_BBRHCJR:
                teleinfo_counter.index5 = str_donnee.toInt ();
                break;
              case TIC_BBRHPJR:
                teleinfo_counter.index6 = str_donnee.toInt ();
                break;
              // option voltage
              case TIC_URMS1:
                teleinfo_phase[0].voltage = str_donnee.toInt ();
                break;
              case TIC_URMS2:
                teleinfo_phase[1].voltage = str_donnee.toInt ();
                break;
              case TIC_URMS3:
                teleinfo_phase[2].voltage = str_donnee.toInt ();
                break;
              // overload flags
              case TIC_ADPS:
              case TIC_ADIR1:
              case TIC_ADIR2:
              case TIC_ADIR3:
                teleinfo_message.overload = true;
                break;
              case TIC_PREF:
              case TIC_PCOUP:
                teleinfo_contract.ssousc = 1000 * str_donnee.toInt ();
                break;
            }

            // if maximum number of lines no reached
            if (teleinfo_message.line_index < TELEINFO_LINE_MAX)
            {
              // add new message line
              index = teleinfo_message.line_index;
              teleinfo_message.line[index].etiquette = str_etiquette;
              teleinfo_message.line[index].donnee    = str_donnee;
              teleinfo_message.line[index].checksum  = checksum;

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
        if (teleinfo_line.str_text.length () < TOPSZ) teleinfo_line.str_text += (char)recv_serial;
        break;
        
      // add other caracters to current line
      default:
        if (teleinfo_line.str_text.length () < TOPSZ) teleinfo_line.str_text += (char)recv_serial;
        break;
    }
  }
}

void TeleinfoEverySecond ()
{
  int      period, phase;
  uint16_t current_papp;

  // loop thru the periods and the phases, to update apparent power to the max on the period
  for (period = 0; period < TELEINFO_PERIOD_MAX; period++)
  {
    // loop thru phases to update max value
    for (phase = 0; phase < teleinfo_contract.phase; phase++)
    {
      // get current apparent power
      current_papp = (uint16_t)teleinfo_phase[phase].papp;

      // if within range, update phase apparent power
      if (current_papp != UINT16_MAX)
      {
        // add power to period total
        teleinfo_graph[period].papp_sum[phase] += current_papp;

        // update peak current
        if (teleinfo_graph[period].papp_peak[phase] < current_papp) teleinfo_graph[period].papp_peak[phase] = current_papp;
      } 
    }

    // increment graph period counter and update graph data if needed
    if (teleinfo_graph[period].counter == 0) TeleinfoUpdateGraphData (period);
    teleinfo_graph[period].counter ++;
    teleinfo_graph[period].counter = teleinfo_graph[period].counter % arr_period_sample[period];
  }

  // if current or overload has been updated, publish teleinfo data
  if (teleinfo_json.updated) TeleinfoShowJSON (false);
}

// update graph history data
void TeleinfoUpdateGraphData (uint8_t graph_period)
{
  int      index, phase;
  unsigned long average_papp;

  // set indexed graph values with current values
  index = teleinfo_graph[graph_period].index;
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    // if overload has been detected during period, save overload value
    if (teleinfo_graph[graph_period].papp_peak[phase] > teleinfo_contract.ssousc)
      teleinfo_graph[graph_period].arr_papp[phase][index] = teleinfo_graph[graph_period].papp_peak[phase];

    // else save average value
    else
    {
      average_papp = teleinfo_graph[graph_period].papp_sum[phase] / arr_period_sample[graph_period];
      teleinfo_graph[graph_period].arr_papp[phase][index] = (uint16_t) average_papp;
    }

    // reset period data
    teleinfo_graph[graph_period].papp_peak[phase] = 0;
    teleinfo_graph[graph_period].papp_sum[phase]  = 0;
  }

  // increase data index in the graph
  teleinfo_graph[graph_period].index++;
  teleinfo_graph[graph_period].index = teleinfo_graph[graph_period].index % TELEINFO_GRAPH_SAMPLE;

  // set update flag
  teleinfo_graph[graph_period].updated = 1;
}

// Show JSON status (for MQTT)
//  "TIC":{ "ADCO":"1234567890","ISOUSC":30,"SSOUSC":6000,"SINSTS":2567,"SINSTS1":1243,"IINST1":14.7,"ADIR1":0,"SINSTS2":290,"IINST2":4.4,"ADIR2":0,"SINSTS3":6500,"IINST3":33,"ADIR3":110, "PHASE":3 }
void TeleinfoShowJSON (bool append)
{
  bool    finished = false;
  int     index;
  String  str_json;

  // if telemetry call, update energy data (to avoid nand wear with too frequent updates)
  if (append == true) EnergyUpdateTotal((float) teleinfo_counter.total, false);

  // reset update flag and update published apparent power
  teleinfo_json.updated   = false;
  teleinfo_json.papp_last = teleinfo_counter.papp;

  // if not in append mode, add current time
  if (append == false)
  {
    str_json = "\"";
    str_json += D_JSON_TIME;
    str_json += "\":\"";
    str_json += GetDateAndTime(DT_LOCAL);
    str_json += "\",";
  } 

  // start TIC section
  str_json += "\"";
  str_json += D_TELEINFO_TIC;
  str_json += "\":{";

  // --------------------
  // TIC received message
  // --------------------

  // loop thru TIC message lines
  for (index = 0; index < TELEINFO_LINE_MAX; index ++)
    if (teleinfo_message.line[index].checksum != 0)
    {
      // add current line
      if (index != 0) str_json += ",";
      str_json += "\"";
      str_json += teleinfo_message.line[index].etiquette;
      str_json += "\":\"";
      str_json += teleinfo_message.line[index].donnee;
      str_json += "\"";
    }

  // --------------
  // TIC extensions
  // --------------

  // PHASE : number of phases 
  str_json += ",\"";
  str_json += TELEINFO_JSON_PHASE;
  str_json += "\":";
  str_json += teleinfo_contract.phase;

  // SSOUSC : power per phase
  str_json += ",\"";
  str_json += TELEINFO_JSON_SSOUSC;
  str_json += "\":\"";
  str_json += teleinfo_contract.ssousc;
  str_json += "\"";

  // SINSTS1 : if only one phase, phase instant power 
  if (teleinfo_contract.phase == 1)
  {
    // instant current
    str_json += ",\"";
    str_json += TELEINFO_JSON_IINST;
    str_json += "1\":\"";
    str_json += teleinfo_phase[0].iinst;
    str_json += "\"";

    // instant power
    str_json += ",\"";
    str_json += TELEINFO_JSON_SINSTS;
    str_json += "1\":\"";
    str_json += teleinfo_phase[0].papp;
    str_json += "\"";
  }

  // end of TIC section
  str_json += "}";

  // generate MQTT message according to append mode
  if (append) ResponseAppend_P (PSTR(",%s"), str_json.c_str ());
  else Response_P (PSTR("{%s}"), str_json.c_str ());

  // publish it if not in append mode
  if (!append) MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));
}

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

// display offload icons
void TeleinfoWebIconTic () { Webserver->send_P (200, PSTR ("image/png"), teleinfo_icon_tic_png, teleinfo_icon_tic_len); }

// append Teleinfo graph button to main page
void TeleinfoWebMainButton ()
{
  // Teleinfo message page button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s %s</button></form></p>\n"), D_TELEINFO_PAGE_TIC, D_TELEINFO_TIC, D_TELEINFO_MESSAGE);

  // Teleinfo graph page button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_TELEINFO_PAGE_GRAPH, D_TELEINFO_GRAPH);
}

// append Teleinfo configuration button to configuration page
void TeleinfoWebButton ()
{
  // heater and meter configuration button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>"), D_TELEINFO_PAGE_CONFIG, D_TELEINFO_CONFIG);
}

// append Teleinfo state to main page
void TeleinfoWebSensor ()
{
  int    phase, period;
  int    sizeString;
  float  percentage;
  char   str_text[32];
  String str_header;
  String str_display;

  // display phase graph bar
  if (teleinfo_contract.ssousc > 0) for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    // calculate phase percentage
    percentage  = 100 * teleinfo_phase[phase].papp;
    percentage  = percentage / teleinfo_contract.ssousc;
    str_display = String (percentage, 0);

    // display graph bar
    WSContentSend_PD (TELEINFO_HTML_BAR, arr_phase_color[phase], str_display.c_str (), str_display.c_str ());
  }

  // if defined, display voltage
  str_display = "";
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
    if (teleinfo_phase[phase].voltage > 0) 
    {
      if (phase > 0) str_display += " / ";
      str_display += teleinfo_phase[phase].voltage;
    }
  if (str_display.length () > 0) WSContentSend_PD (PSTR("{s}%s{m}%s V{e}"), D_TELEINFO_VOLTAGE, str_display.c_str ());

  // get TIC mode
  GetTextIndexed(str_text, sizeof(str_text), teleinfo_contract.mode, kTeleinfoModeName);
  str_header  = D_TELEINFO_MODE;
  str_display = str_text;
  
  // get TIC tarif period, with conversion if historic short version
  str_header += "<br>";
  str_header += D_TELEINFO_PERIOD;
  str_display += "<br>";
  sizeString = teleinfo_contract.period.length ();
  if (sizeString >= 5) str_display += teleinfo_contract.period;
  else if (sizeString > 0)
  {
    // get period label
    period = GetCommandCode (str_text, sizeof(str_text), teleinfo_contract.period.c_str (), kTeleinfoPeriod);
    GetTextIndexed(str_text, sizeof(str_text), period, kTeleinfoPeriodName);

    // add period label
    str_display += D_TELEINFO_HEURES;
    str_display += " ";
    str_display += str_text;
  }

  // display TIC mode and period
  WSContentSend_PD (PSTR("{s}%s{m}%s{e}"), str_header.c_str (), str_display.c_str ());

  // get number of TIC messages received
  str_header  = D_TELEINFO_MESSAGE;
  str_display = teleinfo_message.nb_message;

  // get TIC checksum errors
  if ((teleinfo_message.nb_data > 0) && ( teleinfo_message.nb_error > 0 ))
  {
    // calculate error percentage
    percentage = teleinfo_message.nb_error * 100;
    percentage = percentage / teleinfo_message.nb_data;

    // append data
    str_header += "<br>";
    str_header += D_TELEINFO_ERROR;
    str_display += "<br>";
    str_display += teleinfo_message.nb_error;
    str_display += " <small>(";
    str_display += String (percentage);
    str_display += "%)</small>";
  }

  // get TIC reset
  if (teleinfo_message.nb_reset > 0)
  {
    // append data
    str_header += "<br>";
    str_header += D_TELEINFO_RESET;
    str_display += "<br>";
    str_display += teleinfo_message.nb_reset;
  }

  // display TIC messages, errors and reset
  WSContentSend_PD (PSTR("{s}%s{m}%s{e}"), str_header.c_str (), str_display.c_str ());
}

// Teleinfo web page
void TeleinfoWebPageConfig ()
{
  bool     need_restart = false;
  uint16_t value;
  char     argument[LOGSZ];
  String   str_text;

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // get teleinfo mode according to MODE parameter
  if (Webserver->hasArg(D_CMND_TELEINFO_MODE))
  {
    // set teleinfo speed
    WebGetArg (D_CMND_TELEINFO_MODE, argument, LOGSZ);
    TeleinfoSetMode ((uint16_t) atoi (argument));

    // ask for reboot
    WebRestart(1);
  }

  // beginning of form
  WSContentStart_P (D_TELEINFO_CONFIG);
  WSContentSendStyle ();
  WSContentSend_P (TELEINFO_FORM_START, D_TELEINFO_PAGE_CONFIG);

  // speed selection form : start
  value = TeleinfoGetMode ();
  WSContentSend_P (TELEINFO_FIELD_START, D_TELEINFO);

  // speed selection form : available modes
  if (value == 0) str_text = "checked"; else str_text = "";
  WSContentSend_P (TELEINFO_INPUT_TEXT, D_CMND_TELEINFO_MODE, 0, str_text.c_str (), D_TELEINFO_DISABLED);
  if (value == 1200) str_text = "checked"; else str_text = "";      // 1200 baud
  WSContentSend_P (TELEINFO_INPUT_VALUE, D_CMND_TELEINFO_MODE, 1200, str_text.c_str (), 1200, D_TELEINFO_SPEED_HISTO);
  if (value == 2400) str_text = "checked"; else str_text = "";      // 2400 baud
  WSContentSend_P (TELEINFO_INPUT_VALUE, D_CMND_TELEINFO_MODE, 2400, str_text.c_str (), 2400, D_TELEINFO_SPEED_DEFAULT);
  if (value == 4800) str_text = "checked"; else str_text = "";      // 4800 baud
  WSContentSend_P (TELEINFO_INPUT_VALUE, D_CMND_TELEINFO_MODE, 4800, str_text.c_str (), 4800, D_TELEINFO_SPEED_DEFAULT);
  if (value == 9600) str_text = "checked"; else str_text = "";      // 9600 baud
  WSContentSend_P (TELEINFO_INPUT_VALUE, D_CMND_TELEINFO_MODE, 9600, str_text.c_str (), 9600, D_TELEINFO_SPEED_STD);
  if (value == 19200) str_text = "checked"; else str_text = "";     // 19200 baud
  WSContentSend_P (TELEINFO_INPUT_VALUE, D_CMND_TELEINFO_MODE, 19200, str_text.c_str (), 19200, D_TELEINFO_SPEED_DEFAULT);

  // speed selection form : end
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
void TeleinfoWebPageData ()
{
  bool     first_value;
  int      index, phase,index_array;
  int      graph_period = TELEINFO_PERIOD_LIVE;
  uint16_t value;
  char     srt_argument[8];

  // check graph period to be displayed
  if (Webserver->hasArg(D_CMND_TELEINFO_PERIOD))
  {
    // set graph period
    WebGetArg (D_CMND_TELEINFO_PERIOD, srt_argument, sizeof(srt_argument));
    graph_period = atoi (srt_argument);
    if ((graph_period < 0) || (graph_period >= TELEINFO_PERIOD_MAX)) graph_period = TELEINFO_PERIOD_LIVE;
  }
  GetTextIndexed(srt_argument, sizeof(srt_argument), graph_period, kTeleinfoGraphPeriod);

  // start of data page
  WSContentBegin(200, CT_HTML);
  WSContentSend_P (PSTR("{\"%s\":\"%s\""), TELEINFO_JSON_PERIOD, srt_argument);
  WSContentSend_P (PSTR(",\"%s\":\"%s\""), TELEINFO_JSON_ADCO,   teleinfo_contract.id.c_str ());
  WSContentSend_P (PSTR(",\"%s\":%d"),     TELEINFO_JSON_SSOUSC, teleinfo_contract.ssousc);
  WSContentSend_P (PSTR(",\"%s\":%d"),     TELEINFO_JSON_PHASE,  teleinfo_contract.phase);

  // loop thru phasis
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    // init value list
    first_value = true;

    // loop for the apparent power array
    WSContentSend_P (PSTR(",\"%s\":["), arr_phase_id[phase]);
    for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
    {
      // get target power array position and add value if defined
      index_array = (index + teleinfo_graph[graph_period].index) % TELEINFO_GRAPH_SAMPLE;
      if (teleinfo_graph[graph_period].arr_papp[phase][index_array] == UINT16_MAX) value = 0; 
        else value = teleinfo_graph[graph_period].arr_papp[phase][index_array];

      // add value to JSON array
      if (first_value) WSContentSend_P (PSTR("%d"), value); 
      else WSContentSend_P (PSTR(",%d"), value);

      // first value is over
      first_value = false;
    }
    WSContentSend_P (PSTR("]"));
  }

  // end of page
  WSContentSend_P (PSTR("}"));
  WSContentEnd();
}

// TIC raw message data
void TeleinfoWebTicUpdate ()
{
  int index;

  // start of data page
  WSContentBegin(200, CT_PLAIN);

  // send line number
  WSContentSend_P ("COUNTER|%d\n", teleinfo_message.nb_message);

  // loop thru TIC message lines to publish if defined
  for (index = 0; index < teleinfo_message.line_max; index ++) WSContentSend_P (PSTR ("%s|%s|%s\n"), teleinfo_message.line[index].etiquette.c_str (), teleinfo_message.line[index].donnee.c_str (), String (teleinfo_message.line[index].checksum).c_str ());

  // end of data page
  WSContentEnd();
}

// TIC message page
void TeleinfoWebPageTic ()
{
  int    index;
  String str_text;

  // beginning of form without authentification
  str_text  = D_TELEINFO_TIC;
  str_text += " ";
  str_text += D_TELEINFO_MESSAGE;
  WSContentStart_P (str_text.c_str (), false);
  WSContentSend_P (PSTR ("</script>\n"));

  // page data refresh script
  WSContentSend_P (PSTR ("<script type='text/javascript'>\n"));
  WSContentSend_P (PSTR ("function update() {\n"));
  WSContentSend_P (PSTR (" httpUpd=new XMLHttpRequest();\n"));
  WSContentSend_P (PSTR (" httpUpd.onreadystatechange=function() {\n"));
  WSContentSend_P (PSTR ("  if (httpUpd.responseText.length>0) {\n"));
  WSContentSend_P (PSTR ("   arr_param=httpUpd.responseText.split('\\n');\n"));
  WSContentSend_P (PSTR ("   num_param=arr_param.length;\n"));
  WSContentSend_P (PSTR ("   arr_value=arr_param[0].split('|');\n"));
  WSContentSend_P (PSTR ("   document.getElementById('count').textContent=arr_value[1];\n"));
  WSContentSend_P (PSTR ("   for (i=1;i<num_param;i++) {\n"));
  WSContentSend_P (PSTR ("    arr_value=arr_param[i].split('|');\n"));
  WSContentSend_P (PSTR ("    document.getElementById('l'+i).hidden=false;\n"));
  WSContentSend_P (PSTR ("    document.getElementById('e'+i).textContent=arr_value[0];\n"));
  WSContentSend_P (PSTR ("    document.getElementById('d'+i).textContent=arr_value[1];\n"));
  WSContentSend_P (PSTR ("    document.getElementById('c'+i).textContent=arr_value[2];\n"));
  WSContentSend_P (PSTR ("   }\n"));
  WSContentSend_P (PSTR ("   for (i=num_param;i<=%d;i++) {\n"), teleinfo_message.line_max);
  WSContentSend_P (PSTR ("    document.getElementById('e'+i).textContent='';\n"));
  WSContentSend_P (PSTR ("    document.getElementById('d'+i).textContent='';\n"));
  WSContentSend_P (PSTR ("    document.getElementById('c'+i).textContent='';\n"));
  WSContentSend_P (PSTR ("   }\n"));
  WSContentSend_P (PSTR ("  }\n"));
  WSContentSend_P (PSTR (" }\n"));
  WSContentSend_P (PSTR (" httpUpd.open('GET','%s',true);\n"), D_TELEINFO_PAGE_TIC_UPD);
  WSContentSend_P (PSTR (" httpUpd.send();\n"));
  WSContentSend_P (PSTR ("}\n"));
  WSContentSend_P (PSTR ("setInterval(function() {update();},2000);\n"));
  WSContentSend_P (PSTR ("</script>\n"));

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {width:100%%;margin:12px auto;padding:3px 0px;text-align:center;vertical-align:middle;}\n"));
  WSContentSend_P (PSTR ("div.title {font-size:2rem;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("span {font-size:1.5rem;padding:0.5rem 1rem;border-radius:1rem;background:#4d82bd;}\n"));
  WSContentSend_P (PSTR ("table {width:100%%;max-width:400px;margin:auto;border:none;background:none;padding:0.5rem;color:#fff;border-collapse:collapse;}\n"));
  WSContentSend_P (PSTR ("th,td {font-size:1.2rem;padding:0.2rem 0.1rem;border:1px #666 solid;}\n"));
  WSContentSend_P (PSTR ("th {font-style:bold;background:#555;}\n"));
  WSContentSend_P (PSTR ("th.narrow {width:20%%;}\n"));
  WSContentSend_P (PSTR ("th.wide {width:60%%;}\n"));
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
  WSContentSend_P (PSTR ("<tr><th class='narrow'>Etiquette</th><th class='wide'>Valeur</th><th class='narrow'>Checksum</th></tr>\n"));

  // loop to display TIC messsage lines
  for (index = 0; index < teleinfo_message.line_max; index ++)
    WSContentSend_P (PSTR ("<tr id='l%d'><td id='e%d'>%s</td><td id='d%d'>%s</td><td id='c%d'>%c</td></tr>\n"), index + 1, index + 1, teleinfo_message.line[index].etiquette.c_str (), index + 1, teleinfo_message.line[index].donnee.c_str (), index + 1, teleinfo_message.line[index].checksum);

  // end of table and end of page
  WSContentSend_P (PSTR ("</table></div>\n"));
  WSContentSend_P (PSTR ("<div>\n"));
  WSContentStop ();
}

// Apparent power graph frame
void TeleinfoWebGraphFrame ()
{
  int      graph_period = TELEINFO_PERIOD_LIVE;  
  int      index, phase, graph_left, graph_right, graph_width;
  uint16_t power_papp;
  char     srt_argument[4];

  // check graph period to be displayed
  if (Webserver->hasArg(D_CMND_TELEINFO_PERIOD))
  {
    // set graph period
    WebGetArg (D_CMND_TELEINFO_PERIOD, srt_argument, sizeof(srt_argument));
    graph_period = atoi (srt_argument);
    if ((graph_period < 0) || (graph_period >= TELEINFO_PERIOD_MAX)) graph_period = TELEINFO_PERIOD_LIVE;
  }

  // loop thru phasis and power records to calculate max power
  teleinfo_graph[graph_period].pmax = teleinfo_contract.ssousc;
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
    for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
    {
      // update max power during the period
      power_papp = teleinfo_graph[graph_period].arr_papp[phase][index];
      if ((power_papp != UINT16_MAX) && (power_papp > teleinfo_graph[graph_period].pmax )) teleinfo_graph[graph_period].pmax = power_papp;
    }

  // boundaries of SVG graph
  graph_left  = TELEINFO_GRAPH_PERCENT_START * TELEINFO_GRAPH_WIDTH / 100;
  graph_right = TELEINFO_GRAPH_PERCENT_STOP * TELEINFO_GRAPH_WIDTH / 100;
  graph_width = graph_right - graph_left;

  // start of SVG graph
  WSContentBegin(200, CT_HTML);
  WSContentSend_P (PSTR ("<svg viewBox='%d %d %d %d' preserveAspectRatio='xMinYMin meet'>\n"), 0, 0, TELEINFO_GRAPH_WIDTH, TELEINFO_GRAPH_HEIGHT);

  // SVG style 
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));
  WSContentSend_P (PSTR ("rect {stroke:grey;fill:none;}\n"));
  WSContentSend_P (PSTR ("line.dash {stroke:grey;stroke-width:1;stroke-dasharray:8;}\n"));
  WSContentSend_P (PSTR ("line.time {stroke:white;stroke-width:1;}\n"));
  WSContentSend_P (PSTR ("text.power {font-size:20px;stroke:white;fill:white;}\n"));
  WSContentSend_P (PSTR ("text.time {font-size:16px;fill:white;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // graph frame
  WSContentSend_P (PSTR ("<rect x='%d%%' y='%d%%' width='%d%%' height='%d%%' rx='10' />\n"), TELEINFO_GRAPH_PERCENT_START, 0, TELEINFO_GRAPH_PERCENT_STOP - TELEINFO_GRAPH_PERCENT_START, 100);

  // graph separation lines
  WSContentSend_P (TELEINFO_HTML_DASH, TELEINFO_GRAPH_PERCENT_START, 25, TELEINFO_GRAPH_PERCENT_STOP, 25);
  WSContentSend_P (TELEINFO_HTML_DASH, TELEINFO_GRAPH_PERCENT_START, 50, TELEINFO_GRAPH_PERCENT_STOP, 50);
  WSContentSend_P (TELEINFO_HTML_DASH, TELEINFO_GRAPH_PERCENT_START, 75, TELEINFO_GRAPH_PERCENT_STOP, 75);

  // power units
  WSContentSend_P (PSTR ("<text class='power' x='%d%%' y='%d%%'>VA</text>\n"), TELEINFO_GRAPH_PERCENT_STOP + 2, 4);
  WSContentSend_P (TELEINFO_HTML_POWER, 1, 4,  teleinfo_graph[graph_period].pmax);
  WSContentSend_P (TELEINFO_HTML_POWER, 1, 26, teleinfo_graph[graph_period].pmax * 3 / 4);
  WSContentSend_P (TELEINFO_HTML_POWER, 1, 51, teleinfo_graph[graph_period].pmax / 2);
  WSContentSend_P (TELEINFO_HTML_POWER, 1, 76, teleinfo_graph[graph_period].pmax / 4);
  WSContentSend_P (TELEINFO_HTML_POWER, 1, 99, 0);

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentEnd();
}

// get status update
void TeleinfoWebGraphUpdate ()
{
  int      graph_period = TELEINFO_PERIOD_LIVE;
  int      index;
  long     power_diff;
  uint32_t time_now;
  String   str_text;
  char     srt_argument[4];

  // check graph period to be displayed
  if (Webserver->hasArg(D_CMND_TELEINFO_PERIOD))
  {
    // set graph period
    WebGetArg (D_CMND_TELEINFO_PERIOD, srt_argument, sizeof(srt_argument));
    graph_period = atoi (srt_argument);
    if ((graph_period < 0) || (graph_period >= TELEINFO_PERIOD_MAX)) graph_period = TELEINFO_PERIOD_LIVE;
  }

  // timestamp
  time_now = millis ();

  // chech for graph update
  str_text = teleinfo_graph[graph_period].updated;

  // check power and power difference for each phase
  for (index = 0; index < teleinfo_contract.phase; index++)
  {
    // calculate power difference
    power_diff = teleinfo_phase[index].papp - teleinfo_phase[index].last_papp;

    // if power has changed
    str_text += ";";
    if (power_diff != 0)
    {
      teleinfo_phase[index].last_papp = teleinfo_phase[index].papp;
      str_text += teleinfo_phase[index].papp;
    }

    // if more than 100 VA power difference
    str_text += ";";
    if (abs (power_diff) >= 100)
    {
      if (power_diff < 0)
      { 
        str_text += "- ";
        str_text += abs (power_diff);
      }
      else
      {
        str_text += "+ ";
        str_text += power_diff;
      }
      teleinfo_phase[index].last_diff = millis ();
    }

    // else if diff has been published 10 sec
    else if ((teleinfo_phase[index].last_diff != UINT32_MAX) && (time_now - teleinfo_phase[index].last_diff > 10000))
    {
      str_text += "---";
      teleinfo_phase[index].last_diff = UINT32_MAX;
    }
  }

  // send result
  Webserver->send_P (200, PSTR ("text/plain"), str_text.c_str (), str_text.length ());

  // reset update flags
  teleinfo_graph[graph_period].updated = 0;
}

// Apparent power graph curve
void TeleinfoWebGraphData ()
{
  int      index, phase, index_array;
  int      graph_period = TELEINFO_PERIOD_LIVE;  
  uint16_t power_papp;
  long     graph_left, graph_right, graph_width;  
  long     graph_x, graph_y;  
  long     unit_width, shift_unit, shift_width;  
  TIME_T   current_dst;
  uint32_t current_time;
  char     srt_argument[4];

  // check graph period to be displayed
  if (Webserver->hasArg(D_CMND_TELEINFO_PERIOD))
  {
    // set graph period
    WebGetArg (D_CMND_TELEINFO_PERIOD, srt_argument, sizeof(srt_argument));
    graph_period = atoi (srt_argument);
    if ((graph_period < 0) || (graph_period >= TELEINFO_PERIOD_MAX)) graph_period = TELEINFO_PERIOD_LIVE;
  }

  // boundaries of SVG graph
  graph_left  = TELEINFO_GRAPH_PERCENT_START * TELEINFO_GRAPH_WIDTH / 100;
  graph_right = TELEINFO_GRAPH_PERCENT_STOP * TELEINFO_GRAPH_WIDTH / 100;
  graph_width = graph_right - graph_left;

  // start of SVG graph
  WSContentBegin(200, CT_HTML);
  WSContentSend_P (PSTR ("<svg viewBox='%d %d %d %d' preserveAspectRatio='xMinYMin meet'>\n"), 0, 0, TELEINFO_GRAPH_WIDTH, TELEINFO_GRAPH_HEIGHT);

  // SVG style 
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));
  WSContentSend_P (PSTR ("polyline {fill:none;stroke-width:2;}\n"));
  for (index = 0; index < teleinfo_contract.phase; index++) WSContentSend_P (PSTR ("polyline.ph%d {stroke:%s;}\n"), index, arr_phase_color[index]);
  WSContentSend_P (PSTR ("line.time {stroke:white;stroke-width:1;}\n"));
  WSContentSend_P (PSTR ("text.time {font-size:16px;fill:grey;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // -----------------
  //   Power curves
  // -----------------

  // if a maximum power is defined, loop thru phasis
  if (teleinfo_graph[graph_period].pmax > 0)
  {
    for (phase = 0; phase < teleinfo_contract.phase; phase++)
    {
      // loop for the apparent power graph
      WSContentSend_P (PSTR ("<polyline class='%s' points='"), arr_phase_id[phase]);
      for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
      {
        // get target power value
        index_array = (index + teleinfo_graph[graph_period].index) % TELEINFO_GRAPH_SAMPLE;
        power_papp = teleinfo_graph[graph_period].arr_papp[phase][index_array];

        // if power is defined
        if (power_papp != UINT16_MAX)
        {
          // calculate current position and add the point to the line
          graph_x = graph_left + (graph_width * index / TELEINFO_GRAPH_SAMPLE);
          graph_y = TELEINFO_GRAPH_HEIGHT - (power_papp * TELEINFO_GRAPH_HEIGHT / teleinfo_graph[graph_period].pmax);
          WSContentSend_P (PSTR("%d,%d "), graph_x, graph_y);
        }
      }
      WSContentSend_P (PSTR("'/>\n"));
    }
  }

  // ------------
  //     Time
  // ------------

  // get current time
  current_time = LocalTime();
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
      current_time -= 1500 + (shift_unit * 60); 

      // display 5 mn separation lines with time
      for (index = 0; index < 6; index++)
      {
        // convert back to date and increase time of 5mn
        BreakTime (current_time, current_dst);
        current_time += 300;

        // display separation line and time
        graph_x = graph_left + shift_width + (index * unit_width);
        WSContentSend_P (PSTR ("<line class='time' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n"), graph_x, 49, graph_x, 51);
        WSContentSend_P (PSTR ("<text class='time' x='%d' y='%d%%'>%02dh%02d</text>\n"), graph_x - 25, 55, current_dst.hour, current_dst.minute);
      }
      break;

    case TELEINFO_PERIOD_DAY:
      // calculate horizontal shift
      unit_width  = graph_width / 6;
      shift_unit  = current_dst.hour % 4;
      shift_width = unit_width - (unit_width * shift_unit / 4) - (unit_width * current_dst.minute / 240);

      // calculate first time displayed by substracting (5 * 4h + shift) to current time
      current_time -= 72000 + (shift_unit * 3600); 

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
  WSContentEnd();
}

// Graph public page
void TeleinfoWebPageGraph ()
{
  int     graph_period = TELEINFO_PERIOD_LIVE;  
  int     index, idx_param;
  char    str_period[8], str_tab[8];
  char    srt_argument[8];
  String  str_text;

  // check graph period to be displayed
  if (Webserver->hasArg(D_CMND_TELEINFO_PERIOD))
  {
    // set graph period
    WebGetArg (D_CMND_TELEINFO_PERIOD, srt_argument, sizeof(srt_argument));
    graph_period = atoi (srt_argument);
    if ((graph_period < 0) || (graph_period >= TELEINFO_PERIOD_MAX)) graph_period = TELEINFO_PERIOD_LIVE;
  }
 
  // beginning of form without authentification
  WSContentStart_P (D_TELEINFO_GRAPH, false);
  WSContentSend_P (PSTR ("</script>\n"));

  // page data refresh script
  WSContentSend_P (PSTR ("<script type='text/javascript'>\n"));
  WSContentSend_P (PSTR ("function update() {\n"));
  WSContentSend_P (PSTR ("httpUpd=new XMLHttpRequest();\n"));
  WSContentSend_P (PSTR ("httpUpd.onreadystatechange=function() {\n"));
  WSContentSend_P (PSTR (" if (httpUpd.responseText.length>0) {\n"));
  WSContentSend_P (PSTR ("  arr_param=httpUpd.responseText.split(';');\n"));
  WSContentSend_P (PSTR ("  str_random=Math.floor(Math.random()*100000);\n"));
  WSContentSend_P (PSTR ("  if (arr_param[0]==1) {document.getElementById('data').data='%s?%s=%d&rnd='+str_random;}\n"), D_TELEINFO_PAGE_DATA_SVG, D_CMND_TELEINFO_PERIOD, graph_period);
  for (index = 0; index < teleinfo_contract.phase; index++)
  {
    idx_param = (2 * index) + 1;
    WSContentSend_P (PSTR ("  if (arr_param[%d]!='') {document.getElementById('ph%d').innerHTML=arr_param[%d]+' VA';}\n"), idx_param, index, idx_param);
  }
  WSContentSend_P (PSTR (" }\n"));
  WSContentSend_P (PSTR ("}\n"));
  WSContentSend_P (PSTR ("httpUpd.open('GET','%s?%s=%d',true);\n"), D_TELEINFO_PAGE_GRAPH_UPD, D_CMND_TELEINFO_PERIOD, graph_period);
  WSContentSend_P (PSTR ("httpUpd.send();\n"));
  WSContentSend_P (PSTR ("}\n"));
  WSContentSend_P (PSTR ("setInterval(function() {update();},1000);\n"));
  WSContentSend_P (PSTR ("</script>\n"));

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {width:100%%;margin:12px auto;padding:3px 0px;text-align:center;vertical-align:middle;}\n"));
  WSContentSend_P (PSTR (".title {font-size:2rem;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.papp {display:inline-block;width:auto;padding:0.25rem 0.75rem;margin:0.5rem;border-radius:8px;}\n"));
  WSContentSend_P (PSTR ("div.papp span.power {font-size:2rem;}\n"));
  WSContentSend_P (PSTR ("div.papp span.diff {font-size:1.5rem;font-style:italic;}\n"));
  for (index = 0; index < teleinfo_contract.phase; index++)
  {
    WSContentSend_P (PSTR ("div.ph%d {color:%s;border:1px %s solid;}\n"), index, arr_phase_color[index], arr_phase_color[index]);
    WSContentSend_P (PSTR ("div.ph%d span {color:%s;}\n"), index, arr_phase_color[index]);
  }
  WSContentSend_P (PSTR (".button {font-size:1.2rem;padding:0.5rem 1rem;border:1px #666 solid;background:none;color:#fff;border-radius:8px;}\n"));
  WSContentSend_P (PSTR (".active {background:#666;}\n"));
  WSContentSend_P (PSTR (".svg-container {position:relative;vertical-align:middle;overflow:hidden;width:100%%;max-width:%dpx;padding-bottom:65%%;}\n"), TELEINFO_GRAPH_WIDTH);
  WSContentSend_P (PSTR (".svg-content {display:inline-block;position:absolute;top:0;left:0;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // page body
  WSContentSend_P (PSTR ("</head>\n"));
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (TELEINFO_FORM_START, D_TELEINFO_PAGE_GRAPH);

  // room name
  WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText(SET_DEVICENAME));

  // display icon
  WSContentSend_P (PSTR ("<div><a href='/'><img height=64 src='%s'></a></div>\n"), D_TELEINFO_PAGE_TIC_PNG);

  // display values
  WSContentSend_P (PSTR ("<div>\n"));
  for (index = 0; index < teleinfo_contract.phase; index++)
  {
    str_text = String (Energy.apparent_power[index], 0);
    WSContentSend_P (PSTR ("<div class='papp %s'><span class='power' id='%s'>%s VA</span><br><span class='diff' id='d%s'>---</span></div>\n"), arr_phase_id[index], arr_phase_id[index], str_text.c_str (), arr_phase_id[index]);
  } 
  WSContentSend_P (PSTR ("</div>\n"));

  // display tabs
  WSContentSend_P (PSTR ("<div>\n"));
  for (index = 0; index < TELEINFO_PERIOD_MAX; index++)
  {
    // get active tab and period label
    if (graph_period == index) strcpy (str_tab, "active"); else strcpy (str_tab, "");
    GetTextIndexed(str_period, sizeof(str_period), index, kTeleinfoGraphPeriod);

    // display tab
    WSContentSend_P (PSTR ("<button name='%s' value='%d' class='button %s'>%s</button>\n"), D_CMND_TELEINFO_PERIOD, index, str_tab, str_period);
  }
  WSContentSend_P (PSTR ("</div>\n"));

  // display graph base and data
  WSContentSend_P (PSTR ("<div class='svg-container'>\n"));
  WSContentSend_P (PSTR ("<object class='svg-content' id='base' type='image/svg+xml' width='%d%%' height='%d%%' data='%s?%s=%d'></object>\n"), 100, 100, D_TELEINFO_PAGE_BASE_SVG, D_CMND_TELEINFO_PERIOD, graph_period);
  WSContentSend_P (PSTR ("<object class='svg-content' id='data' type='image/svg+xml' width='%d%%' height='%d%%' data='%s?%s=%d&ts=0'></object>\n"), 100, 100, D_TELEINFO_PAGE_DATA_SVG, D_CMND_TELEINFO_PERIOD, graph_period);
  WSContentSend_P (PSTR ("</div>\n"));

  // end of page
  WSContentSend_P (TELEINFO_FORM_STOP);
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
bool Xsns15 (uint8_t function)
{
  String str_url;

  // swtich according to context
  switch (function) 
  {
    case FUNC_INIT:
      TeleinfoGraphInit ();
      break;
    case FUNC_EVERY_250_MSECOND:
      if (teleinfo_enabled && (TasmotaGlobal.uptime > 4)) TeleinfoEvery250ms ();
      break;
    case FUNC_EVERY_SECOND:
      TeleinfoEverySecond ();
      break;
    case FUNC_JSON_APPEND:
      if (teleinfo_enabled) TeleinfoShowJSON (true);
      break;
#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      // pages
      Webserver->on (FPSTR (D_TELEINFO_PAGE_CONFIG), TeleinfoWebPageConfig);
      Webserver->on (FPSTR (D_TELEINFO_PAGE_TIC),    TeleinfoWebPageTic);
      Webserver->on (FPSTR (D_TELEINFO_PAGE_DATA),   TeleinfoWebPageData);

      // graph
      Webserver->on (FPSTR (D_TELEINFO_PAGE_GRAPH),    TeleinfoWebPageGraph);
      Webserver->on (FPSTR (D_TELEINFO_PAGE_BASE_SVG), TeleinfoWebGraphFrame);
      Webserver->on (FPSTR (D_TELEINFO_PAGE_DATA_SVG), TeleinfoWebGraphData);
      
      // update status
      Webserver->on (FPSTR (D_TELEINFO_PAGE_GRAPH_UPD), TeleinfoWebGraphUpdate);
      Webserver->on (FPSTR (D_TELEINFO_PAGE_TIC_UPD),   TeleinfoWebTicUpdate);

      // icons
      Webserver->on (FPSTR (D_TELEINFO_PAGE_TIC_PNG), TeleinfoWebIconTic);
      break;
    case FUNC_WEB_ADD_BUTTON:
      TeleinfoWebButton ();
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
