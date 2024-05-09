/*
  xdrv_99_integration_homie.ino - Homie auto-discovery integration for Gazpar

  Copyright (C) 2024  Nicolas Bernaerts

  Version history :
    07/05/2024 - v1.0 - Creation
                        
  Configuration values are stored in :
    - Settings->rf_code[16][1]  : Flag en enable/disable integration

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
#ifdef USE_GAZPAR_HOMIE

// declare teleinfo homie integration
#define XDRV_99                    99

/*************************************************\
 *               Variables
\*************************************************/

// Commands
const char kHomieIntegrationCommands[] PROGMEM = "homie_" "|" "enable";
void (* const HomieIntegrationCommand[])(void) PROGMEM = { &CmndHomieIntegrationEnable };

/********************************\
 *              Data
\********************************/

static struct {
  uint8_t enabled = 0;
  uint8_t stage   = GAZ_PUB_CONNECT;      // auto-discovery publication stage
  uint8_t step    = 1;                    // auto-discovery step within a stage
  uint8_t data    = 0;                    // index of data to be published
} homie_integration;

/******************************************\
 *             Configuration
\******************************************/

// load configuration
void HomieIntegrationLoadConfig () 
{
  homie_integration.enabled = Settings->rf_code[16][1];
  homie_integration.stage = GAZ_PUB_CONNECT; 
  homie_integration.step  = 1; 
  homie_integration.data  = 0;
}

// save configuration
void HomieIntegrationSaveConfig () 
{
  Settings->rf_code[16][1] = homie_integration.enabled;
}

/***************************************\
 *               Function
\***************************************/

// set integration
void HomieIntegrationSet (const bool enabled) 
{
  bool is_enabled = (homie_integration.enabled == 1);

  if (is_enabled != enabled)
  {
    // update status
    homie_integration.enabled = enabled;

    // if disabled, publish offline data
    if (!enabled) HomieIntegrationPublish (GAZ_PUB_DISCONNECT, 0);

    // reset publication flags
    homie_integration.stage = GAZ_PUB_CONNECT; 
    homie_integration.step  = 1; 
    homie_integration.data  = 0;

    // save configuration
    HomieIntegrationSaveConfig ();
  }
}

// get integration
bool HomieIntegrationGet () 
{
  return (homie_integration.enabled == 1);
}

/*******************************\
 *          Command
\*******************************/

// Enable homie auto-discovery
void CmndHomieIntegrationEnable ()
{
  if (XdrvMailbox.data_len > 0) HomieIntegrationSet ((XdrvMailbox.payload != 0));
  ResponseCmndNumber (homie_integration.enabled);
}

/*******************************\
 *          Callback
\*******************************/

// trigger publication
void HomieIntegrationInit ()
{
  // load config
  HomieIntegrationLoadConfig ();

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: homie_enable to enable Homie auto-discovery [%u]"), homie_integration.enabled);
}

// trigger publication
void HomieIntegrationEvery250ms ()
{
  bool result = false;

  // check publication validity 
  if (!homie_integration.enabled) return;
  if (homie_integration.stage == UINT8_MAX) return;
  if (!RtcTime.valid) return;
  if (!MqttIsConnected ()) return;

  // publish current data
  result = HomieIntegrationPublish (homie_integration.stage, homie_integration.step);

  // if publication not fully done, increase step within stage
  if (!result) homie_integration.step++;

  // else increase stage
  else 
  {
    homie_integration.step = 1;
    homie_integration.stage++;
    if (homie_integration.stage >= GAZ_PUB_MAX) homie_integration.stage = UINT8_MAX;
  }
}

// trigger publication
void HomieIntegrationEvery100ms ()
{
  // check publication validity
  if (!homie_integration.enabled) return;
  if (homie_integration.data == UINT8_MAX) return;

  // do not publish value before end of auto-discovery declaration
  if (homie_integration.stage != UINT8_MAX) return;

  // publish current data
  HomieIntegrationPublish (homie_integration.data, 0);
  homie_integration.data++;
  if (homie_integration.data >= GAZ_PUB_MAX) homie_integration.data = UINT8_MAX;
}

void HomieIntegrationBeforeRestart ()
{
  // if enabled, publish disconnexion
  if (homie_integration.enabled) HomieIntegrationPublish (GAZ_PUB_DISCONNECT, 0);
}

/***************************************\
 *           JSON publication
\***************************************/

void HomieIntegrationPublishData ()
{
  homie_integration.data = 0;
}

