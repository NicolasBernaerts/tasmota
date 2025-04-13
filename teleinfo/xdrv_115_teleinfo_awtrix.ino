/*
  xdrv_115_teleinfo_awtrix.ino - Handle Awtrix display (Ulanzi TC001)
  Only compatible with ESP32

  Copyright (C) 2023  Nicolas Bernaerts
    15/02/2025 - v1.0 - Creation

  Configuration values are stored in :

  - Settings->rf_code[12][0] : delay between page display (sec.)
  - Settings->rf_code[12][1] : Production power display limit (x100)
  - Settings->rf_code[12][2] : Fixed brightness (1..100%) / 0 for auto brightness
  
  - Settings->rf_code[12][3] : Flag to display instant power
  - Settings->rf_code[12][4] : Flag to display calendar
  - Settings->rf_code[12][5] : Flag to display today's conso
  - Settings->rf_code[12][6] : Flag to display today's production

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
#ifdef USE_AWTRIX

#define XDRV_115                        115

#include <ArduinoJson.h>

// web URL
#define AWTRIX_PAGE_CFG                "/awtrix"

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


// display type
enum TeleinfoAwtrixMode    { TIC_AWTRIX_MODE_CONSO, TIC_AWTRIX_MODE_PROD, TIC_AWTRIX_MODE_MAX };
enum TeleinfoAwtrixType    { TIC_AWTRIX_TYPE_SETTING, TIC_AWTRIX_TYPE_CUSTOM, TIC_AWTRIX_TYPE_MAX };
enum TeleinfoAwtrixDisplay { TIC_AWTRIX_INSTANT, TIC_AWTRIX_CONSO_WH, TIC_AWTRIX_PROD_WH, TIC_AWTRIX_CALENDAR, TIC_AWTRIX_MAX };

// awtrix commands
const char kTicAwtrixCommands[]         PROGMEM = "awtrix|"          "|"      "_addr"    "|"       "_lumi"     "|"      "_delai"    "|"       "_inst"      "|"       "_cwh"       "|"        "_pwh"     "|"       "_cal"        "|"      "_pmax";
void (* const TicAwtrixCommand[])(void) PROGMEM = { &CmndTicAwtrixHelp, &CmndTicAwtrixAddr, &CmndTicAwtrixBright, &CmndTicAwtrixDelay, &CmndTicAwtrixInstant, &CmndTicAwtrixConsoWh, &CmndTicAwtrixProdWh, &CmndTicAwtrixCalendar, &CmndTicAwtrixProdMax };

// awtrix conso bar graph colors
const char kTicAwtrixTextColors[]  PROGMEM = "#FFFFFF|#FFFFFF|#000000|#FFFFFF";                                   // conso : undef, blue, white, red
const char kTicAwtrixConsoBckgrd[] PROGMEM = "#000000|#000040|#404040|#400000";                                   // conso : undef, blue, white, red
const char kTicAwtrixConsoColors[] PROGMEM = "#000080|#0000FF|#000080|#0000FF|#808080|#FFFFFF|#800000|#FF0000";   // conso : undef HC, undef HP, blue HC, blue HP, ...
const char kTicAwtrixProdBckgrd[]  PROGMEM = "#404000";                                                           // prod
const char kTicAwtrixProdColor[]   PROGMEM = "#FFFF00";                                                           // prod
const char kTicAwtrixUnitColor[]   PROGMEM = "#808080";                                                           // unit (W, Wh, ...) 

/************************\
 *         Data
\************************/

static struct {
  uint8_t  delay     = AWTRIX_DELAY_DEFAULT;         // delay between page display
  uint8_t  dis_page  = UINT8_MAX;                    // current displayed data page
  uint8_t  bri_setup = 0;                            // brightness update needed
  long     prod_max  = AWTRIX_PROD_MAX_DEFAULT;      // maximum produced power
  uint32_t dis_time  = UINT32_MAX;                   // timestamp of next page display switch
  uint8_t  arr_flag[TIC_AWTRIX_MAX];                 // array of display flags
} tic_awtrix;

