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

### MQTT ###

Offload MQTT JSON result should look like that :

    23:59:26.476 MQT: chambre2nd/tele/STATE = {"Time":"2022-01-19T23:59:26","Offload":{"State":0,"Stage":0,"Device":1500},"TempUnit":"C"}}

## Pilotwire ##

Pilotwire firmware allows to handle an electrical heater using the **fil pilote** french protocol.

It manages :
  * **thermostat** according to any temperature sensor (local or remote)
  * **presence detection** to lower thermostat in case of vacancy
  * **opened window detection** to cut power when window is opened
  * **night mode** to lower temperature at night
  * **offload** management
  * **ecowatt** signal action to lower thermostat in case of risk of power cut

**Pilotwire** has been tested on **Sonoff Basic** (1Mb), **ESP01** (1Mb), **Wemos D1 Mini** (4Mb) and **ESP32** (4Mb).

Pilotwire protocol is described at http://www.radiateur-electrique.org/fil-pilote-radiateur.php

### Presence Detection ###

Presence detection works as follow :

  * Main rules
    * if no movement sensor is present, movement is considered as permanent
    * you can configure an **initial** timeout period in minutes
    * you can configure a **no movement** timeout period in minutes
  * When **Confort Mode** is set :
    * when you set comfort mode, target temperature is set to comfort mode
    * if there is no movement after **initial** timeout, target temperature is set to **eco** mode 
    * as soon as there is a movement detected, target temperature is set to comfort mode
    * if there is no movement after **no movement** timeout, target temperature is set to **eco** mode
  * When **Confort Temperature** is changed :
    * when you change comfort mode target temperature, target temperature is set back to comfort mode
    * if there is no movement after **no movement** timeout, target temperature is set to **eco** mode
 
### Hardware setup ###

You first need to connect a diode between your relay output and the heater's *fil pilote*
  * connect a temperature sensor
  * Flash firmware
  * setup the device

Typical diode to use is **1N4007**. Connexion should be done directly on the relay output :

![Sonoff Basic](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/offload-pilotwire/screen/pilotwire-diode-single.jpg)

Next you need to connect a temperature sensor to your device. Typically, you can connect a **DF18B20** temperature sensor on **GPIO03 serial RX**.

### Configuration ###

First you need to configure normal Tasmota stuff and **Offload** section.

Then, you may configure **Sensor** section. It allows you to declare topic and key of remote sensors you want to use (temperature & presence detection).

Then you need to configure **Pilotwire** section. It allows :
  * to select pilotwire protocol or direct plug management
  * to select pilotwire options (open window detection, movement detection and Ecowatt signal)
  * to configure target temperature for different cases

Finally you may configure the **Ecowatt** topic you want to subscribe to. This is done in console mode :

     # eco_topic ecowatt/tele/SENSOR

You can adjust topic to your environment.

It then allows to adjust target temperature according to Ecowatt signals level 2 (low risk of power cut) and level 3 (high risk of power cut).

**Night mode** is configure thru standard Tasmota **timers** :
  * Timer **ON** switch to **comfort** temperature
  * Timer **OFF** switches to **night mode** temperature

**open window** detection principle is simple : if temperature drops 0.5°C in less than 4mn heater is switched off, it is switched back on when temperature increase 0.2°C

**presence** detection decreases temperature to **Eco** level after a certain time of vacancy and to **No frost** level after longer vacancy.

Here is the template you can use if you are are using a Sonoff Basic.

![Template Sonoff Basic](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/offload-pilotwire/screen/tasmota-pilotwire-template.png) 


### MQTT ###

Pilotwire MQTT JSON result should look like that :

    23:59:26.476 MQT: chambre2nd/tele/STATE = {"Time":"2022-01-19T23:59:26","Pilotwire":{"Mode":2,"Status":2,"Heating":1,"Temperature":16.9,"Target":17.0,"Detect":128,"Window":0},"Offload":{"State":0,"Stage":0,"Device":1500},"TempUnit":"C"}}

## Compilation ##

Pre-compiled version of Tasmota handling **offload** and **fil pilote** are available in the **bin** directory.

If you want to compile this firmware version, you just need to :
1. install official tasmota sources
2. place or replace files from this repository
3. place specific files from **tasmota/common** repository

Here is where you should place different files from this repository and from **tasmota/common** :
* **platformio_override.ini**
* lib/default/**ArduinoJson**
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
