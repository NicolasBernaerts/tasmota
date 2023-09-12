You'll find here pre-compiled binaries for VMC project.

These binaries are ready to be flashed on ESP8266.

Each binary has its own specificities :
  * **tasmota-sensor.bin** : target is ESP8266 with 1M memory. It won't handle LittleFS.
  * **tasmota32-sensor.bin** : target is ESP32 with 4M memory. It handles 1.3M LittleFS partition.
  * **tasmota-vmc.bin** : target is ESP8266 with 1M memory. It won't handle LittleFS.
  * **tasmota-vmc-4m2m.bin** : target is ESP8266 with 4M memory. It handles a 2M LittleFS partition.

You can flash these binaries with **esptool.py** or with **tasmota-flash** wrapper available from this repository.

Here are the command lines to flash firmware and do partitionning at the same time :

**ESP8266**

    tasmota-flash --erase --flash 'sensor.bin'
   
**ESP32 4M with LittleFS**

    tasmota-flash --erase --esp32 --flash 'tasmota32-sensor.factory.bin'
