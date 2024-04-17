/*
  xsns_124_teleinfo_histo.ino - France Teleinfo energy sensor graph : history (conso & prod)

  Copyright (C) 2019-2024  Nicolas Bernaerts

  Version history :
    29/02/2024 - v14.0 - Split graph in 3 files

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

// declare teleinfo graph curves
#define XSNS_124                         124

// web URL
const char D_TELEINFO_PAGE_GRAPH_CONSO[] PROGMEM = "/conso";
const char D_TELEINFO_PAGE_GRAPH_PROD[]  PROGMEM = "/prod";

// historisation files
const char D_TELEINFO_HISTO_HEADER[]    PROGMEM = "Idx;Month;Day;Global;Daily;00h;01h;02h;03h;04h;05h;06h;07h;08h;09h;10h;11h;12h;13h;14h;15h;16h;17h;18h;19h;20h;21h;22h;23h";
const char D_TELEINFO_HISTO_FILE_YEAR[] PROGMEM = "/teleinfo-year-%04d.csv";
const char D_TELEINFO_HISTO_FILE_PROD[] PROGMEM = "/production-year-%04d.csv";

/****************************************\
 *                 Data
\****************************************/

struct histo_conso {
  long long start_wh;                               // first daily total
  long long hour_wh[24];                            // hours start total
  long long period_wh[TELEINFO_INDEX_MAX];          // period first total
};

struct histo_prod {
  long long start_wh;                               // first daily total
  long long hour_wh[24];                            // hours start total
};

static struct {
  uint8_t     last_day = UINT8_MAX;                 // day index of last saved line
  histo_conso conso;                                // conso historisation data
  histo_prod  prod;                                 // production historisation data
} teleinfo_histo;

/*********************************************\
 *               Functions
\*********************************************/

// calculate number of days in given month
uint8_t TeleinfoHistoGetDaysInMonth (const uint32_t year, const uint8_t month)
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

/*********************************************\
 *               Historisation
\*********************************************/

// Get historisation period start time
uint32_t TeleinfoHistoGetStartTime (const uint8_t histo)
{
  uint32_t start_time, delta_time;
  TIME_T   start_dst;

  // start date
  start_time = LocalTime ();

  // set to beginning of current day
  BreakTime (start_time, start_dst);
  start_dst.hour   = 0;
  start_dst.minute = 0;
  start_dst.second = 0;

  // convert back to localtime
  start_time = MakeTime (start_dst);

  return start_time;
}

// Get historisation period literal date
void TeleinfoHistoGetDate (const int period, const int histo, char* pstr_text)
{
  uint32_t calc_time;
  TIME_T   start_dst;

  // check parameters and init
  if (pstr_text == nullptr) return;

  // start date
  calc_time = TeleinfoHistoGetStartTime (histo);
  BreakTime (calc_time, start_dst);

  // set label according to period
  if ((period == TELEINFO_HISTO_CONSO) || (period == TELEINFO_HISTO_PROD)) sprintf (pstr_text, "%04u", 1970 + start_dst.year - histo);
   else strcpy (pstr_text, "");
}

// Get historisation filename
bool TeleinfoHistoGetFilename (const uint8_t period, const uint8_t histo, char* pstr_filename)
{
  bool exists = false;

  // check parameters
  if (pstr_filename == nullptr) return false;

  // generate filename according to period
  strcpy (pstr_filename, "");
  if (period == TELEINFO_HISTO_CONSO) sprintf_P (pstr_filename, D_TELEINFO_HISTO_FILE_YEAR, RtcTime.year - (uint16_t)histo);
  else if (period == TELEINFO_HISTO_PROD) sprintf_P (pstr_filename, D_TELEINFO_HISTO_FILE_PROD, RtcTime.year - (uint16_t)histo);

  // if filename defined, check existence
  if (strlen (pstr_filename) > 0) exists = ffsp->exists (pstr_filename);

  return exists;
}


// get today's consommation for specific period
long TeleinfoHistoConsoGetPeriod (const uint8_t index)
{
  long long value;

  // out of range
  if (index >= TELEINFO_INDEX_MAX) value = 0;

  // period not defined
  else if (teleinfo_histo.conso.period_wh[index] == 0) value = 0;

  // calculate period increment
  else value = teleinfo_conso.index_wh[index] - teleinfo_histo.conso.period_wh[index];

  return (long)value;
}

