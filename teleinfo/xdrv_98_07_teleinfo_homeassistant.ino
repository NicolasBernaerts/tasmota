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
    07/09/2025 v2.1 - Limit publications to 1 per sec.
    22/09/2025 v2.2 - Add solar production and solar forecast
    21/11/2025 v2.3 - Add volt and load ALERT
    29/11/2025 v2.4 - Handle suppression of retain data according to publication selection

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

/*************************************************\
 *               Variables
\*************************************************/

#define TELEINFO_HASS_NB_MESSAGE    5

static const char HOMEASSISTANT_PUB_CONTRACT_NAME[]    PROGMEM = "Contrat"               "|CONTRACT_NAME"   "|.CONTRACT.name"     "|calendar"               "|"               "|"            "|";
static const char HOMEASSISTANT_PUB_CONTRACT_SERIAL[]  PROGMEM = "N° série compteur"     "|CONTRACT_SERIAL" "|.CONTRACT.serial"   "|calendar"               "|"               "|"            "|";

static const char HOMEASSISTANT_PUB_CALENDAR_PERIOD[]  PROGMEM = "Contrat Période"       "|CONTRACT_PERIOD" "|.CONTRACT.period"   "|calendar"               "|"               "|"            "|";
static const char HOMEASSISTANT_PUB_CALENDAR_COLOR[]   PROGMEM = "Contrat Couleur"       "|CONTRACT_COLOR"  "|.CONTRACT.color"    "|calendar"               "|"               "|"            "|";
static const char HOMEASSISTANT_PUB_CALENDAR_HOUR[]    PROGMEM = "Contrat Heure"         "|CONTRACT_HOUR"   "|.CONTRACT.hour"     "|calendar"               "|"               "|"            "|";
static const char HOMEASSISTANT_PUB_CALENDAR_TODAY[]   PROGMEM = "Couleur Aujourdhui"    "|CONTRACT_TDAY"   "|.CONTRACT.tday"     "|calendar"               "|"               "|"            "|";
static const char HOMEASSISTANT_PUB_CALENDAR_TOMRW[]   PROGMEM = "Couleur Demain"        "|CONTRACT_TMRW"   "|.CONTRACT.tmrw"     "|calendar"               "|"               "|"            "|";

static const char HOMEASSISTANT_PUB_PROD_P[]           PROGMEM = "Prod Puiss. apparente" "|METER_PP"        "|.METER.PP"          "|alpha-p-circle"         "|apparent_power" "|measurement" "|VA";
static const char HOMEASSISTANT_PUB_PROD_W[]           PROGMEM = "Prod Puiss. active"    "|METER_PW"        "|.METER.PW"          "|alpha-p-circle"         "|power"          "|measurement" "|W";
static const char HOMEASSISTANT_PUB_PROD_C[]           PROGMEM = "Prod Cos φ"            "|METER_PC"        "|.METER.PC"          "|alpha-p-circle"         "|power_factor"   "|measurement" "|";
static const char HOMEASSISTANT_PUB_PROD_YTDAY[]       PROGMEM = "Prod Hier"             "|METER_PYDAY"     "|.METER.PYDAY"       "|alpha-p-circle"         "|energy"         "|total"       "|Wh";
static const char HOMEASSISTANT_PUB_PROD_TODAY[]       PROGMEM = "Prod Aujourdhui"       "|METER_PTDAY"     "|.METER.PTDAY"       "|alpha-p-circle"         "|energy"         "|total"       "|Wh";

