all: release

clean:
	-rm -r ./build
	-rm ./Release/rnode_firmware*

prep: prep-avr prep-esp32 prep-samd

prep-avr:
	arduino-cli core update-index --config-file arduino-cli.yaml
	arduino-cli core install arduino:avr
	arduino-cli core install unsignedio:avr

prep-esp32:
	arduino-cli core update-index --config-file arduino-cli.yaml
	arduino-cli core install esp32:esp32
	arduino-cli lib install "Adafruit SSD1306"
	arduino-cli lib install "AXP202X_Library"
	arduino-cli lib install "Crypto"

prep-samd:
	arduino-cli core update-index --config-file arduino-cli.yaml
	arduino-cli core install adafruit:samd

console-site:
	make -C Console clean site

spiffs: console-site spiffs-image 

spiffs-image:
	python Release/esptool/spiffsgen.py 2031616 ./Console/build Release/spiffs.bin
# 	python Release/esptool/spiffsgen.py --obj-name-len 64 2031616 ./Console/build Release/spiffs.bin

upload-spiffs:
	@echo Deploying SPIFFS image...
	python ./Release/esptool/esptool.py --chip esp32 --port /dev/ttyACM1 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 4MB 0x210000 ./Release/spiffs.bin

firmware:
	arduino-cli compile --fqbn unsignedio:avr:rnode

firmware-mega2560:
	arduino-cli compile --fqbn arduino:avr:mega

firmware-tbeam:
	arduino-cli compile --fqbn esp32:esp32:t-beam -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x33\""

firmware-lora32_v20:
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x36\" \"-DEXTERNAL_LEDS=true\""

firmware-lora32_v21:
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x37\""

firmware-lora32_v21_extled:
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x37\" \"-DEXTERNAL_LEDS=true\""

firmware-heltec32_v2:
	arduino-cli compile --fqbn esp32:esp32:heltec_wifi_lora_32_V2 -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x38\""

firmware-heltec32_v2_extled:
	arduino-cli compile --fqbn esp32:esp32:heltec_wifi_lora_32_V2 -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x38\" \"-DEXTERNAL_LEDS=true\""

firmware-rnode_ng_20:
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x40\""

firmware-rnode_ng_21:
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x41\""

firmware-featheresp32:
	arduino-cli compile --fqbn esp32:esp32:featheresp32 -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x34\""

firmware-genericesp32:
	arduino-cli compile --fqbn esp32:esp32:esp32 -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x35\""


upload:
	arduino-cli upload -p /dev/ttyUSB1 --fqbn unsignedio:avr:rnode

upload-mega2560:
	arduino-cli upload -p /dev/ttyACM1 --fqbn arduino:avr:mega

upload-tbeam:
	arduino-cli upload -p /dev/ttyUSB1 --fqbn esp32:esp32:t-beam
	@sleep 1
	rnodeconf /dev/ttyUSB1 --firmware-hash $$(./partition_hashes ./build/esp32.esp32.t-beam/RNode_Firmware.ino.bin)

upload-lora32_v20:
	arduino-cli upload -p /dev/ttyUSB1 --fqbn esp32:esp32:ttgo-lora32
	@sleep 1
	rnodeconf /dev/ttyUSB1 --firmware-hash $$(./partition_hashes ./build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bin)

upload-lora32_v21:
	arduino-cli upload -p /dev/ttyACM1 --fqbn esp32:esp32:ttgo-lora32
	@sleep 1
	rnodeconf /dev/ttyACM1 --firmware-hash $$(./partition_hashes ./build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bin)

upload-heltec32_v2:
	arduino-cli upload -p /dev/ttyUSB1 --fqbn esp32:esp32:heltec_wifi_lora_32_V2
	@sleep 1
	rnodeconf /dev/ttyUSB1 --firmware-hash $$(./partition_hashes ./build/esp32.esp32.heltec_wifi_lora_32_V2/RNode_Firmware.ino.bin)

upload-rnode_ng_20:
	arduino-cli upload -p /dev/ttyUSB1 --fqbn esp32:esp32:ttgo-lora32
	@sleep 1
	rnodeconf /dev/ttyUSB1 --firmware-hash $$(./partition_hashes ./build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bin)

upload-rnode_ng_21:
	arduino-cli upload -p /dev/ttyACM1 --fqbn esp32:esp32:ttgo-lora32
	@sleep 1
	rnodeconf /dev/ttyACM1 --firmware-hash $$(./partition_hashes ./build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bin)

upload-featheresp32:
	arduino-cli upload -p /dev/ttyUSB1 --fqbn esp32:esp32:featheresp32
	@sleep 1
	rnodeconf /dev/ttyUSB1 --firmware-hash $$(./partition_hashes ./build/esp32.esp32.featheresp32/RNode_Firmware.ino.bin)


