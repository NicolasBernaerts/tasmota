/*
  xdrv_98_integration_hass.ino - Home assistant auto-discovery integration for Gazpar

  Copyright (C) 2024  Nicolas Bernaerts

  Version history :
   07/05/2024 - v1.0 - Creation (with help of msevestre31)

  Configuration values are stored in :
    - Settings->rf_code[16][0]  : Flag en enable/disable integration

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
#ifdef USE_GAZPAR_HASS

// declare teleinfo domoticz integration
#define XDRV_98             98

/*************************************************\
 *               Variables
\*************************************************/

const char kHassVersion[] PROGMEM = EXTENSION_VERSION;

// Commands
const char kHassIntegrationCommands[] PROGMEM = "hass_" "|" "enable";
void (* const HassIntegrationCommand[])(void) PROGMEM = { &CmndHassIntegrationEnable };

/********************************\
 *              Data
\********************************/

static struct {
  uint8_t  enabled = 0;
  uint8_t  stage   = GAZ_PUB_CONNECT;       // auto-discovery publication stage
  uint8_t  data    = 0;                     // index of first data to publish
} hass_integration;

/**********************************************\
 *                Configuration
\**********************************************/

// load configuration
void HassIntegrationLoadConfig () 
{
  hass_integration.enabled = Settings->rf_code[16][0];
}

// save configuration
void HassIntegrationSaveConfig () 
{
  Settings->rf_code[16][0] = hass_integration.enabled;
}

/***************************************\
 *               Function
\***************************************/

// set integration
void HassIntegrationSet (const bool enabled) 
{
  bool is_enabled = (hass_integration.enabled == 1);

  if (is_enabled != enabled)
  {
    // update status
    hass_integration.enabled = enabled;

    // reset publication flags
    hass_integration.stage = GAZ_PUB_CONNECT; 
    hass_integration.data  = 0; 

    // save configuration
    HassIntegrationSaveConfig ();
  }
}

// get integration
bool HassIntegrationGet () 
{
  return (hass_integration.enabled == 1);
}

/*******************************\
 *          Command
\*******************************/

// Enable home assistant auto-discovery
void CmndHassIntegrationEnable ()
{
  if (XdrvMailbox.data_len > 0) HassIntegrationSet (XdrvMailbox.payload != 0);
  ResponseCmndNumber (hass_integration.enabled);
}

/*******************************\
 *          Callback
\*******************************/

void CmndHassIntegrationInit ()
{
  // load config
  HassIntegrationLoadConfig ();

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: hass_enable to enable Home Assistant auto-discovery [%u]"), hass_integration.enabled);
}

// trigger publication
void HassIntegrationEvery250ms ()
{
  // if already published or running on battery, ignore 
  if (!hass_integration.enabled) return;
  if (hass_integration.stage == UINT8_MAX) return;
  if (!RtcTime.valid) return;
  if (!MqttIsConnected ()) return;

  // publication and increment
  HassIntegrationPublish (hass_integration.stage);
  hass_integration.stage++;
  if (hass_integration.stage == GAZ_PUB_MAX) hass_integration.stage = UINT8_MAX;
}

/***************************************\
 *           JSON publication
\***************************************/

