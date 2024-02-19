/*
  xsns_126_generic_sensor.ino - Generic temperature, humidity and presence sensors
    Reading is possible from local or remote (MQTT topic/key) sensors
    Module should be 98 as with platformio modules 100, 101, 102, ... are compiled before 99

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
    17/07/2023 - v4.3 - Add yearly graph
    26/08/2023 - v4.4 - Change update slot policy to avoid missed slot
    03/11/2023 - v4.5 - Add pres_device command
    20/11/2023 - v4.6 - Tasmota 13.2 compatibility
                        Switch parameters to rf_code[3]

  Configuration values are stored in :
    - Settings->rf_code[3][0] : number of weeks
    - Settings->rf_code[3][1] : type of presence sensor
    - Settings->rf_code[3][2] : temperature validity timeout (x10 in sec.)
    - Settings->rf_code[3][3] : humidity validity timeout (x10 in sec.)
    - Settings->rf_code[3][4] : presence validity timeout (x10 in sec.)

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
    * SET_SENSOR_PRES_YEARLY  				// presence yearly history

  Handled local sensor are :
    * Temperature = DHT11, AM2301, SI7021, SHT30 or DS18x20
    * Humidity    = DHT11, AM2301, SI7021 or SHT30
    * Presence    = Generic presence/movement detector declared as Counter 1
                    HLK-LD1115H, HLK-LD1125H, HLK-LD2410 or HLK-LD2450 connected thru serial port (Rx and Tx)
  
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

#define XSNS_98                       98
#define XDRV_98                       98

#include <ArduinoJson.h>

#define SENSOR_TEMP_TIMEOUT           600           // temperature default validity (in sec.)
#define SENSOR_HUMI_TIMEOUT           600           // humidity default validity (in sec.)
#define SENSOR_PRES_TIMEOUT           60            // presence default validity (in sec.)

#define SENSOR_TEMP_DRIFT_MAX         10            // maximum temperature correction
#define SENSOR_TEMP_DRIFT_STEP        0.1           // temperature correction steps
#define SENSOR_MEASURE_UPDATE         10            // update sensor measures (in sec.)

#define SENSOR_PRESENCE_INDEX         0             // default presence counter index (0=Counter 1 ... 7=Counter 8)

#define SENSOR_HISTO_DEFAULT          8             // default number of weeks historisation

#define D_SENSOR_PAGE_CONFIG          "sconf"
#define D_SENSOR_PAGE_WEEK_MEASURE    "wmeas"
#define D_SENSOR_PAGE_WEEK_PRESENCE   "wpres"
#define D_SENSOR_PAGE_YEAR_MEASURE    "ymeas"
#define D_SENSOR_PAGE_YEAR_PRESENCE   "ypres"

#define D_CMND_SENSOR_SENS            "sens"
#define D_CMND_SENSOR_TEMP            "temp"
#define D_CMND_SENSOR_HUMI            "humi"
#define D_CMND_SENSOR_PRES            "pres"
#define D_CMND_SENSOR_ACTI            "acti"
#define D_CMND_SENSOR_INAC            "inac"

#define D_CMND_SENSOR_HELP            "help"
#define D_CMND_SENSOR_DEVICE          "device"
#define D_CMND_SENSOR_TOPIC           "topic"
#define D_CMND_SENSOR_KEY             "key"
#define D_CMND_SENSOR_TIMEOUT         "time"
#define D_CMND_SENSOR_WEEKLY          "week"
#define D_CMND_SENSOR_YEARLY          "year"
#define D_CMND_SENSOR_MAX             "max"
#define D_CMND_SENSOR_DRIFT           "drift"
#define D_CMND_SENSOR_TYPE            "type"
#define D_CMND_SENSOR_WEEK            "week"
#define D_CMND_SENSOR_RESET           "reset"
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

// graph colors
#define SENSOR_COLOR_HUMI             "#6bc4ff"
#define SENSOR_COLOR_TEMP             "#f39c12"
#define SENSOR_COLOR_TEMP_MAX         "#f39c12"
#define SENSOR_COLOR_TEMP_MIN         "#5dade2"
#define SENSOR_COLOR_PRES             "#080"
#define SENSOR_COLOR_NONE             "#fff"
#define SENSOR_COLOR_ACTI             "#f00"
#define SENSOR_COLOR_INAC             "#fff"
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

#define SENSOR_GRAPH_WIDTH_HEAD       100
#define SENSOR_GRAPH_WIDTH_LINE       1100
#define SENSOR_GRAPH_HEIGHT_HEAD      20
#define SENSOR_GRAPH_HEIGHT_INTER     25
#define SENSOR_GRAPH_HEIGHT_LINE      20

#define SENSOR_SIZE_FS                2  

#ifdef USE_UFILESYS
const uint8_t SENSOR_SIZE_WEEK_TEMP = SENSOR_SIZE_FS;   
const uint8_t SENSOR_SIZE_WEEK_HUMI = SENSOR_SIZE_FS;   
const uint8_t SENSOR_SIZE_WEEK_PRES = SENSOR_SIZE_FS;   
const uint8_t SENSOR_SIZE_WEEK_ACTI = SENSOR_SIZE_FS;   
const uint8_t SENSOR_SIZE_WEEK_INAC = SENSOR_SIZE_FS;   
const uint8_t SENSOR_SIZE_YEAR_TEMP = SENSOR_SIZE_FS;      
const uint8_t SENSOR_SIZE_YEAR_PRES = SENSOR_SIZE_FS;      
#else
const uint8_t SENSOR_SIZE_WEEK_TEMP = 86;   
const uint8_t SENSOR_SIZE_WEEK_HUMI = 85;   
const uint8_t SENSOR_SIZE_WEEK_PRES = 48;   
const uint8_t SENSOR_SIZE_WEEK_ACTI = 48;   
const uint8_t SENSOR_SIZE_WEEK_INAC = 48;   
const uint8_t SENSOR_SIZE_YEAR_TEMP = 126;      
const uint8_t SENSOR_SIZE_YEAR_PRES = 54;      
#endif    // USE_UFILESYS

// number of days in months
uint8_t days_in_month[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// sensor family type
enum SensorFamilyType { SENSOR_TYPE_TEMP, SENSOR_TYPE_HUMI, SENSOR_TYPE_PRES, SENSOR_TYPE_MAX };

// presence serial sensor type
enum SensorPresenceModel { SENSOR_PRESENCE_NONE, SENSOR_PRESENCE_REMOTE, SENSOR_PRESENCE_SWITCH, SENSOR_PRESENCE_RCWL0516, SENSOR_PRESENCE_HWMS03, SENSOR_PRESENCE_LD1115, SENSOR_PRESENCE_LD1125, SENSOR_PRESENCE_LD2410, SENSOR_PRESENCE_LD2450, SENSOR_PRESENCE_MAX };
const char kSensorPresenceModel[] PROGMEM = "None|Remote|Dry switch|RCWL-0516|HW-MS03|HLK-LD1115|HLK-LD1125|HLK-LD2410|HLK-LD2450|";

// remote sensor sources
enum SensorSource { SENSOR_SOURCE_DSB, SENSOR_SOURCE_DHT, SENSOR_SOURCE_SHT, SENSOR_SOURCE_COUNTER, SENSOR_SOURCE_SERIAL, SENSOR_SOURCE_REMOTE, SENSOR_SOURCE_NONE };

// remote sensor commands
const char kSensorCommands[] PROGMEM = D_CMND_SENSOR_SENS "_" "|" D_CMND_SENSOR_HELP "|" D_CMND_SENSOR_RESET "|" D_CMND_SENSOR_RANDOM "|" D_CMND_SENSOR_WEEK;
void (* const SensorCommand[])(void) PROGMEM = { &CmndSensorHelp, &CmndSensorReset, &CmndSensorRandom, &CmndSensorWeek };

const char kTempCommands[] PROGMEM = D_CMND_SENSOR_TEMP "_" "|" D_CMND_SENSOR_TOPIC "|" D_CMND_SENSOR_KEY "|" D_CMND_SENSOR_TIMEOUT "|" D_CMND_SENSOR_WEEKLY "|" D_CMND_SENSOR_YEARLY "|" D_CMND_SENSOR_DRIFT;
void (* const TempCommand[])(void) PROGMEM = { &CmndSensorTemperatureTopic, &CmndSensorTemperatureKey, &CmndSensorTemperatureTimeout, &CmndSensorTemperatureWeekly, &CmndSensorTemperatureYearly, &CmndSensorTemperatureDrift };

const char kHumiCommands[] PROGMEM = D_CMND_SENSOR_HUMI "_" "|" D_CMND_SENSOR_TOPIC "|" D_CMND_SENSOR_KEY "|" D_CMND_SENSOR_TIMEOUT "|" D_CMND_SENSOR_WEEKLY;
void (* const HumiCommand[])(void) PROGMEM = { &CmndSensorHumidityTopic, &CmndSensorHumidityKey, &CmndSensorHumidityTimeout, &CmndSensorHumidityWeekly };

const char kPresCommands[] PROGMEM = D_CMND_SENSOR_PRES "_" "|" D_CMND_SENSOR_TOPIC "|" D_CMND_SENSOR_KEY "|" D_CMND_SENSOR_TIMEOUT "|" D_CMND_SENSOR_WEEKLY "|" D_CMND_SENSOR_YEARLY "|" D_CMND_SENSOR_DEVICE;
void (* const PresCommand[])(void) PROGMEM = { &CmndSensorPresenceTopic, &CmndSensorPresenceKey, &CmndSensorPresenceTimeout, &CmndSensorPresenceWeekly, &CmndSensorPresenceYearly, &CmndSensorPresenceDevice };

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
const char D_SENSOR_FILENAME_WEEK[] PROGMEM = "/sensor-week-%02u.csv";           // sensor weekly history files
const char D_SENSOR_FILENAME_YEAR[] PROGMEM = "/sensor-%04u.csv";                // sensor yearly history file
#endif    // USE_UFILESYS

/*************************************************\
 *               Variables
\*************************************************/

typedef struct
{
  uint8_t  source;                            // source of data (local sensor type)
  uint32_t timestamp;                         // last time sensor was updated
  int16_t  value;                             // current sensor value
  int16_t  last;                              // last published sensor value
} sensor_data;

typedef struct
{
  uint16_t validity;                          // remote sensor data validity (in sec.)
  String   topic;                             // remote sensor topic
  String   key;                               // remote sensor key
} sensor_remote;

typedef union {                               // Restricted by MISRA-C Rule 18.4 but so useful...
  uint8_t data;                               // Allow bit manipulation
  struct {
    uint8_t pres : 1;                         // presence detected
    uint8_t acti : 1;                         // activity declared
    uint8_t inac : 1;                         // inactivity declared
    uint8_t none : 5;
  };
} sensor_event;

typedef struct
{
  int16_t      temp;                          // temperature
  uint8_t      humi;                          // humidity
  sensor_event event;                         // event : presence, activity and inactivity
} sensor_weekly;

typedef struct {
  int16_t  temp_min;                          // minimum temperature
  int16_t  temp_max;                          // maximum temperature
  uint8_t  pres;                              // daily presence (one bit per 3h slot)
} sensor_yearly;

// configuration
struct {
  uint8_t weekly_acti = 0;                      // activity weekly historisation flag
  uint8_t weekly_inac = 0;                      // inactivity weekly historisation flag
  uint8_t weekly_pres = 0;                      // presence weekly historisation flag
  uint8_t weekly_temp = 0;                      // temperature weekly historisation flag
  uint8_t weekly_humi = 0;                      // humidity weekly historisation flag
  uint8_t yearly_temp = 0;                      // temperature yearly historisation flag
  uint8_t yearly_pres = 0;                      // presence yearly historisation flag
  uint8_t device_pres = SENSOR_PRESENCE_NONE;   // presence sensor type
  uint8_t week_histo  = SENSOR_HISTO_DEFAULT;   // number of weeks historisation
  sensor_remote temp;                           // remote temperature mqtt
  sensor_remote humi;                           // remote humidity mqtt
  sensor_remote pres;                           // remote presence mqtt
} sensor_config;

// current status
struct {
  uint8_t  counter     = 0;                   // measure update counter
  uint32_t time_ignore = UINT32_MAX;          // timestamp to ignore sensor update
  uint32_t time_json   = UINT32_MAX;          // timestamp of next JSON update

  uint8_t  hour;                              // hour of current weekly slot
  uint8_t  dayofweek;                         // day of current weekly slot
  uint8_t  month;                             // month of current yearly slot
  uint8_t  dayofmonth;                        // day of month of current yearly slot
  uint16_t dayofyear;                         // day of year of current yearly slot
  uint16_t year;                              // year of current yearly slot

  sensor_data temp;                           // temperature sensor data
  sensor_data humi;                           // humidity sensor data
  sensor_data pres;                           // presence sensor data
  sensor_weekly hour_slot[6];                 // sensor value for current hour (every 10mn)
  sensor_yearly day_slot;                     // sensor value for current day
} sensor_status;

// history
sensor_weekly sensor_week[7][24][6];          // sensor weekly values (4.03k)
sensor_yearly sensor_year[12][31];            // sensor yearly range (1.86k)

/***********************************************\
 *                  Commands
\***********************************************/

