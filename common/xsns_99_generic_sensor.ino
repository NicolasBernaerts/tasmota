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
    - Settings.weight_max             : type of presence sensor

  If LittleFS partition is available, all settings are stored in sensor.cfg

  To use the remote sensors, you need to add these function calls in any one of your xdrv_ file :
    FUNC_MQTT_SUBSCRIBE: SensorMqttSubscribe ()
    FUNC_MQTT_DATA:      SensorMqttData ()

  Handled local sensor are :
    * Temperature = DHT11, AM2301, SI7021, SHT30 or DS18x20
    * Humidity    = DHT11, AM2301, SI7021 or SHT30
    * Presence    = Generic presence/movement detector declared as Counter 1
                    HLK-LD1115H, HLK-LD1125H or HLK-LD2410 connected thru serial port (Rx and Tx)
    
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

#define XSNS_99                      99

#include <ArduinoJson.h>

#define SENSOR_TEMP_TIMEOUT           600            // temperature default validity (in sec.)
#define SENSOR_HUMI_TIMEOUT           600            // humidity default validity (in sec.)
#define SENSOR_PRES_TIMEOUT           5             // presence default validity (in sec.)

#define SENSOR_TEMP_DRIFT_MAX         10
#define SENSOR_TEMP_DRIFT_STEP        0.1

#define SENSOR_PRESENCE_INDEX         0             // sensor index when Option A selected (0=Counter 1 ... 7=Counter 8)

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
#define D_CMND_SENSOR_PRES_TYPE       "presence"

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

#define SENSOR_COLOR_NONE             "#444"
#define SENSOR_COLOR_MOTION           "#F44"

#ifdef USE_UFILESYS
#define D_SENSOR_FILE_CFG             "/sensor.cfg"
#endif     // USE_UFILESYS

// sensor family type
enum SensorFamilyType { SENSOR_TYPE_TEMP, SENSOR_TYPE_HUMI, SENSOR_TYPE_PRES, SENSOR_TYPE_MAX };

// presence serial sensor type
enum SensorPresenceModel { SENSOR_PRESENCE_NONE, SENSOR_PRESENCE_RCWL0516, SENSOR_PRESENCE_HWMS03, SENSOR_PRESENCE_LD1115, SENSOR_PRESENCE_LD1125, SENSOR_PRESENCE_LD2410, SENSOR_PRESENCE_MAX };
const char kSensorPresenceModel[] PROGMEM = "None|RCWL-0516|HW-MS03|HLK-LD1115|HLK-LD1125|HLK-LD2410|";

// remote sensor sources
enum SensorSource { SENSOR_SOURCE_DSB, SENSOR_SOURCE_DHT, SENSOR_SOURCE_SHT, SENSOR_SOURCE_COUNTER, SENSOR_SOURCE_SERIAL, SENSOR_SOURCE_REMOTE, SENSOR_SOURCE_NONE };
const char kSensorSource[] PROGMEM = "Local|Local|Local|Local|Local|Local|Remote|Undef.";

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

// sensor data
struct sensor_mqtt
{
  String topic;                               // remote sensor topic
  String key;                                 // remote sensor key
};
struct {
  sensor_mqtt mqtt[SENSOR_TYPE_MAX];          // sensor types
  uint8_t  presence = SENSOR_PRESENCE_NONE;   // presence sensor type
} sensor_config;

struct sensor_data
{
  uint8_t  source;                            // source of data (local sensor type)
  uint32_t timeout;                           // data publication timeout
  uint32_t timestamp;                         // last time sensor was updated
  float    value;                             // current remote sensor value
};
struct {
  sensor_data type[SENSOR_TYPE_MAX];          // sensor types
  uint32_t time_ignore = 0;                   // timestamp to ignore sensor update
} sensor_status;

/**************************************************\
 *                  Accessors
\**************************************************/

void SensorInit () 
{
  uint8_t  sensor;
  uint32_t index = UINT32_MAX;
  char     str_type[16];

  // load configuration
  SensorLoadConfig ();

  // init remote sensor values
  for (sensor = 0; sensor < SENSOR_TYPE_MAX; sensor ++)
  {
    sensor_status.type[sensor].source    = SENSOR_SOURCE_NONE;
    sensor_status.type[sensor].value     = NAN;
    sensor_status.type[sensor].timestamp = 0;
  }

  // check for DHT11 sensor
  if (PinUsed (GPIO_DHT11))
  {
    sensor_status.type[SENSOR_TYPE_TEMP].source = SENSOR_SOURCE_DHT;
    sensor_status.type[SENSOR_TYPE_HUMI].source = SENSOR_SOURCE_DHT;
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: %s temperature & humidity sensor"), "DHT11");
  }

  // check for DHT22 sensor
  else if (PinUsed (GPIO_DHT22))
  {
    sensor_status.type[SENSOR_TYPE_TEMP].source = SENSOR_SOURCE_DHT;
    sensor_status.type[SENSOR_TYPE_HUMI].source = SENSOR_SOURCE_DHT;
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: %s temperature & humidity sensor"), "DHT22");
  }

  // check for SI7021 sensor
  else if (PinUsed (GPIO_SI7021))
  {
    sensor_status.type[SENSOR_TYPE_TEMP].source = SENSOR_SOURCE_DHT;
    sensor_status.type[SENSOR_TYPE_HUMI].source = SENSOR_SOURCE_DHT;
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: %s temperature & humidity sensor"), "SI7021");
  }

  // check for SHT30 and SHT40 sensor
  else if (PinUsed (GPIO_I2C_SCL) && PinUsed (GPIO_I2C_SDA))
  {
    sensor_status.type[SENSOR_TYPE_TEMP].source = SENSOR_SOURCE_SHT;
    sensor_status.type[SENSOR_TYPE_HUMI].source = SENSOR_SOURCE_SHT;
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: %s temperature & humidity sensor"), "SHTxx");
  }

  // check for DS18B20 sensor
  else if (PinUsed (GPIO_DSB))
  {
    sensor_status.type[SENSOR_TYPE_TEMP].source = SENSOR_SOURCE_DSB;
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
        sensor_status.type[SENSOR_TYPE_PRES].source = SENSOR_SOURCE_COUNTER;
        AddLog (LOG_LEVEL_INFO, PSTR ("SEN: %s motion sensor"), str_type);
        break;

      case SENSOR_PRESENCE_HWMS03:
        GetTextIndexed (str_type, sizeof (str_type), SENSOR_PRESENCE_HWMS03, kSensorPresenceModel);
        sensor_status.type[SENSOR_TYPE_PRES].source = SENSOR_SOURCE_COUNTER;
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
        if (LD1115InitDevice (0)) sensor_status.type[SENSOR_TYPE_PRES].source = SENSOR_SOURCE_SERIAL;
        break;
#endif    // USE_LD1115

#ifdef USE_LD1125             // HLK-LD1125 sensor
      case SENSOR_PRESENCE_LD1125:
        if (LD1125InitDevice (0)) sensor_status.type[SENSOR_TYPE_PRES].source = SENSOR_SOURCE_SERIAL;
        break;
#endif    // USE_LD1125

#ifdef USE_LD2410             // HLK-LD2410 sensor
      case SENSOR_PRESENCE_LD2410:
        if (LD2410InitDevice (0)) sensor_status.type[SENSOR_TYPE_PRES].source = SENSOR_SOURCE_SERIAL;
        break;
#endif    // USE_LD2410
    }
  }

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: sn_help to get help on Sensor commands"));
}

