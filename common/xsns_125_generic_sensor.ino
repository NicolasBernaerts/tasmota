/*
  xsns_125_sensor.ino - Generic temperature, humidity and movement collector
    Reading is possible from local or remote (thru MQTT topic/key) sensors.

  Copyright (C) 2020  Nicolas Bernaerts
    15/01/2020 - v1.0 - Creation with management of remote MQTT sensor
    23/04/2021 - v1.1 - Remove use of String to avoid heap fragmentation
    18/06/2021 - v1.2 - Bug fixes
    15/11/2021 - v1.3 - Add LittleFS management
    19/11/2021 - v1.4 - Handle drift and priority between local and remote sensors
    01/03/2022 - v2.0 - Complete rewrite to handle local drift and priority between local and remote sensors
    26/06/2022 - v3.0 - Tasmota 12 compatibility
                        Management of HLK-LD11xx sensors
                      
  If no littleFS partition is available, string settings are stored using SettingsTextIndex :
    * SET_SENSOR_TEMP_TOPIC
    * SET_SENSOR_TEMP_KEY
    * SET_SENSOR_HUMI_TOPIC
    * SET_SENSOR_HUMI_KEY
    * SET_SENSOR_MOVE_TOPIC
    * SET_SENSOR_MOVE_KEY

  These keys should be added in tasmota.h at the end of "enum SettingsTextIndex" section just before SET_MAX :

	#ifndef USE_UFILESYS
		SET_SENSOR_TEMP_TOPIC, SET_SENSOR_TEMP_KEY,
		SET_SENSOR_HUMI_TOPIC, SET_SENSOR_HUMI_KEY,
		SET_SENSOR_MOVE_TOPIC, SET_SENSOR_MOVE_KEY,
	#endif 	// USE_UFILESYS

  Configuration values are stored in :
    - Settings.weight_item            : temperature validity timeout
    - Settings.weight_reference       : humidity validity timeout
    - Settings.weight_calibration     : presence validity timeout

  If LittleFS partition is available, all settings are stored in sensor.cfg

  To use the remote sensors, you need to add these function calls in any one of your xdrv_ file :
    FUNC_MQTT_SUBSCRIBE: SensorMqttSubscribe ()
    FUNC_MQTT_DATA:      SensorMqttData ()

  Handled local sensor are :
    * Temperature = DHT11, AM2301, SI7021 or DS18x20
    * Humidity    = DHT11, AM2301, SI7021
    * Presence    = Generic presence/movement detector declared as Input 1
                    HLK-LD1115 or HLK-LD1125 connected thru serial port (Rx and Tx)
    
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

#ifndef FIRMWARE_SAFEBOOT
#ifdef USE_GENERIC_SENSOR

#define XSNS_125                      125

#include <ArduinoJson.h>

#define SENSOR_TIMEOUT_TEMPERATURE    30            // temperature default validity (in sec.)
#define SENSOR_TIMEOUT_HUMIDITY       30            // humidity default validity (in sec.)
#define SENSOR_TIMEOUT_PRESENCE       30            // presence default validity (in sec.)

#define SENSOR_TEMP_DRIFT_MAX         10
#define SENSOR_TEMP_DRIFT_STEP        0.1

#define SENSOR_MOVEMENT_INDEX         0             // counter index for movement detection (0=input1/counter1 ... 7=input7/counter8)

#define D_PAGE_SENSOR                 "sensor"

#define D_CMND_SENSOR_PREFIX          "sn_"
#define D_CMND_SENSOR_HELP            "help"
#define D_CMND_SENSOR_TEMP_VALUE      "tval"
#define D_CMND_SENSOR_TEMP_DRIFT      "tdrift"
#define D_CMND_SENSOR_TEMP_TOPIC      "ttopic"
#define D_CMND_SENSOR_TEMP_KEY        "tkey"
#define D_CMND_SENSOR_TEMP_TIMEOUT    "ttime"
#define D_CMND_SENSOR_HUMI_VALUE      "hval"
#define D_CMND_SENSOR_HUMI_TOPIC      "htopic"
#define D_CMND_SENSOR_HUMI_KEY        "hkey"
#define D_CMND_SENSOR_HUMI_TIMEOUT    "htime"
#define D_CMND_SENSOR_PRES_VALUE      "pval"
#define D_CMND_SENSOR_PRES_TOPIC      "ptopic"
#define D_CMND_SENSOR_PRES_KEY        "pkey"
#define D_CMND_SENSOR_PRES_TIMEOUT    "ptime"

#define D_SENSOR                      "Sensor"
#define D_SENSOR_CONFIGURE            "Configure"
#define D_SENSOR_TEMPERATURE          "Temperature"
#define D_SENSOR_HUMIDITY             "Humidity"
#define D_SENSOR_TIMEOUT              "Data validity"
#define D_SENSOR_PRESENCE             "Presence"
#define D_SENSOR_CORRECTION           "Correction"
#define D_SENSOR_TOPIC                "Topic"
#define D_SENSOR_KEY                  "JSON Key"

#define D_SENSOR_LOCAL                "Local"
#define D_SENSOR_REMOTE               "Remote"

#ifdef USE_UFILESYS
#define D_SENSOR_FILE_CFG             "/sensor.cfg"
#endif     // USE_UFILESYS

// sensor type
enum SensorType { SENSOR_TYPE_TEMP, SENSOR_TYPE_HUMI, SENSOR_TYPE_PRES, SENSOR_TYPE_MAX };

// remote sensor sources
enum SensorSource { SENSOR_SOURCE_DSB, SENSOR_SOURCE_DHT, SENSOR_SOURCE_INPUT, SENSOR_SOURCE_SERIAL, SENSOR_SOURCE_REMOTE, SENSOR_SOURCE_MAX };
const char kSensorSource[] PROGMEM = "Local|Local|Local|Local|Local|Remote|Undef.";                                                           // device source labels

// remote sensor commands
const char kSensorCommands[] PROGMEM = D_CMND_SENSOR_PREFIX "|" D_CMND_SENSOR_HELP "|" D_CMND_SENSOR_TEMP_TOPIC "|" D_CMND_SENSOR_TEMP_KEY "|" D_CMND_SENSOR_TEMP_VALUE "|" D_CMND_SENSOR_TEMP_DRIFT "|" D_CMND_SENSOR_HUMI_TOPIC "|" D_CMND_SENSOR_HUMI_KEY "|" D_CMND_SENSOR_HUMI_VALUE "|" D_CMND_SENSOR_PRES_TOPIC "|" D_CMND_SENSOR_PRES_KEY "|" D_CMND_SENSOR_PRES_VALUE;
void (* const SensorCommand[])(void) PROGMEM = { &CmndSensorHelp, &CmndSensorTemperatureTopic, &CmndSensorTemperatureKey, &CmndSensorTemperatureValue, &CmndSensorTemperatureDrift, &CmndSensorHumidityTopic, &CmndSensorHumidityKey, &CmndSensorHumidityValue, &CmndSensorPresenceTopic, &CmndSensorPresenceKey, &CmndSensorPresenceValue };

// form topic style
const char SENSOR_FIELDSET_START[] PROGMEM = "<p><fieldset><legend><b>&nbsp;%s %s&nbsp;</b></legend>\n";
const char SENSOR_FIELDSET_STOP[]  PROGMEM = "</fieldset></p>\n";
const char SENSOR_FIELD_INPUT[]    PROGMEM = "<p>%s<span class='key'>%s</span><br><input name='%s' value='%s'></p>\n";
const char SENSOR_FIELD_CONFIG[]   PROGMEM = "<p>%s (%s)<span class='key'>%s</span><br><input type='number' name='%s' min='%d' max='%d' step='%s' value='%s'></p>\n";

/*************************************************\
 *               Variables
\*************************************************/

