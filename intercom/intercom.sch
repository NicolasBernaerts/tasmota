EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title ""
Date ""
Rev ""
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L Relay:DIPxx-1Cxx-51x Micro-relais
U 1 1 5ED2FA65
P 2200 2700
F 0 "Micro-relais" H 1770 2700 50  0000 R CNN
F 1 "DIPxx-1Cxx-51x" H 1770 2655 50  0001 R CNN
F 2 "Relay_THT:Relay_StandexMeder_DIP_LowProfile" H 2650 2650 50  0001 L CNN
F 3 "https://standexelectronics.com/wp-content/uploads/datasheet_reed_relay_DIP.pdf" H 2200 2700 50  0001 C CNN
	1    2200 2700
	-1   0    0    -1  
$EndComp
$Comp
L Connector:4P2C Interphone
U 1 1 5ED354AE
P 1200 1500
F 0 "Interphone" H 1257 1967 50  0000 C CNN
F 1 "2 fils" H 1257 1876 50  0000 C CNN
F 2 "" V 1200 1550 50  0001 C CNN
F 3 "~" V 1200 1550 50  0001 C CNN
	1    1200 1500
	1    0    0    -1  
$EndComp
Wire Wire Line
	1600 1400 1650 1400
Wire Wire Line
	1800 1500 1600 1500
Wire Wire Line
	1800 2000 1800 1500
$Comp
L Device:R R1
U 1 1 5ED3DF1C
P 2500 1400
F 0 "R1" V 2293 1400 50  0001 C CNN
F 1 "3.3k" V 2385 1400 50  0000 C CNN
F 2 "" V 2430 1400 50  0001 C CNN
F 3 "~" H 2500 1400 50  0001 C CNN
	1    2500 1400
	0    1    1    0   
$EndComp
Wire Wire Line
	3050 1400 3050 1600
Wire Wire Line
	3050 1600 3150 1600
Wire Wire Line
	3050 2000 3050 1800
Wire Wire Line
	3050 1800 3150 1800
Wire Wire Line
	1900 2400 1900 2250
Wire Wire Line
	1900 2250 1800 2250
Wire Wire Line
	1800 2250 1800 2000
Connection ~ 1800 2000
Wire Wire Line
	2000 3000 2000 3100
Wire Wire Line
	2000 3100 1650 3100
Wire Wire Line
	1650 3100 1650 1400
$Comp
L Diode:1N4148 D?
U 1 1 5ED468D4
P 3350 2700
F 0 "D?" V 3304 2779 50  0001 L CNN
F 1 "1N4148" V 3350 2779 50  0000 L CNN
F 2 "Diode_THT:D_DO-35_SOD27_P7.62mm_Horizontal" H 3350 2525 50  0001 C CNN
F 3 "https://assets.nexperia.com/documents/data-sheet/1N4148_1N4448.pdf" H 3350 2700 50  0001 C CNN
	1    3350 2700
	0    1    1    0   
$EndComp
$Comp
L Transistor_BJT:BC556 Q?
U 1 1 5ED49B5A
P 3450 3300
F 0 "Q?" H 3641 3254 50  0001 L CNN
F 1 "BC556" H 3641 3300 50  0000 L CNN
F 2 "Package_TO_SOT_THT:TO-92_Inline" H 3650 3225 50  0001 L CIN
F 3 "http://www.fairchildsemi.com/ds/BC/BC557.pdf" H 3450 3300 50  0001 L CNN
	1    3450 3300
	-1   0    0    1   
$EndComp
$Comp
L Device:R R?
U 1 1 5ED4AEE4
P 4000 3300
F 0 "R?" V 3793 3300 50  0001 C CNN
F 1 "1.2k" V 3885 3300 50  0000 C CNN
F 2 "" V 3930 3300 50  0001 C CNN
F 3 "~" H 4000 3300 50  0001 C CNN
	1    4000 3300
	0    1    1    0   
$EndComp
Wire Wire Line
	2400 2400 3350 2400
Wire Wire Line
	3350 2400 3350 2550
Wire Wire Line
	3350 3000 3350 2850
Wire Wire Line
	2400 3000 3350 3000
Wire Wire Line
	3350 3000 3350 3100
Connection ~ 3350 3000
$Comp
L power:VCC #PWR?
U 1 1 5ED521DF
P 4350 1000
F 0 "#PWR?" H 4350 850 50  0001 C CNN
F 1 "VCC" V 4367 1128 50  0000 L CNN
F 2 "" H 4350 1000 50  0001 C CNN
F 3 "" H 4350 1000 50  0001 C CNN
	1    4350 1000
	0    1    1    0   
