/*
  xdrv_98_02_teleinfo_graph.ino - France Teleinfo energy sensor graph

  Copyright (C) 2019-2025  Nicolas Bernaerts

  Version history :
    29/02/2024 v1.0 - Split graph in 3 files
    28/06/2024 v1.1 - Remove all String for ESP8266 stability
    10/03/2025 v2.0 - Rework graph data management
    30/04/2025 v2.1 - Optimize memory usage for ESP8266 
    10/07/2025 v3.0 - Refactoring based on Tasmota 15
    22/08/2025 v3.1 - Add solar production active power

  RAM : esp8266 2239 bytes
        esp32   19283 bytes

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

#define GRAPH_VERSION                 2               // saved data version

#define GRAPH_LIVE_DURATION           15              // live recording delay (in minutes)

// graph constant
#define GRAPH_LEFT                    60              // left position of the curve
#define GRAPH_WIDE                    1200            // graph curve width
#define GRAPH_RIGHT                   12              // right position of the curve
#define GRAPH_WIDTH                   1320            // graph width
#define GRAPH_HEIGHT                  600             // default graph height

// graph default and boundaries
#define GRAPH_INC_VOLTAGE             5
#define GRAPH_MIN_VOLTAGE             235
#define GRAPH_DEF_VOLTAGE             240             // default voltage maximum in graph
#define GRAPH_MAX_VOLTAGE             265
#define GRAPH_INC_POWER               3000
#define GRAPH_MIN_POWER               1000L
#define GRAPH_DEF_POWER               6000            // default power maximum consumption in graph
#define GRAPH_MAX_POWER               150000

#define GRAPH_HEAD                    "Graph"

#define CMND_GRAPH_MINUS              "minus"
#define CMND_GRAPH_PLUS               "plus"
#define CMND_GRAPH_DISPLAY            "display"
#define CMND_GRAPH_PERIOD             "period"

// web URL
const char PSTR_GRAPH_PAGE[]          PROGMEM = "/graph";
const char PSTR_GRAPH_PAGE_DATA[]     PROGMEM = "/data.upd";
const char PSTR_GRAPH_PAGE_CURVE[]    PROGMEM = "/curve.upd";

// data file
const char PSTR_GRAPH_DATA_FILE[]     PROGMEM = "/teleinfo-graph.dat";

// graph resolution and periods
#ifdef ESP32
  #define GRAPH_SAMPLE                300
  enum TeleinfoGraphPeriod                    { GRAPH_PERIOD_LIVE, GRAPH_PERIOD_DAY, GRAPH_PERIOD_WEEK, GRAPH_PERIOD_MAX };      // available graph periods
  const char kTeleinfoGraphPeriod[]   PROGMEM =       "Live"    "|"      "24 h"   "|"     "Semaine";                             // units labels
#else
  #define GRAPH_SAMPLE                100
  enum TeleinfoGraphPeriod                    { GRAPH_PERIOD_LIVE, GRAPH_PERIOD_MAX, GRAPH_PERIOD_DAY, GRAPH_PERIOD_WEEK };       // available graph periods
  const char kTeleinfoGraphPeriod[]   PROGMEM =       "Live";                                                                     // units labels
#endif    // ESP32


/****************************************\
 *                 Data
\****************************************/

// main graph data, 17 bytes
static struct {
  uint8_t  data      = TELEINFO_UNIT_VA;        // graph default data
  uint8_t  period    = GRAPH_PERIOD_LIVE;       // graph period
  uint8_t  display   = UINT8_MAX;               // mask of data to display
  long     max_volt  = GRAPH_DEF_VOLTAGE;       // maximum voltage on graph
  long     max_power = GRAPH_DEF_POWER;         // maximum power on graph
} graph_status;

// data collection structure, 138 bytes
struct tic_record {
  uint16_t  slot;                               // current slot
  long      sample;                             // number of samples used for sums

  long      arr_volt_min[TIC_PHASE_MAX];        // lowest voltage during period
  long      arr_volt_max[TIC_PHASE_MAX];        // peak high voltage during period
  long      arr_papp_max[TIC_PHASE_MAX];        // peak apparent power during period
  long      arr_cphi_sum[TIC_PHASE_MAX];        // sum of cos phi during period
  long long arr_papp_sum[TIC_PHASE_MAX];        // sum of apparent power during period
  long long arr_pact_sum[TIC_PHASE_MAX];        // sum of active power during period

  long      prod_cphi_sum;                      // sum of cos phi during period
  long long prod_papp_sum;                      // sum of apparent power during period
  long long prod_pact_sum;                      // sum of active power during period

  long long solar_pact_sum;                     // sum of solar production active power during period
  long long fcast_pact_sum;                     // sum of solar forecast active power during period
}; 
static tic_record teleinfo_record[GRAPH_PERIOD_MAX];      // data collection structure, 366 bytes

// live graph data structure, 23 bytes
struct tic_slot {
  uint8_t arr_volt_min[TIC_PHASE_MAX];          // min voltage
  uint8_t arr_volt_max[TIC_PHASE_MAX];          // max voltage
  uint8_t arr_papp_max[TIC_PHASE_MAX];          // max apparent power
  uint8_t arr_papp[TIC_PHASE_MAX];              // apparent power
  uint8_t arr_pact[TIC_PHASE_MAX];              // active power
  uint8_t arr_cphi[TIC_PHASE_MAX];              // cosphi

  uint8_t prod_papp;                            // production apparent power
  uint8_t prod_pact;                            // production active power
  uint8_t prod_cphi;                            // production cosphi

  uint8_t solar_pact;                           // solar production active power
  uint8_t fcast_pact;                           // solar forecast active power
}; 
static tic_slot tic_graph_slot[GRAPH_PERIOD_MAX][GRAPH_SAMPLE];     // live graph data, esp8266 6.9kB, ESP32 20.7kB

/*********************************************\
 *                 Recording
\*********************************************/

// reset data
void TeleinfoGraphDataInit ()
{
  uint8_t period;

  for (period = 0; period < GRAPH_PERIOD_MAX; period ++)
  {
    TeleinfoGraphInitRecord (period, UINT16_MAX);
    TeleinfoGraphInitPeriodData (period);
  }
}

void TeleinfoGraphInitRecord (const uint8_t period, const uint16_t slot)
{
  uint8_t index;

  // check boundaries
  if (period >= GRAPH_PERIOD_MAX) return;

  // init data
  teleinfo_record[period].slot = slot;
  teleinfo_record[period].sample = 0;
  for (index = 0; index < TIC_PHASE_MAX; index ++) teleinfo_record[period].arr_volt_min[index] = 0;
  for (index = 0; index < TIC_PHASE_MAX; index ++) teleinfo_record[period].arr_volt_max[index] = 0;
  for (index = 0; index < TIC_PHASE_MAX; index ++) teleinfo_record[period].arr_papp_max[index] = 0;
  for (index = 0; index < TIC_PHASE_MAX; index ++) teleinfo_record[period].arr_cphi_sum[index] = 0;
  for (index = 0; index < TIC_PHASE_MAX; index ++) teleinfo_record[period].arr_papp_sum[index] = 0;
  for (index = 0; index < TIC_PHASE_MAX; index ++) teleinfo_record[period].arr_pact_sum[index] = 0;
  teleinfo_record[period].prod_cphi_sum  = 0;
  teleinfo_record[period].prod_papp_sum  = 0;
  teleinfo_record[period].prod_pact_sum  = 0;
  teleinfo_record[period].solar_pact_sum = 0;
  teleinfo_record[period].fcast_pact_sum = 0;
}