// configuration
struct sensor_mqtt
{
  uint16_t timeout;                               // data publication timeout
  String   topic;                                 // remote sensor topic
  String   key;                                   // remote sensor key
};
sensor_mqtt sensor_config[SENSOR_TYPE_MAX];       // remote sensors configuration

// status
struct sensor_data
{
  float    value;                                 // current sensor value
  uint8_t  source;                                // source of data (local sensor type)
  uint32_t delay;                                 // delay since last update
};
struct {
  bool     publish_json     = false;              // flag to publish JSON
  uint32_t presence_counter = 0;                  // presence detection counter
  sensor_data type[SENSOR_TYPE_MAX];              // sensor types
} sensor_status;

/**************************************************\
 *                  Accessors
\**************************************************/

void SensorInit () 
{
  uint8_t  sensor;
  uint32_t index = UINT32_MAX;

  // init remote sensor values
  for (sensor = 0; sensor < SENSOR_TYPE_MAX; sensor ++)
  {
    sensor_status.type[sensor].source = SENSOR_SOURCE_MAX;
    sensor_status.type[sensor].value  = NAN;
    sensor_status.type[sensor].delay  = UINT32_MAX;
  }

  // check for DHT11 sensor
  if (PinUsed (GPIO_DHT11))
  {
    sensor_status.type[SENSOR_TYPE_TEMP].source = SENSOR_SOURCE_DHT;
    sensor_status.type[SENSOR_TYPE_HUMI].source = SENSOR_SOURCE_DHT;
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: %s temperature & humidity sensor detected"), "DHT11");
  }

  // check for DHT22 sensor
  else if (PinUsed (GPIO_DHT22))
  {
    sensor_status.type[SENSOR_TYPE_TEMP].source = SENSOR_SOURCE_DHT;
    sensor_status.type[SENSOR_TYPE_HUMI].source = SENSOR_SOURCE_DHT;
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: %s temperature & humidity sensor detected"), "DHT22");
  }

  // check for SI7021 sensor
  else if (PinUsed (GPIO_SI7021))
  {
    sensor_status.type[SENSOR_TYPE_TEMP].source = SENSOR_SOURCE_DHT;
    sensor_status.type[SENSOR_TYPE_HUMI].source = SENSOR_SOURCE_DHT;
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: %s temperature & humidity sensor detected"), "DHT22");
  }

  // check for DS18B20 sensor
  else if (PinUsed (GPIO_DSB))
  {
    sensor_status.type[SENSOR_TYPE_TEMP].source = SENSOR_SOURCE_DSB;
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: %s temperature sensor detected"), "DS18B20");

    // force pullup for single DS18B20
    Settings->flag3.ds18x20_internal_pullup = 1;
  }

  // presence : check for generic sensor
  if (PinUsed (GPIO_INPUT, SENSOR_MOVEMENT_INDEX))
  {
    sensor_status.type[SENSOR_TYPE_PRES].source = SENSOR_SOURCE_INPUT;
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: %s presence sensor detected"), "Generic");
  }

  // movement : HLK-LD11xx sensor
#ifdef USE_HLKLD11
  else if (PinUsed (GPIO_TXD) && PinUsed (GPIO_RXD))
  {
    // init HLK-LD11xx device
    if (HLKLDInitDevice (index, HLKLD_DEFAULT_TIMEOUT))
    {
      sensor_status.type[SENSOR_TYPE_PRES].source = SENSOR_SOURCE_SERIAL;
      AddLog (LOG_LEVEL_INFO, PSTR ("SEN: %s presence sensor detected"), "HLK-LD");
    }
  }
#endif    // USE_HLKLD11

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: sn_help to get help on Sensor commands"));

  // load configuration
  SensorLoadConfig ();
}

