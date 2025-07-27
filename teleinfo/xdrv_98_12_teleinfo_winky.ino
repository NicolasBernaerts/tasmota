/*
  xdrv_98_12_teleinfo_winky.ino - Teleinfo sensor for Winky boards to handle voltages and deep sleep

  Copyright (C) 2024-2025  Nicolas Bernaerts

  Version history :
    16/02/2024 v1.0 - Creation, support for Winky C6
    25/08/2024 v1.1 - Add Thingsboard integration
                        Support for Winky C3
    12/10/2024 v1.2 - Force deepsleeptime to default if undefined
                        Add meter current supply calculation
                        add super capa capacitance calculation
    22/03/2025 v1.3 - add max received message before sleep
    10/06/2025 v2.0 - Complete rework of deepsleep management
    10/07/2025 v3.0 - Refactoring based on Tasmota 15
                      Conversion from sensor to driver

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
#ifdef USE_TELEINFO_WINKY

/*****************************************\
 *               Constants
\*****************************************/

#define mS_TO_uS_FACTOR            1000ULL                            // conversion factor from seconds to micro seconds

#define WINKY_SLEEP_MINIMUM        10000                              // minimum acceptable sleep time (ms)
#define WINKY_SLEEP_DEFAULT        30000                              // default deep sleep time (ms)
#define WINKY_SLEEP_MAXIMUM        60000                              // maximum acceptable sleep time (ms)

#define WINKY_VOLTAGE_ADC_MAX      4095                               // max counter for ADC input

#define WINKY_CAPA_REFERENCE       1500                               // by default 1.5F 
#define WINKY_POWER_LINKY          130                                // Linky average power (mW)
#define WINKY_CURRENT_ESP          60                                 // ESP32 average current (mA)

// voltage levels
#define WINKY_USB_MAXIMUM          5000                               // USB max voltage
#define WINKY_USB_CHARGED          4400                               // USB voltage correct
#define WINKY_USB_DISCHARGED       4000                               // USB voltage disconnected
#define WINKY_USB_CRITICAL         3000                               // USB voltage disconnected

#define WINKY_CAPA_MAXIMUM         5000                               // Capa max voltage
#define WINKY_CAPA_TARGET          4250                               // target capa voltage during deepsleep charge
#define WINKY_CAPA_CHARGED         4200                               // minimum capa voltage to start cycle
#define WINKY_CAPA_DISCHARGED      3700                               // minimum capa voltage to start sleep process
#define WINKY_CAPA_CRITICAL        3600                               // minimum capa voltage to sleep immediatly

#define WINKY_LINKY_MAXIMUM        WINKY_CAPA_MAXIMUM * 57 / 20       // Linky max voltage
#define WINKY_LINKY_CHARGED        9000                               // minimum linky voltage to consider as connected
#define WINKY_LINKY_DISCHARGED     7000                               // minimum linky voltage to consider as low
#define WINKY_LINKY_CRITICAL       5000                               // minimum linky voltage to consider as very low

/***************************************\
 *               Variables
\***************************************/

// Commands
static const char kTeleinfoWinkyCommands[]  PROGMEM =          "winky|"      "|"       "_ref"       "|"       "_start"       "|"       "_stop"       "|"        "_coeff"      "|"        "_meter"      "|"        "_sleep"      "|"      "_max";
void (* const TeleinfoWinkyCommand[])(void) PROGMEM = { &CmndTeleinfoWinkyHelp, &CmndTeleinfoWinkyRef, &CmndTeleinfoWinkyStart, &CmndTeleinfoWinkyStop, &CmndTeleinfoWinkyCoeff, &CmndTeleinfoWinkyMeter, &CmndTeleinfoWinkySleep, &CmndTeleinfoWinkyMax };

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
  uint8_t  pin;
  uint32_t raw;                     // raw value
  uint32_t ref;                     // raw max reference
  uint32_t volt;                    // voltage (mV)
};

struct winky_time {         // ms
  uint32_t wifi;                    // wifi connexion timestamp
  uint32_t rtc;                     // RTC synchro timestamp
  uint32_t mqtt;                    // MQTT connexion timestamp
  uint32_t capa;                    // Capa discharge timestamp
};

