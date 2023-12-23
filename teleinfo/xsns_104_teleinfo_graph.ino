/*
  xsns_104_teleinfo.ino - France Teleinfo energy sensor support for Tasmota devices

  Copyright (C) 2019-2023  Nicolas Bernaerts

  Version history :
    28/02/2023 - v11.0 - Split between xnrg and xsns
    03/06/2023 - v11.1 - Graph curves live update
    11/06/2023 - v11.2 - Change graph organisation & live display
    15/08/2023 - v11.3 - Evolution in graph navigation
                         Change in XMLHttpRequest management
                         Memory leak hunting
    25/08/2023 - v11.4 - Add serial reception in display loops to avoid errors
    20/10/2023 - v12.1 - Separate graphs
                         Add production graph
    07/11/2023 - v12.2 - Remove daily graph to save RAM on ESP8266 1Mb
                         Change daily filename to teleinfo-day-00.csv
    19/11/2023 - v13.0 - Migrate all web from xnrg_15_teleinfo
    19/12/2023 - v13.3 - Handle Tempo and EJP from STGE

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

#ifdef USE_TELEINFO
#ifdef USE_TELEINFO_GRAPH

/*************************************************\
 *               Variables
\*************************************************/

// declare teleinfo energy driver and sensor
#define XSNS_104   104

// graph data
#define TELEINFO_GRAPH_SAMPLE           300       // number of samples per graph data
#define TELEINFO_GRAPH_WIDTH            1200      // graph width
#define TELEINFO_GRAPH_HEIGHT           600       // default graph height
#define TELEINFO_GRAPH_STEP             100       // graph height mofification step
#define TELEINFO_GRAPH_PERCENT_START    5         // start position of graph window
#define TELEINFO_GRAPH_PERCENT_STOP     95        // stop position of graph window
#define TELEINFO_GRAPH_MAX_BARGRAPH     32        // maximum number of bar graph
#define TELEINFO_HISTO_BUFFER_MAX       1024      // history file buffer

// colors for conso and prod
#define TELEINFO_COLOR_PROD             "#1c0"
#define TELEINFO_COLOR_PROD_PREV        "#160"
#define TELEINFO_COLOR_CONSO            "#6cf"
#define TELEINFO_COLOR_CONSO_PREV       "#069"

// web URL
const char D_TELEINFO_ICON_LINKY_PNG[]   PROGMEM = "/linky.png";
const char D_TELEINFO_PAGE_TIC[]         PROGMEM = "/tic";
const char D_TELEINFO_PAGE_TIC_UPD[]     PROGMEM = "/tic.upd";
const char D_TELEINFO_PAGE_GRAPH[]       PROGMEM = "/graph";
const char D_TELEINFO_PAGE_GRAPH_CONSO[] PROGMEM = "/conso";
const char D_TELEINFO_PAGE_GRAPH_PROD[]  PROGMEM = "/prod";
const char D_TELEINFO_PAGE_GRAPH_DATA[]  PROGMEM = "/data.upd";
const char D_TELEINFO_PAGE_GRAPH_CURVE[] PROGMEM = "/curve.upd";

#ifdef USE_UFILESYS

// configuration and history files
const char D_TELEINFO_HISTO_FILE_DAY[]  PROGMEM = "/teleinfo-day-%02u.csv";
const char D_TELEINFO_HISTO_FILE_WEEK[] PROGMEM = "/teleinfo-week-%02u.csv";
const char D_TELEINFO_HISTO_FILE_YEAR[] PROGMEM = "/teleinfo-year-%04d.csv";
const char D_TELEINFO_HISTO_FILE_PROD[] PROGMEM = "/production-year-%04d.csv";
const char D_TELEINFO_HEADER_YEAR[] PROGMEM = "Idx;Month;Day;Global;Daily;00h;01h;02h;03h;04h;05h;06h;07h;08h;09h;10h;11h;12h;13h;14h;15h;16h;17h;18h;19h;20h;21h;22h;23h";

// graph - periods
enum TeleinfoGraphPeriod { TELEINFO_PERIOD_LIVE, TELEINFO_PERIOD_DAY, TELEINFO_PERIOD_WEEK, TELEINFO_PERIOD_YEAR, TELEINFO_PERIOD_PROD, TELEINFO_PERIOD_MAX };    // available graph periods
const char kTeleinfoGraphPeriod[] PROGMEM = "Live|Day|Week|Year|Prod";                                                                                            // period labels
const long ARR_TELEINFO_PERIOD_WINDOW[] = { 300 / TELEINFO_GRAPH_SAMPLE, 86400 / TELEINFO_GRAPH_SAMPLE, 604800 / TELEINFO_GRAPH_SAMPLE, 1, 1, 1 };                // time window between samples (sec.)

#else

// graph - periods
enum TeleinfoGraphPeriod { TELEINFO_PERIOD_LIVE, TELEINFO_PERIOD_MAX };                                           // available graph periods
const char kTeleinfoGraphPeriod[] PROGMEM = "Live";                                                               // period labels
const long ARR_TELEINFO_PERIOD_WINDOW[] = { 300 / TELEINFO_GRAPH_SAMPLE, 1 };                                     // time window between samples (sec.)

#endif    // USE_UFILESYS

// graph - display
enum TeleinfoGraphDisplay { TELEINFO_UNIT_VA, TELEINFO_UNIT_W, TELEINFO_UNIT_V, TELEINFO_UNIT_COSPHI, TELEINFO_UNIT_PEAK_VA, TELEINFO_UNIT_PEAK_V, TELEINFO_UNIT_WH, TELEINFO_UNIT_MAX };       // available graph units
const char kTeleinfoGraphDisplay[] PROGMEM = "VA|W|V|cosφ|VA|V|Wh";                                                                                                                             // units labels

// week days name for history
static const char kWeekdayNames[] = D_DAY3LIST;

// phase colors
const char kTeleinfoColorPhase[] PROGMEM = "#09f|#f90|#093";                   // phase colors (blue, orange, green)
const char kTeleinfoColorPeak[]  PROGMEM = "#5ae|#eb6|#2a6";                   // peak colors (blue, orange, green)

// contract colors
const char kTeleinfoTempoDot[]   PROGMEM = "⚪|⚪|⚫|⚪";
const char kTeleinfoTempoColor[] PROGMEM = "#252525|#06b|#ccc|#b00";

/****************************************\
 *                 Data
\****************************************/

// teleinfo : calculation periods data
struct tic_period {
  bool     updated;                                 // flag to ask for graph update
  int      index;                                   // current array index per refresh period (day of year for yearly period)
  long     counter;                                 // counter in seconds of current refresh period (10k*year + 100*month + day_of_month for yearly period)

  long  volt_low[ENERGY_MAX_PHASES];                // lowest voltage during refresh period
  long  volt_peak[ENERGY_MAX_PHASES];               // peak high voltage during refresh period
  long  papp_peak[ENERGY_MAX_PHASES];               // peak apparent power during refresh period
  long  long papp_sum[ENERGY_MAX_PHASES];           // sum of apparent power during refresh period
  long  long pact_sum[ENERGY_MAX_PHASES];           // sum of active power during refresh period
  float cosphi_sum[ENERGY_MAX_PHASES];              // sum of cos phi during refresh period

  String str_filename;                              // log filename
  String str_buffer;                                // buffer of data waiting to be logged
}; 
static tic_period teleinfo_period[TELEINFO_PERIOD_MAX];

// teleinfo : graph data
struct tic_graph {
  uint8_t arr_papp[ENERGY_MAX_PHASES][TELEINFO_GRAPH_SAMPLE];     // array of apparent power graph values
  uint8_t arr_pact[ENERGY_MAX_PHASES][TELEINFO_GRAPH_SAMPLE];     // array of active power graph values
  uint8_t arr_volt[ENERGY_MAX_PHASES][TELEINFO_GRAPH_SAMPLE];     // array min and max voltage delta
  uint8_t arr_cosphi[ENERGY_MAX_PHASES][TELEINFO_GRAPH_SAMPLE];   // array of cos phi
};
struct tic_daily {
  long      hour_wh[24];                          // hourly increments
  long long last_day_wh  = 0;                     // previous daily total
  long long last_hour_wh = 0;                     // previous hour total
};

static struct {
  bool    serving      = false;                   // flag set when serving graph page
  uint8_t period_curve = 0;                       // graph max period to display curve
  uint8_t period       = TELEINFO_PERIOD_LIVE;    // graph period
  uint8_t data         = TELEINFO_UNIT_VA;        // graph default data
  uint8_t histo        = 0;                       // graph histotisation index
  uint8_t month        = 0;                       // graph current month
  uint8_t day          = 0;                       // graph current day
  uint8_t last_hour    = UINT8_MAX;               // last hour slot

  long    left;                                   // left position of the curve
  long    right;                                  // right position of the curve
  long    width;                                  // width of the curve (in pixels)

  long    papp_peak[ENERGY_MAX_PHASES];           // peak apparent power to save
  long    volt_low[ENERGY_MAX_PHASES];            // voltage to save
  long    volt_peak[ENERGY_MAX_PHASES];           // voltage to save
  long    papp[ENERGY_MAX_PHASES];                // apparent power to save
  long    pact[ENERGY_MAX_PHASES];                // active power to save
  uint8_t cosphi[ENERGY_MAX_PHASES];              // cosphi to save

  char    str_phase[ENERGY_MAX_PHASES + 1];       // phases to displayed
  char    str_buffer[512];                        // buffer used for graph display 

  tic_daily conso;                                // conso daily data
  tic_daily prod;                                 // prod daily data

  tic_graph data_live;                            // live last 10mn values

#ifdef USE_UFILESYS
  uint8_t rotate_daily  = 0;                      // index of daily file to rotate
  uint8_t rotate_weekly = 0;                      // index of weekly file to rotate
#endif    // USE_UFILESYS

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
void TeleinfoSensorLinkyIcon ()
{ 
  Webserver->send_P (200, PSTR ("image/png"), teleinfo_icon_linky_png, teleinfo_icon_linky_png_size);
}

#endif      // USE_WEBSERVER

/*********************************************\
 *               Functions
\*********************************************/

// calculate number of days in given month
uint8_t TeleinfoSensorGetDaysInMonth (const uint32_t year, const uint8_t month)
{
  uint8_t result;

  // calculate number of days in given month
  if ((month == 4) || (month == 6) || (month == 9 || (month == 11))) result = 30;         // months with 30 days  
  else if (month != 2) result = 31;                                                       // months with 31 days
  else if ((year % 400) == 0) result = 29;                                                // leap year
  else if ((year % 100) == 0) result = 28;                                                // not a leap year
  else if ((year % 4) == 0) result = 29;                                                  // leap year
  else result = 28;                                                                       // not a leap year
  
  return result;
}

// Display any value to a string with unit and kilo conversion with number of digits (12000, 2.0k, 12.0k, 2.3M, ...)
void TeleinfoSensorGetDataDisplay (const int unit_type, const long value, char* pstr_result, const int size_result, bool in_kilo) 
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

// Generate current counter values as a string with unit and kilo conversion
void TeleinfoSensorGetCurrentDataDisplay (const int unit_type, const int phase, char* pstr_result, const int size_result) 
{
  int  unit  = unit_type;
  long value = LONG_MAX;

  // handle Wh as W
  if (unit == TELEINFO_UNIT_WH) unit = TELEINFO_UNIT_W;

  // set curve value according to displayed data
  switch (unit) 
  {
    case TELEINFO_UNIT_W:
      value = teleinfo_conso.phase[phase].pact;
      break;
    case TELEINFO_UNIT_VA:
      value = teleinfo_conso.phase[phase].papp;
      break;
    case TELEINFO_UNIT_V:
      value = teleinfo_conso.phase[phase].voltage;
      break;
    case TELEINFO_UNIT_COSPHI:
      value = teleinfo_conso.phase[phase].cosphi;
      break;
  }

  // convert value for display
  TeleinfoSensorGetDataDisplay (unit, value, pstr_result, size_result, false);
}

/*********************************************\
 *               Historisation
\*********************************************/

#ifdef USE_UFILESYS

// Get historisation period start time
uint32_t TeleinfoSensorHistoGetStartTime (const uint8_t period, const uint8_t histo)
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
bool TeleinfoSensorHistoGetDate (const int period, const int histo, char* pstr_text)
{
  uint32_t calc_time;
  TIME_T   start_dst;

  // check parameters and init
  if (pstr_text == nullptr) return false;

  // start date
  calc_time = TeleinfoSensorHistoGetStartTime (period, histo);
  BreakTime (calc_time, start_dst);

  // set label according to period
  switch (period)
  {
    // generate time label for day graph
    case TELEINFO_PERIOD_DAY:
      sprintf (pstr_text, "%02u/%02u", start_dst.day_of_month, start_dst.month);
      break;

    // generate time label for week graph
    case TELEINFO_PERIOD_WEEK:
      sprintf (pstr_text, "W %02u/%02u", start_dst.day_of_month, start_dst.month);
      break;

    // generate time label for year graph
    case TELEINFO_PERIOD_YEAR:
    case TELEINFO_PERIOD_PROD:
      sprintf (pstr_text, "%04u", 1970 + start_dst.year - histo);
      break;

    // default
    default:
      strcpy (pstr_text, "");
      break;
  }

  return (strlen (pstr_text) > 0);
}

// Get historisation filename
bool TeleinfoSensorHistoGetFilename (const uint8_t period, const uint8_t histo, char* pstr_filename)
{
  bool exists = false;

  // check parameters
  if (pstr_filename == nullptr) return false;

  // generate filename according to period
  strcpy (pstr_filename, "");
  if (period == TELEINFO_PERIOD_DAY) sprintf_P (pstr_filename, D_TELEINFO_HISTO_FILE_DAY, histo);
  else if (period == TELEINFO_PERIOD_WEEK) sprintf_P (pstr_filename, D_TELEINFO_HISTO_FILE_WEEK, histo);
  else if (period == TELEINFO_PERIOD_YEAR) sprintf_P (pstr_filename, D_TELEINFO_HISTO_FILE_YEAR, RtcTime.year - (uint16_t)histo);
  else if (period == TELEINFO_PERIOD_PROD) sprintf_P (pstr_filename, D_TELEINFO_HISTO_FILE_PROD, RtcTime.year - (uint16_t)histo);

  // if filename defined, check existence
  if (strlen (pstr_filename) > 0) exists = ffsp->exists (pstr_filename);

  return exists;
}

// Flush log data to littlefs
void TeleinfoSensorFlushDataHisto (const uint8_t period)
{
  uint8_t phase, index;
  char    str_value[32];
  String  str_header;
  File    file;

  // validate parameters
  if ((period != TELEINFO_PERIOD_DAY) && (period != TELEINFO_PERIOD_WEEK)) return;

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
      for (phase = 1; phase <= teleinfo_contract.phase; phase++)
      {
        sprintf (str_value, ";VA%d;W%d;V%d;C%d;pVA%d;pV%d", phase, phase, phase, phase, phase, phase);
        str_header += str_value;
      }

      // append contract period and totals
      str_header += ";Period";
      if (teleinfo_contract.period_first < TIC_PERIOD_MAX) 
        for (index = teleinfo_contract.period_first; index < teleinfo_contract.period_first + teleinfo_contract.period_qty; index++)
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
}

// Save historisation data
void TeleinfoSensorSaveDataHisto (const uint8_t period)
{
  uint8_t  phase;
  long     year, month, remain;
  uint32_t start_time, current_time, day_of_week, index;
  TIME_T   time_dst, start_dst;
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
    start_dst = time_dst;
    start_dst.second = 1;
    start_dst.minute = 0;
    start_dst.hour   = 0;  
    if (start_dst.day_of_week == 1) day_of_week = 8; else day_of_week = start_dst.day_of_week;
    start_time = MakeTime (start_dst) - 86400 * (day_of_week - 2);

    // calculate index of weekly line
    index = (current_time - start_time) * TELEINFO_GRAPH_SAMPLE / 604800;
    if (index >= TELEINFO_GRAPH_SAMPLE) index = TELEINFO_GRAPH_SAMPLE - 1;
  }

