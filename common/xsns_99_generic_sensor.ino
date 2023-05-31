/*
  xsns_99_generic_sensor.ino - Generic temperature, humidity and presence sensors
    Reading is possible from local or remote (MQTT topic/key) sensors
    Module should be 99 as with platformio modules 100, 101, 102, ... are compiled before 99

  Copyright (C) 2020  Nicolas Bernaerts
    15/01/2020 - v1.0 - Creation with management of remote MQTT sensor
    23/04/2021 - v1.1 - Remove use of String to avoid heap fragmentation
    18/06/2021 - v1.2 - Bug fixes
    15/11/2021 - v1.3 - Add LittleFS management
    19/11/2021 - v1.4 - Handle drift and priority between local and remote sensors
    01/03/2022 - v2.0 - Complete rewrite to handle local drift and priority between local and remote sensors
    26/06/2022 - v3.0 - Tasmota 12 compatibility
                        Management of HLK-LD1115H and HLK-LD1125H sensors
    15/01/2023 - v3.1 - Add HLK-LD2410 sensor
    14/03/2023 - v3.2 - Change in configuration management
    25/04/2023 - v4.0 - Switch to drv & sns
                        Add presence and measure graph
    29/04/2023 - v4.1 - Add activity history and graph
    12/05/2023 - v4.2 - Save history in Settings strings
                        Add tweaked timing for SI7021

  Configuration values are stored in :
    - Settings->free_73A[0]  : temperature validity timeout (x10 in sec.)
    - Settings->free_73A[1]  : humidity validity timeout (x10 in sec.)
    - Settings->free_73A[2]  : presence validity timeout (x10 in sec.)
    - Settings->free_ea6[31] : type of presence sensor

  Remote MQTT settings are stored using Settings :
    * SET_SENSOR_TEMP_TOPIC           // remote temperature topic
    * SET_SENSOR_TEMP_KEY             // remote temperature key
    * SET_SENSOR_HUMI_TOPIC           // remote humidity topic
    * SET_SENSOR_HUMI_KEY             // remote humidity key
    * SET_SENSOR_PRES_TOPIC           // remote presence topic
    * SET_SENSOR_PRES_KEY             // remote presence key

  History data are stored either on the filesystem partition if available or using compressed strings in Settings :
    * SET_SENSOR_TEMP_WEEKLY 				  // temperature weekly history
    * SET_SENSOR_HUMI_WEEKLY          // humidity weekly history
    * SET_SENSOR_PRES_WEEKLY  				// presence weekly history
    * SET_SENSOR_ACTI_WEEKLY  				// activity weekly history
    * SET_SENSOR_INAC_WEEKLY  				// inactivity weekly history
    * SET_SENSOR_TEMP_YEARLY  				// temperature yearly history

  Handled local sensor are :
    * Temperature = DHT11, AM2301, SI7021, SHT30 or DS18x20
    * Humidity    = DHT11, AM2301, SI7021 or SHT30
    * Presence    = Generic presence/movement detector declared as Counter 1
                    HLK-LD1115H, HLK-LD1125H or HLK-LD2410 connected thru serial port (Rx and Tx)
  
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

#ifdef USE_GENERIC_SENSOR

#define XSNS_99                       99
#define XDRV_99                       99

#include <ArduinoJson.h>

#define SENSOR_TEMP_TIMEOUT           600           // temperature default validity (in sec.)
#define SENSOR_HUMI_TIMEOUT           600           // humidity default validity (in sec.)
#define SENSOR_PRES_TIMEOUT           60            // presence default validity (in sec.)

#define SENSOR_TEMP_DRIFT_MAX         10            // maximum temperature correction
#define SENSOR_TEMP_DRIFT_STEP        0.1           // temperature correction steps
#define SENSOR_MEASURE_UPDATE         10            // update sensor measures (in sec.)

#define SENSOR_PRESENCE_INDEX         0             // default presence counter index (0=Counter 1 ... 7=Counter 8)

#define SENSOR_YEAR_DAYS              366           // max number of days in year array

#define D_SENSOR_PAGE_CONFIG          "sconf"
#define D_SENSOR_PAGE_MEASURE         "cweek"
#define D_SENSOR_PAGE_PRESENCE        "pweek"
#define D_SENSOR_PAGE_YEARLY          "tyear"

#define D_CMND_SENSOR_SENS            "sens"
#define D_CMND_SENSOR_TEMP            "temp"
#define D_CMND_SENSOR_HUMI            "humi"
#define D_CMND_SENSOR_PRES            "pres"
#define D_CMND_SENSOR_ACTI            "acti"
#define D_CMND_SENSOR_INAC            "inac"

#define D_CMND_SENSOR_HELP            "help"
#define D_CMND_SENSOR_TOPIC           "topic"
#define D_CMND_SENSOR_KEY             "key"
#define D_CMND_SENSOR_TIMEOUT         "time"
#define D_CMND_SENSOR_WEEKLY          "week"
#define D_CMND_SENSOR_YEARLY          "year"
#define D_CMND_SENSOR_MAX             "max"
#define D_CMND_SENSOR_RESET           "reset"
#define D_CMND_SENSOR_DRIFT           "drift"
#define D_CMND_SENSOR_TYPE            "type"
#define D_CMND_SENSOR_LOAD            "load"
#define D_CMND_SENSOR_SAVE            "save"
#define D_CMND_SENSOR_RANDOM          "random"

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

#define SENSOR_COLOR_NONE             "#444"
#define SENSOR_COLOR_MOTION           "#F44"

// graph colors
#define SENSOR_COLOR_HUMI             "#6bc4ff"
#define SENSOR_COLOR_TEMP             "#f39c12"
#define SENSOR_COLOR_TEMP_MAX         "#f39c12"
#define SENSOR_COLOR_TEMP_MIN         "#5dade2"
#define SENSOR_COLOR_PRES             "#080"
#define SENSOR_COLOR_ACTI             "#fff"
#define SENSOR_COLOR_INAC             "#800"
#define SENSOR_COLOR_TIME             "#aaa"
#define SENSOR_COLOR_TODAY            "#fff"
#define SENSOR_COLOR_LINE             "#888"

// graph data
#define SENSOR_GRAPH_WIDTH            1200      
#define SENSOR_GRAPH_HEIGHT           600 
#define SENSOR_GRAPH_START            72           
#define SENSOR_GRAPH_STOP             1128      
#define SENSOR_GRAPH_STOP_PERCENT     94
#define SENSOR_GRAPH_ACTI             50  
#define SENSOR_GRAPH_INAC             50  

#define SENSOR_SIZE_FS                2  

#ifdef USE_UFILESYS
const uint8_t sensor_size_year_temp = SENSOR_SIZE_FS;      
const uint8_t sensor_size_week_temp = SENSOR_SIZE_FS;   
const uint8_t sensor_size_week_humi = SENSOR_SIZE_FS;   
const uint8_t sensor_size_week_pres = SENSOR_SIZE_FS;   
const uint8_t sensor_size_week_acti = SENSOR_SIZE_FS;   
const uint8_t sensor_size_week_inac = SENSOR_SIZE_FS;   
#else
const uint8_t sensor_size_year_temp = 126;      
const uint8_t sensor_size_week_temp = 86;   
const uint8_t sensor_size_week_humi = 85;   
const uint8_t sensor_size_week_pres = 48;   
const uint8_t sensor_size_week_acti = 48;   
const uint8_t sensor_size_week_inac = 48;   
#endif    // USE_UFILESYS


// sensor family type
enum SensorFamilyType { SENSOR_TYPE_TEMP, SENSOR_TYPE_HUMI, SENSOR_TYPE_PRES, SENSOR_TYPE_MAX };

// presence serial sensor type
enum SensorPresenceModel { SENSOR_PRESENCE_NONE, SENSOR_PRESENCE_REMOTE, SENSOR_PRESENCE_RCWL0516, SENSOR_PRESENCE_HWMS03, SENSOR_PRESENCE_LD1115, SENSOR_PRESENCE_LD1125, SENSOR_PRESENCE_LD2410, SENSOR_PRESENCE_MAX };
const char kSensorPresenceModel[] PROGMEM = "None|Remote|RCWL-0516|HW-MS03|HLK-LD1115|HLK-LD1125|HLK-LD2410|";

// remote sensor sources
enum SensorSource { SENSOR_SOURCE_DSB, SENSOR_SOURCE_DHT, SENSOR_SOURCE_SHT, SENSOR_SOURCE_COUNTER, SENSOR_SOURCE_SERIAL, SENSOR_SOURCE_REMOTE, SENSOR_SOURCE_NONE };

// remote sensor commands
const char kSensorCommands[] PROGMEM = D_CMND_SENSOR_SENS "_" "|" D_CMND_SENSOR_HELP "|" D_CMND_SENSOR_RESET "|" D_CMND_SENSOR_LOAD "|" D_CMND_SENSOR_SAVE "|" D_CMND_SENSOR_RANDOM;
void (* const SensorCommand[])(void) PROGMEM = { &CmndSensorHelp, &CmndSensorReset, &CmndSensorLoad, &CmndSensorSave, &CmndSensorRandom };

const char kTempCommands[] PROGMEM = D_CMND_SENSOR_TEMP "_" "|" D_CMND_SENSOR_TOPIC "|" D_CMND_SENSOR_KEY "|" D_CMND_SENSOR_TIMEOUT "|" D_CMND_SENSOR_WEEKLY "|" D_CMND_SENSOR_YEARLY "|" D_CMND_SENSOR_DRIFT;
void (* const TempCommand[])(void) PROGMEM = { &CmndSensorTemperatureTopic, &CmndSensorTemperatureKey, &CmndSensorTemperatureTimeout, &CmndSensorTemperatureWeekly, &CmndSensorTemperatureYearly, &CmndSensorTemperatureDrift };

const char kHumiCommands[] PROGMEM = D_CMND_SENSOR_HUMI "_" "|" D_CMND_SENSOR_TOPIC "|" D_CMND_SENSOR_KEY "|" D_CMND_SENSOR_TIMEOUT "|" D_CMND_SENSOR_WEEKLY;
void (* const HumiCommand[])(void) PROGMEM = { &CmndSensorHumidityTopic, &CmndSensorHumidityKey, &CmndSensorHumidityTimeout, &CmndSensorHumidityWeekly };

const char kPresCommands[] PROGMEM = D_CMND_SENSOR_PRES "_" "|" D_CMND_SENSOR_TOPIC "|" D_CMND_SENSOR_KEY "|" D_CMND_SENSOR_TIMEOUT "|" D_CMND_SENSOR_WEEKLY;
void (* const PresCommand[])(void) PROGMEM = { &CmndSensorPresenceTopic, &CmndSensorPresenceKey, &CmndSensorPresenceTimeout, &CmndSensorPresenceWeekly };

const char kActiCommands[] PROGMEM = D_CMND_SENSOR_ACTI "_" "|" D_CMND_SENSOR_WEEKLY;
void (* const ActiCommand[])(void) PROGMEM = { &CmndSensorActivityWeekly };

const char kInacCommands[] PROGMEM = D_CMND_SENSOR_INAC "_" "|" D_CMND_SENSOR_WEEKLY;
void (* const InacCommand[])(void) PROGMEM = { &CmndSensorInactivityWeekly };

// form topic style
const char SENSOR_FIELDSET_START[] PROGMEM = "<p><fieldset><legend><b>&nbsp;%s %s&nbsp;</b></legend>\n";
const char SENSOR_FIELDSET_STOP[]  PROGMEM = "</fieldset></p>\n";
const char SENSOR_FIELD_INPUT[]    PROGMEM = "<p>%s<span class='key'>%s</span><br><input name='%s' value='%s'></p>\n";
const char SENSOR_FIELD_CONFIG[]   PROGMEM = "<p>%s (%s)<span class='key'>%s</span><br><input type='number' name='%s' min='%d' max='%d' step='%s' value='%s'></p>\n";

// week days name for history
static const char kWeekdayNames[] = D_DAY3LIST;

#ifdef USE_UFILESYS
const char D_SENSOR_FILENAME_WEEK[] PROGMEM = "/sensor-week%02u.csv";           // sensor weekly history files
const char D_SENSOR_FILENAME_YEAR[] PROGMEM = "/sensor-year.csv";               // sensor yearly history file
#endif    // USE_UFILESYS

/*************************************************\
 *               Variables
\*************************************************/

// configuration
struct sensor_remote
{
  uint16_t validity;                          // remote sensor data validity (in sec.)
  String   topic;                             // remote sensor topic
  String   key;                               // remote sensor key
};
struct {
  sensor_remote temp;                         // remote temperature mqtt
  sensor_remote humi;                         // remote humidity mqtt
  sensor_remote pres;                         // remote presence mqtt
  uint8_t presence = SENSOR_PRESENCE_NONE;    // presence sensor type
} sensor_config;

// current status
typedef struct
{
  uint8_t  weekly;                            // flag to enable weekly historisation
} sensor_flag;
typedef struct
{
  uint8_t  weekly;                            // flag to enable weekly historisation
  uint8_t  source;                            // source of data (local sensor type)
  uint32_t timestamp;                         // last time sensor was updated
  int16_t  value;                             // current sensor value
  int16_t  last;                              // last published sensor value
} sensor_data;
struct {
  uint8_t  yearly      = false;               // flag to enable yearly historisation
  uint8_t  counter     = 0;                   // measure update counter
  uint32_t time_ignore = UINT32_MAX;          // timestamp to ignore sensor update
  uint32_t time_json   = UINT32_MAX;          // timestamp of next JSON update
  sensor_data temp;                           // temperature sensor data
  sensor_data humi;                           // humidity sensor data
  sensor_data pres;                           // presence sensor data
  sensor_flag acti;                           // activity sensor flag
  sensor_flag inac;                           // inactivity sensor flag
} sensor_status;

// history
typedef union {                               // Restricted by MISRA-C Rule 18.4 but so useful...
  uint8_t data;                               // Allow bit manipulation
  struct {
    uint8_t presence : 1;                     // presence detected
    uint8_t activity : 1;                     // activity declared
    uint8_t inactivity : 1;                   // inactivitydeclared
    uint8_t unused : 5;
  };
} sensor_event;
typedef struct
{
  int16_t      temp[7][24][6];                // temperature slots (every 10mn)
  int8_t       humi[7][24][6];                // humidity slots (every 10mn)
  sensor_event event[7][24][6];               // event slots : presence, activity and inactivity (every 10mn)
} sensor_histo;
sensor_histo sensor_week;                  
sensor_histo sensor_graph;

struct {
  int16_t temp_min[SENSOR_YEAR_DAYS];         // minimum daily temperature
  int16_t temp_max[SENSOR_YEAR_DAYS];         // maximum daily temperature
} sensor_year;

/***********************************************\
 *                  Commands
\***********************************************/

// timezone help
void CmndSensorHelp ()
{
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Sensor commands :"));

  AddLog (LOG_LEVEL_INFO, PSTR (" - sens_reset <w/y>   = reset history  (w:last week, y:last year)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - sens_load <w/y>    = load history   (w:last week, y:last year)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - sens_save <w/y>    = save history   (w:last week, y:last year)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - sens_random <w/y>  = random history (w:last week, y:last year)"));

  AddLog (LOG_LEVEL_INFO, PSTR (" Temperature :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - temp_topic <topic> = topic of remote sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - temp_key <key>     = key of remote sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - temp_drift <value> = correction (in 1/10 of °C)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - temp_time <value>  = remote sensor timeout"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - temp_week <0/1>    = weekly histo. (%u bytes)"), sensor_size_week_temp);
  AddLog (LOG_LEVEL_INFO, PSTR (" - temp_year <0/1>    = yearly histo. (%u bytes)"), sensor_size_year_temp);

  AddLog (LOG_LEVEL_INFO, PSTR (" Humidity :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - humi_topic <topic> = topic of remote sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - humi_key <key>     = key of remote sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - humi_time <value>  = remote sensor timeout"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - humi_week <0/1>    = weekly histo. (%u bytes)"), sensor_size_week_humi);

  AddLog (LOG_LEVEL_INFO, PSTR (" Presence :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - pres_topic <topic> = topic of remote sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - pres_key <key>     = key of remote sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - pres_time <value>  = remote sensor timeout"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - pres_week <0/1>    = weekly histo. (%u bytes)"), sensor_size_week_pres);

  AddLog (LOG_LEVEL_INFO, PSTR (" Activity :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - acti_week <0/1>    = weekly histo. of activity (%u bytes)"), sensor_size_week_acti);
  AddLog (LOG_LEVEL_INFO, PSTR (" - inac_week <0/1>    = weekly histo. of inactivity (%u bytes)"), sensor_size_week_inac);

  ResponseCmndDone();
}

void CmndSensorReset ()
{
  uint16_t day;

  switch (XdrvMailbox.data[0])
  {
    // week
    case 'w':
      for (day = 0; day < 7; day++) SensorResetWeekDay (day);
      ResponseCmndDone ();
      break;
    // year
    case 'y':
      for (day = 0; day < SENSOR_YEAR_DAYS; day++) SensorResetYearDay (day);
      ResponseCmndDone ();
      break;
    // error
    default:
      ResponseCmndFailed ();
      break;
  }
}

void CmndSensorLoad ()
{
  switch (XdrvMailbox.data[0])
  {
    // week
    case 'w':
      SensorLoadWeek ();
      ResponseCmndDone ();
      break;
    // year
    case 'y':
      SensorLoadYear ();
      ResponseCmndDone ();
      break;
    // error
    default:
      ResponseCmndFailed ();
      break;
  }
}

void CmndSensorSave ()
{
  switch (XdrvMailbox.data[0])
  {
    // week
    case 'w':
      SensorSaveWeek (true);
      ResponseCmndDone ();
      break;
    // year
    case 'y':
      SensorSaveYear ();
      ResponseCmndDone ();
      break;
    // error
    default:
      ResponseCmndFailed ();
      break;
  }
}

void CmndSensorRandom ()
{
  uint16_t day, hour, slot;
  int8_t   ref_humi;
  int16_t  ref_min, ref_max;

  switch (XdrvMailbox.data[0])
  {
    // week
    case 'w':
      ref_max  = 180;
      ref_humi = 70;
      for (day = 0; day < 7; day ++)
        for (hour = 0; hour < 24; hour ++)
          for (slot = 0; slot < 6; slot ++)
          {
            // temperature
            ref_max += 3 - random (7); 
            sensor_week.temp[day][hour][slot] = ref_max;

            // humidity
            ref_humi += 2 - (int8_t)random (5); 
            if (ref_humi>100) ref_humi=100; 
            if (ref_humi<0) ref_humi=0; 
            sensor_week.humi[day][hour][slot] = ref_humi;
          }
      ResponseCmndDone ();
      break;
    // year
    case 'y':
      ref_max = 180;
      ref_min = 160;
      for (day = 0; day < SENSOR_YEAR_DAYS; day ++)
      {
        ref_max += 3 - random (7);
        ref_min = min (ref_max - 20, ref_min + 20 - (int16_t)random (40));
        sensor_year.temp_max[day] = ref_max;
        sensor_year.temp_min[day] = ref_min;
      }
      ResponseCmndDone ();
      break;
    // error
    default:
      ResponseCmndFailed ();
      break;
  }
}

void CmndSensorTemperatureDrift ()
{
  float drift;

  if (XdrvMailbox.data_len > 0)
  {
    drift = atof (XdrvMailbox.data) * 10;
    Settings->temp_comp = (int8_t)drift;
  }
  drift = (float)Settings->temp_comp / 10;
  ResponseCmndFloat (drift, 1);
}

void CmndSensorTemperatureTopic ()
{
  char str_text[64];

  if (XdrvMailbox.data_len > 0)
  {
    strlcpy (str_text, XdrvMailbox.data, sizeof (str_text) - 1);
    sensor_config.temp.topic = str_text;
    SensorSaveConfig ();
  }
  ResponseCmndChar (sensor_config.temp.topic.c_str ());
}

void CmndSensorTemperatureKey ()
{
  char str_text[32];

  if (XdrvMailbox.data_len > 0)
  {
    strlcpy (str_text, XdrvMailbox.data, sizeof (str_text) - 1);
    sensor_config.temp.key = str_text;
    SensorSaveConfig ();
  }
  ResponseCmndChar (sensor_config.temp.key.c_str ());
}

void CmndSensorTemperatureTimeout ()
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload > 0)) sensor_config.temp.validity = (uint16_t)XdrvMailbox.payload; 

  ResponseCmndNumber (sensor_config.temp.validity);
}

void CmndSensorTemperatureWeekly ()
{
  if (XdrvMailbox.data_len > 0)
  {
    sensor_status.temp.weekly = (uint8_t)XdrvMailbox.payload; 
    SensorTemperatureSaveWeek ();
  }
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Size is %u/%u"), GetSettingsTextLen (), settings_text_size);
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Histo takes %u bytes"), sensor_size_week_temp);
  ResponseCmndNumber (sensor_status.temp.weekly);
}

void CmndSensorTemperatureYearly ()
{
  if (XdrvMailbox.data_len > 0)
  {
    sensor_status.yearly = (uint8_t)XdrvMailbox.payload; 
    SensorSaveYear ();
  }
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Size is %u/%u"), GetSettingsTextLen (), settings_text_size);
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Histo takes %u bytes"), sensor_size_year_temp);
  ResponseCmndNumber (sensor_status.yearly);
}

void CmndSensorHumidityTopic ()
{
  char str_text[64];

  if (XdrvMailbox.data_len > 0)
  {
    strlcpy (str_text, XdrvMailbox.data, sizeof (str_text) - 1);
    sensor_config.humi.topic = str_text;
    SensorSaveConfig ();
  }
  ResponseCmndChar (sensor_config.humi.topic.c_str ());
}

void CmndSensorHumidityKey ()
{
  char str_text[32];

  if (XdrvMailbox.data_len > 0)
  {
    strlcpy (str_text, XdrvMailbox.data, sizeof (str_text) - 1);
    sensor_config.humi.key = str_text;
    SensorSaveConfig ();
  }
  ResponseCmndChar (sensor_config.humi.key.c_str ());
}

void CmndSensorHumidityTimeout ()
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload > 0)) sensor_config.humi.validity = (uint16_t)XdrvMailbox.payload; 

  ResponseCmndNumber (sensor_config.humi.validity);
}