$EndComp
$Comp
L power:GNDREF #PWR?
U 1 1 5ED52DF3
P 4450 3600
F 0 "#PWR?" H 4450 3350 50  0001 C CNN
F 1 "GNDREF" H 4455 3427 50  0000 C CNN
F 2 "" H 4450 3600 50  0001 C CNN
F 3 "" H 4450 3600 50  0001 C CNN
	1    4450 3600
	1    0    0    -1  
$EndComp
$Comp
L Connector:Conn_01x01_Male J?
U 1 1 5ED5684B
P 4550 3300
F 0 "J?" H 4522 3324 50  0001 R CNN
F 1 "Tx" H 4522 3278 50  0000 R CNN
F 2 "" H 4550 3300 50  0001 C CNN
F 3 "~" H 4550 3300 50  0001 C CNN
	1    4550 3300
	-1   0    0    -1  
$EndComp
Wire Wire Line
	3650 3300 3850 3300
Wire Wire Line
	4150 3300 4350 3300
Wire Wire Line
	3350 3600 3350 3500
$Comp
L Device:R R?
U 1 1 5ED5D188
P 3950 1300
F 0 "R?" V 3743 1300 50  0001 C CNN
F 1 "3.3k" H 4020 1300 50  0000 L CNN
F 2 "" V 3880 1300 50  0001 C CNN
F 3 "~" H 3950 1300 50  0001 C CNN
	1    3950 1300
	1    0    0    -1  
$EndComp
$Comp
L Connector:Conn_01x01_Male J?
U 1 1 5ED5E0B9
P 4550 1600
F 0 "J?" H 4522 1624 50  0001 R CNN
F 1 "Rx" H 4522 1578 50  0000 R CNN
F 2 "" H 4550 1600 50  0001 C CNN
F 3 "~" H 4550 1600 50  0001 C CNN
	1    4550 1600
	-1   0    0    -1  
$EndComp
Wire Wire Line
	3750 1800 3750 3600
Wire Wire Line
	3750 3600 3350 3600
Wire Wire Line
	3750 1600 3950 1600
Wire Wire Line
	3950 1450 3950 1600
Connection ~ 3950 1600
Wire Wire Line
	3950 1600 4350 1600
Wire Wire Line
	3950 1150 3950 1000
Wire Wire Line
	3950 1000 4250 1000
Wire Wire Line
	3350 2400 4250 2400
Wire Wire Line
	4250 2400 4250 1000
Connection ~ 3350 2400
Connection ~ 4250 1000
Wire Wire Line
	4250 1000 4350 1000
Wire Wire Line
	3750 3600 4450 3600
Connection ~ 3750 3600
$Comp
L Isolator:PC817 U?
U 1 1 618076B4
P 3450 1700
F 0 "U?" H 3450 2025 50  0001 C CNN
F 1 "PC816" H 3450 1933 50  0000 C CNN
F 2 "Package_DIP:DIP-4_W7.62mm" H 3250 1500 50  0001 L CIN
F 3 "http://www.soselectronic.cz/a_info/resource/d/pc817.pdf" H 3450 1700 50  0001 L CNN
	1    3450 1700
	1    0    0    -1  
$EndComp
$Comp
L Diode:1N4148 D?
U 1 1 61F6A4A7
P 2850 1700
F 0 "D?" V 2804 1779 50  0001 L CNN
F 1 "1N4148" V 2850 1779 50  0000 L CNN
F 2 "Diode_THT:D_DO-35_SOD27_P7.62mm_Horizontal" H 2850 1525 50  0001 C CNN
F 3 "https://assets.nexperia.com/documents/data-sheet/1N4148_1N4448.pdf" H 2850 1700 50  0001 C CNN
	1    2850 1700
	0    1    1    0   
$EndComp
Wire Wire Line
	1800 2000 2850 2000
Wire Wire Line
	2650 1400 2850 1400
Wire Wire Line
	2850 1550 2850 1400
Connection ~ 2850 1400
Wire Wire Line
	2850 1400 3050 1400
Wire Wire Line
	2850 1850 2850 2000
Connection ~ 2850 2000
Wire Wire Line
	2850 2000 3050 2000
Wire Wire Line
	2350 1400 1650 1400
Connection ~ 1650 1400
$EndSCHEMATC
