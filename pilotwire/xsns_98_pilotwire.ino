/*
  xsns_98_pilotwire.ino - French Pilot Wire (Fil Pilote) support (~27.5 kb)
  for Sonoff Basic or Sonoff Dual R2

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

  Settings are stored using weighting scale parameters :
    - Settings.weight_reference             = Fil pilote mode
    - Settings.weight_max                   = Target temperature  (x10 -> 192 = 19.2°C)
    - Settings.weight_calibration           = Temperature correction (0 = -5°C, 50 = 0°C, 100 = +5°C)
    - Settings.weight_item                  = Minimum temperature (x10 -> 125 = 12.5°C)
    - Settings.energy_frequency_calibration = Maximum temperature (x10 -> 240 = 24.0°C)
    - Settings.energy_voltage_calibration   = Outside mode temperature (x10 -> 25 = 2.5°C)
    - Settings.energy_current_calibration   = Device type (pilotewire or direct)

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

#define XSNS_98                         98

/*************************************************\
 *               Constants
\*************************************************/

#define D_PAGE_PILOTWIRE_CONFIG         "config"
#define D_PAGE_PILOTWIRE_SWITCH         "mode"
#define D_PAGE_PILOTWIRE_CONTROL        "control"
#define D_PAGE_PILOTWIRE_DATA           "data.json"
#define D_PAGE_PILOTWIRE_BASE_SVG       "base.svg"
#define D_PAGE_PILOTWIRE_DATA_SVG       "data.svg"

#define D_CMND_PILOTWIRE_ON             "on"
#define D_CMND_PILOTWIRE_OFF            "off"
#define D_CMND_PILOTWIRE_MINUSMINUS     "m2"
#define D_CMND_PILOTWIRE_MINUS          "m1"
#define D_CMND_PILOTWIRE_PLUS           "p1"
#define D_CMND_PILOTWIRE_PLUSPLUS       "p2"
#define D_CMND_PILOTWIRE_SET            "set"
#define D_CMND_PILOTWIRE_MODE           "mode"
#define D_CMND_PILOTWIRE_LANG           "lang"
#define D_CMND_PILOTWIRE_OFFLOAD        "offload"
#define D_CMND_PILOTWIRE_MIN            "min"
#define D_CMND_PILOTWIRE_MAX            "max"
#define D_CMND_PILOTWIRE_TARGET         "target"
#define D_CMND_PILOTWIRE_OUTSIDE        "outside"
#define D_CMND_PILOTWIRE_DRIFT          "drift"
#define D_CMND_PILOTWIRE_PULLUP         "pullup"
#define D_CMND_PILOTWIRE_DEVICE         "device"

#define D_JSON_PILOTWIRE                "Pilotwire"
#define D_JSON_PILOTWIRE_MODE           "Mode"
#define D_JSON_PILOTWIRE_OUTSIDE        "Outside"
#define D_JSON_PILOTWIRE_LABEL          "Label"
#define D_JSON_PILOTWIRE_MIN            "Min"
#define D_JSON_PILOTWIRE_MAX            "Max"
#define D_JSON_PILOTWIRE_TARGET         "Target"
#define D_JSON_PILOTWIRE_DRIFT          "Drift"
#define D_JSON_PILOTWIRE_TEMPERATURE    "Temperature"
#define D_JSON_PILOTWIRE_RELAY          "Relay"
#define D_JSON_PILOTWIRE_STATE          "State"

#define D_PILOTWIRE                     "Pilotwire"
#define D_PILOTWIRE_HEATER              "Heater"
#define D_PILOTWIRE_STATE               "State"
#define D_PILOTWIRE_CONFIGURE           "Configure"
#define D_PILOTWIRE_CONNEXION           "Connexion"
#define D_PILOTWIRE_MODE                "Mode"
#define D_PILOTWIRE_NORMAL              "Normal"
#define D_PILOTWIRE_OUTSIDE             "Outside"
#define D_PILOTWIRE_DISABLED            "Disabled"
#define D_PILOTWIRE_OFF                 "Off"
#define D_PILOTWIRE_COMFORT             "Comfort"
#define D_PILOTWIRE_ECO                 "Economy"
#define D_PILOTWIRE_FROST               "No Frost"
#define D_PILOTWIRE_THERMOSTAT          "Thermostat"
#define D_PILOTWIRE_SENSOR              "Sensor"
#define D_PILOTWIRE_TARGET              "Target"
#define D_PILOTWIRE_DROPDOWN            "Night Mode"
#define D_PILOTWIRE_DRIFT               "Sensor correction"
#define D_PILOTWIRE_SETTING             "Public settings"
#define D_PILOTWIRE_MIN                 "Minimum"
#define D_PILOTWIRE_MAX                 "Maximum"
#define D_PILOTWIRE_NOSENSOR            "No temperature sensor available"
#define D_PILOTWIRE_CONTROL             "Control"
#define D_PILOTWIRE_TEMPERATURE         "Temperature"
#define D_PILOTWIRE_RANGE               "Range"
#define D_PILOTWIRE_LOCAL               "Local"
#define D_PILOTWIRE_REMOTE              "Remote"
#define D_PILOTWIRE_DIRECT              "Direct supply"
#define D_PILOTWIRE_PULLUP              "Enable DS18B20 internal pullup"
#define D_PILOTWIRE_CHECKED             "checked"

#define PILOTWIRE_TEMP_MIN              2
#define PILOTWIRE_TEMP_MAX              50
#define PILOTWIRE_TEMP_DRIFT            5
#define PILOTWIRE_TEMP_DROP             10
#define PILOTWIRE_TEMP_DEFAULT_TARGET   18
#define PILOTWIRE_TEMP_DEFAULT_MIN      15
#define PILOTWIRE_TEMP_DEFAULT_MAX      25
#define PILOTWIRE_TEMP_DEFAULT_DRIFT    0
#define PILOTWIRE_TEMP_DEFAULT_DROP     2
#define PILOTWIRE_TEMP_STEP             0.5
#define PILOTWIRE_TEMP_DRIFT_STEP       0.1

#define PILOTWIRE_TEMP_THRESHOLD        0.25
        
#define PILOTWIRE_GRAPH_SAMPLE          800
#define PILOTWIRE_GRAPH_REFRESH         108       // 24 hours display with collect 800 data every 108 sec
#define PILOTWIRE_GRAPH_WIDTH           800
#define PILOTWIRE_GRAPH_HEIGHT          500
#define PILOTWIRE_GRAPH_PERCENT_START   8      
#define PILOTWIRE_GRAPH_PERCENT_STOP    92

// constant chains
const char D_CONF_FIELDSET_START[] PROGMEM = "<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>\n";
const char D_CONF_FIELDSET_STOP[]  PROGMEM = "</fieldset></p>\n";
const char D_CONF_MODE_SELECT[]    PROGMEM = "<input type='radio' name='%s' id='%d' value='%d' %s>%s<br>\n";
const char D_CONF_TEMPERATURE[]    PROGMEM = "<span class='half'>%s (°%c)<span class='key'>%s</span><br><input type='number' name='%s' min='%d' max='%d' step='%s' value='%s'></span>\n";
const char D_GRAPH_SEPARATION[]    PROGMEM = "<line class='dash' x1='%d%%' y1='%d%%' x2='%d%%' y2='%d%%' />\n";
const char D_GRAPH_TEMPERATURE[]   PROGMEM = "<text class='temperature' x='%d%%' y='%d%%'>%s</text>\n";

// enumerations
enum PilotwireDevices { PILOTWIRE_DEVICE_NORMAL, PILOTWIRE_DEVICE_DIRECT };
enum PilotwireModes { PILOTWIRE_DISABLED, PILOTWIRE_OFF, PILOTWIRE_COMFORT, PILOTWIRE_ECO, PILOTWIRE_FROST, PILOTWIRE_THERMOSTAT, PILOTWIRE_OFFLOAD };
enum PilotwireSources { PILOTWIRE_SOURCE_NONE, PILOTWIRE_SOURCE_LOCAL, PILOTWIRE_SOURCE_REMOTE };

// fil pilote commands
enum PilotwireCommands { CMND_PILOTWIRE_MODE, CMND_PILOTWIRE_MIN, CMND_PILOTWIRE_MAX, CMND_PILOTWIRE_TARGET, CMND_PILOTWIRE_OUTSIDE, CMND_PILOTWIRE_DRIFT};
const char kPilotWireCommands[] PROGMEM = D_CMND_PILOTWIRE_MODE "|" D_CMND_PILOTWIRE_MIN "|" D_CMND_PILOTWIRE_MAX "|" D_CMND_PILOTWIRE_TARGET "|" D_CMND_PILOTWIRE_OUTSIDE "|" D_CMND_PILOTWIRE_DRIFT;

/****************************************\
 *               Icons
 * 
 *      xxd -i -c 256 icon.png
\****************************************/

