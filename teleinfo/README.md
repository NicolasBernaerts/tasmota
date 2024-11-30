# Firmware Tasmota Teleinfo

⚠️ Ce firmware n'est pas le firmware officiel **Teleinfo** de **Tasmota** publié en 2020 par Charles Hallard. 

Pour la famille des **ESP32**, merci de faire systématiquement un premier flash en mode **Série** afin de regénérer le partitionnement et d'éviter tout dysfonctionnement. Vous pourrez alors faire toutes les mises à jour suivantes en mode **OTA**.

Si votre ESP est un **Denky D4**, vous pouvez flasher directement le firmware Denky D4 en mode **OTA**.

Le **changelog** général est disponible dans le fichier **user_config_override.h**

![Homepage page](./screen/tasmota-teleinfo-homepage.png)

## Presentation

Cette évolution du firmware **Tasmota 14.1** permet de :
  * gérer le flux **Teleinfo** des compteurs français (**Linky**, **PME/PMI** et **Emeraude**)
  * gérer les compteurs en mode **Historique** et en mode **Standard**
  * gérer les compteurs en mode **Consommation** et/ou **Production**
  * calculer le **Cosφ** en quasi temps réel
  * publier les données pour **Domoticz**, **Home Assistant**, **Homie** et **Thingsboard**
  * s'abonner aux API RTE **Tempo**, **Pointe** et **Ecowatt**

Ce firmware a été développé et testé sur les compteurs suivants :
  * **Sagem Blanc monophase** en TIC **Historique**
  * **Actaris A14C5** en TIC **Historique** (merci à Charles Hallard pour le cadeau)
  * **Linky monophase** en TIC **Historique** & **Standard**
  * **Linky triphase** en TIC **Historique** & **Standard**
  * **Ace6000 triphase** en TIC **PME/PMI**
  * **Emeraude** en TIC **Emeraude 2 quadrands**

Il a été compilé et testé sur les ESP suivants :
  * **ESP8266** 1Mb (Sonoff Basic R2), 4Mb et 16Mb
  * **ESP32** 4Mb et **ESP32 Denky D4** 8Mb
  * **ESP32C3** 4Mb (Sonoff Basic R4 par exemple)
  * **ESP32C3 Winky** 4Mb (auto-alimenté par le compteur)
  * **ESP32C6 Winky** 4Mb (auto-alimenté par le compteur)
  * **ESP32S2** 4Mb
  * **ESP32S3** 4Mb et 16Mb

Ce firmware fournit également :
  * un serveur intégré **TCP** pour diffuser en temps réel les données reçues du compteur
  * un serveur intégré **FTP** pour récupérer les fichiers historiques
  * le suivi en temps réel des trames réçues
  * un graph en temps réel des données principales (tension, puissance et Cosφ)
  * suivi historisé de la consommation
  * suivi historisé de la production

Si votre compteur est en mode historique, la tension est forcée à 230V.

Des versions pré-compilées sont disponibles dans le répertoire [**binary**](./binary).

