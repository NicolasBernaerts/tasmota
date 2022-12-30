/*
  xsns_126_pilotwire.ino - French Pilot Wire (Fil Pilote) support

  Copyright (C) 2019/2020  Theo Arends, Nicolas Bernaerts
    05/04/2019 - v1.0  - Creation
    12/04/2019 - v1.1  - Save settings in Settings.weight... variables
    10/06/2019 - v2.0  - Complete rewrite to add web management
    25/06/2019 - v2.1  - Add DHT temperature sensor and settings validity control
    05/07/2019 - v2.2  - Add embeded icon
    05/07/2019 - v3.0  - Add max power management with automatic offload
                         Save power settings in Settings.energy... variables
    12/12/2019 - v3.1  - Add public configuration page http://.../control
    30/12/2019 - v4.0  - Functions rewrite for Tasmota 8.x compatibility
    06/01/2019 - v4.1  - Handle offloading with finite state machine
    09/01/2019 - v4.2  - Separation between Offloading driver and Pilotwire sensor
    15/01/2020 - v5.0  - Separate temperature driver and add remote MQTT sensor
    05/02/2020 - v5.1  - Block relay command if not coming from a mode set
    21/02/2020 - v5.2  - Add daily temperature graph
    24/02/2020 - v5.3  - Add control button to main page
    27/02/2020 - v5.4  - Add target temperature and relay state to temperature graph
    01/03/2020 - v5.5  - Add timer management with Outside mode
    13/03/2020 - v5.6  - Add time to graph
    05/04/2020 - v5.7  - Add timezone management
    18/04/2020 - v6.0  - Handle direct connexion of heater in addition to pilotwire
    22/08/2020 - v6.1  - Handle out of range values during first flash
    24/08/2020 - v6.5  - Add status icon to Web UI 
    12/09/2020 - v6.6  - Add offload icon status 
    10/10/2020 - v6.8  - Handle graph with javascript auto update 
    14/10/2020 - v6.9  - Serve icons thru web server 
    18/10/2020 - v6.10 - Handle priorities as list of device types
                         Add randomisation to reconnexion
    04/11/2020 - v6.11 - Tasmota 9.0 compatibility
    11/11/2020 - v6.12 - Update to Offload v2.5
                         Add /data.json for history data
    23/04/2021 - v6.20 - Add fixed IP and remove use of String to avoid heap fragmentation
    20/06/2021 - v6.21 - Change in remote temperature sensor management (thanks to Bernard Monot) 
    04/09/2021 - v7.0  - Save configuration and historical data in LittleFS partition if available
    15/11/2021 - v7.1  - Redesign of control page and based on Tasmota 10.0
    15/11/2021 - v8.0  - Merge Offload and Pilotwire projects, based on Tasmota 10
                         Add movement detection with Nobody and Vacancy mode
                         Redesign of pilotwire control page
    02/01/2022 - v9.0  - Complete rework to simplify states
                         Add Open Window detection
    08/04/2022 - v9.1  - Switch from icons to emojis
    16/10/2022 - v10.0 - Add Ecowatt signal management
    03/12/2022 - v10.1 - Redesign of movement detection management
  
  If LittleFS partition is available :
    - settings are stored in /pilotwire.cfg and /offload.cfg
    - room picture can be stored as /room.jpg

  If no LittleFS partition is available, settings are stored using knx_CB_addr and knx_CB_param parameters.

    - Settings.knx_CB_param[0] = Device type (pilotewire or direct)
    - Settings.knx_CB_param[1] = Open Window detection flag
    - Settings.knx_CB_param[2] = Presence detection flag
    - Settings.knx_CB_param[3] = Ecowatt management flag

    - Settings.knx_CB_addr[0] = Presence detection initial timeout (mn)
    - Settings.knx_CB_addr[1] = Presence detection normal timeout (mn)

    - Settings.knx_CB_addr[2] = Lowest acceptable temperature (absolute)
    - Settings.knx_CB_addr[3] = Highest acceptable temperature (absolute)
    - Settings.knx_CB_addr[4] = Comfort target temperature (absolute)
    - Settings.knx_CB_addr[5] = Night mode target temperature (relative)
    - Settings.knx_CB_addr[6] = Eco mode target temperature (relative)
    - Settings.knx_CB_addr[7] = NoFrost mode target temperature (relative)
    - Settings.knx_CB_addr[8] = Ecowatt level 2 target temperature (relative)
    - Settings.knx_CB_addr[9] = Ecowatt level 3 target temperature (relative)

  Absolute temperature are stored as x10 (12.5 °C = 125)
  Relative temperature are stored as positive or negative
    negative : 1000 - x10 (900 = -10°C, 950 = -5°C, ...)
    positive : 1000 + x10 (1100 = 10°C, 1200 = 20°C, ...)

  
  WINDOW OPEN DETECTION
  ---------------------
  To enable Window Open Detection :
    - enable option it in the Pilotwire configuration page
    - if temperature drops of 0.5°C in less than 2mn, window is considered as opened, heater stops
    - if temperature increases of 0.2°C in less than 10mn, window is considered as closed again, heater restart

  PRESENCE DETECTION
  ---------------------
  To enable Presence Detection :
    - enable option it in the Pilotwire configuration page
    - connect local presence detector and declare it as Input 1 or Serial Tx/Rx
    - or declare topic for remote presence detector in Sensor configuration page
    - It should be detected at next restart
  When presence detection is enabled, target temperature is set to Eco if no presence detected after specified timeout.
 
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

#ifdef USE_PILOTWIRE

#define XSNS_126                       126

/*************************************************\
 *               Constants
\*************************************************/

#define D_PILOTWIRE_PAGE_CONFIG       "config"
#define D_PILOTWIRE_PAGE_STATUS       "status"
#define D_PILOTWIRE_PAGE_CONTROL      "control"
#define D_PILOTWIRE_PAGE_UPDATE       "control.upd"
#define D_PILOTWIRE_PAGE_GRAPH        "graph"
#define D_PILOTWIRE_PAGE_BASE_SVG     "graph-base.svg"
#define D_PILOTWIRE_PAGE_DATA_SVG     "graph-data.svg"

#define D_CMND_PILOTWIRE_HELP         "help"
#define D_CMND_PILOTWIRE_PERIOD       "period"
#define D_CMND_PILOTWIRE_HISTO        "histo"
#define D_CMND_PILOTWIRE_HEIGHT       "height"
#define D_CMND_PILOTWIRE_TYPE         "type"
#define D_CMND_PILOTWIRE_MODE         "mode"
#define D_CMND_PILOTWIRE_TARGET       "target"
#define D_CMND_PILOTWIRE_WINDOW       "window"
#define D_CMND_PILOTWIRE_PRESENCE     "pres"
#define D_CMND_PILOTWIRE_LOW          "low"
#define D_CMND_PILOTWIRE_HIGH         "high"
#define D_CMND_PILOTWIRE_COMFORT      "comfort"
#define D_CMND_PILOTWIRE_NIGHT        "night"
#define D_CMND_PILOTWIRE_ECO          "eco"
#define D_CMND_PILOTWIRE_NOFROST      "nofrost"
#define D_CMND_PILOTWIRE_ECOWATT      "ecowatt"
#define D_CMND_PILOTWIRE_ECOWATT_N2   "ecowatt2"
#define D_CMND_PILOTWIRE_ECOWATT_N3   "ecowatt3"
#define D_CMND_PILOTWIRE_INITIAL      "initial"
#define D_CMND_PILOTWIRE_NORMAL       "normal"

#define D_JSON_PILOTWIRE              "Pilotwire"
#define D_JSON_PILOTWIRE_MODE         "Mode"
#define D_JSON_PILOTWIRE_STATUS       "Status"
#define D_JSON_PILOTWIRE_HEATING      "Heating"
#define D_JSON_PILOTWIRE_WINDOW       "Window"
#define D_JSON_PILOTWIRE_PRESENCE     "Presence"
#define D_JSON_PILOTWIRE_DELAY        "Delay"
#define D_JSON_PILOTWIRE_TARGET       "Target"
#define D_JSON_PILOTWIRE_TEMPERATURE  "Temperature"

#define D_PILOTWIRE                   "Pilotwire"
#define D_PILOTWIRE_MODE              "Mode"
#define D_PILOTWIRE_PRESENCE          "Presence"
#define D_PILOTWIRE_WINDOW            "Window"
#define D_PILOTWIRE_TARGET            "Target"
#define D_PILOTWIRE_CONFIGURE         "Configure"
#define D_PILOTWIRE_CONNEXION         "Connexion"
#define D_PILOTWIRE_OPTION            "Options"
#define D_PILOTWIRE_GRAPH             "Graph"
#define D_PILOTWIRE_STATUS            "Status"
#define D_PILOTWIRE_OFF               "Off"
#define D_PILOTWIRE_COMFORT           "Comfort"
#define D_PILOTWIRE_NIGHT             "Night"
#define D_PILOTWIRE_ECO               "Economy"
#define D_PILOTWIRE_NOFROST           "No frost"
#define D_PILOTWIRE_CONTROL           "Control"
#define D_PILOTWIRE_TEMPERATURE       "Temp."
#define D_PILOTWIRE_DIRECT            "Direct"
#define D_PILOTWIRE_CHECKED           "checked"
#define D_PILOTWIRE_DETECTION         "Detection"
#define D_PILOTWIRE_SIGNAL            "Signal"
#define D_PILOTWIRE_ECOWATT           "Ecowatt"

// color codes
#define PILOTWIRE_COLOR_BACKGROUND    "#252525"       // page background
#define PILOTWIRE_COLOR_LOW           "#85c1e9"       // low temperature target
#define PILOTWIRE_COLOR_MEDIUM        "#ff9933"       // medium temperature target
#define PILOTWIRE_COLOR_HIGH          "#cc3300"       // high temperature target

#ifdef USE_UFILESYS
#define D_PILOTWIRE_FILE_CFG                 "/pilotwire.cfg"                 // configuration file
const char D_PILOTWIRE_FILE_WEEK[] PROGMEM = "/pilotwire-week-%02u.csv";      // history files label and filename
#endif    // USE_UFILESYS

// constant : temperature
#define PILOTWIRE_TEMP_THRESHOLD              0.25      // temperature threshold to switch on/off (°C)
#define PILOTWIRE_TEMP_STEP                   0.5       // temperature selection step (°C)
#define PILOTWIRE_TEMP_UPDATE                 5         // temperature update delay (s)
#define PILOTWIRE_TEMP_LIMIT_MIN              6         // minimum acceptable temperature
#define PILOTWIRE_TEMP_LIMIT_MAX              30        // maximum acceptable temperature
#define PILOTWIRE_TEMP_DEFAULT_LOW            12        // low range selectable temperature
#define PILOTWIRE_TEMP_DEFAULT_HIGH           22        // high range selectable temperature
#define PILOTWIRE_TEMP_DEFAULT_NORMAL         18        // default temperature
#define PILOTWIRE_TEMP_DEFAULT_NIGHT          -1        // default night mode adjustment
#define PILOTWIRE_TEMP_DEFAULT_ECO            -3        // default eco mode adjustment
#define PILOTWIRE_TEMP_DEFAULT_NOFROST        7         // default no frost mode adjustment
#define PILOTWIRE_TEMP_SCALE_LOW              19        // temperature under 19 is energy friendly
#define PILOTWIRE_TEMP_SCALE_HIGH             21        // temperature above 21 is energy wastage

// constant : open window detection
#define PILOTWIRE_WINDOW_SAMPLE_NBR           24        // number of temperature samples to detect opened window (4mn for 1 sample every 10s)
#define PILOTWIRE_WINDOW_OPEN_PERIOD          10        // delay between 2 temperature samples in open window detection 
#define PILOTWIRE_WINDOW_OPEN_DROP            0.5       // temperature drop for window open detection (°C)
#define PILOTWIRE_WINDOW_CLOSE_INCREASE       0.2       // temperature increase to detect window closed (°C)  

// constant : presence detection
#define PILOTWIRE_PRESENCE_IGNORE             30        // duration to ignore presence detection, to avoid relay change perturbation (sec)
#define PILOTWIRE_PRESENCE_TIMEOUT            5         // timeout to consider current presence is detected (sec)
#define PILOTWIRE_PRESENCE_TIMEOUT_INITIAL    1440      // initial timeout for presence detection (mn)
#define PILOTWIRE_PRESENCE_TIMEOUT_NORMAL     30        // normal timeout for presence detection (mn)

// constant : graph
#define PILOTWIRE_GRAPH_SAMPLE                600       // number of graph points 
#define PILOTWIRE_GRAPH_WIDTH                 1200      // width of graph (pixels)
#define PILOTWIRE_GRAPH_HEIGHT                600       // height of graph (pixels)
#define PILOTWIRE_GRAPH_STEP                  200       // graph height mofification step
#define PILOTWIRE_GRAPH_PERCENT_START         4         // percent of display before graph
#define PILOTWIRE_GRAPH_PERCENT_STOP          96        // percent of display after graph
#define PILOTWIRE_GRAPH_HEATING               90        // percent level to display heating state

// Historic data files
#define PILOTWIRE_HISTO_MIN_FREE              20        // minimum free size on filesystem (Kb)
#define PILOTWIRE_HISTO_WEEK_MAX              52        // number of weekly histotisation files (1 year)

// constant chains
const char PILOTWIRE_FIELDSET_START[] PROGMEM = "<fieldset><legend><b>&nbsp;%s %s&nbsp;</b></legend>\n";
const char PILOTWIRE_FIELDSET_STOP[]  PROGMEM = "</fieldset><br />\n";
const char PILOTWIRE_TEXT[]           PROGMEM = "<text x='%d%%' y='%d%%'>%s</text>\n";
const char PILOTWIRE_FIELD_FULL[]     PROGMEM = "<p>%s %s<span class='key'>%s</span><br><input type='number' name='%s' min='%d' max='%d' step='%s' value='%s'></p>\n";

// device control
enum PilotwireDevices { PILOTWIRE_DEVICE_PILOTWIRE, PILOTWIRE_DEVICE_DIRECT, PILOTWIRE_DEVICE_MAX };
const char kPilotwireDevice[] PROGMEM = "Pilotwire|Direct";                                                                 // device type labels

enum PilotwireMode { PILOTWIRE_MODE_OFF, PILOTWIRE_MODE_ECO, PILOTWIRE_MODE_NIGHT, PILOTWIRE_MODE_COMFORT, PILOTWIRE_MODE_FORCED, PILOTWIRE_MODE_MAX };
const char kPilotwireModeIcon[] PROGMEM = "❌|🌙|💤|🔆|🔥";                                 // running mode icons
const char kPilotwireModeLabel[] PROGMEM = "Off|Economie|Nuit|Confort|Forcé";               // running mode labels

enum PilotwireTarget { PILOTWIRE_TARGET_LOW, PILOTWIRE_TARGET_HIGH, PILOTWIRE_TARGET_FORCED, PILOTWIRE_TARGET_NOFROST, PILOTWIRE_TARGET_COMFORT, PILOTWIRE_TARGET_ECO, PILOTWIRE_TARGET_NIGHT, PILOTWIRE_TARGET_ECOWATT2, PILOTWIRE_TARGET_ECOWATT3, PILOTWIRE_TARGET_MAX };
const char kPilotwireTargetIcon[] PROGMEM = "➖|➕|🔥|❄|🔆|🌙|💤|🟠|🔴";                                                                       // target temperature icons
const char kPilotwireTargetLabel[] PROGMEM = "Minimale|Maximale|Forcée|Hors gel|Confort|Economie|Nuit|Ecowatt niveau 2|Ecowatt niveau 3";          // target temperature labels

// week days name for graph
const char kPilotwireWeekDay[] PROGMEM = "Mon|Tue|Wed|Thu|Fri|Sat|Sun";                                           // week days labels

enum PilotwirePresence { PILOTWIRE_PRESENCE_NORMAL, PILOTWIRE_PRESENCE_INITIAL, PILOTWIRE_PRESENCE_DISABLED, PILOTWIRE_PRESENCE_MAX };

// list of events associated with heating stage
enum PilotwireEvent { PILOTWIRE_EVENT_NONE, PILOTWIRE_EVENT_HEATING, PILOTWIRE_EVENT_OFFLOAD, PILOTWIRE_EVENT_WINDOW, PILOTWIRE_EVENT_NOBODY, PILOTWIRE_EVENT_ECOWATT2, PILOTWIRE_EVENT_ECOWATT3, PILOTWIRE_EVENT_MAX };
const char kPilotwireEventIcon[] PROGMEM = " |🔥|⚡|🪟|🚷|🟠|🔴";                                                     // icon associated with event

// graph periods
enum PilotwireGraphArray { PILOTWIRE_ARRAY_LIVE, PILOTWIRE_ARRAY_HISTO, PILOTWIRE_ARRAY_MAX };                      // available arrays
enum PilotwireGraphPeriod { PILOTWIRE_PERIOD_LIVE, PILOTWIRE_PERIOD_WEEK, PILOTWIRE_PERIOD_MAX };                   // available periods
const char kPilotwireGraphPeriod[] PROGMEM = "Live|Week";                                                           // period labels
const long ARR_PILOTWIRE_PERIOD_SAMPLE[] = { 86400 / PILOTWIRE_GRAPH_SAMPLE, 604800 / PILOTWIRE_GRAPH_SAMPLE };     // number of seconds between samples

