/*
  xdrv_98_esp32_board.ino - Interface for ESP32 board selection
  
  Copyright (C) 2020  Nicolas Bernaerts

    08/11/2020 - v1.0 - Creation
    15/03/2021 - v1.1 - Detect model board in config page
    10/04/2021 - v1.2 - Remove use of String to avoid heap fragmentation 
    02/05/2021 - v1.3 - Remove JSON data (now handled by IPAddress) 
    22/08/2022 - v1.4 - Renamed to xdrv_98_esp32_board.ino

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

#ifndef FIRMWARE_SAFEBOOT
#ifdef ESP32
#ifdef USE_ETHERNET

/*************************************************\
 *               Variables
\*************************************************/

// declare ESP32 boards driver
#define XDRV_98              98

// commands
#define D_CMND_BOARD         "board"

// strings
#define D_BOARD              "ESP32 board"
#define D_BOARD_CONFIG       "Configure ESP32"
#define D_BOARD_CONNEXION    "Connexion"
#define D_BOARD_WIFI         "Wifi"
#define D_BOARD_ETHERNET     "Ethernet"

// web URL
const char D_PAGE_BOARD_CONFIG[] PROGMEM = "/board";

// form strings
//const char BOARD_FORM_START[] PROGMEM  = "";
//const char BOARD_FORM_STOP[] PROGMEM   = "";
//const char BOARD_FIELD_START[] PROGMEM = "";
//const char BOARD_FIELD_STOP[] PROGMEM  = "</fieldset></p><br>\n";
//const char BOARD_INPUT_TEXT[] PROGMEM  = "";


// ESP32 ethernet - list
enum ESP32BoardList { ETH_NONE, ETH_OLIMEX_ESP32_PoE, ETH_WESP32, ETH_WT32_ETH01, ETH_MAX };
const char kESP32BoardName[] PROGMEM = "ESP32-Dev-Kit|Olimex ESP32-PoE|wESP32|WT32-ETH01|";

// ESP32 ethernet - specific parameters
const uint8_t arr_eth_enable[] PROGMEM = { 0, 1, 1, 1 };
const uint8_t arr_eth_type[]   PROGMEM = { 0, ETH_PHY_LAN8720, ETH_PHY_LAN8720, ETH_PHY_LAN8720 };
const uint8_t arr_eth_clock[]  PROGMEM = { 0, ETH_CLOCK_GPIO17_OUT, ETH_CLOCK_GPIO0_IN, ETH_CLOCK_GPIO0_IN };
const uint8_t  arr_eth_addr[]  PROGMEM = { 0, 0, 0, 1 };

// ESP32 ethernet - templates
const char board_tmpl0[] PROGMEM = "{\"NAME\":\"ESP32-Dev-Kit\",\"GPIO\":[1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,0,1,1,1,0,0,0,0,1,1,1,1,1,0,0,1],\"FLAG\":0,\"BASE\":1}";
const char board_tmpl1[] PROGMEM = "{\"NAME\":\"Olimex ESP32-PoE\",\"GPIO\":[1,1,1,1,1,1,0,0,5536,1,1,1,1,0,5600,0,0,0,0,5568,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,1],\"FLAG\":0,\"BASE\":1}";
const char board_tmpl2[] PROGMEM = "{\"NAME\":\"wESP32\",\"GPIO\":[0,0,1,0,1,1,0,0,1,1,1,1,5568,5600,1,0,0,0,0,1,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,1],\"FLAG\":0,\"BASE\":1}";
const char board_tmpl3[] PROGMEM = "{\"NAME\":\"WT32-ETH01\",\"GPIO\":[1,1,1,1,1,1,0,0,1,0,1,1,3840,576,5600,0,0,0,0,5568,0,0,0,0,0,0,0,0,1,1,0,1,1,0,0,1],\"FLAG\":0,\"BASE\":1}";
const char* const arr_board_tmpl[] PROGMEM = { board_tmpl0, board_tmpl1, board_tmpl2, board_tmpl3 };

// init procedure
void ESP32BoardInit ()
{
  int  index;
  char str_board[32];
  char str_text[32];

  // get board template name and index
  strcpy (str_board, SettingsText(SET_TEMPLATE_NAME));
  index = GetCommandCode (str_text, sizeof(str_text), str_board, kESP32BoardName);

  // if template name is registered, set technical parameters
  if (index >= 0)
  {
    Settings->flag4.network_ethernet = arr_eth_enable[index];
    Settings->eth_address  = arr_eth_addr[index];
    Settings->eth_type     = arr_eth_type[index];
    Settings->eth_clk_mode = arr_eth_clock[index];
  }
}