release: release-all

release-all: spiffs-image release-rnode release-mega2560 release-tbeam release-lora32_v20 release-lora32_v21 release-lora32_v20_extled release-lora32_v21_extled release-featheresp32 release-genericesp32 release-heltec32_v2 release-heltec32_v2_extled release-rnode_ng_20 release-rnode_ng_21 release-hashes

release-hashes:
	python ./release_hashes.py > ./Release/release.json

release-rnode:
	arduino-cli compile --fqbn unsignedio:avr:rnode -e
	cp build/unsignedio.avr.rnode/RNode_Firmware.ino.hex Release/rnode_firmware.hex
	rm -r build

release-tbeam:
	arduino-cli compile --fqbn esp32:esp32:t-beam -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x33\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/2.0.?/tools/partitions/boot_app0.bin build/rnode_firmware_tbeam.boot_app0
	cp build/esp32.esp32.t-beam/RNode_Firmware.ino.bin build/rnode_firmware_tbeam.bin
	cp build/esp32.esp32.t-beam/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_tbeam.bootloader
	cp build/esp32.esp32.t-beam/RNode_Firmware.ino.partitions.bin build/rnode_firmware_tbeam.partitions
	zip --junk-paths ./Release/rnode_firmware_tbeam.zip ./Release/esptool/esptool.py build/rnode_firmware_tbeam.boot_app0 build/rnode_firmware_tbeam.bin build/rnode_firmware_tbeam.bootloader build/rnode_firmware_tbeam.partitions
	rm -r build

release-lora32_v20:
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x36\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/2.0.?/tools/partitions/boot_app0.bin build/rnode_firmware_lora32v20.boot_app0
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bin build/rnode_firmware_lora32v20.bin
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_lora32v20.bootloader
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.partitions.bin build/rnode_firmware_lora32v20.partitions
	zip --junk-paths ./Release/rnode_firmware_lora32v20.zip ./Release/esptool/esptool.py build/rnode_firmware_lora32v20.boot_app0 build/rnode_firmware_lora32v20.bin build/rnode_firmware_lora32v20.bootloader build/rnode_firmware_lora32v20.partitions
	rm -r build

release-lora32_v21:
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x37\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/2.0.?/tools/partitions/boot_app0.bin build/rnode_firmware_lora32v21.boot_app0
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bin build/rnode_firmware_lora32v21.bin
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_lora32v21.bootloader
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.partitions.bin build/rnode_firmware_lora32v21.partitions
	zip --junk-paths ./Release/rnode_firmware_lora32v21.zip ./Release/esptool/esptool.py build/rnode_firmware_lora32v21.boot_app0 build/rnode_firmware_lora32v21.bin build/rnode_firmware_lora32v21.bootloader build/rnode_firmware_lora32v21.partitions
	rm -r build

release-lora32_v20_extled:
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x36\" \"-DEXTERNAL_LEDS=true\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/2.0.?/tools/partitions/boot_app0.bin build/rnode_firmware_lora32v20.boot_app0
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bin build/rnode_firmware_lora32v20.bin
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_lora32v20.bootloader
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.partitions.bin build/rnode_firmware_lora32v20.partitions
	zip --junk-paths ./Release/rnode_firmware_lora32v20_extled.zip ./Release/esptool/esptool.py build/rnode_firmware_lora32v20.boot_app0 build/rnode_firmware_lora32v20.bin build/rnode_firmware_lora32v20.bootloader build/rnode_firmware_lora32v20.partitions
	rm -r build

release-lora32_v21_extled:
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x37\" \"-DEXTERNAL_LEDS=true\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/2.0.?/tools/partitions/boot_app0.bin build/rnode_firmware_lora32v21.boot_app0
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bin build/rnode_firmware_lora32v21.bin
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_lora32v21.bootloader
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.partitions.bin build/rnode_firmware_lora32v21.partitions
	zip --junk-paths ./Release/rnode_firmware_lora32v21_extled.zip ./Release/esptool/esptool.py build/rnode_firmware_lora32v21.boot_app0 build/rnode_firmware_lora32v21.bin build/rnode_firmware_lora32v21.bootloader build/rnode_firmware_lora32v21.partitions
	rm -r build

release-heltec32_v2:
	arduino-cli compile --fqbn esp32:esp32:heltec_wifi_lora_32_V2 -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x38\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/2.0.?/tools/partitions/boot_app0.bin build/rnode_firmware_heltec32v2.boot_app0
	cp build/esp32.esp32.heltec_wifi_lora_32_V2/RNode_Firmware.ino.bin build/rnode_firmware_heltec32v2.bin
	cp build/esp32.esp32.heltec_wifi_lora_32_V2/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_heltec32v2.bootloader
	cp build/esp32.esp32.heltec_wifi_lora_32_V2/RNode_Firmware.ino.partitions.bin build/rnode_firmware_heltec32v2.partitions
	zip --junk-paths ./Release/rnode_firmware_heltec32v2.zip ./Release/esptool/esptool.py build/rnode_firmware_heltec32v2.boot_app0 build/rnode_firmware_heltec32v2.bin build/rnode_firmware_heltec32v2.bootloader build/rnode_firmware_heltec32v2.partitions
	rm -r build

