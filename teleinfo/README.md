Tasmota firmware modified for France energy meters
=============

This evolution of Tasmota firmware has been enhanced to handle France energy meters using **Teleinfo** protocol.

These meters are :
  * Classical electronic meter (white)
  * Linky meter (green)

This Tasmota firmware is based on sonoff original version **v8.1** modified with :
  * serial as 7 bits, parity Even, 1 stop bit
  * default speed as 1200 or 9600 bauds
  * interface to handle teleinfo messages
  * standard energy MQTT message (IINST and PAPP)
  * warning MQTT message when max current reached (IMAX)

Between your Energy meter and your Tasmota device, you'll need an adapter like the one described in https://hallard.me/demystifier-la-teleinfo/. You need to connect your adapter **output** to your Tasmota **Rx** port.

Then, you need to declare in Tasmota :
  * **Serial Out** to **Serial Tx** (unused)
  * **Serial In** to **Serial RX**

Finaly, you need to select your Teleinfo adapter baud rate :
  * **Teleinfo 1200**
  * **Teleinfo 9600 (Linky)**

Teleinfo protocol is described in this document : https://www.enedis.fr/sites/default/files/Enedis-NOI-CPT_54E.pdf

Pre-compiled version is available under **tasmota-teleinfo.bin**

MQTT result should look like that :

    test/sonoff/tele/STATE {"Time":"2019-06-03T07:13:33","Uptime":"0T00:00:14","Vcc":3.366,"SleepMode":"Dynamic","Sleep":50,"LoadAvg":19,"POWER":"OFF","Wifi":{"AP":1,"SSId":"bernaerts-nantes-front","BSSId":"58:EF:68:55:38:E4","Channel":11,"RSSI":84,"LinkCount":1,"Downtime":"0T00:00:04"}}
    test/sonoff/tele/SENSOR {"Time":"2019-06-03T07:13:33","ENERGY":{"TotalStartTime":"2019-06-02T16:47:02","Total":0.000,"Yesterday":10340.432,"Today":0.000,"Period":0,"Power":0,"ApparentPower":0,"ReactivePower":0,"Factor":0.00,"Voltage":230,"Current":0.000}}
    test/sonoff/tele/SENSOR {"Teleinfo":{"ADPS":28}}
    test/sonoff/tele/SENSOR {"Teleinfo":{"ADPS":32}}
    test/sonoff/tele/SENSOR {"Teleinfo":{"ADPS":34}}
    test/sonoff/tele/STATE {"Time":"2019-06-03T07:15:13","Uptime":"0T00:01:54","Vcc":3.366,"SleepMode":"Dynamic","Sleep":50,"LoadAvg":19,"POWER":"OFF","Wifi":{"AP":1,"SSId":"bernaerts-nantes-front","BSSId":"58:EF:68:55:38:E4","Channel":11,"RSSI":76,"LinkCount":1,"Downtime":"0T00:00:04"}}
    test/sonoff/tele/SENSOR {"Time":"2019-06-03T07:15:13","ENERGY":{"TotalStartTime":"2019-06-02T16:47:02","Total":26645.109,"Yesterday":10340.432,"Today":26645.109,"Period":0,"Power":0,"ApparentPower":1760,"ReactivePower":1760,"Factor":0.00,"Voltage":230,"Current":7.000},"Teleinfo":{"ADCO":"020522001636","OPTARIF":"BASE","ISOUSC":"30","BASE":"026645109","PTEC":"TH..","IINST":"007","ADPS":"034","IMAX":"026","PAPP":"01760","MOTDETAT":"000000"}}
    test/sonoff/tele/SENSOR {"Teleinfo":{"ADPS":29}}
    test/sonoff/tele/SENSOR {"Teleinfo":{"ADPS":30}}
    test/sonoff/tele/STATE {"Time":"2019-06-03T07:16:53","Uptime":"0T00:03:34","Vcc":3.250,"SleepMode":"Dynamic","Sleep":50,"LoadAvg":19,"POWER":"OFF","Wifi":{"AP":1,"SSId":"bernaerts-nantes-front","BSSId":"58:EF:68:55:38:E4","Channel":11,"RSSI":80,"LinkCount":1,"Downtime":"0T00:00:04"}}

Complete setup guide is available at http://www.bernaerts-nicolas.fr/iot/...