// icon : off
unsigned char pilotwire_off_png[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x40, 0x02, 0x03, 0x00, 0x00, 0x00, 0xd7, 0x07, 0x99, 0x4d, 0x00, 0x00, 0x00, 0x09, 0x50, 0x4c, 0x54, 0x45, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0xf2, 0x46, 0xe4, 0xb1, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x00, 0x01, 0x62, 0x4b, 0x47, 0x44, 0x00, 0x88, 0x05, 0x1d, 0x48, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x2e, 0x23, 0x00, 0x00, 0x2e, 0x23, 0x01, 0x78, 0xa5, 0x3f, 0x76, 0x00, 0x00, 0x00, 0x07, 0x74, 0x49, 0x4d, 0x45, 0x07, 0xe4, 0x0a, 0x12, 0x10, 0x30, 0x2e, 0xee, 0x17, 0xec, 0xcd, 0x00, 0x00, 0x00, 0xc5, 0x49, 0x44, 0x41, 0x54, 0x38, 0xcb, 0xcd, 0x93, 0x4b, 0x0e, 0xc3, 0x30, 0x08, 0x44, 0x2d, 0x96, 0x1c, 0xc5, 0xa7, 0xf4, 0x51, 0xba, 0xb4, 0x38, 0x65, 0x45, 0x66, 0x00, 0xd3, 0xb4, 0x52, 0x97, 0x61, 0x11, 0x5b, 0x0f, 0x0f, 0x36, 0x9f, 0x8c, 0x01, 0xd3, 0xd7, 0xe8, 0x36, 0xf7, 0x07, 0x30, 0x7b, 0x20, 0xb0, 0x05, 0x20, 0x7c, 0xad, 0xd8, 0x06, 0x98, 0xee, 0xf1, 0xbc, 0x0a, 0x20, 0xc3, 0x59, 0x60, 0x7f, 0x07, 0x56, 0xc0, 0x10, 0xd3, 0xa5, 0x04, 0xab, 0x01, 0x05, 0xc0, 0x72, 0xbd, 0xe3, 0x72, 0x51, 0xc9, 0xcf, 0x8e, 0x4b, 0x00, 0x62, 0xfb, 0x0a, 0xa0, 0x3c, 0x77, 0x03, 0x2b, 0x80, 0x30, 0xf4, 0x09, 0x16, 0x68, 0xd4, 0xe3, 0x07, 0xd8, 0xd5, 0x4a, 0xaf, 0x88, 0x02, 0xa0, 0xd9, 0x5e, 0x11, 0x02, 0xb6, 0xfc, 0x41, 0x40, 0x3a, 0xc8, 0xe4, 0x62, 0x06, 0xfe, 0x01, 0x2c, 0x21, 0x07, 0xa3, 0x6a,
  0xda, 0x40, 0xce, 0xbd, 0x1e, 0xdd, 0xa9, 0xc1, 0xb8, 0x03, 0xad, 0x6b, 0xa0, 0xd6, 0x8c, 0xca, 0xee, 0x4b, 0x46, 0x0d, 0x97, 0x85, 0x26, 0x36, 0x46, 0x87, 0x04, 0x98, 0xc6, 0x5e, 0x71, 0x75, 0xa9, 0x47, 0xe1, 0xc2, 0xa3, 0xb4, 0x95, 0xc1, 0x68, 0xf9, 0x40, 0xda, 0xae, 0x24, 0x9b, 0xa2, 0x34, 0xe3, 0x48, 0xdb, 0xec, 0x28, 0x43, 0x1c, 0x39, 0x7e, 0x32, 0x69, 0x11, 0x28, 0xa2, 0xe0, 0x0d, 0x55, 0xd7, 0xe0, 0x7f, 0x12, 0xf5, 0xa9, 0x53, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
unsigned int pilotwire_off_len = 341;

// icon : comfort
unsigned char pilotwire_comfort_png[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x40, 0x02, 0x03, 0x00, 0x00, 0x00, 0xd7, 0x07, 0x99, 0x4d, 0x00, 0x00, 0x00, 0x09, 0x50, 0x4c, 0x54, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xe5, 0x00, 0x1e, 0xf2, 0xda, 0x84, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x00, 0x01, 0x62, 0x4b, 0x47, 0x44, 0x00, 0x88, 0x05, 0x1d, 0x48, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x2e, 0x23, 0x00, 0x00, 0x2e, 0x23, 0x01, 0x78, 0xa5, 0x3f, 0x76, 0x00, 0x00, 0x00, 0x07, 0x74, 0x49, 0x4d, 0x45, 0x07, 0xe4, 0x0a, 0x12, 0x10, 0x30, 0x24, 0x0e, 0xc2, 0x05, 0xd3, 0x00, 0x00, 0x00, 0xd2, 0x49, 0x44, 0x41, 0x54, 0x38, 0xcb, 0xb5, 0x94, 0x41, 0x12, 0xc3, 0x20, 0x08, 0x45, 0x1d, 0x96, 0x1c, 0x85, 0x53, 0x7a, 0x14, 0x97, 0x1d, 0x4e, 0xd9, 0x00, 0xc1, 0x10, 0x7e, 0x3a, 0xdd, 0xb4, 0x59, 0x24, 0xfa, 0x88, 0xf0, 0x41, 0x74, 0x8c, 0x78, 0x78, 0x8e, 0xfb, 0x23, 0xab, 0x01, 0x7d, 0xfd, 0x14, 0x90, 0x6e, 0x10, 0x43, 0xd2, 0x99, 0x80, 0x1d, 0xb0, 0xae, 0x04, 0xe2, 0x36, 0xf2, 0x71, 0xbc, 0xce, 0x25, 0x7a, 0x2a, 0x8d, 0x91, 0xf1, 0x19, 0xb9, 0xf0, 0x19, 0x2a, 0x56, 0x6e, 0x6f, 0xd7, 0x77, 0x5b, 0xe8, 0x02, 0xa9, 0x72, 0x46, 0x14, 0x4e, 0xb5, 0x62, 0xc6, 0xe3, 0x37, 0x5a, 0x45, 0xbe, 0xaa, 0xd6, 0xd4, 0xd8, 0x40, 0x2d, 0xa2, 0x18, 0xa8, 0x45, 0xb4, 0x79, 0xad, 0x88, 0xbb, 0xa8, 0x4e, 0xd8, 0x26, 0xd5, 0x89, 0x6b, 0x95, 0x02, 0x7c, 0xcc, 0xc5, 0xab, 0x6b, 0xa6, 0x02, 0x22, 0x40,
  0x09, 0x73, 0x03, 0x87, 0xa6, 0x0d, 0xc8, 0xd4, 0x39, 0xf0, 0xd5, 0x12, 0xc0, 0x92, 0x48, 0xc0, 0x26, 0x17, 0x00, 0x2c, 0x01, 0xa7, 0xdf, 0x75, 0x3c, 0x4a, 0x87, 0xe4, 0x20, 0x7d, 0x2b, 0x10, 0xd5, 0x02, 0x41, 0x09, 0xa1, 0xc8, 0xb8, 0x0d, 0xb0, 0x51, 0x6d, 0x2b, 0x61, 0xb3, 0x7b, 0x3b, 0x40, 0xc3, 0x40, 0x4b, 0x49, 0x82, 0xb4, 0xf4, 0xb6, 0x84, 0xc6, 0xe5, 0xde, 0xda, 0x0f, 0xcd, 0xdf, 0x8e, 0x07, 0x1c, 0xa0, 0xbf, 0x1c, 0xd3, 0x0f, 0x97, 0xc1, 0xbe, 0x2e, 0xde, 0x62, 0xf9, 0xbc, 0x07, 0x67, 0xb5, 0x77, 0x75, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
unsigned int pilotwire_comfort_len = 354;

// icon : economy
unsigned char pilotwire_eco_png[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x40, 0x02, 0x03, 0x00, 0x00, 0x00, 0xd7, 0x07, 0x99, 0x4d, 0x00, 0x00, 0x00, 0x09, 0x50, 0x4c, 0x54, 0x45, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0xff, 0xb5, 0x00, 0x68, 0x54, 0x18, 0x5b, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x00, 0x01, 0x62, 0x4b, 0x47, 0x44, 0x00, 0x88, 0x05, 0x1d, 0x48, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x2e, 0x23, 0x00, 0x00, 0x2e, 0x23, 0x01, 0x78, 0xa5, 0x3f, 0x76, 0x00, 0x00, 0x00, 0x07, 0x74, 0x49, 0x4d, 0x45, 0x07, 0xe4, 0x0a, 0x12, 0x10, 0x30, 0x04, 0x35, 0xac, 0x25, 0x1b, 0x00, 0x00, 0x00, 0x9a, 0x49, 0x44, 0x41, 0x54, 0x38, 0xcb, 0x9d, 0xd3, 0x41, 0x0e, 0x84, 0x30, 0x08, 0x05, 0x50, 0xc3, 0x92, 0x53, 0xcc, 0x9a, 0x53, 0xf6, 0x28, 0x2c, 0x09, 0xa7, 0x34, 0x63, 0xd4, 0xf2, 0x7f, 0xc6, 0x62, 0x86, 0x95, 0xbe, 0x14, 0x68, 0xa9, 0x6e, 0xdb, 0xab, 0x50, 0x86, 0xa0, 0x77, 0x61, 0x50, 0x27, 0x48, 0x02, 0x61, 0x50, 0x06, 0xcb, 0xd1, 0x40, 0x12, 0x08, 0x83, 0xb6, 0x60, 0x2d, 0x64, 0x07, 0xf2, 0x0a, 0x9c, 0xb6, 0xf1, 0x0f, 0x44, 0x0b, 0xb9, 0x04, 0xfb, 0x09, 0x83, 0xc1, 0x57, 0x70, 0x14, 0x0d, 0x86, 0xec, 0x61, 0x30, 0x38, 0x43, 0xe0, 0xc4, 0xa0, 0xc8, 0x09, 0xce, 0x10, 0x70, 0x2f, 0xd8, 0x27, 0x39, 0xc7, 0x60, 0xc9, 0x67, 0x42, 0x5c, 0x9f, 0xac, 0xe6, 0x15, 0xdf, 0xe7, 0x51, 0xe1, 0x4c, 0x14, 0x80, 0xd2, 0x66, 0x16, 0xaa, 0xe0, 0xb5, 0xef, 0x0d, 0x8a, 0x35, 0xb1, 0x2a, 0x6c, 0x7e,
  0x1e, 0xd1, 0x30, 0xa3, 0xe6, 0xd0, 0x81, 0xe7, 0x50, 0xf4, 0x61, 0x26, 0xb1, 0xb8, 0x9c, 0xa3, 0x91, 0xaf, 0x7f, 0xf7, 0x3b, 0x76, 0x7a, 0xdb, 0xe6, 0x0d, 0xa2, 0xe2, 0x14, 0xb4, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
unsigned int pilotwire_eco_len = 298;

// icon : no frost
unsigned char pilotwire_frost_png[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x40, 0x08, 0x06, 0x00, 0x00, 0x00, 0xaa, 0x69, 0x71, 0xde, 0x00, 0x00, 0x00, 0x06, 0x62, 0x4b, 0x47, 0x44, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0xa0, 0xbd, 0xa7, 0x93, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0b, 0x13, 0x00, 0x00, 0x0b, 0x13, 0x01, 0x00, 0x9a, 0x9c, 0x18, 0x00, 0x00, 0x00, 0x07, 0x74, 0x49, 0x4d, 0x45, 0x07, 0xe4, 0x0a, 0x12, 0x10, 0x2e, 0x39, 0xb9, 0x85, 0x56, 0xd5, 0x00, 0x00, 0x02, 0x11, 0x49, 0x44, 0x41, 0x54, 0x78, 0xda, 0xed, 0x5b, 0x41, 0x92, 0xc4, 0x20, 0x08, 0x4c, 0x5b, 0xfe, 0x67, 0xff, 0xbd, 0xbf, 0xdb, 0x17, 0xf4, 0x5e, 0xb7, 0xa6, 0x36, 0x06, 0x10, 0x84, 0x24, 0x7a, 0x9a, 0xa9, 0x89, 0x4a, 0xb7, 0x08, 0x0d, 0xa9, 0xc1, 0xd7, 0xf7, 0xcf, 0xf1, 0xe6, 0xd1, 0x13, 0xf7, 0xe6, 0x9f, 0xcf, 0xc8, 0x32, 0xa2, 0x1d, 0x2f, 0x1f, 0x9b, 0x80, 0x4d, 0xc0, 0x26, 0xe0, 0xdd, 0x23, 0x2a, 0x0b, 0xf0, 0xe3, 0x3b, 0x92, 0xd6, 0x28, 0xe3, 0x01, 0x0c, 0x7e, 0xbe, 0x1c, 0x01, 0x98, 0x00, 0x45, 0xe1, 0x7a, 0xe5, 0x3d, 0xe0, 0x8c, 0x04, 0x0e, 0x80, 0x2f, 0x05, 0x3f, 0x43, 0x00, 0x27, 0x48, 0xf8, 0x6f, 0x3e, 0x95, 0xf3, 0xdd, 0xae, 0x4c, 0x9b, 0x00, 0xaf, 0x21, 0x01, 0x06, 0xef, 0x41, 0x90, 0x3d, 0x69, 0x69, 0x10, 0xce, 0xcf, 0x95, 0x88, 0x01, 0x74, 0x26, 0x01, 0xc1, 0xfb, 0x97, 0x10, 0x42, 0xa8, 0x70, 0xf2, 0xff, 0x09, 0x21, 0x2e, 0x34, 0x08, 0x8b, 0xcb, 0xe1, 0x53, 0x6c, 0x5d, 0x30, 0x29, 0xad, 0x56, 0x5f, 0x21, 0xc0, 0x9a, 0x30, 0x65, 0x8d, 0x4e,
  0x8b, 0x45, 0xc0, 0x42, 0xa8, 0x37, 0x30, 0x8a, 0x01, 0xb8, 0x60, 0x93, 0xc5, 0x4f, 0xf7, 0xca, 0x46, 0x48, 0x8a, 0x21, 0x5c, 0x9c, 0x2c, 0x6f, 0xe8, 0xee, 0xb0, 0x54, 0x83, 0x28, 0x0e, 0xd8, 0x45, 0x53, 0x74, 0xc5, 0x22, 0x7c, 0x12, 0x70, 0xef, 0x7e, 0xc0, 0xca, 0x8c, 0xe1, 0x7a, 0x10, 0xdd, 0x79, 0xb3, 0x48, 0x22, 0x68, 0x7c, 0x16, 0x5a, 0x02, 0xa8, 0x74, 0x35, 0x06, 0x13, 0x41, 0xc3, 0xfe, 0x62, 0x32, 0xba, 0x01, 0x3c, 0x14, 0x31, 0x62, 0x86, 0x08, 0x69, 0x54, 0x87, 0xc2, 0x7e, 0x7e, 0xda, 0xd2, 0x1d, 0x83, 0x0a, 0x04, 0xa9, 0x13, 0x0e, 0xae, 0x0e, 0x85, 0x18, 0x52, 0xc7, 0x00, 0x4c, 0x9e, 0x9a, 0x54, 0x43, 0xc0, 0x19, 0xb8, 0x94, 0x0c, 0x95, 0x10, 0xf2, 0x48, 0x43, 0x52, 0x31, 0x45, 0x47, 0xe0, 0x6e, 0x42, 0xe8, 0x58, 0x40, 0x44, 0x7a, 0x63, 0x64, 0xd5, 0xdb, 0x61, 0x0d, 0x11, 0xb7, 0xea, 0x08, 0xdd, 0x7e, 0xac, 0xf2, 0x00, 0x8b, 0x88, 0x79, 0xc4, 0x15, 0xa0, 0xc3, 0xdc, 0xe5, 0xef, 0x05, 0x3c, 0xea, 0x7e, 0x6d, 0x5d, 0x1e, 0xdd, 0x87, 0x38, 0x5d, 0x63, 0xa4, 0x04, 0x2d, 0x3d, 0x3b, 0x0f, 0x11, 0x43, 0x27, 0x8f, 0xa0, 0xa7, 0x12, 0x94, 0x90, 0xe1, 0x95, 0xcb, 0x25, 0x44, 0xc0, 0xeb, 0xca, 0x75, 0x87, 0xe2, 0x22, 0x4a, 0xc4, 0x48, 0x6b, 0x0c, 0x1a, 0xd6, 0x14, 0x2b, 0x41, 0x1a, 0x5d, 0xde, 0x33, 0x70, 0x49, 0x88, 0x30, 0xeb, 0x8a, 0xae, 0x98, 0x9c, 0x2d, 0x62, 0x42, 0xc4, 0x54, 0x77, 0x36, 0xae, 0x9a, 0xaa, 0x74, 0x21, 0xe0, 0x8e, 0x4d, 0x51, 0x71, 0xc6, 0xe8, 0x0f, 0x03, 0xae, 0x26, 0xa2, 0x05, 0xa7, 0xb4, 0x23, 0xe1, 0x6a, 0xa8, 0x84, 0x50, 0x53, 0xa8, 0x2e, 0x14, 0x03, 0x0f, 0x83, 0x8d, 0x1c, 0x11, 0x20,
  0x05, 0xce, 0xa2, 0x1e, 0x41, 0x21, 0x11, 0x94, 0xc6, 0x00, 0x3c, 0x20, 0x06, 0x5c, 0x66, 0x8c, 0x9e, 0x04, 0x98, 0x0a, 0x79, 0x1b, 0x1a, 0x1f, 0x5a, 0xb2, 0xab, 0xa6, 0x67, 0x9d, 0x16, 0xc5, 0xac, 0x11, 0x24, 0x83, 0xf7, 0x4f, 0xf5, 0x00, 0x3a, 0x3f, 0x97, 0xe6, 0x01, 0x30, 0xd4, 0xe5, 0x96, 0x77, 0x8c, 0x0c, 0xb2, 0xc7, 0xc5, 0x03, 0x66, 0x1b, 0x24, 0xd2, 0x8e, 0x10, 0xa3, 0xaf, 0x42, 0x5b, 0xec, 0xf2, 0xa3, 0xfc, 0x7c, 0xf6, 0x1b, 0xab, 0x5d, 0x81, 0x19, 0xf0, 0xd6, 0xd3, 0xe4, 0xdd, 0x08, 0x98, 0x75, 0xd1, 0x65, 0x9a, 0xa4, 0x17, 0x01, 0x9c, 0x46, 0xc2, 0xfe, 0xd3, 0xd4, 0x26, 0x60, 0x13, 0xb0, 0x09, 0x78, 0xf5, 0xc8, 0xfc, 0xf7, 0x78, 0x89, 0x7e, 0xc3, 0x2f, 0xda, 0xf4, 0x91, 0x81, 0x9a, 0x2a, 0x32, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
unsigned int pilotwire_frost_len = 644;

// icon : thermostat Off
unsigned char pilotwire_therm_off_png[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x02, 0x03, 0x00, 0x00, 0x00, 0xbe, 0x50, 0x89, 0x58, 0x00, 0x00, 0x00, 0x09, 0x50, 0x4c, 0x54, 0x45, 0x00, 0x00, 0xf0, 0x2d, 0xab, 0xf2, 0xfc, 0xfe, 0xfb, 0x6b, 0x39, 0xd8, 0x49, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x00, 0x01, 0x62, 0x4b, 0x47, 0x44, 0x00, 0x88, 0x05, 0x1d, 0x48, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0b, 0x13, 0x00, 0x00, 0x0b, 0x13, 0x01, 0x00, 0x9a, 0x9c, 0x18, 0x00, 0x00, 0x00, 0x07, 0x74, 0x49, 0x4d, 0x45, 0x07, 0xe4, 0x0a, 0x18, 0x0d, 0x39, 0x2e, 0x44, 0x96, 0x87, 0xc3, 0x00, 0x00, 0x02, 0x8a, 0x49, 0x44, 0x41, 0x54, 0x58, 0xc3, 0xd5, 0x97, 0x4b, 0x6e, 0xe4, 0x20, 0x10, 0x40, 0xed, 0x96, 0xbc, 0xf1, 0x2a, 0x1b, 0x8e, 0x90, 0x8d, 0x4f, 0xe1, 0xdc, 0x80, 0x48, 0x26, 0x8b, 0x9c, 0x86, 0x4b, 0x64, 0xef, 0x4d, 0x46, 0x2d, 0x4e, 0x99, 0xfa, 0xd9, 0x18, 0x28, 0x20, 0x51, 0xcf, 0x48, 0x13, 0x2b, 0x72, 0x4b, 0xe1, 0x19, 0xea, 0x5f, 0xc5, 0x30, 0x7c, 0xeb, 0x09, 0xf7, 0xf6, 0xfa, 0x12, 0x82, 0x6f, 0x6f, 0x10, 0xda, 0x5b, 0xcc, 0x00, 0x84, 0xce, 0x09, 0xed, 0x33, 0x08, 0xd8, 0x9b, 0x22, 0xf8, 0xb9, 0x25, 0xc4, 0x0d, 0x17, 0xdb, 0xc0, 0x8e, 0xc7, 0xb4, 0x94, 0xf0, 0xf8, 0xea, 0x02, 0xbe, 0x01, 0xd0, 0x39, 0xff, 0x05, 0xb0, 0xd4, 0x3c, 0x72, 0x05, 0xfc, 0x43, 0xc0, 0xaf, 0xb0, 0x43, 0xd5, 0x5f, 0x51, 0xcd, 0x47, 0x81, 0xee, 0x11, 0xbf, 0x26, 0x60, 0xfc, 0xc3, 0xc0, 0xdf, 0x12, 0x72,
  0x5a, 0x3b, 0x80, 0xf9, 0x07, 0xc0, 0xec, 0x13, 0x77, 0x97, 0xc0, 0xb2, 0x77, 0x80, 0xf0, 0x2d, 0xa0, 0x25, 0xc3, 0x8f, 0x00, 0x67, 0x11, 0x30, 0xb6, 0x0e, 0x6c, 0x08, 0xb8, 0x06, 0xe0, 0x00, 0x18, 0x35, 0x40, 0xbc, 0x39, 0xb9, 0xd5, 0xac, 0xf0, 0xd2, 0x01, 0xd8, 0x66, 0x74, 0x9b, 0x79, 0x81, 0x6d, 0x1a, 0x80, 0x33, 0xaf, 0x20, 0x88, 0x2e, 0xc3, 0x13, 0x09, 0x01, 0x0f, 0x8a, 0x60, 0x15, 0x00, 0x3f, 0x84, 0x2d, 0x68, 0x83, 0x71, 0x2b, 0x81, 0xd1, 0xb1, 0x1e, 0x1d, 0xc0, 0xf0, 0x09, 0x3a, 0xb0, 0x92, 0xa2, 0xfc, 0x93, 0x03, 0x90, 0xdd, 0x0c, 0x8c, 0xac, 0x64, 0x01, 0x2c, 0x07, 0x30, 0xb0, 0x92, 0x05, 0x60, 0xde, 0x0f, 0x80, 0x3d, 0x85, 0x00, 0x09, 0x73, 0x02, 0x6f, 0x27, 0xb0, 0x0a, 0x30, 0xa5, 0x80, 0xb3, 0x39, 0x60, 0x12, 0x60, 0x42, 0x3f, 0xda, 0x0b, 0x60, 0x27, 0x96, 0xf6, 0x00, 0x40, 0xf8, 0xed, 0x0a, 0x38, 0xcb, 0x06, 0x8b, 0x76, 0xc8, 0x81, 0xd7, 0x1c, 0x18, 0xc5, 0x49, 0x07, 0xe0, 0xc4, 0xe9, 0xd1, 0x0e, 0x1f, 0x39, 0xb0, 0x66, 0xc0, 0x9f, 0x1e, 0x00, 0x5b, 0x24, 0x80, 0x2d, 0x9c, 0x25, 0x1f, 0x11, 0x60, 0x14, 0xe0, 0x49, 0xa4, 0x44, 0x60, 0x04, 0x60, 0xb3, 0x19, 0xe0, 0x44, 0x70, 0x04, 0x26, 0x27, 0x71, 0x93, 0xd8, 0x01, 0xe2, 0x15, 0xff, 0x89, 0x80, 0x43, 0x33, 0xa4, 0x76, 0x40, 0x33, 0xa0, 0x2d, 0x38, 0xf5, 0x60, 0x6d, 0x74, 0xa9, 0xa9, 0x9f, 0xe9, 0x03, 0x01, 0x48, 0x98, 0x02, 0xd8, 0x58, 0x78, 0xca, 0x2c, 0x52, 0x27, 0x05, 0x96, 0x37, 0xcf, 0xea, 0x53, 0x66, 0xb1, 0x7c, 0xa9, 0x2f, 0x58, 0x29, 0x43, 0x99, 0x25, 0x36, 0xb0, 0x4a, 0x76, 0x8f, 0x2e, 0x2a, 0xa8, 0x57, 0x18, 0xb2, 0xc0, 0xda, 0x03, 0x9a,
  0x35, 0x2a, 0x7a, 0x41, 0x2d, 0x20, 0x6d, 0xc0, 0xb7, 0x8e, 0x48, 0x80, 0x86, 0x90, 0x0d, 0x35, 0x7b, 0x86, 0x92, 0x7a, 0x50, 0x37, 0xf5, 0xd4, 0x73, 0xd6, 0xd4, 0x73, 0x77, 0x35, 0x60, 0x02, 0x03, 0x7b, 0x35, 0xe4, 0x04, 0xb8, 0xd7, 0x82, 0xf6, 0x26, 0x40, 0xa8, 0x85, 0xfd, 0x0c, 0x9b, 0xe3, 0x9f, 0xc4, 0x7d, 0x99, 0x38, 0xd8, 0xaa, 0x00, 0x98, 0x19, 0x50, 0x52, 0x0f, 0x07, 0x66, 0x10, 0x00, 0xa5, 0x9c, 0xd4, 0xe4, 0x85, 0x45, 0x98, 0xfe, 0x3d, 0x02, 0xa6, 0x06, 0xd0, 0x68, 0x1f, 0xee, 0x7a, 0x01, 0xb9, 0x91, 0x06, 0x40, 0x2d, 0x77, 0xa3, 0x96, 0x20, 0x01, 0xa0, 0xfb, 0x06, 0xa3, 0x16, 0x31, 0x2a, 0xd2, 0x18, 0x2a, 0xcb, 0xa7, 0x5e, 0x06, 0x4f, 0x60, 0xfe, 0xd0, 0x0b, 0xa9, 0xc4, 0x5a, 0xd8, 0x01, 0x58, 0xb5, 0x52, 0x3c, 0x47, 0x60, 0x1b, 0xb4, 0x62, 0x8e, 0xc3, 0x01, 0x03, 0xe1, 0x68, 0x28, 0x69, 0x3b, 0x50, 0x80, 0xb4, 0xa1, 0x28, 0x40, 0xda, 0x92, 0xa2, 0x0c, 0x41, 0x6f, 0x6a, 0x87, 0x16, 0x3e, 0x02, 0x69, 0x5b, 0x8c, 0x76, 0x08, 0x7a, 0x63, 0x8d, 0x96, 0x0c, 0x7a, 0x6b, 0xbe, 0xf8, 0xe2, 0x4c, 0xbd, 0xb4, 0xb9, 0x53, 0xb8, 0x91, 0x37, 0x0f, 0x20, 0x1b, 0x0f, 0x70, 0x8d, 0xe3, 0xe1, 0x28, 0x30, 0xd9, 0x80, 0x01, 0xbb, 0x1f, 0x11, 0xa5, 0x8f, 0x28, 0x12, 0x93, 0xf3, 0x09, 0xe4, 0x43, 0x8e, 0x0c, 0x60, 0xe7, 0x1c, 0x56, 0x8c, 0x49, 0x72, 0x93, 0xbc, 0xce, 0xb3, 0xe9, 0xa0, 0x45, 0x39, 0x25, 0x6f, 0x7d, 0x12, 0xa3, 0xdb, 0x6a, 0x72, 0x2d, 0xce, 0x00, 0x72, 0x45, 0x72, 0xa7, 0x2d, 0x66, 0xb9, 0x90, 0xdd, 0x8a, 0x33, 0xe0, 0xc6, 0xc0, 0x50, 0x05, 0xca, 0x5b, 0x73, 0x0e, 0x14, 0x17, 0xf3, 0x1c, 0x60, 0x5f, 0x0c,
  0xd5, 0xc1, 0xbb, 0xfd, 0x7c, 0x01, 0x0d, 0x89, 0x6e, 0x9f, 0xbd, 0x65, 0xe8, 0x9c, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
unsigned int pilotwire_therm_off_len = 794;

// icon : thermostat On
unsigned char pilotwire_therm_on_png[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x02, 0x03, 0x00, 0x00, 0x00, 0xbe, 0x50, 0x89, 0x58, 0x00, 0x00, 0x00, 0x09, 0x50, 0x4c, 0x54, 0x45, 0x00, 0x00, 0x80, 0xff, 0xe5, 0x00, 0xfd, 0xff, 0xfc, 0x95, 0xb6, 0x43, 0x97, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x00, 0x01, 0x62, 0x4b, 0x47, 0x44, 0x00, 0x88, 0x05, 0x1d, 0x48, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0b, 0x13, 0x00, 0x00, 0x0b, 0x13, 0x01, 0x00, 0x9a, 0x9c, 0x18, 0x00, 0x00, 0x00, 0x07, 0x74, 0x49, 0x4d, 0x45, 0x07, 0xe4, 0x0a, 0x18, 0x0d, 0x3b, 0x06, 0x43, 0x15, 0x4d, 0xbb, 0x00, 0x00, 0x02, 0x5c, 0x49, 0x44, 0x41, 0x54, 0x58, 0xc3, 0xd5, 0x97, 0x51, 0x6e, 0xc3, 0x20, 0x0c, 0x86, 0x93, 0x48, 0xbc, 0xe4, 0x9d, 0x4b, 0xe4, 0x14, 0x39, 0x42, 0x26, 0x85, 0x4b, 0xec, 0x14, 0xec, 0x10, 0x7b, 0xcf, 0xcb, 0xa4, 0x8a, 0x53, 0x0e, 0x83, 0x21, 0xc6, 0x18, 0xa8, 0xd4, 0x3d, 0xac, 0x51, 0xd5, 0x4a, 0xe5, 0x0b, 0xb6, 0xf1, 0x8f, 0x31, 0xd3, 0xf4, 0xd4, 0xe3, 0x1e, 0xfd, 0xf1, 0xcd, 0x39, 0xdb, 0x9f, 0xc0, 0xf5, 0xa7, 0x58, 0x3d, 0xe0, 0x06, 0x16, 0xfa, 0x36, 0x02, 0x70, 0x75, 0x5d, 0xb0, 0x6b, 0xcf, 0x89, 0x05, 0x06, 0xfb, 0xc0, 0x05, 0x66, 0x7a, 0x41, 0x58, 0xf8, 0x1a, 0x02, 0xb6, 0x03, 0x04, 0x3b, 0xff, 0x02, 0xd8, 0x5a, 0x19, 0xa1, 0x80, 0x7d, 0x09, 0x78, 0x8b, 0x75, 0x68, 0xe6, 0xeb, 0x0e, 0xf3, 0x55, 0x60, 0x68, 0xe2, 0x6d, 0x04, 0x63, 0x5f, 0x06, 0xc2, 0x1f, 0xf3, 0x39, 0xf0, 0x41,
  0x99, 0x01, 0xa0, 0xcd, 0xde, 0x07, 0x8c, 0x39, 0xfe, 0x1e, 0x58, 0x2d, 0x4d, 0xb7, 0x00, 0x6c, 0xd7, 0x00, 0x70, 0x4f, 0x01, 0x3d, 0x1f, 0x22, 0xa0, 0x30, 0xfe, 0x04, 0x68, 0x53, 0x01, 0xc7, 0x08, 0x38, 0x0b, 0xc0, 0xe0, 0x8c, 0x19, 0xd0, 0xc6, 0x50, 0x60, 0x4e, 0x60, 0x8e, 0xe2, 0x07, 0x5f, 0xc1, 0x01, 0xcd, 0x67, 0x00, 0xe0, 0x28, 0x81, 0xa9, 0x02, 0xce, 0x98, 0x6e, 0x9c, 0x88, 0xfb, 0xe0, 0x8d, 0x9a, 0x68, 0x3c, 0x62, 0xe8, 0x33, 0x59, 0xa8, 0x04, 0x9c, 0x2d, 0x20, 0x5b, 0x45, 0xe0, 0xe0, 0x80, 0xa2, 0x8b, 0x7c, 0xd3, 0x37, 0x30, 0xf7, 0x01, 0xbf, 0xbb, 0x4b, 0xe0, 0xac, 0xd3, 0x1d, 0xe5, 0x1a, 0x31, 0x95, 0xa5, 0x4b, 0x80, 0x20, 0x78, 0x1f, 0x0b, 0x10, 0xb3, 0x11, 0x04, 0x13, 0x02, 0x04, 0x20, 0xfc, 0x36, 0x04, 0xa3, 0x01, 0x28, 0x76, 0x06, 0x03, 0xc2, 0xb8, 0x39, 0x9b, 0xc0, 0x3c, 0x02, 0x54, 0x04, 0x4c, 0x13, 0xd0, 0x08, 0x1c, 0x92, 0xec, 0xa1, 0x3e, 0x98, 0x01, 0x30, 0x27, 0xe0, 0x94, 0x81, 0xe0, 0x02, 0x15, 0x4b, 0xe5, 0x83, 0x07, 0x3e, 0x46, 0xc0, 0x4e, 0x05, 0x5b, 0x01, 0x59, 0x75, 0xc4, 0x4b, 0x06, 0x9c, 0xd5, 0xfe, 0x64, 0xc0, 0xd1, 0x04, 0xb6, 0x28, 0xec, 0x9d, 0xe9, 0xa9, 0x01, 0xa8, 0x11, 0x20, 0xcc, 0xf0, 0x0d, 0xb1, 0x17, 0x33, 0x68, 0x5c, 0x0d, 0x06, 0xe4, 0xac, 0x4b, 0xc0, 0x51, 0x01, 0xc7, 0x0d, 0x84, 0x0c, 0x15, 0x40, 0xca, 0xd9, 0xb3, 0x00, 0x98, 0xf8, 0xec, 0x99, 0x08, 0xc0, 0xd7, 0xc0, 0x49, 0x00, 0x9a, 0x61, 0x5a, 0x7c, 0xb1, 0xbd, 0xd4, 0x05, 0xa0, 0x46, 0x40, 0x6b, 0x86, 0x6e, 0xba, 0x9f, 0x15, 0x4c, 0x2d, 0x39, 0x17, 0x81, 0xab, 0x29, 0x5a, 0x04, 0x1e, 0x44, 0xf6, 0x33, 0x95, 0xfd,
  0x82, 0x80, 0xcb, 0x7b, 0x37, 0x7e, 0xef, 0xb7, 0x0b, 0xd7, 0x04, 0x9f, 0x1c, 0x27, 0xdf, 0x7a, 0xa0, 0x34, 0x0f, 0xac, 0xb7, 0x97, 0x6c, 0xf3, 0x42, 0xc3, 0xec, 0x1d, 0x48, 0x5e, 0xd6, 0xdb, 0xdf, 0x0f, 0xfa, 0xee, 0xdf, 0x92, 0x30, 0x58, 0x01, 0xf1, 0x40, 0x68, 0xed, 0x31, 0x8c, 0xaa, 0x04, 0x2d, 0x21, 0x02, 0x3f, 0xba, 0x3d, 0xe4, 0x22, 0x86, 0x80, 0xcb, 0xcd, 0x3b, 0x2f, 0x83, 0xa1, 0x48, 0x43, 0x6d, 0x01, 0x40, 0x2a, 0xa4, 0x19, 0x80, 0x26, 0x40, 0x2a, 0xc5, 0x58, 0x7b, 0xdc, 0x05, 0x80, 0x54, 0xcc, 0x57, 0x0a, 0x48, 0xc7, 0x01, 0x34, 0x07, 0x09, 0x10, 0x0f, 0x14, 0x0a, 0xa8, 0x11, 0xa0, 0x29, 0x90, 0x68, 0xea, 0x03, 0xc6, 0x5f, 0x1e, 0x8b, 0x29, 0x0a, 0x1f, 0xa6, 0x7c, 0xb0, 0x92, 0x75, 0x50, 0xe2, 0xd1, 0x4c, 0x56, 0x52, 0x8b, 0x87, 0x3b, 0xc9, 0x85, 0x96, 0xdb, 0x83, 0x20, 0xb7, 0x90, 0xcd, 0x46, 0x83, 0x01, 0x63, 0x51, 0x0f, 0x8d, 0x16, 0xc5, 0xcf, 0x8e, 0x8a, 0x6a, 0x34, 0x39, 0xa8, 0xc9, 0x15, 0x80, 0x5d, 0x06, 0x2c, 0xf9, 0x11, 0x1a, 0x2d, 0xbc, 0x49, 0xba, 0x26, 0x10, 0xf7, 0xd4, 0x42, 0xae, 0x50, 0xbc, 0x55, 0x0b, 0xb7, 0x55, 0x7a, 0x2d, 0xe6, 0x40, 0x48, 0x05, 0xbd, 0xd3, 0xd6, 0xcd, 0x9e, 0x2b, 0x6f, 0xc5, 0x1c, 0x58, 0x22, 0x30, 0x35, 0x81, 0xea, 0xd6, 0xcc, 0xdb, 0xe6, 0xea, 0x62, 0xce, 0x1b, 0xef, 0x29, 0xe6, 0xe2, 0x7e, 0x78, 0xeb, 0xde, 0x7f, 0x7e, 0x01, 0xfe, 0xb3, 0x77, 0x8f, 0x34, 0x31, 0xb4, 0xf5, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
unsigned int pilotwire_therm_on_len = 748;

/*************************************************\
 *               Variables
\*************************************************/

// variables
struct {
  bool    outside_mode      = false;              // flag for outside mode (target temperature dropped)
  uint8_t devices_present   = 0;                 // number of relays
  uint8_t source;                                 // temperature source (local or distant)
  uint8_t state             = PILOTWIRE_OFF;      // graph pilotwire state
  float   temperature       = NAN;                // graph temperature
  float   target            = NAN;                // graph target temperature
} pilotwire_status;

// status update
struct {
  bool   json  = false;                             // JSON should be updated
  bool   mode  = false;                             // mode should be updated (web client)
  bool   graph = false;                             // graph should be updated (web client)
  float  temp  = 0;                                 // last temperature updated (web client)
} pilotwire_updated;

// graph data
struct {
  float    temperature = NAN;                       // graph temperature
  float    temp_min    = NAN;                       // graph minimum temperature
  float    temp_max    = NAN;                       // graph maximum temperature
  float    target      = NAN;                       // graph target temperature
  uint8_t  state       = PILOTWIRE_OFF;             // graph pilotwire state
  uint32_t index       = 0;                         // current graph index for data
  uint32_t counter     = 0;                         // graph update counter
  short    arr_temp[PILOTWIRE_GRAPH_SAMPLE];        // graph temperature array (value x 10)
  short    arr_target[PILOTWIRE_GRAPH_SAMPLE];      // graph target temperature array (value x 10)
  uint8_t  arr_state[PILOTWIRE_GRAPH_SAMPLE];       // graph command state array
} pilotwire_graph;

/**************************************************\
 *                  Accessors
\**************************************************/

// get state label
void PilotwireGetStateLabel (uint8_t state, char* pstr_label, int size_label)
{
  // get label
  strcpy (pstr_label, "");
  switch (state)
  {
    case PILOTWIRE_DISABLED:         // Disabled
      strlcpy (pstr_label, D_PILOTWIRE_DISABLED, size_label);
      break;
    case PILOTWIRE_OFF:              // Off
      strlcpy (pstr_label, D_PILOTWIRE_OFF, size_label);
      break;
    case PILOTWIRE_COMFORT:          // Comfort
      strlcpy (pstr_label, D_PILOTWIRE_COMFORT, size_label);
      break;
    case PILOTWIRE_ECO:              // Economy
      strlcpy (pstr_label, D_PILOTWIRE_ECO, size_label);
      break;
    case PILOTWIRE_FROST:            // No frost
      strlcpy (pstr_label, D_PILOTWIRE_FROST, size_label);
      break;
    case PILOTWIRE_THERMOSTAT:       // Thermostat
      strlcpy (pstr_label, D_PILOTWIRE_THERMOSTAT, size_label);
      break;
  }
}

// get pilot wire state from relays state
uint8_t PilotwireGetRelayState ()
{
  uint8_t device_type;
  uint8_t relay1 = 0;
  uint8_t relay2 = 0;
  uint8_t state  = 0;
    
  // set number of relay to read status
  TasmotaGlobal.devices_present = pilotwire_status.devices_present;

  // get device connexion type
  device_type = PilotwireGetDeviceType ();

  // read relay state
  relay1 = bitRead (TasmotaGlobal.power, 0);

  // if pilotwire connexion and 2 relays
  if ((device_type == PILOTWIRE_DEVICE_NORMAL) && (TasmotaGlobal.devices_present > 1))
  {
    // read second relay state and convert to pilotwire state
    relay2 = bitRead (TasmotaGlobal.power, 1);
    if (relay1 == 0 && relay2 == 0) state = PILOTWIRE_COMFORT;
    else if (relay1 == 0 && relay2 == 1) state = PILOTWIRE_OFF;
    else if (relay1 == 1 && relay2 == 0) state = PILOTWIRE_FROST;
    else if (relay1 == 1 && relay2 == 1) state = PILOTWIRE_ECO;
  }

  // else, if pilotwire connexion and 1 relay
  else if (device_type == PILOTWIRE_DEVICE_NORMAL)
  {
    // convert to pilotwire state
    if (relay1 == 0) state = PILOTWIRE_COMFORT; else state = PILOTWIRE_OFF;
  }

  // else, direct connexion
  else
  {
    // convert to pilotwire state
    if (relay1 == 0) state = PILOTWIRE_OFF; else state = PILOTWIRE_COMFORT;
  }

  // reset number of relay
  TasmotaGlobal.devices_present = 0;

  return state;
}

// set relays state
void PilotwireSetRelayState (uint8_t new_state)
{
  uint8_t device_type;

  // set number of relay to set status
  TasmotaGlobal.devices_present = pilotwire_status.devices_present;

  // get device connexion type
  device_type = PilotwireGetDeviceType ();

  // if pilotwire connexion and 2 relays
  if ((device_type == PILOTWIRE_DEVICE_NORMAL) && (TasmotaGlobal.devices_present > 1))
  {
    // command relays for 4 modes pilotwire
    switch (new_state)
    {
      case PILOTWIRE_OFF:       // Set Off
        ExecuteCommandPower (1, POWER_OFF, SRC_MAX);
        ExecuteCommandPower (2, POWER_ON, SRC_MAX);
        break;
      case PILOTWIRE_COMFORT:   // Set Comfort
        ExecuteCommandPower (1, POWER_OFF, SRC_MAX);
        ExecuteCommandPower (2, POWER_OFF, SRC_MAX);
        break;
      case PILOTWIRE_ECO:       // Set Economy
        ExecuteCommandPower (1, POWER_ON, SRC_MAX);
        ExecuteCommandPower (2, POWER_ON, SRC_MAX);
        break;
      case PILOTWIRE_FROST:     // Set No Frost
        ExecuteCommandPower (1, POWER_ON, SRC_MAX);
        ExecuteCommandPower (2, POWER_OFF, SRC_MAX);
        break;
    }
  }

  // else, if pilotwire connexion and 1 relay
  else if (device_type == PILOTWIRE_DEVICE_NORMAL)
  {
    // command relay for 2 modes pilotwire
    if ((new_state == PILOTWIRE_OFF) || (new_state == PILOTWIRE_FROST)) ExecuteCommandPower (1, POWER_ON, SRC_MAX);
    else if ((new_state == PILOTWIRE_COMFORT) || (new_state == PILOTWIRE_ECO)) ExecuteCommandPower (1, POWER_OFF, SRC_MAX);
  }

  // else direct connexion
  else
  {
    // command relay for direct connexion
    if ((new_state == PILOTWIRE_OFF) || (new_state == PILOTWIRE_FROST)) ExecuteCommandPower (1, POWER_OFF, SRC_MAX);
    else if ((new_state == PILOTWIRE_COMFORT) || (new_state == PILOTWIRE_ECO)) ExecuteCommandPower (1, POWER_ON, SRC_MAX);
  }

  // reset number of relay
  TasmotaGlobal.devices_present = 0;
}

// get pilotwire device type (pilotwire or direct command)
uint8_t PilotwireGetDeviceType ()
{
  uint8_t device_type;

  // read actual VMC mode
  device_type = (uint8_t) Settings.energy_current_calibration;

  // if outvalue, set to disabled
  if (device_type > PILOTWIRE_DEVICE_DIRECT) device_type = PILOTWIRE_DEVICE_NORMAL;

  return device_type;
}

// set pilotwire device type (pilotwire or direct command)
void PilotwireSetDeviceType (uint8_t device_type)
{
  // if within range, set device type
  if (device_type <= PILOTWIRE_DEVICE_DIRECT) Settings.energy_current_calibration = (unsigned long) device_type;

  // update JSON status
  pilotwire_updated.json = true;
}

// get pilot actual mode
uint8_t PilotwireGetMode ()
{
  uint8_t actual_mode;

  // read actual mode and set to disabled if out of range
  actual_mode = (uint8_t) Settings.weight_reference;
  if (actual_mode > PILOTWIRE_OFFLOAD) actual_mode = PILOTWIRE_DISABLED;

  return actual_mode;
}

// set pilot wire mode
void PilotwireSetMode (uint8_t new_mode)
{
  uint8_t device_type;

  // get device connexion type
  device_type = PilotwireGetDeviceType ();

  // handle 1 relay device or direct connexion
  if ((TasmotaGlobal.devices_present == 1) || (device_type == PILOTWIRE_DEVICE_DIRECT))
  {
    if (new_mode == PILOTWIRE_ECO)   new_mode = PILOTWIRE_COMFORT;
    if (new_mode == PILOTWIRE_FROST) new_mode = PILOTWIRE_OFF;
  }

  // if within range, set mode
  if (new_mode <= PILOTWIRE_OFFLOAD) Settings.weight_reference = new_mode;

  // update JSON status
  pilotwire_updated.json = true;
  pilotwire_updated.mode = true;
}

// set pilot wire minimum temperature
void PilotwireSetMinTemperature (float new_temperature)
{
  // if within range, save temperature correction
  if ((new_temperature >= PILOTWIRE_TEMP_MIN) && (new_temperature <= PILOTWIRE_TEMP_MAX)) Settings.weight_item = (unsigned long) int (new_temperature * 10);

  // update JSON status
  pilotwire_updated.json = true;
}

// get pilot wire minimum temperature
float PilotwireGetMinTemperature ()
{
  float temperature;

  // get drift temperature (/10)
  temperature = float (Settings.weight_item) / 10;
  
  // check if within range
  if (temperature < PILOTWIRE_TEMP_MIN || temperature > PILOTWIRE_TEMP_MAX) temperature = PILOTWIRE_TEMP_DEFAULT_MIN;

  return temperature;
}

// set pilot wire maximum temperature
void PilotwireSetMaxTemperature (float new_temperature)
{
  // if within range, save temperature correction
  if (new_temperature >= PILOTWIRE_TEMP_MIN && new_temperature <= PILOTWIRE_TEMP_MAX) Settings.energy_frequency_calibration = (unsigned long) int (new_temperature * 10);

  // update JSON status
  pilotwire_updated.json = true;
}

// get pilot wire maximum temperature
float PilotwireGetMaxTemperature ()
{
  float temperature;

  // get drift temperature (/10)
  temperature = float (Settings.energy_frequency_calibration) / 10;
  
  // check if within range
  if (temperature < PILOTWIRE_TEMP_MIN || temperature > PILOTWIRE_TEMP_MAX) temperature = PILOTWIRE_TEMP_DEFAULT_MAX;

  return temperature;
}

// set target temperature
void PilotwireSetTargetTemperature (float new_temperature)
{
  // set within range
  new_temperature = max (new_temperature, PilotwireGetMinTemperature ());
  new_temperature = min (new_temperature, PilotwireGetMaxTemperature ());

  // save new target
  Settings.weight_max = (uint16_t) int (new_temperature * 10);

  // reset outside mode
  pilotwire_status.outside_mode = false;

  // update JSON status
  pilotwire_updated.json = true;
}

// get target temperature
float PilotwireGetTargetTemperature ()
{
  float temperature;

  // get target temperature (/10)
  temperature = float (Settings.weight_max) / 10;
  
  // set within range
  temperature = max (temperature, PilotwireGetMinTemperature ());
  temperature = min (temperature, PilotwireGetMaxTemperature ());

  return temperature;
}

// set outside mode dropdown temperature
void PilotwireSetOutsideDropdown (float new_dropdown)
{
  // save target temperature
  if (new_dropdown <= PILOTWIRE_TEMP_DROP) Settings.energy_voltage_calibration = (unsigned long) int (new_dropdown * 10);

  // update JSON status
  pilotwire_updated.json = true;
}

// get outside mode dropdown temperature
float PilotwireGetOutsideDropdown ()
{
  float temperature;

  // get target temperature (/10)
  temperature = float (Settings.energy_voltage_calibration) / 10;

  // check if within range
  if (temperature > PILOTWIRE_TEMP_DROP) temperature = PILOTWIRE_TEMP_DEFAULT_DROP;

  return temperature;
}

// set pilot wire drift temperature
void PilotwireSetDrift (float new_drift)
{
  // if within range, save temperature correction
  if (abs (new_drift) <= PILOTWIRE_TEMP_DRIFT) Settings.weight_calibration = (unsigned long) int (50 + (new_drift * 10));

  // update JSON status
  pilotwire_updated.json = true;
}

// get pilot wire drift temperature
float PilotwireGetDrift ()
{
  float temperature;

  // get drift temperature (/10)
  temperature = float (Settings.weight_calibration);
  temperature = ((temperature - 50) / 10);
  
  // check if within range
  if (abs (temperature) > PILOTWIRE_TEMP_DRIFT) temperature = PILOTWIRE_TEMP_DEFAULT_DRIFT;

  return temperature;
}

/******************************************************\
 *                         Functions
\******************************************************/

// get DS18B20 internal pullup state
bool PilotwireGetDS18B20Pullup ()
{
  bool flag_ds18b20 = bitRead (Settings.flag3.data, 24);

  return flag_ds18b20;
}

// set DS18B20 internal pullup state
bool PilotwireSetDS18B20Pullup (bool new_state)
{
  bool actual_state = PilotwireGetDS18B20Pullup ();

  // if not set, set pullup resistor for DS18B20 temperature sensor
  if (actual_state != new_state) bitWrite (Settings.flag3.data, 24, new_state);

  return (actual_state != new_state);
}

// get current temperature with drift correction
float PilotwireGetTemperature ()
{
  uint8_t index;
  float   temperature;

  char str_buffer[16];

  // try to read MQTT temperature
  pilotwire_status.source = PILOTWIRE_SOURCE_REMOTE;
  temperature = TemperatureGetValue ();

  // if not available, read temperature from local sensor
  if (isnan (temperature)) pilotwire_status.source = PILOTWIRE_SOURCE_LOCAL;

#ifdef USE_DS18x20
  if (isnan (temperature))
  {
    // read from DS18B20 sensor
    index = ds18x20_sensor[0].index;
    if (ds18x20_sensor[index].valid) temperature = ds18x20_sensor[index].temperature;
  }
#endif // USE_DS18x20

#ifdef USE_DHT
  if (isnan (temperature))
  {
    // read from DHT sensor
    if (isnan (temperature)) temperature = Dht[0].t;
  }
#endif // USE_DHT

  // if temperature is available, apply correction
  if (!isnan (temperature))
  {
    // trunc temperature to 0.2°C
    temperature = ceil (temperature * 10) / 10;

    // add correction
    temperature += PilotwireGetDrift ();
  }

  // if still nothing, no temperature source available
  if (isnan (temperature)) pilotwire_status.source = PILOTWIRE_SOURCE_NONE;

  return temperature;
}

// get current target temperature (handling outside dropdown)
float PilotwireGetCurrentTarget ()
{
  float  temp_target;

  // get target temperature
  temp_target = PilotwireGetTargetTemperature ();

  // if outside mode enabled, substract outside mode dropdown
  if (pilotwire_status.outside_mode) temp_target -= PilotwireGetOutsideDropdown ();

  return temp_target;
}

// handle commands issued by external triggers
void PilotwireHandleCommand (uint8_t state)
{
  // set outside mode according to relay state (OFF = outside mode)
  switch (state)
  {
    case POWER_OFF:
      pilotwire_status.outside_mode = true;
      break;
    case POWER_ON:
      pilotwire_status.outside_mode = false;
      break;
    case POWER_TOGGLE:
      pilotwire_status.outside_mode = !pilotwire_status.outside_mode;
      break;
  }
}

// Show JSON status (for MQTT)
void PilotwireShowJSON (bool append)
{
  uint8_t mode_pw;
  float   temperature;
  char    str_value[16];

  // add , in append mode or { in direct publish mode
  if (append) ResponseAppend_P (PSTR (",")); else Response_P (PSTR ("{"));

  // pilotwire mode  -->  "PilotWire":{"Relay":2,"Mode":5,"Label":"Thermostat",Offload:"ON","Temperature":20.5,"Target":21,"Min"=15.5,"Max"=25}
  ResponseAppend_P (PSTR ("\"%s\":{"), D_JSON_PILOTWIRE);

  // target temperature
  temperature = PilotwireGetTargetTemperature ();
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &temperature);
  ResponseAppend_P (PSTR ("\"%s\":%s"), D_JSON_PILOTWIRE_TARGET, str_value);

  // drift temperature
  temperature = PilotwireGetDrift ();
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &temperature);
  ResponseAppend_P (PSTR (",\"%s\":%s"), D_JSON_PILOTWIRE_DRIFT, str_value);

  // minimum temperature
  temperature = PilotwireGetMinTemperature ();
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &temperature);
  ResponseAppend_P (PSTR (",\"%s\":%s"), D_JSON_PILOTWIRE_MIN, str_value);

  // maximum temperature
  temperature = PilotwireGetMaxTemperature ();
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &temperature);
  ResponseAppend_P (PSTR (",\"%s\":%s"), D_JSON_PILOTWIRE_MAX, str_value);

  // outside mode status
  if (pilotwire_status.outside_mode) strcpy (str_value, "ON"); else strcpy (str_value, "OFF");
  ResponseAppend_P (PSTR (",\"%s\":\"%s\""), D_JSON_PILOTWIRE_OUTSIDE, str_value);

  // if defined, add current temperature
  temperature = PilotwireGetTemperature ();
  if (!isnan (temperature))
  {
    ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &temperature);
    ResponseAppend_P (PSTR (",\"%s\":%s"), D_JSON_PILOTWIRE_TEMPERATURE, str_value);
  }

  // current status
  mode_pw = PilotwireGetMode ();
  PilotwireGetStateLabel (mode_pw, str_value, sizeof (str_value));
  ResponseAppend_P (PSTR (",\"%s\":%d"), D_JSON_PILOTWIRE_RELAY, TasmotaGlobal.devices_present);
  ResponseAppend_P (PSTR (",\"%s\":%d"), D_JSON_PILOTWIRE_MODE, mode_pw);
  ResponseAppend_P (PSTR (",\"%s\":\"%s\""), D_JSON_PILOTWIRE_LABEL, str_value);
  ResponseAppend_P (PSTR ("}"));

  // if pilot wire mode is enabled
  //      relay state  -->  ,"State":{"Mode":4,"Label":"Comfort"}
  if (mode_pw != PILOTWIRE_DISABLED)
  {
    // get mode and associated label
    mode_pw = PilotwireGetRelayState ();
    PilotwireGetStateLabel (mode_pw, str_value, sizeof (str_value));
    ResponseAppend_P (PSTR (",\"%s\":{"), D_JSON_PILOTWIRE_STATE);
    ResponseAppend_P (PSTR ("\"%s\":%d"), D_JSON_PILOTWIRE_MODE, mode_pw);
    ResponseAppend_P (PSTR (",\"%s\":\"%s\""), D_JSON_PILOTWIRE_LABEL, str_value);
    ResponseAppend_P (PSTR ("}"));
  }

  ResponseAppend_P (PSTR ("}"));

  // publish it if not in append mode
  if (!append)
  {
    ResponseAppend_P (PSTR ("}"));
    MqttPublishPrefixTopic_P (TELE, PSTR (D_RSLT_SENSOR));
  } 

  // updates have been published
  pilotwire_updated.json = false; 
  pilotwire_updated.mode = true; 
}

// Handle pilot wire MQTT commands
bool PilotwireMqttCommand ()
{
  bool command_handled = true;
  int  command_code;
  char command [CMDSZ];

  // check MQTT command
  command_code = GetCommandCode (command, sizeof (command), XdrvMailbox.topic, kPilotWireCommands);

  // handle command
  switch (command_code)
  {
    case CMND_PILOTWIRE_MODE:  // set mode
      PilotwireSetMode (XdrvMailbox.payload);
      break;
    case CMND_PILOTWIRE_TARGET:  // set target temperature 
      PilotwireSetTargetTemperature (atof (XdrvMailbox.data));
      break;
    case CMND_PILOTWIRE_DRIFT:  // set temperature drift correction 
      PilotwireSetDrift (atof (XdrvMailbox.data));
      break;
    case CMND_PILOTWIRE_MIN:  // set minimum temperature 
      PilotwireSetMinTemperature (atof (XdrvMailbox.data));
      break;
    case CMND_PILOTWIRE_MAX:  // set maximum temperature 
      PilotwireSetMaxTemperature (atof (XdrvMailbox.data));
      break;
    case CMND_PILOTWIRE_OUTSIDE:  // set outside mode dropdown 
      PilotwireSetOutsideDropdown (atof (XdrvMailbox.data));
      break;
    default:
      command_handled = false;
  }

  // if needed, send updated status
  if (command_handled == true) PilotwireShowJSON (false);
  
  return command_handled;
}

void PilotwireUpdateGraph ()
{
  float value;

  // if temperature has been mesured, update current graph index
  if (!isnan (pilotwire_graph.temperature))
  {
    // set current temperature
    value = pilotwire_graph.temperature * 10;
    pilotwire_graph.arr_temp[pilotwire_graph.index] = (short)value;

    // set target temperature
    value = pilotwire_graph.target * 10;
    pilotwire_graph.arr_target[pilotwire_graph.index] = (short)value;

    // set pilotwire state
    pilotwire_graph.arr_state[pilotwire_graph.index] = pilotwire_graph.state;
  }

  // increase temperature data index and reset if max reached
  pilotwire_graph.index ++;
  pilotwire_graph.index = pilotwire_graph.index % PILOTWIRE_GRAPH_SAMPLE;

  // set graph updated flag and init graph values
  pilotwire_updated.graph = true;
  pilotwire_graph.temperature = NAN;
  pilotwire_graph.target      = NAN;
  pilotwire_graph.state       = PILOTWIRE_OFF;
}

// update pilotwire status
void PilotwireUpdateStatus ()
{
  uint8_t heater_state, target_state, heater_mode;
  float   heater_temperature, target_temperature;

  // get heater data
  heater_mode  = PilotwireGetMode ();
  heater_state = PilotwireGetRelayState ();
  heater_temperature = PilotwireGetTemperature ();
  target_temperature = PilotwireGetCurrentTarget ();

  // update current temperature
  if (!isnan (heater_temperature))
  {
    // set new temperature
    pilotwire_status.temperature = heater_temperature;

    // update graph temperature for current slot
    if (!isnan (pilotwire_graph.temperature)) pilotwire_graph.temperature = min (pilotwire_graph.temperature, heater_temperature);
    else pilotwire_graph.temperature = heater_temperature;
  }

  // update target temperature
  if (!isnan (target_temperature))
  {
    // if temperature change, data update
    if (target_temperature != pilotwire_status.target) pilotwire_updated.json = true;
    pilotwire_status.target = target_temperature;
    
    // update graph target temperature for current slot
    if (!isnan (pilotwire_graph.target)) pilotwire_graph.target = min (pilotwire_graph.target, target_temperature);
    else pilotwire_graph.target = target_temperature;
  } 

  // if relay change, data update
  if (heater_state != pilotwire_status.state) pilotwire_updated.json = true;
  pilotwire_status.state = heater_state;

  // update graph relay state  for current slot
  if (pilotwire_graph.state != PILOTWIRE_COMFORT) pilotwire_graph.state = heater_state;

  // if pilotwire mode is enabled
  if (heater_mode != PILOTWIRE_DISABLED)
  {
    // if offload mode, target state is off
    if (OffloadIsOffloaded () == true) target_state = PILOTWIRE_OFF;
 
    // else if thermostat mode
    else if (heater_mode == PILOTWIRE_THERMOSTAT)
    {
      // if temperature is too low, target state is on
      // else, if too high, target state is off
      target_state = heater_state;
      if (heater_temperature < (target_temperature - PILOTWIRE_TEMP_THRESHOLD)) target_state = PILOTWIRE_COMFORT;
      else if (heater_temperature > (target_temperature + PILOTWIRE_TEMP_THRESHOLD)) target_state = PILOTWIRE_OFF;
    }

    // else target mode is heater mode
    else target_state = heater_mode;

    // if target state is different than heater state, change state
    if (target_state != heater_state)
    {
      // set relays
      PilotwireSetRelayState (target_state);

      // trigger state update
      pilotwire_updated.json = true;
    }
  }

  // if needed, publish new state
  if (pilotwire_updated.json) PilotwireShowJSON (false);
}

void PilotwireEverySecond ()
{
  // increment delay counter and if delay reached, update history data
  if (pilotwire_graph.counter == 0) PilotwireUpdateGraph ();
  pilotwire_graph.counter ++;
  pilotwire_graph.counter = pilotwire_graph.counter % PILOTWIRE_GRAPH_REFRESH;
}

void PilotwireInit ()
{
  int index;

  // save and reset number of relays
  pilotwire_status.devices_present = TasmotaGlobal.devices_present;
  TasmotaGlobal.devices_present = 0;

  // offload : module is not managing the relay
  OffloadSetRelayMode (false);

  // offload : remove all device types
  for (index = 0; index < OFFLOAD_DEVICE_MAX; index++) OffloadSetAvailableDevice (index, OFFLOAD_DEVICE_MAX);

  // offload : declare available device types
  index = 0;
  OffloadSetAvailableDevice (index++, OFFLOAD_DEVICE_ROOM);
  OffloadSetAvailableDevice (index++, OFFLOAD_DEVICE_OFFICE);
  OffloadSetAvailableDevice (index++, OFFLOAD_DEVICE_LIVING);
  OffloadSetAvailableDevice (index++, OFFLOAD_DEVICE_BATHROOM);
  OffloadSetAvailableDevice (index++, OFFLOAD_DEVICE_KITCHEN);

  // init default values
  pilotwire_status.source = PILOTWIRE_SOURCE_NONE;

  // initialise temperature graph
  for (index = 0; index < PILOTWIRE_GRAPH_SAMPLE; index++)
  {
    pilotwire_graph.arr_temp[index]   = SHRT_MAX;
    pilotwire_graph.arr_target[index] = SHRT_MAX;
    pilotwire_graph.arr_state[index]  = PILOTWIRE_DISABLED;
  }

  // user template for Pilotwire (force non inverted LED)
  SettingsUpdateText (SET_TEMPLATE_NAME, "Sonoff Pilotwire");
  if (Settings.user_template.gp.io[13] == 320) Settings.user_template.gp.io[13] = 288;
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// display pilotwire and thermostat icons
void PilotwireWebIconPilotOff ()     { Webserver->send (200, "image/png", pilotwire_off_png,       pilotwire_off_len);       }
void PilotwireWebIconPilotComfort () { Webserver->send (200, "image/png", pilotwire_comfort_png,   pilotwire_comfort_len);   }
void PilotwireWebIconPilotEco ()     { Webserver->send (200, "image/png", pilotwire_eco_png,       pilotwire_eco_len);       }
void PilotwireWebIconPilotFrost ()   { Webserver->send (200, "image/png", pilotwire_frost_png,     pilotwire_frost_len);     }
void PilotwireWebIconThermOff ()     { Webserver->send (200, "image/png", pilotwire_therm_off_png, pilotwire_therm_off_len); }
void PilotwireWebIconThermOn  ()     { Webserver->send (200, "image/png", pilotwire_therm_on_png,  pilotwire_therm_on_len);  }

// display status icon
void PilotwireWebIconMode ()
{
  uint8_t device_mode, device_state;

  // get device mode and state
  device_mode  = PilotwireGetMode ();
  device_state = PilotwireGetRelayState ();

  if (OffloadIsOffloaded ()) OffloadWebIconOffload ();
  else if (device_mode == PILOTWIRE_THERMOSTAT)
  {
    // display icon according to relay state
    switch (device_state)
    {
      case PILOTWIRE_OFF:
      case PILOTWIRE_FROST:
        PilotwireWebIconThermOff ();
        break;
      case PILOTWIRE_ECO:
      case PILOTWIRE_COMFORT:
        PilotwireWebIconThermOn ();
        break;
    }
  }
  else
  {
    // display icon according to relay state
    switch (device_state)
    {
      case PILOTWIRE_OFF:
        PilotwireWebIconPilotOff ();
        break;
      case PILOTWIRE_COMFORT:
        PilotwireWebIconPilotComfort ();
        break;
      case PILOTWIRE_ECO:
        PilotwireWebIconPilotEco ();
        break;
      case PILOTWIRE_FROST:
        PilotwireWebIconPilotFrost ();
        break;
    }
  }
}

// get status update
void PilotwireWebUpdate ()
{
  float temperature;
  char  str_value[8];
  char  str_text[32];

  // update temperature if it has changed since last update
  temperature = PilotwireGetTemperature ();
  if (pilotwire_updated.temp != temperature) ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &temperature); else strcpy (str_value, "");

  // send result
  sprintf (str_text, "%s;%d;%d", str_value, pilotwire_updated.mode, pilotwire_updated.graph);
  Webserver->send (200, "text/plain", str_text, strlen (str_text));

  // reset update flags
  pilotwire_updated.temp  = temperature;
  pilotwire_updated.mode  = false;
  pilotwire_updated.graph = false;
}

// append pilot wire state to main page
void PilotwireWebSensor ()
{
  uint8_t mode_pw;
  float   temperature;
  char    temp_unit;
  char    str_title[64];
  char    str_text[32];
  char    str_value[8];

  // get current mode and temperature
  mode_pw     = PilotwireGetMode ();
  temperature = PilotwireGetTemperature ();

  // set title according to sensor source
  switch (pilotwire_status.source)
  {
    case PILOTWIRE_SOURCE_NONE:  // no temperature source available
      sprintf (str_title, "%s", D_PILOTWIRE_TEMPERATURE);
      break;
    case PILOTWIRE_SOURCE_LOCAL:  // local temperature source used 
      sprintf (str_title, "%s <small><i>(%s)</i></small>", D_PILOTWIRE_TEMPERATURE, D_PILOTWIRE_LOCAL);
      break;
    case PILOTWIRE_SOURCE_REMOTE:  // remote temperature source used 
      sprintf (str_title, "%s <small><i>(%s)</i></small>", D_PILOTWIRE_TEMPERATURE, D_PILOTWIRE_REMOTE);
      break;
  }

  // generate current temperature string in bold
  if (isnan (temperature)) strcpy (str_value, "---"); else ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%01_f"), &temperature);
  sprintf (str_text, "<b>%s</b>", str_value);

  // add target temperature and unit
  if (mode_pw == PILOTWIRE_THERMOSTAT)
  {
    temperature = PilotwireGetCurrentTarget ();
    ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%01_f"), &temperature);
    strlcat (str_text, " / ", sizeof (str_text));
    strlcat (str_text, str_value, sizeof (str_text));
  }

  strlcat (str_text, " °", sizeof (str_text));
  temp_unit = TempUnit ();
  strncat (str_text, &temp_unit, 1);

  // display temperature
  WSContentSend_PD ("{s}%s{m}%s{e}\n", str_title, str_text);

  // display day or outside mode
  if (pilotwire_status.outside_mode)
  {
    temperature = PilotwireGetOutsideDropdown ();
    ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &temperature);
    strcpy (str_title, D_PILOTWIRE_OUTSIDE);
    sprintf (str_text, " (-%s°%c)", str_value, TempUnit ());
  }
  else
  {
    strcpy (str_title, D_PILOTWIRE_NORMAL);
    strcpy (str_text, "");
  }
  WSContentSend_PD (PSTR("{s}%s{m}<span><b>%s</b>%s</span>{e}\n"), D_PILOTWIRE_MODE, str_title, str_text);

  // display heater icon status
  WSContentSend_PD (PSTR("<tr><td colspan=2 style='width:100%%;text-align:center;padding:10px;'><img height=64 src='pw-mode.png'></td></tr>\n"));
}

