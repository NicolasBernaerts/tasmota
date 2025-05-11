/*
  user_config_override.h - user configuration overrides my_user_config.h for Tasmota

  Copyright (C) 2022  Theo Arends, Nicolas Bernaerts

  Gazpar impulse connector should be declared as Input 1

  Version history :
    28/04/2022 - v1.0 - Creation
    06/11/2022 - v1.1 - Rename to XSNS_119
    04/12/2022 - v1.2 - Add graphs
    15/05/2023 - v1.3 - Rewrite CSV file access
    05/05/2024 - v2.0 - Use Tasmota native FTP
                        Domoticz, Home Asisstant and Homie auto-discovery
    10/05/2024 - v3.0 - Based on Tasmota 14.6.0
                        Use interrupt for meter pulse detection
                        Rewrite historisation graphs

  tasmota_template.h has been modified to allow INPUT for Esp8266

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
 *
 ******************************************************************************************************
 * ATTENTION:
 *   - Changes to SECTION1 PARAMETER defines will only override flash settings if you change define CFG_HOLDER.
 *   - Expect compiler warnings when no ifdef/undef/endif sequence is used.
 *   - You still need to update my_user_config.h for major define USE_MQTT_TLS.
 *   - All parameters can be persistent changed online using commands via MQTT, WebConsole or Serial.
\*****************************************************************************************************/

/********************************************\
 *    Teleinfo firmware configuration
\********************************************/

// complementary modules
#define USE_IP_OPTION                       // Add IP and options configuration page
#define USE_TIMEZONE                        // Enable Timezone management
#define USE_GAZPAR                          // Enable Teleinfo

// home management integration
#define USE_GAZPAR_DOMOTICZ                 // Domoticz auto-discovery integration
#define USE_GAZPAR_HASS                     // Home Assistant auto-discovery integration
#define USE_GAZPAR_HOMIE                    // Homie protocol auto-discovery integration

// FTP server credentials
#ifdef USE_FTP
#define USER_FTP          "gazpar"          // FTP server login
#define PW_FTP            "gazpar"          // FTP server password
#endif

// build
#if defined BUILD_ESP32_4M
#define EXTENSION_BUILD   "esp32-4m"

#elif defined BUILD_16M
#define EXTENSION_BUILD   "esp8266-16m"

#elif defined BUILD_4M
#define EXTENSION_BUILD   "esp8266-4m"

#else
#define EXTENSION_BUILD   "other"
#endif

// extension data
#define EXTENSION_NAME    "Gazpar"            // name
#define EXTENSION_AUTHOR  "Nicolas Bernaerts" // author
#define EXTENSION_VERSION "3.0"               // version

// MQTT default
#undef MQTT_HOST
#define MQTT_HOST          "mqtt.local"
#undef MQTT_PORT
#define MQTT_PORT          1883              
#undef MQTT_USER
#define MQTT_USER          ""
#undef MQTT_PASS
#define MQTT_PASS          ""
#undef MQTT_TOPIC
#define MQTT_TOPIC         "gazpar"
#undef MQTT_FULLTOPIC
#define MQTT_FULLTOPIC     "%topic%/%prefix%/"
#undef FRIENDLY_NAME
#define FRIENDLY_NAME      "Gazpar"

// disable serial log
#undef SERIAL_LOG_LEVEL 
#define SERIAL_LOG_LEVEL   LOG_LEVEL_NONE

// ----------------------
// Common ESP8266 & ESP32
// ----------------------

#undef USE_ARDUINO_OTA                        // support for Arduino OTA
#undef USE_WPS                                // support for WPS as initial wifi configuration tool
#undef USE_SMARTCONFIG                        // support for Wifi SmartConfig as initial wifi configuration tool

#undef USE_TASMOTA_DISCOVERY                  // Enable Tasmota discovery
#undef USE_DOMOTICZ                           // Domoticz
#undef USE_HOME_ASSISTANT                     // Home Assistant

#undef USE_MQTT_TLS                           // TLS support won't work as the MQTTHost is not set

#undef USE_KNX                                // KNX IP Protocol Support
#undef USE_KNX_WEB_MENU                       // Enable KNX WEB MENU (+8.3k code, +144 mem)

//#undef USE_WEBSERVER                        // Webserver
#undef USE_EMULATION_HUE                      // Hue Bridge emulation for Alexa (+14k code, +2k mem common)
#undef USE_EMULATION_WEMO                     // Belkin WeMo emulation for Alexa (+6k code, +2k mem common)
#undef USE_CUSTOM                             // Custom features

#undef USE_DISCOVERY                          // Discovery services for both MQTT and web server
#undef WEBSERVER_ADVERTISE                    // Provide access to webserver by name <Hostname>.local/
#undef MQTT_HOST_DISCOVERY                    // Find MQTT host server (overrides MQTT_HOST if found)

