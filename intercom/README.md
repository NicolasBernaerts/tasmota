# Intercom Tasmota firmware #

## Presentation ##

This evolution of **Tasmota 12** provide a simple way to control your building entrance door directly from your smartphone.

It connects directly to your 2 wires private intercom and is totally non intrusive for the common infrastructure.

It needs a small electronic interface board which is described in the **diagram.png**

![Intercom adapter](diagram.png)

Pre-compiled versions are available in the [**binary**](https://github.com/NicolasBernaerts/tasmota/tree/master/intercom/binary) folder.

## Configuration ##

Input devices should be configured as followed :
  - Counter 1 : connected to the intercom button circuit
  - Relay  1  : connected to external relay in charge of opening the door

You should setup counter debounce as follow to be on the safe side :

    # CounterDebounce 1000
    # CounterDebounceHigh 50
    # CounterDebounceLow 50

You may have to adjust settings according to your ring

To enable Telegram notification in case of door opening, run these commands once in console :

    # setoption132 1
    # tmtoken yourtelegramtoken
    # tmchatid yourtelegramchatid
    # tmstate 1

When door opens, you'll receive a Telegram notification "DeviceName : Gate Opened"

Settings are stored using unused parameters :

| Settings   | Parameter |
| ------     | -----     |
| free_ea6[0] | Global activation timeout (in 10mn slots) |
| free_ea6[1] | Number of rings to open                   |
| free_ea6[2] | Door opening duration (seconds)           |
| domoticz_relay_idx[] | Last 4 intercom activation |
| domoticz_key_idx[]   | Last 4 gate opening |


Once installed, you get a log of the last 8 activities (intercom activation and gate opening).

## Commands ##

    CMD: int_help
    HLP: Intercom commands
    - int_reset = reset event log
    - int_timeout <120> = enabling timeout (mn) in 10mn slots
    - int_ring <3> = number of rings
    - int_latch <5> = gate open duration (sec)
    - int_action <val> = action (0:disable, 1:enable, 2:open)
    
 ## History ##
 
 You can get history log of last 4 events (activation and opening)
  
![Intercom activity](intercom-activity.png)

## Compilation ##

If you want to compile this firmware version, you just need to :
1. install official tasmota sources
2. place or replace files from this repository
3. place specific files from **tasmota/common** repository

Here is where you should place different files from this repository and from **tasmota/common** :
* **platformio_override.ini**
* tasmota/**user_config_override.h**
* tasmota/tasmota_drv_driver/**xdrv_01_9_webserver.ino**
* tasmota/tasmota_drv_driver/**xdrv_40_telegram_extension.ino**
* tasmota/tasmota_drv_driver/**xdrv_94_ip_address.ino**
* tasmota/tasmota_drv_driver/**xdrv_98_intercom.ino**
* tasmota/tasmota_sns_sensor/**xsns_120_timezone.ino**

If everything goes fine, you should be able to compile your own build.

