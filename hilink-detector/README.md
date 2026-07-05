Hilink presence detector drivers for Tasmota
-------

You'll get here a generic Tasmota driver to handle different series of Hilink presence detectors.

Devices currently handled are :
  * HLK-LD1115
  * HLK-LD1125
  * HLK-LD2401
  * HLK-LD2402
  * HLK-LD2410b
  * HLK-LD2410s
  * HLK-LD2420
  * HLK-LD2450

Each devices has its own specificities, but the driver provides some generic commands to select and manage the connected device.

Any Hilink detector should be connected thru Rx/Tx and declared as **Tx 2410** and **Rx 2410**

You can then select your specific decice with **hlk_device** command.

After reboot, your detector should be recognised and operationnal.

You should see a graphical target range display on the main page with your device type.

# Generic commands

Run **hlk** command to get help on available device commands.

```
  hlk_device <dev>  = set device type
    none LD1115 LD1125 LD2401 LD2402 LD2410 LD2410s LD2420 LD2450
  hlk_log <val>     = log policy [off]
    off recv sent all
  hlk_param         = read sensor parameters
  hlk_delay delay   = detection timeout [5 s]
  hlk_min dist      = minimum detection distance [0 cm]
  hlk_max pres,move = maximum detection distance
    pres : 1 .. 600 [600 cm]
    move : 1 .. 600 [600 cm]
```
**hlk-device** allows you to declare your connected device.

**hlk_log** command will log hexadecimal values of received data or of commands. It may be very useful to check is your device is properly connected with the proper speed.

**hlk_param** query all parameters important for your device.

# HLK-LD1115

LD1115 specific commands are :

```

```

# HLK-LD1125

LD1125 specific commands are :

```

```


# HLK-LD2401

LD2401 specific commands are :

```
  hlk_version   = get sensor version
  hlk_auto      = auto calibrate sensitivity
  hlk_restart   = restart sensor
  hlk_reset     = reset sensor
  hlk_mac       = read MAC address
  hlk_mode 0/1  = set energy mode
  hlk_delay val = detection delay (sec.)
  hlk_bluetooth 0/1   = set bluetooth
  hlk_password pass   = set bluetooth password
  hlk_gate gate pres,motion = gate sensitivity (%)
    gate   : 1 .. 8
    pres   : 0 .. 100
    motion : 0 .. 100
```

# HLK-LD2402

LD2402 specific commands are :

```

```

# HLK-LD2410b

LD1115 specific commands are :

```

```

# HLK-LD2410s

LD2410s specific commands are :

```

```


# HLK-LD2420

LD2420 specific commands are :

```

```

# HLK-LD2450

LD2450 specific commands are :

```

```
