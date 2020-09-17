Tasmota firmware modified for France Pilotwire heater protocol
=============

This evolution of Tasmota firmware has been enhanced to handle France electrical heaters **Fil Pilote** protocol on **Sonoff Basic** and **Sonoff Dual R2**.

This firmware handles :
  * a pilotwire 2 orders on **Sonoff Basic**
  * a pilotwire 4 orders on **Sonoff Dual R2**.

To enable **Pilotwire** mode on a **Sonoff Basic** or **Sonoff Dual R2**, you need to :
  * connect diodes on the Sonoff output port(s)
  * flash provided firmware
  * Setup configuration pages
  
You'll also get a **Thermostat** mode that will allow you to pilot the heater to maintain a target temperature in the room. To activate that mode, you'll need either to :
  * connect a **local DF18B20** temperature sensor (on **serial RX** for example)
  * declare a **MQTT remote** temperature sensor

You can also declare a **MQTT remote power meter** to handle **automatic offload** when global power is to high according to your energy contract.

In thermostat mode, if you use Tasmota standard timers, you'll be able to manage 2 different target temperatures :
  * Timer **ON** means normal target temperature
  * Timer **OFF** means target temperature minus night dropdown

This Tasmota firmware is based on version **v8.4** modified to handle **Pilotwire** with :
  * Web configuration interface
  * extension of JSON MQTT status
  * new specific MQTT commands
  * automatic offload when global power is overloading your contract
  * timers management (ON = target temperature, OFF = night mode temperature)
  * **/control** public page to control thermostat
  * **/info.json** public page to get device main caracteristics

MQTT JSON result should look like that :

    mqtt/topic/tele/STATE = {"Time":"2020-01-10T02:31:52","Uptime":"0T00:06:44","UptimeSec":404,"Heap":29,"SleepMode":"Dynamic","Sleep":50,"LoadAvg":19,"MqttCount":1,"POWER":"OFF","Wifi":{"AP":1,"SSId":"bernaerts-nantes","BSSId":"30:23:03:84:B0:20","Channel":2,"RSSI":78,"Signal":-61,"LinkCount":1,"Downtime":"0T00:00:08"}}
    mqtt/topic/tele/SENSOR = {"Time":"2020-01-10T02:31:52","DS18B20":{"Id":"0119123F8828","Temperature":21.2},"Offload":{"State":"OFF","Before":0,"After":0,"Power":1000,"Contract":7000,"Topic":"nantes/energy/pzem004/tele/SENSOR","Key":"ApparentPower"},"Pilotwire":{"Relay":1,"Mode":5,"Label":"Thermostat","Temperature":21.25,"Target":24.00,"Drift":0.00,"Min":12.00,"Max":25.00},"State":{"Mode":2,"Label":"Comfort"},"TempUnit":"C"}
    mqtt/topic/tele/SENSOR = {"Offload":{"State":"ON","Before":0,"After":0,"Power":1000,"Contract":700,"Topic":"nantes/energy/pzem004/tele/SENSOR","Key":"ApparentPower"}}
    mqtt/topic/stat/RESULT = {"POWER":"ON"}
    mqtt/topic/stat/POWER = ON
    mqtt/topic/tele/SENSOR = {"Pilotwire":{"Relay":1,"Mode":5,"Label":"Thermostat","Temperature":21.25,"Target":24.00,"Drift":0.00,"Min":12.00,"Max":25.00},"State":{"Mode":1,"Label":"Off"}}
    mqtt/topic/tele/STATE = {"Time":"2020-01-10T02:32:22","Uptime":"0T00:07:14","UptimeSec":434,"Heap":29,"SleepMode":"Dynamic","Sleep":50,"LoadAvg":19,"MqttCount":1,"POWER":"ON","Wifi":{"AP":1,"SSId":"bernaerts-nantes","BSSId":"30:23:03:84:B0:20","Channel":2,"RSSI":78,"Signal":-61,"LinkCount":1,"Downtime":"0T00:00:08"}}
    mqtt/topic/tele/SENSOR = {"Time":"2020-01-10T02:32:22","DS18B20":{"Id":"0119123F8828","Temperature":21.2},"Offload":{"State":"ON","Before":0,"After":0,"Power":1000,"Contract":700,"Topic":"nantes/energy/pzem004/tele/SENSOR","Key":"ApparentPower"},"Pilotwire":{"Relay":1,"Mode":5,"Label":"Thermostat","Temperature":21.25,"Target":24.00,"Drift":0.00,"Min":12.00,"Max":25.00},"State":{"Mode":1,"Label":"Off"},"TempUnit":"C"}

Pilotwire protocol is described at http://www.radiateur-electrique.org/fil-pilote-radiateur.php

Pre-compiled version of Tasmota handling fil pilote is available : **tasmota.bin**

If you want to comile the firmware, don't forget to uncomment following line in **my_user_config.h**

    #define USE_CONFIG_OVERRIDE             // Uncomment to use user_config_override.h file. See README.md

Complete setup guide will be available at http://www.bernaerts-nicolas.fr/iot/...

![Control page](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/pilotwire/tasmota-pilotwire-control.png)
