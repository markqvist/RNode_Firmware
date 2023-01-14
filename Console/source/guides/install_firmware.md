[date]: <> (2023-01-12)
[title]: <> (Installing RNode Firmware on Supported Devices)
[image]: <> (images/g2p.webp)
[excerpt]: <> (If you have a T-Beam or LoRa32 device handy, it is very easy to get it set up for all the things that the RNode firmware allows you to do.)
<div class="article_date">{DATE}</div>
# Installing RNode Firmware on Supported Devices

Do you have one of the devices available that the RNode Firmware supports? In that case, it is very easy to turn it into a working RNode by using the `rnodeconf` autoinstaller.

With the firmware installed, you can use your newly created RNode as:

- A [LoRa interface for Reticulum]({ASSET_PATH}m/interfaces.html#rnode-lora-interface)
- A LoRa packet sniffer with [LoRaMon](https://unsigned.io/loramon/)
- A Linux network interface using the [tncattach program]({ASSET_PATH}pkg/tncattach.zip)
- A LoRa-based TNC for almost any amateur radio packet application

So let's get started! You will need either a **LilyGO T-Beam v1.1**, a **LilyGO LoRa32 v2.0**, a **LilyGO LoRa32 v2.1** or a **Heltec LoRa32 v2** device. More supported devices are added regularly, so it might be useful to check the latest [list of supported devices]({ASSET_PATH}supported.html) as well.

It is currently recommended to use one of the following devices: A **LilyGO LoRa32 v2.1** (also known as **TTGO T3 v1.6.1**) or a **LilyGO T-Beam v1.1**.

![Compatible LoRa devices]({ASSET_PATH}images/g2p.webp)
<center>*Some of the device types compatible with this installation guide*</center>

## Device Variations

Some devices come with transceiver chips that are currently unsupported by the RNode Firmware. Currently devices with an **SX1276** or **SX1278** chip are supported. Support for **SX1262**, **SX1268** and **SX1280** is being added. Please support the development with [donations]({ASSET_PATH}donate.html), if you would like to see these chips supported.

> **Beware!** Some devices, like the T-Beam, use SiLabs USB chips. These may need [additional drivers](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) to work well on macOS and Windows. Linux usually has up-to-date drivers pre-installed. The SiLabs driver may also experience conflicts with earlier, pre-installed versions of the driver, causing a *resource busy* error, which can be fixed by [removing the old driver](https://community.platformio.org/t/mac-usb-port-detected-but-won-t-upload/20663/2).

## Preparations

To get started, you will need to install at least version 2.1.0 of the [RNode Configuration Utility]({ASSET_PATH}m/using.html#the-rnodeconf-utility). 

The `rnodeconf` program is included in the `rns` package. Please read [these instructions]({ASSET_PATH}s_rns.html) for more information on how to install it from this repository, or from the Internet. If installation goes well, you can now move on to the next step.

## Install The Firmware

We are now ready to start installing the firmware. To install the RNode Firmware on your devices, run the RNode autoinstaller using this command:

```txt
rnodeconf --autoinstall
```

The installer will now ask you to insert the device you want to set up, scan for connected serial ports, and ask you a number of questions regarding the device. When it has the information it needs, it will install the correct firmware and configure the necessary parameters in the device EEPROM for it to function properly.

If the install goes well, you will be greated with a success message telling you that your device is now ready.

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

## Verify Installation
To confirm everything is OK, you can query the device info with:

```txt
rnodeconf --info /dev/ttyUSB0
```

Remember to replace `/dev/ttyUSB0` with the actual port the installer used in the previous step. You should now see `rnodeconf` connect to your device and show something like this:

```txt
[20:11:22] Opening serial port /dev/ttyUSB0...
[20:11:25] Device connected
[20:11:25] Current firmware version: 1.26
[20:11:25] Reading EEPROM...
[20:11:25] EEPROM checksum correct
[20:11:25] Device signature validated
[20:11:25]
[20:11:25] Device info:
[20:11:25]   Product            : LilyGO LoRa32 v2.0 850 - 950 MHz (b0:b8:36)
[20:11:25]   Device signature   : Validated - Local signature
[20:11:25]   Firmware version   : 1.26
[20:11:25]   Hardware revision  : 1
[20:11:25]   Serial number      : 00:00:00:02
[20:11:25]   Frequency range    : 850.0 MHz - 950.0 MHz
[20:11:25]   Max TX power       : 17 dBm
[20:11:25]   Manufactured       : 2022-01-27 20:10:32
[20:11:25]   Device mode        : Normal (host-controlled)
```

On the hardware side, you should see the status LED flashing briefly approximately every 2 seconds. If all of the above checks out, congratulations! Your RNode is now ready to use. If your device has a display, it should also come alive and show you various information related to the device state.

If you want to use it with [Reticulum]({ASSET_PATH}s_rns.html), [Nomad Network]({ASSET_PATH}s_nn.html), [LoRaMon](https://unsigned.io/loramon), or other such applications, leave it in the default `Normal (host-controlled)` mode.

If you want to use it with legacy amateur radio applications that work with KISS TNCs, you should [set it up in TNC mode]({ASSET_PATH}guides/tnc_mode.html).

## External RGB LED
If you are using a **LilyGO LoRa32 v2.1** device, you can connect an external **NeoPixel RGB LED** for device status using the following setup:

- Connect the NeoPixel **V+** pin to the **3.3v** pin on the board.
- Connect the NeoPixel **GND** pin to the **GND** pin on the board.
- Connect the NeoPixel **DATA** pin to **IO Pin 12** on the board.

For the firmware to activate the NeoPixel LED, you must also make specific choices in the autoinstaller guide:

- When asked what type of device you have, select **A specific kind of RNode**.
- When asked what model the device is, select the **Handheld v2.x RNode** that matches the frequency of your board.

## External Display & LEDs
If you are using a **LilyGO T-Beam** device, you can connect an external **SSD1306 OLED** display using the following setup:

- The **SSD1306**-based display must be set to use **I2C** and address `0x3D`
- Connect display **GND** to T-Beam **GND**
- Connect display **Vin** to suitable power-supplying pin on the T-Beam
- Connect display **RST** to T-Beam **Pin 13**
- Connect display **I2C CLK** to T-Beam **SCL** / **Pin 22**
- Connect display **I2C DATA** to T-Beam **SDA** / **Pin 21**

On **T-Beam** devices, you can also connect external RX/TX LEDs to **Pin 2** and **Pin 4**.