struct winky_volt {         // mV
  uint32_t start;                   // minimal capa voltage to start
  uint32_t stop;                    // minimal capa voltage before switching off
  uint32_t boot;                    // capa voltage at boot
  uint32_t measure;                 // capa voltage at beginning of discharge
};

struct winky_farad {        // mF
  uint32_t ref;                     // reference capacitor value
};

struct winky_meter {
  long     max_msg;                 // max number of messsage before going to deepsleep
  uint32_t volt;                    // meter max voltage (mV)
};

struct {
  uint8_t      enabled   = 0;       // flag set if winky configured
  uint8_t      suspend   = 0;       // flag set if suspending soon
  uint32_t     deepsleep = 0;       // calculated deepsleep time (ms)
  winky_meter  meter;
  winky_farad  farad;
  winky_volt   volt;
  winky_time   timestamp;
  winky_source usb;
  winky_source capa;
  winky_source linky;
} teleinfo_winky;

struct winky_sleep {
  uint32_t delay_ms;                 // last deepsleep delay (ms)
  uint32_t volt_mv;                  // last deepsleep voltage (mV)
  uint32_t charge_mw;                // average charging power during deepsleep (mW)
};

RTC_DATA_ATTR winky_sleep teleinfo_sleep;

/************************\
 *        Commands
\************************/

// Capacitor management help
void CmndTeleinfoWinkyHelp ()
{
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: gestion du winky"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - winky_sleep        = deepsleep [%u.%02u]"), Settings->deepsleep / 1000, Settings->deepsleep % 1000 / 10);
  AddLog (LOG_LEVEL_INFO, PSTR ("                        0           : calcul base sur la tension"));
  AddLog (LOG_LEVEL_INFO, PSTR ("                        1.00 a 9.99 : multiplicateur duree eveil"));
  AddLog (LOG_LEVEL_INFO, PSTR ("                        10 ou +     : duree fixe (max %us)"), WINKY_SLEEP_MAXIMUM / 1000);
  AddLog (LOG_LEVEL_INFO, PSTR (" - winky_max          = nbre de trames avant deepsleep [%u]"), teleinfo_winky.meter.max_msg);
  AddLog (LOG_LEVEL_INFO, PSTR ("                        0       : attente tension basse"));
  AddLog (LOG_LEVEL_INFO, PSTR ("                        %u ou + : valeurs acceptables"), TIC_MESSAGE_MIN);
  AddLog (LOG_LEVEL_INFO, PSTR (" - winky_start <volt> = tension min. démarrage [%u.%u]"), teleinfo_winky.volt.start / 1000, teleinfo_winky.volt.start % 1000 / 100);
  AddLog (LOG_LEVEL_INFO, PSTR (" - winky_stop <volt>  = tension min. arrêt [%u.%u]"), teleinfo_winky.volt.stop / 1000, teleinfo_winky.volt.stop % 1000 / 100);
  AddLog (LOG_LEVEL_INFO, PSTR (" - winky_ref <farad>  = capacité super capa (F) [%u.%u]"), teleinfo_winky.farad.ref / 1000, teleinfo_winky.farad.ref % 1000 / 100);
  AddLog (LOG_LEVEL_INFO, PSTR (" - winky_coeff        = raz coefficients ajustement des tensions"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - winky_meter        = raz tension/courant linky"));
  ResponseCmndDone ();
}

// Set deepsleep time in seconds (0 : auto)
void CmndTeleinfoWinkySleep ()
{
  float value;
  char  str_result[8];
  
  // set limits
  if (strlen (XdrvMailbox.data))
  {
    value = atof (XdrvMailbox.data) * 1000;
    Settings->deepsleep = (uint32_t)value;
    if (Settings->deepsleep > WINKY_SLEEP_MAXIMUM) Settings->deepsleep = WINKY_SLEEP_MAXIMUM;
    SettingsSave (0);
  }

  // display value
  sprintf_P (str_result, PSTR ("%u.%02u"), Settings->deepsleep / 1000, Settings->deepsleep % 1000 / 10);
  ResponseCmndChar (str_result);
}

// Set maximum number of messages before deepsleep
void CmndTeleinfoWinkyMax ()
{
  // set value
  teleinfo_winky.meter.max_msg = (long)XdrvMailbox.payload;
  if ((teleinfo_winky.meter.max_msg > 0) && (teleinfo_winky.meter.max_msg < TIC_MESSAGE_MIN)) teleinfo_winky.meter.max_msg = TIC_MESSAGE_MIN;

  // save value
  Settings->knx_GA_addr[8] = (uint16_t)XdrvMailbox.payload;
  SettingsSave (0);
  
  // display value
  ResponseCmndNumber (teleinfo_winky.meter.max_msg);
}

// Set reference of super capa
void CmndTeleinfoWinkyRef ()
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
      teleinfo_winky.farad.ref = capa_ref;
      Settings->knx_GA_addr[5] = (uint16_t)capa_ref;
      SettingsSave (0);
    }
  }
  
  // display value
  sprintf_P (str_result, PSTR ("%u.%02u Farad"), teleinfo_winky.farad.ref / 1000, (teleinfo_winky.farad.ref % 1000) / 10);
  ResponseCmndChar (str_result);
}