// load configuration parameters
void SensorLoadConfig () 
{
  char str_text[128];

#ifdef USE_UFILESYS

  // retrieve saved settings from flash filesystem
  sensor_config.mqtt[SENSOR_TYPE_TEMP].topic   = UfsCfgLoadKey    (D_SENSOR_FILE_CFG, D_CMND_SENSOR_TEMP_TOPIC);
  sensor_config.mqtt[SENSOR_TYPE_TEMP].key     = UfsCfgLoadKey    (D_SENSOR_FILE_CFG, D_CMND_SENSOR_TEMP_KEY);
  sensor_config.mqtt[SENSOR_TYPE_HUMI].topic   = UfsCfgLoadKey    (D_SENSOR_FILE_CFG, D_CMND_SENSOR_HUMI_TOPIC);
  sensor_config.mqtt[SENSOR_TYPE_HUMI].key     = UfsCfgLoadKey    (D_SENSOR_FILE_CFG, D_CMND_SENSOR_HUMI_KEY);
  sensor_config.mqtt[SENSOR_TYPE_PRES].topic   = UfsCfgLoadKey    (D_SENSOR_FILE_CFG, D_CMND_SENSOR_PRES_TOPIC);
  sensor_config.mqtt[SENSOR_TYPE_PRES].key     = UfsCfgLoadKey    (D_SENSOR_FILE_CFG, D_CMND_SENSOR_PRES_KEY);

  sensor_status.type[SENSOR_TYPE_TEMP].timeout = (uint32_t)UfsCfgLoadKeyInt (D_SENSOR_FILE_CFG, D_CMND_SENSOR_TEMP_TIMEOUT, SENSOR_TEMP_TIMEOUT);
  sensor_status.type[SENSOR_TYPE_HUMI].timeout = (uint32_t)UfsCfgLoadKeyInt (D_SENSOR_FILE_CFG, D_CMND_SENSOR_HUMI_TIMEOUT, SENSOR_HUMI_TIMEOUT);
  sensor_status.type[SENSOR_TYPE_PRES].timeout = (uint32_t)UfsCfgLoadKeyInt (D_SENSOR_FILE_CFG, D_CMND_SENSOR_PRES_TIMEOUT, SENSOR_PRES_TIMEOUT);

  sensor_config.presence = UfsCfgLoadKeyInt (D_SENSOR_FILE_CFG, D_CMND_SENSOR_PRES_TYPE, SENSOR_PRESENCE_NONE);

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Config from LittleFS"));

#else       // No LittleFS

  // retrieve saved settings from flash memory
  sensor_config.mqtt[SENSOR_TYPE_TEMP].topic = SettingsText (SET_SENSOR_TEMP_TOPIC);
  sensor_config.mqtt[SENSOR_TYPE_TEMP].key   = SettingsText (SET_SENSOR_TEMP_KEY);
  sensor_config.mqtt[SENSOR_TYPE_HUMI].topic = SettingsText (SET_SENSOR_HUMI_TOPIC);
  sensor_config.mqtt[SENSOR_TYPE_HUMI].key   = SettingsText (SET_SENSOR_HUMI_KEY);
  sensor_config.mqtt[SENSOR_TYPE_PRES].topic = SettingsText (SET_SENSOR_MOVE_TOPIC);
  sensor_config.mqtt[SENSOR_TYPE_PRES].key   = SettingsText (SET_SENSOR_MOVE_KEY);

  sensor_status.type[SENSOR_TYPE_TEMP].timeout = (uint32_t)Settings->weight_item;
  sensor_status.type[SENSOR_TYPE_HUMI].timeout = (uint32_t)Settings->weight_reference;
  sensor_status.type[SENSOR_TYPE_PRES].timeout = (uint32_t)Settings->weight_calibration;

  sensor_config.presence = Settings->weight_max;

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Config from Settings"));

# endif     // USE_UFILESYS

  // check parameters validity
  if ((sensor_status.type[SENSOR_TYPE_TEMP].timeout == 0) || (sensor_status.type[SENSOR_TYPE_TEMP].timeout == UINT32_MAX)) sensor_status.type[SENSOR_TYPE_TEMP].timeout = SENSOR_TEMP_TIMEOUT;
  if ((sensor_status.type[SENSOR_TYPE_HUMI].timeout == 0) || (sensor_status.type[SENSOR_TYPE_HUMI].timeout == UINT32_MAX)) sensor_status.type[SENSOR_TYPE_HUMI].timeout = SENSOR_HUMI_TIMEOUT;
  if ((sensor_status.type[SENSOR_TYPE_PRES].timeout == 0) || (sensor_status.type[SENSOR_TYPE_PRES].timeout == UINT32_MAX)) sensor_status.type[SENSOR_TYPE_PRES].timeout = SENSOR_PRES_TIMEOUT;
  if (sensor_config.presence >= SENSOR_PRESENCE_MAX) sensor_config.presence = SENSOR_PRESENCE_NONE;
}

