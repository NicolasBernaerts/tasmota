You'll find here pre-compiled binaries for Teleinfo project.
These binaries are ready to be flashed on ESP8266 or ESP32.
Each binary has its own specificities :
  * **teleinfo.bin** : target is ESP8266 with 1M memory. It won't handle LittleFS to store hirstorical files.
  * **teleinfo-1m128k.bin** : target is ESP8266 with 1M memory. It handles a small 128k LittleFS partition to store historical files.
  * **teleinfo-4m2m.bin** : target is ESP8266 with 4M memory. It handles a 2M LittleFS partition to store historical files.
  * **teleinfo-16m14m.bin** : target is ESP8266 with 16M memory. It handles a 14M LittleFS partition to store historical files.
  * **teleinfo32.bin** : target is ESP32 with 4M memory. It handles a 1.2M LittleFS partition to store historical files.

If you want to flash these binaries, you'll need to use **esptool**.

You can easily partition your ESP8266 or ESP32 with the help of **tasmota-flash** wrapper.

Here are the command lines to flash firmware and do partitionning at the same time :

**ESP8266 1M**

    tasmota-flash --erase --flash 'teleinfo.bin'

**ESP8266 1M with 128k LittleFS**

    tasmota-flash --erase --flash 'teleinfo-1m128k.bin'

**ESP8266 4M with 2M LittleFS**

    tasmota-flash --erase --flash 'teleinfo-4m2m.bin'
    
**ESP32 4M with 1.2M LittleFS**

    tasmota-flash --esp32 --flash 'teleinfo32.bin' --partition 'esp32_partition_app1441k_spiffs1245.csv'
