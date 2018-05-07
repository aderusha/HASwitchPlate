# Nextion HMI

The Nextion panel ships with a default demo configuration which we need to overwrite with the included compiled code.  The file [HASwitchPlate.tft](HASwitchPlate.tft) can be saved to a FAT32-formatted microSD card and placed into the panel before power on.  The panel will recognize the code update and load it automatically, after which you're ready to connect to the microcontroller.  Note that some users have reported problems with cards formatted under Linux, so use a Windows system for this process and it should work without trouble.

If you want to manually edit your own panels, [download the editor from Nextion](https://nextion.itead.cc/resource/download/nextion-editor/) and you can flash the panel directly via serial.

Please [check the Nextion HMI documentation](../Documentation/02_Nextion_HMI.md) for additional details.