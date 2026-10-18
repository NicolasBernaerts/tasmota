/*
  xdrv_99_misc_helper.ino - Misc helper functions
  
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
    29/03/2026 v3.0 - Separate ethernet, timezone and cpu load
    15/08/2026 v3.1 - Add CSS and Javascript cache management function
    03/10/2026 v3.2 - Handle fixed IP for ethernet connexion

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
 *                     Data
\*************************************************/

#ifdef USE_MISC_HELPER

#define D_CMND_AUTOCONF     "autoconf"

// IP network configuration
constexpr char kIpAddressLabel[] PROGMEM = "Address|Gateway|Netmask|DNS 1|DNS 2";

// data publication commands
constexpr char kMiscOptionCommands[]     PROGMEM = "|"    "InitDevice"      ;
void (* const MiscOptionCommand[])(void) PROGMEM = { &CmndHelperInitDevice };

// wifi color levels
enum WifiLevelColor                      {  WIFI_BAD , WIFI_AVERAGE, WIFI_CORRECT, WIFI_GOOD, WIFI_MAX };
constexpr char kWifiLevelColor[] PROGMEM = "#e00" "|"  "#e80" "|"  "#ee0" "|" "#0e0"            ;     

// web URL
constexpr char PSTR_MISC_OPTION_PAGE_CONFIG[] PROGMEM = "/option";
constexpr char PSTR_MISC_OPTION_PAGE_JSON[]   PROGMEM = "/info.json";

// constant strings
constexpr char PSTR_MISCOPTION_FIELDSET_START[] PROGMEM = "<fieldset><legend>%s</legend>\n";
constexpr char PSTR_MISCOPTION_FIELDSET_STOP[]  PROGMEM = "</fieldset>\n";
constexpr char PSTR_MISCOPTION_FIELD_CHECKBOX[] PROGMEM = "<p class='dat'><input type='checkbox' name='%s' title='%s' %s><label for='%s'>%s</label></p>\n";
constexpr char PSTR_MISCOPTION_FIELD_INPUT[]    PROGMEM = "<p class='dat'><span class='hea'>%s</span><span class='val'><input type='number' name='%s' min='%d' max='%d' step='%d' value='%d' title='%s'></span><span class='uni'>%s</span></p>\n";

// publication type
#define HELPER_PUBLISH_DELAY_MINIMUM      500
enum HelperPublishType                                            { HELPER_PUBLISH_MQTT, HELPER_PUBLISH_HTTP, HELPER_PUBLISH_HTTPS, HELPER_PUBLISH_MAX };
constexpr uint16_t arr_helper_publish_delay[HELPER_PUBLISH_MAX] = {          1000      ,         1500       ,         2500                             };
static    uint32_t arr_helper_publish_ts[HELPER_PUBLISH_MAX]    = {           0        ,          0         ,           0                              };

/********************************\
 *             Data
\********************************/

static struct {
  bool publish = false;
} helper_status;

/********************************\
 *            Commands
\********************************/

