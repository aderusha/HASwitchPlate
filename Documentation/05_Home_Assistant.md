# Home Assistant integration

Configuring Home Assistant for the HASP requires making some basic changes to your configuration and downloading the packages to your installation.  The procedure will be a little different if you're running Hass.io, so [skip to that section](#hassio) or continue below for a standard Home Assistant installation (hassbian, venv, whatever).

## Automatic Home Assistant installation

For standard Home Assistant installations you can run an [automatic deployment script](../Home_Assistant/deployhasp.sh) which will attempt to make the required changes to your Home Assistant installation to support the HASP and a [Home Assistant Packages](../Home_Assistant/packages) bundle for each HASP device which you deploy.  If you'd rather make all the changes yourself, jump to the [Manual Home Assistant installation section](#manual-home-assistant-installation).

You'll need to ssh to your Home Assistant installation as a user who has access to write to your home assistant installation.  For most installations, this will be the user `homeassistant`.

```bash
sudo su -s /bin/bash homeassistant
cd ~/.homeassistant
bash <(wget -qO- -o /dev/null https://raw.githubusercontent.com/aderusha/HASwitchPlate/master/Home_Assistant/deployhasp.sh)
```

You will be prompted for a device name and the script will do the rest.  Once it completes, restart your Home Assistant service to apply changes and then continue to the [First time setup](#first-time-setup) section below to initialize your environment.

---

## Hass.io

If you're running [Hass.io](https://www.home-assistant.io/hassio/), you'll need to install and configure the [`Mosquitto broker`](https://www.home-assistant.io/addons/mosquitto/) and [`SSH server`](https://www.home-assistant.io/addons/ssh/) add-ons from the default repository.  Be sure to follow the configuration instructions provided for both add-ons.  Once those are installed, configured, and started, you can ssh to your hass.io installation and execute the following commands:

```bash
cd /config
apk add tar wget
bash <(wget -qO- -o /dev/null https://raw.githubusercontent.com/aderusha/HASwitchPlate/master/Home_Assistant/deployhasp.sh)
```

You will be prompted for a device name and the script will do the rest.  Once it completes, restart your Home Assistant service to apply changes (`Configuration` > `General` > `Server Management` > `RESTART`) and then continue to the [First time setup](#first-time-setup) section below to initialize your environment.

---

## Manual Home Assistant installation

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

Now you'll need to copy over the [packages directory](https://github.com/aderusha/HASwitchPlate/tree/master/Home_Assistant/packages) and modify it for your new device.  The folder name, file names, and the contents of the `.yaml` files will all need to have `plate01` replaced with your new device name.

Finally, you'll need to restart Home Assistant to apply your changes then continue to the [First time setup](#first-time-setup) section below to initialize your environment.

---

## First time setup

> ### NOTICE
>
> If you're running Lovelace (now the new default Home Assistant web UI), there [currently is no way for a project like HASP to add panels](https://community.home-assistant.io/t/lovelace-in-ha-package-files/92619).  You'll need to switch to the "states" UI before proceeding.  If your installation is accessed at `http://hassio.local:8123`, navigate to `http://hassio.local:8123/states` to access the states UI.

Upon startup the default HMI display file contains empty buttons with no text.  Launch the Home Assistant web UI and look for a new tab with your chosen device name.  Select that tab and look for the automation titled `hasp_<your_device_name>_00_FirstTimeSetup`.  Select that automation and click "TRIGGER" to apply the basic configuration to your new device.

![Home_Assistant_FirstTimeSetup](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/Home_Assistant_FirstTimeSetup.png?raw=true)