// timezone help
void CmndSensorHelp ()
{
  uint8_t index;
  char    str_text[16];

  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Sensor commands :"));

#ifdef USE_UFILESYS
  AddLog (LOG_LEVEL_INFO, PSTR (" - sens_week <value>  = number of weeks of historisation"));
#endif      // USE_UFILESYS
  AddLog (LOG_LEVEL_INFO, PSTR (" - sens_reset <w/y>   = reset data (w:current week, y:current year)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - sens_rand <w/y>    = random data (w:current week, y:current year)"));

  AddLog (LOG_LEVEL_INFO, PSTR (" Temperature :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - temp_topic <topic> = topic of remote sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - temp_key <key>     = key of remote sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - temp_drift <value> = correction (in 1/10 of °C)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - temp_time <value>  = remote sensor timeout"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - temp_week <0/1>    = weekly histo [%u], %u bytes"), sensor_config.weekly_temp, SENSOR_SIZE_WEEK_TEMP);
  AddLog (LOG_LEVEL_INFO, PSTR (" - temp_year <0/1>    = yearly histo [%u], %u bytes"), sensor_config.yearly_temp, SENSOR_SIZE_YEAR_TEMP);

  AddLog (LOG_LEVEL_INFO, PSTR (" Humidity :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - humi_topic <topic> = topic of remote sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - humi_key <key>     = key of remote sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - humi_time <value>  = remote sensor timeout"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - humi_week <0/1>    = weekly histo [%u], %u bytes"), sensor_config.weekly_humi, SENSOR_SIZE_WEEK_HUMI);

  AddLog (LOG_LEVEL_INFO, PSTR (" Activity :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - acti_week <0/1>    = weekly histo [%u], %u bytes"), sensor_config.weekly_acti, SENSOR_SIZE_WEEK_ACTI);

  AddLog (LOG_LEVEL_INFO, PSTR (" Inactivity :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - inac_week <0/1>    = weekly histo [%u], %u bytes"), sensor_config.weekly_inac, SENSOR_SIZE_WEEK_INAC);

  AddLog (LOG_LEVEL_INFO, PSTR (" Presence :"));

  AddLog (LOG_LEVEL_INFO, PSTR (" - pres_topic <topic> = topic of remote sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - pres_key <key>     = key of remote sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - pres_time <value>  = sensor timeout"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - pres_week <0/1>    = weekly histo [%u], %u bytes"), sensor_config.weekly_pres, SENSOR_SIZE_WEEK_PRES);
  AddLog (LOG_LEVEL_INFO, PSTR (" - pres_year <0/1>    = yearly histo [%u], %u bytes"), sensor_config.yearly_pres, SENSOR_SIZE_YEAR_PRES);

  AddLog (LOG_LEVEL_INFO, PSTR (" - pres_device <dev>  = presence sensor device"));

  for (index = 0; index < SENSOR_PRESENCE_MAX; index ++)
  {
    GetTextIndexed (str_text, sizeof (str_text), index, kSensorPresenceModel);
  AddLog (LOG_LEVEL_INFO, PSTR ("                   %u : %s"), index, str_text);
  }


  ResponseCmndDone();
}

void CmndSensorReset ()
{
  uint8_t month, day, hour, slot;

  switch (XdrvMailbox.data[0])
  {
    // week
    case 'w':
      for (day = 0; day < 7; day ++)
        for (hour = 0; hour < 24; hour ++)
          for (slot = 0; slot < 6; slot ++)
          {
            sensor_week[day][hour][slot].temp = INT16_MAX;
            sensor_week[day][hour][slot].humi = INT8_MAX;
            sensor_week[day][hour][slot].event.data = 0;
          }
      ResponseCmndDone ();
      break;

    // year
    case 'y':
      for (month = 0; month < 12; month++)
        for (day = 0; day < 31; day ++)
        {
          sensor_year[month][day].temp_max = INT16_MAX;
          sensor_year[month][day].temp_min = INT16_MAX;
          sensor_year[month][day].pres     = UINT8_MAX;
        }
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
  uint8_t month, day, hour, slot;
  int8_t  humidity;
  int16_t temperature;

  switch (XdrvMailbox.data[0])
  {
    // week
    case 'w':
      temperature = 180;
      humidity    = 70;
      for (day = 0; day < 7; day ++)
        for (hour = 0; hour < 24; hour ++)
          for (slot = 0; slot < 6; slot ++)
          {
            // temperature
            temperature += 3 - (int16_t)random (7);
            sensor_week[day][hour][slot].temp = temperature;

            // humidity
            humidity += 2 - (int8_t)random (5);
            if (humidity < 0) humidity = 0; 
            if (humidity > 100) humidity = 100; 
            sensor_week[day][hour][slot].humi = humidity;

            // presence
            sensor_week[day][hour][slot].event.data = (uint8_t)random (256);
          }
      ResponseCmndDone ();
      break;

    // year
    case 'y':
      temperature = 180;
      for (month = 0; month < 12; month++)
        for (day = 0; day < 31; day ++)
        {
          // temperature
          temperature += 3 - (int16_t)random (7);
          sensor_year[month][day].temp_max = temperature;
          sensor_year[month][day].temp_min = temperature - 30 + (int16_t)random (7);

          // presence
          sensor_year[month][day].pres = (uint8_t)random (256);
        }
      ResponseCmndDone ();
      break;

    // error
    default:
      ResponseCmndFailed ();
      break;
  }
}

void CmndSensorWeek ()
{
  if ((XdrvMailbox.payload > 0) && (XdrvMailbox.payload < 54))
  {
    sensor_config.week_histo = (uint8_t)XdrvMailbox.payload;
    SensorSaveConfig ();
  }
  ResponseCmndNumber (sensor_config.week_histo);
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
    sensor_config.weekly_temp = (uint8_t)XdrvMailbox.payload; 
    SensorTemperatureSaveWeekToSettings ();
  }
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Size is %u/%u"), GetSettingsTextLen (), settings_text_size);
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Histo takes %u bytes"), SENSOR_SIZE_WEEK_TEMP);
  ResponseCmndNumber (sensor_config.weekly_temp);
}

void CmndSensorTemperatureYearly ()
{
  if (XdrvMailbox.data_len > 0)
  {
    sensor_config.yearly_temp = (uint8_t)XdrvMailbox.payload; 
    SensorTemperatureSaveYearToSettings ();
  }
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Size is %u/%u"), GetSettingsTextLen (), settings_text_size);
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Histo takes %u bytes"), SENSOR_SIZE_YEAR_TEMP);
  ResponseCmndNumber (sensor_config.yearly_temp);
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
    sensor_config.weekly_humi = (uint8_t)XdrvMailbox.payload; 
    SensorHumiditySaveWeekToSettings ();
  }
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Size is %u/%u"), GetSettingsTextLen (), settings_text_size);
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Histo takes %u bytes"), SENSOR_SIZE_WEEK_HUMI);
  ResponseCmndNumber (sensor_config.weekly_humi);
}

void CmndSensorPresenceDevice ()
{
  if (XdrvMailbox.data_len > 0)
  {
    sensor_config.device_pres = (uint8_t)XdrvMailbox.payload;
    SensorSaveConfig ();
  }

  ResponseCmndNumber (sensor_config.device_pres);
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
    sensor_config.weekly_pres = (uint8_t)XdrvMailbox.payload; 
    SensorPresenceSaveWeekToSettings ();
  }
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Size is %u/%u"), GetSettingsTextLen (), settings_text_size);
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Histo takes %u bytes"), SENSOR_SIZE_WEEK_PRES);
  ResponseCmndNumber (sensor_config.weekly_pres);
}

void CmndSensorPresenceYearly ()
{
  if (XdrvMailbox.data_len > 0)
  {
    sensor_config.yearly_pres = (uint8_t)XdrvMailbox.payload; 
    SensorPresenceSaveYearToSettings ();
  }
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Size is %u/%u"), GetSettingsTextLen (), settings_text_size);
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Histo takes %u bytes"), SENSOR_SIZE_YEAR_PRES);
  ResponseCmndNumber (sensor_config.yearly_pres);
}

void CmndSensorActivityWeekly ()
{
  if (XdrvMailbox.data_len > 0)
  {
    sensor_config.weekly_acti = (uint8_t)XdrvMailbox.payload; 
    SensorActivitySaveWeekToSettings ();
  }
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Size is %u/%u"), GetSettingsTextLen (), settings_text_size);
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Histo takes %u bytes"), SENSOR_SIZE_WEEK_ACTI);
  ResponseCmndNumber (sensor_config.weekly_acti);
}

void CmndSensorInactivityWeekly ()
{
  if (XdrvMailbox.data_len > 0)
  {
    sensor_config.weekly_inac = (uint8_t)XdrvMailbox.payload; 
    SensorInactivitySaveWeekToSettings ();
  }
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Size is %u/%u"), GetSettingsTextLen (), settings_text_size);
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Histo takes %u bytes"), SENSOR_SIZE_WEEK_INAC);
  ResponseCmndNumber (sensor_config.weekly_inac);
}

/**************************************************\
 *                  Functions
\**************************************************/

// get current week label dd/mm - dd/mm (0 is current week)
void SensorGetWeekLabel (const uint8_t shift_week, char* pstr_label, size_t size_label)
{
  uint16_t day_shift;
  uint32_t day_time;
  TIME_T   start_dst, stop_dst;

  // check parameters
  if (pstr_label == nullptr) return;
  if (size_label < 16) return;

#ifdef USE_UFILESYS
  // calculate start day
  day_shift = (7 + RtcTime.day_of_week - 2) % 7;
  day_time = LocalTime () - day_shift * 86400 - shift_week * 604800;
  BreakTime (day_time, start_dst);

  // calculate stop day
  day_time += 6 * 86400;
  BreakTime(day_time, stop_dst);

  // generate string
  sprintf (pstr_label, "%02u/%02u - %02u/%02u", start_dst.day_of_month, start_dst.month, stop_dst.day_of_month, stop_dst.month);

#else
  // 7 days shift
  strcpy (pstr_label, "Last 7 days");
#endif      // USE_UFILESYS
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
  else if (time_delay >= 86400) sprintf (pstr_result, "%u d %u h", time_delay / 86400, (time_delay % 86400) / 3600);
  else if (time_delay >= 36000) sprintf (pstr_result, "%u h", time_delay / 3600);
  else if (time_delay >= 3600) sprintf (pstr_result, "%u h %u mn", time_delay / 3600, (time_delay % 3600) / 60);
  else if (time_delay >= 600) sprintf (pstr_result, "%u mn", time_delay / 60);
  else if (time_delay >= 60) sprintf (pstr_result, "%u mn %u s", time_delay / 60, time_delay % 60);
  else if (time_delay > 0) sprintf (pstr_result, "%u s", time_delay);
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
    for (slot = 0; slot < 6; slot ++) sensor_week[day][hour][slot].temp = INT16_MAX;

  // log
  strlcpy (str_day, kWeekdayNames + day * 3, 4);
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Temperature - Reset %s"), str_day);
}

// load weekly temperature history from settings string
//   byte 0       = if first temperature is negative, abs (temperature) else 0xFF
//   byte 1       = if first temperature is positive, temperature else 0xFF
//   byte 2..85   = delta per hour on 4 bits (N/A, -5, -3, -2, -1.5, -1, -0.5, 0, +0.5, +1, +1.5, +2, N/A, +3, +5, NAN)
bool SensorTemperatureLoadWeekFromSettings ()
{
  uint8_t index, day, hour, slot, value;
  int16_t temp_hour, temp_ref;
  char    str_history[SENSOR_SIZE_WEEK_TEMP+1];

  // reset temperature data to N/A
  for (day = 0; day < 7; day ++)
    for (hour = 0; hour < 24; hour ++)
      for (slot = 0; slot < 6; slot ++) sensor_week[day][hour][slot].temp = INT16_MAX;

  // check if history is available
  strlcpy (str_history, SettingsText (SET_SENSOR_TEMP_WEEKLY), sizeof (str_history));
  value = strlen (str_history);
  if ( value > 0) sensor_config.weekly_temp = 1;
    else sensor_config.weekly_temp = 0;
  if ((value == SENSOR_SIZE_FS) || (value != SENSOR_SIZE_WEEK_TEMP)) return false;

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
      for (slot = 0; slot < 6; slot ++) sensor_week[day][hour][slot].temp = temp_hour;

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
void SensorTemperatureSaveWeekToSettings ()
{
  uint8_t index, part, day, hour, slot, value;
  char    str_history[SENSOR_SIZE_WEEK_TEMP+1];
  int16_t delta, ref_temp, hour_temp;

  // if no temperature weekly history
  SettingsUpdateText (SET_SENSOR_TEMP_WEEKLY, "");

  // if weekly history should be saved
  if (sensor_config.weekly_temp == 1)

#ifdef USE_UFILESYS
    SettingsUpdateText (SET_SENSOR_TEMP_WEEKLY, "fs");

#else
  {
    // if settings size availability is not enough, disable data saving
    if (settings_text_size - GetSettingsTextLen () < SENSOR_SIZE_WEEK_TEMP)
    {
      sensor_config.weekly_temp = 0;
      AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Week Temp. - No space to save"));
    }

    // else, save data to settings
    else
    {
      // loop thru temperature slots to get first reference temperature
      ref_temp = INT16_MAX;  
      for (day = 0; day < 7; day ++)
        for (hour = 0; hour < 24; hour ++)
          for (slot = 0; slot < 6; slot ++) if (ref_temp == INT16_MAX) ref_temp = sensor_week[day][hour][slot].temp;

      // init history string
      for (index = 0; index < SENSOR_SIZE_WEEK_TEMP; index ++) str_history[index] = 0xFF;

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
            hour_temp = sensor_week[day][hour][0].temp;
            for (slot = 1; slot < 6; slot ++)
              if (hour_temp == INT16_MAX) hour_temp = sensor_week[day][hour][slot].temp;
                else if ((sensor_week[day][hour][slot].temp != INT16_MAX) && (hour_temp < sensor_week[day][hour][slot].temp)) hour_temp = sensor_week[day][hour][slot].temp;

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
      str_history[SENSOR_SIZE_WEEK_TEMP] = 0;
      SettingsUpdateText (SET_SENSOR_TEMP_WEEKLY, str_history);

      // log
      AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Week temp. - Saved to settings"));
    }
  }
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
void SensorTemperatureLoadYearFromSettings ()
{
  uint8_t  value;
  uint8_t  month, day;
  uint16_t index, curr_shift, last_shift;
  int16_t  temp_min, temp_max, temp_ref_min, temp_ref_max;
  char     str_history[SENSOR_SIZE_YEAR_TEMP+1];

  // reset temperature data to N/A
  for (month = 0; month < 12; month ++)
    for (day = 0; day < 31; day ++)
    {
      sensor_year[month][day].temp_min = INT16_MAX;
      sensor_year[month][day].temp_max = INT16_MAX;
    }

  // check if history is available
  strlcpy (str_history, SettingsText (SET_SENSOR_TEMP_YEARLY), sizeof (str_history));
  value = strlen (str_history);
  if (value > 0) sensor_config.yearly_temp = 1; else sensor_config.yearly_temp = 0;
  if ((value == SENSOR_SIZE_FS) || (value != SENSOR_SIZE_YEAR_TEMP)) return;

  // calculate reference minimum temperature
  temp_min = INT16_MAX;
  if (str_history[0] != 0xFF) temp_min = 0 - (int16_t)str_history[0] * 5;
    else if (str_history[1] != 0xFF) temp_min = (int16_t)(str_history[1] - 1) * 5;
  if (temp_min == INT16_MAX) return;

  // calculate reference maximum temperature
  temp_max = INT16_MAX;
  if (str_history[2] != 0xFF) temp_max = 0 - (int16_t)str_history[2] * 5;
    else if (str_history[3] != 0xFF) temp_max = (int16_t)(str_history[3] - 1) * 5;
  if (temp_max == INT16_MAX) return;

  // update sensor values from string (one value every 3 days)
  index = 0;
  last_shift = UINT16_MAX;
  curr_shift = 0;
  temp_ref_min = temp_ref_max = INT16_MAX;
  for (month = 0; month < 12; month ++)
    for (day = 0; day < days_in_month[month]; day ++)
    {
      // if changing block of 3 days
      curr_shift = index++ / 3;
      if (curr_shift != last_shift)
      {
        // calculate minimum temperature evolution
        value = str_history[curr_shift + 4] & 0x0F;
        temp_ref_min = SensorTemperatureCalculateFromDelta (temp_min, value);
        if (temp_ref_min != INT16_MAX) temp_min = temp_ref_min;

        // calculate maximum temperature evolution
        value = str_history[curr_shift + 4] >> 4;
        temp_ref_max = SensorTemperatureCalculateFromDelta (temp_max, value);
        if (temp_ref_max != INT16_MAX) temp_max = temp_ref_max;

        // set last shift
        last_shift = curr_shift;
      }

      // set min and max temperature
      sensor_year[month][day].temp_min = temp_ref_min;
      sensor_year[month][day].temp_max = temp_ref_max;
    }
}

// save yearly temperature history to settings string
//   byte 0       = if first min temperature is negative, abs (min temperature) else 0xFF
//   byte 1       = if first min temperature is positive, min temperature else 0xFF
//   byte 2       = if first max temperature is negative, abs (max temperature) else 0xFF
//   byte 3       = if first max temperature is positive, max temperature else 0xFF
//   byte 4..125  = delta per every 3 days on 4 bits
//                  first half byte is min temperature evolution
//                  second half byte is max temperature evolution
void SensorTemperatureSaveYearToSettings ()
{
  uint8_t  month, day, value;
  uint16_t index, last_shift, curr_shift;
  int16_t  temp_min, temp_max, temp_ref_min, temp_ref_max, delta;
  char     str_history[SENSOR_SIZE_YEAR_TEMP+1];

  // if no temperature weekly history
  SettingsUpdateText (SET_SENSOR_TEMP_YEARLY, "");

  // if data should be saved
  if (sensor_config.yearly_temp == 1)

#ifdef USE_UFILESYS
    SettingsUpdateText (SET_SENSOR_TEMP_YEARLY, "fs");

#else
  {
    // if settings size availability is not enough, disable data saving
    if (settings_text_size - GetSettingsTextLen () < SENSOR_SIZE_YEAR_TEMP)
    {
      sensor_config.yearly_temp = 0;
      AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Year temp. - No space to save"));
    }

    // else save data to settings
    else
    {
      // loop thru temperature slots to get first minimum and maximum temperature
      temp_min = temp_max = INT16_MAX;  
      for (month = 0; month < 12; month ++)
        for (day = 0; day < days_in_month[month]; day ++)
        {
          if (temp_min == INT16_MAX) temp_min = sensor_year[month][day].temp_min;
          if (temp_max == INT16_MAX) temp_max = sensor_year[month][day].temp_max;
        }

      // init history string
      for (index = 0; index < SENSOR_SIZE_YEAR_TEMP; index ++) str_history[index] = 0xFF;

      // write first minimum and maximum temperature
      if (temp_min < 0) str_history[0] = (uint8_t)((0 - temp_min) / 5);
        else if (temp_min != INT16_MAX) str_history[1] = (uint8_t)(temp_min / 5) + 1;
      if (temp_max < 0) str_history[2] = (uint8_t)((0 - temp_max) / 5);
        else if (temp_max != INT16_MAX) str_history[3] = (uint8_t)(temp_max / 5) + 1;

      // update sensor history string
      index = 0;
      last_shift = curr_shift = 0;
      temp_ref_min = temp_ref_max = INT16_MAX;
      for (month = 0; month < 12; month ++)
        for (day = 0; day < days_in_month[month]; day ++)
        {
          // calculate minimum temperature evolution
          if (temp_ref_min == INT16_MAX) temp_ref_min = sensor_year[month][day].temp_min;
            else if ((sensor_year[month][day].temp_min != INT16_MAX) && (sensor_year[month][day].temp_min < temp_ref_min)) temp_ref_min = sensor_year[month][day].temp_min;

          // calculate maximum temperature evolution
          if (temp_ref_max == INT16_MAX) temp_ref_max = sensor_year[month][day].temp_max;
            else if ((sensor_year[month][day].temp_max != INT16_MAX) && (sensor_year[month][day].temp_max > temp_ref_max)) temp_ref_max = sensor_year[month][day].temp_max;

          // if changing block of 3 days
          curr_shift = index++ / 3;
          if (curr_shift != last_shift)
          {
            // define evolution of minimum temperature
            if (temp_ref_min == INT16_MAX) delta = INT16_MAX;
              else delta = temp_ref_min - temp_min;

            // calculate delta according to temperature difference
            value = SensorTemperatureCalculateDelta (temp_min, delta);
            str_history[last_shift + 4] = value;

            // define evolution of maximum temperature
            if (temp_ref_max == INT16_MAX) delta = INT16_MAX;
              else delta = temp_ref_max - temp_max;
            
            // calculate delta according to temperature difference
            value = SensorTemperatureCalculateDelta (temp_max, delta);
            str_history[last_shift + 4] = str_history[last_shift + 4] | (value << 4);

            // update references
            if (temp_ref_min != INT16_MAX) temp_min = temp_ref_min;
            if (temp_ref_max != INT16_MAX) temp_max = temp_ref_max;
            temp_ref_min = temp_ref_max = INT16_MAX;
            last_shift = curr_shift;
          }
        }

      // save history string
      str_history[SENSOR_SIZE_YEAR_TEMP] = 0;
      SettingsUpdateText (SET_SENSOR_TEMP_YEARLY, str_history);

      // log
      AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Year temp. - Saved to settings")); 
    }
  }
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
}

// load weekly humidity history from settings string
//   byte 0     = first humidity value else 0xFF
//   byte 1..84 = delta per hour on 4 bits (N/A, -30, -20, -10, -5, -3, -1, 0, +1, +3, +5, +10, N/A, +20, +30, N/A)
void SensorHumidityLoadWeekFromSettings ()
{
  int8_t  humi_ref, humi_hour;
  uint8_t day, hour, slot, index, value;
  char    str_history[SENSOR_SIZE_WEEK_HUMI+1];

  // init slots to N/A
  for (day = 0; day < 7; day ++)
    for (hour = 0; hour < 24; hour ++)
      for (slot = 0; slot < 6; slot ++) sensor_week[day][hour][slot].humi = INT8_MAX;

  // check if history is available
  strlcpy (str_history, SettingsText (SET_SENSOR_HUMI_WEEKLY), sizeof (str_history));
  value = strlen (str_history);
  if ( value > 0) sensor_config.weekly_humi = 1;
    else sensor_config.weekly_humi = 0;
  if ((value == SENSOR_SIZE_FS) || (value != SENSOR_SIZE_WEEK_HUMI)) return;

  // calculate reference humidity
  humi_ref = INT8_MAX;
  if (str_history[0] != 0xFF) humi_ref = (int8_t)str_history[0] - 1;
  if (humi_ref == INT8_MAX) return;

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
      for (slot = 0; slot < 6; slot ++) sensor_week[day][hour][slot].humi = humi_hour;
    }

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Humidity - Loaded from settings"));
}

// save weekly humidity history as settings string
//   byte 0     = first humidity value else 0xFF
//   byte 1..84 = delta per hour on 4 bits (N/A, -30, -20, -10, -5, -3, -1, 0, +1, +3, +5, +10, N/A, +20, +30, N/A)
void SensorHumiditySaveWeekToSettings ()
{
  uint8_t index, part, day, hour, slot, value;
  int8_t  humi_ref, humi_hour, humi_delta;
  char    str_history[SENSOR_SIZE_WEEK_HUMI+1];

  // if no humidity sensor declared, nothing to do
  SettingsUpdateText (SET_SENSOR_HUMI_WEEKLY, "");

  // if weekly history should be saved
  if (sensor_config.weekly_humi == 1)

#ifdef USE_UFILESYS
    SettingsUpdateText (SET_SENSOR_HUMI_WEEKLY, "fs");

#else
  {
    // if settings size availability is not enough, disable data saving
    if (settings_text_size - GetSettingsTextLen () < SENSOR_SIZE_WEEK_HUMI)
    {
      sensor_config.weekly_humi = 0;
      AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Week humi. - No space to save"));
    }

    else
    {
      // loop thru humiditu slots to get first reference humidity
      humi_ref = INT8_MAX;  
      for (day = 0; day < 7; day ++)
        for (hour = 0; hour < 24; hour ++)
          for (slot = 0; slot < 6; slot ++) if (humi_ref == INT8_MAX) humi_ref = sensor_week[day][hour][slot].humi;

      // init history string
      for (index = 0; index < SENSOR_SIZE_WEEK_HUMI; index ++) str_history[index] = 0xFF;

      // write reference humidity
      if (humi_ref != INT8_MAX) str_history[0] = (uint8_t)humi_ref + 1;

      // update sensor history string
      index = 1;
      if (humi_ref != INT8_MAX) 
        for (day = 0; day < 7; day ++)
          for (hour = 0; hour < 24; hour ++)
          {
              // get max humidity in the hour slots
              humi_hour = sensor_week[day][hour][0].humi;
              for (slot = 1; slot < 6; slot ++)
              {
                if (humi_hour == INT8_MAX) humi_hour = sensor_week[day][hour][slot].humi;
                  else if ((sensor_week[day][hour][slot].humi != INT8_MAX) && (humi_hour < sensor_week[day][hour][slot].humi)) humi_hour = sensor_week[day][hour][slot].humi;
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
      str_history[SENSOR_SIZE_WEEK_HUMI] = 0;
      SettingsUpdateText (SET_SENSOR_HUMI_WEEKLY, str_history);

      // log
      AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Week humi. - Saved to settings"));
    }
  }
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
bool SensorPresenceSet ()
{
  uint32_t time_now = LocalTime ();

  // check validity
  if (!RtcTime.valid) return false;

  // check ignore time frame
  if ((sensor_status.time_ignore != UINT32_MAX) && (sensor_status.time_ignore < time_now)) sensor_status.time_ignore = UINT32_MAX;
  if (sensor_status.time_ignore != UINT32_MAX) return false;

  // if new detection, log
  if (sensor_status.pres.value != 1) AddLog (LOG_LEVEL_DEBUG, PSTR ("SEN: Presence detected"));

  // update value and timestamp
  sensor_status.pres.value     = 1;
  sensor_status.pres.timestamp = time_now;

  return true;
}

// reset current presence
bool SensorPresenceReset ()
{
  // update presence status
  sensor_status.pres.value = 0;

  // log
  AddLog (LOG_LEVEL_DEBUG, PSTR ("SEN: Presence released"));

  return true;
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
void SensorPresenceLoadWeekFromSettings ()
{
  uint8_t day, hour, slot, index, value, presence;
  size_t  length;
  char    str_history[SENSOR_SIZE_WEEK_PRES+1];

  // init slots to no presence
  for (day = 0; day < 7; day ++)
    for (hour = 0; hour < 24; hour ++)
      for (slot = 0; slot < 6; slot ++) sensor_week[day][hour][slot].event.pres = 0;

  // check if history is available
  strlcpy (str_history, SettingsText (SET_SENSOR_PRES_WEEKLY), sizeof (str_history));
  value = strlen (str_history);
  if ( value > 0) sensor_config.weekly_pres = 1;
    else sensor_config.weekly_pres = 0;
  if ((value == SENSOR_SIZE_FS) || (value != SENSOR_SIZE_WEEK_PRES)) return;

  // loop to load slots
  for (day = 0; day < 7; day ++)
  {
    value = 0x01 << day;
    for (hour = 0; hour < 24; hour ++)
      for (slot = 0; slot < 6; slot ++)
      {
        index = hour * 2 + slot / 3;
        presence = str_history[index] & value;
        if (presence != 0) sensor_week[day][hour][slot].event.pres = 1;
      }
  }

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Week pres. - Loaded from settings"));
}

// save weekly presence history in settings string
void SensorPresenceSaveWeekToSettings ()
{
  uint8_t day, hour, slot, index, value;
  char    str_history[SENSOR_SIZE_WEEK_PRES+1];

  // if presence detection not enabled, nothing to save
  SettingsUpdateText (SET_SENSOR_PRES_WEEKLY, "");

  // if weekly history should be saved
  if (sensor_config.weekly_pres == 1)

#ifdef USE_UFILESYS
    SettingsUpdateText (SET_SENSOR_PRES_WEEKLY, "fs");

#else
  {
    // if settings size availability is not enough, disable data saving
    if (settings_text_size - GetSettingsTextLen () < SENSOR_SIZE_WEEK_PRES)
    {
      sensor_config.weekly_pres = 0;
      AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Week pres. - No space to save"));
    }

    // else save data
    else
    {
      // init sensor history string
      for (slot = 0; slot < SENSOR_SIZE_WEEK_PRES; slot ++) str_history[slot] = 128;

      // update sensor history string
      for (day = 0; day < 7; day ++)
      {
        value = 0x01 << day;
        for (hour = 0; hour < 24; hour ++)
          for (slot = 0; slot < 6; slot ++)
            if (sensor_week[day][hour][slot].event.pres == 1)
            {
              index = hour * 2 + slot / 3;
              str_history[index] = str_history[index] | value;
            }
      }

      // save history string
      str_history[SENSOR_SIZE_WEEK_PRES] = 0;
      SettingsUpdateText (SET_SENSOR_PRES_WEEKLY, str_history);

      // log
      AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Week pres. - Saved to settings"));
    }
  } 
#endif    // USE_UFILESYS
}

// load yearly presence history from settings string
void SensorPresenceLoadYearFromSettings ()
{
  uint8_t  value, presence;
  uint8_t  month, day;
  uint16_t index, shift, slot;
  char     str_history[SENSOR_SIZE_YEAR_PRES+1];

  // init slots to no presence
  for (month = 0; month < 12; month ++)
    for (day = 0; day < 31; day ++) sensor_year[month][day].pres = 0;

  // check if history is available
  strlcpy (str_history, SettingsText (SET_SENSOR_PRES_YEARLY), sizeof (str_history));
  value = strlen (str_history);
  if ( value > 0) sensor_config.yearly_pres = 1; else sensor_config.yearly_pres = 0;
  if ((value == SENSOR_SIZE_FS) || (value != SENSOR_SIZE_YEAR_PRES)) return;

  // loop to load slots
  index = 0;
  for (month = 0; month < 12; month ++)
    for (day = 0; day < days_in_month[month]; day ++)
    {
      // get presence
      shift = index / 7;
      slot  = index % 7;
      value = 0x01 << slot;
      presence = str_history[shift] & value;
      if (presence != 0) sensor_year[month][day].pres = 255;

      // increment
      index ++;
    }

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Year pres. - Loaded from settings"));
}

// save yearly presence history in settings string
void SensorPresenceSaveYearToSettings ()
{
  uint8_t  value;
  uint8_t  month, day;
  uint16_t index, shift, slot;
  char     str_history[SENSOR_SIZE_YEAR_PRES+1];

  // if presence detection not enabled, nothing to save
  SettingsUpdateText (SET_SENSOR_PRES_YEARLY, "");

  // if history should be saved
  if (sensor_config.yearly_pres == 1)

#ifdef USE_UFILESYS
    SettingsUpdateText (SET_SENSOR_PRES_YEARLY, "fs");

#else
  {
    // if settings size availability is not enough, disable data saving
    if (settings_text_size - GetSettingsTextLen () < SENSOR_SIZE_YEAR_PRES)
    {
      sensor_config.yearly_pres = 0;
      AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Year pres. - No space to save"));
    }

    // else save data
    else
    {
      // init sensor history string
      for (slot = 0; slot < SENSOR_SIZE_YEAR_PRES; slot ++) str_history[slot] = 128;

      // update sensor history string
      index = 0;
      for (month = 0; month < 12; month ++)
        for (day = 0; day < days_in_month[month]; day ++)
        {
          // save presence
          if (sensor_year[month][day].pres > 0) 
          {
            shift = index / 7;
            slot  = index % 7;
            value = 0x01 << slot;
            str_history[shift] = str_history[shift] | value;
          }

          // increment
          index ++;
        }

      // save history string
      str_history[53] = 0;
      SettingsUpdateText (SET_SENSOR_PRES_YEARLY, str_history);

      // log
      AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Year pres. - Saved to settings"));
    }
  } 
#endif    // USE_UFILESYS
}

// ---------------
//    Activity
// ---------------

// set current activity
void SensorActivitySet ()
{
  uint8_t slot;

  // check validity
  if (!RtcTime.valid) return;

  // update current slot
  slot = RtcTime.minute / 10; 
  if (slot < 6)
  {
    sensor_status.hour_slot[slot].event.acti = 1;
    AddLog (LOG_LEVEL_DEBUG, PSTR ("SEN: Activity - Declared slot %u"), slot);
  }
}

// load weekly activity history from settings string
void SensorActivityLoadWeekFromSettings ()
{
  bool    history;
  uint8_t day, hour, slot, index, value, action;
  size_t  length;
  char    str_history[SENSOR_SIZE_WEEK_ACTI+1];

  // init slots to no presence
  for (day = 0; day < 7; day ++)
    for (hour = 0; hour < 24; hour ++)
      for (slot = 0; slot < 6; slot ++) sensor_week[day][hour][slot].event.acti = 0;

  // check if history is available
  strlcpy (str_history, SettingsText (SET_SENSOR_ACTI_WEEKLY), sizeof (str_history));
  value = strlen (str_history);
  if ( value > 0) sensor_config.weekly_acti = 1;
    else sensor_config.weekly_acti = 0;
  if ((value == SENSOR_SIZE_FS) || (value != SENSOR_SIZE_WEEK_ACTI)) return;

  // loop to load slots
  for (day = 0; day < 7; day ++)
  {
    value = 0x01 << day;
    for (hour = 0; hour < 24; hour ++)
      for (slot = 0; slot < 6; slot ++)
      {
        index = hour * 2 + slot / 3;
        action = str_history[index] & value;
        if (action != 0) sensor_week[day][hour][slot].event.acti = 1;
      }
  }

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Activity - Loaded from settings"));
}

// save weekly activity history in configuration strings
void SensorActivitySaveWeekToSettings ()
{
  uint8_t day, hour, slot, index, value;
  char    str_history[SENSOR_SIZE_WEEK_ACTI+1];

  // if presence detection not enabled, nothing to save
  SettingsUpdateText (SET_SENSOR_ACTI_WEEKLY, "");

  // if  history should be saved
  if (sensor_config.weekly_acti == 1)

#ifdef USE_UFILESYS
    SettingsUpdateText (SET_SENSOR_ACTI_WEEKLY, "fs");

#else
  {
    // if settings size availability is not enough, disable data saving
    if (settings_text_size - GetSettingsTextLen () < SENSOR_SIZE_WEEK_ACTI)
    {
      AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Week acti. - No space to save"));
      sensor_config.weekly_acti = 0;
      return;
    }

    // else save data
    else
    {
      // init sensor history string
      for (slot = 0; slot < SENSOR_SIZE_WEEK_ACTI; slot ++) str_history[slot] = 128;

      // update sensor history string
      for (day = 0; day < 7; day ++)
      {
        value = 0x01 << day;
        for (hour = 0; hour < 24; hour ++)
          for (slot = 0; slot < 6; slot ++)
            if (sensor_week[day][hour][slot].event.acti == 1)
            {
              index = hour * 2 + slot / 3;
              str_history[index] = str_history[index] | value;
            }
      }

      // save history string
      str_history[SENSOR_SIZE_WEEK_ACTI] = 0;
      SettingsUpdateText (SET_SENSOR_ACTI_WEEKLY, str_history);

      // log
      AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Week acti. - Saved to settings"));
    }
  }
#endif    // USE_UFILESYS
}

// ----------------
//    Inactivity
// ----------------

// set current inactivity
void SensorInactivitySet ()
{
  uint8_t slot;

  // check validity
  if (!RtcTime.valid) return;
  
  // update history slot
  slot = RtcTime.minute / 10; 
  if (slot < 6)
  {
    sensor_status.hour_slot[slot].event.inac = 1;
    AddLog (LOG_LEVEL_DEBUG, PSTR ("SEN: Inactivity declared slot %u"), slot);
  }
}

// load weekly inactivity history from settings string
void SensorInactivityLoadWeekFromSettings ()
{
  bool    history;
  uint8_t day, hour, slot, index, value, detected;
  size_t  length;
  char    str_history[SENSOR_SIZE_WEEK_INAC+1];

  // init slots to no presence
  for (day = 0; day < 7; day ++)
    for (hour = 0; hour < 24; hour ++)
      for (slot = 0; slot < 6; slot ++) sensor_week[day][hour][slot].event.inac = 0;

  // check if history is available
  strlcpy (str_history, SettingsText (SET_SENSOR_INAC_WEEKLY), sizeof (str_history));
  value = strlen (str_history);
  if ( value > 0) sensor_config.weekly_inac = 1;
    else sensor_config.weekly_inac = 0;
  if ((value == SENSOR_SIZE_FS) || (value != SENSOR_SIZE_WEEK_INAC)) return;

  // loop to load slots
  for (day = 0; day < 7; day ++)
  {
    value = 0x01 << day;
    for (hour = 0; hour < 24; hour ++)
      for (slot = 0; slot < 6; slot ++)
      {
        index = hour * 2 + slot / 3;
        detected = str_history[index] & value;
        if (detected != 0) sensor_week[day][hour][slot].event.inac = 1;
      }
  }

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Inactivity - Loaded from settings"));
}

// save weekly inactivity history in settings string
void SensorInactivitySaveWeekToSettings ()
{
  uint8_t day, hour, slot, index, value;
  char    str_history[SENSOR_SIZE_WEEK_INAC+1];

  // if presence detection not enabled, nothing to save
  SettingsUpdateText (SET_SENSOR_INAC_WEEKLY, "");

  // if history should be saved
  if (sensor_config.weekly_inac == 1)

#ifdef USE_UFILESYS
    SettingsUpdateText (SET_SENSOR_INAC_WEEKLY, "fs");

#else
  {
    // if settings size is not enough, nothing to save
    if (settings_text_size - GetSettingsTextLen () < SENSOR_SIZE_WEEK_INAC)
    {
      sensor_config.weekly_inac = 0;
      AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Week inac. - No space to save"));
    }

    // else save data
    {
      // init sensor history string
      for (slot = 0; slot < SENSOR_SIZE_WEEK_INAC; slot ++) str_history[slot] = 128;

      // update sensor history string
      for (day = 0; day < 7; day ++)
      {
        value = 0x01 << day;
        for (hour = 0; hour < 24; hour ++)
          for (slot = 0; slot < 6; slot ++)
            if (sensor_week[day][hour][slot].event.inac == 1)
            {
              index = hour * 2 + slot / 3;
              str_history[index] = str_history[index] | value;
            }
      }

      // save history string
      str_history[SENSOR_SIZE_WEEK_INAC] = 0;
      SettingsUpdateText (SET_SENSOR_INAC_WEEKLY, str_history);

      // log
      AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Week inac. - Saved to settings"));
    }
  }
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

  // load presence detection flag
  sensor_config.week_histo  = Settings->rf_code[3][0];
  sensor_config.device_pres = Settings->rf_code[3][1];

  // load sensor data validity timeout
  sensor_config.temp.validity = (uint16_t)Settings->rf_code[3][2] * 10;
  sensor_config.humi.validity = (uint16_t)Settings->rf_code[3][3] * 10;
  sensor_config.pres.validity = (uint16_t)Settings->rf_code[3][4] * 10;

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
  if (sensor_config.week_histo == 0) sensor_config.week_histo = SENSOR_HISTO_DEFAULT;
  if (sensor_config.device_pres >= SENSOR_PRESENCE_MAX) sensor_config.device_pres = SENSOR_PRESENCE_NONE;
}

// save configuration parameters
void SensorSaveConfig () 
{
  // save presence detection flag
  Settings->rf_code[3][0] = sensor_config.week_histo;
  Settings->rf_code[3][1] = sensor_config.device_pres;

  // save sensor data validity timeout
  Settings->rf_code[3][2] = (uint8_t)(sensor_config.temp.validity / 10);
  Settings->rf_code[3][3] = (uint8_t)(sensor_config.humi.validity / 10);
  Settings->rf_code[3][4] = (uint8_t)(sensor_config.pres.validity / 10);

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

// load current data history
void SensorLoadData () 
{
  // load from settings
  SensorTemperatureLoadWeekFromSettings ();
  SensorHumidityLoadWeekFromSettings ();
  SensorPresenceLoadWeekFromSettings ();
  SensorActivityLoadWeekFromSettings ();
  SensorInactivityLoadWeekFromSettings ();
  SensorTemperatureLoadYearFromSettings ();
  SensorPresenceLoadYearFromSettings ();
}

// save data
void SensorSaveData ()
{
  // save data to settings
  SensorTemperatureSaveWeekToSettings ();
  SensorHumiditySaveWeekToSettings ();
  SensorPresenceSaveWeekToSettings ();
  SensorActivitySaveWeekToSettings ();
  SensorInactivitySaveWeekToSettings ();
  SensorTemperatureSaveYearToSettings ();
  SensorPresenceSaveYearToSettings ();

  // if available, append append data to files
#ifdef USE_UFILESYS
  SensorFileWeekAppend ();
  SensorFileYearAppend ();
#endif    //  USE_UFILESYS
}

/***************************\
 *      History files
\***************************/

#ifdef USE_UFILESYS

// test presence of weekly file
bool SensorFileWeekExist (const uint8_t week)
{
  char str_file[32];

  // check file existence
  sprintf_P (str_file, D_SENSOR_FILENAME_WEEK, week);
  return TfsFileExists (str_file);
}

// shift weekly file
bool SensorFileWeekShift (const uint8_t week)
{
  char str_file_org[32];
  char str_file_dest[32];

  // calculate file names
  sprintf_P (str_file_org, D_SENSOR_FILENAME_WEEK, week);
  sprintf_P (str_file_dest, D_SENSOR_FILENAME_WEEK, week + 1);

  // if exists, rename
  if (TfsFileExists (str_file_org)) return TfsRenameFile (str_file_org, str_file_dest);
    else return false;  
}

// delete weekly file if exists
void SensorFileWeekDelete (const uint8_t week)
{
  char str_file[32];

  // check file
  sprintf_P (str_file, D_SENSOR_FILENAME_WEEK, week);
  TfsDeleteFile (str_file);
}

// load week file to graph data
void SensorFileWeekLoad (const uint8_t week)
{
  uint8_t  token, day, hour, slot;
  uint8_t  humidity;
  uint32_t len_buffer, size_buffer;
  int      value;
  int16_t  temperature;
  char     *pstr_token, *pstr_buffer, *pstr_line;
  char     str_buffer[256];
  File     file;

  // check filesystem
  if (ufs_type == 0) return;

  // init
  pstr_buffer = str_buffer;

  // init values
  for (day = 0; day < 7; day++)
    for (hour = 0; hour < 24; hour++)
      for (slot = 0; slot < 6; slot++)
      {
        sensor_week[day][hour][slot].temp  = INT16_MAX;
        sensor_week[day][hour][slot].humi  = UINT8_MAX;
        sensor_week[day][hour][slot].event.data = 0;
      }

  // loop to read file
  sprintf_P (str_buffer, D_SENSOR_FILENAME_WEEK, week);
  if (TfsFileExists (str_buffer))
  {
    file = ffsp->open (str_buffer, "r");
    strcpy (str_buffer, "");
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
          if (isdigit (pstr_token[0]) || (pstr_token[0] == '-')) 
          {
            value = atoi (pstr_token);
            switch (token)
            {
              case 0:     // day
                day = (value + 1) % 7;
                break;
              case 1:     // hour
                hour = value;
                break;
              case 2:     // slot
                slot = value;
                break;
              case 3:     // temperature
                temperature = (int16_t)value;
                if ((day < 7) && (hour < 24) && (slot < 6)) 
                {
                  if (sensor_week[day][hour][slot].temp == INT16_MAX) sensor_week[day][hour][slot].temp = temperature;
                    else sensor_week[day][hour][slot].temp = max (temperature, sensor_week[day][hour][slot].temp);
                }
                break;
              case 4:     // humidity
                humidity = (uint8_t)value;
                if ((day < 7) && (hour < 24) && (slot < 6))
                {
                  if (sensor_week[day][hour][slot].humi == UINT8_MAX) sensor_week[day][hour][slot].humi = humidity;
                    else sensor_week[day][hour][slot].humi = max (humidity, sensor_week[day][hour][slot].humi);
                }
                break;
              case 5:     // presence
                if ((day < 7) && (hour < 24) && (slot < 6) && (value > 0)) sensor_week[day][hour][slot].event.pres = 1;
                break;
              case 6:     // activity
                if ((day < 7) && (hour < 24) && (slot < 6) && (value > 0)) sensor_week[day][hour][slot].event.acti = 1;
                break;
              case 7:     // inactivity
                if ((day < 7) && (hour < 24) && (slot < 6) && (value > 0)) sensor_week[day][hour][slot].event.inac = 1;
                break;
            }
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
      if (pstr_buffer != str_buffer) strcpy (str_buffer, pstr_buffer);
        else strcpy (str_buffer, "");
    }

    // close file
    file.close ();

    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Week %02u loaded from file"), week);
  }
}

// save current weekly data to file
void SensorFileWeekAppend ()
{
  uint8_t  day, hour, slot;
  char     str_value[32];
  String   str_buffer;
  File     file;

  // check filesystem
  if (ufs_type == 0) return;

  // calculate file name
  sprintf_P (str_value, D_SENSOR_FILENAME_WEEK, 0);

  // if file doesn't exist, create it and append header
  if (!TfsFileExists (str_value))
  {
    file = ffsp->open (str_value, "w");
    file.print ("Day;Hour;Slot;Temp;Humi;Pres;Acti;Inac\n");
  }

  // else, file exists, open in append mode
  else file = ffsp->open (str_value, "a");

  // loop to write all slots
  str_buffer = "";
  day  = (7 + sensor_status.dayofweek - 1) % 7;           // monday is 0
  hour = sensor_status.hour;
  for (slot = 0; slot < 6; slot ++)
  {
    // day, hour, slot
    sprintf (str_value, "%u;%u;%u;", day, hour, slot);
    str_buffer += str_value;

    // temperature
    if (sensor_status.hour_slot[slot].temp == INT16_MAX) strcpy (str_value, "*;");
      else sprintf (str_value, "%d;", sensor_status.hour_slot[slot].temp);
    str_buffer += str_value;

    // humidity
    if (sensor_status.hour_slot[slot].humi == INT8_MAX) strcpy (str_value, "*;");
      else sprintf (str_value, "%d;", sensor_status.hour_slot[slot].humi);
    str_buffer += str_value;

    // presence, activity and inactivity
    sprintf (str_value, "%u;%u;%u\n", sensor_status.hour_slot[slot].event.pres, sensor_status.hour_slot[slot].event.acti, sensor_status.hour_slot[slot].event.inac);
    str_buffer += str_value;
  }

  // write and close
  file.print (str_buffer.c_str ());
  file.close ();

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Last week data saved"));
}

// test presence of yearly file
bool SensorFileYearExist (const uint16_t year)
{
  char str_file[32];

  // check file existence
  sprintf_P (str_file, D_SENSOR_FILENAME_YEAR, year);
  return TfsFileExists (str_file);
}

// load year data from file
void SensorFileYearLoad (const uint16_t year)
{
  uint8_t  token;
  uint8_t  month, day;
  uint32_t len_buffer, size_buffer;
  int      value;
  char     *pstr_token, *pstr_buffer, *pstr_line;
  char     str_value[32];
  char     str_buffer[512];
  File     file;

  // check filesystem
  if (ufs_type == 0) return;

  // init data
  for (month = 0; month < 12; month ++)
    for (day = 0; day < 31; day ++)
    {
      sensor_year[month][day].temp_max = INT16_MAX;
      sensor_year[month][day].temp_min = INT16_MAX;
      sensor_year[month][day].pres     = 0;
    }

  // loop to read file
  pstr_buffer = str_buffer;
  sprintf_P (str_value, D_SENSOR_FILENAME_YEAR, year);
  if (TfsFileExists (str_value))
  {
    file = ffsp->open (str_value, "r");
    strcpy (str_buffer, "");
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
        month = day = UINT8_MAX;
        *pstr_line = 0;
        pstr_token = strtok (pstr_buffer, ";");
        while (pstr_token != nullptr)
        {
          // if value is a numerical one
          if (isdigit (pstr_token[0]) || (pstr_token[0] == '-')) 
          {
            value = atoi (pstr_token);
            switch (token)
            {
              case 1: 
                month = value - 1;
                break;
              case 2:
                day = value - 1; 
                break;
              case 3: 
                if ((month < 12) && (day < 31)) 
                  if (sensor_year[month][day].temp_max == INT16_MAX) sensor_year[month][day].temp_max = value;
                    else if (sensor_year[month][day].temp_max < value) sensor_year[month][day].temp_max = value;
                break;
              case 4: 
                if ((month < 12) && (day < 31)) 
                  if (sensor_year[month][day].temp_min == INT16_MAX) sensor_year[month][day].temp_min = value;
                    else if (sensor_year[month][day].temp_min > value) sensor_year[month][day].temp_min = value;
                break;
              case 7: 
                if ((month < 12) && (day < 31)) sensor_year[month][day].pres |= (uint8_t)value;
                break;
            }
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
      if (pstr_buffer != str_buffer) strcpy (str_buffer, pstr_buffer);
        else strcpy (str_buffer, "");
    }

    // close file
    file.close ();

    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Year %u loaded from file"), year);
  }
}

// save current day to file
void SensorFileYearAppend ()
{
  char     str_value[32];
  String   str_buffer;
  File     file;

  // check filesystem
  if (ufs_type == 0) return;

  // if file doesn't exist, create it and append header
  sprintf_P (str_value, D_SENSOR_FILENAME_YEAR, sensor_status.year);
  if (!TfsFileExists (str_value))
  {
    file = ffsp->open (str_value, "w");
    file.print ("DayOfYear;Month;DayOfMonth;Tmax;Tmin;Hmax;Hmin;Pres\n");
  }

  // else, file exists, open in append mode
  else file = ffsp->open (str_value, "a");

  // day
  sprintf (str_value, "%u;%u;%u;", sensor_status.dayofyear, sensor_status.month, sensor_status.dayofmonth);
  str_buffer = str_value;

  // temperature max
  if (sensor_status.day_slot.temp_max == INT16_MAX) strcpy (str_value, "*;");
    else sprintf (str_value, "%d;", sensor_status.day_slot.temp_max);
  str_buffer += str_value;

  // temperature min
  if (sensor_status.day_slot.temp_min == INT16_MAX) strcpy (str_value, "*;");
    else sprintf (str_value, "%d;", sensor_status.day_slot.temp_min);
  str_buffer += str_value;

  // humidity
  str_buffer += "*;*;";

  // presence
  sprintf (str_value, "%u\n", sensor_status.day_slot.pres);
  str_buffer += str_value;

  // write and close
  file.print (str_buffer.c_str ());
  file.close ();

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Latest year data saved"));
}

#endif      // USE_UFILESYS

/**************************************************\
 *                  Callback
\**************************************************/

void SensorInit () 
{
  uint8_t index;
  char    str_type[16];

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

  // init slot data
  sensor_status.hour       = UINT8_MAX;
  sensor_status.dayofweek  = UINT8_MAX;
  sensor_status.month      = UINT8_MAX;
  sensor_status.dayofmonth = UINT8_MAX;
  sensor_status.dayofyear  = UINT16_MAX;

  // init hourly data
  for (index = 0; index < 6; index ++)
  {
    sensor_status.hour_slot[index].temp = INT16_MAX;
    sensor_status.hour_slot[index].humi = INT8_MAX;
    sensor_status.hour_slot[index].event.data = 0;     // pres, acti & inac
  }

  // init daily data
  sensor_status.day_slot.temp_max = INT16_MAX;
  sensor_status.day_slot.temp_min = INT16_MAX;
  sensor_status.day_slot.pres     = 0;
  
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
  }

  // presence : check for generic sensor
  if (PinUsed (GPIO_CNTR1, SENSOR_PRESENCE_INDEX))
  {
    switch (sensor_config.device_pres)
    {
      case SENSOR_PRESENCE_SWITCH:
      case SENSOR_PRESENCE_RCWL0516:
      case SENSOR_PRESENCE_HWMS03:
        sensor_status.pres.source = SENSOR_SOURCE_COUNTER;
        GetTextIndexed (str_type, sizeof (str_type), sensor_config.device_pres, kSensorPresenceModel);
        AddLog (LOG_LEVEL_INFO, PSTR ("SEN: %s motion sensor"), str_type);
        break;
    }
  }

  // movement : serial sensors
  else if (PinUsed (GPIO_LD2410_TX) && PinUsed (GPIO_LD2410_RX))
  {
    switch (sensor_config.device_pres)
    {
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

#ifdef USE_LD2450             // HLK-LD2450 sensor
      case SENSOR_PRESENCE_LD2450:
        if (LD2450InitDevice ()) sensor_status.pres.source = SENSOR_SOURCE_SERIAL;
        break;
#endif    // USE_LD2450

      default:
        break;
    }
  }

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: sens_help to get help on Sensor commands"));
}

// update sensor condition every second
void SensorEverySecond ()
{
  bool     presence = true;                   // if no presence sensor, presence triggered by default
  bool     publish  = false;
  int8_t   value;
  uint8_t  mask;
  uint8_t  month, hour, minute, second;
  uint8_t  dayofmonth, dayofweek;
  uint8_t  day_slot, hour_slot;
  uint32_t sensor, index, slot;
  float    temperature = NAN;
  float    humidity    = NAN;

  // get current time
  month      = RtcTime.month - 1;
  dayofmonth = RtcTime.day_of_month - 1;
  dayofweek  = RtcTime.day_of_week - 1;
  hour       = RtcTime.hour;
  minute     = RtcTime.minute;
  second     = RtcTime.second;
  day_slot   = hour / 3; 
  hour_slot  = minute / 10; 

  // if needed, update current slot values
  if (sensor_status.dayofyear == UINT16_MAX) sensor_status.dayofyear  = RtcTime.day_of_year;
  if (sensor_status.year == UINT16_MAX)      sensor_status.year       = RtcTime.year;
  if (sensor_status.hour == UINT8_MAX)       sensor_status.hour       = RtcTime.hour;
  if (sensor_status.dayofweek == UINT8_MAX)  sensor_status.dayofweek  = dayofweek;
  if (sensor_status.dayofmonth == UINT8_MAX) sensor_status.dayofmonth = dayofmonth;
  if (sensor_status.month == UINT8_MAX)      sensor_status.month      = month;

  // if hour has changed
  if (sensor_status.hour != RtcTime.hour)
  {
#ifdef USE_UFILESYS
    // save weekly slot
    SensorFileWeekAppend ();
#endif      // USE_UFILESYS

    // reset hourly data
    for (index = 0; index < 6; index ++)
    {
      sensor_status.hour_slot[index].temp = INT16_MAX;
      sensor_status.hour_slot[index].humi = INT8_MAX;
      sensor_status.hour_slot[index].event.data = 0;     // pres, acti & inac
    }
  }

  // if day has changed, save yearly slot
  if (sensor_status.dayofyear != RtcTime.day_of_year)
  {
#ifdef USE_UFILESYS
    // save yearly slot
    SensorFileYearAppend ();
#endif      // USE_UFILESYS

    // reset daily data
    sensor_status.day_slot.temp_max  = INT16_MAX;
    sensor_status.day_slot.temp_min  = INT16_MAX;
    sensor_status.day_slot.pres      = 0;

    // reset current day in weekly data
    for (index = 0; index < 24; index ++)
      for (slot = 0; slot < 6; slot ++)
      {
        sensor_week[dayofweek][index][slot].temp = INT16_MAX;
        sensor_week[dayofweek][index][slot].humi = INT8_MAX;
        sensor_week[dayofweek][index][slot].event.data = 0;
      }
  }

  // if 1st of the month at midnight, erase current month in yearly data
  if ((sensor_status.month != month) && (sensor_status.dayofmonth != dayofmonth))
  {
    // reset daily data
    for (index = 0; index < 31; index ++)
    {
      sensor_year[month][index].temp_max = INT16_MAX;
      sensor_year[month][index].temp_min = INT16_MAX;
      sensor_year[month][index].pres     = 0;
    }
  }

    // ---------------------------------------
  //   EVERY SUNDAY NIMUTE BEFORE MIDNIGHT
  //   shift and clenup weekly files
  // ----------------------------------

// a changer

#ifdef USE_UFILESYS
  if ((dayofweek == 0) && (hour == 23) && (minute == 59))
  {
    index = 59 - second;
    if (index > sensor_config.week_histo) SensorFileWeekDelete (index);
      else SensorFileWeekShift (index);
  }
#endif      // USE_UFILESYS

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
      if (sensor_status.temp.last == INT16_MAX) publish = true;
      if ((sensor_status.temp.last != INT16_MAX) && (sensor_status.temp.last > sensor_status.temp.value + 1)) publish = true;
      if ((sensor_status.temp.last != INT16_MAX) && (sensor_status.temp.last < sensor_status.temp.value - 1)) publish = true;
    }
    
    // check if humidity needs JSON update
    if (sensor_status.humi.value != INT16_MAX)
    {
      if (sensor_status.humi.last == INT16_MAX) publish = true;
      if ((sensor_status.humi.last != INT16_MAX) && (sensor_status.humi.last > sensor_status.humi.value + 4)) publish = true;
      if ((sensor_status.humi.last != INT16_MAX) && (sensor_status.humi.last < sensor_status.humi.value - 4)) publish = true;
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
    switch (sensor_config.device_pres)
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

#ifdef USE_LD2450
      case SENSOR_PRESENCE_LD2450:
        presence = LD2450GetDetectionStatus (1);
        break;
#endif      // USE_LD2450
    }
  }

  // if presence newly detected
  if (presence)
  {
    // if newly detected, publish
    publish = (sensor_status.pres.value != 1);
    
    // set presence
    SensorPresenceSet ();
  } 

  // if presence detectection is active
  else if (sensor_status.pres.value == 1)
  {
    // if needed, ask for a JSON update
    if (Rtc.local_time > sensor_status.time_json) publish = true;

    // check if presence should be reset
    if (LocalTime () > sensor_status.pres.timestamp + SENSOR_PRES_TIMEOUT) publish = SensorPresenceReset ();
  }

  // update hourly data and current weekly data
  if ((dayofweek < 7) && (hour < 24) && (hour_slot < 6))
  {
    // update current temperature slot
    if (sensor_status.temp.value != INT16_MAX)
    {
      if (sensor_status.hour_slot[hour_slot].temp == INT16_MAX) sensor_status.hour_slot[hour_slot].temp = sensor_status.temp.value;
        else if (sensor_status.hour_slot[hour_slot].temp < sensor_status.temp.value) sensor_status.hour_slot[hour_slot].temp = sensor_status.temp.value;
    }
 
    // update current humidity slot
    if (sensor_status.humi.value != INT16_MAX)
    {
      value = (int8_t)(sensor_status.humi.value / 10);
      if (sensor_status.hour_slot[hour_slot].humi == INT8_MAX) sensor_status.hour_slot[hour_slot].humi = value;
        else if (sensor_status.hour_slot[hour_slot].humi < value) sensor_status.hour_slot[hour_slot].humi = value;
    }

    // update current presence slot
    if (sensor_status.pres.value == 1) sensor_status.hour_slot[hour_slot].event.pres = 1;

    // update current weekly data
    sensor_week[dayofweek][hour][hour_slot].temp       = sensor_status.hour_slot[hour_slot].temp;
    sensor_week[dayofweek][hour][hour_slot].humi       = sensor_status.hour_slot[hour_slot].humi;
    sensor_week[dayofweek][hour][hour_slot].event.data = sensor_status.hour_slot[hour_slot].event.data;
  }

  // update daily data and current yearly data
  if ((month < 12) && (dayofmonth < 31))
  {
    // if temperature is available
    if (sensor_status.temp.value != INT16_MAX)
    {
      // update minimum daily temperature
      if (sensor_status.day_slot.temp_min == INT16_MAX) sensor_status.day_slot.temp_min = sensor_status.temp.value;
        else if (sensor_status.day_slot.temp_min > sensor_status.temp.value) sensor_status.day_slot.temp_min = sensor_status.temp.value;

      // update maximum daily temperature
      if (sensor_status.day_slot.temp_max == INT16_MAX) sensor_status.day_slot.temp_max = sensor_status.temp.value;
        else if (sensor_status.day_slot.temp_max < sensor_status.temp.value) sensor_status.day_slot.temp_max = sensor_status.temp.value;
    }

    // update daily presence in 3h slots
    if (sensor_status.pres.value == 1)
    {
      mask = 1;
      for (index = 0; index < day_slot; index ++) mask = mask << 1;
      sensor_status.day_slot.pres = sensor_status.day_slot.pres | mask;
    }

    // current yearly data
    sensor_year[month][dayofmonth].temp_max = sensor_status.day_slot.temp_max;
    sensor_year[month][dayofmonth].temp_min = sensor_status.day_slot.temp_min;
    sensor_year[month][dayofmonth].pres     = sensor_status.day_slot.pres;
  }

  // update new slot data
  sensor_status.dayofyear  = RtcTime.day_of_year;
  sensor_status.year       = RtcTime.year;
  sensor_status.hour       = RtcTime.hour;
  sensor_status.dayofweek  = dayofweek;
  sensor_status.dayofmonth = dayofmonth;
  sensor_status.month      = month;

  // ---------
  //   JSON
  // ---------

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
  sprintf (str_text, "\"%s\"", pstr_key);
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
      if (sensor_status.pres.source == SENSOR_SOURCE_NONE)
      {
        sensor_status.pres.source = SENSOR_SOURCE_REMOTE;
        sensor_config.device_pres = SENSOR_PRESENCE_REMOTE;
      }

      // read presence detection
      detected = ((strcmp (str_value, "1") == 0) || (strcmp (str_value, "on") == 0) || (strcmp (str_value, "ON") == 0));
      if (detected) SensorPresenceSet ();
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
  is_pres  = (sensor_status.pres.source < SENSOR_SOURCE_REMOTE);
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
      if (sensor_status.pres.value != UINT16_MAX) ResponseAppend_P (PSTR ("\"Pres\":%d,"), sensor_status.pres.value);
      if (sensor_status.pres.timestamp != UINT32_MAX) ResponseAppend_P (PSTR ("\"Since\":%u,"), LocalTime () - sensor_status.pres.timestamp);
      SensorGetDelayText (sensor_status.pres.timestamp, str_value, sizeof (str_value));
      ResponseAppend_P (PSTR ("\"Delay\":\"%s\""), str_value);
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

// append remote sensor to main page
void SensorWebSensor ()
{
  bool result, temp_remote, humi_remote, pres_remote;
  char str_type[16];
  char str_text[32];

  // basic counter or remote sensor display
  if ((sensor_status.pres.source == SENSOR_SOURCE_COUNTER) || (sensor_status.pres.source == SENSOR_SOURCE_REMOTE))
  {
    // init
    strcpy (str_type, "");
    strcpy (str_text, "");

    // get type
    GetTextIndexed (str_type, sizeof (str_type), sensor_config.device_pres, kSensorPresenceModel);

    // display presence sensor
    WSContentSend_P (PSTR ("<div style='display:flex;font-size:10px;text-align:center;margin:4px 0px;padding:2px 6px;background:#333333;border-radius:8px;'>\n"));
    WSContentSend_P (PSTR ("<div style='width:40%%;padding:0px;text-align:left;font-size:12px;font-weight:bold;'>%s</div>\n"), str_type);
    if (sensor_status.pres.value == 1) strcpy (str_text, "#D00"); else strcpy (str_text, "#444");
    WSContentSend_P (PSTR ("<div style='width:30%%;padding:0px;border-radius:6px;background:%s;'></div>"), str_text);
    switch (sensor_config.device_pres)
    {
      case SENSOR_PRESENCE_SWITCH:   strcpy (str_text, "contact"); break;
      case SENSOR_PRESENCE_RCWL0516: strcpy (str_text, "0 - 7m"); break;
      case SENSOR_PRESENCE_HWMS03:   strcpy (str_text, "0.5 - 10m"); break;
      case SENSOR_PRESENCE_REMOTE:   strcpy (str_text, D_SENSOR_REMOTE); break;
      default: strcpy (str_text, ""); break;
    }
    WSContentSend_P (PSTR ("<div style='width:30%%;padding:2px 0px 0px 0px;text-align:right;'>%s</div>\n"), str_text);
    WSContentSend_P (PSTR ("</div>\n"));
  }
 
  // sensor display
  WSContentSend_P (PSTR ("<div style='font-size:14px;text-align:center;margin:4px 0px;padding:4px 6px;background:#333333;border-radius:8px;'>\n"));
  WSContentSend_P (PSTR ("<div style='display:flex;margin:2px 0px;padding:1px;'><div style='width:100%%;padding:0px;text-align:left;font-weight:bold;'>Sensor</div></div>\n"));

  // temperature
  if (sensor_status.temp.source != SENSOR_SOURCE_NONE)
  {
    if (sensor_status.temp.source == SENSOR_SOURCE_REMOTE) strcpy (str_type, "🌐"); else strcpy (str_type, "🌡️");
    if (sensor_status.temp.value == INT16_MAX) strcpy (str_text, "---"); else sprintf (str_text, "%d.%d", sensor_status.temp.value / 10, sensor_status.temp.value % 10);
    WSContentSend_P (PSTR ("<div style='display:flex;margin:2px 0px;padding:1px;'>\n"));
    WSContentSend_P (PSTR ("<div style='width:15%%;padding:0px;'>%s</div>\n"), str_type);
    WSContentSend_P (PSTR ("<div style='width:47%%;padding:0px;text-align:left;'>Temperature</div>\n"));
    WSContentSend_P (PSTR ("<div style='width:26%%;padding:0px;text-align:right;'>%s</div><div style='width:2%%;padding:0px;'></div><div style='width:10%%;padding:0px;text-align:left;'>°C</div>\n"), str_text);
    WSContentSend_P (PSTR ("</div>\n"));
  }

  // humidity
  if (sensor_status.humi.source != SENSOR_SOURCE_NONE)
  {
    if (sensor_status.humi.source == SENSOR_SOURCE_REMOTE) strcpy (str_type, "🌐"); else strcpy (str_type, "💧");
    if (sensor_status.humi.value == INT16_MAX) strcpy (str_text, "---"); else sprintf (str_text, "%d.%d", sensor_status.humi.value / 10, sensor_status.humi.value % 10);
    WSContentSend_P (PSTR ("<div style='display:flex;margin:2px 0px;padding:1px;'>\n"));
    WSContentSend_P (PSTR ("<div style='width:15%%;padding:0px;'>%s</div>\n"), str_type);
    WSContentSend_P (PSTR ("<div style='width:47%%;padding:0px;text-align:left;'>Humidity</div>\n"));
    WSContentSend_P (PSTR ("<div style='width:26%%;padding:0px;text-align:right;'>%s</div><div style='width:2%%;padding:0px;'></div><div style='width:10%%;padding:0px;text-align:left;'>%%</div>\n"), str_text);
    WSContentSend_P (PSTR ("</div>\n"));
  }

  // presence
  if (sensor_status.pres.source != SENSOR_SOURCE_NONE)
  {
    if (sensor_status.humi.source == SENSOR_SOURCE_REMOTE) strcpy (str_type, "🌐"); else strcpy (str_type, "📡");
    WSContentSend_P (PSTR ("<div style='display:flex;margin:2px 0px;padding:1px;'>\n"));
    WSContentSend_P (PSTR ("<div style='width:15%%;padding:0px;'>%s</div>\n"), str_type);
    WSContentSend_P (PSTR ("<div style='width:47%%;padding:0px;text-align:left;'>Presence</div>\n"));
    SensorGetDelayText (sensor_status.pres.timestamp, str_text, sizeof (str_text));
    WSContentSend_P (PSTR ("<div style='width:33%%;padding:0px;text-align:right;'>%s</div><div style='width:5%%;padding:0px;'></div>\n"), str_text);
    WSContentSend_P (PSTR ("</div>\n"));
  }

  WSContentSend_P (PSTR ("</div>\n"));
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
    sensor_config.device_pres = (uint8_t)atoi (str_text);

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
  sprintf (str_value, "%u.%u", Settings->temp_comp / 10, Settings->temp_comp % 10);
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
#ifdef USE_GENERIC_PRESENCE
  WSContentSend_P (SENSOR_FIELDSET_START, "📡", "Presence Sensor");

  for (index = 0; index < SENSOR_PRESENCE_MAX; index ++)
  {
    GetTextIndexed (str_text, sizeof (str_text), index, kSensorPresenceModel);
    if (sensor_config.device_pres == index) strcpy (str_value, "checked"); else strcpy (str_value, "");
    WSContentSend_P (PSTR ("<p><input type='radio' name='%s' value='%d' %s>%s</p>\n"), D_CMND_SENSOR_PRES D_CMND_SENSOR_TYPE, index, str_value, str_text);
  }

  WSContentSend_P (SENSOR_FIELDSET_STOP);
#endif      // USE_GENERIC_PRESENCE

  // end of form and save button
  WSContentSend_P (PSTR ("<br><p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), D_SAVE);
  WSContentSend_P (PSTR ("</form>\n"));       // get

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

// Sensor graph style
void SensorWebGraphSwipe ()
{
  // javascript : screen swipe for previous and next period
  WSContentSend_P (PSTR ("\nlet startX=0;let stopX=0;\n"));
  WSContentSend_P (PSTR ("window.addEventListener('touchstart',function(evt){startX=evt.changedTouches[0].screenX;},false);\n"));
  WSContentSend_P (PSTR ("window.addEventListener('touchend',function(evt){stopX=evt.changedTouches[0].screenX;handleGesture();},false);\n"));
  WSContentSend_P (PSTR ("function handleGesture(){\n"));
  WSContentSend_P (PSTR ("if(stopX<startX-100){document.getElementById('next').click();}\n"));
  WSContentSend_P (PSTR ("else if(stopX>startX+100){document.getElementById('prev').click();}\n"));
  WSContentSend_P (PSTR ("}\n"));
}

// --------------------
//    Measure Graph
// --------------------

// Sensor graph style
void SensorWebGraphWeeklyMeasureStyle ()
{
  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));

  WSContentSend_P (PSTR ("a {color:white;}\n"));
  WSContentSend_P (PSTR ("a:link {text-decoration:none;}\n"));

  WSContentSend_P (PSTR ("div {padding:0px;margin:0px;text-align:center;}\n"));
  WSContentSend_P (PSTR ("div.title {font-size:4vh;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.header {font-size:3vh;margin:1vh auto;}\n"));

  WSContentSend_P (PSTR ("div.value {font-size:4vh;padding-bottom:1vh;}\n"));
  WSContentSend_P (PSTR ("div.value span {margin:0px 2vw;}\n"));
  WSContentSend_P (PSTR ("div.value span#humi {color:%s;}\n"), SENSOR_COLOR_HUMI);
  WSContentSend_P (PSTR ("div.value span#temp {color:%s;}\n"), SENSOR_COLOR_TEMP);

  WSContentSend_P (PSTR ("button {padding:2px 12px;font-size:2.5vh;background:none;color:#fff;border:1px #666 solid;border-radius:6px;}\n"));
  WSContentSend_P (PSTR ("button:hover {background:#aaa;}\n"));
  WSContentSend_P (PSTR ("button:disabled {color:#252525;background:#252525;border:1px #252525 solid;}\n"));

  WSContentSend_P (PSTR ("div.banner {width:88%%;margin:auto;}\n"));
  WSContentSend_P (PSTR ("div.banner div {display:inline-block;}\n"));
  WSContentSend_P (PSTR ("div.date {font-size:3vh;}\n"));
  WSContentSend_P (PSTR ("div.prev {float:left;}\n"));
  WSContentSend_P (PSTR ("div.next {float:right;}\n"));

  // main menu button
  WSContentSend_P (PSTR ("button.menu {background:%s;color:%s;line-height:2rem;font-size:1.2rem;cursor:pointer;border:none;border-radius:0.3rem;width:150px;margin:2vh;}\n"), COLOR_BUTTON, COLOR_BUTTON_TEXT);

  WSContentSend_P (PSTR ("div.graph {width:100%%;margin:1vh auto;}\n"));
  WSContentSend_P (PSTR ("svg.graph {width:100%%;height:50vh;}\n"));
}

// Sensor graph curve display
void SensorWebGraphWeeklyMeasureCurve (const uint8_t week, const char* pstr_url)
{
//  bool     data_ok = true;
  uint8_t  day, hour, slot, value;
  uint8_t  count, count_start, count_stop;
  uint8_t  prev_week, next_week;
  int16_t  scale, temp_range, temp_incr, temp_min, temp_max;
  uint32_t index, unit, graph_width, graph_x, prev_x, graph_y, prev_y, last_x;
  char     str_text[16];
  char     str_type[16];

  // check parameters
  if (week > 52) return;
  if (pstr_url == nullptr) return;

#ifdef USE_UFILESYS
  // set display sequence
  count_start = 1;
  count_stop  = 8;

  // load weekly data
  SensorFileWeekLoad (week);
#else
  // set display sequence
  count_start = RtcTime.day_of_week;
  count_stop  = count_start + 7;
#endif      // USE_UFILESYS

  // boundaries of SVG graph
  last_x = SENSOR_GRAPH_START;
  graph_width = SENSOR_GRAPH_STOP - SENSOR_GRAPH_START;

  // loop to calculate minimum and maximum temperature for the graph
  if (sensor_config.weekly_temp)
  {
    temp_min = temp_max = INT16_MAX;
    for (day = 0; day < 7; day++)
      for (hour = 0; hour < 24; hour++)
        for (slot = 0; slot < 6; slot++)
        {
          // minimum temperature
          if (temp_min == INT16_MAX) temp_min = sensor_week[day][hour][slot].temp;
            else if (sensor_week[day][hour][slot].temp != INT16_MAX) temp_min = min (temp_min, sensor_week[day][hour][slot].temp);

          // maximum temperature
          if (temp_max == INT16_MAX) temp_max = sensor_week[day][hour][slot].temp;
            else if (sensor_week[day][hour][slot].temp != INT16_MAX) temp_max = max (temp_max, sensor_week[day][hour][slot].temp);
        }

    // set upper and lower range according to min and max temperature
    if (temp_min != INT16_MAX) temp_min = (temp_min / 10 - 2) * 10;
      else temp_min = 180; 
    if (temp_max != INT16_MAX) temp_max = (temp_max / 10 + 2) * 10;
      else temp_max = 220;

    // calculate temperature range and increment
    temp_range = temp_max - temp_min;
    temp_incr  = max (temp_range / 40 * 10, 10);
  }

#ifdef USE_UFILESYS
 
  // calculate previous and next week
  if (week > 0) next_week = week - 1; else next_week = UINT8_MAX;
  prev_week = week + 1;

  // start of form
  WSContentSend_P (PSTR ("<form method='get' action='/%s'>\n"), pstr_url);
  WSContentSend_P (PSTR ("<div class='banner'>\n"));

  // date
  SensorGetWeekLabel (week, str_text, sizeof (str_text));
  WSContentSend_P (PSTR ("<div class='date'>%s</div>\n"), str_text);

  // if exist, navigation to previous week
  SensorGetWeekLabel (prev_week, str_text, sizeof (str_text));
  if (SensorFileWeekExist (prev_week)) strcpy (str_type, ""); else strcpy (str_type, "disabled");
  WSContentSend_P (PSTR ("<div class='prev'><button name='week' title='%s' value=%u id='prev' %s>&lt;&lt;</button></div>\n"), str_text, prev_week, str_type);

  // if exist, navigation to next week or to live values
  SensorGetWeekLabel (next_week, str_text, sizeof (str_text));
  if (SensorFileWeekExist (next_week)) strcpy (str_type, ""); else strcpy (str_type, "disabled");
  WSContentSend_P (PSTR ("<div class='next'><button name='week' title='%s' value=%u id='next' %s>&gt;&gt;</button></div>\n"), str_text, next_week, str_type);

  // end of form
  WSContentSend_P (PSTR ("</div>\n"));      // banner
  WSContentSend_P (PSTR ("</form>\n"));     // get

#endif    // USE_UFILESYS

  // start of SVG graph
  WSContentSend_P (PSTR ("<div class='graph'>\n"));
  WSContentSend_P (PSTR ("<svg class='graph' viewBox='%d %d %d %d' preserveAspectRatio='none'>\n"), 0, 0, SENSOR_GRAPH_WIDTH, SENSOR_GRAPH_HEIGHT + 1);

  // SVG style
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));

  WSContentSend_P (PSTR ("text {font-size:18px}\n"));

  WSContentSend_P (PSTR ("polyline.temp {stroke:%s;fill:none;stroke-width:2;}\n"), SENSOR_COLOR_TEMP);
  WSContentSend_P (PSTR ("path.humi {stroke:%s;fill:%s;opacity:1;fill-opacity:0.5;}\n"), SENSOR_COLOR_HUMI, SENSOR_COLOR_HUMI);

  WSContentSend_P (PSTR ("rect.frame {fill:none;stroke:#aaa;}\n"));
  WSContentSend_P (PSTR ("rect.none {fill:%s;opacity:0.8;}\n"), SENSOR_COLOR_NONE);
  WSContentSend_P (PSTR ("rect.acti {fill:%s;opacity:0.8;}\n"), SENSOR_COLOR_ACTI);
  WSContentSend_P (PSTR ("rect.inac {fill:%s;opacity:0.8;}\n"), SENSOR_COLOR_INAC);

  WSContentSend_P (PSTR ("line {stroke-dasharray:1 2;stroke-width:0.5;}\n"));
  WSContentSend_P (PSTR ("line.time {stroke:%s;}\n"), SENSOR_COLOR_LINE);
  WSContentSend_P (PSTR ("line.temp {stroke:%s;}\n"), SENSOR_COLOR_TEMP);
  WSContentSend_P (PSTR ("line.humi {stroke:%s;}\n"), SENSOR_COLOR_HUMI);

  WSContentSend_P (PSTR ("text.day {fill:%s;font-size:16px;}\n"), SENSOR_COLOR_TIME);
  WSContentSend_P (PSTR ("text.today {fill:%s;font-size:16px;font-weight:bold;}\n"), SENSOR_COLOR_TODAY);
  WSContentSend_P (PSTR ("text.temp {fill:%s;}\n"), SENSOR_COLOR_TEMP);
  WSContentSend_P (PSTR ("text.humi {fill:%s;}\n"), SENSOR_COLOR_HUMI);

  WSContentSend_P (PSTR ("</style>\n"));

  // ------  Humidity  ---------

  if (sensor_config.weekly_humi)
  {
    // init
    index   = 0;
    graph_x = prev_x = SENSOR_GRAPH_START;
    graph_y = prev_y = UINT32_MAX;

    // loop thru points
    for (count = count_start; count < count_stop; count++)
    {
      day = count % 7;
      for (hour = 0; hour < 24; hour++)
        for (slot = 0; slot < 6; slot++)
        {
          // calculate x coordinate
          graph_x = SENSOR_GRAPH_START + (index * graph_width / 1008);

          // calculate y coordinate
          if (sensor_week[day][hour][slot].humi != INT8_MAX) graph_y = SENSOR_GRAPH_HEIGHT - SENSOR_GRAPH_HEIGHT * (uint32_t)sensor_week[day][hour][slot].humi / 100;
            else graph_y = UINT32_MAX;

          // if needed, start path
          if ((graph_y != UINT32_MAX) && (prev_y == UINT32_MAX))  WSContentSend_P (PSTR ("<path class='humi' d='M%d %d "), graph_x, SENSOR_GRAPH_HEIGHT);

          // else if needed, stop path
          else if ((graph_y == UINT32_MAX) && (prev_y != UINT32_MAX)) WSContentSend_P (PSTR ("L%d %d '/>\n"), prev_x, SENSOR_GRAPH_HEIGHT);

          // if needed, draw point
          else if (graph_y != UINT32_MAX) WSContentSend_P (PSTR ("L%u %u "), graph_x, graph_y);

          // save previous values
          if (graph_y != UINT32_MAX) last_x = max (last_x, graph_x);
          prev_x = graph_x;
          prev_y = graph_y;
          index++;
        }
    }

    // end of graph
    if (graph_y != UINT32_MAX) WSContentSend_P (PSTR ("L%u %u '/>\n"), graph_x, SENSOR_GRAPH_HEIGHT);
  }

  // ---------  Temperature  ---------

  if (sensor_config.weekly_temp)
  {
    // init
    index   = 0;
    graph_x = prev_x = SENSOR_GRAPH_START;
    graph_y = prev_y = UINT32_MAX;

    // loop thru points
    for (count = count_start; count < count_stop; count++)
    {
      day = count % 7;
      for (hour = 0; hour < 24; hour++)
        for (slot = 0; slot < 6; slot++)
        {
          // calculate x coordinate
          graph_x = SENSOR_GRAPH_START + (index * graph_width / 1008);

          // calculate y coordinate
          if (sensor_week[day][hour][slot].temp == INT16_MAX) graph_y = UINT32_MAX;
            else graph_y = SENSOR_GRAPH_HEIGHT - SENSOR_GRAPH_HEIGHT * (uint32_t)(sensor_week[day][hour][slot].temp - temp_min) / (uint32_t)temp_range;

          // if needed, start polyline
          if ((graph_y != UINT32_MAX) && (prev_y == UINT32_MAX)) WSContentSend_P (PSTR ("<polyline class='temp' points='"));

          // else if needed, stop polyline
          else if ((graph_y == UINT32_MAX) && (prev_y != UINT32_MAX)) WSContentSend_P (PSTR ("'/>\n"));

          // if needed, draw point
          if (graph_y != UINT32_MAX) WSContentSend_P (PSTR ("%u,%u "), graph_x, graph_y);

          // save previous values
          if (graph_y != UINT32_MAX) last_x = max (last_x, graph_x);

          // increment
          prev_x = graph_x;
          prev_y = graph_y;
          index++;
        }
    }

    if (graph_y != UINT32_MAX) WSContentSend_P (PSTR ("'/>\n"));
  }

  // ---------  Activity  ---------

  if (sensor_config.weekly_acti)
  {
    // init
    index   = 0;
    graph_x = prev_x = UINT32_MAX;
    graph_y = prev_y = SENSOR_GRAPH_HEIGHT - SENSOR_GRAPH_ACTI;

    // loop thru points
    for (count = count_start; count < count_stop; count++)
    {
      day = count % 7;
      for (hour = 0; hour < 24; hour++)
        for (slot = 0; slot < 6; slot++)
        {
          // read slot status
          value = sensor_week[day][hour][slot].event.acti;

          // start and end of activity
          if ((prev_x == UINT32_MAX) && (value == 1)) prev_x = SENSOR_GRAPH_START + index * graph_width / 1008;
            else if ((prev_x != UINT32_MAX) && (value == 0)) graph_x = SENSOR_GRAPH_START + index * graph_width / 1008;

          // if activity should be drawn
          if ((prev_x != UINT32_MAX) && (graph_x != UINT32_MAX))
          {
            WSContentSend_P (PSTR ("<rect class='acti' x=%d y=%d width=%d height=%d />\n"), prev_x, graph_y, graph_x - prev_x, SENSOR_GRAPH_ACTI);
            prev_x = graph_x = UINT32_MAX;
          }

          // increase index
          index++;
        } 
    }

    if (prev_x != UINT32_MAX) WSContentSend_P (PSTR ("<rect class='acti' x=%d y=%d width=%d height=%d />\n"), prev_x, graph_y, graph_x - prev_x, SENSOR_GRAPH_ACTI);
  }

  // ---------  Inactivity  ---------

  if (sensor_config.weekly_inac)
  {
    // init
    index   = 1;
    graph_x = prev_x = UINT32_MAX;
    graph_y = prev_y = SENSOR_GRAPH_HEIGHT - SENSOR_GRAPH_INAC;

    // loop thru points
    for (count = count_start; count < count_stop; count++)
    {
      day = count % 7;
      for (hour = 0; hour < 24; hour++)
        for (slot = 0; slot < 6; slot++)
        {
          // read slot status
          value = sensor_week[day][hour][slot].event.inac;

          // start and end of activity
          if ((prev_x == UINT32_MAX) && (value == 1)) prev_x = SENSOR_GRAPH_START + (index * graph_width / 1008);
            else if ((prev_x != UINT32_MAX) && (value != 1)) graph_x = SENSOR_GRAPH_START + (index - 1) * graph_width / 1008;

          // if activity should be drawn
          if ((prev_x != UINT32_MAX) && (graph_x != UINT32_MAX))
          {
            WSContentSend_P (PSTR ("<rect class='inac' x=%d y=%d width=%d height=%d />\n"), prev_x, graph_y, graph_x - prev_x, SENSOR_GRAPH_INAC);
            prev_x = graph_x = UINT32_MAX;
          }

          // increase index
          index++;
        } 
    }

    if (prev_x != UINT32_MAX) WSContentSend_P (PSTR ("<rect class='inac' x=%d y=%d width=%d height=%d />\n"), prev_x, graph_y, graph_x - prev_x, SENSOR_GRAPH_INAC);
  }

  // -------  Frame  -------

  WSContentSend_P (PSTR ("<rect class='frame' x=%u y=%u width=%u height=%u rx=4 />\n"), SENSOR_GRAPH_START, 0, SENSOR_GRAPH_STOP - SENSOR_GRAPH_START, SENSOR_GRAPH_HEIGHT + 1);

  // -------  Time line  --------

  // display week days
  for (count = count_start; count < count_stop; count++)
  {
    // get day label
    day = count % 7;
    strlcpy (str_text, kWeekdayNames + day * 3, 4);

    // display day
    graph_x = SENSOR_GRAPH_START + ((count - count_start) * 2 + 1) * graph_width / 14 - 10;
    WSContentSend_P (PSTR ("<text class='day' x=%u y=%u>%s</text>\n"), graph_x, 20, str_text);
  }

  // display separation lines
  for (index = 1; index < 7; index++)
  {
    graph_x = SENSOR_GRAPH_START + index * graph_width / 7;
    WSContentSend_P (PSTR ("<line class='time' x1=%u y1=%u x2=%u y2=%u />\n"), graph_x, 5, graph_x, SENSOR_GRAPH_HEIGHT - 5);
  }

  // -------  Units  -------

  // temperature units
  if (sensor_config.weekly_temp)
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
  if (sensor_config.weekly_humi)
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

  WSContentSend_P (PSTR ("</svg>\n"));      // graph
  WSContentSend_P (PSTR ("</div>\n"));      // graph
}

// Temperature and humidity weekly page
void SensorWebWeeklyMeasure ()
{
  uint8_t week;
  char    str_value[4];

  // if target temperature has been changed
  if (Webserver->hasArg ("week"))
  {
    WebGetArg ("week", str_value, sizeof(str_value));
    if (strlen(str_value) > 0) week = (uint8_t)atoi (str_value);
  }
  else week = 0;

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
  SensorWebGraphWeeklyMeasureStyle ();
  WSContentSend_P (PSTR ("</style>\n"));

  WSContentSend_P (PSTR ("</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  // room name
  WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText(SET_DEVICENAME));

  // header
  WSContentSend_P (PSTR ("<div class='header'>Weekly Measure</div>\n"));

  // ------- Values --------

  WSContentSend_P (PSTR ("<div class='value'><a href='/'>"));

  // temperature
  if (sensor_config.weekly_temp)
    if (sensor_status.temp.value != INT16_MAX) WSContentSend_P (PSTR ("<span id='temp'>%d.%d °C</span>"), sensor_status.temp.value / 10, sensor_status.temp.value % 10);
      else WSContentSend_P (PSTR ("<span id='temp'>--- °C</span>"));

  // humidity
  if (sensor_config.weekly_humi)
    if (sensor_status.humi.value != INT16_MAX) WSContentSend_P (PSTR ("<span id='humi'>%d.%d %%</span>"), sensor_status.humi.value / 10, sensor_status.humi.value % 10);
      else WSContentSend_P (PSTR ("<span id='humi'>--- %%</span>"));

  WSContentSend_P (PSTR ("</a></div>\n"));

  // graph
  SensorWebGraphWeeklyMeasureCurve (week, D_SENSOR_PAGE_WEEK_MEASURE);

  // main menu button
  WSContentSend_P (PSTR ("<div><form action='/' method='get'><button class='menu'>%s</button></form></div>\n"), D_MAIN_MENU);

  // end of page
  WSContentStop ();
}


// Sensor graph style
void SensorWebGraphYearlyMeasureStyle ()
{
  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));

  WSContentSend_P (PSTR ("div {margin:0.25rem auto;padding:0.1rem 0px;text-align:center;}\n"));
  WSContentSend_P (PSTR ("div.title {font-size:3.5vh;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.header {font-size:2.5vh;margin:2vh auto;}\n"));

  WSContentSend_P (PSTR ("a {color:white;}\n"));
  WSContentSend_P (PSTR ("a:link {text-decoration:none;}\n"));

  WSContentSend_P (PSTR ("span {font-size:4vh;padding:0vh 3vh;}\n"));
  WSContentSend_P (PSTR ("span#temp {color:%s;}\n"), SENSOR_COLOR_TEMP);

  // navigation buttons
  WSContentSend_P (PSTR ("button {padding:2px 12px;font-size:2.5vh;background:none;color:#fff;border:1px #666 solid;border-radius:6px;}\n"));
  WSContentSend_P (PSTR ("button:hover {background:#aaa;}\n"));
  WSContentSend_P (PSTR ("button:disabled {color:#252525;background:#252525;border:1px #252525 solid;}\n"));

  // banner
  WSContentSend_P (PSTR ("div.banner {width:88%%;margin:auto;}\n"));
  WSContentSend_P (PSTR ("div.banner div {display:inline-block;}\n"));
  WSContentSend_P (PSTR ("div.date {font-size:3vh;}\n"));
  WSContentSend_P (PSTR ("div.prev {float:left;}\n"));
  WSContentSend_P (PSTR ("div.next {float:right;}\n"));

  // main menu button
  WSContentSend_P (PSTR ("button.menu {background:%s;color:%s;line-height:2rem;font-size:1.2rem;cursor:pointer;border:none;border-radius:0.3rem;width:150px;margin:2vh;}\n"), COLOR_BUTTON, COLOR_BUTTON_TEXT);

  WSContentSend_P (PSTR("div.graph {width:100%%;margin:1vh auto;}\n"));
  WSContentSend_P (PSTR("svg.graph {width:100%%;height:50vh;}\n"));
}

// Sensor year graph curve display
void SensorWebGraphYearlyMeasureCurve (const uint16_t year, const char* pstr_url)
{
  uint8_t  month, month_start, month_stop, day, counter;
  int16_t  temp_min, temp_max, temp_range, temp_incr, scale;
  uint32_t index, unit, graph_width, graph_x, prev_x, graph_y, prev_y;
  char     str_text[16];

#ifdef USE_UFILESYS

  // load data from file
  SensorFileYearLoad (year);

  // start of form
  WSContentSend_P (PSTR ("<form method='get' action='/%s'>\n"), pstr_url);
  WSContentSend_P (PSTR ("<div class='banner'>\n"));

  // date
  WSContentSend_P (PSTR ("<div class='date'>%u</div>\n"), year);

  // if exist, navigation to previous year
  if (SensorFileYearExist (year - 1)) strcpy (str_text, ""); else strcpy (str_text, "disabled");
  WSContentSend_P (PSTR ("<div class='prev'><button name='year' title='%u' value=%u id='prev' %s>&lt;&lt;</button></div>\n"), year - 1, year - 1, str_text);

  // if exist, navigation to next year
  if (SensorFileYearExist (year + 1)) strcpy (str_text, ""); else strcpy (str_text, "disabled");
  WSContentSend_P (PSTR ("<div class='next'><button name='year' title='%u' value=%u id='next' %s>&gt;&gt;</button></div>\n"), year + 1, year + 1, str_text);

  // end of form
  WSContentSend_P (PSTR ("</div>\n"));      // banner
  WSContentSend_P (PSTR ("</form>\n"));     // get

  // set month window
  month_start = 0;
  month_stop  = 12;

#else

  // set month window
  month_start = RtcTime.month;
  month_stop  = month_start + 12;

#endif    // USE_UFILESYS

  // boundaries of SVG graph
  graph_width = SENSOR_GRAPH_STOP - SENSOR_GRAPH_START;

  // loop to calculate minimum and maximum temperature for the graph
  temp_min = temp_max = INT16_MAX;
  for (month = 0; month < 12; month ++)
    for (day = 0; day < 31; day++)
    {
      // minimum temperature
      if (temp_min == INT16_MAX) temp_min = sensor_year[month][day].temp_min;
        else if (sensor_year[month][day].temp_min != INT16_MAX) temp_min = min (temp_min, sensor_year[month][day].temp_min);

      // maximum temperature
      if (temp_max == INT16_MAX) temp_max = sensor_year[month][day].temp_max;
        else if (sensor_year[month][day].temp_max != INT16_MAX) temp_max = max (temp_max, sensor_year[month][day].temp_max);
    }

  // set upper and lower range according to min and max temperature
  if (temp_min != INT16_MAX) temp_min = (temp_min / 10 - 2) * 10;
    else temp_min = 180;
  if (temp_max != INT16_MAX) temp_max = (temp_max / 10 + 2) * 10;
    else temp_max = 220;

  // calculate temperature range and increment
  temp_range = temp_max - temp_min;
  temp_incr  = max (temp_range / 40 * 10, 10);

  // start of SVG graph
  WSContentSend_P (PSTR ("<div class='graph'>\n"));
  WSContentSend_P (PSTR ("<svg class='graph' viewBox='%d %d %d %d' preserveAspectRatio='none'>\n"), 0, 0, SENSOR_GRAPH_WIDTH, SENSOR_GRAPH_HEIGHT + 1);

  // SVG style
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));

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

  WSContentSend_P (PSTR ("</style>\n"));

  // --- Maximum temperature curve ---

  // loop thru months and days
  graph_x = prev_x = UINT32_MAX;
  graph_y = prev_y = UINT32_MAX;
  index = 0;
  for (counter = month_start; counter < month_stop; counter ++)
  {
    // loop thru days
    month = counter % 12;
    for (day = 0; day < days_in_month[month]; day ++)
    {
      // calculate x coordinate
      graph_x = SENSOR_GRAPH_START + (index * graph_width / 366);

      // calculate y coordinate
      if (sensor_year[month][day].temp_max == INT16_MAX) graph_y = UINT32_MAX;
        else graph_y = SENSOR_GRAPH_HEIGHT - SENSOR_GRAPH_HEIGHT * (uint32_t)(sensor_year[month][day].temp_max - temp_min) / (uint32_t)temp_range;

      // if curve restart from beginning
      if ((graph_x < prev_x) && (prev_x != UINT32_MAX)) WSContentSend_P (PSTR ("'/>\n<polyline class='max' points='"));

      // if needed, start polyline
      else if ((graph_y != UINT32_MAX) && (prev_y == UINT32_MAX)) WSContentSend_P (PSTR ("<polyline class='max' points='"));

      // else if needed, stop polyline
      else if ((graph_y == UINT32_MAX) && (prev_y != UINT32_MAX)) WSContentSend_P (PSTR ("'/>\n"));

      // if needed, draw point
      if (graph_y != UINT32_MAX) WSContentSend_P (PSTR ("%u,%u "), graph_x, graph_y);

      // save previous values
      prev_x = graph_x;
      prev_y = graph_y;
      index ++;
    }
  }
  if (graph_y != UINT32_MAX) WSContentSend_P (PSTR ("'/>\n"));

  // ---  Minimum temperature curve  ---

  // loop thru months and days
  graph_x = prev_x = UINT32_MAX;
  graph_y = prev_y = UINT32_MAX;
  index = 0;
  for (counter = month_start; counter < month_stop; counter ++)
  {
    // loop thru days
    month = counter % 12;
    for (day = 0; day < days_in_month[month]; day ++)
    {
      // calculate day in array and x coordinate
      graph_x = SENSOR_GRAPH_START + (index * graph_width / 366);

      // calculate y coordinate
      if (sensor_year[month][day].temp_min == INT16_MAX) graph_y = UINT32_MAX;
        else graph_y = SENSOR_GRAPH_HEIGHT - SENSOR_GRAPH_HEIGHT * (uint32_t)(sensor_year[month][day].temp_min - temp_min) / (uint32_t)temp_range;

      // if curve restart from beginning
      if ((graph_x < prev_x) && (prev_x != UINT32_MAX)) WSContentSend_P (PSTR ("'/>\n<polyline class='min' points='"));

      // if needed, start polyline
      else if ((graph_y != UINT32_MAX) && (prev_y == UINT32_MAX)) WSContentSend_P (PSTR ("<polyline class='min' points='"));

      // else if needed, stop polyline
      else if ((graph_y == UINT32_MAX) && (prev_y != UINT32_MAX)) WSContentSend_P (PSTR ("'/>\n"));

      // if needed, draw point
      if (graph_y != UINT32_MAX) WSContentSend_P (PSTR ("%u,%u "), graph_x, graph_y);

      // save previous values
      prev_x = graph_x;
      prev_y = graph_y;
      index ++;
    }
  }
  if (graph_y != UINT32_MAX) WSContentSend_P (PSTR ("'/>\n"));

  // ---  Frame  ---

  WSContentSend_P (PSTR ("<rect class='frame' x=%u y=%u width=%u height=%u rx=4 />\n"), SENSOR_GRAPH_START, 0, SENSOR_GRAPH_STOP - SENSOR_GRAPH_START, SENSOR_GRAPH_HEIGHT + 1);

  // ---  Time line  ---

  // display month names
  index = 15;
  for (counter = month_start; counter < month_stop; counter ++)
  {
    // get month name
    month = counter % 12;
    strlcpy (str_text, kMonthNames + month * 3, 4);

    // display month
    graph_x = SENSOR_GRAPH_START + (index * graph_width / 366);
    WSContentSend_P (PSTR ("<text class='month' text-anchor='middle' x=%u y=%u>%s</text>\n"), graph_x, 20, str_text);

    // increment to next position
    index += days_in_month[month];
  }

  // ---  Units  ---

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

  // ---  End  ---

  WSContentSend_P(PSTR ("</svg>\n"));      // graph
  WSContentSend_P(PSTR ("</div>\n"));      // graph
}

// Temperature yearly page
void SensorWebYearlyMeasure ()
{
  uint16_t year;
  char     str_value[8];

  // if target temperature has been changed
  if (Webserver->hasArg ("year"))
  {
    WebGetArg ("year", str_value, sizeof(str_value));
    if (strlen(str_value) > 0) year = atoi (str_value);
  }
  else year = RtcTime.year;

  // set page label
  WSContentStart_P (D_SENSOR_TEMPERATURE, false);
  WSContentSend_P (PSTR ("</script>\n"));

  // set page as scalable
  WSContentSend_P (PSTR ("<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=yes'/>\n"));

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  SensorWebGraphYearlyMeasureStyle ();
  WSContentSend_P (PSTR ("</style>\n"));

  WSContentSend_P (PSTR ("</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  // room name and header
  WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText(SET_DEVICENAME));
  WSContentSend_P (PSTR ("<div class='header'>Yearly %s</div>\n"), D_SENSOR_TEMPERATURE);

  // ------- Values --------

  // temperature
  WSContentSend_P (PSTR ("<div class='value'><a href='/'>"));
  if ((sensor_status.temp.source != SENSOR_SOURCE_NONE) && (sensor_status.temp.value != INT16_MAX)) WSContentSend_P (PSTR ("<span id='temp'>%d.%d °C</span>"), sensor_status.temp.value / 10, sensor_status.temp.value % 10);
  WSContentSend_P (PSTR ("</a></div>\n"));

  // ------- Graph --------

  SensorWebGraphYearlyMeasureCurve (year, D_SENSOR_PAGE_YEAR_MEASURE);

  // main menu button
  WSContentSend_P (PSTR ("<div><form action='/' method='get'><button class='menu'>%s</button></form></div>\n"), D_MAIN_MENU);

  // end of page
  WSContentStop ();
}

// --------------------
//    Presence Graph
// --------------------

// Presence page style
void SensorWebGraphPresenceStyle ()
{
  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));

  WSContentSend_P (PSTR ("div {margin:0.25rem auto;padding:0.1rem 0px;text-align:center;}\n"));
  WSContentSend_P (PSTR ("div.title {font-size:3.5vh;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.header {font-size:2.5vh;margin:2vh auto;}\n"));

  WSContentSend_P (PSTR ("a {color:white;}\n")); 
  WSContentSend_P (PSTR ("a:link {text-decoration:none;}\n"));

  // navigation button
  WSContentSend_P (PSTR ("button {padding:2px 12px;font-size:2.5vh;background:none;color:#fff;border:1px #666 solid;border-radius:6px;}\n"));
  WSContentSend_P (PSTR ("button:hover {background:#aaa;}\n"));
  WSContentSend_P (PSTR ("button:disabled {color:#252525;background:#252525;border:1px #252525 solid;}\n"));

  // banner
  WSContentSend_P (PSTR ("div.banner {width:88%%;}\n"));
  WSContentSend_P (PSTR ("div.banner div {display:inline-block;}\n"));
  WSContentSend_P (PSTR ("div.date {font-size:2.5vh;}\n"));
  WSContentSend_P (PSTR ("div.prev {float:left;}\n"));
  WSContentSend_P (PSTR ("div.next {float:right;}\n"));

  // main menu button
  WSContentSend_P (PSTR ("button.menu {background:%s;color:%s;line-height:2rem;font-size:1.2rem;cursor:pointer;border:none;border-radius:0.3rem;width:150px;margin:2vh;}\n"), COLOR_BUTTON, COLOR_BUTTON_TEXT);

  WSContentSend_P (PSTR("div.graph {width:90%%;margin:2vh auto;}\n"));
  WSContentSend_P (PSTR("svg.graph {width:100%%;height:70vh;}\n"));
}

// Presence SVG style
void SensorWebGraphPresenceSVGStyle ()
{
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));

  WSContentSend_P (PSTR ("text {font-size:3vh;fill:%s;}\n"), SENSOR_COLOR_TIME);
  WSContentSend_P (PSTR ("text.time {}\n"));
  WSContentSend_P (PSTR ("text.head {}\n"));
  WSContentSend_P (PSTR ("text.today {font-weight:bold;}\n"));

  WSContentSend_P (PSTR ("rect {opacity:0.8;}\n"));
  WSContentSend_P (PSTR ("rect.s0 {stroke:none;div.graph {width:96%%;}fill:%s;}\n"), "#333");
  WSContentSend_P (PSTR ("rect.s1 {stroke:none;fill:%s;}\n"), SENSOR_COLOR_PRES);

  WSContentSend_P (PSTR ("line {stroke:#aaa;stroke-dasharray:2 6;}\n"));

  WSContentSend_P (PSTR ("</style>\n"));
}

// Presence history page
void SensorWebGraphWeeklyPresenceCurve (const uint8_t week, const char* pstr_url)
{
  uint8_t  slot_state, slot_new;
  uint8_t  prev_week, next_week;
  uint32_t index, day, hour, slot;
  uint32_t count, count_start, count_stop;
  uint32_t last_x, graph_x, graph_y, line_height, bar_height;
  char     str_text[16];
  char     str_style[16];

  // check parameters
  if (pstr_url == nullptr) return;

#ifdef USE_UFILESYS
  // calculate previous and next week
  if (week > 0) next_week = week - 1; else next_week = UINT8_MAX;
  prev_week = week + 1;

  // start of form
  WSContentSend_P (PSTR("<form method='get' action='/%s'>\n"), pstr_url);
  WSContentSend_P (PSTR("<div class='banner'>\n"));

  // date
  SensorGetWeekLabel (week, str_text, sizeof (str_text));
  WSContentSend_P (PSTR ("<div class='date'>%s</div>\n"), str_text);

  // if exist, navigation to previous week
  SensorGetWeekLabel (prev_week, str_text, sizeof (str_text));
  if (SensorFileWeekExist (prev_week)) strcpy (str_style, ""); else strcpy (str_style, "disabled");
  WSContentSend_P (PSTR ("<div class='prev'><button name='week' title='%s' value=%u id='prev' %s>&lt;&lt;</button></div>\n"), str_text, prev_week, str_style);

  // if exist, navigation to next week
  SensorGetWeekLabel (next_week, str_text, sizeof (str_text));
  if (SensorFileWeekExist (next_week)) strcpy (str_style, ""); else strcpy (str_style, "disabled");
  WSContentSend_P (PSTR ("<div class='next'><button name='week' title='%s' value=%u id='next' %s>&gt;&gt;</button></div>\n"), str_text, next_week, str_style);

  // end of form
  WSContentSend_P (PSTR ("</div>\n"));     // banner
  WSContentSend_P (PSTR ("<form method='get' action='/%s'>\n"), D_SENSOR_PAGE_WEEK_PRESENCE);

  // load weekly data
  SensorFileWeekLoad (week);

  // set display sequence
  count_start = 1;
  if (week != 0) count_stop = 8;                            // previous weeks
    else if (RtcTime.day_of_week == 1) count_stop = 8;      // current week on sunday
    else count_stop = RtcTime.day_of_week;                  // current week other days
#else
  // set display sequence
  count_start = RtcTime.day_of_week;
  count_stop = count_start + 7;
#endif      // USE_UFILESYS

  // calculate line height and bar height
  line_height = SENSOR_GRAPH_HEIGHT / 8;
  bar_height  = line_height - 5;

  // start of SVG graph
  WSContentSend_P (PSTR ("<div class='graph'>\n"));
  WSContentSend_P (PSTR ("<svg class='graph' viewBox='%d %d %d %d' preserveAspectRatio='none'>\n"), 0, 0, SENSOR_GRAPH_WIDTH, SENSOR_GRAPH_HEIGHT + 1);

  // SVG style
  SensorWebGraphPresenceSVGStyle ();

  // display header
  WSContentSend_P (PSTR ("<text class='time' text-anchor='start' x=%u y=%u>%s</text>\n"), SENSOR_GRAPH_WIDTH_HEAD, bar_height - 10, "0h");
  for (index = 1; index < 6;index ++) WSContentSend_P (PSTR ("<text class='time' text-anchor='middle' x=%u y=%u>%02uh</text>\n"), SENSOR_GRAPH_WIDTH_HEAD + index * SENSOR_GRAPH_WIDTH_LINE / 6, bar_height - 10, index * 4);
  WSContentSend_P (PSTR ("<text class='time' text-anchor='end' x=%u y=%u>%s</text>\n"), SENSOR_GRAPH_WIDTH_HEAD + SENSOR_GRAPH_WIDTH_LINE, bar_height - 10, "24h");

  // loop thru days
  index = 1;
  for (count = count_start; count < count_stop; count++)
  {
    // convert to current day
    day = count % 7;

    // get upper line position
    graph_y = index * line_height;

    // display week day
    strlcpy (str_text, kWeekdayNames + day * 3, 4);
    if ((week == 0) && (day == RtcTime.day_of_week - 1)) strcpy (str_style, "today"); else strcpy (str_style, "head");
    WSContentSend_P (PSTR ("<text class='%s' text-anchor='start' x=%u y=%u>%s</text>\n"), str_style, 20, graph_y + bar_height - 25, str_text);

    // loop to display slots of current day
    slot_state = sensor_week[day][0][0].event.pres;
    last_x = SENSOR_GRAPH_WIDTH_HEAD;
    for (hour = 0; hour < 24; hour++)
      for (slot = 0; slot < 6; slot++)
      {
        // read slot status
        slot_new = sensor_week[day][hour][slot].event.pres;

        // last cell
        if ((hour == 23) && (slot == 5))
        {
          graph_x = SENSOR_GRAPH_WIDTH_HEAD + SENSOR_GRAPH_WIDTH_LINE;
          WSContentSend_P (PSTR ("<rect class='s%u' x=%u y=%u width=%u height=%u />\n"), slot_state, last_x, graph_y, graph_x - last_x, bar_height);
        }

        // else if presence has changed
        else if (slot_state != slot_new)
        {
          graph_x = SENSOR_GRAPH_WIDTH_HEAD + SENSOR_GRAPH_WIDTH_LINE * (6 * hour + slot) / 144;
          WSContentSend_P (PSTR ("<rect class='s%u' x=%u y=%u width=%u height=%u />\n"), slot_state, last_x, graph_y, graph_x - last_x, bar_height);
          last_x = graph_x;
          slot_state = slot_new;
        }
      }

    // loop to display separation lines
    last_x = SENSOR_GRAPH_WIDTH_HEAD;
    for (hour = 1; hour < 12; hour++)
    {
      // draw line
      graph_x = SENSOR_GRAPH_WIDTH_HEAD + SENSOR_GRAPH_WIDTH_LINE * hour / 12;
      WSContentSend_P (PSTR ("<line x1=%u y1=%u x2=%u y2=%u />\n"), graph_x, graph_y, graph_x, graph_y + bar_height);
    }

    // increment
    index++;
  }

  // ---  End  ---
  WSContentSend_P (PSTR ("</svg>\n"));      // graph
  WSContentSend_P (PSTR ("</div>\n"));      // graph
}

// Presence weekly history page
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
  else week = 0;

  // beginning page without authentification
  WSContentStart_P (PSTR ("Presence history"), false);

  // graph swipe section script
  SensorWebGraphSwipe ();
  WSContentSend_P (PSTR ("</script>\n"));

  // set page as scalable
  WSContentSend_P (PSTR ("<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=yes'/>\n"));

  // refresh every 10 mn
  WSContentSend_P (PSTR ("<meta http-equiv='refresh' content='%u'/>\n"), 600);

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  SensorWebGraphPresenceStyle ();
  WSContentSend_P (PSTR ("</style>\n"));

  WSContentSend_P (PSTR ("</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  // room name and header
  WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText(SET_DEVICENAME));
  WSContentSend_P (PSTR ("<div class='header'>Presence</div>\n"));

  // ---------------
  //     Graph
  // ---------------

  SensorWebGraphWeeklyPresenceCurve (week, D_SENSOR_PAGE_WEEK_PRESENCE);

  // main menu button
  WSContentSend_P (PSTR ("<div><form action='/' method='get'><button class='menu'>%s</button></form></div>\n"), D_MAIN_MENU);

  // end of page
  WSContentStop ();
}


// Presence yearly history page
void SensorWebGraphYearlyPresenceCurve (const uint16_t year, const char* pstr_url)
{
  uint8_t  slot_state, slot_new;
  uint32_t month, month_start, month_stop, day;
  uint32_t index, count;
  uint32_t last_x, graph_x, graph_y, line_height, bar_height;
  char     str_name[4];
  char     str_text[64];
  String   str_result;

  // check parameters
  if (pstr_url == nullptr) return;

#ifdef USE_UFILESYS
  // start of form
  WSContentSend_P (PSTR("<form method='get' action='/%s'>\n"), pstr_url);
  WSContentSend_P (PSTR("<div class='banner'>\n"));

  // date
  WSContentSend_P (PSTR("<div class='date'>%u</div>\n"), year);

  // if exist, navigation to previous year
  if (SensorFileYearExist (year - 1)) strcpy (str_text, ""); else strcpy (str_text, "disabled");
  WSContentSend_P (PSTR("<div class='prev'><button name='year' title='%u' value=%u id='prev' %s>&lt;&lt;</button></div>\n"), year - 1, year - 1, str_text);

  // if exist, navigation to next year
  if (SensorFileYearExist (year + 1)) strcpy (str_text, ""); else strcpy (str_text, "disabled");
  WSContentSend_P (PSTR("<div class='next'><button name='year' title='%u' value=%u id='next' %s>&gt;&gt;</button></div>\n"), year + 1, year + 1, str_text);

  // end of form
  WSContentSend_P (PSTR("</div>\n"));     // banner
  WSContentSend_P (PSTR("<form method='get' action='/%s'>\n"), D_SENSOR_PAGE_YEAR_PRESENCE);

  // load data from file
  SensorFileYearLoad (year);

  // set month window
  month_start = 0;
  if (year != RtcTime.year) month_stop = 12;          // previous years
    else month_stop = RtcTime.month;                  // current year
#else
  // set month window
  month_start = RtcTime.month;
  month_stop  = month_start + 12;
#endif    // USE_UFILESYS

  // calculate line height and bar height
  line_height = SENSOR_GRAPH_HEIGHT / 13;
  bar_height  = line_height - 5;

  // start of SVG graph
  WSContentSend_P (PSTR("<div class='graph'>\n"));
  WSContentSend_P (PSTR("<svg class='graph' viewBox='%d %d %d %d' preserveAspectRatio='none'>\n"), 0, 0, SENSOR_GRAPH_WIDTH, SENSOR_GRAPH_HEIGHT + 1);

  // SVG style
  SensorWebGraphPresenceSVGStyle ();

  // display header
  WSContentSend_P (PSTR ("<text class='time' text-anchor='start' x=%u y=%u>%s</text>\n"), SENSOR_GRAPH_WIDTH_HEAD, line_height - 10, "01");
  for (index = 1; index < 7;index ++) WSContentSend_P (PSTR ("<text class='time' text-anchor='middle' x=%u y=%u>%02u</text>\n"), SENSOR_GRAPH_WIDTH_HEAD + 5 * index * SENSOR_GRAPH_WIDTH_LINE / 31, line_height - 10, index * 5);

  // loop thru days
  str_result = "";
  index = 1;
  for (count = month_start; count < month_stop; count ++)
  {
    // convert to current month
    month = count % 12;

    // get upper line position
    graph_y = index * line_height;

    // month name
    strlcpy (str_name, kMonthNames + month * 3, 4);
    sprintf_P (str_text, PSTR ("<text class='head' text-anchor='start' x=%u y=%u>%s</text>\n"), 20, graph_y + bar_height - 15, str_name);
    str_result += str_text;

    // loop to display slots of days
    last_x = SENSOR_GRAPH_WIDTH_HEAD;
    if (sensor_year[month][0].pres > 0) slot_state = 1; else slot_state = 0;
    for (day = 0; day < days_in_month[month]; day ++)
    {
      // read slot status
      if (sensor_year[month][day].pres > 0) slot_new = 1; else slot_new = 0;

      // last cell
      if (day == days_in_month[month] - 1)
      {
        graph_x = SENSOR_GRAPH_WIDTH_HEAD + SENSOR_GRAPH_WIDTH_LINE * (day + 1) / 31;
        sprintf_P (str_text, PSTR ("<rect class='s%u' x=%u y=%u width=%u height=%u />\n"), slot_state, last_x, graph_y, graph_x - last_x, bar_height);
        str_result += str_text;
      }

      // else if presence has changed
      else if (slot_state != slot_new)
      {
        graph_x = SENSOR_GRAPH_WIDTH_HEAD + SENSOR_GRAPH_WIDTH_LINE * day / 31;
        sprintf_P (str_text, PSTR ("<rect class='s%u' x=%u y=%u width=%u height=%u />\n"), slot_state, last_x, graph_y, graph_x - last_x, bar_height);
        str_result += str_text;
        last_x = graph_x;
        slot_state = slot_new;
      }

      // if needed, publish result
      if (str_result.length () > 256) { WSContentSend_P (str_result.c_str ()); str_result = ""; }
    }

    // loop to display separation lines
    for (day = 1; day < 7; day++)
    {
      // draw line
      graph_x = SENSOR_GRAPH_WIDTH_HEAD + SENSOR_GRAPH_WIDTH_LINE * 5 * day / 31;
      sprintf_P (str_text, PSTR ("<line x1=%u y1=%u x2=%u y2=%u />\n"), graph_x, graph_y, graph_x, graph_y + bar_height);
      str_result += str_text;

      // if needed, publish result
      if (str_result.length () > 256) { WSContentSend_P (str_result.c_str ()); str_result = ""; }
    }

    // increment
    index++;
  }

  // publish last part
  if (str_result.length () > 0) WSContentSend_P (str_result.c_str ());

  // ---  End  ---
  WSContentSend_P (PSTR("</svg>\n"));      // graph
  WSContentSend_P (PSTR("</div>\n"));      // graph
}

// Presence yearly history page
void SensorWebYearlyPresence ()
{
  uint16_t year;
  char     str_value[8];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // if target temperature has been changed
  if (Webserver->hasArg ("year"))
  {
    WebGetArg ("year", str_value, sizeof(str_value));
    if (strlen(str_value) > 0) year = atoi (str_value);
  }
  else year = RtcTime.year;

  // beginning page without authentification
  WSContentStart_P (PSTR ("Presence history"), false);

  // graph swipe section script
  SensorWebGraphSwipe ();
  WSContentSend_P (PSTR ("</script>\n"));

  // set page as scalable
  WSContentSend_P (PSTR ("<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=yes'/>\n"));

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  SensorWebGraphPresenceStyle ();
  WSContentSend_P (PSTR ("</style>\n"));

  WSContentSend_P (PSTR ("</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  // room name and header
  WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText(SET_DEVICENAME));
  WSContentSend_P (PSTR ("<div class='header'>Presence</div>\n"));

  // ---------------
  //     Graph
  // ---------------

  SensorWebGraphYearlyPresenceCurve (year, D_SENSOR_PAGE_YEAR_PRESENCE);

  // main menu button
  WSContentSend_P (PSTR ("<div><form action='/' method='get'><button class='menu'>%s</button></form></div>\n"), D_MAIN_MENU);

  // end of page
  WSContentStop ();
}

#endif      // USE_WEBSERVER

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xsns98 (uint32_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
   case FUNC_INIT:
      SensorLoadConfig ();    // load configuration
      SensorInit ();          // init variables
      SensorLoadData ();      // load data from settings
      break;

    case FUNC_COMMAND:
      result  = DecodeCommand (kSensorCommands, SensorCommand);
      result |= DecodeCommand (kTempCommands,   TempCommand);
      result |= DecodeCommand (kHumiCommands,   HumiCommand);
      result |= DecodeCommand (kPresCommands,   PresCommand);
      result |= DecodeCommand (kActiCommands,   ActiCommand);
      result |= DecodeCommand (kInacCommands,   InacCommand);
      break;

    case FUNC_SAVE_BEFORE_RESTART:
      SensorSaveConfig ();    // save configuration
      SensorSaveData ();      // save full data
      break;

    case FUNC_EVERY_SECOND:
      if (RtcTime.valid) SensorEverySecond ();
      break;

    case FUNC_JSON_APPEND:
      SensorShowJSON (true);
      break;
  }
  
  return result;
}

bool Xdrv98 (uint32_t function)
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
      Webserver->on ("/" D_SENSOR_PAGE_CONFIG,        SensorWebConfigure);
      Webserver->on ("/" D_SENSOR_PAGE_WEEK_MEASURE,  SensorWebWeeklyMeasure);
      Webserver->on ("/" D_SENSOR_PAGE_YEAR_MEASURE,  SensorWebYearlyMeasure);
#ifdef USE_GENERIC_PRESENCE
      Webserver->on ("/" D_SENSOR_PAGE_WEEK_PRESENCE, SensorWebWeeklyPresence);
      Webserver->on ("/" D_SENSOR_PAGE_YEAR_PRESENCE, SensorWebYearlyPresence);
#endif    // USE_GENERIC_PRESENCE
      break;

    case FUNC_WEB_ADD_MAIN_BUTTON:
      if (sensor_config.weekly_temp || sensor_config.weekly_humi) WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>Weekly Measure</button></form></p>\n"), D_SENSOR_PAGE_WEEK_MEASURE);
      if (sensor_config.yearly_temp) WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>Yearly Measure</button></form></p>\n"), D_SENSOR_PAGE_YEAR_MEASURE);
#ifdef USE_GENERIC_PRESENCE
      if (sensor_config.weekly_pres) WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>Weekly Presence</button></form></p>\n"), D_SENSOR_PAGE_WEEK_PRESENCE);
      if (sensor_config.yearly_pres) WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>Yearly Presence</button></form></p>\n"), D_SENSOR_PAGE_YEAR_PRESENCE);
#endif    // USE_GENERIC_PRESENCE
      break;
#endif      // USE_WEBSERVER
  }
  return result;
}

#endif      // USE_GENERIC_SENSOR