// init device with same options as reset but without FS erase and restart :
//   1..3 = reset all settings
//   4..5 = reset all settings but keep Wi-Fi credentials
//   6    = reset all settings but keep Wi-Fi credentials and MQTT settings
void CmndHelperInitDevice ()
{
  uint16_t mqtt_port;
  char     str_ssid1[strlen     (SettingsText (SET_STASSID1))   + 1];
  char     str_ssid2[strlen     (SettingsText (SET_STASSID2))   + 1];
  char     str_pass1[strlen     (SettingsText (SET_STAPWD1))    + 1];
  char     str_pass2[strlen     (SettingsText (SET_STAPWD2))    + 1];
  char     str_mqtthost[strlen  (SettingsText (SET_MQTT_HOST))  + 1];
  char     str_mqttuser[strlen  (SettingsText (SET_MQTT_USER))  + 1];
  char     str_mqttpwd[strlen   (SettingsText (SET_MQTT_PWD))   + 1];
  char     str_mqtttopic[strlen (SettingsText (SET_MQTT_TOPIC)) + 1];

  // backup wifi
  if ((XdrvMailbox.payload >= 4) && (XdrvMailbox.payload <= 6))
  {
    strncpy (str_ssid1, SettingsText (SET_STASSID1), sizeof (str_ssid1));
    strncpy (str_ssid2, SettingsText (SET_STASSID2), sizeof (str_ssid2));
    strncpy (str_pass1, SettingsText (SET_STAPWD1),  sizeof (str_pass1));
    strncpy (str_pass2, SettingsText (SET_STAPWD2),  sizeof (str_pass2));
  }

  // backup MQTT
  if (XdrvMailbox.payload == 6)
  {
    mqtt_port = Settings->mqtt_port;
    strncpy (str_mqtthost,  SettingsText (SET_MQTT_HOST),  sizeof (str_mqtthost));
    strncpy (str_mqttuser,  SettingsText (SET_MQTT_USER),  sizeof (str_mqttuser));
    strncpy (str_mqttpwd,   SettingsText (SET_MQTT_PWD),   sizeof (str_mqttpwd));
    strncpy (str_mqtttopic, SettingsText (SET_MQTT_TOPIC), sizeof (str_mqtttopic));
  }

  // reset settings to factory
  if ((XdrvMailbox.payload >= 1) && (XdrvMailbox.payload <= 6)) SettingsDefault ();

  // Restore MQTT
  if (XdrvMailbox.payload == 6)
  {
    Settings->mqtt_port = mqtt_port;
    SettingsUpdateText (SET_MQTT_HOST,  str_mqtthost);
    SettingsUpdateText (SET_MQTT_USER,  str_mqttuser);
    SettingsUpdateText (SET_MQTT_PWD,   str_mqttpwd);
    SettingsUpdateText (SET_MQTT_TOPIC, str_mqtttopic);
  }

  // Restore wifi
  if ((XdrvMailbox.payload >= 4) && (XdrvMailbox.payload <= 6))
  {
    SettingsUpdateText (SET_STASSID1, str_ssid1);
    SettingsUpdateText (SET_STASSID2, str_ssid2);
    SettingsUpdateText (SET_STAPWD1,  str_pass1);
    SettingsUpdateText (SET_STAPWD2,  str_pass2);
  }
  
  // command message
  if ((XdrvMailbox.payload >= 1) && (XdrvMailbox.payload <= 6)) ResponseCmndDone ();
    else ResponseCmndFailed ();
}

/**************************************************\
 *              Publication Helpers
\**************************************************/

// set publication flag
void HelperPublishMqtt ()
{
  helper_status.publish = true;
}

bool HelperPublishAllowMqtt ()  { return HelperPublishAllow (HELPER_PUBLISH_MQTT);  }
bool HelperPublishAllowHttp ()  { return HelperPublishAllow (HELPER_PUBLISH_HTTP);  }
bool HelperPublishAllowHttps () { return HelperPublishAllow (HELPER_PUBLISH_HTTPS); }
bool HelperPublishAllow (const uint8_t type)
{
  // check parameter and time
  if (type >= HELPER_PUBLISH_MAX) return false;
  if (!RtcTime.valid) return false;

  // if needed, init timestamp
  if (arr_helper_publish_ts[type] == 0) arr_helper_publish_ts[type] = millis () + arr_helper_publish_delay[type];

  // validate if timeout has been reached
  return (TimeDifference (arr_helper_publish_ts[type], millis()) >= 0);
}

