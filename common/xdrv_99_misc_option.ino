/*
  xdrv_99_misc_option.ino - Misc system options management
  
  Copyright (C) 2020-2025  Nicolas Bernaerts
    20/04/2021 v1.0 - Creation 
    22/04/2021 v1.1 - Merge IPAddress and InfoJson 
    02/05/2021 v1.2 - Add Ethernet adapter status (ESP32) 
    22/06/2021 v1.3 - Change in wifi activation/desactivation 
    07/05/2022 v1.4 - Add command to enable JSON publishing 
    14/08/2022 v1.5 - Simplification, slim down of JSON 
    15/04/2024 v1.6 - Handle IPV6 and IPV4 difference for DNS 
    30/07/2024 v1.7 - Add common options and rename to xdrv_94_ip_option.ino 
    21/09/2024 v1.8 - Add command 'data' to publish sensor data 
    10/07/2025 v2.0 - Merge of options and timezone
                      Rename to xdrv_99_ip_option.ino
                      Refactoring based on Tasmota 15
    22/10/2025 v2.1 - Add load average
    28/01/2026 v2.2 - Correct IPv4 DNS bug, add DNS2

  Settings are stored using :
    - Settings->rf_code[16][8] = Web display of load average

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

/*************************************************\
 *                IP Address
\*************************************************/

#ifdef USE_MISC_OPTION

#define MISCOPTION_LOAD_SAMPLE     60

#define MISCOPTION_INTERNET_DELAY  15

// timezone commands
#define D_CMND_TIMEZONE_NTP       "ntp"
#define D_CMND_TIMEZONE_STDO      "stdo"
#define D_CMND_TIMEZONE_STDM      "stdm"
#define D_CMND_TIMEZONE_STDW      "stdw"
#define D_CMND_TIMEZONE_STDD      "stdd"
#define D_CMND_TIMEZONE_DSTO      "dsto"
#define D_CMND_TIMEZONE_DSTM      "dstm"
#define D_CMND_TIMEZONE_DSTW      "dstw"
#define D_CMND_TIMEZONE_DSTD      "dstd"

// IP address source
enum IPAddressSource { IP_SOURCE_NONE, IP_SOURCE_WIFI, IP_SOURCE_ETHERNET };
const char kIpAddressLabel[] PROGMEM = "Address|Gateway|Netmask|DNS 1|DNS 2";

// data publication commands
static const char kMiscOptionCommands[]  PROGMEM = "|"    "option"     "|"     "data"      "|"        "webload";
void (* const MiscOptionCommand[])(void) PROGMEM = { &CmndMiscOptionHelp, &CmndMiscOptionData,  &CmndMiscOptionWebLoad };

// timezone setiing commands
const char kTimezoneCommands[]         PROGMEM = "tz|"    "_pub"      "|"     "_ntp"   " |"     "_stdo"    "|"     "_stdm"    "|"     "_stdw"    "|"     "_stdd"    "|"      "_dsto"   "|"      "_dstm"   "|"     "_dstw"    "|"     "_dstd";
void (* const TimezoneCommand[])(void) PROGMEM = { &CmndTimezonePublish, &CmndTimezoneNtp, &CmndTimezoneStdO, &CmndTimezoneStdM, &CmndTimezoneStdW, &CmndTimezoneStdD, &CmndTimezoneDstO, &CmndTimezoneDstM, &CmndTimezoneDstW, &CmndTimezoneDstD };

// wifi color levels
enum WifiLevelColor                  {  WIFI_BAD , WIFI_AVERAGE, WIFI_CORRECT, WIFI_GOOD, WIFI_MAX };
const char kWifiLevelColor[] PROGMEM = "#e00" "|"  "#e80" "|"  "#ee0" "|" "#0e0";     

// load color levels
enum LoadLevelColor                  { LOAD_HIGH , LOAD_MEDIUM,  LOAD_LOW, LOAD_MAX };
const char kLoadLevelColor[] PROGMEM = "#b00" "|" "#c80" "|" "#090";     

// web URL
static const char PSTR_MISC_OPTION_PAGE_CONFIG[] PROGMEM = "/opt-cfg";
static const char PSTR_MISC_OPTION_PAGE_JSON[]   PROGMEM = "/info.json";

// constant strings
static const char PSTR_MISCOPTION_FIELDSET_START[] PROGMEM = "<fieldset><legend>%s</legend>\n";
static const char PSTR_MISCOPTION_FIELDSET_STOP[]  PROGMEM = "</fieldset>\n";
static const char PSTR_MISCOPTION_FIELD_CHECKBOX[] PROGMEM = "<p class='dat'><input type='checkbox' name='%s' title='%s' %s><label for='%s'>%s</label></p>\n";
static const char PSTR_MISCOPTION_FIELD_INPUT[]    PROGMEM = "<p class='dat'><span class='hea'>%s</span><span class='val'><input type='number' name='%s' min='%d' max='%d' step='%d' value='%d' title='%s'></span><span class='uni'>%s</span></p>\n";

// dialog name and URL
const char PSTR_TIMEZONE_TITLE[]       PROGMEM = "Timezone";
const char PSTR_TIMEZONE_PAGE_CONFIG[] PROGMEM = "/tz";

/**************************************************\
 *                  Variables
\**************************************************/

static struct {
  bool publish = false;
} misc_timezone;