// Set minimal super capacitor voltage to start Winky
void CmndTeleinfoWinkyStart ()
{
  uint32_t voltage;
  char     str_result[8];

  // if voltage is set
  if (XdrvMailbox.data_len > 0)
  {
    // if new start voltage greater than stop voltage, set and save
    voltage = (uint32_t)(atof (XdrvMailbox.data) * 1000);
    if (voltage > teleinfo_winky.volt.stop)
    {
      teleinfo_winky.volt.start = voltage;
      Settings->knx_GA_addr[3] = (uint16_t)voltage;
      SettingsSave (0);
    }
  }
  
  // display value
  sprintf_P (str_result, PSTR ("%u.%02u Volt"), teleinfo_winky.volt.start / 1000, (teleinfo_winky.volt.start % 1000) / 10);
  ResponseCmndChar (str_result);
}

// Set minimal super capacitor voltage to stop Winky
void CmndTeleinfoWinkyStop ()
{
  uint32_t voltage;
  char     str_result[8];

  // if voltage is set
  if (XdrvMailbox.data_len > 0)
  {
    // if new stop voltage lower than start voltage, set and save
    voltage = (uint32_t)(atof (XdrvMailbox.data) * 1000);
    if (voltage < teleinfo_winky.volt.start)
    {
      teleinfo_winky.volt.stop = voltage;
      Settings->knx_GA_addr[4] = (uint16_t)voltage;
      SettingsSave (0);
    }
  }
  
  // display value
  sprintf_P (str_result, PSTR ("%u.%02u Volt"), teleinfo_winky.volt.stop / 1000, (teleinfo_winky.volt.stop % 1000) / 10);
  ResponseCmndChar (str_result);
}

// Reset winky ajustemnt coefficient
void CmndTeleinfoWinkyCoeff ()
{
  teleinfo_winky.usb.ref   = (uint16_t)(WINKY_VOLTAGE_ADC_MAX / 2);
  teleinfo_winky.capa.ref  = (uint16_t)(WINKY_VOLTAGE_ADC_MAX / 2);
  teleinfo_winky.linky.ref = (uint16_t)(WINKY_VOLTAGE_ADC_MAX / 2);
  ResponseCmndDone ();
}

