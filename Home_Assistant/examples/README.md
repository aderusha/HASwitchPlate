# Home Assistant examples

This folder provides example configurations demonstrating ideas and solutions utilizing HASP with Home Assistant.  Configurations here should be in [Home Assistant package format](https://www.home-assistant.io/docs/configuration/packages/) for easy deployment.

If you've developed something that you'd like to share, please submit a pull request or contact me and I'll add it to the repository.

* `hasp_plate01_00_autofirmwareupdate.yaml` This automation uses the update status from the HASP to update the ESP8266 Arduino code from GitHub when a new version is available.  Firmware updates will happen at 3:00am local time.
* `hasp_plate01_00_backlightbysun.yaml` Adjust the HASP backlight based on the position of the sun.  Useful in sunlit rooms to dim down the panel as night falls, then brighten again in the morning.
* `hasp_plate01_00_kodisensor.yaml` Sensor templates to pull attributes from Kodi which can be easily used to display media information on the HASP.  Thanks to Home Assistant community user @squirtbrnr for this one!
* `hasp_plate01_p0_messagedisplay.yaml` Input fields and automations to display pop-up notifications or messages on the HASP.  Text and/or QR codes can be displayed with a timed duration, after which the HASP will return to the previous page.