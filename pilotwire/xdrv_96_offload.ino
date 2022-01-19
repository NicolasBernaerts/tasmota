/*
  xdrv_96_offload.ino - Device offloading thru MQTT instant and max power
  
  Copyright (C) 2020  Nicolas Bernaerts
    23/03/2020 - v1.0   - Creation
    26/05/2020 - v1.1   - Add Information JSON page
    07/07/2020 - v1.2   - Enable discovery (mDNS)
    20/07/2020 - v1.3   - Change offloading delays to seconds
    22/07/2020 - v1.3.1 - Update instant device power in case of Sonoff energy module
    05/08/2020 - v1.4   - Add /control page to have a public switch
                          If available, get max power thru MQTT meter
                          Phase selection and disable mDNS 
    22/08/2020 - v1.4.1 - Add restart after offload configuration
    05/09/2020 - v1.4.2 - Correct display exception on mainpage
    22/08/2020 - v1.5   - Save offload config using new Settings text
                          Add restart after offload configuration
    15/09/2020 - v1.6   - Add OffloadJustSet and OffloadJustRemoved
    19/09/2020 - v2.0   - Add Contract power adjustment in %
                          Set offload priorities as standard options
                          Add icons to /control page
    15/10/2020 - v2.1   - Expose icons on web server
    16/10/2020 - v2.2   - Handle priorities as list of device types
                          Add randomisation to reconnexion
    23/10/2020 - v2.3   - Update control page in real time
    05/11/2020 - v2.4   - Tasmota 9.0 compatibility
    11/11/2020 - v2.5   - Add offload history pages (/histo and /histo.json)
    23/04/2021 - v3.0   - Add fixed IP and remove use of String to avoid heap fragmentation
    22/09/2021 - v3.1   - Add LittleFS support to store offload events
    17/01/2022 - v3.2   - Use device type priority to handle delays
                   
  When using LittleFS, settings are stored under /offload.cfg
  If there is no LittleFS partition, settings are stored using unused KNX parameters :
    - Settings.knx_GA_addr[0]   = Maximum Power of plugged appliance (W) 
    - Settings.knx_GA_addr[1]   = Maximum power of contract (W) 
    - Settings.knx_GA_addr[2]   = Device priority
    - Settings.knx_GA_addr[5]   = Acceptable % of overload
    - Settings.knx_GA_addr[7]   = Appliance type

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

/*************************************************\
 *                  Offload
\*************************************************/

#ifdef USE_OFFLOAD

#define XDRV_96                 96

#define OFFLOAD_PHASE_MAX       3
#define OFFLOAD_EVENT_MAX       10
#define OFFLOAD_POWER_MAX       10000
#define OFFLOAD_DELAY_MAX       1000
#define OFFLOAD_PRIORITY_MAX    10

# define OFFLOAD_JSON_UPDATE    5

#define D_PAGE_OFFLOAD_CONFIG    "offload"
#define D_PAGE_OFFLOAD_CONTROL   "control"
#define D_PAGE_OFFLOAD_HISTORY   "histo"

#define D_CMND_OFFLOAD_PREFIX    "off_"
#define D_CMND_OFFLOAD_POWER     "power"
#define D_CMND_OFFLOAD_TYPE      "type"
#define D_CMND_OFFLOAD_PRIORITY  "priority"
//#define D_CMND_OFFLOAD_BEFORE    "before"
//#define D_CMND_OFFLOAD_AFTER     "after"
//#define D_CMND_OFFLOAD_RANDOM    "random"
#define D_CMND_OFFLOAD_CONTRACT  "contract"
#define D_CMND_OFFLOAD_ADJUST    "adjust"
//#define D_CMND_OFFLOAD_PHASE     "phase"
#define D_CMND_OFFLOAD_TOPIC     "topic"
#define D_CMND_OFFLOAD_KINST     "kinst"
#define D_CMND_OFFLOAD_KMAX      "kmax"

#define D_JSON_OFFLOAD           "Offload"
#define D_JSON_OFFLOAD_STATE     "State"
#define D_JSON_OFFLOAD_STAGE     "Stage"
//#define D_JSON_OFFLOAD_PHASE     "Phase"
//#define D_JSON_OFFLOAD_ADJUST    "Adjust"
//#define D_JSON_OFFLOAD_BEFORE    "Before"
//#define D_JSON_OFFLOAD_AFTER     "After"
//#define D_JSON_OFFLOAD_CONTRACT  "Contract"
#define D_JSON_OFFLOAD_DEVICE    "Device"
//#define D_JSON_OFFLOAD_MAX       "Max"
#define D_JSON_OFFLOAD_TOPIC     "Topic"
#define D_JSON_OFFLOAD_KEY_INST  "KeyInst"
#define D_JSON_OFFLOAD_KEY_MAX   "KeyMax"

#define D_OFFLOAD                "Offload"
#define D_OFFLOAD_CONFIGURE      "Configure"
#define D_OFFLOAD_CONTROL        "Control"
#define D_OFFLOAD_HISTORY        "History"
#define D_OFFLOAD_DEVICE         "Device"
//#define D_OFFLOAD_DELAY          "Delay"
#define D_OFFLOAD_TYPE           "Type"
//#define D_OFFLOAD_INSTCONTRACT   "Act/Max"
//#define D_OFFLOAD_PHASE          "Phase"
#define D_OFFLOAD_ADJUST         "Adjustment"
#define D_OFFLOAD_CONTRACT       "Contract"
#define D_OFFLOAD_METER          "Meter"
#define D_OFFLOAD_TOPIC          "MQTT Topic"
#define D_OFFLOAD_KEY_INST       "Power JSON Key"
#define D_OFFLOAD_KEY_MAX        "Contract JSON Key"
//#define D_OFFLOAD_ACTIVE         "Offload active"
#define D_OFFLOAD_POWER          "Power"
#define D_OFFLOAD_PRIORITY       "Priority"
//#define D_OFFLOAD_BEFORE         "Off delay"
//#define D_OFFLOAD_AFTER          "On delay"
//#define D_OFFLOAD_RANDOM         "Random"
#define D_OFFLOAD_TOGGLE         "Toggle plug switch"

#define D_OFFLOAD_UNIT_VA        "VA"
#define D_OFFLOAD_UNIT_W         "W"
#define D_OFFLOAD_UNIT_SEC       "sec."
#define D_OFFLOAD_UNIT_PERCENT   "%"
#define D_OFFLOAD_SELECTED       "selected"

#define D_OFFLOAD_CFG            "/offload.cfg"
#define D_OFFLOAD_CSV            "/offload.csv"

// offloading commands
const char kOffloadCommands[] PROGMEM = D_CMND_OFFLOAD_PREFIX "|" D_CMND_OFFLOAD_TYPE "|" D_CMND_OFFLOAD_PRIORITY "|" D_CMND_OFFLOAD_POWER  "|" D_CMND_OFFLOAD_CONTRACT "|" D_CMND_OFFLOAD_ADJUST "|" D_CMND_OFFLOAD_TOPIC "|" D_CMND_OFFLOAD_KINST "|" D_CMND_OFFLOAD_KMAX;
void (* const OffloadCommand[])(void) PROGMEM = { &CmndOffloadType, &CmndOffloadPriority, &CmndOffloadPower, &CmndOffloadContract, &CmndOffloadAdjust, &CmndOffloadPowerTopic, &CmndOffloadPowerKeyInst, &CmndOffloadPowerKeyMax };

// strings
const char OFFLOAD_FIELDSET_START[] PROGMEM = "<fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>\n";
const char OFFLOAD_FIELDSET_STOP[]  PROGMEM = "</fieldset><br />\n";
const char OFFLOAD_INPUT_NUMBER[]   PROGMEM = "<p>%s<span class='key'>%s</span><br><input class='%s' type='number' name='%s' id='%s' min='%d' max='%d' step='%d' value='%d'></p>\n";
const char OFFLOAD_INPUT_TEXT[]     PROGMEM = "<p>%s<span class='key'>%s</span><br><input name='%s' value='%s' placeholder='%s'></p>\n";
const char OFFLOAD_BUTTON[]         PROGMEM = "<p><form action='%s' method='get'><button>%s</button></form></p>";

// definition of types of device with associated delays
enum OffloadDevice                     { OFFLOAD_DEVICE_APPLIANCE, OFFLOAD_DEVICE_LIGHT, OFFLOAD_DEVICE_FRIDGE, OFFLOAD_DEVICE_WASHING, OFFLOAD_DEVICE_DISH, OFFLOAD_DEVICE_DRIER, OFFLOAD_DEVICE_CUMULUS, OFFLOAD_DEVICE_IRON, OFFLOAD_DEVICE_BATHROOM, OFFLOAD_DEVICE_OFFICE, OFFLOAD_DEVICE_LIVING, OFFLOAD_DEVICE_ROOM, OFFLOAD_DEVICE_KITCHEN, OFFLOAD_DEVICE_MAX };
const char kOffloadDevice[] PROGMEM  = "Other|Light|Fridge|Washing machine|Dish washer|Drier|Cumulus|Iron|Bathroom|Office|Living|Sleeping|Kitchen";               // labels
const uint8_t arr_offload_priority[] = { 4,   1,    1,     3,              3,          4,    1,      2,   6,       7,     8,     9,       10 };

/****************************************\
 *               Icons
 * 
 *      xxd -i -c 256 icon.png
\****************************************/

#define OFFLOAD_ICON_POWER "off-power.png"      

