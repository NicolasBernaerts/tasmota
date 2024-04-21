/*
  xsns_126_teleinfo_winky.ino - Deep Sleep integration for Winky Teleinfo board

  Copyright (C) 2024  Nicolas Bernaerts

  Version history :
    16/02/2024 - v1.0 - Creation

  Configuration values are stored in :
    - Settings->knx_GA_addr[0..2] : multiplicator

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

#ifdef USE_TELEINFO
#ifdef USE_WINKY

// declare teleinfo winky sensor
#define XSNS_126                   126

/*****************************************\
 *               Constants
\*****************************************/

#define WINKY_SLEEP_MINIMUM        30          // minimum acceptable sleep time (seconds)

#define WINKY_MULTIPLY_MINIMUM     200         // minimum multiplication factor
#define WINKY_MULTIPLY_MAXIMUM     400         // maximum multiplication factor

#define WINKY_VOLTAGE_REF          5000        // USB and Capa voltage reference

// voltage levels
#define WINKY_USB_CHARGED          4500        // USB voltage correct
#define WINKY_USB_DISCHARGED       4000        // USB voltage disconnected
#define WINKY_USB_CRITICAL         3000        // USB voltage disconnected

#define WINKY_CAPA_CHARGED         4750        // minimum capa voltage to start cycle
#define WINKY_CAPA_DISCHARGED      3800        // minimum capa voltage to start sleep process
#define WINKY_CAPA_CRITICAL        3700        // minimum capa voltage to sleep immediatly

#define WINKY_LINKY_CHARGED        9000        // minimum linky voltage to consider as connected
#define WINKY_LINKY_DISCHARGED     7000        // minimum linky voltage to consider as low
#define WINKY_LINKY_CRITICAL       5000        // minimum linky voltage to consider as very low

/***************************************\
 *               Variables
\***************************************/

// voltage sources and levels
enum TeleinfoWinkyStage  { WINKY_STAGE_INIT, WINKY_STAGE_RUNNING, WINKY_STAGE_MAX };                                                      // list of running stage
enum TeleinfoWinkySource { WINKY_SOURCE_USB, WINKY_SOURCE_CAPA, WINKY_SOURCE_LINKY, WINKY_SOURCE_MAX };                                   // list of sources
const char kTeleinfoWinkySource[] PROGMEM   = "USB|Capa|Linky";                                                                           // label of sources
enum TeleinfoWinkyLevel  { WINKY_LEVEL_CRITICAL, WINKY_LEVEL_DISCHARGED, WINKY_LEVEL_CORRECT, WINKY_LEVEL_CHARGED, WINKY_LEVEL_MAX };     // voltage levels
const char kTeleinfoWinkyColor[] PROGMEM    = "red|orange|yellow|white";                                                                   // level display color

const uint32_t arrTeleinfoWinkyDivider[]    = { 20, 20, 57 };                                                                             // divider used for source voltage reading
const uint32_t arrTeleinfoWinkyCharged[]    = { WINKY_USB_CHARGED,    WINKY_CAPA_CHARGED,    WINKY_LINKY_CHARGED    };                    // voltage (mV) to consider source as charged
const uint32_t arrTeleinfoWinkyDischarged[] = { WINKY_USB_DISCHARGED, WINKY_CAPA_DISCHARGED, WINKY_LINKY_DISCHARGED };                    // voltage (mV) under which source is discharged
const uint32_t arrTeleinfoWinkyCritical[]   = { WINKY_USB_CRITICAL,   WINKY_CAPA_CRITICAL,   WINKY_LINKY_CRITICAL   };                    // voltage (mV) under which source is critical

/***************************************\
 *                  Data
\***************************************/

static struct {
  uint32_t arr_multiply[WINKY_SOURCE_MAX];      // measure multiplier
  uint32_t arr_value[WINKY_SOURCE_MAX];         // input raw value
  uint32_t arr_voltage[WINKY_SOURCE_MAX];       // input voltage
  uint32_t vcap_boot = 0;                       // capa voltage at start
  uint32_t time_boot = UINT32_MAX;              // timestamp of device start
  uint32_t time_ip   = UINT32_MAX;              // timestamp of network connectivity
  uint32_t time_ntp  = UINT32_MAX;              // timestamp of ntp start
  uint32_t time_mqtt = UINT32_MAX;              // timestamp of mqtt connectivity
} teleinfo_winky;

