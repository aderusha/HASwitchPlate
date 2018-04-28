# Arduino sketch

The first order of business will be to flash the provided Arduino sketch to the ESP8266 with the Arduino IDE.  [Download the IDE for your platform](https://www.arduino.cc/en/Main/Software) and [follow these instructions to add support for the ESP8266 platform](https://github.com/esp8266/Arduino#installing-with-boards-manager).

Next you will need to add several libraries to your Arduino environment.  [Follow this guide for the general process](https://www.arduino.cc/en/Guide/Libraries) and add the following libraries to your IDE:

* [ArduinoJson](https://arduinojson.org/?utm_source=meta&utm_medium=library.properties)
* [MQTT](https://github.com/256dpi/arduino-mqtt)
* [WiFiManager](https://github.com/tzapu/WiFiManager)

To enable future firmware updates you'll need to modify settings in the Arudino IDE for 1M SPIFFs, leaving 3M free for code and updates.  In the Arduino IDE select `Tools` > `Flash Size:` > `4M (1M SPIFFS)`.  If you're using [PlatformIO](https://platformio.org/) instead of Arduino, [modify the build flags](http://docs.platformio.org/en/latest/platforms/espressif8266.html#flash-size) to include `-Wl,-Teagle.flash.4m1m.ld`
![Arduino Erase All Flash Contents](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/Arduino_1M_SPIFFS.png?raw=true)

Flash the sketch to your device, power it up, and connect to the WiFi AP displayed on the LCD panel using the password also displayed on the panel.  You should be prompted to open a [configuration website](http://192.168.4.1) to find your WiFi network, enter WiFi password, and define your MQTT broker.  You can also set an MQTT broker username/password (if required) and define or change the OTA firmware update password.

![WiFi Config 0](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/WiFi_Config_0.png?raw=true) ![WiFi Config 1](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/WiFi_Config_1.png?raw=true) ![WiFi Config 2](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/WiFi_Config_2.png?raw=true)

After the initial Arduino deployment via USB you should be able to upload new firmware through the web admin interface or [using Arduino OTA updates](https://randomnerdtutorials.com/esp8266-ota-updates-with-arduino-ide-over-the-air/).