// Reset winky ajustemnt coefficient
void CmndTeleinfoWinkyMeter ()
{
  uint8_t index;
  
  // reset meter values and save
  teleinfo_winky.meter.volt    = 0;
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
  
  // load USB reference
  teleinfo_winky.usb.raw  = 0;
  teleinfo_winky.usb.volt = 0;
  teleinfo_winky.usb.ref  = max (Settings->knx_GA_addr[0], (uint16_t)(WINKY_VOLTAGE_ADC_MAX / 2));

  // load Capa reference
  teleinfo_winky.capa.raw  = 0;
  teleinfo_winky.capa.volt = 0;
  teleinfo_winky.capa.ref  = max (Settings->knx_GA_addr[1], (uint16_t)(WINKY_VOLTAGE_ADC_MAX / 2));

  // load Linky reference
  teleinfo_winky.linky.raw  = 0;
  teleinfo_winky.linky.volt = 0;
  teleinfo_winky.linky.ref  = max (Settings->knx_GA_addr[2], (uint16_t)(WINKY_VOLTAGE_ADC_MAX / 2));

  // load super capacitor voltage limits
  teleinfo_winky.volt.boot    = 0;
  teleinfo_winky.volt.measure = 0;
  teleinfo_winky.volt.start   = (uint32_t)Settings->knx_GA_addr[3];
  teleinfo_winky.volt.stop    = (uint32_t)Settings->knx_GA_addr[4];
  if (teleinfo_winky.volt.start == 0) teleinfo_winky.volt.start = WINKY_CAPA_CHARGED;
  if (teleinfo_winky.volt.stop  == 0) teleinfo_winky.volt.stop  = WINKY_CAPA_DISCHARGED;

  // load super capacitor reference
  teleinfo_winky.farad.ref  = (uint32_t)Settings->knx_GA_addr[5];
  if (teleinfo_winky.farad.ref == 0) teleinfo_winky.farad.ref = WINKY_CAPA_REFERENCE;

  // load meter data
  teleinfo_winky.meter.volt    = (uint32_t)Settings->knx_GA_addr[7];
  teleinfo_winky.meter.max_msg = (long)Settings->knx_GA_addr[8];
}

// save multipliers to settings
void TeleinfoWinkySaveBeforeRestart () 
{
  // save reference maximum voltage
  Settings->knx_GA_addr[0] = teleinfo_winky.usb.ref;
  Settings->knx_GA_addr[1] = teleinfo_winky.capa.ref;
  Settings->knx_GA_addr[2] = teleinfo_winky.linky.ref;
}

// update USB voltage : get raw value, calculate max raw for 5V and calculate voltage
void TeleinfoWinkyReadVoltageUSB ()
{
  if (teleinfo_winky.usb.pin > 0)
  {
    teleinfo_winky.usb.raw = (uint32_t)AdcRead (teleinfo_winky.usb.pin, 3);
    teleinfo_winky.usb.ref = max (teleinfo_winky.usb.ref, teleinfo_winky.usb.raw);
    teleinfo_winky.usb.volt = WINKY_USB_MAXIMUM * teleinfo_winky.usb.raw / teleinfo_winky.usb.ref;
  }

  // if no usb, declare tasmota on battery
  if (teleinfo_winky.usb.volt < WINKY_USB_CHARGED) TeleinfoDriverSetBattery (true);
    else TeleinfoDriverSetBattery (false);
}

// update CAPA voltage : get raw value, calculate max raw for 5V and calculate voltage
void TeleinfoWinkyReadVoltageCapa ()
{
  if (teleinfo_winky.capa.pin > 0)
  {
    teleinfo_winky.capa.raw = (uint32_t)AdcRead (teleinfo_winky.capa.pin, 3);
    teleinfo_winky.capa.ref  = max (teleinfo_winky.capa.ref, teleinfo_winky.capa.raw);
    teleinfo_winky.capa.volt = WINKY_CAPA_MAXIMUM * teleinfo_winky.capa.raw / teleinfo_winky.capa.ref;

    // if needed; update boot voltage
    if (teleinfo_winky.volt.boot == 0) teleinfo_winky.volt.boot = teleinfo_winky.capa.volt;
  }
}

// update LINKY voltage : get raw value and calculate voltage based on dividers
void TeleinfoWinkyReadVoltageLinky ()
{
  if (teleinfo_winky.linky.pin > 0)
  {
    teleinfo_winky.linky.raw = (uint32_t)AdcRead (teleinfo_winky.linky.pin, 5);
    teleinfo_winky.linky.volt = WINKY_LINKY_MAXIMUM * teleinfo_winky.linky.raw / teleinfo_winky.capa.ref;
  }
}

uint32_t TeleinfoWinkyCalculateChargePower (const uint32_t high_mv, const uint32_t low_mv, const uint32_t capa_mf, const uint32_t delay_ms)
{
  float high_v, low_v, capa_f, delay_s, energy_mj, power_mw;

  // collect data
  high_v  = (float)(high_mv)   / 1000;
  low_v   = (float)(low_mv)    / 1000;
  capa_f  = (float)(capa_mf)   / 1000;
  delay_s = (float)(delay_ms) / 1000;

  // calculate energy charged (mJ)
  energy_mj = 500 * capa_f * (high_v * high_v - low_v * low_v);

  // convert into power (mW)
  power_mw = energy_mj / delay_s;

  return (uint32_t)power_mw;
}

