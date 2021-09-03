You'll find here pre-compiled binaries for Teleinfo project.
These binaries are ready to be flashed on ESP8266 or ESP32.
Each binary has its own specificities :
  * **tasmota.bin** : target is ESP8266 with 1Mb memory. It won't handle LittleFS to store hirstorical files.
  * **tasmota-littlefs** : target is ESP8266 with 4Mb memory minimum. It handles a LittleFS partition to store historical files.
  * **tasmota32** : target is ESP32 with 4Mb memory minimum. It handles a LittleFS partition to store historical files.

If you want to flash these binaries, you'll need to use **esptool**.

You can easily partition your ESP8266 or ESP32 with the help of **tasmota-flash** wrapper.

Here are the command lines to flash firmware and do partitionning at the same time :

**ESP8266 1Mb**

    tasmota-flash --flash 'tasmota.bin'
    
**ESP8266 4Mb**

    tasmota-flash --flash 'tasmota-littlefs.bin'  --partition 'esp8266_partition_app1441k_spiffs1245.csv'
    
**ESP32 4Mb**

    tasmota-flash --esp32 --flash 'tasmota32.bin' --partition 'esp32_partition_app1441k_spiffs1245.csv'
