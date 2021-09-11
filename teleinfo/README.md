Teleinfo Tasmota firmware for Linky energy meters
=============

This evolution of Tasmota firmware **v9.5** has been enhanced to handle France energy meters using **Teleinfo** protocol.
These meters are widely known as **Linky**.

This implementation has been tested on :
  * Sagem classic meter **monophase** with TIC **historique**
  * Linky meter **monophase** with TIC **historique**
  * Linky meter **monophase** with TIC **standard**
  * Linky meter **triphase** with TIC **historique**
  * Linky meter **triphase** with TIC **standard**
  * Ace6000 meter **triphase** with TIC **PME/PMI**

It is compatible with **ESP8266** and **ESP32** chipsets.
 
Please note that it is a completly different implementation than the one published early 2020 by Charles Hallard. 

Some of these firmware versions are using a LittleFS partition to store graph data. Il allows to keep historical data over reboots.
To take advantage of this feature, make sure to follow partitioning procedure given in the **readme** of the **binary** folder.

This firmware calculates Power Factor (Cos φ) from Teleinfo totals (W) and Instant Power (VA).
It is evaluated everytime total power increases of a certain amount of W that you can configure.

It also provides :
  * some real time energy graphs (VA, W, V and Cos phi)
  * some historical energy graphs (if chipset is partitionned with LittleFS)
  * Some MQTT data (tasmota energy data, Teleinfo data and some plain meter data)

If you are using a LittleFS version, you'll also get peak apparent power and peak voltage on the graphs.

If your linky in in historic mode, it doesn't provide instant voltage. Voltage is then forced to 230V.

Teleinfo data can be transfered thru MQTT with **etiquette** and **donnee**. For example :
  * **ADCO**, **ADCS** = contract number
  * **ISOUSC** = max contract current per phase 
  * **SSOUSC** = max contract power per phase
  * **IINST**, **IINST1**, **IINST2**, **IINST3** = instant current per phase
  * **ADIR1**, **ADIR2**, **ADIR3** = overload message
  * ...

Some meter MQTT data Meter data allows easy reading and offloading of electrical appliances or heaters :
  * **PHASE** = number of phases
  * **PREF** = power per phase in the contract (VA) 
  * **IREF** = current per phase in the contract 
  * **PMAX** = maximum power per phase including an accetable % of overload (VA)  
  * **Ix** = instant current on phase **x** 
  * **Ux** = instant voltage on phase **x** 
  * **Px** = instant apparent power on phase **x** 
  * **Wx** = instant active power on phase **x** 
  * **Cx** = instant power factor on phase **x** 

This firmware provides some extra Web page on the device :
  * **/tic-graph** : live, daily and weekly graphs
  * **/tic-msg** : real time display of received Teleinfo messages

Between your Energy meter and your Tasmota device, you'll need an adapter to convert **Teleinfo** signal to **TTL serial**.

A very simple adapter diagram can be this one. Pleasee note that some Linky meters may need a resistor as low as **1k** instead of **1.5k** to avoid transmission errors.

![Simple Teleinfo adapter](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/screen/teleinfo-adapter-simple-diagram.png)

Here is a board example using a monolithic 3.3V power supply and an ESP-01.

![Simple Teleinfo board](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/screen/teleinfo-adapter-simple-board.png)

A more complex diagram can be this one. It will reshape the signal in a cleaner way, and variable resistors will allow you to have a better compatibility with different type of meters available.

![Complete Teleinfo adapter](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/screen/teleinfo-adapter-complete-diagram.png)

Here is a board example using a monolithic 3.3V power supply and an ESP-01.

![Complete Teleinfo board](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/screen/teleinfo-adapter-complete-board.png)

You need to connect your adapter output **ESP Rx** to any available serial port of your Tasmota device.

This port should be connected to your ESP UART and be declared as **TInfo RX**.

For example, you can use :
  * ESP8266 : **GPIO3 (RXD)** port
  * WT32-ETH01 : **GPIO5 (RXD)** port
  * Olimex ESP32-POE : **GPIO2** port

Finaly, in **Configure Teleinfo** you need to select your Teleinfo adapter baud rate :
  * **1200** (original white meter or green Linky in historic mode)
  * **9600** (green Linky in standard mode)

If you are using an **ESP32** device, you can use its **Ethernet** port after selecting the proper board on the **Configure ESP32** menu.

