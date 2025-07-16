/*
  user_config_override.h - user configuration overrides my_user_config.h for Tasmota

  Copyright (C) 2019-2024  Theo Arends, Nicolas Bernaerts

    05/05/2019 - v1.0  - Creation
    16/05/2019 - v1.1  - Add Tempo and EJP contracts
    08/06/2019 - v1.2  - Handle active and apparent power
    05/07/2019 - v2.0  - Rework with selection thru web interface
    02/01/2020 - v3.0  - Functions rewrite for Tasmota 8.x compatibility
    05/02/2020 - v3.1  - Add support for 3 phases meters
    14/03/2020 - v3.2  - Add apparent power graph
    05/04/2020 - v3.3  - Add Timezone management
    13/05/2020 - v3.4  - Add overload management per phase
    19/05/2020 - v3.6  - Add configuration for first NTP server
    26/05/2020 - v3.7  - Add Information JSON page
    29/07/2020 - v3.8  - Add Meter section to JSON
    05/08/2020 - v4.0  - Major code rewrite, JSON section is now TIC, numbered like new official Teleinfo module
                         Web sensor display update
    18/09/2020 - v4.1  - Based on Tasmota 8.4
    07/10/2020 - v5.0  - Handle different graph periods and javascript auto update
    18/10/2020 - v5.1  - Expose icon on web server
    25/10/2020 - v5.2  - Real time graph page update
    30/10/2020 - v5.3  - Add TIC message page
    02/11/2020 - v5.4  - Tasmota 9.0 compatibility
    09/11/2020 - v6.0  - Handle ESP32 ethernet devices with board selection
    11/11/2020 - v6.1  - Add data.json page
    20/11/2020 - v6.2  - Checksum bug
    29/12/2020 - v6.3  - Strengthen message error control
    25/02/2021 - v7.0  - Prepare compatibility with TIC standard
                         Add power status bar
    05/03/2021 - v7.1  - Correct bug on hardware energy counter
    08/03/2021 - v7.2  - Handle voltage and checksum for horodatage
    12/03/2021 - v7.3  - Use average / overload for graph
    15/03/2021 - v7.4  - Change graph period parameter
    21/03/2021 - v7.5  - Support for TIC Standard
    29/03/2021 - v7.6  - Add voltage graph
    04/04/2021 - v7.7  - Change in serial port & graph height selection
                         Handle number of indexes according to contract
                         Remove use of String to avoid heap fragmentation 
    14/04/2021 - v7.8  - Calculate Cos phi and Active power (W)
    21/04/2021 - v8.0  - Fixed IP configuration and change in Cos phi calculation
    29/04/2021 - v8.1  - Bug fix in serial port management and realtime energy totals
                         Control initial baud rate to avoid crash (thanks to Seb)
    26/05/2021 - v8.2  - Add active power (W) graph
    22/06/2021 - v8.3  - Change in serial management for ESP32
    04/08/2021 - v9.0  - Tasmota 9.5 compatibility
                         Add LittleFS historic data record
                         Complete change in VA, W and cos phi measurement based on transmission time
                         Add PME/PMI ACE6000 management
                         Add energy update interval configuration
                         Add TIC to TCP bridge (command 'tcpstart 8888' to publish teleinfo stream on port 8888)
    04/09/2021 - v9.1  - Save settings in LittleFS partition if available
                         Log rotate and old files deletion if space low
    10/10/2021 - v9.2  - Add peak VA and V in history files
    02/11/2021 - v9.3  - Add period and totals in history files
                         Add simple FTP server to access history files
    13/03/2022 - v9.4  - Change keys to ISUB and PSUB in METER section
    20/03/2022 - v9.5  - Change serial init and major rework in active power calculation
    01/04/2022 - v9.6  - Add software watchdog feed to avoid lock
    22/04/2022 - v9.7  - Option to minimise LittleFS writes (day:every 1h and week:every 6h)
                         Correction of EAIT bug
    04/08/2022 - v9.8  - Based on Tasmota 12, add ESP32S2 support
                         Remove FTP server auto start
    18/08/2022 - v9.9  - Force GPIO_TELEINFO_RX as digital input
                         Correct bug littlefs config and graph data recording
                         Add Tempo and Production mode (thanks to Sébastien)
                         Correct publication synchronised with teleperiod
    26/10/2022 - v10.0 - Add bar graph monthly (every day) and yearly (every month)
    06/11/2022 - v10.1 - Bug fixes on bar graphs and change in lltoa conversion
    15/11/2022 - v10.2 - Add bar graph daily (every hour)
    04/02/2023 - v10.3 - Add graph swipe (horizontal and vertical)
                         Disable wifi sleep on ESP32 to avoid latency
    25/02/2023 - v11.0 - Split between xnrg and xsns
                         Use Settings->teleinfo to store configuration
                         Update today and yesterday totals
    03/06/2023 - v11.1 - Graph curves live update
                         Rewrite of Tasmota energy update
                         Avoid 100ms rules teleperiod update
    11/06/2023 - v11.2 - Change graph organisation & live display
    15/08/2023 - v11.3 - Evolution in graph navigation
                         Change in XMLHttpRequest management 
    10/10/2023 - v12.0 - Add support for Ecowatt signal in ESP32 versions
    17/10/2023 - v12.1 - Handle Production & consommation simultaneously
                         Display all periods with total
    07/11/2023 - v12.2 - Rotate daily and weekly files every second
                         Change in ecowatt stream reception to avoid overload
                         Remove daily graph to save RAM on ESP8266 1Mb
                         Change daily filename to teleinfo-day-00.csv
    19/11/2023 - v13.0 - Tasmota 13 compatibility
                         Switch to safeboot partitionning
                         Calculate active power for production
    05/12/2023 - v13.1 - Add RTE Tempo calendar
    07/12/2023 - v13.2 - Handle both Ecowatt v4 and v5 (command eco_version)
    03/01/2024 - v13.3 - Add alert management thru STGE
    15/01/2024 - v13.6 - Add support for Denky (thanks to C. Hallard prototype)
                         Add RTE pointe API
                         Add Emeraude 2 meter management
                         Add calendar and virtual relay management
    25/02/2024 - v14.0 - Complete rewrite of Contrat and Period management
                         Activate serial reception when NTP time is ready
                         Change MQTT publication and data reception handling to minimize errors
                         Support various temperature sensors
                         Add Domoticz topics publication (idea from Sebastien)
                         Add support for Wenky with deep sleep (thanks to C. Hallard prototype)
                         Lots of bug fixes (thanks to B. Monot and Sebastien)
                         Separation of curve and historisation sources
                         Removal of all float calculations
    27/03/2024 - v14.1 - Integration of Home Assistant auto discovery (with help of msevestre31)
                         Section COUNTER renamed as CONTRACT with addition of contract data
    28/03/2024 - v14.2 - Add Today and Yesterday conso and Prod
                         Disable Tasmota auto-discovery
    04/04/2024 - v14.3 - Correct RTE Tempo summer bug (different URL for Heure Ete / Heure Hiver)
                         All ESP32 are using Arduino 3.0 libraries
                         Add integration of Homie auto-discovery
                         Update Home assistant auto-discovery with state_class
                         Switch to native Tasmota FTP server
                         Correct graph display bug
                         Add HC/HP 12h30 contract
    21/05/2024 - v14.4 - Based on Tasmota 14
                         Group all sensor data in a single frame
                         Publish Teleinfo raw data under /TIC instead of /SENSOR
                         Publish RTE data under /RTE instead of /SENSOR
                         Add serial reception buffer to minimize reception errors
    01/06/2024 - v14.5 - Add contract auto-discovery for unknown Standard contracts
    28/06/2024 - v14.6 - JSON : Change in calendar (for compliance)
                         JSON : Add CONTRACT/serial and CONTRACT/CONSO
                         Remove all String for ESP8266 stability
    30/06/2024 - v14.7 - Add virtual and physical reception status LED (WS2812 ...)
                         Add commands full and noraw (compatibility with official version)
                         Always publish CONTRACT data with METER and PROD
    16/07/2024 - v14.8 - Add global power, current and voltage to Domoticz integration
                         Add ThingsBoard integration
                         Add relay management for periods and linky virtual relays
                         Increase ESP32 reception buffer to 8192 bytes
                         Redesign of contract management and auto-discovery
                         Rewrite of periods management by meter type
                         Change of contract format in teleinfo.cfg
                         Add contract change detection on main page
                         Optimisation of serial reception to minimise errors
                         Support for Winky C3
                         Correct bug in Tempo Historique contract management
                         Add Live publication option
                         Add command 'data' to publish teleinfo data
                         Add command 'tic' to publish raw TIC data
    08/03/2025 - v14.9 - Based on Tasmota 14.5.0
                         Synchronise time on first meter frame before NTP
                         Add InfluxDB integration with indexes and totals
                         Change Domoticz conso total to P1SmartMeter
                         Publish HomeAssistant topic in mode retain
                         Add command energyconfig skip=x (0..7) to publish LIVE topic
                         Adaptation on Winky analog input for Tasmota 14.3 compatibility
                         Correct bug in Linky calendar management
                         RTE API used with RTC connexions, no need of MQTT connexion
                         Rework of cosphi calculation algo
                         Convert contract type to upper case before detection
                         Correct brand new counter index bug
                         Complete rewrite of calendar management
                         Handle calendar for TEMPO and EJP in historic mode
                         First step of generic TEMPO contract detection
                         Avoid NTARF and STGE to detect period as they as out of synchro very often
                         ESP8266 memory optimisation
                         Add Ulanzi remote display management thru Awtrix open-source firmware
    16/03/2025 - v14.10  Correct bug in contract auto-discovery
    01/05/2025 - v14.11  Based on Tasmota 14.6.0
                         Correct bug in week number calculation for sundays
                         Cleanup old data files if FS is full
                         Complete rewrite of speed detection
                         Add period profile
                                                
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License aStart STGE managements published by
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
#endif      // _USER_CONFIG_OVERRIDE_H_

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

// extension description
#define EXTENSION_NAME    "Teleinfo"              // name
#define EXTENSION_AUTHOR  "Nicolas Bernaerts"     // author
#define EXTENSION_VERSION "15beta1"               // version

// FTP server credentials
#ifdef USE_FTP
  #define USER_FTP          "teleinfo"            // FTP server login
  #define PW_FTP            "teleinfo"            // FTP server password
#endif

// complementary modules
#define USE_MISC_OPTION                           // Add IP and common options configuration page
#define USE_TELEINFO_RELAY                        // Enable Teleinfo period and virtual relay association to local relays

// teleinfo display is in French
#define MY_LANGUAGE        fr_FR

// devices specificities
// ---------------------

#undef MQTT_TOPIC

// build
#ifdef BUILD_ESP32S3_16M
  #define EXTENSION_BUILD "esp32s3-16m"
  #define MQTT_TOPIC "teleinfo_%06X"

#elif BUILD_ESP32S3_4M
  #define EXTENSION_BUILD "esp32s3-4m"
  #define MQTT_TOPIC "teleinfo_%06X"

#elif BUILD_ESP32S2
  #define EXTENSION_BUILD "esp32s2-4m"
  #define MQTT_TOPIC "teleinfo_%06X"

#elif BUILD_ESP32_DENKYD4
  #define EXTENSION_BUILD "denkyd4-8m"
  #define USER_TEMPLATE "{\"NAME\":\"Denky D4\",\"GPIO\":[32,0,0,0,1,0,0,0,0,1,1376,1,0,0,0,0,0,640,608,0,0,0,0,0,0,0,5632,0,0,0,0,0,0,0,0,0],\"FLAG\":0,\"BASE\":1}" 
  #define MQTT_TOPIC "denky_%06X"

#elif BUILD_ESP32_WINKYC6
  #define EXTENSION_BUILD "winkyc6-4m"
  #define USER_TEMPLATE "{\"NAME\":\"Winky C6\",\"GPIO\":[1,4704,1376,4705,5632,4706,640,608,1,32,1,0,0,0,0,0,0,0,1,1,1,1,1,4096,0,0,0,0,0,0,0],\"FLAG\":0,\"BASE\":1}"
  #define MQTT_TOPIC "winky_%06X"

#elif BUILD_ESP32_WINKYC3
  #define EXTENSION_BUILD "winkyc3-4m"
  #define USER_TEMPLATE "{\"NAME\":\"Winky C3\",\"GPIO\":[1,4704,1376,5632,4705,640,608,1,1,32,1,0,0,0,0,0,0,0,1,1,1,1],\"FLAG\":0,\"BASE\":1}" 
  #define MQTT_TOPIC "winky_%06X"

#elif BUILD_ESP32C3
  #define EXTENSION_BUILD "esp32c3-4m"
  #define MQTT_TOPIC "teleinfo_%06X"

#elif BUILD_ESP32_4M
  #define EXTENSION_BUILD "esp32-4m"
  #define MQTT_TOPIC "teleinfo_%06X"

#elif BUILD_16M
  #define EXTENSION_BUILD "esp8266-16m"
  #define MQTT_TOPIC "teleinfo_%06X"

#elif BUILD_4M
  #define EXTENSION_BUILD "esp8266-4m"
  #define MQTT_TOPIC "teleinfo_%06X"

#elif BUILD_1M
  #define EXTENSION_BUILD "esp8266-1m"
  #define MQTT_TOPIC "teleinfo_%06X"

#endif

// MQTT default
#undef MQTT_HOST
#define MQTT_HOST          "mqtt.local"
#undef MQTT_PORT
#define MQTT_PORT          1883              
#undef MQTT_USER
#define MQTT_USER          ""
#undef MQTT_PASS
#define MQTT_PASS          ""
#undef MQTT_FULLTOPIC
#define MQTT_FULLTOPIC     "%topic%/%prefix%/"
#undef FRIENDLY_NAME
#define FRIENDLY_NAME      "Teleinfo"

// disable serial log
#undef SERIAL_LOG_LEVEL
#define SERIAL_LOG_LEVEL LOG_LEVEL_NONE       // disable SerialLog

// ----------------------
// Common ESP8266 & ESP32
// ----------------------

#define USE_WEB_STATUS_LINE_WIFI              // enable wifi icon

#define MDNS_ENABLE false                     // disable multicast DNS

#define MQTT_DATA_STRING                      // Enable use heap instead of fixed memory for TasmotaGlobal.mqtt_data

#undef FS_SD_MMC                              // disable SD MMC to remove warnings

#undef USE_ARDUINO_OTA                        // supporTeleinfoContractUpdatet for Arduino OTA
#undef USE_WPS                                // support for WPS as initial wifi configuration tool
#undef USE_SMARTCONFIG                        // support for Wifi SmartConfig as initial wifi configuration tool
#undef USE_MQTT_TLS                           // TLS support won't work as the MQTTHost is not set

#undef USE_DOMOTICZ                           // Disable official Domoticz

#undef USE_TASMOTA_DISCOVERY                  // Enable Tasmota discovery
#undef USE_HOME_ASSISTANT                     // Disable historic Home Assistant

#undef USE_KNX                                // KNX IP Protocol Support
#undef USE_KNX_WEB_MENU                       // Enable KNX WEB MENU (+8.3k code, +144 mem)

#undef USE_EMULATION_HUE                      // Hue Bridge emulation for Alexa (+14k code, +2k mem common)
#undef USE_EMULATION_WEMO                     // Belkin WeMo emulation for Alexa (+6k code, +2k mem common)
#undef USE_CUSTOM                             // Custom features

#undef WEBSERVER_ADVERTISE                    // Provide access to webserver by name <Hostname>.local/
#undef USE_DISCOVERY                          // Discovery services for both MQTT and web server
#undef MQTT_HOST_DISCOVERY                    // Find MQTT host serTeleinfoContractUpdatever (overrides MQTT_HOST if found)

#undef USE_TIMERS                             // support for up to 16 timers
#undef USE_TIMERS_WEB                         // support for timer webpage
#undef USE_SUNRISE                            // support for Sunrise and sunset tools

#undef USE_SCRIPT                             // Add support for script (+17k code)

#define USE_RULES                             // Support for rules
  #define USE_EXPRESSION                      // Add support for expression evaluation in rules (+3k2 code, +64 bytes mem)  
    #define SUPPORT_IF_STATEMENT              // Add support for IF statement in rules (+4k2 code, -332 bytes mem)  

#undef ROTARY_V1                              // Add support for Rotary Encoder as used in MI Desk Lamp (+0k8 code)
#undef USE_SONOFF_RF                          // Add support for Sonoff Rf Bridge (+3k2 code)
#undef USE_RF_FLASH                           // Add support for flashing the EFM8BB1 chip on the Sonoff RF Bridge.
#undef USE_SONOFF_SC                          // Add support for Sonoff Sc (+1k1 code)
#undef USE_TUYA_MCU                           // Add support for Tuya Serial MCU
#undef USE_TUYAMCUBR                            // Add support for TuyaMCU Bridge
#undef USE_ARMTRONIX_DIMMERS                  // Add support for Armtronix Dimmers (+1k4 code)
#undef USE_PS_16_DZ                           // Add support for PS-16-DZ Dimmer (+2k code)
#undef USE_SONOFF_IFAN                        // Add support for Sonoff iFan02 and iFan03 (+2k code)
#undef USE_BUZZER                             // Add support for a buzzer (+0k6 code)
#undef USE_ARILUX_RF                          // Add support for Arilux RF remote controller (+0k8 code, 252 iram (non 2.3.0))
#undef USE_SHUTTER                            // Add Shutter support for up to 4 shutter with different motortypes (+11k code)

#undef USE_DEEPSLEEP                          // Add support for deepsleep (+1k code)

#undef USE_EXS_DIMMER                         // Add support for ES-Store WiFi Dimmer (+1k5 code)
#undef USE_HOTPLUG                              // Add support for sensor HotPlug
#undef EXS_MCU_CMNDS                          // Add command to send MCU commands (+0k8 code)
#undef USE_DEVICE_GROUPS                      // Add support for device groups (+5k5 code)
#undef USE_DEVICE_GROUPS_SEND                 // Add support for the DevGroupSend command (+0k6 code)
#undef USE_PWM_DIMMER                         // Add support for MJ-SD01/acenx/NTONPOWER PWM dimmers (+2k2 code, DGR=0k4)
#undef USE_PWM_DIMMER_REMOTE                  // Add support for remote switches to PWM Dimmer (requires USE_DEVICE_GROUPS) (+0k9 code)
#undef USE_SONOFF_D1                          // Add support for Sonoff D1 Dimmer (+0k7 code)
#undef USE_SHELLY_DIMMER                      // Add support for Shelly Dimmer (+3k code)
#undef SHELLY_CMDS                            // Add command to send co-processor commands (+0k3 code)
#undef SHELLY_FW_UPGRADE                      // Add firmware upgrade option for co-processor (+3k4 code)
#undef SHELLY_VOLTAGE_MON                     // Add support for reading voltage and current measurment (-0k0 code)
#undef USE_MAGIC_SWITCH                         // Add Sonoff MagicSwitch support as implemented in Sonoff Basic R4 (+612B flash, +64B IRAM for intr)

#undef USE_LIGHT                              // Add support for light control
#undef USE_WS2812                             // WS2812 Led string using library NeoPixelBus (+5k code, +1k mem, 232 iram) - Disable by //
#undef USE_WS2812_DMA                         // ESP8266 only, DMA supports only GPIO03 (= Serial RXD) (+1k mem). When USE_WS2812_DMA is enabled expect Exceptions on Pow
#undef USE_WS2812_INVERTED                    // Use inverted data signal
#undef USE_MY92X1                             // Add support for MY92X1 RGBCW led controller as used in Sonoff B1, Ailight and Lohas
#undef USE_SM16716                            // Add support for SM16716 RGB LED controller (+0k7 code)
#undef USE_SM2135                             // Add support for SM2135 RGBCW led control as used in Action LSC (+0k6 code)
#undef USE_SM2335                               // Add support for SM2335 RGBCW led control as used in SwitchBot Color Bulb (+0k7 code)
#undef USE_BP1658CJ                             // Add support for BP1658CJ RGBCW led control as used in Orein OS0100411267 Bulb
#undef USE_BP5758D                              // Add support for BP5758D RGBCW led control as used in some Tuya lightbulbs (+0k8 code)
#undef USE_SONOFF_L1                          // Add support for Sonoff L1 led control
#undef USE_ELECTRIQ_MOODL                     // Add support for ElectriQ iQ-wifiMOODL RGBW LED controller (+0k3 code)
#undef USE_LIGHT_PALETTE                      // Add support for color palette (+0k7 code)
#undef USE_LIGHT_VIRTUAL_CT                   // Add support for Virtual White Color Temperature (+1.1k code)
#undef USE_DGR_LIGHT_SEQUENCE                 // Add support for device group light sequencing (requires USE_DEVICE_GROUPS) (+0k2 code)
#undef USE_LSC_MCSL                             // Add support for GPE Multi color smart light as sold by Action in the Netherlands (+1k1 code)
#undef USE_LIGHT_ARTNET                         // Add support for DMX/ArtNet via UDP on port 6454 (+3.5k code)

#undef USE_COUNTER                            // Enable inputs as counter (+0k8 code)

//#define USE_ADC_VCC                            // display analog input as VCC

#define USE_DS18x20                            // Add support for DS18x20 sensors with id sort, single scan and read retry (+2k6 code)

#define USE_I2C                               // Enable all I2C sensors and devices

#define USE_SHT3X                             // Enable SHT30 and SHT40
#define USE_SHT                                // [I2cDriver8] Enable SHT1X sensor (+1k4 code)
#define USE_HTU                                // [I2cDriver9] Enable HTU21/SI7013/SI7020/SI7021 sensor (I2C address 0x40) (+1k5 code)
#define USE_BMP                                // [I2cDriver10] Enable BMP085/BMP180/BMP280/BME280 sensors (I2C addresses 0x76 and 0x77) (+4k4 code)

#undef USE_BME68X                           // Enable support for BME680/BME688 sensor using Bosch BME68x library (+6k9 code)
#undef USE_BH1750                             // [I2cDriver11] Enable BH1750 sensor (I2C address 0x23 or 0x5C) (+0k5 code)
#undef USE_VEML6070                           // [I2cDriver12] Enable VEML6070 sensor (I2C addresses 0x38 and 0x39) (+1k5 code)
#undef USE_ADS1115                            // [I2cDriver13] Enable ADS1115 16 bit A/D converter (I2C address 0x48, 0x49, 0x4A or 0x4B) based on Adafruit ADS1x15 library (no library needed) (+0k7 code)
#undef USE_INA219                             // [I2cDriver14] Enable INA219 (I2C address 0x40, 0x41 0x44 or 0x45) Low voltage and current sensor (+1k code)
#undef USE_INA226                             // [I2cDriver35] Enable INA226 (I2C address 0x40, 0x41 0x44 or 0x45) Low voltage and current sensor (+2k3 code)
#undef USE_SHT3X                              // [I2cDriver15] Enable SHT3x (I2C address 0x44 or 0x45) or SHTC3 (I2C address 0x70) sensor (+0k7 code)
#undef USE_TSL2561                            // [I2cDriver16] Enable TSL2561 sensor (I2C address 0x29, 0x39 or 0x49) using library Joba_Tsl2561 (+2k3 code)
#undef USE_TSL2591                            // [I2cDriver40] Enable TSL2591 sensor (I2C address 0x29) using library Adafruit_TSL2591 (+1k6 code)
#undef USE_MGS                                // [I2cDriver17] Enable Xadow and Grove Mutichannel Gas sensor using library Multichannel_Gas_Sensor (+10k code)
#undef USE_SGP30                              // [I2cDriver18] Enable SGP30 sensor (I2C address 0x58) (+1k1 code)
#undef USE_SGP40                              // [I2cDriver69] Enable SGP40 sensor (I2C address 0x59) (+1k4 code)
#undef USE_SGP4X                              // [I2cDriver82] Enable SGP41 sensor (I2C address 0x59) (+7k2 code)
#undef USE_SEN5X                              // [I2cDriver76] Enable SEN5X sensor (I2C address 0x69) (+3k code)
#undef USE_SI1145                             // [I2cDriver19] Enable SI1145/46/47 sensor (I2C address 0x60) (+1k code)
#undef USE_LM75AD                             // [I2cDriver20] Enable LM75AD sensor (I2C addresses 0x48 - 0x4F) (+0k5 code)
#undef USE_APDS9960                           // [I2cDriver21] Enable APDS9960 Proximity Sensor (I2C address 0x39). Disables SHT and VEML6070 (+4k7 code)
#undef USE_MCP230xx                           // [I2cDriver22] Enable MCP23008/MCP23017 - Must define I2C Address in #undef USE_MCP230xx_ADDR below - range 0x20 - 0x27 (+5k1 code)
#undef USE_MCP23XXX_DRV                       // [I2cDriver77] Enable MCP23xxx support as virtual switch/button/relay (+3k(I2C)/+5k(SPI) code)
#undef USE_PCA9685                            // [I2cDriver1] Enable PCA9685 I2C HW PWM Driver - Must define I2C Address in #undef USE_PCA9685_ADDR below - range 0x40 - 0x47 (+1k4 code)
#undef USE_PCA9685_V2                         // [I2cDriver1] Enable PCA9685 I2C HW PWM Driver - Must define I2C Address in #undef USE_PCA9685_ADDR below - range 0x40 - 0x47 (+3k4 code)
#undef USE_PCA9632                            // [I2cDriver75] Enable PCA9632 I2C HW PWM Driver (+1k8 code)
#undef USE_MPR121                             // [I2cDriver23] Enable MPR121 controller (I2C addresses 0x5A, 0x5B, 0x5C and 0x5D) in input mode for touch buttons (+1k3 code)
#undef USE_CCS811                             // [I2cDriver24] Enable CCS811 sensor (I2C address 0x5A) (+2k2 code)
#undef USE_CCS811_V2                          // [I2cDriver24] Enable CCS811 sensor (I2C addresses 0x5A and 0x5B) (+2k8 code)
#undef USE_ENS16x                             // [I2cDriver85] Enable ENS160 and ENS161 sensor (I2C addresses 0x52 and 0x53) (+1.9kB of code and 12B of RAM)
#undef USE_ENS210                             // [I2cDriver86] Enable ENS210 sensor (I2C addresses 0x43) (+1.7kB of code and 12B of RAM)
#undef USE_MPU6050                            // [I2cDriver25] Enable MPU6050 sensor (I2C address 0x68 AD0 low or 0x69 AD0 high) (+3K3 of code and 188 Bytes of RAM)
#undef USE_MGC3130                            // [I2cDriver27] Enable MGC3130 Electric Field Effect Sensor (I2C address 0x42) (+2k7 code, 0k3 mem)
#undef USE_MAX44009                           // [I2cDriver28] Enable MAX44009 Ambient Light sensor (I2C addresses 0x4A and 0x4B) (+0k8 code)
#undef USE_SCD30                              // [I2cDriver29] Enable Sensiron SCd30 CO2 sensor (I2C address 0x61) (+3k3 code)
#undef USE_SCD40                              // [I2cDriver62] Enable Sensiron SCd40/Scd41 CO2 sensor (I2C address 0x62) (+3k5 code)
#undef USE_SPS30                              // [I2cDriver30] Enable Sensiron SPS30 particle sensor (I2C address 0x69) (+1.7 code)
#undef USE_ADE7880                            // [I2cDriver65] Enable ADE7880 Energy monitor as used on Shelly 3EM (I2C address 0x38) (+3k8)
#undef USE_ADE7953                            // [I2cDriver7] Enable ADE7953 Energy monitor as used on Shelly 2.5 (I2C address 0x38) (+1k5)
#undef USE_VL53L0X                            // [I2cDriver31] Enable VL53L0x time of flight sensor (I2C address 0x29) (+4k code)
#undef USE_VL53L1X                            // [I2cDriver54] Enable VL53L1X time of flight sensor (I2C address 0x29) using Pololu VL53L1X library (+2k9 code)
#undef USE_TOF10120                           // [I2cDriver57] Enable TOF10120 time of flight sensor (I2C address 0x52) (+0k6 code)
#undef USE_MLX90614                           // [I2cDriver32] Enable MLX90614 ir temp sensor (I2C address 0x5a) (+0.6k code)
#undef USE_CHIRP                              // [I2cDriver33] Enable CHIRP soil moisture sensor (variable I2C address, default 0x20)
#undef USE_PAJ7620                            // [I2cDriver34] Enable PAJ7620 gesture sensor (I2C address 0x73) (+2.5k code)
#undef USE_PCF8574                            // [I2cDriver2] Enable PCF8574 I/O Expander (I2C addresses 0x20 - 0x26 and 0x39 - 0x3F) (+2k1 code)
#undef USE_HIH6                               // [I2cDriver36] Enable Honeywell HIH Humidity and Temperature sensor (I2C address 0x27) (+0k6)
#undef USE_DHT12                              // [I2cDriver41] Enable DHT12 humidity and temperature sensor (I2C address 0x5C) (+0k7 code)
#undef USE_DS1624                             // [I2cDriver42] Enable DS1624, DS1621 temperature sensor (I2C addresses 0x48 - 0x4F) (+1k2 code)
#undef USE_AHT1x                              // [I2cDriver43] Enable AHT10/15 humidity and temperature sensor (I2C address 0x38, 0x39) (+0k8 code)
#undef USE_AHT2x                              // [I2cDriver43] Enable AHT20/AM2301B instead of AHT1x humidity and temperature sensor (I2C address 0x38) (+0k8 code)
#undef USE_WEMOS_MOTOR_V1                     // [I2cDriver44] Enable Wemos motor driver V1 (I2C addresses 0x2D - 0x30) (+0k7 code)
#undef USE_HDC1080                            // [I2cDriver45] Enable HDC1080 temperature/humidity sensor (I2C address 0x40) (+1k5 code)
#undef USE_IAQ                                // [I2cDriver46] Enable iAQ-core air quality sensor (I2C address 0x5a) (+0k6 code)
#undef USE_AS3935                             // [I2cDriver48] Enable AS3935 Franklin Lightning Sensor (I2C address 0x03) (+5k4 code)
#undef USE_VEML6075                           // [I2cDriver49] Enable VEML6075 UVA/UVB/UVINDEX Sensor (I2C address 0x10) (+2k1 code)
#undef USE_VEML7700                           // [I2cDriver50] Enable VEML7700 Ambient Light sensor (I2C addresses 0x10) (+4k5 code)
#undef USE_MCP9808                            // [I2cDriver51] Enable MCP9808 temperature sensor (I2C addresses 0x18 - 0x1F) (+0k9 code)
#undef USE_HP303B                             // [I2cDriver52] Enable HP303B temperature and pressure sensor (I2C address 0x76 or 0x77) (+6k2 code)
#undef USE_MLX90640                           // [I2cDriver53] Enable MLX90640 IR array temperature sensor (I2C address 0x33) (+20k code)
#undef USE_EZOPH                              // [I2cDriver55] Enable support for EZO's pH sensor (+0k3 code) - Shared EZO code required for any EZO device (+1k2 code)
#undef USE_EZOORP                             // [I2cDriver55] Enable support for EZO's ORP sensor (+0k3 code) - Shared EZO code required for any EZO device (+1k2 code)
#undef USE_EZORTD                             // [I2cDriver55] Enable support for EZO's RTD sensor (+0k2 code) - Shared EZO code required for any EZO device (+1k2 code)
#undef USE_EZOHUM                             // [I2cDriver55] Enable support for EZO's HUM sensor (+0k3 code) - Shared EZO code required for any EZO device (+1k2 code)
#undef USE_EZOEC                              // [I2cDriver55] Enable support for EZO's EC sensor (+0k3 code) - Shared EZO code required for any EZO device (+1k2 code)
#undef USE_EZOCO2                             // [I2cDriver55] Enable support for EZO's CO2 sensor (+0k2 code) - Shared EZO code required for any EZO device (+1k2 code)
#undef USE_EZOO2                              // [I2cDriver55] Enable support for EZO's O2 sensor (+0k3 code) - Shared EZO code required for any EZO device (+1k2 code)
#undef USE_EZOPRS                             // [I2cDriver55] Enable support for EZO's PRS sensor (+0k7 code) - Shared EZO code required for any EZO device (+1k2 code)
#undef USE_EZOFLO                             // [I2cDriver55] Enable support for EZO's FLO sensor (+0k4 code) - Shared EZO code required for any EZO device (+1k2 code)
#undef USE_EZODO                              // [I2cDriver55] Enable support for EZO's DO sensor (+0k3 code) - Shared EZO code required for any EZO device (+1k2 code)
#undef USE_EZORGB                             // [I2cDriver55] Enable support for EZO's RGB sensor (+0k5 code) - Shared EZO code required for any EZO device (+1k2 code)
#undef USE_EZOPMP                             // [I2cDriver55] Enable support for EZO's PMP sensor (+0k3 code) - Shared EZO code required for any EZO device (+1k2 code)
#undef USE_SEESAW_SOIL                        // [I2cDriver56] Enable Capacitice Soil Moisture & Temperature Sensor (I2C addresses 0x36 - 0x39) (+1k3 code)
#undef USE_MPU_ACCEL                          // [I2cDriver58] Enable MPU6886/MPU9250 - found in M5Stack - support both I2C buses on ESP32 (I2C address 0x68) (+2k code)
#undef USE_AM2320                             // [I2cDriver60] Enable AM2320 temperature and humidity Sensor (I2C address 0x5C) (+1k code)
#undef USE_T67XX                              // [I2cDriver61] Enable Telaire T67XX CO2 sensor (I2C address 0x15) (+1k3 code)
#undef USE_HM330X                             // [I2cDriver63] Enable support for SeedStudio Grove Particule sensor (I2C address 0x40) (+1k5 code)
#undef USE_HDC2010                            // [I2cDriver64] Enable HDC2010 temperature/humidity sensor (I2C address 0x40) (+1k5 code)
#undef USE_DS3502                             // [I2CDriver67] Enable DS3502 digital potentiometer (I2C address 0x28 - 0x2B) (+0k4 code)
#undef USE_HYT                                // [I2CDriver68] Enable HYTxxx temperature and humidity sensor (I2C address 0x28) (+0k5 code)
#undef USE_LUXV30B                            // [I2CDriver70] Enable RFRobot SEN0390 LuxV30b ambient light sensor (I2C address 0x4A) (+0k5 code)
#undef USE_QMC5883L                           // [I2CDriver71] Enable QMC5883L magnetic induction sensor (I2C address 0x0D) (+0k8 code)
#undef USE_HMC5883L                           // [I2CDriver73] Enable HMC5883L magnetic induction sensor (I2C address 0x1E) (+1k3 code)
#undef USE_INA3221                            // [I2CDriver72] Enable INA3221 3-channel DC voltage and current sensor (I2C address 0x40-0x44) (+3.2k code)
#undef USE_PMSA003I                           // [I2cDriver78] Enable PMSA003I Air Quality Sensor (I2C address 0x12) (+1k8 code)
#undef USE_GDK101                             // [I2cDriver79] Enable GDK101 sensor (I2C addresses 0x18 - 0x1B) (+1k2 code)
#undef USE_TC74                               // [I2cDriver80] Enable TC74 sensor (I2C addresses 0x48 - 0x4F) (+1k code)
#undef USE_PCA9557                            // [I2cDriver81] Enable PCA9557 8-bit I/O Expander (I2C addresses 0x18 - 0x1F) (+2k5 code)
#undef USE_MAX17043                           // [I2cDriver83] Enable MAX17043 fuel-gauge systems Lipo batteries sensor (I2C address 0x36) (+0k9 code)
#undef USE_AMSX915                            // [I2CDriver86] Enable AMS5915/AMS6915 pressure/temperature sensor (+1k2 code)
#undef USE_SPL06_007                          // [I2cDriver87] Enable SPL06_007 pressure and temperature sensor (I2C addresses 0x76) (+2k5 code)
#undef USE_RTC_CHIPS                          // Enable RTC chip support and NTP server - Select only one
#undef USE_DS3231                           // [I2cDriver26] Enable DS3231 RTC (I2C address 0x68) (+1k2 code)
#undef DS3231_ENABLE_TEMP                   //   In DS3231 driver, enable the internal temperature sensor
#undef USE_BM8563                           // [I2cDriver59] Enable BM8563 RTC - found in M5Stack - support both I2C buses on ESP32 (I2C address 0x51) (+2.5k code)
#undef USE_PCF85363                         // [I2cDriver66] Enable PCF85363 RTC - found Shelly 3EM (I2C address 0x51) (+0k7 code)

#undef USE_DISPLAY                            // Add I2C/TM1637/MAX7219 Display Support (+2k code)
#undef USE_DISPLAY_MODES1TO5                // Enable display mode 1 to 5 in addition to mode 0
#undef USE_DISPLAY_LCD                      // [DisplayModel 1] [I2cDriver3] Enable Lcd display (I2C addresses 0x27 and 0x3F) (+6k code)
#undef USE_DISPLAY_MATRIX                   // [DisplayModel 3] [I2cDriver5] Enable 8x8 Matrix display (I2C adresseses see below) (+11k code)
#undef USE_DISPLAY_SEVENSEG                 // [DisplayModel 11] [I2cDriver47] Enable sevenseg display (I2C 0x70-0x77) (<+11k code)
#undef USE_DISPLAY_SH1106                   // [DisplayModel 7] [I2cDriver6] Enable SH1106 Oled 128x64 display (I2C addresses 0x3C and 0x3D)
#undef USE_DISPLAY_TM1650                   // [DisplayModel 20] [I2cDriver74] Enable TM1650 display (I2C addresses 0x24 - 0x27 and 0x34 - 0x37)
#undef USE_DT_VARS                          // Display variables that are exposed in JSON MQTT strings e.g. in TelePeriod messages.
#undef USE_GRAPH                            // Enable line charts with displays

#undef USE_DISPLAY_TM1637                   // [DisplayModel 15] Enable TM1637 Module
#undef USE_DISPLAY_MAX7219                  // [DisplayModel 19] Enable MAX7219 Module
#undef USE_UNIVERSAL_DISPLAY                   // New universal display driver for both I2C and SPI

#undef USE_SPI                                // Disable all SPI devices

#undef USE_HDMI_CEC                              // Add support for HDMI CEC bus (+7k code, 1456 bytes IRAM)

// -- Serial sensors ------------------------------
#undef USE_MHZ19                                // Add support for MH-Z19 CO2 sensor (+2k code)
#undef USE_SENSEAIR                             // Add support for SenseAir K30, K70 and S8 CO2 sensor (+2k3 code)
#undef USE_CM110x                               // Add support for CM110x CO2 sensors (+2k7code)
#undef USE_PMS5003                              // Add support for PMS5003 and PMS7003 particle concentration sensor (+1k3 code)
#undef USE_NOVA_SDS                             // Add support for SDS011 and SDS021 particle concentration sensor (+1k5 code)
#undef USE_HPMA                                 // Add support for Honeywell HPMA115S0 particle concentration sensor (+1k4)
#undef USE_SR04                                 // Add support for HC-SR04 ultrasonic devices (+1k code)
#undef USE_ME007                                // Add support for ME007 ultrasonic devices (+1k5 code)
#undef USE_DYP                                  // Add support for DYP ME-007 ultrasonic distance sensor, serial port version (+0k5 code)
#undef USE_SERIAL_BRIDGE                        // Add support for software Serial Bridge (+2k code)
#undef USE_MODBUS_BRIDGE                        // Add support for software Modbus Bridge (+4.5k code)
#undef USE_MODBUS_BRIDGE_TCP                    // Add support for software Modbus TCP Bridge (also enable Modbus TCP Bridge) (+2k code)

#undef USE_TCP_BRIDGE                           //  Add support for Serial to TCP bridge (+1.3k code)

#undef USE_MP3_PLAYER                           // Use of the DFPlayer Mini MP3 Player RB-DFR-562 commands: play, pause, stop, track, volume and reset
#undef USE_DY_SV17F                           // Use of DY-SV17F MP3 Player commands: play, stop, track and volume
#undef USE_AZ7798                               // Add support for AZ-Instrument 7798 CO2 datalogger (+1k6 code)
#undef USE_PN532_HSU                            // Add support for PN532 using HSU (Serial) interface (+1k7 code, 156 bytes mem)
#undef USE_RDM6300                              // Add support for RDM6300 125kHz RFID Reader (+0k8)
#undef USE_IBEACON                              // Add support for bluetooth LE passive scan of ibeacon devices (uses HM17 module)
#undef USE_GPS                                  // Add support for GPS and NTP Server for becoming Stratus 1 Time Source (+3k1 code, +132 bytes RAM)
#undef USE_HM10                                 // (ESP8266 only) Add support for HM-10 as a BLE-bridge (+17k code)
#undef USE_HRXL                                 // Add support for MaxBotix HRXL-MaxSonar ultrasonic range finders (+0k7)
#undef USE_TASMOTA_CLIENT                       // Add support for Arduino Uno/Pro Mini via serial interface including flashing (+2k6 code, 64 mem)
#undef USE_OPENTHERM                            // Add support for OpenTherm (+15k code)
#undef USE_MIEL_HVAC                            // Add support for Mitsubishi Electric HVAC serial interface (+5k code)
#undef USE_TUYAMCUBR                            // Add support for TuyaMCU Bridge
#undef USE_PROJECTOR_CTRL                       // Add support for LCD/DLP Projector serial control interface (+2k code)
#undef USE_AS608                                // Add support for AS608 optical and R503 capacitive fingerprint sensor (+3k code)
#undef USE_TFMINIPLUS                           // Add support for TFmini Plus (TFmini, TFmini-S) LiDAR modules via UART interface (+0k8)
#undef USE_HRG15                                // Add support for Hydreon RG-15 Solid State Rain sensor (+1k5 code)
#undef USE_VINDRIKTNING                         // Add support for IKEA VINDRIKTNING particle concentration sensor (+0k6 code)
#undef USE_LD2410                               // Add support for HLK-LD2410 24GHz smart wave motion sensor (+2k8 code)
#undef USE_LOX_O2                               // Add support for LuminOx LOX O2 Sensor (+0k8 code)
#undef USE_GM861                                // Add support for GM861 1D and 2D Bar Code Reader (+1k3 code)


#define USE_ENERGY_SENSOR                        // Add support for Energy Monitors (+14k code)
#undef USE_ENERGY_MARGIN_DETECTION              // Add support for Energy Margin detection (+1k6 code)
#undef USE_ENERGY_POWER_LIMIT                 // Add additional support for Energy Power Limit detection (+1k2 code)
#undef USE_ENERGY_DUMMY                         // Add support for dummy Energy monitor allowing user values (+0k7 code)
#undef USE_HLW8012                              // Add support for HLW8012, BL0937 or HJL-01 Energy Monitor for Sonoff Pow and WolfBlitz
#undef USE_CSE7766                              // Add support for CSE7766 Energy Monitor for Sonoff S31 and Pow R2
#undef USE_PZEM004T                             // Add support for PZEM004T Energy monitor (+2k code)
#undef USE_PZEM_AC                              // Add support for PZEM014,016 Energy monitor (+1k1 code)
#undef USE_PZEM_DC                              // Add support for PZEM003,017 Energy monitor (+1k1 code)
#undef USE_MCP39F501                            // Add support for MCP39F501 Energy monitor as used in Shelly 2 (+3k1 code)
#undef USE_SDM72                                // Add support for Eastron SDM72-Modbus energy monitor (+0k3 code)
#undef USE_SDM120                               // Add support for Eastron SDM120-Modbus energy monitor (+1k1 code)
#undef USE_SDM230                               // Add support for Eastron SDM230-Modbus energy monitor (+1k6 code)
#undef USE_SDM630                               // Add support for Eastron SDM630-Modbus energy monitor (+0k6 code)
#undef USE_DDS2382                              // Add support for Hiking DDS2382 Modbus energy monitor (+0k6 code)
#undef USE_DDSU666                              // Add support for Chint DDSU666 Modbus energy monitor (+0k6 code)
#undef USE_SOLAX_X1                             // Add support for Solax X1 series Modbus log info (+3k1 code)
#undef SOLAXX1_PV2                            // Solax X1 using second PV
#undef USE_LE01MR                               // Add support for F&F LE-01MR Modbus energy monitor (+1k code)
#undef USE_BL09XX                               // Add support for various BL09XX Energy monitor as used in Blitzwolf SHP-10 or Sonoff Dual R3 v2 (+1k6 code)

#define USE_TELEINFO                             // Add support for Teleinfo via serial RX interface (+5k2 code, +168 RAM + SmartMeter LinkedList Values RAM)
//#undef USE_TELEINFO                             // Add support for Teleinfo via serial RX interface (+5k2 code, +168 RAM + SmartMeter LinkedList Values RAM)

#undef USE_IEM3000                              // Add support for Schneider Electric iEM3000-Modbus series energy monitor (+0k8 code)
#undef USE_WE517                                // Add support for Orno WE517-Modbus energy monitor (+1k code)
#undef USE_MODBUS_ENERGY                        // Add support for generic modbus energy monitor using a user file in rule space (+5k)
#undef USE_SONOFF_SPM

#undef USE_DHT                                  // Add support for DHT11, AM2301 (DHT21, DHT22, AM2302, AM2321) and SI7021 Temperature and Humidity sensor (1k6 code)

#undef USE_MAX31855                             // Add support for MAX31855/MAX6675 K-Type thermocouple sensor using softSPI
#undef USE_MAX31865                             // Add support for MAX31865 RTD sensors using softSPI
#undef USE_LMT01                                // Add support for TI LMT01 temperature sensor, count pulses on single GPIO (+0k5 code)
#undef USE_WIEGAND                              // Add support for 24/26/32/34 bit RFID Wiegand interface (D0/D1) (+1k7 code)

#undef USE_AC_ZERO_CROSS_DIMMER

// -- IR Remote features - subset of IR protocols --------------------------
#undef USE_IR_REMOTE                          // Send IR remote commands using library IRremoteESP8266 and ArduinoJson (+4k3 code, 0k3 mem, 48 iram)
#undef USE_IR_REMOTE_FULL                     // complete integration of IRremoteESP8266 for Tasmota
#undef USE_IR_SEND_NEC                        // Support IRsend NEC protocol
#undef USE_IR_SEND_RC5                        // Support IRsend Philips RC5 protocol
#undef USE_IR_SEND_RC6                        // Support IRsend Philips RC6 protocol
#undef USE_IR_RECEIVE                         // Support for IR receiver (+7k2 code, 264 iram)

// -- SD Card support -----------------------------
#undef USE_SDCARD                               // mount SD Card, requires configured SPI pins and setting of `SDCard CS` gpio
#undef SDC_HIDE_INVISIBLES                    // hide hidden directories from the SD Card, which prevents crashes when dealing SD created on MacOS

// -- Zigbee interface ----------------------------
#undef USE_ZIGBEE                             // Enable serial communication with Zigbee CC2530 flashed with ZNP (+49k code, +3k mem)
#undef USE_ZIGBEE_ZNP                         // Enable ZNP protocol, needed for CC2530 based devices
#undef USE_ZIGBEE_EZSP                        // Enable EZSP protocol, needed for EFR32 EmberZNet based devices, like Sonoff Zigbee bridge                                             
#undef USE_ZIGBEE_EEPROM                      // Use the EEPROM from the Sonoff ZBBridge to save Zigbee configuration and data
#undef USE_ZBBRIDGE_TLS                       // TLS support for zbbridge

// -- Other sensors/drivers -----------------------

#undef USE_SHIFT595                             // Add support for 74xx595 8-bit shift registers (+0k7 code)
#undef USE_TM1638                               // Add support for TM1638 switches copying Switch1 .. Switch8 (+1k code)
#undef TM1638_USE_AS_BUTTON                   // Add support for buttons
#undef TM1638_USE_AS_SWITCH                   // Add support for switches (default)
#undef USE_HX711                                // Add support for HX711 load cell (+1k5 code)
#undef USE_HX711_GUI                          // Add optional web GUI to HX711 as scale (+1k8 code)
#undef USE_DINGTIAN_RELAY                       // Add support for the Dingian board using 74'595 et 74'165 shift registers
#undef USE_TX20_WIND_SENSOR                     // Add support for La Crosse TX20 anemometer (+2k6/0k8 code)
#undef USE_TX23_WIND_SENSOR                     // Add support for La Crosse TX23 anemometer (+2k7/1k code)
#undef USE_WINDMETER                            // Add support for analog anemometer (+2k2 code)
#undef USE_FTC532                               // Add support for FTC532 8-button touch controller (+0k6 code)
#undef USE_RC_SWITCH                            // Add support for RF transceiver using library RcSwitch (+2k7 code, 460 iram)
#undef USE_RF_SENSOR                            // Add support for RF sensor receiver (434MHz or 868MHz) (+0k8 code)
#undef USE_THEO_V2                            // Add support for decoding Theo V2 sensors as documented on https://sidweb.nl using 434MHz RF sensor receiver (+1k4 code)
#undef USE_ALECTO_V2                          // Add support for decoding Alecto V2 sensors like ACH2010, WS3000 and DKW2012 weather stations using 868MHz RF sensor receiver (+1k7 code)
#undef USE_HRE                                  // Add support for Badger HR-E Water Meter (+1k4 code)
#undef USE_A4988_STEPPER                        // Add support for A4988/DRV8825 stepper-motor-driver-circuit (+10k5 code)
#undef USE_PROMETHEUS                           // Add support for https://prometheus.io/ metrics exporting over HTTP /metrics endpoint
#undef USE_NEOPOOL                              // Add support for Sugar Valley NeoPool Controller - also known under brands Hidrolife, Aquascenic, Oxilife, Bionet, Hidroniser, UVScenic, Station, Brilix, Bayrol and Hay (+14k flash, +120 mem)
#undef USE_FLOWRATEMETER                        // Add support for water flow meter YF-DN50 and similary (+1k7 code)

// -- Thermostat control ----------------------------
#undef USE_THERMOSTAT                           // Add support for Thermostat

// -- PID and Timeprop ------------------------------ // Both together will add +12k1 code
#undef USE_TIMEPROP                            // Add support for the timeprop feature (+9k1 code)
#undef USE_PID                                 // Add suport for the PID  feature (+11k2 code)
#undef USE_DRV_FILE_JSON_DEMO

// -- TLS support ----------------------------
//#define USE_TLS                               // for safeboot and BearSSL

// ----------------------
//    ESP32 specific
// ----------------------

#ifdef ESP32

// berry and autoconf
#define USE_AUTOCONF                          // Enable Esp32 autoconf feature
//#undef USE_BERRY                            // Enable Berry scripting langage

// display
#define USE_I2C                               // All I2C sensors and devices
#define USE_DISPLAY                           // Add Display support
#undef  USE_DISPLAY_TM1621_SONOFF

#define USE_INFLUXDB                          // InfluxDB integration
#define USE_WEBCLIENT_HTTPS                   // HTTPs for InfluxDB
#define USE_WIREGUARD                         // Wireguard VPN client

#define USE_RTE                               // RTE Tempo, Pointe and Ecowatt

//#define USE_DEEPSLEEP                         // Add support for deepsleep (+1k code)

//#undef USE_ESP32_SENSORS

#define USE_TLS                               // for safeboot and BearSSL
#define USE_MQTT_TLS                          // enable mqtts connexion
#define USE_LIB_SSL_ENGINE

// conso LED status
#define USE_LIGHT                              // Add support for light control
#define USE_WS2812                             // WS2812 Led string using library NeoPixelBus (+5k code, +1k mem, 232 iram) - Disable by //
#define USE_ADC                                // Add support for ADC on GPIO32 to GPIO39

#undef USE_BLE_ESP32
#undef USE_MI_ESP32
#undef USE_IBEACON

#undef USE_SR04

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

