/*
  xdrv_92_ipaddress.ino - Fixed IP address management
  
  Copyright (C) 2020  Nicolas Bernaerts
    20/04/2021 - v1.0 - Creation 
    22/04/2021 - v1.1 - Merge IPAddress and InfoJson 
    02/05/2021 - v1.2 - Add Ethernet adapter status (ESP32) 
    22/06/2021 - v1.3 - Change in wifi activation/desactivation 

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

// commands
#define D_CMND_IPADDRESS_ADDR     "addr"
#define D_CMND_IPADDRESS_GATEWAY  "gway"
#define D_CMND_IPADDRESS_NETMASK  "nmsk"
#define D_CMND_IPADDRESS_DNS      "dns"

// web URL
const char D_IPADDRESS_PAGE_CONFIG[] PROGMEM = "/ip";
const char D_IPADDRESS_PAGE_JSON[]   PROGMEM = "/info.json";

// dialog strings
const char D_IPADDRESS_CONFIGURE[] PROGMEM = "Configure IP";

// constant strings
const char D_IPADDRESS_FIELD_INPUT[] PROGMEM = "<p>%s<br><input type='text' name='%s' value='%_I' minlength='7' maxlength='15'></p>\n";

// variables
static struct {
  bool     publish_json = true;
  uint32_t address;
} ipaddress;

/**************************************************\
 *                  Functions
\**************************************************/

// Get IP connexion status
bool IPAddressIsConnected ()
{
  bool is_connected;

  // wifi
  is_connected = ((uint32_t)WiFi.localIP () > 0);
  //WiFi.isConnected ();

#ifdef ESP32
#ifdef USE_ETHERNET
  // ethernet
  is_connected |= ((uint32_t)ETH.localIP () > 0);
#endif // USE_ETHERNET
#endif // ESP32

return is_connected;
}

// Show JSON status (for MQTT)
void IPAddressShowJSON ()
{
  // append Wifi data
  if (WiFi.getMode() != WIFI_OFF) ResponseAppend_P (PSTR (",\"Wifi\":{\"IP\":\"%s\",\"MAC\":\"%s\",\"Host\":\"%s\",\"SSID\":\"%s\",\"Qty\":\"%d\"}"), WiFi.localIP ().toString ().c_str (), WiFi.macAddress ().c_str (), TasmotaGlobal.hostname, SettingsText (SET_STASSID1 + Settings->sta_active), WifiGetRssiAsQuality (WiFi.RSSI ()));

#ifdef ESP32
#ifdef USE_ETHERNET
  // if Ethernet adapter declared, append Ethenet data
  if (Settings->flag4.network_ethernet == 1) ResponseAppend_P (PSTR (",\"Eth\":{\"IP\":\"%s\",\"MAC\":\"%s\",\"Host\":\"%s\"}"), EthernetLocalIP ().toString ().c_str (), EthernetMacAddress ().c_str (), EthernetHostname ());
#endif // USE_ETHERNET
#endif // ESP32
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
  bool     updated = false;
  uint32_t value;
  char     str_argument[32];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg ("save"))
  {
    // device ip address
    WebGetArg (D_CMND_IPADDRESS_ADDR, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0)
    {
      ParseIPv4 (&value, str_argument);
      updated |= (value != Settings->ipv4_address[0]);
      Settings->ipv4_address[0] = value;
    }

    // gateway
    WebGetArg (D_CMND_IPADDRESS_GATEWAY, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0)
    {
      ParseIPv4 (&value, str_argument);
      updated |= (value != Settings->ipv4_address[1]);
      Settings->ipv4_address[1] = value;
    }

    // net mask
    WebGetArg (D_CMND_IPADDRESS_NETMASK, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0)
    {
      ParseIPv4 (&value, str_argument);
      updated |= (value != Settings->ipv4_address[2]);
      Settings->ipv4_address[2] = value;
    }

    // gateway
    WebGetArg (D_CMND_IPADDRESS_DNS, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0)
    {
      ParseIPv4 (&value, str_argument);
      updated |= (value != Settings->ipv4_address[3]);
      Settings->ipv4_address[3] = value;
    }

    // if data have been updated, restart
    if (updated) WebRestart (1);
  }

  // beginning of form
  WSContentStart_P (D_IPADDRESS_CONFIGURE);
  WSContentSendStyle ();
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_IPADDRESS_PAGE_CONFIG);

  // IP address section  
  // ---------------------
  WSContentSend_P (PSTR ("<p><fieldset><legend><b>&nbsp;IP&nbsp;</b></legend>\n"));

  WSContentSend_P (D_IPADDRESS_FIELD_INPUT, PSTR ("Address <small>(0.0.0.0 for DHCP)</small>"), PSTR (D_CMND_IPADDRESS_ADDR),    Settings->ipv4_address[0]);
  WSContentSend_P (D_IPADDRESS_FIELD_INPUT, PSTR ("Gateway"), PSTR (D_CMND_IPADDRESS_GATEWAY), Settings->ipv4_address[1]);
  WSContentSend_P (D_IPADDRESS_FIELD_INPUT, PSTR ("Netmask"), PSTR (D_CMND_IPADDRESS_NETMASK), Settings->ipv4_address[2]);
  WSContentSend_P (D_IPADDRESS_FIELD_INPUT, PSTR ("DNS"),     PSTR (D_CMND_IPADDRESS_DNS),     Settings->ipv4_address[3]);

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
  // "Ext-type":"type","Ext-version":"version","Ext-author":"author"}
  WSContentSend_P ( PSTR (",\"Extension Type\":\"%s\""), EXTENSION_NAME);
  WSContentSend_P ( PSTR (",\"Extension Version\":\"%s\""), EXTENSION_VERSION);
  WSContentSend_P ( PSTR (",\"Extension Aauthor\":\"%s\""), EXTENSION_AUTHOR);
