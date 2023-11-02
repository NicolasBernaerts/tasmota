/*
  xsns_119_ecowatt_server.ino - Ecowatt MQTT local server
  
  Copyright (C) 2022  Nicolas Bernaerts
    06/10/2022 - v1.0 - Creation 
    21/10/2022 - v1.1 - Add sandbox management (with day shift to have green/orange/red on current day)
    09/02/2023 - v1.2 - Disable wifi sleep to avoid latency
    15/05/2023 - v1.3 - Rewrite CSV file access
    14/10/2023 - v1.4 - Rewrite API management and intergrate RTE root certificate
    28/10/2023 - v1.5 - Change in ecowatt stream reception to avoid overload

  This module connects to french RTE EcoWatt server to retrieve electricity production forecast.
  It publishes status of current slot and next 2 slots on the MQTT stream under
  
    {"Time":"2022-10-10T23:51:09","Ecowatt":{"dval":2,"hour":14,"now":1,"next":2,
      "day0":{"jour":"06-10-2022","dval":1,"0":1,"1":1,"2":1,"3":1,"4":1,"5":1,"6":1,...,"23":1},
      "day1":{"jour":"07-10-2022","dval":2,"0":1,"1":1,"2":2,"3":1,"4":1,"5":1,"6":1,...,"23":1},
      "day2":{"jour":"08-10-2022","dval":3,"0":1,"1":1,"2":1,"3":1,"4":1,"5":3,"6":1,...,"23":1},
      "day3":{"jour":"09-10-2022","dval":2,"0":1,"1":1,"2":1,"3":2,"4":1,"5":1,"6":1,...,"23":1}}}

  RTE root certification authority should be placed under /ecowatt.pem for the server to work

  See https://github.com/NicolasBernaerts/tasmota/tree/master/ecowatt for instructions

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

/**********************************************************\
 *       France EcoWatt electricity signal management
\**********************************************************/

#ifdef ESP32
#ifdef USE_UFILESYS
#ifdef USE_ECOWATT_SERVER

#define XSNS_119                    119

#include <ArduinoJson.h>

// constant
#define ECOWATT_STARTUP_DELAY       10         // ecowatt connexion delay
#define ECOWATT_UPDATE_JSON         900        // publish JSON every 15 mn
#define ECOWATT_SLOT_PER_DAY        24         // maximum number of 1h slots per day

#define ECOWATT_TOKEN_RETRY         300        // token retry timeout (sec.)
#define ECOWATT_SIGNAL_RETRY        900        // signal retry timeout (sec.)
#define ECOWATT_SIGNAL_RENEW        3595       // signal renew period (sec.)

// configuration file
#define D_ECOWATT_CFG               "/ecowatt.cfg"

// commands
#define D_CMND_ECOWATT_ENABLE       "enable"
#define D_CMND_ECOWATT_KEY          "key"
#define D_CMND_ECOWATT_SANDBOX      "sandbox"
#define D_CMND_ECOWATT_UPDATE       "update"
#define D_CMND_ECOWATT_PUBLISH      "publish"
#define D_CMND_ECOWATT_TOKEN        "token"

// URL
#define ECOWATT_URL_OAUTH2          "https://digital.iservices.rte-france.com/token/oauth/"
#define ECOWATT_URL_DATA            "https://digital.iservices.rte-france.com/open_api/ecowatt/v4/signals"
#define ECOWATT_URL_SANDBOX         "https://digital.iservices.rte-france.com/open_api/ecowatt/v4/sandbox/signals"

// MQTT commands
const char kEcowattCommands[] PROGMEM = "eco_" "|" "help" "|" D_CMND_ECOWATT_ENABLE "|" D_CMND_ECOWATT_KEY "|" D_CMND_ECOWATT_UPDATE "|" D_CMND_ECOWATT_PUBLISH "|" D_CMND_ECOWATT_SANDBOX "|" D_CMND_ECOWATT_TOKEN;
void (* const EcowattCommand[])(void) PROGMEM = { &CmndEcowattHelp, &CmndEcowattEnable, &CmndEcowattKey, &CmndEcowattUpdate, &CmndEcowattPublish, &CmndEcowattSandbox, &CmndEcowattToken };

// https stream status
enum EcowattHttpsUpdate { ECOWATT_UPDATE_NONE, ECOWATT_UPDATE_TOKEN_BEGIN, ECOWATT_UPDATE_TOKEN_GET, ECOWATT_UPDATE_TOKEN_RECEIVE, ECOWATT_UPDATE_TOKEN_END, ECOWATT_UPDATE_SIGNAL_BEGIN, ECOWATT_UPDATE_SIGNAL_GET, ECOWATT_UPDATE_SIGNAL_RECEIVE, ECOWATT_UPDATE_SIGNAL_END, ECOWATT_UPDATE_MAX};

