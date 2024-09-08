/*
  xdrv_116_integration_domoticz.ino - Domoticz integration for Teleinfo

  Copyright (C) 2024  Nicolas Bernaerts

  Version history :
    07/02/2024 - v1.0 - Creation
    14/04/2024 - v1.1 - Switch to rf_code configuration
    16/07/2024 - v1.2 - Add global power, current and voltage

  Configuration values are stored in :
    - Settings->rf_code[16][2]  : Flag en enable/disable integration

    - Settings->domoticz_sensor_idx[0]  : totals HC/HP & conso energy for Base, EJP Normal, Tempo bleu
    - Settings->domoticz_sensor_idx[1]  : totals HC/HP & conso energyfor EJP Pointe, Tempo blanc
    - Settings->domoticz_sensor_idx[2]  : totals HC/HP & conso energyfor Tempo rouge
    - Settings->domoticz_sensor_idx[8]  : total & prod energy
    - Settings->domoticz_sensor_idx[9]  : current hc/hp
    - Settings->domoticz_sensor_idx[10] : today's level (1,2 or 3)
    - Settings->domoticz_sensor_idx[11] : tomorrow's level (1,2 or 3)

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
#ifdef USE_TELEINFO_DOMOTICZ

// declare teleinfo domoticz integration
#define XDRV_116              116

/*******************************************\
 *               Variables
\*******************************************/

#define TELEINFO_DOMOTICZ_NB_MESSAGE    5

// Data type
enum DomoticzDataType { DOMO_DATA_METER_BLUE, DOMO_DATA_METER_WHITE, DOMO_DATA_METER_RED, 
                        DOMO_DATA_POWER_CONSO, DOMO_DATA_CURRENT,
                        DOMO_DATA_VOLTAGE1, DOMO_DATA_VOLTAGE2, DOMO_DATA_VOLTAGE3,
                        DOMO_DATA_METER_PROD, 
                        DOMO_DATA_ALERT_HCHP, DOMO_DATA_ALERT_TODAY, DOMO_DATA_ALERT_TOMORROW,
                        DOMO_DATA_12, DOMO_DATA_13, DOMO_DATA_14, DOMO_DATA_15, DOMO_DATA_MAX };

const char kDomoticzIntegrationLabels[] PROGMEM = 
                        "Totaux conso. Bleu" "|" 
                        "Totaux conso. Blanc" "|" 
                        "Totaux conso. Rouge" "|"
                        "Total global conso." "|" 
                        "Courant (3 phases)" "|" 
                        "Tension (phase 1)" "|" 
                        "Tension (phase 2)" "|" 
                        "Tension (phase 3)" "|"
                        "Total Production" "|"
                        "Heure Pleine /Creuse" "|" 
                        "Couleur du Jour" "|" 
                        "Couleur du Lendemain" "|"
                        "12" "|" "13" "|" "14" "|" "15";

const char kDomoticzIntegrationTitles[] PROGMEM = 
                        "[P1SmartMeter] Totaux HC/HP pour contrat Base, EJP, HC/HP, Tempo bleu, ..." "|"
                        "[P1SmartMeter] Totaux HC/HP pour période Tempo Blanc" "|"
                        "[P1SmartMeter] Totaux HC/HP pour période Tempo Rouge" "|"
                        "[Instant & counter] Total global de consommation" "|" 
                        "[Current] Courant monophasé ou sur chacune des 3 phases" "|" 
                        "[Voltage] Tension monophasée ou phase 1" "|" 
                        "[Voltage] Tension phase 2" "|" 
                        "[Voltage] Tension phase 3" "|"
                        "[P1SmartMeter] Total global de production" "|"
                        "[Alert] Status HC/HP (0:Heure Pleine, 1:Heure creuse)" "|"
                        "[Alert] Couleur du Jour (0:Inconnue, 1:Bleu, 2:Blanc, 4:Rouge)" "|" 
                        "[Alert] Couleur du Lendemain (0:Inconnue, 1:Bleu, 2:Blanc, 4:Rouge)" "|"
                        "Non utilisé" "|" "Non utilisé" "|" "Non utilisé" "|" "Non utilisé";

