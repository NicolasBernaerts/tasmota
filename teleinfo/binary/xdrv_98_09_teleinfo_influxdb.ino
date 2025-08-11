/*
  xdrv_98_09_teleinfo_influxdb.ino - InfluxDB 2 extension for teleinfo

  Only compatible with ESP32

  Copyright (C) 2024-2025  Nicolas Bernaerts

  Version history :
    25/09/2024 v1.0 - Creation
    18/02/2025 v1.1 - Switch port number to Settings->influxdb_port
                      Handle InfluxDB version
    10/07/2025 v2.0 - Refactoring based on Tasmota 15

  Configuration values are stored in :

    - Settings->rf_code[16][5]              : Flag to enable/disable influxdb extension

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

// web URL
#define INFLUXDB_PAGE_CFG           "/influx"

// Commands
static const char kTeleinfoInfluxDbCommands[]  PROGMEM = "Ifx" "|"    "Tic"          "|"         "Version";
void (* const TeleinfoInfluxDbCommand[])(void) PROGMEM = { &CmndTeleinfoInfluxDbEnable, &CmndTeleinfoInfluxDbVersion };

/**************************************\
 *               Data
\**************************************/

static struct {
  uint8_t enabled = 0;                  // flag to enable integration
  uint8_t ready   = 0;                  // all data ready for publication
  uint8_t init    = 0;                  // influxdb initialisation done
  uint8_t publish = 0;                  // flag to publish data
} teleinfo_influxdb;

/**********************************************\
 *                Configuration
\**********************************************/

// load configuration
void TeleinfoInfluxDbLoadConfig () 
{
  teleinfo_influxdb.enabled = Settings->rf_code[16][5];
}

// save configuration
void TeleinfoInfluxDbSaveConfig () 
{
  Settings->rf_code[16][5] = teleinfo_influxdb.enabled;
}

/***************************************\
 *               Function
\***************************************/

// set integration
void TeleinfoInfluxDbSet (const bool enabled) 
{
  // update status
  if (enabled) teleinfo_influxdb.enabled = 1;
    else teleinfo_influxdb.enabled = 0;

  // save configuration
  TeleinfoInfluxDbSaveConfig ();
}

// get integration
bool TeleinfoInfluxDbGet () 
{
  return (teleinfo_influxdb.enabled == 1);
}

void TeleinfoInfluxDbData ()
{
  // check if enabled, ready and if there is conso or production 
  if (TasmotaGlobal.global_state.network_down) return;
  if (!teleinfo_influxdb.enabled) return;
  if (!teleinfo_influxdb.ready) return;

  // set publication flag
  teleinfo_influxdb.publish = 1;
}

