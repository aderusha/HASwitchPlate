# Project Enclosure

## 3D Printed Enclosure

The [enclosure provided](../3D_Printable_Models) includes STL files ready for slicing and the source models editable in the free edition of [SketchUp Make](https://www.sketchup.com/download).

## Assembling the completed project

### High voltage AC cabling

The AC power cables should be at least 18AWG 300V stranded cable with a white jacket soldered to the `AC/N` pad on the PCB and a similar wire with a black jacket soldered to `AN/L`.  These are fed through a [rubber push-in grommet](https://www.mcmaster.com/#9600k41) mounted into the rear enclousure.  I've had good luck stripping an existing 3 conductor power cord and using the black/white wires inside.

### Heat-set inserts

To hold the two halves on the enclosure together I've used [four 20mm M2 flat-head screws](https://www.amazon.com/gp/product/B000FN3Q94) through the rear of the enclosure into a set of [four 3mm M2 heat-set threaded inserts](https://www.amazon.com/gp/product/B01IZ157KS).  For installation I'm using a conical tip on my soldering iron set at 350°F to heat up the insert and press into the front plate.

![Installing threaded inserts](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/Assembly_Installing_Inserts.jpg?raw=true)

I've threaded in a brass standoff I had laying around to prevent melted plastic from entering the nut during assembly and to give myself a handle to position and retain the insert when removing the soldering iron.  The model includes a small cutout for the inserts to sit centered on their hole in preparation for installation, so I just set the insert in place, heat it for a moment with the iron, gently press until it's inserted, then hold the standoff with some pliers (it's hot!) while I remove the iron and adjust to make sure it's reasonably vertical while the plastic is still pliable.  Be careful not to press in too far or you'll leave a dimple in the front of the plate!

### PCB Installation

Installing the [HASP PCB](../PCB) requires a pair of small m2-ish screws, I'm using [M2 self-tapping 6MM screws](https://www.amazon.com/gp/product/B01FXGHO2M) but M2 machine screws will work fine too.  There shouldn't be much strain on these two so I haven't bothered with inserts here.

![Parts installed](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/PCB_Assembly_Parts_Installed.jpg?raw=true)

Once everything is fit together then it's a simple matter of running the four 20mm m2 screws through the back to hold the two halves of the enclosure together.  Again, be careful not to tighten these too hard as the screws can dimple the front of the switch plate.  Now you should be ready to mount into the wall!

![Assembled side view](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/Assembly_Assembled_Side.jpg?raw=true) ![Assembled front view](https://github.com/aderusha/HASwitchPlate/blob/master/Documentation/Images/Assembly_Assembled_Front.jpg?raw=true)