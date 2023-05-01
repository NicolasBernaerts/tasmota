/*
  xsns_104_teleinfo.ino - France Teleinfo energy sensor support for Tasmota devices

  Copyright (C) 2019-2023  Nicolas Bernaerts

  Version history :
    28/02/2023 - v11.0 - Split between xnrg and xsns

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

#ifdef USE_UFILESYS

// configuration and history files
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

// graph - periods
enum TeleinfoGraphPeriod { TELEINFO_PERIOD_LIVE, TELEINFO_PERIOD_DAY, TELEINFO_PERIOD_WEEK, TELEINFO_PERIOD_YEAR, TELEINFO_PERIOD_MAX };              // available graph periods
const char kTeleinfoGraphPeriod[] PROGMEM = "Live|Day|Week|Year";                                                                                     // period labels
const long ARR_TELEINFO_PERIOD_WINDOW[] = { 3600 / TELEINFO_GRAPH_SAMPLE, 86400 / TELEINFO_GRAPH_SAMPLE, 604800 / TELEINFO_GRAPH_SAMPLE };            // time window between samples (sec.)
const long ARR_TELEINFO_PERIOD_FLUSH[]  = { 1, 3600, 21600 };                                                                                         // max time between log flush (sec.)

// graph - display
enum TeleinfoGraphDisplay { TELEINFO_UNIT_VA, TELEINFO_UNIT_W, TELEINFO_UNIT_V, TELEINFO_UNIT_COSPHI, TELEINFO_UNIT_PEAK_VA, TELEINFO_UNIT_PEAK_V, TELEINFO_UNIT_WH, TELEINFO_UNIT_MAX };       // available graph units
const char kTeleinfoGraphDisplay[] PROGMEM = "VA|W|V|cosφ|VA|V|Wh";                                                                                                                             // units labels

// month and week day names
const char kTeleinfoYearMonth[] PROGMEM = "|Jan|Fév|Mar|Avr|Mai|Jun|Jui|Aoû|Sep|Oct|Nov|Déc";         // month name for selection
const char kTeleinfoWeekDay[]   PROGMEM = "Lun|Mar|Mer|Jeu|Ven|Sam|Dim";                              // day name for selection
const char kTeleinfoWeekDay2[]  PROGMEM = "lu|ma|me|je|ve|sa|di";                                     // day name for bar graph

/****************************************\
 *                 Data
\****************************************/

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
void TeleinfoSensorDisplayValue (const int unit_type, const long value, char* pstr_result, const int size_result, bool in_kilo) 
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
void TeleinfoSensorDisplayCurrentValue (const int unit_type, const int phase, char* pstr_result, const int size_result) 
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
  TeleinfoSensorDisplayValue (unit_type, value, pstr_result, size_result, false);
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
      sprintf_P (pstr_text, PSTR ("%02u/%02u"), start_dst.day_of_month, start_dst.month);
      break;

    // generate time label for week graph
    case TELEINFO_PERIOD_WEEK:
      sprintf_P (pstr_text, PSTR ("W %02u/%02u"), start_dst.day_of_month, start_dst.month);
      break;

    // generate time label for year graph
    case TELEINFO_PERIOD_YEAR:
      sprintf_P (pstr_text, PSTR ("%04u"), 1970 + start_dst.year - histo);
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
  else if (period == TELEINFO_PERIOD_YEAR) sprintf_P (pstr_filename, D_TELEINFO_HISTO_FILE_YEAR, RtcTime.year - histo);

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
void TeleinfoSensorSaveDataHisto (const uint8_t period, const uint8_t log_buffered)
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
  if ((teleinfo_period[period].str_filename != "") && (teleinfo_period[period].str_filename != str_filename)) TeleinfoSensorFlushDataHisto (period);

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
  if ((log_buffered == 0) || (teleinfo_period[period].str_buffer.length () > TELEINFO_HISTO_BUFFER_MAX)) TeleinfoSensorFlushDataHisto (period);
}

// rotate files according to file naming convention
//   file-2.csv -> file-3.csv
//   file-1.csv -> file-2.csv
void TeleinfoSensorFileRotate (const char* pstr_filename, const int index_min, const int index_max) 
{
  int  index;
  char str_original[UFS_FILENAME_SIZE];
  char str_target[UFS_FILENAME_SIZE];

  // check parameter
  if (pstr_filename == nullptr) return;

  // rotate previous daily files
  for (index = index_max; index > index_min; index--)
  {
    // generate file names
    sprintf (str_original, pstr_filename, index - 1);
    sprintf (str_target, pstr_filename, index);

    // if target exists, remove it
    if (ffsp->exists (str_target))
    {
      ffsp->remove (str_target);
      AddLog (LOG_LEVEL_INFO, PSTR ("UFS: deleted %s"), str_target);
    }

    // if file exists, rotate it
    if (ffsp->exists (str_original))
    {
      ffsp->rename (str_original, str_target);
      AddLog (LOG_LEVEL_INFO, PSTR ("UFS: renamed %s to %s"), str_original, str_target);
    }

    // handle received data
    TeleinfoReceiveData ();
  }
}

