# Nextion HMI

For basic use the deployment of the Nextion HMI firmware is straightforward - simply copy the compiled firmware image to a FAT32-formatted microSD card and apply 5VDC to the power pins on the panel with the card inserted.  It should power up the panel, recognize the .TFT file, and update the panel automatically.  Once the firmware update has completed you can eject the card and restart the device.

For advanced customization you will need to download the (Windows-only) [Nextion editor ](https://nextion.itead.cc/resource/download/nextion-editor/).  You can find instructions on its use [here](https://www.itead.cc/blog/nextion-editor-a-basic-introduction).

The Nextion panel accepts and sends commands over the serial interface.  A [detailed guide to the Nextion control language can be found here](https://www.itead.cc/wiki/Nextion_Instruction_Set).