/***********************************************\
 *                  Commands
\***********************************************/

// whatsapp help
void CmndTicAwtrixHelp ()
{
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Commandes d'affichage Awtrix :"));
  AddLog (LOG_LEVEL_INFO, PSTR ("  awtrix_addr <addr>  = Adresse IP du device Awtrix"));
  AddLog (LOG_LEVEL_INFO, PSTR ("  awtrix_delai [%u]    = Délai entre 2 pages (min. 2s)"), tic_awtrix.delay);
  AddLog (LOG_LEVEL_INFO, PSTR ("  awtrix_lumi  [%u]    = Luminosité (1..100%%), 0=auto"),   tic_awtrix.bri_setup);
  AddLog (LOG_LEVEL_INFO, PSTR ("  awtrix_inst  [%u]    = Puissance instantanée"), tic_awtrix.arr_flag[TIC_AWTRIX_INSTANT]);
  AddLog (LOG_LEVEL_INFO, PSTR ("  awtrix_cwh   [%u]    = Consommation du jour"),  tic_awtrix.arr_flag[TIC_AWTRIX_CONSO_WH]);
  AddLog (LOG_LEVEL_INFO, PSTR ("  awtrix_pwh   [%u]    = Production du jour"),    tic_awtrix.arr_flag[TIC_AWTRIX_PROD_WH]);
  AddLog (LOG_LEVEL_INFO, PSTR ("  awtrix_cal   [%u]    = Calendrier"),            tic_awtrix.arr_flag[TIC_AWTRIX_CALENDAR]);
  AddLog (LOG_LEVEL_INFO, PSTR ("  awtrix_pmax  [%d] = Puissance produite max"),   tic_awtrix.prod_max);
  ResponseCmndDone();
}

void CmndTicAwtrixAddr ()
{
  if (XdrvMailbox.data_len > 0) SettingsUpdateText (SET_TIC_AW_URL, XdrvMailbox.data);
  ResponseCmndChar (SettingsText (SET_TIC_AW_URL));
}

void CmndTicAwtrixBright ()
{
  // if brightness is valid, update and set
  if ((XdrvMailbox.payload >= 0) && (XdrvMailbox.payload <= 100))
  {
    tic_awtrix.bri_setup = (uint8_t)XdrvMailbox.payload;
    TicAwtrixSaveConfig ();
    TicAwtrixSetBrightness ();
  }
  ResponseCmndNumber (tic_awtrix.bri_setup);
}

void CmndTicAwtrixDelay ()
{
  if (XdrvMailbox.data_len > 0)
  {
    if (XdrvMailbox.payload > 1) tic_awtrix.delay = (uint8_t)XdrvMailbox.payload;
    TicAwtrixSaveConfig ();
  }
  ResponseCmndNumber (tic_awtrix.delay);
}

void CmndTicAwtrixProdMax ()
{
  if (XdrvMailbox.data_len > 0)
  {
    if (XdrvMailbox.payload > 99) tic_awtrix.prod_max = (long)(XdrvMailbox.payload / 100) * 100;
    TicAwtrixSaveConfig ();
  }
  ResponseCmndNumber (tic_awtrix.prod_max);
}

void CmndTicAwtrixInstant ()
{
  if (XdrvMailbox.data_len > 0)
  {
    tic_awtrix.arr_flag[TIC_AWTRIX_INSTANT] = (uint8_t)atoi (XdrvMailbox.data);
    TicAwtrixSaveConfig ();
  }
  ResponseCmndNumber (tic_awtrix.arr_flag[TIC_AWTRIX_INSTANT]);
}

void CmndTicAwtrixConsoWh ()
{
  if (XdrvMailbox.data_len > 0)
  {
    tic_awtrix.arr_flag[TIC_AWTRIX_CONSO_WH] = (uint8_t)atoi (XdrvMailbox.data);
    TicAwtrixSaveConfig ();
  }
  ResponseCmndNumber (tic_awtrix.arr_flag[TIC_AWTRIX_CONSO_WH]);
}