void TeleinfoGraphInitPeriodSlot (const uint8_t period, const uint16_t slot)
{
  uint8_t phase;

  // check boundaries
  if (period >= GRAPH_PERIOD_MAX) return;
  if (slot >= GRAPH_SAMPLE) return;

  // loop thru conso phases
  for (phase = 0; phase < TIC_PHASE_MAX; phase ++)
  {
    tic_graph_slot[period][slot].arr_volt_min[phase] = UINT8_MAX;
    tic_graph_slot[period][slot].arr_volt_max[phase] = UINT8_MAX;
    tic_graph_slot[period][slot].arr_papp_max[phase] = UINT8_MAX;
    tic_graph_slot[period][slot].arr_papp[phase]     = UINT8_MAX;
    tic_graph_slot[period][slot].arr_pact[phase]     = UINT8_MAX;
    tic_graph_slot[period][slot].arr_cphi[phase]     = UINT8_MAX;
  }

  // prod, solar and forecast
  tic_graph_slot[period][slot].prod_papp  = UINT8_MAX;
  tic_graph_slot[period][slot].prod_pact  = UINT8_MAX;
  tic_graph_slot[period][slot].prod_cphi  = UINT8_MAX;
  tic_graph_slot[period][slot].solar_pact = UINT8_MAX;
  tic_graph_slot[period][slot].fcast_pact = UINT8_MAX;
}

void TeleinfoGraphInitPeriodData (const uint8_t period)
{
  uint16_t slot;

  // check boundaries
  if (period >= GRAPH_PERIOD_MAX) return;

  // loop thru samples
  for (slot = 0; slot < GRAPH_SAMPLE; slot ++) TeleinfoGraphInitPeriodSlot (period, slot);
}

uint16_t TeleinfoGraphRecordingGetSlot (const uint8_t period, const uint8_t weekday, const uint8_t hour, const uint8_t minute, const uint8_t second)
{
  uint32_t delay, slot;

  // check parameters
  if (period >= GRAPH_PERIOD_MAX) return UINT16_MAX;
  if (hour    > 23) return UINT16_MAX;
  if (minute  > 59) return UINT16_MAX;
  if (second  > 59) return UINT16_MAX;

  switch (period) 
  {
    // calculate slot within 15 mn interval
    case GRAPH_PERIOD_LIVE:
      delay = (uint32_t)(minute % GRAPH_LIVE_DURATION) * 60 + (uint32_t)second;
      slot  = delay * GRAPH_SAMPLE / 60 / GRAPH_LIVE_DURATION;
      break;

    // calculate slot within day (in sec increment)
    case GRAPH_PERIOD_DAY:
      delay = (uint32_t)hour * 60 + (uint32_t)minute;
      slot  = delay * GRAPH_SAMPLE / 1440;
      break;

    // calculate slot within week (in mn increment)
    case GRAPH_PERIOD_WEEK:
      delay = (uint32_t)((weekday + 5) % 7) * 1440 + (uint32_t)hour * 60 + (uint32_t)minute;
      slot  = delay * GRAPH_SAMPLE / 10080;
      break;
  }

  return (uint16_t)slot;
}

void TeleinfoGraphRecordingUpdate (const uint8_t period)
{
  uint8_t phase;

  // check parameters
  if (period >= GRAPH_PERIOD_MAX) return;

  // increase samples
  teleinfo_record[period].sample++;

  // solar forecast data
  teleinfo_record[period].fcast_pact_sum += (long long)teleinfo_forecast.pact;

  // solar production data
  teleinfo_record[period].solar_pact_sum += (long long)teleinfo_solar.pact;

  // production data
  teleinfo_record[period].prod_pact_sum += (long long)teleinfo_prod.pact;
  teleinfo_record[period].prod_papp_sum += (long long)teleinfo_prod.papp;
  teleinfo_record[period].prod_cphi_sum += teleinfo_prod.cosphi.value / 10;

  // conso data, loop thru phases
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    if (teleinfo_record[period].arr_volt_min[phase] == 0) teleinfo_record[period].arr_volt_min[phase] = teleinfo_conso.phase[phase].voltage;
      else teleinfo_record[period].arr_volt_min[phase] = min (teleinfo_record[period].arr_volt_min[phase], teleinfo_conso.phase[phase].voltage);
    teleinfo_record[period].arr_volt_max[phase]  = max (teleinfo_record[period].arr_volt_max[phase], teleinfo_conso.phase[phase].voltage);
    teleinfo_record[period].arr_papp_max[phase]  = max (teleinfo_record[period].arr_papp_max[phase], teleinfo_conso.phase[phase].papp);
    teleinfo_record[period].arr_pact_sum[phase] += (long long)teleinfo_conso.phase[phase].pact;
    teleinfo_record[period].arr_papp_sum[phase] += (long long)teleinfo_conso.phase[phase].papp;
    teleinfo_record[period].arr_cphi_sum[phase] += teleinfo_conso.phase[phase].cosphi / 10;
  }
}

void TeleinfoGraphRecording2Slot (const uint8_t period, const uint16_t slot)
{
  uint8_t phase;

  // check parameters
  if (period >= GRAPH_PERIOD_MAX) return;
  if (slot >= GRAPH_SAMPLE) return;

  // check data validity
  if (teleinfo_contract.ssousc == 0) return;
  if (teleinfo_record[period].sample == 0) return;

  // save solar forecast data
  tic_graph_slot[period][slot].fcast_pact = (uint8_t)(teleinfo_record[period].fcast_pact_sum * 200 / teleinfo_record[period].sample / teleinfo_contract.ssousc);

  // save solar production data
  tic_graph_slot[period][slot].solar_pact = (uint8_t)(teleinfo_record[period].solar_pact_sum * 200 / teleinfo_record[period].sample / teleinfo_contract.ssousc);

  // save production data
  tic_graph_slot[period][slot].prod_papp = (uint8_t)(teleinfo_record[period].prod_papp_sum * 200 / teleinfo_record[period].sample / teleinfo_contract.ssousc);
  tic_graph_slot[period][slot].prod_pact = (uint8_t)(teleinfo_record[period].prod_pact_sum * 200 / teleinfo_record[period].sample / teleinfo_contract.ssousc);
  tic_graph_slot[period][slot].prod_cphi = (uint8_t)(teleinfo_record[period].prod_cphi_sum / teleinfo_record[period].sample);

  // save conso data
  for (phase = 0; phase < teleinfo_contract.phase; phase ++)
  {
    tic_graph_slot[period][slot].arr_volt_min[phase] = (uint8_t)(teleinfo_record[period].arr_volt_min[phase] + 128 - TIC_VOLTAGE_DEFAULT);
    tic_graph_slot[period][slot].arr_volt_max[phase] = (uint8_t)(teleinfo_record[period].arr_volt_max[phase] + 128 - TIC_VOLTAGE_DEFAULT);
    tic_graph_slot[period][slot].arr_papp_max[phase] = (uint8_t)(teleinfo_record[period].arr_papp_max[phase] * 200 / teleinfo_contract.ssousc);
    tic_graph_slot[period][slot].arr_papp[phase]     = (uint8_t)(teleinfo_record[period].arr_papp_sum[phase] * 200 / teleinfo_record[period].sample / teleinfo_contract.ssousc);
    tic_graph_slot[period][slot].arr_pact[phase]     = (uint8_t)(teleinfo_record[period].arr_pact_sum[phase] * 200 / teleinfo_record[period].sample / teleinfo_contract.ssousc);
    tic_graph_slot[period][slot].arr_cphi[phase]     = (uint8_t)(teleinfo_record[period].arr_cphi_sum[phase] / teleinfo_record[period].sample);
  }
}

