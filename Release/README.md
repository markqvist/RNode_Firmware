# Precompiled Firmware
You can download and flash the firmware to supported boards using the [RNode Config Utility](https://github.com/markqvist/rnodeconfigutil). All firmware releases are now handled and installed directly through `rnodeconf`, which is inclueded in the `rns` package. It can be installed via `pip`:

```
# Install rnodeconf via rns package
pip install rns --upgrade

# Install the firmware on a board with the install guide
rnodeconf --autoinstall
```
