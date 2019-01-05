# MQTT Control

## MQTT

[MQTT](https://mqtt.org/faq) is a standard IoT protocol in widespread use everywhere from small home automation projects to global industrial IoT environments.  Think of it as a "twitter for devices" - it works in very much the same way.  MQTT is a "publish/subscribe" message bus, where attached devices can publish `messages` on `topics`, and other devices can subscribe to those `topics` to receive the published message.  MQTT requires a central MQTT server we call a "broker".  Built-in MQTT broker services are available in [Home Assistant](https://www.home-assistant.io/docs/mqtt/broker/) and [OpenHAB](https://www.openhab.org/addons/bindings/mqtt/) or you can run your own standalone instance.  The [Mosquitto](https://mosquitto.org/) MQTT broker is a common open source solution and is probably a good place to start for home use.

At it's core, the HASP project acts as a gateway between MQTT and the Nextion LCD.  It will "subscribe" to MQTT messages sent by your home automation software and forward the contents of those messages to the Nextion LCD.  Interactions from the LCD, such as a user pressing a button, are sent back to your home automation system as published MQTT messages.  In order to make use of the HASP project to its full potential, you'll need to understand how the Nextion LCD sends and receives commands, and how to make your home automation system interact with those commands over MQTT.

## Nextion Instructions

A [detailed guide to the Nextion instruction set can be found here](https://nextion.itead.cc/resources/documents/instruction-set/).  [A mostly-complete list of all available instructions and their use is available here](https://www.itead.cc/wiki/Nextion_Instruction_Set).  These Nextion instructions are then sent as MQTT messages per the [`MQTT Namespace`](#mqtt-namespace) outlined below.  A common issue people encounter centers around the use of quotes in the Nextion instruction set.  As a general rule, attributes which accept an numeric value cannot have quotes, all other attributes must be enclosed in double quotes.

## Nextion Page and Object IDs

Objects are referenced by page number (not page name) and object ID (not object name) as shown in the Nextion editor.  The Nextion notation for each object is of the form `p[1].b[2]` meaning page number 1, object number 2.  Confusingly, button objects will be named starting at b0 but it might be object 2, or 7, etc. Other objects will be named similarly with different letters.  For example, the first text field on the page will be automatically named t0.  These names have nothing to do with the object ID, and all objects regardless of type are still referenced as `p[<page number>].b[<object id>]`

![Page and Object IDs](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/Nextion_Editor_Page_and_Object_Ids.png?raw=true)

While it is possible to send `command` messages using the object or page names, all messages coming back from the panel under `state` will be using page numbers and object IDs.  Save yourself the confusion and do everything in the format `p[<page number>].b[<object id>]` and things will go easier.

## MQTT Message Examples

With the information above we can now take a look at a some example MQTT  transaction.  To begin, let's customize the text appearing on a button on page 1.  The topmost button on page one is `p[1].b[4]`.  If we want to set the text on that button to read `HASP` we could send the following MQTT message:

> topic: `hasp/plate01/command/p[1].b[4].txt`  
> message: `"HASP"`

The HASP device named `plate01` will be subscribed to the topic `hasp/plate01/command`.  When it sees this message, it will send the Nextion LCD the command `p[1].b[4]="HASP"` which will update the text on our button.

Now let's try one going the other way.  When a user presses that button, HASP is going to publish an MQTT message that looks like this:

> topic: `hasp/plate01/state/p[1].b[4]`  
> message: `ON`  

Your home automation system will be subscribed to `hasp/plate01/state`.  When the message example above is published, your home automation system will know that somebody pressed button `p[1].b[4]` on a HASP device named `plate01` and can take appropriate action in response.

## MQTT Namespace

By default the device will subscribe to `hasp/<node_name>/command/#` to accept incoming commands.  The device will also subscribe to `hasp/<group_name>/command/#` to accept incoming commands aimed at a group of devices.

There are two subtopics to send commands to and receive messages from the panel:

* `command` will send commands or set attribute of the display, such as button text or screen dim. The specific attribute must be appended as a subtopic, with the value to be set delivered as the payload.  For example, the following command will set the text displayed on page 1/button 4 to "Lamp On": `mosquitto_pub -h mqtt -t hasp/plate01/command/p[1].b[4].txt -m '"Lamp On"'`
* `state` topics will be sent by the panel in response to local user interactions or received commands which provide output in return.  For example, a user pressing button 4 on page 1 will cause the panel to publish a message: `'hasp/plate01/state/p[1].b[4]' 'ON'`

## `command` Syntax

Messages sent to the panel under the `command` topic will be handled based on the following rules:

* **`-t 'hasp/plate01/command' -m 'dim=50'`** A `command` with no subtopic will send the command in the payload to the panel directly.
* **`-t 'hasp/plate01/command/page' -m '1'`** The `page` command subtopic will set the current page on the device to the page number included in the payload.
* **`-t 'hasp/plate01/command/json' -m '["dim=50", "page 1"]'`** The `json` command subtopic will send a JSON array of commands one-by-one to the panel.
* **`-t 'hasp/plate01/command/p[1].b[4].txt' -m '"Lamp On"'`** A `command` with a subtopic will set the attribute named in the subtopic to the value sent in the payload.
* **`-t 'hasp/plate01/command/p[1].b[4].txt' -m ''`** A `command` with a subtopic and an empty payload will request the current value of the attribute named in the subtopic from the panel.  The value will be returned under the `state` topic as `'hasp/plate01/state/p[1].b[4].txt' -m '"Lamp On"'`
* **`-t 'hasp/plate01/command/statusupdate'`** `statusupdate` will publish a JSON string indicating system status.
* **`-t 'hasp/plate01/command/reboot'`** The `reboot` command will reboot the HASP device.
* **`-t 'hasp/plate01/command/factoryreset'`** The `factoryreset` command will wipe out saved WiFi, nodename, and MQTT broker details to reset the device back to default settings.
* **`-t 'hasp/plate01/command/lcdupdate'`** The `lcdupdate` command subtopic with no message will attempt to update the Nextion from the HASP GitHub repository.
* **`-t 'hasp/plate01/command/lcdupdate' -m 'http://192.168.0.10:8123/local/HASwitchPlate.tft'`** The `lcdupdate` command subtopic attempts to update the Nextion from the HTTP URL named in the payload.
* **`-t 'hasp/plate01/command/espupdate'`** The `espupdate` command subtopic with no message will attempt to update the ESP8266 from the HASP GitHub repository.
* **`-t 'hasp/plate01/command/espupdate' -m 'http://192.168.0.10/local/HASwitchPlate.ino.d1_mini.bin'`** The `espdupdate` command subtopic attempts to update the ESP8266 from the HTTP URL named in the payload.

In each of those commands, you can substitute the `<node_name>` for the `<group_name>` if you want to target all devices in a group.

### MQTT Error codes (rc=n)

If the HASP cannot connect to MQTT it will display a return code on the screen as RC=_n_.  These codes are specified by the MQTT spec [here](http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Table_3.1_-).

| Value | Return Code Response                                   | Description                                                                        |
|-------|--------------------------------------------------------|------------------------------------------------------------------------------------|
| 0     | 0x00 Connection Accepted                               | Connection accepted                                                                |
| 1     | 0x01 Connection Refused, unacceptable protocol version | The Server does not support the level of the MQTT protocol requested by the Client |
| 2     | 0x02 Connection Refused, identifier rejected           | The Client identifier is correct UTF-8 but not allowed by the Server               |
| 3     | 0x03 Connection Refused, Server unavailable            | The Network Connection has been made but the MQTT service is unavailable           |
| 4     | 0x04 Connection Refused, bad user name or password     | The data in the user name or password is malformed                                 |
| 5     | 0x05 Connection Refused, not authorized                | The Client is not authorized to connect                                            |
| 6-255 |                                                        | Reserved for future use                                                            |