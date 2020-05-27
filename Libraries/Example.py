# This is a short example program that
# demonstrates the bare minimum of using
# RNode in a Python program. First we'll
# import the RNodeInterface class.
from RNode import RNodeInterface

# We'll also define which serial port the
# RNode is attached to.
serialPort = "/dev/ttyUSB0"
# TODO: Remove
serialPort = "/dev/tty.usbserial-DN03E0FQ"

# This function gets called every time a
# packet is received
def gotPacket(data, rnode):
	print("Received a packet: "+data)
	print("RSSI: "+str(rnode.r_stat_rssi)+" dBm")
	print("SNR:  "+str(rnode.r_stat_snr)+" dB")

# Create an RNode instance. This configures
# and powers up the radio.
rnode = RNodeInterface(
	callback = gotPacket,
	name = "My RNode",
	port = serialPort,
	frequency = 868000000,
	bandwidth = 125000,
	txpower = 2,
	sf = 7,
	cr = 5,
	loglevel = RNodeInterface.LOG_DEBUG)

# Enter a loop waiting for user input.
try:
	print("Waiting for packets, hit enter to send a packet, Ctrl-C to exit")
	while True:
		input()
		rnode.send("Hello World!")
except KeyboardInterrupt as e:
	print("")
	exit()