// icon : offload
#define OFFLOAD_ICON_ACTIVE "off-active.png"      
unsigned char offload_icon_png[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x01, 0x03, 0x00, 0x00, 0x00, 0xf9, 0xf0, 0xf3, 0x88, 0x00, 0x00, 0x00, 0x06, 0x50, 0x4c, 0x54, 0x45, 0xcc, 0xdb, 0x55, 0xff, 0x99, 0x00, 0xdf, 0xa0, 0xd3, 0x66, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x00, 0x01, 0x62, 0x4b, 0x47, 0x44, 0x00, 0x88, 0x05, 0x1d, 0x48, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0d, 0xd7, 0x00, 0x00, 0x0d, 0xd7, 0x01, 0x42, 0x28, 0x9b, 0x78, 0x00, 0x00, 0x00, 0x07, 0x74, 0x49, 0x4d, 0x45, 0x07, 0xe4, 0x0a, 0x18, 0x0d, 0x27, 0x2e, 0x90, 0xd7, 0xb8, 0x1c, 0x00, 0x00, 0x01, 0xcf, 0x49, 0x44, 0x41, 0x54, 0x48, 0xc7, 0x95, 0x96, 0x41, 0x72, 0x83, 0x30, 0x0c, 0x45, 0xe5, 0xba, 0x33, 0xec, 0xea, 0x23, 0x70, 0x93, 0xd2, 0x93, 0x95, 0x1c, 0x8d, 0xa3, 0xf8, 0x08, 0x74, 0xba, 0xf1, 0x82, 0xb1, 0x2a, 0x63, 0x8c, 0xf5, 0xc5, 0x24, 0x4c, 0xb3, 0x49, 0xf2, 0x06, 0xa2, 0x67, 0x49, 0x48, 0x21, 0xfa, 0xcf, 0xeb, 0xcb, 0x82, 0xc5, 0x7c, 0x77, 0x17, 0xf0, 0x30, 0xc0, 0x5b, 0x30, 0x5c, 0x80, 0x0d, 0x12, 0x2c, 0x18, 0x6f, 0xc1, 0xa7, 0x05, 0xdf, 0x16, 0xe4, 0x67, 0xa0, 0x09, 0xba, 0xad, 0xf9, 0x1c, 0x67, 0xf2, 0x0d, 0x84, 0x68, 0xc0, 0xb8, 0x1e, 0x57, 0xa6, 0x06, 0x8e, 0x0f, 0xa1, 0x81, 0xb0, 0x19, 0x30, 0x64, 0xbc, 0x92, 0x3c, 0xd7, 0xf7, 0x69, 0x6d, 0x99, 0xe3, 0x2a, 0x32, 0x37, 0x40, 0xbc, 0x18, 0x30, 0x57, 0x11, 0x3e, 0x41, 0xbd, 0xd9, 0x71, 0x24, 0x10, 0xf1, 0x1d, 0x54, 0x11, 0x05, 0xaa,
  0xc8, 0xd0, 0x41, 0x15, 0x19, 0xf8, 0xac, 0x64, 0x15, 0x09, 0x1d, 0x54, 0x91, 0x51, 0x81, 0x5d, 0x64, 0xe2, 0x5e, 0xc9, 0x5d, 0x44, 0x83, 0x5d, 0x64, 0x56, 0x60, 0x17, 0x61, 0x05, 0x8a, 0x88, 0x63, 0x56, 0x9d, 0xc2, 0x06, 0x14, 0x11, 0xaf, 0x41, 0x11, 0x41, 0x20, 0x22, 0x81, 0x75, 0x69, 0x45, 0x04, 0x81, 0x88, 0x8c, 0x00, 0x44, 0x64, 0xe4, 0x4d, 0xf7, 0x5f, 0x16, 0x73, 0x0d, 0x44, 0x04, 0x81, 0x88, 0xe0, 0x2d, 0x22, 0x12, 0x38, 0x11, 0x88, 0x0c, 0x08, 0xa6, 0xd5, 0x23, 0x18, 0x93, 0x43, 0x20, 0x22, 0xbd, 0x92, 0x87, 0xc8, 0x0c, 0xa0, 0x88, 0x00, 0x28, 0x22, 0x00, 0x8a, 0x48, 0x24, 0x2b, 0xc2, 0xaa, 0x54, 0x12, 0xc3, 0x1b, 0xb0, 0x49, 0x96, 0x55, 0x21, 0x9c, 0x1c, 0x8d, 0xb1, 0x10, 0x22, 0xa2, 0x73, 0x26, 0xbf, 0x88, 0x19, 0x11, 0x80, 0x19, 0x09, 0x6c, 0x32, 0x22, 0x40, 0x44, 0x94, 0xeb, 0xc4, 0x9c, 0x54, 0x1b, 0x56, 0xe0, 0xb4, 0xd7, 0xcc, 0x45, 0x04, 0x81, 0x88, 0xa8, 0x71, 0xc1, 0xc5, 0x7b, 0x22, 0x03, 0x70, 0x18, 0x48, 0xdf, 0x7e, 0x18, 0xb0, 0xd2, 0x1d, 0x48, 0x66, 0xbc, 0x60, 0xb1, 0x0b, 0xe0, 0x7b, 0x80, 0x73, 0x4d, 0x8e, 0x17, 0x6f, 0x01, 0x8a, 0xc8, 0x79, 0x93, 0x05, 0xf9, 0x35, 0x60, 0xfe, 0x65, 0x03, 0x7e, 0x50, 0x44, 0xa2, 0x62, 0x5c, 0xa9, 0xd4, 0x1c, 0xa1, 0xa7, 0x24, 0xa7, 0x2b, 0x02, 0x1a, 0xb1, 0xfd, 0xf3, 0x39, 0xd0, 0x4e, 0x30, 0x64, 0x68, 0xd4, 0xed, 0x1c, 0x68, 0x07, 0x48, 0x04, 0xc5, 0xdd, 0x7b, 0x01, 0x41, 0x3c, 0x07, 0x5a, 0x07, 0x53, 0xd2, 0x60, 0x51, 0xf3, 0xb2, 0x8d, 0xa4, 0x00, 0x5d, 0xf7, 0x20, 0x10, 0x09, 0xbc, 0xc7, 0x56, 0x53, 0x27, 0xab, 0xc9, 0xba, 0xd7, 0x69, 0x53, 0x93, 0xb5,
  0x03, 0xf5, 0xdc, 0xd5, 0x88, 0x2a, 0x23, 0xf5, 0x89, 0x53, 0x22, 0x53, 0xd4, 0x23, 0xbe, 0x80, 0xa5, 0xfa, 0xf6, 0x3a, 0x2d, 0x7a, 0xc4, 0x17, 0xf0, 0x20, 0x14, 0x99, 0x8f, 0x62, 0x44, 0xb3, 0xb0, 0xba, 0x48, 0x36, 0xfb, 0x81, 0x36, 0xb3, 0x41, 0xc8, 0x2e, 0x1d, 0x6a, 0x6b, 0xe9, 0x5c, 0x29, 0xc7, 0x07, 0xdf, 0xa2, 0xb8, 0x68, 0xd6, 0xf6, 0x65, 0x7f, 0x5f, 0xd6, 0xb5, 0xa7, 0x3b, 0xf0, 0x66, 0xc1, 0xfb, 0x8b, 0x7f, 0x14, 0x7f, 0x6e, 0xc8, 0x4e, 0x9e, 0xa5, 0x4d, 0x6a, 0x22, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
unsigned int offload_icon_len = 604;

#ifdef USE_OFFLOAD_WEB

// icon : power off
#define OFFLOAD_ICON_OFF "off-off.png"      
unsigned char offload_power_off_png[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x02, 0x03, 0x00, 0x00, 0x00, 0xbe, 0x50, 0x89, 0x58, 0x00, 0x00, 0x00, 0x09, 0x50, 0x4c, 0x54, 0x45, 0x00, 0x00, 0x00, 0xff, 0x26, 0x00, 0x7a, 0x7a, 0x7a, 0x15, 0x40, 0x7c, 0x70, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x00, 0x01, 0x62, 0x4b, 0x47, 0x44, 0x00, 0x88, 0x05, 0x1d, 0x48, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x10, 0xe0, 0x00, 0x00, 0x10, 0xe0, 0x01, 0xd9, 0xa7, 0xc1, 0x74, 0x00, 0x00, 0x01, 0xd0, 0x49, 0x44, 0x41, 0x54, 0x58, 0xc3, 0xed, 0x97, 0xcd, 0x6d, 0x03, 0x21, 0x10, 0x85, 0x31, 0x52, 0x2e, 0xdc, 0xd3, 0x04, 0x55, 0xa4, 0x84, 0x1c, 0x4c, 0x3f, 0x94, 0xc2, 0x11, 0x4d, 0x95, 0x09, 0xb6, 0x97, 0xf9, 0x65, 0x8c, 0x2d, 0x47, 0x8a, 0xa2, 0x70, 0x83, 0xfd, 0xe0, 0xbd, 0x61, 0x81, 0x81, 0x10, 0x78, 0x29, 0x25, 0xb8, 0xe5, 0xad, 0x94, 0x4f, 0x17, 0x78, 0x2f, 0xe5, 0xec, 0x02, 0xa5, 0xf8, 0x1a, 0xa7, 0x01, 0x7c, 0xf8, 0x16, 0x7c, 0x13, 0xef, 0x03, 0x38, 0xff, 0x28, 0x50, 0xca, 0x9d, 0x30, 0xfe, 0x81, 0xbf, 0x01, 0xa4, 0xee, 0x03, 0x11, 0xa0, 0xb9, 0x40, 0x02, 0xe8, 0x0c, 0xc8, 0x50, 0x19, 0x00, 0xdf, 0x85, 0x02, 0xf1, 0xda, 0x81, 0x2a, 0xc0, 0xe8, 0x32, 0x81, 0x7c, 0xed, 0x40, 0x15, 0x2e, 0x26, 0x26, 0x70, 0xab, 0xcf, 0x32, 0x3a, 0x8c, 0x31, 0x0f, 0x20, 0xde, 0xea, 0xfb, 0x00, 0x08, 0xe0, 0x22, 0x09, 0xc2, 0xe3, 0x68, 0xe0, 0x40, 0xe5, 0x1e, 0x35, 0xd0, 0xb8, 0x05, 0x0a, 0x64, 0xe0, 0x26,
  0xee, 0x02, 0x60, 0x03, 0x20, 0x3c, 0x6a, 0xa0, 0x72, 0xa0, 0x2f, 0x81, 0xb4, 0x02, 0x1a, 0xf7, 0xa8, 0x26, 0x6a, 0xba, 0x44, 0x7e, 0x01, 0xc0, 0x54, 0x64, 0xff, 0x02, 0xc3, 0xc0, 0x9a, 0x0d, 0x60, 0x10, 0x7c, 0x3d, 0xcc, 0x30, 0x22, 0x5a, 0x66, 0x2b, 0x6a, 0x02, 0x09, 0x2b, 0x74, 0x4d, 0x62, 0x9c, 0xbb, 0x00, 0xdf, 0x38, 0x14, 0x20, 0xbf, 0x8e, 0xee, 0x0b, 0x9c, 0x88, 0x8c, 0xf0, 0x03, 0x40, 0x22, 0x00, 0x89, 0x08, 0x81, 0x48, 0x66, 0x6a, 0x13, 0x90, 0xe7, 0x03, 0xb6, 0x46, 0x0f, 0xa8, 0x13, 0xe8, 0x12, 0xc8, 0x2f, 0x07, 0x9a, 0x04, 0x70, 0xf7, 0xd1, 0x6d, 0x46, 0x92, 0xda, 0x26, 0xd0, 0xf6, 0x81, 0x2a, 0x53, 0x73, 0x7c, 0x0e, 0x20, 0xd9, 0xdf, 0x06, 0xc8, 0xfd, 0x61, 0x01, 0xe0, 0x0d, 0x64, 0x01, 0x9c, 0xe6, 0xfd, 0x62, 0x01, 0xc8, 0x83, 0xed, 0x97, 0x03, 0xf6, 0xcf, 0x7a, 0x0e, 0x88, 0x1e, 0x50, 0xf7, 0x81, 0x2e, 0x81, 0xfc, 0x72, 0x00, 0x24, 0x40, 0xb6, 0xb4, 0x07, 0x48, 0xd6, 0x48, 0x63, 0x3b, 0x40, 0xb6, 0x26, 0x22, 0xd9, 0x67, 0x94, 0x07, 0x74, 0x2b, 0xca, 0x2e, 0x0f, 0x52, 0x19, 0x44, 0x5b, 0x02, 0xab, 0xb3, 0xda, 0x06, 0x22, 0x68, 0x97, 0xc9, 0xc8, 0x17, 0x5d, 0x7b, 0x3c, 0x46, 0x05, 0x6d, 0xc2, 0xcc, 0x59, 0x55, 0x59, 0x00, 0x95, 0x37, 0x83, 0xdd, 0x92, 0x95, 0x06, 0x70, 0x20, 0x49, 0x8d, 0x43, 0xa1, 0x89, 0x7a, 0x17, 0x0a, 0x32, 0xfb, 0xcf, 0x06, 0x59, 0x9f, 0x92, 0xc7, 0x10, 0xc7, 0x00, 0x20, 0x4d, 0xdf, 0x88, 0x0c, 0x52, 0x12, 0x9b, 0x46, 0x1b, 0xab, 0x88, 0x30, 0x44, 0x69, 0x6a, 0xe2, 0x44, 0xa9, 0x6a, 0x62, 0x44, 0x09, 0x0f, 0x00, 0xd9, 0xfa, 0xde, 0xf5, 0xfa, 0x58, 0x79, 0x5c, 0xb8, 0xac, 0xe1,
  0x8e, 0x09, 0x63, 0x1b, 0x2c, 0x2d, 0x98, 0x1a, 0xd5, 0xda, 0x28, 0x4b, 0x05, 0x43, 0xa3, 0x9b, 0x27, 0xef, 0x5a, 0x41, 0x6b, 0x04, 0xfb, 0x5c, 0x34, 0x67, 0xc9, 0xd4, 0xa8, 0xfa, 0x05, 0x91, 0x3d, 0x8b, 0x6a, 0x88, 0x6a, 0x3d, 0x42, 0xb2, 0x3f, 0x00, 0x0b, 0x64, 0xf1, 0x46, 0x8a, 0xae, 0x00, 0x09, 0xb5, 0x39, 0x0f, 0x2d, 0x3d, 0xfe, 0x17, 0x69, 0x39, 0x1a, 0x70, 0x7c, 0x3a, 0x50, 0x97, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
unsigned int offload_power_off_len = 589;

// icon : power on
#define OFFLOAD_ICON_ON "off-on.png"      
unsigned char offload_power_on_png[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x02, 0x03, 0x00, 0x00, 0x00, 0xbe, 0x50, 0x89, 0x58, 0x00, 0x00, 0x00, 0x09, 0x50, 0x4c, 0x54, 0x45, 0x00, 0x00, 0x00, 0x7a, 0x7a, 0x7a, 0x3c, 0xaa, 0x00, 0xe7, 0xa3, 0x37, 0x66, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x00, 0x01, 0x62, 0x4b, 0x47, 0x44, 0x00, 0x88, 0x05, 0x1d, 0x48, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x10, 0xe0, 0x00, 0x00, 0x10, 0xe0, 0x01, 0xd9, 0xa7, 0xc1, 0x74, 0x00, 0x00, 0x01, 0xd6, 0x49, 0x44, 0x41, 0x54, 0x58, 0xc3, 0xed, 0x97, 0x49, 0x6e, 0xc3, 0x30, 0x0c, 0x45, 0xa5, 0x02, 0xde, 0x74, 0xaf, 0x4b, 0xe4, 0x14, 0x3a, 0x82, 0x16, 0xe2, 0x7d, 0x7a, 0x94, 0x2e, 0x0b, 0x9f, 0xb2, 0x71, 0x06, 0x89, 0xfc, 0x1c, 0xec, 0x04, 0xe9, 0x22, 0x40, 0xb9, 0x53, 0xf2, 0x2c, 0x7e, 0x6a, 0xe0, 0xb7, 0x53, 0x92, 0x71, 0xfa, 0x49, 0x61, 0x7c, 0xae, 0xeb, 0x77, 0x08, 0x9c, 0xd6, 0x35, 0x9e, 0x62, 0x3d, 0x47, 0xf4, 0xff, 0xc7, 0x06, 0x7c, 0xc5, 0x12, 0x62, 0x11, 0xbb, 0xc0, 0x69, 0x03, 0x7e, 0xfe, 0x14, 0x58, 0xd7, 0x9d, 0x3a, 0xff, 0x81, 0xb7, 0x00, 0x72, 0x8b, 0x81, 0x4c, 0x54, 0x43, 0x60, 0x21, 0xea, 0x02, 0x28, 0x24, 0x01, 0x3a, 0x07, 0x07, 0xf2, 0xf5, 0x01, 0x09, 0x54, 0x06, 0x2c, 0xd7, 0x07, 0xb8, 0x04, 0xa2, 0xc6, 0x80, 0xdb, 0x03, 0x5c, 0xc2, 0x65, 0x4e, 0x01, 0x34, 0x06, 0x14, 0x00, 0xf2, 0x6d, 0x2c, 0x24, 0x70, 0xe0, 0x32, 0x23, 0x81, 0x84, 0xed, 0x07, 0x09, 0x54, 0x29, 0x41, 0x03,
  0x4d, 0x4a, 0xe0, 0x40, 0x21, 0x29, 0x62, 0x17, 0x20, 0x1b, 0x20, 0xd0, 0xa8, 0x81, 0x2a, 0x81, 0xee, 0x02, 0x0b, 0xd9, 0xeb, 0x30, 0xca, 0xd8, 0x05, 0xca, 0x18, 0x02, 0xd0, 0x65, 0x11, 0x15, 0xf6, 0x62, 0x96, 0x31, 0x47, 0x62, 0x37, 0x07, 0x30, 0x8b, 0x50, 0x40, 0xe5, 0x40, 0x4b, 0x70, 0xa2, 0x10, 0xa8, 0x70, 0x26, 0x27, 0xb0, 0xcc, 0x7c, 0x08, 0x34, 0x17, 0x48, 0x1c, 0x60, 0x5b, 0xc7, 0xef, 0xc5, 0x5c, 0x88, 0x32, 0xe1, 0x09, 0x2c, 0x8f, 0x00, 0x4c, 0xf0, 0x04, 0x32, 0x5b, 0xa9, 0x83, 0x00, 0xf6, 0x87, 0x07, 0x80, 0xcc, 0xe4, 0x30, 0xa0, 0x8c, 0xc4, 0x07, 0x81, 0x76, 0x0c, 0x60, 0x96, 0xb4, 0xbc, 0x10, 0xe0, 0x37, 0x59, 0x01, 0x0d, 0x00, 0xe6, 0xbc, 0xd9, 0x04, 0x98, 0xb9, 0x3f, 0x09, 0xb0, 0xf7, 0x07, 0x07, 0x98, 0xaf, 0x28, 0x0e, 0x90, 0x46, 0xa7, 0xf6, 0x00, 0x68, 0x6c, 0xef, 0x0e, 0xcc, 0xcd, 0xca, 0x84, 0xbd, 0x3f, 0x38, 0x0f, 0x2f, 0x05, 0xec, 0x63, 0xff, 0x1c, 0x90, 0x08, 0xfc, 0x27, 0x05, 0x77, 0x33, 0x02, 0xaa, 0xb5, 0x90, 0x07, 0x81, 0x62, 0xd5, 0xe9, 0xf4, 0xa8, 0x08, 0xe8, 0x56, 0x95, 0x1d, 0x1b, 0x29, 0x16, 0xd1, 0x8e, 0x01, 0xd9, 0x28, 0x23, 0x3b, 0x76, 0x80, 0x66, 0x5d, 0xd1, 0x50, 0xd0, 0x8b, 0x2b, 0x5a, 0x92, 0xf6, 0x62, 0x30, 0x35, 0xed, 0xc5, 0x60, 0x8b, 0x49, 0x39, 0x29, 0x07, 0x48, 0x65, 0xe8, 0x92, 0x9f, 0x39, 0xee, 0x19, 0x1a, 0x8c, 0x3b, 0xd4, 0x80, 0xee, 0x3f, 0x7e, 0xc0, 0xf1, 0x48, 0x79, 0x9f, 0xe2, 0x3e, 0x01, 0xe1, 0xb2, 0xdc, 0x92, 0x2e, 0x84, 0x29, 0x27, 0xb0, 0x11, 0x73, 0xd0, 0xb1, 0x6c, 0x8c, 0xa6, 0x16, 0x0e, 0xa2, 0xaa, 0x85, 0x81, 0xd0, 0x9b, 0x27, 0xa3, 0xeb, 0xed, 0xf7,
  0x24, 0x38, 0x22, 0xaa, 0xb1, 0xff, 0x9e, 0x04, 0x53, 0x44, 0x37, 0xee, 0x89, 0x2b, 0xc1, 0x14, 0x51, 0xad, 0x7b, 0xe0, 0x4a, 0x30, 0x44, 0x74, 0xb3, 0xb1, 0xfa, 0x19, 0x74, 0x8e, 0x64, 0xf7, 0x45, 0xa7, 0x06, 0x63, 0x8a, 0xaa, 0x81, 0x12, 0x49, 0x54, 0x32, 0xab, 0xf3, 0x11, 0x12, 0x28, 0x90, 0x2a, 0x9c, 0x6f, 0xa4, 0x1c, 0x26, 0xf0, 0x7a, 0x8d, 0x2e, 0x05, 0x0a, 0xf8, 0x05, 0x86, 0xa0, 0xc9, 0x5d, 0xc5, 0x74, 0x28, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
unsigned int offload_power_on_len = 595;

#endif  // USE_OFFLOAD_WEB


/*************************************************\
 *               Variables
\*************************************************/

// offloading stages
enum OffloadStages { OFFLOAD_NONE, OFFLOAD_BEFORE, OFFLOAD_ACTIVE, OFFLOAD_AFTER };

// offload event structure type
struct offload_event {                         // offload event structure
  time_t   start;                              // offload start time
  time_t   stop;                               // offload release time
  uint16_t power;                              // power overload when offload triggered
};

// variables
struct {
  uint8_t   arr_device_type[OFFLOAD_DEVICE_MAX];
  uint8_t   nbr_device_type = 0;
  uint16_t  device_type     = 0;               // device type
  uint16_t  device_priority = 0;               // device priority
  uint16_t  device_power    = 0;               // power of device
  uint16_t  contract_power  = 0;               // maximum power limit before offload
  int       contract_adjust = 0;               // adjustement of maximum power in %
  char      str_topic[64];                     // mqtt topic to be used for meter
  char      str_kinst[8];                      // mqtt instant apparent power key
  char      str_kmax[8];                       // mqtt maximum apparent power key
} offload_config;

struct {
  uint8_t  stage          = OFFLOAD_NONE;      // current offloading state
  uint8_t  relay_state    = 0;                 // relay state before offloading
  uint16_t power_inst     = 0;                 // actual phase instant power (retrieved thru MQTT)
  uint32_t time_message   = 0;                 // time of last power message
  uint32_t time_stage     = UINT32_MAX;        // time of next stage
  uint32_t time_json      = UINT32_MAX;        // time of next JSON update
  uint16_t  delay_before  = 0;                 // delay in seconds before effective offloading
  uint16_t  delay_after   = 0;                 // delay in seconds before removing offload
  bool relay_managed      = true;              // flag to define if relay is managed directly
  bool state_changed      = false;             // flag to signal that offload state has changed
  offload_event event;                         // current offload event
} offload_status;

#ifndef USE_UFILESYS
struct {
  uint8_t index = 0;                           // current event index
  offload_event arr_event[OFFLOAD_EVENT_MAX];  // array of past events
} offload_memory;
#endif  // USE_UFILESYS

/**************************************************\
 *                  Accessors
\**************************************************/

// load config parameters
void OffloadLoadConfig ()
{
  uint16_t device_type;

#ifdef USE_UFILESYS

  // retrieve saved settings from flash filesystem
  device_type = UfsCfgLoadKeyInt (D_OFFLOAD_CFG, D_CMND_OFFLOAD_TYPE);
  offload_config.device_power = UfsCfgLoadKeyInt (D_OFFLOAD_CFG, D_CMND_OFFLOAD_POWER);
  offload_config.contract_power = UfsCfgLoadKeyInt (D_OFFLOAD_CFG, D_CMND_OFFLOAD_CONTRACT);
  offload_config.device_priority = UfsCfgLoadKeyInt (D_OFFLOAD_CFG, D_CMND_OFFLOAD_PRIORITY);
  offload_config.contract_adjust = UfsCfgLoadKeyInt (D_OFFLOAD_CFG, D_CMND_OFFLOAD_ADJUST);

  // mqtt config
  UfsCfgLoadKey (D_OFFLOAD_CFG, D_CMND_OFFLOAD_TOPIC, offload_config.str_topic, sizeof (offload_config.str_topic));
  UfsCfgLoadKey (D_OFFLOAD_CFG, D_CMND_OFFLOAD_KINST, offload_config.str_kinst, sizeof (offload_config.str_kinst));
  UfsCfgLoadKey (D_OFFLOAD_CFG, D_CMND_OFFLOAD_KMAX, offload_config.str_kmax, sizeof (offload_config.str_kmax));

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("OFL: Config from LittleFS"));

#else       // No LittleFS

  // retrieve saved settings from flash memory
  device_type = Settings->knx_GA_addr[0];
  offload_config.device_power = Settings->knx_GA_addr[1];
  offload_config.device_priority = Settings->knx_GA_addr[2];
  offload_config.contract_power = Settings->knx_GA_addr[3];
  offload_config.contract_adjust = (int)Settings->knx_GA_addr[4] - 100;

  // mqtt config
  strlcpy (offload_config.str_topic, SettingsText (SET_OFFLOAD_TOPIC), sizeof (offload_config.str_topic));
  strlcpy (offload_config.str_kinst, SettingsText (SET_OFFLOAD_KEY_INST), sizeof (offload_config.str_kinst));
  strlcpy (offload_config.str_kmax, SettingsText (SET_OFFLOAD_KEY_MAX), sizeof (offload_config.str_kmax));

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("OFL: Config from Settings"));

# endif     // USE_UFILESYS

  // check for out of range values
  if (offload_config.device_power > OFFLOAD_POWER_MAX) offload_config.device_power = 0;
  if (offload_config.contract_power > OFFLOAD_POWER_MAX) offload_config.contract_power = 0;
  if (offload_config.device_priority > OFFLOAD_PRIORITY_MAX) offload_config.device_priority = OFFLOAD_PRIORITY_MAX;
  if (offload_config.contract_adjust > 100) offload_config.contract_adjust = 0;
  if (offload_config.contract_adjust < -99) offload_config.contract_adjust = 0;

  // validate device type
  OffloadValidateDeviceType (device_type);
}

// save config parameters
void OffloadSaveConfig ()
{
#ifdef USE_UFILESYS

  // save settings to flash filesystem
  UfsCfgSaveKeyInt (D_OFFLOAD_CFG, D_CMND_OFFLOAD_TYPE, offload_config.device_type, true);
  UfsCfgSaveKeyInt (D_OFFLOAD_CFG, D_CMND_OFFLOAD_PRIORITY, offload_config.device_priority, false);
  UfsCfgSaveKeyInt (D_OFFLOAD_CFG, D_CMND_OFFLOAD_POWER, offload_config.device_power, false);
  UfsCfgSaveKeyInt (D_OFFLOAD_CFG, D_CMND_OFFLOAD_CONTRACT, offload_config.contract_power, false);
  UfsCfgSaveKeyInt (D_OFFLOAD_CFG, D_CMND_OFFLOAD_ADJUST, offload_config.contract_adjust, false);

  // mqtt config
  UfsCfgSaveKey (D_OFFLOAD_CFG, D_CMND_OFFLOAD_TOPIC, offload_config.str_topic, false);
  UfsCfgSaveKey (D_OFFLOAD_CFG, D_CMND_OFFLOAD_KINST, offload_config.str_kinst, false);
  UfsCfgSaveKey (D_OFFLOAD_CFG, D_CMND_OFFLOAD_KMAX, offload_config.str_kmax, false);

# else      // No LittleFS

  // save settings to flash memory
  Settings->knx_GA_addr[0] = offload_config.device_type;
  Settings->knx_GA_addr[1] = offload_config.device_power;
  Settings->knx_GA_addr[2] = offload_config.device_priority;
  Settings->knx_GA_addr[3] = offload_config.contract_power;
  Settings->knx_GA_addr[4] = (uint16_t)(offload_config.contract_adjust + 100);

  // mqtt config
  SettingsUpdateText (SET_OFFLOAD_TOPIC, offload_config.str_topic);
  SettingsUpdateText (SET_OFFLOAD_KEY_INST, offload_config.str_kinst);
  SettingsUpdateText (SET_OFFLOAD_KEY_MAX, offload_config.str_kmax);

# endif     // USE_UFILESYS
}

/**************************************************\
 *                  Functions
\**************************************************/

// declare device type in the available list
void OffloadResetAvailableType ()
{
  offload_config.nbr_device_type = 0;
}

// declare device type in the available list
void OffloadAddAvailableType (uint8_t device_type)
{
  if ((offload_config.nbr_device_type < OFFLOAD_DEVICE_MAX) && (device_type < OFFLOAD_DEVICE_MAX))
  {
    offload_config.arr_device_type[offload_config.nbr_device_type] = device_type;
    offload_config.nbr_device_type++;
  }
}

// validate device type selection
uint16_t OffloadValidateDeviceType (uint16_t new_type)
{
  bool     is_ok = false;
  uint16_t index;

  // loop to check if device is in the availability list
  for (index = 0; index < offload_config.nbr_device_type; index ++) if (offload_config.arr_device_type[index] == new_type) is_ok = true;
 
  // if device is available, save appliance type and associatd delays
  if (is_ok)
  {
    offload_config.device_type = new_type;
    if (offload_config.device_priority == 0) offload_config.device_priority = arr_offload_priority[new_type];

    // calculate delay before offloading (10 - priority in sec.)
    offload_status.delay_before = OFFLOAD_PRIORITY_MAX - offload_config.device_priority;

    // calculate delay after offloading (priority x 6 sec. + 5 sec. random)
    offload_status.delay_after  = 6 * offload_config.device_priority + random (0, 5);
  }
  else new_type = UINT16_MAX;

  return new_type;
}

// get maximum power limit before offload
uint16_t OffloadGetMaxPower ()
{
  uint32_t power_max = 0;

  // calculate maximum power including extra overload
  if (offload_config.contract_power != UINT32_MAX) power_max = offload_config.contract_power * (100 + offload_config.contract_adjust) / 100;

  return (uint16_t)power_max;
}

// get offload state
bool OffloadIsOffloaded ()
{
  return (offload_status.stage >= OFFLOAD_ACTIVE);
}

// get offload newly set state
bool OffloadJustSet ()
{
  bool result;
  
  // calculate and reset state changed flag
  result = (offload_status.state_changed && (offload_status.event.start != UINT32_MAX));
  offload_status.state_changed = false;

  return result;
}

// get offload newly removed state
bool OffloadJustRemoved ()
{
  bool result;

  // calculate and reset state changed flag
  result  = (offload_status.state_changed && (offload_status.event.start != UINT32_MAX) && (offload_status.event.stop != UINT32_MAX));
  result |= (offload_status.state_changed && (offload_status.event.start == UINT32_MAX));
  offload_status.state_changed = false;

  return result;
}

// set status flags
void OffloadSetManagedMode (bool is_managed) { offload_status.relay_managed = is_managed; }

// generate time string : 12d 03h 22m 05s
void OffloadGenerateTime (char *pstr_time, size_t size, uint32_t local_time)
{
  bool   set_date, set_hour, set_minute;
  TIME_T time_dst;
  char   str_part[8];

  // init
  strcpy (pstr_time, "");

  // generate time structure
  BreakTime (local_time, time_dst);

  // set flags
  set_date = (time_dst.days > 0);
  set_hour = (set_date || (time_dst.hour > 0));
  set_minute = (set_date || set_hour || (time_dst.minute > 0));

  // generate string
  if (set_date) sprintf_P (pstr_time, PSTR ("%ud "), time_dst.days); 
  if (set_hour)
  {
    sprintf_P (str_part, PSTR ("%uh "), time_dst.hour);
    strlcat (pstr_time, str_part, size);
  }
  if (set_minute)
  {
    sprintf_P (str_part, PSTR ("%um "), time_dst.minute);
    strlcat (pstr_time, str_part, size);
  }
  sprintf_P (str_part, PSTR ("%us"), time_dst.second);
  strlcat (pstr_time, str_part, size);
}

// activate offload state
void OffloadActivate ()
{
  // set current event
  offload_status.event.power   = offload_status.power_inst;
  offload_status.event.start   = LocalTime ();
  offload_status.state_changed = true;

  // read relay state and switch off if needed
  if (offload_status.relay_managed)
  {
    // save relay state
    offload_status.relay_state = bitRead (TasmotaGlobal.power, 0);

    // if relay is ON, switch off
    if (offload_status.relay_state == 1) ExecuteCommandPower (1, POWER_OFF, SRC_MAX);
  }

  // get relay state and log
  AddLog (LOG_LEVEL_INFO, PSTR ("OFL: Offload starts, relay was %u"), offload_status.relay_state);
}

// remove offload state
void OffloadRemove ()
{
  char str_line[64];

  // set release time for current event
  offload_status.event.stop    = LocalTime ();
  offload_status.state_changed = true;

#ifdef USE_UFILESYS

  // if new file, generate header
  if (!ffsp->exists (D_OFFLOAD_CSV)) UfsCsvAppend (D_OFFLOAD_CSV, "Start;Stop;Power", false);

  // add data to CSV file
  sprintf_P (str_line, PSTR ("%u;%u;%u"), offload_status.event.start, offload_status.event.stop, offload_status.event.power);
  UfsCsvAppend (D_OFFLOAD_CSV, str_line, false);

# else      // No LittleFS

  // set overload data in array current index
  offload_memory.index ++;
  offload_memory.index = offload_memory.index % OFFLOAD_EVENT_MAX;
  offload_memory.arr_event[offload_memory.index].start = offload_status.event.start;
  offload_memory.arr_event[offload_memory.index].stop  = offload_status.event.stop;
  offload_memory.arr_event[offload_memory.index].power = offload_status.event.power;

# endif     // USE_UFILESYS

  // if relay is managed and it was ON, switch it back
  if (offload_status.relay_managed && (offload_status.relay_state == 1)) ExecuteCommandPower (1, POWER_ON, SRC_MAX);

  // reset current offload data
  offload_status.event.start = UINT32_MAX;
  offload_status.event.stop  = UINT32_MAX;
  offload_status.event.power = 0;

  // log offloading removal
  AddLog (LOG_LEVEL_INFO, PSTR ("OFL: Offload stops, relay is %u"), offload_status.relay_state);
}

// Called just before setting relays
bool OffloadSetDevicePower ()
{
  bool result = false;

  // if relay is managed, 
  if (offload_status.relay_managed)
  {
    // if offload is active and action is not from the module
    if (OffloadIsOffloaded () && (XdrvMailbox.payload != SRC_MAX))
    {
      // save target state of first relay
      offload_status.relay_state = XdrvMailbox.index & 1;

      // log and ignore action
      AddLog (LOG_LEVEL_INFO, PSTR ("OFF: Offload active, relay order %u blocked"), offload_status.relay_state);
      result = true;
    }
  }

#ifdef USE_PILOTWIRE
  else result = PilotwireSetDevicePower ();
#endif // USE_PILOTWIRE

  return result;
}

// Show JSON status (for MQTT)
void OffloadShowJSON (bool is_autonomous)
{
//  char str_buffer[64];

  // add , in append mode
  if (is_autonomous) Response_P (PSTR ("{")); else ResponseAppend_P (PSTR (","));

  // "Offload":{"State":"OFF","Stage":1,"Device":1000}
  ResponseAppend_P (PSTR ("\"%s\":{"), D_JSON_OFFLOAD);
  
  // state and stage
  ResponseAppend_P (PSTR ("\"%s\":%u"), D_JSON_OFFLOAD_STATE, OffloadIsOffloaded ());
  ResponseAppend_P (PSTR (",\"%s\":%d"), D_JSON_OFFLOAD_STAGE, offload_status.stage);
  
  // device power
  ResponseAppend_P (PSTR (",\"%s\":%d"), D_JSON_OFFLOAD_DEVICE, offload_config.device_power);

  ResponseAppend_P (PSTR ("}"));

  // publish it if message is autonomous
  if (is_autonomous)
  {
    // publish message
    ResponseAppend_P (PSTR ("}"));
    MqttPublishTeleSensor ();

    // set next JSON update trigger
    if (offload_status.stage == OFFLOAD_NONE) offload_status.time_json = UINT32_MAX;
    else offload_status.time_json = millis () + (1000 * OFFLOAD_JSON_UPDATE);
  } 
}

// check and update MQTT power subsciption after disconnexion
void OffloadMqttSubscribe ()
{
  // if subsciption topic defined
  if (strlen (offload_config.str_topic) > 0)
  {
    // subscribe to power meter and log
    MqttSubscribe (offload_config.str_topic);
    AddLog (LOG_LEVEL_INFO, PSTR ("OFL: Subscribed to %s"), offload_config.str_topic);
  }
}

// read received MQTT data to retrieve house instant power
bool OffloadMqttData ()
{
  bool  data_handled;
  long  mqtt_inst = 0;
  long  mqtt_pmax = 0;
  char  str_key[64];
  char* pstr_result;

  // if topic is the instant house power
  data_handled = (strcmp (offload_config.str_topic, XdrvMailbox.topic) == 0);
  if (data_handled)
  {
    // log and counter increment
    AddLog (LOG_LEVEL_INFO, PSTR ("OFL: Received %s"), offload_config.str_topic);
    offload_status.time_message = LocalTime ();

    // look for max power key
    sprintf_P (str_key, PSTR ("\"%s\":"), offload_config.str_kmax);
    pstr_result = strstr (XdrvMailbox.data, str_key);
    if (pstr_result != nullptr)
    {
      mqtt_pmax = atol (pstr_result + strlen (str_key));
      if (mqtt_pmax > 0) offload_config.contract_power = (uint16_t)mqtt_pmax;
    }

    // look for instant power key
    sprintf_P (str_key, PSTR ("\"%s\":"), offload_config.str_kinst);
    pstr_result = strstr (XdrvMailbox.data, str_key);
    if (pstr_result != nullptr)
    {
      mqtt_inst = atol (pstr_result + strlen (str_key));
      offload_status.power_inst = (uint16_t)mqtt_inst;
    }

    // log
    AddLog (LOG_LEVEL_DEBUG, PSTR ("OFL: Power is %d/%d"), offload_status.power_inst, offload_config.contract_power);
  }

  return data_handled;
}

// update offloading status according to all parameters
void OffloadEvery250ms ()
{
  uint8_t  prev_stage, next_stage;
  uint16_t power = 0;
  float    apparent_power;

#ifdef USE_ENERGY_SENSOR
  // if instant power has been mesured, read it
  apparent_power = Energy.voltage[0] * Energy.current[0];
  if (apparent_power > 0) power = (uint16_t)apparent_power;
#endif

  // check if device instant power is beyond defined power
  if (power > offload_config.device_power) offload_config.device_power = power;

  // if contract power and device power are defined
  power = OffloadGetMaxPower ();
  if ((power > 0) && (offload_config.device_power > 0))
  {
    // set previous and next state to current state
    prev_stage = offload_status.stage;
    next_stage = offload_status.stage;
  
    // switch according to current state
    switch (offload_status.stage)
    { 
      // actually not offloaded
      case OFFLOAD_NONE:
        // save relay state
        offload_status.relay_state = bitRead (TasmotaGlobal.power, 0);

        // if overload is detected
        if (offload_status.power_inst > power)
        { 
          // set next statge and calculate delay
          next_stage = OFFLOAD_BEFORE;
          offload_status.time_stage = millis () + (1000 * (uint32_t)offload_status.delay_before);
        }
        break;

      // pending offloading
      case OFFLOAD_BEFORE:
        // save relay state
        offload_status.relay_state = bitRead (TasmotaGlobal.power, 0);

        // if house power has gone down, remove pending offloading
        if (offload_status.power_inst <= power)
        {
          next_stage = OFFLOAD_NONE;
          offload_status.time_stage = UINT32_MAX;
        }

        // else if delay is reached, set active offloading
        else if (TimeReached (offload_status.time_stage))
        {
          next_stage = OFFLOAD_ACTIVE;
          offload_status.time_stage = UINT32_MAX;
          OffloadActivate ();
        }
        break;

      // offloading is active
      case OFFLOAD_ACTIVE:
        // calculate maximum power allowed when substracting device power
        if (power > offload_config.device_power) power -= offload_config.device_power;
        else power = 0;

        // if instant power is under this value, prepare to remove offload
        if (offload_status.power_inst <= power)
        {
          next_stage = OFFLOAD_AFTER;
          offload_status.time_stage = millis () + (1000 * (uint32_t)offload_status.delay_after);
        }
        break;

      // actually just after offloading should stop
      case OFFLOAD_AFTER:
        // calculate maximum power allowed when substracting device power
        if (power > offload_config.device_power) power -= offload_config.device_power;
        else power = 0;

        // if house power has gone again too high, offloading back again
        if (offload_status.power_inst > power)
        {
          next_stage = OFFLOAD_ACTIVE;
          offload_status.time_stage = UINT32_MAX;
          OffloadActivate ();
        }
        
        // else if delay is reached, set remove offloading
        else if (TimeReached (offload_status.time_stage))
        {
          next_stage = OFFLOAD_NONE;
          offload_status.time_stage = UINT32_MAX;
          OffloadRemove ();
        } 
        break;
    }

    // update offloading state and send MQTT status if needed
    offload_status.stage = next_stage;
    if (next_stage != prev_stage) OffloadShowJSON (true);
  }
}


// check if MQTT message should be sent
void OffloadEverySecond ()
{
  // if JSON needs to be updated
  if ((offload_status.time_json != UINT32_MAX) && TimeReached (offload_status.time_json)) OffloadShowJSON (true);
}

// offload initialisation
void OffloadInit ()
{
  uint32_t index;

  // init available devices list
  for (index = 0; index < OFFLOAD_DEVICE_MAX; index++) OffloadAddAvailableType (index);
  
  // init current event
  offload_status.event.power = 0;
  offload_status.event.start = UINT32_MAX;
  offload_status.event.stop  = UINT32_MAX;

#ifndef USE_UFILESYS
  // loop to init offload events array
  for (index = 0; index < OFFLOAD_EVENT_MAX; index++)
  {
    offload_memory.arr_event[index].start = UINT32_MAX;
    offload_memory.arr_event[index].stop  = UINT32_MAX;
  }
#endif    // USE_UFILESYS

  // load configuration
  OffloadLoadConfig ();
}

/**************************************************\
 *                  Commands
\**************************************************/

void CmndOffloadPower ()
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload <= OFFLOAD_POWER_MAX))
  {
    offload_config.device_power = (uint16_t)XdrvMailbox.payload;
    OffloadSaveConfig ();
  }
  ResponseCmndNumber (offload_config.device_power);
}

