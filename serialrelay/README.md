Tasmota modified for Serial Relay Boards
=============

This firmware is a modified version of **Tasmota 9.3.1** wich handle 2 majors families of serial relay boards :
  * **ICSE01xA boards**

![ICSE013A](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/serialrelay/screen/serialrelay-icse013a.png) ![ICSE012A](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/serialrelay/screen/serialrelay-icse012a.png) ![ICSE014A](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/serialrelay/screen/serialrelay-icse014a.png)

  * **LC Technology boards**

![LC Tech x1](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/serialrelay/screen/serialrelay-lctech-x1.png) ![LC Tech x2](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/serialrelay/screen/serialrelay-lctech-x2.png) ![LC Tech x4](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/serialrelay/screen/serialrelay-lctech-x4.png) 

ICSE01xA boards come with a TTL serial interface. You need to provide a controler.

LC Technology boards come with an ESP-01, but features provided are far from what tasmota provides.

So, this tasmota modified version fully manage these boards without any specific configuration needed for GPIO or rules. Relays are managed like any relay directly connected to the ESP controler. Only thing you have to do is to select the board type you've connected and after a reboot, everything should be working.

Firmware is configured to use the Normally Open (NO) input. When relay is ON, contact is closed.

These serial relay boards are quite cheap, but not hyper reactive. So, you should expect few seconds to fully initialise the board.

It has been tested succesfully on a **HW-034** (ICSE012A), **HW-149** (ICSE014A), **LC Tech x2** and **LC Tech x4** boards.

**ICSE01xA specificities**

As ICSE01xA board works on 5V input, if you plan to use it with a Sonoff Basic or any ESP8266 board, you'll need an interface board to adjust **Rx** and **Tx** from 5V to 3.3V levels. Here is an example of working interface board using an **ESP-01**. Power supply is directly provided by the ICSE01xA board thru the micro-USB port. You just need a 5V / 1A USB charger.

![ESP01 interface](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/serialrelay/screen/tasmota-icse-diagram.png)

Here is an example of PCB board that implements this diagram.

![ESP01 board](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/serialrelay/screen/tasmota-icse-pcb.png)

