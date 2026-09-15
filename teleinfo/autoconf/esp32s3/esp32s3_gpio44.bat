; ESP32S3
; --------

; Set module configuration
Template {"NAME":"ESP32S3 [GPIO44]","GPIO":[1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,1,5632,1,1,1,1376],"FLAG":0,"BASE":1}

; Set LED brightness to 75%, in sleep mode it will be bright/2
Energyconfig bright=75

; OTA Url
OtaUrl https://github.com/NicolasBernaerts/tasmota/raw/master/teleinfo/binary/tasmota32s3-teleinfo-16m.bin

; Set Default module and Ethernet settings
Backlog Module 0;