void CmndOffloadType ()
{
  if (XdrvMailbox.data_len > 0)
  {
    OffloadValidateDeviceType ((uint16_t)XdrvMailbox.payload);
    OffloadSaveConfig ();
  }
  ResponseCmndNumber (offload_config.device_type);
}

void CmndOffloadPriority ()
{
  if (XdrvMailbox.data_len > 0)
  {
    offload_config.device_priority = (uint16_t)XdrvMailbox.payload;
    OffloadSaveConfig ();
  }
  ResponseCmndNumber (offload_config.device_priority);
}

void CmndOffloadContract ()
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload <= OFFLOAD_POWER_MAX)) offload_config.contract_power = (uint16_t)XdrvMailbox.payload;
  ResponseCmndNumber (offload_config.contract_power);
}

void CmndOffloadAdjust ()
{
  if ((XdrvMailbox.data_len > 0) && (abs (XdrvMailbox.payload) <= 100))
  {
    offload_config.contract_adjust = XdrvMailbox.payload;
    OffloadSaveConfig ();
  }
  ResponseCmndNumber (offload_config.contract_adjust);
}

void CmndOffloadPowerTopic ()
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.data_len < sizeof (offload_config.str_topic)))
  {
    strncpy (offload_config.str_topic, XdrvMailbox.data, XdrvMailbox.data_len);
    OffloadSaveConfig ();
  }
  ResponseCmndChar (offload_config.str_topic);
}