void CmndSensorHumidityWeekly ()
{
  if (XdrvMailbox.data_len > 0)
  {
    sensor_status.humi.weekly = (uint8_t)XdrvMailbox.payload; 
    SensorHumiditySaveWeek ();
  }
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Size is %u/%u"), GetSettingsTextLen (), settings_text_size);
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Histo takes %u bytes"), sensor_size_week_humi);
  ResponseCmndNumber (sensor_status.humi.weekly);
}

void CmndSensorPresenceTopic ()
{
  char str_text[64];

  if (XdrvMailbox.data_len > 0)
  {
    strlcpy (str_text, XdrvMailbox.data, sizeof (str_text) - 1);
    sensor_config.pres.topic = str_text;
    SensorSaveConfig ();
  }
  ResponseCmndChar (sensor_config.pres.topic.c_str ());
}

void CmndSensorPresenceKey ()
{
  char str_text[32];

  if (XdrvMailbox.data_len > 0)
  {
    strlcpy (str_text, XdrvMailbox.data, sizeof (str_text) - 1);
    sensor_config.pres.key = str_text;
    SensorSaveConfig ();
  }
  ResponseCmndChar (sensor_config.pres.key.c_str ());
}

void CmndSensorPresenceTimeout ()
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload > 0)) sensor_config.pres.validity = (uint16_t)XdrvMailbox.payload; 

  ResponseCmndNumber (sensor_config.pres.validity);
}

void CmndSensorPresenceWeekly ()
{
  if (XdrvMailbox.data_len > 0)
  {
    sensor_status.pres.weekly = (uint8_t)XdrvMailbox.payload; 
    SensorPresenceSaveWeek ();
  }
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Size is %u/%u"), GetSettingsTextLen (), settings_text_size);
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Histo takes %u bytes"), sensor_size_week_pres);
  ResponseCmndNumber (sensor_status.pres.weekly);
}

void CmndSensorActivityWeekly ()
{
  if (XdrvMailbox.data_len > 0)
  {
    sensor_status.acti.weekly = (uint8_t)XdrvMailbox.payload; 
    SensorActivitySaveWeek ();
  }
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Size is %u/%u"), GetSettingsTextLen (), settings_text_size);
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Histo takes %u bytes"), sensor_size_week_acti);
  ResponseCmndNumber (sensor_status.acti.weekly);
}

void CmndSensorInactivityWeekly ()
{
  if (XdrvMailbox.data_len > 0)
  {
    sensor_status.inac.weekly = (uint8_t)XdrvMailbox.payload; 
    SensorInactivitySaveWeek ();
  }
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Size is %u/%u"), GetSettingsTextLen (), settings_text_size);
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Histo takes %u bytes"), sensor_size_week_inac);
  ResponseCmndNumber (sensor_status.inac.weekly);
}

/**************************************************\
 *                  Functions
\**************************************************/

// get current week number (1 to 52, week starting on monday)
uint16_t SensorGetCurrentWeek ()
{
  uint16_t shift, week;

  shift = (8 - RtcTime.day_of_week + (RtcTime.day_of_year % 7)) % 7;
  week  = (6 + RtcTime.day_of_year - shift) / 7;
  if (week == 0) week = 52;

  return week;
}

// get current week label dd/mm - dd/mm
void SensorGetWeekLabel (const uint32_t shift_week, char* pstr_label, size_t size_label)
{
  uint16_t day_shift;
  uint32_t day_time;
  TIME_T   start_dst, stop_dst;

  // check parameters
  if (pstr_label == nullptr) return;
  if (size_label < 16) return;

  // if current week
  if (shift_week == 0) strcpy (pstr_label, "Last 7 days");

  // else calculate week boundaries
  else
  {
    // calculate start day
    day_shift = (7 + RtcTime.day_of_week - 2) % 7;
    day_time = LocalTime () - day_shift * 86400 - shift_week * 604800;
    BreakTime (day_time, start_dst);

    // calculate stop day
    day_time += 6 * 86400;
    BreakTime(day_time, stop_dst);

    // generate string
    sprintf_P (pstr_label, PSTR("%02u/%02u - %02u/%02u"), start_dst.day_of_month, start_dst.month, stop_dst.day_of_month, stop_dst.month);
  }
}

void SensorGetDelayText (const uint32_t timestamp, char* pstr_result, size_t size_result)
{
  uint32_t time_now, time_delay;

  // check parameters
  if (pstr_result == nullptr) return;
  if (size_result < 12) return;

  // calculate delay
  time_now = LocalTime ();
  if ((timestamp == 0) || (time_now == 0) || (time_now == UINT32_MAX)) time_delay = UINT32_MAX;
  else if (timestamp > time_now) time_delay = UINT32_MAX;
  else time_delay = time_now - timestamp;

  // set unit according to value
  if (time_delay == UINT32_MAX) strcpy (pstr_result, "---");
  else if (time_delay >= 86400) sprintf (pstr_result, "%ud %uh", time_delay / 86400, (time_delay % 86400) / 3600);
  else if (time_delay >= 36000) sprintf (pstr_result, "%uh", time_delay / 3600);
  else if (time_delay >= 3600) sprintf (pstr_result, "%uh %umn", time_delay / 3600, (time_delay % 3600) / 60);
  else if (time_delay >= 600) sprintf (pstr_result, "%umn", time_delay / 60);
  else if (time_delay >= 60) sprintf (pstr_result, "%umn %us", time_delay / 60, time_delay % 60);
  else if (time_delay > 0) sprintf (pstr_result, "%us", time_delay);
  else strcpy (pstr_result, "now");
}

// -----------------
//    Temperature
// -----------------

// read temperature value
float SensorTemperatureGet () { return SensorTemperatureGet (0); }
float SensorTemperatureGet (uint32_t timeout)
{
  float result;

  if (sensor_status.temp.value == INT16_MAX) result = NAN;
    else if (timeout == 0) result = (float)sensor_status.temp.value / 10;
    else if (sensor_status.temp.timestamp == UINT32_MAX) result = NAN;
    else if (Rtc.local_time > sensor_status.temp.timestamp + sensor_config.temp.validity) result = NAN;
    else result = (float)sensor_status.temp.value / 10;

  return result;
}

// set current temperature
void SensorTemperatureSet (const float temperature)
{
  uint8_t  day, hour, slot;
  uint16_t index, new_temp;

  // check validity
  if (isnan (temperature)) return;
  if (!RtcTime.valid) return;

  // update current value and timestamp
  sensor_status.temp.value = (int16_t)(temperature * 10);
  sensor_status.temp.timestamp = LocalTime ();

  // log
  AddLog (LOG_LEVEL_DEBUG, PSTR ("SEN: Temperature %u.%u°C"), sensor_status.temp.value / 10, sensor_status.temp.value % 10);

  if (!RtcTime.valid) return;

  // update weekly slot
  day  = RtcTime.day_of_week - 1;
  hour = RtcTime.hour;
  slot = RtcTime.minute / 10; 
  if ((day < 7) && (hour < 24) && (slot < 6))
  {
    if (sensor_week.temp[day][hour][slot] == INT16_MAX) sensor_week.temp[day][hour][slot] = sensor_status.temp.value;
      else sensor_week.temp[day][hour][slot] = max (sensor_week.temp[day][hour][slot], sensor_status.temp.value);
  }

  // update yearly slot
  index = RtcTime.day_of_year - 1;
  if (index < SENSOR_YEAR_DAYS)
  {
    // maximum value
    if (sensor_year.temp_max[index] == INT16_MAX) sensor_year.temp_max[index] = sensor_status.temp.value;
      else sensor_year.temp_max[index] = max (sensor_year.temp_max[index], sensor_status.temp.value);

    // minimum value
    if (sensor_year.temp_min[index] == INT16_MAX) sensor_year.temp_min[index] = sensor_status.temp.value;
      else sensor_year.temp_min[index] = min (sensor_year.temp_min[index], sensor_status.temp.value);
  }
}

// calculate new temperature from a delta coded on 4 bits : N/A, -5, -3, -2, -1.5, -1, -0.5, 0, +0.5, +1, +1.5, +2, N/A, +3, +5, NAN
int16_t SensorTemperatureCalculateFromDelta (const int16_t temp_ref, const uint8_t delta)
{
  int16_t result = INT16_MAX;

  // calculate hour temperature
  switch (delta)
  {
    case 0x01: result = temp_ref - 50; break;        // -5.0 °C
    case 0x02: result = temp_ref - 30; break;        // -3.0 °C
    case 0x03: result = temp_ref - 20; break;        // -2.0 °C
    case 0x04: result = temp_ref - 15; break;        // -1.5 °C
    case 0x05: result = temp_ref - 10; break;        // -1.0 °C
    case 0x06: result = temp_ref - 5;  break;        // -0.5 °C
    case 0x07: result = temp_ref;      break;        // +0.0 °C
    case 0x08: result = temp_ref + 5;  break;        // +0.5 °C
    case 0x09: result = temp_ref + 10; break;        // +1.0 °C
    case 0x0A: result = temp_ref + 15; break;        // +1.5 °C
    case 0x0B: result = temp_ref + 20; break;        // +2.0 °C
    case 0x0D: result = temp_ref + 30; break;        // +3.0 °C
    case 0x0E: result = temp_ref + 50; break;        // +5.0 °C
  }

  return result;
}

// calculate a temperature delta coded on 4 bits : N/A, -5, -3, -2, -1.5, -1, -0.5, 0, +0.5, +1, +1.5, +2, N/A, +3, +5, NAN
//  reference temperature is updated according to calculated delta
uint8_t SensorTemperatureCalculateDelta (int16_t &temp_ref, const int16_t temp_delta)
{
  uint8_t result;

  if (temp_delta == INT16_MAX)  result = 0x0F;                        // N/A
  else if (temp_delta >= 50)  { result = 0x0E; temp_ref += 50; }      // +5.0 °C
  else if (temp_delta >= 30)  { result = 0x0D; temp_ref += 30; }      // +3.0 °C
  else if (temp_delta >= 20)  { result = 0x0B; temp_ref += 20; }      // +2.0 °C
  else if (temp_delta >= 15)  { result = 0x0A; temp_ref += 15; }      // +1.5 °C
  else if (temp_delta >= 10)  { result = 0x09; temp_ref += 10; }      // +1.0 °C
  else if (temp_delta >= 5)   { result = 0x08; temp_ref += 5;  }      // +0.5 °C
  else if (temp_delta >= 0)     result = 0x07;                        // +0.0 °C
  else if (temp_delta >= -5)  { result = 0x06; temp_ref -= 5;  }      // -0.5 °C
  else if (temp_delta >= -10) { result = 0x05; temp_ref -= 10; }      // -1.0 °C
  else if (temp_delta >= -15) { result = 0x04; temp_ref -= 15; }      // -1.5 °C
  else if (temp_delta >= -20) { result = 0x03; temp_ref -= 20; }      // -2.0 °C
  else if (temp_delta >= -30) { result = 0x02; temp_ref -= 30; }      // -3.0 °C
  else                        { result = 0x01; temp_ref -= 50; }      // -5.0 °C

  return result;
}

// reset temperature for specific week day (0=sunday ... 6=saturday)
void SensorTemperatureResetWeekDay (const uint8_t day)
{
  uint8_t hour, slot;
  char    str_day[4];

  // check parameters
  if (day >= 7) return;

  // reset daily slots
  for (hour = 0; hour < 24; hour ++)
    for (slot = 0; slot < 6; slot ++) sensor_week.temp[day][hour][slot] = INT16_MAX;

  // log
  strlcpy (str_day, kWeekdayNames + day * 3, 4);
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Temperature - Reset %s"), str_day);
}

// load weekly temperature history from settings string
//   byte 0       = if first temperature is negative, abs (temperature) else 0xFF
//   byte 1       = if first temperature is positive, temperature else 0xFF
//   byte 2..85   = delta per hour on 4 bits (N/A, -5, -3, -2, -1.5, -1, -0.5, 0, +0.5, +1, +1.5, +2, N/A, +3, +5, NAN)
bool SensorTemperatureLoadWeek ()
{
  uint8_t index, day, hour, slot, value;
  int16_t temp_hour, temp_ref;
  char    str_history[88];

  // reset temperature data to N/A
  for (day = 0; day < 7; day ++)
    for (hour = 0; hour < 24; hour ++)
      for (slot = 0; slot < 6; slot ++) sensor_week.temp[day][hour][slot] = INT16_MAX;

  // check if history is available
  strlcpy (str_history, SettingsText (SET_SENSOR_TEMP_WEEKLY), sizeof (str_history));
  value = strlen (str_history);
  if ( value > 0) sensor_status.temp.weekly = 1;
    else sensor_status.temp.weekly = 0;
  if ((value == SENSOR_SIZE_FS) || (value != sensor_size_week_temp)) return false;

  // calculate reference temperature
  temp_ref = INT16_MAX;
  if (str_history[0] != 0xFF) temp_ref = 0 - (int16_t)str_history[0] * 5;
    else if (str_history[1] != 0xFF) temp_ref = (int16_t)(str_history[1] - 1) * 5;
  if (temp_ref == INT16_MAX) return false;

  // update sensor history string
  index = 2;
  for (day = 0; day < 7; day ++)
    for (hour = 0; hour < 24; hour ++)
    {
      // load history delta
      if (hour % 2 == 0) value = str_history[index] & 0x0F;
        else value = str_history[index++] >> 4;

      // calculate hour temperature
      temp_hour = SensorTemperatureCalculateFromDelta (temp_ref, value);

      // update hour slots
      for (slot = 0; slot < 6; slot ++) sensor_week.temp[day][hour][slot] = temp_hour;

      // update reference value
      if (temp_hour != INT16_MAX) temp_ref = temp_hour;
    }

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Temperature - Loaded from settings"));

  return true;
}

// save weekly temperature history as settings string
//   byte 0       = if first temperature is negative, abs (temperature) else 0xFF
//   byte 1       = if first temperature is positive, temperature else 0xFF
//   byte 2..85   = delta per hour on 4 bits (N/A, -5, -3, -2, -1.5, -1, -0.5, 0, +0.5, +1, +1.5, +2, N/A, +3, +5, NAN)
void SensorTemperatureSaveWeek ()
{
  uint8_t index, part, day, hour, slot, value;
  char    str_history[88];
  int16_t delta, ref_temp, hour_temp;

  // if no temperature weekly history
  SettingsUpdateText (SET_SENSOR_TEMP_WEEKLY, "");
  if (sensor_status.temp.weekly == 0) return;

#ifdef USE_UFILESYS
  // if weekly history saved on filesystem
  SettingsUpdateText (SET_SENSOR_TEMP_WEEKLY, "fs");
  return;

#else
  // check settings size availability
  if (settings_text_size - GetSettingsTextLen () < sensor_size_week_temp)
  {
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Temperature - No space to save to settings"));
    sensor_status.temp.weekly = 0;
    return;
  }

  // loop thru temperature slots to get first reference temperature
  ref_temp = INT16_MAX;  
  for (day = 0; day < 7; day ++)
    for (hour = 0; hour < 24; hour ++)
      for (slot = 0; slot < 6; slot ++) if (ref_temp == INT16_MAX) ref_temp = sensor_week.temp[day][hour][slot];

  // init history string
  for (index = 0; index < 86; index ++) str_history[index] = 0xFF;

  // write reference temperature
  if (ref_temp < 0) str_history[0] = (uint8_t)((0 - ref_temp) / 5);
    else if (ref_temp != INT16_MAX) str_history[1] = (uint8_t)(ref_temp / 5) + 1;

  // loop thru temperature slots
  index = 2;
  if (ref_temp != INT16_MAX) 
    for (day = 0; day < 7; day ++)
      for (hour = 0; hour < 24; hour ++)
      {
        // get max temperature in the hour slots
        hour_temp = sensor_week.temp[day][hour][0];
        for (slot = 1; slot < 6; slot ++)
          if (hour_temp == INT16_MAX) hour_temp = sensor_week.temp[day][hour][slot];
            else if ((sensor_week.temp[day][hour][slot] != INT16_MAX) && (hour_temp < sensor_week.temp[day][hour][slot])) hour_temp = sensor_week.temp[day][hour][slot];

        // define increment of decrement
        if (hour_temp == INT16_MAX) delta = INT16_MAX;
          else delta = hour_temp - ref_temp;
        
        // calculate delta according to temperature difference
        value = SensorTemperatureCalculateDelta (ref_temp, delta);

        // update history string
        if (hour % 2 == 0) str_history[index] = value;
          else str_history[index] = str_history[index++] | (value << 4);
      }

  // save history string
  str_history[sensor_size_week_temp] = 0;
  SettingsUpdateText (SET_SENSOR_TEMP_WEEKLY, str_history);

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Temperature - Saved to settings"));
#endif    // USE_UFILESYS
}

