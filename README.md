# RNode Firmware

This is the firmware for [RNode](https://unsigned.io/rnode), a very flexible LoRa-based communication device. RNode can functions as a:

- Network adapter for [RNS](https://github.com/markqvist/Reticulum)
- LoRa interface for your computer (or any device with a USB or serial port)
- Packet sniffer for LoRa networks
- LoRa development board
- A LoRa-based KISS-compatible TNC
- A flexible platform for experiementing with LoRa technology

RNode is controlled by a powerful ATmega1284p MCU, and is fully Arduino compatible. You can use this firmware, or it can be programmed any way you like, either from the Arduino IDE, or using any of the available tools for AVR development. This firmware can also be edited and compiled directly from the Arduino IDE.

For adding RNode to your Arduino environment, please see [this post](https://unsigned.io/board-support-in-arduino-ide/).

For configuring an RNode with this firmware, please have a look at the [RNode Config Utility](https://github.com/markqvist/rnodeconfigutil).

## Current Status
The RNode firmware has not seen much real world testing, and should be considered beta at this point.

## Operating Modes
RNode can operate in two modes, host-controlled (default) and TNC mode:

- When RNode is in host-controlled mode, it will stay in standby when powered on, until the host specifies frequency, bandwidth, transmit power and other required parameters. This mode can be enabled by using the -N option of this utility. In host-controlled mode, promiscuous mode can be activated to sniff any LoRa frames.

- When RNode is in TNC mode, it will configure itself on powerup and enable the radio immediately. This mode can be enabled by using the -T option of this utility (the utility will guide you through the settings if you don't specify them directly).

## USB and Serial Protocol
You can communicate with RNode either via the on-board USB connector, or using the serial pins on the board (labeled RX0 and TX0). RNode uses a standard FTDI USB chip, so it works out of the box without additional drivers in most operating systems.

All communications to and from the board uses [KISS framing](https://en.wikipedia.org/wiki/KISS_(TNC)) with a custom command set. RNode also does not use HDLC ports in the command byte, and as such uses the full 8 bits of the command byte is available for the actual command. Please see table below for supported commands.

| Command          | Byte | Description
| -----------------|------| -----------------------------------------------
| Data frame       | 0x00 | A data packet to or from the device
| Frequency        | 0x01 | Sets or queries the frequency
| Bandwidth        | 0x02 | Sets or queries the bandwidth
| TX Power         | 0x03 | Sets or queries the TX power
| Spreading Factor | 0x04 | Sets or queries the spreading factor
| Coding Rate      | 0x05 | Sets or queries the coding rate
| Radio State      | 0x06 | Sets or queries radio state
| Radio Lock       | 0x07 | Sets or queries the radio lock
| Device Detect    | 0x08 | Probe command for device detection
| Promiscuous      | 0x0E | Sets or queries promiscuous mode
| Ready            | 0x0F | Flow control command indicating ready for TX
| RX Stats         | 0x21 | Queries received bytes
| TX Stats         | 0x22 | Queries transmitted bytes
| Last RSSI        | 0x23 | Indicates RSSI of last packet received
| Blink            | 0x30 | Blinks LEDs
| Random           | 0x40 | Queries for a random number
| Firmware Version | 0x50 | Queries for installed firmware version
| ROM Read         | 0x51 | Read EEPROM byte
| ROM Write        | 0x52 | Write EEPROM byte
| TNC Mode         | 0x53 | Enables TNC mode
| Normal Mode      | 0x54 | Enables host-controlled mode
| ROM Erase        | 0x59 | Completely erases EEPROM
| Error            | 0x90 | Indicates an error

## Programming Libraries
Have a look in the "Libraries" folder for includes to let you easily use RNode in your own software.

Here's a Python example:

```python
import RNodeInterface

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
	loglevel = RnodeInterface.LOG_DEBUG)

rnode.send("Hello World!")
```
