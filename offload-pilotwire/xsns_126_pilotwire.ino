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
  
  If LittleFS partition is available :
    - settings are stored in /pilotwire.cfg and /offload.cfg
    - room picture can be stored as /logo.png

  If no LittleFS partition is available, settings are stored using knx_CB_addr and knx_GA_addr parameters.
  Absolute temperature are stored as x10 (12.5 °C = 125)
  Relative temperature are stored as positive or negative
    negative : 1000 - x10 (900 = -10°C, 950 = -5°C, ...)
    positive : 1000 + x10 (1100 = 10°C, 1200 = 20°C, ...)

    - Settings.knx_CB_addr[0] = Device mode (Off, On, Comfort, ...)
    - Settings.knx_CB_addr[1] = Device type (pilotewire or direct)

    - Settings.knx_CB_addr[2] = Lowest acceptable temperature (absolute)
    - Settings.knx_CB_addr[3] = Highest acceptable temperature (absolute)
    - Settings.knx_CB_addr[4] = Comfort target temperature (absolute)
    - Settings.knx_CB_addr[5] = Night mode target temperature (relative)
    - Settings.knx_CB_addr[6] = Eco mode target temperature (relative)
    - Settings.knx_CB_addr[7] = NoFrost mode target temperature (relative)
    - Settings.knx_CB_addr[8] = Ecowatt level 2 target temperature (relative)
    - Settings.knx_CB_addr[9] = Ecowatt level 3 target temperature (relative)

    - Settings.knx_GA_addr[7] = Ecowatt management flag
    - Settings.knx_GA_addr[8] = Open Window detection flag
    - Settings.knx_GA_addr[9] = Presence detection flag

  How to enable Movement Detection :
    - connect a sensor like RCWM-0516
    - declare its GPIO as Input 0

  How to enable Window Opened Detection :
    - enable option it in Configuration Pilotwire
    - if temperature drops of 0.5°C in less than 2mn, window is considered as opened, heater stops
    - if temperature increases of 0.2°C in less than 10mn, window is considered as closed again, heater restart

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

#define D_PAGE_PILOTWIRE_CONFIG       "pw-cfg"
#define D_PAGE_PILOTWIRE_STATUS       "pw-status"
#define D_PAGE_PILOTWIRE_CONTROL      "control"
#define D_PAGE_PILOTWIRE_UPDATE       "control.upd"
#define D_PAGE_PILOTWIRE_GRAPH        "graph"
#define D_PAGE_PILOTWIRE_BASE_SVG     "graph-base.svg"
#define D_PAGE_PILOTWIRE_DATA_SVG     "graph-data.svg"

#define D_CMND_PILOTWIRE_HELP         "help"
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
#define D_CMND_PILOTWIRE_ECOWATT2     "eco2"
#define D_CMND_PILOTWIRE_ECOWATT3     "eco3"

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

// configuration file
#define D_PILOTWIRE_FILE_CFG          "/pilotwire.cfg"

// history files label and filename
const char D_PILOTWIRE_HISTO_FILE_WEEK[] PROGMEM = "/log-week-%d.csv";

#endif    // USE_UFILESYS

// constant : power
#define PILOTWIRE_POWER_MINIMUM           100      // minimum power to consider heater on (in case of direct connexion)

// constant : temperature
#define PILOTWIRE_TEMP_THRESHOLD          0.25      // temperature threshold to switch on/off (°C)
#define PILOTWIRE_TEMP_STEP               0.5       // temperature selection step (°C)
#define PILOTWIRE_TEMP_UPDATE             5         // temperature update delay (s)
#define PILOTWIRE_TEMP_LIMIT_MIN          6         // minimum acceptable temperature
#define PILOTWIRE_TEMP_LIMIT_MAX          30        // maximum acceptable temperature
#define PILOTWIRE_TEMP_DEFAULT_LOW        12        // low range selectable temperature
#define PILOTWIRE_TEMP_DEFAULT_HIGH       22        // high range selectable temperature
#define PILOTWIRE_TEMP_DEFAULT_NORMAL     18        // default temperature
#define PILOTWIRE_TEMP_DEFAULT_NIGHT      -1        // default night mode adjustment
#define PILOTWIRE_TEMP_DEFAULT_ECO        -3        // default eco mode adjustment
#define PILOTWIRE_TEMP_DEFAULT_NOFROST    7         // default no frost mode adjustment
#define PILOTWIRE_TEMP_SCALE_LOW          19        // temperature under 19 is energy friendly
#define PILOTWIRE_TEMP_SCALE_HIGH         21        // temperature above 21 is energy wastage

// constant : open window detection
#define PILOTWIRE_WINDOW_SAMPLE_NBR       24        // number of temperature samples to detect opened window (4mn for 1 sample every 10s)
#define PILOTWIRE_WINDOW_OPEN_PERIOD      10        // delay between 2 temperature samples in open window detection 
#define PILOTWIRE_WINDOW_OPEN_DROP        0.5       // temperature drop for window open detection (°C)
#define PILOTWIRE_WINDOW_CLOSE_INCREASE   0.2       // temperature increase to detect window closed (°C)  

// constant : presence detection
#define PILOTWIRE_PRESENCE_IGNORE               30        // time window to ignore presence detection, to avoid relay change perturbation (sec)
#define PILOTWIRE_PRESENCE_TIMEOUT              5         // timeout to consider current presence (sec)
#define PILOTWIRE_PRESENCE_TIMEOUT_SHORT        60        // timeout (60 mn) after last movement detection to switch to ECO mode 
#define PILOTWIRE_PRESENCE_TIMEOUT_MISSING      2880      // timeout (48 hr) after last movement detection to switch to NOFROST mode 

// constant : graph
#define PILOTWIRE_GRAPH_SAMPLE            576       // number of graph points 
#define PILOTWIRE_GRAPH_WIDTH             576       // width of graph (pixels)
#define PILOTWIRE_GRAPH_HEIGHT            500       // height of graph (pixels)
#define PILOTWIRE_GRAPH_PERCENT_START     8         // percent of display before graph
#define PILOTWIRE_GRAPH_PERCENT_STOP      92        // percent of display after graph

// Historic data files
#define PILOTWIRE_HISTO_MIN_FREE          20        // minimum free size on filesystem (Kb)
#define PILOTWIRE_HISTO_WEEK_MAX          4         // number of weekly histotisation files

// constant chains
const char D_CONF_FIELDSET_START[] PROGMEM = "<fieldset><legend><b>&nbsp;%s %s&nbsp;</b></legend>\n";
const char D_CONF_FIELDSET_STOP[]  PROGMEM = "</fieldset><br />\n";
const char D_CONF_MODE_SELECT[]    PROGMEM = "<input type='radio' name='%s' id='%d' value='%d' %s>%s<br>\n";
const char D_GRAPH_SEPARATION[]    PROGMEM = "<line class='dash' x1='%d%%' y1='%d%%' x2='%d%%' y2='%d%%' />\n";
const char D_GRAPH_TEMPERATURE[]   PROGMEM = "<text class='temperature' x='%d%%' y='%d%%'>%s</text>\n";
const char D_CONF_FIELD_FULL[]     PROGMEM = "<p>%s %s<span class='key'>%s</span><br><input type='number' name='%s' min='%d' max='%d' step='%s' value='%s'></p>\n";

// device control
enum PilotwireDevices { PILOTWIRE_DEVICE_NORMAL, PILOTWIRE_DEVICE_DIRECT, PILOTWIRE_DEVICE_MAX };
const char kPilotwireDevice[] PROGMEM = "Pilotwire|Direct";                                                                 // device type labels

enum PilotwireMode { PILOTWIRE_MODE_OFF, PILOTWIRE_MODE_ECO, PILOTWIRE_MODE_COMFORT, PILOTWIRE_MODE_FORCED, PILOTWIRE_MODE_MAX };
const char kPilotwireModeIcon[] PROGMEM = "❌|🌙|🔆|🔥";                          // running mode icons
const char kPilotwireModeLabel[] PROGMEM = "Off|Eco|Confort|Forcé";               // running mode labels

enum PilotwireTarget { PILOTWIRE_TARGET_LOW, PILOTWIRE_TARGET_HIGH, PILOTWIRE_TARGET_FORCED, PILOTWIRE_TARGET_NOFROST, PILOTWIRE_TARGET_COMFORT, PILOTWIRE_TARGET_ECO, PILOTWIRE_TARGET_NIGHT, PILOTWIRE_TARGET_ECOWATT2, PILOTWIRE_TARGET_ECOWATT3, PILOTWIRE_TARGET_MAX };
const char kPilotwireTargetIcon[] PROGMEM = "➖|➕|🔥|❄|🔆|🌙|💤|🟠|🔴";                                                             // target temperature icons
const char kPilotwireTargetLabel[] PROGMEM = "Minimale|Maximale|Forcée|Hors gel|Confort|Eco|Nuit|Risque Ecowatt|Coupure Ecowatt";           // target temperature labels

enum PilotwirePresence { PILOTWIRE_PRESENCE_DETECTED, PILOTWIRE_PRESENCE_SHORT, PILOTWIRE_PRESENCE_MISSING, PILOTWIRE_PRESENCE_MAX };

// graph periods
#ifdef USE_UFILESYS
  enum PilotwireGraphPeriod { PILOTWIRE_PERIOD_LIVE, PILOTWIRE_PERIOD_WEEK, PILOTWIRE_PERIOD_MAX };                 // available periods
  const char kPilotwireGraphPeriod[] PROGMEM = "Live|Week";                                                         // period labels
  const long ARR_PILOTWIRE_PERIOD_SAMPLE[] = { 3600 / PILOTWIRE_GRAPH_SAMPLE, 604800 / PILOTWIRE_GRAPH_SAMPLE };    // number of seconds between samples