// load yearly temperature history from settings string
//   byte 0       = if first min temperature is negative, abs (min temperature) else 0xFF
//   byte 1       = if first min temperature is positive, min temperature else 0xFF
//   byte 2       = if first max temperature is negative, abs (max temperature) else 0xFF
//   byte 3       = if first max temperature is positive, max temperature else 0xFF
//   byte 4..125  = delta per every 3 days on 4 bits
//                  first half byte is min temperature evolution
//                  second half byte is max temperature evolution
bool SensorTemperatureLoadYear ()
{
  uint8_t  value;
  uint16_t index, day;
  int16_t  temp_min, temp_max, temp_ref;
  char     str_history[128];

  // reset temperature data to N/A
  for (day = 0; day < SENSOR_YEAR_DAYS; day ++)
  {
    sensor_year.temp_min[day] = INT16_MAX;
    sensor_year.temp_max[day] = INT16_MAX;
  }

  // check if history is available
  strlcpy (str_history, SettingsText (SET_SENSOR_TEMP_YEARLY), sizeof (str_history));
  value = strlen (str_history);
  if (value > 0) sensor_status.yearly = 1;
    else sensor_status.yearly = 0;
  if ((value == SENSOR_SIZE_FS) || (value != sensor_size_year_temp)) return false;

  // calculate reference minimum temperature
  temp_min = INT16_MAX;
  if (str_history[0] != 0xFF) temp_min = 0 - (int16_t)str_history[0] * 5;
    else if (str_history[1] != 0xFF) temp_min = (int16_t)(str_history[1] - 1) * 5;
  if (temp_min == INT16_MAX) return false;

  // calculate reference maximum temperature
  temp_max = INT16_MAX;
  if (str_history[2] != 0xFF) temp_max = 0 - (int16_t)str_history[2] * 5;
    else if (str_history[3] != 0xFF) temp_max = (int16_t)(str_history[3] - 1) * 5;
  if (temp_max == INT16_MAX) return false;

  // update sensor history string
  for (index = 0; index < 122; index ++)
  {
    // calculate minimum temperature evolution
    value = str_history[index + 4] & 0x0F;
    temp_ref = SensorTemperatureCalculateFromDelta (temp_min, value);
    for (day = index * 3; day < index * 3 + 3; day ++) sensor_year.temp_min[day] = temp_ref;
    if (temp_ref != INT16_MAX) temp_min = temp_ref;

    // calculate maximum temperature evolution
    value = str_history[index + 4] >> 4;
    temp_ref = SensorTemperatureCalculateFromDelta (temp_max, value);
    for (day = index * 3; day < index * 3 + 3; day ++) sensor_year.temp_max[day] = temp_ref;
    if (temp_ref != INT16_MAX) temp_max = temp_ref;
  }

  return true;
}

// save yearly temperature history to settings string
//   byte 0       = if first min temperature is negative, abs (min temperature) else 0xFF
//   byte 1       = if first min temperature is positive, min temperature else 0xFF
//   byte 2       = if first max temperature is negative, abs (max temperature) else 0xFF
//   byte 3       = if first max temperature is positive, max temperature else 0xFF
//   byte 4..125  = delta per every 3 days on 4 bits
//                  first half byte is min temperature evolution
//                  second half byte is max temperature evolution
void SensorTemperatureSaveYear ()
{
  uint8_t  value;
  uint16_t index, day;
  int16_t  temp_min, temp_max, temp_ref, delta;
  char     str_history[128];

  // if no temperature weekly history
  SettingsUpdateText (SET_SENSOR_TEMP_YEARLY, "");
  if (sensor_status.yearly == 0) return;

#ifdef USE_UFILESYS

  // if weekly history saved on filesystem
  SettingsUpdateText (SET_SENSOR_TEMP_YEARLY, "fs");

#else

  // check settings size availability
  if (settings_text_size - GetSettingsTextLen () < sensor_size_year_temp)
  {
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Temperature - No space to save to settings"));
    sensor_status.yearly = 0;
    return;
  }


  // loop thru temperature slots to get first minimum and maximum temperature
  temp_min = temp_max = INT16_MAX;  
  for (index = 0; index < SENSOR_YEAR_DAYS; index ++)
  {
    if (temp_min == INT16_MAX) temp_min = sensor_year.temp_min[index];
    if (temp_max == INT16_MAX) temp_max = sensor_year.temp_max[index];
  }

  // init history string
  for (index = 0; index < 126; index ++) str_history[index] = 0xFF;

  // write first minimum and maximum temperature
  if (temp_min < 0) str_history[0] = (uint8_t)((0 - temp_min) / 5);
    else if (temp_min != INT16_MAX) str_history[1] = (uint8_t)(temp_min / 5) + 1;
  if (temp_max < 0) str_history[2] = (uint8_t)((0 - temp_max) / 5);
    else if (temp_max != INT16_MAX) str_history[3] = (uint8_t)(temp_max / 5) + 1;

  // update sensor history string
  for (index = 0; index < 122; index ++)
  {
    // calculate minimum temperature evolution
    temp_ref = INT16_MAX;
    for (day = index * 3; day < index * 3 + 3; day ++)
      if (temp_ref == INT16_MAX) temp_ref = sensor_year.temp_min[day];
        else if ((sensor_year.temp_min[day] != INT16_MAX) && (temp_ref > sensor_year.temp_min[day])) temp_ref = sensor_year.temp_min[day];

    // define evolution
    if (temp_ref == INT16_MAX) delta = INT16_MAX;
      else delta = temp_ref - temp_min;
    
    // calculate delta according to temperature difference
    value = SensorTemperatureCalculateDelta (temp_min, delta);
    str_history[index + 4] = value;

    // calculate maximum temperature evolution
    temp_ref = INT16_MAX;
    for (day = index * 3; day < index * 3 + 3; day ++)
      if (temp_ref == INT16_MAX) temp_ref = sensor_year.temp_max[day];
        else if ((sensor_year.temp_max[day] != INT16_MAX) && (temp_ref < sensor_year.temp_max[day])) temp_ref = sensor_year.temp_max[day];

    // define increment of decrement
    if (temp_ref == INT16_MAX) delta = INT16_MAX;
      else delta = temp_ref - temp_max;
    
    // calculate delta according to temperature difference
    value = SensorTemperatureCalculateDelta (temp_max, delta);
    str_history[index + 4] = str_history[index + 4] | (value << 4);
  }

  // save history string
  str_history[sensor_size_year_temp] = 0;
  SettingsUpdateText (SET_SENSOR_TEMP_YEARLY, str_history);

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Temperature - Saved to settings"));

#endif    // USE_UFILESYS
}

// ---------------
//    Humidity
// ---------------

// read humidity level
float SensorHumidityGet () { return SensorHumidityGet (0); }
float SensorHumidityGet (uint32_t timeout)
{
  float result;

  if (sensor_status.humi.value == INT16_MAX) result = NAN;
    else if (timeout == 0) result = (float)sensor_status.humi.value / 10;
    else if (sensor_status.humi.timestamp == UINT32_MAX) result = NAN;
    else if (Rtc.local_time > sensor_status.humi.timestamp + sensor_config.humi.validity) result = NAN;
    else result = (float)sensor_status.humi.value / 10;

  return result;
}

// set humidity for current 10mn slot
void SensorHumiditySet (float humidity)
{
  uint8_t day, hour, slot;
  int8_t  value;

  // check validity
  if (isnan (humidity) || (humidity <= 0) || (humidity >= 100)) return;
  if (!RtcTime.valid) return;

  // update value and timestamp
  sensor_status.humi.value = (int16_t)(humidity * 10);
  sensor_status.humi.timestamp = LocalTime ();

  // log
  AddLog (LOG_LEVEL_DEBUG, PSTR ("SEN: humidity %d.%d%%"), sensor_status.humi.value / 10, sensor_status.humi.value % 10);

  // update weekly data
  if (!RtcTime.valid) return;

  // check slot validity
  day  = RtcTime.day_of_week - 1;
  hour = RtcTime.hour;
  slot = RtcTime.minute / 10; 
  if ((day >= 7) || (hour >= 24) || (slot >= 6)) return;

  // update slot
  value = (int8_t)(sensor_status.humi.value / 10);
  if (sensor_week.humi[day][hour][slot] == INT8_MAX) sensor_week.humi[day][hour][slot] = value;
    else sensor_week.humi[day][hour][slot] = max (sensor_week.humi[day][hour][slot], value);
}

// load weekly humidity history from settings string
//   byte 0     = first humidity value else 0xFF
//   byte 1..84 = delta per hour on 4 bits (N/A, -30, -20, -10, -5, -3, -1, 0, +1, +3, +5, +10, N/A, +20, +30, N/A)
bool SensorHumidityLoadWeek ()
{
  int8_t  humi_ref, humi_hour;
  uint8_t day, hour, slot, index, value;
  char    str_history[86];

  // init slots to N/A
  for (day = 0; day < 7; day ++)
    for (hour = 0; hour < 24; hour ++)
      for (slot = 0; slot < 6; slot ++) sensor_week.humi[day][hour][slot] = INT8_MAX;

  // check if history is available
  strlcpy (str_history, SettingsText (SET_SENSOR_HUMI_WEEKLY), sizeof (str_history));
  value = strlen (str_history);
  if ( value > 0) sensor_status.humi.weekly = 1;
    else sensor_status.humi.weekly = 0;
  if ((value == SENSOR_SIZE_FS) || (value != sensor_size_week_humi)) return false;

  // calculate reference humidity
  humi_ref = INT8_MAX;
  if (str_history[0] != 0xFF) humi_ref = (int8_t)str_history[0] - 1;
  if (humi_ref == INT8_MAX) return false;

  // update sensor history string
  index = 1;
  for (day = 0; day < 7; day ++)
    for (hour = 0; hour < 24; hour ++)
    {
      // load history delta
      if (hour % 2 == 0) value = str_history[index] & 0x0F;
        else value = str_history[index++] >> 4;

      // set hour value according to delta
      switch (value)
      {
        case 1:  humi_hour = humi_ref - 30; break;      // -30%
        case 2:  humi_hour = humi_ref - 20; break;      // -20%
        case 3:  humi_hour = humi_ref - 10; break;      // -10%
        case 4:  humi_hour = humi_ref - 5;  break;      // -5%
        case 5:  humi_hour = humi_ref - 2;  break;      // -2%
        case 6:  humi_hour = humi_ref - 1;  break;      // -1%
        case 7:  humi_hour = humi_ref;      break;      // idem
        case 8:  humi_hour = humi_ref + 1;  break;      // +1%
        case 9:  humi_hour = humi_ref + 2;  break;      // +2%
        case 10: humi_hour = humi_ref + 5;  break;      // +5%
        case 11: humi_hour = humi_ref + 10; break;      // +10%
        case 13: humi_hour = humi_ref + 20; break;      // +20%
        case 14: humi_hour = humi_ref + 30; break;      // +30%
        default: humi_hour = INT8_MAX;      break;      // N/A
      }

      // if update is defined
      if (humi_hour != INT8_MAX)
      {
        // check boundaries
        humi_hour = max (humi_hour, (int8_t)0);
        humi_hour = min (humi_hour, (int8_t)100);

        // update reference humidity
        humi_ref = humi_hour;
      } 

      // update hour slots
      for (slot = 0; slot < 6; slot ++) sensor_week.humi[day][hour][slot] = humi_hour;
    }

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Humidity - Loaded from settings"));

  return true;
}

// save weekly humidity history as settings string
//   byte 0     = first humidity value else 0xFF
//   byte 1..84 = delta per hour on 4 bits (N/A, -30, -20, -10, -5, -3, -1, 0, +1, +3, +5, +10, N/A, +20, +30, N/A)
void SensorHumiditySaveWeek ()
{
  uint8_t index, part, day, hour, slot, value;
  int8_t  humi_ref, humi_hour, humi_delta;
  char    str_history[86];

  // if no humidity sensor declared, nothing to do
  SettingsUpdateText (SET_SENSOR_HUMI_WEEKLY, "");
  if (sensor_status.humi.weekly == 0) return;

#ifdef USE_UFILESYS
  // if weekly history saved on filesystem
  SettingsUpdateText (SET_SENSOR_HUMI_WEEKLY, "fs");

#else
  // check settings size availability
  if (settings_text_size - GetSettingsTextLen () < sensor_size_week_humi)
  {
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Humidity - No space to save to settings"));
    sensor_status.humi.weekly = 0;
    return;
  }


  // loop thru humiditu slots to get first reference humidity
  humi_ref = INT8_MAX;  
  for (day = 0; day < 7; day ++)
    for (hour = 0; hour < 24; hour ++)
      for (slot = 0; slot < 6; slot ++) if (humi_ref == INT8_MAX) humi_ref = sensor_week.humi[day][hour][slot];

  // init history string
  for (index = 0; index < 85; index ++) str_history[index] = 0xFF;

  // write reference humidity
  if (humi_ref != INT8_MAX) str_history[0] = (uint8_t)humi_ref + 1;

  // update sensor history string
  index = 1;
  if (humi_ref != INT8_MAX) 
    for (day = 0; day < 7; day ++)
      for (hour = 0; hour < 24; hour ++)
      {
          // get max humidity in the hour slots
          humi_hour = sensor_week.humi[day][hour][0];
          for (slot = 1; slot < 6; slot ++)
          {
            if (humi_hour == INT8_MAX) humi_hour = sensor_week.humi[day][hour][slot];
              else if ((sensor_week.humi[day][hour][slot] != INT8_MAX) && (humi_hour < sensor_week.humi[day][hour][slot])) humi_hour = sensor_week.humi[day][hour][slot];
          }

          // define increment of decrement
          if (humi_hour == INT8_MAX) humi_delta = INT8_MAX;
            else humi_delta = humi_hour - humi_ref;

          // set value according to delta
          if (humi_delta == INT8_MAX)   value = 15;                        // N/A
          else if (humi_delta >= 30)  { value = 14; humi_ref += 30; }      // +30%
          else if (humi_delta >= 20)  { value = 13; humi_ref += 20; }      // +20%
          else if (humi_delta >= 10)  { value = 11; humi_ref += 10; }      // +10%
          else if (humi_delta >= 5)   { value = 10; humi_ref += 5;  }      // +5%
          else if (humi_delta >= 2)   { value = 9;  humi_ref += 2;  }      // +3%
          else if (humi_delta >= 1)   { value = 8;  humi_ref += 1;  }      // +1%
          else if (humi_delta == 0)     value = 7;                         // +0%
          else if (humi_delta == 1)   { value = 6;  humi_ref -= 1;  }      // +1%
          else if (humi_delta == -2)  { value = 5;  humi_ref -= 2;  }      // +3%
          else if (humi_delta >= -5)  { value = 4;  humi_ref -= 5;  }      // +5%
          else if (humi_delta >= -10) { value = 3;  humi_ref -= 10; }      // +10%
          else if (humi_delta >= -20) { value = 2;  humi_ref -= 20; }      // +20%
          else                        { value = 1;  humi_ref -= 30; }      // +30%

          // check boundaries
          humi_ref = max (humi_ref, (int8_t)0);
          humi_ref = min (humi_ref, (int8_t)100);
          
          // update history string
          if (hour % 2 == 0) str_history[index] = value;
            else str_history[index] = str_history[index++] | (value << 4);
        }

  // save history string
  str_history[sensor_size_week_humi] = 0;
  SettingsUpdateText (SET_SENSOR_HUMI_WEEKLY, str_history);

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Humidity - Saved to settings"));
#endif    // USE_UFILESYS
}

// ---------------
//    Presence
// ---------------

// read movement detection status (timeout in sec.)
bool SensorPresenceGet () { return SensorPresenceGet (0); }
bool SensorPresenceGet (uint32_t timeout)
{
  bool result;

  if (timeout == 0) result = sensor_status.pres.value;
    else if (sensor_status.pres.timestamp == UINT32_MAX) result = false;
    else if (Rtc.local_time > sensor_status.pres.timestamp + sensor_config.pres.validity) result = false;
    else result = sensor_status.pres.value;

  return result;
}

// set current presence
void SensorPresenceSet ()
{
  uint8_t  day, hour, slot;
  uint32_t time_now = LocalTime ();

  // check validity
  if (!RtcTime.valid) return;

  // check ignore time frame
  if ((sensor_status.time_ignore != UINT32_MAX) && (sensor_status.time_ignore < time_now)) sensor_status.time_ignore = UINT32_MAX;
  if (sensor_status.time_ignore != UINT32_MAX) return;

  // update value and timestamp
  sensor_status.pres.value     = 1;
  sensor_status.pres.timestamp = LocalTime ();

  // update history slot
  day  = RtcTime.day_of_week - 1;
  hour = RtcTime.hour;
  slot = RtcTime.minute / 10; 
  if ((day >= 7) || (hour >= 24) || (slot >= 6)) return;
  sensor_week.event[day][hour][slot].presence = 1;

  // log
  AddLog (LOG_LEVEL_DEBUG, PSTR ("SEN: Presence - Detected (%u, %u, %u)"), day, hour, slot);
}

// get delay since last movement was detected
uint32_t SensorPresenceDelaySinceDetection ()
{
  uint32_t result = UINT32_MAX;

  if (sensor_status.pres.timestamp != 0) result = LocalTime () - sensor_status.pres.timestamp;

  return result;
}

// suspend presence detection
void SensorPresenceSuspendDetection (const uint32_t duration)
{
  // set timestamp to ignore sensor state
  sensor_status.time_ignore = LocalTime () + duration;
}

// load weekly presence history from settings string
bool SensorPresenceLoadWeek ()
{
  bool    history;
  uint8_t day, hour, slot, index, value, presence;
  size_t  length;
  char    str_history[50];

  // init slots to no presence
  for (day = 0; day < 7; day ++)
    for (hour = 0; hour < 24; hour ++)
      for (slot = 0; slot < 6; slot ++) sensor_week.event[day][hour][slot].presence = 0;

  // check if history is available
  strlcpy (str_history, SettingsText (SET_SENSOR_PRES_WEEKLY), sizeof (str_history));
  value = strlen (str_history);
  if ( value > 0) sensor_status.pres.weekly = 1;
    else sensor_status.pres.weekly = 0;
  if ((value == SENSOR_SIZE_FS) || (value != sensor_size_week_pres)) return false;

  // loop to load slots
  for (day = 0; day < 7; day ++)
  {
    value = 0x01 << day;
    for (hour = 0; hour < 24; hour ++)
      for (slot = 0; slot < 6; slot ++)
      {
        index = hour * 2 + slot / 3;
        presence = str_history[index] & value;
        if (presence != 0) sensor_week.event[day][hour][slot].presence = 1;
      }
  }

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Presence - Loaded from settings"));

  return true;
}

// save weekly presence history in settings string
void SensorPresenceSaveWeek ()
{
  uint8_t day, hour, slot, index, value;
  char    str_history[50];

  // if presence detection not enabled, nothing to save
  SettingsUpdateText (SET_SENSOR_PRES_WEEKLY, "");
  if (sensor_status.pres.weekly == 0) return;

#ifdef USE_UFILESYS
  // if weekly history saved on filesystem
  SettingsUpdateText (SET_SENSOR_PRES_WEEKLY, "fs");

#else
  // if settings size is not enough, nothing to save
  if (settings_text_size - GetSettingsTextLen () < sensor_size_week_pres)
  {
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Presence - No space to save to settings"));
    sensor_status.pres.weekly = 0;
    return;
  }


  // init sensor history string
  for (slot = 0; slot < 48; slot ++) str_history[slot] = 128;

  // update sensor history string
  for (day = 0; day < 7; day ++)
  {
    value = 0x01 << day;
    for (hour = 0; hour < 24; hour ++)
      for (slot = 0; slot < 6; slot ++)
        if (sensor_week.event[day][hour][slot].presence == 1)
        {
          index = hour * 2 + slot / 3;
          str_history[index] = str_history[index] | value;
        }
  }

  // save history string
  str_history[sensor_size_week_pres] = 0;
  SettingsUpdateText (SET_SENSOR_PRES_WEEKLY, str_history);

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Presence - Saved to settings"));
#endif    // USE_UFILESYS
}

// ---------------
//    Activity
// ---------------

