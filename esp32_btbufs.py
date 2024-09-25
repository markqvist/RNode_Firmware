#!/usr/bin/env python3
import sys

try:
    target_path = sys.argv[1]
    rxbuf_size = 0; rxbuf_minsize = 6144
    txbuf_size = 0; txbuf_minsize = 384
    line_index = 0
    rx_line_index = 0
    tx_line_index = 0
    with open(target_path) as sf:
        for line in sf:
            line_index += 1
            if line.startswith("#define RX_QUEUE_SIZE"):
                ents = line.split(" ")
                try:
                    rxbuf_size = int(ents[2])
                    rx_line_index = line_index
                except Exception as e:
                    print(f"Could not parse Bluetooth RX_QUEUE_SIZE: {e}")

            if line.startswith("#define TX_QUEUE_SIZE"):
                ents = line.split(" ")
                try:
                    txbuf_size = int(ents[2])
                    tx_line_index = line_index
                except Exception as e:
                    print(f"Could not parse Bluetooth RX_QUEUE_SIZE: {e}")

            if rxbuf_size != 0 and txbuf_size != 0:
                break

    if rxbuf_size < rxbuf_minsize:
        print(f"Error: The configured ESP32 Bluetooth RX buffer size is too small, please set it to at least {rxbuf_minsize} and try compiling again.")
        print(f"The buffer configuration can be modified in line {rx_line_index} of: {target_path}")
        exit(1)

    if txbuf_size < txbuf_minsize:
        print(f"Error: The configured ESP32 Bluetooth TX buffer size is too small, please set it to at least {txbuf_minsize} and try compiling again.")
        print(f"The buffer configuration can be modified in line {tx_line_index} of: {target_path}")
        exit(1)

    exit(0)

except Exception as e:
    print(f"Could not determine ESP32 Bluetooth buffer configuration: {e}")
    print("Please fix this error and try again")