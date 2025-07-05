/*
  xsns_99_timezone.ino - Timezone management (~3.2 kb)
  
  Copyright (C) 2020  Nicolas Bernaerts
    04/04/2020 - v1.0 - Creation 
    19/05/2020 - v1.1 - Add configuration for first NTP server 
    22/07/2020 - v1.2 - Memory optimisation 
    10/04/2021 - v1.3 - Remove use of String to avoid heap fragmentation 
    22/04/2021 - v1.4 - Switch to a full Drv (without Sns) 
    11/07/2021 - v1.5 - Based on Tasmota 9.5
    07/05/2022 - v1.6 - Add command to enable JSON publishing 
    15/03/2024 - v1.7 - Add wrong DNS detection
    27/08/2024 - v1.8 - Add RTC, MQTT and Wifi status to main page
    02/07/2025 - v2.0 - Based on Tasmota 15
                        Switch main status bar to new one of Tasmota

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
 *                Timezone
\*************************************************/

#ifdef USE_TIMEZONE

#define XSNS_99                   99

// commands
#define D_CMND_TIMEZONE_NTP       "ntp"
#define D_CMND_TIMEZONE_STDO      "stdo"
#define D_CMND_TIMEZONE_STDM      "stdm"
#define D_CMND_TIMEZONE_STDW      "stdw"
#define D_CMND_TIMEZONE_STDD      "stdd"
#define D_CMND_TIMEZONE_DSTO      "dsto"
#define D_CMND_TIMEZONE_DSTM      "dstm"
#define D_CMND_TIMEZONE_DSTW      "dstw"
#define D_CMND_TIMEZONE_DSTD      "dstd"

// dialog name and URL
const char PSTR_TIMEZONE_TITLE[]       PROGMEM = "Timezone";
const char PSTR_TIMEZONE_PAGE_CONFIG[] PROGMEM = "/tz";

// timezone setiing commands
const char kTimezoneCommands[] PROGMEM = "tz||_pub|_ntp|_stdo|_stdm|_stdw|_stdd|_dsto|_dstm|_dstw|_dstd";
void (* const TimezoneCommand[])(void) PROGMEM = { &CmndTimezoneHelp, &CmndTimezonePublish, &CmndTimezoneNtp, &CmndTimezoneStdO, &CmndTimezoneStdM, &CmndTimezoneStdW, &CmndTimezoneStdD, &CmndTimezoneDstO, &CmndTimezoneDstM, &CmndTimezoneDstW, &CmndTimezoneDstD };

// constant strings
const char PSTR_TZ_FIELDSET_START[] PROGMEM = "<fieldset><legend>%s</legend>\n";
const char PSTR_TZ_FIELDSET_STOP[]  PROGMEM = "</fieldset>\n";
const char PSTR_TZ_FIELD_INPUT[]    PROGMEM = "<p class='dat'><span class='hea'>%s</span><span class='val'><input type='number' name='%s' min='%d' max='%d' step='%d' value='%d'></span><span class='uni'>%s</span></p>\n";


/**************************************************\
 *                  Variables
\**************************************************/

// variables
static struct {
  bool publish_json = false;
} timezone;

/**************************************************\
 *                  Functions
\**************************************************/

// Show JSON status (for MQTT)
void TimezoneShowJSON (bool append)
{
  // add , in append mode or { in direct publish mode
  if (append) ResponseAppend_P (PSTR (",")); else Response_P (PSTR ("{"));

  // generate string
  ResponseAppend_P (PSTR ("\"Timezone\":{"));
  ResponseAppend_P (PSTR ("\"%s\":{\"Offset\":%d,\"Month\":%d,\"Week\":%d,\"Day\":%d}"), "STD", Settings->toffset[0], Settings->tflag[0].month, Settings->tflag[0].week, Settings->tflag[0].dow);
  ResponseAppend_P (PSTR (","));
  ResponseAppend_P (PSTR ("\"%s\":{\"Offset\":%d,\"Month\":%d,\"Week\":%d,\"Day\":%d}"), "DST", Settings->toffset[1], Settings->tflag[1].month, Settings->tflag[1].week, Settings->tflag[1].dow);
  ResponseAppend_P (PSTR ("}"));

  // publish it if not in append mode
  if (!append)
  {
    ResponseAppend_P (PSTR ("}"));
    MqttPublishTeleSensor ();
  } 
}

// init main status
void TimezoneInit ()
{
  // set switch mode
  RtcTime.valid = 0;
  Settings->timezone = 99;

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Run tz to get help on Timezone commands"));
}

/***********************************************\
 *                  Commands
\***********************************************/

