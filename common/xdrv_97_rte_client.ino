/*
  xdrv_97_rte_client.ino - RTE MQTT client to handle Tempo and Ecowatt (needs a RTE server)
  
  This module is a client of the french RTE Tempo and EcoWatt server.

  It subscribes to MQTT topic from the server and display current slot for Tempo and Ecowatt.

  Settings are stored in :
    - Settings->rf_code[6][0]     : tempo signal enabled
    - Settings->rf_code[6][1]     : pointe signal enabled
    - Settings->rf_code[6][2]     : ecowatt signal enabled
    - Settings text SET_RTE_TOPIC : RTE server topic

  Copyright (C) 2022  Nicolas Bernaerts
    06/10/2022 - v1.0 - Creation 
    27/12/2023 - v2.0 - Add Tempo and Pointe signals 

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

/*********************************************************\
 *         RTE electricity production forecast
 *              Tempo, Pointe & Ecowatt
\*********************************************************/

#ifdef USE_RTE_CLIENT

#define XDRV_97                    97

#include <ArduinoJson.h>

// commands
#define D_CMND_RTE_HELP            "help"
#define D_CMND_RTE_TOPIC           "topic"
#define D_CMND_RTE_TEMPO           "tempo"
#define D_CMND_RTE_POINTE          "pointe"
#define D_CMND_RTE_ECOWATT         "ecowatt"

// MQTT commands
const char kRteCommands[] PROGMEM = "rte_" "|" D_CMND_RTE_HELP "|" D_CMND_RTE_TOPIC "|" D_CMND_RTE_TEMPO "|" D_CMND_RTE_POINTE "|" D_CMND_RTE_ECOWATT ;
void (* const RteCommand[])(void) PROGMEM = { &CmndRteHelp, &CmndRteTopic, &CmndRteTempo, &CmndRtePointe, &CmndRteEcowatt };

// tempo status
enum TempoColor  { TEMPO_COLOR_UNKNOWN, BLUE, TEMPO_COLOR_BLUE, TEMPO_COLOR_WHITE, TEMPO_COLOR_RED, TEMPO_COLOR_MAX };
const char kRteTempoStateColor[] PROGMEM = "#252525|#06b|#ccc|#b00";
const char kRteTempoStateDot[]   PROGMEM = "⚪|⚪|⚫|⚪";

// pointe status
enum PointeColor  { POINTE_COLOR_UNKNOWN, POINTE_COLOR_BLUE, POINTE_COLOR_RED, POINTE_COLOR_MAX };
const char kRtePointeStateColor[] PROGMEM = "#252525|#06b|#b00";

// ecowatt status
enum EcowattStates { ECOWATT_LEVEL_CARBONFREE, ECOWATT_LEVEL_NORMAL, ECOWATT_LEVEL_WARNING, ECOWATT_LEVEL_POWERCUT, ECOWATT_LEVEL_MAX };
const char kRteEcowattStateColor[] PROGMEM = "#0a0|#080|#e80|#d00|#252525";

/***********************************************************\
 *                        Data
\***********************************************************/

// RTE configuration
static struct {
  uint8_t tempo   = 1;                              // tempo client enabling flag
  uint8_t pointe  = 1;                              // pointe client enabling flag
  uint8_t ecowatt = 1;                              // ecowatt client enabling flag
  String  str_topic;                                // topic where ecowatt server publishes data
} rte_config;

// tempo status
static struct {
  uint8_t updated   = 0;                            // tempo today signal has been updated
  uint8_t yesterday = TEMPO_COLOR_MAX;              // tempo yesterday color
  uint8_t today     = TEMPO_COLOR_MAX;              // tempo today color
  uint8_t tomorrow  = TEMPO_COLOR_MAX;              // tempo tomorrow color
} rte_tempo;

// pointe status
static struct {
  uint8_t updated   = 0;                            // pointe today signal has been updated
  uint8_t today     = POINTE_COLOR_MAX;             // pointe today color
  uint8_t tomorrow  = POINTE_COLOR_MAX;             // pointe tomorrow color
} rte_pointe;

// ecowatt status
static struct {
  uint8_t updated = 0;                              // ecowatt now signal has been updated
  uint8_t now     = ECOWATT_LEVEL_MAX;              // ecowatt status of current slot
  uint8_t next    = ECOWATT_LEVEL_MAX;              // ecowatt status of current slot
  uint8_t dvalue  = ECOWATT_LEVEL_MAX;              // day global status
  uint8_t arr_hvalue[24];                           // hourly slots
  String  str_jour;                                 // slot date (aaaa-mm-dd)
} rte_ecowatt;

