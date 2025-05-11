/*
  xdrv_114_gazpar_homie.ino - Homie auto-discovery integration for Gazpar

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
#define XDRV_114                    114

/*************************************************\
 *               Variables
\*************************************************/

// URL families
static const char PSTR_HOMIE_GAZ_URL_CONNECT[]    PROGMEM = "|/$homie|/$name|/$state|/$nodes|END";
static const char PSTR_HOMIE_GAZ_URL_DISCONNECT[] PROGMEM = "|/$state|END";
static const char PSTR_HOMIE_GAZ_URL_DATA[]       PROGMEM = "|/$name|/$properties|END";
static const char PSTR_HOMIE_GAZ_URL_DEFAULT[]    PROGMEM = "|/$name|/$datatype|/$unit|END";

// parameters
static const char PSTR_HOMIE_GAZ_PUB_CONNECT[]    PROGMEM = "|3.0|---|ready|data,m3,wh";
static const char PSTR_HOMIE_GAZ_PUB_DISCONNECT[] PROGMEM = "|disconnected";

static const char PSTR_HOMIE_GAZ_PUB_DATA[]       PROGMEM = "/data"        "|Data"        "|factor,power|";
static const char PSTR_HOMIE_GAZ_PUB_FACTOR[]     PROGMEM = "/data/factor" "|Coefficient" "|float"   "|kWh/m3";
static const char PSTR_HOMIE_GAZ_PUB_POWER[]      PROGMEM = "/data/power"  "|Puissance"   "|integer" "|W";

static const char PSTR_HOMIE_GAZ_PUB_M3[]         PROGMEM = "/m3"          "|Volume"      "|total,today,ytday|";
static const char PSTR_HOMIE_GAZ_PUB_M3_TOTAL[]   PROGMEM = "/m3/total"    "|Total"       "|integer" "|m3";
static const char PSTR_HOMIE_GAZ_PUB_M3_TODAY[]   PROGMEM = "/m3/today"    "|Aujourd'hui" "|integer" "|m3";
static const char PSTR_HOMIE_GAZ_PUB_M3_YTDAY[]   PROGMEM = "/m3/ytday"    "|Hier"        "|integer" "|m3";

static const char PSTR_HOMIE_GAZ_PUB_WH[]         PROGMEM = "/wh"          "|Puissance"   "|total,today,ytday|";
static const char PSTR_HOMIE_GAZ_PUB_WH_TOTAL[]   PROGMEM = "/wh/total"    "|Total"       "|integer" "|Wh";
static const char PSTR_HOMIE_GAZ_PUB_WH_TODAY[]   PROGMEM = "/wh/today"    "|Aujourd'hui" "|integer" "|Wh";
static const char PSTR_HOMIE_GAZ_PUB_WH_YTDAY[]   PROGMEM = "/wh/ytday"    "|Hier"        "|integer" "|Wh";

// Commands
static const char kHomieIntegrationCommands[] PROGMEM = "" "|" "homie";
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

    // save configuration
    HomieIntegrationSaveConfig ();

    // reset publication flags
    homie_integration.stage = GAZ_PUB_CONNECT; 
    homie_integration.step  = 1; 
    homie_integration.data  = 0;
    
    // if disabled, publish offline data
    if (!enabled) HomieIntegrationPublish (GAZ_PUB_DISCONNECT, 0);
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
  // reset publication flags
  homie_integration.stage = GAZ_PUB_CONNECT; 
  homie_integration.step  = 1; 
  homie_integration.data  = 0;

  // load config
  HomieIntegrationLoadConfig ();

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: homie to enable Homie auto-discovery [%u]"), homie_integration.enabled);
}

