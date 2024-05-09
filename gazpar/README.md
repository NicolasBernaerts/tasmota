Gazpar Tasmota firmware
=============

Presentation
------------

This evolution of **Tasmota 13.4** firmware has been enhanced to handle France gaz meters known as **Gazpar** using a dry contact counter.

It is compatible with **ESP8266** and **ESP32** chipsets.
 
This firmware uses LittleFS partition to store graph data. Il allows to keep historical data over reboots.
To take advantage of this feature, make sure to follow partitioning procedure given in the **readme** of the **binary** folder.

This firmware provides some extra Web page on the device :
  * **/graph** : yearly, monthly and daily graphs

Pre-compiled versions are available in the [**binary**](https://github.com/NicolasBernaerts/tasmota/tree/master/gazpar/binary) folder.

Configuration
-------------

Gazpar impulse should be declared as **Counter 1**

In LittleFS partition, config is stored in /gazpar.cfg

Compilation
-----------

If you want to compile this firmware version, you just need to :
1. install official tasmota sources
2. place or replace files from this repository
3. place specific files from **tasmota/common** repository
4. install **FTPClientServer** library

Here is where you should place different files from this repository and from **tasmota/common** :
* **platformio_override.ini**
* boards/**esp8266_16M14M.json**
* tasmota/**user_config_override.h**
* tasmota/tasmota_drv_driver/**xdrv_01_9_webserver.ino**
* tasmota/tasmota_drv_driver/**xdrv_50_filesystem_cfg_csv.ino**
* tasmota/tasmota_drv_driver/**xdrv_94_ip_address.ino**
* tasmota/tasmota_drv_driver/**xdrv_96_ftp_server.ino**
* tasmota/tasmota_sns_sensor/**xsns_99_gazpar.ino**
* tasmota/tasmota_sns_sensor/**xsns_120_timezone.ino**
* lib/default/**FTPClientServer** (extract content of **FTPClientServer.zip**) 

If everything goes fine, you should be able to compile your own build.


 ![Grah yearly](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/gazpar/screen/gazpar-graph-year.png)
 
 ![Grah monthly](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/gazpar/screen/gazpar-graph-month.png)
