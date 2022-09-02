Gazpar Tasmota firmware
=============

Presentation
------------

This evolution of **Tasmota 12** firmware has been enhanced to handle France gaz meters known as **Gazpar** using a druy contact counter.

It is compatible with **ESP8266** chipsets.
 
Some of these firmware versions are using a LittleFS partition to store graph data. Il allows to keep historical data over reboots.
To take advantage of this feature, make sure to follow partitioning procedure given in the **readme** of the **binary** folder.

This firmware provides some extra Web page on the device :
  * **/graph** : live, daily and weekly graphs

It also provides :
  * a FTP server to easily retrieve historical data files

Pre-compiled versions are available in the [**binary**](https://github.com/NicolasBernaerts/tasmota/tree/master/gazpar/binary) folder.

Compilation
-----------

If you want to compile this firmware version, you just need to :
1. install official tasmota sources
2. place or replace files from this repository
3. place specific files from **tasmota/common** repository
4. install **FTPClientServer** library

Here is where you should place different files from this repository and from **tasmota/common** :
* **platformio_override.ini**
* tasmota/**user_config_override.h**
* tasmota/boards/**esp8266_16M14M.json**
* tasmota/tasmota_drv_driver/**xdrv_01_9_webserver.ino**
* tasmota/tasmota_drv_driver/**xdrv_50_filesystem_cfg_csv.ino**
* tasmota/tasmota_drv_driver/**xdrv_94_ip_address.ino**
* tasmota/tasmota_drv_driver/**xdrv_96_ftp_server.ino**
* tasmota/tasmota_sns_sensor/**xsns_99_gazpar.ino**
* tasmota/tasmota_sns_sensor/**xsns_120_timezone.ino**
* lib/default/**FTPClientServer** (extract content of **FTPClientServer.zip**) 

If everything goes fine, you should be able to compile your own build.

