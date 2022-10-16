/*
  xdrv_97_ecowatt_client.ino - Ecowatt MQTT client (needs a ecowatt server)
  
  This module connects to french RTE EcoWatt server to retrieve electricity production forecast.
  It publishes status of current slot and next 2 slots on the MQTT stream under
  
    {"Time":"2022-10-10T23:51:09","Ecowatt":{"now":1,"next":2,
      "day0":{"jour":"2022-10-06","dvalue":1,"0":1,"1":1,"2":1,"3":1,"4":1,"5":1,"6":1,...,"23":1},
      "day1":{"jour":"2022-10-07","dvalue":2,"0":1,"1":1,"2":2,"3":1,"4":1,"5":1,"6":1,...,"23":1},
      "day2":{"jour":"2022-10-08","dvalue":3,"0":1,"1":1,"2":1,"3":1,"4":1,"5":3,"6":1,...,"23":1},
      "day3":{"jour":"2022-10-09","dvalue":2,"0":1,"1":1,"2":1,"3":2,"4":1,"5":1,"6":1,...,"23":1} }}

  Copyright (C) 2022  Nicolas Bernaerts
    06/10/2022 - v1.0 - Creation 

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

/****************************************************\
 *        EcoWatt electricity production forecast
\****************************************************/

#ifndef FIRMWARE_SAFEBOOT
#ifdef USE_ECOWATT_CLIENT

#define XDRV_97                    97

#include <ArduinoJson.h>

// constant
#define ECOWATT_SLOT_PER_DAY       24         // number of 1h slots per day

// configuration file
#define D_ECOWATT_CFG              "/ecowatt.cfg"

// commands
#define D_CMND_ECOWATT_HELP        "help"
#define D_CMND_ECOWATT_TOPIC       "topic"

// MQTT commands
const char kEcowattCommands[] PROGMEM = "eco_" "|" D_CMND_ECOWATT_HELP "|" D_CMND_ECOWATT_TOPIC;
void (* const EcowattCommand[])(void) PROGMEM = { &CmndEcowattHelp, &CmndEcowattTopic };

// ecowatt type of days
enum EcowattDays { ECOWATT_DAY_TODAY, ECOWATT_DAY_TOMORROW, ECOWATT_DAY_DAYAFTER, ECOWATT_DAY_DAYAFTER2, ECOWATT_DAY_MAX };

// Ecowatt states
enum EcowattStates { ECOWATT_LEVEL_NONE, ECOWATT_LEVEL_NORMAL, ECOWATT_LEVEL_WARNING, ECOWATT_LEVEL_POWERCUT, ECOWATT_LEVEL_MAX };
const char kEcowattStateColor[] PROGMEM = "|#00AF00|#FFAF00|#CF0000";

/***********************************************************\
 *                        Data
\***********************************************************/

// Ecowatt configuration
static struct {
  String  str_topic;                                // topic where ecowatt server publishes data
} ecowatt_config;

// Ecowatt status
struct ecowatt_day {
  uint8_t dvalue;                                   // day global status
  String  str_jour;                                 // slot date (aaaa-mm-dd)
  uint8_t arr_hvalue[ECOWATT_SLOT_PER_DAY];         // hourly slots
};
static struct {
  uint8_t hour = UINT8_MAX;                         // current active slot
  uint8_t now  = ECOWATT_LEVEL_NORMAL;              // ecowatt status of current slot
  uint8_t next = ECOWATT_LEVEL_NORMAL;              // ecowatt status of current slot
  ecowatt_day arr_day[ECOWATT_DAY_MAX];             // slots for today and tomorrow
} ecowatt_status;

/***********************************************************\
 *                      Commands
\***********************************************************/

// Ecowatt server help
void CmndEcowattHelp ()
{
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Ecowatt MQTT client commands :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - eco_topic = set topic where Ecowatt data are published"));
  ResponseCmndDone ();
}

