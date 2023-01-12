[title]: <> (Software)
# Software
This RNode contains a repository of downloadable software and utilities, that are useful for bootstrapping communications networks, and for replicating RNodes.

**Please Note!** Whenever you install software onto your computer, there is a risk that someone modified this software to include malicious code. Be extra careful installing anything from this RNode, if you did not get it from a source you trust, or if there is a risk it was modified in transit.

If possible, you can check that the `SHA-256` hashes of any downloaded files correspond to the list of release hashes published on the [Reticulum Release page](https://github.com/markqvist/Reticulum/releases).

**You Have The Source!** Due to the size limitations of shipping all this software within an RNode, we don't include separate source-code archives for the below programs, but *all the source code is included within the Python .whl files*! You can simply unzip any of them with any program that understands `zip` files, and you will find the source code inside the unzipped directory (for some zip programs, you may need to change the file ending to `.zip`).

## Reticulum
-------------
The cryptographic networking stack for building resilient networks anywhere. This packages requires you have `python` and `pip` installed on your computer. This should come as standard on most operating systems released since 2020.

**Local Installation**

If you do not have access to the Internet, or would prefer to install Reticulum directly from this RNode, you can use the following instructions.

- Download the [{PKG_rns}]({ASSET_PATH}{PKG_rns}) package from this RNode and unzip it
- Install it with the command `pip install {PKG_NAME_rns}`
- Verify the installed Reticulum version by running `rnstatus --version`

**Online Installation**

If you are connected to the Internet, you can try to install the latest version of Reticulum via the `pip` package manager.

- Install Reticulum by running the command `pip install rns`
- Verify the installed Reticulum version by running `rnstatus --version`

If the installation has problems resolving dependencies, you can try to install the `python-cryptography`, `python-netifaces` and `python-pyserial` packages from your systems package manager, if they are locally available. If this is not possible, you please read the [Getting Started section of the Reticulum Manual]({ASSET_PATH}manual/gettingstartedfast.html) for more detailed information.

## LXMF
-------------
LXMF is a simple and flexible messaging format and delivery protocol that allows a wide variety of implementations, while using as little bandwidth as possible. It is built on top of [Reticulum](https://reticulum.network) and offers zero-conf message routing, end-to-end encryption and Forward Secrecy, and can be transported over any kind of medium that Reticulum supports.

LXMF is efficient enough that it can deliver messages over extremely low-bandwidth systems such as packet radio or LoRa. Encrypted LXMF messages can also be encoded as QR-codes or text-based URIs, allowing completely analog *paper message* transport.

Installing this LXMF library allows other programs on your system, like Nomad Network, to use the LXMF messaging system. It also includes the `lxmd` program that you can use to run LXMF propagation nodes on your network.

**Local Installation**

If you do not have access to the Internet, or would prefer to install LXMF directly from this RNode, you can use the following instructions.

- Download the [{PKG_lxmf}]({ASSET_PATH}{PKG_lxmf}) package from this RNode and unzip it
- Install it with the command `pip install {PKG_NAME_lxmf}`
- Verify the installed Reticulum version by running `lxmd --version`

**Online Installation**

If you are connected to the Internet, you can try to install the latest version of LXMF via the `pip` package manager.

- Install Nomad Network by running the command `pip install lxmf`
- Verify the installed Reticulum version by running `lxmd --version`

## Nomad Network
-------------
Off-grid, resilient mesh communication with strong encryption, forward secrecy and extreme privacy.

Nomad Network Allows you to build private and resilient communications platforms that are in complete control and ownership of the people that use them. No signups, no agreements, no handover of any data, no permissions and gatekeepers.

![Screenshot]({ASSET_PATH}gfx/nn.webp)

Nomad Network is build on [LXMF](lxmf.html) and [Reticulum]({ASSET_PATH}r/), which together provides the cryptographic mesh functionality and peer-to-peer message routing that Nomad Network relies on. This foundation also makes it possible to use the program over a very wide variety of communication mediums, from packet radio to fiber optics.

Nomad Network does not need any connections to the public internet to work. In fact, it doesn't even need an IP or Ethernet network. You can use it entirely over packet radio, LoRa or even serial lines. But if you wish, you can bridge islanded networks over the Internet or private ethernet networks, or you can build networks running completely over the Internet. The choice is yours.

**Local Installation**

If you do not have access to the Internet, or would prefer to install Nomad Network directly from this RNode, you can use the following instructions.

- Download the [{PKG_nomadnet}]({ASSET_PATH}{PKG_nomadnet}) package from this RNode and unzip it
- Install it with the command `pip install {PKG_NAME_nomadnet}`
- Verify the installed Reticulum version by running `nomadnet --version`

**Online Installation**

If you are connected to the Internet, you can try to install the latest version of Nomad Network via the `pip` package manager.

- Install Nomad Network by running the command `pip install nomadnet`
- Verify the installed Reticulum version by running `nomadnet --version`

## Sideband
-------------
Sideband is an LXMF client for Android, Linux and macOS. It has built-in support for communicating over RNodes, and many other mediums, such as Packet Radio, WiFi, I2P, or anything else Reticulum supports.

Sideband also supports exchanging messages through encrypted QR-codes on paper, or through messages embedded directly in lxm:// links.

![Screenshot]({ASSET_PATH}gfx/sideband.webp)

The installation files for the Sideband program is too large to be included on this RNode, but downloads for Linux, Android and macOS can be obtained from following sources:

- The [Sideband page](https://unsigned.io/sideband/) on [unsigned.io](https://unsigned.io/)
- The [GitHub release page for Sideband](https://github.com/markqvist/Sideband/releases/latest)
- The [IzzyOnDroid repository for F-Droid](https://android.izzysoft.de/repo/apk/io.unsigned.sideband)
