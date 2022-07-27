/*
  xsns_125_sensor.ino - Collect temperature, humidity and movement
  from local or remote (thru MQTT topic/key) sensors.

  Copyright (C) 2020  Nicolas Bernaerts
    15/01/2020 - v1.0 - Creation with management of remote MQTT sensor
    23/04/2021 - v1.1 - Remove use of String to avoid heap fragmentation
    18/06/2021 - v1.2 - Bug fixes
    15/11/2021 - v1.3 - Add LittleFS management
    19/11/2021 - v1.4 - Handle drift and priority between local and remote sensors
    01/03/2022 - v2.0 - Complete rewrite to handle local drift and priority between local and remote sensors
    26/06/2022 - v3.0 - Tasmota 12 compatibility
                        Management of HLK-LD11xx sensors
                      
  If no littleFS partition is available, settings are stored using SettingsTextIndex :
    * SET_SENSOR_TEMP_TOPIC
    * SET_SENSOR_TEMP_KEY
    * SET_SENSOR_HUMI_TOPIC
    * SET_SENSOR_HUMI_KEY
    * SET_SENSOR_MOVE_TOPIC
    * SET_SENSOR_MOVE_KEY
  These keys should be added in tasmota.h at the end of "enum SettingsTextIndex" section just before SET_MAX :

        #ifndef USE_UFILESYS
          SET_OFFLOAD_TOPIC, SET_OFFLOAD_KEY_INST, SET_OFFLOAD_KEY_MAX,
          SET_SENSOR_TEMP_TOPIC, SET_SENSOR_TEMP_KEY,
          SET_SENSOR_HUMI_TOPIC, SET_SENSOR_HUMI_KEY,
          SET_SENSOR_MOVE_TOPIC, SET_SENSOR_MOVE_KEY,
        #endif 	// USE_UFILESYS
  
  If LittleFS partition is available, settings are stored in sensor.cfg

  To use the remote sensors, you need to add these function calls in any one of your xdrv_ file :
    FUNC_MQTT_SUBSCRIBE: SensorMqttSubscribe ()
    FUNC_MQTT_DATA:      SensorMqttData ()

  You can then access values thru :
    * float SensorReadTemperature ()
    * float SensorReadHumidity ()
    * bool SensorReadMovement ()
    * uint32_t SensorLastMovement ()

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
 *              Remote Sensor
\*************************************************/

//#ifdef USE_REMOTE_SENSOR

#define XSNS_125                      125

#define SENSOR_TIMEOUT_TEMPERATURE    30            // 30 seconds
#define SENSOR_TIMEOUT_HUMIDITY       30            // 30 seconds
#define SENSOR_TIMEOUT_MOVEMENT       5             // 5 seconds

#define SENSOR_TEMP_DRIFT_MAX         10
#define SENSOR_TEMP_DRIFT_STEP        0.1

#define SENSOR_MOVEMENT_INDEX         7             // switch index for movement detection (0=switch1 ... 7=switch8)

#define D_PAGE_SENSOR                 "rsensor"

#define D_CMND_SENSOR_PREFIX          "sn_"
#define D_CMND_SENSOR_HELP            "help"
#define D_CMND_SENSOR_TEMP_TOPIC      "ttopic"
#define D_CMND_SENSOR_TEMP_KEY        "tkey"
#define D_CMND_SENSOR_TEMP_VALUE      "tval"
#define D_CMND_SENSOR_TEMP_DRIFT      "tdrift"
#define D_CMND_SENSOR_HUMI_TOPIC      "htopic"
#define D_CMND_SENSOR_HUMI_KEY        "hkey"
#define D_CMND_SENSOR_HUMI_VALUE      "hval"
#define D_CMND_SENSOR_MOVE_TOPIC      "mtopic"
#define D_CMND_SENSOR_MOVE_KEY        "mkey"
#define D_CMND_SENSOR_MOVE_VALUE      "mval"

#define D_SENSOR                      "Sensor"
#define D_SENSOR_CONFIGURE            "Configure"
#define D_SENSOR_TEMPERATURE          "Temperature"
#define D_SENSOR_HUMIDITY             "Humidity"
#define D_SENSOR_MOVEMENT             "Movement"
#define D_SENSOR_CORRECTION           "Correction"
#define D_SENSOR_TOPIC                "Topic"
#define D_SENSOR_KEY                  "JSON Key"

