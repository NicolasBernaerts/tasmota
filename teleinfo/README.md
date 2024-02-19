# Firmware Tasmota Teleinfo alternatif (avec gestion RTE Tempo, Pointe & Ecowatt)

⚠️ Depuis la **version 13**, le partitionnement a évolué pour la famille des **ESP32**. Il utilise maintenant le partitionnement standard **safeboot**.

Si vous faites une mise à jour ESP32 depuis une version plus ancienne, Vous devez faire un flash **serial**.

Si vous faites une mise à jour **OTA** vous pourrez rencontrer des dysfonctionnements. Mais à partir du moment où vous disposez d'une version **13++**, vous pouvez bien entendu réaliser les mises à jour en mode **OTA**.

Le partitionnement des **ESP8266** n'a pas changé.

## Presentation

Cette évolution du firmware **Tasmota 13.2.0** permet de :
  * gérer les compteurs français (**Linky**, **PME/PMI** et **Emeraude**) utilisant le protocole **Teleinfo**
  * s'abonner aux API RTE **Tempo**, **Pointe** et **Ecowatt**

ce firmware a été développé et testé sur les compteurs suivants :
  * **Sagem Classic monophase** en TIC **Historique**
  * **Linky monophase** en TIC **Historique** & **Standard**
  * **Linky triphase** en TIC **Historique** & **Standard**
  * **Ace6000 triphase** en TIC **PME/PMI**
  * **Emeraude** en TIC **Emeraude 2 quadrands**

Il a été compilé et testé sur les ESP suivants :
  * **ESP8266** 1Mb
  * **ESP12F** 4Mb and 16Mb
  * **ESP32** 4Mb (safeboot)
  * **Denky D4** 8Mb (safeboot)
  * **ESP32C3** 4Mb (safeboot)
  * **ESP32S2** 4Mb (safeboot)
  * **ESP32S3** 16Mb (safeboot)

Ce firmware fournit également :
  * un serveur intégré **TCP** pour diffuser en temps réel les données reçues du compteur
  * un serveur intégré **FTP** pour récupérer les fichiers historiques

Des versions pré-compilées sont disponibles dans le répertoire [**binary**](./binary).

## Teleinfo

Ce firmware n'est pas le firmware officiel **Teleinfo** de **Tasmota**. C'est une implémentation complètement différente de celle publiée en 2020 par Charles Hallard. 

Il gère les compteurs en mode consommation et production. 

Ce firmware gère les données suivantes :
  * Tension (**V**)
  * Courant (**A**)
  * Puissance instantanée et active (**VA** & **W**)
  * Compteurs de période (**Wh**)
  * Facteur de puissance (**Cosφ**), calculé sur la base du compteur de période et de la puissance instantanée.

Il fournit des pages web spécifiques :
  * **/tic** : suivi en temps réel des données réçues
  * **/graph** : graph en temps réel des données du compteur
  * **/conso** : suivi des consommations
  * **/prod** : suivi de la production
  
Si votre compteur est en mode historique, la tension est forcée à 230V.

Si vous souhaitez supprimer l'affichage des données Energy sur la page d'accueil, vous devez passer la commande suivante en console :

    websensor3 0

