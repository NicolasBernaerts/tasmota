/*
  xsns_98_02_sensor_histo.ino - Generic sensor historisation graph

  Copyright (C) 2025  Nicolas Bernaerts

  Version history :
    11/11/2025 v1.0 - Split form generic sensor file

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

#ifdef USE_SENSOR_HISTO
#ifdef USE_UFILESYS

/*************************************************\
 *               Variables
\*************************************************/

#define SENSOR_HISTO_VERSION              2           // saved data version

#define SENSOR_SLOT_MAX                   186         // one record every 2 days

#define SENSOR_HISTO_LINE_SIZE            52          // size of historisation line
#define SENSOR_HISTO_GRAPH_MAX            32          // maximum number of values in graph

#define SENSOR_HISTO_TEMP_MIN             10
#define SENSOR_HISTO_TEMP_MAX             25 

// graph dimensions
#define SENSOR_HISTO_LEFT                 60          // left position of the curve
#define SENSOR_HISTO_WIDE                 1200        // graph usable width
#define SENSOR_HISTO_WIDTH                1320        // graph global width
#define SENSOR_HISTO_HEIGHT               600         // default graph height
#define TIC_HISTO_BAR_HEIGHT_MIN          20          // min bar height to display value

#define SENSOR_HISTO_FS_MINSIZE           50          // minimum left size on FS before cleaning old files (kB)

#define SENSOR_TITLE_HISTO                "History"

// web URL
const char PSTR_SENSOR_PAGE_HISTO[] PROGMEM = "/histo";

enum SensorHistoFilePosition { SENSOR_HISTO_FILE_BEFORE, SENSOR_HISTO_FILE_FOUND, SENSOR_HISTO_FILE_BEYOND, SENSOR_HISTO_FILE_MAX };

enum SensorHistoPeriod                  { SENSOR_HISTO_PERIOD_DAY, SENSOR_HISTO_PERIOD_WEEK, SENSOR_HISTO_PERIOD_MONTH, SENSOR_HISTO_PERIOD_YEAR, SENSOR_HISTO_PERIOD_MAX };
const char kTeleinfoHistoPeriod[] PROGMEM =      "Journée"     "|"      "Semaine"     "|"         "Mois"      "|"        "Année";                                                                                                                             // units labels

// config and historisation file
const char PSTR_SENSOR_HISTO_CONFIG[] PROGMEM = "/sensor-histo.dat";
const char PSTR_SENSOR_HISTO_FILE[]   PROGMEM = "/sensor-%04u.csv";
const char PSTR_SENSOR_HISTO_MASK[]   PROGMEM = "sensor-*.csv";

#define CMND_SENSOR_HISTO_PERIOD                "period"
#define CMND_SENSOR_HISTO_DISPLAY               "display"
#define CMND_SENSOR_HISTO_MIN_MINUS             "minminus"
#define CMND_SENSOR_HISTO_MIN_PLUS              "minplus"
#define CMND_SENSOR_HISTO_MAX_MINUS             "maxminus"
#define CMND_SENSOR_HISTO_MAX_PLUS              "maxplus"
#define CMND_SENSOR_HISTO_PREV                  "prev"
#define CMND_SENSOR_HISTO_NEXT                  "next"

/****************************************\
 *                 Data
\****************************************/

// sensor data
struct histo_item {        // 14 bytes
  float    min;                           // minimum 
  float    max;                           // maximum 
  float    sum;                           // sum of samples
  uint16_t nbr;                           // number of samples
};

struct histo_config {       // 16 bytes
  uint8_t  period;                        // current period of graph
  uint8_t  mask;                          // mask of data to display
  float    temp_min;                      // minimum temperature
  float    temp_max;                      // maximum temperature
  uint16_t powr_max;                      // maximum power
  uint32_t timestamp;                     // current timestamp for graph
};

static struct {             // 60 bytes
  uint8_t      hour = UINT8_MAX;          // last hour slot
  bool         activity = false;          // activity flag
  bool         presence = false;          // presence flag
  histo_item   humidity;                  // humidity data
  histo_item   temperature;               // temperature data
  histo_item   power;                     // power data
  histo_config graph;                     // graph data
} sensor_histo;

struct histo_sample {       // 17 bytes
  bool pres;                              // graph data presence
  int  humi_avg;                          // graph data average humidty
  int  temp_min;                          // graph data minimal temperature
  int  temp_max;                          // graph data maximum temperature
  int  powr_avg;                          // graph data average power
};

histo_sample sensor_histo_data[SENSOR_SLOT_MAX];         // display data for graph, 3.35kb

/*********************************************\
 *               Functions
\*********************************************/

void SensorHistoHumiInit ()
{
  sensor_histo.humidity.min = NAN;
  sensor_histo.humidity.max = NAN;
  sensor_histo.humidity.nbr = 0;
  sensor_histo.humidity.sum = 0;
}

void SensorHistoHumiUpdate (const float humi)
{
  // check parameter
  if (isnan (humi)) return;

  // update
  if (isnan (sensor_histo.humidity.min)) sensor_histo.humidity.min = humi;
    else sensor_histo.humidity.min = min (sensor_histo.humidity.min, humi);
  if (isnan (sensor_histo.humidity.max)) sensor_histo.humidity.max = humi;
    else sensor_histo.humidity.max = max (sensor_histo.humidity.max, humi);
  sensor_histo.humidity.sum += humi; 
  sensor_histo.humidity.nbr ++; 
}

void SensorHistoTempInit ()
{
  sensor_histo.temperature.min = NAN;
  sensor_histo.temperature.max = NAN;
  sensor_histo.temperature.nbr = 0;
  sensor_histo.temperature.sum = 0;
}

void SensorHistoTempUpdate (const float temp)
{
  // check parameter
  if (isnan (temp)) return;

  // update
  if (isnan (sensor_histo.temperature.min)) sensor_histo.temperature.min = temp;
    else sensor_histo.temperature.min = min (sensor_histo.temperature.min, temp);
  if (isnan (sensor_histo.temperature.max)) sensor_histo.temperature.max = temp;
    else sensor_histo.temperature.max = max (sensor_histo.temperature.max, temp);
  sensor_histo.temperature.sum += temp; 
  sensor_histo.temperature.nbr ++; 
}

void SensorHistoPowerInit ()
{
  sensor_histo.power.min = UINT16_MAX;
  sensor_histo.power.max = 0;
  sensor_histo.power.nbr = 0;
  sensor_histo.power.sum = 0;
}

void SensorHistoPowerUpdate (const float power)
{
  // check parameter
  if (isnan (power)) return;

  // update
  if (isnan (sensor_histo.power.min)) sensor_histo.power.min = power;
    else sensor_histo.power.min = min (sensor_histo.power.min, power);
  if (isnan (sensor_histo.power.max)) sensor_histo.power.max = power;
    else sensor_histo.power.max = max (sensor_histo.power.max, power);
  sensor_histo.power.nbr++;
  sensor_histo.power.sum += power;
}

void SensorHistoPresenceInit ()
{
  sensor_histo.presence = false;
}

void SensorHistoPresenceSet ()
{
  sensor_histo.presence = true;
}

void SensorHistoActivityInit ()
{
  sensor_histo.activity = false;
}

void SensorHistoActivitySet ()
{
  sensor_histo.activity = true;
}

/*********************************************\
 *             Configuration
\*********************************************/

void SensorHistoInitGraph ()
{
  sensor_histo.graph.timestamp = 0;
  sensor_histo.graph.mask      = 0xff;
  sensor_histo.graph.period    = SENSOR_HISTO_PERIOD_DAY;
  sensor_histo.graph.temp_min  = SENSOR_HISTO_TEMP_MIN;
  sensor_histo.graph.temp_max  = SENSOR_HISTO_TEMP_MAX;
  sensor_histo.graph.powr_max  = 0;
}

// load config file
void SensorHistoLoadData ()
{
  uint8_t  version;
  char     str_filename[UFS_FILENAME_SIZE];
  File     file;

  // open file in read mode
  strcpy_P (str_filename, PSTR_SENSOR_HISTO_CONFIG);
  if (ffsp->exists (str_filename))
  {
    file = ffsp->open (str_filename, "r");
    file.read ((uint8_t*)&version, sizeof (version));
    if (version == SENSOR_HISTO_VERSION)
    {
      file.read ((uint8_t*)&sensor_histo, sizeof (sensor_histo));
    }
    file.close ();
  }
}

// save config file
void SensorHistoSaveData ()
{
  uint8_t version;
  char    str_filename[UFS_FILENAME_SIZE];
  File    file;

  // init data
  version = SENSOR_HISTO_VERSION;

  // open file in write mode
  strcpy_P (str_filename, PSTR_SENSOR_HISTO_CONFIG);
  file = ffsp->open (str_filename, "w");
  if (file > 0)
  {
    file.write ((uint8_t*)&version, sizeof (version));
    file.write ((uint8_t*)&sensor_histo, sizeof (sensor_histo));
    file.close ();
  }
}

/*********************************************\
 *               Historisation
\*********************************************/

bool SensorHistoFileIsCandidate (const char* pstr_filename, const time_t filetime, time_t& prevtime)
{
  bool  candidate = false;
  char *pstr_digit;
  char  str_mask[UFS_FILENAME_SIZE];

  // check parameters
  if (pstr_filename == nullptr) return false;

  // setup file mask
  strcpy_P (str_mask, PSTR_SENSOR_HISTO_MASK);
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
    AddLog (LOG_LEVEL_DEBUG, PSTR ("SEN: match %s"), pstr_filename);
  }

  return candidate;
}

// check FS size and clenup oldest file if needed
void SensorHistoFileCleanup ()
{
  uint32_t free;
  time_t   time_target = 0;
  String   str_root, str_target;

  // init
  str_root = F ("/");

  // if not enough space on FS, loop to find oldest file
  free = UfsFree ();
  AddLog (LOG_LEVEL_INFO, PSTR ("TIC: free %u kB for %u kB minimum"), free, SENSOR_HISTO_FS_MINSIZE);
  if (free < SENSOR_HISTO_FS_MINSIZE)
  {
#ifdef ESP32
    File directory = ffsp->open (str_root.c_str ());
    File file      = directory.openNextFile ();
    while (file) 
    {
      if (SensorHistoFileIsCandidate (file.name (), file.getLastWrite (), time_target)) str_target = str_root + file.name ();
      file = directory.openNextFile ();
    }

#else     // ESP8266
    Dir directory = ffsp->openDir (str_root.c_str ());
    while (directory.next ())
      if (SensorHistoFileIsCandidate (directory.fileName ().c_str (), directory.fileTime (), time_target)) str_target = str_root + directory.fileName ();

#endif    // ESP32 or ESP8266
  }

  // if file was found, remove and log
  if (str_target != "")
  {
    ffsp->remove (str_target.c_str ());
    AddLog (LOG_LEVEL_INFO, PSTR ("TIC: %s deleted"), str_target.c_str ());
  }
}

// Check and create historisation file
//   dddwwdmmddhh;humi_min;humi_max;temp_min;temp_max;powr_min;powr_max;powr_avg;pres;acti
void SensorHistoFilePrepare (const uint16_t year)
{
  uint8_t index;
  char    str_value[12];
  char    str_text[SENSOR_HISTO_LINE_SIZE];
  File    file;

  // set filename
  sprintf_P (str_text, PSTR_SENSOR_HISTO_FILE, year);

  // if file exists, ignore
  if (ffsp->exists (str_text)) return;

  // open file in create mode
  file = ffsp->open (str_text, "w");

  // generate header
  strcpy_P (str_text, PSTR ("dddwwdmmddhh;humi_min;humi_max;humi_avg;temp_min;temp_max;temp_avg;powr_min;powr_max;powr_avg;pres;acti\n"));
  file.print (str_text);

  // close file
  file.close ();
}

