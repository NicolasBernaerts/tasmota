/*
  xsns_123_teleinfo_winky.ino - Deep Sleep integration for Winky Teleinfo board

  Copyright (C) 2024  Nicolas Bernaerts

  Version history :
    16/02/2024 - v1.0 - Creation, support for Winky C6
    25/08/2024 - v1.1 - Add Thingsboard integration
                        Support for Winky C3
    12/10/2024 - v1.2 - Force deepsleeptime to default if undefined
                        Add meter current supply calculation
                        add super capa capacitance calculation
    22/03/2025 - v1.3 - add max received message before sleep

  Configuration values are stored in :
    - Settings->knx_GA_addr[0..2] : multiplicator
    - Settings->knx_GA_addr[3]    : minimum start voltage (mV)
    - Settings->knx_GA_addr[4]    : minimum stop voltage (mV)
    - Settings->knx_GA_addr[5]    : reference capacity (mF)
    - Settings->knx_GA_addr[6]    : meter average current (mA)
    - Settings->knx_GA_addr[7]    : meter max voltage (mV)
    - Settings->knx_GA_addr[8]    : Maximum number of message before sleep (0 no max.)
    - Settings->knx_GA_addr[9]    : nominal super capa voltage (mV)

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
#define XSNS_123                   123

/*****************************************\
 *               Constants
\*****************************************/

#define mS_TO_uS_FACTOR            1000ULL   // conversion factor from seconds to micro seconds

#define WINKY_SLEEP_MINIMUM        5000         // minimum acceptable sleep time (ms)
#define WINKY_SLEEP_DEFAULT        30000        // default deep sleep time (ms)
#define WINKY_SLEEP_MAXIMUM        120000       // maximum acceptable sleep time (ms)

#define WINKY_VOLTAGE_ADC_MAX      4095         // max counter for ADC input

#define WINKY_CAPA_REFERENCE       1500         // by default 1.5F 
#define WINKY_POWER_LINKY          130          // Linky average power (mW)
#define WINKY_CURRENT_ESP          60           // ESP32 average current (mA)

// voltage levels
#define WINKY_USB_MAXIMUM          5000         // USB max voltage
#define WINKY_USB_CHARGED          4400         // USB voltage correct
#define WINKY_USB_DISCHARGED       4000         // USB voltage disconnected
#define WINKY_USB_CRITICAL         3000         // USB voltage disconnected

#define WINKY_CAPA_MAXIMUM         5000         // Capa max voltage
#define WINKY_CAPA_TARGET          4250         // target capa voltage during deepsleep charge
#define WINKY_CAPA_CHARGED         4200         // minimum capa voltage to start cycle
#define WINKY_CAPA_DISCHARGED      3700         // minimum capa voltage to start sleep process
#define WINKY_CAPA_CRITICAL        3600         // minimum capa voltage to sleep immediatly

#define WINKY_LINKY_MAXIMUM        WINKY_CAPA_MAXIMUM * 57 / 20       // Linky max voltage
#define WINKY_LINKY_CHARGED        9000         // minimum linky voltage to consider as connected
#define WINKY_LINKY_DISCHARGED     7000         // minimum linky voltage to consider as low
#define WINKY_LINKY_CRITICAL       5000         // minimum linky voltage to consider as very low

/***************************************\
 *               Variables
\***************************************/

// Commands
static const char kCapaCommands[]  PROGMEM =      "winky|"        "|_ref"       "|_start"        "|_stop"        "|_coeff"        "|_meter"         "|_sleep"       "|_max";
void (* const CapaCommand[])(void) PROGMEM = { &CmndWinkyHelp, &CmndWinkyRef, &CmndWinkyStart, &CmndWinkyStop, &CmndWinkyCoeff, &CmndWinkyMeter, &CmndWinkySleep, &CmndWinkyMax };

// timings (ms)
//  - WINKY_TIME_BOOT : timestamp at boot time
//  - WINKY_TIME_NET  : timestamp at network connexion time
//  - WINKY_TIME_RTC  : timestamp at RTC connexion time
//  - WINKY_TIME_MQTT : timestamp at MQTT connexion time
//  - WINKY_TIME_CAPA : timestamp of capacitor measure start
enum TeleinfoWinkyTime  { WINKY_TIME_BOOT, WINKY_TIME_NET, WINKY_TIME_RTC, WINKY_TIME_MQTT, WINKY_TIME_CAPA, WINKY_TIME_MAX };

// capacitor voltage (mV)
//  - WINKY_VCAP_START   : minimal capa voltage to start
//  - WINKY_VCAP_STOP    : minimal capa voltage before switching off
//  - WINKY_VCAP_BOOT    : capa voltage at start
//  - WINKY_VCAP_MEASURE : capa voltage at beginning of capacitor measure
enum TeleinfoWinkyVolt  { WINKY_VCAP_START, WINKY_VCAP_STOP, WINKY_VCAP_BOOT, WINKY_VCAP_MEASURE, WINKY_VCAP_MAX };

// voltage levels
enum  TeleinfoWinkyLevel                    { WINKY_LEVEL_CRITICAL, WINKY_LEVEL_DISCHARGED, WINKY_LEVEL_CORRECT, WINKY_LEVEL_CHARGED, WINKY_LEVEL_MAX };  // voltage levels
const char kTeleinfoWinkyColor[] PROGMEM    =       "red"        "|"     "orange"        "|"     "yellow"     "|"     "white";                            // level display color

/***************************************\
 *                  Data
\***************************************/

