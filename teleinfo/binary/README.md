You'll find here pre-compiled binaries for Teleinfo project.

These binaries are ready to be flashed on ESP8266 or ESP32. Each binary has its own specificities :

|         Firmware           |      Target      | LittleFS |           Notice                |
| -------------------------- | ---------------- | -------- | ------------------------------- |
| tasmota-teleinfo           | **ESP8266 1Mb**  |   none   |                                 |
| tasmota-teleinfo-4m        | **ESP8266 4Mb**  |   2 Mb   |                                 |
| tasmota-teleinfo-16m       | **ESP8266 16Mb** |   14 Mb  |                                 |
| tasmota32-teleinfo         | **ESP32 4Mb**    | 1.34 Mb  | To use with Ethernet boards     |
| tasmota32-teleinfo-denkyd4 | **DenkyD4**      |          |                                 |
| tasmota32c3-teleinfo       | **ESP32C3 4Mb**  | 1.34 Mb  | Sonoff basic R4                 |
| tasmota32c6-teleinfo-winky | **Winky**        | 1.34 Mb  | Integrates deepsleep management |
| tasmota32s2-teleinfo       | **ESP32S2 4Mb**  | 1.34 Mb  |                                 |
| tasmota32s3-teleinfo       | **ESP32S3 4Mb**  | 1.34 Mb  |                                 |
| tasmota32s3-teleinfo-16m   | **ESP32S3 16Mb** |   10 Mb  |                                 |

Here are the flash method to use under Linux :

|        Flash method        |       ESP8266      |         ESP32         |
| -------------------------- | ------------------ | --------------------- |
| Serial flash (**esptool**) |   firmware.bin     |  firmware.factory.bin | 
|        **OTA**             |   firmware.bin.gz  |  firmware.bin         | 

**Important** : If your ESP runs another firmware than this one (for example official Tasmota build), as partitioning is not identical, it is **very** important to do a full serial flash first to avoid any boot loop or crashes.

The **tasmota-flash** wrapper provided in this repository should simplify the flash process for ESP8266 or ESP32.

Here are the command lines to flash firmware and do partitionning at the same time :

**ESP8266**

    tasmota-flash --erase --flash 'tasmota-image-name.bin'

**ESP32**

    tasmota-flash --erase --esp32 --flash 'tasmota32-image-name.factory.bin'
