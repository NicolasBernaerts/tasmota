Tasmota firmware modified to control VMC
=============

This evolution of Tasmota firmware has been enhanced to handle a Motor Controled Ventilator according to current humidity level.
In **Auto** mode, VMC is :
  * started as soon as humidity goes beyond a target value
  * stopped as soon as humidity goes below this target value.

Humidity sensor can either be :
  * a local SI7021 sensor connected to your Sonoff
  * a remote MQTT humidity sensor
  
This Tasmota firmware is based on sonoff original version **v8.4** modified with extended MQTT messages.

You'll get some extra Web pages on the device :
  * **/control** : page to control operating mode and with a 24h humidity/temperature graph
  * **/info.json** : Main device characteristics in JSON format

MQTT result should look like that :

    {"Time":"2020-05-16T17:11:55","SI7021":{"Temperature":22.2,"Humidity":72.4},"Timezone":{"STD":{"Offset":60,"Month":10,"Week":0,"Day":1},"DST":{"Offset":120,"Month":3,"Week":0,"Day":1}},"VMC":{"Relay":1,"Mode":3,"Label":"Automatic","Temperature":22.2,"Humidity":72.4,"Target":60,"Threshold":2},"State":{"Mode":2,"Label":"High speed"},"Humidity":{"State":nan,"Topic":"","Key":""},"TempUnit":"C"}    {"VMC":{"Relay":1,"Mode":3,"Label":"Automatic","Temperature":22.2,"Humidity":71.5,"Target":60,"Threshold":2},"State":{"Mode":2,"Label":"High speed"},"Humidity":{"State":nan,"Topic":"","Key":""}}
    {"VMC":{"Relay":1,"Mode":3,"Label":"Automatic","Temperature":22.2,"Humidity":71.6,"Target":60,"Threshold":2},"State":{"Mode":2,"Label":"High speed"},"Humidity":{"State":nan,"Topic":"","Key":""}}
    {"VMC":{"Relay":1,"Mode":3,"Label":"Automatic","Temperature":22.2,"Humidity":71.7,"Target":60,"Threshold":2},"State":{"Mode":2,"Label":"High speed"},"Humidity":{"State":nan,"Topic":"","Key":""}}
    {"VMC":{"Relay":1,"Mode":3,"Label":"Automatic","Temperature":22.2,"Humidity":71.8,"Target":60,"Threshold":2},"State":{"Mode":2,"Label":"High speed"},"Humidity":{"State":nan,"Topic":"","Key":""}}

Pre-compiled version is available with **tasmota.bin**

If you want to compile the firmware, don't forget to uncomment following line in **my_user_config.h**

    #define USE_CONFIG_OVERRIDE             // Uncomment to use user_config_override.h file. See README.md


Complete setup guide is available at http://www.bernaerts-nicolas.fr/iot/...

![Graph page](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/vmc/tasmota-vmc-graph.png)
