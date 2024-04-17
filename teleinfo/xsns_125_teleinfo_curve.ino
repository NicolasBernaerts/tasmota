/*
  xsns_125_teleinfo_curve.ino - France Teleinfo energy sensor graph : curves

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
#ifdef USE_TELEINFO_CURVE

/*************************************************\
 *               Variables
\*************************************************/

// declare teleinfo graph curves
#define XSNS_125                         125

#define TELEINFO_CURVE_BUFFER_MAX       1024      // history file buffer

// web URL
const char D_TELEINFO_PAGE_GRAPH[]       PROGMEM = "/graph";
const char D_TELEINFO_PAGE_GRAPH_DATA[]  PROGMEM = "/data.upd";
const char D_TELEINFO_PAGE_GRAPH_CURVE[] PROGMEM = "/curve.upd";

#ifdef USE_UFILESYS

// curve files
const char D_TELEINFO_CURVE_HEADER[]    PROGMEM = "Idx;Date;Time";
const char D_TELEINFO_CURVE_FILE_DAY[]  PROGMEM = "/teleinfo-day-%02u.csv";
const char D_TELEINFO_CURVE_FILE_WEEK[] PROGMEM = "/teleinfo-week-%02u.csv";

// curve - periods
const char kTeleinfoCurvePeriod[] PROGMEM = "Live|Day|Week";                                                                                            // period labels
const long ARR_TELEINFO_CURVE_WINDOW[] = { 300 / TELEINFO_GRAPH_SAMPLE, 86400 / TELEINFO_GRAPH_SAMPLE, 604800 / TELEINFO_GRAPH_SAMPLE, 1};                // time window between samples (sec.)

#else

// curve - periods
const char kTeleinfoCurvePeriod[] PROGMEM = "Live";                                                               // period labels
const long ARR_TELEINFO_CURVE_WINDOW[] = { 300 / TELEINFO_GRAPH_SAMPLE, 1 };                                     // time window between samples (sec.)

#endif    // USE_UFILESYS

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
  long  cosphi_sum[ENERGY_MAX_PHASES];              // sum of cos phi during refresh period

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

static struct {
  uint8_t period_max = 0;                         // graph max period to display curve

  long    papp_peak[ENERGY_MAX_PHASES];           // peak apparent power to save
  long    volt_low[ENERGY_MAX_PHASES];            // voltage to save
  long    volt_peak[ENERGY_MAX_PHASES];           // voltage to save
  long    papp[ENERGY_MAX_PHASES];                // apparent power to save
  long    pact[ENERGY_MAX_PHASES];                // active power to save
  uint8_t cosphi[ENERGY_MAX_PHASES];              // cosphi to save

  uint8_t rotate_daily  = 0;                      // index of daily file to rotate
  uint8_t rotate_weekly = 0;                      // index of weekly file to rotate

  char  str_phase[ENERGY_MAX_PHASES + 2];       // phases to displayed

  tic_graph live;                                   // live last 10mn values
} teleinfo_curve;

/*********************************************\
 *               Historisation
\*********************************************/

#ifdef USE_UFILESYS

// Get curve period start time
uint32_t TeleinfoCurveGetStartTime (const uint8_t period, const uint8_t histo)
{
  uint32_t start_time, delta_time;
  TIME_T   start_dst;

  // start date
  start_time = LocalTime ();
  if (period == TELEINFO_CURVE_DAY) start_time -= 86400 * (uint32_t)histo;
  else if (period == TELEINFO_CURVE_WEEK) start_time -= 604800 * (uint32_t)histo;

  // set to beginning of current day and convert back to localtime
  BreakTime (start_time, start_dst);
  start_dst.hour   = 0;
  start_dst.minute = 0;
  start_dst.second = 0;
  start_time = MakeTime (start_dst);

  // if weekly period, start from monday
  if (period == TELEINFO_CURVE_WEEK)
  {
    delta_time = ((start_dst.day_of_week + 7) - 2) % 7;
    start_time -= delta_time * 86400;
  }

  return start_time;
}

// Get curve period literal date
void TeleinfoCurveGetDate (const int period, const int histo, char* pstr_text)
{
  uint32_t calc_time;
  TIME_T   start_dst;

  // check parameters and init
  if (pstr_text == nullptr) return;

  // start date
  calc_time = TeleinfoCurveGetStartTime (period, histo);
  BreakTime (calc_time, start_dst);

  // set label according to period
  if (period == TELEINFO_CURVE_DAY) sprintf (pstr_text, "%02u/%02u", start_dst.day_of_month, start_dst.month);
  else if (period == TELEINFO_CURVE_WEEK) sprintf (pstr_text, "W %02u/%02u", start_dst.day_of_month, start_dst.month);
  else strcpy (pstr_text, "");
}