/*********************************************\
 *                  Callback
\*********************************************/

void TeleinfoGraphInit ()
{
  // init data
  TeleinfoGraphDataInit ();

  // set max graph values
  graph_status.max_volt  = GRAPH_MIN_VOLTAGE + (long)Settings->teleinfo.adjust_v * 5;
  graph_status.max_power = GRAPH_MIN_POWER * (long)pow (2, (float)Settings->teleinfo.adjust_va);

#ifdef USE_UFILESYS
  uint8_t  version;
  uint32_t time_save;
  char     str_filename[24];
  File     file;

  // open file in read mode
  strcpy_P (str_filename, PSTR_GRAPH_DATA_FILE);
  if (ffsp->exists (str_filename))
  {
    file = ffsp->open (str_filename, "r");
    file.read ((uint8_t*)&version, sizeof (version));
    if (version == GRAPH_VERSION)
    {
      file.read ((uint8_t*)&time_save,      sizeof (time_save));
      file.read ((uint8_t*)&tic_graph_slot, sizeof (tic_graph_slot));
    }
    file.close ();
  }
#endif    // USE_UFILESYS
}

// called every second
void TeleinfoGraphEverySecond ()
{
  uint8_t  period;
  uint16_t slot;

  // handle only if time is valid and minimum messages received
  if (!RtcTime.valid) return;
  if (teleinfo_meter.nb_message < TIC_MESSAGE_MIN) return;

  // save live data in case of slot change
  for (period = 0; period < GRAPH_PERIOD_MAX; period ++)
  {
    slot = TeleinfoGraphRecordingGetSlot (period, RtcTime.day_of_week, RtcTime.hour, RtcTime.minute, RtcTime.second);
    if (teleinfo_record[period].slot == UINT16_MAX) teleinfo_record[period].slot = slot;
    if (slot != teleinfo_record[period].slot)
    {
      TeleinfoGraphRecording2Slot (period, teleinfo_record[period].slot);
      TeleinfoGraphInitRecord (period, slot);
    }
  }

  // update live collection according to time window
  TeleinfoGraphRecordingUpdate (GRAPH_PERIOD_LIVE);
  if ((RtcTime.second % 4)  == 0) TeleinfoGraphRecordingUpdate (GRAPH_PERIOD_DAY);
  if ((RtcTime.second % 15) == 0) TeleinfoGraphRecordingUpdate (GRAPH_PERIOD_WEEK);
}

void TeleinfoGraphSaveBeforeRestart ()
{
  // save max graph values
  Settings->teleinfo.adjust_v  = (graph_status.max_volt - GRAPH_MIN_VOLTAGE) / 5;
  Settings->teleinfo.adjust_va = (uint32_t)sqrt (graph_status.max_power / GRAPH_MIN_POWER);

#ifdef USE_UFILESYS
  uint8_t  version;
  uint32_t time_now;
  char     str_filename[24];
  File     file;

  // init data
  version  = GRAPH_VERSION;
  time_now = LocalTime ();

  // open file in write mode
  strcpy_P (str_filename, PSTR_GRAPH_DATA_FILE);
  file = ffsp->open (str_filename, "w");
  if (file > 0)
  {
    file.write ((uint8_t*)&version, sizeof (version));
    file.write ((uint8_t*)&time_now, sizeof (time_now));
    file.write ((uint8_t*)&tic_graph_slot, sizeof (tic_graph_slot));
    file.close ();
  }
#endif    // USE_UFILESYS
}

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

// Graph frame
void TeleinfoGraphDisplayUnits (const uint8_t data_type)
{
  long    index, unit_min, unit_max, unit_range;
  char    str_text[8];
  char    arr_label[5][12];
  uint8_t arr_pos[5] = {98,76,51,26,2};  

  // set labels according to data type
  switch (data_type) 
  {
    // power
    case TELEINFO_UNIT_VA:
    case TELEINFO_UNIT_W:
      for (index = 0; index < 5; index ++) TeleinfoGetFormattedValue (TELEINFO_UNIT_MAX, index * graph_status.max_power / 4, arr_label[index], sizeof (arr_label[index]), true);
      break;

    // voltage
    case TELEINFO_UNIT_V:
      unit_max   = graph_status.max_volt;
      unit_range = 2 * (graph_status.max_volt - TIC_VOLTAGE_DEFAULT);
      unit_min   = unit_max - unit_range;
      for (index = 0; index < 5; index ++) ltoa (unit_min + index * unit_range / 4, arr_label[index], 10);
      break;

    // cos phi
    case TELEINFO_UNIT_COS:
      for (index = 0; index < 5; index ++) sprintf_P (arr_label[index], PSTR ("%d.%02d"), index * 25 / 100, index * 25 % 100);
      break;

    default:
      for (index = 0; index < 5; index ++) strcpy (arr_label[index], "");
      break;
  }

  // get unit label
  GetTextIndexed (str_text, sizeof (str_text), data_type, kTeleinfoUnit);

  // units graduation
  WSContentSend_P (PSTR ("<text class='power' x='%d' y='%d%%'>%s</text>\n"), GRAPH_WIDTH * 98 / 100, 2, str_text);
  for (index = 0; index < 5; index ++) WSContentSend_P (PSTR ("<text class='power' x='%u%%' y='%u%%'>%s</text>\n"), 2, arr_pos[index],  arr_label[index]);
}

// Append Teleinfo curve button to main page
void TeleinfoGraphWebMainButton ()
{
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s %s</button></form></p>\n"), PSTR_GRAPH_PAGE, PSTR (TIC_TELEINFO), PSTR ("Courbes"));
}