// pilotwire commands
const char kPilotwireCommand[] PROGMEM = "pw_" "|" D_CMND_PILOTWIRE_HELP "|" D_CMND_PILOTWIRE_TYPE "|" D_CMND_PILOTWIRE_MODE "|" D_CMND_PILOTWIRE_TARGET "|" D_CMND_PILOTWIRE_ECOWATT "|" D_CMND_PILOTWIRE_WINDOW "|" D_CMND_PILOTWIRE_PRESENCE "|" D_CMND_PILOTWIRE_LOW "|" D_CMND_PILOTWIRE_HIGH "|" D_CMND_PILOTWIRE_NOFROST "|" D_CMND_PILOTWIRE_COMFORT "|" D_CMND_PILOTWIRE_ECO "|" D_CMND_PILOTWIRE_NIGHT "|" D_CMND_PILOTWIRE_ECOWATT_N2 "|" D_CMND_PILOTWIRE_ECOWATT_N3 "|" D_CMND_PILOTWIRE_INITIAL "|" D_CMND_PILOTWIRE_NORMAL;
void (* const PilotwireCommand[])(void) PROGMEM = { &CmndPilotwireHelp, &CmndPilotwireType, &CmndPilotwireMode, &CmndPilotwireTarget, &CmndPilotwireEcowatt, &CmndPilotwireWindow, &CmndPilotwirePresence, &CmndPilotwireTargetLow, &CmndPilotwireTargetHigh, &CmndPilotwireTargetNofrost, &CmndPilotwireTargetComfort, &CmndPilotwireTargetEco, &CmndPilotwireTargetNight, &CmndPilotwireTargetEcowatt2, &CmndPilotwireTargetEcowatt3, &CmndPilotwireTimeoutInitial, &CmndPilotwireTimeoutNormal };

/****************************************\
 *               Icons
 * 
 *      xxd -i -c 256 icon.png
\****************************************/

// icon : none
#define PILOTWIRE_ICON_LOGO       "/room.jpg"

#define PILOTWIRE_ICON_DEFAULT    "/default.png"      
char pilotwire_default_png[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x04, 0x03, 0x00, 0x00, 0x00, 0x31, 0x10, 0x7c, 0xf8, 0x00, 0x00, 0x00, 0x15, 0x50, 0x4c, 0x54, 0x45, 0x6f, 0x72, 0x6d, 0x00, 0x6f, 0x6c, 0x00, 0x00, 0x00, 0x34, 0x34, 0x34, 0x74, 0x74, 0x74, 0xaf, 0xaf, 0xaf, 0xff, 0xff, 0xff, 0x33, 0xef, 0x90, 0x91, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x00, 0x01, 0x62, 0x4b, 0x47, 0x44, 0x00, 0x88, 0x05, 0x1d, 0x48, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0b, 0x13, 0x00, 0x00, 0x0b, 0x13, 0x01, 0x00, 0x9a, 0x9c, 0x18, 0x00, 0x00, 0x00, 0x07, 0x74, 0x49, 0x4d, 0x45, 0x07, 0xe6, 0x0b, 0x1b, 0x12, 0x3b, 0x1a, 0x22, 0x73, 0xd2, 0xec, 0x00, 0x00, 0x01, 0x6f, 0x49, 0x44, 0x41, 0x54, 0x68, 0xde, 0xed, 0x98, 0xd1, 0x8d, 0x83, 0x30, 0x10, 0x44, 0x91, 0x68, 0xc8, 0xd2, 0x9a, 0x0a, 0x0c, 0x34, 0x10, 0x4c, 0x05, 0xe0, 0xfe, 0x4b, 0x38, 0xd6, 0xc6, 0x66, 0xc9, 0x25, 0x4b, 0x74, 0xe6, 0x7e, 0xa2, 0x19, 0x29, 0x51, 0x60, 0x1e, 0x83, 0x05, 0x4b, 0x24, 0xa6, 0x69, 0x20, 0x08, 0x82, 0x20, 0x08, 0x82, 0xfe, 0x49, 0x2d, 0x6d, 0x2a, 0x5b, 0xbc, 0x61, 0x3e, 0xb0, 0x84, 0xa8, 0x77, 0xd6, 0x65, 0xac, 0xb5, 0x3d, 0xf5, 0x05, 0x53, 0x2c, 0xb1, 0x80, 0x2e, 0x3c, 0xc6, 0x35, 0x63, 0x34, 0xaf, 0x36, 0x4c, 0x3b, 0xa6, 0x58, 0xe2, 0x78, 0x1b, 0x36, 0x2a, 0xac, 0x89, 0xa2, 0xed, 0x17, 0xef, 0x30, 0x17, 0x96, 0x0c, 0x18, 0x22, 0xb5, 0x1b, 0x76, 0x8e, 0xd4, 0x42, 0xd2, 0x72, 0xd2, 0x5a, 0xe9,
  0xf9, 0x0a, 0xcc, 0x89, 0x5a, 0x4c, 0x5a, 0x73, 0xa4, 0x82, 0xbb, 0xb0, 0x84, 0x78, 0x1f, 0x53, 0x31, 0xb9, 0x1d, 0x77, 0x2a, 0xae, 0x47, 0xb1, 0xce, 0x97, 0x20, 0x52, 0x31, 0x99, 0x32, 0xc5, 0x27, 0x55, 0xac, 0xf3, 0x3d, 0x90, 0xd4, 0x2c, 0x03, 0xde, 0x5b, 0x3c, 0x21, 0x66, 0x9f, 0x8c, 0x83, 0xe2, 0xad, 0x42, 0x5d, 0x58, 0x14, 0x27, 0xa2, 0xad, 0x08, 0xb0, 0x7c, 0x2d, 0x5a, 0xeb, 0xfd, 0x74, 0x50, 0x83, 0xf7, 0xae, 0x50, 0xba, 0xe5, 0x1f, 0x5d, 0x0c, 0xd8, 0x88, 0xe5, 0xa0, 0xe6, 0xf4, 0x49, 0x94, 0x6e, 0xf1, 0xd7, 0x97, 0x04, 0x54, 0xdc, 0x85, 0x2e, 0x4f, 0xa4, 0xa4, 0xb6, 0xc4, 0x83, 0x32, 0x9a, 0xd5, 0x90, 0xf5, 0xae, 0x6e, 0x12, 0xcb, 0x3f, 0xd5, 0x5f, 0x03, 0xaa, 0x9f, 0x05, 0x04, 0x20, 0x00, 0x01, 0x08, 0x40, 0x00, 0x02, 0x10, 0x80, 0x00, 0x04, 0x20, 0x00, 0x01, 0x08, 0x40, 0x80, 0x12, 0x40, 0xcd, 0x4d, 0xaf, 0x7d, 0x15, 0x2f, 0x9e, 0x37, 0xbc, 0xfa, 0xa2, 0x3f, 0xb8, 0xa3, 0x84, 0xa9, 0xaa, 0x81, 0x9c, 0x79, 0x1e, 0x24, 0x23, 0xa7, 0x85, 0x54, 0x2b, 0x9e, 0xfb, 0xae, 0x67, 0xa1, 0xba, 0x8c, 0x3b, 0x75, 0x7e, 0x24, 0x3b, 0x3f, 0xc5, 0x7a, 0xd5, 0x27, 0x2e, 0xa7, 0xd2, 0x30, 0x15, 0x92, 0x41, 0x16, 0x92, 0xe3, 0x9b, 0x42, 0x92, 0x93, 0x4b, 0x25, 0xca, 0x27, 0x3d, 0x7a, 0x4f, 0xc5, 0x7a, 0x55, 0xca, 0x1a, 0xd9, 0xbc, 0xd2, 0x85, 0x75, 0x5a, 0xc2, 0xc0, 0x54, 0xbe, 0xab, 0x96, 0xbb, 0xdf, 0x85, 0x2e, 0xad, 0x53, 0xb1, 0x3d, 0xb8, 0xae, 0xcf, 0xfb, 0xc9, 0x4e, 0x34, 0xed, 0x87, 0x68, 0xd6, 0xaf, 0x6a, 0xdc, 0xc8, 0xfe, 0x9b, 0x3e, 0xb0, 0xbe, 0x43, 0x3f, 0x45, 0xa0, 0x4f, 0x6b, 0x5c, 0xdc, 0x81, 0xa1, 0x00,
  0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
unsigned int pilotwire_default_len = 523;

/*****************************************\
 *               Variables
\*****************************************/

// pilotwire : configuration
struct {
  bool     window   = false;                                        // flag to enable window open detection
  bool     presence = false;                                        // flag to enable movement detection
  bool     ecowatt  = true;                                         // flag to enable ecowatt management
  uint8_t  type     = UINT8_MAX;                                    // pilotwire or direct connexion
  uint8_t  mode     = PILOTWIRE_MODE_OFF;                           // default mode is off
  uint16_t height   = PILOTWIRE_GRAPH_HEIGHT;                       // default graph height
  uint16_t timeout_initial = PILOTWIRE_PRESENCE_TIMEOUT_INITIAL;    // default presence detection initial timeout
  uint16_t timeout_normal  = PILOTWIRE_PRESENCE_TIMEOUT_NORMAL;     // default presence detection normal timeout
  float    arr_temp[PILOTWIRE_TARGET_MAX];                          // array of target temperatures (according to status)
} pilotwire_config;

// pilotwire : general status
struct {
  bool    night_mode    = false;                            // flag to indicate night mode
  bool    json_update   = false;                            // flag to publish a json update
  float   temp_json     = PILOTWIRE_TEMP_DEFAULT_NORMAL;    // last published temperature
  float   temp_current  = NAN;                              // current temperature
  float   temp_target   = NAN;                              // target temperature
} pilotwire_status;

// pilotwire : open window detection
struct {
  bool    opened   = false;                                 // open window active
  float   low_temp = NAN;                                   // lower temperature during open window phase
  uint8_t period   = 0;                                     // period for temperature measurement (sec)
  uint8_t idx_temp = 0;                                     // current index of temperature array
  float   arr_temp[PILOTWIRE_WINDOW_SAMPLE_NBR];            // array of values to detect open window
} pilotwire_window;

// pilotwire : movement detection
struct {
  uint32_t time_last = UINT32_MAX;                          // timestamp of last detection
  uint32_t time_over = UINT32_MAX;                          // timestamp of end of active detection
} pilotwire_presence;

// pilotwire : graph data
struct {
  float    temp_min = NAN;                                                // graph minimum temperature
  float    temp_max = NAN;                                                // graph maximum temperature
  uint32_t index[PILOTWIRE_PERIOD_MAX];                                   // current graph index for data
  uint32_t counter[PILOTWIRE_PERIOD_MAX];                                 // graph update counter
  float    temp[PILOTWIRE_PERIOD_MAX];                                    // graph current temperature
  float    target[PILOTWIRE_PERIOD_MAX];                                  // graph current temperature
  uint8_t  device[PILOTWIRE_PERIOD_MAX];                                  // graph periods device state

  short    arr_last[PILOTWIRE_ARRAY_MAX];                                 // last index of array
  short    arr_temp[PILOTWIRE_ARRAY_MAX][PILOTWIRE_GRAPH_SAMPLE];         // graph temperature array (value x 10)
  short    arr_target[PILOTWIRE_ARRAY_MAX][PILOTWIRE_GRAPH_SAMPLE];       // graph target temperature array (value x 10)
  uint8_t  arr_state[PILOTWIRE_ARRAY_MAX][PILOTWIRE_GRAPH_SAMPLE];        // graph command state array
} pilotwire_graph;

/**************************************************\
 *                  Commands
\**************************************************/

// offload help
void CmndPilotwireHelp ()
{
  uint8_t index;
  char    str_text[32];

  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Pilotwire commands :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - pw_type = device type"));
  for (index = 0; index < PILOTWIRE_DEVICE_MAX; index++)
  {
    GetTextIndexed (str_text, sizeof (str_text), index, kPilotwireDevice);
    AddLog (LOG_LEVEL_INFO, PSTR ("     %u - %s"), index, str_text);
  }

  AddLog (LOG_LEVEL_INFO, PSTR (" - pw_mode = heater running mode"));
  for (index = 0; index < PILOTWIRE_MODE_MAX; index++)
  {
    GetTextIndexed (str_text, sizeof (str_text), index, kPilotwireModeLabel);
    AddLog (LOG_LEVEL_INFO, PSTR ("     %u - %s"), index, str_text);
  }

  AddLog (LOG_LEVEL_INFO, PSTR (" - pw_target = target temperature (°C)"));

  AddLog (LOG_LEVEL_INFO, PSTR (" - pw_ecowatt  = enable ecowatt management (0/1)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - pw_window   = enable open window mode (0/1)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - pw_presence = enable presence detection (0/1)"));

  AddLog (LOG_LEVEL_INFO, PSTR (" - pw_low      = lowest temperature (°C)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - pw_high     = highest temperature (°C)"));

  AddLog (LOG_LEVEL_INFO, PSTR (" - pw_comfort  = default comfort temperature (°C)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - pw_nofrost  = no frost temperature (delta with comfort if negative)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - pw_eco      = eco temperature (delta with comfort if negative)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - pw_night    = night temperature (delta with comfort if negative)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - pw_eco2     = ecowatt level 2 temperature (delta with comfort if negative)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - pw_eco3     = ecowatt level 3 temperature (delta with comfort if negative)"));

  AddLog (LOG_LEVEL_INFO, PSTR (" - pw_initial  = presence detection timeout when changing running mode (mn)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - pw_normal   = presence detection timeout after detection (mn)"));

  ResponseCmndDone();
}

// set heater type and save
void CmndPilotwireType ()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetDeviceType ((uint8_t)XdrvMailbox.payload, true);
  ResponseCmndNumber (pilotwire_config.type);
}

// set running mode
void CmndPilotwireMode ()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetMode ((uint8_t)XdrvMailbox.payload);
  ResponseCmndNumber (pilotwire_config.mode);
}

// set target temperature without saving
void CmndPilotwireTarget ()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetTargetTemperature (PILOTWIRE_TARGET_COMFORT, atof (XdrvMailbox.data), false);
  ResponseCmndFloat (pilotwire_config.arr_temp[PILOTWIRE_TARGET_COMFORT], 1);
}

// set 
void CmndPilotwireEcowatt ()
{
  if (XdrvMailbox.data_len > 0) PilotwireEcowattEnable ((XdrvMailbox.payload != 0), true);
  ResponseCmndNumber (pilotwire_config.ecowatt);
}

void CmndPilotwireWindow ()
{
  if (XdrvMailbox.data_len > 0) PilotwireWindowEnable ((XdrvMailbox.payload != 0), true);
  ResponseCmndNumber (pilotwire_config.window);
}

void CmndPilotwirePresence ()
{
  if (XdrvMailbox.data_len > 0) PilotwirePresenceEnable ((XdrvMailbox.payload != 0), true);
  ResponseCmndNumber (pilotwire_config.presence);
}

void CmndPilotwireTargetLow ()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetTargetTemperature (PILOTWIRE_TARGET_LOW, atof (XdrvMailbox.data), true);
  ResponseCmndFloat (pilotwire_config.arr_temp[PILOTWIRE_TARGET_LOW], 1);
}

void CmndPilotwireTargetHigh ()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetTargetTemperature (PILOTWIRE_TARGET_HIGH, atof (XdrvMailbox.data), true);
  ResponseCmndFloat (pilotwire_config.arr_temp[PILOTWIRE_TARGET_HIGH], 1);
}

void CmndPilotwireTargetNofrost ()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetTargetTemperature (PILOTWIRE_TARGET_NOFROST, atof (XdrvMailbox.data), true);
  ResponseCmndFloat (pilotwire_config.arr_temp[PILOTWIRE_TARGET_NOFROST], 1);
}

void CmndPilotwireTargetComfort ()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetTargetTemperature (PILOTWIRE_TARGET_COMFORT, atof (XdrvMailbox.data), true);
  ResponseCmndFloat (pilotwire_config.arr_temp[PILOTWIRE_TARGET_COMFORT], 1);
}

void CmndPilotwireTargetEco ()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetTargetTemperature (PILOTWIRE_TARGET_ECO, atof (XdrvMailbox.data), true);
  ResponseCmndFloat (pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECO], 1);
}

void CmndPilotwireTargetNight ()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetTargetTemperature (PILOTWIRE_TARGET_NIGHT, atof (XdrvMailbox.data), true);
  ResponseCmndFloat (pilotwire_config.arr_temp[PILOTWIRE_TARGET_NIGHT], 1);
}

void CmndPilotwireTargetEcowatt2 ()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetTargetTemperature (PILOTWIRE_TARGET_ECOWATT2, atof (XdrvMailbox.data), true);
  ResponseCmndFloat (pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT2], 1);
}

