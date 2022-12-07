/*
  xsns_119_gazpar.ino - Enedis Gazpar Gas meter support for Sonoff-Tasmota

  Copyright (C) 2022  Nicolas Bernaerts

  Gazpar impulse connector should be declared as Counter 1
  To be on the safe side and to avoid false trigger, set :
    - CounterDebounce 150
    - CounterDebounceHigh 50
    - CounterDebounceLow 50

  Config is stored in LittleFS partition in /gazpar.cfg

  Version history :
    28/04/2022 - v1.0 - Creation
    06/11/2022 - v1.1 - Rename to XSNS_119
    04/12/2022 - v1.2 - Add graphs

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

#ifdef USE_GAZPAR
#ifdef USE_UFILESYS

/*************************************************\
 *               Variables
\*************************************************/

// declare gazpar energy driver and sensor
#define XSNS_119   119

// web strings
#define D_GAZPAR_CONFIG             "Configure Gazpar"
#define D_GAZPAR_POWER              "Power"
#define D_GAZPAR_GRAPH              "Graph"
#define D_GAZPAR_TODAY              "Today"
#define D_GAZPAR_TOTAL              "Total"

#define D_GAZPAR_FACTOR             "Conversion factor"

// commands
#define D_CMND_GAZPAR_FACTOR        "factor"
#define D_CMND_GAZPAR_TOTAL         "total"
#define D_CMND_GAZPAR_MAX_HOUR      "max-hour"
#define D_CMND_GAZPAR_MAX_DAY       "max-day"
#define D_CMND_GAZPAR_MAX_MONTH     "max-month"
#define D_CMND_GAZPAR_HISTO         "histo"
#define D_CMND_GAZPAR_MONTH         "month"
#define D_CMND_GAZPAR_DAY           "day"
#define D_CMND_GAZPAR_INCREMENT     "incr"
#define D_CMND_GAZPAR_DECREMENT     "decr"

// default values
#define GAZPAR_CONVERSION_DEFAULT     11.2

// graph data
#define GAZPAR_GRAPH_WIDTH            1200      // graph width
#define GAZPAR_GRAPH_HEIGHT           600       // default graph height
#define GAZPAR_GRAPH_PERCENT_START    5         // start position of graph window
#define GAZPAR_GRAPH_PERCENT_STOP     95        // stop position of graph window

#define GAZPAR_GRAPH_MAX_BARGRAPH     32        // maximum number of bar graph
#define GAZPAR_GRAPH_DEFAULT_HOUR     5
#define GAZPAR_GRAPH_STEP_HOUR        1
#define GAZPAR_GRAPH_DEFAULT_DAY      10
#define GAZPAR_GRAPH_STEP_DAY         5
#define GAZPAR_GRAPH_DEFAULT_MONTH    100
#define GAZPAR_GRAPH_STEP_MONTH       50

// web URL
#define D_GAZPAR_PAGE_CONFIG          "/cfg"
#define D_GAZPAR_PAGE_GRAPH           "/graph"
#define D_GAZPAR_ICON_PNG             "/icon.png"

// files
#define D_GAZPAR_CFG                  "/gazpar.cfg"
#define D_GAZPAR_CSV                  "/gazpar-year-%04u.csv" 

// Gazpar - MQTT commands : gaz_count
const char kGazparCommands[]          PROGMEM = "gaz_" "|" D_CMND_GAZPAR_FACTOR "|" D_CMND_GAZPAR_TOTAL;
void (* const GazparCommand[])(void)  PROGMEM = { &CmndGazparFactor, &CmndGazparTotal };

// month and week day names
const char kGazparYearMonth[] PROGMEM = "|Jan|Fév|Mar|Avr|Mai|Jun|Jui|Aoû|Sep|Oct|Nov|Déc";         // month name for selection
const char kGazparWeekDay[]   PROGMEM = "Lun|Mar|Mer|Jeu|Ven|Sam|Dim";                              // day name for selection
const char kGazparWeekDay2[]  PROGMEM = "lu|ma|me|je|ve|sa|di";                                     // day name for bar graph

/*********************************\
 *              Data
\*********************************/

// gazpar : configuration
static struct {
  float    conv_factor = NAN;                           // coefficient de conversion
  int      height      = GAZPAR_GRAPH_HEIGHT;           // graph height in pixels
  uint32_t max_hour    = GAZPAR_GRAPH_DEFAULT_HOUR;         // maximum hourly graph value for daily active power total
  uint32_t max_day     = GAZPAR_GRAPH_DEFAULT_DAY;          // maximum daily graph value for monthly active power total
  uint32_t max_month   = GAZPAR_GRAPH_DEFAULT_MONTH;        // maximum monthly graph value for yearly active power total
} gazpar_config;

// gazpar : meter
static struct {
  long last_count      = 0;
  long last_count_day  = 0;                             // previous daily total
  long last_count_hour = 0;                             // previous hour total
  long count_hour[24];                                  // hourly increments
} gazpar_meter;

// gazpar : graph data
static struct {
  long    left;                                         // left position of the curve
  long    right;                                        // right position of the curve
  long    width;                                        // width of the curve (in pixels)
  uint8_t histo = 0;                                    // graph historisation index
  uint8_t month = 0;                                    // graph current month
  uint8_t day = 0;                                      // graph current day
} gazpar_graph;

/****************************************\
 *               Icons
 * 
 *      xxd -i -c 256 icon.png
\****************************************/

