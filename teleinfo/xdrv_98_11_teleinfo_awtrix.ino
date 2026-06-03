/*
  xdrv_98_11_awtrix_config.ino - Handle Awtrix display (Ulanzi TC001)
  
  Only compatible with ESP32

  Copyright (C) 2025  Nicolas Bernaerts
    15/02/2025 v1.0 - Creation
    10/07/2025 v2.0 - Refactoring based on Tasmota 15
    15/08/2025 v2.1 - Add Cosphi
    07/09/2025 v2.2 - Limit publications to 1 per sec.
    23/09/2025 v2.3 - Handle publication for Winky with deepsleep on super capa
    01/03/2026 v3.0 - Complete rework (api/mqtt, colors, ...)
                      Data are now saved on FS

  Previous configuration values were stored in :

  - Settings->rf_code[12][0] : delay between page display (sec.)
  - Settings->rf_code[12][1] : Production power display limit (x100)
  - Settings->rf_code[12][2] : Fixed brightness (1..100%) / 0 for auto brightness
  
  - Settings->rf_code[12][3] : Flag to display instant power
  - Settings->rf_code[12][4] : Flag to display calendar
  - Settings->rf_code[12][5] : Flag to display today's conso
  - Settings->rf_code[12][6] : Flag to display today's production
  - Settings->rf_code[12][7] : Flag to display cosphi

  TextSettings :
  - SET_TIC_AW_URL           : URL to access Awtrix display

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

#ifdef ESP32
#ifdef USE_TELEINFO
#ifdef USE_TELEINFO_AWTRIX

#include <ArduinoJson.h>

// constants
#define AWTRIX_BRIGHTNESS_MIN           1             // minimum brightness
#define AWTRIX_BRIGHTNESS_TRIGGER       2             // change brightness on % change
#define AWTRIX_BRIGHTNESS_DELAY         10            // minimum delay between brightness reading
#define AWTRIX_DELAY_DEFAULT            4             // minimum delay between 2 pages
#define AWTRIX_PROD_MAX_DEFAULT         1000          // default maximum produced power
#define AWTRIX_LUX_MIN_DEFAULT          10            // minimum Lux level
#define AWTRIX_LUX_MAX_DEFAULT          1000          // maximum Lux level
#define AWTRIX_LUX_REF_DEFAULT          100000        // reference Lux level
#define AWTRIX_CONNECT_TIMEOUT          250           // connexion timeout to Awtrix device (ms)
#define AWTRIX_ANSWER_TIMEOUT           1000          // result timeout to Awtrix device (ms)

// data file
#define AWTRIX_FILE_VERSION             1
const char PSTR_AWTRIX_DATA_FILE[]      PROGMEM = "/teleinfo-awtrix.dat";

// web URL
#define AWTRIX_PAGE_CFG                 "/awtrix"

// display type
enum TeleinfoAwtrixMode    { TIC_AWTRIX_MODE_CONSO, TIC_AWTRIX_MODE_PROD, TIC_AWTRIX_MODE_MAX };
enum TeleinfoAwtrixType    { TIC_AWTRIX_TYPE_SETTING, TIC_AWTRIX_TYPE_DATA, TIC_AWTRIX_TYPE_MAX };
enum TeleinfoAwtrixDisplay { TIC_AWTRIX_INSTANT, TIC_AWTRIX_CONSO_WH, TIC_AWTRIX_PROD_WH, TIC_AWTRIX_CALENDAR, TIC_AWTRIX_COSPHI, TIC_AWTRIX_DMAX };

// awtrix commands
const char kTeleinfoAwtrixCommands[]         PROGMEM =   "awtrix|"             "|"       "_type"        "|"       "_addr"        "|"         "_lumi"        "|"       "_delai"        "|"          "_inst"        "|"          "_cos"        "|"          "_cwh"         "|"           "_pwh"       "|"          "_cal"            ;
void (* const TeleinfoAwtrixCommand[])(void) PROGMEM = { &CmndTeleinfoAwtrixHelp, &CmndTeleinfoAwtrixType, &CmndTeleinfoAwtrixAddr, &CmndTeleinfoAwtrixBright, &CmndTeleinfoAwtrixDelay, &CmndTeleinfoAwtrixInstant, &CmndTeleinfoAwtrixCosphi, &CmndTeleinfoAwtrixConsoWh, &CmndTeleinfoAwtrixProdWh, &CmndTeleinfoAwtrixCalendar};

// awtrix conso bar graph colors
enum TeleinfoAwtrixPeriod                       { TIC_AWTRIX_UNDEF, TIC_AWTRIX_BLUE, TIC_AWTRIX_WHITE, TIC_AWTRIX_RED, TIC_AWTRIX_PMAX };
const char kTeleinfoAwtrixConsoColors[] PROGMEM =   "#0000FF"  "|"  "#0000FF" "|"  "#FFFFFF"  "|"  "#FF0000" ;                      // conso : undef, blue, white, red
const char kTeleinfoAwtrixTextColors[]  PROGMEM =   "#FFFFFF"  "|"  "#FFFFFF" "|"  "#000000"  "|"  "#FFFFFF" ;                      // conso : undef, blue, white, red
const char kTeleinfoAwtrixProdColor[]   PROGMEM =   "#FFFF00";                                                                            // prod
const char kTeleinfoAwtrixUnitColor[]   PROGMEM =   "#808080";                                                                            // unit (W, Wh, ...) 

/************************\
 *         Data
\************************/

struct {                                // 115 bytes
  bool     use_mqtt   = false;                      // use API or MQTT
  bool     arr_flag[TIC_AWTRIX_DMAX];               // array of display flags
  uint8_t  brightness = 0;                          // brightness value
  uint8_t  delay      = AWTRIX_DELAY_DEFAULT;       // delay between page display
  uint8_t  tr_effect  = 1;                          // transition effect between display
  uint16_t tr_speed   = 200;                        // transition speed between display
  uint32_t time_upd   = 0;                          // timestamp of next data update
  char     str_conso[TIC_AWTRIX_PMAX][2][8];        // color for conso display [undef/blue/white/red][hc/hp]
  char     str_prod[8];                             // color for production display
  char     str_device[32];                          // IP address / topic of device
} awtrix_config;

