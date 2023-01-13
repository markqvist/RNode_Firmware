[date]: <> (2023-01-07)
[title]: <> (Using RNodes With Amateur Radio Software)
[image]: <> (images/xastir2-e1643321757361-400x275.jpg)
[excerpt]: <> (If you want to use an RNode with amateur radio applications, like APRS or a packet radio BBS, you will need to put the device into TNC Mode. In this mode, an RNode will behave exactly like a KISS-compatible TNC, which will make it usable with any amateur radio software.)
<div class="article_date">{DATE}</div>
# Using RNodes With Amateur Radio Software

If you want to use an [RNode](https://unsigned.io/rnode/) with amateur radio applications, like [APRS](https://unsigned.io/aprs-over-lora-with-rnode/) or a packet radio BBS, you will need to put the device into *TNC Mode*. In this mode, an RNode will behave exactly like a KISS-compatible TNC, which will make it usable with any amateur radio software that can talk to a KISS TNC over a serial port.

Whether you RNode is [bought from my shop](https://unsigned.io/shop/product/rnode/), [made from a compatible LoRa board](https://unsigned.io/installing-rnode-firmware-on-supported-devices/) or [built by yourself](https://unsigned.io/how-to-make-your-own-rnodes/), you can use the [RNode Configuration Utility](https://unsigned.io/rnodeconf) to change settings on your device, including putting it into TNC mode.

The easiest way to install `rnodeconf` on your system is by installing the `rns` package using `pip`. You probably already have `python` and `pip` installed if you use a relatively recent version of Linux or macOS. If not, go and install Python 3 now. When that is done, you can simply install `rnodeconf` by opening up a terminal and typing:

```
pip install rns
```

After a few seconds, the program should be installed and ready to use. If this is the very first time you are installing something with `pip`, you might need to close your terminal and open it again, or in some cases restart your computer, before the `rnodeconf` command becomes available.

With the `rnodeconf` program installed, you can put your RNode into TNC mode simply by entering the command:

```
rnodeconf -T /dev/ttyUSB0
```

Remember to replace `/dev/ttyUSB0` with the actual port your RNode is connected to. The program will now ask you for the channel configuration parameters, like frequency, bandwidth, transmission power and so on. It is also possible to specify all the parameters at once on the command line, see the `rnodeconf --help` for information on how to do this.

That's all there is to it! Your RNode is now configured in TNC mode, and ready for use with amateur radio applications.