// Save historisation data (max 422k per year)
//   dddwwdmmddhh;humi_min;humi_max;humi_avg;temp_min;temp_max;temp_avg;powr_min;powr_max;powr_avg;pres
void SensorHistoFileSaveData ()
{
  uint8_t   week, doweek;
  uint16_t  year;
  uint32_t  time_now;
  TIME_T    time_dst;
  char      str_value[24];
  char      str_text[SENSOR_HISTO_LINE_SIZE];
  File      file;

  // if date not defined, cancel
  if (!RtcTime.valid) return;

  // calculate timestamp (30sec before to get previous slot)
  time_now = LocalTime () - 30;
  BreakTime (time_now, time_dst);
  doweek = CalendarGetDayOfWeek (time_now);
  week   = CalendarGetWeekNumber (time_now);
  year   = time_dst.year;

  // open in append mode
  sprintf_P (str_text, PSTR_SENSOR_HISTO_FILE, year + 1970);
  file = ffsp->open (str_text, "a");

  // line : beginning
  CalendarFileGetHeader (time_now, str_text, sizeof (str_text));

  // humidity
  if (sensor_histo.humidity.nbr > 0) sprintf_P (str_value, PSTR (";%d;%d;%d"), (int)sensor_histo.humidity.min, (int)sensor_histo.humidity.max, (int)sensor_histo.humidity.sum / sensor_histo.humidity.nbr);
    else strcpy_P (str_value, PSTR (";;;"));
  strlcat (str_text, str_value, sizeof (str_text));

  // temperature
  if (sensor_histo.temperature.nbr > 0) sprintf_P (str_value, PSTR (";%d;%d;%d"), (int)sensor_histo.temperature.min, (int)sensor_histo.temperature.max, (int)sensor_histo.temperature.sum / sensor_histo.temperature.nbr);
    else strcpy_P (str_value, PSTR (";;;"));
  strlcat (str_text, str_value, sizeof (str_text));

  // power
  if (sensor_histo.power.nbr > 0) sprintf_P (str_value, PSTR (";%d;%d;%d"), (int)sensor_histo.power.min, (int)sensor_histo.power.max, (int)sensor_histo.power.sum / sensor_histo.power.nbr);
    else strcpy_P (str_value, PSTR (";;;"));
  strlcat (str_text, str_value, sizeof (str_text));

  // presence
  sprintf_P (str_value, PSTR (";%u"), sensor_histo.presence);
  strlcat (str_text, str_value, sizeof (str_text));

  // activity
  sprintf_P (str_value, PSTR (";%u"), sensor_histo.activity);
  strlcat (str_text, str_value, sizeof (str_text));

  // write line and close file
  strcat_P (str_text, PSTR ("\n"));
  file.print (str_text);
  file.close ();

  // if year has changed, prepare file for new year
  BreakTime (LocalTime (), time_dst);
  if (year != time_dst.year) SensorHistoFilePrepare (time_dst.year + 1970);

  // reset historisation data
  SensorHistoTempInit ();
  SensorHistoHumiInit ();
  SensorHistoPowerInit ();
  SensorHistoPresenceInit ();
  SensorHistoActivityInit ();
}

// extract data from a line
int SensorHistoExtractLine (char* pstr_line, struct histo_sample &data)
{
  int   value, column;
  char *pstr_token;
  char *pstr_digit;

  // loop thru delimiter
  column = 0;
  pstr_token = strtok (pstr_line, ";");
  while (pstr_token)
  {
    // convert value to value * 10
    if (column > 0)
    {
      value = atoi (pstr_token) * 10;
      pstr_digit = strchr (pstr_token, '.');
      if (pstr_digit) value += atoi (++pstr_digit);
    }

    // extract data according to column
    switch (++column)
    {
      case 4:     // average humidity
        data.humi_avg = value;
        break;      

      case 5:     // minimal temperature
        data.temp_min = value;
        break;     

      case 6:     // maximum temperature
        data.temp_max = value;
        break;     

      case 10:     // average power
        data.powr_avg = value;
        break;   

      case 11:
        data.pres = (value != 0);
        break;
    }

    // look for next token
    pstr_token = strtok (nullptr, ";");
  }

 return column;
}

// load data from file
bool SensorHistoFileLoadPeriod (const uint8_t period, const uint32_t timestamp)
{
  bool     read_next;
  int      index, index1, index2;
  uint8_t  status;
  uint32_t time_now;
  size_t   target_start, target_length, index1_start, index1_length, index2_start, index2_length;
  size_t   length, line_first, line_current;
  char     str_target[4];
  char     str_index[4];
  char     str_line[SENSOR_HISTO_LINE_SIZE];
  TIME_T   time_dst;
  File     file;
  histo_sample line_data;

  // check parameters
  if (period >= SENSOR_HISTO_PERIOD_MAX) return false;
  if (timestamp == 0) return false;

  // check data file presence
  BreakTime (timestamp, time_dst);
  sprintf_P (str_line, PSTR_SENSOR_HISTO_FILE, 1970 + time_dst.year);
  if (!ffsp->exists (str_line)) return false;
  
  // init data
  time_now     = millis ();
  status       = SENSOR_HISTO_FILE_BEFORE;
  line_first   = 0;
  line_current = 0;
  index        = INT_MAX;
//  index_begin  = INT_MAX;
  memset (&line_data, 0, sizeof (histo_sample));

  // prepare data according to period
  switch (period)
  {
    case SENSOR_HISTO_PERIOD_DAY:
      target_start = 0;  target_length = 3;
      index1_start = 10; index1_length = 2;
      index2_start = 0;  index2_length = 0;
      sprintf_P (str_target, PSTR ("%03u"), time_dst.day_of_year);
      break;

    case SENSOR_HISTO_PERIOD_WEEK:
      target_start = 3;  target_length = 2;
      index1_start = 5;  index1_length = 1;
      index2_start = 10; index2_length = 2;
      sprintf_P (str_target, PSTR ("%02u"), CalendarGetWeekNumber (timestamp));
      break;

    case SENSOR_HISTO_PERIOD_MONTH:
      target_start = 6;  target_length = 2;
      index1_start = 8;  index1_length = 2;
      index2_start = 10; index2_length = 2;
      sprintf_P (str_target, PSTR ("%02u"), time_dst.month);
      break;

    case SENSOR_HISTO_PERIOD_YEAR:
      target_start = 0; target_length = 0;
      index1_start = 0; index1_length = 3;
      index2_start = 0; index2_length = 0;
      str_target[0] = 0;
      break;
  }

  // open file and skip header
  file = ffsp->open (str_line, "r");
  file.readBytesUntil ('\n', str_line, sizeof (str_line) - 1);

  // parse file
  read_next = file.available ();
  while (read_next)
  {
    // read next line
    length    = file.readBytesUntil ('\n', str_line, sizeof (str_line) - 1);
    read_next = file.available ();

    // handle current line
    str_line[length] = 0;
    line_current++;

    // check if target is found or beyond
    if (period == SENSOR_HISTO_PERIOD_YEAR) (line_current > 1) ? status = SENSOR_HISTO_FILE_FOUND : status = SENSOR_HISTO_FILE_BEFORE;
    else if (strncmp (str_line + target_start, str_target, target_length) == 0) status = SENSOR_HISTO_FILE_FOUND;
    else if (status == SENSOR_HISTO_FILE_FOUND) status = SENSOR_HISTO_FILE_BEYOND;

    // handle according to status
    switch (status)
    {
      case SENSOR_HISTO_FILE_BEYOND:
        read_next = false;
        break;
      
      case SENSOR_HISTO_FILE_FOUND:
        // if needed, set first line
        if (line_first == 0) line_first = line_current;

        // read new indexes
        index2 = 0;
        strlcpy (str_index, str_line + index1_start, index1_length + 1);
        index1 = atoi (str_index);
        strlcpy (str_index, str_line + index2_start, index2_length + 1);
        if (str_index[0] != 0) index2 = atoi (str_index); 
        
        // calculate global index
        index = 0;
        switch (period)
        {
          case SENSOR_HISTO_PERIOD_DAY:
            index = index1;
            break;

          case SENSOR_HISTO_PERIOD_WEEK:
            index = (index1 - 1) * 24 + index2;
            break;

          case SENSOR_HISTO_PERIOD_MONTH:
            index = (index1 - 1) * 6 + index2 / 4;
            break;

          case SENSOR_HISTO_PERIOD_YEAR:
            index = index1 / 2;
            break;
          }
        
        // extract data from current line
        SensorHistoExtractLine (str_line, line_data);

        // update slot data
        sensor_histo_data[index].humi_avg = max (sensor_histo_data[index].humi_avg, line_data.humi_avg);
        sensor_histo_data[index].temp_min = min (sensor_histo_data[index].temp_min, line_data.temp_min);
        sensor_histo_data[index].temp_max = max (sensor_histo_data[index].temp_max, line_data.temp_max);
        sensor_histo_data[index].powr_avg = max (sensor_histo_data[index].powr_avg, line_data.powr_avg);
        sensor_histo_data[index].pres |= line_data.pres;

        // update graph boundaries (even numbers)
        sensor_histo.graph.temp_min = min (sensor_histo.graph.temp_min, ((float)line_data.temp_min / 20) * 20);
        sensor_histo.graph.temp_max = max (sensor_histo.graph.temp_max, ((float)line_data.temp_max / 20) * 20 + 20);
        sensor_histo.graph.powr_max = max (sensor_histo.graph.powr_max, (uint16_t)((line_data.powr_avg / 100 + 1) * 100));
        break;
    }

    // do a yield every 500 lines
    if (line_current % 500 == 0) yield ();
  }

  // log data loading
  if (line_first == 0) strcpy_P (str_line, PSTR ("no data"));
    else sprintf_P (str_line, PSTR ("lines %u to %u"),  line_first, line_current);
  AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: Loaded %u, period %u, target %s, %s [%ums]"), 1970 + time_dst.year, sensor_histo.graph.period, str_target, str_line, millis () - time_now);

  return (line_first > 0);
}

// load data from file
void SensorHistoFileLoadData ()
{
  uint8_t index, week;

  // init data
  for (index = 0; index < SENSOR_HISTO_GRAPH_MAX; index ++)
  {
    sensor_histo_data[index].pres = false;
    sensor_histo_data[index].humi_avg = 0;
    sensor_histo_data[index].temp_min = INT16_MAX;
    sensor_histo_data[index].temp_max = INT16_MIN;
    sensor_histo_data[index].powr_avg = 0;
  }

  // check parameters
  if (sensor_histo.graph.period >= SENSOR_HISTO_PERIOD_MAX) return;
  if (sensor_histo.graph.timestamp == 0) return;

  // load data for current period
  SensorHistoFileLoadPeriod (sensor_histo.graph.period, sensor_histo.graph.timestamp);

  // if dealing with weekly period
  if (sensor_histo.graph.period == SENSOR_HISTO_PERIOD_WEEK)
  {
    // get current week number
    week = CalendarIsWeekComplete (sensor_histo.graph.timestamp);

    // if needed, append first week of next year (6 days shift)
    if (week == 1) SensorHistoFileLoadPeriod (sensor_histo.graph.period, sensor_histo.graph.timestamp + 518400);

    // else if needed, append last week of previous year (6 days shift)
    else if (week == 53) SensorHistoFileLoadPeriod (sensor_histo.graph.period, sensor_histo.graph.timestamp - 518400);
  }
}

/*********************************************\
 *                   Callback
\*********************************************/

// load config file
void SensorHistoInit ()
{ 
  // load graph configuration
  SensorHistoInitGraph ();
  SensorHistoLoadData ();

  // init data
  SensorHistoHumiInit ();
  SensorHistoTempInit ();
  SensorHistoPowerInit ();
  SensorHistoPresenceInit ();
  SensorHistoActivityInit ();

  // reset current hour
  sensor_histo.hour = UINT8_MAX;
}

// called every second
void SensorHistoEverySecond ()
{
  bool  result;
  float value;

  // ignore if time is not valid
  if (!RtcTime.valid) return;

  // if needed, init time reference
  if (sensor_histo.graph.timestamp == 0) sensor_histo.graph.timestamp = LocalTime ();

  // temperature
  value = SensorTemperatureGet ();
  SensorHistoTempUpdate (value);

  // humidity
  value = SensorHumidityGet ();
  SensorHistoHumiUpdate (value);

  // power
  value = SensorPowerGet ();
  SensorHistoPowerUpdate (value);

  // presence
  result = SensorPresenceGet ();
  if (result) SensorHistoPresenceSet ();
        
  // activity
  result = SensorActivityGet ();
  if (result) SensorHistoActivitySet ();
        
  // if hour has changed, save data
  if (sensor_histo.hour == UINT8_MAX) sensor_histo.hour = RtcTime.hour;
  if (sensor_histo.hour != RtcTime.hour)
  {
    // free space on FS
    SensorHistoFileCleanup ();

    // update data
    SensorHistoFileSaveData ();

    // update current hour
    sensor_histo.hour = RtcTime.hour;
  }
  
}