struct {
  bool init = true;                                 // flag for awtrix initialisation
} awtrix_status;

/***********************************************\
 *                  Commands
\***********************************************/

// whatsapp help
void CmndTeleinfoAwtrixHelp ()
{
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Commandes d'affichage Awtrix :"));
  AddLog (LOG_LEVEL_INFO, PSTR ("  awtrix_type <type> = Type d'acces (api ou mqtt)"));
  AddLog (LOG_LEVEL_INFO, PSTR ("  awtrix_addr <addr> = Adresse IP / topic du device"));
  AddLog (LOG_LEVEL_INFO, PSTR ("  awtrix_delai <val> = Délai entre 2 pages (min. 2s) [%u]"),     awtrix_config.delay);
  AddLog (LOG_LEVEL_INFO, PSTR ("  awtrix_lumi  <val> = Luminosité (1..100%%, 0=auto) [%u]"),     awtrix_config.brightness);
  AddLog (LOG_LEVEL_INFO, PSTR ("  awtrix_inst  <0/1> = Publication puissance instantanée [%u]"), awtrix_config.arr_flag[TIC_AWTRIX_INSTANT]);
  AddLog (LOG_LEVEL_INFO, PSTR ("  awtrix_cos   <0/1> = Publication Cos φ [%u]"),                 awtrix_config.arr_flag[TIC_AWTRIX_COSPHI]);
  AddLog (LOG_LEVEL_INFO, PSTR ("  awtrix_cwh   <0/1> = Publication consommation du jour [%u]"),  awtrix_config.arr_flag[TIC_AWTRIX_CONSO_WH]);
  AddLog (LOG_LEVEL_INFO, PSTR ("  awtrix_pwh   <0/1> = Publication production du jour [%u]"),    awtrix_config.arr_flag[TIC_AWTRIX_PROD_WH]);
  AddLog (LOG_LEVEL_INFO, PSTR ("  awtrix_cal   <0/1> = Publication calendrier [%u]"),            awtrix_config.arr_flag[TIC_AWTRIX_CALENDAR]);
  ResponseCmndDone();
}

void CmndTeleinfoAwtrixType ()
{
  char str_type[8];

  // get parameter
  if (XdrvMailbox.data_len > 0)
  {
    if (strstr_P (XdrvMailbox.data, PSTR ("api")) != nullptr) awtrix_config.use_mqtt = false;
      else if (strstr_P (XdrvMailbox.data, PSTR ("mqtt")) != nullptr) awtrix_config.use_mqtt = true;
    TeleinfoAwtrixSaveConfig ();
  }
  
  // result
  if (awtrix_config.use_mqtt) strcpy_P (str_type, PSTR ("mqtt"));
    else strcpy_P (str_type, PSTR ("api"));
  ResponseCmndChar (str_type);
}

void CmndTeleinfoAwtrixAddr ()
{
  if (XdrvMailbox.data_len > 0) SettingsUpdateText (SET_TIC_AW_URL, XdrvMailbox.data);

  ResponseCmndChar (SettingsText (SET_TIC_AW_URL));
}

void CmndTeleinfoAwtrixBright ()
{
  // if brightness is valid, update and set
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload >= 0) && (XdrvMailbox.payload <= 100))
  {
    awtrix_config.brightness = (uint8_t)XdrvMailbox.payload;

    // save config
    TeleinfoAwtrixSaveConfig ();

    // ask for parameters update
    awtrix_status.init = false;
  }

  ResponseCmndNumber (awtrix_config.brightness);
}

void CmndTeleinfoAwtrixDelay ()
{
  if (XdrvMailbox.data_len > 0)
  {
    if (XdrvMailbox.payload > 1) awtrix_config.delay = (uint8_t)XdrvMailbox.payload;
    TeleinfoAwtrixSaveConfig ();
  }
  
  ResponseCmndNumber (awtrix_config.delay);
}

void CmndTeleinfoAwtrixInstant ()
{
  if (XdrvMailbox.data_len > 0)
  {
    awtrix_config.arr_flag[TIC_AWTRIX_INSTANT] = (atoi (XdrvMailbox.data) != 0);
    TeleinfoAwtrixSaveConfig ();
  }
  ResponseCmndNumber (awtrix_config.arr_flag[TIC_AWTRIX_INSTANT]);
}

void CmndTeleinfoAwtrixCosphi ()
{
  if (XdrvMailbox.data_len > 0)
  {
    awtrix_config.arr_flag[TIC_AWTRIX_COSPHI] = (atoi (XdrvMailbox.data) != 0);
    TeleinfoAwtrixSaveConfig ();
  }
  ResponseCmndNumber (awtrix_config.arr_flag[TIC_AWTRIX_COSPHI]);
}

void CmndTeleinfoAwtrixConsoWh ()
{
  if (XdrvMailbox.data_len > 0)
  {
    awtrix_config.arr_flag[TIC_AWTRIX_CONSO_WH] = (atoi (XdrvMailbox.data) != 0);
    TeleinfoAwtrixSaveConfig ();
  }
  ResponseCmndNumber (awtrix_config.arr_flag[TIC_AWTRIX_CONSO_WH]);
}

void CmndTeleinfoAwtrixProdWh ()
{
  if (XdrvMailbox.data_len > 0)
  {
    awtrix_config.arr_flag[TIC_AWTRIX_PROD_WH] = (atoi (XdrvMailbox.data) != 0);
    TeleinfoAwtrixSaveConfig ();
  }
  ResponseCmndNumber (awtrix_config.arr_flag[TIC_AWTRIX_PROD_WH]);
}

void CmndTeleinfoAwtrixCalendar ()
{
  if (XdrvMailbox.data_len > 0)
  {
    awtrix_config.arr_flag[TIC_AWTRIX_CALENDAR] = (atoi (XdrvMailbox.data) != 0);
    TeleinfoAwtrixSaveConfig ();
  }
  ResponseCmndNumber (awtrix_config.arr_flag[TIC_AWTRIX_CALENDAR]);
}