// get today's consommation for specific hour
long TeleinfoHistoConsoGetHour (const uint8_t index)
{
  long long value;

  // not available
  if (teleinfo_conso.total_wh == 0) value = 0;

  // out of range
  else if (index > 23) value = 0;

  // last daily slot
  else if (index == 23)
  {
    if (teleinfo_histo.conso.hour_wh[index] == 0) value = 0;
    else value = teleinfo_conso.total_wh - teleinfo_histo.conso.hour_wh[index];
  }

  // other slots
  else
  {
    if (teleinfo_histo.conso.hour_wh[index] == 0) value = 0;
    else if (teleinfo_histo.conso.hour_wh[index + 1] == 0) value = teleinfo_conso.total_wh - teleinfo_histo.conso.hour_wh[index];
    else value = teleinfo_histo.conso.hour_wh[index + 1] - teleinfo_histo.conso.hour_wh[index];
  }

  return (long)value;
}

// get today's production for specific hour
long TeleinfoHistoProdGetHour (const uint8_t index)
{
  long long value;

  // not available
  if (teleinfo_prod.total_wh == 0) value = 0;

  // out of range
  else if (index > 23) value = 0;

  // last daily slot
  else if (index == 23)
  {
    if (teleinfo_histo.prod.hour_wh[index] == 0) value = 0;
    else value = teleinfo_prod.total_wh - teleinfo_histo.prod.hour_wh[index];
  }

  // other slots
  else
  {
    if (teleinfo_histo.prod.hour_wh[index] == 0) value = 0;
    else if (teleinfo_histo.prod.hour_wh[index + 1] == 0) value = teleinfo_prod.total_wh - teleinfo_histo.prod.hour_wh[index];
    else value = teleinfo_histo.prod.hour_wh[index + 1] - teleinfo_histo.prod.hour_wh[index];
  }

  return (long)value;
}

