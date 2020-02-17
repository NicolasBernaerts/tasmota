/*
  xnrg_16_teleinfo.ino - France Teleinfo energy sensor support for Sonoff-Tasmota
  
  Copyright (C) 2019  Nicolas Bernaerts

    05/05/2019 - v1.0 - Creation
    16/05/2019 - v1.1 - Add Tempo and EJP contracts
    08/06/2019 - v1.2 - Handle active and apparent power
    05/07/2019 - v2.0 - Rework with selection thru web interface
    02/01/2020 - v3.0 - Functions rewrite for Tasmota 8.x compatibility
    
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

#ifdef USE_ENERGY_SENSOR
#ifdef USE_TELEINFO

// declare energy driver and teleinfo sensor
#define XSNS_98   98

/*********************************************\
 *               Variables
\*********************************************/

// web configuration page
#define D_PAGE_TELEINFO               "teleinfo"
#define D_CMND_TELEINFO_MODE          "mode"
#define D_WEB_TELEINFO_CHECKED        "checked"

// teleinfo constant
#define TELEINFO_MESSAGE_BUFFER_SIZE  64

// form strings
const char INPUT_MODE_SELECT[] PROGMEM = "<input type='radio' name='%s' id='%d' value='%d' %s>%s";
const char INPUT_FORM_START[] PROGMEM = "<form method='get' action='%s'>";
const char INPUT_FORM_STOP[] PROGMEM = "</form>";
const char INPUT_FIELDSET_START[] PROGMEM = "<fieldset><legend><b>&nbsp;%s&nbsp;</b></legend><br />";
const char INPUT_FIELDSET_STOP[] PROGMEM = "</fieldset><br />";

/*********************************************\
 *               Functions
\*********************************************/

// Show JSON status (for MQTT)
void TeleinfoShowJSON (bool append)
{
  // if JSON is ready
  if (str_teleinfo_json.length () > 0)
  {
    // if we are in append mode, just append teleinfo data to current MQTT message
    if (append == true) snprintf_P (mqtt_data, sizeof(mqtt_data), "%s,%s", mqtt_data, str_teleinfo_json.c_str ());

    // else publish teleinfo message right away 
    else
    { 
      // create message { teleinfo }
      snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("{%s}"), str_teleinfo_json.c_str ());

      // publish full sensor state
      MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));
    }

    // reset teleinfo data
    str_teleinfo_json = "";

    // if overload was detected, reset the trigger
    if (teleinfo_overload == TELEINFO_OVERLOAD_READY) teleinfo_overload = TELEINFO_OVERLOAD_NONE;
  }
}

void TeleinfoEvery250MSecond ()
{
  // if overload has been detected, publish teleinfo data
  if (teleinfo_overload == TELEINFO_OVERLOAD_READY) TeleinfoShowJSON (false);
}

void TeleinfoJSONAppend ()
{
  // publish teleinfo data appended JSON under construction
  TeleinfoShowJSON (true);
}

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

// Teleinfo mode select combo
void TeleinfoWebSelectMode ()
{
  uint16_t actual_mode;
  String   str_checked;

  // get mode
  actual_mode = TeleinfoGetMode ();

  // selection : disabled
  str_checked = "";
  if (actual_mode == 0) str_checked = D_WEB_TELEINFO_CHECKED;
  WSContentSend_P (INPUT_MODE_SELECT, D_CMND_TELEINFO_MODE, 0, 0, str_checked.c_str (), D_TELEINFO_DISABLED);
  WSContentSend_P (PSTR ("<br/>"));

  if (teleinfo_configured == true)
  {
    // selection : 1200 baud
    str_checked = "";
    if (actual_mode == 1200) str_checked = D_WEB_TELEINFO_CHECKED;
    WSContentSend_P (INPUT_MODE_SELECT, D_CMND_TELEINFO_MODE, 1200, 1200, str_checked.c_str (), D_TELEINFO_1200);
    WSContentSend_P (PSTR ("<br/>"));

    // selection : no frost
    str_checked = "";
    if (actual_mode == 9600) str_checked = D_WEB_TELEINFO_CHECKED;
    WSContentSend_P (INPUT_MODE_SELECT, D_CMND_TELEINFO_MODE, 9600, 9600, str_checked.c_str (), D_TELEINFO_9600);
    WSContentSend_P (PSTR ("<br/>"));
  }
}

// Teleinfo configuration button
void TeleinfoWebButton ()
{
  // heater and meter configuration button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>"), D_PAGE_TELEINFO, D_TELEINFO_CONFIG);
}

// Teleinfo web page
void TeleinfoWebPage ()
{
  char argument[TELEINFO_MESSAGE_BUFFER_SIZE];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // get teleinfo mode according to MODE parameter
  if (WebServer->hasArg(D_CMND_TELEINFO_MODE))
  {
    WebGetArg (D_CMND_TELEINFO_MODE, argument, TELEINFO_MESSAGE_BUFFER_SIZE);
    TeleinfoSetMode ((uint16_t) atoi (argument)); 
  }

  // beginning of form
  WSContentStart_P (D_TELEINFO_CONFIG);
  WSContentSendStyle ();
  WSContentSend_P (INPUT_FORM_START, D_PAGE_TELEINFO);

  // mode selection
  WSContentSend_P (INPUT_FIELDSET_START, D_TELEINFO_MODE);
  TeleinfoWebSelectMode ();

  // end of form
  WSContentSend_P (INPUT_FIELDSET_STOP);
  WSContentSend_P (PSTR ("<button name='save' type='submit' class='button bgrn'>%s</button>"), D_SAVE);
  WSContentSend_P (INPUT_FORM_STOP);

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

// append Teleinfo state to main page
bool TeleinfoWebSensor ()
{
  String strPower[3];

  strPower [0] = String (Energy.apparent_power[0], 0);
  strPower [1] = String (Energy.apparent_power[1], 0);
  strPower [2] = String (Energy.apparent_power[2], 0);

  // display apparent power
  if (teleinfo_phase == 1) WSContentSend_PD (PSTR("<tr><th>%s</th><td>%s</td></tr>"), D_TELEINFO_POWER, strPower[0].c_str ());
  else if (teleinfo_phase == 3) WSContentSend_PD (PSTR("<tr><th>%s</th><td>%s / %s / %s VA</td></tr>"), D_TELEINFO_POWER, strPower[0].c_str (), strPower[1].c_str (), strPower[2].c_str ());

  // display frame counter
  WSContentSend_PD (PSTR("<tr><th>%s</th><td>%d</td></tr>"), D_TELEINFO_COUNTER, teleinfo_framecount);
}

#endif  // USE_WEBSERVER

/*******************************************\
 *                 Interface
\*******************************************/

// teleinfo sensor
bool Xsns98 (uint8_t function)
{
  bool result = false;
  
  // swtich according to context
  switch (function) 
  {
    case FUNC_EVERY_250_MSECOND:
      TeleinfoEvery250MSecond ();
      break;
    case FUNC_JSON_APPEND:
      if (teleinfo_enabled == true) TeleinfoJSONAppend ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      WebServer->on ("/" D_PAGE_TELEINFO, TeleinfoWebPage);
      break;
    case FUNC_WEB_ADD_BUTTON:
      TeleinfoWebButton ();
      break;
    case FUNC_WEB_SENSOR:
      TeleinfoWebSensor ();
      break;
#endif  // USE_WEBSERVER

  }
  return result;
}

#endif   // USE_TELEINFO
#endif   // USE_ENERGY_SENSOR