void HelperPublishDeclareMqtt ()  { HelperPublishDeclare (HELPER_PUBLISH_MQTT);  }
void HelperPublishDeclareHttp ()  { HelperPublishDeclare (HELPER_PUBLISH_HTTP);  }
void HelperPublishDeclareHttps () { HelperPublishDeclare (HELPER_PUBLISH_HTTPS); }
void HelperPublishDeclare (const uint8_t type)
{
  uint8_t  index;
  uint32_t timestamp;

  // check parameter
  if (type >= HELPER_PUBLISH_MAX) return;

  // update next publication timestamp
  timestamp = millis ();
  arr_helper_publish_ts[type] = timestamp + arr_helper_publish_delay[type];

  // update all publication timestamp according to minimum delay
  timestamp += HELPER_PUBLISH_DELAY_MINIMUM;
  for (index = 0; index < HELPER_PUBLISH_MAX; index ++)
  {
    // if not initialised, set to next minimum delay
    if (arr_helper_publish_ts[index] == 0) arr_helper_publish_ts[index] = timestamp;

    // else if previous to next minimum delay, set to next minimum delay
    else if (TimeDifference (timestamp, arr_helper_publish_ts[index]) < 0) arr_helper_publish_ts[index] = timestamp;
  }
}

/**************************************************\
 *                Helper Functions
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

#ifdef USE_UFILESYS

// copy autoconf file
void MiscOptionCopyFile (const char *pstr_source, const char *pstr_target)
{
  bool    read_next;
  size_t  length;
  uint8_t arr_data[128];
  File    file_source, file_target;

  // check if source file exists
  if (!ffsp->exists (pstr_source)) return;

  // remove target file if exists
  if (!ffsp->exists (pstr_target)) ffsp->remove (pstr_target);

  // copy source to target 
  file_source = ffsp->open (pstr_source, "r");
  file_target = ffsp->open (pstr_target, "w");
  while (file_source.available ())
  {
    // read line and write back to target
    length = file_source.read (arr_data, sizeof (arr_data));
    file_target.write (arr_data, length);
  }
  file_source.close ();
  file_target.close ();
}

#endif  // USE_UFILESYS

/**************************************************\
 *        Helper to compare and update data
\**************************************************/

// update boolean given as parameter, return true if changed
bool MiscCommandUpdateFlag (const char* pstr_input, bool &current)
{
  bool changed, status;

  // check parameters
  if (pstr_input == nullptr) return false;
  if (strlen (pstr_input) == 0) return false;
  
  // extract param flag
  status = (strcmp_P (pstr_input, PSTR ("1")) == 0);
  if (!status) status = (strcmp_P (pstr_input, PSTR ("on")) == 0);
  if (!status) status = (strcmp_P (pstr_input, PSTR ("ON")) == 0);

  // set result and flag
  changed  = (status != current);
  current = status;

  return changed;
}

// update integer given as parameter according to input, return true if different
bool MiscCommandUpdateInteger (const char* pstr_input, int &current, const int minimum, const int maximum)
{
  bool changed;
  int  value;

  // check parameters
  if (pstr_input == nullptr) return false;
  if (strlen (pstr_input) == 0) return false;

  // extract param flag, set result and flag
  value = atoi (pstr_input);
  if ((minimum != INT_MIN) && (value < minimum)) value = current;
  else if ((maximum != INT_MAX) && (value > maximum)) value = current;
  changed = (value != current);
  current = value;

  return changed;
}

// update string given as parameter (null reset the value)
bool MiscCommandUpdateString (char* pstr_input, char *pstr_result, const size_t size_result)
{
  bool changed;

  // check parameters
  if (pstr_input == nullptr) return false;
  if (pstr_result == nullptr) return false;
  if (size_result == 0) return false;
  
  // detect change
  if (strcmp_P (pstr_input, PSTR ("null")) == 0) pstr_input[0] = 0;
  changed = (strcmp (pstr_input, pstr_result) != 0);
  strlcpy (pstr_result, pstr_input, size_result);

  return changed;
}

/**************************************************\
 *        Helper for Network
\**************************************************/

bool MiscOptionIsWifiActive ()
{
  bool result;

  result = WifiHasIPv4 ();
  if (!result) result = WifiHasIPv6 ();

  return result;
}

