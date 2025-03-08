/*
  xsns_126_influxdb_extension.ino - InfluxDB 2 extension to handle FUNC_API_INFLUXDB message

  Copyright (C) 2024  Nicolas Bernaerts

  Version history :
    25/09/2024 - v1.0 - Creation
    18/02/2025 - v1.1 - Switch port number to Settings->influxdb_port
                        Handle InfluxDB version

  Configuration values are stored in :

    - Settings->rf_code[16][5]              : Flag to enable/disable influxdb extension

  FUNC_API_INFLUXDB should be added to XsnsFunctions in tasmota.h

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
#ifdef USE_INFLUXDB

// declare influxdb extension
#define XSNS_126                    126

// web URL
#define INFLUXDB_PAGE_CFG           "/influx"

// Commands
static const char kInfluxDbExtensionCommands[]  PROGMEM = "Ifx" "|"    "Tic"           "|"           "Version";
void (* const InfluxDbExtensionCommand[])(void) PROGMEM = { &CmndInfluxDbExtensionEnable, &CmndInfluxDbExtensionVersion };

/**************************************\
 *               Data
\**************************************/

static struct {
  uint8_t enabled = 0;                  // flag to enable integration
  uint8_t ready   = 0;                  // all data ready for publication
  uint8_t init    = 0;                  // influxdb initialisation done
  uint8_t publish = 0;                  // flag to publish data
} influxdb_extension;

/**********************************************\
 *                Configuration
\**********************************************/

// load configuration
void InfluxDbExtensionLoadConfig () 
{
  influxdb_extension.enabled = Settings->rf_code[16][5];
}

// save configuration
void InfluxDbExtensionSaveConfig () 
{
  Settings->rf_code[16][5] = influxdb_extension.enabled;
}

/***************************************\
 *               Function
\***************************************/

// set integration
void InfluxDbExtensionSet (const bool enabled) 
{
  // update status
  if (enabled) influxdb_extension.enabled = 1;
    else influxdb_extension.enabled = 0;

  // save configuration
  InfluxDbExtensionSaveConfig ();
}

// get integration
bool InfluxDbExtensionGet () 
{
  return (influxdb_extension.enabled == 1);
}

void InfluxDbExtensionData ()
{
  // check if enabled, ready and if there is conso or production 
  if (TasmotaGlobal.global_state.network_down) return;
  if (!influxdb_extension.enabled) return;
  if (!influxdb_extension.ready) return;

  // set publication flag
  influxdb_extension.publish = 1;
}

void InfluxDbExtensionPublishData ()
{
  // if nothing to publish, ignore
  if (TasmotaGlobal.global_state.network_down) return;
  if (!influxdb_extension.enabled) return;
  if (!influxdb_extension.ready) return;

  // if needed, init connexion
  if (!influxdb_extension.init)
  {
    InfluxDbLoop ();
    influxdb_extension.init = 1;
  }

  // collect data from drivers and sensors
  TasmotaGlobal.mqtt_data = "";
  XdrvXsnsCall (FUNC_API_INFLUXDB);

  // if needed, publish data
  if (TasmotaGlobal.mqtt_data.length () > 0)
  {
    InfluxDbPostData (TasmotaGlobal.mqtt_data.c_str ());
    AddLog (LOG_LEVEL_INFO,  PSTR("IDB: Donnees emises"));
    TasmotaGlobal.mqtt_data = "";
  }

  // reset publication flag
  influxdb_extension.publish = 0;
}

/*******************************\
 *          Command
\*******************************/

// Enable influxDB Teleinfo extension
void CmndInfluxDbExtensionEnable ()
{
  if (XdrvMailbox.data_len > 0) InfluxDbExtensionSet (XdrvMailbox.payload != 0);
  ResponseCmndNumber (influxdb_extension.enabled);
}


// Set InfluxDB version
void CmndInfluxDbExtensionVersion ()
{
  if (XdrvMailbox.data_len > 0) Settings->influxdb_version = (uint8_t)XdrvMailbox.payload;
  ResponseCmndNumber (Settings->influxdb_version);
}

/****************************\
 *        Callback
\****************************/

// Domoticz init message
void InfluxDbExtensionInit ()
{
  // load config 
  InfluxDbExtensionLoadConfig ();

  // check if all influxdb data are defined
  influxdb_extension.ready = 1;
  if (strlen (SettingsText(SET_INFLUXDB_HOST))   == 0) influxdb_extension.ready = 0;
  if (strlen (SettingsText(SET_INFLUXDB_ORG))    == 0) influxdb_extension.ready = 0;
  if (strlen (SettingsText(SET_INFLUXDB_BUCKET)) == 0) influxdb_extension.ready = 0;
  if (strlen (SettingsText(SET_INFLUXDB_TOKEN))  == 0) influxdb_extension.ready = 0;
  if (Settings->influxdb_port == 0)                    influxdb_extension.ready = 0;
}

// called every second
void InfluxDbExtensionEverySecond ()
{
  // if nothing to publish, ignore
  if (!influxdb_extension.publish) return;

  // publish data
  InfluxDbExtensionPublishData ();
}

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

// Append InfluxDB configuration button
void InfluxDbExtensionWebConfigButton ()
{
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>Configure InfluxDB</button></form></p>\n"), PSTR (INFLUXDB_PAGE_CFG));
}