  // if log file name has changed, flush data of previous file
  if ((teleinfo_period[period].str_filename != "") && (teleinfo_period[period].str_filename != str_filename)) TeleinfoSensorFlushDataHisto (period);

  // set new log filename
  teleinfo_period[period].str_filename = str_filename;

  // line : index and date
  teleinfo_period[period].str_buffer += index;
  sprintf (str_value, ";%02u/%02u;%02u:%02u", time_dst.day_of_month, time_dst.month, time_dst.hour, time_dst.minute);
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
  GetTextIndexed (str_value, sizeof (str_value), teleinfo_contract.period_current, kTeleinfoPeriod);
  teleinfo_period[period].str_buffer += ";";
  teleinfo_period[period].str_buffer += str_value;
  if (teleinfo_contract.period_qty < TIC_PERIOD_MAX) 
    for (index = 0; index < teleinfo_contract.period_qty; index++)
    {
      lltoa (teleinfo_conso.index_wh[index], str_value, 10); 
      teleinfo_period[period].str_buffer += ";";
      teleinfo_period[period].str_buffer += str_value;
    }

  // line : end
  teleinfo_period[period].str_buffer += "\n";

  // if log should be saved now
  if (teleinfo_period[period].str_buffer.length () > TELEINFO_HISTO_BUFFER_MAX) TeleinfoSensorFlushDataHisto (period);
}

// rotate file according to file naming convention
void TeleinfoSensorFileRotate (const char* pstr_filename, const uint8_t index) 
{
  char str_original[UFS_FILENAME_SIZE];
  char str_target[UFS_FILENAME_SIZE];

  // check parameter
  if (pstr_filename == nullptr) return;
  if (index == 0) return;

  // generate file names
  sprintf (str_original, pstr_filename, index - 1);
  sprintf (str_target, pstr_filename, index);

  // if target exists, remove it
  if (ffsp->exists (str_target)) ffsp->remove (str_target);

  // if file exists, rotate it
  if (ffsp->exists (str_original))
  {
    ffsp->rename (str_original, str_target);
    AddLog (LOG_LEVEL_INFO, PSTR ("UFS: database %s renamed to %s"), str_original, str_target);
  }
}

// Rotation of log files
void TeleinfoSensorMidnightRotate ()
{
  // rotate tempo data
  teleinfo_meter.tempo.yesterday = teleinfo_meter.tempo.today;
  teleinfo_meter.tempo.today     = teleinfo_meter.tempo.tomorrow;
  teleinfo_meter.tempo.tomorrow  = TIC_TEMPO_NONE;

  // flush daily file and prepare rotation of daily files
  TeleinfoSensorFlushDataHisto (TELEINFO_PERIOD_DAY);
  teleinfo_graph.rotate_daily  = (uint8_t)teleinfo_config.param[TELEINFO_CONFIG_NBDAY];
  AddLog (LOG_LEVEL_INFO, PSTR ("TIC: %u daily files will rotate"), teleinfo_graph.rotate_daily);

  // flush daily file and if we are on monday, prepare rotation of weekly files
  TeleinfoSensorFlushDataHisto (TELEINFO_PERIOD_WEEK);
  if (RtcTime.day_of_week == 2)
  {
    teleinfo_graph.rotate_weekly = (uint8_t)teleinfo_config.param[TELEINFO_CONFIG_NBWEEK];
    AddLog (LOG_LEVEL_INFO, PSTR ("TIC: %u weekly files will rotate"), teleinfo_graph.rotate_weekly);
  }
}

