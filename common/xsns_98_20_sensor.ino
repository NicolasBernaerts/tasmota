/*
  xsns_99_20_sensor.ino - Generic temperature, humidity and presence sensors
    Reading is possible from local or remote (MQTT topic/key) sensors
    Module should be 99 as with platformio modules 100, 101, 102, ... are compiled before 99

  Copyright (C) 2020-2026  Nicolas Bernaerts
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
    11/09/2024 - v4.7 - Based on Tasmota 14.0
                        Use new options configuration page
    14/09/2024 - v4.8 - New sensor display in main page
    20/01/2025 - v4.9 - Correct bug of negative temperature digit
    09/02/2025 - v5.0 - Ignore local sensors if remote one is configured
    11/11/2025 - v6.0 - Rewrite to separate data and historisation
    29/06/2026 - v6.1 - Integrate all Hilink presence detector drivers

  Configuration values are stored in :
    - Settings->rf_code[0][0]    : temperature validity timeout
    - Settings->rf_code[0][1]    : humidity    validity timeout
    - Settings->rf_code[0][2]    : presence    validity timeout
  
  Validity timeout is handled thru ranges :
    - 1   ... 99  : seconds
    - 100 ... 199 : 100 to 1090 seconds
    - 200 ... 255 : 18mn to 73 mn 

  Remote MQTT settings are stored using Settings :
    * SET_SENSOR_TEMP_TOPIC           // remote temperature topic
    * SET_SENSOR_TEMP_KEY             // remote temperature key
    * SET_SENSOR_HUMI_TOPIC           // remote humidity topic
    * SET_SENSOR_HUMI_KEY             // remote humidity key
    * SET_SENSOR_PRES_TOPIC           // remote presence topic
    * SET_SENSOR_PRES_KEY             // remote presence key

  Handled local sensor are :
    * Temperature = DHT11, AM2301, SI7021, SHT30 or DS18x20
    * Humidity    = DHT11, AM2301, SI7021 or SHT30
    * Presence    = Generic presence/movement detector declared as Interrupt 1
                    HLK-LDxxxx serial sensors connected thru serial port (Rx 2410 and Tx 2410)
  
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

#define XDRV_98              98
#define XSNS_98              98

// sensor commands
const char kSensCommands[]         PROGMEM = D_CMND_SENSOR_SENS "|"         "|"  ;
void (* const SensCommand[])(void) PROGMEM = {                  &CmndSensorHelp };

const char kPowrCommands[]         PROGMEM = D_CMND_SENSOR_POWR "|_"  D_CMND_SENSOR_TIMEOUT;
void (* const PowrCommand[])(void) PROGMEM = {                     &CmndSensorPowerTimeout };

const char kTempCommands[]         PROGMEM = D_CMND_SENSOR_TEMP "|_"    D_CMND_SENSOR_TOPIC  "|_"   D_CMND_SENSOR_KEY   "|_"    D_CMND_SENSOR_TIMEOUT  "|_"   D_CMND_SENSOR_DRIFT;
void (* const TempCommand[])(void) PROGMEM = {                     &CmndSensorTemperatureTopic, &CmndSensorTemperatureKey, &CmndSensorTemperatureTimeout, &CmndSensorTemperatureDrift };

const char kHumiCommands[]         PROGMEM = D_CMND_SENSOR_HUMI "|_"   D_CMND_SENSOR_TOPIC  "|_"  D_CMND_SENSOR_KEY  "|_"  D_CMND_SENSOR_TIMEOUT;
void (* const HumiCommand[])(void) PROGMEM = {                      &CmndSensorHumidityTopic , &CmndSensorHumidityKey , &CmndSensorHumidityTimeout };

const char kPresCommands[]         PROGMEM = D_CMND_SENSOR_PRES "|_"   D_CMND_SENSOR_TOPIC   "|_"  D_CMND_SENSOR_KEY  "|_"  D_CMND_SENSOR_TIMEOUT;
void (* const PresCommand[])(void) PROGMEM = {                       &CmndSensorPresenceTopic , &CmndSensorPresenceKey , &CmndSensorPresenceTimeout };

/***********************************************\
 *                  Commands
\***********************************************/

// timezone help
void CmndSensorHelp ()
{
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Sensor commands :"));

  AddLog (LOG_LEVEL_INFO, PSTR (" Temperature :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - temp_topic <topic> = topic of remote sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - temp_key <key>     = key of remote sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - temp_delay <value> = temperature validity (sec.)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - temp_drift <value> = correction (in 1/10 of °C)"));

  AddLog (LOG_LEVEL_INFO, PSTR (" Humidity :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - humi_topic <topic> = topic of remote sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - humi_key <key>     = key of remote sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - humi_delay <value> = humidity validity (sec.)"));

  AddLog (LOG_LEVEL_INFO, PSTR (" Presence :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - pres_topic <topic> = topic of remote sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - pres_key <key>     = key of remote sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - pres_delay <value> = presence validity (sec.)"));

  AddLog (LOG_LEVEL_INFO, PSTR (" Power :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - power_time <value> = power validity (sec.)"));

  ResponseCmndDone();
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
  sensor_config.temp.topic = XdrvMailbox.data;
  ResponseCmndChar (sensor_config.temp.topic.c_str ());
}

void CmndSensorTemperatureKey ()
{
  sensor_config.temp.key = XdrvMailbox.data;
  ResponseCmndChar (sensor_config.temp.key.c_str ());
}

void CmndSensorTemperatureTimeout ()
{
  if (XdrvMailbox.payload > 0) sensor_config.temp.validity = (uint16_t)XdrvMailbox.payload; 
  ResponseCmndNumber (sensor_config.temp.validity);
}

void CmndSensorHumidityTopic ()
{
  sensor_config.humi.topic = XdrvMailbox.data;
  ResponseCmndChar (sensor_config.humi.topic.c_str ());
}

void CmndSensorHumidityKey ()
{
  sensor_config.humi.key = XdrvMailbox.data;
  ResponseCmndChar (sensor_config.humi.key.c_str ());
}

void CmndSensorHumidityTimeout ()
{
  if (XdrvMailbox.payload > 0) sensor_config.humi.validity = (uint16_t)XdrvMailbox.payload; 
  ResponseCmndNumber (sensor_config.humi.validity);
}

void CmndSensorPresenceTopic ()
{
  sensor_config.pres.topic = XdrvMailbox.data;
  ResponseCmndChar (sensor_config.pres.topic.c_str ());
}

