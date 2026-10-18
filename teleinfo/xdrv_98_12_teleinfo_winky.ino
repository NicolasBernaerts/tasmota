/*
  xdrv_98_12_winky_status.ino - Teleinfo sensor for Winky boards to handle voltages and deep sleep

  Copyright (C) 2024-2025  Nicolas Bernaerts

  Version history :
    16/02/2024 v1.0 - Creation, support for Winky C6
    25/08/2024 v1.1 - Add Thingsboard integration
                      Support for Winky C3
    12/10/2024 v1.2 - Force deepsleeptime to default if undefined
    22/03/2025 v1.3 - Add max received message before sleep
    10/06/2025 v2.0 - Complete rework of deepsleep management
    10/07/2025 v3.0 - Refactoring based on Tasmota 15
                      Conversion from sensor to driver
    01/08/2025 v3.1 - Add winky_display command
    19/09/2025 v3.2 - Hide and Show with click on main page display
    12/10/2025 v3.3 - Change on winky_max command
    21/06/2026 v4.0 - Complete rewrite of deepsleep triggers and calculation
                      Supercapa safe voltage is now targeted to extend lifetime
                      Removed winky_max, winky_sleep, winky_start, winky_stop, winky_coeff and winky_meter
                      Added winky_target and winky_cosphi
    26/06/2026 v4.1 - Add mw in InfluxDB message
                      Reboot if too many reception errors
    27/08/2026 v4.2 - Add mw in InfluxDB message
    28/09/2026 v3.1 - Check network connectivity before suspend
                      
  Configuration values are stored in :
    - Settings->knx_GA_param[0]   : main page display
    - Settings->knx_GA_param[1]   : number of cosphi before deepsleep
    - Settings->knx_GA_addr[3]    : targeted start voltage (mV)
    - Settings->knx_GA_addr[5]    : reference capacity (mF)

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

#define mS_TO_uS_FACTOR             1000ULL                            // conversion factor from seconds to micro seconds

#define WINKY_SLEEP_MINIMUM         10000                              // minimum sleep time (10 sec)
#define WINKY_SLEEP_RESTART         30000                              // minimum sleep time if sleep started at boot (30 sec)

#define WINKY_VOLTAGE_ADC_MAX       4095                               // max counter for ADC input

#define WINKY_CAPA_CREF             1500                               // by default 1.5F
#define WINKY_CAPA_CMIN             500                                // minimum is 0.5F

#define WINKY_LOAD_REFERENCE        130                                // reference linky load capability (130 mW)

#define WINKY_ERROR_MAX             10                                 // maximum number of errors before reboot

// voltage levels
#define WINKY_USB_VMAX              5000                               // USB max voltage
#define WINKY_USB_VMIN              4400                               // minimum USB voltage

#define WINKY_CAPA_VMAX             5000                               // Capa max voltage
#define WINKY_CAPA_VTARGET          4500                               // target capa voltage to start cycle
#define WINKY_CAPA_VMIN             3800                               // minimum capa voltage to start sleep process
#define WINKY_CAPA_VDELTA           100                                // minimum delta between discharged and charged

#define WINKY_LINKY_VMAX            14250                              // Linky max voltage (57 / 20 * 5 V)
#define WINKY_LINKY_VDISPLAY        12000                              // Linky max display voltage (12 V)

enum TeleinfoWinkySusupend { WINKY_SUSPEND_NONE, WINKY_SUSPEND_SOON, WINKY_SUSPEND_IMMEDIATE, WINKY_SUSPEND_MAX };
enum TeleinfoWinkyPublish  { WINKY_PUBLISH_NONE, WINKY_PUBLISH_HTTP, WINKY_PUBLISH_MQTT, WINKY_PUBLISH_DONE };

// Commands
static const char kTeleinfoWinkyCommands[]  PROGMEM =          "winky|"      "|"       "_capa"       "|"       "_target"       "|"       "_display"       "|"       "_cosphi";
void (* const TeleinfoWinkyCommand[])(void) PROGMEM = { &CmndTeleinfoWinkyHelp, &CmndTeleinfoWinkyCapa, &CmndTeleinfoWinkyTarget, &CmndTeleinfoWinkyDisplay, &CmndTeleinfoWinkyCosPhi };

/***************************************\
 *               Data
\***************************************/

