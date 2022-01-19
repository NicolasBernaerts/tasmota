Plug management with Offload capacity / Pilotwire (Fil Pilote) heater protocol management
=============

This evolution of Tasmota firmware has been enhanced to handle France electrical heaters **Fil Pilote** protocol on **Sonoff Basic** and **Wemos D1 Mini**.

This firmware handles :
  * a plug switch with offload capacity
  * a pilotwire 2 orders on **Sonoff Basic** and **Wemos D1 Mini**

To enable **Pilotwire** mode on a **Sonoff Basic** you need to :
  * connect diode on the Sonoff output port
  * flash provided firmware
  * Setup configuration pages

Typical diode to use is **1N4007**. Connexion should be done directly on the relay output :

![Sonoff Basic](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/pilotwire/screen/filpilote-diode-single.jpg)


You'll also get a **Thermostat** mode that will allow you to pilot the heater to maintain a target temperature in the room. To activate that mode, you'll need either to :
  * connect a **local DF18B20** temperature sensor (on **GPIO03 serial RX**)
  * declare a **MQTT remote** temperature sensor

Pilotwire controler provides 2 more options :
  * open window detection (activated if temperature drops of 0.5°C in less than 4mn, released when temperature goes up 0.2°C)
  * movement detection (based on RCML0516 declared as switch 1, after 1 hour, decreases temperature of 0.5°C every 30mn down to target temperature)
  * 
Here is the template you can use if you are are using a Sonoff Basic.

![Template Sonoff Basic](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/pilotwire/screen/tasmota-pilotwire-sonoff-template.png) 

You can also declare a **MQTT remote power meter** to handle **automatic offload** when global power is to high according to your energy contract.

In thermostat mode, if you use Tasmota standard timers, you'll be able to manage 2 different target temperatures :
  * Timer **ON** means normal target temperature
  * Timer **OFF** means target temperature minus night dropdown

This firmware is based on Tasmota **v9.1** modified to handle **Pilotwire** with :
  * Web configuration interface
  * extension of JSON MQTT status
  * new specific MQTT commands
  * automatic offload when global power is overloading your contract
  * timers management (switch ON = go to night mode, switch OFF = go back to normal mode)
  * **/control** public page to control thermostat
  * **/histo** public page to get offloading history
  * **/info.json** to get device main caracteristics

MQTT JSON result should look like that :

    23:59:26.476 MQT: chambre2nd/tele/STATE = {"Time":"2022-01-19T23:59:26","Uptime":"0T23:10:12","UptimeSec":83412,"Heap":23,"SleepMode":"Dynamic","Sleep":50,"LoadAvg":19,"MqttCount":5,"POWER":"OFF","Wifi":{"AP":1,"SSId":"xxxxxxx-yyyyyyy","BSSId":"30:23:03:xx:xx:xx","Channel":1,"Mode":"11n","RSSI":54,"Signal":-73,"LinkCount":1,"Downtime":"0T00:00:05"}}
    23:59:26.484 MQT: chambre2nd/tele/SENSOR = {"Time":"2022-01-19T23:59:26","DS18B20":{"Id":"030E97943560","Temperature":16.9},"Pilotwire":{"Mode":2,"Status":2,"Heating":1,"Temperature":16.9,"Target":17.0},"Offload":{"State":0,"Stage":0,"Device":1500},"TempUnit":"C"}


Pilotwire protocol is described at http://www.radiateur-electrique.org/fil-pilote-radiateur.php

Pre-compiled version of Tasmota handling fil pilote is available : **tasmota.bin**

If you want to compile it, you need to add following files from my **tasmota/common** repository :
  * xdrv_50_filesystem_cfg_csv.ino
  * xdrv_95_timezone.ino
  * xdrv_94_ip_address.ino
  * xdrv_97_remote_sensor.ino

Complete setup guide will be available at http://www.bernaerts-nicolas.fr/iot/...

![Main page](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/pilotwire/screen/tasmota-pilotwire-main.png)   ![Config page](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/pilotwire/screen/tasmota-pilotwire-config.png)   ![Control page](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/pilotwire/screen/tasmota-pilotwire-control.png)
