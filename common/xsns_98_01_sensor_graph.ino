/*
  xsns_98_01_sensor_graph.ino - Generic sensor graph

  Copyright (C) 2025  Nicolas Bernaerts

  Version history :
    10/01/2026 v1.0 - Split form generic sensor file

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

#ifdef USE_SENSOR_GRAPH

/*************************************************\
 *               Variables
\*************************************************/

#ifdef ESP32
  #define SENSOR_SLOT_PER_MINUTE            12            // live, sample for 15mn (every 5 sec)
  #define SENSOR_SLOT_PER_HOUR              12            // slots per hour in daily graph (every 5 mn)
  #define SENSOR_SLOT_PER_DAY               48            // slots per day in weekly graph (every 30 mn)
#else
  #define SENSOR_SLOT_PER_MINUTE            6             // live, sample for 15mn (every 10 sec)
  #define SENSOR_SLOT_PER_HOUR              6             // slots per hour in daily graph (every 10 mn)
  #define SENSOR_SLOT_PER_DAY               24            // slots per day in weekly graph (every hour)
#endif  // ESP32

#define SENSOR_SLOT_PER_MONTH               31            // slots per month (31 days max)
#define SENSOR_SLOT_GRAPH_MAX               366           // max slots to display

// minimum and maximum values with ratio 1
#define SENSOR_GRAPH_TEMP_MIN               -10
#define SENSOR_GRAPH_TEMP_MAX               40
#define SENSOR_GRAPH_HUMI_MIN               0
#define SENSOR_GRAPH_HUMI_MAX               100 
#define SENSOR_GRAPH_POWR_MIN               0 
#define SENSOR_GRAPH_POWR_MAX               2500 

// graph dimensions
#define SENSOR_GRAPH_WIDTH                  1200        // graph global width
#define SENSOR_GRAPH_HEIGHT                 600         // default graph height
#define SENSOR_GRAPH_PRES_HEAD              100
#define SENSOR_GRAPH_PRES_LINE              1100
#define SENSOR_GRAPH_DATA_LEFT              60          // left position of the curve
#define SENSOR_GRAPH_DATA_TOP               40          // height of the header part
#define SENSOR_GRAPH_DATA_WIDTH             1080
#define SENSOR_GRAPH_DATA_HEIGHT            560
#define SENSOR_GRAPH_BLOCK_HEIGHT           15          // block line height
#define SENSOR_GRAPH_DATA_UPDATE            10          // update data every 10 sec

#define D_CMND_SENSOR_GRAPH_HEIGHT          "height"

// config and historisation file
#define SENSOR_GRAPH_VERSION                1
constexpr char PSTR_SENSOR_GRAPH_FILE[] PROGMEM = "/sensor-graph.dat";

// web URL
constexpr char PSTR_SENSOR_PAGE_CURV_PAGE[] PROGMEM = "/scurv";
constexpr char PSTR_SENSOR_PAGE_CURV_CSS[]  PROGMEM = "/scurv.css";
constexpr char PSTR_SENSOR_PAGE_CURV_JS[]   PROGMEM = "/scurv.js";
constexpr char PSTR_SENSOR_PAGE_CURV_UPD[]  PROGMEM = "/scurv.upd";
constexpr char PSTR_SENSOR_PAGE_PRES_PAGE[] PROGMEM = "/spres";
constexpr char PSTR_SENSOR_PAGE_PRES_CSS[]  PROGMEM = "/spres.css";
constexpr char PSTR_SENSOR_PAGE_ACTI_PAGE[] PROGMEM = "/sacti";
constexpr char PSTR_SENSOR_PAGE_ACTI_CSS[]  PROGMEM = "/sacti.css";

enum SensorGraphPeriod                  { SENSOR_PERIOD_LIVE, SENSOR_PERIOD_DAY, SENSOR_PERIOD_WEEK, SENSOR_PERIOD_YEAR, SENSOR_PERIOD_MAX };
const char kSensorGraphPeriod[] PROGMEM =       "Live"    "|"     "24h"     "|"     "Week"     "|"     "Year"      ;

enum SensorGraphDataFamily             {  SENSOR_DATA_PRES  ,  SENSOR_DATA_ACTI   ,  SENSOR_DATA_HUMI   ,  SENSOR_DATA_TEMP   ,  SENSOR_DATA_POWR , SENSOR_DATA_MAX };
const char kSensorGraphLabel[] PROGMEM =    "Presence"     "|"     "Activity"    "|"    "Humidity"     "|"   "Temperature"   "|"      "Power"     ;
const char kSensorGraphColor[] PROGMEM = SENSOR_COLOR_PRES "|" SENSOR_COLOR_ACTI "|" SENSOR_COLOR_HUMI "|" SENSOR_COLOR_TEMP "|" SENSOR_COLOR_POWR;

enum SensorGraphDataType { SENSOR_TYPE_HUMIAVG, SENSOR_TYPE_HUMIMIN, SENSOR_TYPE_HUMIMAX, SENSOR_TYPE_TEMPAVG, SENSOR_TYPE_TEMPMIN, SENSOR_TYPE_TEMPMAX, SENSOR_TYPE_POWRAVG, SENSOR_TYPE_POWRMAX, SENSOR_TYPE_PRESENCE, SENSOR_TYPE_ACTIVITY, SENSOR_TYPE_MAX };


/****************************************\
 *                 Data
\****************************************/

// display parameters
struct {                                                          // 22 bytes
  bool     arr_graph_flag[SENSOR_DATA_MAX]   = { false, false, true, true, false };
  uint8_t  period = SENSOR_PERIOD_LIVE;
  uint8_t  arr_ratio_temp[SENSOR_PERIOD_MAX] = { 1, 1, 1, 1};
  uint8_t  arr_ratio_powr[SENSOR_PERIOD_MAX] = { 1, 1, 1, 1};
} sensor_graph_cfg;

// graph data structure
struct graph_slot {
  float minimum;
  float maximum;
  float sum;
  float sample;
};

// live graph update data
struct {                                                          // 204 bytes
  uint8_t    index1;
  uint8_t    index2;
  graph_slot humi;
  graph_slot temp;
  graph_slot powr;
} arr_graph_data[SENSOR_PERIOD_MAX];

// graph data                                                               ESP8266    ESP32
uint8_t arr_graph_live[SENSOR_TYPE_MAX][15][SENSOR_SLOT_PER_MINUTE];    //   0.9k      0.9k
uint8_t arr_graph_day[SENSOR_TYPE_MAX][24][SENSOR_SLOT_PER_HOUR];       //   1.44k     2.88k
uint8_t arr_graph_week[SENSOR_TYPE_MAX][7][SENSOR_SLOT_PER_DAY];        //   1.68k     3.36k
uint8_t arr_graph_year[SENSOR_TYPE_MAX][12][SENSOR_SLOT_PER_MONTH];     //   3.72k     3.72k
                                                                        //   -----     -----
                                                                        //   7,74k     10.86k

/*********************************************\
 *                Filesystem
\*********************************************/

#ifdef USE_UFILESYS

// load config file
void SensorGraphLoadData ()
{
  uint8_t  index, version;
  char     str_filename[UFS_FILENAME_SIZE];
  File     file;

  // open file in read mode
  strcpy_P (str_filename, PSTR_SENSOR_GRAPH_FILE);
  if (ffsp->exists (str_filename))
  {
    file = ffsp->open (str_filename, "r");
    file.read ((uint8_t*)&version, sizeof (version));
    if (version == SENSOR_GRAPH_VERSION)
    {
      file.read ((uint8_t*)&sensor_graph_cfg, sizeof (sensor_graph_cfg));
      file.read ((uint8_t*)&arr_graph_live,   sizeof (arr_graph_live));
      file.read ((uint8_t*)&arr_graph_day,    sizeof (arr_graph_day));
      file.read ((uint8_t*)&arr_graph_week,   sizeof (arr_graph_week));
      file.read ((uint8_t*)&arr_graph_year,   sizeof (arr_graph_year));
    }
    file.close ();
  }

  // set minimum ratio to 1
  for (index = 0; index < SENSOR_PERIOD_MAX; index ++)
  {
    if (sensor_graph_cfg.arr_ratio_temp[index] == 0) sensor_graph_cfg.arr_ratio_temp[index] = 1;
    if (sensor_graph_cfg.arr_ratio_powr[index] == 0) sensor_graph_cfg.arr_ratio_powr[index] = 1;
  }
}

// save config file
void SensorGraphSaveData ()
{
  uint8_t version;
  char    str_filename[UFS_FILENAME_SIZE];
  File    file;

  // init data
  version = SENSOR_GRAPH_VERSION;

  // open file in write mode
  strcpy_P (str_filename, PSTR_SENSOR_GRAPH_FILE);
  file = ffsp->open (str_filename, "w");
  if (file > 0)
  {
    file.write ((uint8_t*)&version,          sizeof (version));
    file.write ((uint8_t*)&sensor_graph_cfg, sizeof (sensor_graph_cfg));
    file.write ((uint8_t*)&arr_graph_live,   sizeof (arr_graph_live));
    file.write ((uint8_t*)&arr_graph_day,    sizeof (arr_graph_day));
    file.write ((uint8_t*)&arr_graph_week,   sizeof (arr_graph_week));
    file.write ((uint8_t*)&arr_graph_year,   sizeof (arr_graph_year));
    file.close ();
  }
}

#endif  // UFILESYS  

/*********************************************\
 *               Functions
\*********************************************/

void SensorGraphPeriodSlotInit (const uint8_t period)
{
  // check parameters
  if (period >= SENSOR_PERIOD_MAX) return;

  // reset acquisition data
  arr_graph_data[period].index1       = UINT8_MAX;
  arr_graph_data[period].index2       = UINT8_MAX;
  arr_graph_data[period].humi.minimum = NAN;
  arr_graph_data[period].humi.maximum = NAN;
  arr_graph_data[period].humi.sum     = 0;
  arr_graph_data[period].humi.sample  = 0;
  arr_graph_data[period].temp.minimum = NAN;
  arr_graph_data[period].temp.maximum = NAN;
  arr_graph_data[period].temp.sum     = 0;
  arr_graph_data[period].temp.sample  = 0;
  arr_graph_data[period].powr.minimum = NAN;
  arr_graph_data[period].powr.maximum = NAN;
  arr_graph_data[period].powr.sum     = 0;
  arr_graph_data[period].powr.sample  = 0;
}

void SensorGraphPeriodInit (const uint8_t period, const uint8_t index1)
{
  uint8_t slot, type, limit1, limit2;

  // check parameters
  if (period >= SENSOR_PERIOD_MAX) return;

  // get period data limits
  SensorGraphGetIndexLimits (period, limit1, limit2);

  for (type = 0; type < SENSOR_TYPE_MAX; type++)
    for (slot = 0; slot < limit2; slot ++)
      switch (period)
      {
        case SENSOR_PERIOD_LIVE: if (index1 < 15) arr_graph_live[type][index1][slot] = UINT8_MAX; break;
        case SENSOR_PERIOD_DAY:  if (index1 < 24) arr_graph_day[type][index1][slot]  = UINT8_MAX; break;
        case SENSOR_PERIOD_WEEK: if (index1 < 7)  arr_graph_week[type][index1][slot] = UINT8_MAX; break;
        case SENSOR_PERIOD_YEAR: if (index1 < 12) arr_graph_year[type][index1][slot] = UINT8_MAX; break;
      }
}

