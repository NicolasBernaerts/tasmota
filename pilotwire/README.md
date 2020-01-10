Tasmota firmware modified for France Pilotwire heater protocol
=============

This evolution of Tasmota firmware has been enhanced to handle France electrical heaters **Fil Pilote** protocol on **Sonoff Basic** and **Sonoff Dual R2**.

Protocol is a pilotwire 2 orders on **Sonoff Basic** and pilotwire 4 orders on **Sonoff Dual R2**.

This Tasmota firmware is based on version **v8.1** modified to handle **Pilotwire** with :
  * Web configuration interface
  * public page (**/control**)
  * MQTT status messages (JSON)
  * specific MQTT commands
  * automatic offload when house instant power is overloading

To enable **Pilotwire** mode on a **Sonoff Basic** or **Sonoff Dual R2**, you need to :
  * connect diodes on the Sonoff output port(s)
  * connect a DF18B20 temperature sensor on **serial RX**
  * flash provided firmware
  * Setup configuration pages
 
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

Complete setup guide will be available at http://www.bernaerts-nicolas.fr/iot/...