void CmndOffloadPowerKeyInst ()
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.data_len < sizeof (offload_config.str_kinst)))
  {
    strncpy (offload_config.str_kinst, XdrvMailbox.data, XdrvMailbox.data_len);
    OffloadSaveConfig ();
  }
  ResponseCmndChar (offload_config.str_kinst);
}

void CmndOffloadPowerKeyMax ()
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.data_len < sizeof (offload_config.str_kmax)))
  {
    strncpy (offload_config.str_kmax, XdrvMailbox.data, XdrvMailbox.data_len);
    OffloadSaveConfig ();
  }
  ResponseCmndChar (offload_config.str_kmax);
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

#ifdef USE_OFFLOAD_WEB

// power switch icons
void OffloadWebIconPowerOff () { Webserver->send (200, "image/png", offload_power_off_png, offload_power_off_len); }
void OffloadWebIconPowerOn ()  { Webserver->send (200, "image/png", offload_power_on_png, offload_power_on_len); }
void OffloadWebIconPower () 
{ 
  uint8_t relay_state;

  // read real relay state or saved one if currently offloaded
  relay_state = bitRead (TasmotaGlobal.power, 0);
  if (OffloadIsOffloaded ()) relay_state = offload_status.relay_state;

  if (relay_state == 1) OffloadWebIconPowerOff (); else OffloadWebIconPowerOn ();
}

