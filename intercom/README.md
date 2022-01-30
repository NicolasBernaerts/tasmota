Tasmota firmware to control your flat intercom
=============

This evolution of Tasmota firmware provide a simple way to control your building entrance door directly from your smartphone.

It connects directly to your 2 wires private intercom and is totally non intrusive for the common infrastructure.

It needs a small electronic interface board which is described in the **diagram.png**

![Intercom adapter](https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/intercom/diagram.png)

If you want to compile it, you need to add following files from my **tasmota/common** repository :
  * xdrv_40_telegram_extension.ino
  * xdrv_50_filesystem_cfg_csv.ino
  * xdrv_95_timezone.ino
  * xdrv_94_ip_address.ino

