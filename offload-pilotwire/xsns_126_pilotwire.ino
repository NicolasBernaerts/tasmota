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
    09/02/2023 - v10.2 - Disable wifi sleep to avoid latency
                         Redesign of graph generation
                         Handle external icon URL
    12/05/2023 - v10.3 - Save history in Settings strings

  Settings are stored using sbflag1.sparexx and free_ea6 parameters.

    - Settings->sbflag1.spare28 = Command type (pilotewire or direct)
    - Settings->sbflag1.spare29 = Open Window detection flag
    - Settings->sbflag1.spare30 = Presence detection flag
    - Settings->sbflag1.spare31 = Ecowatt management flag

    - Settings->free_ea6[0] = Presence detection initial delay (0 ... 64h in 15mn steps)
    - Settings->free_ea6[1] = Temperature drop per hour (0 ... 2.55°C in 0.01°C steps)

  Following temperature are stored in 3 ways :
    * absolute : 100 ... 200 = 0 to 50°C in 0.5°C steps
    * relative : 99 ... 0    = -0.5 to -24.5°C in 0.5°C steps

    - Settings->free_ea6[2] = Lowest acceptable temperature (absolute)
    - Settings->free_ea6[3] = Highest acceptable temperature (absolute)
    - Settings->free_ea6[4] = Comfort target temperature (absolute)
    - Settings->free_ea6[5] = Night mode target temperature (relative)
    - Settings->free_ea6[6] = Eco mode target temperature (relative)
    - Settings->free_ea6[7] = NoFrost mode target temperature (relative)
    - Settings->free_ea6[8] = Ecowatt level 2 target temperature (relative)
    - Settings->free_ea6[9] = Ecowatt level 3 target temperature (relative)

  If LittleFS partition is available room picture can be stored as /room.jpg

  Use pw_help command to list available commands
  
  WINDOW OPEN DETECTION
  ---------------------
  To enable Window Open Detection :
    - enable option it in the Pilotwire configuration page
    - if temperature drops of 3°C in less than 10mn, window is considered as opened, heater stops
    - if temperature increases of 0.2°C in less than 30mn, window is considered as closed again, heater restart
    - heater restart anyway after 30mn

  PRESENCE DETECTION
  ---------------------
  To enable Presence Detection :
    - enable option it in the Pilotwire configuration page
    - connect local presence detector and declare it as Counter 1 or Serial Tx/Rx
    - or declare topic for remote presence detector in Sensor configuration page
    - It should be detected at next restart
  When presence detection is enabled, behaviour is :
    - if there is no presence for a certain time, target temperature drop starts
    - target temperature drops is configured per °C per hour
    - target temperature drops down to ECO temperature
    - as soon as the is a movement, target temperature is back to before

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

#define XSNS_126 126

/*************************************************\
 *               Constants
\*************************************************/

#define D_PILOTWIRE_PAGE_CONFIG "config"
#define D_PILOTWIRE_PAGE_STATUS "status"
#define D_PILOTWIRE_PAGE_CONTROL "control"
#define D_PILOTWIRE_PAGE_UPDATE "control.upd"
#define D_PILOTWIRE_PAGE_GRAPH "graph"
#define D_PILOTWIRE_PAGE_BASE_SVG "graph-base.svg"
#define D_PILOTWIRE_PAGE_DATA_SVG "graph-data.svg"

#define D_CMND_PILOTWIRE_HELP "help"
#define D_CMND_PILOTWIRE_PERIOD "period"
#define D_CMND_PILOTWIRE_HISTO "histo"
#define D_CMND_PILOTWIRE_TYPE "type"
#define D_CMND_PILOTWIRE_MODE "mode"
#define D_CMND_PILOTWIRE_TARGET "target"
#define D_CMND_PILOTWIRE_WINDOW "window"
#define D_CMND_PILOTWIRE_PRESENCE "pres"
#define D_CMND_PILOTWIRE_PRES_DELAY "pdelay"
#define D_CMND_PILOTWIRE_PRES_REDUCE "preduce"
#define D_CMND_PILOTWIRE_ICON_URL "icon"
#define D_CMND_PILOTWIRE_LOW "low"
#define D_CMND_PILOTWIRE_HIGH "high"
#define D_CMND_PILOTWIRE_COMFORT "comfort"
#define D_CMND_PILOTWIRE_NIGHT "night"
#define D_CMND_PILOTWIRE_ECO "eco"
#define D_CMND_PILOTWIRE_NOFROST "nofrost"
#define D_CMND_PILOTWIRE_ECOWATT "ecowatt"
#define D_CMND_PILOTWIRE_ECOWATT_N2 "ecowatt2"
#define D_CMND_PILOTWIRE_ECOWATT_N3 "ecowatt3"
#define D_CMND_PILOTWIRE_PREV "prev"
#define D_CMND_PILOTWIRE_NEXT "next"

#define D_JSON_PILOTWIRE "Pilotwire"
#define D_JSON_PILOTWIRE_MODE "Mode"
#define D_JSON_PILOTWIRE_STATUS "Status"
#define D_JSON_PILOTWIRE_HEATING "Heating"
#define D_JSON_PILOTWIRE_WINDOW "Window"
#define D_JSON_PILOTWIRE_PRESENCE "Presence"
#define D_JSON_PILOTWIRE_DELAY "Delay"
#define D_JSON_PILOTWIRE_TARGET "Target"
#define D_JSON_PILOTWIRE_TEMPERATURE "Temperature"

#define D_PILOTWIRE "Pilotwire"
#define D_PILOTWIRE_MODE "Mode"
#define D_PILOTWIRE_PRESENCE "Presence"
#define D_PILOTWIRE_WINDOW "Window"
#define D_PILOTWIRE_TARGET "Target"
#define D_PILOTWIRE_CONFIGURE "Configure"
#define D_PILOTWIRE_CONNEXION "Connexion"
#define D_PILOTWIRE_OPTION "Options"
#define D_PILOTWIRE_GRAPH "Graph"
#define D_PILOTWIRE_STATUS "Status"
#define D_PILOTWIRE_OFF "Off"
#define D_PILOTWIRE_COMFORT "Comfort"
#define D_PILOTWIRE_NIGHT "Night"
#define D_PILOTWIRE_ECO "Economy"
#define D_PILOTWIRE_NOFROST "No frost"
#define D_PILOTWIRE_CONTROL "Control"
#define D_PILOTWIRE_HEATER "Heater"
#define D_PILOTWIRE_TEMPERATURE "Temp."
#define D_PILOTWIRE_DIRECT "Direct"
#define D_PILOTWIRE_CHECKED "checked"
#define D_PILOTWIRE_DETECTION "Detection"
#define D_PILOTWIRE_SIGNAL "Signal"
#define D_PILOTWIRE_ECOWATT "Ecowatt"

// color codes
#define PILOTWIRE_COLOR_BACKGROUND      "#252525" // page background
#define PILOTWIRE_COLOR_LOW             "#85c1e9"        // low temperature target
#define PILOTWIRE_COLOR_MEDIUM          "#ff9933"     // medium temperature target
#define PILOTWIRE_COLOR_HIGH            "#cc3300"       // high temperature target

/*
#ifdef USE_UFILESYS
#define D_PILOTWIRE_FILE_CFG "/pilotwire.cfg"                            // configuration file
const char D_PILOTWIRE_FILE_WEEK[] PROGMEM = "/pilotwire-week-%02u.csv"; // history files label and filename
#endif                                                                   // USE_UFILESYS
*/

// constant : temperature
#define PILOTWIRE_TEMP_THRESHOLD            0.25    // temperature threshold to switch on/off (°C)
#define PILOTWIRE_TEMP_STEP                 0.5          // temperature selection step (°C)
#define PILOTWIRE_TEMP_UPDATE               5          // temperature update delay (s)
#define PILOTWIRE_TEMP_LIMIT_MIN            6       // minimum acceptable temperature
#define PILOTWIRE_TEMP_LIMIT_MAX            30      // maximum acceptable temperature
#define PILOTWIRE_TEMP_DEFAULT_LOW          12    // low range selectable temperature
#define PILOTWIRE_TEMP_DEFAULT_HIGH         22   // high range selectable temperature
#define PILOTWIRE_TEMP_DEFAULT_NORMAL       18 // default temperature
#define PILOTWIRE_TEMP_DEFAULT_NIGHT        -1  // default night mode adjustment
#define PILOTWIRE_TEMP_DEFAULT_ECO          -3    // default eco mode adjustment
#define PILOTWIRE_TEMP_DEFAULT_NOFROST      7 // default no frost mode adjustment
#define PILOTWIRE_TEMP_SCALE_LOW            19      // temperature under 19 is energy friendly
#define PILOTWIRE_TEMP_SCALE_HIGH           21     // temperature above 21 is energy wastage

// constant : open window detection
#define PILOTWIRE_WINDOW_SAMPLE_NBR         20      // number of temperature samples to detect opened window (10mn for 1 sample every 30s)
#define PILOTWIRE_WINDOW_OPEN_PERIOD        30     // delay between 2 temperature samples in open window detection (30s)
#define PILOTWIRE_WINDOW_OPEN_DROP          3        // temperature drop for window open detection (°C)
#define PILOTWIRE_WINDOW_CLOSE_INCREASE     0.2 // temperature increase to detect window closed (°C)
#define PILOTWIRE_WINDOW_TIMEOUT            1800       // timeout in sec after window detected opened (30mn)

// constant : presence detection
#define PILOTWIRE_PRESENCE_IGNORE           30       // duration to ignore presence detection, to avoid relay change perturbation (sec)
#define PILOTWIRE_PRESENCE_TIMEOUT          5       // timeout to consider current presence is detected (sec)
#define PILOTWIRE_PRESENCE_HOUR_DELAY       6    // initial timeout for non presence detection (hour)
#define PILOTWIRE_PRESENCE_HOUR_REDUCE      0.1 // non presence temperature decrease (per hour)

// constant chains
const char PILOTWIRE_FIELDSET_START[] PROGMEM = "<fieldset><legend><b>&nbsp;%s %s&nbsp;</b></legend>\n";
const char PILOTWIRE_FIELDSET_STOP[]  PROGMEM = "</fieldset><br />\n";
const char PILOTWIRE_TEXT[]           PROGMEM = "<text x='%d%%' y='%d%%'>%s</text>\n";
const char PILOTWIRE_FIELD_FULL[]     PROGMEM = "<p>%s %s<span class='key'>%s</span><br><input type='number' name='%s' min='%d' max='%d' step='%s' value='%s'></p>\n";

// device control
enum PilotwireDevices
{
  PILOTWIRE_DEVICE_PILOTWIRE,
  PILOTWIRE_DEVICE_DIRECT,
  PILOTWIRE_DEVICE_MAX
};
const char kPilotwireDevice[] PROGMEM = "Pilotwire|Direct"; // device type labels

enum PilotwireMode
{
  PILOTWIRE_MODE_OFF,
  PILOTWIRE_MODE_ECO,
  PILOTWIRE_MODE_NIGHT,
  PILOTWIRE_MODE_COMFORT,
  PILOTWIRE_MODE_FORCED,
  PILOTWIRE_MODE_MAX
};
const char kPilotwireModeIcon[] PROGMEM = "❌|🌙|💤|🔆|🔥";                        // running mode icons
const char kPilotwireModeLabel[] PROGMEM = "Off|Economie|Nuit|Confort|Forcé"; // running mode labels

enum PilotwireTarget
{
  PILOTWIRE_TARGET_LOW,
  PILOTWIRE_TARGET_HIGH,
  PILOTWIRE_TARGET_FORCED,
  PILOTWIRE_TARGET_NOFROST,
  PILOTWIRE_TARGET_COMFORT,
  PILOTWIRE_TARGET_ECO,
  PILOTWIRE_TARGET_NIGHT,
  PILOTWIRE_TARGET_ECOWATT2,
  PILOTWIRE_TARGET_ECOWATT3,
  PILOTWIRE_TARGET_MAX
};
const char kPilotwireTargetIcon[] PROGMEM = "➖|➕|🔥|❄|🔆|🌙|💤|🟠|🔴";                                                                          // target temperature icons
const char kPilotwireTargetLabel[] PROGMEM = "Minimale|Maximale|Forcée|Hors gel|Confort|Economie|Nuit|Ecowatt niveau 2|Ecowatt niveau 3"; // target temperature labels

// week days name for graph
const char kPilotwireWeekDay[] PROGMEM = "Mon|Tue|Wed|Thu|Fri|Sat|Sun"; // week days labels

enum PilotwirePresence
{
  PILOTWIRE_PRESENCE_NORMAL,
  PILOTWIRE_PRESENCE_INITIAL,
  PILOTWIRE_PRESENCE_DISABLED,
  PILOTWIRE_PRESENCE_MAX
};

// list of events associated with heating stage
enum PilotwireEvent
{
  PILOTWIRE_EVENT_NONE,
  PILOTWIRE_EVENT_HEATING,
  PILOTWIRE_EVENT_OFFLOAD,
  PILOTWIRE_EVENT_WINDOW,
  PILOTWIRE_EVENT_ECOWATT2,
  PILOTWIRE_EVENT_ECOWATT3,
  PILOTWIRE_EVENT_MAX
};
const char kPilotwireEventIcon[] PROGMEM = " |🔥|⚡|🪟|🟠|🔴"; // icon associated with event

// pilotwire commands
const char kPilotwireCommand[]          PROGMEM = "pw_" "|" D_CMND_PILOTWIRE_HELP "|" D_CMND_PILOTWIRE_TYPE "|" D_CMND_PILOTWIRE_MODE "|" D_CMND_PILOTWIRE_TARGET "|" D_CMND_PILOTWIRE_ECOWATT "|" D_CMND_PILOTWIRE_WINDOW "|" D_CMND_PILOTWIRE_PRESENCE "|" D_CMND_PILOTWIRE_LOW "|" D_CMND_PILOTWIRE_HIGH "|" D_CMND_PILOTWIRE_NOFROST "|" D_CMND_PILOTWIRE_COMFORT "|" D_CMND_PILOTWIRE_ECO "|" D_CMND_PILOTWIRE_NIGHT "|" D_CMND_PILOTWIRE_ECOWATT_N2 "|" D_CMND_PILOTWIRE_ECOWATT_N3 "|" D_CMND_PILOTWIRE_PRES_DELAY "|" D_CMND_PILOTWIRE_PRES_REDUCE "|" D_CMND_PILOTWIRE_ICON_URL;
void (*const PilotwireCommand[])(void)  PROGMEM = {&CmndPilotwireHelp, &CmndPilotwireType, &CmndPilotwireMode, &CmndPilotwireTarget, &CmndPilotwireEcowatt, &CmndPilotwireWindow, &CmndPilotwirePresence, &CmndPilotwireTargetLow, &CmndPilotwireTargetHigh, &CmndPilotwireTargetNofrost, &CmndPilotwireTargetComfort, &CmndPilotwireTargetEco, &CmndPilotwireTargetNight, &CmndPilotwireTargetEcowatt2, &CmndPilotwireTargetEcowatt3, &CmndPilotwirePresenceDelay, &CmndPilotwirePresenceReduce, &CmndPilotwireIconURL};

