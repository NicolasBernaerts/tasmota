You'll find here pre-compiled binaries for Ecowatt project.
These binaries are ready to be flashed on ESP32.
Each binary has its own specificities :
  * **ecowatt32.bin** : target is ESP32 with 4M memory. It handles a 1.2M LittleFS partition to store historical files.
  * **ecowatt32s2.bin** : target is ESP32S2 with 4M memory. It handles a 1.2M LittleFS partition to store historical files.

If you want to flash these binaries, you'll need to use **esptool**.

You can easily partition your ESP32 with the help of **tasmota-flash** wrapper.

Here are the command lines to flash firmware and do partitionning at the same time :

**ESP32 4M with 1.2M LittleFS**

    tasmota-flash --esp32 --flash 'ecowatt32.factory.bin'

**ESP32S2 4M with 1.2M LittleFS**

    tasmota-flash --esp32 --flash 'ecowatt32s2.factory.bin'
