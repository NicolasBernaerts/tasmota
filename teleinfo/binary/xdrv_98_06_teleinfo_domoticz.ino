/*
  xdrv_98_06_teleinfo_domoticz.ino - Domoticz integration for Teleinfo

  Copyright (C) 2024-2025  Nicolas Bernaerts

  Version history :
    07/02/2024 v1.0 - Creation
    14/04/2024 v1.1 - Switch to rf_code configuration
    16/07/2024 v1.2 - Add global power, current and voltage
    24/09/2024 v1.3 - Add VA publication flag
    14/10/2024 v1.4 - Change conso total to P1SmartMeter
    03/07/2025 v1.5 - Rename to xsns_126_teleinfo_domoticz.ino
    10/07/2025 v2.0 - Refactoring based on Tasmota 15

  Configuration values are stored in :

    - Settings->rf_code[16][2]              : Flag to enable/disable integration
    - Settings->rf_code[16][4]              : Flag to publish VA instead of W

    - Settings->domoticz_sensor_idx[0..11]
      Settings->domoticz_switch_idx[0..3]   : Domoticz indexes

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

// declare teleinfo domoticz integration
//#define XDRV_116              116

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

const char kTeleinfoDomoticzLabels[] PROGMEM = 
                        "Conso. Bleu (Wh/W)" "|" 
                        "Conso. Blanc (Wh/W)" "|" 
                        "Conso. Rouge (Wh/W)" "|"
                        "Conso générale (Wh/W)" "|" 
                        "Courant (3 phases)" "|" 
                        "Tension (phase 1)" "|" 
                        "Tension (phase 2)" "|" 
                        "Tension (phase 3)" "|"
                        "Production (Wh/W)" "|"
                        "Heure Pleine /Creuse" "|" 
                        "Couleur du Jour" "|" 
                        "Couleur du Lendemain" "|"
                        "12" "|" 
                        "13" "|" 
                        "14" "|" 
                        "15";

const char kTeleinfoDomoticzTitles[] PROGMEM = 
                        "[P1SmartMeter] Total HC/HP + conso. instantanée (Base, EJP, HC/HP, Tempo bleu, ...)" "|"
                        "[P1SmartMeter] Total HC/HP + conso. instantanée (Tempo Blanc)" "|"
                        "[P1SmartMeter] Total HC/HP + conso. instantanée (Tempo Rouge)" "|"
                        "[P1SmartMeter] Total général + conso. instnatanée" "|" 
                        "[Current] Courant monophasé ou sur chacune des 3 phases" "|" 
                        "[Voltage] Tension phase 1" "|" 
                        "[Voltage] Tension phase 2" "|" 
                        "[Voltage] Tension phase 3" "|"
                        "[P1SmartMeter] Total + production instantanée" "|"
                        "[Alert] HC/HP actuel (0:Heure Pleine, 1:Heure creuse)" "|"
                        "[Alert] Couleur du Jour (0:Inconnue, 1:Bleu, 2:Blanc, 4:Rouge)" "|" 
                        "[Alert] Couleur du Lendemain (0:Inconnue, 1:Bleu, 2:Blanc, 4:Rouge)" "|"
                        "Non utilisé" "|"
                        "Non utilisé" "|"
                        "Non utilisé" "|"
                        "Non utilisé";

// Commands
const char kTeleinfoDomoticzCommands[]         PROGMEM = "domo" "|"                "|"          "_set"          "|"            "_va"        "|"          "_key";
void (* const TeleinfoDomoticzCommand[])(void) PROGMEM = { &CmndTeleinfoDomoticzHelp, &CmndTeleinfoDomoticzEnable, &CmndTeleinfoDomoticzUseVA, &CmndTeleinfoDomoticzSetKey };

/**************************************\
 *               Data
\**************************************/

static struct {
  uint8_t enabled = 0;                  // flag to enable integration
  uint8_t use_va  = 0;                  // flag to publish VA instead of W
  uint8_t index   = UINT8_MAX;          // next index to be published
} teleinfo_domoticz;