/**************************************************\
 *                 Configuration
\**************************************************/

void TeleinfoAwtrixLoadConfig ()
{
  uint8_t index, version;
  char    str_filename[24];
  File    file;

  // init device address
  awtrix_config.str_device[0] = 0;

  // init default colors
  strcpy_P (awtrix_config.str_prod, kTeleinfoAwtrixProdColor);
  for (index = 0; index < TIC_AWTRIX_PMAX; index ++)
  {
    GetTextIndexed (awtrix_config.str_conso[index][1], 8, index, kTeleinfoAwtrixConsoColors);
    TeleinfoAwtrixCalculateColor (awtrix_config.str_conso[index][0], awtrix_config.str_conso[index][1], 8, 2);
  }

  // load data from settings
  awtrix_config.delay = Settings->rf_code[12][0];
  if (awtrix_config.delay == 0) awtrix_config.delay = AWTRIX_DELAY_DEFAULT;
  awtrix_config.brightness = Settings->rf_code[12][2];
  if (awtrix_config.brightness > 100) awtrix_config.brightness = 0;
  strcpy (awtrix_config.str_device, SettingsText (SET_TIC_AW_URL));
  for (index = 0; index < TIC_AWTRIX_DMAX; index ++) awtrix_config.arr_flag[index] = (bool)Settings->rf_code[12][index + 3];

  // open file in read mode
  strcpy_P (str_filename, PSTR_AWTRIX_DATA_FILE);
  if (ffsp->exists (str_filename))
  {
    file = ffsp->open (str_filename, "r");
    file.read ((uint8_t*)&version, sizeof (version));
    if (version == AWTRIX_FILE_VERSION)
    {
      file.read ((uint8_t*)&awtrix_config, sizeof (awtrix_config));
    }
    else AddLog (LOG_LEVEL_INFO, PSTR ("AWT: Attention, format de stockage different. Configuration Awtrix réinitialisée !"));
    file.close ();
  }
}

void TeleinfoAwtrixSaveConfig ()
{
  uint8_t version;
  char    str_filename[24];
  File    file;

  // init data
  version = AWTRIX_FILE_VERSION;

  // open file in write mode
  strcpy_P (str_filename, PSTR_AWTRIX_DATA_FILE);
  file = ffsp->open (str_filename, "w");
  if (file > 0)
  {
    file.write ((uint8_t*)&version, sizeof (version));
    file.write ((uint8_t*)&awtrix_config, sizeof (awtrix_config));
    file.close ();
  }
}

/**************************************************\
 *                  Functions
\**************************************************/

// send instant power to Awtrix display
void TeleinfoAwtrixCalculateColor (char *pstr_target, const char *pstr_origin, const size_t size_color, const long divider)
{
  long rgb_r, rgb_g, rgb_b;
  char str_color[8];

  // check parameters
  if (divider == 0) return;
  if (size_color < 8) return;
  if (pstr_target == nullptr) return;
  if (pstr_origin == nullptr) return;

  // calculation of background color
  strcpy (str_color, pstr_origin);
  rgb_b = strtol (str_color + 5, 0, 16) / divider; str_color[5] = 0;
  rgb_g = strtol (str_color + 3, 0, 16) / divider; str_color[3] = 0;
  rgb_r = strtol (str_color + 1, 0, 16) / divider; str_color[1] = 0;
  snprintf_P (pstr_target, size_color, PSTR ("#%X%X%X%X%X%X"), rgb_r / 16, rgb_r % 16, rgb_g / 16, rgb_g % 16, rgb_b / 16, rgb_b % 16);
}

// send realtime conso to Awtrix display
uint8_t TeleinfoAwtrixGenerateDigit (const uint32_t value, char *pstr_digit, const size_t size_digit, char *pstr_unit, const size_t size_unit)
{
  uint8_t offset;

  if (pstr_digit == nullptr) return UINT8_MAX;
  if (pstr_unit == nullptr)  return UINT8_MAX;
  if (size_digit < 5) return UINT8_MAX;
  if (size_unit < 3)  return UINT8_MAX;
  
  // 100 000 and more : 100kW
  if (value >= 100000)
  {
    sprintf_P (pstr_digit, PSTR ("%u"), value / 1000);
    strcpy_P (pstr_unit, "kW");
    offset = 3; 
  }

  // 10 000 - 100 000 : 10.2 kW
  else if (value >= 10000)
  {
    sprintf_P (pstr_digit, PSTR ("%u.%u"), value / 1000, value % 1000 / 100);
    strcpy_P (pstr_unit, "kW");
    offset = 1; 
  }

  // 1 000 - 10 000 : 8927 W
  else if (value >= 1000)
  {
    sprintf_P (pstr_digit, PSTR ("%u"), value);
    strcpy_P (pstr_unit, " W");
    offset = 1; 
  }

  // 100 - 1 000 : 670 W
  else if (value >= 100)
  {
    sprintf_P (pstr_digit, PSTR ("%u"), value);
    strcpy_P (pstr_unit, " W");
    offset = 5; 
  }

  // 10 - 100 : 76 W
  else if (value >= 10)
  {
    sprintf_P (pstr_digit, PSTR ("%u"), value);
    strcpy_P (pstr_unit, " W");
    offset = 9; 
  }

  // 0 - 9 : 6 W
  else
  {
    sprintf_P (pstr_digit, PSTR ("%u"), value);
    strcpy_P (pstr_unit, " W");
    offset = 13; 
  }

  return offset;
}

