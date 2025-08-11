/*
  xdrv_98_07_teleinfo_homeassistant.ino - Home assistant auto-discovery integration for Teleinfo

  Copyright (C) 2024-2025  Nicolas Bernaerts

  Version history :
    23/03/2024 v1.0 - Creation (with help of msevestre31)
    28/03/2024 v1.1 - Home Assistant auto-discovery only with SetOption19 1
    31/03/2024 v1.2 - Remove SetOption19 dependancy
    14/04/2024 v1.3 - Switch to rf_code configuration
                        Change key 2DAY to TDAY
    28/04/2024 v1.4 - Publish all counters even if equal to 0
    20/06/2024 v1.5 - Add meter serial number and global conso counter
    15/11/2024 v1.6 - Publish in mode retain
    07/02/2025 v1.7 - Add RTE Tempo and Pointe colors
    10/07/2025 v2.0 - Refactoring based on Tasmota 15

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

/*************************************************\
 *               Variables
\*************************************************/

#define TELEINFO_HASS_NB_MESSAGE    5

static const char HOMEASSISTANT_PUB_CONTRACT_NAME[]    PROGMEM = "Contrat"               "|CONTRACT_NAME"   "|.CONTRACT.name"   "|calendar" "|" "|" "|";
static const char HOMEASSISTANT_PUB_CONTRACT_SERIAL[]  PROGMEM = "N° série compteur"     "|CONTRACT_SERIAL" "|.CONTRACT.serial" "|calendar" "|" "|" "|";

static const char HOMEASSISTANT_PUB_CALENDAR_PERIOD[]  PROGMEM = "Contrat Période"       "|CONTRACT_PERIOD" "|.CONTRACT.period" "|calendar" "|" "|" "|";
static const char HOMEASSISTANT_PUB_CALENDAR_COLOR[]   PROGMEM = "Contrat Couleur"       "|CONTRACT_COLOR"  "|.CONTRACT.color"  "|calendar" "|" "|" "|";
static const char HOMEASSISTANT_PUB_CALENDAR_HOUR[]    PROGMEM = "Contrat Heure"         "|CONTRACT_HOUR"   "|.CONTRACT.hour"   "|calendar" "|" "|" "|";
static const char HOMEASSISTANT_PUB_CALENDAR_TODAY[]   PROGMEM = "Couleur Aujourdhui"    "|CONTRACT_TDAY"   "|.CONTRACT.tday"   "|calendar" "|" "|" "|";
static const char HOMEASSISTANT_PUB_CALENDAR_TOMRW[]   PROGMEM = "Couleur Demain"        "|CONTRACT_TMRW"   "|.CONTRACT.tmrw"   "|calendar" "|" "|" "|";

static const char HOMEASSISTANT_PUB_PROD_P[]           PROGMEM = "Prod Puiss. apparente" "|METER_PP"    "|.METER.PP"    "|alpha-p-circle" "|apparent_power" "|measurement" "|VA";
static const char HOMEASSISTANT_PUB_PROD_W[]           PROGMEM = "Prod Puiss. active"    "|METER_PW"    "|.METER.PW"    "|alpha-p-circle" "|power"          "|measurement" "|W";
static const char HOMEASSISTANT_PUB_PROD_C[]           PROGMEM = "Prod Cos φ"            "|METER_PC"    "|.METER.PC"    "|alpha-p-circle" "|power_factor"   "|measurement" "|";
static const char HOMEASSISTANT_PUB_PROD_YTDAY[]       PROGMEM = "Prod Hier"             "|METER_PYDAY" "|.METER.PYDAY" "|alpha-p-circle" "|energy"         "|total"       "|Wh";
static const char HOMEASSISTANT_PUB_PROD_TODAY[]       PROGMEM = "Prod Aujourdhui"       "|METER_PTDAY" "|.METER.PTDAY" "|alpha-p-circle" "|energy"         "|total"       "|Wh";