const char gazpar_icon_png[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x40, 0x08, 0x03, 0x00, 0x00, 0x00, 0x9d, 0xb7, 0x81, 0xec, 0x00, 0x00, 0x00, 0xc0, 0x50, 0x4c, 0x54, 0x45, 0x7f, 0x00, 0x00, 0x30, 0x32, 0x40, 0x43, 0x3e, 0x3c, 0x0d, 0x49, 0x8c, 0x79, 0x41, 0x27, 0x88, 0x42, 0x14, 0x85, 0x42, 0x4b, 0x54, 0x54, 0x56, 0x69, 0x50, 0x45, 0x52, 0x55, 0x5f, 0x13, 0x6b, 0xba, 0x4d, 0x60, 0x80, 0xb3, 0x49, 0x4e, 0x9d, 0x5b, 0x23, 0x96, 0x66, 0x3e, 0x71, 0x71, 0x70, 0x6c, 0x76, 0x88, 0x8a, 0x70, 0x62, 0x2d, 0x8a, 0xdc, 0x91, 0x79, 0x57, 0x83, 0x8e, 0x49, 0xc0, 0x81, 0x3e, 0xa9, 0x86, 0x78, 0x7d, 0x90, 0xaa, 0x8c, 0x8e, 0x93, 0xb5, 0x8d, 0x61, 0xa0, 0x93, 0x80, 0xd3, 0x91, 0x34, 0x92, 0xaa, 0xc3, 0xaa, 0xaa, 0xa4, 0xa5, 0xb8, 0x49, 0xaa, 0xb1, 0xbd, 0xa8, 0xc0, 0x2d, 0xd8, 0xb2, 0x3f, 0xe5, 0xaf, 0x34, 0xb8, 0xb4, 0xa3, 0xac, 0xc9, 0x06, 0xb4, 0xbf, 0x7f, 0xb9, 0xb9, 0xb5, 0xa4, 0xbd, 0xd6, 0xb2, 0xd0, 0x00, 0xcd, 0xc0, 0x8c, 0xc3, 0xc6, 0xc4, 0xc9, 0xc6, 0xbc, 0xf1, 0xc5, 0x38, 0xb0, 0xcc, 0xe2, 0xbc, 0xdd, 0x00, 0xf4, 0xc9, 0x2f, 0xf9, 0xcc, 0x27, 0xf2, 0xce, 0x53, 0xd2, 0xd2, 0xb2, 0xe9, 0xd0, 0x7b, 0xc6, 0xdf, 0x56, 0xd7, 0xd2, 0xd1, 0xd3, 0xd5, 0xd2, 0xd5, 0xd5, 0xcb, 0xf4, 0xd5, 0x6b, 0xcb, 0xec, 0x00, 0xd5, 0xe1, 0xa7, 0xc9, 0xe2, 0xf1, 0xd6, 0xe8, 0x7e, 0xe7, 0xe0, 0xad, 0xe9, 0xe9, 0xd7, 0xea, 0xee, 0xef, 0xe7, 0x51, 0x7c, 0x50, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x03, 0xa9, 0x49, 0x44,
  0x41, 0x54, 0x58, 0xc3, 0xbd, 0x97, 0x7f, 0x6f, 0xa3, 0x30, 0x0c, 0x86, 0x4b, 0x45, 0x47, 0xa3, 0x1e, 0x90, 0xea, 0x44, 0x81, 0xc2, 0x54, 0xc1, 0xfe, 0x40, 0xdd, 0x05, 0x71, 0xa1, 0xd2, 0x84, 0x54, 0x6d, 0xfb, 0xfe, 0xdf, 0xea, 0xec, 0x24, 0x84, 0xa4, 0xfc, 0x68, 0x6f, 0x3b, 0xdd, 0xbb, 0x95, 0x4d, 0x15, 0x7e, 0xe2, 0x38, 0x8e, 0xe3, 0xac, 0x56, 0xff, 0x5b, 0xaf, 0xa7, 0xd3, 0x37, 0xac, 0xaf, 0xd7, 0x6b, 0x5d, 0xe5, 0xc9, 0xf7, 0x00, 0xf5, 0x63, 0x80, 0x6b, 0x3d, 0xe1, 0xe9, 0xe7, 0xe7, 0x27, 0x22, 0x92, 0xc7, 0x06, 0xaa, 0x9e, 0x67, 0x00, 0xd7, 0x6b, 0x7e, 0x2f, 0x0c, 0x6f, 0xf8, 0x56, 0xf5, 0xfc, 0xf2, 0xab, 0x45, 0x29, 0x53, 0x29, 0x40, 0xe7, 0x55, 0x15, 0xd3, 0x25, 0x04, 0x58, 0xb4, 0xef, 0xed, 0x3b, 0xe8, 0x02, 0x82, 0x67, 0x77, 0x79, 0xff, 0xf8, 0x50, 0xe6, 0x10, 0xc4, 0x98, 0xc6, 0x94, 0xd0, 0x89, 0x79, 0xe0, 0x68, 0xc2, 0x4c, 0x18, 0x76, 0x1c, 0xc4, 0x18, 0xe7, 0xdd, 0xe5, 0x52, 0x14, 0xc5, 0xb9, 0x95, 0xde, 0xc3, 0x22, 0xc4, 0x84, 0x10, 0xcf, 0x23, 0x37, 0x84, 0xb3, 0x30, 0xea, 0x3a, 0x34, 0x62, 0x8d, 0x29, 0xc6, 0x3b, 0x5e, 0x14, 0x97, 0x73, 0xfb, 0x66, 0x00, 0x40, 0x61, 0x68, 0xd9, 0xf3, 0xe6, 0x56, 0xbf, 0x35, 0x81, 0xf1, 0x32, 0x4b, 0xd3, 0x73, 0xab, 0x00, 0x54, 0xd8, 0x7b, 0x9e, 0xb7, 0x6c, 0x2f, 0x4d, 0x85, 0x37, 0xac, 0x2c, 0xa3, 0x28, 0x4d, 0xd3, 0xd7, 0x79, 0xc0, 0x85, 0x69, 0x03, 0x78, 0xbb, 0x2c, 0x33, 0x25, 0xf8, 0x57, 0x7c, 0x5d, 0x46, 0xbe, 0x1f, 0xa4, 0x47, 0x13, 0x00, 0x08, 0x03, 0x50, 0x34, 0x8d, 0x30, 0x8a, 0x94, 0x7c, 0x29, 0x31, 0x3c, 0x3c, 0xc0, 0x83, 0x0d, 0x12, 0x6a, 0x4c, 0x0f,
  0x0d, 0x20, 0xc6, 0x0c, 0x18, 0xd8, 0xc3, 0x38, 0x1b, 0x77, 0x2b, 0x85, 0x21, 0x0a, 0x49, 0xc8, 0x95, 0x5f, 0x65, 0x16, 0x6d, 0x36, 0x7e, 0x10, 0x9c, 0xea, 0x59, 0x00, 0x2b, 0x21, 0x68, 0xa5, 0x4b, 0x28, 0xa5, 0x71, 0x9c, 0xc7, 0xf0, 0xc9, 0xf3, 0x38, 0xe9, 0x23, 0x53, 0x66, 0xfe, 0x06, 0x09, 0x03, 0xc0, 0xf3, 0xac, 0x18, 0xa0, 0x07, 0xf8, 0x9e, 0x8b, 0x76, 0x79, 0x05, 0x2f, 0xc1, 0xa3, 0xce, 0xf3, 0xce, 0x06, 0xf8, 0x77, 0x01, 0x5b, 0xc8, 0x52, 0xb0, 0x44, 0x55, 0x38, 0xdf, 0x59, 0x80, 0x77, 0x1f, 0x50, 0xe3, 0xdf, 0x25, 0x80, 0xb5, 0x0a, 0x7a, 0x0a, 0xb9, 0xb3, 0x23, 0x0e, 0x21, 0xf0, 0x7c, 0x02, 0xad, 0x8b, 0x19, 0x80, 0x27, 0x00, 0x64, 0x0e, 0xb0, 0x73, 0x9c, 0xdd, 0xd3, 0x0f, 0xd0, 0x9e, 0x2f, 0x01, 0x26, 0x83, 0x08, 0xf1, 0xa7, 0xf8, 0x9b, 0xe7, 0x09, 0x6a, 0x09, 0x30, 0x19, 0x83, 0x20, 0xc7, 0x37, 0x60, 0x15, 0xc4, 0xe7, 0x74, 0xed, 0x66, 0x96, 0x71, 0x1e, 0x70, 0xaa, 0x7b, 0x55, 0xb0, 0x94, 0xf5, 0x1c, 0x80, 0x2c, 0x79, 0x80, 0xf9, 0x2e, 0x4a, 0x1a, 0xfc, 0xfb, 0x17, 0x00, 0x86, 0x00, 0x7e, 0xc1, 0x9a, 0xf2, 0xa1, 0xd5, 0xf2, 0xd9, 0x44, 0x12, 0xb9, 0x64, 0x7b, 0xc0, 0xd4, 0x66, 0xe4, 0x83, 0x58, 0xa3, 0xf7, 0x82, 0xf2, 0xa0, 0xaa, 0xa7, 0xb7, 0xb3, 0x02, 0x08, 0x45, 0xee, 0x76, 0xbf, 0xdf, 0x1f, 0x38, 0x1b, 0x8a, 0x02, 0x6c, 0xd4, 0x31, 0x60, 0x94, 0x48, 0xfd, 0x70, 0x91, 0xe3, 0x38, 0x64, 0x47, 0x86, 0x0a, 0x83, 0x05, 0x62, 0xb4, 0x99, 0xa6, 0xa7, 0x20, 0x01, 0x3f, 0x85, 0xac, 0x12, 0xc5, 0xa6, 0x00, 0xa6, 0x07, 0x6d, 0x39, 0x00, 0xec, 0xf9, 0x2f, 0x01, 0x88, 0x05, 0x90, 0xc5, 0x8f, 0xdd, 0x96, 0xe4,
  0x79, 0x00, 0xa5, 0x06, 0x60, 0xa5, 0x00, 0x0d, 0x8e, 0xdd, 0x59, 0x92, 0xa7, 0x03, 0x02, 0xfc, 0x1e, 0x10, 0x53, 0x2a, 0xea, 0x4e, 0x4c, 0xc6, 0x00, 0xb3, 0x1c, 0x8b, 0x43, 0x45, 0x51, 0x04, 0x20, 0x08, 0x64, 0x8e, 0x62, 0xc1, 0x82, 0x0d, 0xb3, 0x73, 0x3c, 0xb3, 0xa8, 0x4e, 0xb9, 0x8e, 0xdf, 0xb1, 0x21, 0x91, 0xb6, 0xc9, 0x09, 0x13, 0x1c, 0x3c, 0x20, 0xb0, 0x52, 0xce, 0xda, 0xae, 0xca, 0xac, 0x4f, 0x24, 0x66, 0x7b, 0x61, 0x00, 0xdc, 0x2d, 0x14, 0x5a, 0x08, 0xfe, 0x5a, 0xc9, 0x3c, 0x99, 0x0a, 0xf1, 0xa6, 0x38, 0x12, 0x06, 0x31, 0x8d, 0x90, 0x00, 0xf0, 0x01, 0xe4, 0x69, 0xc0, 0x71, 0x04, 0x30, 0xc6, 0x95, 0x08, 0x71, 0xb6, 0x34, 0x4c, 0x03, 0xf6, 0x50, 0xec, 0x43, 0x0d, 0x48, 0x26, 0x01, 0xcd, 0x38, 0x96, 0xda, 0x03, 0x37, 0x39, 0x41, 0xf4, 0x34, 0x61, 0x06, 0x60, 0xa3, 0xcc, 0x18, 0x6c, 0x02, 0x11, 0xc4, 0xf8, 0xcb, 0x00, 0xdc, 0x4c, 0x15, 0x9e, 0xee, 0xdf, 0x06, 0x78, 0xd3, 0xab, 0xc0, 0x46, 0xab, 0xa7, 0x11, 0xb7, 0x00, 0x20, 0x40, 0x39, 0xb0, 0x01, 0xb0, 0x68, 0xc6, 0x7e, 0xb8, 0x07, 0x10, 0x1a, 0x79, 0x20, 0xed, 0xbe, 0x0a, 0x00, 0x0f, 0x4a, 0xf8, 0xe9, 0xed, 0xcd, 0x29, 0x3d, 0x0a, 0xc0, 0xdc, 0x6b, 0x26, 0x01, 0x2c, 0xb3, 0x01, 0xea, 0x74, 0xb6, 0x00, 0x0d, 0x26, 0xae, 0xed, 0xc0, 0x12, 0x40, 0x78, 0xf0, 0x7c, 0x3b, 0x05, 0x56, 0x32, 0xb6, 0x0c, 0xa8, 0xee, 0x00, 0xc4, 0x16, 0x60, 0xb7, 0x7b, 0xb2, 0xd1, 0x80, 0xbd, 0x0d, 0x58, 0x59, 0x5d, 0x5a, 0xbf, 0x89, 0xd8, 0x68, 0x15, 0xe6, 0x00, 0x56, 0xa7, 0x7a, 0xc6, 0xf2, 0x33, 0xe1, 0xff, 0x12, 0xc0, 0x6a, 0x54, 0x5f, 0x20, 0x11, 0x3a, 0xde, 0xd8, 0x0c, 0xd5,
  0x65, 0xce, 0x01, 0x6e, 0x5a, 0xe5, 0xa1, 0x0a, 0x72, 0xc3, 0x17, 0x59, 0xd1, 0x00, 0x80, 0x9b, 0xd1, 0x3f, 0x54, 0xe6, 0x32, 0xde, 0xb4, 0xea, 0x69, 0xa9, 0x33, 0x51, 0xd5, 0x52, 0xae, 0xa7, 0xa2, 0x00, 0x6e, 0x62, 0x02, 0x46, 0xed, 0x7e, 0x2a, 0x16, 0x40, 0x27, 0x02, 0x52, 0xa0, 0x7f, 0xe7, 0x03, 0xc0, 0xb5, 0x00, 0xe1, 0xf8, 0xba, 0xf0, 0x92, 0xa6, 0x99, 0x51, 0x09, 0x25, 0x06, 0x27, 0x01, 0x9d, 0x72, 0x04, 0xe6, 0xae, 0xe8, 0xe1, 0xfa, 0x18, 0xcc, 0xdd, 0x9b, 0x8e, 0x69, 0x1a, 0x15, 0x6a, 0x36, 0xfa, 0x91, 0x45, 0x3e, 0x00, 0xb6, 0x89, 0x04, 0x88, 0x0e, 0x27, 0x5c, 0xba, 0x31, 0x1d, 0x0f, 0x07, 0xa0, 0x48, 0x0c, 0x02, 0x32, 0xdf, 0xc7, 0x26, 0x3a, 0x8c, 0x0d, 0xc0, 0xfd, 0x6b, 0x9f, 0xa4, 0x60, 0x65, 0x86, 0xd6, 0xdd, 0x85, 0x62, 0xee, 0x51, 0x01, 0x80, 0x63, 0xc5, 0x5b, 0x87, 0x8f, 0xde, 0x33, 0x0f, 0xa0, 0x20, 0x38, 0x1c, 0x93, 0x30, 0x5c, 0xf7, 0x00, 0x4a, 0x1e, 0x19, 0x7f, 0x42, 0x49, 0x28, 0x0e, 0x54, 0x4a, 0xbf, 0x71, 0xf1, 0x5d, 0x41, 0xef, 0xf9, 0xef, 0x6f, 0xf3, 0x7f, 0x00, 0x8e, 0x90, 0x71, 0x52, 0x4c, 0xd0, 0xa4, 0x82, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
unsigned int gazpar_icon_len = 1211;

/************************************\
 *             Commands
\************************************/

void CmndGazparFactor (void)
{
  if (XdrvMailbox.data_len > 0)
  {
    gazpar_config.conv_factor = atof (XdrvMailbox.data);
    GazparSaveConfig ();
  }
  ResponseCmndFloat (gazpar_config.conv_factor, 2);
}

void CmndGazparTotal (void)
{
  long delta, total;

  if (XdrvMailbox.data_len > 0)
  {
    // update last hour total
    gazpar_meter.count_hour[RtcTime.hour] = (long)RtcSettings.pulse_counter[0] - gazpar_meter.last_count_hour;

    // calculate delta between current and new total
    total = (long)XdrvMailbox.payload;
    delta = total - (long)RtcSettings.pulse_counter[0];

    // update new counters according to new total
    RtcSettings.pulse_counter[0] = (uint32_t)total;
    gazpar_meter.last_count_day  += delta;
    gazpar_meter.last_count_hour += delta;
  }
  ResponseCmndNumber (RtcSettings.pulse_counter[0]);
}

/************************************\
 *            Functions
\************************************/

// Load configuration from Settings or from LittleFS
void GazparLoadConfig () 
{
  // retrieve saved settings from flash filesystem
  gazpar_config.conv_factor = UfsCfgLoadKeyFloat (D_GAZPAR_CFG, D_CMND_GAZPAR_FACTOR,    GAZPAR_CONVERSION_DEFAULT);
  gazpar_config.max_hour    = UfsCfgLoadKeyInt   (D_GAZPAR_CFG, D_CMND_GAZPAR_MAX_HOUR,  GAZPAR_GRAPH_DEFAULT_HOUR);
  gazpar_config.max_day     = UfsCfgLoadKeyInt   (D_GAZPAR_CFG, D_CMND_GAZPAR_MAX_DAY,   GAZPAR_GRAPH_DEFAULT_DAY);
  gazpar_config.max_month   = UfsCfgLoadKeyInt   (D_GAZPAR_CFG, D_CMND_GAZPAR_MAX_MONTH, GAZPAR_GRAPH_DEFAULT_MONTH);

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("GAZ: Config loaded from LittleFS"));

  // validate boundaries
  if ((gazpar_config.conv_factor < 5) || (gazpar_config.conv_factor > 20)) gazpar_config.conv_factor = 11.46;
  gazpar_config.height = (gazpar_config.height / 100) * 100; 
}

