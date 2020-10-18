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

#define PILOTWIRE_BUFFER_SIZE           128

#define D_PAGE_PILOTWIRE_CONFIG         "config"
#define D_PAGE_PILOTWIRE_CONTROL        "control"
#define D_PAGE_PILOTWIRE_SWITCH         "mode"
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
#define D_PILOTWIRE_ENGLISH             "en"
#define D_PILOTWIRE_FRENCH              "fr"
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
#define PILOTWIRE_GRAPH_PERCENT_START   10      
#define PILOTWIRE_GRAPH_PERCENT_STOP    90

// constant chains
const char D_CONF_FIELDSET_START[] PROGMEM = "<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>\n";
const char D_CONF_FIELDSET_STOP[]  PROGMEM = "</fieldset></p>\n";
const char D_CONF_MODE_SELECT[]    PROGMEM = "<input type='radio' name='%s' id='%d' value='%d' %s>%s<br>\n";
const char D_CONF_BUTTON[]         PROGMEM = "<p><form action='%s' method='get'><button>%s</button></form></p>\n";
const char D_CONF_TEMPERATURE[]    PROGMEM = "<span class='half'>%s (°%s)<span class='key'>%s</span><br><input type='number' name='%s' min='%d' max='%d' step='%s' value='%s'></span>\n";
const char D_GRAPH_SEPARATION[]    PROGMEM = "<line class='dash' x1='%d%%' y1='%d%%' x2='%d%%' y2='%d%%' />\n";
const char D_GRAPH_TEMPERATURE[]   PROGMEM = "<text class='temperature' x='%d%%' y='%d%%'>%s°C</text>\n";

// control page texts
enum PilotwireLangages { PILOTWIRE_LANGAGE_ENGLISH, PILOTWIRE_LANGAGE_FRENCH, PILOTWIRE_LANGAGE_MAX };
const char *const arr_control_lang[]    PROGMEM = {"en", "fr"};
const char *const arr_control_langage[] PROGMEM = {"English", "Français"};
const char *const arr_control_off[]     PROGMEM = {"Switch Off", "Eteindre"};
const char *const arr_control_on[]      PROGMEM = {"Switch On", "Allumer"};

