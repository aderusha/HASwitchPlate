# HA SwitchPlate Documentation

## First, a note of caution

The HA SwitchPlate project requires some basic knowledge of soldering, electronics, Arduino programming, MQTT, and how to safely handle high-voltage AC.  This point bears some repeating so let's try a bigger font to make sure everyone gets it:

# THIS PROJECT INVOLVES MONKEYING WITH HIGH VOLTAGE AND YOU COULD KILL YOURSELF AND/OR SET YOUR HOUSE ON FIRE

The end result is a thing that connects to live voltage and will be placed into your wall for years to come. It will not carry any sort of UL/CE/etc certification.  Proceed with extreme caution.

## With that out of the way, let's get started!

If you want to jump right in, follow the steps below in order to build and setup your device.  Keep reading after that for some background on how this all actually works.

## Step-by-step Instructions

### Step 1 - [Arduino Sketch](01_Arduino_Sketch.md)

Get started with deploying your code to the ESP8266 and getting it connected to WiFi and your MQTT broker.

### Step 2 - [Nextion HMI](02_Nextion_HMI.md)

Next you'll want to flash the Nextion touchscreen with the provided firmware.

### Step 3 - [Electronics Assembly](03_Electronics_Assembly.md)

Now we can hook the ESP8266 to the Nextion HMI and provide power to both.

### Step 4 - [Project Enclosure](04_Project_Enclosure.md)

Time to move from the breadboard into a nice new home suitable for mounting in your wall.

### Step 5 - [Home Assistant](05_Home_Assistant.md)

Utilize the provided Home Assistant automations to control and interact with the panel.

### Step 6 - [MQTT Control](06_MQTT_Control.md)

Time to remove the training wheels and learn to send commands to and from the panel via MQTT.

## HASwitchPlate Architecture

The complete HA SwitchPlate build consists of 4 physical components:

* Nextion LCD Touchscreen
* ESP8266 WiFi microcontroller
* Power supply and circuit board
* 3D-printed enclosure

In addition, there are three different programming environments involved:

* Nextion Instruction set
* Arduino for ESP8266
* Home Assistant automations (or automation platform of your choice)

### The basics

At its core this project is an MQTT-to-Nextion gateway, allowing the user to send commands to and receive data from the Nextion LCD panel over a WiFi connection.  Example code for each of the three components above is provided as a starting point for your own development.  Each of these three components are capable of being extended to handle your own use case and each environment comes with it's own considerations so we'll take a moment to review some specifics.

### Nextion Instruction Set

The Nextion panels mate an LCD touchscreen with an embedded microcontroller which is controlled over a standard serial interface.  The panel needs to be flashed with a firmware image generated in a Windows GUI designer application. The GUI designer can embed control instructions into the firmware image allowing the panel to run basic interaction tasks on its own.  The panel accepts instructions over the serial interface to change visual elements and will send serial data in response to user interactions.  Finally, graphical resources (pictures and fonts) can be flashed into the device to create a custom user interface.

The Nextion environment is centered around "pages", each of which have their own graphical elements and controls.  The provided Nextion firmware creates a very basic user interface with few hard-coded instructions.  Several pages have been created in various layouts, each of which can be customized by sending commands to the panel over MQTT.  This approach allows for external control over the panel interface and interactions in a generic way.

### Nextion Considerations

* Nextion instructions executed locally on the panel are fast and always available, even if the other components are offline for whichever reason.  User interaction code happening on the panel will create the most responsive experience from a user perspective.
* Updating firmware on the panel over the air carries some risk of failure.  Recovery requires rebooting the panel, which may involve a trip to the circuit break panel.
* The Nextion command set is very basic and memory is limited.  Don't expect to be accomplish much of your automation logic at this layer.
* The provided pages utilize no bitmap graphical resources, resulting in a very "flat" UI which is general-purpose and easy to modify over the wire.  Feel free to add your own images and customize the panel for your specific use case!

## Arduino IDE

The ESP8266 microcontroller physically attaches to the Nextion LCD via serial, connects to your network via WiFi, and connects to an MQTT broker to gateway Nextion control messages.  For the most part the provided code has been designed to gateway messages back and forth without a lot of automation logic happening at this layer.

### Arduino Considerations

* The provided Arduino sketch supports OTA programming allowing for remote code updates without involving a screwdriver and circuit breaker.  This process is reasonably reliable, but if you flash an update that leaves the device unresponsive you'll need to unplug the device from AC and re-flash via USB.
* The Arduino platform and the C++ programming language is a powerful and flexible environment allowing for nearly any degree of automation you might dream up.
* The ESP8266 does not include a realtime clock, so any logic dependent upon time-of-day will be problematic to implement at this layer.
* The ESP8266 provides a watchdog process which will reset the device if anything hangs up.  This makes the device reliable in the face of network outages, but also means that the system may restart (and thus lose state) unexpectedly.  Clever use of local EEPROM or retained MQTT messages may help with this.

## Home Assistant (or other HA platforms)

The ultimate goal of this project is to allow your home automation platform to control the LCD panel by way of MQTT messages.  Home Assistant provides a powerful option for device communication and automation, but any platform capable of sending/receiving MQTT messages could be substituted in its place.

### Home Assistant Considerations

* For purposes of this project I've elected to utilize Home Assistant automations as they are reasonably accessible to new users.  While flexible, they also can be functionally limiting and hard to manage at scale.
* More advanced users may chose to utilize Python scripts or any other available external scripting/programming language that can be executed on your home automation platform of choice.
* User interactions controlled at this layer will have the highest latency, and any broken link in the command chain (MQTT, WiFi, etc) will prevent the panel from being able to respond to commands happening here.
