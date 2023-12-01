/*
  xdrv_97_ecowatt_client.ino - Ecowatt MQTT client (needs a ecowatt server)
  
  This module is a client of the french RTE EcoWatt server available under ecowatt project.

  It subscribes to MQTT topic from the server and make current and next slot availables.

  Settings are stored in :
    - Settings->sbflag1.spare31 : module enabled
    - Settings text SET_ECOWATT_TOPIC : ecowatt server topic

  It also add today's ecowatt slots on the main page.

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

#ifdef USE_ECOWATT_CLIENT

#define XDRV_97                    97

#include <ArduinoJson.h>

// constant
#define ECOWATT_SLOT_PER_DAY       24         // number of 1h slots per day

// configuration file
#define D_ECOWATT_CFG              "/ecowatt.cfg"

// commands
#define D_CMND_ECOWATT_HELP        "help"
#define D_CMND_ECOWATT_ENABLE      "enable"
#define D_CMND_ECOWATT_TOPIC       "topic"

// MQTT commands
const char kEcowattCommands[] PROGMEM = "eco_" "|" D_CMND_ECOWATT_HELP "|" D_CMND_ECOWATT_ENABLE "|" D_CMND_ECOWATT_TOPIC;
void (* const EcowattCommand[])(void) PROGMEM = { &CmndEcowattHelp, &CmndEcowattEnable, &CmndEcowattTopic };

// ecowatt type of days
enum EcowattDays { ECOWATT_DAY_TODAY, ECOWATT_DAY_TOMORROW, ECOWATT_DAY_DAYAFTER, ECOWATT_DAY_DAYAFTER2, ECOWATT_DAY_MAX };

// Ecowatt states
enum EcowattStates { ECOWATT_LEVEL_NONE, ECOWATT_LEVEL_NORMAL, ECOWATT_LEVEL_WARNING, ECOWATT_LEVEL_POWERCUT, ECOWATT_LEVEL_MAX };
const char kEcowattStateColor[] PROGMEM = "|#080|#E80|#D00";

/***********************************************************\
 *                        Data
\***********************************************************/