uint32_t TeleinfoWinkyCalculateChargeTime (const uint32_t high_mv, const uint32_t low_mv, const uint32_t capa_mf, const uint32_t power_mw)
{
  float high_v, low_v, capa_f, power_w, energy_mj, time_ms;

  // check parameters
  if (power_mw == 0) return WINKY_SLEEP_DEFAULT;
  if (high_mv <= low_mv) return WINKY_SLEEP_MINIMUM;

  // collect data
  high_v  = (float)(high_mv)  / 1000;
  low_v   = (float)(low_mv)   / 1000;
  capa_f  = (float)(capa_mf)  / 1000;
  power_w = (float)(power_mw) / 1000;

  // calculate needed energy (mJ)
  energy_mj = 500 * capa_f * (high_v * high_v - low_v * low_v);

  // convert into power (mW)
  time_ms = energy_mj / power_w;

  return (uint32_t)time_ms;
}

uint32_t TeleinfoWinkyCalculateSleepTime ()
{
  uint32_t delay_ms;

  // if fixed deepsleep time should be applied
  if (Settings->deepsleep >= 10000) delay_ms = Settings->deepsleep;

  // else if needed apply a multiplicator to awake time
  else if (Settings->deepsleep >= 1000) delay_ms = millis () * Settings->deepsleep / 1000;

  // else read capa voltage and calculate estimated charging time to VSTART
  else
  {
    TeleinfoWinkyReadVoltageCapa ();
    delay_ms = TeleinfoWinkyCalculateChargeTime (teleinfo_winky.volt.start, teleinfo_winky.capa.volt, teleinfo_winky.farad.ref, teleinfo_sleep.charge_mw);
  }

  // cap min and max deepsleep time
  delay_ms = max (delay_ms, (uint32_t)WINKY_SLEEP_MINIMUM);
  delay_ms = min (delay_ms, (uint32_t)WINKY_SLEEP_MAXIMUM);

  return delay_ms;
}

// Enter deep sleep mode
void TeleinfoWinkyEnterSleepMode (const uint32_t delay_ms, const bool append)
{
  uint32_t sleep_ms;
  uint64_t sleep_us;

  // if needed, calculate delay
  if (delay_ms == 0) sleep_ms = TeleinfoWinkyCalculateSleepTime ();
    else sleep_ms = delay_ms;

  // update deepsleep data
  if (append) teleinfo_sleep.delay_ms += sleep_ms;
  else
  {
    teleinfo_sleep.delay_ms = sleep_ms;
    teleinfo_sleep.volt_mv  = teleinfo_winky.capa.volt;
  }

  // convert sleeptime in micro seconds
  sleep_us = (uint64_t)sleep_ms * mS_TO_uS_FACTOR;

  // switch off wifi and enter deepsleep
  WifiShutdown ();
  esp_sleep_enable_timer_wakeup (sleep_us);
  esp_deep_sleep_start ();
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
  IPAddress   ip_addr;

  // if called before suspend phase, ignore
  if (!teleinfo_winky.suspend) return;

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
  ResponseAppend_P (PSTR ("\"param\":{\"Vstart\":%u.%02u,\"Vstop\":%u.%02u}"), teleinfo_winky.volt.start / 1000, teleinfo_winky.volt.start % 1000 / 10, teleinfo_winky.volt.stop / 1000, teleinfo_winky.volt.stop % 1000 / 10);
  ResponseAppend_P (PSTR (",\"count\":{\"msg\":%u,\"cos\":%d,\"boot\":%d,\"write\":%d}"), teleinfo_meter.nb_message, teleinfo_conso.cosphi.quantity + teleinfo_prod.cosphi.quantity, Settings->bootcount, Settings->save_flag);
  ResponseAppend_P (PSTR (",\"capa\":{\"Vmax\":%u.%02u,\"Vmin\":%u.%02u}"), teleinfo_winky.volt.boot / 1000, teleinfo_winky.volt.boot % 1000 / 10, teleinfo_winky.capa.volt / 1000, teleinfo_winky.capa.volt % 1000 / 10);
  ResponseAppend_P (PSTR (",\"meter\":{\"V\":%u.%02u}"), teleinfo_winky.meter.volt / 1000, teleinfo_winky.meter.volt % 1000 / 10);
  ResponseAppend_P (PSTR (",\"time\":{\"net\":%u,\"rtc\":%u,\"mqtt\":%u,\"awake\":%u,\"sleep\":%u}}"), teleinfo_winky.timestamp.wifi, teleinfo_winky.timestamp.rtc, teleinfo_winky.timestamp.mqtt, millis (), teleinfo_winky.deepsleep);
}

