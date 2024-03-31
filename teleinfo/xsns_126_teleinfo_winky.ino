/*
  xsns_126_teleinfo_winky.ino - Deep Sleep integration for Winky Teleinfo board

  Copyright (C) 2024  Nicolas Bernaerts

  Version history :
    16/02/2024 - v1.0 - Creation

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

#ifdef USE_TELEINFO

#ifdef USE_WINKY

// declare teleinfo winky sensor
#define XSNS_126                   126

/*****************************************\
 *               Constants
\*****************************************/

#define WINKY_SLEEP_DEFAULT        60          // sleep time (seconds)

// voltage levels
#define WINKY_USB_CHARGED          4500        // USB voltage correct
#define WINKY_USB_DISCHARGED       4000        // USB voltage disconnected
#define WINKY_USB_CRITICAL         3000        // USB voltage disconnected

#define WINKY_CAPA_CHARGED         4800        // minimum capa voltage to start cycle
#define WINKY_CAPA_DISCHARGED      3800        // minimum capa voltage to start sleep process
#define WINKY_CAPA_CRITICAL        3700        // minimum capa voltage to sleep immediatly

#define WINKY_LINKY_CHARGED        9000        // minimum linky voltage to consider as connected
#define WINKY_LINKY_DISCHARGED     7000        // minimum linky voltage to consider as low
#define WINKY_LINKY_CRITICAL       5000        // minimum linky voltage to consider as very low

/***************************************\
 *               Variables
\***************************************/

// voltage sources and levels
enum TeleinfoWinkySource { WINKY_SOURCE_USB, WINKY_SOURCE_CAPA, WINKY_SOURCE_LINKY, WINKY_SOURCE_MAX };                                   // list of sources
enum TeleinfoWinkyLevel  { WINKY_LEVEL_CRITICAL, WINKY_LEVEL_DISCHARGED, WINKY_LEVEL_CORRECT, WINKY_LEVEL_CHARGED, WINKY_LEVEL_MAX };     // voltage levels
const char kTeleinfoWinkySource[] PROGMEM   = "USB|Capa|Linky";                                                                           // label of sources
const uint32_t arrTeleinfoWinkyDivider[]    = { 20, 20, 57 };                                                                             // divider used for source voltage reading
const uint32_t arrTeleinfoWinkyCharged[]    = { WINKY_USB_CHARGED,    WINKY_CAPA_CHARGED,    WINKY_LINKY_CHARGED    };                    // voltage (mV) to consider source as charged
const uint32_t arrTeleinfoWinkyDischarged[] = { WINKY_USB_DISCHARGED, WINKY_CAPA_DISCHARGED, WINKY_LINKY_DISCHARGED };                    // voltage (mV) under which source is discharged
const uint32_t arrTeleinfoWinkyCritical[]   = { WINKY_USB_CRITICAL,   WINKY_CAPA_CRITICAL,   WINKY_LINKY_CRITICAL   };                    // voltage (mV) under which source is critical

/***************************************\
 *                  Data
\***************************************/

static struct {
  uint32_t vcap_boot = 0;                       // capa voltage at start
  uint32_t time_boot = UINT32_MAX;              // timestamp of device start
  uint32_t time_ip   = UINT32_MAX;              // timestamp of network connectivity
  uint32_t time_ntp  = UINT32_MAX;              // timestamp of ntp start
  uint32_t time_mqtt = UINT32_MAX;              // timestamp of mqtt connectivity
} teleinfo_winky;

/************************************\
 *           Functions
\************************************/

// Enter deep sleep mode
void TeleinfoWinkyEnterSleepMode ()
{
  DeepSleepStart ();
}

// Get sensor raw value
uint32_t TeleinfoWinkySensorGetRaw (const uint8_t index)
{
  uint32_t value = 0;

  // read value
  if (index < WINKY_SOURCE_MAX)
  {
    value = (uint32_t)AdcRead (Adc[index].pin, 1);
    if (Adc[index].param2 > 0) value = value * Adc[index].param4 / Adc[index].param2;
  }

  return value;
}

// Get sensor voltage in mV
uint32_t TeleinfoWinkySensorGetVoltage (const uint8_t index)
{
  uint32_t value;
  uint32_t voltage = 0;

  // read value
  if (index < WINKY_SOURCE_MAX)
  {
    value = TeleinfoWinkySensorGetRaw (index);
    voltage = value * 330 * arrTeleinfoWinkyDivider[index] / 4095;
  }

  return voltage;
}

