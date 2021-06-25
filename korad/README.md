KORAD power supply Tasmota firmware
---------------

This firmware version has been specifically developped to manage Korad KA3005P laboratory power supply.

Korad serial protocol is well documented at https://sigrok.org/wiki/Korad_KAxxxxP_series

To use it, your need to add one specific ESP8266 extension card connected to the internal serial port of the power supply.

Here is the adapter board diagram and board design based on a **Wemos mini D1 Pro**.

![Interface diagram](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/korad/screen/korad-interface-diagram.png)  

![Interface board](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/korad/screen/korad-interface-board.png)  

Once you've flashed the specific Tasmota firmware, you should get the following control panel accessible thru Tasmota.

![Control page](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/korad/screen/korad-tasmota-control.png)  