void CmndTicAwtrixProdWh ()
{
  if (XdrvMailbox.data_len > 0)
  {
    tic_awtrix.arr_flag[TIC_AWTRIX_PROD_WH] = (uint8_t)atoi (XdrvMailbox.data);
    TicAwtrixSaveConfig ();
  }
  ResponseCmndNumber (tic_awtrix.arr_flag[TIC_AWTRIX_PROD_WH]);
}

void CmndTicAwtrixCalendar ()
{
  if (XdrvMailbox.data_len > 0)
  {
    tic_awtrix.arr_flag[TIC_AWTRIX_CALENDAR] = (uint8_t)atoi (XdrvMailbox.data);
    TicAwtrixSaveConfig ();
  }
  ResponseCmndNumber (tic_awtrix.arr_flag[TIC_AWTRIX_CALENDAR]);
}

/**************************************************\
 *                Confoguration
\**************************************************/

void TicAwtrixLoadConfig ()
{
  uint8_t index;

  // general data
  tic_awtrix.delay     = Settings->rf_code[12][0];
  tic_awtrix.prod_max  = (long)Settings->rf_code[12][1] * 100;
  tic_awtrix.bri_setup = Settings->rf_code[12][2];

  // set default values
  if (tic_awtrix.delay    == 0) tic_awtrix.delay    = AWTRIX_DELAY_DEFAULT;
  if (tic_awtrix.prod_max == 0) tic_awtrix.prod_max = AWTRIX_PROD_MAX_DEFAULT;
  if (tic_awtrix.bri_setup > 100) tic_awtrix.bri_setup = 0;

  // display pages
  for (index = 0; index < TIC_AWTRIX_MAX; index ++) tic_awtrix.arr_flag[index] = Settings->rf_code[12][index + 3];
}

void TicAwtrixSaveConfig ()
{
  uint8_t index;

  // general data
  Settings->rf_code[12][0] = tic_awtrix.delay;
  Settings->rf_code[12][1] = (uint8_t)(tic_awtrix.prod_max / 100);
  Settings->rf_code[12][2] = tic_awtrix.bri_setup;

  // display pages
  for (index = 0; index < TIC_AWTRIX_MAX; index ++) Settings->rf_code[12][index + 3] = tic_awtrix.arr_flag[index];
}

/**************************************************\
 *                  Functions
\**************************************************/

// send realtime conso to Awtrix display
uint8_t TicAwtrixGenerateDigit (const uint32_t value, char *pstr_digit, const size_t size_digit, char *pstr_unit, const size_t size_unit)
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

  // 10 - 100 : 676 W
  else if (value >= 10)
  {
    sprintf_P (pstr_digit, PSTR ("%u"), value);
    strcpy_P (pstr_unit, " W");
    offset = 9; 
  }

  // 1 - 10 : 6 W
  else
  {
    sprintf_P (pstr_digit, PSTR ("%u"), value);
    strcpy_P (pstr_unit, " W");
    offset = 13; 
  }

  return offset;
}

