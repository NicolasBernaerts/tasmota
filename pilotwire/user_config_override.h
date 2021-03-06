/*
  user_config_override.h - user configuration overrides my_user_config.h for Tasmota

  Copyright (C) 2019/2020  Theo Arends, Nicolas Bernaerts
    05/04/2019 - v1.0   - Creation
    12/04/2019 - v1.1   - Save settings in Settings.weight... variables
    10/06/2019 - v2.0   - Complete rewrite to add web management
    25/06/2019 - v2.1   - Add DHT temperature sensor and settings validity control
    05/07/2019 - v2.2   - Add embeded icon
    05/07/2019 - v3.0   - Add max power management with automatic offload
                          Save power settings in Settings.energy... variables
    12/12/2019 - v3.1   - Add public configuration page http://.../control
    30/12/2019 - v4.0   - Functions rewrite for Tasmota 8.x compatibility
    06/01/2019 - v4.1   - Handle offloading with finite state machine
    09/01/2019 - v4.2   - Separation between Offloading driver and Pilotwire sensor
    15/01/2020 - v5.0   - Separate temperature driver and add remote MQTT sensor
    05/02/2020 - v5.1   - Block relay command if not coming from a mode set
    21/02/2020 - v5.2   - Add daily temperature graph
    24/02/2020 - v5.3   - Add control button to main page
    27/02/2020 - v5.4   - Add target temperature and relay state to temperature graph
    01/03/2020 - v5.5   - Add timer management with Outside mode
    13/03/2020 - v5.6   - Add time to graph
    05/04/2020 - v5.7   - Add timezone management
    18/04/2020 - v6.0   - Handle direct connexion of heater in addition to pilotwire
    16/05/2020 - v6.1   - Add /json page and outside mode in JSON
    20/05/2020 - v6.2   - Add configuration for first NTP server
    26/05/2020 - v6.3   - Add Information JSON page
    07/07/2020 - v6.4   - Change MQTT subscription and parameters
    22/08/2020 - v6.4.1 - Save offload config using new Settings text
                          Add restart after offload configuration
                          Handle out of range values during first flash
    24/08/2020 - v6.5   - Add status icon to Web UI 
    12/09/2020 - v6.6   - Add offload icon status and based on Tasmota 8.4 
    19/09/2020 - v6.7   - Based on Offload 2.0 
    10/10/2020 - v6.8   - Handle graph with javascript auto update 
    14/10/2020 - v6.9   - Serve icons thru web server 
    18/10/2020 - v6.10  - Handle priorities as list of device types
                          Add randomisation to reconnexion
    04/11/2020 - v6.11  - Tasmota 9.0 compatibility
    11/11/2020 - v6.12 - Update to Offload v2.5

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

#ifndef _USER_CONFIG_OVERRIDE_H_
#define _USER_CONFIG_OVERRIDE_H_

// force the compiler to show a warning to confirm that this file is included
//#warning **** user_config_override.h: Using Settings from this File ****

/*****************************************************************************************************\
 * USAGE:
 *   To modify the stock configuration without changing the my_user_config.h file:
 *   (1) copy this file to "user_config_override.h" (It will be ignored by Git)
 *   (2) define your own settings below
 *   (3) for platformio:
 *         define USE_CONFIG_OVERRIDE as a build flags.
 *         ie1 : export PLATFORMIO_BUILD_FLAGS='-DUSE_CONFIG_OVERRIDE'
 *       for Arduino IDE:
 *         enable define USE_CONFIG_OVERRIDE in my_user_config.h
 ******************************************************************************************************
 * ATTENTION:
 *   - Changes to SECTION1 PARAMETER defines will only override flash settings if you change define CFG_HOLDER.
 *   - Expect compiler warnings when no ifdef/undef/endif sequence is used.
 *   - You still need to update my_user_config.h for major define USE_MQTT_TLS.
 *   - All parameters can be persistent changed online using commands via MQTT, WebConsole or Serial.
\*****************************************************************************************************/

/********************************************\
 *    Pilotwire firmware configuration
\********************************************/