static const char HOMEASSISTANT_PUB_CONSO_U[]          PROGMEM = "Conso Tension"          "|METER_U"    "|.METER.U"     "|alpha-c-circle" "|voltage"        "|measurement" "|V";
static const char HOMEASSISTANT_PUB_CONSO_I[]          PROGMEM = "Conso Courant"          "|METER_I"    "|.METER.I"     "|alpha-c-circle" "|current"        "|measurement" "|A";
static const char HOMEASSISTANT_PUB_CONSO_P[]          PROGMEM = "Conso Puiss. apparente" "|METER_P"    "|.METER.P"     "|alpha-c-circle" "|apparent_power" "|measurement" "|VA";
static const char HOMEASSISTANT_PUB_CONSO_W[]          PROGMEM = "Conso Puiss. active"    "|METER_W"    "|.METER.W"     "|alpha-c-circle" "|power"          "|measurement" "|W";
static const char HOMEASSISTANT_PUB_CONSO_C[]          PROGMEM = "Conso Cos φ"            "|METER_C"    "|.METER.C"     "|alpha-c-circle" "|power_factor"   "|measurement" "|";
static const char HOMEASSISTANT_PUB_CONSO_YTDAY[]      PROGMEM = "Conso Hier"             "|METER_YDAY" "|.METER.YDAY"  "|alpha-c-circle" "|energy"         "|total"       "|Wh";
static const char HOMEASSISTANT_PUB_CONSO_TODAY[]      PROGMEM = "Conso Aujourdhui"       "|METER_TDAY" "|.METER.TDAY"  "|alpha-c-circle" "|energy"         "|total"       "|Wh";

static const char HOMEASSISTANT_PUB_PH1_U[]            PROGMEM = "Ph1 Tension"            "|METER_U1"   "|.METER.U1"    "|numeric-1-box-multiple" "|voltage"        "|measurement" "|V";
static const char HOMEASSISTANT_PUB_PH1_I[]            PROGMEM = "Ph1 Courant"            "|METER_I1"   "|.METER.I1"    "|numeric-1-box-multiple" "|current"        "|measurement" "|A";
static const char HOMEASSISTANT_PUB_PH1_P[]            PROGMEM = "Ph1 Puiss. apparente"   "|METER_P1"   "|.METER.P1"    "|numeric-1-box-multiple" "|apparent_power" "|measurement" "|VA";
static const char HOMEASSISTANT_PUB_PH1_W[]            PROGMEM = "Ph1 Puiss. active"      "|METER_W1"   "|.METER.W1"    "|numeric-1-box-multiple" "|power"          "|measurement" "|W";

static const char HOMEASSISTANT_PUB_PH2_U[]            PROGMEM = "Ph2 Tension"            "|METER_U2"   "|.METER.U2"    "|numeric-2-box-multiple" "|voltage"        "|measurement" "|V";
static const char HOMEASSISTANT_PUB_PH2_I[]            PROGMEM = "Ph2 Courant"            "|METER_I2"   "|.METER.I2"    "|numeric-2-box-multiple" "|current"        "|measurement" "|A";
static const char HOMEASSISTANT_PUB_PH2_P[]            PROGMEM = "Ph2 Puiss. apparente"   "|METER_P2"   "|.METER.P2"    "|numeric-2-box-multiple" "|apparent_power" "|measurement" "|VA";
static const char HOMEASSISTANT_PUB_PH2_W[]            PROGMEM = "Ph2 Puiss. active"      "|METER_W2"   "|.METER.W2"    "|numeric-2-box-multiple" "|power"          "|measurement" "|W";

static const char HOMEASSISTANT_PUB_PH3_U[]            PROGMEM = "Ph3 Tension"            "|METER_U3"   "|.METER.U3"    "|numeric-3-box-multiple" "|voltage"        "|measurement" "|V";
static const char HOMEASSISTANT_PUB_PH3_I[]            PROGMEM = "Ph3 Courant"            "|METER_I3"   "|.METER.I3"    "|numeric-3-box-multiple" "|current"        "|measurement" "|A";
static const char HOMEASSISTANT_PUB_PH3_P[]            PROGMEM = "Ph3 Puiss. apparente"   "|METER_P3"   "|.METER.P3"    "|numeric-3-box-multiple" "|apparent_power" "|measurement" "|VA";
static const char HOMEASSISTANT_PUB_PH3_W[]            PROGMEM = "Ph3 Puiss. active"      "|METER_W3"   "|.METER.W3"    "|numeric-3-box-multiple" "|power"          "|measurement" "|W";