/************************************\
 *             Callback
\************************************/

// Domoticz init message
void TeleinfoWinkyInit ()
{
  uint8_t  counter;
  uint32_t pin;
  uint32_t charge_mw;
  uint32_t adc_type;

  // init timestamps
  teleinfo_winky.timestamp.wifi = 0;
  teleinfo_winky.timestamp.rtc  = 0;
  teleinfo_winky.timestamp.mqtt = 0;
  teleinfo_winky.timestamp.capa = 0;

  // detect analog GPIO
  counter = 0;
  teleinfo_winky.usb.pin = 0;
  teleinfo_winky.capa.pin = 0;
  teleinfo_winky.linky.pin = 0;
  for (pin = 0; pin < nitems (TasmotaGlobal.gpio_pin); pin++) 
  {
    adc_type = TasmotaGlobal.gpio_pin[pin] >> 5;
    if (adc_type == GPIO_ADC_INPUT)
    {
      switch (counter)
      {
        case 0: teleinfo_winky.usb.pin   = pin; counter ++; break;
        case 1: teleinfo_winky.capa.pin  = pin; counter ++; break;
        case 2: teleinfo_winky.linky.pin = pin; counter ++; break;
      }
    }
  }

  // check if winky configured
  if (teleinfo_winky.capa.pin > 0) teleinfo_winky.enabled = 1;

  // load configuration
  TeleinfoWinkyLoadConfiguration ();

  // calculate charge power
  TeleinfoWinkyReadVoltageCapa ();
  TeleinfoWinkyReadVoltageUSB ();

  // if running on capa
  if (teleinfo_winky.enabled && TeleinfoDriverIsOnBattery ())
  {
    // if voltage too low to start, back to sleep mode
    if (teleinfo_winky.capa.volt < teleinfo_winky.volt.start - 50) TeleinfoWinkyEnterSleepMode (WINKY_SLEEP_DEFAULT, true);

    // if waking-up from deepslep, calculate charging power
    if (ESP_SLEEP_WAKEUP_TIMER == esp_sleep_get_wakeup_cause ())
    {
      // calculate charge power
      charge_mw = TeleinfoWinkyCalculateChargePower (teleinfo_winky.volt.boot, teleinfo_sleep.volt_mv, teleinfo_winky.farad.ref, teleinfo_sleep.delay_ms);

      // save average charging power
      if (teleinfo_sleep.charge_mw == 0) teleinfo_sleep.charge_mw = charge_mw;
        else teleinfo_sleep.charge_mw = (3 * teleinfo_sleep.charge_mw + charge_mw) / 4;
    }
  } 

  // save boot voltage
  teleinfo_winky.volt.boot = teleinfo_winky.capa.volt;

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Run winky to get help on Winky commands"));
}

// called 10 times per second to update start times
void TeleinfoWinkyEvery100ms ()
{
  // check for RTC connexion
  if ((teleinfo_winky.timestamp.rtc == 0) && RtcTime.valid) teleinfo_winky.timestamp.rtc = millis ();

  // check for network connectivity
  if (teleinfo_winky.timestamp.wifi == 0)
  {
    if (WifiHasIPv4 ())      teleinfo_winky.timestamp.wifi = millis ();
    else if (WifiHasIPv6 ()) teleinfo_winky.timestamp.wifi = millis ();
  }

  // check for mqtt connectivity
  if ((teleinfo_winky.timestamp.mqtt == 0) && MqttIsConnected ()) teleinfo_winky.timestamp.mqtt = millis ();
}

