# HA SwitchPlate Documentation

## First, a note of caution

The HA SwitchPlate project requires some basic knowledge of soldering, electronics, Arduino programming, MQTT, and how to safely handle high-voltage AC.  This point bears some repeating so let's try a bigger font to make sure everyone gets it:

# THIS PROJECT INVOLVES MONKEYING WITH HIGH VOLTAGE AND YOU COULD KILL YOURSELF AND/OR SET YOUR HOUSE ON FIRE

The end result is a thing that connects to live voltage and will be placed into your wall for years to come. It will not carry any sort of UL/CE/etc certification.  Proceed with extreme caution.

## With that out of the way, let's get started!

If you have an assembled device and you want to get it connected to Home Assistant, start with **[Step 5 - Home Assistant](05_Home_Assistant.md)**

If you are building your own device from scratch, follow the steps below in order to build and setup your device.  Keep reading after that for some background on how this all actually works.

## Step-by-step Build Instructions

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

----

## Frequently Asked Questions

* **Can I install HASP outside of North America?** While the various power supply options for the HASP electronics should offer a solution which can handle line voltage in most countries, the physical installation of the device probably won't work outside of North America.  There is no international standard for the size and shape of workboxes, so things like screw hole locations and overall mounting dimensions used for the HASP are unlikely to work in any workbox outside of North America.
* **Can I use HASP with \<Some Home Automation System\>**?  If your home automation system can send Nextion commands as MQTT payloads, then yes.  Getting it all to work is going to take a lot of work, but it's possible.
* **Can I run HASP without a neutral wire?** Like most smart devices, a HASP installation will require a neutral wire available in the box you're mounting into.

----

## HASwitchPlate Architecture

Below is an overview of how HASP works under the hood.  It might be helpful to review this information to get a better understanding of the functional components at work.

The complete HA SwitchPlate ("HASP") build consists of 4 physical components:

* Nextion LCD Touchscreen
* ESP8266 WiFi microcontroller
* Power supply and circuit board
* 3D-printed enclosure

In addition, there are three different programming environments involved:

* Nextion Instruction set
* Arduino for ESP8266
* Home Assistant automations (or automation platform of your choice)

## The basics

A home automation system such as Home Assistant can interact with the HASP by sending and receiving MQTT messages to control what appears on the HASP and to respond to user interactions at the HASP.  At its core this project is an Nextion-over-MQTT gateway.  [Nextion Instruction Set](https://nextion.tech/instruction-set/) commands are sent to the HASP via an MQTT message and delivered to the Nextion LCD.  User interactions triggered from the LCD (such as button presses) are sent back out from HASP via MQTT messages to be acted upon by your home automation system.

### Example: one screen button

To illustrate, let's discuss the case of a single button shown on the screen.  Below we see  "page 1", showing a set of 4 buttons arranged vertically and a set of 3 page select buttons at the bottom.

![Scene Controls](Images/HASwitchPlate_Demo_SceneController.png?raw=true)

Referring to the [Nextion Object Reference](02_Nextion_HMI.md#hasp-nextion-object-reference) (more on this later), we find that the top-most button above is an object named `p[1].b[4]`.  When the HASP device shown here powered on, the home automation controller sent an MQTT message that looked like this:

| topic                                | message              |
|--------------------------------------|----------------------|
| `hasp/plate01/command/p[1].b[4].txt` | `"     Lights On "` |

Let's take a look at this `command` message:

`hasp` All messages to and from the HASP appear under the `hasp/` namespace in MQTT

`plate01` This device is named `plate01`.  You can have several HASP devices in your home and each gets their own name.

`command` This message is a `command` being sent *to* the HASP device.

`p[1].b[4]` is the name of a Nextion "object".  A full discussion of these names and a map to the objects included in the HASP project can be found [here](02_Nextion_HMI.md#hasp-nextion-object-reference).

`.txt` is a property of the object named above.  In this case, `.txt` is a reference to the text which appears on this object.

`"     Lights On "` Is the text we wish to appear on the screen.  The `` part of this text is a [FontAwesome icon codepoint](https://fontawesome.com/cheatsheet), which here happens to be a lightbulb.  We've mixed the icon and the text to be displayed in this message.

Put together, we've told the HASP named `plate01` to send a Nextion instruction `p[1].b[4].txt="     Lights On "`.  The HASP sent this command to the Nextion LCD, and the result is the text you see on the screenshot above.

### User Interaction

Now that we have some nice text appearing on the screen, let's see what happens when the user interacts with our button.  When the `Lights On` button is pressed, the Nextion LCD sends a command to the HASP device, which publishes the following MQTT message:

| topic                          | message |
|--------------------------------|---------|
| `hasp/plate01/state/p[1].b[4]` | `ON`    |

Let's take a look at this `state` message:

`hasp` Just as before, all messages to and from the HASP appear under the `hasp/` namespace

`plate01` This message came from a device named `plate01`.

`state` This message is a `state` update sent *from* the HASP device.

`p[1].b[4]` is the Nextion object which triggered the new `state`

`ON` This messages tells us that the object above has entered the `ON` state.  In the case of a button object, this means somebody has pressed the button.  An `OFF` message will be sent later when the user releases the button.

### Responding to User Interaction

Now it is up to your home automation system to pick up this message and doing something meaningful in response, like turning on the lights.  In this project, a set of [Home Assistant automations](https://www.home-assistant.io/docs/automation/) has been provided to demonstrate various interactions which you can use as a starting point for customizing your own environment.

[This is an example Home Assistant automation which will listen for HASP button presses](../Home_Assistant/packages/plate01/hasp_plate01_p1_scenes.yaml#L45-L53):

```yaml
# Trigger scene.lights_on when p[1].b[4] pressed
- alias: hasp_plate01_p1_SceneButton4
  trigger:
    - platform: mqtt
      topic: "hasp/plate01/state/p[1].b[4]"
      payload: "ON"
  action:
    - service: scene.turn_on
      entity_id: scene.lights_on
```

This automation tells Home Assistant to listen to the MQTT topic `hasp/plate01/state/p[1].b[4]` for a message with the payload `ON`.  As we learned above, this is the message that will be sent out when a user presses the button which we've applied the text label "Lights on" (with a nice icon).  When Home Assistant receives this button press, we trigger a [Home Assistant scene](https://www.home-assistant.io/docs/scene/) called `lights_on`.

And that's it!  All of the HASP interactions follow some version of this basic workflow.  The device is configured to display things by receiving MQTT messages.  When the user does things at the HASP device, MQTT messages are sent back for your home automation platform to respond to.

Of course, things can get complicated.  The included automations make extensive use of [templates](https://www.home-assistant.io/docs/automation/templating/) and other advanced features of Home Assistant.  Don't be worried though, underneath it all it's always going to be some version of the simple interaction shown above.