// Ecowatt configuration
static struct {
  uint8_t enabled = 1;                              // ecowatt client enabling flag
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
  AddLog (LOG_LEVEL_INFO, PSTR (" - eco_enable <0/1>  = enable ecowatt client"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - eco_topic <topic> = Ecowatt topic"));
  ResponseCmndDone ();
}

// Ecowatt MQTT publication topic
void CmndEcowattEnable ()
{
  if (XdrvMailbox.data_len > 0)
  {
    if (XdrvMailbox.payload != 0) ecowatt_config.enabled = 1;
      else ecowatt_config.enabled = 0; 
    EcowattSaveConfig ();
  }
  
  ResponseCmndNumber (ecowatt_config.enabled);
}

// Ecowatt MQTT publication topic
void CmndEcowattTopic ()
{
  if (XdrvMailbox.data_len > 0)
  {
    ecowatt_config.str_topic = XdrvMailbox.data;
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
  ecowatt_config.enabled = Settings->sbflag1.spare31;
  ecowatt_config.str_topic = SettingsText (SET_ECOWATT_TOPIC);
}

// save configuration
void EcowattSaveConfig () 
{
  Settings->sbflag1.spare31 = ecowatt_config.enabled;
  SettingsUpdateText (SET_ECOWATT_TOPIC, ecowatt_config.str_topic.c_str ());
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
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: eco_help to get help on Ecowatt client commands"));
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

  // check for topic and Ecowatt section
  is_ecowatt = (strcmp (ecowatt_config.str_topic.c_str (), XdrvMailbox.topic) == 0);
  if (is_ecowatt) 
  {
    // log
    AddLog (LOG_LEVEL_DEBUG, PSTR ("ECO: Received %s"), XdrvMailbox.topic);

    // if section Ecowatt is present
    if (strstr (XdrvMailbox.data, "\"Ecowatt\":{") != nullptr)
    {
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
        ecowatt_status.arr_day[index].str_jour = json_result["Ecowatt"][str_day]["jour"].as<String> ();
        ecowatt_status.arr_day[index].dvalue   = json_result["Ecowatt"][str_day]["dval"];

        // loop to populate the slots
        for (slot = 0; slot < ECOWATT_SLOT_PER_DAY; slot ++)
        {
          sprintf (str_value, "%u", slot);
          value = json_result["Ecowatt"][str_day][str_value];
          if ((value == ECOWATT_LEVEL_NONE) || (value >= ECOWATT_LEVEL_MAX)) value = ECOWATT_LEVEL_NORMAL;
          ecowatt_status.arr_day[index].arr_hvalue[slot] = value;
        }
      }

      // log
      AddLog (LOG_LEVEL_INFO, PSTR ("ECO: Now is %u, next is %u"), ecowatt_status.now, ecowatt_status.next);
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
  char    str_text[8];
  char    str_color[12];

  // if topic is missing, nothing to display
  if (ecowatt_config.str_topic.length () == 0) return;

  WSContentSend_P (PSTR ("<div style='font-size:10px;text-align:center;margin:4px 0px;padding:2px 6px;background:#333333;border-radius:8px;'>\n"));

  // ecowatt header
  WSContentSend_P (PSTR ("<div style='display:flex;padding:0px;margin-bottom:4px;'>\n"));
  WSContentSend_P (PSTR ("<div style='width:28%%;padding:0px;margin-bottom:-4px;text-align:left;font-size:16px;font-weight:bold;'>Ecowatt</div>\n"));
  if (ecowatt_status.hour == UINT8_MAX) WSContentSend_P (PSTR ("<div style='width:72%%;padding:0px;'>Waiting for %s</div>\n"), ecowatt_config.str_topic.c_str ());
    else WSContentSend_P (PSTR ("<div style='width:72%%;padding:0px;'></div>\n"));
  WSContentSend_P (PSTR ("</div>\n"));

  // ecowatt chart
  WSContentSend_P (PSTR ("<div style='display:flex;padding:0px;'>\n"));
  WSContentSend_P (PSTR ("<div style='width:15%%;padding:0px;'></div>\n"));
  WSContentSend_P (PSTR ("<div style='width:85%%;padding:0px;display:flex;font-size:9px;'>\n"));
  for (slot = 0; slot < ECOWATT_SLOT_PER_DAY; slot ++)
  {
    GetTextIndexed (str_color, sizeof (str_color), ecowatt_status.arr_day[0].arr_hvalue[slot], kEcowattStateColor);
    WSContentSend_P (PSTR ("<div style='width:4%%;padding:0px;background:%s;"), str_color);
    if (slot == 0) WSContentSend_P (PSTR ("border-top-left-radius:4px;border-bottom-left-radius:4px;"));
      else if (slot == ECOWATT_SLOT_PER_DAY - 1) WSContentSend_P (PSTR ("border-top-right-radius:4px;border-bottom-right-radius:4px;"));
    WSContentSend_P (PSTR ("'>"));
    if (slot == ecowatt_status.hour) WSContentSend_P (PSTR ("⚪"));
    WSContentSend_P (PSTR ("</div>\n"));
  }
  WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div>\n"));
  WSContentSend_P (PSTR ("</div>\n"));
  WSContentSend_P (PSTR ("</div>\n"));

  // hour scale
  WSContentSend_P (PSTR ("<div style='display:flex;padding:0px;'>\n"));
  WSContentSend_P (PSTR ("<div style='width:15%%;padding:0px;'></div>\n"));
  WSContentSend_P (PSTR ("<div style='width:85%%;padding:0px;display:flex;'>\n"));
  WSContentSend_P (PSTR ("<div style='width:8.2%%;padding:0px;text-align:left;'>%uh</div>\n"), 0);
  for (slot = 1; slot < 6; slot ++) WSContentSend_P (PSTR ("<div style='width:16.3%%;padding:0px;'>%uh</div>\n"), slot * 4);
  WSContentSend_P (PSTR ("<div style='width:8.3%%;padding:0px;text-align:right;'>%uh</div>\n"), 24);
  WSContentSend_P (PSTR ("<div style='width:2%%;padding:0px;'></div>\n"));
  WSContentSend_P (PSTR ("</div>\n"));
  WSContentSend_P (PSTR ("</div>\n"));

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
      EcowattInit ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kEcowattCommands, EcowattCommand);
      break;
    case FUNC_MQTT_SUBSCRIBE:
      if (ecowatt_config.enabled) EcowattMqttSubscribe ();
      break;
    case FUNC_MQTT_DATA:
      if (ecowatt_config.enabled) result = EcowattMqttData ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      if (ecowatt_config.enabled) EcowattWebSensor ();
      break;
#endif  // USE_WEBSERVER
  }
  
  return result;
}

#endif    // USE_ECOWATT_CLIENT
