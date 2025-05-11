/*
  xsns_110_gazpar_histo.ino - France Gazpar energy meter graph : history

  Copyright (C) 2022-2025  Nicolas Bernaerts

  Version history :
    05/05/2025 v1.0 - Split from drv

  RAM : 724 bytes

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

#ifdef USE_GAZPAR
#ifdef USE_HISTO
#ifdef USE_UFILESYS

/*************************************************\
 *               Variables
\*************************************************/

// declare teleinfo graph curves
#define XSNS_110                      110

#define HISTO_VERSION                 2           // saved data version

#define HISTO_LINE_SIZE               64          // size of historisation line
#define HISTO_GRAPH_MAX               32          // maximum number of values in graph

#define HISTO_FS_MINSIZE              50          // minimum left size on FS before cleaning old files (kB)

// graph default and boundaries
#define HISTO_DEF_WH_DAY             100           // default hourly power in day graph
#define HISTO_DEF_WH_WEEK            1000          // default daily power in week graph
#define HISTO_DEF_WH_MONTH           1000          // default daily power in month graph
#define HISTO_DEF_WH_YEAR            25000         // default monthly power in year graph

// graph dimensions
#define HISTO_LEFT                    60          // left position of the curve
#define HISTO_WIDE                    1200        // graph usable width
#define HISTO_RIGHT                   1260        // right position of the curve
#define HISTO_WIDTH                   1320        // graph global width
#define HISTO_HEIGHT                  600         // default graph height
#define HISTO_BAR_HEIGHT_MIN          20          // min bar height to display value

#define HISTO_HEAD                    "Histo"

#define CMND_HISTO_MINUS              "minus"
#define CMND_HISTO_PLUS               "plus"
#define CMND_HISTO_PREV               "prev"
#define CMND_HISTO_NEXT               "next"
#define CMND_HISTO_DISPLAY            "display"
#define CMND_HISTO_PERIOD             "period"
#define CMND_HISTO_DAYOFWEEK          "dow"
#define CMND_HISTO_DAYOFMONTH         "dom"
#define CMND_HISTO_MONTHOFYEAR        "moy"

// week days name for history
static const char kHistoWeekdayNames[] = D_DAY3LIST;

// web URL
const char PSTR_PAGE_HISTO[] PROGMEM = "/histo";

enum HistoFile    { HISTO_FILE_BEFORE, HISTO_FILE_FOUND, HISTO_FILE_LAST, HISTO_FILE_BEYOND, HISTO_FILE_MAX };

enum HistoPeriod                  { HISTO_PERIOD_DAY, HISTO_PERIOD_WEEK, HISTO_PERIOD_MONTH, HISTO_PERIOD_YEAR, HISTO_PERIOD_MAX };
const char kHistoPeriod[] PROGMEM =      "Journée"     "|"      "Semaine"     "|"         "Mois"      "|"        "Année";                                                                                                                             // units labels

const char PSTR_HISTO_COLOR_DATA[] PROGMEM = "#09f";       // data bar color (blue)
const char PSTR_HISTO_COLOR_TEXT[] PROGMEM = "#fff";       // text bar color (white)

// files on FS
const char PSTR_HISTO_DATA[]  PROGMEM = "/gazpar-histo.dat";
const char PSTR_HISTO_FILE[]  PROGMEM = "/gazpar-%04u.csv";
const char PSTR_HISTO_MASK[]  PROGMEM = "gazpar-*.csv";

/****************************************\
 *                 Data
\****************************************/

struct histo_value {                // 8 bytes
  long liter;                              // total volume (in l)
  long wh;                                 // total power (in wh)
};

struct histo_delta {                // 8 bytes
  long liter;                              // delta of volume (in l)
  long wh;                                 // delta of power (in wh)
};

static struct {                     // 22 bytes
  uint8_t  hour      = UINT8_MAX;                 // last recorded hour
  uint8_t  period    = HISTO_PERIOD_DAY;          // current period of graph
  uint32_t timestamp = 0;                         // current timestamp for graph
  long     max_value[HISTO_PERIOD_MAX];           // max value according to period
} histo_status;

histo_delta histo_data[HISTO_GRAPH_MAX];          // display data for graph, 704 bytes

/*********************************************\
 *               Functions
\*********************************************/

// calculate number of days in given month
uint8_t HistoGetDaysInMonth (const uint32_t timestamp)
{
  uint8_t result;
  TIME_T  time_dst;

  // extract timestamp data
  BreakTime (timestamp, time_dst);

  // calculate number of days in given month
  if ((time_dst.month == 4) || (time_dst.month == 6) || (time_dst.month == 9 || (time_dst.month == 11))) result = 30;     // months with 30 days  
  else if (time_dst.month != 2)        result = 31;                                                                       // months with 31 days
  else if ((time_dst.year % 400) == 0) result = 29;                                                                       // leap year
  else if ((time_dst.year % 100) == 0) result = 28;                                                                       // not a leap year
  else if ((time_dst.year % 4) == 0)   result = 29;                                                                       // leap year
  else result = 28;                                                                                                       // not a leap year
  
  return result;
}

// get timestamp of first day of year
uint32_t HistoGetFirstDayOfYear (const uint32_t timestamp)
{
  uint8_t  dayofmonth;
  uint32_t time_calc;
  TIME_T   time_dst;

  // check parameter
  if (timestamp == 0) return 0;

  // calculate first day of year
  BreakTime (timestamp, time_dst);
  time_dst.month = 1;
  time_dst.day_of_month = 1;
  time_dst.day_of_week = 0;
  time_calc = MakeTime (time_dst);

  return time_calc;
}

// get timestamp of first day of month
uint32_t HistoGetFirstDayOfMonth (const uint32_t timestamp)
{
  uint8_t  dayofmonth;
  uint32_t time_calc;
  TIME_T   time_dst;

  // check parameter
  if (timestamp == 0) return 0;

  // get current day of year
  BreakTime (timestamp, time_dst);
  dayofmonth = time_dst.day_of_month - 1;

  // shift to start of week
  time_calc = timestamp - (uint32_t)dayofmonth * 86400;

  return time_calc;
}