/**********************************************\
 *                Configuration
\**********************************************/

// load configuration
void TeleinfoDomoticzLoadConfig () 
{
  teleinfo_domoticz.enabled = Settings->rf_code[16][2];
  teleinfo_domoticz.use_va  = Settings->rf_code[16][4];
}

// save configuration
void TeleinfoDomoticzSaveConfig () 
{
  Settings->rf_code[16][2] = teleinfo_domoticz.enabled;
  Settings->rf_code[16][4] = teleinfo_domoticz.use_va;
}

/***************************************\
 *               Function
\***************************************/

// set integration
void TeleinfoDomoticzSet (const bool enabled) 
{
  // update status
  teleinfo_domoticz.enabled = enabled;

  // reset publication flags
  teleinfo_domoticz.index = 0; 

  // save configuration
  TeleinfoDomoticzSaveConfig ();
}

// get integration
bool TeleinfoDomoticzGet () 
{
  return (teleinfo_domoticz.enabled == 1);
}

// set Domoticz key
void TeleinfoDomoticzSetKey (const uint8_t index, const uint16_t value)
{
  // key stored in SENSOR IDX
  if (index < MAX_DOMOTICZ_SNS_IDX) Settings->domoticz_sensor_idx[index] = value;

  // key stored in SWITCH IDX
  else if (index < MAX_DOMOTICZ_SNS_IDX + MAX_DOMOTICZ_IDX) Settings->domoticz_switch_idx[index - MAX_DOMOTICZ_SNS_IDX] = value;
}

// get Domoticz key
uint16_t TeleinfoDomoticzGetKey () { return TeleinfoDomoticzGetKey (teleinfo_domoticz.index); }
uint16_t TeleinfoDomoticzGetKey (const uint8_t index)
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

// Domoticz integration help
void CmndTeleinfoDomoticzHelp ()
{
  uint8_t index;
  char    str_text[48];

  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: commands for Teleinfo Domoticz integration"));
  AddLog (LOG_LEVEL_INFO, PSTR ("  domo_set <0/1>     = activation de l'integration [%u]"), teleinfo_domoticz.enabled);
  AddLog (LOG_LEVEL_INFO, PSTR ("  domo_va <0/1>      = puissances en VA plutot que W [%u]"), teleinfo_domoticz.use_va);
  AddLog (LOG_LEVEL_INFO, PSTR ("  domo_key <num,idx> = index idx pour la clé num"));
  for (index = 0; index < DOMO_DATA_12; index ++) AddLog (LOG_LEVEL_INFO, PSTR ("    <%u,%u>  : %s"), index, TeleinfoDomoticzGetKey (index), GetTextIndexed (str_text, sizeof (str_text), index, kTeleinfoDomoticzLabels));

  ResponseCmndDone ();
}

// Enable Domoticz integration
void CmndTeleinfoDomoticzEnable ()
{
  if (XdrvMailbox.data_len > 0) TeleinfoDomoticzSet (XdrvMailbox.payload != 0); 
  ResponseCmndNumber (teleinfo_domoticz.enabled);
}

// Enable Domoticz integration
void CmndTeleinfoDomoticzUseVA ()
{
  if (XdrvMailbox.data_len > 0) teleinfo_domoticz.use_va = (uint8_t)XdrvMailbox.payload; 
  ResponseCmndNumber (teleinfo_domoticz.use_va);
}

