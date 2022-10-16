/*
  xdrv_93_log_manager.ino - Extensions for UFS driver
  This exension provides functions to save and display logs

  Copyright (C) 2019-2021  Nicolas Bernaerts

  Version history :
    08/02/2022 - v1.0 - Creation

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

#ifndef FIRMWARE_SAFEBOOT
#ifdef USE_COMMON_LOG

#define XDRV_93                   93

#define LOG_EVENT_QTY             10
#define LOG_LINE_MAX              128

#define D_PAGE_LOG_HISTORY        "log"

// log periods
enum LogPeriod { UFS_LOG_PERIOD_DAY, UFS_LOG_PERIOD_MONTH, UFS_LOG_PERIOD_YEAR, UFS_LOG_PERIOD_MAX };       // periods
const char kLogPeriod[] PROGMEM = "Day|Month|year";                                                         // period labels

// log events
enum LogEventType { LOG_EVENT_NEW, LOG_EVENT_UPDATE, LOG_EVENT_MAX };                                       // event type

// log file management structure
static struct {
  uint8_t  file_unit     = UFS_LOG_PERIOD_DAY;          // log file split unit
  uint8_t  nbr_column    = 0;                           // number of columns (according to title)
  uint16_t file_index    = UINT16_MAX;                  // last record file index
  uint32_t file_position = UINT32_MAX;                  // last record file position
  uint32_t event_start   = UINT32_MAX;                  // last record timestamp
  String   str_filename;                                // filename of logs (may include %u for file index)
  String   str_title;                                   // history page title
  String   str_header;                                  // history table header
} log_status;

static struct {
  bool start_date = true;                               // display start date column
  bool start_time = true;                               // display start time column
  bool stop_date  = false;                              // display stop date column
  bool stop_time  = false;                              // display stop time column
  bool duration   = true;                               // display duration column
} log_display;

struct {
  uint8_t  index = 0;                                   // current event index
  uint32_t start[LOG_EVENT_QTY];                        // event start time
  uint32_t duration[LOG_EVENT_QTY];                     // event duration
  String   str_desc[LOG_EVENT_QTY];                     // event data (string separated by ;)
} log_event;

/********************************************\
 *              Configuration
\********************************************/

// set log file split unit
void LogFileSetFilename (const char *pstr_filename, uint8_t unit) 
{
  // check parameters
  if (pstr_filename == nullptr) return;
  if (unit >= UFS_LOG_PERIOD_MAX) return;

  // set filename and unit type
  log_status.str_filename = pstr_filename;
  log_status.file_unit = unit;
}

// set title, header and number of columns to display
void LogHistoSetDescription (const char* pstr_title, const char* pstr_header, uint8_t nbr_column) 
{
  // set title, header and number of columns
  log_status.str_title  = pstr_title;
  log_status.str_header = pstr_header;
  log_status.nbr_column = nbr_column;
}

// set date columns to display
void LogHistoSetDateColumn (bool start_date, bool start_time, bool stop_date, bool stop_time, bool duration) 
{
  log_display.start_date = start_date;
  log_display.start_time = start_time;
  log_display.stop_date  = stop_date;
  log_display.stop_time  = stop_time;
  log_display.duration   = duration;
}

/*********************************************\
 *               Functions
\*********************************************/

// determine log filename according to index
void LogFileGetNameFromIndex (char* pstr_filename, size_t size_filename, uint16_t index) 
{
  // check parameters
  if (pstr_filename == nullptr) return;

  // generate log file name according to unit period
  sprintf (pstr_filename, log_status.str_filename.c_str (), index);
}

// determine log filename
void LogFileGetName (char* pstr_filename, size_t size_filename) 
{
  uint16_t index;
  TIME_T   time_dst;

  // check parameters
  if (pstr_filename == nullptr) return;

  // get period current index
  BreakTime (LocalTime (), time_dst);
 
  // set file index according to period unit
  switch (log_status.file_unit)
  { 
    case UFS_LOG_PERIOD_DAY:   log_status.file_index = time_dst.day_of_month; break;
    case UFS_LOG_PERIOD_MONTH: log_status.file_index = time_dst.month;        break;
    case UFS_LOG_PERIOD_YEAR:  log_status.file_index = time_dst.year + 1970;  break;
  }

  // generate log file name according to index
  LogFileGetNameFromIndex (pstr_filename, size_filename, log_status.file_index);
}

