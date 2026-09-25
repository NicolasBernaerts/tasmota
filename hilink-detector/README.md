Hilink presence detector drivers for Tasmota
-------

You'll get here a generic Tasmota driver to handle different series of Hilink presence detectors.

Devices currently handled are :
  * HLK-LD1115
  * HLK-LD1125
  * HLK-LD2401
  * HLK-LD2402
  * HLK-LD2410b
  * HLK-LD2410c
  * HLK-LD2410s
  * HLK-LD2420
  * HLK-LD2450
  * HLK-LD2454

Each devices has its own specificities, but the driver provides some generic commands to select and manage the connected device.

Any Hilink detector should be connected thru Rx/Tx and declared as **Tx 2410** and **Rx 2410**

You can then select your specific decice with **hlk_device** command.

After reboot, your detector should be recognised and operationnal.

You should see a graphical target range display on the main page with your device type.

# Generic commands

Run **hlk** command to get help on available device commands.

```
hlk_device <dev>  = set device type
  none LD1115 LD1125 LD2401 LD2402 LD2410 LD2410s LD2412 LD2420 LD2450 LD2454
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

[LD1115](xsns_102_01_ld1115.ino) specific commands are :

```
hlk_reset      = reset parameters to default
hlk_save       = save parameters to sensor
hlk_th idx val = sensor threshold (%)
  idx  : 1 .. 3      (sensor index)
  val  : 1 .. 100000 (sensor threshold)
hlk_sn type num = set samples to average
  type : occ (presence), mov (motion)
  num  : number of samples
```

# HLK-LD1125

[LD1125](xsns_102_01_ld1125.ino) specific commands are :

```
hlk_reset      = reset parameters to default
hlk_save       = save parameters to sensor
hlk_th idx val = sensor threshold (%)
  idx  : 1 .. 3      (sensor index)
  val  : 1 .. 100000 (sensor threshold)
```


# HLK-LD2401

[LD2401](xsns_102_01_ld2401.ino) specific commands are :

```
hlk_restart        = restart sensor
hlk_reset          = reset sensor
hlk_auto           = auto calibrate sensitivity
hlk_energy <0/1>   = set energy mode (default)
hlk_delay <val>    = detection delay (sec.)
hlk_width <20/75>  = gate width (cm)
hlk_bt <0/1> <pwd> = bluetooth status and password
hlk_out <0/1>      = output level on detection
hlk_gate gate pres,motion = gate sensitivity (%)
  gate   : 1 .. 8
  pres   : 0 .. 100
  motion : 0 .. 100
hlk_light <conf> <thres>  = light sensitivity control
  conf  : off             disabled
          below           detection on low light
          above           detection on high light
  thres : trigger level   0..100%
```

# HLK-LD2402

[LD2402](xsns_102_01_ld2402.ino) specific commands are :

```
hlk_save         = save parameters
hlk_energy <0/1> = set energy mode (defaut)
hlk_gain         = auto gain adjustment
hlk_auto trig,keep,micr = start auto level detection
  trig : trigger coefficient    1..20 [2]
  keep : keep coefficient       1..20 [3]
  micr : micro move coefficient 1..20 [2]
hlk_gate gate move,micr = gate sensitivity (dB)
  gate : gate number      1..16
  move : motion level     0.00..95.00
  micr : micro move level 0.00..95.00
```

# HLK-LD2410b and HLK-LD2410c

[LD2410b and LD2410c](xsns_102_01_ld2410.ino) specific commands are :

```
hlk_reset          = reset sensor
hlk_restart        = restart sensor
hlk_auto           = start auto level detection
hlk_energy <0/1>   = set data energy (default)
hlk_bt <0/1> <pwd> = bluetooth status and password
hlk_width <val>    = set gate width cm (20 or 75)
hlk_out <0/1>      = output level on detection
hlk_gate gate presence,motion = gate sensitivity (%)
  gate     : 1..8
  presence : 1..100
  motion   : 1..100
hlk_light <conf> <thres> = light sensitivity control
  conf  : off             disabled
          below           detection on low light
          above           detection on high light
  thres : trigger level   0..100%
```

# HLK-LD2410s

[LD2410s](xsns_102_01_ld2410s.ino) specific commands are :

```
hlk_reset        = reset coefficients to factory
hlk_restart      = restart device
hlk_serial <sn>  = set serial number
hlk_energy <0/1> = set energy mode (default)
hlk_freq <val>   = update data frequency (Hz)
  val     : 0.5 .. 8
hlk_auto time,trig,ret = start auto level detection
  time : scanning time in sec. [60]
  trig : trigger factor [2]
  ret  : retention factor [1]
hlk_gate gate trig,hold = gate sensitivity (%)
  gate : gate number   1 .. 16
  trig : trigger level 0 .. 100
  hold : holding level 0 .. 100
```
# HLK-LD2412

[LD2412](xsns_102_01_ld2412.ino) specific commands are :

```
hlk_reset          = reset sensor
hlk_restart        = restart sensor
hlk_auto           = start auto level detection
hlk_energy <0/1>   = set data energy (default)
hlk_bt <0/1>       = bluetooth status
hlk_width <val>    = set gate width cm (20, 50 or 75)
hlk_out <0/1>      = output level on detection
hlk_gate gate motion,static = gate sensitivity (%)
  gate     : 1..14
  presence : 1..100
  motion   : 1..100
hlk_light <conf> <thres> = light sensitivity control
  conf  : off             disabled
          below           detection on low light
          above           detection on high light
  thres : trigger level   0..100%
```

# HLK-LD2420

[LD2420](xsns_102_01_ld2420.ino) specific commands are :

```
hlk_restart      = restart sensor
hlk_reset        = reset sensor
hlk_serial <sn>  = set serial number
hlk_energy <0/1> = set energy mode
hlk_gate gate trigger,hold = gate sensitivity (%)
  gate    : 1 .. 12
  trigger : 0.00 .. 100
  hold    : 0.00 .. 100
```

# HLK-LD2450

[LD2450](xsns_102_01_ld2450.ino) specific commands are :

```
hlk_reset       = reset detector (will restart)
hlk_restart     = restart detector
hlk_bt <0/1>    = set bluetooth
hlk_zone        = query detection zone
hlk_mode <mode> = target mode
  single : single target mode
  multi  : multi target mode
hlk_zone <cmnd>     = set zone detection behaviour
  reset : reset all detection zone
  off   : disable zones
  inc   : detection within zones
  exc   : detection outside zones
hlk_zone id x1,y1,x2,y2 = set detection zone
  id    : 1..3
  x1,x2 : -600..600 (cm)
  y1,y2 : 0..600 (cm)
```

# HLK-LD2454

LD2454 is a simplified version of LD2450.

[LD2454](xsns_102_01_ld2454.ino) specific commands are :

```
hlk_reset       = reset detector (will restart)
hlk_restart     = restart detector
hlk_mode <mode> = target mode
  single : single target mode
  multi  : multi target mode
```
