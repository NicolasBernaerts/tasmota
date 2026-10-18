/*
  xdrv_99_02_timezone.ino - Timezone management
  
  Copyright (C) 2020-2025  Nicolas Bernaerts
    20/04/2021 v1.0 - Creation 
    29/03/2026 v2.0 - Separate form misc options

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

#ifdef USE_TIMEZONE

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

// timezone setting commands
const char kTimezoneCommands[]         PROGMEM = "tz|"             "|"      "_pub"       "|"     "_ntp"   " |"     "_stdo"    "|"     "_stdm"    "|"     "_stdw"    "|"     "_stdd"    "|"      "_dsto"   "|"      "_dstm"   "|"     "_dstw"    "|"     "_dstd";
void (* const TimezoneCommand[])(void) PROGMEM = { &CmndTimezoneHelp, &CmndTimezonePublish, &CmndTimezoneNtp, &CmndTimezoneStdO, &CmndTimezoneStdM, &CmndTimezoneStdW, &CmndTimezoneStdD, &CmndTimezoneDstO, &CmndTimezoneDstM, &CmndTimezoneDstW, &CmndTimezoneDstD };

/**************************************************\
 *                  Variables
\**************************************************/

static struct {
  bool publish = false;
} misc_timezone;

/**************************************************\
 *                    Timezone
\**************************************************/

// timezone help
void CmndTimezoneHelp ()
{
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Timezone commands :"));

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
void TimezoneAppendSensor ()
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
 *                  Call back
\**************************************************/

// init main status
void TimezoneInit ()
{
  // set switch mode
  Settings->timezone = 99;

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Run tz to get help on Timezone commands"));
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// IP address configuration web page
bool TimezoneWebConfigGetArgs ()
{
  char str_argument[32];

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

  return false;
}

// Timezone configuration
void TimezoneWebConfigDisplay ()
{
  // NTP server section  
  // ---------------------
  WSContentSend_P (PSTR ("<fieldset><legend>%s</legend>\n"), PSTR ("🕛 Time server"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  WSContentSend_P (PSTR ("<p class='dat'>%s</p>\n"), PSTR ("First server"));
  WSContentSend_P (PSTR ("<p class='dat'><input type='text' name='%s' value='%s'></p>\n"), PSTR (D_CMND_TIMEZONE_NTP), SettingsText(SET_NTPSERVER1));
  WSContentSend_P (PSTR ("<hr>\n"));

  // Standard Time section  
  // ---------------------
  WSContentSend_P (PSTR ("<p class='head'>%s</p>\n"), PSTR ("❄️ Standard Time"));
  WSContentSend_P (PSTR ("<p class='dat'><span class='hea'>%s</span><span class='val'><input type='number' name='%s' min='%d' max='%d' step='%d' value='%d'></span><span class='uni'>%s</span></p>\n"), PSTR ("Offset to GMT"),                         PSTR (D_CMND_TIMEZONE_STDO), -720, 720, 30, Settings->toffset[0],     PSTR ("mn"));
  WSContentSend_P (PSTR ("<p class='dat'><span class='hea'>%s</span><span class='val'><input type='number' name='%s' min='%d' max='%d' step='%d' value='%d'></span><span class='uni'>%s</span></p>\n"), PSTR ("Month<small> (1:jan 12:dec)</small>"),   PSTR (D_CMND_TIMEZONE_STDM), 1,    12,  1,  Settings->tflag[0].month, PSTR (""));
  WSContentSend_P (PSTR ("<p class='dat'><span class='hea'>%s</span><span class='val'><input type='number' name='%s' min='%d' max='%d' step='%d' value='%d'></span><span class='uni'>%s</span></p>\n"), PSTR ("Week<small> (0:last 4:fourth)</small>"), PSTR (D_CMND_TIMEZONE_STDW), 0,    4,   1,  Settings->tflag[0].week,  PSTR (""));
  WSContentSend_P (PSTR ("<p class='dat'><span class='hea'>%s</span><span class='val'><input type='number' name='%s' min='%d' max='%d' step='%d' value='%d'></span><span class='uni'>%s</span></p>\n"), PSTR ("Week day<small> (1:sun 7:sat)</small>"), PSTR (D_CMND_TIMEZONE_STDD), 1,    7,   1,  Settings->tflag[0].dow,   PSTR (""));
  WSContentSend_P (PSTR ("<hr>\n"));

  // Daylight Saving Time section  
  // ----------------------------
  WSContentSend_P (PSTR ("<p class='head'>%s</p>\n"), PSTR ("⛱️ Daylight Saving Time"));
  WSContentSend_P (PSTR ("<p class='dat'><span class='hea'>%s</span><span class='val'><input type='number' name='%s' min='%d' max='%d' step='%d' value='%d'></span><span class='uni'>%s</span></p>\n"), PSTR ("Offset to GMT"),                         PSTR (D_CMND_TIMEZONE_DSTO), -720, 720, 30, Settings->toffset[1],     PSTR ("mn"));
  WSContentSend_P (PSTR ("<p class='dat'><span class='hea'>%s</span><span class='val'><input type='number' name='%s' min='%d' max='%d' step='%d' value='%d'></span><span class='uni'>%s</span></p>\n"), PSTR ("Month<small> (1:jan 12:dec)</small>"),   PSTR (D_CMND_TIMEZONE_DSTM), 1,    12,  1,  Settings->tflag[1].month, PSTR (""));
  WSContentSend_P (PSTR ("<p class='dat'><span class='hea'>%s</span><span class='val'><input type='number' name='%s' min='%d' max='%d' step='%d' value='%d'></span><span class='uni'>%s</span></p>\n"), PSTR ("Week<small> (0:last 4:fourth)</small>"), PSTR (D_CMND_TIMEZONE_DSTW), 0,    4,   1,  Settings->tflag[1].week,  PSTR (""));
  WSContentSend_P (PSTR ("<p class='dat'><span class='hea'>%s</span><span class='val'><input type='number' name='%s' min='%d' max='%d' step='%d' value='%d'></span><span class='uni'>%s</span></p>\n"), PSTR ("Week day<small> (1:sun 7:sat)</small>"), PSTR (D_CMND_TIMEZONE_DSTD), 1,    7,   1,  Settings->tflag[1].dow,   PSTR (""));

  WSContentSend_P (PSTR ("</div>\n"));
  WSContentSend_P (PSTR ("</fieldset>\n"));
}

#endif  // USE_WEBSERVER

/***********************************************************\
 *                      Interface
\***********************************************************/

bool XdrvTimezone (uint32_t function)
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
      if (misc_timezone.publish && (TasmotaGlobal.tele_period == 0)) TimezoneAppendSensor ();
      break;
  }

  return result;
}

#endif    // USE_TIMEZONE
