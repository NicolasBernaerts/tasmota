/*
  xdrv_95_timezone.ino - Timezone management (~3.5 kb)
  
  Copyright (C) 2020  Nicolas Bernaerts
    04/04/2020 - v1.0 - Creation 

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

#define XDRV_95                  95
#define XSNS_95                  95

#define TIMEZONE_BUFFER_SIZE     128

#define D_PAGE_TIMEZONE_CONFIG   "tz"

#define D_CMND_TIMEZONE_STDO     "stdo"
#define D_CMND_TIMEZONE_STDM     "stdm"
#define D_CMND_TIMEZONE_STDW     "stdw"
#define D_CMND_TIMEZONE_STDD     "stdd"
#define D_CMND_TIMEZONE_DSTO     "dsto"
#define D_CMND_TIMEZONE_DSTM     "dstm"
#define D_CMND_TIMEZONE_DSTW     "dstw"
#define D_CMND_TIMEZONE_DSTD     "dstd"

#define D_JSON_TIMEZONE          "Timezone"
#define D_JSON_TIMEZONE_STD      "STD"
#define D_JSON_TIMEZONE_DST      "DST"
#define D_JSON_TIMEZONE_OFFSET   "Offset"
#define D_JSON_TIMEZONE_MONTH    "Month"
#define D_JSON_TIMEZONE_WEEK     "Week"
#define D_JSON_TIMEZONE_DAY      "Day"

// xdrv_95_timezone.ino
#define D_TIMEZONE               "Timezone"
#define D_TIMEZONE_CONFIG        "Configure"
#define D_TIMEZONE_TIME          "Time"
#define D_TIMEZONE_STD           "Standard Time"
#define D_TIMEZONE_DST           "Daylight Saving Time"
#define D_TIMEZONE_OFFSET        "Offset to GMT (mn)"
#define D_TIMEZONE_MONTH         "Month (1:jan ... 12:dec)"
#define D_TIMEZONE_WEEK          "Week (0:last ... 4:fourth)"
#define D_TIMEZONE_DAY           "Day of week (1:sun ... 7:sat)"

// offloading commands
enum TimezoneCommands { CMND_TIMEZONE_STDO, CMND_TIMEZONE_STDM, CMND_TIMEZONE_STDW, CMND_TIMEZONE_STDD, CMND_TIMEZONE_DSTO, CMND_TIMEZONE_DSTM, CMND_TIMEZONE_DSTW, CMND_TIMEZONE_DSTD };
const char kTimezoneCommands[] PROGMEM = D_CMND_TIMEZONE_STDO "|" D_CMND_TIMEZONE_STDM "|" D_CMND_TIMEZONE_STDW "|" D_CMND_TIMEZONE_STDD "|" D_CMND_TIMEZONE_DSTO "|" D_CMND_TIMEZONE_DSTM "|" D_CMND_TIMEZONE_DSTW "|" D_CMND_TIMEZONE_DSTD;

// form topic style
const char TIMEZONE_TOPIC_STYLE[] PROGMEM = "style='float:right;font-size:0.7rem;'";

/**************************************************\
 *                  Functions
\**************************************************/