void ESP32BoardEverySecond ()
{
  // 10 seconds after boot, if ethernet has got an IP and wifi enable : disable wifi
  if ((TasmotaGlobal.uptime > 10) && (Settings->flag4.network_wifi == 1) && (0 != (uint32_t)EthernetLocalIP ()))
  {
    // disable wifi for next reboot
    Settings->flag4.network_wifi = 0;
    AddLog (LOG_LEVEL_INFO, PSTR ("ETH: Ethernet got %s, disabling wifi"), EthernetLocalIP ().toString ().c_str ());
  }

  // else, 10 seconds after boot, if ethernet has no IP and wifi is disabled : enable wifi
  else if ((TasmotaGlobal.uptime > 10) && (Settings->flag4.network_wifi == 0) && (0 == (uint32_t)EthernetLocalIP ()))
  {
    // enable wifi for next reboot
    Settings->flag4.network_wifi = 1;
    AddLog (LOG_LEVEL_INFO, PSTR ("ETH: Ethernet got no IP, enabling Wifi"));
  }

  // else if fixed IP is defined, wifi is not using it, ethernet is different : set fixed IP to ethernet
  else if ((Settings->ipv4_address[0] != 0) && (WiFi.localIP () != Settings->ipv4_address[0]) && (EthernetLocalIP () != Settings->ipv4_address[0]) && (EthernetLocalIP () != 0))
  {
    // configure ethernet adapter with fixed IP
    ETH.config (Settings->ipv4_address[0], Settings->ipv4_address[1], Settings->ipv4_address[2], Settings->ipv4_address[3]);

    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("ETH: IP changed to %s"), EthernetLocalIP ().toString ().c_str ());
  }
}

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

// append network adapter info to main page
void ESP32BoardWebSensor ()
{
  // display active network adapter
  if (EthernetLocalIP () > 0) WSContentSend_P (PSTR("{s}%s{m}%s{e}"), D_BOARD_CONNEXION, D_BOARD_ETHERNET);
  if (WiFi.localIP () > 0) WSContentSend_P (PSTR("{s}%s{m}%s{e}"), D_BOARD_CONNEXION, D_BOARD_WIFI);
}

// append Teleinfo configuration button to configuration page
void ESP32BoardWebConfigButton ()
{
  // heater and meter configuration button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>"), D_PAGE_BOARD_CONFIG, D_BOARD_CONFIG);
}

// board config page
void ESP32BoardWebPageConfig ()
{
  int  index;
  char str_text[256];
  char str_board[32];
  char str_active[16];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess ()) return;

  // if board type is provided, set ETH parameters accordingly
  WebGetArg (D_CMND_BOARD, str_text, sizeof(str_text));
  if (strlen (str_text) > 0)
  {
    // if adapter is referenced
    index = atoi (str_text);
    if (index < ETH_MAX)
    {
      // set technical parameters
      Settings->flag4.network_ethernet = arr_eth_enable[index];
      Settings->eth_address  = arr_eth_addr[index];
      Settings->eth_type     = arr_eth_type[index];
      Settings->eth_clk_mode = arr_eth_clock[index];

      // apply template as default one
      snprintf_P (str_text, sizeof(str_text), PSTR (D_CMND_BACKLOG " " D_CMND_TEMPLATE " %s; %s 0"), arr_board_tmpl[index], D_CMND_MODULE);
      ExecuteWebCommand (str_text, SRC_WEBGUI);

      // if some data has been updated, ask for reboot
      WebRestart(1);
    }
  }

  // beginning of form
  WSContentStart_P (D_BOARD_CONFIG);
  WSContentSendStyle ();
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_PAGE_BOARD_CONFIG);

  // get board template name
  strcpy (str_board, SettingsText (SET_TEMPLATE_NAME));

  // board selection
  WSContentSend_P (PSTR ("<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>\n"), D_BOARD);
  for (index = 0; index < ETH_MAX; index ++) 
  {
    strcpy (str_text, "");
    strcpy (str_active, "");
    GetTextIndexed (str_text, sizeof (str_text), index, kESP32BoardName);
    if (strcmp (str_board, str_text) == 0) strcpy (str_active, "checked");
    WSContentSend_P (PSTR ("<p><input type='radio' name='%s' value='%d' %s>%s</p>\n"), D_CMND_BOARD, index, str_active, str_text);
  }
  WSContentSend_P (PSTR ("</fieldset></p><br>\n"));
   
  // save button
  WSContentSend_P (PSTR ("<button name='save' type='submit' class='button bgrn'>%s</button>"), D_SAVE);
  WSContentSend_P (PSTR ("</form>\n"));

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

#endif  // USE_WEBSERVER

/***************************************\
 *              Interface
\***************************************/

// teleinfo sensor
bool Xdrv98 (uint8_t function)
{
  // swtich according to context
  switch (function) 
  {
    case FUNC_INIT:
      ESP32BoardInit ();
      break;
    case FUNC_EVERY_SECOND:
      ESP32BoardEverySecond ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      ESP32BoardWebSensor ();
      break;
    case FUNC_WEB_ADD_HANDLER:
      // pages
      Webserver->on (FPSTR (D_PAGE_BOARD_CONFIG), ESP32BoardWebPageConfig);
      break;
    case FUNC_WEB_ADD_BUTTON:
      ESP32BoardWebConfigButton ();
      break;
#endif  // USE_WEBSERVER

  }
  return false;
}

#endif     // USE_ETHERNET
#endif     // ESP32
#endif     // FIRMWARE_SAFEBOOT