// get timestamp of first day of week
uint32_t HistoGetFirstDayOfWeek (const uint32_t timestamp)
{
  uint8_t  dayofweek;
  uint32_t time_calc;
  TIME_T   time_dst;

  // check parameter
  if (timestamp == 0) return 0;

  // get current day of year
  BreakTime (timestamp, time_dst);
  dayofweek = (time_dst.day_of_week + 5) % 7;

  // shift to start of week
  time_calc = timestamp - (uint32_t)dayofweek * 86400;

  return time_calc;
}

// get current week number
uint8_t HistoGetWeekNumber (const uint32_t timestamp)
{
  uint16_t week, dayofyear, dayof1stweek, nbday1stweek;
  uint32_t time_calc;
  TIME_T   time_dst;

  // check parameter
  if (timestamp == 0) return 0;

  // get current day of year
  BreakTime (timestamp, time_dst);
  dayofyear = time_dst.day_of_year;

  //calculate 1st jan of same year
  time_dst.month        = 1;
  time_dst.day_of_month = 1;
  time_dst.day_of_year  = 1;
  time_dst.day_of_week  = 0;
  time_calc = MakeTime (time_dst);
  BreakTime (time_calc, time_dst);

  // calculate data for 1st week (monday = 0)
  dayof1stweek = (time_dst.day_of_week + 5) % 7;
  nbday1stweek = 7 - dayof1stweek;

  // if first week starts on monday, simple
  if (dayof1stweek == 0) week = 1 + (dayofyear - 1) / 7;

  // else if day within first incomplete week
  else if (dayofyear <= nbday1stweek) week = 1;

  // else if day after first incomplete week
//  else week = 2 + (dayofyear - 1 - nbday1stweek) / 7;
  else week = 2 + (dayofyear - nbday1stweek) / 7;

  return (uint8_t)week;
}

// check if week is complete (0 if complete, missing week number if incomplete)
uint8_t HistoIsWeekComplete (const uint32_t timestamp)
{
  uint8_t  week, result;
  uint32_t time_calc;
  TIME_T   time_dst;

  // calculate week number
  week = HistoGetWeekNumber (timestamp);

  // if week is complete
  if (week == 53) result = 1;
  else if (week == 1)
  {
    // calculate 1st of jan for current year
    BreakTime (timestamp, time_dst);
    time_dst.month        = 1;
    time_dst.day_of_month = 1;
    time_dst.day_of_week  = 0;
    time_dst.day_of_year  = 0;
    time_calc = MakeTime (time_dst);
    BreakTime (time_calc, time_dst);
  
    // if 1st is monday, week is complete else incomplete
    if (time_dst.day_of_week == 2) result = 0;
      else result = 53;
  }
  else result = 0;

  return result;
}

// calculate start timestamp according period and date
uint32_t HistoGetPreviousTimestamp (const uint8_t period, const uint32_t timestamp, const uint32_t quantity)
{
  uint32_t result = 0;
  TIME_T   calc_dst;

  // check current timestamp
  if (period >= HISTO_PERIOD_MAX) return 0;
  if (timestamp == 0) return 0;
  if (quantity == 0) return 0;

  // calculate according to period
  switch (period)
  {
    case HISTO_PERIOD_DAY :
      result = timestamp - 86400 * quantity;
      break;

    case HISTO_PERIOD_WEEK :
      result = timestamp - 7 * 86400 * quantity;
      break;

    case HISTO_PERIOD_MONTH :
      BreakTime (timestamp, calc_dst);
      calc_dst.day_of_week = 0;
      if (calc_dst.day_of_month > 28) calc_dst.day_of_month = 28;
      if (calc_dst.month > quantity) calc_dst.month = calc_dst.month - quantity;
      else { calc_dst.month = calc_dst.month + 12 - quantity; calc_dst.year--; }
      result = MakeTime (calc_dst);
      break;

    case HISTO_PERIOD_YEAR :
      result = timestamp - 365 * 86400 * quantity;
      break;
  }

  return result;
}

// calculate start timestamp according period and date
uint32_t HistoGetNextTimestamp (const uint8_t period, const uint32_t timestamp, const uint32_t quantity)
{
  uint32_t result = 0;
  TIME_T   calc_dst;

  // check current timestamp
  if (period >= HISTO_PERIOD_MAX) return 0;
  if (timestamp == 0) return 0;
  if (quantity == 0) return 0;

  // calculate according to period
  switch (period)
  {
    case HISTO_PERIOD_DAY:
      result = timestamp + 86400 * quantity;
      break;

    case HISTO_PERIOD_WEEK:
      result = timestamp + 7 * 86400 * quantity;
      break;

    case HISTO_PERIOD_MONTH:
      BreakTime (timestamp, calc_dst);
      calc_dst.day_of_week = 0;
      if (calc_dst.day_of_month > 28) calc_dst.day_of_month = 28;
      calc_dst.month = calc_dst.month + quantity;
      if (calc_dst.month > 12) { calc_dst.month = calc_dst.month - 12; calc_dst.year++; }
      result = MakeTime (calc_dst);
      break;

    case HISTO_PERIOD_YEAR:
      result = timestamp + 365 * 86400 * quantity;
      break;
  }

  return result;
}