// Teleinfo web page
void InfluxDbExtensionWebPageConfigure ()
{
  bool        status, actual;
  bool        restart = false;
  uint8_t     index;
  uint16_t    value; 
  uint32_t    baudrate;
  char        str_select[16];
  char        str_title[40];
  char        str_text[64];
  const char *pstr_title;

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess ()) return;

  char str_value[128];

  // page comes from save button on configuration page
  if (Webserver->hasArg (F ("save")))
  {
    // retrieve parameters
    if (Webserver->hasArg (F ("idb_host"))) { WebGetArg (PSTR ("idb_host"), str_value, sizeof (str_value)); SettingsUpdateText (SET_INFLUXDB_HOST,   str_value); }
    if (Webserver->hasArg (F ("idb_orga"))) { WebGetArg (PSTR ("idb_orga"), str_value, sizeof (str_value)); SettingsUpdateText (SET_INFLUXDB_ORG,    str_value); }
    if (Webserver->hasArg (F ("idb_bukt"))) { WebGetArg (PSTR ("idb_bukt"), str_value, sizeof (str_value)); SettingsUpdateText (SET_INFLUXDB_BUCKET, str_value); }
    if (Webserver->hasArg (F ("idb_tokn"))) { WebGetArg (PSTR ("idb_tokn"), str_value, sizeof (str_value)); SettingsUpdateText (SET_INFLUXDB_TOKEN,  str_value); }
    if (Webserver->hasArg (F ("idb_port"))) { WebGetArg (PSTR ("idb_port"), str_value, sizeof (str_value)); Settings->influxdb_port = (uint16_t)atoi (str_value); }
    if (Webserver->hasArg (F ("idb_ver")))  { WebGetArg (PSTR ("idb_ver"),  str_value, sizeof (str_value)); Settings->influxdb_version = (uint8_t)atoi (str_value); }

    // ask for restart
    WebRestart (1);
  }

  // beginning of form
  WSContentStart_P (PSTR ("Configure InfluxDB"));

    // page style
  WSContentSendStyle ();
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("p,br,hr {clear:both;}\n"));
  WSContentSend_P (PSTR ("fieldset {background:#333;margin:15px 0px;border:none;border-radius:8px;}\n")); 
  WSContentSend_P (PSTR ("legend {font-weight:bold;margin:0px;padding:5px;color:#888;background:transparent;}\n")); 
  WSContentSend_P (PSTR ("legend:after {content:'';display:block;height:1px;margin:15px 0px;}\n"));
  WSContentSend_P (PSTR ("div.main {padding:0px;margin-top:-25px;}\n"));
  WSContentSend_P (PSTR ("input,select {margin-bottom:10px;text-align:center;border:none;border-radius:4px;}\n"));
  WSContentSend_P (PSTR ("input[type='text'] {text-align:left;}\n"));
  WSContentSend_P (PSTR ("span {float:left;padding-top:4px;}\n"));
  WSContentSend_P (PSTR ("span.hval {width:70%%;}\n"));
  WSContentSend_P (PSTR ("span.htxt {width:100%%;}\n"));
  WSContentSend_P (PSTR ("span.val {width:30%%;padding:0px;}\n"));
  WSContentSend_P (PSTR ("span.sel {width:100%%;padding:0px;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // form
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), PSTR (INFLUXDB_PAGE_CFG));

  WSContentSend_P (PSTR ("<fieldset><legend>InfluxDB Server</legend>\n"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  WSContentSend_P (PSTR ("<p class='dat'><span class='hval'>%s</span><span class='val'><input type='number' name='idb_%s' min=1 max=2 step=1 value=%u></span></p>\n"),     PSTR ("Version"),     PSTR ("ver"),  Settings->influxdb_version);
  WSContentSend_P (PSTR ("<p class='dat'><span class='hval'>%s</span><span class='val'><input type='number' name='idb_%s' min=1 max=65535 step=1 value=%u></span></p>\n"), PSTR ("Port"),        PSTR ("port"), Settings->influxdb_port);
  WSContentSend_P (PSTR ("<p class='dat'><span class='htxt'>%s</span><span class='sel'><input type='text' name='idb_%s' value='%s'></span></p>\n"),                        PSTR ("Host"),        PSTR ("host"), SettingsText (SET_INFLUXDB_HOST));
  WSContentSend_P (PSTR ("<p class='dat'><span class='htxt'>%s</span><span class='sel'><input type='text' name='idb_%s' value='%s'></span></p>\n"),                        PSTR ("User/Orga"),   PSTR ("orga"), SettingsText (SET_INFLUXDB_ORG));
  WSContentSend_P (PSTR ("<p class='dat'><span class='htxt'>%s</span><span class='sel'><input type='text' name='idb_%s' value='%s'></span></p>\n"),                        PSTR ("DB/Bucket"),   PSTR ("bukt"), SettingsText (SET_INFLUXDB_BUCKET));
  WSContentSend_P (PSTR ("<p class='dat'><span class='htxt'>%s</span><span class='sel'><input type='text' name='idb_%s' value='%s'></span></p>\n"),                        PSTR ("Passw/Token"), PSTR ("tokn"), SettingsText (SET_INFLUXDB_TOKEN));

  WSContentSend_P (PSTR ("</div>\n"));
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

// InfluxDB driver extension
bool Xsns126 (uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
    case FUNC_INIT:
      InfluxDbExtensionInit ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kInfluxDbExtensionCommands, InfluxDbExtensionCommand);
      break;
   case FUNC_EVERY_SECOND:
      InfluxDbExtensionEverySecond ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_BUTTON:
      InfluxDbExtensionWebConfigButton ();
      break;
    case FUNC_WEB_ADD_HANDLER:
      Webserver->on (F (INFLUXDB_PAGE_CFG), InfluxDbExtensionWebPageConfigure);
    break;
#endif    // USE_WEBSERVER
  }

  return result;
}

#endif      // USE_INFLUXDB
#endif      // ESP32
