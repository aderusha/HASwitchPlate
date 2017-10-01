# HA SwitchPlate

This project utilizes a [Nextion 2.4" LCD Touchscreen display](https://www.itead.cc/nextion-nx3224t024.html) mounted in a standard-sized single-gang workbox as a touchscreen panel for home control and information display.  An [ESP8266-based microcontroller](https://wiki.wemos.cc/products:d1:d1_mini) provides WiFi connectivity and system control.  The project has been developed to integrate with [Home Assistant](https://home-assistant.io/) but [should be compatible](Documentation/06_MQTT_Control.md) with any other MQTT-enabled automation platform such as [OpenHAB](https://github.com/openhab/openhab1-addons/wiki/MQTT-Binding), [Domoticz](https://www.domoticz.com/wiki/MQTT), [Node-Red](http://noderedguide.com/tag/mqtt/), [Wink](https://github.com/danielolson13/wink-mqtt), [SmartThings](https://github.com/stjohnjohnson/smartthings-mqtt-bridge), [Vera](https://github.com/jonferreira/vera-mqtt), [HomeKit](https://www.npmjs.com/package/homekit2mqtt), etc.

![HA SwitchPlate Isometric](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/HASwitchPlate_Techpen-Isometric.png?raw=true)![HA SwitchPlate Exploded](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/HASwitchPlate-animation-Explosion.gif?raw=true)

The [Arduino code](Arduino_Sketch) for the ESP8266 provides a generic gateway between [MQTT](https://en.wikipedia.org/wiki/MQTT) and the [Nextion control language](https://www.itead.cc/wiki/Nextion_Instruction_Set).  A basic [Nextion HMI display file](Nextion_HMI) has been included with several pages of various layouts to provide user controls or to present information in response to MQTT messages sent to the device.

## Demo screens
![Scene controller](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/HASwitchPlate_Demo_SceneController.png?raw=true) ![Status display](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/HASwitchPlate_Demo_Status.png?raw=true) ![Media control](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/HASwitchPlate_Demo_Media.png?raw=true) ![3D printer monitor](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/HASwitchPlate_Demo_PrintStatus.png?raw=true) ![Alarm Panel](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/HASwitchPlate_Demo_AlarmPanel.png?raw=true) ![Slider/Dimmer Controls](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/HASwitchPlate_Demo_Dimmers.png?raw=true)

## Bill of Materials
To build a simple version of this project you will only need the [LCD panel]((https://www.itead.cc/nextion-nx3224t024.html)) and the [WeMos D1 Mini]((https://wiki.wemos.cc/products:d1:d1_mini)), 4 jumper wires, and a USB cable to power both devices.

A complete build that's ready to install will require the following components:
* [Nextion 2.4" LCD Touchscreen display](https://www.itead.cc/nextion-nx3224t024.html)
* [WeMos D1 Mini ESP8266 WiFi microcontroller](https://wiki.wemos.cc/products:d1:d1_mini)
* [3D printed switch plate](https://github.com/aderusha/HASwitchPlate/blob/master/3D_Printable_Models/HASwitchPlate_front.stl)
* [3D printed rear cover](https://github.com/aderusha/HASwitchPlate/blob/master/3D_Printable_Models/HASwitchPlate_rear.stl)
* [5V Power supply](https://www.arrow.com/en/products/irm-03-5/mean-well-enterprises)
* [Screw terminal connectors](https://www.amazon.com/gp/product/B011QFLS0S)
* [4x6 protoboard](https://www.amazon.com/gp/product/B06XTPGBS5)
* [Three 20mm M2 flathead screws](https://www.amazon.com/Stainless-Machine-Finish-Phillips-Threads/dp/B000FN3Q94) and [three heat-set threaded inserts](https://www.amazon.com/uxcell-Female-Knurled-Threaded-Embedment/dp/B01IYWTCWW) to fasten things together (feel free to improvise here)

The end result is a highly-customized touchscreen solution for controlling your home, mounted in the wall in a functional and attractive enclosure.

# Getting Started
[Check out the documentation](Documentation/) to get started building your own HA SwitchPlate.