// Commands
const char kDomoticzIntegrationCommands[] PROGMEM = "domo" "|"  "|" "_set" "|" "_key";
void (* const DomoticzIntegrationCommand[])(void) PROGMEM = { &CmndDomoticzIntegrationHelp, &CmndDomoticzIntegrationEnable, &CmndDomoticzIntegrationSetKey };

/**************************************\
 *               Data
\**************************************/

static struct {
  uint8_t enabled = 0;
  uint8_t index   = UINT8_MAX;              // reset publication flag
} domoticz_integration;

/**********************************************\
 *                Configuration
\**********************************************/

// load configuration
void DomoticzIntegrationLoadConfig () 
{
  domoticz_integration.enabled = Settings->rf_code[16][2];
}

// save configuration
void DomoticzIntegrationSaveConfig () 
{
  Settings->rf_code[16][2] = domoticz_integration.enabled;
}

/***************************************\
 *               Function
\***************************************/

// set integration
void DomoticzIntegrationSet (const bool enabled) 
{
  // update status
  domoticz_integration.enabled = enabled;

  // reset publication flags
  domoticz_integration.index = 0; 

  // save configuration
  DomoticzIntegrationSaveConfig ();
}

// get integration
bool DomoticzIntegrationGet () 
{
  return (domoticz_integration.enabled == 1);
}

// set Domoticz key
void DomoticzIntegrationSetKey (const uint8_t index, const uint16_t value)
{
  // key stored in SENSOR IDX
  if (index < MAX_DOMOTICZ_SNS_IDX) Settings->domoticz_sensor_idx[index] = value;

  // key stored in SWITCH IDX
  else if (index < MAX_DOMOTICZ_SNS_IDX + MAX_DOMOTICZ_IDX) Settings->domoticz_switch_idx[index - MAX_DOMOTICZ_SNS_IDX] = value;
}

// get Domoticz key
uint16_t DomoticzIntegrationGetKey () { return DomoticzIntegrationGetKey (domoticz_integration.index); }
uint16_t DomoticzIntegrationGetKey (const uint8_t index)
{
  uint16_t value = 0;

  // key stored in SENSOR IDX
  if (index < MAX_DOMOTICZ_SNS_IDX) value = Settings->domoticz_sensor_idx[index];

  // key stored in SWITCH IDX
  else if (index < MAX_DOMOTICZ_SNS_IDX + MAX_DOMOTICZ_IDX) value = Settings->domoticz_switch_idx[index - MAX_DOMOTICZ_SNS_IDX];

  return value;
}

/****************************\
 *        Commands
\****************************/

// Domoticz interation help
void CmndDomoticzIntegrationHelp ()
{
  uint8_t index;
  char    str_text[48];

  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: commands for Teleinfo Domoticz integration"));
  AddLog (LOG_LEVEL_INFO, PSTR ("  domo_set <%u>      = activation/desactivation de l'integration"), domoticz_integration.enabled);
  AddLog (LOG_LEVEL_INFO, PSTR ("  domo_key <num,idx> = index idx pour la clé num"));
  for (index = 0; index < DOMO_DATA_12; index ++) AddLog (LOG_LEVEL_INFO, PSTR ("    <%u,%u>  : %s"), index, DomoticzIntegrationGetKey (index), GetTextIndexed (str_text, sizeof (str_text), index, kDomoticzIntegrationLabels));

  ResponseCmndDone ();
}

// Enable Domoticz integration
void CmndDomoticzIntegrationEnable ()
{
  if (XdrvMailbox.data_len > 0) DomoticzIntegrationSet (XdrvMailbox.payload != 0); 
  ResponseCmndNumber (domoticz_integration.enabled);
}

// set Domoticz key
void CmndDomoticzIntegrationSetKey ()
{
  uint8_t  index = UINT8_MAX;
  uint16_t value = 0;
  char    *pstr_value;
  char     str_data[64];

  if (XdrvMailbox.data_len > 0)
  {
    // look for separator
    strlcpy (str_data, XdrvMailbox.data, sizeof (str_data));
    pstr_value = strchr (str_data, ',');
    if (pstr_value != nullptr) *pstr_value++ = 0;

    // if separator was found, update value
    index = (uint8_t)atoi (str_data);
    if (pstr_value != nullptr) DomoticzIntegrationSetKey (index, (uint16_t)atoi (pstr_value));
  }

  // answer
  if (index < DOMO_DATA_MAX)
  {
    value = DomoticzIntegrationGetKey (index);
    if (value > 0) snprintf_P (str_data, sizeof (str_data), PSTR ("Domoticz sensor %u enabled (idx %u)"), index, value);
      else snprintf_P (str_data, sizeof (str_data), PSTR ("Domoticz sensor %u disabled"), index);
    ResponseCmndChar_P (str_data);
  }
  else ResponseCmndFailed ();
}