#else
  enum PilotwireGraphPeriod { PILOTWIRE_PERIOD_LIVE, PILOTWIRE_PERIOD_MAX };                                        // available periods
  const char kPilotwireGraphPeriod[] PROGMEM = "Live";                                                              // period labels
  const long ARR_PILOTWIRE_PERIOD_SAMPLE[] = { 3600 / PILOTWIRE_GRAPH_SAMPLE };                                     // number of seconds between samples
#endif    // USE_UFILESYS

// pilotwire commands
const char kPilotwireCommand[] PROGMEM = "pw_" "|" D_CMND_PILOTWIRE_HELP "|" D_CMND_PILOTWIRE_TYPE "|" D_CMND_PILOTWIRE_MODE "|" D_CMND_PILOTWIRE_ECOWATT "|" D_CMND_PILOTWIRE_WINDOW "|" D_CMND_PILOTWIRE_PRESENCE "|" D_CMND_PILOTWIRE_LOW "|" D_CMND_PILOTWIRE_HIGH "|" D_CMND_PILOTWIRE_NOFROST "|" D_CMND_PILOTWIRE_COMFORT "|" D_CMND_PILOTWIRE_ECO "|" D_CMND_PILOTWIRE_NIGHT;
void (* const PilotwireCommand[])(void) PROGMEM = { &CmndPilotwireHelp, &CmndPilotwireType, &CmndPilotwireMode, &CmndPilotwireEcowatt, &CmndPilotwireWindow, &CmndPilotwirePresence, &CmndPilotwireTargetLow, &CmndPilotwireTargetHigh, &CmndPilotwireTargetNofrost, &CmndPilotwireTargetComfort, &CmndPilotwireTargetEco, &CmndPilotwireTargetNight };

/****************************************\
 *               Icons
 * 
 *      xxd -i -c 256 icon.png
\****************************************/

// icon : none
#define PILOTWIRE_ICON_LOGO       "/room.jpg"

#define PILOTWIRE_ICON_DEFAULT    "/default.png"      
char pilotwire_default_png[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x04, 0x03, 0x00, 0x00, 0x00, 0x31, 0x10, 0x7c, 0xf8, 0x00, 0x00, 0x00, 0x0f, 0x50, 0x4c, 0x54, 0x45, 0x00, 0x00, 0x00, 0x34, 0x34, 0x34, 0x74, 0x74, 0x74, 0xaf, 0xaf, 0xaf, 0xff, 0xff, 0xff, 0x33, 0x73, 0x8d, 0x9c, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x01, 0x9b, 0x49, 0x44, 0x41, 0x54, 0x68, 0xde, 0xed, 0x99, 0x51, 0xae, 0x83, 0x20, 0x10, 0x45, 0xcb, 0x0e, 0x18, 0x74, 0x03, 0x98, 0x6e, 0xc0, 0xc6, 0x0d, 0x54, 0xd9, 0xff, 0x9a, 0x1e, 0x33, 0x08, 0x0e, 0x5a, 0xc1, 0x04, 0x3f, 0xfa, 0x9a, 0x21, 0xa6, 0x89, 0xde, 0xcb, 0x19, 0xa4, 0xe0, 0x07, 0xf7, 0xf1, 0xf8, 0x9e, 0x06, 0x9a, 0xdd, 0x28, 0xb8, 0x28, 0xb1, 0xc7, 0xbe, 0x6d, 0x3d, 0x7c, 0xd3, 0x17, 0x24, 0x5e, 0x04, 0x98, 0x8d, 0xba, 0x24, 0x5b, 0x41, 0xe2, 0x03, 0x98, 0xc6, 0xfe, 0x15, 0x05, 0xe8, 0x16, 0x58, 0x2c, 0x54, 0x25, 0x3e, 0x80, 0xde, 0x8d, 0xfe, 0x82, 0xb5, 0x8b, 0x5b, 0xf0, 0xd2, 0x35, 0x89, 0x0f, 0xc0, 0xa1, 0x6b, 0x81, 0xd8, 0x65, 0xc1, 0x07, 0xba, 0x22, 0x71, 0x40, 0x47, 0x2e, 0x67, 0x49, 0x80, 0x89, 0x5c, 0x33, 0x54, 0x24, 0xfe, 0x06, 0xcf, 0xe0, 0x7a, 0xa3, 0x4b, 0x19, 0x47, 0xae, 0x50, 0x34, 0x4a, 0x23, 0x97, 0xdc, 0x01, 0x30, 0x05, 0x17, 0x91, 0xb1, 0x26, 0xb9, 0xa8, 0x68, 0x41, 0xe2, 0x00, 0x17, 0x5c, 0x54, 0x54, 0xf5, 0xab, 0x8b, 0x8a, 0x16, 0x24, 0x36, 0x05, 0x66, 0x75, 0xd1, 0xd0, 0x70, 0xd0, 0xe4, 0xc2, 0x17, 0x2a, 0x48, 0xbb, 0x39, 0x64, 0xae,
  0x89, 0x03, 0xce, 0x25, 0xbc, 0x6f, 0x04, 0x84, 0x05, 0x8a, 0x4b, 0x73, 0x73, 0x61, 0x4b, 0xae, 0x8a, 0x04, 0x30, 0xf8, 0x15, 0xa5, 0x1a, 0x00, 0x06, 0x27, 0x53, 0xc1, 0x30, 0xd8, 0xcd, 0x65, 0x86, 0x81, 0xb9, 0x8a, 0x12, 0x6a, 0x08, 0xf0, 0x8e, 0x79, 0x73, 0x79, 0x87, 0x4d, 0xae, 0xb2, 0x84, 0x3f, 0x02, 0xb8, 0x09, 0xd0, 0xb0, 0x0e, 0x9a, 0x01, 0x26, 0x6e, 0x6b, 0xb6, 0xe0, 0x35, 0x5f, 0xf0, 0x50, 0x94, 0xa8, 0xf6, 0x1d, 0x9b, 0xa9, 0x65, 0x3b, 0x37, 0x7f, 0x0f, 0x04, 0x20, 0x00, 0x01, 0x08, 0x40, 0x00, 0x02, 0x10, 0x80, 0x00, 0x04, 0x20, 0x00, 0x01, 0x08, 0x40, 0x00, 0x02, 0x10, 0x80, 0x00, 0x04, 0xf0, 0x1f, 0x00, 0xf7, 0x9c, 0xea, 0xb6, 0x1c, 0x85, 0xdd, 0x74, 0xaa, 0xdb, 0x7a, 0x1c, 0x28, 0x67, 0xaa, 0x72, 0xaa, 0xfb, 0x43, 0xa7, 0xba, 0xcd, 0x31, 0x51, 0x96, 0x46, 0x01, 0x4f, 0xa3, 0x12, 0xe0, 0x28, 0x7d, 0x4a, 0xba, 0xe6, 0x2c, 0xce, 0xb2, 0xba, 0x2c, 0x7d, 0xc8, 0xda, 0xde, 0x59, 0xa0, 0x06, 0x99, 0xa4, 0x0f, 0x52, 0x9e, 0x57, 0xb2, 0x48, 0x0f, 0xb2, 0xb4, 0xef, 0x5c, 0xda, 0xcd, 0x22, 0xa5, 0x92, 0x7a, 0x4d, 0x08, 0x29, 0x95, 0x84, 0x8a, 0xb4, 0x0b, 0x1c, 0x59, 0x2e, 0x6a, 0xb2, 0xc8, 0xf4, 0x5c, 0xe2, 0x00, 0xff, 0x79, 0xeb, 0xe3, 0x73, 0x05, 0x4f, 0xef, 0x8a, 0x55, 0x0a, 0x52, 0xf6, 0x47, 0x1a, 0xdb, 0xa5, 0x6c, 0x18, 0x43, 0xb4, 0x31, 0x56, 0x29, 0x48, 0x87, 0xe0, 0x5a, 0x67, 0x29, 0xf4, 0x05, 0x69, 0x47, 0xd0, 0x59, 0x8e, 0x7d, 0x49, 0xfa, 0x86, 0xf6, 0x07, 0x1a, 0x58, 0x22, 0xd7, 0xc3, 0x2c, 0x71, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
unsigned int pilotwire_default_len = 508;

/*****************************************\
 *               Variables
\*****************************************/

// pilotwire : configuration
struct {
  bool    window   = false;                             // flag to enable window open detection
  bool    presence = false;                             // flag to enable movement detection
  bool    ecowatt  = true;                              // flag to enable ecowatt management
  uint8_t type     = UINT8_MAX;                         // pilotwire or direct connexion
  uint8_t mode     = UINT8_MAX;                         // running mode (off, on, thermostat)
  float   arr_temp[PILOTWIRE_TARGET_MAX];               // array of target temperatures (according to status)
} pilotwire_config;

// pilotwire : general status
struct {
  bool    night_mode    = false;                            // flag to indicate night mode
  bool    json_update   = false;                            // flag to publish a json update
  uint8_t mode          = PILOTWIRE_MODE_OFF;               // actual device mùode, at start device is OFF
  uint8_t presence      = PILOTWIRE_PRESENCE_DETECTED;      // presence detection status
  float   temp_json     = PILOTWIRE_TEMP_DEFAULT_NORMAL;    // last published temperature
  float   temp_current  = NAN;                              // current temperature
  float   temp_target   = NAN;                              // target temperature
} pilotwire_status;

// pilotwire : open window detection
struct {
  bool    opened   = false;                             // open window active
  float   low_temp = NAN;                               // lower temperature during open window phase
  uint8_t period   = 0;                                 // period for temperature measurement (sec)
  uint8_t idx_temp = 0;                                 // current index of temperature array
  float   arr_temp[PILOTWIRE_WINDOW_SAMPLE_NBR];        // array of values to detect open window
} pilotwire_window;

// pilotwire : movement detection
struct {
  uint32_t time_ignore = UINT32_MAX;                    // if set, movement should be ignored till timestamp
  uint32_t delay = 0;                                   // delay since last movement detection (in sec.)
} pilotwire_presence;

// pilotwire : graph data
struct {
  float    temp_min = NAN;                                                // graph minimum temperature
  float    temp_max = NAN;                                                // graph maximum temperature
  uint8_t  device_state[PILOTWIRE_PERIOD_MAX];                            // graph periods device state
  uint32_t index[PILOTWIRE_PERIOD_MAX];                                   // current graph index for data
  uint32_t counter[PILOTWIRE_PERIOD_MAX];                                 // graph update counter
  float    temperature[PILOTWIRE_PERIOD_MAX];                             // graph current temperature
  short    arr_temp[PILOTWIRE_PERIOD_MAX][PILOTWIRE_GRAPH_SAMPLE];        // graph temperature array (value x 10)
  short    arr_target[PILOTWIRE_PERIOD_MAX][PILOTWIRE_GRAPH_SAMPLE];      // graph target temperature array (value x 10)
  uint8_t  arr_state[PILOTWIRE_PERIOD_MAX][PILOTWIRE_GRAPH_SAMPLE];       // graph command state array
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

  AddLog (LOG_LEVEL_INFO, PSTR (" - pw_%s = enable open window mode (0/1)"), D_CMND_PILOTWIRE_WINDOW);
  AddLog (LOG_LEVEL_INFO, PSTR (" - pw_%s = enable presence detection (0/1)"), D_CMND_PILOTWIRE_PRESENCE);

  AddLog (LOG_LEVEL_INFO, PSTR (" - pw_%s = lowest temperature (°C)"), D_CMND_PILOTWIRE_LOW);
  AddLog (LOG_LEVEL_INFO, PSTR (" - pw_%s = highest temperature (°C)"), D_CMND_PILOTWIRE_HIGH);

  AddLog (LOG_LEVEL_INFO, PSTR (" - pw_%s = comfort temperature (°C)"), D_CMND_PILOTWIRE_COMFORT);
  AddLog (LOG_LEVEL_INFO, PSTR (" - pw_%s = no frost temperature (delta with comfort if negative)"), D_CMND_PILOTWIRE_NOFROST);
  AddLog (LOG_LEVEL_INFO, PSTR (" - pw_%s = eco temperature (delta with comfort if negative)"), D_CMND_PILOTWIRE_ECO);
  AddLog (LOG_LEVEL_INFO, PSTR (" - pw_%s = night temperature (delta with comfort if negative)"), D_CMND_PILOTWIRE_NIGHT);

  ResponseCmndDone();
}

void CmndPilotwireType ()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetDeviceType ((uint8_t)XdrvMailbox.payload, true);
  ResponseCmndNumber (pilotwire_config.type);
}

