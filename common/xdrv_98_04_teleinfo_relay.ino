/*
  xdrv_97_02_relay_teleinfo.ino - Virtual relay management from Linky meter
  
  Copyright (C) 2024-2025  Nicolas Bernaerts
    10/08/2024 v1.0 - Creation
    17/08/2024 v1.1 - Add production power trigger
    07/09/2025 v1.2 - Redesign of main page
    19/09/2025 v1.3 - Hide and Show with click on main page display

  Settings are stored using :
    - Settings->rf_code[13][0..7] = Assocation of virtual relay R1..R7 to local relays
    - Settings->rf_code[14][0..7] = Assocation of periods P1..P8 to local relays
    - Settings->rf_code[15][0]    = Assocation of production relay to local relays

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
 *           Teleinfo Virtual Relay
\*************************************************/

#ifdef USE_TELEINFO_RELAY

#include <ArduinoJson.h>

#define RELAY_PRESENT_MAX                 8
#define RELAY_VIRTUAL_MAX                 8
#define RELAY_PERIOD_MAX                  8
#define RELAY_PROD_MAX                    4

#define D_PAGE_TELEINFO_RELAY_CONFIG      "relay"

#define D_CMND_TELEINFO_RELAY_TOPIC       "topic"
#define D_CMND_TELEINFO_RELAY_DELTA       "delta"

// offloading commands
const char kTeleinfoRelayCommands[]         PROGMEM = "relai|"               "|"       "_topic"       "|"        "_virtuel"      "|"        "_periode"     "|"       "_prod";
void (* const TeleinfoRelayCommand[])(void) PROGMEM = { &CmndTeleinfoRelayHelp, &CmndTeleinfoRelayTopic, &CmndTeleinfoRelayVirtual, &CmndTeleinfoRelayPeriod, &CmndTeleinfoRelayProd };

// relay commands
enum TeleinfoRelayStatus { TELEINFO_RELAY_UNDEF, TELEINFO_RELAY_OFF, TELEINFO_RELAY_ON, TELEINFO_RELAY_MAX };

// local relay sign
const char kTeleinfoRelayLocalSign[]         PROGMEM = "❌|1|2|3|4|5|6|7|8";

/*****************************************\
 *               Variables
\*****************************************/

// relay status
typedef union {                     // restricted by MISRA-C Rule 18.4 but so useful...
  uint8_t data;
  struct {
    uint8_t status : 1;             // relay status
    uint8_t hchp   : 1;             // hc / hp status (for period relay)
    uint8_t level  : 3;             // level (for period relay)
    uint8_t spare  : 3;             // unused
  };
} relay_state;

// relay data
struct tic_relay {
  relay_state state;               // relay status
  uint8_t     relay;               // physical relay associated to period
  String      str_name;            // relay/period name
};

struct {
  bool      prod_enabled    = false;              // production reception flag
  bool      period_enabled  = false;              // conso periods reception flag
  bool      virtual_enabled = false;              // virtual relay reception flag
  bool      mqtt_enabled    = false;              // MQTT data reception flag
  tic_relay prod_relay;                           // teleinfo production relay 
  tic_relay arr_period[RELAY_PERIOD_MAX];         // teleinfo period relays 
  tic_relay arr_virtual[RELAY_VIRTUAL_MAX];       // teleinfo virtual relays 
  String    str_topic;                            // mqtt topic to be used for meter
} teleinfo_relay;

/**************************************************\
 *                  Accessors
\**************************************************/

// load config parameters
void TeleinfoRelayLoadConfig ()
{
  uint8_t index;

#ifndef USE_TELEINFO
  // mqtt config
  teleinfo_relay.str_topic = SettingsText (SET_RELAY_LINKY_TOPIC);
#endif    // USE_TELEINFO

  // association between virtual relays and physical relays
  for (index = 0; index < RELAY_VIRTUAL_MAX; index ++)
  {
    teleinfo_relay.arr_virtual[index].relay = Settings->rf_code[13][index];
    if (teleinfo_relay.arr_virtual[index].relay > 8) teleinfo_relay.arr_virtual[index].relay = 0;
  }

  // association between periods and physical relays
  for (index = 0; index < RELAY_PERIOD_MAX; index ++)
  {
    teleinfo_relay.arr_period[index].relay = Settings->rf_code[14][index];
    if (teleinfo_relay.arr_period[index].relay > 8) teleinfo_relay.arr_period[index].relay = 0;
  } 

  // association between production relay and physical relay
  teleinfo_relay.prod_relay.relay = Settings->rf_code[15][0];
  if (teleinfo_relay.prod_relay.relay > 8) teleinfo_relay.prod_relay.relay = 0;

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("REL: Loaded linky relay association"));
}

// save config parameters
void TeleinfoRelaySaveConfig ()
{
  uint8_t index;

#ifndef USE_TELEINFO
  // mqtt config
  SettingsUpdateText (SET_RELAY_LINKY_TOPIC, teleinfo_relay.str_topic.c_str ());
#endif    // USE_TELEINFO

  // association between virtual relays and physical relays
  for (index = 0; index < RELAY_VIRTUAL_MAX; index ++) Settings->rf_code[13][index] = teleinfo_relay.arr_virtual[index].relay;

  // association between contract period relays and physical relays
  for (index = 0; index < RELAY_PERIOD_MAX; index ++) Settings->rf_code[14][index] = teleinfo_relay.arr_period[index].relay;

  // association between production relay and physical relays
  Settings->rf_code[15][0] = teleinfo_relay.prod_relay.relay;

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("REL: saved linky relay association"));
}