#define USE_INFOJSON                          // Add support for Information JSON page
#define USE_TIMEZONE                          // Add support for Timezone management
#define USE_OFFLOADING                        // Add support for MQTT maximum power offloading
#define USE_TEMPERATURE_MQTT                  // Add support for MQTT temperature sensor
#define USE_PILOTWIRE                         // Add support for France Pilotwire protocol for electrical heaters

#define EXTENSION_VERSION "6.12"              // version
#define EXTENSION_NAME    "Pilotwire"         // name
#define EXTENSION_AUTHOR  "Nicolas Bernaerts" // author

// MQTT default
#undef MQTT_HOST
#define MQTT_HOST          "openhab"
#undef MQTT_PORT
#define MQTT_PORT          1883              
#undef MQTT_USER
#define MQTT_USER          ""
#undef MQTT_PASS
#define MQTT_PASS          ""
#undef MQTT_TOPIC
#define MQTT_TOPIC         "filpilote_%06X"
#undef MQTT_FULLTOPIC
#define MQTT_FULLTOPIC     "%topic%/%prefix%/"
#undef FRIENDLY_NAME
#define FRIENDLY_NAME      "Fil Pilote"

// default template : Sonoff Basic with non inverted LED
#define USER_TEMPLATE "{\"NAME\":\"Sonoff Basic Pilotwire\",\"GPIO\":[32,1,1,1,1,0,0,0,224,288,1,0,0,0],\"FLAG\":0,\"BASE\":1}"
#undef FALLBACK_MODULE
#define FALLBACK_MODULE  USER_MODULE

//#undef USE_ENERGY_SENSOR                    // Disable energy sensors
#undef USE_ARDUINO_OTA                        // support for Arduino OTA
#undef USE_WPS                                // support for WPS as initial wifi configuration tool
#undef USE_SMARTCONFIG                        // support for Wifi SmartConfig as initial wifi configuration tool
#undef USE_DOMOTICZ                           // Domoticz
#undef USE_HOME_ASSISTANT                     // Home Assistant
#undef USE_MQTT_TLS                           // TLS support won't work as the MQTTHost is not set
#undef USE_KNX                                // KNX IP Protocol Support
//#undef USE_WEBSERVER                        // Webserver
#undef USE_EMULATION_HUE                      // Hue Bridge emulation for Alexa (+14k code, +2k mem common)
#undef USE_EMULATION_WEMO                     // Belkin WeMo emulation for Alexa (+6k code, +2k mem common)
#undef USE_CUSTOM                             // Custom features
#undef USE_DISCOVERY                          // Discovery services for both MQTT and web server
//#undef WEBSERVER_ADVERTISE                  // Provide access to webserver by name <Hostname>.local/
#undef MQTT_HOST_DISCOVERY                    // Find MQTT host server (overrides MQTT_HOST if found)
#define USE_TIMERS_WEB                        // timer web config
//#undef USE_TIMERS                           // support for up to 16 timers
//#undef USE_TIMERS_WEB                       // support for timer webpage
//#undef USE_SUNRISE                          // support for Sunrise and sunset tools
//#undef SUNRISE_DAWN_ANGLE DAWN_NORMAL       // Select desired Dawn Angle from
#undef USE_RULES                              // Disable support for rules