// Save historisation data
void TeleinfoSensorMidnightSaveTotal ()
{
  uint8_t   index;
  uint32_t  current_time;
  TIME_T    today_dst;
  long long delta;
  char      str_value[32];
  String    str_line;
  File      file;
  char      str_filename[UFS_FILENAME_SIZE];

  // if date not defined, cancel
  if (!RtcTime.valid) return;
  
  // calculate last day filename (shift 30s before)
  current_time = LocalTime () - 30;
  BreakTime (current_time, today_dst);

  // if daily conso is available
  if (teleinfo_conso.total_wh > 0)
  {
    // calculate daily ahd hourly delta
    delta = teleinfo_conso.total_wh - teleinfo_graph.conso.last_day_wh;
    teleinfo_graph.conso.hour_wh[today_dst.hour] += (long)(teleinfo_conso.total_wh - teleinfo_graph.conso.last_hour_wh);

    // update last day and hour total
    teleinfo_graph.conso.last_day_wh  = teleinfo_conso.total_wh;
    teleinfo_graph.conso.last_hour_wh = teleinfo_conso.total_wh;

    // get filename
    sprintf_P (str_filename, D_TELEINFO_HISTO_FILE_YEAR, 1970 + today_dst.year);

    // if file exists, open in append mode
    if (ffsp->exists (str_filename)) file = ffsp->open (str_filename, "a");

    // else create file and add header
    else
    {
      file = ffsp->open (str_filename, "w");
      sprintf_P (teleinfo_graph.str_buffer, "%s\n", D_TELEINFO_HEADER_YEAR);
      file.print (teleinfo_graph.str_buffer);
    }

    // generate today's line
    sprintf (str_value, "%u;%u;%u;", today_dst.day_of_year, today_dst.month, today_dst.day_of_month);
    str_line = str_value;
    lltoa (teleinfo_conso.total_wh, str_value, 10);
    str_line += str_value;
    str_line += ";";
    lltoa (delta, str_value, 10);
    str_line += str_value;

    // loop to add hourly totals
    for (index = 0; index < 24; index ++)
    {
      // append hourly increment to line
      str_line += ";";
      str_line += teleinfo_graph.conso.hour_wh[index];

      // reset hourly increment
      teleinfo_graph.conso.hour_wh[index] = 0;
    }

    // write line and close file
    str_line += "\n";
    file.print (str_line.c_str ());
    file.close ();
  }

  // if daily production is available
  if (teleinfo_prod.total_wh > 0)
  {
    // calculate daily ahd hourly delta
    delta = teleinfo_prod.total_wh - teleinfo_graph.prod.last_day_wh;
    teleinfo_graph.prod.hour_wh[today_dst.hour] += (long)(teleinfo_prod.total_wh - teleinfo_graph.prod.last_hour_wh);

    // update last day and hour total
    teleinfo_graph.prod.last_day_wh  = teleinfo_prod.total_wh;
    teleinfo_graph.prod.last_hour_wh = teleinfo_prod.total_wh;

    // get filename
    sprintf_P (str_filename, D_TELEINFO_HISTO_FILE_PROD, 1970 + today_dst.year);

    // if file exists, open in append mode
    if (ffsp->exists (str_filename)) file = ffsp->open (str_filename, "a");

    // else create file and add header
    else
    {
      file = ffsp->open (str_filename, "w");
      sprintf_P (teleinfo_graph.str_buffer, "%s\n", D_TELEINFO_HEADER_YEAR);
      file.print (teleinfo_graph.str_buffer);
    }

    // generate today's line
    sprintf (str_value, "%u;%u;%u;", today_dst.day_of_year, today_dst.month, today_dst.day_of_month);
    str_line = str_value;
    lltoa (teleinfo_prod.total_wh, str_value, 10);
    str_line += str_value;
    str_line += ";";
    lltoa (delta, str_value, 10);
    str_line += str_value;

    // loop to add hourly totals
    for (index = 0; index < 24; index ++)
    {
      // append hourly increment to line
      str_line += ";";
      str_line += teleinfo_graph.prod.hour_wh[index];

      // reset hourly increment
      teleinfo_graph.prod.hour_wh[index] = 0;
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
void TeleinfoSensorSaveDataPeriod (const uint8_t period)
{
  uint8_t phase;
  long    power;

  // if period out of range, return
  if (period >= TELEINFO_PERIOD_MAX) return;
  if (ARR_TELEINFO_PERIOD_WINDOW[period] == 0) return;

#ifdef USE_UFILESYS
  if (period < TELEINFO_PERIOD_YEAR)
#endif    // USE_UFILESYS

    // loop thru phases to update period totals  
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
      teleinfo_graph.cosphi[phase] = (uint8_t)(teleinfo_period[period].cosphi_sum[phase] / ARR_TELEINFO_PERIOD_WINDOW[period]);

      // reset period _
      teleinfo_period[period].cosphi_sum[phase] = 0;
    }

  // save to memory array
  TeleinfoSensorSaveDataMemory (period);

  // save to historisation file
#ifdef USE_UFILESYS
  TeleinfoSensorSaveDataHisto (period);
#endif    // USE_UFILESYS

  // data updated for period
  teleinfo_period[period].updated = true;
}

// Save current values to graph data
void TeleinfoSensorSaveDataMemory (const uint8_t period)
{
  uint8_t  phase;
  uint16_t cell_index;
  long     apparent, active, voltage;

  // check parameters
  if (period >= TELEINFO_PERIOD_MAX) return;
#ifdef USE_UFILESYS
  if (period != TELEINFO_PERIOD_LIVE) return;
#endif    // USE_UFILESYS

  // set array index and current index of cell in memory array
  cell_index = teleinfo_period[period].index;

  // loop thru phases
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    // calculate graph values
    voltage = 128 + teleinfo_graph.volt_low[phase] - TELEINFO_VOLTAGE;
    if (teleinfo_contract.ssousc > 0)
    {
      apparent = teleinfo_graph.papp[phase] * 200 / teleinfo_contract.ssousc;
      active   = teleinfo_graph.pact[phase] * 200 / teleinfo_contract.ssousc;
    } 
    else
    {
      apparent = UINT8_MAX;
      active   = UINT8_MAX;
    } 

    if (period == TELEINFO_PERIOD_LIVE)
    {
      teleinfo_graph.data_live.arr_papp[phase][cell_index]   = (uint8_t)apparent;
      teleinfo_graph.data_live.arr_pact[phase][cell_index]   = (uint8_t)active;
      teleinfo_graph.data_live.arr_volt[phase][cell_index]   = (uint8_t)voltage;
      teleinfo_graph.data_live.arr_cosphi[phase][cell_index] = teleinfo_graph.cosphi[phase];
    }
  }

  // increase data index in the graph and set update flag
  cell_index++;
  teleinfo_period[period].index = cell_index % TELEINFO_GRAPH_SAMPLE;
}

/*********************************************\
 *                   Callback
\*********************************************/

// Teleinfo module initialisation
void TeleinfoSensorModuleInit ()
{
  // disable wifi sleep mode
  Settings->flag5.wifi_no_sleep  = true;
  TasmotaGlobal.wifi_stay_asleep = false;
}

// Teleinfo graph data initialisation
void TeleinfoSensorInit ()
{
  uint32_t period, phase, index;

  // maximum period to display curve
#ifdef USE_UFILESYS
  teleinfo_graph.period_curve = TELEINFO_PERIOD_YEAR;
#else
  teleinfo_graph.period_curve = TELEINFO_PERIOD_MAX;
#endif      // USE_UFILESYS

  // init hourly values
  for (index = 0; index < 24; index ++)
  {
    teleinfo_graph.conso.hour_wh[index] = 0;
    teleinfo_graph.prod.hour_wh[index]  = 0;
  }

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
  }

  // initialise graph data
  strcpy (teleinfo_graph.str_phase, "");
  for (phase = 0; phase < ENERGY_MAX_PHASES; phase++)
  {
    // data of historisation files
    teleinfo_graph.papp[phase]      = 0;
    teleinfo_graph.pact[phase]      = 0;
    teleinfo_graph.volt_low[phase]  = LONG_MAX;
    teleinfo_graph.volt_peak[phase] = 0;
    teleinfo_graph.cosphi[phase]    = 100;

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
void TeleinfoSensorSaveBeforeRestart ()
{
  // update energy total (in kwh)
  EnergyUpdateToday ();

#ifdef USE_UFILESYS
  // flush logs
  TeleinfoSensorFlushDataHisto (TELEINFO_PERIOD_DAY);
  TeleinfoSensorFlushDataHisto (TELEINFO_PERIOD_WEEK);

  // save daily total
  TeleinfoSensorMidnightSaveTotal ();
#endif      // USE_UFILESYS
}

// Calculate if some JSON should be published (called every second)
void TeleinfoSensorEverySecond ()
{
  uint8_t  period, phase;

  // do nothing during energy setup time
  if (TasmotaGlobal.uptime < ENERGY_WATCHDOG) return;
  if (!RtcTime.valid) return;

  // if conso is defined
  if (teleinfo_conso.total_wh > 0)
  {
    // if needed, init data
    if (teleinfo_graph.conso.last_day_wh  == 0) teleinfo_graph.conso.last_day_wh  = teleinfo_conso.total_wh;
    if (teleinfo_graph.conso.last_hour_wh == 0) teleinfo_graph.conso.last_hour_wh = teleinfo_conso.total_wh;

    // if hour change, save hourly increment
    if (teleinfo_conso.total_wh > teleinfo_graph.conso.last_hour_wh)
    {
      teleinfo_graph.conso.hour_wh[RtcTime.hour] += (long)(teleinfo_conso.total_wh - teleinfo_graph.conso.last_hour_wh);
      teleinfo_graph.conso.last_hour_wh = teleinfo_conso.total_wh;
    }
  }

  // if production is defined
  if (teleinfo_prod.total_wh > 0)
  {
    // if needed, init data
    if (teleinfo_graph.prod.last_day_wh  == 0) teleinfo_graph.prod.last_day_wh  = teleinfo_prod.total_wh;
    if (teleinfo_graph.prod.last_hour_wh == 0) teleinfo_graph.prod.last_hour_wh = teleinfo_prod.total_wh;

    // if hour change, save hourly increment
    if (teleinfo_prod.total_wh > teleinfo_graph.prod.last_hour_wh)
    {
      teleinfo_graph.prod.hour_wh[RtcTime.hour] += (long)(teleinfo_prod.total_wh - teleinfo_graph.prod.last_hour_wh);
      teleinfo_graph.prod.last_hour_wh = teleinfo_prod.total_wh;
    }
  }

  // loop thru the periods and the phases, to update all values over the period
  for (period = 0; period < teleinfo_graph.period_curve; period++)
  {
    // loop thru phases to update max value
    for (phase = 0; phase < teleinfo_contract.phase; phase++)
    {
      // if within range, update phase apparent power
      if (teleinfo_conso.phase[phase].papp != LONG_MAX)
      {
        // add power to period total (for average calculation)
        teleinfo_period[period].pact_sum[phase]   += (long long)teleinfo_conso.phase[phase].pact;
        teleinfo_period[period].papp_sum[phase]   += (long long)teleinfo_conso.phase[phase].papp;
        teleinfo_period[period].cosphi_sum[phase] += teleinfo_conso.phase[phase].cosphi;

        // if voltage defined, update lowest and highest voltage level
        if (teleinfo_conso.phase[phase].voltage > 0)
        {
          if (teleinfo_conso.phase[phase].voltage < teleinfo_period[period].volt_low[phase])  teleinfo_period[period].volt_low[phase]  = teleinfo_conso.phase[phase].voltage;
          if (teleinfo_conso.phase[phase].voltage > teleinfo_period[period].volt_peak[phase]) teleinfo_period[period].volt_peak[phase] = teleinfo_conso.phase[phase].voltage;
        }       

        // update highest apparent power level
        if (teleinfo_conso.phase[phase].papp > teleinfo_period[period].papp_peak[phase]) teleinfo_period[period].papp_peak[phase] = teleinfo_conso.phase[phase].papp;
      }
    }

    // increment graph period counter and update graph data if needed
    teleinfo_period[period].counter ++;
    teleinfo_period[period].counter = teleinfo_period[period].counter % ARR_TELEINFO_PERIOD_WINDOW[period];
    if (teleinfo_period[period].counter == 0) TeleinfoSensorSaveDataPeriod (period);
  }

#ifdef USE_UFILESYS
  // if needed, rotate daily file
  if (teleinfo_graph.rotate_daily > 0) TeleinfoSensorFileRotate (D_TELEINFO_HISTO_FILE_DAY, teleinfo_graph.rotate_daily--);

  // else, if needed, rotate daily file
  else if (teleinfo_graph.rotate_weekly > 0) TeleinfoSensorFileRotate (D_TELEINFO_HISTO_FILE_WEEK, teleinfo_graph.rotate_weekly--);
#endif      // USE_UFILESYS
}

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

// Append Teleinfo state to main page
void TeleinfoSensorWebSensor ()
{
  bool      display_mode = true;
  uint8_t   index, phase, period, slot, color;
  long      value, red, green;
  long long total;
  char      str_color[24];
  char      str_text[64];

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
      // start
      WSContentSend_P (PSTR ("<div style='font-size:13px;text-align:center;padding:4px 6px;margin-bottom:5px;background:#333333;border-radius:12px;'>\n"));

      // title
      WSContentSend_P (PSTR ("<div style='display:flex;margin:2px 0px 6px 0px;padding:0px;font-size:16px;'>\n"));
      WSContentSend_P (PSTR ("<div style='width:28%%;padding:0px;text-align:left;font-weight:bold;'>Teleinfo</div>\n"));

      // over voltage
      if (teleinfo_meter.stge.overvolt != 0) { strcpy (str_color, "background:#d70;"); strcpy (str_text, "V"); }
        else { strcpy (str_color, ""); strcpy (str_text, ""); }
      WSContentSend_P (PSTR ("<div style='width:12%%;padding:2px 0px;border-radius:12px;font-size:12px;%s'>%s</div>\n"), str_color, str_text);

      // over load
      if (teleinfo_meter.stge.overvolt != 0) { strcpy (str_color, "background:#900;"); strcpy (str_text, "VA"); }
        else { strcpy (str_color, ""); strcpy (str_text, ""); }
      WSContentSend_P (PSTR ("<div style='width:12%%;padding:2px 0px;border-radius:12px;font-size:12px;%s'>%s</div>\n"), str_color, str_text);

      // status icons (EJP)
      if (teleinfo_meter.ejp.active != 0) { strcpy (str_color, "background:#900;"); strcpy (str_text, "EJP"); }
        else if (teleinfo_meter.ejp.preavis != 0) { strcpy (str_color, "background:#d70;"); strcpy (str_text, "EJP"); }
        else { strcpy (str_color, ""); strcpy (str_text, ""); }
      WSContentSend_P (PSTR ("<div style='width:16%%;padding:2px 0px;border-radius:12px;font-size:12px;%s'>%s</div>\n"), str_color, str_text);
      
      // contract supply
      if (teleinfo_contract.phase > 1) sprintf (str_text, "%ux", teleinfo_contract.phase); else strcpy (str_text, ""); 
      WSContentSend_P (PSTR ("<div style='width:20%%;padding:0px;text-align:right;font-weight:bold;'>%s%d</div>\n"), str_text, teleinfo_contract.ssousc / 1000);
      WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div><div style='width:10%%;padding:0px;text-align:left;'>kVA</div>\n"));
      WSContentSend_P (PSTR ("</div>\n"));

      // contract
      if (teleinfo_contract.period_qty <= TELEINFO_INDEX_MAX) 
        for (period = 0; period < teleinfo_contract.period_qty; period++)
          if (teleinfo_conso.index_wh[period] > 0)
          {
            WSContentSend_P (PSTR ("<div style='display:flex;padding:2px 0px;'>\n"));

            // linky mode
            if (display_mode)
            {
              GetTextIndexed (str_text, sizeof (str_text), teleinfo_contract.mode, kTeleinfoModeIcon);
              WSContentSend_P (PSTR ("<div style='width:16%%;padding:0px;font-size:16px;'>%s</div>\n"), str_text);
              display_mode = false;
            }
            else WSContentSend_P (PSTR ("<div style='width:16%%;padding:0px;'></div>\n"));

            // period name
            GetTextIndexed (str_text, sizeof (str_text), teleinfo_contract.period_first + period, kTeleinfoPeriodName);
            if (teleinfo_contract.period_current == teleinfo_contract.period_first + period) WSContentSend_P (PSTR ("<div style='width:31%%;padding:1px 0px;font-size:12px;background-color:#09f;border-radius:6px;'>%s</div>\n"), str_text);
              else WSContentSend_P (PSTR ("<div style='width:31%%;padding:0px;font-size:12px;'>%s</div>\n"), str_text);

            // counter value
            lltoa (teleinfo_conso.index_wh[period], str_text, 10);
            WSContentSend_P (PSTR ("<div style='width:41%%;padding:0px;text-align:right;'>%s</div>\n"), str_text);
            WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div><div style='width:10%%;padding:0px;text-align:left;'>Wh</div>\n"));
            WSContentSend_P (PSTR ("</div>\n"));
          }

      // production total
      if (teleinfo_prod.total_wh != 0)
      {
        WSContentSend_P (PSTR ("<div style='display:flex;padding:2px 0px;'>\n"));

        // period name
        WSContentSend_P (PSTR ("<div style='width:16%%;padding:0px;'></div>\n"));
        if (teleinfo_prod.papp == 0) WSContentSend_P (PSTR ("<div style='width:31%%;padding:0px;'>Production</div>\n"));
          else WSContentSend_P (PSTR ("<div style='width:31%%;padding:0px;background-color:#080;border-radius:6px;'>Production</div>\n"));

        // counter value
        lltoa (teleinfo_prod.total_wh, str_text, 10);
        WSContentSend_P (PSTR ("<div style='width:41%%;padding:0px;text-align:right;'>%s</div>\n"), str_text);
        WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div><div style='width:10%%;padding:0px;text-align:left;'>Wh</div>\n"));
        WSContentSend_P (PSTR ("</div>\n"));
      }

      // if meter is running consommation mode
      if (teleinfo_conso.total_wh > 0)
      {
        // consumption : separator and header
        WSContentSend_P (PSTR ("<hr>\n"));
        WSContentSend_P (PSTR ("<div style='padding:0px 0px 5px 0px;text-align:left;font-weight:bold;'>Consommation</div>\n"));

        // consumption : bar graph per phase
        if (teleinfo_contract.ssousc > 0)
          for (phase = 0; phase < teleinfo_contract.phase; phase++)
          {
            WSContentSend_P (PSTR ("<div style='display:flex;margin:2px 0px;padding:1px;height:16px;opacity:75%%;'>\n"));

            // display voltage
            if (teleinfo_meter.stge.overvolt == 1) strcpy (str_text, "font-weight:bold;color:red;"); else strcpy (str_text, "");
            WSContentSend_P (PSTR ("<div style='width:16%%;padding:0px;text-align:left;%s'>%d V</div>\n"), str_text, teleinfo_conso.phase[phase].voltage);

            // calculate percentage and value
            value = 100 * teleinfo_conso.phase[phase].papp / teleinfo_contract.ssousc;
            if (value > 100) value = 100;
            if (teleinfo_conso.phase[phase].papp > 0) ltoa (teleinfo_conso.phase[phase].papp, str_text, 10); 
              else strcpy (str_text, "");

            // calculate color
            if (value < 50) green = TELEINFO_RGB_GREEN_MAX; else green = TELEINFO_RGB_GREEN_MAX * 2 * (100 - value) / 100;
            if (value > 50) red = TELEINFO_RGB_RED_MAX; else red = TELEINFO_RGB_RED_MAX * 2 * value / 100;
            sprintf (str_color, "#%02x%02x00", red, green);

            // display bar graph percentage
            WSContentSend_P (PSTR ("<div style='width:72%%;padding:0px;text-align:left;background-color:%s;border-radius:6px;'><div style='width:%d%%;height:16px;padding:0px;text-align:center;font-size:12px;background-color:%s;border-radius:6px;'>%s</div></div>\n"), COLOR_BACKGROUND, value, str_color, str_text);
            WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div><div style='width:10%%;padding:0px;text-align:left;'>VA</div>\n"));
            WSContentSend_P (PSTR ("</div>\n"));
          }

        // consumption : active power
        value = 0; 
        for (phase = 0; phase < teleinfo_contract.phase; phase++) value += teleinfo_conso.phase[phase].pact;
        WSContentSend_P (PSTR ("<div style='display:flex;padding:2px 0px;'>\n"));
        WSContentSend_P (PSTR ("<div style='width:16%%;padding:0px;'></div>\n"));
        WSContentSend_P (PSTR ("<div style='width:47%%;padding:0px;text-align:left;'>cosφ <b>%u.%02u</b></div>\n"), teleinfo_conso.phase[0].cosphi / 100, teleinfo_conso.phase[0].cosphi % 100);
        WSContentSend_P (PSTR ("<div style='width:25%%;padding:0px;text-align:right;font-weight:bold;'>%d</div>\n"), value);
        WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div><div style='width:10%%;padding:0px;text-align:left;'>W</div>\n"));
        WSContentSend_P (PSTR ("</div>\n"));

        // consumption : today's total
        if (teleinfo_conso.midnight_wh != 0)
        {
          total = teleinfo_conso.total_wh - teleinfo_conso.midnight_wh;
          WSContentSend_P (PSTR ("<div style='display:flex;padding:2px 0px;'>\n"));
          WSContentSend_P (PSTR ("<div style='width:16%%;padding:0px;'></div>\n"));
          WSContentSend_P (PSTR ("<div style='width:37%%;padding:0px;text-align:left;'>Aujourd'hui</div>\n"));
          WSContentSend_P (PSTR ("<div style='width:35%%;padding:0px;text-align:right;font-weight:bold;'>%d</div>\n"), (long)total);
          WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div><div style='width:10%%;padding:0px;text-align:left;'>Wh</div>\n"));
          WSContentSend_P (PSTR ("</div>\n"));
        }
      }

      // if meter is running production mode
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

        // production : active power
        WSContentSend_P (PSTR ("<div style='display:flex;padding:2px 0px;'>\n"));
        WSContentSend_P (PSTR ("<div style='width:16%%;padding:0px;'></div>\n"));
        WSContentSend_P (PSTR ("<div style='width:47%%;padding:0px;text-align:left;'>cosφ <b>%u.%02u</b></div>\n"), teleinfo_prod.cosphi / 100, teleinfo_prod.cosphi % 100);
        WSContentSend_P (PSTR ("<div style='width:25%%;padding:0px;text-align:right;font-weight:bold;'>%d</div>\n"), teleinfo_prod.pact);
        WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div><div style='width:10%%;padding:0px;text-align:left;'>W</div>\n"));
        WSContentSend_P (PSTR ("</div>\n"));

        // production : today's total
        if (teleinfo_prod.midnight_wh != 0)
        {
          total = teleinfo_prod.total_wh - teleinfo_prod.midnight_wh;
          WSContentSend_P (PSTR ("<div style='display:flex;padding:2px 0px;'>\n"));
          WSContentSend_P (PSTR ("<div style='width:16%%;padding:0px;'></div>\n"));
          WSContentSend_P (PSTR ("<div style='width:37%%;padding:0px;text-align:left;'>Aujourd'hui</div>\n"));
          WSContentSend_P (PSTR ("<div style='width:35%%;padding:0px;text-align:right;font-weight:bold;'>%d</div>\n"), (long)total);
          WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div><div style='width:10%%;padding:0px;text-align:left;'>Wh</div>\n"));
          WSContentSend_P (PSTR ("</div>\n"));
        }
      }

      // if using tempo
      if (teleinfo_meter.tempo.today != TIC_TEMPO_NONE)
      {
        // separator and header
        WSContentSend_P (PSTR ("<hr>\n"));

        // hour scale
        WSContentSend_P (PSTR ("<div style='display:flex;margin:0px;padding:0px;font-size:10px;'>\n"));
        WSContentSend_P (PSTR ("<div style='width:20.8%%;padding:0px;text-align:left;font-weight:bold;font-size:12px;'>Tempo</div>\n"), 0);
        WSContentSend_P (PSTR ("<div style='width:13.2%%;padding:0px;text-align:left;'>%uh</div>\n"), 0);
        WSContentSend_P (PSTR ("<div style='width:13.2%%;padding:0px;'>%uh</div>\n"), 6);
        WSContentSend_P (PSTR ("<div style='width:26.4%%;padding:0px;'>%uh</div>\n"), 12);
        WSContentSend_P (PSTR ("<div style='width:13.2%%;padding:0px;'>%uh</div>\n"), 18);
        WSContentSend_P (PSTR ("<div style='width:13.2%%;padding:0px;'>%uh</div>\n"), 22);
        WSContentSend_P (PSTR ("</div>\n"));

        // loop thru days
        for (index = 0; index < 2; index ++)
        {
          WSContentSend_P (PSTR ("<div style='display:flex;margin:0px;padding:1px;height:14px;'>\n"));

          // display day
          if (index == 0) strcpy (str_text, "Aujourd'hui"); else strcpy (str_text, "Demain");
          WSContentSend_P (PSTR ("<div style='width:20.8%%;padding:0px;font-size:10px;text-align:left;'>%s</div>\n"), str_text);

          // display hourly slots
          for (slot = 0; slot < 24; slot ++)
          {
            // calculate color of current slot
            if ((index == 0) && (slot < 6)) color = teleinfo_meter.tempo.yesterday;
              else if ((index == 1) && (slot >= 6)) color = teleinfo_meter.tempo.tomorrow;
              else color = teleinfo_meter.tempo.today;
            GetTextIndexed (str_color, sizeof (str_color), color, kTeleinfoTempoColor);  

            // segment beginning
            WSContentSend_P (PSTR ("<div style='width:3.3%%;padding:1px 0px;background-color:%s;"), str_color);

            // set opacity for HC
            if ((slot < 6) || (slot >= 22)) WSContentSend_P (PSTR ("opacity:75%%;"));

            // first and last segment specificities
            if (slot == 0) WSContentSend_P (PSTR ("border-top-left-radius:6px;border-bottom-left-radius:6px;"));
              else if (slot == 23) WSContentSend_P (PSTR ("border-top-right-radius:6px;border-bottom-right-radius:6px;"));

            // segment end
            if ((index == 0) && (slot == RtcTime.hour))
            {
              GetTextIndexed (str_text, sizeof (str_text), color, kTeleinfoTempoDot);  
              WSContentSend_P (PSTR ("font-size:9px;'>%s</div>\n"), str_text);
            }
            else WSContentSend_P (PSTR ("'></div>\n"));
          }
          WSContentSend_P (PSTR ("</div>\n"));
        }
      }

      // separator
      WSContentSend_P (PSTR ("<hr>\n"));

      // counters
      WSContentSend_P (PSTR ("<div style='display:flex;padding:2px 0px;'>\n"));
      WSContentSend_P (PSTR ("<div style='width:50%%;padding:0px;'>%d trames</div>\n"), teleinfo_meter.nb_message);
      WSContentSend_P (PSTR ("<div style='width:50%%;padding:0px;'>%d cosφ</div>\n"), teleinfo_meter.nb_update);
      WSContentSend_P (PSTR ("</div>\n"));

      // if reset or more than 1% errors, display counters
      if ((teleinfo_meter.nb_reset > 0) || (teleinfo_meter.nb_error > teleinfo_meter.nb_line / 100))
      {
        WSContentSend_P (PSTR ("<div style='display:flex;padding:2px 0px;'>\n"));
        if (teleinfo_meter.nb_error == 0) strcpy (str_text, "white"); else strcpy (str_text, "red");
        WSContentSend_P (PSTR ("<div style='width:50%%;padding:0px;color:%s;'>%d erreurs</div>\n"), str_text, teleinfo_meter.nb_error);
        if (teleinfo_meter.nb_reset == 0) strcpy (str_text, "white"); else strcpy (str_text, "orange");
        WSContentSend_P (PSTR ("<div style='width:50%%;padding:0px;color:%s;'>%d reset</div>\n"), str_text, teleinfo_meter.nb_reset);
        WSContentSend_P (PSTR ("</div>\n"));
      }

      // end
      WSContentSend_P (PSTR ("</div>\n"));
      break;
  }
}

