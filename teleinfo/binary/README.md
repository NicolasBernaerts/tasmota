You'll find here pre-compiled binaries for Teleinfo project.

These binaries are ready to be flashed on ESP8266 or ESP32.
Each binary has its own specificities :
  * **tasmota-teleinfo.bin** : target is ESP8266 with 1M memory. It won't handle LittleFS to store history files.
  * **tasmota-teleinfo-4m2m.bin** : target is ESP8266 with 4M memory. It handles a 2M LittleFS partition to store history files.
  * **tasmota-teleinfo-16m14m.bin** : target is ESP8266 with 16M memory. It handles a 14M LittleFS partition to store history files.
  * **tasmota32-teleinfo.bin** : target is ESP32 with 4M memory. It handles a 1344k LittleFS partition to store history files.
  * **tasmota32-teleinfo-denkyd4.bin** : target is ESP32 specific DenkyD4 board.
  * **tasmota32-teleinfo-ethernet.bin** : target is ESP32 boards with Ethernet adapter.
  * **tasmota32c3-teleinfo.bin** : target is ESP32C3 with 4M memory. It handles a 1344k LittleFS partition to store history files.
  * **tasmota32c6-teleinfo-winky.bin** : target is ESP32C6 specific Winky board.
  * **tasmota32s2-teleinfo.bin** : target is ESP32S2 with 4M memory. It handles a 1344k LittleFS partition to store history files.
  * **tasmota32s3-teleinfo** : target is ESP32S3 with 4M memory. It handles a 1344k LittleFS partition to store history files.
  * **tasmota32s3-teleinfo-16m12m.bin** : target is ESP32S3 with 16M memory. It handles a 10M LittleFS partition to store history files.

For ESP8266, OTA should be done using **.bin.gz**

For ESP32, OTA should be done using **.bin** (not .factory.bin !)

If you want to flash these binaries, you'll need to use **esptool**.

You can easily partition your ESP8266 or ESP32 with the help of **tasmota-flash** wrapper.

Here are the command lines to flash firmware and do partitionning at the same time :

**ESP8266**

    tasmota-flash --erase --flash 'tasmota-teleinfo-image-name.bin'

**ESP32**

    tasmota-flash --esp32 --flash 'tasmota32-teleinfo-image-name.factory.bin'
