Light & Motion Tasmota firmware
=============

Presentation
------------

This evolution of **Tasmota 12** provide light management thru a presence detector and a push button.

Pre-compiled versions are available in the [**binary**](https://github.com/NicolasBernaerts/tasmota/tree/master/lightmotion/binary) folder.

Configuration
-------------

Input devices should be configured as followed :

  - Counter1 = Light On sensor
  - Counter2 = Movement detection sensor
  - Counter3 = Button to force On/Off

Settings are stored using weighting scale parameters :

   - Settings.energy_power_calibration = Light timeout (s)

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
* tasmota/tasmota_drv_driver/**xdrv_98_light_motion.ino**
* tasmota/tasmota_sns_sensor/**xsns_120_timezone.ino**

If everything goes fine, you should be able to compile your own build.
