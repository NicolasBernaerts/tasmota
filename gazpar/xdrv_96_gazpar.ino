/*
  xdrv_96_gazpar.ino - Enedis Gazpar Gas meter support for Tasmota

  Copyright (C) 2022-2024  Nicolas Bernaerts

  Gazpar impulse connector should be declared as Counter 1
  To be on the safe side and to avoid false trigger, set :
    - CounterDebounce 150
    - CounterDebounceHigh 50
    - CounterDebounceLow 50

  Config is stored in LittleFS partition in /gazpar.cfg

  Version history :
    28/04/2022 - v1.0 - Creation
    06/11/2022 - v1.1 - Rename to xsns_119
    04/12/2022 - v1.2 - Add graphs
    15/05/2023 - v1.3 - Rewrite CSV file access
    07/05/2024 - v2.0 - JSON data change
                        Add active power calculation
                        Homie and Home Assistant integration

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
#ifdef USE_UFILESYS

/*************************************************\
 *               Variables
\*************************************************/

// declare gazpar energy driver
#define XDRV_96                     96

// web strings
#define D_GAZPAR_CONFIG             "Configure Gazpar"
#define D_GAZPAR_POWER              "Power"
#define D_GAZPAR_GRAPH              "Graph"
#define D_GAZPAR_TODAY              "Today"
#define D_GAZPAR_TOTAL              "Total"

#define D_GAZPAR_FACTOR             "Conversion factor"

// commands
#define D_CMND_GAZPAR_HELP          "help"
#define D_CMND_GAZPAR_FACTOR        "factor"
#define D_CMND_GAZPAR_HOUR          "hour"
#define D_CMND_GAZPAR_TODAY         "today"
#define D_CMND_GAZPAR_YESTERDAY     "yesterday"
#define D_CMND_GAZPAR_TOTAL         "total"
#define D_CMND_GAZPAR_MAX_HOUR      "maxhour"
#define D_CMND_GAZPAR_MAX_DAY       "maxday"
#define D_CMND_GAZPAR_MAX_MONTH     "maxmonth"
#define D_CMND_GAZPAR_HISTO         "histo"
#define D_CMND_GAZPAR_MONTH         "month"
#define D_CMND_GAZPAR_DAY           "day"
#define D_CMND_GAZPAR_INCREMENT     "incr"
#define D_CMND_GAZPAR_DECREMENT     "decr"
#define D_CMND_GAZPAR_PREV          "prev"
#define D_CMND_GAZPAR_NEXT          "next"
#define D_CMND_GAZPAR_PLUS          "plus"
#define D_CMND_GAZPAR_MINUS         "minus"
#define D_CMND_GAZPAR_HOMIE         "homie"
#define D_CMND_GAZPAR_HASS          "hass"

// default values
#define GAZPAR_FACTOR_DEFAULT         1120
#define GAZPAR_INCREMENT_MAXTIME      360000    // 6mn (equivalent to 500W, a small gaz cooker)
#define GAZPAR_INCREMENT_TIMEOUT      10000     // 10s (power will go to 0 after this timeout)

// graph data
#define GAZPAR_GRAPH_WIDTH            1200      // graph width
#define GAZPAR_GRAPH_HEIGHT           600       // default graph height
#define GAZPAR_GRAPH_PERCENT_START    5         // start position of graph window
#define GAZPAR_GRAPH_PERCENT_STOP     95        // stop position of graph window

#define GAZPAR_GRAPH_MAX_BARGRAPH     32        // maximum number of bar graph
#define GAZPAR_GRAPH_DEF_KWH_HOUR     4
#define GAZPAR_GRAPH_DEF_KWH_DAY      16
#define GAZPAR_GRAPH_DEF_KWH_MONTH    128
//#define GAZPAR_GRAPH_MIN_KWH_HOUR     1
//#define GAZPAR_GRAPH_INC_KWH_HOUR     1
//#define GAZPAR_GRAPH_MIN_KWH_DAY      5
//#define GAZPAR_GRAPH_INC_KWH_DAY      5
//#define GAZPAR_GRAPH_MIN_KWH_MONTH    50
//#define GAZPAR_GRAPH_INC_KWH_MONTH    50

// web URL
#define D_GAZPAR_PAGE_CONFIG          "/cfg"
#define D_GAZPAR_PAGE_GRAPH           "/graph"
#define D_GAZPAR_ICON_SVG             "/gazpar.svg"

// files
#define D_GAZPAR_CFG                  "/gazpar.cfg"
#define D_GAZPAR_CSV                  "/gazpar-year-%04u.csv" 

// Gazpar - MQTT commands : gaz_count
const char kGazparCommands[]          PROGMEM = "gaz_" "|" D_CMND_GAZPAR_HELP "|" D_CMND_GAZPAR_FACTOR "|" D_CMND_GAZPAR_TOTAL "|" D_CMND_GAZPAR_MAX_HOUR "|" D_CMND_GAZPAR_MAX_DAY "|" D_CMND_GAZPAR_MAX_MONTH;
void (* const GazparCommand[])(void)  PROGMEM = { &CmndGazparHelp, &CmndGazparFactor, &CmndGazparTotal, &CmndGazparMaxHour, &CmndGazparMaxDay, &CmndGazparMaxMonth };

// month and week day names
const char kGazparYearMonth[] PROGMEM = "|Jan|Fév|Mar|Avr|Mai|Jun|Jui|Aoû|Sep|Oct|Nov|Déc";         // month name for selection
const char kGazparWeekDay[]   PROGMEM = "Lun|Mar|Mer|Jeu|Ven|Sam|Dim";                              // day name for selection
const char kGazparWeekDay2[]  PROGMEM = "lu|ma|me|je|ve|sa|di";                                     // day name for bar graph

// config
enum GazparConfigKey { GAZPAR_CONFIG_FACTOR, GAZPAR_CONFIG_TODAY, GAZPAR_CONFIG_YESTERDAY, GAZPAR_CONFIG_KWH_HOUR, GAZPAR_CONFIG_KWH_DAY, GAZPAR_CONFIG_KWH_MONTH, GAZPAR_CONFIG_MAX };          // configuration parameters
const char kGazparConfigKey[] PROGMEM = D_CMND_GAZPAR_FACTOR "|" D_CMND_GAZPAR_TODAY "|" D_CMND_GAZPAR_YESTERDAY "|" D_CMND_GAZPAR_MAX_HOUR "|" D_CMND_GAZPAR_MAX_DAY "|" D_CMND_GAZPAR_MAX_MONTH;     // configuration keys

// published data 
enum GazparPublish { GAZ_PUB_CONNECT,
                     GAZ_PUB_DATA, GAZ_PUB_FACTOR, GAZ_PUB_POWER,
                     GAZ_PUB_M3, GAZ_PUB_M3_TOTAL, GAZ_PUB_M3_TODAY, GAZ_PUB_M3_YTDAY, 
                     GAZ_PUB_WH, GAZ_PUB_WH_TOTAL, GAZ_PUB_WH_TODAY, GAZ_PUB_WH_YTDAY, 
                     GAZ_PUB_MAX, 
                     GAZ_PUB_DISCONNECT };

/*********************************\
 *              Data
\*********************************/

// gazpar : configuration
static struct {
  long  param[GAZPAR_CONFIG_MAX];                 // configuration parameters
} gazpar_config;

// gazpar : meter
static struct {
  uint8_t  hour        = UINT8_MAX;               // current hour
  uint32_t last_stamp  = UINT32_MAX;              // millis of last detected increment
  uint32_t last_length = 0;                       // length of last increment
  uint32_t last_count  = UINT32_MAX;              // last counter value
  long     last_start  = 0;                       // previous daily total
  long     last_hour   = 0;                       // previous hour total
  long     power       = 0;                       // active power
  long     count_hour[24];                        // hourly increments
} gazpar_meter;

// gazpar : graph data
static struct {
  long    left  = GAZPAR_GRAPH_WIDTH * GAZPAR_GRAPH_PERCENT_START / 100;                                  // left position of the curve
  long    right = GAZPAR_GRAPH_WIDTH * GAZPAR_GRAPH_PERCENT_STOP  / 100;                                  // right position of the curve
  long    width = GAZPAR_GRAPH_WIDTH * (GAZPAR_GRAPH_PERCENT_STOP - GAZPAR_GRAPH_PERCENT_START) / 100;    // width of the curve (in pixels)
  uint8_t histo = 0;                                    // graph historisation index
  uint8_t month = 0;                                    // graph current month
  uint8_t day = 0;                                      // graph current day
  char    str_filename[UFS_FILENAME_SIZE];              // graph current file
} gazpar_graph;


/****************************************\
 *               Icons
\****************************************/