void CmndPilotwireMode ()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetMode ((uint8_t)XdrvMailbox.payload);
  ResponseCmndNumber (pilotwire_config.mode);
}

void CmndPilotwireEcowatt ()
{
  if (XdrvMailbox.data_len > 0) PilotwireEcowattEnable (XdrvMailbox.payload != 0);
  ResponseCmndNumber (pilotwire_config.ecowatt);
}

void CmndPilotwireWindow ()
{
  if (XdrvMailbox.data_len > 0) PilotwireWindowEnable (XdrvMailbox.payload != 0);
  ResponseCmndNumber (pilotwire_config.window);
}

void CmndPilotwirePresence ()
{
  if (XdrvMailbox.data_len > 0) PilotwirePresenceEnable (XdrvMailbox.payload != 0);
  ResponseCmndNumber (pilotwire_config.presence);
}

void CmndPilotwireTargetLow ()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetTargetTemperature (PILOTWIRE_TARGET_LOW, atof(XdrvMailbox.data), true);
  ResponseCmndFloat (pilotwire_config.arr_temp[PILOTWIRE_TARGET_LOW], 1);
}

void CmndPilotwireTargetHigh ()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetTargetTemperature (PILOTWIRE_TARGET_HIGH, atof(XdrvMailbox.data), true);
  ResponseCmndFloat (pilotwire_config.arr_temp[PILOTWIRE_TARGET_HIGH], 1);
}

void CmndPilotwireTargetNofrost ()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetTargetTemperature (PILOTWIRE_TARGET_NOFROST, atof(XdrvMailbox.data), true);
  ResponseCmndFloat (pilotwire_config.arr_temp[PILOTWIRE_TARGET_NOFROST], 1);
}

void CmndPilotwireTargetComfort ()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetTargetTemperature (PILOTWIRE_TARGET_COMFORT, atof(XdrvMailbox.data), true);
  ResponseCmndFloat (pilotwire_config.arr_temp[PILOTWIRE_TARGET_COMFORT], 1);
}

void CmndPilotwireTargetEco ()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetTargetTemperature (PILOTWIRE_TARGET_ECO, atof(XdrvMailbox.data), true);
  ResponseCmndFloat (pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECO], 1);
}

void CmndPilotwireTargetNight ()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetTargetTemperature (PILOTWIRE_TARGET_NIGHT, atof(XdrvMailbox.data), true);
  ResponseCmndFloat (pilotwire_config.arr_temp[PILOTWIRE_TARGET_NIGHT], 1);
}

/******************************************************\
 *                  Main functions
\******************************************************/

// get pilotwire device state from relay state
bool PilotwireGetDeviceState ()
{
  bool state = false;
  bool relay = bitRead (TasmotaGlobal.power, 0);

  // if pilotwire connexion, convert to pilotwire state
  if ((pilotwire_config.type == PILOTWIRE_DEVICE_NORMAL) && !relay) state = true;

  // else, direct connexion, convert to pilotwire state
  else if ((pilotwire_config.type == PILOTWIRE_DEVICE_DIRECT) && relay) state = true;

  return state;
}

