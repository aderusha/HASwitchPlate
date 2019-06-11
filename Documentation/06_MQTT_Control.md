# MQTT Control

## MQTT

[MQTT](https://mqtt.org/faq) is a standard IoT protocol in widespread use everywhere from small home automation projects to global industrial IoT environments.  Think of it as a "twitter for devices" - it works in very much the same way.  MQTT is a "publish/subscribe" message bus, where attached devices can publish `messages` on `topics`, and other devices can subscribe to those `topics` to receive the published message.  MQTT requires a central MQTT server we call a "broker".  Built-in MQTT broker services are available in [Home Assistant](https://www.home-assistant.io/docs/mqtt/broker/) and [OpenHAB](https://www.openhab.org/addons/bindings/mqtt/) or you can run your own standalone instance.  The [Mosquitto](https://mosquitto.org/) MQTT broker is a common open source solution and is probably a good place to start for home use.

At its core, the HASP project acts as a gateway between MQTT and the Nextion LCD.  It will "subscribe" to MQTT messages sent by your home automation software and forward the contents of those messages to the Nextion LCD.  Interactions from the LCD, such as a user pressing a button, are sent back to your home automation system as published MQTT messages.

In order to make use of the HASP project to its full potential, you'll need to understand how the Nextion LCD sends and receives commands, and how to make your home automation system interact with those commands over MQTT.

## Nextion Instructions

A detailed guide to the Nextion instruction set can be found [here](https://nextion.itead.cc/resources/documents/instruction-set/).  A mostly-complete list of all available instructions and their use is available [here](https://www.itead.cc/wiki/Nextion_Instruction_Set).  These Nextion instructions are sent as MQTT messages per the [`MQTT Namespace`](#mqtt-namespace) outlined below.

A common issue people encounter centers around the use of quotes in the Nextion instruction set.  As a general rule, attributes which accept only numbers do not have quotes, and all other values must be enclosed in double quotes.

## Nextion Page and Object IDs

Objects are referenced by page number (*not page name*) and object ID (*not object name*) as shown in the Nextion editor.  The Nextion notation for each object is of the form `p[1].b[2]` meaning page number 1, object ID 2.  Confusingly, button object *names* will start at `b0` but it might be object ID 2, or 7, etc. Other objects will be named similarly with different letters.  For example, the first text field on the page will be automatically named `t0`.  **Ignore these names**.  They have nothing to do with the object ID, and all objects regardless of type are still referenced as `p[<page number>].b[<object id>]`

![Page and Object IDs](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/Nextion_Editor_Page_and_Object_Ids.png?raw=true)

Screenshots of each object number on each page of the HASP project can be found in the [Nextion HMI documentation section](02_Nextion_HMI.md#hasp-nextion-object-reference).

An object will have multiple attributes, some of which can be changed.  For example, a button named `p[1].b[2]` may have a "txt" attribute which we'd refer to as `p[1].b[2].txt`.  Not all attributes can be changed at runtime, the ones which can are shown in a green font in the Nextion editor (see the right pane of the image above.)

## MQTT Message Examples

With the information above we can now take a look at a some example MQTT  transactions.  To begin, let's customize the text appearing on a button.  The [topmost button on page one](02_Nextion_HMI.md#pages-1-3) is `p[1].b[4]`.  If we want to set the text on that button to read `HASP` we could send the following MQTT message:

> topic: `hasp/plate01/command/p[1].b[4].txt`  
> message: `"HASP"`

The HASP device named `plate01` will be subscribed to the topic `hasp/plate01/command`.  When it sees this message, it will send the Nextion LCD the command `p[1].b[4].txt="HASP"` which will update the text on our button.

Now let's try one going the other way.  When a user presses that button, HASP is going to publish an MQTT message that looks like this:

> topic: `hasp/plate01/state/p[1].b[4]`  
> message: `ON`  

Your home automation system will be subscribed to `hasp/plate01/state`.  When the message example above is published, your home automation system will know that somebody pressed button `p[1].b[4]` on a HASP device named `plate01` and can take appropriate action in response.

## Home Assistant Automation Example

The HASP project includes [a number of Home Assistant automations](../Home_Assistant) to get you up and running.  To make full use of your HASP you will want to customize these automations to suit your own needs.  The example automations provided are broken down into pages, and the ideas presented build on each other as you work through each page in numerical order.  If you're looking to understand how this works, [start with page 1](../Home_Assistant/packages/plate01/hasp_plate01_p1_scenes.yaml) and work your way up through the rest.

Let's take a look at an automation example to handle the `p[1].b[4]` button we were just working with.  The automations discussed here will be specific to Home Assistant but the concepts should apply to any home automation platform.

### Send commands from Home Assistant to the HASP

[The first page 1 automation](../Home_Assistant/packages/plate01/hasp_plate01_p1_scenes.yaml#L6-L21), like most Home Assistant automations, begins with a [trigger](https://www.home-assistant.io/docs/automation/trigger/).  Ours looks like this:

```yaml
  - alias: hasp_plate01_p1_ScenesInit
    trigger:
    - platform: state
      entity_id: 'binary_sensor.plate01_connected'
      to: 'on'
    - platform: homeassistant
      event: start
```

This trigger will fire whenever the HASP device `plate01` connects to Home Assistant, or whenever Home Assistant starts.  When the HASP starts up it has no text on any buttons, so the first thing we'll do is start sending commands to tell the HASP what we want our buttons to say.  Let's look at the [action](https://www.home-assistant.io/docs/automation/action/) section:

```yaml
    action:
    - service: mqtt.publish
      data:
        topic: 'hasp/plate01/command/p[1].b[4].font'
        payload: '2'
    - service: mqtt.publish
      data:
        topic: 'hasp/plate01/command/p[1].b[4].txt'
        payload: '"Lights On"'
```

The first action tells Home Assistant to publish the message `2` on the topic `hasp/plate01/command/p[1].b[4].font`.  This command sets the font for our button to [font number 2](02_Nextion_HMI.md#hasp-default-fonts) which "Consolas 48 point".  That font allows us to fit 10 characters into a standard-sized button.

Our next action tells Home Assistant to publish the message `"Lights On"` on the topic `hasp/plate01/command/p[1].b[4].txt`.  Note the use of quotes here, as we're sending text instead of a numeric value.  This command will program button `p[1].b[4]` to show the text `Lights On`.

If you would like this button to say something else, you can make that change now and restart Home Assistant to see what happens.

### Send commands from HASP to Home Assistant

[The second page 1 automation](../Home_Assistant/packages/plate01/hasp_plate01_p1_scenes.yaml#L47-L55) in our example looks like this:

```yaml
# Trigger scene.lights_on when p[1].b[4] pressed
  - alias: hasp_plate01_p1_SceneButton4
    trigger:
    - platform: mqtt
      topic: 'hasp/plate01/state/p[1].b[4]'
      payload: 'ON'
    action:
    - service: scene.turn_on
      entity_id: scene.lights_on
```

This automation is `trigger`ed when Home Assistant receives the message `ON` in the topic `hasp/plate01/state/p[1].b[4]`.  The `action:` calls a [Home Assistant scene](https://www.home-assistant.io/components/scene/) called `lights_on` which was [defined elsewhere](../Home_Assistant/packages/hasp_demo.yaml#L43-L57).

You can [change the action](https://www.home-assistant.io/docs/automation/action/) to be anything that Home Assistant can do - [turn on a light](https://www.home-assistant.io/components/light/#service-lightturn_on), [play a song](https://www.home-assistant.io/components/media_player/#service-media_playerplay_media), [notify a user over SMS](https://www.home-assistant.io/components/notify.twilio_sms/#usage), or whatever else you can think of.

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