// Append Teleinfo configuration button to configuration page
void TeleinfoSensorWebConfigButton ()
{
  // heater and meter configuration button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>Configure Teleinfo</button></form></p>\n"), D_TELEINFO_PAGE_CONFIG);
}

// Teleinfo web page
void TeleinfoSensorWebPageConfigure ()
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

    // parameter 'msgpol' : set TIC messages diffusion policy
    WebGetArg (D_CMND_TELEINFO_MSG_POLICY, str_text, sizeof (str_text));
    if (strlen (str_text) > 0) teleinfo_config.msg_policy = atoi (str_text);

    // parameter 'msgtype' : set TIC messages type diffusion policy
    WebGetArg (D_CMND_TELEINFO_MSG_TYPE, str_text, sizeof (str_text));
    if (strlen (str_text) > 0) teleinfo_config.msg_type = atoi (str_text);

    // save configuration
    TeleinfoSaveConfig ();
  }

  // beginning of form
  WSContentStart_P (PSTR ("Configure Teleinfo"));
  WSContentSendStyle ();
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_TELEINFO_PAGE_CONFIG);

  // speed selection form
  WSContentSend_P (PSTR ("<p><fieldset><legend><b>&nbsp;%s %s&nbsp;</b></legend>\n"), "📨", PSTR ("Teleinfo (need restart)"));
  if ((TasmotaGlobal.baudrate != 1200) && (TasmotaGlobal.baudrate != 9600)) strcpy_P (str_select, PSTR ("checked")); else strcpy (str_select, "");
  WSContentSend_P (PSTR ("<p><input type='radio' name='%s' value='%d' %s>%s</p>\n"), D_CMND_TELEINFO_RATE, 115200, str_select, "Disabled");
  if (TasmotaGlobal.baudrate == 1200) strcpy_P (str_select, PSTR ("checked")); else strcpy (str_select, "");
  WSContentSend_P (PSTR ("<p><input type='radio' name='%s' value='%d' %s>%s</p>\n"), D_CMND_TELEINFO_RATE, 1200, str_select, "Historique (1200 bauds)");
  if (TasmotaGlobal.baudrate == 9600) strcpy_P (str_select, PSTR ("checked")); else strcpy (str_select, "");
  WSContentSend_P (PSTR ("<p><input type='radio' name='%s' value='%d' %s>%s</p>\n"), D_CMND_TELEINFO_RATE, 9600, str_select, "Standard (9600 bauds)");
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
  WSContentSend_P (PSTR ("<br><button name='save' type='submit' class='button bgrn'>%s</button>"), D_SAVE);
  WSContentSend_P (PSTR ("</form>\n"));

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

// Get specific argument as a value with min and max
int TeleinfoSensorWebGetArgValue (const char* pstr_argument, const int value_min, const int value_max, const int value_default)
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
void TeleinfoSensorWebMainButton ()
{
  // Teleinfo message page button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>Messages</button></form></p>\n"), D_TELEINFO_PAGE_TIC);

  // Teleinfo curve page button
  if (teleinfo_conso.total_wh > 0) WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>Courbes</button></form></p>\n"), D_TELEINFO_PAGE_GRAPH);

#ifdef USE_UFILESYS
  // Teleinfo conso and prod
  if (teleinfo_conso.total_wh > 0) WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>Consommation</button></form></p>\n"), D_TELEINFO_PAGE_GRAPH_CONSO);
  if (teleinfo_prod.total_wh > 0)  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>Production</button></form></p>\n"), D_TELEINFO_PAGE_GRAPH_PROD);
#endif      // USE_UFILESYS
}

// TIC raw message data
void TeleinfoSensorWebTicUpdate ()
{
  int      index;
  uint32_t timestart;
  char     str_line[TELEINFO_LINE_MAX];

  // start timestamp
  timestart = millis ();

  // start of data page
  WSContentBegin (200, CT_PLAIN);

  // send line number
  sprintf (str_line, "%d\n", teleinfo_meter.nb_message);
  WSContentSend_P (PSTR ("%s"), str_line); 


  // loop thru TIC message lines to publish if defined
  for (index = 0; index < teleinfo_message.line_max; index ++)
  {
    if (teleinfo_message.line[index].checksum != 0) 
      sprintf (str_line, "%s|%s|%c\n", teleinfo_message.line[index].str_etiquette.c_str (), teleinfo_message.line[index].str_donnee.c_str (), teleinfo_message.line[index].checksum);
    else strcpy (str_line, " | | \n");
    WSContentSend_P (PSTR ("%s"), str_line); 
  }

  // end of data page
  WSContentEnd ();

  // log data serving time
  AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: Update Message in %ums"), millis () - timestart);
}

// TIC message page
void TeleinfoSensorWebPageTic ()
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
  WSContentSend_P (PSTR ("    num_param=arr_param.length;\n"));
  WSContentSend_P (PSTR ("    if (document.getElementById('msg')!=null) document.getElementById('msg').textContent=arr_param[0];\n"));
  WSContentSend_P (PSTR ("    for (i=1;i<num_param;i++){\n"));
  WSContentSend_P (PSTR ("     arr_value=arr_param[i].split('|');\n"));
  WSContentSend_P (PSTR ("     if (document.getElementById('e'+i)!=null) document.getElementById('e'+i).textContent=arr_value[0];\n"));
  WSContentSend_P (PSTR ("     if (document.getElementById('d'+i)!=null) document.getElementById('d'+i).textContent=arr_value[1];\n"));
  WSContentSend_P (PSTR ("     if (document.getElementById('c'+i)!=null) document.getElementById('c'+i).textContent=arr_value[2];\n"));
  WSContentSend_P (PSTR ("    }\n"));
  WSContentSend_P (PSTR ("    for (i=num_param;i<=%d;i++){\n"), teleinfo_message.line_max);
  WSContentSend_P (PSTR ("     if (document.getElementById('e'+i)!=null) document.getElementById('e'+i).textContent='';\n"));
  WSContentSend_P (PSTR ("     if (document.getElementById('d'+i)!=null) document.getElementById('d'+i).textContent='';\n"));
  WSContentSend_P (PSTR ("     if (document.getElementById('c'+i)!=null) document.getElementById('c'+i).textContent='';\n"));
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
  for (index = 1; index <= teleinfo_message.line_max; index ++)
    WSContentSend_P (PSTR ("<tr id='l%d'><td id='e%d'>&nbsp;</td><td id='d%d'>&nbsp;</td><td id='c%d'>&nbsp;</td></tr>\n"), index, index, index, index);

  // end of table and end of page
  WSContentSend_P (PSTR ("</table></div>\n"));
  WSContentSend_P (PSTR ("<div>\n"));

  // main menu button
  WSContentSend_P (PSTR ("<div><form action='/' method='get'><button class='menu'>%s</button></form></div>\n"), D_MAIN_MENU);
  
  // page end
  WSContentStop ();
}

// Display graph frame and time marks
void TeleinfoSensorGraphDisplayTime (const uint8_t period, const uint8_t histo)
{
  uint32_t index, day;
  uint32_t unit_width, shift_unit, shift_width;  
  uint32_t graph_x;  
  uint32_t current_time;
  TIME_T   time_dst;
  char     str_text[8];

  // handle graph units according to period
  switch (period) 
  {
    case TELEINFO_PERIOD_LIVE:
      unit_width  = teleinfo_graph.width / 5;
      for (index = 1; index < 5; index++)
      {
        graph_x = teleinfo_graph.left + (index * unit_width);
        WSContentSend_P (PSTR ("<line id='l%u' class='time' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n"), index, graph_x, 1, graph_x, 99);
      }
      break;

#ifdef USE_UFILESYS
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
        day = (index + 1) % 7;
        strlcpy (str_text, kWeekdayNames + day * 3, 4);
        WSContentSend_P (PSTR ("<text class='time' x='%d' y='%u%%'>%s</text>\n"), graph_x + (unit_width / 2), 3, str_text);
      }
      break;
#endif      // USE_UFILESYS

  }
}

// Display data curve
void TeleinfoSensorGraphDisplayCurve (const uint8_t phase, const uint8_t data)
{
  bool     analyse;
  uint32_t timestart;
  int      index, index_array;
  long     graph_x, graph_y, graph_range, graph_delta;
  long     prev_x, prev_y, bezier_y1, bezier_y2; 
  long     value, prev_value;
  long     arr_value[TELEINFO_GRAPH_SAMPLE];
  char     str_value[36];

  // check parameters
  if (teleinfo_config.max_power == 0) return;

  // give control back to system to avoid watchdog
  yield ();

  // start timestamp
  timestart = millis ();

  // init array
  for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index ++) arr_value[index] = LONG_MAX;

  // collect data in temporary array
  switch (teleinfo_graph.period)
  {
    case TELEINFO_PERIOD_LIVE:
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
      break;

#ifdef USE_UFILESYS
    case TELEINFO_PERIOD_DAY:
    case TELEINFO_PERIOD_WEEK:
      uint8_t  column;
      uint32_t len_buffer, size_buffer;
      char    *pstr_token, *pstr_buffer, *pstr_line;
      char     str_filename[UFS_FILENAME_SIZE];
      File     file;

      // if data file exists
      if (TeleinfoSensorHistoGetFilename (teleinfo_graph.period, teleinfo_graph.histo, str_filename))
      {
        // init
        strcpy (teleinfo_graph.str_buffer, "");

        // set column with data
        column = 3 + (phase * 6) + data;

        // open file and skip header
        file = ffsp->open (str_filename, "r");
        while (file.available ())
        {
          // read next block
          size_buffer = strlen (teleinfo_graph.str_buffer);
          len_buffer = file.readBytes (teleinfo_graph.str_buffer + size_buffer, sizeof (teleinfo_graph.str_buffer) - size_buffer - 1);
          teleinfo_graph.str_buffer[size_buffer + len_buffer] = 0;

          // loop to read lines
          pstr_buffer = teleinfo_graph.str_buffer;
          pstr_line = strchr (pstr_buffer, '\n');
          while (pstr_line)
          {
            // init
            index = 0;
            index_array = INT_MAX;
            value = LONG_MAX;

            // set end of line
            *pstr_line = 0;

            // if first column is not numerical, ignore line
            analyse = isdigit (pstr_buffer[0]);

            pstr_token = strtok (pstr_buffer, ";");
            while (analyse && (pstr_token != nullptr))
            {
              // if token is defined
              if (strlen (pstr_token) > 0)
              {
                if (index == 0) index_array = atoi (pstr_token);
                else if (index == column)
                {
                  value = atol (pstr_token); 
                  analyse = false;
                }
              }

              // if index and value are valid, update value to be displayed
              if ((index_array < TELEINFO_GRAPH_SAMPLE) && (value != LONG_MAX)) arr_value[index_array] = value;

              // next token in the line
              index++;
              pstr_token = strtok (nullptr, ";");
            }

            // look for next line
            pstr_buffer = pstr_line + 1;
            pstr_line = strchr (pstr_buffer, '\n');

            // handle data update
            TeleinfoDataUpdate ();
          }

          // deal with remaining string
          if (pstr_buffer != teleinfo_graph.str_buffer) strcpy (teleinfo_graph.str_buffer, pstr_buffer);
            else strcpy(teleinfo_graph.str_buffer, "");
        }

        // close file
        file.close ();
      }
      break;
#endif    // USE_UFILESYS

  }

  // display values
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
          if (teleinfo_config.max_power != 0) graph_y = TELEINFO_GRAPH_HEIGHT - (value * TELEINFO_GRAPH_HEIGHT / teleinfo_config.max_power);
          break;

        case TELEINFO_UNIT_V:
        case TELEINFO_UNIT_PEAK_V:
          graph_range = abs (teleinfo_config.max_volt - TELEINFO_VOLTAGE);
          if (graph_range != 0) graph_delta = (TELEINFO_VOLTAGE - value) * TELEINFO_GRAPH_HEIGHT / 2 / graph_range;
          graph_y = (TELEINFO_GRAPH_HEIGHT / 2) + graph_delta;
          if (graph_y < 0) graph_y = 0;
          if (graph_y > TELEINFO_GRAPH_HEIGHT) graph_y = TELEINFO_GRAPH_HEIGHT;
          break;

        case TELEINFO_UNIT_COSPHI:
          graph_y = TELEINFO_GRAPH_HEIGHT - (value * TELEINFO_GRAPH_HEIGHT / 100);
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
            WSContentSend_P (PSTR ("M%d %d "), graph_x, TELEINFO_GRAPH_HEIGHT); 

            // first point
            WSContentSend_P (PSTR ("L%d %d "), graph_x, graph_y); 
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
            WSContentSend_P (PSTR ("C%d %d,%d %d,%d %d "), graph_x, bezier_y1, prev_x, bezier_y2, graph_x, graph_y); 
          }
          break;

        case TELEINFO_UNIT_PEAK_VA:
        case TELEINFO_UNIT_PEAK_V:
        case TELEINFO_UNIT_V:
        case TELEINFO_UNIT_COSPHI:
          // if first point
          if (prev_x == LONG_MAX) WSContentSend_P (PSTR ("M%d %d "), graph_x, graph_y); 

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
            WSContentSend_P (PSTR ("C%d %d,%d %d,%d %d "), graph_x, bezier_y1, prev_x, bezier_y2, graph_x, graph_y); 
          }
          break;
      }

      // save previous y position
      prev_x = graph_x;
      prev_y = graph_y;
    }

    // handle data update
    TeleinfoDataUpdate ();
  }

  // end data value curve
  switch (data)
  {
    case TELEINFO_UNIT_VA:
    case TELEINFO_UNIT_W:
      WSContentSend_P (PSTR ("L%d,%d Z"), graph_x, TELEINFO_GRAPH_HEIGHT); 
      break;
  }
}

#ifdef USE_UFILESYS