// set relays state
void PilotwireSetDeviceState (const bool new_state)
{
  bool relay_state  = false;
  bool relay_active = bitRead (TasmotaGlobal.power, 0);

  // calculate relay state
  if ((pilotwire_config.type == PILOTWIRE_DEVICE_DIRECT) && relay_active) relay_state = true;
  else if ((pilotwire_config.type == PILOTWIRE_DEVICE_NORMAL) && !relay_active) relay_state = true;

  // if relay needs to be toggled
  if (relay_state != new_state)
  {
    // disable presence detection to avoid false positive*
    pilotwire_presence.time_ignore = LocalTime () + PILOTWIRE_PRESENCE_IGNORE;

    // toggle relay
    if (new_state) ExecuteCommandPower (1, POWER_ON, SRC_MAX);
      else ExecuteCommandPower (1, POWER_OFF,  SRC_MAX);

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
float PilotwireGetModeTargetTemperature (const uint8_t mode)
{
  float temperature;
  
  // set initial temperature according to running mode
  switch (mode)
  {
    case PILOTWIRE_MODE_ECO:
      temperature = PilotwireConvertTargetTemperature (PILOTWIRE_TARGET_ECO);
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
  uint8_t level;
  float   temperature;
  
  // set initial temperature according to running mode
  temperature = PilotwireGetModeTargetTemperature (pilotwire_config.mode);

  // read ecowatt level
  level = PilotwireEcowattGetLevel ();

  // if main supply is offloaded, target is no frost
  if (OffloadIsOffloaded ()) temperature = min (temperature, PilotwireConvertTargetTemperature (PILOTWIRE_TARGET_NOFROST));

  // if window opened, target is no frost
  if (pilotwire_window.opened) temperature = min (temperature, PilotwireConvertTargetTemperature (PILOTWIRE_TARGET_NOFROST));

  // if night mode, update target
  if (pilotwire_status.night_mode) temperature = min (temperature, PilotwireConvertTargetTemperature (PILOTWIRE_TARGET_NIGHT));

  // if presence not detected for a short time, update target to Eco
  if (pilotwire_status.presence == PILOTWIRE_PRESENCE_SHORT) temperature = min (temperature, PilotwireConvertTargetTemperature (PILOTWIRE_TARGET_ECO));

  // else if presence not detected for a long time, update target to No Frost
  else if (pilotwire_status.presence == PILOTWIRE_PRESENCE_MISSING) temperature = min (temperature, PilotwireConvertTargetTemperature (PILOTWIRE_TARGET_NOFROST));

  // if ecowatt level 2 (warning) is detected
  if (level == ECOWATT_LEVEL_WARNING) temperature = min (temperature, PilotwireConvertTargetTemperature (PILOTWIRE_TARGET_ECOWATT2));

  // if ecowatt level 3 (critical) is detected
  else if (level == ECOWATT_LEVEL_POWERCUT) temperature = min (temperature, PilotwireConvertTargetTemperature (PILOTWIRE_TARGET_ECOWATT3));

  return temperature;
}

// set different target temperatures
void PilotwireSetTargetTemperature (const uint8_t type, const float target, const bool to_save)
{
  bool is_valid = false;
  char str_target[8];
  char str_type[16];

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
}

// set heater target mode
void PilotwireSetMode (const uint8_t mode)
{
  char str_mode[16];

  // check parameter
  if (mode >= PILOTWIRE_MODE_MAX) return;

  // set target mode and actual mode if forced
  pilotwire_config.mode = mode;

  // reset night mode
  pilotwire_status.night_mode = false;

  // ask for JSON update
  pilotwire_status.json_update = true;

  // reset presence detection 
  PilotwirePresenceResetDetection ();

  // reset open window detection
  PilotwireWindowResetDetection ();

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

  // update target temperature
  pilotwire_status.temp_target = PilotwireGetCurrentTargetTemperature ();

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

// update relay state
void PilotwireUpdateRelay ()
{
  bool  current, target;
  float temperature;

  // get current relay state
  current = PilotwireGetDeviceState ();
  target  = current;

  // if current temperature not available, consider it as higher possible range (it enable forced mode in case no sensor is available)
  temperature = pilotwire_status.temp_current;
  if (isnan (temperature)) temperature = PILOTWIRE_TEMP_DEFAULT_HIGH;

  // if temperature is too low, set target relay state to ON
  if (temperature < (pilotwire_status.temp_target - PILOTWIRE_TEMP_THRESHOLD)) target = true;

  // else, if temperature is too high, set target relay state OFF
  else if (temperature > (pilotwire_status.temp_target + PILOTWIRE_TEMP_THRESHOLD)) target = false;

  // if relay state has changed, set it
  if (target != current) PilotwireSetDeviceState (target);
}

/***********************************************\
 *                  Ecowatt
\***********************************************/

// set ecowatt management option
void PilotwireEcowattEnable (const bool state)
{
  // set main flag
  pilotwire_config.ecowatt = state;
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
void PilotwireWindowEnable (const bool state)
{
  // set main flag
  pilotwire_config.window = state;

  // reset window detection algo
  PilotwireWindowResetDetection ();
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
      pilotwire_window.opened = true;
      pilotwire_window.low_temp = pilotwire_status.temp_current;
    }
  }

  // else, window detected as opened, try to detect closure
  else
  {
    // update lower temperature
    pilotwire_window.low_temp = min (pilotwire_window.low_temp, pilotwire_status.temp_current);

    // if current temperature has increased enought, window is closed
    if (pilotwire_status.temp_current - pilotwire_window.low_temp >= PILOTWIRE_WINDOW_CLOSE_INCREASE) PilotwireWindowResetDetection ();
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
void PilotwirePresenceEnable (const bool state)
{
  // set main flag
  pilotwire_config.presence = state;

  // reset presence detection algo
  PilotwirePresenceResetDetection ();
}

void PilotwirePresenceResetDetection ()
{
  pilotwire_presence.delay = 0;
}

void PilotwirePresenceUpdateDetection ()
{
  // if presence detection is active
  if (pilotwire_config.presence)
  {
    // if presence is ignored, check for timeout
    if ((pilotwire_presence.time_ignore != UINT32_MAX) && (pilotwire_presence.time_ignore < LocalTime ())) pilotwire_presence.time_ignore = UINT32_MAX;

    // if presence should not be ignored
    if (pilotwire_presence.time_ignore == UINT32_MAX)
    {
#ifdef USE_PRESENCE_FORECAST
      pilotwire_presence.delay = PresenceDelaySinceLastActivity ();
#else
      if (SensorPresenceDetected (PILOTWIRE_PRESENCE_TIMEOUT)) pilotwire_presence.delay = 0;
        else pilotwire_presence.delay++;
#endif
    }

    // else presence is ignored, increase last detection delay
    else pilotwire_presence.delay++;
  }

  // else presence is forced
  else pilotwire_presence.delay = 0;
}

uint8_t PilotwirePresenceGetStatus ()
{
  uint8_t status = PILOTWIRE_PRESENCE_DETECTED;

  // if no presence detected since a long time
  if (pilotwire_presence.delay > 60 * PILOTWIRE_PRESENCE_TIMEOUT_MISSING) status = PILOTWIRE_PRESENCE_MISSING;

  // else if no presence detected since a short time
  else if (pilotwire_presence.delay < 60 * PILOTWIRE_PRESENCE_TIMEOUT_SHORT) status = PILOTWIRE_PRESENCE_SHORT;

  return status;
}

/**************************************************\
 *                  Configuration
\**************************************************/

void PilotwireLoadConfig () 
{
  bool    ecowatt, window, presence;
  uint8_t device_mode;

#ifdef USE_UFILESYS

  // retrieve saved settings from flash filesystem
  pilotwire_config.mode = (uint8_t)UfsCfgLoadKeyInt (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_MODE, 0);
  pilotwire_config.type = (uint8_t)UfsCfgLoadKeyInt (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_TYPE, 0);

  pilotwire_config.arr_temp[PILOTWIRE_TARGET_LOW]      = UfsCfgLoadKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_LOW,      PILOTWIRE_TEMP_DEFAULT_LOW);
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_HIGH]     = UfsCfgLoadKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_HIGH,     PILOTWIRE_TEMP_DEFAULT_HIGH);
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_COMFORT]  = UfsCfgLoadKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_COMFORT,  PILOTWIRE_TEMP_DEFAULT_NORMAL);
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_NIGHT]    = UfsCfgLoadKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_NIGHT,    0);
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECO]      = UfsCfgLoadKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_ECO,      0);
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_NOFROST]  = UfsCfgLoadKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_NOFROST,  0);
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT2] = UfsCfgLoadKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_ECOWATT2, PILOTWIRE_TEMP_DEFAULT_LOW);
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT3] = UfsCfgLoadKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_ECOWATT3, PILOTWIRE_TEMP_DEFAULT_LOW);

  ecowatt  = (bool)UfsCfgLoadKeyInt (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_ECOWATT,  1);
  window   = (bool)UfsCfgLoadKeyInt (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_WINDOW,   0);
  presence = (bool)UfsCfgLoadKeyInt (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_PRESENCE, 0);

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("PIL: Config from LittleFS"));

#else       // No LittleFS

  // retrieve saved settings from flash memory
  pilotwire_config.mode = (uint8_t)Settings->knx_CB_addr[0];
  pilotwire_config.type = (uint8_t)Settings->knx_CB_addr[1];

  pilotwire_config.arr_temp[PILOTWIRE_TARGET_LOW]      = (float)Settings->knx_CB_addr[2] / 10;
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_HIGH]     = (float)Settings->knx_CB_addr[3] / 10;
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_COMFORT]  = (float)Settings->knx_CB_addr[4] / 10;
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_NIGHT]    = ((float)Settings->knx_CB_addr[5] - 1000) / 10;
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECO]      = ((float)Settings->knx_CB_addr[6] - 1000) / 10;
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_NOFROST]  = ((float)Settings->knx_CB_addr[7] - 1000) / 10;
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT2] = ((float)Settings->knx_CB_addr[8] - 1000) / 10;
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT3] = ((float)Settings->knx_CB_addr[9] - 1000) / 10;
  
  ecowatt  = (bool)Settings->knx_GA_addr[7];
  window   = (bool)Settings->knx_GA_addr[8];
  presence = (bool)Settings->knx_GA_addr[9];

   // log
  AddLog (LOG_LEVEL_INFO, PSTR ("PIL: Config from Settings"));

# endif     // USE_UFILESYS

  // avoid values out of range
  if (pilotwire_config.mode >= PILOTWIRE_MODE_MAX) pilotwire_config.mode = PILOTWIRE_MODE_OFF;
  if (pilotwire_config.type >= PILOTWIRE_DEVICE_MAX) pilotwire_config.type = PILOTWIRE_DEVICE_NORMAL;
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
  PilotwireEcowattEnable (ecowatt);
  PilotwireWindowEnable (window);
  PilotwirePresenceEnable (presence);
}

void PilotwireSaveConfig () 
{
#ifdef USE_UFILESYS

  // save settings into flash filesystem
  UfsCfgSaveKeyInt (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_MODE, (int)pilotwire_config.mode, true);
  UfsCfgSaveKeyInt (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_TYPE, (int)pilotwire_config.type, false);

  UfsCfgSaveKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_LOW, pilotwire_config.arr_temp[PILOTWIRE_TARGET_LOW], false);
  UfsCfgSaveKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_HIGH, pilotwire_config.arr_temp[PILOTWIRE_TARGET_HIGH], false);
  UfsCfgSaveKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_COMFORT, pilotwire_config.arr_temp[PILOTWIRE_TARGET_COMFORT], false);
  UfsCfgSaveKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_NIGHT, pilotwire_config.arr_temp[PILOTWIRE_TARGET_NIGHT], false);
  UfsCfgSaveKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_ECO, pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECO], false);
  UfsCfgSaveKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_NOFROST, pilotwire_config.arr_temp[PILOTWIRE_TARGET_NOFROST], false);
  UfsCfgSaveKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_ECOWATT2, pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT2], false);
  UfsCfgSaveKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_ECOWATT3, pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT3], false);

  UfsCfgSaveKeyInt (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_ECOWATT, (int)pilotwire_config.ecowatt, false);
  UfsCfgSaveKeyInt (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_WINDOW, (int)pilotwire_config.window, false);
  UfsCfgSaveKeyInt (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_PRESENCE, (int)pilotwire_config.presence, false);

# else       // No LittleFS

  // save settings into flash memory
  Settings->knx_CB_addr[0] = (uint16_t)pilotwire_config.mode;
  Settings->knx_CB_addr[1] = (uint16_t)pilotwire_config.type;

  Settings->knx_CB_addr[2] = (uint16_t)(10 * pilotwire_config.arr_temp[PILOTWIRE_TARGET_LOW]);
  Settings->knx_CB_addr[3] = (uint16_t)(10 * pilotwire_config.arr_temp[PILOTWIRE_TARGET_HIGH]);
  Settings->knx_CB_addr[4] = (uint16_t)(10 * pilotwire_config.arr_temp[PILOTWIRE_TARGET_COMFORT]);
  Settings->knx_CB_addr[5] = (uint16_t)(1000 + (10 * pilotwire_config.arr_temp[PILOTWIRE_TARGET_NIGHT]));
  Settings->knx_CB_addr[6] = (uint16_t)(1000 + (10 * pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECO]));
  Settings->knx_CB_addr[7] = (uint16_t)(1000 + (10 * pilotwire_config.arr_temp[PILOTWIRE_TARGET_NOFROST]));
  Settings->knx_CB_addr[8] = (uint16_t)(1000 + (10 * pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT2]));
  Settings->knx_CB_addr[9] = (uint16_t)(1000 + (10 * pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT3]));

  Settings->knx_GA_addr[7] = (uint16_t)pilotwire_config.ecowatt;
  Settings->knx_GA_addr[8] = (uint16_t)pilotwire_config.window;
  Settings->knx_GA_addr[9] = (uint16_t)pilotwire_config.presence;