/**************************************************\
 *                  Commands
\**************************************************/

// offload help
void CmndTeleinfoRelayHelp ()
{
  AddLog (LOG_LEVEL_NONE, PSTR ("HLP: Commandes pour les relais associés au Linky :"));
  AddLog (LOG_LEVEL_NONE, PSTR (" - relai_topic <topic>    topic du compteur Linky"));
  AddLog (LOG_LEVEL_NONE, PSTR (" - relai_virtuel <v,l>    association a un relai virtuel"));
  AddLog (LOG_LEVEL_NONE, PSTR (" -                        v = index relai virtuel (1..8)"));
  AddLog (LOG_LEVEL_NONE, PSTR (" -                        l = index relai local (1..8)"));
  AddLog (LOG_LEVEL_NONE, PSTR (" - relai_periode <p,l>    association a une période du compteur"));
  AddLog (LOG_LEVEL_NONE, PSTR (" -                        p = index période (1..)"));
  AddLog (LOG_LEVEL_NONE, PSTR (" -                        l = index relai local (1..8)"));
  AddLog (LOG_LEVEL_NONE, PSTR (" - relai_prod <l>         association au relai de production"));
  AddLog (LOG_LEVEL_NONE, PSTR (" -                        l = index relai local (1..8)"));
  ResponseCmndDone();
}

void CmndTeleinfoRelayTopic ()
{
  if (XdrvMailbox.data_len > 0)
  {
    teleinfo_relay.str_topic = XdrvMailbox.data;
    TeleinfoRelaySaveConfig ();
  }
  ResponseCmndChar (teleinfo_relay.str_topic.c_str ());
}

void CmndTeleinfoRelayVirtual ()
{
  uint8_t index, value;
  char    str_data[8];
  char    str_answer[36];
  char   *pstr_separator;

  // if data is given
  if (XdrvMailbox.data_len > 0)
  {
    // if separator ',' is found
    pstr_separator = strchr (XdrvMailbox.data, ',');
    if (pstr_separator != nullptr)
    {
      // extract index and value
      *pstr_separator = 0;
      index = (uint8_t)atoi (XdrvMailbox.data);
      value = (uint8_t)atoi (pstr_separator + 1);

      // if valid, update virtual relay association
      if ((index < 8) && (value < 8) && (teleinfo_relay.arr_virtual[index].relay != value))
      {
        teleinfo_relay.arr_virtual[index].relay = value;
        TeleinfoRelaySaveConfig ();
      }
    }
  }

  // publish status
  str_answer[0] = 0;
  for (index = 0; index < 8; index ++)
  {
    sprintf_P (str_data, PSTR ("%u,%u "), index, teleinfo_relay.arr_virtual[index].relay);
    strlcat (str_answer, str_data, sizeof (str_answer));
  }
  ResponseCmndChar (str_answer);
}

void CmndTeleinfoRelayPeriod ()
{
  uint8_t index, value;
  char    str_data[8];
  char    str_answer[36];
  char   *pstr_separator;

  // if data is given
  if (XdrvMailbox.data_len > 0)
  {
    // if separator ',' is found
    pstr_separator = strchr (XdrvMailbox.data, ',');
    if (pstr_separator != nullptr)
    {
      // extract index and value
      *pstr_separator = 0;
      index = (uint8_t)atoi (XdrvMailbox.data);
      value = (uint8_t)atoi (pstr_separator + 1);

      // if valid, update virtual relay association
      if ((index < 8) && (value <= 8) && (teleinfo_relay.arr_period[index].relay != value))
      {
        teleinfo_relay.arr_period[index].relay = value;
        TeleinfoRelaySaveConfig ();
      }
    }
  }

  // publish status
  str_answer[0] = 0;
  for (index = 0; index < 8; index ++)
  {
    sprintf_P (str_data, PSTR ("%u,%u "), index, teleinfo_relay.arr_period[index].relay);
    strlcat (str_answer, str_data, sizeof (str_answer));
  }
  ResponseCmndChar (str_answer);
}

void CmndTeleinfoRelayProd ()
{
  uint8_t value;

  // if data is given
  if (XdrvMailbox.data_len > 0)
  {
      // if valid, update virtual relay association
      value = (uint8_t)atoi (XdrvMailbox.data);
      if ((value <= 8) && (teleinfo_relay.prod_relay.relay != value))
      {
        teleinfo_relay.prod_relay.relay = value;
        TeleinfoRelaySaveConfig ();
      }
  }

  // publish status
  ResponseCmndNumber (teleinfo_relay.prod_relay.relay);
}

/**************************************************\
 *                  Functions
\**************************************************/