/*
// update stats from awtrix device
void TicAwtrixUpdateStats ()
{
  int      http_code;
  uint8_t  delta, target;
  uint16_t brightness;
  long     lux_act;
  float    value;
  char    *pstr_device;
  char     str_url[48];
  HTTPClientLight    *phttp_client;
  DynamicJsonDocument json_result(768);

  // check if URL is defined
  pstr_device = SettingsText (SET_TIC_AW_URL);
  if (strlen (pstr_device) == 0) return;

  // generate URL
  sprintf_P (str_url, PSTR ("http://%s/api/stats"), pstr_device); 

  // call awtrix device
  phttp_client = new HTTPClientLight ();  
  phttp_client->begin (str_url);

  // send command
  phttp_client->setConnectTimeout (AWTRIX_CONNECT_TIMEOUT);
  phttp_client->setTimeout (AWTRIX_ANSWER_TIMEOUT);
  phttp_client->addHeader (F ("Accept"), F ("application/json"));
  http_code = phttp_client->GET ();

  // if success
  if (http_code >= 0)
  {
    // update data from JSON
    deserializeJson (json_result, phttp_client->getString ().c_str ());

    // get brightness and luminosity
    brightness = json_result["bri"].as<unsigned int>();
    tic_awtrix.bri_actual = (uint8_t)(brightness * 100 / 255);

    // if fixed brightness is set
    if (tic_awtrix.bri_setup > 0)
    {
      // set target if different from more than 1 %
      if (tic_awtrix.bri_setup > tic_awtrix.bri_actual + 1) tic_awtrix.bri_target = tic_awtrix.bri_setup;
        else if (tic_awtrix.bri_setup < tic_awtrix.bri_actual - 1) tic_awtrix.bri_target = tic_awtrix.bri_setup;
    }

    // else auto-brightness calculation
    else
    {
      // get current luminosity
      lux_act = json_result["lux"].as<signed long>();
      if (lux_act < tic_awtrix.lux_min) lux_act = tic_awtrix.lux_min;
        else if (lux_act > tic_awtrix.lux_max) lux_act = tic_awtrix.lux_max;

      // calculate new target brightness
      value = (float)(lux_act - tic_awtrix.lux_min) / (float)(tic_awtrix.lux_max - tic_awtrix.lux_min);
      value = 100 - 100 * (1 - powf (value, 2.2));
  
      // calculate brightness delta in percent
      brightness = (uint16_t)tic_awtrix.bri_mini;
      target = (uint8_t)(brightness + (uint16_t)value * (100 - brightness) / 100);
      if (target > tic_awtrix.bri_actual) delta = target - tic_awtrix.bri_actual;
        else delta = tic_awtrix.bri_actual - target;
      if (delta >= AWTRIX_BRIGHTNESS_TRIGGER) tic_awtrix.bri_target = target;
    }
  }

  // end HTTP client 
  phttp_client->end ();
  delete phttp_client;
  yield ();
}
*/

// send realtime conso to Awtrix display
void TicAwtrixPublish (const uint8_t type)
{
  char       *pstr_device;
  const char *pstr_type;
  char        str_url[48];
  HTTPClientLight *phttp_client;

  // set URL type
  switch (type)
  {
    case TIC_AWTRIX_TYPE_SETTING:
      pstr_type = PSTR ("settings");
      break;
    case TIC_AWTRIX_TYPE_CUSTOM:
      pstr_type = PSTR ("custom?name=tic");
      break;
    default:
    pstr_type = nullptr;
  }
  if (pstr_type == nullptr) return;

  // check if URL is defined
  pstr_device = SettingsText (SET_TIC_AW_URL);
  if (strlen (pstr_device) == 0) return;

  // generate URL
  sprintf_P (str_url, PSTR ("http://%s/api/%s"), pstr_device, pstr_type); 

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
  AddLog (LOG_LEVEL_DEBUG, PSTR ("AWT: Update - %s"), ResponseData ());
  ResponseClear ();
  yield ();
}

// send realtime conso to Awtrix display
void TicAwtrixSetBrightness ()
{
  uint16_t brightness;

  // auto-brightness
  if (tic_awtrix.bri_setup == 0) Response_P (PSTR ("{'TIM':false,'DAT':false,'HUM':false,'TEMP':false,'BAT':false,'ABRI':true}"));

  // else fixed brightness, convert from percentage to 0..255
  else
  {
    brightness = (uint16_t)tic_awtrix.bri_setup * 255 / 100;
    Response_P (PSTR ("{'TIM':false,'DAT':false,'HUM':false,'TEMP':false,'BAT':false,'ABRI':false,'BRI':%u}"), brightness);
  }

  // publish
  TicAwtrixPublish (TIC_AWTRIX_TYPE_SETTING);
}