# endif     // USE_UFILESYS
}

/******************************************************\
 *                  Historical files
\******************************************************/

#ifdef USE_UFILESYS

// clean and rotate histo files
void PilotwireHistoRotate ()
{
  TIME_T time_dst;

  // extract data from current time
  BreakTime (LocalTime (), time_dst);

  // if we are monday, rotate weekly files
  if (time_dst.day_of_week == 2) UfsFileRotate (D_PILOTWIRE_HISTO_FILE_WEEK, 0, PILOTWIRE_HISTO_WEEK_MAX);

  // clean old CSV files
  UfsCleanupFileSystem (PILOTWIRE_HISTO_MIN_FREE, "csv");
}

// save current data to histo file
void PilotwireHistoSaveData (uint32_t period)
{
  long   index;
  TIME_T now_dst;
  char   str_filename[UFS_FILENAME_SIZE];
  char   str_value[32];
  char   str_line[64];
  File   file;

  // validate period value
  if (period != PILOTWIRE_PERIOD_WEEK) return;

  // extract current time data
  BreakTime (LocalTime (), now_dst);

  // set target is weekly records
  sprintf_P (str_filename, D_PILOTWIRE_HISTO_FILE_WEEK, 0);

  // generate current line
  index = (((now_dst.day_of_week + 5) % 7) * 86400 + now_dst.hour * 3600 + now_dst.minute * 60) / ARR_PILOTWIRE_PERIOD_SAMPLE[period];
  sprintf_P (str_line, PSTR ("%d;%02u/%02u;%02u:%02u"), index, now_dst.day_of_month, now_dst.month, now_dst.hour, now_dst.minute);

  // append actual temperature
  ext_snprintf_P (str_value, sizeof (str_value), PSTR (";%1_f"), &pilotwire_status.temp_current);
  strlcat (str_line, str_value, sizeof (str_line));

  // append actual mode target temperature
  ext_snprintf_P (str_value, sizeof (str_value), PSTR (";%1_f"), &pilotwire_status.temp_target);
  strlcat (str_line, str_value, sizeof (str_line));

  // append heater status
  sprintf_P (str_value, PSTR (";%d"), PilotwireGetDeviceState ());
  strlcat (str_line, str_value, sizeof (str_line));

  // end of line
  strlcat (str_line, "\n", sizeof (str_line));

  // if file doesn't exist, add header
  if (!ffsp->exists (str_filename))
  {
    file = ffsp->open (str_filename, "w"); 
    file.print ("Idx;Date;Time;Temp;Target;Status\n");
    file.print (str_line);
    file.close ();
  }

  // else append to current file
  else
  {
    file = ffsp->open (str_filename, "a"); 
    file.print (str_line);
    file.close ();
  }
}

# endif     // USE_UFILESYS

// Rotate files at day change
void PilotwireRotateAtMidnight ()
{
#ifdef USE_UFILESYS
  // if filesystem is available, save data to history file
  if (ufs_type) PilotwireHistoRotate ();

#endif    // USE_UFILESYS
}

void PilotwireUpdateGraph (uint32_t period)
{
  uint32_t index;
  float    value;

  // check period validity
  if (period >= PILOTWIRE_PERIOD_MAX) return;

  // get graph index
  index = pilotwire_graph.index[period];

  // if live update
  if (period == PILOTWIRE_PERIOD_LIVE)
  {
    // if temperature has been mesured, update current graph index
    if (!isnan (pilotwire_status.temp_current) && (index < PILOTWIRE_GRAPH_SAMPLE))
    {
      // set current temperature
      value = 10 * pilotwire_status.temp_current;
      pilotwire_graph.arr_temp[period][index] = (short)value;

      // set target temperature
      value = 10 * pilotwire_status.temp_target;
      pilotwire_graph.arr_target[period][index] = (short)value;

      // set pilotwire state
      pilotwire_graph.arr_state[period][index] = pilotwire_graph.device_state[period];
    }
  }

#ifdef USE_UFILESYS
  // else save data to history file
  else if (period == PILOTWIRE_PERIOD_WEEK)
  {
    // if filesystem is available, save data
    if (ufs_type) PilotwireHistoSaveData (period);
  }
#endif    // USE_UFILESYS

  // increase temperature data index and reset if max reached
  index++;
  pilotwire_graph.index[period] = index % PILOTWIRE_GRAPH_SAMPLE;

  // set graph updated flag and init graph values
  pilotwire_graph.device_state[period] = UINT8_MAX;
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
void PilotwireShowJSON (bool is_autonomous)
{
  float temperature;
  char  str_value[16];

  // add , in append mode or { in direct publish mode
  if (is_autonomous) Response_P (PSTR ("{"));
    else ResponseAppend_P (PSTR (","));

  // add pilotwire data
  //   "PilotWire":{"Mode":2,"Heating":1,"Temperature":18.6,"Target":21.0,"Window":0,"Presence":0,"Delay":262}
  ResponseAppend_P (PSTR ("\"%s\":{"), D_JSON_PILOTWIRE);

  // configuration mode
  ResponseAppend_P (PSTR ("\"%s\":%d"), D_JSON_PILOTWIRE_MODE, pilotwire_config.mode);

  // heating state
  ResponseAppend_P (PSTR (",\"%s\":%u"), D_JSON_PILOTWIRE_HEATING, PilotwireGetDeviceState ());

  // add current temperature
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &pilotwire_status.temp_current);
  ResponseAppend_P (PSTR (",\"%s\":%s"), D_JSON_PILOTWIRE_TEMPERATURE, str_value);

  // target temperature
  temperature = PilotwireGetModeTargetTemperature (pilotwire_config.mode);
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &temperature);
  ResponseAppend_P (PSTR (",\"%s\":%s"), D_JSON_PILOTWIRE_TARGET, str_value);

  // window open detection status
  ResponseAppend_P (PSTR (",\"%s\":%u"), D_JSON_PILOTWIRE_WINDOW, PilotwireWindowGetStatus ());

  // presence detection status
  ResponseAppend_P (PSTR (",\"%s\":%u"), D_JSON_PILOTWIRE_PRESENCE, PilotwirePresenceGetStatus ());

  // last detection (in minutes)
  ResponseAppend_P (PSTR (",\"%s\":%u"), D_JSON_PILOTWIRE_DELAY, pilotwire_presence.delay / 60);

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