// called 4 times per second to check capa level
void TeleinfoWinkyEvery250ms ()
{
  bool     voltage_low, cosphi_reached, max_reached;
  uint32_t value;

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
  if (TeleinfoDriverIsPowered ()) teleinfo_winky.timestamp.capa = 0;

  // else if first time on battery, init capacitor calculation
  else if (teleinfo_winky.timestamp.capa == 0)
  {
    teleinfo_winky.timestamp.capa = millis ();
    teleinfo_winky.volt.measure = teleinfo_winky.capa.volt;
  }

  //  battery mode
  //  ------------

  if (TeleinfoDriverIsOnBattery ())
  {
    // check if capa voltage is too low
    voltage_low = (teleinfo_winky.capa.volt <= teleinfo_winky.volt.stop);

    // check cosphi calculation quantity
    cosphi_reached = ((teleinfo_conso.cosphi.quantity >= TIC_COSPHI_SAMPLE) || (teleinfo_prod.cosphi.quantity >= TIC_COSPHI_SAMPLE));

    // check if maximum message reception is reached
    if (teleinfo_winky.meter.max_msg == 0) max_reached = false;
      else max_reached = (teleinfo_meter.nb_message >= teleinfo_winky.meter.max_msg);

    // if voltage is low, uptime to long, cosphi ratio reached or max received message reached, start suspend
    if (voltage_low || cosphi_reached || max_reached)
    {
      // start suspend phase
      teleinfo_winky.suspend = 1;

      // if needed, publish raw TIC
      if (teleinfo_meter.json.tic) TeleinfoDriverPublishTic (true);

      // calculate deepsleep time
      teleinfo_winky.deepsleep = TeleinfoWinkyCalculateSleepTime ();

      // publish Domoticz data
      TeleinfoDomoticzPublishAll ();

      // publish Homie data
      TeleinfoHomiePublishAllData ();

      // publish Thingsboard data
      TeleinfoThingsboardPublishData ();

      // publish InfluxDB data
      TeleinfoInfluxDbPublishData ();

      // publish MQTT data
      TeleinfoDriverPublishAllData (false);

      // enter deepsleep
      TeleinfoWinkyEnterSleepMode (0, false);
    }
  }
}

// called every second to check meter max volatge level
void TeleinfoWinkyEverySecond ()
{
  bool save = false;

  // if max voltage reached, save new max
  if (teleinfo_winky.meter.volt < teleinfo_winky.linky.volt)
  {
    teleinfo_winky.meter.volt = teleinfo_winky.linky.volt;
    Settings->knx_GA_addr[7]  = (uint16_t)teleinfo_winky.meter.volt;
    save = true;
   }

  // if needed, save config
  if (save) SettingsSave (0);
}