// ecowatt type of days
enum EcowattDays { ECOWATT_DAY_TODAY, ECOWATT_DAY_TOMORROW, ECOWATT_DAY_DAYAFTER, ECOWATT_DAY_DAYAFTER2, ECOWATT_DAY_MAX };

// Ecowatt states
enum EcowattLevels { ECOWATT_LEVEL_NONE, ECOWATT_LEVEL_NORMAL, ECOWATT_LEVEL_WARNING, ECOWATT_LEVEL_POWERCUT, ECOWATT_LEVEL_MAX };
const char kEcowattLevelColor[] PROGMEM = "|#080|#F80|#A00";

// config
enum EcowattConfigKey { ECOWATT_CONFIG_ENABLE, ECOWATT_CONFIG_SANDBOX, ECOWATT_CONFIG_KEY, ECOWATT_CONFIG_MAX };         // configuration parameters
const char kEcowattConfigKey[] PROGMEM = D_CMND_ECOWATT_ENABLE "|" D_CMND_ECOWATT_SANDBOX "|" D_CMND_ECOWATT_KEY;        // configuration keys

/***********************************************************\
 *                        Data
\***********************************************************/

// RTE API root certificate
const char ecowatt_root_certificate[] PROGMEM = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDXzCCAkegAwIBAgILBAAAAAABIVhTCKIwDQYJKoZIhvcNAQELBQAwTDEgMB4G\n" \
"A1UECxMXR2xvYmFsU2lnbiBSb290IENBIC0gUjMxEzARBgNVBAoTCkdsb2JhbFNp\n" \
"Z24xEzARBgNVBAMTCkdsb2JhbFNpZ24wHhcNMDkwMzE4MTAwMDAwWhcNMjkwMzE4\n" \
"MTAwMDAwWjBMMSAwHgYDVQQLExdHbG9iYWxTaWduIFJvb3QgQ0EgLSBSMzETMBEG\n" \
"A1UEChMKR2xvYmFsU2lnbjETMBEGA1UEAxMKR2xvYmFsU2lnbjCCASIwDQYJKoZI\n" \
"hvcNAQEBBQADggEPADCCAQoCggEBAMwldpB5BngiFvXAg7aEyiie/QV2EcWtiHL8\n" \
"RgJDx7KKnQRfJMsuS+FggkbhUqsMgUdwbN1k0ev1LKMPgj0MK66X17YUhhB5uzsT\n" \
"gHeMCOFJ0mpiLx9e+pZo34knlTifBtc+ycsmWQ1z3rDI6SYOgxXG71uL0gRgykmm\n" \
"KPZpO/bLyCiR5Z2KYVc3rHQU3HTgOu5yLy6c+9C7v/U9AOEGM+iCK65TpjoWc4zd\n" \
"QQ4gOsC0p6Hpsk+QLjJg6VfLuQSSaGjlOCZgdbKfd/+RFO+uIEn8rUAVSNECMWEZ\n" \
"XriX7613t2Saer9fwRPvm2L7DWzgVGkWqQPabumDk3F2xmmFghcCAwEAAaNCMEAw\n" \
"DgYDVR0PAQH/BAQDAgEGMA8GA1UdEwEB/wQFMAMBAf8wHQYDVR0OBBYEFI/wS3+o\n" \
"LkUkrk1Q+mOai97i3Ru8MA0GCSqGSIb3DQEBCwUAA4IBAQBLQNvAUKr+yAzv95ZU\n" \
"RUm7lgAJQayzE4aGKAczymvmdLm6AC2upArT9fHxD4q/c2dKg8dEe3jgr25sbwMp\n" \
"jjM5RcOO5LlXbKr8EpbsU8Yt5CRsuZRj+9xTaGdWPoO4zzUhw8lo/s7awlOqzJCK\n" \
"6fBdRoyV3XpYKBovHd7NADdBj+1EbddTKJd+82cEHhXXipa0095MJ6RMG3NzdvQX\n" \
"mcIfeg7jLQitChws/zyrVQ4PkX4268NXSb7hLi18YIvDQVETI53O9zJrlAGomecs\n" \
"Mx86OyXShkDOOyyGeMlhLxS67ttVb9+E7gUJTb0o2HLO02JQZR7rkpeDMdmztcpH\n" \
"WD9f\n" \
"-----END CERTIFICATE-----\n";

// update management
static struct {
  uint8_t     step       = ECOWATT_UPDATE_NONE;     // current reception step
  uint32_t    timeout    = UINT32_MAX;              // timeout of current reception
  uint32_t    time_token = UINT32_MAX;              // timestamp of token update
  uint32_t    time_signal = UINT32_MAX;              // timestamp of signal update
  char        str_token[128];                       // token value
} ecowatt_update;

