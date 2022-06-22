/*
  xsns_97_pilotwire.ino - French Pilot Wire (Fil Pilote) support

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
  
  If LittleFS partition is available :
    - settings are stored in /pilotwire.cfg and /offload.cfg
    - room picture can be stored as /logo.png

  If no LittleFS partition is available, settings are stored using knx_CB_addr parameters :
    - Settings.knx_CB_addr[0]   = Device type (pilotewire or direct)
    - Settings.knx_CB_addr[1]   = Pilotwire mode
    - Settings.knx_CB_addr[2]   = Minimum temperature (x10 -> 125 = 12.5°C)
    - Settings.knx_CB_addr[3]   = Maximum temperature (x10 -> 240 = 24.0°C)
    - Settings.knx_CB_addr[4]   = Thermostat target temperature (x10 -> 185 = 18.5°C)

    - Settings.knx_CB_addr[5]   = Thermostat Night mode temperature
    - Settings.knx_CB_addr[6]   = Thermostat Nobody moving temperature
    - Settings.knx_CB_addr[7]   = Thermostat Vacancy temperature
        < 1000 = negative correction  : 900 = -10°C, 950 = -5°C
        > 1000 = absolute temperature : 1100 = 10°C, 1200 = 20°C

  How to enable Movement Detection :
    - connect a sensor like RCWM-0516
    - declare its GPIO as Switch 0

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

#define XSNS_97                         97

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

#define D_CMND_PILOTWIRE_PREFIX       "pw_"
#define D_CMND_PILOTWIRE_HELP         "help"
#define D_CMND_PILOTWIRE_TYPE         "type"
#define D_CMND_PILOTWIRE_MODE         "mode"
#define D_CMND_PILOTWIRE_STATUS       "status"
#define D_CMND_PILOTWIRE_TARGET       "target"
#define D_CMND_PILOTWIRE_WINDOW       "window"
#define D_CMND_PILOTWIRE_MVT          "mvt"
#define D_CMND_PILOTWIRE_LOW          "low"
#define D_CMND_PILOTWIRE_HIGH         "high"
#define D_CMND_PILOTWIRE_COMFORT      "comfort"
#define D_CMND_PILOTWIRE_NIGHT        "night"
#define D_CMND_PILOTWIRE_ECO          "eco"
#define D_CMND_PILOTWIRE_NOFROST      "nofrost"

#define D_JSON_PILOTWIRE              "Pilotwire"
#define D_JSON_PILOTWIRE_MODE         "Mode"
#define D_JSON_PILOTWIRE_STATUS       "Status"
#define D_JSON_PILOTWIRE_HEATING      "Heating"
#define D_JSON_PILOTWIRE_DETECT       "Detect"
#define D_JSON_PILOTWIRE_WINDOW       "Window"
#define D_JSON_PILOTWIRE_TARGET       "Target"
#define D_JSON_PILOTWIRE_TEMPERATURE  "Temperature"

#define D_PILOTWIRE                   "Pilotwire"
#define D_PILOTWIRE_CONFIGURE         "Configure"
#define D_PILOTWIRE_CONNEXION         "Connexion"
#define D_PILOTWIRE_MODE              "Mode"
#define D_PILOTWIRE_OPTION            "Options"
#define D_PILOTWIRE_GRAPH             "Graph"
#define D_PILOTWIRE_STATUS            "Status"
#define D_PILOTWIRE_COMFORT           "Comfort"
#define D_PILOTWIRE_ECO               "Economy"
#define D_PILOTWIRE_NOFROST           "No frost"
#define D_PILOTWIRE_TARGET            "Target"
#define D_PILOTWIRE_CONTROL           "Control"
#define D_PILOTWIRE_TEMPERATURE       "Temp."
#define D_PILOTWIRE_DIRECT            "Direct"
#define D_PILOTWIRE_CHECKED           "checked"
#define D_PILOTWIRE_WINDOW            "Open Window Detection"
#define D_PILOTWIRE_MVT               "Movement Detection"
#define D_PILOTWIRE_MOVEMENT          "Last movement"
#define D_PILOTWIRE_SOURCE_LOCAL      "Local"
#define D_PILOTWIRE_SOURCE_REMOTE     "Remote"
#define D_PILOTWIRE_SOURCE_UNDEFINED  "Undef."

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
#define PILOTWIRE_TEMP_SCALE_LOW          19        // limit between economy and medium temperature scale
#define PILOTWIRE_TEMP_SCALE_HIGH         21        // limit between economy and medium temperature scale

// constant : movement detection
#define PILOTWIRE_MOVEMENT_TIMEOUT        30        // timeout after last movement detection (in sec.)
#define PILOTWIRE_MOVEMENT_NOBODY_START   60        // if no mvt during 1h, nobody at home (in mn)
#define PILOTWIRE_MOVEMENT_NOBODY_DROP    30        // delay between 0.5°C target drop in NOBODY mode (in mn)
#define PILOTWIRE_MOVEMENT_VACANCY_START  2880      // if no mvt during 48h, home is vacant (in mn)

// constant : open window detection
#define PILOTWIRE_WINDOW_SAMPLE_NBR       24        // number of temperature samples to detect opened window (4mn for 1 sample every 10s)
#define PILOTWIRE_WINDOW_OPEN_PERIOD      10        // delay between 2 temperature samples in open window detection 
#define PILOTWIRE_WINDOW_OPEN_DROP        0.5       // temperature drop for window open detection (°C)
#define PILOTWIRE_WINDOW_CLOSE_INCREASE   0.2       // temperature increase to detect window closed (°C)  

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
const char D_CONF_FIELD_FULL[]     PROGMEM = "<p>%s<span class='key'>%s</span><br><input type='number' name='%s' min='%d' max='%d' step='%s' value='%s'></p>\n";

// device control
enum PilotwireDevices { PILOTWIRE_DEVICE_NORMAL, PILOTWIRE_DEVICE_DIRECT, PILOTWIRE_DEVICE_MAX };
const char kPilotwireDevice[] PROGMEM = "Pilotwire|Direct";                                                                 // device type labels

// device modes
enum PilotwireConfigs  { PILOTWIRE_CONFIG_OFF, PILOTWIRE_CONFIG_ON, PILOTWIRE_CONFIG_THERM, PILOTWIRE_CONFIG_MAX };         // device configuration modes
const char kPilotwireConfig[] PROGMEM = "Forced OFF|Forced ON|Thermostat";                                                  // device mode labels

// device temperature
enum PilotwireMode { PILOTWIRE_MODE_LOW, PILOTWIRE_MODE_HIGH, PILOTWIRE_MODE_COMFORT, PILOTWIRE_MODE_NIGHT, PILOTWIRE_MODE_ECO, PILOTWIRE_MODE_NOFROST, PILOTWIRE_MODE_MAX };
const char kPilotwireMode[] PROGMEM = "➖ Minimum|➕ Maximum|🔆 Comfort|💤 Night|🌙 Economy|❄ No frost";                   // temperature type labels

// device heating status
enum PilotwireHeating { PILOTWIRE_HEATING_OFF, PILOTWIRE_HEATING_ON, PILOTWIRE_HEATING_OFFLOAD, PILOTWIRE_HEATING_WINDOW, PILOTWIRE_HEATING_MAX };

// device running status
enum PilotwireStatus { PILOTWIRE_STATUS_OFF, PILOTWIRE_STATUS_ON, PILOTWIRE_STATUS_COMFORT, PILOTWIRE_STATUS_NIGHT, PILOTWIRE_STATUS_ECO, PILOTWIRE_STATUS_NOFROST, PILOTWIRE_STATUS_MAX };
const char kPilotwireStatus[] PROGMEM = "❌ OFF|🔥 ON|🔆 Comfort|💤 Night|🌙 Economy|❄ No frost";                          // thermostat option labels

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
const char kPilotwireCommand[] PROGMEM = D_CMND_PILOTWIRE_PREFIX "|" D_CMND_PILOTWIRE_HELP "|" D_CMND_PILOTWIRE_TYPE "|" D_CMND_PILOTWIRE_MODE "|" D_CMND_PILOTWIRE_TARGET "|" D_CMND_PILOTWIRE_WINDOW "|" D_CMND_PILOTWIRE_MVT "|" D_CMND_PILOTWIRE_STATUS "|" D_CMND_PILOTWIRE_LOW "|" D_CMND_PILOTWIRE_HIGH "|" D_CMND_PILOTWIRE_COMFORT "|" D_CMND_PILOTWIRE_NIGHT "|" D_CMND_PILOTWIRE_ECO "|" D_CMND_PILOTWIRE_NOFROST;
void (* const PilotwireCommand[])(void) PROGMEM = { &CmndPilotwireHelp, &CmndPilotwireType, &CmndPilotwireMode, &CmndPilotwireTarget, &CmndPilotwireWindow, &CmndPilotwireMvtDetect, &CmndPilotwireStatus, &CmndPilotwireLow, &CmndPilotwireHigh, &CmndPilotwireNormal, &CmndPilotwireNight, &CmndPilotwireNobody, &CmndPilotwireVacancy };

/****************************************\
 *               Icons
 * 
 *      xxd -i -c 256 icon.png
\****************************************/

// icon : none
#define PILOTWIRE_ICON_LOGO       "room.jpg"