/****************************************\
 *               Icons
 *
 *      xxd -i -c 256 icon.png
\****************************************/

// icon : none
#define PILOTWIRE_ICON_LOGO     "/room.jpg"
#define PILOTWIRE_ICON_DEFAULT  "/default.png"
char pilotwire_default_png[] PROGMEM = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xcf, 0x08, 0x03, 0x00, 0x00, 0x00, 0x2d, 0x65, 0xff, 0x81, 0x00, 0x00, 0x00, 0x33, 0x50, 0x4c, 0x54, 0x45, 0xda, 0xae, 0xb6, 0x6e, 0x47, 0x3b, 0x56, 0x51, 0x4e, 0x93, 0x59, 0x44, 0x79, 0x71, 0x6c, 0xa6, 0x7a, 0x49, 0xad, 0x81, 0x56, 0xac, 0x81, 0x5c, 0x91, 0x93, 0x90, 0xb6, 0x91, 0x6a, 0xc9, 0xa4, 0x81, 0xa9, 0xab, 0xa8, 0xd3, 0xaf, 0x8b, 0xd6, 0xb9, 0x98, 0xbe, 0xc1, 0xbd, 0xea, 0xcc, 0xab, 0xf5, 0xd8, 0xbc, 0xa3, 0xe7, 0x47, 0x29, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x00, 0x01, 0x62, 0x4b, 0x47, 0x44, 0x00, 0x88, 0x05, 0x1d, 0x48, 0x00, 0x00, 0x09, 0x69, 0x49, 0x44, 0x41, 0x54, 0x78, 0xda, 0xed, 0xd9, 0xd1, 0x72, 0xc3, 0xa8, 0x0e, 0x00, 0xd0, 0xa4, 0x89, 0xa1, 0xa2, 0xa6, 0xf8, 0xff, 0xbf, 0xf6, 0x22, 0x90, 0x40, 0xd8, 0x32, 0x89, 0xdb, 0xdb, 0xd9, 0x99, 0x5d, 0xf1, 0xd0, 0xb4, 0x91, 0x41, 0xe2, 0x80, 0x9d, 0xd8, 0xbd, 0xdd, 0xac, 0x59, 0xb3, 0x66, 0xcd, 0x9a, 0x35, 0x6b, 0xd6, 0x64, 0x83, 0x65, 0x71, 0xf3, 0xb0, 0x9f, 0x84, 0xfd, 0xb2, 0xc0, 0x24, 0xec, 0xe6, 0xe1, 0x97, 0xa9, 0xdd, 0xcf, 0xc7, 0x7e, 0xbb, 0x2d, 0xcb, 0x2c, 0x0f, 0x60, 0x18, 0xe6, 0xe1, 0x29, 0xcf, 0x2c, 0xec, 0xa6, 0xa9, 0x6f, 0x6f, 0xa4, 0x86, 0xff, 0xc7, 0x06, 0x70, 0x30, 0x29, 0xd2, 0x63, 0xd8, 0x4d, 0xa6, 0x00, 0x6e, 0x52, 0x45, 0xae, 0x70, 0x52, 0x24, 0xbc, 0x0a, 0xbf, 0x4e, 0xed, 0x7e, 0x3d, 0xff, 0xbc, 0x08, 0x6e, 0x3d, 0xa5, 0x04, 0xdc, 0x66,
    0x21, 0x1f, 0x72, 0x1e, 0x0e, 0x70, 0x1e, 0xce, 0xe3, 0x46, 0x77, 0xba, 0xc8, 0xb8, 0x3d, 0xe2, 0x72, 0x7a, 0x86, 0xbd, 0xae, 0x6c, 0x92, 0xfa, 0xc2, 0xf9, 0x4f, 0x69, 0xd4, 0x2a, 0xeb, 0x2e, 0x0b, 0xcb, 0x49, 0x95, 0x65, 0x83, 0x87, 0xd3, 0xad, 0x88, 0x1b, 0xbc, 0x00, 0xe8, 0x61, 0x7c, 0x7f, 0x99, 0xa7, 0x3e, 0xaf, 0xec, 0x45, 0xea, 0x2b, 0xd7, 0xbf, 0x08, 0x39, 0x4d, 0x08, 0x5a, 0x1e, 0xbc, 0xfe, 0x45, 0x07, 0x21, 0x1f, 0xa3, 0x85, 0x71, 0x77, 0xe4, 0xae, 0xe0, 0xa2, 0x7a, 0x39, 0xc2, 0x25, 0x0a, 0x19, 0x00, 0xd4, 0x30, 0xd6, 0x9e, 0xc3, 0x35, 0xf5, 0xa2, 0x57, 0xe6, 0x72, 0x65, 0xeb, 0x69, 0xea, 0x88, 0xa9, 0xd7, 0xdf, 0x5c, 0x09, 0x01, 0xc7, 0x4e, 0x1b, 0x02, 0xc4, 0x2d, 0x1e, 0x76, 0x53, 0xd9, 0xfe, 0x69, 0x43, 0x80, 0xb4, 0x05, 0x35, 0x1c, 0xb6, 0x84, 0x55, 0x6c, 0x09, 0xd4, 0x30, 0x8e, 0x9a, 0x01, 0xb6, 0x94, 0x7f, 0xf7, 0x70, 0x9c, 0xe0, 0x56, 0x00, 0xb6, 0x0c, 0xa4, 0x74, 0xf6, 0x69, 0xf3, 0xa5, 0xb2, 0x69, 0xea, 0x94, 0x7e, 0x74, 0x1a, 0x80, 0x2b, 0x1b, 0xb3, 0xcc, 0x9f, 0x00, 0xb6, 0xb0, 0xd0, 0x5b, 0x0e, 0x3c, 0x87, 0x61, 0xcb, 0xc5, 0x17, 0x80, 0x0d, 0x38, 0xea, 0x72, 0x67, 0x0a, 0x87, 0x8d, 0x00, 0xb6, 0xe4, 0x39, 0xec, 0xdb, 0xd8, 0x38, 0x68, 0x05, 0x28, 0x02, 0x34, 0x76, 0xeb, 0xed, 0x4a, 0x4e, 0x04, 0x40, 0x7c, 0x8a, 0xc3, 0x50, 0x99, 0x1f, 0x2a, 0x1b, 0x52, 0x03, 0xa5, 0x4e, 0x43, 0xea, 0x2b, 0x1f, 0x7c, 0xad, 0x46, 0x06, 0xe0, 0x29, 0x8a, 0x86, 0x45, 0x30, 0x00, 0xcf, 0x41, 0x34, 0x9f, 0x1a, 0xc0, 0x16, 0x8f, 0x61, 0xd8, 0x1a, 0x40, 0x9f, 0x62, 0x6f, 0x79, 0xe2, 0x0c, 0xd0, 0xf0, 0x97, 0xb1,
    0x32, 0x3f, 0xaf, 0xac, 0x02, 0xc8, 0xca, 0x50, 0xe0, 0xf9, 0x81, 0x6d, 0x3a, 0xff, 0xfc, 0xc9, 0x16, 0x72, 0x8b, 0xb1, 0xd4, 0xd8, 0x00, 0x92, 0x73, 0x31, 0xe6, 0xf7, 0xa1, 0x84, 0xd7, 0x98, 0xab, 0xdb, 0x3a, 0x40, 0x9e, 0x03, 0xc4, 0x35, 0x07, 0x7c, 0xbe, 0x2a, 0x62, 0x18, 0x6a, 0x27, 0x02, 0xc8, 0x73, 0x08, 0xa5, 0xb7, 0x2b, 0xe1, 0x98, 0xcf, 0xdf, 0x24, 0x00, 0xf2, 0x1c, 0x56, 0x8c, 0x8a, 0xf0, 0x26, 0x00, 0xb6, 0x21, 0x75, 0xc8, 0xa9, 0xcb, 0xbb, 0x0c, 0x80, 0xa9, 0x6b, 0xd8, 0xef, 0x53, 0x63, 0x92, 0xb5, 0xa5, 0xc6, 0x6b, 0xc9, 0xf3, 0xf3, 0xfb, 0xfb, 0xf9, 0x9c, 0x10, 0x3c, 0xef, 0x1f, 0x1f, 0xa5, 0xa6, 0x9c, 0x3e, 0x4a, 0x00, 0xfe, 0x3b, 0xd5, 0xec, 0x5b, 0x9d, 0x42, 0x03, 0xc8, 0xe5, 0x94, 0x97, 0x48, 0x47, 0xd5, 0x89, 0x35, 0x00, 0xfa, 0x3b, 0x0f, 0x42, 0x47, 0x85, 0x4d, 0x02, 0x10, 0xf6, 0x06, 0xc3, 0x20, 0x0d, 0x60, 0x1c, 0x94, 0x8f, 0x62, 0x80, 0xcc, 0x37, 0xa6, 0xf6, 0x9b, 0x04, 0x48, 0x55, 0x33, 0x8f, 0xe6, 0x6e, 0x79, 0xea, 0x08, 0xf0, 0xf9, 0xfd, 0x79, 0x4a, 0xf0, 0x7c, 0x30, 0x40, 0x1d, 0xa7, 0x03, 0xd0, 0xcc, 0x13, 0xd7, 0x14, 0xb6, 0x01, 0x80, 0xd2, 0x8f, 0x2f, 0x0d, 0x80, 0xf9, 0x08, 0xa0, 0x97, 0x4c, 0x73, 0x22, 0x3e, 0x18, 0x5e, 0x1a, 0x00, 0x73, 0x0e, 0x2f, 0x1d, 0x20, 0x0e, 0x4c, 0x32, 0x75, 0x1a, 0x52, 0xbb, 0xe7, 0xfd, 0x41, 0x00, 0xdf, 0x48, 0x20, 0xdb, 0x11, 0x20, 0x96, 0x2d, 0x2e, 0x00, 0x36, 0x5a, 0xcb, 0x9a, 0x8d, 0x66, 0xd2, 0x01, 0x12, 0xaf, 0x65, 0x14, 0x53, 0xe8, 0x00, 0x51, 0x76, 0xe3, 0x29, 0x74, 0x80, 0xa1, 0x1b, 0x6f, 0xb2, 0x06, 0x30, 0x76, 0xe3, 0x7a, 0x1a, 0x00, 0xad,
    0x55, 0x1c, 0xf6, 0x43, 0x07, 0xa0, 0xd4, 0xf9, 0x04, 0x7a, 0x3e, 0x1e, 0x4f, 0x02, 0xf8, 0xfa, 0xfa, 0xfc, 0x78, 0xb6, 0x26, 0x04, 0xda, 0x29, 0x40, 0x33, 0x14, 0x00, 0x81, 0xcf, 0xad, 0x9a, 0x7d, 0x1b, 0x01, 0xa8, 0xf8, 0x48, 0x07, 0xd5, 0x79, 0x75, 0x80, 0xc4, 0xa7, 0x75, 0xea, 0x25, 0x09, 0x00, 0x1a, 0x95, 0xcf, 0xda, 0xb8, 0x03, 0xa0, 0x51, 0xe9, 0x82, 0x41, 0x09, 0xbd, 0xa8, 0x2c, 0xed, 0x0f, 0x12, 0x00, 0x5b, 0x4d, 0xfd, 0xbc, 0x63, 0x6b, 0x3b, 0xe0, 0xeb, 0xf9, 0xfc, 0x6a, 0x4d, 0x9c, 0x11, 0x6d, 0x07, 0x80, 0xdb, 0x03, 0x44, 0x01, 0x90, 0x5c, 0xd8, 0x03, 0x04, 0xba, 0x22, 0x45, 0xb1, 0x7d, 0x3a, 0x00, 0x8d, 0x57, 0x4b, 0x0d, 0x7d, 0x44, 0x2a, 0x97, 0x8a, 0xac, 0x07, 0x41, 0x1b, 0x91, 0x01, 0x6a, 0xba, 0x3a, 0x37, 0x3a, 0xc7, 0x25, 0x40, 0x1c, 0x52, 0x87, 0x3d, 0x40, 0x7d, 0xcd, 0xcb, 0xff, 0xe8, 0x3b, 0x20, 0x9f, 0x03, 0xcf, 0xcf, 0xde, 0x14, 0x00, 0xae, 0xac, 0x03, 0xd0, 0xd4, 0xcb, 0xcf, 0xc8, 0xef, 0x75, 0x80, 0x3a, 0x69, 0x59, 0xcb, 0x00, 0x50, 0x27, 0xcd, 0x00, 0x69, 0x0f, 0x50, 0x37, 0x6e, 0x2d, 0xd5, 0xb5, 0x2e, 0x1d, 0xc0, 0x0b, 0x80, 0xb0, 0x07, 0x90, 0x35, 0x0d, 0xa9, 0xe5, 0xd2, 0x1c, 0x01, 0xbe, 0x5b, 0xfb, 0x3a, 0x02, 0xb4, 0x34, 0x02, 0xa0, 0xe6, 0x3f, 0x07, 0xe8, 0x01, 0x9e, 0xa1, 0x00, 0x90, 0x01, 0xf1, 0x1e, 0x03, 0x84, 0x0e, 0x90, 0xfa, 0x7b, 0x0c, 0x20, 0x03, 0xbc, 0xc4, 0x27, 0x00, 0x43, 0xea, 0x24, 0x52, 0x8f, 0x00, 0x5f, 0x9f, 0xf9, 0x03, 0x61, 0xb6, 0x03, 0xd2, 0x72, 0x00, 0xa8, 0xe7, 0x7d, 0x0d, 0xac, 0x47, 0x00, 0x59, 0x05, 0x38, 0x05, 0x60, 0x6d, 0xe5, 0x79, 0x0d, 0xa0, 0xf5,
    0x6b, 0xa9, 0xcf, 0x00, 0xe2, 0x1e, 0x60, 0x13, 0x01, 0x15, 0x20, 0x5c, 0xdf, 0x01, 0x51, 0x01, 0x80, 0x0e, 0xd0, 0xd2, 0x48, 0x80, 0x5e, 0x85, 0xd7, 0x00, 0x7a, 0x3f, 0x9e, 0xb5, 0x00, 0x58, 0xff, 0x10, 0x20, 0x31, 0x80, 0xbc, 0x08, 0xe6, 0xcf, 0x41, 0x7d, 0x07, 0xdc, 0x19, 0x20, 0xce, 0x76, 0xc0, 0x0b, 0x00, 0xb8, 0x0c, 0x10, 0xe7, 0x00, 0xe1, 0x05, 0x80, 0xd7, 0x53, 0x4b, 0x00, 0xc8, 0xcb, 0x2f, 0x76, 0x40, 0xfe, 0x14, 0x78, 0xb1, 0x03, 0xfe, 0x6d, 0x00, 0xe1, 0xf0, 0x3d, 0x40, 0xfd, 0x26, 0xf4, 0x4f, 0x03, 0x6c, 0x7f, 0x09, 0x70, 0xeb, 0xa7, 0xc0, 0xd7, 0x53, 0x7c, 0x0f, 0xca, 0x4d, 0xde, 0x0c, 0x78, 0x17, 0x53, 0xd2, 0x01, 0x52, 0x4a, 0xeb, 0x02, 0xf9, 0xa7, 0x0e, 0x50, 0x02, 0x21, 0xff, 0xd4, 0x01, 0x30, 0xb0, 0xe4, 0xb1, 0x93, 0x0e, 0x90, 0x93, 0x02, 0xa6, 0x3e, 0x01, 0xc0, 0x9a, 0x20, 0x62, 0x01, 0x1a, 0x80, 0x9e, 0xba, 0x03, 0x60, 0x6a, 0xc7, 0x37, 0x43, 0x9f, 0x38, 0xfd, 0xe9, 0x73, 0x30, 0xbc, 0xb1, 0x56, 0x00, 0x16, 0xbe, 0x59, 0xcf, 0xaf, 0x0a, 0x40, 0x0f, 0x2f, 0x1a, 0x80, 0xeb, 0x61, 0x0d, 0x80, 0xc2, 0xf8, 0xa2, 0x00, 0xc8, 0xb0, 0x02, 0x70, 0x92, 0x9a, 0x7f, 0xad, 0x9d, 0x0b, 0xc0, 0x13, 0xaf, 0x78, 0x93, 0xe9, 0xd3, 0x13, 0x15, 0xba, 0xe5, 0xde, 0x03, 0x94, 0x1c, 0xf5, 0xe6, 0x5a, 0x03, 0xe8, 0x61, 0x0d, 0x40, 0x84, 0x35, 0x00, 0xaa, 0x9f, 0x1f, 0x06, 0xec, 0x01, 0x44, 0x6a, 0x0d, 0x40, 0x4f, 0x3d, 0x54, 0x56, 0x9f, 0x5b, 0xee, 0x36, 0xfc, 0xe4, 0x71, 0x98, 0xba, 0x03, 0x00, 0x98, 0x48, 0xdd, 0x01, 0x35, 0x0a, 0xfa, 0x0e, 0x58, 0xa0, 0x85, 0x55, 0x00, 0x0e, 0xeb, 0x00, 0xae, 0x87, 0x35, 0x00,
    0x4f, 0x61, 0xaf, 0x03, 0x00, 0x5c, 0x7f, 0x1e, 0xae, 0x02, 0xf4, 0xa7, 0x46, 0x1a, 0x00, 0xf4, 0x47, 0xf6, 0x1a, 0x40, 0x3f, 0xc9, 0x54, 0x80, 0x9e, 0x5a, 0x03, 0xf0, 0x3d, 0xb5, 0x06, 0xa0, 0xa7, 0x6e, 0x95, 0xfd, 0xe4, 0x1f, 0x02, 0x06, 0x60, 0x00, 0x87, 0xaf, 0xc2, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0x00, 0xff, 0x1c, 0x80, 0x37, 0x00, 0x03, 0x30, 0x80, 0xff, 0x06,
    0x00, 0x80, 0x5b, 0x74, 0x00, 0x07, 0x90, 0xa3, 0xde, 0xe9, 0x00, 0x14, 0x76, 0x4e, 0x07, 0xc8, 0x51, 0x0a, 0xab, 0x00, 0xd8, 0xb9, 0xa6, 0xd6, 0x00, 0x96, 0x9e, 0x5a, 0x03, 0xd0, 0x53, 0xf7, 0xca, 0x30, 0xf5, 0xfb, 0x8b, 0x5f, 0x9a, 0x06, 0x20, 0x9a, 0x06, 0xd0, 0x9b, 0x0a, 0xd0, 0x9b, 0x0a, 0xd0, 0x9b, 0x0a, 0xd0, 0x9b, 0x06, 0xa0, 0xa7, 0x1e, 0x2a, 0x73, 0xef, 0x2e, 0x7f, 0x3e, 0xd4, 0x03, 0xa8, 0x00, 0x75, 0x05, 0x72, 0xd8, 0x69, 0x00, 0xae, 0x2c, 0xdf, 0xe2, 0x71, 0x21, 0x14, 0x80, 0x1a, 0xc6, 0xb5, 0x70, 0x1a, 0x80, 0x6f, 0x61, 0x15, 0xa0, 0xa5, 0xf6, 0x1a, 0x80, 0x2b, 0x8b, 0x5f, 0x52, 0x68, 0x00, 0xae, 0x76, 0x7e, 0x6b, 0xfe, 0xf7, 0xfb, 0xdd, 0xa7, 0x52, 0xdb, 0x11, 0xc0, 0x95, 0x40, 0x28, 0x05, 0x29, 0x00, 0x38, 0x97, 0x7a, 0x30, 0x68, 0x00, 0xb9, 0x5f, 0x0a, 0x18, 0x4e, 0x2a, 0x00, 0x06, 0x4a, 0xbf, 0xa4, 0x01, 0xd4, 0x00, 0xc8, 0xca, 0x24, 0x00, 0x70, 0x86, 0x31, 0x75, 0xfb, 0xb5, 0x8c, 0xf6, 0x16, 0xc0, 0xf3, 0x71, 0xff, 0x80, 0xed, 0x0c, 0x40, 0x8c, 0x76, 0x02, 0x10, 0xa7, 0x00, 0xd4, 0xef, 0x0c, 0x60, 0x7b, 0x01, 0xe0, 0x86, 0xca, 0x24, 0x80, 0xd7, 0x53, 0xef, 0x00, 0x9e, 0x79, 0x75, 0xef, 0xff, 0x69, 0x80, 0xc7, 0xe3, 0xf1, 0x12, 0xe0, 0xfe, 0xaf, 0x05, 0xc0, 0xe5, 0x7f, 0x03, 0xa0, 0xed, 0x80, 0x70, 0x19, 0x20, 0xfc, 0x1d, 0x00, 0xbc, 0x00, 0xe8, 0x81, 0x56, 0x59, 0x07, 0xa0, 0x8b, 0x03, 0xce, 0xfe, 0x7d, 0x80, 0x34, 0x07, 0xe0, 0xf7, 0xae, 0x00, 0xf4, 0xf2, 0xbc, 0x3b, 0x00, 0x04, 0x09, 0x00, 0x17, 0x01, 0xd2, 0x2b, 0x80, 0xb5, 0xec, 0x80, 0xc7, 0xe3, 0x71, 0x01, 0xc0, 0x1d, 0x00, 0x6a, 0x1a,
    0x39, 0xcf, 0x01, 0x20, 0x8a, 0xc0, 0x50, 0xc5, 0x11, 0xa0, 0x57, 0x26, 0x00, 0x7a, 0xc0, 0x1d, 0x01, 0xbc, 0x04, 0x08, 0x07, 0x80, 0x21, 0x75, 0x3c, 0x02, 0xc4, 0xeb, 0x00, 0xad, 0x0a, 0xd8, 0xa5, 0xe1, 0x64, 0xe1, 0x08, 0x10, 0x77, 0x3f, 0x07, 0x80, 0x20, 0x00, 0xd8, 0x47, 0x00, 0xd4, 0x62, 0xeb, 0x4f, 0xef, 0xf6, 0x00, 0xa9, 0x9c, 0xe4, 0xb4, 0x02, 0xdc, 0xa5, 0x03, 0x48, 0xfb, 0x78, 0x04, 0x08, 0x3f, 0x02, 0xe0, 0xde, 0x20, 0xd2, 0xc4, 0xce, 0x40, 0x55, 0x08, 0x80, 0x30, 0x00, 0x84, 0x3d, 0x40, 0x1d, 0xaf, 0x4e, 0x3d, 0xf6, 0x11, 0x79, 0x36, 0x0e, 0xf6, 0x07, 0x0d, 0x00, 0x22, 0x69, 0x72, 0xee, 0x08, 0x10, 0x77, 0xf5, 0x0d, 0x00, 0x84, 0xfb, 0xe6, 0x35, 0x80, 0x3e, 0x05, 0xda, 0x56, 0xea, 0x00, 0xb5, 0x2e, 0x4a, 0x40, 0x83, 0x0b, 0x80, 0xfa, 0x15, 0x8c, 0xaa, 0x60, 0x9f, 0x06, 0x40, 0xef, 0x30, 0x40, 0xd8, 0x01, 0x44, 0x39, 0x6a, 0x6c, 0x09, 0xd7, 0x7e, 0x60, 0x1f, 0x83, 0xe7, 0xe5, 0x77, 0x95, 0x51, 0x37, 0x99, 0x3a, 0x09, 0xdc, 0x8b, 0x9f, 0x02, 0xad, 0xc8, 0x0e, 0x50, 0xe5, 0x09, 0x80, 0x7c, 0x3a, 0x00, 0x55, 0x47, 0x55, 0xb0, 0x4f, 0x03, 0x88, 0xdc, 0x2d, 0x89, 0x45, 0xec, 0x00, 0x41, 0x76, 0xe3, 0xab, 0x60, 0x07, 0xa0, 0xf7, 0x87, 0x14, 0x02, 0xa0, 0x9c, 0x20, 0xfc, 0x3e, 0xed, 0x9f, 0x0e, 0xd0, 0x52, 0xbf, 0xf7, 0x3d, 0x80, 0x01, 0x68, 0xbe, 0x1d, 0x80, 0xc6, 0x21, 0x00, 0xfe, 0xab, 0x01, 0x04, 0x3e, 0x4a, 0xbe, 0x74, 0x00, 0x58, 0x92, 0xa8, 0xae, 0x1f, 0xec, 0x40, 0x6c, 0x1f, 0x76, 0x03, 0x1e, 0x72, 0x6d, 0x59, 0x8e, 0x2f, 0x1d, 0x20, 0x72, 0x49, 0xbb, 0x3a, 0x61, 0xa8, 0x2c, 0x03, 0x7c, 0xbe, 0xf1,
    0x4d, 0x10, 0xcf, 0x01, 0x3f, 0x2e, 0x0a, 0xa5, 0xa1, 0x29, 0xf0, 0xce, 0xf0, 0xbc, 0x28, 0x81, 0x7e, 0x61, 0xee, 0xd8, 0xaf, 0x5a, 0x1d, 0xa0, 0x2f, 0x6a, 0x14, 0x7f, 0x36, 0x00, 0x2e, 0x7a, 0xbf, 0x66, 0xeb, 0xb8, 0xdd, 0xc6, 0x41, 0x1a, 0x00, 0x0c, 0xa9, 0x09, 0xb3, 0x01, 0xa4, 0xb6, 0xc9, 0xdc, 0xfb, 0x77, 0x83, 0xf9, 0x96, 0x2c, 0xe4, 0x0e, 0x12, 0x20, 0xdf, 0xcd, 0x05, 0xc0, 0x3b, 0xb9, 0x72, 0xeb, 0x0d, 0x01, 0xf8, 0xba, 0x14, 0xe8, 0xbc, 0x05, 0x0c, 0xe7, 0x9b, 0x2e, 0xbc, 0x61, 0x83, 0xe0, 0xf9, 0x8a, 0x54, 0x01, 0xc2, 0x12, 0x30, 0x8c, 0xb7, 0xeb, 0x38, 0x48, 0x70, 0x7c, 0xd9, 0xa2, 0xd3, 0x9a, 0xc2, 0xf5, 0x46, 0x34, 0x04, 0x47, 0x17, 0xc3, 0x95, 0x92, 0x2c, 0x34, 0x36, 0x85, 0x2b, 0x84, 0x1f, 0x2a, 0xf3, 0x98, 0xba, 0x84, 0x81, 0x3f, 0xad, 0xf8, 0x92, 0x0a, 0x9c, 0xfa, 0xda, 0xf3, 0x00, 0xba, 0x2f, 0x27, 0x80, 0x04, 0xcb, 0xa1, 0xc5, 0x0e, 0x90, 0xdc, 0x21, 0x4a, 0x5f, 0xea, 0x0a, 0x40, 0x3c, 0x76, 0xf6, 0x02, 0x60, 0x3d, 0x86, 0x83, 0x00, 0x08, 0x6a, 0x6a, 0x06, 0x98, 0xa5, 0x4e, 0x32, 0xb5, 0x83, 0xf7, 0x1f, 0x08, 0x55, 0xee, 0xc5, 0xc5, 0x06, 0x80, 0x45, 0x38, 0x8f, 0x8b, 0x5b, 0xa2, 0xe5, 0xc6, 0x3f, 0x8f, 0x4f, 0x00, 0x85, 0x07, 0x97, 0xde, 0xc3, 0x0d, 0x7c, 0x59, 0x8d, 0xf2, 0xd4, 0x83, 0x00, 0x4a, 0x8d, 0x0e, 0x57, 0x09, 0xfb, 0xfa, 0x1a, 0x0e, 0x0d, 0x20, 0x3a, 0x1a, 0xbc, 0x0c, 0x8d, 0x77, 0xfc, 0x75, 0x8a, 0x04, 0x50, 0x78, 0xb0, 0x97, 0xaf, 0x51, 0xae, 0x8c, 0x00, 0x64, 0x6a, 0xa0, 0x70, 0xbe, 0x6d, 0x26, 0x00, 0x4e, 0x7d, 0xe5, 0x89, 0x50, 0x73, 0x70, 0x98, 0xa7, 0x02, 0x84, 0xe3, 0xe3,
    0x84, 0x9c, 0xd8, 0xa7, 0xad, 0x00, 0xe0, 0xfc, 0xfd, 0x2e, 0xec, 0xb1, 0x8c, 0x0a, 0x80, 0x13, 0x84, 0xe3, 0x36, 0x0b, 0x04, 0xa0, 0x84, 0x31, 0xf5, 0x4a, 0x00, 0xab, 0xd3, 0x52, 0xe7, 0xca, 0x3c, 0x57, 0xe6, 0x8f, 0x61, 0x4a, 0x9d, 0x92, 0x3f, 0xa6, 0xbe, 0x20, 0x90, 0xf3, 0xe4, 0xd3, 0x31, 0x57, 0xe1, 0xb5, 0xc7, 0x29, 0x35, 0x0c, 0xf9, 0xf4, 0x76, 0x5a, 0x16, 0xec, 0x13, 0xb0, 0x0a, 0x3d, 0x8c, 0x6f, 0x06, 0x04, 0xc0, 0xbd, 0x75, 0xcc, 0x8d, 0xcb, 0x08, 0x19, 0x00, 0xe0, 0x3c, 0x35, 0x56, 0xe6, 0x8e, 0xf3, 0xa7, 0xf0, 0x79, 0xea, 0x4b, 0xcf, 0x86, 0xf3, 0x50, 0x75, 0x0b, 0xea, 0x40, 0x65, 0x16, 0x8b, 0x9e, 0xa5, 0x84, 0x03, 0x9c, 0x85, 0xcb, 0xa9, 0x5b, 0x77, 0xff, 0xe9, 0x95, 0x68, 0x3d, 0x7b, 0x96, 0x07, 0xbd, 0xb2, 0x1f, 0xa4, 0xbe, 0xb8, 0x09, 0xd6, 0xf3, 0xeb, 0x87, 0xaf, 0x00, 0xfe, 0xf4, 0x1c, 0x2a, 0x55, 0x4c, 0xc2, 0xf1, 0x7c, 0x89, 0x30, 0x3c, 0x4f, 0x3d, 0xab, 0x6c, 0x9e, 0xfa, 0x92, 0x80, 0x83, 0xc9, 0x63, 0x75, 0x8f, 0x61, 0x3f, 0xd9, 0x41, 0xe0, 0x26, 0x6b, 0x90, 0x8b, 0x9c, 0x2c, 0x11, 0x94, 0xf0, 0x3c, 0xf5, 0x32, 0x4f, 0xed, 0x6e, 0xbf, 0x6f, 0xf3, 0x07, 0xca, 0x2f, 0x76, 0xd9, 0x8b, 0x70, 0xd9, 0xe6, 0x3f, 0x4d, 0x7d, 0xfb, 0x55, 0xea, 0x6b, 0x27, 0xc1, 0x24, 0xec, 0xe7, 0x61, 0x37, 0xdf, 0x85, 0xf3, 0x6b, 0x14, 0x2c, 0xaf, 0xc2, 0xbf, 0x48, 0x7d, 0x85, 0xe0, 0x0f, 0x3b, 0xff, 0x65, 0x18, 0x6e, 0xd6, 0xac, 0x59, 0xb3, 0x76, 0xa1, 0xfd, 0x0f, 0xfe, 0x7c, 0xe5, 0x50, 0x9f, 0x14, 0x88, 0x19, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};