// Ecowatt configuration
static struct {
  bool  enabled = false;                            // flag to enable ecowatt server
  bool  sandbox = false;                            // flag to use RTE sandbox
  char  str_private_key[128];                       // base 64 private key (provided on RTE site)
} ecowatt_config;

// Ecowatt status
struct ecowatt_day {
  uint8_t dvalue;                                   // day global status
  char    str_day[12];                              // slot date (dd-mm-aaaa)
  uint8_t arr_hvalue[ECOWATT_SLOT_PER_DAY];         // hourly slots
};
static struct {
  uint32_t    time_json   = UINT32_MAX;             // timestamp of next JSON update
  ecowatt_day arr_day[ECOWATT_DAY_MAX];             // slots for today and next days
} ecowatt_status;

WiFiClientSecure ecowatt_wifi;                      // wifi secure client
HTTPClient       ecowatt_http;                      // https client

/***********************************************************\
 *                      Commands
\***********************************************************/

// Ecowatt server help
void CmndEcowattHelp ()
{
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Ecowatt server commands :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - eco_enable <0/1>  = enable/disable ecowatt server"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - eco_sandbox <0/1> = set sandbox mode (0/1)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - eco_key <key>     = set RTE base64 private key"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - eco_update        = force ecowatt update from RTE server"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - eco_publish       = publish ecowatt data now"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - eco_token         = display current token"));
  ResponseCmndDone ();
}

// Enable ecowatt server
void CmndEcowattEnable ()
{
  uint32_t time_now = LocalTime ();

  if (XdrvMailbox.data_len > 0)
  {
    // set status
    ecowatt_config.enabled = XdrvMailbox.payload;

    // if anabled
    if (ecowatt_config.enabled)
    {
      // force update
      ecowatt_update.time_token  = time_now;
      ecowatt_update.time_signal = time_now;

      // log
      AddLog (LOG_LEVEL_INFO, PSTR ("ECO: Ecowatt server enabled"));
    }

    else AddLog (LOG_LEVEL_INFO, PSTR ("ECO: Ecowatt server disabled"));

    // save
    EcowattSaveConfig ();
  }
  
  ResponseCmndNumber (ecowatt_config.enabled);
}

// Set sandbox mode
void CmndEcowattSandbox ()
{
  if (XdrvMailbox.data_len > 0)
  {
    // set sandbox mode
    ecowatt_config.sandbox = XdrvMailbox.payload;
    AddLog (LOG_LEVEL_INFO, PSTR ("ECO: Sandbox mode is %d"), ecowatt_config.sandbox);
    EcowattSaveConfig ();

    // force update
    ecowatt_update.time_token = ecowatt_update.time_signal = LocalTime ();
  }
  
  ResponseCmndNumber (ecowatt_config.sandbox);
}

// Ecowatt RTE base64 private key
void CmndEcowattKey ()
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.data_len < 128))
  {
    strlcpy (ecowatt_config.str_private_key, XdrvMailbox.data, sizeof (ecowatt_config.str_private_key));
    AddLog (LOG_LEVEL_INFO, PSTR ("ECO: Private key is %s"), ecowatt_config.str_private_key);
    EcowattSaveConfig ();
  }
  
  ResponseCmndChar (ecowatt_config.str_private_key);
}

// Ecowatt forced update from RTE server
void CmndEcowattUpdate ()
{
  ecowatt_update.time_token = ecowatt_update.time_signal = LocalTime ();
  ResponseCmndDone ();
}

// Ecowatt publish data
void CmndEcowattPublish ()
{
  ecowatt_status.time_json = LocalTime ();
  ResponseCmndDone ();
}

// Ecowatt token
void CmndEcowattToken ()
{
  ResponseCmndChar (ecowatt_update.str_token);
}

/***********************************************************\
 *                      Configuration
\***********************************************************/