/***********************************************************\
 *                      Commands
\***********************************************************/

// Ecowatt server help
void CmndRteHelp ()
{
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: RTE MQTT client commands"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - rte_topic <topic>  = RTE data topic"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - rte_tempo <0/1>    = enable tempo client"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - rte_pointe <0/1>   = enable pointe client"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - rte_ecowatt <0/1>  = enable ecowatt client"));
  ResponseCmndDone ();
}


// RTE publication topic
void CmndRteTopic ()
{
  if (XdrvMailbox.data_len > 0)
  {
    rte_config.str_topic = XdrvMailbox.data;
    RteSaveConfig ();
  }
  
  ResponseCmndChar (rte_config.str_topic.c_str ());
}

// Tempo enable command
void CmndRteTempo ()
{
  if (XdrvMailbox.data_len > 0) RteTempoEnable ((XdrvMailbox.payload != 0));
  ResponseCmndNumber (rte_config.tempo);
}

// Pointe enable command
void CmndRtePointe ()
{
  if (XdrvMailbox.data_len > 0) RtePointeEnable ((XdrvMailbox.payload != 0));
  ResponseCmndNumber (rte_config.pointe);
}

// Ecowatt enable command
void CmndRteEcowatt ()
{
  if (XdrvMailbox.data_len > 0) RteEcowattEnable ((XdrvMailbox.payload != 0)); 
  ResponseCmndNumber (rte_config.ecowatt);
}
/***********************************************************\
 *                      Configuration
\***********************************************************/

// load configuration
void RteLoadConfig () 
{
  
  rte_config.tempo     = Settings->rf_code[6][0];
  rte_config.pointe    = Settings->rf_code[6][1];
  rte_config.ecowatt   = Settings->rf_code[6][2];
  rte_config.str_topic = SettingsText (SET_RTE_TOPIC);
}

// save configuration
void RteSaveConfig () 
{
  Settings->rf_code[6][0] = rte_config.tempo;
  Settings->rf_code[6][1] = rte_config.pointe;
  Settings->rf_code[6][2] = rte_config.ecowatt;
  SettingsUpdateText (SET_RTE_TOPIC, rte_config.str_topic.c_str ());
}

/*******************************************\
 *          Tempo, Pointe and Ecowatt
\*******************************************/

// Set Tempo management flag
void RteTempoEnable (const bool status)
{
  if (status != rte_config.tempo)
  {
    rte_config.tempo = status;
    RteSaveConfig ();
  }
}

// get current tempo level
uint8_t RteTempoGetLevel () 
{
  uint8_t level = TEMPO_COLOR_MAX;

  // check time
  if (RtcTime.valid && rte_config.tempo && rte_tempo.updated)
  {
    if (RtcTime.hour < 6) level = rte_tempo.yesterday;
      else level = rte_tempo.today;
  }

  return level;
}

// Set Pointe management flag
void RtePointeEnable (const bool status)
{
  if (status != rte_config.pointe)
  {
    rte_config.pointe = status;
    RteSaveConfig ();
  }
}

// get current pointe level
uint8_t RtePointeGetLevel () 
{
  uint8_t level = POINTE_COLOR_MAX;

  // check time
  if (RtcTime.valid && rte_config.pointe && rte_pointe.updated) level = rte_pointe.today;

  return level;
}

// Set Ecowatt management flag
void RteEcowattEnable (const bool status)
{
  if (status != rte_config.ecowatt)
  {
    rte_config.ecowatt = status;
    RteSaveConfig ();
  }
}

// get current ecowatt level
uint8_t RteEcowattGetLevel () 
{
  uint8_t level = ECOWATT_LEVEL_MAX;

  if (RtcTime.valid && rte_config.ecowatt && rte_ecowatt.updated) level = rte_ecowatt.now;

  return level;
}

// Reset last Ecowatt update flag
void RteResetAlert ()
{
  rte_tempo.updated = 0;
  rte_pointe.updated = 0;
  rte_ecowatt.updated = 0;
}

/***********************************************************\
 *                      Callback
\***********************************************************/

// init main status
void RteInit ()
{
  uint8_t slot;

  // load configuration file
  RteLoadConfig ();

  // initialisation of slot array
  rte_ecowatt.str_jour = "";
  for (slot = 0; slot < 24; slot ++) rte_ecowatt.arr_hvalue[slot] = ECOWATT_LEVEL_MAX;

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: rte_help to get help on RTE client commands"));
}

