; Denky D4
; --------

; Set module configuration
Template {"NAME":"Denky D4 (v1.3a)","GPIO":[32,3200,0,3232,1,0,0,0,0,1,1376,1,0,0,0,0,0,640,608,0,0,0,0,0,0,0,5632,0,0,0,0,0,0,0,0,0],"FLAG":0,"BASE":1}

; Set LED brightness to 75%, in sleep mode it will be bright/2
Energyconfig bright=75

; OTA Url
OtaUrl https://github.com/NicolasBernaerts/tasmota/raw/master/teleinfo/binary/tasmota32-teleinfo-denkyd4.bin

; Set Default module and Ethernet settings
Backlog Module 0;