// load configuration
void EcowattLoadConfig () 
{
  int    index;
  char   str_text[16];
  char   str_line[128];
  char   *pstr_key, *pstr_value;
  File   file;

  // if file exists, open and read each line
  if (ffsp->exists (D_ECOWATT_CFG))
  {
    file = ffsp->open (D_ECOWATT_CFG, "r");
    while (file.available ())
    {
      // read current line and extract key and value
      index = file.readBytesUntil ('\n', str_line, sizeof (str_line) - 1);
      if (index >= 0) str_line[index] = 0;
      pstr_key   = strtok (str_line, "=");
      pstr_value = strtok (nullptr,  "\n");

      // if key and value are defined, look for config keys
      if ((pstr_key != nullptr) && (pstr_value != nullptr))
      {
        index = GetCommandCode (str_text, sizeof (str_text), pstr_key, kEcowattConfigKey);
        switch (index)
        {
          case ECOWATT_CONFIG_KEY:
            strlcpy (ecowatt_config.str_private_key, pstr_value, sizeof (ecowatt_config.str_private_key));
            break;
         case ECOWATT_CONFIG_SANDBOX:
            ecowatt_config.sandbox = (bool)atoi (pstr_value);
            break;
         case ECOWATT_CONFIG_ENABLE:
            ecowatt_config.enabled = (bool)atoi (pstr_value);
            break;
        }
      }
    }

    // close file
    file.close ();

    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("ECO: Config loaded from LittleFS"));
  }
}

// save configuration
void EcowattSaveConfig () 
{
  uint8_t index;
  char    str_line[128];
  File    file;

  // open file
  file = ffsp->open (D_ECOWATT_CFG, "w");

  // loop to write config
  for (index = 0; index < ECOWATT_CONFIG_MAX; index ++)
  {
    if (GetTextIndexed (str_line, sizeof (str_line), index, kEcowattConfigKey) != nullptr)
    {
      // generate key=value
      strcat (str_line, "=");
      switch (index)
        {
          case ECOWATT_CONFIG_KEY:
            strcat (str_line, ecowatt_config.str_private_key);
            break;
         case ECOWATT_CONFIG_SANDBOX:
            if (ecowatt_config.sandbox) strcat (str_line, "1");
              else strcat (str_line, "0");
            break;
         case ECOWATT_CONFIG_ENABLE:
            if (ecowatt_config.enabled) strcat (str_line, "1");
              else strcat (str_line, "0");
            break;
        }

      // write to file
      strcat (str_line, "\n");
      file.print (str_line);
    }
  }

  // close file
  file.close ();
}

/***********************************************************\
 *                      Functions
\***********************************************************/

// publish Ecowatt JSON thru MQTT
void EcowattPublishJson ()
{
  uint8_t  index, slot;
  uint8_t  slot_day, slot_hour;
  uint32_t time_now;
  TIME_T   dst_now;
  char     str_text[16];

  // current time
  time_now = LocalTime ();
  BreakTime (time_now, dst_now);
  slot_day  = ECOWATT_DAY_TODAY;
  slot_hour = dst_now.hour;

  // Start Ecowatt section
  Response_P (PSTR ("{\"Time\":\"%s\",\"Ecowatt\":{"), GetDateAndTime (DT_LOCAL).c_str ());

  // publish day global status
  ResponseAppend_P (PSTR ("\"dval\":%u"), ecowatt_status.arr_day[slot_day].dvalue);

  // publish hour slot index
  ResponseAppend_P (PSTR (",\"hour\":%u"), slot_hour);

  // publish current slot
  ResponseAppend_P (PSTR (",\"now\":%u"), ecowatt_status.arr_day[slot_day].arr_hvalue[slot_hour]);

  // publish next slot
  if (slot_hour == 23) { slot_hour = 0; slot_day++; }
    else slot_hour++;
  ResponseAppend_P (PSTR (",\"next\":%u"), ecowatt_status.arr_day[slot_day].arr_hvalue[slot_hour]);

  // loop thru number of slots to publish
  for (index = 0; index < ECOWATT_DAY_MAX; index ++)
  {
    ResponseAppend_P (PSTR (",\"day%u\":{\"jour\":\"%s\",\"dval\":%u"), index, ecowatt_status.arr_day[index].str_day, ecowatt_status.arr_day[index].dvalue);
    for (slot = 0; slot < ECOWATT_SLOT_PER_DAY; slot ++) ResponseAppend_P (PSTR (",\"%u\":%u"), slot, ecowatt_status.arr_day[index].arr_hvalue[slot]);
    ResponseAppend_P (PSTR ("}"));
  }

  // end of Ecowatt section and publish JSON
  ResponseAppend_P (PSTR ("}}"));

  // publish under tele/ECOWATT
  MqttPublishPrefixTopicRulesProcess_P (TELE, PSTR("ECOWATT"), Settings->flag.mqtt_sensor_retain);

  // plan next update
  ecowatt_status.time_json = time_now + ECOWATT_UPDATE_JSON;
}

/***********************************************************\
 *                      Stream reception
\***********************************************************/

// plan token retry
void EcowattStreamTokenRetry ()
{
  ecowatt_update.step        = ECOWATT_UPDATE_NONE;
  ecowatt_update.time_token  = LocalTime () + ECOWATT_TOKEN_RETRY;
  ecowatt_update.time_signal = max (ecowatt_update.time_signal, ecowatt_update.time_token + 30);
}