static const char HOMEASSISTANT_PUB_CONSO_U[]          PROGMEM = "Conso Tension"          "|METER_U"        "|.METER.U"           "|alpha-c-circle"         "|voltage"        "|measurement" "|V";
static const char HOMEASSISTANT_PUB_CONSO_I[]          PROGMEM = "Conso Courant"          "|METER_I"        "|.METER.I"           "|alpha-c-circle"         "|current"        "|measurement" "|A";
static const char HOMEASSISTANT_PUB_CONSO_P[]          PROGMEM = "Conso Puiss. apparente" "|METER_P"        "|.METER.P"           "|alpha-c-circle"         "|apparent_power" "|measurement" "|VA";
static const char HOMEASSISTANT_PUB_CONSO_W[]          PROGMEM = "Conso Puiss. active"    "|METER_W"        "|.METER.W"           "|alpha-c-circle"         "|power"          "|measurement" "|W";
static const char HOMEASSISTANT_PUB_CONSO_C[]          PROGMEM = "Conso Cos φ"            "|METER_C"        "|.METER.C"           "|alpha-c-circle"         "|power_factor"   "|measurement" "|";
static const char HOMEASSISTANT_PUB_CONSO_YTDAY[]      PROGMEM = "Conso Hier"             "|METER_YDAY"     "|.METER.YDAY"        "|alpha-c-circle"         "|energy"         "|total"       "|Wh";
static const char HOMEASSISTANT_PUB_CONSO_TODAY[]      PROGMEM = "Conso Aujourdhui"       "|METER_TDAY"     "|.METER.TDAY"        "|alpha-c-circle"         "|energy"         "|total"       "|Wh";

static const char HOMEASSISTANT_PUB_PH1_U[]            PROGMEM = "Ph1 Tension"            "|METER_U1"       "|.METER.U1"          "|numeric-1-box-multiple" "|voltage"        "|measurement" "|V";
static const char HOMEASSISTANT_PUB_PH1_I[]            PROGMEM = "Ph1 Courant"            "|METER_I1"       "|.METER.I1"          "|numeric-1-box-multiple" "|current"        "|measurement" "|A";
static const char HOMEASSISTANT_PUB_PH1_P[]            PROGMEM = "Ph1 Puiss. apparente"   "|METER_P1"       "|.METER.P1"          "|numeric-1-box-multiple" "|apparent_power" "|measurement" "|VA";
static const char HOMEASSISTANT_PUB_PH1_W[]            PROGMEM = "Ph1 Puiss. active"      "|METER_W1"       "|.METER.W1"          "|numeric-1-box-multiple" "|power"          "|measurement" "|W";

static const char HOMEASSISTANT_PUB_PH2_U[]            PROGMEM = "Ph2 Tension"            "|METER_U2"       "|.METER.U2"          "|numeric-2-box-multiple" "|voltage"        "|measurement" "|V";
static const char HOMEASSISTANT_PUB_PH2_I[]            PROGMEM = "Ph2 Courant"            "|METER_I2"       "|.METER.I2"          "|numeric-2-box-multiple" "|current"        "|measurement" "|A";
static const char HOMEASSISTANT_PUB_PH2_P[]            PROGMEM = "Ph2 Puiss. apparente"   "|METER_P2"       "|.METER.P2"          "|numeric-2-box-multiple" "|apparent_power" "|measurement" "|VA";
static const char HOMEASSISTANT_PUB_PH2_W[]            PROGMEM = "Ph2 Puiss. active"      "|METER_W2"       "|.METER.W2"          "|numeric-2-box-multiple" "|power"          "|measurement" "|W";

static const char HOMEASSISTANT_PUB_PH3_U[]            PROGMEM = "Ph3 Tension"            "|METER_U3"       "|.METER.U3"          "|numeric-3-box-multiple" "|voltage"        "|measurement" "|V";
static const char HOMEASSISTANT_PUB_PH3_I[]            PROGMEM = "Ph3 Courant"            "|METER_I3"       "|.METER.I3"          "|numeric-3-box-multiple" "|current"        "|measurement" "|A";
static const char HOMEASSISTANT_PUB_PH3_P[]            PROGMEM = "Ph3 Puiss. apparente"   "|METER_P3"       "|.METER.P3"          "|numeric-3-box-multiple" "|apparent_power" "|measurement" "|VA";
static const char HOMEASSISTANT_PUB_PH3_W[]            PROGMEM = "Ph3 Puiss. active"      "|METER_W3"       "|.METER.W3"          "|numeric-3-box-multiple" "|power"          "|measurement" "|W";

static const char HOMEASSISTANT_PUB_RELAY_DATA[]       PROGMEM = "Relai virtuel %u"       "|RELAY_%u"       "|.RELAY.R%u"         "|toggle-switch-off"      "|"               "|"            "|";

static const char HOMEASSISTANT_PUB_TOTAL_PROD[]       PROGMEM = "Total Production"       "|CONTRACT_PROD"  "|.CONTRACT.PROD"     "|counter"                "|energy"         "|total"       "|Wh";