unsigned int pilotwire_default_len = 2555;

/*****************************************\
 *               Variables
\*****************************************/

// pilotwire : configuration
struct
{
  bool direct = false;                                  // pilotwire or direct connexion
  bool window = false;                                  // flag to enable window open detection
  bool presence = false;                                // flag to enable movement detection
  bool ecowatt = true;                                  // flag to enable ecowatt management
  uint8_t mode = PILOTWIRE_MODE_OFF;                    // default mode is off
  float pres_delay = PILOTWIRE_PRESENCE_HOUR_DELAY;     // default presence detection initial delay
  float pres_reduce = PILOTWIRE_PRESENCE_HOUR_REDUCE;   // default presence detection temperature reduction every hour
  float arr_temp[PILOTWIRE_TARGET_MAX];                 // array of target temperatures (according to status)
} pilotwire_config;

// pilotwire : general status
struct
{
  bool  heating    = false;                             // flag to indicate night mode
  bool  night_mode = false;                             // flag to indicate night mode
  bool  json_update = false;                            // flag to publish a json update
  float temp_json = PILOTWIRE_TEMP_DEFAULT_NORMAL;      // last published temperature
  float temp_current = NAN;                             // current temperature
  float temp_target = NAN;                              // target temperature
} pilotwire_status;

