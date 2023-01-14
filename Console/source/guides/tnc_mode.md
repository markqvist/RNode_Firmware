[date]: <> (2023-01-07)
[title]: <> (Using an RNode With Amateur Radio Software)
[image]: <> (images/g4p.webp)
[excerpt]: <> (If you want to use an RNode with amateur radio applications, like APRS or a packet radio BBS, you will need to put the device into TNC Mode. In this mode, an RNode will behave exactly like a KISS-compatible TNC, which will make it usable with any amateur radio software.)
<div class="article_date">{DATE}</div>
# Using an RNode With Amateur Radio Software

If you want to use an RNode with amateur radio applications, like APRS or a packet radio BBS, you will need to put the device into *TNC Mode*. In this mode, an RNode will behave exactly like a KISS-compatible TNC, which will make it usable with any amateur radio software that can talk to a KISS TNC over a serial port.

You can use the [RNode Configuration Utility]({ASSET_PATH}m/using.html#the-rnodeconf-utility) to change settings on your device, including putting it into TNC mode. 

The `rnodeconf` program is included in the `rns` package. Please read [these instructions]({ASSET_PATH}s_rns.html) for more information on how to install it from this repository, or from the Internet.

With the `rnodeconf` program installed, you can put your RNode into TNC mode simply by entering the command:

```
rnodeconf -T /dev/ttyUSB0
```

Remember to replace `/dev/ttyUSB0` with the actual port your RNode is connected to. The program will now ask you for the channel configuration parameters, like frequency, bandwidth, transmission power and so on. It is also possible to specify all the parameters at once on the command line, see the `rnodeconf --help` for information on how to do this.

That's all there is to it! Your RNode is now configured in TNC mode, and ready for use with amateur radio applications.