// enumarations
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
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x08, 0x03, 0x00, 0x00, 0x00, 0xf4, 0xe0, 0x91, 0xf9, 0x00, 0x00, 0x00, 0x33, 0x50, 0x4c, 0x54, 0x45, 0x40, 0x00, 0x00, 0x25, 0xaa, 0xf3, 0x43, 0xad, 0xf1, 0x5c, 0xb4, 0xf4, 0x73, 0xb9, 0xf5, 0x8c, 0xc4, 0xf6, 0x9c, 0xca, 0xf7, 0xa4, 0xcd, 0xf5, 0xad, 0xd3, 0xf5, 0xba, 0xd7, 0xfb, 0xbe, 0xd8, 0xf6, 0xd3, 0xe7, 0xfa, 0xe3, 0xed, 0xfb, 0xec, 0xf3, 0xfc, 0xf4, 0xf8, 0xfb, 0xfa, 0xff, 0xff, 0xfd, 0xff, 0xfc, 0x7f, 0xc1, 0x89, 0x5f, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x00, 0x01, 0x62, 0x4b, 0x47, 0x44, 0x00, 0x88, 0x05, 0x1d, 0x48, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0b, 0x13, 0x00, 0x00, 0x0b, 0x13, 0x01, 0x00, 0x9a, 0x9c, 0x18, 0x00, 0x00, 0x00, 0x07, 0x74, 0x49, 0x4d, 0x45, 0x07, 0xe4, 0x0a, 0x12, 0x13, 0x33, 0x13, 0x9f, 0x14, 0x4d, 0x46, 0x00, 0x00, 0x04, 0xd9, 0x49, 0x44, 0x41, 0x54, 0x78, 0xda, 0xed, 0x5b, 0x8b, 0x92, 0xdb, 0x20, 0x0c, 0xb4, 0x94, 0xdc, 0xab, 0xcd, 0x83, 0xff, 0xff, 0xda, 0x4b, 0x8c, 0xc1, 0x02, 0x6b, 0x01, 0x19, 0x27, 0xbe, 0x99, 0xc4, 0x9d, 0xb9, 0x69, 0xdd, 0x38, 0x5a, 0x56, 0x62, 0xf5, 0x30, 0x37, 0x0c, 0x7f, 0xf1, 0x72, 0xfe, 0xda, 0xd7, 0xfa, 0x6e, 0x10, 0x5c, 0x7a, 0xed, 0x6c, 0xfe, 0xe9, 0x08, 0x9c, 0xdb, 0x19, 0x81, 0xdb, 0x19, 0x41, 0x6a, 0xf3, 0xf9, 0x08, 0x16, 0x06, 0x77, 0x02, 0xb0, 0x9b, 0x20, 0x68, 0xd6, 0xfe, 0x08, 0x00, 0xf7, 0x4c, 0x00, 0x2d, 0x37, 0xdf,
  0x00, 0x5e, 0x14, 0xc0, 0xe3, 0xf5, 0xd1, 0x06, 0xc0, 0xbd, 0x1e, 0x80, 0x77, 0x10, 0xbe, 0x01, 0xec, 0x0f, 0xe0, 0xe1, 0x89, 0xa9, 0x71, 0x1b, 0xbe, 0x2e, 0x80, 0xdd, 0x5d, 0xf0, 0xde, 0x05, 0x6f, 0x00, 0x7f, 0xad, 0x20, 0x71, 0x2f, 0x0f, 0xe0, 0x1d, 0x84, 0xf8, 0x22, 0xa2, 0x5d, 0x01, 0xd0, 0xde, 0x00, 0x98, 0x79, 0x6f, 0x06, 0xb6, 0x00, 0x20, 0xff, 0x62, 0x4c, 0xc7, 0x9b, 0xb8, 0x20, 0xcf, 0xc1, 0x36, 0x00, 0xfd, 0x0c, 0xb8, 0x1e, 0x00, 0xbc, 0x01, 0x03, 0x2a, 0x80, 0xe6, 0x20, 0x7c, 0x14, 0x03, 0xcf, 0xdc, 0x86, 0x56, 0x00, 0x44, 0xc2, 0xaa, 0x64, 0x80, 0x4c, 0x68, 0x7a, 0x01, 0xd0, 0x82, 0x01, 0x7a, 0x16, 0x00, 0xbe, 0x5f, 0xc1, 0xd4, 0xcc, 0x00, 0xf9, 0xfb, 0x5b, 0xef, 0x02, 0x75, 0x33, 0xd0, 0x7d, 0xf3, 0x79, 0x04, 0x71, 0xd1, 0xe3, 0x1d, 0x4b, 0x3c, 0xd8, 0x00, 0x2c, 0x10, 0x8c, 0x1c, 0x50, 0x60, 0x80, 0x88, 0x8c, 0x0e, 0xb0, 0x02, 0xb8, 0x2c, 0xc3, 0xc0, 0x23, 0xa0, 0xe9, 0xa7, 0x75, 0xfd, 0xe6, 0x18, 0xa0, 0xa5, 0x13, 0x46, 0x12, 0xee, 0x0c, 0x8c, 0x74, 0xdc, 0x03, 0x20, 0xff, 0xcc, 0x86, 0x00, 0x94, 0xd5, 0x05, 0x12, 0xfc, 0x4f, 0x26, 0xab, 0x42, 0x98, 0x00, 0x10, 0xab, 0x08, 0xd8, 0xaf, 0x5f, 0x77, 0xff, 0x9d, 0x96, 0xcd, 0x00, 0x8c, 0x86, 0x90, 0x20, 0x78, 0x12, 0xcc, 0x69, 0xca, 0xc6, 0x00, 0xa9, 0xeb, 0x11, 0x11, 0x30, 0xa8, 0x1e, 0xea, 0x01, 0x90, 0x6c, 0x3f, 0xb4, 0xc7, 0x62, 0x04, 0x00, 0xbd, 0xec, 0x00, 0x70, 0x19, 0x2f, 0x01, 0x40, 0x17, 0xb9, 0xb0, 0x1b, 0xd5, 0x3c, 0xdd, 0xc5, 0xc0, 0xf1, 0x70, 0x38, 0xf0, 0xbf, 0x00, 0x00, 0xef, 0x72, 0xb0, 0xfe, 0x3c, 0x6e, 0x15, 0x0a, 0x2b, 0x00, 0xc6,
  0x07, 0x8e, 0x3f, 0xdf, 0x73, 0x44, 0xeb, 0xeb, 0x21, 0x24, 0xff, 0x24, 0xf2, 0xa4, 0xaa, 0x93, 0x35, 0x00, 0x41, 0x67, 0x2a, 0x89, 0x0e, 0x7a, 0x3a, 0xc9, 0x98, 0xac, 0x24, 0xaa, 0x5a, 0x10, 0x06, 0xa9, 0x13, 0x59, 0x8f, 0x4c, 0x00, 0xd8, 0xbb, 0x86, 0x90, 0x50, 0x54, 0xb7, 0x21, 0x89, 0x0b, 0x07, 0x35, 0x70, 0x8d, 0x7c, 0x0c, 0x08, 0x55, 0x5d, 0x07, 0xbc, 0xde, 0xcf, 0x59, 0x8f, 0xa9, 0xbd, 0x26, 0xf4, 0x0a, 0x11, 0xb3, 0xe6, 0x3a, 0x00, 0x21, 0xc6, 0xf1, 0x77, 0x60, 0x17, 0x84, 0x34, 0xc1, 0xd3, 0x37, 0xac, 0x4c, 0xc7, 0x97, 0xd3, 0xf9, 0x6b, 0xe2, 0x90, 0x21, 0xd7, 0xc8, 0x05, 0x01, 0x37, 0x52, 0x83, 0xb6, 0x7a, 0xe0, 0x72, 0xfa, 0xfa, 0x3e, 0x4e, 0x49, 0x07, 0xe9, 0x0d, 0x64, 0x80, 0x51, 0x9a, 0x68, 0x05, 0x70, 0xbd, 0xba, 0xcb, 0xed, 0xcf, 0xcf, 0xb4, 0x10, 0x63, 0x0c, 0x10, 0x94, 0x4f, 0x6b, 0x32, 0x0a, 0x65, 0x87, 0x1a, 0x06, 0xe8, 0x5e, 0x8c, 0x80, 0x5e, 0x00, 0xb1, 0xe0, 0x0b, 0xa9, 0xbf, 0xca, 0x00, 0x0d, 0x44, 0x49, 0xf4, 0x03, 0x10, 0x6d, 0x00, 0x92, 0xef, 0x62, 0x25, 0x25, 0x68, 0x37, 0x82, 0x88, 0x62, 0x0d, 0x68, 0xd6, 0x81, 0x58, 0x74, 0x8d, 0x0b, 0x9d, 0xb1, 0x40, 0x06, 0x44, 0xe4, 0xf9, 0x82, 0x89, 0x95, 0x67, 0x0c, 0x4a, 0xe8, 0x1f, 0xe7, 0x98, 0xef, 0x04, 0x1d, 0x0a, 0x03, 0xd1, 0x53, 0x94, 0xd6, 0x0b, 0xab, 0xa4, 0xd8, 0x7d, 0x0a, 0x01, 0xa2, 0xa4, 0x34, 0x62, 0xd6, 0x7b, 0x43, 0x8d, 0xf1, 0xc9, 0x75, 0xac, 0x14, 0x54, 0x55, 0x00, 0x41, 0x81, 0x94, 0x08, 0x17, 0x18, 0xe6, 0xce, 0x28, 0xdc, 0x5d, 0xc8, 0x1e, 0xd0, 0xa3, 0x1a, 0x80, 0xf3, 0x6d, 0xfb, 0x7f, 0x9c, 0x4e, 0xa8, 0x06, 0x09, 0x4b,
  0xcd, 0x3a, 0x23, 0x86, 0xc9, 0x99, 0xd7, 0x04, 0x61, 0xa1, 0x3d, 0x0e, 0x24, 0xc4, 0xce, 0x68, 0x0a, 0x3e, 0x46, 0x8f, 0x6c, 0x3b, 0xa0, 0xa0, 0x65, 0x67, 0x44, 0xd6, 0x69, 0x51, 0xef, 0x84, 0x84, 0xd2, 0xce, 0x08, 0xa4, 0xbc, 0x47, 0x03, 0x90, 0x9d, 0x91, 0x75, 0x52, 0xb3, 0xc1, 0x8c, 0x48, 0x74, 0x46, 0x2b, 0x26, 0x55, 0x3d, 0x03, 0x0a, 0x21, 0x7b, 0x3c, 0x8f, 0x0a, 0x1e, 0x0a, 0xc0, 0xc1, 0xe5, 0xcf, 0x71, 0xf0, 0x74, 0x00, 0x32, 0x02, 0x56, 0x90, 0xd0, 0xbb, 0x0d, 0x85, 0x50, 0x4f, 0xf3, 0x89, 0xcd, 0x27, 0x24, 0x45, 0x21, 0x0a, 0xb2, 0xe3, 0xdb, 0xe6, 0x4a, 0xe6, 0x5d, 0x27, 0x44, 0x85, 0x9e, 0x87, 0xe7, 0x14, 0x2b, 0xba, 0x06, 0x2e, 0xcc, 0x89, 0x56, 0xa4, 0x63, 0x90, 0xc7, 0x85, 0xf0, 0xe4, 0xc9, 0x28, 0x4f, 0x94, 0xb9, 0xbf, 0x8c, 0x00, 0x84, 0xc2, 0xa4, 0x5d, 0x6e, 0x9a, 0xf2, 0x28, 0x6b, 0x41, 0x59, 0x49, 0xc7, 0x7a, 0x96, 0x6a, 0x70, 0x41, 0x18, 0x89, 0x8a, 0xf2, 0x63, 0xd1, 0xa4, 0xe4, 0xb3, 0xe2, 0x34, 0x18, 0x45, 0xce, 0x50, 0x5b, 0x53, 0x00, 0x20, 0x69, 0x0e, 0x05, 0x88, 0xb8, 0x7a, 0x86, 0x35, 0xe1, 0x1c, 0x8d, 0x43, 0xfa, 0x70, 0x4e, 0x40, 0x19, 0x80, 0xcb, 0xeb, 0x0f, 0x92, 0xf3, 0xa8, 0x72, 0x67, 0x94, 0x7d, 0x5c, 0x2d, 0x51, 0xb2, 0x23, 0xb4, 0xc5, 0xa3, 0xb5, 0x34, 0x87, 0x17, 0x2b, 0x05, 0x92, 0xd2, 0x19, 0x91, 0x9f, 0x2d, 0xc4, 0xc5, 0xe3, 0x2d, 0xb8, 0x7c, 0x53, 0xa5, 0x1f, 0xee, 0x9d, 0x25, 0x57, 0xc9, 0x39, 0xac, 0x8f, 0x28, 0x23, 0x09, 0x8c, 0x35, 0x28, 0x1a, 0x49, 0x2d, 0x22, 0x00, 0xe0, 0xcb, 0x0a, 0x4d, 0x33, 0xc1, 0xf1, 0xbd, 0x2b, 0x9a, 0xcc, 0xfe, 0x49, 0x5a, 0x71, 0x5c, 0xef,
  0x0d, 0xd3, 0xf1, 0x46, 0xc9, 0xfe, 0x22, 0xe1, 0xb8, 0x65, 0x77, 0x12, 0x22, 0xa0, 0x9d, 0x01, 0xe6, 0x02, 0x6e, 0xf5, 0x40, 0xb7, 0x8e, 0x20, 0x3a, 0x93, 0x51, 0xc9, 0x57, 0x66, 0x80, 0x35, 0x71, 0xc4, 0xf6, 0xf3, 0xa8, 0x14, 0x9b, 0x7a, 0x20, 0xdb, 0x90, 0x6a, 0x92, 0x0b, 0xd5, 0x11, 0x25, 0x00, 0xe2, 0xee, 0xf9, 0xff, 0x30, 0xf5, 0x65, 0x21, 0xdd, 0xd8, 0x19, 0xd0, 0x33, 0x44, 0xc9, 0xbe, 0x18, 0x4e, 0x7c, 0x06, 0xde, 0x79, 0x9e, 0x95, 0x5a, 0xe7, 0x84, 0x69, 0x2b, 0xd5, 0x0e, 0xe0, 0x76, 0xff, 0xfc, 0x91, 0xd4, 0xba, 0xb8, 0xe7, 0x80, 0x6f, 0x4e, 0x85, 0xcf, 0xb2, 0x24, 0x85, 0xcb, 0x2e, 0xf1, 0x1f, 0xe7, 0xb4, 0xcf, 0xc5, 0x45, 0x1f, 0x64, 0x40, 0x26, 0xa5, 0xa4, 0x6a, 0x74, 0x0d, 0x00, 0xae, 0xd7, 0xf3, 0xf1, 0x70, 0x14, 0x1a, 0x8e, 0xbb, 0x3e, 0x34, 0x41, 0x1d, 0x52, 0xdd, 0xf4, 0x5e, 0x68, 0xf0, 0x40, 0x44, 0x76, 0xb9, 0x9c, 0x33, 0xc5, 0x27, 0xaa, 0xbd, 0x45, 0x6d, 0x7b, 0x5f, 0xd0, 0x02, 0x60, 0x1c, 0x8f, 0xb9, 0x6c, 0xf0, 0xc9, 0x04, 0x3a, 0x03, 0xdd, 0x3b, 0x70, 0xbe, 0xde, 0xc8, 0xc0, 0x6d, 0x1f, 0xd4, 0x19, 0x28, 0x35, 0x05, 0x15, 0x06, 0x6a, 0x31, 0xe0, 0xae, 0x39, 0x03, 0xea, 0x6b, 0x21, 0x0e, 0x49, 0x52, 0xb1, 0x05, 0xf7, 0x47, 0x19, 0x40, 0xfc, 0xc8, 0xd5, 0xd5, 0x96, 0x23, 0x6a, 0x46, 0x6e, 0x9c, 0x20, 0xb6, 0xeb, 0xc0, 0x35, 0xeb, 0x4f, 0x58, 0x7f, 0x31, 0xc9, 0x71, 0x1c, 0x4d, 0xb4, 0x82, 0x81, 0x92, 0x12, 0xe6, 0x0d, 0x12, 0xc1, 0xe9, 0x29, 0xec, 0xcd, 0xca, 0x43, 0xe2, 0x6a, 0x2e, 0x28, 0x76, 0x68, 0xa2, 0x33, 0x1a, 0xb7, 0x87, 0xed, 0xe5, 0xb5, 0x5a, 0x82, 0xa1, 0x1a, 0x0d, 0x9e, 0x61,
  0x98, 0xfc, 0x2e, 0x73, 0x4e, 0x2b, 0x02, 0x07, 0x11, 0x0c, 0x26, 0x02, 0x96, 0x63, 0xba, 0x46, 0x04, 0x0e, 0x21, 0x68, 0xb4, 0x3f, 0x4d, 0x3f, 0x39, 0xab, 0x8a, 0x0d, 0x53, 0x82, 0x62, 0x11, 0xe8, 0x1a, 0x00, 0x08, 0x7f, 0x8b, 0xba, 0x93, 0xa8, 0xf6, 0xc2, 0x7c, 0x68, 0xb0, 0xe1, 0x5a, 0x1c, 0xc0, 0x72, 0x7a, 0x9c, 0x4d, 0x86, 0x2d, 0x51, 0xa0, 0x98, 0x31, 0x9f, 0xe1, 0x5c, 0x79, 0x96, 0x0c, 0xd9, 0xb1, 0x9f, 0x21, 0x5d, 0x7b, 0x92, 0x4a, 0x1b, 0xfe, 0xac, 0x3a, 0xc3, 0xba, 0xfa, 0x2c, 0x99, 0x2b, 0x5d, 0x86, 0xef, 0xe9, 0x3d, 0x4b, 0xd6, 0xfd, 0x6b, 0xa7, 0xeb, 0x4f, 0xd3, 0x0d, 0x9b, 0xd8, 0xef, 0x60, 0x60, 0xd8, 0xe6, 0xb7, 0x6e, 0x3b, 0x18, 0x18, 0x36, 0xf9, 0xc5, 0xe7, 0x2e, 0x06, 0x40, 0x2e, 0x30, 0x22, 0xe0, 0x3e, 0x00, 0x4f, 0xbe, 0x7e, 0x01, 0x36, 0x42, 0xaf, 0x14, 0x66, 0x21, 0x34, 0x88, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
