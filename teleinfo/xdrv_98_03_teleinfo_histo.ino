/*
  xdrv_98_03_teleinfo_histo.ino - France Teleinfo energy sensor history (conso & prod)

  Copyright (C) 2019-2025  Nicolas Bernaerts

  Version history :
    29/02/2024 v1.0 - Split graph in 3 files
    28/06/2024 v1.1 - Remove all String for ESP8266 stability
    20/03/2025 v2.0 - Complete rewrite
    30/04/2025 v2.1 - Optimize memory usage for ESP8266 
    10/07/2025 v3.0 - Refactoring based on Tasmota 15

  RAM : 1944 bytes

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
#ifdef USE_TELEINFO_HISTO
#ifdef USE_UFILESYS

/*************************************************\
 *               Variables
\*************************************************/

#define HISTO_VERSION                 2           // saved data version

#define HISTO_LINE_SIZE               168         // size of historisation line
#define HISTO_GRAPH_MAX               32          // maximum number of values in graph

#define HISTO_FS_MINSIZE              50          // minimum left size on FS before cleaning old files (kB)

// graph default and boundaries
#define HISTO_DEF_KWH_DAY             2           // default hourly power in day graph
#define HISTO_DEF_KWH_WEEK            10          // default daily power in week graph
#define HISTO_DEF_KWH_MONTH           10          // default daily power in month graph
#define HISTO_DEF_KWH_YEAR            100         // default monthly power in year graph

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
static const char kTeleinfoHistoWeekdayNames[] = D_DAY3LIST;

// web URL
const char PSTR_PAGE_HISTO[] PROGMEM = "/histo";

enum HistoFile { HISTO_FILE_BEFORE, HISTO_FILE_FOUND, HISTO_FILE_LAST, HISTO_FILE_BEYOND, HISTO_FILE_MAX };

// files on FS
const char PSTR_HISTO_DATA[]          PROGMEM = "/teleinfo-histo.dat";
const char PSTR_HISTO_FILE_ORIGINAL[] PROGMEM = "/teleinfo-%04u.csv";
const char PSTR_HISTO_FILE_CONTRACT[] PROGMEM = "/teleinfo-%s-%04u.csv";
const char PSTR_HISTO_FILE_MASK[]     PROGMEM = "teleinfo-*.csv";

/****************************************\
 *                 Data
\****************************************/

static struct {                     // 24 bytes
  uint8_t  hour      = UINT8_MAX;                 // last recorded hour
  uint8_t  period    = CALENDAR_PERIOD_DAY;          // current period of graph
  uint16_t display   = UINT16_MAX;                // mask of data to display
  uint32_t timestamp = 0;                         // current timestamp for graph
  long     max_value[CALENDAR_PERIOD_MAX];           // max value according to period
} histo_status;

struct histo_delta {                // 68 bytes
  long forecast;                                  // forecast solar production (in wh)
  long solar;                                     // solar production (in wh)
  long prod;                                      // production (in wh)
  long conso[TIC_PERIOD_MAX];                     // conso periods (in wh)
};

histo_delta histo_data[HISTO_GRAPH_MAX];          // display data for graph, 2048 bytes

struct histo_value {                // 136 bytes
  long long forecast;                             // forecast solar production (in wh)
  long long solar;                                // solar production (in wh)
  long long prod;                                 // production (in wh)
  long long conso[TIC_PERIOD_MAX];                // conso periods (in wh)
};

/*********************************************\
 *               Functions
\*********************************************/

void TeleinfoHistoInitStatus ()
{
  histo_status.hour      = UINT8_MAX;
  histo_status.period    = CALENDAR_PERIOD_DAY;
  histo_status.display   = UINT16_MAX;
  histo_status.timestamp = 0;
  histo_status.max_value[CALENDAR_PERIOD_DAY]   = HISTO_DEF_KWH_DAY;
  histo_status.max_value[CALENDAR_PERIOD_WEEK]  = HISTO_DEF_KWH_WEEK;
  histo_status.max_value[CALENDAR_PERIOD_MONTH] = HISTO_DEF_KWH_MONTH;
  histo_status.max_value[CALENDAR_PERIOD_YEAR]  = HISTO_DEF_KWH_YEAR;
}

void TeleinfoHistoInitData ()
{
  uint8_t slot, index;

  // init data
  for (slot = 0; slot < HISTO_GRAPH_MAX; slot ++)
  {
    histo_data[slot].forecast = 0;
    histo_data[slot].solar    = 0;
    histo_data[slot].prod     = 0;
    for (index = 0; index < TIC_PERIOD_MAX; index ++) histo_data[slot].conso[index]= 0;
  }
}

/*********************************************\
 *                 Files
\*********************************************/

bool TeleinfoHistoFileIsCandidate (const char* pstr_filename, const time_t filetime, time_t& prevtime)
{
  bool  candidate = false;
  char *pstr_digit;
  char  str_mask[32];

  // check parameters
  if (pstr_filename == nullptr) return false;

  // setup file mask
  strcpy_P (str_mask, PSTR_HISTO_FILE_MASK);
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
void TeleinfoHistoFileCleanup ()
{
  uint32_t free;
  time_t   time_target = 0;
  String   str_root, str_target;

  // get space left on FS (in kB)
  free = UfsFree ();
  AddLog (LOG_LEVEL_INFO, PSTR ("TIC: free %u kB for %u kB minimum"), free, HISTO_FS_MINSIZE);

  // if not enough space, loop to find oldest file
  str_root = F ("/");
  if (free < HISTO_FS_MINSIZE)
  {
#ifdef ESP32
    File directory = ffsp->open (str_root.c_str ());
    File file      = directory.openNextFile ();
    while (file) 
    {
      if (TeleinfoHistoFileIsCandidate (file.name (), file.getLastWrite (), time_target)) str_target = str_root + file.name ();
      file = directory.openNextFile ();
    }

#else     // ESP8266
    Dir directory = ffsp->openDir (str_root.c_str ());
    while (directory.next ())
      if (TeleinfoHistoFileIsCandidate (directory.fileName ().c_str (), directory.fileTime (), time_target)) str_target = str_root + directory.fileName ();

#endif    // ESP32 or ESP8266
  }

  // if file was found
  if (str_target != "")
  {
    // remove candidate file
    ffsp->remove (str_target.c_str ());
    
    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("TIC: removed %s"), str_target.c_str ());
  }
}

