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

#define OFFLOAD_BUFFER_SIZE     128

#define OFFLOAD_PHASE_MAX       3

#define OFFLOAD_MESSAGE_LEFT    5
#define OFFLOAD_MESSAGE_DELAY   5

#define D_PAGE_OFFLOAD_CONFIG   "offload"
#define D_PAGE_OFFLOAD_CONTROL  "control"

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
#define MQTT_OFFLOAD_KEY_INST   "SINSTS"
#define MQTT_OFFLOAD_KEY_MAX    "SSOUSC"

#define D_OFFLOAD               "Offload"
#define D_OFFLOAD_CONFIGURE     "Configure"
#define D_OFFLOAD_CONTROL       "Control"
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

// switching off/on priorities 
enum OffloadPriorities { OFFLOAD_PRIORITY_APPLIANCE, OFFLOAD_PRIORITY_FRIDGE, OFFLOAD_PRIORITY_WASHING, OFFLOAD_PRIORITY_DISH, OFFLOAD_PRIORITY_DRIER, OFFLOAD_PRIORITY_WATER, OFFLOAD_PRIORITY_IRON, OFFLOAD_PRIORITY_BATHROOM, OFFLOAD_PRIORITY_LIVING, OFFLOAD_PRIORITY_ROOM, OFFLOAD_PRIORITY_KITCHEN, OFFLOAD_PRIORITY_MAX };
const char offload_priority_appliance[] PROGMEM = "Misc appliance";
const char offload_priority_fridge[]    PROGMEM = "Fridge";
const char offload_priority_washing[]   PROGMEM = "Washing machine";
const char offload_priority_dish[]      PROGMEM = "Dish washer";
const char offload_priority_drier[]     PROGMEM = "Drier";
const char offload_priority_water[]     PROGMEM = "Water heater";
const char offload_priority_iron[]      PROGMEM = "Iron";
const char offload_priority_bathroom[]  PROGMEM = "Bathroom heater";
const char offload_priority_living[]    PROGMEM = "Living room heater";
const char offload_priority_room[]      PROGMEM = "Sleeping room heater";
const char offload_priority_kitchen[]   PROGMEM = "Kitchen heater";
const char* const arr_offload_priority_label[] PROGMEM = { offload_priority_appliance, offload_priority_fridge, offload_priority_washing, offload_priority_dish, offload_priority_drier, offload_priority_water, offload_priority_iron, offload_priority_bathroom, offload_priority_living, offload_priority_room, offload_priority_kitchen };
const uint8_t arr_offload_priority_before[] = { 0,  0, 8,  4,  4,  0,  4,  0,  0,  0,  0   };
const uint8_t arr_offload_priority_after[]  = { 15, 5, 20, 30, 25, 35, 10, 50, 60, 90, 120 };
const uint8_t arr_offload_priority_random[] = { 5,  5, 5 , 5,  5,  5,  5,  10, 30, 30, 30  };

/****************************************\
 *               Icons
 * 
 *      xxd -i -c 256 icon.png
\****************************************/