// get label according to periods and timestamp limits
void HistoGetPeriodLabel (const uint8_t period, const uint32_t timestamp, char* pstr_label, const size_t size_label)
{
  uint32_t calc_time, delta;
  TIME_T   start_dst, stop_dst;
  char     str_start[4];
  char     str_stop[4];

  // check parameters
  if (period >= HISTO_PERIOD_MAX) return;
  if (pstr_label == nullptr) return;
  if (size_label < 24) return;
  
  // init
  pstr_label[0] = 0;

  // calculate according to period
  switch (period)
  {
    case HISTO_PERIOD_DAY:
      BreakTime (timestamp, start_dst);
      strlcpy (str_start, kMonthNames + start_dst.month * 3 - 3, 4);
      sprintf_P (pstr_label, PSTR ("%u %s %u"), start_dst.day_of_month, str_start, 1970 + start_dst.year);
      break;

    case HISTO_PERIOD_WEEK:
      BreakTime (timestamp, start_dst);
      delta = (uint32_t)(start_dst.day_of_week + 5) % 7;
      calc_time = timestamp - delta * 86400;
      BreakTime (calc_time, start_dst);
      calc_time = calc_time + 6 * 86400;
      BreakTime (calc_time, stop_dst);
      strlcpy (str_start, kMonthNames + start_dst.month * 3 - 3, 4);
      strlcpy (str_stop,  kMonthNames + stop_dst.month  * 3 - 3, 4);
      if (start_dst.month == stop_dst.month) sprintf_P (pstr_label, PSTR ("%u - %u %s %u"), start_dst.day_of_month, stop_dst.day_of_month, str_stop, 1970 + stop_dst.year);
        else sprintf_P (pstr_label, PSTR ("%u %s - %u %s %u"), start_dst.day_of_month, str_start, stop_dst.day_of_month, str_stop, 1970 + stop_dst.year);
      break;

    case HISTO_PERIOD_MONTH:
      BreakTime (timestamp, start_dst);
      strlcpy (str_start, kMonthNames + start_dst.month * 3 - 3, 4);
      sprintf_P (pstr_label, PSTR ("%s %u"), str_start, 1970 + start_dst.year);
      break;

    case HISTO_PERIOD_YEAR:
      BreakTime (timestamp, start_dst);
      sprintf_P (pstr_label, PSTR ("%u"), 1970 + start_dst.year);
      break;
  }
}

/*********************************************\
 *                 Files
\*********************************************/

bool HistoFileIsCandidate (const char* pstr_filename, const time_t filetime, time_t& prevtime)
{
  bool  candidate = false;
  char *pstr_digit;
  char  str_mask[32];

  // check parameters
  if (pstr_filename == nullptr) return false;

  // setup file mask
  strcpy_P (str_mask, PSTR_HISTO_MASK);
  pstr_digit = strchr (str_mask, '*');
  *pstr_digit++ = 0;

  // check file mask (without leading /)
  candidate = (strncmp (str_mask, pstr_filename, strlen (str_mask)) == 0);
  if (candidate) candidate = (strstr (pstr_filename, pstr_digit) != nullptr);

  // if file mask matches
  if (candidate)
  {
    // handle first found file
    if (prevtime == 0) prevtime = filetime;

    // discard file younger than previous one
    if (filetime > prevtime) candidate = false;
      else prevtime = filetime;
    
    // log
    AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: match %s"), pstr_filename);
  }

  return candidate;
}

// check FS size and clenup oldest file if needed
void HistoFileCleanup ()
{
  uint32_t free;
  time_t   time_target = 0;
  String   str_root, str_target;

  // get space left on FS (in kB)
  free = UfsFree ();
  AddLog (LOG_LEVEL_INFO, PSTR ("GAZ: free %u kB for %u kB minimum"), free, HISTO_FS_MINSIZE);

  // if not enough space, loop to find oldest file
  str_root = F ("/");
  if (free < HISTO_FS_MINSIZE)
  {
#ifdef ESP32
    File directory = ffsp->open (str_root.c_str ());
    File file      = directory.openNextFile ();
    while (file) 
    {
      if (HistoFileIsCandidate (file.name (), file.getLastWrite (), time_target)) str_target = str_root + file.name ();
      file = directory.openNextFile ();
    }

#else     // ESP8266
    Dir directory = ffsp->openDir (str_root.c_str ());
    while (directory.next ())
      if (HistoFileIsCandidate (directory.fileName ().c_str (), directory.fileTime (), time_target)) str_target = str_root + directory.fileName ();

#endif    // ESP32 or ESP8266
  }

  // if file was found
  if (str_target != "")
  {
    // remove candidate file
    ffsp->remove (str_target.c_str ());
    
    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("GAZ: removed %s"), str_target.c_str ());
  }
}

// Check and create historisation file
//   dddwwdmmddhh;liter;wh
void HistoFilePrepare (const uint16_t year)
{
  char str_text[HISTO_LINE_SIZE];
  File file;

  // set filename
  sprintf_P (str_text, PSTR_HISTO_FILE, year);

  // if file exists, ignore
  if (ffsp->exists (str_text)) return;

  // open file in create mode
  file = ffsp->open (str_text, "w");

  // generate header
  strcpy_P (str_text, PSTR ("dddwwdmmddhh;liter;wh\n"));
  file.print (str_text);

  // first line : beginning
  sprintf_P (str_text, PSTR ("000000000000;%d;%d\n"), gazpar_config.total, GazparConvertLiter2Wh (gazpar_config.total));
  file.print (str_text);

  // close file
  file.close ();
}

// Save historisation data (max 630k per year)
//   dddwwdmmddhh;prod;conso1;conso2;...
void HistoFileSaveData ()
{
  uint8_t   index, week, doweek;
  uint16_t  year;
  uint32_t  time_now;
  TIME_T    time_dst;
  char      str_text[HISTO_LINE_SIZE];
  File      file;

  // if date not defined, cancel
  if (!RtcTime.valid) return;

  // calculate timestamp (30sec before to get previous slot)
  time_now = LocalTime () - 30;
  BreakTime (time_now, time_dst);
  doweek = 1 + (time_dst.day_of_week + 5) % 7;
  week   = HistoGetWeekNumber (time_now);
  year   = time_dst.year;

  // if needed, create file
  HistoFilePrepare (year + 1970);

  // open in append mode
  sprintf_P (str_text, PSTR_HISTO_FILE, year + 1970);
  file = ffsp->open (str_text, "a");

  // generate line
  sprintf_P (str_text, PSTR ("%03u%02u%u%02u%02u%02u;%d;%d\n"), time_dst.day_of_year, week, doweek, time_dst.month, time_dst.day_of_month, time_dst.hour, gazpar_config.total, GazparConvertLiter2Wh (gazpar_config.total));

  // write line and close file
  file.print (str_text);
  file.close ();
}

// extract data from a line
int HistoExtractLine (char* pstr_line, struct histo_value &value)
{
  int   column = 0;
  char *pstr_token;

  // loop thru delimiter
  pstr_token = strtok (pstr_line, ";");
  while (pstr_token)
  {
    // extract data according to column
    column++;
    if (column == 2) value.liter = atol (pstr_token);
    else if (column == 3) value.wh = atol (pstr_token);

    // look for next token
    pstr_token = strtok (nullptr, ";");
  }

 return column;
}