// Display graph time marks
void TeleinfoGraphDisplayTime ()
{
  uint32_t index, hour, minute, doweek;
  uint32_t unit_width, graph_x, bar_x, bar_width;
  TIME_T   time_dst;
  char     str_text[8];

  // get hour and minute
  BreakTime (LocalTime (), time_dst);
  hour   = (uint32_t)time_dst.hour;
  minute = (uint32_t)time_dst.minute;
  doweek = (uint32_t)(time_dst.day_of_week % 7);

  // handle graph units according to period
  switch (graph_status.period) 
  {
    case GRAPH_PERIOD_LIVE:
      // start data
      unit_width = GRAPH_WIDE / GRAPH_LIVE_DURATION;

      // loop thru hour slots
      for (index = 0; index < GRAPH_LIVE_DURATION / 2; index++)
      {
        graph_x = GRAPH_LEFT + unit_width + (2 * index * unit_width);
        WSContentSend_P (PSTR ("<rect class='dash' x=%u y=0 width=%u height=%u />\n"), graph_x, unit_width, GRAPH_HEIGHT);
      }

      // loop to display units
      for (index = 1; index < 3; index++)
      {
        graph_x = GRAPH_LEFT + index * GRAPH_WIDE / 3;
        minute = (3 - index) * GRAPH_LIVE_DURATION / 3;
        WSContentSend_P (PSTR ("<text class='time' x=%u y=20>-%u mn</text>\n"), graph_x, minute);
        WSContentSend_P (PSTR ("<line class='dash' x1=%u y1=40 x2=%u y2=%u></line>\n"), graph_x, graph_x, GRAPH_HEIGHT);
      }
      break;

    case GRAPH_PERIOD_DAY:
      // start data
      unit_width = GRAPH_WIDE / 24;
      graph_x    = GRAPH_LEFT + GRAPH_WIDE * (240 - (hour % 4 * 60 + minute)) / 1440; 
      hour       = (hour / 4 + 1) * 4 % 24;

      // loop to display time by 4h blocs
      for (index = 0; index < 6; index++)
      {
        // display bloc and label
        bar_x = max (graph_x - unit_width, (uint32_t)GRAPH_LEFT);
        if (graph_x < GRAPH_LEFT + unit_width) bar_width = graph_x + unit_width - GRAPH_LEFT;
          else if (graph_x > GRAPH_LEFT + GRAPH_WIDE - unit_width) bar_width = GRAPH_LEFT + GRAPH_WIDE + unit_width - graph_x;
          else bar_width = unit_width * 2;
        WSContentSend_P (PSTR ("<rect class='dash' x=%u y=0 width=%u height=%u />\n"), bar_x, bar_width, GRAPH_HEIGHT);
        WSContentSend_P (PSTR ("<text class='time' x=%u y=20>%02uh</text>\n"), graph_x, hour);
        WSContentSend_P (PSTR ("<line class='dash' x1=%u y1=40 x2=%u y2=%u></line>\n"), graph_x, graph_x, GRAPH_HEIGHT);

        // increase to next separation
        graph_x += unit_width * 4;
        hour     = (hour + 4) % 24;
      }
      break;

    case GRAPH_PERIOD_WEEK:
      // start data
      unit_width = GRAPH_WIDE / 14;
      graph_x = GRAPH_LEFT + GRAPH_WIDE * (1440 - (hour * 60 + minute)) / 10080; 

      // loop thru days
      for (index = 0; index < 7; index++)
      {
        // display bloc
        bar_width = min (unit_width * 2, (uint32_t)(GRAPH_LEFT + GRAPH_WIDE) - graph_x);
        if (index % 2 == 0) WSContentSend_P (PSTR ("<rect class='dash' x=%u y=0 width=%u height=%u />\n"), graph_x, bar_width, GRAPH_HEIGHT);

        // display label
        graph_x += unit_width;
        if (graph_x > GRAPH_LEFT + GRAPH_WIDE) graph_x -= GRAPH_WIDE;
        strlcpy (str_text, D_DAY3LIST + doweek * 3, 4);
        if ((graph_x > GRAPH_LEFT + 20) && (graph_x < GRAPH_LEFT + GRAPH_WIDE - 20)) WSContentSend_P (PSTR ("<text class='time' x=%u y=20>%s</text>\n"), graph_x, str_text);

        // increase to next separation
        graph_x += unit_width;
        doweek   = (doweek + 1) % 7;
      }
      break;
  }
}

// Display graph level lines
void TeleinfoGraphDisplayLevel ()
{
  uint32_t index;

  // graph level lines
  for (index = 1; index < 4; index ++) WSContentSend_P (PSTR ("<line class='dash' x1=%u y1=%u x2=%u y2=%u />\n"), GRAPH_LEFT, index * GRAPH_HEIGHT / 4, GRAPH_LEFT + GRAPH_WIDE, index * GRAPH_HEIGHT / 4);
}

// Display data curve
void TeleinfoGraphDisplayCurve (const uint8_t data, const uint8_t period, const uint8_t phase)
{
  uint16_t index, index_array, start_slot;
  long     value, graph_x, graph_y, graph_range, graph_delta;
  long     prev_value, prev_x, prev_y; 
  long     arr_value[GRAPH_SAMPLE];

  // check parameters
  if (!RtcTime.valid) return;
  if (graph_status.max_power == 0) return;
  if (data >= TELEINFO_UNIT_MAX) return;
  if (phase >= TIC_PHASE_END) return;
  if ((phase == TIC_PHASE_PROD) && !teleinfo_prod.enabled) return;
  if ((phase == TIC_PHASE_PROD) && (data == TELEINFO_UNIT_V)) return;
  if ((phase == TIC_PHASE_SOLAR) && !teleinfo_solar.enabled) return;
  if ((phase == TIC_PHASE_SOLAR) && (data != TELEINFO_UNIT_W)) return;
  if ((phase == TIC_PHASE_FORECAST) && !teleinfo_forecast.enabled) return;
  if ((phase == TIC_PHASE_FORECAST) && (data != TELEINFO_UNIT_W)) return;

  // init array of values
  for (index = 0; index < GRAPH_SAMPLE; index ++) arr_value[index] = LONG_MAX;

  // get start slot
  start_slot = teleinfo_record[period].slot;

  // loop to load array
  for (index = 0; index < GRAPH_SAMPLE; index++)
  {
    // get current array index
    index_array = (start_slot + index) % GRAPH_SAMPLE;

    // solar forecast production data
    if ((phase == TIC_PHASE_FORECAST) && (data == TELEINFO_UNIT_W) && (tic_graph_slot[period][index_array].fcast_pact != UINT8_MAX))
      arr_value[index] = (long)tic_graph_slot[period][index_array].fcast_pact * teleinfo_contract.ssousc / 200;

    // solar production data
    else if ((phase == TIC_PHASE_SOLAR) && (data == TELEINFO_UNIT_W) && (tic_graph_slot[period][index_array].solar_pact != UINT8_MAX))
      arr_value[index] = (long)tic_graph_slot[period][index_array].solar_pact * teleinfo_contract.ssousc / 200;

    // meter production data
    else if (phase == TIC_PHASE_PROD) switch (data)
    {
      case TELEINFO_UNIT_VA:  if (tic_graph_slot[period][index_array].prod_papp != UINT8_MAX) arr_value[index] = (long)tic_graph_slot[period][index_array].prod_papp * teleinfo_contract.ssousc / 200; break;
      case TELEINFO_UNIT_W:   if (tic_graph_slot[period][index_array].prod_pact != UINT8_MAX) arr_value[index] = (long)tic_graph_slot[period][index_array].prod_pact * teleinfo_contract.ssousc / 200; break;
      case TELEINFO_UNIT_COS: if (tic_graph_slot[period][index_array].prod_cphi != UINT8_MAX) arr_value[index] = (long)tic_graph_slot[period][index_array].prod_cphi; break;
    }

    // conso data
    else switch (data)
    {
      case TELEINFO_UNIT_VA:    if (tic_graph_slot[period][index_array].arr_papp[phase]     != UINT8_MAX) arr_value[index] = (long)tic_graph_slot[period][index_array].arr_papp[phase] * teleinfo_contract.ssousc / 200; break;
      case TELEINFO_UNIT_VAMAX: if (tic_graph_slot[period][index_array].arr_papp_max[phase] != UINT8_MAX) arr_value[index] = (long)tic_graph_slot[period][index_array].arr_papp_max[phase] * teleinfo_contract.ssousc / 200; break;
      case TELEINFO_UNIT_W:     if (tic_graph_slot[period][index_array].arr_pact[phase]     != UINT8_MAX) arr_value[index] = (long)tic_graph_slot[period][index_array].arr_pact[phase] * teleinfo_contract.ssousc / 200; break;
      case TELEINFO_UNIT_V:     if (tic_graph_slot[period][index_array].arr_volt_min[phase] != UINT8_MAX) arr_value[index] = (long)TIC_VOLTAGE_DEFAULT - 128 + tic_graph_slot[period][index_array].arr_volt_min[phase]; break;
      case TELEINFO_UNIT_VMAX:  if (tic_graph_slot[period][index_array].arr_volt_max[phase] != UINT8_MAX) arr_value[index] = (long)TIC_VOLTAGE_DEFAULT - 128 + tic_graph_slot[period][index_array].arr_volt_max[phase]; break;
      case TELEINFO_UNIT_COS:   if (tic_graph_slot[period][index_array].arr_cphi[phase]     != UINT8_MAX) arr_value[index] = (long)tic_graph_slot[period][index_array].arr_cphi[phase]; break;
    }
  }

  // init display
  prev_x  = LONG_MAX;
  prev_y  = 0;
  graph_x = 0;

  // loop to display points
  for (index = 0; index < GRAPH_SAMPLE; index++)
  {
    // get previous value
    value = arr_value[index];
    if (index == 0) prev_value = arr_value[0];
      else prev_value = arr_value[index - 1];

    // if curve should start
    if ((prev_x != LONG_MAX) || (value != LONG_MAX))
    {
      // calculate x position
      graph_x = GRAPH_LEFT + index * GRAPH_WIDE / GRAPH_SAMPLE;

      // calculate y position according to data
      graph_y     = 0;
      graph_delta = 0;
      switch (data)
      {
        case TELEINFO_UNIT_VA:
        case TELEINFO_UNIT_VAMAX:
        case TELEINFO_UNIT_W:
          if (value == LONG_MAX) graph_y = GRAPH_HEIGHT;
            else if (graph_status.max_power != 0) graph_y = GRAPH_HEIGHT - (value * GRAPH_HEIGHT / graph_status.max_power);
          break;

        case TELEINFO_UNIT_V:
        case TELEINFO_UNIT_VMAX:
          if (value == LONG_MAX) graph_y = prev_y;
          else
          {
            graph_range = abs (graph_status.max_volt - TIC_VOLTAGE_DEFAULT);
            if (graph_range != 0) graph_delta = (TIC_VOLTAGE_DEFAULT - value) * GRAPH_HEIGHT / 2 / graph_range;
            graph_y = (GRAPH_HEIGHT / 2) + graph_delta;
            if (graph_y < 0) graph_y = 0;
            if (graph_y > GRAPH_HEIGHT) graph_y = GRAPH_HEIGHT;  
          }
          break;

        case TELEINFO_UNIT_COS:
          if (value == LONG_MAX) graph_y = prev_y;
            else graph_y = GRAPH_HEIGHT - (value * GRAPH_HEIGHT / 100);
          break;
      }

      // display curve point
      switch (data)
      {
        // path curve
        case TELEINFO_UNIT_VA:
        case TELEINFO_UNIT_W:
        case TELEINFO_UNIT_V:
          // if first point draw start point and line to first point, else draw line to next point
          if (prev_x == LONG_MAX) WSContentSend_P (PSTR ("M%d %d L%d %d "), graph_x, GRAPH_HEIGHT, graph_x, graph_y); 
            else WSContentSend_P (PSTR ("L%d %d "), graph_x, graph_y); 
          break;

        // polyline curve
        case TELEINFO_UNIT_VAMAX:
        case TELEINFO_UNIT_VMAX:
        case TELEINFO_UNIT_COS:
          // draw point
          WSContentSend_P (PSTR ("%d,%d "), graph_x, graph_y);
          break;
      }

      // save previous y position
      prev_x = graph_x;
      prev_y = graph_y;
    }
  }

  // if curve has started, end curve
  if (prev_x != LONG_MAX) switch (data)
  {
    case TELEINFO_UNIT_VA:
    case TELEINFO_UNIT_W:
    case TELEINFO_UNIT_V:
      WSContentSend_P (PSTR ("L%d %d Z"), graph_x, GRAPH_HEIGHT); 
      break;
  }
}