// save configuration parameters
void SensorSaveConfig () 
{
#ifdef USE_UFILESYS

  // save settings into flash filesystem
  UfsCfgSaveKey    (D_SENSOR_FILE_CFG, D_CMND_SENSOR_TEMP_TOPIC,   sensor_config.mqtt[SENSOR_TYPE_TEMP].topic.c_str (), true);
  UfsCfgSaveKey    (D_SENSOR_FILE_CFG, D_CMND_SENSOR_TEMP_KEY,     sensor_config.mqtt[SENSOR_TYPE_TEMP].key.c_str (),   false);
  UfsCfgSaveKey    (D_SENSOR_FILE_CFG, D_CMND_SENSOR_HUMI_TOPIC,   sensor_config.mqtt[SENSOR_TYPE_HUMI].topic.c_str (), false);
  UfsCfgSaveKey    (D_SENSOR_FILE_CFG, D_CMND_SENSOR_HUMI_KEY,     sensor_config.mqtt[SENSOR_TYPE_HUMI].key.c_str (),   false);
  UfsCfgSaveKey    (D_SENSOR_FILE_CFG, D_CMND_SENSOR_PRES_TOPIC,   sensor_config.mqtt[SENSOR_TYPE_PRES].topic.c_str (), false);
  UfsCfgSaveKey    (D_SENSOR_FILE_CFG, D_CMND_SENSOR_PRES_KEY,     sensor_config.mqtt[SENSOR_TYPE_PRES].key.c_str (),   false);

  UfsCfgSaveKeyInt (D_SENSOR_FILE_CFG, D_CMND_SENSOR_TEMP_TIMEOUT, (int)sensor_status.type[SENSOR_TYPE_TEMP].timeout, false);
  UfsCfgSaveKeyInt (D_SENSOR_FILE_CFG, D_CMND_SENSOR_HUMI_TIMEOUT, (int)sensor_status.type[SENSOR_TYPE_HUMI].timeout, false);
  UfsCfgSaveKeyInt (D_SENSOR_FILE_CFG, D_CMND_SENSOR_PRES_TIMEOUT, (int)sensor_status.type[SENSOR_TYPE_PRES].timeout, false);

  UfsCfgSaveKeyInt (D_SENSOR_FILE_CFG, D_CMND_SENSOR_PRES_TYPE, sensor_config.presence, false);

# else       // No LittleFS

  // save settings into flash memory
  SettingsUpdateText (SET_SENSOR_TEMP_TOPIC, sensor_config.mqtt[SENSOR_TYPE_TEMP].topic.c_str ());
  SettingsUpdateText (SET_SENSOR_TEMP_KEY,   sensor_config.mqtt[SENSOR_TYPE_TEMP].key.c_str ());
  SettingsUpdateText (SET_SENSOR_HUMI_TOPIC, sensor_config.mqtt[SENSOR_TYPE_HUMI].topic.c_str ());
  SettingsUpdateText (SET_SENSOR_HUMI_KEY,   sensor_config.mqtt[SENSOR_TYPE_HUMI].key.c_str ());
  SettingsUpdateText (SET_SENSOR_MOVE_TOPIC, sensor_config.mqtt[SENSOR_TYPE_PRES].topic.c_str ());
  SettingsUpdateText (SET_SENSOR_MOVE_KEY,   sensor_config.mqtt[SENSOR_TYPE_PRES].key.c_str ());

  Settings->weight_item        = sensor_status.type[SENSOR_TYPE_TEMP].timeout;
  Settings->weight_reference   = sensor_status.type[SENSOR_TYPE_HUMI].timeout;
  Settings->weight_calibration = sensor_status.type[SENSOR_TYPE_PRES].timeout;
  Settings->weight_max         = sensor_config.presence;

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
    sensor_config.mqtt[SENSOR_TYPE_TEMP].topic = str_text;
    SensorSaveConfig ();
  }
  ResponseCmndChar (sensor_config.mqtt[SENSOR_TYPE_TEMP].topic.c_str ());
}

void CmndSensorTemperatureKey ()
{
  char str_text[32];

  if (XdrvMailbox.data_len > 0)
  {
    strncpy (str_text, XdrvMailbox.data, min (sizeof (str_text) - 1, XdrvMailbox.data_len));
    sensor_config.mqtt[SENSOR_TYPE_TEMP].key = str_text;
    SensorSaveConfig ();
  }
  ResponseCmndChar (sensor_config.mqtt[SENSOR_TYPE_TEMP].key.c_str ());
}

void CmndSensorTemperatureValue ()
{
  uint32_t timeout = SENSOR_TEMP_TIMEOUT;

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
    sensor_config.mqtt[SENSOR_TYPE_HUMI].topic = str_text;
    SensorSaveConfig ();
  }
  ResponseCmndChar (sensor_config.mqtt[SENSOR_TYPE_HUMI].topic.c_str ());
}

void CmndSensorHumidityKey ()
{
  char str_text[32];

  if (XdrvMailbox.data_len > 0)
  {
    strncpy (str_text, XdrvMailbox.data, min (sizeof (str_text) - 1, XdrvMailbox.data_len));
    sensor_config.mqtt[SENSOR_TYPE_HUMI].key = str_text;
    SensorSaveConfig ();
  }
  ResponseCmndChar (sensor_config.mqtt[SENSOR_TYPE_HUMI].key.c_str ());
}

void CmndSensorHumidityValue ()
{
  uint32_t timeout = SENSOR_HUMI_TIMEOUT;

  if (XdrvMailbox.payload > 0) timeout = (uint32_t)XdrvMailbox.payload; 

  ResponseCmndFloat (SensorHumidityRead (timeout), 1);
}

