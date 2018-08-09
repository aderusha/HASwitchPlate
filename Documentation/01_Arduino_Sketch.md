# Flash firmware to ESP8266

The first order of business will be to flash the provided firmware to the ESP8266.  You can do this using the [NodeMCU Flasher](#nodemcu-flasher) (easy, Windows-only) or the [Arduino IDE](#arduino-ide) (less easy, but cross-platform).  You should only need to run through this process once as future firmware updates can be applied through the provided web interface.

## NodeMCU Flasher

If you're running Windows and you just want to get started without dealing with Arduino this is probably going to be your best option.

### Required Downloads

* NodeMCU Flasher: [Win32](https://github.com/nodemcu/nodemcu-flasher/raw/master/Win32/Release/ESP8266Flasher.exe) / [Win64](https://github.com/nodemcu/nodemcu-flasher/raw/master/Win64/Release/ESP8266Flasher.exe)
* [HASwitchPlate Firmware](https://github.com/aderusha/HASwitchPlate/raw/master/Arduino_Sketch/HASwitchPlate.ino.d1_mini.bin)

### NodeMCU Flasher Process

* Plug your WeMos D1 into an available USB port on your PC
* Launch the NodeMCU flasher and select the COM port for your device from the `COM Port` drop-down menu
* Select the `Config` tab<sup>(1)</sup>, then click the top-most gear icon to the right<sup>(2)</sup> to open a file browser. ![NodeMCU_Flasher_OpenFirmware](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/NodeMCU_Flasher_OpenFirmware.png?raw=true)
* Navigate to the HASP firmware image you downloaded and click `Open` to select it
* Switch back to the `Operation` tab and click `Flash(F)`

Now proceed to the [Nextion HMI](02_Nextion_HMI.md) document to flash firmware to the LCD.

---

## Arduino IDE

[Download the IDE for your platform](https://www.arduino.cc/en/Main/Software) and [follow these instructions to add support for the ESP8266 platform](https://github.com/esp8266/Arduino#installing-with-boards-manager).

### Required Libraries

Next you will need to add several libraries to your Arduino environment.  [Follow this guide for the general process](https://www.arduino.cc/en/Guide/Libraries) and add the following libraries to your IDE:

* [ArduinoJson](https://arduinojson.org/?utm_source=meta&utm_medium=library.properties)
* [MQTT](https://github.com/256dpi/arduino-mqtt)
* [WiFiManager](https://github.com/tzapu/WiFiManager)

To enable future firmware updates you'll need to modify settings in the Arudino IDE for 1M SPIFFs, leaving 3M free for code and updates.  In the Arduino IDE select `Tools` > `Flash Size:` > `4M (1M SPIFFS)`.  If you're using [PlatformIO](https://platformio.org/) instead of Arduino, [modify the build flags](http://docs.platformio.org/en/latest/platforms/espressif8266.html#flash-size) to include `-Wl,-Teagle.flash.4m1m.ld`
![Arduino Erase All Flash Contents](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/Arduino_1M_SPIFFS.png?raw=true)

## Firmware updates

After the initial firmware deployment you should be able to upload new firmware through the web admin interface or [using Arduino OTA updates](https://randomnerdtutorials.com/esp8266-ota-updates-with-arduino-ide-over-the-air/) without connecting to your device via USB.