static struct {
  bool    enabled = false;
  uint8_t second  = UINT8_MAX;
  uint8_t arr_load[MISCOPTION_LOAD_SAMPLE];
} misc_load;

static struct {
  uint8_t  delay     = 0;
  uint32_t time_last = UINT32_MAX;
  uint32_t time_ping = UINT32_MAX;
  uint32_t time_out  = UINT32_MAX;
} misc_internet;

/*************************************************\
 *                  Commands
\*************************************************/

// publish all sensor data
void CmndMiscOptionData ()
{
  uint16_t teleperiod;

  // force teleperiod to 0
  teleperiod = TasmotaGlobal.tele_period;
  TasmotaGlobal.tele_period = 0;

  // generate answer
  ResponseClear ();
  ResponseAppendTime ();
  XsnsXdrvCall (FUNC_JSON_APPEND);
  ResponseJsonEnd ();

  // reload teleperiod
  TasmotaGlobal.tele_period = teleperiod;

  // publish answer
  Response_P (ResponseData ());
}

/**************************************************\
 *                  Functions
\**************************************************/

#ifdef ESP8266

int64_t esp_timer_get_time ()
{
  int64_t ts_now;
  struct  timeval tv_now; 

  gettimeofday (&tv_now, NULL);
  ts_now = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;

  return ts_now;
}

#endif    // ESP8266

// check need of '{' or ',' to append new section in TasmotaGlobal.mqtt_data
void MiscOptionPrepareJsonSection ()
{
  int  length;
  char last;

  length = TasmotaGlobal.mqtt_data.length ();
  if (length == 0) ResponseAppend_P (PSTR ("{"));
  else
  {
    last = ResponseData ()[length - 1];
    if ((last != '{') && (last != ',')) ResponseAppend_P (PSTR (","));
  }
}

void MiscOptionLoadConfig ()
{
  misc_internet.delay = Settings->rf_code[16][7];
  misc_load.enabled   = (Settings->rf_code[16][8] != 0);
}

void MiscOptionSaveConfig ()
{
  Settings->rf_code[16][7] = misc_internet.delay;
  Settings->rf_code[16][8] = misc_load.enabled;
}

/**************************************************\
 *                Fixed IP address
\**************************************************/

// get active network adapter
uint8_t MiscOptionIPAddressGetSource ()
{
#ifdef USE_ETHERNET
  // ethernet
  if ((uint32_t)ETH.localIP () > 0) return IP_SOURCE_ETHERNET;
  else
#endif // USE_ETHERNET

  // wifi
  if ((uint32_t)WiFi.localIP () > 0) return IP_SOURCE_WIFI;

  else return IP_SOURCE_NONE;
}

// save IP address in setup
bool MiscOptionIPAddressUpdate (const uint8_t index, const char* pstr_address)
{
  bool     updated;
  uint32_t value;

  if (pstr_address == nullptr) return false;
  if (index > 3) return false;

  ParseIPv4 (&value, pstr_address);
  updated = (value != Settings->ipv4_address[index]);
  if (updated) Settings->ipv4_address[index] = value;
  
  return updated;
}

/**************************************************\
 *                    Timezone
\**************************************************/

// timezone help
void CmndMiscOptionHelp ()
{
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Options commands :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - data          = Publish sensor data"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - webload <0/1> = Display average load graph on main page"));

  AddLog (LOG_LEVEL_INFO, PSTR (" - tz_pub <0/1>  = Add timezone data in telemetry JSON"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - tz_ntp <name> = Set NTP server"));

  AddLog (LOG_LEVEL_INFO, PSTR (" - tz_stdo <val> = Standard time offset to GMT (mn)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - tz_stdm <val> = Standard time month (1:jan ... 12:dec)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - tz_stdw <val> = Standard time week (0:last ... 4:fourth)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - tz_stdd <val> = Standard time day of week (1:sun ... 7:sat)"));

  AddLog (LOG_LEVEL_INFO, PSTR (" - tz_dsto <val> = Daylight savings time offset to GMT (mn)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - tz_dstm <val> = Daylight savings time month (1:jan ... 12:dec)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - tz_dstw <val> = Daylight savings time week (0:last ... 4:fourth)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - tz_dstd <val> = Daylight savings time day of week (1:sun ... 7:sat)"));
  
  ResponseCmndDone();
}

void CmndTimezonePublish ()
{
  if (XdrvMailbox.data_len > 0)
  {
    // update flag
    misc_timezone.publish = ((XdrvMailbox.payload != 0) || (strcasecmp_P (XdrvMailbox.data, PSTR ("ON")) == 0));
  }
  ResponseCmndNumber (misc_timezone.publish);
}

void CmndTimezoneNtp ()
{
  if (XdrvMailbox.data_len > 0) SettingsUpdateText (SET_NTPSERVER1, XdrvMailbox.data);
  ResponseCmndChar (SettingsText(SET_NTPSERVER1));
}

void CmndTimezoneStdO ()
{
  if (XdrvMailbox.data_len > 0) Settings->toffset[0] = XdrvMailbox.payload;
  ResponseCmndNumber (Settings->toffset[0]);
}

void CmndTimezoneStdM ()
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload > 0) && (XdrvMailbox.payload < 13)) Settings->tflag[0].month = XdrvMailbox.payload;
  ResponseCmndNumber (Settings->tflag[0].month);
}

