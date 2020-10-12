Tasmota firmware modified for France energy meters
=============

This evolution of Tasmota firmware has been enhanced to handle France energy meters using **Teleinfo** protocol.

It is a completly different implementation than the one published early 2020 by Charles Hallard. It replaces it.

It provides Teleinfo data thru MQTT to allow easy offloading of electrical appliances or heaters.
Teleinfo data have been slightly adapted to handle easily mono-phase and tri-phase meters.
Data provided are :
  * **PHASE** (number of phases)
  * **ADCO** (contract number)
  * **ISOUSC** (max contract current per phase) 
  * **SSOUSC** (max contract power per phase)
  * **IINST1**, **IINST2**, **IINST3** (instant current per phase)
  * **SINSTS1**, **SINSTS2**, **SINSTS3** (instant apparent power per phase)
  * **ADIR1**, **ADIR2**, **ADIR3** (% of instant power according to the contract, >100 means overload)
  
These meters are :
  * Classical electronic meter (white)
  * Linky meter (green)

This Tasmota firmware is based on sonoff original version **v8.1** modified with :
  * serial as 7 bits, parity Even, 1 stop bit
  * default speed as 1200 or 9600 bauds
  * interface to handle teleinfo messages
  * standard energy MQTT message (IINST, SINSTS, ADIR, ...)

You'll get one extra Web page on the device :
  * **/graph** : 24h power consumption graph (updated every 5mn)

Between your Energy meter and your Tasmota device, you'll need an adapter like this one :

![Teleinfo adapter](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/teleinfo-serial-adapter.png)

You need to connect your adapter output **Tx** to your Tasmota **Rx** port.

Then, you need to declare in Tasmota :
  * **Serial Out** to **Serial Tx** (unused)
  * **Serial In** to **Serial RX**

Finaly, you need to select your Teleinfo adapter baud rate :
  * **Teleinfo 1200** (original white meter or green Linky)
  * **Teleinfo 9600**

Teleinfo protocol is described in this document : https://www.enedis.fr/sites/default/files/Enedis-NOI-CPT_54E.pdf

Pre-compiled version is available under **tasmota.bin**

MQTT result should look like that :

    compteur/tele/STATE = {"Time":"2020-08-07T19:49:49","Uptime":"0T00:50:14","UptimeSec":3014,"Heap":26,"SleepMode":"Dynamic","Sleep":50,"LoadAvg":19,"MqttCount":1,"POWER":"OFF","Wifi":{"AP":1,"SSId":"bernaerts-xxxx","BSSId":"24:F5:A2:B8:AF:FB","Channel":7,"RSSI":100,"Signal":-34,"LinkCount":1,"Downtime":"0T00:00:08"}}
    compteur/tele/SENSOR = {"Time":"2020-08-07T19:49:49","ENERGY":{"TotalStartTime":"2020-01-05T13:58:16","Total":5321.567,"Yesterday":42353.109,"Today":3775.768,"Period":376,"Power":[490,0,0],"Current":[2.130,0.000,0.000]},"TIC":{"PHASE":3,"ADCO":"2147483647","ISOUSC":30,"SSOUSC":6000,"SINSTS":490,"SINSTS1":490,"IINST1":2.1,"ADIR1":8,"SINSTS2":0,"IINST2":0.0,"ADIR2":0,"SINSTS3":0,"IINST3":0.0,"ADIR3":0},"Timezone":{"STD":{"Offset":60,"Month":10,"Week":0,"Day":1},"DST":{"Offset":120,"Month":3,"Week":0,"Day":1}}}
    compteur/tele/SENSOR = {"TIC":{"SINSTS":480,"SINSTS1":740,"IINST1":3.2,"ADIR1":12,"SINSTS2":0,"IINST2":0.0,"ADIR2":0,"SINSTS3":0,"IINST3":0.0,"ADIR3":0}}
    compteur/tele/SENSOR = {"TIC":{"SINSTS":480,"SINSTS1":160,"IINST1":0.7,"ADIR1":2,"SINSTS2":0,"IINST2":0.0,"ADIR2":0,"SINSTS3":320,"IINST3":1.4,"ADIR3":5}}
    compteur/tele/SENSOR = {"TIC":{"SINSTS":480,"SINSTS1":480,"IINST1":2.1,"ADIR1":8,"SINSTS2":0,"IINST2":0.0,"ADIR2":0,"SINSTS3":0,"IINST3":0.0,"ADIR3":0}}
    compteur/tele/SENSOR = {"TIC":{"SINSTS":480,"SINSTS1":160,"IINST1":0.7,"ADIR1":2,"SINSTS2":0,"IINST2":0.0,"ADIR2":0,"SINSTS3":320,"IINST3":1.4,"ADIR3":5}}
    compteur/tele/SENSOR = {"TIC":{"SINSTS":470,"SINSTS1":470,"IINST1":2.0,"ADIR1":7,"SINSTS2":0,"IINST2":0.0,"ADIR2":0,"SINSTS3":0,"IINST3":0.0,"ADIR3":0}}

If you want to compile the firmware, don't forget to uncomment following line in **my_user_config.h**

    #define USE_CONFIG_OVERRIDE             // Uncomment to use user_config_override.h file. See README.md


Complete setup guide is available at http://www.bernaerts-nicolas.fr/iot/...

![Main page](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/tasmota-teleinfo-main.png)
![Grah monophase](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/tasmota-teleinfo-graph-monophase.png) ![Grah triphase](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/tasmota-teleinfo-graph-triphase.png)
