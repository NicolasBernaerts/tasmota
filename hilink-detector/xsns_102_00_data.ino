/*
  xsns_102_00_data.ino - Hi-Link movement and presence detector driver data

  Copyright (C) 2020-2025  Nicolas Bernaerts
    01/01/2026 - v1.0 - Rewrite for Hi-Link driver

  Manage sensors connected thru serial port (Rx 2410 and Tx 2410) :
    - HLK-LD1115H
    - HLK-LD1125H
    - HLK-LD2402
    - HLK-LD2410
    - HLK-LD2410S
    - HLK-LD2420
    - HLK-LD2450

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

#ifdef USE_HILINK_DETECTOR

#include <TasmotaSerial.h>

#define HILINK_GPIO_RX                   GPIO_LD2410_RX
#define HILINK_GPIO_TX                   GPIO_LD2410_TX

#define HILINK_MSG_SIZE_MAX              148       // maximum message size

#define HILINK_MAX_GATE                  16        // maximum number of detection gates
#define HILINK_MAX_ZONE                  3         // maximum number of detection zones

#define HILINK_MAX_PRESENCE              1         // maximum number of presence targets
#define HILINK_MAX_MOTION                3         // maximum number of motion targets

#define HILINK_DEFAULT_DELAY             5         // delay to trigger inactivity (sec.)
#define HILINK_DEFAULT_SAMPLE            8         // number of samples to average
#define HILINK_DEFAULT_DISTANCE          500       // default max distance is 5 m

#define HILINK_COMMAND_TIMEOUT_DELAY     1000      // command answer timeout (1 sec)

// hilink sensor JSON part
enum HilinkDetectorContext          { HILINK_JSON_GENERAL, HILINK_JSON_PRESENCE, HILINK_JSON_MOTION, HILINK_JSON_MAX };

// hilink sensor common commands        display help      device name     first commands     display info      display params        set delay      set min distance  set max distance    display JSON
enum HilinkDetectorCommands         { HILINK_CMND_HELP, HILINK_CMND_NAME, HILINK_CMND_INIT, HILINK_CMND_FIRST, HILINK_CMND_INFO, HILINK_CMND_PARAM, HILINK_CMND_DELAY, HILINK_CMND_DMIN, HILINK_CMND_DMAX, HILINK_CMND_JSON, HILINK_CMND_MAX };

// hilink sensor list
enum HilinkDetectorType             { HLK_DEVICE_NONE, HLK_DEVICE_LD1115, HLK_DEVICE_LD1125, HLK_DEVICE_LD2410B, HLK_DEVICE_LD2410C, HLK_DEVICE_LD2420, HLK_DEVICE_LD2450, HLK_DEVICE_LD2410S, HLK_DEVICE_LD2402, HLK_DEVICE_LD2401, HLK_DEVICE_LD2412, HLK_DEVICE_MAX };
const char kHilinkModel[]   PROGMEM =      "none"   "|"    "LD1115"    "|"     "LD1125"   "|"     "LD2410b"   "|"     "LD2410c"   "|"     "LD2420"   "|"     "LD2450"   "|"     "LD2410s"   "|"     "LD2402"   "|"     "LD2401"    "|"     "LD2412"    ;

// hilink log level
enum HilinkLogLevel                  { HILINK_LOG_OFF, HILINK_LOG_RECV, HILINK_LOG_SENT, HILINK_LOG_ALL, HILINK_LOG_MAX };
const char kHilinkLogLevel[] PROGMEM =      "off"   "|"      "data"    "|"      "cmnd"    "|"    "all"    ;

// zones
enum HilinkZone                       { HILINK_ZONE_DISABLE, HILINK_ZONE_INCLUDE, HILINK_ZONE_EXCLUDE, HILINK_ZONE_1, HILINK_ZONE_2, HILINK_ZONE_3, HILINK_ZONE_RESET };
const char kHilinkZone[]      PROGMEM =        "off"      "|"      "inc"       "|"      "exc"       "|"     "1"    "|"     "2"    "|"     "3"    "|"     "reset"       ;
const char kHilinkZoneLabel[] PROGMEM =      "DISABLED"   "|"     "INCLUDE"    "|"     "EXCLUDE"     ;

// light commands
enum HilinkListLight               { HILINK_LIGHT_OFF, HILINK_LIGHT_LOW, HILINK_LIGHT_HIGH };
const char kHilinkLights[] PROGMEM =       "off"    "|"     "below"    "|"    "above"       ;

// ----------------
// binary commands
// ----------------

// body of commands
uint8_t hilink_cmnd_header[]  PROGMEM = { 0xfd, 0xfc, 0xfb, 0xfa };
uint8_t hilink_cmnd_footer[]  PROGMEM = { 0x04, 0x03, 0x02, 0x01 };

uint8_t hilink_cmnd_start[]   PROGMEM = { 0xff, 0x00, 0x01, 0x00 };
uint8_t hilink_cmnd_stop[]    PROGMEM = { 0xfe, 0x00 };

/*************************************************\
 *               Variables
\*************************************************/