// configuration
struct {
  bool     display = false;             // flag set if winky is displayed in main mage
  uint32_t capa_mf;                     // reference capacitor value (mF)
  uint32_t target_mv;                   // target supercapa voltage at boot (mV)
  uint32_t linky_mv;                    // meter max voltage (mV)
  long     cosphi;                      // number of cosphi calculation to do before deepsleep
} winky_config;

// RTC protected data
RTC_DATA_ATTR struct {      // data in NVRAM to survive deepsleep
  uint32_t sleep_ms;                    // last deepsleep delay (ms)
  uint32_t sleep_mv;                    // capa voltage when entering last deepsleep (mV)
  uint32_t charge_mw;                   // average charging power during deepsleep (mW)
} winky_rtc;

struct winky_src {
  uint8_t  pin;                         // pin used for source
  uint32_t raw;                         // raw value
  uint32_t volt;                        // voltage (mV)
};

struct {
  bool      enabled   = false;          // flag set if winky configured
  uint8_t   suspend   = WINKY_SUSPEND_NONE;          // suspend steps
  uint8_t   publish   = WINKY_PUBLISH_NONE;          // publication steps
  bool      pub_http  = false;          // flag to validate http publication
  uint32_t  deepsleep = 0;              // calculated deepsleep time (ms)
  uint32_t  boot_mv   = 0;              // capa voltage at boot (mV)
  uint32_t  wifi_ms   = 0;              // wifi connexion timestamp
  uint32_t  mqtt_ms   = 0;              // MQTT connexion timestamp
  uint32_t  rtc_ms    = 0;              // RTC synchro timestamp
  winky_src usb;
  winky_src capa;
  winky_src linky;
} winky_status;

/************************\
 *        Commands
\************************/

// Capacitor management help
void CmndTeleinfoWinkyHelp ()
{
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: gestion du winky"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - winky_display <0/1> = données sur page d'accueil [%u]"), winky_config.display);
  AddLog (LOG_LEVEL_INFO, PSTR (" - winky_target <volt> = tension cible au démarrage [%u.%02u]"), winky_config.target_mv / 1000, (winky_config.target_mv % 1000) / 10);
  AddLog (LOG_LEVEL_INFO, PSTR (" - winky_capa <farad>  = capacité de la super capa (F) [%u.%02u]"), winky_config.capa_mf / 1000, (winky_config.capa_mf % 1000) / 10);
  AddLog (LOG_LEVEL_INFO, PSTR (" - winky_cosphi <num>  = nombre de calculs cosphi avant sommeil [%d]"), winky_config.cosphi);
  ResponseCmndDone ();
}

// Set reference of super capa
void CmndTeleinfoWinkyCapa ()
{
  uint32_t capa_mf;
  char     str_result[8];

  // if voltage is set
  if (XdrvMailbox.data_len > 0)
  {
    // if new start voltage greater than stop voltage, set and save
    capa_mf = (uint32_t)(atof (XdrvMailbox.data) * 1000);
    if (capa_mf > 0)
    {
      winky_config.capa_mf = capa_mf;
      Settings->knx_GA_addr[5] = (uint16_t)capa_mf;
      SettingsSave (0);
    }
  }
  
  // display value
  sprintf_P (str_result, PSTR ("%u.%02u Farad"), winky_config.capa_mf / 1000, (winky_config.capa_mf % 1000) / 10);
  ResponseCmndChar (str_result);
}

// Set target super capacitor voltage to start Winky
void CmndTeleinfoWinkyTarget ()
{
  uint32_t voltage;
  char     str_result[8];

  // if voltage is set
  if (XdrvMailbox.data_len > 0)
  {
    // if new start voltage greater than stop voltage, set and save
    voltage = (uint32_t)(atof (XdrvMailbox.data) * 1000);
    if (voltage > WINKY_CAPA_VMIN + WINKY_CAPA_VDELTA)
    {
      winky_config.target_mv = voltage;
      Settings->knx_GA_addr[3] = (uint16_t)voltage;
      SettingsSave (0);
    }
  }
  
  // display value
  sprintf_P (str_result, PSTR ("%u.%02u Volt"), winky_config.target_mv / 1000, (winky_config.target_mv % 1000) / 10);
  ResponseCmndChar (str_result);
}

