Tasmota modified for ICSE014A, ICSE013A and ICSE012A
=============

This firmware is a modified version of **Tasmota 9.1.0** wich handle **ICSE01xA** based serial relay boards :
  * ICSE012A : 4 relays
  * ICSE013A : 2 relays
  * ICSE014A : 8 relays

**ICSE01xA** chipstet protocol is very badly documented. But some very usefull informations are available on this [stackoverflow page](https://stackoverflow.com/questions/26913755/need-help-understading-sending-bytes-to-serial-port).

These boards are quite cheap, but not hyper reactive. So you should expect around 8 seconds to fully initialise the board.

This firmware detects the board connected to the serial port and setup the number of relays accordingly.

Firmware is configured to use the Normally Open (NO) contact of the relays.

It has been tested succesfully with few days stability on a **HW-149** board, based on ICSE014A.

Here is an example of working interface board for ESP01 :

![ESP01 interface](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/icse/tasmota-icse-diagram.png)

On this diagram, power supply comes fully from the micro-USB port of the ICSE01xA board. \
You just need a 5V / 1A USB charger for the complete setup.

Here is an example of PCB board that just fits and works :

![ESP01 board](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/icse/tasmota-icse-pcb.png)

As ICSE01xA board works on 5v, if you plan to use it with a Sonoff Basic (or similar), you'll need an interface board to adjust Rx and Tx from 5V (ICSE) to 3.3V (Sonoff).