// send instant power to Awtrix display
void TicAwtrixPublishInstant ()
{
  uint8_t mode, phase, nb_phase, level, hphc, offset;
  long    power, digit, height, width;
  long    bar_unit, bar_width, bar_left;
  char    str_unit[4];
  char    str_digit[8];
  char    str_bkgrd[8];
  char    str_color[8];

  // check environment
  if (teleinfo_prod.pact > 0) mode = TIC_AWTRIX_MODE_PROD;
  else if ((teleinfo_conso.total_wh > 0) && (teleinfo_contract.ssousc > 0)) mode = TIC_AWTRIX_MODE_CONSO;
  else return;

  // set number of phases
  if (mode == TIC_AWTRIX_MODE_PROD) nb_phase = 1;
    else nb_phase = teleinfo_contract.phase;

  // set display parameters according to number of phases to display
  bar_left = 26;
  if (mode == TIC_AWTRIX_MODE_PROD) power = tic_awtrix.prod_max;
    else power = teleinfo_contract.ssousc;
  if (mode == TIC_AWTRIX_MODE_PROD) { bar_width = 6; bar_unit = power / 48; }
    else if (nb_phase == 1) { bar_width = 6; bar_unit = power / 48; }
      else { bar_width = 2; bar_unit = power / 16; }

  // display power value
  if (mode == TIC_AWTRIX_MODE_PROD) power = teleinfo_prod.pact;
    else power = teleinfo_conso.pact;
  offset = TicAwtrixGenerateDigit (power, str_digit, sizeof (str_digit), str_unit, sizeof (str_unit));
  Response_P (PSTR ("{\"textOffset\":%u,\"textCase\":2,\"center\":false,\"text\":[{\"t\":\"%s\"},{\"t\":\"%s\",\"c\":\"%s\"}]"), offset, str_digit, str_unit, kTicAwtrixUnitColor);

  // conso : calculate color of bargraph
  if (mode == TIC_AWTRIX_MODE_CONSO)
  {
    level  = TeleinfoPeriodGetLevel ();
    hphc   = TeleinfoPeriodGetHP ();
    offset = 2 * level + hphc;
    GetTextIndexed (str_bkgrd, sizeof (str_bkgrd), level,  kTicAwtrixConsoBckgrd);
    GetTextIndexed (str_color, sizeof (str_color), offset, kTicAwtrixConsoColors);  
  }

  // prod : set color of bargraph
  else
  {
    strcpy_P (str_bkgrd, kTicAwtrixProdBckgrd);
    strcpy_P (str_color, kTicAwtrixProdColor);
  }

  // start of diplay
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

  // end of diplay
  ResponseAppend_P (PSTR ("]}"));

  // publish
  TicAwtrixPublish (TIC_AWTRIX_TYPE_CUSTOM);
}

void TicAwtrixPublishConsoWh ()
{
  uint8_t offset;
  char    str_unit[4];
  char    str_digit[8];

  // calculate value to display
  offset = TicAwtrixGenerateDigit (teleinfo_conso.today_wh, str_digit, sizeof (str_digit), str_unit, sizeof (str_unit));

  // display value
  Response_P (PSTR ("{\"textOffset\":%u,\"textCase\":2,\"center\":false,\"text\":[{\"t\":\"%s\"},{\"t\":\"%sh\",\"c\":\"%s\"}]}"), offset, str_digit, str_unit, kTicAwtrixUnitColor);

  // publish
  TicAwtrixPublish (TIC_AWTRIX_TYPE_CUSTOM);
}

void TicAwtrixPublishProdWh ()
{
  uint8_t offset;
  char    str_unit[4];
  char    str_digit[8];

  // calculate value to display
  offset = TicAwtrixGenerateDigit (teleinfo_prod.today_wh, str_digit, sizeof (str_digit), str_unit, sizeof (str_unit));

  // display value
  Response_P (PSTR ("{\"textOffset\":%u,\"textCase\":2,\"center\":false,\"text\":[{\"t\":\"%s\",\"c\":\"%s\"},{\"t\":\"%sh\",\"c\":\"%s\"}]}"), offset, str_digit, kTicAwtrixProdColor, str_unit, kTicAwtrixUnitColor);

  // publish
  TicAwtrixPublish (TIC_AWTRIX_TYPE_CUSTOM);
}

