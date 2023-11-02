# Teleinfo & Ecowatt Tasmota firmware

## Presentation

This evolution of **Tasmota 12.5.0** firmware has been enhanced to :
  * handle France energy meters known as **Linky** using **Teleinfo** protocol
  * implements an **Ecowatt** server to publish RTE Ecowatt signals

This firmware has been developped and tested on  :
  * **Sagem Classic Monophase** with TIC **Historique**
  * **Linky Monophase** with TIC **Historique** & **Standard**
  * **Linky Triphase** with TIC **Historique** & **Standard**
  * **Ace6000 Triphase** with TIC **PME/PMI**

It has been compiled and tested on the following devices :
  * **ESP8266** 1Mb
  * **ESP12F** 4Mb and 16Mb
  * **ESP32** 4Mb
  * **ESP32S2** 4Mb
  * **ESP32S3** 16Mb

This firmware also provides :
  * a TCP server to live stream **teleinfo** data
  * a FTP server to easily retrieve graph data

Pre-compiled versions are available in the [**binary**](./binary) folder.

## Teleinfo

Please note that it is a completly different implementation than the one published early 2020 by Charles Hallard and actually on the official Tasmota repository. 

Some of these firmware versions are using a LittleFS partition to store graph data. Il allows to keep historical data over reboots.
To take advantage of this feature, make sure to follow partitioning procedure given in the **readme** of the **binary** folder.

It manages :
  * Voltage (**V**)
  * Current (**A**)
  * Instant Power (**VA** and **W**)
  * Active Power total (**Wh**)
  * Power Factor (**Cosφ**), calculated from Instant Power (VA) and meter Total (Wh)

This firmware provides some extra Web page on the device :
  * **/tic** : real time display of last received Teleinfo message
  * **/graph** : live, daily and weekly graphs
  * **/conso** : yearly consumption data
  * **/prod** : yearly production
  
According to your installation, you may get some production data.

If you are using a LittleFS version, you'll also get peak apparent power and peak voltage on the graphs.

If your linky in in historic mode, it doesn't provide instant voltage. Voltage is then forced to 230V.

If you are using an **ESP32** board, you can use the wired connexion by selection the proper board model in **Configuration/ESP32 board**.

