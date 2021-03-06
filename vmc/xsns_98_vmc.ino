/*
  xsns_98_vmc.ino - Ventilation Motor Controled support 
  for Sonoff TH, Sonoff Basic or SonOff Dual
  
  Copyright (C) 2019 Nicolas Bernaerts
    15/03/2019 - v1.0 - Creation
    01/03/2020 - v2.0 - Functions rewrite for Tasmota 8.x compatibility
    07/03/2020 - v2.1 - Add daily humidity / temperature graph
    13/03/2020 - v2.2 - Add time on graph
    17/03/2020 - v2.3 - Handle Sonoff Dual and remote humidity sensor
    05/04/2020 - v2.4 - Add Timezone management
    15/05/2020 - v2.5 - Add /json page
    20/05/2020 - v2.6 - Add configuration for first NTP server
    26/05/2020 - v2.7 - Add Information JSON page
    15/09/2020 - v2.8 - Remove /json page, based on Tasmota 8.4
                        Add status icons and mode control
    08/10/2020 - v3.0 - Handle graph with js auto update
    18/10/2020 - v3.1 - Expose icons on web server
    30/10/2020 - v3.2 - Real time graph page update

  Settings are stored using weighting scale parameters :
    - Settings.weight_reference   = VMC mode
    - Settings.weight_max         = Target humidity level (%)
    - Settings.weight_calibration = Humidity thresold (%) 
    
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

#ifdef USE_VMC

/*********************************************************************************************\
 * Fil Pilote
\*********************************************************************************************/

#define XSNS_98                  98

// web configuration page
#define D_PAGE_VMC_CONFIG       "vmc"
#define D_PAGE_VMC_CONTROL      "control"
#define D_PAGE_VMC_BASE_SVG     "base.svg"
#define D_PAGE_VMC_DATA_SVG     "data.svg"

// commands
#define D_CMND_VMC_MODE         "mode"
#define D_CMND_VMC_TARGET       "target"
#define D_CMND_VMC_THRESHOLD    "thres"
#define D_CMND_VMC_LOW          "low"
#define D_CMND_VMC_HIGH         "high"
#define D_CMND_VMC_AUTO         "auto"

// JSON data
#define D_JSON_VMC              "VMC"
#define D_JSON_VMC_MODE         "Mode"
#define D_JSON_VMC_STATE        "State"
#define D_JSON_VMC_LABEL        "Label"
#define D_JSON_VMC_TARGET       "Target"
#define D_JSON_VMC_HUMIDITY     "Humidity"
#define D_JSON_VMC_TEMPERATURE  "Temperature"
#define D_JSON_VMC_THRESHOLD    "Threshold"
#define D_JSON_VMC_RELAY        "Relay"

#define D_VMC_MODE              "VMC Mode"
#define D_VMC_STATE             "VMC State"
#define D_VMC_CONTROL           "Control"
#define D_VMC_LOCAL             "Local"
#define D_VMC_REMOTE            "Remote"
#define D_VMC_HUMIDITY          "Humidity"
#define D_VMC_SENSOR            "Sensor"
#define D_VMC_DISABLED          "Disabled"
#define D_VMC_LOW               "Low speed"
#define D_VMC_HIGH              "High speed"
#define D_VMC_AUTO              "Automatic"
#define D_VMC_BTN_LOW           "Low"
#define D_VMC_BTN_HIGH          "High"
#define D_VMC_BTN_AUTO          "Auto"
#define D_VMC_PARAMETERS        "VMC Parameters"
#define D_VMC_TARGET            "Target Humidity"
#define D_VMC_THRESHOLD         "Humidity Threshold"
#define D_VMC_CONFIGURE         "Configure VMC"
#define D_VMC_TIME              "Time"

// graph data
#define VMC_GRAPH_WIDTH         800      
#define VMC_GRAPH_HEIGHT        500 
#define VMC_GRAPH_SAMPLE        800
#define VMC_GRAPH_REFRESH       108         // collect data every 108 sec to get 24h graph with 800 samples
#define VMC_GRAPH_MODE_LOW      20 
#define VMC_GRAPH_MODE_HIGH     40 
#define VMC_GRAPH_PERCENT_START 12      
#define VMC_GRAPH_PERCENT_STOP  88
#define VMC_GRAPH_TEMP_MIN      15      
#define VMC_GRAPH_TEMP_MAX      25       

// VMC data
#define VMC_HUMIDITY_MAX        100  
#define VMC_TARGET_MAX          99
#define VMC_TARGET_DEFAULT      50
#define VMC_THRESHOLD_MAX       10
#define VMC_THRESHOLD_DEFAULT   2

// buffer
#define VMC_BUFFER_SIZE         128

// vmc humidity source
enum VmcSources { VMC_SOURCE_NONE, VMC_SOURCE_LOCAL, VMC_SOURCE_REMOTE };

// vmc states and modes
enum VmcStates { VMC_STATE_OFF, VMC_STATE_LOW, VMC_STATE_HIGH, VMC_STATE_NONE, VMC_STATE_MAX };
enum VmcModes { VMC_MODE_DISABLED, VMC_MODE_LOW, VMC_MODE_HIGH, VMC_MODE_AUTO, VMC_MODE_MAX };

// vmc commands
enum VmcCommands { CMND_VMC_MODE, CMND_VMC_TARGET, CMND_VMC_THRESHOLD, CMND_VMC_LOW, CMND_VMC_HIGH, CMND_VMC_AUTO };
const char kVmcCommands[] PROGMEM = D_CMND_VMC_MODE "|" D_CMND_VMC_TARGET "|" D_CMND_VMC_THRESHOLD "|" D_CMND_VMC_LOW "|" D_CMND_VMC_HIGH "|" D_CMND_VMC_AUTO;

// HTML chains
const char VMC_LINE_DASH[]        PROGMEM = "<line class='dash' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n";
const char VMC_TEXT_TEMPERATURE[] PROGMEM = "<text class='temperature' x='%d%%' y='%d%%'>%s°C</text>\n";
const char VMC_TEXT_HUMIDITY[]    PROGMEM = "<text class='humidity' x='%d%%' y='%d%%'>%d %%</text>\n";
const char VMC_TEXT_TIME[]        PROGMEM = "<text class='time' x='%d' y='%d%%'>%sh</text>\n";
const char VMC_INPUT_BUTTON[]     PROGMEM = "<button name='%s' class='button %s'>%s</button>\n";
const char VMC_INPUT_NUMBER[]     PROGMEM = "<p>%s<br/><input type='number' name='%s' min='0' max='%d' step='1' value='%d'></p>\n";
const char VMC_INPUT_OPTION[]     PROGMEM = "<option value='%d' %s>%s</option>";

/****************************************\
 *               Icons
\****************************************/

