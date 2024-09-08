/*
  xdrv_120_linky_relay.ino - Virtual relay management from Linky meter
  
  Copyright (C) 2020  Nicolas Bernaerts
    10/08/2024 - v1.0  - Creation

  Settings are stored using free_f63 parameters :
    - Settings->rf_code[13][0..7] = Assocation of virtual relay R1..R7 to local relays
    - Settings->rf_code[14][0..7] = Assocation of periods P1..P8 to local relays

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

#ifdef USE_RELAY_LINKY

#define XDRV_120                          120

#include <ArduinoJson.h>

#define RELAY_PRESENT_MAX                 8
#define RELAY_VIRTUAL_MAX                 8
#define RELAY_PERIOD_MAX                  8

#define D_PAGE_LINKY_RELAY_CONFIG         "lrelay"

#define D_CMND_LINKY_RELAY_TOPIC          "topic"

// offloading commands
const char kLinkyRelayCommands[]         PROGMEM = "relai" "|" "|_topic" "|_virtuel" "|_periode";
void (* const LinkyRelayCommand[])(void) PROGMEM = { &CmndLinkyRelayHelp, &CmndLinkyRelayTopic, &CmndLinkyRelayVirtual, &CmndLinkyRelayPeriod };
 
/*************************************************\
 *               Variables
\*************************************************/

// configuration
struct linky_relay {
  uint8_t status;                                   // period status (0/1)
  uint8_t relay;                                    // physical relay associated to period
  String  str_name;                                 // relay/period name
};
struct {
  uint8_t     received = 0;                         // flag to check if data has been received
  String      str_topic;                            // mqtt topic to be used for meter
  linky_relay arr_virtual[RELAY_VIRTUAL_MAX];       // teleinfo virtual relays 
  linky_relay arr_period[RELAY_PERIOD_MAX];         // teleinfo period status 
} linkyrelay_config;

/**************************************************\
 *                  Accessors
\**************************************************/

// load config parameters
void LinkyRelayLoadConfig ()
{
  uint8_t index;

#ifndef USE_TELEINFO
  // mqtt config
  linkyrelay_config.str_topic = SettingsText (SET_VIRTUAL_RELAY_TOPIC);
#endif    // USE_TELEINFO

  // association between virtual relays and physical relays
  for (index = 0; index < RELAY_VIRTUAL_MAX; index ++)
  {
    linkyrelay_config.arr_virtual[index].relay = Settings->rf_code[13][index];
    if (linkyrelay_config.arr_virtual[index].relay > 8) linkyrelay_config.arr_virtual[index].relay = 0;
  }

  // association between periods and physical relays
  for (index = 0; index < RELAY_PERIOD_MAX; index ++)
  {
    linkyrelay_config.arr_period[index].relay = Settings->rf_code[14][index];
    if (linkyrelay_config.arr_period[index].relay > 8) linkyrelay_config.arr_period[index].relay = 0;
  } 

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("REL: Loaded linky relay association"));
}

// save config parameters
void LinkyRelaySaveConfig ()
{
  uint8_t index;

#ifndef USE_TELEINFO
  // mqtt config
  SettingsUpdateText (SET_VIRTUAL_RELAY_TOPIC, linkyrelay_config.str_topic.c_str ());
#endif    // USE_TELEINFO

  // association between virtual relays and physical relays
  for (index = 0; index < RELAY_VIRTUAL_MAX; index ++) Settings->rf_code[13][index] = linkyrelay_config.arr_virtual[index].relay;

  // association between periods and physical relays
  for (index = 0; index < RELAY_PERIOD_MAX; index ++) Settings->rf_code[14][index] = linkyrelay_config.arr_period[index].relay;

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("REL: saved linky relay association"));
}

/**************************************************\
 *                  Commands
\**************************************************/