// get current graph splot according to period
bool SensorGraphPeriodGetLastIndex (const uint8_t period, uint8_t &index1, uint8_t &index2)
{
  // check parameter
  if (period >= SENSOR_PERIOD_MAX) return false;

  // set slot if not defined
  if (arr_graph_data[period].index1 == UINT8_MAX) SensorGraphPeriodGetActualIndex (period, arr_graph_data[period].index1, arr_graph_data[period].index2);

  // get slot
  index1 = arr_graph_data[period].index1;
  index2 = arr_graph_data[period].index2;
  
  return true;
}

// calculate current graph splot according to period
bool SensorGraphPeriodGetActualIndex (const uint8_t period, uint8_t &index1, uint8_t &index2)
{
  uint32_t value;

  // check parameters
  if (period >= SENSOR_PERIOD_MAX) return false;
  if (!RtcTime.valid) return false;

  // handle according to period
  switch (period)
  {
    case SENSOR_PERIOD_LIVE: 
      index1 = RtcTime.minute % 15;
      value  = (uint32_t)RtcTime.second;
      value  = value * SENSOR_SLOT_PER_MINUTE / 60;
      index2 = (uint8_t)value;
      break;

    case SENSOR_PERIOD_DAY:
      index1 = RtcTime.hour;
      value  = (uint32_t)RtcTime.minute;
      value  = value * SENSOR_SLOT_PER_HOUR / 60;
      index2 = (uint8_t)value;
      break;

    case SENSOR_PERIOD_WEEK:
      index1 = RtcTime.day_of_week - 1;                                   // (RtcTime.day_of_week + 5) % 7
      value  = (uint32_t)RtcTime.hour * 60 + (uint32_t)RtcTime.minute;
      value  = value * SENSOR_SLOT_PER_DAY / 1440;
      index2 = (uint8_t)value;
      break;

    case SENSOR_PERIOD_YEAR:
      index1 = RtcTime.month - 1;
      index2 = RtcTime.day_of_month - 1;
      break;
  }

  return true;
}

uint32_t SensorGraphGetCurveWidth (const uint8_t period)
{
  uint32_t width = 0;

  // set data according to period
  switch (period)
  {
    case SENSOR_PERIOD_LIVE: width = 15 * SENSOR_SLOT_PER_MINUTE; break;
    case SENSOR_PERIOD_DAY:  width = 24 * SENSOR_SLOT_PER_HOUR;   break;
    case SENSOR_PERIOD_WEEK: width = 7 * SENSOR_SLOT_PER_DAY;     break;
    case SENSOR_PERIOD_YEAR: width = 366;
  }

  return width;
}

uint8_t SensorGraphGetStartSlot (const uint8_t period)
{
  uint8_t slot = 0;

  // set data according to period
  switch (period)
  {
    case SENSOR_PERIOD_LIVE: slot = (RtcTime.minute % 15) + 1; break;
    case SENSOR_PERIOD_DAY:  slot = RtcTime.hour + 1;          break;
    case SENSOR_PERIOD_WEEK: slot = RtcTime.day_of_week;       break;
    case SENSOR_PERIOD_YEAR: slot = RtcTime.month;             break;
  }

  return slot;
}

bool SensorGraphIndexIsValid (const uint8_t period, const uint8_t index1, const uint8_t index2)
{
  bool result;

  // check parameters
  if (period >= SENSOR_PERIOD_MAX) return false;

  // set data according to period
  switch (period)
  {
    case SENSOR_PERIOD_LIVE:
      result = ((index1 < 15) && (index2 < SENSOR_SLOT_PER_MINUTE));
      break;

    case SENSOR_PERIOD_DAY:
      result = ((index1 < 24) && (index2 < SENSOR_SLOT_PER_HOUR));
      break;

    case SENSOR_PERIOD_WEEK:
      result = ((index1 < 7) && (index2 < SENSOR_SLOT_PER_DAY));
      break;

    case SENSOR_PERIOD_YEAR:
      result = ((index1 < 12) && (index2 < CalendarGetDaysInMonth (1970 + RtcTime.year, index1 + 1)));
  }

  return result;
}

void SensorGraphSetData (const uint8_t type, const uint8_t period, const uint8_t index1, const uint8_t index2, const uint8_t value)
{
  // check parameters
  if (period >= SENSOR_PERIOD_MAX) return;
  if (type >= SENSOR_TYPE_MAX) return;

  // set data according to period
  switch (period)
  {
    case SENSOR_PERIOD_LIVE:
      if ((index1 < 15) && (index2 < SENSOR_SLOT_PER_MINUTE)) arr_graph_live[type][index1][index2] = value;
      break;

    case SENSOR_PERIOD_DAY:
      if ((index1 < 24) && (index2 < SENSOR_SLOT_PER_HOUR)) arr_graph_day[type][index1][index2] = value;
      break;

    case SENSOR_PERIOD_WEEK:
      if ((index1 < 7) && (index2 < SENSOR_SLOT_PER_DAY)) arr_graph_week[type][index1][index2] = value;
      break;

      case SENSOR_PERIOD_YEAR:
      if ((index1 < 12) && (index2 < SENSOR_SLOT_PER_MONTH)) arr_graph_year[type][index1][index2] = value;
      break;
  }
}

uint8_t SensorGraphGetData (const uint8_t type, const uint8_t period, const uint8_t index1, const uint8_t index2)
{
  uint8_t value = UINT8_MAX;

  // check parameters
  if (period >= SENSOR_PERIOD_MAX) return UINT8_MAX;
  if (type >= SENSOR_TYPE_MAX) return UINT8_MAX;

  // collect value
  switch (period)
  {
    case SENSOR_PERIOD_LIVE: if ((index1 < 15) && (index2 < SENSOR_SLOT_PER_MINUTE)) value = arr_graph_live[type][index1][index2]; break;
    case SENSOR_PERIOD_DAY:  if ((index1 < 24) && (index2 < SENSOR_SLOT_PER_HOUR))   value = arr_graph_day[type][index1][index2];  break;
    case SENSOR_PERIOD_WEEK: if ((index1 < 7)  && (index2 < SENSOR_SLOT_PER_DAY))    value = arr_graph_week[type][index1][index2]; break;
    case SENSOR_PERIOD_YEAR: if ((index1 < 12) && (index2 < SENSOR_SLOT_PER_MONTH))  value = arr_graph_year[type][index1][index2]; break;
  }

  return value;
}

void SensorGraphGetIndexLimits (const uint8_t period, uint8_t &limit1, uint8_t &limit2)
{
  switch (period)
  {
    case SENSOR_PERIOD_LIVE:
      limit1 = 15; 
      limit2 = SENSOR_SLOT_PER_MINUTE; 
      break;
    case SENSOR_PERIOD_DAY:  
      limit1 = 24; 
      limit2 = SENSOR_SLOT_PER_HOUR;   
      break;
    case SENSOR_PERIOD_WEEK: 
      limit1 = 7;  
      limit2 = SENSOR_SLOT_PER_DAY;    
      break;
    case SENSOR_PERIOD_YEAR: 
      limit1 = 12; 
      limit2 = SENSOR_SLOT_PER_MONTH;  
      break;
  } 
}

//     Humidity
// ----------------

// convert humidity value to graph value
uint8_t SensorGraphHumidity2Data (const float humidity)
{
  uint8_t value;

  if (isnan (humidity)) value = UINT8_MAX;
    else if (humidity < 0) value = 0;
    else if (humidity > 100) value = 240;
    else value = (uint8_t)(humidity * 2.4);

  return value;
}

//     Temperature
// -------------------

// convert temperature value to graph value according to display ratio
uint8_t SensorGraphTemperature2Data (const float temperature, const int ratio)
{
  int result, value, mini, maxi;

  // check parameter
  if (ratio < 1) return UINT8_MAX;

  // set value and boundaries
  value = ratio * (int)(100 * temperature);
  mini  = ratio * 100 * SENSOR_GRAPH_TEMP_MIN;
  maxi  = ratio * 100 * SENSOR_GRAPH_TEMP_MAX;

  // calculate result
  if (value < mini) result = UINT8_MAX;
    else if (value < 0) result = 50 - 50 * value / mini;
    else if (value == 0) result = 50;
    else if (value <= maxi) result = 50 + 200 * value / maxi;
    else result = UINT8_MAX;

  return (uint8_t)result;
}

// convert temperature value to graph value according to display ratio
uint8_t SensorGraphTemperatureUpdateDataRatio (const uint8_t temperature, const int prev_ratio, const int new_ratio)
{
  int result;

  // check parameters
  if (temperature == UINT8_MAX) return UINT8_MAX;
  if (prev_ratio < 1) return UINT8_MAX;
  if (new_ratio < 1) return UINT8_MAX;

  // calculate value according to sign
  result = (int)temperature - 50;
  result = result * prev_ratio / new_ratio;
  result = 50 + result;

  // check boundaries
  if (result < 0) result = UINT8_MAX;
    else if (result > 250) result = UINT8_MAX;
  
  return (uint8_t)result;
}