// Data history JSON page
void PilotwireWebPageDataJson ()
{
  bool     first_value;
  uint16_t index, index_array;
  float    temperature;
  char     str_value[8];

  // start of data page
  WSContentBegin (200, CT_HTML);

  // loop thru temperature array
  WSContentSend_P (PSTR ("{\"temperature\":["));
  first_value = true;
  for (index = 1; index <= PILOTWIRE_GRAPH_SAMPLE; index++)
  {
    index_array = (index + pilotwire_graph.index) % PILOTWIRE_GRAPH_SAMPLE;
    if (pilotwire_graph.arr_temp[index_array] != SHRT_MAX)
    {
      temperature = (float)pilotwire_graph.arr_temp[index_array] / 10;
      if (first_value) ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &temperature);
      else ext_snprintf_P (str_value, sizeof (str_value), PSTR (",%1_f"), &temperature);
      WSContentSend_P (str_value);
      first_value = false;
    }
  }
  WSContentSend_P (PSTR ("]"));

  // loop thru target temperature array
  WSContentSend_P (PSTR (",\"target\":["));
  first_value = true;
  for (index = 1; index <= PILOTWIRE_GRAPH_SAMPLE; index++)
  {
    index_array = (index + pilotwire_graph.index) % PILOTWIRE_GRAPH_SAMPLE;
    if (pilotwire_graph.arr_temp[index_array] != SHRT_MAX)
    {
      temperature = (float)pilotwire_graph.arr_target[index_array] / 10;
      if (first_value) ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &temperature);
      else ext_snprintf_P (str_value, sizeof (str_value), PSTR (",%1_f"), &temperature);
      WSContentSend_P (str_value);
      first_value = false;
    }
  }
  WSContentSend_P (PSTR ("]"));

  // end of page
  WSContentSend_P (PSTR ("}"));
  WSContentEnd ();
}