// offload help
void CmndLinkyRelayHelp ()
{
  AddLog (LOG_LEVEL_NONE, PSTR ("HLP: Linky relay association commands :"));
  AddLog (LOG_LEVEL_NONE, PSTR (" - relai_topic   <topic>  topic du compteur Linky"));
  AddLog (LOG_LEVEL_NONE, PSTR (" - relai_virtuel <v,l>    association a un relai virtuel"));
  AddLog (LOG_LEVEL_NONE, PSTR (" -                        v = index relai virtuel"));
  AddLog (LOG_LEVEL_NONE, PSTR (" -                        l = index relai local"));
  AddLog (LOG_LEVEL_NONE, PSTR (" - relai_periode <p,l>    association a une période du compteur"));
  AddLog (LOG_LEVEL_NONE, PSTR (" -                        p = index période"));
  AddLog (LOG_LEVEL_NONE, PSTR (" -                        l = index relai local"));
  ResponseCmndDone();
}

void CmndLinkyRelayTopic ()
{
  if (XdrvMailbox.data_len > 0)
  {
    linkyrelay_config.str_topic = XdrvMailbox.data;
    LinkyRelaySaveConfig ();
  }
  ResponseCmndChar (linkyrelay_config.str_topic.c_str ());
}

void CmndLinkyRelayVirtual ()
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
      if ((index < 8) && (value < 8))
      {
        linkyrelay_config.arr_virtual[index].relay = value;
        LinkyRelaySaveConfig ();
      }
    }
  }

  // publish status
  str_answer[0] = 0;
  for (index = 0; index < 8; index ++)
  {
    sprintf_P (str_data, PSTR ("%u,%u "), index, linkyrelay_config.arr_virtual[index].relay);
    strlcat (str_answer, str_data, sizeof (str_answer));
  }
  ResponseCmndChar (str_answer);
}

void CmndLinkyRelayPeriod ()
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
      if ((index < 8) && (value <= 8))
      {
        linkyrelay_config.arr_period[index].relay = value;
        LinkyRelaySaveConfig ();
      }
    }
  }

  // publish status
  str_answer[0] = 0;
  for (index = 0; index < 8; index ++)
  {
    sprintf_P (str_data, PSTR ("%u,%u "), index, linkyrelay_config.arr_period[index].relay);
    strlcat (str_answer, str_data, sizeof (str_answer));
  }
  ResponseCmndChar (str_answer);
}

/**************************************************\
 *                  Functions
\**************************************************/

// offload initialisation
void LinkyRelayInit ()
{
  uint8_t index;

  // disable fast cycle power recovery
  Settings->flag3.fast_power_cycle_disable = true;

  // init virtual relay association
  for (index = 0; index < RELAY_VIRTUAL_MAX; index ++) linkyrelay_config.arr_virtual[index].status = 0;
  for (index = 0; index < RELAY_PERIOD_MAX; index ++) linkyrelay_config.arr_period[index].status = UINT8_MAX;

  // load configuration
  LinkyRelayLoadConfig ();

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: tapez 'relai' pour aide sur commandes association compteur / relais"));
}

void LinkyRelayEverySecond ()
{
  bool    handled;
  uint8_t act_status, new_status;
  uint8_t device, index, relay;

#ifdef USE_TELEINFO
  // direct update of virtual relay statusdevice
  for (index = 0; index < RELAY_VIRTUAL_MAX; index ++) linkyrelay_config.arr_virtual[index].status = TeleinfoRelayStatus (index);

  // direct update of period status
  relay = min ((uint8_t)RELAY_PERIOD_MAX, teleinfo_contract.period_qty);
  for (index = 0; index < relay; index ++) 
    if (index == teleinfo_contract.period_idx) linkyrelay_config.arr_period[index].status = 1;
      else linkyrelay_config.arr_period[index].status = 0;
#endif

  // loop thru local relays
  for (device = 0; device < TasmotaGlobal.devices_present; device ++)
  {
    // init
    handled    = false;
    new_status = 0;
    relay      = device + 1;

    // loop thru virtual relay
    for (index = 0; index < RELAY_VIRTUAL_MAX; index ++)
      if (linkyrelay_config.arr_virtual[index].relay == relay)
      {
        handled = true;
        if (linkyrelay_config.arr_virtual[index].status == 1) new_status = 1;
      }

    // loop thru periods
    for (index = 0; index < RELAY_PERIOD_MAX; index ++)
      if (linkyrelay_config.arr_period[index].relay == relay)
      {
        handled = true;
        if (linkyrelay_config.arr_virtual[index].status == 1) new_status = 1;
      }

    // if relay is handled
    if (handled)
    {
      // get actual relay status
      act_status = bitRead (TasmotaGlobal.power, device);

      // is state has changed, switch relay and log
      if (act_status != new_status)
      {
        if (new_status == 0) ExecuteCommandPower (relay, POWER_OFF, SRC_MAX);
          else ExecuteCommandPower (relay, POWER_ON, SRC_MAX);
        AddLog (LOG_LEVEL_INFO, PSTR ("REL: Relay %u changed to %u"), relay, new_status);
      }
    }
  }
}