#endif    // USE_OFFLOAD_WEB

// offload icon
void OffloadWebIconOffload ()  { Webserver->send (200, "image/png", offload_icon_png, offload_icon_len); }

// append offloading buttons to main page
void OffloadWebMainButton ()
{
  // if in managed mode, append control button
  if (offload_status.relay_managed) WSContentSend_P (OFFLOAD_BUTTON, D_PAGE_OFFLOAD_CONTROL, D_OFFLOAD_CONTROL);

  // offload history button
  WSContentSend_P (OFFLOAD_BUTTON, D_PAGE_OFFLOAD_HISTORY, D_OFFLOAD " " D_OFFLOAD_HISTORY);
}

// append offloading state to main page
void OffloadWebSensor ()
{
  uint16_t power;
  uint32_t time_left;
  TIME_T   time_dst;
  char     str_text[40];
  char     str_value[12];

  // display device type
  GetTextIndexed (str_text, sizeof (str_text), offload_config.device_type, kOffloadDevice);
  WSContentSend_PD (PSTR ("{s}%s{m}%u VA{e}"), str_text, offload_config.device_power);

  // if house power is subscribed, display power
  if (strlen (offload_config.str_topic) > 0)
  {
    // calculate delay since last power message
    strcpy_P (str_text, PSTR ("..."));
    if (offload_status.time_message > 0)
    {
      // calculate delay
      time_left = LocalTime() - offload_status.time_message;
      BreakTime (time_left, time_dst);

      // generate readable format
      strcpy (str_text, "");
      if (time_dst.hour > 0)
      {
        sprintf_P (str_value, PSTR ("%dh"), time_dst.hour);
        strlcat (str_text, str_value, sizeof (str_text));
      }
      if (time_dst.hour > 0 || time_dst.minute > 0)
      {
        sprintf_P (str_value, PSTR ("%dm"), time_dst.minute);
        strlcat (str_text, str_value, sizeof (str_text));
      }
      sprintf_P (str_value, PSTR ("%ds"), time_dst.second);
      strlcat (str_text, str_value, sizeof (str_text));
    }

    // display current power and max power limit
    power = OffloadGetMaxPower ();
    if (power > 0) WSContentSend_PD (PSTR ("{s}%s <small><i>[%s]</i></small>{m}<b>%u</b> / %u VA{e}"), D_OFFLOAD_CONTRACT, str_text, offload_status.power_inst, power);

    // switch according to current state
    switch (offload_status.stage)
    { 
      // calculate number of ms left before offloading
      case OFFLOAD_BEFORE:
        time_left = TimeDifference (millis (), offload_status.time_stage) / 1000;
        strcpy_P (str_value, PSTR ("orange"));
        sprintf_P (str_text, PSTR ("Starting in <b>%d sec.</b>") , time_left);
        break;

      // calculate number of ms left before offload removal
      case OFFLOAD_AFTER:
        time_left = TimeDifference (millis (), offload_status.time_stage) / 1000;
        strcpy_P (str_value, PSTR ("red"));
        sprintf_P (str_text, PSTR ("Ending in <b>%d sec.</b>"), time_left);
        break;

      // device is offloaded
      case OFFLOAD_ACTIVE:
        strcpy_P (str_value, PSTR ("red"));
        strcpy_P (str_text, PSTR ("<b>Active</b>"));
        break;

      default:
        strcpy (str_text, "");
        strcpy (str_value, "");
        break;
    }
    
    // display current state
    if (strlen (str_text) > 0) WSContentSend_PD (PSTR ("{s}%s{m}<span style='color:%s;'>%s</span>{e}"), D_OFFLOAD, str_value, str_text);
  }
}

