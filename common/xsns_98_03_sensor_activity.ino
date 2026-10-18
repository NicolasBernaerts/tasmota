/*
  xdrv_98_03_sensor_activity.ino - Graph for activity detection

  Copyright (C) 2019-2025  Nicolas Bernaerts

  Version history :
    14/05/2025 v1.0 - Split from generic sensor
                      Add yearly history
    21/11/2025 v1.1 - Tasmota 15 port

  RAM : 497 bytes
  
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

#ifdef USE_SENSOR_ACTIVITY

/*************************************************\
 *               Variables
\*************************************************/

#define SENSOR_ACTIVITY_VERSION            1           // saved data version

// graph dimensions
//#define SENSOR_ACTIVITY_LEFT               60          // left position of the curve
//#define SENSOR_ACTIVITY_WIDE               1200        // graph usable width
//#define SENSOR_ACTIVITY_RIGHT              1260        // right position of the curve
#define SENSOR_ACTIVITY_WIDTH              1320        // graph global width
#define SENSOR_ACTIVITY_WIDTH_HEAD         100         // header width
#define SENSOR_ACTIVITY_WIDTH_LINE         1100        // bar full width
#define SENSOR_ACTIVITY_HEIGHT             600         // default graph height
#define SENSOR_ACTIVITY_MAIN_RADIUS        8           // radius of main background
#define SENSOR_ACTIVITY_SLOT_RADIUS        6           // radius of slot bar

#define SENSOR_ACTIVITY_COLOR_SIZE         8           // max size of color
#define SENSOR_ACTIVITY_TITLE_SIZE         32          // max size of title

#define SENSOR_ACTIVITY_TITLE              "Activity"
#define SENSOR_ACTIVITY_COLOR              "#080"

// web URL
const char PSTR_SENSOR_ACTIVITY_PAGE[] PROGMEM = "/activity";

// historisation file
const char PSTR_SENSOR_ACTIVITY_DATA[]  PROGMEM = "/sensor-activity.dat";

// data publication commands
static const char kSensorActivityCommands[]  PROGMEM = "activity|"   "reset";
void (* const SensorActivityCommand[])(void) PROGMEM = {  &CmndSensorActivityResetData };

// graph periods
enum ActivityGraphPeriod                  { SENSOR_ACTIVITY_PERIOD_WEEK, SENSOR_ACTIVITY_PERIOD_YEAR, SENSOR_ACTIVITY_PERIOD_MAX };
const char kActivityGraphPeriod[] PROGMEM =          "week"          "|"         "year";

// week days name
static const char kActivityWeekdayNames[] = D_DAY3LIST;

/****************************************\
 *                 Data
\****************************************/

struct {                            // RAM : 457 bytes
  uint8_t period = SENSOR_ACTIVITY_PERIOD_WEEK;               // graph period
  uint8_t arr_week[7][12];                                    // week : one slot every 15mn (8 within 2h)
  uint8_t arr_year[12][31];                                   // year : one slot every 3h (8 within 24h)
} activity_data;

struct {                            // RAM : 42 bytes
  bool    managed = false;                                    // activity managed by external module
  uint8_t hour    = UINT8_MAX;                                // current hour
  char    str_color[SENSOR_ACTIVITY_COLOR_SIZE] = { 0 };      // display color
  char    str_title[SENSOR_ACTIVITY_TITLE_SIZE] = { 0 };      // display title
} activity_status;

/*********************************************\
 *                 Commands
\*********************************************/

void CmndSensorActivityResetData ()
{
  // init data
  memset (&activity_data, 0, sizeof (activity_data));

  ResponseCmndDone ();
}

/*********************************************\
 *                 Functions
\*********************************************/

void SensorActivitySetManaged ()
{
  // set managed mode
  activity_status.managed = true;
}

void SensorActivitySetTitle (const char *pstr_title)
{
  // check parameter
  if (pstr_title == nullptr) return;
  if (strlen_P (pstr_title) >= SENSOR_ACTIVITY_TITLE_SIZE) return;

  // set title
  strcpy_P (activity_status.str_title, pstr_title);
}