// publish data thru REST API
void TeleinfoAwtrixPublishAPI (const uint8_t type)
{
  const char      *pstr_type;
  char             str_url[48];
  HTTPClientLight *phttp_client;

  // check parameter
  if (type >= TIC_AWTRIX_TYPE_MAX) return;

  // set URL type
  if (type == TIC_AWTRIX_TYPE_SETTING) pstr_type = PSTR ("settings");
    else pstr_type = PSTR ("custom?name=tic");

  // declare publication
  TeleinfoDriverWebDeclare (TIC_WEB_HTTP);

  // generate URL
  sprintf_P (str_url, PSTR ("http://%s/api/%s"), awtrix_config.str_device, pstr_type); 

  // call awtrix device
  phttp_client = new HTTPClientLight ();  
  phttp_client->begin (str_url);

  // send command
  phttp_client->setConnectTimeout (AWTRIX_CONNECT_TIMEOUT);
  phttp_client->setTimeout (AWTRIX_ANSWER_TIMEOUT);
  phttp_client->addHeader (F ("Content-Type"), F ("application/json"));
  phttp_client->POST (TasmotaGlobal.mqtt_data);

  // end connexion
  phttp_client->end ();
  delete phttp_client;

  // log
  AddLog (LOG_LEVEL_DEBUG, PSTR ("API: %s = %s"), str_url, ResponseData ());
  ResponseClear ();
}

// publish data thru REST API
void TeleinfoAwtrixPublishMQTT (const uint8_t type)
{
  const char *pstr_type;
  char        str_topic[48];

  // check parameter
  if (type >= TIC_AWTRIX_TYPE_MAX) return;

  // set topic type
  if (type == TIC_AWTRIX_TYPE_SETTING) pstr_type = PSTR ("settings");
    else pstr_type = PSTR ("custom/tic");

  // generate topic
  snprintf_P (str_topic, sizeof (str_topic), PSTR ("%s/%s"), awtrix_config.str_device, pstr_type);

  // publish awtrix topic
  MqttPublish (str_topic, false);
  TeleinfoDriverWebDeclare (TIC_WEB_MQTT);
  ResponseClear ();
}

// publish data
void TeleinfoAwtrixPublish (const uint8_t type)
{
  // if no target, ignore
  if (strlen (awtrix_config.str_device) == 0) return;

  // set next update
  awtrix_config.time_upd = LocalTime () + awtrix_config.delay;

  // publish thru API or MQTT
  if (awtrix_config.use_mqtt) TeleinfoAwtrixPublishMQTT (type);
    else TeleinfoAwtrixPublishAPI (type);
  }

// send realtime conso to Awtrix display
bool TeleinfoAwtrixUpdateParameters ()
{
  // disable default apps et set default parameters
  Response_P (PSTR ("{'TIM':false,'DAT':false,'HUM':false,'TEMP':false,'BAT':false,'ATIME':%u,'TSPEED':%u,'TEFF':%u"), awtrix_config.delay, awtrix_config.tr_speed, awtrix_config.tr_effect);

  // set auto-brightness, or if fixed brightness, convert from percentage to 0..255
  if (awtrix_config.brightness == 0) ResponseAppend_P (PSTR (",'ABRI':true}"));
    else ResponseAppend_P (PSTR (",'ABRI':false,'BRI':%u}"), (uint16_t)awtrix_config.brightness * 255 / 100);

  // publish
  TeleinfoAwtrixPublish (TIC_AWTRIX_TYPE_SETTING);

  return true;
}

// send instant power to Awtrix display
void TeleinfoAwtrixUpdateInstant ()
{
  uint8_t mode, phase, nb_phase, offset, level, hchp;
  long    power, digit, height, width;
  long    bar_unit, bar_width, bar_left;
  char    str_unit[4];
  char    str_digit[8];
  char    str_bkgrd[8];
  char    str_color[8];

  // check environment
  if (teleinfo_prod.pact > 0) mode = TIC_AWTRIX_MODE_PROD;
  else if (teleinfo_conso.enabled && (teleinfo_contract.ssousc > 0)) mode = TIC_AWTRIX_MODE_CONSO;
  else return;

  // set environment
  bar_left = 26;
  if (mode == TIC_AWTRIX_MODE_PROD) 
  {
    // set phase and bar width
    nb_phase = 1;
    bar_width = 6; 
    bar_unit = teleinfo_config.prod_max / bar_width / 8;

    // get current power
    power = teleinfo_prod.pact;

    // set bar colors
    strcpy (str_color, awtrix_config.str_prod);
  }

  else
  {
    // set phase and bar width
    nb_phase = teleinfo_contract.phase;
    if (nb_phase == 1) bar_width = 6; else bar_width = 2;   // 1 phase : width = 6, 3 phases : width = 2
    bar_unit = teleinfo_contract.ssousc / bar_width / 8;    // 1 phase : 48 units,  3 phases : 16 unit, 

    // get current power
    power = teleinfo_conso.pact;

    // calculate bar color
    level = TeleinfoContractGetPeriodLevel ();
    hchp  = TeleinfoContractGetPeriodHP ();
    strcpy (str_color, awtrix_config.str_conso[level][hchp]);
  }

  // calculation of background color
  TeleinfoAwtrixCalculateColor (str_bkgrd, str_color, sizeof (str_bkgrd), 3);

  // display power value
  offset = TeleinfoAwtrixGenerateDigit (power, str_digit, sizeof (str_digit), str_unit, sizeof (str_unit));
  ResponseAppend_P (PSTR ("{\"textOffset\":%u,\"textCase\":2,\"center\":false,\"text\":[{\"t\":\"%s\"},{\"t\":\"%s\",\"c\":\"%s\"}]"), offset, str_digit, str_unit, kTeleinfoAwtrixUnitColor);

  // start of display
  ResponseAppend_P (PSTR (",\"draw\":[{\"df\":[%d,%d,%d,%d,\"%s\"]}"), bar_left, 0, bar_left + 6, 8, str_bkgrd);
  
  // loop thru phase
  for (phase = 0; phase < nb_phase; phase ++)
  {
    // calculate number of digits in the bargraph
    if (mode == TIC_AWTRIX_MODE_PROD) digit = teleinfo_prod.papp / bar_unit;
      else digit = teleinfo_conso.phase[phase].papp / bar_unit;

    // calculate bargraph data
    width  = digit % bar_width;
    height = min ((digit - width) / bar_width, 8L);

    // display bargraph : upper part and lower block
    if ((height < 8) && (width > 0)) ResponseAppend_P (PSTR (",{\"dl\":[%d,%d,%d,%d,\"%s\"]}"), bar_left, 7 - height, bar_left + width, 7 - height, str_color);
    if (height > 0) ResponseAppend_P (PSTR (",{\"df\":[%d,%d,%d,%d,\"%s\"]}"), bar_left, 8 - height, bar_width, height, str_color);

    // shift to next bar
    bar_left += bar_width; 
  }

  // end of display
  ResponseAppend_P (PSTR ("]}"));
}