// save before restart
void SensorHistoSaveBeforeRestart ()
{
  // save config parameters
  SensorHistoSaveData ();
}

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

// Graph units
void SensorHistoDisplayUnits ()
{
  uint8_t arr_pos[5] = {98,76,51,26,2};  
  int     index, value, delta;

  // check parameter
  if (sensor_histo.graph.period >= SENSOR_HISTO_PERIOD_MAX) return;

  // loop thru 5 unit levels
  for (index = 0; index < 5; index ++)
  {
    // temperature
    delta = sensor_histo.graph.temp_max - sensor_histo.graph.temp_min;
    value = sensor_histo.graph.temp_max - index * delta / 5;
    WSContentSend_P (PSTR ("<text class='temp' x=%u%% y=%u%% >%d.%d °C</text>\n"), 2, arr_pos[index], value / 10, abs (value) % 10);

    // humidity
    WSContentSend_P (PSTR ("<text class='humi' x=%u%% y=%u%% >%u %%</text>\n"), 98, arr_pos[index], 100 - index * 25);
  }
}

// Graph frame
void SensorHistoDisplayLevel ()
{
  uint32_t index;  

  // graph level lines
  for (index = 1; index < 4; index ++) WSContentSend_P (PSTR ("<line class='dash' x1=%u y1=%u x2=%u y2=%u />\n"), SENSOR_HISTO_LEFT, index * SENSOR_HISTO_HEIGHT / 4, SENSOR_HISTO_LEFT + SENSOR_HISTO_WIDE, index * SENSOR_HISTO_HEIGHT / 4);
}

// Graph frame
void SensorHistoDisplayFrame ()
{
  // graph frame
  WSContentSend_P (PSTR ("<rect class='main' x=%d y=%d width=%d height=%d rx=10 />\n"), SENSOR_HISTO_LEFT, 0, SENSOR_HISTO_WIDE, SENSOR_HISTO_HEIGHT);
}

// Append sensor histo button to main page
void SensorHistoWebMainButton ()
{
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>Historique</button></form></p>\n"), PSTR_SENSOR_PAGE_HISTO);
}

// display curves
void SensorHistoDisplayCurve ()
{
  bool    is_first;
  long    index, temp_delta, slot_max, graph_x, graph_y, graph_stop;
  uint8_t arr_slot[SENSOR_HISTO_PERIOD_MAX] = { 24, 168, 186, 183 };

  // check parameter
  if (sensor_histo.graph.period >= SENSOR_HISTO_PERIOD_MAX) return;

  // init graph units
  slot_max = arr_slot[sensor_histo.graph.period];

  // humidity
  WSContentSend_P (PSTR ("<polyline class='humi' points='"));
  for (index = 0; index < slot_max; index++)
    if (sensor_histo_data[index].humi_avg > 0)
    {
      graph_x = SENSOR_HISTO_LEFT + SENSOR_HISTO_WIDE * index / slot_max;
      graph_y = SENSOR_HISTO_HEIGHT - SENSOR_HISTO_HEIGHT * (long)sensor_histo_data[index].humi_avg / 100;
      WSContentSend_P (PSTR ("%d,%d "), graph_x, graph_y);
    }
  WSContentSend_P (PSTR ("' />"));

  // temperature
  is_first = true;
  temp_delta = (long)(sensor_histo.graph.temp_max - sensor_histo.graph.temp_min);
  WSContentSend_P (PSTR ("<path class='temp' d='M%d 0 M%d %d "), SENSOR_HISTO_LEFT, SENSOR_HISTO_LEFT, SENSOR_HISTO_HEIGHT);
  for (index = 0; index < slot_max; index++)
    if (sensor_histo_data[index].temp_max != INT16_MIN)
    {
      graph_x = SENSOR_HISTO_LEFT + SENSOR_HISTO_WIDE * index / slot_max;
      graph_y = SENSOR_HISTO_HEIGHT * (long)(sensor_histo.graph.temp_max - sensor_histo_data[index].temp_max) / temp_delta;
      if (is_first) WSContentSend_P (PSTR ("M%d,%d "), graph_x, graph_y);
        else WSContentSend_P (PSTR ("L%d,%d "), graph_x, graph_y);
      is_first = false; 
    }
  for (index = slot_max - 1; index >= 0; index--)
    if (sensor_histo_data[index].temp_min != INT16_MAX)
    {
      graph_x = SENSOR_HISTO_LEFT + SENSOR_HISTO_WIDE * index / slot_max;
      graph_y = SENSOR_HISTO_HEIGHT * (long)(sensor_histo.graph.temp_max - sensor_histo_data[index].temp_min) / temp_delta;
      WSContentSend_P (PSTR ("L%d,%d "), graph_x, graph_y);
    }
  WSContentSend_P (PSTR ("Z'></path>\\n"));  

  // presence
  graph_x = LONG_MAX;
  for (index = 0; index < slot_max; index++)
    if (sensor_histo_data[index].pres && (graph_x == LONG_MAX)) graph_x = SENSOR_HISTO_LEFT + SENSOR_HISTO_WIDE * index / slot_max;
    else if (sensor_histo_data[index].pres) graph_stop = SENSOR_HISTO_LEFT + SENSOR_HISTO_WIDE * index / slot_max;
    else if (!sensor_histo_data[index].pres)
    {
      WSContentSend_P (PSTR ("<rect class='acti' x=%d y=%d rx='4' ry='4' width=%d height=%d ></rect>"), SENSOR_HISTO_LEFT, SENSOR_HISTO_HEIGHT - 45, graph_stop - graph_x, 45);
      graph_x = LONG_MAX;
    }
}