void SensorActivitySetColor (const char *pstr_color)
{
  // check parameter
  if (pstr_color == nullptr) return;
  if (strlen_P (pstr_color) >= SENSOR_ACTIVITY_COLOR_SIZE) return;

  // set color
  strcpy_P (activity_status.str_color, pstr_color);
}

void SensorActivityTrigger ()
{
  // ignore if time is not valid
  if (!RtcTime.valid) return;
  
  // update weekly slot
  SensorActivitySetWeekData (RtcTime.day_of_week, RtcTime.hour, RtcTime.minute);

  // update yearly slot
  SensorActivitySetYearData (RtcTime.month, RtcTime.day_of_month, RtcTime.hour);
}

// load data file
void SensorActivityLoadData ()
{
  // init data
  memset (&activity_data, 0, sizeof (activity_data));

#ifdef USE_UFILESYS
  uint8_t version;
  char    str_filename[24];
  File    file;

  // open file in read mode
  strcpy_P (str_filename, PSTR_SENSOR_ACTIVITY_DATA);
  if (ffsp->exists (str_filename))
  {
    file = ffsp->open (str_filename, "r");
    file.read ((uint8_t*)&version, sizeof (version));
    if (version == SENSOR_ACTIVITY_VERSION) file.read ((uint8_t*)&activity_data, sizeof (activity_data));
    file.close ();
  }
#endif    // USE_UFILESYS
}

// save data file
void SensorActivitySaveData ()
{
#ifdef USE_UFILESYS
  uint8_t  version;
  char     str_filename[24];
  File     file;

  // init data
  version = SENSOR_ACTIVITY_VERSION;

  // open file in write mode
  strcpy_P (str_filename, PSTR_SENSOR_ACTIVITY_DATA);
  file = ffsp->open (str_filename, "w");
  if (file > 0)
  {
    file.write ((uint8_t*)&version, sizeof (version));
    file.write ((uint8_t*)&activity_data, sizeof (activity_data));
    file.close ();
  }
#endif    // USE_UFILESYS
}

/*********************************************\
 *                 function
\*********************************************/

// reset daily activity (sunday:1 ... saturday:7)
void SensorActivityResetWeekData (const uint8_t dayofweek)
{
  uint8_t index, slot;

  // check parameters
  if ((dayofweek == 0) || (dayofweek > 7)) return;

  // reset day data
  index = (dayofweek + 5) % 7;
  for (slot = 0; slot < 12; slot ++) activity_data.arr_week[index][slot] = 0;
}

// declare daily activity (sunday:1 ... saturday:7)
void SensorActivitySetWeekData (const uint8_t dayofweek, const uint8_t hour, const uint8_t minute)
{
  uint8_t day, slot, shift;

  // check parameters
  if ((dayofweek == 0) || (dayofweek > 7)) return;
  if (hour > 23) return;
  if (minute > 59) return;

  // calculate slot and bit to set (8 slots in 2 hours)
  day = (dayofweek + 5) % 7;
  slot = hour / 2;
  shift = (hour % 2) * 4 + minute / 15;

  // update value
  activity_data.arr_week[day][slot] = activity_data.arr_week[day][slot] | (1 << shift);
}

void SensorActivityResetYearData (const uint8_t month)
{
  uint8_t index, slot;

  // check parameters
  if ((month == 0) || (month > 12)) return;

  // reset month data
  index = month - 1;
  for (slot = 0; slot < 31; slot ++) activity_data.arr_year[index][slot] = 0;
}

void SensorActivitySetYearData (const uint8_t month, const uint8_t day, const uint8_t hour)
{
  uint8_t idx_month, idx_day, shift;

  // check parameters
  if ((month == 0) || (month > 12)) return;
  if ((day == 0) || (day > 31)) return;
  if (hour > 23) return;

  // calculate slot and bit to set (8 slots in 2 hours)
  idx_month = month - 1;
  idx_day = day - 1;
  shift = hour / 3;

  // update value
  activity_data.arr_year[idx_month][idx_day] = activity_data.arr_year[idx_month][idx_day] | (1 << shift);
}