void CmndPilotwireTargetEcowatt3 ()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetTargetTemperature (PILOTWIRE_TARGET_ECOWATT3, atof (XdrvMailbox.data), true);
  ResponseCmndFloat (pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT3], 1);
}

void CmndPilotwireTimeoutInitial ()
{
  if (XdrvMailbox.payload > 0)
  {
    pilotwire_config.timeout_initial = XdrvMailbox.payload;
    PilotwireSaveConfig ();
  }
  ResponseCmndNumber (pilotwire_config.timeout_initial);
}

void CmndPilotwireTimeoutNormal ()
{
  if (XdrvMailbox.payload > 0)
  {
    pilotwire_config.timeout_normal = XdrvMailbox.payload;
    PilotwireSaveConfig ();
  }
  ResponseCmndNumber (pilotwire_config.timeout_normal);
}

/******************************************************\
 *                  Main functions
\******************************************************/

// get pilotwire device state from relay state
bool PilotwireGetHeaterState ()
{
  bool status;

  // read relay state and invert if in pilotwire mode
  status = bitRead (TasmotaGlobal.power, 0);
  if (pilotwire_config.type == PILOTWIRE_DEVICE_PILOTWIRE) status = !status;

  return status;
}

// set relays state
bool PilotwireGetHeatingStatus ()
{
  bool status = PilotwireGetHeaterState ();

  // if device is having a power sensor, declare no heating if less than minimum drawn power
  if (offload_status.device_sensor && (offload_status.device_pinst < OFFLOAD_POWER_MINIMUM)) status = false;

  return status;
}

// set relays state
void PilotwireSetHeaterState (const bool new_state)
{
  // if relay needs to be toggled
  if (new_state != PilotwireGetHeaterState ())
  {
    // disable presence detection to avoid false positive
    SensorPresenceSuspendDetection (PILOTWIRE_PRESENCE_IGNORE);

    // toggle relay and ask for JSON update
    ExecuteCommandPower (1, POWER_TOGGLE,  SRC_MAX);
  
    // ask for JSON update
    pilotwire_status.json_update = true;
  }
}

// read target temperature according to given mode
float PilotwireConvertTargetTemperature (const uint8_t target)
{
  float temperature = NAN;

  // check limits
  if (target >= PILOTWIRE_TARGET_MAX) return temperature;

  // if target temperature is positive, use it
  if (pilotwire_config.arr_temp[target] > 0) temperature = pilotwire_config.arr_temp[target];

  // else, if negative, substract it from comfort target
  else temperature = pilotwire_config.arr_temp[PILOTWIRE_TARGET_COMFORT] + pilotwire_config.arr_temp[target];

  // validate boundaries
  if (temperature < PILOTWIRE_TEMP_LIMIT_MIN) temperature = PILOTWIRE_TEMP_LIMIT_MIN;
  else if (temperature > PILOTWIRE_TEMP_LIMIT_MAX) temperature = PILOTWIRE_TEMP_LIMIT_MAX;

  return temperature;
}

// get target temperature of specific mode
float PilotwireGetModeTemperature (const uint8_t mode)
{
  float temperature;
  
  // set initial temperature according to running mode
  switch (mode)
  {
    case PILOTWIRE_MODE_ECO:
      temperature = PilotwireConvertTargetTemperature (PILOTWIRE_TARGET_ECO);
      break;
    case PILOTWIRE_MODE_NIGHT:
      temperature = PilotwireConvertTargetTemperature (PILOTWIRE_TARGET_NIGHT);
      break;
    case PILOTWIRE_MODE_COMFORT:
      temperature = PilotwireConvertTargetTemperature (PILOTWIRE_TARGET_COMFORT);
      break;
    case PILOTWIRE_MODE_FORCED:
      temperature = PILOTWIRE_TEMP_LIMIT_MAX;
      break;
    default:
      temperature = PilotwireConvertTargetTemperature (PILOTWIRE_TARGET_NOFROST);
      break;
  }

  return temperature;
}

float PilotwireGetCurrentTargetTemperature ()
{
  bool    presence;
  uint8_t level;
  float   temperature;
  
  // set initial temperature according to running mode
  temperature = PilotwireGetModeTemperature (pilotwire_config.mode);

  // if main supply is offloaded, target is no frost
  if (OffloadIsOffloaded ()) temperature = min (temperature, PilotwireConvertTargetTemperature (PILOTWIRE_TARGET_NOFROST));

  // if window opened, target is no frost
  if (pilotwire_window.opened) temperature = min (temperature, PilotwireConvertTargetTemperature (PILOTWIRE_TARGET_NOFROST));

  // if status is not forced, check night mode, presence and ecowatt
  if (pilotwire_config.mode != PILOTWIRE_MODE_FORCED)
  {
    // if night mode, update target
    if (pilotwire_status.night_mode) temperature = min (temperature, PilotwireConvertTargetTemperature (PILOTWIRE_TARGET_NIGHT));

    // if presence is not detected : target is Eco
    presence = PilotwirePresenceIsActive ();
    if (!presence) temperature = min (temperature, PilotwireConvertTargetTemperature (PILOTWIRE_TARGET_ECO));

    // if ecowatt level is detected : 2 (warning) or 3 (critical)
    level = PilotwireEcowattGetLevel ();
    if (level == ECOWATT_LEVEL_WARNING) temperature = min (temperature, PilotwireConvertTargetTemperature (PILOTWIRE_TARGET_ECOWATT2));
    else if (level == ECOWATT_LEVEL_POWERCUT) temperature = min (temperature, PilotwireConvertTargetTemperature (PILOTWIRE_TARGET_ECOWATT3));
  }

  return temperature;
}

// set different target temperatures
void PilotwireSetTargetTemperature (const uint8_t type, const float target, const bool to_save)
{
  bool is_valid = false;
  char str_target[8];
  char str_type[24];

  // check parameters
  if (type >= PILOTWIRE_TARGET_MAX) return;
  if (target > PILOTWIRE_TEMP_LIMIT_MAX) return;

  // set temperature
  pilotwire_config.arr_temp[type] = target;

  // log
  ext_snprintf_P (str_target, sizeof (str_target), PSTR ("%01_f"), &target);
  GetTextIndexed (str_type,   sizeof (str_type), type, kPilotwireTargetLabel);
  AddLog (LOG_LEVEL_INFO, PSTR ("PIL: %s temperature set to %s"), str_type, str_target);

  // if needed, save configuration
  if (to_save) PilotwireSaveConfig ();

  // ask for JSON update
  pilotwire_status.json_update = true;
}

// set heater configuration mode
void PilotwireSetDeviceType (const uint8_t type, const bool to_save)
{
  char str_type[16];

  // check parameters
  if (type >= PILOTWIRE_DEVICE_MAX) return;

  // set type
  pilotwire_config.type = type;

  // log action
  GetTextIndexed (str_type, sizeof (str_type), pilotwire_config.type, kPilotwireDevice);
  AddLog (LOG_LEVEL_INFO, PSTR ("PIL: Device set to %s"), str_type);

  // if needed, save
  if (to_save) PilotwireSaveConfig ();

  // ask for JSON update
  pilotwire_status.json_update = true;
}

// set heater target mode
void PilotwireSetMode (const uint8_t mode)
{
  char str_mode[16];

  // check parameter
  if (mode >= PILOTWIRE_MODE_MAX) return;

  // handle night mode
  if (mode == PILOTWIRE_MODE_NIGHT)
  {
    // set to COMFORT with night mode enabled
    pilotwire_config.mode = PILOTWIRE_MODE_COMFORT;
    pilotwire_status.night_mode = true;
  }

  // set modes other than night mode
  else
  {
    // set to new mode with night mode disabled
    pilotwire_config.mode = mode;
    pilotwire_status.night_mode = false;

    // if heater is not off,
    if (mode != PILOTWIRE_MODE_OFF)
    {
      // reset presence detection
      if (pilotwire_config.presence) PilotwirePresenceDeclareDetection (PILOTWIRE_PRESENCE_INITIAL);

      // if window detection is active, reset open window detection
      if (pilotwire_config.window) PilotwireWindowResetDetection ();
    }
  }

  // ask for JSON update
  pilotwire_status.json_update = true;

  // log change
  GetTextIndexed (str_mode, sizeof (str_mode), mode, kPilotwireModeLabel);
  AddLog (LOG_LEVEL_INFO, PSTR ("PIL: Switch status to %s"), str_mode);
}

// set current target temperature
void PilotwireSetCurrentTarget (float target)
{
  char str_value[8];

  // if target is within range
  if ((target >= pilotwire_config.arr_temp[PILOTWIRE_TARGET_LOW]) && (target <= pilotwire_config.arr_temp[PILOTWIRE_TARGET_HIGH]))
  {
    // set new comfort target temperature
    pilotwire_config.arr_temp[PILOTWIRE_TARGET_COMFORT] = target;

    // ask for JSON update
    pilotwire_status.json_update = true;

    // log new target
    ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%01_f"), &target);
    AddLog (LOG_LEVEL_INFO, PSTR ("PIL: Set target temp. to %s"), str_value);
  }
}

// update temperature
void PilotwireUpdateTemperature ()
{
  // update current temperature
  pilotwire_status.temp_current = SensorTemperatureRead ();

  // update JSON if temperature has changed of 0.2°C minimum
  if (!isnan (pilotwire_status.temp_current))
  {
    if (abs (pilotwire_status.temp_json - pilotwire_status.temp_current) >= 0.2)
    {
      pilotwire_status.temp_json   = pilotwire_status.temp_current;
      pilotwire_status.json_update = true;
    }
  }
}

// update heater relay state
void PilotwireUpdateHeaterState ()
{
  bool  status, target;
  float temperature, temp_target;

  // get current relay state
  status = PilotwireGetHeaterState ();
  target = status;

  // get target temperature
  temp_target = PilotwireGetCurrentTargetTemperature ();

  // if current temperature not available, consider it as higher possible range (it enable forced mode in case no sensor is available)
  if (isnan (pilotwire_status.temp_current)) temperature = PILOTWIRE_TEMP_DEFAULT_HIGH;
    else temperature = pilotwire_status.temp_current;

  // if temperature is too low, heater should be ON
  if (temperature < (temp_target - PILOTWIRE_TEMP_THRESHOLD)) target = true;

  // else, if temperature is too high, heater should be OFF
  else if (temperature > (temp_target + PILOTWIRE_TEMP_THRESHOLD)) target = false;

  // if relay state has changed, set it
  if (target != status) PilotwireSetHeaterState (target);
}

/***********************************************\
 *                  Ecowatt
\***********************************************/

// set ecowatt management option
void PilotwireEcowattEnable (const bool state, const bool to_save)
{
  // set main flag
  pilotwire_config.ecowatt = state;

  // if needed, save
  if (to_save) PilotwireSaveConfig ();

  // ask for JSON update
  pilotwire_status.json_update = true;
}

// set ecowatt management option
uint8_t PilotwireEcowattGetLevel ()
{
  uint8_t level = ECOWATT_LEVEL_NORMAL;

  // if ecowatt management is enabled
  if (pilotwire_config.ecowatt) level = EcowattGetCurrentLevel ();

  return level;
}

/***********************************************\
 *              Window detection
\***********************************************/

// set window detection option
void PilotwireWindowEnable (const bool state, const bool to_save)
{
  // set main flag
  pilotwire_config.window = state;

  // reset window detection algo
  PilotwireWindowResetDetection ();

  // if needed, save
  if (to_save) PilotwireSaveConfig ();

  // ask for JSON update
  pilotwire_status.json_update = true;
}

// reset window detection data
void PilotwireWindowResetDetection ()
{
  uint8_t index;

  // declare window closed
  pilotwire_window.opened = false;
  pilotwire_window.low_temp = NAN;

  // reset detection temperatures
  for (index = 0; index < PILOTWIRE_WINDOW_SAMPLE_NBR; index ++) pilotwire_window.arr_temp[index] = NAN;
}

// update opened window detection data
void PilotwireWindowUpdateDetection ()
{
  uint8_t index, idx_array;
  float   delta = 0;

  // if window detection is not enabled, exit
  if (!pilotwire_config.window) return;

  // if window considered closed, do open detection
  if (!pilotwire_window.opened)
  {
    // if temperature is available, update temperature detection array
    if (!isnan (pilotwire_status.temp_current))
    {
      // if period reached
      if (pilotwire_window.period == 0)
      {
        // record temperature and increment index
        pilotwire_window.arr_temp[pilotwire_window.idx_temp] = pilotwire_status.temp_current;
        pilotwire_window.idx_temp++;
        pilotwire_window.idx_temp = pilotwire_window.idx_temp % PILOTWIRE_WINDOW_SAMPLE_NBR;
      }

      // increment period counter
      pilotwire_window.period++;
      pilotwire_window.period = pilotwire_window.period % PILOTWIRE_WINDOW_OPEN_PERIOD;
    }

    // loop thru last measured temperatures
    for (index = 0; index < PILOTWIRE_WINDOW_SAMPLE_NBR; index ++)
    {
      // calculate temperature array in reverse order
      idx_array = (PILOTWIRE_WINDOW_SAMPLE_NBR + pilotwire_window.idx_temp - index - 1) % PILOTWIRE_WINDOW_SAMPLE_NBR;

      // calculate temperature delta 
      if (!isnan (pilotwire_window.arr_temp[idx_array])) delta = max (delta, pilotwire_window.arr_temp[idx_array] - pilotwire_status.temp_current);

      // if temperature drop detected, stop analysis
      if (delta >= PILOTWIRE_WINDOW_OPEN_DROP) break;
    }

    // if temperature drop detected, window is detected as opended
    if (delta >= PILOTWIRE_WINDOW_OPEN_DROP)
    {
      // declare window as opened
      pilotwire_window.opened = true;
      pilotwire_window.low_temp = pilotwire_status.temp_current;

      // ask for JSON update
      pilotwire_status.json_update = true;
    }
  }

  // else, window detected as opened, try to detect closure
  else
  {
    // update lower temperature
    pilotwire_window.low_temp = min (pilotwire_window.low_temp, pilotwire_status.temp_current);

    // if current temperature has increased enought, window is closed
    if (pilotwire_status.temp_current - pilotwire_window.low_temp >= PILOTWIRE_WINDOW_CLOSE_INCREASE)
    {
      // reset window detection data
      PilotwireWindowResetDetection ();

      // ask for JSON update
      pilotwire_status.json_update = true;
    }
  }
}

// get target temperature according to window detection
bool PilotwireWindowGetStatus ()
{
  return pilotwire_window.opened;
}

/***********************************************\
 *             Presence detection
\***********************************************/

// set presence detection option
void PilotwirePresenceEnable (const bool state, const bool to_save)
{
  // set main flag
  pilotwire_config.presence = state;

  // reset presence detection algo
  if (pilotwire_config.presence) PilotwirePresenceDeclareDetection (PILOTWIRE_PRESENCE_INITIAL);
    else PilotwirePresenceDeclareDetection (PILOTWIRE_PRESENCE_DISABLED);

  // if needed, save
  if (to_save) PilotwireSaveConfig ();

  // ask for JSON update
  pilotwire_status.json_update = true;
}

void PilotwirePresenceDeclareDetection (const uint8_t type)
{
  // switch according to detection type
  switch (type)
  {
    case PILOTWIRE_PRESENCE_INITIAL:
      SensorPresenceResetDetection ();
      pilotwire_presence.time_last = LocalTime ();
      pilotwire_presence.time_over = pilotwire_presence.time_last + (uint32_t)pilotwire_config.timeout_initial * 60;
      break;

    case PILOTWIRE_PRESENCE_NORMAL:
      SensorPresenceResetDetection ();
      pilotwire_presence.time_last = LocalTime ();
      pilotwire_presence.time_over = pilotwire_presence.time_last + (uint32_t)pilotwire_config.timeout_normal * 60;
      break;
 
     case PILOTWIRE_PRESENCE_DISABLED:
      pilotwire_presence.time_last = UINT32_MAX;
      pilotwire_presence.time_over = UINT32_MAX;
      break;
  }
}

void PilotwirePresenceUpdateDetection ()
{
  // if presence detection is active and presence currently detected, set detection
  if (pilotwire_config.presence && SensorPresenceDetected (PILOTWIRE_PRESENCE_TIMEOUT)) PilotwirePresenceDeclareDetection (PILOTWIRE_PRESENCE_NORMAL);
}

bool PilotwirePresenceIsActive ()
{
  bool presence = true;

  // if enabled, check presence according to timeout
  if (pilotwire_config.presence) presence = (LocalTime () < pilotwire_presence.time_over);

  return presence;
}

/**************************************************\
 *                  Configuration
\**************************************************/

