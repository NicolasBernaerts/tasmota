/*
  xdrv_96_offloading.ino - Device offloading thru MQTT instant and max power
  
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
                   
  Settings are stored using unused KNX parameters :
    - Settings.knx_GA_addr[0]   = Power of plugged appliance (W) 
    - Settings.knx_GA_addr[1]   = Maximum power of contract (W) 
    - Settings.knx_GA_addr[2]   = Delay in seconds before effective offload
    - Settings.knx_GA_addr[3]   = Delay in seconds before removal of offload
    - Settings.knx_GA_addr[4]   = Phase number (1...3)
    - Settings.knx_GA_addr[5]   = Acceptable % of overload
    - Settings.knx_GA_addr[6]   = Delay randomisation in seconds
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
 *                Offloading
\*************************************************/

#ifdef USE_OFFLOADING

#define XDRV_96                 96
#define XSNS_96                 96

#define OFFLOAD_PHASE_MAX       3
#define OFFLOAD_EVENT_MAX       10

#define OFFLOAD_MESSAGE_LEFT    5
#define OFFLOAD_MESSAGE_DELAY   5

#define D_PAGE_OFFLOAD_CONFIG   "offload"
#define D_PAGE_OFFLOAD_CONTROL  "control"
#define D_PAGE_OFFLOAD_HISTORY  "histo"
#define D_PAGE_OFFLOAD_JSON     "histo.json"


#define D_CMND_OFFLOAD_DEVICE     "device"
#define D_CMND_OFFLOAD_TYPE       "type"
#define D_CMND_OFFLOAD_BEFORE     "before"
#define D_CMND_OFFLOAD_AFTER      "after"
#define D_CMND_OFFLOAD_RANDOM     "random"
#define D_CMND_OFFLOAD_CONTRACT   "contract"
#define D_CMND_OFFLOAD_ADJUST     "adjust"
#define D_CMND_OFFLOAD_PHASE      "phase"
#define D_CMND_OFFLOAD_TOPIC      "topic"
#define D_CMND_OFFLOAD_KEY_INST   "kinst"
#define D_CMND_OFFLOAD_KEY_MAX    "kmax"
#define D_CMND_OFFLOAD_OFF        "off"
#define D_CMND_OFFLOAD_ON         "on"
#define D_CMND_OFFLOAD_TOGGLE     "toggle"

#define D_JSON_OFFLOAD          "Offload"
#define D_JSON_OFFLOAD_STATE    "State"
#define D_JSON_OFFLOAD_STAGE    "Stage"
#define D_JSON_OFFLOAD_PHASE    "Phase"
#define D_JSON_OFFLOAD_ADJUST   "Adjust"
#define D_JSON_OFFLOAD_BEFORE   "Before"
#define D_JSON_OFFLOAD_AFTER    "After"
#define D_JSON_OFFLOAD_CONTRACT "Contract"
#define D_JSON_OFFLOAD_DEVICE   "Device"
#define D_JSON_OFFLOAD_MAX      "Max"
#define D_JSON_OFFLOAD_TOPIC    "Topic"
#define D_JSON_OFFLOAD_KEY_INST "KeyInst"
#define D_JSON_OFFLOAD_KEY_MAX  "KeyMax"

#define MQTT_OFFLOAD_TOPIC      "compteur/tele/SENSOR"
#define MQTT_OFFLOAD_KEY_INST   "P"
#define MQTT_OFFLOAD_KEY_MAX    "PREF"

#define D_OFFLOAD               "Offload"
#define D_OFFLOAD_EVENT         "Events"
#define D_OFFLOAD_CONFIGURE     "Configure"
#define D_OFFLOAD_CONTROL       "Control"
#define D_OFFLOAD_HISTORY       "History"
#define D_OFFLOAD_DEVICE        "Device"
#define D_OFFLOAD_DELAY         "Delay"
#define D_OFFLOAD_TYPE          "Type"
#define D_OFFLOAD_INSTCONTRACT  "Act/Max"
#define D_OFFLOAD_PHASE         "Phase"
#define D_OFFLOAD_ADJUST        "Adjustment"
#define D_OFFLOAD_CONTRACT      "Contract"
#define D_OFFLOAD_METER         "Meter"
#define D_OFFLOAD_TOPIC         "MQTT Topic"
#define D_OFFLOAD_KEY_INST      "Power JSON Key"
#define D_OFFLOAD_KEY_MAX       "Contract JSON Key"
#define D_OFFLOAD_ACTIVE        "Offload active"
#define D_OFFLOAD_POWER         "Power"
#define D_OFFLOAD_PRIORITY      "Priority to"
#define D_OFFLOAD_BEFORE        "Off delay"
#define D_OFFLOAD_AFTER         "On delay"
#define D_OFFLOAD_RANDOM        "Random"
#define D_OFFLOAD_TOGGLE        "Toggle plug switch"

#define D_OFFLOAD_UNIT_VA       "VA"
#define D_OFFLOAD_UNIT_W        "W"
#define D_OFFLOAD_UNIT_SEC      "sec."
#define D_OFFLOAD_UNIT_PERCENT  "%"
#define D_OFFLOAD_SELECTED      "selected"

// list of MQTT extra parameters
enum OffloadParameters { PARAM_OFFLOAD_TOPIC, PARAM_OFFLOAD_KEY_INST, PARAM_OFFLOAD_KEY_MAX };

// offloading commands
enum OffloadCommands { CMND_OFFLOAD_DEVICE, CMND_OFFLOAD_TYPE, CMND_OFFLOAD_CONTRACT, CMND_OFFLOAD_ADJUST, CMND_OFFLOAD_TOPIC, CMND_OFFLOAD_KEY_INST, CMND_OFFLOAD_KEY_MAX };
const char kOffloadCommands[] PROGMEM = D_CMND_OFFLOAD_DEVICE "|" D_CMND_OFFLOAD_TYPE "|" D_CMND_OFFLOAD_CONTRACT "|" D_CMND_OFFLOAD_ADJUST "|" D_CMND_OFFLOAD_TOPIC "|" D_CMND_OFFLOAD_KEY_INST "|" D_CMND_OFFLOAD_KEY_MAX;

// strings
const char OFFLOAD_FIELDSET_START[] PROGMEM = "<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>\n";
const char OFFLOAD_FIELDSET_STOP[]  PROGMEM = "</fieldset></p>\n";
const char OFFLOAD_INPUT_NUMBER[]   PROGMEM = "<p class='%s'>%s<span class='key'>%s</span><br><input class='%s' type='number' name='%s' id='%s' min='%d' max='%d' step='%d' value='%d'></p>\n";
const char OFFLOAD_INPUT_TEXT[]     PROGMEM = "<p>%s<span class='key'>%s</span><br><input name='%s' value='%s' placeholder='%s'></p>\n";
const char OFFLOAD_BUTTON[]         PROGMEM = "<p><form action='%s' method='get'><button>%s</button></form></p>";

// definition of types of device with associated delays
enum OffloadDevices { OFFLOAD_DEVICE_APPLIANCE, OFFLOAD_DEVICE_FRIDGE, OFFLOAD_DEVICE_WASHING, OFFLOAD_DEVICE_DISH, OFFLOAD_DEVICE_DRIER, OFFLOAD_DEVICE_CUMULUS, OFFLOAD_DEVICE_IRON, OFFLOAD_DEVICE_BATHROOM, OFFLOAD_DEVICE_OFFICE, OFFLOAD_DEVICE_LIVING, OFFLOAD_DEVICE_ROOM, OFFLOAD_DEVICE_KITCHEN, OFFLOAD_DEVICE_MAX };
const char offload_device_appliance[] PROGMEM = "Misc appliance";
const char offload_device_fridge[]    PROGMEM = "Fridge";
const char offload_device_washing[]   PROGMEM = "Washing machine";
const char offload_device_dish[]      PROGMEM = "Dish washer";
const char offload_device_drier[]     PROGMEM = "Drier";
const char offload_device_cumulus[]   PROGMEM = "Cumulus";
const char offload_device_iron[]      PROGMEM = "Iron";
const char offload_device_bathroom[]  PROGMEM = "Bathroom heater";
const char offload_device_office[]    PROGMEM = "Office heater";
const char offload_device_living[]    PROGMEM = "Living room heater";
const char offload_device_room[]      PROGMEM = "Sleeping room heater";
const char offload_device_kitchen[]   PROGMEM = "Kitchen heater";
const char* const arr_offload_device_label[] PROGMEM = { offload_device_appliance, offload_device_fridge, offload_device_washing, offload_device_dish, offload_device_drier, offload_device_cumulus, offload_device_iron, offload_device_bathroom, offload_device_office, offload_device_living, offload_device_room, offload_device_kitchen };
const uint8_t arr_offload_delay_before[] = { 0,  0, 8,  4,  4,  0,  4,  0,  0,  0,  0,   0   };
const uint8_t arr_offload_delay_after[]  = { 15, 5, 20, 30, 25, 35, 10, 50, 60, 90, 120, 150 };
const uint8_t arr_offload_delay_random[] = { 5,  5, 5 , 5,  5,  5,  5,  10, 30, 30, 30,  30  };

/****************************************\
 *               Icons
 * 
 *      xxd -i -c 256 icon.png
\****************************************/