// Graph dislay page
void TeleinfoGraphWebDisplayPage ()
{
  bool     display;
  uint8_t  phase, choice, counter, mask;  
  uint16_t index;
  uint32_t timestart;
  char     str_label[24];
  char     str_text[16];
  uint8_t  arr_data[4] = { TELEINFO_UNIT_VA, TELEINFO_UNIT_W, TELEINFO_UNIT_COS, TELEINFO_UNIT_V };

  // timestamp
  timestart = millis ();

  // beginning of form without authentification
  WSContentStart_P (PSTR (GRAPH_HEAD), false);

  // check data selection argument
  if (Webserver->hasArg (F (CMND_TIC_DATA))) graph_status.data = (uint8_t)TeleinfoGetArgValue (CMND_TIC_DATA, 0, TELEINFO_UNIT_MAX - 1, graph_status.data);

  // check period selection argument
  if (Webserver->hasArg (F (CMND_GRAPH_PERIOD))) graph_status.period = (uint8_t)TeleinfoGetArgValue (CMND_GRAPH_PERIOD, 0, GRAPH_PERIOD_MAX - 1, graph_status.period);  

  // check phase display argument
  if (Webserver->hasArg (F (CMND_GRAPH_DISPLAY)))
  {
    WebGetArg (PSTR (CMND_GRAPH_DISPLAY), str_text, sizeof (str_text));
    graph_status.display = (uint8_t)atoi (str_text);
  }

  // graph increment
  if (TeleinfoGetArgValue (CMND_GRAPH_PLUS, 0, 1, 0) == 1)
  {
    if ((graph_status.data == TELEINFO_UNIT_VA) || (graph_status.data == TELEINFO_UNIT_W)) graph_status.max_power *= 2;
      else if (graph_status.data == TELEINFO_UNIT_V) graph_status.max_volt += GRAPH_INC_VOLTAGE;
  }

  // graph decrement
  if (TeleinfoGetArgValue (CMND_GRAPH_MINUS, 0, 1, 0) == 1)
  {
    if ((graph_status.data == TELEINFO_UNIT_VA) || (graph_status.data == TELEINFO_UNIT_W)) graph_status.max_power = max (GRAPH_MIN_POWER, graph_status.max_power / 2);
      else if (graph_status.data == TELEINFO_UNIT_V) graph_status.max_volt = max ((long)GRAPH_MIN_VOLTAGE, graph_status.max_volt - GRAPH_INC_VOLTAGE);
  }

  // javascript : remove parameters
  WSContentSend_P (PSTR ("\n\nvar newURL = location.href.split('?')[0];\n"));
  WSContentSend_P (PSTR ("window.history.pushState('object', document.title, newURL);\n"));

  //   data update
  //   -> messages, phase 1, phase 2, phase 3, prod, solar, forecast
  // ----------------------------------------
  counter = 0;
  WSContentSend_P (PSTR ("\nfunction updateData(){\n"));
  WSContentSend_P (PSTR (" httpData=new XMLHttpRequest();\n"));
  WSContentSend_P (PSTR (" httpData.open('GET','%s',true);\n"), PSTR_GRAPH_PAGE_DATA);
  WSContentSend_P (PSTR (" httpData.onreadystatechange=function(){\n"));
  WSContentSend_P (PSTR ("  if (httpData.readyState===XMLHttpRequest.DONE){\n"));
  WSContentSend_P (PSTR ("   if (httpData.status===0 || (httpData.status>=200 && httpData.status<400)){\n"));
  WSContentSend_P (PSTR ("    arr_value=httpData.responseText.split(';');\n"));
  WSContentSend_P (PSTR ("    if (document.getElementById('msg')!=null) document.getElementById('msg').textContent=arr_value[%u];\n"), counter++ );                   // number of messages                                                                   // number of received messages
  for (phase = 0; phase < TIC_PHASE_END; phase++) 
    WSContentSend_P (PSTR ("    if (document.getElementById('v%u')!=null) document.getElementById('v%u').textContent=arr_value[%u];\n"), phase, phase, counter++ );   // phase values
  WSContentSend_P (PSTR ("   }\n"));
  WSContentSend_P (PSTR ("   setTimeout(updateData,%u);\n"), 1000);       // ask for data update every second 
  WSContentSend_P (PSTR ("  }\n"));
  WSContentSend_P (PSTR (" }\n"));
  WSContentSend_P (PSTR (" httpData.send();\n"));
  WSContentSend_P (PSTR ("}\n"));

  // ask for first data update after 100ms
  WSContentSend_P (PSTR ("setTimeout(updateData,%u);\n\n"), 100);

  //   curve update
  //   -> phase 1, phase 2, phase 3, prod, solar, forecast, peak 1, ...
  // -------------------------------------------

  counter = 0;
  WSContentSend_P (PSTR ("\nfunction updateCurve(){\n"));
  WSContentSend_P (PSTR (" httpCurve=new XMLHttpRequest();\n"));
  WSContentSend_P (PSTR (" httpCurve.open('GET','%s',true);\n"), PSTR_GRAPH_PAGE_CURVE);
  WSContentSend_P (PSTR (" httpCurve.onreadystatechange=function(){\n"));
  WSContentSend_P (PSTR ("  if (httpCurve.readyState===XMLHttpRequest.DONE){\n"));
  WSContentSend_P (PSTR ("   if (httpCurve.status===0 || (httpCurve.status>=200 && httpCurve.status<400)){\n"));
  WSContentSend_P (PSTR ("    arr_value=httpCurve.responseText.split(';');\n"));
  for (phase = 0; phase < TIC_PHASE_END; phase++)
    if (graph_status.data == TELEINFO_UNIT_COS) WSContentSend_P (PSTR ("    if (document.getElementById('m%u')!=null) document.getElementById('m%u').setAttribute('points',arr_value[%u]);\n"), phase, phase, counter++ );
      else WSContentSend_P (PSTR ("    if (document.getElementById('m%u')!=null) document.getElementById('m%u').setAttribute('d',arr_value[%u]);\n"), phase, phase, counter++ );
  for (phase = 0; phase < teleinfo_contract.phase; phase++) WSContentSend_P (PSTR ("    if (document.getElementById('p%u')!=null) document.getElementById('p%u').setAttribute('points',arr_value[%u]);\n"), phase, phase, counter++ );     // phase peak curve
  WSContentSend_P (PSTR ("   }\n"));
  if (graph_status.period == GRAPH_PERIOD_LIVE) WSContentSend_P (PSTR ("   setTimeout(updateCurve,%u);\n"), 2000);     // ask for next curve update 
  WSContentSend_P (PSTR ("  }\n"));
  WSContentSend_P (PSTR (" }\n"));
  WSContentSend_P (PSTR (" httpCurve.send();\n"));
  WSContentSend_P (PSTR ("}\n"));

  // ask for first curve update after 200ms
  WSContentSend_P (PSTR ("setTimeout(updateCurve,%u);\n\n"), 200);

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
  WSContentSend_P (PSTR ("body {color:white;background-color:%s;font-size:2.2vmin;font-family:Arial, Helvetica, sans-serif;}\n"), COLOR_BACKGROUND);
  WSContentSend_P (PSTR ("div {margin:0.3vh auto;padding:2px 0px;text-align:center;vertical-align:top;}\n"));

  WSContentSend_P (PSTR ("a {text-decoration:none;}\n"));
  WSContentSend_P (PSTR ("div a {color:white;}\n"));

  WSContentSend_P (PSTR ("div.head {font-size:3vmin;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.count {position:absolute;left:74%%;padding:6px 15px;margin-top:0.2vh;border-radius:6px;background:#333;}\n"));
  
  WSContentSend_P (PSTR ("div.range {position:absolute;left:2vw;width:2.5vw;margin:auto;border:1px #666 solid;border-radius:4px;}\n"));
  WSContentSend_P (PSTR ("div.range:hover {background:#aaa;}\n"));

  WSContentSend_P (PSTR ("div.choice {display:inline-block;padding:0px 2px;margin:0px;background:none;color:#fff;border:1px #666 solid;border-radius:6px;}\n"));
  WSContentSend_P (PSTR ("div.choice div {background:#666;}\n"));
  WSContentSend_P (PSTR ("div.choice a div {background:none;}\n"));

  WSContentSend_P (PSTR ("div.item {display:inline-block;padding:2px 20px;border-radius:4px;}\n"));
  WSContentSend_P (PSTR ("a div.item:hover {background:#aaa;}\n"));

  WSContentSend_P (PSTR ("div.live div {display:inline-block;min-width:8vw;padding:2px 10px;margin:0px 1.5vw;border-radius:6px;}\n"));

  // solar forecast, solar production and production color
  GetTextIndexed (str_text, sizeof (str_text), TIC_PHASE_FORECAST, kTeleinfoPhaseCurve);
  WSContentSend_P (PSTR ("div.live div.ph%u {color:%s;border:1px %s solid;}\n"), TIC_PHASE_FORECAST, str_text, str_text);    
  GetTextIndexed (str_text, sizeof (str_text), TIC_PHASE_SOLAR, kTeleinfoPhaseCurve);
  WSContentSend_P (PSTR ("div.live div.ph%u {color:%s;border:1px %s solid;}\n"), TIC_PHASE_SOLAR, str_text, str_text);    
  GetTextIndexed (str_text, sizeof (str_text), TIC_PHASE_PROD, kTeleinfoPhaseCurve);
  WSContentSend_P (PSTR ("div.live div.ph%u {color:%s;border:1px %s solid;}\n"), TIC_PHASE_PROD, str_text, str_text);    

  // phases color
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    GetTextIndexed (str_text, sizeof (str_text), phase, kTeleinfoPhaseCurve);
    WSContentSend_P (PSTR ("div.live div.ph%d {color:%s;border:1px %s solid;}\n"), phase, str_text, str_text);    
  }

  WSContentSend_P (PSTR ("div.live div.disabled {color:#666;border-color:#666;}\n"));

  WSContentSend_P (PSTR ("button.menu {margin:1vh;padding:5px 20px;font-size:1.2rem;background:%s;color:%s;cursor:pointer;border:none;border-radius:8px;}\n"), COLOR_BUTTON, COLOR_BUTTON_TEXT);

  WSContentSend_P (PSTR ("div.graph {width:100%%;margin:auto;margin-top:15px;}\n"));
  WSContentSend_P (PSTR ("svg.graph {width:100%%;height:60vh;}\n"));

  WSContentSend_P (PSTR ("</style>\n\n"));

  // page body
  WSContentSend_P (PSTR ("</head>\n"));
  WSContentSend_P (PSTR ("<body>\n"));

  // title and message counter
  WSContentSend_P (PSTR ("<div>\n"));
  WSContentSend_P (PSTR ("<div class='count'>✉️ <span id='msg'>%u</span></div>\n"), teleinfo_meter.nb_message);
  WSContentSend_P (PSTR ("<div class='head'>Courbes</div>\n"));
  WSContentSend_P (PSTR ("</div>\n"));

  // --------------
  //    Level 1
  // --------------

  WSContentSend_P (PSTR ("<div>\n"));               // level 1
  WSContentSend_P (PSTR ("<a href='%s?%s=1'><div class='range'>+</div></a>\n"), PSTR_GRAPH_PAGE, PSTR (CMND_GRAPH_PLUS));
  WSContentSend_P (PSTR ("<div class='choice'>\n"));

  for (index = 0; index < GRAPH_PERIOD_MAX; index ++)
  {
    GetTextIndexed (str_text, sizeof (str_text), index, kTeleinfoGraphPeriod);
    if (graph_status.period != index) WSContentSend_P (PSTR ("<a href='%s?period=%u'>"), PSTR_GRAPH_PAGE, index);
    WSContentSend_P (PSTR ("<div class='item'>%s</div>"), str_text);
    if (graph_status.period != index) WSContentSend_P (PSTR ("</a>"));
    WSContentSend_P (PSTR ("\n"));
  }
  
  WSContentSend_P (PSTR ("</div></div>\n"));        // choice & level1

  // ------------
  //    Level 2
  // ------------

  WSContentSend_P (PSTR ("<div>\n"));               // level 2
  WSContentSend_P (PSTR ("<a href='%s?%s=1'><div class='range'>-</div></a>\n"), PSTR_GRAPH_PAGE, PSTR (CMND_GRAPH_MINUS));
  WSContentSend_P (PSTR ("<div class='choice'>\n"));

  for (index = 0; index < sizeof (arr_data); index++)
  {
    // get unit label
    choice = arr_data[index];

    GetTextIndexed (str_text, sizeof (str_text), choice, kTeleinfoUnit);

    // display selection
    if (graph_status.data != choice) WSContentSend_P (PSTR ("<a href='%s?data=%d'>"), PSTR_GRAPH_PAGE, choice);
    WSContentSend_P (PSTR ("<div class='item'>%s</div>"), str_text);
    if (graph_status.data != choice) WSContentSend_P (PSTR ("</a>"));
    WSContentSend_P (PSTR ("\n"));
  }

  WSContentSend_P (PSTR ("</div></div>\n"));      // choice & level2

  WSContentSend_P (PSTR ("</form>\n"));

  // ------------
  //     SVG
  // ------------

  // svg : start 
  WSContentSend_P (PSTR ("<div class='graph'>\n"));
  WSContentSend_P (PSTR ("<svg class='graph' viewBox='0 0 %d %d' preserveAspectRatio='none'>\n"), GRAPH_WIDTH, GRAPH_HEIGHT + 1);

  // svg : main styles
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));
  
  WSContentSend_P (PSTR ("rect.main {fill:none;stroke:#888;stroke-width:1;}\n"));
  WSContentSend_P (PSTR ("rect.dash {fill:#333;fill-opacity:0.5;stroke-width:0;}\n"));
  WSContentSend_P (PSTR ("line.dash {stroke:grey;stroke-width:1;stroke-dasharray:2 6;}\n"));
  WSContentSend_P (PSTR ("line.time {stroke-width:1;stroke:white;stroke-dasharray:1 8;}\n"));
  WSContentSend_P (PSTR ("text {fill:white;text-anchor:middle;dominant-baseline:middle;}\n"));
  WSContentSend_P (PSTR ("text.time {font-style:italic;fill-opacity:0.6;}\n"));
  WSContentSend_P (PSTR ("text.power {font-size:1.2rem;}\n"));

  // svg : curves
  WSContentSend_P (PSTR ("path {fill-opacity:0.25;}\n"));
  WSContentSend_P (PSTR ("polyline {fill:none;}\n"));

  // svg : style of curves for phases, solar production, forecast and production curves
  for (phase = 0; phase < TIC_PHASE_END; phase++) 
  {
    // phase color
    GetTextIndexed (str_text, sizeof (str_text), phase, kTeleinfoPhaseCurve);

    // if curve is cosphi use polyline, else use path
    if (graph_status.data == TELEINFO_UNIT_COS) WSContentSend_P (PSTR ("polyline.ph%u {stroke:%s;}\n"), phase, str_text);
      else WSContentSend_P (PSTR ("path.ph%u {stroke:%s;fill:%s;}\n"), phase, str_text, str_text);

    // if conso pahse, set peak curve
    GetTextIndexed (str_text, sizeof (str_text), phase, kTeleinfoPhasePeak);
    if (phase < TIC_PHASE_PROD) WSContentSend_P (PSTR ("polyline.pk%u {stroke:%s;stroke-dasharray:1 3;}\n"), phase, str_text);
  }

  WSContentSend_P (PSTR ("</style>\n"));

  // svg : units
  TeleinfoGraphDisplayUnits (graph_status.data);

  // svg : time 
  TeleinfoGraphDisplayTime ();

  // svg : levels 
  TeleinfoGraphDisplayLevel ();

  // display curves of all phase types
  for (phase = 0; phase < TIC_PHASE_END; phase++) 
  {
    // cosphi : use polyline
    if (graph_status.data == TELEINFO_UNIT_COS) WSContentSend_P (PSTR ("<polyline id='m%u' class='ph%u' points='' />\n"), phase, phase);

    // VA, W and V : use path
    else WSContentSend_P (PSTR ("<path id='m%u' class='ph%u' d='' />\n"), phase, phase);

  }

  // svg : style of conso phase peaks
  for (phase = 0; phase < teleinfo_contract.phase; phase++) WSContentSend_P (PSTR ("<polyline id='p%u' class='pk%u' points='' />\n"), phase, phase);

  // svg : frame
  WSContentSend_P (PSTR ("<rect class='main' x=%d y=%d width=%d height=%d rx=10 />\n"), GRAPH_LEFT, 0, GRAPH_WIDE, GRAPH_HEIGHT + 1);

  // svg : end
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentSend_P (PSTR ("</div>\n"));

  // ------------------
  //    Live values
  // ------------------

  WSContentSend_P (PSTR ("<div><div class='live'>\n"));

  // solar production forecast
  if (teleinfo_forecast.enabled && (graph_status.data == TELEINFO_UNIT_W)) 
  {
    // display context
    mask = 0x01 << 0;
    display = (graph_status.display & mask);

    // link to invert display
    if (display) choice = graph_status.display & (mask ^ 0xff);
      else choice = graph_status.display | mask;
    if (display) strcpy (str_text, ""); else strcpy_P (str_text, PSTR (" disabled"));

    // display phase data
    GetTextIndexed (str_label, sizeof (str_label), TIC_PHASE_FORECAST, kTeleinfoPhaseLabel);
    WSContentSend_P (PSTR ("<a href='%s?%s=%u'><div id='v%u' class='ph%u%s' title='%s'>&nbsp;</div></a>\n"), PSTR_GRAPH_PAGE, PSTR (CMND_GRAPH_DISPLAY), choice, TIC_PHASE_FORECAST, TIC_PHASE_FORECAST, str_text, str_label);
  }

  // solar production
  if (teleinfo_solar.enabled && (graph_status.data == TELEINFO_UNIT_W)) 
  {
    // display context
    mask = 0x01 << 1;
    display = (graph_status.display & mask);

    // link to invert display
    if (display) choice = graph_status.display & (mask ^ 0xff);
      else choice = graph_status.display | mask;
    if (display) strcpy (str_text, ""); else strcpy_P (str_text, PSTR (" disabled"));

    // display phase data
    GetTextIndexed (str_label, sizeof (str_label), TIC_PHASE_SOLAR, kTeleinfoPhaseLabel);
    WSContentSend_P (PSTR ("<a href='%s?%s=%u'><div id='v%u' class='ph%u%s' title='%s'>&nbsp;</div></a>\n"), PSTR_GRAPH_PAGE, PSTR (CMND_GRAPH_DISPLAY), choice, TIC_PHASE_SOLAR, TIC_PHASE_SOLAR, str_text, str_label);
  }

  // production
  if (teleinfo_prod.enabled && (graph_status.data != TELEINFO_UNIT_V)) 
  {
    // display context
    mask = 0x01 << 2;
    display = (graph_status.display & mask);

    // link to invert display
    if (display) choice = graph_status.display & (mask ^ 0xff);
      else choice = graph_status.display | mask;
    if (display) strcpy (str_text, ""); else strcpy_P (str_text, PSTR (" disabled"));

    // display phase data
    GetTextIndexed (str_label, sizeof (str_label), TIC_PHASE_PROD, kTeleinfoPhaseLabel);
    WSContentSend_P (PSTR ("<a href='%s?%s=%u'><div id='v%u' class='ph%u%s' title='%s'>&nbsp;</div></a>\n"), PSTR_GRAPH_PAGE, PSTR (CMND_GRAPH_DISPLAY), choice, TIC_PHASE_PROD, TIC_PHASE_PROD, str_text, str_label);
  }

  // conso
  if (teleinfo_conso.enabled) for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    // display context
    mask = 0x01 << (phase + 3);
    display = (graph_status.display & mask);

    // link to invert display
    if (display) choice = graph_status.display & (mask ^ 0xff);
      else choice = graph_status.display | mask;
    if (display) strcpy (str_text, ""); else strcpy_P (str_text, PSTR (" disabled"));

    // display phase data
    GetTextIndexed (str_label, sizeof (str_label), phase, kTeleinfoPhaseLabel);
    WSContentSend_P (PSTR ("<a href='%s?%s=%u'><div id='v%u' class='phase ph%u%s' title='%s'>&nbsp;</div></a>\n"), PSTR_GRAPH_PAGE, PSTR (CMND_GRAPH_DISPLAY), choice, phase, phase, str_text, str_label);
  }

  WSContentSend_P (PSTR ("</div></div>\n"));      // live

  // ----------------
  //    End of page
  // ----------------

  // main menu button
  WSContentSend_P (PSTR ("<div><form action='/' method='get'><button class='menu'>%s</button></form></div>\n"), D_MAIN_MENU);

  // end of page
  WSContentStop ();

  // log page serving time
  AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: Graph [%ums]"), millis () - timestart);
}