// Ecowatt MQTT publication topic
void CmndEcowattTopic ()
{
  if (XdrvMailbox.data_len > 0)
  {
    ecowatt_config.str_topic = XdrvMailbox.data;
    AddLog (LOG_LEVEL_INFO, PSTR ("ECO: Topic set to %s"), ecowatt_config.str_topic.c_str ());
    EcowattSaveConfig ();
  }
  
  ResponseCmndChar (ecowatt_config.str_topic.c_str ());
}

/***********************************************************\
 *                      Configuration
\***********************************************************/

// load configuration
void EcowattLoadConfig () 
{
#ifdef USE_UFILESYS

  // retrieve saved settings from flash filesystem
  ecowatt_config.str_topic = UfsCfgLoadKey (D_ECOWATT_CFG, D_CMND_ECOWATT_TOPIC);

#else       // No LittleFS

  // mqtt config
  ecowatt_config.str_topic = SettingsText (SET_ECOWATT_TOPIC);

# endif     // USE_UFILESYS
}

// save configuration
void EcowattSaveConfig () 
{
#ifdef USE_UFILESYS

  // save settings into flash filesystem
  UfsCfgSaveKey (D_ECOWATT_CFG, D_CMND_ECOWATT_TOPIC, ecowatt_config.str_topic.c_str (), true);

#else       // No LittleFS

  // mqtt config
  SettingsUpdateText (SET_ECOWATT_TOPIC, ecowatt_config.str_topic.c_str ());

# endif     // USE_UFILESYS
}

/***********************************************************\
 *                      Functions
\***********************************************************/

// get level of current slot
uint8_t EcowattGetCurrentLevel () 
{
  return ecowatt_status.now;
}

// get level of next slot
uint8_t EcowattGetNextLevel () 
{
  return ecowatt_status.next;
}

/***********************************************************\
 *                      Callback
\***********************************************************/

// init main status
void EcowattInit ()
{
  uint8_t index, slot;

  // load configuration file
  EcowattLoadConfig ();

  // initialisation of slot array
  for (index = 0; index < ECOWATT_DAY_MAX; index ++)
  {
    // init date
    ecowatt_status.arr_day[index].str_jour = "";

    // slot initialisation to normal state
    for (slot = 0; slot < ECOWATT_SLOT_PER_DAY; slot ++) ecowatt_status.arr_day[index].arr_hvalue[slot] = ECOWATT_LEVEL_NORMAL;
  }

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: eco_help to get help on Ecowatt production forecast commands"));
}

// check and update MQTT ecowatt
void EcowattMqttSubscribe ()
{
  // if subsciption topic defined
  if (ecowatt_config.str_topic.length () > 0)
  {
    // subscribe to ecowatt data
    MqttSubscribe (ecowatt_config.str_topic.c_str ());
    AddLog (LOG_LEVEL_INFO, PSTR ("ECO: Subscribed to %s"), ecowatt_config.str_topic.c_str ());
  }
}