// Display bar graph
void TeleinfoSensorGraphDisplayBar (const uint8_t histo, const bool is_main)
{
  bool     analyse;
  int      index, count;
  int      month, day, hour;
  long     value, value_x, value_y;
  long     graph_x, graph_y, graph_delta, graph_width, graph_height, graph_x_end, graph_max;
  long     graph_start, graph_stop;  
  long     arr_value[TELEINFO_GRAPH_MAX_BARGRAPH];
  uint8_t  day_of_week;
  uint32_t time_bar;
  uint32_t len_buffer, size_buffer;
  size_t   size_line, size_value;
  TIME_T   time_dst;
  char     str_type[8];
  char     str_value[16];
  char    *pstr_token, *pstr_buffer, *pstr_line, *pstr_error;
  File     file;
  char     str_filename[UFS_FILENAME_SIZE];

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
    graph_max = teleinfo_config.param[TELEINFO_CONFIG_MAX_MONTH];
    graph_width = 90;             // width of graph bar area
    graph_start = 1;
    graph_stop  = 12;             // number of graph bars (months per year)
    graph_delta = 20;             // separation between bars (pixels)
    strcpy (str_type, "month");
  }

  // else init graph units for full month display (day bars)
  else if (teleinfo_graph.day == 0)
  {
    graph_max = teleinfo_config.param[TELEINFO_CONFIG_MAX_DAY];
    graph_width = 35;             // width of graph bar area
    graph_start = 1;
    graph_stop  = 31;             // number of graph bars (days per month)
    graph_delta = 4;              // separation between bars (pixels)
    strcpy (str_type, "day");
  }

  // else init graph units for full day display (hour bars)
  else
  {
    graph_max = teleinfo_config.param[TELEINFO_CONFIG_MAX_HOUR];
    graph_width = 45;             // width of graph bar area
    graph_start = 0;
    graph_stop  = 23;             // number of graph bars (hours per day)
    graph_delta = 10;             // separation between bars (pixels)
    strcpy (str_type, "hour");
  }

  // calculate graph height and graph start
  graph_height = TELEINFO_GRAPH_HEIGHT;
  graph_x      = teleinfo_graph.left + graph_delta / 2;
  graph_x_end  = graph_x + graph_width - graph_delta;
  if (!is_main) { graph_x +=4; graph_x_end -= 4; }

  // load data from file
  if (TeleinfoSensorHistoGetFilename (teleinfo_graph.period, histo, str_filename))
  {
    // init
    strcpy (teleinfo_graph.str_buffer, "");
    pstr_buffer = teleinfo_graph.str_buffer;

    //open file and skip header
    file = ffsp->open (str_filename, "r");
    while (file.available ())
    {
      // read next block
      size_buffer = strlen (teleinfo_graph.str_buffer);
      len_buffer = file.readBytes (teleinfo_graph.str_buffer + size_buffer, sizeof (teleinfo_graph.str_buffer) - size_buffer - 1);
      teleinfo_graph.str_buffer[size_buffer + len_buffer] = 0;

      // loop to read lines
      pstr_buffer = teleinfo_graph.str_buffer;
      pstr_line = strchr (pstr_buffer, '\n');
      while (pstr_line)
      {
        // init
        index = 0;
        month = INT_MAX;
        day   = INT_MAX;
        value = LONG_MAX;

        // set end of line
        *pstr_line = 0;

        // if first column is not numerical, ignore line
        analyse = isdigit (pstr_buffer[0]);

        pstr_token = strtok (pstr_buffer, ";");
        while (analyse && (pstr_token != nullptr))
        {
          // if token is defined
          if (strlen (pstr_token) > 0)
          {
            switch (index)
            {
              case 1:
                month = atoi (pstr_token);
                break;
              case 2:
                day = atoi (pstr_token);
                break;
              case 4:
                value = atol (pstr_token);
                break;
              default:
                if ((index > 4) && (index < 24 + 5)) value = atol (pstr_token);
                break;
            }
          }

          // year graph, stop line analysis if data available
          if ((teleinfo_graph.month == 0) && (index >= 4))
          {
            analyse = false;
            if (month > graph_stop) month = TELEINFO_GRAPH_MAX_BARGRAPH - 1;
            if (value != 0)
              if (arr_value[month] == LONG_MAX) arr_value[month] = value; 
                else arr_value[month] += value;
          }

          // if line deals with target month, display days
          else if ((teleinfo_graph.month == month) && (teleinfo_graph.day == 0) && (index >= 4))
          {
            analyse = false;
            if (day > graph_stop) day = TELEINFO_GRAPH_MAX_BARGRAPH - 1;
            if (value != 0)
              if (arr_value[day] == LONG_MAX) arr_value[day] = value; 
                else arr_value[day] += value;
          }

          // if line deals with target month and target day, display hours
          else if ((teleinfo_graph.month == month) && (teleinfo_graph.day == day) && (index > 4) && (index < 24 + 5))
          {
            hour = index - 5;
            if (value != 0)             
              if (arr_value[hour] == LONG_MAX) arr_value[hour] = value;
                else arr_value[hour] += value;
          }

          // stop after max token
          if (index >= 24 + 5) analyse = false;

          // next token in the line
          index++;
          pstr_token = strtok (nullptr, ";");
        }

        // look for next line
        pstr_buffer = pstr_line + 1;
        pstr_line = strchr (pstr_buffer, '\n');
      }

      // deal with remaining string
      if (pstr_buffer != teleinfo_graph.str_buffer) strcpy (teleinfo_graph.str_buffer, pstr_buffer);
        else strcpy (teleinfo_graph.str_buffer, "");

      // handle data update
      TeleinfoDataUpdate ();
    }

    // close file
    file.close ();
  }

  // if current year data
  if (histo == 0)
  {
    // if displaying production data
    if (teleinfo_graph.period == TELEINFO_PERIOD_PROD)
    {
      // if dealing with current day, add live values to hourly slots
      if ((teleinfo_graph.month == RtcTime.month) && (teleinfo_graph.day == RtcTime.day_of_month))
      {
        for (index = 0; index < 24; index ++)
          if (teleinfo_graph.prod.hour_wh[index] != 0) 
          {
            if (arr_value[index] == LONG_MAX) arr_value[index] = teleinfo_graph.prod.hour_wh[index];
              else arr_value[index] += teleinfo_graph.prod.hour_wh[index];
          }
      }

      // else if current monthly data, init current day slot from live values sum
      else if (teleinfo_graph.month == RtcTime.month)
      {
        for (index = 0; index < 24; index ++)
          if (teleinfo_graph.prod.hour_wh[index] != 0)
          {
            if (arr_value[RtcTime.day_of_month] == LONG_MAX) arr_value[RtcTime.day_of_month] = teleinfo_graph.prod.hour_wh[index];
              else arr_value[RtcTime.day_of_month] += teleinfo_graph.prod.hour_wh[index];
          }
      }
    } 

    // else displaying conso data
    else if (teleinfo_graph.period == TELEINFO_PERIOD_YEAR)
    {
      // if current daily data, init previous hour slot from live values
      if ((teleinfo_graph.month == RtcTime.month) && (teleinfo_graph.day == RtcTime.day_of_month))
      {
        for (index = 0; index < 24; index ++)
          if (teleinfo_graph.conso.hour_wh[index] != 0)
          {
            if (arr_value[index] == LONG_MAX) arr_value[index] = teleinfo_graph.conso.hour_wh[index];
              else arr_value[index] += teleinfo_graph.conso.hour_wh[index]; 
          }
      }

      // else if current monthly data, init current day slot from live values sum
      else if (teleinfo_graph.month == RtcTime.month)
      {
        for (index = 0; index < 24; index ++) 
          if (teleinfo_graph.conso.hour_wh[index] != 0)
          {
            if (arr_value[RtcTime.day_of_month] == LONG_MAX) arr_value[RtcTime.day_of_month] = teleinfo_graph.conso.hour_wh[index];
              else arr_value[RtcTime.day_of_month] += teleinfo_graph.conso.hour_wh[index];
          }
      }
    } 
  }

  // bar graph
  // ---------

  for (index = graph_start; index <= graph_stop; index ++)
  {
    // if value is defined, display bar and value
    if (arr_value[index] != LONG_MAX)
    {
      // display
      if (graph_max != 0) graph_y = graph_height - (arr_value[index] * graph_height / graph_max / 1000); else graph_y = 0;
      if (graph_y < 0) graph_y = 0;

      // display link
      if (teleinfo_graph.period == TELEINFO_PERIOD_PROD) strcpy (str_value, D_TELEINFO_PAGE_GRAPH_PROD); else strcpy (str_value, D_TELEINFO_PAGE_GRAPH_CONSO);
      if (is_main && (teleinfo_graph.month == 0)) WSContentSend_P (PSTR ("<a href='%s?month=%d&day=0'>"), str_value, index);
      else if (is_main && (teleinfo_graph.day == 0)) WSContentSend_P (PSTR ("<a href='%s?day=%d'>"), str_value, index);

      // display bar
      if (is_main) strcpy (str_value, "now"); else strcpy (str_value, "prev");
      WSContentSend_P (PSTR ("<path class='%s' d='M%d %d L%d %d L%d %d L%d %d L%d %d L%d %d Z'></path>"), str_value, graph_x, graph_height, graph_x, graph_y + 2, graph_x + 2, graph_y, graph_x_end - 2, graph_y, graph_x_end, graph_y + 2, graph_x_end, graph_height);

      // end of link 
      if (is_main && ((teleinfo_graph.month == 0) || (teleinfo_graph.day == 0))) WSContentSend_P (PSTR("</a>\n"));
        else WSContentSend_P (PSTR ("\n"));

      // if main graph
      if (is_main)
      {
        // value on top of bar
        // -------------------

        // calculate graph x position
        value_x = (graph_x + graph_x_end) / 2;

        // display bar value
        if (arr_value[index] > 0)
        {
          // calculate y position
          value_y = graph_y - 15;
          if (value_y < 15) value_y = 15;
          if (value_y > graph_height - 50) value_y = graph_height - 50;

          // display value
          TeleinfoSensorGetDataDisplay (TELEINFO_UNIT_MAX, arr_value[index], str_value, sizeof (str_value), true);
          WSContentSend_P (PSTR ("<text class='%s value' x='%d' y='%d'>%s</text>\n"), str_type, value_x, value_y, str_value);
        }

        // month name or day / hour number
        // ----------------

        // if full year, get month name else get day of month
        if (teleinfo_graph.month == 0) strlcpy (str_value, kMonthNames + (index - 1) * 3, 4);
          else if (teleinfo_graph.day == 0) sprintf (str_value, "%02d", index);
            else sprintf (str_value, "%dh", index);

        // display
        value_y = graph_height - 10;
        WSContentSend_P (PSTR ("<text class='%s' x='%d' y='%d'>%s</text>\n"), str_type, value_x, value_y, str_value);

        // week day name
        // -------------

        if ((teleinfo_graph.month != 0) && (teleinfo_graph.day == 0))
        {
          // calculate day name (2 letters)
          time_dst.day_of_month = index;
          time_bar = MakeTime (time_dst);
          BreakTime (time_bar, time_dst);
          day_of_week = (time_dst.day_of_week + 6) % 7;
          strlcpy (str_value, kWeekdayNames + day_of_week * 3, 3);

          // display
          value_y = graph_height - 30;
          WSContentSend_P (PSTR ("<text class='%s' x='%d' y='%d'>%s</text>\n"), str_type, value_x, value_y, str_value);
        }
      }
    }

    // increment bar position
    graph_x     += graph_width;
    graph_x_end += graph_width;

    // handle data update
    TeleinfoDataUpdate ();
  }
}

// calculate previous period parameters
bool TeleinfoSensorGraphPreviousPeriod (const bool update)
{
  bool     is_file = false;
  uint8_t  histo = teleinfo_graph.histo;
  uint8_t  month = teleinfo_graph.month;
  uint8_t  day   = teleinfo_graph.day;
  uint32_t year;
  TIME_T   time_dst;
  char     str_filename[UFS_FILENAME_SIZE];

  // handle next according to period type
  switch (teleinfo_graph.period)
  {
    case TELEINFO_PERIOD_DAY:
      while (!is_file && (histo < teleinfo_config.param[TELEINFO_CONFIG_NBDAY])) is_file = TeleinfoSensorHistoGetFilename (TELEINFO_PERIOD_DAY, ++histo, str_filename);
      break;

    case TELEINFO_PERIOD_WEEK:
      while (!is_file && (histo < teleinfo_config.param[TELEINFO_CONFIG_NBWEEK])) is_file = TeleinfoSensorHistoGetFilename (TELEINFO_PERIOD_WEEK, ++histo, str_filename);
      break;

    case TELEINFO_PERIOD_YEAR:
    case TELEINFO_PERIOD_PROD:
      // full year view
      if ((day == 0) && (month == 0)) histo++;

      // month view
      else if (day == 0)
      {
        month--;
        if (month == 0) { month = 12; histo++; }
      }

      // day view
      else
      {
        day--;
        if (day == 0) month--;
        if (month == 0) { month = 12; histo++; }
        if (day == 0)
        {
          // calculate number of days in current month
          BreakTime (LocalTime (), time_dst);
          year = 1970 + (uint32_t)time_dst.year - histo;
          day = TeleinfoSensorGetDaysInMonth (year, month);
        }
      }

      // check if yearly file is available
      is_file = TeleinfoSensorHistoGetFilename (teleinfo_graph.period, histo, str_filename);
      break;
  }

  // if asked, set next period
  if (is_file && update)
  {
    teleinfo_graph.histo = histo;
    teleinfo_graph.month = month;
    teleinfo_graph.day   = day;
  }

  return is_file;
}

// check if next period is available
bool TeleinfoSensorGraphNextPeriod (const bool update)
{
  bool     is_file = false;
  bool     is_next = false;
  uint8_t  nb_day;
  uint8_t  histo = teleinfo_graph.histo;
  uint8_t  month = teleinfo_graph.month;
  uint8_t  day   = teleinfo_graph.day;
  uint32_t year;
  TIME_T   time_dst;
  char     str_filename[UFS_FILENAME_SIZE];

  // handle next according to period type
  switch (teleinfo_graph.period)
  {
    case TELEINFO_PERIOD_DAY:
      while (!is_file && (histo > 0)) is_file = TeleinfoSensorHistoGetFilename (TELEINFO_PERIOD_DAY, --histo, str_filename);
      break;

    case TELEINFO_PERIOD_WEEK:
      while (!is_file && (histo > 0)) is_file = TeleinfoSensorHistoGetFilename (TELEINFO_PERIOD_WEEK, --histo, str_filename);
      break;

    case TELEINFO_PERIOD_YEAR:
    case TELEINFO_PERIOD_PROD:
      // full year view
      if ((day == 0) && (month == 0)) is_next = true;

      // month view
      else if (day == 0)
      {
        month++;
        if (month > 12) { month = 1; is_next = true; }
      }

      // day view
      else
      {
        // calculate number of days in current month
        BreakTime (LocalTime (), time_dst);
        year = 1970 + (uint32_t)time_dst.year - histo;
        nb_day = TeleinfoSensorGetDaysInMonth (year, month);

        // next day
        day++;
        if (day > nb_day) { day = 1; month++; }
        if (month == 13) { month = 1; is_next = true; }
      }

      // if yearly file available
      if ((histo == 0) && is_next) histo = UINT8_MAX;
        else if (is_next) histo--;

      // check if yearly file is available
      is_file = TeleinfoSensorHistoGetFilename (teleinfo_graph.period, histo, str_filename);
      break;
  }

  // if asked, set next period
  if (is_file && update)
  {
    teleinfo_graph.histo = histo;
    teleinfo_graph.month = month;
    teleinfo_graph.day   = day;
  }

  return is_file;
}

#endif    // USE_UFILESYS