#undef ROTARY_V1                              // Add support for Rotary Encoder as used in MI Desk Lamp (+0k8 code)
#undef USE_SONOFF_RF                          // Add support for Sonoff Rf Bridge (+3k2 code)
#undef USE_RF_FLASH                           // Add support for flashing the EFM8BB1 chip on the Sonoff RF Bridge.
#undef USE_SONOFF_SC                          // Add support for Sonoff Sc (+1k1 code)
#undef USE_TUYA_MCU                           // Add support for Tuya Serial MCU
#undef USE_ARMTRONIX_DIMMERS                  // Add support for Armtronix Dimmers (+1k4 code)
#undef USE_PS_16_DZ                           // Add support for PS-16-DZ Dimmer (+2k code)
#undef USE_SONOFF_IFAN                        // Add support for Sonoff iFan02 and iFan03 (+2k code)
#undef USE_BUZZER                             // Add support for a buzzer (+0k6 code)
#undef USE_ARILUX_RF                          // Add support for Arilux RF remote controller (+0k8 code, 252 iram (non 2.3.0))
#undef USE_SHUTTER                            // Add Shutter support for up to 4 shutter with different motortypes (+11k code)
#undef USE_DEEPSLEEP                          // Add support for deepsleep (+1k code)
#undef USE_EXS_DIMMER                         // Add support for ES-Store WiFi Dimmer (+1k5 code)
#undef USE_DEVICE_GROUPS                      // Add support for device groups (+5k5 code)
#undef USE_DEVICE_GROUPS_SEND                 // Add support for the DevGroupSend command (+0k6 code)
#undef USE_PWM_DIMMER                         // Add support for MJ-SD01/acenx/NTONPOWER PWM dimmers (+2k2 code, DGR=0k4)
#undef USE_PWM_DIMMER_REMOTE                  // Add support for remote switches to PWM Dimmer (requires USE_DEVICE_GROUPS) (+0k9 code)
#undef USE_SONOFF_D1                          // Add support for Sonoff D1 Dimmer (+0k7 code)
#undef USE_WS2812                             // WS2812 Led string using library NeoPixelBus (+5k code, +1k mem, 232 iram) - Disable by //
#undef USE_MY92X1                             // Add support for MY92X1 RGBCW led controller as used in Sonoff B1, Ailight and Lohas
#undef USE_SM16716                            // Add support for SM16716 RGB LED controller (+0k7 code)
#undef USE_SM2135                             // Add support for SM2135 RGBCW led control as used in Action LSC (+0k6 code)
#undef USE_SONOFF_L1                          // Add support for Sonoff L1 led control
#undef USE_ELECTRIQ_MOODL                     // Add support for ElectriQ iQ-wifiMOODL RGBW LED controller (+0k3 code)
#undef USE_LIGHT_PALETTE                      // Add support for color palette (+0k7 code)
#undef USE_DGR_LIGHT_SEQUENCE                 // Add support for device group light sequencing (requires USE_DEVICE_GROUPS) (+0k2 code)

#undef USE_COUNTER                            // Enable inputs as counter (+0k8 code)
//#undef USE_DS18x20                          // Add support for DS18x20 sensors with id sort, single scan and read retry (+2k6 code)
//#undef USE_DHT                              // Disable internal DHT sensor

#undef USE_I2C                                // Disable all I2C sensors and devices
#undef USE_SPI                                // Disable all SPI devices

#undef USE_SERIAL_BRIDGE                      // Disable support for software Serial Bridge (+0k8 code)

#undef USE_ENERGY_MARGIN_DETECTION            // Add support for Energy Margin detection (+1k6 code)
#undef USE_ENERGY_POWER_LIMIT                 // Add additional support for Energy Power Limit detection (+1k2 code)
#undef USE_PZEM004T                           // Add support for PZEM004T Energy monitor (+2k code)
#undef USE_PZEM_AC                            // Add support for PZEM014,016 Energy monitor (+1k1 code)
#undef USE_PZEM_DC                            // Add support for PZEM003,017 Energy monitor (+1k1 code)
#undef USE_MCP39F501                          // Add support for MCP39F501 Energy monitor as used in Shelly 2 (+3k1 code)

#undef USE_IR_REMOTE                          // Send IR remote commands using library IRremoteESP8266 and ArduinoJson (+4k3 code, 0k3 mem, 48 iram)
#undef USE_IR_SEND_NEC                        // Support IRsend NEC protocol
#undef USE_IR_SEND_RC5                        // Support IRsend Philips RC5 protocol
#undef USE_IR_SEND_RC6                        // Support IRsend Philips RC6 protocol
#undef USE_IR_RECEIVE                         // Support for IR receiver (+7k2 code, 264 iram)
#undef USE_ZIGBEE_ZNP                         // Enable ZNP protocol, needed for CC2530 based devices

// add support to MQTT events subscription
#ifndef SUPPORT_MQTT_EVENT
#define SUPPORT_MQTT_EVENT
#endif

#endif  // _USER_CONFIG_OVERRIDE_H_

