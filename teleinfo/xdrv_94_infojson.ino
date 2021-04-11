/*
  xdrv_94_infojson.ino - Module information JSON (/info.json)
  
  Copyright (C) 2020  Nicolas Bernaerts
    22/05/2020 - v1.0 - Creation 
    12/09/2020 - v1.1 - Correction of PSTR bug 
    19/09/2020 - v1.2 - Add MAC address to JSON 
    10/04/2021 - v1.3 - Remove use of String to avoid heap fragmentation 

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
 *                Info JSON page
\*************************************************/

#ifdef USE_INFOJSON

#define XDRV_94             94

// constants
#define INFOJSON_JSON_MAX      64

// strings
const char D_INFOJSON_PAGE[] PROGMEM = "/info.json";

/**************************************************\
 *                  Functions
\**************************************************/

// Show JSON status (for MQTT)
void InfoJsonShowJSON ()
{
  // append JSON to MQTT message
  ResponseAppend_P (PSTR (",\"Wifi\":{\"Mac\":\"%s\",\"IP\":\"%s\"}"), WiFi.macAddress().c_str (), WiFi.localIP().toString().c_str ());
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// JSON version page
void InfoJsonWebPage ()
{
  char stopic[TOPSZ];

  // start of page
  WSContentBegin (200, CT_HTML);

  // {"Module":"NomDuModule","Version":"x.x.x",
  WSContentSend_P ( PSTR ("{\"Module\":\"%s\",\"Version\":\"%s\","), ModuleName ().c_str (), TasmotaGlobal.version);

  // "Extension type":"type","Extension version":"version","Extension author":"author",
  WSContentSend_P ( PSTR ("\"Extension type\":\"%s\",\"Extension version\":\"%s\",\"Extension author\":\"%s\","), EXTENSION_NAME, EXTENSION_VERSION, EXTENSION_AUTHOR);

  // "Uptime":"xxxxxx","Friendly name":"name",
  WSContentSend_P ( PSTR ("\"%s\":\"%s\",\"%s\":\"%s\","), D_UPTIME, GetUptime ().c_str (), D_FRIENDLY_NAME, SettingsText (SET_DEVICENAME));

  // "SSID":"ssid (70%)","Hostname":"hostname",
  WSContentSend_P ( PSTR ("\"%s\":\"%s (%d%%)\",\"%s\":\"%s\","), D_SSID, SettingsText (SET_STASSID1 + Settings.sta_active), WifiGetRssiAsQuality (WiFi.RSSI ()), D_HOSTNAME, TasmotaGlobal.hostname);

  // "MQTT Host":"host","MQTT Port":"port","MQTT Topic":"topic",
  WSContentSend_P ( PSTR ("\"%s\":\"%s\",\"%s\":\"%d\",\"%s\":\"%s\","), D_MQTT_HOST, SettingsText (SET_MQTT_HOST), D_MQTT_PORT, Settings.mqtt_port, D_MQTT_FULL_TOPIC, GetTopic_P (stopic, CMND, TasmotaGlobal.mqtt_topic, ""));

  // "IP":"xx.xx.xx.xx","MAC":"xx:xx:xx:xx:xx:xx:xx"}
  WSContentSend_P ( PSTR ("\"%s\":\"%s\",\"%s\":\"%s\"}"), D_IP_ADDRESS, WiFi.localIP ().toString ().c_str (), D_MAC_ADDRESS, WiFi.macAddress ().c_str ());

  // end of page
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
      InfoJsonShowJSON ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      Webserver->on (FPSTR (D_INFOJSON_PAGE), InfoJsonWebPage);
      break;
#endif  // USE_WEBSERVER
  }
  
  return result;
}

#endif // USE_INFOJSON