// Get curve data filename
bool TeleinfoCurveGetFilename (const uint8_t period, const uint8_t histo, char* pstr_filename)
{
  bool exists = false;

  // check parameters
  if (pstr_filename == nullptr) return false;

  // generate filename according to period
  strcpy (pstr_filename, "");
  if (period == TELEINFO_CURVE_DAY) sprintf_P (pstr_filename, D_TELEINFO_CURVE_FILE_DAY, histo);
  else if (period == TELEINFO_CURVE_WEEK) sprintf_P (pstr_filename, D_TELEINFO_CURVE_FILE_WEEK, histo);

  // if filename defined, check existence
  if (strlen (pstr_filename) > 0) exists = ffsp->exists (pstr_filename);

  return exists;
}

// Flush curve data to littlefs
void TeleinfoCurveFlushData (const uint8_t period)
{
  uint8_t phase, index;
  char    str_value[32];
  String  str_header;
  File    file;

  // validate parameters
  if ((period != TELEINFO_CURVE_DAY) && (period != TELEINFO_CURVE_WEEK)) return;

  // if buffer is filled, save buffer to log
  if (teleinfo_period[period].str_buffer.length () > 0)
  {
    // if file doesn't exist, create it and append header
    if (!ffsp->exists (teleinfo_period[period].str_filename.c_str ()))
    {
      // create file
      file = ffsp->open (teleinfo_period[period].str_filename.c_str (), "w");

      // create header
      str_header = D_TELEINFO_CURVE_HEADER;
      for (phase = 1; phase <= teleinfo_contract.phase; phase++)
      {
        sprintf (str_value, ";VA%d;W%d;V%d;C%d;pVA%d;pV%d", phase, phase, phase, phase, phase, phase);
        str_header += str_value;
      }

      // append contract period and totals
      str_header += ";Period";
      for (index = 0; index < teleinfo_contract.period_qty; index++)
      {
        TeleinfoPeriodGetCode (str_value, sizeof (str_value));
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
void TeleinfoCurveSaveData (const uint8_t period)
{
  uint8_t  phase;
  long     year, month, remain;
  uint32_t start_time, current_time, day_of_week, index;
  TIME_T   time_dst, start_dst;
  char     str_value[32];
  char     str_filename[UFS_FILENAME_SIZE];

  // check boundaries
  if ((period != TELEINFO_CURVE_DAY) && (period != TELEINFO_CURVE_WEEK)) return;

  // extract current time data
  current_time = LocalTime ();
  BreakTime (current_time, time_dst);

  // if saving daily record
  if (period == TELEINFO_CURVE_DAY)
  {
    // set current weekly file name
    sprintf_P (str_filename, D_TELEINFO_CURVE_FILE_DAY, 0);

    // calculate index of daily line
    index = (time_dst.hour * 3600 + time_dst.minute * 60) * TELEINFO_GRAPH_SAMPLE / 86400;
  }

  // else if saving weekly record
  else if (period == TELEINFO_CURVE_WEEK)
  {
    // set current weekly file name
    sprintf_P (str_filename, D_TELEINFO_CURVE_FILE_WEEK, 0);

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
  if ((teleinfo_period[period].str_filename != "") && (teleinfo_period[period].str_filename != str_filename)) TeleinfoCurveFlushData (period);

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
    if (teleinfo_curve.papp[phase] != LONG_MAX) teleinfo_period[period].str_buffer += teleinfo_curve.papp[phase];

    // active power
    teleinfo_period[period].str_buffer += ";";
    if (teleinfo_curve.pact[phase] != LONG_MAX) teleinfo_period[period].str_buffer += teleinfo_curve.pact[phase];

    // lower voltage
    teleinfo_period[period].str_buffer += ";";
    teleinfo_period[period].str_buffer += teleinfo_curve.volt_low[phase];

    // cos phi
    teleinfo_period[period].str_buffer += ";";
    teleinfo_period[period].str_buffer += teleinfo_curve.cosphi[phase];

    // peak apparent power
    teleinfo_period[period].str_buffer += ";";
    if (teleinfo_curve.papp_peak[phase] != LONG_MAX) teleinfo_period[period].str_buffer += teleinfo_curve.papp_peak[phase];

    // peak voltage
    teleinfo_period[period].str_buffer += ";";
    teleinfo_period[period].str_buffer += teleinfo_curve.volt_peak[phase];
  }

  // line : period name
  TeleinfoPeriodGetCode (str_value, sizeof (str_value));
  teleinfo_period[period].str_buffer += ";";
  teleinfo_period[period].str_buffer += str_value;

  // line : totals
  for (index = 0; index < teleinfo_contract.period_qty; index++)
  {
    lltoa (teleinfo_conso.index_wh[index], str_value, 10); 
    teleinfo_period[period].str_buffer += ";";
    teleinfo_period[period].str_buffer += str_value;
  }

  // line : end
  teleinfo_period[period].str_buffer += "\n";

  // if log should be saved now
  if (teleinfo_period[period].str_buffer.length () > TELEINFO_CURVE_BUFFER_MAX) TeleinfoCurveFlushData (period);
}

// rotate file according to file naming convention
void TeleinfoCurveFileRotate (const char* pstr_filename, const uint8_t index) 
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
    AddLog (LOG_LEVEL_DEBUG, PSTR ("UFS: database %s renamed to %s"), str_original, str_target);
  }
}

// Rotation of data files
void TeleinfoCurveRotate ()
{
  // flush daily file and prepare rotation of daily files
  TeleinfoCurveFlushData (TELEINFO_CURVE_DAY);
  teleinfo_curve.rotate_daily  = (uint8_t)teleinfo_config.param[TELEINFO_CONFIG_NBDAY];
  AddLog (LOG_LEVEL_INFO, PSTR ("TIC: %u daily files will rotate"), teleinfo_curve.rotate_daily);

  // flush daily file and if we are on monday, prepare rotation of weekly files
  TeleinfoCurveFlushData (TELEINFO_CURVE_WEEK);
  if (RtcTime.day_of_week == 2)
  {
    teleinfo_curve.rotate_weekly = (uint8_t)teleinfo_config.param[TELEINFO_CONFIG_NBWEEK];
    AddLog (LOG_LEVEL_INFO, PSTR ("TIC: %u weekly files will rotate"), teleinfo_curve.rotate_weekly);
  }
}

#endif    // USE_UFILESYS

/*********************************************\
 *                   Save
\*********************************************/

// Save current values to graph data
void TeleinfoCurveSavePeriod (const uint8_t period)
{
  uint8_t phase;
  long    power;

  // if period out of range, return
  if (period >= TELEINFO_PERIOD_MAX) return;
  if (ARR_TELEINFO_CURVE_WINDOW[period] == 0) return;

#ifdef USE_UFILESYS
  if (period < TELEINFO_HISTO_CONSO)
#endif    // USE_UFILESYS

    // loop thru phases to update period totals  
    for (phase = 0; phase < teleinfo_contract.phase; phase++)
    {
      //   Apparent power
      // -----------------

      // save average and peak value over the period
      power = LONG_MAX;
      if (teleinfo_contract.ssousc > 0) power = (long)(teleinfo_period[period].papp_sum[phase] / ARR_TELEINFO_CURVE_WINDOW[period]);
      teleinfo_curve.papp[phase] = power;
      teleinfo_curve.papp_peak[phase] = teleinfo_period[period].papp_peak[phase];

      // reset period data
      teleinfo_period[period].papp_sum[phase]  = 0;
      teleinfo_period[period].papp_peak[phase] = 0;

      //   Active power
      // -----------------

      // save average value over the period
      power = LONG_MAX;
      if (teleinfo_contract.ssousc > 0) power = (long)(teleinfo_period[period].pact_sum[phase] / ARR_TELEINFO_CURVE_WINDOW[period]);
      if (power > teleinfo_curve.papp[phase]) power = teleinfo_curve.papp[phase];
      teleinfo_curve.pact[phase] = power;

      // reset period data
      teleinfo_period[period].pact_sum[phase] = 0;

      //   Voltage
      // -----------

      // save graph current value
      teleinfo_curve.volt_low[phase]  = teleinfo_period[period].volt_low[phase];
      teleinfo_curve.volt_peak[phase] = teleinfo_period[period].volt_peak[phase];

      // reset period data
      teleinfo_period[period].volt_low[phase]  = LONG_MAX;
      teleinfo_period[period].volt_peak[phase] = 0;

      //   CosPhi
      // -----------

      // save average value over the period
      teleinfo_curve.cosphi[phase] = (uint8_t)(teleinfo_period[period].cosphi_sum[phase] / ARR_TELEINFO_CURVE_WINDOW[period]);

      // reset period _
      teleinfo_period[period].cosphi_sum[phase] = 0;
    }

  // save to memory array
  if (period == TELEINFO_CURVE_LIVE) TeleinfoCurveSaveLiveData ();

  // save to historisation file
#ifdef USE_UFILESYS
  else TeleinfoCurveSaveData (period);
#endif    // USE_UFILESYS

  // data updated for period
  teleinfo_period[period].updated = true;
}

// Save current values to graph data
void TeleinfoCurveSaveLiveData ()
{
  uint8_t  phase;
  uint16_t cell_index;
  long     apparent, active, voltage;

  // set array index and current index of cell in memory array
  cell_index = teleinfo_period[TELEINFO_CURVE_LIVE].index;

  // loop thru phases
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    // calculate graph values
    voltage = 128 + teleinfo_curve.volt_low[phase] - TELEINFO_VOLTAGE;
    if (teleinfo_contract.ssousc > 0)
    {
      apparent = teleinfo_curve.papp[phase] * 200 / teleinfo_contract.ssousc;
      active   = teleinfo_curve.pact[phase] * 200 / teleinfo_contract.ssousc;
    } 
    else
    {
      apparent = UINT8_MAX;
      active   = UINT8_MAX;
    } 

    // update live array
    teleinfo_curve.live.arr_papp[phase][cell_index]   = (uint8_t)apparent;
    teleinfo_curve.live.arr_pact[phase][cell_index]   = (uint8_t)active;
    teleinfo_curve.live.arr_volt[phase][cell_index]   = (uint8_t)voltage;
    teleinfo_curve.live.arr_cosphi[phase][cell_index] = teleinfo_curve.cosphi[phase];
  }

  // increase data index in the graph and set update flag
  cell_index++;
  teleinfo_period[TELEINFO_CURVE_LIVE].index = cell_index % TELEINFO_GRAPH_SAMPLE;
}

// Save data before ESP restart
void TeleinfoCurveSaveData ()
{
#ifdef USE_UFILESYS
  TeleinfoCurveFlushData (TELEINFO_CURVE_DAY);
  TeleinfoCurveFlushData (TELEINFO_CURVE_WEEK);
#endif      // USE_UFILESYS
}

/*********************************************\
 *                   Callback
\*********************************************/

void TeleinfoCurveInit ()
{
  uint8_t  period, phase;
  uint16_t index;

  // initialise graph data
  strcpy (teleinfo_curve.str_phase, "");

  // maximum period to display curve
#ifdef USE_UFILESYS
  teleinfo_curve.period_max = TELEINFO_HISTO_CONSO;
#else
  teleinfo_curve.period_max = TELEINFO_PERIOD_MAX;
#endif      // USE_UFILESYS

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
  for (phase = 0; phase < ENERGY_MAX_PHASES; phase++)
  {
    // data of historisation files
    teleinfo_curve.papp[phase]      = 0;
    teleinfo_curve.pact[phase]      = 0;
    teleinfo_curve.volt_low[phase]  = LONG_MAX;
    teleinfo_curve.volt_peak[phase] = 0;
    teleinfo_curve.cosphi[phase]    = 100;
    
    // init live values
    for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
    {
      teleinfo_curve.live.arr_pact[phase][index]   = UINT8_MAX;
      teleinfo_curve.live.arr_papp[phase][index]   = UINT8_MAX;
      teleinfo_curve.live.arr_volt[phase][index]   = UINT8_MAX;
      teleinfo_curve.live.arr_cosphi[phase][index] = UINT8_MAX;
    } 
  }
}

// Save data in case of planned restart
void TeleinfoCurveSaveBeforeRestart ()
{
  // save curve data
  TeleinfoCurveSaveData ();
}

// Rotation of data files
void TeleinfoCurveSaveAtMidnight ()
{
#ifdef USE_UFILESYS
  TeleinfoCurveRotate ();
#endif    // USE_UFILESYS
}

// called every second
void TeleinfoCurveEverySecond ()
{
  uint8_t  period, phase;

  // handle only if time is valid and minimum messages received
  if (!RtcTime.valid) return;
  if (teleinfo_meter.nb_message < TELEINFO_MESSAGE_MIN) return;

  // loop thru the periods and the phases, to update all values over the period
  for (period = 0; period < teleinfo_curve.period_max; period++)
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
        teleinfo_period[period].cosphi_sum[phase] += teleinfo_conso.phase[phase].cosphi / 10;

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
    teleinfo_period[period].counter = teleinfo_period[period].counter % ARR_TELEINFO_CURVE_WINDOW[period];
    if (teleinfo_period[period].counter == 0) TeleinfoCurveSavePeriod (period);
  }

#ifdef USE_UFILESYS
  // if needed, rotate daily file
  if (teleinfo_curve.rotate_daily > 0) TeleinfoCurveFileRotate (D_TELEINFO_CURVE_FILE_DAY, teleinfo_curve.rotate_daily--);

  // else, if needed, rotate daily file
  else if (teleinfo_curve.rotate_weekly > 0) TeleinfoCurveFileRotate (D_TELEINFO_CURVE_FILE_WEEK, teleinfo_curve.rotate_weekly--);
#endif      // USE_UFILESYS
}

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