void CmndSensorPresenceKey ()
{
  sensor_config.pres.key = XdrvMailbox.data;
  ResponseCmndChar (sensor_config.pres.key.c_str ());
}

void CmndSensorPresenceTimeout ()
{
  if (XdrvMailbox.payload > 0) sensor_config.pres.validity = (uint16_t)XdrvMailbox.payload; 
  ResponseCmndNumber (sensor_config.pres.validity);
}

void CmndSensorPowerTimeout ()
{
  if (XdrvMailbox.payload > 0) sensor_config.powr.validity = (uint16_t)XdrvMailbox.payload; 
  ResponseCmndNumber (sensor_config.powr.validity);
}

/**************************************************\
 *                  Functions
\**************************************************/

// convert config value to delay value
uint16_t SensorConvertDelayFromConfig (const uint8_t config)
{
  uint16_t delay;

  if (config < 100) delay = (uint16_t)config;
    else if (config < 200) delay = 100 + (uint16_t)(config - 100) * 10;
    else delay = 1080 + (uint16_t)(config - 200) * 60;

  return delay;
}

// convert delay to config value
uint8_t SensorConvertDelayToConfig (const uint16_t delay)
{
  uint8_t config;

  if (delay < 100) config = (uint8_t)delay;
    else if (delay < 1100)  config = 100 + (uint8_t)((delay - 100) / 10);
    else if (delay <= 4380) config = 200 - 18 + (uint8_t)(delay / 60);
    else config = 255;

  return config;
}

// -----------------
//    Power
// -----------------

bool SensorPowerUpdate ()
{
  bool publish = false;

#ifdef USE_ENERGY_SENSOR

  // handle according to context
  switch (sensor_status.powr.source)
  {
    case SENSOR_POWER_NONE:
      // check if power sensor is available
      if (Energy->current[0] > 0) sensor_status.powr.source = SENSOR_POWER_LOCAL;
      break;
      
    case SENSOR_POWER_LOCAL:
      // uddate current power
      sensor_status.powr.value = Energy->voltage[0] * Energy->current[0];

      // check if power needs JSON update
      if (sensor_status.powr.last == 0) sensor_status.powr.last = sensor_status.powr.value;
      publish = (abs (sensor_status.powr.value - sensor_status.powr.last) >= 50);
      break;
  }
  
  // if data will be published, update and log
  if (publish)
  {
    sensor_status.powr.last = sensor_status.powr.value;
    AddLog (LOG_LEVEL_DEBUG, PSTR ("SEN: Power %d VA"), (int)sensor_status.powr.value);
  }
  
#endif    // USE_ENERGY_SENSOR

  return publish;
}

float SensorPowerGet ()
{
  return sensor_status.powr.value;
}

// -----------------
//    Temperature
// -----------------

void SensorTemperatureDetectSource ()
{
  char str_sensor[16];

  // init
  str_sensor[0] = 0;

  // check for DHT11 sensor
  if (PinUsed (GPIO_DHT11))
  {
    sensor_status.temp.source = SENSOR_TEMP_DHT;
    strcpy_P (str_sensor, PSTR ("DHT11"));
  }

  // check for DHT22 sensor
  else if (PinUsed (GPIO_DHT22))
  {
    sensor_status.temp.source = SENSOR_TEMP_DHT;
    strcpy_P (str_sensor, PSTR ("DHT22"));
  }

  // check for SI7021 sensor
  else if (PinUsed (GPIO_SI7021))
  {
    Dht[0].delay_lo = 480;
    Dht[0].delay_hi = 40;
    sensor_status.temp.source = SENSOR_TEMP_DHT;
    strcpy_P (str_sensor, PSTR ("SI7021"));
  }

  // check for SHT30 and SHT40 sensor
  else if (PinUsed (GPIO_I2C_SCL) && PinUsed (GPIO_I2C_SDA))
  {
    sensor_status.temp.source = SENSOR_TEMP_SHT;
    strcpy_P (str_sensor, PSTR ("SHTxx"));
  }

  // check for DS18B20 sensor
  else if (PinUsed (GPIO_DSB))
  {
    sensor_status.temp.source = SENSOR_TEMP_DSB;
    strcpy_P (str_sensor, PSTR ("DS18B20"));
  }

  // log detection
  if (strlen (str_sensor) > 0) AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Temperature sensor is %s"), str_sensor);
}

// set current temperature
bool SensorTemperatureSet (const float temperature)
{
  bool publish = false;

  // uddate current temperature
  sensor_status.temp.value = temperature;

  // set next timeover
  sensor_status.temp.timeout = millis () + 1000 * sensor_config.temp.validity;
  
  // if temperature not available, reset last value
  if (isnan (temperature)) sensor_status.temp.last = NAN;

  // else, publish if temperature just came
  else if (isnan (sensor_status.temp.last)) publish = true;
  
  // else publish if 0.2°C difference
  else publish = (abs (sensor_status.temp.last - sensor_status.temp.value) > 0.2);

  // if publication asked, udate last value
  if (publish) sensor_status.temp.last = sensor_status.temp.value;

  return publish;
}

// get current temperature
float SensorTemperatureGet ()
{
  return sensor_status.temp.value;
}

// update temperature
bool SensorTemperatureUpdate ()
{
  bool    publish = false;
  uint8_t sensor, index;
  float   value;

  // update according to sensor type
  switch (sensor_status.temp.source)
  {
    case SENSOR_TEMP_SHT:
      Sht3xRead (0);
      publish = SensorTemperatureSet (sht3x_sensors[0].temp);
      break;

    case SENSOR_TEMP_DHT:
      publish = SensorTemperatureSet (Dht[0].t);
      break;

    case SENSOR_TEMP_DSB:
#ifdef ESP8266
      for (sensor = 0; sensor < DS18X20Data.sensors; sensor++)
      {
        index = ds18x20_sensor[sensor].index;
        if (ds18x20_sensor[index].valid) value = ds18x20_sensor[index].temperature;
      }
#else
      Ds18x20Read (0, value);
#endif

      // update temperature
      publish = SensorTemperatureSet (value);
      break;

    case SENSOR_TEMP_REMOTE:
      if (TimeDifference (millis (), sensor_status.temp.timeout) < 0) publish = SensorTemperatureSet (NAN);
      break;
  }

  return publish;
}

// ---------------
//    Humidity
// ---------------