// load data from file
bool HistoFileLoadPeriod (const uint8_t period, const uint32_t timestamp)
{
  bool     read_next;
  int      index_min, index_begin, index_previous, index_current;
  uint8_t  count, status;
  uint32_t time_now;
  size_t   tgt_start, tgt_length, idx_start, idx_length;
  size_t   length, line_first, line_current;
  char     str_target[4];
  char     str_index[4];
  char     str_previous[HISTO_LINE_SIZE];
  char     str_current[HISTO_LINE_SIZE];
  TIME_T   time_dst;
  File     file;
  histo_value total_begin, total_stop;

  // check parameters
  if (period >= HISTO_PERIOD_MAX) return false;
  if (timestamp == 0) return false;

  // check data file presence
  BreakTime (timestamp, time_dst);
  sprintf_P (str_current, PSTR_HISTO_FILE, 1970 + time_dst.year);
  if (!ffsp->exists (str_current)) return false;
  
  // init data
  time_now = millis ();
  status   = HISTO_FILE_BEFORE;
  line_first   = 0;
  line_current = 0;
  index_begin    = INT_MAX;
  index_previous = INT_MAX;
  index_current  = INT_MAX;
  memset (&total_begin, 0, sizeof (histo_value));
  memset (&total_stop,  0, sizeof (histo_value));

  // prepare data according to period
  switch (period)
  {
    case HISTO_PERIOD_DAY:
      tgt_start = 0;  tgt_length = 3;
      idx_start = 10; idx_length = 2;
      index_min = 0;
      sprintf_P (str_target, PSTR ("%03u"), time_dst.day_of_year);
      break;

    case HISTO_PERIOD_WEEK:
      tgt_start = 3; tgt_length = 2;
      idx_start = 5; idx_length = 1;
      index_min = 1;
      count = HistoGetWeekNumber (timestamp);
      sprintf_P (str_target, PSTR ("%02u"), count);
      break;

    case HISTO_PERIOD_MONTH:
      tgt_start = 6; tgt_length = 2;
      idx_start = 8; idx_length = 2;
      index_min = 1;
      sprintf_P (str_target, PSTR ("%02u"), time_dst.month);
      break;

    case HISTO_PERIOD_YEAR:
      tgt_start = 0; tgt_length = 0;
      idx_start = 6; idx_length = 2;
      index_min = 1;
      str_target[0] = 0;
      break;
  }

  // open file and skip header
  file = ffsp->open (str_current, "r");
  file.readBytesUntil ('\n', str_current, sizeof (str_current) - 1);

  // parse file
  read_next = file.available ();
  while (read_next)
  {
    // read next line
    length    = file.readBytesUntil ('\n', str_current, sizeof (str_current) - 1);
    read_next = file.available ();

    // handle current line
    str_current[length] = 0;
    line_current++;

    // check if target is found or beyond
    if (period == HISTO_PERIOD_YEAR) (line_current > 1) ? status = HISTO_FILE_FOUND : status = HISTO_FILE_BEFORE;
    else if (strncmp (str_current + tgt_start, str_target, tgt_length) == 0) status = HISTO_FILE_FOUND;
    else if (status == HISTO_FILE_FOUND) status = HISTO_FILE_BEYOND;

    // if target found and end of file is reached, set flag to read last line
    if (!read_next && (status == HISTO_FILE_FOUND)) status = HISTO_FILE_LAST;

    // if beyond target, stop to read file
    if (status == HISTO_FILE_BEYOND) read_next = false;

    // if line should be analysed
    if (status > HISTO_FILE_BEFORE)
    {
      // if needed, set first line
      if (line_first == 0) line_first = line_current;

      // read new index
      strlcpy (str_index, str_current + idx_start, idx_length + 1);
      index_current = (uint8_t)atoi (str_index);

      // if no index for previous line, set current index
      if (index_previous == INT_MAX) index_previous = index_current;

      // if no begin index, extract values from previous line (wich was not within target)
      if (index_begin == INT_MAX) 
      {
        index_begin = index_min - 1;
        HistoExtractLine (str_previous, total_begin);
      }

      // if beyond target, set current index to max value
      if (status == HISTO_FILE_BEYOND) index_current = INT_MAX;

      // if index has changed from previous line, calculate delta for current valid index
      if (index_current > index_previous)
      {
        if ((index_previous < HISTO_GRAPH_MAX) && (index_previous > index_begin))
        {
          // extract totals from previous line
          HistoExtractLine (str_previous, total_stop);

          // save delta from last valid index till previous index
          histo_data[index_previous].liter = total_stop.liter - total_begin.liter;
          histo_data[index_previous].wh    = total_stop.wh    - total_begin.wh;
        }

        // set begin totals to previous ones
        index_begin = index_previous;
        total_begin = total_stop;
      }

      // handle last line of file if it is within index target
      if (status == HISTO_FILE_LAST)
      {
        if ((index_current < HISTO_GRAPH_MAX) && (index_current > index_begin))
        {
          // extract totals from current line
          HistoExtractLine (str_current, total_stop);
  
          // save delta from last valid index till previous index
          histo_data[index_current].liter = total_stop.liter - total_begin.liter;
          histo_data[index_current].wh    = total_stop.wh    - total_begin.wh;
        }
      }

      // save previous index
      index_previous = index_current;
    }

    // save previous line data
    strcpy (str_previous, str_current);

    // do a yield every 500 lines
    if (line_current % 500 == 0) yield ();
  }

  // log data loading
  if (line_first == 0) strcpy_P (str_current, PSTR ("no data"));
    else sprintf_P (str_current, PSTR ("lines %u to %u"),  line_first, line_current);
  AddLog (LOG_LEVEL_DEBUG, PSTR ("GAZ: Loaded %u, period %u, target %s, %s [%ums]"), 1970 + time_dst.year, histo_status.period, str_target, str_current, millis () - time_now);

  return (line_first > 0);
}