// save data to log file
bool LogSaveEvent (uint8_t event_type, const char* pstr_event) 
{
  uint32_t event_time, event_duration;

  // check event type
  if (pstr_event == nullptr) return false;
  if (event_type >= LOG_EVENT_MAX) return false;
  if ((event_type == LOG_EVENT_UPDATE) && (log_status.event_start == UINT32_MAX)) event_type = LOG_EVENT_NEW;

  // get current timestamp
  event_time = LocalTime ();

  // calculate current event date
  if (event_type == LOG_EVENT_NEW) log_status.event_start = event_time;

  // calculate event duration
  event_duration = event_time - log_status.event_start;

#ifdef USE_UFILESYS

  File file;
  char str_line[128];
  char str_filename[UFS_FILENAME_SIZE];

  // check update and known position
  if  ((event_type == LOG_EVENT_UPDATE) && (log_status.file_position == UINT32_MAX)) return false;

  // generate line
  sprintf_P (str_line, PSTR ("%u;%u;%s\n"), log_status.event_start, event_duration, pstr_event);

  // get log file name
  if (event_type == LOG_EVENT_NEW) LogFileGetName (str_filename, sizeof (str_filename));
  else LogFileGetNameFromIndex (str_filename, sizeof (str_filename), log_status.file_index);

  // open file in append mode
  file = ffsp->open (str_filename, "a");

  // if new event save file position, else seek to previous event position for update
  if (event_type == LOG_EVENT_NEW) log_status.file_position = file.position ();
  
  // seek to file position
  file.seek (log_status.file_position);

  // write and close file
  file.print (str_line);
  file.close ();

#else

  // if new event, increase counter 
  if (event_type == LOG_EVENT_NEW) log_event.index++;
  log_event.index = log_event.index % LOG_EVENT_QTY;

  // save event
  log_event.start[log_event.index]    = log_status.event_start;
  log_event.duration[log_event.index] = event_duration;
  log_event.str_desc[log_event.index] = pstr_event;

#endif    // USE_UFILESYS

  return true;
}

// Log initialisation
void LogInit ()
{
  uint8_t index;

  // init available event list
  for (index = 0; index < LOG_EVENT_QTY; index++) log_event.start[index] = UINT32_MAX;

#ifdef USE_UFILESYS

  // set default log filename
//  if (log_status.str_filename.length () == 0) log_status.str_filename = "period-%u.log";

#else

#endif  // USE_UFILESYS
}

/*********************************************\
 *                    Web
\*********************************************/

#ifdef USE_WEBSERVER

// display table header
void LogWebDisplayHeader ()
{
  char* pstr_token;
  char  str_header[LOG_LINE_MAX];

  // init event string
  strcpy_P (str_header, log_status.str_header.c_str ());

  // beginning of line
  WSContentSend_P (PSTR ("<tr>"));

  // loop thru data
  pstr_token = strtok (str_header, ";");
  while (pstr_token != nullptr)
  {
    WSContentSend_P (PSTR ("<th>%s</th>"), pstr_token);
    pstr_token = strtok (nullptr, ";");
  }

  // end of line
  WSContentSend_P (PSTR ("</tr>\n"));
}

// display one table event
bool LogWebDisplayEvent (uint8_t index_array)
{
  bool    event_displayed;
  uint8_t index  = 0;
  uint8_t column = 0;
  TIME_T  event_dst;
  char*   pstr_token;
  char    str_event[LOG_LINE_MAX];

  // if event exist
  event_displayed  = (log_event.start[index_array] != UINT32_MAX);
  if (event_displayed)
  {
    // init data
    strlcpy (str_event, log_event.str_desc[index_array].c_str (), sizeof (str_event));

    // beginning of line
    WSContentSend_P (PSTR ("<tr>"));

    // column : start date and time
    BreakTime (log_event.start[index_array], event_dst);
    if (event_dst.year < 1970) event_dst.year += 1970;
    if (log_display.start_date) WSContentSend_P (PSTR ("<td class='date'>%d-%02d-%02d</td>"), event_dst.year, event_dst.month, event_dst.day_of_month);
    if (log_display.start_time) WSContentSend_P (PSTR ("<td class='time'>%02d:%02d:%02d</td>"), event_dst.hour, event_dst.minute, event_dst.second);

    // if event currently active
    if (log_event.duration[index_array] == 0)
    {
      if (log_display.stop_date) index++;
      if (log_display.stop_time) index++;
      if (log_display.duration) index++;
      if (index > 0) WSContentSend_P (PSTR ("<td class='time red' colspan=%u>Active</td>"), index);
    }

    // else event is over, stop time and duration are defined
    else
    {
      // column : stop date and time
      BreakTime (log_event.start[index_array] + log_event.duration[index_array], event_dst);
      if (event_dst.year < 1970) event_dst.year += 1970;
      if (log_display.stop_date) WSContentSend_P (PSTR ("<td class='date'>%d-%02d-%02d</td>"), event_dst.year, event_dst.month, event_dst.day_of_month);
      if (log_display.stop_time) WSContentSend_P (PSTR ("<td class='time'>%02d:%02d:%02d</td>"), event_dst.hour, event_dst.minute, event_dst.second);

      // column : duration
      if (log_display.duration) WSContentSend_P (PSTR ("<td class='duration'>%u</td>"), log_event.duration[index_array]);
    }

    // calculate number of already displayed columns
    if (log_display.start_date) column++;
    if (log_display.start_time) column++;
    if (log_display.stop_date) column++;
    if (log_display.stop_time) column++;
    if (log_display.duration) column++;

    // loop thru data
    pstr_token = strtok (str_event, ";");
    while (pstr_token != nullptr)
    {
      column++;
      WSContentSend_P (PSTR ("<td class='data'>%s</td>"), pstr_token);
      pstr_token = strtok (nullptr, ";");
    }

    // loop thru unavailable data
    for (index = column; index < log_status.nbr_column; index++) WSContentSend_P (PSTR ("<td class='data'></td>"));

    // end of line
    WSContentSend_P (PSTR ("</tr>\n"));
  }

  return event_displayed;
}