Teleinfo protocol is described in this document : https://www.enedis.fr/sites/default/files/Enedis-NOI-CPT_54E.pdf

MQTT result should look like that :

    compteur/tele/STATE = {"Time":"2021-03-13T09:20:26","Uptime":"0T13:20:12","UptimeSec":48012,"Heap":17,"SleepMode":"Dynamic","Sleep":50,"LoadAvg":19,"MqttCount":1,"Wifi":{"AP":1,"SSId":"hello-nantes","BSSId":"30:23:03:xx:xx:xx","Channel":5,"RSSI":64,"Signal":-68,"LinkCount":1,"Downtime":"0T00:00:05"}}
    compteur/tele/SENSOR = {"Time":"2021-03-13T09:20:26","ENERGY":{"TotalStartTime":"2021-03-13T09:20:26","Total":7970.903,"Yesterday":3.198,"Today":6.024,"Period":63,"Power":860,"Current":4.000},"TIC":{"ADCO":"061964xxxxxx","OPTARIF":"BASE","ISOUSC":"30","BASE":"007970903","PTEC":"TH..","IINST":"004","IMAX":"090","PAPP":"00860","HHPHC":"A","MOTDETAT":"000000","PHASE":1,"SSOUSC":"6000","IINST1":"4","SINSTS1":"860"},"IP":"192.168.xx.xx","MAC":"50:02:91:xx:xx:xx"}
    compteur/tele/SENSOR = {"Time":"2021-03-13T09:20:30","TIC":{"ADCO":"061964xxxxxx","OPTARIF":"BASE","ISOUSC":"30","BASE":"007970903","PTEC":"TH..","IINST":"003","IMAX":"090","PAPP":"00780","HHPHC":"A","MOTDETAT":"000000","PHASE":1,"SSOUSC":"6000","IINST1":"3","SINSTS1":"780"}}
    compteur/tele/SENSOR = {"Time":"2021-03-13T09:25:11","TIC":{"ADCO":"061964xxxxxx","OPTARIF":"BASE","ISOUSC":"30","BASE":"007970947","PTEC":"TH..","IINST":"004","IMAX":"090","PAPP":"00860","HHPHC":"A","MOTDETAT":"000000","PHASE":1,"SSOUSC":"6000","IINST1":"4","SINSTS1":"860"}}
    compteur/tele/STATE = {"Time":"2021-03-13T09:25:26","Uptime":"0T13:25:12","UptimeSec":48312,"Heap":18,"SleepMode":"Dynamic","Sleep":50,"LoadAvg":19,"MqttCount":1,"Wifi":{"AP":1,"SSId":"hello-nantes","BSSId":"30:23:03:xx:xx:xx","Channel":5,"RSSI":64,"Signal":-68,"LinkCount":1,"Downtime":"0T00:00:05"}}
    compteur/tele/SENSOR = {"Time":"2021-03-13T09:25:26","ENERGY":{"TotalStartTime":"2021-03-13T09:25:26","Total":7970.950,"Yesterday":3.198,"Today":6.071,"Period":47,"Power":860,"Current":4.000},"TIC":{"ADCO":"061964xxxxxx","OPTARIF":"BASE","ISOUSC":"30","BASE":"007970950","PTEC":"TH..","IINST":"004","IMAX":"090","PAPP":"00860","HHPHC":"A","MOTDETAT":"000000","PHASE":1,"SSOUSC":"6000","IINST1":"4","SINSTS1":"860"},"IP":"192.168.xx.xx","MAC":"50:02:91:xx:xx:xx"}

Complete setup guide is available at http://www.bernaerts-nicolas.fr/iot/...


 ![Grah monophase power](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/screen/tasmota-teleinfo-graph-power.png)
 
 ![Grah monophase voltage](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/screen/tasmota-teleinfo-graph-voltage.png) 
 
 ![Grah monophase Cos phi](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/screen/tasmota-teleinfo-graph-cosphi.png) 
 
![Main page](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/screen/tasmota-teleinfo-main.png)   ![Main page](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/screen/tasmota-teleinfo-main-triphase.png) ![Config main](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/screen/tasmota-teleinfo-config.png) 

![Grah message](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/screen/tasmota-teleinfo-message.png) 
![Config page](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/screen/tasmota-teleinfo-speed.png)  

![Board page](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/screen/tasmota-teleinfo-board.png)
![LittleFS page](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/teleinfo/screen/tasmota-teleinfo-littlefs.png)
