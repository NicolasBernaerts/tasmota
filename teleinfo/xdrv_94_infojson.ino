/*
  xdrv_94_infojson.ino - Module information JSON (/info.json)
  
  Copyright (C) 2020  Nicolas Bernaerts
    22/05/2020 - v1.0 - Creation 
    12/09/2020 - v1.1 - Correction of PSTR bug 
    19/09/2020 - v1.2 - Add MAC address to JSON 

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

// JSON strings
#define D_JSON_INFO_WIFI    "Wifi"
#define D_JSON_INFO_MAC     "MAC"
#define D_JSON_INFO_IP      "IP"

// web URL
const char D_INFO_PAGE_JSON[] PROGMEM = "/info.json";

/**************************************************\
 *                  Functions
\**************************************************/

// Show JSON status (for MQTT)
void InfoShowJSON ()
{
  String str_json;

  // append to MQTT message
  str_json  = ",\"";
  str_json += D_JSON_INFO_WIFI;
  str_json += "\":{\"";
  str_json += D_JSON_INFO_MAC;
  str_json += "\":\"";
  str_json += WiFi.macAddress();
  str_json += "\",";
  str_json += "\"";
  str_json += D_JSON_INFO_IP;
  str_json += "\":\"";
  str_json += WiFi.localIP().toString();
  str_json += "\"}";

  // append JSON to MQTT message
  ResponseAppend_P (PSTR("%s"), str_json.c_str ());
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// JSON version page
void InfoWebPageJson ()
{
  String json_version;
  char stopic[TOPSZ];

  // generate version string
  json_version  = "{";
  json_version += "\"Module\":\"";
  json_version += ModuleName ();
  json_version += "\",";

  json_version += "\"";
  json_version += D_PROGRAM_VERSION;
  json_version += "\":\"";
  json_version += TasmotaGlobal.version;
  json_version += "\",";

  json_version += "\"Extension type\":\"";
  json_version += EXTENSION_NAME;
  json_version += "\",";

  json_version += "\"Extension version\":\"";
  json_version += EXTENSION_VERSION;
  json_version += "\",";

  json_version += "\"Extension author\":\"";
  json_version += EXTENSION_AUTHOR;
  json_version += "\",";

  json_version += "\"";
  json_version += D_UPTIME;
  json_version += "\":\"";
  json_version += GetUptime ();
  json_version += "\",";

  json_version += "\"";
  json_version += D_FRIENDLY_NAME;
  json_version += "\":\"";
  json_version += SettingsText (SET_DEVICENAME);
  json_version += "\",";

  json_version += "\"";
  json_version += D_SSID;
  json_version += "\":\"";
  json_version += SettingsText (SET_STASSID1 + Settings.sta_active);
  json_version += " (";
  json_version += WifiGetRssiAsQuality (WiFi.RSSI ());
  json_version += "%%)\",";

  json_version += "\"";
  json_version += D_HOSTNAME;
  json_version += "\":\"";
  json_version += TasmotaGlobal.hostname;
  json_version += "\",";

  json_version += "\"";
  json_version += D_MQTT_HOST;
  json_version += "\":\"";
  json_version += SettingsText (SET_MQTT_HOST);
  json_version += "\",";

  json_version += "\"";
  json_version += D_MQTT_PORT;
  json_version += "\":\"";
  json_version += Settings.mqtt_port;
  json_version += "\",";

  json_version += "\"";
  json_version += D_MQTT_FULL_TOPIC;
  json_version += "\":\"";
  json_version += GetTopic_P (stopic, CMND, TasmotaGlobal.mqtt_topic, "");
  json_version += "\",";

  json_version += "\"";
  json_version += D_IP_ADDRESS;
  json_version += "\":\"";
  json_version += WiFi.localIP ().toString ();
  json_version += "\",";

  json_version += "\"";
  json_version += D_MAC_ADDRESS;
  json_version += "\":\"";
  json_version += WiFi.macAddress ();
  json_version += "\"";

  json_version += "}";

  // publish public page
  WSContentBegin (200, CT_HTML);
  WSContentSend_P (json_version.c_str ());
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
      InfoShowJSON ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      Webserver->on (FPSTR (D_INFO_PAGE_JSON), InfoWebPageJson);
      break;
#endif  // USE_WEBSERVER
  }
  
  return result;
}

#endif // USE_INFOJSON