// called to append winky InfluxDB data
void TeleinfoWinkyApiInfluxDb ()
{
  char str_line[96];

  // if called before suspend phase, ignore
  if (!teleinfo_winky.suspend) return;

  // number of received messages
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("nb-msg"), teleinfo_meter.nb_message);
  TasmotaGlobal.mqtt_data += str_line;

  // number of calculated cosphi
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%d\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("nb-cos"), teleinfo_conso.cosphi.quantity + teleinfo_prod.cosphi.quantity);
  TasmotaGlobal.mqtt_data += str_line;

  // capacitor reference
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u.%02u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("farad-ref"), teleinfo_winky.farad.ref / 1000, teleinfo_winky.farad.ref % 1000 / 10);
  TasmotaGlobal.mqtt_data += str_line;

  // meter voltage
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u.%02u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("meter-volt"), teleinfo_winky.meter.volt / 1000, teleinfo_winky.meter.volt % 1000 / 10);
  TasmotaGlobal.mqtt_data += str_line;

  // voltage capacitor at start
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u.%02u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("volt-start"), teleinfo_winky.volt.boot / 1000, teleinfo_winky.volt.boot % 1000 / 10);
  TasmotaGlobal.mqtt_data += str_line;

  // voltage capacitor at stop
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u.%02u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("volt-stop"), teleinfo_winky.capa.volt / 1000, teleinfo_winky.capa.volt % 1000 / 10);
  TasmotaGlobal.mqtt_data += str_line;

  // time to establish network (ms)
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("ms-net"), teleinfo_winky.timestamp.wifi);
  TasmotaGlobal.mqtt_data += str_line;

  // time to establish rtc (ms)
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("ms-rtc"), teleinfo_winky.timestamp.rtc);
  TasmotaGlobal.mqtt_data += str_line;

  // time to establish mqtt (ms)
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("ms-mqtt"), teleinfo_winky.timestamp.mqtt);
  TasmotaGlobal.mqtt_data += str_line;

  // total alive time (ms)
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("ms-awake"), millis ());
  TasmotaGlobal.mqtt_data += str_line;

  // duration of next deepsleep (ms)
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("ms-sleep"), teleinfo_winky.deepsleep);
  TasmotaGlobal.mqtt_data += str_line;
}

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
  WSContentSend_P (PSTR ("div.tz {float:left;-webkit-mask-image:var(--svg);mask-image:var(--svg);-webkit-mask-repeat:no-repeat;mask-repeat:no-repeat;-webkit-mask-size:100%% 100%%;mask-size:100%% 100%%;}\n"));
  WSContentSend_P (PSTR ("div.bars {width:30%%;text-align:left;background-color:#252525;border-radius:6px;}\n"));
  WSContentSend_P (PSTR ("div.bare {width:42%%;}\n"));
  WSContentSend_P (PSTR ("div.usb {height:32px;background-color:%s;--svg:url(\"data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 640 512'><path d='M641.5 256c0 3.1-1.7 6.1-4.5 7.5L547.9 317c-1.4 .8-2.8 1.4-4.5 1.4-1.4 0-3.1-.3-4.5-1.1-2.8-1.7-4.5-4.5-4.5-7.8v-35.6H295.7c25.3 39.6 40.5 106.9 69.6 106.9H392V354c0-5 3.9-8.9 8.9-8.9H490c5 0 8.9 3.9 8.9 8.9v89.1c0 5-3.9 8.9-8.9 8.9h-89.1c-5 0-8.9-3.9-8.9-8.9v-26.7h-26.7c-75.4 0-81.1-142.5-124.7-142.5H140.3c-8.1 30.6-35.9 53.5-69 53.5C32 327.3 0 295.3 0 256s32-71.3 71.3-71.3c33.1 0 61 22.8 69 53.5 39.1 0 43.9 9.5 74.6-60.4C255 88.7 273 95.7 323.8 95.7c7.5-20.9 27-35.6 50.4-35.6 29.5 0 53.5 23.9 53.5 53.5s-23.9 53.5-53.5 53.5c-23.4 0-42.9-14.8-50.4-35.6H294c-29.1 0-44.3 67.4-69.6 106.9h310.1v-35.6c0-3.3 1.7-6.1 4.5-7.8 2.8-1.7 6.4-1.4 8.9 .3l89.1 53.5c2.8 1.1 4.5 4.1 4.5 7.2z'/></svg>\");}\n"), str_color);
  WSContentSend_P (PSTR ("</style>\n"));

  // title and USB icon
  WSContentSend_P (PSTR ("<div class='tic bar'>\n"));
  WSContentSend_P (PSTR ("<div style='width:46%%;text-align:left;font-weight:bold;'>Winky</div>\n"));
  WSContentSend_P (PSTR ("<div style='width:30%%;margin-top:-2px;' class='tz usb' title='Entrée %u/4095 (%u.%02u V)'></div>\n"), teleinfo_winky.usb.raw, teleinfo_winky.usb.volt / 1000, teleinfo_winky.usb.volt % 1000 / 10);
  WSContentSend_P (PSTR ("</div>\n"));

  // capa status bar
  // ---------------
  if (teleinfo_winky.capa.pin > 0)
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

  if (teleinfo_winky.linky.pin > 0)
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

bool XdrvTeleinfoWinky (const uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
    case FUNC_COMMAND:
      result = DecodeCommand (kTeleinfoWinkyCommands, TeleinfoWinkyCommand);
      break;

    case FUNC_INIT:
      TeleinfoWinkyInit ();
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
      TeleinfoWinkySaveBeforeRestart ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      TeleinfoWinkyWebSensor ();
      break;
#endif    // USE_WEBSERVER
  }

  return result;
}

#endif    // USE_TELEINFO_WINKY
#endif    // USE_TELEINFO
