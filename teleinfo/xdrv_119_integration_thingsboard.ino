/*
  xdrv_119_integration_thingsboard.ino - ThingsBoard integration for Teleinfo

  Copyright (C) 2024  Nicolas Bernaerts

  Version history :
    20/07/2024 - v1.0 - Creation
    13/08/2024 - v1.1 - Remove ts from JSON
    26/08/2024 - v1.2 - Add attributes data
                        
  Configuration values are stored in :
    - Settings->rf_code[16][3]  : Flag en enable/disable integration

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
#ifdef USE_TELEINFO_THINGSBOARD

// declare teleinfo homie integration
#define XDRV_119      119

/*************************************************\
 *               Variables
\*************************************************/

#define TELEINFO_THINGSBOARD_NB_MESSAGE    5

// Commands
const char kThingsboardIntegrationCommands[] PROGMEM = "" "|" "thingsboard";
void (* const ThingsboardIntegrationCommand[])(void) PROGMEM = { &CmndThingsboardIntegrationEnable };

/********************************\
 *              Data
\********************************/

static struct {
  uint8_t enabled  = 0;            // integration enabled flag
  uint8_t pub_data = 0;            // data publication flag
  uint8_t pub_attr = 0;            // attributes publication flag
} thingsboard_integration;

/******************************************\
 *             Configuration
\******************************************/

// load configuration
void ThingsboardIntegrationLoadConfig () 
{
  thingsboard_integration.enabled = Settings->rf_code[16][3];
}

// save configuration
void ThingsboardIntegrationSaveConfig () 
{
  Settings->rf_code[16][3] = thingsboard_integration.enabled;
}

/***************************************\
 *               Function
\***************************************/

// set integration
void ThingsboardIntegrationSet (const bool enabled) 
{
  // update status
  thingsboard_integration.enabled = enabled;

  // save configuration
  ThingsboardIntegrationSaveConfig ();
}

// get integration
bool ThingsboardIntegrationGet () 
{
  return (thingsboard_integration.enabled == 1);
}

/*******************************\
 *          Command
\*******************************/

// Enable thingsboard integration
void CmndThingsboardIntegrationEnable ()
{
  if (XdrvMailbox.data_len > 0) ThingsboardIntegrationSet ((XdrvMailbox.payload != 0));
  ResponseCmndNumber (thingsboard_integration.enabled);
}

/*******************************\
 *          Callback
\*******************************/

// trigger publication
void ThingsboardIntegrationInit ()
{
  // load config
  ThingsboardIntegrationLoadConfig ();

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Run thingsboard to set ThingsBoard integration [%u]"), thingsboard_integration.enabled);
}

// trigger publication
void ThingsboardIntegrationEvery250ms ()
{
  // check publication validity 
  if (!thingsboard_integration.enabled) return;
  if (!RtcTime.valid) return;
  if (!MqttIsConnected ()) return;

  // publish current data
  if (thingsboard_integration.pub_data) ThingsboardIntegrationPublishData ();
  else if (thingsboard_integration.pub_attr) ThingsboardIntegrationPublishAttribute ();
}

