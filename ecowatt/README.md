# Ecowatt server Tasmota firmware #

## Presentation ##

This evolution of **Tasmota 12** firmware allows to collect [**France RTE Ecowatt**](https://data.rte-france.com/catalog/-/api/doc/user-guide/Ecowatt/4.0) signals and to publish them thru MQTT.

It is compatible with **ESP32** and **ESP32S2** only, as SSL connexions are using too much memory for ESP8266.

This firmware will connect every hour to RTE API and collect Ecowatt signals.

Result will be published as an MQTT message :

    {"Time":"2022-10-10T23:51:09","Ecowatt":{"dval":2,"hour":14,"now":1,"next":2,
      "day0":{"jour":"2022-10-06","dval":1,"0":1,"1":1,"2":1,"3":1,"4":1,"5":1,"6":1,...,"23":1},
      "day1":{"jour":"2022-10-07","dval":2,"0":1,"1":1,"2":2,"3":1,"4":1,"5":1,"6":1,...,"23":1},
      "day2":{"jour":"2022-10-08","dval":3,"0":1,"1":1,"2":1,"3":1,"4":1,"5":3,"6":1,...,"23":1},
      "day3":{"jour":"2022-10-09","dval":2,"0":1,"1":1,"2":1,"3":2,"4":1,"5":1,"6":1,...,"23":1}}}
 
Pre-compiled versions are available in the [**binary**](https://github.com/NicolasBernaerts/tasmota/tree/master/ecowatt/binary) folder.

## Configuration ##

This **Ecowatt** firmware needs :
  * a Base64 private key provided by RTE when you create you account on https://data.rte-france.com/
  * the PEM root certificate authority from https://data.rte-france.com/
  * a topic where to publish Ecowatt data

### Private Key ###

You can declare your private Base64 key thru this console command :

    # eco_key your-base64-private-key

### Root CA ###

To declare the root CA of https://data.rte-france.com/, you can collect it from the site itself :
  * go on the home page
  * click on the lock just before the URL
  * select the certificate in the menu and select **more information**
  * on the page, select **display the certificate**
  * on the new page, select **Global Sign** tab
  * click to download **PEM(cert)**
  
This certificate should be named ***ecowatt.pem*** and uploaded to the root of the LittleFS partition.

To ease the process, the certificate has been uploaded to this repository. 

### MQTT Topic ### 

To declare the MQTT topic where to publish Ecowatt data, run following console command :

    # eco_topic your/topic/for/ecowatt

## Restart ##

Once you've setup these 3 data, restart your Tasmota module.

Your Ecowatt MQTT server should be up and running.

## Compilation ##

If you want to compile this firmware version, you just need to :
1. install official tasmota sources
2. place or replace files from this repository
3. place specific files from **tasmota/common** repository

Here is where you should place different files from this repository and from [**tasmota/common**](https://github.com/NicolasBernaerts/tasmota/tree/master/common) :
* **platformio_override.ini**
* tasmota/**user_config_override.h**
* tasmota/tasmota_drv_driver/**xdrv_01_9_webserver.ino**
* tasmota/tasmota_drv_driver/**xdrv_50_filesystem_cfg_csv.ino**
* tasmota/tasmota_drv_driver/**xdrv_94_ip_address.ino**
* tasmota/tasmota_sns_sensor/**xsns_120_timezone.ino**
* tasmota/tasmota_sns_sensor/**xsns_121_ecowatt-server.ino**
* lib/default/**ArduinoJson** (extract content of **ArduinoJson.zip**)

You can also retrieve latest ArduinoJson library thru git :

    # cd your-project/lib/default
    # git clone https://github.com/bblanchon/ArduinoJson.git

If everything goes fine, you should be able to compile your own build.