static const char HOMEASSISTANT_PUB_RELAY_DATA[]       PROGMEM = "Relai virtuel %u"       "|RELAY_%u"       "|.RELAY.R%u"         "|toggle-switch-off" "|"       "|"      "|";

static const char HOMEASSISTANT_PUB_TOTAL_PROD[]       PROGMEM = "Total Production"       "|CONTRACT_PROD"  "|.CONTRACT.PROD"     "|counter"           "|energy" "|total" "|Wh";

static const char HOMEASSISTANT_PUB_TOTAL_CONSO[]      PROGMEM = "Total Conso"            "|CONTRACT_CONSO" "|.CONTRACT.CONSO"    "|counter"           "|energy" "|total" "|Wh";
static const char HOMEASSISTANT_PUB_TOTAL_PERIOD[]     PROGMEM = "Index %s"               "|CONTRACT_%u"    "|['CONTRACT']['%s']" "|counter"           "|energy" "|total" "|Wh";

#ifdef USE_RTE_SERVER
static const char HOMEASSISTANT_PUB_RTE_TEMPO_TODAY[]  PROGMEM = "RTE Tempo Aujourdhui"   "|TEMPO_TDAY"      "|.TEMPO.tday"      "|calendar" "|" "|" "|";
static const char HOMEASSISTANT_PUB_RTE_TEMPO_TOMRW[]  PROGMEM = "RTE Tempo Demain"       "|TEMPO_TMRW"      "|.TEMPO.tmrw"      "|calendar" "|" "|" "|";
static const char HOMEASSISTANT_PUB_RTE_POINTE_TODAY[] PROGMEM = "RTE Pointe Aujourdhui"  "|POINTE_TDAY"     "|.POINTE.tday"     "|calendar" "|" "|" "|";
static const char HOMEASSISTANT_PUB_RTE_POINTE_TOMRW[] PROGMEM = "RTE Pointe Demain"      "|POINTE_TMRW"     "|.POINTE.tmrw"     "|calendar" "|" "|" "|";
#endif  // RTE_SERVER

static const char kTicHomeAssistantVersion[]        PROGMEM = EXTENSION_VERSION;

// Commands
static const char kTeleinfoHomeAsssistantCommands[] PROGMEM = "|"           "hass";
void (* const TeleinfoHomeAssistantCommand[])(void) PROGMEM = { &CmndTeleinfoHomeAssistantEnable };

/********************************\
 *              Data
\********************************/

static struct {
  uint8_t  enabled   = 0;
  uint8_t  stage     = TIC_PUB_CONNECT;       // auto-discovery publication stage
  uint8_t  sub_stage = 0;                     // period within CONTRACT stage
} teleinfo_homeassistant;

/**********************************************\
 *                Configuration
\**********************************************/

// load configuration
void TeleinfoHomeAssistantLoadConfig () 
{
  teleinfo_homeassistant.enabled = Settings->rf_code[16][0];
}

// save configuration
void TeleinfoHomeAssistantSaveConfig () 
{
  Settings->rf_code[16][0] = teleinfo_homeassistant.enabled;
}

/***************************************\
 *               Function
\***************************************/

// set integration
void TeleinfoHomeAssistantSet (const bool enabled) 
{
  // update status
  teleinfo_homeassistant.enabled = enabled;

  // reset publication flags
  teleinfo_homeassistant.stage     = TIC_PUB_CONNECT; 
  teleinfo_homeassistant.sub_stage = 0; 

  // save configuration
  TeleinfoHomeAssistantSaveConfig ();
}

// get integration
bool TeleinfoHomeAssistantGet () 
{
  return (teleinfo_homeassistant.enabled == 1);
}