// token connexion
bool EcowattStreamTokenBegin ()
{
  bool is_ok;
  char str_auth[128];

  // check private key
  is_ok = (strlen (ecowatt_config.str_private_key) > 0);
  if (!is_ok) AddLog (LOG_LEVEL_INFO, PSTR ("ECO: Token - Private key missing"));

  // prepare connexion
  if (is_ok)
  {
    // declare connexion
    is_ok = ecowatt_http.begin (ecowatt_wifi, ECOWATT_URL_OAUTH2);

    // connexion refused
    if (!is_ok)
    {
      AddLog (LOG_LEVEL_INFO, PSTR ("ECO: Token - Connexion refused"));
      ecowatt_http.end ();
      ecowatt_wifi.stop ();
    }
  }

  // set headers
  if (is_ok)
  {
    // set authorisation
    strcpy (str_auth, "Basic ");
    strlcat (str_auth, ecowatt_config.str_private_key, sizeof (str_auth));

    // set headers
    ecowatt_http.addHeader ("Authorization", str_auth, false, true);
    ecowatt_http.addHeader ("Content-Type", "application/x-www-form-urlencoded", false, true);

    // set http timeout
    ecowatt_http.setTimeout (2000);
  }

  return is_ok;
}

// start token reception
bool EcowattStreamTokenGet ()
{
  bool is_ok;
  int  http_code;

#ifdef USE_TELEINFO
    // to avoid reception errors, flush reception buffer before long POST
    TeleinfoDataUpdate ();
#endif    // USE_TELEINFO

  // connexion
  http_code = ecowatt_http.POST (nullptr, 0);

  // check if connexion failed
  is_ok = ((http_code == HTTP_CODE_OK) || (http_code == HTTP_CODE_MOVED_PERMANENTLY));
  if (!is_ok)
  {
    AddLog (LOG_LEVEL_INFO, PSTR ("ECO: Token - Error %d (%s)"), http_code, ecowatt_http.errorToString (http_code).c_str());
    ecowatt_http.end ();
    ecowatt_wifi.stop ();
  }

  return is_ok;
}

// end of token reception
bool EcowattStreamTokenEnd ()
{
  uint32_t time_now, token_validity;
  DynamicJsonDocument json_result(256);

  // current time
  time_now = LocalTime ();

  // extract token from JSON
  deserializeJson (json_result, ecowatt_http.getString ().c_str ());

  // get token value
  strlcpy (ecowatt_update.str_token, json_result["access_token"].as<const char*> (), sizeof (ecowatt_update.str_token));

  // get token validity
  token_validity = json_result["expires_in"].as<unsigned int> ();

  // set next token and next signal update
  ecowatt_update.time_token  = time_now + token_validity - 60;
  ecowatt_update.time_signal = max (ecowatt_update.time_signal, time_now + 30);

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("ECO: Token - %s valid for %u seconds"), ecowatt_update.str_token, token_validity);

  // end connexion
  ecowatt_http.end ();
  ecowatt_wifi.stop ();

  return (strlen (ecowatt_update.str_token) > 0);
}

// plan token retry
void EcowattStreamSignalRetry ()
{
  ecowatt_update.step        = ECOWATT_UPDATE_NONE;
  ecowatt_update.time_signal = LocalTime () + ECOWATT_SIGNAL_RETRY;
}

// start of signal reception
bool EcowattStreamSignalBegin ()
{
  bool is_ok;
  char str_auth[128];

  // check token
  is_ok = (strlen (ecowatt_update.str_token) > 0);
  if (!is_ok) AddLog (LOG_LEVEL_INFO, PSTR ("ECO: Signal - Token missing"));

  // prepare connexion
  if (is_ok) 
  {
    // connexion
    if (ecowatt_config.sandbox) is_ok = ecowatt_http.begin (ecowatt_wifi, ECOWATT_URL_SANDBOX);
      else is_ok = ecowatt_http.begin (ecowatt_wifi, ECOWATT_URL_DATA);

    // connexion refused
    if (!is_ok)
    {
      AddLog (LOG_LEVEL_INFO, PSTR ("ECO: Signal - Connexion refused"));
      ecowatt_http.end ();
      ecowatt_wifi.stop ();
    }
  }

  // set headers
  if (is_ok)
  {
    // set authorisation
    strcpy (str_auth, "Bearer ");
    strlcat (str_auth, ecowatt_update.str_token, sizeof (str_auth));

    // set headers
    ecowatt_http.addHeader ("Authorization", str_auth, false, true);
    ecowatt_http.addHeader ("Content-Type", "application/x-www-form-urlencoded", false, true);

    // set http timeout
    ecowatt_http.setTimeout (2000);
  }

  return is_ok;
}

