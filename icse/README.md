Tasmota modified for ICSE014A, ICSE013A and ICSE012A
=============

This firmware is a modified version of Tasmota wich handle **ICSE01xA** based serial relay boards :
  * ICSE012A : 4 rpelays
  * ICSE013A : 2 relays
  * ICSE014A : 8 relays

ICSE chipstet protocol is not really documented. Some very usefull info are available on a [stackoverflow page](https://stackoverflow.com/questions/26913755/need-help-understading-sending-bytes-to-serial-port).

These boards are quite cheap, but not hyper reactive. So you should expect around 8 seconds to fully initialise the board.

This firmware detects the board connected to the serial port and setup the number of relays accordingly.

On the relays, you should use the Normally open (NO) contact.

It has been tested on **HW-149** board based on ICSE014A.

Here is an example of working interface board for ESP01 :

![ESP01 interface](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/icse/tasmota-icse-diagram.png)