/*******************************\
 *          Command
\*******************************/

// Enable home assistant auto-discovery
void CmndTeleinfoHomeAssistantEnable ()
{
  if (XdrvMailbox.data_len > 0) TeleinfoHomeAssistantSet (XdrvMailbox.payload != 0);
  ResponseCmndNumber (teleinfo_homeassistant.enabled);
}

/*******************************\
 *          Callback
\*******************************/

void TeleinfoHomeAssistantInit ()
{
  // load config
  TeleinfoHomeAssistantLoadConfig ();

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Run hass to set Home Assistant auto-discovery [%u]"), teleinfo_homeassistant.enabled);
}

// trigger publication
void TeleinfoHomeAssistantEvery250ms ()
{
  // if on battery, no publication of persistent data
  if (TeleinfoDriverIsOnBattery ()) teleinfo_homeassistant.stage = UINT8_MAX;

  // if already published or running on battery, ignore 
  if (!teleinfo_homeassistant.enabled) return;
  if (teleinfo_homeassistant.stage == UINT8_MAX) return;
  if (!RtcTime.valid) return;
  if (!MqttIsConnected ()) return;

  // publication
  TeleinfoHomeAssistantPublish (teleinfo_homeassistant.stage, teleinfo_homeassistant.sub_stage);

  // handle increment
  if ((teleinfo_homeassistant.stage == TIC_PUB_RELAY_DATA) || (teleinfo_homeassistant.stage == TIC_PUB_TOTAL_INDEX)) teleinfo_homeassistant.sub_stage++;
    else teleinfo_homeassistant.stage++;
  if ((teleinfo_homeassistant.stage == TIC_PUB_RELAY_DATA)  && (teleinfo_homeassistant.sub_stage >= 8)) teleinfo_homeassistant.stage++;
  if ((teleinfo_homeassistant.stage == TIC_PUB_TOTAL_INDEX) && (teleinfo_homeassistant.sub_stage >= teleinfo_contract.period_qty)) teleinfo_homeassistant.stage++;
  if ((teleinfo_homeassistant.stage != TIC_PUB_RELAY_DATA)  && (teleinfo_homeassistant.stage != TIC_PUB_TOTAL_INDEX)) teleinfo_homeassistant.sub_stage = 0; 

  // increment to next stage
  if ((teleinfo_homeassistant.stage == TIC_PUB_PROD)        && (!teleinfo_config.meter))       teleinfo_homeassistant.stage = TIC_PUB_RELAY;
  if ((teleinfo_homeassistant.stage == TIC_PUB_PROD)        && (teleinfo_prod.total_wh == 0))  teleinfo_homeassistant.stage = TIC_PUB_CONSO;
  if ((teleinfo_homeassistant.stage == TIC_PUB_CONSO)       && (teleinfo_conso.total_wh == 0)) teleinfo_homeassistant.stage = TIC_PUB_RELAY;
  if ((teleinfo_homeassistant.stage == TIC_PUB_PH1)         && (teleinfo_contract.phase == 1)) teleinfo_homeassistant.stage = TIC_PUB_RELAY;
  if ((teleinfo_homeassistant.stage == TIC_PUB_RELAY)       && (!teleinfo_config.relay))       teleinfo_homeassistant.stage = TIC_PUB_TOTAL;
  if ((teleinfo_homeassistant.stage == TIC_PUB_TOTAL)       && (!teleinfo_config.meter))       teleinfo_homeassistant.stage = TIC_PUB_TOTAL_INDEX + 1;
  if ((teleinfo_homeassistant.stage == TIC_PUB_TOTAL_PROD)  && (teleinfo_prod.total_wh == 0))  teleinfo_homeassistant.stage = TIC_PUB_TOTAL_CONSO;
  if ((teleinfo_homeassistant.stage == TIC_PUB_TOTAL_CONSO) && (teleinfo_conso.total_wh == 0)) teleinfo_homeassistant.stage = TIC_PUB_TOTAL_INDEX + 1;

#ifdef USE_RTE_SERVER
  if ((teleinfo_homeassistant.stage == TIC_PUB_RTE_TEMPO_TODAY)  && !TeleinfoRteTempoIsEnabled ())  teleinfo_homeassistant.stage = TIC_PUB_RTE_POINTE_TODAY;
  if ((teleinfo_homeassistant.stage == TIC_PUB_RTE_POINTE_TODAY) && !TeleinfoRtePointeIsEnabled ()) teleinfo_homeassistant.stage = TIC_PUB_RTE_POINTE_TOMRW + 1;
#endif    // USE_RTE_SERVER

  if (teleinfo_homeassistant.stage == TIC_PUB_MAX) teleinfo_homeassistant.stage = UINT8_MAX;
}