// pilotwire : open window detection
struct
{
  bool opened = false;                         // open window active
  uint8_t cnt_period = 0;                      // period for temperature measurement (sec)
  uint8_t idx_temp = 0;                        // current index of temperature array
  uint32_t time_over = UINT32_MAX;             // timestamp for end of open window timeout
  float low_temp = NAN;                        // lower temperature during open window phase
  float arr_temp[PILOTWIRE_WINDOW_SAMPLE_NBR]; // array of values to detect open window
} pilotwire_window;

// pilotwire : movement detection
struct
{
  uint32_t time_presence = UINT32_MAX; // timestamp of last presence detection
  uint32_t time_reset = UINT32_MAX;    // timestamp of last detection reset
} pilotwire_presence;

/**************************************************\
 *                  Commands
\**************************************************/

// offload help
void CmndPilotwireHelp ()
{
  uint8_t index;
  char    str_text[32];

  AddLog (LOG_LEVEL_INFO, PSTR("HLP: Pilotwire commands :"));
  AddLog (LOG_LEVEL_INFO, PSTR(" - pw_type = device type"));
  for (index = 0; index < PILOTWIRE_DEVICE_MAX; index++)
  {
    GetTextIndexed (str_text, sizeof(str_text), index, kPilotwireDevice);
    AddLog (LOG_LEVEL_INFO, PSTR("     %u - %s"), index, str_text);
  }

  AddLog (LOG_LEVEL_INFO, PSTR(" - pw_mode = heater running mode"));
  for (index = 0; index < PILOTWIRE_MODE_MAX; index++)
  {
    GetTextIndexed (str_text, sizeof(str_text), index, kPilotwireModeLabel);
    AddLog (LOG_LEVEL_INFO, PSTR("     %u - %s"), index, str_text);
  }

  AddLog (LOG_LEVEL_INFO, PSTR(" - pw_target   = target temperature (°C)"));

  AddLog (LOG_LEVEL_INFO, PSTR(" - pw_ecowatt  = enable ecowatt management (0/1)"));
  AddLog (LOG_LEVEL_INFO, PSTR(" - pw_window   = enable open window mode (0/1)"));
  AddLog (LOG_LEVEL_INFO, PSTR(" - pw_presence = enable presence detection (0/1)"));

  AddLog (LOG_LEVEL_INFO, PSTR(" - pw_low      = lowest temperature (°C)"));
  AddLog (LOG_LEVEL_INFO, PSTR(" - pw_high     = highest temperature (°C)"));

  AddLog (LOG_LEVEL_INFO, PSTR(" - pw_comfort  = default comfort temperature (°C)"));
  AddLog (LOG_LEVEL_INFO, PSTR(" - pw_nofrost  = no frost temperature (delta with comfort if negative)"));
  AddLog (LOG_LEVEL_INFO, PSTR(" - pw_eco      = eco temperature (delta with comfort if negative)"));
  AddLog (LOG_LEVEL_INFO, PSTR(" - pw_night    = night temperature (delta with comfort if negative)"));
  AddLog (LOG_LEVEL_INFO, PSTR(" - pw_eco2     = ecowatt level 2 temperature (delta with comfort if negative)"));
  AddLog (LOG_LEVEL_INFO, PSTR(" - pw_eco3     = ecowatt level 3 temperature (delta with comfort if negative)"));

  AddLog (LOG_LEVEL_INFO, PSTR(" - pw_pdelay   = delay before temperature drop if no presence (hour)"));
  AddLog (LOG_LEVEL_INFO, PSTR(" - pw_preduce  = temperature reduction per hour if no presence (°C)"));

  AddLog (LOG_LEVEL_INFO, PSTR(" - pw_icon     = set room icon URL (null to remove)"));

  ResponseCmndDone ();
}

// set heater type and save
void CmndPilotwireType()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetDeviceType ((bool)XdrvMailbox.payload, true);
  ResponseCmndNumber (pilotwire_config.direct);
}

// set running mode
void CmndPilotwireMode()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetMode ((uint8_t)XdrvMailbox.payload);
  ResponseCmndNumber (pilotwire_config.mode);
}

// set target temperature without saving
void CmndPilotwireTarget ()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetTargetTemperature (PILOTWIRE_TARGET_COMFORT, atof(XdrvMailbox.data), false);
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

void CmndPilotwireTargetEcowatt2 ()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetTargetTemperature (PILOTWIRE_TARGET_ECOWATT2, atof(XdrvMailbox.data), true);
  ResponseCmndFloat (pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT2], 1);
}

void CmndPilotwireTargetEcowatt3 ()
{
  if (XdrvMailbox.data_len > 0) PilotwireSetTargetTemperature (PILOTWIRE_TARGET_ECOWATT3, atof(XdrvMailbox.data), true);
  ResponseCmndFloat (pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT3], 1);
}

void CmndPilotwirePresenceDelay ()
{
  if (XdrvMailbox.data_len > 0)
  {
    pilotwire_config.pres_delay = atof (XdrvMailbox.data);
    PilotwireSaveConfig();
  }
  ResponseCmndFloat (pilotwire_config.pres_delay, 2);
}

void CmndPilotwirePresenceReduce ()
{
  if (XdrvMailbox.data_len > 0)
  {
    pilotwire_config.pres_reduce = atof (XdrvMailbox.data);
    PilotwireSaveConfig ();
  }
  ResponseCmndFloat (pilotwire_config.pres_reduce, 2);
}

void CmndPilotwireIconURL ()
{
  char str_url[64];

  if (XdrvMailbox.data_len > 0)
  {
    strcpy (str_url, "");
    if (strcmp (XdrvMailbox.data, "null") != 0) strlcpy (str_url, XdrvMailbox.data, sizeof (str_url));
    SettingsUpdateText (SET_PILOTWIRE_ICON_URL, str_url);
  }
  ResponseCmndChar (SettingsText (SET_PILOTWIRE_ICON_URL));
}

/******************************************************\
 *                  Main functions
\******************************************************/

// get pilotwire device state from relay state
bool PilotwireGetHeaterState ()
{
  bool status;

  // read relay state and invert if in pilotwire mode
  status = bitRead(TasmotaGlobal.power, 0);
  if (!pilotwire_config.direct) status = !status;

  return status;
}

// set relays state
bool PilotwireGetHeatingStatus ()
{
  bool status = PilotwireGetHeaterState();

  // if device is having a power sensor, declare no heating if less than minimum drawn power
  if (offload_status.sensor && (offload_status.pinst < OFFLOAD_POWER_MINIMUM)) status = false;

  return status;
}

// set relays state
void PilotwireSetHeaterState (const bool new_state)
{
  // if relay needs to be toggled
  if (new_state != PilotwireGetHeaterState())
  {
    // disable presence detection to avoid false positive
    SensorPresenceSuspendDetection(PILOTWIRE_PRESENCE_IGNORE);

    // toggle relay and ask for JSON update
    ExecuteCommandPower(1, POWER_TOGGLE, SRC_MAX);

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
  if (OffloadIsOffloaded()) temperature = min (temperature, PilotwireConvertTargetTemperature (PILOTWIRE_TARGET_NOFROST));

  // if window opened, target is no frost
  if (pilotwire_window.opened) temperature = min (temperature, PilotwireConvertTargetTemperature (PILOTWIRE_TARGET_NOFROST));

  // if status is not forced, check night mode, presence and ecowatt
  if (pilotwire_config.mode != PILOTWIRE_MODE_FORCED)
  {
    // if night mode, update target
    if (pilotwire_status.night_mode) temperature = min (temperature, PilotwireConvertTargetTemperature (PILOTWIRE_TARGET_NIGHT));

    // if ecowatt level is detected : 2 (warning) or 3 (critical)
    level = PilotwireEcowattGetLevel();
    if (level == ECOWATT_LEVEL_WARNING) temperature = min (temperature, PilotwireConvertTargetTemperature (PILOTWIRE_TARGET_ECOWATT2));
    else if (level == ECOWATT_LEVEL_POWERCUT) temperature = min (temperature, PilotwireConvertTargetTemperature (PILOTWIRE_TARGET_ECOWATT3));

    // adjust temperature according to presence detection
    if (pilotwire_config.presence) temperature = PilotwirePresenceAdjustTargetTemperature (temperature);
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
  ext_snprintf_P (str_target, sizeof(str_target), PSTR("%01_f"), &target);
  GetTextIndexed (str_type, sizeof(str_type), type, kPilotwireTargetLabel);
  AddLog (LOG_LEVEL_INFO, PSTR("PIL: %s temperature set to %s"), str_type, str_target);

  // if needed, save configuration
  if (to_save) PilotwireSaveConfig();

  // ask for JSON update
  pilotwire_status.json_update = true;
}

// set heater configuration mode
void PilotwireSetDeviceType (const bool is_direct, const bool to_save)
{
  char str_type[16];

  // set type
  pilotwire_config.direct = is_direct;

  // log action
  GetTextIndexed (str_type, sizeof(str_type), (uint8_t)pilotwire_config.direct, kPilotwireDevice);
  AddLog (LOG_LEVEL_INFO, PSTR("PIL: Device set to %s"), str_type);

  // if needed, save
  if (to_save) PilotwireSaveConfig();

  // ask for JSON update
  pilotwire_status.json_update = true;
}

// set heater target mode
void PilotwireSetMode(const uint8_t mode)
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
      pilotwire_presence.time_reset = LocalTime();

      // if window detection is active, reset open window detection
      if (pilotwire_config.window) PilotwireWindowResetDetection();
    }
  }

  // ask for JSON update
  pilotwire_status.json_update = true;

  // log change
  GetTextIndexed (str_mode, sizeof(str_mode), mode, kPilotwireModeLabel);
  AddLog (LOG_LEVEL_INFO, PSTR("PIL: Switch status to %s"), str_mode);
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
    ext_snprintf_P (str_value, sizeof(str_value), PSTR("%01_f"), &target);
    AddLog (LOG_LEVEL_INFO, PSTR("PIL: Set target temp. to %s"), str_value);
  }

  // ask for JSON update
  pilotwire_status.json_update = true;
}

// update temperature
void PilotwireTemperatureUpdate ()
{
  // update current temperature
  pilotwire_status.temp_current = SensorTemperatureGet (60);

  // update JSON if temperature has changed of 0.2°C minimum
  if (!isnan(pilotwire_status.temp_current))
  {
    if (abs(pilotwire_status.temp_json - pilotwire_status.temp_current) >= 0.2)
    {
      pilotwire_status.temp_json = pilotwire_status.temp_current;
      pilotwire_status.json_update = true;
    }
  }
}

