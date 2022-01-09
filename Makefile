prep:
	arduino-cli core update-index --config-file arduino-cli.yaml
	arduino-cli core install unsignedio:avr

firmware:
	arduino-cli compile --fqbn unsignedio:avr:rnode

release:
	arduino-cli compile --fqbn unsignedio:avr:rnode -e
	cp build/unsignedio.avr.rnode/RNode_Firmware.ino.hex Precompiled/rnode_firmware_latest.hex
	rm -r build

upload:
	arduino-cli upload -p /dev/ttyUSB0 --fqbn unsignedio:avr:rnode

prep-tbeam:
	arduino-cli core update-index --config-file arduino-cli.yaml
	arduino-cli core install esp32:esp32

firmware-tbeam:
	arduino-cli compile --fqbn esp32:esp32:t-beam

upload-tbeam:
	arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:t-beam

release-tbeam:
	arduino-cli compile --fqbn esp32:esp32:t-beam -e
	cp build/esp32.esp32.t-beam/RNode_Firmware.ino.bin Precompiled/rnode_firmware_latest_tbeam.bin
	rm -r build