#endif

  // "MQTT":{"Host":"host","Port":"port","Topic":"topic"}
  WSContentSend_P ( PSTR (",\"MQTT Host\":\"%s\""), SettingsText (SET_MQTT_HOST));
  WSContentSend_P ( PSTR (",\"MQTT Pport\":\"%d\""), Settings->mqtt_port);
  WSContentSend_P ( PSTR (",\"MQTT Full Topic\":\"%s\""), GetTopic_P (stopic, CMND, TasmotaGlobal.mqtt_topic, ""));

  // "Wifi":{"IP":"xx.xx.xx.xx","MAC":"xx:xx:xx:xx:xx:xx:xx","Host":"hostname","SSID":"ssid,"Quality":"70"}
  WSContentSend_P ( PSTR (",\"Wifi IP\":\"%s\""), WiFi.localIP ().toString ().c_str ());
  WSContentSend_P ( PSTR (",\"Wifi Mac\":\"%s\""), WiFi.macAddress ().c_str ());
  WSContentSend_P ( PSTR (",\"Wifi Host\":\"%s\""), TasmotaGlobal.hostname);
  WSContentSend_P ( PSTR (",\"Wifi Ssid\":\"%s\""), SettingsText (SET_STASSID1 + Settings->sta_active));
  WSContentSend_P ( PSTR (",\"Wifi Quality\":\"%d\""), WifiGetRssiAsQuality (WiFi.RSSI ()));

#ifdef ESP32
#ifdef USE_ETHERNET

  // "Ethernet":{"IP":"xx.xx.xx.xx","MAC":"xx:xx:xx:xx:xx:xx:xx","Host":"hostname"}
  if (Settings->flag4.network_ethernet == 1)
  {
    WSContentSend_P (PSTR (",\"Eth IP\":\"%s\""), EthernetLocalIP ().toString ().c_str ());
    WSContentSend_P (PSTR (",\"Eth Mac\":\"%s\""), EthernetMacAddress ());
    WSContentSend_P (PSTR (",\"Eth Host\":\"%s\""), EthernetHostname ());
  }

#endif // USE_ETHERNET
#endif // ESP32

  // end of page
  WSContentSend_P ( PSTR ("}"));
  WSContentEnd ();
}

#endif  // USE_WEBSERVER

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xdrv94 (uint8_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
    case FUNC_JSON_APPEND:
      if (ipaddress.publish_json) IPAddressShowJSON ();
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

#endif // USE_IPADDRESS