#define D_SENSOR_UNDEFINED            "Undef."
#define D_SENSOR_LOCAL                "Local"
#define D_SENSOR_REMOTE               "Remote"

#ifdef USE_UFILESYS
#define D_SENSOR_FILE_CFG             "/sensor.cfg"
#endif     // USE_UFILESYS

// sensor type
enum SensorType { SENSOR_TYPE_TEMP, SENSOR_TYPE_HUMI, SENSOR_TYPE_MOVE, SENSOR_TYPE_MAX };

// remote sensor sources
enum SensorSource { SENSOR_SOURCE_DSB, SENSOR_SOURCE_DHT, SENSOR_SOURCE_SWITCH, SENSOR_SOURCE_REMOTE, SENSOR_SOURCE_MAX };
const char kSensorSource[] PROGMEM = "Local|Local|Local|Remote|Undef.";                                                           // device source labels

// remote sensor commands
const char kSensorCommands[] PROGMEM = D_CMND_SENSOR_PREFIX "|" D_CMND_SENSOR_HELP "|" D_CMND_SENSOR_TEMP_TOPIC "|" D_CMND_SENSOR_TEMP_KEY "|" D_CMND_SENSOR_TEMP_VALUE "|" D_CMND_SENSOR_TEMP_DRIFT "|" D_CMND_SENSOR_HUMI_TOPIC "|" D_CMND_SENSOR_HUMI_KEY "|" D_CMND_SENSOR_HUMI_VALUE "|" D_CMND_SENSOR_MOVE_TOPIC "|" D_CMND_SENSOR_MOVE_KEY "|" D_CMND_SENSOR_MOVE_VALUE;
void (* const SensorCommand[])(void) PROGMEM = { &CmndSensorHelp, &CmndSensorTemperatureTopic, &CmndSensorTemperatureKey, &CmndSensorTemperatureValue, &CmndSensorTemperatureDrift, &CmndSensorHumidityTopic, &CmndSensorHumidityKey, &CmndSensorHumidityValue, &CmndSensorMovementTopic, &CmndSensorMovementKey, &CmndSensorMovementValue };

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
  String topic;                 // remote sensor topic
  String key;                   // remote sensor key
};
sensor_mqtt sensor_config[SENSOR_TYPE_MAX];      // remote sensors configuration

// status
struct sensor_data
{
  float    value;               // current temperature value
  uint8_t  source;              // source of data (local sensor type)
  uint32_t index;               // index associated with the sensor (used for movement)
  uint32_t time_update;         // last time remote value was updated
};
sensor_data sensor_status[SENSOR_TYPE_MAX];      // sensor status

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
    sensor_status[sensor].source      = SENSOR_SOURCE_MAX;
    sensor_status[sensor].value       = NAN;
    sensor_status[sensor].index       = UINT32_MAX;
    sensor_status[sensor].time_update = UINT32_MAX;
  }

  // temperature : check for local source
  if (PinUsed (GPIO_DHT11) || PinUsed (GPIO_DHT22) || PinUsed (GPIO_SI7021))
  {
    sensor_status[SENSOR_TYPE_TEMP].source = SENSOR_SOURCE_DHT;
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: DHT temperature sensor detected"));
  }

  else if (PinUsed (GPIO_DSB))
  {
    sensor_status[SENSOR_TYPE_TEMP].source = SENSOR_SOURCE_DSB;
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: DS18x20 temperature sensor detected"));

    // force pullup for single DS18B20
    Settings->flag3.ds18x20_internal_pullup = 1;
  }

  // humidity : check for local source
  if (PinUsed (GPIO_DHT11) || PinUsed (GPIO_DHT22) || PinUsed (GPIO_SI7021))
  {
    sensor_status[SENSOR_TYPE_HUMI].source = SENSOR_SOURCE_DHT;
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: DHT humidity sensor detected"));
  }

  // movement : check for local source
  if (PinUsed (GPIO_SWT1, SENSOR_MOVEMENT_INDEX))
  {
    sensor_status[SENSOR_TYPE_MOVE].index  = SENSOR_MOVEMENT_INDEX;
    sensor_status[SENSOR_TYPE_MOVE].source = SENSOR_SOURCE_SWITCH;
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: generic movement sensor detected"));
  }

  // movement : HLK-LD11xx sensor