#define PILOTWIRE_ICON_DEFAULT    "default.png"      
unsigned char pilotwire_default_png[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x04, 0x03, 0x00, 0x00, 0x00, 0x31, 0x10, 0x7c, 0xf8, 0x00, 0x00, 0x00, 0x0f, 0x50, 0x4c, 0x54, 0x45, 0x00, 0x00, 0x00, 0x34, 0x34, 0x34, 0x74, 0x74, 0x74, 0xaf, 0xaf, 0xaf, 0xff, 0xff, 0xff, 0x33, 0x73, 0x8d, 0x9c, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x01, 0x9b, 0x49, 0x44, 0x41, 0x54, 0x68, 0xde, 0xed, 0x99, 0x51, 0xae, 0x83, 0x20, 0x10, 0x45, 0xcb, 0x0e, 0x18, 0x74, 0x03, 0x98, 0x6e, 0xc0, 0xc6, 0x0d, 0x54, 0xd9, 0xff, 0x9a, 0x1e, 0x33, 0x08, 0x0e, 0x5a, 0xc1, 0x04, 0x3f, 0xfa, 0x9a, 0x21, 0xa6, 0x89, 0xde, 0xcb, 0x19, 0xa4, 0xe0, 0x07, 0xf7, 0xf1, 0xf8, 0x9e, 0x06, 0x9a, 0xdd, 0x28, 0xb8, 0x28, 0xb1, 0xc7, 0xbe, 0x6d, 0x3d, 0x7c, 0xd3, 0x17, 0x24, 0x5e, 0x04, 0x98, 0x8d, 0xba, 0x24, 0x5b, 0x41, 0xe2, 0x03, 0x98, 0xc6, 0xfe, 0x15, 0x05, 0xe8, 0x16, 0x58, 0x2c, 0x54, 0x25, 0x3e, 0x80, 0xde, 0x8d, 0xfe, 0x82, 0xb5, 0x8b, 0x5b, 0xf0, 0xd2, 0x35, 0x89, 0x0f, 0xc0, 0xa1, 0x6b, 0x81, 0xd8, 0x65, 0xc1, 0x07, 0xba, 0x22, 0x71, 0x40, 0x47, 0x2e, 0x67, 0x49, 0x80, 0x89, 0x5c, 0x33, 0x54, 0x24, 0xfe, 0x06, 0xcf, 0xe0, 0x7a, 0xa3, 0x4b, 0x19, 0x47, 0xae, 0x50, 0x34, 0x4a, 0x23, 0x97, 0xdc, 0x01, 0x30, 0x05, 0x17, 0x91, 0xb1, 0x26, 0xb9, 0xa8, 0x68, 0x41, 0xe2, 0x00, 0x17, 0x5c, 0x54, 0x54, 0xf5, 0xab, 0x8b, 0x8a, 0x16, 0x24, 0x36, 0x05, 0x66, 0x75, 0xd1, 0xd0, 0x70, 0xd0, 0xe4, 0xc2, 0x17, 0x2a, 0x48, 0xbb, 0x39, 0x64, 0xae,
  0x89, 0x03, 0xce, 0x25, 0xbc, 0x6f, 0x04, 0x84, 0x05, 0x8a, 0x4b, 0x73, 0x73, 0x61, 0x4b, 0xae, 0x8a, 0x04, 0x30, 0xf8, 0x15, 0xa5, 0x1a, 0x00, 0x06, 0x27, 0x53, 0xc1, 0x30, 0xd8, 0xcd, 0x65, 0x86, 0x81, 0xb9, 0x8a, 0x12, 0x6a, 0x08, 0xf0, 0x8e, 0x79, 0x73, 0x79, 0x87, 0x4d, 0xae, 0xb2, 0x84, 0x3f, 0x02, 0xb8, 0x09, 0xd0, 0xb0, 0x0e, 0x9a, 0x01, 0x26, 0x6e, 0x6b, 0xb6, 0xe0, 0x35, 0x5f, 0xf0, 0x50, 0x94, 0xa8, 0xf6, 0x1d, 0x9b, 0xa9, 0x65, 0x3b, 0x37, 0x7f, 0x0f, 0x04, 0x20, 0x00, 0x01, 0x08, 0x40, 0x00, 0x02, 0x10, 0x80, 0x00, 0x04, 0x20, 0x00, 0x01, 0x08, 0x40, 0x00, 0x02, 0x10, 0x80, 0x00, 0x04, 0xf0, 0x1f, 0x00, 0xf7, 0x9c, 0xea, 0xb6, 0x1c, 0x85, 0xdd, 0x74, 0xaa, 0xdb, 0x7a, 0x1c, 0x28, 0x67, 0xaa, 0x72, 0xaa, 0xfb, 0x43, 0xa7, 0xba, 0xcd, 0x31, 0x51, 0x96, 0x46, 0x01, 0x4f, 0xa3, 0x12, 0xe0, 0x28, 0x7d, 0x4a, 0xba, 0xe6, 0x2c, 0xce, 0xb2, 0xba, 0x2c, 0x7d, 0xc8, 0xda, 0xde, 0x59, 0xa0, 0x06, 0x99, 0xa4, 0x0f, 0x52, 0x9e, 0x57, 0xb2, 0x48, 0x0f, 0xb2, 0xb4, 0xef, 0x5c, 0xda, 0xcd, 0x22, 0xa5, 0x92, 0x7a, 0x4d, 0x08, 0x29, 0x95, 0x84, 0x8a, 0xb4, 0x0b, 0x1c, 0x59, 0x2e, 0x6a, 0xb2, 0xc8, 0xf4, 0x5c, 0xe2, 0x00, 0xff, 0x79, 0xeb, 0xe3, 0x73, 0x05, 0x4f, 0xef, 0x8a, 0x55, 0x0a, 0x52, 0xf6, 0x47, 0x1a, 0xdb, 0xa5, 0x6c, 0x18, 0x43, 0xb4, 0x31, 0x56, 0x29, 0x48, 0x87, 0xe0, 0x5a, 0x67, 0x29, 0xf4, 0x05, 0x69, 0x47, 0xd0, 0x59, 0x8e, 0x7d, 0x49, 0xfa, 0x86, 0xf6, 0x07, 0x1a, 0x58, 0x22, 0xd7, 0xc3, 0x2c, 0x71, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
unsigned int pilotwire_default_len = 508;

/*****************************************\
 *               Variables
\*****************************************/

// pilotwire : configuration
struct {
  bool    window      = false;                          // enable window open detection
  bool    mvt_detect  = false;                          // enable movement detection
  uint8_t device_type = UINT8_MAX;                      // pilotwire or direct connexion
  uint8_t device_mode = UINT8_MAX;                      // running mode (off, on, thermostat)
  float   arr_target[PILOTWIRE_MODE_MAX];               // array of target temperatures (according to status)
} pilotwire_config;

// pilotwire : general status
struct {
  uint8_t  device_state     = PILOTWIRE_STATUS_OFF;      // at startup, device state is considered OFF
  uint8_t  device_mode      = PILOTWIRE_STATUS_OFF;      // start heater OFF
  uint8_t  heating_mode     = PILOTWIRE_HEATING_OFF;     // heating is OFF at startup
  uint8_t  next_mode        = PILOTWIRE_STATUS_COMFORT;   // next status
  uint32_t next_time        = UINT32_MAX;                // next time status should change
  uint32_t last_time        = UINT32_MAX;                // last time status has changed
  uint8_t  json_update      = false;                     // flag to publish a json update
  float    json_temperature = NAN;                       // last published temperature
} pilotwire_status;

// pilotwire : temperature data
struct {
//  uint32_t source  = UINT32_MAX;                        // type of temperature sensor
  float    current = NAN;                               // current temperature
  float    target  = NAN;                               // target temperature
} pilotwire_temperature;

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
  bool     present = false;                             // enable movement detection   
  bool     active  = false;                             // movement actually detected   
  uint32_t last    = UINT32_MAX;                        // last movement detection timestamp
} pilotwire_mvt;

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

/******************************************************\
 *                     Functions
\******************************************************/

// get pilotwire device state from relay state
uint8_t PilotwireGetDeviceState ()
{
  uint8_t relay = 0;
  uint8_t state = 0;
    
  // read relay state
  relay = bitRead (TasmotaGlobal.power, 0);

  // if pilotwire connexion, convert to pilotwire state
  if (pilotwire_config.device_type == PILOTWIRE_DEVICE_NORMAL) { if (relay == 0) state = PILOTWIRE_STATUS_ON; else state = PILOTWIRE_STATUS_OFF; }

  // else, direct connexion, convert to pilotwire state
  else { if (relay == 0) state = PILOTWIRE_STATUS_OFF; else state = PILOTWIRE_STATUS_ON; }

  return state;
}

// set relays state
void PilotwireSetDeviceState (uint8_t new_state)
{
  uint32_t new_power = UINT32_MAX;

  // if pilotwire connexion
  if (pilotwire_config.device_type == PILOTWIRE_DEVICE_NORMAL)
  {
    // set power state according to mode
    if (new_state == PILOTWIRE_STATUS_OFF) ExecuteCommandPower (1, POWER_ON, SRC_MAX);
    else if (new_state == PILOTWIRE_STATUS_ON) ExecuteCommandPower (1, POWER_OFF, SRC_MAX);    
  }

  // else direct connexion
  else
  {
    // set power state according to mode
    if (new_state == PILOTWIRE_STATUS_OFF) ExecuteCommandPower (1, POWER_OFF, SRC_MAX);
    else if (new_state == PILOTWIRE_STATUS_ON) ExecuteCommandPower (1, POWER_ON, SRC_MAX);
  }

  // ask for JSON update
  pilotwire_status.json_update = true;
}