void CmndTeleinfoWinkyDisplay ()
{
  uint32_t capa_ref;
  char     str_result[8];

  // if voltage is set
  if (XdrvMailbox.data_len > 0)
  {
    // if new start voltage greater than stop voltage, set and save
    winky_config.display = (XdrvMailbox.payload != 0);
    Settings->knx_GA_param[0] = (uint8_t)(!winky_config.display);
    SettingsSave (0);
  }

  // display value
  ResponseCmndNumber (winky_config.display);
}

void CmndTeleinfoWinkyCosPhi ()
{
  // if voltage is set
  if (XdrvMailbox.payload >= 0)
  {
    // if new start voltage greater than stop voltage, set and save
    winky_config.cosphi = (long)XdrvMailbox.payload;
    Settings->knx_GA_param[1] = (uint8_t)winky_config.cosphi;
    SettingsSave (0);
  }

  // display value
  ResponseCmndNumber (winky_config.cosphi);
}

/************************************\
 *           Functions
\************************************/

// save multipliers to settings
void TeleinfoWinkyLoadConfiguration () 
{
  // load display and cosphi calculation
  winky_config.display = !(bool)Settings->knx_GA_param[0];
  winky_config.cosphi  = (long)Settings->knx_GA_param[1];

  // load super capacitor voltage limits
  winky_config.target_mv = (uint32_t)Settings->knx_GA_addr[3];
  if (winky_config.target_mv <= WINKY_CAPA_VMIN) winky_config.target_mv = WINKY_CAPA_VTARGET;
    else if (winky_config.target_mv >= WINKY_CAPA_VMAX) winky_config.target_mv = WINKY_CAPA_VTARGET;

  // load super capacitor reference
  winky_config.capa_mf = (uint32_t)Settings->knx_GA_addr[5];
  if (winky_config.capa_mf <= WINKY_CAPA_CMIN) winky_config.capa_mf = WINKY_CAPA_CREF;
}

// update USB voltage : get raw value, calculate max raw for 5V and calculate voltage
void TeleinfoWinkyReadVoltageUSB ()
{
  if (winky_status.usb.pin > 0)
  {
    // read USB voltage
    winky_status.usb.raw  = (uint32_t)AdcRead (winky_status.usb.pin, 3);
    winky_status.usb.volt = 5000 * winky_status.usb.raw * 33 / 25 / 4095;         // 10k/10k, 5 V -> 2.5 V

    // check if running on battery
    if (winky_status.usb.volt < WINKY_USB_VMIN) TeleinfoDriverSetBattery (true);
      else TeleinfoDriverSetBattery (false);

    // log
    AddLog (LOG_LEVEL_DEBUG, PSTR ("HLP: USB voltage = %u mV (%u)"), winky_status.usb.volt, winky_status.usb.raw);
  }
}

// update CAPA voltage : get raw value, calculate max raw for 5V and calculate voltage
void TeleinfoWinkyReadVoltageCapa ()
{
  if (winky_status.capa.pin > 0)
  {
    // read Capa voltage
    winky_status.capa.raw  = (uint32_t)AdcRead (winky_status.capa.pin, 5);
    winky_status.capa.volt = 5000 * winky_status.capa.raw * 33 / 25 / 4095;       // 470k/470k, 2.5 V -> 5 V

    // log
    AddLog (LOG_LEVEL_DEBUG, PSTR ("HLP: Capa voltage = %u mV (%u)"), winky_status.capa.volt, winky_status.capa.raw);
  }
}

// update LINKY voltage : get raw value and calculate voltage based on dividers
void TeleinfoWinkyReadVoltageLinky ()
{
  uint32_t meter_mv;

  if (winky_status.linky.pin > 0)
  {
    // read Linky voltage
    winky_status.linky.raw = (uint32_t)AdcRead (winky_status.linky.pin, 3);
    meter_mv = 18810 * winky_status.linky.raw / 4095;                             // 470k/100k, 18.81 V -> 3.3 V

    // average linky voltage
    if (winky_status.linky.volt == 0) winky_status.linky.volt = meter_mv;
      else winky_status.linky.volt = (3 * winky_status.linky.volt + meter_mv) / 4;

    // log
    AddLog (LOG_LEVEL_DEBUG, PSTR ("HLP: Linky voltage = %u mV (%u)"), winky_status.linky.volt, winky_status.linky.raw);
  }
}