// get daily activity (monday:0 ... sunda:6)
uint8_t SensorActivityGetWeekData (const uint8_t day, const uint16_t slot)
{
  uint8_t index, shift, value;

  // check parameters
  if (day >= 7) return 0;
  if (slot >= 96) return 0;

  // calculate index and shift
  index = (uint8_t)(slot / 8);
  shift = (uint8_t)(slot % 8);

  // get value
  value = activity_data.arr_week[day][index] & (1 << shift);
  if (value > 0) value = 1;

  return value;
}

uint8_t SensorActivityGetYearData (const uint8_t month, const uint16_t slot)
{
  uint8_t  shift;
  uint16_t index, value;

  // check parameters
  if (month >= 12) return 0;
  if (slot >= 248) return 0;

  // calculate index and shift
  index = slot / 8;
  shift = (uint8_t)(slot % 8);

  // get value
  value = activity_data.arr_year[month][index] & (1 << shift);
  if (value > 0) value = 1;

  return value;
}

/*********************************************\
 *                   Callback
\*********************************************/

// called to init module
void SensorActivityInit ()
{
  // load data
  SensorActivityLoadData ();

  // if needed, set default strings
  if (activity_status.str_title[0] == 0) strcpy_P (activity_status.str_title, PSTR (SENSOR_ACTIVITY_TITLE));
  if (activity_status.str_color[0] == 0) strcpy_P (activity_status.str_color, PSTR (SENSOR_ACTIVITY_COLOR));

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Run activity_reset to reset all activity data"));
}

// called every second to check midnight
void SensorActivityEverySecond ()
{
  // if no RTC, ignore
  if (!RtcTime.valid) return;

  // check if hour has changed at midnight
  if (activity_status.hour == UINT8_MAX) activity_status.hour = RtcTime.hour;
  if (RtcTime.hour < activity_status.hour)
  {
    // reset data for current week day
    SensorActivityResetWeekData (RtcTime.day_of_week);

    // if 1st of month, reset data for current month
    if (RtcTime.day_of_month == 1) SensorActivityResetYearData (RtcTime.day_of_month);

    // save data
    SensorActivitySaveData ();
  }

  // set current hour
  activity_status.hour = RtcTime.hour;

  // if not managed, declare relay activity
  if (!activity_status.managed && bitRead (TasmotaGlobal.power, 0)) SensorActivityTrigger ();
}

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

// Append Teleinfo histo button to main page
void SensorActivityWebMainButton ()
{
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), PSTR_SENSOR_ACTIVITY_PAGE, activity_status.str_title);
}