// Save historisation data
void TeleinfoHistoSaveData ()
{
  uint8_t   index;
  uint16_t  year;
  TIME_T    today_dst;
  long      result;
  long long value;
  String    str_line;
  File      file;
  char      str_text[32];
  char      str_filename[UFS_FILENAME_SIZE];

  // if date not defined, cancel
  if (!RtcTime.valid) return;

  // calculate filename year (shift 1h before)
  BreakTime (LocalTime () - 3600, today_dst);
  year = 1970 + today_dst.year;

  // if daily conso is available
  if (teleinfo_histo.conso.start_wh > 0)
  {
    // get filename
    sprintf_P (str_filename, D_TELEINFO_HISTO_FILE_YEAR, year);

    // if file exists, open in append mode
    if (ffsp->exists (str_filename)) file = ffsp->open (str_filename, "a");

    // else create file and add header
    else
    {
      // create file
      file = ffsp->open (str_filename, "w");

      // generate header with period name and totals
      str_line = D_TELEINFO_HISTO_HEADER;
      str_line += ";Period";
      for (index = 0; index < teleinfo_contract.period_qty; index++)
      {
        TeleinfoPeriodGetCode (index, str_text, sizeof (str_text));
        str_line += ";";
        str_line += str_text;
      }
      str_line += "\n";
 
      // write header
      file.print (str_line.c_str ());
    }

    // line : beginning
    sprintf (str_text, "%u;%u;%u", today_dst.day_of_year, today_dst.month, today_dst.day_of_month);
    str_line = str_text;

    // line : global counter
    lltoa (teleinfo_conso.total_wh, str_text, 10);
    str_line += ";";
    str_line += str_text;

    // line : daily counter
    value = teleinfo_conso.total_wh - teleinfo_histo.conso.start_wh;
    lltoa (value, str_text, 10);
    str_line += ";";
    str_line += str_text;

    // line : hourly totals
    for (index = 0; index < 24; index ++)
    {
      str_line += ";";
      str_line += TeleinfoHistoConsoGetHour (index);
    }

    // line : period name
    TeleinfoPeriodGetCode (str_text, sizeof (str_text));
    str_line += ";";
    str_line += str_text;

    // line : totals
    for (index = 0; index < teleinfo_contract.period_qty; index++)
    {
      str_line += ";";
      str_line += TeleinfoHistoConsoGetPeriod (index);
    }

    // write line and close file
    str_line += "\n";
    file.print (str_line.c_str ());
    file.close ();

    // reset historisation data
    teleinfo_histo.conso.start_wh = teleinfo_conso.total_wh;
    for (index = 0; index < 24; index ++) teleinfo_histo.conso.hour_wh[index] = 0;
    teleinfo_histo.conso.hour_wh[RtcTime.hour] = teleinfo_conso.total_wh;
    for (index = 0; index < TELEINFO_INDEX_MAX; index ++) teleinfo_histo.conso.period_wh[index] = 0;
    teleinfo_histo.conso.period_wh[teleinfo_contract.period] = teleinfo_conso.index_wh[teleinfo_contract.period];
  }

  // if daily production is available
  if (teleinfo_histo.prod.start_wh > 0)
  {
    // get filename
    sprintf_P (str_filename, D_TELEINFO_HISTO_FILE_PROD, year);

    // if file exists, open in append mode
    if (ffsp->exists (str_filename)) file = ffsp->open (str_filename, "a");

    // else create file and add header
    else
    {
      // create file
      file = ffsp->open (str_filename, "w");

      // write header
      str_line = D_TELEINFO_HISTO_HEADER;
      str_line += "\n";
      file.print (str_line.c_str ());
    }

    // line : beginning
    sprintf (str_text, "%u;%u;%u", today_dst.day_of_year, today_dst.month, today_dst.day_of_month);
    str_line = str_text;

    // line : global counter
    lltoa (teleinfo_prod.total_wh, str_text, 10);
    str_line += ";";
    str_line += str_text;

    // line : daily counter
    value = teleinfo_prod.total_wh - teleinfo_histo.prod.start_wh;
    lltoa (value, str_text, 10);
    str_line += ";";
    str_line += str_text;

    // line : hourly totals
    for (index = 0; index < 24; index ++)
    {
      str_line += ";";
      str_line += TeleinfoHistoProdGetHour (index);
    }

    // write line and close file
    str_line += "\n";
    file.print (str_line.c_str ());
    file.close ();

    // reset historisation data
    teleinfo_histo.prod.start_wh = teleinfo_prod.total_wh;
    for (index = 0; index < 24; index ++) teleinfo_histo.prod.hour_wh[index] = 0;
    teleinfo_histo.prod.hour_wh[RtcTime.hour] = teleinfo_prod.total_wh;
  }
}

/*********************************************\
 *                   Callback
\*********************************************/

// Teleinfo historisation initialisation
void TeleinfoHistoInit ()
{
  uint8_t index;

  AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: Init HISTO module"));

  // init start value
  teleinfo_histo.conso.start_wh = 0;
  teleinfo_histo.prod.start_wh  = 0;

  // init hourly values
  for (index = 0; index < 24; index ++)
  {
    teleinfo_histo.conso.hour_wh[index] = 0;
    teleinfo_histo.prod.hour_wh[index]  = 0;
  }

  // init period values
  for (index = 0; index < TELEINFO_INDEX_MAX; index ++) teleinfo_histo.conso.period_wh[index] = 0;
}