// Graph frame
void TeleinfoSensorGraphDisplayFrame ()
{
  int     nb_digit = -1;
  long    index, unit_min, unit_max, unit_range;
  float   value;
  char    str_unit[8];
  char    str_text[8];
  char    arr_label[5][12];
  uint8_t arr_pos[5] = {98,76,51,26,2};  

  // set labels according t data type
  switch (teleinfo_graph.data) 
  {
    // power
    case TELEINFO_UNIT_VA:
    case TELEINFO_UNIT_W:
      for (index = 0; index < 5; index ++) TeleinfoSensorGetDataDisplay (TELEINFO_UNIT_MAX, index * teleinfo_config.max_power / 4, arr_label[index], sizeof (arr_label[index]), true);
      break;

    // voltage
    case TELEINFO_UNIT_V:
      unit_max   = teleinfo_config.max_volt;
      unit_range = 2 * (teleinfo_config.max_volt - TELEINFO_VOLTAGE);
      unit_min   = unit_max - unit_range;
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
      if (teleinfo_graph.month == 0) unit_max = teleinfo_config.param[TELEINFO_CONFIG_MAX_MONTH];
        else if (teleinfo_graph.day == 0) unit_max = teleinfo_config.param[TELEINFO_CONFIG_MAX_DAY];
          else unit_max = teleinfo_config.param[TELEINFO_CONFIG_MAX_HOUR];
      unit_max *= 1000;
      for (index = 0; index < 5; index ++) TeleinfoSensorGetDataDisplay (TELEINFO_UNIT_MAX, index * unit_max / 4, arr_label[index], sizeof (arr_label[index]), true);
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
  GetTextIndexed (str_unit, sizeof (str_unit), teleinfo_graph.data, kTeleinfoGraphDisplay);
#ifdef USE_UFILESYS
  if ((teleinfo_graph.period == TELEINFO_PERIOD_YEAR) || (teleinfo_graph.period == TELEINFO_PERIOD_PROD)) strcpy (str_unit, "Wh");
#endif      // USE_UFILESYS
  strlcat (str_text, str_unit, sizeof (str_text));

  // units graduation
  WSContentSend_P (PSTR ("<text class='power' x='%d%%' y='%d%%'>%s</text>\n"), TELEINFO_GRAPH_PERCENT_STOP + 3, 2, str_text);
  for (index = 0; index < 5; index ++) WSContentSend_P (PSTR ("<text class='power' x='%u%%' y='%u%%'>%s</text>\n"), 2, arr_pos[index],  arr_label[index]);
}

// Graph public page
void TeleinfoSensorWebGraph ()
{
  bool     display;
  uint8_t  phase, choice, counter;  
  uint16_t index;
  uint32_t timestart, delay, year;
  long     percentage;
  TIME_T   time_dst;
  char     str_text[16];
  char     str_date[16];

  // timestamp
  timestart = millis ();

  // beginning of form without authentification
  WSContentStart_P (D_TELEINFO_GRAPH, false);

  // serve page if possible
  if (!teleinfo_graph.serving)
  {
    // start of serving
    teleinfo_graph.serving = true;

    // get numerical argument values
    teleinfo_graph.data  = (uint8_t)TeleinfoSensorWebGetArgValue (D_CMND_TELEINFO_DATA,  0, TELEINFO_UNIT_MAX - 1, teleinfo_graph.data);
    teleinfo_graph.histo = (uint8_t)TeleinfoSensorWebGetArgValue (D_CMND_TELEINFO_HISTO, 0, 52, teleinfo_graph.histo);

#ifdef USE_UFILESYS
    if (teleinfo_graph.period >= TELEINFO_PERIOD_YEAR) teleinfo_graph.period = TELEINFO_PERIOD_LIVE;
#endif      // USE_UFILESYS

    choice = (uint8_t)TeleinfoSensorWebGetArgValue (D_CMND_TELEINFO_PERIOD, 0, TELEINFO_PERIOD_MAX - 1, teleinfo_graph.period);
    if (choice != teleinfo_graph.period) teleinfo_graph.histo = 0;
    teleinfo_graph.period = choice;  

    // if unit is Wh, force to W
    if (teleinfo_graph.data == TELEINFO_UNIT_WH) teleinfo_graph.data = TELEINFO_UNIT_W;

    // check phase display argument
    if (Webserver->hasArg (D_CMND_TELEINFO_PHASE)) WebGetArg (D_CMND_TELEINFO_PHASE, teleinfo_graph.str_phase, sizeof (teleinfo_graph.str_phase));
    while (strlen (teleinfo_graph.str_phase) < teleinfo_contract.phase) strlcat (teleinfo_graph.str_phase, "1", sizeof (teleinfo_graph.str_phase));

    // graph increment
    choice = TeleinfoSensorWebGetArgValue (D_CMND_TELEINFO_PLUS, 0, 1, 0);
    if (choice == 1)
    {
      if ((teleinfo_graph.data == TELEINFO_UNIT_VA) || (teleinfo_graph.data == TELEINFO_UNIT_W)) teleinfo_config.max_power += TELEINFO_GRAPH_INC_POWER;
      else if (teleinfo_graph.data == TELEINFO_UNIT_V) teleinfo_config.max_volt += TELEINFO_GRAPH_INC_VOLTAGE;
    }

    // graph decrement
    choice = TeleinfoSensorWebGetArgValue (D_CMND_TELEINFO_MINUS, 0, 1, 0);
    if (choice == 1)
    {
      if ((teleinfo_graph.data == TELEINFO_UNIT_VA) || (teleinfo_graph.data == TELEINFO_UNIT_W)) teleinfo_config.max_power = max ((long)TELEINFO_GRAPH_MIN_POWER, teleinfo_config.max_power - TELEINFO_GRAPH_INC_POWER);
      else if (teleinfo_graph.data == TELEINFO_UNIT_V) teleinfo_config.max_volt = max ((long)TELEINFO_GRAPH_MIN_VOLTAGE, teleinfo_config.max_volt - TELEINFO_GRAPH_INC_VOLTAGE);
    }

#ifdef USE_UFILESYS
    // handle previous page
    choice = TeleinfoSensorWebGetArgValue (D_CMND_TELEINFO_PREV, 0, 1, 0);
    if (choice == 1) TeleinfoSensorGraphPreviousPeriod (true);

    // handle next page
    choice = TeleinfoSensorWebGetArgValue (D_CMND_TELEINFO_NEXT, 0, 1, 0);
    if (choice == 1) TeleinfoSensorGraphNextPeriod (true);
#endif      // USE_UFILESYS

    // javascript : screen swipe for previous and next period
    WSContentSend_P (PSTR ("\n\nlet startX=0;let stopX=0;let startY=0;let stopY=0;\n"));
    WSContentSend_P (PSTR ("window.addEventListener('touchstart',function(evt){startX=evt.changedTouches[0].pageX;startY=evt.changedTouches[0].pageY;},false);\n"));
    WSContentSend_P (PSTR ("window.addEventListener('touchend',function(evt){stopX=evt.changedTouches[0].pageX;stopY=evt.changedTouches[0].pageY;handleGesture();},false);\n"));
    WSContentSend_P (PSTR ("function handleGesture(){\n"));
    WSContentSend_P (PSTR (" let deltaX=stopX-startX;let deltaY=Math.abs(stopY-startY);\n"));
    WSContentSend_P (PSTR (" if(deltaY<10 && deltaX<-100){document.getElementById('%s').click();}\n"), D_CMND_TELEINFO_NEXT);
    WSContentSend_P (PSTR (" else if(deltaY<10 && deltaX>100){document.getElementById('%s').click();}\n"), D_CMND_TELEINFO_PREV);
    WSContentSend_P (PSTR ("}\n"));

    // end of script section
    WSContentSend_P (PSTR ("</script>\n\n"));

    // page data refresh script
    //  format : value phase 1;value phase 2;value phase 3;curve data phase 1
    WSContentSend_P (PSTR ("<script type='text/javascript'>\n\n"));

    // ---------------
    //   data update
    // ---------------
    counter = 0;
    WSContentSend_P (PSTR ("function updateData(){\n"));

    WSContentSend_P (PSTR (" httpData=new XMLHttpRequest();\n"));
    WSContentSend_P (PSTR (" httpData.open('GET','%s',true);\n"), D_TELEINFO_PAGE_GRAPH_DATA);

    WSContentSend_P (PSTR (" httpData.onreadystatechange=function(){\n"));
    WSContentSend_P (PSTR ("  if (httpData.readyState===XMLHttpRequest.DONE){\n"));
    WSContentSend_P (PSTR ("   if (httpData.status===0 || (httpData.status>=200 && httpData.status<400)){\n"));
    WSContentSend_P (PSTR ("    arr_value=httpData.responseText.split(';');\n"));
    WSContentSend_P (PSTR ("    if (document.getElementById('msg')!=null) document.getElementById('msg').textContent=arr_value[%u];\n"), counter++ );                                                                        // number of received messages
    for (phase = 0; phase < teleinfo_contract.phase; phase++) WSContentSend_P (PSTR ("    if (document.getElementById('v%u')!=null) document.getElementById('v%u').textContent=arr_value[%u];\n"), phase, phase, counter++ );     // phase values
    WSContentSend_P (PSTR ("   }\n"));
    WSContentSend_P (PSTR ("   setTimeout(updateData,%u);\n"), 1000);       // ask for data update every second 
    WSContentSend_P (PSTR ("  }\n"));
    WSContentSend_P (PSTR (" }\n"));

    WSContentSend_P (PSTR (" httpData.send();\n"));
    WSContentSend_P (PSTR ("}\n"));

    WSContentSend_P (PSTR ("setTimeout(updateData,%u);\n\n"), 100);               // ask for first data update after 100ms

    // ----------------
    //   curve update
    // ----------------

    // set curve update delay (live = every second, other = every 30s)
    if (teleinfo_graph.period == TELEINFO_PERIOD_LIVE) delay = 1000; else delay = 30000;

    WSContentSend_P (PSTR ("function updateCurve(){\n"));

    WSContentSend_P (PSTR (" httpCurve=new XMLHttpRequest();\n"));
    WSContentSend_P (PSTR (" httpCurve.open('GET','%s',true);\n"), D_TELEINFO_PAGE_GRAPH_CURVE);

    WSContentSend_P (PSTR (" httpCurve.onreadystatechange=function(){\n"));
    WSContentSend_P (PSTR ("  if (httpCurve.readyState===XMLHttpRequest.DONE){\n"));
    WSContentSend_P (PSTR ("   if (httpCurve.status===0 || (httpCurve.status>=200 && httpCurve.status<400)){\n"));
    WSContentSend_P (PSTR ("    arr_value=httpCurve.responseText.split(';');\n"));
    counter = 0;
    for (phase = 0; phase < teleinfo_contract.phase; phase++) WSContentSend_P (PSTR ("    if (document.getElementById('m%u')!=null) document.getElementById('m%u').setAttribute('d',arr_value[%u]);\n"), phase, phase, counter++ );     // phase main curve
    for (phase = 0; phase < teleinfo_contract.phase; phase++) WSContentSend_P (PSTR ("    if (document.getElementById('p%u')!=null) document.getElementById('p%u').setAttribute('d',arr_value[%u]);\n"), phase, phase, counter++ );     // phase peak curve
    WSContentSend_P (PSTR ("   }\n"));
    WSContentSend_P (PSTR ("   setTimeout(updateCurve,%u);\n"), delay);     // ask for next curve update 
    WSContentSend_P (PSTR ("  }\n"));
    WSContentSend_P (PSTR (" }\n"));

    WSContentSend_P (PSTR (" httpCurve.send();\n"));
    WSContentSend_P (PSTR ("}\n"));

    WSContentSend_P (PSTR ("setTimeout(updateCurve,%u);\n\n"), 200);             // ask for first curve update after 200ms

    WSContentSend_P (PSTR ("</script>\n\n"));

    // ------------
    //   page CSS
    // ------------

    // set page as scalable
    WSContentSend_P (PSTR ("<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=yes'/>\n"));

    // set cache policy, no cache for 12h
    WSContentSend_P (PSTR ("<meta http-equiv='Cache-control' content='public,max-age=720000'/>\n"));

    // page style
    WSContentSend_P (PSTR ("<style>\n"));
    WSContentSend_P (PSTR ("body {color:white;background-color:%s;font-family:Arial, Helvetica, sans-serif;}\n"), COLOR_BACKGROUND);
    WSContentSend_P (PSTR ("div {margin:4px auto;padding:2px 0px;text-align:center;vertical-align:top;}\n"));

    WSContentSend_P (PSTR ("a {text-decoration:none;}\n"));
    WSContentSend_P (PSTR ("div a {color:white;}\n"));

    WSContentSend_P (PSTR ("div.title {font-size:28px;}\n"));
    WSContentSend_P (PSTR ("div.count {position:relative;top:-36px;}\n"));
    WSContentSend_P (PSTR ("div.count span {font-size:12px;background:#4d82bd;color:white;}\n"));
    WSContentSend_P (PSTR ("div span {padding:0px 6px;border-radius:6px;}\n"));

    WSContentSend_P (PSTR ("div.inline {display:flex;}\n"));
    WSContentSend_P (PSTR ("div.live {width:40%%;margin:0px;padding:0px;}\n"));
    WSContentSend_P (PSTR ("div.phase {width:90px;padding:0.2rem;margin:0.5rem;border-radius:8px;}\n"));
    WSContentSend_P (PSTR ("div.phase span {font-size:1rem;font-weight:bold;}\n"));

    for (phase = 0; phase < teleinfo_contract.phase; phase++)
    {
      GetTextIndexed (str_text, sizeof (str_text), phase, kTeleinfoColorPhase);
      WSContentSend_P (PSTR ("div.ph%d {color:%s;border:1px %s solid;}\n"), phase, str_text, str_text);    
    }

    WSContentSend_P (PSTR ("div.disabled {color:#666;border-color:#666;}\n"));

    WSContentSend_P (PSTR ("div.level1 {margin-top:-40px;}\n"));

    WSContentSend_P (PSTR ("div.choice {display:inline-block;padding:0px 2px;margin:0px;background:none;color:#fff;border:1px #666 solid;border-radius:6px;}\n"));
    WSContentSend_P (PSTR ("div.choice div {background:#666;}\n"));
    WSContentSend_P (PSTR ("div.choice a div {background:none;}\n"));

    WSContentSend_P (PSTR ("div.item {display:inline-block;margin:1px 0px;border-radius:4px;}\n"));
    WSContentSend_P (PSTR ("div a div.item:hover {background:#aaa;}\n"));
    WSContentSend_P (PSTR ("div.histo {font-size:0.9rem;padding:0px;margin-top:0px;}\n"));

    WSContentSend_P (PSTR ("div.prev {position:absolute;top:8px;left:2%%;padding:0px;}\n"));
    WSContentSend_P (PSTR ("div.next {position:absolute;top:8px;right:2%%;padding:0px;}\n"));
    WSContentSend_P (PSTR ("div.range {position:absolute;top:160px;left:2%%;padding:0px;}\n"));
    
    WSContentSend_P (PSTR ("div.period {width:60px;margin-top:2px;}\n"));
    WSContentSend_P (PSTR ("div.month {width:28px;font-size:0.85rem;}\n"));
    WSContentSend_P (PSTR ("div.day {width:16px;font-size:0.8rem;margin-top:2px;}\n"));
    
    WSContentSend_P (PSTR ("div.data {width:50px;}\n"));
    WSContentSend_P (PSTR ("div.size {width:25px;}\n"));
    WSContentSend_P (PSTR ("div a div.active {background:#666;}\n"));

    WSContentSend_P (PSTR ("select {background:#666;color:white;font-size:1rem;margin:1px;border:1px #666 solid;border-radius:6px;}\n"));

    WSContentSend_P (PSTR ("button {padding:2px;font-size:1rem;background:#444;color:%s;border:none;border-radius:6px;}\n"), COLOR_BUTTON_TEXT);
    WSContentSend_P (PSTR ("button.navi {padding:2px 16px;}\n"));
    WSContentSend_P (PSTR ("button.range {width:24px;margin-bottom:8px;}\n"));
    WSContentSend_P (PSTR ("button:hover {background:#aaa;}\n"));
    WSContentSend_P (PSTR ("button:disabled {background:%s;color:%s;}\n"), COLOR_BACKGROUND, COLOR_BACKGROUND);

    WSContentSend_P (PSTR ("button.menu {background:%s;color:%s;line-height:2rem;font-size:1.2rem;cursor:pointer;border:none;border-radius:0.3rem;width:150px;margin:2vh;}\n"), COLOR_BUTTON, COLOR_BUTTON_TEXT);

    WSContentSend_P (PSTR ("div.graph {width:100%%;margin:auto;margin-top:4px;}\n"));
    WSContentSend_P (PSTR ("svg.graph {width:100%%;height:60vh;}\n"));
    WSContentSend_P (PSTR ("</style>\n\n"));

    // page body
    WSContentSend_P (PSTR ("</head>\n"));
    WSContentSend_P (PSTR ("<body>\n"));

    // form start
    WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_TELEINFO_PAGE_GRAPH);

    // -------------------
    //      Unit range
    // -------------------

    WSContentSend_P (PSTR ("<div class='range'>\n"));
    WSContentSend_P (PSTR ("<button class='range' name='%s' id='%s' value=1>+</button><br>"), D_CMND_TELEINFO_PLUS,  D_CMND_TELEINFO_PLUS);
    WSContentSend_P (PSTR ("<button class='range' name='%s' id='%s' value=1>-</button>\n"),   D_CMND_TELEINFO_MINUS, D_CMND_TELEINFO_MINUS);
    WSContentSend_P (PSTR ("</div>\n"));        // range

    // --------------------------
    //      Previous and Next
    // --------------------------

#ifdef USE_UFILESYS
    if (teleinfo_graph.period != TELEINFO_PERIOD_LIVE)
    {
      // previous button
      if (TeleinfoSensorGraphPreviousPeriod (false)) strcpy (str_text, "");
        else strcpy (str_text, "disabled");
      WSContentSend_P (PSTR ("<div class='prev'><button class='navi' name='%s' id='%s' value=1 %s>&lt;&lt;</button></div>\n"), D_CMND_TELEINFO_PREV, D_CMND_TELEINFO_PREV, str_text);

      // next button
      if (TeleinfoSensorGraphNextPeriod (false)) strcpy (str_text, "");
        else strcpy (str_text, "disabled");
      WSContentSend_P (PSTR ("<div class='next'><button class='navi' name='%s' id='%s' value=1 %s>&gt;&gt;</button></div>\n"), D_CMND_TELEINFO_NEXT, D_CMND_TELEINFO_NEXT, str_text);
    }
#endif      // USE_UFILESYS

    // title
    WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText (SET_DEVICENAME));

    // -------------------------
    //    Icon and Live values
    // -------------------------
    
    WSContentSend_P (PSTR ("<div class='inline'>\n"));

    WSContentSend_P (PSTR ("<div class='live'>&nbsp;</div>\n"));

    // icon and counter
    WSContentSend_P (PSTR ("<div><img src='%s'><div class='count'><span id='msg'>%u</span></div></div>\n"), D_TELEINFO_ICON_LINKY_PNG, teleinfo_meter.nb_message);

    // live values
    WSContentSend_P (PSTR ("<div class='live'>\n"));
    for (phase = 0; phase < teleinfo_contract.phase; phase++)
    {
      // display phase inverted link
      strlcpy (str_text, teleinfo_graph.str_phase, sizeof (str_text));
      display = (str_text[phase] != '0');
      if (display) str_text[phase] = '0'; else str_text[phase] = '1';
      WSContentSend_P (PSTR ("<a href='%s?phase=%s'>"), D_TELEINFO_PAGE_GRAPH, str_text);

      // display phase data
      if (display) strcpy (str_text, ""); else strcpy_P (str_text, PSTR (" disabled"));
      WSContentSend_P (PSTR ("<div class='phase ph%u%s'><span id='v%u'>&nbsp;</span></div></a>\n"), phase, str_text, phase);    
    }

    WSContentSend_P (PSTR ("</div>\n"));      // live

    WSContentSend_P (PSTR ("</div>\n"));      // inline

    // --------------
    //    Level 1
    // --------------

    WSContentSend_P (PSTR ("<div class='level1'>\n"));
    WSContentSend_P (PSTR ("<div class='choice'>\n"));

    // Live button
    GetTextIndexed (str_text, sizeof (str_text), TELEINFO_PERIOD_LIVE, kTeleinfoGraphPeriod);
    if (teleinfo_graph.period != TELEINFO_PERIOD_LIVE) WSContentSend_P (PSTR ("<a href='%s?period=%d'>"), D_TELEINFO_PAGE_GRAPH, TELEINFO_PERIOD_LIVE);
    WSContentSend_P (PSTR ("<div class='item period'>%s</div>"), str_text);
    if (teleinfo_graph.period != TELEINFO_PERIOD_LIVE) WSContentSend_P (PSTR ("</a>"));
    WSContentSend_P (PSTR ("\n"));

    // other periods button
#ifdef USE_UFILESYS
    char str_filename[UFS_FILENAME_SIZE];

    for (index = TELEINFO_PERIOD_DAY; index < TELEINFO_PERIOD_YEAR; index++)
    {
      // get period label
      GetTextIndexed (str_text, sizeof (str_text), index, kTeleinfoGraphPeriod);

      // set button according to active state
      if (teleinfo_graph.period == index)
      {
        // get number of saved periods
        if (teleinfo_graph.period == TELEINFO_PERIOD_DAY) choice = teleinfo_config.param[TELEINFO_CONFIG_NBDAY];
        else if (teleinfo_graph.period == TELEINFO_PERIOD_WEEK) choice = teleinfo_config.param[TELEINFO_CONFIG_NBWEEK];
        else choice = 0;

        WSContentSend_P (PSTR ("<select name='histo' id='histo' onchange='this.form.submit();'>\n"));

        for (counter = 0; counter < choice; counter++)
        {
          // check if file exists
          if (TeleinfoSensorHistoGetFilename (teleinfo_graph.period, counter, str_filename))
          {
            TeleinfoSensorHistoGetDate (teleinfo_graph.period, counter, str_date);
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

    WSContentSend_P (PSTR ("</div></div>\n"));        // choice & level1

    // ------------
    //    Level 2
    // ------------

    WSContentSend_P (PSTR ("<div class='level2'>\n"));
    WSContentSend_P (PSTR ("<div class='choice'>\n"));

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

    WSContentSend_P (PSTR ("</div></div>\n"));      // choice & level2

    WSContentSend_P (PSTR ("</form>\n"));

    // ------------
    //     SVG
    // ------------

    // svg : start 
    WSContentSend_P (PSTR ("<div class='graph'>\n"));
    WSContentSend_P (PSTR ("<svg class='graph' viewBox='0 0 1200 %d' preserveAspectRatio='none'>\n"), TELEINFO_GRAPH_HEIGHT);

    // svg : style 
    WSContentSend_P (PSTR ("<style type='text/css'>\n"));

    WSContentSend_P (PSTR ("rect {stroke:grey;fill:none;}\n"));
    WSContentSend_P (PSTR ("line.dash {stroke:grey;stroke-width:1;stroke-dasharray:2 8;}\n"));
    WSContentSend_P (PSTR ("text.power {font-size:1.2rem;fill:white;}\n"));

    // svg : phase colors
    WSContentSend_P (PSTR ("path {stroke-width:1.5;opacity:0.8;fill-opacity:0.6;}\n"));
    for (phase = 0; phase < teleinfo_contract.phase; phase++) 
    {
      // phase colors
      GetTextIndexed (str_text, sizeof (str_text), phase, kTeleinfoColorPhase);
      WSContentSend_P (PSTR ("path.ph%d {stroke:%s;fill:%s;}\n"), phase, str_text, str_text);
      WSContentSend_P (PSTR ("path.ln%d {stroke:%s;fill:none;}\n"), phase, str_text);

      // peak colors
      GetTextIndexed (str_text, sizeof (str_text), phase, kTeleinfoColorPeak);
      WSContentSend_P (PSTR ("path.pk%d {stroke:%s;fill:none;stroke-dasharray:1 3;}\n"), phase, str_text);
    }

    // svg : text style
    WSContentSend_P (PSTR ("text {fill:white;text-anchor:middle;dominant-baseline:middle;}\n"));
    WSContentSend_P (PSTR ("text.value {font-style:italic;}}\n"));
    WSContentSend_P (PSTR ("text.time {font-size:1.2rem;}\n"));
    WSContentSend_P (PSTR ("text.month {font-size:1.2rem;}\n"));
    WSContentSend_P (PSTR ("text.day {font-size:0.9rem;}\n"));
    WSContentSend_P (PSTR ("text.hour {font-size:1rem;}\n"));

    // svg : time line
    WSContentSend_P (PSTR ("line.time {stroke-width:1;stroke:white;stroke-dasharray:1 8;}\n"));

    WSContentSend_P (PSTR ("</style>\n"));

    // svg : frame
    TeleinfoSensorGraphDisplayFrame ();

    // svg : time 
    TeleinfoSensorGraphDisplayTime (teleinfo_graph.period, teleinfo_graph.histo);

    // svg : curves
#ifdef USE_UFILESYS
    // if data is for current period file, force flush for the period
    if ((teleinfo_graph.histo == 0) && (teleinfo_graph.period != TELEINFO_PERIOD_LIVE)) TeleinfoSensorFlushDataHisto (teleinfo_graph.period);
#endif    // USE_UFILESYS

    // loop thru phases to display curves
    for (phase = 0; phase < teleinfo_contract.phase; phase++)
    {
      // set curve typa according to data type
      if ((teleinfo_graph.data == TELEINFO_UNIT_VA) || (teleinfo_graph.data == TELEINFO_UNIT_W)) strcpy (str_text, "ph");
        else strcpy (str_text, "ln");

      // display main and peak curve
      WSContentSend_P (PSTR ("<path id='m%u' class='%s%u' d='' />\n"), phase, str_text, phase);     // main curve
      WSContentSend_P (PSTR ("<path id='p%u' class='%s%u' d='' />\n"), phase, "pk",     phase);     // peak curve
    }

    // svg : end
    WSContentSend_P (PSTR ("</svg>\n"));
    WSContentSend_P (PSTR ("</div>\n"));

    // end of serving
    teleinfo_graph.serving = false;
  }

  // main menu button
  WSContentSend_P (PSTR ("<div><form action='/' method='get'><button class='menu'>%s</button></form></div>\n"), D_MAIN_MENU);
  
  // end of page
  WSContentStop ();

  // log page serving time
  AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: Graph in %ums"), millis () - timestart);
}

#ifdef USE_UFILESYS

// Graph public page
void TeleinfoSensorWebConso () { TeleinfoSensorWebHisto (TELEINFO_PERIOD_YEAR); }
void TeleinfoSensorWebProd () { TeleinfoSensorWebHisto (TELEINFO_PERIOD_PROD); }
void TeleinfoSensorWebHisto (const uint8_t period_type)
{
  uint8_t  choice, counter, index;  
  uint32_t timestart;
  TIME_T   time_dst;
  char     str_text[16];
  char     str_date[16];
  char     str_active[16];
  char     str_filename[UFS_FILENAME_SIZE];

  // timestamp
  timestart = millis ();

  // beginning of form without authentification
  WSContentStart_P (D_TELEINFO_GRAPH, false);

  // serve page if possible
  if (!teleinfo_graph.serving)
  {
    // start of serving
    teleinfo_graph.serving = true;

    // get numerical argument values
    teleinfo_graph.period = period_type;
    teleinfo_graph.data   = TELEINFO_UNIT_WH;
    teleinfo_graph.day    = (uint8_t)TeleinfoSensorWebGetArgValue (D_CMND_TELEINFO_DAY,   0, 31, teleinfo_graph.day);
    teleinfo_graph.month  = (uint8_t)TeleinfoSensorWebGetArgValue (D_CMND_TELEINFO_MONTH, 0, 12, teleinfo_graph.month);
    teleinfo_graph.histo  = (uint8_t)TeleinfoSensorWebGetArgValue (D_CMND_TELEINFO_HISTO, 0, 20, teleinfo_graph.histo);

    // graph increment
    choice = TeleinfoSensorWebGetArgValue (D_CMND_TELEINFO_PLUS, 0, 1, 0);
    if (choice == 1)
    {
      if (teleinfo_graph.month == 0) teleinfo_config.param[TELEINFO_CONFIG_MAX_MONTH] += TELEINFO_GRAPH_INC_WH_MONTH;
      else if (teleinfo_graph.day == 0) teleinfo_config.param[TELEINFO_CONFIG_MAX_DAY] += TELEINFO_GRAPH_INC_WH_DAY;
      else teleinfo_config.param[TELEINFO_CONFIG_MAX_HOUR] += TELEINFO_GRAPH_INC_WH_HOUR;
    }

    // graph decrement
    choice = TeleinfoSensorWebGetArgValue (D_CMND_TELEINFO_MINUS, 0, 1, 0);
    if (choice == 1)
    {
      if (teleinfo_graph.month == 0) teleinfo_config.param[TELEINFO_CONFIG_MAX_MONTH] = max ((long)TELEINFO_GRAPH_MIN_WH_MONTH, teleinfo_config.param[TELEINFO_CONFIG_MAX_MONTH] - TELEINFO_GRAPH_INC_WH_MONTH);
      else if (teleinfo_graph.day == 0) teleinfo_config.param[TELEINFO_CONFIG_MAX_DAY] = max ((long)TELEINFO_GRAPH_MIN_WH_DAY , teleinfo_config.param[TELEINFO_CONFIG_MAX_DAY] - TELEINFO_GRAPH_INC_WH_DAY);
      else teleinfo_config.param[TELEINFO_CONFIG_MAX_HOUR] = max ((long)TELEINFO_GRAPH_MIN_WH_HOUR, teleinfo_config.param[TELEINFO_CONFIG_MAX_HOUR] - TELEINFO_GRAPH_INC_WH_HOUR);
    }

    // handle previous page
    choice = TeleinfoSensorWebGetArgValue (D_CMND_TELEINFO_PREV, 0, 1, 0);
    if (choice == 1) TeleinfoSensorGraphPreviousPeriod (true);

    // handle next page
    choice = TeleinfoSensorWebGetArgValue (D_CMND_TELEINFO_NEXT, 0, 1, 0);
    if (choice == 1) TeleinfoSensorGraphNextPeriod (true);

    // javascript : screen swipe for previous and next period
    WSContentSend_P (PSTR ("\n\nlet startX=0;let stopX=0;let startY=0;let stopY=0;\n"));
    WSContentSend_P (PSTR ("window.addEventListener('touchstart',function(evt){startX=evt.changedTouches[0].pageX;startY=evt.changedTouches[0].pageY;},false);\n"));
    WSContentSend_P (PSTR ("window.addEventListener('touchend',function(evt){stopX=evt.changedTouches[0].pageX;stopY=evt.changedTouches[0].pageY;handleGesture();},false);\n"));
    WSContentSend_P (PSTR ("function handleGesture(){\n"));
    WSContentSend_P (PSTR (" let deltaX=stopX-startX;let deltaY=Math.abs(stopY-startY);\n"));
    WSContentSend_P (PSTR (" if(deltaY<10 && deltaX<-100){document.getElementById('%s').click();}\n"), D_CMND_TELEINFO_NEXT);
    WSContentSend_P (PSTR (" else if(deltaY<10 && deltaX>100){document.getElementById('%s').click();}\n"), D_CMND_TELEINFO_PREV);
    WSContentSend_P (PSTR ("}\n"));

    // end of script section
    WSContentSend_P (PSTR ("</script>\n\n"));

    // set page as scalable
    WSContentSend_P (PSTR ("<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=yes'/>\n"));

    // set cache policy, no cache for 12h
    WSContentSend_P (PSTR ("<meta http-equiv='Cache-control' content='public,max-age=720000'/>\n"));

    // page style
    WSContentSend_P (PSTR ("<style>\n"));

    WSContentSend_P (PSTR ("body {color:white;background-color:%s;font-family:Arial, Helvetica, sans-serif;}\n"), COLOR_BACKGROUND);
    WSContentSend_P (PSTR ("div {margin:8px auto;padding:2px 0px;text-align:center;vertical-align:top;}\n"));

    WSContentSend_P (PSTR ("a {text-decoration:none;}\n"));
    WSContentSend_P (PSTR ("div a {color:white;}\n"));

    WSContentSend_P (PSTR ("div.range {position:absolute;top:128px;left:2%%;padding:0px;}\n"));
    WSContentSend_P (PSTR ("div.prev {position:absolute;top:16px;left:2%%;padding:0px;}\n"));
    WSContentSend_P (PSTR ("div.next {position:absolute;top:16px;right:2%%;padding:0px;}\n"));

    WSContentSend_P (PSTR ("div.title {font-size:28px;}\n"));
    if (teleinfo_graph.period == TELEINFO_PERIOD_PROD) strcpy (str_text, TELEINFO_COLOR_PROD); else strcpy (str_text, TELEINFO_COLOR_CONSO);
    WSContentSend_P (PSTR ("div.type {font-size:24px;font-weight:bold;color:%s;}\n"), str_text);

    WSContentSend_P (PSTR ("div.level {margin-top:-4px;}\n"));

    WSContentSend_P (PSTR ("div.choice {display:inline-block;padding:0px 2px;margin:0px;background:none;color:#fff;border:1px #666 solid;border-radius:6px;}\n"));
    WSContentSend_P (PSTR ("div.choice div {background:#666;}\n"));
    WSContentSend_P (PSTR ("div.choice a div {background:none;}\n"));

    WSContentSend_P (PSTR ("div.item {display:inline-block;margin:1px 0px;border-radius:4px;}\n"));
    WSContentSend_P (PSTR ("div a div.item:hover {background:#aaa;}\n"));
    
    WSContentSend_P (PSTR ("div.month {width:28px;font-size:0.85rem;}\n"));
    WSContentSend_P (PSTR ("div.day {width:16px;font-size:0.8rem;margin-top:2px;}\n"));
    
    WSContentSend_P (PSTR ("div a div.active {background:#666;}\n"));

    WSContentSend_P (PSTR ("select {background:#666;color:white;font-size:1rem;margin:1px;border:1px #666 solid;border-radius:6px;}\n"));

    WSContentSend_P (PSTR ("button {padding:2px;font-size:1rem;background:#444;color:#fff;border:none;border-radius:6px;}\n"));
    WSContentSend_P (PSTR ("button.navi {padding:2px 16px;}\n"));
    WSContentSend_P (PSTR ("button.range {width:24px;margin-bottom:8px;}\n"));
    WSContentSend_P (PSTR ("button:hover {background:#aaa;}\n"));
    WSContentSend_P (PSTR ("button:disabled {background:%s;color:%s;}\n"), COLOR_BACKGROUND, COLOR_BACKGROUND);

    WSContentSend_P (PSTR ("button.menu {background:%s;color:%s;line-height:2rem;font-size:1.2rem;cursor:pointer;border:none;border-radius:0.3rem;width:150px;margin:2vh;}\n"), COLOR_BUTTON, COLOR_BUTTON_TEXT);

    WSContentSend_P (PSTR ("div.graph {width:100%%;margin:auto;margin-top:4px;}\n"));
    WSContentSend_P (PSTR ("svg.graph {width:100%%;height:60vh;}\n"));

    WSContentSend_P (PSTR ("</style>\n\n"));

    // page body
    WSContentSend_P (PSTR ("</head>\n"));
    WSContentSend_P (PSTR ("<body>\n"));

    // form start
    if (teleinfo_graph.period == TELEINFO_PERIOD_PROD) strcpy (str_text, D_TELEINFO_PAGE_GRAPH_PROD); else strcpy (str_text, D_TELEINFO_PAGE_GRAPH_CONSO);
    WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), str_text);

    // -------------------
    //      Unit range
    // -------------------

    WSContentSend_P (PSTR ("<div class='range'>\n"));
    WSContentSend_P (PSTR ("<button class='range' name='%s' id='%s' value=1>+</button><br>\n"), D_CMND_TELEINFO_PLUS, D_CMND_TELEINFO_PLUS);
    WSContentSend_P (PSTR ("<button class='range' name='%s' id='%s' value=1>-</button><br>\n"), D_CMND_TELEINFO_MINUS, D_CMND_TELEINFO_MINUS);
    WSContentSend_P (PSTR ("</div>\n"));

    // --------------------------
    //      Previous and Next
    // --------------------------

    // previous button
    if (TeleinfoSensorGraphPreviousPeriod (false)) strcpy (str_text, "");
      else strcpy (str_text, "disabled");
    WSContentSend_P (PSTR ("<div class='prev'><button class='navi' name='%s' id='%s' value=1 %s>&lt;&lt;</button></div>\n"), D_CMND_TELEINFO_PREV, D_CMND_TELEINFO_PREV, str_text);

    // next button
    if (TeleinfoSensorGraphNextPeriod (false)) strcpy (str_text, "");
      else strcpy (str_text, "disabled");
    WSContentSend_P (PSTR ("<div class='next'><button class='navi' name='%s' id='%s' value=1 %s>&gt;&gt;</button></div>\n"), D_CMND_TELEINFO_NEXT, D_CMND_TELEINFO_NEXT, str_text);

    // -----------------
    //    Title
    // -----------------

    WSContentSend_P (PSTR ("<div>\n"));

    WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText (SET_DEVICENAME));
    if (teleinfo_graph.period == TELEINFO_PERIOD_PROD) strcpy (str_text, "Production"); else strcpy (str_text, "Consommation");
    WSContentSend_P (PSTR ("<div class='type'>%s</div>\n"), str_text);

    WSContentSend_P (PSTR ("</div>\n"));

    // ---------------------
    //    Level 1 - Period 
    // ---------------------

    WSContentSend_P (PSTR ("<div class='level'>\n"));
    WSContentSend_P (PSTR ("<div class='choice'>\n"));

    // display available periods
    WSContentSend_P (PSTR ("<select name='histo' id='histo' onchange='this.form.submit();'>\n"));
    for (counter = 0; counter < 20; counter++)
    {
      // check if file exists
      if (TeleinfoSensorHistoGetFilename (teleinfo_graph.period, counter, str_filename))
      {
        TeleinfoSensorHistoGetDate (teleinfo_graph.period, counter, str_date);
        if (counter == teleinfo_graph.histo) strcpy_P (str_text, PSTR (" selected")); 
          else strcpy (str_text, "");
        WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>\n"), counter, str_text, str_date);
      }
    }

    WSContentSend_P (PSTR ("</select>\n"));      

    WSContentSend_P (PSTR ("</div></div>\n"));        // choice & level1

    // ------------
    //    Level 2
    // ------------

    WSContentSend_P (PSTR ("<div class='level'>\n"));
    WSContentSend_P (PSTR ("<div class='choice'>\n"));

    for (counter = 0; counter < 12; counter++)
    {
      // get month name
      strlcpy (str_date, kMonthNames + counter * 3, 4);

      // handle selected month
      strcpy (str_active, "");
      index = counter + 1;
      if (teleinfo_graph.month == index)
      {
        strcpy_P (str_active, PSTR (" active"));
        if ((teleinfo_graph.month != 0) && (teleinfo_graph.day == 0)) index = 0;
      }

      // display month selection
      if (teleinfo_graph.period == TELEINFO_PERIOD_PROD) strcpy (str_text, D_TELEINFO_PAGE_GRAPH_PROD); else strcpy (str_text, D_TELEINFO_PAGE_GRAPH_CONSO);
      WSContentSend_P (PSTR ("<a href='%s?month=%u&day=0'><div class='item month%s'>%s</div></a>\n"), str_text, index, str_active, str_date);       
    }

    WSContentSend_P (PSTR ("</div></div>\n"));      // choice & level2

    // --------------------
    //    Level 3 : Days 
    // --------------------

    WSContentSend_P (PSTR ("<div class='level'>\n"));

    if (teleinfo_graph.month != 0)
    {
      // calculate number of days in current month
      BreakTime (LocalTime (), time_dst);
      choice = TeleinfoSensorGetDaysInMonth (1970 + (uint32_t)time_dst.year - teleinfo_graph.histo, teleinfo_graph.month);

      // loop thru days in the month
      WSContentSend_P (PSTR ("<div class='choice'>\n"));
      for (counter = 1; counter <= choice; counter++)
      {
        // handle selected day
        strcpy (str_active, "");
        index = counter;
        if (teleinfo_graph.day == counter) strcpy_P (str_active, PSTR (" active"));
        if ((teleinfo_graph.day == counter) && (teleinfo_graph.day != 0)) index = 0;

        // display day selection
        if (teleinfo_graph.period == TELEINFO_PERIOD_PROD) strcpy (str_text, D_TELEINFO_PAGE_GRAPH_PROD); else strcpy (str_text, D_TELEINFO_PAGE_GRAPH_CONSO);
        WSContentSend_P (PSTR ("<a href='%s?day=%u'><div class='item day%s'>%u</div></a>\n"), str_text, index, str_active, counter);
      }

      WSContentSend_P (PSTR ("</div>\n"));        // choice
    }
    else WSContentSend_P (PSTR ("&nbsp;\n"));  

    WSContentSend_P (PSTR ("</div>\n"));        // level3

    WSContentSend_P (PSTR ("</form>\n"));

    // -------------
    //      SVG
    // -------------

    // svg : start
    WSContentSend_P (PSTR ("<div class='graph'>\n"));
    WSContentSend_P (PSTR ("<svg class='graph' viewBox='0 0 1200 %d' preserveAspectRatio='none'>\n"), TELEINFO_GRAPH_HEIGHT);

     // svg : style
    WSContentSend_P (PSTR ("<style type='text/css'>\n"));

    WSContentSend_P (PSTR ("rect {stroke:grey;fill:none;}\n"));
    WSContentSend_P (PSTR ("line.dash {stroke:grey;stroke-width:1;stroke-dasharray:2 8;}\n"));
    WSContentSend_P (PSTR ("text.power {font-size:1.2rem;fill:white;}\n"));

    // bar graph
    WSContentSend_P (PSTR ("path {stroke-width:1.5;opacity:0.8;fill-opacity:0.6;}\n"));
    if (teleinfo_graph.period == TELEINFO_PERIOD_PROD) strcpy (str_text, TELEINFO_COLOR_PROD_PREV); else strcpy (str_text, TELEINFO_COLOR_CONSO_PREV);
    WSContentSend_P (PSTR ("path.prev {stroke:%s;fill:%s;fill-opacity:1;}\n"), str_text, str_text);
    if (teleinfo_graph.period == TELEINFO_PERIOD_PROD) strcpy (str_text, TELEINFO_COLOR_PROD); else strcpy (str_text, TELEINFO_COLOR_CONSO);
    WSContentSend_P (PSTR ("path.now {stroke:%s;fill:%s;}\n"), str_text, str_text);
    if (teleinfo_graph.day == 0) WSContentSend_P (PSTR ("path.now:hover {fill-opacity:0.8;}\n"));

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

    // svg : frame
    TeleinfoSensorGraphDisplayFrame ();

    // svg : time
    TeleinfoSensorGraphDisplayTime (teleinfo_graph.period, teleinfo_graph.histo);

    // svg : curves
    TeleinfoSensorGraphDisplayBar (teleinfo_graph.histo + 1, false);       // previous period
    TeleinfoSensorGraphDisplayBar (teleinfo_graph.histo,     true);        // current period

    // svg : end
    WSContentSend_P (PSTR ("</svg>\n"));
    WSContentSend_P (PSTR ("</div>\n"));

    // end of serving
    teleinfo_graph.serving = false;
  }

  // main menu button
  WSContentSend_P (PSTR ("<div><form action='/' method='get'><button class='menu'>%s</button></form></div>\n"), D_MAIN_MENU);

  // end of page
  WSContentStop ();

  // log page serving time
  AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: Graph in %ums"), millis () - timestart);
}

