# RNode Firmware

This is the open firmware that powers RNode devices.

An RNode is an open, free and unrestricted digital radio transceiver. It enables anyone to send and receive any kind of data over both short and very long distances. RNodes can be used with many different kinds of programs and systems, but they are especially well suited for use with [Reticulum](https://reticulum.network).

RNode is not a product, and not any *one* specific device in particular. It is a system that is easy to replicate across space and time, that produces highly functional communications tools, which respects user autonomy and empowers individuals and communities to protect their sovereignty, privacy and ability to communicate and exchange data and ideas freely.

<img src="Documentation/images/rnv21_bgp.webp" width="100%">

<center><i>An RNode made from readily available and cheap parts, in a durable 3D printed case</i></center><br/><br/>

The RNode system is primarily software, which *transforms* different kinds of available hardware devices into functional, physical RNodes, which can then be used to solve a wide range of communications tasks. Such RNodes can be modified and built to suit the specific time, locale and environment they need to exist in.

## Latest Release

The latest release, installable through `rnodeconf`, is version `1.55`. This release brings the following changes:

- Added the **RNode Bootstrap Console** to all WiFi-capable RNodes. All such RNodes now contain a complete repository of information, software, guides and tools for replicating the RNode desing, and creating more RNodes.
- RNodes can now supply other RNodes with firmware images, with allows creating RNodes and bootstrapping communication networks, even if the Internet is unavailable.
- Improved RGB LED handling.
- Improved power management and charge status handling.
- Improved Bluetooth handling.

You must have at least version `2.1.0` of `rnodeconf` installed to update the RNode Firmware to version `1.55`. Get it by updating the `rns` package to at least version `0.4.7`.

## A Self-Replicating System

If you notice the presence of a circularity in the naming of the system as a whole, and the physical devices, it is no coincidence. Every RNode contains the seeds necessary to reproduce the system, the [RNode Bootstrap Console](/rnode_bootstrap_console), which is hosted locally on every RNode, and can be activated and accesses at any time - no Internet required.

The designs, guides and software stored within allows users to create more RNodes, and even to bootstrap entire communications networks, completely independently of existing infrastructure, or in situations where infrastructure has become unreliable or is broken.

<img src="Documentation/images/126dcfe92fb7.webp" width="100%"/>

<center><i>Where there is no Internet, RNodes will still communicate</i></center><br/><br/>

The production of one particular RNode device is not an end, but the potential starting point of a new branch of devices on the tree of the RNode system as a whole.

This tree fits into the larger biome of Free & Open Communications Systems, which I hope that you - by using communications tools like RNode - will help grow and prosper.

## One Tool, Many Uses

The RNode design is meant to flexible and hackable. At it's core, it is a low-power, but extremely long-range digital radio transceiver. Coupled with Reticulum, it provides encrypted and secure communications.

Depeding on configuration, it can be used for local networking purposes, or to send data over very long distances. Once you have an RNode, there is a wide variety of possible uses:

- As a network adapter for [Reticulum](https://reticulum.network)
- Messaging using [Sideband](https://unsigned.io/software/Sideband.html)
- Information sharing and communication using [Nomad Network](https://unsigned.io/software/Nomad_Network.html)
- LoRa-based [KISS-compatible amateur radio TNC](https://unsigned.io/guides/2020_05_03_using_rnodes_with_amateur_radio_software.html)
- LoRa development platform
- [Packet sniffer](https://unsigned.io/software/LoRaMon.html) for LoRa networks
- Long range [Ethernet and IP network interface](https://unsigned.io/guides/2020_05_27_ethernet-and-ip-over-packet-radio-tncs.html) on Linux
- As a general-purpose long-range data radio

## Types & Performance

RNodes can be made in many different configurations, and can use many different radio bands, but they will generally operate in the **433 MHz**, **868 MHz**, **915 MHZ** and **2.4 GHz** bands. They will usually offer configurable on-air data speeds between just a **few hundred bits per second**, up to **a couple of megabits per second**. Maximum output power will depend on the transceiver and PA setup used, but will generally lie between **17 dbm** and **27 dBm**.

The RNode system has been designed to allow reliable systems for basic human communications, over very wide areas, while using very little power, being cheap to build, free to operate, and near impossible to censor.

While **speeds are lower** than WiFi, typical communication **ranges are many times higher**. Several kilometers can be acheived with usable bitrates, even in urban areas, and over **100 kilometers** can be achieved in line-of-sight conditions.

## Supported Boards & Devices
The RNode Firmware supports the following boards:

- Handheld v2.x RNodes from [unsigned.io](https://unsigned.io/shop/product/handheld-rnode)
- Original v1.x RNodes from [unsigned.io](https://unsigned.io/shop/product/rnode)
- LilyGO T-Beam v1.1 devices
- LilyGO LoRa32 v2.0 devices
- LilyGO LoRa32 v2.1 devices
- Heltec LoRa32 v2 devices
- Homebrew RNodes based on ATmega1284p boards
- Homebrew RNodes based on ATmega2560 boards
- Homebrew RNodes based on Adafruit Feather ESP32 boards
- Homebrew RNodes based on generic ESP32 boards

## Supported Transceiver Modules
The RNode Firmware supports all transceiver modules based on **Semtech SX1276** or **Semtech SX1278** chips, that have an **SPI interface** and expose the **DIO_0** interrupt pin from the chip.

Support for **SX1262**, **SX1268** and **SX1280** is being implemented. Please support the project with donations if you want this faster!

## Getting Started Fast
You can download and flash the firmware to all the supported boards using the [RNode Config Utility](https://github.com/markqvist/rnodeconfigutil). All firmware releases are now handled and installed directly through the `rnodeconf` utility, which is inclueded in the `rns` package. It can be installed via `pip`:

```
# Install rnodeconf via rns package
pip install rns --upgrade

# Install the firmware on a board with the install guide
rnodeconf --autoinstall
```

For more detailed instruction and in-depth guides, you can have a look at some of these resources:

- Create a [basic RNode from readily available development boards](https://unsigned.io/guides/2022_01_25_installing-rnode-firmware-on-supported-devices.html)
- Follow a complete build recipe for [making a handheld RNode](https://unsigned.io/guides/2023_01_14_Making_A_Handheld_RNode.html), like the one pictured above
- Learn the basics on how to [create and build your own RNode designs](https://unsigned.io/guides/2022_01_26_how-to-make-your-own-rnodes.html) from scratch
- Once you've got the hang of it, start [selling your RNodes](https://unsigned.io/sell_rnodes.html)

If you would rather just buy a pre-made unit, you can visit [my shop](https://unsigned.io/shop) and purchase my particular version of the [Handheld RNode](https://unsigned.io/shop/handheld-rnode/), which I can assure you is made to the highest quality, and with a lot of care.

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
The RNode Firmware is Copyright © 2023 Mark Qvist / [unsigned.io](https://unsigned.io), and is made available under a dual GPL+commercial license.

Unless otherwise noted, the source code and binaries in this repository and related release sections is licensed under the **GNU General Public License v3.0**. The source code includes an SX1276 driver that is released under MIT License, and Copyright © 2018 Sandeep Mistry / Mark Qvist.

The 3D-printable object files, mechanical design, illustrations and related material, and all guide and photo material is released under a **Creative Commons - Attribution - Noncommercial - Share Alike 4.0** license.

Permission is hereby granted to use the RNode Firmware in *binary form from this repository* for any and all purposes, ***so long as no payment or compensation is charged or received for such distribution or use***. Binary copies of the firmware obtained from this repository (or via the `rnodeconf` utility) are explicitly licensed under these conditions.

If you distribute or modify this work, you **must** adhere to the terms of the GPLv3, including, but not limited to, providing up-to-date source code upon distribution, displaying appropriate copyright and license notices in prominent positions of all conveyed works, and making users aware of their rights to the software under the GPLv3.

If you want to charge money (or similar) or receive compensation for providing the firmware to others, use the firmware in your own commercial products, or make and sell RNodes, you simplify commercial deployment by purchasing a commercial license, which is cheap and easy.

I provide a range of convenient tools and resources for anyone who wants commercially produce RNodes or provide services based on RNode, so just get in [contact with me](https://unsigned.io/contact/) to get started.