// called every second
void TeleinfoHistoEverySecond ()
{
  uint8_t day, hour, period;

  // ignore if time is not valid
  if (!RtcTime.valid) return;

  // init
  day    = RtcTime.day_of_month;
  hour   = RtcTime.hour;
  period = teleinfo_contract.period;

  // in case of day change
  if (teleinfo_histo.last_day == UINT8_MAX) teleinfo_histo.last_day = day;
  else if (teleinfo_histo.last_day != day)
  {
    // save historisation data
    TeleinfoHistoSaveData ();

    // update current day
    teleinfo_histo.last_day = day;
  }

  // if meter is not considered as ready, ignore next
  if (!TeleinfoDriverMeterReady ()) return;

  // if conso is defined
  if (teleinfo_conso.total_wh > 0)
  {
    // if needed, init start counter
    if (teleinfo_histo.conso.start_wh == 0) teleinfo_histo.conso.start_wh = teleinfo_conso.total_wh;

    // if needed, init hourly counter
    if (teleinfo_histo.conso.hour_wh[hour] == 0) teleinfo_histo.conso.hour_wh[hour] = teleinfo_conso.total_wh;

    // if needed, init current period counter
    if (period < TELEINFO_INDEX_MAX)
    {
      if (teleinfo_histo.conso.period_wh[period] == 0) teleinfo_histo.conso.period_wh[period] = teleinfo_conso.index_wh[period];
    }
  }

  // if production is defined
  if (teleinfo_prod.total_wh > 0)
  {
    // if needed, init start counter
    if (teleinfo_histo.prod.start_wh == 0) teleinfo_histo.prod.start_wh = teleinfo_prod.total_wh;

    // if needed, init hourly counter
    if (teleinfo_histo.prod.hour_wh[hour] == 0) teleinfo_histo.prod.hour_wh[hour] = teleinfo_prod.total_wh;
  }
}

// Save data in case of planned restart
void TeleinfoHistoSaveBeforeRestart ()
{
  // save historisation data
  TeleinfoHistoSaveData ();
}

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

// Graph frame
void TeleinfoHistoDisplayFrame (const uint8_t month, const uint8_t day)
{
  long    index, unit_max;
  char    arr_label[5][12];
  uint8_t arr_pos[5] = {98,76,51,26,2};  

  // set labels according to data type
  if (month == 0) unit_max = teleinfo_config.param[TELEINFO_CONFIG_MAX_MONTH];
    else if (day == 0) unit_max = teleinfo_config.param[TELEINFO_CONFIG_MAX_DAY];
      else unit_max = teleinfo_config.param[TELEINFO_CONFIG_MAX_HOUR];
  unit_max = unit_max * 1000;
  for (index = 0; index < 5; index ++) TeleinfoDriverGetDataDisplay (TELEINFO_UNIT_MAX, index * unit_max / 4, arr_label[index], sizeof (arr_label[index]), true);


  // graph frame
  WSContentSend_P (PSTR ("<rect x='%d%%' y='%d%%' width='%d%%' height='99.9%%' rx='10' />\n"), TELEINFO_GRAPH_PERCENT_START, 0, TELEINFO_GRAPH_PERCENT_STOP - TELEINFO_GRAPH_PERCENT_START);

  // graph separation lines
  WSContentSend_P (PSTR ("<line class='dash' x1='%d%%' y1='%d%%' x2='%d%%' y2='%d%%' />\n"), TELEINFO_GRAPH_PERCENT_START, 25, TELEINFO_GRAPH_PERCENT_STOP, 25);
  WSContentSend_P (PSTR ("<line class='dash' x1='%d%%' y1='%d%%' x2='%d%%' y2='%d%%' />\n"), TELEINFO_GRAPH_PERCENT_START, 50, TELEINFO_GRAPH_PERCENT_STOP, 50);
  WSContentSend_P (PSTR ("<line class='dash' x1='%d%%' y1='%d%%' x2='%d%%' y2='%d%%' />\n"), TELEINFO_GRAPH_PERCENT_START, 75, TELEINFO_GRAPH_PERCENT_STOP, 75);

  // units graduation
  WSContentSend_P (PSTR ("<text class='power' x='%d%%' y='%d%%'>Wh</text>\n"), TELEINFO_GRAPH_PERCENT_STOP + 3, 2);
  for (index = 0; index < 5; index ++) WSContentSend_P (PSTR ("<text class='power' x='%u%%' y='%u%%'>%s</text>\n"), 2, arr_pos[index],  arr_label[index]);
}

// Append Teleinfo histo button to main page
void TeleinfoHistoWebMainButton ()
{
  if (teleinfo_conso.total_wh > 0) WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>Consommation</button></form></p>\n"), D_TELEINFO_PAGE_GRAPH_CONSO);
  if (teleinfo_prod.total_wh > 0)   WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>Production</button></form></p>\n"), D_TELEINFO_PAGE_GRAPH_PROD);
}

