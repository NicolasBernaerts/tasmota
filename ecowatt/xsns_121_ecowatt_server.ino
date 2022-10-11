/*
  xsns_121_ecowatt_server.ino - Ecowatt MQTT local server
  
  This module connects to french RTE EcoWatt server to retrieve electricity production forecast.
  It publishes status of current slot and next 2 slots on the MQTT stream under
  
    {"Time":"2022-10-10T23:51:09","Ecowatt":{"now":1,"day0":{"0":1,"1":1,"2":1,"3":1,"4":1,"5":1,"6":1,...,"23":1},"day1":{"0":1,"1":1,"...
  
  Copyright (C) 2022  Nicolas Bernaerts
    06/10/2022 - v1.0 - Creation 

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

/****************************************************\
 *        EcoWatt electricity production forecast
\****************************************************/

#ifndef FIRMWARE_SAFEBOOT
#ifdef ESP32
#ifdef USE_UFILESYS
#ifdef USE_ECOWATT_SERVER

#define XSNS_121                   121

#include <ArduinoJson.h>

// constant
#define ECOWATT_STARTUP_DELAY      10         // ecowatt reading starts 10 seconds after startup
#define ECOWATT_UPDATE_SIGNAL      3600       // update ecowatt signals every hour
#define ECOWATT_UPDATE_RETRY       900        // retry after 15mn in case of error during signals reading
#define ECOWATT_UPDATE_JSON        600        // publish JSON every 10 mn
#define ECOWATT_SLOT_PER_DAY       24         // maximum number of 1h slots per day
#define ECOWATT_HTTPS_TIMEOUT      2000       // reception is considered over after 2sec without anything received

// configuration file
#define D_ECOWATT_CFG              "/ecowatt.cfg"
#define D_ECOWATT_PEM              "/ecowatt.pem"

// commands
#define D_CMND_ECOWATT_KEY         "key"
#define D_CMND_ECOWATT_UPDATE      "update"
#define D_CMND_ECOWATT_PUBLISH     "publish"

// URL
#define ECOWATT_URL_OAUTH2 "https://digital.iservices.rte-france.com/token/oauth/"
#define ECOWATT_URL_DATA "https://digital.iservices.rte-france.com/open_api/ecowatt/v4/signals"
//#define ECOWATT_URL_DATA "https://digital.iservices.rte-france.com/open_api/ecowatt/v4/sandbox/signals"

// MQTT commands
const char kEcowattCommands[] PROGMEM = "eco_" "|" "help" "|" D_CMND_ECOWATT_KEY  "|" D_CMND_ECOWATT_UPDATE "|" D_CMND_ECOWATT_PUBLISH;
void (* const EcowattCommand[])(void) PROGMEM = { &CmndEcowattHelp, &CmndEcowattKey, &CmndEcowattUpdate, &CmndEcowattPublish };

// https stream status
enum EcowattHttpsStream { ECOWATT_HTTPS_NONE, ECOWATT_HTTPS_START_TOKEN, ECOWATT_HTTPS_RECEIVE_TOKEN, ECOWATT_HTTPS_END_TOKEN, ECOWATT_HTTPS_START_DATA, ECOWATT_HTTPS_RECEIVE_DATA, ECOWATT_HTTPS_END_DATA };

// ecowatt type of days
enum EcowattDays { ECOWATT_DAY_TODAY, ECOWATT_DAY_TOMORROW, ECOWATT_DAY_DAYAFTER, ECOWATT_DAY_DAYAFTER2, ECOWATT_DAY_MAX };

// Ecowatt states
enum EcowattStates { ECOWATT_STATE_NONE, ECOWATT_STATE_NORMAL, ECOWATT_STATE_WARNING, ECOWATT_STATE_POWERCUT, ECOWATT_STATE_MAX };
const char kEcowattStateName[] PROGMEM = "|Normal|Risque|Coupure";
const char kEcowattStateColor[] PROGMEM = "|#02f0c6|#f2790f|#e63946";

