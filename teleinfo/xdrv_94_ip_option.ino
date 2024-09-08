/*
  xdrv_92_ip_option.ino - IP address and options management
  
  Copyright (C) 2020  Nicolas Bernaerts
    20/04/2021 - v1.0 - Creation 
    22/04/2021 - v1.1 - Merge IPAddress and InfoJson 
    02/05/2021 - v1.2 - Add Ethernet adapter status (ESP32) 
    22/06/2021 - v1.3 - Change in wifi activation/desactivation 
    07/05/2022 - v1.4 - Add command to enable JSON publishing 
    14/08/2022 - v1.5 - Simplification, slim down of JSON 
    15/04/2024 - v1.6 - Handle IPV6 and IPV4 difference for DNS 
    30/07/2024 - v1.7 - Add common options and rename to xdrv_94_ip_option.ino 

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

#ifdef USE_IP_OPTION

#define XDRV_94             94

// web URL
static const char PSTR_IP_OPTION_PAGE_CONFIG[] PROGMEM = "/ip";
static const char PSTR_IP_OPTION_PAGE_JSON[]   PROGMEM = "/info.json";

// constant strings
static const char PSTR_IP_OPTION_FIELDSET_START[] PROGMEM = "<fieldset><legend>%s</legend>\n";
static const char PSTR_IP_OPTION_FIELDSET_STOP[]  PROGMEM = "</fieldset>\n";
static const char PSTR_IP_OPTION_FIELD_CHECKBOX[] PROGMEM = "<p class='dat'><input type='checkbox' name='%s' title='%s' %s><label for='%s'>%s</label></p>\n";
static const char PSTR_IP_OPTION_FIELD_ADDRESS[]  PROGMEM = "<p class='dat'><span class='pri'>%s</span><span class='sel'><input type='text' name='%s' value='%_I' minlength='7' maxlength='15' placeholder='%s'></span></p>\n";
static const char PSTR_IP_OPTION_FIELD_INPUT[]    PROGMEM = "<p class='dat'><span class='hea'>%s</span><span class='val'><input type='number' name='%s' title='%s' min='%u' max='%u' step='%u' value='%u'></span><span class='uni'>%s</span></p>\n";

// IP address source
enum IPAddressSource { IP_SOURCE_NONE, IP_SOURCE_WIFI, IP_SOURCE_ETHERNET };
//static const char kIPAddressSources[] PROGMEM = "None|Wifi|Eth";

/**************************************************\
 *                  Functions
\**************************************************/

// get active network adapter
uint8_t IPAddressGetSource ()
{
#ifdef USE_ETHERNET
  // ethernet
  if ((uint32_t)ETH.localIP () > 0) return IP_SOURCE_ETHERNET;
  else
#endif // USE_ETHERNET

  // wifi
  if ((uint32_t)WiFi.localIP () > 0) return IP_SOURCE_WIFI;

  else return IP_SOURCE_NONE;
}

// get literal IP address
void IPAddressGetIP (char *pstr_result, const size_t size_result)
{
  uint8_t source;

  // check parameters
  if (pstr_result == nullptr) return;
  if (size_result < 16) return;

  // init IP to DHCP
  source = IPAddressGetSource ();
  strcpy_P (pstr_result, PSTR ("0.0.0.0"));

#ifdef USE_ETHERNET
  // ethernet
  if (source == IP_SOURCE_ETHERNET) strlcpy (pstr_result, ETH.localIP ().toString ().c_str (), size_result);
  else
#endif // USE_ETHERNET

  // wifi
  if (source == IP_SOURCE_WIFI) strlcpy (pstr_result, WiFi.localIP ().toString ().c_str (), size_result);
}

// save IP address in setup
bool IPAddressUpdate (const uint8_t index, const char* pstr_address)
{
  bool     updated;
  uint32_t value;

  if (pstr_address == nullptr) return false;
  if (index > 3) return false;

  ParseIPv4 (&value, pstr_address);
  updated = (value != Settings->ipv4_address[index]);
  if (updated) Settings->ipv4_address[index] = value;
  
  return updated;
}

