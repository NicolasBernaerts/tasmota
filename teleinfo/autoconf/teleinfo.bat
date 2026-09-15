; Generic Teleinfo
; ----------------

; Reset settings, but preserve wifi, MQTT and FS
;InitDevice 6

; Disable analog values display
WebSensor2 0

; Disable energy values display
WebSensor3 0

; Disable Boot Loop Detection
SetOption65 1

; Enable Wifi Scan (avoid wifi lost if router change channel)
SetOption56 1

; Disable ESP32 temperature display
SetOption146 0

; Set WebRefresh to 1 second
WebRefresh 1000

; Set auto timezone
Backlog Timezone 99; TimeStd 0,0,10,1,3,60; TimeDst 0,0,3,1,2,120;

; Set blinking LED color : 0 for Green LED and 1 for Period Indicator (blue, white or red)
; Set Teleinfo to autodetect mode 
Energyconfig period=1 reset

