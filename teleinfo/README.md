Teleinfo Tasmota firmware for Linky energy meters
=============

This evolution of Tasmota firmware **v9.3.1** has been enhanced to handle France energy meters using **Teleinfo** protocol.
These meters are widely known as **Linky**.

This implementation handles :
  * TIC **historique**
  * TIC **standard**
  * Single-phase (**monophase**)
  * Three-phase (**triphase**)

It also provides some real time consumption graphs.

Please note that it is a completly different implementation than the one published early 2020 by Charles Hallard. 

Since **v6.0** onward, it is compatible with **ESP8266** and **ESP32** chipsets.

It provides thru MQTT :
  * tasmota energy data
  * all Teleinfo data
Some Teleinfo data are also added to handle easily mono-phase and tri-phase meters (**PHASE**, **SSOUSC**, **IINST1**, **SINSTS1**, ...).
It allows easy offloading of electrical appliances or heaters.

Data provided are for example :
  * **PHASE** (number of phases)
  * **ADCO**, **ADCS** (contract number)
  * **ISOUSC** (max contract current per phase) 
  * **SSOUSC** (max contract power per phase)
  * **IINST1**, **IINST2**, **IINST3** (instant current per phase)
  * **SINSTS1**, **SINSTS2**, **SINSTS3** (instant apparent power per phase)
  * **ADIR1**, **ADIR2**, **ADIR3** (overload message)
  
These meters are :
  * Classical electronic meter (white)
  * Linky meter (green)

This firmware is based on Tasmota modified with :
  * serial as 7 bits, parity Even, 1 stop bit
  * default speed as 1200 or 9600 bauds
  * interface to handle teleinfo messages
  * standard energy MQTT message (IINST, SINSTS, ADIR, ...)

You'll get one extra Web page on the device :
  * **/tic-graph** : live, daily, weekly or yearly graph
  * **/tic-msg** : real time display of received Teleinfo messages

Between your Energy meter and your Tasmota device, you'll need an adapter like this one :

![Teleinfo adapter](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/screen/teleinfo-serial-adapter.png)

With modern Linky meters, **4.7k** resistor should be replaced by a **1.5k** resistor.

You need to connect your adapter output **Tx** to any available port of your Tasmota device.
This port should be declared as **TInfo RX**.

For example, you can use :
  * ESP8266 : **GPIO03 RXD** port
  * WT32-ETH01 : **GPIO36** port
  * Olimex ESP32-POE : **GPIO02** port

Finaly, in **Configure Teleinfo** you need to select your Teleinfo adapter baud rate :
  * **1200** (original white meter or green Linky in historic mode)
  * **9600** (green Linky in standard mode)

If you are using an **ESP32** device, you can use its **Ethernet** port after selecting the proper board on the **Configure ESP32** menu.

Teleinfo protocol is described in this document : https://www.enedis.fr/sites/default/files/Enedis-NOI-CPT_54E.pdf

Pre-compiled version is available under **tasmota.bin** and **tasmota32.bin**.

MQTT result should look like that :

    compteur/tele/STATE = {"Time":"2021-03-13T09:20:26","Uptime":"0T13:20:12","UptimeSec":48012,"Heap":17,"SleepMode":"Dynamic","Sleep":50,"LoadAvg":19,"MqttCount":1,"Wifi":{"AP":1,"SSId":"hello-nantes","BSSId":"30:23:03:xx:xx:xx","Channel":5,"RSSI":64,"Signal":-68,"LinkCount":1,"Downtime":"0T00:00:05"}}
    compteur/tele/SENSOR = {"Time":"2021-03-13T09:20:26","ENERGY":{"TotalStartTime":"2021-03-13T09:20:26","Total":7970.903,"Yesterday":3.198,"Today":6.024,"Period":63,"Power":860,"Current":4.000},"TIC":{"ADCO":"061964xxxxxx","OPTARIF":"BASE","ISOUSC":"30","BASE":"007970903","PTEC":"TH..","IINST":"004","IMAX":"090","PAPP":"00860","HHPHC":"A","MOTDETAT":"000000","PHASE":1,"SSOUSC":"6000","IINST1":"4","SINSTS1":"860"},"IP":"192.168.xx.xx","MAC":"50:02:91:xx:xx:xx"}
    compteur/tele/SENSOR = {"Time":"2021-03-13T09:20:30","TIC":{"ADCO":"061964xxxxxx","OPTARIF":"BASE","ISOUSC":"30","BASE":"007970903","PTEC":"TH..","IINST":"003","IMAX":"090","PAPP":"00780","HHPHC":"A","MOTDETAT":"000000","PHASE":1,"SSOUSC":"6000","IINST1":"3","SINSTS1":"780"}}
    compteur/tele/SENSOR = {"Time":"2021-03-13T09:25:11","TIC":{"ADCO":"061964xxxxxx","OPTARIF":"BASE","ISOUSC":"30","BASE":"007970947","PTEC":"TH..","IINST":"004","IMAX":"090","PAPP":"00860","HHPHC":"A","MOTDETAT":"000000","PHASE":1,"SSOUSC":"6000","IINST1":"4","SINSTS1":"860"}}
    compteur/tele/STATE = {"Time":"2021-03-13T09:25:26","Uptime":"0T13:25:12","UptimeSec":48312,"Heap":18,"SleepMode":"Dynamic","Sleep":50,"LoadAvg":19,"MqttCount":1,"Wifi":{"AP":1,"SSId":"hello-nantes","BSSId":"30:23:03:xx:xx:xx","Channel":5,"RSSI":64,"Signal":-68,"LinkCount":1,"Downtime":"0T00:00:05"}}
    compteur/tele/SENSOR = {"Time":"2021-03-13T09:25:26","ENERGY":{"TotalStartTime":"2021-03-13T09:25:26","Total":7970.950,"Yesterday":3.198,"Today":6.071,"Period":47,"Power":860,"Current":4.000},"TIC":{"ADCO":"061964xxxxxx","OPTARIF":"BASE","ISOUSC":"30","BASE":"007970950","PTEC":"TH..","IINST":"004","IMAX":"090","PAPP":"00860","HHPHC":"A","MOTDETAT":"000000","PHASE":1,"SSOUSC":"6000","IINST1":"4","SINSTS1":"860"},"IP":"192.168.xx.xx","MAC":"50:02:91:xx:xx:xx"}

Complete setup guide is available at http://www.bernaerts-nicolas.fr/iot/...

![Main page](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/screen/tasmota-teleinfo-main.png)   ![Config page](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/screen/tasmota-teleinfo-config.png)   ![Board page](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/screen/tasmota-teleinfo-board.png)

![Grah message](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/screen/tasmota-teleinfo-message.png)   ![Grah monophase](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/screen/tasmota-teleinfo-graph.png)   ![Grah power triphase](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/screen/tasmota-teleinfo-graph-triphase.png)  ![Grah voltage triphase](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/screen/tasmota-teleinfo-voltage-triphase.png)