// trigger publication
void HomieIntegrationPublishAutoHeader ()
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
void HomieIntegrationPublishAutoData ()
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
  long   value;
  static const char* pstr_url   = nullptr;
  static const char* pstr_param = nullptr;
  char   str_text[64];
  String str_topic;

  // check parameters
  if (stage == UINT8_MAX) return false;

  // set url leaves
  switch (stage)
  {
    case GAZ_PUB_CONNECT:    pstr_url = PSTR_HOMIE_GAZ_URL_CONNECT; break;
    case GAZ_PUB_DISCONNECT: pstr_url = PSTR_HOMIE_GAZ_URL_DISCONNECT; break;
    case GAZ_PUB_DATA:
    case GAZ_PUB_M3:
    case GAZ_PUB_WH:         pstr_url = PSTR_HOMIE_GAZ_URL_DATA; break;
    default:                 pstr_url = PSTR_HOMIE_GAZ_URL_DEFAULT; break;
  }

  // check if anything to publish
  GetTextIndexed (str_text, sizeof (str_text), step, pstr_url);
  if (strcmp_P (str_text, PSTR ("END")) == 0) return true;

  // load parameters according to stage/data
  switch (stage)
  {
    // declaration
    case GAZ_PUB_CONNECT:    pstr_param = PSTR_HOMIE_GAZ_PUB_CONNECT; break;
    case GAZ_PUB_DISCONNECT: pstr_param = PSTR_HOMIE_GAZ_PUB_DISCONNECT; break;

    case GAZ_PUB_DATA:       pstr_param = PSTR_HOMIE_GAZ_PUB_DATA; break;
    case GAZ_PUB_FACTOR:     pstr_param = PSTR_HOMIE_GAZ_PUB_FACTOR; break;
    case GAZ_PUB_POWER:      pstr_param = PSTR_HOMIE_GAZ_PUB_POWER; break;

    case GAZ_PUB_M3:         pstr_param = PSTR_HOMIE_GAZ_PUB_M3; break;
    case GAZ_PUB_M3_TOTAL:   pstr_param = PSTR_HOMIE_GAZ_PUB_M3_TOTAL; break;
    case GAZ_PUB_M3_TODAY:   pstr_param = PSTR_HOMIE_GAZ_PUB_M3_TODAY; break;
    case GAZ_PUB_M3_YTDAY:   pstr_param = PSTR_HOMIE_GAZ_PUB_M3_YTDAY; break;

    case GAZ_PUB_WH:         pstr_param = PSTR_HOMIE_GAZ_PUB_WH; break;
    case GAZ_PUB_WH_TOTAL:   pstr_param = PSTR_HOMIE_GAZ_PUB_WH_TOTAL; break;
    case GAZ_PUB_WH_TODAY:   pstr_param = PSTR_HOMIE_GAZ_PUB_WH_TODAY; break;
    case GAZ_PUB_WH_YTDAY:   pstr_param = PSTR_HOMIE_GAZ_PUB_WH_YTDAY; break;
  }

  // if no valid data, ignore
  if (pstr_param == nullptr) return true;

  // if publishing metadata
  Response_P (PSTR (""));
  if ((stage == GAZ_PUB_CONNECT) && (step == 2)) Response_P (PSTR ("%s"), SettingsText (SET_DEVICENAME));
  else if (step > 0) Response_P (PSTR ("%s"), GetTextIndexed (str_text, sizeof (str_text), step, pstr_param));

  // else, publishing value
  else switch (stage)
  {
    // data
    case GAZ_PUB_FACTOR:
      Response_P (PSTR ("%d.%02d"), gazpar_config.power_factor / 100, gazpar_config.power_factor % 100); 
      break;
    case GAZ_PUB_POWER:
      Response_P (PSTR ("%d"), gazpar_meter.power); 
      break;

    // volume
    case GAZ_PUB_M3_TOTAL: 
      value = gazpar_config.total; 
      Response_P (PSTR ("%d.%02d"), value / 100, value % 100); 
      break;
    case GAZ_PUB_M3_TODAY: 
      value = gazpar_config.total_today; 
      Response_P (PSTR ("%d.%02d"), value / 100, value % 100); 
      break;
    case GAZ_PUB_M3_YTDAY: 
      value = gazpar_config.total_ytday;
      Response_P (PSTR ("%d.%02d"), value / 100, value % 100); 
      break;

    // wh
    case GAZ_PUB_WH_TOTAL:
      value = GazparConvertLiter2Wh (10L * gazpar_config.total);
      Response_P (PSTR ("%d"), value); 
      break;
    case GAZ_PUB_WH_TODAY:
      value = GazparConvertLiter2Wh (10L * gazpar_config.total_today);
      Response_P (PSTR ("%d"), value); 
      break;
    case GAZ_PUB_WH_YTDAY: 
      value = GazparConvertLiter2Wh (10L * gazpar_config.total_ytday);
      Response_P (PSTR ("%d"), value); 
      break;
  }

  // if something to publish
  if (ResponseLength () > 0)
  {
    // set topic to lower case
    str_topic  = F ("homie/");
    str_topic += TasmotaGlobal.hostname;
    str_topic += GetTextIndexed (str_text, sizeof (str_text), 0, pstr_param);
    str_topic += GetTextIndexed (str_text, sizeof (str_text), step, pstr_url);
    str_topic.toLowerCase ();

    // publish topic : retain flag if not data
    MqttPublish (str_topic.c_str (), (step > 0));
  }

  // if data publication, return true, else return false as END has not been reached
  return (step == 0);
}

/***************************************\
 *              Interface
\***************************************/

// Teleinfo homie driver
bool Xdrv114 (uint32_t function)
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
      HomieIntegrationPublishAutoHeader ();
      HomieIntegrationPublishAutoData ();
      break;
    case FUNC_SAVE_BEFORE_RESTART:
      HomieIntegrationBeforeRestart ();
      break;
  }

  return result;
}

#endif      // USE_GAZPAR_HOMIE
#endif      // USE_GAZPAR