// calculate meter charging capability according to 
//   - voltage when charge started
//   - voltage when charge stopped
//   - supercapa value
//   - charging duration
uint32_t TeleinfoWinkyCalculateChargePower (const uint32_t capa_mf, const uint32_t start_mv, const uint32_t stop_mv, const uint32_t duration_ms)
{
  uint32_t power_mw = WINKY_LOAD_REFERENCE;
  float    duration_s, capa_f, start_v, stop_v, energy_j, power_w;

  // if calculation is possible
  duration_s = (float)duration_ms / 1000;
  if ((duration_s > 0) && (start_mv < stop_mv))
  {
    // convert data to float
    capa_f  = (float)capa_mf  / 1000;
    start_v = (float)start_mv / 1000;
    stop_v  = (float)stop_mv  / 1000;

    // calculate energy charged (mJ)
    energy_j = 0.5 * capa_f * (stop_v * stop_v - start_v * start_v);

    // convert into power (mW)
    power_w  = energy_j / duration_s;
    power_mw = (uint32_t)(1000 * power_w);
  }

  return power_mw;
}

// calculate supercapa charging time according to
//   - current voltage
//   - target voltage
//   - supercapa value
//   - average meter charge capability
uint32_t TeleinfoWinkyCalculateChargeTime (const uint32_t capa_mf, const uint32_t start_mv, const uint32_t target_mv, const uint32_t charge_mw)
{
  uint32_t time_ms = WINKY_SLEEP_MINIMUM;
  float    charge_w, capa_f, start_v, target_v, energy_j, time_s;

  // if calculation is possible
  charge_w = (float)charge_mw / 1000;
  if ((charge_w > 0) && (start_mv < target_mv))
  {
    // convert data to float
    capa_f   = (float)capa_mf   / 1000;
    start_v  = (float)start_mv  / 1000;
    target_v = (float)target_mv / 1000;

    // calculate needed energy (mJ)
    energy_j = 0.5 * capa_f * (target_v * target_v - start_v * start_v);

    // convert energy to charging time
    time_s  = energy_j / charge_w;
    time_ms = (uint32_t)(1000 * time_s);
  }

  return time_ms;
}

void TeleinfoWinkyCalculateSleepTime ()
{
  uint32_t delay_ms;

  // read capa voltage
  TeleinfoWinkyReadVoltageCapa ();

  // calculate charging time to reach target voltage
  delay_ms = TeleinfoWinkyCalculateChargeTime (winky_config.capa_mf, winky_status.capa.volt, winky_config.target_mv, winky_rtc.charge_mw);

  // set deep sleep time with a minimum value
  winky_status.deepsleep = max (delay_ms, (uint32_t)WINKY_SLEEP_MINIMUM);
}

// Enter deep sleep mode
void TeleinfoWinkyEnterSleepMode (const bool at_boot)
{
  uint64_t sleep_us;

  // switch off wifi
  WifiShutdown ();

  // if update deepsleep data, disable charge calculation during next wake-up
  if (at_boot) 
  {
    // force immediate restart sleep time
    winky_status.deepsleep = WINKY_SLEEP_RESTART;

    // reset sleep time to cancel next calculation charge
    winky_rtc.sleep_ms = 0;
  }

  // else set data for wake-up charge calculation
  else
  {
    // calculate sleep time
    TeleinfoWinkyCalculateSleepTime ();

    // save current capa voltage and sllep time
    winky_rtc.sleep_ms = winky_status.deepsleep;
    winky_rtc.sleep_mv = winky_status.capa.volt;
  }

  // convert sleeptime in micro seconds
  sleep_us = (uint64_t)winky_status.deepsleep * mS_TO_uS_FACTOR;

  // enter deepsleep
  esp_sleep_enable_timer_wakeup (sleep_us);
  esp_deep_sleep_start ();
}


// called 10 times per second to update start times
void TeleinfoWinkyCheckConnectivity ()
{
  // check for network connectivity
  if ((winky_status.wifi_ms == 0) && !TasmotaGlobal.global_state.network_down) winky_status.wifi_ms = millis ();

  // check for RTC connexion
  if ((winky_status.rtc_ms == 0) && RtcTime.valid) winky_status.rtc_ms = millis ();

  // check for mqtt connectivity
  if ((winky_status.mqtt_ms == 0) && MqttIsConnected ()) winky_status.mqtt_ms = millis ();
}