// Offload configuration web page
void OffloadWebPageConfig ()
{
  int      index, value, result, device;
  uint16_t power;
  char     str_argument[64];
  char     str_default[64];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg("save"))
  {
    // set contract power limit according to 'contract' parameter
    WebGetArg (D_CMND_OFFLOAD_TYPE, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) OffloadValidateDeviceType ((uint16_t)atoi (str_argument));

    // set power of heater according to 'power' parameter
    WebGetArg (D_CMND_OFFLOAD_POWER, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0)
    {
      value = atoi (str_argument);
      if (value <= OFFLOAD_POWER_MAX) offload_config.device_power = (uint16_t)value;
    }

    // set contract power limit according to 'contract' parameter
    WebGetArg (D_CMND_OFFLOAD_CONTRACT, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0)
    {
      value = atoi (str_argument);
      if (value <= OFFLOAD_POWER_MAX) offload_config.contract_power = (uint16_t)value;
    }

    // set contract power limit according to 'adjust' parameter
    WebGetArg (D_CMND_OFFLOAD_ADJUST, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0)
    {
      value = atoi (str_argument);
      if (abs (value) < 100) offload_config.contract_adjust = value;
    }

    // set offloading device priority according to 'priority' parameter
    WebGetArg (D_CMND_OFFLOAD_PRIORITY, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0)
    {
      value = atoi (str_argument);
      if (value <= OFFLOAD_PRIORITY_MAX) offload_config.device_priority = (uint16_t)value;
    }

    // set MQTT topic according to 'topic' parameter
    WebGetArg (D_CMND_OFFLOAD_TOPIC, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) strlcpy (offload_config.str_topic, str_argument, sizeof (offload_config.str_topic));

    // set JSON key according to 'inst' parameter
    WebGetArg (D_CMND_OFFLOAD_KINST, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) strlcpy (offload_config.str_kinst, str_argument, sizeof (offload_config.str_kinst));

    // set JSON key according to 'max' parameter
    WebGetArg (D_CMND_OFFLOAD_KMAX, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) strlcpy (offload_config.str_kmax, str_argument, sizeof (offload_config.str_kmax));

    // save configuration
    OffloadSaveConfig ();
  }

  // beginning of form
  WSContentStart_P (D_OFFLOAD_CONFIGURE);
  WSContentSendStyle ();

  // specific style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("p.half {display:inline-block;width:47%%;margin-right:2%%;}\n"));
  WSContentSend_P (PSTR ("p.third {display:inline-block;width:30%%;margin-right:2%%;}\n"));
  WSContentSend_P (PSTR ("span.key {float:right;font-size:0.7rem;}\n"));
  WSContentSend_P (PSTR ("input.switch {background:#aaa;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_PAGE_OFFLOAD_CONFIG);

  // --------------
  //     Device  
  // --------------

  WSContentSend_P (OFFLOAD_FIELDSET_START, D_OFFLOAD_DEVICE);

  // appliance type
  WSContentSend_P (PSTR ("<p>%s<span class='key'>%s</span>\n"), D_OFFLOAD_TYPE, D_CMND_OFFLOAD_TYPE);
  WSContentSend_P (PSTR ("<select name='%s' id='%s'>\n"), D_CMND_OFFLOAD_TYPE, D_CMND_OFFLOAD_TYPE);
  for (index = 0; index < offload_config.nbr_device_type; index ++)
  {
    device = offload_config.arr_device_type[index];
    GetTextIndexed (str_default, sizeof (str_default), device, kOffloadDevice);
    if (device == offload_config.device_type) strcpy_P (str_argument, PSTR (D_OFFLOAD_SELECTED)); else strcpy (str_argument, "");
    WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>\n"), device, str_argument, str_default);
  }
  WSContentSend_P (PSTR ("</select>\n"));
  WSContentSend_P (PSTR ("</p>\n"));

  // device power
  sprintf_P (str_argument, PSTR ("%s (%s)"), D_OFFLOAD_POWER, D_OFFLOAD_UNIT_VA);
  WSContentSend_P (OFFLOAD_INPUT_NUMBER, str_argument, D_CMND_OFFLOAD_POWER, PSTR ("none"), D_CMND_OFFLOAD_POWER, D_CMND_OFFLOAD_POWER, 0, 65000, 1, offload_config.device_power);

  // priority
  sprintf_P (str_argument, PSTR ("%s (%u high ... %u low)"), D_OFFLOAD_PRIORITY, 1, OFFLOAD_PRIORITY_MAX);
  WSContentSend_P (OFFLOAD_INPUT_NUMBER, str_argument, D_CMND_OFFLOAD_PRIORITY, PSTR ("switch"), D_CMND_OFFLOAD_PRIORITY, D_CMND_OFFLOAD_PRIORITY, 1, OFFLOAD_PRIORITY_MAX, 1, offload_config.device_priority);

  WSContentSend_P (OFFLOAD_FIELDSET_STOP);

  // --------------
  //    Contract  
  // --------------

  WSContentSend_P (OFFLOAD_FIELDSET_START, D_OFFLOAD_CONTRACT);

  // contract power
  sprintf_P (str_argument, PSTR ("%s (%s)"), D_OFFLOAD_CONTRACT, D_OFFLOAD_UNIT_VA);
  WSContentSend_P (OFFLOAD_INPUT_NUMBER, str_argument, D_CMND_OFFLOAD_CONTRACT, PSTR ("none"), D_CMND_OFFLOAD_CONTRACT, D_CMND_OFFLOAD_CONTRACT, 0, 65000, 1, offload_config.contract_power);

  // contract adjustment
  sprintf_P (str_argument, PSTR ("%s (%s)"), D_OFFLOAD_ADJUST, D_OFFLOAD_UNIT_PERCENT);
  WSContentSend_P (OFFLOAD_INPUT_NUMBER, str_argument, D_CMND_OFFLOAD_ADJUST, PSTR ("none"), D_CMND_OFFLOAD_ADJUST, D_CMND_OFFLOAD_ADJUST, -99, 100, 1, offload_config.contract_adjust);

  WSContentSend_P (OFFLOAD_FIELDSET_STOP);

  // ------------
  //    Meter  
  // ------------

  WSContentSend_P (OFFLOAD_FIELDSET_START, D_OFFLOAD_METER);

  // instant power mqtt topic
  WSContentSend_P (OFFLOAD_INPUT_TEXT, D_OFFLOAD_TOPIC, D_CMND_OFFLOAD_TOPIC, D_CMND_OFFLOAD_TOPIC, offload_config.str_topic, "");

  // instant power json key
  WSContentSend_P (OFFLOAD_INPUT_TEXT, D_OFFLOAD_KEY_INST, D_CMND_OFFLOAD_KINST, D_CMND_OFFLOAD_KINST, offload_config.str_kinst, "");

  // max power json key
  WSContentSend_P (OFFLOAD_INPUT_TEXT, D_OFFLOAD_KEY_MAX, D_CMND_OFFLOAD_KMAX, D_CMND_OFFLOAD_KMAX, offload_config.str_kmax, "");

  WSContentSend_P (OFFLOAD_FIELDSET_STOP);
  WSContentSend_P (PSTR ("<br>\n"));

  // ------------
  //    Script  
  // ------------

  WSContentSend_P (PSTR ("<script type='text/javascript'>\n"));

  WSContentSend_P (PSTR ("var arr_priority = [%d"), arr_offload_priority[0]);
  for (index = 1; index < OFFLOAD_DEVICE_MAX; index ++) WSContentSend_P (PSTR (",%d"), arr_offload_priority[index]);
  WSContentSend_P (PSTR ("];\n"));

  WSContentSend_P (PSTR ("var device_type = document.getElementById ('type');\n"));
  WSContentSend_P (PSTR ("var device_priority = document.getElementById ('priority');\n"));
  WSContentSend_P (PSTR ("device_type.onchange = function () {\n"));
  WSContentSend_P (PSTR ("device_priority.value = arr_priority[this.value];\n"));
  WSContentSend_P (PSTR ("}\n"));

  WSContentSend_P (PSTR ("</script>\n"));

  // save button  
  // -----------
  WSContentSend_P (PSTR ("<p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), D_SAVE);
  WSContentSend_P (PSTR ("</form>\n"));

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

uint8_t OffloadWebDisplayEvent (uint8_t prev_month, struct offload_event *ptr_event)
{
  uint8_t curr_month = prev_month;
  TIME_T  start_dst, stop_dst;
  char    str_time[16];
  char    str_text[16];

  // generate time structure
  BreakTime (ptr_event->start, start_dst);
  if (ptr_event->stop != UINT32_MAX) BreakTime (ptr_event->stop, stop_dst);

  // if month has changed, display month
  strcpy (str_text, "");
  if (start_dst.month != prev_month)
  { 
    // display month separator
    start_dst.year += 1970;
    WSContentSend_P (PSTR ("<div class='month'>%s %u</div>\n"), start_dst.name_of_month, start_dst.year);
    curr_month = start_dst.month;
  }
  else strcpy_P (str_text, PSTR ("inter"));

  // event section
  WSContentSend_P (PSTR ("<div class='event %s'>\n"), str_text);

  // offload power
  WSContentSend_P (PSTR ("<div class='power'>%u</div>\n"), ptr_event->power);

  // detail of event
  WSContentSend_P (PSTR ("<div class='detail'>"));

  // start day
  sprintf_P (str_time, PSTR ("%02u/%02u"), start_dst.day_of_month, start_dst.month);
  WSContentSend_P (PSTR ("<div class='date'>%s</div>"), str_time);

  // start time
  sprintf_P (str_time, PSTR ("%02u:%02u"), start_dst.hour, start_dst.minute);
  WSContentSend_P (PSTR ("<div class='begin'>%s</div>"), str_time);

  // if stop time is defined
  if (ptr_event->stop != UINT32_MAX) 
  {
    // if start and stop days are different, display stop day
    if (start_dst.day_of_month != stop_dst.day_of_month)
    {
      sprintf_P (str_time, PSTR ("%02u/%02u"), stop_dst.day_of_month, stop_dst.month);
      WSContentSend_P (PSTR ("<div class='date'>%s</div>"), str_time);
    }

    // stop time
    sprintf_P (str_time, PSTR ("%02u:%02u"), stop_dst.hour, stop_dst.minute);
    WSContentSend_P (PSTR ("<div class='end'>%s</div>"), str_time);

    // duration
    OffloadGenerateTime (str_text, sizeof (str_text), ptr_event->stop - ptr_event->start);
    WSContentSend_P (PSTR ("<div class='duration'>%s</div>"), str_text);
  }

  // else, event still running
  else WSContentSend_P (PSTR ("<div class='end'>...</div>"));

  // end of detail of event
  WSContentSend_P (PSTR ("</div>\n"));

  // end of event
  WSContentSend_P (PSTR ("</div>\n"));

  return curr_month;
}

// Offload history page
void OffloadWebPageHistory ()
{
  int      result;
  int      counter = 0;
  uint8_t  index, index_array;
  uint8_t  curr_month = UINT8_MAX;
  offload_event curr_event;

  // beginning of page without authentification
  WSContentStart_P (D_OFFLOAD " " D_OFFLOAD_HISTORY, false);
  WSContentSend_P (PSTR ("</script>\n"));

  // page style
  WSContentSend_P (PSTR ("<style>\n"));

  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {width:100%;margin:0.25rem auto;padding:0.1rem 0px;text-align:center;vertical-align:middle;}\n"));
  WSContentSend_P (PSTR ("div.title {font-size:2rem;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.header {font-size:2rem;color:yellow;}\n"));
  WSContentSend_P (PSTR ("div.main {margin:1rem auto;padding:0px;text-align:center;vertical-align:middle;}\n"));

  WSContentSend_P (PSTR ("div.list {margin:2%% auto;padding-bottom:10px;max-width:600px;background:#333;border-radius:12px;}\n"));
  WSContentSend_P (PSTR ("div.month {display:block;font-size:1rem;font-style:italic;text-align:left;padding-left:1rem;padding-bottom:0.1rem;margin-top:0.5rem;color:#999;border-bottom:1px #666 solid;}\n"));
  WSContentSend_P (PSTR ("div.event {display:block;text-align:left;margin:5px 20px;}\n"));
  WSContentSend_P (PSTR ("div.inter {padding-top:5px;border-top:1px #666 dashed;}\n"));
  WSContentSend_P (PSTR ("div.power {display:inline-block;width:35%%;font-size:1.25rem;font-weight:bold;text-align:center;vertical-align:top;color:yellow;margin-left:25px;}\n"));
  WSContentSend_P (PSTR ("div.power::after {font-size:0.9rem;margin-left:0.25rem;content:'%s';}\n"), D_OFFLOAD_UNIT_VA);
  WSContentSend_P (PSTR ("div.detail {display:inline-block;width:auto;}\n"));
  WSContentSend_P (PSTR ("div.detail div {display:inline-block;font-size:0.9rem;margin:1px;padding:0px 2px;}\n"));
  WSContentSend_P (PSTR ("div.detail div.date {font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.begin::after {padding-left:6px;content:'-'}\n"));
  WSContentSend_P (PSTR ("div.detail div.duration {display:block;font-size:0.9rem;font-style:italic;text-align:left;color:#aaa;}\n"));

  WSContentSend_P (PSTR ("</style>\n"));

  // page body
  WSContentSend_P (PSTR ("</head>\n"));
  WSContentSend_P (PSTR ("<body>\n"));

  WSContentSend_P (PSTR ("<div class='main'>\n"));

  // device name, icon and title
  WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText(SET_DEVICENAME));
  WSContentSend_P (PSTR ("<div class='header'>Offload Events</div>\n"));
  WSContentSend_P (PSTR ("<div><img height=64 src='%s'></div>\n"), OFFLOAD_ICON_ACTIVE);

  WSContentSend_P (PSTR ("<div class='list'>\n"));

  // if there is a current offload event
  if (offload_status.event.start != UINT32_MAX)
  {
    // read event data
    curr_event.start = offload_status.event.start;
    curr_event.stop  = offload_status.event.stop;
    curr_event.power = offload_status.event.power;

    // display history record
    curr_month = OffloadWebDisplayEvent (curr_month, &curr_event);
    counter ++;
  }
  
#ifdef USE_UFILESYS

  // read last line
  if (UfsCsvOpen (D_OFFLOAD_CSV, true))
  {
    // go to end of file
    result = UfsCsvSeekToEnd ();

    // loop thru lines
    while (result > 0)
    {
      // read event data
      curr_event.start = strtoul (ufs_csv.pstr_value[0], nullptr, 10);
      curr_event.stop  = strtoul (ufs_csv.pstr_value[1], nullptr, 10);
      curr_event.power = strtoul (ufs_csv.pstr_value[2], nullptr, 10);

      // display history record
      curr_month = OffloadWebDisplayEvent (curr_month, &curr_event);
      counter++;

      // read next line
      result = UfsCsvPreviousLine ();
    } 

    // close file
    UfsCsvClose (UFS_CSV_ACCESS_READ);
  }

#else       // No LittleFS

  // loop thru offload events array
  for (index = OFFLOAD_EVENT_MAX; index > 0; index--)
  {
    // get target power array position and add value if defined
    index_array = (index + offload_memory.index) % OFFLOAD_EVENT_MAX;
    if (offload_memory.arr_event[index_array].start != UINT32_MAX)
    {
      // read event data
      curr_event.start = offload_memory.arr_event[index_array].start;
      curr_event.stop  = offload_memory.arr_event[index_array].stop;
      curr_event.power = offload_memory.arr_event[index_array].power;

      // display history record
      curr_month = OffloadWebDisplayEvent (curr_month, &curr_event);
      counter++;
    }
  }

#endif  // USE_UFILESYS

  // if no event
  if (counter == 0) WSContentSend_P (PSTR ("<div class='month'>No offload history available</div>\n"));

  // end of section
  WSContentSend_P (PSTR ("</div>\n"));      // list
  WSContentStop ();
}

#ifdef USE_OFFLOAD_WEB

// Offloading public configuration page
void OffloadWebPageControl ()
{
  bool updated = false;

  // if switch has been switched OFF
  if (Webserver->hasArg(MQTT_CMND_TOGGLE))
  {
    // status updated
    updated = true;

    // if device is actually offloaded, set next state
    if (OffloadIsOffloaded ()) { if ( offload_status.relay_state == 0) offload_status.relay_state = 1; else offload_status.relay_state = 0; }

    // else set current state
    else
    {
      if (bitRead (TasmotaGlobal.power, 0) == 1) ExecuteCommandPower (1, POWER_OFF, SRC_MAX);
        else ExecuteCommandPower (1, POWER_ON, SRC_MAX);
    }
  }

  // beginning of form without authentification
  WSContentStart_P (D_OFFLOAD_CONTROL, false);
  WSContentSend_P (PSTR ("</script>\n"));

  // if parameters have been updated, auto reload page
  if (updated)
  {
    WSContentSend_P (PSTR ("<meta http-equiv='refresh' content='0;URL=/%s' />\n"), D_PAGE_OFFLOAD_CONTROL);
    WSContentSend_P (PSTR ("</head>\n"));
    WSContentSend_P (PSTR ("<body bgcolor='#252525'></body>\n"));
    WSContentSend_P (PSTR ("</html>\n"));
    WSContentEnd ();
  }

  // else, display control page
  else
  {
    // check for update every second
    WSContentSend_P (PSTR ("<script type='text/javascript'>\n"));
    WSContentSend_P (PSTR ("function updTempo() {\n"));
    WSContentSend_P (PSTR (" upd=new XMLHttpRequest();\n"));
    WSContentSend_P (PSTR (" upd.onreadystatechange=function(){\n"));
    WSContentSend_P (PSTR (" if (upd.responseText == 1) {\n"));
    WSContentSend_P (PSTR ("  now=new Date();\n"));
    WSContentSend_P (PSTR ("  plugImg=document.getElementById('plug');\n"));
    WSContentSend_P (PSTR ("  plugURL=plugImg.getAttribute('src');\n"));
    WSContentSend_P (PSTR ("  plugURL=plugURL.substring(0,plugURL.indexOf('ts=')) + 'ts=' + now.getTime();\n"));
    WSContentSend_P (PSTR ("  plugImg.setAttribute('src', plugURL);\n"));
    WSContentSend_P (PSTR ("  powerImg=document.getElementById('power');\n"));
    WSContentSend_P (PSTR ("  powerURL=powerImg.getAttribute('src');\n"));
    WSContentSend_P (PSTR ("  powerURL=powerURL.substring(0,powerURL.indexOf('ts=')) + 'ts=' + now.getTime();\n"));
    WSContentSend_P (PSTR ("  powerImg.setAttribute('src', powerURL);\n"));
    WSContentSend_P (PSTR ("  }\n"));
    WSContentSend_P (PSTR (" }\n"));
    WSContentSend_P (PSTR (" upd.open('GET','offload.upd',true);\n"));
    WSContentSend_P (PSTR (" upd.send();\n"));
    WSContentSend_P (PSTR ("}\n"));
    WSContentSend_P (PSTR ("setInterval(function() {updTempo();},%d);\n"), 1000);
    WSContentSend_P (PSTR ("</script>\n"));

    // page style
    WSContentSend_P (PSTR ("<style>\n"));
    WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));
    WSContentSend_P (PSTR ("div {width:100%%;margin:auto;padding:3px 0px;text-align:center;vertical-align:middle;}\n"));
    WSContentSend_P (PSTR ("button.power {background:none;padding:8px;margin:16px;border-radius:32px;border:1px solid #666;}\n"));
    WSContentSend_P (PSTR (".title {margin:20px auto;font-size:5vh;}\n"));
    WSContentSend_P (PSTR ("img.plug {height:128px;margin:30px auto;}\n"));
    WSContentSend_P (PSTR ("img.power {height:48px;}\n"));
    WSContentSend_P (PSTR ("</style>\n"));
    WSContentSend_P (PSTR ("</head>\n"));

    // page body
    WSContentSend_P (PSTR ("<body>\n"));
    WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_PAGE_OFFLOAD_CONTROL);

    // device name
    WSContentSend_P (PSTR ("<div class='title bold'>%s</div>\n"), SettingsText(SET_DEVICENAME));

    // information about current offloading
    WSContentSend_P (PSTR ("<div class='icon'><img class='plug' id='plug' src='%s?ts=0'></div>\n"), OFFLOAD_ICON_POWER);

    // display switch button
    WSContentSend_P (PSTR ("<div><button class='power' type='submit' name='%s' title='%s'><img class='power' id='power' src='%s?ts=0' /></button>"), MQTT_CMND_TOGGLE,  D_OFFLOAD_TOGGLE, OFFLOAD_ICON_POWER);

    // end of page
    WSContentSend_P (PSTR ("</form>\n"));
    WSContentStop ();
  }
}