void SensorGraphTemperatureUpdateArrayRatio (const uint8_t period, const int actual_ratio, const int new_ratio)
{
  uint8_t index1, index2, limit1, limit2;

  // check ratio
  if (period >= SENSOR_PERIOD_MAX) return;
  if (new_ratio == actual_ratio) return;
  if ((actual_ratio < 1) || (new_ratio < 1)) return;

  // get limits according to period
  SensorGraphGetIndexLimits (period, limit1, limit2);

  // convert all array values according to new ratio
  for (index1 = 0; index1 < limit1; index1 ++)
    for (index2 = 0; index2 < limit2; index2 ++)
      switch (period)
      {
        case SENSOR_PERIOD_LIVE:
          if (arr_graph_live[SENSOR_TYPE_TEMPMIN][index1][index2] != UINT8_MAX) arr_graph_live[SENSOR_TYPE_TEMPMIN][index1][index2] = SensorGraphTemperatureUpdateDataRatio (arr_graph_live[SENSOR_TYPE_TEMPMIN][index1][index2], actual_ratio, new_ratio);
          if (arr_graph_live[SENSOR_TYPE_TEMPMAX][index1][index2] != UINT8_MAX) arr_graph_live[SENSOR_TYPE_TEMPMAX][index1][index2] = SensorGraphTemperatureUpdateDataRatio (arr_graph_live[SENSOR_TYPE_TEMPMAX][index1][index2], actual_ratio, new_ratio);
          if (arr_graph_live[SENSOR_TYPE_TEMPAVG][index1][index2] != UINT8_MAX) arr_graph_live[SENSOR_TYPE_TEMPAVG][index1][index2] = SensorGraphTemperatureUpdateDataRatio (arr_graph_live[SENSOR_TYPE_TEMPAVG][index1][index2], actual_ratio, new_ratio);
          break;

        case SENSOR_PERIOD_DAY:
          if (arr_graph_day[SENSOR_TYPE_TEMPMIN][index1][index2] != UINT8_MAX) arr_graph_day[SENSOR_TYPE_TEMPMIN][index1][index2] = SensorGraphTemperatureUpdateDataRatio (arr_graph_day[SENSOR_TYPE_TEMPMIN][index1][index2], actual_ratio, new_ratio);
          if (arr_graph_day[SENSOR_TYPE_TEMPMAX][index1][index2] != UINT8_MAX) arr_graph_day[SENSOR_TYPE_TEMPMAX][index1][index2] = SensorGraphTemperatureUpdateDataRatio (arr_graph_day[SENSOR_TYPE_TEMPMAX][index1][index2], actual_ratio, new_ratio);
          if (arr_graph_day[SENSOR_TYPE_TEMPAVG][index1][index2] != UINT8_MAX) arr_graph_day[SENSOR_TYPE_TEMPAVG][index1][index2] = SensorGraphTemperatureUpdateDataRatio (arr_graph_day[SENSOR_TYPE_TEMPAVG][index1][index2], actual_ratio, new_ratio);
          break;

        case SENSOR_PERIOD_WEEK:
          if (arr_graph_week[SENSOR_TYPE_TEMPMIN][index1][index2] != UINT8_MAX) arr_graph_week[SENSOR_TYPE_TEMPMIN][index1][index2] = SensorGraphTemperatureUpdateDataRatio (arr_graph_week[SENSOR_TYPE_TEMPMIN][index1][index2], actual_ratio, new_ratio);
          if (arr_graph_week[SENSOR_TYPE_TEMPMAX][index1][index2] != UINT8_MAX) arr_graph_week[SENSOR_TYPE_TEMPMAX][index1][index2] = SensorGraphTemperatureUpdateDataRatio (arr_graph_week[SENSOR_TYPE_TEMPMAX][index1][index2], actual_ratio, new_ratio);
          if (arr_graph_week[SENSOR_TYPE_TEMPAVG][index1][index2] != UINT8_MAX) arr_graph_week[SENSOR_TYPE_TEMPAVG][index1][index2] = SensorGraphTemperatureUpdateDataRatio (arr_graph_week[SENSOR_TYPE_TEMPAVG][index1][index2], actual_ratio, new_ratio);
          break;

        case SENSOR_PERIOD_YEAR:
          if (arr_graph_year[SENSOR_TYPE_TEMPMIN][index1][index2] != UINT8_MAX) arr_graph_year[SENSOR_TYPE_TEMPMIN][index1][index2] = SensorGraphTemperatureUpdateDataRatio (arr_graph_year[SENSOR_TYPE_TEMPMIN][index1][index2], actual_ratio, new_ratio);
          if (arr_graph_year[SENSOR_TYPE_TEMPMAX][index1][index2] != UINT8_MAX) arr_graph_year[SENSOR_TYPE_TEMPMAX][index1][index2] = SensorGraphTemperatureUpdateDataRatio (arr_graph_year[SENSOR_TYPE_TEMPMAX][index1][index2], actual_ratio, new_ratio);
          if (arr_graph_year[SENSOR_TYPE_TEMPAVG][index1][index2] != UINT8_MAX) arr_graph_year[SENSOR_TYPE_TEMPAVG][index1][index2] = SensorGraphTemperatureUpdateDataRatio (arr_graph_year[SENSOR_TYPE_TEMPAVG][index1][index2], actual_ratio, new_ratio);
          break;
      }

  // update ratio
  sensor_graph_cfg.arr_ratio_temp[period] = new_ratio;
}

//       Power
// -------------------

// convert power value to graph value according to display ratio
uint8_t SensorGraphPower2Data (const float power, const int ratio)
{
  uint32_t result;

  // check parameter
  if (power < 0) return UINT8_MAX;
  if (ratio < 1) return UINT8_MAX;

  // set value and boundaries
  result = 250 * (uint32_t)power / SENSOR_GRAPH_POWR_MAX / (uint32_t)ratio;

  return (uint8_t)result;
}

// convert temperature value to graph value according to display ratio
uint8_t SensorGraphPowerUpdateDataRatio (const uint8_t power, const int curr_ratio, const int new_ratio)
{
  int result;

  // check parameters
  if (power == UINT8_MAX) return UINT8_MAX;
  if (curr_ratio < 1) return UINT8_MAX;
  if (new_ratio  < 1) return UINT8_MAX;

  // calculate value according to sign
  result = (int)power * curr_ratio / new_ratio;

  // check boundaries
  if (result > 250) result = UINT8_MAX;
  
  return (uint8_t)result;
}

void SensorGraphPowerUpdateArrayRatio (const uint8_t period, const int actual_ratio, const int new_ratio)
{
  uint8_t index1, index2, limit1, limit2;

  // check ratio
  if (period >= SENSOR_PERIOD_MAX) return;
  if (new_ratio == actual_ratio) return;
  if ((actual_ratio < 1) || (new_ratio < 1)) return;

  // get limits according to period
  SensorGraphGetIndexLimits (period, limit1, limit2);

  // convert all array values according to new ratio
  for (index1 = 0; index1 < limit1; index1 ++)
    for (index2 = 0; index2 < limit2; index2 ++)
      switch (period)
      {
        case SENSOR_PERIOD_LIVE:
          if (arr_graph_live[SENSOR_TYPE_POWRMAX][index1][index2] != UINT8_MAX) arr_graph_live[SENSOR_TYPE_POWRMAX][index1][index2] = SensorGraphPowerUpdateDataRatio (arr_graph_live[SENSOR_TYPE_POWRMAX][index1][index2], actual_ratio, new_ratio);
          if (arr_graph_live[SENSOR_TYPE_POWRAVG][index1][index2] != UINT8_MAX) arr_graph_live[SENSOR_TYPE_POWRAVG][index1][index2] = SensorGraphPowerUpdateDataRatio (arr_graph_live[SENSOR_TYPE_POWRAVG][index1][index2], actual_ratio, new_ratio);
          break;
        case SENSOR_PERIOD_DAY:
          if (arr_graph_day[SENSOR_TYPE_POWRMAX][index1][index2] != UINT8_MAX) arr_graph_day[SENSOR_TYPE_POWRMAX][index1][index2] = SensorGraphPowerUpdateDataRatio (arr_graph_day[SENSOR_TYPE_POWRMAX][index1][index2], actual_ratio, new_ratio);
          if (arr_graph_day[SENSOR_TYPE_POWRAVG][index1][index2] != UINT8_MAX) arr_graph_day[SENSOR_TYPE_POWRAVG][index1][index2] = SensorGraphPowerUpdateDataRatio (arr_graph_day[SENSOR_TYPE_POWRAVG][index1][index2], actual_ratio, new_ratio);
          break;
        case SENSOR_PERIOD_WEEK:
          if (arr_graph_week[SENSOR_TYPE_POWRMAX][index1][index2] != UINT8_MAX) arr_graph_week[SENSOR_TYPE_POWRMAX][index1][index2] = SensorGraphPowerUpdateDataRatio (arr_graph_week[SENSOR_TYPE_POWRMAX][index1][index2], actual_ratio, new_ratio);
          if (arr_graph_week[SENSOR_TYPE_POWRAVG][index1][index2] != UINT8_MAX) arr_graph_week[SENSOR_TYPE_POWRAVG][index1][index2] = SensorGraphPowerUpdateDataRatio (arr_graph_week[SENSOR_TYPE_POWRAVG][index1][index2], actual_ratio, new_ratio);
          break;
        case SENSOR_PERIOD_YEAR:
          if (arr_graph_year[SENSOR_TYPE_POWRMAX][index1][index2] != UINT8_MAX) arr_graph_year[SENSOR_TYPE_POWRMAX][index1][index2] = SensorGraphPowerUpdateDataRatio (arr_graph_year[SENSOR_TYPE_POWRMAX][index1][index2], actual_ratio, new_ratio);
          if (arr_graph_year[SENSOR_TYPE_POWRAVG][index1][index2] != UINT8_MAX) arr_graph_year[SENSOR_TYPE_POWRAVG][index1][index2] = SensorGraphPowerUpdateDataRatio (arr_graph_year[SENSOR_TYPE_POWRAVG][index1][index2], actual_ratio, new_ratio);
          break;
      }

  // set new power ratio
  sensor_graph_cfg.arr_ratio_powr[period] = new_ratio;
}

/*********************************************\
 *                   Callback
\*********************************************/

void SensorGraphInit ()
{
  uint8_t period;

  // init data collection structure
  for (period = 0; period < SENSOR_PERIOD_MAX; period ++)
  {
    SensorGraphPeriodSlotInit (period);
  }

  // init graph structures
  memset (&arr_graph_live, UINT8_MAX, sizeof (arr_graph_live));
  memset (&arr_graph_day,  UINT8_MAX, sizeof (arr_graph_day));
  memset (&arr_graph_week, UINT8_MAX, sizeof (arr_graph_week));
  memset (&arr_graph_year, UINT8_MAX, sizeof (arr_graph_year));
  
#ifdef USE_UFILESYS
  // load data
  SensorGraphLoadData ();
#endif    // USE_UFILESYS
/*
for (period = 0; period < 15; period++) SensorGraphSetData (SENSOR_TYPE_ACTIVITY, SENSOR_PERIOD_YEAR, 4, period * 2, 1);
SensorGraphSetData (SENSOR_TYPE_ACTIVITY, SENSOR_PERIOD_YEAR, 6, 29, 1);
SensorGraphSetData (SENSOR_TYPE_ACTIVITY, SENSOR_PERIOD_YEAR, 6, 30, 1);
for (period = 0; period < 15; period++) SensorGraphSetData (SENSOR_TYPE_PRESENCE, SENSOR_PERIOD_YEAR, 6, period * 2, 1);
SensorGraphSetData (SENSOR_TYPE_PRESENCE, SENSOR_PERIOD_YEAR, 6, 29, 1);
SensorGraphSetData (SENSOR_TYPE_PRESENCE, SENSOR_PERIOD_YEAR, 6, 30, 1);
for (period = 0; period < SENSOR_SLOT_PER_MINUTE; period++) SensorGraphSetData (SENSOR_TYPE_PRESENCE, SENSOR_PERIOD_LIVE, 2, period, 0);
*/
}

// called before restart
void SensorGraphSaveBeforeRestart ()
{
#ifdef USE_UFILESYS
  // save data
  SensorGraphSaveData ();
#endif    // USE_UFILESYS
}

