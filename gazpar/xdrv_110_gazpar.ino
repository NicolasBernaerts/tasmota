/*
  xdrv_110_gazpar.ino - Enedis Gazpar Gas meter support for Tasmota

  Copyright (C) 2022-2024  Nicolas Bernaerts

  Gazpar impulse connector should be declared as Input 1

  Config is stored in Settings :
      Settings->weight_max         = power factor
      Settings->weight_item        = gazpar total;
      Settings->weight_reference   = today's total
      Settings->weight_calibration = yesterday's total

  Version history :
    28/04/2022 - v1.0 - Creation
    06/11/2022 - v1.1 - Rename to xsns_119
    04/12/2022 - v1.2 - Add graphs
    15/05/2023 - v1.3 - Rewrite CSV file access
    07/05/2024 - v2.0 - JSON data change
                        Add active power calculation
                        Homie and Home Assistant integration
    10/05/2024 - v3.0 - Use interrupt for gazpar increment

  RAM : 48 bytes
  
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

// declare gazpar energy driver
#define XDRV_110                    110

#define D_GAZPAR_FACTOR             "Facteur de Conversion"

// commands
#define D_CMND_GAZPAR               "gaz"
#define D_CMND_GAZPAR_FACTOR        "factor"
#define D_CMND_GAZPAR_TOTAL         "total"
#define D_CMND_GAZPAR_DOMOTICZ      "domo"
#define D_CMND_GAZPAR_HASS          "hass"
#define D_CMND_GAZPAR_HOMIE         "homie"

// default values
#define GAZPAR_FACTOR_DEFAULT         1120
#define GAZPAR_INCREMENT_MAXTIME      360000    // 6mn (equivalent to 500W, a small gaz cooker)
#define GAZPAR_INCREMENT_TIMEOUT      10000     // 10s (power will go to 0 after this timeout)
#define GAZPAR_PULSE_DEBOUNCE         50000     // meter pulse debounce (50 ms)
 
// web URL
#define D_GAZPAR_PAGE_CONFIG          "/cfg"

// Gazpar - MQTT commands : gaz_count
static const char kGazparCommands[]  PROGMEM =   D_CMND_GAZPAR "|" "|_" D_CMND_GAZPAR_FACTOR "|_" D_CMND_GAZPAR_TOTAL;
void (* const GazparCommand[])(void) PROGMEM = { &CmndGazparHelp,         &CmndGazparFactor   ,    &CmndGazparTotal };

// published data 
enum GazparPublish { GAZ_PUB_CONNECT,
                     GAZ_PUB_DATA, GAZ_PUB_FACTOR, GAZ_PUB_POWER,
                     GAZ_PUB_M3, GAZ_PUB_M3_TOTAL, GAZ_PUB_M3_TODAY, GAZ_PUB_M3_YTDAY, 
                     GAZ_PUB_WH, GAZ_PUB_WH_TOTAL, GAZ_PUB_WH_TODAY, GAZ_PUB_WH_YTDAY, 
                     GAZ_PUB_MAX, 
                     GAZ_PUB_DISCONNECT };

/*********************************\
 *              Data
\*********************************/

// gazpar : configuration         // 16 bytes
static struct {
  long total       = 0;                           // global total
  long total_today = 0;                           // today's_today
  long total_ytday = 0;                           // yesterday's total
  long power_factor = GAZPAR_FACTOR_DEFAULT;      // power factor
} gazpar_config;

// gazpar : meter                 // 32 bytes
static struct {
  uint8_t  gpio        = UINT8_MAX;               // gazpar GPIO
  uint8_t  pulse       = 0;                       // current pulse counter
  uint8_t  publish     = 0;                       // flag to publish JSON
  uint8_t  hour        = UINT8_MAX;               // current hour
  uint32_t last_stamp  = UINT32_MAX;              // millis of last detected increment
  uint32_t last_length = 0;                       // length of last increment
  int64_t  pulse_start = 0;                       // microsecond pulse start timestamp
  int64_t  pulse_stop  = 0;                       // microsecond pulse stop timestamp
  long     power       = 0;                       // active power
} gazpar_meter;

/****************************************\
 *               Icons
\****************************************/