// offload initialisation
void TeleinfoRelayInit ()
{
  uint8_t index;

  // init production relay
  teleinfo_relay.prod_relay.state.data = 0;
  teleinfo_relay.prod_relay.relay      = 0;
  teleinfo_relay.prod_relay.str_name   = "";

  // init period relays
  for (index = 0; index < RELAY_PERIOD_MAX; index ++)
  {
    teleinfo_relay.arr_period[index].state.data = 0;
    teleinfo_relay.arr_period[index].relay      = 0;
    teleinfo_relay.arr_period[index].str_name   = "";
  }

  // init virtual relays
  for (index = 0; index < RELAY_VIRTUAL_MAX; index ++)
  {
    teleinfo_relay.arr_virtual[index].state.data = 0;
    teleinfo_relay.arr_virtual[index].relay      = 0;
    teleinfo_relay.arr_virtual[index].str_name   = "";
  }

  // load configuration
  TeleinfoRelayLoadConfig ();

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: tapez 'relai' pour aide sur commandes linky & relais"));
}

#ifdef USE_TELEINFO

void TeleinfoRelayStatusFromMeter ()
{
  uint8_t index, period, nb_relay;
  char    str_name[64];

  // update flags
  teleinfo_relay.prod_enabled    = teleinfo_prod.enabled;
  teleinfo_relay.virtual_enabled = teleinfo_conso.enabled;
  teleinfo_relay.period_enabled  = (teleinfo_conso.enabled && (TeleinfoDriverGetPeriodQuantity () > 1));

  // update of production relay device
  if (teleinfo_relay.prod_enabled)
  {
    teleinfo_relay.prod_relay.state.status = (uint8_t)TeleinfoDriverGetProductionRelay ();
    teleinfo_relay.prod_relay.str_name     = TeleinfoDriverGetProductionRelayTrigger ();
  }

  // update of period status
  if (teleinfo_relay.period_enabled)
  {
    period   = TeleinfoDriverGetPeriod ();
    nb_relay = min ((uint8_t)RELAY_PERIOD_MAX, TeleinfoDriverGetPeriodQuantity ());
    for (index = 0; index < nb_relay; index ++)
    {
      // if not done
      if (teleinfo_relay.arr_period[index].str_name.isEmpty ())
      {
        // set period name
        TeleinfoPeriodGetLabel (str_name, sizeof (str_name), index);
        teleinfo_relay.arr_period[index].str_name = str_name;

        // set period data
        teleinfo_relay.arr_period[index].state.level = TeleinfoPeriodGetLevel (index);
        teleinfo_relay.arr_period[index].state.hchp  = TeleinfoPeriodGetHP (index);
      }

      // set current status
      teleinfo_relay.arr_period[index].state.status = (uint8_t)(index == period);
    }
  }

  // update of virtual relay status
  if (teleinfo_relay.virtual_enabled)
    for (index = 0; index < RELAY_VIRTUAL_MAX; index ++) teleinfo_relay.arr_virtual[index].state.status = TeleinfoRelayStatus (index);
}

#endif    // USE_TELEINFO

void TeleinfoRelayEverySecond ()
{
  uint8_t index, target, relay, nb_relay;
  uint8_t arr_target[RELAY_PRESENT_MAX];

  // init physical relay state as undefined
  for (index = 0; index < RELAY_PRESENT_MAX; index ++) arr_target[index] = TELEINFO_RELAY_UNDEF;

#ifdef USE_TELEINFO
  TeleinfoRelayStatusFromMeter ();
#endif    // USE_TELEINFO

  // check : loop thru period relays
  for (index = 0; index < RELAY_PERIOD_MAX; index ++)
  {
    relay = teleinfo_relay.arr_period[index].relay;
    if (relay > 0)
    {
      if (teleinfo_relay.arr_period[index].state.status) arr_target[relay - 1] = max (arr_target[relay - 1], (uint8_t)TELEINFO_RELAY_ON);
        else arr_target[relay - 1] = max (arr_target[relay - 1], (uint8_t)TELEINFO_RELAY_OFF);
    }
  }

  // check : loop thru virtual relays
  for (index = 0; index < RELAY_VIRTUAL_MAX; index ++)
  {
    relay = teleinfo_relay.arr_virtual[index].relay;
    if (relay > 0)
    {
      if (teleinfo_relay.arr_virtual[index].state.status) arr_target[relay - 1] = max (arr_target[relay - 1], (uint8_t)TELEINFO_RELAY_ON);
        else arr_target[relay - 1] = max (arr_target[relay - 1], (uint8_t)TELEINFO_RELAY_OFF);
    }
  }

  // check : production relay
  relay = teleinfo_relay.prod_relay.relay;
  if (relay > 0)
  {
    if (teleinfo_relay.prod_relay.state.status) arr_target[relay - 1] = max (arr_target[relay - 1], (uint8_t)TELEINFO_RELAY_ON);
      else arr_target[relay - 1] = max (arr_target[relay - 1], (uint8_t)TELEINFO_RELAY_OFF);
  }

  // set : loop thru local relays
  nb_relay = min ((uint8_t)RELAY_PRESENT_MAX, TasmotaGlobal.devices_present);
  for (index = 0; index < nb_relay; index ++)
  {
    if (arr_target[index] != TELEINFO_RELAY_UNDEF)
    {
      if (arr_target[index] == TELEINFO_RELAY_ON) target = 1; else target = 0;
      if (bitRead (TasmotaGlobal.power, index)) relay = 1; else relay = 0;
      if (relay != target)
      {
        if (target) ExecuteCommandPower (index + 1, POWER_ON, SRC_MAX);
          else ExecuteCommandPower (index + 1, POWER_OFF, SRC_MAX);
        AddLog (LOG_LEVEL_INFO, PSTR ("REL: [Linky] Relay %u is %u"), index + 1, target);
      }
    }
  }
}