void PilotwireLoadConfig () 
{
  bool window, presence, ecowatt;

#ifdef USE_UFILESYS

  // mode and flags
  pilotwire_config.type = (uint8_t)UfsCfgLoadKeyInt (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_TYPE, 0);
  window   = (bool)UfsCfgLoadKeyInt (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_WINDOW,   0);
  presence = (bool)UfsCfgLoadKeyInt (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_PRESENCE, 0);
  ecowatt  = (bool)UfsCfgLoadKeyInt (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_ECOWATT,  1);

  // presence timeout
  pilotwire_config.timeout_initial = UfsCfgLoadKeyInt (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_INITIAL, PILOTWIRE_PRESENCE_TIMEOUT_INITIAL);
  pilotwire_config.timeout_normal  = UfsCfgLoadKeyInt (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_NORMAL, PILOTWIRE_PRESENCE_TIMEOUT_NORMAL);

  // target temperature
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_LOW]      = UfsCfgLoadKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_LOW,      PILOTWIRE_TEMP_DEFAULT_LOW);
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_HIGH]     = UfsCfgLoadKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_HIGH,     PILOTWIRE_TEMP_DEFAULT_HIGH);
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_COMFORT]  = UfsCfgLoadKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_COMFORT,  PILOTWIRE_TEMP_DEFAULT_NORMAL);
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_NIGHT]    = UfsCfgLoadKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_NIGHT,    0);
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECO]      = UfsCfgLoadKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_ECO,      0);
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_NOFROST]  = UfsCfgLoadKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_NOFROST,  0);
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT2] = UfsCfgLoadKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_ECOWATT_N2, PILOTWIRE_TEMP_DEFAULT_LOW);
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT3] = UfsCfgLoadKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_ECOWATT_N3, PILOTWIRE_TEMP_DEFAULT_LOW);

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("PIL: Config from LittleFS"));

#else       // No LittleFS

  // mode and flags
  pilotwire_config.type = Settings->knx_CB_param[0];
  window   = (bool)Settings->knx_CB_param[1];
  presence = (bool)Settings->knx_CB_param[2];
  ecowatt  = (bool)Settings->knx_CB_param[3];

  // presence timeout
  pilotwire_config.timeout_initial = Settings->knx_CB_addr[0];
  pilotwire_config.timeout_normal  = Settings->knx_CB_addr[1];

  // target temperature
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_LOW]      = (float)Settings->knx_CB_addr[2] / 10;
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_HIGH]     = (float)Settings->knx_CB_addr[3] / 10;
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_COMFORT]  = (float)Settings->knx_CB_addr[4] / 10;
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_NIGHT]    = ((float)Settings->knx_CB_addr[5] - 1000) / 10;
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECO]      = ((float)Settings->knx_CB_addr[6] - 1000) / 10;
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_NOFROST]  = ((float)Settings->knx_CB_addr[7] - 1000) / 10;
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT2] = ((float)Settings->knx_CB_addr[8] - 1000) / 10;
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT3] = ((float)Settings->knx_CB_addr[9] - 1000) / 10;
  

   // log
  AddLog (LOG_LEVEL_INFO, PSTR ("PIL: Config from Settings"));

# endif     // USE_UFILESYS

  // avoid values out of range
  if (pilotwire_config.type >= PILOTWIRE_DEVICE_MAX) pilotwire_config.type = PILOTWIRE_DEVICE_PILOTWIRE;
  if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_LOW] < PILOTWIRE_TEMP_LIMIT_MIN) pilotwire_config.arr_temp[PILOTWIRE_TARGET_LOW] = PILOTWIRE_TEMP_DEFAULT_LOW;
    else if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_LOW] > PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_temp[PILOTWIRE_TARGET_LOW] = PILOTWIRE_TEMP_DEFAULT_LOW;
  if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_HIGH] < PILOTWIRE_TEMP_LIMIT_MIN) pilotwire_config.arr_temp[PILOTWIRE_TARGET_HIGH] = PILOTWIRE_TEMP_DEFAULT_HIGH;
    else if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_HIGH] > PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_temp[PILOTWIRE_TARGET_HIGH] = PILOTWIRE_TEMP_DEFAULT_HIGH;
  if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_COMFORT] < PILOTWIRE_TEMP_LIMIT_MIN) pilotwire_config.arr_temp[PILOTWIRE_TARGET_COMFORT] = PILOTWIRE_TEMP_DEFAULT_NORMAL;
    else if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_COMFORT] > PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_temp[PILOTWIRE_TARGET_COMFORT] = PILOTWIRE_TEMP_DEFAULT_NORMAL;
  if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_NIGHT] < -PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_temp[PILOTWIRE_TARGET_NIGHT] = PILOTWIRE_TEMP_DEFAULT_NIGHT;
    else if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_NIGHT] >  PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_temp[PILOTWIRE_TARGET_NIGHT] = PILOTWIRE_TEMP_DEFAULT_NIGHT;
  if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECO] < -PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECO] = PILOTWIRE_TEMP_DEFAULT_ECO;
    else if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECO] >  PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECO] = PILOTWIRE_TEMP_DEFAULT_ECO;
  if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_NOFROST] < -PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_temp[PILOTWIRE_TARGET_NOFROST] = PILOTWIRE_TEMP_DEFAULT_NOFROST;
    else if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_NOFROST] >  PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_temp[PILOTWIRE_TARGET_NOFROST] = PILOTWIRE_TEMP_DEFAULT_NOFROST;
  if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT2] < -PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT2] = PILOTWIRE_TEMP_DEFAULT_ECO;
    else if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT2] >  PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT2] = PILOTWIRE_TEMP_DEFAULT_ECO;
  if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT3] < -PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT3] = PILOTWIRE_TEMP_DEFAULT_NOFROST;
    else if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT3] >  PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT3] = PILOTWIRE_TEMP_DEFAULT_NOFROST;

  // enable options
  PilotwireEcowattEnable (ecowatt, false);
  PilotwireWindowEnable (window, false);
  PilotwirePresenceEnable (presence, false);
}

void PilotwireSaveConfig () 
{
#ifdef USE_UFILESYS

  // mode and flags
  UfsCfgSaveKeyInt (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_TYPE, (int)pilotwire_config.type, true);
  UfsCfgSaveKeyInt (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_WINDOW, (int)pilotwire_config.window, false);
  UfsCfgSaveKeyInt (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_PRESENCE, (int)pilotwire_config.presence, false);
  UfsCfgSaveKeyInt (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_ECOWATT, (int)pilotwire_config.ecowatt, false);

  // presence timeout
  UfsCfgSaveKeyInt (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_INITIAL, (int)pilotwire_config.timeout_initial, false);
  UfsCfgSaveKeyInt (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_NORMAL, (int)pilotwire_config.timeout_normal, false);

  // target temperature
  UfsCfgSaveKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_LOW, pilotwire_config.arr_temp[PILOTWIRE_TARGET_LOW], false);
  UfsCfgSaveKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_HIGH, pilotwire_config.arr_temp[PILOTWIRE_TARGET_HIGH], false);
  UfsCfgSaveKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_COMFORT, pilotwire_config.arr_temp[PILOTWIRE_TARGET_COMFORT], false);
  UfsCfgSaveKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_NIGHT, pilotwire_config.arr_temp[PILOTWIRE_TARGET_NIGHT], false);
  UfsCfgSaveKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_ECO, pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECO], false);
  UfsCfgSaveKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_NOFROST, pilotwire_config.arr_temp[PILOTWIRE_TARGET_NOFROST], false);
  UfsCfgSaveKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_ECOWATT_N2, pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT2], false);
  UfsCfgSaveKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_ECOWATT_N3, pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT3], false);

# else       // No LittleFS

  // mode and flags
  Settings->knx_CB_param[0] = pilotwire_config.type;
  Settings->knx_CB_param[1] = (uint8_t)pilotwire_config.window;
  Settings->knx_CB_param[2] = (uint8_t)pilotwire_config.presence;
  Settings->knx_CB_param[3] = (uint8_t)pilotwire_config.ecowatt;

  // presence timeout
  Settings->knx_CB_addr[0] = pilotwire_config.timeout_initial;
  Settings->knx_CB_addr[1] = pilotwire_config.timeout_normal;

  // target temperature
  Settings->knx_CB_addr[2] = (uint16_t)(10 * pilotwire_config.arr_temp[PILOTWIRE_TARGET_LOW]);
  Settings->knx_CB_addr[3] = (uint16_t)(10 * pilotwire_config.arr_temp[PILOTWIRE_TARGET_HIGH]);
  Settings->knx_CB_addr[4] = (uint16_t)(10 * pilotwire_config.arr_temp[PILOTWIRE_TARGET_COMFORT]);
  Settings->knx_CB_addr[5] = (uint16_t)(1000 + (10 * pilotwire_config.arr_temp[PILOTWIRE_TARGET_NIGHT]));
  Settings->knx_CB_addr[6] = (uint16_t)(1000 + (10 * pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECO]));
  Settings->knx_CB_addr[7] = (uint16_t)(1000 + (10 * pilotwire_config.arr_temp[PILOTWIRE_TARGET_NOFROST]));
  Settings->knx_CB_addr[8] = (uint16_t)(1000 + (10 * pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT2]));
  Settings->knx_CB_addr[9] = (uint16_t)(1000 + (10 * pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT3]));

# endif     // USE_UFILESYS
}

/******************************************************\
 *                  Historical files
\******************************************************/

#ifdef USE_UFILESYS

// Get historisation period literal date
bool PilotwireHistoWeekDate (const uint8_t histo, char* pstr_text, const size_t size_text)
{
  uint32_t start_time, delta_time;
  TIME_T   start_dst, stop_dst;

  // check parameters
  if (pstr_text == nullptr) return false;
  if (size_text < 16) return false;

  // init
  strcpy (pstr_text, "");

  // calculate week reference date
  start_time = LocalTime () - 604800 * (uint32_t)histo;
  BreakTime (start_time, start_dst);
  start_dst.hour   = 0;
  start_dst.minute = 0;
  start_dst.second = 0;

  // calculate date of first day of the week
  start_time = MakeTime (start_dst);
  delta_time = ((start_dst.day_of_week + 7) - 2) % 7;
  start_time -= delta_time * 86400;
  BreakTime (start_time, start_dst);

  // calculate date of last day of the week
  start_time += 518400;
  BreakTime (start_time, stop_dst);

  // generate string
  sprintf_P (pstr_text, PSTR ("%02u/%02u - %02u/%02u"), start_dst.day_of_month, start_dst.month, stop_dst.day_of_month, stop_dst.month);

  return (strlen (pstr_text) > 0);
}

// clean and rotate histo files
void PilotwireHistoRotate ()
{
  TIME_T time_dst;

  // extract data from current time
  BreakTime (LocalTime (), time_dst);

  // if we are monday, rotate weekly files
  if (time_dst.day_of_week == 2) UfsFileRotate (D_PILOTWIRE_FILE_WEEK, 0, PILOTWIRE_HISTO_WEEK_MAX);
}

// save current data to histo file
void PilotwireHistoSaveData (uint32_t period)
{
  uint32_t start_time, current_time, day_of_week, delta, index, slot;
  TIME_T   time_dst;
  char     str_filename[UFS_FILENAME_SIZE];
  char     str_value[8];
  char     str_line[64];
  File     file;

  // validate parameters
  if (ufs_type == 0) return; 
  if (period != PILOTWIRE_PERIOD_WEEK) return;

  // get current time
  current_time = LocalTime ();
  BreakTime (current_time, time_dst);

  // calculate delta for start day
  if (time_dst.day_of_week == 1) day_of_week = 8; else day_of_week = time_dst.day_of_week;
  delta = day_of_week - 2;

  // calculate start of current week
  time_dst.second = 1;
  time_dst.minute = 0;
  time_dst.hour   = 0;  
  start_time = MakeTime (time_dst) - delta * 86400;

  // calculate index from delta time
  delta = current_time - start_time;
  slot  = 604800 / PILOTWIRE_GRAPH_SAMPLE;
  index = delta / slot;
  if (index >= PILOTWIRE_GRAPH_SAMPLE) index = PILOTWIRE_GRAPH_SAMPLE - 1;

  // generate current week filename
  sprintf_P (str_filename, D_PILOTWIRE_FILE_WEEK, 0);

  // generate current line
  BreakTime (current_time, time_dst);
  sprintf_P (str_line, PSTR ("%u;%02u/%02u;%02u:%02u"), index, time_dst.day_of_month, time_dst.month, time_dst.hour, time_dst.minute);

  // append actual temperature
  if (!isnan (pilotwire_graph.temp[period])) ext_snprintf_P (str_value, sizeof (str_value), PSTR (";%1_f"), &pilotwire_graph.temp[period]);
    else strcpy (str_value, ";");
  strlcat (str_line, str_value, sizeof (str_line));

  // append actual mode target temperature
  if (!isnan (pilotwire_graph.target[period])) ext_snprintf_P (str_value, sizeof (str_value), PSTR (";%1_f"), &pilotwire_graph.target[period]);
    else strcpy (str_value, ";");
  strlcat (str_line, str_value, sizeof (str_line));

  // append heater status
  sprintf_P (str_value, PSTR (";%d"), pilotwire_graph.device[period]);
  strlcat (str_line, str_value, sizeof (str_line));

  // end of line
  strlcat (str_line, "\n", sizeof (str_line));

  // if file doesn't exist, add header
  if (!ffsp->exists (str_filename))
  {
    file = ffsp->open (str_filename, "w"); 
    file.print ("Idx;Date;Time;Temp;Target;Status\n");
  }
  else file = ffsp->open (str_filename, "a"); 

  // write and close
  file.print (str_line);
  file.close ();
}

// Rotate files at day change
void PilotwireRotateAtMidnight ()
{
  // if filesystem is available, save data to history file
  if (ufs_type) PilotwireHistoRotate ();
}

# endif     // USE_UFILESYS

void PilotwireUpdateGraph (uint32_t period)
{
  short    result;
  uint32_t index;
  float    value;

  // check period validity
  if (period >= PILOTWIRE_PERIOD_MAX) return;

  // get graph index
  index = pilotwire_graph.index[period] % PILOTWIRE_GRAPH_SAMPLE;

  // if live update
  if (period == PILOTWIRE_PERIOD_LIVE)
  {
    // if temperature has been mesured, update current graph index
    if (!isnan (pilotwire_status.temp_current))
    {
      // set current temperature
      value = pilotwire_graph.temp[period];
      if (isnan (value)) result = SHRT_MAX; else result = (short)(value * 10);
      pilotwire_graph.arr_temp[PILOTWIRE_ARRAY_LIVE][index] = result;

      // set target temperature
      value = pilotwire_graph.target[period];
      if (isnan (value)) result = SHRT_MAX; else result = (short)(value * 10);
      pilotwire_graph.arr_target[PILOTWIRE_ARRAY_LIVE][index] = result;

      // set pilotwire state
      pilotwire_graph.arr_state[PILOTWIRE_ARRAY_LIVE][index] = pilotwire_graph.device[period];
    }
  }

  // else save data to history file
#ifdef USE_UFILESYS
  else if (period == PILOTWIRE_PERIOD_WEEK) PilotwireHistoSaveData (period);
#endif    // USE_UFILESYS

  // reset period data and increase index
  pilotwire_graph.temp[period]   = NAN;
  pilotwire_graph.target[period] = NAN;
  pilotwire_graph.device[period] = UINT8_MAX;
  pilotwire_graph.index[period]  = (index + 1) % PILOTWIRE_GRAPH_SAMPLE;
}

/***********************************************\
 *                  Callback
\***********************************************/

// Called by Offload driver 
//  - if coming from timer : relay ON = switch to night mode, relay OFF = switch back to normal mode
//  - if coming from anything else than SRC_MAX : ignore
bool PilotwireSetDevicePower ()
{
  bool     result = true;           // command handled by default
  uint32_t target;

  // if command is from a timer, handle night mode
  if (XdrvMailbox.payload == SRC_TIMER)
  {
    // set night mode according to POWER switch
    if (XdrvMailbox.index == POWER_OFF) pilotwire_status.night_mode = false;
    else if (XdrvMailbox.index == POWER_ON) pilotwire_status.night_mode = true;
  }

  // else if command is not from the module, log and ignore it
  else if (XdrvMailbox.payload != SRC_MAX) AddLog (LOG_LEVEL_INFO, PSTR ("PIL: Relay order ignored from %u"), XdrvMailbox.payload);

  // else command not handled
  else result = false;

  return result;
}

