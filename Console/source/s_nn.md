[title]: <> (Nomad Network)
## Nomad Network
Off-grid, resilient mesh communication with strong encryption, forward secrecy and extreme privacy.

Nomad Network Allows you to build private and resilient communications platforms that are in complete control and ownership of the people that use them. No signups, no agreements, no handover of any data, no permissions and gatekeepers.

![Screenshot]({ASSET_PATH}gfx/nn.webp)

Nomad Network is build on [LXMF](lxmf.html) and [Reticulum]({ASSET_PATH}r/), which together provides the cryptographic mesh functionality and peer-to-peer message routing that Nomad Network relies on. This foundation also makes it possible to use the program over a very wide variety of communication mediums, from packet radio to fiber optics.

Nomad Network does not need any connections to the public internet to work. In fact, it doesn't even need an IP or Ethernet network. You can use it entirely over packet radio, LoRa or even serial lines. But if you wish, you can bridge islanded networks over the Internet or private ethernet networks, or you can build networks running completely over the Internet. The choice is yours.

### Local Installation

If you do not have access to the Internet, or would prefer to install Nomad Network directly from this RNode, you can use the following instructions.

- If you do not have an Internet connection while installing make sure to install the [Reticulum](./s_rns.html) and [LXMF](./s_lxmf.html) packages first
- Download the [{PKG_BASE_nomadnet}]({ASSET_PATH}{PKG_nomadnet}) package from this RNode and unzip it
- Install it with the command `pip install ./{PKG_NAME_nomadnet}`
- Verify the installed Nomad Network version by running `nomadnet --version`

### Online Installation

If you are connected to the Internet, you can try to install the latest version of Nomad Network via the `pip` package manager.

- Install Nomad Network by running the command `pip install nomadnet`
- Verify the installed Nomad Network version by running `nomadnet --version`