// load configuration parameters
void SensorLoadConfig () 
{
  char str_text[128];

#ifdef USE_UFILESYS

  // retrieve saved settings from flash filesystem
  sensor_config[SENSOR_TYPE_TEMP].topic   = UfsCfgLoadKey    (D_SENSOR_FILE_CFG, D_CMND_SENSOR_TEMP_TOPIC);
  sensor_config[SENSOR_TYPE_TEMP].key     = UfsCfgLoadKey    (D_SENSOR_FILE_CFG, D_CMND_SENSOR_TEMP_KEY);
  sensor_config[SENSOR_TYPE_HUMI].topic   = UfsCfgLoadKey    (D_SENSOR_FILE_CFG, D_CMND_SENSOR_HUMI_TOPIC);
  sensor_config[SENSOR_TYPE_HUMI].key     = UfsCfgLoadKey    (D_SENSOR_FILE_CFG, D_CMND_SENSOR_HUMI_KEY);
  sensor_config[SENSOR_TYPE_PRES].topic   = UfsCfgLoadKey    (D_SENSOR_FILE_CFG, D_CMND_SENSOR_PRES_TOPIC);
  sensor_config[SENSOR_TYPE_PRES].key     = UfsCfgLoadKey    (D_SENSOR_FILE_CFG, D_CMND_SENSOR_PRES_KEY);

  sensor_config[SENSOR_TYPE_TEMP].timeout = UfsCfgLoadKeyInt (D_SENSOR_FILE_CFG, D_CMND_SENSOR_TEMP_TIMEOUT, SENSOR_TIMEOUT_TEMPERATURE);
  sensor_config[SENSOR_TYPE_HUMI].timeout = UfsCfgLoadKeyInt (D_SENSOR_FILE_CFG, D_CMND_SENSOR_HUMI_TIMEOUT, SENSOR_TIMEOUT_HUMIDITY);
  sensor_config[SENSOR_TYPE_PRES].timeout = UfsCfgLoadKeyInt (D_SENSOR_FILE_CFG, D_CMND_SENSOR_PRES_TIMEOUT, SENSOR_TIMEOUT_PRESENCE);

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Config from LittleFS"));

#else       // No LittleFS

  // retrieve saved settings from flash memory
  sensor_config[SENSOR_TYPE_TEMP].topic = SettingsText (SET_SENSOR_TEMP_TOPIC);
  sensor_config[SENSOR_TYPE_TEMP].key   = SettingsText (SET_SENSOR_TEMP_KEY);
  sensor_config[SENSOR_TYPE_HUMI].topic = SettingsText (SET_SENSOR_HUMI_TOPIC);
  sensor_config[SENSOR_TYPE_HUMI].key   = SettingsText (SET_SENSOR_HUMI_KEY);
  sensor_config[SENSOR_TYPE_PRES].topic = SettingsText (SET_SENSOR_MOVE_TOPIC);
  sensor_config[SENSOR_TYPE_PRES].key   = SettingsText (SET_SENSOR_MOVE_KEY);

  sensor_config[SENSOR_TYPE_TEMP].timeout = Settings->weight_item;
  sensor_config[SENSOR_TYPE_HUMI].timeout = Settings->weight_reference;
  sensor_config[SENSOR_TYPE_PRES].timeout = Settings->weight_calibration;

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Config from Settings"));

# endif     // USE_UFILESYS

  // check parameters validity
  if (sensor_config[SENSOR_TYPE_TEMP].timeout == 0)          sensor_config[SENSOR_TYPE_TEMP].timeout = SENSOR_TIMEOUT_TEMPERATURE;
  if (sensor_config[SENSOR_TYPE_TEMP].timeout == UINT32_MAX) sensor_config[SENSOR_TYPE_TEMP].timeout = SENSOR_TIMEOUT_TEMPERATURE;
  if (sensor_config[SENSOR_TYPE_HUMI].timeout == 0)          sensor_config[SENSOR_TYPE_HUMI].timeout = SENSOR_TIMEOUT_HUMIDITY;
  if (sensor_config[SENSOR_TYPE_HUMI].timeout == UINT32_MAX) sensor_config[SENSOR_TYPE_HUMI].timeout = SENSOR_TIMEOUT_HUMIDITY;
  if (sensor_config[SENSOR_TYPE_PRES].timeout == 0)          sensor_config[SENSOR_TYPE_PRES].timeout = SENSOR_TIMEOUT_PRESENCE;
  if (sensor_config[SENSOR_TYPE_PRES].timeout == UINT32_MAX) sensor_config[SENSOR_TYPE_PRES].timeout = SENSOR_TIMEOUT_PRESENCE;
}

// save configuration parameters
void SensorSaveConfig () 
{
#ifdef USE_UFILESYS

  // save settings into flash filesystem
  UfsCfgSaveKey    (D_SENSOR_FILE_CFG, D_CMND_SENSOR_TEMP_TOPIC,   sensor_config[SENSOR_TYPE_TEMP].topic.c_str (), true);
  UfsCfgSaveKey    (D_SENSOR_FILE_CFG, D_CMND_SENSOR_TEMP_KEY,     sensor_config[SENSOR_TYPE_TEMP].key.c_str (),   false);
  UfsCfgSaveKey    (D_SENSOR_FILE_CFG, D_CMND_SENSOR_HUMI_TOPIC,   sensor_config[SENSOR_TYPE_HUMI].topic.c_str (), false);
  UfsCfgSaveKey    (D_SENSOR_FILE_CFG, D_CMND_SENSOR_HUMI_KEY,     sensor_config[SENSOR_TYPE_HUMI].key.c_str (),   false);
  UfsCfgSaveKey    (D_SENSOR_FILE_CFG, D_CMND_SENSOR_PRES_TOPIC,   sensor_config[SENSOR_TYPE_PRES].topic.c_str (), false);
  UfsCfgSaveKey    (D_SENSOR_FILE_CFG, D_CMND_SENSOR_PRES_KEY,     sensor_config[SENSOR_TYPE_PRES].key.c_str (),   false);

  UfsCfgSaveKeyInt (D_SENSOR_FILE_CFG, D_CMND_SENSOR_TEMP_TIMEOUT, sensor_config[SENSOR_TYPE_TEMP].timeout, false);
  UfsCfgSaveKeyInt (D_SENSOR_FILE_CFG, D_CMND_SENSOR_HUMI_TIMEOUT, sensor_config[SENSOR_TYPE_HUMI].timeout, false);
  UfsCfgSaveKeyInt (D_SENSOR_FILE_CFG, D_CMND_SENSOR_PRES_TIMEOUT, sensor_config[SENSOR_TYPE_PRES].timeout, false);

# else       // No LittleFS

  // save settings into flash memory
  SettingsUpdateText (SET_SENSOR_TEMP_TOPIC, sensor_config[SENSOR_TYPE_TEMP].topic.c_str ());
  SettingsUpdateText (SET_SENSOR_TEMP_KEY,   sensor_config[SENSOR_TYPE_TEMP].key.c_str ());
  SettingsUpdateText (SET_SENSOR_HUMI_TOPIC, sensor_config[SENSOR_TYPE_HUMI].topic.c_str ());
  SettingsUpdateText (SET_SENSOR_HUMI_KEY,   sensor_config[SENSOR_TYPE_HUMI].key.c_str ());
  SettingsUpdateText (SET_SENSOR_MOVE_TOPIC, sensor_config[SENSOR_TYPE_PRES].topic.c_str ());
  SettingsUpdateText (SET_SENSOR_MOVE_KEY,   sensor_config[SENSOR_TYPE_PRES].key.c_str ());

  Settings->weight_item        = sensor_config[SENSOR_TYPE_TEMP].timeout;
  Settings->weight_reference   = sensor_config[SENSOR_TYPE_HUMI].timeout;
  Settings->weight_calibration = sensor_config[SENSOR_TYPE_PRES].timeout;

# endif     // USE_UFILESYS
}