/**********************************************\
 *                Configuration
\**********************************************/

// load configuration
void TeleinfoWinkyLoadConfig () 
{
  uint8_t index;

  for (index = 0; index < WINKY_SOURCE_MAX; index ++)
  {
    teleinfo_winky.arr_multiply[index] = (uint32_t)Settings->knx_GA_addr[index];
    if (teleinfo_winky.arr_multiply[index] < WINKY_MULTIPLY_MINIMUM) teleinfo_winky.arr_multiply[index] = WINKY_MULTIPLY_MAXIMUM;
    else if (teleinfo_winky.arr_multiply[index] > WINKY_MULTIPLY_MAXIMUM) teleinfo_winky.arr_multiply[index] = WINKY_MULTIPLY_MAXIMUM;
  }
}

// save configuration
void TeleinfoWinkySaveConfig () 
{
  uint8_t index;
  
  for (index = 0; index < WINKY_SOURCE_MAX; index ++)
  {
    Settings->knx_GA_addr[index] = (uint16_t)teleinfo_winky.arr_multiply[index];
  }
}

/************************************\
 *           Functions
\************************************/

// check winky configuration
bool TeleinfoWinkyIsConfigured ()
{
  bool    conf_ok = true;
  uint8_t index;

  for (index = 0; index < WINKY_SOURCE_MAX; index ++) if (Adc[index].pin == 0) conf_ok = false;

  return conf_ok;
}

// check winky configuration
void TeleinfoWinkyUpdateVoltage ()
{
  uint8_t  index;
  uint32_t value;

  // loop to read current values
  for (index = 0; index < WINKY_SOURCE_MAX; index ++)
  {
    // read raw value
    teleinfo_winky.arr_value[index] = (uint32_t)AdcRead (Adc[index].pin, 1);
    if (Adc[index].param2 > 0) teleinfo_winky.arr_value[index] = teleinfo_winky.arr_value[index] * Adc[index].param4 / Adc[index].param2;

    // calculate voltage
    teleinfo_winky.arr_voltage[index] = teleinfo_winky.arr_value[index] * teleinfo_winky.arr_multiply[index] * arrTeleinfoWinkyDivider[index] / 4095;

    // if USB or Capa check for maximum reference voltage
    if ((index == WINKY_SOURCE_USB) || (index == WINKY_SOURCE_CAPA))
    {
      if (teleinfo_winky.arr_voltage[index] > WINKY_VOLTAGE_REF)
      {
        // calculate new multiplier
        teleinfo_winky.arr_multiply[index]--;

        // calculate Linky multiplier as average of USB and Capa multiplier
        teleinfo_winky.arr_multiply[WINKY_SOURCE_LINKY] = (teleinfo_winky.arr_multiply[WINKY_SOURCE_USB] + teleinfo_winky.arr_multiply[WINKY_SOURCE_CAPA]) / 2;
      }
    }
  }

  // if no usb, declare tasmota on battery
  if (teleinfo_winky.arr_voltage[WINKY_SOURCE_USB] < arrTeleinfoWinkyCharged[WINKY_SOURCE_USB]) teleinfo_config.battery = 1;
    else teleinfo_config.battery = 0;
}

// check winky configuration
bool TeleinfoWinkyIsBootPossible ()
{
  // if ADC not configured, return voltage ok
  if (!TeleinfoWinkyIsConfigured ()) return true;

  // update voltage
  TeleinfoWinkyUpdateVoltage ();

  // if USB connected, ok
  if (teleinfo_winky.arr_voltage[WINKY_SOURCE_USB] >= arrTeleinfoWinkyCharged[WINKY_SOURCE_USB]) return true;
 
  // if Capa charged enought, ok
  if (teleinfo_winky.arr_voltage[WINKY_SOURCE_CAPA] >= arrTeleinfoWinkyCharged[WINKY_SOURCE_CAPA]) return true;

  // if no deep sleep, ok
  if (Settings->deepsleep == 0) return true;

  return false;
}

// check winky configuration
bool TeleinfoWinkyIsVoltageCorrect ()
{
  // if ADC not configured, return voltage ok
  if (!TeleinfoWinkyIsConfigured ()) return true;

  // update voltage
  TeleinfoWinkyUpdateVoltage ();

  // if USB connected, ok
  if (teleinfo_winky.arr_voltage[WINKY_SOURCE_USB] >= arrTeleinfoWinkyCharged[WINKY_SOURCE_USB]) return true;
 
  // if Capa charged enought, ok
  if (teleinfo_winky.arr_voltage[WINKY_SOURCE_CAPA] >= arrTeleinfoWinkyDischarged[WINKY_SOURCE_CAPA]) return true;

  // if no deep sleep, ok
  if (Settings->deepsleep == 0) return true;

  return false;  
}