// called every second
void SensorGraphEverySecond ()
{
  bool     update, presence, activity;
  uint8_t  period, value;
  uint8_t  minimum, maximum, average;
  uint8_t  last_index1, last_index2, actual_index1, actual_index2;
  int      ratio, start;
  float    temperature, humidity, power;

  // ignore if time is not valid
  if (!RtcTime.valid) return;

  // get current values
  presence    = SensorPresenceGet (1);
  activity    = SensorActivityGet ();
  temperature = SensorTemperatureGet ();
  humidity    = SensorHumidityGet ();
  power       = SensorPowerGet ();

  // loop to update period data
  for (period = SENSOR_PERIOD_LIVE; period < SENSOR_PERIOD_MAX; period ++)
  {
    // calculate update flag
    switch (period)
    {
      case SENSOR_PERIOD_LIVE: update = true;                       break;
      case SENSOR_PERIOD_DAY:  update = (RtcTime.second % 5 == 0);  break;
      case SENSOR_PERIOD_WEEK: update = (RtcTime.second % 30 == 0); break;
      case SENSOR_PERIOD_YEAR: update = (RtcTime.second == 0);      break;
    }

    // get current and target slots
    SensorGraphPeriodGetLastIndex   (period, last_index1,   last_index2);
    SensorGraphPeriodGetActualIndex (period, actual_index1, actual_index2);

    // if main slot has changed, init full slot data
    if (last_index1 != actual_index1) SensorGraphPeriodInit (period, actual_index1);

    // if second slot has changed, init calculation data
    if (last_index2 != actual_index2)
    {
      SensorGraphPeriodSlotInit (period);
      arr_graph_data[period].index1 = actual_index1;
      arr_graph_data[period].index2 = actual_index2;
      update = true;
    }

    // update current presence and activity
    if (presence) SensorGraphSetData (SENSOR_TYPE_PRESENCE, period, actual_index1, actual_index2, 1);
    if (activity) SensorGraphSetData (SENSOR_TYPE_ACTIVITY, period, actual_index1, actual_index2, 1);

    // if update is triggered, update acquisitions
    if (update)
    {
      // update humidity
      if (!isnan (humidity))
      {
        // update current value
        (isnan (arr_graph_data[period].humi.minimum)) ? arr_graph_data[period].humi.minimum = humidity : arr_graph_data[period].humi.minimum = min (arr_graph_data[period].humi.minimum, humidity);
        (isnan (arr_graph_data[period].humi.maximum)) ? arr_graph_data[period].humi.maximum = humidity : arr_graph_data[period].humi.maximum = max (arr_graph_data[period].humi.maximum, humidity);
        arr_graph_data[period].humi.sum += humidity;
        arr_graph_data[period].humi.sample++;

        // update graph data
        SensorGraphSetData (SENSOR_TYPE_HUMIMIN, period, actual_index1, actual_index2, SensorGraphHumidity2Data (arr_graph_data[period].humi.minimum));
        SensorGraphSetData (SENSOR_TYPE_HUMIMAX, period, actual_index1, actual_index2, SensorGraphHumidity2Data (arr_graph_data[period].humi.maximum));
        SensorGraphSetData (SENSOR_TYPE_HUMIAVG, period, actual_index1, actual_index2, SensorGraphHumidity2Data (arr_graph_data[period].humi.sum / arr_graph_data[period].humi.sample));
      }

      // update temperature
      if (!isnan (temperature))
      {
        // update current value
        (isnan (arr_graph_data[period].temp.minimum)) ? arr_graph_data[period].temp.minimum = temperature : arr_graph_data[period].temp.minimum = min (arr_graph_data[period].temp.minimum, temperature);
        (isnan (arr_graph_data[period].temp.maximum)) ? arr_graph_data[period].temp.maximum = temperature : arr_graph_data[period].temp.maximum = max (arr_graph_data[period].temp.maximum, temperature);
        arr_graph_data[period].temp.sum += temperature;
        arr_graph_data[period].temp.sample++;

        // if needed, update graph ratio
        ratio = (int)sensor_graph_cfg.arr_ratio_temp[period];
        start = ratio;
        value = SensorGraphTemperature2Data (temperature, ratio);
        while ((ratio < 4) && (value == UINT8_MAX))
        {
          ratio = ratio * 2;
          value = SensorGraphTemperature2Data (temperature, ratio);
        }

        // if ratio has chnaged, update all graph values
        if (start != ratio) SensorGraphTemperatureUpdateArrayRatio (period, start, ratio);

        // update graph data
        SensorGraphSetData (SENSOR_TYPE_TEMPMIN, period, actual_index1, actual_index2, SensorGraphTemperature2Data (arr_graph_data[period].temp.minimum, ratio));
        SensorGraphSetData (SENSOR_TYPE_TEMPMAX, period, actual_index1, actual_index2, SensorGraphTemperature2Data (arr_graph_data[period].temp.maximum, ratio));
        SensorGraphSetData (SENSOR_TYPE_TEMPAVG, period, actual_index1, actual_index2, SensorGraphTemperature2Data (arr_graph_data[period].temp.sum / arr_graph_data[period].temp.sample, ratio));
      }

      // update power
      if (!isnan (power))
      {
        // update current value
        (isnan (arr_graph_data[period].powr.maximum)) ? arr_graph_data[period].powr.maximum = power : arr_graph_data[period].powr.maximum = max (arr_graph_data[period].powr.maximum, power);
        arr_graph_data[period].powr.sum += power;
        arr_graph_data[period].powr.sample++;

        // if needed, update graph ratio
        ratio = (int)sensor_graph_cfg.arr_ratio_powr[period];
        start = ratio;
        value = SensorGraphPower2Data (power, ratio);
        while ((ratio < 4) && (value == UINT8_MAX))
        {
          ratio = ratio * 2;
          value = SensorGraphPower2Data (power, ratio);
        }

        // if ratio has chnaged, update all graph values
        if (start != ratio) SensorGraphPowerUpdateArrayRatio (period, start, ratio);

        // update graph data
        SensorGraphSetData (SENSOR_TYPE_POWRMAX, period, actual_index1, actual_index2, SensorGraphPower2Data (arr_graph_data[period].powr.maximum, ratio));
        SensorGraphSetData (SENSOR_TYPE_POWRAVG, period, actual_index1, actual_index2, SensorGraphPower2Data (arr_graph_data[period].powr.sum / arr_graph_data[period].powr.sample, ratio));
      }
    }
  }
}

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

// Append sensor histo button to main page
void SensorGraphWebMainButton ()
{
  uint8_t element, width;

  // calculate buttons width
  element = 1;
  if (sensor_graph_cfg.arr_graph_flag[SENSOR_DATA_ACTI]) element++;
  if (sensor_status.pres.source != SENSOR_PRES_NONE)     element++;
  width = 100 / element - 1;

  // style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("button.sens {width:%u%%;margin:auto;}\n"), width);
  WSContentSend_P (PSTR ("</style>\n"));

  // button
  WSContentSend_P (PSTR ("<p class='center'>\n"));
  WSContentSend_P (PSTR ("<a href='%s'><button class='sens'>Sensors</button></a>\n"), PSTR_SENSOR_PAGE_CURV_PAGE);
  if (sensor_graph_cfg.arr_graph_flag[SENSOR_DATA_ACTI]) WSContentSend_P (PSTR ("<a href='%s'><button class='sens'>Activity</button></a>\n"), PSTR_SENSOR_PAGE_ACTI_PAGE);
  if (sensor_status.pres.source != SENSOR_PRES_NONE)     WSContentSend_P (PSTR ("<a href='%s'><button class='sens'>Presence</button></a>\n"), PSTR_SENSOR_PAGE_PRES_PAGE);
  WSContentSend_P (PSTR ("</p>\n"));
}

void SensorGraphDisplayCurveHeader (const uint8_t period)
{
  uint8_t  start, limit1, limit2;
  uint32_t slot, number, width, pos_x;
  char     str_text[8];

  // set number of separation lines according to period
  switch (period)
  {
    case SENSOR_PERIOD_LIVE: number = 15; break;
    case SENSOR_PERIOD_DAY:  number = 24; break;
    case SENSOR_PERIOD_WEEK: number = 7;  break;
    case SENSOR_PERIOD_YEAR: number = 12; break;
  }

  // display separation lines
  for (slot = 1; slot < number; slot ++) 
  {
    pos_x = SENSOR_GRAPH_DATA_LEFT + slot * SENSOR_GRAPH_DATA_WIDTH / number;
    WSContentSend_P (PSTR ("<line class='dash' x1=%u y1=%u x2=%u y2=%u />\n"), pos_x, SENSOR_GRAPH_DATA_TOP, pos_x, SENSOR_GRAPH_DATA_TOP + SENSOR_GRAPH_DATA_HEIGHT);
  }

  // set environment according to period
  start = SensorGraphGetStartSlot (period);
  width = SENSOR_GRAPH_DATA_WIDTH / number;
  SensorGraphGetIndexLimits (period, limit1, limit2);

  // display headers
  for (slot = 0; slot < number; slot ++) 
  {
    // display data
    pos_x = SENSOR_GRAPH_DATA_LEFT + (slot + 1) * width;
    switch (period)
    {
      case SENSOR_PERIOD_LIVE:
        if ((slot < number -1) && (slot % 2 == 0)) WSContentSend_P (PSTR ("<text x=%u y=%u >-%umn</text>\n"), pos_x, 20, number - slot - 1);
        break;
      case SENSOR_PERIOD_DAY:
        if (slot % 2 == 1) WSContentSend_P (PSTR ("<text x=%u y=%u >%uh</text>\n"), pos_x - width, 20, start);
        break;
      case SENSOR_PERIOD_WEEK:
        strlcpy (str_text, kWeekdayNames + start * 3, 4);
        WSContentSend_P (PSTR ("<text x=%u y=%u >%s</text>\n"), pos_x - width / 2, 20, str_text);
        break;
      case SENSOR_PERIOD_YEAR:
        strlcpy (str_text, kMonthNames + start * 3, 4);
        WSContentSend_P (PSTR ("<text x=%u y=%u >%s</text>\n"), pos_x - width / 2, 20, str_text);
        break;
    }

    // increment
    start++;
    start = start % limit1;
  }
}

// Graph units
void SensorGraphDisplayCurveTempUnits (const uint8_t period, const uint8_t position)
{
  uint8_t  arr_pos[6] = {0,20,40,60,80,99};  
  uint32_t pos_y;
  int      index, value, ratio, minimum, maximum, delta;

  // get context
  ratio   = (int)sensor_graph_cfg.arr_ratio_temp[period];
  minimum = 10 * SENSOR_GRAPH_TEMP_MIN * ratio;
  maximum = 10 * SENSOR_GRAPH_TEMP_MAX * ratio;

  // loop thru 5 unit levels
  for (index = 0; index < 6; index ++)
  {
    delta = maximum - minimum;
    value = maximum - index * delta / 5;
    pos_y = SENSOR_GRAPH_DATA_TOP + (uint32_t)arr_pos[index] * SENSOR_GRAPH_DATA_HEIGHT / 100;
    WSContentSend_P (PSTR ("<text class='temp' x=%u%% y=%u >%d.%d°</text>\n"), position, pos_y, value / 10, abs (value) % 10);
  }
}

