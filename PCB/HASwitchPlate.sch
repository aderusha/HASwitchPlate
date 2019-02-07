EESchema Schematic File Version 4
LIBS:HASwitchPlate-cache
EELAYER 26 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title "HA SwitchPlate"
Date ""
Rev ""
Comp "https://github.com/aderusha/HASwitchPlate"
Comment1 "allen@derusha.org"
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L HASwitchPlate-library:WeMos_mini-HASwitchPlate U2
U 1 1 59E27669
P 5850 3800
F 0 "U2" H 5850 4300 60  0000 C CNN
F 1 "WeMos_mini" H 5850 3300 60  0000 C CNN
F 2 "wemos-d1-mini:wemos-d1-mini-with-pin-header-and-connector" H 6400 3100 60  0001 C CNN
F 3 "" H 6400 3100 60  0000 C CNN
	1    5850 3800
	1    0    0    -1  
$EndComp
$Comp
L HASwitchPlate-library:Screw_Terminal_01x02-HASwitchPlate-library J1
U 1 1 59E27A1F
P 3600 3450
F 0 "J1" H 3600 3550 50  0000 C CNN
F 1 "Screw Term." H 3600 3250 50  0001 C CNN
F 2 "Connectors_Terminal_Blocks:TerminalBlock_bornier-2_P5.08mm" H 3600 3450 50  0001 C CNN
F 3 "" H 3600 3450 50  0001 C CNN
	1    3600 3450
	-1   0    0    -1  
$EndComp
$Comp
L HASwitchPlate-library:XH2.54-4P-HASwitchPlate-library J2
U 1 1 59E297BA
P 7150 3650
F 0 "J2" H 7150 3250 50  0000 C CNN
F 1 "XH2.54-4P" H 7150 3350 50  0000 C CNN
F 2 "HASwitchPlate:JST_XH2.54_04x2.54mm_Straight" H 7150 3650 50  0001 C CNN
F 3 "" H 7150 3650 50  0001 C CNN
	1    7150 3650
	-1   0    0    1   
$EndComp
$Comp
L HASwitchPlate-library:IRM-03-5-HASwitchPlate-library U1
U 1 1 59E37A8B
P 4150 3550
F 0 "U1" H 4550 3800 50  0000 C CNN
F 1 "IRM-03-5" H 4550 3400 50  0000 C CNN
F 2 "HASwitchPlate:ACDC-Converter_MeanWell-IRM-03-x" H 4550 3300 50  0001 C CNN
F 3 "https://www.meanwell.com/Upload/PDF/IRM-03/IRM-03-SPEC.PDF" H 4550 3200 50  0001 C CNN
	1    4150 3550
	1    0    0    -1  
$EndComp
$Comp
L HASwitchPlate-library:Conn_01x08-conn-HASwitchPlate J3
U 1 1 5AB13587
P 5750 2350
F 0 "J3" H 5750 2775 50  0000 C CNN
F 1 "Breakout" V 5750 1700 50  0000 C CNN
F 2 "Pin_Headers:Pin_Header_Straight_1x08_Pitch2.54mm" H 5750 2350 50  0001 C CNN
F 3 "" H 5750 2350 50  0001 C CNN
	1    5750 2350
	0    -1   -1   0   
$EndComp
Text Notes 5475 2275 1    60   ~ 0
GND
Text Notes 5575 2300 1    60   ~ 0
+5V
Text Notes 5675 2300 1    60   ~ 0
+3.3V
Text Notes 5775 2275 1    60   ~ 0
A0
Text Notes 5875 2275 1    60   ~ 0
D0
Text Notes 5975 2275 1    60   ~ 0
D1
Text Notes 6075 2275 1    60   ~ 0
D2
Text Notes 6175 2275 1    60   ~ 0
DBG
Text Label 6650 4500 0    60   ~ 0
GND
Text Label 6950 3650 2    60   ~ 0
LCD_RX
Text Label 6925 3550 2    60   ~ 0
LCD_TX
Text Label 6850 3450 2    60   ~ 0
+5V
Wire Wire Line
	6425 3550 6950 3550
Wire Wire Line
	6425 3650 6425 3550
Wire Wire Line
	6350 3650 6425 3650
Wire Wire Line
	4950 3550 5050 3550
Wire Wire Line
	4950 3450 5000 3450
Wire Wire Line
	5350 3650 5300 3650
Wire Wire Line
	5300 3650 5300 4400
Wire Wire Line
	5300 4400 6525 4400
Wire Wire Line
	6525 4400 6525 3650
Wire Wire Line
	6525 3650 6950 3650
Wire Wire Line
	5200 3200 5550 3200
Wire Wire Line
	6525 3200 6525 3450
Wire Wire Line
	6525 3450 6950 3450
Wire Wire Line
	5200 3550 5200 4500
Connection ~ 5200 3550
Wire Wire Line
	3800 3550 4075 3550
Wire Wire Line
	3800 3450 4125 3450
Wire Wire Line
	5200 3200 5200 3450
Connection ~ 5200 3450
Wire Wire Line
	5275 3550 5275 3075
Wire Wire Line
	5275 3075 5450 3075
Connection ~ 5275 3550
Wire Wire Line
	5550 2550 5550 3200