#ifndef USE_TELEINFO

// check and update MQTT power subsciption after disconnexion
void TeleinfoRelayMqttSubscribe ()
{
  // if subsciption topic defined
  if (teleinfo_relay.str_topic.length () > 0)
  {
    // subscribe to power meter and log
    MqttSubscribe (teleinfo_relay.str_topic.c_str ());
    AddLog (LOG_LEVEL_INFO, PSTR ("REL: Subscribed to %s"), teleinfo_relay.str_topic.c_str ());
  }
}

// read received MQTT data to retrieve house instant power
//   P1 & W1 : production relay status and power trigger
//   C1..C8 : contract period relay (status;level;hchp;name)
//   V1..V8 : virtual relay status
bool TeleinfoRelayMqttData ()
{
  bool  is_topic;
  int   index, column;
  char *pstr_token;
  char  str_key[8];
  char  str_text[64];
  JsonDocument json_result;
  JsonVariant  json_section;

  // check for meter topic
  is_topic   = (strcmp (teleinfo_relay.str_topic.c_str (), XdrvMailbox.topic) == 0);

  // check if handled sections are present
  if (is_topic)
  {
    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("REL: Received %s"), XdrvMailbox.topic);

    // if section exists, extract data
    if (strstr_P (XdrvMailbox.data, PSTR ("\"RELAY\":")) != nullptr)
    {
      // extract token from JSON
      deserializeJson (json_result, (const char*)XdrvMailbox.data);

      // look for RELAY section
      strcpy_P (str_key, PSTR ("RELAY"));
      json_section = json_result[str_key].as<JsonVariant>();
      if (!json_section.isNull ())
      {
        // check for P1 production relay status
        strcpy_P (str_key, PSTR ("P1"));
        if (!json_section[str_key].isNull ())
        {
          // set state
          teleinfo_relay.prod_relay.state.status = (uint8_t)json_section[str_key].as<unsigned int>();
          teleinfo_relay.prod_enabled = true;

          // check for W1 production relay trigger
          strcpy_P (str_key, PSTR ("W1"));
          if (!json_section[str_key].isNull ()) teleinfo_relay.prod_relay.str_name = json_section[str_key].as<String>();

          // log
          AddLog (LOG_LEVEL_DEBUG, PSTR ("REL: MQTT production -> %u (%s W)"), teleinfo_relay.prod_relay.state.status, teleinfo_relay.prod_relay.str_name.c_str ());
        }

        // loop thru virtual relays to check for status in received JSON
        for (index = 0; index < RELAY_VIRTUAL_MAX; index ++)
        {
          sprintf_P (str_key, PSTR ("V%u"), index + 1);
          if (!json_section[str_key].isNull ())
          {
            // set state
            teleinfo_relay.arr_virtual[index].state.status = (uint8_t)json_section[str_key].as<unsigned int>();
            teleinfo_relay.virtual_enabled = true;

            // log
            AddLog (LOG_LEVEL_DEBUG, PSTR ("REL: MQTT virtuel %u  -> %u"), index + 1, teleinfo_relay.arr_virtual[index].state.status);
          }
        }

        // loop thru periods to check for status and label in received JSON
        for (index = 0; index < RELAY_PERIOD_MAX; index ++)
        {
          // check for period relay status
          sprintf_P (str_key, PSTR ("C%u"), index + 1);
          if (!json_section[str_key].isNull ())
          {
            // extract value
            strlcpy (str_text, json_section[str_key].as<const char*>(), sizeof (str_text));

            // loop thru delimiter
            column = 0;
            pstr_token = strtok (str_text, ",");
            while (pstr_token)
            {
              // extract data according to column
              column++;
              switch (column)
              {
                case 1 : teleinfo_relay.arr_period[index].state.status = (uint8_t)atoi (pstr_token); break;
                case 2 : teleinfo_relay.arr_period[index].state.level  = (uint8_t)atoi (pstr_token); break;
                case 3 : teleinfo_relay.arr_period[index].state.hchp   = (uint8_t)atoi (pstr_token); break;
                case 4 : teleinfo_relay.arr_period[index].str_name     = pstr_token; break;
              }

              // look for next token
              pstr_token = strtok (nullptr, ",");
            }

            // log
            AddLog (LOG_LEVEL_DEBUG, PSTR ("REL: MQTT periode %d  -> %u (%u,%u,%s)"), index + 1, teleinfo_relay.arr_period[index].state.status, teleinfo_relay.arr_period[index].state.level, teleinfo_relay.arr_period[index].state.hchp, teleinfo_relay.arr_period[index].str_name.c_str ());

            // period relays are available
            teleinfo_relay.period_enabled = true;
          }
        }

        // set reception flag and log
        teleinfo_relay.mqtt_enabled = true;
      }
    }
  }

  return is_topic;
}

#endif    // USE_TELEINFO

/**************************************************\
 *                     Web
\**************************************************/

#ifdef USE_WEBSERVER

void TeleinfoRelayWebAddButton ()
{
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>Relay Linky</button></form></p>\n"), PSTR (D_PAGE_TELEINFO_RELAY_CONFIG));
}

// teleinfo relays configuration web page
void TeleinfoRelayWebConfigure ()
{
  uint8_t     index, device, quantity;
  long        delta;
  char        str_style[8];
  char        str_argument[16];
  char        str_text[64];
  const char *pstr_title;

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess ()) return;