void SensorHumidityDetectSource ()
{
  char str_sensor[16];

  // init
  str_sensor[0] = 0;

  // check for DHT11 sensor
  if (PinUsed (GPIO_DHT11))
  {
    sensor_status.humi.source = SENSOR_HUMI_DHT;
    strcpy_P (str_sensor, PSTR ("DHT11"));
  }

  // check for DHT22 sensor
  else if (PinUsed (GPIO_DHT22))
  {
    sensor_status.humi.source = SENSOR_HUMI_DHT;
    strcpy_P (str_sensor, PSTR ("DHT22"));
  }

  // check for SI7021 sensor
  else if (PinUsed (GPIO_SI7021))
  {
    Dht[0].delay_lo = 480;
    Dht[0].delay_hi = 40;
    sensor_status.humi.source = SENSOR_HUMI_DHT;
    strcpy_P (str_sensor, PSTR ("SI7021"));
  }

  // check for SHT30 and SHT40 sensor
  else if (PinUsed (GPIO_I2C_SCL) && PinUsed (GPIO_I2C_SDA))
  {
    sensor_status.humi.source = SENSOR_HUMI_SHT;
    strcpy_P (str_sensor, PSTR ("SHTxx"));
  }

  // log detection
  if (strlen (str_sensor) > 0) AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Humidity sensor is %s"), str_sensor);
}

// set humidity for current 10mn slot
bool SensorHumiditySet (const float humidity)
{
  bool publish = false;

  // set value
  sensor_status.humi.value = humidity;

  // set next timeover
  sensor_status.humi.timeout = millis () + 1000 * sensor_config.humi.validity;

  // if humidity is not available, reset last value
  if (isnan (humidity)) sensor_status.humi.last = NAN;

  // else if humidity newly set, publish
  else if (isnan (sensor_status.humi.last)) publish = true;

  // else publish if 1% difference
  else publish = (abs (sensor_status.humi.value - sensor_status.humi.last) > 1);

  // if publication asked, update last value
  if (publish) sensor_status.humi.last = sensor_status.humi.value;

  return publish;
}

// read humidity level
float SensorHumidityGet ()
{
  return sensor_status.humi.value;
}

// update humidity
bool SensorHumidityUpdate ()
{
  bool  publish = false;
  float value;

  // update according to sensor type
  switch (sensor_status.humi.source)
  {
    case SENSOR_HUMI_SHT:
      Sht3xRead (0);
      value = sht3x_sensors[0].humi;
      publish = SensorHumiditySet (value);
      break;
        
    case SENSOR_HUMI_DHT:
      value = Dht[0].h;
      publish = SensorHumiditySet (value);
      break;

    case SENSOR_HUMI_REMOTE:
      if (TimeDifference (millis (), sensor_status.humi.timeout) < 0) publish = SensorHumiditySet (NAN);
      break;
    }

  return publish;
}

// ---------------
//    Presence
// ---------------

// presence detection interrupt
void ICACHE_RAM_ATTR SensorPresenceInterrupt ()
{
  sensor_presence = true;
}

void SensorPresenceDetectSource ()
{
  char str_name[16];

  // hilink serial sensors
#ifdef USE_HILINK_DETECTOR
  if (HilinkIsDeclared ()) sensor_status.pres.source = SENSOR_PRES_HILINK;
  else
#endif  // USE_HILINK_DETECTOR

  // generic switch sensor
  if (PinUsed (GPIO_INPUT, 0)) sensor_status.pres.source = SENSOR_PRES_LOCAL;

  // log
  if (sensor_status.pres.source != SENSOR_PRES_NONE)
  {
    GetTextIndexed (str_name, sizeof (str_name), sensor_status.pres.source, kSensorPresModel);
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Presence sensor is %s"), str_name);
  }
}

// set current presence
bool SensorPresenceSet ()
{
  bool publish;

  // check if status has changed
  if (sensor_status.pres.timelast == 0) publish = true;
    else publish = (TimeDifference (LocalTime (), sensor_status.pres.timelast + (uint32_t)sensor_config.pres.validity) < 0);

  // set last presence
  sensor_status.pres.timelast = LocalTime ();

  // check for publication
  if (publish) AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Presence detected"));

  return publish;
}

// read movement detection status including a timeout (in sec.)
bool SensorPresenceGet () { return SensorPresenceGet (UINT32_MAX); }
bool SensorPresenceGet (const uint32_t timeout)
{
  bool     status;
  uint32_t delay;

  // default delay
  if (timeout == UINT32_MAX) delay = (uint32_t)sensor_config.pres.validity;
    else delay = timeout;

  // check status
  if (sensor_status.pres.timelast == 0) status = false;
    else status = (TimeDifference (LocalTime (), sensor_status.pres.timelast + delay) > 0);

  return status;
}

// suspend presence detection (to avoid presence detection on relay latch)
void SensorPresenceSuspendDetection (const uint32_t duration)
{
  // set timestamp to ignore sensor state
  sensor_status.pres.suspend = millis () + 1000 * duration;
}

bool SensorPresenceUpdate ()
{
  bool publish = false;

  // check if timeout is reached in case of presence detection suspension
  if ((sensor_status.pres.suspend > 0) && (TimeDifference (sensor_status.pres.suspend, millis ()) > 0)) sensor_status.pres.suspend = 0;

#ifdef USE_HILINK_DETECTOR
  if (sensor_status.pres.source == SENSOR_PRES_HILINK) sensor_presence = HilinkIsActive ();
#endif      // USE_HILINK_DETECTOR

  // if detection is suspended, reset detection flag
  if (sensor_status.pres.suspend != 0) sensor_presence = false;
  
  // if detection flag is set, handle detection and reset the flag
  if (sensor_presence) publish = SensorPresenceSet ();
  sensor_presence = false;

  return publish;
}

// get delay since last movement detection (in sec.)
uint32_t SensorPresenceGetDelay ()
{
  uint32_t delay;

  if (sensor_status.pres.timelast == 0) delay = UINT32_MAX;
    else delay = LocalTime () - sensor_status.pres.timelast;

  return delay;
}

