
firmware:
	arduino-cli compile --fqbn unsignedio:avr:rnode

upload:
	arduino-cli upload -p /dev/ttyUSB0 --fqbn unsignedio:avr:rnode
