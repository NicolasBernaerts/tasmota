# Tasmota Firmware Extensions

You'll find here some ESP8266 and ESP32 Tasmota firmware extensions :
  * **teleinfo** : manage **Teleinfo** protocol (Linky) & RTE API for **Tempo** and **Ecowatt** protocol
  * **gazpar** : manage french gaz smart meters (Gazpar)
  * **intercom** : manage building gate opening upon flat intercom rings
  * **motion** : handle motion detectors and push buttons in parallel to light circuit
  * **offload** : pilot device with power offloading according to power meter publishing thru MQTT
  * **pilotwire** : manage french pilotwire electrical heaters with **offload** functionnality
  * **serialrelay** : handle ICSE01xA and LC Tech serial relay boards
  * **sensor-vmc** : handle local and MQTT sensors & Motor Controled Ventilator according to target humidity level

# ESP8266 & ESP32 tools

In the **tools** folder you get 2 simple tools :
  * **tasmota-discover** to list all devices (tasmota and others) available on the same LAN.
  * **tasmota-flash** to erase and/or flash ESP8266 and ESP32 devices.


