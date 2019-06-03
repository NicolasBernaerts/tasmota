Tasmota firmware modified for France Pilot Wire heater control
=============

This evolution of Tasmota firmware has been enhanced to handle France electrical heaters **Fil Pilote** protocol on **Sonoff Dual R2**.

This Tasmota firmware is based on sonoff original version **v6.5.0** modified with :
  * new fil pilote MQTT SENSOR message 
  * new fil pilote MQTT command message (FILPILOTE)
  * fil pilote condition on the web interface

To enable **Fil Pilote** mode on a **Sonoff Dual R2**, you need to :
  * flash provided firmware
  * connect diodes on the Sonoff output ports
 
In **my_user_config.h** you should define :

    #define USE_PILOTWIRE              // Add support for French Fil Pilote on Sonoff DUAL

Fil pilote protocol is described at http://www.radiateur-electrique.org/fil-pilote-radiateur.php

Pre-compiled version of Tasmota handling fil pilote is available : **tasmota-pilotwire.bin**

It is based on Tasmota v6.5.0.

MQTT result should look like that :

    test/tele/sonoff/STATE {"Time":"2019-04-23T20:53:29","Uptime":"0T00:08:18","Vcc":3.495,"SleepMode":"Dynamic","Sleep":50,"LoadAvg":19,"POWER1":"ON","POWER2":"ON","Wifi":{"AP":1,"SSId":"your-ssid","BSSId":"58:aa:68:bb:38:cc","Channel":11,"RSSI":86,"LinkCount":1,"Downtime":"0T00:00:08"}}
    test/tele/sonoff/SENSOR {"Time":"2019-06-01T23:56:59","DS18B20":{"Temperature":25.5},"Pilotwire":{"Relay":1,"Mode":5,"Label":"Thermostat","Offload":"OFF","Temperature":25.5,"Target":20.0,"Drift":0.0},"State":{"Mode":1,"Label":"Off"},"TempUnit":"C"}
    test/stat/sonoff/RESULT {"POWER2":"OFF"}
    test/stat/sonoff/POWER2 OFF
    test/tele/sonoff/STATE {"Time":"2019-04-23T20:55:59","Uptime":"0T00:10:48","Vcc":3.495,"SleepMode":"Dynamic","Sleep":50,"LoadAvg":19,"POWER1":"OFF","POWER2":"OFF","Wifi":{"AP":1,"SSId":"your-ssid","BSSId":"58:aa:68:bb:38:cc","Channel":11,"RSSI":90,"LinkCount":1,"Downtime":"0T00:00:08"}}
    test/tele/sonoff/SENSOR {"Time":"2019-06-01T23:57:59","DS18B20":{"Temperature":25.5},"Pilotwire":{"Relay":1,"Mode":5,"Label":"Thermostat","Offload":"OFF","Temperature":25.5,"Target":20.0,"Drift":0.0},"State":{"Mode":1,"Label":"Off"},"TempUnit":"C"}

Complete setup guide is available at http://www.bernaerts-nicolas.fr/iot/...