// Graph public page
void SensorHistoWebPage ()
{
  bool     display;
  bool     refresh = false;
  uint8_t  index, level, hchp;
  uint16_t choice, mask;  
  uint32_t timestart, timestamp;
  char     str_color[8];
  char     str_status[16];
  char     str_text[32];

  // timestamp
  timestart = millis ();

  // beginning of form without authentification
  WSContentStart_P (PSTR (SENSOR_TITLE_HISTO), false);

  // get period
  sensor_histo.graph.period = (uint8_t)SensorGetArgValue (CMND_SENSOR_HISTO_PERIOD, 0, SENSOR_HISTO_PERIOD_MAX - 1, sensor_histo.graph.period);  

  // check data display argument
  if (Webserver->hasArg (F (CMND_SENSOR_HISTO_DISPLAY)))
  {
    WebGetArg (PSTR (CMND_SENSOR_HISTO_DISPLAY), str_text, sizeof (str_text));
    sensor_histo.graph.mask = (uint16_t)atoi (str_text);
    refresh = true;
  }
  
  // minimum graph decrement
  index = SensorGetArgValue (CMND_SENSOR_HISTO_MIN_MINUS, 0, 1, 0);
  if (index == 1) { refresh = true; sensor_histo.graph.temp_min -= 200; }

  // minimum graph increment
  index = SensorGetArgValue (CMND_SENSOR_HISTO_MIN_PLUS, 0, 1, 0);
  if (index == 1) { refresh = true; sensor_histo.graph.temp_min += 200; }

  // maximum graph decrement
  index = SensorGetArgValue (CMND_SENSOR_HISTO_MAX_MINUS, 0, 1, 0);
  if (index == 1) { refresh = true; sensor_histo.graph.temp_max -= 200; }
    
  // maximum graph increment
  index = SensorGetArgValue (CMND_SENSOR_HISTO_MAX_PLUS, 0, 1, 0);
  if (index == 1) { refresh = true; sensor_histo.graph.temp_max += 200; }

  // handle previous period
  index = SensorGetArgValue (CMND_SENSOR_HISTO_PREV, 0, 5, 0);
  if (index > 0) { refresh = true; sensor_histo.graph.timestamp = CalendarGetPreviousTimestamp (sensor_histo.graph.period, sensor_histo.graph.timestamp, index); }

  // handle next period
  index = SensorGetArgValue (CMND_SENSOR_HISTO_NEXT, 0, 5, 0);
  if (index > 0) { refresh = true; sensor_histo.graph.timestamp = CalendarGetNextTimestamp (sensor_histo.graph.period, sensor_histo.graph.timestamp, index); }

  if (refresh)
  {
    WSContentSend_P (PSTR ("window.location.href=window.location.href.split('?')[0];\n"));
    WSContentSend_P (PSTR ("</script>\n"));
  }

  else
  {
    // javascript : screen swipe for previous and next period
    WSContentSend_P (PSTR ("\n\nlet startX=0;let stopX=0;let startY=0;let stopY=0;\n"));
    WSContentSend_P (PSTR ("window.addEventListener('touchstart',function(evt){startX=evt.changedTouches[0].pageX;startY=evt.changedTouches[0].pageY;},false);\n"));
    WSContentSend_P (PSTR ("window.addEventListener('touchend',function(evt){stopX=evt.changedTouches[0].pageX;stopY=evt.changedTouches[0].pageY;handleGesture();},false);\n"));
    WSContentSend_P (PSTR ("function handleGesture(){\n"));
    WSContentSend_P (PSTR (" let deltaX=stopX-startX;let deltaY=Math.abs(stopY-startY);\n"));
    WSContentSend_P (PSTR (" if(deltaY<10 && deltaX<-100){window.location='%s?next=1';}\n"), PSTR_SENSOR_PAGE_HISTO);
    WSContentSend_P (PSTR (" else if(deltaY<10 && deltaX>100){window.location='%s?prev=1';}\n"), PSTR_SENSOR_PAGE_HISTO);
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

    WSContentSend_P (PSTR ("div.minus {position:absolute;left:1vw;width:2.5vw;margin:auto;border:1px #666 solid;border-radius:4px;}\n"));
    WSContentSend_P (PSTR ("div.plus {position:absolute;left:4.5vw;width:2.5vw;margin:auto;border:1px #666 solid;border-radius:4px;}\n"));
    WSContentSend_P (PSTR ("div.minus:hover,div.plus:hover {background:#aaa;}\n"));

    WSContentSend_P (PSTR ("div.lvl {display:inline-block;padding:1px;margin:0px;color:#fff;}\n"));
    WSContentSend_P (PSTR ("div.lvl div {background:#666;}\n"));
    WSContentSend_P (PSTR ("div.lvl a div {background:none;}\n"));
    WSContentSend_P (PSTR ("div.l1 {border:1px #666 solid;border-radius:6px;}\n"));
    WSContentSend_P (PSTR ("div.l1 div {padding:2px 10px;}\n"));
    WSContentSend_P (PSTR ("div.l2 div {padding:2px 10px;border:1px #666 solid;}\n"));

    WSContentSend_P (PSTR ("div.l3 div {padding:4px 12px;margin:auto 2px;width:10vw;border:1px #444 solid;border-radius:6px;}\n"));
    WSContentSend_P (PSTR ("div.l3 div.temp {color:#333;background:#f7dc6f;}\n"));
    WSContentSend_P (PSTR ("div.l3 div.humi {color:#fff;background:#3498db;}\n"));
    WSContentSend_P (PSTR ("div.l3 div.pres {color:#333;background:#73c6b6;}\n"));
    WSContentSend_P (PSTR ("div.l3 div.acti {color:#fff;background:#ec7063;}\n"));
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
    WSContentSend_P (PSTR ("<a href='%s?%s=1'><div class='range'>+</div></a>\n"), PSTR_SENSOR_PAGE_HISTO, PSTR (CMND_SENSOR_HISTO_MAX_PLUS));
    WSContentSend_P (PSTR ("<div class='lvl l1'>\n"));

    for (index = 0; index < SENSOR_HISTO_PERIOD_MAX; index ++)
    {
      GetTextIndexed (str_text, sizeof (str_text), index, kTeleinfoHistoPeriod);
      if (sensor_histo.graph.period != index) WSContentSend_P (PSTR ("<a href='%s?period=%u'>"), PSTR_SENSOR_PAGE_HISTO, index);
      WSContentSend_P (PSTR ("<div class='item'>%s</div>"), str_text);
      if (sensor_histo.graph.period != index) WSContentSend_P (PSTR ("</a>"));
      WSContentSend_P (PSTR ("\n"));
    }

    WSContentSend_P (PSTR ("</div></div>\n"));        // lvl l1

    // ----------------------
    //    Level 2 - Periods
    // ----------------------

    WSContentSend_P (PSTR ("<div>\n"));
    WSContentSend_P (PSTR ("<a href='%s?%s=1'><div class='range'>-</div></a>\n"), PSTR_SENSOR_PAGE_HISTO, PSTR (CMND_SENSOR_HISTO_MAX_MINUS));
    WSContentSend_P (PSTR ("<div class='lvl l2'>\n"));

    // previous periods
    timestamp = CalendarGetPreviousTimestamp (sensor_histo.graph.period, sensor_histo.graph.timestamp, 5);
    CalendarGetPeriodLabel (sensor_histo.graph.period, timestamp, str_text, sizeof (str_text));
    WSContentSend_P (PSTR ("<a href='%s?prev=%u'><div class='item' title='%s'>&lt; &lt;</div></a>\n"), PSTR_SENSOR_PAGE_HISTO, 5, str_text);

    timestamp = CalendarGetPreviousTimestamp (sensor_histo.graph.period, sensor_histo.graph.timestamp, 1);
    CalendarGetPeriodLabel (sensor_histo.graph.period, timestamp, str_text, sizeof (str_text));
    WSContentSend_P (PSTR ("<a href='%s?prev=%u'><div class='item' title='%s'>&lt;</div></a>\n"), PSTR_SENSOR_PAGE_HISTO, 1, str_text);

    // current period
    CalendarGetPeriodLabel (sensor_histo.graph.period, sensor_histo.graph.timestamp, str_text, sizeof (str_text));
    WSContentSend_P (PSTR ("<div class='item'>%s</div>\n"), str_text);

    // next periods
    timestamp = CalendarGetNextTimestamp (sensor_histo.graph.period, sensor_histo.graph.timestamp, 1);
    CalendarGetPeriodLabel (sensor_histo.graph.period, timestamp, str_text, sizeof (str_text));
    WSContentSend_P (PSTR ("<a href='%s?next=%u'><div class='item' title='%s'>&gt;</div></a>\n"), PSTR_SENSOR_PAGE_HISTO, 1, str_text);

    timestamp = CalendarGetNextTimestamp (sensor_histo.graph.period, sensor_histo.graph.timestamp, 5);
    CalendarGetPeriodLabel (sensor_histo.graph.period, timestamp, str_text, sizeof (str_text));
    WSContentSend_P (PSTR ("<a href='%s?next=%u'><div class='item' title='%s'>&gt; &gt;</div></a>\n"), PSTR_SENSOR_PAGE_HISTO, 5, str_text);

    WSContentSend_P (PSTR ("</div></div>\n"));        // lvl l2

    // -------------
    //      SVG
    // -------------

    // svg : start
    WSContentSend_P (PSTR ("<div class='graph'>\n"));
    WSContentSend_P (PSTR ("<svg class='graph' viewBox='0 0 %d %d' preserveAspectRatio='none'>\n"), SENSOR_HISTO_WIDTH, SENSOR_HISTO_HEIGHT);

    // svg : temperature gradient
    WSContentSend_P (PSTR ("<defs>\n"));
    WSContentSend_P (PSTR ("<linearGradient id='fade' x1=0%% y1=0%% x2=0%% y2=100%%>\n"));
    WSContentSend_P (PSTR ("<stop offset=0%% stop-color='#f7dc6f' />\n"));
    WSContentSend_P (PSTR ("<stop offset=100%% stop-color='#3498DB' />\n"));
    WSContentSend_P (PSTR ("</linearGradient>\n"));
    WSContentSend_P (PSTR ("</defs>\n"));

    // svg : style
    WSContentSend_P (PSTR ("<style type='text/css'>\n"));

    WSContentSend_P (PSTR ("rect.main {fill:none;stroke:#888;stroke-width:1;}\n"));
    WSContentSend_P (PSTR ("rect.dash {fill:#333;fill-opacity:0.5;stroke-width:0;}\n"));
    WSContentSend_P (PSTR ("line.dash {stroke:grey;stroke-width:1;stroke-dasharray:2 8;}\n"));

    WSContentSend_P (PSTR ("text {fill:white;text-anchor:middle;dominant-baseline:middle;}\n"));
    WSContentSend_P (PSTR ("text.time {font-style:italic;fill-opacity:0.6;}\n"));
    WSContentSend_P (PSTR ("text.temp {fill:#f7dc6f;}\n"));
    WSContentSend_P (PSTR ("text.humi {fill:#3498db;}\n"));

    WSContentSend_P (PSTR ("path.temp {stroke:url('#fade');stroke-width:2;fill:url('#fade');fill-opacity:0.7;}\n"));
    WSContentSend_P (PSTR ("polyline.humi {stroke:#3498db;fill:none;stroke-width:4;}\n"));
    WSContentSend_P (PSTR ("rect.pres {stroke:#73c6b6;fill:#73c6b6;fill-opacity:0.6;}\n"));

    WSContentSend_P (PSTR ("</style>\n"));

    // load data from file
    SensorHistoFileLoadData ();

    // svg : units
    SensorHistoDisplayUnits ();

    // svg : curves
    SensorHistoDisplayCurve ();

    // svg : main frame
    SensorHistoDisplayFrame ();

    // svg : end
    WSContentSend_P (PSTR ("</svg>\n"));
    WSContentSend_P (PSTR ("</div>\n"));

    // --------------------
    //    Data selection
    // --------------------

    WSContentSend_P (PSTR ("<div><div class='lvl l3'>\n"));

    // link to display temperature
    mask = 0x0001;
    display = (sensor_histo.graph.mask & mask);
    if (display) choice = sensor_histo.graph.mask & (mask ^ 0xffff);
      else choice = sensor_histo.graph.mask | mask;
    if (display) strcpy (str_status, ""); else strcpy_P (str_status, PSTR ("disabled"));
    WSContentSend_P (PSTR ("<a href='%s?%s=%u'><div class='item temp %s'>Temperature</div></a>\n"), PSTR_SENSOR_PAGE_HISTO, PSTR (CMND_SENSOR_HISTO_DISPLAY), choice, str_status);

    // link to display humidity
    mask = 0x0002;
    display = (sensor_histo.graph.mask & mask);
    if (display) choice = sensor_histo.graph.mask & (mask ^ 0xffff);
      else choice = sensor_histo.graph.mask | mask;
    if (display) strcpy (str_status, ""); else strcpy_P (str_status, PSTR ("disabled"));
    WSContentSend_P (PSTR ("<a href='%s?%s=%u'><div class='item humi %s'>Humidity</div></a>\n"), PSTR_SENSOR_PAGE_HISTO, PSTR (CMND_SENSOR_HISTO_DISPLAY), choice, str_status);

    // link to display presence
    mask = 0x0004;
    display = (sensor_histo.graph.mask & mask);
    if (display) choice = sensor_histo.graph.mask & (mask ^ 0xffff);
      else choice = sensor_histo.graph.mask | mask;
    if (display) strcpy (str_status, ""); else strcpy_P (str_status, PSTR ("disabled"));
    WSContentSend_P (PSTR ("<a href='%s?%s=%u'><div class='item humi %s'>Presence</div></a>\n"), PSTR_SENSOR_PAGE_HISTO, PSTR (CMND_SENSOR_HISTO_DISPLAY), choice, str_status);

    WSContentSend_P (PSTR ("</div></div>\n"));          // lvl l3

    // ----------------
    //    End of page
    // ----------------

    // main menu button
    WSContentSend_P (PSTR ("<div><form action='/' method='get'><button class='menu'>%s</button></form></div>\n"), D_MAIN_MENU);
  }

  // end of page
  WSContentStop ();

  // log page serving time
  if (!refresh) AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: Graph in %ums"), millis () - timestart);
}