release-heltec32_v2_extled:
	arduino-cli compile --fqbn esp32:esp32:heltec_wifi_lora_32_V2 -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x38\" \"-DEXTERNAL_LEDS=true\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/2.0.?/tools/partitions/boot_app0.bin build/rnode_firmware_heltec32v2.boot_app0
	cp build/esp32.esp32.heltec_wifi_lora_32_V2/RNode_Firmware.ino.bin build/rnode_firmware_heltec32v2.bin
	cp build/esp32.esp32.heltec_wifi_lora_32_V2/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_heltec32v2.bootloader
	cp build/esp32.esp32.heltec_wifi_lora_32_V2/RNode_Firmware.ino.partitions.bin build/rnode_firmware_heltec32v2.partitions
	zip --junk-paths ./Release/rnode_firmware_heltec32v2.zip ./Release/esptool/esptool.py build/rnode_firmware_heltec32v2.boot_app0 build/rnode_firmware_heltec32v2.bin build/rnode_firmware_heltec32v2.bootloader build/rnode_firmware_heltec32v2.partitions
	rm -r build

release-rnode_ng_20:
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x40\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/2.0.?/tools/partitions/boot_app0.bin build/rnode_firmware_ng20.boot_app0
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bin build/rnode_firmware_ng20.bin
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_ng20.bootloader
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.partitions.bin build/rnode_firmware_ng20.partitions
	zip --junk-paths ./Release/rnode_firmware_ng20.zip ./Release/esptool/esptool.py build/rnode_firmware_ng20.boot_app0 build/rnode_firmware_ng20.bin build/rnode_firmware_ng20.bootloader build/rnode_firmware_ng20.partitions
	rm -r build

release-rnode_ng_21:
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=1966080" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x41\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/2.0.?/tools/partitions/boot_app0.bin build/rnode_firmware_ng21.boot_app0
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bin build/rnode_firmware_ng21.bin
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_ng21.bootloader
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.partitions.bin build/rnode_firmware_ng21.partitions
	zip --junk-paths ./Release/rnode_firmware_ng21.zip ./Release/esptool/esptool.py build/rnode_firmware_ng21.boot_app0 build/rnode_firmware_ng21.bin build/rnode_firmware_ng21.bootloader build/rnode_firmware_ng21.partitions
	rm -r build

release-featheresp32:
	arduino-cli compile --fqbn esp32:esp32:featheresp32 -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x34\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/2.0.?/tools/partitions/boot_app0.bin build/rnode_firmware_featheresp32.boot_app0
	cp build/esp32.esp32.featheresp32/RNode_Firmware.ino.bin build/rnode_firmware_featheresp32.bin
	cp build/esp32.esp32.featheresp32/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_featheresp32.bootloader
	cp build/esp32.esp32.featheresp32/RNode_Firmware.ino.partitions.bin build/rnode_firmware_featheresp32.partitions
	zip --junk-paths ./Release/rnode_firmware_featheresp32.zip ./Release/esptool/esptool.py build/rnode_firmware_featheresp32.boot_app0 build/rnode_firmware_featheresp32.bin build/rnode_firmware_featheresp32.bootloader build/rnode_firmware_featheresp32.partitions
	rm -r build

release-genericesp32:
	arduino-cli compile --fqbn esp32:esp32:esp32 -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x35\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/2.0.?/tools/partitions/boot_app0.bin build/rnode_firmware_esp32_generic.boot_app0
	cp build/esp32.esp32.esp32/RNode_Firmware.ino.bin build/rnode_firmware_esp32_generic.bin
	cp build/esp32.esp32.esp32/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_esp32_generic.bootloader
	cp build/esp32.esp32.esp32/RNode_Firmware.ino.partitions.bin build/rnode_firmware_esp32_generic.partitions
	zip --junk-paths ./Release/rnode_firmware_esp32_generic.zip ./Release/esptool/esptool.py build/rnode_firmware_esp32_generic.boot_app0 build/rnode_firmware_esp32_generic.bin build/rnode_firmware_esp32_generic.bootloader build/rnode_firmware_esp32_generic.partitions
	rm -r build

release-mega2560:
	arduino-cli compile --fqbn arduino:avr:mega -e
	cp build/arduino.avr.mega/RNode_Firmware.ino.hex Release/rnode_firmware_m2560.hex
	rm -r build