// called every second, to update
//  - temperature
//  - presence detection
//  - graph data
void PilotwireEverySecond ()
{
  uint32_t counter;

  // update window open detection
  PilotwireWindowUpdateDetection ();

  // update presence detection
  PilotwirePresenceUpdateDetection ();

  // update temperature and target
  PilotwireUpdateTemperature ();

  // update relay state
  PilotwireUpdateRelay ();

  // loop thru the periods, to update graph data to the max on the period
  for (counter = 0; counter < PILOTWIRE_PERIOD_MAX; counter++)
  {
    // update graph temperature for current period (keep minimal value)
    if (isnan (pilotwire_graph.temperature[counter])) pilotwire_graph.temperature[counter] = pilotwire_status.temp_current;
    else if (!isnan (pilotwire_status.temp_current)) pilotwire_graph.temperature[counter] = min (pilotwire_graph.temperature[counter], pilotwire_status.temp_current);

    // update device state
    if (pilotwire_graph.device_state[counter] != true) pilotwire_graph.device_state[counter] = PilotwireGetDeviceState ();

    // increment delay counter and if delay reached, update history data
    pilotwire_graph.counter[counter] = pilotwire_graph.counter[counter] % ARR_PILOTWIRE_PERIOD_SAMPLE[counter];
    if (pilotwire_graph.counter[counter] == 0) PilotwireUpdateGraph (counter);
    pilotwire_graph.counter[counter]++;
  }
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
    pilotwire_graph.temperature[index] = NAN;
    pilotwire_graph.device_state[index] = UINT8_MAX;

    // initialise temperature graph
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
  if (include_heating && PilotwireGetDeviceState ()) str_result += " 🔥";

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
  char  str_icon[8];
  char  str_status[16];
  char  str_label[16];

  // display current mode
  GetTextIndexed (str_icon, sizeof (str_icon), pilotwire_config.mode, kPilotwireModeIcon);
  GetTextIndexed (str_label, sizeof (str_label), pilotwire_config.mode, kPilotwireModeLabel);
  PilotwireWebIconStatus (str_status, sizeof (str_status), false);
  WSContentSend_PD (PSTR ("{s}%s %s %s{m}%s %s{e}\n"), D_PILOTWIRE, D_PILOTWIRE_MODE, str_icon, str_label, str_status);

  // get current and target temperature

  // device heating
  if (PilotwireGetDeviceState ()) strcpy (str_icon, "🔥"); else strcpy (str_icon, "");
  ext_snprintf_P (str_label, sizeof (str_label), PSTR ("%01_f"), &pilotwire_status.temp_target);
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
  char     str_temp[8];
  char     str_label[16];

  // control button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_PAGE_PILOTWIRE_CONTROL, D_PILOTWIRE_CONTROL);

  // graph button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_PAGE_PILOTWIRE_GRAPH, D_PILOTWIRE_GRAPH);

  // status mode options
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'>\n"), D_PAGE_PILOTWIRE_STATUS);
  WSContentSend_P (PSTR ("<select style='width:100%%;text-align:center;padding:8px;border-radius:0.3rem;' name='%s' onchange='this.form.submit();'>\n"), D_CMND_PILOTWIRE_MODE, pilotwire_config.mode);
  WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>\n"), UINT8_MAX, "", PSTR ("-- Select mode --"));
  for (index = 0; index < PILOTWIRE_MODE_MAX; index ++)
  {
    // get display
    GetTextIndexed (str_icon, sizeof (str_icon), index, kPilotwireModeIcon);
    GetTextIndexed (str_label, sizeof (str_label), index, kPilotwireModeLabel);

    // get temperature
    temperature = PilotwireGetModeTargetTemperature (index);
    ext_snprintf_P (str_temp, sizeof (str_temp), PSTR ("(%01_f)"), &temperature);

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

    // enable ecowatt detection according to 'ecowatt' parameter
    WebGetArg (D_CMND_PILOTWIRE_ECOWATT, str_argument, sizeof (str_argument));
    state = (strcmp (str_argument, "on") == 0);
    PilotwireEcowattEnable (state);

    // enable open window detection according to 'window' parameter
    WebGetArg (D_CMND_PILOTWIRE_WINDOW, str_argument, sizeof (str_argument));
    state = (strcmp (str_argument, "on") == 0);
    PilotwireWindowEnable (state);

    // enable presence detection according to 'pres' parameter
    WebGetArg (D_CMND_PILOTWIRE_PRESENCE, str_argument, sizeof (str_argument));
    state = (strcmp (str_argument, "on") == 0);
    PilotwirePresenceEnable (state);

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
    WebGetArg (D_CMND_PILOTWIRE_ECOWATT2, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) PilotwireSetTargetTemperature (PILOTWIRE_TARGET_ECOWATT2, atof (str_argument), false);

    // set ecowatt level 3 temperature according to 'ecowatt3' parameter
    WebGetArg (D_CMND_PILOTWIRE_ECOWATT3, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) PilotwireSetTargetTemperature (PILOTWIRE_TARGET_ECOWATT3, atof (str_argument), false);

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

  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_PAGE_PILOTWIRE_CONFIG);

  //    Device type 
  // -----------------

  WSContentSend_P (D_CONF_FIELDSET_START, "🔗", D_PILOTWIRE_CONNEXION);

  // command type selection
  if (pilotwire_config.type == PILOTWIRE_DEVICE_NORMAL) strcpy_P (str_argument, PSTR (D_PILOTWIRE_CHECKED)); else strcpy (str_argument, "");
  WSContentSend_P (PSTR ("<input type='radio' id='normal' name='%s' value=%d %s>%s<br>\n"), D_CMND_PILOTWIRE_TYPE, PILOTWIRE_DEVICE_NORMAL, str_argument, D_PILOTWIRE);
  if (pilotwire_config.type == PILOTWIRE_DEVICE_DIRECT) strcpy_P (str_argument, PSTR (D_PILOTWIRE_CHECKED)); else strcpy (str_argument, "");
  WSContentSend_P (PSTR ("<input type='radio' id='direct' name='%s' value=%d %s>%s<br>\n"), D_CMND_PILOTWIRE_TYPE, PILOTWIRE_DEVICE_DIRECT, str_argument, D_PILOTWIRE_DIRECT);

  WSContentSend_P (D_CONF_FIELDSET_STOP);

  //   Options
  //    - Ecowatt management
  //    - Window opened detection
  //    - Presence detection
  // -----------------------------

  WSContentSend_P (D_CONF_FIELDSET_START, "♻️", D_PILOTWIRE_OPTION);

  // ecowatt
  if (pilotwire_config.ecowatt) strcpy (str_value, D_PILOTWIRE_CHECKED); else strcpy (str_value, "");
  WSContentSend_P (PSTR ("<p><input type='checkbox' id='%s' name='%s' %s>\n"), D_CMND_PILOTWIRE_ECOWATT, D_CMND_PILOTWIRE_ECOWATT, str_value);
  WSContentSend_P (PSTR ("<label for='%s'>%s %s</label></p>\n"), D_CMND_PILOTWIRE_ECOWATT, D_PILOTWIRE_SIGNAL, D_PILOTWIRE_ECOWATT);

  // window opened
  if (pilotwire_config.window) strcpy (str_value, D_PILOTWIRE_CHECKED); else strcpy (str_value, "");
  WSContentSend_P (PSTR ("<p><input type='checkbox' id='%s' name='%s' %s>\n"), D_CMND_PILOTWIRE_WINDOW, D_CMND_PILOTWIRE_WINDOW, str_value);
  WSContentSend_P (PSTR ("<label for='%s'>%s %s</label></p>\n"), D_CMND_PILOTWIRE_WINDOW, D_PILOTWIRE_WINDOW, D_PILOTWIRE_DETECTION);

  // presence
  if (pilotwire_config.presence) strcpy (str_value, D_PILOTWIRE_CHECKED); else strcpy (str_value, "");
  WSContentSend_P (PSTR ("<p><input type='checkbox' id='%s' name='%s' %s>\n"), D_CMND_PILOTWIRE_PRESENCE, D_CMND_PILOTWIRE_PRESENCE, str_value);
  WSContentSend_P (PSTR ("<label for='%s'>%s %s</label></p>\n"), D_CMND_PILOTWIRE_PRESENCE, D_PILOTWIRE_PRESENCE, D_PILOTWIRE_DETECTION);

  WSContentSend_P (D_CONF_FIELDSET_STOP);

  //   Temperature : Range
  // -----------------------

  WSContentSend_P (D_CONF_FIELDSET_START, "🌡️", PSTR ("Range (°C)"));

  // lowest temperature
  GetTextIndexed (str_icon, sizeof (str_icon), PILOTWIRE_TARGET_LOW, kPilotwireTargetIcon);
  GetTextIndexed (str_argument, sizeof (str_argument), PILOTWIRE_TARGET_LOW, kPilotwireTargetLabel);
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &pilotwire_config.arr_temp[PILOTWIRE_TARGET_LOW]);
  WSContentSend_P (D_CONF_FIELD_FULL, str_icon, str_argument, D_CMND_PILOTWIRE_LOW, D_CMND_PILOTWIRE_LOW, PILOTWIRE_TEMP_LIMIT_MIN, PILOTWIRE_TEMP_LIMIT_MAX, str_step, str_value);

  // highest temperature
  GetTextIndexed (str_icon, sizeof (str_icon), PILOTWIRE_TARGET_HIGH, kPilotwireTargetIcon);
  GetTextIndexed (str_argument, sizeof (str_argument), PILOTWIRE_TARGET_HIGH, kPilotwireTargetLabel);
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &pilotwire_config.arr_temp[PILOTWIRE_TARGET_HIGH]);
  WSContentSend_P (D_CONF_FIELD_FULL, str_icon, str_argument, D_CMND_PILOTWIRE_HIGH, D_CMND_PILOTWIRE_HIGH, PILOTWIRE_TEMP_LIMIT_MIN, PILOTWIRE_TEMP_LIMIT_MAX, str_step, str_value);

  WSContentSend_P (D_CONF_FIELDSET_STOP);

  //   Temperature : Mode
  // -----------------------

  WSContentSend_P (D_CONF_FIELDSET_START, "🌡️", PSTR ("Mode (°C)"));

  // comfort temperature
  GetTextIndexed (str_icon, sizeof (str_icon), PILOTWIRE_TARGET_COMFORT, kPilotwireTargetIcon);
  GetTextIndexed (str_argument, sizeof (str_argument), PILOTWIRE_TARGET_COMFORT, kPilotwireTargetLabel);
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &pilotwire_config.arr_temp[PILOTWIRE_TARGET_COMFORT]);
  WSContentSend_P (D_CONF_FIELD_FULL, str_icon, str_argument, D_CMND_PILOTWIRE_COMFORT, D_CMND_PILOTWIRE_COMFORT, PILOTWIRE_TEMP_LIMIT_MIN, PILOTWIRE_TEMP_LIMIT_MAX, str_step, str_value);

  // eco temperature
  GetTextIndexed (str_icon, sizeof (str_icon), PILOTWIRE_TARGET_ECO, kPilotwireTargetIcon);
  GetTextIndexed (str_argument, sizeof (str_argument), PILOTWIRE_TARGET_ECO, kPilotwireTargetLabel);
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECO]);
  WSContentSend_P (D_CONF_FIELD_FULL, str_icon, str_argument, D_CMND_PILOTWIRE_ECO, D_CMND_PILOTWIRE_ECO, - PILOTWIRE_TEMP_LIMIT_MAX, PILOTWIRE_TEMP_LIMIT_MAX, str_step, str_value);

  // no frost temperature
  GetTextIndexed (str_icon, sizeof (str_icon), PILOTWIRE_TARGET_NOFROST, kPilotwireTargetIcon);
  GetTextIndexed (str_argument, sizeof (str_argument), PILOTWIRE_TARGET_NOFROST, kPilotwireTargetLabel);
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &pilotwire_config.arr_temp[PILOTWIRE_TARGET_NOFROST]);
  WSContentSend_P (D_CONF_FIELD_FULL, str_icon, str_argument, D_CMND_PILOTWIRE_NOFROST, D_CMND_PILOTWIRE_NOFROST, - PILOTWIRE_TEMP_LIMIT_MAX, PILOTWIRE_TEMP_LIMIT_MAX, str_step, str_value);

  WSContentSend_P (D_CONF_FIELDSET_STOP);

  //   Temperature : Specific
  // -----------------------

  WSContentSend_P (D_CONF_FIELDSET_START, "🌡️", PSTR ("Specific (°C)"));

  // night mode temperature
  GetTextIndexed (str_icon, sizeof (str_icon), PILOTWIRE_TARGET_NIGHT, kPilotwireTargetIcon);
  GetTextIndexed (str_argument, sizeof (str_argument), PILOTWIRE_TARGET_NIGHT, kPilotwireTargetLabel);
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &pilotwire_config.arr_temp[PILOTWIRE_TARGET_NIGHT]);
  WSContentSend_P (D_CONF_FIELD_FULL, str_icon, str_argument, D_CMND_PILOTWIRE_NIGHT, D_CMND_PILOTWIRE_NIGHT, - PILOTWIRE_TEMP_LIMIT_MAX, PILOTWIRE_TEMP_LIMIT_MAX, str_step, str_value);

  // ecowatt level 2 temperature
  GetTextIndexed (str_icon, sizeof (str_icon), PILOTWIRE_TARGET_ECOWATT2, kPilotwireTargetIcon);
  GetTextIndexed (str_argument, sizeof (str_argument), PILOTWIRE_TARGET_ECOWATT2, kPilotwireTargetLabel);
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT2]);
  WSContentSend_P (D_CONF_FIELD_FULL, str_icon, str_argument, D_CMND_PILOTWIRE_ECOWATT2, D_CMND_PILOTWIRE_ECOWATT2, - PILOTWIRE_TEMP_LIMIT_MAX, PILOTWIRE_TEMP_LIMIT_MAX, str_step, str_value);

  // ecowatt level 3 temperature
  GetTextIndexed (str_icon, sizeof (str_icon), PILOTWIRE_TARGET_ECOWATT3, kPilotwireTargetIcon);
  GetTextIndexed (str_argument, sizeof (str_argument), PILOTWIRE_TARGET_ECOWATT3, kPilotwireTargetLabel);
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT3]);
  WSContentSend_P (D_CONF_FIELD_FULL, str_icon, str_argument, D_CMND_PILOTWIRE_ECOWATT3, D_CMND_PILOTWIRE_ECOWATT3, - PILOTWIRE_TEMP_LIMIT_MAX, PILOTWIRE_TEMP_LIMIT_MAX, str_step, str_value);

  WSContentSend_P (D_CONF_FIELDSET_STOP);

  WSContentSend_P (PSTR ("<br>\n"));

  // save button
  WSContentSend_P (PSTR ("<p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), D_SAVE);
  WSContentSend_P (PSTR ("</form>\n"));

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

// Temperature graph frame
void PilotwireWebGraphFrame ()
{
  float temperature;
  char  str_value[8];

  // start of SVG graph
  WSContentBegin (200, CT_HTML);
  WSContentSend_P (PSTR ("<svg viewBox='0 0 %d %d' preserveAspectRatio='xMinYMinmeet'>\n"), PILOTWIRE_GRAPH_WIDTH, PILOTWIRE_GRAPH_HEIGHT);

  // SVG style 
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));
  WSContentSend_P (PSTR ("rect {stroke:grey;fill:none;}\n"));
  WSContentSend_P (PSTR ("line.dash {stroke:grey;stroke-dasharray:8;}\n"));
  WSContentSend_P (PSTR ("text.temperature {font-size:18px;fill:yellow;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // graph curve zone
  WSContentSend_P (PSTR ("<rect x='%d%%' y='0%%' width='%d%%' height='100%%' rx='10' />\n"), PILOTWIRE_GRAPH_PERCENT_START, PILOTWIRE_GRAPH_PERCENT_STOP - PILOTWIRE_GRAPH_PERCENT_START);

  // graph temperature units
  temperature = pilotwire_graph.temp_max;
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &temperature);
  WSContentSend_P (D_GRAPH_TEMPERATURE, 1, 4, str_value);
  temperature = pilotwire_graph.temp_min + (pilotwire_graph.temp_max - pilotwire_graph.temp_min) * 0.75;
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &temperature);
  WSContentSend_P (D_GRAPH_TEMPERATURE, 1, 27, str_value);
  temperature = pilotwire_graph.temp_min + (pilotwire_graph.temp_max - pilotwire_graph.temp_min) * 0.50;
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &temperature);
  WSContentSend_P (D_GRAPH_TEMPERATURE, 1, 52, str_value);
  temperature = pilotwire_graph.temp_min + (pilotwire_graph.temp_max - pilotwire_graph.temp_min) * 0.25;
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &temperature);
  WSContentSend_P (D_GRAPH_TEMPERATURE, 1, 77, str_value);
  temperature = pilotwire_graph.temp_min;
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &temperature);
  WSContentSend_P (D_GRAPH_TEMPERATURE, 1, 99, str_value);

  // graph separation lines
  WSContentSend_P (D_GRAPH_SEPARATION, PILOTWIRE_GRAPH_PERCENT_START, 25, PILOTWIRE_GRAPH_PERCENT_STOP, 25);
  WSContentSend_P (D_GRAPH_SEPARATION, PILOTWIRE_GRAPH_PERCENT_START, 50, PILOTWIRE_GRAPH_PERCENT_STOP, 50);
  WSContentSend_P (D_GRAPH_SEPARATION, PILOTWIRE_GRAPH_PERCENT_START, 75, PILOTWIRE_GRAPH_PERCENT_STOP, 75);

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentEnd ();
}