// get delay since last movement detection (in text)
void SensorPresenceGetDelayText (char* pstr_result, size_t size_result)
{
  uint32_t delay;

  // check parameters
  if (pstr_result == nullptr) return;
  if (size_result < 12) return;

  // calculate delay
  delay = SensorPresenceGetDelay ();

  // set unit according to value
  if (delay == UINT32_MAX) strcpy_P (pstr_result, PSTR ("---"));
  else if (delay >= 86400) sprintf_P (pstr_result, PSTR ("%d d %d h"), delay / 86400, (delay % 86400) / 3600);
  else if (delay >= 36000) sprintf_P (pstr_result, PSTR ("%d h"), delay / 3600);
  else if (delay >= 3600) sprintf_P (pstr_result, PSTR ("%d h %d mn"), delay / 3600, (delay % 3600) / 60);
  else if (delay >= 600) sprintf_P (pstr_result, PSTR ("%d mn"), delay / 60);
  else if (delay >= 60) sprintf_P (pstr_result, PSTR ("%d mn %d s"), delay / 60, delay % 60);
  else if (delay > 0) sprintf_P (pstr_result, PSTR ("%d s"), delay);
  else strcpy_P (pstr_result, PSTR ("now"));
}

// ---------------
//    Activity
// ---------------

// read activity detection status
bool SensorActivityGet ()
{
  bool status;

  // check status
  if (sensor_status.pres.timelast == 0) status = false;
    else status = (TimeDifference (LocalTime (), sensor_status.acti.timelast + SENSOR_ACTI_VALIDITY) > 0);

  return status;
}

// set current presence
bool SensorActivitySet ()
{
  bool publish;

  // check if status has changed
  if (sensor_status.acti.timelast == 0) publish = true;
    publish = (TimeDifference (LocalTime (), sensor_status.acti.timelast + SENSOR_ACTI_VALIDITY) < 0);

  // set last presence
  sensor_status.acti.timelast = LocalTime ();

  // check for publication
  if (publish) AddLog (LOG_LEVEL_DEBUG, PSTR ("SEN: Presence detected"));

  return (publish);
}

/**************************************************\
 *                  Callback
\**************************************************/

// Sensor module initialisation
void SensorPreInit ()
{
  uint8_t gpio;

  // if Interrupt 1 is set
  if (PinUsed (GPIO_INTERRUPT, 0))
  {
    // init levels
    gpio = Pin (GPIO_INTERRUPT, 0);
    pinMode (gpio, INPUT_PULLUP);
    attachInterrupt (gpio, SensorPresenceInterrupt, FALLING);

    // log gpio
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Presence detector on GPIO%02u"), gpio);
  }
}

void SensorInit () 
{
  //     Status
  // ----------------

  // temperature
  sensor_status.temp.source   = SENSOR_TEMP_NONE;
  sensor_status.temp.timeout  = UINT32_MAX;
  sensor_status.temp.value    = NAN;
  sensor_status.temp.last     = NAN;

  // humidity
  sensor_status.humi.source   = SENSOR_HUMI_NONE;
  sensor_status.humi.timeout  = UINT32_MAX;
  sensor_status.humi.value    = NAN;
  sensor_status.humi.last     = NAN;

  // power
  sensor_status.powr.source   = SENSOR_POWER_NONE;
  sensor_status.powr.value    = NAN;
  sensor_status.powr.last     = NAN;

  // presence
  sensor_status.pres.source   = SENSOR_PRES_NONE;
  sensor_status.pres.timelast = 0;
  sensor_status.pres.suspend  = 0;

  // activity
  sensor_status.pres.timelast = 0;

  //   Config
  // -----------

  // data validity timeout
  sensor_config.powr.validity = 30;
  sensor_config.temp.validity = SensorConvertDelayFromConfig (Settings->rf_code[0][0]);
  sensor_config.humi.validity = SensorConvertDelayFromConfig (Settings->rf_code[0][1]);
  sensor_config.pres.validity = SensorConvertDelayFromConfig (Settings->rf_code[0][2]);

  if (sensor_config.temp.validity == 0) sensor_config.temp.validity = SENSOR_TEMP_VALIDITY;
  if (sensor_config.humi.validity == 0) sensor_config.humi.validity = SENSOR_HUMI_VALIDITY;
  if (sensor_config.pres.validity == 0) sensor_config.pres.validity = SENSOR_PRES_VALIDITY;

  // MQTT topics
  sensor_config.temp.topic = SettingsText (SET_SENSOR_TEMP_TOPIC);
  sensor_config.humi.topic = SettingsText (SET_SENSOR_HUMI_TOPIC);
  sensor_config.pres.topic = SettingsText (SET_SENSOR_PRES_TOPIC);

  // MQTT keys
  sensor_config.temp.key = SettingsText (SET_SENSOR_TEMP_KEY);
  sensor_config.humi.key = SettingsText (SET_SENSOR_HUMI_KEY);
  sensor_config.pres.key = SettingsText (SET_SENSOR_PRES_KEY);

  if (sensor_config.temp.key.isEmpty ()) sensor_config.temp.key = D_SENSOR_TEMPERATURE;
  if (sensor_config.humi.key.isEmpty ()) sensor_config.humi.key = D_SENSOR_HUMIDITY;
  if (sensor_config.pres.key.isEmpty ()) sensor_config.pres.key = D_SENSOR_PRESENCE;

  //     Detection
  // -----------------

  SensorTemperatureDetectSource ();
  SensorHumidityDetectSource ();
  SensorPresenceDetectSource ();

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Run sensor to get help on Sensor commands"));
}

