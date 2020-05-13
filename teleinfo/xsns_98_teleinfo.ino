/*
  xnrg_16_teleinfo.ino - France Teleinfo energy sensor support for Sonoff-Tasmota
  
  Copyright (C) 2019  Nicolas Bernaerts

    05/05/2019 - v1.0 - Creation
    16/05/2019 - v1.1 - Add Tempo and EJP contracts
    08/06/2019 - v1.2 - Handle active and apparent power
    05/07/2019 - v2.0 - Rework with selection thru web interface
    02/01/2020 - v3.0 - Functions rewrite for Tasmota 8.x compatibility
    05/02/2020 - v3.1 - Add support for 3 phases meters
    14/03/2020 - v3.2 - Add apparent power graph
    05/04/2020 - v3.3 - Add Timezone management
    13/05/2020 - v3.4 - Add overload management per phase

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

#ifdef USE_ENERGY_SENSOR
#ifdef USE_TELEINFO

// declare energy driver and teleinfo sensor
#define XSNS_98   98

/*********************************************\
 *               Teleinfo
\*********************************************/

// web configuration page
#define D_PAGE_TELEINFO_CONFIG          "teleinfo"
#define D_PAGE_TELEINFO_GRAPH           "graph"
#define D_CMND_TELEINFO_MODE            "mode"
#define D_WEB_TELEINFO_CHECKED          "checked"

// graph data
#define TELEINFO_GRAPH_STEP             5           // collect graph data every 5 mn
#define TELEINFO_GRAPH_SAMPLE           288         // 24 hours if data is collected every 5mn
#define TELEINFO_GRAPH_WIDTH            800      
#define TELEINFO_GRAPH_HEIGHT           400 
#define TELEINFO_GRAPH_PERCENT_START    10      
#define TELEINFO_GRAPH_PERCENT_STOP     90

// colors
#define TELEINFO_POWER_OVERLOAD         "style='color:#FF0000;font-weight:bold;'"

// others
#define TELEINFO_MAX_PHASE              3      
#define TELEINFO_MESSAGE_BUFFER_SIZE    64

// form strings
const char INPUT_MODE_SELECT[] PROGMEM = "<input type='radio' name='%s' id='%d' value='%d' %s>%s";
const char INPUT_FORM_START[] PROGMEM = "<form method='get' action='%s'>";
const char INPUT_FORM_STOP[] PROGMEM = "</form>";
const char INPUT_FIELDSET_START[] PROGMEM = "<fieldset><legend><b>&nbsp;%s&nbsp;</b></legend><br />";
const char INPUT_FIELDSET_STOP[] PROGMEM = "</fieldset><br />";

// graph colors
const char arrColorPhase[TELEINFO_MAX_PHASE][8] PROGMEM = { "phase1", "phase2", "phase3" };

/*********************************************\
 *               Variables
\*********************************************/

// variables
int      teleinfo_graph_refresh;
uint32_t teleinfo_graph_index;
uint32_t teleinfo_graph_counter;
int      teleinfo_apparent_power[TELEINFO_MAX_PHASE];
int      arr_apparent_power[TELEINFO_MAX_PHASE][TELEINFO_GRAPH_SAMPLE];

/*********************************************\
 *               Functions
\*********************************************/

// Show JSON status (for MQTT)
void TeleinfoShowJSON (bool append)
{
  // if JSON is ready
  if (str_teleinfo_json.length () > 0)
  {
    // if we are in append mode, just append teleinfo data to current MQTT message
    if (append == true) snprintf_P (mqtt_data, sizeof(mqtt_data), "%s,%s", mqtt_data, str_teleinfo_json.c_str ());

    // else publish teleinfo message right away 
    else
    { 
      // create message { teleinfo }
      snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("{%s}"), str_teleinfo_json.c_str ());

      // publish full sensor state
      MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));
    }

    // reset teleinfo data and update status
    str_teleinfo_json = "";
    teleinfo_overload_json = false;
  }
}

