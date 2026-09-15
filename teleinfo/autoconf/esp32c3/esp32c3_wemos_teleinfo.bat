; ESP32S3
; --------

; Set module configuration
Template {"NAME":"Wemos C3 Teleinfo","GPIO":[1,1,1376,1,5632,1,1,288,640,1,608,1,1,1,1376,1,1,640,1,0,1,1],"FLAG":0,"BASE":1}

; Set LED brightness to 75%, in sleep mode it will be bright/2
Energyconfig bright=75

; OTA Url
OtaUrl https://github.com/NicolasBernaerts/tasmota/raw/master/teleinfo/binary/tasmota32c3-teleinfo.bin

; Set Default module and Ethernet settings
Backlog Ethernet 0; Module 0;