/***********************************************************\
 *                        Data
\***********************************************************/

// HTTPS management
static struct {
  uint8_t     step       = ECOWATT_HTTPS_NONE;      // current reception step
  uint32_t    timeout    = UINT32_MAX;              // timeout of current reception
  uint32_t    time_token = UINT32_MAX;              // timestamp of token end of validity
  String      str_token_type;                       // token type
  String      str_token_value;                      // token value
  String      str_recv;                             // string being received
  HTTPClient  client;                               // https client
  WiFiClient *stream = nullptr;                     // https stream manager
} ecowatt_https;

// Ecowatt configuration
static struct {
  String  str_private_key;                          // base 64 private key (provided on RTE site)
  String  str_root_ca;                              // root certification authority key
} ecowatt_config;

// Ecowatt status
struct ecowatt_day {
  uint8_t status;                                   // day global status
  char str_date[12];                                // slot date (aaaa-mm-dd)
  uint8_t arr_slot[ECOWATT_SLOT_PER_DAY];           // hourly slots
};
static struct {
  uint8_t  hour = UINT8_MAX;                        // slots for today and tomorrow
  uint32_t time_update = UINT32_MAX;                // timestamp of next update
  uint32_t time_json   = UINT32_MAX;                // timestamp of next JSON update
  ecowatt_day arr_day[ECOWATT_DAY_MAX];             // slots for today and tomorrow
} ecowatt_status;

/***********************************************************\
 *                      Commands
\***********************************************************/

// Ecowatt server help
void CmndEcowattHelp ()
{
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Ecowatt local server commands :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - eco_key     = set RTE base64 private key"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - eco_update  = force ecowatt data update from RTE server"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - eco_publish = ask to publish data"));
  AddLog (LOG_LEVEL_INFO, PSTR (" Slot values can be :"));
  AddLog (LOG_LEVEL_INFO, PSTR ("  1 = Situation normale"));
  AddLog (LOG_LEVEL_INFO, PSTR ("  2 = Risque de coupures"));
  AddLog (LOG_LEVEL_INFO, PSTR ("  3 = Coupures programmées"));
  ResponseCmndDone ();
}

// Ecowatt RTE base64 private key
void CmndEcowattKey ()
{
  char str_key[128];

  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.data_len < 128))
  {
    strncpy (str_key, XdrvMailbox.data, XdrvMailbox.data_len);
    str_key[XdrvMailbox.data_len] = 0;
    ecowatt_config.str_private_key = str_key;
    AddLog (LOG_LEVEL_INFO, PSTR ("ECO: Private key set to %s"), str_key);
    EcowattSaveConfig ();
  }
  
  ResponseCmndChar (ecowatt_config.str_private_key.c_str ());
}

// Ecowatt forced update from RTE server
void CmndEcowattUpdate ()
{
  ecowatt_status.time_update = LocalTime ();
  ResponseCmndDone ();
}

// Ecowatt publish data
void CmndEcowattPublish ()
{
  ecowatt_status.time_json = LocalTime ();
  ResponseCmndDone ();
}

/***********************************************************\
 *                      Configuration
\***********************************************************/

// load configuration
void EcowattLoadConfig () 
{
  File file;

  // if file exists, load root CA
  if (ffsp->exists (D_ECOWATT_PEM))
  {
    // open file in read only mode in littlefs filesystem
    file = ffsp->open (D_ECOWATT_PEM, "r");
    ecowatt_config.str_root_ca = file.readString ();

    // close file
    file.close (); 

    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("ECO: Root CA loaded"));
    AddLog (LOG_LEVEL_DEBUG, PSTR ("%s"), ecowatt_config.str_root_ca.c_str ());
  }

  // else, root CA file missing
  else AddLog (LOG_LEVEL_INFO, PSTR ("ECO: Root CA missing"));

  // retrieve saved settings from flash filesystem
  ecowatt_config.str_private_key = UfsCfgLoadKey (D_ECOWATT_CFG, D_CMND_ECOWATT_KEY);
}