void CmndSensorPresenceTopic ()
{
  char str_text[64];

  if (XdrvMailbox.data_len > 0)
  {
    strncpy (str_text, XdrvMailbox.data, min (sizeof (str_text) - 1, XdrvMailbox.data_len));
    sensor_config.mqtt[SENSOR_TYPE_PRES].topic = str_text;
    SensorSaveConfig ();
  }
  ResponseCmndChar (sensor_config.mqtt[SENSOR_TYPE_PRES].topic.c_str ());
}

void CmndSensorPresenceKey ()
{
  char str_text[32];

  if (XdrvMailbox.data_len > 0)
  {
    strncpy (str_text, XdrvMailbox.data, min (sizeof (str_text) - 1, XdrvMailbox.data_len));
    sensor_config.mqtt[SENSOR_TYPE_PRES].key = str_text;
    SensorSaveConfig ();
  }
  ResponseCmndChar (sensor_config.mqtt[SENSOR_TYPE_PRES].key.c_str ());
}

void CmndSensorPresenceValue ()
{
  uint32_t timeout = SENSOR_PRES_TIMEOUT;

  if (XdrvMailbox.payload > 0) timeout = (uint32_t)XdrvMailbox.payload; 

  ResponseCmndNumber ((int)SensorPresenceDetected (timeout));
}

/**************************************************\
 *                  Functions
\**************************************************/

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
  else if (time_delay >= 3600) sprintf (pstr_result, "%uh %umn", time_delay / 3600, (time_delay % 3600) / 60);
  else if (time_delay >= 60) sprintf (pstr_result, "%umn %us", time_delay / 60, time_delay % 60);
  else if (time_delay > 0) sprintf (pstr_result, "%us", time_delay);
  else strcpy (pstr_result, "now");
}

// get temperature source
uint8_t SensorTemperatureSource (char* pstr_source, size_t size_source)
{
  // check parameters
  if ((pstr_source == nullptr) || (size_source == 0)) return UINT8_MAX;

  // retrieve source label
  GetTextIndexed (pstr_source, size_source, sensor_status.type[SENSOR_TYPE_TEMP].source, kSensorSource);

  return sensor_status.type[SENSOR_TYPE_TEMP].source;
}

// read temperature value
float SensorTemperatureRead () { return SensorTemperatureRead (0); }
float SensorTemperatureRead (uint32_t timeout)
{
  uint32_t sensor, index;
  float    temperature = NAN;
  float    humidity    = NAN;

  // if needed, set default timeout
  if (timeout == 0) timeout = sensor_status.type[SENSOR_TYPE_TEMP].timeout;

  // handle reading according to sensor
  switch (sensor_status.type[SENSOR_TYPE_TEMP].source)
  { 
    case SENSOR_SOURCE_DSB:
#ifdef ESP8266
      for (sensor = 0; sensor < DS18X20Data.sensors; sensor++)
      {
        index = ds18x20_sensor[sensor].index;
        if (ds18x20_sensor[index].valid) temperature = ds18x20_sensor[index].temperature;
      }
#else
      Ds18x20Read (0, temperature);
#endif
      break;

    case SENSOR_SOURCE_DHT:
      temperature = Dht[0].t;
      break;

    case SENSOR_SOURCE_SHT:
      Sht3xRead ((uint32_t)sht3x_sensors[0].type, temperature, humidity, sht3x_sensors[0].address);
      break;

    case SENSOR_SOURCE_REMOTE:
      if ((sensor_status.type[SENSOR_TYPE_TEMP].timestamp != 0) && (LocalTime () < sensor_status.type[SENSOR_TYPE_TEMP].timestamp + timeout)) temperature = sensor_status.type[SENSOR_TYPE_TEMP].value;
      break;
  }

  // truncate temperature to 0.1 °C
  if (!isnan (temperature)) temperature = round (temperature * 10) / 10;

  return temperature;
}

// get humidity source
uint8_t SensorHumiditySource (char* pstr_source, size_t size_source)
{
  // check parameters
  if ((pstr_source == nullptr) || (size_source == 0)) return UINT8_MAX;

  // retrieve source label
  GetTextIndexed (pstr_source, size_source, sensor_status.type[SENSOR_TYPE_HUMI].source, kSensorSource);
  
  return sensor_status.type[SENSOR_TYPE_HUMI].source;
}

// read humidity level
float SensorHumidityRead () { return SensorHumidityRead (0); }
float SensorHumidityRead (uint32_t timeout)
{
  float humidity    = NAN;
  float temperature = NAN;

  // if needed, set default timeout
  if (timeout == 0) timeout = sensor_status.type[SENSOR_TYPE_HUMI].timeout;

  // handle reading according to sensor
  switch (sensor_status.type[SENSOR_TYPE_HUMI].source)
  { 
    case SENSOR_SOURCE_DHT:
      humidity = Dht[0].h;
      break;

    case SENSOR_SOURCE_SHT:
      Sht3xRead ((uint32_t)sht3x_sensors[0].type, temperature, humidity, sht3x_sensors[0].address);
      break;

    case SENSOR_SOURCE_REMOTE:
      if ((sensor_status.type[SENSOR_TYPE_HUMI].timestamp != 0) && (LocalTime () < sensor_status.type[SENSOR_TYPE_HUMI].timestamp + timeout)) humidity = sensor_status.type[SENSOR_TYPE_HUMI].value;
      break;
  }

  return humidity;
}

// get movement source
uint8_t SensorPresenceSource (char* pstr_source, size_t size_source)
{
  // check parameters
  if ((pstr_source == nullptr) || (size_source == 0)) return UINT8_MAX;

  // retrieve source label
  GetTextIndexed (pstr_source, size_source, sensor_status.type[SENSOR_TYPE_PRES].source, kSensorSource);
  
  return sensor_status.type[SENSOR_TYPE_PRES].source;
}

// read movement detection status (timeout in sec.)
bool SensorPresenceDetected (uint32_t timeout)
{
  uint32_t time_now = LocalTime ();

  // if some data are missing or not available, declare presence
  if ((time_now == 0) || (time_now == UINT32_MAX)) return true;
  if (sensor_status.type[SENSOR_TYPE_PRES].source == SENSOR_SOURCE_NONE) return true;
  if (sensor_status.type[SENSOR_TYPE_PRES].timestamp == 0) return true;

  // if needed, set default timeout
  if (timeout == 0) timeout = sensor_status.type[SENSOR_TYPE_PRES].timeout;

  // check for timeout
  return (time_now < sensor_status.type[SENSOR_TYPE_PRES].timestamp + timeout);
}