// set Domoticz key
void CmndTeleinfoDomoticzSetKey ()
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
    if (pstr_value != nullptr) TeleinfoDomoticzSetKey (index, (uint16_t)atoi (pstr_value));
  }

  // answer
  if (index < DOMO_DATA_MAX)
  {
    value = TeleinfoDomoticzGetKey (index);
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
void TeleinfoDomoticzInit ()
{
  // load config
  TeleinfoDomoticzLoadConfig ();
  
  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Run domo to get help on Domoticz integration"));
}

// called 10 times per second
void TeleinfoDomoticzEvery250ms ()
{
  // check if enabled
  if (!teleinfo_domoticz.enabled) return;
  if (!RtcTime.valid) return;
  if (!MqttIsConnected ()) return;

  // publish if needed
  if (teleinfo_domoticz.index < DOMO_DATA_MAX) TeleinfoDomoticzPublishNext ();
}

/***************************************\
 *           JSON publication
\***************************************/

// trigger publication
void TeleinfoDomoticzData ()
{
  teleinfo_domoticz.index = 0;
}

// publish next pending data
uint8_t TeleinfoDomoticzPublishNext ()
{
  uint8_t  index;
  uint16_t sns_index, nvalue;
  int      wifi;
  long     value;
  char     str_total_1[16];
  char     str_total_2[16];
  char     str_svalue[64];

  // check for current period 
  if (!teleinfo_domoticz.enabled) return DOMO_DATA_MAX;
  if (teleinfo_contract.period == UINT8_MAX) return DOMO_DATA_MAX;

  // check if there is something to publish
  if (teleinfo_domoticz.index >= DOMO_DATA_MAX) return DOMO_DATA_MAX;

  // publish next topic
  while ((teleinfo_domoticz.index < DOMO_DATA_MAX) && (TeleinfoDomoticzGetKey () == 0)) teleinfo_domoticz.index++;
  if (teleinfo_domoticz.index < DOMO_DATA_MAX)
  {
    // init
    value = 0;
    nvalue = 0;
    strcpy (str_svalue,  "" );
    strcpy_P (str_total_1, PSTR ("0"));
    strcpy_P (str_total_2, PSTR ("0"));
    sns_index = TeleinfoDomoticzGetKey ();

    // set data according to index
    switch (teleinfo_domoticz.index)
    {
      // conso (hc/hp for first 3 periods)
      case DOMO_DATA_METER_BLUE:
      case DOMO_DATA_METER_WHITE:
      case DOMO_DATA_METER_RED:
        index = teleinfo_domoticz.index * 2;
        lltoa (teleinfo_conso.index_wh[index], str_total_1, 10);
        lltoa (teleinfo_conso.index_wh[index + 1], str_total_2, 10);
        if ((teleinfo_contract.period == index) || (teleinfo_contract.period == index + 1))
        {
          if (teleinfo_domoticz.use_va) value = teleinfo_conso.papp;
            else value = teleinfo_conso.pact;
        }
        sprintf_P (str_svalue, PSTR ("%s;%s;0;0;%d;0"), str_total_1, str_total_2, value);
        break;

      // total power
      case DOMO_DATA_POWER_CONSO:
        lltoa (teleinfo_conso.total_wh, str_total_1, 10);
        if (teleinfo_domoticz.use_va) value = teleinfo_conso.papp;
          else value = teleinfo_conso.pact;
        sprintf_P (str_svalue, PSTR ("%s;%s;0;0;%d;0"), str_total_1, str_total_2, value);
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
        if (teleinfo_domoticz.use_va) value = teleinfo_prod.papp;
          else value = teleinfo_prod.pact;
        sprintf_P (str_svalue, PSTR ("%s;%s;0;0;%d;0"), str_total_1, str_total_2, value);
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
        nvalue = TeleinfoDriverCalendarGetLevel (TIC_DAY_TODAY, 12 * 2, true);
        GetTextIndexed (str_svalue, sizeof (str_svalue), nvalue, kTeleinfoLevelLabel);
        if (nvalue == 3) nvalue = 4;
        break;

      // color tomorrow
      case DOMO_DATA_ALERT_TOMORROW:
        nvalue = TeleinfoDriverCalendarGetLevel (TIC_DAY_TMROW, 12 * 2, true);
        GetTextIndexed (str_svalue, sizeof (str_svalue), nvalue, kTeleinfoLevelLabel);
        if (nvalue == 3) nvalue = 4;
        break;
    }

    // if some data should be published;
    if (strlen (str_svalue) > 0)
    {
      // get wifi RSSI data
      wifi = WifiGetRssiAsQuality (WiFi.RSSI ());

      // publish data
      ResponseClear ();
      ResponseAppend_P (PSTR ("{\"idx\":%u,\"nvalue\":%d,\"svalue\":\"%s\",\"Battery\":%u,\"RSSI\":%d}"), sns_index, nvalue, str_svalue, 100, wifi / 10);

      // publish message
      MqttPublishPayload (DOMOTICZ_IN_TOPIC, ResponseData());
    }
  }

  // increase next index
  teleinfo_domoticz.index++;

  return teleinfo_domoticz.index;
}

// publish next pending data
void TeleinfoDomoticzPublishAll ()
{
  uint8_t result = 0;

  // if not enabled, ignore
  if (!teleinfo_domoticz.enabled) return;

  // loop to publish all data
  while (result < DOMO_DATA_MAX) result = TeleinfoDomoticzPublishNext ();
}

/***************************************\
 *              Web
\***************************************/

#ifdef USE_WEBSERVER

void TeleinfoDomoticzDisplayParameters ()
{
  uint8_t     index;
  char        str_text[32];
  char        str_title[92];
  const char *pstr_title;

  // section start
  WSContentSend_P (PSTR ("<fieldset class='config'>\n"));

  // loop to display indexes
  for (index = 0; index < DOMO_DATA_12; index ++)
  {
    GetTextIndexed (str_text,  sizeof (str_text),  index, kTeleinfoDomoticzLabels);
    GetTextIndexed (str_title, sizeof (str_title), index, kTeleinfoDomoticzTitles);
    WSContentSend_P (PSTR ("<p class='dat' title='%s'><span class='hval'>%s</span><span class='val'><input type='number' name='idx%u' min='0' max='65535' step='1' value='%u'></span></p>\n"),  str_title, str_text, index, TeleinfoDomoticzGetKey (index));
  }

  // parameter 'useva' : power in VA or W
  if (teleinfo_domoticz.use_va) strcpy_P (str_text, PSTR ("checked")); else str_text[0] = 0;
  pstr_title = PSTR ("Pulication des puissances apparentes (VA) au lieu des puissances actives (W)");
  WSContentSend_P (PSTR ("<p class='dat' title='%s'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), pstr_title, PSTR ("useva"), str_text, PSTR ("useva"), PSTR ("Puissances en VA"));

  // section end
  WSContentSend_P (PSTR ("</fieldset>\n"));
}

void TeleinfoDomoticzRetrieveParameters ()
{
  uint8_t index;
  char    str_param[8];
  char    str_value[8];

  // indexes
  for (index = 0; index < DOMO_DATA_12; index ++)
  {
    sprintf_P (str_param, PSTR ("idx%u"), index);
    WebGetArg (str_param, str_value, sizeof (str_value));
    TeleinfoDomoticzSetKey (index, (uint16_t) atoi (str_value));
  }

  // parameter 'useva'
  WebGetArg (PSTR ("useva"), str_param, sizeof (str_param));
  if (strlen (str_param) > 0) teleinfo_domoticz.use_va = 1;
    else teleinfo_domoticz.use_va = 0;
}

#endif    // USE_WEBSERVER


/***************************************\
 *              Interface
\***************************************/

bool XdrvTeleinfoDomoticz (const uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
    case FUNC_COMMAND:
      result = DecodeCommand (kTeleinfoDomoticzCommands, TeleinfoDomoticzCommand);
      break;

    case FUNC_INIT:
      TeleinfoDomoticzInit ();
      break;

    case FUNC_EVERY_250_MSECOND:
      TeleinfoDomoticzEvery250ms ();
      break;
  }

  return result;
}

#endif      // USE_TELEINFO