// set current activity
void SensorActivitySet ()
{
  uint8_t day, hour, slot;

  // check validity
  if (!RtcTime.valid) return;

  // update history slot
  day  = RtcTime.day_of_week - 1;
  hour = RtcTime.hour;
  slot = RtcTime.minute / 10; 
  if ((day >= 7) || (hour >= 24) || (slot >= 6)) return;
  sensor_week.event[day][hour][slot].activity = 1;

  // log
  AddLog (LOG_LEVEL_DEBUG, PSTR ("SEN: Activity - Declared (%u, %u, %u)"), day, hour, slot);
}

// load weekly activity history from settings string
bool SensorActivityLoadWeek ()
{
  bool    history;
  uint8_t day, hour, slot, index, value, action;
  size_t  length;
  char    str_history[50];

  // init slots to no presence
  for (day = 0; day < 7; day ++)
    for (hour = 0; hour < 24; hour ++)
      for (slot = 0; slot < 6; slot ++) sensor_week.event[day][hour][slot].activity = 0;

  // check if history is available
  strlcpy (str_history, SettingsText (SET_SENSOR_ACTI_WEEKLY), sizeof (str_history));
  value = strlen (str_history);
  if ( value > 0) sensor_status.acti.weekly = 1;
    else sensor_status.acti.weekly = 0;
  if ((value == SENSOR_SIZE_FS) || (value != sensor_size_week_acti)) return false;

  // loop to load slots
  for (day = 0; day < 7; day ++)
  {
    value = 0x01 << day;
    for (hour = 0; hour < 24; hour ++)
      for (slot = 0; slot < 6; slot ++)
      {
        index = hour * 2 + slot / 3;
        action = str_history[index] & value;
        if (action != 0) sensor_week.event[day][hour][slot].activity = 1;
      }
  }

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Activity - Loaded from settings"));

  return true;
}

// save weekly activity history in configuration strings
void SensorActivitySaveWeek ()
{
  uint8_t day, hour, slot, index, value;
  char    str_history[50];

  // if presence detection not enabled, nothing to save
  SettingsUpdateText (SET_SENSOR_ACTI_WEEKLY, "");
  if (sensor_status.acti.weekly == 0) return;

#ifdef USE_UFILESYS
  // if weekly history saved on filesystem
  SettingsUpdateText (SET_SENSOR_ACTI_WEEKLY, "fs");

#else
  // if settings size is not enough, nothing to save
  if (settings_text_size - GetSettingsTextLen () < sensor_size_week_acti)
  {
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Activity - No space to save to settings"));
    sensor_status.acti.weekly = 0;
    return;
  }


  // init sensor history string
  for (slot = 0; slot < 48; slot ++) str_history[slot] = 128;

  // update sensor history string
  for (day = 0; day < 7; day ++)
  {
    value = 0x01 << day;
    for (hour = 0; hour < 24; hour ++)
      for (slot = 0; slot < 6; slot ++)
        if (sensor_week.event[day][hour][slot].activity == 1)
        {
          index = hour * 2 + slot / 3;
          str_history[index] = str_history[index] | value;
        }
  }

  // save history string
  str_history[sensor_size_week_acti] = 0;
  SettingsUpdateText (SET_SENSOR_ACTI_WEEKLY, str_history);

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Activity - Saved to settings"));
#endif    // USE_UFILESYS
}

// ----------------
//    Inactivity
// ----------------

// set current inactivity
void SensorInactivitySet ()
{
  uint8_t day, hour, slot;

  // check validity
  if (!RtcTime.valid) return;
  
  // update history slot
  day  = RtcTime.day_of_week - 1;
  hour = RtcTime.hour;
  slot = RtcTime.minute / 10; 
  if ((day >= 7) || (hour >= 24) || (slot >= 6)) return;
  sensor_week.event[day][hour][slot].inactivity = 1;

  // log
  AddLog (LOG_LEVEL_DEBUG, PSTR ("SEN: Inactivity declared"));
}

// load weekly inactivity history from settings string
bool SensorInactivityLoadWeek ()
{
  bool    history;
  uint8_t day, hour, slot, index, value, detected;
  size_t  length;
  char    str_history[50];

  // init slots to no presence
  for (day = 0; day < 7; day ++)
    for (hour = 0; hour < 24; hour ++)
      for (slot = 0; slot < 6; slot ++) sensor_week.event[day][hour][slot].inactivity = 0;

  // check if history is available
  strlcpy (str_history, SettingsText (SET_SENSOR_INAC_WEEKLY), sizeof (str_history));
  value = strlen (str_history);
  if ( value > 0) sensor_status.inac.weekly = 1;
    else sensor_status.inac.weekly = 0;
  if ((value == SENSOR_SIZE_FS) || (value != sensor_size_week_inac)) return false;

  // loop to load slots
  for (day = 0; day < 7; day ++)
  {
    value = 0x01 << day;
    for (hour = 0; hour < 24; hour ++)
      for (slot = 0; slot < 6; slot ++)
      {
        index = hour * 2 + slot / 3;
        detected = str_history[index] & value;
        if (detected != 0) sensor_week.event[day][hour][slot].inactivity = 1;
      }
  }

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Inactivity - Loaded from settings"));

  return true;
}

// save weekly inactivity history in settings string
void SensorInactivitySaveWeek ()
{
  uint8_t day, hour, slot, index, value;
  char    str_history[50];

  // if presence detection not enabled, nothing to save
  SettingsUpdateText (SET_SENSOR_INAC_WEEKLY, "");
  if (sensor_status.inac.weekly == 0) return;

#ifdef USE_UFILESYS
  // if weekly history saved on filesystem
  SettingsUpdateText (SET_SENSOR_INAC_WEEKLY, "fs");

#else
  // if settings size is not enough, nothing to save
  if (settings_text_size - GetSettingsTextLen () < sensor_size_week_inac)
  {
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Inactivity - No space to save to settings"));
    sensor_status.inac.weekly = 0;
    return;
  }


  // init sensor history string
  for (slot = 0; slot < 48; slot ++) str_history[slot] = 128;

  // update sensor history string
  for (day = 0; day < 7; day ++)
  {
    value = 0x01 << day;
    for (hour = 0; hour < 24; hour ++)
      for (slot = 0; slot < 6; slot ++)
        if (sensor_week.event[day][hour][slot].inactivity == 1)
        {
          index = hour * 2 + slot / 3;
          str_history[index] = str_history[index] | value;
        }
  }

  // save history string
  str_history[sensor_size_week_inac] = 0;
  SettingsUpdateText (SET_SENSOR_INAC_WEEKLY, str_history);

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Inactivity - Saved to settings"));
#endif    // USE_UFILESYS
}

/**************************************************\
 *                     Config
\**************************************************/

// load configuration parameters
void SensorLoadConfig () 
{
  uint8_t index, value, day, hour, slot;
  char    str_history[50];

  // load sensor data validity timeout
  sensor_config.temp.validity = (uint16_t)Settings->free_73A[0] * 10;
  sensor_config.humi.validity = (uint16_t)Settings->free_73A[1] * 10;
  sensor_config.pres.validity = (uint16_t)Settings->free_73A[2] * 10;

  // load presence detection flag
  sensor_config.presence = Settings->free_ea6[31];

  // retrieve MQTT topics
  sensor_config.temp.topic = SettingsText (SET_SENSOR_TEMP_TOPIC);
  sensor_config.humi.topic = SettingsText (SET_SENSOR_HUMI_TOPIC);
  sensor_config.pres.topic = SettingsText (SET_SENSOR_PRES_TOPIC);

  // retrieve MQTT keys
  sensor_config.temp.key = SettingsText (SET_SENSOR_TEMP_KEY);
  sensor_config.humi.key = SettingsText (SET_SENSOR_HUMI_KEY);
  sensor_config.pres.key = SettingsText (SET_SENSOR_PRES_KEY);

  // check MQTT keys
  if (sensor_config.temp.key == "") sensor_config.temp.key = D_SENSOR_TEMPERATURE;
  if (sensor_config.humi.key == "") sensor_config.humi.key = D_SENSOR_HUMIDITY;
  if (sensor_config.pres.key == "") sensor_config.pres.key = D_SENSOR_PRESENCE;

  // check parameters validity
  if (sensor_config.temp.validity == 0) sensor_config.temp.validity = SENSOR_TEMP_TIMEOUT;
  if (sensor_config.humi.validity == 0) sensor_config.humi.validity = SENSOR_HUMI_TIMEOUT;
  if (sensor_config.pres.validity == 0) sensor_config.pres.validity = SENSOR_PRES_TIMEOUT;
  if (sensor_config.presence >= SENSOR_PRESENCE_MAX) sensor_config.presence = SENSOR_PRESENCE_NONE;
}

// save configuration parameters
void SensorSaveConfig () 
{
  // save sensor data validity timeout
  Settings->free_73A[0] = (uint8_t)(sensor_config.temp.validity / 10);
  Settings->free_73A[1] = (uint8_t)(sensor_config.humi.validity / 10);
  Settings->free_73A[2] = (uint8_t)(sensor_config.pres.validity / 10);

  // save presence detection flag
  Settings->free_ea6[31] = sensor_config.presence;

  // save MQTT topics
  SettingsUpdateText (SET_SENSOR_TEMP_TOPIC, sensor_config.temp.topic.c_str ());
  SettingsUpdateText (SET_SENSOR_HUMI_TOPIC, sensor_config.humi.topic.c_str ());
  SettingsUpdateText (SET_SENSOR_PRES_TOPIC, sensor_config.pres.topic.c_str ());

  // save MQTT keys
  if (sensor_config.temp.key == D_SENSOR_TEMPERATURE) SettingsUpdateText (SET_SENSOR_TEMP_KEY, "");
    else SettingsUpdateText (SET_SENSOR_TEMP_KEY, sensor_config.temp.key.c_str ());
  if (sensor_config.humi.key == D_SENSOR_HUMIDITY) SettingsUpdateText (SET_SENSOR_HUMI_KEY, "");
    else SettingsUpdateText (SET_SENSOR_HUMI_KEY, sensor_config.humi.key.c_str ());
  if (sensor_config.pres.key == D_SENSOR_PRESENCE) SettingsUpdateText (SET_SENSOR_PRES_KEY, "");
    else SettingsUpdateText (SET_SENSOR_PRES_KEY, sensor_config.pres.key.c_str ());
}

// reset sensor history for specific week day (0=sunday ... 6=saturday)
void SensorResetWeekDay (const uint8_t day)
{
  uint8_t hour, slot;
  char    str_day[4];

  // check parameters
  if (day >= 7) return;

  // reset daily slots
  for (hour = 0; hour < 24; hour ++)
    for (slot = 0; slot < 6; slot ++)
    {
      sensor_week.temp[day][hour][slot] = INT16_MAX;
      sensor_week.humi[day][hour][slot] = INT8_MAX;
      sensor_week.event[day][hour][slot].data = 0;
    }

  // log
  strlcpy (str_day, kWeekdayNames + day * 3, 4);
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: reset %s"), str_day);
}

// load weekly data history
void SensorLoadWeek () 
{
  bool done;

  // load from settings
  done  = SensorTemperatureLoadWeek ();
  done |= SensorHumidityLoadWeek ();
  done |= SensorPresenceLoadWeek ();
  done |= SensorActivityLoadWeek ();
  done |= SensorInactivityLoadWeek ();

#ifdef USE_UFILESYS
  if (!done) SensorFileLoadWeek (0);
#endif    //  USE_UFILESYS
}

// save weekly data history
void SensorSaveWeek (const bool current) 
{
  uint8_t week;

  // save to settings
  if (current)
  {
    SensorTemperatureSaveWeek ();
    SensorHumiditySaveWeek ();
    SensorPresenceSaveWeek ();
    SensorActivitySaveWeek ();
    SensorInactivitySaveWeek ();
  }

#ifdef USE_UFILESYS
  // set weekly file index
  if (current) week = 0;
  else
  {
    week = SensorGetCurrentWeek () - 1;
    if (week == 0) week = 52;
  }

  // save weekly file
  SensorFileSaveWeek (week);
#endif    //  USE_UFILESYS
}

// reset temperature for specific year day (0 to 365)
void SensorResetYearDay (const uint16_t index)
{
  // check parameters
  if (index >= SENSOR_YEAR_DAYS) return;

  // reset yearly slot
  sensor_year.temp_min[index] = INT16_MAX;
  sensor_year.temp_max[index] = INT16_MAX;
}

// load yearly data history
void SensorLoadYear ()
{
  bool done;

  // load from settings
  done = SensorTemperatureLoadYear ();

#ifdef USE_UFILESYS
  if (!done) SensorFileLoadYear ();
#endif    //  USE_UFILESYS
}

// save yearly data history
void SensorSaveYear () 
{
  uint8_t week;

  // save to settings
  SensorTemperatureSaveYear ();

#ifdef USE_UFILESYS
  // save yearly file
  SensorFileSaveYear ();
#endif    //  USE_UFILESYS
}

/***************************\
 *      History files
\***************************/

#ifdef USE_UFILESYS

// load week file to graph data
bool SensorFileExist (const uint8_t week)
{
  bool exist;
  char str_file[32];

  // check file
  sprintf (str_file, D_SENSOR_FILENAME_WEEK, week);
  exist = ffsp->exists (str_file);

  return exist;
}

// save current week to file
bool SensorFileSaveWeek (const uint8_t week)
{
  uint8_t day, hour, slot;
  char    str_value[32];
  String  str_buffer;
  File    file;

  // check filesystem
  if (ufs_type == 0) return false;

  // create file and write header
  sprintf (str_value, D_SENSOR_FILENAME_WEEK, week);
  file = ffsp->open (str_value, "w");
  file.print ("Day;Hour;Slot;Temp;Humi;Pres;Acti;Inac\n");

  // loop to write data
  str_buffer = "";
  for (day = 0; day < 7; day ++)
    for (hour = 0; hour < 24; hour ++)
      for (slot = 0; slot < 6; slot ++)
      {
        // day, hour, slot
        sprintf_P (str_value, PSTR ("%u;%u;%u;"), day, hour, slot);
        str_buffer += str_value;

        // temperature
        if (sensor_week.temp[day][hour][slot] == INT16_MAX) strcpy (str_value, "*;");
          else sprintf_P (str_value, PSTR ("%d;"), sensor_week.temp[day][hour][slot]);
        str_buffer += str_value;

        // humidity
        if (sensor_week.humi[day][hour][slot] == INT8_MAX) strcpy (str_value, "*;");
          else sprintf_P (str_value, PSTR ("%d;"), sensor_week.humi[day][hour][slot]);
        str_buffer += str_value;

        // presence, activity and inactivity
        sprintf_P (str_value, PSTR ("%u;%u;%u\n"), sensor_week.event[day][hour][slot].presence, sensor_week.event[day][hour][slot].activity, sensor_week.event[day][hour][slot].inactivity);
        str_buffer += str_value;

        // if needed, append line to file
        if (str_buffer.length () > 256) { file.print (str_buffer.c_str ()); str_buffer = ""; }
      }

  // write and close
  if (str_buffer.length () > 0) file.print (str_buffer.c_str ());
  file.close ();

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Last week saved to file"));

  return true;
}

// load week file to graph data
bool SensorFileLoadWeek (const uint8_t week)
{
  uint8_t  token, day, hour, slot;
  uint32_t len_buffer, size_buffer;
  char     *pstr_token, *pstr_buffer, *pstr_line;
  char     str_buffer[512];
  File     file;

  // check filesystem
  if (ufs_type == 0) return false;

  // check file
  pstr_buffer = str_buffer;
  sprintf (str_buffer, D_SENSOR_FILENAME_WEEK, week);
  if (!ffsp->exists (str_buffer)) return false;

  // init graph data
  for (day = 0; day < 7; day++)
    for (hour = 0; hour < 24; hour++)
      for (slot = 0; slot < 6; slot++)
      {
        sensor_graph.temp[day][hour][slot] = INT16_MAX;
        sensor_graph.humi[day][hour][slot] = INT8_MAX;
        sensor_graph.event[day][hour][slot].data = 0;
      }

  // loop to read file
  file = ffsp->open (str_buffer, "r");
  strcpy(str_buffer, "");
  while (file.available ())
  {
    // read next block
    size_buffer = strlen (str_buffer);
    len_buffer = file.readBytes (str_buffer + size_buffer, sizeof (str_buffer) - size_buffer - 1);
    str_buffer[size_buffer + len_buffer] = 0;

    // loop to read lines
    pstr_buffer = str_buffer;
    pstr_line = strchr (pstr_buffer, '\n');
    while (pstr_line)
    {
      // extract line
      token = 0;
      day = hour = slot = UINT8_MAX;
      *pstr_line = 0;
      pstr_token = strtok (pstr_buffer, ";");
      while (pstr_token != nullptr)
      {
        // if value is a numerical one
        if (isdigit (pstr_token[0]) || (pstr_token[0] == '-')) switch (token)
        {
          case 0: day  = atoi (pstr_token); break;
          case 1: hour = atoi (pstr_token); break;
          case 2: slot = atoi (pstr_token); break;
          case 3: if ((day < 7) && (hour < 24) && (slot < 6)) sensor_graph.temp[day][hour][slot] = (int16_t)atoi (pstr_token); break;
          case 4: if ((day < 7) && (hour < 24) && (slot < 6)) sensor_graph.humi[day][hour][slot] = (int16_t)atoi (pstr_token); break;
          case 5: if ((day < 7) && (hour < 24) && (slot < 6)) sensor_graph.event[day][hour][slot].presence   = (uint8_t)atoi (pstr_token); break;
          case 6: if ((day < 7) && (hour < 24) && (slot < 6)) sensor_graph.event[day][hour][slot].activity   = (uint8_t)atoi (pstr_token); break;
          case 7: if ((day < 7) && (hour < 24) && (slot < 6)) sensor_graph.event[day][hour][slot].inactivity = (uint8_t)atoi (pstr_token); break;
        }

        // next token in the line
        token++;
        pstr_token = strtok (nullptr, ";");
      }

      // look for next line
      pstr_buffer = pstr_line + 1;
      pstr_line = strchr (pstr_buffer, '\n');
    }

    // deal with remaining string
    if (pstr_buffer != str_buffer) strcpy(str_buffer, pstr_buffer);
      else strcpy(str_buffer, "");
  }

  // close file
  file.close ();

  // if dealing with current week, load main data
  if (week == 0)
  {
    for (day = 0; day < 7; day ++)
      for (hour = 0; hour < 24; hour ++)
        for (slot = 0; slot < 6; slot ++)
        {
          sensor_week.temp[day][hour][slot] = sensor_graph.temp[day][hour][slot];
          sensor_week.humi[day][hour][slot] = sensor_graph.humi[day][hour][slot];
          sensor_week.event[day][hour][slot].data = sensor_graph.event[day][hour][slot].data;
        }

    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Last week loaded from file"));
  }

  return true;
}

// save current week to file
bool SensorFileSaveYear ()
{
  uint16_t day;
  char     str_value[32];
  String   str_buffer;
  File     file;

  // check filesystem
  if (ufs_type == 0) return false;

  // create file and write header
  file = ffsp->open (D_SENSOR_FILENAME_YEAR, "w");
  file.print ("Day;Tmax;Tmin\n");

  // loop to write data
  str_buffer = "";
  for (day = 0; day < SENSOR_YEAR_DAYS; day ++)
  {
    // day
    sprintf (str_value, "%u;", day);
    str_buffer += str_value;

    // temperature max
    if (sensor_year.temp_max[day] == INT16_MAX) strcpy (str_value, "*;");
      else sprintf (str_value, "%d;", sensor_year.temp_max[day]);
    str_buffer += str_value;

    // temperature min
    if (sensor_year.temp_min[day] == INT16_MAX) strcpy (str_value, "*\n");
      else sprintf (str_value, "%d\n", sensor_year.temp_min[day]);
    str_buffer += str_value;

    // if needed, append line to file
    if (str_buffer.length () > 256) { file.print (str_buffer.c_str ()); str_buffer = ""; }
  }

  // write and close
  if (str_buffer.length () > 0) file.print (str_buffer.c_str ());
  file.close ();

  return true;
}