// start of signal reception
bool EcowattStreamSignalGet ()
{
  bool is_ok;
  int  http_code;

#ifdef USE_TELEINFO
    // to avoid reception errors, flush reception buffer before long GET
    TeleinfoDataUpdate ();
#endif    // USE_TELEINFO

  // connexion
  http_code = ecowatt_http.GET ();

  // check if connexion failed
  is_ok = ((http_code == HTTP_CODE_OK) || (http_code == HTTP_CODE_MOVED_PERMANENTLY));
  if (!is_ok)
  {
    AddLog (LOG_LEVEL_INFO, PSTR ("ECO: Signal - Error %d (%s)"), http_code, ecowatt_http.errorToString (http_code).c_str());
    ecowatt_http.end ();
    ecowatt_wifi.stop ();
  }

  return is_ok;
}

// handle end of signals page reception
bool EcowattStreamSignalEnd ()
{
  uint8_t  index, index_array, slot, value;
  String   str_day;
  DynamicJsonDocument json_result(8192);

  // extract token from JSON
  deserializeJson (json_result, ecowatt_http.getString ().c_str ());

  // loop thru all 4 days to get today's and tomorrow's index
  for (index = 0; index < ECOWATT_DAY_MAX; index ++)
  {
    // if sandbox, shift array index to avoid current day fully ok
    if (ecowatt_config.sandbox) index_array = (index + ECOWATT_DAY_MAX - 1) % ECOWATT_DAY_MAX;
      else index_array = index;

    // get day for current signal index
    str_day = json_result["signals"][index]["jour"].as<const char*> ();
    str_day = str_day.substring (0, 10);

    // get global status and date
    ecowatt_status.arr_day[index_array].dvalue = json_result["signals"][index]["dvalue"];
    strlcpy (ecowatt_status.arr_day[index_array].str_day, json_result["signals"][index]["jour"], 12);
    ecowatt_status.arr_day[index_array].str_day[10] = 0;

    // loop to populate the slots
    for (slot = 0; slot < ECOWATT_SLOT_PER_DAY; slot ++)
    {
      value = json_result["signals"][index]["values"][slot]["hvalue"];
      if ((value == ECOWATT_LEVEL_NONE) || (value >= ECOWATT_LEVEL_MAX)) value = ECOWATT_LEVEL_NORMAL;
      ecowatt_status.arr_day[index_array].arr_hvalue[slot] = value;
    }
  }

  // set signal next update
  ecowatt_update.time_signal = LocalTime () + ECOWATT_SIGNAL_RENEW;

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("ECO: Signal - Update till %s"), str_day.c_str ());

  // end connexion
  ecowatt_http.end ();
  ecowatt_wifi.stop ();

  return true;
}

/***********************************************************\
 *                      Callback
\***********************************************************/

// module init
void EcowattModuleInit ()
{
  // disable wifi sleep mode
  Settings->flag5.wifi_no_sleep = true;
  TasmotaGlobal.wifi_stay_asleep = false;
}

// init main status
void EcowattInit ()
{
  uint8_t index, slot;

  // init data*
  strcpy (ecowatt_update.str_token, "");

  // load configuration file
  EcowattLoadConfig ();

  // initialisation of slot array
  for (index = 0; index < ECOWATT_DAY_MAX; index ++)
  {
    // init date
    strcpy (ecowatt_status.arr_day[index].str_day, "");

    // slot initialisation to normal state
    for (slot = 0; slot < ECOWATT_SLOT_PER_DAY; slot ++) ecowatt_status.arr_day[index].arr_hvalue[slot] = ECOWATT_LEVEL_NORMAL;
  }

  // set root certificate
  ecowatt_wifi.setCACert (ecowatt_root_certificate);

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: eco_help to get help on Ecowatt module"));
}


// handle Ecowatt stream update steps (with RTE API)
void EcowattEvery250ms ()
{

}