// Show JSON status (for MQTT)
// Format is
//   "PilotWire":{"Mode":2,"Temp":18.6,"Target":21.0,"High":22.0,"Heat":1,"Night":0,"Eco":1,"Win":0,"Pres":1,"Delay":262,"Event":2}
void PilotwireShowJSON (bool is_autonomous)
{
  bool     heating, window, presence;
  uint8_t  mode, ecowatt, event;
  uint32_t time_now, delay;
  float    temperature;
  char     str_value[16];

  // get current time
  time_now = LocalTime ();

  // add , in append mode or { in direct publish mode
  if (is_autonomous) Response_P (PSTR ("{"));
    else ResponseAppend_P (PSTR (","));

  // pilotwire section
  ResponseAppend_P (PSTR ("\"Pilotwire\":{"));

  // configuration mode
  if ((pilotwire_config.mode == PILOTWIRE_MODE_COMFORT) && pilotwire_status.night_mode) mode = PILOTWIRE_MODE_NIGHT;
    else mode = pilotwire_config.mode;
  ResponseAppend_P (PSTR ("\"Mode\":%d"), mode);

  // current temperature
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &pilotwire_status.temp_current);
  ResponseAppend_P (PSTR (",\"Temp\":%s"), str_value);

  // target temperature
  temperature = PilotwireGetModeTemperature (pilotwire_config.mode);
  if (pilotwire_status.night_mode) temperature = min (temperature, PilotwireConvertTargetTemperature (PILOTWIRE_TARGET_NIGHT));
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &temperature);
  ResponseAppend_P (PSTR (",\"Target\":%s"), str_value);

  // max temperature
  temperature = PilotwireConvertTargetTemperature (PILOTWIRE_TARGET_HIGH);
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &temperature);
  ResponseAppend_P (PSTR (",\"High\":%s"), str_value);

  // default heating state
  ResponseAppend_P (PSTR (",\"Heat\":%u"), PilotwireGetHeatingStatus ());

  // ecowatt management status
  ecowatt = PilotwireEcowattGetLevel ();
  ResponseAppend_P (PSTR (",\"Eco\":%u"), ecowatt);

  // window open detection status
  window = PilotwireWindowGetStatus ();
  ResponseAppend_P (PSTR (",\"Win\":%u"), window);

  // presence detection status and delay (in mn)
  presence = PilotwirePresenceIsActive ();
  ResponseAppend_P (PSTR (",\"Pres\":%u"), presence);

  // time since last presence
  if (pilotwire_config.presence)
  {
    if (time_now < pilotwire_presence.time_last) delay = 0;
      else delay = (time_now - pilotwire_presence.time_last) / 60;
    ResponseAppend_P (PSTR (",\"Since\":%u"), delay);
  }

  // set event type thru priority : none, offload, ecowatt level 2, ecowatt level 3, window opened, night mode, absence, heating, temperature reached
  if (pilotwire_config.mode == PILOTWIRE_MODE_OFF) event = PILOTWIRE_EVENT_NONE;
  else if (PilotwireGetHeatingStatus ()) event = PILOTWIRE_EVENT_HEATING;
  else if (OffloadIsOffloaded ()) event = PILOTWIRE_EVENT_OFFLOAD;
  else if (ecowatt == ECOWATT_LEVEL_WARNING) event = PILOTWIRE_EVENT_ECOWATT2;
  else if (ecowatt == ECOWATT_LEVEL_POWERCUT) event = PILOTWIRE_EVENT_ECOWATT3;
  else if (window) event = PILOTWIRE_EVENT_WINDOW;
  else if (!presence) event = PILOTWIRE_EVENT_NOBODY;
  else event = PILOTWIRE_EVENT_NONE;
  ResponseAppend_P (PSTR (",\"Event\":%u"), event);

  // set icon associated with event
  GetTextIndexed (str_value, sizeof (str_value), event, kPilotwireEventIcon);
  ResponseAppend_P (PSTR (",\"Icon\":\"%s\""), str_value);

  ResponseAppend_P (PSTR ("}"));

  // publish it if not in append mode
  if (is_autonomous)
  {
    // add offload status
    OffloadShowJSON (false);

    // publish message
    ResponseAppend_P (PSTR ("}"));
    MqttPublishTeleSensor ();
  } 

  // reset update flag and update switch status
  pilotwire_status.json_update = false;
  pilotwire_status.temp_json   = pilotwire_status.temp_current;
}

void PilotwireInit ()
{
  uint32_t index, count;

  // offload : module is not managing the relay
  OffloadSetManagedMode (false);

  // set MQTT message only for switch 1 (movement detection)
  Settings->switchmode[0] = 15;

  // disable reset 1 with button multi-press (SetOption1)
  Settings->flag.button_restrict = 1;

  // offload : remove all device types, declare available device types and set default as room heater
  OffloadResetAvailableType ();
  OffloadAddAvailableType (OFFLOAD_DEVICE_ROOM);
  OffloadAddAvailableType (OFFLOAD_DEVICE_OFFICE);
  OffloadAddAvailableType (OFFLOAD_DEVICE_LIVING);
  OffloadAddAvailableType (OFFLOAD_DEVICE_BATHROOM);
  OffloadAddAvailableType (OFFLOAD_DEVICE_KITCHEN);

  // initialise graph data per period
  for (index = 0; index < PILOTWIRE_PERIOD_MAX; index++)
  {
    // main grah data
    pilotwire_graph.index[index] = 0;
    pilotwire_graph.counter[index] = 0;
    pilotwire_graph.temp[index] = NAN;
    pilotwire_graph.target[index] = NAN;
    pilotwire_graph.device[index] = UINT8_MAX;
  }

  // initialise temperature graph
  for (index = 0; index < PILOTWIRE_ARRAY_MAX; index ++)
  {
    pilotwire_graph.arr_last[index] = PILOTWIRE_GRAPH_SAMPLE;
    for (count = 0; count < PILOTWIRE_GRAPH_SAMPLE; count++)
    {
      pilotwire_graph.arr_temp[index][count]   = SHRT_MAX;
      pilotwire_graph.arr_target[index][count] = SHRT_MAX;
      pilotwire_graph.arr_state[index][count]  = UINT8_MAX;
    }
  }

  // initialise open window detection temperature array
  for (index = 0; index < PILOTWIRE_WINDOW_SAMPLE_NBR; index++) pilotwire_window.arr_temp[index] = NAN;

  // load configuration
  PilotwireLoadConfig ();

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: pw_help to get help on Pilotwire commands"));
}

// called every 100ms to publish JSON
void PilotwireEvery100ms ()
{
  if (pilotwire_status.json_update)
  {
    PilotwireShowJSON (true);
    pilotwire_status.json_update = false;
  }
}