#ifndef USE_TELEINFO

// check and update MQTT power subsciption after disconnexion
void LinkyRelayMqttSubscribe ()
{
  // if subsciption topic defined
  if (linkyrelay_config.str_topic.length () > 0)
  {
    // subscribe to power meter and log
    MqttSubscribe (linkyrelay_config.str_topic.c_str ());
    AddLog (LOG_LEVEL_INFO, PSTR ("REL: Subscribed to %s"), linkyrelay_config.str_topic.c_str ());
  }
}

// read received MQTT data to retrieve house instant power
bool LinkyRelayMqttData ()
{
  bool    is_found;
  uint8_t index, relay, act_state, new_state;
  char    str_key[8];
  DynamicJsonDocument json_result(3584);
  JsonVariant         json_section, json_key;

  // check for meter topic
  is_found = (strcmp (linkyrelay_config.str_topic.c_str (), XdrvMailbox.topic) == 0);

  // check if handled sections are present
  if (is_found) is_found = (strstr_P (XdrvMailbox.data, PSTR ("\"RELAY\":")) != nullptr);

  // if section exists, extract data
  if (is_found)
  {
    // extract token from JSON
    deserializeJson (json_result, (const char*)XdrvMailbox.data);

    // look for RELAY section
    json_section = json_result["RELAY"].as<JsonVariant>();
    if (!json_section.isNull ())
    {
      // loop thru virtual relays to check for status in received JSON
      for (index = 0; index < RELAY_VIRTUAL_MAX; index ++)
      {
        sprintf_P (str_key, PSTR ("R%u"), index + 1);
        json_key = json_section[str_key].as<JsonVariant>();
        if (!json_key.isNull ()) linkyrelay_config.arr_virtual[index].status = (uint8_t)json_section[str_key].as<unsigned int>();
      }

      // loop thru periods to check for status and label in received JSON
      for (index = 0; index < RELAY_PERIOD_MAX; index ++)
      {
        // check for status
        sprintf_P (str_key, PSTR ("P%u"), index + 1);
        json_key = json_section[str_key].as<JsonVariant>();
        if (!json_key.isNull ()) linkyrelay_config.arr_period[index].status = (uint8_t)json_section[str_key].as<unsigned int>();

        // check for label
        sprintf_P (str_key, PSTR ("L%u"), index + 1);
        json_key = json_section[str_key].as<JsonVariant>();
        if (!json_key.isNull ()) linkyrelay_config.arr_period[index].str_name = json_section[str_key].as<String>();
      }

      // set reception flag
      linkyrelay_config.received = 1;
    }
  }

  return is_found;
}

#endif    // USE_TELEINFO

/**************************************************\
 *                     Web
\**************************************************/

#ifdef USE_WEBSERVER

// teleinfo relays configuration web page
void LinkyRelayWebConfigure ()
{
  uint8_t     index, device;
  char        str_style[8];
  char        str_argument[16];
  char        str_text[64];
  const char *pstr_title;

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess ()) return;