// icon : unplugged
unsigned char offload_unplugged_png[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x02, 0x03, 0x00, 0x00, 0x00, 0xbe, 0x50, 0x89, 0x58, 0x00, 0x00, 0x00, 0x09, 0x50, 0x4c, 0x54, 0x45, 0x00, 0x00, 0x60, 0x84, 0x97, 0x7f, 0xff, 0x00, 0x00, 0x63, 0x16, 0x8c, 0xcc, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x00, 0x01, 0x62, 0x4b, 0x47, 0x44, 0x00, 0x88, 0x05, 0x1d, 0x48, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0b, 0x13, 0x00, 0x00, 0x0b, 0x13, 0x01, 0x00, 0x9a, 0x9c, 0x18, 0x00, 0x00, 0x00, 0x07, 0x74, 0x49, 0x4d, 0x45, 0x07, 0xe4, 0x0a, 0x12, 0x10, 0x38, 0x16, 0x0e, 0xcc, 0xde, 0x5b, 0x00, 0x00, 0x01, 0xe9, 0x49, 0x44, 0x41, 0x54, 0x58, 0xc3, 0xc5, 0x97, 0x41, 0x8e, 0xc3, 0x20, 0x0c, 0x45, 0x2d, 0xaf, 0x46, 0x1c, 0x85, 0x53, 0x5a, 0x3d, 0x09, 0x4b, 0xc4, 0x29, 0x87, 0x40, 0x93, 0x18, 0xc6, 0xe6, 0x57, 0x41, 0xa3, 0x76, 0x97, 0xf2, 0xc0, 0x1f, 0x63, 0x6c, 0x43, 0xf4, 0xe5, 0x5f, 0x4c, 0xeb, 0x71, 0x2e, 0x19, 0x01, 0x05, 0x58, 0x28, 0x45, 0xb6, 0x80, 0x6a, 0x61, 0x13, 0x88, 0x9f, 0x00, 0x4b, 0x8d, 0x05, 0x00, 0xa1, 0x8e, 0xa7, 0x87, 0x00, 0xe7, 0xb5, 0x85, 0xd0, 0x47, 0x5c, 0xe0, 0x18, 0xaf, 0xa7, 0xe4, 0x5b, 0x38, 0x76, 0x57, 0x1d, 0x60, 0x03, 0xd5, 0x31, 0x6d, 0x81, 0xba, 0x84, 0x69, 0xe1, 0x08, 0x80, 0xbe, 0x40, 0x87, 0x2c, 0xf3, 0x54, 0x00, 0xf0, 0x52, 0x80, 0xd8, 0x1b, 0xf8, 0x1c, 0xc8, 0xb4, 0x0b, 0xc8, 0x03, 0x80, 0x07, 0xc0, 0x8b, 0x91, 0xd5, 0x0a, 0xda, 0x8d, 0x36, 0x30, 0x88,
  0x48, 0x04, 0x44, 0x24, 0x24, 0x22, 0x13, 0x10, 0x91, 0x09, 0x88, 0xc8, 0x84, 0x44, 0x10, 0x12, 0x41, 0x48, 0x04, 0x21, 0x11, 0x84, 0x44, 0x10, 0x12, 0x21, 0x48, 0x84, 0x20, 0x11, 0x82, 0x44, 0x08, 0x12, 0xb1, 0xc8, 0x3c, 0x1e, 0xd0, 0x72, 0x73, 0x58, 0x00, 0xed, 0x3f, 0xf6, 0x81, 0x77, 0x6e, 0x5e, 0x5e, 0x4d, 0x51, 0x22, 0xb2, 0x97, 0x9b, 0x83, 0x0b, 0xbc, 0xb7, 0xce, 0x1e, 0x70, 0xe5, 0xe6, 0xbf, 0x40, 0x2c, 0xa7, 0x93, 0x45, 0x79, 0x22, 0xe9, 0xb5, 0x45, 0xe7, 0xe6, 0x30, 0x7b, 0x9a, 0x3b, 0x7d, 0x6d, 0x9d, 0x0d, 0x20, 0xf7, 0xbf, 0xb3, 0x3e, 0x8e, 0x01, 0x28, 0x43, 0x6e, 0x8e, 0x93, 0x23, 0xbb, 0x7c, 0x55, 0x3f, 0xc2, 0xb4, 0x09, 0x6e, 0x73, 0xd5, 0x24, 0x9e, 0xaf, 0xe6, 0xc1, 0xb3, 0x9e, 0x34, 0x87, 0x4b, 0x9b, 0xac, 0x27, 0xcd, 0xe5, 0xaa, 0x7d, 0xeb, 0x2a, 0x39, 0x67, 0xf2, 0xf6, 0x1d, 0x94, 0x6e, 0x9e, 0x8e, 0x82, 0xbb, 0x88, 0xec, 0x16, 0x34, 0xee, 0x22, 0x92, 0x5b, 0x12, 0xdb, 0xbe, 0x25, 0xca, 0x18, 0x98, 0x32, 0x05, 0x72, 0x0a, 0x2a, 0xee, 0xa6, 0xb2, 0xac, 0x8f, 0x3f, 0x5c, 0x81, 0x29, 0xf3, 0x65, 0x1a, 0xe2, 0x2e, 0x1a, 0x80, 0xe8, 0xb8, 0x0b, 0x06, 0x90, 0x74, 0xdc, 0x0d, 0xed, 0x49, 0x50, 0x31, 0x78, 0xc9, 0x1f, 0x43, 0xf2, 0x16, 0x61, 0x36, 0x0d, 0x51, 0x5d, 0x77, 0xf3, 0xca, 0xc5, 0x3b, 0x39, 0xb3, 0x77, 0xa3, 0x4e, 0x11, 0x4e, 0x41, 0xbe, 0xaf, 0xb3, 0xdd, 0xb7, 0xa8, 0x9c, 0xe3, 0xa5, 0x85, 0x53, 0x84, 0x2d, 0xe1, 0x06, 0x32, 0x9b, 0x12, 0x54, 0x5e, 0x23, 0x53, 0x82, 0x4e, 0x7c, 0xd1, 0x92, 0xa0, 0x80, 0x1a, 0x98, 0x79, 0x09, 0x54, 0x11, 0x09, 0x24, 0x78, 0x54, 0x90, 0x25, 0x82, 0x0a, 0x90,
  0x7e, 0x00, 0x90, 0x9f, 0x15, 0x32, 0x54, 0xd1, 0x87, 0x4a, 0x97, 0x1e, 0xd5, 0xe3, 0x88, 0x44, 0x84, 0xed, 0xb6, 0x22, 0xa0, 0xae, 0x61, 0xbf, 0xf9, 0x19, 0x57, 0x48, 0x0f, 0x80, 0xb8, 0x0b, 0x8c, 0x16, 0x00, 0xf0, 0xb2, 0x80, 0xa1, 0x6d, 0x01, 0xc0, 0xd1, 0x35, 0xdb, 0x77, 0xf7, 0x2a, 0x74, 0x5c, 0xec, 0x67, 0x42, 0x58, 0x44, 0x64, 0xcf, 0x19, 0xd1, 0x0d, 0xa7, 0x33, 0x25, 0xf8, 0xcf, 0x8d, 0xf3, 0x04, 0xbd, 0x96, 0x65, 0xe7, 0x29, 0xf3, 0xd9, 0x63, 0x89, 0xdc, 0xdd, 0x39, 0xe5, 0xeb, 0x1f, 0x9e, 0x84, 0x0c, 0x2c, 0x60, 0x80, 0xd0, 0xbb, 0xd6, 0x0c, 0x80, 0xd1, 0x93, 0xf9, 0xdb, 0xcf, 0xff, 0x5f, 0xcb, 0x44, 0x48, 0x02, 0x3a, 0x9b, 0xfb, 0x48, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
unsigned int offload_unplugged_len = 633;

// icon : plugged
unsigned char offload_plugged_png[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x02, 0x03, 0x00, 0x00, 0x00, 0xbe, 0x50, 0x89, 0x58, 0x00, 0x00, 0x00, 0x09, 0x50, 0x4c, 0x54, 0x45, 0x00, 0x00, 0x00, 0x73, 0x6e, 0x7f, 0x00, 0x99, 0x33, 0x04, 0xd8, 0xbf, 0x0c, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x00, 0x01, 0x62, 0x4b, 0x47, 0x44, 0x00, 0x88, 0x05, 0x1d, 0x48, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x2e, 0x23, 0x00, 0x00, 0x2e, 0x23, 0x01, 0x78, 0xa5, 0x3f, 0x76, 0x00, 0x00, 0x00, 0x07, 0x74, 0x49, 0x4d, 0x45, 0x07, 0xe4, 0x0a, 0x12, 0x10, 0x38, 0x28, 0xcf, 0xad, 0xc3, 0xf0, 0x00, 0x00, 0x01, 0xa5, 0x49, 0x44, 0x41, 0x54, 0x58, 0xc3, 0xed, 0x98, 0x41, 0x8e, 0xc3, 0x20, 0x0c, 0x45, 0x11, 0xcb, 0x1c, 0x85, 0x53, 0x72, 0x14, 0x2f, 0x2d, 0x9f, 0xb2, 0x21, 0x29, 0x0d, 0x60, 0xfb, 0x3b, 0x55, 0xa5, 0x6c, 0x66, 0xd8, 0xf2, 0x82, 0x1f, 0x08, 0x8c, 0x49, 0x4a, 0xb0, 0x95, 0x8a, 0xfb, 0xb3, 0x30, 0x06, 0xb6, 0x08, 0x28, 0x22, 0x18, 0x90, 0x00, 0xc8, 0x11, 0xb0, 0x45, 0xc0, 0xae, 0xc0, 0x91, 0x02, 0x47, 0x0a, 0x14, 0x29, 0xd4, 0x40, 0x41, 0xfe, 0xb2, 0x42, 0x11, 0xac, 0x50, 0xce, 0x4f, 0x5d, 0x85, 0x36, 0xf4, 0xfe, 0xad, 0xaf, 0x70, 0xf4, 0x0b, 0xb9, 0x0a, 0xe7, 0x00, 0xc2, 0xae, 0x82, 0x5c, 0x8d, 0xbd, 0xd9, 0xf7, 0x46, 0x20, 0xc2, 0xd1, 0xaa, 0x37, 0xc7, 0xde, 0xbe, 0x57, 0xa8, 0x81, 0x42, 0xe1, 0x40, 0xa1, 0x05, 0x85, 0x0a, 0xed, 0x1b, 0x01, 0xc0, 0x1e, 0x9e, 0x46, 0x05, 0x15, 0x62,
  0xef, 0xe4, 0x2d, 0x00, 0xe6, 0x46, 0x60, 0x09, 0xee, 0x01, 0x8c, 0x56, 0xd9, 0x00, 0xca, 0xb4, 0x48, 0x1a, 0xc8, 0xcb, 0x1c, 0x15, 0xd0, 0x12, 0xde, 0x2a, 0xb1, 0xee, 0x03, 0x25, 0xb1, 0x1a, 0xd6, 0x2d, 0x00, 0x94, 0xc4, 0x1a, 0x42, 0x49, 0xe8, 0x94, 0x58, 0x00, 0xd0, 0x86, 0x5f, 0x25, 0xd4, 0x42, 0xaf, 0x12, 0x46, 0x56, 0x45, 0x80, 0x21, 0x91, 0x22, 0x09, 0x23, 0xe3, 0xe4, 0x00, 0x58, 0x24, 0x8c, 0x43, 0x39, 0x4b, 0x18, 0xdb, 0x65, 0x92, 0x60, 0x63, 0xc3, 0x4d, 0x12, 0x6c, 0xec, 0x69, 0x4e, 0x60, 0xc3, 0xc8, 0x2a, 0x41, 0xd6, 0x8e, 0x1d, 0x25, 0xc8, 0xda, 0xf3, 0xa3, 0x44, 0xb5, 0x8e, 0xd5, 0x28, 0x51, 0xcd, 0xd4, 0x34, 0x48, 0xd8, 0x87, 0x66, 0x90, 0xb0, 0x8f, 0xdd, 0x25, 0xc1, 0x36, 0x70, 0x49, 0x90, 0x73, 0xf4, 0x3f, 0xd1, 0xaa, 0x03, 0x7c, 0x24, 0xbc, 0xfc, 0xda, 0x25, 0xd8, 0x03, 0xba, 0x04, 0xf9, 0x09, 0xb8, 0x18, 0x19, 0x6a, 0x04, 0xde, 0x12, 0x7e, 0x72, 0x39, 0x25, 0xd8, 0x4f, 0x4f, 0xa7, 0x04, 0x81, 0xfc, 0xd5, 0xaf, 0x3d, 0x17, 0xa8, 0x46, 0x6d, 0x33, 0xe7, 0xc7, 0x1c, 0x8c, 0x70, 0x48, 0x20, 0x07, 0xd1, 0xe5, 0xd1, 0x72, 0xec, 0xb5, 0xc4, 0x02, 0x18, 0x12, 0x12, 0x49, 0x88, 0x96, 0x80, 0x40, 0x93, 0x80, 0x40, 0x93, 0x80, 0x40, 0x93, 0x80, 0x40, 0x93, 0xc0, 0xc0, 0x2e, 0x51, 0xe1, 0x65, 0x43, 0x89, 0xbf, 0xba, 0x8d, 0x34, 0x20, 0xa8, 0xac, 0xb8, 0x07, 0xc0, 0x5b, 0x97, 0x4a, 0x00, 0xb4, 0x85, 0x46, 0x95, 0xc9, 0x51, 0x84, 0x05, 0xa5, 0x8b, 0x59, 0x3b, 0x8c, 0xc9, 0xad, 0xf0, 0xc3, 0xd5, 0xd3, 0x63, 0x05, 0x1c, 0x17, 0x10, 0x21, 0xf7, 0x12, 0xd5, 0x1b, 0xe0, 0x2c, 0x5f, 0xfd, 0x01, 0xde, 0x15, 0x74,
  0xf6, 0x4b, 0xf5, 0xde, 0xe3, 0x55, 0xea, 0xff, 0x4f, 0x99, 0x87, 0x14, 0xee, 0x3c, 0x3a, 0x29, 0x02, 0xea, 0x4f, 0x2f, 0xe7, 0x24, 0xf1, 0xe3, 0x9c, 0x22, 0x20, 0xfa, 0x41, 0x10, 0x44, 0x78, 0x01, 0x52, 0x14, 0xf0, 0x94, 0x77, 0xac, 0xa8, 0x59, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
unsigned int offload_plugged_len = 565;

// icon : offloaded
unsigned char offload_offloaded_png[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x02, 0x03, 0x00, 0x00, 0x00, 0xbe, 0x50, 0x89, 0x58, 0x00, 0x00, 0x00, 0x09, 0x50, 0x4c, 0x54, 0x45, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0xff, 0x99, 0x00, 0xf9, 0x06, 0xd6, 0xa7, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x00, 0x01, 0x62, 0x4b, 0x47, 0x44, 0x00, 0x88, 0x05, 0x1d, 0x48, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0d, 0xd7, 0x00, 0x00, 0x0d, 0xd7, 0x01, 0x42, 0x28, 0x9b, 0x78, 0x00, 0x00, 0x00, 0x07, 0x74, 0x49, 0x4d, 0x45, 0x07, 0xe4, 0x0a, 0x12, 0x10, 0x31, 0x01, 0x5c, 0xdd, 0xe0, 0xd5, 0x00, 0x00, 0x01, 0xe0, 0x49, 0x44, 0x41, 0x54, 0x58, 0xc3, 0xb5, 0x97, 0x4b, 0x72, 0x85, 0x20, 0x10, 0x45, 0x29, 0x46, 0x29, 0x56, 0x91, 0x31, 0xab, 0x74, 0x29, 0x0e, 0xa9, 0x5e, 0x65, 0x94, 0xbf, 0xd0, 0xf7, 0x76, 0x5e, 0xac, 0x38, 0x53, 0x8f, 0x70, 0xf8, 0x74, 0xdb, 0x38, 0xf7, 0x4f, 0xd7, 0xb7, 0x05, 0x9c, 0xc6, 0x7b, 0x6f, 0x02, 0x87, 0x01, 0x04, 0x0b, 0x88, 0x26, 0x60, 0x0d, 0x42, 0x2c, 0xc7, 0xd7, 0x40, 0x48, 0x96, 0xa3, 0x05, 0xc8, 0xa7, 0xc0, 0x3a, 0x6f, 0x5e, 0x96, 0xa5, 0x88, 0xcb, 0x7d, 0x58, 0x81, 0xad, 0xc5, 0x05, 0xf0, 0xb2, 0x0c, 0x3b, 0xca, 0xb1, 0x02, 0xcf, 0x07, 0xeb, 0xfd, 0xf5, 0xe0, 0xe4, 0x40, 0x7c, 0x4a, 0x6c, 0x2d, 0x5e, 0x52, 0x42, 0x6e, 0xf7, 0x4f, 0xe2, 0x06, 0x2c, 0x12, 0x0a, 0xf0, 0x94, 0x90, 0x1d, 0x78, 0xf4, 0x7a, 0x75, 0x98, 0x1c, 0x93, 0x08, 0x0a, 0xf0, 0x90, 0x50, 0x81,
  0x59, 0x22, 0x6a, 0xc0, 0x2c, 0x11, 0xd7, 0x89, 0x5d, 0x25, 0x44, 0x03, 0x26, 0x09, 0xaf, 0x03, 0x43, 0xe2, 0xea, 0x4d, 0x94, 0xc0, 0x1b, 0x12, 0x00, 0x18, 0x12, 0x51, 0x07, 0x86, 0x84, 0x00, 0xa0, 0x49, 0xdc, 0x8e, 0x6a, 0xe0, 0x35, 0x09, 0x08, 0x34, 0x89, 0x80, 0x80, 0x26, 0x81, 0x81, 0x2a, 0x71, 0xbf, 0xd7, 0x23, 0xb3, 0x4a, 0x60, 0xa0, 0x48, 0x78, 0x0c, 0x14, 0x89, 0x0c, 0x80, 0x2c, 0x9a, 0x25, 0x02, 0x01, 0xb2, 0x04, 0x03, 0xb2, 0x04, 0xeb, 0xa2, 0x48, 0xa0, 0xa5, 0xe8, 0x12, 0x91, 0x00, 0x5d, 0xe2, 0x70, 0x86, 0x04, 0xcc, 0xe4, 0x4d, 0x02, 0xe7, 0xcf, 0x2a, 0x81, 0x33, 0x70, 0x95, 0x20, 0x39, 0xbc, 0x48, 0x90, 0x0c, 0x9b, 0x25, 0x48, 0x1e, 0xee, 0x33, 0x81, 0xa6, 0x53, 0xfa, 0x72, 0x60, 0xe0, 0x2c, 0xcb, 0xa1, 0x4f, 0x46, 0x0d, 0xcb, 0x02, 0x80, 0x61, 0x76, 0x89, 0xa4, 0x3b, 0x0a, 0xdf, 0x13, 0xb1, 0xf4, 0xed, 0x99, 0x23, 0xdf, 0x13, 0x22, 0x43, 0x02, 0x39, 0xe6, 0x6f, 0x03, 0x70, 0xec, 0x80, 0x67, 0x8e, 0x55, 0x82, 0x01, 0x69, 0xff, 0x27, 0xcd, 0x8e, 0x55, 0xc2, 0x71, 0xc0, 0xd3, 0x2d, 0x71, 0xf7, 0x9f, 0x28, 0xc0, 0x6b, 0x83, 0x5f, 0x01, 0x07, 0xad, 0x3d, 0x48, 0xf0, 0x76, 0x40, 0xde, 0x03, 0x44, 0x22, 0x08, 0xc9, 0x62, 0x1f, 0x00, 0xc2, 0x62, 0xcb, 0x90, 0x18, 0x4b, 0xfe, 0x57, 0x20, 0xbf, 0x66, 0x29, 0x22, 0x03, 0x9e, 0xa6, 0xa9, 0xbb, 0x7d, 0xd2, 0x47, 0x59, 0xab, 0x08, 0x01, 0xdf, 0x03, 0x83, 0x01, 0x0e, 0x06, 0x46, 0x01, 0x92, 0x83, 0x81, 0x31, 0x00, 0x28, 0x11, 0xea, 0x7e, 0x82, 0x12, 0x2d, 0x95, 0x43, 0x89, 0x1e, 0xf8, 0x04, 0x48, 0x5a, 0x79, 0xb7, 0x03, 0x01, 0xcc, 0x76, 0x2f, 0x70, 0x3c, 0x4e,
  0x42, 0xa7, 0x63, 0x12, 0x32, 0x15, 0x22, 0x09, 0x26, 0x6b, 0x26, 0x31, 0xd6, 0xd9, 0xa3, 0x52, 0xe7, 0x44, 0x85, 0xee, 0x06, 0xa8, 0xff, 0xae, 0xb9, 0xdd, 0x00, 0x00, 0x52, 0x1b, 0xe7, 0xaf, 0x12, 0xae, 0xb6, 0xb7, 0xa2, 0x5e, 0x93, 0x88, 0x33, 0x10, 0x54, 0xe0, 0x70, 0x54, 0x22, 0x8a, 0x3e, 0x6b, 0xe0, 0xf0, 0xa3, 0x55, 0xe8, 0x89, 0xd7, 0xf8, 0xee, 0x39, 0x30, 0x45, 0xc2, 0x3a, 0x88, 0x38, 0x61, 0xce, 0xca, 0x11, 0x6f, 0x3b, 0xd1, 0xf9, 0x64, 0x9c, 0x7c, 0xcd, 0xa3, 0xb0, 0x79, 0xd2, 0x0d, 0xee, 0x2d, 0xf0, 0xf5, 0xfa, 0xc0, 0xaf, 0x5e, 0x3f, 0x9e, 0xb5, 0xbd, 0xe6, 0x3b, 0xb0, 0xbd, 0x72, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
unsigned int offload_offloaded_len = 624;

/*************************************************\
 *               Variables
\*************************************************/

// offloading stages
enum OffloadStages { OFFLOAD_NONE, OFFLOAD_BEFORE, OFFLOAD_ACTIVE, OFFLOAD_AFTER };

// variables
bool     offload_relay_managed    = true;               // flag to define if relay is managed directly
bool     offload_topic_subscribed = false;              // flag for power subscription
bool     offload_just_set         = false;              // flag to signal that offload has just been set
bool     offload_just_removed     = false;              // flag to signal that offload has just been removed
int      offload_power_inst       = 0;                  // actual phase instant power (retrieved thru MQTT)
uint8_t  offload_relay_state      = 0;                  // relay state before offloading
uint8_t  offload_stage            = OFFLOAD_NONE;       // current offloading state
uint8_t  offload_message_left     = 0;                  // number of JSON messages to send
uint8_t  offload_message_delay    = 0;                  // delay before next JSON message (seconds)
uint32_t offload_message_time     = 0;                  // time of last message
ulong    offload_stage_time       = 0;                  // time of current stage
ulong    offload_removal_delay    = 0;                  // delay before removing offload (seconds)

// list of available choices in the configuration page
uint8_t arr_offload_priority_available[OFFLOAD_PRIORITY_MAX] = { OFFLOAD_PRIORITY_APPLIANCE, OFFLOAD_PRIORITY_FRIDGE, OFFLOAD_PRIORITY_WASHING, OFFLOAD_PRIORITY_DISH, OFFLOAD_PRIORITY_DRIER, OFFLOAD_PRIORITY_WATER, OFFLOAD_PRIORITY_IRON, OFFLOAD_PRIORITY_BATHROOM, OFFLOAD_PRIORITY_LIVING, OFFLOAD_PRIORITY_ROOM, OFFLOAD_PRIORITY_KITCHEN };

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
  for (index = 0; index < OFFLOAD_PRIORITY_MAX; index ++) if (device_type == arr_offload_priority_available[index]) is_available = true;

  // if not available or not in range
  if (!is_available || (device_type >= OFFLOAD_PRIORITY_MAX))
  {
    // set to first of available devices list
    device_type = arr_offload_priority_available[0];
    OffloadSetDeviceType (device_type);
  } 

  return device_type;
}

// set device type
void OffloadSetDeviceType (uint16_t new_type)
{
  if ( new_type < OFFLOAD_PRIORITY_MAX)
  {
    // save appliance type
    Settings.knx_GA_addr[7] = new_type;

    // save associated delays
    OffloadSetDelayBeforeOffload (arr_offload_priority_before[new_type]);
    OffloadSetDelayBeforeRemoval (arr_offload_priority_after[new_type]);
    OffloadSetDelayRandom (arr_offload_priority_random[new_type]);
  }
}

// get instant power MQTT topic
String OffloadGetPowerTopic (bool get_default)
{
  String str_result;

  // get value or default value if not set
  str_result = SettingsText (SET_OFFLOAD_TOPIC);
  if (get_default == true || str_result == "") str_result = MQTT_OFFLOAD_TOPIC;

  return str_result;
}

// set power MQTT topic
void OffloadSetPowerTopic (char* str_topic)
{
  SettingsUpdateText (SET_OFFLOAD_TOPIC, str_topic);
}

// get contract power JSON key
String OffloadGetContractPowerKey (bool get_default)
{
  String str_result;

  // get value or default value if not set
  str_result = SettingsText (SET_OFFLOAD_KEY_MAX);
  if (get_default == true || str_result == "") str_result = MQTT_OFFLOAD_KEY_MAX;

  return str_result;
}

// set contract power JSON key
void OffloadSetContractPowerKey (char* str_key)
{
  SettingsUpdateText (SET_OFFLOAD_KEY_MAX, str_key);
}

// get instant power JSON key
String OffloadGetInstPowerKey (bool get_default)
{
  String str_result;

  // get value or default value if not set
  str_result = SettingsText (SET_OFFLOAD_KEY_INST);
  if (get_default == true || str_result == "") str_result = MQTT_OFFLOAD_KEY_INST + String (OffloadGetPhase ());

  return str_result;
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
  if ((index < OFFLOAD_PRIORITY_MAX) && (device_type <= OFFLOAD_PRIORITY_MAX)) arr_offload_priority_available[index] = device_type;
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
  return (offload_stage >= OFFLOAD_ACTIVE);
}

// get offload newly set state
bool OffloadJustSet ()
{
  bool result;

  // get the flag and reset it
  result = offload_just_set;
  offload_just_set = false;

  return (result);
}

// get offload newly removed state
bool OffloadJustRemoved ()
{
  bool result;

  // get the flag and reset it
  result = offload_just_removed;
  offload_just_removed = false;

  return (result);
}

// set relay managed mode
void OffloadSetRelayMode (bool is_managed)
{
  offload_relay_managed = is_managed;
}

// activate offload state
bool OffloadActivate ()
{
  // set flag to signal offload has just been set
  offload_just_set     = true;
  offload_just_removed = false;

  // read relay state and switch off if needed
  if (offload_relay_managed == true)
  {
    // save relay state
    offload_relay_state = bitRead (power, 0);

    // if relay is ON, switch off
    if (offload_relay_state == 1) ExecuteCommandPower (1, POWER_OFF, SRC_MAX);
  }

  // get relay state and log
  AddLog_P2(LOG_LEVEL_INFO, PSTR("PWR: Offload starts (relay = %d)"), offload_relay_state);
}

// remove offload state
void OffloadRemove ()
{
  // set flag to signal offload has just been removed
  offload_just_removed = true;
  offload_just_set     = false;

  // switch back relay ON if needed
  if (offload_relay_managed == true)
  {
    // if relay was ON, switch it back
    if (offload_relay_state == 1) ExecuteCommandPower (1, POWER_ON, SRC_MAX);
  }

  // log offloading removal
  AddLog_P2(LOG_LEVEL_INFO, PSTR("PWR: Offload stops (relay = %d)"), offload_relay_state);
}

// Show JSON status (for MQTT)
void OffloadShowJSON (bool append)
{
  bool     is_offloaded;
  uint16_t power_max, power_device, power_contract, delay_before, delay_after;
  int      adjust_contract;
  String   str_mqtt, str_json, str_status;

  // read data
  is_offloaded    = OffloadIsOffloaded ();
  delay_before    = OffloadGetDelayBeforeOffload ();
  delay_after     = OffloadGetDelayBeforeRemoval ();
  power_device    = OffloadGetDevicePower ();
  power_max       = OffloadGetMaxPower ();
  power_contract  = OffloadGetContractPower ();
  adjust_contract = OffloadGetContractAdjustment ();

  // "Offload":{
  //   "State":"OFF","Stage":1,
  //   "Phase":2,"Before":1,"After":5,"Device":1000,"Contract":5000,"Topic":"...","KeyInst":"...","KeyMax":"..."  //           
  //   }
  str_json = "\"" + String (D_JSON_OFFLOAD) + "\":{";
  
  // dynamic data
  if (is_offloaded == true) str_status = MQTT_STATUS_ON;
  else str_status = MQTT_STATUS_OFF;
  str_json += "\"" + String (D_JSON_OFFLOAD_STATE) + "\":\"" + str_status + "\"";
  str_json += ",\"" + String (D_JSON_OFFLOAD_STAGE) + "\":" + String (offload_stage);
  
  // static data
  if (append == true) 
  {
    str_json += ",\"" + String (D_JSON_OFFLOAD_PHASE) + "\":" + OffloadGetPhase ();
    str_json += ",\"" + String (D_JSON_OFFLOAD_BEFORE) + "\":" + String (delay_before);
    str_json += ",\"" + String (D_JSON_OFFLOAD_AFTER) + "\":" + String (delay_after);
    str_json += ",\"" + String (D_JSON_OFFLOAD_DEVICE) + "\":" + String (power_device);
    str_json += ",\"" + String (D_JSON_OFFLOAD_MAX) + "\":" + String (power_max);
    str_json += ",\"" + String (D_JSON_OFFLOAD_CONTRACT) + "\":" + String (power_contract);
    str_json += ",\"" + String (D_JSON_OFFLOAD_ADJUST) + "\":" + String (adjust_contract);
    str_json += ",\"" + String (D_JSON_OFFLOAD_TOPIC) + "\":\"" + OffloadGetPowerTopic (false) + "\"";
    str_json += ",\"" + String (D_JSON_OFFLOAD_KEY_INST) + "\":\"" + OffloadGetInstPowerKey (false) + "\"";
    str_json += ",\"" + String (D_JSON_OFFLOAD_KEY_MAX) + "\":\"" + OffloadGetContractPowerKey (false) + "\"";
  }

  str_json += "}";

  // generate MQTT message according to append mode
  if (append == true) str_mqtt = String (mqtt_data) + "," + str_json;
  else str_mqtt = "{" + str_json + "}";

  // place JSON back to MQTT data and publish it if not in append mode
  snprintf_P (mqtt_data, sizeof(mqtt_data), str_mqtt.c_str ());
  if (append == false) MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));
}