// save IP address in setup
bool MiscOptionIPAddressUpdate (const uint8_t index, const char* pstr_address)
{
  bool     updated = false;
  uint32_t value;

  if (pstr_address == nullptr) return false;
  if (index > 4) return false;

  // convert string into IP
  ParseIPv4 (&value, pstr_address);

#ifdef USE_ETHERNET
  // if ethernet is connected
  if (EthernetHasIP ())
  {
    // apply to ethernet config
    updated = (value != Settings->eth_ipv4_address[index]);
    if (updated) Settings->eth_ipv4_address[index] = value;

    // if fixed IP address, set wifi back to DHCP
    if ((index == 0) && (value != 0)) Settings->ipv4_address[index] = 0;
  }
  else
#endif    // USE_ETHERNET

  if (WifiHasIP ()) 
  {
    // apply to wifi config
    updated = (value != Settings->ipv4_address[index]);
    if (updated) Settings->ipv4_address[index] = value;

    // if fixed IP address, set ethernet back to DHCP
    if ((index == 0) && (value != 0)) Settings->eth_ipv4_address[index] = 0;
  }

  return updated;
}

/*********************************************\
 *       Helper for Autoconf management
\*********************************************/

#ifdef USE_AUTOCONF
#ifdef USE_WEBSERVER

// ethernet board argument
bool AutoconfWebConfigGetArgs ()
{
  bool   changed, read_next;
  int    index, select;
  size_t length;
  char  *pstr_separator;
  char   str_text[64];
  char   str_source[32];
  char   str_target[32];
  File   file;

  // check if file exists
  strcpy_P (str_source, PSTR ("/.autoconf.ini"));
  if (!ffsp->exists (str_source)) return false;

  // check if board is selected
  select  = -1;
  changed = WebGetArgInt (D_CMND_AUTOCONF, select, 0, INT_MAX);
  if (changed)
  {
    // parse autoconf.ini to find selection
    changed   = false;
    index     = 0;
    file      = ffsp->open (str_source, "r");
    read_next = file.available ();
    while (read_next)
    {
      // read line and append \0
      length = file.readBytesUntil ('\n', str_text, sizeof (str_text) - 1);
      if (length > 0) length--;
      str_text[length] = 0;

      // check if selection reached
      changed = (index == select);
      read_next = (!changed && file.available ());
      index++;
    }
    file.close ();

    // if template found
    if (changed)
    {
      // look for separator
      pstr_separator = strchr (str_text, ';');
      if (pstr_separator != nullptr)
      {
        // set destination as Autoconf Profile.autoconf
        *pstr_separator = 0;
        sprintf_P (str_target, PSTR ("/%s.autoconf"), str_text);

        // set source
        pstr_separator++;
        strcpy (str_source, pstr_separator);

        // copy autoconf file
        MiscOptionCopyFile (str_source, str_target);

        // if needed, remove autoconf flag
        strcpy (str_target, PSTR ("/.autoconf"));
        if (ffsp->exists (str_target)) ffsp->remove (str_target);
      }
    }
  }

  return changed;
}