/****************************\
 *        Callback
\****************************/

// Domoticz init message
void DomoticzIntegrationInit ()
{
  // load config
  DomoticzIntegrationLoadConfig ();
  
  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Run domo to get help on Domoticz integration"));
}

// called 10 times per second
void DomoticzIntegrationEvery250ms ()
{
  // check if enabled
  if (!domoticz_integration.enabled) return;
  if (!RtcTime.valid) return;
  if (!MqttIsConnected ()) return;

  // publish if needed
  if (domoticz_integration.index < DOMO_DATA_MAX) DomoticzIntegrationPublish ();
}

/***************************************\
 *           JSON publication
\***************************************/

// trigger publication
void DomoticzIntegrationPublishTrigger ()
{
  domoticz_integration.index = 0;
}

// publish next pending data
uint8_t DomoticzIntegrationPublish ()
{
  uint8_t  index;
  uint16_t sns_index, nvalue;
  int      rssi, wifi;
  long     value;
  char     str_total_1[16];
  char     str_total_2[16];
  char     str_svalue[64];

  // check for current period 
  if (!domoticz_integration.enabled) return DOMO_DATA_MAX;
  if (teleinfo_contract.period_idx == UINT8_MAX) return DOMO_DATA_MAX;

  // check if there is something to publish
  if (domoticz_integration.index >= DOMO_DATA_MAX) return DOMO_DATA_MAX;

  // publish next topic
  while ((domoticz_integration.index < DOMO_DATA_MAX) && (DomoticzIntegrationGetKey () == 0)) domoticz_integration.index++;
  if (domoticz_integration.index < DOMO_DATA_MAX)
  {
    // init
    nvalue = 0;
    strcpy (str_svalue,  "" );
    strcpy_P (str_total_1, PSTR ("0"));
    strcpy_P (str_total_2, PSTR ("0"));
    sns_index = DomoticzIntegrationGetKey ();

    // set data according to index
    switch (domoticz_integration.index)
    {
      // conso (hc/hp for first 3 periods)
      case DOMO_DATA_METER_BLUE:
      case DOMO_DATA_METER_WHITE:
      case DOMO_DATA_METER_RED:
        value = 0;
        index = domoticz_integration.index * 2;
        lltoa (teleinfo_conso.index_wh[index], str_total_1, 10);
        lltoa (teleinfo_conso.index_wh[index + 1], str_total_2, 10);
        if ((teleinfo_contract.period_idx == index) || (teleinfo_contract.period_idx == index + 1)) value = teleinfo_conso.pact;
        sprintf_P (str_svalue, PSTR ("%s;%s;0;0;%d;0"), str_total_1, str_total_2, value);
        break;

      // total power
      case DOMO_DATA_POWER_CONSO:
        lltoa (teleinfo_conso.total_wh, str_total_1, 10);
        sprintf_P (str_svalue, PSTR ("%d;%s"), teleinfo_conso.phase[0].pact, str_total_1);
        break;

      // current on 3 phases
      case DOMO_DATA_CURRENT:
        sprintf_P (str_svalue, PSTR ("%d.%d;%d.%d;%d.%d"), teleinfo_conso.phase[0].current / 1000, teleinfo_conso.phase[0].current % 1000, teleinfo_conso.phase[1].current / 1000, teleinfo_conso.phase[1].current % 1000, teleinfo_conso.phase[2].current / 1000, teleinfo_conso.phase[2].current % 1000);
        break;

      // voltage phase 1
      case DOMO_DATA_VOLTAGE1:
        sprintf_P (str_svalue, PSTR ("%d"), teleinfo_conso.phase[0].voltage);
        break;

      // voltage phase 2
      case DOMO_DATA_VOLTAGE2:
        sprintf_P (str_svalue, PSTR ("%d"), teleinfo_conso.phase[1].voltage);
        break;
      
      // voltage phase 3
      case DOMO_DATA_VOLTAGE3:
        sprintf_P (str_svalue, PSTR ("%d"), teleinfo_conso.phase[2].voltage);
        break;

      // prod
      case DOMO_DATA_METER_PROD:
        lltoa (teleinfo_prod.total_wh, str_total_1, 10);
        sprintf_P (str_svalue, PSTR ("%s;0;0;0;%d;0"), str_total_1, teleinfo_prod.pact);
        break;

      // current hc / hp
      case DOMO_DATA_ALERT_HCHP:
        nvalue = 1;
        if (TeleinfoPeriodGetHP ()) nvalue = 0;
        if (nvalue == 0) strcpy_P (str_svalue, PSTR ("Heure Pleine")); 
          else strcpy_P (str_svalue, PSTR ("Heure Creuse"));
        break;

      // color today
      case DOMO_DATA_ALERT_TODAY:
        nvalue = TeleinfoPeriodGetLevel ();
        GetTextIndexed (str_svalue, sizeof (str_svalue), nvalue, kTeleinfoPeriodLabel);
        if (nvalue == 3) nvalue = 4;
        break;

      // color tomorrow
      case DOMO_DATA_ALERT_TOMORROW:
        nvalue = TeleinfoDriverCalendarGetLevel (TIC_DAY_TOMORROW, 12, true);
        GetTextIndexed (str_svalue, sizeof (str_svalue), nvalue, kTeleinfoPeriodLabel);
        if (nvalue == 3) nvalue = 4;
        break;
    }

    // if some data should be published;
    if (strlen (str_svalue) > 0)
    {
      // get wifi RSSI data
      rssi = WiFi.RSSI ();
      wifi = WifiGetRssiAsQuality (rssi);

      // publish data
      ResponseClear ();
      ResponseAppend_P (PSTR ("{\"idx\":%u,\"nvalue\":%d,\"svalue\":\"%s\",\"Battery\":%u,\"RSSI\":%d}"), sns_index, nvalue, str_svalue, 100, wifi / 10);

      // publish message
      MqttPublishPayload (DOMOTICZ_IN_TOPIC, ResponseData());
    }
  }

  // increase next index
  domoticz_integration.index++;

  return domoticz_integration.index;
}