// Graph units
void SensorGraphDisplayCurveHumiUnits (const uint8_t position)
{
  uint8_t  arr_pos[6] = {0,20,40,60,80,99};
  uint32_t pos_y;
  int      index, value, delta, minimum, maximum;
  
  minimum = 0;
  maximum = 100;

  // loop thru 5 unit levels
  for (index = 0; index < 6; index ++)
  {
    delta = maximum - minimum;
    value = maximum - index * delta / 5;
    pos_y = SENSOR_GRAPH_DATA_TOP + (uint32_t)arr_pos[index] * SENSOR_GRAPH_DATA_HEIGHT / 100;
    WSContentSend_P (PSTR ("<text class='humi' x=%u%% y=%u >%d%%</text>\n"), position, pos_y, value);
  }
}

// Graph units
void SensorGraphDisplayCurvePowrUnits (const uint8_t period, const uint8_t position)
{
  uint8_t  arr_pos[6] = {0,20,40,60,80,99};
  uint32_t pos_y;
  int      index, value, ratio, minimum, maximum, delta;
  
  ratio   = (int)sensor_graph_cfg.arr_ratio_powr[period];
  minimum = 0;
  maximum = SENSOR_GRAPH_POWR_MAX * ratio;

  // loop thru 5 unit levels
  for (index = 0; index < 6; index ++)
  {
    delta = maximum - minimum;
    value = maximum - index * delta / 5;
    pos_y = SENSOR_GRAPH_DATA_TOP + (uint32_t)arr_pos[index] * SENSOR_GRAPH_DATA_HEIGHT / 100;
    WSContentSend_P (PSTR ("<text class='powr' x=%u%% y=%u >%d W</text>\n"), position, pos_y, value);
  }
}

// display block element
void SensorGraphDisplayBlock (const uint8_t period, const uint8_t type, const char* pstr_style, const char* pstr_id, const uint32_t offset_y, const uint32_t height)
{
  WSContentSend_P (PSTR ("<path class='%s' id='%s' d='"), pstr_style, pstr_id);
  SensorGraphDisplayBlockData (period, type, offset_y, height);
  WSContentSend_P (PSTR ("' />\n"));
}

// display block data
void SensorGraphDisplayBlockData (const uint8_t period, const uint8_t type, const uint32_t offset_y, const uint32_t height)
{
  bool     previous = false;
  uint8_t  index1, start1, slot1, slot2, limit1, limit2;
  uint32_t width, size, start, position;
  uint32_t x_left, x_right, y_top, y_bottom;
  bool     arr_value[SENSOR_SLOT_GRAPH_MAX];

  // get limits according to period
  width  = SensorGraphGetCurveWidth (period);
  start1 = SensorGraphGetStartSlot  (period);
  SensorGraphGetIndexLimits (period, limit1, limit2);

  // loop thru main and sub slots to prepare data
  size = 0;
  for (index1 = start1; index1 < start1 + limit1; index1 ++)
    for (slot2 = 0; slot2 < limit2; slot2 ++)
    {
      slot1 = index1 % limit1;
      if (SensorGraphIndexIsValid (period, slot1, slot2)) arr_value[size++] = (SensorGraphGetData (type, period, slot1, slot2) == 1);
    }

  // calculate vertical position
  y_top    = SENSOR_GRAPH_DATA_TOP + offset_y * SENSOR_GRAPH_DATA_HEIGHT / 250;
  y_bottom = y_top + height * SENSOR_GRAPH_DATA_HEIGHT / 250;

  // loop thru values
  start    = 0;
  position = 0;
  for (position = 0; position < size; position++)
  {
    // block start
    if (!previous && arr_value[position])
    {
      // record position and update previous state
      start    = position;
      previous = true;
    }

    // block end or last position
    else if ((previous && !arr_value[position]) || ((position == size - 1) && arr_value[position]))
    {
      // display block
      x_left  = SENSOR_GRAPH_DATA_LEFT + start * SENSOR_GRAPH_DATA_WIDTH / width;
      x_right = SENSOR_GRAPH_DATA_LEFT + (position - 1) * SENSOR_GRAPH_DATA_WIDTH / width;
      WSContentSend_P (PSTR ("M%u,%u %u,%u %u,%u %u,%u "), x_left, y_top, x_right, y_top, x_right, y_bottom, x_left, y_bottom);

      // save previous value
      previous = false;
    }
  }
}

// display line element
void SensorGraphDisplayLine (const uint8_t period, const uint8_t type, const char* pstr_style, const char* pstr_id)
{
  WSContentSend_P (PSTR ("<polyline class='%s' id='%s' points='"), pstr_style, pstr_id);
  SensorGraphDisplayLineData (period, type);
  WSContentSend_P (PSTR ("' />\n"));  
}

// display line data
void SensorGraphDisplayLineData (const uint8_t period, const uint8_t type)
{
  uint8_t  index1, start1, slot1, slot2, limit1, limit2;
  uint32_t size, position, width, graph_x, graph_y;
  uint8_t  arr_value[SENSOR_SLOT_GRAPH_MAX];

  // get limits according to period
  width  = SensorGraphGetCurveWidth (period);
  start1 = SensorGraphGetStartSlot (period);
  SensorGraphGetIndexLimits (period, limit1, limit2);

  // loop thru main and sub slots to prepare data
  size = 0;
  for (index1 = start1; index1 < start1 + limit1; index1 ++)
    for (slot2 = 0; slot2 < limit2; slot2 ++)
    {
      slot1 = index1 % limit1;
      if (SensorGraphIndexIsValid (period, slot1, slot2)) arr_value[size++] = SensorGraphGetData (type, period, slot1, slot2);
    }

  // display graph
  for (position = 0; position < size; position++)
  {
    if (arr_value[position] != UINT8_MAX)
    {
      graph_x = SENSOR_GRAPH_DATA_LEFT + position * SENSOR_GRAPH_DATA_WIDTH / width;
      graph_y = SENSOR_GRAPH_DATA_TOP  + (uint32_t)(250 - arr_value[position]) * SENSOR_GRAPH_DATA_HEIGHT / 250;
      WSContentSend_P (PSTR ("%u,%u "), graph_x, graph_y);
    }
  }
}

// display area element
void SensorGraphDisplayArea (const uint8_t period, const uint8_t type, const char* pstr_style, const char* pstr_id)
{
  WSContentSend_P (PSTR ("<polygon class='%s' id='%s' points='"), pstr_style, pstr_id);
  SensorGraphDisplayAreaData (period, type);
  WSContentSend_P (PSTR ("' />\n"));  
}

// display area element
void SensorGraphDisplayAreaData (const uint8_t period, const uint8_t type)
{
  bool     first;
  uint8_t  index1, start1, slot1, slot2, limit1, limit2;
  uint32_t size, position, width, graph_x, graph_y;
  uint8_t  arr_value[SENSOR_SLOT_GRAPH_MAX];

  // get limits according to period
  width  = SensorGraphGetCurveWidth (period);
  start1 = SensorGraphGetStartSlot (period);
  SensorGraphGetIndexLimits (period, limit1, limit2);

  // loop thru main and sub slots to prepare data
  size = 0;
  for (index1 = start1; index1 < start1 + limit1; index1 ++)
    for (slot2 = 0; slot2 < limit2; slot2 ++)
    {
      slot1 = index1 % limit1;
      if (SensorGraphIndexIsValid (period, slot1, slot2)) arr_value[size++] = SensorGraphGetData (type, period, slot1, slot2);
    }

  // display graph
  first = true;
  for (position = 0; position < size; position++)
  {
    if (arr_value[position] != UINT8_MAX)
    {
      graph_x = SENSOR_GRAPH_DATA_LEFT + position * SENSOR_GRAPH_DATA_WIDTH / width;
      graph_y = SENSOR_GRAPH_DATA_TOP  + (uint32_t)(250 - arr_value[position]) * SENSOR_GRAPH_DATA_HEIGHT / 250;
      if (first) WSContentSend_P (PSTR ("%u,%u "), graph_x, SENSOR_GRAPH_DATA_TOP + SENSOR_GRAPH_DATA_HEIGHT);
      WSContentSend_P (PSTR ("%u,%u "), graph_x, graph_y);
      first = false;
    }
  }
  WSContentSend_P (PSTR ("%u,%u"), graph_x, SENSOR_GRAPH_DATA_TOP + SENSOR_GRAPH_DATA_HEIGHT);  
}

// Graph page SVG
void SensorGraphWebCurveSVG (const uint8_t period, const bool temp, const bool humi, const bool acti, const bool powr, const bool pres)
{
  // graph parameters
  sensor_graph_cfg.period = period;
  sensor_graph_cfg.arr_graph_flag[SENSOR_DATA_TEMP] = temp;
  sensor_graph_cfg.arr_graph_flag[SENSOR_DATA_HUMI] = humi;
  sensor_graph_cfg.arr_graph_flag[SENSOR_DATA_ACTI] = acti;
  sensor_graph_cfg.arr_graph_flag[SENSOR_DATA_POWR] = powr;
  sensor_graph_cfg.arr_graph_flag[SENSOR_DATA_PRES] = pres;

  // display graph
  SensorGraphWebCurveSVG ();
}