static const char HOMEASSISTANT_PUB_TOTAL_CONSO[]      PROGMEM = "Total Conso"            "|CONTRACT_CONSO" "|.CONTRACT.CONSO"    "|counter"                "|energy"         "|total"       "|Wh";
static const char HOMEASSISTANT_PUB_TOTAL_PERIOD[]     PROGMEM = "Index %s"               "|CONTRACT_%u"    "|['CONTRACT']['%s']" "|counter"                "|energy"         "|total"       "|Wh";

static const char HOMEASSISTANT_PUB_ALERT_VOLT[]       PROGMEM = "Surtension"            "|ALERT_VOLT"      "|.ALERT.volt"        "|flash-triangle-outline" "|"               "|"            "|";
static const char HOMEASSISTANT_PUB_ALERT_LOAD[]       PROGMEM = "Surcharge"             "|ALERT_LOAD"      "|.ALERT.load"        "|flash-triangle-outline" "|"               "|"            "|";

#ifdef USE_TELEINFO_SOLAR
static const char HOMEASSISTANT_PUB_SOLAR_W[]          PROGMEM = "Solaire Puissance"     "|SOLAR_W"         "|.SOLAR.W"           "|weather-sunny"          "|power"          "|measurement" "|W";
static const char HOMEASSISTANT_PUB_SOLAR_TOTAL[]      PROGMEM = "Solaire Total"         "|SOLAR_TOTAL"     "|.SOLAR.Wh"          "|weather-sunny"          "|energy"         "|total"       "|Wh";

static const char HOMEASSISTANT_PUB_FORECAST_W[]       PROGMEM = "Prévision Puissance"   "|FORECAST_W"      "|.FORECAST.W"        "|weather-sunny"          "|power"          "|measurement" "|W";
static const char HOMEASSISTANT_PUB_FORECAST_TODAY[]   PROGMEM = "Prévision Aujourd'hui" "|FORECAST_TDAY"   "|.FORECAST.tday"     "|weather-sunny"          "|energy"         "|total"       "|Wh";
static const char HOMEASSISTANT_PUB_FORECAST_TOMRW[]   PROGMEM = "Prévision Demain"      "|FORECAST_TMRW"   "|.FORECAST.tmrw"     "|weather-sunny"          "|energy"         "|total"       "|Wh";
#endif  // USE_TELEINFO_SOLAR

#ifdef USE_TELEINFO_RTE
static const char HOMEASSISTANT_PUB_RTE_TEMPO_TODAY[]  PROGMEM = "Tempo Aujourdhui"      "|TEMPO_TDAY"     "|.TEMPO.tday"        "|calendar"               "|"               "|"            "|";
static const char HOMEASSISTANT_PUB_RTE_TEMPO_TOMRW[]  PROGMEM = "Tempo Demain"          "|TEMPO_TMRW"     "|.TEMPO.tmrw"        "|calendar"               "|"               "|"            "|";
static const char HOMEASSISTANT_PUB_RTE_TEMPO_DAY2[]   PROGMEM = "Tempo dans 2 jours"    "|TEMPO_DAY2"     "|.TEMPO.day2"        "|calendar"               "|"               "|"            "|";
static const char HOMEASSISTANT_PUB_RTE_TEMPO_DAY3[]   PROGMEM = "Tempo dans 3 jours"    "|TEMPO_DAY3"     "|.TEMPO.day3"        "|calendar"               "|"               "|"            "|";
static const char HOMEASSISTANT_PUB_RTE_TEMPO_DAY4[]   PROGMEM = "Tempo dans 4 jours"    "|TEMPO_DAY4"     "|.TEMPO.day4"        "|calendar"               "|"               "|"            "|";
static const char HOMEASSISTANT_PUB_RTE_TEMPO_DAY5[]   PROGMEM = "Tempo dans 5 jours"    "|TEMPO_DAY5"     "|.TEMPO.day5"        "|calendar"               "|"               "|"            "|";
static const char HOMEASSISTANT_PUB_RTE_TEMPO_DAY6[]   PROGMEM = "Tempo dans 6 jours"    "|TEMPO_DAY6"     "|.TEMPO.day6"        "|calendar"               "|"               "|"            "|";