// Save configuration to Settings or to LittleFS
void GazparSaveConfig () 
{
  // save settings into flash filesystem
  UfsCfgSaveKeyFloat (D_GAZPAR_CFG, D_CMND_GAZPAR_FACTOR,    gazpar_config.conv_factor, true);
  UfsCfgSaveKeyInt   (D_GAZPAR_CFG, D_CMND_GAZPAR_MAX_HOUR,  gazpar_config.max_hour,    false);
  UfsCfgSaveKeyInt   (D_GAZPAR_CFG, D_CMND_GAZPAR_MAX_DAY,   gazpar_config.max_day,     false);
  UfsCfgSaveKeyInt   (D_GAZPAR_CFG, D_CMND_GAZPAR_MAX_MONTH, gazpar_config.max_month,   false);
}

// Convert from Wh to liter
long GazparConvertWh2Liter (const long wh)
{
  float liter;

  liter = (float)wh / gazpar_config.conv_factor;

  return (long)liter;
}

// Convert from liter to wh
long GazparConvertLiter2Wh (const long liter)
{
  float wh;

  wh = (float)liter * gazpar_config.conv_factor;

  return (long)wh;
}

// Get historisation filename
bool GazparGetFilename (const uint8_t histo, char* pstr_filename)
{
  bool exists = false;

  // check parameters
  if (pstr_filename != nullptr)
  {
    // generate filename
    sprintf_P (pstr_filename, D_GAZPAR_CSV, RtcTime.year - histo);

AddLog (LOG_LEVEL_INFO, PSTR ("FILE: %s"), pstr_filename);

    // if filename defined, check existence
    if (strlen (pstr_filename) > 0) exists = ffsp->exists (pstr_filename);
  }

  return exists;
}