// read target temperature according to running status
float PilotwireGetTargetTemperature ()
{
  uint8_t  device_status;
  uint32_t delay;
  float    temperature, target, multiply;

  // switch according to temperature type
  device_status = pilotwire_status.device_mode;
  switch (device_status)
  {
    case PILOTWIRE_STATUS_COMFORT:
      target = pilotwire_temperature.target;
      break;

    case PILOTWIRE_STATUS_NIGHT:
    case PILOTWIRE_STATUS_ECO:
    case PILOTWIRE_STATUS_NOFROST:
      if (pilotwire_config.arr_target[device_status] > 0) target = pilotwire_config.arr_target[device_status];
      else target = pilotwire_temperature.target + pilotwire_config.arr_target[device_status];
      break;

    default:
      target = NAN;
      break;
  }

  // validate boundaries
  if (target < PILOTWIRE_TEMP_LIMIT_MIN) target = PILOTWIRE_TEMP_LIMIT_MIN;
  else if (target > PILOTWIRE_TEMP_LIMIT_MAX) target = PILOTWIRE_TEMP_LIMIT_MAX;

  return target;
}

// set different running temperatures
void PilotwireSetTemperature (uint8_t new_type, float new_temp, bool to_save = true)
{
  bool is_valid = false;
  char str_temp[8];
  char str_type[16];

  if (new_type < PILOTWIRE_MODE_MAX)
  {
    // check validity
    if (new_temp < 0) is_valid = true;
    else if ((new_temp > 0) && (new_temp >= PILOTWIRE_TEMP_LIMIT_MIN) && (new_temp <= PILOTWIRE_TEMP_LIMIT_MAX)) is_valid = true;

    // if valid, save temperature
    if (is_valid)
    {
      // set temperature and save config
      pilotwire_config.arr_target[new_type] = new_temp;
      if (to_save) PilotwireSaveConfig ();
      ext_snprintf_P (str_temp, sizeof (str_temp), PSTR ("%01_f"), &new_temp);
      GetTextIndexed (str_type, sizeof (str_type), new_type, kPilotwireMode);
      AddLog (LOG_LEVEL_INFO, PSTR ("PIL: %s temperature set to %s"), str_type, str_temp);
    }
  }
}

// set heater configuration mode
void PilotwireSetDeviceType (uint8_t new_type, bool to_save = true)
{
  char str_type[16];

  if (new_type < PILOTWIRE_DEVICE_MAX)
  {
    // set type
    pilotwire_config.device_type = new_type;

    // save and log action
    if (to_save) PilotwireSaveConfig ();
    GetTextIndexed (str_type, sizeof (str_type), pilotwire_config.device_type, kPilotwireDevice);
    AddLog (LOG_LEVEL_INFO, PSTR ("PIL: Device set to %s"), str_type);
  }
}

// set heater status mode
void PilotwireSetStatusMode (uint8_t new_mode, bool forced = false)
{
  uint8_t target_mode = UINT8_MAX;
  char    str_mode[16];

  // switch action according to mode
  switch (new_mode)
  {
    // switch heater off
    case PILOTWIRE_STATUS_OFF:
      if (forced || (pilotwire_status.device_mode != PILOTWIRE_STATUS_OFF))
      {
        // set target mode
        target_mode = PILOTWIRE_STATUS_OFF;
        pilotwire_status.next_mode = PILOTWIRE_STATUS_OFF;
        pilotwire_status.next_time = UINT32_MAX;
      }
      break;

    // force heater ON
    case PILOTWIRE_STATUS_ON:
      if (forced || (pilotwire_status.device_mode != PILOTWIRE_STATUS_ON))
      {
        // set target mode
        target_mode = PILOTWIRE_STATUS_ON;
        pilotwire_status.next_mode = PILOTWIRE_STATUS_ON;
        pilotwire_status.next_time = UINT32_MAX;
      }
      break;

    // set heater in normal thermostat mode
    case PILOTWIRE_STATUS_COMFORT:
      // set target mode
      target_mode = PILOTWIRE_STATUS_COMFORT;
      if (pilotwire_config.mvt_detect && pilotwire_mvt.present)
      {
        pilotwire_status.next_mode = PILOTWIRE_STATUS_ECO;
        pilotwire_status.next_time = LocalTime () + (PILOTWIRE_MOVEMENT_NOBODY_START * 60);
      }
      else
      {
        pilotwire_status.next_mode = PILOTWIRE_STATUS_COMFORT;
        pilotwire_status.next_time = UINT32_MAX;
      }
      break;

    // set heater in night thermostat mode
    case PILOTWIRE_STATUS_NIGHT:
      // if forced or actual status is NORMAL
      if (forced || (pilotwire_status.device_mode == PILOTWIRE_STATUS_COMFORT))
      {
        // set target mode
        target_mode = PILOTWIRE_STATUS_NIGHT;
        pilotwire_status.next_mode = PILOTWIRE_STATUS_COMFORT;
        pilotwire_status.next_time = UINT32_MAX;
      }
      break;

    // set heater in nobody thermostat mode
    case PILOTWIRE_STATUS_ECO:
      // if forced or actual status is NORMAL
      if (forced || (pilotwire_status.device_mode == PILOTWIRE_STATUS_COMFORT))
      {
        // set target mode
        target_mode = PILOTWIRE_STATUS_ECO;
        pilotwire_status.next_mode = PILOTWIRE_STATUS_NOFROST;
        pilotwire_status.next_time = LocalTime () + (PILOTWIRE_MOVEMENT_VACANCY_START * 60);
      }
      break;

    // set heater in vacancy thermostat mode
    case PILOTWIRE_STATUS_NOFROST:
      // if forced or actual status is NOBODY
      if (forced || (pilotwire_status.device_mode == PILOTWIRE_STATUS_ECO))
      {
        // set target mode
        target_mode = PILOTWIRE_STATUS_NOFROST;
        pilotwire_status.next_mode = PILOTWIRE_STATUS_NOFROST;
        pilotwire_status.next_time = UINT32_MAX;
      }
      break;
  }

  // if status should change
  if (target_mode != UINT8_MAX)
  {
    // set new status mode
    pilotwire_status.last_time   = LocalTime ();
    pilotwire_status.device_mode = target_mode;

    // reset open window detection
    PilotwireWindowResetDetection ();

    // ask for JSON update
    pilotwire_status.json_update = true;

    // log change
    GetTextIndexed (str_mode, sizeof (str_mode), target_mode, kPilotwireStatus);
    AddLog (LOG_LEVEL_INFO, PSTR ("PIL: Switch status to %s"), str_mode);
  }
}

// update movement detection status
void PilotwireMovementUpdateDetection ()
{
  bool     mvt_detected;
  uint32_t timestamp;

  timestamp = LocalTime ();

  // check if movement is actually detected 
  mvt_detected = SensorReadMovementBool ();

  // if movement just detected, set movement status
  if (mvt_detected && !pilotwire_mvt.active) pilotwire_mvt.active = true;

  // if movement actually detected
  if (mvt_detected)
  {
    // update last detection timestamp
    pilotwire_mvt.last = timestamp;

    // if needed, set back status to NORMAL
    if ((pilotwire_status.device_mode == PILOTWIRE_STATUS_ECO) || (pilotwire_status.device_mode == PILOTWIRE_STATUS_NOFROST)) 
      PilotwireSetStatusMode (PILOTWIRE_STATUS_COMFORT);
  }

  // else, if no movement after detection timeout, 
  else if (pilotwire_mvt.active && (timestamp > pilotwire_mvt.last + PILOTWIRE_MOVEMENT_TIMEOUT))
  {
    // reset movement detection
    pilotwire_mvt.active = false;

    // set next mode to NOBODY
    pilotwire_status.next_mode = PILOTWIRE_STATUS_ECO;
    pilotwire_status.next_time = timestamp + (PILOTWIRE_MOVEMENT_NOBODY_START * 60);
  }
}

// reset window detection data
void PilotwireWindowResetDetection ()
{
  uint8_t index;

  // declare window closed
  pilotwire_window.opened   = false;
  pilotwire_window.low_temp = NAN;

  // reset detection temperatures
  for (index = 0; index < PILOTWIRE_WINDOW_SAMPLE_NBR; index ++) pilotwire_window.arr_temp[index] = NAN;
}