Teleinfo protocol is described in [this document](https://www.enedis.fr/sites/default/files/Enedis-NOI-CPT_54E.pdf)

#### MQTT data

Standard **ENERGY** section is published during **Telemetry**.

You can also publish energy data under 2 different sections :
  * **TIC** : Teleinfo data are publish as is
  * **METER** : Energy data are published in a condensed form

These options can be enabled in **Configure Teleinfo** page.

Here are some example of what you'll get if you publish **TIC** section :
  * **ADCO**, **ADCS** = contract number
  * **ISOUSC** = max contract current per phase 
  * **SSOUSC** = max contract power per phase
  * **IINST**, **IINST1**, **IINST2**, **IINST3** = instant current per phase
  * **ADIR1**, **ADIR2**, **ADIR3** = overload message
  * ...

Here are some example of what you'll get if you publish **METER** section :
  * **PH** = number of phases
  * **PSUB** = power per phase in the contract (VA) 
  * **ISUB** = current per phase in the contract 
  * **PMAX** = maximum power per phase including an accetable % of overload (VA)
  * **I** = total instant current (on all phases)
  * **P** = total instant apparent power (on all phases)
  * **W** = total instant active power (on all phases)
  * **Ix** = instant current on phase **x** 
  * **Ux** = instant voltage on phase **x** 
  * **Px** = instant apparent power on phase **x** 
  * **Wx** = instant active power on phase **x** 
  * **Cx** = instant power factor on phase **x** 
  * **PROD** = instant production apparent power

MQTT result should look like that :

    compteur/tele/SENSOR = {"Time":"2021-03-13T09:20:26","ENERGY":{"TotalStartTime":"2021-03-13T09:20:26","Total":7970.903,"Yesterday":3.198,"Today":6.024,"Period":63,"Power":860,"Current":4.000},"IP":"192.168.xx.xx","MAC":"50:02:91:xx:xx:xx"}
    compteur/tele/SENSOR = {"Time":"2021-03-13T09:20:30","TIC":{"ADCO":"061964xxxxxx","OPTARIF":"BASE","ISOUSC":"30","BASE":"007970903","PTEC":"TH..","IINST":"003","IMAX":"090","PAPP":"00780","HHPHC":"A","MOTDETAT":"000000","PHASE":1,"SSOUSC":"6000","IINST1":"3","SINSTS1":"780"}}
    compteur/tele/SENSOR = {"Time":"2023-03-10T13:53:42","METER":{"PH":1,"ISUB":45,"PSUB":9000,"PMAX":8910,"U1":235,"P1":1470,"W1":1470,"I1":6.0,"C1":1.00,"P":1470,"W":1470,"I":6.0}}

#### Configuration

This Teleinfo firmware can be configured thru some **EnergyConfig** console commands :

    EnergyConfig Teleinfo parameters :
      Historique (enable teleinfo historique mode - need restart)
      Standard (enable teleinfo standard mode - need restart)
      Mode=1 (set teleinfo mode : 0=historique, 1=standard, 2=custom[9600 baud] - need restart)
      Stats (display reception statistics)
      Led=1 (enable RGB LED display status)
      Percent=100 (maximum acceptable contract in %)
      msgpol=1 (message policy : 0=Every TIC, 1=± 5% Power Change, 2=Telemetry only)
      msgtype=1 (message type : 0=None, 1=METER only, 2=TIC only, 3=METER and TIC)
      maxv=240     (graph max voltage, in V)
      maxva=9000    (graph max power, in VA or W)
      nbday=8    (number of daily logs)
      nbweek=4   (number of weekly logs)
      maxhour=8  (graph max total per hour, in Wh)
      maxday=110   (graph max total per day, in Wh)
      maxmonth=2000 (graph max total per month, in Wh)

You can use few commands at once :

      EnergyConfig percent=110 nbday=8 nbweek=12

#### Log files

If you run this firmware on an ESP having a LittleFS partition, it will generate 3 types of energy logs :
  * **teleinfo-day-nn.csv** : average values daily file with a record every ~5 mn (**00** is today's log, **01** yesterday's log, ...)
  * **teleinfo-week-nn.csv** : average values weekly file with a record every ~30 mn (**00** is current week's log, **01** is previous week's log, ...)
  * **teleinfo-year-yyyy.csv** : Consumption kWh total yearly file with a line per day and detail of hourly total for each day.
  * **production-year-yyyy.csv** : Production kWh total yearly file with a line per day and detail of hourly total for each day.

Every CSV file includes a header.

These files are used to generate all graphs other than **Live** ones.

## Ecowatt

This evolution of Tasmota firmware allows to collect [**France RTE Ecowatt**](https://data.rte-france.com/catalog/-/api/doc/user-guide/Ecowatt/4.0) signals and to publish them thru MQTT.

It uses SSL RTE API and update Ecowatt signals every hour.

It is only enabled with **ESP32**, **ESP32S2** and **ESP32S3**, as SSL connexions are using too much memory for ESP8266.

Ecowatt data are published as a specific MQTT message :

    your-device/tele/ECOWATT = {"Time":"2022-10-10T23:51:09","Ecowatt":{"dval":2,"hour":14,"now":1,"next":2,
      "day0":{"jour":"2022-10-06","dval":1,"0":1,"1":1,"2":1,"3":1,"4":1,"5":1,"6":1,...,"23":1},
      "day1":{"jour":"2022-10-07","dval":2,"0":1,"1":1,"2":2,"3":1,"4":1,"5":1,"6":1,...,"23":1},
      "day2":{"jour":"2022-10-08","dval":3,"0":1,"1":1,"2":1,"3":1,"4":1,"5":3,"6":1,...,"23":1},
      "day3":{"jour":"2022-10-09","dval":2,"0":1,"1":1,"2":1,"3":2,"4":1,"5":1,"6":1,...,"23":1}}}
 
#### Configuration

Here the **Ecowatt** server console commands :

    eco_enable <0/1>  = enable/disable ecowatt server
    eco_sandbox <0/1> = set sandbox mode (0/1)
    eco_key <key>     = set RTE base64 private key
    eco_update        = force ecowatt update from RTE server
    eco_publish       = publish ecowatt data now

You first need to declare your **Ecowatt Base64 private** that you create from your account on RTE site https://data.rte-france.com/

Once you've declared your key, you need to enable Ecowatt module as it is disabled by default.

    eco_key your_rte_key_in_base64
    eco_enable 1

Once configuration is complete, restart Tasmota.

You should see connexion in the console logs

    08:40:55.046 ECO: Token - HTTP code 200 ()
    08:40:55.344 ECO: Token - xxxxxxxxxL23OeISCK50tsGKzYD60hUt2TeESE1kBEe38x0MH0apF0y valid for 7200 seconds

#### Root CA

Root CA is included in the source code.
It has been collected form https://data.rte-france.com/ with Firefox :
 - on the home page, click on the lock just before the URL
 - select the certificate in the menu and select more information
 - on the page, select display the certificate
 - on the new page, select Global Sign tab
 - click to download PEM(cert)

## TCP server

This firmware brings a minimal embedded TCP server.

This server allows you to retrieve the complete teleinfo stream over your LAN.

Here are the commands available for this TCP server :

  * **tcp_help** : list of available commands
  * **tcp_status** : status of TCP server (running port or 0 if not running)
  * **tcp_start** [port] : start TCP server on specified port
  * **tcp_stop** : stop TCP server

When started, you can now receive your Linky teleinfo stream in real time on any Linux pc :

    # nc 192.168.1.10 8888
        SMAXSN-1	E220422144756	05210	W
        CCASN	E220423110000	01468	:
        CCASN-1	E220423100000	01444	Q
        UMOY1	E220423114000	235	(
        STGE	003A0001	:
        MSG1	PAS DE          MESSAGE         	<

Server allows only 1 concurrent connexion. Any new client will kill previous one.

## FTP server

If you are using a build with a LittleFS partition, you can access the partition thru a very basic FTP server embedded in this firmware.

This can allow you to retrieve automatically any CSV generated file.

Here are the commands available for this TCP server :
  * **ftp_help** : list of available commands
  * **ftp_status** : status of FTP server (running port or 0 if not running)
  * **ftp_start** : start FTP server on port 21
  * **ftp_stop** : stop FTP server

On the client side, credentials are :
  * login : **teleinfo**
  * password : **teleinfo**

This embedded FTP server main limitation is that it can only one connexion at a time. \
So you need to limit simultaneous connexions to **1** on your FTP client. Otherwise, connexion will fail.

## Compilation

If you want to compile this firmware version, you just need to :
1. install official tasmota sources (please get exact version given at the beginning of this page)
2. place or replace files from this repository
3. place specific files from **tasmota/common** repository
4. install **FTPClientServer** and **ArduinoJson** libraries

Here is where you should place different files.
Files should be taken from this repository and from **tasmota/common** :
| File    |  Comment  |
| --- | --- |
| **platformio_override.ini** |    |
| tasmota/**user_config_override.h**  |    |
| boards/**esp32_4M1300k_FS.json** |  Partitioning to get 1.3Mb FS on 4Mb ESP32  |
| boards/**esp32s2_4M1300k_FS.json** |  Partitioning to get 1.3Mb FS on 4Mb ESP32S2  |
| boards/**esp8266_16M14M.json** |  Partitioning to get 14Mb FS on 16Mb ESP32  |
| boards/**esp32s3_lillygo_t7s3.json** |  Partitioning for 16Mb ESP32-S3  |
| tasmota/include/**tasmota_type.h** | Redefinition of teleinfo structure |
| tasmota/tasmota_nrg_energy/**xnrg_15_teleinfo.ino** | Teleinfo driver  |
| tasmota/tasmota_drv_driver/**xdrv_01_9_webserver.ino** | Add compilation target in footer  |
| tasmota/tasmota_drv_driver/**xdrv_94_ip_address.ino** | Fixed IP address Web configuration |
| tasmota/tasmota_drv_driver/**xdrv_96_ftp_server.ino** | Embedded FTP server |
| tasmota/tasmota_drv_driver/**xdrv_97_tcp_server.ino** | Embedded Teleinfo stream server |
| tasmota/tasmota_drv_driver/**xdrv_98_esp32_board.ino** | Configuration of Ethernet ESP32 boards |
| tasmota/tasmota_sns_sensor/**xsns_104_teleinfo_graph.ino** | Teleinfo Graphs |
| tasmota/tasmota_sns_sensor/**xsns_119_ecowatt_server.ino** | Ecowatt server |
| tasmota/tasmota_sns_sensor/**xsns_120_timezone.ino** | Timezone Web configuration |
| lib/default/**ArduinoJSON** | JSON handling library used by Ecowatt server, extract content of **ArduinoJson.zip** |
| lib/default/**FTPClientServer** | FTP server library, extract content of **FTPClientServer.zip** |

If everything goes fine, you should be able to compile your own build.

## Adapter

Between your Energy meter and your Tasmota device, you'll need an adapter to convert **Teleinfo** signal to **TTL serial**.

A very simple adapter diagram can be this one. Pleasee note that some Linky meters may need a resistor as low as **1k** instead of **1.5k** to avoid transmission errors.

![Simple Teleinfo adapter](./screen/teleinfo-adapter-simple-diagram.png)

Here is a board example using a monolithic 3.3V power supply and an ESP-01.

![Simple Teleinfo board](./screen/teleinfo-adapter-simple-board.png)

You need to connect your adapter output **ESP Rx** to any available serial port of your Tasmota device.

This port should be connected to your ESP UART and be declared as **TInfo RX**.

For example, you can use :
  * ESP8266 : **GPIO3 (RXD)** port
  * WT32-ETH01 : **GPIO5 (RXD)** port
  * Olimex ESP32-POE : **GPIO2** port

Finaly, in **Configure Teleinfo** you need to select your Teleinfo adapter baud rate :
  * **1200** (original white meter or green Linky in historic mode)
  * **9600** (green Linky in standard mode)

## Screenshot ##

![Monophasé](./screen/tasmota-teleinfo-main.png)
![Triphasé](./screen/tasmota-teleinfo-main-triphase.png)
![Config page](./screen/tasmota-teleinfo-config.png)

### Realtime messages ###

![Grah message](./screen/tasmota-teleinfo-message.png) 

### Power, Voltage and Cos φ ###

![Grah monophase power](./screen/tasmota-teleinfo-graph-daily.png)
![Grah monophase power](./screen/tasmota-teleinfo-graph-weekly.png)
![Grah monophase voltage](./screen/tasmota-teleinfo-graph-voltage.png) 
![Grah monophase Cos phi](./screen/tasmota-teleinfo-graph-cosphi.png) 
 
### Totals (kWh) ###

![Yearly total](./screen/tasmota-teleinfo-total-year.png)
![Monthly total](./screen/tasmota-teleinfo-total-month.png)
![Daily total](./screen/tasmota-teleinfo-total-day.png)
