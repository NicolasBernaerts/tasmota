# Offload & Pilotwire Tasmota firmware #

These firmware are based on Tasmota **v12**. It has been enhanced to handle :
* **Offload** capacity based on a MQTT subscription to a general Meter
* **Pilotwire** to manage France electrical heaters using **Fil Pilote** protocol

This firmare provides :
  * Web configuration interface
  * extension of JSON MQTT status
  * new specific MQTT commands
  * automatic offload when global power is overloading your contract
  * timers management (switch ON = go to night mode, switch OFF = go back to normal mode)
  * **/control** public page to control the device
  * **/histo** public page to get offloading history
  * **/info.json** to get device main caracteristics

## Offload ##

**Offload** firmware provides an offload capability based on the device consumption, your actual global consumption and your contact level.

If global power exceed your contract level, device is offloaded as long as your global consumption has not drop enougth.

It has been tested on **Sonoff S26**, **Sonoff Pow R2**, **Athom power plug** and various **ESP32** boards.

### Configuration ###

You need to configure usual tasmota stuff, plus the **Offload** section.

Here, you need to configure :
  * The topic where your energy meter data are published
  * The key where to read your contract maximum power (VA)
  * The key xhere to read your global instant power (VA)
  * The power of device controled by your Tasmota
  * Priority of the device in case off offloading (several devices may offload the one after the other)
  
After next restart, offload mecanism will start.

Pilotwire
---------

**Pilotwire** has been tested on :
  * **Sonoff Basic** (1Mb)
  * **ESP01** (1Mb)
  * **Wemos D1 Mini** (4Mb)
  * **ESP32** (4Mb)

To enable **Pilotwire** mode you need to :
  * connect diode between the Sonoff output **Line** and the heater **fil pilote**
  * connect a temperature sensor
  * Flash firmware
  * setup configuration

* **Ecowatt** signal from France RTE authority

Typical diode to use is **1N4007**. Connexion should be done directly on the relay output :

![Sonoff Basic](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/offload-pilotwire/screen/pilotwire-diode-single.jpg)

You'll also get a **Confort** mode that will allow you to pilot the heater to maintain a target temperature in the room. \\
To activate that mode, you'll need either to :
  * connect a **local DF18B20** temperature sensor (on **GPIO03 serial RX**)
  * or declare a **MQTT remote** temperature sensor

If you use Tasmota standard timers, you'll be able to manage 2 different target temperatures :
  * Timer **ON** targets **normal temperature**
  * Timer **OFF** targets **night temperature**

Pilotwire controler provides 2 more options :
  * **open window detection** : if temperature drops 0.5°C in less than 4mn heater is switched off, it is switched back on when temperature increase 0.2°C
  * **movement detection** : based on detector declared as switch 7, decreases temperature 0.5°C every 30mn after first inactivity hour, down to **vacancy temperature**

Pilotwire controler can also handle **Ecowatt** MQTT signal to protect energy network.
It need to have a [**Ecowatt server**](https://github.com/NicolasBernaerts/tasmota/tree/master/ecowatt) running on the same MQTT server.
It then allows to handle Ecowatt signals :
  * in case of level 2, a specific target temperature will be applied
  * in case of level 3, another specific target temperature will be applied

Pilotwire protocol is described at http://www.radiateur-electrique.org/fil-pilote-radiateur.php

Here is the template you can use if you are are using a Sonoff Basic.

![Template Sonoff Basic](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/offload-pilotwire/screen/tasmota-pilotwire-template.png) 


MQTT
----

MQTT JSON result should look like that :

    23:59:26.476 MQT: chambre2nd/tele/STATE = {"Time":"2022-01-19T23:59:26","Uptime":"0T23:10:12","UptimeSec":83412,"Heap":23,"SleepMode":"Dynamic","Sleep":50,"LoadAvg":19,"MqttCount":5,"POWER":"OFF","Wifi":{"AP":1,"SSId":"xxxxxxx-yyyyyyy","BSSId":"30:23:03:xx:xx:xx","Channel":1,"Mode":"11n","RSSI":54,"Signal":-73,"LinkCount":1,"Downtime":"0T00:00:05"}}
    23:59:26.484 MQT: chambre2nd/tele/SENSOR = {"Time":"2022-01-19T23:59:26","DS18B20":{"Id":"030E97943560","Temperature":16.9},"Pilotwire":{"Mode":2,"Status":2,"Heating":1,"Temperature":16.9,"Target":17.0,"Detect":128,"Window":0},"Offload":{"State":0,"Stage":0,"Device":1500},"TempUnit":"C"}

Pre-compiled version of Tasmota handling **offload** and **fil pilote** are available in the **bin** directory.

Compilation
-----------

If you want to compile this firmware version, you just need to :
1. install official tasmota sources
2. place or replace files from this repository
3. place specific files from **tasmota/common** repository

Here is where you should place different files from this repository and from **tasmota/common** :
* **platformio_override.ini**
* tasmota/**user_config_override.h**
* tasmota/include/**tasmota.h**
* tasmota/tasmota_drv_driver/**xdrv_01_9_webserver.ino**
* tasmota/tasmota_drv_driver/**xdrv_50_filesystem_cfg_csv.ino**
* tasmota/tasmota_drv_driver/**xdrv_93_filesystem_log.ino**
* tasmota/tasmota_drv_driver/**xdrv_94_ip_address.ino**
* tasmota/tasmota_drv_driver/**xdrv_96_offload.ino**
* tasmota/tasmota_drv_driver/**xdrv_97_ecowatt_client.ino**
* tasmota/tasmota_sns_sensor/**xsns_120_timezone.ino**
* tasmota/tasmota_sns_sensor/**xsns_124_hlk_ld.ino**
* tasmota/tasmota_sns_sensor/**xsns_125_generic_sensor.ino**
* tasmota/tasmota_sns_sensor/**xsns_126_pilotwire.ino**

If everything goes fine, you should be able to compile your own build.

Screen shot
-----------

![Main page](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/offload-pilotwire/screen/tasmota-pilotwire-main.png) 
![Offload config page](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/offload-pilotwire/screen/tasmota-offload-config.png) 
![Pilotwire cnfig page](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/offload-pilotwire/screen/tasmota-pilotwire-config.png) 
![Control page](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/offload-pilotwire/screen/tasmota-pilotwire-control.jpg)