// update graph history data
void TeleinfoUpdateHistory ()
{
  int phase;

  // set indexed graph values with current values
  for (phase = 0; phase < teleinfo_phase; phase++)
  {
    arr_apparent_power[phase][teleinfo_graph_index] = teleinfo_apparent_power[phase];
    teleinfo_apparent_power[phase] = 0;
  }

  // increase power data index and reset if max reached
  teleinfo_graph_index ++;
  teleinfo_graph_index = teleinfo_graph_index % TELEINFO_GRAPH_SAMPLE;
}

void TeleinfoEverySecond ()
{
  int phase;

  // loop thru the phases, to update apparent power to the max on the period
  for (phase = 0; phase < teleinfo_phase; phase++)
    teleinfo_apparent_power[phase] = max (int (Energy.apparent_power[phase]), teleinfo_apparent_power[phase]);

  // increment delay counter and if delay reached, update history data
  if (teleinfo_graph_counter == 0) TeleinfoUpdateHistory ();
  teleinfo_graph_counter ++;
  teleinfo_graph_counter = teleinfo_graph_counter % teleinfo_graph_refresh;

  // if overload has been detected, publish teleinfo data
  if (teleinfo_overload_json == true) TeleinfoShowJSON (false);
}

void TeleinfoDataInit ()
{
  int phase, index;

  // init default values
  str_teleinfo_contract  = "";
  teleinfo_graph_index   = 0;
  teleinfo_graph_counter = 0;
  teleinfo_graph_refresh = 60 * TELEINFO_GRAPH_STEP;

  // initialise graph data
  for (phase = 0; phase < TELEINFO_MAX_PHASE; phase++)
  {
    teleinfo_apparent_power[phase] = 0;
    for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++) 
      arr_apparent_power[phase][index] = 0;
  }
}

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

// Teleinfo mode select combo
void TeleinfoWebSelectMode ()
{
  uint16_t actual_mode;
  String   str_checked;

  // get mode
  actual_mode = TeleinfoGetMode ();

  // selection : disabled
  str_checked = "";
  if (actual_mode == 0) str_checked = D_WEB_TELEINFO_CHECKED;
  WSContentSend_P (INPUT_MODE_SELECT, D_CMND_TELEINFO_MODE, 0, 0, str_checked.c_str (), D_TELEINFO_DISABLED);
  WSContentSend_P (PSTR ("<br/>"));

  if (teleinfo_configured == true)
  {
    // selection : 1200 baud
    str_checked = "";
    if (actual_mode == 1200) str_checked = D_WEB_TELEINFO_CHECKED;
    WSContentSend_P (INPUT_MODE_SELECT, D_CMND_TELEINFO_MODE, 1200, 1200, str_checked.c_str (), D_TELEINFO_1200);
    WSContentSend_P (PSTR ("<br/>"));

    // selection : 9600 baud
    str_checked = "";
    if (actual_mode == 9600) str_checked = D_WEB_TELEINFO_CHECKED;
    WSContentSend_P (INPUT_MODE_SELECT, D_CMND_TELEINFO_MODE, 9600, 9600, str_checked.c_str (), D_TELEINFO_9600);
    WSContentSend_P (PSTR ("<br/>"));
  }
}

// append Teleinfo graph button to main page
void TeleinfoWebMainButton ()
{
    // Teleinfo control page button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_PAGE_TELEINFO_GRAPH, D_TELEINFO_GRAPH);
}

// append Teleinfo configuration button to configuration page
void TeleinfoWebButton ()
{
  // heater and meter configuration button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>"), D_PAGE_TELEINFO_CONFIG, D_TELEINFO_CONFIG);
}