// Show JSON status (for MQTT)
void TimezoneShowJSON (bool append)
{
  String  str_json;

  // start message  -->  {  or message,
  if (append == false) str_json = "{";
  else str_json = ",";

  // Timezone section start -->  "Timezone":{
  str_json += "\"" + String (D_JSON_TIMEZONE) + "\":{";

  // STD section -->  "STD":{"Offset":60,"Month":10,"Week":0,"Day":1}
  str_json += "\"" + String (D_JSON_TIMEZONE_STD) + "\":{";
  str_json += "\"" + String (D_JSON_TIMEZONE_OFFSET) + "\":" + String (Settings.toffset[0]) + ",";
  str_json += "\"" + String (D_JSON_TIMEZONE_MONTH) + "\":" + String (Settings.tflag[0].month) + ",";
  str_json += "\"" + String (D_JSON_TIMEZONE_WEEK) + "\":" + String (Settings.tflag[0].week) + ",";
  str_json += "\"" + String (D_JSON_TIMEZONE_DAY) + "\":" + String (Settings.tflag[0].dow) + "}";
  str_json += ",";

  // DST section -->  "DST":{"Offset":60,"Month":10,"Week":0,"Day":1}
  str_json += "\"" + String (D_JSON_TIMEZONE_DST) + "\":{";
  str_json += "\"" + String (D_JSON_TIMEZONE_OFFSET) + "\":" + String (Settings.toffset[1]) + ",";
  str_json += "\"" + String (D_JSON_TIMEZONE_MONTH) + "\":" + String (Settings.tflag[1].month) + ",";
  str_json += "\"" + String (D_JSON_TIMEZONE_WEEK) + "\":" + String (Settings.tflag[1].week) + ",";
  str_json += "\"" + String (D_JSON_TIMEZONE_DAY) + "\":" + String (Settings.tflag[1].dow) + "}";
  str_json += "}";

  // if append mode, add json string to MQTT message
  if (append == true) ResponseAppend_P (str_json.c_str ());

  // else, add last bracket and directly publish message
  else 
  {
    str_json += "}";
    Response_P (str_json.c_str ());
    MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));
  }
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
    case CMND_TIMEZONE_STDO:  // set timezone STD offset
      Settings.toffset[0] = atoi (XdrvMailbox.data);
      break;
    case CMND_TIMEZONE_STDM:  // set timezone STD month switch
      Settings.tflag[0].month = atoi (XdrvMailbox.data);
      break;
    case CMND_TIMEZONE_STDW:  // set timezone STD week of month switch
      Settings.tflag[0].week = atoi (XdrvMailbox.data);
      break;
    case CMND_TIMEZONE_STDD:  // set timezone STD day of week switch
      Settings.tflag[0].dow = atoi (XdrvMailbox.data);
      break;
    case CMND_TIMEZONE_DSTO:  // set timezone DST offset
      Settings.toffset[1] = atoi (XdrvMailbox.data);
      break;
    case CMND_TIMEZONE_DSTM:  // set timezone DST month switch
      Settings.tflag[1].month = atoi (XdrvMailbox.data);
      break;
    case CMND_TIMEZONE_DSTW:  // set timezone DST week of month switch
      Settings.tflag[1].week = atoi (XdrvMailbox.data);
      break;
    case CMND_TIMEZONE_DSTD:  // set timezone DST day of week switch
      Settings.tflag[1].dow = atoi (XdrvMailbox.data);
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
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s %s</button></form></p>"), D_PAGE_TIMEZONE_CONFIG, D_TIMEZONE_CONFIG, D_TIMEZONE);
}

// append local time to main page
bool TimezoneWebSensor ()
{
  TIME_T   current_dst;
  uint32_t current_time;

  // dislay local time
  current_time = LocalTime();
  BreakTime (current_time, current_dst);
  WSContentSend_PD (PSTR("{s}%s{m}%02d:%02d:%02d{e}"), D_TIMEZONE_TIME, current_dst.hour, current_dst.minute, current_dst.second);
}

