/*
  xdrv_99_01_ethernet.ino - Interface for ESP32 ethernet board management
  
  Copyright (C) 2020  Nicolas Bernaerts

    08/11/2020 - v1.0 - Creation
    15/03/2021 - v1.1 - Detect model board in config page
    10/04/2021 - v1.2 - Remove use of String to avoid heap fragmentation 
    02/05/2021 - v1.3 - Remove JSON data (now handled by IPAddress) 
    22/08/2022 - v1.4 - Renamed to xdrv_98_esp32_board.ino
    28/03/2026 - v2.0 - Renamed to xdrv_99_01_ethernet.ino
                        Add Wifi / ethernet auto switch
                        Add Ethernet icon
    08/04/2026 - v3.0 - Remove board selection, now handled thru autoconf

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

#ifdef USE_ETHERNET

/*************************************************\
 *                     Data
\*************************************************/

#define ETH_DELAY_CONFIG          10     // delay between network config (s)

static struct {
  uint32_t timestamp = 0;
} misc_ethernet;

/**************************************************\
 *                  Call back
\**************************************************/

// init procedure
void EthernetSwitchInit ()
{
  // set initial ethernet config delay
  misc_ethernet.timestamp = TasmotaGlobal.uptime + ETH_DELAY_CONFIG;
}

// Show JSON status (for MQTT)
void EthernetSwitchAppendSensor ()
{
  // ignore if not teleperiod
  if (TasmotaGlobal.tele_period != 0) return; 

  // ethernet MAC
  if (Settings->flag4.network_ethernet == 1) ResponseAppend_P (PSTR (",\"Eth\":\"%s\""), EthernetMacAddress ().c_str ());
}

void EthernetSwitchEverySecond ()
{
  // ignore if ethernet disabled or less than 10 sec after last update
  if (Settings->flag4.network_ethernet == 0) return;
  if (TasmotaGlobal.uptime < misc_ethernet.timestamp) return;

  // next check in 10 sec.
  misc_ethernet.timestamp = TasmotaGlobal.uptime + ETH_DELAY_CONFIG;

  // if ethernet connected
  if (EthernetHasIP ())
  {
    // if wifi enabled, disable wifi
    if (Settings->flag4.network_wifi == 1)
    {
      // disconnect wifi, delete SDK wifi config and calibrate data
      Settings->flag4.network_wifi = 0;
      WiFi.disconnect (false);   
      SettingsErase (4);

      // store empty ssid/wpa into sdk storage
      ETS_UART_INTR_DISABLE ();
#ifdef ESP8266
      wifi_station_disconnect ();
#else
      WiFi.disconnect (true, true);
#endif
      ETS_UART_INTR_ENABLE ();

      // flush anything in the network buffers and log
      delay(100);
      AddLog (LOG_LEVEL_INFO, PSTR ("ETH: Ethernet got %s, disabling wifi"), ETH.localIP ().toString ().c_str ());
    }             
  }

  // else if wifi not connected
  else if (!WifiHasIP ())
  {
    // if wifi disabled, enable wifi
    if (Settings->flag4.network_wifi == 0)
    {
      // enable wifi and log
      Settings->flag4.network_wifi = 1;
      AddLog (LOG_LEVEL_INFO, PSTR ("ETH: Ethernet disconnected, enabling Wifi"));

#ifdef ESP32
      WifiConnect ();
#else   // ESP8266
      WifiEnable ();
#endif  // ESP32
    }
  }
}

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

// main page display style
void EthernetSwitchWebMainButton ()
{
  // ignore if ethernet disabled
  if (Settings->flag4.network_ethernet == 0) return;

  // ethernet icon style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("div#l1 .eth {padding:0px;width:20px;height:20px;-webkit-mask-repeat:no-repeat;mask-repeat:no-repeat;-webkit-mask-size:100%%;mask-size:100%%;-webkit-mask-image:var(--svg);mask-image:var(--svg);--svg:url(\"data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 13 14'><path d='M12.074,0H1.165C0.523,0,0,0.522,0,1.164v10.909c0,0.643,0.522,1.165,1.165,1.165h10.909 c0.643,0,1.165-0.522,1.165-1.165V1.164C13.239,0.522,12.717,0,12.074,0z M10.64,10.405c0,0.367-0.338,0.665-0.756,0.665H8.562 V7.317H7.915v3.754H6.944V7.317H6.297v3.754H5.272V7.317H4.625v3.754H3.354c-0.417,0-0.756-0.298-0.756-0.665V4.661 c0-0.367,0.338-0.665,0.756-0.665h0.928v-1.45c0-0.209,0.193-0.379,0.431-0.379H8.44c0.237,0,0.43,0.17,0.43,0.379v1.45h1.014 c0.418,0,0.756,0.298,0.756,0.665C10.64,4.661,10.64,10.405,10.64,10.405z'/></svg>\");}\n"));
  WSContentSend_P (PSTR ("div#l1 .on {background-color:#0b0;}\n"));
  WSContentSend_P (PSTR ("div#l1 .off {background-color:#666;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));
}

// append network adapter info to main page
void EthernetSwitchWebStatusLeft ()
{
  char str_text[8];

  // ignore if ethernet disabled
  if (Settings->flag4.network_ethernet == 0) return;

  // display ethernet icon
  if ((uint32_t)EthernetLocalIP () != 0) strcpy_P (str_text, PSTR ("on")); else strcpy_P (str_text, PSTR ("off"));
  WSContentSend_P (PSTR ("<div class='eth %s'></div>\n"), str_text);
}

#endif  // USE_WEBSERVER

/***************************************\
 *              Interface
\***************************************/

bool XdrvEthernetSwitch (uint32_t function)
{
  // swtich according to context
  switch (function) 
  {
    case FUNC_INIT:
      EthernetSwitchInit ();
      break;

    case FUNC_EVERY_SECOND:
      EthernetSwitchEverySecond ();
      break;

    case FUNC_JSON_APPEND:
      EthernetSwitchAppendSensor ();
      break;


#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_MAIN_BUTTON:
      EthernetSwitchWebMainButton ();
      break;
#endif  // USE_WEBSERVER
      
  }
  return false;
}

#endif     // USE_ETHERNET