// send cosphi to Awtrix display
void TeleinfoAwtrixUpdateCosphi ()
{
  long cosphi;

  // if in production mode
  if (teleinfo_prod.pact > 0) cosphi = teleinfo_prod.cosphi.value;
    else if (teleinfo_conso.enabled && (teleinfo_contract.ssousc > 0)) cosphi = teleinfo_conso.cosphi.value;
    else return;

  // start of display
  ResponseAppend_P (PSTR ("{"));

  // display cosphi value
  ResponseAppend_P (PSTR ("\"textOffset\":3,\"textCase\":2,\"center\":false,\"text\":[{\"t\":\"%d.%02d\"}]"), cosphi / 1000, cosphi % 1000 / 10);

  // start of draw
  ResponseAppend_P (PSTR (",\"draw\":["));

  // display phi unit
  ResponseAppend_P (PSTR ("{\"dl\":[19,1,19,2,\"#404040\"]},{\"dl\":[20,3,22,3,\"#404040\"]},{\"dl\":[21,1,21,5,\"#404040\"]},{\"dl\":[22,1,23,2,\"#404040\"]}"));

  // end of draw
  ResponseAppend_P (PSTR ("]"));

  // end of display
  ResponseAppend_P (PSTR ("}"));
}

void TeleinfoAwtrixUpdateConsoWh ()
{
  uint8_t offset;
  char    str_unit[4];
  char    str_digit[8];

  // calculate value to display
  offset = TeleinfoAwtrixGenerateDigit (teleinfo_conso_wh.today, str_digit, sizeof (str_digit), str_unit, sizeof (str_unit));

  // display value
  ResponseAppend_P (PSTR ("{\"textOffset\":%u,\"textCase\":2,\"center\":false,\"text\":[{\"t\":\"%s\"},{\"t\":\"%sh\",\"c\":\"%s\"}]}"), offset, str_digit, str_unit, kTeleinfoAwtrixUnitColor);
}

void TeleinfoAwtrixUpdateProdWh ()
{
  uint8_t offset;
  char    str_unit[4];
  char    str_digit[8];

  // calculate value to display
  offset = TeleinfoAwtrixGenerateDigit (teleinfo_prod_wh.today, str_digit, sizeof (str_digit), str_unit, sizeof (str_unit));

  // display value
  ResponseAppend_P (PSTR ("{\"textOffset\":%u,\"textCase\":2,\"center\":false,\"text\":[{\"t\":\"%s\",\"c\":\"%s\"},{\"t\":\"%sh\",\"c\":\"%s\"}]}"), offset, str_digit, awtrix_config.str_prod, str_unit, kTeleinfoAwtrixUnitColor);
}

void TeleinfoAwtrixUpdateCalendar ()
{
  uint8_t  hchp_now, level_now, level_tday, level_tmrw;
  uint32_t current_time;
  TIME_T   time_dst;
  char     str_label[4];
  char     str_color[8];

  // get time
  current_time = LocalTime ();

  // current period logo
  level_now  = TeleinfoContractGetPeriodLevel ();
  level_tday = TeleinfoDriverCalendarGetLevel (TIC_DAY_TODAY, UINT8_MAX, false);
  level_tmrw = TeleinfoDriverCalendarGetLevel (TIC_DAY_TMROW, UINT8_MAX, false);
  hchp_now   = TeleinfoContractGetPeriodHP ();

  // init display
  ResponseAppend_P (PSTR ("{\"draw\":["));

  // today's color logo
  ResponseAppend_P (PSTR ("{\"dl\":[12,0,19,0,\"%s\"]},{\"df\":[11,1,9,6,\"%s\"]}"), awtrix_config.str_conso[level_tday][1], awtrix_config.str_conso[level_tday][1]);

  // tomorrow's color logo
  ResponseAppend_P (PSTR (",{\"dl\":[22,0,29,0,\"%s\"]},{\"df\":[21,1,9,6,\"%s\"]}"), awtrix_config.str_conso[level_tmrw][1], awtrix_config.str_conso[level_tmrw][1]);

  // set current HC/HP label
  GetTextIndexed (str_label, sizeof (str_label), hchp_now, kTeleinfoHourShort);
  ResponseAppend_P (PSTR ("],\"text\":[{\"t\":\"%s \",\"c\":\"%s\"}"), str_label, awtrix_config.str_conso[level_now][1]);

  // set today's date
  BreakTime (current_time, time_dst);
  sprintf_P (str_label, PSTR ("%02u"), time_dst.day_of_month);
  GetTextIndexed (str_color, sizeof (str_color), level_tday, kTeleinfoAwtrixTextColors);
  ResponseAppend_P (PSTR (",{\"t\":\"%s \",\"c\":\"%s\"}"), str_label, str_color);
    
  // set tomorrow's date
  BreakTime (current_time + 86400, time_dst);
  sprintf_P (str_label, PSTR ("%02u"), time_dst.day_of_month);
  GetTextIndexed (str_color, sizeof (str_color), level_tmrw, kTeleinfoAwtrixTextColors);
  ResponseAppend_P (PSTR (",{\"t\":\"%s\",\"c\":\"%s\"}"), str_label, str_color);
  
  // end display
  ResponseAppend_P (PSTR ("],\"topText\":true}"));
}

#ifdef USE_TELEINFO_RTE