unsigned int pilotwire_therm_off_len = 1427;

// icon : thermostat On
unsigned char pilotwire_therm_on_png[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x04, 0x03, 0x00, 0x00, 0x00, 0x31, 0x10, 0x7c, 0xf8, 0x00, 0x00, 0x00, 0x30, 0x50, 0x4c, 0x54, 0x45, 0x56, 0x00, 0x00, 0xff, 0xe5, 0x00, 0xff, 0xe5, 0x0d, 0xff, 0xe7, 0x4d, 0xff, 0xe9, 0x66, 0xff, 0xe9, 0x68, 0xff, 0xef, 0xa2, 0xff, 0xf2, 0xbb, 0xff, 0xf5, 0xc9, 0xff, 0xf7, 0xd5, 0xff, 0xf8, 0xdb, 0xff, 0xf9, 0xe0, 0xff, 0xf9, 0xe2, 0xff, 0xfb, 0xee, 0xff, 0xfd, 0xf7, 0xff, 0xff, 0xff, 0x11, 0xc4, 0x2e, 0x05, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x00, 0x01, 0x62, 0x4b, 0x47, 0x44, 0x00, 0x88, 0x05, 0x1d, 0x48, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0b, 0x13, 0x00, 0x00, 0x0b, 0x13, 0x01, 0x00, 0x9a, 0x9c, 0x18, 0x00, 0x00, 0x00, 0x07, 0x74, 0x49, 0x4d, 0x45, 0x07, 0xe4, 0x0a, 0x12, 0x13, 0x34, 0x30, 0x72, 0x32, 0xaa, 0xf3, 0x00, 0x00, 0x02, 0xa9, 0x49, 0x44, 0x41, 0x54, 0x68, 0xde, 0xed, 0x99, 0xbd, 0x95, 0x14, 0x31, 0x0c, 0x80, 0xc7, 0x21, 0x91, 0x2d, 0x02, 0x82, 0x7b, 0x24, 0x94, 0x40, 0x09, 0x94, 0x40, 0x49, 0x74, 0x70, 0xe9, 0x56, 0x43, 0x19, 0xd7, 0x86, 0x69, 0x00, 0x71, 0xc7, 0x8d, 0xff, 0x64, 0xcb, 0xb2, 0xac, 0xe1, 0x2d, 0xc1, 0x4e, 0xb0, 0xbb, 0xf3, 0x66, 0xfd, 0x59, 0xff, 0x96, 0x3d, 0xc7, 0x71, 0xdd, 0x85, 0x88, 0xd1, 0x30, 0xdc, 0x23, 0xda, 0x08, 0x88, 0x36, 0x02, 0xa6, 0xcb, 0xa4, 0x80, 0x41, 0x84, 0x02, 0xc0, 0x7d, 0x0d, 0x62, 0xfe, 0xda, 0x13, 0x20, 0x5a, 0x5c, 0xe9, 0x93, 0xe8,
  0x7e, 0x53, 0x87, 0x32, 0xf1, 0x15, 0x80, 0xb8, 0x07, 0x28, 0xca, 0x3c, 0x00, 0x6f, 0x00, 0xaf, 0x0e, 0x4a, 0x06, 0x10, 0xef, 0x06, 0x78, 0xb8, 0xf1, 0x0a, 0x80, 0x3a, 0xa5, 0x06, 0x6e, 0xbc, 0x2f, 0xc0, 0xac, 0xc2, 0x23, 0x0e, 0x2e, 0x2c, 0x28, 0xf1, 0xee, 0x80, 0xea, 0x1f, 0x0e, 0x82, 0xcd, 0x06, 0x00, 0x60, 0x02, 0xb8, 0x57, 0x40, 0xb0, 0x00, 0x00, 0xe6, 0x22, 0xfc, 0x2f, 0x80, 0xb7, 0xa1, 0x4c, 0x3a, 0x2f, 0x01, 0xfc, 0xf9, 0xb1, 0x0d, 0xc0, 0xeb, 0x00, 0x9b, 0x36, 0x68, 0x00, 0xc4, 0xef, 0x14, 0xe0, 0x08, 0x6f, 0x08, 0x00, 0x33, 0x20, 0xf0, 0x00, 0xf2, 0xb8, 0x07, 0x38, 0x32, 0x82, 0xdc, 0xd2, 0xc7, 0xbd, 0x17, 0x5e, 0xbe, 0x02, 0x7c, 0xac, 0xe6, 0x68, 0x07, 0xbc, 0x8f, 0x0f, 0x53, 0x37, 0x7e, 0x7b, 0xfd, 0xc7, 0x17, 0x98, 0x02, 0xa6, 0x71, 0xf0, 0xfb, 0x3b, 0x99, 0x84, 0xde, 0x48, 0x36, 0x38, 0x27, 0x69, 0x26, 0x9d, 0x8c, 0x1f, 0x05, 0x52, 0x07, 0x08, 0x4a, 0x80, 0xe3, 0x83, 0x0f, 0xfa, 0x47, 0xa3, 0x50, 0x06, 0x8e, 0x30, 0x42, 0x8f, 0x00, 0xce, 0x02, 0xf8, 0x9b, 0x8d, 0x53, 0x40, 0x58, 0x48, 0xe7, 0xba, 0x90, 0x36, 0xb0, 0x41, 0x81, 0x1d, 0xd7, 0x83, 0x3c, 0x06, 0xd2, 0xd5, 0xbb, 0x74, 0x0a, 0xc8, 0xae, 0xcb, 0x80, 0x7c, 0xaf, 0x2a, 0x28, 0x0e, 0x80, 0x12, 0x74, 0x15, 0x09, 0x40, 0x26, 0xcc, 0x00, 0xce, 0x0a, 0x80, 0xf6, 0x52, 0x03, 0x1c, 0xac, 0x10, 0x06, 0x5e, 0x48, 0xfd, 0x01, 0x18, 0x01, 0x9d, 0x00, 0x63, 0x2b, 0x30, 0x80, 0x22, 0x00, 0x9b, 0xc7, 0xa2, 0x0d, 0xde, 0xc7, 0x38, 0x23, 0x20, 0xb0, 0xc5, 0x54, 0x04, 0x80, 0x54, 0x4d, 0x97, 0x00, 0x41, 0x5e, 0x23, 0xe7, 0x80, 0x43, 0x0d, 0xc8, 0xcb, 0x7b,
  0x57, 0x3b, 0x38, 0x1d, 0x96, 0x01, 0xc7, 0x1a, 0xe0, 0xc3, 0xed, 0x76, 0x7b, 0xde, 0x97, 0xe0, 0x57, 0x09, 0x39, 0x56, 0x02, 0x47, 0xc2, 0x72, 0x0a, 0xe8, 0x73, 0x4b, 0x00, 0xfc, 0x28, 0x69, 0x23, 0x02, 0x60, 0x04, 0xa8, 0xd2, 0x86, 0x05, 0xd0, 0xd4, 0xba, 0x16, 0x90, 0x54, 0xf8, 0xf4, 0x73, 0x53, 0x85, 0x64, 0xc4, 0xcf, 0x18, 0xf7, 0x8c, 0x98, 0x00, 0x4f, 0x27, 0x40, 0xed, 0xc6, 0x38, 0x59, 0x06, 0x17, 0x43, 0x79, 0xb2, 0x8e, 0x5a, 0x01, 0x5a, 0x09, 0x76, 0xd3, 0xf9, 0xea, 0x82, 0x02, 0x2b, 0x1a, 0x54, 0x00, 0xd4, 0x17, 0x55, 0x02, 0x88, 0x4c, 0x59, 0x77, 0x5c, 0x59, 0xf7, 0x04, 0x80, 0xfd, 0xd2, 0x5a, 0xff, 0x0e, 0x8c, 0x00, 0x47, 0xfe, 0xec, 0x1c, 0x29, 0x2c, 0x6d, 0x49, 0xea, 0x73, 0xea, 0xa1, 0x0e, 0xd3, 0xc5, 0xd5, 0xb7, 0x23, 0x3d, 0xdd, 0x75, 0x8b, 0x80, 0x22, 0x00, 0x9e, 0x3b, 0x47, 0xd4, 0x35, 0x18, 0xe5, 0x54, 0x3c, 0x5b, 0x21, 0x6a, 0x5a, 0x1c, 0x5f, 0xac, 0x9f, 0x45, 0x88, 0x9a, 0x26, 0xab, 0x05, 0x0c, 0x62, 0x49, 0x6a, 0xf3, 0x7c, 0xad, 0x41, 0x5a, 0x97, 0xda, 0x3d, 0xc2, 0xbc, 0xd1, 0xec, 0x00, 0x79, 0x2f, 0xbf, 0xd8, 0xea, 0x36, 0x4d, 0x4d, 0x09, 0xc9, 0x63, 0xb9, 0xd9, 0x46, 0x0e, 0xb0, 0xda, 0xee, 0xb7, 0x36, 0x2c, 0x80, 0xe5, 0x0d, 0x07, 0x07, 0x00, 0x23, 0xc0, 0xf1, 0x6d, 0x25, 0x79, 0xc2, 0xd8, 0x80, 0xf8, 0x7d, 0xb2, 0xed, 0x23, 0x5e, 0x38, 0xdd, 0xa8, 0xd8, 0x78, 0x8e, 0xe3, 0x00, 0xd6, 0xb7, 0xbe, 0xc3, 0x48, 0xa4, 0x02, 0xcc, 0x6a, 0xeb, 0x30, 0x17, 0x5c, 0x2f, 0x00, 0xbf, 0xfd, 0x2f, 0xa5, 0xac, 0x64, 0xa3, 0xea, 0x00, 0xa2, 0xd4, 0x91, 0xaa, 0x1e, 0x68, 0x8e, 0x40, 0xb0, 0x12, 0x21,
  0x57, 0x24, 0xcd, 0x21, 0x0c, 0xa9, 0x89, 0x98, 0x00, 0x41, 0x03, 0x88, 0xcc, 0xdd, 0xda, 0x41, 0x54, 0xb3, 0x14, 0xe0, 0x06, 0xa0, 0x5e, 0x8d, 0xfc, 0xf8, 0xf5, 0x90, 0x70, 0x14, 0x56, 0x4e, 0x60, 0xb9, 0xc3, 0x5c, 0x01, 0x50, 0xa5, 0x12, 0xf3, 0x7e, 0x6a, 0xe1, 0x30, 0x0e, 0xa7, 0xaf, 0x6d, 0x25, 0x80, 0x17, 0x04, 0x10, 0x01, 0xe2, 0x5b, 0x5f, 0xe9, 0x58, 0x58, 0x7e, 0x71, 0x2d, 0x08, 0x70, 0xd4, 0xb9, 0xc0, 0x88, 0x10, 0x8e, 0x7f, 0x73, 0xfd, 0x01, 0x7c, 0x8f, 0x62, 0x7d, 0xea, 0x3d, 0x73, 0x92, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
unsigned int pilotwire_therm_on_len = 864;

/*************************************************\
 *               Variables
\*************************************************/

// variables
bool     pilotwire_outside_mode;                   // flag for outside mode (target temperature dropped)
bool     pilotwire_updated;                        // JSON needs update
uint8_t  pilotwire_devices_present;                // number of relays on the devices
uint8_t  pilotwire_temperature_source;             // temperature source (local or distant)
uint8_t  pilotwire_langage;                        // current langange for control page
float    pilotwire_temperature;                    // graph temperature
float    pilotwire_target;                         // graph target temperature
uint8_t  pilotwire_state;                          // graph pilotwire state
float    pilotwire_graph_temperature;              // graph temperature
float    pilotwire_graph_target;                   // graph target temperature
uint8_t  pilotwire_graph_state;                    // graph pilotwire state
uint32_t pilotwire_graph_index;                    // current graph index for data
uint32_t pilotwire_graph_counter;                  // graph update counter
short    arr_temperature[PILOTWIRE_GRAPH_SAMPLE];  // graph temperature array (value x 10)
short    arr_target[PILOTWIRE_GRAPH_SAMPLE];       // graph target temperature array (value x 10)
uint8_t  arr_state[PILOTWIRE_GRAPH_SAMPLE];        // graph command state array

/**************************************************\
 *                  Accessors
\**************************************************/

// get state label
String PilotwireGetStateLabel (uint8_t state)
{
  String str_label;

  // get label
  switch (state)
  {
   case PILOTWIRE_DISABLED:         // Disabled
     str_label = D_PILOTWIRE_DISABLED;
     break;
   case PILOTWIRE_OFF:              // Off
     str_label = D_PILOTWIRE_OFF;
     break;
   case PILOTWIRE_COMFORT:          // Comfort
     str_label = D_PILOTWIRE_COMFORT;
     break;
   case PILOTWIRE_ECO:              // Economy
     str_label = D_PILOTWIRE_ECO;
     break;
   case PILOTWIRE_FROST:            // No frost
     str_label = D_PILOTWIRE_FROST;
     break;
   case PILOTWIRE_THERMOSTAT:       // Thermostat
     str_label = D_PILOTWIRE_THERMOSTAT;
     break;
  }

  return str_label;
}

// get pilot wire state from relays state
uint8_t PilotwireGetRelayState ()
{
  uint8_t device_type;
  uint8_t relay1 = 0;
  uint8_t relay2 = 0;
  uint8_t state  = 0;
    
  // get device connexion type
  device_type = PilotwireGetDeviceType ();

  // read relay state
  relay1 = bitRead (power, 0);

  // if pilotwire connexion and 2 relays
  if ((device_type == PILOTWIRE_DEVICE_NORMAL) && (pilotwire_devices_present > 1))
  {
    // read second relay state and convert to pilotwire state
    relay2 = bitRead (power, 1);
    if (relay1 == 0 && relay2 == 0) state = PILOTWIRE_COMFORT;
    else if (relay1 == 0 && relay2 == 1) state = PILOTWIRE_OFF;
    else if (relay1 == 1 && relay2 == 0) state = PILOTWIRE_FROST;
    else if (relay1 == 1 && relay2 == 1) state = PILOTWIRE_ECO;
  }

  // else, if pilotwire connexion and 1 relay
  else if (device_type == PILOTWIRE_DEVICE_NORMAL)
  {
    // convert to pilotwire state
    if (relay1 == 0) state = PILOTWIRE_COMFORT;
    else state = PILOTWIRE_OFF;
  }

  // else, direct connexion
  else
  {
    // convert to pilotwire state
    if (relay1 == 0) state = PILOTWIRE_OFF;
    else state = PILOTWIRE_COMFORT;
  }

  return state;
}

// set relays state
void PilotwireSetRelayState (uint8_t new_state)
{
  uint8_t device_type;

  // get device connexion type
  device_type = PilotwireGetDeviceType ();

  // set number of relay to start command
  devices_present = pilotwire_devices_present;

  // if pilotwire connexion and 2 relays
  if ((device_type == PILOTWIRE_DEVICE_NORMAL) && (pilotwire_devices_present > 1))
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
  devices_present = 0;
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
  pilotwire_updated = true;
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
  if ((pilotwire_devices_present == 1) || (device_type == PILOTWIRE_DEVICE_DIRECT))
  {
    if (new_mode == PILOTWIRE_ECO)   new_mode = PILOTWIRE_COMFORT;
    if (new_mode == PILOTWIRE_FROST) new_mode = PILOTWIRE_OFF;
  }

  // if within range, set mode
  if (new_mode <= PILOTWIRE_OFFLOAD) Settings.weight_reference = (unsigned long) new_mode;

  // update JSON status
  pilotwire_updated = true;
}

// set pilot wire minimum temperature
void PilotwireSetMinTemperature (float new_temperature)
{
  // if within range, save temperature correction
  if ((new_temperature >= PILOTWIRE_TEMP_MIN) && (new_temperature <= PILOTWIRE_TEMP_MAX)) Settings.weight_item = (unsigned long) int (new_temperature * 10);

  // update JSON status
  pilotwire_updated = true;
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
  pilotwire_updated = true;
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
  pilotwire_outside_mode = false;

  // update JSON status
  pilotwire_updated = true;
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
  pilotwire_updated = true;
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
  pilotwire_updated = true;
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
  float   temperature = NAN;

  // read temperature from local sensor
  pilotwire_temperature_source = PILOTWIRE_SOURCE_LOCAL;

#ifdef USE_DS18x20
  // read from DS18B20 sensor
  index = ds18x20_sensor[0].index;
  if (ds18x20_sensor[index].valid) temperature = ds18x20_sensor[index].temperature;
#endif // USE_DS18x20

#ifdef USE_DHT
  // read from DHT sensor
  if (isnan (temperature)) temperature = Dht[0].t;
#endif // USE_DHT

  // if not available, read MQTT temperature
  if (isnan (temperature))
  {
    pilotwire_temperature_source = PILOTWIRE_SOURCE_REMOTE;
    temperature = TemperatureGetValue ();
  }

  // if temperature is available, apply correction
  if (!isnan (temperature))
  {
    // trunc temperature to 0.2°C
    temperature = ceil (temperature * 10) / 10;

    // add correction
    temperature += PilotwireGetDrift ();
  }

  // else, no temperature source available
  else pilotwire_temperature_source = PILOTWIRE_SOURCE_NONE;

  return temperature;
}

// get current target temperature (handling outside dropdown)
float PilotwireGetCurrentTarget ()
{
  float  temp_target;

  // get target temperature
  temp_target = PilotwireGetTargetTemperature ();

  // if outside mode enabled, substract outside mode dropdown
  if (pilotwire_outside_mode == true) temp_target -= PilotwireGetOutsideDropdown ();

  return temp_target;
}

void PilotwireHandleTimer (uint8_t state)
{
  // set outside mode according to timer state (OFF = outside mode)
  pilotwire_outside_mode = (state == 0);
}

// Show JSON status (for MQTT)
void PilotwireShowJSON (bool append)
{
  uint8_t  actual_mode;
  float    actual_temperature;
  String   str_temperature, str_label, str_json, str_mqtt;

  // get temperature and mode
  actual_temperature = PilotwireGetTemperature ();
  actual_mode = PilotwireGetMode ();
  str_label   = PilotwireGetStateLabel (actual_mode);
 
  // pilotwire mode  -->  "PilotWire":{"Relay":2,"Mode":5,"Label":"Thermostat",Offload:"ON","Temperature":20.5,"Target":21,"Min"=15.5,"Max"=25}
  str_json = "\"" + String (D_JSON_PILOTWIRE) + "\":{";

  // thermostat data
  str_json += "\"" + String (D_JSON_PILOTWIRE_TARGET) + "\":" + String (PilotwireGetTargetTemperature (), 1) + ",";
  str_json += "\"" + String (D_JSON_PILOTWIRE_DRIFT) + "\":" + String (PilotwireGetDrift (), 1) + ",";
  str_json += "\"" + String (D_JSON_PILOTWIRE_MIN) + "\":" + String (PilotwireGetMinTemperature (), 1) + ",";
  str_json += "\"" + String (D_JSON_PILOTWIRE_MAX) + "\":" + String (PilotwireGetMaxTemperature (), 1) + ",";

  // outside mode
  str_json += "\"" + String (D_JSON_PILOTWIRE_OUTSIDE) + "\":";
  if (pilotwire_outside_mode == true) str_json += "\"ON\",";
  else str_json += "\"OFF\",";

  // if defined, add current temperature
  if (actual_temperature != NAN) str_json += "\"" + String (D_JSON_PILOTWIRE_TEMPERATURE) + "\":" + String (actual_temperature, 1) + ",";

  // current status
  str_json += "\"" + String (D_JSON_PILOTWIRE_RELAY) + "\":" + String (pilotwire_devices_present) + ",";
  str_json += "\"" + String (D_JSON_PILOTWIRE_MODE) + "\":" + String (actual_mode) + ",";
  str_json += "\"" + String (D_JSON_PILOTWIRE_LABEL) + "\":\"" + str_label + "\"}";

  // if pilot wire mode is enabled
  if (actual_mode != PILOTWIRE_DISABLED)
  {
    // get mode and associated label
    actual_mode = PilotwireGetRelayState ();
    str_label   = PilotwireGetStateLabel (actual_mode);

    // relay state  -->  ,"State":{"Mode":4,"Label":"Comfort"}
    str_json += ",\"" + String (D_JSON_PILOTWIRE_STATE) + "\":{";
    str_json += "\"" + String (D_JSON_PILOTWIRE_MODE) + "\":" + String (actual_mode);
    str_json += ",\"" + String (D_JSON_PILOTWIRE_LABEL) + "\":\"" + str_label + "\"}";
  }

  // generate MQTT message according to append mode
  if (append == true) str_mqtt = mqtt_data + String (",") + str_json;
  else str_mqtt = "{" + str_json + "}";

  // place JSON back to MQTT data and publish it if not in append mode
  snprintf_P (mqtt_data, sizeof(mqtt_data), str_mqtt.c_str ());
  if (append == false) MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));

  // updates have been published
  pilotwire_updated = false; 
}