// Save historisation data
void GazparSaveDailyTotal ()
{
  uint8_t   index;
  uint32_t  current_time;
  TIME_T    today_dst;
  uint32_t  delta;
  char      str_value[32];
  char      str_filename[UFS_FILENAME_SIZE];
  String    str_line;
  File      file;

  // save daily ahd hourly delta
  delta = (long)RtcSettings.pulse_counter[0] - gazpar_meter.last_count_day;
  if (gazpar_meter.count_hour[RtcTime.hour] == 0) gazpar_meter.count_hour[RtcTime.hour] = (long)RtcSettings.pulse_counter[0] - gazpar_meter.last_count_hour;

  // update last day and hour total
  gazpar_meter.last_count_day  = (long)RtcSettings.pulse_counter[0];
  gazpar_meter.last_count_hour = (long)RtcSettings.pulse_counter[0];
    
  // calculate today's filename (shift 5 sec in case of sligth midnight call delay)
  current_time = LocalTime () - 5;
  BreakTime (current_time, today_dst);
  sprintf_P (str_filename, D_GAZPAR_CSV, 1970 + today_dst.year);

  // if file exists, open in append mode
  if (ffsp->exists (str_filename)) file = ffsp->open (str_filename, "a");

  // else open in creation mode
  else
  {
    file = ffsp->open (str_filename, "w");

    // generate header for daily sum
    str_line = "Idx;Month;Day;Global;Daily";
    for (index = 0; index < 24; index ++)
    {
      sprintf_P (str_value, ";%02uh", index);
      str_line += str_value;
    }
    str_line += "\n";
    file.print (str_line.c_str ());
  }

  // generate today's line
  sprintf (str_value, "%u;%u;%u;%u;%u", today_dst.day_of_year, today_dst.month, today_dst.day_of_month, RtcSettings.pulse_counter[0], delta);
  str_line = str_value;

  // loop to add hourly totals
  for (index = 0; index < 24; index ++)
  {
    // append hourly increment to line
    str_line += ";";
    str_line += gazpar_meter.count_hour[index];

    // reset hourly increment
    gazpar_meter.count_hour[index] = 0;
  }

  // write line and close file
  str_line += "\n";
  file.print (str_line.c_str ());
  file.close ();
}

/**********************************\
 *            Callback
\**********************************/

// Gazpar driver initialisation
void GazparInit ()
{
  uint8_t index;

  // init meter data
  gazpar_meter.last_count_day  = (long)RtcSettings.pulse_counter[0];                     // previous daily total
  gazpar_meter.last_count_hour = (long)RtcSettings.pulse_counter[0];                     // previous hour total
  for (index = 0; index < 24; index ++) gazpar_meter.count_hour[index] = 0;

  // boundaries of SVG graph
  gazpar_graph.left  = GAZPAR_GRAPH_PERCENT_START * GAZPAR_GRAPH_WIDTH / 100;
  gazpar_graph.right = GAZPAR_GRAPH_PERCENT_STOP  * GAZPAR_GRAPH_WIDTH / 100;
  gazpar_graph.width = gazpar_graph.right - gazpar_graph.left;

  // load configuration
  GazparLoadConfig ();
}

// Called every second
void GazparEverySecond ()
{
  // if hour change, save hourly increment
  if ((RtcTime.minute == 59) && (RtcTime.second == 59))
  {
    // calculate last hour increment and update last hour total
    gazpar_meter.count_hour[RtcTime.hour] = (long)RtcSettings.pulse_counter[0] - gazpar_meter.last_count_hour;
    gazpar_meter.last_count_hour = (long)RtcSettings.pulse_counter[0];
  }
}

// Save configuration in case of restart
void GazparSaveBeforeRestart ()
{
  // save new values
  GazparSaveConfig (); 
}

// Counter management and log files rotation
void GazparMidnight ()
{
  // save daily totals
  GazparSaveDailyTotal ();
}

// Show JSON status (for MQTT)
void GazparShowJSON (bool append)
{
  int   value;
  long  power_max, power_app, power_act;
  float current;
  char  str_text[16];

  // if not a telemetry call, add {"Time":"xxxxxxxx",
  if (!append) Response_P (PSTR ("{\"%s\":\"%s\","), D_JSON_TIME, GetDateAndTime (DT_LOCAL).c_str ());

  // Start Gazpar section "Gazpar":{
  Response_P (PSTR ("\"gazpar\":{"));

  // Gazpar total : "total":"1267.25 kWh"
  GazparDisplayInWh (true, false, (long)RtcSettings.pulse_counter[0], str_text, sizeof (str_text));
  ResponseAppend_P (PSTR ("\"total\":\"%sWh\""), str_text);

  // Gazpar total : "total-pulse":1267245
  ResponseAppend_P (PSTR (",\"total-pulse\":%u"), RtcSettings.pulse_counter[0]);

  // Gazpar today : "today":"7.25 kWh"
  GazparDisplayInWh (true, false, (long)RtcSettings.pulse_counter[0] - gazpar_meter.last_count_day, str_text, sizeof (str_text));
  ResponseAppend_P (PSTR (",\"today\":\"%sWh\""), str_text);

  // Gazpar today : "today-pulse":245
  ResponseAppend_P (PSTR (",\"today-pulse\":%d"), (long)RtcSettings.pulse_counter[0] - gazpar_meter.last_count_day);

  // end of Gazpar section
  ResponseAppend_P (PSTR ("}"));

  // if not in telemetry, publish JSON and process rulesdd
  if (!append)
  {
    ResponseAppend_P (PSTR ("}"));
    MqttPublishTeleSensor ();
  }
}

/**********************************\
 *               Web
\**********************************/

#ifdef USE_WEBSERVER

// Display offload icons
void GazparWebIcon () { Webserver->send_P (200, PSTR ("image/png"), gazpar_icon_png, gazpar_icon_len); }