#ifdef USE_HLKLD
  if (PinUsed (GPIO_TXD) && PinUsed (GPIO_RXD))
  {
    // if switch is not declared, declare it
    if (sensor_status[SENSOR_TYPE_MOVE].index == UINT32_MAX) index = SENSOR_MOVEMENT_INDEX;

    // init HLK-LD11xx device
    if (HLKLDInitDevice (index))
    {
      sensor_status[SENSOR_TYPE_MOVE].source = SENSOR_SOURCE_SWITCH;
      AddLog (LOG_LEVEL_INFO, PSTR ("SEN: HLK-LD movement sensor detected"));
    }
  }
#endif    // USE_HLKLD

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: sn_help to get help on sensor manager commands"));

  // load configuration
  SensorLoadConfig ();
}

// load configuration parameters
void SensorLoadConfig () 
{
  char str_text[128];

#ifdef USE_UFILESYS

  // retrieve saved settings from flash filesystem
  sensor_config[SENSOR_TYPE_TEMP].topic = UfsCfgLoadKey (D_SENSOR_FILE_CFG, D_CMND_SENSOR_TEMP_TOPIC);
  sensor_config[SENSOR_TYPE_TEMP].key   = UfsCfgLoadKey (D_SENSOR_FILE_CFG, D_CMND_SENSOR_TEMP_KEY);
  sensor_config[SENSOR_TYPE_HUMI].topic = UfsCfgLoadKey (D_SENSOR_FILE_CFG, D_CMND_SENSOR_HUMI_TOPIC);
  sensor_config[SENSOR_TYPE_HUMI].key   = UfsCfgLoadKey (D_SENSOR_FILE_CFG, D_CMND_SENSOR_HUMI_KEY);
  sensor_config[SENSOR_TYPE_MOVE].topic = UfsCfgLoadKey (D_SENSOR_FILE_CFG, D_CMND_SENSOR_MOVE_TOPIC);
  sensor_config[SENSOR_TYPE_MOVE].key   = UfsCfgLoadKey (D_SENSOR_FILE_CFG, D_CMND_SENSOR_MOVE_KEY);

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Config from LittleFS"));

#else       // No LittleFS

  // retrieve saved settings from flash memory
  sensor_config[SENSOR_TYPE_TEMP].topic = SettingsText (SET_SENSOR_TEMP_TOPIC);
  sensor_config[SENSOR_TYPE_TEMP].key   = SettingsText (SET_SENSOR_TEMP_KEY);
  sensor_config[SENSOR_TYPE_HUMI].topic = SettingsText (SET_SENSOR_HUMI_TOPIC);
  sensor_config[SENSOR_TYPE_HUMI].key   = SettingsText (SET_SENSOR_HUMI_KEY);
  sensor_config[SENSOR_TYPE_MOVE].topic = SettingsText (SET_SENSOR_MOVE_TOPIC);
  sensor_config[SENSOR_TYPE_MOVE].key   = SettingsText (SET_SENSOR_MOVE_KEY);

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Config from Settings"));

# endif     // USE_UFILESYS
}

// save configuration parameters
void SensorSaveConfig () 
{
#ifdef USE_UFILESYS

  // save settings into flash filesystem
  UfsCfgSaveKey (D_SENSOR_FILE_CFG, D_CMND_SENSOR_TEMP_TOPIC, sensor_config[SENSOR_TYPE_TEMP].topic.c_str (), true);
  UfsCfgSaveKey (D_SENSOR_FILE_CFG, D_CMND_SENSOR_TEMP_KEY,   sensor_config[SENSOR_TYPE_TEMP].key.c_str (), false);
  UfsCfgSaveKey (D_SENSOR_FILE_CFG, D_CMND_SENSOR_HUMI_TOPIC, sensor_config[SENSOR_TYPE_HUMI].topic.c_str (), false);
  UfsCfgSaveKey (D_SENSOR_FILE_CFG, D_CMND_SENSOR_HUMI_KEY,   sensor_config[SENSOR_TYPE_HUMI].key.c_str (), false);
  UfsCfgSaveKey (D_SENSOR_FILE_CFG, D_CMND_SENSOR_MOVE_TOPIC, sensor_config[SENSOR_TYPE_MOVE].topic.c_str (), false);
  UfsCfgSaveKey (D_SENSOR_FILE_CFG, D_CMND_SENSOR_MOVE_KEY,   sensor_config[SENSOR_TYPE_MOVE].key.c_str (), false);

# else       // No LittleFS

  // save settings into flash memory
  SettingsUpdateText (SET_SENSOR_TEMP_TOPIC, sensor_config[SENSOR_TYPE_TEMP].topic.c_str ());
  SettingsUpdateText (SET_SENSOR_TEMP_KEY,   sensor_config[SENSOR_TYPE_TEMP].key.c_str ());
  SettingsUpdateText (SET_SENSOR_HUMI_TOPIC, sensor_config[SENSOR_TYPE_HUMI].topic.c_str ());
  SettingsUpdateText (SET_SENSOR_HUMI_KEY,   sensor_config[SENSOR_TYPE_HUMI].key.c_str ());
  SettingsUpdateText (SET_SENSOR_MOVE_TOPIC, sensor_config[SENSOR_TYPE_MOVE].topic.c_str ());
  SettingsUpdateText (SET_SENSOR_MOVE_KEY,   sensor_config[SENSOR_TYPE_MOVE].key.c_str ());

# endif     // USE_UFILESYS
}