// Graph frame
void TeleinfoCurveDisplayFrame (const uint8_t data_type, const uint8_t month, const uint8_t day)
{
  long    index, unit_min, unit_max, unit_range;
  char    str_unit[8];
  char    str_text[8];
  char    arr_label[5][12];
  uint8_t arr_pos[5] = {98,76,51,26,2};  

  // set labels according to data type
  switch (data_type) 
  {
    // power
    case TELEINFO_UNIT_VA:
    case TELEINFO_UNIT_W:
      for (index = 0; index < 5; index ++) TeleinfoDriverGetDataDisplay (TELEINFO_UNIT_MAX, index * teleinfo_config.max_power / 4, arr_label[index], sizeof (arr_label[index]), true);
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
      for (index = 0; index < 5; index ++) sprintf_P (arr_label[index], PSTR ("%d.%02d"), index * 25 / 100, index * 25 % 100);
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
  GetTextIndexed (str_text, sizeof (str_text), data_type, kTeleinfoGraphDisplay);

  // units graduation
  WSContentSend_P (PSTR ("<text class='power' x='%d%%' y='%d%%'>%s</text>\n"), TELEINFO_GRAPH_PERCENT_STOP + 3, 2, str_text);
  for (index = 0; index < 5; index ++) WSContentSend_P (PSTR ("<text class='power' x='%u%%' y='%u%%'>%s</text>\n"), 2, arr_pos[index],  arr_label[index]);
}

// Append Teleinfo curve button to main page
void TeleinfoCurveWebMainButton ()
{
  if (teleinfo_conso.total_wh > 0) WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>Courbes</button></form></p>\n"), D_TELEINFO_PAGE_GRAPH);
}

// Display graph frame and time marks
void TeleinfoCurveDisplayTime (const uint8_t period, const uint8_t histo)
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
    case TELEINFO_CURVE_LIVE:
      unit_width  = teleinfo_graph.width / 5;
      for (index = 1; index < 5; index++)
      {
        graph_x = teleinfo_graph.left + (index * unit_width);
        WSContentSend_P (PSTR ("<line id='l%u' class='time' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n"), index, graph_x, 1, graph_x, 99);
      }
      break;

#ifdef USE_UFILESYS
    case TELEINFO_CURVE_DAY:
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

    case TELEINFO_CURVE_WEEK:
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
void TeleinfoCurveDisplay (const uint8_t phase, const uint8_t data)
{
  bool     analyse;
  uint32_t timestart;
  int      index, index_array, count;
  long     graph_x, graph_y, graph_range, graph_delta;
  long     prev_x, prev_y, bezier_y1, bezier_y2; 
  long     value, prev_value;
  long     arr_value[TELEINFO_GRAPH_SAMPLE];
  char     str_value[36];

  // check parameters
  if (teleinfo_config.max_power == 0) return;

  // start timestamp
  timestart = millis ();

  // init array
  for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index ++) arr_value[index] = LONG_MAX;

  // collect data in temporary array
  switch (teleinfo_graph.period)
  {
    case TELEINFO_CURVE_LIVE:
      for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
      {
        // get current array index if in live memory mode
        index_array = (teleinfo_period[teleinfo_graph.period].index + index) % TELEINFO_GRAPH_SAMPLE;

        // display according to data type
        switch (data)
        {
          case TELEINFO_UNIT_VA:
            if (teleinfo_curve.live.arr_papp[phase][index_array] != UINT8_MAX) arr_value[index] = (long)teleinfo_curve.live.arr_papp[phase][index_array] * teleinfo_contract.ssousc / 200;
            break;

          case TELEINFO_UNIT_W:
            if (teleinfo_curve.live.arr_pact[phase][index_array] != UINT8_MAX) arr_value[index] = (long)teleinfo_curve.live.arr_pact[phase][index_array] * teleinfo_contract.ssousc / 200;
            break;

          case TELEINFO_UNIT_V:
            if (teleinfo_curve.live.arr_volt[phase][index_array] != UINT8_MAX) arr_value[index] = (long)TELEINFO_VOLTAGE - 128 + teleinfo_curve.live.arr_volt[phase][index_array];
            break;

          case TELEINFO_UNIT_COSPHI:
            if (teleinfo_curve.live.arr_cosphi[phase][index_array] != UINT8_MAX) arr_value[index] = (long)teleinfo_curve.live.arr_cosphi[phase][index_array];
            break;
        }
      }
      break;

#ifdef USE_UFILESYS
    case TELEINFO_CURVE_DAY:
    case TELEINFO_CURVE_WEEK:
      uint8_t  column;
      uint32_t len_buffer;
      char    *pstr_token;
      char     str_filename[UFS_FILENAME_SIZE];
      char     str_buffer[192];
      File     file;

      // if data file exists
      count = 0;
      if (TeleinfoCurveGetFilename (teleinfo_graph.period, teleinfo_graph.histo, str_filename))
      {
        // set column with data
        column = 3 + (phase * 6) + data;

        // open file and skip header
        file = ffsp->open (str_filename, "r");
        while (file.available ())
        {
          // init
          index = 0;
          index_array = INT_MAX;
          value = LONG_MAX;
          pstr_token = nullptr;

          // read next line
          len_buffer = file.readBytesUntil ('\n', str_buffer, sizeof (str_buffer));
          str_buffer[len_buffer] = 0;

          if (isdigit (str_buffer[0])) pstr_token = strtok (str_buffer, ";");
          while (pstr_token != nullptr)
          {
            // if token is defined
            if (strlen (pstr_token) > 0)
            {
              // collect current value
              if (index == 0) index_array = atoi (pstr_token);
              else if (index == column) value = atol (pstr_token);

              // maximum column reached
              if (index == column) pstr_token = nullptr;
            }

            // if index and value are valid, update value to be displayed
            if ((index_array < TELEINFO_GRAPH_SAMPLE) && (value != LONG_MAX)) arr_value[index_array] = value;

            // next token in the line
            if (pstr_token != nullptr) pstr_token = strtok (nullptr, ";");
            index++;
          }

          // receive new data every 20 lines
          if (count % 20 == 0) TeleinfoProcessRealTime ();
          count++;
        }

        // close file
        file.close ();
      }
      break;
#endif    // USE_UFILESYS
  }

  // display values
  count = 0;
  graph_x = 0;
  prev_x = LONG_MAX;
  prev_y = 0;
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
      graph_y = 0;
      graph_delta = 0;
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
            if ((graph_x - prev_x > 10) || (prev_value == value)) { bezier_y1 = graph_y; bezier_y2 = prev_y; }
              else { bezier_y1 = (prev_y + graph_y) / 2; bezier_y2 = bezier_y1; }

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
            if ((graph_x - prev_x > 10) || (prev_value == value)) { bezier_y1 = graph_y; bezier_y2 = prev_y; }
              else { bezier_y1 = (prev_y + graph_y) / 2; bezier_y2 = bezier_y1; }

            // display point
            WSContentSend_P (PSTR ("C%d %d,%d %d,%d %d "), graph_x, bezier_y1, prev_x, bezier_y2, graph_x, graph_y); 
          }
          break;
      }

      // save previous y position
      prev_x = graph_x;
      prev_y = graph_y;
    }

    // handle received data update every 20 points
    if (count % 20 == 0) TeleinfoProcessRealTime ();
    count++;
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

