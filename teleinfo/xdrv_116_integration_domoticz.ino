/*
  xdrv_116_integration_domoticz.ino - Domoticz integration for Teleinfo

  Copyright (C) 2024  Nicolas Bernaerts

  Version history :
    07/02/2024 - v1.0 - Creation
    14/04/2024 - v1.1 - switch to rf_code configuration

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

// Commands
const char kDomoticzIntegrationCommands[] PROGMEM = "domo_" "|" "help" "|" "enable" "|" "key";
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

/****************************\
 *        Commands
\****************************/

// Domoticz interation help
void CmndDomoticzIntegrationHelp ()
{
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: commands for Teleinfo Domoticz integration"));
  AddLog (LOG_LEVEL_INFO, PSTR ("  domo_enable <%u>    = enable/disable Domoticz integration"), domoticz_integration.enabled);
  AddLog (LOG_LEVEL_INFO, PSTR ("  domo_key <num,idx> = set key num to index idx"));
  AddLog (LOG_LEVEL_INFO, PSTR ("    <0,%u>  : total Wh (hc/hp) and power W for conso (base,hc/hp,ejp,bleu)"), Settings->domoticz_sensor_idx[0]);
  AddLog (LOG_LEVEL_INFO, PSTR ("    <1,%u>  : total Wh (hc/hp) and power W for conso (blanc)"), Settings->domoticz_sensor_idx[1]);
  AddLog (LOG_LEVEL_INFO, PSTR ("    <2,%u>  : total Wh (hc/hp) and power W for conso (rouge)"), Settings->domoticz_sensor_idx[2]);
  AddLog (LOG_LEVEL_INFO, PSTR ("    <8,%u>  : total Wh and power W for production"), Settings->domoticz_sensor_idx[8]);
  AddLog (LOG_LEVEL_INFO, PSTR ("    <9,%u>  : alert for current hc/hp"), Settings->domoticz_sensor_idx[9]);
  AddLog (LOG_LEVEL_INFO, PSTR ("    <10,%u> : alert fot current period color (bleu, blanc, rouge)"), Settings->domoticz_sensor_idx[10]);
  AddLog (LOG_LEVEL_INFO, PSTR ("    <11,%u> : alert for tomorrow's period color (bleu, blanc, rouge)"), Settings->domoticz_sensor_idx[11]);
  ResponseCmndDone ();
}

// Enable Domoticz integration
void CmndDomoticzIntegrationEnable ()
{
  if (XdrvMailbox.data_len > 0)
  {
    // update status
    domoticz_integration.enabled = (XdrvMailbox.payload != 0);

    // reset publication flags
    domoticz_integration.index = 0; 

    // save configuration
    DomoticzIntegrationSaveConfig ();
  }

  ResponseCmndNumber (domoticz_integration.enabled);
}

// set Domoticz key
void CmndDomoticzIntegrationSetKey ()
{
  uint8_t index = UINT8_MAX;
  char   *pstr_value;
  char    str_data[16];
  char    str_text[64];

  if (XdrvMailbox.data_len > 0)
  {
    // look for separator
    strlcpy (str_data, XdrvMailbox.data, sizeof (str_data));
    pstr_value = strchr (str_data, ',');
    if (pstr_value != nullptr) *pstr_value++ = 0;

    // get key
    index = (uint8_t)atoi (str_data);

    // if separator was found, update value
    if ((index < MAX_DOMOTICZ_SNS_IDX) && (pstr_value != nullptr)) Settings->domoticz_sensor_idx[index] = (uint16_t)atoi (pstr_value);
  }

  // answer
  if (index < MAX_DOMOTICZ_SNS_IDX)
  {
    if (Settings->domoticz_sensor_idx[index] > 0) snprintf_P (str_text, sizeof (str_text), PSTR ("Domoticz sensor %u enabled (idx %u)"), index, Settings->domoticz_sensor_idx[index]);
      else snprintf_P (str_text, sizeof (str_text), PSTR ("Domoticz sensor %u disabled"), index);
    ResponseCmndChar_P (str_text);
  }
  else ResponseCmndFailed ();
}

/****************************\
 *        Callback
\****************************/