#ifdef USE_TELEINFO

  // save production trigger
  if (Webserver->hasArg (F ("save")))
  {
    // set production trigger
    WebGetArg (PSTR (D_CMND_TELEINFO_RELAY_DELTA), str_text, sizeof (str_text));
    delta = atol (str_text);
    if ((delta > 0) && (delta != teleinfo_config.prod_trigger))
    {
      teleinfo_config.prod_trigger = delta;
      TeleinfoDriverSaveSettings ();
    }
  }

  // update production relay name with production trigger value
  sprintf_P (str_text, PSTR ("%d"), TeleinfoDriverGetProductionRelayTrigger ());
  teleinfo_relay.prod_relay.str_name = str_text;

  // update period name
  quantity = TeleinfoDriverGetPeriodQuantity ();
  for (index = 0; index < quantity; index ++)
  {
    TeleinfoPeriodGetLabel (str_text, sizeof (str_text), index);
    teleinfo_relay.arr_period[index].str_name = str_text;
  }

#endif

  // page comes from save button on configuration page
  if (Webserver->hasArg (F ("save")))
  {
    // set MQTT topic according to 'topic' parameter
    WebGetArg (PSTR (D_CMND_TELEINFO_RELAY_TOPIC), str_text, sizeof (str_text));
    teleinfo_relay.str_topic = str_text;

    // save production relay association according to 'p1' parameter
    sprintf_P (str_text, PSTR ("p%u"), 1);
    WebGetArg (str_text, str_argument, sizeof (str_argument));
    if (strlen (str_argument) > 0) teleinfo_relay.prod_relay.relay = (uint8_t)atoi (str_argument);

    // loop to save virtual relay association according to 'v.' parameter
    for (index = 0; index < RELAY_VIRTUAL_MAX; index ++)
    {
      sprintf_P (str_text, PSTR ("v%u"), index);
      WebGetArg (str_text, str_argument, sizeof (str_argument));
      if (strlen (str_argument) > 0) teleinfo_relay.arr_virtual[index].relay = (uint8_t)atoi (str_argument);
    }

    // loop to save period association according to 'c.' parameter
    for (index = 0; index < RELAY_PERIOD_MAX; index ++)
    {
      sprintf_P (str_text, PSTR ("c%u"), index);
      WebGetArg (str_text, str_argument, sizeof (str_argument));
      if (strlen (str_argument) > 0) teleinfo_relay.arr_period[index].relay = (uint8_t)atoi (str_argument);
    }

    // save configuration
    TeleinfoRelaySaveConfig ();
  }

  // beginning of form
  WSContentStart_P (PSTR ("Relais Linky"));
  WSContentSendStyle ();

  // specific style
  WSContentSend_P (PSTR ("\n<style>\n"));

  WSContentSend_P (PSTR ("fieldset {background:#333;margin:6px 0px 0px 0px;border:none;border-radius:8px;}\n")); 
  WSContentSend_P (PSTR ("legend {margin:0px;padding:5px;color:#888;background:transparent;font-style:italic;}\n")); 
  WSContentSend_P (PSTR ("legend:after {content:'';display:block;height:1px;margin:15px 0px;}\n"));

  WSContentSend_P (PSTR ("div.main {padding:0px;margin-top:-28px;}\n"));
  WSContentSend_P (PSTR ("input {margin-bottom:10px;border:none;text-align:center;border:none;border-radius:4px;}\n")); 
  WSContentSend_P (PSTR ("input.topic {text-align:left;}\n")); 
  WSContentSend_P (PSTR ("p,hr {clear:both;}\n")); 
  WSContentSend_P (PSTR ("p.dat {margin:0px 10px;font-size:14px;}\n")); 
  WSContentSend_P (PSTR ("p.dat span {float:left;padding-top:4px;}\n")); 
  WSContentSend_P (PSTR ("p.header span {font-weight:bold;padding-bottom:10px;}\n")); 
  WSContentSend_P (PSTR ("p.header span.cb {font-size:12px;}\n")); 
  WSContentSend_P (PSTR ("p.header span.missing {font-weight:normal;font-style:italic;width:60%%;text-align:center;color:red;}\n")); 
  WSContentSend_P (PSTR ("p.item {font-size:12px;}\n")); 

  WSContentSend_P (PSTR ("span input {accent-color:black;}\n")); 
  WSContentSend_P (PSTR ("span.name {width:32%%;}\n")); 
  WSContentSend_P (PSTR ("span.cb {width:7.5%%;text-align:center;}\n")); 
  WSContentSend_P (PSTR ("span.none input {accent-color:red;}\n"));
  WSContentSend_P (PSTR ("span.val {width:23%%;padding:0px;}\n"));
  WSContentSend_P (PSTR ("span.uni {width:10%%;text-align:center;}\n"));

  WSContentSend_P (PSTR ("</style>\n"));

  // form start  
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), PSTR (D_PAGE_TELEINFO_RELAY_CONFIG));

  // --------------
  //     Topic  
  // --------------