// get delay since last movement was detected
uint32_t SensorPresenceDelaySinceDetection ()
{
  uint32_t result = UINT32_MAX;

  if (sensor_status.type[SENSOR_TYPE_PRES].timestamp != 0) result = LocalTime () - sensor_status.type[SENSOR_TYPE_PRES].timestamp;

  return result;
}

// suspend presence detection
void SensorPresenceSuspendDetection (const uint32_t duration)
{
  // set timestamp to ignore sensor state
  sensor_status.time_ignore = LocalTime () + duration;
}

// reset presence detection
void SensorPresenceResetDetection ()
{
  sensor_status.type[SENSOR_TYPE_PRES].timestamp = LocalTime ();
}

// update sensor condition every second
void SensorEverySecond ()
{
  bool     prev_detected, new_detected;
  uint32_t time_now = LocalTime ();

  // ---------------------
  // local presence sensor
  // ---------------------
  if (sensor_status.type[SENSOR_TYPE_PRES].source < SENSOR_SOURCE_REMOTE)
  {
    // if presence detection is suspended
    if ((sensor_status.time_ignore != 0) && (sensor_status.time_ignore < time_now)) sensor_status.time_ignore = 0;
 
    // check according to sensor type
    switch (sensor_status.type[SENSOR_TYPE_PRES].source)
    {
      // generic sensor
      case SENSOR_SOURCE_COUNTER:
        new_detected = digitalRead (Pin (GPIO_CNTR1, SENSOR_PRESENCE_INDEX));
        break;

      case SENSOR_SOURCE_SERIAL:
        switch (sensor_config.presence)
        {
#ifdef USE_LD1115
          case SENSOR_PRESENCE_LD1115:
            new_detected = LD1115GetGlobalDetectionStatus (2);
            break;
#endif      // USE_LD1115

#ifdef USE_LD1125
          case SENSOR_PRESENCE_LD1125:
            new_detected = LD1125GetGlobalDetectionStatus (2);
            break;
#endif      // USE_LD1125

#ifdef USE_LD2410
          case SENSOR_PRESENCE_LD2410:
            new_detected = LD2410GetGlobalDetectionStatus (2);
            break;
#endif      // USE_LD2410

          default:
            new_detected = false;
            break;
        }
        break;
    }

    // if not in ignore timeframe
    if (sensor_status.time_ignore == 0)
    {
      // if presence detected, update timestamp
      if (new_detected) sensor_status.type[SENSOR_TYPE_PRES].timestamp = time_now;

      // check for timeout
      new_detected = (time_now <= sensor_status.type[SENSOR_TYPE_PRES].timestamp + sensor_status.type[SENSOR_TYPE_PRES].timeout);

      // if detection status changed, update sensor status
      prev_detected = (sensor_status.type[SENSOR_TYPE_PRES].value == 1);
      if (prev_detected != new_detected)
      {
        if (new_detected) sensor_status.type[SENSOR_TYPE_PRES].value = 1;
          else sensor_status.type[SENSOR_TYPE_PRES].value = 0;
        SensorShowJSON (false);
      }
    }
  }
}