// publish current data
void ThingsboardIntegrationPublishData ()
{
  uint8_t index, phase, value;
  long    voltage, current, power_app, power_act;
  char    str_value[32];
  char    str_text[32];

  ResponseClear ();
  ResponseAppend_P (PSTR ("{"));

  // METER basic data
  ResponseAppend_P (PSTR ("\"PH\":%u"), teleinfo_contract.phase);

  // conso 
  if (teleinfo_conso.total_wh > 0)
  {
    // conso : loop thru phases
    voltage = current = power_app = power_act = 0;
    for (phase = 0; phase < teleinfo_contract.phase; phase++)
    {
      // calculate parameters
      value = phase + 1;
      voltage   += teleinfo_conso.phase[phase].voltage;
      current   += teleinfo_conso.phase[phase].current;
      power_app += teleinfo_conso.phase[phase].papp;
      power_act += teleinfo_conso.phase[phase].pact;

      // voltage and current
      ResponseAppend_P (PSTR (",\"U%u\":%d,\"I%u\":%d.%02d"), value, teleinfo_conso.phase[phase].voltage, value, teleinfo_conso.phase[phase].current / 1000, teleinfo_conso.phase[phase].current % 1000 / 10);

      // apparent and active power
      ResponseAppend_P (PSTR (",\"P%u\":%d,\"W%u\":%d"), value, teleinfo_conso.phase[phase].papp, value, teleinfo_conso.phase[phase].pact);

      // cos phi
      if (teleinfo_conso.cosphi.nb_measure > 1) ResponseAppend_P (PSTR (",\"C%u\":%d.%02d"), value, teleinfo_conso.phase[phase].cosphi / 1000, teleinfo_conso.phase[phase].cosphi % 1000 / 10);
    } 

    // conso : global values
    ResponseAppend_P (PSTR (",\"U\":%d,\"I\":%d.%02d,\"P\":%d,\"W\":%d"), voltage / (long)teleinfo_contract.phase, current / 1000, current % 1000 / 10, power_app, power_act);

    // conso : cosphi
    if (teleinfo_conso.cosphi.nb_measure > 1) ResponseAppend_P (PSTR (",\"C\":%d.%02d"), teleinfo_conso.cosphi.value / 1000, teleinfo_conso.cosphi.value % 1000 / 10);
  }
  
  // production 
  if (teleinfo_prod.total_wh != 0)
  {
    // prod : global values
    ResponseAppend_P (PSTR (",\"PP\":%d,\"PW\":%d"), teleinfo_prod.papp, teleinfo_prod.pact);

    // prod : cosphi
    if (teleinfo_prod.cosphi.nb_measure > 1) ResponseAppend_P (PSTR (",\"PC\":%d.%02d"), teleinfo_prod.cosphi.value / 1000, teleinfo_prod.cosphi.value % 1000 / 10);
  } 

  // meter serial number
  lltoa (teleinfo_contract.ident, str_value, 10);
  ResponseAppend_P (PSTR (",\"SERIAL\":%s"), str_value);

  // contract name
  TeleinfoContractGetName (str_value, sizeof (str_value));
  ResponseAppend_P (PSTR (",\"NAME\":\"%s\""), str_value);

  // contract period
  TeleinfoPeriodGetLabel (str_value, sizeof (str_value));
  ResponseAppend_P (PSTR (",\"PERIOD\":\"%s\""), str_value);

  // contract color
  index = TeleinfoPeriodGetLevel ();
  GetTextIndexed (str_value, sizeof (str_value), index, kTeleinfoPeriodLabel);
  ResponseAppend_P (PSTR (",\"COLOR\":\"%s\""), str_value);

  // contract hour type
  index = TeleinfoPeriodGetHP ();
  GetTextIndexed (str_value, sizeof (str_value), index, kTeleinfoPeriodHour);
  ResponseAppend_P (PSTR (",\"HOUR\":\"%s\""), str_value);

  // total conso counter
  lltoa (teleinfo_conso.total_wh, str_value, 10);
  ResponseAppend_P (PSTR (",\"TOTAL\":%s"), str_value);

  // loop to publish conso counters
  for (index = 0; index < teleinfo_contract.period_qty; index ++)
  {
    TeleinfoPeriodGetCode (str_text, sizeof (str_text), index);
    lltoa (teleinfo_conso.index_wh[index], str_value, 10);
    ResponseAppend_P (PSTR (",\"%s\":%s"), str_text, str_value);
  }

  // total production counter
  if (teleinfo_prod.total_wh != 0)
  {
    lltoa (teleinfo_prod.total_wh, str_value, 10) ;
    ResponseAppend_P (PSTR (",\"PROD\":%s"), str_value);
  }

  // wifi signal
  ResponseAppend_P (PSTR (",\"RSSI\":%d"), WiFi.RSSI ());

  // end of message and publication
  ResponseJsonEnd ();

  // publish message
  MqttPublishPayload ("v1/devices/me/telemetry", ResponseData());

  // reset publication flag
  thingsboard_integration.pub_data = 0;
}

// publish current data
void ThingsboardIntegrationPublishAttribute ()
{
  ResponseClear ();
  ResponseAppend_P (PSTR ("{"));

  // module name
  ResponseAppend_P (PSTR ("\"Module\":\"%s\""), ModuleName().c_str());

  // version Tasmota
  ResponseAppend_P (PSTR (",\"Tasmota\":\"%s\""), TasmotaGlobal.version);

  // version Teleinfo
  ResponseAppend_P (PSTR (",\"Teleinfo\":\"%s\""), PSTR (EXTENSION_VERSION));

  // IP address
  ResponseAppend_P (PSTR (",\"IP\":\"%_I\""), (uint32_t)WiFi.localIP());

  // restarts reason
  ResponseAppend_P (PSTR (",\"Restart\":\"%s\""), GetResetReason().c_str());

  // wifi signal
  ResponseAppend_P (PSTR (",\"BootCount\":%d"), Settings->bootcount +1);

  // end of message and publication
  ResponseJsonEnd ();

  // publish message
  MqttPublishPayload ("v1/devices/me/attributes", ResponseData());

  // reset publication flag
  thingsboard_integration.pub_attr = 0;
}

/***************************************\
 *           JSON publication
\***************************************/

// ask for data publication
void ThingsboardIntegrationTriggerData ()
{
  thingsboard_integration.pub_data = 1;
}

// ask for attributes publication
void ThingsboardIntegrationTriggerAttribute ()
{
  thingsboard_integration.pub_attr = 1;
}

/***************************************\
 *              Interface
\***************************************/

// Teleinfo homie driver
bool Xdrv119 (uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
    case FUNC_INIT:
      ThingsboardIntegrationInit ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kThingsboardIntegrationCommands, ThingsboardIntegrationCommand);
      break;
    case FUNC_EVERY_250_MSECOND:
      ThingsboardIntegrationEvery250ms ();
      break;
  }

  return result;
}

#endif      // USE_TELEINFO_THINGSBOARD
#endif      // USE_TELEINFO