Le protocole **Teleinfo** est décrit dans [ce document](https://www.enedis.fr/sites/default/files/Enedis-NOI-CPT_54E.pdf)

#### Publication MQTT

En complément de la section officielle **ENERGY**, les sections suivantes peuvent être publiées :
  * **METER** : données de consommation et prodcution en temps réel sous une forme compacte.
  * **ALERT** : alertes publiées dans les messages STGE (changement Tempo / EJP, surpuissance & survoltage)
  * **TOTAL** : compteurs de périodes en Wh
  * **CAL** : calendrier consolidé entre le compteur et les données RTE (Tempo, Pointe & Ecowatt)
  * **RELAY** : relais virtuels publiés par le compteur
  * **TIC** : etiquettes et données brutes reçues depuis le compteur

Toutes ces publications sont activables à travers la page **Configuration Teleinfo**.

Voici la liste des données publiées dans la section **METER** :
  * **PH** = nombre de phases (1 ou 3)
  * **PSUB** = puissance apparente (VA) maximale par phase dans le contrat
  * **ISUB** = courant (A) maximal par phase dans le contrat 
  * **PMAX** = puissance apparente (VA) maximale par phase intégrant le pourcentage acceptable
  * **I** = courant instantané (A) global
  * **P** = puissance instantanée (VA) globale
  * **W** = puissance active (W) globale
  * **Ix** = courant instantané (A) sur la phase **x** 
  * **Ux** = tension (V) sur la phase **x** 
  * **Px** = puissance instantanée (VA) sur la phase **x** 
  * **Wx** = puissance active (W) sur la phase **x** 
  * **Cx** = facteur de puissance (cos φ) sur la phase **x**
  * **PP** = puissance instantanée (VA) produite
  * **PW** = puissance active (W) produite
  * **PC** = facteur de puissance (cos φ) de la production

Voici les données publiées dans la section **CAL** :
  * **lv** = niveau de la période actuelle (0 inconnu, 1 bleu, 2 blanc, 3 rouge)
  * **hp** = type de la période courante (0:heure creuse, 1 heure pleine)
  * **today** = section avec le niveau et le type de chaque heure du jour
  * **tomorrow** = section avec le niveau et le type de chaque heure du lendemain

Voici les données publiées dans la section **RELAY** :
  * **R1** = état du relai virtual n°1 (0:ouvert, 1:fermé)
  * **R2** = état du relai virtual n°2 (0:ouvert, 1:fermé)
  * .. 

Voici les données publiées dans la section **ALERT** :
  * **Load** = indicateur de surconsommation (0:pas de pb, 1:surconsommation)
  * **Volt** = indicateur de surtension (0:pas de pb, 1:au moins 1 phase est en surtension)
  * **Preavis** = niveau du prochain préavis (utilisé en Tempo & EJP)
  * **Label** = Libellé du prochain préavis

La section **TOTAL** comprend autant de clés que de périodes dans votre contrat. Seules les périodes avec un total de consommation différent de **0** sont publiées.

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

#### LittleFS

Certaines variantes de ce firmware utilisent une partition **LittleFS** pour stocker les données historisées qui servent à générer les graphs de suivi. Lorsque vous souhaitez utiliser cette fonctionnalité, vérifier que vous flashez bien l'ESP en mode série la première fois.
Si vous utilisez une version avec partition **LittleFS**, les graphs afficheront en complément les tensions et puissances crête.

If you run this firmware on an ESP having a LittleFS partition, it will generate 3 types of energy logs :
  * **teleinfo-day-nn.csv** : average values daily file with a record every ~5 mn (**00** is today's log, **01** yesterday's log, ...)
  * **teleinfo-week-nn.csv** : average values weekly file with a record every ~30 mn (**00** is current week's log, **01** is previous week's log, ...)
  * **teleinfo-year-yyyy.csv** : Consumption kWh total yearly file with a line per day and detail of hourly total for each day.
  * **production-year-yyyy.csv** : Production kWh total yearly file with a line per day and detail of hourly total for each day.

Every CSV file includes a header.

These files are used to generate all graphs other than **Live** ones.

## RTE Tempo, Pointe and Ecowatt

This evolution of Tasmota firmware allows to collect [**France RTE**](https://data.rte-france.com/) data thru their Web Services :
  * **Tempo**
  * **Pointe**
  * **Ecowatt**
These data are then published thru MQTT.

![RTE applications](./screen/tasmota-rte-display.png)

It is only enabled on **ESP32** familiies, as SSL connexions are using too much memory for ESP8266.

RTE configuration is saved under **rte.cfg** at the root of littlefs partition.

To get all available commands, just run **rte_help** in console :

    HLP: RTE server commands
    RTE global commands :
     - rte_key <key>      = set RTE base64 private key
     - rte_token          = display current token
     - rte_sandbox <0/1>  = set sandbox mode (0/1)
    Ecowatt commands :
     - eco_enable <0/1>   = enable/disable ecowatt server
     - eco_display <0/1>  = display ecowatt calendra in main page
     - eco_version <4/5>  = set ecowatt API version to use
     - eco_update         = force ecowatt update from RTE server 
     - eco_publish        = publish ecowatt data now
    Tempo commands :
     - tempo_enable <0/1>  = enable/disable tempo server
     - tempo_display <0/1> = display tempo calendra in main page
     - tempo_update        = force tempo update from RTE server
     - tempo_publish       = publish tempo data now
    Pointe commands :
     - pointe_enable <0/1> = enable/disable pointe period server
     - pointe_display <0/1 = display pointe calendra in main page
     - pointe_update       = force pointe period update from RTE server
     - pointe_publish      = publish pointe period data now
  
You first need to create **RTE account** from RTE site [https://data.rte-france.com/]

Then enable **Tempo**, **Demand Response Signal** and/or **Ecowatt** application to be able to access its API.

![RTE applications](./screen/rte-application-list.png) 

Once your applications have been enabled, copy and declare your **private Base64 key** in console mode :

    rte_key your_rte_key_in_base64

You can then enable **Tempo**, **Pointe**  and/or **Ecowatt** data collection : 

    eco_enable 1
    tempo_enable 1
    pointe_enable 1

After a restart you'll see that you ESP32 first gets a token and then gets the data. Every step is traced in the console logs.

    RTE: Token - abcdefghiL23OeISCK50tsGKzYD60hUt2TeESE1kBEe38x0MH0apF0y valid for 7200 seconds
    RTE: Ecowatt - Success 200
    RTE: Tempo - Update done (2/1/1)

Tempo, Pointe and Ecowatt data are published as SENSOR data :

    your-device/tele/SENSOR = {"Time":"2023-12-20T07:23:39",TEMPO":{"lv":1,"hp":0,"label":"blue","icon":"🟦","yesterday":1,"today":1,"tomorrow":1}}

    your-device/tele/SENSOR = {"Time":"2023-12-20T07:36:02","POINTE":{"lv":1,"label":"blue","icon":"🟦","today":1,"tomorrow":1}}

    your-device/tele/SENSOR = {"Time":"2022-10-10T23:51:09","ECOWATT":{"dval":2,"hour":14,"now":1,"next":2,
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
| tasmota/tasmota_sns_sensor/**xsns_119_rte_server.ino** | RTE Tempo and Ecowatt data collection |
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