#endif    // USE_UFILESYS

// Graph data update
void  TeleinfoSensorWebGraphUpdateData ()
{
  uint8_t  phase;
  uint32_t timestart;
  char     str_text[16];

  // timestamp
  timestart = millis ();

  // start stream
  WSContentBegin (200, CT_PLAIN);

  // serve page if possible
  if (!teleinfo_graph.serving)
  {
    // start of serving
    teleinfo_graph.serving = true;

    // send number of received messages
    WSContentSend_P (PSTR ("%u;"), teleinfo_meter.nb_message);

    // update phase data
    for (phase = 0; phase < teleinfo_contract.phase; phase++)
    {
      TeleinfoSensorGetCurrentDataDisplay (teleinfo_graph.data, phase, str_text, sizeof (str_text));
      WSContentSend_P (PSTR ("%s;"), str_text);
    }

    // end of serving
    teleinfo_graph.serving = false;
  }

  // end of stream
  WSContentEnd ();

  // log page serving time
  AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: Update Data in %ums"), millis () - timestart);
}

// Graph data update
void  TeleinfoSensorWebGraphUpdateCurve ()
{
  uint8_t  phase;
  uint32_t timestart;
  char     str_text[16];

  // timestamp
  timestart = millis ();

  // start stream
  WSContentBegin (200, CT_PLAIN);

  // serve page if possible
  if (!teleinfo_graph.serving)
  {
    // start of serving
    teleinfo_graph.serving = true;

    // loop thru phases to display main curve
    for (phase = 0; phase < teleinfo_contract.phase; phase++)
    {
      if (teleinfo_graph.str_phase[phase] == '1') TeleinfoSensorGraphDisplayCurve (phase, teleinfo_graph.data);
      WSContentSend_P (PSTR (";"));
    }
      
    // loop thru phases to display peak curve
    for (phase = 0; phase < teleinfo_contract.phase; phase++)
    {
      if (teleinfo_graph.str_phase[phase] == '1')
      {
        if (teleinfo_graph.data == TELEINFO_UNIT_VA) TeleinfoSensorGraphDisplayCurve (phase, TELEINFO_UNIT_PEAK_VA);
          else if (teleinfo_graph.data == TELEINFO_UNIT_V) TeleinfoSensorGraphDisplayCurve (phase, TELEINFO_UNIT_PEAK_V);
      }
      WSContentSend_P (PSTR (";"));
    }

    // end of serving
    teleinfo_graph.serving = false;
  }

  // end of stream
  WSContentEnd ();

  // log page serving time
  AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: Update Curve in %ums"), millis () - timestart);
}