// called every second
void EcowattEverySecond ()
{
  bool     result;
  uint32_t time_now;

  // Handle stream recetion steps
  time_now = LocalTime ();
  switch (ecowatt_update.step)
  {
    // no current stream operation
    case ECOWATT_UPDATE_NONE:
      // if first update, ask for token update
      if (ecowatt_update.time_token == UINT32_MAX) ecowatt_update.time_token = time_now;

      // check for need of token update
      if (ecowatt_update.time_token <= time_now) ecowatt_update.step = ECOWATT_UPDATE_TOKEN_BEGIN;

      // else, check for need of signal update
      else if (ecowatt_update.time_signal <= time_now) ecowatt_update.step = ECOWATT_UPDATE_SIGNAL_BEGIN;
      break;

    // begin token reception
    case ECOWATT_UPDATE_TOKEN_BEGIN:
      // init token update
      result = EcowattStreamTokenBegin ();
      
      // if success, set next step else plan retry
      if (result) ecowatt_update.step = ECOWATT_UPDATE_TOKEN_GET; 
        else EcowattStreamTokenRetry ();
      break;

    // start token reception
    case ECOWATT_UPDATE_TOKEN_GET:
      // init token update
      result = EcowattStreamTokenGet ();
      
      // if success, set next step else plan retry
      if (result) ecowatt_update.step = ECOWATT_UPDATE_TOKEN_END;
        else EcowattStreamTokenRetry ();
      break;

    // handle end of token reception
    case ECOWATT_UPDATE_TOKEN_END:
      // manage received data
      result = EcowattStreamTokenEnd ();

      // if token is received, reset operation and plan next signal update else plan retry
      if (result) ecowatt_update.step = ECOWATT_UPDATE_NONE;
        else EcowattStreamTokenRetry ();

      // if token is available and signal not plannned, plan it
      if (result && (ecowatt_update.time_signal == UINT32_MAX)) ecowatt_update.time_signal = time_now + 30;
      break;

    case ECOWATT_UPDATE_SIGNAL_BEGIN:
      // init signal update
      result = EcowattStreamSignalBegin ();
      
      // if success, set next step else plan retry
      if (result) ecowatt_update.step = ECOWATT_UPDATE_SIGNAL_GET; 
        else EcowattStreamSignalRetry ();
      break;

    // start signal reception
    case ECOWATT_UPDATE_SIGNAL_GET:
      // init signal reception
      result = EcowattStreamSignalGet ();
      
      // if success, set next step else plan retry
      if (result) ecowatt_update.step = ECOWATT_UPDATE_SIGNAL_END;
        else EcowattStreamSignalRetry ();
      break;

    // handle end of signal reception
    case ECOWATT_UPDATE_SIGNAL_END:
      // manage received data
      result = EcowattStreamSignalEnd ();

      // if signal is received, plan next signal update else plan retry
      if (result) ecowatt_update.step = ECOWATT_UPDATE_NONE;
        else EcowattStreamSignalRetry ();
      break;
  }

  // check for JSON update
  if (ecowatt_status.time_json == UINT32_MAX) result = true;
    else if (ecowatt_status.time_json < time_now) result = true;
    else result = false;
  if (result) EcowattPublishJson ();
}