void TeleinfoAwtrixUpdateCalendarTempo ()
{
  uint8_t index, level;
  char    str_label[4];
  char    str_color[8];
//  TIME_T   time_dst;

  // init display
  ResponseAppend_P (PSTR ("{\"draw\":["));

  // today's color
  level = TeleinfoRteTempoGetDailyLevel (RTE_DAY_TODAY);
  ResponseAppend_P (PSTR ("{\"dl\":[1,0,8,0,\"%s\"]},{\"df\":[0,1,9,8,\"%s\"]}"), awtrix_config.str_conso[level][1], awtrix_config.str_conso[level][1]);

  // today's hc/hp
//  BreakTime (LocalTime (), time_dst);
//  sprintf_P (str_label, PSTR ("%02u"), time_dst.day_of_month);
  GetTextIndexed (str_label, sizeof (str_label), TeleinfoContractGetPeriodHP (), kTeleinfoHourShort);
  GetTextIndexed (str_color, sizeof (str_color), TeleinfoRteTempoGetDailyLevel (RTE_DAY_TODAY), kTeleinfoAwtrixTextColors);
  ResponseAppend_P (PSTR (",{\"dt\":[1,2,\"%s\",\"%s\"]}"), str_label, str_color);

  // tomorrow's color
  level = TeleinfoRteTempoGetDailyLevel (RTE_DAY_TOMORROW);
  ResponseAppend_P (PSTR (",{\"df\":[10,1,4,7,\"%s\"]}"), awtrix_config.str_conso[level][1]);

  // day after tomorrow's color
  level = TeleinfoRteTempoGetDailyLevel (RTE_DAY_PLUS2);
  ResponseAppend_P (PSTR (",{\"df\":[15,1,3,7,\"%s\"]}"), awtrix_config.str_conso[level][1]);

  // day+3 ... day+6 color
  for (index = 0; index < 4; index ++)
  {
    level = TeleinfoRteTempoGetDailyLevel (RTE_DAY_PLUS3 + index);
    ResponseAppend_P (PSTR (",{\"df\":[%u,1,2,7,\"%s\"]}"), 19 + index * 3, awtrix_config.str_conso[level][1]);
  }

  // end display
  ResponseAppend_P (PSTR ("]}"));
}

#endif  // USE_TELEINFO_RTE

void TeleinfoAwtrixUpdatePage ()
{
  bool    publish;
  uint8_t index;

  // check if anything to publish
  publish = false;
  for (index = 0; index < TIC_AWTRIX_DMAX; index ++) publish |= awtrix_config.arr_flag[index];
  if (!publish) return;

  // init display
  Response_P (PSTR ("["));

  // loop to find next data to display
  publish = false;
  for (index = 0; index < TIC_AWTRIX_DMAX; index ++)
    if (awtrix_config.arr_flag[index])
    {
      // append separator
      if (publish) ResponseAppend_P (PSTR (","));
      publish = true;

      // add data
      switch (index)
      {
        case TIC_AWTRIX_INSTANT:  TeleinfoAwtrixUpdateInstant (); break;
        case TIC_AWTRIX_CONSO_WH: TeleinfoAwtrixUpdateConsoWh (); break;
        case TIC_AWTRIX_PROD_WH:  TeleinfoAwtrixUpdateProdWh  (); break;
        case TIC_AWTRIX_COSPHI:   TeleinfoAwtrixUpdateCosphi  (); break;
        case TIC_AWTRIX_CALENDAR: 
#ifdef USE_TELEINFO_RTE
          if (rte_config.tempo.enabled) TeleinfoAwtrixUpdateCalendarTempo (); else
#endif  // USE_TELEINFO_RTE
          TeleinfoAwtrixUpdateCalendar ();
          break;
      }
    }

  // end of data
  ResponseAppend_P (PSTR ("]"));

  // publish
  TeleinfoAwtrixPublish (TIC_AWTRIX_TYPE_DATA);
}

// send realtime conso to Awtrix display
void TeleinfoAwtrixEverySecond ()
{
  // if environment not ready, ignore
  if (teleinfo_meter.nb_message < TIC_MESSAGE_MIN) return;
  if (!RtcTime.valid) return;

  // check if publication can be done
  if (awtrix_config.time_upd > LocalTime ()) return;
  if (!TeleinfoDriverWebAllow (TIC_WEB_HTTP)) return;

  // if needed display parameters, else 
  if (!awtrix_status.init) awtrix_status.init = TeleinfoAwtrixUpdateParameters ();

  // else, display data
  else TeleinfoAwtrixUpdatePage ();
}

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

// Append InfluxDB configuration button
void TeleinfoAwtrixWebConfigButton ()
{
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), PSTR (AWTRIX_PAGE_CFG), PSTR ("Afficheur Awtrix"));
}