void TicAwtrixPublishCalendar ()
{
  uint8_t  index, hchp, level1, level2, level3;
  uint32_t current_time;
  TIME_T   time_dst;
  char     str_label[4];
  char     str_color[8];


  // get time
  current_time = LocalTime ();

  // init display
  Response_P (PSTR ("{\"draw\":["));

  // current period logo
  level1 = TeleinfoPeriodGetLevel ();
  hchp   = TeleinfoPeriodGetHP ();

  // today's color logo
  level2 = TeleinfoDriverCalendarGetLevel (TIC_DAY_TODAY, UINT8_MAX, false);
  GetTextIndexed (str_color, sizeof (str_color), 2 * level2 + 1, kTicAwtrixConsoColors);
  ResponseAppend_P (PSTR ("{\"dl\":[12,0,19,0,\"%s\"]},{\"df\":[11,1,9,6,\"%s\"]}"), str_color, str_color);

  // tomorrow's color logo
  level3 = TeleinfoDriverCalendarGetLevel (TIC_DAY_TMROW, UINT8_MAX, false);
  GetTextIndexed (str_color, sizeof (str_color), 2 * level3 + 1, kTicAwtrixConsoColors);
  ResponseAppend_P (PSTR (",{\"dl\":[22,0,29,0,\"%s\"]},{\"df\":[21,1,9,6,\"%s\"]}"), str_color, str_color);

  // set current HC/HP label
  GetTextIndexed (str_label, sizeof (str_label), hchp,   kTeleinfoHourShort);
  GetTextIndexed (str_color, sizeof (str_color), 2 * level1 + 1, kTicAwtrixConsoColors);
  ResponseAppend_P (PSTR ("],\"text\":[{\"t\":\"%s \",\"c\":\"%s\"}"), str_label, str_color);

  // set today's date
  BreakTime (current_time, time_dst);
  sprintf_P (str_label, PSTR ("%02u"), time_dst.day_of_month);
  GetTextIndexed (str_color, sizeof (str_color), level2, kTicAwtrixTextColors);
  ResponseAppend_P (PSTR (",{\"t\":\"%s \",\"c\":\"%s\"}"), str_label, str_color);
    
  // set tomorrow's date
  BreakTime (current_time + 86400, time_dst);
  sprintf_P (str_label, PSTR ("%02u"), time_dst.day_of_month);
  GetTextIndexed (str_color, sizeof (str_color), level3, kTicAwtrixTextColors);
  ResponseAppend_P (PSTR (",{\"t\":\"%s\",\"c\":\"%s\"}"), str_label, str_color);
  
  // end display
  ResponseAppend_P (PSTR ("],\"topText\":true}"));

  // publish
  TicAwtrixPublish (TIC_AWTRIX_TYPE_CUSTOM);
}

