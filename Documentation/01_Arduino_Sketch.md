# Arduino sketch

The first order of business will be to flash the provided Arduino sketch to the ESP8266 with the Arduino IDE.  [Download the IDE for your platform](https://www.arduino.cc/en/Main/Software) and [follow these instructions to add support for the ESP8266 platform](https://github.com/esp8266/Arduino#installing-with-boards-manager).

Next you will need to add the PubSubClient library for MQTT.  [Follow this guide for the general process](https://www.arduino.cc/en/Guide/Libraries) and add the 'PubSubClient' from the Library Manager.  Once that is installed you will need to edit the `PubSubClient.h` file and change the line `#define MQTT_MAX_PACKET_SIZE 128` to `#define MQTT_MAX_PACKET_SIZE 512`.  You can find the installed library under the path shown in `File > Preferences > Sketchbook location`.

[At the top of the Arduino sketch are several fields you must modify to fit your environment (WiFi details, MQTT broker IP, node name, etc)](https://github.com/aderusha/HASwitchPlate/blob/master/Arduino_Sketch/HASwitchPlate/HASwitchPlate.ino#L3-L10).  Once those fields have been set you can upload to your microcontroller and start sending commands to the display.

> ## WARNING
> If you will be deploying more than one of these devices you **must** change the node names to be unique.  Failure to do so will result in a cascading series of MQTT connections/disconnections as your devices compete for access to your broker.

After the initial Arduino deployment via USB you should be able to [apply future updates to the ESP8266 using OTA updates](https://randomnerdtutorials.com/esp8266-ota-updates-with-arduino-ide-over-the-air/), allowing you to update code while the device is installed.