// Timezone configuration web page
void TimezoneWebPageConfigure ()
{
  char argument[TIMEZONE_BUFFER_SIZE];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // page comes from save button on configuration page
  if (WebServer->hasArg("save"))
  {
    // set timezone STD offset according to 'stdo' parameter
    WebGetArg (D_CMND_TIMEZONE_STDO, argument, TIMEZONE_BUFFER_SIZE);
    if (strlen(argument) > 0) Settings.toffset[0] = atoi (argument);
    
    // set timezone STD month switch according to 'stdm' parameter
    WebGetArg (D_CMND_TIMEZONE_STDM, argument, TIMEZONE_BUFFER_SIZE);
    if (strlen(argument) > 0) Settings.tflag[0].month = atoi (argument);

    // set timezone STD week of month switch according to 'stdw' parameter
    WebGetArg (D_CMND_TIMEZONE_STDW, argument, TIMEZONE_BUFFER_SIZE);
    if (strlen(argument) > 0) Settings.tflag[0].week = atoi (argument);

    // set timezone STD day of week switch according to 'stdd' parameter
    WebGetArg (D_CMND_TIMEZONE_STDD, argument, TIMEZONE_BUFFER_SIZE);
    if (strlen(argument) > 0) Settings.tflag[0].dow = atoi (argument);

    // set timezone DST offset according to 'dsto' parameter
    WebGetArg (D_CMND_TIMEZONE_DSTO, argument, TIMEZONE_BUFFER_SIZE);
    if (strlen(argument) > 0) Settings.toffset[1] = atoi (argument);
    
    // set timezone DST month switch according to 'dstm' parameter
    WebGetArg (D_CMND_TIMEZONE_DSTM, argument, TIMEZONE_BUFFER_SIZE);
    if (strlen(argument) > 0) Settings.tflag[1].month = atoi (argument);

    // set timezone DST week of month switch according to 'dstw' parameter
    WebGetArg (D_CMND_TIMEZONE_DSTW, argument, TIMEZONE_BUFFER_SIZE);
    if (strlen(argument) > 0) Settings.tflag[1].week = atoi (argument);

    // set timezone DST day of week switch according to 'dstd' parameter
    WebGetArg (D_CMND_TIMEZONE_DSTD, argument, TIMEZONE_BUFFER_SIZE);
    if (strlen(argument) > 0) Settings.tflag[1].dow = atoi (argument);
  }

  // beginning of form
  WSContentStart_P (D_TIMEZONE_CONFIG);
  WSContentSendStyle ();
  WSContentSend_P (PSTR("<form method='get' action='%s'>\n"), D_PAGE_TIMEZONE_CONFIG);

  // Standard Time section  
  // ---------------------
  WSContentSend_P (PSTR("<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>"), D_TIMEZONE_STD);
  WSContentSend_P (PSTR ("<p>%s<span %s>%s</span><br><input type='number' name='%s' min='-720' max='720' step='1' value='%d'></p>\n"), D_TIMEZONE_OFFSET, TIMEZONE_TOPIC_STYLE, D_CMND_TIMEZONE_STDO, D_CMND_TIMEZONE_STDO, Settings.toffset[0]);
  WSContentSend_P (PSTR ("<p>%s<span %s>%s</span><br><input type='number' name='%s' min='1' max='12' step='1' value='%d'></p>\n"), D_TIMEZONE_MONTH, TIMEZONE_TOPIC_STYLE, D_CMND_TIMEZONE_STDM, D_CMND_TIMEZONE_STDM, Settings.tflag[0].month);
  WSContentSend_P (PSTR ("<p>%s<span %s>%s</span><br><input type='number' name='%s' min='0' max='4' step='1' value='%d'></p>\n"), D_TIMEZONE_WEEK, TIMEZONE_TOPIC_STYLE, D_CMND_TIMEZONE_STDW, D_CMND_TIMEZONE_STDW, Settings.tflag[0].week);
  WSContentSend_P (PSTR ("<p>%s<span %s>%s</span><br><input type='number' name='%s' min='1' max='7' step='1' value='%d'></p>\n"), D_TIMEZONE_DAY, TIMEZONE_TOPIC_STYLE, D_CMND_TIMEZONE_STDD, D_CMND_TIMEZONE_STDD, Settings.tflag[0].dow);
  WSContentSend_P (PSTR("</fieldset></p>\n"));

  // Daylight Saving Time section  
  // ----------------------------
  WSContentSend_P (PSTR("<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>"), D_TIMEZONE_DST);
  WSContentSend_P (PSTR ("<p>%s<span %s>%s</span><br><input type='number' name='%s' min='-720' max='720' step='1' value='%d'></p>\n"), D_TIMEZONE_OFFSET, TIMEZONE_TOPIC_STYLE, D_CMND_TIMEZONE_DSTO, D_CMND_TIMEZONE_DSTO, Settings.toffset[1]);
  WSContentSend_P (PSTR ("<p>%s<span %s>%s</span><br><input type='number' name='%s' min='1' max='12' step='1' value='%d'></p>\n"), D_TIMEZONE_MONTH, TIMEZONE_TOPIC_STYLE, D_CMND_TIMEZONE_DSTM, D_CMND_TIMEZONE_DSTM, Settings.tflag[1].month);
  WSContentSend_P (PSTR ("<p>%s<span %s>%s</span><br><input type='number' name='%s' min='0' max='4' step='1' value='%d'></p>\n"), D_TIMEZONE_WEEK, TIMEZONE_TOPIC_STYLE, D_CMND_TIMEZONE_DSTW, D_CMND_TIMEZONE_DSTW, Settings.tflag[1].week);
  WSContentSend_P (PSTR ("<p>%s<span %s>%s</span><br><input type='number' name='%s' min='1' max='7' step='1' value='%d'></p>\n"), D_TIMEZONE_DAY, TIMEZONE_TOPIC_STYLE, D_CMND_TIMEZONE_DSTD, D_CMND_TIMEZONE_DSTD, Settings.tflag[1].dow);
  WSContentSend_P (PSTR("</fieldset></p>\n"));

  // save button  
  // --------------
  WSContentSend_P (PSTR ("<p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), D_SAVE);
  WSContentSend_P (PSTR("</form>\n"));

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
      TimezoneShowJSON (true);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      WebServer->on ("/" D_PAGE_TIMEZONE_CONFIG, TimezoneWebPageConfigure);
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