void PilotwireWebButtonMode ()
{
  char str_label[16];

  if (pilotwire_status.outside_mode) sprintf (str_label, "%s %s", D_PILOTWIRE_NORMAL, D_PILOTWIRE_MODE);
  else sprintf (str_label, "%s %s", D_PILOTWIRE_OUTSIDE, D_PILOTWIRE_MODE);
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_PAGE_PILOTWIRE_SWITCH, str_label); 
}

// Pilot Wire heater mode switch page
void PilotwireWebPageSwitchMode ()
{
  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // invert mode
  pilotwire_status.outside_mode = !pilotwire_status.outside_mode;

  // auto reload root page with dark background
  WSContentStart_P (D_PILOTWIRE_CONTROL, false);
  WSContentSend_P (PSTR ("</script>\n"));
  WSContentSend_P (PSTR ("<meta http-equiv='refresh' content='0;URL=/' />\n"));
  WSContentSend_P (PSTR ("</head>\n"));
  WSContentSend_P (PSTR ("<body bgcolor='#303030'></body>\n"));
  WSContentSend_P (PSTR ("</html>\n"));
  WSContentEnd ();
}

// Pilotwire heater configuration web page
void PilotwireWebPageConfigure ()
{
  bool    is_modified;
  uint8_t device_mode, device_type;
  float   device_temp, temperature;
  char    str_argument[16];
  char    str_value[8];
  char    str_step[8];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg ("save"))
  {
    // get pilot wire mode according to 'mode' parameter
    WebGetArg (D_CMND_PILOTWIRE_MODE, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) PilotwireSetMode ((uint8_t)atoi (str_argument)); 

    // get pilot wire device type according to 'device' parameter
    WebGetArg (D_CMND_PILOTWIRE_DEVICE, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) PilotwireSetDeviceType ((uint8_t)atoi (str_argument)); 

    // get minimum temperature according to 'min' parameter
    WebGetArg (D_CMND_PILOTWIRE_MIN, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) PilotwireSetMinTemperature (atof (str_argument));

    // get maximum temperature according to 'max' parameter
    WebGetArg (D_CMND_PILOTWIRE_MAX, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) PilotwireSetMaxTemperature (atof (str_argument));

    // get target temperature according to 'target' parameter
    WebGetArg (D_CMND_PILOTWIRE_TARGET, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) PilotwireSetTargetTemperature (atof (str_argument));

    // get outside mode dropdown temperature according to 'outside' parameter
    WebGetArg (D_CMND_PILOTWIRE_OUTSIDE, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) PilotwireSetOutsideDropdown (abs (atof (str_argument)));

    // get temperature drift according to 'drift' parameter
    WebGetArg (D_CMND_PILOTWIRE_DRIFT, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) PilotwireSetDrift (atof (str_argument));

    // set ds18b20 pullup according to 'pullup' parameter
    WebGetArg (D_CMND_PILOTWIRE_PULLUP, str_argument, sizeof (str_argument));
    is_modified = PilotwireSetDS18B20Pullup (strlen (str_argument) > 0);
    if (is_modified == true) WebRestart (1);
  }

  // get temperature and target mode
  device_temp = PilotwireGetTemperature ();
  device_mode = PilotwireGetMode ();
  device_type = PilotwireGetDeviceType ();

  // beginning of form
  WSContentStart_P (D_PILOTWIRE_CONFIGURE " " D_PILOTWIRE);
  WSContentSendStyle ();
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("span.half {display:inline-block;width:47%%;margin-right:2%%;}\n"));
  WSContentSend_P (PSTR ("span.key {float:right;font-size:0.7rem;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  WSContentSend_P (PSTR("<form method='get' action='%s'>\n"), D_PAGE_PILOTWIRE_CONFIG);

  //    Connexion 
  // ---------------

  WSContentSend_P (D_CONF_FIELDSET_START, D_PILOTWIRE_CONNEXION);
  WSContentSend_P (PSTR ("<p>\n"));

  // command type selection
  device_type = PilotwireGetDeviceType ();
  if (device_type == PILOTWIRE_DEVICE_NORMAL) strcpy (str_argument, D_PILOTWIRE_CHECKED); else strcpy (str_argument, "");
  WSContentSend_P (PSTR ("<input type='radio' id='normal' name='%s' value=%d %s>%s<br>\n"), D_CMND_PILOTWIRE_DEVICE, PILOTWIRE_DEVICE_NORMAL, str_argument, D_PILOTWIRE);
  if (device_type == PILOTWIRE_DEVICE_DIRECT) strcpy (str_argument, D_PILOTWIRE_CHECKED); else strcpy (str_argument, "");
  WSContentSend_P (PSTR ("<input type='radio' id='direct' name='%s' value=%d %s>%s<br>\n"), D_CMND_PILOTWIRE_DEVICE, PILOTWIRE_DEVICE_DIRECT, str_argument, D_PILOTWIRE_DIRECT);

  WSContentSend_P (PSTR ("</p>\n"));
  WSContentSend_P (D_CONF_FIELDSET_STOP);

  //    Mode 
  // ----------------

  WSContentSend_P (D_CONF_FIELDSET_START, D_PILOTWIRE_MODE);
  WSContentSend_P (PSTR ("<p><span class='key'>%s</span>\n"), D_CMND_PILOTWIRE_MODE);

  // seletion : disabled
  if (device_mode == PILOTWIRE_DISABLED) strcpy (str_argument, D_PILOTWIRE_CHECKED); else strcpy (str_argument, "");
  WSContentSend_P (D_CONF_MODE_SELECT, D_CMND_PILOTWIRE_MODE, PILOTWIRE_DISABLED, PILOTWIRE_DISABLED, str_argument, D_PILOTWIRE_DISABLED);

  // selection : off
  if (device_mode == PILOTWIRE_OFF) strcpy (str_argument, D_PILOTWIRE_CHECKED); else strcpy (str_argument, "");
  WSContentSend_P (D_CONF_MODE_SELECT, D_CMND_PILOTWIRE_MODE, PILOTWIRE_OFF, PILOTWIRE_OFF, str_argument, D_PILOTWIRE_OFF);

  // if dual relay device
  if ((TasmotaGlobal.devices_present > 1) && (device_type == PILOTWIRE_DEVICE_NORMAL))
  {
    // selection : no frost
    if (device_mode == PILOTWIRE_FROST) strcpy (str_argument, D_PILOTWIRE_CHECKED); else strcpy (str_argument, "");
    WSContentSend_P (D_CONF_MODE_SELECT, D_CMND_PILOTWIRE_MODE, PILOTWIRE_FROST, PILOTWIRE_FROST, str_argument, D_PILOTWIRE_FROST);

    // selection : eco
    if (device_mode == PILOTWIRE_ECO) strcpy (str_argument, D_PILOTWIRE_CHECKED); else strcpy (str_argument, "");
    WSContentSend_P (D_CONF_MODE_SELECT, D_CMND_PILOTWIRE_MODE, PILOTWIRE_ECO, PILOTWIRE_ECO, str_argument, D_PILOTWIRE_ECO);
  }

  // selection : comfort
  if (device_mode == PILOTWIRE_COMFORT) strcpy (str_argument, D_PILOTWIRE_CHECKED); else strcpy (str_argument, "");
  WSContentSend_P (D_CONF_MODE_SELECT, D_CMND_PILOTWIRE_MODE, PILOTWIRE_COMFORT, PILOTWIRE_COMFORT, str_argument, D_PILOTWIRE_COMFORT);

  // selection : thermostat
  if (!isnan (device_temp)) 
  {
    // selection : target temperature
    if (device_mode == PILOTWIRE_THERMOSTAT) strcpy (str_argument, D_PILOTWIRE_CHECKED); else strcpy (str_argument, "");
    WSContentSend_P (D_CONF_MODE_SELECT, D_CMND_PILOTWIRE_MODE, PILOTWIRE_THERMOSTAT, PILOTWIRE_THERMOSTAT, str_argument, D_PILOTWIRE_THERMOSTAT); 
  }

  WSContentSend_P (PSTR ("</p>\n"));
  WSContentSend_P (D_CONF_FIELDSET_STOP);

  //   Thermostat 
  // --------------

  WSContentSend_P (D_CONF_FIELDSET_START, D_PILOTWIRE_THERMOSTAT);

  // if temperature is available
  if (!isnan (device_temp)) 
  {
    // get step temperature
    temperature = PILOTWIRE_TEMP_STEP;
    ext_snprintf_P (str_step, sizeof (str_step), PSTR ("%1_f"), &temperature);

    WSContentSend_P (PSTR ("<p>\n"));

    // target temperature
    temperature = PilotwireGetTargetTemperature ();
    ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &temperature);
    WSContentSend_P (D_CONF_TEMPERATURE, D_PILOTWIRE_TARGET, TempUnit (), D_CMND_PILOTWIRE_TARGET, D_CMND_PILOTWIRE_TARGET, PILOTWIRE_TEMP_MIN, PILOTWIRE_TEMP_MAX, str_step, str_value);

    // outside mode temperature dropdown
    temperature = PilotwireGetOutsideDropdown ();
    ext_snprintf_P (str_value, sizeof (str_value), PSTR ("-%1_f"), &temperature);
    WSContentSend_P (D_CONF_TEMPERATURE, D_PILOTWIRE_DROPDOWN, TempUnit (), D_CMND_PILOTWIRE_OUTSIDE, D_CMND_PILOTWIRE_OUTSIDE, - PILOTWIRE_TEMP_DROP, 0, str_step, str_value);

    WSContentSend_P (PSTR ("</p><p>\n"));

    // temperature minimum label and input
    temperature = PilotwireGetMinTemperature ();
    ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &temperature);
    WSContentSend_P (D_CONF_TEMPERATURE, D_PILOTWIRE_MIN, TempUnit (), D_CMND_PILOTWIRE_MIN, D_CMND_PILOTWIRE_MIN, PILOTWIRE_TEMP_MIN, PILOTWIRE_TEMP_MAX, str_step, str_value);

    // temperature maximum label and input
    temperature = PilotwireGetMaxTemperature ();
    ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &temperature);
    WSContentSend_P (D_CONF_TEMPERATURE, D_PILOTWIRE_MAX, TempUnit (), D_CMND_PILOTWIRE_MAX, D_CMND_PILOTWIRE_MAX, PILOTWIRE_TEMP_MIN, PILOTWIRE_TEMP_MAX, str_step, str_value);

    WSContentSend_P (PSTR ("</p>\n"));
  }
  else WSContentSend_P (PSTR ("<p><i>%s</i></p>\n"), D_PILOTWIRE_NOSENSOR);

  WSContentSend_P (D_CONF_FIELDSET_STOP);

  //   Sensor 
  // ----------

  WSContentSend_P (D_CONF_FIELDSET_START, D_PILOTWIRE_SENSOR);
  WSContentSend_P (PSTR ("<p>\n"));

  // if temperature is available
  if (!isnan (device_temp)) 
  {
    // temperature correction label and input
    temperature = PilotwireGetDrift ();
    ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &temperature);
    temperature = PILOTWIRE_TEMP_DRIFT_STEP;
    ext_snprintf_P (str_step, sizeof (str_step), PSTR ("%1_f"), &temperature);
    WSContentSend_P (D_CONF_TEMPERATURE, D_PILOTWIRE_DRIFT, TempUnit (), D_CMND_PILOTWIRE_DRIFT, D_CMND_PILOTWIRE_DRIFT, - PILOTWIRE_TEMP_DRIFT, PILOTWIRE_TEMP_DRIFT, str_step, str_value);
  }

  // pullup option for ds18b20 sensor
  if (PilotwireGetDS18B20Pullup ()) strcpy (str_argument, D_PILOTWIRE_CHECKED); else strcpy (str_argument, "");
  WSContentSend_P (PSTR ("<hr>\n"));
  WSContentSend_P (PSTR ("<p><input type='checkbox' name='%s' %s>%s</p>\n"), D_CMND_PILOTWIRE_PULLUP, str_argument, D_PILOTWIRE_PULLUP);

  WSContentSend_P (PSTR ("</p>\n"));
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
  int      index, array_index;
  int      unit_width, shift_unit, shift_width;
  int      graph_x, graph_left, graph_right, graph_width, graph_low, graph_high;
  float    graph_y, value, temp_scope;
  TIME_T   current_dst;
  uint32_t current_time;

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

  // get min and max temperature difference
  temp_scope = pilotwire_graph.temp_max - pilotwire_graph.temp_min;

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
    array_index = (index + pilotwire_graph.index) % PILOTWIRE_GRAPH_SAMPLE;
    if (pilotwire_graph.arr_state[array_index] != PILOTWIRE_DISABLED)
    {
      // calculate end point x and y
      graph_x = graph_left + (graph_width * index / PILOTWIRE_GRAPH_SAMPLE);

      // add the point to the line
      if (pilotwire_graph.arr_state[array_index] == PILOTWIRE_COMFORT) WSContentSend_P (PSTR ("%d,%d "), graph_x, graph_high);
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
    array_index = (index + pilotwire_graph.index) % PILOTWIRE_GRAPH_SAMPLE;
    if (pilotwire_graph.arr_target[array_index] != SHRT_MAX)
    {
      // read indexed temperature
      value = (float)pilotwire_graph.arr_target[array_index];
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
    array_index = (index + pilotwire_graph.index) % PILOTWIRE_GRAPH_SAMPLE;
    if (pilotwire_graph.arr_temp[array_index] != SHRT_MAX)
    {
      // read indexed temperature
      value = (float)pilotwire_graph.arr_temp[array_index];
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

// Pilot Wire heater public configuration web page
void PilotwireWebPageControl ()
{
  bool    updated = false;
  int     index;
  uint8_t device_mode;
  float   value, target;
  char    str_value[8];

  // get target temperature
  value = PilotwireGetTargetTemperature ();

  // if heater has to be switched off, set in anti-frost mode
  if (Webserver->hasArg(D_CMND_PILOTWIRE_OFF)) PilotwireSetMode (PILOTWIRE_FROST);

  // else, if heater has to be switched on, set in thermostat mode
  else if (Webserver->hasArg(D_CMND_PILOTWIRE_ON)) PilotwireSetMode (PILOTWIRE_THERMOSTAT);

  // else, if target temperature has been changed
  else if (Webserver->hasArg(D_CMND_PILOTWIRE_TARGET))
  {
    WebGetArg (D_CMND_PILOTWIRE_TARGET, str_value, sizeof (str_value));
    PilotwireSetTargetTemperature (atof (str_value));
  }

  // beginning of page
  WSContentStart_P (D_PILOTWIRE_CONTROL, false);
  WSContentSend_P (PSTR ("</script>\n"));

    // set default min and max graph temperature
    pilotwire_graph.temp_min = PilotwireGetMinTemperature ();
    pilotwire_graph.temp_max = PilotwireGetMaxTemperature ();

    // adjust to current temperature
    value = PilotwireGetTemperature ();
    if (pilotwire_graph.temp_min > value) pilotwire_graph.temp_min = floor (value);
    if (pilotwire_graph.temp_max < value) pilotwire_graph.temp_max = ceil  (value);

    // loop to adjust graph min and max temperature
    for (index = 0; index < PILOTWIRE_GRAPH_SAMPLE; index++)
      if (pilotwire_graph.arr_temp[index] != SHRT_MAX)
      {
        // read indexed temperature
        value = (float)pilotwire_graph.arr_temp[index];
        value = value / 10; 

        // adjust minimum and maximum temperature
        if (pilotwire_graph.temp_min > value) pilotwire_graph.temp_min = floor (value);
        if (pilotwire_graph.temp_max < value) pilotwire_graph.temp_max = ceil  (value);
      }

    // page data refresh script
    WSContentSend_P (PSTR ("<script type='text/javascript'>\n"));
    WSContentSend_P (PSTR ("function update() {\n"));
    WSContentSend_P (PSTR ("httpUpd=new XMLHttpRequest();\n"));
    WSContentSend_P (PSTR ("httpUpd.onreadystatechange=function() {\n"));
    WSContentSend_P (PSTR (" if (httpUpd.responseText.length>0) {\n"));
    WSContentSend_P (PSTR ("  arr_param=httpUpd.responseText.split(';');\n"));
    WSContentSend_P (PSTR ("  str_random=Math.floor(Math.random()*100000);\n"));
    WSContentSend_P (PSTR ("  if (arr_param[0]!='') {document.getElementById('temp').innerHTML=arr_param[0];}\n"));
    WSContentSend_P (PSTR ("  if (arr_param[1]==1) {document.getElementById('mode').setAttribute('src','pw-mode.png?rnd='+str_random);}\n"));
    WSContentSend_P (PSTR ("  if (arr_param[2]==1) {document.getElementById('data').data='data.svg?rnd='+str_random;}\n"));
    WSContentSend_P (PSTR (" }\n"));
    WSContentSend_P (PSTR ("}\n"));
    WSContentSend_P (PSTR ("httpUpd.open('GET','pw.upd',true);\n"));
    WSContentSend_P (PSTR ("httpUpd.send();\n"));
    WSContentSend_P (PSTR ("}\n"));
    WSContentSend_P (PSTR ("setInterval(function() {update();},1000);\n"));
    WSContentSend_P (PSTR ("</script>\n"));

    // page style
    WSContentSend_P (PSTR ("<style>\n"));
    WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));
    WSContentSend_P (PSTR ("div {width:100%%;max-width:800px;margin:0.2rem auto;padding:0;text-align:center;vertical-align:middle;}\n"));
    WSContentSend_P (PSTR ("div.title {font-size:1.5rem;font-weight:bold;margin:10px auto;}\n"));
    WSContentSend_P (PSTR ("div.temp {font-size:2.5rem;color:yellow;}\n"));
    WSContentSend_P (PSTR ("span.unit {font-size:2rem;margin:0;padding:0 0.25rem 0 0.5rem;}\n"));

    // power button
    WSContentSend_P (PSTR ("button {background:none;color:#fff;padding:0.2rem 0.5rem;border:1px #666 solid;}\n"));
    WSContentSend_P (PSTR ("button.power {margin:0.5rem;padding:8px;border-radius:48px;}\n"));
    WSContentSend_P (PSTR ("img.power {height:64px;}\n"));

    // current room temperature
    WSContentSend_P (PSTR ("div.target {text-align:center;background:#6e2c00;width:14rem;margin:10px auto;border:1px orange solid;border-radius:8px;}\n"));

    WSContentSend_P (PSTR ("div.read {width:60%%;color:orange;padding:0px;margin:0px auto;border:none;}\n"));
    WSContentSend_P (PSTR ("div.read span {color:orange;}\n"));
    WSContentSend_P (PSTR ("div.read span.value {font-size:2.5rem;}\n"));

    // control to set room temperature
    WSContentSend_P (PSTR ("div.set {width:100%%;font-size:0.9rem;padding:0px;margin:2px auto;border:none;}\n"));
    WSContentSend_P (PSTR ("div.set div {display:inline-block;color:orange;background:black;width:auto;padding:0.1rem 0.5rem;margin:0.1rem auto;border:none;border-radius:4px;}\n"));
    WSContentSend_P (PSTR ("div.set div.minus {border-top-left-radius:16px;border-bottom-left-radius:16px;padding-left:0.5rem;}\n"));
    WSContentSend_P (PSTR ("div.set div.plus {border-top-right-radius:16px;border-bottom-right-radius:16px;padding-right:0.5rem;}\n"));
    WSContentSend_P (PSTR ("div.set div:hover {color:black;background:orange;}\n"));

    // graph
    WSContentSend_P (PSTR (".svg-container {position:relative;vertical-align:middle;overflow:hidden;width:100%%;max-width:%dpx;padding-bottom:65%%;}\n"), PILOTWIRE_GRAPH_WIDTH);
    WSContentSend_P (PSTR (".svg-content {display:inline-block;position:absolute;top:0;left:0;}\n"));
    WSContentSend_P (PSTR ("</style>\n"));

    // page body
    WSContentSend_P (PSTR ("</head>\n"));
    WSContentSend_P (PSTR ("<body>\n"));
    WSContentSend_P (PSTR ("<form name='control' method='get' action='%s'>\n"), D_PAGE_PILOTWIRE_CONTROL);

    // room name
    WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText(SET_DEVICENAME));

    // actual temperature
    value = PilotwireGetTemperature ();
    ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &value);
    WSContentSend_P (PSTR ("<div class='temp'><span id='temp'>%s</span><span class='unit'>°C</span></div>\n"), str_value);

    // if device in thermostat mode,
    device_mode = PilotwireGetMode ();
    if (device_mode == PILOTWIRE_THERMOSTAT)
    {
      // button to switch off
      WSContentSend_P (PSTR ("<div><button class='power' type='submit' name='off'><img class='power' id='mode' src='pw-mode.png' /></button></div>\n"));

      // target temperature display
      value = PilotwireGetTargetTemperature ();
      ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &value);
      WSContentSend_P (PSTR ("<div class='target'>\n"));
      WSContentSend_P (PSTR ("<div class='read'><span class='value'>%s</span><span class='unit'>°C</span></div>\n"), str_value);

      // target temperature increase / decrease
      WSContentSend_P (PSTR ("<div class='set'>\n"), str_value);

      target = value - 2;
      ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &target);
      WSContentSend_P (PSTR ("<a href='/control?%s=%s'><div class='minus' title='%s °C'>%s</div></a>\n"), D_CMND_PILOTWIRE_TARGET, str_value, str_value, "- 2°");
      target = value - 0.5;
      ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &target);
      WSContentSend_P (PSTR ("<a href='/control?%s=%s'><div class='minus' title='%s °C'>%s</div></a>\n"), D_CMND_PILOTWIRE_TARGET, str_value, str_value, "- 0.5°");

      target = value + 0.5;
      ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &target);
      WSContentSend_P (PSTR ("<a href='/control?%s=%s'><div class='plus' title='%s °C'>%s</div></a>\n"), D_CMND_PILOTWIRE_TARGET, str_value, str_value, "+ 0.5°");
      target = value + 2;
      ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &target);
      WSContentSend_P (PSTR ("<a href='/control?%s=%s'><div class='plus' title='%s °C'>%s</div></a>\n"), D_CMND_PILOTWIRE_TARGET, str_value, str_value, "+ 2°");

      WSContentSend_P (PSTR ("</div>\n"));      // set
      WSContentSend_P (PSTR ("</div>\n"));      // target
    }

    // else, display switch on button
    else WSContentSend_P (PSTR ("<div><button class='power' type='submit' name='on'><img class='power' id='mode' src='pw-mode.png' /></button></div>"));

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