// Display bar graph
void TeleinfoHistoDisplayBar (const uint8_t histo, const bool is_main)
{
  bool     analyse;
  int      index, count;
  int      month, day, hour;
  long     value, value_x, value_y;
  long     graph_x, graph_y, graph_delta, graph_width, graph_height, graph_x_end, graph_max;
  long     graph_start, graph_stop;  
  long     arr_total[TELEINFO_GRAPH_MAX_BARGRAPH];
  uint8_t  day_of_week;
  uint32_t time_bar;
  uint32_t len_buffer;
  TIME_T   time_dst;
  char    *pstr_token;
  char     str_type[8];
  char     str_value[16];
  char     str_buffer[192];
  char     str_filename[UFS_FILENAME_SIZE];
  File     file;

  // init array
  for (index = 0; index < TELEINFO_GRAPH_MAX_BARGRAPH; index ++) arr_total[index] = 0;

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
  if (TeleinfoHistoGetFilename (teleinfo_graph.period, histo, str_filename))
  {
    // open file
    count = 0;
    file = ffsp->open (str_filename, "r");
    while (file.available ())
    {
      // init
      index = 0;
      month = INT_MAX;
      day   = INT_MAX;
      pstr_token = nullptr;

      // read next line
      len_buffer = file.readBytesUntil ('\n', str_buffer, sizeof (str_buffer));
      str_buffer[len_buffer] = 0;

      // if first caracter is not numerical, ignore line (header, empty line, ...)
      if (isdigit (str_buffer[0])) pstr_token = strtok (str_buffer, ";");
      while (pstr_token != nullptr)
      {
        // if token is defined
        value = 0;
        if (strlen (pstr_token) > 0)
        {
          if (index == 1) month = atoi (pstr_token);
          else if (index == 2) day = atoi (pstr_token);
          else if (index == 4) value = atol (pstr_token);
          else if ((index > 4) && (index < 24 + 5)) value = atol (pstr_token);
        }

        // display months of the year
        if ((teleinfo_graph.month == 0) && (index >= 4))
        {
          // update current month total
          if (month > graph_stop) month = TELEINFO_GRAPH_MAX_BARGRAPH - 1;
          if (arr_total[month] == LONG_MAX) arr_total[month] = value; 
            else arr_total[month] += value;

          // end line analysis
          pstr_token = nullptr;
        }

        // display days of the month
        else if ((teleinfo_graph.month == month) && (teleinfo_graph.day == 0) && (index >= 4))
        {
          // update current day total
          if (day > graph_stop) day = TELEINFO_GRAPH_MAX_BARGRAPH - 1;
          if (arr_total[day] == LONG_MAX) arr_total[day] = value; 
            else arr_total[day] += value;

          // end line analysis
          pstr_token = nullptr;
        }

        // display hours of the day
        else if ((teleinfo_graph.month == month) && (teleinfo_graph.day == day) && (index > 4) && (index < 24 + 5))
        {
          hour = index - 5;
          if (arr_total[hour] == LONG_MAX) arr_total[hour] = value;
            else arr_total[hour] += value;
        }

        // if max columns reached, end line analysis
        index++;
        if (index > 24 + 5) pstr_token = nullptr;

        // next token in the line
        if (pstr_token != nullptr) pstr_token = strtok (nullptr, ";");
      }

      // handle data reception update every 20 lines
      if (count % 20 == 0) TeleinfoProcessRealTime ();
      count++;
    }

    // close file
    file.close ();
  }

  // if current year data
  if (histo == 0)
  {
    // if displaying production data
    if (teleinfo_graph.period == TELEINFO_HISTO_PROD)
    {
      // if dealing with current day, add live values to hourly slots
      if ((teleinfo_graph.month == RtcTime.month) && (teleinfo_graph.day == RtcTime.day_of_month))
      {
        for (index = 0; index < 24; index ++)
        {
          value = TeleinfoHistoProdGetHour (index);
          if (arr_total[index] == LONG_MAX) arr_total[index] = value;
            else arr_total[index] += value;
        }
      }

      // else if current monthly data, init current day slot from live values sum
      else if ((teleinfo_graph.month == RtcTime.month) && (teleinfo_graph.day == 0))
      {
        for (index = 0; index < 24; index ++)
        {
          value = TeleinfoHistoProdGetHour (index);
          if (arr_total[RtcTime.day_of_month] == LONG_MAX) arr_total[RtcTime.day_of_month] = value;
            else arr_total[RtcTime.day_of_month] += value;
        }
      }
    } 

    // else displaying conso data
    else if (teleinfo_graph.period == TELEINFO_HISTO_CONSO)
    {
      // if current daily data, init previous hour slot from live values
      if ((teleinfo_graph.month == RtcTime.month) && (teleinfo_graph.day == RtcTime.day_of_month))
      {
        for (index = 0; index < 24; index ++)
        {
          value = TeleinfoHistoConsoGetHour (index);
          if (arr_total[index] == LONG_MAX) arr_total[index] = value;
            else arr_total[index] += value; 
        }
      }

      // else if current monthly data, init current day slot from live values sum
      else if ((teleinfo_graph.month == RtcTime.month) && (teleinfo_graph.day == 0))
      {
        for (index = 0; index < 24; index ++)
        {
          value = TeleinfoHistoConsoGetHour (index);
          if (arr_total[RtcTime.day_of_month] == LONG_MAX) arr_total[RtcTime.day_of_month] = value;
          else arr_total[RtcTime.day_of_month] += value;
        }
      }
    } 
  }

  // bar graph
  // ---------

  for (index = graph_start; index <= graph_stop; index ++)
  {
    // if value is defined, display bar and value
    if (arr_total[index] != 0)
    {
      // display
      if (graph_max != 0) graph_y = graph_height - (arr_total[index] * graph_height / graph_max / 1000); else graph_y = 0;
      if (graph_y < 0) graph_y = 0;

      // display link
      if (teleinfo_graph.period == TELEINFO_HISTO_PROD) strcpy (str_value, D_TELEINFO_PAGE_GRAPH_PROD); else strcpy (str_value, D_TELEINFO_PAGE_GRAPH_CONSO);
      if (is_main && (teleinfo_graph.month == 0)) WSContentSend_P (PSTR ("<a href='%s?histo=%u&month=%d&day=0'>"), str_value, histo, index);
      else if (is_main && (teleinfo_graph.day == 0)) WSContentSend_P (PSTR ("<a href='%s?histo=%u&month=%u&day=%d'>"), str_value, histo, teleinfo_graph.month, index);

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
        if (arr_total[index] > 0)
        {
          // calculate y position
          value_y = graph_y - 15;
          if (value_y < 15) value_y = 15;
          if (value_y > graph_height - 50) value_y = graph_height - 50;

          // display value
          TeleinfoDriverGetDataDisplay (TELEINFO_UNIT_MAX, arr_total[index], str_value, sizeof (str_value), true);
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
    TeleinfoProcessRealTime ();
  }
}

// calculate historisation previous period parameters
bool TeleinfoHistoPreviousPeriod (const bool update)
{
  bool     is_file = false;
  uint8_t  histo = teleinfo_graph.histo;
  uint8_t  month = teleinfo_graph.month;
  uint8_t  day   = teleinfo_graph.day;
  uint32_t year;
  TIME_T   time_dst;
  char     str_filename[UFS_FILENAME_SIZE];

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
      day = TeleinfoHistoGetDaysInMonth (year, month);
    }
  }

  // check if yearly file is available
  is_file = TeleinfoHistoGetFilename (teleinfo_graph.period, histo, str_filename);

  // if asked, set next period
  if (is_file && update)
  {
    teleinfo_graph.histo = histo;
    teleinfo_graph.month = month;
    teleinfo_graph.day   = day;
  }

  return is_file;
}

