[title]: <> (Reticulum)
## Reticulum
The cryptographic networking stack for building resilient networks anywhere. The vision of Reticulum is to allow anyone to operate their own sovereign communication networks, and to make it cheap and easy to cover vast areas with a myriad of independent, interconnectable and autonomous networks. Reticulum is Unstoppable Networks for The People.

<p align="center"><img width="30%" src="{ASSET_PATH}m/_static/rns_logo_512.png"></p>

This packages requires you have `python` and `pip` installed on your computer. This should come as standard on most operating systems released since 2020.

### Local Installation
If you do not have access to the Internet, or would prefer to install Reticulum directly from this RNode, you can use the following instructions.

- Download the [{PKG_BASE_rns}]({ASSET_PATH}{PKG_rns}) package from this RNode and unzip it
- Install it with the command `pip install ./{PKG_NAME_rns}`
- Verify the installed Reticulum version by running `rnstatus --version`

### Online Installation
If you are connected to the Internet, you can try to install the latest version of Reticulum via the `pip` package manager.

- Install Reticulum by running the command `pip install rns`
- Verify the installed Reticulum version by running `rnstatus --version`

### Dependencies
If the installation has problems resolving dependencies, first try installing the `python-cryptography`, `python-netifaces` and `python-pyserial` packages from your systems package manager.

If this fails, or is simply not possible in your situation, you can make the installation of Reticulum ignore the resolution of dependencies using the command:

`pip install --no-dependencies ./{PKG_NAME_rns}`

This will allow you to install Reticulum on systems, or in circumstances, where one or more dependencies cannot be resolved. This will most likely mean that some functionality will not be available, which may be a worthwhile tradeoff in some situations.

If you use this method of installation, it is essential to read the [Pure-Python Reticulum]({ASSET_PATH}m/gettingstartedfast.html#pure-python-reticulum) section of the Reticulum Manual, and to understand the potential security implications of this installation method.

For more detailed information, please read the entire [Getting Started section of the Reticulum Manual]({ASSET_PATH}m/gettingstartedfast.html).
