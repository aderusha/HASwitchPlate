# Electronics Assembly

## BOM
* [85-305VAC to 5VDC power supply](https://www.arrow.com/en/products/irm-03-5/mean-well-enterprises)
* [Nextion 2.4" LCD Touchscreen display](https://www.itead.cc/nextion-nx3224t024.html)
* [WeMos D1 Mini ESP8266 WiFi microcontroller](https://wiki.wemos.cc/products:d1:d1_mini)
* [Screw terminal connectors](https://www.amazon.com/gp/product/B011QFLS0S)
* [4-pin Male "JST-XH" header](https://www.aliexpress.com/wholesale?SearchText=jst+xh+2.54+male+4+pin) *(note that these are the knockoff 2.54mm versions of the 2.50 XH series from JST.  What they lack for in authenticity they make up for in ubiquiti and dirt cheap pricing)*
* [PCB](../PCB)

> ## WARNING: do not connect AC power and USB at the same time, as there is a chance you could fry your USB ports.  Always disconnect AC before connecting USB or serial to the ESP8266 and Nextion panel (or doing anything else to the device for that matter).

## PCB Unassembled
This is what your PCB should look like from the top before you've placed any of the components.

![PCB Unassembled](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/HASP_PCB_Front.png?raw=true)

## PCB Assembled
Now place the components, taking care to align the WeMos D1 Mini and JST-XH header as shown in the image below.

![PCB Unassembled](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/HASP_PCB_Front_Assembled.png?raw=true)

## Schematic for protoboard assembly
This project was initially built on a [4x6 protoboard](https://www.amazon.com/gp/product/B06XTPGBS5) and the enclosure should still accomodate this approach if you have boards on hand.  Note that this approach is probably dangerous as 120VAC and protoboards are often a bad mix.

The parts should be fit much as shown in the PCB images above.  In my case the bottom-most row of 8 pins on the WeMos D1 mini [as shown in this image](https://raw.githubusercontent.com/aderusha/HASwitchPlate/master/Documentation/Images/Perfboard_Assembly_Parts_Installed.jpg) would not fit on the perforated area of the board, instead i soldered the pin header with the pins projecting upward from the board to attach wires above.  It wasn't pretty, it's likely not safe, and you really should use the PCB.

![Schematic](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/Schematic.png?raw=true)