// get current filename and create it if needed
void TeleinfoHistoGetFilename (const uint16_t number, char *pstr_filename, const size_t size_filename)
{
  uint8_t  index;
  uint16_t year;
  char     str_value[12];
  char     str_name[HISTO_LINE_SIZE];
  char    *pstr_name;
  File     file;

  // check parameters
  if (pstr_filename == nullptr) return;
  if (size_filename < UFS_FILENAME_SIZE) return;

  // set year
  if (number < 1970) year = 1970 + number;
    else year = number;

  // set filename according to contract
  strcpy_P (pstr_filename, PSTR("/"));
  sprintf_P (str_name, PSTR_HISTO_FILE_CONTRACT, teleinfo_contract_db.str_code, year);

  // cleanup name
  pstr_name = str_name;
  while (*pstr_name != 0)
  {
    switch (*pstr_name)
    {
      case ' ': break;
      case '/': break;
      default:  strncat (pstr_filename, pstr_name, 1);
    }
    pstr_name++;
  }

  // if file doesn't exist
  if (!ffsp->exists (pstr_filename))
  {
    // check previous generic name
    sprintf_P (str_name, PSTR_HISTO_FILE_ORIGINAL, year);
    if (ffsp->exists (str_name))
    {
      ffsp->rename (str_name, pstr_filename);
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: File %s renamed to %s"), str_name, pstr_filename);
    } 
  }

  // if file exists, ignore
  if (ffsp->exists (pstr_filename)) return;

  // open file in create mode
  file = ffsp->open (pstr_filename, "w");

  // generate header :  dddwwdmmddhh;prod;conso1;conso2;...;solar;forecast
  strcpy_P (str_name, PSTR ("dddwwdmmddhh;prod"));
  for (index = 0; index < teleinfo_contract_db.period_qty; index++)
  {
    sprintf_P (str_value, PSTR (";conso%u"), index + 1);
    strcat (str_name, str_value);
  }
  strcat_P (str_name, PSTR (";solar;forecast\n"));
  file.print (str_name);

  // first line : beginning
  strcpy_P (str_name, PSTR ("000000000000;"));

  // first line : production counter
  lltoa (teleinfo_prod_wh.total, str_value, 10);
  strlcat (str_name, str_value, sizeof (str_name));

  // first line : conso indexes
  for (index = 0; index < teleinfo_contract_db.period_qty; index++)
  {
    lltoa (teleinfo_conso_wh.index[index], str_value, 10);
    strcat_P (str_name, PSTR (";"));
    strlcat (str_name, str_value, sizeof (str_name));
  }

  // first line : solar production counter
  lltoa (teleinfo_solar.total_wh, str_value, 10);
  strcat_P (str_name, PSTR (";"));
  strlcat (str_name, str_value, sizeof (str_name));

  // first line : solar production forecast counter
  lltoa (teleinfo_forecast.total_wh, str_value, 10);
  strcat_P (str_name, PSTR (";"));
  strlcat (str_name, str_value, sizeof (str_name));

  // first line : write
  strcat_P (str_name, PSTR ("\n"));
  file.print (str_name);

  // close file
  file.close ();
}

// Save historisation data : dddwwdmmddhh;prod;conso1;conso2;...;solar;forecast (max 630k per year)
void TeleinfoHistoFileSaveData ()
{
  uint8_t  index;
  uint32_t time_now;
  TIME_T   time_dst;
  char     str_value[12];
  char     str_text[HISTO_LINE_SIZE];
  File     file;

  // if date not defined, cancel
  if (!RtcTime.valid) return;

  // calculate timestamp (30sec before to get previous slot)
  time_now = LocalTime () - 30;
  BreakTime (time_now, time_dst);

  // open in append mode
  TeleinfoHistoGetFilename (time_dst.year, str_text, sizeof (str_text));
  file = ffsp->open (str_text, "a");

  // line : beginning
  CalendarFileGetHeader (time_now, str_text, sizeof (str_text));

  // line : production counter
  lltoa (teleinfo_prod_wh.total, str_value, 10);
  strlcat (str_text, str_value, sizeof (str_text));

  // line : conso indexes
  for (index = 0; index < teleinfo_contract_db.period_qty; index++)
  {
    lltoa (teleinfo_conso_wh.index[index], str_value, 10);
    strcat_P (str_text, PSTR (";"));
    strlcat (str_text, str_value, sizeof (str_text));
  }

  // line : solar production counter
  lltoa (teleinfo_solar.total_wh, str_value, 10);
  strcat_P (str_text, PSTR (";"));
  strlcat (str_text, str_value, sizeof (str_text));

  // line : solar production forecast counter
  lltoa (teleinfo_forecast.total_wh, str_value, 10);
  strcat_P (str_text, PSTR (";"));
  strlcat (str_text, str_value, sizeof (str_text));

  // write line and close file
  strcat_P (str_text, PSTR ("\n"));
  file.print (str_text);
  file.close ();
}