// called every day at midnight
void EcowattMidnigth ()
{
  uint8_t index, slot;

  // shift day's data from 0 to day+2
  for (index = 0; index < ECOWATT_DAY_MAX - 1; index ++)
  {
    // shift date, global status and slots for day, day+1 and day+2
    strcpy (ecowatt_status.arr_day[index].str_day, ecowatt_status.arr_day[index + 1].str_day);
    ecowatt_status.arr_day[index].dvalue = ecowatt_status.arr_day[index + 1].dvalue;
    for (slot = 0; slot < ECOWATT_SLOT_PER_DAY; slot ++) ecowatt_status.arr_day[index].arr_hvalue[slot] = ecowatt_status.arr_day[index + 1].arr_hvalue[slot];
  }

  // init date, global status and slots for day+3
  index = ECOWATT_DAY_MAX - 1;
  strcpy (ecowatt_status.arr_day[index].str_day, "");
  ecowatt_status.arr_day[index].dvalue = ECOWATT_LEVEL_NORMAL;
  for (slot = 0; slot < ECOWATT_SLOT_PER_DAY; slot ++) ecowatt_status.arr_day[index].arr_hvalue[slot] = ECOWATT_LEVEL_NORMAL;

  // publish data change
  EcowattPublishJson ();
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// append presence forecast to main page
void EcowattWebSensor ()
{
  bool     ca_ready, pk_ready;
  uint8_t  index, slot, opacity;
  uint32_t time_now;   //, time_next;
  TIME_T   dst_now;
  char     str_text[16];

  // init
  time_now = LocalTime ();
  BreakTime (time_now, dst_now);

  // start of ecowatt data
  WSContentSend_P (PSTR ("<div style='font-size:10px;text-align:center;padding:2px 6px;background:#333333;border-radius:12px;'>\n"));

  // title display
  WSContentSend_P (PSTR ("<div style='display:flex;margin:2px 0px 6px 0px;padding:0px;font-size:16px;'>\n"));
  WSContentSend_P (PSTR ("<div style='width:25%%;padding:0px;text-align:left;font-weight:bold;'>Ecowatt</div>\n"));
  WSContentSend_P (PSTR ("<div style='width:75%%;padding:4px 0px 8px 0px;text-align:right;font-size:11px;font-style:italic;'>"));

  // set display message
  if (strlen (ecowatt_config.str_private_key) == 0) WSContentSend_P (PSTR ("private key missing"));
  else if (ecowatt_update.time_token == UINT32_MAX) WSContentSend_P (PSTR ("initialisation stage"));
  else if (ecowatt_update.time_token < time_now + 10) WSContentSend_P (PSTR ("token updating soon"));
  else if (ecowatt_update.time_signal < time_now + 10) WSContentSend_P (PSTR ("signal updating soon"));
  else if (ecowatt_update.time_token < ecowatt_update.time_signal) WSContentSend_P (PSTR ("token updating in %d mn"), 1 + (ecowatt_update.time_token - time_now) / 60);
  else if (ecowatt_update.time_signal != UINT32_MAX) WSContentSend_P (PSTR ("signal updating in %d mn"), 1 + (ecowatt_update.time_signal - time_now) / 60);

  // end of title display
  WSContentSend_P (PSTR ("</div>\n</div>\n"));

  // if ecowatt signal has been received previously
  if (strlen (ecowatt_status.arr_day[0].str_day) > 0)
  {
    // loop thru days
    for (index = 0; index < ECOWATT_DAY_MAX; index ++)
    {
      if (index == 0) opacity = 100; else opacity = 75;
      WSContentSend_P (PSTR ("<div style='display:flex;margin:0px;padding:1px;height:14px;opacity:%u%%;'>\n"), opacity);
      WSContentSend_P (PSTR ("<div style='width:20%%;padding:0px;text-align:left;'>%s</div>\n"), ecowatt_status.arr_day[index].str_day);
      for (slot = 0; slot < ECOWATT_SLOT_PER_DAY; slot ++)
      {
        // segment beginning
        GetTextIndexed (str_text, sizeof (str_text), ecowatt_status.arr_day[index].arr_hvalue[slot], kEcowattLevelColor);
        WSContentSend_P (PSTR ("<div style='width:3.3%%;padding:1px 0px;background-color:%s;"), str_text);

        // first and last segment specificities
        if (slot == 0) WSContentSend_P (PSTR ("border-top-left-radius:6px;border-bottom-left-radius:6px;"));
          else if (slot == ECOWATT_SLOT_PER_DAY - 1) WSContentSend_P (PSTR ("border-top-right-radius:6px;border-bottom-right-radius:6px;"));

        // segment end
        if ((index == ECOWATT_DAY_TODAY) && (slot == dst_now.hour)) WSContentSend_P (PSTR ("font-size:9px;'>⚪</div>\n"), str_text);
          else WSContentSend_P (PSTR ("'></div>\n"));
      }
      WSContentSend_P (PSTR ("</div>\n"));
    }

    // hour scale
    WSContentSend_P (PSTR ("<div style='display:flex;margin:0px;padding:0px;'>\n"));
    WSContentSend_P (PSTR ("<div style='width:20%%;padding:0px;'></div>\n"), 0);
    WSContentSend_P (PSTR ("<div style='width:6.6%%;padding:0px;text-align:left;'>%uh</div>\n"), 0);
    for (index = 1; index < 6; index ++) WSContentSend_P (PSTR ("<div style='width:13.2%%;padding:0px;'>%uh</div>\n"), index * 4);
    WSContentSend_P (PSTR ("<div style='width:6.6%%;padding:0px;text-align:right;'>%uh</div>\n"), 24);
    WSContentSend_P (PSTR ("</div>\n"));
  }  

  // end of ecowatt data
  WSContentSend_P (PSTR ("</div>\n")); 
}

#endif  // USE_WEBSERVER

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xsns119 (uint32_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
    case FUNC_MODULE_INIT:
      EcowattModuleInit ();
      break;
    case FUNC_INIT:
      EcowattInit ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kEcowattCommands, EcowattCommand);
      break;
    case FUNC_EVERY_SECOND:
      if (ecowatt_config.enabled && (TasmotaGlobal.uptime > ECOWATT_STARTUP_DELAY)) EcowattEverySecond ();
      break;
    case FUNC_SAVE_AT_MIDNIGHT:
      if (ecowatt_config.enabled && (TasmotaGlobal.uptime > ECOWATT_STARTUP_DELAY)) EcowattMidnigth ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      if (ecowatt_config.enabled && (TasmotaGlobal.uptime > ECOWATT_STARTUP_DELAY)) EcowattWebSensor ();
      break;
#endif  // USE_WEBSERVER
  }
  
  return result;
}

#endif    // USE_ECOWATT_SERVER
#endif    // USE_UFILESYS
#endif    // ESP32