// icon : fan off
unsigned char vmc_icon_off_png[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x01, 0x03, 0x00, 0x00, 0x00, 0xf9, 0xf0, 0xf3, 0x88, 0x00, 0x00, 0x00, 0x06, 0x50, 0x4c, 0x54, 0x45, 0x48, 0xbe, 0x55, 0xc8, 0x00, 0x00, 0x8a, 0xf4, 0xbf, 0xd3, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x00, 0x01, 0x62, 0x4b, 0x47, 0x44, 0x00, 0x88, 0x05, 0x1d, 0x48, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0d, 0xd7, 0x00, 0x00, 0x0d, 0xd7, 0x01, 0x42, 0x28, 0x9b, 0x78, 0x00, 0x00, 0x00, 0x07, 0x74, 0x49, 0x4d, 0x45, 0x07, 0xe4, 0x0a, 0x12, 0x15, 0x0d, 0x24, 0x62, 0xe1, 0x8f, 0x86, 0x00, 0x00, 0x02, 0x00, 0x49, 0x44, 0x41, 0x54, 0x48, 0xc7, 0xcd, 0x96, 0x31, 0x92, 0xe4, 0x20, 0x0c, 0x45, 0xa1, 0x1c, 0x38, 0xf4, 0x11, 0xb8, 0x89, 0x7d, 0x31, 0x97, 0x71, 0xb6, 0xd7, 0xf2, 0x0d, 0xf6, 0x0a, 0xcc, 0x0d, 0xa8, 0xda, 0x84, 0xc0, 0x85, 0x56, 0x5f, 0x78, 0xba, 0x2d, 0x31, 0x33, 0x4e, 0x36, 0x58, 0xaa, 0x83, 0xe6, 0x15, 0x20, 0x21, 0xe9, 0x23, 0x3b, 0xf7, 0xcd, 0x18, 0x8b, 0x01, 0x91, 0x76, 0x35, 0x1f, 0x88, 0x92, 0xde, 0x41, 0xa4, 0xf7, 0x04, 0xa2, 0x53, 0x81, 0xa5, 0x4e, 0x55, 0x9f, 0x99, 0x1c, 0x69, 0x20, 0xbf, 0xdb, 0xe0, 0xf5, 0x41, 0xd9, 0x3d, 0x0d, 0xf0, 0x6c, 0x73, 0x3a, 0x0c, 0x18, 0xef, 0x60, 0xc8, 0x8f, 0x20, 0xfd, 0x1b, 0x30, 0x3d, 0x83, 0xdd, 0x80, 0xf0, 0x08, 0x16, 0x67, 0x80, 0x0a, 0x10, 0xee, 0xb2, 0xdd, 0x81, 0xcf, 0x12, 0xa3, 0x9f, 0x81, 0x57, 0x89, 0xe2, 0x59, 0x07, 0x70, 0xee, 0x6d, 0x14,
  0xb1, 0xac, 0xd2, 0x30, 0x76, 0xe0, 0x50, 0x60, 0xd5, 0xb7, 0x87, 0x9b, 0x3d, 0xd0, 0x25, 0x35, 0x9b, 0x5c, 0xf3, 0xdd, 0x7b, 0xa0, 0xab, 0x32, 0xec, 0x4f, 0x60, 0x3a, 0x3a, 0xb0, 0x98, 0x52, 0x4f, 0xf3, 0x7b, 0x7b, 0x79, 0x81, 0x51, 0xea, 0x3b, 0x92, 0x44, 0x19, 0x40, 0xea, 0xdb, 0x13, 0x1d, 0x17, 0xe0, 0x82, 0xaf, 0xa2, 0x83, 0x8c, 0x90, 0x31, 0x58, 0x88, 0x58, 0x24, 0x4d, 0x07, 0xfe, 0x9c, 0xb1, 0x16, 0xab, 0xa7, 0x22, 0x65, 0xcf, 0x8b, 0x47, 0x16, 0x04, 0xd7, 0xfc, 0x94, 0x06, 0x88, 0x29, 0x56, 0x96, 0xc8, 0xe1, 0x42, 0x72, 0xe1, 0x70, 0x30, 0xc6, 0x80, 0x7f, 0x6e, 0xcc, 0xb8, 0x76, 0x2c, 0xb0, 0xe8, 0x71, 0xf8, 0x50, 0x50, 0x05, 0x81, 0x70, 0xe0, 0x2f, 0xec, 0xf4, 0x02, 0xf8, 0x34, 0x36, 0xfe, 0xbb, 0xb6, 0x04, 0xcd, 0xae, 0xd9, 0xbb, 0x54, 0x78, 0x02, 0xb0, 0xf7, 0x18, 0xa9, 0x81, 0xad, 0x69, 0x52, 0x9c, 0x94, 0x04, 0x01, 0xc8, 0x9e, 0x7a, 0x65, 0x6c, 0x95, 0xf8, 0xd0, 0xa7, 0x6e, 0xb7, 0x06, 0xb0, 0xa9, 0x7c, 0x0d, 0xe6, 0xab, 0xf8, 0x7e, 0x02, 0x2d, 0xb8, 0x88, 0x4d, 0xbe, 0x83, 0xd8, 0x82, 0xf3, 0x02, 0xfe, 0xe5, 0xfb, 0xd6, 0x6a, 0x7e, 0xa4, 0x4f, 0xd7, 0x2e, 0xd0, 0x7c, 0xc7, 0x9e, 0xb5, 0xd5, 0x45, 0xfc, 0xc3, 0xb3, 0x80, 0xeb, 0x35, 0xe0, 0xe9, 0x03, 0x77, 0xc3, 0x21, 0xa7, 0x14, 0xdb, 0x44, 0x3b, 0x71, 0xfe, 0x42, 0x45, 0xc4, 0xa6, 0x77, 0x84, 0x07, 0x02, 0xe0, 0x27, 0x6e, 0xe0, 0xb5, 0x51, 0x92, 0x73, 0x70, 0xc5, 0x0f, 0x95, 0xdd, 0x4c, 0x2d, 0xe5, 0xb1, 0x4c, 0x19, 0xb9, 0x85, 0xbd, 0x75, 0xd9, 0xc5, 0x7a, 0x42, 0x46, 0x10, 0x9b, 0x15, 0xd6, 0x46, 0x71, 0x25, 0x0a, 0x5e, 0x61, 0x6d, 0x90, 0xc0,
  0x06, 0x58, 0x6c, 0xc0, 0xc9, 0xdf, 0x56, 0x47, 0xab, 0xe8, 0x25, 0xbc, 0x75, 0x76, 0x1a, 0x01, 0xb9, 0xd3, 0x68, 0xce, 0x95, 0x0e, 0xf8, 0xf2, 0x04, 0xb4, 0xd4, 0x71, 0x4b, 0x03, 0xf2, 0x95, 0xc2, 0x1b, 0xd8, 0x9c, 0x79, 0x51, 0x3a, 0x30, 0x3b, 0xf3, 0x08, 0x2d, 0x46, 0x40, 0x16, 0x1c, 0x46, 0xa7, 0x1c, 0x0d, 0xad, 0x64, 0x9e, 0x75, 0x40, 0xbf, 0x06, 0x8b, 0x7b, 0x02, 0x9b, 0x79, 0x60, 0xd1, 0x2c, 0xf4, 0x23, 0x74, 0x1a, 0xd0, 0xf5, 0x86, 0x1e, 0xe4, 0xff, 0xb8, 0x7b, 0xe8, 0xde, 0x90, 0x2d, 0x28, 0xa6, 0x59, 0xe0, 0x72, 0xaa, 0x59, 0xf8, 0x2a, 0x31, 0xba, 0x0d, 0x72, 0xa3, 0xae, 0x8f, 0x98, 0xa3, 0xe9, 0xd9, 0x5f, 0x74, 0xf5, 0xae, 0xef, 0xeb, 0x77, 0xdd, 0x53, 0x35, 0xdf, 0x0e, 0x21, 0x7d, 0xf7, 0x99, 0xf1, 0x17, 0x88, 0xda, 0x7a, 0x42, 0xbe, 0x93, 0x4f, 0xfd, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
unsigned int vmc_icon_off_len = 653;

// icon : fan slow speed
unsigned char vmc_icon_slow_png[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x02, 0x03, 0x00, 0x00, 0x00, 0xbe, 0x50, 0x89, 0x58, 0x00, 0x00, 0x00, 0x09, 0x50, 0x4c, 0x54, 0x45, 0x00, 0x00, 0x00, 0x08, 0x45, 0x7f, 0x33, 0xcc, 0x00, 0x8d, 0x1d, 0xcf, 0xd5, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x00, 0x01, 0x62, 0x4b, 0x47, 0x44, 0x00, 0x88, 0x05, 0x1d, 0x48, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0d, 0xd7, 0x00, 0x00, 0x0d, 0xd7, 0x01, 0x42, 0x28, 0x9b, 0x78, 0x00, 0x00, 0x00, 0x07, 0x74, 0x49, 0x4d, 0x45, 0x07, 0xe4, 0x0a, 0x12, 0x14, 0x15, 0x14, 0xc7, 0xe1, 0x4d, 0x44, 0x00, 0x00, 0x02, 0x5c, 0x49, 0x44, 0x41, 0x54, 0x58, 0xc3, 0xe5, 0x97, 0x4b, 0x6e, 0xc4, 0x30, 0x08, 0x86, 0x23, 0x2f, 0x7d, 0x14, 0x4e, 0x69, 0xcd, 0x49, 0x58, 0x5a, 0x9c, 0xb2, 0xe6, 0x65, 0x7b, 0xda, 0x80, 0xdb, 0x66, 0x53, 0xa9, 0x91, 0x5a, 0x69, 0xe2, 0x2f, 0x18, 0x08, 0xfc, 0x38, 0xd7, 0xf5, 0xc3, 0x0b, 0xa8, 0x1d, 0xd6, 0x29, 0x25, 0xea, 0x58, 0xa7, 0x7e, 0x30, 0x90, 0x99, 0x28, 0xb2, 0x4e, 0x98, 0xed, 0xd0, 0xaf, 0x6c, 0x0f, 0x90, 0x35, 0xa2, 0x04, 0x58, 0xff, 0x6f, 0x2f, 0x35, 0x5e, 0x62, 0x2f, 0xd5, 0xbd, 0x18, 0xf0, 0x95, 0x30, 0x0c, 0x07, 0x00, 0xc3, 0x28, 0xaf, 0xa7, 0x40, 0xff, 0x3b, 0x00, 0x3d, 0x07, 0x5a, 0x0e, 0x94, 0xc7, 0x40, 0x0d, 0xeb, 0xc1, 0x80, 0xb8, 0x60, 0xec, 0x51, 0xe8, 0x71, 0x51, 0xe7, 0x51, 0x7e, 0x17, 0x48, 0x4a, 0x52, 0x97, 0x8e, 0x40, 0x1c, 0xa5, 0xe5, 0xb8, 0x26, 0xcd,
  0x2b, 0xee, 0xc1, 0x11, 0xc0, 0xa4, 0xfb, 0x31, 0x8d, 0xd2, 0xac, 0x9f, 0x81, 0x44, 0x82, 0x38, 0x80, 0x92, 0x02, 0xc4, 0xc0, 0xeb, 0x04, 0x54, 0xbc, 0xb2, 0x54, 0x0e, 0x20, 0x7e, 0xdd, 0x2f, 0x01, 0x92, 0xa2, 0xc5, 0x11, 0xe2, 0x00, 0xc2, 0x54, 0x41, 0x1b, 0xc0, 0xf0, 0xa3, 0x44, 0x7b, 0x10, 0x27, 0x82, 0x3d, 0xa0, 0x2f, 0xf2, 0x29, 0x9b, 0x8e, 0x07, 0x0d, 0x80, 0xa6, 0xaa, 0xdb, 0x97, 0xfe, 0xaa, 0x0b, 0x6c, 0x9f, 0x01, 0x0e, 0x74, 0x89, 0x72, 0x71, 0xf5, 0xad, 0xcd, 0x81, 0x82, 0xa6, 0xda, 0x7d, 0x2a, 0x3c, 0x9b, 0x00, 0x49, 0x84, 0x64, 0xa1, 0xe9, 0x4d, 0x35, 0x31, 0x15, 0x5e, 0xd2, 0x88, 0x02, 0x98, 0xaa, 0xab, 0x65, 0x5e, 0x14, 0x63, 0xaa, 0xd3, 0x62, 0x16, 0x74, 0xcd, 0xb4, 0xbb, 0xcb, 0x36, 0xad, 0xe0, 0x74, 0xbd, 0xd8, 0xb3, 0x45, 0x7f, 0xa8, 0x99, 0x7e, 0x2d, 0x60, 0x46, 0x08, 0xe4, 0xef, 0x1f, 0x2c, 0x24, 0xf1, 0xb7, 0x90, 0x4f, 0x84, 0x4a, 0xcd, 0xda, 0xc0, 0xee, 0x89, 0x73, 0x0d, 0xe6, 0x54, 0x2a, 0x13, 0x30, 0xb7, 0x24, 0x3a, 0xdc, 0x66, 0x0e, 0x03, 0x7d, 0x1f, 0x57, 0x5b, 0xf8, 0xde, 0x08, 0xb5, 0xef, 0x03, 0x8f, 0xe8, 0x7d, 0x30, 0x0e, 0xc0, 0x1b, 0xa9, 0xdc, 0x19, 0xe0, 0xd2, 0x98, 0x9d, 0x56, 0x6e, 0x0c, 0x08, 0xb0, 0xca, 0x67, 0xcf, 0xef, 0xea, 0x94, 0xbd, 0xbe, 0xca, 0xe7, 0x1d, 0xce, 0x40, 0xed, 0x6f, 0xdd, 0xfe, 0x3b, 0x60, 0x6f, 0x13, 0x2f, 0x13, 0x8a, 0x80, 0x95, 0x2c, 0xbc, 0x05, 0x6e, 0x72, 0xc5, 0x65, 0x4e, 0x37, 0x06, 0x56, 0xc5, 0xef, 0xc0, 0xfb, 0xfb, 0x42, 0xcf, 0xe4, 0x12, 0x8c, 0x61, 0x40, 0x5f, 0xb7, 0xa0, 0xfd, 0x0b, 0x30, 0xee, 0xf6, 0xe2, 0xdb, 0xbb, 0x17, 0x6c, 0xc8, 0x3d,
  0x26, 0x2d, 0x6f, 0xf2, 0x0a, 0xeb, 0xae, 0xbd, 0xb3, 0x3e, 0xb7, 0x72, 0xad, 0x9c, 0x12, 0x07, 0xc0, 0x14, 0x9a, 0xf4, 0x87, 0x02, 0x6c, 0x0b, 0x4d, 0xb7, 0xab, 0x35, 0xc6, 0xec, 0x18, 0xed, 0x42, 0xd0, 0x1d, 0xf7, 0xfc, 0xca, 0x0a, 0xd6, 0xd9, 0x85, 0x33, 0x1d, 0x1e, 0x38, 0x08, 0x20, 0xdb, 0xba, 0x31, 0x0d, 0xc0, 0x3b, 0xbd, 0x36, 0x89, 0x9b, 0xf3, 0x80, 0x6e, 0x79, 0x09, 0x04, 0xa9, 0x6c, 0x28, 0xa0, 0x4a, 0xb9, 0x5e, 0xab, 0xb9, 0xce, 0xda, 0xa5, 0x55, 0x0c, 0x6d, 0x97, 0xa6, 0x5d, 0xc4, 0xac, 0x0f, 0x28, 0x94, 0x41, 0xb1, 0x4f, 0xa1, 0x0c, 0x8e, 0xd2, 0x90, 0x66, 0xad, 0xf1, 0x41, 0xeb, 0x12, 0x2d, 0x7e, 0xb5, 0x78, 0x24, 0x09, 0x10, 0x0f, 0x8c, 0x8a, 0x92, 0xc6, 0x64, 0x26, 0xbd, 0x4e, 0x40, 0xa1, 0xc3, 0x5c, 0x54, 0xa0, 0xe7, 0x33, 0xe9, 0x0c, 0x64, 0xc3, 0x5b, 0xd6, 0xb2, 0xf1, 0x0f, 0x47, 0x00, 0xd7, 0x49, 0x25, 0x1e, 0xff, 0xcf, 0xcf, 0x28, 0xc9, 0x78, 0x37, 0xf7, 0x1e, 0x00, 0x96, 0xc3, 0xf8, 0x6d, 0xad, 0x9e, 0x4f, 0x0f, 0xff, 0x31, 0x70, 0x3c, 0xdb, 0x7f, 0x03, 0x78, 0x7c, 0xf8, 0xff, 0x57, 0x5f, 0x07, 0xc9, 0xd9, 0x9e, 0x8e, 0x40, 0xcb, 0x0f, 0xff, 0xf6, 0x28, 0x24, 0x40, 0xdf, 0xca, 0xe6, 0xb6, 0x60, 0xa4, 0x77, 0x13, 0x7d, 0x18, 0x82, 0xc9, 0x7f, 0x98, 0x7f, 0x55, 0x9f, 0xbf, 0x9a, 0x8f, 0xdf, 0xdd, 0x89, 0x02, 0x6d, 0xe3, 0xee, 0x10, 0xe9, 0x4f, 0xae, 0x0f, 0x17, 0xcb, 0x13, 0x7f, 0xc5, 0xaa, 0x53, 0x50, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
unsigned int vmc_icon_slow_len = 748;

// icon : fan fast speed
unsigned char vmc_icon_fast_png[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x02, 0x03, 0x00, 0x00, 0x00, 0xbe, 0x50, 0x89, 0x58, 0x00, 0x00, 0x00, 0x09, 0x50, 0x4c, 0x54, 0x45, 0x00, 0x00, 0x00, 0x99, 0x88, 0x7f, 0x33, 0xcc, 0x00, 0x4a, 0xa2, 0x26, 0x59, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x00, 0x01, 0x62, 0x4b, 0x47, 0x44, 0x00, 0x88, 0x05, 0x1d, 0x48, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0d, 0xd7, 0x00, 0x00, 0x0d, 0xd7, 0x01, 0x42, 0x28, 0x9b, 0x78, 0x00, 0x00, 0x00, 0x07, 0x74, 0x49, 0x4d, 0x45, 0x07, 0xe4, 0x0a, 0x12, 0x14, 0x15, 0x01, 0xaa, 0x3c, 0xa9, 0xaf, 0x00, 0x00, 0x03, 0x67, 0x49, 0x44, 0x41, 0x54, 0x58, 0xc3, 0x9d, 0x57, 0x49, 0x8e, 0xe5, 0x20, 0x0c, 0x8d, 0x58, 0xb5, 0x38, 0x45, 0x2f, 0x4b, 0x3e, 0x25, 0xca, 0x49, 0x58, 0x5a, 0x3e, 0x65, 0x83, 0x27, 0xcc, 0x90, 0x44, 0xd5, 0x5f, 0xaa, 0x92, 0x12, 0x5e, 0x3c, 0x3c, 0xcc, 0xb3, 0xb9, 0xae, 0x5f, 0xfe, 0x80, 0xca, 0xc7, 0x3a, 0xbd, 0x22, 0x72, 0x5b, 0x27, 0xfc, 0x30, 0xf0, 0x66, 0x22, 0xf1, 0x3a, 0xd5, 0x37, 0x0f, 0x78, 0xbd, 0xf9, 0x00, 0x5e, 0x23, 0x7a, 0x01, 0x8c, 0xff, 0xc7, 0x9f, 0x18, 0x4f, 0xcf, 0x51, 0x4a, 0x78, 0xcf, 0x00, 0x5b, 0x79, 0x4c, 0xc3, 0x00, 0xf0, 0x00, 0xa0, 0xac, 0xd1, 0x41, 0xa5, 0x33, 0x09, 0x01, 0x50, 0x8f, 0x11, 0x66, 0x34, 0x40, 0xa6, 0x93, 0x81, 0x6b, 0x00, 0x4e, 0x71, 0xb6, 0xb7, 0x01, 0x90, 0xf1, 0x94, 0x41, 0x00, 0xec, 0x5c, 0xf4, 0x45, 0x03, 0x74, 0xfb, 0x5b, 0xaa, 0x50,
  0x66, 0x40, 0x5a, 0x7d, 0xd0, 0x15, 0x00, 0x45, 0x5f, 0xc4, 0x10, 0x70, 0x00, 0xc4, 0x3f, 0x94, 0x35, 0x87, 0x05, 0xb0, 0xe4, 0x41, 0xd7, 0x78, 0xa7, 0x2c, 0xd1, 0xe6, 0xc1, 0x00, 0x5a, 0x30, 0x93, 0x8f, 0x54, 0xc3, 0xa7, 0x80, 0x7f, 0xf9, 0x29, 0x02, 0xe4, 0x21, 0xa9, 0x6d, 0xd9, 0xcc, 0x14, 0x99, 0x00, 0x46, 0xdd, 0x0e, 0xe0, 0x35, 0x5c, 0x63, 0x04, 0x01, 0xb4, 0x24, 0x24, 0x18, 0x5a, 0x62, 0x4c, 0x24, 0xf9, 0xf5, 0xff, 0x2b, 0x13, 0x59, 0x58, 0x50, 0x02, 0xc8, 0x68, 0xa9, 0x73, 0x8c, 0xa0, 0x1f, 0x76, 0xfb, 0x62, 0xb2, 0xce, 0x31, 0x92, 0x96, 0x33, 0xa0, 0xf9, 0xc7, 0x29, 0xc6, 0xfe, 0x91, 0x00, 0xaa, 0xf9, 0x1f, 0x51, 0xa2, 0x7a, 0xe4, 0x35, 0x72, 0xff, 0x30, 0xf1, 0xd8, 0xbf, 0x11, 0xeb, 0xd5, 0x77, 0xb7, 0x44, 0x00, 0xb9, 0xfb, 0xe2, 0xf5, 0x51, 0x42, 0x12, 0x5e, 0x10, 0x5a, 0x8d, 0xdd, 0xa0, 0xa7, 0xd1, 0x01, 0x42, 0x05, 0x75, 0xc0, 0x6d, 0xd4, 0x38, 0x00, 0xcc, 0x9c, 0x00, 0x18, 0xcb, 0x8b, 0x18, 0x00, 0x60, 0xb5, 0xd4, 0x00, 0x68, 0x99, 0x19, 0xe0, 0xc7, 0xa8, 0xb8, 0x19, 0x30, 0x8a, 0xf6, 0x27, 0x00, 0x84, 0x0a, 0xea, 0x47, 0x46, 0xa9, 0x0a, 0x80, 0x41, 0x45, 0x03, 0xb4, 0x38, 0x24, 0xa3, 0xa5, 0xec, 0x85, 0x8a, 0x46, 0x44, 0x8f, 0x80, 0x36, 0xf9, 0x2c, 0x4a, 0x85, 0x02, 0x78, 0x27, 0xc0, 0x05, 0xb3, 0xeb, 0xaf, 0x52, 0xa1, 0x1a, 0xd0, 0x13, 0x1d, 0xa2, 0x9c, 0x4c, 0x7d, 0x1b, 0x48, 0x01, 0xcd, 0xa1, 0xa8, 0x36, 0xba, 0xc2, 0x93, 0xc4, 0x6d, 0xf5, 0x58, 0xe4, 0xa5, 0x98, 0x70, 0x85, 0x67, 0x1a, 0x45, 0x87, 0x54, 0xd5, 0xc5, 0x72, 0x5f, 0x64, 0x63, 0xa2, 0xd3, 0x6c, 0x16, 0x64, 0x4d, 0xb5, 0x1b, 0xd9,
  0x4d, 0x11, 0x2a, 0x18, 0x90, 0xf4, 0xdb, 0x24, 0x0f, 0x62, 0x06, 0xaf, 0x01, 0xf0, 0x0c, 0x81, 0x5c, 0x0d, 0x34, 0x25, 0x8e, 0x37, 0x91, 0x75, 0x84, 0x4c, 0x45, 0x4f, 0xac, 0xbe, 0xe3, 0xe0, 0x0a, 0x78, 0x57, 0x4a, 0x0e, 0xd0, 0xb0, 0x38, 0xbb, 0x1a, 0x7a, 0xce, 0xd0, 0x3e, 0x4f, 0x6c, 0x6e, 0x6b, 0x14, 0xd4, 0x33, 0xac, 0x63, 0xec, 0x20, 0x80, 0xd7, 0x66, 0xa2, 0x44, 0xed, 0x32, 0x40, 0x40, 0xe0, 0x24, 0x6e, 0x41, 0x52, 0x23, 0xbf, 0x06, 0xc0, 0x49, 0x73, 0xd3, 0xea, 0xe1, 0x1b, 0x90, 0x71, 0xc4, 0xf0, 0xdf, 0x80, 0x28, 0xb9, 0x56, 0x26, 0xf4, 0x04, 0x18, 0x64, 0xd5, 0x23, 0xe0, 0xc0, 0x55, 0x2f, 0x73, 0x3a, 0x18, 0x18, 0x15, 0x1f, 0x01, 0xf3, 0x7e, 0xd5, 0xbd, 0x7d, 0x35, 0x03, 0xb2, 0xdd, 0x0c, 0xc5, 0x0d, 0xd0, 0xde, 0x62, 0x32, 0xf7, 0x16, 0x45, 0x37, 0x64, 0x11, 0x93, 0x94, 0x37, 0x59, 0x85, 0xa1, 0x35, 0x1f, 0xaf, 0xcf, 0x50, 0xae, 0xb9, 0x53, 0x62, 0x00, 0x50, 0x85, 0x26, 0x79, 0x20, 0x15, 0x40, 0x0e, 0x93, 0x33, 0xc8, 0x7a, 0x30, 0xfc, 0xc4, 0xc8, 0x29, 0x04, 0xf1, 0x18, 0xf9, 0x25, 0x6d, 0xfc, 0x76, 0x0a, 0x9d, 0x0e, 0x4b, 0x9c, 0xb5, 0xac, 0xb2, 0x5b, 0x33, 0x26, 0x09, 0xd8, 0x49, 0xef, 0x1a, 0x21, 0x93, 0x41, 0xaa, 0x66, 0x79, 0x08, 0x04, 0xb9, 0x9c, 0xf7, 0x4a, 0xaf, 0x7e, 0xca, 0x42, 0x86, 0xac, 0x5d, 0x52, 0xc5, 0xac, 0xc3, 0x2c, 0x4d, 0x5b, 0x83, 0x97, 0x73, 0x40, 0x6b, 0xdb, 0x6c, 0x3f, 0xed, 0x08, 0x7c, 0xfc, 0xe6, 0x5e, 0x10, 0x85, 0x14, 0xf9, 0x2c, 0x52, 0x5e, 0x85, 0xd4, 0xa5, 0x98, 0x55, 0xb8, 0xc9, 0xfd, 0x2a, 0xc5, 0x2e, 0xe6, 0x6d, 0x80, 0x2a, 0xb6, 0xbd, 0xb8, 0xa8, 0xbd, 0xb4, 0x83,
  0xca, 0x34, 0x6e, 0xed, 0xc0, 0x1b, 0xca, 0x75, 0x3b, 0x60, 0x6a, 0x28, 0xde, 0x92, 0xa4, 0xff, 0x7b, 0x5f, 0x74, 0x80, 0x37, 0x35, 0x05, 0xe0, 0xda, 0xd4, 0xbc, 0x2d, 0xca, 0xfe, 0x03, 0xae, 0x6d, 0xd1, 0x1b, 0xab, 0x16, 0x08, 0xae, 0x8d, 0xd5, 0x5b, 0xb3, 0x9c, 0x22, 0x6e, 0xff, 0x73, 0x6b, 0xb6, 0xe6, 0x2e, 0xee, 0x3b, 0x60, 0x69, 0xee, 0x36, 0x1e, 0x48, 0x02, 0x89, 0xb6, 0xf1, 0x20, 0xdb, 0x98, 0xa5, 0x0a, 0x5d, 0xd6, 0x01, 0x43, 0x47, 0x94, 0xcb, 0x67, 0x94, 0x6d, 0x58, 0x93, 0x21, 0xa7, 0xea, 0x36, 0xd0, 0xbd, 0x0e, 0x39, 0x3a, 0x26, 0xd9, 0xd4, 0x4b, 0xf7, 0x36, 0x26, 0x09, 0x25, 0x0e, 0x28, 0xdb, 0xa0, 0x25, 0x0f, 0x7f, 0x54, 0xaf, 0xa4, 0xd8, 0xa7, 0x9a, 0xd3, 0xf9, 0x73, 0x9c, 0xf9, 0x89, 0xc7, 0x11, 0xd0, 0x50, 0x0d, 0x1f, 0xec, 0xa6, 0x81, 0x73, 0x9a, 0xed, 0x73, 0xdd, 0x47, 0xd6, 0x09, 0x00, 0xfb, 0x99, 0x70, 0xab, 0x50, 0xf7, 0xa1, 0x57, 0xc6, 0xe6, 0x00, 0xd8, 0xc6, 0x66, 0x2e, 0x82, 0x00, 0xd8, 0x06, 0xef, 0xcf, 0xd1, 0xfd, 0x73, 0xf8, 0xff, 0xbe, 0x3e, 0x7c, 0x5e, 0x40, 0xfa, 0x66, 0xfa, 0xf0, 0x7f, 0xbe, 0x27, 0x45, 0xc0, 0xc3, 0x5d, 0x10, 0xaf, 0xf7, 0x8b, 0x98, 0x7a, 0x7e, 0x03, 0x94, 0x80, 0x7b, 0xbe, 0xa9, 0xc1, 0x0b, 0x00, 0x43, 0xd9, 0x1c, 0xf3, 0xe0, 0xd3, 0xf5, 0x72, 0xa5, 0x6d, 0x82, 0xd9, 0xff, 0xea, 0xfb, 0xad, 0xfa, 0xfb, 0xd6, 0xfc, 0x79, 0xef, 0xae, 0x2f, 0x80, 0xf4, 0x61, 0xc0, 0x33, 0xfd, 0xcd, 0xef, 0x1f, 0xf7, 0x65, 0x90, 0xab, 0xe0, 0xbc, 0x56, 0x6a, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
unsigned int vmc_icon_fast_len = 1015;

/*************************************************\
 *               Variables
\*************************************************/

// global variables
struct {
  uint8_t humidity_source = VMC_SOURCE_NONE;     // humidity source
  uint8_t humidity    = UINT8_MAX;               // last read humidity level
  uint8_t target      = UINT8_MAX;               // last target humidity level
  float   temperature = NAN;                     // last temperature read
} vmc_status;

// web client status
struct {
  bool state       = false;
  bool temperature = false;
  bool humidity    = false;
  bool graph       = false;
} vmc_web;

// graph variables
struct {
  uint32_t index    = 0;
  uint32_t counter  = 0;
  uint8_t  humidity = UINT8_MAX;
  uint8_t  target   = UINT8_MAX;
  uint8_t  state    = VMC_STATE_NONE;
  float    temperature = NAN;
  uint8_t  arr_state[VMC_GRAPH_SAMPLE];
  uint8_t  arr_humidity[VMC_GRAPH_SAMPLE];
  uint8_t  arr_target[VMC_GRAPH_SAMPLE];
  float    arr_temperature[VMC_GRAPH_SAMPLE];
} vmc_graph;

/**************************************************\
 *                  Accessors
\**************************************************/

// get VMC label according to state
String VmcGetStateLabel (uint8_t state)
{
  String str_label;

  // get label
  switch (state)
  {
   case VMC_MODE_DISABLED:          // Disabled
     str_label = D_VMC_DISABLED;
     break;
   case VMC_MODE_LOW:               // Forced Low speed
     str_label = D_VMC_LOW;
     break;
   case VMC_MODE_HIGH:              // Forced High speed
     str_label = D_VMC_HIGH;
     break;
   case VMC_MODE_AUTO:              // Automatic mode
     str_label = D_VMC_AUTO;
     break;
  }
  
  return str_label;
}

// get VMC state from relays state
uint8_t VmcGetRelayState ()
{
  uint8_t vmc_relay1 = 0;
  uint8_t vmc_relay2 = 1;
  uint8_t relay_state = VMC_STATE_OFF;

  // set number of relay to read status
  //devices_present = vmc_devices_present;

  // read relay states
  vmc_relay1 = bitRead (TasmotaGlobal.power, 0);
  if (TasmotaGlobal.devices_present == 2) vmc_relay2 = bitRead (TasmotaGlobal.power, 1);

  // convert to vmc state
  if ((vmc_relay1 == 0) && (vmc_relay2 == 1)) relay_state = VMC_STATE_LOW;
  else if (vmc_relay1 == 1) relay_state = VMC_STATE_HIGH;
  
  // reset number of relay
//  devices_present = 0;

  return relay_state;
}

// set relays state
void VmcSetRelayState (uint8_t new_state)
{
  // set number of relay to start command
//  devices_present = vmc_devices_present;

  // set relays
  switch (new_state)
  {
    case VMC_STATE_OFF:  // VMC disabled
      ExecuteCommandPower (1, POWER_OFF, SRC_IGNORE);
      if (TasmotaGlobal.devices_present == 2) ExecuteCommandPower (2, POWER_OFF, SRC_IGNORE);
      break;
    case VMC_STATE_LOW:  // VMC low speed
      ExecuteCommandPower (1, POWER_OFF, SRC_IGNORE);
      if (TasmotaGlobal.devices_present == 2) ExecuteCommandPower (2, POWER_ON, SRC_IGNORE);
      break;
    case VMC_STATE_HIGH:  // VMC high speed
      if (TasmotaGlobal.devices_present == 2) ExecuteCommandPower (2, POWER_OFF, SRC_IGNORE);
      ExecuteCommandPower (1, POWER_ON, SRC_IGNORE);
      break;
  }

  // reset number of relay
//  devices_present = 0;
}

// get vmc actual mode
uint8_t VmcGetMode ()
{
  uint8_t actual_mode;

  // read actual VMC mode
  actual_mode = (uint8_t) Settings.weight_reference;

  // if outvalue, set to disabled
  if (actual_mode >= VMC_MODE_MAX) actual_mode = VMC_MODE_DISABLED;

  return actual_mode;
}

// set vmc mode
void VmcSetMode (uint8_t new_mode)
{
  // if outvalue, set to disabled
  if (new_mode >= VMC_MODE_MAX) new_mode = VMC_MODE_DISABLED;
  Settings.weight_reference = (unsigned long) new_mode;

  // if forced mode, set relay state accordingly
  if (new_mode == VMC_MODE_LOW) VmcSetRelayState (VMC_STATE_LOW);
  else if (new_mode == VMC_MODE_HIGH) VmcSetRelayState (VMC_STATE_HIGH);

  // set web update flag
  vmc_web.state = true;
}

// get current temperature
float VmcGetTemperature ()
{
  float temperature = NAN;

#ifdef USE_DHT
  // if dht sensor present, read it 
  if (Dht[0].t != 0) temperature = Dht[0].t;
#endif

  //if temperature mesured, round at 0.1 °C
  if (!isnan (temperature)) temperature = floor (temperature * 10) / 10;

  return temperature;
}

// get current humidity level
uint8_t VmcGetHumidity ()
{
  uint8_t result   = UINT8_MAX;
  float   humidity = NAN;

  // read humidity from local sensor
  vmc_status.humidity_source = VMC_SOURCE_LOCAL;

#ifdef USE_DHT
  // if dht sensor present, read it 
  if (Dht[0].h != 0) humidity = Dht[0].h;
#endif

  // if not available, read MQTT humidity
  if (isnan (humidity))
  {
    vmc_status.humidity_source = VMC_SOURCE_REMOTE;
    humidity = HumidityGetValue ();
  }

  // convert to integer and keep in range 0..100
  if (isnan (humidity) == false)
  {
    result = (uint8_t) humidity;
    if (result > VMC_HUMIDITY_MAX) result = VMC_HUMIDITY_MAX;
  }

  return result;
}

// set target humidity
void VmcSetTargetHumidity (uint8_t new_target)
{
  // if in range, save target humidity level
  if (new_target <= VMC_TARGET_MAX) Settings.weight_max = (uint16_t) new_target;
}

// get target humidity
uint8_t VmcGetTargetHumidity ()
{
  uint8_t target;

  // get target temperature
  target = (uint8_t) Settings.weight_max;
  if (target > VMC_TARGET_MAX) target = VMC_TARGET_DEFAULT;
  
  return target;
}

// set vmc humidity threshold
void VmcSetThreshold (uint8_t new_threshold)
{
  // if within range, save threshold
  if (new_threshold <= VMC_THRESHOLD_MAX) Settings.weight_calibration = (unsigned long) new_threshold;
}

// get vmc humidity threshold
uint8_t VmcGetThreshold ()
{
  uint8_t threshold;

  // get humidity threshold
  threshold = (uint8_t) Settings.weight_calibration;
   if (threshold > VMC_THRESHOLD_MAX) threshold = VMC_THRESHOLD_DEFAULT;

  return threshold;
}

// Show JSON status (for MQTT)
void VmcShowJSON (bool append)
{
  uint8_t humidity, vmc_value, vmc_mode;
  float   temperature;
  String  str_json, str_text;

  // get mode and humidity
  vmc_mode = VmcGetMode ();
  str_text = VmcGetStateLabel (vmc_mode);

  // vmc mode  -->  "VMC":{"Relay":2,"Mode":4,"Label":"Automatic","Humidity":70.5,"Target":50,"Temperature":18.4}
  str_json  = "\"" + String (D_JSON_VMC) + "\":{";
  str_json += "\"" + String (D_JSON_VMC_RELAY) + "\":"   + String (TasmotaGlobal.devices_present) + ",";
  str_json += "\"" + String (D_JSON_VMC_MODE)  + "\":"   + String (vmc_mode) + ",";
  str_json += "\"" + String (D_JSON_VMC_LABEL) + "\":\"" + str_text + "\",";

  // temperature
  temperature = VmcGetTemperature ();
  if (!isnan(temperature)) str_text = String (temperature, 1); else str_text = "n/a";
  str_json += "\"" + String (D_JSON_VMC_TEMPERATURE) + "\":" + str_text + ",";

  // humidity level
  humidity = VmcGetHumidity ();
  if (humidity != UINT8_MAX) str_text = String (humidity); else str_text = "n/a";
  str_json += "\"" + String (D_JSON_VMC_HUMIDITY) + "\":" + str_text + ",";

  // target humidity
  vmc_value = VmcGetTargetHumidity ();
  str_json += "\"" + String (D_JSON_VMC_TARGET) + "\":" + String (vmc_value) + ",";

  // humidity thresold
  vmc_value = VmcGetThreshold ();
  str_json += "\"" + String (D_JSON_VMC_THRESHOLD) + "\":" + String (vmc_value) + "}";

  // if VMC mode is enabled
  if (vmc_mode != VMC_MODE_DISABLED)
  {
    // get relay state and label
    vmc_value = VmcGetRelayState ();
    str_text  = VmcGetStateLabel (vmc_value);

    // relay state  -->  ,"State":{"Mode":1,"Label":"On"}
    str_json += ",";
    str_json += "\"" + String (D_JSON_VMC_STATE) + "\":{";
    str_json += "\"" + String (D_JSON_VMC_MODE)  + "\":"   + String (vmc_value) + ",";
    str_json += "\"" + String (D_JSON_VMC_LABEL) + "\":\"" + str_text + "\"}";
  }

  // add remote humidity to JSON
  HumidityShowJSON (true);

  // if append mode, add json string to MQTT message
  if (append) ResponseAppend_P (PSTR(",%s"), str_json.c_str ());
  else Response_P (PSTR("{%s}"), str_json.c_str ());
  
  // publish it if not in append mode
  if (!append) MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));
}

// Handle VMC MQTT commands
bool VmcCommand ()
{
  bool command_serviced = true;
  int  command_code;
  char command [CMDSZ];

  // check MQTT command
  command_code = GetCommandCode (command, sizeof(command), XdrvMailbox.topic, kVmcCommands);

  // handle command
  switch (command_code)
  {
    case CMND_VMC_MODE:        // set mode
      VmcSetMode (XdrvMailbox.payload);
      break;
    case CMND_VMC_LOW:         // set mode to low
      VmcSetMode (VMC_MODE_LOW);
      break;
    case CMND_VMC_HIGH:         // set mode to high
      VmcSetMode (VMC_MODE_HIGH);
      break;
    case CMND_VMC_AUTO:         // set mode to auto
      VmcSetMode (VMC_MODE_AUTO);
      break;
    case CMND_VMC_TARGET:     // set target humidity 
      VmcSetTargetHumidity ((uint8_t) XdrvMailbox.payload);
      break;
    case CMND_VMC_THRESHOLD:  // set humidity threshold 
      VmcSetThreshold ((uint8_t) XdrvMailbox.payload);
      break;
    default:
      command_serviced = false;
      break;
  }

  // if command processed, publish JSON
  if (command_serviced == true) VmcShowJSON (false);

  return command_serviced;
}

// update graph history data
void VmcUpdateGraphData ()
{
  // set indexed graph values with current values
  vmc_graph.arr_temperature[vmc_graph.index] = vmc_graph.temperature;
  vmc_graph.arr_humidity[vmc_graph.index] = vmc_graph.humidity;
  vmc_graph.arr_target[vmc_graph.index] = vmc_graph.target;
  vmc_graph.arr_state[vmc_graph.index] = vmc_graph.state;

  // init current values
  vmc_graph.temperature = NAN;
  vmc_graph.humidity    = UINT8_MAX;
  vmc_graph.target      = UINT8_MAX;
  vmc_graph.state       = VMC_STATE_OFF;

  // increase graph data index and reset if max reached
  vmc_graph.index ++;
  vmc_graph.index = vmc_graph.index % VMC_GRAPH_SAMPLE;

  // ask for graph update
  vmc_web.graph = true;
}

void VmcEverySecond ()
{
  bool    need_update = false;
  uint8_t humidity, threshold, vmc_mode, target, actual_state, target_state;
  float   temperature, compar_current, compar_read, compar_diff;

  // update current temperature
  temperature = VmcGetTemperature ( );
  if (!isnan(temperature))
  {
    // if temperature was previously mesured, compare
    if (!isnan(vmc_status.temperature))
    {
      // update JSN if temperature is at least 0.2°C different
      compar_current = floor (10 * vmc_status.temperature);
      compar_read    = floor (10 * temperature);
      compar_diff    = abs (compar_current - compar_read);
      if (compar_diff >= 2) need_update = true;
    }

    // else, temperature is the mesured temperature
    else need_update = true;

    // if needed, update temperature value
    if (need_update)
    {
      vmc_status.temperature = temperature;
      vmc_web.temperature    = true;
    }

    // update graph value
    if (!isnan(vmc_graph.temperature)) vmc_graph.temperature = min (vmc_graph.temperature, temperature); else vmc_graph.temperature = temperature;
  }

  // update current humidity
  humidity = VmcGetHumidity ();
  if (humidity != UINT8_MAX)
  {
    // save current value and ask for JSON update if any change
    if (vmc_status.humidity != humidity)
    {
      vmc_status.humidity = humidity;
      vmc_web.humidity    = true;
      need_update = true;
    }

    // update graph value
    if (vmc_graph.humidity != UINT8_MAX) vmc_graph.humidity = max (vmc_graph.humidity, humidity); else vmc_graph.humidity = humidity;
  }

  // update target humidity
  target = VmcGetTargetHumidity ();
  if (target != UINT8_MAX)
  {
    // save current value and ask for JSON update if any change
    if (vmc_status.target != target) need_update = true;
    vmc_status.target = target;

    // update graph value
    if (vmc_graph.target != UINT8_MAX) vmc_graph.target = min (vmc_graph.target, target); else vmc_graph.target = target;
  } 

  // update relay state
  actual_state = VmcGetRelayState ();
  if (vmc_graph.state != VMC_STATE_HIGH) vmc_graph.state = actual_state;

  // get VMC mode and consider as low if mode disabled and single relay
  vmc_mode = VmcGetMode ();
  if ((vmc_mode == VMC_MODE_DISABLED) && (TasmotaGlobal.devices_present == 1)) vmc_mode = VMC_MODE_LOW;

  // determine relay target state according to vmc mode
  target_state = actual_state;
  switch (vmc_mode)
  {
    case VMC_MODE_LOW: 
      target_state = VMC_STATE_LOW;
      break;
    case VMC_MODE_HIGH:
      target_state = VMC_STATE_HIGH;
      break;
    case VMC_MODE_AUTO: 
      // read humidity threshold
      threshold = VmcGetThreshold ();

      // if humidity is low enough, target VMC state is low speed
      if (humidity + threshold < target) target_state = VMC_STATE_LOW;
      
      // else, if humidity is too high, target VMC state is high speed
      else if (humidity > target + threshold) target_state = VMC_STATE_HIGH;
      break;
  }

  // if VMC state is different than target state, set relay
  if (actual_state != target_state)
  {
    VmcSetRelayState (target_state);
    vmc_web.state = true;
    need_update = true;
  }
  
  // if JSON update needed, publish
  if (need_update == true) VmcShowJSON (false);

  // increment delay counter and if delay reached, update history data
  if (vmc_graph.counter == 0) VmcUpdateGraphData ();
  vmc_graph.counter ++;
  vmc_graph.counter = vmc_graph.counter % VMC_GRAPH_REFRESH;
}

void VmcInit ()
{
  int index;

  // save number of devices and set it to 0 to avoid direct command
// vmc_devices_present = devices_present;
//  devices_present = 0;

  // initialise graph data
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    vmc_graph.arr_temperature[index] = NAN;
    vmc_graph.arr_humidity[index] = UINT8_MAX;
    vmc_graph.arr_target[index] = UINT8_MAX;
    vmc_graph.arr_state[index] = VMC_STATE_NONE;
  }
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// display offload icons
void VmceWebIconOff ()  { Webserver->send (200, "image/png", vmc_icon_off_png,  vmc_icon_off_len);  }
void VmceWebIconLow ()  { Webserver->send (200, "image/png", vmc_icon_slow_png, vmc_icon_slow_len); }
void VmceWebIconHigh () { Webserver->send (200, "image/png", vmc_icon_fast_png, vmc_icon_fast_len); }
void VmceWebIconState ()
{
  uint8_t  device_state = VmcGetRelayState ();

  if (device_state == VMC_STATE_HIGH) VmceWebIconHigh ();
  else if (device_state == VMC_STATE_LOW) VmceWebIconLow ();
  else VmceWebIconOff ();
}

// update status for web client
// format is A1;A2;A3;A4
// A1 : icon update (1:update, empty if no change)
// A2 : new temperature value (empty if no change)
// A3 : new humidity value (empty if no change)
// A4 : graph update (1:update, empty if no update)
void VmcWebUpdate ()
{
  String str_text;

  // A1 : icon state update
  if (vmc_web.state) str_text = "1";
  str_text += ";";

  // A2 : temperature value
  if (vmc_web.temperature) str_text += String (VmcGetTemperature (), 1);
  str_text += ";";

  // A3 : humidity value
  if (vmc_web.humidity) str_text += String (VmcGetHumidity ());
  str_text += ";";

  // A4 : graph update
  if (vmc_web.graph) str_text += "1";

  // send result
  Webserver->send (200, "text/plain", str_text.c_str (), str_text.length ());

  // reset web flags
  vmc_web.state       = false;
  vmc_web.temperature = false;
  vmc_web.humidity    = false;
  vmc_web.graph       = false;
}

// append VMC control button to main page
void VmcWebMainButton ()
{
  // VMC control page button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_PAGE_VMC_CONTROL, D_VMC_CONTROL);
}

// append VMC configuration button to configuration page
void VmcWebConfigButton ()
{
  // VMC configuration button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>"), D_PAGE_VMC_CONFIG, D_VMC_CONFIGURE);
}

// append VMC state to main page
bool VmcWebSensor ()
{
  uint8_t mode, humidity, target;
  String  str_title, str_text, str_value, str_source, str_color;

  // if automatic mode, display humidity and target humidity
  mode = VmcGetMode ();
  if (mode == VMC_MODE_AUTO)
  {
    // read current and target humidity
    humidity = VmcGetHumidity ();
    target   = VmcGetTargetHumidity ();

    // handle sensor source
    switch (vmc_status.humidity_source)
    {
      case VMC_SOURCE_NONE:  // no humidity source available 
        str_source = "";
        str_value  = "--";
        break;
      case VMC_SOURCE_LOCAL:  // local humidity source used 
        str_source = D_VMC_LOCAL;
        str_value  = String (humidity);
        break;
      case VMC_SOURCE_REMOTE:  // remote humidity source used 
        str_source = D_VMC_REMOTE;
        str_value  = String (humidity);
        break;
    }

    // set title and text
    str_title = D_VMC_HUMIDITY;
    if (str_source.length() > 0) str_title += " (" + str_source + ")";
    str_text  = "<b>" + str_value + "</b> / " + String (target) + "%";

    // display
    WSContentSend_PD (PSTR("{s}%s{m}%s{e}"), str_title.c_str(), str_text.c_str());
  }

  // display vmc icon status
  WSContentSend_PD (PSTR("<tr><td colspan=2 style='width:100%;text-align:center;padding:10px;'><img height=64 src='state.png' ></td></tr>\n"));
}

// VMC web page
void VmcWebPageConfig ()
{
  uint8_t value, humidity;
  char    argument[VMC_BUFFER_SIZE];
  String  str_text;

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg("save"))
  {
    // get VMC mode according to MODE parameter
    WebGetArg (D_CMND_VMC_MODE, argument, VMC_BUFFER_SIZE);
    if (strlen(argument) > 0) VmcSetMode ((uint8_t) atoi (argument)); 

    // get VMC target humidity according to TARGET parameter
    WebGetArg (D_CMND_VMC_TARGET, argument, VMC_BUFFER_SIZE);
    if (strlen(argument) > 0) VmcSetTargetHumidity ((uint8_t) atoi (argument));

    // get VMC humidity threshold according to THRESHOLD parameter
    WebGetArg (D_CMND_VMC_THRESHOLD, argument, VMC_BUFFER_SIZE);
    if (strlen(argument) > 0) VmcSetThreshold ((uint8_t) atoi (argument));
  }

  // beginning of form
  WSContentStart_P (D_VMC_CONFIGURE);
  WSContentSendStyle ();
  WSContentSend_P (PSTR("<form method='get' action='%s'>\n"), D_PAGE_VMC_CONFIG);

  WSContentSend_P (PSTR ("<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>\n"), D_VMC_PARAMETERS);

  // get mode and humidity
  value    = VmcGetMode ();
  humidity = VmcGetHumidity ();

  // selection : start
  WSContentSend_P (PSTR ("<p>%s<br/><select name='%s'>"), D_VMC_MODE, D_CMND_VMC_MODE);
  
  // selection : disabled
  if (value == VMC_MODE_DISABLED) str_text = "selected"; else str_text = "";
  WSContentSend_P (VMC_INPUT_OPTION, VMC_MODE_DISABLED, str_text.c_str (), D_VMC_DISABLED);

  // selection : low speed
  if (value == VMC_MODE_LOW) str_text = "selected"; else str_text = "";
  WSContentSend_P (VMC_INPUT_OPTION, VMC_MODE_LOW, str_text.c_str (), D_VMC_LOW);

  // selection : high speed
  if (value == VMC_MODE_HIGH) str_text = "selected"; else str_text = "";
  WSContentSend_P (VMC_INPUT_OPTION, VMC_MODE_HIGH, str_text.c_str (), D_VMC_HIGH);

  // selection : automatic
  if (value == VMC_MODE_AUTO) str_text = "selected"; else str_text = "";
  if (humidity != UINT8_MAX) WSContentSend_P (VMC_INPUT_OPTION, VMC_MODE_AUTO, str_text.c_str (), D_VMC_AUTO);

  // selection : end
  WSContentSend_P (PSTR ("</select></p>\n"));

  // target humidity label and input
  value = VmcGetTargetHumidity ();
  WSContentSend_P (VMC_INPUT_NUMBER, D_VMC_TARGET, D_CMND_VMC_TARGET, VMC_TARGET_MAX, value);

  // humidity threshold label and input
  value = VmcGetThreshold ();
  WSContentSend_P (VMC_INPUT_NUMBER, D_VMC_THRESHOLD, D_CMND_VMC_THRESHOLD, VMC_THRESHOLD_MAX, value);

  WSContentSend_P (PSTR("</fieldset></p>\n"));

  // save button
  WSContentSend_P (PSTR ("<p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), D_SAVE);
  WSContentSend_P (PSTR ("</form>"));

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

// Temperature & humidity graph frame
void VmcWebGraphFrame ()
{
  int   index;
  int   graph_left, graph_right, graph_width;
  float temperature, temp_min, temp_max, temp_scope;

  // start of SVG graph
  WSContentBegin(200, CT_HTML);
  WSContentSend_P (PSTR ("<svg viewBox='%d %d %d %d' preserveAspectRatio='xMinYMinmeet'>\n"), 0, 0, VMC_GRAPH_WIDTH, VMC_GRAPH_HEIGHT);

  // SVG style 
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));
  WSContentSend_P (PSTR ("rect {stroke:grey;fill:none;}\n"));
  WSContentSend_P (PSTR ("line.dash {stroke:grey;stroke-width:1;stroke-dasharray:8;}\n"));
  WSContentSend_P (PSTR ("text.temperature {font-size:18px;fill:yellow;}\n"));
  WSContentSend_P (PSTR ("text.humidity {font-size:18px;fill:orange;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // loop to adjust min and max temperature
  temp_min = VMC_GRAPH_TEMP_MIN;
  temp_max = VMC_GRAPH_TEMP_MAX;
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    // if needed, adjust minimum and maximum temperature
    if (!isnan (vmc_graph.arr_temperature[index]))
    {
      if (temp_min > vmc_graph.arr_temperature[index]) temp_min = floor (vmc_graph.arr_temperature[index]);
      if (temp_max < vmc_graph.arr_temperature[index]) temp_max = ceil (vmc_graph.arr_temperature[index]);
    } 
  }
  temp_scope = temp_max - temp_min;

  // boundaries of SVG graph
  graph_left  = VMC_GRAPH_PERCENT_START * VMC_GRAPH_WIDTH / 100;
  graph_right = VMC_GRAPH_PERCENT_STOP * VMC_GRAPH_WIDTH / 100;
  graph_width = graph_right - graph_left;

  // graph curve zone
  WSContentSend_P (PSTR ("<rect x='%d%%' y='%d%%' width='%d%%' height='%d%%' rx='10' />\n"), VMC_GRAPH_PERCENT_START, 0, VMC_GRAPH_PERCENT_STOP - VMC_GRAPH_PERCENT_START, 100);

  // graph separation lines
  WSContentSend_P (VMC_LINE_DASH, graph_left, 25, graph_right, 25);
  WSContentSend_P (VMC_LINE_DASH, graph_left, 50, graph_right, 50);
  WSContentSend_P (VMC_LINE_DASH, graph_left, 75, graph_right, 75);

  // temperature units
  WSContentSend_P (VMC_TEXT_TEMPERATURE, 2, 4,  String (temp_max, 1).c_str ());
  WSContentSend_P (VMC_TEXT_TEMPERATURE, 2, 27, String (temp_min + temp_scope * 0.75, 1).c_str ());
  WSContentSend_P (VMC_TEXT_TEMPERATURE, 2, 52, String (temp_min + temp_scope * 0.50, 1).c_str ());
  WSContentSend_P (VMC_TEXT_TEMPERATURE, 2, 77, String (temp_min + temp_scope * 0.25, 1).c_str ());
  WSContentSend_P (VMC_TEXT_TEMPERATURE, 2, 99, String (temp_min, 1).c_str ());

  // humidity units
  WSContentSend_P (VMC_TEXT_HUMIDITY, VMC_GRAPH_PERCENT_STOP + 2, 4, 100);
  WSContentSend_P (VMC_TEXT_HUMIDITY, VMC_GRAPH_PERCENT_STOP + 2, 27, 75);
  WSContentSend_P (VMC_TEXT_HUMIDITY, VMC_GRAPH_PERCENT_STOP + 2, 52, 50);
  WSContentSend_P (VMC_TEXT_HUMIDITY, VMC_GRAPH_PERCENT_STOP + 2, 77, 25);
  WSContentSend_P (VMC_TEXT_HUMIDITY, VMC_GRAPH_PERCENT_STOP + 2, 99, 0);

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentEnd();
}

// Temperature & humidity graph data
void VmcWebGraphData ()
{
  bool     draw;
  int      index, array_idx;
  int      graph_left, graph_right, graph_width;
  int      graph_x, graph_y, graph_off, graph_low, graph_high;
  int      unit_width, shift_unit, shift_width;
  uint8_t  humidity;
  float    temperature, temp_min, temp_max, temp_scope;
  TIME_T   current_dst;
  uint32_t current_time;
  String   str_text;

  // vmc state graph position
  graph_off  = VMC_GRAPH_HEIGHT;
  graph_low  = VMC_GRAPH_HEIGHT - VMC_GRAPH_MODE_LOW;
  graph_high = VMC_GRAPH_HEIGHT - VMC_GRAPH_MODE_HIGH;

  // loop to adjust min and max temperature
  temp_min = VMC_GRAPH_TEMP_MIN;
  temp_max = VMC_GRAPH_TEMP_MAX;
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    // if needed, adjust minimum and maximum temperature
    if (!isnan (vmc_graph.arr_temperature[index]))
    {
      if (temp_min > vmc_graph.arr_temperature[index]) temp_min = floor (vmc_graph.arr_temperature[index]);
      if (temp_max < vmc_graph.arr_temperature[index]) temp_max = ceil (vmc_graph.arr_temperature[index]);
    } 
  }
  temp_scope = temp_max - temp_min;

  // boundaries of SVG graph
  graph_left  = VMC_GRAPH_PERCENT_START * VMC_GRAPH_WIDTH / 100;
  graph_right = VMC_GRAPH_PERCENT_STOP * VMC_GRAPH_WIDTH / 100;
  graph_width = graph_right - graph_left;

  // start of SVG graph
  WSContentBegin(200, CT_HTML);
  WSContentSend_P (PSTR ("<svg viewBox='%d %d %d %d' preserveAspectRatio='xMinYMinmeet'>\n"), 0, 0, VMC_GRAPH_WIDTH, VMC_GRAPH_HEIGHT);

  // SVG style 
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));
  WSContentSend_P (PSTR ("polyline {fill:none;stroke-width:1;}\n"));
  WSContentSend_P (PSTR ("polyline.vmc {stroke:white;stroke-width:2;}\n"));
  WSContentSend_P (PSTR ("polyline.target {stroke:orange;stroke-dasharray:4;}\n"));
  WSContentSend_P (PSTR ("polyline.temperature {stroke:yellow;}\n"));
  WSContentSend_P (PSTR ("polyline.humidity {stroke:orange;}\n"));
  WSContentSend_P (PSTR ("line.time {stroke:white;stroke-width:1;}\n"));
  WSContentSend_P (PSTR ("text.time {font-size:16px;fill:white;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // ---------------
  //     Curves
  // ---------------

  // loop to display vmc state curve
  WSContentSend_P (PSTR ("<polyline class='vmc' points='"));
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    // get array index and read vmc state
    array_idx  = (index + vmc_graph.index) % VMC_GRAPH_SAMPLE;
    if (vmc_graph.arr_state[array_idx] != VMC_STATE_NONE)
    {
      // calculate current position and add the point to the line
      graph_x = graph_left + (graph_width * index / VMC_GRAPH_SAMPLE);
      if (vmc_graph.arr_state[array_idx] == VMC_STATE_HIGH) graph_y = graph_high;
      else if (vmc_graph.arr_state[array_idx] == VMC_STATE_LOW) graph_y = graph_low;
      else graph_y = graph_off;
      WSContentSend_P (PSTR("%d,%d "), graph_x, graph_y);
    }
  }
  WSContentSend_P (PSTR("'/>\n"));

  // loop to display target humidity curve
  WSContentSend_P (PSTR ("<polyline class='target' points='"));
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    // if humidity has been mesured
    array_idx = (index + vmc_graph.index) % VMC_GRAPH_SAMPLE;
    if (vmc_graph.arr_target[array_idx] != UINT8_MAX)
    {
      // calculate current position and add the point to the line
      graph_x = graph_left + (graph_width * index / VMC_GRAPH_SAMPLE);
      graph_y = VMC_GRAPH_HEIGHT - (vmc_graph.arr_target[array_idx] * VMC_GRAPH_HEIGHT / 100);
      WSContentSend_P (PSTR("%d,%d "), graph_x, graph_y);
    }
  }
  WSContentSend_P (PSTR("'/>\n"));

  // loop to display temperature curve
  WSContentSend_P (PSTR ("<polyline class='temperature' points='"));
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    // if temperature has been mesured
    array_idx   = (index + vmc_graph.index) % VMC_GRAPH_SAMPLE;
    if (!isnan (vmc_graph.arr_temperature[array_idx]))
    {
      // calculate current position and add the point to the line
      graph_x = graph_left + (graph_width * index / VMC_GRAPH_SAMPLE);
      graph_y = VMC_GRAPH_HEIGHT - int (((vmc_graph.arr_temperature[array_idx] - temp_min) / temp_scope) * VMC_GRAPH_HEIGHT);
      WSContentSend_P (PSTR("%d,%d "), graph_x, graph_y);
    }
  }
  WSContentSend_P (PSTR("'/>\n"));

  // loop to display humidity curve
  WSContentSend_P (PSTR ("<polyline class='humidity' points='"));
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    // if temperature value is defined
    array_idx = (index + vmc_graph.index) % VMC_GRAPH_SAMPLE;
    if (vmc_graph.arr_humidity[array_idx] != UINT8_MAX)
    {
      // calculate current position and add the point to the line
      graph_x = graph_left + (graph_width * index / VMC_GRAPH_SAMPLE);
      graph_y = VMC_GRAPH_HEIGHT - (vmc_graph.arr_humidity[array_idx] * VMC_GRAPH_HEIGHT / 100);
      WSContentSend_P (PSTR("%d,%d "), graph_x, graph_y);
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

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentEnd();
}

