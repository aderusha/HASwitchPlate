# Home Assistant configurations

These folders contain a ready-to-run Home Assistant configuration for testing and demonstration. 4 dimmable lights are defined to demonstrate device control, some weather sensors are defined to show status output, and the [Google Desktop Music Player](https://home-assistant.io/components/media_player.gpmdp/) component is included to demonstrate media player controls.

[Check the documentation](../Documentation/05_Home_Assistant.md) for deployment details.

Due to the large size of the automation files included, the `configuration.yaml` makes use of a [split configuration](https://home-assistant.io/docs/configuration/splitting_configuration/).  Note that this prevents you from using the web UI's built-in automation editor.