// gazpar icon
static const char pstr_gazpar_icon_svg[] PROGMEM =
"<svg xmlns='http://www.w3.org/2000/svg' viewBox='230 70 350 440'>"
"<defs><style>.cls-1{fill:#f1f2f3;}.cls-2{fill:#f8cd46;}.cls-3{fill:#f6c243;}.cls-4{fill:#fff;}.cls-5,.cls-9{fill:#d8e0ed;}.cls-5{opacity:0.38;}.cls-6{fill:#353a4b;}.cls-7{fill:#e15f60;}.cls-8{font-size:12.56px;fill:#e3e9fc;font-family:Nunito-Black, Nunito;font-weight:800;}.cls-10{fill:#cfd7e3;}</style></defs>"
"<rect class='cls-1' x='236.74' y='114.95' width='334.03' height='391.29' rx='40.21'/>"
"<path class='cls-2' d='M565.77,463.9c0,20-16.54,36.37-36.76,36.37H278.49c-20.21,0-36.75-16.37-36.75-36.37v-308c0-20,16.54-36.38,36.75-36.38H529c20.22,0,36.76,16.37,36.76,36.38Z'/>"
"<path class='cls-3' d='M527.86,489.39H279.65A27.66,27.66,0,0,1,252,461.73V158.25a27.67,27.67,0,0,1,27.67-27.67H527.86a27.66,27.66,0,0,1,27.66,27.66V282.31c0,8-6.15,14.94-14.13,15.18a14.58,14.58,0,0,1-15-14.57V162.07a2.32,2.32,0,0,0-2.32-2.32H283.47a2.32,2.32,0,0,0-2.32,2.32V457.9a2.32,2.32,0,0,0,2.32,2.32H524a2.32,2.32,0,0,0,2.32-2.32V357.64a2.81,2.81,0,0,0-2.8-2.8H323.94c-8,0-14.95-6.15-15.19-14.13a14.59,14.59,0,0,1,14.58-15h225.8a6.39,6.39,0,0,1,6.39,6.39V461.73A27.66,27.66,0,0,1,527.86,489.39Z'/>"
"<rect class='cls-4' x='310.65' y='176.43' width='132.25' height='100.08' rx='6.89'/><rect class='cls-5' x='327.22' y='191' width='40.06' height='8.08' rx='4.04'/>"
"<rect class='cls-5' x='327.22' y='255.32' width='40.06' height='4.04' rx='2.02'/><rect class='cls-5' x='327.22' y='263.3' width='26.21' height='4.04' rx='2.02'/><rect class='cls-5' x='371.58' y='255.32' width='23.09' height='4.04' rx='2.02'/>"
"<rect class='cls-5' x='327.22' y='205.72' width='4.04' height='8.08' rx='1.28'/><rect class='cls-5' x='332.78' y='205.72' width='1.1' height='8.08' rx='0.55'/><rect class='cls-5' x='334.75' y='205.72' width='1.93' height='8.08' rx='0.73'/>"
"<rect class='cls-5' x='337.71' y='205.72' width='3.28' height='8.08' rx='0.95'/><rect class='cls-5' x='342.3' y='205.72' width='0.99' height='8.08' rx='0.5'/><rect class='cls-5' x='344.76' y='205.72' width='1.37' height='8.08' rx='0.58'/>"
"<rect class='cls-5' x='351.47' y='205.72' width='1.1' height='8.08' rx='0.55'/><rect class='cls-5' x='353.43' y='205.72' width='1.93' height='8.08' rx='0.73'/><rect class='cls-5' x='360' y='205.72' width='0.99' height='8.08' rx='0.5' transform='translate(720.99 419.51) rotate(180)'/>"
"<rect class='cls-5' x='357.17' y='205.72' width='1.37' height='8.08' rx='0.58' transform='translate(715.71 419.52) rotate(180)'/><rect class='cls-5' x='347.25' y='205.71' width='2.71' height='8.08' rx='0.82'/>"
"<rect class='cls-5' x='382.61' y='205.71' width='3.28' height='8.08' rx='0.95' transform='translate(768.5 419.5) rotate(180)'/><rect class='cls-5' x='380.3' y='205.71' width='0.99' height='8.08' rx='0.5' transform='translate(761.6 419.5) rotate(180)'/>"
"<rect class='cls-5' x='377.48' y='205.72' width='1.37' height='8.08' rx='0.58' transform='translate(756.32 419.51) rotate(180)'/><rect class='cls-5' x='371.03' y='205.71' width='1.1' height='8.08' rx='0.55' transform='translate(743.17 419.5) rotate(180)'/>"
"<rect class='cls-5' x='368.24' y='205.71' width='1.93' height='8.08' rx='0.73' transform='translate(738.41 419.5) rotate(180)'/><rect class='cls-5' x='362.61' y='205.72' width='0.99' height='8.08' rx='0.5'/><rect class='cls-5' x='365.06' y='205.71' width='1.37' height='8.08' rx='0.58'/>"
"<rect class='cls-5' x='373.65' y='205.72' width='2.71' height='8.08' rx='0.82' transform='translate(750 419.52) rotate(180)'/>"
"<rect class='cls-6' x='320.22' y='224.96' width='170.23' height='23.03' rx='3.99'/>"
"<path class='cls-7' d='M399,225h32.07a4,4,0,0,1,4,4V244a4,4,0,0,1-4,4H391.39a0,0,0,0,1,0,0V225a0,0,0,0,1,0,0Z'/>"
"<text class='cls-8' id='digit' transform='translate(324 240)'>0 0 0 0 0</text><text class='cls-8' id='milli' transform='translate(395 240)'>0 0 0</text>"
"<path class='cls-9' d='M306.25,71.65h51.23a4.41,4.41,0,0,1,4.41,4.41v38.89a0,0,0,0,1,0,0h-60a0,0,0,0,1,0,0V76.06A4.41,4.41,0,0,1,306.25,71.65Z'/>"
"<path class='cls-10' d='M299.56,103.48h64.61a5,5,0,0,1,5,5v6.51a0,0,0,0,1,0,0H294.59a0,0,0,0,1,0,0v-6.51A5,5,0,0,1,299.56,103.48Z'/>"
"<path class='cls-9' d='M455.38,71.65h51.23A4.41,4.41,0,0,1,511,76.06v38.89a0,0,0,0,1,0,0H451a0,0,0,0,1,0,0V76.06A4.41,4.41,0,0,1,455.38,71.65Z'/>"
"<path class='cls-10' d='M448.7,103.48H513.3a5,5,0,0,1,5,5v6.51a0,0,0,0,1,0,0H443.73a0,0,0,0,1,0,0v-6.51A5,5,0,0,1,448.7,103.48Z'/>"
"<rect class='cls-3' x='310.65' y='402.72' width='64.59' height='8.3' rx='4.15'/><rect class='cls-3' x='310.65' y='418.56' width='40.68' height='8.3' rx='4.15'/>"
"</svg>";