// check if next historisation period is available
bool TeleinfoHistoNextPeriod (const bool update)
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
    nb_day = TeleinfoHistoGetDaysInMonth (year, month);

    // next day
    day++;
    if (day > nb_day) { day = 1; month++; }
    if (month == 13) { month = 1; is_next = true; }
  }

  // if yearly file available
  if ((histo == 0) && is_next) histo = UINT8_MAX;
    else if (is_next) histo--;

  // check if yearly file is available
  is_file = TeleinfoHistoGetFilename (teleinfo_graph.period, histo, str_filename);

  // if asked, set next period
  if (is_file && update)
  {
    teleinfo_graph.histo = histo;
    teleinfo_graph.month = month;
    teleinfo_graph.day   = day;
  }

  return is_file;
}

// Graph public page
void TeleinfoHistoGraphWebConso () { TeleinfoHistoGraphWeb (TELEINFO_HISTO_CONSO); }
void TeleinfoHistoGraphWebProd () { TeleinfoHistoGraphWeb (TELEINFO_HISTO_PROD); }
void TeleinfoHistoGraphWeb (const uint8_t period_type)
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
    teleinfo_graph.day    = (uint8_t)TeleinfoDriverGetArgValue (D_CMND_TELEINFO_DAY,   0, 31, teleinfo_graph.day);
    teleinfo_graph.month  = (uint8_t)TeleinfoDriverGetArgValue (D_CMND_TELEINFO_MONTH, 0, 12, teleinfo_graph.month);
    teleinfo_graph.histo  = (uint8_t)TeleinfoDriverGetArgValue (D_CMND_TELEINFO_HISTO, 0, 20, teleinfo_graph.histo);

    // graph increment
    choice = TeleinfoDriverGetArgValue (D_CMND_TELEINFO_PLUS, 0, 1, 0);
    if (choice == 1)
    {
      if (teleinfo_graph.month == 0) teleinfo_config.param[TELEINFO_CONFIG_MAX_MONTH] *= 2;
      else if (teleinfo_graph.day == 0) teleinfo_config.param[TELEINFO_CONFIG_MAX_DAY] *= 2;
      else teleinfo_config.param[TELEINFO_CONFIG_MAX_HOUR] *= 2;
    }

    // graph decrement
    choice = TeleinfoDriverGetArgValue (D_CMND_TELEINFO_MINUS, 0, 1, 0);
    if (choice == 1)
    {
      if (teleinfo_graph.month == 0) teleinfo_config.param[TELEINFO_CONFIG_MAX_MONTH] = max ((long)TELEINFO_GRAPH_MIN_WH_MONTH, teleinfo_config.param[TELEINFO_CONFIG_MAX_MONTH] / 2);
      else if (teleinfo_graph.day == 0) teleinfo_config.param[TELEINFO_CONFIG_MAX_DAY] = max ((long)TELEINFO_GRAPH_MIN_WH_DAY , teleinfo_config.param[TELEINFO_CONFIG_MAX_DAY] / 2);
      else teleinfo_config.param[TELEINFO_CONFIG_MAX_HOUR] = max ((long)TELEINFO_GRAPH_MIN_WH_HOUR, teleinfo_config.param[TELEINFO_CONFIG_MAX_HOUR] / 2);
    }

    // handle previous page
    choice = TeleinfoDriverGetArgValue (D_CMND_TELEINFO_PREV, 0, 1, 0);
    if (choice == 1) TeleinfoHistoPreviousPeriod (true);

    // handle next page
    choice = TeleinfoDriverGetArgValue (D_CMND_TELEINFO_NEXT, 0, 1, 0);
    if (choice == 1) TeleinfoHistoNextPeriod (true);

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
    if (teleinfo_graph.period == TELEINFO_HISTO_PROD) strcpy (str_text, TELEINFO_COLOR_PROD);
      else strcpy (str_text, TELEINFO_COLOR_CONSO);
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
    if (teleinfo_graph.period == TELEINFO_HISTO_PROD) strcpy (str_text, D_TELEINFO_PAGE_GRAPH_PROD);
      else strcpy (str_text, D_TELEINFO_PAGE_GRAPH_CONSO);
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
    if (TeleinfoHistoPreviousPeriod (false)) strcpy (str_text, "");
      else strcpy (str_text, "disabled");
    WSContentSend_P (PSTR ("<div class='prev'><button class='navi' name='%s' id='%s' value=1 %s>&lt;&lt;</button></div>\n"), D_CMND_TELEINFO_PREV, D_CMND_TELEINFO_PREV, str_text);

    // next button
    if (TeleinfoHistoNextPeriod (false)) strcpy (str_text, "");
      else strcpy (str_text, "disabled");
    WSContentSend_P (PSTR ("<div class='next'><button class='navi' name='%s' id='%s' value=1 %s>&gt;&gt;</button></div>\n"), D_CMND_TELEINFO_NEXT, D_CMND_TELEINFO_NEXT, str_text);

    // -----------------
    //    Title
    // -----------------

    WSContentSend_P (PSTR ("<div>\n"));

    WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText (SET_DEVICENAME));
    if (teleinfo_graph.period == TELEINFO_HISTO_PROD) strcpy (str_text, "Production"); else strcpy (str_text, "Consommation");
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
      if (TeleinfoHistoGetFilename (teleinfo_graph.period, counter, str_filename))
      {
        TeleinfoHistoGetDate (teleinfo_graph.period, counter, str_date);
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
      if (teleinfo_graph.period == TELEINFO_HISTO_PROD) strcpy (str_text, D_TELEINFO_PAGE_GRAPH_PROD);
        else strcpy (str_text, D_TELEINFO_PAGE_GRAPH_CONSO);
      WSContentSend_P (PSTR ("<a href='%s?histo=%u&month=%u&day=0'><div class='item month%s'>%s</div></a>\n"), str_text, teleinfo_graph.histo, index, str_active, str_date);       
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
      choice = TeleinfoHistoGetDaysInMonth (1970 + (uint32_t)time_dst.year - teleinfo_graph.histo, teleinfo_graph.month);

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
        if (teleinfo_graph.period == TELEINFO_HISTO_PROD) strcpy (str_text, D_TELEINFO_PAGE_GRAPH_PROD);
          else strcpy (str_text, D_TELEINFO_PAGE_GRAPH_CONSO);
        WSContentSend_P (PSTR ("<a href='%s?histo=%u&month=%u&day=%u'><div class='item day%s'>%u</div></a>\n"), str_text, teleinfo_graph.histo, teleinfo_graph.month, index, str_active, counter);
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
    if (teleinfo_graph.period == TELEINFO_HISTO_PROD) strcpy (str_text, TELEINFO_COLOR_PROD_PREV);
      else strcpy (str_text, TELEINFO_COLOR_CONSO_PREV);
    WSContentSend_P (PSTR ("path.prev {stroke:%s;fill:%s;fill-opacity:1;}\n"), str_text, str_text);
    if (teleinfo_graph.period == TELEINFO_HISTO_PROD) strcpy (str_text, TELEINFO_COLOR_PROD);
      else strcpy (str_text, TELEINFO_COLOR_CONSO);
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
    TeleinfoHistoDisplayFrame (teleinfo_graph.month, teleinfo_graph.day);

    // svg : curves
    TeleinfoHistoDisplayBar (teleinfo_graph.histo + 1, false);       // previous period
    TeleinfoHistoDisplayBar (teleinfo_graph.histo,     true);        // current period

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

#endif    // USE_WEBSERVER


/***************************************\
 *              Interface
\***************************************/

// Teleinfo sensor (for graph)
bool Xsns124 (uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function) 
  {
    case FUNC_INIT:
      TeleinfoHistoInit ();
      break;

    case FUNC_SAVE_BEFORE_RESTART:
      TeleinfoHistoSaveBeforeRestart ();
      break;

    case FUNC_EVERY_SECOND:
      TeleinfoHistoEverySecond ();
      break;

#ifdef USE_WEBSERVER

     case FUNC_WEB_ADD_MAIN_BUTTON:
      TeleinfoHistoWebMainButton ();
      break;

    case FUNC_WEB_ADD_HANDLER:
      Webserver->on (FPSTR (D_TELEINFO_PAGE_GRAPH_CONSO), TeleinfoHistoGraphWebConso);
      Webserver->on (FPSTR (D_TELEINFO_PAGE_GRAPH_PROD),  TeleinfoHistoGraphWebProd);
      break;

#endif  // USE_WEBSERVER
  }

  return result;
}

#endif    // USE_UFILESYS

#endif    // USE_TELEINFO_HISTO
#endif    // USE_TELEINFO