// calculate previous period parameters
bool TeleinfoCurvePreviousPeriod (const bool update)
{
  bool    is_file = false;
  uint8_t histo = teleinfo_graph.histo;
  char    str_filename[UFS_FILENAME_SIZE];

  // handle next according to period type
  if (teleinfo_graph.period == TELEINFO_CURVE_DAY)
  {
    while (!is_file && (histo < teleinfo_config.param[TELEINFO_CONFIG_NBDAY])) is_file = TeleinfoCurveGetFilename (TELEINFO_CURVE_DAY, ++histo, str_filename);
  }

  else if (teleinfo_graph.period == TELEINFO_CURVE_WEEK)
  {
    while (!is_file && (histo < teleinfo_config.param[TELEINFO_CONFIG_NBWEEK])) is_file = TeleinfoCurveGetFilename (TELEINFO_CURVE_WEEK, ++histo, str_filename);
  }

  // if asked, set next period
  if (is_file && update) teleinfo_graph.histo = histo;

  return is_file;
}

// check if next curve period is available
bool TeleinfoCurveNextPeriod (const bool update)
{
  bool    is_file = false;
  uint8_t histo = teleinfo_graph.histo;
  char    str_filename[UFS_FILENAME_SIZE];

  // handle next according to period type
  if (teleinfo_graph.period == TELEINFO_CURVE_DAY)
  {
    while (!is_file && (histo > 0)) is_file = TeleinfoCurveGetFilename (TELEINFO_CURVE_DAY, --histo, str_filename);
  }

  else if (teleinfo_graph.period == TELEINFO_CURVE_WEEK)
  {
    while (!is_file && (histo > 0)) is_file = TeleinfoCurveGetFilename (TELEINFO_CURVE_WEEK, --histo, str_filename);
  }

  // if asked, set next period
  if (is_file && update) teleinfo_graph.histo = histo;

  return is_file;
}