// Convert wh/liter kWh with kilo conversion (120, 2.0k, 12.0k, 2.3M, ...)
void GazparDisplayInWh (const bool is_liter, const bool compact, const long value, char* pstr_result, const int size_result) 
{
  float result;
  char  str_sep[2];

  // check parameters
  if (pstr_result == nullptr) return;

  // set separator
  if (compact) strcpy (str_sep, "");
    else strcpy (str_sep, " ");

  // convert liter counter to Wh
  if (is_liter) result = (float)GazparConvertLiter2Wh (value);
    else result = (float)value;

  // convert values in M with 1 digit
  if (result > 9999999)
  {
    result = result / 1000000;
    ext_snprintf_P (pstr_result, size_result, PSTR ("%1_f%sM"), &result, str_sep);
  }

   // convert values in M with 2 digits
  else if (result > 999999)
  {
    result = result / 1000000;
    ext_snprintf_P (pstr_result, size_result, PSTR ("%2_f%sM"), &result, str_sep);
  }

  // else convert values in k with no digit
  else if (result > 99999)
  {
    result = (float)result / 1000;
    ext_snprintf_P (pstr_result, size_result, PSTR ("%0_f%sk"), &result, str_sep);
  }

  // else convert values in k with one digit
  else if (result > 9999)
  {
    result = (float)result / 1000;
    ext_snprintf_P (pstr_result, size_result, PSTR ("%1_f%sk"), &result, str_sep);
  }

  // else convert values in k with two digits
  else if (result > 999)
  {
    result = (float)result / 1000;
    ext_snprintf_P (pstr_result, size_result, PSTR ("%1_f%sk"), &result, str_sep);
  }

  // else no conversion
  else
  {
    result = (float)result;
    ext_snprintf_P (pstr_result, size_result, PSTR ("%0_f%s"), &result, str_sep);
  }
}

// Get specific argument as a value with min and max
long GazparWebGetArgValue (const char* pstr_argument, long value_default, long value_min = 0, long value_max = 0)
{
  long arg_value = value_default;
  char str_argument[8];

  // check for argument
  if (Webserver->hasArg (pstr_argument))
  {
    WebGetArg (pstr_argument, str_argument, sizeof (str_argument));
    arg_value = atol (str_argument);
  }

  // check for min and max value
  if ((value_min > 0) && (arg_value < value_min)) arg_value = value_min;
  if ((value_max > 0) && (arg_value > value_max)) arg_value = value_max;

  return arg_value;
}

// Append Gazpar graph button to main page
void GazparWebMainButton ()
{
  // Gazpar graph page button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_GAZPAR_PAGE_GRAPH, D_GAZPAR_GRAPH);
}

// Append Gazpar configuration button to configuration page
void GazparWebConfigButton ()
{
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_GAZPAR_PAGE_CONFIG, D_GAZPAR_CONFIG);
}

// Append Teleinfo state to main page
void GazparWebSensor ()
{
  char str_value[16];

  // calculate today total in kWh
  GazparDisplayInWh (true, false, (long)RtcSettings.pulse_counter[0] - gazpar_meter.last_count_day, str_value, sizeof (str_value));
  WSContentSend_P (PSTR ("{s}%s %s{m}%sWh{e}"), D_GAZPAR_POWER, D_GAZPAR_TODAY,  str_value);

  // display value
  GazparDisplayInWh (true, false, (long)RtcSettings.pulse_counter[0], str_value, sizeof (str_value));
  WSContentSend_P (PSTR ("{s}%s %s{m}%sWh{e}"), D_GAZPAR_POWER, D_GAZPAR_TOTAL, str_value);
}

// Gazpar configuration web page
void GazparWebPageConfigure ()
{
  char str_text[16];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess ()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg (F ("save")))
  {
    // parameter 'factor' : set conversion factor
    WebGetArg (D_CMND_GAZPAR_FACTOR, str_text, sizeof (str_text));
    if (strlen (str_text) > 0) gazpar_config.conv_factor = atof (str_text);

    // parameter 'maxh' : set graph maximum total per hour
    WebGetArg (D_CMND_GAZPAR_MAX_HOUR, str_text, sizeof (str_text));
    if (strlen (str_text) > 0) gazpar_config.max_hour = (uint32_t)atol (str_text);

    // parameter 'maxd' : set graph maximum total per day
    WebGetArg (D_CMND_GAZPAR_MAX_DAY, str_text, sizeof (str_text));
    if (strlen (str_text) > 0) gazpar_config.max_day = (uint32_t)atol (str_text);

    // parameter 'maxwhm' : set graph maximum total per month
    WebGetArg (D_CMND_GAZPAR_MAX_MONTH, str_text, sizeof (str_text));
    if (strlen (str_text) > 0) gazpar_config.max_month = (uint32_t)atol (str_text);

    // save configuration
    GazparSaveConfig ();

    // update JSON message
    GazparShowJSON (false);
  }

  // beginning of form
  WSContentStart_P (D_GAZPAR_CONFIG);
  WSContentSendStyle ();
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_GAZPAR_PAGE_CONFIG);

  // conversion factor
  WSContentSend_P (PSTR ("<p><fieldset><legend><b>&nbsp;%s %s&nbsp;</b></legend>\n"), "⚡", PSTR ("Conversion"));
  ext_snprintf_P (str_text, sizeof(str_text), PSTR ("%02_f"), &gazpar_config.conv_factor);
  WSContentSend_P (PSTR ("<p>%s<br><input type='number' name='%s' min='%d' max='%d' step='0.01' value='%s'></p>\n"), D_GAZPAR_FACTOR, D_CMND_GAZPAR_FACTOR, 5, 20, str_text);
  WSContentSend_P (PSTR ("</fieldset></p>\n<br>\n"));

  // graph range
  WSContentSend_P (PSTR ("<p><fieldset><legend><b>&nbsp;%s %s&nbsp;</b></legend>\n"), "📊", PSTR ("Graph Range (kWh)"));
  WSContentSend_P (PSTR ("<p>%s<br><input type='number' name='%s' min='%u' max='%u' step='%u' value='%u'></p>\n"), PSTR ("Daily (per Hour)"),   D_CMND_GAZPAR_MAX_HOUR,  0, 1000 * GAZPAR_GRAPH_DEFAULT_HOUR,  GAZPAR_GRAPH_STEP_HOUR,  gazpar_config.max_hour);
  WSContentSend_P (PSTR ("<p>%s<br><input type='number' name='%s' min='%u' max='%u' step='%u' value='%u'></p>\n"), PSTR ("Monthly (per Day)"),  D_CMND_GAZPAR_MAX_DAY,   0, 1000 * GAZPAR_GRAPH_DEFAULT_DAY,   GAZPAR_GRAPH_STEP_DAY,   gazpar_config.max_day);
  WSContentSend_P (PSTR ("<p>%s<br><input type='number' name='%s' min='%u' max='%u' step='%u' value='%u'></p>\n"), PSTR ("Yearly (per Month)"), D_CMND_GAZPAR_MAX_MONTH, 0, 1000 * GAZPAR_GRAPH_DEFAULT_MONTH, GAZPAR_GRAPH_STEP_MONTH, gazpar_config.max_month);
  WSContentSend_P (PSTR ("</fieldset></p>\n"));

  // save button
  WSContentSend_P (PSTR ("<button name='save' type='submit' class='button bgrn'>%s</button>"), D_SAVE);
  WSContentSend_P (PSTR ("</form>\n"));

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