// check and update MQTT power subsciption after disconnexion
void OffloadCheckConnexion ()
{
  bool   is_connected;
  String str_topic;

  // check MQTT connexion
  is_connected = MqttIsConnected();
  if (is_connected)
  {
    // if still no subsciption to power topic
    if (offload_topic_subscribed == false)
    {
      // check power topic availability
      str_topic = OffloadGetPowerTopic (false);
      if (str_topic.length () > 0) 
      {
        // subscribe to power meter
        MqttSubscribe(str_topic.c_str ());

        // subscription done
        offload_topic_subscribed = true;

        // log
        AddLog_P2(LOG_LEVEL_INFO, PSTR("MQT: Subscribed to %s"), str_topic.c_str ());
      }
    }
  }
  else offload_topic_subscribed = false;

  // check if offload JSON messages need to be sent
  if (offload_message_left > 0)
  {
    // decrement delay
    offload_message_delay--;

    // if delay is reached
    if (offload_message_delay == 0)
    {
      // send MQTT message
      OffloadShowJSON (false);

      // update counters
      offload_message_left--;
      if (offload_message_left > 0) offload_message_delay = OFFLOAD_MESSAGE_DELAY;
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
  int     idx_value, mqtt_power;
  String  str_data, str_topic, str_key;

  // if topic is the instant house power
  str_topic = OffloadGetPowerTopic (false);
  if (str_topic == XdrvMailbox.topic)
  {
    // log and counter increment
    AddLog_P2(LOG_LEVEL_INFO, PSTR("MQT: Received %s"), str_topic.c_str ());
    offload_message_time = LocalTime ();

    // get message data (removing SPACE and QUOTE)
    str_data  = XdrvMailbox.data;
    str_data.replace (" ", "");
    str_data.replace ("\"", "");

    // if instant power key is present
    str_key = OffloadGetInstPowerKey (false) + ":";
    idx_value = str_data.indexOf (str_key);
    if (idx_value >= 0) idx_value = str_data.indexOf (':', idx_value + 1);
    if (idx_value >= 0)
    {
      offload_power_inst = str_data.substring (idx_value + 1).toInt ();
      data_handled = true;
    }

    // if max power key is present
    str_key = OffloadGetContractPowerKey (false) + ":";
    idx_value = str_data.indexOf (str_key);
    if (idx_value >= 0) idx_value = str_data.indexOf (':', idx_value + 1);
    if (idx_value >= 0)
    {
      mqtt_power = str_data.substring (idx_value + 1).toInt ();
      if (mqtt_power > 0) OffloadSetContractPower ((uint16_t)mqtt_power);
      data_handled = true;
    }
  }

  return data_handled;
}

// update offloading status according to all parameters
void OffloadUpdateStatus ()
{
  uint8_t  prev_stage, next_stage;
  uint16_t power_mesured, power_device, power_max;
  ulong    time_end, time_now;

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
    prev_stage = offload_stage;
    next_stage = offload_stage;
  
    // switch according to current state
    switch (offload_stage)
    { 
      // actually not offloaded
      case OFFLOAD_NONE:
        // save relay state
        offload_relay_state = bitRead (power, 0);

        // if overload is detected
        if (offload_power_inst > power_max)
        { 
          // set time for effective offloading calculation
          offload_stage_time = time_now;

          // next state is before offloading
          next_stage = OFFLOAD_BEFORE;
        }
        break;

      // pending offloading
      case OFFLOAD_BEFORE:
        // save relay state
        offload_relay_state = bitRead (power, 0);

        // if house power has gone down, remove pending offloading
        if (offload_power_inst <= power_max) next_stage = OFFLOAD_NONE;

        // else if delay is reached, set active offloading
        else
        {
          time_end = offload_stage_time + (ulong) OffloadGetDelayBeforeOffload ();
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
        if (power_max > power_device) power_max -= power_device;
        else power_max = 0;

        // if instant power is under this value, prepare to remove offload
        if (offload_power_inst <= power_max)
        {
          // set time for removing offloading calculation
          offload_stage_time = time_now;

          // set stage to after offloading
          next_stage = OFFLOAD_AFTER;
        }
        break;

      // actually just after offloading should stop
      case OFFLOAD_AFTER:
        // calculate maximum power allowed when substracting device power
        if (power_max > power_device) power_max -= power_device;
        else power_max = 0;

        // if house power has gone again too high, offloading back again
        if (offload_power_inst > power_max) next_stage = OFFLOAD_ACTIVE;
        
        // else if delay is reached, set active offloading
        else
        {
          time_end = offload_stage_time + offload_removal_delay;
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
    offload_stage = next_stage;

    // if state has changed,
    if (next_stage != prev_stage)
    {
      // send MQTT status
      OffloadShowJSON (false);

      // set counters of JSON message update
      offload_message_left  = OFFLOAD_MESSAGE_LEFT;
      offload_message_delay = OFFLOAD_MESSAGE_DELAY;
    } 
  }
}

// offload initialisation
void OffloadInit ()
{
  // calculate offload removal delay including randomisation
  offload_removal_delay = (ulong) OffloadGetDelayBeforeRemoval ();
  offload_removal_delay += (ulong) random (0, OffloadGetDelayRandom ());
}


/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// display offload icons
void OffloadWebIconUnplugged () { Webserver->send (200, "image/png", offload_unplugged_png, offload_unplugged_len); }
void OffloadWebIconPlugged   () { Webserver->send (200, "image/png", offload_plugged_png,   offload_plugged_len);   }
void OffloadWebIconOffloaded () { Webserver->send (200, "image/png", offload_offloaded_png, offload_offloaded_len); }

// append offload control button to main page
void OffloadWebControlButton ()
{
  // control page button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_PAGE_OFFLOAD_CONTROL, D_OFFLOAD_CONTROL);
}

// append offload configuration button
void OffloadWebConfigButton ()
{
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s %s</button></form></p>"), D_PAGE_OFFLOAD_CONFIG, D_OFFLOAD_CONFIGURE, D_OFFLOAD);
}

// append offloading state to main page
bool OffloadWebSensor ()
{
  uint16_t index, device_power;
  ulong    time_now, time_left, time_end;
  String   str_title, str_text;
  uint32_t message_delay;
  TIME_T   message_dst;

  // display device type
  index = OffloadGetDeviceType ();
  WSContentSend_PD (PSTR("{s}%s{m}%s{e}"), D_OFFLOAD_DEVICE, arr_offload_priority_label[index]);

  // display appliance type
  time_left = (ulong) OffloadGetDelayBeforeOffload ();
  WSContentSend_PD (PSTR("{s}%s <small>(Off/On)</small>{m}%d / %d %s{e}"), D_OFFLOAD_DELAY, time_left, offload_removal_delay, D_OFFLOAD_UNIT_SEC);

  // device power
  index = OffloadGetPhase ();
  device_power = OffloadGetDevicePower ();
  WSContentSend_PD (PSTR("{s}%s <small><i>[phase %d]</i></small>{m}%d W{e}"), D_OFFLOAD_POWER, index, device_power);

  // if house power is subscribed, display power
  if (offload_topic_subscribed == true)
  {
    // calculate delay since last power message
    str_text = "...";
    if (offload_message_time > 0)
    {
      // calculate delay
      message_delay = LocalTime() - offload_message_time;
      BreakTime (message_delay, message_dst);

      // generate readable format
      str_text = "";
      if (message_dst.hour > 0) str_text += String (message_dst.hour) + "h";
      if (message_dst.hour > 0 || message_dst.minute > 0) str_text += String (message_dst.minute) + "m";
      str_text += String (message_dst.second) + "s";
    }

    // display current power and max power limit
    device_power = OffloadGetMaxPower ();
    if (device_power > 0) WSContentSend_PD (PSTR("{s}%s <small><i>[%s]</i></small>{m}<b>%d</b> / %d W{e}"), D_OFFLOAD_CONTRACT, str_text.c_str (), offload_power_inst, device_power);

    // switch according to current state
    time_now  = millis () / 1000;
    time_left = 0;
    str_text  = "";
    str_title = D_OFFLOAD;
    
    switch (offload_stage)
    { 
      // calculate number of ms left before offloading
      case OFFLOAD_BEFORE:
        time_end = offload_stage_time + (ulong) OffloadGetDelayBeforeOffload ();
        if (time_end > time_now) time_left = time_end - time_now;
        str_text = "<span style='color:orange;'>Starting in <b>" + String (time_left) + " sec.</b></span>";
        break;

      // calculate number of ms left before offload removal
      case OFFLOAD_AFTER:
        time_end = offload_stage_time + offload_removal_delay;
        if (time_end > time_now) time_left = time_end - time_now;
        str_text = "<span style='color:red;'>Ending in <b>" + String (time_left) + " sec.</b></span>";
        break;

      // device is offloaded
      case OFFLOAD_ACTIVE:
        str_text = "<span style='color:red;'><b>Active</b></span>";
        break;
    }
    
    // display current state
    if (str_text.length () > 0) WSContentSend_PD (PSTR("{s}%s{m}%s{e}"), str_title.c_str (), str_text.c_str ());
  }
}

// Offload configuration web page
void OffloadWebPageConfig ()
{
  int      index, value, result, device;
  uint16_t power;
  char     argument[OFFLOAD_BUFFER_SIZE];
  String   str_text, str_default;

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg("save"))
  {
    // set power of heater according to 'power' parameter
    WebGetArg (D_CMND_OFFLOAD_DEVICE, argument, OFFLOAD_BUFFER_SIZE);
    if (strlen(argument) > 0) OffloadSetDevicePower ((uint16_t)atoi (argument));

    // set contract power limit according to 'contract' parameter
    WebGetArg (D_CMND_OFFLOAD_TYPE, argument, OFFLOAD_BUFFER_SIZE);
    if (strlen(argument) > 0) OffloadSetDeviceType ((uint16_t)atoi (argument));

    // set contract power limit according to 'contract' parameter
    WebGetArg (D_CMND_OFFLOAD_CONTRACT, argument, OFFLOAD_BUFFER_SIZE);
    if (strlen(argument) > 0) OffloadSetContractPower ((uint16_t)atoi (argument));

    // set contract power limit according to 'adjust' parameter
    WebGetArg (D_CMND_OFFLOAD_ADJUST, argument, OFFLOAD_BUFFER_SIZE);
    if (strlen(argument) > 0) OffloadSetContractAdjustment (atoi (argument));

    // set phase number according to 'phase' parameter
    WebGetArg (D_CMND_OFFLOAD_PHASE, argument, OFFLOAD_BUFFER_SIZE);
    if (strlen(argument) > 0) OffloadSetPhase ((uint16_t)atoi (argument));

    // set delay in sec. before offloading device according to 'before' parameter
    WebGetArg (D_CMND_OFFLOAD_BEFORE, argument, OFFLOAD_BUFFER_SIZE);
    if (strlen(argument) > 0) OffloadSetDelayBeforeOffload ((uint16_t)atoi (argument));

    // set delay in sec. after offloading device according to 'after' parameter
    WebGetArg (D_CMND_OFFLOAD_AFTER, argument, OFFLOAD_BUFFER_SIZE);
    if (strlen(argument) > 0) OffloadSetDelayBeforeRemoval ((uint16_t)atoi (argument));

    // set random delay in sec. according to 'random' parameter
    WebGetArg (D_CMND_OFFLOAD_RANDOM, argument, OFFLOAD_BUFFER_SIZE);
    if (strlen(argument) > 0) OffloadSetDelayRandom ((uint16_t)atoi (argument));

    // set MQTT topic according to 'topic' parameter
    WebGetArg (D_CMND_OFFLOAD_TOPIC, argument, OFFLOAD_BUFFER_SIZE);
    if (strlen(argument) > 0) OffloadSetPowerTopic (argument);

    // set JSON key according to 'inst' parameter
    WebGetArg (D_CMND_OFFLOAD_KEY_INST, argument, OFFLOAD_BUFFER_SIZE);
    if (strlen(argument) > 0) OffloadSetInstPowerKey (argument);

    // set JSON key according to 'max' parameter
    WebGetArg (D_CMND_OFFLOAD_KEY_MAX, argument, OFFLOAD_BUFFER_SIZE);
    if (strlen(argument) > 0) OffloadSetContractPowerKey (argument);

    // restart device
    WebRestart (1);
  }

  // beginning of form
  WSContentStart_P (D_OFFLOAD_CONFIGURE);
  WSContentSendStyle ();

  // specific style
  WSContentSend_P (PSTR("<style>\n"));
  WSContentSend_P (PSTR("p.half {display:inline-block;width:47%%;margin-right:2%%;}\n"));
  WSContentSend_P (PSTR("p.third {display:inline-block;width:30%%;margin-right:2%%;}\n"));
  WSContentSend_P (PSTR("span.key {float:right;font-size:0.7rem;}\n"));
  WSContentSend_P (PSTR("input.switch {background:#aaa;}\n"));
  WSContentSend_P (PSTR("</style>\n"));

  WSContentSend_P (PSTR("<form method='get' action='%s'>\n"), D_PAGE_OFFLOAD_CONFIG);

  // --------------
  //     Device  
  // --------------

  WSContentSend_P (OFFLOAD_FIELDSET_START, D_OFFLOAD_DEVICE);

  // appliance type
  result = (int) OffloadGetDeviceType ();
  WSContentSend_P (PSTR("<p>%s<span class='key'>%s</span>\n"), D_OFFLOAD_TYPE, D_CMND_OFFLOAD_TYPE);
  WSContentSend_P (PSTR("<select name='%s' id='%s'>\n"), D_CMND_OFFLOAD_TYPE, D_CMND_OFFLOAD_TYPE);
  for (index = 0; index < OFFLOAD_PRIORITY_MAX; index ++)
  {
    device = arr_offload_priority_available[index];
    if (device < OFFLOAD_PRIORITY_MAX)
    {
      if (device == result) str_text = "selected"; else str_text = "";
      WSContentSend_P (PSTR("<option value='%d' %s>%s</option>\n"), device, str_text.c_str (), arr_offload_priority_label[device]);
    }
  }
  WSContentSend_P (PSTR("</select>\n"));
  WSContentSend_P (PSTR("</p>\n"));

  // device power
  power = OffloadGetDevicePower ();
  str_text = D_OFFLOAD_POWER + String (" (") + D_OFFLOAD_UNIT_W + String (")");
  WSContentSend_P (OFFLOAD_INPUT_NUMBER, "none", str_text.c_str (), D_CMND_OFFLOAD_DEVICE, "none", D_CMND_OFFLOAD_DEVICE, D_CMND_OFFLOAD_DEVICE, 0, 65000, 1,  power);

  // delays
  value = (int) OffloadGetDelayBeforeOffload ();
  str_text = D_OFFLOAD_BEFORE + String (" (sec)");
  WSContentSend_P (OFFLOAD_INPUT_NUMBER, "third", str_text.c_str (), D_CMND_OFFLOAD_BEFORE, "switch", D_CMND_OFFLOAD_BEFORE, D_CMND_OFFLOAD_BEFORE, 0, 60, 1, value);
  value = (int) OffloadGetDelayBeforeRemoval ();
  str_text = D_OFFLOAD_AFTER + String (" (sec)");
  WSContentSend_P (OFFLOAD_INPUT_NUMBER, "third", str_text.c_str (), D_CMND_OFFLOAD_AFTER, "switch", D_CMND_OFFLOAD_AFTER, D_CMND_OFFLOAD_AFTER, 0, 600, 1, value);
  value = (int) OffloadGetDelayRandom ();
  str_text = D_OFFLOAD_RANDOM + String (" (sec)");
  WSContentSend_P (OFFLOAD_INPUT_NUMBER, "third", str_text.c_str (), D_CMND_OFFLOAD_RANDOM, "switch", D_CMND_OFFLOAD_RANDOM, D_CMND_OFFLOAD_RANDOM, 0, 60, 1, value);
  
  WSContentSend_P (OFFLOAD_FIELDSET_STOP);

  // --------------
  //    Contract  
  // --------------

  WSContentSend_P (OFFLOAD_FIELDSET_START, D_OFFLOAD_CONTRACT);

  // contract power
  power = OffloadGetContractPower ();
  WSContentSend_P (OFFLOAD_INPUT_NUMBER, "none", D_OFFLOAD_CONTRACT, D_CMND_OFFLOAD_CONTRACT, "none", D_CMND_OFFLOAD_CONTRACT, D_CMND_OFFLOAD_CONTRACT, 0, 65000, 1, power);

  // contract adjustment
  value = OffloadGetContractAdjustment ();
  WSContentSend_P (OFFLOAD_INPUT_NUMBER, "half", D_OFFLOAD_ADJUST, D_CMND_OFFLOAD_ADJUST, "none", D_CMND_OFFLOAD_ADJUST, D_CMND_OFFLOAD_ADJUST, -99, 100, 1, value);

  // phase
  value = (int) OffloadGetPhase ();
  WSContentSend_P (PSTR("<p class='%s'>%s<span class='key'>%s</span>\n"), "half", D_OFFLOAD_PHASE, D_CMND_OFFLOAD_PHASE);
  WSContentSend_P (PSTR("<select name='%s' id='%s'>\n"), D_CMND_OFFLOAD_PHASE, D_CMND_OFFLOAD_PHASE);
  for (index = 1; index <= OFFLOAD_PHASE_MAX; index ++)
  {
    if (index == value) str_text = "selected"; else str_text = "";
    WSContentSend_P (PSTR("<option value='%d' %s>%d</option>\n"), index, str_text.c_str (), index);
  }
  WSContentSend_P (PSTR("</select>\n"));
  WSContentSend_P (PSTR("</p>\n"));

  WSContentSend_P (OFFLOAD_FIELDSET_STOP);

  // ------------
  //    Meter  
  // ------------

  WSContentSend_P (OFFLOAD_FIELDSET_START, D_OFFLOAD_METER);

  // instant power mqtt topic
  str_text    = OffloadGetPowerTopic (false);
  str_default = OffloadGetPowerTopic (true);
  if (str_text == str_default) str_text = "";
  WSContentSend_P (OFFLOAD_INPUT_TEXT, D_OFFLOAD_TOPIC, D_CMND_OFFLOAD_TOPIC, D_CMND_OFFLOAD_TOPIC, str_text.c_str (), str_default.c_str ());

  // max power json key
  str_text    = OffloadGetContractPowerKey (false);
  str_default = OffloadGetContractPowerKey (true);
  if (str_text == str_default) str_text = "";
  WSContentSend_P (OFFLOAD_INPUT_TEXT, D_OFFLOAD_KEY_MAX, D_CMND_OFFLOAD_KEY_MAX, D_CMND_OFFLOAD_KEY_MAX, str_text.c_str (), str_default.c_str ());

  // instant power json key
  str_text    = OffloadGetInstPowerKey (false);
  str_default = OffloadGetInstPowerKey (true);
  if (str_text == str_default) str_text = "";
  WSContentSend_P (OFFLOAD_INPUT_TEXT, D_OFFLOAD_KEY_INST, D_CMND_OFFLOAD_KEY_INST, D_CMND_OFFLOAD_KEY_INST, str_text.c_str (), str_default.c_str ());

  WSContentSend_P (OFFLOAD_FIELDSET_STOP);
  WSContentSend_P (PSTR("<br>\n"));

  // ------------
  //    Script  
  // ------------

  WSContentSend_P (PSTR("<script type='text/javascript'>\n"));

  WSContentSend_P (PSTR("var arr_delay_before = [%d"), arr_offload_priority_before[0]);
  for (index = 1; index < OFFLOAD_PRIORITY_MAX; index ++) WSContentSend_P (PSTR(",%d"), arr_offload_priority_before[index]);
  WSContentSend_P (PSTR("];\n"));

  WSContentSend_P (PSTR("var arr_delay_after = [%d"), arr_offload_priority_after[0]);
  for (index = 1; index < OFFLOAD_PRIORITY_MAX; index ++) WSContentSend_P (PSTR(",%d"), arr_offload_priority_after[index]);
  WSContentSend_P (PSTR("];\n"));

  WSContentSend_P (PSTR("var arr_delay_random = [%d"), arr_offload_priority_random[0]);
  for (index = 1; index < OFFLOAD_PRIORITY_MAX; index ++) WSContentSend_P (PSTR(",%d"), arr_offload_priority_random[index]);
  WSContentSend_P (PSTR("];\n"));

  WSContentSend_P (PSTR("var device_type  = document.getElementById ('type');\n"));
  WSContentSend_P (PSTR("var delay_before = document.getElementById ('before');\n"));
  WSContentSend_P (PSTR("var delay_after  = document.getElementById ('after');\n"));
  WSContentSend_P (PSTR("var delay_random = document.getElementById ('random');\n"));

  WSContentSend_P (PSTR("device_type.onchange = function () {\n"));
  WSContentSend_P (PSTR("delay_before.value = arr_delay_before[this.value];\n"));
  WSContentSend_P (PSTR("delay_after.value = arr_delay_after[this.value];\n"));
  WSContentSend_P (PSTR("delay_random.value = arr_delay_random[this.value];\n"));
  WSContentSend_P (PSTR("}\n"));

  WSContentSend_P (PSTR("</script>\n"));

  // save button  
  // -----------
  WSContentSend_P (PSTR ("<p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), D_SAVE);
  WSContentSend_P (PSTR("</form>\n"));

  // configuration button
  WSContentSpaceButton(BUTTON_CONFIGURATION);

  // end of page
  WSContentStop();
}

// Offloading public configuration page
void OffloadWebPageControl ()
{
  bool    is_offloaded;
  uint8_t relay_state;
  String  str_name, str_style;

  // get offloaded status
  is_offloaded = OffloadIsOffloaded ();

  // if switch has been switched OFF
  if (Webserver->hasArg(D_CMND_OFFLOAD_OFF))
  {
    if (is_offloaded == true) offload_relay_state = 0;
    else ExecuteCommandPower (1, POWER_OFF, SRC_MAX); 
  }

  // else, if switch has been switched ON
  else if (Webserver->hasArg(D_CMND_OFFLOAD_ON))
  {
    if (is_offloaded == true) offload_relay_state = 1;
    else ExecuteCommandPower (1, POWER_ON, SRC_MAX);
  }

  // read real relay state or saved one if currently offloaded
  if (is_offloaded == true) relay_state = offload_relay_state;
  else relay_state = bitRead (power, 0);

  // beginning of form without authentification
  WSContentStart_P (D_OFFLOAD_CONTROL, false);
  WSContentSend_P (PSTR ("</script>\n"));
  
  // page style
  WSContentSend_P (PSTR ("<style>\n"));

  WSContentSend_P (PSTR ("body {color:white;background-color:#303030;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {width:100%%;margin:auto;padding:3px 0px;text-align:center;vertical-align:middle;}\n"));
  
  WSContentSend_P (PSTR (".title {margin:20px auto;font-size:5vh;}\n"));
  WSContentSend_P (PSTR (".icon {margin:30px auto;}\n"));
  WSContentSend_P (PSTR (".switch {text-align:left;font-size:5vh;}\n"));
  WSContentSend_P (PSTR (".toggle input {display:none;}\n"));
  WSContentSend_P (PSTR (".toggle {position:relative;display:inline-block;width:160px;height:60px;margin:50px auto;}\n"));
  WSContentSend_P (PSTR (".slide-off {position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background-color:#ff0000;border:1px solid #aaa;border-radius:10px;}\n"));
  WSContentSend_P (PSTR (".slide-off:before {position:absolute;content:'';width:70px;height:48px;left:4px;top:5px;background-color:#eee;border-radius:6px;}\n"));
  WSContentSend_P (PSTR (".slide-off:after {position:absolute;content:'Off';top:6px;right:15px;color:#fff;}\n"));
  WSContentSend_P (PSTR (".slide-on {position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background-color:#8cbc13;border:1px solid #aaa;border-radius:10px;}\n"));
  WSContentSend_P (PSTR (".slide-on:before {position:absolute;content:'';width:70px;height:48px;left:84px;top:5px;background-color:#eee;border-radius:6px;}\n"));
  WSContentSend_P (PSTR (".slide-on:after {position:absolute;content:'On';top:6px;left:15px;color:#fff;}\n"));
  WSContentSend_P (PSTR (".offload {background-color:#ffa000;}\n"));

  WSContentSend_P (PSTR ("</style>\n</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_PAGE_OFFLOAD_CONTROL);

  // device name
  WSContentSend_P (PSTR ("<div class='title bold'>%s</div>\n"), SettingsText(SET_FRIENDLYNAME1));

  // information about current offloading
  WSContentSend_P (PSTR ("<div class='icon'><img src='"));
  if (is_offloaded == true) WSContentSend_P (PSTR ("offloaded.png"));
  else if (relay_state == 1) WSContentSend_P (PSTR ("plugged.png"));
  else WSContentSend_P (PSTR ("unplugged.png"));
  WSContentSend_P (PSTR ("' ></div>\n"));

  // display switch button
  if (relay_state == 0) { str_name = "on"; str_style = "slide-off"; }
  else { str_name = "off"; str_style = "slide-on"; }
  if (is_offloaded == true) str_style += " offload";
  WSContentSend_P (PSTR ("<div><label class='toggle'><input name='%s' type='submit'/><span class='switch %s'></span></label></div>"), str_name.c_str (), str_style.c_str ());

  // end of page
  WSContentSend_P (PSTR ("</form>\n"));
  WSContentStop ();
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
      OffloadUpdateStatus ();
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
      Webserver->on ("/" D_PAGE_OFFLOAD_CONFIG, OffloadWebPageConfig);
      if (offload_relay_managed == true) Webserver->on ("/" D_PAGE_OFFLOAD_CONTROL, OffloadWebPageControl);
      Webserver->on ("/unplugged.png", OffloadWebIconUnplugged);
      Webserver->on ("/plugged.png",   OffloadWebIconPlugged  );
      Webserver->on ("/offloaded.png", OffloadWebIconOffloaded);
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      if (offload_relay_managed == true) OffloadWebControlButton ();
      break;
    case FUNC_WEB_ADD_BUTTON:
      OffloadWebConfigButton ();
      break;
    case FUNC_WEB_SENSOR:
      OffloadWebSensor ();
      break;
#endif  // USE_WEBSERVER

  }
  
  return result;
}

#endif // USE_OFFLOADING