/**************************************************\
 *                  Functions
\**************************************************/

// get temperature source
uint8_t SensorSourceTemperature (char* pstr_label = nullptr, size_t size_label = 0)
{
  // if asked, retrieve source label
  if (pstr_label) GetTextIndexed (pstr_label, size_label, sensor_status[SENSOR_TYPE_TEMP].source, kSensorSource);

  return sensor_status[SENSOR_TYPE_TEMP].source;
}

// read temperature value
float SensorReadTemperature (uint32_t timeout = SENSOR_TIMEOUT_TEMPERATURE)
{
  uint32_t index;
  float    value = NAN;

  switch (sensor_status[SENSOR_TYPE_TEMP].source)
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
      if (sensor_status[SENSOR_TYPE_TEMP].time_update == UINT32_MAX) value = NAN;
      else if (LocalTime () - sensor_status[SENSOR_TYPE_TEMP].time_update > timeout) value = NAN;
      else value = sensor_status[SENSOR_TYPE_TEMP].value + ((float)Settings->temp_comp / 10);
      break;
  }

  // truncate temperature to 0.1 °C
  if (!isnan (value)) value = round (value * 10) / 10;

  return value;
}

// get humidity source
uint8_t SensorSourceHumidity (char* pstr_label = nullptr, size_t size_label = 0)
{
  // if asked, retrieve source label
  if (pstr_label) GetTextIndexed (pstr_label, size_label, sensor_status[SENSOR_TYPE_HUMI].source, kSensorSource);
  
  return sensor_status[SENSOR_TYPE_HUMI].source;
}

// read humidity level
float SensorReadHumidity (uint32_t timeout = SENSOR_TIMEOUT_HUMIDITY)
{
  float value = NAN;

  switch (sensor_status[SENSOR_TYPE_HUMI].source)
  { 
    case SENSOR_SOURCE_DHT:
      value = Dht[0].h;
      break;
    case SENSOR_SOURCE_REMOTE:
      if (sensor_status[SENSOR_TYPE_HUMI].time_update == UINT32_MAX) value = NAN;
      else if (LocalTime () - sensor_status[SENSOR_TYPE_HUMI].time_update > timeout) value = NAN;
      else value = sensor_status[SENSOR_TYPE_HUMI].value;
      break;
  }

  // truncate humidity to 0.1 %
  if (!isnan (value)) value = round (value * 10) / 10;

  return value;
}

// get movement source
uint8_t SensorSourceMovement (char* pstr_label = nullptr, size_t size_label = 0)
{
  // if asked, retrieve source label
  if (pstr_label) GetTextIndexed (pstr_label, size_label, sensor_status[SENSOR_TYPE_MOVE].source, kSensorSource);
  
  return sensor_status[SENSOR_TYPE_MOVE].source;
}

// read movement detection status
bool SensorReadMovement (uint32_t timeout = SENSOR_TIMEOUT_MOVEMENT)
{
  bool result = false;

  if (sensor_status[SENSOR_TYPE_MOVE].time_update == UINT32_MAX) result = false;
  else if (LocalTime () - sensor_status[SENSOR_TYPE_MOVE].time_update < timeout) result = true;

  return result;
}