// check and update MQTT power subsciption after disconnexion
void SensorMqttSubscribe ()
{
  // if topic is defined, subscribe to remote temperature
  if (sensor_config.mqtt[SENSOR_TYPE_TEMP].topic.length () > 0)
  {
    // subscribe to sensor topic
    MqttSubscribe (sensor_config.mqtt[SENSOR_TYPE_TEMP].topic.c_str ());
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Subscribed remote temperature from %s"), sensor_config.mqtt[SENSOR_TYPE_TEMP].topic.c_str ());
  }

  // if topic is defined, subscribe to remote humidity
  if (sensor_config.mqtt[SENSOR_TYPE_HUMI].topic.length () > 0)
  {
    // subscribe to sensor topic
    MqttSubscribe (sensor_config.mqtt[SENSOR_TYPE_HUMI].topic.c_str ());
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Subscribed remote humidity from %s"), sensor_config.mqtt[SENSOR_TYPE_HUMI].topic.c_str ());
  }

  // if topic is defined, subscribe to remote movement
  if (sensor_config.mqtt[SENSOR_TYPE_PRES].topic.length () > 0)
  {
    // subscribe to sensor topic
    MqttSubscribe (sensor_config.mqtt[SENSOR_TYPE_PRES].topic.c_str ());
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Subscribed remote movement from %s"), sensor_config.mqtt[SENSOR_TYPE_PRES].topic.c_str ());
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
  uint32_t time_now;

  // read timestamp
  time_now = LocalTime ();

  // if dealing with temperature topic
  is_topic = (sensor_config.mqtt[SENSOR_TYPE_TEMP].topic == XdrvMailbox.topic);
  if (is_topic)
  {
    // log
    is_found = true;
    AddLog (LOG_LEVEL_DEBUG, PSTR ("SEN: Received %s"), XdrvMailbox.topic);

    // look for temperature key
    is_key = SensorGetJsonKey (XdrvMailbox.data, sensor_config.mqtt[SENSOR_TYPE_TEMP].key.c_str (), str_value, sizeof (str_value));
    if (is_key)
    {
      // if needed, set remote source for temperature
      if (sensor_status.type[SENSOR_TYPE_TEMP].source == SENSOR_SOURCE_NONE) sensor_status.type[SENSOR_TYPE_TEMP].source = SENSOR_SOURCE_REMOTE;

      // save remote value
      sensor_status.type[SENSOR_TYPE_TEMP].value = atof (str_value) + ((float)Settings->temp_comp / 10);
      sensor_status.type[SENSOR_TYPE_TEMP].timestamp = time_now;

      // log
      is_found = true;
      AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Remote %s °C"), str_value);
    }
  }

  // if dealing with humidity topic
  is_topic = (sensor_config.mqtt[SENSOR_TYPE_HUMI].topic == XdrvMailbox.topic);
  if (is_topic)
  {
    // log
    is_found = true;
    AddLog (LOG_LEVEL_DEBUG, PSTR ("SEN: Received %s"), XdrvMailbox.topic);

    // look for humidity key
    is_key = SensorGetJsonKey (XdrvMailbox.data, sensor_config.mqtt[SENSOR_TYPE_HUMI].key.c_str (), str_value, sizeof (str_value));
    if (is_key)
    {
      // if needed, set remote source for humidity
      if (sensor_status.type[SENSOR_TYPE_HUMI].source == SENSOR_SOURCE_NONE) sensor_status.type[SENSOR_TYPE_HUMI].source = SENSOR_SOURCE_REMOTE;

      // save remote value
      sensor_status.type[SENSOR_TYPE_HUMI].value = atof (str_value);
      sensor_status.type[SENSOR_TYPE_HUMI].timestamp = time_now;

      // log
      AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Remote humidity is %s %%"), str_value);
    }
  }

  // if dealing with movement topic
  is_topic = (sensor_config.mqtt[SENSOR_TYPE_PRES].topic == XdrvMailbox.topic);
  if (is_topic)
  {
    // log
    is_found = true;
    AddLog (LOG_LEVEL_DEBUG, PSTR ("SEN: Received %s"), XdrvMailbox.topic);

    // look for movement key
    is_key = SensorGetJsonKey (XdrvMailbox.data, sensor_config.mqtt[SENSOR_TYPE_PRES].key.c_str (), str_value, sizeof (str_value));
    if (is_key)
    {
      // if needed, set remote source for movement
      if (sensor_status.type[SENSOR_TYPE_PRES].source == SENSOR_SOURCE_NONE) sensor_status.type[SENSOR_TYPE_PRES].source = SENSOR_SOURCE_REMOTE;

      // read presence detection
      detected = ((strcmp (str_value, "1") == 0) || (strcmp (str_value, "on") == 0) || (strcmp (str_value, "ON") == 0));
      if (detected) sensor_status.type[SENSOR_TYPE_PRES].value = 1;
        else sensor_status.type[SENSOR_TYPE_PRES].value = 0;
      if (detected) sensor_status.type[SENSOR_TYPE_PRES].timestamp = time_now;

      // log
      AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Remote movement is %u"), detected);
    }
  }

  return is_found;
}

// Show JSON status (for local sensors and presence sensor)
void SensorShowJSON (bool append)
{
  bool     first = true;
  bool     presence;
  uint32_t delay;
  float    value;
  char     str_value[16];

  // add , in append mode or { in direct publish mode
  if (append) ResponseAppend_P (PSTR (",")); else Response_P (PSTR ("{"));

  // if local presence sensor present
  if (sensor_status.type[SENSOR_TYPE_PRES].source == SENSOR_SOURCE_COUNTER) ResponseAppend_P (PSTR ("\"detect\":%u"), (sensor_status.type[SENSOR_TYPE_PRES].value == 1));

  // in append mode add all sensor data
  if (append) 
  {
    // if local presence sensor present
    if (sensor_status.type[SENSOR_TYPE_PRES].source < SENSOR_SOURCE_REMOTE)
    {
      if (sensor_status.type[SENSOR_TYPE_PRES].timestamp != 0) ResponseAppend_P (PSTR (",\"delay\":%u"), LocalTime () - sensor_status.type[SENSOR_TYPE_PRES].timestamp);
      first = false;
    }

    // if local temperature sensor present
    if (sensor_status.type[SENSOR_TYPE_TEMP].source < SENSOR_SOURCE_REMOTE)
    {
      value = SensorTemperatureRead ();
      ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%01_f"), &value);
      if (!first) ResponseAppend_P (PSTR (","));
      ResponseAppend_P (PSTR ("\"temp\":%s"), str_value);
      first = false;
    }

    // if local humidity sensor present
    if (sensor_status.type[SENSOR_TYPE_HUMI].source < SENSOR_SOURCE_REMOTE)
    {
      value = SensorHumidityRead ();
      ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%00_f"), &value);
      if (!first) ResponseAppend_P (PSTR (","));
      ResponseAppend_P (PSTR ("\"humi\":%s"), str_value);
    }
  }

  // publish it if not in append mode
  if (!append)
  {
    ResponseAppend_P (PSTR ("}"));
    MqttPublishTeleSensor ();
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
  char str_time[16];
  char str_value[16];

  // check for remote sensors
  temp_remote = (sensor_status.type[SENSOR_TYPE_TEMP].source == SENSOR_SOURCE_REMOTE);
  humi_remote = (sensor_status.type[SENSOR_TYPE_HUMI].source == SENSOR_SOURCE_REMOTE);
  pres_remote = (sensor_status.type[SENSOR_TYPE_PRES].source == SENSOR_SOURCE_REMOTE);

  // calculate last presence dectetion delay
  SensorGetDelayText (sensor_status.type[SENSOR_TYPE_PRES].timestamp, str_time, sizeof (str_time));

  // if needed, display remote sensors
  if (temp_remote || humi_remote || pres_remote)
  {
    WSContentSend_PD (PSTR ("<div style='display:flex;font-size:16px;text-align:center;margin-top:4px;padding:2px 6px;background:#333333;border-radius:8px;'>\n"));
    WSContentSend_PD (PSTR ("<div style='width:26%%;padding:0px;text-align:left;font-size:16px;font-weight:bold;'>Remote</div>\n"));
    WSContentSend_PD (PSTR ("<div style='width:74%%;padding:0px;'>\n"));

    // remote temperature
    if (temp_remote)
    {
      ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &sensor_status.type[SENSOR_TYPE_TEMP].value);
      WSContentSend_PD (PSTR ("<span style='float:right;margin-left:8px;'>🌡️%s °C</span>\n"), str_value);
    }

    // remote humidity
    if (humi_remote)
    {
      ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &sensor_status.type[SENSOR_TYPE_HUMI].value);
      WSContentSend_PD (PSTR ("<span style='float:right;margin-left:8px;'>💧%s %%</span>\n"), str_value);
    }

    // remote presence
    if (pres_remote)
    {
      WSContentSend_PD (PSTR ("<span style='float:left;margin-top:2px;padding:0px 12px;font-size:12px;border-radius:6px;"));
      if (SensorPresenceDetected (0)) WSContentSend_PD (PSTR ("background:#D00;"));
        else WSContentSend_PD (PSTR ("background:#444;color:grey;"));
      WSContentSend_PD (PSTR ("'>%s</span>\n"), str_time);
    }

    WSContentSend_PD (PSTR ("</div>\n"));
    WSContentSend_PD (PSTR ("</div>\n"));
  }

  // handle local presence sensor
  if (sensor_status.type[SENSOR_TYPE_PRES].source == SENSOR_SOURCE_COUNTER)
  {
    // set data according to sensor
    switch (sensor_config.presence)
    {
      case SENSOR_PRESENCE_RCWL0516:
        GetTextIndexed (str_type, sizeof (str_type), SENSOR_PRESENCE_RCWL0516, kSensorPresenceModel);
        strcpy (str_value, "0 - 7m");
        result = (sensor_status.type[SENSOR_TYPE_PRES].value == 1);
        break;

      case SENSOR_PRESENCE_HWMS03:
        GetTextIndexed (str_type, sizeof (str_type), SENSOR_PRESENCE_HWMS03, kSensorPresenceModel);
        strcpy (str_value, "0.5 - 10m");
        result = (sensor_status.type[SENSOR_TYPE_PRES].value == 1);
        break;

      default:
        strcpy (str_type,  "");
        strcpy (str_value, "");
        result = false;
        break;
    }

    // if local presence sensor is detected, display status
    if (strlen (str_type) > 0)
    {
      WSContentSend_PD (PSTR ("<div style='display:flex;font-size:10px;text-align:center;margin-top:4px;padding:2px 6px;background:#333333;border-radius:8px;'>\n"));
      WSContentSend_PD (PSTR ("<div style='width:40%%;padding:0px;text-align:left;font-size:12px;font-weight:bold;'>%s</div>\n"), str_type);
      WSContentSend_PD (PSTR ("<div style='width:30%%;padding:0px;border-radius:6px;"));
      if (result) WSContentSend_PD (PSTR ("background:#D00;'>%s</div>\n"), str_time);
      else WSContentSend_PD (PSTR ("background:#444;color:grey;'>%s</div>\n"), str_time);
      WSContentSend_PD (PSTR ("<div style='width:30%%;padding:2px 0px 0px 0px;text-align:right;'>%s</div>\n"), str_value);
      WSContentSend_PD (PSTR ("</div>\n"));
    }
  }
}