// called every second, to update
//  - temperature
//  - presence detection
//  - graph data
void PilotwireEverySecond ()
{
  uint32_t counter;
  float    target;

  // update window open detection
  PilotwireWindowUpdateDetection ();

  // update presence detection
  PilotwirePresenceUpdateDetection ();

  // update temperature and target
  PilotwireUpdateTemperature ();

  // update heater relay state
  PilotwireUpdateHeaterState ();

  // read target temperature
  target = PilotwireGetCurrentTargetTemperature ();

  // loop thru the periods, to update graph data to the max on the period
  for (counter = 0; counter < PILOTWIRE_PERIOD_MAX; counter++)
  {
    // update graph temperature for current period (keep minimal value)
    if (isnan (pilotwire_graph.temp[counter])) pilotwire_graph.temp[counter] = pilotwire_status.temp_current;
    else if (!isnan (pilotwire_status.temp_current)) pilotwire_graph.temp[counter] = min (pilotwire_graph.temp[counter], pilotwire_status.temp_current);

    // update graph target temperature for current period (keep maximal value)
    if (isnan (pilotwire_graph.target[counter])) pilotwire_graph.target[counter] = target;
    else if (!isnan (target)) pilotwire_graph.target[counter] = max (pilotwire_graph.target[counter], target);

    // update device status
    if (pilotwire_graph.device[counter] != true) pilotwire_graph.device[counter] = PilotwireGetHeatingStatus ();

    // increment delay counter and if delay reached, update history data
    pilotwire_graph.counter[counter] = pilotwire_graph.counter[counter] % ARR_PILOTWIRE_PERIOD_SAMPLE[counter];
    if (pilotwire_graph.counter[counter] == 0) PilotwireUpdateGraph (counter);
    pilotwire_graph.counter[counter]++;
  }
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// icon : room logo
#ifdef USE_UFILESYS
void PilotwireWebIconLogo ()
{
  File file;

  // open file in read only mode in littlefs filesystem
  file = ffsp->open (PILOTWIRE_ICON_LOGO, "r");
  Webserver->streamFile (file, "image/jpeg");
  file.close ();
}
#endif    // USE_UFILESYS

// icons
void PilotwireWebIconDefault () { Webserver->send_P (200, "image/png", pilotwire_default_png, pilotwire_default_len); }

// get status logo : flame, offload, window opened, ecowatt, ...
void PilotwireWebIconStatus (char* pstr_status, const size_t size_status, const bool include_heating)
{
  uint8_t ecowatt;
  String  str_result = "";

  // verification
  if (pstr_status == nullptr) return;
  if (size_status == 0) return;

  // heating status
  if (include_heating && PilotwireGetHeatingStatus ()) str_result += " 🔥";

  // device offloaded
  if (OffloadIsOffloaded ()) str_result += " ⚡";

  // window opened
  if (PilotwireWindowGetStatus ()) str_result += " 🪟";

  // night mode
  if (pilotwire_status.night_mode) str_result += " 💤";

  // device ecowatt alert
  ecowatt = EcowattGetCurrentLevel () ;
  if (ecowatt == ECOWATT_LEVEL_WARNING) str_result += " 🟠";
  else if (ecowatt == ECOWATT_LEVEL_POWERCUT) str_result += " 🔴";

  // format result
  str_result.trim ();
  if (str_result.length () == 0) str_result = "&nbsp;";
  strlcpy (pstr_status, str_result.c_str (), size_status);
}

// append pilot wire state to main page
void PilotwireWebSensor ()
{
  float temp_target;
  char  str_icon[8];
  char  str_status[16];
  char  str_label[16];

  // read target temperature
  temp_target = PilotwireGetCurrentTargetTemperature ();

  // display current mode
  GetTextIndexed (str_icon, sizeof (str_icon), pilotwire_config.mode, kPilotwireModeIcon);
  GetTextIndexed (str_label, sizeof (str_label), pilotwire_config.mode, kPilotwireModeLabel);
  PilotwireWebIconStatus (str_status, sizeof (str_status), false);
  WSContentSend_PD (PSTR ("{s}%s %s %s{m}%s %s{e}\n"), D_PILOTWIRE, D_PILOTWIRE_MODE, str_icon, str_label, str_status);

  // get current and target temperature

  // device heating
  if (PilotwireGetHeatingStatus ()) strcpy (str_icon, "🔥"); else strcpy (str_icon, "");
  ext_snprintf_P (str_label, sizeof (str_label), PSTR ("%01_f"), &temp_target);
  ext_snprintf_P (str_status, sizeof (str_status), PSTR ("%01_f"), &pilotwire_status.temp_current);
  WSContentSend_PD (PSTR ("{s}%s %s %s{m}<b>%s</b> / %s °C{e}\n"), D_PILOTWIRE, D_PILOTWIRE_TEMPERATURE, str_icon, str_status, str_label);
}

// intermediate page to update selected status from main page
void PilotwireWebPageSwitchStatus ()
{
  char str_argument[16];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // check for 'therm' parameter
  WebGetArg (D_CMND_PILOTWIRE_MODE, str_argument, sizeof (str_argument));
  if (strlen (str_argument) > 0) PilotwireSetMode ((uint8_t)atoi (str_argument));

  // auto reload root page with dark background
  WSContentStart_P (D_PILOTWIRE_CONTROL, false);
  WSContentSend_P (PSTR ("</script>\n"));
  WSContentSend_P (PSTR ("<meta http-equiv='refresh' content='0;URL=/' />\n"));
  WSContentSend_P (PSTR ("</head>\n"));
  WSContentSend_P (PSTR ("<body bgcolor='#303030'></body>\n"));
  WSContentSend_P (PSTR ("</html>\n"));
  WSContentEnd ();
}

// add buttons on main page
void PilotwireWebMainButton ()
{
  uint32_t index;
  float    temperature;
  char     str_icon[8];
  char     str_temp[12];
  char     str_label[16];

  // control button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_PILOTWIRE_PAGE_CONTROL, D_PILOTWIRE_CONTROL);

  // graph button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_PILOTWIRE_PAGE_GRAPH, D_PILOTWIRE_GRAPH);

  // status mode options
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'>\n"), D_PILOTWIRE_PAGE_STATUS);
  WSContentSend_P (PSTR ("<select style='width:100%%;text-align:center;padding:8px;border-radius:0.3rem;' name='%s' onchange='this.form.submit();'>\n"), D_CMND_PILOTWIRE_MODE, pilotwire_config.mode);
  WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>\n"), UINT8_MAX, "", PSTR ("-- Select mode --"));
  for (index = 0; index < PILOTWIRE_MODE_MAX; index ++)
  {
    // get display
    GetTextIndexed (str_icon, sizeof (str_icon), index, kPilotwireModeIcon);
    GetTextIndexed (str_label, sizeof (str_label), index, kPilotwireModeLabel);

    // get temperature
    temperature = PilotwireGetModeTemperature (index);
    if (index == PILOTWIRE_MODE_FORCED) strcpy (str_temp, "");
      else ext_snprintf_P (str_temp, sizeof (str_temp), PSTR ("(%01_f °C)"), &temperature);

    // display status mode
    WSContentSend_P (PSTR ("<option value='%d'>%s %s %s</option>\n"), index, str_icon, str_label, str_temp);
  }
  WSContentSend_P (PSTR ("</select>\n"));
  WSContentSend_P (PSTR ("</form></p>\n"));
}

// Pilotwire heater configuration web page
void PilotwireWebPageConfigure ()
{
  bool    state;
  uint8_t value;
  float   temperature;
  char    str_icon[8];
  char    str_step[8];
  char    str_value[16];
  char    str_argument[24];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg (F ("save")))
  {
    // get pilotwire device type according to 'type' parameter
    WebGetArg (D_CMND_PILOTWIRE_TYPE, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) PilotwireSetDeviceType ((uint8_t)atoi (str_argument), false);

    // enable open window detection according to 'window' parameter
    WebGetArg (D_CMND_PILOTWIRE_WINDOW, str_argument, sizeof (str_argument));
    state = (strcmp (str_argument, "on") == 0);
    PilotwireWindowEnable (state, false);

    // enable presence detection according to 'pres' parameter
    WebGetArg (D_CMND_PILOTWIRE_PRESENCE, str_argument, sizeof (str_argument));
    state = (strcmp (str_argument, "on") == 0);
    PilotwirePresenceEnable (state, false);

    // enable ecowatt detection according to 'ecowatt' parameter
    WebGetArg (D_CMND_PILOTWIRE_ECOWATT, str_argument, sizeof (str_argument));
    state = (strcmp (str_argument, "on") == 0);
    PilotwireEcowattEnable (state, false);

    // set lowest temperature according to 'low' parameter
    WebGetArg (D_CMND_PILOTWIRE_LOW, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) PilotwireSetTargetTemperature (PILOTWIRE_TARGET_LOW, atof (str_argument), false);

    // set highest temperature according to 'high' parameter
    WebGetArg (D_CMND_PILOTWIRE_HIGH, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) PilotwireSetTargetTemperature (PILOTWIRE_TARGET_HIGH, atof (str_argument), false);

    // set comfort mode temperature according to 'comfort' parameter
    WebGetArg (D_CMND_PILOTWIRE_COMFORT, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) PilotwireSetTargetTemperature (PILOTWIRE_TARGET_COMFORT, atof (str_argument), false);

    // set night mode temperature according to 'night' parameter
    WebGetArg (D_CMND_PILOTWIRE_NIGHT, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) PilotwireSetTargetTemperature (PILOTWIRE_TARGET_NIGHT, atof (str_argument), false);

    // set eco temperature according to 'eco' parameter
    WebGetArg (D_CMND_PILOTWIRE_ECO, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) PilotwireSetTargetTemperature (PILOTWIRE_TARGET_ECO, atof (str_argument), false);

    // set no frost temperature according to 'nofrost' parameter
    WebGetArg (D_CMND_PILOTWIRE_NOFROST, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) PilotwireSetTargetTemperature (PILOTWIRE_TARGET_NOFROST, atof (str_argument), false);

    // set ecowatt level 2 temperature according to 'ecowatt2' parameter
    WebGetArg (D_CMND_PILOTWIRE_ECOWATT_N2, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) PilotwireSetTargetTemperature (PILOTWIRE_TARGET_ECOWATT2, atof (str_argument), false);

    // set ecowatt level 3 temperature according to 'ecowatt3' parameter
    WebGetArg (D_CMND_PILOTWIRE_ECOWATT_N3, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) PilotwireSetTargetTemperature (PILOTWIRE_TARGET_ECOWATT3, atof (str_argument), false);

    // set presence detection initial timeout
    WebGetArg (D_CMND_PILOTWIRE_INITIAL, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) pilotwire_config.timeout_initial = atoi (str_argument);

    // set presence detection normal timeout
    WebGetArg (D_CMND_PILOTWIRE_NORMAL, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) pilotwire_config.timeout_normal = atoi (str_argument);

    // save configuration
    PilotwireSaveConfig ();
  }

  // get step temperature
  temperature = PILOTWIRE_TEMP_STEP;
  ext_snprintf_P (str_step, sizeof (str_step), PSTR ("%1_f"), &temperature);

  // beginning of form
  WSContentStart_P (D_PILOTWIRE_CONFIGURE " " D_PILOTWIRE);
  WSContentSendStyle ();
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("span.key {float:right;font-size:0.7rem;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_PILOTWIRE_PAGE_CONFIG);

  //    Device type 
  // -----------------

  WSContentSend_P (PILOTWIRE_FIELDSET_START, "🔗", D_PILOTWIRE_CONNEXION);

  // command type selection
  if (pilotwire_config.type == PILOTWIRE_DEVICE_PILOTWIRE) strcpy (str_argument, D_PILOTWIRE_CHECKED); else strcpy (str_argument, "");
  WSContentSend_P (PSTR ("<input type='radio' id='normal' name='%s' value=%d %s>%s<br>\n"), D_CMND_PILOTWIRE_TYPE, PILOTWIRE_DEVICE_PILOTWIRE, str_argument, D_PILOTWIRE);
  if (pilotwire_config.type == PILOTWIRE_DEVICE_DIRECT) strcpy_P (str_argument, PSTR (D_PILOTWIRE_CHECKED)); else strcpy (str_argument, "");
  WSContentSend_P (PSTR ("<input type='radio' id='direct' name='%s' value=%d %s>%s<br>\n"), D_CMND_PILOTWIRE_TYPE, PILOTWIRE_DEVICE_DIRECT, str_argument, D_PILOTWIRE_DIRECT);

  WSContentSend_P (PILOTWIRE_FIELDSET_STOP);

  //   Options
  //    - Ecowatt management
  //    - Window opened detection
  //    - Presence detection
  // -----------------------------

  WSContentSend_P (PILOTWIRE_FIELDSET_START, "♻️", D_PILOTWIRE_OPTION);

  // window opened
  if (pilotwire_config.window) strcpy (str_value, D_PILOTWIRE_CHECKED); else strcpy (str_value, "");
  WSContentSend_P (PSTR ("<p><input type='checkbox' id='%s' name='%s' %s>\n"), D_CMND_PILOTWIRE_WINDOW, D_CMND_PILOTWIRE_WINDOW, str_value);
  WSContentSend_P (PSTR ("<label for='%s'>%s %s</label></p>\n"), D_CMND_PILOTWIRE_WINDOW, D_PILOTWIRE_WINDOW, D_PILOTWIRE_DETECTION);

  // presence
  if (pilotwire_config.presence) strcpy (str_value, D_PILOTWIRE_CHECKED); else strcpy (str_value, "");
  WSContentSend_P (PSTR ("<p><input type='checkbox' id='%s' name='%s' %s>\n"), D_CMND_PILOTWIRE_PRESENCE, D_CMND_PILOTWIRE_PRESENCE, str_value);
  WSContentSend_P (PSTR ("<label for='%s'>%s %s</label></p>\n"), D_CMND_PILOTWIRE_PRESENCE, D_PILOTWIRE_PRESENCE, D_PILOTWIRE_DETECTION);

  // ecowatt
  if (pilotwire_config.ecowatt) strcpy (str_value, D_PILOTWIRE_CHECKED); else strcpy (str_value, "");
  WSContentSend_P (PSTR ("<p><input type='checkbox' id='%s' name='%s' %s>\n"), D_CMND_PILOTWIRE_ECOWATT, D_CMND_PILOTWIRE_ECOWATT, str_value);
  WSContentSend_P (PSTR ("<label for='%s'>%s %s</label></p>\n"), D_CMND_PILOTWIRE_ECOWATT, D_PILOTWIRE_SIGNAL, D_PILOTWIRE_ECOWATT);

  WSContentSend_P (PILOTWIRE_FIELDSET_STOP);

  //   Temperatures
  // -----------------

  WSContentSend_P (PILOTWIRE_FIELDSET_START, "🌡️", PSTR ("Temperatures (°C)"));

  // lowest temperature
  GetTextIndexed (str_icon, sizeof (str_icon), PILOTWIRE_TARGET_LOW, kPilotwireTargetIcon);
  GetTextIndexed (str_argument, sizeof (str_argument), PILOTWIRE_TARGET_LOW, kPilotwireTargetLabel);
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &pilotwire_config.arr_temp[PILOTWIRE_TARGET_LOW]);
  WSContentSend_P (PILOTWIRE_FIELD_FULL, str_icon, str_argument, D_CMND_PILOTWIRE_LOW, D_CMND_PILOTWIRE_LOW, PILOTWIRE_TEMP_LIMIT_MIN, PILOTWIRE_TEMP_LIMIT_MAX, str_step, str_value);

  // highest temperature
  GetTextIndexed (str_icon, sizeof (str_icon), PILOTWIRE_TARGET_HIGH, kPilotwireTargetIcon);
  GetTextIndexed (str_argument, sizeof (str_argument), PILOTWIRE_TARGET_HIGH, kPilotwireTargetLabel);
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &pilotwire_config.arr_temp[PILOTWIRE_TARGET_HIGH]);
  WSContentSend_P (PILOTWIRE_FIELD_FULL, str_icon, str_argument, D_CMND_PILOTWIRE_HIGH, D_CMND_PILOTWIRE_HIGH, PILOTWIRE_TEMP_LIMIT_MIN, PILOTWIRE_TEMP_LIMIT_MAX, str_step, str_value);

  // comfort temperature
  GetTextIndexed (str_icon, sizeof (str_icon), PILOTWIRE_TARGET_COMFORT, kPilotwireTargetIcon);
  GetTextIndexed (str_argument, sizeof (str_argument), PILOTWIRE_TARGET_COMFORT, kPilotwireTargetLabel);
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &pilotwire_config.arr_temp[PILOTWIRE_TARGET_COMFORT]);
  WSContentSend_P (PILOTWIRE_FIELD_FULL, str_icon, str_argument, D_CMND_PILOTWIRE_COMFORT, D_CMND_PILOTWIRE_COMFORT, PILOTWIRE_TEMP_LIMIT_MIN, PILOTWIRE_TEMP_LIMIT_MAX, str_step, str_value);

  // eco temperature
  GetTextIndexed (str_icon, sizeof (str_icon), PILOTWIRE_TARGET_ECO, kPilotwireTargetIcon);
  GetTextIndexed (str_argument, sizeof (str_argument), PILOTWIRE_TARGET_ECO, kPilotwireTargetLabel);
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECO]);
  WSContentSend_P (PILOTWIRE_FIELD_FULL, str_icon, str_argument, D_CMND_PILOTWIRE_ECO, D_CMND_PILOTWIRE_ECO, - PILOTWIRE_TEMP_LIMIT_MAX, PILOTWIRE_TEMP_LIMIT_MAX, str_step, str_value);

  // no frost temperature
  GetTextIndexed (str_icon, sizeof (str_icon), PILOTWIRE_TARGET_NOFROST, kPilotwireTargetIcon);
  GetTextIndexed (str_argument, sizeof (str_argument), PILOTWIRE_TARGET_NOFROST, kPilotwireTargetLabel);
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &pilotwire_config.arr_temp[PILOTWIRE_TARGET_NOFROST]);
  WSContentSend_P (PILOTWIRE_FIELD_FULL, str_icon, str_argument, D_CMND_PILOTWIRE_NOFROST, D_CMND_PILOTWIRE_NOFROST, - PILOTWIRE_TEMP_LIMIT_MAX, PILOTWIRE_TEMP_LIMIT_MAX, str_step, str_value);

  // night mode temperature
  GetTextIndexed (str_icon, sizeof (str_icon), PILOTWIRE_TARGET_NIGHT, kPilotwireTargetIcon);
  GetTextIndexed (str_argument, sizeof (str_argument), PILOTWIRE_TARGET_NIGHT, kPilotwireTargetLabel);
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &pilotwire_config.arr_temp[PILOTWIRE_TARGET_NIGHT]);
  WSContentSend_P (PILOTWIRE_FIELD_FULL, str_icon, str_argument, D_CMND_PILOTWIRE_NIGHT, D_CMND_PILOTWIRE_NIGHT, - PILOTWIRE_TEMP_LIMIT_MAX, PILOTWIRE_TEMP_LIMIT_MAX, str_step, str_value);

  // ecowatt level 2 temperature
  GetTextIndexed (str_icon, sizeof (str_icon), PILOTWIRE_TARGET_ECOWATT2, kPilotwireTargetIcon);
  GetTextIndexed (str_argument, sizeof (str_argument), PILOTWIRE_TARGET_ECOWATT2, kPilotwireTargetLabel);
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT2]);
  WSContentSend_P (PILOTWIRE_FIELD_FULL, str_icon, str_argument, D_CMND_PILOTWIRE_ECOWATT_N2, D_CMND_PILOTWIRE_ECOWATT_N2, - PILOTWIRE_TEMP_LIMIT_MAX, PILOTWIRE_TEMP_LIMIT_MAX, str_step, str_value);

  // ecowatt level 3 temperature
  GetTextIndexed (str_icon, sizeof (str_icon), PILOTWIRE_TARGET_ECOWATT3, kPilotwireTargetIcon);
  GetTextIndexed (str_argument, sizeof (str_argument), PILOTWIRE_TARGET_ECOWATT3, kPilotwireTargetLabel);
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT3]);
  WSContentSend_P (PILOTWIRE_FIELD_FULL, str_icon, str_argument, D_CMND_PILOTWIRE_ECOWATT_N3, D_CMND_PILOTWIRE_ECOWATT_N3, - PILOTWIRE_TEMP_LIMIT_MAX, PILOTWIRE_TEMP_LIMIT_MAX, str_step, str_value);

  WSContentSend_P (PILOTWIRE_FIELDSET_STOP);

  //   Presence detection
  // ----------------------

  WSContentSend_P (PILOTWIRE_FIELDSET_START, "👋", PSTR ("Presence"));

  // initial presence detection timeout
  itoa (pilotwire_config.timeout_initial, str_value, 10);
  WSContentSend_P (PILOTWIRE_FIELD_FULL, "⌛", PSTR ("Disable on mode change (mn)"), D_CMND_PILOTWIRE_INITIAL, D_CMND_PILOTWIRE_INITIAL, 1, 43200, "1", str_value);

  // normal presence detection timeout
  itoa (pilotwire_config.timeout_normal, str_value, 10);
  WSContentSend_P (PILOTWIRE_FIELD_FULL, "⏳", PSTR ("No presence timeout (mn)"), D_CMND_PILOTWIRE_NORMAL, D_CMND_PILOTWIRE_NORMAL, 1, 43200, "1", str_value);

  WSContentSend_P (PILOTWIRE_FIELDSET_STOP);

  WSContentSend_P (PSTR ("<br>\n"));

  // save button
  WSContentSend_P (PSTR ("<p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), D_SAVE);
  WSContentSend_P (PSTR ("</form>\n"));

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

// Get specific argument as a value with min and max
int PilotwireWebGetArgValue (const char* pstr_argument, const int value_default, const int value_min, const int value_max)
{
  int  arg_value = value_default;
  char str_argument[8];

  // check arguments
  if (pstr_argument == nullptr) return arg_value;

  // check for argument
  if (Webserver->hasArg (pstr_argument))
  {
    WebGetArg (pstr_argument, str_argument, sizeof (str_argument));
    arg_value = atoi (str_argument);
  }

  // check for min and max value
  if ((value_min > 0) && (arg_value < value_min)) arg_value = value_min;
  if ((value_max > 0) && (arg_value > value_max)) arg_value = value_max;

  return arg_value;
}

// display temperature graph page
void PilotwireWebPageGraph ()
{
  bool     state;
  uint8_t  period, histo, histo_index;
  uint32_t index;
  float    value;
  short    temperature, target;
  char     str_date[16];
  char     str_value[48];

#ifdef USE_UFILESYS
  char     str_line[32];
  char     str_filename[UFS_FILENAME_SIZE];
  File     file;
# endif     // USE_UFILESYS

  // get numerical argument values
  period = PilotwireWebGetArgValue (D_CMND_PILOTWIRE_PERIOD, PILOTWIRE_PERIOD_LIVE,  0, PILOTWIRE_PERIOD_MAX  - 1);
  histo  = PilotwireWebGetArgValue (D_CMND_PILOTWIRE_HISTO,  0, 0, 0);
  pilotwire_config.height = (uint16_t)PilotwireWebGetArgValue (D_CMND_PILOTWIRE_HEIGHT, (int)pilotwire_config.height, 100, INT_MAX);

  // set target graph array according to period
  switch (period)
  {
    case PILOTWIRE_PERIOD_LIVE:
      histo_index = PILOTWIRE_ARRAY_LIVE;
      break;

    case PILOTWIRE_PERIOD_WEEK:
      histo_index = PILOTWIRE_ARRAY_HISTO;
      pilotwire_graph.arr_last[histo_index] = 1;
      for (index = 0; index < PILOTWIRE_GRAPH_SAMPLE; index++)
      {
        pilotwire_graph.arr_temp[histo_index][index]   = SHRT_MAX;
        pilotwire_graph.arr_target[histo_index][index] = SHRT_MAX;
        pilotwire_graph.arr_state[histo_index][index]  = UINT8_MAX;
      }

#ifdef USE_UFILESYS
      // check if file exists
      sprintf_P (str_filename, D_PILOTWIRE_FILE_WEEK, histo);
      if (ffsp->exists (str_filename))
      {
        //open file and skip header
        file = ffsp->open (str_filename, "r");
        UfsReadNextLine (file, str_line, sizeof (str_line));

        // loop to load historical array
        while (UfsReadNextLine (file, str_line, sizeof (str_line)) > 0)
        {
          // read index
          UfsExtractCsvColumn (str_line, ';', 1, str_value, sizeof (str_value), false);

          if (strlen (str_value) > 0) index = (uint32_t)atoi (str_value);
            else index = UINT32_MAX;
          if (index < PILOTWIRE_GRAPH_SAMPLE) 
          {
            // read temperature
            UfsExtractCsvColumn (str_line, ';', 4, str_value, sizeof (str_value), false);
            if (strlen (str_value) > 0) pilotwire_graph.arr_temp[histo_index][index] = (short)(atof (str_value) * 10);

            // read target temperature
            UfsExtractCsvColumn (str_line, ';', 5, str_value, sizeof (str_value), false);
            if (strlen (str_value) > 0) pilotwire_graph.arr_target[histo_index][index] = (short)(atof (str_value) * 10);

            // read state
            UfsExtractCsvColumn (str_line, ';', 6, str_value, sizeof (str_value), false);
            if (strlen (str_value) > 0)
            {
              pilotwire_graph.arr_state[histo_index][index] = (bool)atoi (str_value);
              pilotwire_graph.arr_last[histo_index] = index + 1;
            }
          }
        }
      }
# endif     // USE_UFILESYS

      break;
  }

  // set default min and max graph temperature
  pilotwire_graph.temp_max = PILOTWIRE_TEMP_DEFAULT_NORMAL;
  pilotwire_graph.temp_min = PILOTWIRE_TEMP_DEFAULT_NORMAL - 2;

  // loop to adjust graph min and max temperature
  for (index = 0; index < PILOTWIRE_GRAPH_SAMPLE; index++)
  {
    if (pilotwire_graph.arr_temp[histo_index][index] != SHRT_MAX)
    {
      value = (float)pilotwire_graph.arr_temp[histo_index][index] / 10;
      pilotwire_graph.temp_min = min (pilotwire_graph.temp_min, value);
      pilotwire_graph.temp_max = max (pilotwire_graph.temp_max, value);
    }
  }

  // adjust to lower and upper range
  pilotwire_graph.temp_max = ceil  (pilotwire_graph.temp_max + 0.5);
  pilotwire_graph.temp_min = floor (pilotwire_graph.temp_min);

  // -----------------
  //    Page style
  // -----------------

  // beginning of page
  WSContentStart_P (D_PILOTWIRE_PAGE_GRAPH, false);
  WSContentSend_P (PSTR ("</script>\n"));

  // set page as scalable
  WSContentSend_P (PSTR ("<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=yes'/>\n"));

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {margin:0.25rem auto;padding:0.1rem 0px;text-align:center;vertical-align:middle;}\n"));
  WSContentSend_P (PSTR ("div a {color:white;}\n"));

  WSContentSend_P (PSTR ("div.title {position:relative;height:160px;text-align:center;}\n"));
  WSContentSend_P (PSTR ("div.title img {height:160px;border-radius:12px;}\n"));
  WSContentSend_P (PSTR ("div.title div {position:absolute;font-size:1.25rem;font-weight:bold;top:15px;left:50%%;transform:translate(-50%%,-50%%);text-shadow:-1px -1px 0 grey,1px -1px 0 grey,-1px 1px 0 grey,1px 1px 0 grey;}\n"));

  WSContentSend_P (PSTR ("div.choice {display:inline-block;font-size:1rem;padding:0px;margin:auto 5px;border:1px #666 solid;background:none;color:#fff;border-radius:6px;}\n"));
  WSContentSend_P (PSTR ("div.choice div {background:#666;}\n"));
  WSContentSend_P (PSTR ("div.choice a div {background:none;}\n"));

  WSContentSend_P (PSTR ("div.item {display:inline-block;padding:0.2rem auto;margin:1px;border:none;border-radius:4px;}\n"));
  WSContentSend_P (PSTR ("div.histo {font-size:0.9rem;padding:0px;margin-top:0px;}\n"));
  WSContentSend_P (PSTR ("div.day {width:50px;}\n"));
  WSContentSend_P (PSTR ("div.week {width:90px;}\n"));
  WSContentSend_P (PSTR ("div a div.item:hover {background:#aaa;}\n"));

  WSContentSend_P (PSTR ("div.incr {padding:0px;color:white;border:1px #666 solid;border-radius:6px;}\n"));
  WSContentSend_P (PSTR ("div.left {float:left;margin-left:6%%;}\n"));
  WSContentSend_P (PSTR ("div.right {float:right;margin-right:6%%;}\n"));

  WSContentSend_P (PSTR ("div.period {width:70px;}\n"));
  WSContentSend_P (PSTR ("div.size {width:25px;}\n"));

  WSContentSend_P (PSTR ("select {background:#666;color:white;font-size:0.8rem;margin:1px;border:solid #666 2px;border-radius:4px;}\n"));

  WSContentSend_P (PSTR ("div.graph {width:100%%;margin:auto;margin-top:2vh;}\n"));
  WSContentSend_P (PSTR ("svg.graph {width:100%%;height:50vh;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // page body
  WSContentSend_P (PSTR ("</head>\n"));
  WSContentSend_P (PSTR ("<body>\n"));

  // -----------------
  //      Title 
  // -----------------

  // picture
  strcpy (str_value, PILOTWIRE_ICON_DEFAULT);
#ifdef USE_UFILESYS
  if (ffsp->exists (PILOTWIRE_ICON_LOGO)) strcpy (str_value, PILOTWIRE_ICON_LOGO);
#endif    // USE_UFILESYS

  // title
  WSContentSend_P (PSTR ("<div class='title'><a href='/'><img src='%s'><div>%s</div></a></div>\n"), str_value, SettingsText(SET_DEVICENAME));

  // -----------------
  //      Period 
  // -----------------

  WSContentSend_P (PSTR ("<form method='post' action='%s?period=%d'>\n"), D_PILOTWIRE_PAGE_GRAPH, period);
  WSContentSend_P (PSTR ("<div>\n"));       // line

  WSContentSend_P (PSTR ("<div class='choice'>\n"));

  //    Live
  // ----------
  GetTextIndexed (str_value, sizeof (str_value), PILOTWIRE_PERIOD_LIVE, kPilotwireGraphPeriod);

  // set button according to active state
  if (period != PILOTWIRE_PERIOD_LIVE) WSContentSend_P (PSTR ("<a href='%s?period=%d'>"), D_PILOTWIRE_PAGE_GRAPH, PILOTWIRE_PERIOD_LIVE);
  WSContentSend_P (PSTR ("<div class='item period'>%s</div>"), str_value);
  if (period != PILOTWIRE_PERIOD_LIVE) WSContentSend_P (PSTR ("</a>"));
  WSContentSend_P (PSTR ("\n"));

  //    Week
  // ----------

#ifdef USE_UFILESYS
  // if period is weekly, display available weeks
  if (period == PILOTWIRE_PERIOD_WEEK)
  {
    // set button according to active state
    WSContentSend_P (PSTR ("<select name='histo' id='histo' onchange='this.form.submit();'>\n"));

    // loop thru week files
    for (index = 0; index < PILOTWIRE_HISTO_WEEK_MAX; index++)
    {
      // check if file exists
      sprintf_P (str_filename, D_PILOTWIRE_FILE_WEEK, index);
      if (ffsp->exists (str_filename))
      {
        PilotwireHistoWeekDate (index, str_date, sizeof (str_date));
        if (index == histo) strcpy_P (str_value, PSTR (" selected"));
          else strcpy (str_value, "");
        WSContentSend_P (PSTR ("<option value='%u' %s>%s</option>\n"), index, str_value, str_date);
      }
    }

    WSContentSend_P (PSTR ("</select>\n"));      
  }

  // else link to the period
  else
  {
    GetTextIndexed (str_value, sizeof (str_value), PILOTWIRE_PERIOD_WEEK, kPilotwireGraphPeriod);
    WSContentSend_P (PSTR ("<a href='%s?period=%d'><div class='item period'>%s</div></a>\n"), D_PILOTWIRE_PAGE_GRAPH, PILOTWIRE_PERIOD_WEEK, str_value);
  } 

# endif     // USE_UFILESYS

  WSContentSend_P (PSTR ("</div>\n"));        // choice
  WSContentSend_P (PSTR ("</div>\n"));        // line
  WSContentSend_P (PSTR ("</form>\n"));

  // -----------------
  //   Graph - style
  // -----------------

  // start of SVG graph
  WSContentSend_P (PSTR ("<div class='graph'>\n"));
  WSContentSend_P (PSTR ("<svg class='graph' viewBox='%d %d %d %d' preserveAspectRatio='none'>>\n"), 0, 0, PILOTWIRE_GRAPH_WIDTH, pilotwire_config.height);

  // SVG style 
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));

  WSContentSend_P (PSTR ("rect {stroke:white;fill:none;}\n"));

  WSContentSend_P (PSTR ("text {fill:white;font-size:1.2rem;}\n"));
  WSContentSend_P (PSTR ("text.temp {fill:yellow;}\n"));

  WSContentSend_P (PSTR ("line {stroke:white;stroke-width:1;stroke-dasharray:1 8;}\n"));
  WSContentSend_P (PSTR ("line.temp {stroke:yellow;}\n"));

  WSContentSend_P (PSTR ("path {stroke-width:1;opacity:1;}\n"));
  WSContentSend_P (PSTR ("path.target {stroke:#ffca74;fill:#ffca74;opacity:0.9;fill-opacity:0.5;}\n"));
  WSContentSend_P (PSTR ("path.temp {stroke:#6bc4ff;fill:#6bc4ff;fill-opacity:0.8;}\n"));
  WSContentSend_P (PSTR ("path.state {stroke:#bb3f52;fill:#bb3f52;}\n"));

  WSContentSend_P (PSTR ("</style>\n"));

  // -----------------
  //   Graph - curve
  // -----------------

  PilotwireWebGraphData ();

  // -----------------
  //   Graph - frame
  // -----------------

  PilotwireWebGraphFrame ();

  // -----------------
  //   Graph - end
  // -----------------

  WSContentSend_P (PSTR ("</svg>\n"));      // graph
  WSContentSend_P (PSTR ("</div>\n"));      // graph

  // end of page
  WSContentStop ();
}

// Temperature graph frame
void PilotwireWebGraphFrame ()
{
  float temperature, position;
  char  str_temperature[8], str_position[8];

  // graph frame
  WSContentSend_P (PSTR ("<rect x='%d%%' y='0%%' width='%d%%' height='99.9%%' rx='10' />\n"), PILOTWIRE_GRAPH_PERCENT_START, PILOTWIRE_GRAPH_PERCENT_STOP - PILOTWIRE_GRAPH_PERCENT_START);

  // display temperature boundaries
  WSContentSend_P (PSTR ("<text class='temp' x='%d%%' y='%d%%'>%s</text>\n"), 97, 3, "°C");
  ext_snprintf_P (str_position, sizeof (str_position), PSTR ("%0_f"), &pilotwire_graph.temp_max);
  WSContentSend_P (PSTR ("<text class='temp' x='%d%%' y='%d%%'>%s</text>\n"), 1, 3, str_position);
  ext_snprintf_P (str_position, sizeof (str_position), PSTR ("%0_f"), &pilotwire_graph.temp_min);
  WSContentSend_P (PSTR ("<text class='temp' x='%d%%' y='%d%%'>%s</text>\n"), 1, 99, str_position);

  // graph temperature units
  for (temperature = pilotwire_graph.temp_min + 1; temperature < pilotwire_graph.temp_max; temperature++)
  {
    // calculate temperature
    ext_snprintf_P (str_temperature, sizeof (str_temperature), PSTR ("%0_f"), &temperature);

    // calculate line position
    position = 100 * ((pilotwire_graph.temp_max - temperature) / (pilotwire_graph.temp_max - pilotwire_graph.temp_min));
    ext_snprintf_P (str_position, sizeof (str_position), PSTR ("%1_f"), &position);
    WSContentSend_P (PSTR ("<line class='temp' x1='%d%%' y1='%s%%' x2='%d%%' y2='%s%%' />\n"), PILOTWIRE_GRAPH_PERCENT_START, str_position, PILOTWIRE_GRAPH_PERCENT_STOP, str_position);

    // calculate text position
    position++;
    ext_snprintf_P (str_position, sizeof (str_position), PSTR ("%1_f"), &position);
    WSContentSend_P (PSTR ("<text class='temp' x='%d%%' y='%s%%'>%s</text>\n"), 1, str_position, str_temperature);
  }
}

// Temperature graph data
void PilotwireWebGraphData ()
{
  bool     first_point;
  int      period, histo;
  uint32_t unit_width, shift_unit, shift_width;
  uint32_t graph_x, draw_x, prev_x;
  uint32_t graph_left, graph_right, graph_width;
  uint32_t graph_value;
  uint32_t index, histo_index, array_index;
  uint32_t current_time, delta_time;
  float    graph_y, prev_y;
  float    graph_on, graph_off;
  float    value, temp_scope;
  TIME_T   current_dst;
  char     str_text[8];

  // current time
  current_time = LocalTime ();
  BreakTime (current_time, current_dst);

  // validature min and max temperature difference
  temp_scope = pilotwire_graph.temp_max - pilotwire_graph.temp_min;
  if (temp_scope <= 0) return;

  // get numerical argument values
  period = PilotwireWebGetArgValue (D_CMND_PILOTWIRE_PERIOD, PILOTWIRE_PERIOD_LIVE,  0, PILOTWIRE_PERIOD_MAX  - 1);
  histo  = PilotwireWebGetArgValue (D_CMND_PILOTWIRE_HISTO,  0, 0, 0);

  // set period array index
  if (period == PILOTWIRE_PERIOD_LIVE) histo_index = PILOTWIRE_ARRAY_LIVE;
    else histo_index = PILOTWIRE_ARRAY_HISTO;

  // boundaries of SVG graph
  graph_left  = PILOTWIRE_GRAPH_PERCENT_START * PILOTWIRE_GRAPH_WIDTH / 100 + 1;
  graph_right = PILOTWIRE_GRAPH_PERCENT_STOP * PILOTWIRE_GRAPH_WIDTH / 100;
  graph_width = graph_right - graph_left;
  graph_off   = PILOTWIRE_GRAPH_HEIGHT;
  graph_on    = PILOTWIRE_GRAPH_HEATING * PILOTWIRE_GRAPH_HEIGHT / 100;

  // ---- Target ---- //

  draw_x = 0;
  prev_x = 0;
  prev_y = NAN;
  WSContentSend_P (PSTR ("<path class='target' "));

  for (index = 0; index < pilotwire_graph.arr_last[histo_index]; index++)
  {
    // calculate array index
    if (histo_index == PILOTWIRE_ARRAY_LIVE) array_index = (index + pilotwire_graph.index[histo_index]) % PILOTWIRE_GRAPH_SAMPLE;
      else array_index = index;

    // calculate target temperature and limit value to boundaries
    value = (float)pilotwire_graph.arr_target[histo_index][array_index] / 10;
    value = max (value, pilotwire_graph.temp_min);
    value = min (value, pilotwire_graph.temp_max);

    // calculate end point x and y
    graph_x = graph_left + (graph_width * index / PILOTWIRE_GRAPH_SAMPLE);
    if (pilotwire_graph.arr_state[histo_index][array_index] == UINT8_MAX) graph_y = prev_y;
      else graph_y = (1 - ((value - pilotwire_graph.temp_min) / temp_scope)) * PILOTWIRE_GRAPH_HEIGHT;

    // first point
    if (draw_x == 0) WSContentSend_P ("d='M%u %u ", graph_x, pilotwire_config.height);

    // if value is different than previous one, draw curve
    if (graph_y != prev_y)
    {
      if (draw_x != prev_x) WSContentSend_P (PSTR ("L%u %u "), prev_x, (uint16_t)prev_y);
      WSContentSend_P (PSTR ("L%u %u "), graph_x, (uint16_t)graph_y);
      draw_x = graph_x;
    }

    // save previous values
    prev_x = graph_x;
    prev_y = graph_y;
  }

  // end of graph
  if (draw_x != 0)
  {
    WSContentSend_P ("L%u %u ", graph_x, (uint16_t)graph_y);
    WSContentSend_P ("L%u %u ", graph_x, pilotwire_config.height);
  }
  WSContentSend_P (PSTR ("'/>\n"));

  // ---- Temperature ---- //

  draw_x = 0;
  prev_x = 0;
  prev_y  = NAN;
  graph_y = NAN;
  WSContentSend_P ("<path class='temp' ");
  for (index = 0; index < pilotwire_graph.arr_last[histo_index]; index++)
  {
    // calculate array index
    if (histo_index == PILOTWIRE_ARRAY_LIVE) array_index = (index + pilotwire_graph.index[histo_index]) % PILOTWIRE_GRAPH_SAMPLE;
      else array_index = index;

    // calculate end point x and y
    graph_x = graph_left + (graph_width * index / PILOTWIRE_GRAPH_SAMPLE);
    if (pilotwire_graph.arr_temp[histo_index][array_index] == SHRT_MAX) graph_y = prev_y;
      else
      {
        value   = (float)pilotwire_graph.arr_temp[histo_index][array_index] / 10;
        graph_y = (1 - ((value - pilotwire_graph.temp_min) / temp_scope)) * PILOTWIRE_GRAPH_HEIGHT;
      }

    // first point
    if (draw_x == 0) WSContentSend_P ("d='M%u %u ", graph_x, pilotwire_config.height);

    // if value is different than previous one, draw curve
    if (graph_y != prev_y)
    {
      if (draw_x != prev_x) WSContentSend_P (PSTR ("L%u %u "), prev_x, (uint16_t)prev_y);
      WSContentSend_P (PSTR ("L%u %u "), graph_x, (uint16_t)graph_y);
      draw_x = graph_x;
    }

    // save previous values
    prev_x = graph_x;
    prev_y = graph_y;
  }

  // end of graph
  if (draw_x != 0)
  {
    WSContentSend_P ("L%u %u ", graph_x, (uint16_t)graph_y);
    WSContentSend_P ("L%u %u ", graph_x, pilotwire_config.height);
  }
  WSContentSend_P (PSTR ("'/>\n"));

  // ---- State ---- //

  draw_x = 0;
  prev_x = 0;
  prev_y = NAN;
  WSContentSend_P ("<path class='state' ");
  for (index = 0; index < pilotwire_graph.arr_last[histo_index]; index++)
  {
    // calculate array index
    if (histo_index == PILOTWIRE_ARRAY_LIVE) array_index = (index + pilotwire_graph.index[histo_index]) % PILOTWIRE_GRAPH_SAMPLE;
      else array_index = index;

    // if state has been defined
    if (pilotwire_graph.arr_state[histo_index][array_index] != UINT8_MAX)
    {
      // calculate end point x and y
      graph_x = graph_left + (graph_width * index / PILOTWIRE_GRAPH_SAMPLE);
      if (pilotwire_graph.arr_state[histo_index][array_index] == UINT8_MAX) graph_y = prev_y;
        else if (pilotwire_graph.arr_state[histo_index][array_index]) graph_y = graph_on;
          else graph_y = graph_off;

      // first point
      if (draw_x == 0) WSContentSend_P ("d='M%u %u ", graph_x, pilotwire_config.height);
      if (prev_x == 0) prev_x = graph_x;

      // if value is different than previous one, draw curve
      if (graph_y != prev_y)
      {
        WSContentSend_P (PSTR ("L%u %u "), graph_x, (uint16_t)prev_y);
        WSContentSend_P (PSTR ("L%u %u "), graph_x, (uint16_t)graph_y);
        draw_x = graph_x;
      }

      // save previous values
      prev_x = graph_x;
      prev_y = graph_y;
    }
  }

  // end of graph
  if (draw_x != 0)
  {
    WSContentSend_P ("L%u %u ", graph_x, (uint16_t)graph_y);
    WSContentSend_P ("L%u %u ", graph_x, pilotwire_config.height);
  }
  WSContentSend_P (PSTR ("'/>\n"));

  // ---- Time ---- //

  if (period == PILOTWIRE_PERIOD_LIVE)
  {
    // calculate horizontal shift
    unit_width  = graph_width / 6;
    shift_unit  = current_dst.hour % 4;
    shift_width = unit_width - (unit_width * shift_unit / 4) - (unit_width * current_dst.minute / 240);

    // calculate first time displayed by substracting (5 * 4h + shift) to current time
    current_time -= (5 * 14400) + (shift_unit * 3600); 

    // display 4 hours separation lines with hour
    for (index = 0; index < 6; index++)
    {
      // convert back to date and increase time of 4h
      BreakTime (current_time, current_dst);
      current_time += 14400;

      // display separation line and time
      graph_x = graph_left + shift_width + (index * unit_width);
      WSContentSend_P (PSTR ("<line x1='%u' y1='%u%%' x2='%u' y2='%u%%' />\n"), graph_x, 1, graph_x, 96);
      WSContentSend_P (PSTR ("<text x='%u' y='%u%%'>%02uh</text>\n"), graph_x - 15, 99, current_dst.hour);
    }
  }

  else if (period == PILOTWIRE_PERIOD_WEEK)
  {
    // calculate horizontal shift
    unit_width = graph_width / 7;

    // display day lines with day name
    for (index = 0; index < 7; index++)
    {
      // display week day separation line
      graph_x = graph_left + (index * unit_width);
      if (index > 0) WSContentSend_P (PSTR ("<line x1='%u' y1='%u%%' x2='%u' y2='%u%%' />\n"), graph_x, 1, graph_x, 99);
      GetTextIndexed (str_text, sizeof (str_text), index, kPilotwireWeekDay);
      WSContentSend_P (PSTR ("<text x='%u' y='%u%%'>%s</text>\n"), graph_x + 50, 99, str_text);
    }
  }  
}

// Get temperature attributes according to target temperature
void PilotwireWebGetTemperatureClass (float temperature, char* pstr_class, size_t size_class)
{
  // check prameters
  if ((pstr_class == nullptr) || (size_class < 6)) return;

  // set temperature color code
  if (isnan (temperature)) strcpy (pstr_class, "");
  else if (temperature <= PILOTWIRE_TEMP_SCALE_LOW) strlcpy (pstr_class, "low", size_class);
  else if (temperature >= PILOTWIRE_TEMP_SCALE_HIGH) strlcpy (pstr_class, "high", size_class);
  else strlcpy (pstr_class, "mid", size_class);
}

// get status update
void PilotwireWebUpdate ()
{
  float temperature;
  char  str_text[16];
  char  str_update[64];

  // update current temperature
  if (!isnan (pilotwire_status.temp_current)) ext_snprintf_P (str_text, sizeof (str_text), PSTR ("%1_f"), &pilotwire_status.temp_current);
    else strcpy_P (str_text, PSTR ("---"));
  strlcpy (str_update, str_text, sizeof (str_update));

  // update current temperature class
  strlcat (str_update, ";", sizeof (str_update));
  PilotwireWebGetTemperatureClass (pilotwire_status.temp_current, str_text, sizeof (str_text));
  strlcat (str_update, str_text, sizeof (str_update));

  // display status icon (window, flame, ...)
  strlcat (str_update, ";", sizeof (str_update));
  PilotwireWebIconStatus (str_text, sizeof (str_text), true);
  strlcat (str_update, str_text, sizeof (str_update));

  // display target thermometer icon
  temperature = PilotwireGetModeTemperature (pilotwire_config.mode);
  if (pilotwire_status.night_mode) temperature = min (temperature, PilotwireConvertTargetTemperature (PILOTWIRE_TARGET_NIGHT));
  PilotwireWebGetTemperatureClass (temperature, str_text, sizeof (str_text));
  strlcat (str_update, ";/", sizeof (str_update));
  strlcat (str_update, "therm-", sizeof (str_update));
  strlcat (str_update, str_text, sizeof (str_update));
  strlcat (str_update, ".png", sizeof (str_update));

  // send result
  Webserver->send_P (200, "text/plain", str_update);
}

// Pilot Wire heater public configuration web page
void PilotwireWebPageControl ()
{
  bool  is_logged;
  float temperature, value;
  char  str_class[16];
  char  str_value[48];

  // check if access is allowed
  is_logged = WebAuthenticate();

  // handle mode change
  if (Webserver->hasArg (D_CMND_PILOTWIRE_MODE))
  {
    WebGetArg (D_CMND_PILOTWIRE_MODE, str_value, sizeof (str_value));
    if (strlen (str_value) > 0) PilotwireSetMode (atoi (str_value));
  }

  // if target temperature has been changed
  if (Webserver->hasArg (D_CMND_PILOTWIRE_TARGET))
  {
    WebGetArg (D_CMND_PILOTWIRE_TARGET, str_value, sizeof (str_value));
    if (strlen (str_value) > 0) PilotwireSetCurrentTarget (atof (str_value));
  }

  // get current target temperature
  temperature = PilotwireGetModeTemperature (pilotwire_config.mode);
  if (pilotwire_status.night_mode) temperature = min (temperature, PilotwireConvertTargetTemperature (PILOTWIRE_TARGET_NIGHT));

  // beginning of page
  WSContentStart_P (D_PILOTWIRE_CONTROL, false);
  WSContentSend_P (PSTR ("</script>\n"));

  // page data refresh script
  WSContentSend_P (PSTR ("<script type='text/javascript'>\n"));
  WSContentSend_P (PSTR ("function update() {\n"));
  WSContentSend_P (PSTR ("httpUpd=new XMLHttpRequest();\n"));
  WSContentSend_P (PSTR ("httpUpd.onreadystatechange=function() {\n"));
  WSContentSend_P (PSTR (" if (httpUpd.responseText.length>0) {\n"));
  WSContentSend_P (PSTR ("  arr_param=httpUpd.responseText.split(';');\n"));
  WSContentSend_P (PSTR ("  if (arr_param[0]!='') {document.getElementById('temp').innerHTML=arr_param[0];}\n"));
  WSContentSend_P (PSTR ("  if (arr_param[1]!='') {document.getElementById('actual').className=arr_param[1]+' temp';}\n"));
  WSContentSend_P (PSTR ("  if (arr_param[2]!='') {document.getElementById('status').innerHTML=arr_param[2];}\n"));
  WSContentSend_P (PSTR ("  if (arr_param[3]!='') {document.getElementById('therm').setAttribute('src',arr_param[3]);}\n"));
  WSContentSend_P (PSTR (" }\n"));
  WSContentSend_P (PSTR ("}\n"));
  WSContentSend_P (PSTR ("httpUpd.open('GET','%s',true);\n"), D_PILOTWIRE_PAGE_UPDATE);
  WSContentSend_P (PSTR ("httpUpd.send();\n"));
  WSContentSend_P (PSTR ("}\n"));
  WSContentSend_P (PSTR ("setInterval(function() {update();},2000);\n"));
  WSContentSend_P (PSTR ("</script>\n"));

  // page style
  WSContentSend_P (PSTR ("<style>\n"));

  WSContentSend_P (PSTR ("body {background-color:%s;font-family:Arial, Helvetica, sans-serif;}\n"), PILOTWIRE_COLOR_BACKGROUND);
  WSContentSend_P (PSTR ("div {color:white;font-size:1.5rem;text-align:center;vertical-align:middle;margin:10px auto;}\n"));
  WSContentSend_P (PSTR ("div a {color:white;text-decoration:none;}\n"));

  WSContentSend_P (PSTR ("div.inline {display:inline-block;}\n"));
  WSContentSend_P (PSTR ("div.title {font-weight:bold;}\n"));

  WSContentSend_P (PSTR ("div.temp {font-size:3rem;font-weight:bold;}\n"));

  WSContentSend_P (PSTR (".low {color:%s;}\n"), PILOTWIRE_COLOR_LOW);
  WSContentSend_P (PSTR (".mid {color:%s;}\n"), PILOTWIRE_COLOR_MEDIUM);
  WSContentSend_P (PSTR (".high {color:%s;}\n"), PILOTWIRE_COLOR_HIGH);

  WSContentSend_P (PSTR ("div.pix img {width:50%%;max-width:350px;border-radius:12px;}\n"));
  WSContentSend_P (PSTR ("div.status {font-size:2.5rem;margin-top:20px}\n"));

  WSContentSend_P (PSTR ("div.therm {font-size:2.5rem;}\n"));
  WSContentSend_P (PSTR ("select {background:none;color:#ddd;font-size:2rem;border:none;}\n"));

  WSContentSend_P (PSTR ("div.item {padding:0.2rem;margin:0px;border:none;}\n"));
  WSContentSend_P (PSTR ("a div.item:hover {background:#aaa;}\n"));

  WSContentSend_P (PSTR ("div.adjust {width:240px;font-size:2rem;}\n"));
  WSContentSend_P (PSTR ("div.adjust div.item {width:30px;color:#ddd;border:1px #444 solid;border-radius:8px;}\n"));

  WSContentSend_P (PSTR ("div.mode {padding:2px;margin-bottom:50px;border:1px #666 solid;border-radius:8px;}\n"));
  WSContentSend_P (PSTR ("div.mode div {background:#666;}\n"));
  WSContentSend_P (PSTR ("div.mode a div {background:none;}\n"));
  WSContentSend_P (PSTR ("div.mode div.item {width:60px;border-radius:6px;}\n"));

  WSContentSend_P (PSTR ("</style>\n"));
  WSContentSend_P (PSTR ("</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (PSTR ("<form method='post' action='/control'>\n"));

  // room name
  WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText (SET_DEVICENAME));

  // actual temperature
  strcpy_P (str_value, PSTR ("---"));
  if (!isnan (pilotwire_status.temp_current)) ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &pilotwire_status.temp_current);
  PilotwireWebGetTemperatureClass (pilotwire_status.temp_current, str_class, sizeof (str_class));
  WSContentSend_P (PSTR ("<div id='actual' class='%s temp'><span id='temp'>%s</span><small> °C</small></div>\n"), str_class, str_value);

  // room picture
  WSContentSend_P (PSTR ("<div class='section pix'>"));
  if (is_logged) WSContentSend_P (PSTR ("<a href='/'>"));

  strcpy (str_value, PILOTWIRE_ICON_DEFAULT);
#ifdef USE_UFILESYS
  if (ffsp->exists (PILOTWIRE_ICON_LOGO)) strcpy (str_value, PILOTWIRE_ICON_LOGO);
#endif    // USE_UFILESYS
  WSContentSend_P (PSTR ("<img src='%s'>"), str_value);

  if (is_logged) WSContentSend_P (PSTR ("</a>"));
  WSContentSend_P (PSTR ("</div>\n"));

  // status icon
  PilotwireWebIconStatus (str_value, sizeof (str_value), true);
  WSContentSend_P (PSTR ("<div class='section status' id='status'>%s</div>\n"), str_value);

  // section : target 
  WSContentSend_P (PSTR ("<div class='section target'>\n"));

  // thermometer
  if (pilotwire_config.mode != PILOTWIRE_MODE_OFF) strcpy (str_class, "🌡");
    else strcpy (str_class, "&nbsp;");
  WSContentSend_P (PSTR ("<div class='inline therm'>%s</div>\n"), str_class);

  // if heater in thermostat mode, display selection
  if ((pilotwire_config.mode == PILOTWIRE_MODE_COMFORT) && !pilotwire_status.night_mode)
  {
    WSContentSend_P (PSTR ("<div class='inline adjust'>\n"));

    // button -0.5
    value = temperature - 0.5;
    ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &value);
    WSContentSend_P (PSTR ("<a href='/%s?%s=%s'><div class='inline item'>%s</div></a>\n"), D_PILOTWIRE_PAGE_CONTROL, D_CMND_PILOTWIRE_TARGET, str_value, "-");

    // target selection
    WSContentSend_P (PSTR ("<select name='%s' id='%s' onchange='this.form.submit();'>\n"), D_CMND_PILOTWIRE_TARGET, D_CMND_PILOTWIRE_TARGET);
    for (value = pilotwire_config.arr_temp[PILOTWIRE_TARGET_LOW]; value <= pilotwire_config.arr_temp[PILOTWIRE_TARGET_HIGH]; value += 0.5)
    {
      ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%01_f"), &value);
      if (value == temperature) strcpy (str_class, "selected"); else strcpy (str_class, "");
      WSContentSend_P (PSTR ("<option value='%s' %s>%s °C</option>\n"), str_value, str_class, str_value);
    }
    WSContentSend_P (PSTR ("</select>\n"));

    // button +0.5
    value = temperature + 0.5;
    ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &value);
    WSContentSend_P (PSTR ("<a href='/%s?%s=%s'><div class='inline item'>%s</div></a>\n"), D_PILOTWIRE_PAGE_CONTROL, D_CMND_PILOTWIRE_TARGET, str_value, "+");

    WSContentSend_P (PSTR ("</div>\n"));    // inline adjust
  }

  // else, if heater is not in forced mode
  else if (pilotwire_config.mode != PILOTWIRE_MODE_OFF)
  {
    ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &temperature);
    WSContentSend_P (PSTR ("<div class='inline adjust %s'>%s<small> °C</small></div>\n"), str_class, str_value);
  }

  WSContentSend_P (PSTR ("</div>\n"));    // section target

  // section : mode 
  WSContentSend_P (PSTR ("<div class='section'>\n"));
  WSContentSend_P (PSTR ("<div class='inline mode'>\n"));

  // OFF
  if (pilotwire_config.mode == PILOTWIRE_MODE_OFF) WSContentSend_P (PSTR ("<div class='inline item' title='%s'>❄</div>\n"), D_PILOTWIRE_OFF);
    else WSContentSend_P (PSTR ("<a href='/%s?%s=%u'><div class='inline item' title='%s'>❄</div></a>\n"), D_PILOTWIRE_PAGE_CONTROL, D_CMND_PILOTWIRE_MODE, PILOTWIRE_MODE_OFF, D_PILOTWIRE_OFF);

  // ECO
  if (pilotwire_config.mode == PILOTWIRE_MODE_ECO) WSContentSend_P (PSTR ("<div class='inline item' title='%s'>🌙</div>\n"), D_PILOTWIRE_ECO);
    else WSContentSend_P (PSTR ("<a href='/%s?%s=%u'><div class='inline item' title='%s'>🌙</div></a>\n"), D_PILOTWIRE_PAGE_CONTROL, D_CMND_PILOTWIRE_MODE, PILOTWIRE_MODE_ECO, D_PILOTWIRE_ECO);

  // NIGHT
  if ((pilotwire_config.mode == PILOTWIRE_MODE_COMFORT) && pilotwire_status.night_mode) WSContentSend_P (PSTR ("<div class='inline item' title='%s'>💤</div>\n"), D_PILOTWIRE_NIGHT);
    else WSContentSend_P (PSTR ("<a href='/%s?%s=%u'><div class='inline item' title='%s'>💤</div></a>\n"), D_PILOTWIRE_PAGE_CONTROL, D_CMND_PILOTWIRE_MODE, PILOTWIRE_MODE_NIGHT, D_PILOTWIRE_NIGHT);

  // COMFORT
  if ((pilotwire_config.mode == PILOTWIRE_MODE_COMFORT) && !pilotwire_status.night_mode) WSContentSend_P (PSTR ("<div class='inline item' title='%s'>🔆</div>\n"), D_PILOTWIRE_COMFORT);
    else WSContentSend_P (PSTR ("<a href='/%s?%s=%u'><div class='inline item' title='%s'>🔆</div></a>\n"), D_PILOTWIRE_PAGE_CONTROL, D_CMND_PILOTWIRE_MODE, PILOTWIRE_MODE_COMFORT, D_PILOTWIRE_COMFORT);

  WSContentSend_P (PSTR ("</div>\n"));    // inline mode

  // end of form
  WSContentSend_P (PSTR ("</form>\n"));

  // end of page
  WSContentStop ();
}