// --------------------
//    Presence Graph
// --------------------
/*

// Presence weekly history page
void SensorWebGraphPresence ()
{
  uint8_t index, period;
  char    str_text[8];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // if target temperature has been changed
  period = 1;
  if (Webserver->hasArg ("period"))
  {
    WebGetArg ("period", str_text, sizeof(str_text));
    period = (uint8_t)atoi (str_text);
  }

  // beginning page without authentification
  WSContentStart_P (PSTR ("Presence history"), false);

  // graph swipe section script
//  SensorWebGraphSwipe ();
  WSContentSend_P (PSTR ("</script>\n"));

  // set page as scalable
  WSContentSend_P (PSTR ("<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=yes'/>\n"));

  // refresh every 10 mn
  WSContentSend_P (PSTR ("<meta http-equiv='refresh' content='%u'/>\n"), 600);

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  SensorWebGraphPresenceStyle ();
  WSContentSend_P (PSTR ("</style>\n"));

  WSContentSend_P (PSTR ("</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  // room name and header
  WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText(SET_DEVICENAME));
  WSContentSend_P (PSTR ("<div class='header'>Presence</div>\n"));

  // periods
  WSContentSend_P (PSTR ("<div class='level1'>\n"));
  WSContentSend_P (PSTR ("<div class='choice'>\n"));
  for (index = 0; index < SENSOR_GRAPH_MAX; index ++)
  {
    GetTextIndexed (str_text, sizeof (str_text), index, kSensorGraphPeriod);
    if (period != index) WSContentSend_P (PSTR ("<a href='%s?period=%u'>"), D_SENSOR_PAGE_PRESENCE, index);
    WSContentSend_P (PSTR ("<div class='item period'>%s</div>"), str_text);
    if (period != index) WSContentSend_P (PSTR ("</a>"));
    WSContentSend_P (PSTR ("\n"));
  }
  WSContentSend_P (PSTR ("</div></div>\n"));        // choice & level1

  // graph
  SensorWebGraphPresenceContent (period);

  // main menu button
  WSContentSend_P (PSTR ("<div><form action='/' method='get'><button class='menu'>%s</button></form></div>\n"), D_MAIN_MENU);

  // end of page
  WSContentStop ();
}

// presence history graph
void SensorWebGraphPresenceContent (const uint8_t period)
{
  uint8_t  slot_state, slot_new;
  uint16_t day_start, slot_start;
  uint16_t nb_line, nb_slot, day, line, slot, index;
  uint16_t last_x, graph_x, graph_y, line_height, bar_height;
  char     str_text[16];
  char     str_style[16];

  switch (period)
  {
    case SENSOR_PERIOD_DAY:
      nb_line = 12;           // 12
      nb_slot = 24;           // 5 mn / 2h
      break;
    
    case SENSOR_PERIOD_WEEK: 
    nb_line = 7;              // 7 days
    nb_slot = 48;             // 30mn / day
    break;
    
    case SENSOR_PERIOD_YEAR:
    nb_line = 12;             // 12 months
    nb_slot = 31;             // day / month
    break;
  }

  // calculate line height and bar height
  line_height = SENSOR_GRAPH_HEIGHT / (nb_line + 1);
  bar_height  = line_height - 5;

  // start of SVG graph
  WSContentSend_P (PSTR ("<div class='graph'>\n"));
  WSContentSend_P (PSTR ("<svg class='graph' viewBox='%d %d %d %d' preserveAspectRatio='none'>\n"), 0, 0, SENSOR_GRAPH_WIDTH, SENSOR_GRAPH_HEIGHT + 1);

  // SVG style
  SensorWebGraphPresenceStyleSVG ();

  // display header
  WSContentSend_P (PSTR ("<text class='time' text-anchor='start' x=%u y=%u>%s</text>\n"), SENSOR_GRAPH_WIDTH_HEAD, bar_height - 10, "0h");
  for (index = 1; index < 6;index ++) WSContentSend_P (PSTR ("<text class='time' text-anchor='middle' x=%u y=%u>%02uh</text>\n"), SENSOR_GRAPH_WIDTH_HEAD + index * SENSOR_GRAPH_WIDTH_LINE / 6, bar_height - 10, index * 4);
  WSContentSend_P (PSTR ("<text class='time' text-anchor='end' x=%u y=%u>%s</text>\n"), SENSOR_GRAPH_WIDTH_HEAD + SENSOR_GRAPH_WIDTH_LINE, bar_height - 10, "24h");

  // loop thru days
  day_start = (uint16_t)(RtcTime.day_of_week + 6) % 7;
  graph_y   = 0;
  for (line = 0; line < 7; line ++)
  {
    // convert to current day
    day = (day_start + line) % 7;
    slot_start = day * nb_slot;

    // get upper line position
    graph_y += line_height;

    // display week day
    index = (day + 1) % 7;
    strlcpy (str_text, kWeekdayNames + index * 3, 4);
    if (line == 6) strcpy_P (str_style, PSTR ("today")); else strcpy_P (str_style, PSTR ("head"));
    WSContentSend_P (PSTR ("<text class='%s' text-anchor='start' x=%u y=%u>%s</text>\n"), str_style, 20, graph_y + bar_height - 25, str_text);

    // loop to display slots of current day
    if (arr_sensor_week[slot_start].event.valid) slot_state = arr_sensor_week[slot_start].event.pres;
      else slot_state = 0;
    last_x = SENSOR_GRAPH_WIDTH_HEAD;
    for (slot = 0; slot < nb_slot; slot ++)
    {
      // read slot status
      index = slot_start + slot;
      if (arr_sensor_week[index].event.valid) slot_new = arr_sensor_week[index].event.pres;
        else slot_new = 0;

      // last cell
      if (slot == nb_slot - 1) slot++;
      if ((slot_state != slot_new) || (slot > nb_slot - 1))
      {
        graph_x = SENSOR_GRAPH_WIDTH_HEAD + SENSOR_GRAPH_WIDTH_LINE * slot / nb_slot;
        WSContentSend_P (PSTR ("<rect class='s%u' x=%u y=%u width=%u height=%u />\n"), slot_state, last_x, graph_y, graph_x - last_x, bar_height);
        last_x     = graph_x;
        slot_state = slot_new;
      }
    }

    // loop to display separation lines every 2h
    last_x = SENSOR_GRAPH_WIDTH_HEAD;
    for (slot = 1; slot < 12; slot++)
    {
      // draw line
      graph_x = SENSOR_GRAPH_WIDTH_HEAD + SENSOR_GRAPH_WIDTH_LINE * slot / 12;
      WSContentSend_P (PSTR ("<line x1=%u y1=%u x2=%u y2=%u />\n"), graph_x, graph_y, graph_x, graph_y + bar_height);
    }
  }

  // ---  End  ---
  WSContentSend_P (PSTR ("</svg>\n"));      // graph
  WSContentSend_P (PSTR ("</div>\n"));      // graph
}

// Presence page style
void SensorWebGraphPresenceStyle ()
{
  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));

  WSContentSend_P (PSTR ("div {margin:0.25rem auto;padding:0.1rem 0px;text-align:center;}\n"));
  WSContentSend_P (PSTR ("div.title {font-size:3.5vh;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.header {font-size:2.5vh;margin:2vh auto;}\n"));

  WSContentSend_P (PSTR ("a {color:white;}\n")); 
  WSContentSend_P (PSTR ("a:link {text-decoration:none;}\n"));

  // banner
//  WSContentSend_P (PSTR ("div.banner {width:88%%;}\n"));
//  WSContentSend_P (PSTR ("div.banner div {display:inline-block;}\n"));
//  WSContentSend_P (PSTR ("div.date {font-size:2.5vh;}\n"));
//  WSContentSend_P (PSTR ("div.prev {float:left;}\n"));
//  WSContentSend_P (PSTR ("div.next {float:right;}\n"));

  // periods
  WSContentSend_P (PSTR ("div.level1 {margin-top:-40px;}\n"));
  WSContentSend_P (PSTR ("div.choice {display:inline-block;padding:0px 2px;margin:0px;background:none;color:#fff;border:1px #666 solid;border-radius:6px;}\n"));
  WSContentSend_P (PSTR ("div.choice div {background:#666;}\n"));
  WSContentSend_P (PSTR ("div.choice a div {background:none;}\n"));

  // main menu button
  WSContentSend_P (PSTR ("button.menu {background:%s;color:%s;line-height:2rem;font-size:1.2rem;cursor:pointer;border:none;border-radius:0.3rem;width:150px;margin:2vh;}\n"), COLOR_BUTTON, COLOR_BUTTON_TEXT);

  WSContentSend_P (PSTR("div.graph {width:90%%;margin:2vh auto;}\n"));
  WSContentSend_P (PSTR("svg.graph {width:100%%;height:70vh;}\n"));
}

// Presence SVG style
void SensorWebGraphPresenceStyleSVG ()
{
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));

  WSContentSend_P (PSTR ("text {font-size:3vh;fill:%s;}\n"), SENSOR_COLOR_TIME);
//  WSContentSend_P (PSTR ("text.time {}\n"));
//  WSContentSend_P (PSTR ("text.head {}\n"));
  WSContentSend_P (PSTR ("text.today {font-weight:bold;}\n"));

  WSContentSend_P (PSTR ("rect {opacity:0.8;}\n"));
  WSContentSend_P (PSTR ("rect.s0 {stroke:none;fill:%s;}\n"), "#333");
  WSContentSend_P (PSTR ("rect.s1 {stroke:none;fill:%s;}\n"), SENSOR_COLOR_PRES);

  WSContentSend_P (PSTR ("line {stroke:#aaa;stroke-dasharray:2 6;}\n"));

  WSContentSend_P (PSTR ("</style>\n"));
}


// Presence yearly history page
void SensorWebGraphYearlyPresenceCurve (const uint16_t year, const char* pstr_url)
{
  uint8_t  slot_state, slot_new;
  uint32_t month, month_start, month_stop, day;
  uint32_t index, count;
  uint32_t last_x, graph_x, graph_y, line_height, bar_height;
  char     str_name[4];
  char     str_text[64];
  String   str_result;

  // check parameters
  if (pstr_url == nullptr) return;

#ifdef USE_UFILESYS
  // start of form
  WSContentSend_P (PSTR("<form method='get' action='/%s'>\n"), pstr_url);
  WSContentSend_P (PSTR("<div class='banner'>\n"));

  // date
  WSContentSend_P (PSTR("<div class='date'>%u</div>\n"), year);

  // if exist, navigation to previous year
  if (SensorFileYearExist (year - 1)) strcpy (str_text, ""); else strcpy_P (str_text, PSTR ("disabled"));
  WSContentSend_P (PSTR("<div class='prev'><button name='year' title='%u' value=%u id='prev' %s>&lt;&lt;</button></div>\n"), year - 1, year - 1, str_text);

  // if exist, navigation to next year
  if (SensorFileYearExist (year + 1)) strcpy (str_text, ""); else strcpy_P (str_text, PSTR ("disabled"));
  WSContentSend_P (PSTR("<div class='next'><button name='year' title='%u' value=%u id='next' %s>&gt;&gt;</button></div>\n"), year + 1, year + 1, str_text);

  // end of form
  WSContentSend_P (PSTR("</div>\n"));     // banner
  WSContentSend_P (PSTR("<form method='get' action='/%s'>\n"), D_SENSOR_PAGE_YEAR_PRESENCE);

  // load data from file
  SensorFileYearLoad (year);

  // set month window
  month_start = 0;
  if (year != RtcTime.year) month_stop = 12;          // previous years
    else month_stop = RtcTime.month;                  // current year
#else
  // set month window
  month_start = RtcTime.month;
  month_stop  = month_start + 12;
#endif    // USE_UFILESYS

  // calculate line height and bar height
  line_height = SENSOR_GRAPH_HEIGHT / 13;
  bar_height  = line_height - 5;

  // start of SVG graph
  WSContentSend_P (PSTR("<div class='graph'>\n"));
  WSContentSend_P (PSTR("<svg class='graph' viewBox='%d %d %d %d' preserveAspectRatio='none'>\n"), 0, 0, SENSOR_GRAPH_WIDTH, SENSOR_GRAPH_HEIGHT + 1);

  // SVG style
  SensorWebGraphPresenceSVGStyle ();

  // display header
  WSContentSend_P (PSTR ("<text class='time' text-anchor='start' x=%u y=%u>%s</text>\n"), SENSOR_GRAPH_WIDTH_HEAD, line_height - 10, "01");
  for (index = 1; index < 7;index ++) WSContentSend_P (PSTR ("<text class='time' text-anchor='middle' x=%u y=%u>%02u</text>\n"), SENSOR_GRAPH_WIDTH_HEAD + 5 * index * SENSOR_GRAPH_WIDTH_LINE / 31, line_height - 10, index * 5);

  // loop thru days
  str_result = "";
  index = 1;
  for (count = month_start; count < month_stop; count ++)
  {
    // convert to current month
    month = count % 12;

    // get upper line position
    graph_y = index * line_height;

    // month name
    strlcpy (str_name, kMonthNames + month * 3, 4);
    sprintf_P (str_text, PSTR ("<text class='head' text-anchor='start' x=%u y=%u>%s</text>\n"), 20, graph_y + bar_height - 15, str_name);
    str_result += str_text;

    // loop to display slots of days
    last_x = SENSOR_GRAPH_WIDTH_HEAD;
    if (sensor_year[month][0].pres > 0) slot_state = 1; else slot_state = 0;
    for (day = 0; day < days_in_month[month]; day ++)
    {
      // read slot status
      if (sensor_year[month][day].pres > 0) slot_new = 1; else slot_new = 0;

      // last cell
      if (day == days_in_month[month] - 1)
      {
        graph_x = SENSOR_GRAPH_WIDTH_HEAD + SENSOR_GRAPH_WIDTH_LINE * (day + 1) / 31;
        sprintf_P (str_text, PSTR ("<rect class='s%u' x=%u y=%u width=%u height=%u />\n"), slot_state, last_x, graph_y, graph_x - last_x, bar_height);
        str_result += str_text;
      }

      // else if presence has changed
      else if (slot_state != slot_new)
      {
        graph_x = SENSOR_GRAPH_WIDTH_HEAD + SENSOR_GRAPH_WIDTH_LINE * day / 31;
        sprintf_P (str_text, PSTR ("<rect class='s%u' x=%u y=%u width=%u height=%u />\n"), slot_state, last_x, graph_y, graph_x - last_x, bar_height);
        str_result += str_text;
        last_x = graph_x;
        slot_state = slot_new;
      }

      // if needed, publish result
      if (str_result.length () > 256) { WSContentSend_P (str_result.c_str ()); str_result = ""; }
    }

    // loop to display separation lines
    for (day = 1; day < 7; day++)
    {
      // draw line
      graph_x = SENSOR_GRAPH_WIDTH_HEAD + SENSOR_GRAPH_WIDTH_LINE * 5 * day / 31;
      sprintf_P (str_text, PSTR ("<line x1=%u y1=%u x2=%u y2=%u />\n"), graph_x, graph_y, graph_x, graph_y + bar_height);
      str_result += str_text;

      // if needed, publish result
      if (str_result.length () > 256) { WSContentSend_P (str_result.c_str ()); str_result = ""; }
    }

    // increment
    index++;
  }

  // publish last part
  if (str_result.length () > 0) WSContentSend_P (str_result.c_str ());

  // ---  End  ---
  WSContentSend_P (PSTR("</svg>\n"));      // graph
  WSContentSend_P (PSTR("</div>\n"));      // graph
}
*/



// --------------------
//    Measure Graph
// --------------------

// Sensor graph style
void SensorWebGraphWeeklyMeasureStyle ()
{
  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));

  WSContentSend_P (PSTR ("a {color:white;}\n"));
  WSContentSend_P (PSTR ("a:link {text-decoration:none;}\n"));

  WSContentSend_P (PSTR ("div {padding:0px;margin:0px;text-align:center;}\n"));
  WSContentSend_P (PSTR ("div.title {font-size:4vh;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.header {font-size:3vh;margin:1vh auto;}\n"));

  WSContentSend_P (PSTR ("div.value {font-size:4vh;padding-bottom:1vh;}\n"));
  WSContentSend_P (PSTR ("div.value span {margin:0px 2vw;}\n"));
  WSContentSend_P (PSTR ("div.value span#humi {color:%s;}\n"), SENSOR_COLOR_HUMI);
  WSContentSend_P (PSTR ("div.value span#temp {color:%s;}\n"), SENSOR_COLOR_TEMP);

  WSContentSend_P (PSTR ("button {padding:2px 12px;font-size:2.5vh;background:none;color:#fff;border:1px #666 solid;border-radius:6px;}\n"));
  WSContentSend_P (PSTR ("button:hover {background:#aaa;}\n"));
  WSContentSend_P (PSTR ("button:disabled {color:#252525;background:#252525;border:1px #252525 solid;}\n"));

  WSContentSend_P (PSTR ("div.banner {width:88%%;margin:auto;}\n"));
  WSContentSend_P (PSTR ("div.banner div {display:inline-block;}\n"));
  WSContentSend_P (PSTR ("div.date {font-size:3vh;}\n"));
  WSContentSend_P (PSTR ("div.prev {float:left;}\n"));
  WSContentSend_P (PSTR ("div.next {float:right;}\n"));

  // main menu button
  WSContentSend_P (PSTR ("button.menu {background:%s;color:%s;line-height:2rem;font-size:1.2rem;cursor:pointer;border:none;border-radius:0.3rem;width:150px;margin:2vh;}\n"), COLOR_BUTTON, COLOR_BUTTON_TEXT);

  WSContentSend_P (PSTR ("div.graph {width:100%%;margin:1vh auto;}\n"));
  WSContentSend_P (PSTR ("svg.graph {width:100%%;height:50vh;}\n"));
}