// load year data from file
bool SensorFileLoadYear ()
{
  uint8_t  token;
  uint16_t day;
  uint32_t len_buffer, size_buffer;
  char     *pstr_token, *pstr_buffer, *pstr_line;
  char     str_buffer[512];
  File     file;

  // check filesystem
  if (ufs_type == 0) return false;

  // check file
  pstr_buffer = str_buffer;
  if (!ffsp->exists (D_SENSOR_FILENAME_YEAR)) return false;

  // init graph data
  for (day = 0; day < SENSOR_YEAR_DAYS; day++)
  {
    sensor_year.temp_max[day] = INT16_MAX;
    sensor_year.temp_min[day] = INT16_MAX;
  }

  // loop to read file
  file = ffsp->open (D_SENSOR_FILENAME_YEAR, "r");
  strcpy(str_buffer, "");
  while (file.available ())
  {
    // read next block
    size_buffer = strlen (str_buffer);
    len_buffer = file.readBytes (str_buffer + size_buffer, sizeof (str_buffer) - size_buffer - 1);
    str_buffer[size_buffer + len_buffer] = 0;

    // loop to read lines
    pstr_buffer = str_buffer;
    pstr_line = strchr (pstr_buffer, '\n');
    while (pstr_line)
    {
      // extract line
      token = 0;
      day = UINT16_MAX;
      *pstr_line = 0;
      pstr_token = strtok (pstr_buffer, ";");
      while (pstr_token != nullptr)
      {
        // if value is a numerical one
        if (isdigit (pstr_token[0]) || (pstr_token[0] == '-')) switch (token)
        {
          case 0: day  = atoi (pstr_token); break;
          case 1: if (day < SENSOR_YEAR_DAYS) sensor_year.temp_max[day] = atoi (pstr_token); break;
          case 2: if (day < SENSOR_YEAR_DAYS) sensor_year.temp_min[day] = atoi (pstr_token); break;
        }

        // next token in the line
        token++;
        pstr_token = strtok (nullptr, ";");
      }

      // look for next line
      pstr_buffer = pstr_line + 1;
      pstr_line = strchr (pstr_buffer, '\n');
    }

    // deal with remaining string
    if (pstr_buffer != str_buffer) strcpy(str_buffer, pstr_buffer);
      else strcpy(str_buffer, "");
  }

  // close file
  file.close ();

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Last year loaded from file"));

  return true;
}

#endif      // USE_UFILESYS

/**************************************************\
 *                  Callback
\**************************************************/

void SensorInit () 
{
  uint16_t day, hour, slot;
  char     str_type[16];

  // init sensor values
  sensor_status.temp.source    = SENSOR_SOURCE_NONE;
  sensor_status.temp.value     = INT16_MAX;
  sensor_status.temp.last      = INT16_MAX;
  sensor_status.temp.timestamp = UINT32_MAX;
  sensor_status.humi.source    = SENSOR_SOURCE_NONE;
  sensor_status.humi.value     = INT16_MAX;
  sensor_status.humi.last      = INT16_MAX;
  sensor_status.humi.timestamp = UINT32_MAX;
  sensor_status.pres.source    = SENSOR_SOURCE_NONE;
  sensor_status.pres.value     = INT16_MAX;
  sensor_status.pres.last      = INT16_MAX;
  sensor_status.pres.timestamp = UINT32_MAX;

  // init sensors history
  for (day = 0; day < 7; day++)   SensorResetWeekDay (day);
  for (day = 0; day < SENSOR_YEAR_DAYS; day++) SensorResetYearDay (day);

  // check for DHT11 sensor
  if (PinUsed (GPIO_DHT11))
  {
    sensor_status.temp.source = SENSOR_SOURCE_DHT;
    sensor_status.humi.source = SENSOR_SOURCE_DHT;
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: %s temperature & humidity sensor"), "DHT11");
  }

  // check for DHT22 sensor
  else if (PinUsed (GPIO_DHT22))
  {
    sensor_status.temp.source = SENSOR_SOURCE_DHT;
    sensor_status.humi.source = SENSOR_SOURCE_DHT;
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: %s temperature & humidity sensor"), "DHT22");
  }

  // check for SI7021 sensor
  else if (PinUsed (GPIO_SI7021))
  {
    // set SI7021 tweaks
    Dht[0].delay_lo = 480;
    Dht[0].delay_hi = 40;

    sensor_status.temp.source = SENSOR_SOURCE_DHT;
    sensor_status.humi.source = SENSOR_SOURCE_DHT;
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: %s temperature & humidity sensor"), "SI7021");
  }

  // check for SHT30 and SHT40 sensor
  else if (PinUsed (GPIO_I2C_SCL) && PinUsed (GPIO_I2C_SDA))
  {
    sensor_status.temp.source = SENSOR_SOURCE_SHT;
    sensor_status.humi.source = SENSOR_SOURCE_SHT;
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: %s temperature & humidity sensor"), "SHTxx");
  }

  // check for DS18B20 sensor
  else if (PinUsed (GPIO_DSB))
  {
    sensor_status.temp.source = SENSOR_SOURCE_DSB;
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: %s temperature sensor"), "DS18B20");

    // force pullup for single DS18B20
    Settings->flag3.ds18x20_internal_pullup = 1;
  }

  // presence : check for generic sensor
  if (PinUsed (GPIO_CNTR1, SENSOR_PRESENCE_INDEX))
  {
    switch (sensor_config.presence)
    {
      case SENSOR_PRESENCE_RCWL0516:
        GetTextIndexed (str_type, sizeof (str_type), SENSOR_PRESENCE_RCWL0516, kSensorPresenceModel);
        sensor_status.pres.source = SENSOR_SOURCE_COUNTER;
        AddLog (LOG_LEVEL_INFO, PSTR ("SEN: %s motion sensor"), str_type);
        break;

      case SENSOR_PRESENCE_HWMS03:
        GetTextIndexed (str_type, sizeof (str_type), SENSOR_PRESENCE_HWMS03, kSensorPresenceModel);
        sensor_status.pres.source = SENSOR_SOURCE_COUNTER;
        AddLog (LOG_LEVEL_INFO, PSTR ("SEN: %s motion sensor"), str_type);
        break;
    }
  }

  // movement : serial sensors
  else if (PinUsed (GPIO_TXD) && PinUsed (GPIO_RXD))
  {
    switch (sensor_config.presence)
    {
      case SENSOR_PRESENCE_NONE:
        break;

#ifdef USE_LD1115             // HLK-LD1115 sensor
      case SENSOR_PRESENCE_LD1115:
        if (LD1115InitDevice (0)) sensor_status.pres.source = SENSOR_SOURCE_SERIAL;
        break;
#endif    // USE_LD1115

#ifdef USE_LD1125             // HLK-LD1125 sensor
      case SENSOR_PRESENCE_LD1125:
        if (LD1125InitDevice (0)) sensor_status.pres.source = SENSOR_SOURCE_SERIAL;
        break;
#endif    // USE_LD1125

#ifdef USE_LD2410             // HLK-LD2410 sensor
      case SENSOR_PRESENCE_LD2410:
        if (LD2410InitDevice ()) sensor_status.pres.source = SENSOR_SOURCE_SERIAL;
        break;
#endif    // USE_LD2410
    }
  }

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: sn_help to get help on Sensor commands"));
}

// update sensor condition every second
void SensorEverySecond ()
{
  bool     presence = true;
  bool     publish  = false;
  uint32_t sensor, index;
  float    temperature = NAN;
  float    humidity    = NAN;

  // check time init
  if (!RtcTime.valid) return;

  // ---------------------------------
  //   temperature & humidity sensor
  // ---------------------------------

  // update local sensors every 5 seconds
  sensor_status.counter = sensor_status.counter++ % SENSOR_MEASURE_UPDATE;
  if (sensor_status.counter == 0)
  {
    // SHT30 temperature and humidity sensor
    if (sensor_status.temp.source == SENSOR_SOURCE_SHT) Sht3xRead ((uint32_t)sht3x_sensors[0].type, temperature, humidity, sht3x_sensors[0].address);

    // SHT30 temperature and humidity sensor
    else if (sensor_status.temp.source == SENSOR_SOURCE_DHT) { temperature = Dht[0].t; humidity = Dht[0].h; }

    // DS18X20 temperature sensor
    else if ( sensor_status.temp.source == SENSOR_SOURCE_DSB)
    {
#ifdef ESP8266
      for (sensor = 0; sensor < DS18X20Data.sensors; sensor++)
      {
        index = ds18x20_sensor[sensor].index;
        if (ds18x20_sensor[index].valid) temperature = ds18x20_sensor[index].temperature;
      }
#else
      Ds18x20Read (0, temperature);
#endif
    }

    // update temperature
    SensorTemperatureSet (temperature);

    // update humidity
    SensorHumiditySet (humidity);

    // check if temperature needs JSON update
    if (sensor_status.temp.value != INT16_MAX)
    {
      publish |= (sensor_status.temp.last == INT16_MAX);
      publish |= ((sensor_status.temp.last != INT16_MAX) && (sensor_status.temp.last > sensor_status.temp.value + 1));
      publish |= ((sensor_status.temp.last != INT16_MAX) && (sensor_status.temp.last < sensor_status.temp.value - 1));
    }
    
    // check if humidity needs JSON update
    if (sensor_status.humi.value != INT16_MAX)
    {
      publish |= (sensor_status.humi.last == INT16_MAX);
      publish |= ((sensor_status.humi.last != INT16_MAX) && (sensor_status.humi.last > sensor_status.humi.value + 4));
      publish |= ((sensor_status.humi.last != INT16_MAX) && (sensor_status.humi.last < sensor_status.humi.value - 4));
    }
  }

  // -------------------------
  //   presence sensor
  // -------------------------

  // check according to sensor
  if (sensor_status.pres.source == SENSOR_SOURCE_COUNTER) presence = digitalRead (Pin (GPIO_CNTR1, SENSOR_PRESENCE_INDEX));
  else if (sensor_status.pres.source == SENSOR_SOURCE_SERIAL)
  {
    // check according to serial sensor
    switch (sensor_config.presence)
    {
#ifdef USE_LD1115
      case SENSOR_PRESENCE_LD1115:
        presence = LD1115GetGlobalDetectionStatus (1);
        break;
#endif      // USE_LD1115

#ifdef USE_LD1125
      case SENSOR_PRESENCE_LD1125:
        presence = LD1125GetGlobalDetectionStatus (1);
        break;
#endif      // USE_LD1125

#ifdef USE_LD2410
      case SENSOR_PRESENCE_LD2410:
        presence = LD2410GetDetectionStatus (1);
        break;
#endif      // USE_LD2410
    }
  }

  // if presence detected
  if (presence)
  {
    // update detection
    SensorPresenceSet ();

    // if needed, ask for a JSON update
    publish |= (Rtc.local_time > sensor_status.time_json);
  }

  // if needed, publish JSON
  if (publish) SensorShowJSON (false);
}

// check and update MQTT power subsciption after disconnexion
void SensorMqttSubscribe ()
{
  // if topic is defined, subscribe to remote temperature
  if (sensor_config.temp.topic.length () > 0)
  {
    // subscribe to sensor topic
    MqttSubscribe (sensor_config.temp.topic.c_str ());
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Subscribed remote temperature from %s"), sensor_config.temp.topic.c_str ());
  }

  // if topic is defined, subscribe to remote humidity
  if (sensor_config.humi.topic.length () > 0)
  {
    // subscribe to sensor topic
    MqttSubscribe (sensor_config.humi.topic.c_str ());
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Subscribed remote humidity from %s"), sensor_config.humi.topic.c_str ());
  }

  // if topic is defined, subscribe to remote movement
  if (sensor_config.pres.topic.length () > 0)
  {
    // subscribe to sensor topic
    MqttSubscribe (sensor_config.pres.topic.c_str ());
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Subscribed remote movement from %s"), sensor_config.pres.topic.c_str ());
  }
}

// read a key value in a JSON string
bool SensorGetJsonKey (const char* pstr_json, const char* pstr_key, char* pstr_value, size_t size_value)
{
  size_t length;
  char*  pstr_position;
  char   str_text[32];

  // check parameters and init
  if (!pstr_json || !pstr_key || !pstr_value) return false;
  strcpy (pstr_value, "");

  // look for provided key
  sprintf_P (str_text, PSTR ("\"%s\""), pstr_key);
  pstr_position = strstr (pstr_json, str_text);

  // if key is found, go to end of value
  if (pstr_position) pstr_position = strchr (pstr_position, ':');

  // extract value
  if (pstr_position)
  {
    // skip trailing space and "
    pstr_position++;
    while ((*pstr_position == ' ') || (*pstr_position == '"')) pstr_position++;

    // search for end of value caracter : " , }
    length = strcspn (pstr_position, "\",}");

    // save resulting value
    length = min (length + 1, size_value);
    strlcpy (pstr_value, pstr_position, length);
  }

  return (strlen (pstr_value) > 0);
}

// read received MQTT data to retrieve sensor value
bool SensorMqttData ()
{
  bool     is_topic, is_key, detected;
  bool     is_found = false;
  char     str_value[32];
  float    value;

  // if dealing with temperature topic
  is_topic = (sensor_config.temp.topic == XdrvMailbox.topic);
  if (is_topic)
  {
    // log
    is_found = true;
    AddLog (LOG_LEVEL_DEBUG, PSTR ("SEN: Received %s"), XdrvMailbox.topic);

    // look for temperature key
    is_key = SensorGetJsonKey (XdrvMailbox.data, sensor_config.temp.key.c_str (), str_value, sizeof (str_value));
    if (is_key)
    {
      // if needed, set remote source for temperature
      if (sensor_status.temp.source == SENSOR_SOURCE_NONE) sensor_status.temp.source = SENSOR_SOURCE_REMOTE;

      // save remote temperature
      value = atof (str_value) + Settings->temp_comp / 10;
      SensorTemperatureSet (value);

      // log
      AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Remote temp %s °C"), str_value);
    }
  }

  // if dealing with humidity topic
  is_topic = (sensor_config.humi.topic == XdrvMailbox.topic);
  if (is_topic)
  {
    // log
    is_found = true;
    AddLog (LOG_LEVEL_DEBUG, PSTR ("SEN: Received %s"), XdrvMailbox.topic);

    // look for humidity key
    is_key = SensorGetJsonKey (XdrvMailbox.data, sensor_config.humi.key.c_str (), str_value, sizeof (str_value));
    if (is_key)
    {
      // if needed, set remote source for humidity
      if (sensor_status.humi.source == SENSOR_SOURCE_NONE) sensor_status.humi.source = SENSOR_SOURCE_REMOTE;

      // save remote humidity
      value = atof (str_value);
      SensorHumiditySet (value);

      // log
      AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Remote humi %s %%"), str_value);
    }
  }

  // if dealing with movement topic
  is_topic = (sensor_config.pres.topic == XdrvMailbox.topic);
  if (is_topic)
  {
    // log
    is_found = true;
    AddLog (LOG_LEVEL_DEBUG, PSTR ("SEN: Received %s"), XdrvMailbox.topic);

    // look for movement key
    is_key = SensorGetJsonKey (XdrvMailbox.data, sensor_config.pres.key.c_str (), str_value, sizeof (str_value));
    if (is_key)
    {
      // if needed, set remote source for movement
      if (sensor_status.pres.source == SENSOR_SOURCE_NONE) sensor_status.pres.source = SENSOR_SOURCE_REMOTE;

      // read presence detection
      detected = ((strcmp (str_value, "1") == 0) || (strcmp (str_value, "on") == 0) || (strcmp (str_value, "ON") == 0));
      if (detected) SensorPresenceSet ();

      // log
      AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Remote pres %u"), detected);
    }
  }

  return is_found;
}