// update sensor condition every second
void SensorEverySecond ()
{
  // if local movement sensor, update movement detection
  if (sensor_status[SENSOR_TYPE_MOVE].source == SENSOR_SOURCE_SWITCH)
  {
    // if movement detected, update time
    if (Switch.virtual_state[SENSOR_MOVEMENT_INDEX]) sensor_status[SENSOR_TYPE_MOVE].time_update = LocalTime ();
  }
}

// get delay since last movement was detected
uint32_t SensorLastMovement ()
{
  uint32_t delay = UINT32_MAX;

  if (sensor_status[SENSOR_TYPE_MOVE].time_update != UINT32_MAX) delay = LocalTime () - sensor_status[SENSOR_TYPE_MOVE].time_update;

  return delay;
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
  if (sensor_config[SENSOR_TYPE_MOVE].topic.length () > 0)
  {
    // subscribe to sensor topic
    MqttSubscribe (sensor_config[SENSOR_TYPE_MOVE].topic.c_str ());
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Subscribed remote movement from %s"), sensor_config[SENSOR_TYPE_MOVE].topic.c_str ());
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

  // look for temperature key
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
  bool     found = false;
  uint32_t time_now;
  char     str_value[32];

  // read current time
  time_now = LocalTime ();

  // if dealing with temperature topic
  if (sensor_config[SENSOR_TYPE_TEMP].topic == XdrvMailbox.topic)
  {
    // look for temperature key
    if (SensorGetJsonKey (XdrvMailbox.data, sensor_config[SENSOR_TYPE_TEMP].key.c_str (), str_value, sizeof (str_value)))
    {
      // save remote value
      sensor_status[SENSOR_TYPE_TEMP].value = atof (str_value);
      sensor_status[SENSOR_TYPE_TEMP].time_update = time_now;

      // if needed, set remote source for temperature
      if (sensor_status[SENSOR_TYPE_TEMP].source == SENSOR_SOURCE_MAX) sensor_status[SENSOR_TYPE_TEMP].source = SENSOR_SOURCE_REMOTE;

      // log
      AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Remote %s °C"), str_value);
      found = true;
    }
  }

  // if dealing with humidity topic
  if (sensor_config[SENSOR_TYPE_HUMI].topic == XdrvMailbox.topic)
  {
    // look for humidity key
    if (SensorGetJsonKey (XdrvMailbox.data, sensor_config[SENSOR_TYPE_HUMI].key.c_str (), str_value, sizeof (str_value)))
    {
      // save remote value
      sensor_status[SENSOR_TYPE_HUMI].value = atof (str_value);
      sensor_status[SENSOR_TYPE_HUMI].time_update = time_now;

      // if needed, set remote source for humidity
      if (sensor_status[SENSOR_TYPE_HUMI].source == SENSOR_SOURCE_MAX) sensor_status[SENSOR_TYPE_HUMI].source = SENSOR_SOURCE_REMOTE;

      // log
      AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Remote humidity is %s %%"), str_value);
      found = true;
    }
  }

  // if dealing with movement topic
  if (sensor_config[SENSOR_TYPE_MOVE].topic == XdrvMailbox.topic)
  {
    // look for movement key
    if (SensorGetJsonKey (XdrvMailbox.data, sensor_config[SENSOR_TYPE_MOVE].key.c_str (), str_value, sizeof (str_value)))
    {
      // convert off, OFF, on, ON and values to 0 or 1
      if (strcasecmp (str_value, "off") == 0) sensor_status[SENSOR_TYPE_MOVE].value = 0;
      else if (strcasecmp (str_value, "on") == 0) sensor_status[SENSOR_TYPE_MOVE].value = 1;
      else sensor_status[SENSOR_TYPE_MOVE].value = atof (str_value);

      // if movement detected, update detection time
      if (sensor_status[SENSOR_TYPE_MOVE].value > 0) sensor_status[SENSOR_TYPE_MOVE].time_update = time_now;

      // if needed, set remote source for movement
      if (sensor_status[SENSOR_TYPE_MOVE].source == SENSOR_SOURCE_MAX) sensor_status[SENSOR_TYPE_MOVE].source = SENSOR_SOURCE_REMOTE;

      // log
      AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Remote movement is %s"), str_value);
      found = true;
    }
  }

  return found;
}

/***********************************************\
 *                  Commands
\***********************************************/

// timezone help
void CmndSensorHelp ()
{
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: sn_ttopic  = topic of remote temperature sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: sn_tkey    = key of remote temperature sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: sn_tdrift  = temperature correction (in 1/10 of °C)"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: sn_tval    = value of remote temperature sensor"));

  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: sn_htopic  = topic of remote humidity sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: sn_hkey    = key of remote humidity sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: sn_hval    = value of remote humidity sensor"));

  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: sn_mtopic  = topic of remote movement sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: sn_mkey    = key of remote movement sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: sn_mval    = value of remote movement sensor"));

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
  ResponseCmndFloat (SensorReadTemperature (), 1);
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
  ResponseCmndFloat (SensorReadHumidity (), 1);
}