#undef USE_TIMERS                             // support for up to 16 timers
#undef USE_TIMERS_WEB                         // support for timer webpage
#undef USE_SUNRISE                            // support for Sunrise and sunset tools

//#undef USE_UNISHOX_COMPRESSION                // Add support for string compression in Rules or Scripts
#define USE_RULES                             // Support for rules
#undef USE_SCRIPT                             // Add support for script (+17k code)

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
#undef USE_SHELLY_DIMMER                      // Add support for Shelly Dimmer (+3k code)
#undef SHELLY_CMDS                            // Add command to send co-processor commands (+0k3 code)
#undef SHELLY_FW_UPGRADE                      // Add firmware upgrade option for co-processor (+3k4 code)

#undef USE_LIGHT                              // Add support for light control
#undef USE_WS2812                             // WS2812 Led string using library NeoPixelBus (+5k code, +1k mem, 232 iram) - Disable by //
#undef USE_MY92X1                             // Add support for MY92X1 RGBCW led controller as used in Sonoff B1, Ailight and Lohas
#undef USE_SM16716                            // Add support for SM16716 RGB LED controller (+0k7 code)
#undef USE_SM2135                             // Add support for SM2135 RGBCW led control as used in Action LSC (+0k6 code)
#undef USE_SONOFF_L1                          // Add support for Sonoff L1 led control
#undef USE_ELECTRIQ_MOODL                     // Add support for ElectriQ iQ-wifiMOODL RGBW LED controller (+0k3 code)
#undef USE_LIGHT_PALETTE                      // Add support for color palette (+0k7 code)
#undef USE_LIGHT_VIRTUAL_CT                   // Add support for Virtual White Color Temperature (+1.1k code)
#undef USE_DGR_LIGHT_SEQUENCE                 // Add support for device group light sequencing (requires USE_DEVICE_GROUPS) (+0k2 code)

#undef USE_COUNTER                            // Enable inputs as counter (+0k8 code)

#undef USE_DS18x20                            // Add support for DS18x20 sensors with id sort, single scan and read retry (+2k6 code)

#undef USE_DHT                                // Disable internal DHT sensor

#undef USE_I2C                                // Disable all I2C sensors and devices

#undef USE_SPI                                // Disable all SPI devices

#undef USE_DISPLAY                            // Add Display support

#undef USE_SERIAL_BRIDGE                      // Disable support for software Serial Bridge (+0k8 code)

#undef USE_ENERGY_SENSOR                      // Disable energy sensors
#undef USE_ENERGY_MARGIN_DETECTION            // Add support for Energy Margin detection (+1k6 code)
#undef USE_ENERGY_POWER_LIMIT                 // Add additional support for Energy Power Limit detection (+1k2 code)
#undef USE_PZEM004T                           // Add support for PZEM004T Energy monitor (+2k code)
#undef USE_PZEM_AC                            // Add support for PZEM014,016 Energy monitor (+1k1 code)
#undef USE_PZEM_DC                            // Add support for PZEM003,017 Energy monitor (+1k1 code)
#undef USE_MCP39F501                          // Add support for MCP39F501 Energy monitor as used in Shelly 2 (+3k1 code)
#undef USE_ENERGY_DUMMY                       // Add support for dummy Energy monitor allowing user values (+0k7 code)
#undef USE_HLW8012                            // Add support for HLW8012, BL0937 or HJL-01 Energy Monitor for Sonoff Pow and WolfBlitz
#undef USE_CSE7766                            // Add support for CSE7766 Energy Monitor for Sonoff S31 and Pow R2
#undef USE_SDM72                              // Add support for Eastron SDM72-Modbus energy monitor (+0k3 code)
#undef USE_SDM120                             // Add support for Eastron SDM120-Modbus energy monitor (+1k1 code)
#undef USE_SDM630                             // Add support for Eastron SDM630-Modbus energy monitor (+0k6 code)
#undef USE_DDS2382                            // Add support for Hiking DDS2382 Modbus energy monitor (+0k6 code)
#undef USE_DDSU666                            // Add support for Chint DDSU666 Modbus energy monitor (+0k6 code)
#undef USE_SOLAX_X1                           // Add support for Solax X1 series Modbus log info (+3k1 code)
#undef USE_LE01MR                             // Add support for F&F LE-01MR Modbus energy monitor (+1k code)
#undef USE_BL09XX                             // Add support for various BL09XX Energy monitor as used in Blitzwolf SHP-10 or Sonoff Dual R3 v2 (+1k6 code)
#undef USE_TELEINFO                           // Add support for Teleinfo via serial RX interface (+5k2 code, +168 RAM + SmartMeter LinkedList Values RAM)
#undef USE_IEM3000                            // Add support for Schneider Electric iEM3000-Modbus series energy monitor (+0k8 code)

