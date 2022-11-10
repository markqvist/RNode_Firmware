# RNode Firmware

This is the firmware for [RNode](https://unsigned.io/rnode), a very flexible LoRa-based Open Communications Device.

An RNode can function as a:

- General purpose long-range data radio module
- Network adapter for [Reticulum](https://github.com/markqvist/Reticulum)
- LoRa interface for your computer (or any device with a USB or serial port)
- Generic [LoRa-based network adapter](https://unsigned.io/15-kilometre-ssh-link-with-rnode/)
- [Packet sniffer](https://github.com/markqvist/LoRaMon) for LoRa networks
- LoRa-based KISS-compatible TNC for amateur radio and AX.25 applications
- Flexible platform for experiementing with LoRa technology

To install this firmware on homebrew RNodes and other supported boards, use `rnodeconf`, the *RNode Config Utility*. For guides and tutorials on how to get started with making your own RNodes, visit [unsigned.io](https://unsigned.io).

![Devices Running RNode Firmware](https://github.com/markqvist/RNode_Firmware/raw/master/Documentation/rnfw_1.jpg)

## Get Started

You can download and flash the firmware to all the supported boards using the [RNode Config Utility](https://github.com/markqvist/rnodeconfigutil). All firmware releases are now handled and installed directly through the `rnodeconf` utility, which is inclueded in the `rns` package. It can be installed via `pip`:

```
# Install rnodeconf via rns package
pip install rns --upgrade

# Install the firmware on a board with the install guide
rnodeconf --autoinstall
```

## Supported Hardware

The RNode Firmware supports the following boards:

- The original RNode from [unsigned.io](https://unsigned.io/)
- Homebrew RNodes based on ATmega1284p boards
- Homebrew RNodes based on ATmega2560 boards
- Homebrew RNodes based on Adafruit Feather ESP32 boards
- Homebrew RNodes based on generic ESP32 boards
- LilyGO T-Beam v1.1 devices
- LilyGO LoRa32 v2.0 devices
- LilyGO LoRa32 v2.1 devices
- Heltec LoRa32 v2 devices

## Operating Modes
RNode can operate in two modes, host-controlled (default) and TNC mode:

- When RNode is in host-controlled mode, it will stay in standby when powered on, until the host specifies frequency, bandwidth, transmit power and other required parameters. This mode can be enabled by using the -N option of this utility. In host-controlled mode, promiscuous mode can be activated to sniff any LoRa frames.

- When RNode is in TNC mode, it will configure itself on powerup and enable the radio immediately. This mode can be enabled by using the -T option of this utility (the utility will guide you through the settings if you don't specify them directly).

## Programming Libraries
Have a look in the "Libraries" folder for includes to let you easily use RNode in your own software.

Here's a Python example:

```python
from RNode import RNodeInterface

def gotPacket(data, rnode):
	print "Received a packet: "+data

rnode = RNodeInterface(
	callback = gotPacket,
	name = "My RNode",
	port = "/dev/ttyUSB0",
	frequency = 868000000,
	bandwidth = 125000,
	txpower = 2,
	sf = 7,
	cr = 5,
	loglevel = RNodeInterface.LOG_DEBUG)

rnode.send("Hello World!")
```

## Promiscuous Mode and LoRa Sniffing
RNode can be put into LoRa promiscuous mode, which will dump raw LoRa frames to the host. Raw LoRa frames can also be sent in this mode, and have the standard LoRa payload size of 255 bytes. To enable promiscuous mode send the "Promiscuous" command to the board, or use one of the programming libraries. You can also use the example program [LoRaMon](https://github.com/markqvist/LoRaMon) for an easy to use LoRa packet sniffer.

## Support RNode Development
You can help support the continued development of open, free and private communications systems by donating via one of the following channels:

- Monero:
  ```
  84FpY1QbxHcgdseePYNmhTHcrgMX4nFfBYtz2GKYToqHVVhJp8Eaw1Z1EedRnKD19b3B8NiLCGVxzKV17UMmmeEsCrPyA5w
  ```
- Ethereum
  ```
  0x81F7B979fEa6134bA9FD5c701b3501A2e61E897a
  ```
- Bitcoin
  ```
  3CPmacGm34qYvR6XWLVEJmi2aNe3PZqUuq
  ```
- Ko-Fi: https://ko-fi.com/markqvist

## License & Use
The RNode Firmware is Copyright © 2022 Mark Qvist / unsigned.io, and is made available under a dual GPL+commercial license.

Unless otherwise noted, the source code and binaries in this repository and related release sections is licensed under the **GNU General Public License v3.0**. The source code includes an SX1276 driver that is released under MIT License, and Copyright © 2018 Sandeep Mistry / Mark Qvist.

Permission is hereby granted to use the RNode Firmware in *binary form from this repository* for any and all purposes, ***so long as no payment or compensation is charged or received for such distribution or use***. Binary copies of the firmware obtained from this repository (or via the `rnodeconf` utility) are explicitly licensed under these conditions.

If you distribute or modify this work, you **must** adhere to the terms of the GPLv3, including, but not limited to, providing up-to-date source code upon distribution, displaying appropriate copyright and license notices in prominent positions of all conveyed works, and making users aware of their rights to the software under the GPLv3.

If you want to charge money (or similar) or receive compensation for providing the firmware to others, use the firmware in your own commercial products, or make and sell RNodes, you simplify commercial deployment by purchasing a commercial license, which is cheap and easy.

I provide a range of convenient tools and resources for anyone who wants commercially produce RNodes or provide services based on RNode, so just get in contact with me to get started.