struct hlk_cfg {
  union {
    uint8_t data;
    struct {
      uint8_t bluetooth : 1;          // bluetooth flag
      uint8_t output    : 1;          // output pin level when detection
      uint8_t light     : 2;          // light detection policy
      uint8_t log       : 2;          // log level
      uint8_t spare     : 2;          // spare bits
    };
  };
};

//    config
// -------------

struct {
  uint8_t  device       = HLK_DEVICE_NONE;          // presence sensor type
  uint8_t  light_thres  = 0;                        // light threshold (ld2412)
  uint16_t delay        = HILINK_DEFAULT_DELAY;     // device detection timeout (s)
  uint16_t dist_min     = 0;                        // minimum detection distance
  uint16_t pres_max     = 0;                        // maximum presence detection distance
  uint16_t move_max     = 0;                        // maximum motion detection distance
  hlk_cfg  param;
} hilink_config;

//    status
// -------------

struct {
  bool     outdated    = false;                     // firmware version is too old
  bool     radar2D     = false;                     // use 2D radar instead of linear radar
  uint8_t  progress    = UINT8_MAX;                 // auto level detection
  uint8_t  protocol    = 0;                         // device protocol
  uint8_t  light_level = 0;                         // light sensor level
  uint8_t  gate_qty    = 0;                         // device number of gates
  uint16_t gate_width  = 100;                       // device gate witdh (cm)
  uint16_t dist_limit  = 0;                         // sensor distance limit (cm)
  uint32_t baudrate    = 115200;                    // device baud rate
  uint32_t baudmode    = SERIAL_8N1;                // device baud mode
  char     char_yellow = 0;                         // detector yellow letter
  char     char_green  = 0;                         // detector green letter
  char     str_model[12];                           // detector model
  char     str_mac[18];                             // MAC address
  String   str_firmware;                            // firmware version
  TasmotaSerial *pserial = nullptr;                 // serial interface
} hilink_status;

//   targets
// -------------

struct sensor_target
{
  bool     active;
  uint8_t  power;
  uint16_t dist;
  int16_t  x;
  int16_t  y;
  int16_t  speed;
};

struct sensor_gate
{
  uint8_t threshold;
  uint8_t power;
};

struct {
  bool          enabled   = false;                      // static detection enabled
  uint32_t      timestamp = UINT32_MAX;                 // timestamp of last presence detection
  sensor_target arr_target[HILINK_MAX_PRESENCE];        // presence targets
  sensor_gate   arr_gate[HILINK_MAX_GATE];              // motion detection gates
} hilink_static;

struct {
  bool          enabled   = false;                      // motion detection enabled
  uint32_t      timestamp = UINT32_MAX;                 // timestamp of last motion detection
  sensor_target arr_target[HILINK_MAX_MOTION];          // motion targets
  sensor_gate   arr_gate[HILINK_MAX_GATE];              // motion detection gates
} hilink_motion;

//     zones
// -------------

struct hlk_zone
{
  int16_t x1;                                     // x1 detection point (-x -> x cm)
  int16_t x2;                                     // x2 detection point (-x -> x cm)
  int16_t y1;                                     // y1 detection point (0 -> y cm)
  int16_t y2;                                     // y2 detection point (O -> y cm)
};

struct {
  uint8_t  status = HILINK_ZONE_DISABLE;          // zone detection status (disabled)
  uint8_t  max    = HILINK_MAX_ZONE;              // number of handled detection zone
  hlk_zone arr_zone[HILINK_MAX_ZONE];             // detection zones
} hilink_zone;

//   reception
// -------------

static struct {   
  bool     data     = true;                       // reception of data
  bool     energy   = false;                      // data is in engineering mode
  uint8_t  since    = 0;                          // seconds since last reception
  uint8_t  idx_body = 0;                          // reception buffer : index
  uint8_t  arr_body[HILINK_MSG_SIZE_MAX];         // reception buffer : content
} hilink_reception; 

//   commands
// -------------

static struct {
  bool     mode     = false;                      // command mode active
  uint8_t  command  = UINT8_MAX;                  // command to handle
  uint8_t  param    = 0;                          // current command param
  uint32_t ts_start = UINT32_MAX;                 // timestamp of entering command mode
  uint32_t ts_next  = UINT32_MAX;                 // timestamp to wait for next command
  String   str_queue;                             // command queue
} hilink_command; 

#endif      // USE_HILINK_DETECTOR