// save configuration
void EcowattSaveConfig () 
{
  // save settings into flash filesystem
  UfsCfgSaveKey (D_ECOWATT_CFG, D_CMND_ECOWATT_KEY, ecowatt_config.str_private_key.c_str (), true);
}

/***********************************************************\
 *                      Functions
\***********************************************************/

// handle beginning of token generation
bool EcowattHttpsStartToken ()
{
  bool     is_ok;
  int      http_code;
  uint32_t token_validity;
  
  // init token data
  ecowatt_https.str_token_value  = "";
  ecowatt_https.str_token_type   = "";
  ecowatt_https.time_token       = UINT32_MAX;

  // check for private key
  is_ok = (ecowatt_config.str_private_key.length () > 0);
  if (!is_ok) AddLog (LOG_LEVEL_INFO, PSTR ("ECO: Private RTE Base64 key missing. Please load it thru eco_key command."));

  // if private key is defined, start token authentification
  else
  {
    // set headers
    ecowatt_https.client.useHTTP10 (true);
    ecowatt_https.client.setAuthorizationType ("Basic");
    ecowatt_https.client.setAuthorization (ecowatt_config.str_private_key.c_str ());
    ecowatt_https.client.addHeader ("Content-Type", "application/x-www-form-urlencoded", false, true);

    // connexion and request
    ecowatt_https.client.begin (ECOWATT_URL_OAUTH2, ecowatt_config.str_root_ca.c_str ());
    http_code = ecowatt_https.client.GET ();
    is_ok = ((http_code > 0) && (http_code < 400));

    // if error, free client ressources
    if (!is_ok) ecowatt_https.client.end ();

    // log connexion result
    if (is_ok) AddLog (LOG_LEVEL_INFO, PSTR ("ECO: Token - Authentification SUCCESS (%d)"), http_code);
      else AddLog (LOG_LEVEL_INFO, PSTR ("ECO: Token - Authentification ERROR (%d)"), http_code);
  }

  return is_ok;
}

// handle end of token generation
bool EcowattHttpsEndToken ()
{
  uint32_t token_validity;
  DynamicJsonDocument json_result(512);
  
  // log reception
  AddLog (LOG_LEVEL_INFO, PSTR ("ECO: Token - Received %d bytes"), ecowatt_https.str_recv.length ());
  AddLog (LOG_LEVEL_DEBUG, PSTR ("%s"), ecowatt_https.str_recv.c_str ());

  // extract token from JSON
  deserializeJson (json_result, ecowatt_https.str_recv);
  ecowatt_https.str_token_value = json_result["access_token"].as<const char*> ();

  // calculate token end of validity
  if (ecowatt_https.str_token_value.length () > 0)
  {
    // token type
    ecowatt_https.str_token_type  = json_result["token_type"].as<const char*> ();

    // token validity
    token_validity = json_result["expires_in"].as<unsigned int> ();
    ecowatt_https.time_token = LocalTime () + token_validity - 60;

    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("ECO: Token - Value %s is valid for %u seconds"), ecowatt_https.str_token_value.c_str (), token_validity);
  }

  // reset reception string
  ecowatt_https.str_recv = "";

  return (ecowatt_https.time_token != UINT32_MAX);
}