/***********************************************\
 *                  Commands
\***********************************************/

// timezone help
void CmndSensorHelp ()
{
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Sensor commands :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - sn_ttopic  = topic of remote temperature sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - sn_tkey    = key of remote temperature sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - sn_tdrift  = temperature correction (in 1/10 of °C)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - sn_tval    = value of remote temperature sensor"));

  AddLog (LOG_LEVEL_INFO, PSTR (" - sn_htopic  = topic of remote humidity sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - sn_hkey    = key of remote humidity sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - sn_hval    = value of remote humidity sensor"));

  AddLog (LOG_LEVEL_INFO, PSTR (" - sn_mtopic  = topic of remote movement sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - sn_mkey    = key of remote movement sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - sn_mval    = value of remote movement sensor"));

  ResponseCmndDone();
}

void CmndSensorTemperatureTopic ()
{
  char str_text[64];

  if (XdrvMailbox.data_len > 0)
  {
    strncpy (str_text, XdrvMailbox.data, min (sizeof (str_text) - 1, XdrvMailbox.data_len));
    sensor_config[SENSOR_TYPE_TEMP].topic = str_text;
    SensorSaveConfig ();
  }
  ResponseCmndChar (sensor_config[SENSOR_TYPE_TEMP].topic.c_str ());
}

void CmndSensorTemperatureKey ()
{
  char str_text[32];

  if (XdrvMailbox.data_len > 0)
  {
    strncpy (str_text, XdrvMailbox.data, min (sizeof (str_text) - 1, XdrvMailbox.data_len));
    sensor_config[SENSOR_TYPE_TEMP].key = str_text;
    SensorSaveConfig ();
  }
  ResponseCmndChar (sensor_config[SENSOR_TYPE_TEMP].key.c_str ());
}