// timezone help
void CmndTimezoneHelp ()
{
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Timezone commands :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - tz_pub  = Add timezone data in telemetry JSON (ON/OFF)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - tz_ntp  = Set NTP server"));

  AddLog (LOG_LEVEL_INFO, PSTR (" - tz_stdo = Standard time offset to GMT (mn)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - tz_stdm = Standard time month (1:jan ... 12:dec)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - tz_stdw = Standard time week (0:last ... 4:fourth)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - tz_stdd = Standard time day of week (1:sun ... 7:sat)"));

  AddLog (LOG_LEVEL_INFO, PSTR (" - tz_dsto = Daylight savings time offset to GMT (mn)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - tz_dstm = Daylight savings time month (1:jan ... 12:dec)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - tz_dstw = Daylight savings time week (0:last ... 4:fourth)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - tz_dstd = Daylight savings time day of week (1:sun ... 7:sat)"));
  
  ResponseCmndDone();
}

void CmndTimezonePublish ()
{
  if (XdrvMailbox.data_len > 0)
  {
    // update flag
    timezone.publish_json = ((XdrvMailbox.payload != 0) || (strcasecmp_P (XdrvMailbox.data, PSTR ("ON")) == 0));
  }
  ResponseCmndNumber (timezone.publish_json);
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

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// append wifi strength
void TimezoneWebStatusLeft ()
{
  WSContentSend_P (PSTR ("<span class='rssi'>%d%%</span>\n"), WifiGetRssiAsQuality (WiFi.RSSI ()));
}

// append NTP
void TimezoneWebStatusRight ()
{
  char str_status[4];

  // NTP icon
  if (RtcTime.valid) strcpy_P (str_status, PSTR ("ok")); else strcpy_P (str_status, PSTR ("ko")); 
  WSContentSend_P (PSTR ("<span class='%s'>NTP</span>\n"), str_status);
}

// append local time to main page
void TimezoneWebSensor ()
{
  int32_t rssi;
  char    str_color[8];

  // style
  WSContentSend_P (PSTR ("<style>\n"));

  // style : wifi
  rssi = WiFi.RSSI();
  if (rssi >= -55) strcpy_P (str_color, PSTR ("#0e0"));
    else if (rssi >= -70) strcpy_P (str_color, PSTR ("#ee0"));
    else if (rssi >= -85) strcpy_P (str_color, PSTR ("#e80"));
    else strcpy_P (str_color, PSTR ("#e00"));
  WSContentSend_P (PSTR (".arc {border-top-color:#888;}\n"));
  WSContentSend_P (PSTR (".arc.active {border-top-color:%s;}\n"), str_color);
  WSContentSend_P (PSTR (".a3{width:24px;height:24px;top:1px;left:3px}\n"));
  WSContentSend_P (PSTR (".a2{width:18px;height:18px;top:5px;left:6px}\n"));
  WSContentSend_P (PSTR (".a1{width:12px;height:12px;top:9px;left:9px}\n"));
  WSContentSend_P (PSTR (".a0{width:4px;height:4px;top:13px;left:12px;border:3px solid transparent;}\n"));

  // style : status
  WSContentSend_P (PSTR ("div#l1 span {margin:2px;padding:1px 4px;border-radius:5px;font-size:10px;}\n"));
  WSContentSend_P (PSTR ("div#l1 span.rssi {padding-left:6px;}\n"));
  WSContentSend_P (PSTR ("div#l1 span.ok {border:#0b0 1px solid;color:#0b0;}\n"));
  WSContentSend_P (PSTR ("div#l1 span.ko {border:#b00 1px solid;color:#b00;}\n"));

  // style : hour
  WSContentSend_P (PSTR ("table hr{display:none;}\n"));
  WSContentSend_P (PSTR ("div.time {text-align:center;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // time
  WSContentSend_P (PSTR ("<div class='time'>%02d:%02d:%02d</div>\n"), RtcTime.hour, RtcTime.minute, RtcTime.second);
}

#ifdef USE_TIMEZONE_WEB_CONFIG

// timezone configuration page button
void TimezoneWebConfigButton ()
{
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>"), PSTR_TIMEZONE_PAGE_CONFIG, PSTR_TIMEZONE_TITLE);
}

// Timezone configuration web page
void TimezoneWebPageConfigure ()
{
  char str_argument[32];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg(F ("save")))
  {
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

  // beginning of form
  WSContentStart_P (PSTR_TIMEZONE_TITLE);
  WSContentSendStyle ();

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("p,br,hr {clear:both;}\n"));
  WSContentSend_P (PSTR ("p.dat {margin:0px 10px;}\n"));
  WSContentSend_P (PSTR ("fieldset {background:#333;margin:15px 0px;border:none;border-radius:8px;}\n")); 
  WSContentSend_P (PSTR ("legend {font-weight:bold;margin:0px;padding:5px;color:#888;background:transparent;}\n")); 
  WSContentSend_P (PSTR ("legend:after {content:'';display:block;height:1px;margin:15px 0px;}\n"));
  WSContentSend_P (PSTR ("div.main {padding:0px;margin-top:-25px;}\n"));
  WSContentSend_P (PSTR ("input,select {margin-bottom:10px;text-align:center;border:none;}\n"));
  WSContentSend_P (PSTR ("span {float:left;padding-top:4px;}\n"));
  WSContentSend_P (PSTR ("span.hea {width:60%%;}\n"));
  WSContentSend_P (PSTR ("span.val {width:25%%;padding:0px;}\n"));
  WSContentSend_P (PSTR ("span.uni {width:15%%;text-align:center;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), PSTR_TIMEZONE_PAGE_CONFIG);

  // NTP server section  
  // ---------------------
  WSContentSend_P (PSTR_TZ_FIELDSET_START, "🕛 Time server");
  WSContentSend_P (PSTR ("<div class='main'>\n"));
  WSContentSend_P (PSTR ("<p class='dat'>%s<br><input type='text' name='%s' value='%s'></p>\n"), PSTR ("First server"), PSTR (D_CMND_TIMEZONE_NTP), SettingsText(SET_NTPSERVER1));
  WSContentSend_P (PSTR ("</div>\n"));
  WSContentSend_P (PSTR_TZ_FIELDSET_STOP);

  // Standard Time section  
  // ---------------------
  WSContentSend_P (PSTR_TZ_FIELDSET_START, "❄️ Standard Time");
  WSContentSend_P (PSTR ("<div class='main'>\n"));
  WSContentSend_P (PSTR_TZ_FIELD_INPUT, PSTR ("Offset to GMT"),                         PSTR (D_CMND_TIMEZONE_STDO), -720, 720, 30, Settings->toffset[0],     PSTR ("mn"));
  WSContentSend_P (PSTR_TZ_FIELD_INPUT, PSTR ("Month<small> (1:jan 12:dec)</small>"),   PSTR (D_CMND_TIMEZONE_STDM), 1,    12,  1,  Settings->tflag[0].month, PSTR (""));
  WSContentSend_P (PSTR_TZ_FIELD_INPUT, PSTR ("Week<small> (0:last 4:fourth)</small>"), PSTR (D_CMND_TIMEZONE_STDW), 0,    4,   1,  Settings->tflag[0].week,  PSTR (""));
  WSContentSend_P (PSTR_TZ_FIELD_INPUT, PSTR ("Week day<small> (1:sun 7:sat)</small>"), PSTR (D_CMND_TIMEZONE_STDD), 1,    7,   1,  Settings->tflag[0].dow,   PSTR (""));
  WSContentSend_P (PSTR ("</div>\n"));
  WSContentSend_P (PSTR_TZ_FIELDSET_STOP);

  // Daylight Saving Time section  
  // ----------------------------
  WSContentSend_P (PSTR_TZ_FIELDSET_START, "⛱️ Daylight Saving Time");
  WSContentSend_P (PSTR ("<div class='main'>\n"));
  WSContentSend_P (PSTR_TZ_FIELD_INPUT, PSTR ("Offset to GMT"),                         PSTR (D_CMND_TIMEZONE_DSTO), -720, 720, 30, Settings->toffset[1],     PSTR ("mn"));
  WSContentSend_P (PSTR_TZ_FIELD_INPUT, PSTR ("Month<small> (1:jan 12:dec)</small>"),   PSTR (D_CMND_TIMEZONE_DSTM), 1,    12,  1,  Settings->tflag[1].month, PSTR (""));
  WSContentSend_P (PSTR_TZ_FIELD_INPUT, PSTR ("Week<small> (0:last 4:fourth)</small>"), PSTR (D_CMND_TIMEZONE_DSTW), 0,    4,   1,  Settings->tflag[1].week,  PSTR (""));
  WSContentSend_P (PSTR_TZ_FIELD_INPUT, PSTR ("Week day<small> (1:sun 7:sat)</small>"), PSTR (D_CMND_TIMEZONE_DSTD), 1,    7,   1,  Settings->tflag[1].dow,   PSTR (""));
  WSContentSend_P (PSTR ("</div>\n"));
  WSContentSend_P (PSTR_TZ_FIELDSET_STOP);

  // save button  
  // --------------
  WSContentSend_P (PSTR ("<p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), D_SAVE);
  WSContentSend_P (PSTR ("</form>\n"));

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop();
}

#endif      // USE_TIMEZONE_WEB_CONFIG

#endif  // USE_WEBSERVER

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xsns99 (uint32_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
    case FUNC_INIT:
      TimezoneInit ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kTimezoneCommands, TimezoneCommand);
      break;
    case FUNC_JSON_APPEND:
      if (timezone.publish_json) TimezoneShowJSON (true);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      TimezoneWebSensor ();
      break;
    case FUNC_WEB_STATUS_LEFT:
      TimezoneWebStatusLeft ();
      break;
    case FUNC_WEB_STATUS_RIGHT:
      TimezoneWebStatusRight ();
      break;
      
#ifdef USE_TIMEZONE_WEB_CONFIG
    case FUNC_WEB_ADD_BUTTON:
      TimezoneWebConfigButton ();
      break;
    case FUNC_WEB_ADD_HANDLER:
      Webserver->on (FPSTR (PSTR_TIMEZONE_PAGE_CONFIG), TimezoneWebPageConfigure);
      break;
#endif        // USE_TIMEZONE_WEB_CONFIG

#endif  // USE_WEBSERVER

  }
  
  return result;
}

#endif      // USE_TIMEZONE