// update heater relay state
void PilotwireStateUpdate ()
{
  bool  status, target;
  float temperature, temp_target;

  // get current relay state
  target = status = PilotwireGetHeaterState ();

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

  // update history slot
  if (target) SensorActivitySet ();
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
  if (to_save) PilotwireSaveConfig();

  // ask for JSON update
  pilotwire_status.json_update = true;
}

// set ecowatt management option
uint8_t PilotwireEcowattGetLevel()
{
  uint8_t level = ECOWATT_LEVEL_NORMAL;

  // if ecowatt management is enabled
  if (pilotwire_config.ecowatt) level = EcowattGetCurrentLevel();

  return level;
}

/***********************************************\
 *              Window detection
\***********************************************/

// set window detection option
void PilotwireWindowEnable(const bool state, const bool to_save)
{
  // set main flag
  pilotwire_config.window = state;

  // reset window detection algo
  PilotwireWindowResetDetection();

  // if needed, save
  if (to_save) PilotwireSaveConfig();
}

// reset window detection data
void PilotwireWindowResetDetection()
{
  uint8_t index;

  // declare window closed
  pilotwire_window.opened = false;
  pilotwire_window.low_temp = NAN;
  pilotwire_window.time_over = UINT32_MAX;

  // reset detection temperatures
  for (index = 0; index < PILOTWIRE_WINDOW_SAMPLE_NBR; index++) pilotwire_window.arr_temp[index] = NAN;

  // ask for JSON update
  pilotwire_status.json_update = true;
}

// update opened window detection data
void PilotwireWindowUpdateDetection()
{
  bool     finished;
  uint8_t  index, idx_array;
  uint32_t time_now = LocalTime();
  float    delta = 0;

  // if window detection is not enabled, exit
  if (!pilotwire_config.window) return;

  // if window considered closed, do open window detection
  if (!pilotwire_window.opened)
  {
    // if temperature is available, update temperature detection array
    if (!isnan(pilotwire_status.temp_current))
    {
      // if period reached
      if (pilotwire_window.cnt_period == 0)
      {
        // record temperature and increment index
        pilotwire_window.arr_temp[pilotwire_window.idx_temp] = pilotwire_status.temp_current;
        pilotwire_window.idx_temp++;
        pilotwire_window.idx_temp = pilotwire_window.idx_temp % PILOTWIRE_WINDOW_SAMPLE_NBR;
      }

      // increment period counter
      pilotwire_window.cnt_period++;
      pilotwire_window.cnt_period = pilotwire_window.cnt_period % PILOTWIRE_WINDOW_OPEN_PERIOD;
    }

    // loop thru last measured temperatures
    for (index = 0; index < PILOTWIRE_WINDOW_SAMPLE_NBR; index++)
    {
      // calculate temperature array in reverse order
      idx_array = (PILOTWIRE_WINDOW_SAMPLE_NBR + pilotwire_window.idx_temp - index - 1) % PILOTWIRE_WINDOW_SAMPLE_NBR;

      // calculate temperature delta
      if (!isnan(pilotwire_window.arr_temp[idx_array])) delta = max(delta, pilotwire_window.arr_temp[idx_array] - pilotwire_status.temp_current);

      // if temperature drop detected, stop analysis
      if (delta >= PILOTWIRE_WINDOW_OPEN_DROP) break;
    }

    // if temperature drop detected, window is detected as opended
    if (delta >= PILOTWIRE_WINDOW_OPEN_DROP)
    {
      // declare window as opened
      pilotwire_window.opened    = true;
      pilotwire_window.low_temp  = pilotwire_status.temp_current;
      pilotwire_window.time_over = time_now + PILOTWIRE_WINDOW_TIMEOUT;

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
    finished = (time_now > pilotwire_window.time_over);

    // if current temperature has increased enought, window is closed
    finished |= (pilotwire_status.temp_current - pilotwire_window.low_temp >= PILOTWIRE_WINDOW_CLOSE_INCREASE);

    // if over, reset window detection data
    if (finished) PilotwireWindowResetDetection ();
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

  // if needed, save
  if (to_save) PilotwireSaveConfig();

  // ask for JSON update
  pilotwire_status.json_update = true;
}

bool PilotwirePresenceUpdateDetection()
{
  bool result;

  // update presence detection
  result = SensorPresenceGet (PILOTWIRE_PRESENCE_TIMEOUT);
  if (result)
  {
    pilotwire_presence.time_presence = LocalTime();
    pilotwire_presence.time_reset = pilotwire_presence.time_presence;
  }

  return result;
}

float PilotwirePresenceAdjustTargetTemperature(const float temp_target)
{
  uint32_t time_start, time_now;
  float    temp_decrease, temp_result, temp_eco;
  float    time_delay;

  // if no presence detection, return current temperature
  if (pilotwire_presence.time_reset == UINT32_MAX)
    return temp_target;

  // if taregt is lower or equal to ECO, nothing to do
  temp_eco = PilotwireConvertTargetTemperature(PILOTWIRE_TARGET_ECO);
  if (temp_target <= temp_eco) return temp_target;

  // if presence detection delay not reached, nothing to do
  time_now = LocalTime();
  time_delay = 3600 * pilotwire_config.pres_delay;
  time_start = pilotwire_presence.time_reset + (uint32_t)time_delay;
  if (time_now < time_start) return temp_target;

  // calculate temperature after decrease
  temp_decrease = (float)(time_now - time_start) * pilotwire_config.pres_reduce / 3600;
  temp_result = max(temp_eco, temp_target - temp_decrease);

  return temp_result;
}

/**************************************************\
 *                  Configuration
\**************************************************/

void PilotwireLoadConfig()
{
  // device type
  pilotwire_config.direct = (bool)Settings->sbflag1.spare28;

  // enable options
  PilotwireWindowEnable   ((Settings->sbflag1.spare29 == 1), false);
  PilotwirePresenceEnable ((Settings->sbflag1.spare30 == 1), false);
  PilotwireEcowattEnable  ((Settings->sbflag1.spare31 == 1), false);

  // presence detection data
  pilotwire_config.pres_delay = (float)Settings->free_ea6[0] / 4;
  pilotwire_config.pres_reduce = (float)Settings->free_ea6[1] / 100;

  // target temperature
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_LOW]      = ((float)Settings->free_ea6[2] - 100) / 2;
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_HIGH]     = ((float)Settings->free_ea6[3] - 100) / 2;
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_COMFORT]  = ((float)Settings->free_ea6[4] - 100) / 2;
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_NIGHT]    = ((float)Settings->free_ea6[5] - 100) / 2;
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECO]      = ((float)Settings->free_ea6[6] - 100) / 2;
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_NOFROST]  = ((float)Settings->free_ea6[7] - 100) / 2;
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT2] = ((float)Settings->free_ea6[8] - 100) / 2;
  pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT3] = ((float)Settings->free_ea6[9] - 100) / 2;

  // avoid values out of range
  if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_LOW] < PILOTWIRE_TEMP_LIMIT_MIN) pilotwire_config.arr_temp[PILOTWIRE_TARGET_LOW] = PILOTWIRE_TEMP_DEFAULT_LOW;
  else if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_LOW] > PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_temp[PILOTWIRE_TARGET_LOW] = PILOTWIRE_TEMP_DEFAULT_LOW;

  if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_HIGH] < PILOTWIRE_TEMP_LIMIT_MIN) pilotwire_config.arr_temp[PILOTWIRE_TARGET_HIGH] = PILOTWIRE_TEMP_DEFAULT_HIGH;
  else if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_HIGH] > PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_temp[PILOTWIRE_TARGET_HIGH] = PILOTWIRE_TEMP_DEFAULT_HIGH;

  if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_COMFORT] < PILOTWIRE_TEMP_LIMIT_MIN) pilotwire_config.arr_temp[PILOTWIRE_TARGET_COMFORT] = PILOTWIRE_TEMP_DEFAULT_NORMAL;
  else if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_COMFORT] > PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_temp[PILOTWIRE_TARGET_COMFORT] = PILOTWIRE_TEMP_DEFAULT_NORMAL;

  if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_NIGHT] < -PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_temp[PILOTWIRE_TARGET_NIGHT] = PILOTWIRE_TEMP_DEFAULT_NIGHT;
  else if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_NIGHT] > PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_temp[PILOTWIRE_TARGET_NIGHT] = PILOTWIRE_TEMP_DEFAULT_NIGHT;

  if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECO] < -PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECO] = PILOTWIRE_TEMP_DEFAULT_ECO;
  else if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECO] > PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECO] = PILOTWIRE_TEMP_DEFAULT_ECO;

  if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_NOFROST] < -PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_temp[PILOTWIRE_TARGET_NOFROST] = PILOTWIRE_TEMP_DEFAULT_NOFROST;
  else if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_NOFROST] > PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_temp[PILOTWIRE_TARGET_NOFROST] = PILOTWIRE_TEMP_DEFAULT_NOFROST;

  if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT2] < -PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT2] = PILOTWIRE_TEMP_DEFAULT_ECO;
  else if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT2] > PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT2] = PILOTWIRE_TEMP_DEFAULT_ECO;

  if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT3] < -PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT3] = PILOTWIRE_TEMP_DEFAULT_NOFROST;
  else if (pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT3] > PILOTWIRE_TEMP_LIMIT_MAX) pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT3] = PILOTWIRE_TEMP_DEFAULT_NOFROST;

  // log
  AddLog (LOG_LEVEL_INFO, PSTR("PIL: Loaded configuration"));
}

void PilotwireSaveConfig()
{
  // mode and flags
  Settings->sbflag1.spare28 = (uint8_t)pilotwire_config.direct;
  Settings->sbflag1.spare29 = (uint8_t)pilotwire_config.window;
  Settings->sbflag1.spare30 = (uint8_t)pilotwire_config.presence;
  Settings->sbflag1.spare31 = (uint8_t)pilotwire_config.ecowatt;

  // presence timeout
  Settings->free_ea6[0] = (uint8_t)(pilotwire_config.pres_delay * 4);
  Settings->free_ea6[1] = (uint8_t)(pilotwire_config.pres_reduce * 100);

  // target temperature
  Settings->free_ea6[2] = (uint8_t)(pilotwire_config.arr_temp[PILOTWIRE_TARGET_LOW]      * 2 + 100);
  Settings->free_ea6[3] = (uint8_t)(pilotwire_config.arr_temp[PILOTWIRE_TARGET_HIGH]     * 2 + 100);
  Settings->free_ea6[4] = (uint8_t)(pilotwire_config.arr_temp[PILOTWIRE_TARGET_COMFORT]  * 2 + 100);
  Settings->free_ea6[5] = (uint8_t)(pilotwire_config.arr_temp[PILOTWIRE_TARGET_NIGHT]    * 2 + 100);
  Settings->free_ea6[6] = (uint8_t)(pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECO]      * 2 + 100);
  Settings->free_ea6[7] = (uint8_t)(pilotwire_config.arr_temp[PILOTWIRE_TARGET_NOFROST]  * 2 + 100);
  Settings->free_ea6[8] = (uint8_t)(pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT2] * 2 + 100);
  Settings->free_ea6[9] = (uint8_t)(pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT3] * 2 + 100);
}

/***********************************************\
 *                  Callback
\***********************************************/

// Called by Offload driver
//  - if coming from timer : relay ON = switch to night mode, relay OFF = switch back to normal mode
//  - if coming from anything else than SRC_MAX : ignore
bool PilotwireSetDevicePower ()
{
  bool result = true; // command handled by default
  uint32_t target;

  // if command is from a timer, handle night mode
  if (XdrvMailbox.payload == SRC_TIMER)
  {
    // set night mode according to POWER switch
    if (XdrvMailbox.index == POWER_OFF) pilotwire_status.night_mode = false;
    else if (XdrvMailbox.index == POWER_ON) pilotwire_status.night_mode = true;
  }

  // else if command is not from the module, log and ignore it
  else if (XdrvMailbox.payload != SRC_MAX) AddLog (LOG_LEVEL_INFO, PSTR("PIL: Relay order ignored from %u"), XdrvMailbox.payload);

  // else command not handled
  else result = false;

  return result;
}

// Show JSON status (for MQTT)
// Format is
//   "PilotWire":{"Mode":2,"Temp":18.6,"Target":21.0,"High":22.0,"Heat":1,"Night":0,"Eco":1,"Win":0,"Pres":1,"Delay":262,"Event":2}
void PilotwireShowJSON(bool is_autonomous)
{
  bool heating, window, presence;
  uint8_t mode, ecowatt, event;
  uint32_t time_now, delay;
  float temperature;
  char str_value[16];

  // get current time
  time_now = LocalTime();

  // add , in append mode or { in direct publish mode
  if (is_autonomous) Response_P (PSTR("{"));
    else ResponseAppend_P (PSTR(","));

  // pilotwire section
  ResponseAppend_P (PSTR("\"Pilotwire\":{"));

  // configuration mode
  if ((pilotwire_config.mode == PILOTWIRE_MODE_COMFORT) && pilotwire_status.night_mode) mode = PILOTWIRE_MODE_NIGHT;
    else mode = pilotwire_config.mode;
  ResponseAppend_P (PSTR("\"Mode\":%d"), mode);

  // current temperature
  ext_snprintf_P (str_value, sizeof(str_value), PSTR("%1_f"), &pilotwire_status.temp_current);
  ResponseAppend_P (PSTR(",\"Temp\":%s"), str_value);

  // target temperature
  temperature = PilotwireGetModeTemperature(pilotwire_config.mode);
  if (pilotwire_status.night_mode) temperature = min(temperature, PilotwireConvertTargetTemperature(PILOTWIRE_TARGET_NIGHT));
  ext_snprintf_P (str_value, sizeof(str_value), PSTR("%1_f"), &temperature);
  ResponseAppend_P (PSTR(",\"Target\":%s"), str_value);

  // max temperature
  temperature = PilotwireConvertTargetTemperature (PILOTWIRE_TARGET_HIGH);
  ext_snprintf_P (str_value, sizeof(str_value), PSTR("%1_f"), &temperature);
  ResponseAppend_P (PSTR(",\"High\":%s"), str_value);

  // default heating state
  ResponseAppend_P (PSTR(",\"Heat\":%u"), pilotwire_status.heating);

  // ecowatt management status
  ecowatt = PilotwireEcowattGetLevel ();
  ResponseAppend_P (PSTR(",\"Eco\":%u"), ecowatt);

  // window open detection status
  window = PilotwireWindowGetStatus ();
  ResponseAppend_P (PSTR(",\"Win\":%u"), window);

  // presence detection status and delay (in mn)
  presence = PilotwirePresenceUpdateDetection ();
  ResponseAppend_P (PSTR(",\"Pres\":%u"), presence);

  // time since last presence (mn)
  if (pilotwire_config.presence)
  {
    if (presence) delay = 0;
    else delay = (time_now - pilotwire_presence.time_presence) / 60;
    ResponseAppend_P (PSTR(",\"Since\":%u"), delay);
  }

  // set event type thru priority : off, heating, offload, ecowatt level 2, ecowatt level 3, window opened, absence, temperature reached
  if (pilotwire_config.mode == PILOTWIRE_MODE_OFF) event = PILOTWIRE_EVENT_NONE;
  else if (pilotwire_status.heating) event = PILOTWIRE_EVENT_HEATING;
  else if (OffloadIsOffloaded()) event = PILOTWIRE_EVENT_OFFLOAD;
  else if (ecowatt == ECOWATT_LEVEL_WARNING) event = PILOTWIRE_EVENT_ECOWATT2;
  else if (ecowatt == ECOWATT_LEVEL_POWERCUT) event = PILOTWIRE_EVENT_ECOWATT3;
  else if (window) event = PILOTWIRE_EVENT_WINDOW;
  else event = PILOTWIRE_EVENT_NONE;
  ResponseAppend_P (PSTR(",\"Event\":%u"), event);

  // set icon associated with event
  GetTextIndexed (str_value, sizeof(str_value), event, kPilotwireEventIcon);
  ResponseAppend_P (PSTR(",\"Icon\":\"%s\""), str_value);

  ResponseAppend_P (PSTR("}"));

  // publish it if not in append mode
  if (is_autonomous)
  {
    // add offload status
    OffloadShowJSON(false);

    // publish message
    ResponseAppend_P (PSTR("}"));
    MqttPublishTeleSensor();
  }

  // reset update flag and update switch status
  pilotwire_status.json_update = false;
  pilotwire_status.temp_json = pilotwire_status.temp_current;
}