static const char HOMEASSISTANT_PUB_RTE_POINTE_TODAY[] PROGMEM = "Pointe Aujourdhui"     "|POINTE_TDAY"    "|.POINTE.tday"       "|calendar"               "|"               "|"            "|";
static const char HOMEASSISTANT_PUB_RTE_POINTE_TOMRW[] PROGMEM = "Pointe Demain"         "|POINTE_TMRW"    "|.POINTE.tmrw"       "|calendar"               "|"               "|"            "|";
#endif  // USE_TELEINFO_RTE

static const char kTicHomeAssistantVersion[]        PROGMEM = EXTENSION_VERSION;

// Commands
static const char kTeleinfoHomeAsssistantCommands[] PROGMEM = "hass|"      ""                  "|"           "_publish";
void (* const TeleinfoHomeAssistantCommand[])(void) PROGMEM = { &CmndTeleinfoHomeAssistantEnable, &CmndTeleinfoHomeAssistantPublish };

/********************************\
 *              Data
\********************************/

static struct {
  bool enabled = false;                       // flag to enable integration
} teleinfo_hass;

#ifdef ESP32
RTC_DATA_ATTR struct {                        // data in NVRAM to survive deepsleep (for Winky)
#else
static struct {                               // data in normal RAM
#endif // ESP32
  uint8_t stage;                              // auto-discovery publication stage
  uint8_t sub_stage;                          // auto-discovery publication sub-stage
} teleinfo_hass_sleep;

/**********************************************\
 *                Configuration
\**********************************************/

// load configuration
void TeleinfoHomeAssistantLoadConfig () 
{
  teleinfo_hass.enabled = (Settings->rf_code[16][0] != 0);
}

// save configuration
void TeleinfoHomeAssistantSaveConfig () 
{
  Settings->rf_code[16][0] = (uint8_t)teleinfo_hass.enabled;
}

/*******************************\
 *          Command
\*******************************/

// Enable home assistant auto-discovery
void CmndTeleinfoHomeAssistantEnable ()
{
  if (XdrvMailbox.data_len > 0) TeleinfoHomeAssistantSet (XdrvMailbox.payload != 0);
  
  ResponseCmndNumber (teleinfo_hass.enabled);
}

// Publish home assistant retain data
void CmndTeleinfoHomeAssistantPublish ()
{
  // reset publication flags
  teleinfo_hass_sleep.stage     = TIC_PUB_CONNECT; 
  teleinfo_hass_sleep.sub_stage = 0; 

  // answer
  ResponseCmndDone ();
}

/***************************************\
 *               Function
\***************************************/

// set integration
void TeleinfoHomeAssistantSet (const bool enabled) 
{
  // update status
  teleinfo_hass.enabled = enabled;

  // reset publication flags
  teleinfo_hass_sleep.stage     = TIC_PUB_CONNECT; 
  teleinfo_hass_sleep.sub_stage = 0; 

  // save configuration
  TeleinfoHomeAssistantSaveConfig ();
}

// get integration
bool TeleinfoHomeAssistantGet () 
{
  return teleinfo_hass.enabled;
}

// puslish next home assistant integration message
void TeleinfoHomeAssistantPublishNextRetain ()
{
  // if not connected or no message received, ignore 
  if (!MqttIsConnected ()) return;
  if (teleinfo_meter.nb_message == 0) return;

  // if disabled or already published, ignore 
  if (!teleinfo_hass.enabled) teleinfo_hass_sleep.stage = UINT8_MAX;
  if (teleinfo_hass_sleep.stage == UINT8_MAX) return;

  // publish current integration message
  TeleinfoHomeAssistantPublish (teleinfo_hass_sleep.stage, teleinfo_hass_sleep.sub_stage);

  // calculate next stage or sub-stage increment
  if ((teleinfo_hass_sleep.stage == TIC_PUB_RELAY_DATA)  || (teleinfo_hass_sleep.stage == TIC_PUB_TOTAL_INDEX) ) teleinfo_hass_sleep.sub_stage++;
    else teleinfo_hass_sleep.stage++;
  if ((teleinfo_hass_sleep.stage == TIC_PUB_RELAY_DATA)  && (teleinfo_hass_sleep.sub_stage >= 8)               ) teleinfo_hass_sleep.stage++;
  if ((teleinfo_hass_sleep.stage == TIC_PUB_TOTAL_INDEX) && (teleinfo_hass_sleep.sub_stage >= teleinfo_contract_db.period_qty)) teleinfo_hass_sleep.stage++;
  if ((teleinfo_hass_sleep.stage != TIC_PUB_RELAY_DATA)  && (teleinfo_hass_sleep.stage != TIC_PUB_TOTAL_INDEX) ) teleinfo_hass_sleep.sub_stage = 0; 
  if (teleinfo_hass_sleep.stage == TIC_PUB_END) teleinfo_hass_sleep.stage = UINT8_MAX;
}

/*******************************\
 *          Callback
\*******************************/

void TeleinfoHomeAssistantInit ()
{
  // load config
  TeleinfoHomeAssistantLoadConfig ();

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Run hass <0/1> to disable/enable Home Assistant auto-discovery"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Run hass_publish to force auto_discovery publication"));
}