// handle beginning of signals page reception
bool EcowattHttpsStartSignals ()
{
  bool is_ok;
  int  http_code;
  
  // init signals data
  ecowatt_status.time_update = LocalTime ();

  // set headers
  ecowatt_https.client.useHTTP10 (true);
  ecowatt_https.client.setAuthorizationType (ecowatt_https.str_token_type.c_str ());
  ecowatt_https.client.setAuthorization (ecowatt_https.str_token_value.c_str ());
  ecowatt_https.client.addHeader ("Content-Type", "application/x-www-form-urlencoded", false, true);

  // connexion and request
  ecowatt_https.client.begin (ECOWATT_URL_DATA, ecowatt_config.str_root_ca.c_str ());
  http_code = ecowatt_https.client.GET ();
  is_ok = ((http_code > 0) && (http_code < 400));

  // if error, free client ressources
  if (!is_ok) ecowatt_https.client.end ();

  // log connexion result
  if (is_ok) AddLog (LOG_LEVEL_INFO, PSTR ("ECO: Signals - Authentification SUCCESS (%d)"), http_code);
  else AddLog (LOG_LEVEL_INFO, PSTR ("ECO: Signals - Authentification ERROR (%d)"), http_code);

  return is_ok;
}

// handle end of signals page reception
bool EcowattHttpsEndSignals ()
{
  uint8_t  index, slot, value;
  uint32_t time_now;
  TIME_T   time_dst;
  String   str_day;
  DynamicJsonDocument json_result(8192);

  // log reception
  AddLog (LOG_LEVEL_INFO, PSTR ("ECO: Signals - Received %d bytes"), ecowatt_https.str_recv.length ());
  AddLog (LOG_LEVEL_DEBUG, PSTR ("%s"), ecowatt_https.str_recv.c_str ());

  // extract token from JSON
  deserializeJson (json_result, ecowatt_https.str_recv);

  // loop thru all 4 days to get today's and tomorrow's index
  for (index = 0; index < ECOWATT_DAY_MAX; index ++)
  {
    // get day for current signal index
    str_day = json_result["signals"][index]["jour"].as<const char*> ();
    str_day = str_day.substring (0, 10);

    // get global status and date
    ecowatt_status.arr_day[index].status = json_result["signals"][index]["dvalue"];
    strncpy (ecowatt_status.arr_day[index].str_date, json_result["signals"][index]["jour"], 10);
    ecowatt_status.arr_day[index].str_date[10] = 0;

    // loop to populate the slots
    for (slot = 0; slot < ECOWATT_SLOT_PER_DAY; slot ++)
    {
      value = json_result["signals"][index]["values"][slot]["hvalue"];
      if ((value == ECOWATT_STATE_NONE) || (value >= ECOWATT_STATE_MAX)) value = ECOWATT_STATE_NORMAL;
      ecowatt_status.arr_day[index].arr_slot[slot] = value;
    }

    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("ECO: Signals - Slot %u is %s, status is %u"), index, str_day.c_str (), ecowatt_status.arr_day[index].status);
  }

  // set next update time
  ecowatt_status.time_update = time_now + ECOWATT_UPDATE_SIGNAL;

  // reset reception string
  ecowatt_https.str_recv = "";

  return true;
}

// handle data reception for current HTTPs page
bool EcowattHttpsReceiveData ()
{
  bool     is_over;
  int      length;
  char     str_content[256];
  String   str_text;

  // if needed, init data
  if (ecowatt_https.stream == nullptr)
  {
    ecowatt_https.stream   = ecowatt_https.client.getStreamPtr ();
    ecowatt_https.str_recv = "";
    ecowatt_https.timeout  = millis () + ECOWATT_HTTPS_TIMEOUT; 
  }

  // if stream pointer is ok
  if (ecowatt_https.stream != nullptr)
  {
    // read available data
    length = ecowatt_https.stream->read ((uint8_t*)str_content, 255);
    if (length > 0)
    {
      // set end of string
      str_content[255] = 0;
      if (length < 255) str_content[length] = 0;
      str_text = str_content;

      // if receiving data, cleanup string
      str_text.replace ("\n","");
      str_text.replace ("\r","");
      if (ecowatt_https.step == ECOWATT_HTTPS_RECEIVE_DATA) str_text.replace (" ", "");

      // append result
      ecowatt_https.str_recv += str_text;

      // reset timeout
      ecowatt_https.timeout = millis () + ECOWATT_HTTPS_TIMEOUT;
    }
  }

  // check if reception is over
  is_over = TimeReached (ecowatt_https.timeout);
  if (is_over)
  {
    // reset data
    ecowatt_https.stream  = nullptr;
    ecowatt_https.timeout = UINT32_MAX; 

    // free client ressources
    ecowatt_https.client.end ();
  }

  return is_over;
}