void SensorGraphWebCurveSVG ()
{
  uint32_t index, graph_y;

  // svg : star
  WSContentSend_P (PSTR ("<div class='graph'>\n"));
  WSContentSend_P (PSTR ("<svg class='graph' viewBox='0 0 %u %u' preserveAspectRatio='none'>\n"), SENSOR_GRAPH_WIDTH, SENSOR_GRAPH_HEIGHT + 1);

  // display zones (header and vertical separation)
  SensorGraphDisplayCurveHeader (sensor_graph_cfg.period);

  // svg : levels
  for (index = 1; index < 5; index ++)
  {
    graph_y = SENSOR_GRAPH_DATA_TOP + index * SENSOR_GRAPH_DATA_HEIGHT / 5;
    WSContentSend_P (PSTR ("<line class='dash' x1=%u y1=%u x2=%u y2=%u />\n"), SENSOR_GRAPH_DATA_LEFT, graph_y, SENSOR_GRAPH_DATA_LEFT + SENSOR_GRAPH_DATA_WIDTH, graph_y);
  } 

  // svg : units
  if (sensor_graph_cfg.arr_graph_flag[SENSOR_DATA_TEMP]) SensorGraphDisplayCurveTempUnits (sensor_graph_cfg.period, 2);
  if (sensor_graph_cfg.arr_graph_flag[SENSOR_DATA_HUMI]) SensorGraphDisplayCurveHumiUnits (98);
  if (sensor_graph_cfg.arr_graph_flag[SENSOR_DATA_POWR]) SensorGraphDisplayCurvePowrUnits (sensor_graph_cfg.period, 98);

  // display zones (header and vertical separation)
  SensorGraphDisplayCurveHeader (sensor_graph_cfg.period);

  // svg : levels
  for (index = 1; index < 5; index ++)
  {
    graph_y = SENSOR_GRAPH_DATA_TOP + index * SENSOR_GRAPH_DATA_HEIGHT / 5;
    WSContentSend_P (PSTR ("<line class='dash' x1=%u y1=%u x2=%u y2=%u />\n"), SENSOR_GRAPH_DATA_LEFT, graph_y, SENSOR_GRAPH_DATA_LEFT + SENSOR_GRAPH_DATA_WIDTH, graph_y);
  } 

  // svg : units
  if (sensor_graph_cfg.arr_graph_flag[SENSOR_DATA_TEMP]) SensorGraphDisplayCurveTempUnits (sensor_graph_cfg.period, 2);
  if (sensor_graph_cfg.arr_graph_flag[SENSOR_DATA_HUMI]) SensorGraphDisplayCurveHumiUnits (98);
  if (sensor_graph_cfg.arr_graph_flag[SENSOR_DATA_POWR]) SensorGraphDisplayCurvePowrUnits (sensor_graph_cfg.period, 98);

  // svg : temperature curve
  if (sensor_graph_cfg.arr_graph_flag[SENSOR_DATA_TEMP])
  {
    SensorGraphDisplayArea (sensor_graph_cfg.period, SENSOR_TYPE_TEMPAVG, PSTR ("temp"), PSTR ("tavg"));
    SensorGraphDisplayLine (sensor_graph_cfg.period, SENSOR_TYPE_TEMPMIN, PSTR ("temp"), PSTR ("tmin"));
    SensorGraphDisplayLine (sensor_graph_cfg.period, SENSOR_TYPE_TEMPMAX, PSTR ("temp"), PSTR ("tmax"));
  }

  // svg : humidity curve
  if (sensor_graph_cfg.arr_graph_flag[SENSOR_DATA_HUMI])
  {
    SensorGraphDisplayArea (sensor_graph_cfg.period, SENSOR_TYPE_HUMIAVG, PSTR ("humi"), PSTR ("havg"));
    SensorGraphDisplayLine (sensor_graph_cfg.period, SENSOR_TYPE_HUMIMIN, PSTR ("humi"), PSTR ("hmin"));
    SensorGraphDisplayLine (sensor_graph_cfg.period, SENSOR_TYPE_HUMIMAX, PSTR ("humi"), PSTR ("hmax"));
  }

  // svg : power curve
  if (sensor_graph_cfg.arr_graph_flag[SENSOR_DATA_POWR])
  {
    SensorGraphDisplayArea (sensor_graph_cfg.period, SENSOR_TYPE_POWRAVG, PSTR ("powr"), PSTR ("pavg"));
    SensorGraphDisplayLine (sensor_graph_cfg.period, SENSOR_TYPE_POWRMAX, PSTR ("powr"), PSTR ("pmax"));
  }

  
  // svg : presence and activity curves
  graph_y = 250 - SENSOR_GRAPH_BLOCK_HEIGHT;
  if (sensor_graph_cfg.arr_graph_flag[SENSOR_DATA_PRES])
  { 
    SensorGraphDisplayBlock (sensor_graph_cfg.period, SENSOR_TYPE_PRESENCE, PSTR ("pres"), PSTR ("pres"), graph_y, SENSOR_GRAPH_BLOCK_HEIGHT); 
    graph_y -= SENSOR_GRAPH_BLOCK_HEIGHT;
  }
  if (sensor_graph_cfg.arr_graph_flag[SENSOR_DATA_ACTI]) SensorGraphDisplayBlock (sensor_graph_cfg.period, SENSOR_TYPE_ACTIVITY, PSTR ("acti"), PSTR ("acti"), graph_y, SENSOR_GRAPH_BLOCK_HEIGHT);

  // svg : frame
  WSContentSend_P (PSTR ("<rect class='main' x=%u y=%u width=%u height=%u rx=10 />\n"), SENSOR_GRAPH_DATA_LEFT, SENSOR_GRAPH_DATA_TOP, SENSOR_GRAPH_DATA_WIDTH, SENSOR_GRAPH_DATA_HEIGHT);

  // svg : end
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentSend_P (PSTR ("</div>\n"));
}

void SensorGraphWebPageCurveUpdate ()
{
  uint32_t index, graph_y;

  // start stream
  WSContentBegin (200, CT_PLAIN);

  // svg : temperature curve
  if (sensor_graph_cfg.arr_graph_flag[SENSOR_DATA_TEMP])
  {
    WSContentSend_P (PSTR ("%s;%s;"), PSTR ("tavg"), PSTR ("points"));
    SensorGraphDisplayAreaData (sensor_graph_cfg.period, SENSOR_TYPE_TEMPAVG);
    WSContentSend_P (PSTR ("\n"));

    WSContentSend_P (PSTR ("%s;%s;"), PSTR ("tmin"), PSTR ("points"));
    SensorGraphDisplayLineData (sensor_graph_cfg.period, SENSOR_TYPE_TEMPMIN);
    WSContentSend_P (PSTR ("\n"));

    WSContentSend_P (PSTR ("%s;%s;"), PSTR ("tmax"), PSTR ("points"));
    SensorGraphDisplayLineData (sensor_graph_cfg.period, SENSOR_TYPE_TEMPMAX);
    WSContentSend_P (PSTR ("\n"));
  }

  // svg : humidity curve
  if (sensor_graph_cfg.arr_graph_flag[SENSOR_DATA_HUMI])
  {
    WSContentSend_P (PSTR ("%s;%s;"), PSTR ("havg"), PSTR ("points"));
    SensorGraphDisplayAreaData (sensor_graph_cfg.period, SENSOR_TYPE_HUMIAVG);
    WSContentSend_P (PSTR ("\n"));

    WSContentSend_P (PSTR ("%s;%s;"), PSTR ("hmin"), PSTR ("points"));
    SensorGraphDisplayLineData (sensor_graph_cfg.period, SENSOR_TYPE_HUMIMIN);
    WSContentSend_P (PSTR ("\n"));

    WSContentSend_P (PSTR ("%s;%s;"), PSTR ("hmax"), PSTR ("points"));
    SensorGraphDisplayLineData (sensor_graph_cfg.period, SENSOR_TYPE_HUMIMAX);
    WSContentSend_P (PSTR ("\n"));
  }

  // svg : power curve
  if (sensor_graph_cfg.arr_graph_flag[SENSOR_DATA_POWR])
  {
    WSContentSend_P (PSTR ("%s;%s;"), PSTR ("pavg"), PSTR ("points"));
    SensorGraphDisplayAreaData (sensor_graph_cfg.period, SENSOR_TYPE_POWRAVG);
    WSContentSend_P (PSTR ("\n"));

    WSContentSend_P (PSTR ("%s;%s;"), PSTR ("pmax"), PSTR ("points"));
    SensorGraphDisplayLineData (sensor_graph_cfg.period, SENSOR_TYPE_POWRMAX);
    WSContentSend_P (PSTR ("\n"));
  }

  
  // svg : presence and activity curves
  graph_y = 250 - SENSOR_GRAPH_BLOCK_HEIGHT;
  if (sensor_graph_cfg.arr_graph_flag[SENSOR_DATA_PRES])
  { 
    WSContentSend_P (PSTR ("%s;%s;"), PSTR ("pres"), PSTR ("d"));
    SensorGraphDisplayBlockData (sensor_graph_cfg.period, SENSOR_TYPE_PRESENCE, graph_y, SENSOR_GRAPH_BLOCK_HEIGHT);
    WSContentSend_P (PSTR ("\n"));
    graph_y -= SENSOR_GRAPH_BLOCK_HEIGHT;
  }
  if (sensor_graph_cfg.arr_graph_flag[SENSOR_DATA_ACTI])
  {
    WSContentSend_P (PSTR ("%s;%s;"), PSTR ("acti"), PSTR ("d"));
    SensorGraphDisplayBlockData (sensor_graph_cfg.period, SENSOR_TYPE_ACTIVITY, graph_y, SENSOR_GRAPH_BLOCK_HEIGHT);
    WSContentSend_P (PSTR ("\n"));
  }

  // end of stream
  WSContentEnd ();
}

void SensorGraphWebCurveDataSelect (const char* pstr_page)
{
  uint8_t index;
  char    str_status[16];
  char    str_text[32];

  WSContentSend_P (PSTR ("<div class='data'>\n"));

  // loop to display graph selection buttons
  for (index = 0; index < SENSOR_DATA_MAX; index++)
  {
    GetTextIndexed (str_text, sizeof (str_text), index, kSensorGraphLabel);
    (sensor_graph_cfg.arr_graph_flag[index]) ? strcpy (str_status, "") : strcpy_P (str_status, PSTR ("disabled"));
    WSContentSend_P (PSTR ("<a href='%s?toggle=%u'><div class='item data%u %s'>%s</div></a>\n"), pstr_page, index, index, str_status, str_text);
  }

  WSContentSend_P (PSTR ("</div>\n"));          // data
}

// Graph page style
void SensorGraphWebPageCurveCSS ()
{
  uint8_t index;
  int     height;
  char    str_text[32];

  // check if height is provided
  height = 60;
  WebGetArgInt (D_CMND_SENSOR_GRAPH_HEIGHT, height, 10, 80);

  // start page as CSS with cache enabled
  WSContentBeginCSS (200, 0);

  // -----------
  //    Page
  // -----------

  WSContentSend_P (PSTR ("body {color:white;background-color:%s;font-size:2.2vmin;font-family:Arial, Helvetica, sans-serif;}\n"), PSTR (COLOR_BACKGROUND));

  WSContentSend_P (PSTR ("div {margin:0.3vh auto;padding:2px 0px;text-align:center;vertical-align:top;}\n"));
  WSContentSend_P (PSTR ("a {text-decoration:none;}\n"));
  WSContentSend_P (PSTR ("div a {color:white;}\n"));

  WSContentSend_P (PSTR ("div.head {font-size:24px;font-weight:bold;}\n"));

  WSContentSend_P (PSTR ("div.graph {width:90%%;margin:2vh auto;}\n"));
  WSContentSend_P (PSTR ("svg.graph {width:100%%;height:%dvh;}\n"), height);

  // period choice
  WSContentSend_P (PSTR ("div.period {display:inline-block;padding:0px 2px;margin:0px;background:none;color:#fff;border:1px #666 solid;border-radius:6px;}\n"));
  WSContentSend_P (PSTR ("div.period div {background:#666;}\n"));
  WSContentSend_P (PSTR ("div.period a div {background:none;}\n"));

  WSContentSend_P (PSTR ("div.item {display:inline-block;padding:2px 20px;border-radius:4px;}\n"));
  WSContentSend_P (PSTR ("a div.item:hover {background:#aaa;}\n"));
  
  // data selection
  WSContentSend_P (PSTR ("div.data div {display:inline-block;min-width:8vw;padding:2px 10px;margin:0px 1vw;border-radius:6px;}\n"));
  for (index = 0; index < SENSOR_DATA_MAX; index ++)
  {
    GetTextIndexed (str_text, sizeof (str_text), index, kSensorGraphColor);
    WSContentSend_P (PSTR ("div.data div.data%u {color:%s;border:1px %s solid;}\n"), index, str_text, str_text);
  }
  WSContentSend_P (PSTR ("div.data div.disabled {color:#666;border-color:#666;}\n"));

  // main mlenu button
  WSContentSend_P (PSTR ("button.menu {background:%s;color:%s;line-height:2rem;font-size:1.2rem;cursor:pointer;border:none;border-radius:0.3rem;width:150px;margin:2vh;}\n"), COLOR_BUTTON, COLOR_BUTTON_TEXT);

  // -----------
  //    SVG
  // -----------

  WSContentSend_P (PSTR ("svg rect.main {fill:none;stroke:#888;stroke-width:1;}\n"));

  WSContentSend_P (PSTR ("svg rect.dash {fill:#333;fill-opacity:0.5;stroke-width:0;}\n"));
  WSContentSend_P (PSTR ("svg line.dash {stroke:grey;stroke-width:1;stroke-dasharray:2 8;}\n"));

  WSContentSend_P (PSTR ("svg text {fill:white;text-anchor:middle;dominant-baseline:middle;font-size:1.2rem;}\n"));
  WSContentSend_P (PSTR ("svg text.time {font-style:italic;fill-opacity:0.6;}\n"));
  WSContentSend_P (PSTR ("svg text.temp {fill:%s;}\n"), PSTR (SENSOR_COLOR_TEMP));
  WSContentSend_P (PSTR ("svg text.humi {fill:%s;}\n"), PSTR (SENSOR_COLOR_HUMI));
  WSContentSend_P (PSTR ("svg text.powr {fill:%s;}\n"), PSTR (SENSOR_COLOR_POWR));

  WSContentSend_P (PSTR ("svg polygon {fill-opacity:0.4;}\n"));
  WSContentSend_P (PSTR ("svg polygon.temp {stroke:%s;fill:%s;}\n"), PSTR (SENSOR_COLOR_TEMP), PSTR (SENSOR_COLOR_TEMP));
  WSContentSend_P (PSTR ("svg polygon.humi {stroke:%s;fill:%s;}\n"), PSTR (SENSOR_COLOR_HUMI), PSTR (SENSOR_COLOR_HUMI));

  WSContentSend_P (PSTR ("svg polyline {fill:none;}\n"));
  WSContentSend_P (PSTR ("svg polyline.temp {stroke:%s;}\n"), PSTR (SENSOR_COLOR_TEMP));
  WSContentSend_P (PSTR ("svg polyline.humi {stroke:%s;}\n"), PSTR (SENSOR_COLOR_HUMI));
  WSContentSend_P (PSTR ("svg polyline.powr {stroke:%s;}\n"), PSTR (SENSOR_COLOR_POWR));

  WSContentSend_P (PSTR ("svg path {fill-opacity:0.4;}\n"));
  WSContentSend_P (PSTR ("svg path.pres {stroke:%s;fill:%s;}\n"), PSTR (SENSOR_COLOR_PRES), PSTR (SENSOR_COLOR_PRES));
  WSContentSend_P (PSTR ("svg path.acti {stroke:%s;fill:%s;}\n"), PSTR (SENSOR_COLOR_ACTI), PSTR (SENSOR_COLOR_ACTI));

  WSContentEnd ();
}