// Temperature graph data
void PilotwireWebGraphData ()
{
  int      unit_width, shift_unit, shift_width;
  int      graph_x, graph_left, graph_right, graph_width, graph_low, graph_high;
  float    graph_y, value, temp_scope;
  TIME_T   current_dst;
  uint32_t current_time;
  uint32_t index, array_index;

  // validature min and max temperature difference
  temp_scope = pilotwire_graph.temp_max - pilotwire_graph.temp_min;
  if (temp_scope == 0) return;

  // start of SVG graph
  WSContentBegin(200, CT_HTML);
  WSContentSend_P (PSTR ("<svg viewBox='0 0 %d %d' preserveAspectRatio='xMinYMinmeet'>\n"), PILOTWIRE_GRAPH_WIDTH, PILOTWIRE_GRAPH_HEIGHT);

  // SVG style 
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));
  WSContentSend_P (PSTR ("polyline.temperature {fill:none;stroke:yellow;stroke-width:2;}\n"));
  WSContentSend_P (PSTR ("polyline.target {fill:none;stroke:orange;stroke-dasharray:4;}\n"));
  WSContentSend_P (PSTR ("polyline.state {fill:none;stroke:white;stroke-width:2;}\n"));
  WSContentSend_P (PSTR ("line.time {stroke:white;}\n"));
  WSContentSend_P (PSTR ("text.time {font-size:16px;fill:white;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // boundaries of SVG graph
  graph_left  = PILOTWIRE_GRAPH_PERCENT_START * PILOTWIRE_GRAPH_WIDTH / 100;
  graph_right = PILOTWIRE_GRAPH_PERCENT_STOP * PILOTWIRE_GRAPH_WIDTH / 100;
  graph_width = graph_right - graph_left;

  // --------------------------
  //    Pilotwire state curve
  // --------------------------

  // loop for the pilotwire state (white curve)
  graph_low  = 100 * PILOTWIRE_GRAPH_HEIGHT / 100;
  graph_high = 95 * PILOTWIRE_GRAPH_HEIGHT / 100;
  WSContentSend_P (PSTR ("<polyline class='state' points='"));
  for (index = 0; index < PILOTWIRE_GRAPH_SAMPLE; index++)
  {
    // if state has been defined
    array_index = (index + pilotwire_graph.index[0]) % PILOTWIRE_GRAPH_SAMPLE;
    if (pilotwire_graph.arr_state[0][array_index] != UINT8_MAX)
    {
      // calculate end point x and y
      graph_x = graph_left + (graph_width * index / PILOTWIRE_GRAPH_SAMPLE);

      // add the point to the line
      if (pilotwire_graph.arr_state[0][array_index] == true) WSContentSend_P (PSTR ("%d,%d "), graph_x, graph_high);
        else WSContentSend_P (PSTR ("%d,%d "), graph_x, graph_low);
    }
  }
  WSContentSend_P (PSTR ("'/>\n"));

  // ----------------------------
  //   Target Temperature curve
  // ----------------------------

  // loop for the target temperature graph
  WSContentSend_P (PSTR ("<polyline class='target' points='"));
  for (index = 0; index < PILOTWIRE_GRAPH_SAMPLE; index++)
  {
    // get target temperature value and set to minimum if not defined
    array_index = (index + pilotwire_graph.index[0]) % PILOTWIRE_GRAPH_SAMPLE;
    if (pilotwire_graph.arr_target[0][array_index] != SHRT_MAX)
    {
      // read indexed temperature
      value = (float)pilotwire_graph.arr_target[0][array_index];
      value = value / 10;

      // calculate end point x and y
      graph_x = graph_left + (graph_width * index / PILOTWIRE_GRAPH_SAMPLE);
      graph_y = (1 - ((value - pilotwire_graph.temp_min) / temp_scope)) * PILOTWIRE_GRAPH_HEIGHT;

      // add the point to the line
      WSContentSend_P (PSTR ("%d,%d "), graph_x, int (graph_y));
    }
  }
  WSContentSend_P (PSTR ("'/>\n"));

  // ---------------------
  //   Temperature curve
  // ---------------------

  // loop for the temperature graph
  WSContentSend_P (PSTR ("<polyline class='temperature' points='"));
  for (index = 0; index < PILOTWIRE_GRAPH_SAMPLE; index++)
  {
    // if temperature value is defined
    array_index = (index + pilotwire_graph.index[0]) % PILOTWIRE_GRAPH_SAMPLE;
    if (pilotwire_graph.arr_temp[0][array_index] != SHRT_MAX)
    {
      // read indexed temperature
      value = (float)pilotwire_graph.arr_temp[0][array_index];
      value = value / 10;

      // calculate end point x and y
      graph_x = graph_left + (graph_width * index / PILOTWIRE_GRAPH_SAMPLE);
      graph_y  = (1 - ((value - pilotwire_graph.temp_min) / temp_scope)) * PILOTWIRE_GRAPH_HEIGHT;

      // add the point to the line
      WSContentSend_P (PSTR ("%d,%d "), graph_x, int (graph_y));
    }
  }
  WSContentSend_P (PSTR ("'/>\n"));

  // ---------------
  //   Time line
  // ---------------

  // get current time
  current_time = LocalTime ();
  BreakTime (current_time, current_dst);

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
    WSContentSend_P (PSTR ("<line class='time' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n"), graph_x, 49, graph_x, 51);
    WSContentSend_P (PSTR ("<text class='time' x='%d' y='%d%%'>%02dh</text>\n"), graph_x - 15, 55, current_dst.hour);
  }

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentEnd();
}

