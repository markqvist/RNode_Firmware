[date]: <> (2023-01-14)
[title]: <> (Handheld RNode)
[image]: <> (gfx/rnode_iso.webp)
[excerpt]: <> (This RNode is suitable for mobile and handheld operation, and offers both wireless and wired connectivity to host devices. A good all-round unit. It is also suitable for permanent installation indoors.)
## Handheld RNode Recipe
*Version 2.1*

This build recipe will help you create an RNode that is suitable for mobile and handheld operation, and offers both wireless and wired connectivity to host devices. It is also useful for permanent installation indoors, or even outdoors, as long as it is protected from water ingress and direct sunlight.

Depending on the board you use, it will offer a workable frequency range between **420 and 520 MHz**, or **820 and 1020 MHz**, and a maximum TX power of **17 dBm** (50 mW).

<img alt="Completed Handheld RNode" src="{ASSET_PATH}images/bg_h_1.webp" style="width: 100%;"/>
<center>*A completed Handheld RNode*</center>

### Table of Contents

1. [Preparation](#prep)
2. [Supported Boards](#devboard)
3. [Materials](#materials)
4. [Print Parts](#parts)
5. [Install Tools](#tools)
6. [Firmware Setup](#firmware)
7. [Assembly](#assembly)


### <a name="prep"></a>Step 1: Preparation

When you have completed this recipe, you will end up with a fully-featured RNode device, similar to the one pictured above. To make it as easy as possible to complete this guide, make sure to read it all in its entirity *before* starting. I also recommend you familiarise yourself with the required materials, and the software tools needed for the setup.

To complete this build recipe, you will need access to the following items:

- A computer with a functional operating system, such as Linux, BSD or macOS
- One of the [supported development boards](#devboard) for this recipe
- A suitable USB cable for connecting the development board to your computer
- A 3D printer and the necessary amount of material for printing the [device parts](#parts)
- 6 pieces of M2x6mm screws to assemble the case
- A suitable antenna
- An optional NeoPixel RGB LED
- An optional [battery](#battery)
    - This build can use any single-cell (3.7v) lithium battery with a 1.25mm JST connector, provided it will fit in the case. Please see [this section](#battery) for details on battery sizes.

### <a name="devboard"></a>Step 2: Supported Development Boards

This RNode design is using a **LilyGO LoRa32 v2.1** board, in either the **433 MHz**, **868 MHz**, **915 MHz** or **923 MHz** variants. It seems that the 868, 915 and 923 MHz variants are in fact completely identical, and all offer a frequency range between 820 and 1020 MHz. The 433MHz variants offer a frequency range between 420 and 520 MHz.

These boards are also sold under many different "brand" names other than LilyGO, but using the images below, you should be able to identify the correct ones.

It is easiest to obtain the version of the board with an **u.FL** (sometimes also labeled *IPX* or *IPEX*) antenna connector, instead of the **SMA** connector. This version comes with an SMA to u.FL pigtail, which is installed into the 3D-printed case. If it is not possible to obtain this version, you can use the one with an **SMA** connector, either as is, or by removing the **SMA** connector, and using the on-board **u.FL** connector instead.

If you do not wish to use the 3D-printable case included in this guide, it does not matter which version you get. There is **no functional difference** between the boards with **SMA** and **u.FL** connectors.

<img alt="Compatible board" src="{ASSET_PATH}images/bg_h_2.webp" style="width: 100%;"/>
<center>*The correct board version for this RNode build recipe*</center>

If you want to use the case provided for this build guide, and you have the version with an *SMA* connector, you will have to desolder the **SMA** connector, and activate the *u.FL* connector instead (it's already installed on all the boards, just not activated on the **SMA** connector versions).

To activate the **u.FL** connector, you will just have to "rotate" the small resistor next to the antenna connectors by 90 degrees, so it "points" at the connector you wish to use.

Please note that the "resistor" is actually just a zero-ohm jumper. If you don't feel like fiddling around with small components, you can simply remove it, and bridge the relevant gap with a blob of solder.

Refer to the following two pictures to locate the resistor that needs moving:

<img alt="Before desoldering" src="{ASSET_PATH}images/bg1ds1.webp" style="display: inline-block;width: 48.7%; margin-right:1%;"/>
<img alt="After desoldering" src="{ASSET_PATH}images/bg1ds2.webp" style="display: inline-block;width: 48.7%; margin-left: 1%;"/>
<center>*Before and after removing the SMA connector and moving the resistor*</center>

You will also need to dismount the OLED display from the small acrylic riser on the board, and unscrew and discard the riser. Be careful not to damage the display or ribbon cable while doing this. The OLED display will be mounted directly into a matching slot in the 3D-printed case.

As before, if you do not want to use the 3D printed case supplied here, it's probably much easier to keep the display on the board, and you can simply skip this step.

### <a name="materials"></a>Step 3: Obtain Materials

In addition to the board, you will need a few other components to build this RNode.

- A suitable **antenna**. Most boards purchased online include a passable antenna, but you may want to upgrade it to a better one.
- 6 pieces of **M2x6mm screws** for assembling the case. Can be bought in most hardware stores or from online vendors.
- An optional **NeoPixel RGB LED** for displaying status, and TX/RX activity. If you do not want to add this, it can simply be omitted.
    - The easiest way is to use the PCB-mounted NeoPixel "mini-buttons" manufactured by [adafruit.com](https://www.adafruit.com/product/1612). These fit exactly into the slot in the mounting position in the 3D-printed case, and are easy to connect cables to.
- An optional **lithium-polymer battery**.
    - This RNode supports **3.7v**, **single-cell** LiPo batteries with a **1.25mm JST connector**
    - The standard case can fit up to a 700mAh LP602248 battery
        - Maximum battery dimensions for this case is 50mm x 25mm x 6mm
    - There is a larger bottom casing available that fits 1100mAh batteries
        - Maximum battery dimensions for this case is 50mm x 25mm x 12mm

### <a name="parts"></a>Step 4: 3D Print Parts

To complete the build of this RNode, you will need to 3D-print the parts for the casing. Download, extract and slice the STL files from the [parts package]({ASSET_PATH}3d/Handheld_RNode_Parts.7z) in your preferred software.

- Two of the parts are LED light-guides, and should be printed in a semi-translucent material:
    - The `LED_Window.stl` file is a light-guide for the NeoPixel LED, mounted in the circular cutout at the top of the device.
    - The `LED_Guide.stl` file is a light-guide for the power and charging LEDs, mounted in the rectangular grove at the bottom of the device.
- The rest of the parts can be printed in any material, but for durability and heat-resistance, PETG is recommended.
    - The `Power_Switch.stl` file is a small power-switch slider, mounted in the matching grove on the bottom-left of the device.
    - The `Case_Top.stl` file is the top shell of the case. It holds the OLED display and NeoPixel RGB LED, and mounts to the bottom shell of the case with 6 M2 screws. The screw holes in both the top and bottom shells of the case are dimensioned to be self-threading when screws are inserted for the first time. Do not over-tighten.
    - The `Case_Bottom_Small_Battery.stl` file is the default bottom shell of the case. It holds batteries up to approximately 700mAh.
    - The `Case_Bottom_Large_Battery.stl` file is an alternative bottom shell for the case. It holds batteries up to approximately 1100mAh.
    - The `Case_Bottom_No_Battery.stl` file is an alternative bottom shell for the case. It does not have space for a battery, but results in a very compact device.
    - The `Case_Battery_Door.stl` file is the door for the battery compartment of the device. It snap-fits tightly into place in the bottom shell, and features a small slot for opening with a flathead screwdriver or similar.

All files are dimensioned to fit together perfectly without any scaling on a well-tuned 3D-printer.

The recommended layer height for all files is 0.15mm for FDM printers.

### <a name="tools"></a>Step 5: Install Tools

To install and configure the RNode Firmware on the device, you will need to install the `rnodeconf` program on your computer. This is included in the `rns` package, that can be [installed directly from this RNode]({ASSET_PATH}s_rns.html). Please carry out the installation instructions on [this page]({ASSET_PATH}s_rns.html), and continue to the next step when the `rnodeconf` program is installed.


### <a name="firmware"></a>Step 6: Firmware Setup

Once the `rnodeconf` program is installed, we will use it to install the RNode Firmware on your device, and do the initial provisioning of configuration parameters. This process can be completed automatically, by using the auto-installer. Run the `rnodeconf` auto-installer with the following command:

```
rnodeconf --autoinstall
```

1. The program will ask you to connect your device to an USB-port on your computer. Do so, and hit enter.
2. Select the serial port the device is connected as.
3. You will now be asked what device this is, select the option **A Specific Kind of RNode**.
4. The installer will ask you what model your device is. Select the **Handheld RNode v2.x** option that matches the frequency band of your device.
5. The installer will display a summary of your choices. If you are satisfied, confirm your selection.
6. The installer will now automatically install and configure the firmware and prepare the device for use.

> **Please Note!** If you are connected to the Internet while installing, the autoinstaller will automatically download any needed firmware files to a local cache before installing.

> If you do not have an active Internet connection while installing, you can extract and use the firmware from this device instead. This will **only** work if you are building the same type of RNode as the device you are extracting from, as the firmware has to match the targeted board and hardware configuration.

If you need to extract the firmware from an existing RNode, run the following command:

```
rnodeconf --extract
```

If `rnodeconf` finds a working RNode, it will extract and save the firmware from the device for later use. You can then run the auto-installer with the `--use-extracted` option to use the locally extracted file:

```
rnodeconf --autoinstall --use-extracted
```

This also works for updating the firmware on existing RNodes, so you can extract a newer firmware from one RNode, and deploy it onto other RNodes using the same method. Just use the `--update` option instead of `--autoinstall`.

### <a name="assembly"></a>Step 7: Assembly

With the firmware installed and configured, and the case parts printed, it's time to put it all together.

1. Insert the **SMA to u.FL** pigtail adatper into the matching **slot** in the top part of the bottom shell. Make sure it lines up with the internal hex-nut cut-out in the bottom shell, as the hex nut of the adapter will get pulled into this cut-out, and thereby self-lock, when an antenna is connected. You can optionally mount a locking nut on the exterior thread of the SMA connector when the case has been completely assembled.
2. Thread the cable of the **SMA to u.FL** pigtail adapter into the matching grove, and run it out of the bottom opening.
3. Mount the **power-switch slider** into the matching slot, in the bottom-left part of the bottom shell.
4. With the SMA connector and power switch mounted, slide the **board** into the bottom shell, such that the **power switch** of the **board** mates with the slot in the already installed power-switch slider. Click the **board** into place in the bottom shell.
5. Optionally mount the **NeoPixel LED**:
    - Measure out cables that matches lenghts between the NeoPixel mounting slot, and the corresponding pins on the board.
    - Solder the **V+**, **GND** and **DATA** cables to the NeoPixel.
    - Solder the **V+** cable to the **3.3v** pin on the board.
    - Solder the **GND** cable to the **GND** pin on the board.
    - Solder the **DATA** cable to **IO Pin 12** on the board.
    - Mount the **NeoPixel** in the circular slot in the top part of the top shell.
6. Carefully mount the OLED display in the rectangular slot in the middle part of the top shell.
7. While ensuring that all internal cables stay within their routing groves, place the **top shell** on top of the **bottom shell**, making sure that the screw-mounting holes line up.
8. Mount the 6 **M2x6mm screws** into the mounting holes, until the two shells of the case are tightly and securely connected.
9. Flip over the device.
10. Connect the male **u.FL** connector to the female **u.FL** socket on the **board**.
11. Optionally, connect the male JST connector of the **battery** to the female JST connector on the **board**.
12. Fit the **battery door** into place.

Congratulations, Your Handheld RNode is now complete!

Flip the power switch, and start using it!