void SensorGraphWebPageCurveJS ()
{
  // start page as Javascript with cache enabled
  WSContentBeginJavascript (200, 0);

  WSContentSend_P (PSTR ("function updateCurve(){\n"));

  WSContentSend_P (PSTR (" httpData=new XMLHttpRequest();\n"));
  WSContentSend_P (PSTR (" httpData.open('GET','%s',true);\n"), PSTR_SENSOR_PAGE_CURV_UPD);
  WSContentSend_P (PSTR (" httpData.onreadystatechange=function(){\n"));
  WSContentSend_P (PSTR ("  if (httpData.readyState===XMLHttpRequest.DONE){\n"));
  WSContentSend_P (PSTR ("   if (httpData.status===0 || (httpData.status>=200 && httpData.status<400)){\n"));
  WSContentSend_P (PSTR ("    arr_param=httpData.responseText.split('\\n');\n"));
  WSContentSend_P (PSTR ("    for (i=0; i<arr_param.length; i++) {\n"));
  WSContentSend_P (PSTR ("     arr_value=arr_param[i].split(';');\n"));
  WSContentSend_P (PSTR ("     if (arr_value.length === 3) document.getElementById(arr_value[0]).setAttribute(arr_value[1],arr_value[2]);\n"));
  WSContentSend_P (PSTR ("    }\n"));
  WSContentSend_P (PSTR ("   }\n"));
  WSContentSend_P (PSTR ("   setTimeout(updateCurve,%u);\n"), SENSOR_GRAPH_DATA_UPDATE);      // ask for update every 10 sec
  WSContentSend_P (PSTR ("  }\n"));
  WSContentSend_P (PSTR (" }\n"));
  WSContentSend_P (PSTR (" httpData.send();\n"));

  WSContentSend_P (PSTR ("}\n"));

  WSContentSend_P (PSTR ("setTimeout(updateCurve,%u);\n"), SENSOR_GRAPH_DATA_UPDATE);         // ask for first update after 10 sec

  WSContentEnd ();
}

// Graph public page
void SensorGraphWebPageCurve ()
{
  uint8_t  index;
  uint32_t timestart;
  int      toggle;
  char     str_text[8];

  // timestamp
  timestart = millis ();

  // beginning of form without authentification
  WSContentStart_P (PSTR ("Sensors"), false);

  // get period
  strcpy_P (str_text, PSTR ("period"));
  sensor_graph_cfg.period = (uint8_t)SensorGetArgValue (str_text, 0, SENSOR_PERIOD_MAX - 1, sensor_graph_cfg.period);  

  // check data display argument
  strcpy_P (str_text, PSTR ("toggle"));
  toggle = SensorGetArgValue (str_text, 0, SENSOR_DATA_MAX - 1, -1); 
  if (toggle >= 0) sensor_graph_cfg.arr_graph_flag[toggle] = !sensor_graph_cfg.arr_graph_flag[toggle];

  // end of script section
  WSContentSend_P (PSTR ("</script>\n"));

  // set page as scalable
  WSContentSend_P (PSTR ("<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=yes'/>\n"));

  // load style sheet (height 60)
  WSContentSend_P (PSTR ("<link rel='stylesheet' type='text/css' href='%s?ts=%u&height=60'>\n"), PSTR_SENSOR_PAGE_CURV_CSS, Rtc.restart_time);

  // if live display load javascript
  if (sensor_graph_cfg.period == SENSOR_PERIOD_LIVE) WSContentSend_P (PSTR ("<script type='text/javascript' src='%s?ts=%u'></script>\n"), PSTR_SENSOR_PAGE_CURV_JS, Rtc.restart_time);

  // page body
  WSContentSend_P (PSTR ("</head>\n"));
  WSContentSend_P (PSTR ("<body>\n"));

  WSContentSend_P (PSTR ("<div><div class='head'>Sensors</div></div>\n"));

  // Level 1 - Period types
  WSContentSend_P (PSTR ("<div><div class='period'>\n"));
  for (index = SENSOR_PERIOD_LIVE; index < SENSOR_PERIOD_MAX; index ++)
  {
    GetTextIndexed (str_text, sizeof (str_text), index, kSensorGraphPeriod);
    if (sensor_graph_cfg.period != index) WSContentSend_P (PSTR ("<a href='%s?period=%u'>"), PSTR_SENSOR_PAGE_CURV_PAGE, index);
    WSContentSend_P (PSTR ("<div class='item'>%s</div>"), str_text);
    if (sensor_graph_cfg.period != index) WSContentSend_P (PSTR ("</a>"));
    WSContentSend_P (PSTR ("\n"));
  }
  WSContentSend_P (PSTR ("</div></div>\n"));

  // SVG
  SensorGraphWebCurveSVG ();

  // Data selection
  SensorGraphWebCurveDataSelect (PSTR_SENSOR_PAGE_CURV_PAGE);

  // main menu button
  WSContentSend_P (PSTR ("<div><form action='/' method='get'><button class='menu'>%s</button></form></div>\n"), D_MAIN_MENU);

  // end of page
  WSContentStop ();

  // log page serving time
  AddLog (LOG_LEVEL_DEBUG, PSTR ("SEN: Graph in %ums"), millis () - timestart);
}

// ------------------------------
//    Presence / Activity Graph
// ------------------------------

