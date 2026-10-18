/*
  xsns_98_00_sensor_data.ino - Data for generic temperature, humidity, presence and activity sensors

  Copyright (C) 2026  Nicolas Bernaerts
    08/01/2026 - v1.0 - Rewrite to separate data and historisation
  
  Use sens_help command to list available commands

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

#ifdef USE_SENSOR_GENERIC

#define SENSOR_DATA_VERSION           1

#define SENSOR_TEMP_VALIDITY          600           // temperature default validity (in sec.)
#define SENSOR_HUMI_VALIDITY          600           // humidity sensor default validity (in sec.)
#define SENSOR_POWR_VALIDITY          60            // power sensor default validity (in sec.)
#define SENSOR_PRES_VALIDITY          60            // presence default validity (in sec.)
#define SENSOR_ACTI_VALIDITY          5             // activity default validity (in sec.)

#define SENSOR_TEMP_DRIFT_MAX         10            // maximum temperature correction
#define SENSOR_TEMP_DRIFT_STEP        0.1           // temperature correction steps
#define SENSOR_MEASURE_UPDATE         10            // update sensor measures (in sec.)

#define SENSOR_PRESENCE_INDEX         0             // default presence counter index (0=Counter 1 ... 7=Counter 8)

#define D_SENSOR_PAGE_CONFIG          "sconf"
#define D_SENSOR_PAGE_HISTO           "shisto"

#define D_CMND_SENSOR_SENS            "sensor"
#define D_CMND_SENSOR_POWR            "power"
#define D_CMND_SENSOR_TEMP            "temp"
#define D_CMND_SENSOR_HUMI            "humi"
#define D_CMND_SENSOR_PRES            "pres"
#define D_CMND_SENSOR_TYPE            "type"

#define D_CMND_SENSOR_HELP            "help"
#define D_CMND_SENSOR_TOPIC           "topic"
#define D_CMND_SENSOR_KEY             "key"
#define D_CMND_SENSOR_TIMEOUT         "delay"
#define D_CMND_SENSOR_DRIFT           "drift"

#define D_SENSOR                      "Sensor"
#define D_SENSOR_CONFIGURE            "Configure"
#define D_SENSOR_HISTORY              "History"
#define D_SENSOR_MEASURE              "Measure"
#define D_SENSOR_TEMPERATURE          "Temperature"
#define D_SENSOR_HUMIDITY             "Humidity"
#define D_SENSOR_TIMEOUT              "Data validity"
#define D_SENSOR_PRESENCE             "Presence"
#define D_SENSOR_CORRECTION           "Correction"
#define D_SENSOR_TOPIC                "Topic"
#define D_SENSOR_KEY                  "JSON Key"

#define D_SENSOR_LOCAL                "Local"
#define D_SENSOR_REMOTE               "Remote"

// data colors
#define SENSOR_COLOR_TIME             "#888"
#define SENSOR_COLOR_HUMI             "#6bc4ff"
#define SENSOR_COLOR_TEMP             "#f39c12"
#define SENSOR_COLOR_PRES             "#080"
#define SENSOR_COLOR_ACTI             "#c00"
#define SENSOR_COLOR_POWR             "#fff"
#define SENSOR_COLOR_LINE             "#666"
 
// remote sensor sources
enum SensorSourcePower                { SENSOR_POWER_NONE, SENSOR_POWER_LOCAL, SENSOR_POWER_MAX };
enum SensorSourceTemp                 { SENSOR_TEMP_NONE,  SENSOR_TEMP_REMOTE, SENSOR_TEMP_DHT,   SENSOR_TEMP_SHT,    SENSOR_TEMP_DSB, SENSOR_TEMP_MAX };
enum SensorSourceHumi                 { SENSOR_HUMI_NONE,  SENSOR_HUMI_REMOTE, SENSOR_HUMI_DHT,   SENSOR_HUMI_SHT,    SENSOR_HUMI_MAX };
enum SensorSourcePres                 { SENSOR_PRES_NONE,  SENSOR_PRES_REMOTE, SENSOR_PRES_LOCAL, SENSOR_PRES_HILINK, SENSOR_PRES_MAX };
const char kSensorPresModel[] PROGMEM =      "None"    "|"  "Remote sensor" "|" "Binary sensor""|"     "Hilink"                        ;

// form topic style
const char SENSOR_FIELDSET_START[] PROGMEM = "<p><fieldset><legend>%s</legend>\n";
const char SENSOR_FIELDSET_STOP[]  PROGMEM = "</fieldset></p>\n";
const char SENSOR_FIELD_NUMBER[]   PROGMEM = "<p class='dat'><span class='hea'>%s</span><span class='val'><input type='number' name='%s' min='%d' max='%d' step='%d' value='%d'></span><span class='uni'>%s</span></p>\n";
const char SENSOR_FIELD_TEXT[]     PROGMEM = "<p class='dat'><span class='hea'>%s</span><span class='val'><input type='number' name='%s' min='%d' max='%d' step='%s' value='%s'></span><span class='uni'>%s</span></p>\n";
const char SENSOR_FIELD_CHECKBOX[] PROGMEM = "<p class='dat'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n";
const char SENSOR_FIELD_MQTT[]     PROGMEM = "<p class='dat'>%s<br><input class='left' name='%s' placeholder='%s' value='%s'></p>\n";

// week days name for history
static const char kWeekdayNames[] = D_DAY3LIST;

#ifdef USE_UFILESYS
const char D_SENSOR_FILENAME[] PROGMEM = "/sensor-%04u.csv";                     // sensor data file
#endif    // USE_UFILESYS

/*************************************************\
 *               Variables
\*************************************************/