// load data from file
void HistoFileLoadData ()
{
  uint8_t week;

  // init data
  memset (&histo_data, 0, sizeof (histo_delta) * HISTO_GRAPH_MAX);

  // check parameters
  if (histo_status.period >= HISTO_PERIOD_MAX) return;
  if (histo_status.timestamp == 0) return;

  // load data for current period
  HistoFileLoadPeriod (histo_status.period, histo_status.timestamp);

  // if dealing with weekly period
  if (histo_status.period == HISTO_PERIOD_WEEK)
  {
    // get current week number
    week = HistoIsWeekComplete (histo_status.timestamp);

    // if needed, append first week of next year (6 days shift)
    if (week == 1) HistoFileLoadPeriod (histo_status.period, histo_status.timestamp + 518400);

    // else if needed, append last week of previous year (6 days shift)
    else if (week == 53) HistoFileLoadPeriod (histo_status.period, histo_status.timestamp - 518400);
  }
}

/*********************************************\
 *                   Callback
\*********************************************/

// load config file
void HistoInit ()
{
  uint8_t  version;
  uint32_t time_save;
  char     str_filename[24];
  File     file;

  // default max value according to period
  histo_status.max_value[HISTO_PERIOD_DAY]   = HISTO_DEF_WH_DAY;
  histo_status.max_value[HISTO_PERIOD_WEEK]  = HISTO_DEF_WH_WEEK;
  histo_status.max_value[HISTO_PERIOD_MONTH] = HISTO_DEF_WH_MONTH;
  histo_status.max_value[HISTO_PERIOD_YEAR]  = HISTO_DEF_WH_YEAR;

  // open file in read mode
  strcpy_P (str_filename, PSTR_HISTO_DATA);
  if (ffsp->exists (str_filename))
  {
    file = ffsp->open (str_filename, "r");
    file.read ((uint8_t*)&version, sizeof (version));
    if (version == HISTO_VERSION)
    {
      file.read ((uint8_t*)&time_save, sizeof (time_save));
      file.read ((uint8_t*)&histo_status, sizeof (histo_status));
    }
    file.close ();
  }
}

// called every second
void HistoEverySecond ()
{
  TIME_T time_dst;

  // ignore if time is not valid
  if (!RtcTime.valid) return;

  // extract current time
  BreakTime (LocalTime (), time_dst);

  // init historisation to today's day
  if (histo_status.timestamp == 0) histo_status.timestamp = LocalTime ();

  // detect hour change to save data
  if (histo_status.hour == UINT8_MAX) histo_status.hour = time_dst.hour;
  if (histo_status.hour != time_dst.hour)
  {
    // free space on FS if needed
    HistoFileCleanup ();

    // save previous hour data
    HistoFileSaveData ();
  }
  histo_status.hour = time_dst.hour;
}

// save config file
void HistoSaveBeforeRestart ()
{
  uint8_t  version;
  uint32_t time_now;
  char     str_filename[24];
  File     file;

  // init data
  version  = HISTO_VERSION;
  time_now = LocalTime ();

  // open file in write mode
  strcpy_P (str_filename, PSTR_HISTO_DATA);
  file = ffsp->open (str_filename, "w");
  if (file > 0)
  {
    file.write ((uint8_t*)&version, sizeof (version));
    file.write ((uint8_t*)&time_now, sizeof (time_now));
    file.write ((uint8_t*)&histo_status, sizeof (histo_status));
    file.close ();
  }
}

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

// Get specific argument as a value with min and max
int HistoGetArgValue (const char* pstr_argument, const int value_min, const int value_max, const int value_default)
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

// Graph frame
void HistoDisplayUnits ()
{
  long    index, unit_max;
  char    arr_label[5][12];
  uint8_t arr_pos[5] = {98,76,51,26,2};  

  // check parameter
  if (histo_status.period >= HISTO_PERIOD_MAX) return;

  // set labels according to data type
  unit_max = histo_status.max_value[histo_status.period];
  for (index = 0; index < 5; index ++) GazparConvertWh2Label (false, false, index * unit_max / 4, arr_label[index], sizeof (arr_label[index]));

  // units graduation
  for (index = 0; index < 5; index ++) WSContentSend_P (PSTR ("<text class='power' x=%u%% y=%u%% >%s</text>\n"), 2, arr_pos[index],  arr_label[index]);
  WSContentSend_P (PSTR ("<text class='power' x=%d y=%d%%>Wh</text>\n"), HISTO_WIDTH * 98 / 100, 2);
}

// Graph frame
void HistoDisplayLevel ()
{
  uint32_t index;  

  // graph level lines
  for (index = 1; index < 4; index ++) WSContentSend_P (PSTR ("<line class='dash' x1=%u y1=%u x2=%u y2=%u />\n"), HISTO_LEFT, index * HISTO_HEIGHT / 4, HISTO_LEFT + HISTO_WIDE, index * HISTO_HEIGHT / 4);
}

// Graph frame
void HistoDisplayFrame ()
{
  // graph frame
  WSContentSend_P (PSTR ("<rect class='main' x=%d y=%d width=%d height=%d rx=10 />\n"), HISTO_LEFT, 0, HISTO_WIDE, HISTO_HEIGHT);
}

// Append histo button to main page
void HistoWebMainButton ()
{
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>Historique</button></form></p>\n"), PSTR_PAGE_HISTO);
}