/************************************\
 *             Commands
\************************************/

void CmndGazparHelp (void)
{
  AddLog (LOG_LEVEL_INFO,   PSTR ("HLP: Gazpar commands"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - gaz_factor <value>   = facteur de conversion"));
  AddLog (LOG_LEVEL_INFO,   PSTR ("   https://www.grdf.fr/particuliers/coefficient-conversion-commune"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - gaz_total <value>    = mise a jour total general"));

  ResponseCmndDone();
}

void CmndGazparFactor (void)
{
  float factor;

  if (XdrvMailbox.data_len > 0)
  {
    factor = 100 * atof (XdrvMailbox.data);
    GazparSetFactor ((long)factor);
  }
  factor = (float)gazpar_config.power_factor / 100;
  ResponseCmndFloat (factor, 2);
}

void CmndGazparTotal (void)
{
  // update new counters according to new total
  if (XdrvMailbox.data_len > 0) GazparSetTotal ((long)XdrvMailbox.payload);

  ResponseCmndNumber (gazpar_config.total);
}

/************************************\
 *            Functions
\************************************/

// Load configuration from Settings or from LittleFS
void GazparLoadConfig () 
{
  gazpar_config.power_factor = (long)Settings->weight_max;
  gazpar_config.total        = (long)Settings->weight_item;
  gazpar_config.total_today  = (long)Settings->weight_reference;
  gazpar_config.total_ytday  = (long)Settings->weight_calibration;

  // set default values
  if (gazpar_config.power_factor == 0) gazpar_config.power_factor = GAZPAR_FACTOR_DEFAULT;
}

// Save configuration to Settings or to LittleFS
void GazparSaveConfig () 
{
  Settings->weight_max         = (uint16_t)gazpar_config.power_factor;
  Settings->weight_item        = (uint32_t)gazpar_config.total;
  Settings->weight_reference   = (uint32_t)gazpar_config.total_today;
  Settings->weight_calibration = (uint32_t)gazpar_config.total_ytday;
}

void GazparSetFactor (const long new_factor)
{
  // check value
  if (new_factor == 0) return;

  // set new factor
  gazpar_config.power_factor = new_factor;
  GazparSaveConfig () ;
}

void GazparSetTotal (const long new_total)
{
  // check value
  if (new_total < 0) return;

  // set new total
  gazpar_config.total = new_total;
  GazparSaveConfig () ;

  // ask to publish JSON
  gazpar_meter.publish = 1;
}

// Convert from Wh to liter
long GazparConvertWh2Liter (const long wh)
{ 
  long liter = LONG_MAX;

  if (gazpar_config.power_factor > 0) liter = wh * 100 / gazpar_config.power_factor;

  return liter;
}

// Convert from liter to wh
long GazparConvertLiter2Wh (const long liter)
{
  long long wh;
  
  wh = (long long)liter * (long long)gazpar_config.power_factor / 1000;

  return (long)wh;
}

// convert a delay in ms to a readable label
void GazparConvertDelay2Label (const uint32_t delay, char* pstr_result, const int size_result)
{
  uint32_t minute, second;

  // check parameters
  if (pstr_result == nullptr) return;
  if (size_result < 16) return;

  // convert delay to minute and seconds
  second = delay / 1000;
  minute = second / 60;
  second = second % 60;

  // generate label
  if (minute == 0) sprintf_P (pstr_result, PSTR ("(%us)"), second);
    else sprintf_P (pstr_result, PSTR ("(%umn %us)"), minute, second);
}

// Convert wh/liter kWh with kilo conversion (120, 2.0k, 12.0k, 2.3M, ...)
void GazparConvertWh2Label (const bool is_liter, const bool compact, const long value, char* pstr_result, const int size_result) 
{
//  float result;
  long result = value;
  char  str_sep[2];

  // check parameters
  if (pstr_result == nullptr) return;
  if (size_result < 12) return;

  // set separator
  if (compact) strcpy_P (str_sep, PSTR ("")); 
    else strcpy_P (str_sep, PSTR (" "));

  // convert liter counter to Wh
  if (is_liter) result = GazparConvertLiter2Wh (10L * value);

  // convert values in M with 1 digit
  if (result > 9999999) sprintf_P (pstr_result, PSTR ("%d.%01d%sM"), result / 1000000, result % 1000000 / 100000, str_sep);

   // convert values in M with 2 digits
  else if (result > 999999) sprintf_P (pstr_result, PSTR ("%d.%02d%sM"), result / 1000000, result % 1000000 / 10000, str_sep);

  // else convert values in k with no digit
  else if (result > 99999) sprintf_P (pstr_result, PSTR ("%d%sk"), result / 1000, str_sep);

  // else convert values in k with one digit
  else if (result > 9999) sprintf_P (pstr_result, PSTR ("%d.%01d%sk"), result / 1000, result % 1000 / 100, str_sep);

  // else convert values in k with two digits
  else if (result > 999) sprintf_P (pstr_result, PSTR ("%d.%02d%sk"), result / 1000, result % 1000 / 10, str_sep);

  // else no conversion
  else sprintf_P (pstr_result, PSTR ("%d%s"), result, str_sep);
}

#ifdef USE_WEBSERVER

// Get specific argument as a value with min and max
int GazparGetArgValue (const char* pstr_argument, const int value_min, const int value_max, const int value_default)
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

#endif    // WEBSERVER

/**********************************\
 *            Callback
\**********************************/

#ifdef ESP8266

int64_t esp_timer_get_time ()
{
  int64_t ts_now;
  struct  timeval tv_now; 

  gettimeofday (&tv_now, NULL);
  ts_now = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;

  return ts_now;
}

#endif    // ESP8266

void ICACHE_RAM_ATTR GazparInterruptPulse ()
{
  uint8_t level;
  int64_t timestamp;

  // get current timestamp and GPIO level
  timestamp = esp_timer_get_time ();
  level = digitalRead (gazpar_meter.gpio);

  // if low level just detected : pulse started
  if ((level == LOW) && (gazpar_meter.pulse_start == 0) && (timestamp > gazpar_meter.pulse_stop + GAZPAR_PULSE_DEBOUNCE)) gazpar_meter.pulse_start = timestamp;

  // else high level just detected : pulse stopped
  else if ((level == HIGH) && (gazpar_meter.pulse_start > 0) && (timestamp > gazpar_meter.pulse_start + GAZPAR_PULSE_DEBOUNCE)) gazpar_meter.pulse_stop = timestamp;
}

// Gazpar module initialisation
void GazparPreInit ()
{
  uint8_t pin_rx;

  // if TinfoRx pin not set, return
  if (!PinUsed (GPIO_INPUT, 0)) return;

  // init levels
  gazpar_meter.gpio = Pin (GPIO_INPUT, 0);
  pinMode(gazpar_meter.gpio, INPUT_PULLUP);
  attachInterrupt (gazpar_meter.gpio, GazparInterruptPulse, CHANGE);

  // log gpio
  AddLog (LOG_LEVEL_INFO, PSTR ("GAZ: Gazpar detected on GPIO%u"), gazpar_meter.gpio);
}
 
// Gazpar driver initialisation
void GazparInit ()
{
  uint8_t index;

  // init meter data
  gazpar_meter.last_stamp = millis ();                               // init counter value

  // load configuration
  GazparLoadConfig ();

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: gaz to get help on Gazpar specific commands"));
}

// Called every 50 ms
void GazparEvery50ms ()
{
  uint32_t time_now;
  int64_t  pulse_ms;

  // if no trigger started, ignore
  if (gazpar_meter.pulse_start == 0) return;
  if (gazpar_meter.pulse_stop < gazpar_meter.pulse_start) return;

  // get timestamp
  time_now = millis ();
  pulse_ms = (gazpar_meter.pulse_stop - gazpar_meter.pulse_start) / 1000;

  // update duration and last timestamp
  gazpar_meter.last_length = (long)TimeDifference (gazpar_meter.last_stamp, time_now);
  gazpar_meter.last_stamp  = time_now;

  // cut length to 6mn max to get 500W minimum (small gaz cooker)
  if (gazpar_meter.last_length > GAZPAR_INCREMENT_MAXTIME) gazpar_meter.last_length = GAZPAR_INCREMENT_MAXTIME;

  // calculate equivalent power
  if (gazpar_meter.last_length > 0) gazpar_meter.power = 360 * 1000 * gazpar_config.power_factor / gazpar_meter.last_length;
    else gazpar_meter.power = 0;

  // increase global total
  gazpar_config.total++;
  gazpar_config.total_today++;

  // reset trigger and publish on MQTT
  gazpar_meter.pulse_start = 0;
  gazpar_meter.publish     = 1;

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("GAZ: %u ms pulse, counter is %u.%02u"), (uint32_t)pulse_ms, gazpar_config.total / 100, gazpar_config.total % 100);
}

// Called every second
void GazparEverySecond ()
{
  // ignore if time not set
  if (!RtcTime.valid) return;
  
  // set current date stamp first time
  if (gazpar_meter.hour == UINT8_MAX) gazpar_meter.hour = RtcTime.hour;

  // if hour changed
  if (gazpar_meter.hour != RtcTime.hour)
  {
    // if day changed, save daily data
    if (gazpar_meter.hour > RtcTime.hour)
    {
      // update today and yesterday
      gazpar_config.total_ytday = gazpar_config.total_today;
      gazpar_config.total_today = 0;

      // publish on MQTT
      gazpar_meter.publish = 1;
    }

    // save config
    GazparSaveConfig ();
  }

  // update current hour
  gazpar_meter.hour = RtcTime.hour;

  // if last increment duration + 30s reached, reset power
  if ((gazpar_meter.power > 0) && (TimeDifference (gazpar_meter.last_stamp, millis ()) > gazpar_meter.last_length + GAZPAR_INCREMENT_TIMEOUT))
  {
    gazpar_meter.power   = 0;
    gazpar_meter.publish = 1;
  }
  
  // if needed, publish JSON
  if (gazpar_meter.publish) GazparShowJSON (false);
}

// Show JSON status (for MQTT)
void GazparShowJSON (bool append)
{
  long factor, total, today, ytday;

  // reset publication flag
  gazpar_meter.publish = 0;

  // if not a telemetry call, add {"Time":"xxxxxxxx",
  if (!append) Response_P (PSTR ("{\"%s\":\"%s\""), PSTR (D_JSON_TIME), GetDateAndTime (DT_LOCAL).c_str ());

  // Start Gazpar section "Gazpar":{
  ResponseAppend_P (PSTR (",\"gazpar\":{"));

  // factor
  factor = (long)gazpar_config.power_factor;
  ResponseAppend_P (PSTR ("\"factor\":%d.%02d"), factor / 100, factor % 100);

  // active power
  ResponseAppend_P (PSTR (",\"power\":%d"), gazpar_meter.power);

  // m3
  total = gazpar_config.total;
  today = gazpar_config.total_today;
  ytday = gazpar_config.total_ytday;
  ResponseAppend_P (PSTR (",\"m3\":{\"total\":%d.%02d,\"today\":%d.%02d,\"yesterday\":%d.%02d}"), total / 100, total % 100, today / 100, today % 100, ytday / 100, ytday % 100);

  // Wh
  total = GazparConvertLiter2Wh (10L * total);
  today = GazparConvertLiter2Wh (10L * today);
  ytday = GazparConvertLiter2Wh (10L * ytday);
  ResponseAppend_P (PSTR (",\"wh\":{\"total\":%d,\"today\":%d,\"yesterday\":%d}"), total, today, ytday);

  // end of Gazpar section
  ResponseAppend_P (PSTR ("}"));

  // if not in telemetry, publish JSON and process rulesdd
  if (!append)
  {
    ResponseAppend_P (PSTR ("}"));
    MqttPublishTeleSensor ();
  }

#ifdef USE_TELEINFO_DOMOTICZ
  DomoticzIntegrationPublishTrigger ();
#endif    // USE_TELEINFO_DOMOTICZ

#ifdef USE_TELEINFO_HOMIE
  HomieIntegrationPublishData ();
#endif    // USE_TELEINFO_HOMIE
}

/**********************************\
 *               Web
\**********************************/

#ifdef USE_WEBSERVER

// Display gazpar icon
void GazparWebIconSvg () { Webserver->send_P (200, PSTR ("image/svg+xml"), pstr_gazpar_icon_svg, strlen_P (pstr_gazpar_icon_svg)); }

// Get specific argument as a value with min and max
long GazparWebGetArgValue (const char* pstr_argument, const long value_min, const long value_max, const long value_default)
{
  long result = value_default;
  char str_value[8];

  // check for argument
  if (Webserver->hasArg (pstr_argument))
  {
    WebGetArg (pstr_argument, str_value, sizeof (str_value));
    result = atol (str_value);
  }

  // check for min and max value
  if ((value_min >= 0) && (result <= value_min)) result = value_min;
  if ((value_max >= 0) && (result >= value_max)) result = value_max;

  return result;
}

// Append Gazpar configuration button to configuration page
void GazparWebConfigButton ()
{
  // Gazpar configuration button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>Gazpar</button></form></p>\n"), PSTR (D_GAZPAR_PAGE_CONFIG));
}

// Gazpar configuration page
void GazparWebPageConfigure ()
{
  bool status, actual;
  char str_text[32];
  char str_select[16];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess ()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg ("save"))
  {
    // parameter 'total' : set counter total
    WebGetArg (PSTR (D_CMND_GAZPAR_TOTAL), str_text, sizeof (str_text));
    if (strlen (str_text) > 0) GazparSetTotal ((long)(atof (str_text) * 100));

    // parameter 'factor' : set conversion factor
    WebGetArg (PSTR (D_CMND_GAZPAR_FACTOR), str_text, sizeof (str_text));
    if (strlen (str_text) > 0) GazparSetFactor ((long)(atof (str_text) * 100));

    // parameter 'hass' : set home assistant integration
#ifdef USE_GAZPAR_HASS
    WebGetArg (PSTR (D_CMND_GAZPAR_HASS), str_text, sizeof (str_text));
    status = (strlen (str_text) > 0);
    actual = HassIntegrationGet ();
    if (actual != status) HassIntegrationSet (status);
#endif    // USE_GAZPAR_HASS

    // parameter 'homie' : set homie integration
#ifdef USE_GAZPAR_HOMIE
    WebGetArg (PSTR (D_CMND_GAZPAR_HOMIE), str_text, sizeof (str_text));
    status = (strlen (str_text) > 0);
    actual = HomieIntegrationGet ();
    if (actual != status) HomieIntegrationSet (status);
#endif    // USE_GAZPAR_HOMIE

    // parameter 'domo' : set domoticz integration
#ifdef USE_GAZPAR_DOMOTICZ
    WebGetArg (PSTR (D_CMND_GAZPAR_DOMOTICZ), str_text, sizeof (str_text));
    status = (strlen (str_text) > 0);
    actual = DomoticzIntegrationGet ();
    if (actual != status) DomoticzIntegrationSet (status);

    WebGetArg (PSTR ("idx0"), str_text, sizeof (str_text));
    Settings->domoticz_sensor_idx[0] = (uint16_t) atoi (str_text);

    WebGetArg (PSTR ("idx1"), str_text, sizeof (str_text));
    Settings->domoticz_sensor_idx[1] = (uint16_t) atoi (str_text);
#endif    // USE_GAZPAR_DOMOTICZ

    // save configuration
    GazparSaveConfig ();
  }

  // beginning of form
  WSContentStart_P (PSTR ("Configure Gazpar"));
  WSContentSendStyle ();

  // page style
  WSContentSend_P (PSTR ("\n<style>\n"));
  WSContentSend_P (PSTR ("p,br,hr {clear:both;}\n"));
  WSContentSend_P (PSTR ("p.dat {margin:0px 10px;}\n"));
  WSContentSend_P (PSTR ("fieldset {background:#333;margin:15px 0px;border:none;border-radius:8px;}\n"));
  WSContentSend_P (PSTR ("legend {font-weight:bold;margin:0px;padding:5px;color:#888;background:transparent;}\n"));
  WSContentSend_P (PSTR ("legend:after {content:'';display:block;height:1px;margin:15px 0px;}\n"));
  WSContentSend_P (PSTR ("div.main {padding:0px;margin-top:-25px;}\n"));
  WSContentSend_P (PSTR ("input,select {margin-bottom:10px;text-align:center;border:none;}\n"));
  WSContentSend_P (PSTR ("span {float:left;padding-top:4px;}\n"));
  WSContentSend_P (PSTR ("span.hea {width:55%%;}\n"));
  WSContentSend_P (PSTR ("span.sub {width:47%%;padding-left:8%%;}\n"));
  WSContentSend_P (PSTR ("span.val {width:30%%;padding:0px;}\n"));
  WSContentSend_P (PSTR ("span.uni {width:15%%;text-align:center;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // page start
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), PSTR (D_GAZPAR_PAGE_CONFIG));

  // teleinfo message diffusion type
  WSContentSend_P (PSTR ("<fieldset><legend><b>📊 Paramètres</b></legend>\n"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));
  WSContentSend_P (PSTR ("<p class='dat'><span class='hea'>Total du compteur</span><span class='val'><input type='number' name='%s' step=0.01 placeholder='%d.%02d'></span><span class='uni'>m³</span></p>\n"), PSTR (D_CMND_GAZPAR_TOTAL), gazpar_config.total / 100, gazpar_config.total % 100);
  WSContentSend_P (PSTR ("<p class='dat'><span class='hea'>Coefficient de conversion</span><span class='val'><input type='number' name='%s' min=9 max=15 step=0.01 value=%d.%02d></span><span class='uni'></span></p>\n"), PSTR (D_CMND_GAZPAR_FACTOR), gazpar_config.power_factor / 100, gazpar_config.power_factor % 100);
  WSContentSend_P (PSTR ("</div>\n"));
  WSContentSend_P (PSTR ("</fieldset>\n"));

  // domotic integration
#if defined USE_GAZPAR_DOMOTICZ || defined USE_GAZPAR_HASS || defined USE_GAZPAR_HOMIE
  WSContentSend_P (PSTR ("<fieldset><legend>🏠 Intégration</legend>\n"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

#ifdef USE_GAZPAR_HASS
  if (HassIntegrationGet ()) strcpy_P (str_select, PSTR ("checked")); else strcpy_P (str_select, PSTR (""));
  WSContentSend_P (PSTR ("<p class='dat'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), PSTR (D_CMND_GAZPAR_HASS), str_select, PSTR (D_CMND_GAZPAR_HASS), PSTR ("Home Assistant"));
#endif    // USE_GAZPAR_HASS

#ifdef USE_GAZPAR_HOMIE
  if (HomieIntegrationGet ()) strcpy_P (str_select, PSTR ("checked")); else strcpy_P (str_select, PSTR (""));
  WSContentSend_P (PSTR ("<p class='dat'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), PSTR (D_CMND_GAZPAR_HOMIE), str_select, PSTR (D_CMND_GAZPAR_HOMIE), PSTR ("Homie"));
#endif    // USE_GAZPAR_HOMIE

#ifdef USE_GAZPAR_DOMOTICZ
  if (DomoticzIntegrationGet ()) strcpy_P (str_select, PSTR ("checked")); else strcpy_P (str_select, PSTR (""));
  WSContentSend_P (PSTR ("<p class='dat'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), PSTR (D_CMND_GAZPAR_DOMOTICZ), str_select, PSTR (D_CMND_GAZPAR_DOMOTICZ), PSTR ("Domoticz"));
  WSContentSend_P (PSTR ("<p class='dat'><span class='sub'>Index %s</span><span class='val'><input type='number' name='idx0' min=0 max=65535 step=1 value=%u></span><span class='uni'></span></p>\n"), PSTR ("Volume"), Settings->domoticz_sensor_idx[0]);
  WSContentSend_P (PSTR ("<p class='dat'><span class='sub'>Index %s</span><span class='val'><input type='number' name='idx0' min=0 max=65535 step=1 value=%u></span><span class='uni'></span></p>\n"), PSTR ("Puissance"), Settings->domoticz_sensor_idx[1]);
#endif    // USE_GAZPAR_DOMOTICZ

  WSContentSend_P (PSTR ("</div>\n"));
  WSContentSend_P (PSTR ("</fieldset>\n"));
#endif    // USE_GAZPAR_DOMOTICZ || USE_GAZPAR_HASS || USE_GAZPAR_HOMIE

  // save button
  WSContentSend_P (PSTR ("<br><button name='save' type='submit' class='button bgrn'>%s</button>"), D_SAVE);
  WSContentSend_P (PSTR ("</form>\n"));

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

// Append Teleinfo state to main page
void GazparWebSensor ()
{
  long     value;
  uint32_t delay;
  char str_unit[4];
  char str_class[8];
  char str_delay[16];
  char str_value[16];

  WSContentSend_P (PSTR ("<div style='font-size:13px;text-align:center;padding:4px 6px;margin-bottom:5px;background:#333333;border-radius:12px;'>\n"));

  // gazpar styles
  WSContentSend_P (PSTR ("<style>\n"));

  WSContentSend_P (PSTR (".meter span{padding:1px 4px;margin:2px;text-align:center;background:black;color:white;}\n"));
  WSContentSend_P (PSTR (".meter span.unit{border:solid 1px #888;}\n"));
  WSContentSend_P (PSTR (".meter span.cent{border:solid 1px #800;}\n"));
  WSContentSend_P (PSTR (".gaz span{padding-left:8px;font-size:9px;font-style:italic;}\n"));
  WSContentSend_P (PSTR (".white{color:white;}\n"));
  WSContentSend_P (PSTR (".grey1{color:#999;}\n"));
  WSContentSend_P (PSTR (".grey2{color:#444;}\n"));

  WSContentSend_P (PSTR ("table hr{display:none;}\n"));

  WSContentSend_P (PSTR (".gaz{display:flex;padding:2px 0px;}\n"));
  WSContentSend_P (PSTR (".gaz div{padding:0px;text-align:left;}\n"));
  WSContentSend_P (PSTR (".gaz .gazh{width:8%%;}\n"));
  WSContentSend_P (PSTR (".gaz .gazt{width:40%%;}\n"));
  WSContentSend_P (PSTR (".gaz .gazv{width:30%%;text-align:right;font-weight:bold;}\n"));
  WSContentSend_P (PSTR (".gaz .gazs{width:4%%;}\n"));
  WSContentSend_P (PSTR (".gaz .gazu{width:18%%;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // counter
  value = gazpar_config.total;
  WSContentSend_P (PSTR ("<div style='display:flex;margin:10px 0px;padding:0px;font-size:18px;font-weight:bold;'>\n"));

  WSContentSend_P (PSTR ("<div style='width:2%%;'></div>"));
  WSContentSend_P (PSTR ("<div class='meter' style='width:80%%;text-align:center;'>"));
  WSContentSend_P (PSTR ("<span class='unit'>%d</span>"), value % 100000000 / 10000000 );
  WSContentSend_P (PSTR ("<span class='unit'>%d</span>"), value % 10000000 / 1000000 );
  WSContentSend_P (PSTR ("<span class='unit'>%d</span>"), value % 1000000 / 100000 );
  WSContentSend_P (PSTR ("<span class='unit'>%d</span>"), value % 100000 / 10000 );
  WSContentSend_P (PSTR ("<span class='unit'>%d</span>"), value % 10000 / 1000 );
  WSContentSend_P (PSTR ("<span class='unit'>%d</span>"), value % 1000 / 100 );
  WSContentSend_P (PSTR ("&nbsp;.&nbsp;"));
  WSContentSend_P (PSTR ("<span class='cent'>%d</span>"), value % 100 / 10 );
  WSContentSend_P (PSTR ("<span class='cent'>%d</span>"), value % 10);
  WSContentSend_P (PSTR ("</div>"));
  WSContentSend_P (PSTR ("<div style='width:18%%;text-align:left;'>m³</div>\n"));

  WSContentSend_P (PSTR ("</div>\n"));

  // current power
  // if last increment duration + 30s reached, reset power
  delay = TimeDifference (gazpar_meter.last_stamp, millis ());
  if (gazpar_meter.power > 0)
  { 
    strcpy_P (str_unit, PSTR ("W"));
    ltoa (gazpar_meter.power, str_value, 10);
    GazparConvertDelay2Label (delay, str_delay, sizeof (str_delay));
  }
  else
  { 
    strcpy_P (str_unit,  PSTR (""));
    strcpy_P (str_value, PSTR (""));
    strcpy_P (str_delay, PSTR (""));
  }
  if (delay <= gazpar_meter.last_length) strcpy_P (str_class, PSTR ("white"));
    else if (delay <= gazpar_meter.last_length + GAZPAR_INCREMENT_TIMEOUT) strcpy_P (str_class, PSTR ("grey1"));
    else strcpy_P (str_class, PSTR ("grey2"));

  WSContentSend_P (PSTR ("<div class='gaz' ><div class='gazh'></div><div class='gazt'>Puissance<span class='%s'>%s</span></div>"), str_class, str_delay);
  WSContentSend_P (PSTR ("<div class='gazv %s'>%s</div>"), str_class, str_value);
  WSContentSend_P (PSTR ("<div class='gazs'></div><div class='gazu %s'>%s</div></div>\n"), str_class, str_unit);

  // today
  value = 10L * gazpar_config.total_today;
  value = GazparConvertLiter2Wh (value);
  WSContentSend_P (PSTR ("<div class='gaz'><div class='gazh'></div><div class='gazt'>%s</div><div class='gazv'>%d</div><div class='gazs'></div><div class='gazu'>Wh</div></div>\n"), PSTR ("Aujourd'hui"), value);

  // yesterday
  value = 10L * gazpar_config.total_ytday;
  value = GazparConvertLiter2Wh (value);
  WSContentSend_P (PSTR ("<div class='gaz'><div class='gazh'></div><div class='gazt'>%s</div><div class='gazv'>%d</div><div class='gazs'></div><div class='gazu'>Wh</div></div>\n"), PSTR ("Hier"), value);

  WSContentSend_P (PSTR ("<hr>\n"));

  // total
  value = 10L * gazpar_config.total;
  value = GazparConvertLiter2Wh (value);
  WSContentSend_P (PSTR ("<div class='gaz'><div class='gazh'></div><div class='gazt'>Total</div><div class='gazv'>%d.%03d</div><div class='gazs'></div><div class='gazu'>kWh</div></div>\n"), value / 1000, value % 1000);

  // conversion factor
  value = gazpar_config.power_factor;
  WSContentSend_P (PSTR ("<div class='gaz'><div class='gazh'></div><div class='gazt'>Coefficient</div><div class='gazv'>%d.%02d</div><div class='gazs'></div><div class='gazu'>kWh/m³</div></div>\n"), value / 100, value % 100);

  WSContentSend_P (PSTR ("</div>\n"));
}

#endif  // USE_WEBSERVER

/***********************************\
 *            Interface
\***********************************/

// Teleinfo sensor
bool Xdrv110 (uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function) 
  {
    case FUNC_PRE_INIT:
      GazparPreInit ();
      break;
    case FUNC_INIT:
      GazparInit ();
      break;
    case FUNC_EVERY_50_MSECOND:
      GazparEvery50ms ();
      break;
    case FUNC_EVERY_SECOND:
      GazparEverySecond ();
      break;
    case FUNC_SAVE_BEFORE_RESTART:
      GazparSaveConfig ();
      break;      
    case FUNC_COMMAND:
      result = DecodeCommand (kGazparCommands, GazparCommand);
      break;
    case FUNC_JSON_APPEND:
      GazparShowJSON (true);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_BUTTON:
      GazparWebConfigButton ();
      break;
    case FUNC_WEB_ADD_HANDLER:
      Webserver->on (F (D_GAZPAR_PAGE_CONFIG), GazparWebPageConfigure);
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