// Display bar graph
void GazparWebDisplayBarGraph (const uint8_t histo, const bool current)
{
  int      index;
  long     value, value_x, value_y;
  long     graph_x, graph_y, graph_range, graph_delta, graph_width, graph_height, graph_x_end, graph_max;    
  long     arr_value[GAZPAR_GRAPH_MAX_BARGRAPH];
  uint8_t  day_of_week;
  uint32_t time_bar;
  size_t   size_line, size_value;
  TIME_T   time_dst;
  char     str_type[8];
  char     str_value[16];
  char     str_line[256];
  char     str_filename[UFS_FILENAME_SIZE];
  File     file;

  // init array
  for (index = 0; index < GAZPAR_GRAPH_MAX_BARGRAPH; index ++) arr_value[index] = LONG_MAX;

  // if full month view, calculate first day of month
  if (gazpar_graph.month != 0)
  {
    BreakTime (LocalTime (), time_dst);
    time_dst.year -= histo;
    time_dst.month = gazpar_graph.month;
    time_dst.day_of_week = 0;
    time_dst.day_of_year = 0;
  }

  // init graph units for full year display (month bars)
  if (gazpar_graph.month == 0)
  {
    graph_width = 90;             // width of graph bar area
    graph_range = 12;             // number of graph bars (months per year)
    graph_delta = 20;             // separation between bars (pixels)
    graph_max   = GazparConvertWh2Liter (1000 * gazpar_config.max_month);
    strcpy (str_type, "month");
  }

  // else init graph units for full month display (day bars)
  else if (gazpar_graph.day == 0)
  {
    graph_width = 35;             // width of graph bar area
    graph_range = 31;             // number of graph bars (days per month)
    graph_delta = 4;              // separation between bars (pixels)
    graph_max   = GazparConvertWh2Liter (1000 * gazpar_config.max_day);
    strcpy (str_type, "day");
  }

  // else init graph units for full day display (hour bars)
  else
  {
    graph_width = 45;             // width of graph bar area
    graph_range = 24;             // number of graph bars (hours per day)
    graph_delta = 10;             // separation between bars (pixels)
    graph_max   = GazparConvertWh2Liter (1000 * gazpar_config.max_hour);
    strcpy (str_type, "hour");
  }

  // if current day, collect live values
  if ((histo == 0) && (gazpar_graph.month == RtcTime.month) && (gazpar_graph.day == RtcTime.day_of_month))
  {
    // update last hour increment
    gazpar_meter.count_hour[RtcTime.hour] += (long)(gazpar_meter.last_count - gazpar_meter.last_count_hour);
    gazpar_meter.last_count_hour = gazpar_meter.last_count;

    // init hour slots from live values
    for (index = 0; index < 24; index ++) if (gazpar_meter.count_hour[index] > 0) arr_value[index] = gazpar_meter.count_hour[index];
  }

  // calculate graph height and graph start
  graph_height = gazpar_config.height;
  graph_x      = gazpar_graph.left + graph_delta / 2;
  graph_x_end  = graph_x + graph_width - graph_delta;
  if (!current) { graph_x += 6; graph_x_end -= 6; }

  // if data file exists
  if (GazparGetFilename (histo, str_filename))
  {
    // open file and skip header
    file = ffsp->open (str_filename, "r");
    UfsReadNextLine (file, str_line, sizeof (str_line));

    // loop to read lines and load array
    do
    {
      // read line
      size_line = UfsReadNextLine (file, str_line, sizeof (str_line));
      if (size_line > 0)
      {
        // init
        index = INT_MAX;
        value = LONG_MAX;

        // handle values for a full year
        if (gazpar_graph.month == 0)
        {
          // extract month index from line
          size_value = UfsExtractCsvColumn (str_line, ';', 2, str_value, sizeof (str_value), false);
          if (size_value > 0) index = atoi (str_value);

          // extract value from line
          size_value = UfsExtractCsvColumn (str_line, ';', 5, str_value, sizeof (str_value), false);
          if (size_value > 0) value = atol (str_value);

          // if index and value are valid, add value to month of year
          if ((index <= 12) && (value != LONG_MAX))
            if (arr_value[index] == LONG_MAX) arr_value[index] = value; 
              else arr_value[index] += value;
        }

        // else check if line deals with target month
        else
        {
          // extract month index from line
          size_value = UfsExtractCsvColumn (str_line, ';', 2, str_value, sizeof (str_value), false);
          if (size_value > 0) index = atoi (str_value);
          if (gazpar_graph.month == index)
          {
            // if display by days of selected month / year
            if (gazpar_graph.day == 0)
            {
              // extract day index from line
              index = INT_MAX;
              size_value = UfsExtractCsvColumn (str_line, ';', 3, str_value, sizeof (str_value), false);
              if (size_value > 0) index = atoi (str_value);

              // extract value from line
              size_value = UfsExtractCsvColumn (str_line, ';', 5, str_value, sizeof (str_value), false);
              if (size_value > 0) value = atol (str_value);

              // if index and value are valid, add value to day of month
              if ((index <= 31) && (value != LONG_MAX))
                if (arr_value[index] == LONG_MAX) arr_value[index] = value;
                  else arr_value[index] += value;
            }

            // else display by hours selected day / month / year
            else
            {
              // extract day index from line
              index = INT_MAX;
              size_value = UfsExtractCsvColumn (str_line, ';', 3, str_value, sizeof (str_value), false);
              if (size_value > 0) index = atoi (str_value);
              if (gazpar_graph.day == index)
              {
                // loop to extract hours increments
                for (index = 1; index <= 24; index ++)
                {
                  // extract value from line
                  size_value = UfsExtractCsvColumn (str_line, ';', index + 5, str_value, sizeof (str_value), false);
                  if (size_value > 0) value = atol (str_value);

                  // if value is valid, add value to hour slot
                  if (arr_value[index] == LONG_MAX) arr_value[index] = value;
                   else arr_value[index] += value;
                }
              }
            }
          }
        }
      }
    }
    while (size_line > 0);

    // close file
    file.close ();
  }

  // loop to display bar graphs
  for (index = 1; index <= graph_range; index ++)
  {
    // if value is defined, display bar and value
    if ((arr_value[index] != LONG_MAX) && (arr_value[index] > 0))
    {
      // bar graph
      // ---------

      // bar y position
      graph_y = graph_height - (arr_value[index] * graph_height / graph_max);
      if (graph_y < 0) graph_y = 0;

      // display link
      if (current && (gazpar_graph.month == 0)) WSContentSend_P (PSTR("<a href='%s?month=%d&day=0'>"), D_GAZPAR_PAGE_GRAPH, index);
      else if (current && (gazpar_graph.day == 0)) WSContentSend_P (PSTR("<a href='%s?day=%d'>"), D_GAZPAR_PAGE_GRAPH, index);

      // display bar
      if (current) strcpy (str_value, "now"); else strcpy (str_value, "prev");
      WSContentSend_P (PSTR("<path class='%s' d='M%d %d L%d %d L%d %d L%d %d L%d %d L%d %d Z'></path>"), str_value, graph_x, graph_height, graph_x, graph_y + 2, graph_x + 2, graph_y, graph_x_end - 2, graph_y, graph_x_end, graph_y + 2, graph_x_end, graph_height);

      // end of link 
      if (current && ((gazpar_graph.month == 0) || (gazpar_graph.day == 0))) WSContentSend_P (PSTR("</a>\n"));
        else WSContentSend_P (PSTR("\n"));

      // bar values
      // -----------
     if (current)
      {
        // top of bar value
        // ----------------

        // calculate bar graph value position
        value_x = (graph_x + graph_x_end) / 2;
        value_y = graph_y - 15;
        if (value_y < 15) value_y = 15;
        if (value_y > graph_height - 50) value_y = graph_height - 50;

        // display value
        GazparDisplayInWh (true, true, arr_value[index], str_value, sizeof (str_value));
        WSContentSend_P (PSTR("<text class='%s value' x='%d' y='%d'>%s</text>\n"), str_type, value_x, value_y, str_value);

        // month name or day / hour number
        // -------------------------------

        // if full year, get month name else get day of month
        if (gazpar_graph.month == 0) GetTextIndexed (str_value, sizeof (str_value), index, kGazparYearMonth);
          else if (gazpar_graph.day == 0) sprintf (str_value, "%02d", index);
            else sprintf (str_value, "%dh", index - 1);

        // display
        value_y = graph_height - 10;
        WSContentSend_P (PSTR("<text class='%s' x='%d' y='%d'>%s</text>\n"), str_type, value_x, value_y, str_value);

        // week day name
        // -------------

        if ((gazpar_graph.month != 0) && (gazpar_graph.day == 0))
        {
          // calculate day name
          time_dst.day_of_month = index;
          time_bar = MakeTime (time_dst);
          BreakTime (time_bar, time_dst);
          day_of_week = (time_dst.day_of_week + 5) % 7;
          GetTextIndexed (str_value, sizeof (str_value), day_of_week, kGazparWeekDay2);

          // display
          value_y = graph_height - 30;
          WSContentSend_P (PSTR("<text class='%s' x='%d' y='%d'>%s</text>\n"), str_type, value_x, value_y, str_value);
        }
      }
    }

    // increment bar position
    graph_x     += graph_width;
    graph_x_end += graph_width;
  }
}

