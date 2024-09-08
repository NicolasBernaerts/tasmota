/*
  xsns_99_timezone.ino - Timezone management (~3.2 kb)
  
  Copyright (C) 2020  Nicolas Bernaerts
    04/04/2020 - v1.0 - Creation 
    19/05/2020 - v1.1 - Add configuration for first NTP server 
    22/07/2020 - v1.2 - Memory optimisation 
    10/04/2021 - v1.3 - Remove use of String to avoid heap fragmentation 
    22/04/2021 - v1.4 - Switch to a full Drv (without Sns) 
    11/07/2021 - v1.5 - Tasmota 9.5 compatibility 
    07/05/2022 - v1.6 - Add command to enable JSON publishing 
    15/03/2024 - v1.7 - Add wrong DNS detection
    27/08/2024 - v1.8 - Add RTC, MQTT and Wifi status to main page

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

// web URL
const char PSTR_TIMEZONE_PAGE_CONFIG[] PROGMEM = "/tz";

// dialog strings
const char PSTR_TIMEZONE[]        PROGMEM = "Timezone";
const char PSTR_TIMEZONE_CONFIG[] PROGMEM = "Configure";

// time icons
//const char kTimezoneIcons[] PROGMEM = "🕛|🕧|🕐|🕜|🕑|🕝|🕒|🕞|🕓|🕟|🕔|🕠|🕕|🕡|🕖|🕢|🕗|🕣|🕘|🕤|🕙|🕥|🕚|🕦";

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

// append local time to main page
void TimezoneWebSensor ()
{
  int  rssi;
  char str_status[8];

  // get wifi RSSI
  rssi = WiFi.RSSI ();

  // begin
  WSContentSend_P (PSTR ("<div style='text-align:center;padding:0px;'>\n"));

  // style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("table hr{display:none;}\n"));
  WSContentSend_P (PSTR ("div.info {display:flex;padding:0px;}\n"));
  WSContentSend_P (PSTR ("div.info div{padding:0px;}\n"));
  if (rssi > -67) strcpy_P (str_status, PSTR ("green"));
    else if (rssi > -75) strcpy_P (str_status, PSTR ("orange"));
    else strcpy_P (str_status, PSTR ("red"));
  WSContentSend_P (PSTR ("div.wifi {height:28px;float:right;padding:0px;aspect-ratio:1;border-radius:50%%;margin-top:4px;margin-bottom:-4px;background:repeating-radial-gradient(50%% 50%%, #0000 0,%s 1px 14%%,#0000 18%% 28%%);mask:conic-gradient(#000 0 0) no-repeat 50%%/16%% 16%%,conic-gradient(from -45deg at 50%% 57%%,#000 90deg,#0000 0);}\n"), str_status);
  if (RtcTime.valid) strcpy_P (str_status, PSTR ("green")); else strcpy_P (str_status, PSTR ("red")); 
  WSContentSend_P (PSTR ("div.rtc {float:left;margin:2px 8px;width:22px;height:22px;background-color:%s;--svg:url(\"data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 256 256'><path d='M128,24A104,104,0,1,0,232,128,104.12041,104.12041,0,0,0,128,24Zm56,112H128a7.99541,7.99541,0,0,1-8-8V72a8,8,0,0,1,16,0v48h48a8,8,0,0,1,0,16Z'/></svg>\");-webkit-mask-image:var(--svg);mask-image:var(--svg);-webkit-mask-repeat:no-repeat;mask-repeat:no-repeat;-webkit-mask-size:100%% 100%%;mask-size:100%% 100%%;}\n"), str_status);
  if (MqttIsConnected ()) strcpy_P (str_status, PSTR ("green")); else strcpy_P (str_status, PSTR ("red")); 
  WSContentSend_P (PSTR ("div.mqtt {float:left;margin:4px 8px;width:16px;height:16px;background-color:%s;--svg:url(\"data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'><path d='M10.657 23.994h-9.45A1.21 1.21 0 0 1 0 22.788v-9.18h.071c5.784 0 10.504 4.65 10.586 10.386m7.606 0h-4.045C14.135 16.246 7.795 9.977 0 9.942V6.038h.071c9.983 0 18.121 8.044 18.192 17.956m4.53 0h-.97C21.754 12.071 11.995 2.407 0 2.372v-1.16C0 .55.544.006 1.207.006h7.64C15.733 2.49 21.257 7.789 24 14.508v8.291c0 .663-.544 1.195-1.207 1.195M16.713.006h6.092A1.19 1.19 0 0 1 24 1.2v5.914c-.91-1.242-2.046-2.65-3.158-3.762C19.588 2.11 18.122.987 16.714.005Z'/></svg>\");-webkit-mask-image:var(--svg);mask-image:var(--svg);-webkit-mask-repeat:no-repeat;mask-repeat:no-repeat;-webkit-mask-size:100%% 100%%;mask-size:100%% 100%%;}\n"), str_status);
  WSContentSend_P (PSTR ("</style>\n"));

  // detect DNS bad DNS server IP
//  if (!RtcTime.valid) WSContentSend_P (PSTR ("<div class='error'>Check DNS server IP address</div>\n"));

  // info bar
  WSContentSend_P (PSTR ("<div class='info'>\n"));

  // RTC and MQTT
  WSContentSend_P (PSTR ("<div style='width:24%%;text-align:left;'>"));
  if (RtcTime.valid) WSContentSend_P (PSTR ("<div class='rtc'></div>"));
    else WSContentSend_P (PSTR ("<div class='rtc' title='Check DNS server IP address'></div>"));
  if (MqttIsConnected ()) WSContentSend_P (PSTR ("<div class='mqtt'></div>"));
    else WSContentSend_P (PSTR ("<div class='mqtt' title='Check MQTT configuration'></div>"));
  WSContentSend_P (PSTR ("</div>\n"));

  // time
  WSContentSend_P (PSTR ("<div style='width:52%%;font-size:16px;font-weight:bold;'>%02d:%02d:%02d</div>\n"), RtcTime.hour, RtcTime.minute, RtcTime.second);

  // wifi signal
  WSContentSend_P (PSTR ("<div style='width:24%%;text-align:right;'><div class='wifi'></div><span style='font-size:12px;'>%d%%</span></div>\n"), WifiGetRssiAsQuality (rssi));

  // end
  WSContentSend_P (PSTR ("</div>\n"));
  WSContentSend_P (PSTR ("</div>\n"));
}

#ifdef USE_TIMEZONE_WEB_CONFIG

// timezone configuration page button
void TimezoneWebConfigButton ()
{
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s %s</button></form></p>"), PSTR_TIMEZONE_PAGE_CONFIG, PSTR_TIMEZONE_CONFIG, PSTR_TIMEZONE);
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
  WSContentStart_P (PSTR_TIMEZONE_CONFIG);
  WSContentSendStyle ();

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("p,br,hr {clear:both;}\n"));
  WSContentSend_P (PSTR ("p.dat {margin:0px 10px;}\n"));
  WSContentSend_P (PSTR ("fieldset {margin-bottom:24px;padding-top:12px;}\n"));
  WSContentSend_P (PSTR ("legend {padding:0px 15px;margin-top:-10px;font-weight:bold;}\n"));
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
  WSContentSend_P (PSTR ("<p class='dat'>%s<br><input type='text' name='%s' value='%s'></p>\n"), PSTR ("First server"), PSTR (D_CMND_TIMEZONE_NTP), SettingsText(SET_NTPSERVER1));
  WSContentSend_P (PSTR_TZ_FIELDSET_STOP);

  // Standard Time section  
  // ---------------------
  WSContentSend_P (PSTR_TZ_FIELDSET_START, "❄️ Standard Time");
  WSContentSend_P (PSTR_TZ_FIELD_INPUT, PSTR ("Offset to GMT"),                         PSTR (D_CMND_TIMEZONE_STDO), -720, 720, 30, Settings->toffset[0],     PSTR ("mn"));
  WSContentSend_P (PSTR_TZ_FIELD_INPUT, PSTR ("Month<small> (1:jan 12:dec)</small>"),   PSTR (D_CMND_TIMEZONE_STDM), 1,    12,  1,  Settings->tflag[0].month, PSTR (""));
  WSContentSend_P (PSTR_TZ_FIELD_INPUT, PSTR ("Week<small> (0:last 4:fourth)</small>"), PSTR (D_CMND_TIMEZONE_STDW), 0,    4,   1,  Settings->tflag[0].week,  PSTR (""));
  WSContentSend_P (PSTR_TZ_FIELD_INPUT, PSTR ("Week day<small> (1:sun 7:sat)</small>"), PSTR (D_CMND_TIMEZONE_STDD), 1,    7,   1,  Settings->tflag[0].dow,   PSTR (""));
  WSContentSend_P (PSTR_TZ_FIELDSET_STOP);

  // Daylight Saving Time section  
  // ----------------------------
  WSContentSend_P (PSTR_TZ_FIELDSET_START, "⛱️ Daylight Saving Time");
  WSContentSend_P (PSTR_TZ_FIELD_INPUT, PSTR ("Offset to GMT"),                         PSTR (D_CMND_TIMEZONE_DSTO), -720, 720, 30, Settings->toffset[1],     PSTR ("mn"));
  WSContentSend_P (PSTR_TZ_FIELD_INPUT, PSTR ("Month<small> (1:jan 12:dec)</small>"),   PSTR (D_CMND_TIMEZONE_DSTM), 1,    12,  1,  Settings->tflag[1].month, PSTR (""));
  WSContentSend_P (PSTR_TZ_FIELD_INPUT, PSTR ("Week<small> (0:last 4:fourth)</small>"), PSTR (D_CMND_TIMEZONE_DSTW), 0,    4,   1,  Settings->tflag[1].week,  PSTR (""));
  WSContentSend_P (PSTR_TZ_FIELD_INPUT, PSTR ("Week day<small> (1:sun 7:sat)</small>"), PSTR (D_CMND_TIMEZONE_DSTD), 1,    7,   1,  Settings->tflag[1].dow,   PSTR (""));
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

