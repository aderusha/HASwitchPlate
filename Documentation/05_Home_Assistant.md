# Home Assistant integration

Configuring Home Assistant for the HASP requires making some basic changes to your configuration and downloading the packages to your installation.  [Continue below](#standard-home-assistant-installation) for a standard Home Assistant installation in a container (HassOS, HassIO, Docker, etc).  If you aren't sure which Home Assistant method you've used, you're probably using the Standard installation.

The procedure will be a little different if you're running Home Assistant Core (venv, hassbian, or similar), so [skip to that section](#home-assistant-core-installation).

Finally, if you'd rather make all the changes yourself, jump to the [Manual Home Assistant installation section](#manual-home-assistant-installation).

## Standard Home Assistant installation

Before deploying your first HASP device, you'll need to install and configure the [`Mosquitto broker`](https://www.home-assistant.io/addons/mosquitto/) and [`Terminal & SSH`](https://www.home-assistant.io/addons/ssh/) add-ons from the default repository.

### Component installation

1. In the Home Assistant web interface, select your user name in the lower-left screen, then select the toggle to enable "Advanced Mode"
2. Select `Supervisor` > `Add-on Store` > `Mosquitto broker`, then select `Install`.  Once that completes, click `START` to start the Mosquitto MQTT broker
3. Select `Supervisor` > `Add-on Store` > `Terminal & SSH`, then select `Install`.  Once the install completes, click "Show in sidebar" and then click `START`
4. Select `Configuration` > `Integrations` and then select `Configure` under `MQTT`.  Leave `Enable discovery` checked and click `SUBMIT`.  Select `FINISH` upon completion.

### HASP configuration for MQTT

1. Open a web browser to the HASP IP shown when the device boots up (or try `<hasp device name>.local`).  
2. On the main admin page, enter the IP address of your Home Assistant installation under `MQTT Broker`, enter port `1883` for `MQTT Port`, and then enter your Home Assistant username and password for `MQTT User` and `MQTT Password`.
3. Click `save settings` to commit your changes and reboot the HASP device.

### Home Assistant automations for HASP

We are now ready to deploy the Home Assistant automations for HASP.  Select `Terminal` from the left pane, and then paste the following:

```bash
bash <(wget -qO- -o /dev/null https://raw.githubusercontent.com/aderusha/HASwitchPlate/master/Home_Assistant/deployhasp.sh)
```

You will be prompted for a device name and the script will do the rest.  Once it completes, the script will display a Lovelace configuration which you can paste into your existing Home Assistant UI through the Lovelace editor.

### Home Assistant Lovelace Configuration

The graphical elements in the Home Assistant web UI are made available through a Lovelace configuration.

1. After `deployhasp.sh` completes a Lovelace configuration will be displayed.  Hold the `SHIFT` key on your keyboard and highlight the code shown to copy to your clipboard.
2. Select `Overview` on the left
3. Select the three dots `⋮` at the top-right, then select `Edit Dashboard`.  If this is your first time editing your dashboard, select `TAKE CONTROL` to proceed.
4. Select the three dots `⋮` at the top-right again, this time selecting the option `Raw configuration editor`
5. Paste the Lovelace configuration you copied in step 1 at the very end of the existing configuration and select `SAVE` from the top-right.
6. Click the `X` at the top-left to exit the raw configuration editor, then click `X` at the top-left again to exit the dashboard editor.

### First time setup

1. To apply your changes, restart Home Assistant (`Configuration` > `Server Controls` > `Server management` > `RESTART`)
2. After Home Assistant has restarted, look for a tab along the top with your device name and select it.  Click the button labeled `Click here for HASP <name> First-Time Setup`.

You should now have a fully-functioning HASP installation ready for customization!

---

## Home Assistant Core installation

Before deploying your first HASP device, you'll need to install and configure an [MQTT broker](https://www.home-assistant.io/docs/mqtt/broker) for Home Assistant.

You'll need to ssh to your Home Assistant installation as a user who has access to write to your home assistant installation.  For most installations, this will be the user `homeassistant`.

```bash
sudo su -s /bin/bash homeassistant
cd ~/.homeassistant
bash <(wget -qO- -o /dev/null https://raw.githubusercontent.com/aderusha/HASwitchPlate/master/Home_Assistant/deployhasp.sh)
```

You will be prompted for a device name and the script will do the rest.  Once it completes, restart your Home Assistant service to apply changes.  After Home Assistant has restarted, look for a tab along the top with your device name and select it.  Click the button labeled `Click here for HASP <name> First-Time Setup`.

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

Finally, you'll need to restart Home Assistant to apply your changes.  After Home Assistant has restarted, look for a tab along the top with your device name and select it.  Click the button labeled `Click here for HASP <name> First-Time Setup`.