// handle all steps to go thru when updating echowatt signals with RTE API
void EcowattHandleUpdateProcess ()
{
  bool     result;
  uint32_t time_now = LocalTime ();

  // switch according to reception step
  switch (ecowatt_https.step)
  {
    // ask for token reception
    case ECOWATT_HTTPS_START_TOKEN:
      // check if token is currently valid
      if ((ecowatt_https.time_token != UINT32_MAX) && (ecowatt_https.time_token > time_now)) ecowatt_https.step = ECOWATT_HTTPS_START_DATA;

      // else start token reception
      else
      {
        // if token reception started
        if (EcowattHttpsStartToken ()) ecowatt_https.step = ECOWATT_HTTPS_RECEIVE_TOKEN;

        // else retry in 5mn
        else
        {
          ecowatt_https.step = ECOWATT_HTTPS_NONE;
          ecowatt_status.time_update = time_now + ECOWATT_UPDATE_RETRY;
        }
      }
      break;

    // token reception
    case ECOWATT_HTTPS_RECEIVE_TOKEN:
      // if token reception is over
      if (EcowattHttpsReceiveData ()) ecowatt_https.step = ECOWATT_HTTPS_END_TOKEN;
      break;

    // handle received token
    case ECOWATT_HTTPS_END_TOKEN:
      // if token is ok
      if (EcowattHttpsEndToken ()) ecowatt_https.step = ECOWATT_HTTPS_START_DATA;

      // else retry in 5mn
      else
      {
        ecowatt_https.step = ECOWATT_HTTPS_NONE;
        ecowatt_status.time_update = time_now + ECOWATT_UPDATE_RETRY;
      }
      break;

    case ECOWATT_HTTPS_START_DATA:
      // if token reception started
      if (EcowattHttpsStartSignals ()) ecowatt_https.step = ECOWATT_HTTPS_RECEIVE_DATA;

      // else retry in 15mn
      else
      {
        ecowatt_https.step = ECOWATT_HTTPS_NONE;
        ecowatt_status.time_update = time_now + ECOWATT_UPDATE_RETRY;
      }
      break;

    case ECOWATT_HTTPS_RECEIVE_DATA:
      // if data reception is over
      if (EcowattHttpsReceiveData ()) ecowatt_https.step = ECOWATT_HTTPS_END_DATA;
      break;

    case ECOWATT_HTTPS_END_DATA:
      // if data reception is ok
      if (EcowattHttpsEndSignals ())
      {
        // end of update process
        ecowatt_https.step = ECOWATT_HTTPS_NONE;

        // ask for JSON update
        ecowatt_status.time_json = time_now;
      }

      // else retry in 5mn
      else
      {
        ecowatt_https.step = ECOWATT_HTTPS_NONE;
        ecowatt_status.time_update = time_now + ECOWATT_UPDATE_RETRY;
      }
      break;
  }
}