#ifndef USE_TELEINFO

  pstr_title = PSTR ("Saisissez le topic complet .../SENSOR publié par le compteur Linky. La publication des données Relais doit être activée sur le compteur");
  if (teleinfo_relay.mqtt_enabled) strcpy_P (str_argument, PSTR("🔗"));
    else strcpy_P (str_argument, PSTR("⛓️‍💥"));

  WSContentSend_P (PSTR ("<fieldset><legend>%s %s</legend>\n"), str_argument, PSTR ("Compteur Linky"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));
  WSContentSend_P (PSTR ("<p class='dat' title='%s'>Topic</p>"), pstr_title);
  WSContentSend_P (PSTR ("<p class='dat'><input class='topic' name='%s' value='%s' placeholder='.../tele/SENSOR'></p>\n"), PSTR (D_CMND_TELEINFO_RELAY_TOPIC), teleinfo_relay.str_topic.c_str ());
  WSContentSend_P (PSTR ("</div>\n"));
  WSContentSend_P (PSTR ("</fieldset>\n"));

#endif    // USE_TELEINFO

  // -------------------
  //     Association  
  // -------------------

  WSContentSend_P (PSTR ("<fieldset><legend>%s</legend><div class='main'>\n"), PSTR ("🎗️ Association des relais"));

  // local relay header
  // ------------------

  WSContentSend_P (PSTR ("<p class='dat header'><span class='name'>Relai local</span>"));
  for (index = 0; index <= TasmotaGlobal.devices_present; index ++)
  {
    GetTextIndexed (str_text, sizeof (str_text), index, kTeleinfoRelayLocalSign);
    WSContentSend_P (PSTR ("<span class='cb'>%s</span>"), str_text);
  }

  // if no relay present
  if (TasmotaGlobal.devices_present == 0) WSContentSend_P (PSTR ("<span class='missing'>Aucun relai déclaré</span>"));

  WSContentSend_P (PSTR ("</p>\n"));

  // virtual relays  
  // --------------

  WSContentSend_P (PSTR ("<hr>\n"));

  // loop thru virtual relays
  for (index = 0; index < RELAY_VIRTUAL_MAX; index ++)
  {
    // display relay name
    WSContentSend_P (PSTR ("<p class='dat item'><span class='name'>Relai virtuel %c</span>\n"), index + 65);

    // loop thru local devices
    for (device = 0; device <= TasmotaGlobal.devices_present; device ++)
    {
      // set radio button color
      str_style[0] = 0;
      if (device == 0) strcpy_P (str_style, PSTR (" none"));

      // set selection status
      str_argument[0] = 0;
      if (teleinfo_relay.arr_virtual[index].relay == device) strcpy_P (str_argument, PSTR ("checked"));

      // set tooltip
      if (device == 0) sprintf_P (str_text, PSTR ("Aucune association"));
        else sprintf_P (str_text, PSTR ("Le relai virtuel %c déclenche le relai physique n°%u"), index + 65, device);

      // display choice
      WSContentSend_P (PSTR ("<span class='cb%s'><input type='radio' name='v%u' value='%u' title='%s' %s></span>"), str_style, index, device, str_text, str_argument);
    }

    WSContentSend_P (PSTR ("</p>\n"));
  }

  // contract periods  
  // ----------------

  WSContentSend_P (PSTR ("<hr>\n"));

  // loop thru periods
  quantity = 0;
  for (index = 0; index < RELAY_PERIOD_MAX; index ++)
  {
    // if period is defined
    if (!teleinfo_relay.arr_period[index].str_name.isEmpty ())
    {
      // increase quantity
      quantity++;

      // display period name
      WSContentSend_P (PSTR ("<p class='dat item'><span class='name'>%s</span>\n"), teleinfo_relay.arr_period[index].str_name.c_str ());

      // loop thru local devices
      for (device = 0; device <= TasmotaGlobal.devices_present; device ++)
      {
        // set radio button color
        str_style[0] = 0;
        if (device == 0) strcpy_P (str_style, PSTR (" none"));

        // set selection status
        str_argument[0] = 0;
        if (teleinfo_relay.arr_period[index].relay == device) strcpy_P (str_argument, PSTR ("checked"));

        // set tooltip
        if (device == 0) sprintf_P (str_text, PSTR ("Aucune association"));
          else sprintf_P (str_text, PSTR ("Période %s déclenche le relai n°%u"), teleinfo_relay.arr_period[index].str_name.c_str (), device);

        // display choice
        WSContentSend_P (PSTR ("<span class='cb%s'><input type='radio' name='c%u' value='%u' title='%s' %s></span>"), str_style, index, device, str_text, str_argument);
      }

      WSContentSend_P (PSTR ("</p>\n"));
    }
  }

  // if no period received
  if (quantity == 0) WSContentSend_P (PSTR ("<p class='dat item'>En attente de réception des périodes ...</p>\n"));
  
  // production  
  // ----------

  WSContentSend_P (PSTR ("<hr>\n"));

  WSContentSend_P (PSTR ("<p class='dat item'><span class='name'>%s</span>\n"), PSTR ("Production"));

  // loop thru local devices
  for (device = 0; device <= TasmotaGlobal.devices_present; device ++)
  {
    // set radio button color
    str_style[0] = 0;
    if (device == 0) strcpy_P (str_style, PSTR (" none"));

    // set selection status
    str_argument[0] = 0;
    if (device == teleinfo_relay.prod_relay.relay) strcpy_P (str_argument, PSTR ("checked"));

    // set tooltip
    if (device == 0) sprintf_P (str_text, PSTR ("Aucune association"));
      else sprintf_P (str_text, PSTR ("La production déclenche le relai n°%u"), device);

    // display choice
    WSContentSend_P (PSTR ("<span class='cb%s'><input type='radio' name='p%u' value='%u' title='%s' %s></span>"), str_style, 1, device, str_text, str_argument);
  }

  WSContentSend_P (PSTR ("</p>\n"));