void CmndTimezoneStdW ()
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload >= 0) && (XdrvMailbox.payload < 5)) Settings->tflag[0].week = XdrvMailbox.payload;
  ResponseCmndNumber (Settings->tflag[0].week);
}

void CmndTimezoneStdD ()
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload > 0) && (XdrvMailbox.payload < 8)) Settings->tflag[0].dow = XdrvMailbox.payload;
  ResponseCmndNumber (Settings->tflag[0].dow);
}

void CmndTimezoneDstO ()
{
  if (XdrvMailbox.data_len > 0) Settings->toffset[1] = XdrvMailbox.payload;
  ResponseCmndNumber (XdrvMailbox.payload);
}

void CmndTimezoneDstM ()
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload > 0) && (XdrvMailbox.payload < 13)) Settings->tflag[1].month = XdrvMailbox.payload;
  ResponseCmndNumber (Settings->tflag[1].month);
}

void CmndTimezoneDstW ()
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload >= 0) && (XdrvMailbox.payload < 5)) Settings->tflag[1].week = XdrvMailbox.payload;
  ResponseCmndNumber (Settings->tflag[1].week);
}

void CmndTimezoneDstD ()
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload > 0) && (XdrvMailbox.payload < 8)) Settings->tflag[1].dow = XdrvMailbox.payload;
  ResponseCmndNumber (Settings->tflag[1].dow);
}

// Show JSON status (for MQTT)
void MiscOptionTimezoneAppendSensor ()
{
  // add , in append mode or { in direct publish mode
  MiscOptionPrepareJsonSection ();

  // generate string
  ResponseAppend_P (PSTR ("\"TZ\":{"));
  ResponseAppend_P (PSTR ("\"%s\":{\"Offset\":%d,\"Month\":%d,\"Week\":%d,\"Day\":%d}"), "STD", Settings->toffset[0], Settings->tflag[0].month, Settings->tflag[0].week, Settings->tflag[0].dow);
  ResponseAppend_P (PSTR (","));
  ResponseAppend_P (PSTR ("\"%s\":{\"Offset\":%d,\"Month\":%d,\"Week\":%d,\"Day\":%d}"), "DST", Settings->toffset[1], Settings->tflag[1].month, Settings->tflag[1].week, Settings->tflag[1].dow);
  ResponseAppend_P (PSTR ("}"));
}

/**************************************************\
 *                  Load average
\**************************************************/

// display web load on main page
void CmndMiscOptionWebLoad ()
{
  // update flag
  if (XdrvMailbox.data_len > 0)
  {
    misc_load.enabled = ((XdrvMailbox.payload != 0) || (strcasecmp_P (XdrvMailbox.data, PSTR ("ON")) == 0));
    MiscOptionSaveConfig ();
  }

  ResponseCmndNumber (misc_load.enabled);
}

void MiscOptionLoadAverageEvery250ms ()
{
  uint8_t second;
  uint8_t load_avg;

  // if no valid time, ignore
  if (!RtcTime.valid) return;

  // reset load average for new slot
  second = RtcTime.second;
  if (misc_load.second != second) misc_load.arr_load[second] = 0;

  // save peak average load
  if (TasmotaGlobal.loop_load_avg < 100) load_avg = (uint8_t)TasmotaGlobal.loop_load_avg;
    else load_avg = 100;
  misc_load.arr_load[second] = max (misc_load.arr_load[second], load_avg);

  // update current slot
  misc_load.second = second;
}

/**************************************************\
 *                  Call back
\**************************************************/

// init main status
void MiscOptionInit ()
{
  // set switch mode
//  RtcTime.valid      = 0;
  Settings->timezone = 99;

  // init array
  memset (&misc_load.arr_load, 0, sizeof (misc_load.arr_load));

  // load config
  MiscOptionLoadConfig ();

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Run tz to get help on Timezone commands"));
}