// Graph frame
void GazparWebGraphFrame ()
{
  long unit_max;
  char arr_label[5][12];

  // get maximum in Wh
  if (gazpar_graph.month == 0) unit_max = 1000 * gazpar_config.max_month;
    else if (gazpar_graph.day == 0) unit_max = 1000 * gazpar_config.max_day;
      else unit_max = 1000 * gazpar_config.max_hour;

  // generate scale values
  itoa (0, arr_label[0], 10);
  GazparDisplayInWh (false, true, unit_max / 4,     arr_label[1], sizeof (arr_label[1]));
  GazparDisplayInWh (false, true, unit_max / 2,     arr_label[2], sizeof (arr_label[2]));
  GazparDisplayInWh (false, true, unit_max * 3 / 4, arr_label[3], sizeof (arr_label[3]));
  GazparDisplayInWh (false, true, unit_max,         arr_label[4], sizeof (arr_label[4]));

  // graph frame
  WSContentSend_P (PSTR ("<rect x='%d%%' y='%d%%' width='%d%%' height='99.9%%' rx='10' />\n"), GAZPAR_GRAPH_PERCENT_START, 0, GAZPAR_GRAPH_PERCENT_STOP - GAZPAR_GRAPH_PERCENT_START);

  // graph separation lines
  WSContentSend_P (PSTR ("<line class='dash' x1='%d%%' y1='%d%%' x2='%d%%' y2='%d%%' />\n"), GAZPAR_GRAPH_PERCENT_START, 25, GAZPAR_GRAPH_PERCENT_STOP, 25);
  WSContentSend_P (PSTR ("<line class='dash' x1='%d%%' y1='%d%%' x2='%d%%' y2='%d%%' />\n"), GAZPAR_GRAPH_PERCENT_START, 50, GAZPAR_GRAPH_PERCENT_STOP, 50);
  WSContentSend_P (PSTR ("<line class='dash' x1='%d%%' y1='%d%%' x2='%d%%' y2='%d%%' />\n"), GAZPAR_GRAPH_PERCENT_START, 75, GAZPAR_GRAPH_PERCENT_STOP, 75);

  // units graduation
  WSContentSend_P (PSTR ("<text class='power' x='%d%%' y='%d%%'>%s</text>\n"), GAZPAR_GRAPH_PERCENT_STOP + 3, 2, "Wh");
  WSContentSend_P (PSTR ("<text class='power' x='%d%%' y='%d%%'>%s</text>\n"), 2, 2,  arr_label[4]);
  WSContentSend_P (PSTR ("<text class='power' x='%d%%' y='%d%%'>%s</text>\n"), 2, 26, arr_label[3]);
  WSContentSend_P (PSTR ("<text class='power' x='%d%%' y='%d%%'>%s</text>\n"), 2, 51, arr_label[2]);
  WSContentSend_P (PSTR ("<text class='power' x='%d%%' y='%d%%'>%s</text>\n"), 2, 76, arr_label[1]);
  WSContentSend_P (PSTR ("<text class='power' x='%d%%' y='%d%%'>%s</text>\n"), 2, 98, arr_label[0]);
}

