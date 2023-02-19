You'll find here all the common libraries and files used by the Tasmota projects of this repository.

Common files are :

  * **xdrv_40_telegram_extension.ino** : Allow to send and update Telegram messages
  * **xdrv_50_filesystem_cfg_csv.ino** : Configuration (INI) and CSV file manager for LittleFS
  * **xdrv_93_filesystem_log.ino** : Log file manager for LittleFS
  * **xdrv_94_ip_address.ino** : Fixed IP address handler
  * **xdrv_96_ftp_server.ino** : Embedded FTP Server
  * **xdrv_97_tcp_server.ino** : Embedded TCP server for Teleinfo stream
  * **xdrv_98_esp32_board.ino** : Handler for ESP32 Ethernet board configuration
  * **xsns_120_timezone.ino** : Timezone setup manager
  * **xsns_102_hlk_ld2410.ino** : Driver for HLK-LD2410 presence & movement detector
  * **xsns_121_hlk_ld1125.ino** : Driver for HLK-LD1125 presence & movement detector
  * **xsns_122_hlk_ld1115.ino** : Driver for HLK-LD1115 presence & movement detector
  * **xsns_125_generic_sensor.ino** : Generic temperature, humidity and movement sensor, handling MQTT remote sensors 

External libraries are :

  * **FTPClientServer.zip** : Embedded Simple FTP server for ESP8266 and ESP32.

  * **ArduinoJson.zip** : Arduino JSON management library. You can also retrieve latest ArduinoJson library thru git.

          # cd your-project/lib/default
          # git clone https://github.com/bblanchon/ArduinoJson.git


