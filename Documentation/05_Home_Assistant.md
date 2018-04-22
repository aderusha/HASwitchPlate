# Home Assistant integration
Upon startup the default HMI display file contains empty buttons with no text.  You must send the device its configuration when it connects to your broker.

The Arduino sketch conforms to the [Home Assistant MQTT Discovery standards](https://home-assistant.io/docs/mqtt/discovery/) to create a `binary_sensor` device indicating the connection state of the panel. A "Last Will and Testament" is registered with the broker to toggle the connectivity state back off in the event that the connection is dropped.  A dimmable `light` device will be created to indicate and control the panel's backlight setting.  You will need to enable MQTT discovery for this functionality to work by adding the following lines to your `configuration.yaml`:

````
# Example configuration.yaml entry
mqtt:
  discovery: true
  discovery_prefix: homeassistant
````

You can setup an automation in Home Assistant to send attribute commands to the device when the state toggles on.  This device makes heavy use of Home Assistant automations to control every aspect of the display, so expect to have a pretty hefty pile of automation files once you have everything setup.

A complete demo configuration including scenes and automations is available [here](../Home_Assistant).

The provided Home Assistant automations presume the default node name of `HASwitchPlate`.  If you've changed the node name (or are adding additional devices) you will require additional automations and careful use of Case Sensitive search+replace to update the MQTT topic strings for your nodes.

With the demonstration automations in place, the default node name of HASwitchPlate, and hass installed on `localhost`, you can issue the following commands to initialize the base automations to their default values:

```bash
curl -X POST -H "x-ha-access: YOUR_PASSWORD" -H "Content-Type: application/json" -d '{ "entity_id": "input_text.haswitchplate_pagebutton1_label", "value": "scenes" }' http://localhost:8123/api/services/input_text/set_value
curl -X POST -H "x-ha-access: YOUR_PASSWORD" -H "Content-Type: application/json" -d '{ "entity_id": "input_text.haswitchplate_pagebutton2_label", "value": "status" }' http://localhost:8123/api/services/input_text/set_value
curl -X POST -H "x-ha-access: YOUR_PASSWORD" -H "Content-Type: application/json" -d '{ "entity_id": "input_text.haswitchplate_pagebutton3_label", "value": "alarm" }' http://localhost:8123/api/services/input_text/set_value
curl -X POST -H "x-ha-access: YOUR_PASSWORD" -H "Content-Type: application/json" -d '{ "entity_id": "input_number.haswitchplate_pagebutton1_page", "value": 1}' http://localhost:8123/api/services/input_number/set_value
curl -X POST -H "x-ha-access: YOUR_PASSWORD" -H "Content-Type: application/json" -d '{ "entity_id": "input_number.haswitchplate_pagebutton2_page", "value": 2}' http://localhost:8123/api/services/input_number/set_value
curl -X POST -H "x-ha-access: YOUR_PASSWORD" -H "Content-Type: application/json" -d '{ "entity_id": "input_number.haswitchplate_pagebutton3_page", "value": 7}' http://localhost:8123/api/services/input_number/set_value
curl -X POST -H "x-ha-access: YOUR_PASSWORD" -H "Content-Type: application/json" -d '{ "entity_id": "input_number.haswitchplate_active_page", "value": 1}' http://localhost:8123/api/services/input_number/set_value
```