// check and update MQTT ecowatt
void RteMqttSubscribe ()
{
  // if subsciption topic defined
  if (rte_config.str_topic.length () > 0)
  {
    // subscribe to ecowatt data
    MqttSubscribe (rte_config.str_topic.c_str ());
    AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Subscribed to %s"), rte_config.str_topic.c_str ());
  }
}

// read received MQTT data to retrieve RTE data
bool RteMqttData ()
{
  bool    is_found;
  uint8_t index, slot, value;
  char    str_value[4];
  DynamicJsonDocument json_result(2048);
  JsonVariant         json_section;

  // check for ECOWATT in data
  is_found  = (strstr (XdrvMailbox.data, "\"TEMPO\":") != nullptr);
  if (!is_found) is_found = (strstr (XdrvMailbox.data, "\"POINTE\":" ) != nullptr);
  if (!is_found) is_found = (strstr (XdrvMailbox.data, "\"ECOWATT\":") != nullptr);
  if (is_found)
  {
    // extract token from JSON
    deserializeJson (json_result, (const char*)XdrvMailbox.data);

    // look for TEMPO section
    json_section = json_result["TEMPO"].as<JsonVariant>();
    if (!json_section.isNull ())
    {
      // check for update
      value = json_section["Aujour"];
      if (rte_tempo.today != value) rte_tempo.updated = 1;

      // get slots and log
      rte_tempo.yesterday = json_section["Hier"];
      rte_tempo.today     = json_section["Aujour"];
      rte_tempo.tomorrow  = json_section["Demain"];
      AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Tempo is %u/%u/%u"), rte_tempo.yesterday, rte_tempo.today, rte_tempo.tomorrow);
    }

    // look for POINTE section
    json_section = json_result["POINTE"].as<JsonVariant>();
    if (!json_section.isNull ())
    {
      // check for update
      value = json_section["Aujour"];
      if (rte_pointe.today != value) rte_pointe.updated = 1;

      // get slots and log
      rte_pointe.today    = json_section["Aujour"];
      rte_pointe.tomorrow = json_section["Demain"];
      AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Pointe is %u/%u"), rte_pointe.today, rte_pointe.tomorrow);
    }

    // look for ECOWATT section
    json_section = json_result["ECOWATT"].as<JsonVariant>();
    if (!json_section.isNull ())
    {
      // check for update
      value = json_section["now"];
      if (rte_ecowatt.now != value) rte_ecowatt.updated = 1;

      // get current and next slot
      rte_ecowatt.now  = json_section["now"];
      rte_ecowatt.next = json_section["next"];

      // get day string for current day
      rte_ecowatt.str_jour = json_section["day0"]["jour"].as<String> ();
      rte_ecowatt.dvalue   = json_section["day0"]["dval"];

      // loop to populate the slots
      for (slot = 0; slot < 24; slot ++)
      {
        sprintf (str_value, "%u", slot);
        value = json_section["day0"][str_value];
        if (value >= ECOWATT_LEVEL_MAX) value = ECOWATT_LEVEL_NORMAL;
        rte_ecowatt.arr_hvalue[slot] = value;
      }
      
      // log
      AddLog (LOG_LEVEL_INFO, PSTR ("RTE: Ecowatt is %u/%u"), rte_ecowatt.now, rte_ecowatt.next);
    }
  }

  return is_found;
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// append RTE data to main page
void RteWebSensor ()
{
  uint8_t slot, day;
  char    str_text[8];
  char    str_color[12];

  // check if data are defined
  if ((rte_config.tempo == 0) && (rte_config.ecowatt == 0)) return;

  // start of RTE section
  WSContentSend_P (PSTR ("<div style='font-size:10px;text-align:center;margin:4px 0px;padding:2px 6px;background:#333333;border-radius:8px;'>\n"));

  WSContentSend_P (PSTR ("<div style='display:flex;padding:0px;margin-bottom:4px;'>\n"));
  WSContentSend_P (PSTR ("<div style='width:20%%;padding:0px;text-align:left;font-size:14px;font-weight:bold;'>RTE</div>\n"));
  WSContentSend_P (PSTR ("<div style='width:80%%;padding:4px 0px 0px 0px;display:flex;'>\n"));
  WSContentSend_P (PSTR ("<div style='width:8.2%%;padding:0px;text-align:left;'>%uh</div>\n"), 0);
  for (slot = 1; slot < 6; slot ++) WSContentSend_P (PSTR ("<div style='width:16.3%%;padding:0px;'>%uh</div>\n"), slot * 4);
  WSContentSend_P (PSTR ("<div style='width:8.3%%;padding:0px;text-align:right;'>%uh</div>\n"), 24);
  WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div>\n"));
  WSContentSend_P (PSTR ("</div>\n"));
  WSContentSend_P (PSTR ("</div>\n"));

  //    tempo
  // -----------

  if (rte_config.tempo != 0)
  {
    // tempo data
    WSContentSend_P (PSTR ("<div style='display:flex;padding:0px;margin-bottom:4px;'>\n"));
    WSContentSend_P (PSTR ("<div style='width:20%%;padding:0px;text-align:left;font-size:12px;'>Tempo</div>\n"));
    WSContentSend_P (PSTR ("<div style='width:80%%;padding:0px;display:flex;font-size:9px;'>\n"));
    for (slot = 0; slot < 24; slot ++)
    {
          // calculate color of current slot
          if (slot < 6) day = rte_tempo.yesterday;
            else day = rte_tempo.today;
          GetTextIndexed (str_color, sizeof (str_color), day, kRteTempoStateColor);  

          // segment beginning
          WSContentSend_P (PSTR ("<div style='width:4%%;padding:1px 0px;background-color:%s;"), str_color);

          // set opacity for HC
          if ((slot < 6) || (slot >= 22)) WSContentSend_P (PSTR ("opacity:75%%;"));

          // first and last segment specificities
          if (slot == 0) WSContentSend_P (PSTR ("border-top-left-radius:6px;border-bottom-left-radius:6px;"));
            else if (slot == 23) WSContentSend_P (PSTR ("border-top-right-radius:6px;border-bottom-right-radius:6px;"));

          // current hour dot and hourly segment end
          if (slot == RtcTime.hour)
          {
            GetTextIndexed (str_text, sizeof (str_text), day, kRteTempoStateDot);  
            WSContentSend_P (PSTR ("font-size:9px;'>%s</div>\n"), str_text);
          }
          else WSContentSend_P (PSTR ("'></div>\n"));
    }
    WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div>\n"));
    WSContentSend_P (PSTR ("</div>\n"));
    WSContentSend_P (PSTR ("</div>\n"));
  }

  //   ecowatt
  // -----------

  if (rte_config.ecowatt != 0)
  {
    // ecowatt data
    WSContentSend_P (PSTR ("<div style='display:flex;padding:0px;margin-bottom:4px;'>\n"));
    WSContentSend_P (PSTR ("<div style='width:20%%;padding:0px;text-align:left;font-size:12px;'>Ecowatt</div>\n"));
    WSContentSend_P (PSTR ("<div style='width:80%%;padding:0px;display:flex;font-size:9px;'>\n"));
    for (slot = 0; slot < 24; slot ++)
    {
      if (slot == RtcTime.hour) strcpy (str_text, "⚪"); else strcpy (str_text, "");
      GetTextIndexed (str_color, sizeof (str_color), rte_ecowatt.arr_hvalue[slot], kRteEcowattStateColor);
      WSContentSend_P (PSTR ("<div style='width:4%%;padding:0px;background:%s;"), str_color);
      if (slot == 0) WSContentSend_P (PSTR ("border-top-left-radius:6px;border-bottom-left-radius:6px;"));
        else if (slot == 23) WSContentSend_P (PSTR ("border-top-right-radius:6px;border-bottom-right-radius:6px;"));
      WSContentSend_P (PSTR ("'>%s</div>\n"), str_text);
    }
    WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div>\n"));
    WSContentSend_P (PSTR ("</div>\n"));
    WSContentSend_P (PSTR ("</div>\n"));
  }

  // end of RTE section
  WSContentSend_P (PSTR ("</div>\n"));
}

#endif  // USE_WEBSERVER

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xdrv97 (uint32_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
    case FUNC_INIT:
      RteInit ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kRteCommands, RteCommand);
      break;
    case FUNC_MQTT_SUBSCRIBE:
      RteMqttSubscribe ();
      break;
    case FUNC_MQTT_DATA:
      result = RteMqttData ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      RteWebSensor ();
      break;
#endif  // USE_WEBSERVER
  }
  
  return result;
}

#endif    // USE_RTE_CLIENT
