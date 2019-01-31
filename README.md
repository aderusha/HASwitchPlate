# HA SwitchPlate

The HA SwitchPlate is a user-programmable LCD touchscreen you can mount into a [standard North American work box](https://www.nema.org/Standards/ComplimentaryDocuments/NEMA%20WD%206%20-%20Dimensions%20for%20Wiring%20Devices%20-%20Excerpt.pdf) in place of a light switch.  It connects to your home automation system over WiFi to send and receive [MQTT](https://en.wikipedia.org/wiki/MQTT) messages in response to user interactions on the screen or events happening in your home.  The result is an attractive and highly-customizable controller for your home automation system which you can build yourself!

![HA SwitchPlate Models](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/HASwitchPlate_Three_Model_Variations.png?raw=true)

The HA SwitchPlate ("HASP") utilizes a [Nextion 2.4" LCD Touchscreen display](https://amzn.to/2DIpahB) mounted in a 3D-printed enclosure as a touchscreen panel for home control and information display.  An [ESP8266-based microcontroller](https://amzn.to/2Gc92Xs) provides WiFi connectivity and system control.  The project has been developed to integrate with [Home Assistant](https://home-assistant.io/) and [OpenHAB](https://www.openhab.org/) but [should be compatible](Documentation/06_MQTT_Control.md) with any other MQTT-enabled automation platform such as [Domoticz](https://www.domoticz.com/wiki/MQTT), [Node-Red](http://noderedguide.com/tag/mqtt/), [Wink](https://github.com/danielolson13/wink-mqtt), [SmartThings](https://github.com/stjohnjohnson/smartthings-mqtt-bridge), [Vera](https://github.com/jonferreira/vera-mqtt), [HomeKit](https://www.npmjs.com/package/homekit2mqtt), etc.

The [Arduino code](Arduino_Sketch) for the ESP8266 provides a generic gateway between [MQTT](https://en.wikipedia.org/wiki/MQTT) and the [Nextion instruction set](https://www.itead.cc/wiki/Nextion_Instruction_Set).  A basic [Nextion HMI display file](Nextion_HMI) has been included with several pages of various layouts to provide user controls or to present information in response to MQTT messages sent to the device.

## Demo screens

![Scene controller](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/HASwitchPlate_Demo_SceneController.png?raw=true) ![Status display](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/HASwitchPlate_Demo_Status.png?raw=true) ![Media control](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/HASwitchPlate_Demo_Media.png?raw=true) ![3D printer monitor](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/HASwitchPlate_Demo_PrintStatus.png?raw=true) ![Alarm Panel](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/HASwitchPlate_Demo_AlarmPanel.png?raw=true) ![Slider/Dimmer Controls](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/HASwitchPlate_Demo_Dimmers.png?raw=true)

## Purchase an assembled unit

As this build requires some specialist skills and tools, I will occasionally be [offering assembled devices for sale here](https://www.tindie.com/products/luma/ha-switchplate-hasp-single-wide-assembled/).

## Bill of Materials

To build a simple version of this project you will minimally need the [Nextion display](https://amzn.to/2DIpahB) and the [WeMos D1 Mini](https://amzn.to/2Gc92Xs), 4 jumper wires, and a USB cable to power both devices.

A complete build that's ready to install will require the following components:

* [Nextion 2.4" LCD Touchscreen display](https://amzn.to/2DIpahB)
* [WeMos D1 Mini ESP8266 WiFi microcontroller](https://amzn.to/2Gc92Xs)
* [3D printed switch plate](3D_Printable_Models/HASwitchPlate_front_single.stl)
* [3D printed rear cover](3D_Printable_Models/HASwitchPlate_rear_nolcdmod.stl)
* [Mean Well IRM-03-5 AC to 5VDC Power supply](https://amzn.to/2MNF7Gx)
* [PCB](PCB/)
* [4pin 2.54mm JST-XH PCB header](https://amzn.to/2MFV15p)
* [Rubber grommet](https://amzn.to/2G9y49I)
* [6" each of white and black 300V 18AWG stranded power cables](https://amzn.to/2SjXO9G)
* [Two M2 self-tapping 6MM screws](https://amzn.to/2TnlsQ6) (or just any 4-6mm M2 screws) to mount PCB in rear enclosure
* [Four 20mm M2 flathead screws](https://amzn.to/2Gb5pAZ) and [four 3mm M2 threaded inserts](https://amzn.to/2SgVO21) to fasten both halves of the enclosure together

## Get Started!

[Check out the documentation](Documentation/) to get started building your own HA SwitchPlate.