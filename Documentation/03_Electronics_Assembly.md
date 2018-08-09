# Electronics Assembly

## Test Assembly

Once the [ESP8266](01_Arduino_Sketch.md) and [Nextion](02_Nextion_HMI.md) firmware images have been flashed, you can connect the two devices together using the following wiring map:

| WeMos Pin | Nextion Cable |
|-----------|---------------|
| 5V        | Red           |
| GND       | Black         |
| D7        | Blue          |
| D4        | Yellow        |

### First-time Setup

With your device flashed and wired up, plug the WeMos into a USB port to power up your HASP.  You should be presented with an initialization screen and then information regarding WiFi access along with a QR code for easy connection.  With a PC or mobile device, connect to the WiFi SSID and password displayed on the Nextion (or serial output if you don't have the Nextion ready).  You should be prompted to open a [configuration website](http://192.168.4.1) which will allow you to configure HASP to connect to your WiFi network.  You can set the MQTT broker information and admin credentials now, or use the web interface to do so later.  Once you `save settings` the device will connect to your network.  Congratulations, you are now online!

![WiFi Config 0](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/WiFi_Config_0.png?raw=true) ![WiFi Config 1](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/WiFi_Config_1.png?raw=true) ![WiFi Config 2](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/WiFi_Config_2.png?raw=true)

If you are still testing the HASP before complete assembly, continue to the [Home Assistant section](../05_Home_Assistant.md) to configure your Home Assistant installation to support your HASP device.

---

## Complete Electronics Assembly

### BOM

* [85-305VAC to 5VDC power supply](https://www.arrow.com/en/products/irm-03-5/mean-well-enterprises)
* [Nextion 2.4" LCD touchscreen display](https://www.itead.cc/nextion-nx3224t024.html)
* [WeMos D1 Mini ESP8266 WiFi microcontroller](https://wiki.wemos.cc/products:d1:d1_mini)
* [4-pin male "JST-XH" header](https://www.aliexpress.com/wholesale?SearchText=jst+xh+4+pin) *(note that these are the knockoff 2.54mm versions of the 2.50 XH series from JST.  What they lack for in authenticity they make up for in ubiquiti and dirt cheap pricing)*
* [PCB](../PCB)

> ## WARNING: do not connect AC power and USB at the same time, as there is a chance you could fry your USB ports.  Always disconnect AC before connecting USB or serial to the ESP8266 and Nextion panel (or doing anything else to the device for that matter)

### PCB Unassembled

This is what your PCB should look like from the top before you've placed any of the components.

![PCB Unassembled](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/HASP_PCB_Front.png?raw=true)

### PCB Assembled

Now place the components, taking care to orient the WeMos D1 Mini and JST-XH header as shown in the image below.

![PCB Assembled](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/HASP_PCB_Front_Assembled.png?raw=true)

### High voltage AC cabling

The AC power cables should be at least 18AWG 300V stranded cable with a white jacket soldered to the `AC/N` pad on the PCB and a similar wire with a black jacket soldered to `AN/L`.  These are fed through a [rubber push-in grommet](https://www.mcmaster.com/#9600k41) mounted into the rear enclosure.  I've had good luck stripping an existing 3 conductor power cord and using the black/white wires inside.

### Nextion LCD Modification

The Nextion LCD can be modified to give a little extra room around the screw lugs which may help with mounting in some work boxes.  If you have the means to do so, you can use a hot air gun to remove the 4-pin JST XHP connector from the PCB and then use the `lcdmod` rear enclosure model for some extra room in your box.

In any event, you're going to need to solder the cable harness included with the Nextion display directly to the pins as shown in the image below.  This demonstrates a panel with the connector removed, but you also solder the cable harness directly to the legs of the existing connector with a simple soldering iron with little trouble.

![Nextion_LCD_modification](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/Nextion_LCD_modification.jpg?raw=true)

### Schematic for protoboard assembly

This project was initially built on a [4x6 protoboard](https://www.amazon.com/gp/product/B06XTPGBS5) and the enclosure should still accomodate this approach if you have boards on hand.  Note that this approach is probably dangerous as 120VAC and protoboards are often a bad mix.

The parts should be fit much as shown in the PCB images above.  In my case the bottom-most row of 8 pins on the WeMos D1 mini [as shown in this image](https://raw.githubusercontent.com/aderusha/HASwitchPlate/master/Documentation/Images/Perfboard_Assembly_Parts_Installed.jpg) would not fit on the perforated area of the board, instead I soldered the pin header with the pins projecting upward from the board to attach wires above.  It wasn't pretty, it's likely not safe, and you really should use the PCB.

![Schematic](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/Schematic.png?raw=true)