// Show JSON status (for MQTT)
void IPOptionShowJSON ()
{
#ifdef USE_ETHERNET
  // ethenet MAC
  if (Settings->flag4.network_ethernet == 1) ResponseAppend_P (PSTR (",\"Eth\":\"%s\""), EthernetMacAddress ().c_str ());
#endif // USE_ETHERNET
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// IP address configuration page button
void IPOptionWebConfigButton ()
{
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>"), PSTR_IP_OPTION_PAGE_CONFIG, PSTR ("Configure Options"));
}

// IP address configuration web page
void IPOptionWebPageConfigure ()
{
  bool      restart = false;
  IPAddress dns_ip;
  char      str_argument[32];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg (F ("save")))
  {
    // device ip address
    WebGetArg (PSTR ("ip0"), str_argument, sizeof (str_argument));
    if (strlen (str_argument) == 0) strcpy_P (str_argument, PSTR ("0.0.0.0"));
    restart |= IPAddressUpdate (0, str_argument);

    // gateway
    WebGetArg (PSTR ("ip1"), str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) restart |= IPAddressUpdate (1, str_argument);

    // net mask
    WebGetArg (PSTR ("ip2"), str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) restart |= IPAddressUpdate (2, str_argument);

    // gateway
    WebGetArg (PSTR ("ip3"), str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) restart |= IPAddressUpdate (3, str_argument);

    // SetOption56 : Scan Wifi on restart
    WebGetArg (PSTR ("opt56"), str_argument, sizeof (str_argument));
    if (strcmp_P (str_argument, PSTR ("on")) == 0) Settings->flag3.use_wifi_scan = 1; else Settings->flag3.use_wifi_scan = 0;

    // SetOption57 : Scan Wifi every 44 mn
    WebGetArg (PSTR ("opt57"), str_argument, sizeof (str_argument));
    if (strcmp_P (str_argument, PSTR ("on")) == 0) Settings->flag3.use_wifi_rescan = 1; else Settings->flag3.use_wifi_rescan = 0;

    // SetOption65 : Reset after 7 fast power cut
    WebGetArg (PSTR ("opt65"), str_argument, sizeof (str_argument));
    if (strcmp_P (str_argument, PSTR ("on")) == 0) Settings->flag3.fast_power_cycle_disable = 0; else Settings->flag3.fast_power_cycle_disable = 1;

    // SetOption36 : Boot loop protection
    WebGetArg (PSTR ("opt36"), str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) Settings->param[P_BOOT_LOOP_OFFSET] = (uint8_t)atoi (str_argument);

    // WebRefresh
    WebGetArg (PSTR ("refresh"), str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) Settings->web_refresh = (uint16_t)atoi (str_argument);

    // SetOption146 : Display ESP32 temperature
    WebGetArg (PSTR ("opt146"), str_argument, sizeof (str_argument));
    if (strcmp_P (str_argument, PSTR ("on")) == 0) Settings->flag6.use_esp32_temperature = 1; else Settings->flag6.use_esp32_temperature = 0;

    // SetOption21 : Monitor power with switch OFF
    WebGetArg (PSTR ("opt21"), str_argument, sizeof (str_argument));
    if (strcmp_P (str_argument, PSTR ("on")) == 0) Settings->flag.no_power_on_check = 1; else Settings->flag.no_power_on_check = 0;
  }

  // if data have been updated, restart
  if (restart) WebRestart (1);

  // beginning of form
  WSContentStart_P (PSTR ("Configure Options"));
  WSContentSendStyle ();

  // specific style
  WSContentSend_P (PSTR ("\n<style>\n"));
  WSContentSend_P (PSTR ("p,br,hr {clear:both;}\n")); 
  WSContentSend_P (PSTR ("p.dat {margin:0px 10px;}\n")); 
  WSContentSend_P (PSTR ("fieldset {margin-bottom:24px;padding-top:12px;}\n")); 
  WSContentSend_P (PSTR ("legend {padding:0px 15px;margin-top:-10px;font-weight:bold;}\n")); 
  WSContentSend_P (PSTR ("input,select {margin-bottom:10px;text-align:center;border:none;}\n")); 
  WSContentSend_P (PSTR ("input.left {text-align:left;}\n")); 
  WSContentSend_P (PSTR ("span {float:left;padding-top:4px;}\n")); 
  WSContentSend_P (PSTR ("span.hea {width:60%%;}\n")); 
  WSContentSend_P (PSTR ("span.pri {width:35%%;}\n"));
  WSContentSend_P (PSTR ("span.val {width:25%%;padding:0px;}\n")); 
  WSContentSend_P (PSTR ("span.sel {width:65%%;padding:0px;}\n"));
  WSContentSend_P (PSTR ("span.uni {width:15%%;text-align:center;}\n")); 
  WSContentSend_P (PSTR ("</style>\n"));

  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), PSTR_IP_OPTION_PAGE_CONFIG);


  //    Network 
  // --------------

  WSContentSend_P (PSTR_IP_OPTION_FIELDSET_START, PSTR ("🌐 Network"));

  WSContentSend_P (PSTR_IP_OPTION_FIELD_ADDRESS, PSTR ("Address"), PSTR ("ip0"), Settings->ipv4_address[0], PSTR ("0.0.0.0"));
  WSContentSend_P (PSTR_IP_OPTION_FIELD_ADDRESS, PSTR ("Gateway"), PSTR ("ip1"), Settings->ipv4_address[1], PSTR (""));
  WSContentSend_P (PSTR_IP_OPTION_FIELD_ADDRESS, PSTR ("Netmask"), PSTR ("ip2"), Settings->ipv4_address[2], PSTR (""));