void CmndSensorMovementTopic ()
{
  char str_text[64];

  if (XdrvMailbox.data_len > 0)
  {
    strncpy (str_text, XdrvMailbox.data, min (sizeof (str_text) - 1, XdrvMailbox.data_len));
    sensor_config[SENSOR_TYPE_MOVE].topic = str_text;
    SensorSaveConfig ();
  }
  ResponseCmndChar (sensor_config[SENSOR_TYPE_MOVE].topic.c_str ());
}

void CmndSensorMovementKey ()
{
  char str_text[32];

  if (XdrvMailbox.data_len > 0)
  {
    strncpy (str_text, XdrvMailbox.data, min (sizeof (str_text) - 1, XdrvMailbox.data_len));
    sensor_config[SENSOR_TYPE_MOVE].key = str_text;
    SensorSaveConfig ();
  }
  ResponseCmndChar (sensor_config[SENSOR_TYPE_MOVE].key.c_str ());
}

void CmndSensorMovementValue ()
{
  ResponseCmndNumber ((int)SensorReadMovement ());
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// append remote sensor to main page
void SensorWebSensor ()
{
  char str_value[8];

  // if temperature is remote
  if (sensor_status[SENSOR_TYPE_TEMP].source == SENSOR_SOURCE_REMOTE)
  {
    ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &sensor_status[SENSOR_TYPE_TEMP].value);
    WSContentSend_PD (PSTR ("{s}%s %s{m}%s °C{e}"), D_SENSOR_REMOTE, D_SENSOR_TEMPERATURE, str_value);
  }

  // if humidity is remote
  if (sensor_status[SENSOR_TYPE_HUMI].source == SENSOR_SOURCE_REMOTE)
  {
    ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &sensor_status[SENSOR_TYPE_HUMI].value);
    WSContentSend_PD (PSTR ("{s}%s %s{m}%s %%{e}"), D_SENSOR_REMOTE, D_SENSOR_HUMIDITY, str_value);
  }

  // if movement is remote
  if (sensor_status[SENSOR_TYPE_MOVE].source == SENSOR_SOURCE_REMOTE)
  {
    ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%0_f"), &sensor_status[SENSOR_TYPE_MOVE].value);
    WSContentSend_PD (PSTR ("{s}%s %s{m}%s{e}"), D_SENSOR_REMOTE, D_SENSOR_MOVEMENT, str_value);
  }
}