// board config page
void AutoconfWebConfigDisplay ()
{
  bool   read_next;
  int    index;
  size_t length;
  char  *pstr_separator;
  char   str_checked[16];
  char   str_text[64];
  char   str_template[32];
  char   str_filename[32];
  File   file;   

  // check if file exists
  strcpy_P (str_filename, PSTR ("/.autoconf.ini"));
  if (!ffsp->exists (str_filename)) return;

  // get template name
  strcpy_P (str_template, SettingsText (SET_TEMPLATE_NAME));

  // board selection
  WSContentSend_P (PSTR ("<fieldset><legend>⚙️ Set Board Type</legend>\n\n"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));
  WSContentSend_P (PSTR ("<p class='dat'><input type='radio' name='%s' value='-1'>No change</p>\n"), D_CMND_AUTOCONF);

  // parse autoconf.ini
  index = 0;
  file  = ffsp->open (str_filename, "r");
  read_next = file.available ();
  while (read_next)
  {
    // read line and append \0
    length = file.readBytesUntil ('\n', str_text, sizeof (str_text) - 1);
    str_text[length] = 0;

    // look for separator
    pstr_separator = strchr (str_text, ';');
    if (pstr_separator != nullptr)
    {
      // separate label and filename
      *pstr_separator = 0;
      pstr_separator++;

      // check for current template
      WSContentSend_P (PSTR ("<p class='dat'><input type='radio' name='%s' value='%d'>%s</p>\n"), D_CMND_AUTOCONF, index, str_text);
    }

    // increment
    index++;
    read_next = file.available ();
  }
  file.close ();

  WSContentSend_P (PSTR ("</div>\n"));
  WSContentSend_P (PSTR ("</fieldset>\n"));
}

#endif  // USE_WEBSERVER
#endif  // USE_AUTOCONF

/***********************************************\
 *                 Callback
\***********************************************/

void HelperEvery100ms ()
{
  // if flag is set, publish data
  if (helper_status.publish)
  {
    // publish SENSOR
    MqttPublishTeleperiodSensor();


    // declare publication
    HelperPublishDeclare (HELPER_PUBLISH_MQTT);

    // reset publication flag
    helper_status.publish = false;
  }
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// IP address configuration page button
void HelperWebConfigButton ()
{
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>"), PSTR_MISC_OPTION_PAGE_CONFIG, PSTR ("Options"));
}

// IP address configuration web page
void HelperWebPageConfigure ()
{
  bool     restart = false;
  bool     act_dhcp, new_dhcp;
  uint8_t  index;
  uint32_t ip_address;
  char     str_arg[8], str_argument[32];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // check dhcp status
  new_dhcp = act_dhcp = (Settings->ipv4_address[0] == 0);

  // page comes from save button on configuration page
  if (Webserver->hasArg (F ("save")))
  {
    // dhcp : check DHCP status
    restart |= WebGetArgFlag (PSTR ("dhcp"), new_dhcp);

    // if needed, set DHCP, else set fixed IP
    if (new_dhcp) Settings->ipv4_address[0] = 0;
    else for (index = 0; index < 5; index ++)
    {
      sprintf_P (str_arg, PSTR ("ip%u"), index);
      WebGetArg (str_arg, str_argument, sizeof (str_argument));
      if (strlen (str_argument) > 0) restart |= MiscOptionIPAddressUpdate (index, str_argument);
    }

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

#ifdef USE_AUTOCONF
    restart |= AutoconfWebConfigGetArgs ();
#endif  // USE_AUTOCONF

#ifdef USE_TIMEZONE
    restart |= TimezoneWebConfigGetArgs ();
#endif  // USE_TIMEZONE
}

  // if data have been updated, restart
  if (restart) WebRestart (1);

  // beginning of form
  WSContentStart_P (PSTR ("Configure Options"));
  WSContentSendStyle ();

  // specific style
  WSContentSend_P (PSTR ("<style>\n"));
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

  // start of form
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

  // Autoconf selection  
#ifdef USE_AUTOCONF
  AutoconfWebConfigDisplay ();
#endif  // USE_AUTOCONF

  // NTP server section  
#ifdef USE_TIMEZONE
  TimezoneWebConfigDisplay ();
#endif  // USE_TIMEZONE

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
void HelperWebPageJSON ()
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

  // disable separation lines
  WSContentSend_P (PSTR ("table hr{display:none;}\n"));

  // display section
  WSContentSend_P (PSTR ("div.sec{font-size:0.9rem;text-align:center;padding:4px 6px;margin-bottom:5px;background:#333333;border-radius:8px;}\n"));

  // centering of multi-buttons
  WSContentSend_P (PSTR ("p.center {text-align:center;}\n"));

  // style : wifi
  WSContentSend_P (PSTR (".a3{width:24px;height:24px;top:1px;left:3px;}\n"));
  WSContentSend_P (PSTR (".a2{width:18px;height:18px;top:5px;left:6px;}\n"));
  WSContentSend_P (PSTR (".a1{width:12px;height:12px;top:9px;left:9px;}\n"));
  WSContentSend_P (PSTR (".a0{width:4px;height:4px;top:13px;left:13px;}\n"));

  // style : status
  WSContentSend_P (PSTR ("div#l1 .top{margin:2px;padding:1px 2px;border-radius:5px;font-size:10px;border:#aaa 1px solid;}\n"));
  WSContentSend_P (PSTR ("div#l1 .ok{border:#0b0 1px solid;color:#0b0;}\n"));
  WSContentSend_P (PSTR ("div#l1 .ko{border:#b00 1px solid;color:#b00;}\n"));
  WSContentSend_P (PSTR ("div#l1 .disabled{border:#666 1px solid;color:#666;}\n"));
  WSContentSend_P (PSTR ("div#l1 .rssi{margin-left:0px;border:none;}\n"));
  WSContentSend_P (PSTR ("div#l1 .span{margin-left:2px;border:none;}\n"));

  WSContentSend_P (PSTR ("</style>\n"));
}

// Left top bar
//   - wifi strength
//   - ethernet connexion
//   - load
void HelperWebStatusLeft ()
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
  if (Settings->flag4.network_wifi == 1) WSContentSend_P (PSTR ("<span class='top rssi'>%d%%</span>\n"), WifiGetRssiAsQuality (WiFi.RSSI ()));
#endif    //  USE_WEB_STATUS_LINE_WIFI

#ifdef USE_ETHERNET
  EthernetSwitchWebStatusLeft ();
#endif    //  USE_ETHERNET

#ifdef USE_CPU_LOAD
  CpuLoadWebStatusLeft ();
#endif    //  USE_CPU_LOAD
}

