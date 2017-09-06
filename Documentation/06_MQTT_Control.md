# MQTT Control

## MQTT Namespace
By default the device will subscribe to `homeassistant/haswitchplate/<node_name>/#` to accept incoming commands.  There are two subtopics to set attributes and to issue commands:

* `nextionattr` will set an attribute of the display, such as button text or screen dim.  The specific attribute must be appended as a subtopic, with the value to be set delivered as the payload.  For example, the following command will set the text displayed on page 1/button 4 to "Lamp On": `mosquitto_pub -h mqtt -t homeassistant/haswitchplate/nodename/nextionattr/p1.b4.txt -m '"Lamp On"'`
* `nextioncmd` will issue a raw command to the panel.  There is no subtopic, the message payload will be sent as-is to the Nextion display.  Here's an example command to change the currently displayed page: `mosquitto_pub -h mqtt -t homeassistant/haswitchplate/nodename/nextioncmd -m "page 2"`

[Detailed documentation on the Nextion command instruction set can be found here](https://www.itead.cc/wiki/Nextion_Instruction_Set).  Note that some values require quotes, while others don't, and different shells (and Home Assistant config files) eat quotes differently.  As a general rule, attributes which accept an numeric value cannot have quotes, all other attributes must be enclosed in double quotes.

When sending messages to MQTT you might find it useful to retain `nextionattr` messages, as the device can then pick those values back up from the broker upon connection.  `nextioncmd` statements typically will not be retained, and they will be overwritten by the next command in any event.

User button presses will be sent to MQTT as `nextionattr` messages with the button ID sent as a subtopic.  For example, when a user presses button 4 on page 2 the device will issue a message 'ON' to the topic `homeassistant/haswitchplate/nodename/nextionattr/p1.b4`.  An `OFF` message will be issued on button release.