// gazpar icon
const char gazpar_icon_svg[] PROGMEM =
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='230 70 350 440'>"
"<defs><style>.cls-1{fill:#f1f2f3;}.cls-2{fill:#f8cd46;}.cls-3{fill:#f6c243;}.cls-4{fill:#fff;}.cls-5,.cls-9{fill:#d8e0ed;}.cls-5{opacity:0.38;}.cls-6{fill:#353a4b;}.cls-7{fill:#e15f60;}.cls-8{font-size:12.56px;fill:#e3e9fc;font-family:Nunito-Black, Nunito;font-weight:800;}.cls-10{fill:#cfd7e3;}</style></defs>"
"<rect class='cls-1' x='236.74' y='114.95' width='334.03' height='391.29' rx='40.21'/>"
"<path class='cls-2' d='M565.77,463.9c0,20-16.54,36.37-36.76,36.37H278.49c-20.21,0-36.75-16.37-36.75-36.37v-308c0-20,16.54-36.38,36.75-36.38H529c20.22,0,36.76,16.37,36.76,36.38Z'/>"
"<path class='cls-3' d='M527.86,489.39H279.65A27.66,27.66,0,0,1,252,461.73V158.25a27.67,27.67,0,0,1,27.67-27.67H527.86a27.66,27.66,0,0,1,27.66,27.66V282.31c0,8-6.15,14.94-14.13,15.18a14.58,14.58,0,0,1-15-14.57V162.07a2.32,2.32,0,0,0-2.32-2.32H283.47a2.32,2.32,0,0,0-2.32,2.32V457.9a2.32,2.32,0,0,0,2.32,2.32H524a2.32,2.32,0,0,0,2.32-2.32V357.64a2.81,2.81,0,0,0-2.8-2.8H323.94c-8,0-14.95-6.15-15.19-14.13a14.59,14.59,0,0,1,14.58-15h225.8a6.39,6.39,0,0,1,6.39,6.39V461.73A27.66,27.66,0,0,1,527.86,489.39Z'/>"
"<rect class='cls-4' x='310.65' y='176.43' width='132.25' height='100.08' rx='6.89'/><rect class='cls-5' x='327.22' y='191' width='40.06' height='8.08' rx='4.04'/>"
"<rect class='cls-5' x='327.22' y='255.32' width='40.06' height='4.04' rx='2.02'/><rect class='cls-5' x='327.22' y='263.3' width='26.21' height='4.04' rx='2.02'/><rect class='cls-5' x='371.58' y='255.32' width='23.09' height='4.04' rx='2.02'/>"
"<rect class='cls-5' x='327.22' y='205.72' width='4.04' height='8.08' rx='1.28'/><rect class='cls-5' x='332.78' y='205.72' width='1.1' height='8.08' rx='0.55'/><rect class='cls-5' x='334.75' y='205.72' width='1.93' height='8.08' rx='0.73'/>"
"<rect class='cls-5' x='337.71' y='205.72' width='3.28' height='8.08' rx='0.95'/><rect class='cls-5' x='342.3' y='205.72' width='0.99' height='8.08' rx='0.5'/><rect class='cls-5' x='344.76' y='205.72' width='1.37' height='8.08' rx='0.58'/>"
"<rect class='cls-5' x='351.47' y='205.72' width='1.1' height='8.08' rx='0.55'/><rect class='cls-5' x='353.43' y='205.72' width='1.93' height='8.08' rx='0.73'/><rect class='cls-5' x='360' y='205.72' width='0.99' height='8.08' rx='0.5' transform='translate(720.99 419.51) rotate(180)'/>"
"<rect class='cls-5' x='357.17' y='205.72' width='1.37' height='8.08' rx='0.58' transform='translate(715.71 419.52) rotate(180)'/><rect class='cls-5' x='347.25' y='205.71' width='2.71' height='8.08' rx='0.82'/>"
"<rect class='cls-5' x='382.61' y='205.71' width='3.28' height='8.08' rx='0.95' transform='translate(768.5 419.5) rotate(180)'/><rect class='cls-5' x='380.3' y='205.71' width='0.99' height='8.08' rx='0.5' transform='translate(761.6 419.5) rotate(180)'/>"
"<rect class='cls-5' x='377.48' y='205.72' width='1.37' height='8.08' rx='0.58' transform='translate(756.32 419.51) rotate(180)'/><rect class='cls-5' x='371.03' y='205.71' width='1.1' height='8.08' rx='0.55' transform='translate(743.17 419.5) rotate(180)'/>"
"<rect class='cls-5' x='368.24' y='205.71' width='1.93' height='8.08' rx='0.73' transform='translate(738.41 419.5) rotate(180)'/><rect class='cls-5' x='362.61' y='205.72' width='0.99' height='8.08' rx='0.5'/><rect class='cls-5' x='365.06' y='205.71' width='1.37' height='8.08' rx='0.58'/>"
"<rect class='cls-5' x='373.65' y='205.72' width='2.71' height='8.08' rx='0.82' transform='translate(750 419.52) rotate(180)'/>"
"<rect class='cls-6' x='320.22' y='224.96' width='170.23' height='23.03' rx='3.99'/>"
"<path class='cls-7' d='M399,225h32.07a4,4,0,0,1,4,4V244a4,4,0,0,1-4,4H391.39a0,0,0,0,1,0,0V225a0,0,0,0,1,0,0Z'/>"
"<text class='cls-8' id='digit' transform='translate(324 240)'>0 0 0 0 0</text><text class='cls-8' id='milli' transform='translate(395 240)'>0 0 0</text>"
"<path class='cls-9' d='M306.25,71.65h51.23a4.41,4.41,0,0,1,4.41,4.41v38.89a0,0,0,0,1,0,0h-60a0,0,0,0,1,0,0V76.06A4.41,4.41,0,0,1,306.25,71.65Z'/>"
"<path class='cls-10' d='M299.56,103.48h64.61a5,5,0,0,1,5,5v6.51a0,0,0,0,1,0,0H294.59a0,0,0,0,1,0,0v-6.51A5,5,0,0,1,299.56,103.48Z'/>"
"<path class='cls-9' d='M455.38,71.65h51.23A4.41,4.41,0,0,1,511,76.06v38.89a0,0,0,0,1,0,0H451a0,0,0,0,1,0,0V76.06A4.41,4.41,0,0,1,455.38,71.65Z'/>"
"<path class='cls-10' d='M448.7,103.48H513.3a5,5,0,0,1,5,5v6.51a0,0,0,0,1,0,0H443.73a0,0,0,0,1,0,0v-6.51A5,5,0,0,1,448.7,103.48Z'/>"
"<rect class='cls-3' x='310.65' y='402.72' width='64.59' height='8.3' rx='4.15'/><rect class='cls-3' x='310.65' y='418.56' width='40.68' height='8.3' rx='4.15'/>"
"</svg>";

/************************************\
 *             Commands
\************************************/