// save configuration parameters
void SensorSaveBeforeRestart ()
{
  // save sensor data validity timeout
  Settings->rf_code[0][0] = SensorConvertDelayToConfig (sensor_config.temp.validity);
  Settings->rf_code[0][1] = SensorConvertDelayToConfig (sensor_config.humi.validity);
  Settings->rf_code[0][2] = SensorConvertDelayToConfig (sensor_config.pres.validity);

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

// update sensor condition every second
void SensorEvery250ms ()
{
  bool publish;

  // check RTC validity
  if (!RtcTime.valid) return;

  // update sensors
  publish = SensorPowerUpdate ();

  // if needed, publish JSON
  if (publish) HelperPublishMqtt ();
}

// update sensor condition every second
void SensorEverySecond ()
{
  bool publish;

  // check RTC validity
  if (!RtcTime.valid) return;

  // update sensors
  publish  = SensorTemperatureUpdate ();
  publish |= SensorHumidityUpdate    ();
  publish |= SensorPresenceUpdate    ();

  // if needed, publish JSON
  if (publish) HelperPublishMqtt ();
}

// check and update MQTT power subsciption after disconnexion
void SensorDriverMqttSubscribe ()
{
  // if topic is defined, subscribe to remote temperature
  if (sensor_config.temp.topic.length () > 0)
  {
    // subscribe to sensor topic
    MqttSubscribe (sensor_config.temp.topic.c_str ());
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Remote temperature from %s"), sensor_config.temp.topic.c_str ());
  }

  // if topic is defined, subscribe to remote humidity
  if (sensor_config.humi.topic.length () > 0)
  {
    // subscribe to sensor topic
    MqttSubscribe (sensor_config.humi.topic.c_str ());
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Remote humidity from %s"), sensor_config.humi.topic.c_str ());
  }

  // if topic is defined, subscribe to remote movement
  if (sensor_config.pres.topic.length () > 0)
  {
    // subscribe to sensor topic
    MqttSubscribe (sensor_config.pres.topic.c_str ());
    AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Remote movement from %s"), sensor_config.pres.topic.c_str ());
  }
}

// read a key value in a JSON string
bool SensorDriverGetJsonKey (const char* pstr_json, const char* pstr_key, char* pstr_value, size_t size_value)
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
bool SensorDriverMqttData ()
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
    is_key = SensorDriverGetJsonKey (XdrvMailbox.data, sensor_config.temp.key.c_str (), str_value, sizeof (str_value));
    if (is_key)
    {
      // if needed, set remote source for temperature
      if (sensor_status.temp.source == SENSOR_TEMP_NONE) sensor_status.temp.source = SENSOR_TEMP_REMOTE;

      // set remote temperature
      SensorTemperatureSet (atof (str_value) + Settings->temp_comp / 10);

      // log
      AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Remote temp %s°C"), str_value);
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
    is_key = SensorDriverGetJsonKey (XdrvMailbox.data, sensor_config.humi.key.c_str (), str_value, sizeof (str_value));
    if (is_key)
    {
      // if needed, set remote source for humidity
      if (sensor_status.humi.source == SENSOR_HUMI_NONE) sensor_status.humi.source = SENSOR_HUMI_REMOTE;

      // set remote humidity
      SensorHumiditySet (atof (str_value));

      // log
      AddLog (LOG_LEVEL_INFO, PSTR ("SEN: Remote humi %s%%"), str_value);
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
    is_key = SensorDriverGetJsonKey (XdrvMailbox.data, sensor_config.pres.key.c_str (), str_value, sizeof (str_value));
    if (is_key)
    {
      // if needed, set remote source for movement
      if (sensor_status.pres.source == SENSOR_PRES_NONE) sensor_status.pres.source = SENSOR_PRES_REMOTE;

      // read presence detection
      detected = ((strcmp (str_value, "1") == 0) || (strcmp (str_value, "on") == 0) || (strcmp (str_value, "ON") == 0));
      if (detected) SensorPresenceSet ();
    }
  }

  return is_found;
}

// Show JSON status (for local sensors and presence sensor)
void SensorShowJSON ()
{
  int  value;
  char str_value[16];

  // if not teleperiod or RTC, ignore
  if (!RtcTime.valid) return;
//  if (TasmotaGlobal.tele_period != 0) return; 

  // add , in append mode or { in direct publish mode
  MiscOptionPrepareJsonSection ();

  ResponseAppend_P (PSTR ("\"Sensor\":{"));

  // presence sensor
  ResponseAppend_P (PSTR ("\"Pres\":%d"), SensorPresenceGet (), str_value);
  if (sensor_status.pres.timelast != 0)
  {
    SensorPresenceGetDelayText (str_value, sizeof (str_value));
    ResponseAppend_P (PSTR (",\"Since\":%u,\"Delay\":\"%s\""), SensorPresenceGetDelay (), str_value);
  }

  // temperature sensor
  if (!isnan (sensor_status.temp.value))
  {
    sensor_status.temp.last = sensor_status.temp.value;
    value = (int)(sensor_status.temp.value * 10);
    ResponseAppend_P (PSTR (",\"Temp\":%d.%d"),  value / 10, abs (value % 10));
  }

  // humidity sensor
  if (!isnan (sensor_status.humi.value))
  {
    sensor_status.humi.last = sensor_status.humi.value;
    value = (int)(sensor_status.humi.value * 10);
    ResponseAppend_P (PSTR (",\"Humi\":%d.%d"),  value / 10, abs (value % 10));
  }

  // power sensor
  if (!isnan (sensor_status.powr.value))
  {
    sensor_status.powr.last = sensor_status.powr.value;
    value = (int)sensor_status.powr.value;
    ResponseAppend_P (PSTR (",\"Power\":%d"),  value);
  }

  ResponseAppend_P (PSTR ("}"));
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// Get specific argument as a value with min and max
int SensorGetArgValue (const char* pstr_argument, const int value_min, const int value_max, const int value_default)
{
  int  value = value_default;
  char str_argument[8];

  // check for argument
  if (Webserver->hasArg (pstr_argument))
  {
    WebGetArg (pstr_argument, str_argument, sizeof (str_argument));
    value = atoi (str_argument);

    // check for min and max value
    value = max (value, value_min);
    value = min (value, value_max);
  }

  return value;
}

// configuration page button
void SensorWebConfigButton ()
{
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>Sensors</button></form></p>"), D_SENSOR_PAGE_CONFIG);
}

// main page button
void SensorWebMainButton ()
{
  // style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR (".gs {display:flex;margin:0px;padding:0px;font-size:14px;text-align:center;}\n"));
  WSContentSend_P (PSTR (".gs-icon {display:inline-block;width:32px;height:32px;background-color:#1fa3ec;-webkit-mask-repeat:no-repeat;mask-repeat:no-repeat;-webkit-mask-size:100%% 100%%;mask-size:100%% 100%%;-webkit-mask-image:var(--svg);mask-image:var(--svg);}\n"));
  WSContentSend_P (PSTR (".temp {--svg:url(\"data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'><path d='M12 2a3.25 3.25 0 0 1 3.245 3.065l.005.185l.001 7.952l.08.069a5 5 0 0 1 1.644 3.223l.019.252L17 17a5 5 0 1 1-8.51-3.56l.18-.17l.079-.068l.001-7.952a3.25 3.25 0 0 1 2.884-3.23l.182-.015zm0 1.5a1.75 1.75 0 0 0-1.744 1.607l-.006.143v8.695l-.309.224a3.5 3.5 0 1 0 4.283.128l-.165-.127l-.307-.225l-.002-8.695A1.75 1.75 0 0 0 12 3.5M12 8a.75.75 0 0 1 .75.75v5.865a2.501 2.501 0 1 1-1.5 0V8.75A.75.75 0 0 1 12 8'/></svg>\");}\n"));
  WSContentSend_P (PSTR (".humi {--svg:url(\"data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'><path d='M26 12a3.9 3.9 0 0 1-4-3.777a3.9 3.9 0 0 1 .653-2.064l2.517-3.745a1.038 1.038 0 0 1 1.66 0l2.485 3.696A3.97 3.97 0 0 1 30 8.223A3.9 3.9 0 0 1 26 12m0-7.237l-1.656 2.463a1.9 1.9 0 0 0-.344.997a2.014 2.014 0 0 0 4 0a2 2 0 0 0-.375-1.047zM23.5 30h-15a6.496 6.496 0 0 1-1.3-12.862a8.994 8.994 0 0 1 17.6 0A6.496 6.496 0 0 1 23.5 30M16 12a7 7 0 0 0-6.941 6.145l-.1.812l-.815.064A4.496 4.496 0 0 0 8.5 28h15a4.496 4.496 0 0 0 .356-8.979l-.815-.064l-.099-.812A7 7 0 0 0 16 12'/></svg>\");}\n"));
  WSContentSend_P (PSTR (".move {--svg:url(\"data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 512 512'><path d='M256 256a112 112 0 1 0-112-112a112 112 0 0 0 112 112m0 32c-69.42 0-208 42.88-208 128v64h416v-64c0-85.12-138.58-128-208-128'/></svg>\");}\n"));
  WSContentSend_P (PSTR (".powr {--svg:url(\"data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 300.145 300.145'><path d='M226.169 134.015c1.26-2.479 1.141-5.202-0.314-7.572c-1.454-2.371-4.036-3.316-6.818-3.316h-60.821L171.309 9.3 c0.446-3.859-1.946-7.857-5.672-8.963C164.868 0.109 164.089 0 163.32 0c-2.954 0-5.746 1.681-7.121 4.442L73.946 169.718 c-1.234 2.479-1.098 5.78 0.36 8.136c1.459 2.355 4.031 4.273 6.802 4.273h60.019l-12.304 109.543 c-0.43 3.844 1.951 7.077 5.657 8.185c0.761 0.228 1.532 0.29 2.293 0.29c2.948 0 5.74-1.637 7.133-4.378L226.169 134.015z'/></svg>\");}\n"));
  WSContentSend_P (PSTR ("</style>\n"));
}

// append remote sensor to main page
void SensorWebSensor ()
{
  uint8_t width, nb_sensor;
  int     value;
  char    str_text[32];

  // calculate number of sensors and ignore if no sensor
  nb_sensor = 0;
  if (sensor_status.temp.source != SENSOR_TEMP_NONE)  nb_sensor++;
  if (sensor_status.humi.source != SENSOR_HUMI_NONE)  nb_sensor++;
  if (sensor_status.pres.source != SENSOR_PRES_NONE)  nb_sensor++;
  if (sensor_status.powr.source != SENSOR_POWER_NONE) nb_sensor++;
  if (nb_sensor == 0) return;

/*
  // basic counter or remote sensor display
  if ((sensor_status.pres.source >= SENSOR_PRES_REMOTE) && (sensor_status.pres.source < SENSOR_PRES_HILINK))
  {
    // get type
    GetTextIndexed (str_text, sizeof (str_text), sensor_status.pres.source, kSensorPresModel);

    // display presence sensor
    WSContentSend_P (PSTR ("<div class='pres'>\n"));
    WSContentSend_P (PSTR ("<div class='head'>%s</div>\n"), str_text);
    if (sensor_status.pres.value == 1) strcpy_P (str_text, PSTR ("#D00"));
      else strcpy_P (str_text, PSTR ("#444"));
    WSContentSend_P (PSTR ("<div class='bar' style='background:%s;'></div>"), str_text);
    switch (sensor_status.pres.source)
    {
      case SENSOR_PRES_SWITCH:   strcpy_P (str_text, PSTR ("contact"));       break;
      case SENSOR_PRES_RCWL0516: strcpy_P (str_text, PSTR ("0 - 7m"));        break;
      case SENSOR_PRES_HWMS03:   strcpy_P (str_text, PSTR ("0.5 - 10m"));     break;
      case SENSOR_PRES_REMOTE:   strcpy_P (str_text, PSTR (D_SENSOR_REMOTE)); break;
      default: strcpy (str_text, ""); break;
    }
    WSContentSend_P (PSTR ("<div class='dist'>%s</div>\n"), str_text);
    WSContentSend_P (PSTR ("</div>\n"));
  }
*/

  // display individual sensor
  width = 100 / nb_sensor;
  WSContentSend_P (PSTR ("<div>\n"));

  // icons
  WSContentSend_P (PSTR ("<div class='gs'>"));
  if (sensor_status.pres.source != SENSOR_PRES_NONE)  WSContentSend_P (PSTR ("<div style='width:%u%%;' class='gs-icon %s'></div>"), width, PSTR ("move"));
  if (sensor_status.powr.source != SENSOR_POWER_NONE) WSContentSend_P (PSTR ("<div style='width:%u%%;' class='gs-icon %s'></div>"), width, PSTR ("powr"));
  if (sensor_status.humi.source != SENSOR_HUMI_NONE)  WSContentSend_P (PSTR ("<div style='width:%u%%;' class='gs-icon %s'></div>"), width, PSTR ("humi"));
  if (sensor_status.temp.source != SENSOR_TEMP_NONE)  WSContentSend_P (PSTR ("<div style='width:%u%%;' class='gs-icon %s'></div>"), width, PSTR ("temp"));
  WSContentSend_P (PSTR ("</div>\n"));

  // values
  WSContentSend_P (PSTR ("<div class='gs'>"));

  // value : presence
  if (sensor_status.pres.source != SENSOR_PRES_NONE) 
  {
    SensorPresenceGetDelayText (str_text, sizeof (str_text));
    WSContentSend_P (PSTR ("<div style='width:%u%%;'>%s</div>"), width, str_text);
  }

  // value : power
  if (sensor_status.powr.source != SENSOR_POWER_NONE) 
  {
    value = (int)sensor_status.powr.value;
    WSContentSend_P (PSTR ("<div style='width:%u%%;'>%d VA</div>"), width, value);
  }

  // value : humidity
  if (sensor_status.humi.source != SENSOR_HUMI_NONE)
  {
    if (isnan (sensor_status.humi.value)) strcpy_P (str_text, PSTR ("---")); 
    else
    {
      value = (int)(sensor_status.humi.value * 10);
      sprintf_P (str_text, PSTR ("%d.%d"), value / 10, abs (value % 10));
    }
    WSContentSend_P (PSTR ("<div style='width:%u%%;'>%s %%</div>"), width, str_text);
  }

  // value : temperature
  if (sensor_status.temp.source != SENSOR_TEMP_NONE) 
  {
    if (isnan (sensor_status.temp.value)) strcpy_P (str_text, PSTR ("---")); 
    else 
    {
      value = (int)(sensor_status.temp.value * 10);
      sprintf_P (str_text, PSTR ("%d.%d"), value / 10, abs (value % 10));
    }
    WSContentSend_P (PSTR ("<div style='width:%u%%;'>%s °C</div>"), width, str_text);
  }

  WSContentSend_P (PSTR ("</div>\n"));

  WSContentSend_P (PSTR ("</div>\n"));
}

// remote sensor configuration web page
void SensorWebConfigure ()
{
  bool    display;
  uint8_t index;
  float   value;
  char    str_value[16];
  char    str_text[128];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess ()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg ("save"))
  {
    // get temperature drift according to 'tdrift' parameter
    WebGetArg (PSTR ("tdrift"), str_text, sizeof (str_text));
    if (strlen (str_text) > 0) Settings->temp_comp = (int8_t)(atof (str_text) * 10);

    // set temperature timeout according to 'ttime' parameter
    WebGetArg (PSTR ("ttime"), str_text, sizeof (str_text));
    sensor_config.temp.validity = (uint16_t)max (10, atoi (str_text));

    // if temperature is remote, set topic and key
    WebGetArg (PSTR ("tmqtt"), str_text, sizeof (str_text));
    sensor_config.temp.topic = "";
    sensor_config.temp.key   = "";
    if (strcmp_P (str_text, PSTR ("on")) == 0)
    {
      WebGetArg (PSTR ("ttopic"), str_text, sizeof (str_text));
      sensor_config.temp.topic = str_text;
      WebGetArg (PSTR ("tkey"), str_text, sizeof (str_text));
      sensor_config.temp.key = str_text;
    }

    // set humidity timeout according to 'htime' parameter
    WebGetArg (PSTR ("htime"), str_text, sizeof (str_text));
    sensor_config.humi.validity = (uint16_t)max (10, atoi (str_text));

    // if humidity is remote, set topic and key
    WebGetArg (PSTR ("hmqtt"), str_text, sizeof (str_text));
    sensor_config.humi.topic = "";
    sensor_config.humi.key   = "";
    if (strcmp (str_text, "on") == 0)
    {
      WebGetArg (PSTR ("htopic"), str_text, sizeof (str_text));
      sensor_config.humi.topic = str_text;
      WebGetArg (PSTR ("hkey"), str_text, sizeof (str_text));
      sensor_config.humi.key = str_text;
    }

    // set presence timeout according to 'ptime' parameter
    WebGetArg (PSTR ("ptime"), str_text, sizeof (str_text));
    sensor_config.pres.validity = (uint32_t)max (10, atoi (str_text));

    // if presence is remote, set topic and key
    WebGetArg (PSTR ("pmqtt"), str_text, sizeof (str_text));
    sensor_config.pres.topic = "";
    sensor_config.pres.key   = "";
    if (strcmp_P (str_text, PSTR ("on")) == 0)
    {
      WebGetArg (PSTR ("ptopic"), str_text, sizeof (str_text));
      sensor_config.pres.topic = str_text;
      WebGetArg (PSTR ("pkey"), str_text, sizeof (str_text));
      sensor_config.pres.key = str_text;
    }
  }

  // beginning of form
  WSContentStart_P (D_SENSOR_CONFIGURE);
  WSContentSendStyle ();

  // specific style
  WSContentSend_P (PSTR ("\n<style>\n"));
  WSContentSend_P (PSTR ("p,br,hr {clear:both;}\n")); 
  WSContentSend_P (PSTR ("p.dat {margin:0px 10px;}\n")); 
  WSContentSend_P (PSTR ("fieldset {background:#333;margin:15px 0px;border:none;border-radius:8px;}\n")); 
  WSContentSend_P (PSTR ("legend {font-weight:bold;margin:0px;padding:5px;color:#888;background:transparent;}\n")); 
  WSContentSend_P (PSTR ("legend:after {content:'';display:block;height:1px;margin:15px 0px;}\n"));
  WSContentSend_P (PSTR ("div.main {padding:0px;margin-top:-25px;}\n"));
  WSContentSend_P (PSTR ("input,select {margin-bottom:10px;text-align:center;border:none;}\n")); 
  WSContentSend_P (PSTR ("input.left {text-align:left;}\n")); 
  WSContentSend_P (PSTR ("span {float:left;padding-top:4px;}\n")); 
  WSContentSend_P (PSTR ("span.hea {width:60%%;}\n")); 
  WSContentSend_P (PSTR ("span.pri {width:35%%;}\n"));
  WSContentSend_P (PSTR ("span.val {width:25%%;padding:0px;}\n")); 
  WSContentSend_P (PSTR ("span.sel {width:65%%;padding:0px;}\n"));
  WSContentSend_P (PSTR ("span.uni {width:15%%;text-align:center;}\n")); 
  WSContentSend_P (PSTR ("</style>\n"));

  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_SENSOR_PAGE_CONFIG);

  //   temperature  
  // ---------------

  WSContentSend_P (SENSOR_FIELDSET_START, PSTR ("🌡️ Temperature"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  // Correction 
  value = SENSOR_TEMP_DRIFT_STEP;
  ext_snprintf_P (str_text, sizeof (str_text), PSTR ("%1_f"), &value);
  value = (float)Settings->temp_comp / 10;
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%1_f"), &value);
  WSContentSend_P (SENSOR_FIELD_TEXT, PSTR ("Correction"), PSTR ("tdrift"), 0 - SENSOR_TEMP_DRIFT_MAX, SENSOR_TEMP_DRIFT_MAX, str_text, str_value, PSTR ("°C"));

  // validity timeout 
  WSContentSend_P (SENSOR_FIELD_NUMBER, PSTR ("Validity"), PSTR ("ttime"), 10, 2500, 10, sensor_config.temp.validity, PSTR ("sec"));

  // if needed, display remote data
  display = !sensor_config.temp.topic.isEmpty ();
  if (display) strcpy_P (str_value, PSTR ("checked")); else strcpy (str_value, "");
  WSContentSend_P (SENSOR_FIELD_CHECKBOX, PSTR ("tmqtt"), str_value, PSTR ("tmqtt"), PSTR ("Remote sensor"));
  if (display)
  {
    WSContentSend_P (PSTR ("<hr>\n"));
    WSContentSend_P (SENSOR_FIELD_MQTT, PSTR ("Remote Topic"), PSTR ("ttopic"), PSTR (".../SENSOR"),  sensor_config.temp.topic.c_str ());
    WSContentSend_P (SENSOR_FIELD_MQTT, PSTR ("Remote Key"),   PSTR ("tkey"),   PSTR ("Temperature"), sensor_config.temp.key.c_str ());
  }
  WSContentSend_P (PSTR ("</div>\n"));
  WSContentSend_P (SENSOR_FIELDSET_STOP);

  //   humidity  
  // ------------

  WSContentSend_P (SENSOR_FIELDSET_START, PSTR ("💧 Humidity"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  // validity timeout 
  WSContentSend_P (SENSOR_FIELD_NUMBER, PSTR ("Validity"), PSTR ("htime"), 10, 2500, 10, sensor_config.humi.validity, PSTR ("sec"));

  // if needed, display remote data
  display = !sensor_config.humi.topic.isEmpty ();
  if (display) strcpy_P (str_value, PSTR ("checked")); else strcpy (str_value, "");
  WSContentSend_P (SENSOR_FIELD_CHECKBOX, PSTR ("hmqtt"), str_value, PSTR ("hmqtt"), PSTR ("Remote sensor"));
  if (display)
  {
    WSContentSend_P (PSTR ("<hr>\n"));
    WSContentSend_P (SENSOR_FIELD_MQTT, PSTR ("Remote Topic"), PSTR ("htopic"), PSTR (".../SENSOR"), sensor_config.humi.topic.c_str ());
    WSContentSend_P (SENSOR_FIELD_MQTT, PSTR ("Remote Key"),   PSTR ("hkey"),   PSTR ("Humidity"),   sensor_config.humi.key.c_str ());
  }
  WSContentSend_P (PSTR ("</div>\n"));
  WSContentSend_P (SENSOR_FIELDSET_STOP);

  //   presence  
  // -------------

  WSContentSend_P (SENSOR_FIELDSET_START, PSTR ("👤 Presence"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  // validity timeout 
  WSContentSend_P (SENSOR_FIELD_NUMBER, PSTR ("Validity"), PSTR ("ptime"), 10, 2500, 10, sensor_config.pres.validity, PSTR ("sec"));

  // if needed, display remote data
  display = !sensor_config.pres.topic.isEmpty ();
  if (display) strcpy_P (str_value, PSTR ("checked")); else strcpy (str_value, "");
  WSContentSend_P (SENSOR_FIELD_CHECKBOX, PSTR ("pmqtt"), str_value, PSTR ("pmqtt"), PSTR ("Remote sensor"));
  if (display)
  {
    WSContentSend_P (PSTR ("<hr>\n"));
    WSContentSend_P (SENSOR_FIELD_MQTT, PSTR ("Remote Topic"), PSTR ("ptopic"), PSTR (".../SENSOR"), sensor_config.pres.topic.c_str ());
    WSContentSend_P (SENSOR_FIELD_MQTT, PSTR ("Remote Key"),   PSTR ("pkey"),   PSTR ("Presence"),   sensor_config.pres.key.c_str ());
  }

  WSContentSend_P (PSTR ("</div>\n"));
  WSContentSend_P (SENSOR_FIELDSET_STOP);

  // end of form and save button
  WSContentSend_P (PSTR ("<br><p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), D_SAVE);
  WSContentSend_P (PSTR ("</form>\n"));

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

#endif      // USE_WEBSERVER

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xdrv98 (uint32_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  {
    case FUNC_MQTT_SUBSCRIBE:
      SensorDriverMqttSubscribe ();
      break;

    case FUNC_MQTT_DATA:
      result = SensorDriverMqttData ();
      break;
  }
  return result;
}

bool Xsns98 (uint32_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
   case FUNC_PRE_INIT:
      SensorPreInit ();
      break;

   case FUNC_INIT:
      SensorInit ();
      break;

    case FUNC_COMMAND:
      result = DecodeCommand (kSensCommands, SensCommand);
      if (!result) result = DecodeCommand (kTempCommands, TempCommand);
      if (!result) result = DecodeCommand (kHumiCommands, HumiCommand);
      if (!result) result = DecodeCommand (kPresCommands, PresCommand);
      if (!result) result = DecodeCommand (kPowrCommands, PowrCommand);
      break;

    case FUNC_SAVE_BEFORE_RESTART:
      SensorSaveBeforeRestart ();
      break;

    case FUNC_EVERY_250_MSECOND:
      SensorEvery250ms ();
      break;

      case FUNC_EVERY_SECOND:
      SensorEverySecond ();
      break;

    case FUNC_JSON_APPEND:
      SensorShowJSON ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      SensorWebSensor ();
      break;

    case FUNC_WEB_ADD_MAIN_BUTTON:
      SensorWebMainButton ();
      break;

    case FUNC_WEB_ADD_BUTTON:
      SensorWebConfigButton ();
      break;

    case FUNC_WEB_ADD_HANDLER:
      Webserver->on ("/" D_SENSOR_PAGE_CONFIG, SensorWebConfigure);
      break;
#endif      // USE_WEBSERVER
  }

#ifdef USE_SENSOR_GRAPH
  if (!result) result = XsnsSensorGraph (function);
#endif  // USE_TELEINFO_GRAPH

#ifdef USE_SENSOR_HISTO
  if (!result) result = XsnsSensorHisto (function);
#endif  // USE_SENSOR_HISTO

#ifdef USE_SENSOR_ACTIVITY
  if (!result) result = XsnsSensorActivity (function);
#endif  // USE_SENSOR_ACTIVITY

return result;
}

#endif      // USE_SENSOR_GENERIC