// Teleinfo web page
void TeleinfoAwtrixWebPageConfigure ()
{
  char str_value[32];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess ()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg (F ("save")))
  {
    // retrieve parameters
    if (Webserver->hasArg (F ("addr")))   { WebGetArg (PSTR ("addr"),   str_value, sizeof (str_value)); strcpy (awtrix_config.str_device, str_value);          }
    if (Webserver->hasArg (F ("link")))   { WebGetArg (PSTR ("link"),   str_value, sizeof (str_value)); awtrix_config.use_mqtt   = (atoi (str_value) != 0);    }
    if (Webserver->hasArg (F ("delay")))  { WebGetArg (PSTR ("delay"),  str_value, sizeof (str_value)); awtrix_config.delay      = (uint8_t)atoi (str_value);  }
    if (Webserver->hasArg (F ("lumi")))   { WebGetArg (PSTR ("lumi"),   str_value, sizeof (str_value)); awtrix_config.brightness = (uint8_t)atoi (str_value);  }
    if (Webserver->hasArg (F ("effect"))) { WebGetArg (PSTR ("effect"), str_value, sizeof (str_value)); awtrix_config.tr_effect  = (uint8_t)atoi (str_value);  }
    if (Webserver->hasArg (F ("speed")))  { WebGetArg (PSTR ("speed"),  str_value, sizeof (str_value)); awtrix_config.tr_speed   = (uint16_t)atoi (str_value); }

    // update publication flags
    awtrix_config.arr_flag[TIC_AWTRIX_INSTANT]  = Webserver->hasArg (F ("inst"));
    awtrix_config.arr_flag[TIC_AWTRIX_COSPHI]   = Webserver->hasArg (F ("cos"));
    awtrix_config.arr_flag[TIC_AWTRIX_CONSO_WH] = Webserver->hasArg (F ("cwh"));
    awtrix_config.arr_flag[TIC_AWTRIX_PROD_WH]  = Webserver->hasArg (F ("pwh"));
    awtrix_config.arr_flag[TIC_AWTRIX_CALENDAR] = Webserver->hasArg (F ("cal"));

    // update colors
    if (Webserver->hasArg (F ("bc"))) { WebGetArg (PSTR ("bc"), str_value, sizeof (str_value)); strcpy (awtrix_config.str_conso[0][0], str_value); strcpy (awtrix_config.str_conso[1][0], str_value); }
    if (Webserver->hasArg (F ("bp"))) { WebGetArg (PSTR ("bp"), str_value, sizeof (str_value)); strcpy (awtrix_config.str_conso[0][1], str_value); strcpy (awtrix_config.str_conso[1][1], str_value); }
    if (Webserver->hasArg (F ("wc"))) { WebGetArg (PSTR ("wc"), str_value, sizeof (str_value)); strcpy (awtrix_config.str_conso[2][0], str_value); }
    if (Webserver->hasArg (F ("wp"))) { WebGetArg (PSTR ("wp"), str_value, sizeof (str_value)); strcpy (awtrix_config.str_conso[2][1], str_value); }
    if (Webserver->hasArg (F ("rc"))) { WebGetArg (PSTR ("rc"), str_value, sizeof (str_value)); strcpy (awtrix_config.str_conso[3][0], str_value); }
    if (Webserver->hasArg (F ("rp"))) { WebGetArg (PSTR ("rp"), str_value, sizeof (str_value)); strcpy (awtrix_config.str_conso[3][1], str_value); }
    if (Webserver->hasArg (F ("pr"))) { WebGetArg (PSTR ("pr"), str_value, sizeof (str_value)); strcpy (awtrix_config.str_prod,        str_value); }

    // save config
    TeleinfoAwtrixSaveConfig ();

    // reset data
    Response_P (PSTR (""));
    TeleinfoAwtrixPublish (TIC_AWTRIX_TYPE_DATA);

    // ask for parameters update
    awtrix_status.init = false;
  }

  // beginning of form
  WSContentStart_P (PSTR ("Configure Awtrix"));

    // page style
  WSContentSendStyle ();
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("p,br,hr {clear:both;}\n"));
  WSContentSend_P (PSTR ("fieldset {background:#333;margin:15px 0px;border:none;border-radius:8px;}\n")); 
  WSContentSend_P (PSTR ("legend {font-weight:bold;margin-bottom:-30px;padding:5px;color:#888;background:transparent;}\n")); 
  WSContentSend_P (PSTR ("legend:after {content:'';display:block;height:1px;margin:15px 0px;}\n"));
  WSContentSend_P (PSTR ("input,select {margin-bottom:10px;text-align:center;border:none;border-radius:4px;}\n"));
  WSContentSend_P (PSTR ("input[type='checkbox'],input[type='radio'] {margin:0px 15px;}\n"));
  WSContentSend_P (PSTR ("p.dat {padding:2px;}\n"));
  WSContentSend_P (PSTR ("span {float:left;padding-top:4px;}\n"));
  WSContentSend_P (PSTR ("span.full {width:100%%;}\n"));
  WSContentSend_P (PSTR ("span.head {width:55%%;}\n"));
  WSContentSend_P (PSTR ("span.val {width:30%%;padding:0px;}\n"));
  WSContentSend_P (PSTR ("span.sel {width:40%%;padding:0px;}\n"));
  WSContentSend_P (PSTR ("span.unit {width:15%%;text-align:center;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // form
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), PSTR (AWTRIX_PAGE_CFG));

  // display address
  WSContentSend_P (PSTR ("<fieldset><legend>Afficheur Awtrix</legend>\n"));
  if (!awtrix_config.use_mqtt) strcpy_P (str_value, PSTR ("checked")); else str_value[0] = 0;
  WSContentSend_P (PSTR ("<p class='dat'><input type='radio' name='link' value=%u %s><label for='%s'>%s</label></p>\n"), 0, str_value, PSTR ("api"),  PSTR ("API <small>(Adresse IP)</small>"));
  if (awtrix_config.use_mqtt) strcpy_P (str_value, PSTR ("checked")); else str_value[0] = 0;
  WSContentSend_P (PSTR ("<p class='dat'><input type='radio' name='link' value=%u %s><label for='%s'>%s</label></p>\n"), 1, str_value, PSTR ("mqtt"), PSTR ("MQTT"));
  WSContentSend_P (PSTR ("<p class='dat'><span class='full'>Addresse IP / Topic MQTT</span><br /><span class='full'><input type='text' name='addr' value='%s'></span></p>\n"), awtrix_config.str_device);
  WSContentSend_P (PSTR ("</fieldset>\n"));

  // parameters
  WSContentSend_P (PSTR ("<fieldset><legend>Paramètres</legend>\n"));
  WSContentSend_P (PSTR ("<p class='dat'><span class='head'>%s</span><span class='val'><input type='number' name='%s' min=%u max=%u step=%u value=%u></span><span class='unit'>%s</span></p>\n"), PSTR ("Luminosité <small>(0=auto)</small>"), PSTR ("lumi"),   0, 100,  1,   awtrix_config.brightness, PSTR ("%%"));
  WSContentSend_P (PSTR ("<p class='dat'><span class='head'>%s</span><span class='val'><input type='number' name='%s' min=%u max=%u step=%u value=%u></span><span class='unit'>%s</span></p>\n"), PSTR ("Délai entre pages"),                  PSTR ("delay"),  4, 255,  1,   awtrix_config.delay,      PSTR ("sec."));
  WSContentSend_P (PSTR ("<p class='dat'><span class='head'>%s</span><span class='val'><input type='number' name='%s' min=%u max=%u step=%u value=%u></span><span class='unit'>%s</span></p>\n"), PSTR ("Durée de transition"),                PSTR ("speed"),  0, 5000, 100, awtrix_config.tr_speed,   PSTR ("ms"));
  WSContentSend_P (PSTR ("<p class='dat'><span class='head'>%s</span><span class='val'><input type='number' name='%s' min=%u max=%u step=%u value=%u></span><span class='unit'>%s</span></p>\n"), PSTR ("Effet de transition"),                PSTR ("effect"), 0, 10,   1,   awtrix_config.tr_effect,  PSTR (""));
  WSContentSend_P (PSTR ("</fieldset>\n"));

  // data
  WSContentSend_P (PSTR ("<fieldset><legend>Données affichées</legend>\n"));
  if (awtrix_config.arr_flag[TIC_AWTRIX_INSTANT]) strcpy_P (str_value, PSTR ("checked")); else str_value[0] = 0;  
  WSContentSend_P (PSTR ("<p class='dat'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), PSTR ("inst"), str_value, PSTR ("inst"), PSTR ("Puissance instantanée"));
  if (awtrix_config.arr_flag[TIC_AWTRIX_COSPHI]) strcpy_P (str_value, PSTR ("checked")); else str_value[0] = 0;  
  WSContentSend_P (PSTR ("<p class='dat'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), PSTR ("cos"), str_value,  PSTR ("cos"),  PSTR ("Cos φ"));
  if (awtrix_config.arr_flag[TIC_AWTRIX_CONSO_WH]) strcpy_P (str_value, PSTR ("checked")); else str_value[0] = 0;  
  WSContentSend_P (PSTR ("<p class='dat'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), PSTR ("cwh"),  str_value, PSTR ("cwh"),  PSTR ("Consommation du jour"));
  if (awtrix_config.arr_flag[TIC_AWTRIX_PROD_WH]) strcpy_P (str_value, PSTR ("checked")); else str_value[0] = 0;  
  WSContentSend_P (PSTR ("<p class='dat'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), PSTR ("pwh"),  str_value, PSTR ("pwh"),  PSTR ("Production du jour"));
  if (awtrix_config.arr_flag[TIC_AWTRIX_CALENDAR]) strcpy_P (str_value, PSTR ("checked")); else str_value[0] = 0;  
  WSContentSend_P (PSTR ("<p class='dat'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), PSTR ("cal"),  str_value, PSTR ("cal"),  PSTR ("Calendrier"));
  WSContentSend_P (PSTR ("</fieldset>\n"));

  // colors
  WSContentSend_P (PSTR ("<fieldset><legend>Couleurs</legend>\n"));
  WSContentSend_P (PSTR ("<p class='dat'><span class='head'>%s</span><span class='val'><input type='color' name='%s' value='%s'></span></p>\n"), PSTR ("Creuses Bleu"),  PSTR ("bc"), awtrix_config.str_conso[1][0]);
  WSContentSend_P (PSTR ("<p class='dat'><span class='head'>%s</span><span class='val'><input type='color' name='%s' value='%s'></span></p>\n"), PSTR ("Pleines Bleu"),  PSTR ("bp"), awtrix_config.str_conso[1][1]);
  WSContentSend_P (PSTR ("<p class='dat'><span class='head'>%s</span><span class='val'><input type='color' name='%s' value='%s'></span></p>\n"), PSTR ("Creuses Blanc"), PSTR ("wc"), awtrix_config.str_conso[2][0]);
  WSContentSend_P (PSTR ("<p class='dat'><span class='head'>%s</span><span class='val'><input type='color' name='%s' value='%s'></span></p>\n"), PSTR ("Pleines Blanc"), PSTR ("wp"), awtrix_config.str_conso[2][1]);
  WSContentSend_P (PSTR ("<p class='dat'><span class='head'>%s</span><span class='val'><input type='color' name='%s' value='%s'></span></p>\n"), PSTR ("Creuses Rouge"), PSTR ("rc"), awtrix_config.str_conso[3][0]);
  WSContentSend_P (PSTR ("<p class='dat'><span class='head'>%s</span><span class='val'><input type='color' name='%s' value='%s'></span></p>\n"), PSTR ("Pleines Rouge"), PSTR ("rp"), awtrix_config.str_conso[3][1]);
  WSContentSend_P (PSTR ("<p class='dat'><span class='head'>%s</span><span class='val'><input type='color' name='%s' value='%s'></span></p>\n"), PSTR ("Production"),    PSTR ("pr"), awtrix_config.str_prod);
  WSContentSend_P (PSTR ("</fieldset>\n"));

  // End of page
  // -----------

  // save button
  WSContentSend_P (PSTR ("<br><button name='save' type='submit' class='button bgrn'>%s</button>"), PSTR (D_SAVE));
  WSContentSend_P (PSTR ("</form>\n"));

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

#endif    // USE_WEBSERVER


/***************************************\
 *              Interface
\***************************************/

bool XdrvTeleinfoAwtrix (const uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
    case FUNC_COMMAND:
      result = DecodeCommand (kTeleinfoAwtrixCommands, TeleinfoAwtrixCommand);
      break;

    case FUNC_INIT:
      TeleinfoAwtrixLoadConfig ();
      break;

    case FUNC_EVERY_SECOND:
      TeleinfoAwtrixEverySecond ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_BUTTON:
      TeleinfoAwtrixWebConfigButton ();
      break;

    case FUNC_WEB_ADD_HANDLER:
      Webserver->on (F (AWTRIX_PAGE_CFG), TeleinfoAwtrixWebPageConfigure);
      break;
#endif    // USE_WEBSERVER
  }

  return result;
}

#endif      // USE_TELEINFO_AWTRIX
#endif      // USE_TELEINFO
#endif      // ESP32
