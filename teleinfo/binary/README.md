You'll find here pre-compiled binaries for Teleinfo project.

These binaries are ready to be flashed on ESP8266 or ESP32.
Each binary has its own specificities :
  * **tasmota-teleinfo.bin** : target is ESP8266 with 1M memory. It won't handle LittleFS to store history files.
  * **tasmota-teleinfo-4m2m.bin** : target is ESP8266 with 4M memory. It handles a 2M LittleFS partition to store history files.
  * **tasmota-teleinfo-16m14m.bin** : target is ESP8266 with 16M memory. It handles a 14M LittleFS partition to store history files.
  * **tasmota32-teleinfo.bin** : target is ESP32 with 4M memory. It handles a 1.2M LittleFS partition to store history files.
  * **tasmota32s2-teleinfo.bin** : target is ESP32S2 with 4M memory. It handles a 320k LittleFS partition to store history files.
  * **tasmota32s3-teleinfo-16m10m.bin** : target is ESP32S3 with 16M memory. It handles a 10M LittleFS partition to store history files.

You can easily extract **.bin** files from **.bin.gz**

If you want to flash these binaries, you'll need to use **esptool**.

You can easily partition your ESP8266 or ESP32 with the help of **tasmota-flash** wrapper.

Here are the command lines to flash firmware and do partitionning at the same time :

**ESP8266 1M**

    tasmota-flash --erase --flash 'tasmota-teleinfo.bin'

**ESP8266 4M with 2M LittleFS**

    tasmota-flash --erase --flash 'tasmota-teleinfo-4m2m.bin'
   
**ESP8266 16M with 14M LittleFS**

    tasmota-flash --erase --flash 'tasmota-teleinfo-16m14m.bin'

**ESP32 4M with 1.2M LittleFS**

    tasmota-flash --esp32 --flash 'tasmota32-teleinfo.factory.bin'

**ESP32S2 4M with 320k LittleFS**

    tasmota-flash --esp32 --flash 'tasmota32s2-teleinfo.factory.bin'

**ESP32S3 16M with 10M LittleFS**

    tasmota-flash --esp32 --flash 'tasmota32s3-teleinfo-16m10m.factory.bin'