#endif    // USE_UFILESYS

// Graph public page
void TeleinfoCurveWebGraph ()
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
    teleinfo_graph.data  = (uint8_t)TeleinfoDriverGetArgValue (D_CMND_TELEINFO_DATA,  0, TELEINFO_UNIT_MAX - 1, teleinfo_graph.data);
    teleinfo_graph.histo = (uint8_t)TeleinfoDriverGetArgValue (D_CMND_TELEINFO_HISTO, 0, 52, teleinfo_graph.histo);

#ifdef USE_UFILESYS
    if (teleinfo_graph.period >= TELEINFO_HISTO_CONSO) teleinfo_graph.period = TELEINFO_CURVE_LIVE;
#endif      // USE_UFILESYS

    choice = (uint8_t)TeleinfoDriverGetArgValue (D_CMND_TELEINFO_PERIOD, 0, TELEINFO_PERIOD_MAX - 1, teleinfo_graph.period);
    if (choice != teleinfo_graph.period) teleinfo_graph.histo = 0;
    teleinfo_graph.period = choice;  

    // if unit is Wh, force to W
    if (teleinfo_graph.data == TELEINFO_UNIT_WH) teleinfo_graph.data = TELEINFO_UNIT_W;

    // check phase display argument
    if (Webserver->hasArg (D_CMND_TELEINFO_PHASE)) WebGetArg (D_CMND_TELEINFO_PHASE, teleinfo_curve.str_phase, sizeof (teleinfo_curve.str_phase));
    while (strlen (teleinfo_curve.str_phase) < teleinfo_contract.phase) strlcat (teleinfo_curve.str_phase, "1", sizeof (teleinfo_curve.str_phase));

    // graph increment
    choice = TeleinfoDriverGetArgValue (D_CMND_TELEINFO_PLUS, 0, 1, 0);
    if (choice == 1)
    {
      if ((teleinfo_graph.data == TELEINFO_UNIT_VA) || (teleinfo_graph.data == TELEINFO_UNIT_W)) teleinfo_config.max_power *= 2;
      else if (teleinfo_graph.data == TELEINFO_UNIT_V) teleinfo_config.max_volt += TELEINFO_GRAPH_INC_VOLTAGE;
    }

    // graph decrement
    choice = TeleinfoDriverGetArgValue (D_CMND_TELEINFO_MINUS, 0, 1, 0);
    if (choice == 1)
    {
      if ((teleinfo_graph.data == TELEINFO_UNIT_VA) || (teleinfo_graph.data == TELEINFO_UNIT_W)) teleinfo_config.max_power = max ((long)TELEINFO_GRAPH_MIN_POWER, teleinfo_config.max_power / 2);
      else if (teleinfo_graph.data == TELEINFO_UNIT_V) teleinfo_config.max_volt = max ((long)TELEINFO_GRAPH_MIN_VOLTAGE, teleinfo_config.max_volt - TELEINFO_GRAPH_INC_VOLTAGE);
    }