void CmndGazparHelp (void)
{
  AddLog (LOG_LEVEL_INFO,   PSTR ("HLP: Gazpar commands"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - gaz_factor <value>   = set conversion factor"));
  AddLog (LOG_LEVEL_INFO,   PSTR ("   https://www.grdf.fr/particuliers/coefficient-conversion-commune"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - gaz_total <value>    = adjust counter grand total"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - gaz_maxhour <value>  = maximum kWh in Hour graph"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - gaz_maxday <value>   = maximum kWh in Day graph"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - gaz_maxmonth <value> = maximum kWh in Month graph"));

  ResponseCmndDone();
}

void CmndGazparFactor (void)
{
  float factor;

  if (XdrvMailbox.data_len > 0)
  {
    factor = 100 * atof (XdrvMailbox.data);
    gazpar_config.param[GAZPAR_CONFIG_FACTOR] = (long)factor;
    GazparSaveConfig ();
  }
  factor = (float)gazpar_config.param[GAZPAR_CONFIG_FACTOR] / 100;
  ResponseCmndFloat (factor, 2);
}

void CmndGazparTotal (void)
{
  // update new counters according to new total
  if (XdrvMailbox.data_len > 0) GazparSetTotal ((long)XdrvMailbox.payload);

  ResponseCmndNumber (RtcSettings.pulse_counter[0]);
}

void CmndGazparMaxHour (void)
{
  // if defined, update maximum graph hour range
  if (XdrvMailbox.data_len > 0) gazpar_config.param[GAZPAR_CONFIG_KWH_HOUR] = max (1L, atol (XdrvMailbox.data));
  ResponseCmndNumber (gazpar_config.param[GAZPAR_CONFIG_KWH_HOUR]);
}

void CmndGazparMaxDay (void)
{
  // if defined, update maximum graph day range
  if (XdrvMailbox.data_len > 0) gazpar_config.param[GAZPAR_CONFIG_KWH_HOUR] = max (1L, atol (XdrvMailbox.data));
  ResponseCmndNumber (gazpar_config.param[GAZPAR_CONFIG_KWH_HOUR]);
}

void CmndGazparMaxMonth (void)
{
  // if defined, update maximum graph day range
  if (XdrvMailbox.data_len > 0) gazpar_config.param[GAZPAR_CONFIG_KWH_MONTH] = max (1L, atol (XdrvMailbox.data));
  ResponseCmndNumber (gazpar_config.param[GAZPAR_CONFIG_KWH_MONTH]);
}

/************************************\
 *            Functions
\************************************/

// Load configuration from Settings or from LittleFS
void GazparLoadConfig () 
{
  // load littlefs settings
#ifdef USE_UFILESYS
  int    index;
  char   str_text[16];
  char   str_line[64];
  char   *pstr_key, *pstr_value;
  File   file;

  // init parameters
  for (index = 0; index < GAZPAR_CONFIG_MAX; index++) gazpar_config.param[index] = 0;

  // if file exists, open and read each line
  if (ffsp->exists (D_GAZPAR_CFG))
  {
    file = ffsp->open (D_GAZPAR_CFG, "r");
    while (file.available ())
    {
      // read current line and extract key and value
      index = file.readBytesUntil ('\n', str_line, sizeof (str_line) - 1);
      if (index >= 0) str_line[index] = 0;
      pstr_key   = strtok (str_line, "=");
      pstr_value = strtok (nullptr,  "=");

      // if key and value are defined, look for config keys
      if ((pstr_key != nullptr) && (pstr_value != nullptr))
      {
        index = GetCommandCode (str_text, sizeof (str_text), pstr_key, kGazparConfigKey);
        if ((index >= 0) && (index < GAZPAR_CONFIG_MAX)) gazpar_config.param[index] = atol (pstr_value);
      }
    }
  }
# endif     // USE_UFILESYS

  // validate default values
  if (gazpar_config.param[GAZPAR_CONFIG_FACTOR] == 0)    gazpar_config.param[GAZPAR_CONFIG_FACTOR]    = GAZPAR_FACTOR_DEFAULT;
  if (gazpar_config.param[GAZPAR_CONFIG_KWH_HOUR] == 0)  gazpar_config.param[GAZPAR_CONFIG_KWH_HOUR]  = GAZPAR_GRAPH_DEF_KWH_HOUR;
  if (gazpar_config.param[GAZPAR_CONFIG_KWH_DAY] == 0)   gazpar_config.param[GAZPAR_CONFIG_KWH_DAY]   = GAZPAR_GRAPH_DEF_KWH_DAY;
  if (gazpar_config.param[GAZPAR_CONFIG_KWH_MONTH] == 0) gazpar_config.param[GAZPAR_CONFIG_KWH_MONTH] = GAZPAR_GRAPH_DEF_KWH_MONTH;

  // validate counters
  if (gazpar_config.param[GAZPAR_CONFIG_TODAY] > RtcSettings.pulse_counter[0]) gazpar_config.param[GAZPAR_CONFIG_TODAY] = RtcSettings.pulse_counter[0];
  if (gazpar_config.param[GAZPAR_CONFIG_YESTERDAY] > gazpar_config.param[GAZPAR_CONFIG_TODAY]) gazpar_config.param[GAZPAR_CONFIG_YESTERDAY] = gazpar_config.param[GAZPAR_CONFIG_TODAY];

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("GAZ: Config loaded from LittleFS"));
}

// Save configuration to Settings or to LittleFS
void GazparSaveConfig () 
{
  // save littlefs settings
#ifdef USE_UFILESYS
  uint8_t index;
  char    str_value[16];
  char    str_text[32];
  File    file;

  // open file and write content
  file = ffsp->open (D_GAZPAR_CFG, "w");
  for (index = 0; index < GAZPAR_CONFIG_MAX; index ++)
  {
    if (GetTextIndexed (str_text, sizeof (str_text), index, kGazparConfigKey) != nullptr)
    {
      // generate key=value
      strcat (str_text, "=");
      ltoa (gazpar_config.param[index], str_value, 10);
      strcat (str_text, str_value);
      strcat (str_text, "\n");

      // write to file
      file.print (str_text);
    }
  }
  file.close ();
# endif     // USE_UFILESYS
}

void GazparSetTotal (const long new_total)
{
  long delta;

  // check value
  if (new_total < 0) return;

  // calculate delta between current and new total
  delta = new_total - (long)RtcSettings.pulse_counter[0];

  // add delta to references
  gazpar_meter.last_hour += delta;
  gazpar_meter.last_start += delta;
  gazpar_config.param[GAZPAR_CONFIG_TODAY] += delta;
  gazpar_config.param[GAZPAR_CONFIG_YESTERDAY] += delta;

  // set new total
  gazpar_meter.last_count = (uint32_t)new_total;
  RtcSettings.pulse_counter[0] = gazpar_meter.last_count;

  // save new configuration
  GazparSaveConfig ();
}

// calculate number of days in given month
uint8_t GazparGetDaysInMonth (const uint32_t year, const uint8_t month)
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

// Convert from Wh to liter
long GazparConvertWh2Liter (const long wh)
{
  long liter = wh * 100 / gazpar_config.param[GAZPAR_CONFIG_FACTOR];

  return liter;
}

// Convert from liter to wh
long GazparConvertLiter2Wh (const long liter)
{
  long wh = liter * gazpar_config.param[GAZPAR_CONFIG_FACTOR] / 100;

  return wh;
}

// convert a delay in ms to a readable label
void GazparConvertDelay2Label (const uint32_t delay, char* pstr_result, const int size_result)
{
  uint32_t minute, second;

  // check parameters
  if (pstr_result == nullptr) return;
  if (size_result < 16) return;

  // convert delay to minute and seconds
  second = delay / 1000;
  minute = second / 60;
  second = second % 60;

  // generate label
  if (minute == 0) sprintf (pstr_result, "(%us)", second);
    else sprintf (pstr_result, "(%umn %us)", minute, second);
}

// Convert wh/liter kWh with kilo conversion (120, 2.0k, 12.0k, 2.3M, ...)
void GazparConvertWh2Label (const bool is_liter, const bool compact, const long value, char* pstr_result, const int size_result) 
{
  float result;
  char  str_sep[2];

  // check parameters
  if (pstr_result == nullptr) return;
  if (size_result < 12) return;

  // set separator
  if (compact) strcpy (str_sep, "");
    else strcpy (str_sep, " ");

  // convert liter counter to Wh
  if (is_liter) result = (float)GazparConvertLiter2Wh (value);
    else result = (float)value;

  // convert values in M with 1 digit
  if (result > 9999999)
  {
    result = result / 1000000;
    ext_snprintf_P (pstr_result, size_result, PSTR ("%1_f%sM"), &result, str_sep);
  }

   // convert values in M with 2 digits
  else if (result > 999999)
  {
    result = result / 1000000;
    ext_snprintf_P (pstr_result, size_result, PSTR ("%2_f%sM"), &result, str_sep);
  }

  // else convert values in k with no digit
  else if (result > 99999)
  {
    result = (float)result / 1000;
    ext_snprintf_P (pstr_result, size_result, PSTR ("%0_f%sk"), &result, str_sep);
  }

  // else convert values in k with one digit
  else if (result > 9999)
  {
    result = (float)result / 1000;
    ext_snprintf_P (pstr_result, size_result, PSTR ("%1_f%sk"), &result, str_sep);
  }

  // else convert values in k with two digits
  else if (result > 999)
  {
    result = (float)result / 1000;
    ext_snprintf_P (pstr_result, size_result, PSTR ("%1_f%sk"), &result, str_sep);
  }

  // else no conversion
  else
  {
    result = (float)result;
    ext_snprintf_P (pstr_result, size_result, PSTR ("%0_f%s"), &result, str_sep);
  }
}

// Get historisation filename
bool GazparGetFilename (const uint8_t histo, char* pstr_filename)
{
  bool exists = false;

  // if filename defined, check existence
  if (pstr_filename != nullptr)
  {
    sprintf_P (pstr_filename, D_GAZPAR_CSV, RtcTime.year - histo);
    if (strlen (pstr_filename) > 0) exists = ffsp->exists (pstr_filename);
  }

  return exists;
}

// Save historisation data
void GazparSaveDailyTotal ()
{
  uint8_t   index;
  uint32_t  current_time;
  TIME_T    today_dst;
  uint32_t  delta;
  char      str_value[32];
  String    str_line;
  File      file;

  // check time validity
  if (!RtcTime.valid) return;

  // calculate today's filename (shift 5 sec in case of sligth midnight call delay)
  current_time = LocalTime () - 5;
  BreakTime (current_time, today_dst);
  sprintf_P (gazpar_graph.str_filename, D_GAZPAR_CSV, 1970 + today_dst.year);

  // if file exists, open in append mode
  if (ffsp->exists (gazpar_graph.str_filename)) file = ffsp->open (gazpar_graph.str_filename, "a");

  // else open in creation mode
  else
  {
    file = ffsp->open (gazpar_graph.str_filename, "w");

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

  // generate today's line : day of year;month;day of month;total counter;delta since last line 
  sprintf (str_value, "%u;%u;%u;%u;%u", today_dst.day_of_year, today_dst.month, today_dst.day_of_month, RtcSettings.pulse_counter[0], RtcSettings.pulse_counter[0] - gazpar_meter.last_start);
  str_line = str_value;

  // update last hour total
  gazpar_meter.count_hour[RtcTime.hour] += (long)RtcSettings.pulse_counter[0] - gazpar_meter.last_hour;

  // loop to add hourly totals
  for (index = 0; index < 24; index ++)
  {
    // append hourly increment to line
    str_line += ";";
    str_line += gazpar_meter.count_hour[index];
  }

  // write line and close file
  str_line += "\n";
  file.print (str_line.c_str ());
  file.close ();

  // update counters
  gazpar_meter.last_start = gazpar_meter.last_hour = (long)RtcSettings.pulse_counter[0];
}

/**********************************\
 *            Callback
\**********************************/

// Gazpar module initialisation
void GazparModuleInit ()
{
  // disable wifi sleep mode
  Settings->flag5.wifi_no_sleep  = true;
  TasmotaGlobal.wifi_stay_asleep = false;
}

// Gazpar driver initialisation
void GazparInit ()
{
  uint8_t index;

  // init meter data
  gazpar_meter.last_stamp = millis ();
  gazpar_meter.last_count = RtcSettings.pulse_counter[0];                                 // init counter value
  gazpar_meter.last_start = gazpar_meter.last_hour = (long)gazpar_meter.last_count;       // init counter stamps
  for (index = 0; index < 24; index ++) gazpar_meter.count_hour[index] = 0;

  // load configuration
  GazparLoadConfig ();

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: gaz_help to get help on Gazpar specific commands"));
}

// Gazpar loop
void GazparLoop ()
{
  uint32_t time_now;

  // if counter has increased, calculate instant power
  if (gazpar_meter.last_count < RtcSettings.pulse_counter[0])
  {
    // update duration and timestamp
    time_now = millis ();
    gazpar_meter.last_length = (long)TimeDifference (gazpar_meter.last_stamp, time_now);
    gazpar_meter.last_stamp  = time_now;

    // cut length to 6mn max to get 500W minimum (small gaz cooker)
    if (gazpar_meter.last_length > GAZPAR_INCREMENT_MAXTIME) gazpar_meter.last_length = GAZPAR_INCREMENT_MAXTIME;

    // calculate equivalent power
    gazpar_meter.power = 360 * 1000 * gazpar_config.param[GAZPAR_CONFIG_FACTOR] / gazpar_meter.last_length;

    // set last count
    gazpar_meter.last_count = RtcSettings.pulse_counter[0];

    // publish data
    GazparShowJSON (false);
  }
}

// Called every second
void GazparEverySecond ()
{
  uint8_t  index;
  uint32_t time_now;

  if (!RtcTime.valid) return;

  // set current date stamp first time
  if (gazpar_meter.hour == UINT8_MAX) gazpar_meter.hour = RtcTime.hour;

  // if hour changed, save hourly increment
  if (gazpar_meter.hour != RtcTime.hour)
  {
    // calculate last hour increment and update last hour total
    gazpar_meter.count_hour[RtcTime.hour] += (long)RtcSettings.pulse_counter[0] - gazpar_meter.last_hour;
    gazpar_meter.last_hour = (long)RtcSettings.pulse_counter[0];
  }

  // if day changed, save daily data
  if (gazpar_meter.hour > RtcTime.hour)
  {
    // update today and yesterday
    gazpar_config.param[GAZPAR_CONFIG_YESTERDAY] = gazpar_config.param[GAZPAR_CONFIG_TODAY];
    gazpar_config.param[GAZPAR_CONFIG_TODAY] = (long)RtcSettings.pulse_counter[0];

    // save daily counters and daily totals
    GazparSaveSettings ();

    // reset hourly counters
    for (index = 0; index < 24; index ++) gazpar_meter.count_hour[index] = 0;
  }

  // if last increment duration + 30s reached, reset power
  if ((gazpar_meter.power > 0) && (TimeDifference (gazpar_meter.last_stamp, millis ()) > gazpar_meter.last_length + GAZPAR_INCREMENT_TIMEOUT)) gazpar_meter.power = 0;

  // update current hour
  gazpar_meter.hour = RtcTime.hour;
}

// Save configuration in case of restart
void GazparSaveSettings ()
{
  // save new values
  GazparSaveConfig ();

  // save daily totals
  GazparSaveDailyTotal ();
}

// Show JSON status (for MQTT)
void GazparShowJSON (bool append)
{
  long factor, total, today, ytday;

  // if not a telemetry call, add {"Time":"xxxxxxxx",
  if (!append) Response_P (PSTR ("{\"%s\":\"%s\""), D_JSON_TIME, GetDateAndTime (DT_LOCAL).c_str ());

  // Start Gazpar section "Gazpar":{
  ResponseAppend_P (PSTR (",\"gazpar\":{"));

  // factor
  factor = (long)gazpar_config.param[GAZPAR_CONFIG_FACTOR];
  ResponseAppend_P (PSTR ("\"factor\":%d.%02d"), factor / 100, factor % 100);

  // active power
  ResponseAppend_P (PSTR (",\"pact\":%d"), gazpar_meter.power);

  // m3
  total = (long)RtcSettings.pulse_counter[0];
  today = (long)RtcSettings.pulse_counter[0] - gazpar_config.param[GAZPAR_CONFIG_TODAY];
  ytday = gazpar_config.param[GAZPAR_CONFIG_TODAY] - gazpar_config.param[GAZPAR_CONFIG_YESTERDAY];
  ResponseAppend_P (PSTR (",\"m3\":{\"total\":%d.%02d,\"today\":%d.%02d,\"yesterday\":%d.%02d}"), total / 100, total % 100, today / 100, today % 100, ytday / 100, ytday % 100);

  // Wh
  total = GazparConvertLiter2Wh (total);
  today = GazparConvertLiter2Wh (today);
  ytday = GazparConvertLiter2Wh (ytday);
  ResponseAppend_P (PSTR (",\"wh\":{\"total\":%d,\"today\":%d,\"yesterday\":%d}"), total, today, ytday);

  // end of Gazpar section
  ResponseAppend_P (PSTR ("}"));

  // if not in telemetry, publish JSON and process rulesdd
  if (!append)
  {
    ResponseAppend_P (PSTR ("}"));
    MqttPublishTeleSensor ();
  }

#ifdef USE_TELEINFO_DOMOTICZ
  DomoticzIntegrationPublishTrigger ();
#endif    // USE_TELEINFO_DOMOTICZ

#ifdef USE_TELEINFO_HOMIE
  HomieIntegrationPublishData ();
#endif    // USE_TELEINFO_HOMIE
}

/**********************************\
 *               Web
\**********************************/

#ifdef USE_WEBSERVER

// Display gazpar icon
void GazparWebIconSvg () { Webserver->send_P (200, PSTR ("image/svg+xml"), gazpar_icon_svg, strlen (gazpar_icon_svg)); }

// Get specific argument as a value with min and max
long GazparWebGetArgValue (const char* pstr_argument, const long value_min, const long value_max, const long value_default)
{
  long result = value_default;
  char str_value[8];

  // check for argument
  if (Webserver->hasArg (pstr_argument))
  {
    WebGetArg (pstr_argument, str_value, sizeof (str_value));
    result = atol (str_value);
  }

  // check for min and max value
  if ((value_min >= 0) && (result <= value_min)) result = value_min;
  if ((value_max >= 0) && (result >= value_max)) result = value_max;

  return result;
}

// Append Gazpar graph button to main page
void GazparWebMainButton ()
{
  // Gazpar graph page button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_GAZPAR_PAGE_GRAPH, D_GAZPAR_GRAPH);
}

// Append Gazpar configuration button to configuration page
void GazparWebConfigButton ()
{
  // Gazpar configuration button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>Configure Gazpar</button></form></p>\n"), D_GAZPAR_PAGE_CONFIG);
}

// Gazpar configuration page
void GazparWebPageConfigure ()
{
  bool hass_available,  hass_enabled;
  bool homie_available, homie_enabled;
  char str_text[32];
  char str_select[16];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess ()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg ("save"))
  {
    // parameter 'total' : set conversion factor
    WebGetArg (D_CMND_GAZPAR_TOTAL, str_text, sizeof (str_text));
    if (strlen (str_text) > 0) GazparSetTotal ((long)(atof (str_text) * 100));

    // parameter 'factor' : set conversion factor
    WebGetArg (D_CMND_GAZPAR_FACTOR, str_text, sizeof (str_text));
    if (strlen (str_text) > 0) gazpar_config.param[GAZPAR_CONFIG_FACTOR] = (long)(atof (str_text) * 100);

    // parameter 'hass' : set home assistant integration
#ifdef USE_GAZPAR_HASS
      WebGetArg (D_CMND_GAZPAR_HASS, str_text, sizeof (str_text));
      HassIntegrationSet (strlen (str_text) > 0);
#endif    // USE_GAZPAR_HASS

    // parameter 'homie' : set homie integration
#ifdef USE_GAZPAR_HOMIE
      WebGetArg (D_CMND_GAZPAR_HOMIE, str_text, sizeof (str_text));
      HomieIntegrationSet (strlen (str_text) > 0);
#endif    // USE_GAZPAR_HOMIE

    // save configuration
    GazparSaveConfig ();
  }

  hass_available = false;
#ifdef USE_GAZPAR_HASS
  hass_available = true;
  hass_enabled   = HassIntegrationGet ();
#endif    // USE_GAZPAR_HASS

  homie_available = false;
#ifdef USE_GAZPAR_HOMIE
  homie_available = true;
  homie_enabled   = HomieIntegrationGet ();
#endif    // USE_GAZPAR_HOMIE

  // beginning of form
  WSContentStart_P (PSTR ("Configure Gazpar"));

  // page style
  WSContentSendStyle ();
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("span.key {float:right;font-size:0.7rem;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // page start
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_GAZPAR_PAGE_CONFIG);

  // teleinfo message diffusion type
  WSContentSend_P (PSTR ("<p><fieldset><legend><b>&nbsp;📊&nbsp;&nbsp;Paramètres&nbsp;</b></legend>\n"));
  WSContentSend_P (PSTR ("<p>Nouveau relevé (m³)<span class='key'>%s</span><br><input type='number' name='%s' step='0.01' value=''></p>\n"), D_CMND_GAZPAR_TOTAL, D_CMND_GAZPAR_TOTAL);
  WSContentSend_P (PSTR ("<p>Coefficient de conversion<span class='key'>%s</span><br><input type='number' name='%s' min='9' max='14' step='0.01' value='%d.%02d'></p>\n"), D_CMND_GAZPAR_FACTOR, D_CMND_GAZPAR_FACTOR, gazpar_config.param[GAZPAR_CONFIG_FACTOR] / 100, gazpar_config.param[GAZPAR_CONFIG_FACTOR] % 100);
  WSContentSend_P (PSTR ("</fieldset></p>\n"));

  // domotic integration
  if (hass_available || homie_available)
  {
    WSContentSend_P (PSTR ("<p><fieldset><legend><b>&nbsp;🏠&nbsp;&nbsp;Intégration&nbsp;</b></legend>\n"));
     if (hass_available)
    {
      if (hass_enabled) strcpy_P (str_select, PSTR ("checked")); else strcpy (str_select, "");
      WSContentSend_P (PSTR ("<p><input type='checkbox' id='%s' name='%s' %s><label for='%s'>Home Assistant</label></p>\n"), D_CMND_GAZPAR_HASS, D_CMND_GAZPAR_HASS, str_select, D_CMND_GAZPAR_HASS);
    }
    if (homie_available)
    {
      if (homie_enabled) strcpy_P (str_select, PSTR ("checked")); else strcpy (str_select, "");
      WSContentSend_P (PSTR ("<p><input type='checkbox' id='%s' name='%s' %s><label for='%s'>Homie</label></p>\n"), D_CMND_GAZPAR_HOMIE, D_CMND_GAZPAR_HOMIE, str_select, D_CMND_GAZPAR_HOMIE);
    }
    WSContentSend_P (PSTR ("</fieldset></p>\n"));
  }

  // save button
  WSContentSend_P (PSTR ("<br><button name='save' type='submit' class='button bgrn'>%s</button>"), D_SAVE);
  WSContentSend_P (PSTR ("</form>\n"));

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

// Append Teleinfo state to main page
void GazparWebSensor ()
{
  long     value;
  uint32_t delay;
  char str_unit[4];
  char str_value[8];
  char str_class[8];
  char str_delay[16];

  WSContentSend_P (PSTR ("<div style='font-size:13px;text-align:center;padding:4px 6px;margin-bottom:5px;background:#333333;border-radius:12px;'>\n"));

  // gazpar styles
  WSContentSend_P (PSTR ("<style>\n"));

  WSContentSend_P (PSTR (".meter span{padding:1px 4px;margin:2px;text-align:center;background:black;color:white;}\n"));
  WSContentSend_P (PSTR (".meter span.unit{border:solid 1px #888;}\n"));
  WSContentSend_P (PSTR (".meter span.cent{border:solid 1px #800;}\n"));
  WSContentSend_P (PSTR (".gaz span{padding-left:8px;font-size:9px;font-style:italic;}\n"));
  WSContentSend_P (PSTR (".white{color:white;}\n"));
  WSContentSend_P (PSTR (".grey{color:#666;}\n"));

  WSContentSend_P (PSTR ("table hr{display:none;}\n"));
  WSContentSend_P (PSTR (".gaz{display:flex;padding:2px 0px;}\n"));
  WSContentSend_P (PSTR (".gazh{width:12%%;padding:0px 2px;text-align:left;font-weight:bold;}\n"));
  WSContentSend_P (PSTR (".gazt{width:38%%;padding:0px;text-align:left;}\n"));
  WSContentSend_P (PSTR (".gazv{width:30%%;padding:0px;text-align:right;font-weight:bold;}\n"));
  WSContentSend_P (PSTR (".gazs{width:2%%;padding:0px;}\n"));
  WSContentSend_P (PSTR (".gazu{width:18%%;padding:0px;text-align:left;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // counter
  value = (long)RtcSettings.pulse_counter[0];
  WSContentSend_P (PSTR ("<div class='meter' style='width=100%%;text-align:center;margin:10px;font-size:18px;font-weight:bold;'>"));
  WSContentSend_P (PSTR ("&nbsp;&nbsp;"));
  WSContentSend_P (PSTR ("<span class='unit'>%d</span>"), value % 100000000 / 10000000 );
  WSContentSend_P (PSTR ("<span class='unit'>%d</span>"), value % 10000000 / 1000000 );
  WSContentSend_P (PSTR ("<span class='unit'>%d</span>"), value % 1000000 / 100000 );
  WSContentSend_P (PSTR ("<span class='unit'>%d</span>"), value % 100000 / 10000 );
  WSContentSend_P (PSTR ("<span class='unit'>%d</span>"), value % 10000 / 1000 );
  WSContentSend_P (PSTR ("<span class='unit'>%d</span>"), value % 1000 / 100 );
  WSContentSend_P (PSTR ("&nbsp;.&nbsp;"));
  WSContentSend_P (PSTR ("<span class='cent'>%d</span>"), value % 100 / 10 );
  WSContentSend_P (PSTR ("<span class='cent'>%d</span>"), value % 10);
  WSContentSend_P (PSTR ("&nbsp;m³"));
  WSContentSend_P (PSTR ("</div>\n"));

  // current power
    // if last increment duration + 30s reached, reset power
  delay = TimeDifference (gazpar_meter.last_stamp, millis ());
  if (gazpar_meter.power > 0)
  { 
    strcpy (str_unit, "W");
    ltoa (gazpar_meter.power, str_value, 10);
    GazparConvertDelay2Label (delay, str_delay, sizeof (str_delay));
  }
  else
  { 
    strcpy (str_unit, "");
    strcpy (str_value, "");
    strcpy (str_delay, "");
  }
  if (delay <= gazpar_meter.last_length) strcpy (str_class, "white");
    else strcpy (str_class, "grey");
  
  WSContentSend_P (PSTR ("<div class='gaz' ><div class='gazh'></div><div class='gazt'>Puissance<span class='%s'>%s</span></div>"), str_class, str_delay);
  WSContentSend_P (PSTR ("<div class='gazv %s'>%s</div>"), str_class, str_value);
  WSContentSend_P (PSTR ("<div class='gazs'></div><div class='gazu %s'>%s</div></div>\n"), str_class, str_unit);

  // today
  value = GazparConvertLiter2Wh ((long)RtcSettings.pulse_counter[0] - gazpar_config.param[GAZPAR_CONFIG_TODAY]);
  WSContentSend_P (PSTR ("<div class='gaz'><div class='gazh'></div><div class='gazt'>Aujourd'hui</div><div class='gazv'>%d</div><div class='gazs'></div><div class='gazu'>Wh</div></div>\n"), value);

  // yesterday
  value = GazparConvertLiter2Wh (gazpar_config.param[GAZPAR_CONFIG_TODAY] - gazpar_config.param[GAZPAR_CONFIG_YESTERDAY]);
  WSContentSend_P (PSTR ("<div class='gaz'><div class='gazh'></div><div class='gazt'>Hier</div><div class='gazv'>%d</div><div class='gazs'></div><div class='gazu'>Wh</div></div>\n"), value);

  WSContentSend_P (PSTR ("<hr>\n"));

  // total
  value = GazparConvertLiter2Wh ((long)RtcSettings.pulse_counter[0]);
  WSContentSend_P (PSTR ("<div class='gaz'><div class='gazh'></div><div class='gazt'>Total</div><div class='gazv'>%d.%03d</div><div class='gazs'></div><div class='gazu'>kWh</div></div>\n"), value / 1000, value % 1000);

  // conversion factor
  value = gazpar_config.param[GAZPAR_CONFIG_FACTOR];
  WSContentSend_P (PSTR ("<div class='gaz'><div class='gazh'></div><div class='gazt'>Coefficient</div><div class='gazv'>%d.%02d</div><div class='gazs'></div><div class='gazu'>kWh/m³</div></div>\n"), value / 100, value % 100);

  WSContentSend_P (PSTR ("</div>\n"));
}

// calculate previous period parameters
bool GazparGraphPreviousPeriod (const bool update)
{
  bool     is_file = false;
  uint8_t  histo = gazpar_graph.histo;
  uint8_t  month = gazpar_graph.month;
  uint8_t  day   = gazpar_graph.day;
  uint32_t year;
  TIME_T   time_dst;

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
      day = GazparGetDaysInMonth (year, month);
    }
  }

  // check if yearly file is available
  is_file = GazparGetFilename (histo, gazpar_graph.str_filename);

  // if asked, set next period
  if (is_file && update)
  {
    gazpar_graph.histo = histo;
    gazpar_graph.month = month;
    gazpar_graph.day   = day;
  }

  return is_file;
}

bool GazparGraphNextPeriod (const bool update)
{
  bool     is_file = false;
  bool     is_next = false;
  uint8_t  nb_day;
  uint8_t  histo = gazpar_graph.histo;
  uint8_t  month = gazpar_graph.month;
  uint8_t  day   = gazpar_graph.day;
  uint32_t year;
  TIME_T   time_dst;

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
    nb_day = GazparGetDaysInMonth (year, month);

    // next day
    day++;
    if (day > nb_day) { day = 1; month++; }
    if (month == 13) { month = 1; is_next = true; }
  }

  // if yearly file available
  if ((histo == 0) && is_next) histo = UINT8_MAX;
    else if (is_next) histo--;

  // check if yearly file is available
  is_file = GazparGetFilename (histo, gazpar_graph.str_filename);

  // if asked, set next period
  if (is_file && update)
  {
    gazpar_graph.histo = histo;
    gazpar_graph.month = month;
    gazpar_graph.day   = day;
  }

  return is_file;
}

// Display bar graph
void GazparWebDisplayBarGraph (const uint8_t histo, const bool current)
{
  bool     analyse;
  uint8_t  index, month, day, range;
  uint32_t len_buffer;
  uint32_t time_bar;
  long     value, value_x, value_y;
  long     graph_x, graph_y, graph_delta, graph_width, graph_x_end, graph_max;    
  long     arr_value[GAZPAR_GRAPH_MAX_BARGRAPH];
  TIME_T   time_dst;
  char     str_type[8];
  char     str_value[16];
  char     str_buffer[192];
  char    *pstr_token;
  File     file;

  // init array
  for (index = 0; index < GAZPAR_GRAPH_MAX_BARGRAPH; index ++) arr_value[index] = 0;

  // if full month view, calculate first day of month
  if (gazpar_graph.month != 0)
  {
    BreakTime (LocalTime (), time_dst);
    time_dst.year -= histo;
    time_dst.month = gazpar_graph.month;
    time_dst.day_of_week = 0;
    time_dst.day_of_year = 0;
  }

  // init graph units for full year display (month bars)
  if (gazpar_graph.month == 0)
  {
    range       = 12;             // number of graph bars (months per year)
    graph_width = 90;             // width of graph bar area
    graph_delta = 20;             // separation between bars (pixels)
    graph_max   = GazparConvertWh2Liter (1000 * gazpar_config.param[GAZPAR_CONFIG_KWH_MONTH]);
    strcpy (str_type, "month");
  }

  // else init graph units for full month display (day bars)
  else if (gazpar_graph.day == 0)
  {
    range       = 31;             // number of graph bars (days per month)
    graph_width = 35;             // width of graph bar area
    graph_delta = 4;              // separation between bars (pixels)
    graph_max   = GazparConvertWh2Liter (1000 * gazpar_config.param[GAZPAR_CONFIG_KWH_DAY]);
    strcpy (str_type, "day");
  }

  // else init graph units for full day display (hour bars)
  else
  {
    range       = 24;             // number of graph bars (hours per day)
    graph_width = 45;             // width of graph bar area
    graph_delta = 10;             // separation between bars (pixels)
    graph_max   = GazparConvertWh2Liter (1000 * gazpar_config.param[GAZPAR_CONFIG_KWH_HOUR]);
    strcpy (str_type, "hour");
  }

  // if current day, collect live values
  if ((histo == 0) && (gazpar_graph.month == RtcTime.month) && (gazpar_graph.day == RtcTime.day_of_month))
  {
    // update current hour increment
    gazpar_meter.count_hour[RtcTime.hour] += (long)gazpar_meter.last_count - gazpar_meter.last_hour;
    gazpar_meter.last_hour = gazpar_meter.last_count;

    // init hour slots from live values
    for (index = 0; index < 24; index ++) if (gazpar_meter.count_hour[index] > 0) arr_value[index] = gazpar_meter.count_hour[index];
  }

  // calculate graph height and graph start
  graph_x     = gazpar_graph.left + graph_delta / 2;
  graph_x_end = graph_x + graph_width - graph_delta;
  if (!current) { graph_x += 6; graph_x_end -= 6; }

  // if data file exists
  if (GazparGetFilename (histo, gazpar_graph.str_filename))
  {
    // open file
    file = ffsp->open (gazpar_graph.str_filename, "r");
    while (file.available ())
    {
      // init
      index = 0;
      month = day = UINT8_MAX;
      value = LONG_MAX;
      pstr_token = nullptr;

      // read next line
      len_buffer = file.readBytesUntil ('\n', str_buffer, sizeof (str_buffer));
      str_buffer[len_buffer] = 0;

      // if first caracter is not numerical, ignore line (header, empty line, ...)
      if (isdigit (str_buffer[0])) pstr_token = strtok (str_buffer, ";");
      while (pstr_token != nullptr)
      {
        // if token is defined
        if (strlen (pstr_token) > 0)
        {
          switch (index)
          {
            case 0:
              break;
            case 1:
              month = atoi (pstr_token);
              break;
            case 2:
              day = atoi (pstr_token);
              break;
            case 3:
              break;
            default:
              value = atol (pstr_token);
              break;
          }
        }

        // if year graph
        if (gazpar_graph.month == 0)
        {
          // if index is on daily total and value is valid, add value to month of year
          if ((index == 4) && (month <= 12) && (value != LONG_MAX)) arr_value[month] += value;
        }

        // else if displaying all days of specific month
        else if ((gazpar_graph.month == month) && (gazpar_graph.day == 0))
        {
          // if index is on daily total and value is valid, add value to day of month
          if ((index == 4) && (day <= 31) && (value != LONG_MAX)) arr_value[day] += value;
        }

        // else if displaying all hours of specific day / month
        else if ((gazpar_graph.month == month) && (gazpar_graph.day == day))
        {
          // if dealing with hourly index, add value to hourly slot
          if ((index > 4) && (index <= 28) && (value != LONG_MAX)) arr_value[index - 5] += value;
        }

        // next token in the line
        index++;
        pstr_token = strtok (nullptr, ";");
      }
    }

    // close file
    file.close ();
  }

  // loop to display bar graphs
  for (index = 1; index <= range; index ++)
  {
    // give control back to system to avoid watchdog
    yield ();
    
    // if value is defined, display bar and value
    if ((arr_value[index] != LONG_MAX) && (arr_value[index] > 0))
    {
      // bar graph
      // ---------

      // bar y position
      graph_y = GAZPAR_GRAPH_HEIGHT - (arr_value[index] * GAZPAR_GRAPH_HEIGHT / graph_max);
      if (graph_y < 0) graph_y = 0;

      // display link
      if (current && (gazpar_graph.month == 0)) WSContentSend_P (PSTR("<a href='%s?month=%d&day=0'>"), D_GAZPAR_PAGE_GRAPH, index);
      else if (current && (gazpar_graph.day == 0)) WSContentSend_P (PSTR("<a href='%s?day=%d'>"), D_GAZPAR_PAGE_GRAPH, index);

      // display bar
      if (current) strcpy (str_value, "now"); else strcpy (str_value, "prev");
      WSContentSend_P (PSTR("<path class='%s' d='M%d %d L%d %d L%d %d L%d %d L%d %d L%d %d Z'></path>"), str_value, graph_x, GAZPAR_GRAPH_HEIGHT, graph_x, graph_y + 2, graph_x + 2, graph_y, graph_x_end - 2, graph_y, graph_x_end, graph_y + 2, graph_x_end, GAZPAR_GRAPH_HEIGHT);

      // end of link 
      if (current && ((gazpar_graph.month == 0) || (gazpar_graph.day == 0))) WSContentSend_P (PSTR("</a>\n"));
        else WSContentSend_P (PSTR("\n"));

      // bar values
      // -----------
     if (current)
      {
        // top of bar value
        // ----------------

        // calculate bar graph value position
        value_x = (graph_x + graph_x_end) / 2;
        value_y = graph_y - 15;
        if (value_y < 15) value_y = 15;
        if (value_y > GAZPAR_GRAPH_HEIGHT - 50) value_y = GAZPAR_GRAPH_HEIGHT - 50;

        // display value
        GazparConvertWh2Label (true, true, arr_value[index], str_value, sizeof (str_value));
        WSContentSend_P (PSTR("<text class='%s value' x='%d' y='%d'>%s</text>\n"), str_type, value_x, value_y, str_value);

        // month name or day / hour number
        // -------------------------------

        // if full year, get month name else get day of month
        if (gazpar_graph.month == 0) GetTextIndexed (str_value, sizeof (str_value), index, kGazparYearMonth);
          else if (gazpar_graph.day == 0) sprintf (str_value, "%02d", index);
            else sprintf (str_value, "%dh", index - 1);

        // display
        value_y = GAZPAR_GRAPH_HEIGHT - 10;
        WSContentSend_P (PSTR("<text class='%s' x='%d' y='%d'>%s</text>\n"), str_type, value_x, value_y, str_value);

        // week day name
        // -------------

        if ((gazpar_graph.month != 0) && (gazpar_graph.day == 0))
        {
          // calculate day name
          time_dst.day_of_month = index;
          time_bar = MakeTime (time_dst);
          BreakTime (time_bar, time_dst);
          day = (time_dst.day_of_week + 5) % 7;
          GetTextIndexed (str_value, sizeof (str_value), day, kGazparWeekDay2);

          // display
          value_y = GAZPAR_GRAPH_HEIGHT - 30;
          WSContentSend_P (PSTR("<text class='%s' x='%d' y='%d'>%s</text>\n"), str_type, value_x, value_y, str_value);
        }
      }
    }

    // increment bar position
    graph_x     += graph_width;
    graph_x_end += graph_width;
  }
}

// Graph frame
void GazparWebGraphFrame ()
{
  long unit_max;
  char arr_label[5][12];

  // get maximum in Wh
  if (gazpar_graph.month == 0) unit_max = gazpar_config.param[GAZPAR_CONFIG_KWH_MONTH];
    else if (gazpar_graph.day == 0) unit_max = gazpar_config.param[GAZPAR_CONFIG_KWH_DAY];
      else unit_max = gazpar_config.param[GAZPAR_CONFIG_KWH_HOUR];
  unit_max *= 1000;

  // generate scale values
  itoa (0, arr_label[0], 10);
  GazparConvertWh2Label (false, true, unit_max / 4,     arr_label[1], sizeof (arr_label[1]));
  GazparConvertWh2Label (false, true, unit_max / 2,     arr_label[2], sizeof (arr_label[2]));
  GazparConvertWh2Label (false, true, unit_max * 3 / 4, arr_label[3], sizeof (arr_label[3]));
  GazparConvertWh2Label (false, true, unit_max,         arr_label[4], sizeof (arr_label[4]));

  // graph frame
  WSContentSend_P (PSTR ("<rect x='%d%%' y='%d%%' width='%d%%' height='99.9%%' rx='10' />\n"), GAZPAR_GRAPH_PERCENT_START, 0, GAZPAR_GRAPH_PERCENT_STOP - GAZPAR_GRAPH_PERCENT_START);

  // graph separation lines
  WSContentSend_P (PSTR ("<line class='dash' x1='%d%%' y1='%d%%' x2='%d%%' y2='%d%%' />\n"), GAZPAR_GRAPH_PERCENT_START, 25, GAZPAR_GRAPH_PERCENT_STOP, 25);
  WSContentSend_P (PSTR ("<line class='dash' x1='%d%%' y1='%d%%' x2='%d%%' y2='%d%%' />\n"), GAZPAR_GRAPH_PERCENT_START, 50, GAZPAR_GRAPH_PERCENT_STOP, 50);
  WSContentSend_P (PSTR ("<line class='dash' x1='%d%%' y1='%d%%' x2='%d%%' y2='%d%%' />\n"), GAZPAR_GRAPH_PERCENT_START, 75, GAZPAR_GRAPH_PERCENT_STOP, 75);

  // units graduation
  WSContentSend_P (PSTR ("<text class='power' x='%d%%' y='%d%%'>%s</text>\n"), GAZPAR_GRAPH_PERCENT_STOP + 3, 2, "Wh");
  WSContentSend_P (PSTR ("<text class='power' x='%d%%' y='%d%%'>%s</text>\n"), 2, 2,  arr_label[4]);
  WSContentSend_P (PSTR ("<text class='power' x='%d%%' y='%d%%'>%s</text>\n"), 2, 26, arr_label[3]);
  WSContentSend_P (PSTR ("<text class='power' x='%d%%' y='%d%%'>%s</text>\n"), 2, 51, arr_label[2]);
  WSContentSend_P (PSTR ("<text class='power' x='%d%%' y='%d%%'>%s</text>\n"), 2, 76, arr_label[1]);
  WSContentSend_P (PSTR ("<text class='power' x='%d%%' y='%d%%'>%s</text>\n"), 2, 98, arr_label[0]);
}

// Graph public page
void GazparWebGraphPage ()
{
  int      index, counter;  
  long     value_inc, value_dec;
  uint8_t  choice, nb_day;
  uint16_t year;
  char     str_text[16];
  char     str_date[16];
  uint32_t start_time = 0;
  TIME_T   start_dst;

  // get numerical argument values
  gazpar_graph.histo = GazparWebGetArgValue (D_CMND_GAZPAR_HISTO, 0, 100, gazpar_graph.histo);
  gazpar_graph.month = GazparWebGetArgValue (D_CMND_GAZPAR_MONTH, 0, 12,  gazpar_graph.month);
  gazpar_graph.day   = GazparWebGetArgValue (D_CMND_GAZPAR_DAY,   0, 31,  gazpar_graph.day);

  // set graph reference date
  BreakTime (LocalTime (), start_dst);
  if (gazpar_graph.histo > 0) start_dst.year -= gazpar_graph.histo; 
  if (gazpar_graph.month > 0) start_dst.month = gazpar_graph.month; 
  if (gazpar_graph.day > 0) start_dst.day_of_month = gazpar_graph.day;

  // handle previous page
  choice = GazparWebGetArgValue (D_CMND_GAZPAR_PREV, 0, 1, 0);
  if (choice == 1) GazparGraphPreviousPeriod (true);

  // handle next page
  choice = GazparWebGetArgValue (D_CMND_GAZPAR_NEXT, 0, 1, 0);
  if (choice == 1) GazparGraphNextPeriod (true);

  // graph increment
  choice = GazparWebGetArgValue (D_CMND_GAZPAR_PLUS, 0, 1, 0);
  if (choice == 1)
  {
    if (gazpar_graph.month == 0) gazpar_config.param[GAZPAR_CONFIG_KWH_MONTH] *= 2;
    else if (gazpar_graph.day == 0) gazpar_config.param[GAZPAR_CONFIG_KWH_DAY] *= 2;
    else gazpar_config.param[GAZPAR_CONFIG_KWH_HOUR] *= 2;
  }

  // graph decrement
  choice = GazparWebGetArgValue (D_CMND_GAZPAR_MINUS, 0, 1, 0);
  if (choice == 1)
  {
    if (gazpar_graph.month == 0) gazpar_config.param[GAZPAR_CONFIG_KWH_MONTH] = max (1L, gazpar_config.param[GAZPAR_CONFIG_KWH_MONTH] / 2);
    else if (gazpar_graph.day == 0) gazpar_config.param[GAZPAR_CONFIG_KWH_DAY] = max (1L , gazpar_config.param[GAZPAR_CONFIG_KWH_DAY] / 2);
    else gazpar_config.param[GAZPAR_CONFIG_KWH_HOUR] = max (1L, gazpar_config.param[GAZPAR_CONFIG_KWH_HOUR] / 2);
  }

  // beginning of form without authentification
  WSContentStart_P (D_GAZPAR_GRAPH, false);
  WSContentSend_P (PSTR ("</script>\n"));

  // set page as scalable
  WSContentSend_P (PSTR ("<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=yes'/>\n"));

  // set cache policy to 12 hours
  WSContentSend_P (PSTR ("<meta http-equiv='Cache-control' content='public,max-age=720000'>\n"));

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {margin:0.25rem auto;padding:0.1rem 0px;text-align:center;vertical-align:middle;}\n"));
  WSContentSend_P (PSTR ("div a {color:white;}\n"));

  WSContentSend_P (PSTR ("div.live {height:32px;}\n"));
  WSContentSend_P (PSTR ("div img {height:96px;}\n"));

  WSContentSend_P (PSTR ("div.prev {position:absolute;top:16px;left:2%%;padding:0px;}\n"));
  WSContentSend_P (PSTR ("div.next {position:absolute;top:16px;right:2%%;padding:0px;}\n"));
  WSContentSend_P (PSTR ("div.range {position:absolute;top:128px;left:2%;padding:0px;}\n"));

  WSContentSend_P (PSTR ("div.choice {display:inline-block;padding:0px 2px;margin:0px;border:1px #666 solid;background:none;color:#fff;border-radius:6px;}\n"));
  WSContentSend_P (PSTR ("div.choice div {background:#666;}\n"));
  WSContentSend_P (PSTR ("div.choice a div {background:none;}\n"));

  WSContentSend_P (PSTR ("div.item {display:inline-block;margin:1px 0px;border-radius:4px;}\n"));
  WSContentSend_P (PSTR ("div a div.item:hover {background:#aaa;}\n"));

  WSContentSend_P (PSTR ("div.incr {position:absolute;top:5vh;left:2%%;padding:0px;color:white;border:1px #666 solid;border-radius:6px;}\n"));
  
  WSContentSend_P (PSTR ("div.year {width:44px;font-size:0.9rem;}\n"));
  WSContentSend_P (PSTR ("div.month {width:28px;font-size:0.85rem;}\n"));
  WSContentSend_P (PSTR ("div.day {width:16px;font-size:0.8rem;margin-top:2px;}\n"));
  
  WSContentSend_P (PSTR ("div.data {width:50px;}\n"));
  WSContentSend_P (PSTR ("div.size {width:25px;}\n"));
  WSContentSend_P (PSTR ("div a div.active {background:#666;}\n"));

  WSContentSend_P (PSTR ("button {padding:2px;font-size:1rem;background:#444;color:#fff;border:none;border-radius:6px;}\n"));
  WSContentSend_P (PSTR ("button.navi {padding:2px 16px;}\n"));
  WSContentSend_P (PSTR ("button.range {display:block;width:24px;margin-bottom:8px;}\n"));
  WSContentSend_P (PSTR ("button:disabled {background:#252525;color:#252525;}\n"));

  WSContentSend_P (PSTR ("button.menu {background:%s;color:%s;line-height:2rem;font-size:1.2rem;cursor:pointer;border:none;border-radius:0.3rem;width:150px;margin:2vh;}\n"), COLOR_BUTTON, COLOR_BUTTON_TEXT);

  WSContentSend_P (PSTR ("div.graph {width:100%%;margin:auto;margin-top:2vh;}\n"));
  WSContentSend_P (PSTR ("svg.graph {width:100%%;height:60vh;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // page body
  WSContentSend_P (PSTR ("</head>\n"));
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_GAZPAR_PAGE_GRAPH);

  // device name
  WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText(SET_DEVICENAME));

  // display icon
  WSContentSend_P (PSTR ("<div><img src='%s'></div>\n"), D_GAZPAR_ICON_SVG);

  // previous button
  if (GazparGraphPreviousPeriod (false)) strcpy (str_text, "");
    else strcpy (str_text, "disabled");
  WSContentSend_P(PSTR ("<div class='prev'><button class='navi' name='%s' id='%s' value=1 %s>&lt;&lt;</button></div>\n"), D_CMND_GAZPAR_PREV, D_CMND_GAZPAR_PREV, str_text);

  // next button
  if (GazparGraphNextPeriod (false)) strcpy (str_text, "");
    else strcpy (str_text, "disabled");
  WSContentSend_P(PSTR ("<div class='next'><button class='navi' name='%s' id='%s' value=1 %s>&gt;&gt;</button></div>\n"), D_CMND_GAZPAR_NEXT, D_CMND_GAZPAR_NEXT, str_text);

  // graph scale buttons (+ / -)
  WSContentSend_P (PSTR ("<div class='range'>"));
  WSContentSend_P (PSTR ("<button class='range' name='%s' id='%s' value=1>+</button><br>\n"), D_CMND_GAZPAR_PLUS, D_CMND_GAZPAR_PLUS);
  WSContentSend_P (PSTR ("<button class='range' name='%s' id='%s' value=1>-</button><br>\n"), D_CMND_GAZPAR_MINUS, D_CMND_GAZPAR_MINUS);
  WSContentSend_P (PSTR ("</div>\n"));      // range

  // -------------------
  //   Level 1 : Years
  // -------------------

  WSContentSend_P (PSTR ("<div><div class='choice'>\n"));

  for (counter = 9; counter >= 0; counter--)
  {
    // check if file exists
    if (GazparGetFilename (counter, gazpar_graph.str_filename))
    {
      // detect active year
      strcpy (str_text, "");
      if (gazpar_graph.histo == counter) strcpy (str_text, " active");

      // display year selection
      WSContentSend_P (PSTR ("<a href='%s?histo=%u&month=0&day=0'><div class='item year%s'>%u</div></a>\n"), D_GAZPAR_PAGE_GRAPH, counter, str_text, RtcTime.year - counter);       
    }
  }

  WSContentSend_P (PSTR ("</div></div>\n"));        // choice

  // --------------------
  //   Level 2 : Months
  // --------------------

  WSContentSend_P (PSTR ("<div><div class='choice'>\n"));

  for (counter = 1; counter <= 12; counter++)
  {
    // get month name
    GetTextIndexed (str_date, sizeof (str_date), counter, kGazparYearMonth);

    // handle selected month
    strcpy (str_text, "");
    index = counter;
    if (gazpar_graph.month == counter)
    {
      strcpy_P (str_text, PSTR (" active"));
      if ((gazpar_graph.month != 0) && (gazpar_graph.day == 0)) index = 0;
    }

    // display month selection
    WSContentSend_P (PSTR ("<a href='%s?month=%u&day=0'><div class='item month%s'>%s</div></a>\n"), D_GAZPAR_PAGE_GRAPH, index, str_text, str_date);       
  }

  WSContentSend_P (PSTR ("</div></div>\n"));      // choice

  // --------------------
  //   Level 3 : Days
  // --------------------

  if (gazpar_graph.month != 0)
  {
    // calculate current year
    year = RtcTime.year - gazpar_graph.histo;
    nb_day = GazparGetDaysInMonth (year, gazpar_graph.month);

    WSContentSend_P (PSTR ("<div class='live'>\n"));
    WSContentSend_P (PSTR ("<div class='choice'>\n"));

    // loop thru days in the month
    for (counter = 1; counter <= nb_day; counter++)
    {
      // handle selected day
      strcpy (str_text, "");
      index = counter;
      if (gazpar_graph.day == counter) strcpy_P (str_text, PSTR (" active"));
      if ((gazpar_graph.day == counter) && (gazpar_graph.day != 0)) index = 0;

      // display day selection
      WSContentSend_P (PSTR ("<a href='%s?day=%u'><div class='item day%s'>%u</div></a>\n"), D_GAZPAR_PAGE_GRAPH, index, str_text, counter);
    }

    WSContentSend_P (PSTR ("</div>\n"));        // choice
    WSContentSend_P (PSTR ("</div>\n"));        // live
  }

  WSContentSend_P (PSTR ("</form>\n"));         // form

  // ---------------
  //   SVG : Start 
  // ---------------

  WSContentSend_P (PSTR ("<div class='graph'>\n"));
  WSContentSend_P (PSTR ("<svg class='graph' viewBox='0 0 1200 %d' preserveAspectRatio='none'>\n"), GAZPAR_GRAPH_HEIGHT);

  // ---------------
  //   SVG : Style 
  // ---------------

  WSContentSend_P (PSTR ("<style type='text/css'>\n"));

  WSContentSend_P (PSTR ("rect {stroke:grey;fill:none;}\n"));
  WSContentSend_P (PSTR ("line.dash {stroke:grey;stroke-width:1;stroke-dasharray:2 8;}\n"));
  WSContentSend_P (PSTR ("text.power {font-size:1.2rem;fill:white;}\n"));

  // bar graph
  WSContentSend_P (PSTR ("path {stroke-width:1.5;opacity:0.8;fill-opacity:0.6;}\n"));
  WSContentSend_P (PSTR ("path.now {stroke:#6cf;fill:#6cf;}\n"));
  if (gazpar_graph.day == 0) WSContentSend_P (PSTR ("path.now:hover {fill-opacity:0.8;}\n"));
  WSContentSend_P (PSTR ("path.prev {stroke:#069;fill:#069;fill-opacity:1;}\n"));

  // text
  WSContentSend_P (PSTR ("text {fill:white;text-anchor:middle;dominant-baseline:middle;}\n"));
  WSContentSend_P (PSTR ("text.value {font-style:italic;}}\n"));
  WSContentSend_P (PSTR ("text.month {font-size:1.2rem;}\n"));
  WSContentSend_P (PSTR ("text.day {font-size:0.9rem;}\n"));
  WSContentSend_P (PSTR ("text.hour {font-size:1rem;}\n"));

  // time line
  WSContentSend_P (PSTR ("line.time {stroke-width:1;stroke:white;stroke-dasharray:1 8;}\n"));

  WSContentSend_P (PSTR ("</style>\n"));

  // ---------------
  //   SVG : Data 
  // ---------------

  GazparWebGraphFrame ();
  GazparWebDisplayBarGraph (gazpar_graph.histo + 1, false);       // previous period
  GazparWebDisplayBarGraph (gazpar_graph.histo,     true);        // current period
  WSContentSend_P (PSTR ("</svg>\n</div>\n"));

  // main menu button
  WSContentSend_P (PSTR ("<div><form action='/' method='get'><button class='menu'>%s</button></form></div>\n"), D_MAIN_MENU);

  // end of page
  WSContentStop ();
}

#endif  // USE_WEBSERVER

/***********************************\
 *            Interface
\***********************************/

// Teleinfo sensor
bool Xdrv96 (uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function) 
  {
    case FUNC_MODULE_INIT:
      GazparModuleInit ();
      break;
    case FUNC_INIT:
      GazparInit ();
      break;
    case FUNC_LOOP:
      GazparLoop ();
      break;
    case FUNC_SAVE_BEFORE_RESTART:
      GazparSaveSettings ();
      break;
    case FUNC_EVERY_SECOND:
      GazparEverySecond ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kGazparCommands, GazparCommand);
      break;
    case FUNC_JSON_APPEND:
      GazparShowJSON (true);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_BUTTON:
      GazparWebConfigButton ();
      break;
    case FUNC_WEB_ADD_HANDLER:
      Webserver->on (FPSTR (D_GAZPAR_PAGE_CONFIG), GazparWebPageConfigure);
      Webserver->on (FPSTR (D_GAZPAR_PAGE_GRAPH), GazparWebGraphPage);
      Webserver->on (FPSTR (D_GAZPAR_ICON_SVG), GazparWebIconSvg);
      break;

    case FUNC_WEB_ADD_MAIN_BUTTON:
      GazparWebMainButton ();
      break;
   case FUNC_WEB_SENSOR:
      GazparWebSensor ();
      break;
#endif  // USE_WEBSERVER
  }

  return result;
}

#endif      // USE_UFILESYS
#endif      // USE_GAZPAR