// called to generate InfluxDB data
void TeleinfoDriverApiInfluxDb ()
{
  uint8_t phase, index;
  char    str_value[16];
  char    str_line[96];

  // if nothing to publish, ignore
  if ((teleinfo_conso.total_wh == 0) && (teleinfo_prod.total_wh == 0)) return;

  // wifi RSSI
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%d\n"), PSTR ("wifi"), TasmotaGlobal.hostname, PSTR ("rssi"), WiFi.RSSI ());
  TasmotaGlobal.mqtt_data += str_line;

  // contract mode
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u\n"), PSTR ("contract"), TasmotaGlobal.hostname, PSTR ("mode"), teleinfo_contract.mode);
  TasmotaGlobal.mqtt_data += str_line;

  // number of periods
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u\n"), PSTR ("contract"), TasmotaGlobal.hostname, PSTR ("periods"), teleinfo_contract.period_qty);
  TasmotaGlobal.mqtt_data += str_line;

  // current period
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u\n"), PSTR ("contract"), TasmotaGlobal.hostname, PSTR ("period"), teleinfo_contract.period);
  TasmotaGlobal.mqtt_data += str_line;

  // color
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u\n"), PSTR ("contract"), TasmotaGlobal.hostname, PSTR ("color"), TeleinfoPeriodGetLevel ());
  TasmotaGlobal.mqtt_data += str_line;

  // hc/hp
  snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%u\n"), PSTR ("contract"), TasmotaGlobal.hostname, PSTR ("hp"), TeleinfoPeriodGetHP ());
  TasmotaGlobal.mqtt_data += str_line;

  // if needed, add conso data
  if (teleinfo_conso.total_wh > 0)
  {
    // conso global total
    lltoa (teleinfo_conso.total_wh, str_value, 10);
    snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%s\n"), PSTR ("conso"), TasmotaGlobal.hostname, PSTR ("total"), str_value);
    TasmotaGlobal.mqtt_data += str_line;
    
    // conso indexes total
    for (index = 0; index < teleinfo_contract.period_qty; index++)
    {
      lltoa (teleinfo_conso.index_wh[index], str_value, 10);
      snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s%u value=%s\n"), PSTR ("conso"), TasmotaGlobal.hostname, PSTR ("index"), index + 1, str_value);
      TasmotaGlobal.mqtt_data += str_line;
    }

    // loop thru phases
    for (phase = 0; phase < teleinfo_contract.phase; phase++)
    {
      // set phase index
      if ((phase == 0) && (teleinfo_contract.phase == 1)) str_value[0] = 0;
        else sprintf_P (str_value, PSTR ("%u"), phase + 1);

      // current
      snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s%s value=%d.%03d\n"), PSTR ("conso"), TasmotaGlobal.hostname, PSTR ("I"), str_value, teleinfo_conso.phase[phase].current / 1000, teleinfo_conso.phase[phase].current % 1000);
      TasmotaGlobal.mqtt_data += str_line;

      // voltage
      snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s%s value=%d\n"), PSTR ("conso"), TasmotaGlobal.hostname, PSTR ("U"), str_value, teleinfo_conso.phase[phase].voltage);
      TasmotaGlobal.mqtt_data += str_line;

      // apparent power
      snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s%s value=%d\n"), PSTR ("conso"), TasmotaGlobal.hostname, PSTR ("VA"), str_value, teleinfo_conso.phase[phase].papp);
      TasmotaGlobal.mqtt_data += str_line;

      // active power
      snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s%s value=%d\n"), PSTR ("conso"), TasmotaGlobal.hostname, PSTR ("W"), str_value, teleinfo_conso.phase[phase].pact);
      TasmotaGlobal.mqtt_data += str_line;
    }

    // cos phi
    snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%d.%02d\n"), PSTR ("conso"), TasmotaGlobal.hostname, PSTR ("cosphi"), teleinfo_conso.cosphi.value / 1000, teleinfo_conso.cosphi.value % 1000 / 10);
    TasmotaGlobal.mqtt_data += str_line;
  }

  // if needed, add prod data
  if (teleinfo_prod.total_wh > 0)
  {
    // prod global total
    lltoa (teleinfo_prod.total_wh, str_value, 10);
    snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%s\n"), PSTR ("prod"), TasmotaGlobal.hostname, PSTR ("total"), str_value);
    TasmotaGlobal.mqtt_data += str_line;

    // apparent power
    snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%d\n"), PSTR ("prod"), TasmotaGlobal.hostname, PSTR ("VA"), teleinfo_prod.papp);
    TasmotaGlobal.mqtt_data += str_line;

    // active power
    snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%d\n"), PSTR ("prod"), TasmotaGlobal.hostname, PSTR ("W"),  teleinfo_prod.pact);
    TasmotaGlobal.mqtt_data += str_line;

    // cos phi
    snprintf_P (str_line, sizeof (str_line), PSTR("%s,device=%s,sensor=%s value=%d.%02d\n"), PSTR ("prod"), TasmotaGlobal.hostname, PSTR ("cosphi"), teleinfo_prod.cosphi.value / 1000, teleinfo_prod.cosphi.value % 1000 / 10);
    TasmotaGlobal.mqtt_data += str_line;
  }
}

void TeleinfoInfluxDbPublishData ()
{
  // if nothing to publish, ignore
  if (TasmotaGlobal.global_state.network_down) return;
  if (!teleinfo_influxdb.enabled) return;
  if (!teleinfo_influxdb.ready) return;

  // if needed, init connexion
  if (!teleinfo_influxdb.init)
  {
    InfluxDbLoop ();
    teleinfo_influxdb.init = 1;
  }

  // collect data from drivers and sensors
  TasmotaGlobal.mqtt_data = "";
  TeleinfoDriverApiInfluxDb ();
#ifdef USE_WINKY
  TeleinfoWinkyApiInfluxDb ();
#endif  // USE_WINKY

  // if needed, publish data
  if (TasmotaGlobal.mqtt_data.length () > 0)
  {
    InfluxDbPostData (TasmotaGlobal.mqtt_data.c_str ());
    AddLog (LOG_LEVEL_INFO,  PSTR("IDB: Donnees emises"));
    TasmotaGlobal.mqtt_data = "";
  }

  // reset publication flag
  teleinfo_influxdb.publish = 0;
}

/*******************************\
 *          Command
\*******************************/

// Enable influxDB Teleinfo extension
void CmndTeleinfoInfluxDbEnable ()
{
  if (XdrvMailbox.data_len > 0) TeleinfoInfluxDbSet (XdrvMailbox.payload != 0);
  ResponseCmndNumber (teleinfo_influxdb.enabled);
}


// Set InfluxDB version
void CmndTeleinfoInfluxDbVersion ()
{
  if (XdrvMailbox.data_len > 0) Settings->influxdb_version = (uint8_t)XdrvMailbox.payload;
  ResponseCmndNumber (Settings->influxdb_version);
}

/****************************\
 *        Callback
\****************************/

