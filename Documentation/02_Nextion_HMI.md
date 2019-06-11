# Nextion HMI

## Basic first-time use

For most users the deployment of the Nextion firmware is straightforward - simply copy [the compiled firmware image](../Nextion_HMI/HASwitchPlate.tft) to a FAT32-formatted microSD card, insert the microSD card into the Nextion LCD, and then apply 5VDC to the power pins on the panel.  It should power up the panel, recognize the .TFT file, and update the panel automatically.  Once the firmware update has completed you can remove power and eject the microSD card.

Compiled TFT files are included for the [Basic](https://github.com/aderusha/HASwitchPlate/raw/master/Nextion_HMI/HASwitchPlate.tft) and [Enhanced](https://github.com/aderusha/HASwitchPlate/raw/master/Nextion_HMI/HASwitchPlate-Enhanced.tft) versions of the panel.  This project does not currently utilize any features offered in the Enhanced panel, and some users have reported the Enhanced device does not fit in the provided enclosure.  **It is strongly recommended that you do not use the enhanced display**.

Once the project is assembled, future updates to the Nextion firmware can be handled over-the-air by utilizing the built-in web interface or by [issuing an MQTT command](06_MQTT_Control.md#command-syntax) with a URL to the target TFT file.

With the Nextion firmware flashed you can proceed to the [Electronics Assembly section](03_Electronics_Assembly.md) to test the assembled system.

---

## Nextion Editor

For advanced customization you will need to download the (Windows-only) [Nextion editor](https://nextion.itead.cc/resource/download/nextion-editor/).  You can find instructions on its use [here](https://www.itead.cc/blog/nextion-editor-a-basic-introduction).

If you want to edit the existing HASP interface, you will likely need to delete some pages in order to add your own work as the project consumes nearly the entire memory space of the Nextion basic panel.  If you'd like to start a new interface from the ground up, copy over the existing Page 0 page as the ESP8266 firmware makes use of page 0 for user interactions and WiFi setup.  The Home Assistant automations will probably need to be changed if any major modifications are made to the HMI.

## Nextion Instruction Set

The Nextion accepts and sends commands over the serial interface.  A detailed guide to the Nextion instruction set can be found [here](https://nextion.itead.cc/resources/documents/instruction-set/).  A mostly-complete list of all available instructions and their use is available [here](https://www.itead.cc/wiki/Nextion_Instruction_Set).

## Nextion color codes

The Nextion environment utilizes RGB 565 encoding.  [Use this handy convertor](https://nodtem66.github.io/nextion-hmi-color-convert/index.html) to select your colors and convert to the RGB 565 format.

## HASP Nextion Object Reference

### Page 0

![Page 0](Images/NextionUI_p0_Init_Screen.png?raw=true)

### Pages 1-3

![Pages 1-3](Images/NextionUI_p1-p3_4buttons.png?raw=true)

### Pages 4-5

![Pages 4-5](Images/NextionUI_p4-p5_3sliders.png?raw=true)

### Page 6

![Page 6](Images/NextionUI_p6_8buttons.png?raw=true)

### Page 7

![Page 7](Images/NextionUI_p7_12buttons.png?raw=true)

### Page 8

![Page 8](Images/NextionUI_p8_5buttons+1slider.png?raw=true)

### Page 9

![Page 9](Images/NextionUI_p9_2buttons+graph.png?raw=true)

## HASP Default Fonts

The Nextion display natively supports monospaced fonts.  The HASP HMI includes the Consolas font in 4 sizes and the [Webdings font](https://en.wikipedia.org/wiki/Webdings#Character_set) in 1 size.

| Number | Font              | Max characters per line | Max lines per button |
|--------|-------------------|-------------------------|----------------------|
| 0      | Consolas 24 point | 20 characters           | 2 lines              |
| 1      | Consolas 32 point | 15 characters           | 2 lines              |
| 2      | Consolas 48 point | 10 characters           | 1 lines              |
| 3      | Consolas 80 point | 6 characters            | 1 lines              |
| 4      | Webdings 56 point | 8 characters            | 1 lines              |

## HOW TO: Run this software with an ESP8266 only

One feature of the Nextion Editor is the [Nextion Simulator](https://www.itead.cc/wiki/Nextion_Editor_Quick_Start_Guide#Debug.2C_online_simulator), which allows the user to debug an HMI being edited.  You've probably used this if you've worked on editing your own HMI file.  You can run through the screens using your mouse to issue touch commands and feed it commands and see output in the text boxes provided.

The Nextion simulator allows you to run [hardware-in-loop](https://en.wikipedia.org/wiki/Hardware-in-the-loop_simulation) with your microcontroller.  If you have a second USB UART kicking around (you could use a second WeMos or Arduino if you have one handy), connect the UART RX to pin D4 and the TX to pin D7 which are the pins you'd use to hook up a real panel.  In the Nextion Simulator you can then select "User MCU Input", select your UARTs COM port, set the Baud to 115200, then click Start.

Now the Simulator will accept input from and send output to your flashed ESP8266 without having a Nextion on hand!

![Nextion Editor Simulator](Images/Nextion_Editor_Simulator.png?raw=true)