// extract data from a line
int TeleinfoHistoExtractLine (char* pstr_line, struct histo_value &value)
{
  int   column = 0;
  char *pstr_token;

  // loop thru delimiter
  pstr_token = strtok (pstr_line, ";");
  while (pstr_token)
  {
    // extract data according to column
    column++;
    if (column == 2) value.prod = atoll (pstr_token);
    else if (column == 3 + teleinfo_contract_db.period_qty) value.solar = atoll (pstr_token);
    else if (column == 4 + teleinfo_contract_db.period_qty) value.forecast = atoll (pstr_token);
    else if (column > 2) value.conso[column - 3] = atoll (pstr_token);

    // look for next token
    pstr_token = strtok (nullptr, ";");
  }

 return column;
}

// load data from file
bool TeleinfoHistoFileLoadPeriod (const uint8_t period, const uint32_t timestamp)
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
  if (period >= CALENDAR_PERIOD_MAX) return false;
  if (timestamp == 0) return false;

  // check data file presence
  BreakTime (timestamp, time_dst);
  TeleinfoHistoGetFilename (time_dst.year, str_current, sizeof (str_current));
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
    case CALENDAR_PERIOD_DAY:
      tgt_start = 0;  tgt_length = 3;
      idx_start = 10; idx_length = 2;
      index_min = 0;
      sprintf_P (str_target, PSTR ("%03u"), time_dst.day_of_year + 1);
      break;

    case CALENDAR_PERIOD_WEEK:
      tgt_start = 3; tgt_length = 2;
      idx_start = 5; idx_length = 1;
      index_min = 1;
      count = CalendarGetWeekNumber (timestamp);
      sprintf_P (str_target, PSTR ("%02u"), count);
      break;

    case CALENDAR_PERIOD_MONTH:
      tgt_start = 6; tgt_length = 2;
      idx_start = 8; idx_length = 2;
      index_min = 1;
      sprintf_P (str_target, PSTR ("%02u"), time_dst.month);
      break;

    case CALENDAR_PERIOD_YEAR:
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
    if (period == CALENDAR_PERIOD_YEAR) (line_current > 1) ? status = HISTO_FILE_FOUND : status = HISTO_FILE_BEFORE;
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
        TeleinfoHistoExtractLine (str_previous, total_begin);
      }

      // if beyond target, set current index to max value
      if (status == HISTO_FILE_BEYOND) index_current = INT_MAX;

      // if index has changed from previous line, calculate delta for current valid index
      if (index_current > index_previous)
      {
        if ((index_previous < HISTO_GRAPH_MAX) && (index_previous > index_begin))
        {
          // extract totals from previous line
          TeleinfoHistoExtractLine (str_previous, total_stop);

          // save delta from last valid index till previous index
          histo_data[index_previous].prod     = (long)(total_stop.prod     - total_begin.prod);
          for (count = 0; count < TIC_PERIOD_MAX; count++) histo_data[index_previous].conso[count] = (long)(total_stop.conso[count] - total_begin.conso[count]);
          histo_data[index_previous].solar    = (long)(total_stop.solar    - total_begin.solar);
          histo_data[index_previous].forecast = (long)(total_stop.forecast - total_begin.forecast);
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
          TeleinfoHistoExtractLine (str_current, total_stop);
  
          // save delta from last valid index till previous index
          histo_data[index_current].prod     = (long)(total_stop.prod     - total_begin.prod);
          for (count = 0; count < TIC_PERIOD_MAX; count++) histo_data[index_current].conso[count] = (long)(total_stop.conso[count] - total_begin.conso[count]);        
          histo_data[index_current].solar    = (long)(total_stop.solar    - total_begin.solar);
          histo_data[index_current].forecast = (long)(total_stop.forecast - total_begin.forecast);
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
  AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: Loaded %u, period %u, target %s, %s [%ums]"), 1970 + time_dst.year, histo_status.period, str_target, str_current, millis () - time_now);

  return (line_first > 0);
}

// load data from file
void TeleinfoHistoFileLoadData ()
{
  uint8_t week;

  // check parameters
  if (histo_status.period >= CALENDAR_PERIOD_MAX) return;
  if (histo_status.timestamp == 0) return;
  
  // init data
  TeleinfoHistoInitData ();

  // load data for current period
  TeleinfoHistoFileLoadPeriod (histo_status.period, histo_status.timestamp);

  // if dealing with weekly period
  if (histo_status.period == CALENDAR_PERIOD_WEEK)
  {
    // get current week number
    week = CalendarIsWeekComplete (histo_status.timestamp);

    // if needed, append first week of next year (6 days shift)
    if (week == 1) TeleinfoHistoFileLoadPeriod (histo_status.period, histo_status.timestamp + 518400);

    // else if needed, append last week of previous year (6 days shift)
    else if (week == 53) TeleinfoHistoFileLoadPeriod (histo_status.period, histo_status.timestamp - 518400);
  }
}

/*********************************************\
 *                   Callback
\*********************************************/

// load config file
void TeleinfoHistoLoadConfig ()
{
  uint8_t  version;
  uint32_t time_save;
  char     str_filename[UFS_FILENAME_SIZE];
  File     file;

  strcpy_P (str_filename, PSTR_HISTO_DATA);
  if (ffsp->exists (str_filename))
  {
    // open file in read mode
    file = ffsp->open (str_filename, "r");

    // check version
    file.read ((uint8_t*)&version, sizeof (version));
    if (version == HISTO_VERSION)
    {
      // read data
      file.read ((uint8_t*)&time_save, sizeof (time_save));
      file.read ((uint8_t*)&histo_status, sizeof (histo_status));
    }

    file.close ();
  }
}

// save config file
void TeleinfoHistoSaveConfig ()
{
  uint8_t  version;
  uint32_t time_now;
  char     str_filename[UFS_FILENAME_SIZE];
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

// init data
void TeleinfoHistoInit ()
{
  // init data
  TeleinfoHistoInitStatus ();
  TeleinfoHistoInitData ();

  // load config file
  TeleinfoHistoLoadConfig ();
}

// called every second
void TeleinfoHistoEverySecond ()
{
  TIME_T time_dst;

  // ignore if time is not valid
  if (!RtcTime.valid) return;
  if (teleinfo_meter.nb_message <= TIC_MESSAGE_MIN) return;

  // extract current time
  BreakTime (LocalTime (), time_dst);

  // init historisation to today's day
  if (histo_status.timestamp == 0) histo_status.timestamp = LocalTime ();

  // detect hour change to save data
  if (histo_status.hour == UINT8_MAX) histo_status.hour = time_dst.hour;
  if (histo_status.hour != time_dst.hour)
  {
    // free space on FS if needed
    TeleinfoHistoFileCleanup ();

    // save previous hour data
    TeleinfoHistoFileSaveData ();
  }
  histo_status.hour = time_dst.hour;
}

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

// Graph frame
void TeleinfoHistoDisplayUnits ()
{
  long    index, unit_max;
  char    arr_label[5][12];
  uint8_t arr_pos[5] = {98,76,51,26,2};  

  // check parameter
  if (histo_status.period >= CALENDAR_PERIOD_MAX) return;

  // set labels according to data type
  unit_max = histo_status.max_value[histo_status.period] * 1000;
  for (index = 0; index < 5; index ++) TeleinfoGetFormattedValue (TELEINFO_UNIT_MAX, index * unit_max / 4, arr_label[index], sizeof (arr_label[index]), true);

  // units graduation
  for (index = 0; index < 5; index ++) WSContentSend_P (PSTR ("<text class='power' x=%u%% y=%u%% >%s</text>\n"), 2, arr_pos[index],  arr_label[index]);
  WSContentSend_P (PSTR ("<text class='power' x=%d y=%d%%>Wh</text>\n"), HISTO_WIDTH * 98 / 100, 2);
}

// Append histo button to main page
void TeleinfoHistoWebMainButton ()
{
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s %s</button></form></p>\n"), PSTR_PAGE_HISTO, PSTR (TIC_TELEINFO), PSTR (TIC_HISTO));
}

// Display bar graph
void TeleinfoHistoDisplayBar ()
{
  bool     display;
  uint8_t  index, period, slot_start, slot_stop;
  uint16_t mask;
  uint32_t timestamp;
  TIME_T   time_dst;
  long     graph_x, graph_y, graph_height, graph_max, slot_width, bar_width, bar_height, prev_y;
  char     str_value[16];
  char     str_comp[8];
  char     str_param[4];

  // check parameter
  if (histo_status.period >= CALENDAR_PERIOD_MAX) return;

  // graph environment
  // -----------------

  // init graph units
  timestamp = 0;
  switch (histo_status.period)
  {
    // full day display (hour bars)
    case CALENDAR_PERIOD_DAY:
      slot_start = 0;  slot_stop = 23;              // slots to display
      slot_width = 50; bar_width = 40;              // width of bars and separator
      str_param[0] = 0;
      break;

    // full week display (day bars)
    case CALENDAR_PERIOD_WEEK:
      slot_start = 1;   slot_stop = 7;              // slots to display
      slot_width = 171; bar_width = 155;            // width of bars and separator
      timestamp  = CalendarGetFirstDayOfWeek (histo_status.timestamp);
      strcpy_P (str_param, PSTR (CMND_HISTO_DAYOFWEEK));
      break;

    // full month display (day bars)
    case CALENDAR_PERIOD_MONTH:
      slot_start = 1;                               // slots to display
      slot_stop  = CalendarGetDaysInMonth (histo_status.timestamp);           
      slot_width = 38; bar_width = 34;              // width of bars and separator
      timestamp  = CalendarGetFirstDayOfMonth (histo_status.timestamp);
      strcpy_P (str_param, PSTR (CMND_HISTO_DAYOFMONTH));
      break;

    // full year display (month bars)
    case CALENDAR_PERIOD_YEAR:
      slot_start = 1;   slot_stop = 12;             // slots to display
      slot_width = 100; bar_width = 84;             // width of bars and separator
      strcpy_P (str_param, PSTR (CMND_HISTO_MONTHOFYEAR));
      break;
  }

  // convert graph max value in Wh
  graph_max = histo_status.max_value[histo_status.period];
  graph_max = max (1000L, graph_max * 1000);

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
      case CALENDAR_PERIOD_YEAR:
        str_comp[0] = 0;
        strlcpy (str_value, kMonthNames + index * 3, 4);
        break;

      case CALENDAR_PERIOD_MONTH:
        BreakTime (timestamp, time_dst);
        strlcpy (str_comp, kTeleinfoHistoWeekdayNames + (time_dst.day_of_week - 1) * 3, 3);
        sprintf_P (str_value, PSTR ("%02d"), index + 1); 
        timestamp += 86400;
        break;

      case CALENDAR_PERIOD_WEEK:
        BreakTime (timestamp, time_dst);
        strlcpy (str_value, kMonthNames + 3 * (time_dst.month - 1), 4);
        sprintf_P (str_comp, PSTR ("%u %s"), time_dst.day_of_month, str_value);
        strlcpy (str_value, kTeleinfoHistoWeekdayNames + ((index + 1) % 7) * 3, 4);
        timestamp += 86400;
        break;

      case CALENDAR_PERIOD_DAY:
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

  // stacked bar graph
  // -----------------

  // loop thru slots
  graph_x = HISTO_LEFT + (slot_width - bar_width) / 2;
  for (index = slot_start; index <= slot_stop; index ++)
  {
    // init
    graph_height = HISTO_HEIGHT;

    // solar production bar
    // --------------------
    mask = 0x0001 << 0;
    if (teleinfo_solar.enabled && (histo_status.display & mask) && (histo_data[index].solar > 0))
    {
      // calculate bar size
      graph_y = graph_height - (histo_data[index].solar * HISTO_HEIGHT / graph_max);
      if (graph_y < 0) graph_y = 0;
      bar_height = graph_height - graph_y;

      // if possible, display bar
      if (bar_height > 0)
      {
        if (strlen (str_param) > 0) WSContentSend_P (PSTR ("<a href='%s?%s=%u'>"), PSTR_PAGE_HISTO, str_param, index);
        WSContentSend_P (PSTR ("<rect class='p%u' x=%d y=%d rx=4 ry=4 width=%d height=%d><title>%d</title></rect>"), TIC_LEVEL_SOLAR, graph_x, graph_y, bar_width, bar_height, histo_data[index].solar);
        if (strlen (str_param) > 0) WSContentSend_P (PSTR ("</a>"));
        WSContentSend_P (PSTR ("\n"));
      }
      graph_height = graph_y;

      // if bar is high enought, display value
      if (bar_height >= HISTO_BAR_HEIGHT_MIN)
      {
        TeleinfoGetFormattedValue (TELEINFO_UNIT_MAX, histo_data[index].solar, str_value, sizeof (str_value), true);
        WSContentSend_P (PSTR ("<text class='p%u' x=%d y=%d>%s</text>\n"), TIC_LEVEL_SOLAR, graph_x + bar_width / 2, graph_y + bar_height / 2, str_value);
      } 
    }

    // production bar
    // --------------
    mask = 0x0001 << 1;
    if (teleinfo_prod.enabled && (histo_status.display & mask) && (histo_data[index].prod > 0))
    {
      // calculate bar size
      graph_y = graph_height - (histo_data[index].prod * HISTO_HEIGHT / graph_max);
      if (graph_y < 0) graph_y = 0;
      bar_height = graph_height - graph_y;

      // if possible, display bar
      if (bar_height > 0)
      {
        if (strlen (str_param) > 0) WSContentSend_P (PSTR ("<a href='%s?%s=%u'>"), PSTR_PAGE_HISTO, str_param, index);
        WSContentSend_P (PSTR ("<rect class='p%u' x=%d y=%d rx=4 ry=4 width=%d height=%d><title>%d</title></rect>"), TIC_LEVEL_PROD, graph_x, graph_y, bar_width, bar_height, histo_data[index].prod);
        if (strlen (str_param) > 0) WSContentSend_P (PSTR ("</a>"));
        WSContentSend_P (PSTR ("\n"));
      }
      graph_height = graph_y;

      // if bar is high enought, display value
      if (bar_height >= HISTO_BAR_HEIGHT_MIN)
      {
        TeleinfoGetFormattedValue (TELEINFO_UNIT_MAX, histo_data[index].prod, str_value, sizeof (str_value), true);
        WSContentSend_P (PSTR ("<text class='p%u' x=%d y=%d>%s</text>\n"), TIC_LEVEL_PROD, graph_x + bar_width / 2, graph_y + bar_height / 2, str_value);
      } 
    }

    // conso period bars
    // -----------------
    for (period = 0; period < teleinfo_contract_db.period_qty; period ++)
    {
      mask = 0x0001 << (2 + period);
      if ((histo_status.display & mask) && (histo_data[index].conso[period] > 0))
      {
        // calculate bar size
        graph_y = graph_height - (histo_data[index].conso[period] * HISTO_HEIGHT / graph_max);
        if (graph_y < 0) graph_y = 0;
        bar_height = graph_height - graph_y;

        // if possible, display bar
        if (bar_height > 0)
        {
          if (strlen (str_param) > 0) WSContentSend_P (PSTR ("<a href='%s?%s=%u'>"), PSTR_PAGE_HISTO, str_param, index);
          WSContentSend_P (PSTR ("<rect class='c%u' x=%d y=%d rx=4 ry=4 width=%d height=%d><title>%d</title></rect>"), period, graph_x, graph_y, bar_width, bar_height, histo_data[index].conso[period]);
          if (strlen (str_param) > 0) WSContentSend_P (PSTR ("</a>"));
          WSContentSend_P (PSTR ("\n"));
        }
        graph_height = graph_y;

        // if bar is high enought, display value
        if (bar_height >= HISTO_BAR_HEIGHT_MIN)
        {
          TeleinfoGetFormattedValue (TELEINFO_UNIT_MAX, histo_data[index].conso[period], str_value, sizeof (str_value), true);
          WSContentSend_P (PSTR ("<text class='c%u' x=%d y=%d>%s</text>\n"), period, graph_x + bar_width / 2, graph_y + bar_height / 2, str_value);
        } 
      }
    }
    
    // shift to next slot
    graph_x += slot_width;
  }

  // forecast curve
  // --------------

  // display if production or solar production are selected
  mask     = 0x0001 << 0; 
  display  = (teleinfo_forecast.enabled && (histo_status.display & mask));

  // if display, 
  if (display)
  {
    // start path
    WSContentSend_P (PSTR ("<path class='p%u' d='M%d,%d"), TIC_LEVEL_FORECAST, HISTO_LEFT, HISTO_HEIGHT);

    // loop thru slots
    prev_y  = HISTO_HEIGHT;
    graph_x = HISTO_LEFT;
    for (index = slot_start; index <= slot_stop; index ++)
    {
      // calculate y position size
      graph_y = HISTO_HEIGHT - (histo_data[index].forecast * HISTO_HEIGHT / graph_max);
      if (graph_y < 0) graph_y = 0;

      // if first slot
      if (graph_x == HISTO_LEFT) WSContentSend_P (PSTR (" C%d,%d %d,%d %d,%d"), graph_x, graph_y, graph_x, graph_y, graph_x + slot_width / 2, graph_y);

      // else next slots
      else WSContentSend_P (PSTR (" C%d,%d %d,%d %d,%d"), graph_x + slot_width / 2, prev_y, graph_x - slot_width / 2, graph_y, graph_x + slot_width / 2, graph_y);

      // shift to next slot
      prev_y   = graph_y;
      graph_x += slot_width;
    }

    // end path
    WSContentSend_P (PSTR (" C%d,%d %d,%d %d,%d' />"), graph_x, prev_y, graph_x, graph_y, graph_x, HISTO_HEIGHT);
  }
}

uint16_t TeleinfoHistoButtonStatus (const uint16_t shift, char *pstr_status, size_t size_status)
{
  bool     display;
  uint16_t mask, choice;

  // check parameters
  if (pstr_status == nullptr) return histo_status.display;
  if (size_status < 10) return histo_status.display;

  // detect current display
  mask = 0x0001 << shift;
  display = (histo_status.display & mask);

  // link to invert display
  strcpy (pstr_status, "");
  if (display) choice = histo_status.display & (mask ^ 0xffff);
  else
  {
    choice = histo_status.display | mask;
    strcpy_P (pstr_status, PSTR (" disabled"));
  } 

  return choice;
} 

// Graph public page
void TeleinfoHistoWebPage ()
{
  uint8_t  index, level, hchp;
  uint16_t choice;  
  uint32_t timestart, timestamp;
  char     str_color[8];
  char     str_status[16];
  char     str_text[32];

  // timestamp
  timestart = millis ();

  // beginning of form without authentification
  WSContentStart_P (PSTR (HISTO_HEAD), false);

  // get period
  histo_status.period = (uint8_t)TeleinfoGetArgValue (CMND_HISTO_PERIOD, 0, CALENDAR_PERIOD_MAX - 1, histo_status.period);  

  // check phase display argument
  if (Webserver->hasArg (F (CMND_HISTO_DISPLAY)))
  {
    WebGetArg (PSTR (CMND_HISTO_DISPLAY), str_text, sizeof (str_text));
    histo_status.display = (uint16_t)atoi (str_text);
  }
  
  // graph increment
  index = TeleinfoGetArgValue (CMND_HISTO_PLUS, 0, 1, 0);
  if (index == 1) histo_status.max_value[histo_status.period] *= 2;

  // graph decrement
  index = TeleinfoGetArgValue (CMND_HISTO_MINUS, 0, 1, 0);
  if (index == 1) histo_status.max_value[histo_status.period] = histo_status.max_value[histo_status.period] / 2;
  if (histo_status.max_value[histo_status.period] < 1) histo_status.max_value[histo_status.period] = 1;
    
  // handle previous page
  index = TeleinfoGetArgValue (CMND_HISTO_PREV, 0, 5, 0);
  if (index > 0) histo_status.timestamp = CalendarGetPreviousTimestamp (histo_status.period, histo_status.timestamp, index);

  // handle next page
  index = TeleinfoGetArgValue (CMND_HISTO_NEXT, 0, 5, 0);
  if (index > 0) histo_status.timestamp = CalendarGetNextTimestamp (histo_status.period, histo_status.timestamp, index);

  // handle day of week
  index = TeleinfoGetArgValue (CMND_HISTO_DAYOFWEEK, 0, 7, 0);
  if (index > 0)
  {
    histo_status.timestamp = CalendarGetFirstDayOfWeek (histo_status.timestamp);
    if (index > 1) histo_status.timestamp = CalendarGetNextTimestamp (CALENDAR_PERIOD_DAY, histo_status.timestamp, index - 1);
    histo_status.period = CALENDAR_PERIOD_DAY;
  }
 
  // handle day of month
  index = TeleinfoGetArgValue (CMND_HISTO_DAYOFMONTH, 0, 31, 0);
  if (index > 0)
  {
    histo_status.timestamp = CalendarGetFirstDayOfMonth (histo_status.timestamp);
    if (index > 1) histo_status.timestamp = CalendarGetNextTimestamp (CALENDAR_PERIOD_DAY, histo_status.timestamp, index - 1);
    histo_status.period = CALENDAR_PERIOD_DAY;
  }

  // handle month of year
  index = TeleinfoGetArgValue (CMND_HISTO_MONTHOFYEAR, 0, 12, 0);
  if (index > 0)
  {
    histo_status.timestamp = CalendarGetFirstDayOfYear (histo_status.timestamp);
    if (index > 1) histo_status.timestamp = CalendarGetNextTimestamp (CALENDAR_PERIOD_MONTH, histo_status.timestamp, index - 1);
    histo_status.period = CALENDAR_PERIOD_MONTH;
  }

  // javascript : remove parameters
  WSContentSend_P (PSTR ("\n\nvar newURL = location.href.split('?')[0];\n"));
  WSContentSend_P (PSTR ("window.history.pushState('object', document.title, newURL);\n"));

  // javascript : screen swipe for previous and next period
  WSContentSend_P (PSTR ("\nlet startX=0;let stopX=0;let startY=0;let stopY=0;\n"));
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
  WSContentSend_P (PSTR ("div.l3 div {padding:4px 12px;margin:auto 2px;border:1px #444 solid;border-radius:6px;}\n"));

  // solar and production style
  for (index = TIC_LEVEL_PROD; index < TIC_LEVEL_END; index ++)
  {
    GetTextIndexed (str_color, sizeof (str_color), index, kTeleinfoLevelCalColor);
    GetTextIndexed (str_text,  sizeof (str_text),  index, kTeleinfoLevelCalText);
    WSContentSend_P (PSTR ("div.l3 div.p%u {color:%s;background:%s;}\n"), index, str_text, str_color);
  }

  // conso style
  for (index = 0; index < teleinfo_contract_db.period_qty; index ++)
  {
    level = teleinfo_contract_db.arr_period[index].level;
    if (level > 0) hchp = teleinfo_contract_db.arr_period[index].hchp;
      else hchp = TIC_HOUR_HP;
    if (hchp == 0) strcpy_P (str_status, PSTR ("opacity:0.6;"));
      else str_status[0] = 0;
    GetTextIndexed (str_color, sizeof (str_color), level, kTeleinfoLevelCalColor);
    GetTextIndexed (str_text,  sizeof (str_text),  level, kTeleinfoLevelCalText);
    WSContentSend_P (PSTR ("div.l3 div.c%u {color:%s;background:%s;%s}\n"), index, str_text, str_color, str_status);      
  }

  WSContentSend_P (PSTR ("div.l3 div.disabled {color:#666;background:#333;}\n"));

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

  for (index = 0; index < CALENDAR_PERIOD_MAX; index ++)
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
  timestamp = CalendarGetPreviousTimestamp (histo_status.period, histo_status.timestamp, 5);
  CalendarGetPeriodLabel (histo_status.period, timestamp, str_text, sizeof (str_text));
  WSContentSend_P (PSTR ("<a href='%s?prev=%u'><div class='item' title='%s'>&lt; &lt;</div></a>\n"), PSTR_PAGE_HISTO, 5, str_text);

  timestamp = CalendarGetPreviousTimestamp (histo_status.period, histo_status.timestamp, 1);
  CalendarGetPeriodLabel (histo_status.period, timestamp, str_text, sizeof (str_text));
  WSContentSend_P (PSTR ("<a href='%s?prev=%u'><div class='item' title='%s'>&lt;</div></a>\n"), PSTR_PAGE_HISTO, 1, str_text);

  // current period
  CalendarGetPeriodLabel (histo_status.period, histo_status.timestamp, str_text, sizeof (str_text));
  WSContentSend_P (PSTR ("<div class='item'>%s</div>\n"), str_text);

  // next periods
  timestamp = CalendarGetNextTimestamp (histo_status.period, histo_status.timestamp, 1);
  CalendarGetPeriodLabel (histo_status.period, timestamp, str_text, sizeof (str_text));
  WSContentSend_P (PSTR ("<a href='%s?next=%u'><div class='item' title='%s'>&gt;</div></a>\n"), PSTR_PAGE_HISTO, 1, str_text);

  timestamp = CalendarGetNextTimestamp (histo_status.period, histo_status.timestamp, 5);
  CalendarGetPeriodLabel (histo_status.period, timestamp, str_text, sizeof (str_text));
  WSContentSend_P (PSTR ("<a href='%s?next=%u'><div class='item' title='%s'>&gt; &gt;</div></a>\n"), PSTR_PAGE_HISTO, 5, str_text);

  WSContentSend_P (PSTR ("</div></div>\n"));        // lvl l2

  // -------------
  //      SVG
  // -------------

  // svg : start
  WSContentSend_P (PSTR ("<div class='graph'>\n"));
  WSContentSend_P (PSTR ("<svg class='graph' viewBox='0 0 %d %d' preserveAspectRatio='none'>\n"), HISTO_WIDTH, HISTO_HEIGHT + 1);

  // svg : style
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));

  // bar graph : general style
  WSContentSend_P (PSTR ("rect.main {fill:none;stroke:#888;}\n"));
  WSContentSend_P (PSTR ("rect.dash {fill:#333;fill-opacity:0.5;stroke-width:0;}\n"));
  WSContentSend_P (PSTR ("line.dash {stroke:grey;stroke-width:1;stroke-dasharray:2 8;}\n"));
  WSContentSend_P (PSTR ("text {font-size:1.1rem;fill:white;text-anchor:middle;dominant-baseline:middle;}\n"));
  WSContentSend_P (PSTR ("text.time {font-size:1rem;font-style:italic;fill-opacity:0.6;}\n"));
  WSContentSend_P (PSTR ("text.comp {font-size:0.8rem;}\n"));
  
  // bar graph : production style
  GetTextIndexed (str_color, sizeof (str_color), TIC_LEVEL_PROD, kTeleinfoLevelCalColor);
  WSContentSend_P (PSTR ("rect.p%u {stroke:%s;fill:%s;fill-opacity:0.5;}\n"), TIC_LEVEL_PROD, str_color, str_color);
  GetTextIndexed (str_color,  sizeof (str_color),  TIC_LEVEL_PROD, kTeleinfoLevelCalText);
  WSContentSend_P (PSTR ("text.p%u {fill:%s;}\n"), TIC_LEVEL_PROD, str_color);

  // bar graph : solar production style
  GetTextIndexed (str_color, sizeof (str_color), TIC_LEVEL_SOLAR, kTeleinfoLevelCalColor);
  WSContentSend_P (PSTR ("rect.p%u {stroke:%s;fill:%s;fill-opacity:0.7;}\n"), TIC_LEVEL_SOLAR, str_color, str_color);
  GetTextIndexed (str_color,  sizeof (str_color),  TIC_LEVEL_SOLAR, kTeleinfoLevelCalText);
  WSContentSend_P (PSTR ("text.p%u {fill:%s;}\n"), TIC_LEVEL_SOLAR, str_color);

  // curve : solar production forecast style
  GetTextIndexed (str_color, sizeof (str_color), TIC_LEVEL_FORECAST, kTeleinfoLevelCalColor);
  WSContentSend_P (PSTR ("path.p%u {stroke:%s;fill:%s;fill-opacity:0.1;}\n"), TIC_LEVEL_FORECAST, str_color, str_color);

  // bar graph : conso style
  for (index = 0; index < teleinfo_contract_db.period_qty; index ++)
  {
    // get level value and color
    level = teleinfo_contract_db.arr_period[index].level;
    if (level > 0) hchp = teleinfo_contract_db.arr_period[index].hchp; else hchp = 1;
    if (hchp == 0) strcpy_P (str_text, PSTR ("fill-opacity:0.2;")); else strcpy_P (str_text, PSTR ("fill-opacity:0.5;"));

    // bar style
    GetTextIndexed (str_color, sizeof (str_color), level, kTeleinfoLevelCalColor);
    WSContentSend_P (PSTR ("rect.c%u {stroke:%s;fill:%s;%s}\n"), index, str_color, str_color, str_text);      

    // text style
    GetTextIndexed (str_color, sizeof (str_color), level, kTeleinfoLevelCalText);
    WSContentSend_P (PSTR ("text.c%u {fill:%s;}\n"), index, str_color);      
  }

  WSContentSend_P (PSTR ("</style>\n"));

  // load data from file
  TeleinfoHistoFileLoadData ();

  // svg : units
  TeleinfoHistoDisplayUnits ();

  // svg : curves
  TeleinfoHistoDisplayBar ();

  // svg : main frame
  WSContentSend_P (PSTR ("<rect class='main' x=%d y=%d width=%d height=%d rx=10 />\n"), HISTO_LEFT, 0, HISTO_WIDE, HISTO_HEIGHT + 1);

  // svg : end
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentSend_P (PSTR ("</div>\n"));

  // --------------------
  //    Data selection
  // --------------------

  WSContentSend_P (PSTR ("<div><div class='lvl l3'>\n"));

  // solar production button
  if (teleinfo_solar.enabled)
  {
    choice = TeleinfoHistoButtonStatus (0, str_status, sizeof (str_status));
    GetTextIndexed (str_text, sizeof (str_text), TIC_LEVEL_SOLAR, kTeleinfoLevelLabel);
    WSContentSend_P (PSTR ("<a href='%s?%s=%u'><div class='item p%u%s'>%s</div></a>\n"), PSTR_PAGE_HISTO, PSTR (CMND_HISTO_DISPLAY), choice, TIC_LEVEL_SOLAR, str_status, str_text);
  }

  // production button
  if (teleinfo_prod.enabled)
  {
    choice = TeleinfoHistoButtonStatus (1, str_status, sizeof (str_status));
    GetTextIndexed (str_text, sizeof (str_text), TIC_LEVEL_PROD, kTeleinfoLevelLabel);
    WSContentSend_P (PSTR ("<a href='%s?%s=%u'><div class='item p%u%s'>%s</div></a>\n"), PSTR_PAGE_HISTO, PSTR (CMND_HISTO_DISPLAY), choice, TIC_LEVEL_PROD, str_status, str_text);
  }

  // conso period buttons
  for (index = 0; index < teleinfo_contract_db.period_qty; index ++)
    if (teleinfo_conso_wh.index[index] > 0)
    {
      choice = TeleinfoHistoButtonStatus (2 + index, str_status, sizeof (str_status));
      TeleinfoPeriodGetLabel (str_text, sizeof (str_text), index);
      WSContentSend_P (PSTR ("<a href='%s?%s=%u'><div class='item c%u%s'>%s</div></a>\n"), PSTR_PAGE_HISTO, PSTR (CMND_HISTO_DISPLAY), choice, index, str_status, str_text);
    }

  WSContentSend_P (PSTR ("</div></div>\n"));          // lvl l3

  // ----------------
  //    End of page
  // ----------------

  // main menu button
  WSContentSend_P (PSTR ("<div><form action='/' method='get'><button class='menu'>%s</button></form></div>\n"), D_MAIN_MENU);

  // end of page
  WSContentStop ();

  // log page serving time
  AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: Graph in %ums"), millis () - timestart);
}

#endif    // USE_WEBSERVER

/***************************************\
 *              Interface
\***************************************/

// Teleinfo historisation driver
bool XdrvTeleinfoHisto (const uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
    case FUNC_INIT:
      TeleinfoHistoInit ();
      break;

    case FUNC_SAVE_BEFORE_RESTART:
      TeleinfoHistoSaveConfig ();
      break;

    case FUNC_EVERY_SECOND:
      TeleinfoHistoEverySecond ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_MAIN_BUTTON:
      TeleinfoHistoWebMainButton ();
      break;

    case FUNC_WEB_ADD_HANDLER:
      Webserver->on (FPSTR (PSTR_PAGE_HISTO), TeleinfoHistoWebPage);
    break;
#endif    // USE_WEBSERVER
  }

  return result;
}

#endif    // USE_UFILESYS
#endif    // USE_TELEINFO_HISTO
#endif    // USE_TELEINFO