/*
// Sensor graph curve display
void SensorWebGraphWeeklyMeasureCurve (const uint8_t week, const char* pstr_url)
{
//  bool     data_ok = true;
  uint8_t  day, hour, slot, value;
  uint8_t  count, count_start, count_stop;
  uint8_t  prev_week, next_week;
  int16_t  scale, temp_range, temp_incr, temp_min, temp_max;
  uint32_t index, unit, graph_width, graph_x, prev_x, graph_y, prev_y, last_x;
  char     str_text[16];
  char     str_type[16];

  // check parameters
  if (pstr_url == nullptr) return;

#ifdef USE_UFILESYS
  // set display sequence
  count_start = 1;
  count_stop  = 8;

  // load weekly data
  SensorFileWeekLoad (week);
#else
  // set display sequence
  count_start = RtcTime.day_of_week;
  count_stop  = count_start + 7;
#endif      // USE_UFILESYS

  // boundaries of SVG graph
  last_x = SENSOR_GRAPH_START;
  graph_width = SENSOR_GRAPH_STOP - SENSOR_GRAPH_START;

  // loop to calculate minimum and maximum temperature for the graph
  if (sensor_config.weekly_temp)
  {
    temp_min = temp_max = INT16_MAX;
    for (day = 0; day < 7; day++)
      for (hour = 0; hour < 24; hour++)
        for (slot = 0; slot < 6; slot++)
        {
          // minimum temperature
          if (temp_min == INT16_MAX) temp_min = sensor_week[day][hour][slot].temp;
            else if (sensor_week[day][hour][slot].temp != INT16_MAX) temp_min = min (temp_min, sensor_week[day][hour][slot].temp);

          // maximum temperature
          if (temp_max == INT16_MAX) temp_max = sensor_week[day][hour][slot].temp;
            else if (sensor_week[day][hour][slot].temp != INT16_MAX) temp_max = max (temp_max, sensor_week[day][hour][slot].temp);
        }

    // set upper and lower range according to min and max temperature
    if (temp_min != INT16_MAX) temp_min = (temp_min / 10 - 2) * 10;
      else temp_min = 180; 
    if (temp_max != INT16_MAX) temp_max = (temp_max / 10 + 2) * 10;
      else temp_max = 220;
    if (temp_max < temp_min + 20) temp_max = temp_min + 20;

    // calculate temperature range and increment
    temp_range = temp_max - temp_min;
    temp_incr  = max (temp_range / 40 * 10, 10);
  }

#ifdef USE_UFILESYS
 
  // calculate previous and next week
  if (week > 0) next_week = week - 1; else next_week = UINT8_MAX;
  prev_week = week + 1;

  // start of form
  WSContentSend_P (PSTR ("<form method='get' action='/%s'>\n"), pstr_url);
  WSContentSend_P (PSTR ("<div class='banner'>\n"));

  // date
  SensorGetWeekLabel (week, str_text, sizeof (str_text));
  WSContentSend_P (PSTR ("<div class='date'>%s</div>\n"), str_text);

  // if exist, navigation to previous week
  SensorGetWeekLabel (prev_week, str_text, sizeof (str_text));
  if (SensorFileWeekExist (prev_week)) strcpy (str_type, ""); else strcpy_P (str_type, PSTR ("disabled"));
  WSContentSend_P (PSTR ("<div class='prev'><button name='week' title='%s' value=%u id='prev' %s>&lt;&lt;</button></div>\n"), str_text, prev_week, str_type);

  // if exist, navigation to next week or to live values
  SensorGetWeekLabel (next_week, str_text, sizeof (str_text));
  if (SensorFileWeekExist (next_week)) strcpy (str_type, ""); else strcpy_P (str_type, PSTR ("disabled"));
  WSContentSend_P (PSTR ("<div class='next'><button name='week' title='%s' value=%u id='next' %s>&gt;&gt;</button></div>\n"), str_text, next_week, str_type);

  // end of form
  WSContentSend_P (PSTR ("</div>\n"));      // banner
  WSContentSend_P (PSTR ("</form>\n"));     // get

#endif    // USE_UFILESYS

  // start of SVG graph
  WSContentSend_P (PSTR ("<div class='graph'>\n"));
  WSContentSend_P (PSTR ("<svg class='graph' viewBox='%d %d %d %d' preserveAspectRatio='none'>\n"), 0, 0, SENSOR_GRAPH_WIDTH, SENSOR_GRAPH_HEIGHT + 1);

  // SVG style
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));

  WSContentSend_P (PSTR ("text {font-size:18px}\n"));

  WSContentSend_P (PSTR ("polyline.temp {stroke:%s;fill:none;stroke-width:2;}\n"), SENSOR_COLOR_TEMP);
  WSContentSend_P (PSTR ("path.humi {stroke:%s;fill:%s;opacity:1;fill-opacity:0.5;}\n"), SENSOR_COLOR_HUMI, SENSOR_COLOR_HUMI);

  WSContentSend_P (PSTR ("rect.frame {fill:none;stroke:#aaa;}\n"));
  WSContentSend_P (PSTR ("rect.none {fill:%s;opacity:0.8;}\n"), SENSOR_COLOR_NONE);
  WSContentSend_P (PSTR ("rect.acti {fill:%s;opacity:0.8;}\n"), SENSOR_COLOR_ACTI);
  WSContentSend_P (PSTR ("rect.inac {fill:%s;opacity:0.8;}\n"), SENSOR_COLOR_INAC);

  WSContentSend_P (PSTR ("line {stroke-dasharray:1 2;stroke-width:0.5;}\n"));
  WSContentSend_P (PSTR ("line.time {stroke:%s;}\n"), SENSOR_COLOR_LINE);
  WSContentSend_P (PSTR ("line.temp {stroke:%s;}\n"), SENSOR_COLOR_TEMP);
  WSContentSend_P (PSTR ("line.humi {stroke:%s;}\n"), SENSOR_COLOR_HUMI);

  WSContentSend_P (PSTR ("text.day {fill:%s;font-size:16px;}\n"), SENSOR_COLOR_TIME);
  WSContentSend_P (PSTR ("text.today {fill:%s;font-size:16px;font-weight:bold;}\n"), SENSOR_COLOR_TODAY);
  WSContentSend_P (PSTR ("text.temp {fill:%s;}\n"), SENSOR_COLOR_TEMP);
  WSContentSend_P (PSTR ("text.humi {fill:%s;}\n"), SENSOR_COLOR_HUMI);

  WSContentSend_P (PSTR ("</style>\n"));

  // ------  Humidity  ---------

  if (sensor_config.weekly_humi)
  {
    // init
    index   = 0;
    graph_x = prev_x = SENSOR_GRAPH_START;
    graph_y = prev_y = UINT32_MAX;

    // loop thru points
    for (count = count_start; count < count_stop; count++)
    {
      day = count % 7;
      for (hour = 0; hour < 24; hour++)
        for (slot = 0; slot < 6; slot++)
        {
          // calculate x coordinate
          graph_x = SENSOR_GRAPH_START + (index * graph_width / 1008);

          // calculate y coordinate
          if (sensor_week[day][hour][slot].humi != INT8_MAX) graph_y = SENSOR_GRAPH_HEIGHT - SENSOR_GRAPH_HEIGHT * (uint32_t)sensor_week[day][hour][slot].humi / 100;
            else graph_y = UINT32_MAX;

          // if needed, start path
          if ((graph_y != UINT32_MAX) && (prev_y == UINT32_MAX))  WSContentSend_P (PSTR ("<path class='humi' d='M%d %d "), graph_x, SENSOR_GRAPH_HEIGHT);

          // else if needed, stop path
          else if ((graph_y == UINT32_MAX) && (prev_y != UINT32_MAX)) WSContentSend_P (PSTR ("L%d %d '/>\n"), prev_x, SENSOR_GRAPH_HEIGHT);

          // if needed, draw point
          else if (graph_y != UINT32_MAX) WSContentSend_P (PSTR ("L%u %u "), graph_x, graph_y);

          // save previous values
          if (graph_y != UINT32_MAX) last_x = max (last_x, graph_x);
          prev_x = graph_x;
          prev_y = graph_y;
          index++;
        }
    }

    // end of graph
    if (graph_y != UINT32_MAX) WSContentSend_P (PSTR ("L%u %u '/>\n"), graph_x, SENSOR_GRAPH_HEIGHT);
  }

  // ---------  Temperature  ---------

  if (sensor_config.weekly_temp)
  {
    // init
    index   = 0;
    graph_x = prev_x = SENSOR_GRAPH_START;
    graph_y = prev_y = UINT32_MAX;

    // loop thru points
    for (count = count_start; count < count_stop; count++)
    {
      day = count % 7;
      for (hour = 0; hour < 24; hour++)
        for (slot = 0; slot < 6; slot++)
        {
          // calculate x coordinate
          graph_x = SENSOR_GRAPH_START + (index * graph_width / 1008);

          // calculate y coordinate
          if (sensor_week[day][hour][slot].temp == INT16_MAX) graph_y = UINT32_MAX;
            else graph_y = SENSOR_GRAPH_HEIGHT - SENSOR_GRAPH_HEIGHT * (uint32_t)(sensor_week[day][hour][slot].temp - temp_min) / (uint32_t)temp_range;

          // if needed, start polyline
          if ((graph_y != UINT32_MAX) && (prev_y == UINT32_MAX)) WSContentSend_P (PSTR ("<polyline class='temp' points='"));

          // else if needed, stop polyline
          else if ((graph_y == UINT32_MAX) && (prev_y != UINT32_MAX)) WSContentSend_P (PSTR ("'/>\n"));

          // if needed, draw point
          if (graph_y != UINT32_MAX) WSContentSend_P (PSTR ("%u,%u "), graph_x, graph_y);

          // save previous values
          if (graph_y != UINT32_MAX) last_x = max (last_x, graph_x);

          // increment
          prev_x = graph_x;
          prev_y = graph_y;
          index++;
        }
    }

    if (graph_y != UINT32_MAX) WSContentSend_P (PSTR ("'/>\n"));
  }

  // ---------  Activity  ---------

  if (sensor_config.weekly_acti)
  {
    // init
    index   = 0;
    graph_x = prev_x = UINT32_MAX;
    graph_y = prev_y = SENSOR_GRAPH_HEIGHT - SENSOR_GRAPH_ACTI;

    // loop thru points
    for (count = count_start; count < count_stop; count++)
    {
      day = count % 7;
      for (hour = 0; hour < 24; hour++)
        for (slot = 0; slot < 6; slot++)
        {
          // read slot status
          value = sensor_week[day][hour][slot].event.acti;

          // start and end of activity
          if ((prev_x == UINT32_MAX) && (value == 1)) prev_x = SENSOR_GRAPH_START + index * graph_width / 1008;
            else if ((prev_x != UINT32_MAX) && (value == 0)) graph_x = SENSOR_GRAPH_START + index * graph_width / 1008;

          // if activity should be drawn
          if ((prev_x != UINT32_MAX) && (graph_x != UINT32_MAX))
          {
            WSContentSend_P (PSTR ("<rect class='acti' x=%d y=%d width=%d height=%d />\n"), prev_x, graph_y, graph_x - prev_x, SENSOR_GRAPH_ACTI);
            prev_x = graph_x = UINT32_MAX;
          }

          // increase index
          index++;
        } 
    }

    if (prev_x != UINT32_MAX) WSContentSend_P (PSTR ("<rect class='acti' x=%d y=%d width=%d height=%d />\n"), prev_x, graph_y, graph_x - prev_x, SENSOR_GRAPH_ACTI);
  }

  // ---------  Inactivity  ---------

  if (sensor_config.weekly_inac)
  {
    // init
    index   = 1;
    graph_x = prev_x = UINT32_MAX;
    graph_y = prev_y = SENSOR_GRAPH_HEIGHT - SENSOR_GRAPH_INAC;

    // loop thru points
    for (count = count_start; count < count_stop; count++)
    {
      day = count % 7;
      for (hour = 0; hour < 24; hour++)
        for (slot = 0; slot < 6; slot++)
        {
          // read slot status
          value = sensor_week[day][hour][slot].event.inac;

          // start and end of activity
          if ((prev_x == UINT32_MAX) && (value == 1)) prev_x = SENSOR_GRAPH_START + (index * graph_width / 1008);
            else if ((prev_x != UINT32_MAX) && (value != 1)) graph_x = SENSOR_GRAPH_START + (index - 1) * graph_width / 1008;

          // if activity should be drawn
          if ((prev_x != UINT32_MAX) && (graph_x != UINT32_MAX))
          {
            WSContentSend_P (PSTR ("<rect class='inac' x=%d y=%d width=%d height=%d />\n"), prev_x, graph_y, graph_x - prev_x, SENSOR_GRAPH_INAC);
            prev_x = graph_x = UINT32_MAX;
          }

          // increase index
          index++;
        } 
    }

    if (prev_x != UINT32_MAX) WSContentSend_P (PSTR ("<rect class='inac' x=%d y=%d width=%d height=%d />\n"), prev_x, graph_y, graph_x - prev_x, SENSOR_GRAPH_INAC);
  }

  // -------  Frame  -------

  WSContentSend_P (PSTR ("<rect class='frame' x=%u y=%u width=%u height=%u rx=4 />\n"), SENSOR_GRAPH_START, 0, SENSOR_GRAPH_STOP - SENSOR_GRAPH_START, SENSOR_GRAPH_HEIGHT + 1);

  // -------  Time line  --------

  // display week days
  for (count = count_start; count < count_stop; count++)
  {
    // get day label
    day = count % 7;
    strlcpy (str_text, kWeekdayNames + day * 3, 4);

    // display day
    graph_x = SENSOR_GRAPH_START + ((count - count_start) * 2 + 1) * graph_width / 14 - 10;
    WSContentSend_P (PSTR ("<text class='day' x=%u y=%u>%s</text>\n"), graph_x, 20, str_text);
  }

  // display separation lines
  for (index = 1; index < 7; index++)
  {
    graph_x = SENSOR_GRAPH_START + index * graph_width / 7;
    WSContentSend_P (PSTR ("<line class='time' x1=%u y1=%u x2=%u y2=%u />\n"), graph_x, 5, graph_x, SENSOR_GRAPH_HEIGHT - 5);
  }

  // -------  Units  -------

  // temperature units
  if (sensor_config.weekly_temp)
  {
    unit = max (temp_range / temp_incr + 1, 2);
    for (index = 0; index < unit; index ++)
    {
      // calculate temperature scale
      scale = temp_min + temp_incr * (int16_t)index;
      scale = min (scale, temp_max);

      // calculate position
      if (index == unit - 1) graph_y = 4 * SENSOR_GRAPH_HEIGHT / 100;
        else graph_y = SENSOR_GRAPH_HEIGHT - SENSOR_GRAPH_HEIGHT * index / (unit - 1);

      // value and line
      WSContentSend_P (PSTR ("<text class='temp' x=%u%% y=%u>%d °C</text>\n"), 0, graph_y, scale / 10);
      if ((index > 0) && (index < unit - 1)) WSContentSend_P (PSTR ("<line class='temp' x1=%u y1=%u x2=%u y2=%u />\n"), SENSOR_GRAPH_START, graph_y, SENSOR_GRAPH_STOP, graph_y);
    }
  }

  // humidity units
  if (sensor_config.weekly_humi)
  {
    for (index = 0; index < 5; index ++)
    {
      // calculate humidity scale
      scale = 25 * index;

      // calculate position
      if (index == 4) graph_y = 4 * SENSOR_GRAPH_HEIGHT / 100;
        else graph_y = SENSOR_GRAPH_HEIGHT - SENSOR_GRAPH_HEIGHT * index / 4;

      WSContentSend_P (PSTR ("<text class='humi' x=%u%% y=%u>%u %%</text>\n"), SENSOR_GRAPH_STOP_PERCENT + 1, graph_y, scale);
      if ((index > 0) && (index < 4)) WSContentSend_P (PSTR ("<line class='humi' x1=%u y1=%u x2=%u y2=%u />\n"), SENSOR_GRAPH_START, graph_y, SENSOR_GRAPH_STOP, graph_y);
    }
  }

  // --------  End  ---------

  WSContentSend_P (PSTR ("</svg>\n"));      // graph
  WSContentSend_P (PSTR ("</div>\n"));      // graph

}

// Load data for graph display
void SensorWebLoadData ()
{
  uint8_t  week = UINT8_MAX;
  uint16_t year = UINT16_MAX;
  char     str_value[8];

  // init graph data
  SensorInitGraph ();
  
  // check for year parameter
  if (Webserver->hasArg ("year"))
  {
    WebGetArg ("year", str_value, sizeof(str_value));
    if (strlen(str_value) > 0) year = (uint16_t)atoi (str_value);
  }

  // check for week parameter
  if (Webserver->hasArg ("week"))
  {
    WebGetArg ("week", str_value, sizeof(str_value));
    if (strlen(str_value) > 0) week = (uint8_t)atoi (str_value);
  }

#ifdef USE_UFILESYS
  // load graph data from FS
  SensorFileLoad (year, week);

#else
  // data are not coming from FS
  sensor_graph.file = 0;
  sensor_graph.week = week;
  sensor_graph.year = year;

  // load last week data
  if (year == UINT16_MAX)
  {
    if (sensor_config.yearly_temp) SensorHumidityLoadYearFromSettings ();
    if (sensor_config.yearly_pres) SensorPresenceLoadYearFromSettings ();
  }

  // load last year data
  else
  {
    if (sensor_config.weekly_temp) SensorTemperatureLoadWeekFromSettings ();
    if (sensor_config.weekly_humi) SensorHumidityLoadWeekFromSettings ();
    if (sensor_config.weekly_pres) SensorPresenceLoadWeekFromSettings ();
    if (sensor_config.weekly_acti) SensorActivityLoadWeekFromSettings ();
    if (sensor_config.weekly_inac) SensorInactivityLoadWeekFromSettings ();
  }

#endif      // USE_UFILESYS
}

/*
// Temperature and humidity weekly page
void SensorWebWeeklyMeasure ()
{
  uint8_t week;
  char    str_value[4];

  // load data for graph display
  SensorWebLoadData ();

  // set page label
  WSContentStart_P ("Weekly Measure", false);

  // graph swipe section script
  SensorWebGraphSwipe ();
  WSContentSend_P (PSTR ("</script>\n"));

  // set page as scalable
  WSContentSend_P (PSTR ("<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=yes'/>\n"));

  // refresh every 5 mn
  WSContentSend_P (PSTR ("<meta http-equiv='refresh' content='%u'/>\n"), 300);

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  SensorWebGraphWeeklyMeasureStyle ();
  WSContentSend_P (PSTR ("</style>\n"));

  WSContentSend_P (PSTR ("</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  // room name
  WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText(SET_DEVICENAME));

  // header
  WSContentSend_P (PSTR ("<div class='header'>Weekly Measure</div>\n"));

  // ------- Values --------

  WSContentSend_P (PSTR ("<div class='value'><a href='/'>"));

  // temperature
  if (sensor_config.weekly_temp)
    if (sensor_status.temp.value != INT16_MAX) WSContentSend_P (PSTR ("<span id='temp'>%d.%d °C</span>"), sensor_status.temp.value / 10, sensor_status.temp.value % 10);
      else WSContentSend_P (PSTR ("<span id='temp'>--- °C</span>"));

  // humidity
  if (sensor_config.weekly_humi)
    if (sensor_status.humi.value != INT16_MAX) WSContentSend_P (PSTR ("<span id='humi'>%d %%</span>"), sensor_status.humi.value);
      else WSContentSend_P (PSTR ("<span id='humi'>--- %%</span>"));

  WSContentSend_P (PSTR ("</a></div>\n"));

  // graph
  SensorWebGraphWeeklyMeasureCurve (week, D_SENSOR_PAGE_WEEK_MEASURE);

  // main menu button
  WSContentSend_P (PSTR ("<div><form action='/' method='get'><button class='menu'>%s</button></form></div>\n"), D_MAIN_MENU);

  // end of page
  WSContentStop ();
}
*/