#ifdef USE_TELEINFO
  // update period name
  for (index = 0; index < teleinfo_contract.period_qty; index ++)
  {
    TeleinfoPeriodGetLabel (str_text, sizeof (str_text), index);
    linkyrelay_config.arr_period[index].str_name = str_text;
  }
#endif

  // page comes from save button on configuration page
  if (Webserver->hasArg (F ("save")))
  {
    // set MQTT topic according to 'topic' parameter
    WebGetArg (PSTR (D_CMND_LINKY_RELAY_TOPIC), str_text, sizeof (str_text));
    linkyrelay_config.str_topic = str_text;

    // loop to save virtual relay association according to 'v.' parameter
    for (index = 0; index < RELAY_VIRTUAL_MAX; index ++)
    {
      sprintf_P (str_text, PSTR ("v%u"), index);
      WebGetArg (str_text, str_argument, sizeof (str_argument));
      if (strlen (str_argument) > 0) linkyrelay_config.arr_virtual[index].relay = (uint8_t)atoi (str_argument);
    }

    // loop to save period association according to 'p.' parameter
    for (index = 0; index < RELAY_PERIOD_MAX; index ++)
    {
      sprintf_P (str_text, PSTR ("p%d"), index);
      WebGetArg (str_text, str_argument, sizeof (str_argument));
      if (strlen (str_argument) > 0) linkyrelay_config.arr_period[index].relay = (uint8_t)atoi (str_argument);
    }

    // save configuration
    LinkyRelaySaveConfig ();
  }

  // beginning of form
  WSContentStart_P (PSTR ("Relais Linky"));
  WSContentSendStyle ();

  // specific style
  WSContentSend_P (PSTR ("\n<style>\n"));
  WSContentSend_P (PSTR ("fieldset {margin-bottom:24px;padding-top:12px;}\n")); 
  WSContentSend_P (PSTR ("legend {padding:0px 15px;margin-top:-10px;font-weight:bold;}\n")); 
  WSContentSend_P (PSTR ("input,select {margin-bottom:10px;border:none;}\n")); 
  WSContentSend_P (PSTR ("p,br,hr {clear:both;}\n")); 
  WSContentSend_P (PSTR ("hr {margin:12px 0px;}\n")); 
  WSContentSend_P (PSTR ("p.dat {margin:0px 10px;}\n")); 
  WSContentSend_P (PSTR ("p.dat span {float:left;padding-top:4px;}\n")); 
  WSContentSend_P (PSTR ("p.header {font-weight:bold;margin-bottom:4px;}\n")); 
  WSContentSend_P (PSTR ("p.item {font-size:14px;}\n")); 
  WSContentSend_P (PSTR ("span input {accent-color:black;}\n")); 
  WSContentSend_P (PSTR ("span.name {width:33%%;}\n")); 
  WSContentSend_P (PSTR ("span.cb {width:7%%;text-align:center;}\n")); 
  WSContentSend_P (PSTR ("span.abs {color:#666;}\n")); 
  WSContentSend_P (PSTR ("span.abs input {accent-color:#666;}\n")); 
  WSContentSend_P (PSTR ("span.none {width:11%%;}\n"));
  WSContentSend_P (PSTR ("span.none input {accent-color:red;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // form start  
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), PSTR (D_PAGE_LINKY_RELAY_CONFIG));

  // --------------
  //     Topic  
  // --------------

#ifndef USE_TELEINFO
  WSContentSend_P (PSTR ("<fieldset><legend>%s</legend>\n"), PSTR ("⚡ Compteur"));
  pstr_title = PSTR ("Saisissez le topic complet .../SENSOR publié par le compteur Linky. La publication des données Relais doit être activée sur le compteur");
  if (linkyrelay_config.received) { strcpy_P (str_argument, PSTR("🔗")); strcpy_P (str_text, PSTR ("connecté")); }
    else { strcpy_P (str_argument, PSTR("⛓️‍💥")); strcpy_P (str_text, PSTR ("non connecté")); }
  WSContentSend_P (PSTR ("<p class='dat' title='[%s] %s'>Topic %s</p>"), str_text, pstr_title, str_argument);
  WSContentSend_P (PSTR ("<p class='dat'><input name='%s' value='%s' placeholder='linky/tele/SENSOR'></p>\n"), PSTR (D_CMND_LINKY_RELAY_TOPIC), linkyrelay_config.str_topic.c_str ());
  WSContentSend_P (PSTR ("</fieldset>\n"));