#endif  // USE_OFFLOAD_WEB

#endif  // USE_WEBSERVER

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xdrv96 (uint8_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
   case FUNC_SET_DEVICE_POWER:
      result = OffloadSetDevicePower ();
      break;
    case FUNC_INIT:
      OffloadInit ();
      break;
    case FUNC_MQTT_SUBSCRIBE:
      OffloadMqttSubscribe ();
      break;
    case FUNC_MQTT_DATA:
      result = OffloadMqttData ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kOffloadCommands, OffloadCommand);
      break;
    case FUNC_EVERY_250_MSECOND:
      OffloadEvery250ms ();
      break;
    case FUNC_EVERY_SECOND:
      OffloadEverySecond ();
      break;
    case FUNC_JSON_APPEND:
      OffloadShowJSON (false);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      //pages
      Webserver->on ("/" D_PAGE_OFFLOAD_CONFIG, OffloadWebPageConfig);
      Webserver->on ("/" D_PAGE_OFFLOAD_HISTORY, OffloadWebPageHistory);

      // offload icon
      Webserver->on ("/" OFFLOAD_ICON_ACTIVE, OffloadWebIconOffload);

#ifdef USE_OFFLOAD_WEB
      // icons
      Webserver->on ("/" OFFLOAD_ICON_OFF, OffloadWebIconPowerOff);
      Webserver->on ("/" OFFLOAD_ICON_ON, OffloadWebIconPowerOn);
      Webserver->on ("/" OFFLOAD_ICON_POWER, OffloadWebIconPower);

      // if relay is managed, relay switch page
      if (offload_status.relay_managed) Webserver->on ("/" D_PAGE_OFFLOAD_CONTROL, OffloadWebPageControl);
#endif  // USE_OFFLOAD_WEB
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      OffloadWebMainButton ();
      break;
    case FUNC_WEB_ADD_BUTTON:
      WSContentSend_P (OFFLOAD_BUTTON, D_PAGE_OFFLOAD_CONFIG, D_OFFLOAD_CONFIGURE " " D_OFFLOAD);
      break;
    case FUNC_WEB_SENSOR:
      OffloadWebSensor ();
      break;
#endif  // USE_WEBSERVER

  }
  
  return result;
}

#endif // USE_OFFLOAD