struct winky_source {
  uint32_t raw;                 // raw value
  uint32_t ref;                 // voltage reference adjustment
  uint32_t volt;                // current voltage (mV)
};

static struct {
  long     max_message   = 0;                     // maximum of messsage before going to deepsleep
  uint8_t  enabled       = 0;                     // flag set if winky configured
  uint8_t  suspend       = 0;                     // flag set if suspending soon
  uint8_t  nb_source     = 0;                     // number of voltage sources
  uint32_t deepsleep     = 0;                     // calculated deepsleep time (ms)
  uint32_t capa_ref      = 0;                     // reference capacitor value (mF)
  uint32_t capa_calc     = 0;                     // calculated capacitor value (mF)
  uint32_t meter_volt    = 0;                     // meter max voltage
  uint32_t meter_current = 0;                     // meter estimated current
  uint32_t arr_time[WINKY_TIME_MAX];              // array of timestamps
  uint32_t arr_vcap[WINKY_VCAP_MAX];              // array of capacitor voltage
  winky_source usb;
  winky_source capa;
  winky_source linky;
} teleinfo_winky;

/************************\
 *        Commands
\************************/

// Capacitor management help
void CmndWinkyHelp ()
{
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: gestion du winky"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - winky_sleep         = duree du deepsleep en sec. [%u] (0 = calculee)"), Settings->deepsleep);
  AddLog (LOG_LEVEL_INFO, PSTR (" - winky_max           = nbre de messages recus avant deepsleep [%u]"), teleinfo_winky.max_message);
  AddLog (LOG_LEVEL_INFO, PSTR (" - winky_ref <farad>   = valeur de la super capa en F [%u.%u]"), teleinfo_winky.capa_ref / 1000, teleinfo_winky.capa_ref % 1000 / 100);
  AddLog (LOG_LEVEL_INFO, PSTR (" - winky_start <volt>  = tension min. pour démarrer le winky [%u.%u]"), teleinfo_winky.arr_vcap[WINKY_VCAP_START] / 1000, teleinfo_winky.arr_vcap[WINKY_VCAP_START] % 1000 / 100);
  AddLog (LOG_LEVEL_INFO, PSTR (" - winky_stop <volt>   = tension min. pour l'arrêt du winky [%u.%u]"), teleinfo_winky.arr_vcap[WINKY_VCAP_STOP] / 1000, teleinfo_winky.arr_vcap[WINKY_VCAP_STOP] % 1000 / 100);
  AddLog (LOG_LEVEL_INFO, PSTR (" - winky_coeff         = raz des coefficients d'ajustement de tensions"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - winky_meter         = raz des valeurs du linky"));
  ResponseCmndDone ();
}

// Set deepsleep time in seconds (0 : auto)
void CmndWinkySleep ()
{
  // set limits
  if (XdrvMailbox.payload <= 0) Settings->deepsleep = 0;
  else Settings->deepsleep = (uint32_t)XdrvMailbox.payload;
  
  // save settings
  SettingsSave (0);

  // display value
  ResponseCmndNumber (Settings->deepsleep);
}

// Set maximum number of messages before deepsleep
void CmndWinkyMax ()
{
  // set value
  teleinfo_winky.max_message = (long)XdrvMailbox.payload;
  Settings->knx_GA_addr[8]   = (uint16_t)XdrvMailbox.payload;
  SettingsSave (0);
  
  // display value
  ResponseCmndNumber (teleinfo_winky.max_message);
}

// Set reference of super capa
void CmndWinkyRef ()
{
  uint32_t capa_ref;
  char     str_result[8];

  // if voltage is set
  if (XdrvMailbox.data_len > 0)
  {
    // if new start voltage greater than stop voltage, set and save
    capa_ref = (uint32_t)(atof (XdrvMailbox.data) * 1000);
    if (capa_ref > 0)
    {
      teleinfo_winky.capa_ref  = capa_ref;
      Settings->knx_GA_addr[5] = (uint16_t)capa_ref;
      SettingsSave (0);
    }
  }
  
  // display value
  sprintf_P (str_result, PSTR ("%u.%u Farad"), teleinfo_winky.capa_ref / 1000, (teleinfo_winky.capa_ref % 1000) / 10);
  ResponseCmndChar (str_result);
}

// Set minimal super capacitor voltage to start Winky
void CmndWinkyStart ()
{
  uint32_t voltage;
  char     str_result[8];

  // if voltage is set
  if (XdrvMailbox.data_len > 0)
  {
    // if new start voltage greater than stop voltage, set and save
    voltage = (uint32_t)(atof (XdrvMailbox.data) * 1000);
    if (voltage > teleinfo_winky.arr_vcap[WINKY_VCAP_STOP])
    {
      teleinfo_winky.arr_vcap[WINKY_VCAP_START] = voltage;
      Settings->knx_GA_addr[3] = (uint16_t)voltage;
      SettingsSave (0);
    }
  }
  
  // display value
  sprintf_P (str_result, PSTR ("%u.%u Volt"), teleinfo_winky.arr_vcap[WINKY_VCAP_START] / 1000, (teleinfo_winky.arr_vcap[WINKY_VCAP_START] % 1000) / 10);
  ResponseCmndChar (str_result);
}

// Set minimal super capacitor voltage to stop Winky
void CmndWinkyStop ()
{
  uint32_t voltage;
  char     str_result[8];

  // if voltage is set
  if (XdrvMailbox.data_len > 0)
  {
    // if new stop voltage lower than start voltage, set and save
    voltage = (uint32_t)(atof (XdrvMailbox.data) * 1000);
    if (voltage < teleinfo_winky.arr_vcap[WINKY_VCAP_START])
    {
      teleinfo_winky.arr_vcap[WINKY_VCAP_STOP] = voltage;
      Settings->knx_GA_addr[4] = (uint16_t)voltage;
      SettingsSave (0);
    }
  }
  
  // display value
  sprintf_P (str_result, PSTR ("%u.%u Volt"), teleinfo_winky.arr_vcap[WINKY_VCAP_STOP] / 1000, (teleinfo_winky.arr_vcap[WINKY_VCAP_STOP] % 1000) / 10);
  ResponseCmndChar (str_result);
}

// Reset winky ajustemnt coefficient
void CmndWinkyCoeff ()
{
  teleinfo_winky.usb.ref   = (uint16_t)(WINKY_VOLTAGE_ADC_MAX / 2);
  teleinfo_winky.capa.ref  = (uint16_t)(WINKY_VOLTAGE_ADC_MAX / 2);
  teleinfo_winky.linky.ref = (uint16_t)(WINKY_VOLTAGE_ADC_MAX / 2);
  ResponseCmndDone ();
}

// Reset winky ajustemnt coefficient
void CmndWinkyMeter ()
{
  uint8_t index;
  
  // reset meter values and save
  teleinfo_winky.meter_volt    = 0;
  teleinfo_winky.meter_current = 0;
  SettingsSave (0);
  
  ResponseCmndDone ();
}

/************************************\
 *           Functions
\************************************/

// save multipliers to settings
void TeleinfoWinkyLoadConfiguration () 
{
  uint8_t index;
  
  // load reference maximum voltage
  teleinfo_winky.usb.ref   = max (Settings->knx_GA_addr[0], (uint16_t)(WINKY_VOLTAGE_ADC_MAX / 2));
  teleinfo_winky.capa.ref  = max (Settings->knx_GA_addr[1], (uint16_t)(WINKY_VOLTAGE_ADC_MAX / 2));
  teleinfo_winky.linky.ref = max (Settings->knx_GA_addr[2], (uint16_t)(WINKY_VOLTAGE_ADC_MAX / 2));

  // load super capacitor start voltage 
  teleinfo_winky.arr_vcap[WINKY_VCAP_START] = (uint32_t)Settings->knx_GA_addr[3];
  if (teleinfo_winky.arr_vcap[WINKY_VCAP_START] == 0) teleinfo_winky.arr_vcap[WINKY_VCAP_START] = WINKY_CAPA_CHARGED;

  // load super capacitor stop voltage 
  teleinfo_winky.arr_vcap[WINKY_VCAP_STOP] = (uint32_t)Settings->knx_GA_addr[4];
  if (teleinfo_winky.arr_vcap[WINKY_VCAP_STOP] == 0) teleinfo_winky.arr_vcap[WINKY_VCAP_STOP] = WINKY_CAPA_DISCHARGED;

  // load super capacitor reference
  teleinfo_winky.capa_ref = (uint32_t)Settings->knx_GA_addr[5];
  if (teleinfo_winky.capa_ref == 0) teleinfo_winky.capa_ref = WINKY_CAPA_REFERENCE;

  // load meter data
  teleinfo_winky.meter_current = (uint32_t)Settings->knx_GA_addr[6];
  teleinfo_winky.meter_volt    = (uint32_t)Settings->knx_GA_addr[7];
  teleinfo_winky.max_message   = (long)Settings->knx_GA_addr[8];
}

// save multipliers to settings
void TeleinfoWinkySaveConfiguration () 
{
  // save reference maximum voltage
  Settings->knx_GA_addr[0] = teleinfo_winky.usb.ref;
  Settings->knx_GA_addr[1] = teleinfo_winky.capa.ref;
  Settings->knx_GA_addr[2] = teleinfo_winky.linky.ref;
}

// update USB voltage : get raw value, calculate max raw for 5V and calculate voltage
void TeleinfoWinkyReadVoltageUSB ()
{
  if (Adcs.present >= 1)
  {
    teleinfo_winky.usb.raw = (uint32_t)AdcRead (Adc[0].pin, 3);
    teleinfo_winky.usb.ref = max (teleinfo_winky.usb.ref, teleinfo_winky.usb.raw);
    teleinfo_winky.usb.volt = WINKY_USB_MAXIMUM * teleinfo_winky.usb.raw / teleinfo_winky.usb.ref;
  }

  // if no usb, declare tasmota on battery
  if (teleinfo_winky.usb.volt < WINKY_USB_CHARGED) teleinfo_config.battery = 1;
    else teleinfo_config.battery = 0;
}

// update CAPA voltage : get raw value, calculate max raw for 5V and calculate voltage
void TeleinfoWinkyReadVoltageCapa ()
{
  if (Adcs.present >= 2)
  {
    teleinfo_winky.capa.raw = (uint32_t)AdcRead (Adc[1].pin, 3);
    teleinfo_winky.capa.ref  = max (teleinfo_winky.capa.ref, teleinfo_winky.capa.raw);
    teleinfo_winky.capa.volt = WINKY_CAPA_MAXIMUM * teleinfo_winky.capa.raw / teleinfo_winky.capa.ref;
  }
}

// update LINKY voltage : get raw value and calculate voltage based on dividers
void TeleinfoWinkyReadVoltageLinky ()
{
  if (Adcs.present >= 3)
  {
    teleinfo_winky.linky.raw = (uint32_t)AdcRead (Adc[2].pin, 5);
    teleinfo_winky.linky.volt = WINKY_LINKY_MAXIMUM * teleinfo_winky.linky.raw / teleinfo_winky.capa.ref;
  }
}

uint32_t TeleinfoWinkyCalculateSleepTime ()
{
  float    delay, volt_now, volt_target, capa_ref, power_linky;
  uint32_t result;

  // collect data
  volt_now = (float)(teleinfo_winky.capa.volt) / 1000;
  volt_target = (float)(teleinfo_winky.arr_vcap[WINKY_VCAP_START]) / 1000 + 0.05;
  capa_ref = (float)(teleinfo_winky.capa_ref) / 1000;
  power_linky = (float)(WINKY_POWER_LINKY) / 1000;

  // calculate sleep time
  delay = 0;
  if (volt_now < volt_target) delay = 1000 * (volt_target * volt_target - volt_now * volt_now) * capa_ref / (2 * power_linky);
  if (delay < WINKY_SLEEP_MINIMUM) delay = WINKY_SLEEP_MINIMUM;

AddLog (LOG_LEVEL_INFO, PSTR ("WIN: volt_now %u, volt_target %u, capa_ref %u, delay %u"), teleinfo_winky.capa.volt, teleinfo_winky.arr_vcap[WINKY_VCAP_START], teleinfo_winky.capa_ref), (uint32_t)delay;

  return (uint32_t)delay;
}

// Enter deep sleep mode
void TeleinfoWinkyEnterSleepMode (const uint32_t delay)
{
  uint64_t sleeptime_us;

  // convert sleeptime in micro seconds
  if (delay < WINKY_SLEEP_MINIMUM) sleeptime_us = (uint64_t)WINKY_SLEEP_MINIMUM * mS_TO_uS_FACTOR;
    else if (delay > WINKY_SLEEP_MAXIMUM) sleeptime_us = (uint64_t)WINKY_SLEEP_MAXIMUM * mS_TO_uS_FACTOR;
    else sleeptime_us = (uint64_t)delay * mS_TO_uS_FACTOR;

  // switch off wifi and enter deepsleep
  WifiShutdown ();
  esp_sleep_enable_timer_wakeup (sleeptime_us);
  esp_deep_sleep_start ();
}

// estimate meter current (based on ESP average current drawn)
void TeleinfoWinkyEstimateCurrent ()
{
  uint32_t current, delay_ms, delta_mv;

  // check environment
  if (teleinfo_winky.arr_time[WINKY_TIME_CAPA] == UINT32_MAX) return;
  if (teleinfo_winky.arr_vcap[WINKY_VCAP_MEASURE] == 0) return;

  // if no time difference, ignore
  delay_ms = TimeDifference (teleinfo_winky.arr_time[WINKY_TIME_CAPA], millis());
  if (delay_ms < 1000) return;

  // if no voltage difference, ignore
  delta_mv = teleinfo_winky.arr_vcap[WINKY_VCAP_MEASURE] - teleinfo_winky.capa.volt;
  if (delta_mv < 250) return;

  // calculate meter current according to capacitor linear discharge
  current = WINKY_CURRENT_ESP - (teleinfo_winky.capa_ref * delta_mv / delay_ms);

  // average value on 4 samples and update meter current
  if (teleinfo_winky.meter_current > 0) current = (3 * teleinfo_winky.meter_current + current) / 4;
  teleinfo_winky.meter_current = current;
}

// estimate winky current super cap capacitance
void TeleinfoWinkyEstimateCapacity ()
{
  uint32_t capacity, delay_ms, delta_mv;

  // check environment
  if (teleinfo_winky.meter_current == 0) return;
  if (teleinfo_winky.arr_time[WINKY_TIME_CAPA] == UINT32_MAX) return;
  if (teleinfo_winky.arr_vcap[WINKY_VCAP_MEASURE] == 0) return;

  // if no time difference, ignore
  delay_ms = TimeDifference (teleinfo_winky.arr_time[WINKY_TIME_CAPA], millis());
  if (delay_ms < 1000) return;

  // if no voltage difference, ignore
  delta_mv = teleinfo_winky.arr_vcap[WINKY_VCAP_MEASURE] - teleinfo_winky.capa.volt;
  if (delta_mv < 250) return;

  // calculate capacitor value with linear discharge current estimated (to ESP - from Meter)
  capacity = (WINKY_CURRENT_ESP - teleinfo_winky.meter_current) * delay_ms / delta_mv;

  // average value on 4 samples
  if (teleinfo_winky.capa_calc == 0) teleinfo_winky.capa_calc = capacity;
    else teleinfo_winky.capa_calc = (3 * teleinfo_winky.capa_calc + capacity) / 4;
}

/************************************\
 *               MQTT
\************************************/

// Generate WINKY section
void TeleinfoWinkyPublish ()
{
  bool        ipv4, ipv6;
  int         length = 0;
  char        last = 0;
  const char *pstr_response;
  uint32_t    delay_net, delay_rtc, delay_mqtt, delay_awake;
  IPAddress   ip_addr;

  // if called before suspend phase, ignore
  if (!teleinfo_winky.suspend) return;

  // calculate delays
  if (teleinfo_winky.arr_time[WINKY_TIME_NET] == UINT32_MAX) delay_net = 0;
    else delay_net = teleinfo_winky.arr_time[WINKY_TIME_NET]  - teleinfo_winky.arr_time[WINKY_TIME_BOOT];
  if (teleinfo_winky.arr_time[WINKY_TIME_RTC] == UINT32_MAX) delay_rtc = 0;
    else delay_rtc = teleinfo_winky.arr_time[WINKY_TIME_RTC]  - teleinfo_winky.arr_time[WINKY_TIME_BOOT];
  if (teleinfo_winky.arr_time[WINKY_TIME_MQTT] == UINT32_MAX) delay_mqtt = 0;
    else delay_mqtt = teleinfo_winky.arr_time[WINKY_TIME_MQTT] - teleinfo_winky.arr_time[WINKY_TIME_BOOT];
  delay_awake = millis () - teleinfo_winky.arr_time[WINKY_TIME_BOOT];

  // check need of ',' according to previous data
  pstr_response = ResponseData ();
  if (pstr_response != nullptr) length = strlen (pstr_response) - 1;
  if (length >= 0) last = pstr_response[length];
  if ((last != 0) && (last != '{') && (last != ',')) ResponseAppend_P (PSTR (","));

  // detect IP configuration
  ipv4 = (static_cast<uint32_t>(WiFi.localIP ()) != 0);
  ipv6 = WifiGetIPv6 (&ip_addr);

  // message
  ResponseAppend_P (PSTR ("\"WINKY\":{"));
  if (ipv4) ResponseAppend_P (PSTR ("\"mac\":\"%s\",\"ipv4\":\"%_I\","), WiFiHelper::macAddress ().c_str (), (uint32_t)WiFi.localIP ());
  if (ipv6) ResponseAppend_P (PSTR ("\"ipv6\":\"%s\","), ip_addr.toString (true).c_str ());
  ResponseAppend_P (PSTR ("\"param\":{\"Vstart\":%u.%02u,\"Vstop\":%u.%02u}"), teleinfo_winky.arr_vcap[WINKY_VCAP_START] / 1000, teleinfo_winky.arr_vcap[WINKY_VCAP_START] % 1000 / 10, teleinfo_winky.arr_vcap[WINKY_VCAP_STOP] / 1000, teleinfo_winky.arr_vcap[WINKY_VCAP_STOP] % 1000 / 10);
  ResponseAppend_P (PSTR (",\"count\":{\"msg\":%u,\"cos\":%d,\"boot\":%d,\"write\":%d}"), teleinfo_meter.nb_message, teleinfo_conso.cosphi.quantity + teleinfo_prod.cosphi.quantity, Settings->bootcount, Settings->save_flag);
  ResponseAppend_P (PSTR (",\"capa\":{\"Vmax\":%u.%02u,\"Vmin\":%u.%02u,\"F\":%u.%02u}"), teleinfo_winky.arr_vcap[WINKY_VCAP_BOOT] / 1000, teleinfo_winky.arr_vcap[WINKY_VCAP_BOOT] % 1000 / 10, teleinfo_winky.capa.volt / 1000, teleinfo_winky.capa.volt % 1000 / 10, teleinfo_winky.capa_calc / 1000, teleinfo_winky.capa_calc % 1000 / 10);
  ResponseAppend_P (PSTR (",\"meter\":{\"V\":%u.%02u,\"mA\":%u}"), teleinfo_winky.meter_volt / 1000, teleinfo_winky.meter_volt % 1000 / 10, teleinfo_winky.meter_current);
  ResponseAppend_P (PSTR (",\"time\":{\"net\":%u,\"rtc\":%u,\"mqtt\":%u,\"awake\":%u,\"sleep\":%u}}"), delay_net, delay_rtc, delay_mqtt, delay_awake, teleinfo_winky.deepsleep);
}

/************************************\
 *             Callback
\************************************/

// Domoticz init message
void TeleinfoWinkyInit ()
{
  uint8_t index;
  esp_sleep_wakeup_cause_t wakeup_reason;

  // check if winky configured
  if (Adcs.present >= 2) teleinfo_winky.enabled = 1;

  // if winky configured and reboot not cause by deep sleep wake-up, back to deep sleep for 30 sec
//  if (teleinfo_winky.enabled && (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED)) TeleinfoWinkyEnterSleepMode (30);

  // init timestamps
  for (index = 0; index < WINKY_TIME_MAX; index ++) teleinfo_winky.arr_time[index] = UINT32_MAX;

  // init capacitor voltages
  for (index = 0; index < WINKY_VCAP_MAX; index ++) teleinfo_winky.arr_vcap[index] = 0;

  // load configuration
  TeleinfoWinkyLoadConfiguration ();

  // read voltage
  TeleinfoWinkyReadVoltageUSB ();
  TeleinfoWinkyReadVoltageCapa ();

  // if running on capa and voltage too low to start, back to sleep mode for 60 sec
  if (teleinfo_winky.enabled && teleinfo_config.battery && (teleinfo_winky.capa.volt < teleinfo_winky.arr_vcap[WINKY_VCAP_START])) TeleinfoWinkyEnterSleepMode (WINKY_SLEEP_DEFAULT);

  // get start datatime_t TeleinfoHistoGetLastWrite (const uint8_t period)
  teleinfo_winky.arr_time[WINKY_TIME_BOOT] = millis ();
  teleinfo_winky.arr_vcap[WINKY_VCAP_BOOT] = teleinfo_winky.capa.volt;

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Run capa to get help on super capacitor commands"));
}

// called 10 times per second to update start times
void TeleinfoWinkyEvery100ms ()
{
  // check for RTC connexion
  if ((teleinfo_winky.arr_time[WINKY_TIME_RTC] == UINT32_MAX) && RtcTime.valid) teleinfo_winky.arr_time[WINKY_TIME_RTC] = millis ();

  // check for network connectivity
  if (teleinfo_winky.arr_time[WINKY_TIME_NET] == UINT32_MAX)
  {
    if (WifiHasIPv4 ())      teleinfo_winky.arr_time[WINKY_TIME_NET] = millis ();
    else if (WifiHasIPv6 ()) teleinfo_winky.arr_time[WINKY_TIME_NET] = millis ();
  }

  // check for mqtt connectivity
  if ((teleinfo_winky.arr_time[WINKY_TIME_MQTT] == UINT32_MAX) && MqttIsConnected ()) teleinfo_winky.arr_time[WINKY_TIME_MQTT] = millis ();
}

// called 4 times per second to check capa level
void TeleinfoWinkyEvery250ms ()
{
  bool     voltage_low, cosphi_reached, max_reached;
  uint32_t sleeptime;

  // if not enabled, ignore
  if (!teleinfo_winky.enabled) return;

  //  voltage calculation
  //  -------------------

  TeleinfoWinkyReadVoltageUSB ();
  TeleinfoWinkyReadVoltageCapa ();
  TeleinfoWinkyReadVoltageLinky ();

  //  current and capacitance calculation
  //  -----------------------------------
  
  // if not on super capa, reset calculation
  if (!teleinfo_config.battery) teleinfo_winky.arr_time[WINKY_TIME_CAPA] = UINT32_MAX;

  // else if first time on battery, init capacitor calculation
  else if (teleinfo_winky.arr_time[WINKY_TIME_CAPA] == UINT32_MAX)
  {
    teleinfo_winky.arr_time[WINKY_TIME_CAPA]    = millis ();
    teleinfo_winky.arr_vcap[WINKY_VCAP_MEASURE] = teleinfo_winky.capa.volt;
  }

  // else calculate meter current and capacitance
  else
  {
    // estimate meter current
    TeleinfoWinkyEstimateCurrent ();

    // estimate capacity
    TeleinfoWinkyEstimateCapacity ();
  }

  //  battery mode
  //  ------------

  if (teleinfo_config.battery)
  {
    // check if capa voltage is too low
    voltage_low = (teleinfo_winky.capa.volt <= teleinfo_winky.arr_vcap[WINKY_VCAP_STOP]);

    // check cosphi calculation quantity
    cosphi_reached = ((teleinfo_conso.cosphi.quantity >= TIC_COSPHI_SAMPLE) || (teleinfo_prod.cosphi.quantity >= TIC_COSPHI_SAMPLE));

    // check if maximum message reception is reached
    if (teleinfo_winky.max_message == 0) max_reached = false;
      else max_reached = (teleinfo_meter.nb_message >= teleinfo_winky.max_message);

    // if voltage is low, uptime to long, cosphi ratio reached or max received message reached, start suspend
    if (voltage_low || cosphi_reached || max_reached)
    {
      // start suspend phase
      teleinfo_winky.suspend = 1;

      // if needed, publish raw TIC
      if (teleinfo_meter.json.tic) TeleinfoDriverPublishTic (true);

      // if needed, calculate deepsleep time
      if (Settings->deepsleep == 0)
      {
        TeleinfoWinkyReadVoltageCapa ();
        teleinfo_winky.deepsleep = TeleinfoWinkyCalculateSleepTime ();
      }
      
      // else apply fixed deepsleep time
      else teleinfo_winky.deepsleep = 1000 * Settings->deepsleep;

#ifdef USE_TELEINFO_DOMOTICZ
      // publish Domoticz data
      DomoticzIntegrationPublishAll ();
#endif    // USE_TELEINFO_DOMOTICZ

#ifdef USE_TELEINFO_HOMIE
      // publish Homie data
      HomieIntegrationPublishAllData ();
#endif    // USE_TELEINFO_HOMIE

#ifdef USE_TELEINFO_THINGSBOARD
      // publish Thingsboard data
      ThingsboardIntegrationPublishData ();
#endif    // USE_TELEINFO_HOMIE

#ifdef USE_INFLUXDB
      // publish InfluxDB data
      InfluxDbExtensionPublishData ();
#endif    // USE_INFLUXDB

      // publish MQTT data
      TeleinfoDriverPublishAllData (false);

      // enter deepsleep
      TeleinfoWinkyEnterSleepMode (teleinfo_winky.deepsleep);
    }
  }
}

// called every second to check meter max volatge level
void TeleinfoWinkyEverySecond ()
{
  bool save = false;

  // if average current increased, save new average max
  if (teleinfo_winky.meter_current > (uint32_t)Settings->knx_GA_addr[6])
  {
    Settings->knx_GA_addr[6] = (uint16_t)teleinfo_winky.meter_current;
    save = true;
  }

  // if max voltage reached, save new max
  if (teleinfo_winky.meter_volt < teleinfo_winky.linky.volt)
  {
    teleinfo_winky.meter_volt = teleinfo_winky.linky.volt;
    Settings->knx_GA_addr[7]  = (uint16_t)teleinfo_winky.meter_volt;
    save = true;
   }

  // if needed, save config
  if (save) SettingsSave (0);
}

#ifdef USE_INFLUXDB

// called to append winky InfluxDB data
void TeleinfoWinkyApiInfluxDb ()
{
  uint32_t value;
  char     str_line[96];

  // if called before suspend phase, ignore
  if (!teleinfo_winky.suspend) return;

  // number of received messages
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("nb-msg"), teleinfo_meter.nb_message);
  TasmotaGlobal.mqtt_data += str_line;

  // number of calculated cosphi
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%d\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("nb-cos"), teleinfo_conso.cosphi.quantity + teleinfo_prod.cosphi.quantity);
  TasmotaGlobal.mqtt_data += str_line;

  // capacitor reference
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u.%02u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("farad-ref"), teleinfo_winky.capa_ref / 1000, teleinfo_winky.capa_ref % 1000 / 10);
  TasmotaGlobal.mqtt_data += str_line;

  // capacitor calculated
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u.%02u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("farad-calc"), teleinfo_winky.capa_calc / 1000, teleinfo_winky.capa_calc % 1000 / 10);
  TasmotaGlobal.mqtt_data += str_line;

  // meter voltage
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u.%02u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("meter-volt"), teleinfo_winky.meter_volt / 1000, teleinfo_winky.meter_volt % 1000 / 10);
  TasmotaGlobal.mqtt_data += str_line;

  // meter current
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("meter-ma"), teleinfo_winky.meter_current);
  TasmotaGlobal.mqtt_data += str_line;

  // voltage capacitor at start
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u.%02u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("volt-start"), teleinfo_winky.arr_vcap[WINKY_VCAP_BOOT] / 1000, teleinfo_winky.arr_vcap[WINKY_VCAP_BOOT] % 1000 / 10);
  TasmotaGlobal.mqtt_data += str_line;

  // voltage capacitor at stop
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u.%02u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("volt-stop"), teleinfo_winky.capa.volt / 1000, teleinfo_winky.capa.volt % 1000 / 10);
  TasmotaGlobal.mqtt_data += str_line;

  // time to establish network (ms)
  if (teleinfo_winky.arr_time[WINKY_TIME_NET] == UINT32_MAX) value = 0;
    else value = teleinfo_winky.arr_time[WINKY_TIME_NET] - teleinfo_winky.arr_time[WINKY_TIME_BOOT];
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("ms-net"), value);
  TasmotaGlobal.mqtt_data += str_line;

  // time to establish rtc (ms)
  if (teleinfo_winky.arr_time[WINKY_TIME_RTC] == UINT32_MAX) value = 0;
    else value = teleinfo_winky.arr_time[WINKY_TIME_RTC] - teleinfo_winky.arr_time[WINKY_TIME_BOOT];
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("ms-rtc"), value);
  TasmotaGlobal.mqtt_data += str_line;

  // time to establish mqtt (ms)
  if (teleinfo_winky.arr_time[WINKY_TIME_MQTT] == UINT32_MAX) value = 0;
    else value = teleinfo_winky.arr_time[WINKY_TIME_MQTT] - teleinfo_winky.arr_time[WINKY_TIME_BOOT];
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("ms-mqtt"), value);
  TasmotaGlobal.mqtt_data += str_line;

  // total alive time (ms)
  value = millis () - teleinfo_winky.arr_time[WINKY_TIME_BOOT];
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("ms-awake"), value);
  TasmotaGlobal.mqtt_data += str_line;

  // duration of next deepsleep (ms)
  value = teleinfo_winky.deepsleep;
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("ms-sleep"), value);
  TasmotaGlobal.mqtt_data += str_line;
}

