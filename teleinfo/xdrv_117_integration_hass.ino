/*
  xdrv_117_integration_hass.ino - Home assistant auto-discovery integration for Teleinfo

  Copyright (C) 2024  Nicolas Bernaerts

  Version history :
    23/03/2024 - v1.0 - Creation (with help of msevestre31)
    28/03/2024 - v1.1 - Home Assistant auto-discovery only with SetOption19 1
    31/03/2024 - v1.2 - Remove SetOption19 dependancy
    14/04/2024 - v1.3 - switch to rf_code configuration

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

#ifdef USE_TELEINFO
#ifdef USE_TELEINFO_HASS

// declare teleinfo domoticz integration
#define XDRV_117              117

/*************************************************\
 *               Variables
\*************************************************/

#define TELEINFO_HASS_NB_MESSAGE    5

const char kTicHassVersion[] PROGMEM = EXTENSION_VERSION;

// Commands
const char kHassIntegrationCommands[] PROGMEM = "hass_" "|" "enable";
void (* const HassIntegrationCommand[])(void) PROGMEM = { &CmndHassIntegrationEnable };

/********************************\
 *              Data
\********************************/

static struct {
  uint8_t  enabled = 0;
  uint8_t  stage   = TIC_PUB_CONNECT;       // auto-discovery publication stage
  uint8_t  index   = 0;                     // period within CONTRACT stage
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

/*******************************\
 *          Command
\*******************************/

// Enable home assistant auto-discovery
void CmndHassIntegrationEnable ()
{
  if (XdrvMailbox.data_len > 0)
  {
    // update status
    hass_integration.enabled = (XdrvMailbox.payload != 0);

    // reset publication flags
    hass_integration.stage = TIC_PUB_CONNECT; 
    hass_integration.index = 0; 

    HassIntegrationSaveConfig ();
  }

  ResponseCmndNumber (hass_integration.enabled);
}

/*******************************\
 *          Callback
\*******************************/

void CmndHassIntegrationInit ()
{
  // load config
  HassIntegrationLoadConfig ();

  // if on battery, no initial publication
  if (teleinfo_config.battery) hass_integration.stage = UINT8_MAX;

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: hass_enable to enable Home Assistant auto-discovery [%u]"), hass_integration.enabled);
}

// trigger publication
void HassIntegrationEvery250ms ()
{
  // if already published or running on battery, ignore 
  if (!hass_integration.enabled) return;
  if (teleinfo_config.battery) return;
  if (!MqttIsConnected ()) return;
  if (!RtcTime.valid) return;
  if (hass_integration.stage == UINT8_MAX) return;
  if (teleinfo_meter.nb_message <= TELEINFO_HASS_NB_MESSAGE) return;

  // publication
  HassIntegrationPublish (hass_integration.stage, hass_integration.index);

  // handle increment
  if ((hass_integration.stage == TIC_PUB_RELAY_DATA) || (hass_integration.stage == TIC_PUB_TOTAL_CONSO)) hass_integration.index++;
    else hass_integration.stage++;
  if ((hass_integration.stage == TIC_PUB_RELAY_DATA)  && (hass_integration.index >= 8)) hass_integration.stage++;
  if ((hass_integration.stage == TIC_PUB_TOTAL_CONSO) && (hass_integration.index >= teleinfo_contract.period_qty)) hass_integration.stage++;
  if ((hass_integration.stage != TIC_PUB_RELAY_DATA)  && (hass_integration.stage != TIC_PUB_TOTAL_CONSO)) hass_integration.index = 0; 

  // increment to next stage
  if ((hass_integration.stage == TIC_PUB_CALENDAR)    && (!teleinfo_config.calendar))    hass_integration.stage = TIC_PUB_PROD;
  if ((hass_integration.stage == TIC_PUB_PROD)        && (!teleinfo_config.meter))       hass_integration.stage = TIC_PUB_RELAY;
  if ((hass_integration.stage == TIC_PUB_PROD)        && (teleinfo_prod.total_wh == 0))  hass_integration.stage = TIC_PUB_CONSO;
  if ((hass_integration.stage == TIC_PUB_CONSO)       && (teleinfo_conso.total_wh == 0)) hass_integration.stage = TIC_PUB_RELAY;
  if ((hass_integration.stage == TIC_PUB_PH1)         && (teleinfo_contract.phase == 1)) hass_integration.stage = TIC_PUB_RELAY;
  if ((hass_integration.stage == TIC_PUB_RELAY)       && (!teleinfo_config.relay))       hass_integration.stage = TIC_PUB_TOTAL;
  if ((hass_integration.stage == TIC_PUB_TOTAL)       && (!teleinfo_config.contract))    hass_integration.stage = TIC_PUB_MAX;
  if ((hass_integration.stage == TIC_PUB_TOTAL_PROD)  && (teleinfo_prod.total_wh == 0))  hass_integration.stage = TIC_PUB_TOTAL_CONSO;
  if ((hass_integration.stage == TIC_PUB_TOTAL_CONSO) && (teleinfo_conso.total_wh == 0)) hass_integration.stage = TIC_PUB_MAX;
  if (hass_integration.stage == TIC_PUB_MAX) hass_integration.stage = UINT8_MAX;
}

/***************************************\
 *           JSON publication
\***************************************/

void HassIntegrationPublish (const uint8_t stage, const uint8_t index)
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
  if (stage >= TIC_PUB_MAX) return;
  if ((stage == TIC_PUB_RELAY_DATA) && (index >= 8)) return;
  if ((stage == TIC_PUB_TOTAL_CONSO) && (index >= TELEINFO_INDEX_MAX)) return;