Le protocole **Teleinfo** est décrit dans [ce document](https://www.enedis.fr/sites/default/files/Enedis-NOI-CPT_54E.pdf)

## Fonctionnalités

Suivant le type d'ESP utilisé, toutes les fonctionnalités ne sont pas disponibles.

Voici un tableau récapitulatif des fonctionnalités par famille d'ESP :

|       Fonctionnalité        | ESP8266 1M | ESP8266 4M+ | ESP32 4M+ |
| --------------------------- | ---------- | ----------- | --------  |
| IP fixe                     |     x      |      x      |     x     |
| Calcul Cosφ                 |     x      |      x      |     x     |
| LED couleur contrat         |     x      |      x      |     x     |
| Trames temps réel           |     x      |      x      |     x     |
| Graph temps réel            |     x      |      x      |     x     |
| Graph historisé             |            |      x      |     x     |
| Consommation historisée     |            |      x      |     x     |
| Production historisée       |            |      x      |     x     |
| Serveur TCP                 |     x      |      x      |     x     |
| Serveur FTP                 |            |             |     x     |
| Intégration Home Assistant  |     x      |      x      |     x     |
| Intégration Domoticz        |     x      |      x      |     x     |
| Intégration Homie           |     x      |      x      |     x     |
| Intégration Thingsboard     |     x      |      x      |     x     |
| Intégration InfluxDB        |            |             |     x     |
| Intégration API RTE         |            |             |     x     |
| Taille max d'une étiquette  |    32      |    32       |    112    |
| Nombre max d'étiquettes     |    48      |    48       |    74     |

## Options de configuration

Ce firmware propose différentes options de configuration. Merci de bien lire ce qui suit afin d'avoir un système optimisé.

### Teleinfo

![Donnees publiées](./screen/tasmota-teleinfo-config-mode.png)

Ces options permettent de définir le type de compteur auquel est connecté le module.
Les anciens compteurs sont tous en mode **Historique**.
Les Linky sont principalement en mode **Historique** mais ceux utilisés avec les nouveaux contrats sont le plus souvent configurés en mode **Standard**.
Si vous modifiez cette option, le module redémarrera après sauvegarde.

### Données publiées

![Donnees publiées](./screen/tasmota-teleinfo-config-donnees.png)

#### Energie Tasmota
Cette option publie la section **ENERGY**, publication standard de tasmota. La plupart du temps vous n'avez pas besoin de cocher cette option car elle ne prend pas en compte la moitié des données publiées par un compteur Linky.
#### Consommation & Production
Cette option publie la section **METER** et **CONTRACT**. C'est l'option que vous devriez cocher par défaut. Elle permet de publier toutes les données utiles du compteur en mode consommation, production et/ou auto-consommation. Ce sont en particulier les données utilisées par Home Assistant.
#### Relais virtuels
Cette option publie la section **RELAY**. Elle permet de s'abonner à l'état des relais virtuels publiés par le compteur ou à des relais fonction de la période en cours du contrat. Elle est à utiliser avec des device ayant été programmés avec mon firmware **Relai**.
#### Calendrier
Cette option publie la section **CAL**. Elle permet de publier les couleurs de contrat heure / heure pour le jour courant et le lendemain.

### Politique de publication

![Donnees publiées](./screen/tasmota-teleinfo-config-publication.png)

Cette option vous permet de définir la fréquence de publication des données :
  - **A chaque télémétrie** : Les données sont publiées à chaque déclenchement de la télémétrie, configurée par **Période télémétrie**.
  - **Evolution de +-** : Les données sont publiées chaque fois que la puissance varie de la valeur configurée sur l'une des phases. C'est mon option de prédilection.
  - **A chaque message reçu** : Les données sont publiées à chaque trame publiée par le compteur, soit toutes les 1 à 2 secondes. Cette option n'est à utiliser que dans des cas très particuliers car elle stresse fortement l'ESP.

### Intégration

![Donnees publiées](./screen/tasmota-teleinfo-config-integration.png)

Ces options permettent de publier les données dans un format spécifiquement attendu par un logiciel domotique ou SGBD.
Les données sont émises au boot après la réception de quelques messages complets depuis le compteur.
Cela permet d'émettre des données correspondant exactement au contrat lié au compteur raccordé.

#### Home Assistant

Toutes les données sélectionnées dans **Données publiées** sont annoncées à Home Assistant à chaque démarrage.

Comme les données sont annoncées à HA, vous ne devriez plus avoir qu'à les sélectionner dans HA, qui s'abonnera et utilisera les données publiées. 

Dans le cas particulier du Wenky, les messages d'auto-découverte ne sont pas émis au réveil s'il ne dispose pas d'une alimentation fixe via USB.

![Home Assistant integration](./screen/tasmota-ha-integration-1.png)  ![Home Assistant integration](./screen/tasmota-ha-integration-2.png)

#### Homie

Les données sont publiées dans un format spécifique reconnu par les applications domotique compatibles [**Homie**](https://homieiot.github.io/). A chaque boot, toutes les données candidates à intégration dans un client **Homie** sont émises via MQTT en mode **retain**.

Dans le cas particulier du Wenky, les messages d'auto-découverte ne sont pas émis au réveil s'il ne dispose pas d'une alimentation fixe via USB.

#### Thingsboard

Les données sont publiées dans un format spécifique reconnu nativement par la plateforme IoT  [**Thingsboard**](https://thingsboard.io/).

Le paramétrage à appliquer coté **Tasmota** et coté **Thingsboard** pour que les données soient publiées et consommées est le suivant :

![Tasmota config](./screen/tasmota-thingsboard-config.jpg)  ![Thingsboard device](./screen/thingsboard-device.jpg)  ![Thingsboard credentials](./screen/thingsboard-credentials.jpg)

#### Domoticz

Les données sont publiées dans un format spécifique reconnu nativement par Domoticz.

Une fois l'option sélectionnée et sauvegardée, vous pourrez définir les index Domoticz définis pour chacune des données publiées. Pour chaque donnée, un tooltip explique le type de données à définir dans Domoticz.

#### InfluxDB

Les principales données sont publiées sur un serveur InfluxDB, à travers les API https :
  * mode du contrat (historique:1, standard:2, PME/PMI:3, Emeraude:4, Jaune:5)
  * nombre de périodes dans le contrat et index de la période en cours
  * niveau de la période en cours (bleu:1, blanc:2, rouge:3)
  * type de la période (hc:0, hp:1)
  * pour chaque phase : courant, tension, puissance apparente & puissance active
  * cosphi
  * si le compteur est en mode production : puissance apparente, puissance active & cosphi

Une fois l'option sélectionnée et sauvegardée, vous pourrez définir les caractéristiques de votre serveur InfluxDB.

Les données publiées devraient apparaître automatiquement sur votre instance InfluxDB.

### Spécificités

Ces options ne sont pas nécessaires dans la plupart des cas, en particulier si vous utilisez une solution domotique.

Si vous n'en avez pas un besoin express, évitez de les sélectionner.

#### Données temps réel

Toutes les données liées à la consommation et la production sont publiée en complément sur un topic **LIVE/...** toutes les 3 secondes.

#### Données Teleinfo brutes

Les données recues depuis le compteur sont publiées telles quelles en complément sur un topic **TIC/...**.

Ces données étant des données brutes, elles n'ont d'autre intérêt que l'analyse des trames en cas de problème.

## Publication MQTT

Dans le topic **../SENSOR**, les sections suivantes seront publiées selon votre configuration : 
  * **ENERGY** : section officielle de Tasmota, qui ne contien qu'un sous ensemble de données
  * **METER** : données normalisées de consommation et production en temps réel
  * **CONTRACT** : données normalisées du contrat intégrant les compteurs de périodes en Wh
  * **CAL** : calendrier heure/heure du jour et du lendemain, consolidé entre la publication compteur et les données RTE reçues (Tempo, Pointe et/ou Ecowatt)
  * **RELAY** : relais virtuels publiés par le compteur
  * **ALERT** : alertes publiées dans les messages STGE (changement Tempo / EJP, surpuissance & survoltage)

Toutes ces publications sont activables à travers la page **Configuration Teleinfo**.

|    Section   |     Clé     |  Valeur   |
| ------------ | ----------- | ----------- |
| **METER**    |    PH       | Nombre de phases (1 ou 3)  | 
|              |   PSUB      | Puissance apparente (VA) maximale par phase dans le contrat    | 
|              |   ISUB      | Courant (A) maximal par phase dans le contrat    | 
|              |   PMAX      | Puissance apparente (VA) maximale par phase intégrant le pourcentage acceptable   | 
|              |     I       | Courant global (A)    | 
|              |     P       | Puissance apparente globale (VA)   | 
|              |     W       | Puissance active globale (W)    | 
|              |     C       | Facteur de puissance (cos φ)   | 
|              |    I*x*     | Courant (A) sur la phase **_x_**   | 
|              |    U*x*     | Tension (V) sur la phase **_x_**    | 
|              |    P*x*     | Puissance apparente (VA) sur la phase **_x_**    | 
|              |    W*x*     | Puissance active (W) sur la phase **_x_**   | 
|              |    TDAY     | Puissance totale consommée aujourd'hui (Wh)   | 
|              |    YDAY     | Puissance totale consommée hier (Wh)   | 
|              |    PP       | Puissance apparente **produite** (VA) | 
|              |    PW       | Puissance active **produite** (VA) | 
|              |    PC       | Facteur de puissance (cos φ) de la **production**  | 
|              |   PTDAY     | Puissance totale **produite** aujourd'hui (Wh) | 
|              |   PYDAY     | Puissance totale **produite** hier (Wh) | 
| **CONTRACT** |   serial    | Numéro de série du compteur    | 
|              |    name     | Nom du contrat en cours        | 
|              |   period    | Nom de la periode en cours     | 
|              |    color    | Couleur de la periode en cours     | 
|              |    hour     | Type de la periode en cours     | 
|              |    tday     | Couleur du jour   | 
|              |    tmrw     | Couleur du lendemain     | 
|              |    CONSO    | Compteur global (Wh) de l'ensemble des périodes de consommation    | 
|              |  *PERIODE*  | Compteur total (Wh) de la période de consommation *PERIODE*      | 
|              |    PROD     | Compteur global (Wh) de la production    | 
| **CAL**      |    lv       | Niveau de la période actuelle (0 inconnu, 1 bleu, 2 blanc, 3 rouge)     | 
|              |    hp       | Type de la période courante (0:heure creuse, 1 heure pleine) | 
|              |  **tday**   | Section avec le niveau et le type de chaque heure du jour | 
|              |  **tmrw**   | Section avec le niveau et le type de chaque heure du lendemain | 
| **RELAY**    |    R1       | Etat du relai virtual n°1 (0:ouvert, 1:fermé)   | 
|              |    ...      |                                                 | 
|              |    R8       | Etat du relai virtual n°8 (0:ouvert, 1:fermé)   | 
|              |    P1       | Etat de la période n°1 (0:inactive, 1:active)   | 
|              |    L1       | Libellé de la période n°1   | 
|              |    ...      |                                                 | 
|              |    P9       | Etat de la période n°9 (0:inactive, 1:active)   | 
|              |    L9       | Libellé de la période n°9   | 
| **ALERT**    |    Load     | Indicateur de surconsommation (0:pas de pb, 1:sur-consommation)     | 
|              |    Volt     | Indicateur de surtension (0:pas de pb, 1:au moins 1 phase est en surtension)    | 
|              |   Preavis   | Niveau du prochain préavis (utilisé en Tempo & EJP)     | 
|              |    Label    | Libellé du prochain préavis    | 

## Commands

Ce firmware propose un certain nombre de commandes **EnergyConfig** spécifiques disponibles en mode console :

      historique   set historique mode at 1200 bauds (needs restart)
      standard     set Standard mode at 9600 bauds (needs restart)
      stats        display reception statistics
      reset        reset contract data
      error=0      display error counters on home page
      percent=100  maximum acceptable contract (%)
      
      policy=1     message policy : 0=Telemetrie seulement, 1=± 5% Evolution puissance, 2=Tous les messages TIC
      meter=1      publish METER & PROD data
      contract=1   publish CONTRACT data
      calendar=1   publish CAL data
      relay=1      publish RELAY data
      
      maxv=235     graph max voltage (V)
      maxva=3000  graph max power (VA or W)
      nbday=8      number of daily logs
      nbweek=4     number of weekly logs
      maxhour=2    graph max total per hour (Wh)
      maxday=10    graph max total per day (Wh)
      maxmonth=100 graph max total per month (Wh)

Vous pouvez passer plusieurs commandes en même temps :

      EnergyConfig percent=110 nbday=8 nbweek=12

### Domoticz

La configuration des messages émis pour Domoticz peut être réalisée en mode console :

    domo
    HLP: commands for Teleinfo Domoticz integration
      domo_set <0/1>     = activation de l'integration [0]
      domo_va <0/1>      = puissances en VA plutot que W [0]
      domo_key <num,idx> = set key num to index idx
        <0,index>  : Totaux conso. Bleu
        <1,index>  : Totaux conso. Blanc
        <2,index>  : Totaux conso. Rouge
        <3,index>  : Total global conso.
        <4,index>  : Courant (3 phases)
        <5,index>  : Tension (phase 1)
        <6,index>  : Tension (phase 2)
        <7,index>  : Tension (phase 3)
        <8,index>  : Total Production
        <9,index>  : Heure Pleine /Creuse
        <10,index> : Couleur du Jour
        <11,index> : Couleur du Lendemain

### Home Assistant

L'intégration Home Assistant peut être activée en mode console : 

    hass 1

### Homie

L'intégration Homie peut être activée en mode console : 

    homie 1
 
### ThingsBoard

L'intégration Thingsboard peut être activée en mode console : 

    thingsboard 1

## Partition LittleFS

Certaines variantes de ce firmware (ESP avec au moins 4Mo de ROM) utilisent une partition **LittleFS** pour stocker les données historisées qui servent à générer les graphs de suivi. Lorsque vous souhaitez utiliser cette fonctionnalité, vérifier que vous flashez bien l'ESP en mode série la première fois afin de modifier le partitionnement.

Pour les versions **LittleFS**, les graphs affichent en complément la tension et la puissances crête.

Avec une partition LittleFS, 4 familles de fichiers sont générées :
  * **teleinfo-day-nn.csv** : valeurs quotidiennes enregistrées toutes les 5 mn (**00** aujourd'hui, **01** hier, ...)
  * **teleinfo-week-nn.csv** : valeurs hebdomadaires enregistrées toutes les 30 mn (**00** semaine courante, **01** semaine précédente, ...)
  * **teleinfo-year-yyyy.csv** : Compteurs de consommation annuels
  * **production-year-yyyy.csv** : Compteur de production annuel

Chacun de ces fichiers inclue un entête.

## Calendriers RTE : Tempo, Pointe & Ecowatt

Ce firmware permet également de s'abonner aux calendriers publiés par [**RTE**](https://data.rte-france.com/) :
  * **Tempo**
  * **Pointe**
  * **Ecowatt**

Cette fonctionnalité n'est disponible que sur les **ESP32**. Vous devez tout d'abord créer un compte sur le site **RTE** [https://data.rte-france.com/] Ensuite vous devez activer l'un ou l'autre des API suivantes :
  * **Tempo**
  * **Demand Response Signal**
  * **Ecowatt**

![RTE applications](./screen/rte-application-list.png) 

Ces calendriers sont utilisés pour générer le calendrier de la journée et du lendemain.

![RTE applications](./screen/tasmota-rte-display.png)

Ils sont utilisés suivant les règles suivantes :
  * si calendrier **Tempo** activé, publication de ses données
  * sinon, si calendrier **Pointe** activé, publication de ses données
  * sinon, publication des données de calendrier fournies par le compteur (**PJOURN+1**)

En complément, si le calendrier **Ecowatt** est activé, les alertes sont publiées suivant les règles suivantes :
  * alerte **orange**  = jour **blanc**
  * alerte **rouge**  = jour **rouge**

La configuration est stockée dans le fichier **rte.cfg**.

Voici la liste de toutes les commandes RTE disponibles en mode console :

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

Une fois votre compte créé chez RTE et les API activées, vous devez déclarer votre **private Base64 key** en mode console :

    rte_key your_rte_key_in_base64

Il ne vous reste plus qu'à activer les modules correspondant aux API RTE : 

    tempo_enable 1
    pointe_enable 1
    eco_enable 1

Au prochain redémarrage, vous verrez dans les logs que votre ESP32 récupère un token puis les données des API activées.

    RTE: Token - abcdefghiL23OeISCK50tsGKzYD60hUt2TeESE1kBEe38x0MH0apF0y valid for 7200 seconds
    RTE: Ecowatt - Success 200
    RTE: Tempo - Update done (2/1/1)

Les données RTE sont publiées sous des sections spécifiques sous **tele/SENSOR** :

    your-device/tele/SENSOR = {"Time":"2023-12-20T07:23:39",TEMPO":{"lv":1,"hp":0,"label":"blue","icon":"🟦","yesterday":1,"today":1,"tomorrow":1}}

    your-device/tele/SENSOR = {"Time":"2023-12-20T07:36:02","POINTE":{"lv":1,"label":"blue","icon":"🟦","today":1,"tomorrow":1}}

    your-device/tele/SENSOR = {"Time":"2022-10-10T23:51:09","ECOWATT":{"dval":2,"hour":14,"now":1,"next":2,
      "day0":{"jour":"2022-10-06","dval":1,"0":1,"1":1,"2":1,"3":1,"4":1,"5":1,"6":1,...,"23":1},
      "day1":{"jour":"2022-10-07","dval":2,"0":1,"1":1,"2":2,"3":1,"4":1,"5":1,"6":1,...,"23":1},
      "day2":{"jour":"2022-10-08","dval":3,"0":1,"1":1,"2":1,"3":1,"4":1,"5":3,"6":1,...,"23":1},
      "day3":{"jour":"2022-10-09","dval":2,"0":1,"1":1,"2":1,"3":2,"4":1,"5":1,"6":1,...,"23":1}}}

## Serveur TCP

Un serveur **TCP** est intégré à cette version de firmware.

Il permet de récupérer très simplement le flux d'information publié par le compteur. Il est à noter que ce flux envoie toutes les données recues, sans aucune correction d'erreur.

La commande **tcp_help** explique toutes les possibilités :
  * **tcp_status** : status of TCP server (running port or 0 if not running)
  * **tcp_start** [port] : start TCP server on specified port
  * **tcp_stop** : stop TCP server

Une fois le serveur activé, la réception du flux sur un PC sous Linux est un jeu d'enfant (ici sur le port 888) :

    # nc 192.168.1.10 8888
        SMAXSN-1	E220422144756	05210	W
        CCASN	E220423110000	01468	:
        CCASN-1	E220423100000	01444	Q
        UMOY1	E220423114000	235	(
        STGE	003A0001	:
        MSG1	PAS DE          MESSAGE         	<

Le serveur étant minimaliste, il ne permet qu'une seule connexion simultanée. Toute nouvelle connexion tuera la connexion précédente.

## Serveur FTP

Si vous utilisez une version de firmware avec partition LittleFS, vous avez à disposition un serveur **FTP** embarqué afin de récupérer les fichiers de manière automatisée.

Les commandes sont les suivantes :
  * **ufsftp 2** : démarrage du serveur FTP sur le port 21
  * **ufsftp 0** : arrêt du serveur FTP

Coté client FTP, vous devez utiliser les login / mot de passe suivants : **teleinfo** / **teleinfo**

Ce serveur FTP ne peut accepter qu'une seule connexion simultanée. Vous devez donc configurer votre client FTP avec une limite de type : **simultaneous connexions = 1**. Sinon, la connexion sera en erreur.

## Carte Winky

La carte [Winky](https://gricad-gitlab.univ-grenoble-alpes.fr/ferrarij/winky) développée par l'université de Grenoble avec Charles Hallard fonctionne de manière un peu particulière car elle peut être auto-alimentée par le compteur Linky à l'aide d'une super-capacité.

Elle peut être alimentée en continu par le port USB ou directement par le compteur Linky. Dans ce cas, elle se réveille régulièrement pour lire les données du compteur, les envoyer via MQTT et se rendort ensuite en mode **deep sleep** le temps de recharger la super capacité qui sera utilisée lors du prochain réveil.

Typiquement, après configuration en alimentation USB, le Winky doit être programmé en mode console afin d'activer le mode **deep sleep**. Ceci se fait à travers la console tasmota :

    deepsleeptime xxx

où **xxx** représente le nombre de secondes entre 2 réveils. Un minimum de 60 (secondes) est préconisé et il faut éviter 300 qui définit un mode de fonctionnement spécifique de Tasmota. Si la super capacité n'est pas assez rechargée lors du prochain réveil, l'ESP se rendort pour un cycle supplémentaire.

## Compilation

Si vous voulez compiler ce firmware vous-même, vous devez :
1. installer les sources **tasmota** officielles (utilisez la même version que celle déclarée en tête de cette page
2. déposez ou remplacez les fichiers de ce **repository**
3. déposez ou remplacez les fichiers du repository **tasmota/common**

Voici la liste exhaustive des fichiers concernés :

| File    |  Comment  |
| --- | --- |
| **platformio_override.ini** |    |
| partition/**esp32_partition_xxx.csv** | Specific ESP32 partitionning files   |
| boards/**espxxx.json** | ESP8266 and ESP32 boards description  |
| tasmota/**user_config_override.h**  |    |
| tasmota/include/**tasmota_type.h** | Redefinition of teleinfo structure |
| tasmota/tasmota_nrg_energy/**xnrg_15_teleinfo.ino** | Teleinfo energy driver  |
| tasmota/tasmota_drv_driver/**xdrv_01_9_webserver.ino** | Add compilation target in footer  |
| tasmota/tasmota_drv_driver/**xdrv_94_ip_option.ino** | Fixed IP address and misc options Web configuration |
| tasmota/tasmota_drv_driver/**xdrv_97_tcp_server.ino** | Embedded TCP stream server |
| tasmota/tasmota_drv_energy/**xdrv_115_teleinfo.ino** | Teleinfo driver  |
| tasmota/tasmota_drv_energy/**xdrv_116_integration_domoticz.ino** | Teleinfo domoticz integration  |
| tasmota/tasmota_drv_energy/**xdrv_117_integration_hass.ino** | Teleinfo home assistant integration  |
| tasmota/tasmota_drv_energy/**xdrv_118_integration_homie.ino** | Teleinfo homie protocol integration  |
| tasmota/tasmota_drv_energy/**xdrv_119_integration_thingsboard.ino** | Teleinfo Thingsboard protocol integration  |
| tasmota/tasmota_drv_energy/**xdrv_120_linky_relay.ino** | Management of relays according to periods and virtual relays  |
| tasmota/tasmota_sns_sensor/**xsns_99_timezone.ino** | Timezone Web configuration |
| tasmota/tasmota_sns_sensor/**xsns_119_rte_server.ino** | RTE Tempo, Pointe and Ecowatt data collection |
| tasmota/tasmota_sns_sensor/**xsns_124_teleinfo_histo.ino** | Teleinfo sensor to handle historisation |
| tasmota/tasmota_sns_sensor/**xsns_125_teleinfo_curve.ino** | Teleinfo sensor to handle curves |
| tasmota/tasmota_sns_sensor/**xsns_126_teleinfo_winky.ino** | Handling of Winky and deep sleep mode |

Si tout se passe bien, vous devriez pouvoir compiler votre propre build.

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

![Monophasé](./screen/tasmota-teleinfo-main.png)  ![Triphasé](./screen/tasmota-teleinfo-main-triphase.png)

If you want to remove default Tasmota energy display, you just need to run this command in console :

    websensor3 0

## Configuration

![Config 1](./screen/tasmota-teleinfo-config-1.png)  ![Config 2](./screen/tasmota-teleinfo-config-2.png)  ![Config 3](./screen/tasmota-teleinfo-config-3.png)

### Realtime messages ###

![Grah message](./screen/tasmota-teleinfo-message.png) 

### Graph for Power, Voltage and Cos φ ###

![Grah monophase power](./screen/tasmota-teleinfo-graph-daily.png)  ![Grah monophase power](./screen/tasmota-teleinfo-graph-weekly.png)  ![Grah monophase voltage](./screen/tasmota-teleinfo-graph-voltage.png)  ![Grah monophase Cos phi](./screen/tasmota-teleinfo-graph-cosphi.png) 
 
### Totals Counters (kWh) ###

![Yearly total](./screen/tasmota-teleinfo-total-year.png)  ![Monthly total](./screen/tasmota-teleinfo-total-month.png)  ![Daily total](./screen/tasmota-teleinfo-total-day.png)