// Handle pilot wire MQTT commands
bool PilotwireMqttCommand ()
{
  bool command_handled = true;
  int  command_code;
  char command [CMDSZ];

  // check MQTT command
  command_code = GetCommandCode (command, sizeof(command), XdrvMailbox.topic, kPilotWireCommands);

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
  if (!isnan (pilotwire_graph_temperature))
  {
    // set current temperature
    value = pilotwire_graph_temperature * 10;
    arr_temperature[pilotwire_graph_index] = (short)value;

    // set target temperature
    value = pilotwire_graph_target * 10;
    arr_target[pilotwire_graph_index] = (short)value;

    // set pilotwire state
    arr_state[pilotwire_graph_index] = pilotwire_graph_state;
  }

  // init graph values
  pilotwire_graph_temperature = NAN;
  pilotwire_graph_target      = NAN;
  pilotwire_graph_state       = PILOTWIRE_OFF;

  // increase temperature data index and reset if max reached
  pilotwire_graph_index ++;
  pilotwire_graph_index = pilotwire_graph_index % PILOTWIRE_GRAPH_SAMPLE;
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
  if (!isnan(heater_temperature))
  {
    // set new temperature
    pilotwire_temperature = heater_temperature;

    // update graph temperature for current slot
    if (!isnan(pilotwire_graph_temperature)) pilotwire_graph_temperature = min (pilotwire_graph_temperature, heater_temperature);
    else pilotwire_graph_temperature = heater_temperature;
  }

  // update target temperature
  if (!isnan(target_temperature))
  {
    // if temperature change, data update
    if (target_temperature != pilotwire_target) pilotwire_updated = true;
    pilotwire_target = target_temperature;
    
    // update graph target temperature for current slot
    if (!isnan(pilotwire_graph_target)) pilotwire_graph_target = min (pilotwire_graph_target, target_temperature);
    else pilotwire_graph_target = target_temperature;
  } 

  // if relay change, data update
  if (heater_state != pilotwire_state) pilotwire_updated = true;
  pilotwire_state = heater_state;

  // update graph relay state  for current slot
  if (pilotwire_graph_state != PILOTWIRE_COMFORT) pilotwire_graph_state = heater_state;

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
      pilotwire_updated = true;
    }
  }

  // if needed, publish new state
  if (pilotwire_updated == true) PilotwireShowJSON (false);
}

