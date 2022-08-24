You'll find here pre-compiled binaries for VMC project.

These binaries are ready to be flashed on ESP8266.

Each binary has its own specificities :
  * **vmc.bin** : target is ESP8266 with 1M memory. It won't handle LittleFS.
  * **vmc-1m64k.bin** : target is ESP8266 with 1M memory. It handles a 64k LittleFS partition.
  * **vmc-4m2m.bin** : target is ESP8266 with 4M memory. It handles a 2M LittleFS partition.

You can flash these binaries with **esptool.py** or with **tasmota-flash** wrapper available from this repository.

Here are the command lines to flash firmware and do partitionning at the same time :

**ESP8266 1M**

    tasmota-flash --erase --flash 'vmc.bin'

**ESP8266 1M with 64k LittleFS**

    tasmota-flash --erase --flash 'vmc-1m64k.bin'
    
**ESP8266 4M with 2M LittleFS**

    tasmota-flash --erase --flash 'vmc-4m2m.bin'