  // process data reception
  TeleinfoProcessRealTime ();

  // set parameter according to data
  strcpy (str_param, "");
  switch (stage)
  {
    // contract
//    case TIC_PUB_CONTRACT_NAME:   strcpy_P (str_param, PSTR ("Contrat"              "|CONTRACT_NAME"   "|['CONTRACT']['name']"     "|calendar"     "|" "|" "|")); break;
    case TIC_PUB_CONTRACT_NAME:   strcpy_P (str_param, PSTR ("Contrat"              "|CONTRACT_NAME"   "|.CONTRACT.name"     "|calendar"   "|" "|" "|")); break;

    // calendar
//    case TIC_PUB_CALENDAR_PERIOD: strcpy_P (str_param, PSTR ("Contrat Période"      "|CONTRACT_PERIOD" "|['CONTRACT']['period']"   "|calendar"     "|" "|" "|")); break;
//    case TIC_PUB_CALENDAR_COLOR:  strcpy_P (str_param, PSTR ("Contrat Couleur"      "|CONTRACT_COLOR"  "|['CONTRACT']['color']"    "|calendar"     "|" "|" "|")); break;
//    case TIC_PUB_CALENDAR_HOUR:   strcpy_P (str_param, PSTR ("Contrat Heure"        "|CONTRACT_HOUR"   "|['CONTRACT']['hour']"     "|calendar"     "|" "|" "|")); break;
//    case TIC_PUB_CALENDAR_TODAY:  strcpy_P (str_param, PSTR ("Couleur Aujourd'hui"  "|CONTRACT_2DAY"   "|['CONTRACT']['today']"    "|calendar"     "|" "|" "|")); break;
//    case TIC_PUB_CALENDAR_TOMRW:  strcpy_P (str_param, PSTR ("Couleur Demain"       "|CONTRACT_TMRW"   "|['CONTRACT']['tomorrow']" "|calendar"     "|" "|" "|")); break;
    case TIC_PUB_CALENDAR_PERIOD: strcpy_P (str_param, PSTR ("Contrat Période"      "|CONTRACT_PERIOD" "|.CONTRACT.period"   "|calendar"   "|" "|" "|")); break;
    case TIC_PUB_CALENDAR_COLOR:  strcpy_P (str_param, PSTR ("Contrat Couleur"      "|CONTRACT_COLOR"  "|.CONTRACT.color"    "|calendar"   "|" "|" "|")); break;
    case TIC_PUB_CALENDAR_HOUR:   strcpy_P (str_param, PSTR ("Contrat Heure"        "|CONTRACT_HOUR"   "|.CONTRACT.hour"     "|calendar"   "|" "|" "|")); break;
    case TIC_PUB_CALENDAR_TODAY:  strcpy_P (str_param, PSTR ("Couleur Aujourd'hui"  "|CONTRACT_2DAY"   "|.CONTRACT.today"    "|calendar"   "|" "|" "|")); break;
    case TIC_PUB_CALENDAR_TOMRW:  strcpy_P (str_param, PSTR ("Couleur Demain"       "|CONTRACT_TMRW"   "|.CONTRACT.tomorrow" "|calendar"   "|" "|" "|")); break;

    // production
/*    case TIC_PUB_PROD_P:    strcpy_P (str_param, PSTR ("Prod Puiss. apparente" "|METER_PP"      "|['METER']['PP']"      "|alpha-p-circle"    "|apparent_power" "|measurement"  "|VA")); break;
    case TIC_PUB_PROD_W:    strcpy_P (str_param, PSTR ("Prod Puiss. active"    "|METER_PW"      "|['METER']['PW']"      "|alpha-p-circle"    "|power"          "|measurement"  "|W" )); break;
    case TIC_PUB_PROD_C:    strcpy_P (str_param, PSTR ("Prod Cos φ"            "|METER_PC"      "|['METER']['PC']"      "|alpha-p-circle"    "|power_factor"   "|measurement"  "|"  )); break;
    case TIC_PUB_PROD_YDAY: strcpy_P (str_param, PSTR ("Prod Hier"             "|METER_PYDAY"   "|['METER']['PYDAY']"   "|alpha-p-circle"    "|energy"         "|total"        "|Wh")); break;
    case TIC_PUB_PROD_2DAY: strcpy_P (str_param, PSTR ("Prod Aujourd'hui"      "|METER_P2DAY"   "|['METER']['P2DAY']"   "|alpha-p-circle"    "|energy"         "|total"        "|Wh")); break;
*/
    case TIC_PUB_PROD_P:    strcpy_P (str_param, PSTR ("Prod Puiss. apparente" "|METER_PP"      "|.METER.PP"     "|alpha-p-circle"  "|apparent_power"  "|measurement"  "|VA")); break;
    case TIC_PUB_PROD_W:    strcpy_P (str_param, PSTR ("Prod Puiss. active"    "|METER_PW"      "|.METER.PW"     "|alpha-p-circle"  "|power"           "|measurement"  "|W" )); break;
    case TIC_PUB_PROD_C:    strcpy_P (str_param, PSTR ("Prod Cos φ"            "|METER_PC"      "|.METER.PC"     "|alpha-p-circle"  "|power_factor"    "|measurement"  "|"  )); break;
    case TIC_PUB_PROD_YDAY: strcpy_P (str_param, PSTR ("Prod Hier"             "|METER_PYDAY"   "|.METER.PYDAY"  "|alpha-p-circle"  "|energy"          "|total"        "|Wh")); break;
    case TIC_PUB_PROD_2DAY: strcpy_P (str_param, PSTR ("Prod Aujourd'hui"      "|METER_P2DAY"   "|.METER.P2DAY"  "|alpha-p-circle"  "|energy"          "|total"        "|Wh")); break;

    // conso
/*    case TIC_PUB_CONSO_U:    strcpy_P (str_param, PSTR ("Conso Tension"          "|METER_U"     "|['METER']['U']"       "|alpha-c-circle"    "|voltage"        "|measurement"  "|V" )); break;
    case TIC_PUB_CONSO_I:    strcpy_P (str_param, PSTR ("Conso Courant"          "|METER_I"     "|['METER']['I']"       "|alpha-c-circle"    "|current"        "|measurement"  "|A" )); break;
    case TIC_PUB_CONSO_P:    strcpy_P (str_param, PSTR ("Conso Puiss. apparente" "|METER_P"     "|['METER']['P']"       "|alpha-c-circle"    "|apparent_power" "|measurement"  "|VA")); break;
    case TIC_PUB_CONSO_W:    strcpy_P (str_param, PSTR ("Conso Puiss. active"    "|METER_W"     "|['METER']['W']"       "|alpha-c-circle"    "|power"          "|measurement"  "|W" )); break;
    case TIC_PUB_CONSO_C:    strcpy_P (str_param, PSTR ("Conso Cos φ"            "|METER_C"     "|['METER']['C']"       "|alpha-c-circle"    "|power_factor"   "|measurement"  "|"  )); break;
    case TIC_PUB_CONSO_YDAY: strcpy_P (str_param, PSTR ("Conso Hier"             "|METER_YDAY"  "|['METER']['YDAY']"    "|alpha-c-circle"    "|energy"         "|total"        "|Wh")); break;
    case TIC_PUB_CONSO_2DAY: strcpy_P (str_param, PSTR ("Conso Aujourd'hui"      "|METER_2DAY"  "|['METER']['2DAY']"    "|alpha-c-circle"    "|energy"         "|total"        "|Wh")); break;
*/
    case TIC_PUB_CONSO_U:    strcpy_P (str_param, PSTR ("Conso Tension"          "|METER_U"     "|.METER.U"      "|alpha-c-circle"  "|voltage"         "|measurement"  "|V" )); break;
    case TIC_PUB_CONSO_I:    strcpy_P (str_param, PSTR ("Conso Courant"          "|METER_I"     "|.METER.I"      "|alpha-c-circle"  "|current"         "|measurement"  "|A" )); break;
    case TIC_PUB_CONSO_P:    strcpy_P (str_param, PSTR ("Conso Puiss. apparente" "|METER_P"     "|.METER.P"      "|alpha-c-circle"  "|apparent_power"  "|measurement"  "|VA")); break;
    case TIC_PUB_CONSO_W:    strcpy_P (str_param, PSTR ("Conso Puiss. active"    "|METER_W"     "|.METER.W"      "|alpha-c-circle"  "|power"           "|measurement"  "|W" )); break;
    case TIC_PUB_CONSO_C:    strcpy_P (str_param, PSTR ("Conso Cos φ"            "|METER_C"     "|.METER.C"      "|alpha-c-circle"  "|power_factor"    "|measurement"  "|"  )); break;
    case TIC_PUB_CONSO_YDAY: strcpy_P (str_param, PSTR ("Conso Hier"             "|METER_YDAY"  "|.METER.YDAY"   "|alpha-c-circle"  "|energy"          "|total"        "|Wh")); break;
    case TIC_PUB_CONSO_2DAY: strcpy_P (str_param, PSTR ("Conso Aujourd'hui"      "|METER_2DAY"  "|.METER.2DAY"   "|alpha-c-circle"  "|energy"          "|total"        "|Wh")); break;

    // 3 phases
/*    case TIC_PUB_PH1_U: strcpy_P (str_param, PSTR ("Ph1 Tension"           "|METER_U1"    "|['METER']['U1']"    "|numeric-1-box-multiple"    "|voltage"        "|measurement"  "|V" )); break;
    case TIC_PUB_PH1_I: strcpy_P (str_param, PSTR ("Ph1 Courant"           "|METER_I1"    "|['METER']['I1']"    "|numeric-1-box-multiple"    "|current"        "|measurement"  "|A" )); break;
    case TIC_PUB_PH1_P: strcpy_P (str_param, PSTR ("Ph1 Puiss. apparente"  "|METER_P1"    "|['METER']['P1']"    "|numeric-1-box-multiple"    "|apparent_power" "|measurement"  "|VA")); break;
    case TIC_PUB_PH1_W: strcpy_P (str_param, PSTR ("Ph1 Puiss. active"     "|METER_W1"    "|['METER']['W1']"    "|numeric-1-box-multiple"    "|power"          "|measurement"  "|W" )); break;

    case TIC_PUB_PH2_U: strcpy_P (str_param, PSTR ("Ph2 Tension"           "|METER_U2"    "|['METER']['U2']"    "|numeric-2-box-multiple"    "|voltage"        "|measurement"  "|V" )); break;
    case TIC_PUB_PH2_I: strcpy_P (str_param, PSTR ("Ph2 Courant"           "|METER_I2"    "|['METER']['I2']"    "|numeric-2-box-multiple"    "|current"        "|measurement"  "|A" )); break;
    case TIC_PUB_PH2_P: strcpy_P (str_param, PSTR ("Ph2 Puiss. apparente"  "|METER_P2"    "|['METER']['P2']"    "|numeric-2-box-multiple"    "|apparent_power" "|measurement"  "|VA")); break;
    case TIC_PUB_PH2_W: strcpy_P (str_param, PSTR ("Ph2 Puiss. active"     "|METER_W2"    "|['METER']['W2']"    "|numeric-2-box-multiple"    "|power"          "|measurement"  "|W" )); break;

    case TIC_PUB_PH3_U: strcpy_P (str_param, PSTR ("Ph3 Tension"           "|METER_U3"    "|['METER']['U3']"    "|numeric-3-box-multiple"    "|voltage"        "|measurement"  "|V" )); break;
    case TIC_PUB_PH3_I: strcpy_P (str_param, PSTR ("Ph3 Courant"           "|METER_I3"    "|['METER']['I3']"    "|numeric-3-box-multiple"    "|current"        "|measurement"  "|A" )); break;
    case TIC_PUB_PH3_P: strcpy_P (str_param, PSTR ("Ph3 Puiss. apparente"  "|METER_P3"    "|['METER']['P3']"    "|numeric-3-box-multiple"    "|apparent_power" "|measurement"  "|VA")); break;
    case TIC_PUB_PH3_W: strcpy_P (str_param, PSTR ("Ph3 Puiss. active"     "|METER_W3"    "|['METER']['W3']"    "|numeric-3-box-multiple"    "|power"          "|measurement"  "|W" )); break;
*/
    case TIC_PUB_PH1_U: strcpy_P (str_param, PSTR ("Ph1 Tension"           "|METER_U1"  "|.METER.U1"  "|numeric-1-box-multiple"  "|voltage"         "|measurement"  "|V" )); break;
    case TIC_PUB_PH1_I: strcpy_P (str_param, PSTR ("Ph1 Courant"           "|METER_I1"  "|.METER.I1"  "|numeric-1-box-multiple"  "|current"         "|measurement"  "|A" )); break;
    case TIC_PUB_PH1_P: strcpy_P (str_param, PSTR ("Ph1 Puiss. apparente"  "|METER_P1"  "|.METER.P1"  "|numeric-1-box-multiple"  "|apparent_power"  "|measurement"  "|VA")); break;
    case TIC_PUB_PH1_W: strcpy_P (str_param, PSTR ("Ph1 Puiss. active"     "|METER_W1"  "|.METER.W1"  "|numeric-1-box-multiple"  "|power"           "|measurement"  "|W" )); break;

    case TIC_PUB_PH2_U: strcpy_P (str_param, PSTR ("Ph2 Tension"           "|METER_U2"  "|.METER.U2"  "|numeric-2-box-multiple"  "|voltage"         "|measurement"  "|V" )); break;
    case TIC_PUB_PH2_I: strcpy_P (str_param, PSTR ("Ph2 Courant"           "|METER_I2"  "|.METER.I2"  "|numeric-2-box-multiple"  "|current"         "|measurement"  "|A" )); break;
    case TIC_PUB_PH2_P: strcpy_P (str_param, PSTR ("Ph2 Puiss. apparente"  "|METER_P2"  "|.METER.P2"  "|numeric-2-box-multiple"  "|apparent_power"  "|measurement"  "|VA")); break;
    case TIC_PUB_PH2_W: strcpy_P (str_param, PSTR ("Ph2 Puiss. active"     "|METER_W2"  "|.METER.W2"  "|numeric-2-box-multiple"  "|power"           "|measurement"  "|W" )); break;

    case TIC_PUB_PH3_U: strcpy_P (str_param, PSTR ("Ph3 Tension"           "|METER_U3"  "|.METER.U3"  "|numeric-3-box-multiple"  "|voltage"         "|measurement"  "|V" )); break;
    case TIC_PUB_PH3_I: strcpy_P (str_param, PSTR ("Ph3 Courant"           "|METER_I3"  "|.METER.I3"  "|numeric-3-box-multiple"  "|current"         "|measurement"  "|A" )); break;
    case TIC_PUB_PH3_P: strcpy_P (str_param, PSTR ("Ph3 Puiss. apparente"  "|METER_P3"  "|.METER.P3"  "|numeric-3-box-multiple"  "|apparent_power"  "|measurement"  "|VA")); break;
    case TIC_PUB_PH3_W: strcpy_P (str_param, PSTR ("Ph3 Puiss. active"     "|METER_W3"  "|.METER.W3"  "|numeric-3-box-multiple"  "|power"           "|measurement"  "|W" )); break;

    // relay
    case TIC_PUB_RELAY_DATA:
      GetTextIndexed (str_text, sizeof (str_text), index, kTeleinfoRelayName);
//      snprintf_P (str_param, sizeof (str_param), PSTR ("Relai %u (%s)"     "|RELAY_%u"       "|['RELAY']['R%u']"      "|toggle-switch-off"    "|" "|" "|"), index + 1, str_text, index + 1,  index + 1);
      snprintf_P (str_param, sizeof (str_param), PSTR ("Relai %u (%s)"     "|RELAY_%u"  "|.RELAY.R%u"  "|toggle-switch-off"  "|" "|" "|"), index + 1, str_text, index + 1,  index + 1);
      break;

    // production counter
    case TIC_PUB_TOTAL_PROD: 
//      strcpy_P (str_param, PSTR ("Total Production"                        "|CONTRACT_PROD"  "|['CONTRACT']['PROD']"  "|counter"    "|energy"    "|total"  "|Wh"));
      strcpy_P (str_param, PSTR ("Total Production"                        "|CONTRACT_PROD"  "|.CONTRACT.PROD"  "|counter"  "|energy"  "|total"  "|Wh"));
      break;

    // loop thru conso counters
    case TIC_PUB_TOTAL_CONSO:
      if (teleinfo_conso.index_wh[index] > 0)
      {
        TeleinfoPeriodGetName (index, str_name, sizeof (str_name));
        TeleinfoPeriodGetCode (index, str_text, sizeof (str_text));
        snprintf_P (str_param, sizeof (str_param), PSTR ("%s"              "|CONTRACT_%u"  "|['CONTRACT']['%s']"  "|counter"  "|energy"  "|total"  "|Wh"), str_name, index, str_text);
      }
      break;
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
    if (hass_integration.stage == TIC_PUB_CONTRACT_NAME) 
    {
      // get IP address
#if defined(ESP32) && defined(USE_ETHERNET)
      if (static_cast<uint32_t>(EthernetLocalIP()) != 0) ip_address = (uint32_t)EthernetLocalIP ();
      else
#endif  
      ip_address = (uint32_t)WiFi.localIP ();

      // publish device description
      ResponseAppend_P (PSTR (",\"device\":{\"identifiers\":[\"%s\"],\"name\":\"%s\",\"manufacturer\":\"Tasmota Teleinfo\",\"sw_version\":\"%s / %s\",\"configuration_url\":\"http://%_I\"}"), NetworkUniqueId ().c_str (), SettingsText (SET_DEVICENAME), kTicHassVersion, TasmotaGlobal.version, ip_address);
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
bool Xdrv117 (uint32_t function)
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

#endif      // USE_TELEINFO_HASS
#endif      // USE_TELEINFO