bool Xsns98 (uint8_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  {
    case FUNC_INIT:
      PilotwireInit ();
      break;
    case FUNC_COMMAND:
      result = PilotwireMqttCommand ();
      break;
    case FUNC_EVERY_250_MSECOND:
      PilotwireUpdateStatus ();
      break;
    case FUNC_EVERY_SECOND:
      PilotwireEverySecond ();
      break;
    case FUNC_JSON_APPEND:
      PilotwireShowJSON (true);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      // pages
      Webserver->on ("/config",    PilotwireWebPageConfigure);
      Webserver->on ("/mode",      PilotwireWebPageSwitchMode);
      Webserver->on ("/control",   PilotwireWebPageControl);
      Webserver->on ("/data.json", PilotwireWebPageDataJson);
      Webserver->on ("/base.svg",  PilotwireWebGraphFrame);
      Webserver->on ("/data.svg",  PilotwireWebGraphData);

      // icons
      Webserver->on ("/th-off.png",   PilotwireWebIconThermOff);
      Webserver->on ("/th-on.png",    PilotwireWebIconThermOn);
      Webserver->on ("/pw-off.png",   PilotwireWebIconPilotOff);
      Webserver->on ("/pw-cmft.png",  PilotwireWebIconPilotComfort);
      Webserver->on ("/pw-eco.png",   PilotwireWebIconPilotEco);
      Webserver->on ("/pw-frost.png", PilotwireWebIconPilotFrost);
      Webserver->on ("/pw-mode.png",  PilotwireWebIconMode);

      // update status
      Webserver->on ("/pw.upd", PilotwireWebUpdate);
      break;
    case FUNC_WEB_SENSOR:
      PilotwireWebSensor ();
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      PilotwireWebButtonMode ();
      WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_PAGE_PILOTWIRE_CONTROL, D_PILOTWIRE_CONTROL);
      break;
    case FUNC_WEB_ADD_BUTTON:
      WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>Configure %s</button></form></p>\n"), D_PAGE_PILOTWIRE_CONFIG, D_PILOTWIRE);
      break;
#endif  // USE_Webserver

  }
  return result;
}

#endif // USE_PILOTWIRE
