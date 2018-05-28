# Home Assistant integration

Configuring Home Assistant for the HASP requires making some basic changes to your configuration and downloading the pacakges to your installation.  The procedure will be a little different if you're running Hass.io, so [skip to that section](#hass.io) or continue below for a standard Home Assistant installation (hassbian, venv, whatever).

## Standard Home Assistant installation

### `configuration.yaml` changes

To deploy your first HASP device into Home Assistant a couple simple changes will need to be made to your `configuration.yaml`.  [See the documentation here](https://www.home-assistant.io/getting-started/configuration/) for the general process of editing that file.

Once these changes have been made for your first device, you can skip to the [`deployhasp.sh`](#deployhaspsh) step when adding additional devices to your installation.

### Packages

The configuration and automation files for the HASP are bundled as [Home Assistant Packages](https://www.home-assistant.io/docs/configuration/packages/).  Enable packages under the `homeassistant` section at the top of your `configuration.yaml` by adding the following line:

```yaml
homeassistant:
  packages: !include_dir_named packages
```

### MQTT

This project relies on [MQTT](https://home-assistant.io/docs/mqtt/) for device communication.  You will need to enable Home Assistant MQTT support by adding the following line to your `configuration.yaml`:

```yaml
# Example configuration.yaml entry
mqtt:
```

If you don't already have an MQTT broker configured, adding this one line will enable the built-in MQTT broker.

### Recorder

The [Home Assistant Recorder](https://www.home-assistant.io/components/recorder/) component is required to allow Home Assistant to save configuration and state of some HASP controls across reboots.  You will can enable Home Assistant Recorder by adding the following line to your `configuration.yaml`:

```yaml
# Example configuration.yaml entry
recorder:
```

### `deployhasp.sh`

Now you'll need to copy over the [packages directory](https://github.com/aderusha/HASwitchPlate/tree/master/Home_Assistant/packages) and modify it for your new device.  A script has been created for this process which you can execute from your Home Assistant server with the following commands:

```bash
cd ~/.homeassistant
bash <(wget -qO- -o /dev/null https://raw.githubusercontent.com/aderusha/HASwitchPlate/master/Home_Assistant/deployhasp.sh)
```

Finally, you'll need to restart Home Assistant to apply your changes.

## Hass.io

If you're running [Hass.io](https://www.home-assistant.io/hassio/), you'll need to install and configure the [`Mosquitto broker`](https://www.home-assistant.io/addons/mosquitto/) and 
 [`SSH server`](https://www.home-assistant.io/addons/ssh/) add-ons from the default repository.  Once those are installed and started, ssh to your hass.io installation and execute the following commands:

```bash
cd /config
sed -i 's/^homeassistant:.*/homeassistant:\n  packages: !include_dir_named packages/' configuration.yaml
echo "recorder:" >> configuration.yaml
apk add tar wget
bash <(wget -qO- -o /dev/null https://raw.githubusercontent.com/aderusha/HASwitchPlate/master/Home_Assistant/deployhasp.sh)
```

## First time setup

Upon startup the default HMI display file contains empty buttons with no text.  Launch the Home Assistant web UI and look for a new tab with your chosen device name.  Select that tab and look for the automation titled `hasp_<your_device_name>_00_FirstTimeSetup`.  Select that automation and click "TRIGGER" to apply the basic configuration to your new device.

![Home_Assistant_FirstTimeSetup](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/Home_Assistant_FirstTimeSetup.png?raw=true)