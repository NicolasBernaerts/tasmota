Tasmota Extensions
==================

Some Tasmota firmware extensions :
  * **intercom** : manage building gate opening upon flat intercom rings
  * **motion** : handle motion detectors and push buttons in parallel to light circuit
  * **offload** : pilot device with power offloading according to power meter publishing thru MQTT
  * **pilotwire** : manage french pilotwire electrical heaters with **offload** functionnality
  * **serialrelay** : handle ICSE01xA and LC Tech serial relay boards
  * **teleinfo** : manage french Linky meter
  * **vmc** : handle motor controled ventilator according to target humidity level

**tasmota-discover** is a simple bash script to list all devices (tasmota and others) available on the same LAN.

**tasmota-flash** is a simple bash script to erase and/or flash ESP8266 and ESP32 decices.
To be able to use it, you first need to :
  * download and extract esptool.zip to any directory
  * update tasmota-flash **ROOT_TOOLS** variable with this directory path

These projects are explained in detail under http://www.bernaerts-nicolas.fr/iot/