typedef struct
{
  uint16_t validity;              // remote sensor data validity (in sec.)
} sensor_local;

typedef struct
{
  uint32_t validity;              // remote sensor data validity (in sec.)
  String   topic;                 // remote sensor topic
  String   key;                   // remote sensor key
} sensor_remote;

// configuration
struct {
  sensor_local  powr;             // local power
  sensor_remote temp;             // remote temperature mqtt
  sensor_remote humi;             // remote humidity mqtt
  sensor_remote pres;             // remote presence mqtt
} sensor_config;

// current status
typedef struct
{
  uint8_t  source;                // source of data (local sensor type)
//  bool     detected;              // current sensor value
//  uint32_t timeout;               // timeout of sensor reading
  uint32_t timelast;              // timestamp of last presence
  uint32_t suspend;               // timestamp to ignore sensor update
} sensor_pres;

typedef struct
{
//  bool     value;                 // current sensor value
//  bool     last;                  // last published sensor value
  uint32_t timelast;               // timeout of activity
} sensor_acti;

typedef struct
{
  uint8_t  source;                // source of data (local sensor type)
  uint32_t timeout;               // timeout of sensor reading
  float    value;                 // current sensor value
  float    last;                  // last published sensor value
} sensor_humi;

typedef struct
{
  uint8_t  source;                // source of data (local sensor type)
  uint32_t timeout;               // timeout of sensor reading
  float    value;                 // current sensor value
  float    last;                  // last published sensor value
} sensor_temp;

typedef struct
{
  uint8_t  source;                // source of data (local sensor type)
  float    value;                 // current sensor value
  float    last;                  // last published sensor value
//  uint32_t time_over;             // timeout of sensor reading
} sensor_powr;

// sensor data
struct {
  sensor_powr powr;               // power sensor data
  sensor_humi humi;               // humidity sensor data
  sensor_temp temp;               // temperature sensor data
  sensor_pres pres;               // presence sensor data
  sensor_acti acti;               // activity sensor data
} sensor_status;

// presence detection
volatile bool sensor_presence = false;              // current sensor value

#endif      // USE_SENSOR_GENERIC
