/*
  xdrv_112_gazpar_domoticz.ino - Domoticz integration for Gazpar

  Copyright (C) 2024  Nicolas Bernaerts

  Version history :
    12/05/2024 - v1.0 - Creation

  Configuration values are stored in :
    - Settings->rf_code[16][2]  : Flag en enable/disable integration

    - Settings->domoticz_sensor_idx[0]  : Gaz meter (volume in liter)
    - Settings->domoticz_sensor_idx[1]  : Energy P1 smart meter (power in Wh and W)

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
#ifdef USE_GAZPAR_DOMOTICZ

// declare gazpar domoticz integration
#define XDRV_112              112

/*******************************************\
 *               Variables
\*******************************************/

// Commands
static const char kDomoticzIntegrationCommands[] PROGMEM = "domo" "|" "" "|" "_set" "|" "_key";
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
  bool is_enabled = (domoticz_integration.enabled == 1);

  if (is_enabled != enabled)
  {
    // update status
    domoticz_integration.enabled = enabled;

    // reset publication flags
    domoticz_integration.index = 0; 

    // save configuration
    DomoticzIntegrationSaveConfig ();
  }
}

// get integration
bool DomoticzIntegrationGet () 
{
  return (domoticz_integration.enabled == 1);
}

/****************************\
 *        Commands
\****************************/

// Domoticz interation help
void CmndDomoticzIntegrationHelp ()
{
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: commands for Teleinfo Domoticz integration"));
  AddLog (LOG_LEVEL_INFO, PSTR ("  domo_set <%u>      = enable/disable Domoticz integration"), domoticz_integration.enabled);
  AddLog (LOG_LEVEL_INFO, PSTR ("  domo_key <num,idx> = set key num to index idx"));
  AddLog (LOG_LEVEL_INFO, PSTR ("    <0,%u>  : total volume (liter)"), Settings->domoticz_sensor_idx[0]);
  AddLog (LOG_LEVEL_INFO, PSTR ("    <1,%u>  : total (Wh) and instant power (W)"), Settings->domoticz_sensor_idx[1]);
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
  // load config
  DomoticzIntegrationLoadConfig ();
  
  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: domo to get help on Domoticz integration"));
}

// called 10 times per second
void DomoticzIntegrationEvery100ms ()
{
  // check if enabled
  if (!domoticz_integration.enabled) return;
  if (!RtcTime.valid) return;
  if (!MqttIsConnected ()) return;

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
void DomoticzIntegrationPublish ()
{
  int      rssi, wifi;
  uint16_t nvalue;
  char     str_svalue[32];

  // check for publication parameters
  if (!domoticz_integration.enabled) return;
  if (domoticz_integration.index >= MAX_DOMOTICZ_SNS_IDX) return;

  // publish next topic
  if (Settings->domoticz_sensor_idx[domoticz_integration.index] == 1)
  {
    // init
    nvalue = 0;
    strcpy (str_svalue,  "" );

    // set data according to index
    switch (domoticz_integration.index)
    {
      // volume (liter)
      case 0:
        nvalue = 0;
        sprintf_P (str_svalue, PSTR ("%u"), gazpar_config.total * 10);
        break;

      // energy (Wh and W)
      case 1:
        nvalue = 0;
        sprintf_P (str_svalue, PSTR ("%d;0;0;0;%d;0"), GazparConvertLiter2Wh (10L * gazpar_config.total), gazpar_meter.power);
        break;
    }

    // if some data should be published;
    if (strlen (str_svalue) > 0)
    {
      // get data
      rssi = WiFi.RSSI ();
      wifi = WifiGetRssiAsQuality (rssi);

      // publish data
      Response_P (PSTR ("{\"idx\":%u,\"nvalue\":%d,\"svalue\":\"%s\",\"Battery\":%u,\"RSSI\":%d}"), Settings->domoticz_sensor_idx[domoticz_integration.index], nvalue, str_svalue, 100, wifi / 10);
      MqttPublishPayload (DOMOTICZ_IN_TOPIC, ResponseData());
    }
  }

  // increase next index
  domoticz_integration.index++;
}

/***************************************\
 *              Interface
\***************************************/

// Gazpar Domoticz integration
bool Xdrv112 (uint32_t function)
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

#endif      // USE_GAZPAR_DOMOTICZ
#endif      // USE_GAZPAR

