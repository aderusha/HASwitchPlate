# Home Assistant example configurations

These configurations serve as additional examples for interacting with the HASP from Home Assistant.  Each `.yaml` file is a Home Assistant package with all of the required components to accomplish the specified task.  Using these examples is as easy as renaming `plate01` to your device name (search and replace will work) and copying the result to your `packages` folder.

## Example Configurations

* [`hasp_plate01_00_autofirmwareupdate.yaml`](hasp_plate01_00_autofirmwareupdate.yaml) Update HASP Arduino code when a new firmware package is published
* [`hasp_plate01_00_backlightbysun.yaml`](hasp_plate01_00_backlightbysun.yaml) Adjust HASP backlight level to gradually dim as the sun goes down, the brighten when it comes back up
* [`hasp_plate01_00_defaultpage.yaml`](hasp_plate01_00_defaultpage.yaml) Define a default page and timeout, switch back to default page after the timeout expires
* [`hasp_plate01_00_kodisensor.yaml`](hasp_plate01_00_kodisensor.yaml) Template sensors to report Kodi media player activity by @squirtbrnr
* [`hasp_plate01_00_motionsensor.yaml`](hasp_plate01_00_motionsensor.yaml) Example automation for use with a motion sensor installed on the HASP
* [`hasp_plate01_p0_messagedisplay.yaml`](hasp_plate01_p0_messagedisplay.yaml) Display messages or QR codes on the HASP
* [`hasp_plate01_p9_3dprint.yaml`](hasp_plate01_p9_3dprint.yaml) Monitor a 3D printer via OctoPrint