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

If you are still testing the HASP before complete assembly, continue to the [Home Assistant section](05_Home_Assistant.md) to configure your Home Assistant installation to support your HASP device.

---

## Complete Electronics Assembly

### BOM

* [Nextion 2.4" LCD Touchscreen display](https://amzn.to/2TRTEU2)
* [WeMos D1 Mini ESP8266 WiFi microcontroller](https://amzn.to/2UZlga4)
* [Mean Well IRM-03-5 AC to 5VDC Power supply](https://amzn.to/2UUWGa8)
* [2N3904 NPN Transistor](https://amzn.to/2TRuwwD)
* [1k Ohm Resistor](https://amzn.to/2Ec3kTZ)
* [4pin 2.54mm JST-XH PCB header](https://amzn.to/2Eaywmt) *(note that these are the knockoff 2.54mm versions of the 2.50 XH series from JST.  What they lack for in authenticity they make up for in ubiquity and dirt cheap pricing)*
* [PCB](../PCB)
* [6" each of white and black 300V 18AWG stranded power cables](https://amzn.to/2EcMmoA)

> ## WARNING: do not connect AC power and USB at the same time, as there is a chance you could fry your USB ports.  Always disconnect AC before connecting USB or serial to the ESP8266 and Nextion panel (or doing anything else to the device for that matter)

### PCB Unassembled

This is what your PCB should look like from the top before you've placed any of the components.

![PCB Unassembled](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/HASP_PCB_Front.png?raw=true)

### PCB Assembled

Now place the components, taking care to orient the WeMos D1 Mini, Mean Well Power Supply, and JST-XH header as shown in the image below.

![PCB Assembled](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/HASP_PCB_Front_Assembled.png?raw=true)

### Transistor soldering

The transistor and resistor act as a switch on the GND connection for the Nextion LCD.  This allows the HASP to reboot the LCD in the event of a failed LCD firmware update.  The pads on the transistor are a little close together, so take care when soldering here and use as little solder as possible.  **The LCD will not power on until the ESP8266 code is flashed and running**, sending power to the "switch" to turn on the LCD.  If you've flashed the ESP8266 code to the WeMos, you've connected the LCD, you can see the program running in the serial console, and the display isn't powering on... you almost certainly have a problem with the soldering job on the transistor.  If you just can't make it work, that's OK!  Simply cut the resistor out of the circuit (important!), and then you can bridge the 3 pads for the transistor together with a glob of solder.  This will disable the power switch for the LCD, which might mean pulling the device out of the wall if an LCD firmware update fails but will not otherwise impact daily operation.

### High voltage AC cabling

The AC power cables should be at least [18AWG 300V stranded cable](https://amzn.to/2EcMmoA) with a white jacket soldered to the `AC/N` pad on the PCB and a similar wire with a black jacket soldered to `AC/L`.  These are fed through a [rubber push-in grommet](https://amzn.to/2N6Etny) mounted into the rear enclosure.  I've had good luck stripping an existing 3 conductor power cord and using the black/white wires inside.

### Nextion LCD Modification

The Nextion LCD can be modified to give a little extra room around the screw lugs which may help with mounting in some work boxes.  If you have the means to do so, you can use a hot air gun to remove the 4-pin JST XHP connector from the PCB and then use the [`lcdmod` rear enclosure model](../3D_Printable_Models/HASwitchPlate_rear_lcdmod.stl) for some extra room in your box.  If you don't have hot air, some careful use of side cutters or a rotary tool may also do the job.  Be careful not to lift any traces as it's easy to do when applying mechanical force the connector.

In any event, you're going to need to solder the cable harness included with the Nextion display directly to the pins as shown in the image below.  This demonstrates a panel with the connector removed, but you also solder the cable harness directly to the legs of the existing connector with a simple soldering iron with little trouble.  After soldering the cables in place and confirming everything works, I like to put a bit of hot glue over the wires to provide some measure of strain relief.

![Nextion_LCD_modification](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/Nextion_LCD_modification.jpg?raw=true)

### DC Power Option

The [Mean Well IRM-03-5 AC to 5VDC Power supply](https://amzn.to/2UUWGa8) can be substituted with any of the following [Mean Well SLC03-series DC-DC power supplies](http://www.meanwellusa.com/productPdf.aspx?i=786) if you'd prefer a low-voltage DC option.  This should be suitable for use over existing Cat5 cables, speaker cables, alarm cabling, etc already in your wall.  Check to confirm that the existing cable wire gauge will be safe for the load you will be putting on the system, and try not to put voltage down a wire where other expensive and flammable things might be connected which aren't expecting it.

| Model     | Input Voltage Range |
|-----------|---------------------|
| SLC03A-05 | 9-18VDC             |
| SLC03B-05 | 18-36VDC            |
| SLC03C-05 | 36-75VDC            |

These DC-DC supplies are smaller than the AC-DC supply and have a different footprint on the board.  Position the supply as shown below.

![PCB_DC_Assembled](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/PCB_DC_Assembled.jpg?raw=true)

### Schematic for protoboard assembly

This project was initially built on a [4x6 protoboard](https://amzn.to/2N6IqbM) and the enclosure should still accommodate this approach if you have boards on hand.  Note that this approach is probably dangerous as 120VAC and protoboards are a bad mix.

The parts should be fit much as shown in the PCB images above.  In my case the bottom-most row of 8 pins on the WeMos D1 mini [as shown in this image](https://raw.githubusercontent.com/aderusha/HASwitchPlate/master/Documentation/Images/Perfboard_Assembly_Parts_Installed.jpg) would not fit on the perforated area of the board, instead I soldered the pin header with the pins projecting upward from the board to attach wires above.  It wasn't pretty, it's likely not safe, and you really should use the PCB.

![Schematic](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/Schematic.png?raw=true)