void PilotwireInit()
{
  uint32_t index, count;

  // offload : module is not managing the relay
  OffloadSetManagedMode(false);

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

  // initialise open window detection temperature array
  for (index = 0; index < PILOTWIRE_WINDOW_SAMPLE_NBR; index++) pilotwire_window.arr_temp[index] = NAN;

  // load configuration
  PilotwireLoadConfig();

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR("HLP: pw_help to get help on Pilotwire commands"));
}

// called every 100ms to publish JSON
void PilotwireEvery100ms()
{
  if (pilotwire_status.json_update)
  {
    PilotwireShowJSON(true);
    pilotwire_status.json_update = false;
  }
}

// called every second, to update
//  - temperature
//  - presence detection
//  - graph data
void PilotwireEverySecond()
{
  bool     heating;
  uint32_t index;


  // nothing in the first 5 seconds
  if (TasmotaGlobal.uptime < 5) return;

  // update window open detection
  PilotwireWindowUpdateDetection ();

  // update presence detection
  PilotwirePresenceUpdateDetection ();

  // update heater status
  heating = PilotwireGetHeatingStatus ();
  if (pilotwire_status.heating != heating) pilotwire_status.json_update = true;
  pilotwire_status.heating = heating;

  // update temperature and target
  PilotwireTemperatureUpdate ();

  // update heater relay state
  PilotwireStateUpdate ();
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// icons
void PilotwireWebIconDefault ()
{ 
#ifdef USE_UFILESYS
  File file;

  // open file in read only mode in littlefs filesystem
  if (ffsp->exists(PILOTWIRE_ICON_LOGO))
  {
    file = ffsp->open(PILOTWIRE_ICON_LOGO, "r");
    Webserver->streamFile(file, "image/jpeg");
    file.close();
  }
  else
#endif // USE_UFILESYS

  // send default icon
  Webserver->send_P (200, "image/png", pilotwire_default_png, pilotwire_default_len);
}

// get status logo : flame, offload, window opened, ecowatt, ...
void PilotwireWebIconStatus(char *pstr_status, const size_t size_status, const bool include_heating)
{
  uint8_t ecowatt;
  String str_result = "";

  // verification
  if (pstr_status == nullptr) return;
  if (size_status == 0) return;

  // heating status
  if (include_heating && pilotwire_status.heating) str_result += " 🔥";

  // device offloaded
  if (OffloadIsOffloaded()) str_result += " ⚡";

  // window opened
  if (PilotwireWindowGetStatus()) str_result += " 🪟";

  // device ecowatt alert
  ecowatt = EcowattGetCurrentLevel();
  if (ecowatt == ECOWATT_LEVEL_WARNING) str_result += " 🟠";
    else if (ecowatt == ECOWATT_LEVEL_POWERCUT) str_result += " 🔴";

  // format result
  str_result.trim();
  if (str_result.length() == 0) str_result = "&nbsp;";
  strlcpy(pstr_status, str_result.c_str(), size_status);
}

// append pilot wire state to main page
void PilotwireWebSensor ()
{
  float temp_target;
  char  str_text[32];
  char  str_temp[8];
  char  str_target[8];

  // get mode and status icon

  // read current and target temperature
  temp_target = PilotwireGetCurrentTargetTemperature();
  ext_snprintf_P (str_target, sizeof(str_target), PSTR("%01_f"), &temp_target);
  ext_snprintf_P (str_temp, sizeof(str_temp), PSTR("%01_f"), &pilotwire_status.temp_current);

  // display
  WSContentSend_PD(PSTR("<div style='font-size:16px;text-align:center;margin-top:4px;padding:2px 6px;background:#333333;border-radius:8px;'>\n"));
  WSContentSend_PD(PSTR("<div style='display:flex;padding:0px;'>\n"));

  GetTextIndexed (str_text, sizeof(str_text), offload_config.type, kOffloadDevice);
  WSContentSend_PD(PSTR("<div style='width:28%%;padding:0px;text-align:left;font-weight:bold;'>%s</div>\n"), str_text);

  GetTextIndexed (str_text, sizeof(str_text), pilotwire_config.mode, kPilotwireModeIcon);
  WSContentSend_PD(PSTR("<div style='width:22%%;padding:0px;text-align:left;font-size:32px;'>%s</div>\n"), str_text);

  PilotwireWebIconStatus(str_text, sizeof(str_text), true);
  WSContentSend_PD(PSTR("<div style='width:15%%;padding:0px;font-size:20px;padding-top:8px;'>%s</div>\n"), str_text);

  WSContentSend_PD(PSTR("<div style='width:35%%;padding:0px;text-align:right;'><b>%s</b><span style='font-size:10px;'> /%s</span> °C<br>%u VA</div>\n"), str_temp, str_target, offload_config.device_power);

  WSContentSend_PD(PSTR("</div>\n"));
  WSContentSend_PD(PSTR("</div>\n"));
}

// intermediate page to update selected status from main page
void PilotwireWebPageSwitchStatus()
{
  char str_argument[16];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // check for 'therm' parameter
  WebGetArg (D_CMND_PILOTWIRE_MODE, str_argument, sizeof(str_argument));
  if (strlen (str_argument) > 0) PilotwireSetMode ((uint8_t)atoi(str_argument));

  // auto reload root page with dark background
  WSContentStart_P (D_PILOTWIRE_CONTROL, false);
  WSContentSend_P (PSTR("</script>\n"));
  WSContentSend_P (PSTR("<meta http-equiv='refresh' content='0;URL=/' />\n"));
  WSContentSend_P (PSTR("</head>\n"));
  WSContentSend_P (PSTR("<body bgcolor='#303030'></body>\n"));
  WSContentSend_P (PSTR("</html>\n"));
  WSContentEnd ();
}

// select mode on main mage
void PilotwireWebMainSelectMode ()
{
  uint32_t index;
  float    temperature;
  char     str_icon[8];
  char     str_temp[12];
  char     str_label[16];

  // status mode options
  WSContentSend_P (PSTR("<p><form action='%s' method='get'>\n"), D_PILOTWIRE_PAGE_STATUS);
  WSContentSend_P (PSTR("<select style='width:100%%;text-align:center;padding:8px;border-radius:0.3rem;' name='%s' onchange='this.form.submit();'>\n"), D_CMND_PILOTWIRE_MODE, pilotwire_config.mode);
  WSContentSend_P (PSTR("<option value='%d' %s>%s</option>\n"), UINT8_MAX, "", PSTR("Pilotwire Mode"));
  for (index = 0; index < PILOTWIRE_MODE_MAX; index++)
  {
    // get display
    GetTextIndexed (str_icon, sizeof(str_icon), index, kPilotwireModeIcon);
    GetTextIndexed (str_label, sizeof(str_label), index, kPilotwireModeLabel);

    // get temperature
    temperature = PilotwireGetModeTemperature(index);
    if (index == PILOTWIRE_MODE_FORCED) strcpy(str_temp, "");
      else ext_snprintf_P (str_temp, sizeof(str_temp), PSTR("(%01_f °C)"), &temperature);

    // display status mode
    WSContentSend_P (PSTR("<option value='%d'>%s %s %s</option>\n"), index, str_icon, str_label, str_temp);
  }
  WSContentSend_P (PSTR("</select>\n"));
  WSContentSend_P (PSTR("</form></p>\n"));
}

// Pilotwire heater configuration web page
void PilotwireWebPageConfigure()
{
  bool state;
  uint8_t value;
  float temperature;
  char str_icon[8];
  char str_step[8];
  char str_value[16];
  char str_argument[24];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg (F("save")))
  {
    // get pilotwire device type according to 'type' parameter
    WebGetArg (D_CMND_PILOTWIRE_TYPE, str_argument, sizeof(str_argument));
    if (strlen(str_argument) > 0) PilotwireSetDeviceType((bool)atoi(str_argument), false);

    // enable open window detection according to 'window' parameter
    WebGetArg (D_CMND_PILOTWIRE_WINDOW, str_argument, sizeof(str_argument));
    state = (strcmp(str_argument, "on") == 0);
    PilotwireWindowEnable (state, false);

    // enable presence detection according to 'pres' parameter
    WebGetArg (D_CMND_PILOTWIRE_PRESENCE, str_argument, sizeof(str_argument));
    state = (strcmp(str_argument, "on") == 0);
    PilotwirePresenceEnable (state, false);

    // enable ecowatt detection according to 'ecowatt' parameter
    WebGetArg (D_CMND_PILOTWIRE_ECOWATT, str_argument, sizeof(str_argument));
    state = (strcmp(str_argument, "on") == 0);
    PilotwireEcowattEnable (state, false);

    // lowest temperature according to 'low' parameter
    WebGetArg (D_CMND_PILOTWIRE_LOW, str_argument, sizeof(str_argument));
    if (strlen(str_argument) > 0) PilotwireSetTargetTemperature(PILOTWIRE_TARGET_LOW, atof(str_argument), false);

    // highest temperature according to 'high' parameter
    WebGetArg (D_CMND_PILOTWIRE_HIGH, str_argument, sizeof(str_argument));
    if (strlen(str_argument) > 0) PilotwireSetTargetTemperature(PILOTWIRE_TARGET_HIGH, atof(str_argument), false);

    // comfort mode temperature according to 'comfort' parameter
    WebGetArg (D_CMND_PILOTWIRE_COMFORT, str_argument, sizeof(str_argument));
    if (strlen(str_argument) > 0) PilotwireSetTargetTemperature(PILOTWIRE_TARGET_COMFORT, atof(str_argument), false);

    // night mode temperature according to 'night' parameter
    WebGetArg (D_CMND_PILOTWIRE_NIGHT, str_argument, sizeof(str_argument));
    if (strlen(str_argument) > 0) PilotwireSetTargetTemperature(PILOTWIRE_TARGET_NIGHT, atof(str_argument), false);

    // eco temperature according to 'eco' parameter
    WebGetArg (D_CMND_PILOTWIRE_ECO, str_argument, sizeof(str_argument));
    if (strlen(str_argument) > 0) PilotwireSetTargetTemperature(PILOTWIRE_TARGET_ECO, atof(str_argument), false);

    // no frost temperature according to 'nofrost' parameter
    WebGetArg (D_CMND_PILOTWIRE_NOFROST, str_argument, sizeof(str_argument));
    if (strlen(str_argument) > 0) PilotwireSetTargetTemperature(PILOTWIRE_TARGET_NOFROST, atof(str_argument), false);

    // ecowatt level 2 temperature according to 'ecowatt2' parameter
    WebGetArg (D_CMND_PILOTWIRE_ECOWATT_N2, str_argument, sizeof(str_argument));
    if (strlen(str_argument) > 0) PilotwireSetTargetTemperature(PILOTWIRE_TARGET_ECOWATT2, atof(str_argument), false);

    // ecowatt level 3 temperature according to 'ecowatt3' parameter
    WebGetArg (D_CMND_PILOTWIRE_ECOWATT_N3, str_argument, sizeof(str_argument));
    if (strlen(str_argument) > 0) PilotwireSetTargetTemperature(PILOTWIRE_TARGET_ECOWATT3, atof(str_argument), false);

    // presence detection initial delay according to 'pdelay' parameter
    WebGetArg (D_CMND_PILOTWIRE_PRES_DELAY, str_argument, sizeof(str_argument));
    if (strlen(str_argument) > 0) pilotwire_config.pres_delay = atof(str_argument);

    // presence detection temperature drop per hour according to 'preduce' parameter
    WebGetArg (D_CMND_PILOTWIRE_PRES_REDUCE, str_argument, sizeof(str_argument));
    if (strlen(str_argument) > 0) pilotwire_config.pres_reduce = atof(str_argument);

    // save configuration
    PilotwireSaveConfig();
  }

  // get step temperature
  temperature = PILOTWIRE_TEMP_STEP;
  ext_snprintf_P (str_step, sizeof(str_step), PSTR("%1_f"), &temperature);

  // beginning of form
  WSContentStart_P (D_PILOTWIRE_CONFIGURE " " D_PILOTWIRE);
  WSContentSendStyle();
  WSContentSend_P (PSTR("<style>\n"));
  WSContentSend_P (PSTR("span.key {float:right;font-size:0.7rem;}\n"));
  WSContentSend_P (PSTR("</style>\n"));

  WSContentSend_P (PSTR("<form method='get' action='%s'>\n"), D_PILOTWIRE_PAGE_CONFIG);

  //    Device type
  // -----------------

  WSContentSend_P (PILOTWIRE_FIELDSET_START, "🔗", D_PILOTWIRE_CONNEXION);

  // command type selection
  if (pilotwire_config.direct) strcpy (str_argument, ""); else strcpy (str_argument, D_PILOTWIRE_CHECKED);
  WSContentSend_P (PSTR("<input type='radio' id='normal' name='%s' value=%d %s>%s<br>\n"), D_CMND_PILOTWIRE_TYPE, PILOTWIRE_DEVICE_PILOTWIRE, str_argument, D_PILOTWIRE);
  if (!pilotwire_config.direct) strcpy (str_argument, ""); else strcpy (str_argument, D_PILOTWIRE_CHECKED); 
  WSContentSend_P (PSTR("<input type='radio' id='direct' name='%s' value=%d %s>%s<br>\n"), D_CMND_PILOTWIRE_TYPE, PILOTWIRE_DEVICE_DIRECT, str_argument, D_PILOTWIRE_DIRECT);

  WSContentSend_P (PILOTWIRE_FIELDSET_STOP);

  //   Options
  //    - Ecowatt management
  //    - Window opened detection
  //    - Presence detection
  // -----------------------------

  WSContentSend_P (PILOTWIRE_FIELDSET_START, "♻️", D_PILOTWIRE_OPTION);

  // window opened
  if (pilotwire_config.window) strcpy(str_value, D_PILOTWIRE_CHECKED);
    else strcpy(str_value, "");
  WSContentSend_P (PSTR("<p><input type='checkbox' id='%s' name='%s' %s>\n"), D_CMND_PILOTWIRE_WINDOW, D_CMND_PILOTWIRE_WINDOW, str_value);
  WSContentSend_P (PSTR("<label for='%s'>%s %s</label></p>\n"), D_CMND_PILOTWIRE_WINDOW, D_PILOTWIRE_WINDOW, D_PILOTWIRE_DETECTION);

  // presence
  if (pilotwire_config.presence) strcpy(str_value, D_PILOTWIRE_CHECKED);
    else strcpy(str_value, "");
  WSContentSend_P (PSTR("<p><input type='checkbox' id='%s' name='%s' %s>\n"), D_CMND_PILOTWIRE_PRESENCE, D_CMND_PILOTWIRE_PRESENCE, str_value);
  WSContentSend_P (PSTR("<label for='%s'>%s %s</label></p>\n"), D_CMND_PILOTWIRE_PRESENCE, D_PILOTWIRE_PRESENCE, D_PILOTWIRE_DETECTION);

  // ecowatt
  if (pilotwire_config.ecowatt) strcpy(str_value, D_PILOTWIRE_CHECKED);
    else strcpy(str_value, "");
  WSContentSend_P (PSTR("<p><input type='checkbox' id='%s' name='%s' %s>\n"), D_CMND_PILOTWIRE_ECOWATT, D_CMND_PILOTWIRE_ECOWATT, str_value);
  WSContentSend_P (PSTR("<label for='%s'>%s %s</label></p>\n"), D_CMND_PILOTWIRE_ECOWATT, D_PILOTWIRE_SIGNAL, D_PILOTWIRE_ECOWATT);

  WSContentSend_P (PILOTWIRE_FIELDSET_STOP);

  //   Temperatures
  // -----------------

  WSContentSend_P (PILOTWIRE_FIELDSET_START, "🌡️", PSTR("Target Temperature (°C)"));

  // lowest temperature
  GetTextIndexed (str_icon, sizeof(str_icon), PILOTWIRE_TARGET_LOW, kPilotwireTargetIcon);
  GetTextIndexed (str_argument, sizeof(str_argument), PILOTWIRE_TARGET_LOW, kPilotwireTargetLabel);
  ext_snprintf_P (str_value, sizeof(str_value), PSTR("%1_f"), &pilotwire_config.arr_temp[PILOTWIRE_TARGET_LOW]);
  WSContentSend_P (PILOTWIRE_FIELD_FULL, str_icon, str_argument, D_CMND_PILOTWIRE_LOW, D_CMND_PILOTWIRE_LOW, PILOTWIRE_TEMP_LIMIT_MIN, PILOTWIRE_TEMP_LIMIT_MAX, str_step, str_value);

  // highest temperature
  GetTextIndexed (str_icon, sizeof(str_icon), PILOTWIRE_TARGET_HIGH, kPilotwireTargetIcon);
  GetTextIndexed (str_argument, sizeof(str_argument), PILOTWIRE_TARGET_HIGH, kPilotwireTargetLabel);
  ext_snprintf_P (str_value, sizeof(str_value), PSTR("%1_f"), &pilotwire_config.arr_temp[PILOTWIRE_TARGET_HIGH]);
  WSContentSend_P (PILOTWIRE_FIELD_FULL, str_icon, str_argument, D_CMND_PILOTWIRE_HIGH, D_CMND_PILOTWIRE_HIGH, PILOTWIRE_TEMP_LIMIT_MIN, PILOTWIRE_TEMP_LIMIT_MAX, str_step, str_value);

  // comfort temperature
  GetTextIndexed (str_icon, sizeof(str_icon), PILOTWIRE_TARGET_COMFORT, kPilotwireTargetIcon);
  GetTextIndexed (str_argument, sizeof(str_argument), PILOTWIRE_TARGET_COMFORT, kPilotwireTargetLabel);
  ext_snprintf_P (str_value, sizeof(str_value), PSTR("%1_f"), &pilotwire_config.arr_temp[PILOTWIRE_TARGET_COMFORT]);
  WSContentSend_P (PILOTWIRE_FIELD_FULL, str_icon, str_argument, D_CMND_PILOTWIRE_COMFORT, D_CMND_PILOTWIRE_COMFORT, PILOTWIRE_TEMP_LIMIT_MIN, PILOTWIRE_TEMP_LIMIT_MAX, str_step, str_value);

  // eco temperature
  GetTextIndexed (str_icon, sizeof(str_icon), PILOTWIRE_TARGET_ECO, kPilotwireTargetIcon);
  GetTextIndexed (str_argument, sizeof(str_argument), PILOTWIRE_TARGET_ECO, kPilotwireTargetLabel);
  ext_snprintf_P (str_value, sizeof(str_value), PSTR("%1_f"), &pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECO]);
  WSContentSend_P (PILOTWIRE_FIELD_FULL, str_icon, str_argument, D_CMND_PILOTWIRE_ECO, D_CMND_PILOTWIRE_ECO, -PILOTWIRE_TEMP_LIMIT_MAX, PILOTWIRE_TEMP_LIMIT_MAX, str_step, str_value);

  // no frost temperature
  GetTextIndexed (str_icon, sizeof(str_icon), PILOTWIRE_TARGET_NOFROST, kPilotwireTargetIcon);
  GetTextIndexed (str_argument, sizeof(str_argument), PILOTWIRE_TARGET_NOFROST, kPilotwireTargetLabel);
  ext_snprintf_P (str_value, sizeof(str_value), PSTR("%1_f"), &pilotwire_config.arr_temp[PILOTWIRE_TARGET_NOFROST]);
  WSContentSend_P (PILOTWIRE_FIELD_FULL, str_icon, str_argument, D_CMND_PILOTWIRE_NOFROST, D_CMND_PILOTWIRE_NOFROST, -PILOTWIRE_TEMP_LIMIT_MAX, PILOTWIRE_TEMP_LIMIT_MAX, str_step, str_value);

  // night mode temperature
  GetTextIndexed (str_icon, sizeof(str_icon), PILOTWIRE_TARGET_NIGHT, kPilotwireTargetIcon);
  GetTextIndexed (str_argument, sizeof(str_argument), PILOTWIRE_TARGET_NIGHT, kPilotwireTargetLabel);
  ext_snprintf_P (str_value, sizeof(str_value), PSTR("%1_f"), &pilotwire_config.arr_temp[PILOTWIRE_TARGET_NIGHT]);
  WSContentSend_P (PILOTWIRE_FIELD_FULL, str_icon, str_argument, D_CMND_PILOTWIRE_NIGHT, D_CMND_PILOTWIRE_NIGHT, -PILOTWIRE_TEMP_LIMIT_MAX, PILOTWIRE_TEMP_LIMIT_MAX, str_step, str_value);

  // ecowatt level 2 temperature
  GetTextIndexed (str_icon, sizeof(str_icon), PILOTWIRE_TARGET_ECOWATT2, kPilotwireTargetIcon);
  GetTextIndexed (str_argument, sizeof(str_argument), PILOTWIRE_TARGET_ECOWATT2, kPilotwireTargetLabel);
  ext_snprintf_P (str_value, sizeof(str_value), PSTR("%1_f"), &pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT2]);
  WSContentSend_P (PILOTWIRE_FIELD_FULL, str_icon, str_argument, D_CMND_PILOTWIRE_ECOWATT_N2, D_CMND_PILOTWIRE_ECOWATT_N2, -PILOTWIRE_TEMP_LIMIT_MAX, PILOTWIRE_TEMP_LIMIT_MAX, str_step, str_value);

  // ecowatt level 3 temperature
  GetTextIndexed (str_icon, sizeof(str_icon), PILOTWIRE_TARGET_ECOWATT3, kPilotwireTargetIcon);
  GetTextIndexed (str_argument, sizeof(str_argument), PILOTWIRE_TARGET_ECOWATT3, kPilotwireTargetLabel);
  ext_snprintf_P (str_value, sizeof(str_value), PSTR("%1_f"), &pilotwire_config.arr_temp[PILOTWIRE_TARGET_ECOWATT3]);
  WSContentSend_P (PILOTWIRE_FIELD_FULL, str_icon, str_argument, D_CMND_PILOTWIRE_ECOWATT_N3, D_CMND_PILOTWIRE_ECOWATT_N3, -PILOTWIRE_TEMP_LIMIT_MAX, PILOTWIRE_TEMP_LIMIT_MAX, str_step, str_value);

  WSContentSend_P (PILOTWIRE_FIELDSET_STOP);

  //   Presence detection
  // ----------------------
  WSContentSend_P (PILOTWIRE_FIELDSET_START, "👋", PSTR("No Presence"));

  // no presence initial delay
  ext_snprintf_P (str_value, sizeof(str_value), PSTR("%2_f"), &pilotwire_config.pres_delay);
  WSContentSend_P (PILOTWIRE_FIELD_FULL, "⌛", PSTR("Initial delay (h)"), D_CMND_PILOTWIRE_PRES_DELAY, D_CMND_PILOTWIRE_PRES_DELAY, 0, 1000, "0.05", str_value);

  // no presence temperature drop per hour
  ext_snprintf_P (str_value, sizeof(str_value), PSTR("%2_f"), &pilotwire_config.pres_reduce);
  WSContentSend_P (PILOTWIRE_FIELD_FULL, "🌡️", PSTR("Temp. drop per hour (°C)"), D_CMND_PILOTWIRE_PRES_REDUCE, D_CMND_PILOTWIRE_PRES_REDUCE, 0, 2, "0.01", str_value);

  WSContentSend_P (PILOTWIRE_FIELDSET_STOP);

  WSContentSend_P (PSTR("<br>\n"));

  // save button
  WSContentSend_P (PSTR("<p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), D_SAVE);
  WSContentSend_P (PSTR("</form>\n"));

  // configuration button
  WSContentSpaceButton(BUTTON_CONFIGURATION);

  // end of page
  WSContentStop();
}

// Get specific argument as a value with min and max
int PilotwireWebGetArgValue(const char *pstr_argument, const int value_min, const int value_max, const int value_default)
{
  int  arg_value = value_default;
  char str_argument[8];

  // check arguments
  if (pstr_argument == nullptr) return arg_value;

  // check for argument
  if (Webserver->hasArg (pstr_argument))
  {
    WebGetArg (pstr_argument, str_argument, sizeof(str_argument));
    arg_value = atoi(str_argument);
  }

  // check for min and max value
  if ((value_min > 0) && (arg_value < value_min)) arg_value = value_min;
  if ((value_max > 0) && (arg_value > value_max)) arg_value = value_max;

  return arg_value;
}

/*
// display temperature graph page
void PilotwireWebPageGraph ()
{
  bool     is_logged;
  uint16_t week;
  uint32_t timestart = millis ();
  char     str_value[64];

  // get week number
  week = (uint16_t)PilotwireWebGetArgValue ("week", 0, 52, 0);

  // beginning of page
  WSContentStart_P (D_PILOTWIRE_PAGE_GRAPH, false);

  // display graph swipe script
  SensorWebGraphSwipe ();
  WSContentSend_P (PSTR("</script>\n"));

  // set page as scalable
  WSContentSend_P (PSTR("<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=yes'/>\n"));

  // refresh every 5 mn
  WSContentSend_P (PSTR ("<meta http-equiv='refresh' content='%u'/>\n"), 300);

  // page style
  WSContentSend_P (PSTR("<style>\n"));
  WSContentSend_P (PSTR("body {color:white;background-color:#252525;font-size:2.5vh;font-family:Arial, Helvetica, sans-serif;}\n"));
  
  WSContentSend_P (PSTR("div {margin:0.25rem auto;padding:0.1rem 0px;text-align:center;}\n"));

  WSContentSend_P (PSTR("div.title {font-size:3.5vh;font-weight:bold;}\n"));
  WSContentSend_P (PSTR("div.header {font-size:2.5vh;color:yellow;margin:2vh auto;}\n"));

  WSContentSend_P (PSTR ("a {color:white;}\n"));
  WSContentSend_P (PSTR ("a:link {text-decoration:none;}\n"));

  // display graph style
  SensorWebGraphWeeklyCurveStyle ();

  WSContentSend_P (PSTR("</style>\n"));

  // page body
  WSContentSend_P (PSTR("</head>\n"));
  WSContentSend_P (PSTR("<body>\n"));

  // room name and header
  WSContentSend_P (PSTR("<div class='title'><a href='/'>%s</a></div>\n"), SettingsText(SET_DEVICENAME));
  WSContentSend_P (PSTR("<div class='header'>History</div>\n"));

  // display graph
  SensorWebGraphWeeklyCurve (week, D_PILOTWIRE_PAGE_GRAPH);

  // end of page
  WSContentStop();

  // log page serving time
  AddLog (LOG_LEVEL_DEBUG, PSTR("PIL: Graph in %ums"), millis() - timestart);
}
*/

// Get temperature attributes according to target temperature
void PilotwireWebGetTemperatureClass (const float temperature, char *pstr_class, size_t size_class)
{
  // check prameters
  if ((pstr_class == nullptr) || (size_class < 6))
    return;

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
  char str_text[16];
  char str_update[64];

  // update current temperature
  if (!isnan (pilotwire_status.temp_current)) ext_snprintf_P (str_text, sizeof (str_text), PSTR("%1_f"), &pilotwire_status.temp_current);
    else strcpy_P (str_text, PSTR("---"));
  strlcpy (str_update, str_text, sizeof (str_update));

  // update current temperature class
  strlcat (str_update, ";", sizeof(str_update));
  PilotwireWebGetTemperatureClass (pilotwire_status.temp_current, str_text, sizeof (str_text));
  strlcat (str_update, str_text, sizeof (str_update));

  // display status icon (window, flame, ...)
  strlcat (str_update, ";", sizeof(str_update));
  PilotwireWebIconStatus (str_text, sizeof(str_text), true);
  strlcat (str_update, str_text, sizeof(str_update));

  // display target thermometer icon
  temperature = PilotwireGetModeTemperature (pilotwire_config.mode);
  if (pilotwire_status.night_mode) temperature = min (temperature, PilotwireConvertTargetTemperature(PILOTWIRE_TARGET_NIGHT));
  PilotwireWebGetTemperatureClass (temperature, str_text, sizeof (str_text));
  strlcat (str_update, ";/", sizeof(str_update));
  strlcat (str_update, "therm-", sizeof(str_update));
  strlcat (str_update, str_text, sizeof(str_update));
  strlcat (str_update, ".png", sizeof(str_update));

  // send result
  Webserver->send_P (200, "text/plain", str_update);
}

// Pilot Wire heater public configuration web page
void PilotwireWebPageControl ()
{
  bool is_logged;
  float temperature, value;
  char str_class[16];
  char str_value[64];

  // check if access is allowed
  is_logged = WebAuthenticate();

  // handle mode change
  if (Webserver->hasArg (D_CMND_PILOTWIRE_MODE))
  {
    WebGetArg (D_CMND_PILOTWIRE_MODE, str_value, sizeof(str_value));
    if (strlen(str_value) > 0) PilotwireSetMode (atoi(str_value));
  }

  // if target temperature has been changed
  if (Webserver->hasArg (D_CMND_PILOTWIRE_TARGET))
  {
    WebGetArg (D_CMND_PILOTWIRE_TARGET, str_value, sizeof(str_value));
    if (strlen(str_value) > 0) PilotwireSetCurrentTarget(atof(str_value));
  }

  // get current target temperature
  temperature = PilotwireGetModeTemperature(pilotwire_config.mode);
  if (pilotwire_status.night_mode) temperature = min(temperature, PilotwireConvertTargetTemperature(PILOTWIRE_TARGET_NIGHT));

  // beginning of page
  WSContentStart_P (D_PILOTWIRE_CONTROL, false);
  WSContentSend_P (PSTR("</script>\n"));

  // page data refresh script
  WSContentSend_P (PSTR("<script type='text/javascript'>\n"));
  WSContentSend_P (PSTR("function update() {\n"));
  WSContentSend_P (PSTR("httpUpd=new XMLHttpRequest();\n"));
  WSContentSend_P (PSTR("httpUpd.onreadystatechange=function() {\n"));
  WSContentSend_P (PSTR(" if (httpUpd.responseText.length>0) {\n"));
  WSContentSend_P (PSTR("  arr_param=httpUpd.responseText.split(';');\n"));
  WSContentSend_P (PSTR("  if (arr_param[0]!='') {document.getElementById('temp').innerHTML=arr_param[0];}\n"));
  WSContentSend_P (PSTR("  if (arr_param[1]!='') {document.getElementById('actual').className=arr_param[1]+' temp';}\n"));
  WSContentSend_P (PSTR("  if (arr_param[2]!='') {document.getElementById('status').innerHTML=arr_param[2];}\n"));
  WSContentSend_P (PSTR("  if (arr_param[3]!='') {document.getElementById('therm').setAttribute('src',arr_param[3]);}\n"));
  WSContentSend_P (PSTR(" }\n"));
  WSContentSend_P (PSTR("}\n"));
  WSContentSend_P (PSTR("httpUpd.open('GET','%s',true);\n"), D_PILOTWIRE_PAGE_UPDATE);
  WSContentSend_P (PSTR("httpUpd.send();\n"));
  WSContentSend_P (PSTR("}\n"));
  WSContentSend_P (PSTR("setInterval(function() {update();},2000);\n"));
  WSContentSend_P (PSTR("</script>\n"));

  // page style
  WSContentSend_P (PSTR("<style>\n"));

  WSContentSend_P (PSTR("body {background-color:%s;font-family:Arial, Helvetica, sans-serif;}\n"), PILOTWIRE_COLOR_BACKGROUND);
  WSContentSend_P (PSTR("div {color:white;font-size:1.5rem;text-align:center;vertical-align:middle;margin:10px auto;}\n"));
  WSContentSend_P (PSTR("div a {color:white;text-decoration:none;}\n"));

  WSContentSend_P (PSTR("div.inline {display:inline-block;}\n"));
  WSContentSend_P (PSTR("div.title {font-size:5vh;font-weight:bold;}\n"));

  WSContentSend_P (PSTR("div.temp {font-size:5vh;font-weight:bold;}\n"));

  WSContentSend_P (PSTR(".low {color:%s;}\n"), PILOTWIRE_COLOR_LOW);
  WSContentSend_P (PSTR(".mid {color:%s;}\n"), PILOTWIRE_COLOR_MEDIUM);
  WSContentSend_P (PSTR(".high {color:%s;}\n"), PILOTWIRE_COLOR_HIGH);

  WSContentSend_P (PSTR("div.pix img {height:25vh;border-radius:12px;}\n"));
  WSContentSend_P (PSTR("div.status {font-size:6vh;margin-top:2vh;}\n"));

  WSContentSend_P (PSTR("div.therm {font-size:5vh;}\n"));
  WSContentSend_P (PSTR("select {background:none;color:#ddd;font-size:4vh;border:none;}\n"));

  WSContentSend_P (PSTR("div.adjust {font-size:4vh;margin-left:5px;}\n"));
  WSContentSend_P (PSTR("div.adjust div.item {width:30px;color:#ddd;border:1px #444 solid;border-radius:8px;}\n"));

  WSContentSend_P (PSTR("div.mode {padding:2px;margin-bottom:5vh;border:1px #666 solid;border-radius:8px;}\n"));
  WSContentSend_P (PSTR("div.mode button {padding:2px 12px;font-size:5vh;color:white;background:none;border:none;border-radius:6px;}\n"));
  WSContentSend_P (PSTR("div.mode button:disabled {background:#666;}\n"));
  WSContentSend_P (PSTR("div.mode button:hover {background:#444;}\n"));

  WSContentSend_P (PSTR("</style>\n"));
  WSContentSend_P (PSTR("</head>\n"));

  // page body
  WSContentSend_P (PSTR("<body>\n"));
  WSContentSend_P (PSTR("<form method='get' action='/control'>\n"));

  // room name
  WSContentSend_P (PSTR("<div class='title'>%s</div>\n"), SettingsText(SET_DEVICENAME));

  // actual temperature
  strcpy_P (str_value, PSTR("---"));
  if (!isnan (pilotwire_status.temp_current)) ext_snprintf_P (str_value, sizeof (str_value), PSTR("%1_f"), &pilotwire_status.temp_current);
  PilotwireWebGetTemperatureClass (pilotwire_status.temp_current, str_class, sizeof (str_class));
  WSContentSend_P (PSTR("<div id='actual' class='%s temp'><span id='temp'>%s</span><small> °C</small></div>\n"), str_class, str_value);

  // room picture
  if (is_logged) strcpy (str_value, "");
    else strcpy (str_value, D_PILOTWIRE_PAGE_CONTROL);
  WSContentSend_P (PSTR("<div class='section pix'><a href='/%s'>"), str_value);

  // display icon
  strlcpy (str_value, SettingsText (SET_PILOTWIRE_ICON_URL), sizeof (str_value));
  if (strlen (str_value) == 0) strcpy(str_value, PILOTWIRE_ICON_DEFAULT);
  WSContentSend_P (PSTR("<img src='%s'></a></div>\n"), str_value);

  // status icon
  PilotwireWebIconStatus (str_value, sizeof(str_value), true);
  WSContentSend_P (PSTR("<div class='section status' id='status'>%s</div>\n"), str_value);

  // section : target
  WSContentSend_P (PSTR("<div class='section target'>\n"));

  // thermometer
  if (pilotwire_config.mode != PILOTWIRE_MODE_OFF) strcpy(str_class, "🌡");
    else strcpy (str_class, "&nbsp;");
  WSContentSend_P (PSTR("<div class='inline therm'>%s</div>\n"), str_class);

  // if heater in thermostat mode, display selection
  if ((pilotwire_config.mode == PILOTWIRE_MODE_COMFORT) && !pilotwire_status.night_mode)
  {
    WSContentSend_P (PSTR("<div class='inline adjust'>\n"));

    // button -0.5
    value = temperature - 0.5;
    ext_snprintf_P (str_value, sizeof(str_value), PSTR("%1_f"), &value);
    WSContentSend_P (PSTR("<a href='/%s?%s=%s'><div class='inline item'>%s</div></a>\n"), D_PILOTWIRE_PAGE_CONTROL, D_CMND_PILOTWIRE_TARGET, str_value, "-");

    // target selection
    WSContentSend_P (PSTR("<select name='%s' id='%s' onchange='this.form.submit();'>\n"), D_CMND_PILOTWIRE_TARGET, D_CMND_PILOTWIRE_TARGET);
    for (value = pilotwire_config.arr_temp[PILOTWIRE_TARGET_LOW]; value <= pilotwire_config.arr_temp[PILOTWIRE_TARGET_HIGH]; value += 0.5)
    {
      ext_snprintf_P (str_value, sizeof (str_value), PSTR("%01_f"), &value);
      if (value == temperature) strcpy (str_class, "selected");
        else strcpy (str_class, "");
      WSContentSend_P (PSTR("<option value='%s' %s>%s °C</option>\n"), str_value, str_class, str_value);
    }
    WSContentSend_P (PSTR("</select>\n"));

    // button +0.5
    value = temperature + 0.5;
    ext_snprintf_P (str_value, sizeof(str_value), PSTR("%1_f"), &value);
    WSContentSend_P (PSTR("<a href='/%s?%s=%s'><div class='inline item'>%s</div></a>\n"), D_PILOTWIRE_PAGE_CONTROL, D_CMND_PILOTWIRE_TARGET, str_value, "+");

    WSContentSend_P (PSTR("</div>\n")); // inline adjust
  }

  // else, if heater is not in forced mode
  else if (pilotwire_config.mode != PILOTWIRE_MODE_OFF)
  {
    ext_snprintf_P (str_value, sizeof(str_value), PSTR("%1_f"), &temperature);
    WSContentSend_P (PSTR("<div class='inline adjust %s'>%s<small> °C</small></div>\n"), str_class, str_value);
  }

  WSContentSend_P (PSTR("</div>\n")); // section target

  // section : mode
  WSContentSend_P (PSTR("<div class='section'>\n"));
  WSContentSend_P (PSTR("<div class='inline mode'>\n"));

  // OFF
  if (pilotwire_config.mode == PILOTWIRE_MODE_OFF) strcpy (str_value, "disabled");
    else strcpy (str_value, "");
  WSContentSend_P (PSTR("<button name='mode' value=%u title='%s' %s>❄</button>\n"), PILOTWIRE_MODE_OFF, D_PILOTWIRE_OFF, str_value);

  // ECO
  if (pilotwire_config.mode == PILOTWIRE_MODE_ECO) strcpy(str_value, "disabled");
    else strcpy (str_value, "");
  WSContentSend_P (PSTR("<button name='mode' value=%u title='%s' %s>🌙</button>\n"), PILOTWIRE_MODE_ECO, D_PILOTWIRE_ECO, str_value);

  // NIGHT
  if ((pilotwire_config.mode == PILOTWIRE_MODE_COMFORT) && pilotwire_status.night_mode) strcpy(str_value, "disabled");
    else strcpy (str_value, "");
  WSContentSend_P (PSTR("<button name='mode' value=%u title='%s' %s>💤</button>\n"), PILOTWIRE_MODE_NIGHT, D_PILOTWIRE_NIGHT, str_value);

  // COMFORT
  if ((pilotwire_config.mode == PILOTWIRE_MODE_COMFORT) && !pilotwire_status.night_mode) strcpy(str_value, "disabled");
    else strcpy (str_value, "");
  WSContentSend_P (PSTR("<button name='mode' value=%u title='%s' %s>🔆</button>\n"), PILOTWIRE_MODE_COMFORT, D_PILOTWIRE_COMFORT, str_value);

  WSContentSend_P (PSTR("</div>\n")); // inline mode

  // end of form
  WSContentSend_P (PSTR("</form>\n"));

  // end of page
  WSContentStop ();
}

#endif // USE_WEBSERVER

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xsns126 (uint32_t function)
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
    PilotwireEverySecond ();
    break;
  case FUNC_COMMAND:
    result = DecodeCommand (kPilotwireCommand, PilotwireCommand);
    break;
  case FUNC_JSON_APPEND:
    PilotwireShowJSON (false);
    break;

#ifdef USE_WEBSERVER
  case FUNC_WEB_ADD_HANDLER:
    Webserver->on (PILOTWIRE_ICON_DEFAULT, PilotwireWebIconDefault);
    Webserver->on ("/" D_PILOTWIRE_PAGE_STATUS, PilotwireWebPageSwitchStatus);
    Webserver->on ("/" D_PILOTWIRE_PAGE_CONFIG, PilotwireWebPageConfigure);
    Webserver->on ("/" D_PILOTWIRE_PAGE_CONTROL, PilotwireWebPageControl);
    Webserver->on ("/" D_PILOTWIRE_PAGE_UPDATE, PilotwireWebUpdate);
//    Webserver->on ("/" D_PILOTWIRE_PAGE_GRAPH, PilotwireWebPageGraph);
    break;
  case FUNC_WEB_SENSOR:
    PilotwireWebSensor ();
    break;
  case FUNC_WEB_ADD_MAIN_BUTTON:
    WSContentSend_P (PSTR("<p><form action='%s' method='get'><button>Pilotwire Control</button></form></p>\n"), D_PILOTWIRE_PAGE_CONTROL);
//    WSContentSend_P (PSTR("<p><form action='%s' method='get'><button>Temperature</button></form></p>\n"), D_PILOTWIRE_PAGE_GRAPH);
    PilotwireWebMainSelectMode ();
    break;
  case FUNC_WEB_ADD_BUTTON:
    WSContentSend_P (PSTR("<p><form action='%s' method='get'><button>Configure %s</button></form></p>\n"), D_PILOTWIRE_PAGE_CONFIG, D_PILOTWIRE);
    break;

#endif // USE_Webserver
  }
  return result;
}

#endif // USE_PILOTWIRE
