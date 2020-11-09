Teleinfo Tasmota firmware for Linky energy meters
=============

This evolution of Tasmota firmware has been enhanced to handle France energy meters using **Teleinfo** protocol. These meters are widely known as **Linky**.

This implementation handles single-phase (monophase) and three-phase (triphase). It provides some real time consumption graphs.
Please note that it is a completly different implementation than the one published early 2020 by Charles Hallard. 

Since **v6.0** onward, it is compatible with **ESP8266** and **ESP32** chipsets.

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

This firmware is based on Tasmota **v9.1** modified with :
  * serial as 7 bits, parity Even, 1 stop bit
  * default speed as 1200 or 9600 bauds
  * interface to handle teleinfo messages
  * standard energy MQTT message (IINST, SINSTS, ADIR, ...)

You'll get one extra Web page on the device :
  * **/graph** : live, daily, weekly, monthly or yearly graph
  * **/tic** : real time display of received Teleinfo messages

Between your Energy meter and your Tasmota device, you'll need an adapter like this one :

![Teleinfo adapter](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/screen/teleinfo-serial-adapter.png)

With modern Linky meters, **4.7k** resistor should be replaced by a **1.5k** resistor.

You need to connect your adapter output **Tx** to any available port of your Tasmota device. This port should be declared as **TInfo RX**.
For example, you can use :
  * ESP8266 : **GPIO03 RXD** port
  * WT32-ETH01 : **GPIO36** port
  * Olimex ESP32-POE : **GPIO02** port

Finaly, in **Configure Teleinfo** you need to select your Teleinfo adapter baud rate :
  * **Teleinfo 1200** (original white meter or green Linky)
  * **Teleinfo 9600**

If you are using an **ESP32** device, you can use its Ethernet port after selecting the proper board on the **Configure ESP32** menu.

Teleinfo protocol is described in this document : https://www.enedis.fr/sites/default/files/Enedis-NOI-CPT_54E.pdf

Pre-compiled version is available under **tasmota.bin** and **tasmota32.bin**.

MQTT result should look like that :

    compteur/tele/STATE = {"Time":"2020-08-07T19:49:49","Uptime":"0T00:50:14","UptimeSec":3014,"Heap":26,"SleepMode":"Dynamic","Sleep":50,"LoadAvg":19,"MqttCount":1,"POWER":"OFF","Wifi":{"AP":1,"SSId":"bernaerts-xxxx","BSSId":"24:F5:A2:B8:AF:FB","Channel":7,"RSSI":100,"Signal":-34,"LinkCount":1,"Downtime":"0T00:00:08"}}
    compteur/tele/SENSOR = {"Time":"2020-08-07T19:49:49","ENERGY":{"TotalStartTime":"2020-01-05T13:58:16","Total":5321.567,"Yesterday":42353.109,"Today":3775.768,"Period":376,"Power":[490,0,0],"Current":[2.130,0.000,0.000]},"TIC":{"PHASE":3,"ADCO":"2147483647","ISOUSC":30,"SSOUSC":6000,"SINSTS":490,"SINSTS1":490,"IINST1":2.1,"ADIR1":8,"SINSTS2":0,"IINST2":0.0,"ADIR2":0,"SINSTS3":0,"IINST3":0.0,"ADIR3":0},"Timezone":{"STD":{"Offset":60,"Month":10,"Week":0,"Day":1},"DST":{"Offset":120,"Month":3,"Week":0,"Day":1}}}
    compteur/tele/SENSOR = {"TIC":{"SINSTS":480,"SINSTS1":740,"IINST1":3.2,"ADIR1":12,"SINSTS2":0,"IINST2":0.0,"ADIR2":0,"SINSTS3":0,"IINST3":0.0,"ADIR3":0}}
    compteur/tele/SENSOR = {"TIC":{"SINSTS":480,"SINSTS1":160,"IINST1":0.7,"ADIR1":2,"SINSTS2":0,"IINST2":0.0,"ADIR2":0,"SINSTS3":320,"IINST3":1.4,"ADIR3":5}}
    compteur/tele/SENSOR = {"TIC":{"SINSTS":480,"SINSTS1":480,"IINST1":2.1,"ADIR1":8,"SINSTS2":0,"IINST2":0.0,"ADIR2":0,"SINSTS3":0,"IINST3":0.0,"ADIR3":0}}
    compteur/tele/SENSOR = {"TIC":{"SINSTS":480,"SINSTS1":160,"IINST1":0.7,"ADIR1":2,"SINSTS2":0,"IINST2":0.0,"ADIR2":0,"SINSTS3":320,"IINST3":1.4,"ADIR3":5}}
    compteur/tele/SENSOR = {"TIC":{"SINSTS":470,"SINSTS1":470,"IINST1":2.0,"ADIR1":7,"SINSTS2":0,"IINST2":0.0,"ADIR2":0,"SINSTS3":0,"IINST3":0.0,"ADIR3":0}}


Complete setup guide is available at http://www.bernaerts-nicolas.fr/iot/...

![Main page](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/screen/tasmota-teleinfo-main.png)   ![Config page](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/screen/tasmota-teleinfo-config.png)   ![Board page](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/screen/tasmota-teleinfo-board.png)

![Grah message](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/screen/tasmota-teleinfo-message.png)   ![Grah monophase](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/screen/tasmota-teleinfo-graph.png)   ![Grah triphase](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/screen/tasmota-teleinfo-graph-triphase.png)