// append Teleinfo state to main page
bool TeleinfoWebSensor ()
{
  int    index;
  String str_power, str_style;

  // display contract number
  WSContentSend_PD (PSTR("{s}%s{m}%s{e}"), D_TELEINFO_CONTRACT, str_teleinfo_contract.c_str());

  // display apparent power
  for (index = 0; index < teleinfo_phase; index++)
  {
    // set display color according to overload
    str_style = "";
    if (teleinfo_overload_phase[index] == true) str_style = TELEINFO_POWER_OVERLOAD;

    // append power
    if (str_power.length () > 0) str_power += " / ";
    str_power += PSTR("<span ") + str_style + PSTR(">") + String (Energy.apparent_power[index], 0) + PSTR("</span>") ;
  }
  WSContentSend_PD (PSTR("<tr><th>%s{m}%s VA{e}"), D_TELEINFO_POWER, str_power.c_str ());

  // display frame counter
  WSContentSend_PD (PSTR("{s}%s{m}%d{e}"), D_TELEINFO_COUNTER, teleinfo_framecount);
}

// Teleinfo web page
void TeleinfoWebPageConfig ()
{
  char argument[TELEINFO_MESSAGE_BUFFER_SIZE];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // get teleinfo mode according to MODE parameter
  if (WebServer->hasArg(D_CMND_TELEINFO_MODE))
  {
    WebGetArg (D_CMND_TELEINFO_MODE, argument, TELEINFO_MESSAGE_BUFFER_SIZE);
    TeleinfoSetMode ((uint16_t) atoi (argument)); 
  }

  // beginning of form
  WSContentStart_P (D_TELEINFO_CONFIG);
  WSContentSendStyle ();
  WSContentSend_P (INPUT_FORM_START, D_PAGE_TELEINFO_CONFIG);

  // mode selection
  WSContentSend_P (INPUT_FIELDSET_START, D_TELEINFO_MODE);
  TeleinfoWebSelectMode ();

  // end of form
  WSContentSend_P (INPUT_FIELDSET_STOP);
  WSContentSend_P (PSTR ("<button name='save' type='submit' class='button bgrn'>%s</button>"), D_SAVE);
  WSContentSend_P (INPUT_FORM_STOP);

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

// Apparent power graph
void TeleinfoWebDisplayGraph ()
{
  TIME_T   current_dst;
  uint32_t current_time;
  int      index, arridx, phase, hour, power, power_min, power_max;
  int      graph_x, graph_y, graph_left, graph_right, graph_width, graph_hour;  
  char     str_hour[4];
  String   str_color;

  // max power adjustment
  power_min = 0;
  power_max = TeleinfoGetContractPower ();

  // loop thru phasis and power records
  for (phase = 0; phase < TELEINFO_MAX_PHASE; phase++)
    for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
    {
      power = arr_apparent_power[phase][index];
      if ((power != INT_MAX) && (power > power_max)) power_max = power;
    }

  // boundaries of SVG graph
  graph_left  = TELEINFO_GRAPH_PERCENT_START * TELEINFO_GRAPH_WIDTH / 100;
  graph_right = TELEINFO_GRAPH_PERCENT_STOP * TELEINFO_GRAPH_WIDTH / 100;
  graph_width = graph_right - graph_left;

  // start of SVG graph
  WSContentSend_P (PSTR ("<svg viewBox='0 0 %d %d'>\n"), TELEINFO_GRAPH_WIDTH, TELEINFO_GRAPH_HEIGHT);

  // graph curve zone
  WSContentSend_P (PSTR ("<rect x='%d%%' y='0%%' width='%d%%' height='100%%' rx='10' />\n"), TELEINFO_GRAPH_PERCENT_START, TELEINFO_GRAPH_PERCENT_STOP - TELEINFO_GRAPH_PERCENT_START);

  // graph separation lines
  WSContentSend_P (PSTR ("<line class='dash' x1='%d%%' y1='25%%' x2='%d%%' y2='25%%' />\n"), TELEINFO_GRAPH_PERCENT_START, TELEINFO_GRAPH_PERCENT_STOP);
  WSContentSend_P (PSTR ("<line class='dash' x1='%d%%' y1='50%%' x2='%d%%' y2='50%%' />\n"), TELEINFO_GRAPH_PERCENT_START, TELEINFO_GRAPH_PERCENT_STOP);
  WSContentSend_P (PSTR ("<line class='dash' x1='%d%%' y1='75%%' x2='%d%%' y2='75%%' />\n"), TELEINFO_GRAPH_PERCENT_START, TELEINFO_GRAPH_PERCENT_STOP);

  // power units
  WSContentSend_P (PSTR ("<text class='unit' x='%d%%' y='%d%%'>VA</text>\n"), TELEINFO_GRAPH_PERCENT_STOP + 2, 5, 100);
  WSContentSend_P (PSTR ("<text class='power' x='%d%%' y='%d%%'>%d</text>\n"), 1, 5, power_max);
  WSContentSend_P (PSTR ("<text class='power' x='%d%%' y='%d%%'>%d</text>\n"), 1, 27, power_max * 3 / 4);
  WSContentSend_P (PSTR ("<text class='power' x='%d%%' y='%d%%'>%d</text>\n"), 1, 52, power_max / 2);
  WSContentSend_P (PSTR ("<text class='power' x='%d%%' y='%d%%'>%d</text>\n"), 1, 77, power_max / 4);
  WSContentSend_P (PSTR ("<text class='power' x='%d%%' y='%d%%'>%d</text>\n"), 1, 99, 0);


  // --------------------
  //   Apparent power
  // --------------------

  // loop thru phasis
  for (phase = 0; phase < teleinfo_phase; phase++)
  {
    // loop for the target humidity graph
    WSContentSend_P (PSTR ("<polyline class='%s' points='"), arrColorPhase[phase]);
    for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
    {
      // get target temperature value and set to minimum if not defined
      arridx = (index + teleinfo_graph_index) % TELEINFO_GRAPH_SAMPLE;
      power  = arr_apparent_power[phase][arridx];

      // if power is defined
      if (power > 0)
      {
        // calculate current position
        graph_x = graph_left + (graph_width * index / TELEINFO_GRAPH_SAMPLE);
        graph_y = TELEINFO_GRAPH_HEIGHT - (power * TELEINFO_GRAPH_HEIGHT / power_max);

        // add the point to the line
        WSContentSend_P (PSTR("%d,%d "), graph_x, graph_y);
      }
    }
    WSContentSend_P (PSTR("'/>\n"));
  }

  // ------------
  //     Time
  // ------------

  // get current time
  current_time = LocalTime();
  BreakTime (current_time, current_dst);

  // calculate width of remaining (minutes) till next hour
  current_dst.hour = (current_dst.hour + 1) % 24;
  graph_hour = ((60 - current_dst.minute) * graph_width / 1440) - 15; 

  // if shift is too small, shift to next hour
  if (graph_hour < 0)
  {
    current_dst.hour = (current_dst.hour + 1) % 24;
    graph_hour += graph_width / 24; 
  }

  // dislay first time mark
  graph_x = graph_left + graph_hour;
  sprintf(str_hour, "%02d", current_dst.hour);
  WSContentSend_P (PSTR ("<text class='time' x='%d' y='52%%'>%sh</text>\n"), graph_x, str_hour);

  // dislay next 5 time marks (every 4 hours)
  for (index = 0; index < 5; index++)
  {
    current_dst.hour = (current_dst.hour + 4) % 24;
    graph_x += graph_width / 6;
    sprintf(str_hour, "%02d", current_dst.hour);
    WSContentSend_P (PSTR ("<text class='time' x='%d' y='52%%'>%sh</text>\n"), graph_x, str_hour);
  }

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
}

// Graph public page
void TeleinfoWebPageGraph ()
{
  int     phase;
  float   value, target;
  String  str_power;

  // beginning of form without authentification with 60 seconds auto refresh
  WSContentStart_P (D_TELEINFO_GRAPH, false);
  WSContentSend_P (PSTR ("</script>\n"));
  WSContentSend_P (PSTR ("<meta http-equiv='refresh' content='%d;URL=/%s' />\n"), 60, D_PAGE_TELEINFO_GRAPH);
  
  // page style
  WSContentSend_P (PSTR ("<style>\n"));

  WSContentSend_P (PSTR ("body {color:white;background-color:#303030;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {width:100%%;margin:auto;padding:3px 0px;text-align:center;vertical-align:middle;}\n"));

  WSContentSend_P (PSTR (".title {font-size:5vh;}\n"));
  WSContentSend_P (PSTR (".contract {font-size:3vh;}\n"));
  WSContentSend_P (PSTR (".phase {font-size:4vh;}\n"));

  WSContentSend_P (PSTR (".power {font-size:24px;}\n"));
  WSContentSend_P (PSTR (".time {font-size:20px;}\n"));
  WSContentSend_P (PSTR (".bold {font-weight:bold;}\n"));

  WSContentSend_P (PSTR (".graph {max-width:%dpx;}\n"), TELEINFO_GRAPH_WIDTH);
  WSContentSend_P (PSTR (".phase1 {color:#FFFF33;}\n"));
  WSContentSend_P (PSTR (".phase2 {color:#FF8C00;}\n"));
  WSContentSend_P (PSTR (".phase3 {color:#FF0000;}\n"));

  WSContentSend_P (PSTR ("rect.graph {stroke:grey;stroke-dasharray:1;fill:#101010;}\n"));
  WSContentSend_P (PSTR ("polyline {fill:none;stroke-width:2;}\n"));
  WSContentSend_P (PSTR ("polyline.phase1 {stroke:yellow;}\n"));
  WSContentSend_P (PSTR ("polyline.phase2 {stroke:orange;}\n"));
  WSContentSend_P (PSTR ("polyline.phase3 {stroke:red;}\n"));
  WSContentSend_P (PSTR ("line.dash {stroke:grey;stroke-width:1;stroke-dasharray:8;}\n"));
  WSContentSend_P (PSTR ("text.power {stroke:yellow;fill:yellow;}\n"));
  WSContentSend_P (PSTR ("text.unit {stroke:white;fill:white;}\n"));
  WSContentSend_P (PSTR ("text.time {stroke:white;fill:white;}\n"));

  WSContentSend_P (PSTR ("</style>\n</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body>\n"));

  // room name
  WSContentSend_P (PSTR ("<div class='title bold'>%s</div>\n"), SettingsText(SET_FRIENDLYNAME1));

  // contract
  WSContentSend_P (PSTR ("<div class='contract'>%s %s</div>\n"), D_TELEINFO_CONTRACT, str_teleinfo_contract.c_str());

  // display apparent power
  for (phase = 0; phase < teleinfo_phase; phase++)
  {
    if (str_power.length () > 0) str_power += " - ";
    str_power += PSTR ("<span class='") + String (arrColorPhase[phase]) + PSTR ("'>");
    str_power += String (Energy.apparent_power[phase], 0);
    str_power += PSTR ("</span>");
  }
  WSContentSend_P (PSTR ("<div class='phase'>%s VA</div>\n"), str_power.c_str());

  // display power graph
  WSContentSend_P (PSTR ("<div class='graph'>\n"));
  TeleinfoWebDisplayGraph ();
  WSContentSend_P (PSTR ("</div>\n"));

  // end of page
  WSContentStop ();
}

#endif  // USE_WEBSERVER

/*******************************************\
 *                 Interface
\*******************************************/

// teleinfo sensor
bool Xsns98 (uint8_t function)
{
  bool result = false;
  
  // swtich according to context
  switch (function) 
  {
    case FUNC_INIT:
      TeleinfoDataInit ();
      break;
    case FUNC_EVERY_SECOND:
      TeleinfoEverySecond ();
      break;
    case FUNC_JSON_APPEND:
      if (teleinfo_enabled == true) TeleinfoShowJSON (true);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      WebServer->on ("/" D_PAGE_TELEINFO_CONFIG, TeleinfoWebPageConfig);
      WebServer->on ("/" D_PAGE_TELEINFO_GRAPH, TeleinfoWebPageGraph);
      break;
    case FUNC_WEB_ADD_BUTTON:
      TeleinfoWebButton ();
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      TeleinfoWebMainButton ();
      break;
    case FUNC_WEB_SENSOR:
      TeleinfoWebSensor ();
      break;
#endif  // USE_WEBSERVER

  }
  return result;
}

#endif   // USE_TELEINFO
#endif   // USE_ENERGY_SENSOR