// Display bar graph
void HistoDisplayBar ()
{
  bool     display;
  uint8_t  index, period, slot_start, slot_stop;
  uint32_t timestamp;
  TIME_T   time_dst;
  long     graph_x, graph_y, graph_height, graph_max, slot_width, bar_width, bar_height;
  char     str_value[16];
  char     str_comp[8];
  char     str_param[4];

  // check parameter
  if (histo_status.period >= HISTO_PERIOD_MAX) return;

  // graph environment
  // -----------------

  // init graph units
  timestamp = 0;
  switch (histo_status.period)
  {
    // full day display (hour bars)
    case HISTO_PERIOD_DAY:
      slot_start = 0;  slot_stop = 23;              // slots to display
      slot_width = 50; bar_width = 40;              // width of bars and separator
      str_param[0] = 0;
      break;

    // full week display (day bars)
    case HISTO_PERIOD_WEEK:
      slot_start = 1;   slot_stop = 7;              // slots to display
      slot_width = 171; bar_width = 155;            // width of bars and separator
      timestamp  = HistoGetFirstDayOfWeek (histo_status.timestamp);
      strcpy_P (str_param, PSTR (CMND_HISTO_DAYOFWEEK));
      break;

    // full month display (day bars)
    case HISTO_PERIOD_MONTH:
      slot_start = 1;                               // slots to display
      slot_stop  = HistoGetDaysInMonth (histo_status.timestamp);           
      slot_width = 38; bar_width = 34;              // width of bars and separator
      timestamp  = HistoGetFirstDayOfMonth (histo_status.timestamp);
      strcpy_P (str_param, PSTR (CMND_HISTO_DAYOFMONTH));
      break;

    // full year display (month bars)
    case HISTO_PERIOD_YEAR:
      slot_start = 1;   slot_stop = 12;             // slots to display
      slot_width = 100; bar_width = 84;             // width of bars and separator
      strcpy_P (str_param, PSTR (CMND_HISTO_MONTHOFYEAR));
      break;
  }

  // convert graph max value in Wh
  graph_max = histo_status.max_value[histo_status.period];

  // bar separators
  // --------------

  graph_x = HISTO_LEFT;
  for (index = 0; index <= slot_stop - slot_start; index++)
  {
    // zone background
    if (index % 2 == 1) WSContentSend_P (PSTR ("<rect class='dash' x=%d y=0 width=%d height=%d />\n"), graph_x, slot_width, HISTO_HEIGHT);

    // set text
    switch (histo_status.period)
    {
      case HISTO_PERIOD_YEAR:
        str_comp[0] = 0;
        strlcpy (str_value, kMonthNames + index * 3, 4);
        break;

      case HISTO_PERIOD_MONTH:
        BreakTime (timestamp, time_dst);
        strlcpy (str_comp, kHistoWeekdayNames + (time_dst.day_of_week - 1) * 3, 3);
        sprintf_P (str_value, PSTR ("%02d"), index + 1); 
        timestamp += 86400;
        break;

      case HISTO_PERIOD_WEEK:
        BreakTime (timestamp, time_dst);
        strlcpy (str_value, kMonthNames + 3 * (time_dst.month - 1), 4);
        sprintf_P (str_comp, PSTR ("%u %s"), time_dst.day_of_month, str_value);
        strlcpy (str_value, kHistoWeekdayNames + ((index + 1) % 7) * 3, 4);
        timestamp += 86400;
        break;

      case HISTO_PERIOD_DAY:
        str_comp[0] = 0;
        sprintf_P (str_value, PSTR ("%dh"), index);
        break;
    }

    // display text
    WSContentSend_P (PSTR ("<text class='time' x=%d y=20>%s</text>\n"), graph_x + slot_width / 2, str_value);
    if (str_comp[0] != 0) WSContentSend_P (PSTR ("<text class='time comp' x=%d y=40>%s</text>\n"), graph_x + slot_width / 2, str_comp);

    // shift to next slot
    graph_x += slot_width;
  }

  // bar graph
  // ---------

  // loop thru slots
  graph_x = HISTO_LEFT + (slot_width - bar_width) / 2;
  for (index = slot_start; index <= slot_stop; index ++)
  {
    // init
    graph_height = HISTO_HEIGHT;

    // calculate bar size
    graph_y = graph_height - (histo_data[index].wh * HISTO_HEIGHT / graph_max);
    if (graph_y < 0) graph_y = 0;
    bar_height = graph_height - graph_y;

    // if possible, display bar
    if (bar_height > 0) 
    {
      if (strlen (str_param) > 0) WSContentSend_P (PSTR ("<a href='%s?%s=%u'>"), PSTR_PAGE_HISTO, str_param, index);
      WSContentSend_P (PSTR ("<rect class='data' x=%d y=%d rx=4 ry=4 width=%d height=%d><title>%d wh</title></rect>\n"), graph_x, graph_y, bar_width, bar_height, histo_data[index].wh);
      if (strlen (str_param) > 0) WSContentSend_P (PSTR ("</a>"));
      WSContentSend_P (PSTR ("\n"));
  }
    graph_height = graph_y;

    // if bar is high enought, display value
    if (bar_height >= HISTO_BAR_HEIGHT_MIN)
    {
      GazparConvertWh2Label (false, false, histo_data[index].wh, str_value, sizeof (str_value));
      WSContentSend_P (PSTR ("<text class='data' x=%d y=%d>%s</text>\n"), graph_x + bar_width / 2, graph_y + bar_height / 2, str_value);
    }

    // shift to next slot
    graph_x += slot_width;
  }
}

