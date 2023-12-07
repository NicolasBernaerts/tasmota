# Teleinfo & Ecowatt Tasmota firmware

**ATTENTION**

From **version 13** onward, partitionning has changed on **ESP32 family**, it uses new **safeboot** partitionning.

If you upgrade ESP32 from previous version, you need to do a serial flash in **erase** mode. If you do OTA, you may encounter serious instabilities. Of course you need to do it once. You'll be able to update futur versions using OTA.

ESP8266 family partitionning hasn't changed.

## Presentation

This evolution of **Tasmota 13.2.0** firmware has been enhanced to :
  * handle France energy meters known as **Linky** using **Teleinfo** protocol
  * publish RTE **Tempo** data
  * publish RTE **Ecowatt** data

This firmware has been developped and tested on  :
  * **Sagem Classic Monophase** with TIC **Historique**
  * **Linky Monophase** with TIC **Historique** & **Standard**
  * **Linky Triphase** with TIC **Historique** & **Standard**
  * **Ace6000 Triphase** with TIC **PME/PMI**

It has been compiled and tested on the following devices :
  * **ESP8266** 1Mb
  * **ESP12F** 4Mb and 16Mb
  * **ESP32** 4Mb (safeboot)
  * **Denky D4** 8Mb (safeboot)
  * **ESP32S2** 4Mb (safeboot)
  * **ESP32S3** 16Mb (safeboot)

This firmware also provides :
  * a TCP server to live stream **teleinfo** data
  * a FTP server to easily retrieve graph data

Pre-compiled versions are available in the [**binary**](./binary) folder.

## Teleinfo

Please note that it is a completly different implementation than the one published early 2020 by Charles Hallard and actually on the official Tasmota repository. 

This tasmota firmware handles consommation and production modes :
  * Consommation publishes standard **ENERGY** and specific **METER** JSON section
  * Production publishes specific **PROD** JSON section
  * You can also publish a specific **TIC** section to have all Teleinfo keys

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
  
If you are using a LittleFS version, you'll also get peak apparent power and peak voltage on the graphs.

If your linky in in historic mode, it doesn't provide instant voltage. Voltage is then forced to 230V.

If you are using an **ESP32** board with Ethernet, you can use the wired connexion by selection the proper board model in **Configuration/ESP32 board**.

