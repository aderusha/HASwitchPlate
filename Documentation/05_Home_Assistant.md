# Home Assistant integration

Upon startup the default HMI display file contains empty buttons with no text.  You must send the device its configuration when it connects to your broker.

## MQTT

This project relies on [MQTT](https://home-assistant.io/docs/mqtt/) for device communication.  You will need to enable Home Assistant MQTT support by adding the following line to your `configuration.yaml`:

```yaml
# Example configuration.yaml entry
mqtt:
```

If you don't already have an MQTT broker configured, adding this one line will enable the built-in MQTT broker.

## Recorder

The [Home Assistant Recorder](https://www.home-assistant.io/components/recorder/) component is required to allow Home Assistant to save configuration and state of some HASP controls across reboots.  You will need to enable Home Assistant MQTT support by adding the following line to your `configuration.yaml`:

```yaml
# Example configuration.yaml entry
recorder:
```

## Packages

The configuration and automation files for the HASP are bundled as [Home Assistant Pacakges](https://www.home-assistant.io/docs/configuration/packages/).  Enable packages under the `homeassistant` section at the top of your `configuration.yaml` by adding the following line:

```yaml
homeassistant:
  packages: !include_dir_named packages
```

Now, copy the entire [`packages` folder structure](../Home_Assistant/packages) to your `.homeassistant` folder and restart Home Assistant to pull in your changes.  From the web UI, find the "hasp_demo_FirstTimeSetup" automation, click on it, and select `TRIGGER` to setup the example configuration.