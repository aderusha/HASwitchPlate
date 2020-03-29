# Home Assistant integration

Configuring Home Assistant for the HASP requires making some basic changes to your configuration and downloading the packages to your installation.  [Continue below]((#standard-home-assistant-installation) for a standard Home Assistant installation in a container (HassOS, HassIO, Docker, etc).

The procedure will be a little different if you're running Home Assistant Core (venv, hassbian, or similar), so [skip to that section](#home-assistant-core-installation).

Finally, if you'd rather make all the changes yourself, jump to the [Manual Home Assistant installation section](#manual-home-assistant-installation).

## Standard Home Assistant installation

Before deploying your first HASP device, you'll need to install and configure the [`Mosquitto broker`](https://www.home-assistant.io/addons/mosquitto/) and [`Terminal & SSH`](https://www.home-assistant.io/addons/ssh/) add-ons from the default repository.  Be sure to follow the configuration instructions provided for both add-ons.

The mosquitto broker package requires a username and password.  Be sure to configure the "MQTT User" and "MQTT Password" on the HASP with a valid user from your Home Assistant configuration, and set the broker to the IP address of your Hass.io installation.  You will also need to setup the MQTT Integration (`Configuration` > `Integrations` > `MQTT` > `CONFIGURE`) and check the box `Enable discovery`.

Once those are installed, configured, and started, you can open the Terminal and execute the following command:

```bash
bash <(wget -qO- -o /dev/null https://raw.githubusercontent.com/aderusha/HASwitchPlate/master/Home_Assistant/deployhasp.sh)
```

You will be prompted for a device name and the script will do the rest.  Once it completes, the script will display a Lovelace configuration which you can paste into your existing Home Assistant UI through the Lovelace editor.

To apply your changes, restart Home Assistant (`Configuration` > `Server Controls` > `Server management` > `RESTART`) and then continue to the [First time setup](#first-time-setup) section below to initialize your environment.

[Check out this video](https://youtu.be/wbtVfuDKaM4) for the complete process of starting up a new Hass.io installation, configuring the required add-ons, and setting up your first HASP device.

---

## Home Assistant Core installation

Before deploying your first HASP device, you'll need to install and configure an [MQTT broker](https://www.home-assistant.io/docs/mqtt/broker) for Home Assistant.

You'll need to ssh to your Home Assistant installation as a user who has access to write to your home assistant installation.  For most installations, this will be the user `homeassistant`.

```bash
sudo su -s /bin/bash homeassistant
cd ~/.homeassistant
bash <(wget -qO- -o /dev/null https://raw.githubusercontent.com/aderusha/HASwitchPlate/master/Home_Assistant/deployhasp.sh)
```

You will be prompted for a device name and the script will do the rest.  Once it completes, restart your Home Assistant service to apply changes and then continue to the [First time setup](#first-time-setup) section below to initialize your environment.

---

## Manual Home Assistant installation

### `configuration.yaml` changes

To deploy your first HASP device into Home Assistant a couple simple changes will need to be made to your `configuration.yaml`.  [See the documentation here](https://www.home-assistant.io/getting-started/configuration/) for the general process of editing that file.  These changes only need to be made once, if you are adding additional HASP devices to an existing installation you can skip directly to the [`deployhasp.sh`](#deployhaspsh) section below.

### Packages

The configuration and automation files for the HASP are bundled as [Home Assistant Packages](https://www.home-assistant.io/docs/configuration/packages/).  Enable packages under the `homeassistant` section at the top of your `configuration.yaml` by adding the following line:

```yaml
homeassistant:
  packages: !include_dir_named packages
```

### MQTT

This project relies on communication with an [MQTT broker](https://www.home-assistant.io/docs/mqtt/broker).  You will need to enable Home Assistant MQTT support by following that guide.

### Recorder

The [Home Assistant Recorder](https://www.home-assistant.io/components/recorder/) component is required to allow Home Assistant to save configuration and state of some HASP controls across reboots.  You will can enable Home Assistant Recorder by adding the following line to your `configuration.yaml`:

```yaml
# Example configuration.yaml entry
recorder:
```

### `deployhasp.sh`

Now you'll need to copy over the [packages directory](https://github.com/aderusha/HASwitchPlate/tree/master/Home_Assistant/packages) and modify it for your new device.  The folder name, file names, and the contents of the `.yaml` files will all need to have `plate01` replaced with your new device name.

The `deployhasp.sh` script will automate this task and can be executed with the following commands:

```bash
sudo su -s /bin/bash homeassistant
cd ~/.homeassistant
bash <(wget -qO- -o /dev/null https://raw.githubusercontent.com/aderusha/HASwitchPlate/master/Home_Assistant/deployhasp.sh)
```

Finally, you'll need to restart Home Assistant to apply your changes then continue to the [First time setup](#first-time-setup) section below to initialize your environment.

---

## First time setup

## Lovelace configuration

Lovelace doesn't support automation of the UI so a few manual steps need to happen next

Upon startup the default HMI display file contains empty buttons with no text.  Launch the Home Assistant web UI and look for a new tab with your chosen device name.  Select that tab and look for the automation titled `hasp_<your_device_name>_00_FirstTimeSetup`.  Select that automation and click "TRIGGER" to apply the basic configuration to your new device.

![Home_Assistant_FirstTimeSetup](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/Home_Assistant_FirstTimeSetup.png?raw=true)