#endif  // USE_WEBSERVER

/***************************************\
 *              Interface
\***************************************/

// Teleinfo sensor (for graph)
bool Xsns104 (uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function) 
  {
    case FUNC_MODULE_INIT:
      TeleinfoSensorModuleInit ();
      break;
    case FUNC_INIT:
      TeleinfoSensorInit ();
      break;
    case FUNC_SAVE_BEFORE_RESTART:
      TeleinfoDisableSerial ();
      TeleinfoSaveConfig ();
      TeleinfoSensorSaveBeforeRestart ();
      break;
    case FUNC_EVERY_SECOND:
      TeleinfoSensorEverySecond ();
      break;

#ifdef USE_UFILESYS
    case FUNC_SAVE_AT_MIDNIGHT:
      TeleinfoSensorMidnightSaveTotal ();
      TeleinfoSensorMidnightRotate ();
      break;
#endif      // USE_UFILESYS

#ifdef USE_WEBSERVER
     case FUNC_WEB_SENSOR:
      TeleinfoSensorWebSensor ();
      break;
    case FUNC_WEB_ADD_BUTTON:
      TeleinfoSensorWebConfigButton ();
      break;
     case FUNC_WEB_ADD_MAIN_BUTTON:
      TeleinfoSensorWebMainButton ();
      break;
    case FUNC_WEB_ADD_HANDLER:
      Webserver->on (FPSTR (D_TELEINFO_ICON_LINKY_PNG),   TeleinfoSensorLinkyIcon);
      Webserver->on (FPSTR (D_TELEINFO_PAGE_CONFIG),      TeleinfoSensorWebPageConfigure);
      Webserver->on (FPSTR (D_TELEINFO_PAGE_TIC),         TeleinfoSensorWebPageTic);
      Webserver->on (FPSTR (D_TELEINFO_PAGE_TIC_UPD),     TeleinfoSensorWebTicUpdate);
      Webserver->on (FPSTR (D_TELEINFO_PAGE_GRAPH),       TeleinfoSensorWebGraph);
      Webserver->on (FPSTR (D_TELEINFO_PAGE_GRAPH_DATA),  TeleinfoSensorWebGraphUpdateData);
      Webserver->on (FPSTR (D_TELEINFO_PAGE_GRAPH_CURVE), TeleinfoSensorWebGraphUpdateCurve);
#ifdef USE_UFILESYS
      Webserver->on (FPSTR (D_TELEINFO_PAGE_GRAPH_CONSO), TeleinfoSensorWebConso);
      Webserver->on (FPSTR (D_TELEINFO_PAGE_GRAPH_PROD),  TeleinfoSensorWebProd);
#endif      // USE_UFILESYS
      break;
#endif  // USE_WEBSERVER
  }

  return result;
}

#endif      // USE_TELEINFO_GRAPH
#endif      // USE_TELEINFO
