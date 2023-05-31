/*
  xdrv_92_ipaddress.ino - Fixed IP address management
  
  Copyright (C) 2020  Nicolas Bernaerts
    20/04/2021 - v1.0 - Creation 
    22/04/2021 - v1.1 - Merge IPAddress and InfoJson 
    02/05/2021 - v1.2 - Add Ethernet adapter status (ESP32) 
    22/06/2021 - v1.3 - Change in wifi activation/desactivation 
    07/05/2022 - v1.4 - Add command to enable JSON publishing 
    14/08/2022 - v1.5 - Simplification, slim down of JSON 

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

#ifdef USE_IPADDRESS

#define XDRV_94                   94

// web URL
const char D_IPADDRESS_PAGE_CONFIG[] PROGMEM = "/ip";
const char D_IPADDRESS_PAGE_JSON[]   PROGMEM = "/info.json";

// dialog strings
const char D_IPADDRESS_CONFIGURE[] PROGMEM = "Configure IP";

// constant strings
const char D_IPADDRESS_FIELD_INPUT[] PROGMEM = "<p>%s<br><input type='text' name='%s' value='%_I' minlength='7' maxlength='15'></p>\n";

// IP address source
enum IPAddressSource { IP_SOURCE_NONE, IP_SOURCE_WIFI, IP_SOURCE_ETHERNET };
const char kIPAddressSources[] PROGMEM = "None|Wifi|Eth";

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
String IPAddressGetIP ()
{
  uint8_t source = IPAddressGetSource ();
  String  str_result = "0.0.0.0";

#ifdef USE_ETHERNET
  // ethernet
  if (source == IP_SOURCE_ETHERNET) str_result = ETH.localIP ().toString ();
  else
#endif // USE_ETHERNET

  // wifi
  if (source == IP_SOURCE_WIFI) str_result = WiFi.localIP ().toString ();

  return str_result;
}

// save IP address in setup
bool IPAddressUpdateSetup (const uint8_t index, const char* pstr_address)
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
void IPAddressShowJSON ()
{
  char str_adapter[16];

  // IP address
  ResponseAppend_P (PSTR (",\"IP\":\"%s\""), IPAddressGetIP ().c_str ());

  // network adapter
  GetTextIndexed (str_adapter, sizeof (str_adapter), IPAddressGetSource (), kIPAddressSources);
  ResponseAppend_P (PSTR (",\"Net\":\"%s\""), str_adapter);

  // wifi MAC
  if (WiFi.getMode() != WIFI_OFF) ResponseAppend_P (PSTR (",\"Wifi-mac\":\"%s\""), WiFi.macAddress ().c_str ());

#ifdef USE_ETHERNET
  // ethenet MAC
  if (Settings->flag4.network_ethernet == 1) ResponseAppend_P (PSTR (",\"Eth-mac\":\"%s\""), EthernetMacAddress ().c_str ());
#endif // USE_ETHERNET
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// IP address configuration page button
void IPAddressWebConfigButton ()
{
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>"), D_IPADDRESS_PAGE_CONFIG, D_IPADDRESS_CONFIGURE);
}

// IP address configuration web page
void IPAddressWebPageConfigure ()
{
  bool updated = false;
  char str_argument[32];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // device ip address
  WebGetArg ("ip0", str_argument, sizeof (str_argument));
  if (strlen (str_argument) > 0) updated |= IPAddressUpdateSetup (0, str_argument);

  // gateway
  WebGetArg ("ip1", str_argument, sizeof (str_argument));
  if (strlen (str_argument) > 0) updated |= IPAddressUpdateSetup (1, str_argument);


  // net mask
  WebGetArg ("ip2", str_argument, sizeof (str_argument));
  if (strlen (str_argument) > 0) updated |= IPAddressUpdateSetup (2, str_argument);

  // gateway
  WebGetArg ("ip3", str_argument, sizeof (str_argument));
  if (strlen (str_argument) > 0) updated |= IPAddressUpdateSetup (3, str_argument);

  // if data have been updated, restart
  if (updated) WebRestart (1);

  // beginning of form
  WSContentStart_P (D_IPADDRESS_CONFIGURE);
  WSContentSendStyle ();
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_IPADDRESS_PAGE_CONFIG);

  // IP address section  
  // ---------------------
  WSContentSend_P (PSTR ("<p><fieldset><legend><b>&nbsp;🔗 IP&nbsp;</b></legend>\n"));

  WSContentSend_P (D_IPADDRESS_FIELD_INPUT, PSTR ("Address <small>(0.0.0.0 for DHCP)</small>"), PSTR ("ip0"), Settings->ipv4_address[0]);
  WSContentSend_P (D_IPADDRESS_FIELD_INPUT, PSTR ("Gateway"),                                   PSTR ("ip1"), Settings->ipv4_address[1]);
  WSContentSend_P (D_IPADDRESS_FIELD_INPUT, PSTR ("Netmask"),                                   PSTR ("ip2"), Settings->ipv4_address[2]);
  WSContentSend_P (D_IPADDRESS_FIELD_INPUT, PSTR ("DNS"),                                       PSTR ("ip3"), Settings->ipv4_address[3]);

  WSContentSend_P (PSTR ("</fieldset></p><br>\n"));

  // save button  
  // --------------
  WSContentSend_P (PSTR ("<p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), D_SAVE);
  WSContentSend_P (PSTR ("</form>\n"));

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

// JSON version page
void IPAddressWebPageJSON ()
{
  char stopic[TOPSZ];

  // start of page
  WSContentBegin (200, CT_HTML);
  WSContentSend_P ( PSTR ("{"));

  // "Module":"NomDuModule","Version":"x.x.x","Uptime":"xxxxxx","Friendly name":"name",
  WSContentSend_P ( PSTR ("\"Module\":\"%s\""), ModuleName ().c_str ());
  WSContentSend_P ( PSTR (",\"Program Version\":\"%s\""), TasmotaGlobal.version);
  WSContentSend_P ( PSTR (",\"Uptime\":\"%s\""), GetUptime ().c_str ());
  WSContentSend_P ( PSTR (",\"Friendly Name\":\"%s\""), SettingsText (SET_DEVICENAME));

#ifdef EXTENSION_NAME
  // "Extension type":"type","Extension version":"version","Extension author":"author"}
  WSContentSend_P ( PSTR (",\"Extension Type\":\"%s\""), EXTENSION_NAME);
  WSContentSend_P ( PSTR (",\"Extension Version\":\"%s\""), EXTENSION_VERSION);
  WSContentSend_P ( PSTR (",\"Extension Author\":\"%s\""), EXTENSION_AUTHOR);
#endif

  // "MQTT":{"Host":"host","Port":"port","Topic":"topic"}
  WSContentSend_P ( PSTR (",\"MQTT Host\":\"%s\""), SettingsText (SET_MQTT_HOST));
  WSContentSend_P ( PSTR (",\"MQTT Port\":\"%d\""), Settings->mqtt_port);
  WSContentSend_P ( PSTR (",\"MQTT Full Topic\":\"%s\""), GetTopic_P (stopic, CMND, TasmotaGlobal.mqtt_topic, ""));

  // "Wifi IP":"xx.xx.xx.xx","Wifi MAC":"xx:xx:xx:xx:xx:xx:xx","Wifi Host":"hostname","Wifi SSID":"ssid,"Wifi Quality":"70"}
  WSContentSend_P ( PSTR (",\"Wifi IP\":\"%s\""), WiFi.localIP ().toString ().c_str ());
  WSContentSend_P ( PSTR (",\"Wifi MAC\":\"%s\""), WiFi.macAddress ().c_str ());
  WSContentSend_P ( PSTR (",\"Wifi Host\":\"%s\""), TasmotaGlobal.hostname);
  WSContentSend_P ( PSTR (",\"Wifi SSID\":\"%s\""), SettingsText (SET_STASSID1 + Settings->sta_active));
  WSContentSend_P ( PSTR (",\"Wifi Quality\":\"%d\""), WifiGetRssiAsQuality (WiFi.RSSI ()));

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
      IPAddressShowJSON ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      Webserver->on (FPSTR (D_IPADDRESS_PAGE_CONFIG), IPAddressWebPageConfigure);
      Webserver->on (FPSTR (D_IPADDRESS_PAGE_JSON), IPAddressWebPageJSON);
      break;
    case FUNC_WEB_ADD_BUTTON:
      IPAddressWebConfigButton ();
      break;
#endif  // USE_WEBSERVER

  }
  
  return result;
}

#endif    // USE_IPADDRESS