#undef USE_IR_REMOTE_FULL
#undef USE_IR_REMOTE                          // Send IR remote commands using library IRremoteESP8266 and ArduinoJson (+4k3 code, 0k3 mem, 48 iram)
#undef USE_IR_SEND_NEC                        // Support IRsend NEC protocol
#undef USE_IR_SEND_RC5                        // Support IRsend Philips RC5 protocol
#undef USE_IR_SEND_RC6                        // Support IRsend Philips RC6 protocol
#undef USE_IR_RECEIVE                         // Support for IR receiver (+7k2 code, 264 iram)

#undef USE_ZIGBEE                             // Enable serial communication with Zigbee CC2530 flashed with ZNP (+49k code, +3k mem)
#undef USE_ZIGBEE_ZNP                         // Enable ZNP protocol, needed for CC2530 based devices
#undef USE_ZIGBEE_EZSP                        // Enable EZSP protocol, needed for EFR32 EmberZNet based devices, like Sonoff Zigbee bridge                                             
#undef USE_ZIGBEE_EEPROM                      // Use the EEPROM from the Sonoff ZBBridge to save Zigbee configuration and data
#undef USE_ZBBRIDGE_TLS                       // TLS support for zbbridge

#undef USE_TM1638                             // Add support for TM1638 switches copying Switch1 .. Switch8 (+1k code)
#undef USE_HX711                              // Add support for HX711 load cell (+1k5 code)
#undef USE_HX711_GUI                          // Add optional web GUI to HX711 as scale (+1k8 code)
#undef USE_TX20_WIND_SENSOR                   // Add support for La Crosse TX20 anemometer (+2k6/0k8 code)
#undef USE_TX23_WIND_SENSOR                   // Add support for La Crosse TX23 anemometer (+2k7/1k code)
#undef USE_WINDMETER                          // Add support for analog anemometer (+2k2 code)
#undef USE_FTC532                             // Add support for FTC532 8-button touch controller (+0k6 code)
#undef USE_RC_SWITCH                          // Add support for RF transceiver using library RcSwitch (+2k7 code, 460 iram)
#undef USE_RF_SENSOR                          // Add support for RF sensor receiver (434MHz or 868MHz) (+0k8 code)
#undef USE_THEO_V2                            // Add support for decoding Theo V2 sensors as documented on https://sidweb.nl using 434MHz RF sensor receiver (+1k4 code)
#undef USE_ALECTO_V2                          // Add support for decoding Alecto V2 sensors like ACH2010, WS3000 and DKW2012 weather stations using 868MHz RF sensor receiver (+1k7 code)
#undef USE_HRE                                // Add support for Badger HR-E Water Meter (+1k4 code)
#undef USE_A4988_STEPPER                      // Add support for A4988/DRV8825 stepper-motor-driver-circuit (+10k5 code)
#undef USE_PROMETHEUS                         // Add support for https://prometheus.io/ metrics exporting over HTTP /metrics endpoint
#undef USE_NEOPOOL                            // Add support for Sugar Valley NeoPool Controller - also known under brands Hidrolife, Aquascenic, Oxilife, Bionet, Hidroniser, UVScenic, 

#undef USE_THERMOSTAT                         // Add support for Thermostat
#undef USE_GPS
#undef USE_WINDMETER
#undef USE_OPENTHERM

#undef USE_TIMEPROP                           // Add support for the timeprop feature (+9k1 code)
#undef USE_PID                                // Add suport for the PID  feature (+11k2 code)

// ----------------------
//    ESP32 specific
// ----------------------

#ifdef ESP32

//#undef USE_ESP32_SENSORS

#define USE_TLS                               // for safeboot and BearSSL
#define USE_MQTT_TLS                          // enable mqtts connexion
#define USE_LIB_SSL_ENGINE

#undef USE_LIGHT                              // Add support for light control
#undef USE_WS2812                             // WS2812 Led string using library NeoPixelBus (+5k code, +1k mem, 232 iram) - Disable by //
#undef USE_ADC                                // Add support for ADC on GPIO32 to GPIO39

#undef USE_BLE_ESP32
#undef USE_MI_ESP32
#undef USE_IBEACON

#undef USE_I2C                                // All I2C sensors and devices
#undef USE_DISPLAY                            // Add Display support

#undef USE_SR04
#undef USE_LVGL

#undef USE_ADC                                // Add support for ADC on GPIO32 to GPIO39

#undef USE_WEBCAM

#undef USE_M5STACK_CORE2
#undef USE_I2S_AUDIO
#undef USE_TTGO_WATCH

#undef USE_ALECTO_V2
#undef USE_RF_SENSOR
#undef USE_HX711
#undef USE_MAX31855

#undef USE_MHZ19
#undef USE_SENSEAIR   

#endif  // ESP32

#endif  // _USER_CONFIG_OVERRIDE_H_