void PilotwireEverySecond ()
{
  // increment delay counter and if delay reached, update history data
  if (pilotwire_graph_counter == 0) PilotwireUpdateGraph ();
  pilotwire_graph_counter ++;
  pilotwire_graph_counter = pilotwire_graph_counter % PILOTWIRE_GRAPH_REFRESH;
}

void PilotwireInit ()
{
  int index;

  // save number of devices and set it to 0
  pilotwire_devices_present = devices_present;
  devices_present = 0;

  // offload : module is not managing the relay
  OffloadSetRelayMode (false);

  // offload : declare device types
  for (index = 0; index < OFFLOAD_PRIORITY_MAX; index++) OffloadSetAvailableDevice (index, OFFLOAD_PRIORITY_MAX);
  OffloadSetAvailableDevice (0, OFFLOAD_PRIORITY_ROOM);
  OffloadSetAvailableDevice (1, OFFLOAD_PRIORITY_LIVING);
  OffloadSetAvailableDevice (2, OFFLOAD_PRIORITY_BATHROOM);
  OffloadSetAvailableDevice (3, OFFLOAD_PRIORITY_KITCHEN);

  // init default values
  pilotwire_temperature_source = PILOTWIRE_SOURCE_NONE;
  pilotwire_updated            = false;
  pilotwire_outside_mode       = false;
  pilotwire_temperature        = NAN;
  pilotwire_target             = NAN;
  pilotwire_state              = PILOTWIRE_OFF;
  pilotwire_graph_temperature  = NAN;
  pilotwire_graph_target       = NAN;
  pilotwire_graph_state        = PILOTWIRE_OFF;
  pilotwire_graph_index        = 0;
  pilotwire_graph_counter      = 0;
  pilotwire_langage            = PILOTWIRE_LANGAGE_ENGLISH;

  // initialise temperature graph
  for (index = 0; index < PILOTWIRE_GRAPH_SAMPLE; index++)
  {
    arr_temperature[index] = SHRT_MAX;
    arr_target[index]      = SHRT_MAX;
    arr_state[index]       = PILOTWIRE_DISABLED;
  }
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// display offload icons
void PilotwireWebIconOff ()      { Webserver->send (200, "image/png", pilotwire_off_png,       pilotwire_off_len);       }
void PilotwireWebIconComfort ()  { Webserver->send (200, "image/png", pilotwire_comfort_png,   pilotwire_comfort_len);   }
void PilotwireWebIconEco ()      { Webserver->send (200, "image/png", pilotwire_eco_png,       pilotwire_eco_len);       }
void PilotwireWebIconFrost ()    { Webserver->send (200, "image/png", pilotwire_frost_png,     pilotwire_frost_len);     }
void PilotwireWebIconThermOff () { Webserver->send (200, "image/png", pilotwire_therm_off_png, pilotwire_therm_off_len); }
void PilotwireWebIconThermOn  () { Webserver->send (200, "image/png", pilotwire_therm_on_png,  pilotwire_therm_on_len);  }

// get status icon
String PilotwireWebGetStatusIcon ()
{
  bool    device_offload;
  uint8_t device_mode, device_state;
  String  str_icon;

  // get device offload status, mode, state and target temperature
  device_offload = OffloadIsOffloaded ();
  device_mode    = PilotwireGetMode ();
  device_state   = PilotwireGetRelayState ();

  if (device_offload) str_icon = "offloaded.png";
  else if (device_mode == PILOTWIRE_THERMOSTAT)
  {
    // display icon according to relay state
    switch (device_state)
    {
      case PILOTWIRE_OFF:
      case PILOTWIRE_FROST:
        str_icon = "therm-off.png";
        break;
      case PILOTWIRE_ECO:
      case PILOTWIRE_COMFORT:
        str_icon = "therm-on.png";
        break;
    }
  }
  else
  {
    // display icon according to relay state
    switch (device_state)
    {
      case PILOTWIRE_OFF:
        str_icon = "off.png";
        break;
      case PILOTWIRE_COMFORT:
        str_icon = "comfort.png";
        break;
      case PILOTWIRE_ECO:
        str_icon = "economy.png";
        break;
      case PILOTWIRE_FROST:
        str_icon = "frost.png";
        break;
    }
  }

  return str_icon;
}

// append pilotwire control button to main page
void PilotwireWebMainButton ()
{
  float  temperature;
  String str_text;

  // heater mode switch button
  temperature = PilotwireGetOutsideDropdown ();
  if (pilotwire_outside_mode == true) str_text = String (D_PILOTWIRE_NORMAL) + " " + String (D_PILOTWIRE_MODE);
  else str_text = String (D_PILOTWIRE_OUTSIDE) + " " + String (D_PILOTWIRE_MODE) + " (-" + String (temperature, 1) + "°" + String (TempUnit ()) + ")";
  WSContentSend_P (D_CONF_BUTTON, D_PAGE_PILOTWIRE_SWITCH, str_text.c_str());

  // heater control page button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_PAGE_PILOTWIRE_CONTROL, D_PILOTWIRE_CONTROL);
}

// append pilotwire configuration button to configuration page
void PilotwireWebButton ()
{
  String str_text;

  // heater configuration button
  str_text = D_PILOTWIRE_CONFIGURE + String (" ") + D_PILOTWIRE;
  WSContentSend_P (D_CONF_BUTTON, D_PAGE_PILOTWIRE_CONFIG, str_text.c_str ());
}

// append pilot wire state to main page
bool PilotwireWebSensor ()
{
  bool    is_offloaded;
  uint8_t actual_mode, icon_index;
  float   temperature;
  String  str_title, str_text, str_label, str_color;

  // get offloaded status
  is_offloaded = OffloadIsOffloaded ();

  // get current mode and temperature
  actual_mode = PilotwireGetMode ();
  temperature = PilotwireGetTemperature ();

  // handle sensor source
  str_title = D_PILOTWIRE_TEMPERATURE;
  switch (pilotwire_temperature_source)
  {
    case PILOTWIRE_SOURCE_NONE:  // no temperature source available 
      str_text  = "<b>---</b>";
      break;
    case PILOTWIRE_SOURCE_LOCAL:  // local temperature source used 
      str_title += " <small><i>(" + String (D_PILOTWIRE_LOCAL) + ")</i></small>";
      str_text  = "<b>" + String (temperature, 1) + "</b>";
      break;
    case PILOTWIRE_SOURCE_REMOTE:  // remote temperature source used 
      str_title += " <small><i>(" + String (D_PILOTWIRE_REMOTE) + ")</i></small>";
      str_text  = "<b>" + String (temperature, 1) + "</b>";
      break;
  }

  // add target temperature and unit
  if (actual_mode == PILOTWIRE_THERMOSTAT) str_text += " / " + String (PilotwireGetCurrentTarget (), 1);
  str_text += " °" + String (TempUnit ());

  // display temperature
  WSContentSend_PD ("{s}%s{m}%s{e}\n", str_title.c_str (), str_text.c_str ());

  // display day or outside mode
  temperature = PilotwireGetOutsideDropdown ();
  if (pilotwire_outside_mode == true) str_text = "<span><b>" + String (D_PILOTWIRE_OUTSIDE) + "</b> (-" + String (temperature, 1) + "°" + String (TempUnit ()) + ")</span>";
  else str_text = "<span><b>" + String (D_PILOTWIRE_NORMAL) + "</b></span>";
  WSContentSend_PD (PSTR("{s}%s{m}%s{e}\n"), D_PILOTWIRE_MODE, str_text.c_str ());

  // display heater icon status
  WSContentSend_PD (PSTR("<tr><td colspan=2 style='width:100%;text-align:center;padding:10px;'>"));
  WSContentSend_P (PSTR ("<img height=64 src='%s'>"), PilotwireWebGetStatusIcon ().c_str ());
  WSContentSend_PD (PSTR("</td></tr>\n"));
}

// Pilot Wire heater mode switch page
void PilotwireWebPageSwitchMode ()
{
  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // invert mode
  pilotwire_outside_mode = !pilotwire_outside_mode;

  // page header with dark background
  WSContentStart_P (D_PILOTWIRE_CONTROL, false);
  WSContentSend_P (PSTR ("</script>\n"));
  WSContentSend_P (PSTR ("<meta http-equiv='refresh' content='0;URL=/' />\n"));
  WSContentSend_P (PSTR ("</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body bgcolor='#303030' >\n"));
  WSContentStop ();
}

// Pilotwire heater configuration web page
void PilotwireWebPageConfigure ()
{
  bool    is_modified;
  uint8_t device_mode, device_type;
  float   device_temp;
  char    argument[PILOTWIRE_BUFFER_SIZE];
  String  str_text, str_step, str_unit;

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // get temperature and target mode
  device_temp = PilotwireGetTemperature ();
  device_mode = PilotwireGetMode ();
  device_type = PilotwireGetDeviceType ();
  str_unit    = String (TempUnit ());
  str_step    = String (PILOTWIRE_TEMP_STEP, 1);

  // page comes from save button on configuration page
  if (Webserver->hasArg("save"))
  {
    // get pilot wire mode according to 'mode' parameter
    WebGetArg (D_CMND_PILOTWIRE_MODE, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetMode ((uint8_t)atoi (argument)); 

    // get pilot wire device type according to 'device' parameter
    WebGetArg (D_CMND_PILOTWIRE_DEVICE, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetDeviceType ((uint8_t)atoi (argument)); 

    // get minimum temperature according to 'min' parameter
    WebGetArg (D_CMND_PILOTWIRE_MIN, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetMinTemperature (atof (argument));

    // get maximum temperature according to 'max' parameter
    WebGetArg (D_CMND_PILOTWIRE_MAX, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetMaxTemperature (atof (argument));

    // get target temperature according to 'target' parameter
    WebGetArg (D_CMND_PILOTWIRE_TARGET, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetTargetTemperature (atof (argument));

    // get outside mode dropdown temperature according to 'outside' parameter
    WebGetArg (D_CMND_PILOTWIRE_OUTSIDE, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetOutsideDropdown (abs (atof (argument)));

    // get temperature drift according to 'drift' parameter
    WebGetArg (D_CMND_PILOTWIRE_DRIFT, argument, PILOTWIRE_BUFFER_SIZE);
    if (strlen(argument) > 0) PilotwireSetDrift (atof (argument));

    // set ds18b20 pullup according to 'pullup' parameter
    WebGetArg (D_CMND_PILOTWIRE_PULLUP, argument, PILOTWIRE_BUFFER_SIZE);
    is_modified = PilotwireSetDS18B20Pullup (strlen(argument) > 0);
    if (is_modified == true) WebRestart (1);
  }

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
  if (device_type == PILOTWIRE_DEVICE_NORMAL) str_text = D_PILOTWIRE_CHECKED; else str_text = "";
  WSContentSend_P (PSTR ("<input type='radio' id='normal' name='%s' value=%d %s>%s<br>\n"), D_CMND_PILOTWIRE_DEVICE, PILOTWIRE_DEVICE_NORMAL, str_text.c_str (), D_PILOTWIRE);
  if (device_type == PILOTWIRE_DEVICE_DIRECT) str_text = D_PILOTWIRE_CHECKED; else str_text = "";
  WSContentSend_P (PSTR ("<input type='radio' id='direct' name='%s' value=%d %s>%s<br>\n"), D_CMND_PILOTWIRE_DEVICE, PILOTWIRE_DEVICE_DIRECT, str_text.c_str (), D_PILOTWIRE_DIRECT);

  WSContentSend_P (PSTR ("</p>\n"));
  WSContentSend_P (D_CONF_FIELDSET_STOP);

  //    Mode 
  // ----------------

  WSContentSend_P (D_CONF_FIELDSET_START, D_PILOTWIRE_MODE);
  WSContentSend_P (PSTR ("<p><span class='key'>%s</span>\n"), D_CMND_PILOTWIRE_MODE);

  // seletion : disabled
  if (device_mode == PILOTWIRE_DISABLED) str_text = D_PILOTWIRE_CHECKED; else str_text = "";
  WSContentSend_P (D_CONF_MODE_SELECT, D_CMND_PILOTWIRE_MODE, PILOTWIRE_DISABLED, PILOTWIRE_DISABLED, str_text.c_str (), D_PILOTWIRE_DISABLED);

  // selection : off
  if (device_mode == PILOTWIRE_OFF) str_text = D_PILOTWIRE_CHECKED; else str_text = "";
  WSContentSend_P (D_CONF_MODE_SELECT, D_CMND_PILOTWIRE_MODE, PILOTWIRE_OFF, PILOTWIRE_OFF, str_text.c_str (), D_PILOTWIRE_OFF);

  // if dual relay device
  if ((pilotwire_devices_present > 1) && (device_type == PILOTWIRE_DEVICE_NORMAL))
  {
    // selection : no frost
    if (device_mode == PILOTWIRE_FROST) str_text = D_PILOTWIRE_CHECKED; else str_text = "";
    WSContentSend_P (D_CONF_MODE_SELECT, D_CMND_PILOTWIRE_MODE, PILOTWIRE_FROST, PILOTWIRE_FROST, str_text.c_str (), D_PILOTWIRE_FROST);

    // selection : eco
    if (device_mode == PILOTWIRE_ECO) str_text = D_PILOTWIRE_CHECKED; else str_text = "";
    WSContentSend_P (D_CONF_MODE_SELECT, D_CMND_PILOTWIRE_MODE, PILOTWIRE_ECO, PILOTWIRE_ECO, str_text.c_str (), D_PILOTWIRE_ECO);
  }

  // selection : comfort
  if (device_mode == PILOTWIRE_COMFORT) str_text = D_PILOTWIRE_CHECKED; else str_text = "";
  WSContentSend_P (D_CONF_MODE_SELECT, D_CMND_PILOTWIRE_MODE, PILOTWIRE_COMFORT, PILOTWIRE_COMFORT, str_text.c_str (), D_PILOTWIRE_COMFORT);

  // selection : thermostat
  if (!isnan(device_temp)) 
  {
    // selection : target temperature
    if (device_mode == PILOTWIRE_THERMOSTAT) str_text = D_PILOTWIRE_CHECKED; else str_text = "";
    WSContentSend_P (D_CONF_MODE_SELECT, D_CMND_PILOTWIRE_MODE, PILOTWIRE_THERMOSTAT, PILOTWIRE_THERMOSTAT, str_text.c_str (), D_PILOTWIRE_THERMOSTAT); 
  }

  WSContentSend_P (PSTR ("</p>\n"));
  WSContentSend_P (D_CONF_FIELDSET_STOP);

  //   Thermostat 
  // --------------

  WSContentSend_P (D_CONF_FIELDSET_START, D_PILOTWIRE_THERMOSTAT);

  // if temperature is available
  if (!isnan (device_temp)) 
  {
    WSContentSend_P (PSTR ("<p>\n"));

    // target temperature
    str_text = String (PilotwireGetTargetTemperature (), 1);
    WSContentSend_P (D_CONF_TEMPERATURE, D_PILOTWIRE_TARGET, str_unit.c_str (), D_CMND_PILOTWIRE_TARGET, D_CMND_PILOTWIRE_TARGET, PILOTWIRE_TEMP_MIN, PILOTWIRE_TEMP_MAX, str_step.c_str(), str_text.c_str());

    // outside mode temperature dropdown
    str_text = "-" + String (PilotwireGetOutsideDropdown (), 1);
    WSContentSend_P (D_CONF_TEMPERATURE, D_PILOTWIRE_DROPDOWN, str_unit.c_str (), D_CMND_PILOTWIRE_OUTSIDE, D_CMND_PILOTWIRE_OUTSIDE, - PILOTWIRE_TEMP_DROP, 0, str_step.c_str(), str_text.c_str());

    WSContentSend_P (PSTR ("<p></p>\n"));

    // temperature minimum label and input
    str_text = String (PilotwireGetMinTemperature (), 1);
    WSContentSend_P (D_CONF_TEMPERATURE, D_PILOTWIRE_MIN, str_unit.c_str (), D_CMND_PILOTWIRE_MIN, D_CMND_PILOTWIRE_MIN, PILOTWIRE_TEMP_MIN, PILOTWIRE_TEMP_MAX, str_step.c_str(), str_text.c_str());

    // temperature maximum label and input
    str_text = String (PilotwireGetMaxTemperature (), 1);
    WSContentSend_P (D_CONF_TEMPERATURE, D_PILOTWIRE_MAX, str_unit.c_str (), D_CMND_PILOTWIRE_MAX, D_CMND_PILOTWIRE_MAX, PILOTWIRE_TEMP_MIN, PILOTWIRE_TEMP_MAX, str_step.c_str(), str_text.c_str());

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
    str_text = String (PilotwireGetDrift (), 1);
    str_step = String (PILOTWIRE_TEMP_DRIFT_STEP, 1);
    WSContentSend_P (D_CONF_TEMPERATURE, D_PILOTWIRE_DRIFT, str_unit.c_str (), D_CMND_PILOTWIRE_DRIFT, D_CMND_PILOTWIRE_DRIFT, - PILOTWIRE_TEMP_DRIFT, PILOTWIRE_TEMP_DRIFT, str_step.c_str(), str_text.c_str());
  }

  // pullup option for ds18b20 sensor
  if (PilotwireGetDS18B20Pullup ()) str_text = D_PILOTWIRE_CHECKED; else str_text = "";
  WSContentSend_P (PSTR ("<hr>\n"));
  WSContentSend_P (PSTR ("<p><input type='checkbox' name='%s' %s>%s</p>\n"), D_CMND_PILOTWIRE_PULLUP, str_text.c_str(), D_PILOTWIRE_PULLUP);

  WSContentSend_P (PSTR ("</p>\n"));
  WSContentSend_P (D_CONF_FIELDSET_STOP);
  WSContentSend_P (PSTR("<br>\n"));

  // save button
  WSContentSend_P (PSTR ("<p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), D_SAVE);
  WSContentSend_P (PSTR("</form>\n"));

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

// Temperature graph frame
void PilotwireWebGraphFrame ()
{
  int    index;
  float  temp_min, temp_max, value;
  String str_text;

  // start of SVG graph
  WSContentBegin(200, CT_HTML);
  WSContentSend_P (PSTR ("<svg viewBox='0 0 %d %d' preserveAspectRatio='xMinYMinmeet'>\n"), PILOTWIRE_GRAPH_WIDTH, PILOTWIRE_GRAPH_HEIGHT);

  // SVG style 
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));
  WSContentSend_P (PSTR ("rect {stroke:grey;fill:none;}\n"));
  WSContentSend_P (PSTR ("line.dash {stroke:grey;stroke-dasharray:8;}\n"));
  WSContentSend_P (PSTR ("text.temperature {font-size:18px;fill:yellow;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // graph curve zone
  WSContentSend_P (PSTR ("<rect x='%d%%' y='0%%' width='%d%%' height='100%%' rx='10' />\n"), PILOTWIRE_GRAPH_PERCENT_START, PILOTWIRE_GRAPH_PERCENT_STOP - PILOTWIRE_GRAPH_PERCENT_START);

  // loop to adjust min and max temperature
  temp_min = PilotwireGetMinTemperature ();
  temp_max = PilotwireGetMaxTemperature ();
  for (index = 0; index < PILOTWIRE_GRAPH_SAMPLE; index++)
    if (arr_temperature[index] != SHRT_MAX)
    {
      // read indexed temperature
      value = (float)arr_temperature[index];
      value = value / 10;

      // update minimum and maximum
      if (temp_min > value) temp_min = floor (value);
      if (temp_max < value) temp_max = ceil  (value);
    }
  
  // graph temperature units
  str_text = String (temp_max, 1);
  WSContentSend_P (D_GRAPH_TEMPERATURE, 1, 4, str_text.c_str ());
  str_text = String (temp_min + (temp_max - temp_min) * 0.75, 1);
  WSContentSend_P (D_GRAPH_TEMPERATURE, 1, 27, str_text.c_str ());
  str_text = String (temp_min + (temp_max - temp_min) * 0.50, 1);
  WSContentSend_P (D_GRAPH_TEMPERATURE, 1, 52, str_text.c_str ());
  str_text = String (temp_min + (temp_max - temp_min) * 0.25, 1);
  WSContentSend_P (D_GRAPH_TEMPERATURE, 1, 77, str_text.c_str ());
  str_text = String (temp_min, 1);
  WSContentSend_P (D_GRAPH_TEMPERATURE, 1, 99, str_text.c_str ());

  // graph separation lines
  WSContentSend_P (D_GRAPH_SEPARATION, PILOTWIRE_GRAPH_PERCENT_START, 25, PILOTWIRE_GRAPH_PERCENT_STOP, 25);
  WSContentSend_P (D_GRAPH_SEPARATION, PILOTWIRE_GRAPH_PERCENT_START, 50, PILOTWIRE_GRAPH_PERCENT_STOP, 50);
  WSContentSend_P (D_GRAPH_SEPARATION, PILOTWIRE_GRAPH_PERCENT_START, 75, PILOTWIRE_GRAPH_PERCENT_STOP, 75);

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentEnd();
}

// Temperature graph data
void PilotwireWebGraphData ()
{
  int      index, array_index;
  int      unit_width, shift_unit, shift_width;
  int      graph_x, graph_left, graph_right, graph_width, graph_low, graph_high;
  float    graph_y, value;
  float    temperature, temp_min, temp_max, temp_scope;
  TIME_T   current_dst;
  uint32_t current_time;
  String   str_text;

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
  WSContentSend_P (PSTR ("rect.temperature {fill:#333;stroke-width:2;opacity:0.5;stroke:yellow;}\n"));
  WSContentSend_P (PSTR ("text.temperature {font-size:36px;stroke:yellow;fill:yellow;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // loop to adjust min and max temperature
  temp_min = PilotwireGetMinTemperature ();
  temp_max = PilotwireGetMaxTemperature ();
  for (index = 0; index < PILOTWIRE_GRAPH_SAMPLE; index++)
    if (arr_temperature[index] != SHRT_MAX)
    {
      // read indexed temperature
      value = (float)arr_temperature[index];
      value = value / 10; 

      // adjust minimum and maximum temperature
      if (temp_min > value) temp_min = floor (value);
      if (temp_max < value) temp_max = ceil  (value);
    }
  temp_scope = temp_max - temp_min;

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
    array_index = (index + pilotwire_graph_index) % PILOTWIRE_GRAPH_SAMPLE;
    if (arr_state[array_index] != PILOTWIRE_DISABLED)
    {
      // calculate end point x and y
      graph_x = graph_left + (graph_width * index / PILOTWIRE_GRAPH_SAMPLE);

      // add the point to the line
      if (arr_state[array_index] == PILOTWIRE_COMFORT) WSContentSend_P (PSTR("%d,%d "), graph_x, graph_high);
      else WSContentSend_P (PSTR("%d,%d "), graph_x, graph_low);
    }
  }
  WSContentSend_P (PSTR("'/>\n"));

  // ----------------------------
  //   Target Temperature curve
  // ----------------------------

  // loop for the target temperature graph
  WSContentSend_P (PSTR ("<polyline class='target' points='"));
  for (index = 0; index < PILOTWIRE_GRAPH_SAMPLE; index++)
  {
    // get target temperature value and set to minimum if not defined
    array_index = (index + pilotwire_graph_index) % PILOTWIRE_GRAPH_SAMPLE;
    if (arr_target[array_index] != SHRT_MAX)
    {
      // read indexed temperature
      value = (float)arr_target[array_index];
      value = value / 10;

      // calculate end point x and y
      graph_x = graph_left + (graph_width * index / PILOTWIRE_GRAPH_SAMPLE);
      graph_y = (1 - ((value - temp_min) / temp_scope)) * PILOTWIRE_GRAPH_HEIGHT;

      // add the point to the line
      WSContentSend_P (PSTR("%d,%d "), graph_x, int (graph_y));
    }
  }
  WSContentSend_P (PSTR("'/>\n"));

  // ---------------------
  //   Temperature curve
  // ---------------------

  // loop for the temperature graph
  WSContentSend_P (PSTR ("<polyline class='temperature' points='"));
  for (index = 0; index < PILOTWIRE_GRAPH_SAMPLE; index++)
  {
    // if temperature value is defined
    array_index = (index + pilotwire_graph_index) % PILOTWIRE_GRAPH_SAMPLE;
    if (arr_temperature[array_index] != SHRT_MAX)
    {
      // read indexed temperature
      value = (float)arr_temperature[array_index];
      value = value / 10;

      // calculate end point x and y
      graph_x = graph_left + (graph_width * index / PILOTWIRE_GRAPH_SAMPLE);
      graph_y  = (1 - ((value - temp_min) / temp_scope)) * PILOTWIRE_GRAPH_HEIGHT;

      // add the point to the line
      WSContentSend_P (PSTR("%d,%d "), graph_x, int (graph_y));
    }
  }
  WSContentSend_P (PSTR("'/>\n"));

  // ---------------
  //   Time line
  // ---------------

  // get current time
  current_time = LocalTime();
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

  // -----------------
  //    Temperature
  // -----------------

  // get current temperature
  temperature = PilotwireGetTemperature ();
  if (!isnan (temperature))
  {
    // get temperature with 1 digit
    str_text = String (temperature, 1) + " °C";

    // calculate centered data position
    graph_x = PILOTWIRE_GRAPH_PERCENT_START + 38 - str_text.length ();
    graph_y = 86;
    unit_width = 5 + 2 * str_text.length ();
    WSContentSend_P ("<rect class='temperature' x='%d%%' y='%d%%' width='%d%%' height='%d%%' rx='%d' ry='%d' />\n", graph_x, (int) graph_y, unit_width, 10, 10, 10);
    WSContentSend_P ("<text class='temperature' x='%d%%' y='%d%%'>%s</text>\n", graph_x + 2, (int) graph_y + 8, str_text.c_str ());
  }

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentEnd();
}

// Pilot Wire heater public configuration web page
void PilotwireWebPageControl ()
{
  int     index;
  uint8_t device_mode;
  float   device_temp;
  char    argument[PILOTWIRE_BUFFER_SIZE];
  String  str_text;

  // get target temperature
  device_temp = PilotwireGetTargetTemperature ();

  // if langage is changed
  if (Webserver->hasArg(D_CMND_PILOTWIRE_LANG))
  {
    // get langage according to 'lang' parameter
    WebGetArg (D_CMND_PILOTWIRE_LANG, argument, PILOTWIRE_BUFFER_SIZE);

    if (strcmp (argument, D_PILOTWIRE_ENGLISH) == 0) pilotwire_langage = PILOTWIRE_LANGAGE_ENGLISH;
    else if (strcmp (argument, D_PILOTWIRE_FRENCH) == 0) pilotwire_langage = PILOTWIRE_LANGAGE_FRENCH;
  }

  // if heater has to be switched off, set in anti-frost mode
  if (Webserver->hasArg(D_CMND_PILOTWIRE_OFF)) PilotwireSetMode (PILOTWIRE_FROST); 

  // else, if heater has to be switched on, set in thermostat mode
  else if (Webserver->hasArg(D_CMND_PILOTWIRE_ON)) PilotwireSetMode (PILOTWIRE_THERMOSTAT);

  // else, if target temperature has been changed
  else if (Webserver->hasArg(D_CMND_PILOTWIRE_MINUSMINUS)) PilotwireSetTargetTemperature (device_temp - 2  );
  else if (Webserver->hasArg(D_CMND_PILOTWIRE_MINUS))      PilotwireSetTargetTemperature (device_temp - 0.5);
  else if (Webserver->hasArg(D_CMND_PILOTWIRE_PLUS))       PilotwireSetTargetTemperature (device_temp + 0.5);
  else if (Webserver->hasArg(D_CMND_PILOTWIRE_PLUSPLUS))   PilotwireSetTargetTemperature (device_temp + 2  );

  // update heater status
  PilotwireUpdateStatus ();

  // get device mode and target temperature
  device_mode = PilotwireGetMode ();
  device_temp = PilotwireGetTargetTemperature ();

  // beginning of form without authentification with 60 seconds auto refresh
  WSContentStart_P (D_PILOTWIRE_CONTROL, false);
  WSContentSend_P (PSTR ("</script>\n"));

  // page data refresh script
  WSContentSend_P (PSTR ("<script type='text/javascript'>\n"));
  WSContentSend_P (PSTR ("function updateData() {\n"));
  WSContentSend_P (PSTR ("dataId='data';\n"));
  WSContentSend_P (PSTR ("now=new Date();\n"));
  WSContentSend_P (PSTR ("svgObject=document.getElementById(dataId);\n"));
  WSContentSend_P (PSTR ("svgObjectURL=svgObject.data;\n"));
  WSContentSend_P (PSTR ("svgObject.data=svgObjectURL.substring(0,svgObjectURL.indexOf('ts=')) + 'ts=' + now.getTime();\n"));
  WSContentSend_P (PSTR ("}\n"));
  WSContentSend_P (PSTR ("setInterval(function() {updateData();},%d);\n"), 10000);
  WSContentSend_P (PSTR ("</script>\n"));

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("body {color:white;background-color:#303030;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {width:100%%;max-width:800px;margin:12px auto;padding:3px 0px;text-align:center;vertical-align:middle;}\n"));
  WSContentSend_P (PSTR (".title {font-size:6vw;font-weight:bold;}\n"));
  WSContentSend_P (PSTR (".lang {font-size:2vh;}\n"));
  WSContentSend_P (PSTR ("button {font-size:4vw;margin:0.25rem 0.5rem;padding:0.75rem 1rem;border:1px #666 solid;background:none;color:#fff;border-radius:8px;}\n"));
  WSContentSend_P (PSTR ("button.target {font-size:3vw;margin:0.25rem;padding:0.5rem 0.75rem;}\n"));
  WSContentSend_P (PSTR ("span.target {font-size:5vw;color:orange;margin:0.25rem 0.5rem;padding:0.5rem 1rem;border:1px orange solid;border-radius:8px;}\n"));
  WSContentSend_P (PSTR (".svg-container {position:relative;vertical-align:middle;overflow:hidden;width:100%%;max-width:%dpx;padding-bottom:65%%;}\n"), PILOTWIRE_GRAPH_WIDTH);
  WSContentSend_P (PSTR (".svg-content {display:inline-block;position:absolute;top:0;left:0;}\n"));
  WSContentSend_P (PSTR ("line.target {stroke:orange;stroke-dasharray:4;}\n"));
  WSContentSend_P (PSTR ("text.target {font-size:20px;fill:orange;}\n"));
  WSContentSend_P (PSTR ("</style>\n</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (PSTR ("<form name='control' method='get' action='%s'>\n"), D_PAGE_PILOTWIRE_CONTROL);

  // room name
  WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText(SET_FRIENDLYNAME1));

  // status icon
  WSContentSend_P (PSTR ("<div><img height=64 src='%s'></div>\n"), PilotwireWebGetStatusIcon ().c_str ());

  // display buttons and target temperature selection
  if (device_mode == PILOTWIRE_THERMOSTAT)
  {
    // get target temprerature
    device_temp = PilotwireGetTargetTemperature ();
    str_text = String (device_temp, 1);

      // button to switch off
    WSContentSend_P (PSTR ("<div><button name='off'>%s</button></div>\n"), arr_control_off[pilotwire_langage]);

    // target temperature selection
    WSContentSend_P (PSTR ("<div>\n"));
    WSContentSend_P (PSTR ("<button class='target' name='%s'><<</button>\n"), D_CMND_PILOTWIRE_MINUSMINUS);
    WSContentSend_P (PSTR ("<button class='target' name='%s'><</button>\n"), D_CMND_PILOTWIRE_MINUS);
    WSContentSend_P (PSTR ("<span class='target'>%s °C</span>\n"), str_text.c_str ());
    WSContentSend_P (PSTR ("<button class='target' name='%s'>></button>\n"), D_CMND_PILOTWIRE_PLUS);
    WSContentSend_P (PSTR ("<button class='target' name='%s'>>></button>\n"), D_CMND_PILOTWIRE_PLUSPLUS);
    WSContentSend_P (PSTR ("</div>\n"));
  }
  else
  {
    // button to switch on
    WSContentSend_P (PSTR ("<div><button name='on'>%s</button></div>\n"), arr_control_on[pilotwire_langage]);
  }

  // graph section
  WSContentSend_P (PSTR ("<div class='svg-container'>\n"));
  WSContentSend_P (PSTR ("<object class='svg-content' id='base' type='image/svg+xml' width='%d%%' height='%d%%' data='%s'></object>\n"), 100, 100, D_PAGE_PILOTWIRE_BASE_SVG);
  WSContentSend_P (PSTR ("<object class='svg-content' id='data' type='image/svg+xml' width='%d%%' height='%d%%' data='%s?ts=0'></object>\n"), 100, 100, D_PAGE_PILOTWIRE_DATA_SVG);
  WSContentSend_P (PSTR ("</div>\n"));

  // langage selection
  WSContentSend_P (PSTR ("<div>\n"));
  WSContentSend_P (PSTR ("<select name=lang class='lang' size=1 onChange='control.submit();'>\n"));
  for (index = 0; index < PILOTWIRE_LANGAGE_MAX; index++)
  {
    if (index == pilotwire_langage) str_text = "selected"; else str_text = "";
    WSContentSend_P (PSTR ("<option value='%s' %s>%s\n"),arr_control_lang[index], str_text.c_str (), arr_control_langage[index]);
  }
  WSContentSend_P (PSTR ("</select>\n"));
  WSContentSend_P (PSTR ("</div>\n"));

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
      Webserver->on ("/" D_PAGE_PILOTWIRE_CONFIG,   PilotwireWebPageConfigure );
      Webserver->on ("/" D_PAGE_PILOTWIRE_SWITCH,   PilotwireWebPageSwitchMode);
      Webserver->on ("/" D_PAGE_PILOTWIRE_CONTROL,  PilotwireWebPageControl   );
      Webserver->on ("/" D_PAGE_PILOTWIRE_BASE_SVG, PilotwireWebGraphFrame    );
      Webserver->on ("/" D_PAGE_PILOTWIRE_DATA_SVG, PilotwireWebGraphData     );
      Webserver->on ("/off.png",       PilotwireWebIconOff     );
      Webserver->on ("/comfort.png",   PilotwireWebIconComfort );
      Webserver->on ("/economy.png",   PilotwireWebIconEco     );
      Webserver->on ("/frost.png",     PilotwireWebIconFrost   );
      Webserver->on ("/therm-off.png", PilotwireWebIconThermOff);
      Webserver->on ("/therm-on.png",  PilotwireWebIconThermOn );
      break;
    case FUNC_WEB_SENSOR:
      PilotwireWebSensor ();
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      PilotwireWebMainButton ();
      break;
    case FUNC_WEB_ADD_BUTTON:
      PilotwireWebButton ();
      break;
#endif  // USE_Webserver

  }
  return result;
}

#endif // USE_PILOTWIRE