// Get temperature attributes according to target temperature
void PilotwireWebGetTemperatureClass (float temperature, char* pstr_class, size_t size_class)
{
  // check prameters
  if ((pstr_class == nullptr) || isnan (temperature)) return;

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
  strlcat (str_update, ";/", sizeof (str_update));
  temperature = PilotwireGetModeTargetTemperature (pilotwire_config.mode);
  PilotwireWebGetTemperatureClass (temperature, str_text, sizeof (str_text));
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
  char  str_value[64];

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
  temperature = PilotwireGetModeTargetTemperature (pilotwire_config.mode);

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
  WSContentSend_P (PSTR ("httpUpd.open('GET','%s',true);\n"), D_PAGE_PILOTWIRE_UPDATE);
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
  WSContentSend_P (PSTR ("div.mode div.item {width:80px;border-radius:6px;}\n"));

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
  if (pilotwire_config.mode == PILOTWIRE_MODE_COMFORT)
  {
    WSContentSend_P (PSTR ("<div class='inline adjust'>\n"));

    // button -0.5
    value = temperature - 0.5;
    ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &value);
    WSContentSend_P (PSTR ("<a href='/%s?%s=%s'><div class='inline item'>%s</div></a>\n"), D_PAGE_PILOTWIRE_CONTROL, D_CMND_PILOTWIRE_TARGET, str_value, "-");

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
    WSContentSend_P (PSTR ("<a href='/%s?%s=%s'><div class='inline item'>%s</div></a>\n"), D_PAGE_PILOTWIRE_CONTROL, D_CMND_PILOTWIRE_TARGET, str_value, "+");

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

  if (pilotwire_config.mode == PILOTWIRE_MODE_OFF) WSContentSend_P (PSTR ("<div class='inline item' title='%s'>❄</div>\n"), D_PILOTWIRE_OFF);
    else WSContentSend_P (PSTR ("<a href='/%s?%s=%u'><div class='inline item' title='%s'>❄</div></a>\n"), D_PAGE_PILOTWIRE_CONTROL, D_CMND_PILOTWIRE_MODE, PILOTWIRE_MODE_OFF, D_PILOTWIRE_OFF);
  if (pilotwire_config.mode == PILOTWIRE_MODE_ECO) WSContentSend_P (PSTR ("<div class='inline item' title='%s'>🌙</div>\n"), D_PILOTWIRE_ECO);
    else WSContentSend_P (PSTR ("<a href='/%s?%s=%u'><div class='inline item' title='%s'>🌙</div></a>\n"), D_PAGE_PILOTWIRE_CONTROL, D_CMND_PILOTWIRE_MODE, PILOTWIRE_MODE_ECO, D_PILOTWIRE_ECO);
  if (pilotwire_config.mode == PILOTWIRE_MODE_COMFORT) WSContentSend_P (PSTR ("<div class='inline item' title='%s'>🔆</div>\n"), D_PILOTWIRE_COMFORT);
    else WSContentSend_P (PSTR ("<a href='/%s?%s=%u'><div class='inline item' title='%s'>🔆</div></a>\n"), D_PAGE_PILOTWIRE_CONTROL, D_CMND_PILOTWIRE_MODE, PILOTWIRE_MODE_COMFORT, D_PILOTWIRE_COMFORT);

  WSContentSend_P (PSTR ("</div>\n"));    // inline mode

  // end of form
  WSContentSend_P (PSTR ("</form>\n"));

  // end of page
  WSContentStop ();
}

// Pilot Wire heater public configuration web page
void PilotwireWebPageGraph ()
{
  bool     updated = false;
  uint32_t index;
  float    value, target;
  char     str_value[8];

  // beginning of page
  WSContentStart_P (D_PAGE_PILOTWIRE_GRAPH, false);
  WSContentSend_P (PSTR ("</script>\n"));

  // set default min and max graph temperature
  pilotwire_graph.temp_min = pilotwire_config.arr_temp[PILOTWIRE_TARGET_LOW];
  pilotwire_graph.temp_max = pilotwire_config.arr_temp[PILOTWIRE_TARGET_HIGH];

  // adjust to current temperature
  if (pilotwire_status.temp_current < pilotwire_config.arr_temp[PILOTWIRE_TARGET_LOW])  pilotwire_graph.temp_min = floor (pilotwire_status.temp_current);
  if (pilotwire_status.temp_current > pilotwire_config.arr_temp[PILOTWIRE_TARGET_HIGH]) pilotwire_graph.temp_max = ceil (pilotwire_status.temp_current);

  // loop to adjust graph min and max temperature
  for (index = 0; index < PILOTWIRE_GRAPH_SAMPLE; index++)
    if (pilotwire_graph.arr_temp[0][index] != SHRT_MAX)
    {
      // read indexed temperature
      value = (float)pilotwire_graph.arr_temp[0][index];
      value = value / 10; 

      // adjust minimum and maximum temperature
      if (pilotwire_graph.temp_min > value) pilotwire_graph.temp_min = floor (value);
      if (pilotwire_graph.temp_max < value) pilotwire_graph.temp_max = ceil  (value);
    }

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {width:100%%;max-width:800px;margin:0.2rem auto;padding:0;text-align:center;vertical-align:middle;}\n"));

  WSContentSend_P (PSTR ("div.title {font-size:1.5rem;font-weight:bold;margin:10px auto;}\n"));

  // graph
  WSContentSend_P (PSTR (".svg-container {position:relative;vertical-align:middle;overflow:hidden;width:100%%;max-width:%dpx;padding-bottom:65%%;}\n"), PILOTWIRE_GRAPH_WIDTH);
  WSContentSend_P (PSTR (".svg-content {display:inline-block;position:absolute;top:0;left:0;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // page body
  WSContentSend_P (PSTR ("</head>\n"));
  WSContentSend_P (PSTR ("<body>\n"));

  // room name
  WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText(SET_DEVICENAME));

  // graph section
  WSContentSend_P (PSTR ("<div class='svg-container'>\n"));
  WSContentSend_P (PSTR ("<object class='svg-content' id='base' type='image/svg+xml' width='%d%%' height='%d%%' data='%s'></object>\n"), 100, 100, D_PAGE_PILOTWIRE_BASE_SVG);
  WSContentSend_P (PSTR ("<object class='svg-content' id='data' type='image/svg+xml' width='%d%%' height='%d%%' data='%s'></object>\n"), 100, 100, D_PAGE_PILOTWIRE_DATA_SVG);
  WSContentSend_P (PSTR ("</div>\n"));      // svg-container

  // end of page
  WSContentSend_P (PSTR ("</form>\n"));
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
    case FUNC_COMMAND:
      result = DecodeCommand (kPilotwireCommand, PilotwireCommand);
      break;
    case FUNC_EVERY_SECOND:
      if (TasmotaGlobal.uptime > 5) PilotwireEverySecond ();
      break;
    case FUNC_SAVE_AT_MIDNIGHT:
      PilotwireRotateAtMidnight ();
      break;
    case FUNC_JSON_APPEND:
      PilotwireShowJSON (false);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:

#ifdef USE_UFILESYS
      // icons : logo
      Webserver->on ("/" PILOTWIRE_ICON_LOGO, PilotwireWebIconLogo);
#endif    // USE_UFILESYS

      // pages
      Webserver->on ("/" D_PAGE_PILOTWIRE_CONFIG,   PilotwireWebPageConfigure);
      Webserver->on ("/" D_PAGE_PILOTWIRE_STATUS,   PilotwireWebPageSwitchStatus);
      Webserver->on ("/" D_PAGE_PILOTWIRE_CONTROL,  PilotwireWebPageControl);
      Webserver->on ("/" D_PAGE_PILOTWIRE_GRAPH,    PilotwireWebPageGraph);
      Webserver->on ("/" D_PAGE_PILOTWIRE_BASE_SVG, PilotwireWebGraphFrame);
      Webserver->on ("/" D_PAGE_PILOTWIRE_DATA_SVG, PilotwireWebGraphData);
      Webserver->on ("/" D_PAGE_PILOTWIRE_UPDATE,   PilotwireWebUpdate);

      // icons
      Webserver->on (PILOTWIRE_ICON_DEFAULT, PilotwireWebIconDefault);
      break;
    case FUNC_WEB_SENSOR:
      PilotwireWebSensor ();
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      PilotwireWebMainButton ();
      break;
    case FUNC_WEB_ADD_BUTTON:
      WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>Configure %s</button></form></p>\n"), D_PAGE_PILOTWIRE_CONFIG, D_PILOTWIRE);
      break;
#endif  // USE_Webserver

  }
  return result;
}

#endif // USE_PILOTWIRE