#endif    // USE_TELEINFO

  // -------------------
  //     Association  
  // -------------------

  WSContentSend_P (PSTR ("<fieldset><legend>%s</legend>\n"), PSTR ("🔌 Pilotage des relais"));

  // local relay header
  // ------------------

  WSContentSend_P (PSTR ("<p class='dat header'><span class='name'>Relais</span>"));
  WSContentSend_P (PSTR ("<span class='cb none'></span>"));
  for (index = 1; index <= RELAY_PRESENT_MAX; index ++)
  {
    if (index > TasmotaGlobal.devices_present) strcpy_P (str_text, PSTR (" abs"));
      else str_text[0] = 0;
    WSContentSend_P (PSTR ("<span class='cb%s'>%u</span>"), str_text, index);
  }
  WSContentSend_P (PSTR ("</p>\n"));

  WSContentSend_P (PSTR ("<br><hr>\n"));

  // periods  
  // -------

  WSContentSend_P (PSTR ("<p class='dat header'>Périodes</p>\n"));

  // loop thru periods
  for (index = 0; index < RELAY_PERIOD_MAX; index ++)
  {
    // if period is defined
    if (linkyrelay_config.arr_period[index].status != UINT8_MAX)
    {
      // display period name
      WSContentSend_P (PSTR ("<p class='dat item'><span class='name'>%s</span>\n"), linkyrelay_config.arr_period[index].str_name.c_str ());

      // loop thru local devices
      for (device = 0; device <= RELAY_PRESENT_MAX; device ++)
      {
        // set radio button color
        if (device == 0) strcpy_P (str_style, PSTR (" none"));
          else if (device > TasmotaGlobal.devices_present) strcpy_P (str_style, PSTR (" abs"));
          else str_style[0] = 0;

        // set selection status
        if (device == linkyrelay_config.arr_period[index].relay) strcpy_P (str_argument, PSTR ("checked"));
          else str_argument[0] = 0;

        // set tooltip
        if (device == 0) sprintf_P (str_text, PSTR ("Aucun relai local ne sera associé à la période %s"), linkyrelay_config.arr_period[index].str_name.c_str ());
          else sprintf_P (str_text, PSTR ("Le relai local n°%u sera actif pendant la période %s"), device, linkyrelay_config.arr_period[index].str_name.c_str ());

        // display choice
        WSContentSend_P (PSTR ("<span class='cb%s'><input type='radio' name='p%u' value='%u' title='%s' %s></span>"), str_style, index, device, str_text, str_argument);
      }

      WSContentSend_P (PSTR ("</p>\n"));
    }
  }

  WSContentSend_P (PSTR ("<br><hr>\n"));

  // virtual relays  
  // --------------

  WSContentSend_P (PSTR ("<p class='dat header'>Virtuels</p>\n"));

  // loop thru virtual relays
  for (index = 0; index < RELAY_VIRTUAL_MAX; index ++)
  {
    // display relay name
    WSContentSend_P (PSTR ("<p class='dat item'><span class='name'>Linky n°%u</span>\n"), index + 1);

    // loop thru local devices
    for (device = 0; device <= RELAY_PRESENT_MAX; device ++)
    {
      // set radio button color
      if (device == 0) strcpy_P (str_style, PSTR (" none"));
        else if (device > TasmotaGlobal.devices_present) strcpy_P (str_style, PSTR (" abs"));
        else str_style[0] = 0;

      // set selection status
      if (device == linkyrelay_config.arr_virtual[index].relay) strcpy_P (str_argument, PSTR ("checked"));
        else str_argument[0] = 0;

      // set tooltip
      if (device == 0) sprintf_P (str_text, PSTR ("Aucun relai local ne sera associé au relai virtuel n°%u"), index + 1);
        else sprintf_P (str_text, PSTR ("Le relai local n°%u sera actif quand le relai virtuel n°%u sera actif"), device, index + 1);

      // display choice
      WSContentSend_P (PSTR ("<span class='cb%s'><input type='radio' name='v%u' value='%u' title='%s' %s></span>"), str_style, index, device, str_text, str_argument);
    }

    WSContentSend_P (PSTR ("</p>\n"));
  }

  WSContentSend_P (PSTR ("</fieldset>\n"));

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