// Enter deep sleep mode
void TeleinfoWinkyEnterSleepMode ()
{
  // if deep sleep less than 30 sec, ignore
  if (Settings->deepsleep < WINKY_SLEEP_MINIMUM) return;

  // enter deepsleep
  DeepSleepStart ();
}

/************************************\
 *               MQTT
\************************************/

// Generate WINKY section
void TeleinfoWinkyPublish ()
{
  uint32_t stop_vcapa;
  uint32_t delay_total, delay_ip, delay_ntp, delay_mqtt;

  // calculate delays
  stop_vcapa  = teleinfo_winky.arr_voltage[WINKY_SOURCE_CAPA];
  delay_total = millis () - teleinfo_winky.time_boot;
  delay_ip    = teleinfo_winky.time_ip - teleinfo_winky.time_boot;
  delay_ntp   = teleinfo_winky.time_ntp - teleinfo_winky.time_boot;
  delay_mqtt  = teleinfo_winky.time_mqtt - teleinfo_winky.time_boot;

  // message
  ResponseClear ();
  ResponseAppend_P (PSTR ("{\"WINKY\":{\"count\":{\"msg\":%u,\"cosphi\":%d,\"boot\":%d,\"write\":%d}"), teleinfo_meter.nb_message, teleinfo_conso.cosphi.nb_measure + teleinfo_prod.cosphi.nb_measure, Settings->bootcount, Settings->save_flag);
  ResponseAppend_P (PSTR (",\"capa\":{\"start\":%u,\"stop\":%u}"), teleinfo_winky.vcap_boot, stop_vcapa);
  ResponseAppend_P (PSTR (",\"ms\":{\"ip\":%u,\"ntp\":%u,\"mqtt\":%u,\"awake\":%u}}}"), delay_ip, delay_ntp, delay_mqtt, delay_total);
  MqttPublishTeleSensor ();
}

/************************************\
 *             Callback
\************************************/

// Domoticz init message
void TeleinfoWinkyInit ()
{
  // load config
  TeleinfoWinkyLoadConfig ();

  // if voltage problem, entering deep sleep
  if (!TeleinfoWinkyIsBootPossible ()) TeleinfoWinkyEnterSleepMode ();

  // get start datatime_t TeleinfoHistoGetLastWrite (const uint8_t period)
  teleinfo_winky.time_boot = millis ();
  teleinfo_winky.vcap_boot = teleinfo_winky.arr_voltage[WINKY_SOURCE_CAPA];

  // publish static data asap
  if (teleinfo_config.calendar) teleinfo_meter.json.calendar = 1;
}

// called 10 times per second
void TeleinfoWinkyEvery100ms ()
{
  uint8_t result; 

  // if set, reset standard deepsleep
  deepsleep_flag = 0;

  // check for network connectivity
  if (teleinfo_winky.time_ip == UINT32_MAX)
  {
    if (WifiHasIPv4 ()) teleinfo_winky.time_ip = millis ();
    else if (WifiHasIPv6 ()) teleinfo_winky.time_ip = millis ();
  }

  // else check for mqtt connectivity
  else if ((teleinfo_winky.time_mqtt == UINT32_MAX) && MqttIsConnected ()) teleinfo_winky.time_mqtt = millis ();

  // check for NTP connexion
  if ((teleinfo_winky.time_ntp == UINT32_MAX) && RtcTime.valid) teleinfo_winky.time_ntp = millis ();

  // check that input voltage is not too low
  if (!TeleinfoWinkyIsVoltageCorrect ())
  {
    // stop serial reception
    TeleinfoSerialStop ();

    // log end
    AddLog (LOG_LEVEL_INFO, PSTR ("TIC: CAPA voltage low, switching off ..."));

    // publish dynamic data
    TeleinfoDriverPublishTrigger ();
    if (teleinfo_meter.json.meter)    TeleinfoDriverPublishMeter ();
    if (teleinfo_meter.json.relay)    TeleinfoDriverPublishRelay ();
    if (teleinfo_meter.json.contract) TeleinfoDriverPublishContract ();
    if (teleinfo_meter.json.tic)      TeleinfoDriverPublishTic ();
    if (teleinfo_meter.json.alert)    TeleinfoDriverPublishAlert ();

#ifdef USE_TELEINFO_DOMOTICZ
    // if voltage is not too low, publish Domoticz data
    if (!TeleinfoWinkyIsVoltageCorrect ()) DomoticzIntegrationPublishAll ();
#endif    // USE_TELEINFO_DOMOTICZ

#ifdef USE_TELEINFO_HOMIE
    // if voltage is not too low, publish Homie data
    if (!TeleinfoWinkyIsVoltageCorrect ()) HomieIntegrationPublishAllData ();
#endif    // USE_TELEINFO_HOMIE

    // if voltage is not too low, publish WINKY message
    if (!TeleinfoWinkyIsVoltageCorrect ()) TeleinfoWinkyPublish ();

    // enter deep sleep
    TeleinfoWinkyEnterSleepMode ();
  }
}