// VMC control public web page
void VmcWebPageControl ()
{
  bool    updated = false;
  uint8_t vmc_mode;
  String  str_text;

  // check if vmc state has changed
  if (Webserver->hasArg(D_CMND_VMC_LOW)) { updated = true; VmcSetMode (VMC_MODE_LOW); }
  else if (Webserver->hasArg(D_CMND_VMC_HIGH)) { updated = true; VmcSetMode (VMC_MODE_HIGH); }
  else if (Webserver->hasArg(D_CMND_VMC_AUTO)) { updated = true; VmcSetMode (VMC_MODE_AUTO); }

  // beginning of form without authentification with 60 seconds auto refresh
  WSContentStart_P (D_VMC_CONTROL, false);
  WSContentSend_P (PSTR ("</script>\n"));

  // if parameters have been updated, auto reload page
  if (updated)
  {
    WSContentSend_P (PSTR ("<meta http-equiv='refresh' content='0;URL=/%s' />\n"), D_PAGE_VMC_CONTROL);
    WSContentSend_P (PSTR ("</head>\n"));
    WSContentSend_P (PSTR ("<body bgcolor='#252525'></body>\n"));
    WSContentSend_P (PSTR ("</html>\n"));
    WSContentEnd ();
  }

  // else, display control page
  else
  {
    // page data refresh script
    WSContentSend_P (PSTR ("<script type='text/javascript'>\n"));
    WSContentSend_P (PSTR ("function update() {\n"));
    WSContentSend_P (PSTR ("httpUpd=new XMLHttpRequest();\n"));
    WSContentSend_P (PSTR ("httpUpd.onreadystatechange=function() {\n"));
    WSContentSend_P (PSTR (" if (httpUpd.responseText.length>0) {\n"));
    WSContentSend_P (PSTR ("  arr_param=httpUpd.responseText.split(';');\n"));
    WSContentSend_P (PSTR ("  str_random=Math.floor(Math.random()*100000);\n"));
    WSContentSend_P (PSTR ("  if (arr_param[0]==1) {document.getElementById('state').setAttribute('src','state.png?rnd='+str_random);}\n"));
    WSContentSend_P (PSTR ("  if (arr_param[1]!='') {document.getElementById('temp').innerHTML=arr_param[1]+' °C';}\n"));
    WSContentSend_P (PSTR ("  if (arr_param[2]!='') {document.getElementById('humi').innerHTML=arr_param[2]+' %%';}\n"));
    WSContentSend_P (PSTR ("  if (arr_param[3]==1) {document.getElementById('data').data='data.svg?rnd='+str_random;}\n"));
    WSContentSend_P (PSTR (" }\n"));
    WSContentSend_P (PSTR ("}\n"));
    WSContentSend_P (PSTR ("httpUpd.open('GET','vmc.upd',true);\n"));
    WSContentSend_P (PSTR ("httpUpd.send();\n"));
    WSContentSend_P (PSTR ("}\n"));
    WSContentSend_P (PSTR ("setInterval(function() {update();},1000);\n"));
    WSContentSend_P (PSTR ("</script>\n"));

    // page style
    WSContentSend_P (PSTR ("<style>\n"));
    WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));
    WSContentSend_P (PSTR ("div {width:100%%;margin:1rem auto;padding:3px 0px;text-align:center;vertical-align:middle;}\n"));
    WSContentSend_P (PSTR ("img {height:64px;}\n"));
    WSContentSend_P (PSTR ("span {font-size:4vw;padding:0.25rem 1rem;margin:0.5rem;border-radius:8px;width:auto;}\n"));
    WSContentSend_P (PSTR ("span.temp {color:yellow;border:1px yellow solid;}\n"));
    WSContentSend_P (PSTR ("span.humi {color:orange;border:1px orange solid;}\n"));
    WSContentSend_P (PSTR (".title {font-size:6vw;font-weight:bold;}\n"));
    WSContentSend_P (PSTR (".button {font-size:3vw;padding:0.5rem 1rem;border:1px #666 solid;background:none;color:#fff;border-radius:8px;}\n"));
    WSContentSend_P (PSTR (".active {background:#666;}\n"));
    WSContentSend_P (PSTR (".svg-container {position:relative;vertical-align:middle;overflow:hidden;width:100%%;max-width:%dpx;padding-bottom:65%%;}\n"), VMC_GRAPH_WIDTH);
    WSContentSend_P (PSTR (".svg-content {display:inline-block;position:absolute;top:0;left:0;}\n"));
    WSContentSend_P (PSTR ("</style>\n"));

    WSContentSend_P (PSTR ("</head>\n"));

    // page body
    WSContentSend_P (PSTR ("<body>\n"));
    WSContentSend_P (PSTR ("<form name='%s' method='get' action='%s'>\n"), D_PAGE_VMC_CONTROL, D_PAGE_VMC_CONTROL);

    // room name
    WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText(SET_DEVICENAME));

    // vmc icon status
    WSContentSend_PD (PSTR("<div><img id='state' src='state.png?rnd=0'></div>\n"));

    // vmc mode selector
    vmc_mode = VmcGetMode ();
    WSContentSend_P (PSTR ("<div>\n"));
    if (vmc_mode == VMC_MODE_AUTO) str_text="active"; else str_text="";
    WSContentSend_P (VMC_INPUT_BUTTON, D_CMND_VMC_AUTO, str_text.c_str (), D_VMC_BTN_AUTO);
    if (vmc_mode == VMC_MODE_LOW) str_text="active"; else str_text="";
    WSContentSend_P (VMC_INPUT_BUTTON, D_CMND_VMC_LOW, str_text.c_str (), D_VMC_BTN_LOW);
    if (vmc_mode == VMC_MODE_HIGH) str_text="active"; else str_text="";
    WSContentSend_P (VMC_INPUT_BUTTON, D_CMND_VMC_HIGH, str_text.c_str (), D_VMC_BTN_HIGH);
    WSContentSend_P (PSTR ("</div>\n"));

    // temperature and humidity values
    WSContentSend_P (PSTR ("<div>"));
    str_text = String (VmcGetTemperature (), 1);
    WSContentSend_P (PSTR ("<span class='temp' id='temp'>%s °C</span>"), str_text.c_str ());
    str_text = String (VmcGetHumidity ());
    WSContentSend_P (PSTR ("<span class='humi' id='humi'>%s %%</span>"), str_text.c_str ());
    WSContentSend_P (PSTR ("</div>\n"));

    // graph base and data
    WSContentSend_P (PSTR ("<div class='svg-container'>\n"));
    WSContentSend_P (PSTR ("<object class='svg-content' id='base' type='image/svg+xml' width='%d%%' height='%d%%' data='%s'></object>\n"), 100, 100, D_PAGE_VMC_BASE_SVG);
    WSContentSend_P (PSTR ("<object class='svg-content' id='data' type='image/svg+xml' width='%d%%' height='%d%%' data='%s?rnd=0'></object>\n"), 100, 100, D_PAGE_VMC_DATA_SVG);
    WSContentSend_P (PSTR ("</div>\n"));

    // end of page
    WSContentSend_P (PSTR ("</form>\n"));
    WSContentStop ();
  }
}