//void HassIntegrationPublish (const uint8_t stage, const uint8_t index)
void HassIntegrationPublish (const uint8_t stage)
{
  uint32_t ip_address;
  char     str_icon[24];
  char     str_key[32];
  char     str_text[32];
  char     str_name[64];
  char     str_sensor[64];
  char     str_param[128];
  String   str_topic;

  // check parameters
  if (stage >= GAZ_PUB_MAX) return;

  // set parameter according to data
  strcpy (str_param, "");
  switch (stage)
  {
    case GAZ_PUB_FACTOR:   strcpy_P (str_param, PSTR ("Coefficient de conversion"  "|FACTOR"  "|.gazpar.factor"  "|gauge"  "|"       "|"             "|kWh/m³"));  break;
    case GAZ_PUB_POWER:    strcpy_P (str_param, PSTR ("Puissance"                  "|POWER"   "|.gazpar.pact"    "|fire"   "|power"  "|measurement"  "|W")); break;

    // m3
    case GAZ_PUB_M3_TOTAL: strcpy_P (str_param, PSTR ("Volume Total"       "|M3_TOTAL"  "|.gazpar.m3.total"      "|propane-tank"    "|volume"  "|total"  "|m³")); break;
    case GAZ_PUB_M3_TODAY: strcpy_P (str_param, PSTR ("Volume Aujourdhui"  "|M3_TDAY"   "|.gazpar.m3.yesterday"  "|propane-tank"    "|volume"  "|total"  "|m³")); break;
    case GAZ_PUB_M3_YTDAY: strcpy_P (str_param, PSTR ("Volume Hier"        "|M3_YDAY"   "|.gazpar.m3.today"      "|propane-tank"    "|volume"  "|total"  "|m³")); break;

    // Wh
    case GAZ_PUB_WH_TOTAL: strcpy_P (str_param, PSTR ("Conso Totale"       "|WH_TOTAL"  "|.gazpar.wh.total"      "|lightning-bolt"  "|energy"  "|total"  "|Wh")); break;
    case GAZ_PUB_WH_TODAY: strcpy_P (str_param, PSTR ("Conso Aujourdhui"   "|WH_TDAY"   "|.gazpar.wh.yesterday"  "|lightning-bolt"  "|energy"  "|total"  "|Wh")); break;
    case GAZ_PUB_WH_YTDAY: strcpy_P (str_param, PSTR ("Conso Hier"         "|WH_YDAY"   "|.gazpar.wh.today"      "|lightning-bolt"  "|energy"  "|total"  "|Wh")); break;
  }

  // if data are available, publish
  if (strlen (str_param) > 0)
  {
   // publish auto-discovery retained message
    GetTextIndexed (str_name, sizeof (str_name), 0, str_param);
    GetTextIndexed (str_text, sizeof (str_text), 1, str_param);
    GetTextIndexed (str_key,  sizeof (str_key),  2, str_param);
    GetTextIndexed (str_icon, sizeof (str_icon), 3, str_param);
    GetTopic_P (str_sensor, TELE, TasmotaGlobal.mqtt_topic, PSTR (D_RSLT_SENSOR));
    Response_P (PSTR ("{\"name\":\"%s\",\"state_topic\":\"%s\",\"unique_id\":\"%s_%s\",\"value_template\":\"{{value_json%s}}\",\"icon\":\"mdi:%s\""), str_name, str_sensor, NetworkUniqueId().c_str(), str_text, str_key, str_icon);

    // if first call, append device description
    if (hass_integration.stage == GAZ_PUB_FACTOR) 
    {
      // get IP address
#if defined(ESP32) && defined(USE_ETHERNET)
      if (static_cast<uint32_t>(EthernetLocalIP()) != 0) ip_address = (uint32_t)EthernetLocalIP ();
      else
#endif  
      ip_address = (uint32_t)WiFi.localIP ();

      // publish device description
      ResponseAppend_P (PSTR (",\"device\":{\"identifiers\":[\"%s\"],\"name\":\"%s\",\"manufacturer\":\"Tasmota Teleinfo\",\"sw_version\":\"%s / %s\",\"configuration_url\":\"http://%_I\"}"), NetworkUniqueId ().c_str (), SettingsText (SET_DEVICENAME), kHassVersion, TasmotaGlobal.version, ip_address);
    }
    else ResponseAppend_P (PSTR (",\"device\":{\"identifiers\":[\"%s\"]}"), NetworkUniqueId().c_str());

    // if defined, append specific values
    GetTextIndexed (str_text, sizeof (str_text), 4, str_param);
    if (strlen (str_text) > 0) ResponseAppend_P (PSTR (",\"device_class\":\"%s\""), str_text);
    GetTextIndexed (str_text, sizeof (str_text), 5, str_param);
    if (strlen (str_text) > 0) ResponseAppend_P (PSTR (",\"state_class\":\"%s\""), str_text);
    GetTextIndexed (str_text, sizeof (str_text), 6, str_param);
    if (strlen (str_text) > 0) ResponseAppend_P (PSTR (",\"unit_of_measurement\":\"%s\""), str_text);
    ResponseAppend_P (PSTR ("}"));

    // get sensor topic
    str_topic  = PSTR ("homeassistant/sensor/");
    str_topic += NetworkUniqueId () + "_";
    GetTextIndexed (str_text, sizeof (str_text), 1, str_param);
    str_topic += str_text;
    str_topic += "/config";

    // publish data
    MqttPublish (str_topic.c_str (), true);
  }
}

/***************************************\
 *              Interface
\***************************************/

// Teleinfo energy driver
bool Xdrv98 (uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
    case FUNC_INIT:
      CmndHassIntegrationInit ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kHassIntegrationCommands, HassIntegrationCommand);
      break;
    case FUNC_EVERY_250_MSECOND:
      HassIntegrationEvery250ms ();
      break;
  }

  return result;
}

#endif      // USE_GAZPAR_HASS
#endif      // USE_GAZPAR

