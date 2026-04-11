You'll find here pre-compiled binaries for Teleinfo project.

**Important** : If your ESP runs another firmware than mine, please do a **factory** serial flash to avoid any boot loop or crashes (partitioning is not identical).

These binaries are ready to be flashed on ESP8266 or ESP32.
Each binary has its own specificities :
  * **tasmota-teleinfo.bin** : firmware for **ESP8266 with 1M memory**. It won't handle LittleFS to store history files.
  * **tasmota-teleinfo-4m.bin** : firmware for **ESP8266 with 4M memory**. It handles a 2M LittleFS partition to store history files.
  * **tasmota-teleinfo-16m.bin** : firmware for **ESP8266 with 16M memory**. It handles a 14M LittleFS partition to store history files.
  * **tasmota32-teleinfo.bin** : firmware for **ESP32 with 4M memory**. It handles **Ethernet boards** and a 1344k LittleFS partition to store history files.
  * **tasmota32-teleinfo-denkyd4.bin** : firmware for **DenkyD4 board**.
  * **tasmota32c3-teleinfo.bin** : firmware for **ESP32C3 with 4M memory** (like sonoff basic R4). It handles a 1344k LittleFS partition to store history files.
  * **tasmota32c6-teleinfo-winky.bin** : firmware for **ESP32C6 Winky board**.
  * **tasmota32s2-teleinfo.bin** : firmware for **ESP32S2 with 4M memory**. It handles a 1344k LittleFS partition to store history files.
  * **tasmota32s3-teleinfo** : firmware for **ESP32S3 with 4M memory**. It handles a 1344k LittleFS partition to store history files.
  * **tasmota32s3-teleinfo-16m.bin** : firmware for **ESP32S3 with 16M memory**. It handles a 10M LittleFS partition to store history files.

For ESP8266, OTA should be done using **.bin.gz**

For ESP32, OTA should be done using **.bin** (not .factory.bin !)

If you want to flash these binaries, you'll need to use **esptool**.

You can easily partition your ESP8266 or ESP32 with the help of **tasmota-flash** wrapper.

Here are the command lines to flash firmware and do partitionning at the same time :

**ESP8266**

    tasmota-flash --erase --flash 'tasmota-teleinfo-image-name.bin'

**ESP32**

    tasmota-flash --erase --esp32 --flash 'tasmota32-teleinfo-image-name.factory.bin'