// Graph public page
void GazparWebGraphPage ()
{
  int      index, period, histo, choice, counter;  
  long     percentage, delta;
  uint16_t year;
  float    value;
  char     str_text[16];
  char     str_date[16];
  char     str_file[32];
  char     str_filename[UFS_FILENAME_SIZE];

  // get numerical argument values
  gazpar_graph.histo = GazparWebGetArgValue (D_CMND_GAZPAR_HISTO, gazpar_graph.histo, 0, 100);
  gazpar_graph.month = GazparWebGetArgValue (D_CMND_GAZPAR_MONTH, gazpar_graph.month, 0, 12);
  gazpar_graph.day   = GazparWebGetArgValue (D_CMND_GAZPAR_DAY,   gazpar_graph.day,   0, 31);

  // check unit increment
  if (Webserver->hasArg (D_CMND_GAZPAR_INCREMENT)) 
  {
    if (gazpar_graph.month == 0) gazpar_config.max_month += GAZPAR_GRAPH_STEP_MONTH;
    else if (gazpar_graph.day == 0) gazpar_config.max_day += GAZPAR_GRAPH_STEP_DAY;
    else gazpar_config.max_hour += GAZPAR_GRAPH_STEP_HOUR;
  }

  // check unit decrement
  if (Webserver->hasArg (D_CMND_GAZPAR_DECREMENT)) 
  {
    if (gazpar_graph.month == 0) gazpar_config.max_month -= GAZPAR_GRAPH_STEP_MONTH;
    else if (gazpar_graph.day == 0) gazpar_config.max_day -= GAZPAR_GRAPH_STEP_DAY;
    else gazpar_config.max_hour -= GAZPAR_GRAPH_STEP_HOUR;
  }

  // check minimum values
  if (gazpar_config.max_month < GAZPAR_GRAPH_STEP_MONTH) gazpar_config.max_month = GAZPAR_GRAPH_STEP_MONTH;
  if (gazpar_config.max_day   < GAZPAR_GRAPH_STEP_DAY)   gazpar_config.max_day   = GAZPAR_GRAPH_STEP_DAY;
  if (gazpar_config.max_hour  < GAZPAR_GRAPH_STEP_HOUR)  gazpar_config.max_hour  = GAZPAR_GRAPH_STEP_HOUR;

  // beginning of form without authentification
  WSContentStart_P (D_GAZPAR_GRAPH, false);
  WSContentSend_P (PSTR ("</script>\n"));

  // set page as scalable
  WSContentSend_P (PSTR ("<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=yes'/>\n"));

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {margin:0.25rem auto;padding:0.1rem 0px;text-align:center;vertical-align:middle;}\n"));
  WSContentSend_P (PSTR ("div a {color:white;}\n"));

  WSContentSend_P (PSTR ("div.live {height:32px;}\n"));

  WSContentSend_P (PSTR ("div.choice {display:inline-block;padding:0px 2px;margin:0px;border:1px #666 solid;background:none;color:#fff;border-radius:6px;}\n"));
  WSContentSend_P (PSTR ("div.choice div {background:#666;}\n"));
  WSContentSend_P (PSTR ("div.choice a div {background:none;}\n"));

  WSContentSend_P (PSTR ("div.item {display:inline-block;margin:1px 0px;border-radius:4px;}\n"));
  WSContentSend_P (PSTR ("div a div.item:hover {background:#aaa;}\n"));

  WSContentSend_P (PSTR ("div.incr {position:absolute;top:5vh;left:2%%;padding:0px;color:white;border:1px #666 solid;border-radius:6px;}\n"));
  
  WSContentSend_P (PSTR ("div.year {width:44px;font-size:0.9rem;}\n"));
  WSContentSend_P (PSTR ("div.month {width:28px;font-size:0.85rem;}\n"));
  WSContentSend_P (PSTR ("div.day {width:16px;font-size:0.8rem;margin-top:2px;}\n"));
  
  WSContentSend_P (PSTR ("div.data {width:50px;}\n"));
  WSContentSend_P (PSTR ("div.size {width:25px;}\n"));
  WSContentSend_P (PSTR ("div a div.active {background:#666;}\n"));

  WSContentSend_P (PSTR ("div.graph {width:100%%;margin:auto;margin-top:2vh;}\n"));
  WSContentSend_P (PSTR ("svg.graph {width:100%%;height:60vh;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // page body
  WSContentSend_P (PSTR ("</head>\n"));
  WSContentSend_P (PSTR ("<body>\n"));


  // device name
  WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText(SET_DEVICENAME));

  // display icon
  WSContentSend_P (PSTR ("<div><a href='/'><img height=64 src='%s'></a></div>\n"), D_GAZPAR_ICON_PNG);

  // ----------------------
  //    Max value control
  // ----------------------

  WSContentSend_P (PSTR ("<div class='incr'>"));
  WSContentSend_P (PSTR ("<a href='%s?%s'><div class='item size'>+</div></a><br>"), D_GAZPAR_PAGE_GRAPH, D_CMND_GAZPAR_INCREMENT);
  WSContentSend_P (PSTR ("<a href='%s?%s'><div class='item size'>-</div></a>"), D_GAZPAR_PAGE_GRAPH, D_CMND_GAZPAR_DECREMENT);
  WSContentSend_P (PSTR ("</div>\n"));      // choice

  // -------------------
  //   Level 1 : Years
  // -------------------

  WSContentSend_P (PSTR ("<div><div class='choice'>\n"));

  for (counter = 9; counter >= 0; counter--)
  {
    // check if file exists
    if (GazparGetFilename (counter, str_filename))
    {
      // detect active year
      strcpy (str_text, "");
      if (gazpar_graph.histo == counter) strcpy (str_text, " active");

      // display year selection
      WSContentSend_P (PSTR ("<a href='%s?histo=%u&month=0&day=0'><div class='item year%s'>%u</div></a>\n"), D_GAZPAR_PAGE_GRAPH, index, str_text, RtcTime.year - counter);       
    }
  }

  WSContentSend_P (PSTR ("</div></div>\n"));        // choice

  // --------------------
  //   Level 2 : Months
  // --------------------

  WSContentSend_P (PSTR ("<div><div class='choice'>\n"));

  for (counter = 1; counter <= 12; counter++)
  {
    // get month name
    GetTextIndexed (str_date, sizeof (str_date), counter, kGazparYearMonth);

    // handle selected month
    strcpy (str_text, "");
    index = counter;
    if (gazpar_graph.month == counter)
    {
      strcpy_P (str_text, PSTR (" active"));
      if ((gazpar_graph.month != 0) && (gazpar_graph.day == 0)) index = 0;
    }

    // display month selection
    WSContentSend_P (PSTR ("<a href='%s?month=%u&day=0'><div class='item month%s'>%s</div></a>\n"), D_GAZPAR_PAGE_GRAPH, index, str_text, str_date);       
  }

  WSContentSend_P (PSTR ("</div></div>\n"));      // choice

  // --------------------
  //   Level 3 : Days
  // --------------------

  WSContentSend_P (PSTR ("<div class='live'>\n"));

  if (gazpar_graph.month != 0)
  {
    // calculate current year
    year = RtcTime.year - gazpar_graph.histo;

    // calculate number of days in current month
    if ((gazpar_graph.month == 4) || (gazpar_graph.month == 11) || (gazpar_graph.month == 9) || (gazpar_graph.month == 6)) choice = 30;     // months with 30 days  
    else if (gazpar_graph.month != 2) choice = 31;                                                                                          // months with 31 days
    else if ((year % 400) == 0) choice = 29;                                                                                                // leap year
    else if ((year % 100) == 0) choice = 28;                                                                                                // not a leap year
    else if ((year % 4) == 0) choice = 29;                                                                                                  // leap year
    else choice = 28;                                                                                                                       // not a leap year 

    WSContentSend_P (PSTR ("<div class='choice'>\n"));

    // loop thru days in the month
    for (counter = 1; counter <= choice; counter++)
    {
      // handle selected day
      strcpy (str_text, "");
      index = counter;
      if (gazpar_graph.day == counter) strcpy_P (str_text, PSTR (" active"));
      if ((gazpar_graph.day == counter) && (gazpar_graph.day != 0)) index = 0;

      // display day selection
      WSContentSend_P (PSTR ("<a href='%s?day=%u'><div class='item day%s'>%u</div></a>\n"), D_GAZPAR_PAGE_GRAPH, index, str_text, counter);
    }

    WSContentSend_P (PSTR ("</div>\n"));      // choice
  }

  WSContentSend_P (PSTR ("</div>\n"));      // live

  // ---------------
  //   SVG : Start 
  // ---------------

  WSContentSend_P (PSTR ("<div class='graph'>\n"));
  WSContentSend_P (PSTR ("<svg class='graph' viewBox='0 0 1200 %d' preserveAspectRatio='none'>\n"), gazpar_config.height);

  // ---------------
  //   SVG : Style 
  // ---------------

  WSContentSend_P (PSTR ("<style type='text/css'>\n"));

  WSContentSend_P (PSTR ("rect {stroke:grey;fill:none;}\n"));
  WSContentSend_P (PSTR ("line.dash {stroke:grey;stroke-width:1;stroke-dasharray:2 8;}\n"));
  WSContentSend_P (PSTR ("text.power {font-size:1.2rem;fill:white;}\n"));

  // bar graph
  WSContentSend_P (PSTR ("path {stroke-width:1.5;opacity:0.8;fill-opacity:0.6;}\n"));
  WSContentSend_P (PSTR ("path.now {stroke:#6cf;fill:#6cf;}\n"));
  if (gazpar_graph.day == 0) WSContentSend_P (PSTR ("path.now:hover {fill-opacity:0.8;}\n"));
  WSContentSend_P (PSTR ("path.prev {stroke:#069;fill:#069;fill-opacity:1;}\n"));

  // text
  WSContentSend_P (PSTR ("text {fill:white;text-anchor:middle;dominant-baseline:middle;}\n"));
  WSContentSend_P (PSTR ("text.value {font-style:italic;}}\n"));
  WSContentSend_P (PSTR ("text.month {font-size:1.2rem;}\n"));
  WSContentSend_P (PSTR ("text.day {font-size:0.9rem;}\n"));
  WSContentSend_P (PSTR ("text.hour {font-size:1rem;}\n"));

  // time line
  WSContentSend_P (PSTR ("line.time {stroke-width:1;stroke:white;stroke-dasharray:1 8;}\n"));

  WSContentSend_P (PSTR ("</style>\n"));

  // ----------------
  //   SVG : Curves
  // ----------------

  // display bar graph
  GazparWebDisplayBarGraph (gazpar_graph.histo + 1, false);       // previous period
  GazparWebDisplayBarGraph (gazpar_graph.histo,     true);        // current period

  // ---------------
  //   SVG : Frame
  // ---------------

  GazparWebGraphFrame ();

  // -----------------
  //   SVG : End 
  // -----------------

  WSContentSend_P (PSTR ("</svg>\n</div>\n"));

  // end of page
  WSContentStop ();
}

#endif  // USE_WEBSERVER

/***********************************\
 *            Interface
\***********************************/

// Teleinfo sensor
bool Xsns119 (uint8_t function)
{
  bool result = false;

  // swtich according to context
  switch (function) 
  {
    case FUNC_INIT:
      GazparInit ();
      break;
    case FUNC_SAVE_AT_MIDNIGHT:
      GazparMidnight ();
      break;
    case FUNC_SAVE_BEFORE_RESTART:
      GazparSaveBeforeRestart ();
      break;
    case FUNC_EVERY_SECOND:
      GazparEverySecond ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kGazparCommands, GazparCommand);
      break;
    case FUNC_JSON_APPEND:
      GazparShowJSON (true);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      // config
      Webserver->on (FPSTR (D_GAZPAR_PAGE_CONFIG), GazparWebPageConfigure);

      // graph
      Webserver->on (FPSTR (D_GAZPAR_PAGE_GRAPH), GazparWebGraphPage);

      // icons
      Webserver->on (FPSTR (D_GAZPAR_ICON_PNG), GazparWebIcon);
      break;

    case FUNC_WEB_ADD_BUTTON:
      GazparWebConfigButton ();
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      GazparWebMainButton ();
      break;
   case FUNC_WEB_SENSOR:
      GazparWebSensor ();
      break;
#endif  // USE_WEBSERVER
  }

  return result;
}

#endif      // USE_UFILESYS
#endif      // USE_GAZPAR
