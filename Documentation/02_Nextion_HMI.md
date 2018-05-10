# Nextion HMI

For basic use the deployment of the Nextion firmware is straightforward - simply copy the compiled firmware image to a FAT32-formatted microSD card and apply 5VDC to the power pins on the panel with the card inserted.  It should power up the panel, recognize the .TFT file, and update the panel automatically.  Once the firmware update has completed you can eject the card and restart the device.

For advanced customization you will need to download the (Windows-only) [Nextion editor](https://nextion.itead.cc/resource/download/nextion-editor/).  You can find instructions on its use [here](https://www.itead.cc/blog/nextion-editor-a-basic-introduction).

The Nextion accepts and sends commands over the serial interface.  A [detailed guide to the Nextion instruction set can be found here](https://nextion.itead.cc/resources/documents/instruction-set/).  [A mostly-complete list of all available instructions and their use is available here](https://www.itead.cc/wiki/Nextion_Instruction_Set).

Two compiled TFT files are included for the [Basic](https://github.com/aderusha/HASwitchPlate/raw/master/Nextion_HMI/HASwitchPlate.tft) and [Enhanced](https://github.com/aderusha/HASwitchPlate/raw/master/Nextion_HMI/HASwitchPlate-Enhanced.tft) versions of the panel.  This project does not currently utilize any features offered in the Enhanced panel.

Once the project is assembled, future updates to the Nextion firmware can be handled over-the-air by [issuing an MQTT command](06_MQTT_Control.md#command-syntax) with a URL to the target TFT file.

## How to: Run this software with an ESP8266 only

One feature of the Nextion Editor is the [Nextion Simulator](https://www.itead.cc/wiki/Nextion_Editor_Quick_Start_Guide#Debug.2C_online_simulator), which allows the user to debug an HMI being edited.  You've probably used this if you've worked on editing your own HMI file.  You can run through the screens using your mouse to issue touch commands and feed it commands and see output in the text boxes provided.

The Nextion simulator allows you to run [hardware-in-loop](https://en.wikipedia.org/wiki/Hardware-in-the-loop_simulation) with your microcontroller.  If you have a second USB UART kicking around (you could use a second WeMos or Arduino if you have one handy), connect the UART RX to pin D4 and the TX to pin D7 which are the pins you'd use to hook up a real panel.  In the Nextion Simulator you can then select "User MCU Input", select your UARTs COM port, set the Baud to 115200, then click Start.

Now the Simulator will accept input from and send output to your flashed ESP8266 without having a Nextion on hand!

![Nextion Editor Simulator](Images/Nextion_Editor_Simulator.png?raw=true)