#ifdef USE_TELEINFO
  // set production trigger
  WSContentSend_P (PSTR ("<p class='dat item'><span class='name'>Trigger</span><span class='val'><input type='number' name='delta' min=0 max=12750 step=50 value=%s></span><span class='uni'>W</span></p>\n"), teleinfo_relay.prod_relay.str_name.c_str ());
#endif  // USE_TELEINFO

  WSContentSend_P (PSTR ("</div></fieldset><br>\n"));

  // --------------
  //  save button  
  // --------------

  WSContentSend_P (PSTR ("<p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), PSTR (D_SAVE));
  WSContentSend_P (PSTR ("</form>\n"));

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

// display relay style on main page
void TeleinfoRelayWebAddMainButton ()
{
  WSContentSend_P (PSTR ("<style>\n"));

#ifndef USE_TELEINFO
  // style inherited from teleinfo
  WSContentSend_P (PSTR ("table hr{display:none;}\n"));
  WSContentSend_P (PSTR (".sec{font-size:12px;text-align:center;padding:4px 6px;margin-bottom:5px;background:#333333;border-radius:12px;}"));
  WSContentSend_P (PSTR (".tic{display:flex;padding:2px 0px;font-size:11px;}\n"));
  WSContentSend_P (PSTR (".tic div{padding:0px;}\n"));
  WSContentSend_P (PSTR (".ticb{font-size:14px;padding:0px 0px 4px 5px;}\n"));
  WSContentSend_P (PSTR (".tic48l{width:48%%;text-align:left;font-weight:bold;}\n"));
#endif    // USE_TELEINFO

  // relay style
  WSContentSend_P (PSTR (".rel{display:flex;padding:3px 0px;font-size:12px;}\n"));
  WSContentSend_P (PSTR (".rel div{padding:0px;}\n"));
  WSContentSend_P (PSTR (".rel02{width:2%%;}\n"));
  WSContentSend_P (PSTR (".rel10l{width:10%%;text-align:left;}\n"));
  WSContentSend_P (PSTR (".rel18r{width:18%%;text-align:right;font-weight:bold;}\n"));
  WSContentSend_P (PSTR (".rel25l{width:25%%;text-align:left;}\n"));
  WSContentSend_P (PSTR (".rel45f{width:45%%;display:flex;}\n"));
  WSContentSend_P (PSTR (".rel50r{width:51%%;}\n"));
  WSContentSend_P (PSTR (".rel50r div{font-size:12px;float:right;}\n"));
  WSContentSend_P (PSTR (".rel75f{width:75%%;display:flex;}\n"));
  WSContentSend_P (PSTR (".relay{width:18px;height:18px;padding-top:1px;margin-right:12px;border-radius:9px;background-color:#444;color:#888;}\n"));
  WSContentSend_P (PSTR (".on{background-color:#0b0;color:white;font-weight:bold;}\n"));
  WSContentSend_P (PSTR (".on1{background-color:#06b;color:white;font-weight:bold;}\n"));
  WSContentSend_P (PSTR (".on2{background-color:#ddd;color:black;font-weight:bold;}\n"));
  WSContentSend_P (PSTR (".on3{background-color:#b00;color:white;font-weight:bold;}\n"));
  WSContentSend_P (PSTR (".on4{background-color:#fb0;color:black;font-weight:bold;}\n"));
  WSContentSend_P (PSTR (".hc{font-weight:normal;opacity:0.75;}\n"));

  WSContentSend_P (PSTR ("</style>\n"));
}

void TeleinfoRelayWebDisplayProduction (const bool always)
{
  char str_class[8];

  // if nothing to display, ignore
  if (!always && !teleinfo_relay.prod_relay.state.status) return;

  // display relay status
  str_class[0] = 0;
  if (teleinfo_relay.prod_relay.state.status) strcpy_P (str_class, PSTR (" on4"));
  WSContentSend_P (PSTR ("<div class='relay%s' title='Relai production'>P</div>"), str_class);
}

void TeleinfoRelayWebDisplayPeriod (const bool always, const uint8_t period)
{
  char str_class[12];

  // check parameters
  if (period >= RELAY_PERIOD_MAX) return;

  // if nothing to display, ignore
  if (teleinfo_relay.arr_period[period].str_name.isEmpty ()) return;
  if (!always && !teleinfo_relay.arr_period[period].state.status) return;

  // display relay status
  str_class[0] = 0;
  if (teleinfo_relay.arr_period[period].state.status) sprintf_P (str_class, PSTR (" on%u"), teleinfo_relay.arr_period[period].state.level);
  if (!teleinfo_relay.arr_period[period].state.hchp) strcat_P (str_class, PSTR (" hc"));
  WSContentSend_P (PSTR ("<div class='relay%s' title='Relai %s'>%u</div>"), str_class, teleinfo_relay.arr_period[period].str_name.c_str (), period + 1);
}

void TeleinfoRelayWebDisplayVirtual (const bool always, const uint8_t index)
{
  char str_class[4];

  // check parameters
  if (index >= RELAY_VIRTUAL_MAX) return;

  // if nothing to display, ignore
  if (!always && !teleinfo_relay.arr_virtual[index].state.status) return;

  // display relay status
  str_class[0] = 0;
  if (teleinfo_relay.arr_virtual[index].state.status) strcpy_P (str_class, PSTR (" on"));
  WSContentSend_P (PSTR ("<div class='relay%s' title='Relai virtuel n°%u'>%c</div>"), str_class, index + 1, index + 65);
}

// Append relay state to main page
void TeleinfoRelayWebSensor ()
{
  int index;

#ifdef USE_TELEINFO
  if (!teleinfo_config.relay) return;
#endif    // USE_TELEINFO

  // header
#ifdef USE_TELEINFO
  WSContentSend_P (PSTR ("<div class='sec' onclick=\"onClickMain(\'%s?%s=%u\')\">\n"), PSTR (TIC_PAGE_DISPLAY), PSTR (CMND_TIC_MAIN), TIC_DISPLAY_RELAY);
  #else
  WSContentSend_P (PSTR ("<div class='sec'>\n"));
#endif    // USE_TELEINFO

  // first line
  WSContentSend_P (PSTR ("<div class='tic ticb'><div class='tic48l'>Relais Linky</div>"));

#ifdef USE_TELEINFO
  // full display or minimize
  if (!teleinfo_config.arr_main[TIC_DISPLAY_RELAY])
  {
    WSContentSend_P (PSTR ("<div class='rel50r'>"));
    for (index = RELAY_VIRTUAL_MAX - 1; index >= 0; index --) TeleinfoRelayWebDisplayVirtual (false, index);
    for (index = RELAY_PERIOD_MAX - 1;  index >= 0; index --) TeleinfoRelayWebDisplayPeriod (false, index);
    TeleinfoRelayWebDisplayProduction (false);
    WSContentSend_P (PSTR ("</div>"));
  } 
#endif    // USE_TELEINFO

  WSContentSend_P (PSTR ("</div>\n"));    // tic ticb

#ifdef USE_TELEINFO
  if (teleinfo_config.arr_main[TIC_DISPLAY_RELAY])
  {
#endif    // USE_TELEINFO

    // production relay (with average production value)
    if (teleinfo_relay.prod_enabled)
    {
      WSContentSend_P (PSTR ("<div class='rel'><div class='rel25l'>Production</div><div class='rel45f'>"));
      TeleinfoRelayWebDisplayProduction (true);
      WSContentSend_P (PSTR ("</div>"));    // rel45f
      WSContentSend_P (PSTR ("<div class='rel18r'>%s</div><div class='rel02'></div><div class='rel10l'>W</div>"), teleinfo_relay.prod_relay.str_name.c_str ());
      WSContentSend_P (PSTR ("</div>"));    // rel
    }

    // period relays
    if (teleinfo_relay.period_enabled)
    {
      WSContentSend_P (PSTR ("<div class='rel'><div class='rel25l'>Périodes</div><div class='rel75f'>"));
      for (index = 0; index < RELAY_PERIOD_MAX; index ++) TeleinfoRelayWebDisplayPeriod (true, index);
      WSContentSend_P (PSTR ("</div></div>\n"));    // rel75f  rel
    }

    // virtual relays
    if (teleinfo_relay.virtual_enabled)
    {
      WSContentSend_P (PSTR ("<div class='rel'><div class='rel25l'>Virtuels</div><div class='rel75f'>"));
      for (index = 0; index < RELAY_VIRTUAL_MAX; index ++) TeleinfoRelayWebDisplayVirtual (true, index);
      WSContentSend_P (PSTR ("</div></div>\n"));    // rel75f  rel
    }

#ifdef USE_TELEINFO
  }
#endif    // USE_TELEINFO

  WSContentSend_P (PSTR ("</div>\n"));    // sec
}

#endif    // USE_WEBSERVER

/***********************************************************\
 *                      Interface
\***********************************************************/

bool XdrvTeleinfoRelay (const uint32_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
    case FUNC_INIT:
      TeleinfoRelayInit ();
      break;

    case FUNC_COMMAND:
      result = DecodeCommand (kTeleinfoRelayCommands, TeleinfoRelayCommand);
      break;

   case FUNC_EVERY_SECOND:
      TeleinfoRelayEverySecond ();
      break;

#ifndef USE_TELEINFO
    case FUNC_MQTT_SUBSCRIBE:
      TeleinfoRelayMqttSubscribe ();
      break;

    case FUNC_MQTT_DATA:
      result = TeleinfoRelayMqttData ();
      break;
#endif  // USE_TELEINFO

#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      TeleinfoRelayWebSensor ();
      break;

    case FUNC_WEB_ADD_MAIN_BUTTON:
      TeleinfoRelayWebAddMainButton ();
      break;

    case FUNC_WEB_ADD_BUTTON:
      TeleinfoRelayWebAddButton ();
      break;

    case FUNC_WEB_ADD_HANDLER:
      Webserver->on (F ("/" D_PAGE_TELEINFO_RELAY_CONFIG), TeleinfoRelayWebConfigure);
      break;

#endif  // USE_WEBSERVER
  }
  
  return result;
}

#endif  // USE_TELEINFO_RELAY
