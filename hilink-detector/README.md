Hilink presence detector drivers for Tasmota
-------

You'll get here a generic Tasmota driver to handle different series of Hilink presence detectors.

Devices currently handled are :
  * HLK-LD1115
  * HLK-LD1125
  * HLK-LD2401
  * HLK-LD1402
  * HLK-LD2410b
  * HLK-LD2410s
  * HLK-LD2420
  * HLK-LD2450

Each devices has its own specificities, but the driver provides some generic commands to select and manage the connected device.

+++ Generic commands

You can get some help on available commands thru **hlk** command :
''
    hlk
    HLP: Hi-Link detector commands :
         hlk_device <dev>  = set device type [LD2401]
           none LD1115 LD1125 LD2401 LD2402 LD2410 LD2410s LD2420 LD2450
         hlk_log <val>     = log policy [off]
20:38:35.874     off recv sent all
20:38:35.874   hlk_param         = read sensor parameters
20:38:35.874   hlk_delay delay   = detection timeout [5 s]
20:38:35.874   hlk_min dist      = minimum detection distance [0 cm]
20:38:35.874   hlk_max pres,move = maximum detection distance
20:38:35.875     pres : 1 .. 600 [600 cm]
20:38:35.875     move : 1 .. 600 [600 cm]
20:38:35.875 LD2401 specific commands :
20:38:35.875   hlk_version   = get sensor version
20:38:35.875   hlk_auto      = auto calibrate sensitivity
20:38:35.875   hlk_restart   = restart sensor
20:38:35.876   hlk_reset     = reset sensor
20:38:35.876   hlk_mac       = read MAC address
20:38:35.876   hlk_mode 0/1  = set energy mode
20:38:35.876   hlk_delay val = detection delay (sec.)
20:38:35.876   hlk_bluetooth 0/1   = set bluetooth
20:38:35.876   hlk_password pass   = set bluetooth password
20:38:35.876   hlk_gate gate pres,motion = gate sensitivity (%)
20:38:35.877     gate   : 1 .. 8
20:38:35.877     pres   : 0 .. 100
20:38:35.877     motion : 0 .. 100
20:38:35.877 RSL: RESULT = {"hlk":"Done"
''
