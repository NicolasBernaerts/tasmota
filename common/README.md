You'll find here all the common libraries and files used by the Tasmota projects of this repository.

## xdrv_94_ip_address.ino ##

This simple driver allows graphical configuration of fixed IP.

## xsns_120_timezone.ino ##

This simple driver allows graphical configuration of Timezone and NTP server.

## xsns_99_generic_sensor.ino ##

![RCWL0516](./screen/tasmota-sensor-main-pres.png)

This generic sensor drivers allows to :
  - standardize local sensors access (temeprature and humidity)
  - weekly and yearly historize data
  - use remote sensors thru MQTT

To get all available function, just type **sensor_help** in console.

 Local sensor can be :
  * Temperature : DHT11, AM2301, SI7021, SHT30 or DS18x20
  * Humidity : DHT11, AM2301, SI7021 or SHT30
  * Movement : RCWL-0516 or HW-MS03 (declared as Counter 1)
  * Presence : HLK-LD1115H, HLK-LD1125H, HLK-LD2410 or HLK-LD2450

![Config](./screen/tasmota-sensor-config-local.png)

If enabled, weekly and/or yearly data can be recorded.

![Config](./screen/tasmota-sensor-weekly.png)

## xsns_102_ld2410.ino ##

![HLK LD2410](./screen/tasmota-ld2410-main.png)

Driver for HLK-LD2410 presence & movement detector.

Please note that this driver is different than the one in official Tasmota repository.

To get the sensor operational :
  - Connect **Rx** and **Tx**
  - Configure **LD2410-Rx** and **LD2410-Tx**
  - Select **LD2410** in **Sensor** configuration 

To get all available function, just type **ld2410_help** in console.

## xsns_102_ld2450.ino ##

![HLK LD2450](./screen/tasmota-ld2450-main.png)

Driver for HLK-LD2450 presence radar detector.

To get the sensor operational :
  - Connect **Rx** and **Tx**
  - Configure **LD2410-Rx** and **LD2410-Tx**
  - Select **LD2450** in **Sensor** configuration

To get all available function, just type **ld2450_help** in console.

You can get a realtime radar view of targets detected :

![HLK LD2450 radar](./screen/tasmota-ld2450-radar.png)

## xdrv_40_telegram_extension.ino ##

This is an extension of **xdrv_40_telegram.ino** to allow :
 - markdown syntax
 - HTML syntax
 - conversion from \n to telegram LF
 - disabling of web link preview
 - disabling of message notification
 - message update
 - message reply

## xdrv_50_filesystem_cfg_csv.ino ##

This exension provides few functions to ease the use of **.ini** configuration files and **.csv** database files.

It provides functions to :
  - Load value from a key in an **INI** file (string, int, float and long long)
  - Save value to a key in an **INI** file (string, int, float and long long)
  - Navigate from line to line in a **CSV** file (normal or reverse)
  - Extract specific column in a **CSV** line
  - File rotate based on provided naming scheme 
 
## xdrv_93_filesystem_log.ino ##

It provides functions to manage a structured event log file :
  - Append data in an existing logged event
  - Generate HTML display from an event log file
  
## Others ##

Common files are :


  * **xdrv_96_ftp_server.ino** : Embedded FTP Server
  * **xdrv_97_tcp_server.ino** : Embedded TCP server for Teleinfo stream
  * **xdrv_98_esp32_board.ino** : Handler for ESP32 Ethernet board configuration
  * **xsns_121_hlk_ld1125.ino** : Driver for HLK-LD1125 presence & movement detector
  * **xsns_122_hlk_ld1115.ino** : Driver for HLK-LD1115 presence & movement detector

External libraries are :

  * **FTPClientServer.zip** : Embedded Simple FTP server for ESP8266 and ESP32.

  * **ArduinoJson.zip** : Arduino JSON management library. You can also retrieve latest ArduinoJson library thru git.

          # cd your-project/lib/default
          # git clone https://github.com/bblanchon/ArduinoJson.git