// publish Ecowatt JSON thru MQTT
// format is  {"Time":"xxxxxxxx","Ecowatt":{"slot":16,"now":2,"next":1,"day0":{"0":2,"1":1,...,"23":1},"day1":{"0":1,"1":1,...,"23":2},...}}
void EcowattPublishJson ()
{
  uint8_t  index, slot;
  uint8_t  slot_day, slot_hour;
  uint32_t time_now;
  TIME_T   dst_now;
  char  str_text[16];

  // current time
  time_now = LocalTime ();
  BreakTime (time_now, dst_now);
  slot_day  = ECOWATT_DAY_TODAY;
  slot_hour = dst_now.hour;

  // Start Ecowatt section
  Response_P (PSTR ("{\"Time\":\"%s\",\"Ecowatt\":{"), GetDateAndTime (DT_LOCAL).c_str ());

  // publish slot index
  ResponseAppend_P (PSTR ("\"slot\":%u"), slot_hour);

  // publish current slot
  ResponseAppend_P (PSTR (",\"now\":%u"), ecowatt_status.arr_day[slot_day].arr_slot[slot_hour]);

  // publish next slot
  if (slot_hour == 23) { slot_hour = 0; slot_day++; }
    else slot_hour++;
  ResponseAppend_P (PSTR (",\"next\":%u"), ecowatt_status.arr_day[slot_day].arr_slot[slot_hour]);

  // loop thru number of slots to publish
  for (index = 0; index < ECOWATT_DAY_MAX; index ++)
  {
    ResponseAppend_P (PSTR (",\"day%u\":{"), index);

    for (slot = 0; slot < ECOWATT_SLOT_PER_DAY; slot ++)
    {
      if (slot > 0) ResponseAppend_P (PSTR (","));
      ResponseAppend_P (PSTR ("\"%u\":%u"), slot, ecowatt_status.arr_day[index].arr_slot[slot]);
    }

    ResponseAppend_P (PSTR ("}"));
  }

  // end of Ecowatt section and publish JSON
  ResponseAppend_P (PSTR ("}}"));
  MqttPublishTeleSensor ();

  // plan next update
  ecowatt_status.time_json = time_now + ECOWATT_UPDATE_JSON;
}

/***********************************************************\
 *                      Callback
\***********************************************************/

// init main status
void EcowattInit ()
{
  uint8_t index, slot;

  // load configuration file
  EcowattLoadConfig ();

  // initialisation of slot array
  for (index = 0; index < ECOWATT_DAY_MAX; index ++)
    for (slot = 0; slot < ECOWATT_SLOT_PER_DAY; slot ++)
      ecowatt_status.arr_day[index].arr_slot[slot] = ECOWATT_STATE_NORMAL;

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: eco_help to get help on Ecowatt production forecast commands"));
}

// called every second
void EcowattEverySecond ()
{
  bool     update_json = false;
  uint32_t time_now;
  TIME_T   dst_now;

  time_now = LocalTime ();
  BreakTime (time_now, dst_now);

  // check for JSON update
  if (ecowatt_status.time_json == UINT32_MAX) update_json = true;
  else if (ecowatt_status.time_json < time_now) update_json = true;

  // check for change of hour
  if (dst_now.hour != ecowatt_status.hour) update_json = true;
  ecowatt_status.hour = dst_now.hour;

  // update ecowatt signal if needed
  if ((ecowatt_status.time_update == UINT32_MAX) || (ecowatt_status.time_update < time_now))
  {
    // if reception is not actually running, start it
    if (ecowatt_https.step == ECOWATT_HTTPS_NONE) ecowatt_https.step = ECOWATT_HTTPS_START_TOKEN;
  }

  // if needed, publish JSON
  if (update_json) EcowattPublishJson ();
}