// remote sensor configuration web page
void SensorWebConfigurePage ()
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
    WebGetArg (D_CMND_SENSOR_TEMP_DRIFT, str_text, sizeof (str_text));
    if (strlen (str_text) > 0)
    {
      drift = atof (str_text) * 10;
      Settings->temp_comp = (int8_t)drift;
    }

    // set temperature timeout according to 'ttime' parameter
    WebGetArg (D_CMND_SENSOR_TEMP_TIMEOUT, str_text, sizeof (str_text));
    sensor_status.type[SENSOR_TYPE_TEMP].timeout = (uint16_t)atoi (str_text);

    // set temperature topic according to 'ttopic' parameter
    WebGetArg (D_CMND_SENSOR_TEMP_TOPIC, str_text, sizeof (str_text));
    sensor_config.mqtt[SENSOR_TYPE_TEMP].topic = str_text;

    // set temperature key according to 'tkey' parameter
    WebGetArg (D_CMND_SENSOR_TEMP_KEY, str_text, sizeof (str_text));
    sensor_config.mqtt[SENSOR_TYPE_TEMP].key = str_text;

    // set humidity timeout according to 'htime' parameter
    WebGetArg (D_CMND_SENSOR_HUMI_TIMEOUT, str_text, sizeof (str_text));
    sensor_status.type[SENSOR_TYPE_HUMI].timeout = (uint16_t)atoi (str_text);

    // set humidity topic according to 'htopic' parameter
    WebGetArg (D_CMND_SENSOR_HUMI_TOPIC, str_text, sizeof (str_text));
    sensor_config.mqtt[SENSOR_TYPE_HUMI].topic = str_text;

    // set humidity key according to 'hkey' parameter
    WebGetArg (D_CMND_SENSOR_HUMI_KEY, str_text, sizeof (str_text));
    sensor_config.mqtt[SENSOR_TYPE_HUMI].key = str_text;

    // set presence timeout according to 'ptime' parameter
    WebGetArg (D_CMND_SENSOR_PRES_TIMEOUT, str_text, sizeof (str_text));
    sensor_status.type[SENSOR_TYPE_PRES].timeout = (uint32_t)atoi (str_text);

    // set presence topic according to 'ptopic' parameter
    WebGetArg (D_CMND_SENSOR_PRES_TOPIC, str_text, sizeof (str_text));
    sensor_config.mqtt[SENSOR_TYPE_PRES].topic = str_text;

    // set presence key according to 'pkey' parameter
    WebGetArg (D_CMND_SENSOR_PRES_KEY, str_text, sizeof (str_text));
    sensor_config.mqtt[SENSOR_TYPE_PRES].key = str_text;

    // set sensor type according to 'ptype' parameter
    WebGetArg (D_CMND_SENSOR_PRES_TYPE, str_text, sizeof (str_text));
    sensor_config.presence = (uint8_t)atoi (str_text);

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
  itoa (sensor_status.type[SENSOR_TYPE_TEMP].timeout, str_value, 10);
  WSContentSend_P (SENSOR_FIELD_CONFIG, D_SENSOR_TIMEOUT, "sec.", D_CMND_SENSOR_TEMP_TIMEOUT, D_CMND_SENSOR_TEMP_TIMEOUT, 1, 3600, "1", str_value);
  WSContentSend_P (PSTR ("</p>\n"));

  WSContentSend_P (PSTR ("<hr>\n"));

  // topic and key
  WSContentSend_P (PSTR ("<p>\n"));

  WSContentSend_P (SENSOR_FIELD_INPUT, D_SENSOR_TOPIC, D_CMND_SENSOR_TEMP_TOPIC, D_CMND_SENSOR_TEMP_TOPIC, sensor_config.mqtt[SENSOR_TYPE_TEMP].topic.c_str ());
  WSContentSend_P (SENSOR_FIELD_INPUT, D_SENSOR_KEY, D_CMND_SENSOR_TEMP_KEY, D_CMND_SENSOR_TEMP_KEY, sensor_config.mqtt[SENSOR_TYPE_TEMP].key.c_str ());

  WSContentSend_P (PSTR ("</p>\n"));
  WSContentSend_P (SENSOR_FIELDSET_STOP);

  //   remote humidity  
  // ----------------------

  WSContentSend_P (SENSOR_FIELDSET_START, "💧", D_SENSOR_HUMIDITY);

  // timeout 
  WSContentSend_P (PSTR ("<p>\n"));
  itoa (sensor_status.type[SENSOR_TYPE_HUMI].timeout, str_value, 10);
  WSContentSend_P (SENSOR_FIELD_CONFIG, D_SENSOR_TIMEOUT, "sec.", D_CMND_SENSOR_HUMI_TIMEOUT, D_CMND_SENSOR_HUMI_TIMEOUT, 1, 3600, "1", str_value);
  WSContentSend_P (PSTR ("</p>\n"));

  WSContentSend_P (PSTR ("<hr>\n"));

  // topic and key
  WSContentSend_P (PSTR ("<p>\n"));

  WSContentSend_P (SENSOR_FIELD_INPUT, D_SENSOR_TOPIC, D_CMND_SENSOR_HUMI_TOPIC, D_CMND_SENSOR_HUMI_TOPIC, sensor_config.mqtt[SENSOR_TYPE_HUMI].topic.c_str ());
  WSContentSend_P (SENSOR_FIELD_INPUT, D_SENSOR_KEY, D_CMND_SENSOR_HUMI_KEY, D_CMND_SENSOR_HUMI_KEY, sensor_config.mqtt[SENSOR_TYPE_HUMI].key.c_str ());

  WSContentSend_P (PSTR ("</p>\n"));
  WSContentSend_P (SENSOR_FIELDSET_STOP);

  //   remote presence  
  // ----------------------

  WSContentSend_P (SENSOR_FIELDSET_START, "🧑", D_SENSOR_PRESENCE);

  // timeout 
  WSContentSend_P (PSTR ("<p>\n"));
  sprintf (str_value, "%u", sensor_status.type[SENSOR_TYPE_PRES].timeout);
  WSContentSend_P (SENSOR_FIELD_CONFIG, D_SENSOR_TIMEOUT, "sec.", D_CMND_SENSOR_PRES_TIMEOUT, D_CMND_SENSOR_PRES_TIMEOUT, 1, 3600, "1", str_value);
  WSContentSend_P (PSTR ("</p>\n"));

  WSContentSend_P (PSTR ("<hr>\n"));
  
  // topic and key 
  WSContentSend_P (PSTR ("<p>\n"));

  WSContentSend_P (SENSOR_FIELD_INPUT, D_SENSOR_TOPIC, D_CMND_SENSOR_PRES_TOPIC, D_CMND_SENSOR_PRES_TOPIC, sensor_config.mqtt[SENSOR_TYPE_PRES].topic.c_str ());
  WSContentSend_P (SENSOR_FIELD_INPUT, D_SENSOR_KEY, D_CMND_SENSOR_PRES_KEY, D_CMND_SENSOR_PRES_KEY, sensor_config.mqtt[SENSOR_TYPE_PRES].key.c_str ());

  WSContentSend_P (PSTR ("</p>\n"));
  WSContentSend_P (SENSOR_FIELDSET_STOP);

  //   presence sensors  
  // ----------------------

  WSContentSend_P (SENSOR_FIELDSET_START, "⚙️", "Presence Sensor");

  for (index = 0; index < SENSOR_PRESENCE_MAX; index ++)
  {
    GetTextIndexed (str_text, sizeof (str_text), index, kSensorPresenceModel);
    if (sensor_config.presence == index) strcpy_P (str_value, PSTR ("checked")); else strcpy (str_value, "");
    WSContentSend_P (PSTR ("<p><input type='radio' name='%s' value='%d' %s>%s</p>\n"), D_CMND_SENSOR_PRES_TYPE, index, str_value, str_text);
  }

