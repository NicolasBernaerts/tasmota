/*
  xdrv_94_infojson.ino - Module information JSON (/info.json)
  
  Copyright (C) 2020  Nicolas Bernaerts
    22/05/2020 - v1.0 - Creation 

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

#define XDRV_94                   94

#define D_PAGE_INFO_JSON          "info.json"

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
  json_version = PSTR ("{");
  json_version += PSTR ("\"Module\":\"") + ModuleName() + "\",";
  json_version += PSTR ("\"") + String (D_PROGRAM_VERSION) + "\":\"" + my_version + "\",";
  json_version += PSTR ("\"Extension type\":\"") + String (EXTENSION_NAME) + "\",";
  json_version += PSTR ("\"Extension version\":\"") + String (EXTENSION_VERSION) + "\",";
  json_version += PSTR ("\"Extension author\":\"") + String (EXTENSION_AUTHOR) + "\",";
  json_version += PSTR ("\"") + String (D_UPTIME) + "\":\"" + GetUptime() + "\",";
  json_version += PSTR ("\"") + String (D_FRIENDLY_NAME) + "\":\"" + SettingsText(SET_FRIENDLYNAME1) + "\",";
  json_version += PSTR ("\"") + String (D_SSID) + "\":\"" + SettingsText(SET_STASSID1 + Settings.sta_active) + " (" + WifiGetRssiAsQuality(WiFi.RSSI()) + "%%)\",";
  json_version += PSTR ("\"") + String (D_HOSTNAME) + "\":\"" + my_hostname + "\",";
  json_version += PSTR ("\"") + String (D_IP_ADDRESS) + "\":\"" + WiFi.localIP().toString() + "\",";
  json_version += PSTR ("\"") + String (D_MAC_ADDRESS) + "\":\"" + WiFi.macAddress() + "\",";
  json_version += PSTR ("\"") + String (D_MQTT_HOST) + "\":\"" + SettingsText(SET_MQTT_HOST) + "\",";
  json_version += PSTR ("\"") + String (D_MQTT_PORT) + "\":\"" + Settings.mqtt_port + "\",";
  json_version += PSTR ("\"") + String (D_MQTT_FULL_TOPIC) + "\":\"" + GetTopic_P(stopic, CMND, mqtt_topic, "") + "\"";
  json_version += PSTR ("}");

  // publish public page
  WSContentBegin(200, CT_HTML);
  WSContentSend_P (json_version.c_str ());
  WSContentEnd();
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
#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      WebServer->on ("/" D_PAGE_INFO_JSON, InfoWebPageJson);
      break;
#endif  // USE_WEBSERVER
  }
  
  return result;
}

#endif // USE_INFOJSON