// Sensor graph style
void SensorWebGraphYearlyMeasureStyle ()
{
  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));

  WSContentSend_P (PSTR ("div {margin:0.25rem auto;padding:0.1rem 0px;text-align:center;}\n"));
  WSContentSend_P (PSTR ("div.title {font-size:3.5vh;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.header {font-size:2.5vh;margin:2vh auto;}\n"));

  WSContentSend_P (PSTR ("a {color:white;}\n"));
  WSContentSend_P (PSTR ("a:link {text-decoration:none;}\n"));

  WSContentSend_P (PSTR ("span {font-size:4vh;padding:0vh 3vh;}\n"));
  WSContentSend_P (PSTR ("span#temp {color:%s;}\n"), SENSOR_COLOR_TEMP);

  // navigation buttons
  WSContentSend_P (PSTR ("button {padding:2px 12px;font-size:2.5vh;background:none;color:#fff;border:1px #666 solid;border-radius:6px;}\n"));
  WSContentSend_P (PSTR ("button:hover {background:#aaa;}\n"));
  WSContentSend_P (PSTR ("button:disabled {color:#252525;background:#252525;border:1px #252525 solid;}\n"));

  // banner
  WSContentSend_P (PSTR ("div.banner {width:88%%;margin:auto;}\n"));
  WSContentSend_P (PSTR ("div.banner div {display:inline-block;}\n"));
  WSContentSend_P (PSTR ("div.date {font-size:3vh;}\n"));
  WSContentSend_P (PSTR ("div.prev {float:left;}\n"));
  WSContentSend_P (PSTR ("div.next {float:right;}\n"));

  // main menu button
  WSContentSend_P (PSTR ("button.menu {background:%s;color:%s;line-height:2rem;font-size:1.2rem;cursor:pointer;border:none;border-radius:0.3rem;width:150px;margin:2vh;}\n"), COLOR_BUTTON, COLOR_BUTTON_TEXT);

  WSContentSend_P (PSTR("div.graph {width:100%%;margin:1vh auto;}\n"));
  WSContentSend_P (PSTR("svg.graph {width:100%%;height:50vh;}\n"));
}

