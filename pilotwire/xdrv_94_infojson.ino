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

#define XDRV_94             94

#define D_PAGE_INFO_JSON    "info.json"

#define D_JSON_INFO_IP      "IP"

/**************************************************\
 *                  Accessors
\**************************************************/

// get configuration string based on an index
String InfoGetConfig (int index)
{
  String str_result;

  switch (index)
  { 
    case 0:
      str_result = Settings.ex_mqtt_client;
      break;
    case 1:
      str_result = Settings.ex_mqtt_user;
      break;
    case 2:
      str_result = Settings.ex_mqtt_pwd;
      break;
    case 3:
      str_result = Settings.ex_mqtt_topic;
      break;
    case 4:
      str_result = Settings.ex_button_topic;
      break;
    case 5:
      str_result = Settings.ex_mqtt_grptopic;
      break;
  }

  return str_result;
}

// set configuration string based on an index
bool InfoSetConfig (int index, char* str_config)
{
  bool result = true;

  switch (index)
  { 
    case 0:
      strncpy ((char*)Settings.ex_mqtt_client, str_config, 32);
      break;
    case 1:
      strncpy ((char*)Settings.ex_mqtt_user, str_config, 32);
      break;
    case 2:
      strncpy ((char*)Settings.ex_mqtt_pwd, str_config, 32);
      break;
    case 3:
      strncpy ((char*)Settings.ex_mqtt_topic, str_config, 32);
      break;
    case 4:
      strncpy ((char*)Settings.ex_button_topic, str_config, 32);
      break;
    case 5:
      strncpy ((char*)Settings.ex_mqtt_grptopic, str_config, 32);
      break;
    default:
      result = false;
      break;
  }

  return result;
}

/**************************************************\
 *                  Functions
\**************************************************/

// Show JSON status (for MQTT)
void InfoShowJSON ()
{
  String str_mqtt;

  // append to MQTT message
  str_mqtt = String (mqtt_data) + ",\"" + String (D_JSON_INFO_IP) + "\":\"" + WiFi.localIP().toString() + "\"";

  // place JSON back to MQTT data
  snprintf_P (mqtt_data, sizeof(mqtt_data), str_mqtt.c_str ());
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
    case FUNC_JSON_APPEND:
      InfoShowJSON ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      WebServer->on ("/" D_PAGE_INFO_JSON, InfoWebPageJson);
      break;
#endif  // USE_WEBSERVER
  }
  
  return result;
}

#endif // USE_INFOJSON
