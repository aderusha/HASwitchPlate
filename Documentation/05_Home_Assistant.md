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
