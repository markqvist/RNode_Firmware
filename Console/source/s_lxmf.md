[title]: <> (LXMF)
## LXMF
LXMF is a simple and flexible messaging format and delivery protocol that allows a wide variety of implementations, while using as little bandwidth as possible. It is built on top of [Reticulum](https://reticulum.network) and offers zero-conf message routing, end-to-end encryption and Forward Secrecy, and can be transported over any kind of medium that Reticulum supports.

LXMF is efficient enough that it can deliver messages over extremely low-bandwidth systems such as packet radio or LoRa. Encrypted LXMF messages can also be encoded as QR-codes or text-based URIs, allowing completely analog *paper message* transport.

Installing this LXMF library allows other programs on your system, like Nomad Network, to use the LXMF messaging system. It also includes the `lxmd` program that you can use to run LXMF propagation nodes on your network.

**Local Installation**

If you do not have access to the Internet, or would prefer to install LXMF directly from this RNode, you can use the following instructions.

- If you do not have an Internet connection while installing make sure to install the [Reticulum](./s_rns.html) package first
- Download the [{PKG_BASE_lxmf}]({ASSET_PATH}{PKG_lxmf}) package from this RNode and unzip it
- Install it with the command `pip install ./{PKG_NAME_lxmf}`
- Verify the installed Reticulum version by running `lxmd --version`

**Online Installation**

If you are connected to the Internet, you can try to install the latest version of LXMF via the `pip` package manager.

- Install Nomad Network by running the command `pip install lxmf`
- Verify the installed Reticulum version by running `lxmd --version`
