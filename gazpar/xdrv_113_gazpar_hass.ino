/*
  xdrv_113_gazpar_hass.ino - Home assistant auto-discovery integration for Gazpar

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
#define XDRV_113             113

/*************************************************\
 *               Variables
\*************************************************/

//const char kHassVersion[] PROGMEM = EXTENSION_VERSION;

static const char PSTR_HASS_GAZ_PUB_FACTOR[]   PROGMEM = "Coefficient de conversion"  "|FACTOR"    "|.gazpar.factor"        "|gauge"           "|"        "|"             "|kWh/m³";
static const char PSTR_HASS_GAZ_PUB_POWER[]    PROGMEM = "Puissance"                  "|POWER"     "|.gazpar.pact"          "|fire"            "|power"   "|measurement"  "|W";
static const char PSTR_HASS_GAZ_PUB_M3_TOTAL[] PROGMEM = "Volume Total"               "|M3_TOTAL"  "|.gazpar.m3.total"      "|propane-tank"    "|volume"  "|total"        "|m³";
static const char PSTR_HASS_GAZ_PUB_M3_TODAY[] PROGMEM = "Volume Aujourdhui"          "|M3_TDAY"   "|.gazpar.m3.yesterday"  "|propane-tank"    "|volume"  "|total"        "|m³";
static const char PSTR_HASS_GAZ_PUB_M3_YTDAY[] PROGMEM = "Volume Hier"                "|M3_YDAY"   "|.gazpar.m3.today"      "|propane-tank"    "|volume"  "|total"        "|m³";
static const char PSTR_HASS_GAZ_PUB_WH_TOTAL[] PROGMEM = "Conso Totale"               "|WH_TOTAL"  "|.gazpar.wh.total"      "|lightning-bolt"  "|energy"  "|total"        "|Wh";
static const char PSTR_HASS_GAZ_PUB_WH_TODAY[] PROGMEM = "Conso Aujourdhui"           "|WH_TDAY"   "|.gazpar.wh.yesterday"  "|lightning-bolt"  "|energy"  "|total"        "|Wh";
static const char PSTR_HASS_GAZ_PUB_WH_YTDAY[] PROGMEM = "Conso Hier"                 "|WH_YDAY"   "|.gazpar.wh.today"      "|lightning-bolt"  "|energy"  "|total"        "|Wh";

// Commands
static const char kHassIntegrationCommands[] PROGMEM = "" "|" "hass";
void (* const HassIntegrationCommand[])(void) PROGMEM = { &CmndHassIntegrationEnable };

/********************************\
 *              Data
\********************************/

static struct {
  uint8_t  enabled = 0;
  uint8_t  stage   = GAZ_PUB_CONNECT;       // auto-discovery publication stage
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
  if (enabled != (hass_integration.enabled != 0))
  {
    // update status
    hass_integration.enabled = enabled;

    // reset publication flags
    hass_integration.stage = GAZ_PUB_CONNECT; 

    // save configuration
    HassIntegrationSaveConfig ();
  }
}

// get integration
bool HassIntegrationGet () 
{
  bool result;

  result = (hass_integration.enabled != 0);

  return result;
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
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: hass to enable Home Assistant auto-discovery [%u]"), hass_integration.enabled);
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
  char        str_text[64];
  static const char* pstr_param = nullptr;
  String      str_topic;

  // check parameters
  if (stage >= GAZ_PUB_MAX) return;

  // set parameter according to data
  switch (stage)
  {
    case GAZ_PUB_FACTOR:   pstr_param = PSTR_HASS_GAZ_PUB_FACTOR; break;
    case GAZ_PUB_POWER:    pstr_param = PSTR_HASS_GAZ_PUB_POWER;  break;

    case GAZ_PUB_M3_TOTAL: pstr_param = PSTR_HASS_GAZ_PUB_M3_TOTAL; break;
    case GAZ_PUB_M3_TODAY: pstr_param = PSTR_HASS_GAZ_PUB_M3_TODAY; break;
    case GAZ_PUB_M3_YTDAY: pstr_param = PSTR_HASS_GAZ_PUB_M3_YTDAY; break;

    case GAZ_PUB_WH_TOTAL: pstr_param = PSTR_HASS_GAZ_PUB_WH_TOTAL; break;
    case GAZ_PUB_WH_TODAY: pstr_param = PSTR_HASS_GAZ_PUB_WH_TODAY; break;
    case GAZ_PUB_WH_YTDAY: pstr_param = PSTR_HASS_GAZ_PUB_WH_YTDAY; break;
  }

  // if data are available, publish
  if (pstr_param != nullptr)
  {
    // set standard data
    GetTextIndexed (str_text, sizeof (str_text), 0, pstr_param);
    Response_P (PSTR ("{\"name\":\"%s\""), str_text);
    GetTextIndexed (str_text, sizeof (str_text), 1, pstr_param);
    ResponseAppend_P (PSTR (",\"unique_id\":\"%s_%s\""), NetworkUniqueId().c_str(), str_text);
    GetTextIndexed (str_text, sizeof (str_text), 2, pstr_param);
    ResponseAppend_P (PSTR (",\"value_template\":\"{{value_json%s}}\""), str_text);
    GetTextIndexed (str_text, sizeof (str_text), 3, pstr_param);
    ResponseAppend_P (PSTR (",\"icon\":\"mdi:%s\""), str_text);

    // if defined, append specific data
    GetTextIndexed (str_text, sizeof (str_text), 4, pstr_param);
    if (strlen (str_text) > 0) ResponseAppend_P (PSTR (",\"device_class\":\"%s\""), str_text);
    GetTextIndexed (str_text, sizeof (str_text), 5, pstr_param);
    if (strlen (str_text) > 0) ResponseAppend_P (PSTR (",\"state_class\":\"%s\""), str_text);
    GetTextIndexed (str_text, sizeof (str_text), 6, pstr_param);
    if (strlen (str_text) > 0) ResponseAppend_P (PSTR (",\"unit_of_measurement\":\"%s\""), str_text);

    // append topic
    GetTopic_P (str_text, TELE, TasmotaGlobal.mqtt_topic, D_RSLT_SENSOR);
    ResponseAppend_P (PSTR (",\"state_topic\":\"%s\""), str_text);

    // if first call, append device description
    if (hass_integration.stage == GAZ_PUB_FACTOR)  ResponseAppend_P (PSTR (",\"device\":{\"identifiers\":[\"%s\"],\"name\":\"%s\",\"manufacturer\":\"Tasmota Teleinfo\",\"sw_version\":\"%s\"}"), NetworkUniqueId ().c_str (), SettingsText (SET_DEVICENAME), TasmotaGlobal.version);
      else ResponseAppend_P (PSTR (",\"device\":{\"identifiers\":[\"%s\"]}"), NetworkUniqueId ().c_str ());

    ResponseAppend_P (PSTR ("}"));

    // get sensor topic
    str_topic  = F ("homeassistant/sensor/");
    str_topic += NetworkUniqueId ();
    str_topic += F ("_");
    str_topic += GetTextIndexed (str_text, sizeof (str_text), 1, pstr_param);
    str_topic += F ("/config");

    // publish data
    MqttPublish (str_topic.c_str (), true);
  }
}

/***************************************\
 *              Interface
\***************************************/

// Teleinfo energy driver
bool Xdrv113 (uint32_t function)
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