// read received MQTT data to retrieve ecowatt data
bool EcowattMqttData ()
{
  bool    is_ecowatt;
  uint8_t index, slot, value;
  char    str_day[8];
  char    str_value[4];
  DynamicJsonDocument json_result(2048);

  // if topic is the right one and JSON stream exists
  is_ecowatt = (strcmp (ecowatt_config.str_topic.c_str (), XdrvMailbox.topic) == 0);

  // check if stream contains Ecowatt section
  if (is_ecowatt) is_ecowatt = (strstr (XdrvMailbox.data, "\"Ecowatt\":{") != nullptr);

  // if Ecowatt section is present
  if (is_ecowatt)
  {
    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("ECO: Received %s"), XdrvMailbox.topic);

    // extract token from JSON
    deserializeJson (json_result, XdrvMailbox.data);

    // get current and next slot
    ecowatt_status.hour = json_result["Ecowatt"]["hour"];
    ecowatt_status.now  = json_result["Ecowatt"]["now"];
    ecowatt_status.next = json_result["Ecowatt"]["next"];

    // loop thru all 4 days to get their data
    for (index = 0; index < ECOWATT_DAY_MAX; index ++)
    {
      // get day string for current day
      sprintf (str_day, "day%u", index);
      ecowatt_status.arr_day[index].str_jour = json_result["ecowatt"][str_day]["jour"].as<String> ();
      ecowatt_status.arr_day[index].dvalue   = json_result["ecowatt"][str_day]["dval"];

      // loop to populate the slots
      for (slot = 0; slot < ECOWATT_SLOT_PER_DAY; slot ++)
      {
        sprintf (str_value, "%u", slot);
        value = json_result["ecowatt"][str_day][str_value];
        if ((value == ECOWATT_LEVEL_NONE) || (value >= ECOWATT_LEVEL_MAX)) value = ECOWATT_LEVEL_NORMAL;
        ecowatt_status.arr_day[index].arr_hvalue[slot] = value;
      }
    }
  }

  return is_ecowatt;
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// append presence forecast to main page
void EcowattWebSensor ()
{
  uint8_t slot;
  char    str_color[12];
  char    str_select[48];

  // if ecowatt signal has been received
  if (ecowatt_status.hour != UINT8_MAX)
  {
    // start of graph
    WSContentSend_P (PSTR ("<tr>\n"));

    // graph header
    WSContentSend_P (PSTR ("<div style='display:flex;margin:0px;padding:0px;font-size:12px;'>\n"));
    WSContentSend_P (PSTR ("<div style='width:50%%;text-align:left;'>%uh</div>\n"), 0);
    WSContentSend_P (PSTR ("<div style='width:100%%;text-align:center;'>%uh</div>\n"), 4);
    WSContentSend_P (PSTR ("<div style='width:100%%;text-align:center;'>%uh</div>\n"), 8);
    WSContentSend_P (PSTR ("<div style='width:100%%;text-align:center;'>%uh</div>\n"), 12);
    WSContentSend_P (PSTR ("<div style='width:100%%;text-align:center;'>%uh</div>\n"), 16);
    WSContentSend_P (PSTR ("<div style='width:100%%;text-align:center;'>%uh</div>\n"), 20);
    WSContentSend_P (PSTR ("<div style='width:50%%;text-align:right;'>%uh</div>\n"), 24);
    WSContentSend_P (PSTR ("</div>\n"));

    // display slots of today
    WSContentSend_P (PSTR ("<div style='display:flex;margin:0px;padding:2px 0px;height:16px;'>\n"));
    for (slot = 0; slot < ECOWATT_SLOT_PER_DAY; slot ++)
    {
      // get segment color
      GetTextIndexed (str_color, sizeof (str_color), ecowatt_status.arr_day[0].arr_hvalue[slot], kEcowattStateColor);

      // check if segment is the current slot
      if (slot == ecowatt_status.hour) strcpy (str_select, "outline:4px solid white;outline-offset:-8px;"); else strcpy (str_select, "");

      // display segment
      if (slot == 0) WSContentSend_P (PSTR ("<div style='width:100%%;margin-right:1px;border-radius:2px;background-color:%s;border-top-left-radius:6px;border-bottom-left-radius:6px;%s'></div>\n"), str_color, str_select);
      else if (slot == ECOWATT_SLOT_PER_DAY - 1) WSContentSend_P (PSTR ("<div style='width:100%%;margin-right:1px;border-radius:2px;background-color:%s;border-top-right-radius:6px;border-bottom-right-radius:6px;%s'></div>\n"), str_color, str_select);
      else WSContentSend_P (PSTR ("<div style='width:100%%;margin-right:1px;border-radius:2px;background-color:%s;%s'></div>\n"), str_color, str_select);
    }
    WSContentSend_P (PSTR ("</div>\n"));

    // end of graph
    WSContentSend_P (PSTR ("</tr>\n"));
  }
}

#endif  // USE_WEBSERVER

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xdrv97 (uint8_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
    case FUNC_INIT:
      EcowattInit ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kEcowattCommands, EcowattCommand);
      break;
    case FUNC_MQTT_SUBSCRIBE:
      EcowattMqttSubscribe ();
      break;
    case FUNC_MQTT_DATA:
      result = EcowattMqttData ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      EcowattWebSensor ();
      break;
#endif  // USE_WEBSERVER
  }
  
  return result;
}

#endif    // USE_ECOWATT_CLIENT
#endif    // FIRMWARE_SAFEBOOT