#ifdef USE_LD1115
//  if (sensor_config.presence == SENSOR_PRESENCE_LD1115) strcpy_P (str_text, PSTR ("checked")); else strcpy (str_text, "");
//  WSContentSend_P (PSTR ("<p><input type='radio' name='%s' value='%d' %s>%s</p>\n"), D_CMND_SENSOR_PRES_TYPE, SENSOR_PRESENCE_LD1115, str_text, D_LD1115_NAME);
#endif        // USE_LD1115

#ifdef USE_LD1125
//  if (sensor_config.presence == SENSOR_PRESENCE_LD1125) strcpy_P (str_text, PSTR ("checked")); else strcpy (str_text, "");
//  WSContentSend_P (PSTR ("<p><input type='radio' name='%s' value='%d' %s>%s</p>\n"), D_CMND_SENSOR_PRES_TYPE, SENSOR_PRESENCE_LD1125, str_text, D_LD1125_NAME);
#endif        // USE_LD1125

#ifdef USE_LD2410
//  if (sensor_config.presence == SENSOR_PRESENCE_LD2410) strcpy_P (str_text, PSTR ("checked")); else strcpy (str_text, "");
//  WSContentSend_P (PSTR ("<p><input type='radio' name='%s' value='%d' %s>%s</p>\n"), D_CMND_SENSOR_PRES_TYPE, SENSOR_PRESENCE_LD2410, str_text, D_LD2410_NAME);
#endif        // USE_LD2410

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

bool Xsns99 (uint32_t function)
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
      SensorEverySecond ();
      break;
    case FUNC_JSON_APPEND:
      SensorShowJSON (true);
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