#ifdef USE_WEBSERVER

// Append winky state to main page
void TeleinfoWinkyWebSensor ()
{
  uint8_t  index, level;
  char     str_name[16];
  char     str_color[8];

  // if not configured, ignore
  if (!TeleinfoWinkyIsConfigured ()) return;

  // start
  WSContentSend_P (PSTR ("<div style='font-size:13px;text-align:center;padding:4px 6px;margin-bottom:5px;background:#333333;border-radius:12px;'>\n"));
  WSContentSend_P (PSTR ("<div style='width:100%%;padding:0px;text-align:left;font-weight:bold;'>Winky</div>\n"));

  for (index = 0; index < WINKY_SOURCE_MAX; index++)
  {
    // calculate voltage criticity level
    if (teleinfo_winky.arr_voltage[index] <= arrTeleinfoWinkyCritical[index]) level = WINKY_LEVEL_CRITICAL;
      else if (teleinfo_winky.arr_voltage[index] <= arrTeleinfoWinkyDischarged[index]) level = WINKY_LEVEL_DISCHARGED;
      else if (teleinfo_winky.arr_voltage[index] <= arrTeleinfoWinkyCharged[index]) level = WINKY_LEVEL_CORRECT;
      else level = WINKY_LEVEL_CHARGED;

    // get input name and display color
    GetTextIndexed (str_name,  sizeof (str_name),  index, kTeleinfoWinkySource);
    GetTextIndexed (str_color, sizeof (str_color), level, kTeleinfoWinkyColor);

    // display
    WSContentSend_P (PSTR ("<div style='display:flex;padding:2px 0px;color:%s'>\n"), str_color);
    WSContentSend_P (PSTR ("<div style='width:16%%;padding:0px;'></div>\n"));
    WSContentSend_P (PSTR ("<div style='width:30%%;padding:0px;text-align:left;'>%s</div>\n"), str_name);
    WSContentSend_P (PSTR ("<div style='width:22%%;padding:0px;text-align:left;color:grey;'>x%u</div>\n"), teleinfo_winky.arr_multiply[index]);
    WSContentSend_P (PSTR ("<div style='width:20%%;padding:0px;text-align:right;font-weight:bold;'>%u.%02u</div>\n"), teleinfo_winky.arr_voltage[index] / 1000, (teleinfo_winky.arr_voltage[index] % 1000) / 10);
    WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div><div style='width:10%%;padding:0px;text-align:left;'>V</div>\n"));
    WSContentSend_P (PSTR ("</div>\n"));
  }

  // end
  WSContentSend_P (PSTR ("</div>\n"));

  // update data reception
  TeleinfoProcessRealTime ();
}

#endif  // USE_WEBSERVER

/***************************************\
 *              Interface
\***************************************/

// Teleinfo sensor (for graph)
bool Xsns126 (uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function) 
  {
    case FUNC_INIT:
      TeleinfoWinkyInit ();
      break;
    case FUNC_EVERY_100_MSECOND:
      TeleinfoWinkyEvery100ms ();
      break;
    case FUNC_SAVE_BEFORE_RESTART:
      TeleinfoWinkySaveConfig ();
      break;

#ifdef USE_WEBSERVER
     case FUNC_WEB_SENSOR:
      TeleinfoWinkyWebSensor ();
      break;
#endif  // USE_WEBSERVER
  }

  return result;
}

#endif    // USE_WINKY
#endif    // USE_TELEINFO