#endif  // USE_WEBSERVER

/*******************************************************\
 *                      Interface
\*******************************************************/

bool Xsns98 (byte callback_id)
{
  bool result = false;

  // main callback switch
  switch (callback_id)
  {
    case FUNC_INIT:
      VmcInit ();
      break;
    case FUNC_COMMAND:
      result = VmcCommand ();
      break;
    case FUNC_EVERY_SECOND:
      VmcEverySecond ();
      break;
    case FUNC_JSON_APPEND:
      VmcShowJSON (true);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      // pages
      Webserver->on ("/" D_PAGE_VMC_CONFIG,  VmcWebPageConfig);
      Webserver->on ("/" D_PAGE_VMC_CONTROL, VmcWebPageControl);
      Webserver->on ("/" D_PAGE_VMC_BASE_SVG, VmcWebGraphFrame);
      Webserver->on ("/" D_PAGE_VMC_DATA_SVG, VmcWebGraphData);

      // icons
      Webserver->on ("/off.png", VmceWebIconOff);
      Webserver->on ("/low.png", VmceWebIconLow);
      Webserver->on ("/high.png", VmceWebIconHigh);
      Webserver->on ("/state.png", VmceWebIconState);

      // update status
      Webserver->on ("/vmc.upd", VmcWebUpdate);

      break;
    case FUNC_WEB_SENSOR:
      VmcWebSensor ();
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      VmcWebMainButton ();
      break;
    case FUNC_WEB_ADD_BUTTON:
      VmcWebConfigButton ();
      break;
#endif  // USE_WEBSERVER

  }
  return result;
}

#endif // USE_VMC