// Right top bar
//  - time
void HelperWebStatusRight ()
{
  // time indicator
  WSContentSend_P (PSTR ("<span class='top"));
  if (!RtcTime.valid) WSContentSend_P (PSTR (" ko"));
  WSContentSend_P (PSTR ("'>%02d:%02d:%02d</span>\n"), RtcTime.hour, RtcTime.minute, RtcTime.second);
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
//    case FUNC_INIT:
//      MiscOptionInit ();
//      break;

    case FUNC_COMMAND:
      result = DecodeCommand (kMiscOptionCommands, MiscOptionCommand);
      break;

    case FUNC_EVERY_100_MSECOND:
      HelperEvery100ms ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_MAIN_BUTTON:
      MiscOptionWebMainButton ();
      break;
      
    case FUNC_WEB_ADD_BUTTON:
      HelperWebConfigButton ();
      break;

    case FUNC_WEB_STATUS_LEFT:
      HelperWebStatusLeft ();
      break;

    case FUNC_WEB_STATUS_RIGHT:
      HelperWebStatusRight ();
      break;
      
    case FUNC_WEB_ADD_HANDLER:
      Webserver->on (FPSTR (PSTR_MISC_OPTION_PAGE_CONFIG), HelperWebPageConfigure);
      Webserver->on (FPSTR (PSTR_MISC_OPTION_PAGE_JSON),   HelperWebPageJSON);
      break;
#endif  // USE_WEBSERVER
  }

#ifdef USE_ETHERNET
  if (!result) result = XdrvEthernetSwitch (function);
#endif  // USE_ETHERNET

#ifdef USE_TIMEZONE
  if (!result) result = XdrvTimezone (function);
#endif  // USE_TIMEZONE

#ifdef USE_CPU_LOAD
  if (!result) result = XdrvCpuLoad (function);
#endif  // USE_CPU_LOAD

  return result;
}

#endif    // USE_MISC_HELPER