// presence graph content
void SensorActivityWebGraph (const uint8_t period)
{
  uint8_t  slot_state, slot_new;
  uint16_t index;
  uint16_t nb_slot, slot;
  uint16_t nb_line, line;
  uint16_t day_start, day;
  uint16_t month_start, month;
  uint16_t line_height, bar_height;
  uint16_t last_x, graph_x, graph_y;
  char     str_text[16];
  char     str_style[16];

  // SVG : start
  WSContentSend_P (PSTR ("<svg class='graph' viewBox='%d %d %d %d' preserveAspectRatio='none'>\n"), 0, 0, SENSOR_ACTIVITY_WIDTH, SENSOR_ACTIVITY_HEIGHT + 1);

  // SVG : style
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));
  WSContentSend_P (PSTR ("text {font-size:2.5vh;fill:#aaa;}\n"));
  WSContentSend_P (PSTR ("text.head {font-size:3vh;}\n"));
  WSContentSend_P (PSTR ("text.now {font-size:3vh;font-weight:bold;font-style:italic;}\n"));
  WSContentSend_P (PSTR ("rect {fill:#333;stroke:none;opacity:0.75;}\n"));
  WSContentSend_P (PSTR ("rect.on {fill:%s;}\n"), activity_status.str_color);
  WSContentSend_P (PSTR ("line {stroke:#aaa;stroke-dasharray:2 6;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // set metrics according to period
  switch (period)
  {
    case SENSOR_ACTIVITY_PERIOD_WEEK: 
    nb_line = 7;              // 7 days
    nb_slot = 96;             // every 30mn
    break;
    
    case SENSOR_ACTIVITY_PERIOD_YEAR:
    nb_line = 12;             // 12 months
    nb_slot = 248;            // avery 3 hours on 31 days
    break;
  }

  // calculate line height and bar height
  line_height = SENSOR_ACTIVITY_HEIGHT / (nb_line + 1);
  bar_height  = line_height - 5;

  switch (period)
  {
    case SENSOR_ACTIVITY_PERIOD_WEEK: 

      // display header (0h  4h 8h 12h 16h 20h 24h)
      WSContentSend_P (PSTR ("<text text-anchor='start' x=%u y=%u>%uh</text>\n"), SENSOR_ACTIVITY_WIDTH_HEAD, bar_height - 10, 0);
      for (index = 1; index <= 11;index ++) WSContentSend_P (PSTR ("<text text-anchor='middle' x=%u y=%u>%uh</text>\n"), SENSOR_ACTIVITY_WIDTH_HEAD + index * SENSOR_ACTIVITY_WIDTH_LINE / 12, bar_height - 10, 2 * index);
      WSContentSend_P (PSTR ("<text text-anchor='end' x=%u y=%u>%uh</text>\n"), SENSOR_ACTIVITY_WIDTH_HEAD + SENSOR_ACTIVITY_WIDTH_LINE, bar_height - 10, 24);

      // loop thru days
      day_start = (uint16_t)((RtcTime.day_of_week - 1) % 7);
      graph_y   = 0;
      for (line = 0; line < 7; line ++)
      {
        // get upper line position
        graph_y += line_height;
        last_x   = SENSOR_ACTIVITY_WIDTH_HEAD;

        // convert to current day
        day = (day_start + line) % 7;

        // display week day
        index = (day + 1) % 7;
        strlcpy (str_text, kActivityWeekdayNames + index * 3, 4);
        if (line == 6) strcpy_P (str_style, PSTR ("now")); else strcpy_P (str_style, PSTR ("head"));
        WSContentSend_P (PSTR ("<text class='%s' x=%u y=%u>%s</text>\n"), str_style, 20, graph_y + bar_height - 25, str_text);

        // display background bar
        WSContentSend_P (PSTR ("<rect x=%u y=%u width=%u height=%u rx=%u ry=%u />\n"), SENSOR_ACTIVITY_WIDTH_HEAD, graph_y, SENSOR_ACTIVITY_WIDTH_LINE, bar_height, SENSOR_ACTIVITY_MAIN_RADIUS, SENSOR_ACTIVITY_MAIN_RADIUS);

        // loop to display slots of current day
        slot_state = 0;
        for (slot = 0; slot <= nb_slot; slot ++)
        {
          // read slot status
          slot_new = SensorActivityGetWeekData (day, slot);

          // calculate slot position
          graph_x = SENSOR_ACTIVITY_WIDTH_HEAD + SENSOR_ACTIVITY_WIDTH_LINE * slot / nb_slot;

          // start of slot
          if ((slot_new == 1) && (slot_state == 0)) last_x = graph_x;

          // if end of slot, draw it
          else if ((slot_new == 0) && (slot_state == 1)) WSContentSend_P (PSTR ("<rect class='on' x=%u y=%u width=%u height=%u rx=%u ry=%u />\n"), last_x, graph_y, graph_x - last_x, bar_height, SENSOR_ACTIVITY_SLOT_RADIUS, SENSOR_ACTIVITY_SLOT_RADIUS);

          // update slot state
          slot_state = slot_new;
        }
      }

      // loop to display separation lines every 2h
      last_x = SENSOR_ACTIVITY_WIDTH_HEAD;
      for (slot = 1; slot < 12; slot++)
      {
        graph_x = SENSOR_ACTIVITY_WIDTH_HEAD + SENSOR_ACTIVITY_WIDTH_LINE * slot / 12;
        WSContentSend_P (PSTR ("<line x1=%u y1=%u x2=%u y2=%u />\n"), graph_x, line_height, graph_x, SENSOR_ACTIVITY_HEIGHT - 5);
      }
      break;
    
    case SENSOR_ACTIVITY_PERIOD_YEAR:

      // display header (2  4  6  ...   30)
      graph_y = 30; 
      for (index = 0; index < 15; index ++) 
      {
        graph_x = SENSOR_ACTIVITY_WIDTH_HEAD + 3 * SENSOR_ACTIVITY_WIDTH_LINE / 62 + 2 * index * SENSOR_ACTIVITY_WIDTH_LINE / 31;
        WSContentSend_P (PSTR ("<text text-anchor='middle' x=%u y=%u>%u</text>\n"), graph_x, graph_y, 2 + index * 2);
      }

      // loop thru months
      month_start = (uint16_t)RtcTime.month % 12;
      graph_y = 0;
      for (line = 0; line < 12; line ++)
      {
        // get upper line position
        graph_y += line_height;
        last_x   = SENSOR_ACTIVITY_WIDTH_HEAD;

        // convert to current month
        month = (month_start + line) % 12;

        // display month name
        strlcpy (str_text, kMonthNames + month * 3, 4);
        if (line == 11) strcpy_P (str_style, PSTR ("now")); else strcpy_P (str_style, PSTR ("head"));
        WSContentSend_P (PSTR ("<text class='%s' x=%u y=%u>%s</text>\n"), str_style, 20, graph_y + bar_height - 15, str_text);

        // display background bar
        index = (uint16_t)kDaysInMonth[month] * SENSOR_ACTIVITY_WIDTH_LINE / 31;
        WSContentSend_P (PSTR ("<rect x=%u y=%u width=%u height=%u rx=%u ry=%u />\n"), SENSOR_ACTIVITY_WIDTH_HEAD, graph_y, index, bar_height, SENSOR_ACTIVITY_MAIN_RADIUS, SENSOR_ACTIVITY_MAIN_RADIUS);

        // loop to display slots of current month
        slot_state = SensorActivityGetYearData (month, 0);
        for (slot = 0; slot <= nb_slot; slot ++)
        {
          // read slot status
          slot_new = SensorActivityGetYearData (month, slot);

          // calculate slot position
          graph_x = SENSOR_ACTIVITY_WIDTH_HEAD + SENSOR_ACTIVITY_WIDTH_LINE * slot / nb_slot;

          // start of slot
          if ((slot_new == 1) && (slot_state == 0)) last_x = graph_x;

          // if end of slot, draw it
          else if ((slot_new == 0) && (slot_state == 1)) WSContentSend_P (PSTR ("<rect class='on' x=%u y=%u width=%u height=%u rx=%u ry=%u />\n"), last_x, graph_y, graph_x - last_x, bar_height, SENSOR_ACTIVITY_SLOT_RADIUS, SENSOR_ACTIVITY_SLOT_RADIUS);

          // update slot state
          slot_state = slot_new;
        }
      }

      // loop to display separation lines every day
      last_x = SENSOR_ACTIVITY_WIDTH_HEAD;
      for (slot = 1; slot < 31; slot++)
      {
        graph_x = SENSOR_ACTIVITY_WIDTH_HEAD + SENSOR_ACTIVITY_WIDTH_LINE * slot / 31;
        WSContentSend_P (PSTR ("<line x1=%u y1=%u x2=%u y2=%u />\n"), graph_x, line_height, graph_x, SENSOR_ACTIVITY_HEIGHT - 5);
      }
      break;
  }

  // SVG : end
  WSContentSend_P (PSTR ("</svg>\n"));
}

// Activity weekly and yearly page
void SensorActivityWebPage ()
{
  uint8_t index, period;
  char    str_text[8];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // if target temperature has been changed
  period = SENSOR_ACTIVITY_PERIOD_WEEK;
  if (Webserver->hasArg ("period"))
  {
    WebGetArg ("period", str_text, sizeof(str_text));
    period = (uint8_t)atoi (str_text);
  }

  // beginning page without authentification
  WSContentStart_P (activity_status.str_title, false);
  WSContentSend_P  (PSTR ("</script>\n"));

  // set page as scalable and set refresh every 10 mn
  WSContentSend_P (PSTR ("<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=yes'/>\n"));
  WSContentSend_P (PSTR ("<meta http-equiv='refresh' content='%u'/>\n"), 600);

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {margin:0.25rem auto;padding:0.1rem 0px;text-align:center;}\n"));
  WSContentSend_P (PSTR ("div.title {font-size:3vh;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.header {font-size:3vh;margin:2vh auto;}\n"));
  WSContentSend_P (PSTR ("a {color:white;}\n")); 
  WSContentSend_P (PSTR ("a:link {text-decoration:none;}\n"));

  WSContentSend_P (PSTR ("div.choice {display:inline-block;padding:0px 4px;margin:0px;border:1px #666 solid;border-radius:8px;background:none;color:#fff;}\n"));
  WSContentSend_P (PSTR ("div.choice div {display:inline-block;padding:2px 30px;border-radius:4px;background:#666;}\n"));
  WSContentSend_P (PSTR ("div.choice a div {background:none;}\n"));
  WSContentSend_P (PSTR ("div.choice a div:hover {background:#aaa;}\n"));

  WSContentSend_P (PSTR ("button.menu {background:%s;color:%s;line-height:2rem;font-size:1.2rem;cursor:pointer;border:none;border-radius:0.3rem;width:150px;margin:2vh;}\n"), COLOR_BUTTON, COLOR_BUTTON_TEXT);
  WSContentSend_P (PSTR ("div.graph {width:90%%;margin:2vh auto;}\n"));
  WSContentSend_P (PSTR ("svg.graph {width:100%%;height:70vh;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  WSContentSend_P (PSTR ("</head>\n"));

  // page body and title
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));
  WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText(SET_DEVICENAME));
  WSContentSend_P (PSTR ("<div class='header'>%s</div>\n"), activity_status.str_title);

  // periods
  WSContentSend_P (PSTR ("<div><div class='choice'>\n"));
  for (index = 0; index < SENSOR_ACTIVITY_PERIOD_MAX; index ++)
  {
    GetTextIndexed (str_text, sizeof (str_text), index, kActivityGraphPeriod);
    if (period != index) WSContentSend_P (PSTR ("<a href='%s?period=%u'>"), PSTR_SENSOR_ACTIVITY_PAGE, index);
    WSContentSend_P (PSTR ("<div>%s</div>"), str_text);
    if (period != index) WSContentSend_P (PSTR ("</a>"));
    WSContentSend_P (PSTR ("\n"));
  }
  WSContentSend_P (PSTR ("</div></div>\n"));        // choice & level1

  // SVG : start
  WSContentSend_P (PSTR ("<div class='graph'>\n"));
  SensorActivityWebGraph (period);
  WSContentSend_P (PSTR ("</div>\n"));              // graph

  // main menu button
  WSContentSend_P (PSTR ("<div><form action='/' method='get'><button class='menu'>%s</button></form></div>\n"), D_MAIN_MENU);

  // end of page
  WSContentStop ();
}

#endif    // USE_WEBSERVER

/***************************************\
 *              Interface
\***************************************/

bool XsnsSensorActivity (const uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function) 
  {
    case FUNC_INIT:
      SensorActivityInit ();
      break;

    case FUNC_COMMAND:
      result = DecodeCommand (kSensorActivityCommands, SensorActivityCommand);
      break;

    case FUNC_SAVE_BEFORE_RESTART:
      SensorActivitySaveData ();
      break;

    case FUNC_EVERY_SECOND:
      SensorActivityEverySecond ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_MAIN_BUTTON:
      SensorActivityWebMainButton ();
      break;

    case FUNC_WEB_ADD_HANDLER:
      Webserver->on (FPSTR (PSTR_SENSOR_ACTIVITY_PAGE), SensorActivityWebPage);
      break;
#endif  // USE_WEBSERVER
  }

  return result;
}

#endif    // USE_SENSOR_ACTIVITY