#ifndef USE_TELEINFO

// Append relay state to main page
void LinkyRelayWebSensor ()
{
  uint8_t index, opacity;
  char    str_text[16];

  // style
  WSContentSend_P (PSTR ("<style>table hr{display:none;}</style>\n"));

  // section start
  WSContentSend_P (PSTR ("<div style='text-align:center;padding:4px 6px;margin-bottom:5px;background:#333333;border-radius:12px;'>\n"));

  // status
  WSContentSend_P (PSTR ("<div style='display:flex;padding:0px;font-size:22px;'>\n"));
  WSContentSend_P (PSTR ("<div style='width:20%%;padding:6px 0px 0px 0px;text-align:left;font-weight:bold;font-size:14px'>Virtual</div>\n"));
  for (index = 0; index < RELAY_VIRTUAL_MAX; index ++) 
  {
    if (linkyrelay_config.arr_virtual[index].relay == 0) opacity = 40;
      else opacity = 100;
    if (linkyrelay_config.arr_virtual[index].status == 1) strcpy_P (str_text, PSTR ("🟢"));
      else strcpy_P (str_text, PSTR ("🔴"));
    WSContentSend_P (PSTR ("<div style='width:10%%;padding:0px;opacity:%u%%;'>%s</div>\n"), opacity, str_text);
  }
  WSContentSend_P (PSTR ("</div>\n"));

  // number
  WSContentSend_P (PSTR ("<div style='display:flex;padding:0px;margin:-24px 0px 4px 0px;font-size:16px;'>\n"));
  WSContentSend_P (PSTR ("<div style='width:20%%;padding:0px;'></div>\n"));
  for (index = 0; index < RELAY_VIRTUAL_MAX; index ++)
  {
    if (linkyrelay_config.arr_virtual[index].relay == 0) strcpy_P (str_text, PSTR ("&nbsp;")); else itoa (linkyrelay_config.arr_virtual[index].relay, str_text, 10);
    WSContentSend_P (PSTR ("<div style='width:10%%;padding:0px;'>%s</div>\n"), str_text);
  }
  WSContentSend_P (PSTR ("</div>\n"));

  // section end
  WSContentSend_P (PSTR ("</div>\n"));
}

#endif    // USE_TELEINFO

#endif    // USE_WEBSERVER

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xdrv120 (uint32_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
    case FUNC_INIT:
      LinkyRelayInit ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kLinkyRelayCommands, LinkyRelayCommand);
      break;
   case FUNC_EVERY_SECOND:
      LinkyRelayEverySecond ();
      break;

#ifndef USE_TELEINFO
    case FUNC_MQTT_SUBSCRIBE:
      LinkyRelayMqttSubscribe ();
      break;
    case FUNC_MQTT_DATA:
      result = LinkyRelayMqttData ();
      break;
#endif    // USE_TELEINFO

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_BUTTON:
      WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>Configure Linky Relay</button></form></p>\n"), PSTR (D_PAGE_LINKY_RELAY_CONFIG));
      break;
    case FUNC_WEB_ADD_HANDLER:
      Webserver->on (F ("/" D_PAGE_LINKY_RELAY_CONFIG), LinkyRelayWebConfigure);
      break;

#ifndef USE_TELEINFO
    case FUNC_WEB_SENSOR:
      LinkyRelayWebSensor ();
      break;
#endif    // USE_TELEINFO

#endif    // USE_WEBSERVER
  }
  
  return result;
}

#endif    // USE_RELAY_LINKY