// publish homie topic (data is published if step = 0)
bool HomieIntegrationPublish (const uint8_t stage, const uint8_t step)
{
  char   str_url[64];
  char   str_text[64];
  char   str_param[128];
  String str_topic;

  // check parameters
  if (stage == UINT8_MAX) return false;

  // init published data
  Response_P ("");

  // set url leaves
  switch (stage)
  {
    case GAZ_PUB_CONNECT:
      strcpy_P (str_url, PSTR ("|/$homie|/$name|/$state|/$nodes|END"));
      break;
    case GAZ_PUB_DISCONNECT:
      strcpy_P (str_url, PSTR ("|/$state|END"));
      break;
    case GAZ_PUB_DATA:
    case GAZ_PUB_M3:
    case GAZ_PUB_WH:
      strcpy_P (str_url, PSTR ("|/$name|/$properties|END"));
      break;
    default:
      strcpy_P (str_url, PSTR ("|/$name|/$datatype|/$unit|END"));
      break;
  }

  // check if anything to publish
  GetTextIndexed (str_text, sizeof (str_text), step, str_url);
  if (strcmp (str_text, "END") == 0) return true;

  // load parameters according to stage/data
  strcpy (str_param, "");
  switch (stage)
  {
    // declaration
    case GAZ_PUB_CONNECT:
      snprintf_P (str_param, sizeof (str_param), PSTR ("|3.0|%s|ready|data,m3,wh"), SettingsText (SET_DEVICENAME));
      break;

    // main data
    case GAZ_PUB_DATA:     strcpy_P (str_param, PSTR ("/data"         "|Data|factor,power|")); break;
    case GAZ_PUB_FACTOR:   strcpy_P (str_param, PSTR ("/data/factor"  "|Coefficient"  "|float"    "|kWh/m3")); break;
    case GAZ_PUB_POWER:    strcpy_P (str_param, PSTR ("/data/power"   "|Puissance"    "|integer"  "|W")); break;

    case GAZ_PUB_M3:       strcpy_P (str_param, PSTR ("/m3"           "|Volume|total,today,ytday|")); break;
    case GAZ_PUB_M3_TOTAL: strcpy_P (str_param, PSTR ("/m3/total"     "|Total"        "|integer"  "|m3")); break;
    case GAZ_PUB_M3_TODAY: strcpy_P (str_param, PSTR ("/m3/today"     "|Aujourd'hui"  "|integer"  "|m3")); break;
    case GAZ_PUB_M3_YTDAY: strcpy_P (str_param, PSTR ("/m3/ytday"     "|Hier"         "|integer"  "|m3")); break;

    case GAZ_PUB_WH:       strcpy_P (str_param, PSTR ("/wh"           "|Puissance|total,today,ytday|")); break;
    case GAZ_PUB_WH_TOTAL: strcpy_P (str_param, PSTR ("/wh/total"     "|Total"        "|integer"  "|Wh")); break;
    case GAZ_PUB_WH_TODAY: strcpy_P (str_param, PSTR ("/wh/today"     "|Aujourd'hui"  "|integer"  "|Wh")); break;
    case GAZ_PUB_WH_YTDAY: strcpy_P (str_param, PSTR ("/wh/ytday"     "|Hier"         "|integer"  "|Wh")); break;

    // disconnexion
    case GAZ_PUB_DISCONNECT:
      snprintf_P (str_param, sizeof (str_param), PSTR ("|disconnected"));
      break;
  }

  // if no valid data, ignore
  if (strlen (str_param) == 0) return true;

  // if publishing metadata
  if (step > 0) Response_P (PSTR ("%s"), GetTextIndexed (str_text, sizeof (str_text), step, str_param));

  // else, publishing value
  else switch (stage)
  {
    // data
    case GAZ_PUB_FACTOR:   Response_P (PSTR ("%d.%02d"), gazpar_config.param[GAZPAR_CONFIG_FACTOR]/100, gazpar_config.param[GAZPAR_CONFIG_FACTOR] % 100); break;
    case GAZ_PUB_POWER:    Response_P (PSTR ("%d"), gazpar_meter.power); break;

    // volume
    case GAZ_PUB_M3_TOTAL: Response_P (PSTR ("%d"), (long)RtcSettings.pulse_counter[0]); break;
    case GAZ_PUB_M3_TODAY: Response_P (PSTR ("%d"), (long)RtcSettings.pulse_counter[0] - gazpar_config.param[GAZPAR_CONFIG_TODAY]); break;
    case GAZ_PUB_M3_YTDAY: Response_P (PSTR ("%d"), gazpar_config.param[GAZPAR_CONFIG_TODAY] - gazpar_config.param[GAZPAR_CONFIG_YESTERDAY]); break;

    // wh
    case GAZ_PUB_WH_TOTAL: Response_P (PSTR ("%d"), GazparConvertLiter2Wh ((long)RtcSettings.pulse_counter[0])); break;
    case GAZ_PUB_WH_TODAY: Response_P (PSTR ("%d"), GazparConvertLiter2Wh ((long)RtcSettings.pulse_counter[0] - gazpar_config.param[GAZPAR_CONFIG_TODAY])); break;
    case GAZ_PUB_WH_YTDAY: Response_P (PSTR ("%d"), GazparConvertLiter2Wh (gazpar_config.param[GAZPAR_CONFIG_TODAY] - gazpar_config.param[GAZPAR_CONFIG_YESTERDAY])); break;
  }

  // if something to publish
  if (ResponseLength () > 0)
  {
    // set topic to lower case
    str_topic  = PSTR ("homie/");
    str_topic += TasmotaGlobal.hostname;
    GetTextIndexed (str_text, sizeof (str_text), 0, str_param);
    if (strlen (str_text) > 0) str_topic += str_text;
    GetTextIndexed (str_text, sizeof (str_text), step, str_url);
    if (strlen (str_text) > 0) str_topic += str_text;
    str_topic.toLowerCase ();

    // publish topic
    if (step == 0) MqttPublish (str_topic.c_str (), false);
      else MqttPublish (str_topic.c_str (), true);
  }

  // if data publication, return true, else return false as END has not been reached
  return (step == 0);
}

/***************************************\
 *              Interface
\***************************************/

// Teleinfo homie driver
bool Xdrv99 (uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
    case FUNC_INIT:
      HomieIntegrationInit ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kHomieIntegrationCommands, HomieIntegrationCommand);
      break;
    case FUNC_EVERY_250_MSECOND:
      HomieIntegrationEvery250ms ();
      break;
    case FUNC_EVERY_100_MSECOND:
      HomieIntegrationEvery100ms ();
      break;
    case FUNC_SAVE_BEFORE_RESTART:
      HomieIntegrationBeforeRestart ();
      break;
  }

  return result;
}

#endif      // USE_GAZPAR_HOMIE
#endif      // USE_GAZPAR

