# Arduino sketch

The first order of business will be to flash the provided Arduino sketch to the ESP8266 with the Arduino IDE.  [Download the IDE for your platform](https://www.arduino.cc/en/Main/Software) and [follow these instructions to add support for the ESP8266 platform](https://github.com/esp8266/Arduino#installing-with-boards-manager).

Next you will need to add several libraries to your Arduino environment.  [Follow this guide for the general process](https://www.arduino.cc/en/Guide/Libraries) and add the following libraries to your IDE:

* [ArduinoJson](https://arduinojson.org/?utm_source=meta&utm_medium=library.properties)
* [MQTT](https://github.com/256dpi/arduino-mqtt)
* [WiFiManager](https://github.com/tzapu/WiFiManager)

Flash the sketch to your device, power it up, and connect to the WiFi AP displayed on the LCD panel using the password also displayed on the panel.  You should be prompted to open a [configuration website](http://192.168.4.1) to find your WiFi network, enter WiFi password, and define your MQTT broker.  You can also set an MQTT broker username/password (if required) and define or change the OTA firmware update password (see below)

After the initial Arduino deployment via USB you should be able to [apply future updates to the ESP8266 using OTA updates](https://randomnerdtutorials.com/esp8266-ota-updates-with-arduino-ide-over-the-air/), allowing you to update code while the device is installed.

# Upgrade from earlier versions

If you're re-flashing over an earlier (pre-v0.2.0) release, be sure to clear out any saved flash contents from the system when you first flash the image.  Select `Tools` > `Erase Flash`> `All Flash Contents` before uploading the software to your device.

![Arduino Erase All Flash Contents](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/Arduino_Erase_All_Flash_Contents.png?raw=true)