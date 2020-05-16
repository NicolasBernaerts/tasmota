Tasmota firmware modified to control VMC
=============

This evolution of Tasmota firmware has been enhanced to handle a Motor Controled Ventilator according to ambient temperature.

As soon as humidity goes beyond a certain value, VMC is started.
It is stopped as soon as humidity goes back to a target value.

This Tasmota firmware is based on sonoff original version **v8.1** modified with extended MQTT messages.

You'll get some extra Web pages on the device :
  * **/graph** : 24h humidity/temperature graph (updated every 5mn)
  * **/json** : last full extended JSON message

Pre-compiled version is available under **tasmota.bin**

MQTT result should look like that :

    {"VMC":{"Relay":1,"Mode":3,"Label":"Automatic","Temperature":22.2,"Humidity":71.5,"Target":60,"Threshold":2},"State":{"Mode":2,"Label":"High speed"},"Humidity":{"State":nan,"Topic":"","Key":""}}
    {"VMC":{"Relay":1,"Mode":3,"Label":"Automatic","Temperature":22.2,"Humidity":71.6,"Target":60,"Threshold":2},"State":{"Mode":2,"Label":"High speed"},"Humidity":{"State":nan,"Topic":"","Key":""}}
    {"VMC":{"Relay":1,"Mode":3,"Label":"Automatic","Temperature":22.2,"Humidity":71.7,"Target":60,"Threshold":2},"State":{"Mode":2,"Label":"High speed"},"Humidity":{"State":nan,"Topic":"","Key":""}}
    {"VMC":{"Relay":1,"Mode":3,"Label":"Automatic","Temperature":22.2,"Humidity":71.8,"Target":60,"Threshold":2},"State":{"Mode":2,"Label":"High speed"},"Humidity":{"State":nan,"Topic":"","Key":""}}

If you want to comile the firmware, don't forget to uncomment following line in **my_user_config.h**

    #define USE_CONFIG_OVERRIDE             // Uncomment to use user_config_override.h file. See README.md


Complete setup guide is available at http://www.bernaerts-nicolas.fr/iot/...
