/*
  xdrv_95_timezone.ino - Timezone management (~3.2 kb)
  
  Copyright (C) 2020  Nicolas Bernaerts
    04/04/2020 - v1.0 - Creation 
    19/05/2020 - v1.1 - Add configuration for first NTP server 
    22/07/2020 - v1.2 - Memory optimisation 
    10/04/2021 - v1.3 - Remove use of String to avoid heap fragmentation 

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

#define XDRV_95                   95
#define XSNS_95                   95

// constants
#define TIMEZONE_JSON_MAX         112

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
const char D_TIMEZONE_PAGE_CONFIG[] PROGMEM = "/tz";

// dialog strings
const char D_TIMEZONE[]        PROGMEM = "Timezone";
const char D_TIMEZONE_CONFIG[] PROGMEM = "Configure";
const char D_TIMEZONE_NTP[]    PROGMEM = "First time server";
const char D_TIMEZONE_TIME[]   PROGMEM = "Time";
const char D_TIMEZONE_STD[]    PROGMEM = "Standard Time";
const char D_TIMEZONE_DST[]    PROGMEM = "Daylight Saving Time";
const char D_TIMEZONE_OFFSET[] PROGMEM = "Offset to GMT (mn)";
const char D_TIMEZONE_MONTH[]  PROGMEM = "Month (1:jan ... 12:dec)";
const char D_TIMEZONE_WEEK[]   PROGMEM = "Week (0:last ... 4:fourth)";
const char D_TIMEZONE_DAY[]    PROGMEM = "Day of week (1:sun ... 7:sat)";
const char D_TIMEZONE_JSON[]   PROGMEM = "\"Timezone\":{\"STD\":{\"Offset\":%d,\"Month\":%d,\"Week\":%d,\"Day\":%d},\"DST\":{\"Offset\":%d,\"Month\":%d,\"Week\":%d,\"Day\":%d}}";

// offloading commands
enum TimezoneCommands { CMND_TIMEZONE_NTP, CMND_TIMEZONE_STDO, CMND_TIMEZONE_STDM, CMND_TIMEZONE_STDW, CMND_TIMEZONE_STDD, CMND_TIMEZONE_DSTO, CMND_TIMEZONE_DSTM, CMND_TIMEZONE_DSTW, CMND_TIMEZONE_DSTD };
const char kTimezoneCommands[] PROGMEM = "ntp|stdo|stdm|sdtw|stdd|dsto|dstm|dstw|dstd";

// constant strings
const char TZ_FIELDSET_START[] PROGMEM = "<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>\n";
const char TZ_FIELDSET_STOP[]  PROGMEM = "</fieldset></p>\n";
const char TZ_FIELD_INPUT[]    PROGMEM = "<p>%s<span style='float:right;font-size:0.7rem;'>%s</span><br><input type='number' name='%s' min='%d' max='%d' step='%d' value='%d'></p>\n";

/**************************************************\
 *                  Functions
\**************************************************/

// Show JSON status (for MQTT)
void TimezoneShowJSON (bool append)
{
  char str_json[TIMEZONE_JSON_MAX];

  // generate string
  sprintf (str_json, D_TIMEZONE_JSON, Settings.toffset[0], Settings.tflag[0].month, Settings.tflag[0].week, Settings.tflag[0].dow, Settings.toffset[1], Settings.tflag[1].month, Settings.tflag[1].week, Settings.tflag[1].dow);

  // if append mode, add json string to MQTT message
  if (append) ResponseAppend_P (PSTR (",%s"), str_json);
  else Response_P (PSTR ("{%s}"), str_json);
  
  // publish it if not in append mode
  if (!append) MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));
}

// init main status
void TimezoneInit ()
{
  // set switch mode
  Settings.timezone = 99;
}