// called every day at midnight
void EcowattMidnigth ()
{
  uint8_t index, slot;

  // shift of one day
  for (index = 0; index < ECOWATT_DAY_MAX; index ++)
    for (slot = 0; slot < ECOWATT_SLOT_PER_DAY; slot ++)
    {
      if (index == ECOWATT_DAY_DAYAFTER2) ecowatt_status.arr_day[index].arr_slot[slot] = ECOWATT_STATE_NORMAL;
      else ecowatt_status.arr_day[index].arr_slot[slot] = ecowatt_status.arr_day[index + 1].arr_slot[slot];
    }
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// append presence forecast to main page
void EcowattWebSensor ()
{
  uint8_t  index, slot;
  uint32_t time_now, time_next;
  char     str_color[12];

  // display next update
  time_now = LocalTime ();
  if (ecowatt_status.time_update > time_now) time_next = ecowatt_status.time_update - time_now; else time_next = 0;
  if (ecowatt_status.time_update != UINT32_MAX)
    if (time_next == 0) WSContentSend_P (PSTR ("{s}Ecowatt{m}Updating now{e}"), time_next / 60); else WSContentSend_P (PSTR ("{s}Ecowatt{m}Update in %u mn{e}"), time_next / 60);

  // start of graph
  WSContentSend_P (PSTR ("<tr>\n"));

  // graph header
  WSContentSend_P (PSTR ("<div style='display:flex;margin:0px;padding:0px;font-size:12px;'>\n"));
  WSContentSend_P (PSTR ("<div style='width:50%%;text-align:left;'>%uh</div>\n"), 0);
  WSContentSend_P (PSTR ("<div style='width:100%%;text-align:center;'>%uh</div>\n"), 4);
  WSContentSend_P (PSTR ("<div style='width:100%%;text-align:center;'>%uh</div>\n"), 8);
  WSContentSend_P (PSTR ("<div style='width:100%%;text-align:center;'>%uh</div>\n"), 12);
  WSContentSend_P (PSTR ("<div style='width:100%%;text-align:center;'>%uh</div>\n"), 16);
  WSContentSend_P (PSTR ("<div style='width:100%%;text-align:center;'>%uh</div>\n"), 20);
  WSContentSend_P (PSTR ("<div style='width:50%%;text-align:right;'>%uh</div>\n"), 24);
  WSContentSend_P (PSTR ("</div>\n"));

  // loop thru days
  for (index = 0; index < ECOWATT_DAY_MAX; index ++)
  {
    WSContentSend_P (PSTR ("<div style='display:flex;margin:0px;padding:2px 0px;height:16px;'>\n"));
    for (slot = 0; slot < ECOWATT_SLOT_PER_DAY; slot ++)
    {
      // get segment color
      GetTextIndexed (str_color, sizeof (str_color), ecowatt_status.arr_day[index].arr_slot[slot], kEcowattStateColor);
      
      // display segment
      if (slot == 0) WSContentSend_P (PSTR ("<div style='width:100%%;margin-right:1px;border-radius:2px;background-color:%s;border-top-left-radius:6px;border-bottom-left-radius:6px;'></div>\n"), str_color);
      else if (slot == ECOWATT_SLOT_PER_DAY - 1) WSContentSend_P (PSTR ("<div style='width:100%%;margin-right:1px;border-radius:2px;background-color:%s;border-top-right-radius:6px;border-bottom-right-radius:6px;'></div>\n"), str_color);
      else WSContentSend_P (PSTR ("<div style='width:100%%;margin-right:1px;border-radius:2px;background-color:%s;'></div>\n"), str_color);
    }
    WSContentSend_P (PSTR ("</div>\n"));
  }

  // end of graph
  WSContentSend_P (PSTR ("</tr>\n"));
}

#endif  // USE_WEBSERVER

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xsns121 (uint8_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
    case FUNC_INIT:
      EcowattInit ();
      break;
    case FUNC_SAVE_AT_MIDNIGHT:
      EcowattMidnigth ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kEcowattCommands, EcowattCommand);
      break;
    case FUNC_EVERY_SECOND:
      if (TasmotaGlobal.uptime > ECOWATT_STARTUP_DELAY) EcowattEverySecond ();
      break;
    case FUNC_EVERY_100_MSECOND:
      EcowattHandleUpdateProcess ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      EcowattWebSensor ();
      break;
#endif  // USE_WEBSERVER
  }
  
  return result;
}

#endif    // USE_ECOWATT_SERVER
#endif    // USE_UFILESYS
#endif    // ESP32
#endif    // FIRMWARE_SAFEBOOT