// Domoticz init message
void DomoticzIntegrationInit ()
{
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: domo_help to get help on Domoticz integration"));
}

// called 10 times per second
void DomoticzIntegrationEvery100ms ()
{
  // check if enabled
  if (!domoticz_integration.enabled) return;

  // publish if needed
  if (domoticz_integration.index < MAX_DOMOTICZ_SNS_IDX) DomoticzIntegrationPublish ();
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
  if (!domoticz_integration.enabled) return MAX_DOMOTICZ_SNS_IDX;
  if (teleinfo_contract.period == UINT8_MAX) return MAX_DOMOTICZ_SNS_IDX;

  // check if there is something to publish
  if (domoticz_integration.index >= MAX_DOMOTICZ_SNS_IDX) return MAX_DOMOTICZ_SNS_IDX;

  // process data reception
  TeleinfoProcessRealTime ();

  // publish next topic
  while ((domoticz_integration.index < MAX_DOMOTICZ_SNS_IDX) && (Settings->domoticz_sensor_idx[domoticz_integration.index] == 0)) domoticz_integration.index++;
  if (domoticz_integration.index < MAX_DOMOTICZ_SNS_IDX)
  {
    // init
    nvalue = 0;
    strcpy (str_svalue,  "" );
    strcpy (str_total_1, "0");
    strcpy (str_total_2, "0");
    sns_index = Settings->domoticz_sensor_idx[domoticz_integration.index];

    // set data according to index
    switch (domoticz_integration.index)
    {
      // conso
      case 0:
      case 1:
      case 2:
        value = 0;
        index = domoticz_integration.index * 2;
        nvalue = 0;
        lltoa (teleinfo_conso.index_wh[index], str_total_1, 10);
        lltoa (teleinfo_conso.index_wh[index + 1], str_total_2, 10);
        if ((teleinfo_contract.period == index) || (teleinfo_contract.period == index + 1)) value = teleinfo_conso.pact;
        sprintf_P (str_svalue, PSTR ("%s;%s;0;0;%d;0"), str_total_1, str_total_2, value);
        break;

      // prod
      case 8:
        nvalue = 0;
        lltoa (teleinfo_prod.total_wh, str_total_1, 10);
        sprintf_P (str_svalue, PSTR ("%s;0;0;0;%d;0"), str_total_1, teleinfo_prod.pact);
        break;

      // current hc / hp
      case 9:
        nvalue = 1;
        if (TeleinfoPeriodGetHP ()) nvalue = 0;
        if (nvalue == 0) strcpy (str_svalue, "Heure Pleine"); else strcpy (str_svalue, "Heure Creuse");
        break;

      // current level
      case 10:
        nvalue = TeleinfoPeriodGetLevel ();
        GetTextIndexed (str_svalue, sizeof (str_svalue), nvalue, kTeleinfoPeriodLevel);
        if (nvalue == 3) nvalue = 4;
        break;

      // tomorrow's level
      case 11:
        nvalue = TeleinfoDriverCalendarGetLevel (TIC_DAY_TOMORROW, 12, true);
        GetTextIndexed (str_svalue, sizeof (str_svalue), nvalue, kTeleinfoPeriodLevel);
        if (nvalue == 3) nvalue = 4;
        break;
    }

    // if some data should be published;
    if (strlen (str_svalue) > 0)
    {
      // get data
      rssi = WiFi.RSSI ();
      wifi = WifiGetRssiAsQuality (rssi);

      // publish data
      ResponseClear ();
      ResponseAppend_P (PSTR ("{\"idx\":%u,\"nvalue\":%d,\"svalue\":\"%s\",\"Battery\":%u,\"RSSI\":%d}"), sns_index, nvalue, str_svalue, 100, wifi / 10);
      MqttPublishPayload (DOMOTICZ_IN_TOPIC, ResponseData());
    }
  }

  // increase next index
  domoticz_integration.index++;

  return domoticz_integration.index;
}

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
   case FUNC_EVERY_100_MSECOND:
      DomoticzIntegrationEvery100ms ();
      break;
  }

  return result;
}

#endif      // USE_TELEINFO_DOMOTICZ
#endif      // USE_TELEINFO