// Handle timezone MQTT commands
bool TimezoneMqttCommand ()
{
  bool command_handled = true;
  int  command_code;
  char command [CMDSZ];

  // check MQTT command
  command_code = GetCommandCode (command, sizeof(command), XdrvMailbox.topic, kTimezoneCommands);

  // handle command
  switch (command_code)
  {
    case CMND_TIMEZONE_NTP:  // set 1st NTP server
      SettingsUpdateText(SET_NTPSERVER1, XdrvMailbox.data);
      break;
    case CMND_TIMEZONE_STDO:  // set timezone STD offset
      Settings.toffset[0] = XdrvMailbox.payload;
      break;
    case CMND_TIMEZONE_STDM:  // set timezone STD month switch
      Settings.tflag[0].month = XdrvMailbox.payload;
      break;
    case CMND_TIMEZONE_STDW:  // set timezone STD week of month switch
      Settings.tflag[0].week = XdrvMailbox.payload;
      break;
    case CMND_TIMEZONE_STDD:  // set timezone STD day of week switch
      Settings.tflag[0].dow = XdrvMailbox.payload;
      break;
    case CMND_TIMEZONE_DSTO:  // set timezone DST offset
      Settings.toffset[1] = XdrvMailbox.payload;
      break;
    case CMND_TIMEZONE_DSTM:  // set timezone DST month switch
      Settings.tflag[1].month = XdrvMailbox.payload;
      break;
    case CMND_TIMEZONE_DSTW:  // set timezone DST week of month switch
      Settings.tflag[1].week = XdrvMailbox.payload;
      break;
    case CMND_TIMEZONE_DSTD:  // set timezone DST day of week switch
      Settings.tflag[1].dow = XdrvMailbox.payload;
      break;
   default:
      command_handled = false;
  }

  // if needed, send updated status
  if (command_handled == true) TimezoneShowJSON (false);

  return command_handled;
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// timezone configuration page button
void TimezoneWebConfigButton ()
{
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s %s</button></form></p>"), D_TIMEZONE_PAGE_CONFIG, D_TIMEZONE_CONFIG, D_TIMEZONE);
}

// append local time to main page
void TimezoneWebSensor ()
{
  TIME_T   current_dst;
  uint32_t current_time;

  // dislay local time
  current_time = LocalTime ();
  BreakTime (current_time, current_dst);
  WSContentSend_PD (PSTR ("<tr><div style='text-align:center;'>%02d:%02d:%02d</div></tr>\n"), current_dst.hour, current_dst.minute, current_dst.second);
}

// Timezone configuration web page
void TimezoneWebPageConfigure ()
{
  char argument[32];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg("save"))
  {
    // set first time server
    WebGetArg (D_CMND_TIMEZONE_NTP, argument, sizeof (argument));
    if (strlen (argument) > 0) SettingsUpdateText (SET_NTPSERVER1, argument);

    // set timezone STD offset according to 'stdo' parameter
    WebGetArg (D_CMND_TIMEZONE_STDO, argument, sizeof (argument));
    if (strlen (argument) > 0) Settings.toffset[0] = atoi (argument);
    
    // set timezone STD month switch according to 'stdm' parameter
    WebGetArg (D_CMND_TIMEZONE_STDM, argument, sizeof (argument));
    if (strlen (argument) > 0) Settings.tflag[0].month = atoi (argument);

    // set timezone STD week of month switch according to 'stdw' parameter
    WebGetArg (D_CMND_TIMEZONE_STDW, argument, sizeof (argument));
    if (strlen (argument) > 0) Settings.tflag[0].week = atoi (argument);

    // set timezone STD day of week switch according to 'stdd' parameter
    WebGetArg (D_CMND_TIMEZONE_STDD, argument, sizeof (argument));
    if (strlen (argument) > 0) Settings.tflag[0].dow = atoi (argument);

    // set timezone DST offset according to 'dsto' parameter
    WebGetArg (D_CMND_TIMEZONE_DSTO, argument, sizeof (argument));
    if (strlen (argument) > 0) Settings.toffset[1] = atoi (argument);
    
    // set timezone DST month switch according to 'dstm' parameter
    WebGetArg (D_CMND_TIMEZONE_DSTM, argument, sizeof (argument));
    if (strlen (argument) > 0) Settings.tflag[1].month = atoi (argument);

    // set timezone DST week of month switch according to 'dstw' parameter
    WebGetArg (D_CMND_TIMEZONE_DSTW, argument, sizeof (argument));
    if (strlen (argument) > 0) Settings.tflag[1].week = atoi (argument);

    // set timezone DST day of week switch according to 'dstd' parameter
    WebGetArg (D_CMND_TIMEZONE_DSTD,argument, sizeof (argument));
    if (strlen (argument) > 0) Settings.tflag[1].dow = atoi (argument);
  }

  // beginning of form
  WSContentStart_P (D_TIMEZONE_CONFIG);
  WSContentSendStyle ();
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_TIMEZONE_PAGE_CONFIG);

  // NTP server section  
  // ---------------------
  WSContentSend_P (TZ_FIELDSET_START, D_TIMEZONE_NTP);
  WSContentSend_P (PSTR ("<p>%s<span style='float:right;font-size:0.7rem;'>%s</span><br><input type='text' name='%s' value='%s'></p>\n"), D_TIMEZONE_NTP, PSTR (D_CMND_TIMEZONE_NTP), PSTR (D_CMND_TIMEZONE_NTP), SettingsText(SET_NTPSERVER1));
  WSContentSend_P (TZ_FIELDSET_STOP);

  // Standard Time section  
  // ---------------------
  WSContentSend_P (TZ_FIELDSET_START, D_TIMEZONE_STD);
  WSContentSend_P (TZ_FIELD_INPUT, D_TIMEZONE_OFFSET, PSTR (D_CMND_TIMEZONE_STDO), PSTR (D_CMND_TIMEZONE_STDO), -720, 720, 1, Settings.toffset[0]);
  WSContentSend_P (TZ_FIELD_INPUT, D_TIMEZONE_MONTH,  PSTR (D_CMND_TIMEZONE_STDM), PSTR (D_CMND_TIMEZONE_STDM), 1,    12,  1, Settings.tflag[0].month);
  WSContentSend_P (TZ_FIELD_INPUT, D_TIMEZONE_WEEK,   PSTR (D_CMND_TIMEZONE_STDW), PSTR (D_CMND_TIMEZONE_STDW), 0,    4,   1, Settings.tflag[0].week);
  WSContentSend_P (TZ_FIELD_INPUT, D_TIMEZONE_DAY,    PSTR (D_CMND_TIMEZONE_STDD), PSTR (D_CMND_TIMEZONE_STDD), 1,    7,   1, Settings.tflag[0].dow);
  WSContentSend_P (TZ_FIELDSET_STOP);

  // Daylight Saving Time section  
  // ----------------------------
  WSContentSend_P (TZ_FIELDSET_START, D_TIMEZONE_DST);
  WSContentSend_P (TZ_FIELD_INPUT, D_TIMEZONE_OFFSET, PSTR (D_CMND_TIMEZONE_DSTO), PSTR (D_CMND_TIMEZONE_DSTO), -720, 720, 1, Settings.toffset[1]);
  WSContentSend_P (TZ_FIELD_INPUT, D_TIMEZONE_MONTH,  PSTR (D_CMND_TIMEZONE_DSTM), PSTR (D_CMND_TIMEZONE_DSTM), 1,    12,  1, Settings.tflag[1].month);
  WSContentSend_P (TZ_FIELD_INPUT, D_TIMEZONE_WEEK,   PSTR (D_CMND_TIMEZONE_DSTW), PSTR (D_CMND_TIMEZONE_DSTW), 0,    4,   1, Settings.tflag[1].week);
  WSContentSend_P (TZ_FIELD_INPUT, D_TIMEZONE_DAY,    PSTR (D_CMND_TIMEZONE_DSTD), PSTR (D_CMND_TIMEZONE_DSTD), 1,    7,   1, Settings.tflag[1].dow);
  WSContentSend_P (TZ_FIELDSET_STOP);

  // save button  
  // --------------
  WSContentSend_P (PSTR ("<p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), D_SAVE);
  WSContentSend_P (PSTR ("</form>\n"));

  // configuration button
  WSContentSpaceButton(BUTTON_CONFIGURATION);

  // end of page
  WSContentStop();
}

#endif  // USE_WEBSERVER

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xdrv95 (uint8_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
    case FUNC_INIT:
      TimezoneInit ();
      break;
    case FUNC_COMMAND:
      result = TimezoneMqttCommand ();
      break;
  }
  
  return result;
}

bool Xsns95 (uint8_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
    case FUNC_JSON_APPEND:
      //TimezoneShowJSON (true);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      Webserver->on (FPSTR (D_TIMEZONE_PAGE_CONFIG), TimezoneWebPageConfigure);
      break;
    case FUNC_WEB_ADD_BUTTON:
      TimezoneWebConfigButton ();
      break;
    case FUNC_WEB_SENSOR:
      TimezoneWebSensor ();
      break;
#endif  // USE_WEBSERVER

  }
  
  return result;
}

#endif // USE_TIMEZONE