// Log history page
void LogWebPageHistory ()
{
  bool     event_ok, event_displayed = false;
  int      index;
  uint32_t event_time = UINT32_MAX;
  TIME_T   event_dst;

  // beginning of page without authentification
  WSContentStart_P (log_status.str_title.c_str (), false);
  WSContentSend_P (PSTR ("</script>\n"));

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {width:100%%;margin:12px auto;padding:3px 0px;text-align:center;vertical-align:middle;}\n"));
  WSContentSend_P (PSTR ("div.title {font-size:1.5rem;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.title a {color:white;}\n"));
  WSContentSend_P (PSTR ("div.header {font-size:2.5rem;color:yellow;}\n"));
  WSContentSend_P (PSTR ("table {width:100%%;max-width:600px;margin:0px auto;border:none;background:none;color:#fff;border-collapse:collapse;}\n"));
  WSContentSend_P (PSTR ("th,td {font-size:1rem;padding:0.5rem 1rem;border:1px #666 solid;}\n"));
  WSContentSend_P (PSTR ("th {font-weight:bold;background:#555;}\n"));
  WSContentSend_P (PSTR ("td.date {font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("td.text {font-style:italic;}\n"));
  WSContentSend_P (PSTR ("td.red {color:red;}\n"));
  WSContentSend_P (PSTR ("a:link {text-decoration:none;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // page body
  WSContentSend_P (PSTR ("</head>\n"));
  WSContentSend_P (PSTR ("<body>\n"));

  // device name, icon and title
  WSContentSend_P (PSTR ("<div class='title'><a href='/'>%s</a></div>\n"), SettingsText(SET_DEVICENAME));
  WSContentSend_P (PSTR ("<div class='header'>%s</div>\n"), log_status.str_title.c_str ());

  // display table with header
  WSContentSend_P (PSTR ("<div><table>\n"));

  // display header
  LogWebDisplayHeader ();

#ifdef USE_UFILESYS
  size_t size_line, size_data;
  char   str_line[LOG_LINE_MAX];
  char   str_value[LOG_LINE_MAX];
  char   str_filename[UFS_FILENAME_SIZE];
  File   file;

  // read current log file, without header
  LogFileGetName (str_filename, sizeof (str_filename));
  if (ffsp->exists (str_filename))
  {
    // open file in read only mode in littlefs filesystem and go to end of file
    file = ffsp->open (str_filename, "r");
    UfsSeekToEnd (file);

    // read line in reverse order
    do
    {
      // read previous line
      size_line = UfsReadPreviousLine (file, str_line, sizeof (str_line));
      if (size_line > 0)
      {
        // read start time in 1st column
        size_data = UfsExtractCsvColumn (str_line, ';', 1, str_value, sizeof (str_value), false);

        // if start time is defined
        event_ok = (size_data > 0);
        if (event_ok)
        {
          // calculate start time
          log_event.start[0] = (uint32_t)atoll (str_value);
          event_ok = (event_time != log_event.start[0]);
        }

        // if event is valid
        if (event_ok)
        {
          // calculate start time and event duration
          event_time = log_event.start[0];

          // read event duration in 2nd column
          size_data = UfsExtractCsvColumn (str_line, ';', 2, str_value, sizeof (str_value), false);
          if (size_data > 0) log_event.duration[0] = (uint32_t)atoll (str_value);

          // read other data from 3rd column
          size_data = UfsExtractCsvColumn (str_line, ';', 3, str_value, sizeof (str_value), true);
          log_event.str_desc[0] = str_value;

          // display first memory event
          event_displayed |= LogWebDisplayEvent (0);
        }
      }
    } while (size_line > 0);

    // close file
    file.close ();
  }

#else       // No LittleFS

  // loop thru offload events array
  for (index = LOG_EVENT_QTY + log_event.index; index > log_event.index; index--) event_displayed |= LogWebDisplayEvent (index % LOG_EVENT_QTY);

#endif  // USE_UFILESYS

  // if no event
  if (!event_displayed) WSContentSend_P (PSTR ("<tr><td class='text' colspan=%u>No event available</td></tr>\n"), log_status.nbr_column);

  // end of table and end of page
  WSContentSend_P (PSTR ("</table></div>\n"));
  WSContentStop ();
}

#endif  // USE_WEBSERVER

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xdrv93 (uint8_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
    case FUNC_INIT:
      LogInit ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      Webserver->on ("/" D_PAGE_LOG_HISTORY, LogWebPageHistory);

    case FUNC_WEB_ADD_MAIN_BUTTON:
      WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>"), D_PAGE_LOG_HISTORY, log_status.str_title.c_str ());
      break;
#endif  // USE_WEBSERVER

  }
  
  return result;
}

#endif		// USE_COMMON_LOG
#endif    // FIRMWARE_SAFEBOOT