/***************************************\
 *           JSON publication
\***************************************/

void TeleinfoHomeAssistantPublish (const uint8_t stage, const uint8_t index)
{
  uint32_t    ip_address;
  const char* pstr_param;
  char        str_text1[32];
  char        str_text2[128];
  char        str_name[64];
  char        str_id[32];
  char        str_key[32];
  char        str_icon[24];

  // check parameters
  if (stage >= TIC_PUB_MAX) return;
  if ((stage == TIC_PUB_RELAY_DATA) && (index >= 8)) return;
  if ((stage == TIC_PUB_TOTAL_INDEX) && (index >= TIC_INDEX_MAX)) return;

  // init
  strcpy (str_text1, "");
  strcpy (str_text2, "");
  strcpy (str_name,  "");
  strcpy (str_id,    "");
  strcpy (str_key,   "");

  // set parameter according to data
  pstr_param = nullptr;
  switch (stage)
  {
    // contract
    case TIC_PUB_CONTRACT_NAME:   pstr_param = HOMEASSISTANT_PUB_CONTRACT_NAME;   break;
    case TIC_PUB_CONTRACT_SERIAL: pstr_param = HOMEASSISTANT_PUB_CONTRACT_SERIAL; break;

    // calendar
    case TIC_PUB_CALENDAR_PERIOD: pstr_param = HOMEASSISTANT_PUB_CALENDAR_PERIOD; break;
    case TIC_PUB_CALENDAR_COLOR:  pstr_param = HOMEASSISTANT_PUB_CALENDAR_COLOR;  break;
    case TIC_PUB_CALENDAR_HOUR:   pstr_param = HOMEASSISTANT_PUB_CALENDAR_HOUR;   break;
    case TIC_PUB_CALENDAR_TODAY:  pstr_param = HOMEASSISTANT_PUB_CALENDAR_TODAY;  break;
    case TIC_PUB_CALENDAR_TOMRW:  pstr_param = HOMEASSISTANT_PUB_CALENDAR_TOMRW;  break;

    // production
    case TIC_PUB_PROD_P:     pstr_param = HOMEASSISTANT_PUB_PROD_P; break;
    case TIC_PUB_PROD_W:     pstr_param = HOMEASSISTANT_PUB_PROD_W; break;
    case TIC_PUB_PROD_C:     pstr_param = HOMEASSISTANT_PUB_PROD_C; break;
    case TIC_PUB_PROD_YTDAY: pstr_param = HOMEASSISTANT_PUB_PROD_YTDAY; break;
    case TIC_PUB_PROD_TODAY: pstr_param = HOMEASSISTANT_PUB_PROD_TODAY; break;

    // conso
    case TIC_PUB_CONSO_U:     pstr_param = HOMEASSISTANT_PUB_CONSO_U; break;
    case TIC_PUB_CONSO_I:     pstr_param = HOMEASSISTANT_PUB_CONSO_I; break;
    case TIC_PUB_CONSO_P:     pstr_param = HOMEASSISTANT_PUB_CONSO_P; break;
    case TIC_PUB_CONSO_W:     pstr_param = HOMEASSISTANT_PUB_CONSO_W; break;
    case TIC_PUB_CONSO_C:     pstr_param = HOMEASSISTANT_PUB_CONSO_C; break;
    case TIC_PUB_CONSO_YTDAY: pstr_param = HOMEASSISTANT_PUB_CONSO_YTDAY; break;
    case TIC_PUB_CONSO_TODAY: pstr_param = HOMEASSISTANT_PUB_CONSO_TODAY; break;

    // 3 phases
    case TIC_PUB_PH1_U: pstr_param = HOMEASSISTANT_PUB_PH1_U; break;
    case TIC_PUB_PH1_I: pstr_param = HOMEASSISTANT_PUB_PH1_I; break;
    case TIC_PUB_PH1_P: pstr_param = HOMEASSISTANT_PUB_PH1_P; break;
    case TIC_PUB_PH1_W: pstr_param = HOMEASSISTANT_PUB_PH1_W; break;

    case TIC_PUB_PH2_U: pstr_param = HOMEASSISTANT_PUB_PH2_U; break;
    case TIC_PUB_PH2_I: pstr_param = HOMEASSISTANT_PUB_PH2_I; break;
    case TIC_PUB_PH2_P: pstr_param = HOMEASSISTANT_PUB_PH2_P; break;
    case TIC_PUB_PH2_W: pstr_param = HOMEASSISTANT_PUB_PH2_W; break;

    case TIC_PUB_PH3_U: pstr_param = HOMEASSISTANT_PUB_PH3_U; break;
    case TIC_PUB_PH3_I: pstr_param = HOMEASSISTANT_PUB_PH3_I; break;
    case TIC_PUB_PH3_P: pstr_param = HOMEASSISTANT_PUB_PH3_P; break;
    case TIC_PUB_PH3_W: pstr_param = HOMEASSISTANT_PUB_PH3_W; break;

    // relay
    case TIC_PUB_RELAY_DATA:
      pstr_param = HOMEASSISTANT_PUB_RELAY_DATA;
      sprintf_P (str_name, GetTextIndexed (str_text1, sizeof (str_text1), 0, pstr_param), index + 1);
      sprintf_P (str_id,   GetTextIndexed (str_text1, sizeof (str_text1), 1, pstr_param), index + 1);
      sprintf_P (str_key,  GetTextIndexed (str_text1, sizeof (str_text1), 2, pstr_param), index + 1);
      break;

    // production counter
    case TIC_PUB_TOTAL_PROD: pstr_param = HOMEASSISTANT_PUB_TOTAL_PROD; break;

    // conso global counter
    case TIC_PUB_TOTAL_CONSO: pstr_param = HOMEASSISTANT_PUB_TOTAL_CONSO; break;

    // loop thru conso period counters
    case TIC_PUB_TOTAL_INDEX:
      pstr_param = HOMEASSISTANT_PUB_TOTAL_PERIOD;
      TeleinfoPeriodGetLabel (str_text2, sizeof (str_text2), index);
      sprintf_P (str_name, GetTextIndexed (str_text1, sizeof (str_text1), 0, pstr_param), str_text2);
      sprintf_P (str_id,   GetTextIndexed (str_text1, sizeof (str_text1), 1, pstr_param), index);
      TeleinfoPeriodGetCode (str_text2, sizeof (str_text2), index);
      sprintf_P (str_key,  GetTextIndexed (str_text1, sizeof (str_text1), 2, pstr_param), str_text2);
      break;

#ifdef USE_RTE_SERVER
    case TIC_PUB_RTE_TEMPO_TODAY:  if (TeleinfoRteTempoIsEnabled ())  pstr_param = HOMEASSISTANT_PUB_RTE_TEMPO_TODAY;  break;
    case TIC_PUB_RTE_TEMPO_TOMRW:  if (TeleinfoRteTempoIsEnabled ())  pstr_param = HOMEASSISTANT_PUB_RTE_TEMPO_TOMRW;  break;
    case TIC_PUB_RTE_POINTE_TODAY: if (TeleinfoRtePointeIsEnabled ()) pstr_param = HOMEASSISTANT_PUB_RTE_POINTE_TODAY; break;
    case TIC_PUB_RTE_POINTE_TOMRW: if (TeleinfoRtePointeIsEnabled ()) pstr_param = HOMEASSISTANT_PUB_RTE_POINTE_TOMRW; break;
#endif  // RTE_SERVER

  }

  // if data are available, publish
  if (pstr_param != nullptr)
  {
    // publish auto-discovery retained message
    if (strlen (str_name) == 0) GetTextIndexed (str_name, sizeof (str_name), 0, pstr_param);
    if (strlen (str_id) == 0)   GetTextIndexed (str_id,   sizeof (str_id),   1, pstr_param);
    if (strlen (str_key) == 0)  GetTextIndexed (str_key,  sizeof (str_key),  2, pstr_param);
    GetTextIndexed (str_icon, sizeof (str_icon), 3, pstr_param);

    // set response
    GetTopic_P (str_text2, TELE, TasmotaGlobal.mqtt_topic, D_RSLT_SENSOR);
    Response_P (PSTR ("{\"name\":\"%s\",\"state_topic\":\"%s\",\"unique_id\":\"%s_%s\",\"value_template\":\"{{value_json%s}}\",\"icon\":\"mdi:%s\""), str_name, str_text2, NetworkUniqueId().c_str(), str_id, str_key, str_icon);

    // if first call, append device description
    if (teleinfo_homeassistant.stage == TIC_PUB_CONTRACT_NAME) 
    {
      // get IP address
#if defined(ESP32) && defined(USE_ETHERNET)
      if (static_cast<uint32_t>(EthernetLocalIP()) != 0) ip_address = (uint32_t)EthernetLocalIP ();
      else
#endif  
      ip_address = (uint32_t)WiFi.localIP ();

      // publish device description
      ResponseAppend_P (PSTR (",\"device\":{\"identifiers\":[\"%s\"],\"name\":\"%s\",\"manufacturer\":\"Tasmota Teleinfo\",\"sw_version\":\"%s / %s\",\"configuration_url\":\"http://%_I\"}"), NetworkUniqueId ().c_str (), SettingsText (SET_DEVICENAME), kTicHomeAssistantVersion, TasmotaGlobal.version, ip_address);
    }
    else ResponseAppend_P (PSTR (",\"device\":{\"identifiers\":[\"%s\"]}"), NetworkUniqueId().c_str());

    // if defined, append specific values
    GetTextIndexed (str_text1, sizeof (str_text1), 4, pstr_param);
    if (strlen (str_text1) > 0) ResponseAppend_P (PSTR (",\"device_class\":\"%s\""), str_text1);
    GetTextIndexed (str_text1, sizeof (str_text1), 5, pstr_param);
    if (strlen (str_text1) > 0) ResponseAppend_P (PSTR (",\"state_class\":\"%s\""), str_text1);
    GetTextIndexed (str_text1, sizeof (str_text1), 6, pstr_param);
    if (strlen (str_text1) > 0) ResponseAppend_P (PSTR (",\"unit_of_measurement\":\"%s\""), str_text1);
    ResponseAppend_P (PSTR ("}"));

    // publish sensor topic
    strcpy_P (str_text2, PSTR ("homeassistant/sensor/"));
    strlcat  (str_text2, NetworkUniqueId ().c_str (), sizeof (str_text2));
    strcat_P (str_text2, PSTR ("_"));
    strlcat  (str_text2, str_id, sizeof (str_text2));
    strcat_P (str_text2, PSTR ("/config"));
    MqttPublish (str_text2, true);
  }
}

/***************************************\
 *              Interface
\***************************************/

bool XdrvTeleinfoHomeAssistant (const uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
    case FUNC_COMMAND:
      result = DecodeCommand (kTeleinfoHomeAsssistantCommands, TeleinfoHomeAssistantCommand);
      break;

    case FUNC_INIT:
      TeleinfoHomeAssistantInit ();
      break;

    case FUNC_EVERY_250_MSECOND:
      TeleinfoHomeAssistantEvery250ms ();
      break;
  }

  return result;
}

#endif      // USE_TELEINFO

