Tasmota modified for ICSE014A, ICSE013A and ICSE012A
=============

This firmware is a modified version of Tasmota wich handle **ICSE01xA** based serial relay boards :
  * ICSE012A : 4 relays
  * ICSE013A : 2 relays
  * ICSE014A : 8 relays

These boards are quite cheap, but not hyper reactive. So you should expect around 8 seconds to fully initialise the board.

This firmware detects the board connected to the serial port and setup the number of relays accordingly.

Relays connection should be the Normally open (NO) contact.

It has been tested on **HW-149** board based on ICSE014A.

Here is an example of working interface board for ESP01 :

![ESP01 interface](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/icse/tasmota-icse-diagram.png)