// trigger publication
void TeleinfoHomeAssistantEvery250ms ()
{
  // if already published, not connected or no message received, ignore 
  if (teleinfo_hass_sleep.stage == UINT8_MAX) return;
  if (!TeleinfoDriverWebAllow (TIC_WEB_MQTT)) return;

  // publish next home assistant integration message
  TeleinfoHomeAssistantPublishNextRetain ();
}

/***************************************\
 *           JSON publication
\***************************************/

void TeleinfoHomeAssistantPublish (const uint8_t stage, const uint8_t index)
{
  bool        publish = true;
  uint32_t    ip_address;
  const char* pstr_param = nullptr;
  char        str_text1[32];
  char        str_text2[128];
  char        str_name[64];
  char        str_id[32];
  char        str_key[32];
  char        str_icon[24];

  // check parameters
  if (stage >= TIC_PUB_END) return;
  if ((stage == TIC_PUB_RELAY_DATA) && (index >= 8)) return;
  if ((stage == TIC_PUB_TOTAL_INDEX) && (index >= TIC_PERIOD_MAX)) return;

  // init
  strcpy_P (str_text1, PSTR (""));
  strcpy_P (str_text2, PSTR (""));
  strcpy_P (str_name,  PSTR (""));
  strcpy_P (str_id,    PSTR (""));
  strcpy_P (str_key,   PSTR (""));
  strcpy_P (str_icon,  PSTR (""));

  // set parameter according to data
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
    case TIC_PUB_PROD_P:          pstr_param = HOMEASSISTANT_PUB_PROD_P;          break;
    case TIC_PUB_PROD_W:          pstr_param = HOMEASSISTANT_PUB_PROD_W;          break;
    case TIC_PUB_PROD_C:          pstr_param = HOMEASSISTANT_PUB_PROD_C;          break;
    case TIC_PUB_PROD_YTDAY:      pstr_param = HOMEASSISTANT_PUB_PROD_YTDAY;      break;
    case TIC_PUB_PROD_TODAY:      pstr_param = HOMEASSISTANT_PUB_PROD_TODAY;      break;

    // conso
    case TIC_PUB_CONSO_U:         pstr_param = HOMEASSISTANT_PUB_CONSO_U;         break;
    case TIC_PUB_CONSO_I:         pstr_param = HOMEASSISTANT_PUB_CONSO_I;         break;
    case TIC_PUB_CONSO_P:         pstr_param = HOMEASSISTANT_PUB_CONSO_P;         break;
    case TIC_PUB_CONSO_W:         pstr_param = HOMEASSISTANT_PUB_CONSO_W;         break;
    case TIC_PUB_CONSO_C:         pstr_param = HOMEASSISTANT_PUB_CONSO_C;         break;
    case TIC_PUB_CONSO_YTDAY:     pstr_param = HOMEASSISTANT_PUB_CONSO_YTDAY;     break;
    case TIC_PUB_CONSO_TODAY:     pstr_param = HOMEASSISTANT_PUB_CONSO_TODAY;     break;

    // 3 phases
    case TIC_PUB_PH1_U: pstr_param = HOMEASSISTANT_PUB_PH1_U; publish = (teleinfo_contract.phase > 1); break;
    case TIC_PUB_PH1_I: pstr_param = HOMEASSISTANT_PUB_PH1_I; publish = (teleinfo_contract.phase > 1); break;
    case TIC_PUB_PH1_P: pstr_param = HOMEASSISTANT_PUB_PH1_P; publish = (teleinfo_contract.phase > 1); break;
    case TIC_PUB_PH1_W: pstr_param = HOMEASSISTANT_PUB_PH1_W; publish = (teleinfo_contract.phase > 1); break;

    case TIC_PUB_PH2_U: pstr_param = HOMEASSISTANT_PUB_PH2_U; publish = (teleinfo_contract.phase > 1); break;
    case TIC_PUB_PH2_I: pstr_param = HOMEASSISTANT_PUB_PH2_I; publish = (teleinfo_contract.phase > 1); break;
    case TIC_PUB_PH2_P: pstr_param = HOMEASSISTANT_PUB_PH2_P; publish = (teleinfo_contract.phase > 1); break;
    case TIC_PUB_PH2_W: pstr_param = HOMEASSISTANT_PUB_PH2_W; publish = (teleinfo_contract.phase > 1); break;

    case TIC_PUB_PH3_U: pstr_param = HOMEASSISTANT_PUB_PH3_U; publish = (teleinfo_contract.phase > 1); break;
    case TIC_PUB_PH3_I: pstr_param = HOMEASSISTANT_PUB_PH3_I; publish = (teleinfo_contract.phase > 1); break;
    case TIC_PUB_PH3_P: pstr_param = HOMEASSISTANT_PUB_PH3_P; publish = (teleinfo_contract.phase > 1); break;
    case TIC_PUB_PH3_W: pstr_param = HOMEASSISTANT_PUB_PH3_W; publish = (teleinfo_contract.phase > 1); break;

    // relay
    case TIC_PUB_RELAY_DATA:
      pstr_param = HOMEASSISTANT_PUB_RELAY_DATA;
      sprintf_P (str_name, GetTextIndexed (str_text1, sizeof (str_text1), 0, pstr_param), index + 1);
      sprintf_P (str_id,   GetTextIndexed (str_text1, sizeof (str_text1), 1, pstr_param), index + 1);
      sprintf_P (str_key,  GetTextIndexed (str_text1, sizeof (str_text1), 2, pstr_param), index + 1);
      publish = teleinfo_config.relay;
      break;

    // production counter
    case TIC_PUB_TOTAL_PROD:  pstr_param = HOMEASSISTANT_PUB_TOTAL_PROD;  break;

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

    // alert
    case TIC_PUB_ALERT_VOLT:  pstr_param = HOMEASSISTANT_PUB_ALERT_VOLT; break;
    case TIC_PUB_ALERT_LOAD:  pstr_param = HOMEASSISTANT_PUB_ALERT_LOAD; break;

#ifdef USE_TELEINFO_SOLAR
    // solar
    case TIC_PUB_SOLAR_W:        pstr_param = HOMEASSISTANT_PUB_SOLAR_W;        publish = teleinfo_solar.enabled;    break;
    case TIC_PUB_SOLAR_TOTAL:    pstr_param = HOMEASSISTANT_PUB_SOLAR_TOTAL;    publish = teleinfo_solar.enabled;    break;

    // forecast
    case TIC_PUB_FORECAST_W:     pstr_param = HOMEASSISTANT_PUB_FORECAST_W;     publish = teleinfo_forecast.enabled; break;
    case TIC_PUB_FORECAST_TODAY: pstr_param = HOMEASSISTANT_PUB_FORECAST_TODAY; publish = teleinfo_forecast.enabled; break;
    case TIC_PUB_FORECAST_TOMRW: pstr_param = HOMEASSISTANT_PUB_FORECAST_TOMRW; publish = teleinfo_forecast.enabled; break;
#endif  // USE_TELEINFO_SOLAR

#ifdef USE_TELEINFO_RTE
    // RTE
    case TIC_PUB_RTE_TEMPO_TODAY:  pstr_param = HOMEASSISTANT_PUB_RTE_TEMPO_TODAY;  publish = TeleinfoRteTempoIsEnabled (); break;
    case TIC_PUB_RTE_TEMPO_TOMRW:  pstr_param = HOMEASSISTANT_PUB_RTE_TEMPO_TOMRW;  publish = TeleinfoRteTempoIsEnabled (); break;
    case TIC_PUB_RTE_TEMPO_DAY2:   pstr_param = HOMEASSISTANT_PUB_RTE_TEMPO_DAY2;   publish = TeleinfoRteTempoIsEnabled (); break;
    case TIC_PUB_RTE_TEMPO_DAY3:   pstr_param = HOMEASSISTANT_PUB_RTE_TEMPO_DAY3;   publish = TeleinfoRteTempoIsEnabled (); break;
    case TIC_PUB_RTE_TEMPO_DAY4:   pstr_param = HOMEASSISTANT_PUB_RTE_TEMPO_DAY4;   publish = TeleinfoRteTempoIsEnabled (); break;
    case TIC_PUB_RTE_TEMPO_DAY5:   pstr_param = HOMEASSISTANT_PUB_RTE_TEMPO_DAY5;   publish = TeleinfoRteTempoIsEnabled (); break;
    case TIC_PUB_RTE_TEMPO_DAY6:   pstr_param = HOMEASSISTANT_PUB_RTE_TEMPO_DAY6;   publish = TeleinfoRteTempoIsEnabled (); break;
    case TIC_PUB_RTE_POINTE_TODAY: pstr_param = HOMEASSISTANT_PUB_RTE_POINTE_TODAY; publish = TeleinfoRteTempoIsEnabled (); break;
    case TIC_PUB_RTE_POINTE_TOMRW: pstr_param = HOMEASSISTANT_PUB_RTE_POINTE_TOMRW; publish = TeleinfoRteTempoIsEnabled (); break;
#endif  // USE_TELEINFO_RTE
  }

  // if data should be published
  if (pstr_param != nullptr)
  {
    // set publish flag
    publish &= teleinfo_hass.enabled;

    // get auto-discovery retained message data
    if (strlen (str_name) == 0) GetTextIndexed (str_name, sizeof (str_name), 0, pstr_param);
    if (strlen (str_id) == 0)   GetTextIndexed (str_id,   sizeof (str_id),   1, pstr_param);
    if (strlen (str_key) == 0)  GetTextIndexed (str_key,  sizeof (str_key),  2, pstr_param);
    GetTextIndexed (str_icon, sizeof (str_icon), 3, pstr_param);
    GetTopic_P (str_text2, TELE, TasmotaGlobal.mqtt_topic, D_RSLT_SENSOR);

    // if data should be published
    if (publish)
    {
      // set response
      Response_P (PSTR ("{\"name\":\"%s\",\"state_topic\":\"%s\",\"unique_id\":\"%s_%s\",\"value_template\":\"{{value_json%s}}\",\"icon\":\"mdi:%s\""), str_name, str_text2, NetworkUniqueId().c_str(), str_id, str_key, str_icon);

      // if first call, append device description
      if (teleinfo_hass_sleep.stage != TIC_PUB_CONTRACT_NAME) ResponseAppend_P (PSTR (",\"device\":{\"identifiers\":[\"%s\"]}"), NetworkUniqueId().c_str());
      else
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

      // if defined, append specific values
      GetTextIndexed (str_text1, sizeof (str_text1), 4, pstr_param);
      if (strlen (str_text1) > 0) ResponseAppend_P (PSTR (",\"device_class\":\"%s\""), str_text1);
      GetTextIndexed (str_text1, sizeof (str_text1), 5, pstr_param);
      if (strlen (str_text1) > 0) ResponseAppend_P (PSTR (",\"state_class\":\"%s\""), str_text1);
      GetTextIndexed (str_text1, sizeof (str_text1), 6, pstr_param);
      if (strlen (str_text1) > 0) ResponseAppend_P (PSTR (",\"unit_of_measurement\":\"%s\""), str_text1);
      ResponseAppend_P (PSTR ("}"));
    }

    // else empty response
    else Response_P (PSTR (""));

    // generate sensor topic
    strcpy_P (str_text2, PSTR ("homeassistant/sensor/"));
    strlcat  (str_text2, NetworkUniqueId ().c_str (), sizeof (str_text2));
    strcat_P (str_text2, PSTR ("_"));
    strlcat  (str_text2, str_id, sizeof (str_text2));
    strcat_P (str_text2, PSTR ("/config"));

    // publish sensor topic
    MqttPublish (str_text2, true);

    // declare publication
    TeleinfoDriverWebDeclare (TIC_WEB_MQTT);
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

#endif      // USE_TELEINFO_HASS

#endif      // USE_TELEINFO

