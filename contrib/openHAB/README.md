# openHAB HASP Configuration

The configuration files in this folder have been created to adapt the HASP project for use with the openHAB home automation system. They interface with aderusha's Arduino code via MQTT, without the need for any additional bindings.

## Installation

1. Ensure you have a working MQTT broker and HASP plate
2. Copy the included .items and .rules files into their appropriate folders within your configuration.
3. Copy the sitemap configuration out of the HASP.sitemap file into your own configuration

## Configuration

1. Edit the HASP.rules file (within the "USER CONFIG" sections only!) to adapt the HASP plate to your configuration. You will want to modify the HASP page settings and buttons, to select the pages and buttons you want to show on your HASP plate. Then, edit the HASP->openHAB and openHAB->HASP rules to make the HASP buttons and dimmers interact with your openHAB items and rules.

***

## Notes

I have included several sample config settings and rules to show you how to interact with your own openHAB items (e.g. setting the first dimmer on page 5 will change the HASP brightness item, and the changing brightness item will modify the state of the second dimmer on page 5 - this will show you how to make HASP work with your own openHAB configuration). I also included a rule to operate the chart on page 9, which is a specific display item on the Nextion HMI and this should show you how to interact with the chart.

If you want to use multiple plates, you'll need to create a second set of items/rules/sitemap files, and rename all of them from "plate01" to your proper HASP name.

If you have any specific questions related to the openHAB implementation of the HASP, please post your questions on the [openHAB forums](http://community.openhab.org), watch [BK Hobby's](https://www.youtube.com/c/BKHobby) Youtube channel for videos related to this or openHAB in general, or join [BK Hobby's Discord](https://discord.gg/jUr7J4u) servier for a direct question to one of the openHAB experts there.