// send realtime conso to Awtrix display
void TicAwtrixEverySecond ()
{
  bool    publish = false;
  uint8_t index;

  // if disabled, running on battery or no reception, ignore
  if (teleinfo_meter.nb_message == 0) return;
  if (!RtcTime.valid) return;

  // check if one data has to be published
  for (index = 0; index < TIC_AWTRIX_MAX; index ++) if (tic_awtrix.arr_flag[index]) publish = true;
  if (!publish) return;

  // first update
  if (tic_awtrix.dis_time == UINT32_MAX) tic_awtrix.dis_time = LocalTime ();

  // if needed, update awtrix brightness
  if (RtcTime.second % 30 == 1) TicAwtrixSetBrightness ();

  // update display every xx seconds
  else if (tic_awtrix.dis_time < LocalTime ())
  {
    // loop to find next data to display
    for (index = 0; index < TIC_AWTRIX_MAX; index ++)
    {
      tic_awtrix.dis_page ++;
      tic_awtrix.dis_page = tic_awtrix.dis_page % TIC_AWTRIX_MAX;
      if (tic_awtrix.arr_flag[tic_awtrix.dis_page] == 1) break;
    }

    if (tic_awtrix.arr_flag[tic_awtrix.dis_page] == 1)
      switch (tic_awtrix.dis_page)
      {
        case TIC_AWTRIX_INSTANT:  TicAwtrixPublishInstant ();  break;
        case TIC_AWTRIX_CONSO_WH: TicAwtrixPublishConsoWh ();  break;
        case TIC_AWTRIX_PROD_WH:  TicAwtrixPublishProdWh ();   break;
        case TIC_AWTRIX_CALENDAR: TicAwtrixPublishCalendar (); break;
      }

    // set next update
    tic_awtrix.dis_time = LocalTime () + (uint32_t)tic_awtrix.delay;
  }
}

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

// Append InfluxDB configuration button
void AwtrixWebConfigButton ()
{
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>Configure Awtrix</button></form></p>\n"), PSTR (AWTRIX_PAGE_CFG));
}