// Show JSON status (for MQTT)
void MiscOptionAppendSensor ()
{
#ifdef USE_ETHERNET
  // ethenet MAC
  if (Settings->flag4.network_ethernet == 1) ResponseAppend_P (PSTR (",\"Eth\":\"%s\""), EthernetMacAddress ().c_str ());
#endif // USE_ETHERNET
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// IP address configuration page button
void MiscOptionWebConfigButton ()
{
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>"), PSTR_MISC_OPTION_PAGE_CONFIG, PSTR ("Options"));
}

// IP address configuration web page
void MiscOptionWebPageConfigure ()
{
  bool    restart = false;
  bool    act_dhcp, new_dhcp;
  uint8_t index;
  uint32_t ip_address;
  char    str_arg[8], str_argument[32];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // check dhcp status
  new_dhcp = act_dhcp = (Settings->ipv4_address[0] == 0);

  // page comes from save button on configuration page
  if (Webserver->hasArg (F ("save")))
  {
    // dhcp : check DHCP status
    WebGetArg (PSTR ("dhcp"), str_argument, sizeof (str_argument));
    new_dhcp  = (strcmp_P (str_argument, PSTR ("on")) == 0);
    restart |= (new_dhcp != act_dhcp); 

    // if needed, set DHCP, else set fixed IP
    if (new_dhcp) Settings->ipv4_address[0] = 0;
    else for (index = 0; index < 5; index ++)
    {
      sprintf_P (str_arg, PSTR ("ip%u"), index);
      WebGetArg (str_arg, str_argument, sizeof (str_argument));
      if (strlen (str_argument) > 0) restart |= MiscOptionIPAddressUpdate (index, str_argument);
    }

    // Internet connexion check delay
    WebGetArg (PSTR ("delay"), str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) misc_internet.delay = (uint8_t)atoi (str_argument);

    // SetOption56 : Scan Wifi on restart
    WebGetArg (PSTR ("opt56"), str_argument, sizeof (str_argument));
    if (strcmp_P (str_argument, PSTR ("on")) == 0) Settings->flag3.use_wifi_scan = 1;
      else Settings->flag3.use_wifi_scan = 0;

    // SetOption57 : Scan Wifi every 44 mn
    WebGetArg (PSTR ("opt57"), str_argument, sizeof (str_argument));
    if (strcmp_P (str_argument, PSTR ("on")) == 0) Settings->flag3.use_wifi_rescan = 1;
      else Settings->flag3.use_wifi_rescan = 0;

    // SetOption65 : Reset after 7 fast power cut
    WebGetArg (PSTR ("opt65"), str_argument, sizeof (str_argument));
    if (strcmp_P (str_argument, PSTR ("on")) == 0) Settings->flag3.fast_power_cycle_disable = 0;
      else Settings->flag3.fast_power_cycle_disable = 1;

    // SetOption36 : Boot loop protection
    WebGetArg (PSTR ("opt36"), str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) Settings->param[P_BOOT_LOOP_OFFSET] = (uint8_t)atoi (str_argument);

    // WebRefresh
    WebGetArg (PSTR ("refresh"), str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) Settings->web_refresh = (uint16_t)atoi (str_argument);

    // SetOption146 : Display ESP32 temperature
    WebGetArg (PSTR ("opt146"), str_argument, sizeof (str_argument));
    if (strcmp_P (str_argument, PSTR ("on")) == 0) Settings->flag6.use_esp32_temperature = 1;
      else Settings->flag6.use_esp32_temperature = 0;

    // SetOption21 : Monitor power with switch OFF
    WebGetArg (PSTR ("opt21"), str_argument, sizeof (str_argument));
    if (strcmp_P (str_argument, PSTR ("on")) == 0) Settings->flag.no_power_on_check = 1;
      else Settings->flag.no_power_on_check = 0;

    // set first time server
    WebGetArg (PSTR (D_CMND_TIMEZONE_NTP), str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) SettingsUpdateText (SET_NTPSERVER1, str_argument);

    // set timezone STD offset according to 'stdo' parameter
    WebGetArg (PSTR (D_CMND_TIMEZONE_STDO), str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) Settings->toffset[0] = atoi (str_argument);
    
    // set timezone STD month switch according to 'stdm' parameter
    WebGetArg (PSTR (D_CMND_TIMEZONE_STDM), str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) Settings->tflag[0].month = atoi (str_argument);

    // set timezone STD week of month switch according to 'stdw' parameter
    WebGetArg (PSTR (D_CMND_TIMEZONE_STDW), str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) Settings->tflag[0].week = atoi (str_argument);

    // set timezone STD day of week switch according to 'stdd' parameter
    WebGetArg (PSTR (D_CMND_TIMEZONE_STDD), str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) Settings->tflag[0].dow = atoi (str_argument);

    // set timezone DST offset according to 'dsto' parameter
    WebGetArg (PSTR (D_CMND_TIMEZONE_DSTO), str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) Settings->toffset[1] = atoi (str_argument);
    
    // set timezone DST month switch according to 'dstm' parameter
    WebGetArg (PSTR (D_CMND_TIMEZONE_DSTM), str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) Settings->tflag[1].month = atoi (str_argument);

    // set timezone DST week of month switch according to 'dstw' parameter
    WebGetArg (PSTR (D_CMND_TIMEZONE_DSTW), str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) Settings->tflag[1].week = atoi (str_argument);

    // set timezone DST day of week switch according to 'dstd' parameter
    WebGetArg (PSTR (D_CMND_TIMEZONE_DSTD), str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) Settings->tflag[1].dow = atoi (str_argument);
  }

  // if data have been updated, restart
  if (restart) WebRestart (1);

  // beginning of form
  WSContentStart_P (PSTR ("Configure Options"));
  WSContentSendStyle ();

  // specific style
  WSContentSend_P (PSTR ("\n<style>\n"));
  WSContentSend_P (PSTR ("p,br,hr {clear:both;}\n")); 
  WSContentSend_P (PSTR ("p.dat {margin:0px 10px;}\n")); 
  WSContentSend_P (PSTR ("p.head {margin:10px;}\n")); 

  WSContentSend_P (PSTR ("fieldset {background:#333;margin:15px 0px;border:none;border-radius:8px;}\n")); 
  WSContentSend_P (PSTR ("fieldset.config {border-color:#888;background:%s;margin-left:12px;margin-top:4px;padding:8px 0px 0px 0px;}\n"), PSTR (COLOR_BACKGROUND));
  WSContentSend_P (PSTR ("legend {font-weight:bold;margin:0px;padding:5px;color:#888;background:transparent;}\n")); 
  WSContentSend_P (PSTR ("legend:after {content:'';display:block;height:1px;margin:15px 0px;}\n"));
  WSContentSend_P (PSTR ("div.main {padding:0px;margin-top:-25px;}\n"));
  WSContentSend_P (PSTR ("input,select {margin-bottom:10px;text-align:center;border:none;border-radius:4px;}\n")); 
  WSContentSend_P (PSTR ("input.left {text-align:left;}\n")); 
  WSContentSend_P (PSTR ("span {float:left;padding-top:4px;}\n")); 
  WSContentSend_P (PSTR ("span.hea {width:60%%;}\n")); 
  WSContentSend_P (PSTR ("span.pri {width:35%%;}\n"));
  WSContentSend_P (PSTR ("span.val {width:25%%;padding:0px;}\n")); 
  WSContentSend_P (PSTR ("span.sel {width:65%%;padding:0px;}\n"));
  WSContentSend_P (PSTR ("span.uni {width:15%%;text-align:center;}\n")); 
  WSContentSend_P (PSTR ("</style>\n"));

  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), PSTR_MISC_OPTION_PAGE_CONFIG);

  //    Network 
  // --------------
  WSContentSend_P (PSTR_MISCOPTION_FIELDSET_START, PSTR ("🌐 Network"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  if (new_dhcp) strcpy_P (str_argument, PSTR ("checked")); else str_argument[0] = 0;
  WSContentSend_P (PSTR ("<p class='dat'><input type='checkbox' id='dhcp' name='dhcp' onchange='onChangeDHCP()' %s><label for='dhcp'>Use DHCP for IPV4</label></p>\n"), str_argument);
  
  WSContentSend_P (PSTR ("<fieldset class='config' id='fixed-ip' style='display:block;'>\n"));
  for (index = 0; index < 5; index ++)
  {
    GetTextIndexed (str_argument, sizeof (str_argument), index, kIpAddressLabel);
    WSContentSend_P (PSTR ("<p class='dat'><span class='pri'>%s</span><span class='sel'><input type='text' id='ip%u' name='ip%u' value='%_I'></span></p>\n"), str_argument, index, index, Settings->ipv4_address[index]);
  }
  WSContentSend_P (PSTR ("</fieldset>\n"));

  WSContentSend_P (PSTR ("<hr>\n"));
  WSContentSend_P (PSTR ("<p class='dat'><span class='hea'>Connexion lost delay</span><span class='val'><input type='number' name='delay' min=0 max=24 step=1 value=%u title='Relay 1 will be switched OFF/ON if internet connexion is lost during given delay. Useful to restart router or wifi AP in case of lost connectivity. Connectivity is tested thru ping on DNS 2. Set to 0 to disable.'></span><span class='uni'>h</span></p>\n"), misc_internet.delay);

  WSContentSend_P (PSTR ("</div>\n"));
  WSContentSend_P (PSTR_MISCOPTION_FIELDSET_STOP);

    // ip masking script
  WSContentSend_P (PSTR ("<script type='text/javascript'>\n"));
  WSContentSend_P (PSTR ("function onChangeDHCP() {\n"));
  WSContentSend_P (PSTR ("document.getElementById('fixed-ip').style.display=document.getElementById('dhcp').checked?'none':'block'; }\n"));
  for (index = 0; index < 5; index ++) WSContentSend_P (PSTR ("document.getElementById('ip%u').value='%_I';\n"), index, Settings->ipv4_address[index]);
  WSContentSend_P (PSTR ("onChangeDHCP();\n"));
  WSContentSend_P (PSTR ("</script>\n"));

  //    Sensor 
  // --------------
  WSContentSend_P (PSTR_MISCOPTION_FIELDSET_START, PSTR ("📏 Sensors"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  if (Settings->flag6.use_esp32_temperature == 1) strcpy_P (str_argument, PSTR ("checked"));
    else str_argument[0] = 0;
  WSContentSend_P (PSTR_MISCOPTION_FIELD_CHECKBOX, PSTR ("opt146"), PSTR ("SetOption146"), str_argument, PSTR ("opt146"), PSTR ("Display ESP32 temperature"));

  if (Settings->flag.no_power_on_check == 1) strcpy_P (str_argument, PSTR ("checked"));
    else str_argument[0] = 0;
  WSContentSend_P (PSTR_MISCOPTION_FIELD_CHECKBOX, PSTR ("opt21"), PSTR ("SetOption21"), str_argument, PSTR ("opt21"), PSTR ("Monitor power with switch OFF"));

  WSContentSend_P (PSTR ("</div>\n"));
  WSContentSend_P (PSTR_MISCOPTION_FIELDSET_STOP);

  //    System 
  // --------------
  WSContentSend_P (PSTR_MISCOPTION_FIELDSET_START, PSTR ("⚙️ System"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  str_argument[0] = 0;
  if (Settings->flag3.use_wifi_scan == 1) strcpy_P (str_argument, PSTR ("checked"));
  WSContentSend_P (PSTR_MISCOPTION_FIELD_CHECKBOX, PSTR ("opt56"), PSTR ("SetOption56"), str_argument, PSTR ("opt56"), PSTR ("Scan Wifi on restart"));

  str_argument[0] = 0;
  if (Settings->flag3.use_wifi_rescan == 1) strcpy_P (str_argument, PSTR ("checked"));
  WSContentSend_P (PSTR_MISCOPTION_FIELD_CHECKBOX, PSTR ("opt57"), PSTR ("SetOption57"), str_argument, PSTR ("opt57"), PSTR ("Scan Wifi every 44 mn"));

  str_argument[0] = 0;
  if (Settings->flag3.fast_power_cycle_disable == 0) strcpy_P (str_argument, PSTR ("checked"));
  WSContentSend_P (PSTR_MISCOPTION_FIELD_CHECKBOX, PSTR ("opt65"), PSTR ("SetOption65"), str_argument, PSTR ("opt65"), PSTR ("Reset after 7 fast power cut"));

  WSContentSend_P (PSTR ("<hr>\n"));
  WSContentSend_P (PSTR_MISCOPTION_FIELD_INPUT, PSTR ("Boot loop protection"), PSTR ("opt36"), 0, 200, 1, Settings->param[P_BOOT_LOOP_OFFSET], PSTR ("SetOption36"), PSTR ("boot"));
  WSContentSend_P (PSTR_MISCOPTION_FIELD_INPUT, PSTR ("Web refresh"), PSTR ("refresh"), 1000, 30000, 1, Settings->web_refresh, PSTR ("WebRefresh"), PSTR ("ms"));

  WSContentSend_P (PSTR ("</div>\n"));
  WSContentSend_P (PSTR_MISCOPTION_FIELDSET_STOP);
  
  // NTP server section  
  // ---------------------
  WSContentSend_P (PSTR_MISCOPTION_FIELDSET_START, "🕛 Time server");
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  WSContentSend_P (PSTR ("<p class='dat'>%s</p>\n"), PSTR ("First server"));
  WSContentSend_P (PSTR ("<p class='dat'><input type='text' name='%s' value='%s'></p>\n"), PSTR (D_CMND_TIMEZONE_NTP), SettingsText(SET_NTPSERVER1));
  WSContentSend_P (PSTR ("<hr>\n"));

  // Standard Time section  
  // ---------------------
  WSContentSend_P (PSTR ("<p class='head'>❄️ Standard Time</p>\n"));
  WSContentSend_P (PSTR_MISCOPTION_FIELD_INPUT, PSTR ("Offset to GMT"),                         PSTR (D_CMND_TIMEZONE_STDO), -720, 720, 30, Settings->toffset[0],     PSTR (""), PSTR ("mn"));
  WSContentSend_P (PSTR_MISCOPTION_FIELD_INPUT, PSTR ("Month<small> (1:jan 12:dec)</small>"),   PSTR (D_CMND_TIMEZONE_STDM), 1,    12,  1,  Settings->tflag[0].month, PSTR (""), PSTR (""));
  WSContentSend_P (PSTR_MISCOPTION_FIELD_INPUT, PSTR ("Week<small> (0:last 4:fourth)</small>"), PSTR (D_CMND_TIMEZONE_STDW), 0,    4,   1,  Settings->tflag[0].week,  PSTR (""), PSTR (""));
  WSContentSend_P (PSTR_MISCOPTION_FIELD_INPUT, PSTR ("Week day<small> (1:sun 7:sat)</small>"), PSTR (D_CMND_TIMEZONE_STDD), 1,    7,   1,  Settings->tflag[0].dow,   PSTR (""), PSTR (""));
  WSContentSend_P (PSTR ("<hr>\n"));

  // Daylight Saving Time section  
  // ----------------------------
  WSContentSend_P (PSTR ("<p class='head'>⛱️ Daylight Saving Time</p>\n"));
  WSContentSend_P (PSTR_MISCOPTION_FIELD_INPUT, PSTR ("Offset to GMT"),                         PSTR (D_CMND_TIMEZONE_DSTO), -720, 720, 30, Settings->toffset[1],     PSTR (""), PSTR ("mn"));
  WSContentSend_P (PSTR_MISCOPTION_FIELD_INPUT, PSTR ("Month<small> (1:jan 12:dec)</small>"),   PSTR (D_CMND_TIMEZONE_DSTM), 1,    12,  1,  Settings->tflag[1].month, PSTR (""), PSTR (""));
  WSContentSend_P (PSTR_MISCOPTION_FIELD_INPUT, PSTR ("Week<small> (0:last 4:fourth)</small>"), PSTR (D_CMND_TIMEZONE_DSTW), 0,    4,   1,  Settings->tflag[1].week,  PSTR (""), PSTR (""));
  WSContentSend_P (PSTR_MISCOPTION_FIELD_INPUT, PSTR ("Week day<small> (1:sun 7:sat)</small>"), PSTR (D_CMND_TIMEZONE_DSTD), 1,    7,   1,  Settings->tflag[1].dow,   PSTR (""), PSTR (""));

  WSContentSend_P (PSTR ("</div>\n"));
  WSContentSend_P (PSTR_MISCOPTION_FIELDSET_STOP);

  // save button  
  // --------------
  WSContentSend_P (PSTR ("<br><p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), PSTR (D_SAVE));
  WSContentSend_P (PSTR ("</form>\n"));

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

// JSON version page
void MiscOptionWebPageJSON ()
{
  char stopic[TOPSZ];

  // start of page
  WSContentBegin (200, CT_HTML);
  WSContentSend_P (PSTR ("{"));

  // "Module":"NomDuModule","Version":"x.x.x","Uptime":"xxxxxx","Friendly name":"name",
  WSContentSend_P (PSTR ("\"Module\":\"%s\""), ModuleName ().c_str ());
  WSContentSend_P (PSTR (",\"Program Version\":\"%s\""), TasmotaGlobal.version);
  WSContentSend_P (PSTR (",\"Uptime\":\"%s\""), GetUptime ().c_str ());
  WSContentSend_P (PSTR (",\"Friendly Name\":\"%s\""), SettingsText (SET_DEVICENAME));

#ifdef EXTENSION_NAME
  // "Extension type":"type","Extension version":"version","Extension author":"author"}
  WSContentSend_P (PSTR (",\"Extension Type\":\"%s\""), EXTENSION_NAME);
  WSContentSend_P (PSTR (",\"Extension Version\":\"%s\""), EXTENSION_VERSION);
  WSContentSend_P (PSTR (",\"Extension Author\":\"%s\""), EXTENSION_AUTHOR);
#endif

  // "MQTT":{"Host":"host","Port":"port","Topic":"topic"}
  WSContentSend_P (PSTR (",\"MQTT Host\":\"%s\""), SettingsText (SET_MQTT_HOST));
  WSContentSend_P (PSTR (",\"MQTT Port\":\"%d\""), Settings->mqtt_port);
  WSContentSend_P (PSTR (",\"MQTT Full Topic\":\"%s\""), GetTopic_P (stopic, CMND, TasmotaGlobal.mqtt_topic, ""));

  // "Wifi IP":"xx.xx.xx.xx","Wifi MAC":"xx:xx:xx:xx:xx:xx:xx","Wifi Host":"hostname","Wifi SSID":"ssid,"Wifi Quality":"70"}
  WSContentSend_P (PSTR (",\"Wifi IP\":\"%s\""), WiFi.localIP ().toString ().c_str ());
  WSContentSend_P (PSTR (",\"Wifi MAC\":\"%s\""), WiFi.macAddress ().c_str ());
  WSContentSend_P (PSTR (",\"Wifi Host\":\"%s\""), TasmotaGlobal.hostname);
  WSContentSend_P (PSTR (",\"Wifi SSID\":\"%s\""), SettingsText (SET_STASSID1 + Settings->sta_active));
  WSContentSend_P (PSTR (",\"Wifi Quality\":\"%d\""), WifiGetRssiAsQuality (WiFi.RSSI ()));

#ifdef USE_ETHERNET

  // "Eth IP":"xx.xx.xx.xx","Eth MAC":"xx:xx:xx:xx:xx:xx:xx","Eth Host":"hostname"}
  if (Settings->flag4.network_ethernet == 1)
  {
    WSContentSend_P (PSTR (",\"Eth IP\":\"%s\""), EthernetLocalIP ().toString ().c_str ());
    WSContentSend_P (PSTR (",\"Eth Mac\":\"%s\""), EthernetMacAddress ());
    WSContentSend_P (PSTR (",\"Eth Host\":\"%s\""), EthernetHostname ());
  }

#endif // USE_ETHERNET

  // end of page
  WSContentSend_P ( PSTR ("}"));
  WSContentEnd ();
}

// main page display style
void MiscOptionWebMainButton ()
{
  WSContentSend_P (PSTR ("<style>\n"));

  // style : wifi
  WSContentSend_P (PSTR (".a3{width:24px;height:24px;top:1px;left:3px;}\n"));
  WSContentSend_P (PSTR (".a2{width:18px;height:18px;top:5px;left:6px;}\n"));
  WSContentSend_P (PSTR (".a1{width:12px;height:12px;top:9px;left:9px;}\n"));
  WSContentSend_P (PSTR (".a0{width:4px;height:4px;top:13px;left:13px;}\n"));

  // style : status
  WSContentSend_P (PSTR ("div#l1 .top{margin:2px;padding:1px 2px;border-radius:5px;font-size:10px;border:#aaa 1px solid;}\n"));
  WSContentSend_P (PSTR ("div#l1 .ko{border:#b00 1px solid;color:#b00;}\n"));
  WSContentSend_P (PSTR ("div#l1 .rssi{margin-left:0px;border:none;}\n"));
  WSContentSend_P (PSTR ("div#l1 .cpu{padding:0px;width:16px;height:20px;margin-left:5px;border-radius:2px;}\n"));
  WSContentSend_P (PSTR ("div#l1 .cpu div{padding:0px;background:#444;border-radius:2px;}\n"));
  WSContentSend_P (PSTR ("div#l1 .span{margin-left:2px;border:none;}\n"));

  WSContentSend_P (PSTR ("div.avg{padding:0px 6px;border:#444 solid 1px;border-radius:12px;}\n"));
  WSContentSend_P (PSTR ("svg.avg{width:100%%;margin:4px 0px;height:80px;}\n"));
  WSContentSend_P (PSTR ("polyline{stroke-width:4;fill:none;}\n"));

  WSContentSend_P (PSTR ("</style>\n"));
}

// append wifi strength
void MiscOptionWebStatusLeft ()
{
  uint8_t level;
  int32_t rssi, load;
  char    str_color[8];

#ifdef USE_WEB_STATUS_LINE_WIFI

  // wifi level
  rssi = WiFi.RSSI();
  if (rssi >= -55) level = WIFI_GOOD;
    else if (rssi >= -70) level = WIFI_CORRECT;
    else if (rssi >= -85) level = WIFI_AVERAGE;
    else level = WIFI_BAD;

  // wifi style
  GetTextIndexed (str_color, sizeof (str_color), level, kWifiLevelColor);
  WSContentSend_P (PSTR ("<style>.arc {border-top-color:%s;}</style>\n"), str_color);

  // wifi percentage
  WSContentSend_P (PSTR ("<span class='top rssi'>%d%%</span>\n"), WifiGetRssiAsQuality (WiFi.RSSI ()));

#endif    //  USE_WEB_STATUS_LINE_WIFI

#ifdef USE_WEB_STATUS_LINE_LOAD

  // load average
  load = min ((uint32_t)100, TasmotaGlobal.loop_load_avg);
  if (load < 60) level = LOAD_LOW;
    else if (load < 90) level = LOAD_MEDIUM;
    else level = LOAD_HIGH;
  GetTextIndexed (str_color, sizeof (str_color), level, kLoadLevelColor);
  WSContentSend_P (PSTR ("<div class='cpu' style='background:%s;'><div style='height:%u%%;'></div></div><span class='top span'>%u%%</span>\n"), str_color, 100 - load, load);

#endif    //  USE_WEB_STATUS_LINE_LOAD
}

// append time
void MiscOptionWebStatusRight ()
{
  // time icon
  WSContentSend_P (PSTR ("<span class='top"));
  if (!RtcTime.valid) WSContentSend_P (PSTR (" ko"));
  WSContentSend_P (PSTR ("'>%02d:%02d:%02d</span>\n"), RtcTime.hour, RtcTime.minute, RtcTime.second);
}

void MiscOptionWebLoadAverageSensor ()
{
  uint8_t  second, index, slot;
  uint16_t pos_x, pos_y;

  // if RTC not set, ignore
  if (!RtcTime.valid) return;
  if (!misc_load.enabled) return;

  // start of section
  WSContentSend_P (PSTR ("<div class='avg'>\n"));

  // curve declaration
  WSContentSend_P (PSTR ("<svg class='avg' viewBox='0 0 %u 200' preserveAspectRatio='none'>\n"), 10 * MISCOPTION_LOAD_SAMPLE - 10);
  WSContentSend_P (PSTR ("<linearGradient id='avg' x1=0 y1=0 x2=0 y2=1>\n"));
  WSContentSend_P (PSTR ("<stop stop-color='red' offset=%u%% /><stop stop-color='red' offset=%u%% />\n"), 100 - 100, 100 - 92);
  WSContentSend_P (PSTR ("<stop stop-color='orange' offset=%u%% /><stop stop-color='orange' offset=%u%% />\n"), 100 - 88, 100 - 72);
  WSContentSend_P (PSTR ("<stop stop-color='green' offset=%u%% /><stop stop-color='green' offset=%u%% />\n"), 100 - 68, 100 - 0);
  WSContentSend_P (PSTR ("</linearGradient>\n"));

  // curve data
  WSContentSend_P (PSTR ("<polyline stroke='url(#avg)' points='"));
  second = (uint16_t)RtcTime.second + 1;
  for (index = 0; index < MISCOPTION_LOAD_SAMPLE; index ++)
  {
    // display slot if valid
    slot = (second + index) % MISCOPTION_LOAD_SAMPLE;
    if (misc_load.arr_load[slot] > 0)
    {
      // display point
      pos_x = (uint16_t)index * 10;
      pos_y = 200 - (uint16_t)misc_load.arr_load[slot] * 2;
      WSContentSend_P (PSTR (" %u,%u"), pos_x, pos_y);

      // last point
      if (index == MISCOPTION_LOAD_SAMPLE - 1) WSContentSend_P (PSTR (" %u,%u %u,0"), pos_x + 10, pos_y, pos_x + 10);
    }
  }
  WSContentSend_P (PSTR ("' />\n"));

  // end of section
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentSend_P (PSTR ("</div>\n"));
}

#endif  // USE_WEBSERVER

/***********************************************************\
 *                      Interface
\***********************************************************/

#define XDRV_99         99

bool Xdrv99 (uint32_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
    case FUNC_INIT:
      MiscOptionInit ();
      break;

    case FUNC_COMMAND:
      result = DecodeCommand (kMiscOptionCommands, MiscOptionCommand);
      if (!result) result = DecodeCommand (kTimezoneCommands, TimezoneCommand);
      break;

    case FUNC_EVERY_250_MSECOND:
      MiscOptionLoadAverageEvery250ms ();
      break;

    case FUNC_JSON_APPEND:
      if (TasmotaGlobal.tele_period == 0) 
      {
        MiscOptionAppendSensor ();
        if (misc_timezone.publish) MiscOptionTimezoneAppendSensor ();
      }
      break;

#ifdef USE_WEBSERVER

    case FUNC_WEB_ADD_MAIN_BUTTON:
      MiscOptionWebMainButton ();
      break;
      
    case FUNC_WEB_ADD_BUTTON:
      MiscOptionWebConfigButton ();
      break;

    case FUNC_WEB_STATUS_LEFT:
      MiscOptionWebStatusLeft ();
      break;

    case FUNC_WEB_STATUS_RIGHT:
      MiscOptionWebStatusRight ();
      break;

    case FUNC_WEB_SENSOR:
      MiscOptionWebLoadAverageSensor ();
      break;
      
    case FUNC_WEB_ADD_HANDLER:
      Webserver->on (FPSTR (PSTR_MISC_OPTION_PAGE_CONFIG), MiscOptionWebPageConfigure);
      Webserver->on (FPSTR (PSTR_MISC_OPTION_PAGE_JSON),   MiscOptionWebPageJSON);
      break;

#endif  // USE_WEBSERVER
  }
  
  return result;
}

#endif    // USE_MISC_OPTION