void SensorGraphWebPagePresenceCSS () { SensorGraphWebPagePlanningCSS (PSTR (SENSOR_COLOR_PRES)); }
void SensorGraphWebPageActivityCSS () { SensorGraphWebPagePlanningCSS (PSTR (SENSOR_COLOR_ACTI)); }
void SensorGraphWebPagePlanningCSS (const char* pstr_color)
{
  // start page as CSS with cache enabled
  WSContentBeginCSS (200, 0);

  // -------------
  //     Page
  // -------------

  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));

  WSContentSend_P (PSTR ("div {margin:0.25rem auto;padding:0.1rem 0px;text-align:center;}\n"));
  WSContentSend_P (PSTR ("div.title {font-size:3.5vh;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.header {font-size:2.5vh;margin:2vh auto;}\n"));

  WSContentSend_P (PSTR ("a {color:white;}\n")); 
  WSContentSend_P (PSTR ("a:link {text-decoration:none;}\n"));

    // period choice
  WSContentSend_P (PSTR ("div.period {display:inline-block;padding:0px 2px;margin:0px;background:none;color:#fff;border:1px #666 solid;border-radius:6px;}\n"));
  WSContentSend_P (PSTR ("div.period div {background:#666;}\n"));
  WSContentSend_P (PSTR ("div.period a div {background:none;}\n"));

  WSContentSend_P (PSTR ("div.item {display:inline-block;padding:2px 20px;border-radius:4px;}\n"));
  WSContentSend_P (PSTR ("a div.item:hover {background:#aaa;}\n"));

  // main menu button
  WSContentSend_P (PSTR ("button.menu {background:%s;color:%s;line-height:2rem;font-size:1.2rem;cursor:pointer;border:none;border-radius:0.3rem;width:150px;margin:2vh;}\n"), COLOR_BUTTON, COLOR_BUTTON_TEXT);

  WSContentSend_P (PSTR("div.graph {width:90%%;margin:2vh auto;}\n"));
  WSContentSend_P (PSTR("svg.graph {width:100%%;height:55vh;}\n"));

  // -------------
  //      SVG
  // -------------

  WSContentSend_P (PSTR ("svg text {font-size:3vh;fill:%s;}\n"), PSTR (SENSOR_COLOR_TIME));
  WSContentSend_P (PSTR ("svg text.today {font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("svg rect {opacity:0.8;}\n"));
  WSContentSend_P (PSTR ("svg rect.s0 {stroke:none;fill:%s;}\n"), "#333");
  WSContentSend_P (PSTR ("svg rect.s1 {stroke:none;fill:%s;}\n"), pstr_color);
  WSContentSend_P (PSTR ("svg line {stroke:%s;stroke-dasharray:2 6;}\n"), PSTR (SENSOR_COLOR_LINE));

  WSContentEnd ();
}

// Presence / activity page
void SensorGraphWebPagePresence () { SensorGraphWebPagePlanning (SENSOR_TYPE_PRESENCE, PSTR_SENSOR_PAGE_PRES_CSS, PSTR_SENSOR_PAGE_PRES_PAGE); }
void SensorGraphWebPageActivity () { SensorGraphWebPagePlanning (SENSOR_TYPE_ACTIVITY, PSTR_SENSOR_PAGE_ACTI_CSS, PSTR_SENSOR_PAGE_ACTI_PAGE); }
void SensorGraphWebPagePlanning (const uint8_t type, const char* pstr_css, const char* pstr_page)
{
  uint8_t index;
  char    str_text[8];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // get display period
  strcpy_P (str_text, PSTR ("period"));
  sensor_graph_cfg.period = (uint8_t)SensorGetArgValue (str_text, SENSOR_PERIOD_WEEK, SENSOR_PERIOD_YEAR, sensor_graph_cfg.period);  
  if (sensor_graph_cfg.period < SENSOR_PERIOD_WEEK) sensor_graph_cfg.period = SENSOR_PERIOD_WEEK;

  // beginning page without authentification
  WSContentStart_P (PSTR ("Presence"), false);

  // graph swipe section script
  WSContentSend_P (PSTR ("</script>\n"));

  // set page as scalable
  WSContentSend_P (PSTR ("<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=yes'/>\n"));

  // load style sheet
  WSContentSend_P (PSTR ("<link rel='stylesheet' type='text/css' href='%s?ts=%u'>\n"), pstr_css, Rtc.restart_time);

  WSContentSend_P (PSTR ("</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  // room name and header
  WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText(SET_DEVICENAME));
  WSContentSend_P (PSTR ("<div class='header'>Presence</div>\n"));

  // ---------------------------
  //    Level 1 - Period types
  // ---------------------------

  WSContentSend_P (PSTR ("<div><div class='period'>\n"));
  for (index = SENSOR_PERIOD_WEEK; index < SENSOR_PERIOD_MAX; index ++)
  {
    GetTextIndexed (str_text, sizeof (str_text), index, kSensorGraphPeriod);
    if (sensor_graph_cfg.period != index) WSContentSend_P (PSTR ("<a href='%s?period=%u'>"), pstr_page, index);
    WSContentSend_P (PSTR ("<div class='item'>%s</div>"), str_text);
    if (sensor_graph_cfg.period != index) WSContentSend_P (PSTR ("</a>"));
    WSContentSend_P (PSTR ("\n"));
  }
  WSContentSend_P (PSTR ("</div></div>\n"));

  // -------------
  //      SVG
  // -------------

  // start of SVG graph
  WSContentSend_P (PSTR ("<div class='graph'>\n"));
  WSContentSend_P (PSTR ("<svg class='graph' viewBox='0 0 %u %u' preserveAspectRatio='none'>\n"), SENSOR_GRAPH_WIDTH, SENSOR_GRAPH_HEIGHT + 1);

  // display SVG
  switch (sensor_graph_cfg.period)
  {
    case SENSOR_PERIOD_WEEK: SensorGraphWebPlanningWeekSVG (type); break;
    case SENSOR_PERIOD_YEAR: SensorGraphWebPlanningYearSVG (type); break;
  }

  // ---  End  ---
  WSContentSend_P (PSTR ("</svg>\n"));      // graph
  WSContentSend_P (PSTR ("</div>\n"));      // graph

  // ----------------
  //    End of page
  // ----------------

  // main menu button
  WSContentSend_P (PSTR ("<div><form action='/' method='get'><button class='menu'>%s</button></form></div>\n"), D_MAIN_MENU);

  // end of page
  WSContentStop ();
}

// weekly presence graph
void SensorGraphWebPlanningWeekSVG (const uint8_t type)
{
  uint8_t  slot_state, slot_new;
  uint16_t slot, day, day_first, day_last, day_idx;
  uint16_t line_height, bar_height;
  uint16_t last_x, graph_x, graph_y;
  uint32_t index;
  char     str_text[8];
  char     str_style[8];

  // validate type
  if ((type != SENSOR_TYPE_PRESENCE) && (type != SENSOR_TYPE_ACTIVITY)) return;

  // calculate lines and slot dimension
  graph_y     = 0;
  line_height = SENSOR_GRAPH_HEIGHT / 8;
  bar_height  = line_height - 5;

  // loop thru days
  day_first = (uint16_t)SensorGraphGetStartSlot (SENSOR_PERIOD_WEEK);
  day_last  = day_first + 7;
  for (day_idx = day_first; day_idx < day_last; day_idx ++)
  {
    // calculate day index
    day = day_idx % 7;

    // get upper line position
    graph_y += line_height;

    // display week day
    strlcpy (str_text, kWeekdayNames + day * 3, 4);
    (day == day_last - 1) ? strcpy_P (str_style, PSTR ("today")) : strcpy_P (str_style, PSTR ("head"));
    WSContentSend_P (PSTR ("<text class='%s' text-anchor='start' x=%u y=%u>%s</text>\n"), str_style, 20, graph_y + bar_height - 25, str_text);

    // loop to display slots of current day
    last_x = SENSOR_GRAPH_PRES_HEAD;
    slot_state = (arr_graph_week[type][day][0] == 1);
    for (slot = 0; slot < SENSOR_SLOT_PER_DAY; slot ++)
    {
      // read slot status and handle last slot
      slot_new = (arr_graph_week[type][day][slot] == 1);
      if (slot == SENSOR_SLOT_PER_DAY - 1) slot++;

      // if needed, display presence
      if ((slot_state != slot_new) || (slot == SENSOR_SLOT_PER_DAY))
      {
        graph_x = SENSOR_GRAPH_PRES_HEAD + SENSOR_GRAPH_PRES_LINE * slot / SENSOR_SLOT_PER_DAY;
        WSContentSend_P (PSTR ("<rect class='s%u' x=%u y=%u width=%u height=%u />\n"), slot_state, last_x, graph_y, graph_x - last_x, bar_height);
        last_x     = graph_x;
        slot_state = slot_new;
      }
    }
  }

  // loop to display headers and separation lines every 2h
  for (slot = 1; slot < 12; slot++)
  {
    graph_x = SENSOR_GRAPH_PRES_HEAD + SENSOR_GRAPH_PRES_LINE * slot / 12;
    WSContentSend_P (PSTR ("<text class='time' text-anchor='middle' x=%u y=%u>%02uh</text>\n"), graph_x, bar_height - 10, slot * 2);
    WSContentSend_P (PSTR ("<line x1=%u y1=%u x2=%u y2=%u />\n"), graph_x, line_height, graph_x, graph_y + bar_height);
  }
}

// yearly presence graph
void SensorGraphWebPlanningYearSVG (const uint8_t type)
{
  uint8_t  slot_state, slot_new, days_in_month;
  uint32_t index, month, day, timestamp;
  uint32_t last_x, graph_x, graph_y, line_height, bar_height;
  TIME_T   time_dst;
  char     str_name[4];

  // validate type
  if ((type != SENSOR_TYPE_PRESENCE) && (type != SENSOR_TYPE_ACTIVITY)) return;

  // calculate line height and bar height
  line_height = SENSOR_GRAPH_HEIGHT / 13;
  bar_height  = line_height - 5;

  // get first day of 11 months back
  timestamp = CalendarGetFirstDayOfMonth (LocalTime ());
  timestamp = CalendarGetPreviousTimestamp (CALENDAR_PERIOD_MONTH, timestamp, 11);
  for (index = 0; index < 12; index ++)
  {
    // get upper line position
    graph_y = line_height * (index + 1);

    // calculate month start data
    BreakTime (timestamp, time_dst);
    month = time_dst.month - 1;
    days_in_month = CalendarGetDaysInMonth (timestamp);

    // month name
    strlcpy (str_name, kMonthNames + month * 3, 4);
    WSContentSend_P (PSTR ("<text class='head' text-anchor='start' x=%u y=%u>%s</text>\n"), 20, graph_y + bar_height - 15, str_name);

    // loop to display slots of days
    last_x = SENSOR_GRAPH_PRES_HEAD;
    slot_state = (arr_graph_year[type][month][0] == 1);
    for (day = 0; day < days_in_month; day++)
    {
      // retrieve presence from slot and handle last slot
      slot_new = (arr_graph_year[type][month][day] == 1);
      if (day == days_in_month - 1) day++;

      // if needed, display presence
      if ((slot_state != slot_new) || (day == days_in_month))
      {
        graph_x = SENSOR_GRAPH_PRES_HEAD + SENSOR_GRAPH_PRES_LINE * day / SENSOR_SLOT_PER_MONTH;
        WSContentSend_P (PSTR ("<rect class='s%u' x=%u y=%u width=%u height=%u />\n"), slot_state, last_x, graph_y, graph_x - last_x, bar_height);
        last_x = graph_x;
        slot_state = slot_new;
      }
    }

    // passer au mois suivant
    timestamp = CalendarGetNextTimestamp (CALENDAR_PERIOD_MONTH, timestamp, 1);
  }

  // loop to display headers and separation lines every 5 days
  for (index = 1; index < 7; index++)
  {
    graph_x = SENSOR_GRAPH_PRES_HEAD + (10 * index - 1) * SENSOR_GRAPH_PRES_LINE / SENSOR_SLOT_PER_MONTH / 2;
    WSContentSend_P (PSTR ("<text class='time' text-anchor='middle' x=%u y=%u>%02u</text>\n"), graph_x, line_height - 10, index * 5);
    WSContentSend_P (PSTR ("<line x1=%u y1=%u x2=%u y2=%u />\n"), graph_x, line_height, graph_x, graph_y + bar_height);
  }
}

#endif    // USE_WEBSERVER

/***************************************\
 *              Interface
\***************************************/

bool XsnsSensorGraph (uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function) 
  {
    case FUNC_INIT:
      SensorGraphInit ();
      break;

    case FUNC_SAVE_BEFORE_RESTART:
      SensorGraphSaveBeforeRestart ();
      break;

    case FUNC_EVERY_SECOND:
      SensorGraphEverySecond ();
      break;

#ifdef USE_WEBSERVER

     case FUNC_WEB_ADD_MAIN_BUTTON:
      SensorGraphWebMainButton ();
      break;

    case FUNC_WEB_ADD_HANDLER:
      Webserver->on (FPSTR (PSTR_SENSOR_PAGE_CURV_PAGE), SensorGraphWebPageCurve);
      Webserver->on (FPSTR (PSTR_SENSOR_PAGE_CURV_CSS),  SensorGraphWebPageCurveCSS);
      Webserver->on (FPSTR (PSTR_SENSOR_PAGE_CURV_JS),   SensorGraphWebPageCurveJS);
      Webserver->on (FPSTR (PSTR_SENSOR_PAGE_CURV_UPD),  SensorGraphWebPageCurveUpdate);
      Webserver->on (FPSTR (PSTR_SENSOR_PAGE_PRES_PAGE), SensorGraphWebPagePresence);
      Webserver->on (FPSTR (PSTR_SENSOR_PAGE_PRES_CSS),  SensorGraphWebPagePresenceCSS);
      Webserver->on (FPSTR (PSTR_SENSOR_PAGE_ACTI_PAGE), SensorGraphWebPageActivity);
      Webserver->on (FPSTR (PSTR_SENSOR_PAGE_ACTI_CSS),  SensorGraphWebPageActivityCSS);
      break;

#endif  // USE_WEBSERVER
  }

  return result;
}

#endif    // USE_SENSOR_GRAPH