uint8_t TeleinfoWinkyReadyToPublish ()
{
  bool    ready;
  uint8_t result = WINKY_PUBLISH_NONE;

  // check minimum massages and cosphi
  ready  = (teleinfo_meter.nb_message >= TIC_MESSAGE_MIN);
  ready &= ((teleinfo_conso.cosphi.quantity >= winky_config.cosphi) || (teleinfo_prod.cosphi.quantity >= winky_config.cosphi));
  
  // if ready, set result
  if (ready) result = WINKY_PUBLISH_HTTP;

  return result;
}

uint8_t TeleinfoWinkyPublishHTTP ()
{
  uint8_t result = WINKY_PUBLISH_HTTP;

  // if wifi is active
  if (!TasmotaGlobal.global_state.network_down)
  {
#ifdef USE_TELEINFO_INFLUXDB
    // if influxDB is enabled and configured
    if (teleinfo_influxdb.ready)
    {
      // if InfluxDB connexion init ok, publish
      if (InfluxDbParameterInit ())
      {
        // estimate next sleeping time
        TeleinfoWinkyCalculateSleepTime ();

        // publish data
        TeleinfoInfluxDbPublishAllData ();
      }

      // HTTP publication done
      result = WINKY_PUBLISH_MQTT;
    }
    else
#endif      // USE_TELEINFO_INFLUXDB

#ifdef USE_TELEINFO_AWTRIX
    if (strlen (awtrix_config.str_device) > 0)
    {
      // update next awtrix page
      TeleinfoAwtrixUpdatePage ();

      // HTTP publication done
      result = WINKY_PUBLISH_MQTT;
    }
    else
#endif      // USE_TELEINFO_AWTRIX

    // if no HTTP clients, declare HTTP publication done
    result = WINKY_PUBLISH_MQTT;
  }

  return result;
}

uint8_t TeleinfoWinkyPublishMQTT ()
{
  uint8_t result = WINKY_PUBLISH_MQTT;

  // if wifi is active
  if (MqttIsConnected ())
  {
    // publish raw tic
    if (teleinfo_meter.json.tic) TeleinfoDriverPublishTic ();

#ifdef USE_TELEINFO_DOMOTICZ
    // publish domoticz data
    else if (teleinfo_domoticz.enabled) TeleinfoDomoticzPublishAll ();
#endif      // USE_TELEINFO_DOMOTICZ

#ifdef USE_TELEINFO_HOMIE
    // publish homie data
    else if (teleinfo_homie.enabled) TeleinfoHomiePublishAllData ();
#endif      // USE_TELEINFO_HOMIE

#ifdef USE_TELEINFO_THINGSBOARD
    // publish thingsboard data
    else if (teleinfo_thingsboard.enabled) TeleinfoThingsboardPublishData ();
#endif      // USE_TELEINFO_THINGSBOARD

    // publish SENSOR data
    else
    {
      TeleinfoWinkyCalculateSleepTime ();
      TeleinfoDriverJsonPublish ();
    }

    // set data as published
    result = WINKY_PUBLISH_DONE;
  }

  return result;
}

/************************************\
 *               MQTT
\************************************/

// Generate WINKY section
void TeleinfoWinkyAppendSensor ()
{
  // message
  MiscOptionPrepareJsonSection ();
  ResponseAppend_P (PSTR ("\"WINKY\":{"));
  ResponseAppend_P (PSTR ("\"msg\":%u,\"cos\":%d,\"mw\":%u"), teleinfo_meter.nb_message, teleinfo_conso.cosphi.quantity + teleinfo_prod.cosphi.quantity, winky_rtc.charge_mw);
  ResponseAppend_P (PSTR (",\"vboot\":%u.%02u,\"vsleep\":%u.%02u,\"vmeter\":%u.%02u"), winky_status.boot_mv / 1000, winky_status.boot_mv % 1000 / 10, winky_status.capa.volt / 1000, winky_status.capa.volt % 1000 / 10, winky_status.linky.volt / 1000, winky_status.linky.volt % 1000 / 10);
  ResponseAppend_P (PSTR (",\"net\":%u,\"rtc\":%u,\"mqtt\":%u,\"up\":%u,\"sleep\":%u}"), winky_status.wifi_ms, winky_status.rtc_ms, winky_status.mqtt_ms, millis (), winky_status.deepsleep);
}