Connection ~ 5550 3200
Wire Wire Line
	5650 2550 5650 3250
Wire Wire Line
	5650 3250 6350 3250
Wire Wire Line
	6350 3250 6350 3450
Wire Wire Line
	6350 4050 6400 4050
Wire Wire Line
	6400 4050 6400 3175
Wire Wire Line
	6400 3175 5750 3175
Wire Wire Line
	5750 3175 5750 2550
Wire Wire Line
	6350 3950 6450 3950
Wire Wire Line
	6450 3950 6450 3150
Wire Wire Line
	6450 3150 5850 3150
Wire Wire Line
	5850 3150 5850 2550
Wire Wire Line
	5350 3950 5325 3950
Wire Wire Line
	5325 3950 5325 3125
Wire Wire Line
	5325 3125 5950 3125
Wire Wire Line
	5950 3125 5950 2550
Wire Wire Line
	5350 3850 5275 3850
Wire Wire Line
	5275 3850 5275 3600
Wire Wire Line
	5275 3600 5300 3600
Wire Wire Line
	5300 3600 5300 3100
Wire Wire Line
	5300 3100 6050 3100
Wire Wire Line
	6050 3100 6050 2550
Wire Wire Line
	6350 3550 6375 3550
Wire Wire Line
	6375 3550 6375 3075
Wire Wire Line
	6375 3075 6150 3075
Wire Wire Line
	5450 3075 5450 2550
Wire Wire Line
	6150 3075 6150 2550
Text Label 6150 2950 1    60   ~ 0
D8_DBG
Text Label 6050 2950 1    60   ~ 0
D2
Text Label 5950 2950 1    60   ~ 0
D1
Text Label 5850 2950 1    60   ~ 0
D0
Text Label 5750 2950 1    60   ~ 0
A0
Text Label 5650 2975 1    60   ~ 0
+3.3V
Text Label 4050 3550 2    60   ~ 0
AC_N
Text Label 3825 3450 0    60   ~ 0
AC_L
Wire Wire Line
	5200 3550 5275 3550
Wire Wire Line
	5200 3450 5350 3450
Wire Wire Line
	5275 3550 5350 3550
Wire Wire Line
	5550 3200 6525 3200
$Comp
L HASwitchPlate-library:SLC03-series U3
U 1 1 5C2FF5D5
P 4150 4150
F 0 "U3" H 4550 4400 50  0000 C CNN
F 1 "SLC03-series" H 4550 4000 50  0000 C CNN
F 2 "HASwitchPlate:ACDC-Converter_MeanWell-SLC03-series" H 4550 3900 50  0001 C CNN
F 3 "http://www.meanwellusa.com/productPdf.aspx?i=786" H 4550 3800 50  0001 C CNN
	1    4150 4150
	1    0    0    -1  
$EndComp
Wire Wire Line
	4125 3450 4125 4050
Wire Wire Line
	4125 4050 4150 4050
Connection ~ 4125 3450
Wire Wire Line
	4125 3450 4150 3450
Wire Wire Line
	4075 3550 4075 4150
Wire Wire Line
	4075 4150 4150 4150
Connection ~ 4075 3550
Wire Wire Line
	4075 3550 4150 3550
Wire Wire Line
	4950 4050 5000 4050
Wire Wire Line
	5000 4050 5000 3450
Connection ~ 5000 3450
Wire Wire Line
	5000 3450 5200 3450
Wire Wire Line
	4950 4150 5050 4150
Wire Wire Line
	5050 4150 5050 3550
Connection ~ 5050 3550
Wire Wire Line
	5050 3550 5200 3550
$Comp
L Transistor_BJT:S8050 Q1
U 1 1 5C306D5D
P 7050 4150
F 0 "Q1" H 7241 4196 50  0000 L CNN
F 1 "S8050" H 7241 4105 50  0000 L CNN
F 2 "Package_TO_SOT_THT:TO-92L_HandSolder" H 7250 4075 50  0001 L CIN
F 3 "http://www.unisonic.com.tw/datasheet/S8050.pdf" H 7050 4150 50  0001 L CNN
	1    7050 4150
	1    0    0    -1  
$EndComp
$Comp
L Device:R_US R1
U 1 1 5C306DF8
P 6600 3900
F 0 "R1" H 6668 3946 50  0000 L CNN
F 1 "1k" H 6668 3855 50  0000 L CNN
F 2 "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal" V 6640 3890 50  0001 C CNN
F 3 "~" H 6600 3900 50  0001 C CNN
	1    6600 3900
	1    0    0    -1  
$EndComp
Wire Wire Line
	6350 3750 6600 3750
Wire Wire Line
	6600 4050 6600 4150
Wire Wire Line
	6600 4150 6850 4150
Wire Wire Line
	7150 3950 6850 3950
Wire Wire Line
	6850 3950 6850 3750
Wire Wire Line
	6850 3750 6950 3750
Text Label 6850 3900 0    60   ~ 0
LCD_GND
Text Label 6600 4150 0    60   ~ 0
LCD_CTL
Wire Wire Line
	5200 4500 7150 4500
Wire Wire Line
	7150 4500 7150 4350
Text Notes 4225 3175 0    60   ~ 0
Choose one PSU\noption below
$EndSCHEMATC