#endif  // USE_WEBSERVER

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xsns126 (uint8_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  {
    case FUNC_INIT:
      PilotwireInit ();
      break;
    case FUNC_EVERY_100_MSECOND:
      PilotwireEvery100ms ();
      break;
     case FUNC_EVERY_SECOND:
      if (TasmotaGlobal.uptime > 5) PilotwireEverySecond ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kPilotwireCommand, PilotwireCommand);
      break;
    case FUNC_JSON_APPEND:
      PilotwireShowJSON (false);
      break;

#ifdef USE_UFILESYS
    case FUNC_SAVE_AT_MIDNIGHT:
      PilotwireRotateAtMidnight ();
      break;
#endif    // USE_UFILESYS

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:

      // icons
      Webserver->on (PILOTWIRE_ICON_DEFAULT, PilotwireWebIconDefault);
#ifdef USE_UFILESYS
      Webserver->on (PILOTWIRE_ICON_LOGO, PilotwireWebIconLogo);
#endif    // USE_UFILESYS

      // pages
      Webserver->on ("/" D_PILOTWIRE_PAGE_STATUS,   PilotwireWebPageSwitchStatus);
      Webserver->on ("/" D_PILOTWIRE_PAGE_CONFIG,   PilotwireWebPageConfigure);
      Webserver->on ("/" D_PILOTWIRE_PAGE_CONTROL,  PilotwireWebPageControl);
      Webserver->on ("/" D_PILOTWIRE_PAGE_UPDATE,   PilotwireWebUpdate);
      Webserver->on ("/" D_PILOTWIRE_PAGE_GRAPH,    PilotwireWebPageGraph);
      break;
    case FUNC_WEB_SENSOR:
      PilotwireWebSensor ();
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      PilotwireWebMainButton ();
      break;
    case FUNC_WEB_ADD_BUTTON:
      WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>Configure %s</button></form></p>\n"), D_PILOTWIRE_PAGE_CONFIG, D_PILOTWIRE);
      break;
#endif  // USE_Webserver

  }
  return result;
}

#endif // USE_PILOTWIRE
