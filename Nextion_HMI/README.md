# Nextion HMI

The Nextion panel ships with a default demo configuration which we need to overwrite with the included compiled code.  The file [HASwitchPlate.tft](HASwitchPlate.tft) can be saved to a FAT32-formatted microSD card and placed into the panel before power on.  The panel will recognize the code update and load it automatically, after which you're ready to connect to the microcontroller.  Note that some users have reported problems with cards formatted under Linux, so use a Windows system for this process and it should work without trouble.

If you want to manually edit your own panels, [download the editor from Nextion](https://nextion.itead.cc/resource/download/nextion-editor/) and you can flash the panel directly via serial.

Please [check the Nextion HMI documentation](../Documentation/02_Nextion_HMI.md) for additional details.

## Nextion Files Explained

* **[HASwitchPlate.hmi](HASwitchPlate.hmi)** This is the "source" file which you can modify in the [Nextion editor](https://nextion.itead.cc/resource/download/nextion-editor/).  If you want to make your own Nextion HMI, I'd recommend starting with this file, keeping Page 0 (`p0`), then start your own design on pages 1+.
* **[HASwitchPlate.tft](HASwitchPlate.tft)** This is the compiled Nextion firmware for the HASwitchPlate usable on a standard Nextion 2.4" LCD, model `NX3224T024_011R`
* **[HASwitchPlate-Enhanced.tft](HASwitchPlate-Enhanced.tft)** This is the compiled Nextion firmware for the HASwitchPlate usable on an enhanced Nextion 2.4" LCD, model `NX4024K032_011R`.  This panel will not fit in the provided 3D printed enclosure and no enhanced features are used in this project.  **Don't buy this panel**, but if you did (*and you shouldn't*), you can use this firmware.
* **[HASwitchPlate-TJC.hmi](HASwitchPlate-TJC.hmi)** This is the "source" file for the Chinese-market TJC LCD model `TJC3224T024_011`.  This file cannot be used with the english language editor.  If you purchase this panel, you will need to use the Chinese-language "USART HMI" editor to modify this file.  **Don't buy this panel**.
* **[HASwitchPlate-TJC.tft](HASwitchPlate-TJC.tft)** This is the compiled Nextion firmware for the HASwitchPlate usable on a Chinese market TJC 2.4" LCD, model `TJC3224T024_011`.
