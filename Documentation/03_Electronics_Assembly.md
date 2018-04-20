# Electronics Assembly

## BOM
* [85-305VAC to 5VDC power supply](https://www.arrow.com/en/products/irm-03-5/mean-well-enterprises)
* [Screw terminal connectors](https://www.amazon.com/gp/product/B011QFLS0S)
* [PCB](../PCB)

> ## WARNING: do not connect AC power and USB at the same time, as there is a chance you could fry your USB ports.  Always disconnect AC before connecting USB or serial to the ESP8266 and Nextion panel (or doing anything else to the device for that matter).

## Schematic
![Schematic](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/Schematic.png?raw=true)

## Connection map
| WeMos D1 Mini | Nextion 2.4" LCD | Power Supply |
|:-------------:|:----------------:|:------------:|
|      +5V      |        +5V       |      +5V     |
|      GND      |        GND       |      GND     |
|       D7      |        TX        |              |
|       D4      |        RX        |              |