// update opened window detection data
void PilotwireWindowUpdateDetection ()
{
  uint8_t index, idx_array;
  float   delta = 0;

  // if window considered closed, do open detection
  if (!pilotwire_window.opened)
  {
    // if temperature is available, update temperature detection array
    if (!isnan (pilotwire_temperature.current))
    {
      // if period reached
      if (pilotwire_window.period == 0)
      {
        // record temperature and increment index
        pilotwire_window.arr_temp[pilotwire_window.idx_temp] = pilotwire_temperature.current;
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
      if (!isnan (pilotwire_window.arr_temp[idx_array])) delta = max (delta, pilotwire_window.arr_temp[idx_array] - pilotwire_temperature.current);

      // if temperature drop detected, stop analysis
      if (delta >= PILOTWIRE_WINDOW_OPEN_DROP) break;
    }

    // if temperature drop detected, window is detected as opended
    if (delta >= PILOTWIRE_WINDOW_OPEN_DROP)
    {
      pilotwire_window.opened   = true;
      pilotwire_window.low_temp = pilotwire_temperature.current;
    }
  }

  // else, window detected as opened, try to detect closure
  else
  {
    // update lower temperature
    pilotwire_window.low_temp = min (pilotwire_window.low_temp, pilotwire_temperature.current);

    // if current temperature has increased enought, window is closed
    if (pilotwire_temperature.current - pilotwire_window.low_temp >= PILOTWIRE_WINDOW_CLOSE_INCREASE) PilotwireWindowResetDetection ();
  }
}

// set heater configuration mode
void PilotwireSetConfigMode (uint8_t new_mode, bool to_save = true)
{
  char str_mode[16];

  // switch action according to mode
  switch (new_mode)
  {
    // switch OFF heater
    case PILOTWIRE_CONFIG_OFF:
      pilotwire_config.device_mode = PILOTWIRE_CONFIG_OFF;
      PilotwireSetStatusMode (PILOTWIRE_STATUS_OFF, true);
      break;

    // force heater ON
    case PILOTWIRE_CONFIG_ON:
      pilotwire_config.device_mode = PILOTWIRE_CONFIG_ON;
      PilotwireSetStatusMode (PILOTWIRE_STATUS_ON, true);
      break;

    // set heater in thermostat mode
    case PILOTWIRE_CONFIG_THERM:
      pilotwire_config.device_mode = PILOTWIRE_CONFIG_THERM;
      if (pilotwire_status.device_mode < PILOTWIRE_STATUS_COMFORT) PilotwireSetStatusMode (PILOTWIRE_STATUS_COMFORT, true);
      break;
  }

  // save and log action
  if (to_save) PilotwireSaveConfig ();
  GetTextIndexed (str_mode, sizeof (str_mode), pilotwire_config.device_mode, kPilotwireConfig);
  AddLog (LOG_LEVEL_INFO, PSTR ("PIL: Switch config to %s"), str_mode);
}

// set current target temperature
void PilotwireSetCurrentTarget (float target)
{
  char str_value[8];

  // if target is within range
  if ((target >= pilotwire_config.arr_target[PILOTWIRE_MODE_LOW]) && (target <= pilotwire_config.arr_target[PILOTWIRE_MODE_HIGH]))
  {
    // set normal thermostat mode and target temperature
    PilotwireSetStatusMode (PILOTWIRE_STATUS_COMFORT);
    pilotwire_temperature.target = target;

    // log new target
    ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%01_f"), &pilotwire_temperature.target);
    AddLog (LOG_LEVEL_INFO, PSTR ("PIL: Set target temp. to %s"), str_value);
  }

  // update JSON
  pilotwire_status.json_update = true;
}

/**************************************************\
 *                  Commands
\**************************************************/


// offload help
void CmndPilotwireHelp ()
{
  uint8_t index;
  char    str_text[32];

  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: pw_type = heater type"));
  for (index = 0; index < offload_config.nbr_device_type; index++)
  {
    GetTextIndexed (str_text, sizeof (str_text), offload_config.arr_device_type[index], kOffloadDevice);
    AddLog (LOG_LEVEL_INFO, PSTR ("HLP:   %u - %s"), offload_config.arr_device_type[index], str_text);
  }

  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: pw_mode   = heater default configuration mode"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: pw_status = heater current mode"));
  for (index = 0; index < PILOTWIRE_CONFIG_MAX; index++)
  {
    GetTextIndexed (str_text, sizeof (str_text), index, kPilotwireConfig);
    AddLog (LOG_LEVEL_INFO, PSTR ("HLP:   %u - %s"), index, str_text);
  }

  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: pw_window  = enable open window mode (0/1)"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: pw_mvt     = enable movement detection (0/1)"));

  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: pw_target  = target temperature (°C)"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: pw_low     = minimum temperature (°C)"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: pw_high    = maximum temperature (°C)"));

  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: pw_comfort = comfort temperature (°C)"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: pw_night   = night temperature (delta with comfort if negative)"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: pw_eco     = eco temperature (delta with comfort if negative)"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: pw_nofrost = no frost temperature (delta with comfort if negative)"));

  ResponseCmndDone();
}

void CmndPilotwireType ()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetDeviceType ((uint8_t)XdrvMailbox.payload);
  ResponseCmndNumber (pilotwire_config.device_type);
}

void CmndPilotwireMode ()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetConfigMode ((uint8_t)XdrvMailbox.payload);
  ResponseCmndNumber (pilotwire_config.device_mode);
}

void CmndPilotwireTarget ()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetCurrentTarget (atof(XdrvMailbox.data));
  ResponseCmndFloat (pilotwire_temperature.target, 1);
}

void CmndPilotwireWindow ()
{
  if (XdrvMailbox.data_len > 0) pilotwire_config.window = (XdrvMailbox.payload != 0);
  ResponseCmndNumber (pilotwire_config.window);
}

void CmndPilotwireMvtDetect ()
{
  if (XdrvMailbox.data_len > 0) pilotwire_config.mvt_detect = (XdrvMailbox.payload != 0);
  ResponseCmndNumber (pilotwire_config.mvt_detect);
}

void CmndPilotwireStatus ()
{
  uint8_t new_status;

  if (XdrvMailbox.data_len > 0)
  {
    if (strcasecmp (XdrvMailbox.data, "off") == 0) new_status = PILOTWIRE_STATUS_OFF;
    else if (strcasecmp (XdrvMailbox.data, "on") == 0) new_status = PILOTWIRE_STATUS_COMFORT;
    else new_status = (uint8_t)XdrvMailbox.payload;
    PilotwireSetStatusMode (new_status, true);
  }
  ResponseCmndNumber (pilotwire_status.device_mode);
}

void CmndPilotwireLow ()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetTemperature (PILOTWIRE_MODE_LOW, atof(XdrvMailbox.data));
  ResponseCmndFloat (pilotwire_config.arr_target[PILOTWIRE_MODE_LOW], 1);
}

void CmndPilotwireHigh ()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetTemperature (PILOTWIRE_MODE_HIGH, atof(XdrvMailbox.data));
  ResponseCmndFloat (pilotwire_config.arr_target[PILOTWIRE_MODE_HIGH], 1);
}

void CmndPilotwireNormal ()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetTemperature (PILOTWIRE_MODE_COMFORT, atof(XdrvMailbox.data));
  ResponseCmndFloat (pilotwire_config.arr_target[PILOTWIRE_MODE_COMFORT], 1);
}

void CmndPilotwireNight ()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetTemperature (PILOTWIRE_MODE_NIGHT, atof(XdrvMailbox.data));
  ResponseCmndFloat (pilotwire_config.arr_target[PILOTWIRE_MODE_NIGHT], 1);
}

void CmndPilotwireNobody ()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetTemperature (PILOTWIRE_MODE_ECO, atof(XdrvMailbox.data));
  ResponseCmndFloat (pilotwire_config.arr_target[PILOTWIRE_MODE_ECO], 1);
}

void CmndPilotwireVacancy ()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetTemperature (PILOTWIRE_MODE_NOFROST, atof(XdrvMailbox.data));
  ResponseCmndFloat (pilotwire_config.arr_target[PILOTWIRE_MODE_NOFROST], 1);
}

/**************************************************\
 *                  Configuration
\**************************************************/

void PilotwireLoadConfig () 
{
  uint8_t device_mode;

#ifdef USE_UFILESYS

  // retrieve saved settings from flash filesystem
  device_mode = (uint8_t)UfsCfgLoadKeyInt (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_MODE, 0);
  pilotwire_config.device_type = (uint8_t)UfsCfgLoadKeyInt (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_TYPE, 0);
  pilotwire_config.window = (bool)UfsCfgLoadKeyInt (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_WINDOW, 0);
  pilotwire_config.mvt_detect = (bool)UfsCfgLoadKeyInt (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_MVT, 0);

  pilotwire_config.arr_target[PILOTWIRE_MODE_LOW] = UfsCfgLoadKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_LOW, PILOTWIRE_TEMP_DEFAULT_LOW);
  pilotwire_config.arr_target[PILOTWIRE_MODE_HIGH] = UfsCfgLoadKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_HIGH, PILOTWIRE_TEMP_DEFAULT_HIGH);
  pilotwire_config.arr_target[PILOTWIRE_MODE_COMFORT] = UfsCfgLoadKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_COMFORT, PILOTWIRE_TEMP_DEFAULT_NORMAL);
  pilotwire_config.arr_target[PILOTWIRE_MODE_NIGHT] = UfsCfgLoadKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_NIGHT, 0);
  pilotwire_config.arr_target[PILOTWIRE_MODE_ECO] = UfsCfgLoadKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_ECO, 0);
  pilotwire_config.arr_target[PILOTWIRE_MODE_NOFROST] = UfsCfgLoadKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_NOFROST, 0);

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("PIL: Config from LittleFS"));

#else       // No LittleFS

  // retrieve saved settings from flash memory
  device_mode = (uint8_t)Settings->knx_CB_addr[0];
  pilotwire_config.device_type = (uint8_t)Settings->knx_CB_addr[1];
  pilotwire_config.window = (bool)Settings->knx_CB_addr[8];
  pilotwire_config.mvt_detect = (bool)Settings->knx_CB_addr[9];

  pilotwire_config.arr_target[PILOTWIRE_MODE_LOW]     = (float)Settings->knx_CB_addr[2] / 10;
  pilotwire_config.arr_target[PILOTWIRE_MODE_HIGH]    = (float)Settings->knx_CB_addr[3] / 10;
  pilotwire_config.arr_target[PILOTWIRE_MODE_COMFORT]  = (float)Settings->knx_CB_addr[4] / 10;
  pilotwire_config.arr_target[PILOTWIRE_MODE_NIGHT]   = ((float)Settings->knx_CB_addr[5] - 1000) / 10;
  pilotwire_config.arr_target[PILOTWIRE_MODE_ECO]  = ((float)Settings->knx_CB_addr[6] - 1000) / 10;
  pilotwire_config.arr_target[PILOTWIRE_MODE_NOFROST] = ((float)Settings->knx_CB_addr[7] - 1000) / 10;
   // log
  AddLog (LOG_LEVEL_INFO, PSTR ("PIL: Config from Settings"));