// Rotation of log files
void TeleinfoSensorRotateLog ()
{
  // log default method
  AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Rotate log files"));

  // flush daily and weekly records
  TeleinfoSensorFlushDataHisto (TELEINFO_PERIOD_DAY);
  TeleinfoSensorFlushDataHisto (TELEINFO_PERIOD_WEEK);

  // rotate daily files
  TeleinfoSensorFileRotate (D_TELEINFO_HISTO_FILE_DAY, 0, teleinfo_config.param[TELEINFO_CONFIG_NBDAY]);

  // if we are monday, week has changed, rotate previous weekly files
  if (RtcTime.day_of_week == 2) TeleinfoSensorFileRotate (D_TELEINFO_HISTO_FILE_WEEK, 0, teleinfo_config.param[TELEINFO_CONFIG_NBWEEK]);
}

// Save historisation data
void TeleinfoSensorSaveDailyTotal ()
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
  current_time = LocalTime ();
  BreakTime (current_time, today_dst);

  // if daily total has been updated and date is defined
  if ((teleinfo_meter.total_wh != 0) && (today_dst.year != 0))
  {
    // calculate daily ahd hourly delta
    delta = teleinfo_meter.total_wh - teleinfo_meter.last_day_wh;
    teleinfo_meter.hour_wh[RtcTime.hour] += (long)(teleinfo_meter.total_wh - teleinfo_meter.last_hour_wh);

    // update last day and hour total
    teleinfo_meter.last_day_wh = teleinfo_meter.total_wh;
    teleinfo_meter.last_hour_wh = teleinfo_meter.total_wh;

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
void TeleinfoSensorSaveDataPeriod (const uint8_t period)
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
  TeleinfoSensorSaveDataMemory (period);

#ifdef USE_UFILESYS
  // save to historisation file
  TeleinfoSensorSaveDataHisto (period, teleinfo_config.param[TELEINFO_CONFIG_BUFFER]);
#endif    // USE_UFILESYS

  // data updated for period
  teleinfo_period[period].updated = true;
}

// Save current values to graph data
void TeleinfoSensorSaveDataMemory (const uint8_t period)
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
  // stop serial reception and disable Teleinfo Rx
  TeleinfoDisableSerial ();

  // save graph configuration
  TeleinfoSaveConfig ();

  // update energy total (in kwh)
  //EnergyUpdateToday ();

#ifdef USE_UFILESYS
  // flush logs
  TeleinfoSensorFlushDataHisto ( TELEINFO_PERIOD_DAY);
  TeleinfoSensorFlushDataHisto ( TELEINFO_PERIOD_WEEK);

  // save daily total
  TeleinfoSensorSaveDailyTotal ();
#endif      // USE_UFILESYS
}

// Calculate if some JSON should be published (called every second)
void TeleinfoSensorEverySecond ()
{
  uint8_t  period, phase;
  uint32_t time_now;

  // do nothing during first 5 seconds
  if (TasmotaGlobal.uptime < 5) return;

  // current time
  time_now = LocalTime ();

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
    if (teleinfo_period[period].counter == 0) TeleinfoSensorSaveDataPeriod (period);
  }

  // if needed, init first meter total
  if (teleinfo_meter.last_day_wh  == 0) teleinfo_meter.last_day_wh  = teleinfo_meter.total_wh;
  if (teleinfo_meter.last_hour_wh == 0) teleinfo_meter.last_hour_wh = teleinfo_meter.total_wh;

  // if hour change, save hourly increment
  if ((RtcTime.minute == 59) && (RtcTime.second == 59))
  {
    // save hourly total
    teleinfo_meter.hour_wh[RtcTime.hour] += (long)(teleinfo_meter.total_wh - teleinfo_meter.last_hour_wh);
    teleinfo_meter.last_hour_wh = teleinfo_meter.total_wh;

#ifdef USE_UFILESYS
    // save daily data
    if (RtcTime.hour == 23) TeleinfoSensorSaveDailyTotal ();
#endif      // USE_UFILESYS
  }
}

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

// Display offload icons
void TeleinfoSensorWebIconTic () { Webserver->send_P (200, PSTR ("image/png"), teleinfo_icon_tic_png, teleinfo_icon_tic_len); }

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
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_TELEINFO_PAGE_TIC, "TIC Message");

  // Teleinfo graph page button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_TELEINFO_PAGE_GRAPH, "TIC Graph");
}