// Teleinfo web page
void AwtrixWebPageConfigure ()
{
  char str_value[32];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess ()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg (F ("save")))
  {
    // retrieve parameters
    if (Webserver->hasArg (F ("addr")))  { WebGetArg (PSTR ("addr"),  str_value, sizeof (str_value)); SettingsUpdateText (SET_TIC_AW_URL, str_value); }
    if (Webserver->hasArg (F ("delai"))) { WebGetArg (PSTR ("delai"), str_value, sizeof (str_value)); tic_awtrix.delay     = (uint8_t)atoi (str_value); }
    if (Webserver->hasArg (F ("lumi")))  { WebGetArg (PSTR ("lumi"),  str_value, sizeof (str_value)); tic_awtrix.bri_setup = (uint8_t)atoi (str_value); }
    if (Webserver->hasArg (F ("pmax")))  { WebGetArg (PSTR ("pmax"),  str_value, sizeof (str_value)); tic_awtrix.prod_max  = (long)atol (str_value); }
    if (Webserver->hasArg (F ("inst")))  tic_awtrix.arr_flag[TIC_AWTRIX_INSTANT]  = 1; else tic_awtrix.arr_flag[TIC_AWTRIX_INSTANT]  = 0;
    if (Webserver->hasArg (F ("cwh")))   tic_awtrix.arr_flag[TIC_AWTRIX_CONSO_WH] = 1; else tic_awtrix.arr_flag[TIC_AWTRIX_CONSO_WH] = 0;
    if (Webserver->hasArg (F ("pwh")))   tic_awtrix.arr_flag[TIC_AWTRIX_PROD_WH]  = 1; else tic_awtrix.arr_flag[TIC_AWTRIX_PROD_WH]  = 0;
    if (Webserver->hasArg (F ("cal")))   tic_awtrix.arr_flag[TIC_AWTRIX_CALENDAR] = 1; else tic_awtrix.arr_flag[TIC_AWTRIX_CALENDAR] = 0;

    // save config and set brightness
    TicAwtrixSaveConfig ();
    TicAwtrixSetBrightness ();
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
  WSContentSend_P (PSTR ("input[type='checkbox'] {margin:0px 15px;}\n"));
  WSContentSend_P (PSTR ("p.dat {padding:2px;}\n"));
  WSContentSend_P (PSTR ("span {float:left;padding-top:4px;}\n"));
  WSContentSend_P (PSTR ("span.head {width:55%%;}\n"));
  WSContentSend_P (PSTR ("span.val {width:30%%;padding:0px;}\n"));
  WSContentSend_P (PSTR ("span.sel {width:40%%;padding:0px;}\n"));
  WSContentSend_P (PSTR ("span.unit {width:15%%;text-align:center;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // form
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), PSTR (AWTRIX_PAGE_CFG));

  WSContentSend_P (PSTR ("<fieldset><legend>Afficheur Awtrix</legend>\n"));
  WSContentSend_P (PSTR ("<p class='dat'><span class='head'>Adresse IP</span><span class='sel'><input type='text' name='addr' value='%s'></span></p>\n"), SettingsText (SET_TIC_AW_URL));
  WSContentSend_P (PSTR ("</fieldset>\n"));

  WSContentSend_P (PSTR ("<fieldset><legend>Paramètres</legend>\n"));
  WSContentSend_P (PSTR ("<p class='dat'><span class='head'>Luminosité <small>(0=auto)</small></span><span class='val'><input type='number' name='lumi' min=0 max=100 step=1 value=%u></span><span class='unit'>%%</span></p>\n"), tic_awtrix.bri_setup);
  WSContentSend_P (PSTR ("<p class='dat'><span class='head'>Délai entre pages</span><span class='val'><input type='number' name='delai' min=2 max=255 step=1 value=%u></span><span class='unit'>sec.</span></p>\n"), tic_awtrix.delay);
  WSContentSend_P (PSTR ("<p class='dat'><span class='head'>Production max.</span><span class='val'><input type='number' name='pmax' min=100 max=32000 step=100 value=%d></span><span class='unit'>W</span></p>\n"), tic_awtrix.prod_max);
  WSContentSend_P (PSTR ("</fieldset>\n"));

  WSContentSend_P (PSTR ("<fieldset><legend>Données affichées</legend>\n"));
  if (tic_awtrix.arr_flag[TIC_AWTRIX_INSTANT]) strcpy_P (str_value, PSTR ("checked")); else str_value[0] = 0;  
  WSContentSend_P (PSTR ("<p class='dat'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), PSTR ("inst"), str_value, PSTR ("inst"), PSTR ("Puissance instantanée"));
  if (tic_awtrix.arr_flag[TIC_AWTRIX_CONSO_WH]) strcpy_P (str_value, PSTR ("checked")); else str_value[0] = 0;  
  WSContentSend_P (PSTR ("<p class='dat'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), PSTR ("cwh"),  str_value, PSTR ("cwh"),  PSTR ("Consommation du jour"));
  if (tic_awtrix.arr_flag[TIC_AWTRIX_PROD_WH]) strcpy_P (str_value, PSTR ("checked")); else str_value[0] = 0;  
  WSContentSend_P (PSTR ("<p class='dat'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), PSTR ("pwh"),  str_value, PSTR ("pwh"),  PSTR ("Production du jour"));
  if (tic_awtrix.arr_flag[TIC_AWTRIX_CALENDAR]) strcpy_P (str_value, PSTR ("checked")); else str_value[0] = 0;  
  WSContentSend_P (PSTR ("<p class='dat'><input type='checkbox' name='%s' %s><label for='%s'>%s</label></p>\n"), PSTR ("cal"),  str_value, PSTR ("cal"),  PSTR ("Calendrier"));
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

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xdrv115 (uint32_t function)
{
  bool result = false;

  if (!teleinfo_config.battery) switch (function)
  {
    case FUNC_INIT:
      TicAwtrixLoadConfig ();
      break;
      case FUNC_EVERY_SECOND:
      TicAwtrixEverySecond ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kTicAwtrixCommands, TicAwtrixCommand);
      break;

#ifdef USE_WEBSERVER
      case FUNC_WEB_ADD_BUTTON:
        AwtrixWebConfigButton ();
        break;
      case FUNC_WEB_ADD_HANDLER:
        Webserver->on (F (AWTRIX_PAGE_CFG), AwtrixWebPageConfigure);
      break;
#endif    // USE_WEBSERVER
  }

  return result;
}

#endif      // USE_AWTRIX
#endif      // USE_TELEINFO
#endif      // ESP32