// Generate current counter values as a string with unit and kilo conversion
void TeleinfoGraphGetCurrentDataDisplay (const int unit_type, const int phase, char* pstr_result, const int size_result) 
{
  int  unit  = unit_type;
  long value = LONG_MAX;

  // check parameters
  if (pstr_result == nullptr) return;
  if (size_result < 8) return;
  if (phase >= TIC_PHASE_END) return;

  // init
  strcpy_P (pstr_result, PSTR (" "));

  // set curve value according to displayed data
  switch (unit) 
  {
    case TELEINFO_UNIT_W:
      if (phase == TIC_PHASE_FORECAST) value = teleinfo_forecast.pact;
        else if (phase == TIC_PHASE_SOLAR) value = teleinfo_solar.pact;
        else if (phase == TIC_PHASE_PROD) value = teleinfo_prod.pact;
        else if (phase < TIC_PHASE_PROD) value = teleinfo_conso.phase[phase].pact;
      break;
    case TELEINFO_UNIT_VA:
      if (phase == TIC_PHASE_PROD) value = teleinfo_prod.papp;
        else if (phase < TIC_PHASE_PROD) value = teleinfo_conso.phase[phase].papp;
      break;
    case TELEINFO_UNIT_V:
      if (phase == TIC_PHASE_PROD) value = teleinfo_conso.phase[0].voltage;
        else if (phase < TIC_PHASE_PROD) value = teleinfo_conso.phase[phase].voltage;
      break;
    case TELEINFO_UNIT_COS:
      if (phase == TIC_PHASE_PROD) value = teleinfo_prod.cosphi.value / 10;
        else if (phase < TIC_PHASE_PROD) value = teleinfo_conso.phase[phase].cosphi / 10;
      break;
  }

  // convert value for display
  if (value != LONG_MAX) TeleinfoGetFormattedValue (unit, value, pstr_result, size_result, false);
}