// icon : offload
unsigned char offload_icon_png[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x01, 0x03, 0x00, 0x00, 0x00, 0xf9, 0xf0, 0xf3, 0x88, 0x00, 0x00, 0x00, 0x06, 0x50, 0x4c, 0x54, 0x45, 0xcc, 0xdb, 0x55, 0xff, 0x99, 0x00, 0xdf, 0xa0, 0xd3, 0x66, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x00, 0x01, 0x62, 0x4b, 0x47, 0x44, 0x00, 0x88, 0x05, 0x1d, 0x48, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0d, 0xd7, 0x00, 0x00, 0x0d, 0xd7, 0x01, 0x42, 0x28, 0x9b, 0x78, 0x00, 0x00, 0x00, 0x07, 0x74, 0x49, 0x4d, 0x45, 0x07, 0xe4, 0x0a, 0x18, 0x0d, 0x27, 0x2e, 0x90, 0xd7, 0xb8, 0x1c, 0x00, 0x00, 0x01, 0xcf, 0x49, 0x44, 0x41, 0x54, 0x48, 0xc7, 0x95, 0x96, 0x41, 0x72, 0x83, 0x30, 0x0c, 0x45, 0xe5, 0xba, 0x33, 0xec, 0xea, 0x23, 0x70, 0x93, 0xd2, 0x93, 0x95, 0x1c, 0x8d, 0xa3, 0xf8, 0x08, 0x74, 0xba, 0xf1, 0x82, 0xb1, 0x2a, 0x63, 0x8c, 0xf5, 0xc5, 0x24, 0x4c, 0xb3, 0x49, 0xf2, 0x06, 0xa2, 0x67, 0x49, 0x48, 0x21, 0xfa, 0xcf, 0xeb, 0xcb, 0x82, 0xc5, 0x7c, 0x77, 0x17, 0xf0, 0x30, 0xc0, 0x5b, 0x30, 0x5c, 0x80, 0x0d, 0x12, 0x2c, 0x18, 0x6f, 0xc1, 0xa7, 0x05, 0xdf, 0x16, 0xe4, 0x67, 0xa0, 0x09, 0xba, 0xad, 0xf9, 0x1c, 0x67, 0xf2, 0x0d, 0x84, 0x68, 0xc0, 0xb8, 0x1e, 0x57, 0xa6, 0x06, 0x8e, 0x0f, 0xa1, 0x81, 0xb0, 0x19, 0x30, 0x64, 0xbc, 0x92, 0x3c, 0xd7, 0xf7, 0x69, 0x6d, 0x99, 0xe3, 0x2a, 0x32, 0x37, 0x40, 0xbc, 0x18, 0x30, 0x57, 0x11, 0x3e, 0x41, 0xbd, 0xd9, 0x71, 0x24, 0x10, 0xf1, 0x1d, 0x54, 0x11, 0x05, 0xaa,
  0xc8, 0xd0, 0x41, 0x15, 0x19, 0xf8, 0xac, 0x64, 0x15, 0x09, 0x1d, 0x54, 0x91, 0x51, 0x81, 0x5d, 0x64, 0xe2, 0x5e, 0xc9, 0x5d, 0x44, 0x83, 0x5d, 0x64, 0x56, 0x60, 0x17, 0x61, 0x05, 0x8a, 0x88, 0x63, 0x56, 0x9d, 0xc2, 0x06, 0x14, 0x11, 0xaf, 0x41, 0x11, 0x41, 0x20, 0x22, 0x81, 0x75, 0x69, 0x45, 0x04, 0x81, 0x88, 0x8c, 0x00, 0x44, 0x64, 0xe4, 0x4d, 0xf7, 0x5f, 0x16, 0x73, 0x0d, 0x44, 0x04, 0x81, 0x88, 0xe0, 0x2d, 0x22, 0x12, 0x38, 0x11, 0x88, 0x0c, 0x08, 0xa6, 0xd5, 0x23, 0x18, 0x93, 0x43, 0x20, 0x22, 0xbd, 0x92, 0x87, 0xc8, 0x0c, 0xa0, 0x88, 0x00, 0x28, 0x22, 0x00, 0x8a, 0x48, 0x24, 0x2b, 0xc2, 0xaa, 0x54, 0x12, 0xc3, 0x1b, 0xb0, 0x49, 0x96, 0x55, 0x21, 0x9c, 0x1c, 0x8d, 0xb1, 0x10, 0x22, 0xa2, 0x73, 0x26, 0xbf, 0x88, 0x19, 0x11, 0x80, 0x19, 0x09, 0x6c, 0x32, 0x22, 0x40, 0x44, 0x94, 0xeb, 0xc4, 0x9c, 0x54, 0x1b, 0x56, 0xe0, 0xb4, 0xd7, 0xcc, 0x45, 0x04, 0x81, 0x88, 0xa8, 0x71, 0xc1, 0xc5, 0x7b, 0x22, 0x03, 0x70, 0x18, 0x48, 0xdf, 0x7e, 0x18, 0xb0, 0xd2, 0x1d, 0x48, 0x66, 0xbc, 0x60, 0xb1, 0x0b, 0xe0, 0x7b, 0x80, 0x73, 0x4d, 0x8e, 0x17, 0x6f, 0x01, 0x8a, 0xc8, 0x79, 0x93, 0x05, 0xf9, 0x35, 0x60, 0xfe, 0x65, 0x03, 0x7e, 0x50, 0x44, 0xa2, 0x62, 0x5c, 0xa9, 0xd4, 0x1c, 0xa1, 0xa7, 0x24, 0xa7, 0x2b, 0x02, 0x1a, 0xb1, 0xfd, 0xf3, 0x39, 0xd0, 0x4e, 0x30, 0x64, 0x68, 0xd4, 0xed, 0x1c, 0x68, 0x07, 0x48, 0x04, 0xc5, 0xdd, 0x7b, 0x01, 0x41, 0x3c, 0x07, 0x5a, 0x07, 0x53, 0xd2, 0x60, 0x51, 0xf3, 0xb2, 0x8d, 0xa4, 0x00, 0x5d, 0xf7, 0x20, 0x10, 0x09, 0xbc, 0xc7, 0x56, 0x53, 0x27, 0xab, 0xc9, 0xba, 0xd7, 0x69, 0x53, 0x93, 0xb5,
  0x03, 0xf5, 0xdc, 0xd5, 0x88, 0x2a, 0x23, 0xf5, 0x89, 0x53, 0x22, 0x53, 0xd4, 0x23, 0xbe, 0x80, 0xa5, 0xfa, 0xf6, 0x3a, 0x2d, 0x7a, 0xc4, 0x17, 0xf0, 0x20, 0x14, 0x99, 0x8f, 0x62, 0x44, 0xb3, 0xb0, 0xba, 0x48, 0x36, 0xfb, 0x81, 0x36, 0xb3, 0x41, 0xc8, 0x2e, 0x1d, 0x6a, 0x6b, 0xe9, 0x5c, 0x29, 0xc7, 0x07, 0xdf, 0xa2, 0xb8, 0x68, 0xd6, 0xf6, 0x65, 0x7f, 0x5f, 0xd6, 0xb5, 0xa7, 0x3b, 0xf0, 0x66, 0xc1, 0xfb, 0x8b, 0x7f, 0x14, 0x7f, 0x6e, 0xc8, 0x4e, 0x9e, 0xa5, 0x4d, 0x6a, 0x22, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
unsigned int offload_icon_len = 604;

// icon : plug off
unsigned char offload_plug_off_png[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x04, 0x03, 0x00, 0x00, 0x00, 0x31, 0x10, 0x7c, 0xf8, 0x00, 0x00, 0x00, 0x12, 0x50, 0x4c, 0x54, 0x45, 0x00, 0x6f, 0x6c, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x3c, 0x01, 0x00, 0x81, 0x00, 0x00, 0xc7, 0x00, 0x00, 0x4f, 0x02, 0x88, 0x16, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x00, 0x01, 0x62, 0x4b, 0x47, 0x44, 0x00, 0x88, 0x05, 0x1d, 0x48, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0d, 0xd7, 0x00, 0x00, 0x0d, 0xd7, 0x01, 0x42, 0x28, 0x9b, 0x78, 0x00, 0x00, 0x00, 0x07, 0x74, 0x49, 0x4d, 0x45, 0x07, 0xe4, 0x0a, 0x17, 0x0a, 0x05, 0x16, 0x42, 0x4b, 0x40, 0x71, 0x00, 0x00, 0x01, 0x49, 0x49, 0x44, 0x41, 0x54, 0x68, 0xde, 0xed, 0x98, 0xdd, 0x0d, 0xc2, 0x30, 0x0c, 0x84, 0xab, 0x4e, 0x12, 0x39, 0xea, 0x02, 0x88, 0x2c, 0x90, 0x19, 0xea, 0xfd, 0x57, 0x01, 0x2a, 0xa7, 0x85, 0x96, 0x42, 0xcf, 0x87, 0x04, 0x12, 0xbe, 0x27, 0x5e, 0xee, 0xe3, 0x92, 0xe6, 0xc7, 0x4e, 0xd7, 0x85, 0x42, 0xa1, 0x50, 0x28, 0x14, 0x42, 0xd5, 0xcb, 0xac, 0xe4, 0xb0, 0xcb, 0xbd, 0x12, 0xf3, 0xef, 0x2e, 0xc0, 0xca, 0x0f, 0x03, 0xd6, 0x7e, 0x14, 0xb0, 0xf1, 0xa3, 0x00, 0x21, 0x01, 0x42, 0x02, 0x7a, 0x16, 0x20, 0x24, 0xa0, 0x67, 0x01, 0xf2, 0x4e, 0xc9, 0x11, 0x00, 0x21, 0xc8, 0x01, 0x91, 0x01, 0x5e, 0x47, 0xa0, 0x01, 0x42, 0x02, 0x0e, 0x05, 0x78, 0x35, 0x09, 0x34, 0x40, 0xd8, 0x21, 0xb0, 0x80, 0x79, 0x04, 0xe7, 0xf6, 0xa3, 0xfa, 0xa6, 0x20, 0xab,
  0x19, 0x87, 0xd1, 0x37, 0x82, 0xa2, 0x66, 0x54, 0xad, 0x9e, 0x55, 0x90, 0xd5, 0x8c, 0x83, 0x1a, 0x29, 0x61, 0x3b, 0xb1, 0xa8, 0x19, 0xb5, 0x91, 0x12, 0x74, 0x14, 0xdc, 0x02, 0x4c, 0xc6, 0x41, 0x1b, 0x09, 0x03, 0x4c, 0xbe, 0x9b, 0x51, 0x1b, 0xc9, 0x95, 0x40, 0xeb, 0x0c, 0x42, 0x01, 0xd3, 0x1c, 0x5c, 0x9d, 0x4b, 0x80, 0x1d, 0xc0, 0xee, 0x4a, 0xb3, 0x08, 0x4b, 0x00, 0x14, 0x60, 0x11, 0x96, 0x00, 0x30, 0x20, 0xaf, 0x02, 0xc0, 0x80, 0x25, 0x42, 0x75, 0x02, 0xf2, 0x63, 0x00, 0x1c, 0xd0, 0x22, 0xd4, 0xaf, 0x01, 0xe8, 0x21, 0xb0, 0x93, 0x48, 0x7f, 0x46, 0x76, 0x21, 0x1d, 0x5d, 0xca, 0xbb, 0x9b, 0x69, 0x38, 0xb8, 0x99, 0x76, 0x01, 0xe6, 0x2c, 0x4a, 0x1e, 0x28, 0x36, 0x92, 0x93, 0xff, 0x48, 0x2b, 0x68, 0x82, 0xf5, 0xa1, 0x9a, 0xd1, 0x39, 0xd8, 0x1c, 0xeb, 0xc5, 0x79, 0xac, 0xcf, 0x17, 0x4b, 0x7e, 0x76, 0xb1, 0x48, 0x07, 0x5c, 0x6d, 0x65, 0x74, 0xd6, 0x17, 0xed, 0x72, 0xcd, 0xe0, 0xe5, 0xfa, 0xfd, 0xfa, 0xe0, 0x07, 0x6a, 0x24, 0xba, 0xcc, 0xfb, 0x81, 0x4a, 0x95, 0x2e, 0xb6, 0xf9, 0x72, 0x9f, 0x6e, 0x38, 0xf8, 0x96, 0x87, 0x6e, 0xba, 0xf8, 0xb6, 0x8f, 0x6e, 0x3c, 0xf9, 0xd6, 0x97, 0x6f, 0xbe, 0xe9, 0xf6, 0x9f, 0x7f, 0x80, 0xe0, 0x9f, 0x40, 0xf8, 0x47, 0x98, 0x0f, 0x3c, 0x03, 0x85, 0x42, 0xa1, 0x50, 0x28, 0xf4, 0x5f, 0xba, 0x00, 0x95, 0xd5, 0x57, 0x4f, 0xe9, 0xcb, 0xe8, 0x91, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
unsigned int offload_plug_off_len = 482;

// icon : plug on
unsigned char offload_plug_on_png[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x04, 0x03, 0x00, 0x00, 0x00, 0x31, 0x10, 0x7c, 0xf8, 0x00, 0x00, 0x00, 0x0f, 0x50, 0x4c, 0x54, 0x45, 0xc4, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x8c, 0x3a, 0x00, 0x9c, 0x41, 0x19, 0xa3, 0x5d, 0x00, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x00, 0x01, 0x62, 0x4b, 0x47, 0x44, 0x00, 0x88, 0x05, 0x1d, 0x48, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0d, 0xd7, 0x00, 0x00, 0x0d, 0xd7, 0x01, 0x42, 0x28, 0x9b, 0x78, 0x00, 0x00, 0x00, 0x07, 0x74, 0x49, 0x4d, 0x45, 0x07, 0xe4, 0x0a, 0x17, 0x0a, 0x05, 0x2a, 0x6d, 0x24, 0x3c, 0xf6, 0x00, 0x00, 0x00, 0xf5, 0x49, 0x44, 0x41, 0x54, 0x68, 0xde, 0xed, 0x98, 0x6d, 0x0a, 0xc2, 0x30, 0x0c, 0x40, 0x47, 0x4f, 0x52, 0x62, 0x4f, 0x20, 0x9e, 0xa0, 0xe6, 0xfe, 0x67, 0x52, 0x41, 0xdd, 0xa8, 0x30, 0x9b, 0xbc, 0x6d, 0x15, 0xcc, 0xfb, 0x9f, 0xb7, 0xac, 0x1f, 0x21, 0xe9, 0x34, 0x05, 0x41, 0x10, 0x04, 0x41, 0x60, 0x25, 0xc9, 0x9b, 0xec, 0x08, 0x97, 0x25, 0x99, 0x7c, 0xdd, 0x25, 0x68, 0xe2, 0xcd, 0x82, 0x36, 0xde, 0x2a, 0xf8, 0x88, 0xb7, 0x0a, 0x04, 0x0a, 0x04, 0x0a, 0x12, 0x15, 0x08, 0x14, 0x24, 0x2a, 0x90, 0x6f, 0x64, 0x47, 0x02, 0x16, 0x83, 0x74, 0x00, 0x13, 0x58, 0x4f, 0x01, 0x0b, 0x04, 0x0a, 0xba, 0x12, 0x58, 0x5b, 0x04, 0x2c, 0x10, 0xfa, 0x0b, 0x54, 0x70, 0xd0, 0x12, 0xe4, 0xfd, 0x36, 0x71, 0x29, 0x28, 0xaa, 0xd5, 0x7e, 0x1b, 0xe7, 0x88, 0x8b, 0xde, 0xb9, 0x5a,
  0x05, 0x69, 0xf9, 0xfd, 0x07, 0xd5, 0x2d, 0xd0, 0x27, 0x5e, 0x41, 0x79, 0x09, 0xaa, 0x53, 0xa0, 0xda, 0xa4, 0x90, 0x6d, 0x9b, 0x76, 0x9a, 0x05, 0x67, 0x97, 0xa0, 0xcc, 0x82, 0x3a, 0x46, 0xa0, 0xda, 0x2e, 0x42, 0x08, 0x8e, 0x17, 0x8c, 0x3f, 0x07, 0xbd, 0x47, 0x19, 0x5f, 0xa6, 0x1d, 0xaf, 0x33, 0x2e, 0x28, 0xde, 0x92, 0x86, 0x8b, 0xea, 0x96, 0x65, 0xdd, 0xd9, 0xe1, 0xa4, 0xe1, 0xfd, 0xc5, 0xf8, 0xfe, 0xe0, 0x07, 0x7a, 0x24, 0xde, 0x21, 0x8c, 0xef, 0x54, 0x71, 0xb3, 0xcd, 0xdb, 0x7d, 0x3c, 0x70, 0xf0, 0x91, 0x07, 0x0f, 0x5d, 0x7c, 0xec, 0xc3, 0x83, 0x27, 0x1f, 0x7d, 0xf9, 0xf0, 0x8d, 0xc7, 0x7f, 0xfe, 0x00, 0xc1, 0x9f, 0x40, 0xf8, 0x23, 0xcc, 0x06, 0xcf, 0x40, 0x41, 0x10, 0x04, 0x41, 0xf0, 0x5f, 0xdc, 0x00, 0x61, 0xa7, 0x40, 0xba, 0xce, 0x2b, 0xc4, 0x4b, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
unsigned int offload_plug_on_len = 395;

// icon : plug offloaded
unsigned char offload_plug_offload_png[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x04, 0x03, 0x00, 0x00, 0x00, 0x31, 0x10, 0x7c, 0xf8, 0x00, 0x00, 0x00, 0x0f, 0x50, 0x4c, 0x54, 0x45, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x61, 0x36, 0x00, 0xa7, 0x61, 0x00, 0xf6, 0x93, 0x00, 0x54, 0xb6, 0x38, 0x41, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x00, 0x01, 0x62, 0x4b, 0x47, 0x44, 0x00, 0x88, 0x05, 0x1d, 0x48, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0d, 0xd7, 0x00, 0x00, 0x0d, 0xd7, 0x01, 0x42, 0x28, 0x9b, 0x78, 0x00, 0x00, 0x00, 0x07, 0x74, 0x49, 0x4d, 0x45, 0x07, 0xe4, 0x0a, 0x17, 0x0a, 0x09, 0x13, 0x9e, 0x94, 0xfb, 0xf2, 0x00, 0x00, 0x01, 0x8b, 0x49, 0x44, 0x41, 0x54, 0x68, 0xde, 0xed, 0x98, 0x5d, 0x92, 0x83, 0x20, 0x10, 0x84, 0xe3, 0x0d, 0x44, 0x72, 0x01, 0xb3, 0x7b, 0x01, 0x22, 0x17, 0x88, 0x72, 0xff, 0x33, 0x2d, 0x28, 0x88, 0x65, 0x2a, 0x9a, 0x9e, 0xce, 0xc3, 0x6e, 0xed, 0xf4, 0x43, 0xf2, 0x22, 0x1f, 0x33, 0xcd, 0x4f, 0xcd, 0x70, 0xb9, 0xa8, 0x54, 0x2a, 0x95, 0x4a, 0xa5, 0x42, 0xd5, 0x98, 0x55, 0xad, 0x60, 0xb8, 0xd9, 0xaa, 0x65, 0x66, 0x17, 0x01, 0x76, 0xe3, 0x61, 0xc0, 0x7e, 0x3c, 0x0a, 0x78, 0x1a, 0x8f, 0x02, 0x0c, 0x09, 0x30, 0x24, 0xa0, 0x61, 0x01, 0x86, 0x04, 0x34, 0x2c, 0xc0, 0x9c, 0xa9, 0x15, 0x04, 0x80, 0x10, 0xcc, 0x1b, 0x22, 0x03, 0x38, 0x0e, 0x81, 0x06, 0x18, 0x12, 0xf0, 0x56, 0x00, 0x47, 0x26, 0xd0, 0x00, 0xc3, 0xa6, 0xb0, 0xff, 0xd4, 0x82, 0x80, 0xa7, 0x0c, 0x06, 0xd2,
  0x02, 0xfb, 0x20, 0x33, 0x08, 0x0f, 0x7c, 0x17, 0xf8, 0xb1, 0x7e, 0x7a, 0x0d, 0x3d, 0x78, 0x1a, 0x13, 0x20, 0xb8, 0x32, 0xbe, 0x0b, 0x01, 0x3c, 0xce, 0xcd, 0x32, 0xaa, 0x4c, 0xfb, 0x1d, 0x26, 0x01, 0xc0, 0xd8, 0x32, 0x2c, 0xa2, 0x46, 0x09, 0x20, 0x4e, 0x3c, 0xe6, 0xff, 0x9a, 0x0d, 0x04, 0xc8, 0x36, 0xc4, 0x00, 0xd6, 0x64, 0x5e, 0x00, 0x5e, 0xed, 0xd4, 0xc5, 0x06, 0x1f, 0xaa, 0x87, 0x20, 0x60, 0xb6, 0x21, 0x05, 0x30, 0x49, 0x01, 0xb3, 0x0d, 0x7e, 0xe3, 0x21, 0x0c, 0x48, 0x36, 0x74, 0x1b, 0x0f, 0x71, 0x40, 0xb2, 0x21, 0x26, 0x32, 0x0c, 0x83, 0x93, 0x01, 0x6c, 0x32, 0x20, 0xae, 0x63, 0x59, 0x08, 0x14, 0xd0, 0xf9, 0xd9, 0xc2, 0xf8, 0x7b, 0x97, 0xa5, 0x60, 0xd3, 0xdc, 0xe1, 0xde, 0xad, 0xeb, 0x00, 0x03, 0x62, 0xf2, 0x71, 0x7a, 0x67, 0x9d, 0xd4, 0xc4, 0x74, 0x17, 0x30, 0xfb, 0x20, 0xef, 0xe4, 0xb3, 0xc3, 0x74, 0x08, 0xb8, 0xce, 0x36, 0x38, 0xd1, 0x61, 0x2a, 0x67, 0x31, 0xdc, 0xd6, 0xe3, 0x24, 0x00, 0x24, 0x0f, 0xeb, 0xdd, 0x80, 0x03, 0xba, 0xc5, 0xc2, 0x72, 0x29, 0xe1, 0x00, 0x9b, 0xf3, 0xf7, 0xcb, 0x56, 0x6a, 0xe1, 0xf2, 0xe6, 0x9a, 0xa7, 0xce, 0x47, 0x0a, 0x07, 0xf8, 0x62, 0xdf, 0x62, 0x03, 0x0e, 0xa8, 0x5b, 0x60, 0x73, 0x37, 0x9f, 0x57, 0x38, 0x4d, 0xf5, 0xb0, 0xaf, 0xc1, 0x4c, 0x82, 0xfa, 0xc2, 0xba, 0xcd, 0x82, 0xf4, 0x40, 0x91, 0x55, 0x3e, 0xf9, 0xfa, 0x54, 0x7d, 0xf0, 0x07, 0x6b, 0x24, 0xba, 0xcc, 0xfb, 0x05, 0x95, 0x2a, 0x5d, 0x6c, 0xf3, 0xe5, 0x3e, 0xdd, 0x70, 0xf0, 0x2d, 0x0f, 0xdd, 0x74, 0xf1, 0x6d, 0x1f, 0xdd, 0x78, 0xf2, 0xad, 0x2f, 0xdf, 0x7c, 0xd3, 0xed, 0x3f, 0xff, 0x00, 0xc1, 0x3f, 0x81, 0xf0, 0x8f,
  0x30, 0x1f, 0x78, 0x06, 0x52, 0xa9, 0x54, 0x2a, 0x95, 0xea, 0x7f, 0xe9, 0x07, 0xa3, 0x01, 0xae, 0x6f, 0x59, 0x74, 0x2c, 0xe8, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
unsigned int offload_plug_offload_len = 545;

// icon : power switch off
unsigned char offload_power_off_png[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x02, 0x03, 0x00, 0x00, 0x00, 0xbe, 0x50, 0x89, 0x58, 0x00, 0x00, 0x00, 0x09, 0x50, 0x4c, 0x54, 0x45, 0x00, 0x00, 0x00, 0xc8, 0x00, 0x00, 0x99, 0x99, 0x99, 0xa5, 0x4a, 0xec, 0xd5, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x00, 0x01, 0x62, 0x4b, 0x47, 0x44, 0x00, 0x88, 0x05, 0x1d, 0x48, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x03, 0xbb, 0x00, 0x00, 0x03, 0xbb, 0x01, 0xae, 0xf7, 0x26, 0xa5, 0x00, 0x00, 0x00, 0x07, 0x74, 0x49, 0x4d, 0x45, 0x07, 0xe4, 0x0a, 0x16, 0x04, 0x1d, 0x09, 0xff, 0x7a, 0x9f, 0xb2, 0x00, 0x00, 0x01, 0xee, 0x49, 0x44, 0x41, 0x54, 0x58, 0xc3, 0xed, 0x97, 0x4d, 0x6e, 0xc4, 0x20, 0x0c, 0x85, 0x33, 0x48, 0xb3, 0x61, 0xdf, 0x4b, 0x70, 0x8a, 0x39, 0xc2, 0x2c, 0xca, 0x7d, 0x38, 0x0a, 0x4b, 0xe4, 0x53, 0x76, 0x32, 0x93, 0xe0, 0xe7, 0x1f, 0x28, 0x52, 0x2b, 0xb5, 0x95, 0xca, 0x2a, 0x38, 0x1f, 0xf8, 0x19, 0x88, 0x4d, 0xb6, 0x4d, 0xb4, 0x4b, 0xce, 0xb7, 0x6d, 0xd6, 0xae, 0x39, 0xdf, 0xa7, 0xc0, 0x5b, 0xce, 0xef, 0x53, 0x20, 0x3f, 0xda, 0xec, 0xfd, 0x65, 0x07, 0x6e, 0x5f, 0x01, 0xae, 0x3b, 0x70, 0xff, 0x07, 0x7e, 0x3b, 0x90, 0x0a, 0x02, 0xa1, 0xe9, 0xf7, 0x91, 0x08, 0x01, 0xa2, 0xaa, 0x00, 0x22, 0x2a, 0x0c, 0x04, 0x7a, 0xf2, 0xd0, 0x76, 0x4b, 0x65, 0x20, 0xbe, 0x78, 0x54, 0xf0, 0xb0, 0x34, 0x06, 0x8e, 0xae, 0xf4, 0xa0, 0x01, 0xd2, 0x1e, 0x76, 0xcb, 0x09, 0x3c, 0xbb, 0x45, 0xc6, 0x60, 0x81, 0xaa,
  0x24, 0xec, 0x43, 0x0e, 0xe0, 0x35, 0x61, 0x53, 0x12, 0x0c, 0x40, 0x5a, 0x82, 0x01, 0x8a, 0x92, 0x60, 0x80, 0xba, 0x0e, 0x24, 0xf2, 0xa2, 0x00, 0x95, 0xe4, 0x03, 0xa4, 0x34, 0x5a, 0xa0, 0x48, 0x40, 0x2f, 0x35, 0x03, 0xb1, 0x6b, 0xc2, 0xdd, 0x04, 0x95, 0xcc, 0xe3, 0x79, 0x00, 0x95, 0x3d, 0x08, 0x3c, 0x51, 0x08, 0x70, 0xaf, 0x03, 0x49, 0x84, 0xc1, 0x0e, 0x3b, 0x10, 0x11, 0x80, 0x95, 0xe7, 0x53, 0x8d, 0x61, 0x00, 0xcd, 0xdf, 0x05, 0x86, 0x11, 0x59, 0x10, 0x03, 0xc9, 0x00, 0x55, 0x02, 0x60, 0xec, 0xc7, 0x49, 0x00, 0x78, 0xa8, 0x12, 0x0b, 0x86, 0x6f, 0x13, 0x00, 0x1a, 0x03, 0xa4, 0x59, 0x00, 0xd2, 0x3a, 0x10, 0x40, 0x2f, 0x00, 0xb1, 0x4b, 0x0f, 0xb0, 0x66, 0x00, 0x84, 0x75, 0x20, 0xc2, 0xb6, 0x60, 0x0a, 0xea, 0x9e, 0xbf, 0x09, 0x68, 0x16, 0x48, 0xcb, 0x40, 0x9a, 0x01, 0x0d, 0x49, 0x05, 0xc4, 0x1f, 0x02, 0xb0, 0xf2, 0x4a, 0xa0, 0x58, 0x20, 0x78, 0x00, 0x56, 0x7f, 0x1f, 0x80, 0xfb, 0x83, 0x0f, 0xc0, 0x0d, 0xa4, 0x03, 0x32, 0xe3, 0xf1, 0xfd, 0xe2, 0x4c, 0xcf, 0x36, 0x2f, 0x6b, 0x20, 0xcd, 0x80, 0xb6, 0x0c, 0x54, 0x0b, 0xc4, 0xbf, 0x07, 0x34, 0xa7, 0x56, 0x2f, 0x03, 0x71, 0x06, 0xd4, 0x65, 0x80, 0x2c, 0x40, 0xcb, 0x40, 0xf0, 0xf7, 0xdb, 0x4f, 0x20, 0x13, 0xa0, 0xba, 0xeb, 0xb4, 0x06, 0x6c, 0xe4, 0xc6, 0xe9, 0xe7, 0xc9, 0x09, 0x40, 0x6e, 0x94, 0xa4, 0xb3, 0xbd, 0x0e, 0xa2, 0xad, 0x01, 0xd1, 0x0b, 0x23, 0xfa, 0x25, 0x49, 0x6b, 0xac, 0x58, 0xf5, 0x3c, 0x8d, 0x45, 0x97, 0x45, 0x7b, 0x69, 0xd9, 0x74, 0x09, 0x94, 0xf5, 0xde, 0x94, 0x66, 0x7b, 0x27, 0x71, 0x7a, 0xce, 0x98, 0x64, 0x7c, 0x44, 0x09, 0x44, 0xe3, 0x43, 0x0d, 0x09,
  0xea, 0xca, 0x62, 0x0c, 0x67, 0xbf, 0xa9, 0x09, 0x78, 0x04, 0x49, 0xc3, 0x39, 0x80, 0xb4, 0xcb, 0xd3, 0x42, 0x7a, 0xc6, 0x53, 0xe5, 0x61, 0xea, 0x78, 0xb5, 0xc0, 0xc3, 0x4b, 0xe0, 0xe7, 0xaa, 0x57, 0x5e, 0xb7, 0xa2, 0x17, 0x4e, 0x37, 0xbb, 0xf4, 0xb2, 0x35, 0xbb, 0x79, 0xb2, 0x55, 0xbb, 0xfd, 0x43, 0x09, 0xbe, 0x08, 0xe7, 0x08, 0x0e, 0x25, 0xb8, 0x3e, 0x8a, 0x77, 0x48, 0x87, 0x1e, 0x1c, 0x1f, 0xcd, 0xcd, 0x17, 0x63, 0x0f, 0x66, 0x8a, 0xe6, 0x67, 0x5e, 0x7f, 0x95, 0x5c, 0x99, 0xdb, 0x36, 0x9f, 0xa2, 0x7a, 0x7f, 0x5a, 0x9f, 0x4c, 0x20, 0x02, 0x29, 0x83, 0x9f, 0xb5, 0x71, 0x08, 0x92, 0x68, 0xe3, 0x9f, 0x41, 0xa7, 0x76, 0x7c, 0x00, 0x5f, 0xb2, 0xf7, 0x06, 0x2b, 0x96, 0x6a, 0x81, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
unsigned int offload_power_off_len = 638;

// icon : power switch on
unsigned char offload_power_on_png[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x02, 0x03, 0x00, 0x00, 0x00, 0xbe, 0x50, 0x89, 0x58, 0x00, 0x00, 0x00, 0x09, 0x50, 0x4c, 0x54, 0x45, 0x00, 0x00, 0x00, 0x99, 0x99, 0x99, 0x1c, 0xff, 0x00, 0x23, 0x4c, 0xe2, 0x62, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x00, 0x01, 0x62, 0x4b, 0x47, 0x44, 0x00, 0x88, 0x05, 0x1d, 0x48, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x03, 0xbb, 0x00, 0x00, 0x03, 0xbb, 0x01, 0xae, 0xf7, 0x26, 0xa5, 0x00, 0x00, 0x00, 0x07, 0x74, 0x49, 0x4d, 0x45, 0x07, 0xe4, 0x0a, 0x16, 0x04, 0x14, 0x17, 0xd4, 0xb7, 0x19, 0x98, 0x00, 0x00, 0x01, 0xee, 0x49, 0x44, 0x41, 0x54, 0x58, 0xc3, 0xed, 0x97, 0x4d, 0x6e, 0xc4, 0x20, 0x0c, 0x85, 0x61, 0x24, 0x36, 0xb3, 0xe7, 0x12, 0x3d, 0x05, 0x47, 0x60, 0x81, 0xef, 0x33, 0x47, 0x99, 0x65, 0x95, 0x53, 0x36, 0x99, 0x99, 0xe0, 0xe7, 0x1f, 0x28, 0x52, 0x2b, 0xb5, 0x95, 0xca, 0x2a, 0x38, 0x1f, 0xf8, 0x19, 0x88, 0x4d, 0x42, 0x10, 0xed, 0xb2, 0x6d, 0xb7, 0x30, 0x6b, 0xd7, 0x6d, 0xbb, 0x4f, 0x81, 0xb7, 0x6d, 0x7b, 0x9f, 0x02, 0xdb, 0xde, 0x66, 0xef, 0x2f, 0x07, 0x70, 0xfb, 0x0a, 0x70, 0x3d, 0x80, 0xfb, 0x3f, 0xf0, 0xdb, 0x81, 0x5c, 0x10, 0x88, 0x4d, 0xbf, 0x4f, 0x44, 0x08, 0x10, 0x55, 0x05, 0x10, 0x51, 0x61, 0x20, 0xd2, 0x83, 0x87, 0x76, 0x58, 0x2a, 0x03, 0xe9, 0xc9, 0xa3, 0x82, 0xdd, 0xd2, 0x18, 0x78, 0x75, 0xa5, 0x07, 0x0d, 0x90, 0xf6, 0x70, 0x58, 0x4e, 0xe0, 0xd1, 0x2d, 0x32, 0x06, 0x0b, 0x54,
  0x25, 0xe1, 0x18, 0xf2, 0x02, 0x9e, 0x13, 0x36, 0x25, 0xc1, 0x00, 0xa4, 0x25, 0x18, 0xa0, 0x28, 0x09, 0x06, 0xa8, 0xeb, 0x40, 0x26, 0x2f, 0x0a, 0x50, 0x49, 0x3e, 0x40, 0x4a, 0xa3, 0x05, 0x8a, 0x04, 0xf4, 0x52, 0x33, 0x90, 0xba, 0x26, 0xdc, 0x4d, 0x50, 0xc9, 0x3c, 0x9e, 0x07, 0x50, 0xd9, 0x83, 0xc0, 0x13, 0x85, 0x00, 0xf7, 0x3a, 0x90, 0x45, 0x18, 0xec, 0xb0, 0x03, 0x09, 0x01, 0x58, 0x79, 0x3e, 0xd5, 0x18, 0x06, 0xd0, 0xfc, 0x5d, 0x60, 0x18, 0x89, 0x05, 0x31, 0x90, 0x0d, 0x50, 0x25, 0x00, 0xc6, 0x7e, 0x9c, 0x04, 0x80, 0x87, 0x2a, 0xb3, 0x60, 0xf8, 0x36, 0x01, 0xa0, 0x31, 0x40, 0x9a, 0x05, 0x20, 0xaf, 0x03, 0x11, 0xf4, 0x02, 0x90, 0xba, 0xf4, 0x08, 0x6b, 0x06, 0x40, 0x5c, 0x07, 0x12, 0x6c, 0x0b, 0xa6, 0xa0, 0xee, 0xf9, 0x9b, 0x80, 0x66, 0x81, 0xbc, 0x0c, 0xe4, 0x19, 0xd0, 0x90, 0x54, 0x40, 0xfa, 0x21, 0x00, 0x2b, 0xaf, 0x04, 0x8a, 0x05, 0xa2, 0x07, 0x60, 0xf5, 0xf7, 0x01, 0xb8, 0x3f, 0xf8, 0x00, 0xdc, 0x40, 0x3a, 0x20, 0x33, 0x1e, 0xdf, 0x2f, 0xce, 0xf4, 0x6c, 0xf3, 0xb2, 0x06, 0xf2, 0x0c, 0x68, 0xcb, 0x40, 0xb5, 0x40, 0xfa, 0x7b, 0x40, 0x73, 0x6a, 0xf5, 0x32, 0x90, 0x66, 0x40, 0x5d, 0x06, 0xc8, 0x02, 0xb4, 0x0c, 0x44, 0x7f, 0xbf, 0xfd, 0x04, 0x32, 0x01, 0xaa, 0xbb, 0x4e, 0x6b, 0x40, 0x20, 0x37, 0x4e, 0x3f, 0x4f, 0x4e, 0x00, 0x72, 0xa3, 0x24, 0x9d, 0xed, 0x75, 0x10, 0x6d, 0x0d, 0x48, 0x5e, 0x18, 0xc9, 0x2f, 0x49, 0x5a, 0x63, 0xc5, 0xaa, 0xe7, 0x69, 0x2c, 0xba, 0x2c, 0xda, 0x4b, 0x4b, 0xd0, 0x25, 0x50, 0xd6, 0x7b, 0x53, 0x9a, 0xed, 0x9d, 0xc4, 0xe9, 0x39, 0x63, 0xb2, 0xf1, 0x91, 0x24, 0x90, 0x8c, 0x0f, 0x35, 0x24,
  0xaa, 0x2b, 0x8b, 0x31, 0x9c, 0xfd, 0xa6, 0x26, 0xe0, 0x11, 0x24, 0x0d, 0xe7, 0x00, 0xd2, 0x2e, 0x4f, 0x0b, 0xe9, 0x19, 0x4f, 0x95, 0x2f, 0x53, 0xc7, 0xab, 0x05, 0x76, 0x2f, 0x91, 0x9f, 0xab, 0x5e, 0x79, 0xdd, 0x8a, 0x5e, 0x38, 0xdd, 0xec, 0xd2, 0xcb, 0xd6, 0xec, 0xe6, 0xc9, 0x56, 0xed, 0xf6, 0x0f, 0x25, 0xf8, 0x22, 0x9c, 0x23, 0x38, 0x94, 0xe0, 0xfa, 0x28, 0xde, 0x21, 0x1d, 0x7a, 0x70, 0x7c, 0x34, 0x37, 0x5f, 0x8c, 0x3d, 0x98, 0x29, 0x9a, 0x9f, 0x79, 0xfd, 0x55, 0x72, 0x65, 0x86, 0x30, 0x9f, 0xa2, 0x7a, 0x7f, 0x5a, 0x9f, 0x4c, 0x20, 0x02, 0x29, 0x83, 0x9f, 0xb5, 0x71, 0x08, 0x92, 0x68, 0xe3, 0x9f, 0x41, 0xa7, 0x76, 0x7c, 0x00, 0x44, 0x1c, 0x9f, 0x8c, 0x04, 0x23, 0x3e, 0x65, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
unsigned int offload_power_on_len = 638;

/*************************************************\
 *               Variables
\*************************************************/

// offloading stages
enum OffloadStages { OFFLOAD_NONE, OFFLOAD_BEFORE, OFFLOAD_ACTIVE, OFFLOAD_AFTER };

// variables
struct {
  uint8_t  stage         = OFFLOAD_NONE;  // current offloading state
  uint8_t  relay_state   = 0;             // relay state before offloading
  uint8_t  message_left  = 0;             // number of JSON messages to send
  uint8_t  message_delay = 0;             // delay before next JSON message (seconds)
  uint16_t power_inst    = 0;             // actual phase instant power (retrieved thru MQTT)
  uint32_t time_message  = 0;             // time of last message
  uint32_t time_stage    = 0;             // time of current stage
  uint32_t delay_removal = 0;             // delay before removing offload (seconds)
} offload_status;

struct {
  bool relay_managed    = true;           // flag to define if relay is managed directly
  bool topic_subscribed = false;          // flag for power subscription
  bool just_set         = false;          // flag to signal that offload has just been set
  bool just_removed     = false;          // flag to signal that offload has just been removed
  bool web_updated      = true;           // flag to uddate web client
} offload_flag;

struct offload_date {
  uint32_t time_start;                    // offload start time
  uint32_t time_stop;                     // offload release time
  uint32_t duration;                      // offload duration (in sec)
  uint16_t power;                         // power overload when offload triggered
};

struct {
  uint8_t index = 0;                            // current event index
  offload_date arr_event[OFFLOAD_EVENT_MAX];    // array of events
} offload_event;

// list of available choices in the configuration page
uint8_t arr_offload_device_available[OFFLOAD_DEVICE_MAX] = { OFFLOAD_DEVICE_APPLIANCE, OFFLOAD_DEVICE_FRIDGE, OFFLOAD_DEVICE_WASHING, OFFLOAD_DEVICE_DISH, OFFLOAD_DEVICE_DRIER, OFFLOAD_DEVICE_CUMULUS, OFFLOAD_DEVICE_IRON, OFFLOAD_DEVICE_OFFICE, OFFLOAD_DEVICE_LIVING, OFFLOAD_DEVICE_ROOM, OFFLOAD_DEVICE_BATHROOM, OFFLOAD_DEVICE_KITCHEN };

/**************************************************\
 *                  Accessors
\**************************************************/

// get power of device
uint16_t OffloadGetDevicePower ()
{
  return Settings.knx_GA_addr[0];
}

// set power of device
void OffloadSetDevicePower (uint16_t new_power)
{
  Settings.knx_GA_addr[0] = new_power;
}

// get maximum power limit before offload
uint16_t OffloadGetContractPower ()
{
  return Settings.knx_GA_addr[1];
}

// set maximum power limit before offload
void OffloadSetContractPower (uint16_t new_power)
{
  Settings.knx_GA_addr[1] = new_power;
}

// get delay in seconds before effective offloading
uint16_t OffloadGetDelayBeforeOffload ()
{
  return Settings.knx_GA_addr[2];
}

// set delay in seconds before effective offloading
void OffloadSetDelayBeforeOffload (uint16_t number)
{
  Settings.knx_GA_addr[2] = number;
}

// get delay in seconds before removing offload
uint16_t OffloadGetDelayBeforeRemoval ()
{
  return Settings.knx_GA_addr[3];
}

// set delay in seconds before removing offload
void OffloadSetDelayBeforeRemoval (uint16_t number)
{
  Settings.knx_GA_addr[3] = number;
}

// get phase number
uint16_t OffloadGetPhase ()
{
  uint16_t number;

  number = Settings.knx_GA_addr[4];
  if ((number == 0) || (number > 3)) number = 1;
  return number;
}

// set phase number
void OffloadSetPhase (uint16_t number)
{
  if ((number == 0) || (number > 3)) number = 1; 
  Settings.knx_GA_addr[4] = number;
}

// get extra overload percentage
int OffloadGetContractAdjustment ()
{
  int percentage;

  percentage = (int) Settings.knx_GA_addr[5];
  percentage -= 100;
  if (percentage < -99) percentage = -99;
  if (percentage > 100) percentage = 100;
  return percentage;
}

// set extra overload percentage
void OffloadSetContractAdjustment (int percentage)
{
  if (percentage < -99) percentage = -99;
  if (percentage > 100) percentage = 100;
  Settings.knx_GA_addr[5] = (uint16_t) (100 + percentage);
}

// get random delay addition
uint16_t OffloadGetDelayRandom ()
{
  return Settings.knx_GA_addr[6];
}

// set random delay addition in seconds
void OffloadSetDelayRandom (uint16_t new_delay)
{
  Settings.knx_GA_addr[6] = new_delay;
}

// get device type
uint16_t OffloadGetDeviceType ()
{
  bool     is_available = false;
  int      index;
  uint16_t device_type;

  // read stored type
  device_type = Settings.knx_GA_addr[7];

  // loop to chack if device is in the availability list
  for (index = 0; index < OFFLOAD_DEVICE_MAX; index ++) if (device_type == arr_offload_device_available[index]) is_available = true;

  // if not available or not in range
  if (!is_available || (device_type >= OFFLOAD_DEVICE_MAX))
  {
    // set to first of available devices list
    device_type = arr_offload_device_available[0];
    OffloadSetDeviceType (device_type);
  } 

  return device_type;
}

// set device type
void OffloadSetDeviceType (uint16_t new_type)
{
  if ( new_type < OFFLOAD_DEVICE_MAX)
  {
    // save appliance type
    Settings.knx_GA_addr[7] = new_type;

    // save associated delays
    OffloadSetDelayBeforeOffload (arr_offload_delay_before[new_type]);
    OffloadSetDelayBeforeRemoval (arr_offload_delay_after[new_type]);
    OffloadSetDelayRandom (arr_offload_delay_random[new_type]);
  }
}

// get instant power MQTT topic
void OffloadGetPowerTopic (bool get_default, char* pstr_result, int size_result)
{
  // get value or default value if not set
  strlcpy (pstr_result, SettingsText (SET_OFFLOAD_TOPIC), size_result);
  if (get_default == true || strlen (pstr_result) == 0) strlcpy (pstr_result, MQTT_OFFLOAD_TOPIC, size_result);
}

// set power MQTT topic
void OffloadSetPowerTopic (char* str_topic)
{
  SettingsUpdateText (SET_OFFLOAD_TOPIC, str_topic);
}

// get contract power JSON key
void OffloadGetContractPowerKey (bool get_default, char* pstr_result, int size_result)
{
  // get value or default value if not set
  strlcpy (pstr_result, SettingsText (SET_OFFLOAD_KEY_MAX), size_result);
  if (get_default == true || strlen (pstr_result) == 0) strlcpy (pstr_result, MQTT_OFFLOAD_KEY_MAX, size_result);
}

// set contract power JSON key
void OffloadSetContractPowerKey (char* str_key)
{
  SettingsUpdateText (SET_OFFLOAD_KEY_MAX, str_key);
}

// get instant power JSON key
void OffloadGetInstPowerKey (bool get_default, char* pstr_result, int size_result)
{
  char str_phase[4];

  // get value or default value if not set
  strlcpy (pstr_result, SettingsText (SET_OFFLOAD_KEY_INST), size_result);
  if (get_default == true || strlen (pstr_result) == 0)
  {
    itoa (OffloadGetPhase (), str_phase, sizeof (str_phase));
    strlcpy (pstr_result, MQTT_OFFLOAD_KEY_INST, size_result);
    strcat (pstr_result, str_phase);
  } 
}

// set instant power JSON key
void OffloadSetInstPowerKey (char* str_key)
{
  SettingsUpdateText (SET_OFFLOAD_KEY_INST, str_key);
}

/**************************************************\
 *                  Functions
\**************************************************/

// declare device type in the available list
void OffloadSetAvailableDevice (uint8_t index, uint8_t device_type)
{
  if ((index < OFFLOAD_DEVICE_MAX) && (device_type <= OFFLOAD_DEVICE_MAX)) arr_offload_device_available[index] = device_type;
}

// get maximum power limit before offload
uint16_t OffloadGetMaxPower ()
{
  int      adjust_percent;
  uint16_t power_contract;
  uint32_t power_max;

  // read data
  power_contract = OffloadGetContractPower ();
  adjust_percent = OffloadGetContractAdjustment ();

  // calculate maximum power including extra overload
  power_max = power_contract * (100 + adjust_percent) / 100;

  return (uint16_t) power_max;
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

  // get the flag and reset it
  result = offload_flag.just_set;
  offload_flag.just_set = false;

  return (result);
}

// get offload newly removed state
bool OffloadJustRemoved ()
{
  bool result;

  // get the flag and reset it
  result = offload_flag.just_removed;
  offload_flag.just_removed = false;

  return (result);
}

// set relay managed mode
void OffloadSetRelayMode (bool is_managed)
{
  offload_flag.relay_managed = is_managed;
}

// activate offload state
void OffloadActivate ()
{
  // set flag to signal offload has just been set
  offload_flag.just_set     = true;
  offload_flag.just_removed = false;

  // set overload and start time for current event
  offload_event.index ++;
  offload_event.index = offload_event.index % OFFLOAD_EVENT_MAX;
  offload_event.arr_event[offload_event.index].power = offload_status.power_inst;
  offload_event.arr_event[offload_event.index].time_start = LocalTime ();
  offload_event.arr_event[offload_event.index].time_stop  = UINT32_MAX;
  offload_event.arr_event[offload_event.index].duration   = UINT32_MAX;

  // read relay state and switch off if needed
  if (offload_flag.relay_managed == true)
  {
    // save relay state
    offload_status.relay_state = bitRead (TasmotaGlobal.power, 0);

    // if relay is ON, switch off
    if (offload_status.relay_state == 1) ExecuteCommandPower (1, POWER_OFF, SRC_MAX);
  }

  // get relay state and log
  AddLog (LOG_LEVEL_INFO, PSTR ("PWR: Offload starts (relay = %d)"), offload_status.relay_state);
}

// remove offload state
void OffloadRemove ()
{
  // set flag to signal offload has just been removed
  offload_flag.just_removed = true;
  offload_flag.just_set     = false;

  // set release time for current event
  offload_event.arr_event[offload_event.index].time_stop = LocalTime ();
  offload_event.arr_event[offload_event.index].duration  = offload_event.arr_event[offload_event.index].time_stop - offload_event.arr_event[offload_event.index].time_start;

  // if relay managed and it was ON, switch it back
  if ((offload_flag.relay_managed) && (offload_status.relay_state == 1)) ExecuteCommandPower (1, POWER_ON, SRC_MAX);

  // log offloading removal
  AddLog (LOG_LEVEL_INFO, PSTR ("PWR: Offload stops (relay = %d)"), offload_status.relay_state);
}

// Show JSON status (for MQTT)
void OffloadShowJSON (bool append)
{
  char str_buffer[64];

  // web client needs update
  offload_flag.web_updated = true;

  // add , in append mode
  if (append) ResponseAppend_P (PSTR (",")); else Response_P (PSTR ("{"));

  // "Offload":{
  //   "State":"OFF","Stage":1,
  //   "Phase":2,"Before":1,"After":5,"Device":1000,"Contract":5000,"Topic":"...","KeyInst":"...","KeyMax":"..."       
  //   }
  ResponseAppend_P (PSTR ("\"%s\":{"), D_JSON_OFFLOAD);
  
  // dynamic data
  if (OffloadIsOffloaded ()) strcpy (str_buffer, MQTT_STATUS_ON); else strcpy (str_buffer, MQTT_STATUS_OFF);
  ResponseAppend_P (PSTR ("\"%s\":\"%s\",\"%s\":%d"), D_JSON_OFFLOAD_STATE, str_buffer, D_JSON_OFFLOAD_STAGE, offload_status.stage);
  
  // static data
  if (append == true) 
  {
    // offloading phase number and delays
    ResponseAppend_P (PSTR (",\"%s\":%d,\"%s\":%d,\"%s\":%d"), D_JSON_OFFLOAD_PHASE, OffloadGetPhase (), D_JSON_OFFLOAD_BEFORE, OffloadGetDelayBeforeOffload (), D_JSON_OFFLOAD_AFTER, OffloadGetDelayBeforeRemoval ());

    // offloading power metrics
    ResponseAppend_P (PSTR (",\"%s\":%d,\"%s\":%d,\"%s\":%d,\"%s\":%d"), D_JSON_OFFLOAD_DEVICE, OffloadGetDevicePower (), D_JSON_OFFLOAD_MAX, OffloadGetMaxPower (), D_JSON_OFFLOAD_CONTRACT, OffloadGetContractPower (), D_JSON_OFFLOAD_ADJUST, OffloadGetContractAdjustment ());

    // offloading power MQTT topic
    OffloadGetPowerTopic (false, str_buffer, sizeof (str_buffer));
    ResponseAppend_P (PSTR (",\"%s\":\"%s\""), D_JSON_OFFLOAD_TOPIC, str_buffer);

    // offloading instant power MQTT key
    OffloadGetInstPowerKey (false, str_buffer, sizeof (str_buffer));
    ResponseAppend_P (PSTR (",\"%s\":\"%s\""), D_JSON_OFFLOAD_KEY_INST, str_buffer);

    // offloading contract power MQTT key
    OffloadGetContractPowerKey (false, str_buffer, sizeof (str_buffer));
    ResponseAppend_P (PSTR (",\"%s\":\"%s\""), D_JSON_OFFLOAD_KEY_MAX, str_buffer);
  }

  ResponseAppend_P (PSTR ("}"));

  // publish it if not in append mode
  if (!append)
  {
    ResponseAppend_P (PSTR ("}"));
    MqttPublishPrefixTopic_P (TELE, PSTR (D_RSLT_SENSOR));
  } 
}

// check and update MQTT power subsciption after disconnexion
void OffloadCheckConnexion ()
{
  bool is_connected;
  char str_buffer[64];

  // check MQTT connexion
  is_connected = MqttIsConnected ();
  if (is_connected)
  {
    // if still no subsciption to power topic
    if (offload_flag.topic_subscribed == false)
    {
      // check power topic availability
      OffloadGetPowerTopic (false, str_buffer, sizeof (str_buffer));
      if (strlen (str_buffer) > 0) 
      {
        // subscribe to power meter
        MqttSubscribe (str_buffer);
        AddLog (LOG_LEVEL_INFO, PSTR ("MQT: Subscribed to %s"), str_buffer);

        // subscription done
        offload_flag.topic_subscribed = true;
      }
    }
  }
  else offload_flag.topic_subscribed = false;

  // check if offload JSON messages need to be sent
  if (offload_status.message_left > 0)
  {
    // decrement delay
    offload_status.message_delay--;

    // if delay is reached
    if (offload_status.message_delay == 0)
    {
      // send MQTT message
      OffloadShowJSON (false);

      // update counters
      offload_status.message_left--;
      if (offload_status.message_left > 0) offload_status.message_delay = OFFLOAD_MESSAGE_DELAY;
    }
  }
}

// Handle offloading MQTT commands
bool OffloadMqttCommand ()
{
  bool command_handled = true;
  int  command_code;
  char command [CMDSZ];

  // check MQTT command
  command_code = GetCommandCode (command, sizeof(command), XdrvMailbox.topic, kOffloadCommands);

  // handle command
  switch (command_code)
  {
    case CMND_OFFLOAD_DEVICE:       // set device power
      OffloadSetDevicePower (XdrvMailbox.payload);
      break;
    case CMND_OFFLOAD_TYPE:         // set appliance type
      OffloadSetDeviceType (XdrvMailbox.payload);
      break;
    case CMND_OFFLOAD_CONTRACT:     // set contract power per phase
      OffloadSetContractPower (XdrvMailbox.payload);
      break;
    case CMND_OFFLOAD_ADJUST:       // set contract adjustment in percentage
      OffloadSetContractAdjustment (XdrvMailbox.payload);
      break;
    default:
      command_handled = false;
  }
  
  return command_handled;
}

// read received MQTT data to retrieve house instant power
bool OffloadMqttData ()
{
  bool    data_handled = false;
  int     mqtt_power = 0;
  char    str_buffer[64];
  char    str_key[64];
  char*   pstr_result;

  // if topic is the instant house power
  OffloadGetPowerTopic (false, str_buffer, sizeof (str_buffer));
  if (strcmp (str_buffer, XdrvMailbox.topic) == 0)
  {
    // log and counter increment
    AddLog (LOG_LEVEL_INFO, PSTR ("MQT: Received %s"), str_buffer);
    offload_status.time_message = LocalTime ();

    // look for instant power key
    OffloadGetInstPowerKey (false, str_buffer, sizeof (str_buffer));
    sprintf (str_key, "\"%s\":", str_buffer);
    pstr_result = strstr (XdrvMailbox.data, str_key);
    if (pstr_result != nullptr) offload_status.power_inst = atoi (pstr_result + strlen (str_key));
    data_handled |= (pstr_result != nullptr);

    // look for max power key
    OffloadGetContractPowerKey (false, str_buffer, sizeof (str_buffer));
    sprintf (str_key, "\"%s\":", str_buffer);
    pstr_result = strstr (XdrvMailbox.data, str_key);
    if (pstr_result != nullptr) mqtt_power = atoi (pstr_result + strlen (str_key));
    if (mqtt_power > 0) OffloadSetContractPower ((uint16_t)mqtt_power);
    data_handled |= (pstr_result != nullptr);
  }

  return data_handled;
}

// update offloading status according to all parameters
void OffloadEvery250ms ()
{
  uint8_t  prev_stage, next_stage;
  uint16_t power_mesured, power_device, power_max;
  uint32_t time_end, time_now;

  // if relay state changed, set udate web flag
  if (offload_status.relay_state != bitRead (TasmotaGlobal.power, 0)) offload_flag.web_updated = true;

  // get device power and global power limit
  power_device = OffloadGetDevicePower ();
  power_max    = OffloadGetMaxPower ();

  #ifdef USE_ENERGY_SENSOR
  power_mesured = (uint16_t)Energy.active_power[0];
  #else
  power_mesured = 0;
  #endif

  // check if device instant power is beyond defined power
  if (power_mesured > power_device)
  {
    power_device = power_mesured;
    OffloadSetDevicePower (power_mesured);
  }

  // get current time
  time_now = millis () / 1000;

  // if contract power and device power are defined
  if ((power_max > 0) && (power_device > 0))
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
        if (offload_status.power_inst > power_max)
        { 
          // set time for effective offloading calculation
          offload_status.time_stage = time_now;

          // next state is before offloading
          next_stage = OFFLOAD_BEFORE;
        }
        break;

      // pending offloading
      case OFFLOAD_BEFORE:
        // save relay state
        offload_status.relay_state = bitRead (TasmotaGlobal.power, 0);

        // if house power has gone down, remove pending offloading
        if (offload_status.power_inst <= power_max) next_stage = OFFLOAD_NONE;

        // else if delay is reached, set active offloading
        else
        {
          time_end = offload_status.time_stage + (uint32_t) OffloadGetDelayBeforeOffload ();
          if (time_end < time_now)
          {
            // set next stage as offloading
            next_stage = OFFLOAD_ACTIVE;

            // set offload
            OffloadActivate ();
          }
        } 
        break;

      // offloading is active
      case OFFLOAD_ACTIVE:
        // calculate maximum power allowed when substracting device power
        if (power_max > power_device) power_max -= power_device; else power_max = 0;

        // if instant power is under this value, prepare to remove offload
        if (offload_status.power_inst <= power_max)
        {
          // set time for removing offloading calculation
          offload_status.time_stage = time_now;

          // set stage to after offloading
          next_stage = OFFLOAD_AFTER;
        }
        break;

      // actually just after offloading should stop
      case OFFLOAD_AFTER:
        // calculate maximum power allowed when substracting device power
        if (power_max > power_device) power_max -= power_device; else power_max = 0;

        // if house power has gone again too high, offloading back again
        if (offload_status.power_inst > power_max) next_stage = OFFLOAD_ACTIVE;
        
        // else if delay is reached, set active offloading
        else
        {
          time_end = offload_status.time_stage + offload_status.delay_removal;
          if (time_end < time_now)
          {
            // set stage to after offloading
            next_stage = OFFLOAD_NONE;

            // remove offloading state
            OffloadRemove ();
          } 
        } 
        break;
    }

    // update offloading state
    offload_status.stage = next_stage;

    // if state has changed,
    if (next_stage != prev_stage)
    {
      // send MQTT status
      OffloadShowJSON (false);

      // set counters of JSON message update
      offload_status.message_left  = OFFLOAD_MESSAGE_LEFT;
      offload_status.message_delay = OFFLOAD_MESSAGE_DELAY;
    } 
  }
}

// offload initialisation
void OffloadInit ()
{
  uint8_t index;

  // calculate offload removal delay including randomisation
  offload_status.delay_removal  = (uint32_t) OffloadGetDelayBeforeRemoval ();
  offload_status.delay_removal += (uint32_t) random (0, OffloadGetDelayRandom ());

  // loop to init offload events array
  for (index = 0; index < OFFLOAD_EVENT_MAX; index++)
  {
    offload_event.arr_event[index].time_start = UINT32_MAX;
    offload_event.arr_event[index].time_stop  = UINT32_MAX;
    offload_event.arr_event[index].duration   = UINT32_MAX;
  }
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// offload icon
void OffloadWebIconOffload ()     { Webserver->send (200, "image/png", offload_icon_png, offload_icon_len); }

// plug icons
void OffloadWebIconPlugOff ()     { Webserver->send (200, "image/png", offload_plug_off_png, offload_plug_off_len); }
void OffloadWebIconPlugOn ()      { Webserver->send (200, "image/png", offload_plug_on_png,   offload_plug_on_len);   }
void OffloadWebIconPlugOffload () { Webserver->send (200, "image/png", offload_plug_offload_png, offload_plug_offload_len); }
void OffloadWebIconPlug ()
{
  if (OffloadIsOffloaded ()) OffloadWebIconPlugOffload ();
  else if (bitRead (TasmotaGlobal.power, 0) == 1) OffloadWebIconPlugOn ();
  else OffloadWebIconPlugOff ();
}

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

void OffloadWebUpdated ()
{
  if (offload_flag.web_updated) Webserver->send (200, "text/plain", "1", 1); else Webserver->send (200, "text/plain", "0", 1);
  offload_flag.web_updated = false;
}

// JSON history page
void OffloadWebPageHistoryJson ()
{
  bool    first_value = true;
  uint8_t index, index_array;
  TIME_T  event_dst;

  // start of data page
  WSContentBegin(200, CT_HTML);
  WSContentSend_P (PSTR ("{\"event\":["));

  // loop thru offload events array
  for (index = 1; index <= OFFLOAD_EVENT_MAX; index++)
  {
    // get target power array position and add value if defined
    index_array = (index + offload_event.index) % OFFLOAD_EVENT_MAX;
    if (offload_event.arr_event[index_array].time_start != UINT32_MAX)
    {
      if (first_value) WSContentSend_P (PSTR ("{")); else WSContentSend_P (PSTR (",{"));
      first_value = false;

      // start time
      BreakTime (offload_event.arr_event[index_array].time_start, event_dst);
      if (event_dst.year < 1970) event_dst.year += 1970;
      WSContentSend_P (PSTR (",\"start\":\"%d-%02d-%02dT%02d:%02d:%02d.000Z\""), event_dst.year, event_dst.month, event_dst.day_of_month, event_dst.hour, event_dst.minute, event_dst.second);

      // release time
      WSContentSend_P (PSTR (",\"stop\":\""));
      if (offload_event.arr_event[index_array].time_stop != UINT32_MAX)
      {
        BreakTime (offload_event.arr_event[index_array].time_stop, event_dst);
        if (event_dst.year < 1970) event_dst.year += 1970;
        WSContentSend_P (PSTR ("%d-%02d-%02dT%02d:%02d:%02d.000Z"), event_dst.year, event_dst.month, event_dst.day_of_month, event_dst.hour, event_dst.minute, event_dst.second);
      }
      WSContentSend_P (PSTR ("\"}"));

      // duration
      WSContentSend_P (PSTR (",\"duration\":\""));
      if (offload_event.arr_event[index_array].duration != UINT32_MAX) WSContentSend_P (PSTR ("%d"), offload_event.arr_event[index_array].duration);
      WSContentSend_P (PSTR ("\"}"));

      // overload power
      WSContentSend_P (PSTR ("\"power\":%d"), offload_event.arr_event[index_array].power);
    }
  }

  // end of page
  WSContentSend_P (PSTR ("]}"));
  WSContentEnd();
}

// append offloading state to main page
void OffloadWebSensor ()
{
  uint16_t index, device_power;
  uint32_t time_now, time_left, time_end;
  char     str_text[40], str_value[8];
  uint32_t message_delay;
  TIME_T   message_dst;

  // display device type
  index = OffloadGetDeviceType ();
  WSContentSend_PD (PSTR ("{s}%s %s{m}%s{e}"), D_OFFLOAD_DEVICE, D_OFFLOAD_TYPE, arr_offload_device_label[index]);

  // display appliance type
  time_left = (uint32_t) OffloadGetDelayBeforeOffload ();
  WSContentSend_PD (PSTR ("{s}%s <small>(Off/On)</small>{m}%d / %d %s{e}"), D_OFFLOAD_DELAY, time_left, offload_status.delay_removal, D_OFFLOAD_UNIT_SEC);

  // device power
  index = OffloadGetPhase ();
  device_power = OffloadGetDevicePower ();
  WSContentSend_PD (PSTR ("{s}%s <small><i>[phase %d]</i></small>{m}%d W{e}"), D_OFFLOAD_POWER, index, device_power);

  // if house power is subscribed, display power
  if (offload_flag.topic_subscribed)
  {
    // calculate delay since last power message
    strcpy (str_text, "...");
    if (offload_status.time_message > 0)
    {
      // calculate delay
      message_delay = LocalTime() - offload_status.time_message;
      BreakTime (message_delay, message_dst);

      // generate readable format
      strcpy (str_text, "");
      if (message_dst.hour > 0)
      {
        sprintf (str_value, "%dh", message_dst.hour);
        strlcat (str_text, str_value, sizeof (str_text));
      }
      if (message_dst.hour > 0 || message_dst.minute > 0)
      {
        sprintf (str_value, "%dm", message_dst.minute);
        strlcat (str_text, str_value, sizeof (str_text));
      }
      sprintf (str_value, "%ds", message_dst.second);
      strlcat (str_text, str_value, sizeof (str_text));
    }

    // display current power and max power limit
    device_power = OffloadGetMaxPower ();
    if (device_power > 0) WSContentSend_PD (PSTR ("{s}%s <small><i>[%s]</i></small>{m}<b>%d</b> / %d W{e}"), D_OFFLOAD_CONTRACT, str_text, offload_status.power_inst, device_power);

    // switch according to current state
    time_now  = millis () / 1000;
    time_left = 0;
    strcpy (str_text, "");
    strcpy (str_value, "");
    
    switch (offload_status.stage)
    { 
      // calculate number of ms left before offloading
      case OFFLOAD_BEFORE:
        time_end = offload_status.time_stage + (uint32_t) OffloadGetDelayBeforeOffload ();
        if (time_end > time_now) time_left = time_end - time_now;
        strcpy (str_value, "orange");
        sprintf (str_text, "Starting in <b>%d sec.</b>" , time_left);
        break;

      // calculate number of ms left before offload removal
      case OFFLOAD_AFTER:
        time_end = offload_status.time_stage + offload_status.delay_removal;
        if (time_end > time_now) time_left = time_end - time_now;
        strcpy (str_value, "red");
        sprintf (str_text, "Ending in <b>%d sec.</b>", time_left);
        break;

      // device is offloaded
      case OFFLOAD_ACTIVE:
        strcpy (str_value, "red");
        strlcpy (str_text, "<b>Active</b>", sizeof (str_text));
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
  char     str_argument[32];
  char     str_default[32];


  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg("save"))
  {
    // set power of heater according to 'power' parameter
    WebGetArg (D_CMND_OFFLOAD_DEVICE, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) OffloadSetDevicePower ((uint16_t)atoi (str_argument));

    // set contract power limit according to 'contract' parameter
    WebGetArg (D_CMND_OFFLOAD_TYPE, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) OffloadSetDeviceType ((uint16_t)atoi (str_argument));

    // set contract power limit according to 'contract' parameter
    WebGetArg (D_CMND_OFFLOAD_CONTRACT, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) OffloadSetContractPower ((uint16_t)atoi (str_argument));

    // set contract power limit according to 'adjust' parameter
    WebGetArg (D_CMND_OFFLOAD_ADJUST, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) OffloadSetContractAdjustment (atoi (str_argument));

    // set phase number according to 'phase' parameter
    WebGetArg (D_CMND_OFFLOAD_PHASE, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) OffloadSetPhase ((uint16_t)atoi (str_argument));

    // set delay in sec. before offloading device according to 'before' parameter
    WebGetArg (D_CMND_OFFLOAD_BEFORE, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) OffloadSetDelayBeforeOffload ((uint16_t)atoi (str_argument));

    // set delay in sec. after offloading device according to 'after' parameter
    WebGetArg (D_CMND_OFFLOAD_AFTER, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) OffloadSetDelayBeforeRemoval ((uint16_t)atoi (str_argument));

    // set random delay in sec. according to 'random' parameter
    WebGetArg (D_CMND_OFFLOAD_RANDOM,str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) OffloadSetDelayRandom ((uint16_t)atoi (str_argument));

    // set MQTT topic according to 'topic' parameter
    WebGetArg (D_CMND_OFFLOAD_TOPIC, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) OffloadSetPowerTopic (str_argument);

    // set JSON key according to 'inst' parameter
    WebGetArg (D_CMND_OFFLOAD_KEY_INST, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) OffloadSetInstPowerKey (str_argument);

    // set JSON key according to 'max' parameter
    WebGetArg (D_CMND_OFFLOAD_KEY_MAX, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) OffloadSetContractPowerKey (str_argument);

    // restart device
    WebRestart (1);
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
  result = (int) OffloadGetDeviceType ();
  WSContentSend_P (PSTR ("<p>%s<span class='key'>%s</span>\n"), D_OFFLOAD_TYPE, D_CMND_OFFLOAD_TYPE);
  WSContentSend_P (PSTR ("<select name='%s' id='%s'>\n"), D_CMND_OFFLOAD_TYPE, D_CMND_OFFLOAD_TYPE);
  for (index = 0; index < OFFLOAD_DEVICE_MAX; index ++)
  {
    device = arr_offload_device_available[index];
    if (device < OFFLOAD_DEVICE_MAX)
    {
      if (device == result) strcpy (str_argument, D_OFFLOAD_SELECTED); else strcpy (str_argument, "");
      WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>\n"), device, str_argument, arr_offload_device_label[device]);
    }
  }
  WSContentSend_P (PSTR ("</select>\n"));
  WSContentSend_P (PSTR ("</p>\n"));

  // device power
  power = OffloadGetDevicePower ();
  sprintf (str_argument, "%s (%s)", D_OFFLOAD_POWER, D_OFFLOAD_UNIT_W);
  WSContentSend_P (OFFLOAD_INPUT_NUMBER, "none", str_argument, D_CMND_OFFLOAD_DEVICE, "none", D_CMND_OFFLOAD_DEVICE, D_CMND_OFFLOAD_DEVICE, 0, 65000, 1,  power);

  // delays
  value = (int) OffloadGetDelayBeforeOffload ();
  sprintf (str_argument, "%s (%s)", D_OFFLOAD_BEFORE, D_OFFLOAD_UNIT_SEC);
  WSContentSend_P (OFFLOAD_INPUT_NUMBER, "third", str_argument, D_CMND_OFFLOAD_BEFORE, "switch", D_CMND_OFFLOAD_BEFORE, D_CMND_OFFLOAD_BEFORE, 0, 60, 1, value);
  value = (int) OffloadGetDelayBeforeRemoval ();
  sprintf (str_argument, "%s (%s)", D_OFFLOAD_AFTER, D_OFFLOAD_UNIT_SEC);
  WSContentSend_P (OFFLOAD_INPUT_NUMBER, "third", str_argument, D_CMND_OFFLOAD_AFTER, "switch", D_CMND_OFFLOAD_AFTER, D_CMND_OFFLOAD_AFTER, 0, 600, 1, value);
  value = (int) OffloadGetDelayRandom ();
  sprintf (str_argument, "%s (%s)", D_OFFLOAD_RANDOM, D_OFFLOAD_UNIT_SEC);
  WSContentSend_P (OFFLOAD_INPUT_NUMBER, "third", str_argument, D_CMND_OFFLOAD_RANDOM, "switch", D_CMND_OFFLOAD_RANDOM, D_CMND_OFFLOAD_RANDOM, 0, 60, 1, value);
  
  WSContentSend_P (OFFLOAD_FIELDSET_STOP);

  // --------------
  //    Contract  
  // --------------

  WSContentSend_P (OFFLOAD_FIELDSET_START, D_OFFLOAD_CONTRACT);

  // contract power
  power = OffloadGetContractPower ();
  sprintf (str_argument, "%s (%s)", D_OFFLOAD_CONTRACT, D_OFFLOAD_UNIT_VA);
  WSContentSend_P (OFFLOAD_INPUT_NUMBER, "none", str_argument, D_CMND_OFFLOAD_CONTRACT, "none", D_CMND_OFFLOAD_CONTRACT, D_CMND_OFFLOAD_CONTRACT, 0, 65000, 1, power);

  // contract adjustment
  value = OffloadGetContractAdjustment ();
  sprintf (str_argument, "%s (%s)", D_OFFLOAD_ADJUST, D_OFFLOAD_UNIT_PERCENT);
  WSContentSend_P (OFFLOAD_INPUT_NUMBER, "half", str_argument, D_CMND_OFFLOAD_ADJUST, "none", D_CMND_OFFLOAD_ADJUST, D_CMND_OFFLOAD_ADJUST, -99, 100, 1, value);

  // phase
  value = (int) OffloadGetPhase ();
  WSContentSend_P (PSTR ("<p class='%s'>%s<span class='key'>%s</span>\n"), "half", D_OFFLOAD_PHASE, D_CMND_OFFLOAD_PHASE);
  WSContentSend_P (PSTR ("<select name='%s' id='%s'>\n"), D_CMND_OFFLOAD_PHASE, D_CMND_OFFLOAD_PHASE);
  for (index = 1; index <= OFFLOAD_PHASE_MAX; index ++)
  {
    if (index == value) strcpy (str_argument, D_OFFLOAD_SELECTED); else strcpy (str_argument, "");
    WSContentSend_P (PSTR ("<option value='%d' %s>%d</option>\n"), index, str_argument, index);
  }
  WSContentSend_P (PSTR ("</select>\n"));
  WSContentSend_P (PSTR ("</p>\n"));

  WSContentSend_P (OFFLOAD_FIELDSET_STOP);

  // ------------
  //    Meter  
  // ------------

  WSContentSend_P (OFFLOAD_FIELDSET_START, D_OFFLOAD_METER);

  // instant power mqtt topic
  OffloadGetPowerTopic (false, str_argument, sizeof (str_argument));
  OffloadGetPowerTopic (true, str_default, sizeof (str_default));
  if (strcmp (str_argument, str_default) == 0) strcpy (str_argument, "");
  WSContentSend_P (OFFLOAD_INPUT_TEXT, D_OFFLOAD_TOPIC, D_CMND_OFFLOAD_TOPIC, D_CMND_OFFLOAD_TOPIC, str_argument, str_default);

  // max power json key
  OffloadGetContractPowerKey (false, str_argument, sizeof (str_argument));
  OffloadGetContractPowerKey (true, str_default, sizeof (str_default));
  if (strcmp (str_argument, str_default) == 0) strcpy (str_argument, "");
  WSContentSend_P (OFFLOAD_INPUT_TEXT, D_OFFLOAD_KEY_MAX, D_CMND_OFFLOAD_KEY_MAX, D_CMND_OFFLOAD_KEY_MAX, str_argument, str_default);

  // instant power json key
  OffloadGetInstPowerKey (false, str_argument, sizeof (str_argument));
  OffloadGetInstPowerKey (true, str_default, sizeof (str_default));
  if (strcmp (str_argument, str_default) == 0) strcpy (str_argument, "");
  WSContentSend_P (OFFLOAD_INPUT_TEXT, D_OFFLOAD_KEY_INST, D_CMND_OFFLOAD_KEY_INST, D_CMND_OFFLOAD_KEY_INST, str_argument, str_default);

  WSContentSend_P (OFFLOAD_FIELDSET_STOP);
  WSContentSend_P (PSTR ("<br>\n"));

  // ------------
  //    Script  
  // ------------

  WSContentSend_P (PSTR ("<script type='text/javascript'>\n"));

  WSContentSend_P (PSTR ("var arr_delay_before = [%d"), arr_offload_delay_before[0]);
  for (index = 1; index < OFFLOAD_DEVICE_MAX; index ++) WSContentSend_P (PSTR (",%d"), arr_offload_delay_before[index]);
  WSContentSend_P (PSTR ("];\n"));

  WSContentSend_P (PSTR ("var arr_delay_after = [%d"), arr_offload_delay_after[0]);
  for (index = 1; index < OFFLOAD_DEVICE_MAX; index ++) WSContentSend_P (PSTR (",%d"), arr_offload_delay_after[index]);
  WSContentSend_P (PSTR ("];\n"));

  WSContentSend_P (PSTR ("var arr_delay_random = [%d"), arr_offload_delay_random[0]);
  for (index = 1; index < OFFLOAD_DEVICE_MAX; index ++) WSContentSend_P (PSTR (",%d"), arr_offload_delay_random[index]);
  WSContentSend_P (PSTR ("];\n"));

  WSContentSend_P (PSTR ("var device_type  = document.getElementById ('type');\n"));
  WSContentSend_P (PSTR ("var delay_before = document.getElementById ('before');\n"));
  WSContentSend_P (PSTR ("var delay_after  = document.getElementById ('after');\n"));
  WSContentSend_P (PSTR ("var delay_random = document.getElementById ('random');\n"));

  WSContentSend_P (PSTR ("device_type.onchange = function () {\n"));
  WSContentSend_P (PSTR ("delay_before.value = arr_delay_before[this.value];\n"));
  WSContentSend_P (PSTR ("delay_after.value = arr_delay_after[this.value];\n"));
  WSContentSend_P (PSTR ("delay_random.value = arr_delay_random[this.value];\n"));
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

// Offload history page
void OffloadWebPageHistory ()
{
  uint8_t index, index_array, counter;
  TIME_T  event_dst;

  // beginning of page without authentification
  WSContentStart_P (D_OFFLOAD " " D_OFFLOAD_EVENT, false);
  WSContentSend_P (PSTR ("</script>\n"));

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {width:100%;margin:12px auto;padding:3px 0px;text-align:center;vertical-align:middle;}\n"));
  WSContentSend_P (PSTR ("div.title {font-size:5vw;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.header {font-size:5vw;color:yellow;}\n"));
  WSContentSend_P (PSTR ("table {display:inline-block;border:none;background:none;padding:0.5rem;color:#fff;border-collapse:collapse;}\n"));
  WSContentSend_P (PSTR ("th,td {font-size:3vw;padding:0.5rem 1rem;border:1px #666 solid;}\n"));
  WSContentSend_P (PSTR ("th {font-style:bold;background:#555;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // page body
  WSContentSend_P (PSTR ("</head>\n"));
  WSContentSend_P (PSTR ("<body>\n"));

  // device name, icon and title
  WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText(SET_DEVICENAME));
  WSContentSend_P (PSTR ("<div class='header'>Offload Events</div>\n"));
  WSContentSend_P (PSTR ("<div><img height=64 src='offload.png'></div>\n"));

  // display table with header
  WSContentSend_P (PSTR ("<div><table>\n"));
  WSContentSend_P (PSTR ("<tr><th>Start</th><th>Stop</th><th>Duration</th><th>Power</th></tr>\n"));

  // loop thru offload events array
  counter = 0;
  for (index = 1; index <= OFFLOAD_EVENT_MAX; index++)
  {
    // get target power array position and add value if defined
    index_array = (index + offload_event.index) % OFFLOAD_EVENT_MAX;
    if (offload_event.arr_event[index_array].time_start != UINT32_MAX)
    {
      // increase events counter
      counter ++;

      // beginning of line
      WSContentSend_P (PSTR ("<tr>"));

      // start time
      BreakTime (offload_event.arr_event[index_array].time_start, event_dst);
      if (event_dst.year < 1970) event_dst.year += 1970;
      WSContentSend_P (PSTR ("<td>%d-%02d-%02d %02d:%02d:%02d</td>"), event_dst.year, event_dst.month, event_dst.day_of_month, event_dst.hour, event_dst.minute, event_dst.second);

      // release time and duration
      if (offload_event.arr_event[index_array].time_stop != UINT32_MAX)
      {
        // release time
        BreakTime (offload_event.arr_event[index_array].time_stop, event_dst);
        if (event_dst.year < 1970) event_dst.year += 1970;
        WSContentSend_P (PSTR ("<td>%d-%02d-%02d %02d:%02d:%02d</td>"), event_dst.year, event_dst.month, event_dst.day_of_month, event_dst.hour, event_dst.minute, event_dst.second);

        // duration
        WSContentSend_P (PSTR ("<td>"));
        BreakTime (offload_event.arr_event[index_array].duration, event_dst);
        if (event_dst.days > 0) WSContentSend_P (PSTR ("%dh "), event_dst.days);
        if (offload_event.arr_event[index_array].duration >= 3600) WSContentSend_P (PSTR ("%dh "), event_dst.hour);
        if (offload_event.arr_event[index_array].duration >= 60) WSContentSend_P (PSTR ("%dm "), event_dst.minute);
        WSContentSend_P (PSTR ("%ds</td>"), event_dst.second);
      }
      else WSContentSend_P (PSTR ("<td colspan=2>Currently active</td>"));

      // overload power
      WSContentSend_P (PSTR ("<td>%d</td>"), offload_event.arr_event[index_array].power);

      // end of line
      WSContentSend_P (PSTR ("</tr>"));
    }
  }

  // if no event
  if (counter == 0) WSContentSend_P (PSTR ("<td colspan=4>No offload event recorded</td>"));

  // end of table and end of page
  WSContentSend_P (PSTR ("</table></div>\n"));
  WSContentStop ();
}

// Offloading public configuration page
void OffloadWebPageControl ()
{
  bool updated = false;

  // if switch has been switched OFF
  if (Webserver->hasArg(D_CMND_OFFLOAD_TOGGLE))
  {
    // status updated
    updated = true;

    // if device is actually offloaded, set next state
    if (OffloadIsOffloaded ()) { if ( offload_status.relay_state == 0) offload_status.relay_state = 1; else offload_status.relay_state = 0; }

    // else set current state
    else { if (bitRead (TasmotaGlobal.power, 0) == 1) ExecuteCommandPower (1, POWER_OFF, SRC_MAX); else ExecuteCommandPower (1, POWER_ON, SRC_MAX); }
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
    WSContentSend_P (PSTR ("<div class='icon'><img class='plug' id='plug' src='plug.png?ts=0'></div>\n"));

    // display switch button
    WSContentSend_P (PSTR ("<div><button class='power' type='submit' name='%s' title='%s'><img class='power' id='power' src='power.png?ts=0' /></button>"), D_CMND_OFFLOAD_TOGGLE,  D_OFFLOAD_TOGGLE);

    // end of page
    WSContentSend_P (PSTR ("</form>\n"));
    WSContentStop ();
  }
}

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
    case FUNC_MQTT_INIT:
      OffloadInit ();
      OffloadCheckConnexion ();
      break;
    case FUNC_MQTT_DATA:
      result = OffloadMqttData ();
      break;
    case FUNC_COMMAND:
      result = OffloadMqttCommand ();
      break;
    case FUNC_EVERY_250_MSECOND:
      OffloadEvery250ms ();
      break;
    case FUNC_EVERY_SECOND:
      OffloadCheckConnexion ();
      break;
  }
  
  return result;
}

bool Xsns96 (uint8_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
    case FUNC_JSON_APPEND:
      OffloadShowJSON (true);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      //pages
      Webserver->on ("/" D_PAGE_OFFLOAD_CONFIG, OffloadWebPageConfig);
      Webserver->on ("/" D_PAGE_OFFLOAD_HISTORY, OffloadWebPageHistory);
      Webserver->on ("/" D_PAGE_OFFLOAD_JSON,    OffloadWebPageHistoryJson);
      if (offload_flag.relay_managed) Webserver->on ("/" D_PAGE_OFFLOAD_CONTROL, OffloadWebPageControl);

      // update status
      Webserver->on ("/offload.upd", OffloadWebUpdated);

      // icons
      Webserver->on ("/offload.png", OffloadWebIconOffload);
      Webserver->on ("/plug-off.png", OffloadWebIconPlugOff);
      Webserver->on ("/plug-on.png", OffloadWebIconPlugOn);
      Webserver->on ("/plug-offload.png", OffloadWebIconPlugOffload);
      Webserver->on ("/plug.png", OffloadWebIconPlug);
      Webserver->on ("/power-off.png", OffloadWebIconPowerOff);
      Webserver->on ("/power-on.png", OffloadWebIconPowerOn);
      Webserver->on ("/power.png", OffloadWebIconPower);
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      if (offload_flag.relay_managed) WSContentSend_P (OFFLOAD_BUTTON, D_PAGE_OFFLOAD_CONTROL, D_OFFLOAD_CONTROL);
      WSContentSend_P (OFFLOAD_BUTTON, D_PAGE_OFFLOAD_HISTORY, D_OFFLOAD " " D_OFFLOAD_HISTORY);
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

#endif // USE_OFFLOADING
