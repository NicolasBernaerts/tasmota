; Winky C6
; --------

; Set module configuration
Template {"NAME":"Winky C6","GPIO":[1,4704,1376,4705,5632,4706,640,608,1,32,1,0,0,0,0,0,0,0,1,1,1,1,3840,4096,0,0,0,0,0,0,0],"FLAG":0,"BASE":1}

; OTA Url
OtaUrl https://github.com/NicolasBernaerts/tasmota/raw/master/teleinfo/binary/tasmota32c6-teleinfo-winky.bin

; Set LED brightness to 75%, in sleep mode it will be bright/2
Energyconfig bright=75

; Disable Wifi Scan on restart (to speed wake-up)
SetOption56 0

; Set Telemetry to 290s (300 special reserved by tasmota)
TelePeriod 290

; Set Winky main options
winky_sleep 0
winky_target 4.2
winky_cosphi 0

; Set Sleep to 100ms to unload CPU
Sleep 100

; Set Default module and Ethernet settings
Backlog Ethernet 0; Module 0;