// Get source supply status
uint8_t TeleinfoWinkySourceGetLevel (const uint8_t index)
{
  uint8_t  level = WINKY_LEVEL_MAX;
  uint32_t voltage;

  // read current voltage
  if (index < WINKY_SOURCE_MAX)
  {
    voltage = TeleinfoWinkySensorGetVoltage (index);
    if (voltage <= arrTeleinfoWinkyCritical[index]) level = WINKY_LEVEL_CRITICAL;
      else if (voltage <= arrTeleinfoWinkyDischarged[index]) level = WINKY_LEVEL_DISCHARGED;
      else if (voltage < arrTeleinfoWinkyCharged[index]) level = WINKY_LEVEL_CORRECT;
      else level = WINKY_LEVEL_CHARGED;
  }

  return level;
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
  stop_vcapa  = TeleinfoWinkySensorGetVoltage (WINKY_SOURCE_CAPA);
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
  bool usb_ok, capa_ok;

  // check if USB is connected and capa charged
  usb_ok = (TeleinfoWinkySourceGetLevel (WINKY_SOURCE_USB) >= WINKY_LEVEL_CORRECT);
  capa_ok = (TeleinfoWinkySourceGetLevel (WINKY_SOURCE_CAPA) >= WINKY_LEVEL_CHARGED);
  
  // if USB not connected and capa not charged enough, entering deep sleep
  if (!usb_ok && !capa_ok) TeleinfoWinkyEnterSleepMode ();

  // if no usb, declare tasmota on battery
  if (!usb_ok) teleinfo_config.battery = 1;

  // get start datatime_t TeleinfoHistoGetLastWrite (const uint8_t period)
  teleinfo_winky.time_boot = millis ();
  teleinfo_winky.vcap_boot = TeleinfoWinkySensorGetVoltage (WINKY_SOURCE_CAPA);

  // force first publication asap
  TeleinfoDriverPublishTrigger ();
}

// called 10 times per second
void TeleinfoWinkyEvery100ms ()
{
  bool    publish;
  uint8_t level_usb, level_capa;

  // check for network connectivity
  if (teleinfo_winky.time_ip == UINT32_MAX)
  {
    if (WifiHasIPv4 ()) teleinfo_winky.time_ip = millis ();
    else if (WifiHasIPv6 ()) teleinfo_winky.time_ip = millis ();
  }

  // else check for mqtt connectivity
  else if (teleinfo_winky.time_mqtt == UINT32_MAX)
  {
    if (MqttIsConnected ()) teleinfo_winky.time_mqtt = millis ();
  }

  // check for NTP connexion
  if (teleinfo_winky.time_ntp == UINT32_MAX)
  {
    if (RtcTime.valid) teleinfo_winky.time_ntp = millis ();
  }

  // check for USB connexion
  level_usb = TeleinfoWinkySourceGetLevel (WINKY_SOURCE_USB);
  if (level_usb >= WINKY_LEVEL_CORRECT) teleinfo_config.battery = 0;

  // else, USB is not connected, check capa level
  else
  {
    // check if capa is too low
    level_capa = TeleinfoWinkySourceGetLevel (WINKY_SOURCE_CAPA);
    if (level_capa <= WINKY_LEVEL_DISCHARGED)
    {
      // stop serial reception
      TeleinfoSerialStop ();

      // log end
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: CAPA voltage low, switching off ..."));

#ifdef USE_TELEINFO_DOMOTICZ
      // publish remaining domoticz messages
      publish = TeleinfoDomoticzPublishNeeded ();
      while (publish)
      {
        // publish next topic
        TeleinfoDomoticzPublish ();

        // publish next if capa is still charged enough
        level_capa = TeleinfoWinkySourceGetLevel (WINKY_SOURCE_CAPA);
        publish = ((level_capa > WINKY_LEVEL_CRITICAL) && TeleinfoDomoticzPublishNeeded ());
      }
#endif    // USE_TELEINFO_DOMOTICZ

      // if capa is still charged enough, publish last messages
      if (level_capa > WINKY_LEVEL_CRITICAL)
      {
        // publish last meter and last alert
        if (teleinfo_config.meter) TeleinfoDriverPublishMeter ();
        TeleinfoDriverPublishAlert ();

        // publish WINKY section
        TeleinfoWinkyPublish ();
      }

      // enter deep sleep
      TeleinfoWinkyEnterSleepMode ();
    }
  }
}

#ifdef USE_WEBSERVER

// Append winky state to main page
void TeleinfoWinkyWebSensor ()
{
  uint8_t  index, level;
  uint32_t value, voltage;
  char     str_name[16];
  char     str_color[16];

  // start
  WSContentSend_P (PSTR ("<div style='font-size:13px;text-align:center;padding:4px 6px;margin-bottom:5px;background:#333333;border-radius:12px;'>\n"));
  WSContentSend_P (PSTR ("<div style='width:100%%;padding:0px;text-align:left;font-weight:bold;'>Winky</div>\n"));

  for (index = 0; index < WINKY_SOURCE_MAX; index++)
  {
    // get values
    value   = TeleinfoWinkySensorGetRaw (index);
    voltage = TeleinfoWinkySensorGetVoltage (index);
    GetTextIndexed (str_name, sizeof (str_name), index, kTeleinfoWinkySource);

    // set line color
    strcpy (str_color, "");
    level = TeleinfoWinkySourceGetLevel (index);
    if (level == WINKY_LEVEL_DISCHARGED) strcpy (str_color, "color:orange;");
    else if (level == WINKY_LEVEL_CRITICAL) strcpy (str_color, "color:red;");

    // display
    WSContentSend_P (PSTR ("<div style='display:flex;padding:2px 0px;%s'>\n"), str_color);
    WSContentSend_P (PSTR ("<div style='width:16%%;padding:0px;'></div>\n"));
    WSContentSend_P (PSTR ("<div style='width:30%%;padding:0px;text-align:left;'>%s</div>\n"), str_name);
    WSContentSend_P (PSTR ("<div style='width:22%%;padding:0px;text-align:left;color:grey;'>%u</div>\n"), value);
    WSContentSend_P (PSTR ("<div style='width:20%%;padding:0px;text-align:right;font-weight:bold;'>%u.%02u</div>\n"), voltage / 1000, (voltage % 1000) / 10);
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

#endif    // FIRMWARE_SAFEBOOT