// Append Teleinfo configuration button to configuration page
void TeleinfoSensorWebConfigButton ()
{
  // heater and meter configuration button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_TELEINFO_PAGE_CONFIG, PSTR ("Configure Teleinfo"));
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
  if (Webserver->hasArg (F ("save")))
  {
    // parameter 'rate' : set teleinfo rate
    WebGetArg (D_CMND_TELEINFO_RATE, str_text, sizeof (str_text));
    if (strlen (str_text) > 0) teleinfo_config.baud_rate = (uint16_t)atoi (str_text);

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
void TeleinfoSensorWebTicUpdate ()
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
void TeleinfoSensorWebPageTic ()
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
void TeleinfoSensorGraphDisplayTime (const uint8_t period, const uint8_t histo)
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
void TeleinfoSensorGraphDisplayCurve (const uint8_t phase, const uint8_t data)
{
  bool analyse;
  int  index, index_array;
  long graph_x, graph_y, graph_range, graph_delta;
  long prev_x, prev_y, bezier_y1, bezier_y2; 
  long value, prev_value;
  long arr_value[TELEINFO_GRAPH_SAMPLE];
  char str_value[36];
  char str_buffer[512];

  // check parameters
  if (teleinfo_graph.period > TELEINFO_PERIOD_WEEK) return;
  if (teleinfo_config.max_power == 0) return;

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
    uint8_t  column;
    uint32_t len_buffer, size_buffer;
    char     str_filename[UFS_FILENAME_SIZE];
    char    *pstr_token, *pstr_buffer, *pstr_line;
    File     file;

    // if data file exists
    if (TeleinfoSensorHistoGetFilename (teleinfo_graph.period, teleinfo_graph.histo, str_filename))
    {
      // init
      strcpy(str_buffer, "");

      // set column with data
      column = 3 + (phase * 6) + data;

      //open file and skip header
      file = ffsp->open (str_filename, "r");
      while (file.available ())
      {
        // read next block
        size_buffer = strlen (str_buffer);
        len_buffer = file.readBytes (str_buffer + size_buffer, sizeof (str_buffer) - size_buffer - 1);
        str_buffer[size_buffer + len_buffer] = 0;

        // loop to read lines
        pstr_buffer = str_buffer;
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

          // handle received data
          TeleinfoReceiveData ();
        }

        // deal with remaining string
        if (pstr_buffer != str_buffer) strcpy (str_buffer, pstr_buffer);
          else strcpy(str_buffer, "");
      }

      // close file
      file.close ();
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
  strcpy (str_buffer, "");
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
          graph_y = TELEINFO_GRAPH_HEIGHT - (value * TELEINFO_GRAPH_HEIGHT / teleinfo_config.max_power);
          break;

        case TELEINFO_UNIT_V:
        case TELEINFO_UNIT_PEAK_V:
          graph_range = abs (teleinfo_config.max_volt - TELEINFO_VOLTAGE);
          graph_delta = (TELEINFO_VOLTAGE - value) * TELEINFO_GRAPH_HEIGHT / 2 / graph_range;
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
            sprintf (str_value, "d='M%d %d ", graph_x, TELEINFO_GRAPH_HEIGHT);
            strcat (str_buffer, str_value);

            // first point
            sprintf (str_value, "L %d %d ", graph_x, graph_y);
            strcat (str_buffer, str_value);
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
            strcat (str_buffer, str_value);
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
            strcat (str_buffer, str_value);
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
            strcat (str_buffer, str_value);
          }
          break;
      }

      // save previous y position
      prev_x = graph_x;
      prev_y = graph_y;
    }

    // if needed, flush buffer
    if ((strlen (str_buffer) > 480) || (index == TELEINFO_GRAPH_SAMPLE - 1))
    {
      WSContentSend_P (str_buffer);
      strcpy (str_buffer, "");
    }
  }

  // end data value curve
  switch (data)
  {
    case TELEINFO_UNIT_VA:
    case TELEINFO_UNIT_W:
      WSContentSend_P ("L%d,%d Z'/>\n", graph_x, TELEINFO_GRAPH_HEIGHT);
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
void TeleinfoSensorGraphDisplayBar (const uint8_t histo, const bool current)
{
  bool    analyse;
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
  char     str_filename[UFS_FILENAME_SIZE];
  char     str_buffer[512];
  char    *pstr_token, *pstr_buffer, *pstr_line, *pstr_error;
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

  // if current day, collect live values
  if ((histo == 0) && (teleinfo_graph.month == RtcTime.month) && (teleinfo_graph.day == RtcTime.day_of_month))
  {
    // update last hour increment
    teleinfo_meter.hour_wh[RtcTime.hour] += (long)(teleinfo_meter.total_wh - teleinfo_meter.last_hour_wh);
    teleinfo_meter.last_hour_wh = teleinfo_meter.total_wh;

    // init hour slots from live values
    for (index = 0; index < 24; index ++) if (teleinfo_meter.hour_wh[index] > 0) arr_value[index] = teleinfo_meter.hour_wh[index];
  }

  // calculate graph height and graph start
  graph_height = TELEINFO_GRAPH_HEIGHT;
  graph_x      = teleinfo_graph.left + graph_delta / 2;
  graph_x_end  = graph_x + graph_width - graph_delta;
  if (!current) { graph_x +=4; graph_x_end -= 4; }

  // if data file exists
  if (TeleinfoSensorHistoGetFilename (TELEINFO_PERIOD_YEAR, histo, str_filename))
  {
    // init
    strcpy(str_buffer, "");
    pstr_buffer = str_buffer;

    //open file and skip header
    file = ffsp->open (str_filename, "r");
    while (file.available ())
    {
      // read next block
      size_buffer = strlen (str_buffer);
      len_buffer = file.readBytes (str_buffer + size_buffer, sizeof (str_buffer) - size_buffer - 1);
      str_buffer[size_buffer + len_buffer] = 0;

      // loop to read lines
      pstr_buffer = str_buffer;
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
            if (arr_value[month] == LONG_MAX) arr_value[month] = value; else arr_value[month] += value;
          }

          // if line deals with target month, display days
          else if ((teleinfo_graph.month == month) && (teleinfo_graph.day == 0) && (index >= 4))
          {
            analyse = false;
            if (day > graph_stop) day = TELEINFO_GRAPH_MAX_BARGRAPH - 1;
            if (arr_value[day] == LONG_MAX) arr_value[day] = value; else arr_value[day] += value;
          }

          // if line deals with target month and target day, display hours
          else if ((teleinfo_graph.month == month) && (teleinfo_graph.day == day) && (index > 4) && (index < 24 + 5))
          {
            hour = index - 5;
            if (arr_value[hour] == LONG_MAX) arr_value[hour] = value; else arr_value[hour] += value;
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
      if (pstr_buffer != str_buffer) strcpy (str_buffer, pstr_buffer);
        else strcpy(str_buffer, "");

      // handle received data
      TeleinfoReceiveData ();
    }

    // close file
    file.close ();
  }

  // loop to display bar graphs
  for (index = graph_start; index <= graph_stop; index ++)
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
          TeleinfoSensorDisplayValue (TELEINFO_UNIT_MAX, arr_value[index], str_value, sizeof (str_value), true);
          WSContentSend_P (PSTR("<text class='%s value' x='%d' y='%d'>%s</text>\n"), str_type, value_x, value_y, str_value);
        }

        // month name or day / hour number
        // ----------------

        // if full year, get month name else get day of month
        if (teleinfo_graph.month == 0) GetTextIndexed (str_value, sizeof (str_value), index, kTeleinfoYearMonth);
          else if (teleinfo_graph.day == 0) sprintf (str_value, "%02d", index);
            else sprintf (str_value, "%dh", index);

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
void TeleinfoSensorGraphDisplayFrame ()
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
      for (index = 0; index < 5; index ++) TeleinfoSensorDisplayValue (TELEINFO_UNIT_MAX, index * teleinfo_config.max_power / 4, arr_label[index], sizeof (arr_label[index]), true);
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
      for (index = 0; index < 5; index ++) TeleinfoSensorDisplayValue (TELEINFO_UNIT_MAX, index * unit_max / 4, arr_label[index], sizeof (arr_label[index]), true);
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
void TeleinfoSensorWebGraphPage ()
{
  bool     display;
  uint8_t  phase, choice, counter;  
  uint16_t index;
  uint32_t year;
  uint32_t timestart;
  long     percentage;
  TIME_T   time_dst;
  char     str_type[8];
  char     str_text[16];
  char     str_date[16];

  // start timestamp
  timestart = millis ();

  // get numerical argument values
  teleinfo_graph.data  = (uint8_t)TeleinfoSensorWebGetArgValue (D_CMND_TELEINFO_DATA,  0, TELEINFO_UNIT_MAX - 1, teleinfo_graph.data);
  teleinfo_graph.day   = (uint8_t)TeleinfoSensorWebGetArgValue (D_CMND_TELEINFO_DAY,   0, 31, teleinfo_graph.day);
  teleinfo_graph.month = (uint8_t)TeleinfoSensorWebGetArgValue (D_CMND_TELEINFO_MONTH, 0, 12, teleinfo_graph.month);
  teleinfo_graph.histo = (uint8_t)TeleinfoSensorWebGetArgValue (D_CMND_TELEINFO_HISTO, 0, 52, teleinfo_graph.histo);
  choice = (uint8_t)TeleinfoSensorWebGetArgValue (D_CMND_TELEINFO_PERIOD, 0, TELEINFO_PERIOD_MAX - 1, teleinfo_graph.period);
  if (choice != teleinfo_graph.period) teleinfo_graph.histo = 0;
  teleinfo_graph.period = choice;  

  // if period is yearly, force data to Wh
  if (teleinfo_graph.period == TELEINFO_PERIOD_YEAR) teleinfo_graph.data = TELEINFO_UNIT_WH;

  // else if back to other curves, switch to W
  else if (teleinfo_graph.data == TELEINFO_UNIT_WH) teleinfo_graph.data = TELEINFO_UNIT_W;

  // check phase display argument
  if (Webserver->hasArg (D_CMND_TELEINFO_PHASE)) WebGetArg (D_CMND_TELEINFO_PHASE, teleinfo_graph.str_phase, sizeof (teleinfo_graph.str_phase));
  while (strlen (teleinfo_graph.str_phase) < teleinfo_contract.phase) strlcat (teleinfo_graph.str_phase, "1", sizeof (teleinfo_graph.str_phase));

  // graph increment
  choice = TeleinfoSensorWebGetArgValue (D_CMND_TELEINFO_PLUS, 0, 1, 0);
  if (choice == 1)
  {
    if ((teleinfo_graph.period == TELEINFO_PERIOD_YEAR) && (teleinfo_graph.month == 0)) teleinfo_config.param[TELEINFO_CONFIG_MAX_MONTH] += TELEINFO_GRAPH_INC_WH_MONTH;
    else if ((teleinfo_graph.period == TELEINFO_PERIOD_YEAR) && (teleinfo_graph.day == 0)) teleinfo_config.param[TELEINFO_CONFIG_MAX_DAY] += TELEINFO_GRAPH_INC_WH_DAY;
    else if (teleinfo_graph.period == TELEINFO_PERIOD_YEAR) teleinfo_config.param[TELEINFO_CONFIG_MAX_HOUR] += TELEINFO_GRAPH_INC_WH_HOUR;
    else if ((teleinfo_graph.data == TELEINFO_UNIT_VA) || (teleinfo_graph.data == TELEINFO_UNIT_W)) teleinfo_config.max_power += TELEINFO_GRAPH_INC_POWER;
    else if (teleinfo_graph.data == TELEINFO_UNIT_V) teleinfo_config.max_volt += TELEINFO_GRAPH_INC_VOLTAGE;
  }

  // graph decrement
  choice = TeleinfoSensorWebGetArgValue (D_CMND_TELEINFO_MINUS, 0, 1, 0);
  if (choice == 1)
  {
    if ((teleinfo_graph.period == TELEINFO_PERIOD_YEAR) && (teleinfo_graph.month == 0)) teleinfo_config.param[TELEINFO_CONFIG_MAX_MONTH] = max ((long)TELEINFO_GRAPH_MIN_WH_MONTH, teleinfo_config.param[TELEINFO_CONFIG_MAX_MONTH] - TELEINFO_GRAPH_INC_WH_MONTH);
    else if ((teleinfo_graph.period == TELEINFO_PERIOD_YEAR) && (teleinfo_graph.day == 0)) teleinfo_config.param[TELEINFO_CONFIG_MAX_DAY] = max ((long)TELEINFO_GRAPH_MIN_WH_DAY , teleinfo_config.param[TELEINFO_CONFIG_MAX_DAY] - TELEINFO_GRAPH_INC_WH_DAY);
    else if (teleinfo_graph.period == TELEINFO_PERIOD_YEAR) teleinfo_config.param[TELEINFO_CONFIG_MAX_HOUR] = max ((long)TELEINFO_GRAPH_MIN_WH_HOUR, teleinfo_config.param[TELEINFO_CONFIG_MAX_HOUR] - TELEINFO_GRAPH_INC_WH_HOUR);
    else if ((teleinfo_graph.data == TELEINFO_UNIT_VA) || (teleinfo_graph.data == TELEINFO_UNIT_W)) teleinfo_config.max_power = max ((long)TELEINFO_GRAPH_MIN_POWER, teleinfo_config.max_power - TELEINFO_GRAPH_INC_POWER);
    else if (teleinfo_graph.data == TELEINFO_UNIT_V) teleinfo_config.max_volt = max ((long)TELEINFO_GRAPH_MIN_VOLTAGE, teleinfo_config.max_volt - TELEINFO_GRAPH_INC_VOLTAGE);
  }

  // handle next page
  choice = TeleinfoSensorWebGetArgValue (D_CMND_TELEINFO_NEXT, 0, 1, 0);
  if ((choice == 1) && (teleinfo_graph.period != TELEINFO_PERIOD_LIVE))
  {
    if ((teleinfo_graph.period != TELEINFO_PERIOD_YEAR) && (teleinfo_graph.histo > 0)) teleinfo_graph.histo--;
    else if (teleinfo_graph.period == TELEINFO_PERIOD_YEAR)
    {
      // full year view
      if ((teleinfo_graph.day == 0) && (teleinfo_graph.month == 0))
      {
       if (teleinfo_graph.histo > 0) teleinfo_graph.histo--;
      }

      // month view
      else if (teleinfo_graph.day == 0)
      {
        if ((teleinfo_graph.month == 12) && (teleinfo_graph.histo > 0)) { teleinfo_graph.month = 1; teleinfo_graph.histo--; }
        else if (teleinfo_graph.month < 12) teleinfo_graph.month++;
      }

      // day view
      else
      {
        // calculate number of days in current month
        BreakTime (LocalTime (), time_dst);
        year = 1970 + (uint32_t)time_dst.year - teleinfo_graph.histo;
        choice = TeleinfoSensorGetDaysInMonth (year, teleinfo_graph.month);

        if (teleinfo_graph.day == choice) { teleinfo_graph.day = 1; teleinfo_graph.month++; }
        else teleinfo_graph.day++;

        if ((teleinfo_graph.month == 13) && (teleinfo_graph.histo > 0)) { teleinfo_graph.month = 1; teleinfo_graph.histo--; }
        else if (teleinfo_graph.month == 13) { teleinfo_graph.month = 12; teleinfo_graph.day = 31; }
      }
    }
  }

  // handle previous page
  choice = TeleinfoSensorWebGetArgValue (D_CMND_TELEINFO_PREV, 0, 1, 0);
  if ((choice == 1) && (teleinfo_graph.period != TELEINFO_PERIOD_LIVE))
  {
    if (teleinfo_graph.period != TELEINFO_PERIOD_YEAR) teleinfo_graph.histo++;
    else if (teleinfo_graph.period == TELEINFO_PERIOD_YEAR)
    {
      // full year view
      if ((teleinfo_graph.day == 0) && (teleinfo_graph.month == 0)) teleinfo_graph.histo++;

      // month view
      else if (teleinfo_graph.day == 0)
      {
        if (teleinfo_graph.month == 1) { teleinfo_graph.month = 12; teleinfo_graph.histo++; }
        else teleinfo_graph.month--;
      }

      // day view
      else
      {
        teleinfo_graph.day--;
        if (teleinfo_graph.day == 0) teleinfo_graph.month--;
        if (teleinfo_graph.month == 0) { teleinfo_graph.month = 12; teleinfo_graph.histo++; }
        if (teleinfo_graph.day == 0)
        {
          // calculate number of days in current month
          BreakTime (LocalTime (), time_dst);
          year = 1970 + (uint32_t)time_dst.year - teleinfo_graph.histo;
          teleinfo_graph.day = TeleinfoSensorGetDaysInMonth (year, teleinfo_graph.month);
        }
      }
    }
  }

  // beginning of form without authentification
  WSContentStart_P (D_TELEINFO_GRAPH, false);

  // javascript : screen swipe for previous and next period
  WSContentSend_P (PSTR ("\nlet startX=0;let stopX=0;\n"));
  WSContentSend_P (PSTR ("window.addEventListener('touchstart',function(evt){startX=evt.changedTouches[0].screenX;},false);\n"));
  WSContentSend_P (PSTR ("window.addEventListener('touchend',function(evt){stopX=evt.changedTouches[0].screenX;handleGesture();},false);\n"));
  WSContentSend_P (PSTR ("function handleGesture(){\n"));
  WSContentSend_P (PSTR ("if(stopX<startX-100){document.getElementById('%s').click();}\n"), D_CMND_TELEINFO_NEXT);
  WSContentSend_P (PSTR ("else if(stopX>startX+100){document.getElementById('%s').click();}\n"), D_CMND_TELEINFO_PREV);
  WSContentSend_P (PSTR ("}\n"));

  // end of script section
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
    GetTextIndexed (str_text, sizeof (str_text), phase, kTeleinfoColorPhase);
    WSContentSend_P (PSTR ("div.ph%d {color:%s;border:1px %s solid;}\n"), phase, str_text, str_text);    
  }
  WSContentSend_P (PSTR ("div.disabled {color:#666;border-color:#666;}\n"));

  WSContentSend_P (PSTR ("div.choice {display:inline-block;padding:0px 2px;margin:0px;background:none;color:#fff;border:1px #666 solid;border-radius:6px;}\n"));
  WSContentSend_P (PSTR ("div.choice div {background:#666;}\n"));
  WSContentSend_P (PSTR ("div.choice a div {background:none;}\n"));

  WSContentSend_P (PSTR ("div.item {display:inline-block;margin:1px 0px;border-radius:4px;}\n"));
  WSContentSend_P (PSTR ("div a div.item:hover {background:#aaa;}\n"));
  WSContentSend_P (PSTR ("div.histo {font-size:0.9rem;padding:0px;margin-top:0px;}\n"));

  WSContentSend_P (PSTR ("div.prev {position:absolute;top:16px;left:2%%;padding:0px;}\n"));
  WSContentSend_P (PSTR ("div.next {position:absolute;top:16px;right:2%%;padding:0px;}\n"));
  WSContentSend_P (PSTR ("div.range {position:absolute;top:48px;left:2%%;padding:0px;}\n"));
  
  WSContentSend_P (PSTR ("div.period {width:60px;margin-top:2px;}\n"));
  WSContentSend_P (PSTR ("div.month {width:28px;font-size:0.85rem;}\n"));
  WSContentSend_P (PSTR ("div.day {width:16px;font-size:0.8rem;margin-top:2px;}\n"));
  
  WSContentSend_P (PSTR ("div.data {width:50px;}\n"));
  WSContentSend_P (PSTR ("div.size {width:25px;}\n"));
  WSContentSend_P (PSTR ("div a div.active {background:#666;}\n"));

  WSContentSend_P (PSTR ("select {background:#666;color:white;font-size:1rem;margin:1px;border:1px #666 solid;border-radius:6px;}\n"));

  WSContentSend_P (PSTR ("button {padding:2px 12px;font-size:1rem;background:none;color:#fff;border:1px #666 solid;border-radius:6px;}\n"));
  WSContentSend_P (PSTR ("button:hover {background:#aaa;}\n"));
  WSContentSend_P (PSTR ("div.range button {width:32px;margin-bottom:8px;}\n"));

  WSContentSend_P (PSTR ("div.graph {width:100%%;margin:auto;margin-top:2vh;}\n"));
  WSContentSend_P (PSTR ("svg.graph {width:100%%;height:60vh;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // page body
  WSContentSend_P (PSTR ("</head>\n"));
  WSContentSend_P (PSTR ("<body>\n"));

  // -------------------
  //      Unit range
  // -------------------

  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_TELEINFO_PAGE_GRAPH);

  // graph increment
  if (teleinfo_graph.period != TELEINFO_PERIOD_LIVE)
  {
    WSContentSend_P (PSTR ("<div class='prev'><button name='%s' id='%s' value=1>&lt;&lt;</button></div>\n"), D_CMND_TELEINFO_PREV, D_CMND_TELEINFO_PREV);
    WSContentSend_P (PSTR ("<div class='next'><button name='%s' id='%s' value=1>&gt;&gt;</button></div>\n"), D_CMND_TELEINFO_NEXT, D_CMND_TELEINFO_NEXT);
  }

  WSContentSend_P (PSTR ("<div class='range'>\n"));
  WSContentSend_P (PSTR ("<button name='%s' id='%s' value=1>+</button><br>\n"), D_CMND_TELEINFO_PLUS, D_CMND_TELEINFO_PLUS);
  WSContentSend_P (PSTR ("<button name='%s' id='%s' value=1>-</button><br>\n"), D_CMND_TELEINFO_MINUS, D_CMND_TELEINFO_MINUS);
  WSContentSend_P (PSTR ("</div>\n"));

  // -----------------
  //      Icon
  // -----------------

  WSContentSend_P (PSTR ("<div><a href='/'><img height=64 src='%s'></a></div>\n"), D_TELEINFO_ICON_PNG);

  // --------------------------
  //    Level 1 : Navigation
  // --------------------------

  WSContentSend_P (PSTR ("<div>\n"));
  WSContentSend_P (PSTR ("<div class='choice'>\n"));

  // ---------------------
  //    Level 1 - Period 
  // ---------------------

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
      if (teleinfo_graph.period == TELEINFO_PERIOD_DAY) choice = teleinfo_config.param[TELEINFO_CONFIG_NBDAY];
      else if (teleinfo_graph.period == TELEINFO_PERIOD_WEEK) choice = teleinfo_config.param[TELEINFO_CONFIG_NBWEEK];
      else if (teleinfo_graph.period == TELEINFO_PERIOD_YEAR) choice = 20;
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

  WSContentSend_P (PSTR ("</div></div>\n"));        // choice

  // ------------
  //    Level 2
  // ------------

  WSContentSend_P (PSTR ("<div>\n"));
  WSContentSend_P (PSTR ("<div class='choice'>\n"));

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
      if (display) str_text[phase] = '0'; else str_text[phase] = '1';
      WSContentSend_P (PSTR ("<a href='%s?phase=%s'>"), D_TELEINFO_PAGE_GRAPH, str_text);

      // display phase data
      if (display) strcpy (str_text, ""); else strcpy_P (str_text, PSTR (" disabled"));
      WSContentSend_P (PSTR ("<div class='phase ph%u%s'>"), phase, str_text);    
      TeleinfoSensorDisplayCurrentValue (teleinfo_graph.data, phase, str_text, sizeof (str_text));
      WSContentSend_P (PSTR ("<span class='power' id='p%u'>%s</span>"), phase + 1, str_text);
      WSContentSend_P (PSTR ("</div></a>\n"));
    }
  }

  // --------------------
  //    Level 3 : Days 
  // --------------------

  else if ((teleinfo_graph.period == TELEINFO_PERIOD_YEAR) && (teleinfo_graph.month != 0))
  {
    // calculate number of days in current month
    BreakTime (LocalTime (), time_dst);
    choice = TeleinfoSensorGetDaysInMonth (1970 + (uint32_t)time_dst.year - teleinfo_graph.histo, teleinfo_graph.month);

    // loop thru days in the month
    WSContentSend_P (PSTR ("<div class='choice'>\n"));
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

  WSContentSend_P (PSTR ("</form>\n"));

  // ---------------
  //   SVG : Start 
  // ---------------

  WSContentSend_P (PSTR ("<div class='graph'>\n"));
  WSContentSend_P (PSTR ("<svg class='graph' viewBox='0 0 1200 %d' preserveAspectRatio='none'>\n"), TELEINFO_GRAPH_HEIGHT);

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
    GetTextIndexed (str_text, sizeof (str_text), phase, kTeleinfoColorPhase);
    WSContentSend_P (PSTR ("path.ph%d {stroke:%s;fill:%s;}\n"), phase, str_text, str_text);
    WSContentSend_P (PSTR ("path.ln%d {stroke:%s;fill:none;}\n"), phase, str_text);

    // peak colors
    GetTextIndexed (str_text, sizeof (str_text), phase, kTeleinfoColorPeak);
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
  if ((teleinfo_graph.histo == 0) && (teleinfo_graph.period != TELEINFO_PERIOD_LIVE)) TeleinfoSensorFlushDataHisto (teleinfo_graph.period);

  // if dealing with yearly data, display bar graph
  if (teleinfo_graph.period == TELEINFO_PERIOD_YEAR)
  {
    TeleinfoSensorGraphDisplayBar (teleinfo_graph.histo + 1, false);       // previous period
    TeleinfoSensorGraphDisplayBar (teleinfo_graph.histo,     true);        // current period
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
          TeleinfoSensorGraphDisplayCurve (phase, TELEINFO_UNIT_PEAK_VA);
          TeleinfoSensorGraphDisplayCurve (phase, TELEINFO_UNIT_VA);
          break;
          
        // Active Power
        case TELEINFO_UNIT_W:
          TeleinfoSensorGraphDisplayCurve (phase, TELEINFO_UNIT_W);
          break;

        // Voltage
        case TELEINFO_UNIT_V:
          TeleinfoSensorGraphDisplayCurve (phase, TELEINFO_UNIT_PEAK_V);
          TeleinfoSensorGraphDisplayCurve (phase, TELEINFO_UNIT_V);
          break;
        
        // Cos Phi
        case TELEINFO_UNIT_COSPHI:
          TeleinfoSensorGraphDisplayCurve (phase, TELEINFO_UNIT_COSPHI);
          break;
      }
    }

  // ---------------
  //   SVG : Frame
  // ---------------

  TeleinfoSensorGraphDisplayFrame ();

  // --------------
  //   SVG : Time 
  // --------------

  TeleinfoSensorGraphDisplayTime (teleinfo_graph.period, teleinfo_graph.histo);

  // -----------------
  //   SVG : End 
  // -----------------

  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentSend_P (PSTR ("</div>\n"));

  // end of page
  WSContentStop ();

  // log page serving time
  AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: Graph in %ums"), millis () - timestart);
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
      TeleinfoSensorSaveBeforeRestart ();
      break;
    case FUNC_EVERY_SECOND:
      TeleinfoSensorEverySecond ();
      break;

#ifdef USE_UFILESYS
    case FUNC_SAVE_AT_MIDNIGHT:
      TeleinfoSensorRotateLog ();
      break;
#endif      // USE_UFILESYS

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      Webserver->on (FPSTR (D_TELEINFO_PAGE_CONFIG), TeleinfoSensorWebPageConfigure);
      Webserver->on (FPSTR (D_TELEINFO_PAGE_TIC), TeleinfoSensorWebPageTic);
      Webserver->on (FPSTR (D_TELEINFO_PAGE_TIC_UPD), TeleinfoSensorWebTicUpdate);
      Webserver->on (FPSTR (D_TELEINFO_PAGE_GRAPH),TeleinfoSensorWebGraphPage);
      Webserver->on (FPSTR (D_TELEINFO_ICON_PNG), TeleinfoSensorWebIconTic);
      break;
    case FUNC_WEB_ADD_BUTTON:
      TeleinfoSensorWebConfigButton ();
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      TeleinfoSensorWebMainButton ();
      break;
#endif  // USE_WEBSERVER
  }

  return result;
}

#endif      // USE_TELEINFO_GRAPH
#endif      // USE_TELEINFO