#ifdef USE_UFILESYS
    // handle previous page
    choice = TeleinfoDriverGetArgValue (D_CMND_TELEINFO_PREV, 0, 1, 0);
    if (choice == 1) TeleinfoCurvePreviousPeriod (true);

    // handle next page
    choice = TeleinfoDriverGetArgValue (D_CMND_TELEINFO_NEXT, 0, 1, 0);
    if (choice == 1) TeleinfoCurveNextPeriod (true);
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
    if (teleinfo_graph.period == TELEINFO_CURVE_LIVE) delay = 1000; else delay = 30000;

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
    if (teleinfo_graph.period != TELEINFO_CURVE_LIVE)
    {
      // previous button
      if (TeleinfoCurvePreviousPeriod (false)) strcpy (str_text, "");
        else strcpy (str_text, "disabled");
      WSContentSend_P (PSTR ("<div class='prev'><button class='navi' name='%s' id='%s' value=1 %s>&lt;&lt;</button></div>\n"), D_CMND_TELEINFO_PREV, D_CMND_TELEINFO_PREV, str_text);

      // next button
      if (TeleinfoCurveNextPeriod (false)) strcpy (str_text, "");
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
      strlcpy (str_text, teleinfo_curve.str_phase, sizeof (str_text));
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
    GetTextIndexed (str_text, sizeof (str_text), TELEINFO_CURVE_LIVE, kTeleinfoCurvePeriod);
    if (teleinfo_graph.period != TELEINFO_CURVE_LIVE) WSContentSend_P (PSTR ("<a href='%s?period=%d'>"), D_TELEINFO_PAGE_GRAPH, TELEINFO_CURVE_LIVE);
    WSContentSend_P (PSTR ("<div class='item period'>%s</div>"), str_text);
    if (teleinfo_graph.period != TELEINFO_CURVE_LIVE) WSContentSend_P (PSTR ("</a>"));
    WSContentSend_P (PSTR ("\n"));

    // other periods button
#ifdef USE_UFILESYS
    char str_filename[UFS_FILENAME_SIZE];

    for (index = TELEINFO_CURVE_DAY; index < TELEINFO_HISTO_CONSO; index++)
    {
      // get period label
      GetTextIndexed (str_text, sizeof (str_text), index, kTeleinfoCurvePeriod);

      // set button according to active state
      if (teleinfo_graph.period == index)
      {
        // get number of saved periods
        if (teleinfo_graph.period == TELEINFO_CURVE_DAY) choice = teleinfo_config.param[TELEINFO_CONFIG_NBDAY];
        else if (teleinfo_graph.period == TELEINFO_CURVE_WEEK) choice = teleinfo_config.param[TELEINFO_CONFIG_NBWEEK];
        else choice = 0;

        WSContentSend_P (PSTR ("<select name='histo' id='histo' onchange='this.form.submit();'>\n"));

        for (counter = 0; counter < choice; counter++)
        {
          // check if file exists
          if (TeleinfoCurveGetFilename (teleinfo_graph.period, counter, str_filename))
          {
            TeleinfoCurveGetDate (teleinfo_graph.period, counter, str_date);
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
    TeleinfoCurveDisplayFrame (teleinfo_graph.data, teleinfo_graph.month, teleinfo_graph.day);

    // svg : time 
    TeleinfoCurveDisplayTime (teleinfo_graph.period, teleinfo_graph.histo);

    // svg : curves
#ifdef USE_UFILESYS
    // if data is for current period file, force flush for the period
    if ((teleinfo_graph.histo == 0) && (teleinfo_graph.period != TELEINFO_CURVE_LIVE)) TeleinfoCurveFlushData (teleinfo_graph.period);
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

// Generate current counter values as a string with unit and kilo conversion
void TeleinfoCurveGetCurrentDataDisplay (const int unit_type, const int phase, char* pstr_result, const int size_result) 
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
      value = teleinfo_conso.phase[phase].cosphi / 10;
      break;
  }

  // convert value for display
  TeleinfoDriverGetDataDisplay (unit, value, pstr_result, size_result, false);
}

// Graph data update
void  TeleinfoCurveWebGraphUpdateData ()
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
      TeleinfoCurveGetCurrentDataDisplay (teleinfo_graph.data, phase, str_text, sizeof (str_text));
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
void  TeleinfoCurveWebGraphUpdateCurve ()
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
      if (teleinfo_curve.str_phase[phase] == '1') TeleinfoCurveDisplay (phase, teleinfo_graph.data);
      WSContentSend_P (PSTR (";"));
    }
      
    // loop thru phases to display peak curve
    for (phase = 0; phase < teleinfo_contract.phase; phase++)
    {
      if (teleinfo_curve.str_phase[phase] == '1')
      {
        if (teleinfo_graph.data == TELEINFO_UNIT_VA) TeleinfoCurveDisplay (phase, TELEINFO_UNIT_PEAK_VA);
          else if (teleinfo_graph.data == TELEINFO_UNIT_V) TeleinfoCurveDisplay (phase, TELEINFO_UNIT_PEAK_V);
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

#endif    // USE_WEBSERVER

/***************************************\
 *              Interface
\***************************************/

// Teleinfo sensor (for curves)
bool Xsns125 (uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function) 
  {
    case FUNC_INIT:
      TeleinfoCurveInit ();
      break;

    case FUNC_SAVE_BEFORE_RESTART:
      TeleinfoCurveSaveBeforeRestart ();
      break;

    case FUNC_EVERY_SECOND:
      TeleinfoCurveEverySecond ();
      break;

    case FUNC_SAVE_AT_MIDNIGHT:
      TeleinfoCurveSaveAtMidnight ();
      break;

#ifdef USE_WEBSERVER

     case FUNC_WEB_ADD_MAIN_BUTTON:
      TeleinfoCurveWebMainButton ();
      break;

    case FUNC_WEB_ADD_HANDLER:
      Webserver->on (FPSTR (D_TELEINFO_PAGE_GRAPH),       TeleinfoCurveWebGraph);
      Webserver->on (FPSTR (D_TELEINFO_PAGE_GRAPH_DATA),  TeleinfoCurveWebGraphUpdateData);
      Webserver->on (FPSTR (D_TELEINFO_PAGE_GRAPH_CURVE), TeleinfoCurveWebGraphUpdateCurve);
      break;

#endif  // USE_WEBSERVER
  }

  return result;
}

#endif    // USE_TELEINFO_CURVE
#endif    // USE_TELEINFO
