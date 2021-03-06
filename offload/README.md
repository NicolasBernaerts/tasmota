Tasmota modified for Device Offloading
=============

This evolution of Tasmota firmware has been enhanced to handle automatic offload when your power contract is exceeded. It has been designed and tested with a French electronic meter called Linky, but it should work on any meter interface able to provide a JSON stream.

It has been designed and tested on different devices :
  * a **Sonoff Basic** : peak device power is declared
  * a **Sonoff S26** : peak device power is declared
  * a **Sonoff POW R2** : peak device power is mesured 

This firmware reads your contract power limit and your real time power consumption from your meter JSON stream.

As soon as you exceed your contract power, the device is offloaded, till your global power goes down enough to power back the device.

It should avoid any power stripping.

You can select the type of device connected to your Tasmota IOT.

Each type of device will have its own priority rules :
  * the delay before offloading
  * the delay before offloading removal when global power is back to normal

This firmware is based on Tasmota **v9.1** modified to handle **Offloading** with :
  * automatic offload when global power is overloading your contract
  * Web configuration interface
  * extension of JSON for MQTT status
  * new specific MQTT commands
  * **/control** public page to control device
  * **/history** public page to view 10 last offload events
  * **/info.json** public page to get device main caracteristics as JSON
  * **/history.json** public page to get 10 last offload events as JSON

MQTT JSON result should look like that :

    turenne/repassage/stat/RESULT {"POWER":"OFF"}
    iron/stat/POWER OFF
    iron/tele/STATE {"Time":"2020-10-25T16:45:38","Uptime":"0T00:13:30","UptimeSec":810,"Heap":27,"SleepMode":"Dynamic","Sleep":50,"LoadAvg":19,"MqttCount":1,"POWER":"OFF","Wifi":{"AP":1,"SSId":"xxxx","BSSId":"30:23:xx:xx:xx:20","Channel":10,"RSSI":86,"Signal":-57,"LinkCount":1,"Downtime":"0T00:00:05"}}
    iron/tele/SENSOR {"Time":"2020-10-25T16:45:38","Offload":{"State":"OFF","Stage":0,"Phase":1,"Before":4,"After":10,"Device":2500,"Max":6600,"Contract":6000,"Adjust":10,"Topic":"compteur/tele/SENSOR","KeyInst":"SINSTS1","KeyMax":"SSOUSC"},"IP":"192.168.1.77","MAC":"A4:CF:xx:xx:xx:2E"}
    iron/tele/STATE {"Time":"2020-10-25T16:45:48","Uptime":"0T00:13:40","UptimeSec":820,"Heap":27,"SleepMode":"Dynamic","Sleep":50,"LoadAvg":19,"MqttCount":1,"POWER":"OFF","Wifi":{"AP":1,"SSId":"xxxx","BSSId":"30:23:xx:xx:xx:20","Channel":10,"RSSI":84,"Signal":-58,"LinkCount":1,"Downtime":"0T00:00:05"}}
    iron/tele/SENSOR {"Time":"2020-10-25T16:45:48","Offload":{"State":"OFF","Stage":0,"Phase":1,"Before":4,"After":10,"Device":2500,"Max":6600,"Contract":6000,"Adjust":10,"Topic":"compteur/tele/SENSOR","KeyInst":"SINSTS1","KeyMax":"SSOUSC"},"IP":"192.168.1.77","MAC":"A4:CF:xx:xx:xx:2E"}

Pre-compiled version of Tasmota handling fil pilote is available : **tasmota.bin**

Complete setup guide will be available at http://www.bernaerts-nicolas.fr/iot/...

![Main page](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/offload/screen/tasmota-offload-main.png)  ![Control page](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/offload/screen/tasmota-offload-config.png)

![History page](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/offload/screen/tasmota-offload-history.png)  ![Control page](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/offload/screen/tasmota-offload-control.png) 