// Graph data update
void TeleinfoGraphWebUpdateData ()
{
  bool     display;
  uint8_t  phase;
  uint32_t timestart;
  char     str_text[16];

  // timestamp
  timestart = millis ();

  // start stream
  WSContentBegin (200, CT_PLAIN);

  // send number of received messages
  WSContentSend_P (PSTR ("%u"), teleinfo_meter.nb_message);

  // loop thru phases
  for (phase = 0; phase < TIC_PHASE_END; phase ++)
  {
    // check if data should be displayed
    display  = (phase < teleinfo_contract.phase);
    display |= ((phase == TIC_PHASE_PROD) && teleinfo_prod.enabled);
    display |= ((phase == TIC_PHASE_SOLAR) && teleinfo_solar.enabled);
    display |= ((phase == TIC_PHASE_FORECAST) && teleinfo_forecast.enabled);

    // display data
    str_text[0] = 0;
    if (display) TeleinfoGraphGetCurrentDataDisplay (graph_status.data, phase, str_text, sizeof (str_text));
    WSContentSend_P (PSTR (";%s"), str_text);
  }

  // end of stream
  WSContentEnd ();

  // log page serving time
  AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: Update Data in %ums"), millis () - timestart);
}

// Graph data update
void TeleinfoGraphWebUpdateCurve ()
{
  bool     display;
  uint8_t  phase, curve;
  uint32_t timestart;

  // timestamp
  timestart = millis ();

  // start stream
  WSContentBegin (200, CT_PLAIN);

  // loop thru phases
  for (phase = 0; phase < TIC_PHASE_END; phase ++)
  {
    // check if curve should be displayed
    display = false;
    if (phase < TIC_PHASE_PROD) display = (teleinfo_conso.enabled && (graph_status.display & (0x01 << (phase + 3))));
    else if (phase == TIC_PHASE_PROD) display = (teleinfo_prod.enabled && (graph_status.display & (0x01 << 2)));
    else if (phase == TIC_PHASE_SOLAR) display = (teleinfo_solar.enabled && (graph_status.display & (0x01 << 1)));
    else if (phase == TIC_PHASE_FORECAST) display = (teleinfo_forecast.enabled && (graph_status.display & (0x01 << 0)));
    
    // if needed, display curve
    if (display) TeleinfoGraphDisplayCurve (graph_status.data, graph_status.period, phase);
    WSContentSend_P (PSTR (";"));
  }

  // loop thru phases to display peak curve
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    // check if curve should be displayed
    display = (teleinfo_conso.enabled && (graph_status.display & (0x01 << (phase + 3))));

    // set curve to display (VA max or V max)
    if (graph_status.data == TELEINFO_UNIT_VA) curve = TELEINFO_UNIT_VAMAX;
    else if (graph_status.data == TELEINFO_UNIT_V) curve = TELEINFO_UNIT_VMAX;
    else curve = TELEINFO_UNIT_MAX;

    // if needed, display curve
    if (display && (curve != TELEINFO_UNIT_MAX)) TeleinfoGraphDisplayCurve (curve, graph_status.period, phase);
    WSContentSend_P (PSTR (";"));
  }

  // end of stream
  WSContentEnd ();

  // log page serving time
  AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: [%ums] Curve period %u"), millis () - timestart);
}

#endif    // USE_WEBSERVER

bool XdrvTeleinfoGraph (const uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
    case FUNC_INIT:
      TeleinfoGraphInit ();
      break;

    case FUNC_SAVE_BEFORE_RESTART:
      TeleinfoGraphSaveBeforeRestart ();
      break;

    case FUNC_EVERY_SECOND:
      TeleinfoGraphEverySecond ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_MAIN_BUTTON:
      TeleinfoGraphWebMainButton ();
      break;

    case FUNC_WEB_ADD_HANDLER:
      Webserver->on (FPSTR (PSTR_GRAPH_PAGE),       TeleinfoGraphWebDisplayPage);
      Webserver->on (FPSTR (PSTR_GRAPH_PAGE_DATA),  TeleinfoGraphWebUpdateData);
      Webserver->on (FPSTR (PSTR_GRAPH_PAGE_CURVE), TeleinfoGraphWebUpdateCurve);
    break;
#endif    // USE_WEBSERVER
  }

  return result;
}

#endif    // USE_TELEINFO_GRAPH
#endif    // USE_TELEINFO