#ifdef USE_IPV6
  DNSGetIP (&dns_ip, 0);
  WSContentSend_P (PSTR_IP_OPTION_FIELD_ADDRESS, PSTR ("DNS"), PSTR ("ip3"), dns_ip, PSTR (""));
#else
  WSContentSend_P (PSTR_IP_OPTION_FIELD_ADDRESS, PSTR ("DNS"), PSTR ("ip3"), Settings->ipv4_address[3], PSTR (""));
#endif // USE_IPV6

  WSContentSend_P (PSTR_IP_OPTION_FIELDSET_STOP);

  //    Sensor 
  // --------------

  WSContentSend_P (PSTR_IP_OPTION_FIELDSET_START, PSTR ("📏 Sensors"));

  if (Settings->flag6.use_esp32_temperature == 1) strcpy_P (str_argument, PSTR ("checked"));
    else str_argument[0] = 0;
  WSContentSend_P (PSTR_IP_OPTION_FIELD_CHECKBOX, PSTR ("opt146"), PSTR ("SetOption146"), str_argument, PSTR ("opt146"), PSTR ("Display ESP32 temperature"));

  if (Settings->flag.no_power_on_check == 1) strcpy_P (str_argument, PSTR ("checked"));
    else str_argument[0] = 0;
  WSContentSend_P (PSTR_IP_OPTION_FIELD_CHECKBOX, PSTR ("opt21"), PSTR ("SetOption21"), str_argument, PSTR ("opt21"), PSTR ("Monitor power with switch OFF"));

  WSContentSend_P (PSTR_IP_OPTION_FIELDSET_STOP);

  //    System 
  // --------------

  WSContentSend_P (PSTR_IP_OPTION_FIELDSET_START, PSTR ("⚙️ System"));

  if (Settings->flag3.use_wifi_scan == 1) strcpy_P (str_argument, PSTR ("checked"));
    else str_argument[0] = 0;
  WSContentSend_P (PSTR_IP_OPTION_FIELD_CHECKBOX, PSTR ("opt56"), PSTR ("SetOption56"), str_argument, PSTR ("opt56"), PSTR ("Scan Wifi on restart"));

  if (Settings->flag3.use_wifi_rescan == 1) strcpy_P (str_argument, PSTR ("checked"));
    else str_argument[0] = 0;
  WSContentSend_P (PSTR_IP_OPTION_FIELD_CHECKBOX, PSTR ("opt57"), PSTR ("SetOption57"), str_argument, PSTR ("opt57"), PSTR ("Scan Wifi every 44 mn"));

  if (Settings->flag3.fast_power_cycle_disable == 0) strcpy_P (str_argument, PSTR ("checked"));
    else str_argument[0] = 0;
  WSContentSend_P (PSTR_IP_OPTION_FIELD_CHECKBOX, PSTR ("opt65"), PSTR ("SetOption65"), str_argument, PSTR ("opt65"), PSTR ("Reset after 7 fast power cut"));

  WSContentSend_P (PSTR ("<hr>\n"));

  WSContentSend_P (PSTR_IP_OPTION_FIELD_INPUT, PSTR ("Boot loop protection"), PSTR ("opt36"), PSTR ("SetOption36"), 0, 200, 1, Settings->param[P_BOOT_LOOP_OFFSET], PSTR ("boot"));

  WSContentSend_P (PSTR_IP_OPTION_FIELD_INPUT, PSTR ("Web refresh"), PSTR ("refresh"), PSTR ("WebRefresh"), 1000, 30000, 1, Settings->web_refresh, PSTR ("ms"));

  WSContentSend_P (PSTR_IP_OPTION_FIELDSET_STOP);
 
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
void IPOptionWebPageJSON ()
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

#endif  // USE_WEBSERVER

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xdrv94 (uint32_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
    case FUNC_JSON_APPEND:
      IPOptionShowJSON ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      Webserver->on (FPSTR (PSTR_IP_OPTION_PAGE_CONFIG), IPOptionWebPageConfigure);
      Webserver->on (FPSTR (PSTR_IP_OPTION_PAGE_JSON),   IPOptionWebPageJSON);
      break;
    case FUNC_WEB_ADD_BUTTON:
      IPOptionWebConfigButton ();
      break;
#endif  // USE_WEBSERVER

  }
  
  return result;
}

#endif    // USE_IP_OPTION