// remote sensor configuration web page
void SensorWebConfigurePage ()
{
  float drift, step;
  char  str_value[8];
  char  str_text[128];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess ()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg ("save"))
  {
    // set temperature topic according to 'ttopic' parameter
    WebGetArg (D_CMND_SENSOR_TEMP_TOPIC, str_text, sizeof (str_text));
    sensor_config[SENSOR_TYPE_TEMP].topic = str_text;

    // set temperature key according to 'tkey' parameter
    WebGetArg (D_CMND_SENSOR_TEMP_KEY, str_text, sizeof (str_text));
    sensor_config[SENSOR_TYPE_TEMP].key = str_text;

    // get temperature drift according to 'tdrift' parameter
    WebGetArg (D_CMND_SENSOR_TEMP_DRIFT, str_text, sizeof (str_text));
    if (strlen (str_text) > 0)
    {
      drift = atof (str_text) * 10;
      Settings->temp_comp = (int8_t)drift;
    }

    // set humidity topic according to 'htopic' parameter
    WebGetArg (D_CMND_SENSOR_HUMI_TOPIC, str_text, sizeof (str_text));
    sensor_config[SENSOR_TYPE_HUMI].topic = str_text;

    // set humidity key according to 'hkey' parameter
    WebGetArg (D_CMND_SENSOR_HUMI_KEY, str_text, sizeof (str_text));
    sensor_config[SENSOR_TYPE_HUMI].key = str_text;

    // set movement topic according to 'mtopic' parameter
    WebGetArg (D_CMND_SENSOR_MOVE_TOPIC, str_text, sizeof (str_text));
    sensor_config[SENSOR_TYPE_MOVE].topic = str_text;

    // set movement key according to 'mkey' parameter
    WebGetArg (D_CMND_SENSOR_MOVE_KEY, str_text, sizeof (str_text));
    sensor_config[SENSOR_TYPE_MOVE].key = str_text;

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

  //   remote temperature  
  // ----------------------

  WSContentSend_P (SENSOR_FIELDSET_START, "🌡️", D_SENSOR_TEMPERATURE);

  //   Correction 
  WSContentSend_P (PSTR ("<p>\n"));
  drift = (float)Settings->temp_comp / 10;
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &drift);
  step = SENSOR_TEMP_DRIFT_STEP;
  ext_snprintf_P (str_text, sizeof (str_text), PSTR ("%1_f"), &step);
  WSContentSend_P (SENSOR_FIELD_CONFIG, D_SENSOR_CORRECTION, "°C", D_CMND_SENSOR_TEMP_DRIFT, D_CMND_SENSOR_TEMP_DRIFT, - SENSOR_TEMP_DRIFT_MAX, SENSOR_TEMP_DRIFT_MAX, str_text, str_value);
  WSContentSend_P (PSTR ("</p>\n"));

  WSContentSend_P (PSTR ("<hr>\n"));

  WSContentSend_P (PSTR ("<p>\n"));

  WSContentSend_P (SENSOR_FIELD_INPUT, D_SENSOR_TOPIC, D_CMND_SENSOR_TEMP_TOPIC, D_CMND_SENSOR_TEMP_TOPIC, sensor_config[SENSOR_TYPE_TEMP].topic.c_str ());
  WSContentSend_P (SENSOR_FIELD_INPUT, D_SENSOR_KEY, D_CMND_SENSOR_TEMP_KEY, D_CMND_SENSOR_TEMP_KEY, sensor_config[SENSOR_TYPE_TEMP].key.c_str ());

  WSContentSend_P (PSTR ("</p>\n"));
  WSContentSend_P (SENSOR_FIELDSET_STOP);

  //   remote humidity  
  // ----------------------

  WSContentSend_P (SENSOR_FIELDSET_START, "💦", D_SENSOR_HUMIDITY);
  WSContentSend_P (PSTR ("<p>\n"));

  WSContentSend_P (SENSOR_FIELD_INPUT, D_SENSOR_TOPIC, D_CMND_SENSOR_HUMI_TOPIC, D_CMND_SENSOR_HUMI_TOPIC, sensor_config[SENSOR_TYPE_HUMI].topic.c_str ());
  WSContentSend_P (SENSOR_FIELD_INPUT, D_SENSOR_KEY, D_CMND_SENSOR_HUMI_KEY, D_CMND_SENSOR_HUMI_KEY, sensor_config[SENSOR_TYPE_HUMI].key.c_str ());

  WSContentSend_P (PSTR ("</p>\n"));
  WSContentSend_P (SENSOR_FIELDSET_STOP);

  //   remote movement  
  // ----------------------

  WSContentSend_P (SENSOR_FIELDSET_START, "👋", D_SENSOR_MOVEMENT);
  WSContentSend_P (PSTR ("<p>\n"));

  WSContentSend_P (SENSOR_FIELD_INPUT, D_SENSOR_TOPIC, D_CMND_SENSOR_MOVE_TOPIC, D_CMND_SENSOR_MOVE_TOPIC, sensor_config[SENSOR_TYPE_MOVE].topic.c_str ());
  WSContentSend_P (SENSOR_FIELD_INPUT, D_SENSOR_KEY, D_CMND_SENSOR_MOVE_KEY, D_CMND_SENSOR_MOVE_KEY, sensor_config[SENSOR_TYPE_MOVE].key.c_str ());

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
      SensorEverySecond ();
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

//#endif // USE_REMOTE_SENSOR
