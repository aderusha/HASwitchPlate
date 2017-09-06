# Nextion UI
The Nextion panel ships with a default demo configuration which we need to overwrite with the included compiled code.  The file [HASwitchPlate.tft](HASwitchPlate.tft) can be saved to a FAT32-formatted microSD card and placed into the panel before power on.  The panel will recognize the code update and load it automatically, after which you're ready to connect to the microcontroller.

If you want to manually edit your own panels, [download the editor from Nextion](https://nextion.itead.cc/resource/download/nextion-editor/) and you can flash the panel directly via serial.

### To-Do:
* Make the slider controls on pages 4 and 5 work
* Figure out some sane means to handle OTA updates to the panel through the ESP8266.  May not be possible with current hardware due to problems with timing and SoftwareSerial.