// publish next pending data
void DomoticzIntegrationPublishAll ()
{
  uint8_t result = 0;

  while (result < DOMO_DATA_MAX) result = DomoticzIntegrationPublish ();
}

/***************************************\
 *              Web
\***************************************/

#ifdef USE_WEBSERVER

void DomoticzIntegrationDisplayParameters ()
{
  uint8_t index;
  char    str_text[32];
  char    str_title[64];

  // loop to display indexes
  for (index = 0; index < DOMO_DATA_12; index ++)
  {
    GetTextIndexed (str_text,  sizeof (str_text),  index, kDomoticzIntegrationLabels);
    GetTextIndexed (str_title, sizeof (str_title), index, kDomoticzIntegrationTitles);
    WSContentSend_P (PSTR ("<p class='dat' title='%s'><span class='domo'>%s</span><span class='val'><input class='idx' type='number' name='idx%u' min='0' max='65535' step='1' value='%u'></span></p>\n"),  str_title, str_text, index, DomoticzIntegrationGetKey (index));
  }
}

void DomoticzIntegrationRetrieveParameters ()
{
  uint8_t index;
  char    str_param[8];
  char    str_value[8];

  for (index = 0; index < DOMO_DATA_12; index ++)
  {
    sprintf_P (str_param, PSTR ("idx%u"), index);
    WebGetArg (str_param, str_value, sizeof (str_value));
    DomoticzIntegrationSetKey (index, (uint16_t) atoi (str_value));
  }
}

#endif    // USE_WEBSERVER

/***************************************\
 *              Interface
\***************************************/

// Teleinfo Domoticz integration
bool Xdrv116 (uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
    case FUNC_INIT:
      DomoticzIntegrationInit ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kDomoticzIntegrationCommands, DomoticzIntegrationCommand);
      break;
   case FUNC_EVERY_250_MSECOND:
      DomoticzIntegrationEvery250ms ();
      break;
  }

  return result;
}

#endif      // USE_TELEINFO_DOMOTICZ
#endif      // USE_TELEINFO

