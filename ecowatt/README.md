Ecowatt Tasmota firmware
=============

Presentation
------------

This evolution of **Tasmota 12** firmware allows to colect France RTE **Ecowatt** signals and to publish them thru MQTT.

It is compatible with **ESP32** only as SSL connexions are using too much memory for ESP8266.

This firmware will connect every hour to RTE API and collect Ecowatt signals.

Result will be published as an MQTT message :

    {"Time":"xxxxxxxx","Ecowatt":{"slot":16,"now":2,"next":1,"day0":{"0":2,"1":1,...,"23":1},"day1":{"0":1,"1":1,...,"23":2},...}}
 
Pre-compiled versions are available in the [**binary**](https://github.com/NicolasBernaerts/tasmota/tree/master/ecowatt/binary) folder.

Configuration
-------------

This **Ecowatt** firmware needs :
  * a Base64 private key provided by RTE when you create you account on https://data.rte-france.com/
  * the PEM root certificate authority from https://data.rte-france.com/

You can declare your private Base64 key thru this console command :

    # eco_key your-base64-private-key

To declare the root CA of https://data.rte-france.com/, you can collect it from the site itself :
  * go on the home page
  * click on the lock just before the URL
  * select the certificate in the menu and select **more information**
  * on the page, select **display the certificate**
  * on the new page, select **Global Sign** tab
  * click to download **PEM(cert)**
You can also download it from this repository  :-)
This certificate should be named **ecowatt.pem** and uploaded to the root of the LittleFS partition.

Compilation
-----------

If you want to compile this firmware version, you just need to :
1. install official tasmota sources
2. place or replace files from this repository
3. place specific files from **tasmota/common** repository

Here is where you should place different files from this repository and from **tasmota/common** :
* **platformio_override.ini**
* tasmota/**user_config_override.h**
* tasmota/tasmota_drv_driver/**xdrv_01_9_webserver.ino**
* tasmota/tasmota_drv_driver/**xdrv_50_filesystem_cfg_csv.ino**
* tasmota/tasmota_drv_driver/**xdrv_94_ip_address.ino**
* tasmota/tasmota_sns_sensor/**xsns_120_timezone.ino**
* tasmota/tasmota_sns_sensor/**xsns_121_ecowatt-server.ino**

If everything goes fine, you should be able to compile your own build.
