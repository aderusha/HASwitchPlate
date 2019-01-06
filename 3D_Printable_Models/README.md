# Switch Plate model for single-gang workbox

## 3D Printed Enclosure

Above you'll find STL files ready for slicing and the source models editable in the free edition of [SketchUp Make](https://www.sketchup.com/download).  There are several models presented here for various plate configurations on the front and two options for the rear enclosure.

For most users, you'll want to print [HASwitchPlate_front_single.stl](HASwitchPlate_front_single.stl) and [HASwitchPlate_rear_nolcdmod.stl](HASwitchPlate_rear_nolcdmod.stl).

### Front plate models

* **[HASwitchPlate_front_single.stl](HASwitchPlate_front_single.stl)** Standard single-wide plate. `[HASP]`
* **[HASwitchPlate_front_double_decora_right.stl](HASwitchPlate_front_double_decora_right.stl)** Double-wide plate with Decora switch to the right `[HASP|Decora]`
* **[HASwitchPlate_front_double_toggle_right.stl](HASwitchPlate_front_double_toggle_right.stl)** Double-wide plate with toggle switch to the right `[HASP|toggle]`
* **[HASwitchPlate_front_triple_hasp_decora_decora.stl](HASwitchPlate_front_triple_hasp_decora_decora.stl)** Triple-wide plate with Decora switches to the right `[HASP|Decora|Decora]`
* **[HASwitchPlate_front_triple_hasp_toggle_toggle.stl](HASwitchPlate_front_triple_hasp_toggle_toggle.stl)** Triple-wide plate with toggle switches to the right `[HASP|toggle|toggle]`
* **[HASwitchPlate_front_5x_decora_decora_hasp_decora_decora.stl](HASwitchPlate_front_5x_decora_decora_hasp_decora_decora.stl)** 5-wide plate with 4x Decora switches and HASP in center `[Decora|Decora|HASP|Decora|Decora]`
* **[HASwitchPlate_front_single_dev.stl](HASwitchPlate_front_single_dev.stl)** Standard single-wide plate with SD card exposed for development use (unsafe!) `[HASP]`

### Rear enclosure models

* **[HASwitchPlate_rear_lcdmod.stl](HASwitchPlate_rear_lcdmod.stl)** The `lcdmod` enclosure requires the removal of the 4-pin XHP connector from the Nextion LCD panel.  This option allows for better clearance around the screw posts which may help in tight work boxes, but the process of safely removing the connecter may require a hot air station.
* **[HASwitchPlate_rear_nolcdmod.stl](HASwitchPlate_rear_nolcdmod.stl)** The `nolcdmod` enclosure does not require removing the 4-pin XHP connector from the Nextion LCD panel.  This simplifies the build process but has just a little less room behind the device for the work box screws.  We're not talking a lot here, so if you don't have the hot air station this option will probably work fine for you.
* **[HASwitchPlate_rear_lcdmod_minimum_clearance.stl](HASwitchPlate_rear_lcdmod_minimum_clearance.stl)** The `lcdmod_minimum_clearance` rear enclosure has been modified to provide the tightest fit around the HASP components that I can work out.  The walls are flimsy and thin, it will require supports to print, and you'll need 4 x [4mm M2 countersunk screws](https://www.amazon.com/uxcell-Stainless-Countersunk-Phillips-Machine/dp/B01L7Q5DR0) to assemble.  I don't recommend this enclosure unless you can't get the standard enclosure to fit in your workbox.
* **[HASwitchPlate_rear_dev.stl](HASwitchPlate_rear_dev.stl)** The `dev` enclosure has USB, SD card, and Nextion XHP connector exposed for development/test purposes.  This is unsafe to install in your workbox.

### Desktop models

An enclosure designed for desktop (table, bedstand, etc) use has been developed.  The enclosure will fit the standard PCB, WeMos D1 Mini, and Nextion 2.4" but will not allow installation or use of the AC PSU.  Power is provided by USB connection to the WeMos D1 Mini.

* **[HASwitchPlate_desktop_front.stl](HASwitchPlate_desktop_front.stl)** Front panel for desktop use with opening for SD card access
* **[HASwitchPlate_desktop_rear.stl](HASwitchPlate_desktop_rear.stl)** Rear enclosure for desktop use with openings for SD card, USB, and Nextion Serial access
* **[HASwitchPlate_desktop_base.stl](HASwitchPlate_desktop_base.stl)** Handy base for desktop use.  Print with higher infill to add mass to weigh it down a bit.

## 3D Printing Notes

### FDM affordance

The careful observer might note that the dimensions of the model vary just slightly from the [published dimensions](https://www.itead.cc/wiki/images/a/ad/2.4%27%27_Nextion_Dimension.pdf) of the panel used in this project.  This has been done to accommodate the nature of FDM 3D printing, which means that other production technologies may require slight modifications to make everything fit snug.  The STLs are published in the correct orientation for printing.

### Filament

I printed these two parts with [Atomic Filament Bright White Opaque PETG Pro](https://atomicfilament.com/products/bright-white-opaque-petg-pro) on a [Prusa i3 MK2S](http://shop.prusa3d.com/en/3d-printers/59-original-prusa-i3-mk2-kit.html) with 40% infill, .15mm layers, no supports, and 6 bottom layers.  The finished results look almost injection molded.

### Inserts

The mounting holes for the Nextion panel are sized to fit [M2 x 3mm threaded inserts](https://www.amazon.com/a16041800ux0765-Cylinder-Knurled-Threaded-Embedded/dp/B01IZ15A5U) but should accept an M3 screw (tightly).  See [the enclosure build documentation](../Documentation/04_Project_Enclosure.md#threaded-inserts) for more details.

### SketchUp

The SketchUp file included here is dimensioned in meters but 1m == 1mm due to issues with how SketchUp (doesn't) handle small dimensions.  If you intend to modify the provided SketchUp model, export to STL in meters then import for print in mm (the standard setting for most slicers) and you'll be fine.  The included STLs are ready to slice as-is.