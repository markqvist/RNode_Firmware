# Building RNode

Using the schematics here, it is possible to build an RNode yourself. Schematics are supplied in PDF and Eagle format. Part list is in RNode_BOM.csv. It's also possible to breadboard an RNode using the following parts:

- An Arduino
- A TXB0801 logic level converter
- A 3.3v regulated DC supply (for supplying the LoRa chip)
- A Semtech SX1276 module

You can assemble the circuit like in the schematic, but only care about the connections going between the MCU and logic level converter, and from the logic level converter to the LoRa module, plus the supply lines, and optionally the LEDs.

Fritzing files for breadboard setup and links to recommended components are coming soon.