/************************************\
 *             Callback
\************************************/

// Domoticz init message
void TeleinfoWinkyPreInit ()
{
  uint8_t  counter;
  uint8_t  pin;
  uint16_t adc_type;
  uint32_t charge_mw;

  // init measures
  memset (&winky_status.usb,   0, sizeof (winky_src));
  memset (&winky_status.capa,  0, sizeof (winky_src));
  memset (&winky_status.linky, 0, sizeof (winky_src));

  // detect analog GPIO
  counter = 0;
  for (pin = 0; pin < nitems (TasmotaGlobal.gpio_pin); pin++)
  {
    adc_type = TasmotaGlobal.gpio_pin[pin] >> 5;
    if (adc_type == GPIO_ADC_INPUT)
    {
      switch (counter)
      {
        case 0: winky_status.usb.pin   = pin; counter ++; break;
        case 1: winky_status.capa.pin  = pin; counter ++; break;
        case 2: winky_status.linky.pin = pin; counter ++; break;
      }
    }
  }
  winky_status.enabled = (winky_status.capa.pin > 0);

  // read initial voltage
  TeleinfoWinkyReadVoltageCapa ();
  TeleinfoWinkyReadVoltageUSB ();

  // set boot voltage
  winky_status.boot_mv = winky_status.capa.volt;

  // reset tasmota deepsleep
  Settings->deepsleep = 0;

  // load configuration
  TeleinfoWinkyLoadConfiguration ();

  // if first boot, init default linky charge 
  if (winky_rtc.charge_mw == 0) winky_rtc.charge_mw = WINKY_LOAD_REFERENCE;

  // enable winky if capa is declared
  if (winky_status.enabled)
  {
    if (TeleinfoDriverIsOnBattery ())
    {
      // if voltage too low to start, back to sleep mode
      if (winky_status.capa.volt < winky_config.target_mv - WINKY_CAPA_VDELTA) TeleinfoWinkyEnterSleepMode (true);

      // if waking-up from deepslep and charge calculation flag is set, calculate charging power with average on 4 samples
      if ((ESP_SLEEP_WAKEUP_TIMER == esp_sleep_get_wakeup_cause ()) && (winky_rtc.sleep_ms > 0))
      {
        charge_mw = TeleinfoWinkyCalculateChargePower (winky_config.capa_mf, winky_rtc.sleep_mv, winky_status.boot_mv, winky_rtc.sleep_ms);
        winky_rtc.charge_mw = (3 * winky_rtc.charge_mw + charge_mw) / 4;
      }
    }

    // log help command
    AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Run winky to get help on Winky commands"));
  }
}

// called 10 times per second to update start times
void TeleinfoWinkyEvery100ms ()
{
  // if too many reception errrors, back to sleep
  if (teleinfo_meter.nb_error >= WINKY_ERROR_MAX) TeleinfoWinkyEnterSleepMode (true);

  // check for connectivity
  TeleinfoWinkyCheckConnectivity ();
}

// called every second to check capa level and deepsleep condition
void TeleinfoWinkyEverySecond ()
{
  // if not enabled, ignore
  if (!winky_status.enabled) return;

  // check network connectivity
  TeleinfoWinkyCheckConnectivity ();

  // read USB and super capa voltage
  TeleinfoWinkyReadVoltageUSB ();
  TeleinfoWinkyReadVoltageCapa ();

  // if on battery mode
  if (TeleinfoDriverIsOnBattery ())
  {
    // if capa voltage is too low, suspend
    if (winky_status.capa.volt <= WINKY_CAPA_VMIN) winky_status.suspend = WINKY_SUSPEND_IMMEDIATE;

    // else if capa voltage is lower than the target, check data reception
    else if (winky_status.capa.volt < winky_config.target_mv - WINKY_CAPA_VDELTA) winky_status.suspend = WINKY_SUSPEND_SOON;

    // if suspend process started and minimum data received
    if (winky_status.suspend == WINKY_SUSPEND_SOON)
    {
      // check minimum message and cosphi
      if (winky_status.publish == WINKY_PUBLISH_NONE) winky_status.publish = TeleinfoWinkyReadyToPublish ();

      // handle HTTP publication stage
      if (winky_status.publish == WINKY_PUBLISH_HTTP) winky_status.publish = TeleinfoWinkyPublishHTTP ();

      // else handle MQTT publication stage
      else if (winky_status.publish == WINKY_PUBLISH_MQTT) winky_status.publish = TeleinfoWinkyPublishMQTT ();

      // if all publication done, suspend can start
      if (winky_status.publish == WINKY_PUBLISH_DONE) winky_status.suspend = WINKY_SUSPEND_IMMEDIATE;
    }

    // if validated, enter deepsleep
    if (winky_status.suspend == WINKY_SUSPEND_IMMEDIATE) TeleinfoWinkyEnterSleepMode (false);
  }

  // read meter voltage
  TeleinfoWinkyReadVoltageLinky ();
}