// Show JSON status (for local sensors and presence sensor)
void SensorShowJSON (bool append)
{
  bool     is_first, is_temp, is_humi, is_pres;
  uint16_t validity;
  char     str_value[16];

  // set next JSON update according to minimum sensor validity
  validity = sensor_config.temp.validity;
  validity = min (validity, sensor_config.humi.validity);
  validity = min (validity, sensor_config.pres.validity);
  sensor_status.time_json = Rtc.local_time + validity;

  // update last published values
  sensor_status.temp.last = sensor_status.temp.value;
  sensor_status.humi.last = sensor_status.humi.value;
  sensor_status.pres.last = sensor_status.pres.value;

  // if at least one local sensor 
  is_first = true;
  is_temp  = ((sensor_status.temp.source < SENSOR_SOURCE_REMOTE) && (sensor_status.temp.value != INT16_MAX));
  is_humi  = ((sensor_status.humi.source < SENSOR_SOURCE_REMOTE) && (sensor_status.humi.value != INT16_MAX));
  is_pres  = ((sensor_status.pres.source < SENSOR_SOURCE_REMOTE) && (sensor_status.pres.value != INT16_MAX));
  if (is_temp || is_humi || is_pres)
  {
    // add , in append mode or { in direct publish mode
    if (append) ResponseAppend_P (PSTR (",")); else Response_P (PSTR ("{"));

    ResponseAppend_P (PSTR ("\"Sensor\":{"));

    // if local temperature sensor
    if (is_temp)
    {
      ResponseAppend_P (PSTR ("\"Temp\":%d.%d"), sensor_status.temp.value / 10, sensor_status.temp.value % 10);
      is_first = false;
    }

    // if local humidity sensor
    if (is_humi)
    {
      if (!is_first) ResponseAppend_P (PSTR (","));
      ResponseAppend_P (PSTR ("\"Humi\":%d.%d"), sensor_status.humi.value / 10, sensor_status.humi.value % 10);
      is_first = false;
    }

    // if local presence sensor present
    if (is_pres)
    {
      if (!is_first) ResponseAppend_P (PSTR (","));
      ResponseAppend_P (PSTR ("\"Pres\":%d"), sensor_status.pres.value);
      if (sensor_status.pres.timestamp != 0) ResponseAppend_P (PSTR (",\"Since\":%u"), LocalTime () - sensor_status.pres.timestamp);
      SensorGetDelayText (sensor_status.pres.timestamp, str_value, sizeof (str_value));
      ResponseAppend_P (PSTR (",\"Delay\":\"%s\""), str_value);
    }

    ResponseAppend_P (PSTR ("}"));

    // publish it if not in append mode
    if (!append)
    {
      ResponseAppend_P (PSTR ("}"));
      MqttPublishTeleSensor ();
    } 
  }
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// measure graph main button
void SensorWebMainButtonMeasure ()
{
  // display yearly button
  if (sensor_status.yearly) WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>📉 Year measure</button></form></p>\n"), D_SENSOR_PAGE_YEARLY);

  // display weekly button
  if (sensor_status.temp.weekly || sensor_status.humi.weekly) WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>📈 Week measure</button></form></p>\n"), D_SENSOR_PAGE_MEASURE);
}

// presence graph main button
void SensorWebMainButtonPresence ()
{
  if (sensor_status.pres.source != SENSOR_SOURCE_NONE) WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>👋 Week detection</button></form></p>\n"), D_SENSOR_PAGE_PRESENCE);
}

// append remote sensor to main page
void SensorWebSensor ()
{
  bool result, temp_remote, humi_remote, pres_remote;
  char str_type[16];
  char str_time[16];
  char str_value[32];

  // init
  strcpy (str_type, "");
  strcpy (str_value, "");

  // if needed, display remote sensors
  if ((sensor_status.temp.source == SENSOR_SOURCE_REMOTE) && (sensor_status.temp.value != INT16_MAX)) WSContentSend_PD (PSTR ("{s}%s %s{m}%d.%d °C{e}"), D_SENSOR_REMOTE, D_SENSOR_TEMPERATURE, sensor_status.temp.value / 10, sensor_status.temp.value % 10);
  if ((sensor_status.humi.source == SENSOR_SOURCE_REMOTE) && (sensor_status.humi.value != INT16_MAX)) WSContentSend_PD (PSTR ("{s}%s %s{m}%d.%d %%{e}"), D_SENSOR_REMOTE, D_SENSOR_HUMIDITY, sensor_status.humi.value / 10, sensor_status.humi.value % 10);

  // calculate last presence detection delay
  SensorGetDelayText (sensor_status.pres.timestamp, str_time, sizeof (str_time));

  // if local RCWL-0516 presence sensor
  if ((sensor_status.pres.source == SENSOR_SOURCE_COUNTER) && (sensor_config.presence == SENSOR_PRESENCE_RCWL0516))
  {
    GetTextIndexed (str_type, sizeof (str_type), SENSOR_PRESENCE_RCWL0516, kSensorPresenceModel);
    strcpy (str_value, "0 - 7m");
  }

  // else if local HWMS-03 presence sensor
  else if ((sensor_status.pres.source == SENSOR_SOURCE_COUNTER) && (sensor_config.presence == SENSOR_PRESENCE_HWMS03))
  {
    GetTextIndexed (str_type, sizeof (str_type), SENSOR_PRESENCE_HWMS03, kSensorPresenceModel);
    strcpy (str_value, "0.5 - 10m");
  }
  
  // else if remote presence sensor
  else if ((sensor_status.pres.source == SENSOR_SOURCE_REMOTE))
  {
    GetTextIndexed (str_type, sizeof (str_type), SENSOR_PRESENCE_REMOTE, kSensorPresenceModel);
    strcpy (str_value, D_SENSOR_REMOTE);
  }

  // if local presence sensor is detected, display status
  if (strlen (str_type) > 0)
  {
    // display presence sensor
    WSContentSend_PD (PSTR ("<div style='display:flex;font-size:10px;text-align:center;margin-top:4px;padding:2px 6px;background:#333333;border-radius:8px;'>\n"));
    WSContentSend_PD (PSTR ("<div style='width:40%%;padding:0px;text-align:left;font-size:12px;font-weight:bold;'>%s</div>\n"), str_type);
    WSContentSend_PD (PSTR ("<div style='width:30%%;padding:0px;border-radius:6px;"));
    if (sensor_status.pres.value == 1) WSContentSend_PD (PSTR ("background:#D00;'>%s</div>\n"), str_time);
    else WSContentSend_PD (PSTR ("background:#444;color:grey;'>%s</div>\n"), str_time);
    WSContentSend_PD (PSTR ("<div style='width:30%%;padding:2px 0px 0px 0px;text-align:right;'>%s</div>\n"), str_value);
    WSContentSend_PD (PSTR ("</div>\n"));
  }

  // display last presence detection
  if (sensor_status.pres.source != SENSOR_SOURCE_NONE) WSContentSend_PD (PSTR ("{s}Last presence{m}%s{e}"), str_time);
}

// remote sensor configuration web page
void SensorWebConfigure ()
{
  uint8_t index;
  float   drift, step;
  char    str_value[16];
  char    str_text[128];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess ()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg ("save"))
  {
    // get temperature drift according to 'tdrift' parameter
    WebGetArg (D_CMND_SENSOR_TEMP D_CMND_SENSOR_DRIFT, str_text, sizeof (str_text));
    if (strlen (str_text) > 0)
    {
      drift = atof (str_text) * 10;
      Settings->temp_comp = (int8_t)drift;
    }

    // set temperature timeout according to 'temptime' parameter
    WebGetArg (D_CMND_SENSOR_TEMP D_CMND_SENSOR_TIMEOUT, str_text, sizeof (str_text));
    sensor_config.temp.validity = (uint16_t)atoi (str_text);

    // set temperature topic according to 'temptopic' parameter
    WebGetArg (D_CMND_SENSOR_TEMP D_CMND_SENSOR_TOPIC, str_text, sizeof (str_text));
    sensor_config.temp.topic = str_text;

    // set temperature key according to 'tempkey' parameter
    WebGetArg (D_CMND_SENSOR_TEMP D_CMND_SENSOR_KEY, str_text, sizeof (str_text));
    sensor_config.temp.key = str_text;

    // set humidity timeout according to 'humitime' parameter
    WebGetArg (D_CMND_SENSOR_HUMI D_CMND_SENSOR_TIMEOUT, str_text, sizeof (str_text));
    sensor_config.humi.validity = (uint16_t)atoi (str_text);

    // set humidity topic according to 'humitopic' parameter
    WebGetArg (D_CMND_SENSOR_HUMI D_CMND_SENSOR_TOPIC, str_text, sizeof (str_text));
    sensor_config.humi.topic = str_text;

    // set humidity key according to 'humikey' parameter
    WebGetArg (D_CMND_SENSOR_HUMI D_CMND_SENSOR_KEY, str_text, sizeof (str_text));
    sensor_config.humi.key = str_text;

    // set presence timeout according to 'prestime' parameter
    WebGetArg (D_CMND_SENSOR_PRES D_CMND_SENSOR_TIMEOUT, str_text, sizeof (str_text));
    sensor_config.pres.validity = (uint32_t)atoi (str_text);

    // set presence topic according to 'prestopic' parameter
    WebGetArg (D_CMND_SENSOR_PRES D_CMND_SENSOR_TOPIC, str_text, sizeof (str_text));
    sensor_config.pres.topic = str_text;

    // set presence key according to 'preskey' parameter
    WebGetArg (D_CMND_SENSOR_PRES D_CMND_SENSOR_KEY, str_text, sizeof (str_text));
    sensor_config.pres.key = str_text;

    // set sensor type according to 'prestype' parameter
    WebGetArg (D_CMND_SENSOR_PRES D_CMND_SENSOR_TYPE, str_text, sizeof (str_text));
    sensor_config.presence = (uint8_t)atoi (str_text);

    // save config
    SensorSaveConfig ();
  }

  // beginning of form
  WSContentStart_P (D_SENSOR_CONFIGURE);
  WSContentSendStyle ();
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("span.key {float:right;font-size:0.7rem;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_SENSOR_PAGE_CONFIG);

  //      temperature  
  // ----------------------

  WSContentSend_P (SENSOR_FIELDSET_START, "🌡️", D_SENSOR_TEMPERATURE);

  // Correction 
  WSContentSend_P (PSTR ("<p>\n"));
  step = SENSOR_TEMP_DRIFT_STEP;
  ext_snprintf_P (str_text, sizeof (str_text), PSTR ("%1_f"), &step);
  drift = (float)Settings->temp_comp / 10;
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &drift);
  WSContentSend_P (SENSOR_FIELD_CONFIG, D_SENSOR_CORRECTION, "°C", D_CMND_SENSOR_TEMP D_CMND_SENSOR_DRIFT, D_CMND_SENSOR_TEMP D_CMND_SENSOR_DRIFT, - SENSOR_TEMP_DRIFT_MAX, SENSOR_TEMP_DRIFT_MAX, str_text, str_value);
  WSContentSend_P (PSTR ("</p>\n"));

  // timeout 
  WSContentSend_P (PSTR ("<p>\n"));
  sprintf (str_value, "%u", sensor_config.temp.validity);
  WSContentSend_P (SENSOR_FIELD_CONFIG, D_SENSOR_TIMEOUT, "sec.", D_CMND_SENSOR_TEMP D_CMND_SENSOR_TIMEOUT, D_CMND_SENSOR_TEMP D_CMND_SENSOR_TIMEOUT, 10, 2500, "10", str_value);
  WSContentSend_P (PSTR ("</p>\n"));

  WSContentSend_P (PSTR ("<hr>\n"));

  // topic and key
  WSContentSend_P (PSTR ("<p>\n"));

  WSContentSend_P (SENSOR_FIELD_INPUT, D_SENSOR_TOPIC, D_CMND_SENSOR_TEMP D_CMND_SENSOR_TOPIC, D_CMND_SENSOR_TEMP D_CMND_SENSOR_TOPIC, sensor_config.temp.topic.c_str ());
  WSContentSend_P (SENSOR_FIELD_INPUT, D_SENSOR_KEY, D_CMND_SENSOR_TEMP D_CMND_SENSOR_KEY, D_CMND_SENSOR_TEMP D_CMND_SENSOR_KEY, sensor_config.temp.key.c_str ());

  WSContentSend_P (PSTR ("</p>\n"));
  WSContentSend_P (SENSOR_FIELDSET_STOP);

  //   remote humidity  
  // ----------------------

  WSContentSend_P (SENSOR_FIELDSET_START, "💧", D_SENSOR_HUMIDITY);

  // timeout 
  WSContentSend_P (PSTR ("<p>\n"));
  sprintf (str_value, "%u", sensor_config.humi.validity);
  WSContentSend_P (SENSOR_FIELD_CONFIG, D_SENSOR_TIMEOUT, "sec.", D_CMND_SENSOR_HUMI D_CMND_SENSOR_TIMEOUT, D_CMND_SENSOR_HUMI D_CMND_SENSOR_TIMEOUT, 10, 2500, "10", str_value);
  WSContentSend_P (PSTR ("</p>\n"));

  WSContentSend_P (PSTR ("<hr>\n"));

  // topic and key
  WSContentSend_P (PSTR ("<p>\n"));

  WSContentSend_P (SENSOR_FIELD_INPUT, D_SENSOR_TOPIC, D_CMND_SENSOR_HUMI D_CMND_SENSOR_TOPIC, D_CMND_SENSOR_HUMI D_CMND_SENSOR_TOPIC, sensor_config.humi.topic.c_str ());
  WSContentSend_P (SENSOR_FIELD_INPUT, D_SENSOR_KEY, D_CMND_SENSOR_HUMI D_CMND_SENSOR_KEY, D_CMND_SENSOR_HUMI D_CMND_SENSOR_KEY, sensor_config.humi.key.c_str ());

  WSContentSend_P (PSTR ("</p>\n"));
  WSContentSend_P (SENSOR_FIELDSET_STOP);

  //   remote presence  
  // ----------------------

  WSContentSend_P (SENSOR_FIELDSET_START, "🧑", D_SENSOR_PRESENCE);

  // timeout 
  WSContentSend_P (PSTR ("<p>\n"));
  sprintf (str_value, "%u", sensor_config.pres.validity);
  WSContentSend_P (SENSOR_FIELD_CONFIG, D_SENSOR_TIMEOUT, "sec.", D_CMND_SENSOR_PRES D_CMND_SENSOR_TIMEOUT, D_CMND_SENSOR_PRES D_CMND_SENSOR_TIMEOUT, 10, 2500, "10", str_value);
  WSContentSend_P (PSTR ("</p>\n"));

  WSContentSend_P (PSTR ("<hr>\n"));
  
  // topic and key 
  WSContentSend_P (PSTR ("<p>\n"));

  WSContentSend_P (SENSOR_FIELD_INPUT, D_SENSOR_TOPIC, D_CMND_SENSOR_PRES D_CMND_SENSOR_TOPIC, D_CMND_SENSOR_PRES D_CMND_SENSOR_TOPIC, sensor_config.pres.topic.c_str ());
  WSContentSend_P (SENSOR_FIELD_INPUT, D_SENSOR_KEY, D_CMND_SENSOR_PRES D_CMND_SENSOR_KEY, D_CMND_SENSOR_PRES D_CMND_SENSOR_KEY, sensor_config.pres.key.c_str ());

  WSContentSend_P (PSTR ("</p>\n"));
  WSContentSend_P (SENSOR_FIELDSET_STOP);

  //   presence sensors  
  // ----------------------

  WSContentSend_P (SENSOR_FIELDSET_START, "⚙️", "Presence Sensor");

  for (index = 0; index < SENSOR_PRESENCE_MAX; index ++)
  {
    GetTextIndexed (str_text, sizeof (str_text), index, kSensorPresenceModel);
    if (sensor_config.presence == index) strcpy_P (str_value, PSTR ("checked")); else strcpy (str_value, "");
    WSContentSend_P (PSTR ("<p><input type='radio' name='%s' value='%d' %s>%s</p>\n"), D_CMND_SENSOR_PRES D_CMND_SENSOR_TYPE, index, str_value, str_text);
  }

  WSContentSend_P (SENSOR_FIELDSET_STOP);

  // end of form and save button
  WSContentSend_P (PSTR ("<p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), D_SAVE);
  WSContentSend_P (PSTR ("</form>\n"));

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

// Sensor graph style
void SensorWebGraphSwipe ()
{
  // javascript : screen swipe for previous and next period
  WSContentSend_P (PSTR("\nlet startX=0;let stopX=0;\n"));
  WSContentSend_P (PSTR("window.addEventListener('touchstart',function(evt){startX=evt.changedTouches[0].screenX;},false);\n"));
  WSContentSend_P (PSTR("window.addEventListener('touchend',function(evt){stopX=evt.changedTouches[0].screenX;handleGesture();},false);\n"));
  WSContentSend_P (PSTR("function handleGesture(){\n"));
  WSContentSend_P (PSTR("if(stopX<startX-100){document.getElementById('next').click();}\n"));
  WSContentSend_P (PSTR("else if(stopX>startX+100){document.getElementById('prev').click();}\n"));
  WSContentSend_P (PSTR("}\n"));
}

// --------------------
//    Measure Graph
// --------------------

// Sensor graph style
void SensorWebGraphWeeklyCurveStyle ()
{
  WSContentSend_P (PSTR("button {padding:2px 12px;font-size:2.5vh;background:none;color:#fff;border:1px #666 solid;border-radius:6px;}\n"));
  WSContentSend_P (PSTR("button:hover {background:#aaa;}\n"));
  WSContentSend_P (PSTR("button:disabled {color:#252525;background:#252525;border:1px #252525 solid;}\n"));

  WSContentSend_P (PSTR("div.banner {width:88%%;}\n"));
  WSContentSend_P (PSTR("div.banner div {display:inline-block;}\n"));
  WSContentSend_P (PSTR("div.date {font-size:2vh;}\n"));
  WSContentSend_P (PSTR("div.prev {float:left;}\n"));
  WSContentSend_P (PSTR("div.next {float:right;}\n"));

  WSContentSend_P (PSTR("div.graph {width:100%%;margin:1vh auto;}\n"));
  WSContentSend_P (PSTR("svg.graph {width:100%%;height:50vh;}\n"));
}

// Sensor graph curve display
void SensorWebGraphWeeklyCurve (const uint16_t week, const char* pstr_url)
{
  bool     data_ok = true;
  uint8_t  day, hour, slot, counter, value;
  int16_t  scale, temp_range, temp_incr, temp_min, temp_max;
  uint16_t act_week, ref_week, prev_week, next_week, shift_week;
  uint32_t index, unit, graph_width, graph_x, prev_x, graph_y, prev_y;
  char     str_type[8];
  char     str_text[48];
  String   str_result;

  // data in memory
  if (week == 0) 
    for (day = 0; day < 7; day++)
      for (hour = 0; hour < 24; hour++)
        for (slot = 0; slot < 6; slot++)
        {
          sensor_graph.temp[day][hour][slot] = sensor_week.temp[day][hour][slot];
          sensor_graph.humi[day][hour][slot] = sensor_week.humi[day][hour][slot];
          sensor_graph.event[day][hour][slot].data = sensor_week.event[day][hour][slot].data;
        }

#ifdef USE_UFILESYS
  // load data from file
  else data_ok = SensorFileLoadWeek (week);
#endif      // USE_UFILESYS

  // if data is not loaded, return
  if (!data_ok) return;

  // boundaries of SVG graph
  graph_width = SENSOR_GRAPH_STOP - SENSOR_GRAPH_START;

  // loop to calculate minimum and maximum temperature for the graph
  if (sensor_status.temp.weekly)
  {
    temp_min = INT16_MAX;
    temp_max = INT16_MAX;
    for (day = 0; day < 7; day++)
      for (hour = 0; hour < 24; hour++)
        for (slot = 0; slot < 6; slot++)
        {
          // minimum temperature
          if (temp_min == INT16_MAX) temp_min = sensor_graph.temp[day][hour][slot];
            else if (sensor_graph.temp[day][hour][slot] != INT16_MAX) temp_min = min (temp_min, sensor_graph.temp[day][hour][slot]);

          // maximum temperature
          if (temp_max == INT16_MAX) temp_max = sensor_graph.temp[day][hour][slot];
            else if (sensor_graph.temp[day][hour][slot] != INT16_MAX) temp_max = max (temp_max, sensor_graph.temp[day][hour][slot]);
        }

    // set upper and lower range according to min and max temperature
    if (temp_min != INT16_MAX) temp_min = (temp_min / 10 - 2) * 10;
    if (temp_max != INT16_MAX) temp_max = (temp_max / 10 + 2) * 10;

    // calculate temperature range and increment
    temp_range = temp_max - temp_min;
    temp_incr  = max (temp_range / 40 * 10, 10);
  }

#ifdef USE_UFILESYS
 
  // calculate actual week (week start on monday)
  act_week = SensorGetCurrentWeek ();
  if (week == 0) ref_week = act_week;
    else ref_week = week;

  // previous week
  prev_week = (ref_week + 52 - 1) % 52;
  if (prev_week == 0) prev_week = 52;

  // next week
  next_week = (ref_week + 1) % 52;
  if (next_week == 0) next_week = 52;
  if (ref_week == act_week - 1) next_week = 0;

  // start of form
  WSContentSend_P(PSTR("<form method='get' action='/%s'>\n"), pstr_url);

  // date
  WSContentSend_P (PSTR("<div class='banner'>\n"));
  if (ref_week <= act_week) shift_week = act_week - ref_week;
    else shift_week = 53 - ref_week + act_week;
  SensorGetWeekLabel (shift_week, str_text, sizeof (str_text));
  WSContentSend_P (PSTR("<div class='date'>%s</div>\n"), str_text);

  // if exist, navigation to previous week
  if (SensorFileExist (prev_week)) strcpy (str_text, "");
    else strcpy (str_text, "disabled");
  WSContentSend_P (PSTR("<div class='prev'><button name='week' value=%u id='prev' %s>&lt;&lt;</button></div>\n"), prev_week, str_text);

  // if exist, navigation to next week or to live values
  if (next_week == 0) strcpy (str_text, "");
    else if (SensorFileExist (next_week)) strcpy (str_text, ""); 
    else strcpy (str_text, "disabled");
  WSContentSend_P (PSTR("<div class='next'><button name='week' value=%u id='next' %s>&gt;&gt;</button></div>\n"), next_week, str_text);
  WSContentSend_P (PSTR("</div>\n"));     // banner

  // end of form
  WSContentSend_P(PSTR("<form method='get' action='/%s'>\n"), D_SENSOR_PAGE_MEASURE);

#endif    // USE_UFILESYS

  // start of SVG graph
  WSContentSend_P (PSTR("<div class='graph'>\n"));
  WSContentSend_P (PSTR("<svg class='graph' viewBox='%d %d %d %d' preserveAspectRatio='none'>\n"), 0, 0, SENSOR_GRAPH_WIDTH, SENSOR_GRAPH_HEIGHT + 1);

  // SVG style
  WSContentSend_P (PSTR("<style type='text/css'>\n"));

  WSContentSend_P (PSTR ("text {font-size:18px}\n"));

  WSContentSend_P (PSTR ("polyline.temp {stroke:%s;fill:none;stroke-width:2;}\n"), SENSOR_COLOR_TEMP);
  WSContentSend_P (PSTR ("path.humi {stroke:%s;fill:%s;opacity:1;fill-opacity:0.5;}\n"), SENSOR_COLOR_HUMI, SENSOR_COLOR_HUMI);

  WSContentSend_P (PSTR ("rect {opacity:0.8;}\n"));
  WSContentSend_P (PSTR ("rect.acti {fill:%s;}\n"), SENSOR_COLOR_ACTI);
  WSContentSend_P (PSTR ("rect.inac {fill:%s;}\n"), SENSOR_COLOR_INAC);
  WSContentSend_P (PSTR ("rect.frame {stroke:white;fill:none;}\n"));

  WSContentSend_P (PSTR ("line {stroke-dasharray:1 2;stroke-width:0.5;}\n"));
  WSContentSend_P (PSTR ("line.time {stroke:%s;}\n"), SENSOR_COLOR_LINE);
  WSContentSend_P (PSTR ("line.temp {stroke:%s;}\n"), SENSOR_COLOR_TEMP);
  WSContentSend_P (PSTR ("line.humi {stroke:%s;}\n"), SENSOR_COLOR_HUMI);

  WSContentSend_P (PSTR ("text.day {fill:%s;font-size:16px;}\n"), SENSOR_COLOR_TIME);
  WSContentSend_P (PSTR ("text.today {fill:%s;font-size:16px;font-weight:bold;}\n"), SENSOR_COLOR_TODAY);
  WSContentSend_P (PSTR ("text.temp {fill:%s;}\n"), SENSOR_COLOR_TEMP);
  WSContentSend_P (PSTR ("text.humi {fill:%s;}\n"), SENSOR_COLOR_HUMI);

  WSContentSend_P(PSTR("</style>\n"));

  // ------  Humidity  ---------

  if (sensor_status.humi.weekly)
  {
    // init
    index   = 0;
    graph_x = prev_x = SENSOR_GRAPH_START;
    graph_y = prev_y = UINT32_MAX;
    str_result = "";

    // loop thru points
    for (counter = 0; counter < 7; counter++)
    {
      day = ((RtcTime.day_of_week + counter) % 7);
      for (hour = 0; hour < 24; hour++)
        for (slot = 0; slot < 6; slot++)
        {
          // calculate x coordinate
          graph_x = SENSOR_GRAPH_START + (index * graph_width / 1008);

          // calculate y coordinate
          if (sensor_graph.humi[day][hour][slot] != INT8_MAX) graph_y = SENSOR_GRAPH_HEIGHT - SENSOR_GRAPH_HEIGHT * (uint32_t)sensor_graph.humi[day][hour][slot] / 100;
            else graph_y = UINT32_MAX;

          // if needed, start path
          if ((graph_y != UINT32_MAX) && (prev_y == UINT32_MAX))  { sprintf (str_text, "<path class='humi' d='M%d %d ", graph_x, SENSOR_GRAPH_HEIGHT); str_result += str_text; }

          // else if needed, stop path
          else if ((graph_y == UINT32_MAX) && (prev_y != UINT32_MAX)) { sprintf (str_text, "L%d %d '/>\n", prev_x, SENSOR_GRAPH_HEIGHT); str_result += str_text; }

          // if needed, draw point
          else if (graph_y != UINT32_MAX) { sprintf (str_text, "L%u %u ", graph_x, graph_y); str_result += str_text; }

          // save previous values
          prev_x = graph_x;
          prev_y = graph_y;
          index++;

          // if needed, publish result
          if (str_result.length () > 256) { WSContentSend_P (str_result.c_str ()); str_result = ""; }
        }
    }

    // end of graph
    if (graph_y != UINT32_MAX) { sprintf (str_text, "L%u %u '/>\n", graph_x, SENSOR_GRAPH_HEIGHT); str_result += str_text; }
    WSContentSend_P (str_result.c_str ());
  }

  // ---------  Temperature  ---------

  if (sensor_status.temp.weekly)
  {
    // init
    index   = 0;
    graph_x = prev_x = SENSOR_GRAPH_START;
    graph_y = prev_y = UINT32_MAX;
    str_result = "";

    // loop thru points
    for (counter = 0; counter < 7; counter++)
    {
      if (week == 0) day = ((RtcTime.day_of_week + counter) % 7);
        else day = (counter + 1) % 7;
      for (hour = 0; hour < 24; hour++)
        for (slot = 0; slot < 6; slot++)
        {
          // calculate x coordinate
          graph_x = SENSOR_GRAPH_START + (index * graph_width / 1008);

          // calculate y coordinate
          if (sensor_graph.temp[day][hour][slot] == INT16_MAX) graph_y = UINT32_MAX;
            else graph_y = SENSOR_GRAPH_HEIGHT - SENSOR_GRAPH_HEIGHT * (uint32_t)(sensor_graph.temp[day][hour][slot] - temp_min) / (uint32_t)temp_range;

          // if needed, start polyline
          if ((graph_y != UINT32_MAX) && (prev_y == UINT32_MAX)) str_result += "<polyline class='temp' points='";

          // else if needed, stop polyline
          else if ((graph_y == UINT32_MAX) && (prev_y != UINT32_MAX)) str_result += "'/>\n";

          // if needed, draw point
          if (graph_y != UINT32_MAX) { sprintf (str_text, "%u,%u ", graph_x, graph_y); str_result += str_text; }

          // save previous values
          prev_x = graph_x;
          prev_y = graph_y;
          index++;

          // if needed, publish result
          if (str_result.length () > 256) { WSContentSend_P (str_result.c_str ()); str_result = ""; }
        }
    }

    if (graph_y != UINT32_MAX) str_result += "'/>\n";
    WSContentSend_P (str_result.c_str ());
  }

  // ---------  Activity  ---------

  if (sensor_status.acti.weekly)
  {
    // init
    index   = 0;
    graph_x = prev_x = UINT32_MAX;
    graph_y = prev_y = SENSOR_GRAPH_HEIGHT - SENSOR_GRAPH_ACTI;

    // loop thru points
    for (counter = 0; counter < 7; counter++)
    {
      day = ((RtcTime.day_of_week + counter) % 7);
      for (hour = 0; hour < 24; hour++)
        for (slot = 0; slot < 6; slot++)
        {
          // read slot status
          value = sensor_graph.event[day][hour][slot].activity;

          // start and end of activity
          if ((prev_x == UINT32_MAX) && (value == 1)) prev_x = SENSOR_GRAPH_START + index * graph_width / 1008;
            else if ((prev_x != UINT32_MAX) && (value == 0)) graph_x = SENSOR_GRAPH_START + index * graph_width / 1008;

          // if activity should be drawn
          if ((prev_x != UINT32_MAX) && (graph_x != UINT32_MAX))
          {
            WSContentSend_P ("<rect class='acti' x=%d y=%d width=%d height=%d />\n", prev_x, graph_y, graph_x - prev_x, SENSOR_GRAPH_ACTI);
            prev_x = graph_x = UINT32_MAX;
          }

          // increase index
          index++;
        } 
    }

    if (prev_x != UINT32_MAX) WSContentSend_P ("<rect class='acti' x=%d y=%d width=%d height=%d />\n", prev_x, graph_y, graph_x - prev_x, SENSOR_GRAPH_ACTI);
  }

  // ---------  Inactivity  ---------

  if (sensor_status.inac.weekly)
  {
    // init
    index   = 1;
    graph_x = prev_x = UINT32_MAX;
    graph_y = prev_y = SENSOR_GRAPH_HEIGHT - SENSOR_GRAPH_INAC;

    // loop thru points
    for (counter = 0; counter < 7; counter++)
    {
      day = ((RtcTime.day_of_week + counter) % 7);
      for (hour = 0; hour < 24; hour++)
        for (slot = 0; slot < 6; slot++)
        {
          // read slot status
          value = sensor_graph.event[day][hour][slot].inactivity;

          // start and end of activity
          if ((prev_x == UINT32_MAX) && (value == 1)) prev_x = SENSOR_GRAPH_START + (index * graph_width / 1008);
            else if ((prev_x != UINT32_MAX) && (value != 1)) graph_x = SENSOR_GRAPH_START + (index - 1) * graph_width / 1008;

          // if activity should be drawn
          if ((prev_x != UINT32_MAX) && (graph_x != UINT32_MAX))
          {
            WSContentSend_P ("<rect class='inac' x=%d y=%d width=%d height=%d />\n", prev_x, graph_y, graph_x - prev_x, SENSOR_GRAPH_INAC);
            prev_x = graph_x = UINT32_MAX;
          }

          // increase index
          index++;
        } 
    }

    if (prev_x != UINT32_MAX) WSContentSend_P ("<rect class='inac' x=%d y=%d width=%d height=%d />\n", prev_x, graph_y, graph_x - prev_x, SENSOR_GRAPH_INAC);
  }

  // -------  Frame  -------

  WSContentSend_P (PSTR ("<rect class='frame' x=%u y=%u width=%u height=%u rx=4 />\n"), SENSOR_GRAPH_START, 0, SENSOR_GRAPH_STOP - SENSOR_GRAPH_START, SENSOR_GRAPH_HEIGHT + 1);

  // -------  Time line  --------

  // display week days
  for (counter = 0; counter < 7; counter++)
  {
    // get day label
    if (week == 0) day = ((RtcTime.day_of_week + counter) % 7);
      else day = (counter + 1) % 7;
    strlcpy (str_text, kWeekdayNames + day * 3, 4);
    if ((week == 0) && (counter == 6)) strcpy (str_type, "today");
      else strcpy (str_type, "day");

    // display day
    graph_x = SENSOR_GRAPH_START + (counter * 2 + 1) * graph_width / 14 - 10;
    WSContentSend_P (PSTR ("<text class='%s' x=%u y=%u>%s</text>\n"), str_type, graph_x, 20, str_text);
  }

  // display separation lines
  for (index = 1; index < 7; index++)
  {
    graph_x = SENSOR_GRAPH_START + index * graph_width / 7;
    WSContentSend_P (PSTR ("<line class='time' x1=%u y1=%u x2=%u y2=%u />\n"), graph_x, 5, graph_x, SENSOR_GRAPH_HEIGHT - 5);
  }

  // -------  Units  -------

  // temperature units
  if (sensor_status.temp.weekly)
  {
    unit = max (temp_range / temp_incr + 1, 2);
    for (index = 0; index < unit; index ++)
    {
      // calculate temperature scale
      scale = temp_min + temp_incr * (int16_t)index;
      scale = min (scale, temp_max);

      // calculate position
      if (index == unit - 1) graph_y = 4 * SENSOR_GRAPH_HEIGHT / 100;
        else graph_y = SENSOR_GRAPH_HEIGHT - SENSOR_GRAPH_HEIGHT * index / (unit - 1);

      // value and line
      WSContentSend_P (PSTR ("<text class='temp' x=%u%% y=%u>%d °C</text>\n"), 0, graph_y, scale / 10);
      if ((index > 0) && (index < unit - 1)) WSContentSend_P (PSTR ("<line class='temp' x1=%u y1=%u x2=%u y2=%u />\n"), SENSOR_GRAPH_START, graph_y, SENSOR_GRAPH_STOP, graph_y);
    }
  }

  // humidity units
  if (sensor_status.humi.weekly)
  {
    for (index = 0; index < 5; index ++)
    {
      // calculate humidity scale
      scale = 25 * index;

      // calculate position
      if (index == 4) graph_y = 4 * SENSOR_GRAPH_HEIGHT / 100;
        else graph_y = SENSOR_GRAPH_HEIGHT - SENSOR_GRAPH_HEIGHT * index / 4;

      WSContentSend_P (PSTR ("<text class='humi' x=%u%% y=%u>%u %%</text>\n"), SENSOR_GRAPH_STOP_PERCENT + 1, graph_y, scale);
      if ((index > 0) && (index < 4)) WSContentSend_P (PSTR ("<line class='humi' x1=%u y1=%u x2=%u y2=%u />\n"), SENSOR_GRAPH_START, graph_y, SENSOR_GRAPH_STOP, graph_y);
    }
  }

  // --------  End  ---------

  WSContentSend_P(PSTR("</svg>\n"));      // graph
  WSContentSend_P(PSTR("</div>\n"));      // graph
}

// Temperature and humidity weekly page
void SensorWebWeeklyMeasure ()
{
  uint16_t week = 0;
  char     str_value[4];

  // if target temperature has been changed
  if (Webserver->hasArg ("week"))
  {
    WebGetArg ("week", str_value, sizeof(str_value));
    if (strlen(str_value) > 0) week = atoi (str_value);
  }

  // set page label
  WSContentStart_P ("Weekly Measure", false);

  // graph swipe section script
  SensorWebGraphSwipe ();
  WSContentSend_P (PSTR ("</script>\n"));

  // set page as scalable
  WSContentSend_P (PSTR ("<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=yes'/>\n"));

  // refresh every 5 mn
  WSContentSend_P (PSTR ("<meta http-equiv='refresh' content='%u'/>\n"), 300);

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));

  WSContentSend_P (PSTR ("div {margin:0.25rem auto;padding:0.1rem 0px;text-align:center;}\n"));

  WSContentSend_P (PSTR ("div.title {font-size:3.5vh;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.header {font-size:2.5vh;margin:2vh auto;}\n"));

  WSContentSend_P (PSTR ("a {color:white;}\n"));
  WSContentSend_P (PSTR ("a:link {text-decoration:none;}\n"));

  WSContentSend_P (PSTR ("span {font-size:4vh;padding:0vh 3vh;}\n"));
  WSContentSend_P (PSTR ("span#humi {color:%s;}\n"), SENSOR_COLOR_HUMI);
  WSContentSend_P (PSTR ("span#temp {color:%s;}\n"), SENSOR_COLOR_TEMP);

  // graph section style
  SensorWebGraphWeeklyCurveStyle ();

  WSContentSend_P (PSTR ("</style>\n"));

  WSContentSend_P (PSTR ("</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  // room name
  WSContentSend_P (PSTR ("<div class='title'><a href='/'>%s</a></div>\n"), SettingsText(SET_DEVICENAME));

  // header
  WSContentSend_P (PSTR ("<div class='header'>Weekly Measure</div>\n"));

  // ------- Values --------

  WSContentSend_P (PSTR ("<div class='value'><a href='/'>"));

  // temperature
  if (sensor_status.temp.weekly)
    if (sensor_status.temp.value != INT16_MAX) WSContentSend_P (PSTR ("<span id='temp'>%d.%d °C</span>"), sensor_status.temp.value / 10, sensor_status.temp.value % 10);
      else WSContentSend_P (PSTR ("<span id='temp'>--- °C</span>"));

  // humidity
  if (sensor_status.humi.weekly)
    if (sensor_status.humi.value != INT16_MAX) WSContentSend_P (PSTR ("<span id='humi'>%d.%d %%</span>"), sensor_status.humi.value / 10, sensor_status.humi.value % 10);
      else WSContentSend_P (PSTR ("<span id='humi'>--- %%</span>"));

  WSContentSend_P (PSTR ("</a></div>\n"));

  // ------- Graph --------

  SensorWebGraphWeeklyCurve (week, D_SENSOR_PAGE_MEASURE);

  // end of page
  WSContentStop ();
}


// Sensor graph style
void SensorWebGraphYearlyCurveStyle ()
{
  WSContentSend_P (PSTR("div.graph {width:100%%;margin:1vh auto;}\n"));
  WSContentSend_P (PSTR("svg.graph {width:100%%;height:50vh;}\n"));
}

// Sensor graph curve display
void SensorWebGraphYearlyCurve ()
{
  int16_t  temp_min, temp_max, temp_range, temp_incr, scale;
  uint32_t counter, index, day, month, start;
  uint32_t unit, graph_width, graph_x, prev_x, graph_y, prev_y;
//  char     str_type[8];
  char     str_text[48];
  String   str_result;

  // boundaries of SVG graph
  graph_width = SENSOR_GRAPH_STOP - SENSOR_GRAPH_START;

  // loop to calculate minimum and maximum temperature for the graph
  temp_min = INT16_MAX;
  temp_max = INT16_MAX;
  for (index = 0; index < SENSOR_YEAR_DAYS; index++)
  {
    // minimum temperature
    if (temp_min == INT16_MAX) temp_min = sensor_year.temp_min[index];
      else if (sensor_year.temp_min[index] != INT16_MAX) temp_min = min (temp_min, sensor_year.temp_min[index]);

    // maximum temperature
    if (temp_max == INT16_MAX) temp_max = sensor_year.temp_max[index];
      else if (sensor_year.temp_max[index] != INT16_MAX) temp_max = max (temp_max, sensor_year.temp_max[index]);
  }

  // set upper and lower range according to min and max temperature
  if (temp_min != INT16_MAX) temp_min = (temp_min / 10 - 2) * 10;
  if (temp_max != INT16_MAX) temp_max = (temp_max / 10 + 2) * 10;

  // calculate temperature range and increment
  temp_range = temp_max - temp_min;
  temp_incr  = max (temp_range / 40 * 10, 10);
  start      = RtcTime.day_of_year;

  // start of SVG graph
  WSContentSend_P (PSTR("<div class='graph'>\n"));
  WSContentSend_P (PSTR("<svg class='graph' viewBox='%d %d %d %d' preserveAspectRatio='none'>\n"), 0, 0, SENSOR_GRAPH_WIDTH, SENSOR_GRAPH_HEIGHT + 1);

  // SVG style
  WSContentSend_P (PSTR("<style type='text/css'>\n"));

  WSContentSend_P (PSTR ("text {font-size:18px}\n"));

  WSContentSend_P (PSTR ("rect {opacity:0.8;}\n"));
  WSContentSend_P (PSTR ("rect.frame {stroke:white;fill:none;}\n"));

  WSContentSend_P (PSTR ("polyline {fill:none;}\n"));
  WSContentSend_P (PSTR ("polyline.max {stroke-width:2;stroke:%s;}\n"), SENSOR_COLOR_TEMP_MAX);
  WSContentSend_P (PSTR ("polyline.min {stroke-width:1;stroke:%s;}\n"), SENSOR_COLOR_TEMP_MIN);

  WSContentSend_P (PSTR ("line {stroke-dasharray:1 2;stroke-width:0.5;}\n"));
  WSContentSend_P (PSTR ("line.time {stroke:%s;}\n"), SENSOR_COLOR_LINE);
  WSContentSend_P (PSTR ("line.temp {stroke:%s;}\n"), SENSOR_COLOR_TEMP);

  WSContentSend_P (PSTR ("text.month {fill:%s;font-size:16px;}\n"), SENSOR_COLOR_TIME);
  WSContentSend_P (PSTR ("text.temp {fill:%s;}\n"), SENSOR_COLOR_TEMP);

  WSContentSend_P(PSTR("</style>\n"));

  // ---------  Maximum Temperature  ---------

  // loop thru points
  str_result = "";
  graph_x = prev_x = UINT32_MAX;
  graph_y = prev_y = UINT32_MAX;
  index = 0;
  for (counter = start; counter < start + SENSOR_YEAR_DAYS; counter++)
  {
        // calculate day in array and x coordinate
        day = counter % SENSOR_YEAR_DAYS;
        graph_x = SENSOR_GRAPH_START + (index * graph_width / SENSOR_YEAR_DAYS);

        // calculate y coordinate
        if (sensor_year.temp_max[day] == INT16_MAX) graph_y = UINT32_MAX;
          else graph_y = SENSOR_GRAPH_HEIGHT - SENSOR_GRAPH_HEIGHT * (uint32_t)(sensor_year.temp_max[day] - temp_min) / (uint32_t)temp_range;

        // if curve restart from beginning
        if ((graph_x < prev_x) && (prev_x != UINT32_MAX)) str_result += "'/>\n<polyline class='max' points='";

        // if needed, start polyline
        else if ((graph_y != UINT32_MAX) && (prev_y == UINT32_MAX)) str_result += "<polyline class='max' points='";

        // else if needed, stop polyline
        else if ((graph_y == UINT32_MAX) && (prev_y != UINT32_MAX)) str_result += "'/>\n";

        // if needed, draw point
        if (graph_y != UINT32_MAX) { sprintf (str_text, "%u,%u ", graph_x, graph_y); str_result += str_text; }

        // save previous values
        prev_x = graph_x;
        prev_y = graph_y;
        index ++;

        // if needed, publish result
        if (str_result.length () > 256) { WSContentSend_P (str_result.c_str ()); str_result = ""; }
  }
  if (graph_y != UINT32_MAX) str_result += "'/>\n";
  WSContentSend_P (str_result.c_str ());

  // ---------  Minimum Temperature  ---------

  // loop thru points
  str_result = "";
  graph_x = prev_x = UINT32_MAX;
  graph_y = prev_y = UINT32_MAX;
  index = 0;
  for (counter = start; counter < start + SENSOR_YEAR_DAYS; counter++)
  {
        // calculate day in array and x coordinate
        day = counter % SENSOR_YEAR_DAYS;
        graph_x = SENSOR_GRAPH_START + (index * graph_width / SENSOR_YEAR_DAYS);

        // calculate y coordinate
        if (sensor_year.temp_min[day] == INT16_MAX) graph_y = UINT32_MAX;
          else graph_y = SENSOR_GRAPH_HEIGHT - SENSOR_GRAPH_HEIGHT * (uint32_t)(sensor_year.temp_min[day] - temp_min) / (uint32_t)temp_range;

        // if curve restart from beginning
        if ((graph_x < prev_x) && (prev_x != UINT32_MAX)) str_result += "'/>\n<polyline class='min' points='";

        // if needed, start polyline
        else if ((graph_y != UINT32_MAX) && (prev_y == UINT32_MAX)) str_result += "<polyline class='min' points='";

        // else if needed, stop polyline
        else if ((graph_y == UINT32_MAX) && (prev_y != UINT32_MAX)) str_result += "'/>\n";

        // if needed, draw point
        if (graph_y != UINT32_MAX) { sprintf (str_text, "%u,%u ", graph_x, graph_y); str_result += str_text; }

        // save previous values
        prev_x = graph_x;
        prev_y = graph_y;
        index++;

        // if needed, publish result
        if (str_result.length () > 256) { WSContentSend_P (str_result.c_str ()); str_result = ""; }
  }
  if (graph_y != UINT32_MAX) str_result += "'/>\n";
  WSContentSend_P (str_result.c_str ());

  // -------  Frame  -------

  WSContentSend_P (PSTR ("<rect class='frame' x=%u y=%u width=%u height=%u rx=4 />\n"), SENSOR_GRAPH_START, 0, SENSOR_GRAPH_STOP - SENSOR_GRAPH_START, SENSOR_GRAPH_HEIGHT + 1);

  // -------  Time line  --------

  // display months
  day   = RtcTime.day_of_month;
  month = RtcTime.month - 1;
  if (day > 15) { month++; start = 30 - day + 15; }
    else { start = 15 - day; }
  for (counter = 0; counter < 12; counter++)
  {
    // get month name
    month = month % 12;
    strlcpy (str_text, kMonthNames + month * 3, 4);

    // display month
    if ((start > 3) && (start < 363))
    {
      graph_x = SENSOR_GRAPH_START + start * graph_width / SENSOR_YEAR_DAYS;
      WSContentSend_P (PSTR ("<text class='month' text-anchor='middle' x=%u y=%u>%s</text>\n"), graph_x, 20, str_text);
    } 

    // increment next position
    if (month % 2 == 0) start += 31; else start += 30;
    month ++;
  }
/*
  for (month = 0; month < 12; month++)
  {
    // index of first day of month
    index = ((month * 61 / 2) + 366 - RtcTime.day_of_year) % 366 + 10;

    // if possible, display month label
    if (index < 356)
    {
      // get month name
      strlcpy (str_text, kMonthNames + month * 3, 4);

      // display month
      graph_x = SENSOR_GRAPH_START + index * graph_width / 366;
      WSContentSend_P (PSTR ("<text class='month' x=%u y=%u>%s</text>\n"), graph_x, 20, str_text);
    } 
  }
*/

  // -------  Units  -------

  unit = max (temp_range / temp_incr + 1, 2);
  for (index = 0; index < unit; index ++)
  {
    // calculate temperature scale
    scale = temp_min + temp_incr * (int16_t)index;
    scale = min (scale, temp_max);

    // calculate position
    if (index == unit - 1) graph_y = 4 * SENSOR_GRAPH_HEIGHT / 100;
      else graph_y = SENSOR_GRAPH_HEIGHT - SENSOR_GRAPH_HEIGHT * index / (unit - 1);

    // value and line
    WSContentSend_P (PSTR ("<text class='temp' x=%u%% y=%u>%d °C</text>\n"), 0, graph_y, scale / 10);
    if ((index > 0) && (index < unit - 1)) WSContentSend_P (PSTR ("<line class='temp' x1=%u y1=%u x2=%u y2=%u />\n"), SENSOR_GRAPH_START, graph_y, SENSOR_GRAPH_STOP, graph_y);
  }

  // --------  End  ---------

  WSContentSend_P(PSTR("</svg>\n"));      // graph
  WSContentSend_P(PSTR("</div>\n"));      // graph
}

// Temperature yearly page
void SensorWebYearlyTemperature ()
{
  char str_value[4];

  // set page label
  WSContentStart_P (D_SENSOR_TEMPERATURE, false);
  WSContentSend_P (PSTR ("</script>\n"));

  // set page as scalable
  WSContentSend_P (PSTR ("<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=yes'/>\n"));

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));

  WSContentSend_P (PSTR ("div {margin:0.25rem auto;padding:0.1rem 0px;text-align:center;}\n"));

  WSContentSend_P (PSTR ("div.title {font-size:3.5vh;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.header {font-size:2.5vh;margin:2vh auto;}\n"));

  WSContentSend_P (PSTR ("a {color:white;}\n"));
  WSContentSend_P (PSTR ("a:link {text-decoration:none;}\n"));

  WSContentSend_P (PSTR ("span {font-size:4vh;padding:0vh 3vh;}\n"));
  WSContentSend_P (PSTR ("span#temp {color:%s;}\n"), SENSOR_COLOR_TEMP);

  // graph section style
  SensorWebGraphYearlyCurveStyle ();

  WSContentSend_P (PSTR ("</style>\n"));

  WSContentSend_P (PSTR ("</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  // room name and header
  WSContentSend_P (PSTR ("<div class='title'><a href='/'>%s</a></div>\n"), SettingsText(SET_DEVICENAME));
  WSContentSend_P (PSTR ("<div class='header'>Yearly %s</div>\n"), D_SENSOR_TEMPERATURE);

  // ------- Values --------

  WSContentSend_P (PSTR ("<div class='value'><a href='/'>"));

  // temperature
  if ((sensor_status.temp.source != SENSOR_SOURCE_NONE) && (sensor_status.temp.value != INT16_MAX)) WSContentSend_P (PSTR ("<span id='temp'>%d.%d °C</span>"), sensor_status.temp.value / 10, sensor_status.temp.value % 10);

  WSContentSend_P (PSTR ("</a></div>\n"));

  // ------- Graph --------

  SensorWebGraphYearlyCurve ();

  // end of page
  WSContentStop ();
}

// --------------------
//    Presence Graph
// --------------------

// Presence page style
void SensorWebGraphPresenceStyle ()
{
  WSContentSend_P (PSTR("button {padding:2px 12px;font-size:2.5vh;background:none;color:#fff;border:1px #666 solid;border-radius:6px;}\n"));
  WSContentSend_P (PSTR("button:hover {background:#aaa;}\n"));
  WSContentSend_P (PSTR("button:disabled {color:#252525;background:#252525;border:1px #252525 solid;}\n"));

  WSContentSend_P (PSTR("div.banner {width:88%%;}\n"));
  WSContentSend_P (PSTR("div.banner div {display:inline-block;}\n"));
  WSContentSend_P (PSTR("div.date {font-size:2vh;}\n"));
  WSContentSend_P (PSTR("div.prev {float:left;}\n"));
  WSContentSend_P (PSTR("div.next {float:right;}\n"));

  WSContentSend_P (PSTR ("table {width:88%%;margin:0px auto;border:none;color:#fff;border-collapse:separate;border-spacing:0 8px;}\n"));
  WSContentSend_P (PSTR ("th {text-align:center;font-size:0.8rem;}\n"));
  WSContentSend_P (PSTR ("th.first {text-align:left;}\n"));
  WSContentSend_P (PSTR ("th.last {text-align:right;}\n"));
  WSContentSend_P (PSTR ("td {height:32px;}\n"));
  WSContentSend_P (PSTR ("td.day {text-align:left;font-size:0.8rem;color:#aaa;}\n"));
  WSContentSend_P (PSTR ("td.today {text-align:left;font-size:0.8rem;font-weight:bold;color:white;}\n"));
  WSContentSend_P (PSTR ("td.on {background:%s;}\n"), SENSOR_COLOR_PRES);
  WSContentSend_P (PSTR ("td.off {background:#333;}\n"));
  WSContentSend_P (PSTR ("td.sep {border-right:1px dashed #888;}\n"));
}

// Presence history page
void SensorWebGraphPresenceCurve (const uint16_t week, const char* pstr_url)
{
  bool     data_ok = true;
  bool     change_slot;
  uint8_t  curr_slot, new_slot, curr_span, day_span;
  uint8_t  index, day, hour, slot;
  uint16_t act_week, ref_week, prev_week, next_week, shift_week;
  char     str_text[64];
  char     str_type[12];
  char     str_day[8];
  String   str_result;

  // data in memory
  if (week == 0) 
    for (day = 0; day < 7; day++)
      for (hour = 0; hour < 24; hour++)
        for (slot = 0; slot < 6; slot++)
          sensor_graph.event[day][hour][slot].data = sensor_week.event[day][hour][slot].data;

#ifdef USE_UFILESYS
  // load data from file
  else data_ok = SensorFileLoadWeek (week);
#endif      // USE_UFILESYS

  // if data is not loaded, return
  if (!data_ok) return;

#ifdef USE_UFILESYS
 
  // calculate actual week (week start on monday)
  act_week = SensorGetCurrentWeek ();
  if (week == 0) ref_week = act_week;
    else ref_week = week;

  // previous week
  prev_week = (ref_week + 52 - 1) % 52;
  if (prev_week == 0) prev_week = 52;

  // next week
  next_week = (ref_week + 1) % 52;
  if (next_week == 0) next_week = 52;
  if (ref_week == act_week - 1) next_week = 0;

  // start of form
  WSContentSend_P(PSTR("<form method='get' action='/%s'>\n"), pstr_url);

  // date
  WSContentSend_P (PSTR("<div class='banner'>\n"));
  if (ref_week <= act_week) shift_week = act_week - ref_week;
    else shift_week = 53 - ref_week + act_week;
  SensorGetWeekLabel (shift_week, str_text, sizeof (str_text));
  WSContentSend_P (PSTR("<div class='date'>%s</div>\n"), str_text);

  // if exist, navigation to previous week
  if (SensorFileExist (prev_week)) strcpy (str_text, "");
    else strcpy (str_text, "disabled");
  WSContentSend_P (PSTR("<div class='prev'><button name='week' value=%u id='prev' %s>&lt;&lt;</button></div>\n"), prev_week, str_text);

  // if exist, navigation to next week or to live values
  if (next_week == 0) strcpy (str_text, "");
    else if (SensorFileExist (next_week)) strcpy (str_text, ""); 
    else strcpy (str_text, "disabled");
  WSContentSend_P (PSTR("<div class='next'><button name='week' value=%u id='next' %s>&gt;&gt;</button></div>\n"), next_week, str_text);
  WSContentSend_P (PSTR("</div>\n"));     // banner

  // end of form
  WSContentSend_P(PSTR("<form method='get' action='/%s'>\n"), D_SENSOR_PAGE_PRESENCE);

#endif    // USE_UFILESYS

  // display table with header
  WSContentSend_P (PSTR ("<div><table cellspacing=0>\n"));

  // display header
  WSContentSend_P (PSTR ("<tr><th colspan=12></th>"));
  WSContentSend_P (PSTR ("<th colspan=12 class='first'>0h</th>"));
  WSContentSend_P (PSTR ("<th colspan=24>04h</th><th colspan=24>08h</th><th colspan=24>12h</th><th colspan=24>16h</th><th colspan=24>20h</th>"));
  WSContentSend_P (PSTR ("<th colspan=12 class='last'>24h</th></tr>\n"));

  // loop thru days
  str_result = "";
  for (index = 0; index < 7; index++)
  {
    // shift day according to current day
    if (week == 0) day = (RtcTime.day_of_week + index) % 7;
      else day = (index + 1) % 7;

    // line start and display week day
    strlcpy (str_day, kWeekdayNames + day * 3, 4);

    if ((week == 0) && (index == 6)) strcpy (str_type, "today");
      else strcpy (str_type, "day");
    sprintf_P (str_text, PSTR ("<tr><td colspan=12 class='%s'>%s</td>"), str_type, str_day);
    str_result += str_text;

    // loop to display slots of current day
    curr_slot = false;
    day_span  = curr_span = 0;
    for (hour = 0; hour < 24; hour++)
      for (slot = 0; slot < 6; slot++)
      {
        // read slot status
        new_slot = sensor_graph.event[day][hour][slot].presence;

        // check if slot change is needed
        if (curr_span == 0) change_slot = false;
        else if (day_span % 12 == 0) change_slot = true;
        else if (curr_slot != new_slot) change_slot = true;
        else change_slot = false;

        // if no slot change, increase width, else change slot
        if (!change_slot) curr_span++;
        else
        {
          // set slot class (on/off and separation line)
          if (curr_slot) strcpy (str_type, "on"); else strcpy (str_type, "off");
          if (day_span % 12 == 0) strcat (str_type, " sep");

          // display slot
          if (curr_span == 1) sprintf_P (str_text, PSTR ("<td class='%s'></td>"), str_type);
            else if (curr_span > 1) sprintf_P (str_text, PSTR ("<td colspan=%u class='%s'></td>"), curr_span, str_type);
          str_result += str_text;
          
          // reset slot data
          curr_slot = new_slot;
          curr_span = 1;
        }

        // increase day slot counter
        day_span++;
      }

    // display last slot and line stop
    if (curr_slot) strcpy (str_type, "on"); else strcpy (str_type, "off");
    sprintf_P (str_text, PSTR ("<td colspan=%u class='%s'></td></tr>\n"), curr_span, str_type);
    str_result += str_text;

    // if needed, publish result
    if (str_result.length () > 256) { WSContentSend_P (str_result.c_str ()); str_result = ""; }
  }

  // last line
  str_result += "<tr>";
  for (slot = 0; slot < 12; slot++) str_result += "<td></td>";
  for (hour = 0; hour < 24; hour++) for (slot = 0; slot < 6; slot++) str_result += "<td></td>";
  str_result += "</tr>\n";

  // end of table and end of page
  str_result += "</table></div>\n";
  WSContentSend_P (str_result.c_str ());
}

// Presence history page
void SensorWebWeeklyPresence ()
{
  uint16_t week;
  char     str_value[4];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // if target temperature has been changed
  if (Webserver->hasArg ("week"))
  {
    WebGetArg ("week", str_value, sizeof(str_value));
    if (strlen(str_value) > 0) week = atoi (str_value);
  }

  // beginning page without authentification
  WSContentStart_P (PSTR ("Measure history"), false);

  // graph swipe section script
  SensorWebGraphSwipe ();
  WSContentSend_P (PSTR ("</script>\n"));

  // set page as scalable
  WSContentSend_P (PSTR ("<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=yes'/>\n"));

  // refresh every 1 mn
  WSContentSend_P (PSTR ("<meta http-equiv='refresh' content='%u'/>\n"), 60);

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));

  WSContentSend_P (PSTR ("div {margin:0.25rem auto;padding:0.1rem 0px;text-align:center;}\n"));

  WSContentSend_P (PSTR ("div.title {font-size:3.5vh;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.header {font-size:2.5vh;margin:2vh auto;}\n"));

  WSContentSend_P (PSTR ("a {color:white;}\n"));
  WSContentSend_P (PSTR ("a:link {text-decoration:none;}\n"));

  // graph section style
  SensorWebGraphPresenceStyle ();

  WSContentSend_P (PSTR ("</style>\n"));

  WSContentSend_P (PSTR ("</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  // room name and header
  WSContentSend_P (PSTR ("<div class='title'><a href='/'>%s</a></div>\n"), SettingsText(SET_DEVICENAME));
  WSContentSend_P (PSTR ("<div class='header'>Presence</div>\n"));

  // ---------------
  //     Graph
  // ---------------

  SensorWebGraphPresenceCurve (week, D_SENSOR_PAGE_PRESENCE);

  // end of page
  WSContentStop ();
}

#endif      // USE_WEBSERVER

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xsns99 (uint32_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
   case FUNC_INIT:
      SensorLoadConfig ();
      SensorInit ();
      SensorLoadWeek ();
      SensorLoadYear ();
      break;

    case FUNC_COMMAND:
      result  = DecodeCommand (kSensorCommands, SensorCommand);
      result |= DecodeCommand (kTempCommands,   TempCommand);
      result |= DecodeCommand (kHumiCommands,   HumiCommand);
      result |= DecodeCommand (kPresCommands,   PresCommand);
      result |= DecodeCommand (kActiCommands,   ActiCommand);
      result |= DecodeCommand (kInacCommands,   InacCommand);
      break;

    case FUNC_SAVE_AT_MIDNIGHT:
      // monday morning, save previous week in indexed file
      if (RtcTime.day_of_week == 2) SensorSaveWeek (false);

      // save yearly data
      SensorResetYearDay (RtcTime.day_of_year - 1);
      SensorSaveYear ();

      // reset current day and save current history
      SensorResetWeekDay (RtcTime.day_of_week - 1);
      SensorSaveWeek (true);
      SensorSaveYear ();
      break;

    case FUNC_SAVE_BEFORE_RESTART:
      SensorSaveConfig ();
      SensorSaveWeek (true);
      SensorSaveYear ();
      break;

    case FUNC_EVERY_SECOND:
      SensorEverySecond ();
      break;

    case FUNC_JSON_APPEND:
      SensorShowJSON (true);
      break;
  }
  
  return result;
}

bool Xdrv99 (uint32_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  {
    case FUNC_MQTT_SUBSCRIBE:
      SensorMqttSubscribe ();
      break;

    case FUNC_MQTT_DATA:
      result = SensorMqttData ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      SensorWebSensor ();
      break;

    case FUNC_WEB_ADD_BUTTON:
      WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s %s</button></form></p>"), D_SENSOR_PAGE_CONFIG, D_SENSOR_CONFIGURE, D_SENSOR);
      break;

    case FUNC_WEB_ADD_HANDLER:
      Webserver->on ("/" D_SENSOR_PAGE_CONFIG,   SensorWebConfigure);
      Webserver->on ("/" D_SENSOR_PAGE_PRESENCE, SensorWebWeeklyPresence);
      Webserver->on ("/" D_SENSOR_PAGE_MEASURE,  SensorWebWeeklyMeasure);
      Webserver->on ("/" D_SENSOR_PAGE_YEARLY,   SensorWebYearlyTemperature);
      break;

    case FUNC_WEB_ADD_MAIN_BUTTON:
      if ((sensor_status.temp.source != SENSOR_SOURCE_NONE) || (sensor_status.humi.source != SENSOR_SOURCE_NONE)) SensorWebMainButtonMeasure ();
      if (sensor_status.pres.source != SENSOR_SOURCE_NONE) SensorWebMainButtonPresence ();
      break;
#endif      // USE_WEBSERVER
  }
  return result;
}

#endif      // USE_GENERIC_SENSOR