Teleinfo protocol is described in [this document](https://www.enedis.fr/sites/default/files/Enedis-NOI-CPT_54E.pdf)

#### MQTT data

Standard **ENERGY** section is published during **Telemetry**.

You can also publish energy data under 2 different sections :
  * **TIC** : Teleinfo data are publish as is
  * **METER** : Consommation energy data are in a condensed form
  * **PROD** : Production energy data

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
  * **Cx** = current calculated power factor (cos φ) on phase **x** 

Here is what you'll get in **PROD** section if your Linky is in production mode :
  * **VA**  = instant apparent power
  * **W**   = instant active power 
  * **COS** = current calculated power factor (cos φ)

MQTT result should look like that :

    compteur/tele/SENSOR = {"Time":"2021-03-13T09:20:26","ENERGY":{"TotalStartTime":"2021-03-13T09:20:26","Total":7970.903,"Yesterday":3.198,"Today":6.024,"Period":63,"Power":860,"Current":4.000},"IP":"192.168.xx.xx","MAC":"50:02:91:xx:xx:xx"}
    compteur/tele/SENSOR = {"Time":"2021-03-13T09:20:30","TIC":{"ADCO":"061964xxxxxx","OPTARIF":"BASE","ISOUSC":"30","BASE":"007970903","PTEC":"TH..","IINST":"003","IMAX":"090","PAPP":"00780","HHPHC":"A","MOTDETAT":"000000","PHASE":1,"SSOUSC":"6000","IINST1":"3","SINSTS1":"780"}}
    compteur/tele/SENSOR = {"Time":"2023-03-10T13:53:42","METER":{"PH":1,"ISUB":45,"PSUB":9000,"PMAX":8910,"U1":235,"P1":1470,"W1":1470,"I1":6.0,"C1":1.00,"P":1470,"W":1470,"I":6.0}}
    compteur/tele/SENSOR = {"Time":"2023-03-10T13:54:42","PROD":{"VA":1470,"W":1470,"COS":1.00}}

#### Commands

This Teleinfo firmware can be configured thru some **EnergyConfig** console commands :

    EnergyConfig Teleinfo parameters :
      historique      set historique mode at 1200 bauds (needs restart)
      standard        set standard mode at 9600 bauds (needs restart)
      stats           display reception statistics
      percent=100     maximum acceptable % of total contract
      msgpol=1        message policy : 0=Every TIC, 1=± 5% Power Change, 2=Telemetry only
      msgtype=1       message type : 0=None, 1=METER only, 2=TIC only, 3=METER and TIC
      maxv=240        graph max voltage (V)
      maxva=9000      graph max power (VA or W)
      nbday=8         number of daily logs
      nbweek=4        number of weekly logs
      maxhour=8       graph max total per hour (Wh)
      maxday=110      graph max total per day (Wh)
      maxmonth=2000   graph max total per month (Wh)

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

## RTE Tempo and Ecowatt

This evolution of Tasmota firmware allows to collect [**France RTE**](https://data.rte-france.com/) **Tempo** and **Ecowatt** data thru SSL API and to publish them thru MQTT.

It is only enabled on **ESP32** familiies, as SSL connexions are using too much memory for ESP8266.

RTE configuration is saved under **rte.cfg** at the root of littlefs partition.

To get all available commands, just run **rte_help** in console :

    HLP: RTE server commands
    RTE global commands :
     - rte_key <key>      = set RTE base64 private key
     - rte_token          = display current token
     - rte_sandbox <0/1>  = set sandbox mode (0/1)
    ECOWATT commands :
     - eco_enable <0/1>   = enable/disable ecowatt server
     - eco_version <4/5>  = set ecowatt API version to use
     - eco_update         = force ecowatt update from RTE server 
     - eco_publish        = publish ecowatt data now
    TEMPO commands :
     - tempo_enable <0/1> = enable/disable tempo server
     - tempo_update       = force tempo update from RTE server
     - tempo_publish      = publish tempo data now

You first need to declare your **RTE Base64 private** that you create from your account on RTE site [https://data.rte-france.com/]

On RTE site make sure to enable **Ecowatt** and / or **Tempo** application to be able to access its API.



Once you've declared your key, you need to enable Ecowatt module as it is disabled by default.

    eco_key your_rte_key_in_base64
    eco_enable 1

You'll see that you ESP32 first gets a token and then gets the Ecowatt signal data. Every step is traced in the console logs.

    ECO: Token - abcdefghiL23OeISCK50tsGKzYD60hUt2TeESE1kBEe38x0MH0apF0y valid for 7200 seconds

**Root CA** is included in the source code, but connexion is currently done in unsecure mode, not using the certificate.

If you want to get Root CA from RTE site https://data.rte-france.com/ you need to follow these steps in Firefox :
 - on the home page, click on the lock just before the URL
 - select the certificate in the menu and select more information
 - on the page, select display the certificate
 - on the new page, select Global Sign tab
 - click to download PEM(cert)

Ecowatt data are published as a specific MQTT message :

    your-device/tele/ECOWATT = {"Time":"2022-10-10T23:51:09","Ecowatt":{"dval":2,"hour":14,"now":1,"next":2,
      "day0":{"jour":"2022-10-06","dval":1,"0":1,"1":1,"2":1,"3":1,"4":1,"5":1,"6":1,...,"23":1},
      "day1":{"jour":"2022-10-07","dval":2,"0":1,"1":1,"2":2,"3":1,"4":1,"5":1,"6":1,...,"23":1},
      "day2":{"jour":"2022-10-08","dval":3,"0":1,"1":1,"2":1,"3":1,"4":1,"5":3,"6":1,...,"23":1},
      "day3":{"jour":"2022-10-09","dval":2,"0":1,"1":1,"2":1,"3":2,"4":1,"5":1,"6":1,...,"23":1}}}


## TCP server

This firmware brings a minimal embedded TCP server.

This server allows you to retrieve the complete teleinfo stream over your LAN.

Type **tcp_help** to list all available commands :
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

Type **ftp_help** to list all available commands :
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
| partition/**esp32_partition_4M_app1800k_fs1200k.csv** | Safeboot partitioning to get 1.3Mb FS on 4Mb ESP32   |
| partition/**esp32_partition_8M_app3M_fs4M.csv** | Safeboot partitioning to get 4Mb FS on 8Mb ESP32   |
| partition/**esp32_partition_16M_app3M_fs12M.csv** | Safeboot partitioning to get 12Mb FS on 16Mb ESP32   |
| boards/**esp8266_16M14M.json** | ESP8266 16Mb boards  |
| boards/**esp32_4M1200k.json** | ESP32 4Mb boards  |
| boards/**esp32s2_4M1200k.json** | ESP32S2 4Mb boards  |
| boards/**denkyd4_8M4M-safeboot.json** | ESP32 Denky D4 8Mb boards  |
| boards/**esp32s3_16M12M-safeboot.json** | ESP32S3 16Mb boards  |
| tasmota/include/**tasmota_type.h** | Redefinition of teleinfo structure |
| tasmota/tasmota_nrg_energy/**xnrg_15_teleinfo.ino** | Teleinfo driver  |
| tasmota/tasmota_drv_driver/**xdrv_01_9_webserver.ino** | Add compilation target in footer  |
| tasmota/tasmota_drv_driver/**xdrv_94_ip_address.ino** | Fixed IP address Web configuration |
| tasmota/tasmota_drv_driver/**xdrv_96_ftp_server.ino** | Embedded FTP server |
| tasmota/tasmota_drv_driver/**xdrv_97_tcp_server.ino** | Embedded TCP stream server |
| tasmota/tasmota_drv_driver/**xdrv_98_esp32_board.ino** | Configuration of Ethernet ESP32 boards |
| tasmota/tasmota_sns_sensor/**xsns_104_teleinfo_graph.ino** | Teleinfo Graphs |
| tasmota/tasmota_sns_sensor/**xsns_119_ecowatt_server.ino** | Ecowatt server |
| tasmota/tasmota_sns_sensor/**xsns_126_timezone.ino** | Timezone Web configuration |
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

Finaly, in **Configure Teleinfo** you need to select your Teleinfo adapter protocol :
  * **Historique** (original white meter or green Linky in historic mode, 1200 bauds)
  * **Standard** (green Linky in standard mode, 9600 bauds)

## Main screen ##

![Monophasé](./screen/tasmota-teleinfo-main.png)
![Triphasé](./screen/tasmota-teleinfo-main-triphase.png)

If you want to remove default Tasmota energy display, you just need to run this command in console :

    websensor3 0

## Configuration##

![Config page](./screen/tasmota-teleinfo-config.png)

### Realtime messages ###

![Grah message](./screen/tasmota-teleinfo-message.png) 

### Graph for Power, Voltage and Cos φ ###

![Grah monophase power](./screen/tasmota-teleinfo-graph-daily.png)
![Grah monophase power](./screen/tasmota-teleinfo-graph-weekly.png)
![Grah monophase voltage](./screen/tasmota-teleinfo-graph-voltage.png) 
![Grah monophase Cos phi](./screen/tasmota-teleinfo-graph-cosphi.png) 
 
### Totals Counters (kWh) ###

![Yearly total](./screen/tasmota-teleinfo-total-year.png)
![Monthly total](./screen/tasmota-teleinfo-total-month.png)
![Daily total](./screen/tasmota-teleinfo-total-day.png)