// called to append winky InfluxDB data
void TeleinfoWinkyInfluxDbAppendData ()
{
  char str_line[96];

  // number of received messages
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("msg"), teleinfo_meter.nb_message);
  TasmotaGlobal.mqtt_data += str_line;

  // capacitor reference
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u.%02u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("capa"), winky_config.capa_mf / 1000, winky_config.capa_mf % 1000 / 10);
  TasmotaGlobal.mqtt_data += str_line;

  // meter charging power
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("mw"), winky_rtc.charge_mw);
  TasmotaGlobal.mqtt_data += str_line;

  // meter voltage
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u.%02u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("vmeter"), winky_status.linky.volt / 1000, winky_status.linky.volt % 1000 / 10);
  TasmotaGlobal.mqtt_data += str_line;

  // voltage capacitor at start
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u.%02u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("vstart"), winky_status.boot_mv / 1000, winky_status.boot_mv % 1000 / 10);
  TasmotaGlobal.mqtt_data += str_line;

  // voltage capacitor at stop
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u.%02u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("vstop"), winky_status.capa.volt / 1000, winky_status.capa.volt % 1000 / 10);
  TasmotaGlobal.mqtt_data += str_line;

  // time to establish network (ms)
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("net"), winky_status.wifi_ms);
  TasmotaGlobal.mqtt_data += str_line;

  // time to establish rtc (ms)
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("rtc"), winky_status.rtc_ms);
  TasmotaGlobal.mqtt_data += str_line;

  // total alive time (ms)
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("up"), millis ());
  TasmotaGlobal.mqtt_data += str_line;

  // duration of next deepsleep (ms)
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u\n"), PSTR ("winky"), TasmotaGlobal.hostname, PSTR ("sleep"), winky_status.deepsleep);
  TasmotaGlobal.mqtt_data += str_line;
}

#ifdef USE_WEBSERVER
 
// winky style on main page
void TeleinfoWinkyWebMainButton ()
{
  WSContentSend_P (PSTR ("<style>\n"));

  WSContentSend_P (PSTR (".bar30 {width:30%%;background-color:%s;border-radius:6px;}\n"), PSTR (COLOR_BACKGROUND));
  WSContentSend_P (PSTR (".bar72 {width:72%%;background-color:%s;border-radius:6px;}\n"), PSTR (COLOR_BACKGROUND));
  WSContentSend_P (PSTR (".winky {width:12%%;margin-left:12%%;height:24px;-webkit-mask-image:var(--svg);mask-image:var(--svg);-webkit-mask-repeat:no-repeat;mask-repeat:no-repeat;-webkit-mask-size:100%% 100%%;mask-size:100%% 100%%;--svg:url(\"data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 640 512'><path d='M641.5 256c0 3.1-1.7 6.1-4.5 7.5L547.9 317c-1.4 .8-2.8 1.4-4.5 1.4-1.4 0-3.1-.3-4.5-1.1-2.8-1.7-4.5-4.5-4.5-7.8v-35.6H295.7c25.3 39.6 40.5 106.9 69.6 106.9H392V354c0-5 3.9-8.9 8.9-8.9H490c5 0 8.9 3.9 8.9 8.9v89.1c0 5-3.9 8.9-8.9 8.9h-89.1c-5 0-8.9-3.9-8.9-8.9v-26.7h-26.7c-75.4 0-81.1-142.5-124.7-142.5H140.3c-8.1 30.6-35.9 53.5-69 53.5C32 327.3 0 295.3 0 256s32-71.3 71.3-71.3c33.1 0 61 22.8 69 53.5 39.1 0 43.9 9.5 74.6-60.4C255 88.7 273 95.7 323.8 95.7c7.5-20.9 27-35.6 50.4-35.6 29.5 0 53.5 23.9 53.5 53.5s-23.9 53.5-53.5 53.5c-23.4 0-42.9-14.8-50.4-35.6H294c-29.1 0-44.3 67.4-69.6 106.9h310.1v-35.6c0-3.3 1.7-6.1 4.5-7.8 2.8-1.7 6.4-1.4 8.9 .3l89.1 53.5c2.8 1.1 4.5 4.1 4.5 7.2z'/></svg>\");}\n"));
  WSContentSend_P (PSTR (".disc{background-color:#e00;}\n"));
  WSContentSend_P (PSTR (".load{background-color:#0b0;}\n"));
  WSContentSend_P (PSTR (".linky{background-color:#2af;}\n"));

  WSContentSend_P (PSTR ("</style>\n"));
}