# endif     // USE_UFILESYS

  // avoid values out of range
  if (device_mode >= PILOTWIRE_CONFIG_MAX) device_mode = PILOTWIRE_CONFIG_THERM;
  if (pilotwire_config.device_type > PILOTWIRE_DEVICE_DIRECT) pilotwire_config.device_type = PILOTWIRE_DEVICE_NORMAL;
  if (pilotwire_config.arr_target[PILOTWIRE_MODE_LOW] < PILOTWIRE_TEMP_LIMIT_MIN) pilotwire_config.arr_target[PILOTWIRE_MODE_LOW] = PILOTWIRE_TEMP_DEFAULT_LOW;
  if (pilotwire_config.arr_target[PILOTWIRE_MODE_LOW] > PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_target[PILOTWIRE_MODE_LOW] = PILOTWIRE_TEMP_DEFAULT_LOW;
  if (pilotwire_config.arr_target[PILOTWIRE_MODE_HIGH] < PILOTWIRE_TEMP_LIMIT_MIN) pilotwire_config.arr_target[PILOTWIRE_MODE_HIGH] = PILOTWIRE_TEMP_DEFAULT_HIGH;
  if (pilotwire_config.arr_target[PILOTWIRE_MODE_HIGH] > PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_target[PILOTWIRE_MODE_HIGH] = PILOTWIRE_TEMP_DEFAULT_HIGH;
  if (pilotwire_config.arr_target[PILOTWIRE_MODE_COMFORT] < PILOTWIRE_TEMP_LIMIT_MIN) pilotwire_config.arr_target[PILOTWIRE_MODE_COMFORT] = PILOTWIRE_TEMP_DEFAULT_NORMAL;
  if (pilotwire_config.arr_target[PILOTWIRE_MODE_COMFORT] > PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_target[PILOTWIRE_MODE_COMFORT] = PILOTWIRE_TEMP_DEFAULT_NORMAL;
  if (pilotwire_config.arr_target[PILOTWIRE_MODE_NIGHT] < -PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_target[PILOTWIRE_MODE_NIGHT] = PILOTWIRE_TEMP_DEFAULT_NIGHT;
  if (pilotwire_config.arr_target[PILOTWIRE_MODE_NIGHT] >  PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_target[PILOTWIRE_MODE_NIGHT] = PILOTWIRE_TEMP_DEFAULT_NIGHT;
  if (pilotwire_config.arr_target[PILOTWIRE_MODE_ECO] < -PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_target[PILOTWIRE_MODE_ECO] = PILOTWIRE_TEMP_DEFAULT_ECO;
  if (pilotwire_config.arr_target[PILOTWIRE_MODE_ECO] >  PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_target[PILOTWIRE_MODE_ECO] = PILOTWIRE_TEMP_DEFAULT_ECO;
  if (pilotwire_config.arr_target[PILOTWIRE_MODE_NOFROST] < -PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_target[PILOTWIRE_MODE_NOFROST] = PILOTWIRE_TEMP_DEFAULT_NOFROST;
  if (pilotwire_config.arr_target[PILOTWIRE_MODE_NOFROST] >  PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_target[PILOTWIRE_MODE_NOFROST] = PILOTWIRE_TEMP_DEFAULT_NOFROST;

  // set configuration mode without saving it back
  PilotwireSetConfigMode (device_mode, false);
}

void PilotwireSaveConfig () 
{
#ifdef USE_UFILESYS

  // save settings into flash filesystem
  UfsCfgSaveKeyInt (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_MODE, (int)pilotwire_config.device_mode, true);
  UfsCfgSaveKeyInt (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_TYPE, (int)pilotwire_config.device_type, false);
  UfsCfgSaveKeyInt (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_WINDOW, (int)pilotwire_config.window, false);
  UfsCfgSaveKeyInt (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_MVT, (int)pilotwire_config.mvt_detect, false);

  UfsCfgSaveKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_LOW, pilotwire_config.arr_target[PILOTWIRE_MODE_LOW], false);
  UfsCfgSaveKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_HIGH, pilotwire_config.arr_target[PILOTWIRE_MODE_HIGH], false);
  UfsCfgSaveKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_COMFORT, pilotwire_config.arr_target[PILOTWIRE_MODE_COMFORT], false);
  UfsCfgSaveKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_NIGHT, pilotwire_config.arr_target[PILOTWIRE_MODE_NIGHT], false);
  UfsCfgSaveKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_ECO, pilotwire_config.arr_target[PILOTWIRE_MODE_ECO], false);
  UfsCfgSaveKeyFloat (D_PILOTWIRE_FILE_CFG, D_CMND_PILOTWIRE_NOFROST, pilotwire_config.arr_target[PILOTWIRE_MODE_NOFROST], false);

# else       // No LittleFS

  // save settings into flash memory
  Settings->knx_CB_addr[0] = (uint16_t)pilotwire_config.device_mode;
  Settings->knx_CB_addr[1] = (uint16_t)pilotwire_config.device_type;
  Settings->knx_CB_addr[8] = (uint16_t)pilotwire_config.window;
  Settings->knx_CB_addr[9] = (uint16_t)pilotwire_config.mvt_detect;

  Settings->knx_CB_addr[2] = (uint16_t)(10 * pilotwire_config.arr_target[PILOTWIRE_MODE_LOW]);
  Settings->knx_CB_addr[3] = (uint16_t)(10 * pilotwire_config.arr_target[PILOTWIRE_MODE_HIGH]);
  Settings->knx_CB_addr[4] = (uint16_t)(10 * pilotwire_config.arr_target[PILOTWIRE_MODE_COMFORT]);
  Settings->knx_CB_addr[5] = (uint16_t)(1000 + (10 * pilotwire_config.arr_target[PILOTWIRE_MODE_NIGHT]));
  Settings->knx_CB_addr[6] = (uint16_t)(1000 + (10 * pilotwire_config.arr_target[PILOTWIRE_MODE_ECO]));
  Settings->knx_CB_addr[7] = (uint16_t)(1000 + (10 * pilotwire_config.arr_target[PILOTWIRE_MODE_NOFROST]));

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
  if (time_dst.day_of_week == 2) UfsCsvFileRotate (D_PILOTWIRE_HISTO_FILE_WEEK, 0, PILOTWIRE_HISTO_WEEK_MAX);

  // clean old CSV files
  UfsCsvCleanupFileSystem (PILOTWIRE_HISTO_MIN_FREE);
}

// save current data to histo file
void PilotwireHistoSaveData (uint32_t period)
{
  long     index;
  float    temperature;
  TIME_T   now_dst;
  char str_filename[UFS_FILENAME_SIZE];
  char str_value[32];
  char str_line[64];

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
  ext_snprintf_P (str_value, sizeof (str_value), PSTR (";%1_f"), &pilotwire_temperature.current);
  strlcat (str_line, str_value, sizeof (str_line));

  // append target temperature
  temperature = PilotwireGetTargetTemperature ();
  ext_snprintf_P (str_value, sizeof (str_value), PSTR (";%1_f"), &temperature);
  strlcat (str_line, str_value, sizeof (str_line));

  // append heater status
  sprintf_P (str_value, PSTR (";%d"), PilotwireGetDeviceState ());
  strlcat (str_line, str_value, sizeof (str_line));

  // if file doesn't exist, add header
  if (!ffsp->exists (str_filename)) UfsCsvAppend (str_filename, "Idx;Date;Time;Temp;Target;Status", true);

  // add current line
  UfsCsvAppend (str_filename, str_line, false);
}

# endif     // USE_UFILESYS


// intercept command to relay 
//  - if coming from timer : relay ON = switch to night mode, relay OFF = switch back to normal mode
//  - if coming from anything else than SRC_MAX : ignore
bool PilotwireSetDevicePower ()
{
  bool     result = false;
  uint32_t target;

  // if command is from a timer, handle night mode
  if (XdrvMailbox.payload == SRC_TIMER)
  {
    // get target state of first relay (off = normal, on = night mode)
    target = XdrvMailbox.index & 1;

    // if relay OFF and night mode ON : night mode should be disabled
    if (!target && (pilotwire_status.device_mode == PILOTWIRE_STATUS_NIGHT)) PilotwireSetStatusMode (PILOTWIRE_STATUS_COMFORT);
 
    // relay ON and normal mode : night mode should be enabled
    else if (target && (pilotwire_status.device_mode == PILOTWIRE_STATUS_COMFORT)) PilotwireSetStatusMode (PILOTWIRE_STATUS_NIGHT);

    // ignore action
    result = true;
  }

  // else if command is not from the module, ignore it
  else if (XdrvMailbox.payload != SRC_MAX)
  {
    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("PIL: Relay order ignored from %u"), XdrvMailbox.payload);

    // ignore action
    result = true;
  }

  return result;
}

// Rotate files at day change
void PilotwireRotateAtMidnight ()
{
#ifdef USE_UFILESYS
  // if filesystem is available, save data to history file
  if (ufs_type) PilotwireHistoRotate ();

#endif    // USE_UFILESYS
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
  //   "PilotWire":{"Mode":2,"Status":4,"Temperature":18.6,"Target":21.0,"Detect":262}
  ResponseAppend_P (PSTR ("\"%s\":{"), D_JSON_PILOTWIRE);

  // current mode
  ResponseAppend_P (PSTR ("\"%s\":%d"), D_JSON_PILOTWIRE_MODE, pilotwire_config.device_mode);

  // current status
  ResponseAppend_P (PSTR (",\"%s\":%d"), D_JSON_PILOTWIRE_STATUS, pilotwire_status.device_mode);

  // heating state
  ResponseAppend_P (PSTR (",\"%s\":%d"), D_JSON_PILOTWIRE_HEATING, pilotwire_status.heating_mode);

  // add current temperature
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &pilotwire_temperature.current);
  ResponseAppend_P (PSTR (",\"%s\":%s"), D_JSON_PILOTWIRE_TEMPERATURE, str_value);

  // target temperature
  temperature = PilotwireGetTargetTemperature ();
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &temperature);
  ResponseAppend_P (PSTR (",\"%s\":%s"), D_JSON_PILOTWIRE_TARGET, str_value);

  // if window open detection enabled, add status
  if (pilotwire_config.window) ResponseAppend_P (PSTR (",\"%s\":%u"), D_JSON_PILOTWIRE_WINDOW, pilotwire_window.opened);

  // if movement detection enabled, add last detection (in minutes)
  if (pilotwire_mvt.last != UINT32_MAX) ResponseAppend_P (PSTR (",\"%s\":%u"), D_JSON_PILOTWIRE_DETECT, (LocalTime () - pilotwire_mvt.last) / 60);

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
  pilotwire_status.json_update      = false;
  pilotwire_status.json_temperature = pilotwire_temperature.current;
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
    if (!isnan (pilotwire_temperature.current) && (index < PILOTWIRE_GRAPH_SAMPLE))
    {
      // set current temperature
      value = pilotwire_temperature.current * 10;
      pilotwire_graph.arr_temp[period][index] = (short)value;

      // set target temperature
      value = pilotwire_temperature.target * 10;
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

// update pilotwire status
void PilotwireEvery250ms ()
{
  bool    is_offloaded;
  uint8_t device_state, target_state, target_heating;
  float   target_temp;

  // update device state et set target state identical
  pilotwire_status.device_state = PilotwireGetDeviceState ();
  target_state = pilotwire_status.device_state;
  is_offloaded = OffloadIsOffloaded ();

  // -----------------------
  //   Update target state
  // -----------------------

  // if device is offloaded, target state is off
  if (is_offloaded) target_state = PILOTWIRE_STATUS_OFF;

  // else if window is opened, target status is OFF
  else if (pilotwire_window.opened) target_state = PILOTWIRE_STATUS_OFF;

  // else if status is OFF
  else if (pilotwire_status.device_mode == PILOTWIRE_STATUS_OFF) target_state = PILOTWIRE_STATUS_OFF;

  // else if status is ON
  else if (pilotwire_status.device_mode == PILOTWIRE_STATUS_ON) target_state = PILOTWIRE_STATUS_ON;

  // else thermostat mode, check according to target temperature
  else
  {
    // get current target temperature
    target_temp = PilotwireGetTargetTemperature ();

    // if temperature is too low, target state is on
    if (pilotwire_temperature.current < (target_temp - PILOTWIRE_TEMP_THRESHOLD)) target_state = PILOTWIRE_STATUS_ON;

    // else, if too high, target state is off
    else if (pilotwire_temperature.current > (target_temp + PILOTWIRE_TEMP_THRESHOLD)) target_state = PILOTWIRE_STATUS_OFF;
  }

  // if device target state has changed
  if (target_state != pilotwire_status.device_state) PilotwireSetDeviceState (target_state);

  // -----------------------
  //   Update heating state
  // -----------------------

  // default target is OFF
  target_heating = PILOTWIRE_HEATING_OFF;

  // if device is offloaded, target state is off
  if (is_offloaded) target_heating = PILOTWIRE_HEATING_OFFLOAD;

  // else if window is opened
  else if (pilotwire_window.opened) target_heating = PILOTWIRE_HEATING_WINDOW;

  // else if pilotwire mode
  else if (pilotwire_config.device_type == PILOTWIRE_DEVICE_NORMAL)
  {
    // if status ON, heating considered as ON
    if (target_state == PILOTWIRE_STATUS_ON) target_heating = PILOTWIRE_HEATING_ON;
  } 

  // else if direct mode
  else if (pilotwire_config.device_type == PILOTWIRE_DEVICE_DIRECT)
  {
    // if no power sensor
    if (isnan (Energy.apparent_power[0]))
    {
      // if status ON, heating considered as ON
      if (target_state == PILOTWIRE_STATUS_ON) target_heating = PILOTWIRE_HEATING_ON;
    } 

    // else power is read, if it is beyond minimum value, heating considered as ON
    else if (Energy.apparent_power[0] >= PILOTWIRE_POWER_MINIMUM) target_heating = PILOTWIRE_HEATING_ON;
  } 

  // update heating state
  if (target_heating != pilotwire_status.heating_mode)
  {
    pilotwire_status.heating_mode = target_heating;
    pilotwire_status.json_update  = true;
  }

  // ---------------
  //   Update JSON
  // ---------------

  // if needed, publish new state
  if (pilotwire_status.json_update) PilotwireShowJSON (true);
}

// called every second, to update
//  - temperature
//  - presence detection
//  - graph data
void PilotwireEverySecond ()
{
  uint32_t counter;

  // check if movement detection is active
  counter = SensorSourceMovement ();
  pilotwire_mvt.present = (counter != SENSOR_SOURCE_MAX);

  // update temperature and update JSON if temperature has changed of more than 0.1°C
  pilotwire_temperature.current = SensorReadTemperatureFloat ();
  if (!isnan (pilotwire_temperature.current) && (abs (pilotwire_status.json_temperature - pilotwire_temperature.current) > 0.1))
  {
    pilotwire_status.json_temperature = pilotwire_temperature.current;
    pilotwire_status.json_update = true;
  }

  // if next mode is defined, set next mode as target
  if (pilotwire_status.next_time < LocalTime ()) PilotwireSetStatusMode (pilotwire_status.next_mode);

  // update movement detection
  if (pilotwire_config.mvt_detect && pilotwire_mvt.present) PilotwireMovementUpdateDetection ();

  // update window open detection
  if (pilotwire_config.window) PilotwireWindowUpdateDetection ();

  // loop thru the periods, to update graph data to the max on the period
  for (counter = 0; counter < PILOTWIRE_PERIOD_MAX; counter++)
  {
    // update graph temperature for current period (keep minimal value)
    if (isnan (pilotwire_graph.temperature[counter])) pilotwire_graph.temperature[counter] = pilotwire_temperature.current;
    else if (!isnan (pilotwire_temperature.current)) pilotwire_graph.temperature[counter] = min (pilotwire_graph.temperature[counter], pilotwire_temperature.current);

    // update device state
    if (pilotwire_graph.device_state[counter] != PILOTWIRE_STATUS_ON) pilotwire_graph.device_state[counter] = pilotwire_status.device_state;

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

  // init movement detection data
  pilotwire_mvt.last    = UINT32_MAX;
  pilotwire_mvt.present = PinUsed (GPIO_SWT1, 0);

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

  // set default temperature
  pilotwire_temperature.target = pilotwire_config.arr_target[PILOTWIRE_MODE_COMFORT];

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: pw_help to get help on pilotwire commands"));
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// icon : room logo
#ifdef USE_UFILESYS
void PilotwireWebIconLogo ()
{
  char str_name[32];
  File file;

  // open file in read only mode in littlefs filesystem
  sprintf_P (str_name, PSTR("/%s"), PILOTWIRE_ICON_LOGO);
  file = ffsp->open (str_name, "r");
  Webserver->streamFile (file, "image/jpeg", HTTP_GET);
  file.close ();
}
#endif    // USE_UFILESYS

// icons
void PilotwireWebIconDefault () { Webserver->send (200, "image/png", pilotwire_default_png,   pilotwire_default_len); }

// get status logo : flame, window or offload
void PilotwireWebIconStatus (char* pstr_status)
{
  // verification
  if (pstr_status == nullptr) return;


  // get logo
  if (OffloadIsOffloaded ()) strcpy (pstr_status, "⚡");
  else if (pilotwire_window.opened) strcpy (pstr_status, "🪟");
  else if (pilotwire_status.device_state == PILOTWIRE_STATUS_ON) strcpy (pstr_status, "🔥");
  else if (pilotwire_mvt.active) strcpy (pstr_status, "👋");
  else strcpy (pstr_status, "&nbsp;");
}

// append pilot wire state to main page
void PilotwireWebSensor ()
{
  float temperature;
  char  str_text[32];

  // if target temperature is defined in current running status
  if (pilotwire_status.device_mode >= PILOTWIRE_STATUS_COMFORT)
  {
    temperature = PilotwireGetTargetTemperature ();
    ext_snprintf_P (str_text, sizeof (str_text), PSTR ("%01_f"), &temperature);

    // add temperature unit
    strlcat (str_text, " °C", sizeof (str_text));

    // display temperature
    WSContentSend_PD (PSTR ("{s}%s{m}%s{e}\n"), D_PILOTWIRE_TARGET, str_text);
  }

  // display config and status mode
  GetTextIndexed (str_text, sizeof (str_text), pilotwire_status.device_mode, kPilotwireStatus);
  WSContentSend_PD (PSTR ("{s}%s{m}%s{e}\n"), D_PILOTWIRE_MODE, str_text);

  // if movement detection is enabled
  if (pilotwire_config.mvt_detect && pilotwire_mvt.present)
  {
    if (pilotwire_mvt.last == UINT32_MAX) strcpy (str_text, "---");
      else OffloadGenerateTime (str_text, sizeof (str_text), LocalTime () - pilotwire_mvt.last);
    WSContentSend_PD (PSTR ("{s}%s{m}%s{e}\n"), D_PILOTWIRE_MOVEMENT, str_text);
  }
}

// intermediate page to update selected status from main page
void PilotwireWebPageSwitchStatus ()
{
  char str_argument[16];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // check for 'therm' parameter
  WebGetArg (D_CMND_PILOTWIRE_STATUS, str_argument, sizeof (str_argument));
  if (strlen (str_argument) > 0) PilotwireSetStatusMode ((uint8_t)atoi (str_argument), true);

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
  char     str_value[16];
  char     str_text[32];

  // control button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_PAGE_PILOTWIRE_CONTROL, D_PILOTWIRE_CONTROL);

  // graph button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_PAGE_PILOTWIRE_GRAPH, D_PILOTWIRE_GRAPH);

  // status mode options
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'>\n"), D_PAGE_PILOTWIRE_STATUS);
  WSContentSend_P (PSTR ("<select style='width:100%%;text-align:center;padding:8px;border-radius:0.3rem;' name='%s' onchange='this.form.submit();'>\n"), D_CMND_PILOTWIRE_STATUS, pilotwire_status.device_mode);
  WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>\n"), UINT8_MAX, "", PSTR ("-- Select mode --"));
  for (index = 0; index < PILOTWIRE_STATUS_MAX; index ++)
  {
    // status label
    GetTextIndexed (str_text, sizeof (str_text), index, kPilotwireStatus);

    // get temperature
    if (index >= PILOTWIRE_STATUS_COMFORT)
    {
      ext_snprintf_P (str_value, sizeof (str_value), PSTR (" (%01_f)"), &pilotwire_config.arr_target[index]);
      strlcat (str_text, str_value, sizeof (str_text));
    } 

    // display status mode
    WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>\n"), index, "", str_text);
  }
  WSContentSend_P (PSTR ("</select>\n"));
  WSContentSend_P (PSTR ("</form></p>\n"));
}

// Pilotwire heater configuration web page
void PilotwireWebPageConfigure ()
{
  uint8_t value;
  float   temperature;
  char    str_argument[24];
  char    str_value[16];
  char    str_step[8];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg (F ("save")))
  {
    // get pilotwire device type according to 'type' parameter
    WebGetArg (D_CMND_PILOTWIRE_TYPE, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) PilotwireSetDeviceType ((uint8_t)atoi (str_argument), false);

    // get pilotwire open window detection setting according to 'window' parameter
    WebGetArg (D_CMND_PILOTWIRE_WINDOW, str_argument, sizeof (str_argument));
    pilotwire_config.window = (strcmp (str_argument, "on") == 0);

    // get pilotwire movement detection setting according to 'mvt' parameter
    WebGetArg (D_CMND_PILOTWIRE_MVT, str_argument, sizeof (str_argument));
    pilotwire_config.mvt_detect = (strcmp (str_argument, "on") == 0);

    // get minimum temperature according to 'tlow' parameter
    WebGetArg (D_CMND_PILOTWIRE_LOW, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) PilotwireSetTemperature (PILOTWIRE_MODE_LOW, atof (str_argument), false);

    // get minimum temperature according to 'thigh' parameter
    WebGetArg (D_CMND_PILOTWIRE_HIGH, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) PilotwireSetTemperature (PILOTWIRE_MODE_HIGH, atof (str_argument), false);

    // get normal mode temperature according to 'tnormal' parameter
    WebGetArg (D_CMND_PILOTWIRE_COMFORT, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) PilotwireSetTemperature (PILOTWIRE_MODE_COMFORT, atof (str_argument), false);

    // get night mode temperature according to 'tnight' parameter
    WebGetArg (D_CMND_PILOTWIRE_NIGHT, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) PilotwireSetTemperature (PILOTWIRE_MODE_NIGHT, atof (str_argument), false);

    // get minimum temperature according to 'tnobody' parameter
    WebGetArg (D_CMND_PILOTWIRE_ECO, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) PilotwireSetTemperature (PILOTWIRE_MODE_ECO, atof (str_argument), false);

    // get minimum temperature according to 'tvacancy' parameter
    WebGetArg (D_CMND_PILOTWIRE_NOFROST, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) PilotwireSetTemperature (PILOTWIRE_MODE_NOFROST, atof (str_argument), false);

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
  if (pilotwire_config.device_type == PILOTWIRE_DEVICE_NORMAL) strcpy_P (str_argument, PSTR (D_PILOTWIRE_CHECKED)); else strcpy (str_argument, "");
  WSContentSend_P (PSTR ("<input type='radio' id='normal' name='%s' value=%d %s>%s<br>\n"), D_CMND_PILOTWIRE_TYPE, PILOTWIRE_DEVICE_NORMAL, str_argument, D_PILOTWIRE);
  if (pilotwire_config.device_type == PILOTWIRE_DEVICE_DIRECT) strcpy_P (str_argument, PSTR (D_PILOTWIRE_CHECKED)); else strcpy (str_argument, "");
  WSContentSend_P (PSTR ("<input type='radio' id='direct' name='%s' value=%d %s>%s<br>\n"), D_CMND_PILOTWIRE_TYPE, PILOTWIRE_DEVICE_DIRECT, str_argument, D_PILOTWIRE_DIRECT);

  WSContentSend_P (D_CONF_FIELDSET_STOP);

  //   Options
  //    - Window open
  //    - Mvt detect
  // ------------------

  WSContentSend_P (D_CONF_FIELDSET_START, "♻️", D_PILOTWIRE_OPTION);

  if (pilotwire_config.window) strcpy (str_value, D_PILOTWIRE_CHECKED); else strcpy (str_value, "");
  WSContentSend_P (PSTR ("<p><input type='checkbox' id='%s' name='%s' %s>\n"), D_CMND_PILOTWIRE_WINDOW, D_CMND_PILOTWIRE_WINDOW, str_value);
  WSContentSend_P (PSTR ("<label for='%s'>%s</label></p>\n"), D_CMND_PILOTWIRE_WINDOW, D_PILOTWIRE_WINDOW);

  if (pilotwire_config.mvt_detect) strcpy (str_value, D_PILOTWIRE_CHECKED); else strcpy (str_value, "");
  WSContentSend_P (PSTR ("<p><input type='checkbox' id='%s' name='%s' %s>\n"), D_CMND_PILOTWIRE_MVT, D_CMND_PILOTWIRE_MVT, str_value);
  WSContentSend_P (PSTR ("<label for='%s'>%s</label></p>\n"), D_CMND_PILOTWIRE_MVT, D_PILOTWIRE_MVT);

  WSContentSend_P (D_CONF_FIELDSET_STOP);

  //   Temperatures 
  // ----------------

  strlcpy (str_argument, D_PILOTWIRE_TEMPERATURE, sizeof (str_argument));
  strlcat (str_argument, " (°C)", sizeof (str_argument));
  WSContentSend_P (D_CONF_FIELDSET_START, "🌡️", str_argument);

  // minimum temperature
  GetTextIndexed (str_argument, sizeof (str_argument), PILOTWIRE_MODE_LOW, kPilotwireMode);
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &pilotwire_config.arr_target[PILOTWIRE_MODE_LOW]);
  WSContentSend_P (D_CONF_FIELD_FULL, str_argument, D_CMND_PILOTWIRE_LOW, D_CMND_PILOTWIRE_LOW, PILOTWIRE_TEMP_LIMIT_MIN, PILOTWIRE_TEMP_LIMIT_MAX, str_step, str_value);

  // maximum temperature
  GetTextIndexed (str_argument, sizeof (str_argument), PILOTWIRE_MODE_HIGH, kPilotwireMode);
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &pilotwire_config.arr_target[PILOTWIRE_MODE_HIGH]);
  WSContentSend_P (D_CONF_FIELD_FULL, str_argument, D_CMND_PILOTWIRE_HIGH, D_CMND_PILOTWIRE_HIGH, PILOTWIRE_TEMP_LIMIT_MIN, PILOTWIRE_TEMP_LIMIT_MAX, str_step, str_value);

  // normal temperature
  GetTextIndexed (str_argument, sizeof (str_argument), PILOTWIRE_MODE_COMFORT, kPilotwireMode);
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &pilotwire_config.arr_target[PILOTWIRE_MODE_COMFORT]);
  WSContentSend_P (D_CONF_FIELD_FULL, str_argument, D_CMND_PILOTWIRE_COMFORT, D_CMND_PILOTWIRE_COMFORT, PILOTWIRE_TEMP_LIMIT_MIN, PILOTWIRE_TEMP_LIMIT_MAX, str_step, str_value);

  // night temperature
  GetTextIndexed (str_argument, sizeof (str_argument), PILOTWIRE_MODE_NIGHT, kPilotwireMode);
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &pilotwire_config.arr_target[PILOTWIRE_MODE_NIGHT]);
  WSContentSend_P (D_CONF_FIELD_FULL, str_argument, D_CMND_PILOTWIRE_NIGHT, D_CMND_PILOTWIRE_NIGHT, - PILOTWIRE_TEMP_LIMIT_MAX, PILOTWIRE_TEMP_LIMIT_MAX, str_step, str_value);

  // nobody temperature
  GetTextIndexed (str_argument, sizeof (str_argument), PILOTWIRE_MODE_ECO, kPilotwireMode);
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &pilotwire_config.arr_target[PILOTWIRE_MODE_ECO]);
  WSContentSend_P (D_CONF_FIELD_FULL, str_argument, D_CMND_PILOTWIRE_ECO, D_CMND_PILOTWIRE_ECO, - PILOTWIRE_TEMP_LIMIT_MAX, PILOTWIRE_TEMP_LIMIT_MAX, str_step, str_value);

  // vacancy temperature
  GetTextIndexed (str_argument, sizeof (str_argument), PILOTWIRE_MODE_NOFROST, kPilotwireMode);
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &pilotwire_config.arr_target[PILOTWIRE_MODE_NOFROST]);
  WSContentSend_P (D_CONF_FIELD_FULL, str_argument, D_CMND_PILOTWIRE_NOFROST, D_CMND_PILOTWIRE_NOFROST, - PILOTWIRE_TEMP_LIMIT_MAX, PILOTWIRE_TEMP_LIMIT_MAX, str_step, str_value);

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
      if (pilotwire_graph.arr_state[0][array_index] == PILOTWIRE_STATUS_ON) WSContentSend_P (PSTR ("%d,%d "), graph_x, graph_high);
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
  if (!isnan (pilotwire_temperature.current)) ext_snprintf_P (str_text, sizeof (str_text), PSTR ("%1_f"), &pilotwire_temperature.current);
    else strcpy_P (str_text, PSTR ("---"));
  strlcpy (str_update, str_text, sizeof (str_update));

  // update current temperature class
  strlcat (str_update, ";", sizeof (str_update));
  PilotwireWebGetTemperatureClass (pilotwire_temperature.current, str_text, sizeof (str_text));
  strlcat (str_update, str_text, sizeof (str_update));

  // display status icon (window, flame, ...)
  strlcat (str_update, ";", sizeof (str_update));
  PilotwireWebIconStatus (str_text);
  strlcat (str_update, str_text, sizeof (str_update));

  // display target thermometer icon
  strlcat (str_update, ";/", sizeof (str_update));
  temperature = PilotwireGetTargetTemperature ();
  PilotwireWebGetTemperatureClass (temperature, str_text, sizeof (str_text));
  strlcat (str_update, "therm-", sizeof (str_update));
  strlcat (str_update, str_text, sizeof (str_update));
  strlcat (str_update, ".png", sizeof (str_update));

  // send result
  Webserver->send (200, "text/plain", str_update, strlen (str_update));
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
    if (strlen (str_value) > 0) PilotwireSetStatusMode (atoi (str_value), true);
  }

  // if target temperature has been changed
  if (Webserver->hasArg (D_CMND_PILOTWIRE_TARGET))
  {
    WebGetArg (D_CMND_PILOTWIRE_TARGET, str_value, sizeof (str_value));
    if (strlen (str_value) > 0) PilotwireSetCurrentTarget (atof (str_value));
  }

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
  WSContentSend_P (PSTR ("div {color:white;font-size:1.5rem;text-align:center;vertical-align:middle;margin:20px auto;}\n"));
  WSContentSend_P (PSTR ("div a {color:white;text-decoration:none;}\n"));

  WSContentSend_P (PSTR ("div.inline {display:inline-block;}\n"));
  WSContentSend_P (PSTR ("div.title {font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.section {margin:20px auto;}\n"));

  WSContentSend_P (PSTR ("div.temp {font-size:3rem;font-weight:bold;}\n"));

  WSContentSend_P (PSTR (".low {color:%s;}\n"), PILOTWIRE_COLOR_LOW);
  WSContentSend_P (PSTR (".mid {color:%s;}\n"), PILOTWIRE_COLOR_MEDIUM);
  WSContentSend_P (PSTR (".high {color:%s;}\n"), PILOTWIRE_COLOR_HIGH);

  WSContentSend_P (PSTR ("div.pix {height:128px;}\n"));
  WSContentSend_P (PSTR ("div.pix img {height:128px;border-radius:12px;}\n"));
  WSContentSend_P (PSTR ("div.status {font-size:2.5rem;}\n"));

  WSContentSend_P (PSTR ("div.therm {font-size:2.5rem;}\n"));
  WSContentSend_P (PSTR ("select {background:none;color:#ddd;font-size:2rem;border:none;}\n"));

  WSContentSend_P (PSTR ("div.item {padding:0.2rem;margin:0px;border:none;}\n"));
  WSContentSend_P (PSTR ("a div.item:hover {background:#aaa;}\n"));

  WSContentSend_P (PSTR ("div.adjust {width:240px;font-size:2rem;}\n"));
  WSContentSend_P (PSTR ("div.adjust div.item {width:30px;color:#ddd;border:1px #444 solid;border-radius:8px;}\n"));

  WSContentSend_P (PSTR ("div.mode {padding:2px;border:1px #666 solid;border-radius:8px;}\n"));
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
  if (!isnan (pilotwire_temperature.current)) ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &pilotwire_temperature.current);
  PilotwireWebGetTemperatureClass (pilotwire_temperature.current, str_class, sizeof (str_class));
  WSContentSend_P (PSTR ("<div id='actual' class='%s temp'><span id='temp'>%s</span><small> °C</small></div>\n"), str_class, str_value);

  // room picture
  WSContentSend_P (PSTR ("<div class='section pix'>"));
  if (is_logged) WSContentSend_P (PSTR ("<a href='/'>"));