#endif     // USE_INFLUXDB

#ifdef USE_WEBSERVER

// Append winky state to main page
void TeleinfoWinkyWebSensor ()
{
  uint8_t level;
  long    percent, factor, color;
  long    red, green;
  char    str_name[16];
  char    str_color[12];
  char    str_text[8];

  // if not configured, ignore
  if (!teleinfo_winky.enabled) return;

  // start
  WSContentSend_P (PSTR ("<div style='font-size:13px;text-align:center;padding:4px 6px;margin-bottom:5px;background:#333333;border-radius:12px;'>\n"));

  // style
  if (teleinfo_winky.usb.volt > WINKY_USB_CHARGED) strcpy_P (str_color, PSTR ("#0c0")); else strcpy_P (str_color, PSTR ("#e00")); 
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("div.bars {width:30%%;text-align:left;background-color:#252525;border-radius:6px;}\n"));
  WSContentSend_P (PSTR ("div.bare {width:42%%;}\n"));
  WSContentSend_P (PSTR ("div.usb {height:32px;background-color:%s;--svg:url(\"data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 640 512'><path d='M641.5 256c0 3.1-1.7 6.1-4.5 7.5L547.9 317c-1.4 .8-2.8 1.4-4.5 1.4-1.4 0-3.1-.3-4.5-1.1-2.8-1.7-4.5-4.5-4.5-7.8v-35.6H295.7c25.3 39.6 40.5 106.9 69.6 106.9H392V354c0-5 3.9-8.9 8.9-8.9H490c5 0 8.9 3.9 8.9 8.9v89.1c0 5-3.9 8.9-8.9 8.9h-89.1c-5 0-8.9-3.9-8.9-8.9v-26.7h-26.7c-75.4 0-81.1-142.5-124.7-142.5H140.3c-8.1 30.6-35.9 53.5-69 53.5C32 327.3 0 295.3 0 256s32-71.3 71.3-71.3c33.1 0 61 22.8 69 53.5 39.1 0 43.9 9.5 74.6-60.4C255 88.7 273 95.7 323.8 95.7c7.5-20.9 27-35.6 50.4-35.6 29.5 0 53.5 23.9 53.5 53.5s-23.9 53.5-53.5 53.5c-23.4 0-42.9-14.8-50.4-35.6H294c-29.1 0-44.3 67.4-69.6 106.9h310.1v-35.6c0-3.3 1.7-6.1 4.5-7.8 2.8-1.7 6.4-1.4 8.9 .3l89.1 53.5c2.8 1.1 4.5 4.1 4.5 7.2z'/></svg>\");}\n"), str_color);
  WSContentSend_P (PSTR ("</style>\n"));

  // title and USB icon
  WSContentSend_P (PSTR ("<div class='tic bar'>\n"));
  WSContentSend_P (PSTR ("<div style='width:46%%;text-align:left;font-weight:bold;'>Winky</div>\n"));
  WSContentSend_P (PSTR ("<div style='width:30%%;margin-top:-2px;' class='tz usb' title='Entrée %u/4095 (%u.%02u V)'></div>\n"), teleinfo_winky.usb.raw, teleinfo_winky.usb.volt / 1000, teleinfo_winky.usb.volt % 1000 / 10);

  // calculated capacity
  if (teleinfo_winky.arr_time[WINKY_TIME_CAPA] == UINT32_MAX) str_text[0] = 0;
    else sprintf_P (str_text, PSTR ("%u.%02u"), teleinfo_winky.capa_calc / 1000, teleinfo_winky.capa_calc % 1000 / 10);
  WSContentSend_P (PSTR ("<div style='width:12%%;'>%s</div>\n"), str_text);
  if (teleinfo_winky.arr_time[WINKY_TIME_CAPA] == UINT32_MAX) str_text[0] = 0;
    else strcpy_P (str_text, PSTR ("F"));
  WSContentSend_P (PSTR ("<div class='tics'></div><div class='ticu'>%s</div>\n"), str_text);
  WSContentSend_P (PSTR ("</div>\n"));

  // capa status bar
  // ---------------
  if (Adcs.present >= 2)
  {
    // get value
    sprintf_P (str_text, PSTR ("%u.%02u"), teleinfo_winky.capa.volt / 1000, teleinfo_winky.capa.volt % 1000 / 10);

    // calculate percentage
    if (teleinfo_winky.capa.ref > 0) percent = min (100L, 100 * (long)teleinfo_winky.capa.raw / (long)teleinfo_winky.capa.ref); 
      else percent = 0;

    // calculate color
    color = min (100L, 100 * (long)teleinfo_winky.capa.volt / WINKY_CAPA_CHARGED);
    if (color > 50) green = TIC_RGB_GREEN_MAX; else green = TIC_RGB_GREEN_MAX * 2 * color / 100;
    if (color < 50) red   = TIC_RGB_RED_MAX;   else red   = TIC_RGB_RED_MAX * 2 * (100 - color) / 100;
    sprintf_P (str_color, PSTR ("#%02x%02x%02x"), red, green, 0);

    // display bar graph percentage
    WSContentSend_P (PSTR ("<div class='tic bar'>\n"));
    WSContentSend_P (PSTR ("<div class='tich'>%s</div>\n"), PSTR ("Capa"));
    WSContentSend_P (PSTR ("<div class='bars'>"));
    WSContentSend_P (PSTR ("<div class='barv' style='width:%u%%;background-color:%s;' title='Entrée %u/4095'>%s</div></div>\n"), percent, str_color, teleinfo_winky.capa.raw, str_text);
    WSContentSend_P (PSTR ("<div class='bare'></div>"));
    WSContentSend_P (PSTR ("<div class='tics'></div><div class='ticu'>V</div>\n"));
    WSContentSend_P (PSTR ("</div>\n"));
  }

  // Linky status bar
  // ---------------

  if (Adcs.present >= 3)
  {
    // get value
    sprintf_P (str_text, PSTR ("%u.%02u"), teleinfo_winky.linky.volt / 1000, teleinfo_winky.linky.volt % 1000 / 10);

    // calculate percentage
    if (teleinfo_winky.capa.ref > 0) percent = min (100L, 119 * (long)teleinfo_winky.linky.raw / (long)teleinfo_winky.capa.ref); 
      else percent = 0;

    // calculate color
    strcpy_P (str_color, PSTR (COLOR_BUTTON));

    // display bar graph percentage
    WSContentSend_P (PSTR ("<div class='tic bar'>\n"));
    WSContentSend_P (PSTR ("<div class='tich'>%s</div>\n"), PSTR ("Linky"));
    WSContentSend_P (PSTR ("<div class='barm'>"));
    WSContentSend_P (PSTR ("<div class='barv' style='width:%u%%;background-color:%s;' title='Entrée %u/4095'>%s</div></div>\n"), percent, str_color, teleinfo_winky.linky.raw, str_text);
    WSContentSend_P (PSTR ("<div class='tics'></div><div class='ticu'>V</div>\n"));
    WSContentSend_P (PSTR ("</div>\n"));
  }

  // end
  WSContentSend_P (PSTR ("</div>\n"));
}

#endif  // USE_WEBSERVER

/***************************************\
 *              Interface
\***************************************/

// Teleinfo sensor (for graph)
bool Xsns123 (uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function) 
  {
    case FUNC_INIT:
      TeleinfoWinkyInit ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kCapaCommands, CapaCommand);
      break;
    case FUNC_EVERY_100_MSECOND:
      TeleinfoWinkyEvery100ms ();
      break;
    case FUNC_EVERY_250_MSECOND:
      TeleinfoWinkyEvery250ms ();
      break;
    case FUNC_EVERY_SECOND:
      TeleinfoWinkyEverySecond ();
      break;
    case FUNC_SAVE_BEFORE_RESTART:
      TeleinfoWinkySaveConfiguration ();
      break;

#ifdef USE_INFLUXDB
   case FUNC_API_INFLUXDB:
      TeleinfoWinkyApiInfluxDb ();
      break;
#endif    // USE_INFLUXDB

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