// Domoticz init message
void TeleinfoInfluxDbInit ()
{
  // load config 
  TeleinfoInfluxDbLoadConfig ();

  // check if all influxdb data are defined
  teleinfo_influxdb.ready = 1;
  if (strlen (SettingsText(SET_INFLUXDB_HOST))   == 0) teleinfo_influxdb.ready = 0;
  if (strlen (SettingsText(SET_INFLUXDB_ORG))    == 0) teleinfo_influxdb.ready = 0;
  if (strlen (SettingsText(SET_INFLUXDB_BUCKET)) == 0) teleinfo_influxdb.ready = 0;
  if (strlen (SettingsText(SET_INFLUXDB_TOKEN))  == 0) teleinfo_influxdb.ready = 0;
  if (Settings->influxdb_port == 0)                    teleinfo_influxdb.ready = 0;
}

// called every second
void TeleinfoInfluxDbEverySecond ()
{
  // if nothing to publish, ignore
  if (!teleinfo_influxdb.publish) return;

  // publish data
  TeleinfoInfluxDbPublishData ();
}

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

void TeleinfoInfluxDbDisplayParameters ()
{
  // section start
  WSContentSend_P (PSTR ("<fieldset class='config'>\n"));

  // parameters
  WSContentSend_P (PSTR ("<p class='dat'><span class='hval'>%s</span><span class='val'><input type='number' name='idb_%s' min=1 max=2 step=1 value=%u></span></p>\n"),     PSTR ("Version"),     PSTR ("ver"),  Settings->influxdb_version);
  WSContentSend_P (PSTR ("<p class='dat'><span class='hval'>%s</span><span class='val'><input type='number' name='idb_%s' min=1 max=65535 step=1 value=%u></span></p>\n"), PSTR ("Port"),        PSTR ("port"), Settings->influxdb_port);
  WSContentSend_P (PSTR ("<p class='dat'><span class='htxt'>%s</span><span class='sel'><input type='text' name='idb_%s' value='%s'></span></p>\n"),                        PSTR ("Host"),        PSTR ("host"), SettingsText (SET_INFLUXDB_HOST));
  WSContentSend_P (PSTR ("<p class='dat'><span class='htxt'>%s</span><span class='sel'><input type='text' name='idb_%s' value='%s'></span></p>\n"),                        PSTR ("User/Orga"),   PSTR ("orga"), SettingsText (SET_INFLUXDB_ORG));
  WSContentSend_P (PSTR ("<p class='dat'><span class='htxt'>%s</span><span class='sel'><input type='text' name='idb_%s' value='%s'></span></p>\n"),                        PSTR ("DB/Bucket"),   PSTR ("bukt"), SettingsText (SET_INFLUXDB_BUCKET));
  WSContentSend_P (PSTR ("<p class='dat'><span class='htxt'>%s</span><span class='sel'><input type='text' name='idb_%s' value='%s'></span></p>\n"),                        PSTR ("Passw/Token"), PSTR ("tokn"), SettingsText (SET_INFLUXDB_TOKEN));

  // section end
  WSContentSend_P (PSTR ("</fieldset>\n"));
}

void TeleinfoInfluxDbRetrieveParameters ()
{
  char str_value[128];

  // retrieve parameters
  if (Webserver->hasArg (F ("idb_host"))) { WebGetArg (PSTR ("idb_host"), str_value, sizeof (str_value)); SettingsUpdateText (SET_INFLUXDB_HOST,   str_value); }
  if (Webserver->hasArg (F ("idb_orga"))) { WebGetArg (PSTR ("idb_orga"), str_value, sizeof (str_value)); SettingsUpdateText (SET_INFLUXDB_ORG,    str_value); }
  if (Webserver->hasArg (F ("idb_bukt"))) { WebGetArg (PSTR ("idb_bukt"), str_value, sizeof (str_value)); SettingsUpdateText (SET_INFLUXDB_BUCKET, str_value); }
  if (Webserver->hasArg (F ("idb_tokn"))) { WebGetArg (PSTR ("idb_tokn"), str_value, sizeof (str_value)); SettingsUpdateText (SET_INFLUXDB_TOKEN,  str_value); }
  if (Webserver->hasArg (F ("idb_port"))) { WebGetArg (PSTR ("idb_port"), str_value, sizeof (str_value)); Settings->influxdb_port    = (uint16_t)atoi (str_value); }
  if (Webserver->hasArg (F ("idb_ver")))  { WebGetArg (PSTR ("idb_ver"),  str_value, sizeof (str_value)); Settings->influxdb_version = (uint8_t)atoi  (str_value); }
}

#endif    // USE_WEBSERVER

/***************************************\
 *              Interface
\***************************************/

bool XdrvTeleinfoInfluxDB (const uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
    case FUNC_COMMAND:
      result = DecodeCommand (kTeleinfoInfluxDbCommands, TeleinfoInfluxDbCommand);
      break;

    case FUNC_INIT:
      TeleinfoInfluxDbInit ();
      break;

    case FUNC_EVERY_SECOND:
      TeleinfoInfluxDbEverySecond ();
      break;
  }

  return result;
}

#endif      // USE_TELEINFO
#endif      // ESP32