#ifdef USE_UFILESYS
  sprintf_P (str_value, PSTR("/%s"), PILOTWIRE_ICON_LOGO);
  if (ffsp->exists (str_value)) WSContentSend_P (PSTR ("<img src='%s'>"), str_value);
  else
#endif    // USE_UFILESYS
  WSContentSend_P (PSTR ("<img src='/%s'>"), PILOTWIRE_ICON_DEFAULT);

  if (is_logged) WSContentSend_P (PSTR ("</a>"));
  WSContentSend_P (PSTR ("</div>\n"));

  // status icon
  PilotwireWebIconStatus (str_value);
  WSContentSend_P (PSTR ("<div class='section status' id='status'>%s</div>\n"), str_value);

  // section : target 
  WSContentSend_P (PSTR ("<div class='section target'>\n"));

  // thermometer
  WSContentSend_P (PSTR ("<div class='inline therm'>🌡</div>\n"));

  // get current target temperature
  temperature = PilotwireGetTargetTemperature ();

  // if heater in normal thermostat mode, display selection
  if (pilotwire_status.device_mode == PILOTWIRE_STATUS_COMFORT)
  {
    WSContentSend_P (PSTR ("<div class='inline adjust'>\n"));

    // button -0.5
    value = temperature - 0.5;
    ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &value);
    WSContentSend_P (PSTR ("<a href='/%s?%s=%s'><div class='inline item'>%s</div></a>\n"), D_PAGE_PILOTWIRE_CONTROL, D_CMND_PILOTWIRE_TARGET, str_value, "-");

    // target selection
    WSContentSend_P (PSTR ("<select name='%s' id='%s' onchange='this.form.submit();'>\n"), D_CMND_PILOTWIRE_TARGET, D_CMND_PILOTWIRE_TARGET);
    for (value = pilotwire_config.arr_target[PILOTWIRE_MODE_LOW]; value <= pilotwire_config.arr_target[PILOTWIRE_MODE_HIGH]; value += 0.5)
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
  else if (pilotwire_status.device_mode > PILOTWIRE_STATUS_ON)
  {
    ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &temperature);
    WSContentSend_P (PSTR ("<div class='inline adjust %s'>%s<small> °C</small></div>\n"), str_class, str_value);
  }

  WSContentSend_P (PSTR ("</div>\n"));    // section target

  // section : mode 
  WSContentSend_P (PSTR ("<div class='section'>\n"));
  WSContentSend_P (PSTR ("<div class='inline mode'>\n"));

  if (pilotwire_status.device_mode == PILOTWIRE_STATUS_NOFROST) WSContentSend_P (PSTR ("<div class='inline item' title='%s'>❄</div>\n"), D_PILOTWIRE_NOFROST);
    else WSContentSend_P (PSTR ("<a href='/%s?%s=%u'><div class='inline item' title='%s'>❄</div></a>\n"), D_PAGE_PILOTWIRE_CONTROL, D_CMND_PILOTWIRE_MODE, PILOTWIRE_STATUS_NOFROST, D_PILOTWIRE_NOFROST);
  if (pilotwire_status.device_mode == PILOTWIRE_STATUS_ECO) WSContentSend_P (PSTR ("<div class='inline item' title='%s'>🌙</div>\n"), D_PILOTWIRE_ECO);
    else WSContentSend_P (PSTR ("<a href='/%s?%s=%u'><div class='inline item' title='%s'>🌙</div></a>\n"), D_PAGE_PILOTWIRE_CONTROL, D_CMND_PILOTWIRE_MODE, PILOTWIRE_STATUS_ECO, D_PILOTWIRE_ECO);
  if (pilotwire_status.device_mode == PILOTWIRE_STATUS_COMFORT) WSContentSend_P (PSTR ("<div class='inline item' title='%s'>🔆</div>\n"), D_PILOTWIRE_COMFORT);
    else WSContentSend_P (PSTR ("<a href='/%s?%s=%u'><div class='inline item' title='%s'>🔆</div></a>\n"), D_PAGE_PILOTWIRE_CONTROL, D_CMND_PILOTWIRE_MODE, PILOTWIRE_STATUS_COMFORT, D_PILOTWIRE_COMFORT);

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
  pilotwire_graph.temp_min = pilotwire_config.arr_target[PILOTWIRE_MODE_LOW];
  pilotwire_graph.temp_max = pilotwire_config.arr_target[PILOTWIRE_MODE_HIGH];

  // adjust to current temperature
  if (pilotwire_temperature.current < pilotwire_config.arr_target[PILOTWIRE_MODE_LOW]) pilotwire_graph.temp_min = floor (pilotwire_temperature.current);
  if (pilotwire_temperature.current > pilotwire_config.arr_target[PILOTWIRE_MODE_HIGH]) pilotwire_graph.temp_max = ceil (pilotwire_temperature.current);

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

bool Xsns97 (uint8_t function)
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
    case FUNC_EVERY_250_MSECOND:
      PilotwireEvery250ms ();
      break;
    case FUNC_EVERY_SECOND:
      if (TasmotaGlobal.uptime > 4) PilotwireEverySecond ();
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
      Webserver->on ("/" PILOTWIRE_ICON_DEFAULT,    PilotwireWebIconDefault);
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