// Append winky state to main page
void TeleinfoWinkyWebSensor ()
{
  uint32_t percent;
  char     str_color[8];

  // if not configured, ignore
  if (!winky_status.enabled) return;
  if (!winky_config.display) return;

  // section
  WSContentSend_P (PSTR ("<div class='sec' onclick=\"onClickMain(\'%s?%s=%u\')\">\n"), PSTR_TIC_PAGE_DISPLAY, PSTR (CMND_TIC_MAIN), TIC_DISPLAY_WINKY);

  // title and USB icon
  if (winky_status.usb.volt > WINKY_USB_VMIN) strcpy_P (str_color, PSTR ("load")); else strcpy_P (str_color, PSTR ("disc")); 
  WSContentSend_P (PSTR ("<div class='tic ticb'><div class='tic70l'>Winky</div><div class='winky %s' title='Entrée %u/4095 (%u.%02u V)'></div></div>\n"), str_color, winky_status.usb.raw, winky_status.usb.volt / 1000, winky_status.usb.volt % 1000 / 10);

  if (teleinfo_config.arr_main[TIC_DISPLAY_WINKY])
  {
    // capa status bar
    if (winky_status.capa.pin > 0)
    {
      // calculate percentage
      percent = 100 * min (winky_status.capa.volt, (uint32_t)WINKY_CAPA_VMAX) / WINKY_CAPA_VMAX; 

      // display bar graph percentage
      WSContentSend_P (PSTR ("<div class='tic bar'><div class='tic16l'>%s</div>"), PSTR ("Capa"));
      WSContentSend_P (PSTR ("<div class='bar30'><div class='barv load' style='width:%u%%;' title='Entrée %u/4095'>%u.%02u</div></div><div class='tic46'></div><div class='tic10l'>V</div>"), percent, winky_status.capa.raw, winky_status.capa.volt / 1000, winky_status.capa.volt % 1000 / 10);
      WSContentSend_P (PSTR ("</div>\n"));
    }

    // linky status bar
    if (winky_status.linky.pin > 0)
    {
      // calculate percentage
      percent = 100 * min (winky_status.linky.volt, (uint32_t)WINKY_LINKY_VDISPLAY) / WINKY_LINKY_VDISPLAY; 

      // display bar graph percentage
      WSContentSend_P (PSTR ("<div class='tic bar'><div class='tic16l'>%s</div>"), PSTR ("Linky"));
      WSContentSend_P (PSTR ("<div class='bar72'><div class='barv linky' style='width:%u%%;' title='Entrée %u/4095'>%u.%02u</div></div><div class='tic02'></div><div class='tic10l'>V</div>"), percent, winky_status.linky.raw, winky_status.linky.volt / 1000, winky_status.linky.volt % 1000 / 10);
      WSContentSend_P (PSTR ("</div>\n"));
    }
  }

  WSContentSend_P (PSTR ("</div>\n"));    // sec
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

    case FUNC_PRE_INIT:
      TeleinfoWinkyPreInit ();
      break;

    case FUNC_EVERY_100_MSECOND:
      TeleinfoWinkyEvery100ms ();
      break;

    case FUNC_EVERY_SECOND:
       TeleinfoWinkyEverySecond ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      TeleinfoWinkyWebSensor ();
      break;

    case FUNC_WEB_ADD_MAIN_BUTTON:
      TeleinfoWinkyWebMainButton ();
      break;
#endif    // USE_WEBSERVER
  }

  return result;
}

#endif    // USE_TELEINFO_WINKY
#endif    // USE_TELEINFO
