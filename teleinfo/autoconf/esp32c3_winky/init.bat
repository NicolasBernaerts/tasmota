; Winky C3
; --------

; Set module configuration
Template {"NAME":"Winky C3","GPIO":[1,4704,1376,5632,4705,640,608,1,1,32,1,0,0,0,0,0,0,0,1,1,1,1],"FLAG":0,"BASE":1}

; OTA Url
OtaUrl https://github.com/NicolasBernaerts/tasmota/raw/master/teleinfo/binary/tasmota32c3-teleinfo-winky.bin

; Set LED brightness to 75%, in sleep mode it will be bright/2
Energyconfig bright=75

; Disable Wifi Scan on restart (to speed wake-up)
SetOption56 0

; Set Telemetry to 290s (300 special reserved by tasmota)
TelePeriod 290

; Set DeepSleep time based on tension
winky_sleep 0

; Set Sleep to 100ms to unload CPU
Sleep 100

; Set Default module and Ethernet settings
Backlog Ethernet 0; Module 0;