// Graph public page
void HistoWebPage ()
{
  bool     refresh = false;
  uint8_t  index;
  uint32_t timestart, timestamp;
  char     str_text[32];

  // timestamp
  timestart = millis ();

  // beginning of form without authentification
  WSContentStart_P (PSTR (HISTO_HEAD), false);

  // get period
  histo_status.period = (uint8_t)GazparGetArgValue (CMND_HISTO_PERIOD, 0, HISTO_PERIOD_MAX - 1, histo_status.period);  

  // graph increment
  index = HistoGetArgValue (CMND_HISTO_PLUS, 0, 1, 0);
  refresh |= (index == 1);
  if (index == 1) histo_status.max_value[histo_status.period] *= 2;

  // graph decrement
  index = HistoGetArgValue (CMND_HISTO_MINUS, 0, 1, 0);
  refresh |= (index == 1);
  if (index == 1) histo_status.max_value[histo_status.period] = histo_status.max_value[histo_status.period] / 2;
  if (histo_status.max_value[histo_status.period] < 10) histo_status.max_value[histo_status.period] = 10;
    
  // handle previous page
  index = HistoGetArgValue (CMND_HISTO_PREV, 0, 5, 0);
  refresh |= (index > 0);
  if (index > 0) histo_status.timestamp = HistoGetPreviousTimestamp (histo_status.period, histo_status.timestamp, index);

  // handle next page
  index = HistoGetArgValue (CMND_HISTO_NEXT, 0, 5, 0);
  refresh |= (index > 0);
  if (index > 0) histo_status.timestamp = HistoGetNextTimestamp (histo_status.period, histo_status.timestamp, index);

  // handle day of week
  index = HistoGetArgValue (CMND_HISTO_DAYOFWEEK, 0, 7, 0);
  refresh |= (index > 0);
  if (index > 0)
  {
    histo_status.timestamp = HistoGetFirstDayOfWeek (histo_status.timestamp);
    if (index > 1) histo_status.timestamp = HistoGetNextTimestamp (HISTO_PERIOD_DAY, histo_status.timestamp, index - 1);
    histo_status.period = HISTO_PERIOD_DAY;
  }
  
  // handle day of month
  index = HistoGetArgValue (CMND_HISTO_DAYOFMONTH, 0, 31, 0);
  refresh |= (index > 0);
  if (index > 0)
  {
    histo_status.timestamp = HistoGetFirstDayOfMonth (histo_status.timestamp);
    if (index > 1) histo_status.timestamp = HistoGetNextTimestamp (HISTO_PERIOD_DAY, histo_status.timestamp, index - 1);
    histo_status.period = HISTO_PERIOD_DAY;
  }

  // handle month of year
  index = HistoGetArgValue (CMND_HISTO_MONTHOFYEAR, 0, 12, 0);
  refresh |= (index > 0);
  if (index > 0)
  {
    histo_status.timestamp = HistoGetFirstDayOfYear (histo_status.timestamp);
    if (index > 1) histo_status.timestamp = HistoGetNextTimestamp (HISTO_PERIOD_MONTH, histo_status.timestamp, index - 1);
    histo_status.period = HISTO_PERIOD_MONTH;
  }
  
  if (refresh)
  {
    WSContentSend_P (PSTR ("window.location.href=window.location.href.split('?')[0];\n"));
    WSContentSend_P (PSTR ("</script>\n"));
    WSContentSend_P (PSTR ("<style>body {background-color:%s;}</style>\n"), PSTR (COLOR_BACKGROUND));
    WSContentSend_P (PSTR ("</head>\n"));
    WSContentSend_P (PSTR ("<body>\n"));
  }

  else
  {
    // javascript : screen swipe for previous and next period
    WSContentSend_P (PSTR ("\n\nlet startX=0;let stopX=0;let startY=0;let stopY=0;\n"));
    WSContentSend_P (PSTR ("window.addEventListener('touchstart',function(evt){startX=evt.changedTouches[0].pageX;startY=evt.changedTouches[0].pageY;},false);\n"));
    WSContentSend_P (PSTR ("window.addEventListener('touchend',function(evt){stopX=evt.changedTouches[0].pageX;stopY=evt.changedTouches[0].pageY;handleGesture();},false);\n"));
    WSContentSend_P (PSTR ("function handleGesture(){\n"));
    WSContentSend_P (PSTR (" let deltaX=stopX-startX;let deltaY=Math.abs(stopY-startY);\n"));
    WSContentSend_P (PSTR (" if(deltaY<10 && deltaX<-100){window.location='%s?next=1';}\n"), PSTR_PAGE_HISTO);
    WSContentSend_P (PSTR (" else if(deltaY<10 && deltaX>100){window.location='%s?prev=1';}\n"), PSTR_PAGE_HISTO);
    WSContentSend_P (PSTR ("}\n"));

    // end of script section
    WSContentSend_P (PSTR ("</script>\n"));

    // set page as scalable
    WSContentSend_P (PSTR ("<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=yes'/>\n"));

    // set cache policy, no cache for 12h
    WSContentSend_P (PSTR ("<meta http-equiv='Cache-control' content='public,max-age=720000'/>\n"));

    // page style
    WSContentSend_P (PSTR ("<style>\n"));

    WSContentSend_P (PSTR ("body {color:white;background-color:%s;font-size:2.2vmin;font-family:Arial, Helvetica, sans-serif;}\n"), PSTR (COLOR_BACKGROUND));
    WSContentSend_P (PSTR ("div {margin:0.3vh auto;padding:2px 0px;text-align:center;vertical-align:top;}\n"));

    WSContentSend_P (PSTR ("a {text-decoration:none;}\n"));
    WSContentSend_P (PSTR ("div a {color:white;}\n"));

    WSContentSend_P (PSTR ("div.head {font-size:24px;font-weight:bold;}\n"));

    WSContentSend_P (PSTR ("div.range {position:absolute;left:2vw;width:2.5vw;margin:auto;border:1px #666 solid;border-radius:4px;}\n"));
    WSContentSend_P (PSTR ("div.range:hover {background:#aaa;}\n"));

    WSContentSend_P (PSTR ("div.lvl {display:inline-block;padding:1px;margin:0px;color:#fff;}\n"));
    WSContentSend_P (PSTR ("div.lvl div {background:#666;}\n"));
    WSContentSend_P (PSTR ("div.lvl a div {background:none;}\n"));
    WSContentSend_P (PSTR ("div.l1 {border:1px #666 solid;border-radius:6px;}\n"));
    WSContentSend_P (PSTR ("div.l1 div {padding:2px 10px;}\n"));
    WSContentSend_P (PSTR ("div.l2 div {padding:2px 10px;border:1px #666 solid;}\n"));

    WSContentSend_P (PSTR ("div.item {display:inline-block;margin:1px 0px;border-radius:4px;}\n"));
    WSContentSend_P (PSTR ("a div.item:hover {background:#aaa;}\n"));

    WSContentSend_P (PSTR ("button.menu {margin:1vh;padding:5px 20px;font-size:1.2rem;background:%s;color:%s;cursor:pointer;border:none;border-radius:8px;}\n"), PSTR (COLOR_BUTTON), PSTR (COLOR_BUTTON_TEXT));

    WSContentSend_P (PSTR ("div.graph {width:100%%;margin:auto;margin-top:15px;}\n"));
    WSContentSend_P (PSTR ("svg.graph {width:100%%;height:60vh;}\n"));

    WSContentSend_P (PSTR ("</style>\n\n"));

    // page body
    WSContentSend_P (PSTR ("</head>\n"));
    WSContentSend_P (PSTR ("<body>\n"));

    WSContentSend_P (PSTR ("<div><div class='head'>Historique</div></div>\n"));

    // ---------------------------
    //    Level 1 - Period types
    // ---------------------------

    WSContentSend_P (PSTR ("<div>\n"));
    WSContentSend_P (PSTR ("<a href='%s?%s=1'><div class='range'>+</div></a>\n"), PSTR_PAGE_HISTO, PSTR (CMND_HISTO_PLUS));
    WSContentSend_P (PSTR ("<div class='lvl l1'>\n"));

    for (index = 0; index < HISTO_PERIOD_MAX; index ++)
    {
      GetTextIndexed (str_text, sizeof (str_text), index, kHistoPeriod);
      if (histo_status.period != index) WSContentSend_P (PSTR ("<a href='%s?period=%u'>"), PSTR_PAGE_HISTO, index);
      WSContentSend_P (PSTR ("<div class='item'>%s</div>"), str_text);
      if (histo_status.period != index) WSContentSend_P (PSTR ("</a>"));
      WSContentSend_P (PSTR ("\n"));
    }

    WSContentSend_P (PSTR ("</div></div>\n"));        // lvl l1

    // ----------------------
    //    Level 2 - Periods
    // ----------------------

    WSContentSend_P (PSTR ("<div>\n"));
    WSContentSend_P (PSTR ("<a href='%s?%s=1'><div class='range'>-</div></a>\n"), PSTR_PAGE_HISTO, PSTR (CMND_HISTO_MINUS));
    WSContentSend_P (PSTR ("<div class='lvl l2'>\n"));

    // previous periods
    timestamp = HistoGetPreviousTimestamp (histo_status.period, histo_status.timestamp, 5);
    HistoGetPeriodLabel (histo_status.period, timestamp, str_text, sizeof (str_text));
    WSContentSend_P (PSTR ("<a href='%s?prev=%u'><div class='item' title='%s'>&lt; &lt;</div></a>\n"), PSTR_PAGE_HISTO, 5, str_text);

    timestamp = HistoGetPreviousTimestamp (histo_status.period, histo_status.timestamp, 1);
    HistoGetPeriodLabel (histo_status.period, timestamp, str_text, sizeof (str_text));
    WSContentSend_P (PSTR ("<a href='%s?prev=%u'><div class='item' title='%s'>&lt;</div></a>\n"), PSTR_PAGE_HISTO, 1, str_text);

    // current period
    HistoGetPeriodLabel (histo_status.period, histo_status.timestamp, str_text, sizeof (str_text));
    WSContentSend_P (PSTR ("<div class='item'>%s</div>\n"), str_text);

    // next periods
    timestamp = HistoGetNextTimestamp (histo_status.period, histo_status.timestamp, 1);
    HistoGetPeriodLabel (histo_status.period, timestamp, str_text, sizeof (str_text));
    WSContentSend_P (PSTR ("<a href='%s?next=%u'><div class='item' title='%s'>&gt;</div></a>\n"), PSTR_PAGE_HISTO, 1, str_text);

    timestamp = HistoGetNextTimestamp (histo_status.period, histo_status.timestamp, 5);
    HistoGetPeriodLabel (histo_status.period, timestamp, str_text, sizeof (str_text));
    WSContentSend_P (PSTR ("<a href='%s?next=%u'><div class='item' title='%s'>&gt; &gt;</div></a>\n"), PSTR_PAGE_HISTO, 5, str_text);

    WSContentSend_P (PSTR ("</div></div>\n"));        // lvl l2

    // -------------
    //      SVG
    // -------------

    // svg : start
    WSContentSend_P (PSTR ("<div class='graph'>\n"));
    WSContentSend_P (PSTR ("<svg class='graph' viewBox='0 0 %d %d' preserveAspectRatio='none'>\n"), HISTO_WIDTH, HISTO_HEIGHT);

    // svg : style
    WSContentSend_P (PSTR ("<style type='text/css'>\n"));

    // bar graph : general style
    WSContentSend_P (PSTR ("rect.main {fill:none;stroke:#888;stroke-width:1;}\n"));
    WSContentSend_P (PSTR ("rect.dash {fill:#333;fill-opacity:0.5;stroke-width:0;}\n"));
    WSContentSend_P (PSTR ("line.dash {stroke:grey;stroke-width:1;stroke-dasharray:2 8;}\n"));
    WSContentSend_P (PSTR ("text {font-size:1.1rem;fill:white;text-anchor:middle;dominant-baseline:middle;}\n"));
    WSContentSend_P (PSTR ("text.time {font-size:1rem;font-style:italic;fill-opacity:0.6;}\n"));
    WSContentSend_P (PSTR ("text.comp {font-size:0.8rem;}\n"));
    

    // bar graph : data style
    WSContentSend_P (PSTR ("rect.data {stroke:%s;fill:%s;fill-opacity:0.9;}\n"), PSTR_HISTO_COLOR_DATA, PSTR_HISTO_COLOR_DATA);
    WSContentSend_P (PSTR ("text.data {fill:%s;}\n"), PSTR_HISTO_COLOR_TEXT);

    WSContentSend_P (PSTR ("</style>\n"));

    // load data from file
    HistoFileLoadData ();

    // svg : units
    HistoDisplayUnits ();

    // svg : curves
    HistoDisplayBar ();

    // svg : main frame
    HistoDisplayFrame ();

    // svg : end
    WSContentSend_P (PSTR ("</svg>\n"));
    WSContentSend_P (PSTR ("</div>\n"));

    // ----------------
    //    End of page
    // ----------------

    // main menu button
    WSContentSend_P (PSTR ("<div><form action='/' method='get'><button class='menu'>%s</button></form></div>\n"), D_MAIN_MENU);
  }

  // end of page
  WSContentStop ();
  
  // log page serving time
  if (!refresh) AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: Histo drawn in %ums"), millis () - timestart);
}

#endif    // USE_WEBSERVER

/***************************************\
 *              Interface
\***************************************/

// Historisation module
bool Xsns110 (uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function) 
  {
    case FUNC_INIT:
      HistoInit ();
      break;

    case FUNC_SAVE_BEFORE_RESTART:
      HistoSaveBeforeRestart ();
      break;

    case FUNC_EVERY_SECOND:
      HistoEverySecond ();
      break;

#ifdef USE_WEBSERVER

    case FUNC_WEB_ADD_MAIN_BUTTON:
      HistoWebMainButton ();
      break;

    case FUNC_WEB_ADD_HANDLER:
      Webserver->on (FPSTR (PSTR_PAGE_HISTO), HistoWebPage);
      break;

#endif  // USE_WEBSERVER
  }

  return result;
}

#endif    // USE_UFILESYS
#endif    // USE_HISTO
#endif    // USE_GAZPAR