/*
// Sensor year graph curve display
void SensorWebGraphYearlyMeasureCurve (const uint16_t year, const char* pstr_url)
{
  uint8_t  month, month_start, month_stop, day, counter;
  int16_t  temp_min, temp_max, temp_range, temp_incr, scale;
  uint32_t index, unit, graph_width, graph_x, prev_x, graph_y, prev_y;
  char     str_text[16];

#ifdef USE_UFILESYS

  // load data from file
  SensorFileYearLoad (year);

  // start of form
  WSContentSend_P (PSTR ("<form method='get' action='/%s'>\n"), pstr_url);
  WSContentSend_P (PSTR ("<div class='banner'>\n"));

  // date
  WSContentSend_P (PSTR ("<div class='date'>%u</div>\n"), year);

  // if exist, navigation to previous year
  if (SensorFileYearExist (year - 1)) strcpy (str_text, ""); else strcpy_P (str_text, PSTR ("disabled"));
  WSContentSend_P (PSTR ("<div class='prev'><button name='year' title='%u' value=%u id='prev' %s>&lt;&lt;</button></div>\n"), year - 1, year - 1, str_text);

  // if exist, navigation to next year
  if (SensorFileYearExist (year + 1)) strcpy (str_text, ""); else strcpy_P (str_text, PSTR ("disabled"));
  WSContentSend_P (PSTR ("<div class='next'><button name='year' title='%u' value=%u id='next' %s>&gt;&gt;</button></div>\n"), year + 1, year + 1, str_text);

  // end of form
  WSContentSend_P (PSTR ("</div>\n"));      // banner
  WSContentSend_P (PSTR ("</form>\n"));     // get

  // set month window
  month_start = 0;
  month_stop  = 12;

#else

  // set month window
  month_start = RtcTime.month;
  month_stop  = month_start + 12;

#endif    // USE_UFILESYS

  // boundaries of SVG graph
  graph_width = SENSOR_GRAPH_STOP - SENSOR_GRAPH_START;

  // loop to calculate minimum and maximum temperature for the graph
  temp_min = temp_max = INT16_MAX;
  for (month = 0; month < 12; month ++)
    for (day = 0; day < 31; day++)
    {
      // minimum temperature
      if (temp_min == INT16_MAX) temp_min = sensor_year[month][day].temp_min;
        else if (sensor_year[month][day].temp_min != INT16_MAX) temp_min = min (temp_min, sensor_year[month][day].temp_min);

      // maximum temperature
      if (temp_max == INT16_MAX) temp_max = sensor_year[month][day].temp_max;
        else if (sensor_year[month][day].temp_max != INT16_MAX) temp_max = max (temp_max, sensor_year[month][day].temp_max);
    }

  // set upper and lower range according to min and max temperature
  if (temp_min != INT16_MAX) temp_min = (temp_min / 10 - 2) * 10;
    else temp_min = 180;
  if (temp_max != INT16_MAX) temp_max = (temp_max / 10 + 2) * 10;
    else temp_max = 220;
  if (temp_max < temp_min + 20) temp_max = temp_min + 20;

  // calculate temperature range and increment
  temp_range = temp_max - temp_min;
  temp_incr  = max (temp_range / 40 * 10, 10);

  // start of SVG graph
  WSContentSend_P (PSTR ("<div class='graph'>\n"));
  WSContentSend_P (PSTR ("<svg class='graph' viewBox='%d %d %d %d' preserveAspectRatio='none'>\n"), 0, 0, SENSOR_GRAPH_WIDTH, SENSOR_GRAPH_HEIGHT + 1);

  // SVG style
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));

  WSContentSend_P (PSTR ("text {font-size:18px}\n"));

  WSContentSend_P (PSTR ("rect {opacity:0.8;}\n"));
  WSContentSend_P (PSTR ("rect.frame {stroke:white;fill:none;}\n"));

  WSContentSend_P (PSTR ("polyline {fill:none;}\n"));
  WSContentSend_P (PSTR ("polyline.max {stroke-width:2;stroke:%s;}\n"), SENSOR_COLOR_TEMP_MAX);
  WSContentSend_P (PSTR ("polyline.min {stroke-width:1;stroke:%s;}\n"), SENSOR_COLOR_TEMP_MIN);

  WSContentSend_P (PSTR ("line {stroke-dasharray:1 2;stroke-width:0.5;}\n"));
  WSContentSend_P (PSTR ("line.time {stroke:%s;}\n"), SENSOR_COLOR_LINE);
  WSContentSend_P (PSTR ("line.temp {stroke:%s;}\n"), SENSOR_COLOR_TEMP);

  WSContentSend_P (PSTR ("text.month {fill:%s;font-size:16px;}\n"), SENSOR_COLOR_TIME);
  WSContentSend_P (PSTR ("text.temp {fill:%s;}\n"), SENSOR_COLOR_TEMP);

  WSContentSend_P (PSTR ("</style>\n"));

  // --- Maximum temperature curve ---

  // loop thru months and days
  graph_x = prev_x = UINT32_MAX;
  graph_y = prev_y = UINT32_MAX;
  index = 0;
  for (counter = month_start; counter < month_stop; counter ++)
  {
    // loop thru days
    month = counter % 12;
    for (day = 0; day < days_in_month[month]; day ++)
    {
      // calculate x coordinate
      graph_x = SENSOR_GRAPH_START + (index * graph_width / 366);

      // calculate y coordinate
      if (sensor_year[month][day].temp_max == INT16_MAX) graph_y = UINT32_MAX;
        else graph_y = SENSOR_GRAPH_HEIGHT - SENSOR_GRAPH_HEIGHT * (uint32_t)(sensor_year[month][day].temp_max - temp_min) / (uint32_t)temp_range;

      // if curve restart from beginning
      if ((graph_x < prev_x) && (prev_x != UINT32_MAX)) WSContentSend_P (PSTR ("'/>\n<polyline class='max' points='"));

      // if needed, start polyline
      else if ((graph_y != UINT32_MAX) && (prev_y == UINT32_MAX)) WSContentSend_P (PSTR ("<polyline class='max' points='"));

      // else if needed, stop polyline
      else if ((graph_y == UINT32_MAX) && (prev_y != UINT32_MAX)) WSContentSend_P (PSTR ("'/>\n"));

      // if needed, draw point
      if (graph_y != UINT32_MAX) WSContentSend_P (PSTR ("%u,%u "), graph_x, graph_y);

      // save previous values
      prev_x = graph_x;
      prev_y = graph_y;
      index ++;
    }
  }
  if (graph_y != UINT32_MAX) WSContentSend_P (PSTR ("'/>\n"));

  // ---  Minimum temperature curve  ---

  // loop thru months and days
  graph_x = prev_x = UINT32_MAX;
  graph_y = prev_y = UINT32_MAX;
  index = 0;
  for (counter = month_start; counter < month_stop; counter ++)
  {
    // loop thru days
    month = counter % 12;
    for (day = 0; day < days_in_month[month]; day ++)
    {
      // calculate day in array and x coordinate
      graph_x = SENSOR_GRAPH_START + (index * graph_width / 366);

      // calculate y coordinate
      if (sensor_year[month][day].temp_min == INT16_MAX) graph_y = UINT32_MAX;
        else graph_y = SENSOR_GRAPH_HEIGHT - SENSOR_GRAPH_HEIGHT * (uint32_t)(sensor_year[month][day].temp_min - temp_min) / (uint32_t)temp_range;

      // if curve restart from beginning
      if ((graph_x < prev_x) && (prev_x != UINT32_MAX)) WSContentSend_P (PSTR ("'/>\n<polyline class='min' points='"));

      // if needed, start polyline
      else if ((graph_y != UINT32_MAX) && (prev_y == UINT32_MAX)) WSContentSend_P (PSTR ("<polyline class='min' points='"));

      // else if needed, stop polyline
      else if ((graph_y == UINT32_MAX) && (prev_y != UINT32_MAX)) WSContentSend_P (PSTR ("'/>\n"));

      // if needed, draw point
      if (graph_y != UINT32_MAX) WSContentSend_P (PSTR ("%u,%u "), graph_x, graph_y);

      // save previous values
      prev_x = graph_x;
      prev_y = graph_y;
      index ++;
    }
  }
  if (graph_y != UINT32_MAX) WSContentSend_P (PSTR ("'/>\n"));

  // ---  Frame  ---

  WSContentSend_P (PSTR ("<rect class='frame' x=%u y=%u width=%u height=%u rx=4 />\n"), SENSOR_GRAPH_START, 0, SENSOR_GRAPH_STOP - SENSOR_GRAPH_START, SENSOR_GRAPH_HEIGHT + 1);

  // ---  Time line  ---

  // display month names
  index = 15;
  for (counter = month_start; counter < month_stop; counter ++)
  {
    // get month name
    month = counter % 12;
    strlcpy (str_text, kMonthNames + month * 3, 4);

    // display month
    graph_x = SENSOR_GRAPH_START + (index * graph_width / 366);
    WSContentSend_P (PSTR ("<text class='month' text-anchor='middle' x=%u y=%u>%s</text>\n"), graph_x, 20, str_text);

    // increment to next position
    index += days_in_month[month];
  }

  // ---  Units  ---

  unit = max (temp_range / temp_incr + 1, 2);
  for (index = 0; index < unit; index ++)
  {
    // calculate temperature scale
    scale = temp_min + temp_incr * (int16_t)index;
    scale = min (scale, temp_max);

    // calculate position
    if (index == unit - 1) graph_y = 4 * SENSOR_GRAPH_HEIGHT / 100;
      else graph_y = SENSOR_GRAPH_HEIGHT - SENSOR_GRAPH_HEIGHT * index / (unit - 1);

    // value and line
    WSContentSend_P (PSTR ("<text class='temp' x=%u%% y=%u>%d °C</text>\n"), 0, graph_y, scale / 10);
    if ((index > 0) && (index < unit - 1)) WSContentSend_P (PSTR ("<line class='temp' x1=%u y1=%u x2=%u y2=%u />\n"), SENSOR_GRAPH_START, graph_y, SENSOR_GRAPH_STOP, graph_y);
  }

  // ---  End  ---

  WSContentSend_P(PSTR ("</svg>\n"));      // graph
  WSContentSend_P(PSTR ("</div>\n"));      // graph
}
*/

/*
// Temperature yearly page
void SensorWebYearlyMeasure ()
{
  uint16_t year;
  char     str_value[8];

  // if target temperature has been changed
  if (Webserver->hasArg ("year"))
  {
    WebGetArg ("year", str_value, sizeof(str_value));
    if (strlen(str_value) > 0) year = atoi (str_value);
  }
  else year = RtcTime.year;

  // set page label
  WSContentStart_P (D_SENSOR_TEMPERATURE, false);
  WSContentSend_P (PSTR ("</script>\n"));

  // set page as scalable
  WSContentSend_P (PSTR ("<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=yes'/>\n"));

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  SensorWebGraphYearlyMeasureStyle ();
  WSContentSend_P (PSTR ("</style>\n"));

  WSContentSend_P (PSTR ("</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  // room name and header
  WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText(SET_DEVICENAME));
  WSContentSend_P (PSTR ("<div class='header'>Yearly %s</div>\n"), D_SENSOR_TEMPERATURE);

  // ------- Values --------

  // temperature
  WSContentSend_P (PSTR ("<div class='value'><a href='/'>"));
  if ((sensor_status.temp.source != SENSOR_SOURCE_NONE) && (sensor_status.temp.value != INT16_MAX)) WSContentSend_P (PSTR ("<span id='temp'>%d.%d °C</span>"), sensor_status.temp.value / 10, sensor_status.temp.value % 10);
  WSContentSend_P (PSTR ("</a></div>\n"));

  // ------- Graph --------

  SensorWebGraphYearlyMeasureCurve (year, D_SENSOR_PAGE_YEAR_MEASURE);

  // main menu button
  WSContentSend_P (PSTR ("<div><form action='/' method='get'><button class='menu'>%s</button></form></div>\n"), D_MAIN_MENU);

  // end of page
  WSContentStop ();
}
*/

// Sensor graph style
void SensorWebGraphSwipe ()
{
  // javascript : screen swipe for previous and next period
  WSContentSend_P (PSTR ("\nlet startX=0;let stopX=0;\n"));
  WSContentSend_P (PSTR ("window.addEventListener('touchstart',function(evt){startX=evt.changedTouches[0].screenX;},false);\n"));
  WSContentSend_P (PSTR ("window.addEventListener('touchend',function(evt){stopX=evt.changedTouches[0].screenX;handleGesture();},false);\n"));
  WSContentSend_P (PSTR ("function handleGesture(){\n"));
  WSContentSend_P (PSTR ("if(stopX<startX-100){document.getElementById('next').click();}\n"));
  WSContentSend_P (PSTR ("else if(stopX>startX+100){document.getElementById('prev').click();}\n"));
  WSContentSend_P (PSTR ("}\n"));
}

#endif    // USE_WEBSERVER

/***************************************\
 *              Interface
\***************************************/

// Teleinfo sensor (for graph)
bool XsnsSensorHisto (uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function) 
  {
    case FUNC_INIT:
      SensorHistoInit ();
      break;

    case FUNC_SAVE_BEFORE_RESTART:
      SensorHistoSaveBeforeRestart ();
      break;

    case FUNC_EVERY_SECOND:
      SensorHistoEverySecond ();
      break;

#ifdef USE_WEBSERVER

     case FUNC_WEB_ADD_MAIN_BUTTON:
      SensorHistoWebMainButton ();
      break;

    case FUNC_WEB_ADD_HANDLER:
      Webserver->on (FPSTR (PSTR_SENSOR_PAGE_HISTO), SensorHistoWebPage);
      break;

#endif  // USE_WEBSERVER
  }

  return result;
}

#endif    // USE_UFILESYS
#endif    // USE_SENSOR_HISTO