void CmndSensorTemperatureValue ()
{
  uint32_t timeout = SENSOR_TIMEOUT_TEMPERATURE;

  if (XdrvMailbox.payload > 0) timeout = (uint32_t)XdrvMailbox.payload; 

  ResponseCmndFloat (SensorTemperatureRead (timeout), 1);
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

void CmndSensorHumidityTopic ()
{
  char str_text[64];

  if (XdrvMailbox.data_len > 0)
  {
    strncpy (str_text, XdrvMailbox.data, min (sizeof (str_text) - 1, XdrvMailbox.data_len));
    sensor_config[SENSOR_TYPE_HUMI].topic = str_text;
    SensorSaveConfig ();
  }
  ResponseCmndChar (sensor_config[SENSOR_TYPE_HUMI].topic.c_str ());
}

void CmndSensorHumidityKey ()
{
  char str_text[32];

  if (XdrvMailbox.data_len > 0)
  {
    strncpy (str_text, XdrvMailbox.data, min (sizeof (str_text) - 1, XdrvMailbox.data_len));
    sensor_config[SENSOR_TYPE_HUMI].key = str_text;
    SensorSaveConfig ();
  }
  ResponseCmndChar (sensor_config[SENSOR_TYPE_HUMI].key.c_str ());
}

void CmndSensorHumidityValue ()
{
  uint32_t timeout = SENSOR_TIMEOUT_HUMIDITY;

  if (XdrvMailbox.payload > 0) timeout = (uint32_t)XdrvMailbox.payload; 

  ResponseCmndFloat (SensorHumidityRead (timeout), 1);
}

void CmndSensorPresenceTopic ()
{
  char str_text[64];

  if (XdrvMailbox.data_len > 0)
  {
    strncpy (str_text, XdrvMailbox.data, min (sizeof (str_text) - 1, XdrvMailbox.data_len));
    sensor_config[SENSOR_TYPE_PRES].topic = str_text;
    SensorSaveConfig ();
  }
  ResponseCmndChar (sensor_config[SENSOR_TYPE_PRES].topic.c_str ());
}

void CmndSensorPresenceKey ()
{
  char str_text[32];

  if (XdrvMailbox.data_len > 0)
  {
    strncpy (str_text, XdrvMailbox.data, min (sizeof (str_text) - 1, XdrvMailbox.data_len));
    sensor_config[SENSOR_TYPE_PRES].key = str_text;
    SensorSaveConfig ();
  }
  ResponseCmndChar (sensor_config[SENSOR_TYPE_PRES].key.c_str ());
}

void CmndSensorPresenceValue ()
{
  uint32_t timeout = SENSOR_TIMEOUT_PRESENCE;

  if (XdrvMailbox.payload > 0) timeout = (uint32_t)XdrvMailbox.payload; 

  ResponseCmndNumber ((int)SensorPresenceDetected (timeout));
}

/**************************************************\
 *                  Functions
\**************************************************/

void SensorGetDelayText (const uint32_t delay, char* pstr_result, size_t size_result)
{
  uint32_t days, hours, minutes, remain;

  // check parameters
  if (pstr_result == nullptr) return;
  if (size_result < 12) return;

  // set unit according to value
  if (delay == UINT32_MAX) strcpy (pstr_result, "---");
  else if (delay >= 86400) sprintf (pstr_result, "%ud %uh", delay / 86400, (delay % 86400) / 3600);
  else if (delay >= 3600) sprintf (pstr_result, "%uh %umn", delay / 3600, (delay % 3600) / 60);
  else if (delay >= 60) sprintf (pstr_result, "%umn %us", delay / 60, delay % 60);
  else if (delay > 0) sprintf (pstr_result, "%us", delay);
  else strcpy (pstr_result, "now");
}

// get temperature source
uint8_t SensorTemperatureSource (char* pstr_source, size_t size_source)
{
  // check parameters
  if (pstr_source == nullptr) return UINT8_MAX;
  if (size_source == 0) return UINT8_MAX;

  // retrieve source label
  GetTextIndexed (pstr_source, size_source, sensor_status.type[SENSOR_TYPE_TEMP].source, kSensorSource);

  return sensor_status.type[SENSOR_TYPE_TEMP].source;
}

// read temperature value
float SensorTemperatureRead () { return SensorTemperatureRead (SENSOR_TIMEOUT_TEMPERATURE); }
float SensorTemperatureRead (uint32_t timeout)
{
  uint32_t index;
  float    value = NAN;

  switch (sensor_status.type[SENSOR_TYPE_TEMP].source)
  { 
    case SENSOR_SOURCE_DSB:
#ifdef ESP8266
      Ds18x20Read (0);
      index = ds18x20_sensor[0].index;
      value = ds18x20_sensor[index].temperature;
#else
      Ds18x20Read (0, value);
#endif
      break;

    case SENSOR_SOURCE_DHT:
      value = Dht[0].t;
      break;

    case SENSOR_SOURCE_REMOTE:
      if (sensor_status.type[SENSOR_TYPE_TEMP].delay == UINT32_MAX) value = NAN;
      else if (sensor_status.type[SENSOR_TYPE_TEMP].delay > timeout) value = NAN;
      else value = sensor_status.type[SENSOR_TYPE_TEMP].value + ((float)Settings->temp_comp / 10);
      break;
  }

  // truncate temperature to 0.1 °C
  if (!isnan (value)) value = round (value * 10) / 10;

  return value;
}

// get humidity source
uint8_t SensorHumiditySource (char* pstr_source, size_t size_source)
{
  // check parameters
  if (pstr_source == nullptr) return UINT8_MAX;
  if (size_source == 0) return UINT8_MAX;

  // retrieve source label
  GetTextIndexed (pstr_source, size_source, sensor_status.type[SENSOR_TYPE_HUMI].source, kSensorSource);
  
  return sensor_status.type[SENSOR_TYPE_HUMI].source;
}

// read humidity level
float SensorHumidityRead () { return SensorHumidityRead (SENSOR_TIMEOUT_HUMIDITY); }
float SensorHumidityRead (uint32_t timeout)
{
  float value = NAN;

  switch (sensor_status.type[SENSOR_TYPE_HUMI].source)
  { 
    case SENSOR_SOURCE_DHT:
      value = Dht[0].h;
      break;
    case SENSOR_SOURCE_REMOTE:
      if (sensor_status.type[SENSOR_TYPE_HUMI].delay == UINT32_MAX) value = NAN;
      else if (sensor_status.type[SENSOR_TYPE_HUMI].delay > timeout) value = NAN;
      else value = sensor_status.type[SENSOR_TYPE_HUMI].value;
      break;
  }

  // truncate humidity to 0.1 %
  if (!isnan (value)) value = round (value * 10) / 10;

  return value;
}

// get movement source
uint8_t SensorPresenceSource (char* pstr_source, size_t size_source)
{
  // check parameters
  if (pstr_source == nullptr) return UINT8_MAX;
  if (size_source == 0) return UINT8_MAX;

  // retrieve source label
  GetTextIndexed (pstr_source, size_source, sensor_status.type[SENSOR_TYPE_PRES].source, kSensorSource);
  
  return sensor_status.type[SENSOR_TYPE_PRES].source;
}

// read movement detection status (timeout in sec.)
bool SensorPresenceDetected () { return SensorPresenceDetected (SENSOR_TIMEOUT_PRESENCE); }
bool SensorPresenceDetected (uint32_t timeout)
{
  bool result = true;        // by default, movement considered ON

  // if movement detection sensor is available
  if (sensor_status.type[SENSOR_TYPE_PRES].source != SENSOR_SOURCE_MAX)
  {
    // if movement has never been detected 
    if (sensor_status.type[SENSOR_TYPE_PRES].delay == UINT32_MAX) result = false;

    // else movement detected within timeout
    else if (sensor_status.type[SENSOR_TYPE_PRES].delay > timeout) result = false;
  }

  return result;
}

// get delay since last movement was detected
uint32_t SensorPresenceDelaySinceDetection ()
{
  return sensor_status.type[SENSOR_TYPE_PRES].delay;
}

// reset presence detection
void SensorPresenceResetDetection ()
{
  sensor_status.type[SENSOR_TYPE_PRES].delay = 0;
}

// update sensor condition every second
void SensorEverySecond ()
{
  bool sensor   = false;
  bool presence = false;
  bool publish  = false;

  // handle local temperature sensor
  if ((sensor_status.type[SENSOR_TYPE_TEMP].source != SENSOR_SOURCE_MAX) && (sensor_status.type[SENSOR_TYPE_TEMP].source != SENSOR_SOURCE_REMOTE))
  {
    // first reading
    if (sensor_status.type[SENSOR_TYPE_TEMP].delay == UINT32_MAX) sensor_status.type[SENSOR_TYPE_TEMP].delay = 0;

    // if needed, read temperature
    if (sensor_status.type[SENSOR_TYPE_TEMP].delay == 0)
    {
      SensorTemperatureRead ();
      publish = true;
    }

    // update delay
    sensor_status.type[SENSOR_TYPE_TEMP].delay = (sensor_status.type[SENSOR_TYPE_TEMP].delay + 1) % SENSOR_TIMEOUT_TEMPERATURE;
  }

  // handle local humidity sensor
  if ((sensor_status.type[SENSOR_TYPE_HUMI].source != SENSOR_SOURCE_MAX) && (sensor_status.type[SENSOR_TYPE_HUMI].source == SENSOR_SOURCE_REMOTE))
  {
    // first reading
    if (sensor_status.type[SENSOR_TYPE_HUMI].delay == UINT32_MAX) sensor_status.type[SENSOR_TYPE_HUMI].delay = 0;

    // if needed, read temperature
    if (sensor_status.type[SENSOR_TYPE_HUMI].delay == 0)
    {
      SensorHumidityRead ();
      publish = true;
    }

    // update delay
    sensor_status.type[SENSOR_TYPE_HUMI].delay = (sensor_status.type[SENSOR_TYPE_HUMI].delay + 1) % SENSOR_TIMEOUT_HUMIDITY;
  }

  // handle local presence sensor
  switch (sensor_status.type[SENSOR_TYPE_PRES].source)
  {
    // generic sensor
    case SENSOR_SOURCE_INPUT:
      sensor   = true;
      presence = digitalRead (Pin (GPIO_INPUT, 0));
      break;

#ifdef USE_HLKLD11
    case SENSOR_SOURCE_SERIAL:
      sensor   = true;
      presence = HLKLDGetDetectionStatus (HLKLD_DATA_ANY, 1);
      break;
#endif
  }

  // if sensor is present
  if (sensor)
  {
    // if first detection, publish value
    if (sensor_status.type[SENSOR_TYPE_PRES].delay == UINT32_MAX) publish = true;

    // if presence is currently detected
    if (presence)
    {
      // if last detection was before timeout, publish value
      if ((sensor_status.type[SENSOR_TYPE_PRES].value == 0) && (sensor_status.type[SENSOR_TYPE_PRES].delay > SENSOR_TIMEOUT_PRESENCE)) publish = true;

      // update sensor value
      sensor_status.type[SENSOR_TYPE_PRES].delay = 0;
      sensor_status.type[SENSOR_TYPE_PRES].value = 1;
    }

    // else, no presence detected
    else 
    {
      sensor_status.type[SENSOR_TYPE_PRES].value = 0;
      sensor_status.type[SENSOR_TYPE_PRES].delay++;
    }

    // if timeout reached, publish value
    if (sensor_status.type[SENSOR_TYPE_PRES].delay % SENSOR_TIMEOUT_PRESENCE == 0) publish = true;
  }

  // if needed, publish sensor values
  if (publish) OffloadShowJSON (true);
}

// Show JSON status (for MQTT)
void SensorShowJSON (bool is_autonomous)
{
  bool temperature, humidity, presence;
  bool first = true;
  char str_value[8];

  // check if sensors are available
  temperature = (sensor_status.type[SENSOR_TYPE_TEMP].source != SENSOR_SOURCE_MAX);
  humidity    = (sensor_status.type[SENSOR_TYPE_HUMI].source != SENSOR_SOURCE_MAX);
  presence    = (sensor_status.type[SENSOR_TYPE_PRES].source != SENSOR_SOURCE_MAX);

  // if at least one sensor is available
  if (temperature || humidity || presence)
  {
    // add , in append mode
    if (is_autonomous) Response_P (PSTR ("{")); else ResponseAppend_P (PSTR (","));

    // temperature
    if (temperature)
    {
      ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%01_f"), &sensor_status.type[SENSOR_TYPE_TEMP].value);
      ResponseAppend_P (PSTR ("\"temp\":%s"), str_value);
      first = false;
    }

    // humidity
    if (humidity)
    {
      ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%00_f"), &sensor_status.type[SENSOR_TYPE_HUMI].value);
      if (!first) ResponseAppend_P (PSTR (","));
      ResponseAppend_P (PSTR ("\"humi\":%s"), str_value);
      first = false;
    }

    // presence
    if (presence)
    {
      ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%00_f"), &sensor_status.type[SENSOR_TYPE_HUMI].value);
      if (!first) ResponseAppend_P (PSTR (","));
      ResponseAppend_P (PSTR ("\"pres\":%s"), str_value);
    }

    // publish it if message is autonomous
    if (is_autonomous)
    {
      // publish message
      ResponseAppend_P (PSTR ("}"));
      MqttPublishTeleSensor ();
    } 
  }
}

// check and update MQTT power subsciption after disconnexion
void SensorMqttSubscribe ()
{
  // if topic is defined, subscribe to remote temperature
  if (sensor_config[SENSOR_TYPE_TEMP].topic.length () > 0)
  {
    // subscribe to sensor topic
    MqttSubscribe (sensor_config[SENSOR_TYPE_TEMP].topic.c_str ());
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Subscribed remote temperature from %s"), sensor_config[SENSOR_TYPE_TEMP].topic.c_str ());
  }

  // if topic is defined, subscribe to remote humidity
  if (sensor_config[SENSOR_TYPE_HUMI].topic.length () > 0)
  {
    // subscribe to sensor topic
    MqttSubscribe (sensor_config[SENSOR_TYPE_HUMI].topic.c_str ());
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Subscribed remote humidity from %s"), sensor_config[SENSOR_TYPE_HUMI].topic.c_str ());
  }

  // if topic is defined, subscribe to remote movement
  if (sensor_config[SENSOR_TYPE_PRES].topic.length () > 0)
  {
    // subscribe to sensor topic
    MqttSubscribe (sensor_config[SENSOR_TYPE_PRES].topic.c_str ());
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Subscribed remote movement from %s"), sensor_config[SENSOR_TYPE_PRES].topic.c_str ());
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
  bool is_topic, is_key;
  bool is_found = false;
  char str_value[32];

  // if dealing with temperature topic
  is_topic = (sensor_config[SENSOR_TYPE_TEMP].topic == XdrvMailbox.topic);
  if (is_topic)
  {
    // log
    is_found = true;
    AddLog (LOG_LEVEL_DEBUG, PSTR ("SEN: Received %s"), XdrvMailbox.topic);

    // look for temperature key
    is_key = SensorGetJsonKey (XdrvMailbox.data, sensor_config[SENSOR_TYPE_TEMP].key.c_str (), str_value, sizeof (str_value));
    if (is_key)
    {
      // if needed, set remote source for temperature
      if (sensor_status.type[SENSOR_TYPE_TEMP].source == SENSOR_SOURCE_MAX) sensor_status.type[SENSOR_TYPE_TEMP].source = SENSOR_SOURCE_REMOTE;

      // save remote value
      sensor_status.type[SENSOR_TYPE_TEMP].delay = 0;
      sensor_status.type[SENSOR_TYPE_TEMP].value = atof (str_value);

      // log
      is_found = true;
      AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Remote %s °C"), str_value);
    }
  }

  // if dealing with humidity topic
  is_topic = (sensor_config[SENSOR_TYPE_HUMI].topic == XdrvMailbox.topic);
  if (is_topic)
  {
    // log
    is_found = true;
    AddLog (LOG_LEVEL_DEBUG, PSTR ("SEN: Received %s"), XdrvMailbox.topic);

    // look for humidity key
    is_key = SensorGetJsonKey (XdrvMailbox.data, sensor_config[SENSOR_TYPE_HUMI].key.c_str (), str_value, sizeof (str_value));
    if (is_key)
    {
      // if needed, set remote source for humidity
      if (sensor_status.type[SENSOR_TYPE_HUMI].source == SENSOR_SOURCE_MAX) sensor_status.type[SENSOR_TYPE_HUMI].source = SENSOR_SOURCE_REMOTE;

      // save remote value
      sensor_status.type[SENSOR_TYPE_HUMI].delay = 0;
      sensor_status.type[SENSOR_TYPE_HUMI].value = atof (str_value);

      // log
      AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Remote humidity is %s %%"), str_value);
    }
  }

  // if dealing with movement topic
  is_topic = (sensor_config[SENSOR_TYPE_PRES].topic == XdrvMailbox.topic);
  if (is_topic)
  {
    // log
    is_found = true;
    AddLog (LOG_LEVEL_DEBUG, PSTR ("SEN: Received %s"), XdrvMailbox.topic);

    // look for movement key
    is_key = SensorGetJsonKey (XdrvMailbox.data, sensor_config[SENSOR_TYPE_PRES].key.c_str (), str_value, sizeof (str_value));
    if (is_key)
    {
      // if needed, set remote source for movement
      if (sensor_status.type[SENSOR_TYPE_PRES].source == SENSOR_SOURCE_MAX) sensor_status.type[SENSOR_TYPE_PRES].source = SENSOR_SOURCE_REMOTE;

      // convert off, OFF, on, ON and values to 0 or 1
      if (strcmp (str_value, "off") == 0) sensor_status.type[SENSOR_TYPE_PRES].value = 0;
      else if (strcmp (str_value, "OFF") == 0) sensor_status.type[SENSOR_TYPE_PRES].value = 0;
      else if (strcmp (str_value, "on") == 0) sensor_status.type[SENSOR_TYPE_PRES].value = 1;
      else if (strcmp (str_value, "ON") == 0) sensor_status.type[SENSOR_TYPE_PRES].value = 1;
      else sensor_status.type[SENSOR_TYPE_PRES].value = atof (str_value);

      // if movement detected, update detection time
      if (sensor_status.type[SENSOR_TYPE_PRES].value > 0) sensor_status.type[SENSOR_TYPE_PRES].delay = 0;

      // log
      AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Remote movement is %s"), str_value);
    }
  }

  return is_found;
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// append remote sensor to main page
void SensorWebSensor ()
{
  char str_type[8];
  char str_value[16];

  // if temperature is remote
  if (sensor_status.type[SENSOR_TYPE_TEMP].source == SENSOR_SOURCE_REMOTE)
  {
    ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &sensor_status.type[SENSOR_TYPE_TEMP].value);
    WSContentSend_PD (PSTR ("{s}%s %s{m}%s °C{e}"), D_SENSOR_REMOTE, D_SENSOR_TEMPERATURE, str_value);
  }

  // if humidity is remote
  if (sensor_status.type[SENSOR_TYPE_HUMI].source == SENSOR_SOURCE_REMOTE)
  {
    ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &sensor_status.type[SENSOR_TYPE_HUMI].value);
    WSContentSend_PD (PSTR ("{s}%s %s{m}%s %%{e}"), D_SENSOR_REMOTE, D_SENSOR_HUMIDITY, str_value);
  }

  // if movement is remote or generic
  if ((sensor_status.type[SENSOR_TYPE_PRES].source == SENSOR_SOURCE_REMOTE) || (sensor_status.type[SENSOR_TYPE_PRES].source == SENSOR_SOURCE_INPUT))
  {
    // check sensor type
    if (sensor_status.type[SENSOR_TYPE_PRES].source == SENSOR_SOURCE_REMOTE) strcpy (str_type, D_SENSOR_REMOTE);
      else strcpy (str_type, D_SENSOR_LOCAL);
    
    // get last presence detection delay
    SensorGetDelayText (sensor_status.type[SENSOR_TYPE_PRES].delay, str_value, sizeof (str_value));
    WSContentSend_PD (PSTR ("{s}%s detector{m}%s{e}"), str_type, str_value);
  }
}

// remote sensor configuration web page
void SensorWebConfigurePage ()
{
  float drift, step;
  uint16_t value;
  char  str_value[8];
  char  str_text[128];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess ()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg ("save"))
  {
    // get temperature drift according to 'tdrift' parameter
    WebGetArg (D_CMND_SENSOR_TEMP_DRIFT, str_text, sizeof (str_text));
    if (strlen (str_text) > 0)
    {
      drift = atof (str_text) * 10;
      Settings->temp_comp = (int8_t)drift;
    }

    // set temperature timeout according to 'ttime' parameter
    WebGetArg (D_CMND_SENSOR_TEMP_TIMEOUT, str_text, sizeof (str_text));
    sensor_config[SENSOR_TYPE_TEMP].timeout = (uint16_t)atoi (str_text);

    // set temperature topic according to 'ttopic' parameter
    WebGetArg (D_CMND_SENSOR_TEMP_TOPIC, str_text, sizeof (str_text));
    sensor_config[SENSOR_TYPE_TEMP].topic = str_text;

    // set temperature key according to 'tkey' parameter
    WebGetArg (D_CMND_SENSOR_TEMP_KEY, str_text, sizeof (str_text));
    sensor_config[SENSOR_TYPE_TEMP].key = str_text;

    // set humidity timeout according to 'htime' parameter
    WebGetArg (D_CMND_SENSOR_HUMI_TIMEOUT, str_text, sizeof (str_text));
    sensor_config[SENSOR_TYPE_HUMI].timeout = (uint16_t)atoi (str_text);

    // set humidity topic according to 'htopic' parameter
    WebGetArg (D_CMND_SENSOR_HUMI_TOPIC, str_text, sizeof (str_text));
    sensor_config[SENSOR_TYPE_HUMI].topic = str_text;

    // set humidity key according to 'hkey' parameter
    WebGetArg (D_CMND_SENSOR_HUMI_KEY, str_text, sizeof (str_text));
    sensor_config[SENSOR_TYPE_HUMI].key = str_text;

    // set presence timeout according to 'ptime' parameter
    WebGetArg (D_CMND_SENSOR_PRES_TIMEOUT, str_text, sizeof (str_text));
    sensor_config[SENSOR_TYPE_PRES].timeout = (uint16_t)atoi (str_text);

    // set presence topic according to 'ptopic' parameter
    WebGetArg (D_CMND_SENSOR_PRES_TOPIC, str_text, sizeof (str_text));
    sensor_config[SENSOR_TYPE_PRES].topic = str_text;

    // set presence key according to 'pkey' parameter
    WebGetArg (D_CMND_SENSOR_PRES_KEY, str_text, sizeof (str_text));
    sensor_config[SENSOR_TYPE_PRES].key = str_text;

    // save config
    SensorSaveConfig ();

    // restart device
    WebRestart (1);
  }

  // beginning of form
  WSContentStart_P (D_SENSOR_CONFIGURE);
  WSContentSendStyle ();
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("span.key {float:right;font-size:0.7rem;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_PAGE_SENSOR);

  //      temperature  
  // ----------------------

  WSContentSend_P (SENSOR_FIELDSET_START, "🌡️", D_SENSOR_TEMPERATURE);

  // Correction 
  WSContentSend_P (PSTR ("<p>\n"));
  step = SENSOR_TEMP_DRIFT_STEP;
  ext_snprintf_P (str_text, sizeof (str_text), PSTR ("%1_f"), &step);
  drift = (float)Settings->temp_comp / 10;
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &drift);
  WSContentSend_P (SENSOR_FIELD_CONFIG, D_SENSOR_CORRECTION, "°C", D_CMND_SENSOR_TEMP_DRIFT, D_CMND_SENSOR_TEMP_DRIFT, - SENSOR_TEMP_DRIFT_MAX, SENSOR_TEMP_DRIFT_MAX, str_text, str_value);
  WSContentSend_P (PSTR ("</p>\n"));

  // timeout 
  WSContentSend_P (PSTR ("<p>\n"));
  itoa (sensor_config[SENSOR_TYPE_TEMP].timeout, str_value, 10);
  WSContentSend_P (SENSOR_FIELD_CONFIG, D_SENSOR_TIMEOUT, "sec.", D_CMND_SENSOR_TEMP_TIMEOUT, D_CMND_SENSOR_TEMP_TIMEOUT, 1, 3600, "1", str_value);
  WSContentSend_P (PSTR ("</p>\n"));

  WSContentSend_P (PSTR ("<hr>\n"));

  // topic and key
  WSContentSend_P (PSTR ("<p>\n"));

  WSContentSend_P (SENSOR_FIELD_INPUT, D_SENSOR_TOPIC, D_CMND_SENSOR_TEMP_TOPIC, D_CMND_SENSOR_TEMP_TOPIC, sensor_config[SENSOR_TYPE_TEMP].topic.c_str ());
  WSContentSend_P (SENSOR_FIELD_INPUT, D_SENSOR_KEY, D_CMND_SENSOR_TEMP_KEY, D_CMND_SENSOR_TEMP_KEY, sensor_config[SENSOR_TYPE_TEMP].key.c_str ());

  WSContentSend_P (PSTR ("</p>\n"));
  WSContentSend_P (SENSOR_FIELDSET_STOP);

  //   remote humidity  
  // ----------------------

  WSContentSend_P (SENSOR_FIELDSET_START, "💦", D_SENSOR_HUMIDITY);

  // timeout 
  WSContentSend_P (PSTR ("<p>\n"));
  itoa (sensor_config[SENSOR_TYPE_HUMI].timeout, str_value, 10);
  WSContentSend_P (SENSOR_FIELD_CONFIG, D_SENSOR_TIMEOUT, "sec.", D_CMND_SENSOR_HUMI_TIMEOUT, D_CMND_SENSOR_HUMI_TIMEOUT, 1, 3600, "1", str_value);
  WSContentSend_P (PSTR ("</p>\n"));

  WSContentSend_P (PSTR ("<hr>\n"));

  // topic and key
  WSContentSend_P (PSTR ("<p>\n"));

  WSContentSend_P (SENSOR_FIELD_INPUT, D_SENSOR_TOPIC, D_CMND_SENSOR_HUMI_TOPIC, D_CMND_SENSOR_HUMI_TOPIC, sensor_config[SENSOR_TYPE_HUMI].topic.c_str ());
  WSContentSend_P (SENSOR_FIELD_INPUT, D_SENSOR_KEY, D_CMND_SENSOR_HUMI_KEY, D_CMND_SENSOR_HUMI_KEY, sensor_config[SENSOR_TYPE_HUMI].key.c_str ());

  WSContentSend_P (PSTR ("</p>\n"));
  WSContentSend_P (SENSOR_FIELDSET_STOP);

  //   remote movement  
  // ----------------------

  WSContentSend_P (SENSOR_FIELDSET_START, "👋", D_SENSOR_PRESENCE);

  // timeout 
  WSContentSend_P (PSTR ("<p>\n"));
  itoa (sensor_config[SENSOR_TYPE_PRES].timeout, str_value, 10);
  WSContentSend_P (SENSOR_FIELD_CONFIG, D_SENSOR_TIMEOUT, "sec.", D_CMND_SENSOR_PRES_TIMEOUT, D_CMND_SENSOR_PRES_TIMEOUT, 1, 3600, "1", str_value);
  WSContentSend_P (PSTR ("</p>\n"));

  WSContentSend_P (PSTR ("<hr>\n"));
  
  // topic and key 
  WSContentSend_P (PSTR ("<p>\n"));

  WSContentSend_P (SENSOR_FIELD_INPUT, D_SENSOR_TOPIC, D_CMND_SENSOR_PRES_TOPIC, D_CMND_SENSOR_PRES_TOPIC, sensor_config[SENSOR_TYPE_PRES].topic.c_str ());
  WSContentSend_P (SENSOR_FIELD_INPUT, D_SENSOR_KEY, D_CMND_SENSOR_PRES_KEY, D_CMND_SENSOR_PRES_KEY, sensor_config[SENSOR_TYPE_PRES].key.c_str ());

  WSContentSend_P (PSTR ("</p>\n"));
  WSContentSend_P (SENSOR_FIELDSET_STOP);

  // end of form and save button
  WSContentSend_P (PSTR ("<p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), D_SAVE);
  WSContentSend_P (PSTR ("</form>\n"));

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

#endif  // USE_WEBSERVER

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xsns125 (uint8_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
   case FUNC_INIT:
      SensorInit ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kSensorCommands, SensorCommand);
      break;
    case FUNC_EVERY_SECOND:
      if (TasmotaGlobal.uptime > 5) SensorEverySecond ();
      break;
    case FUNC_JSON_APPEND:
      SensorShowJSON (false);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      SensorWebSensor ();
      break;
    case FUNC_WEB_ADD_HANDLER:
      Webserver->on ("/" D_PAGE_SENSOR, SensorWebConfigurePage);
      break;
    case FUNC_WEB_ADD_BUTTON:
      WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s %s</button></form></p>"), D_PAGE_SENSOR, D_SENSOR_CONFIGURE, D_SENSOR);
      break;
#endif  // USE_WEBSERVER
  }
  
  return result;
}

#endif      // USE_GENERIC_SENSOR
#endif      //  FIRMWARE_SAFEBOOT

