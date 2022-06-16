prep:
	arduino-cli core update-index --config-file arduino-cli.yaml
	arduino-cli core install unsignedio:avr

prep-esp32:
	arduino-cli core update-index --config-file arduino-cli.yaml
	arduino-cli core install esp32:esp32

prep-samd:
	arduino-cli core update-index --config-file arduino-cli.yaml
	arduino-cli core install adafruit:samd



firmware:
	arduino-cli compile --fqbn unsignedio:avr:rnode

firmware-tbeam:
	arduino-cli compile --fqbn esp32:esp32:t-beam --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x33\""

firmware-lora32_v20:
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x36\""

firmware-lora32_v21:
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x37\""

firmware-lora32_v20_extled:
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x36\" \"-DEXTERNAL_LEDS=true\""

firmware-lora32_v21_extled:
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x37\" \"-DEXTERNAL_LEDS=true\""

firmware-heltec32_v2:
	arduino-cli compile --fqbn esp32:esp32:heltec_wifi_lora_32_V2 --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x38\""

firmware-heltec32_v2_extled:
	arduino-cli compile --fqbn esp32:esp32:heltec_wifi_lora_32_V2 --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x38\" \"-DEXTERNAL_LEDS=true\""

firmware-rnode_ng_20:
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x40\""

firmware-rnode_ng_21:
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x41\""

firmware-featheresp32:
	arduino-cli compile --fqbn esp32:esp32:featheresp32 --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x34\""

firmware-genericesp32:
	arduino-cli compile --fqbn esp32:esp32:esp32 --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x35\""

firmware-mega2560:
	arduino-cli compile --fqbn arduino:avr:mega



upload:
	arduino-cli upload -p /dev/ttyUSB0 --fqbn unsignedio:avr:rnode

upload-tbeam:
	arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:t-beam

upload-lora32_v20:
	arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:ttgo-lora32

upload-lora32_v21:
	arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:ttgo-lora32

upload-heltec32_v2:
	arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:heltec_wifi_lora_32_V2

upload-rnode_ng_20:
	arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:ttgo-lora32

upload-featheresp32:
	arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:featheresp32

upload-mega2560:
	arduino-cli upload -p /dev/ttyACM0 --fqbn arduino:avr:mega



release-all: release-rnode release-tbeam release-lora32_v20 release-lora32_v21 release-lora32_v20_extled release-lora32_v21_extled release-featheresp32 release-genericesp32 release-heltec32_v2 release-heltec32_v2_extled

release-rnode:
	arduino-cli compile --fqbn unsignedio:avr:rnode -e
	cp build/unsignedio.avr.rnode/RNode_Firmware.ino.hex Release/rnode_firmware_latest.hex
	rm -r build

release-tbeam:
	arduino-cli compile --fqbn esp32:esp32:t-beam -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x33\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/2.0.2/tools/partitions/boot_app0.bin build/rnode_firmware_latest_tbeam.boot_app0
	cp build/esp32.esp32.t-beam/RNode_Firmware.ino.bin build/rnode_firmware_latest_tbeam.bin
	cp build/esp32.esp32.t-beam/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_latest_tbeam.bootloader
	cp build/esp32.esp32.t-beam/RNode_Firmware.ino.partitions.bin build/rnode_firmware_latest_tbeam.partitions
	zip --junk-paths ./Release/rnode_firmware_latest_tbeam.zip ./Release/esptool/esptool.py build/rnode_firmware_latest_tbeam.boot_app0 build/rnode_firmware_latest_tbeam.bin build/rnode_firmware_latest_tbeam.bootloader build/rnode_firmware_latest_tbeam.partitions
	rm -r build

release-lora32_v20:
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x36\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/2.0.2/tools/partitions/boot_app0.bin build/rnode_firmware_latest_lora32v20.boot_app0
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bin build/rnode_firmware_latest_lora32v20.bin
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_latest_lora32v20.bootloader
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.partitions.bin build/rnode_firmware_latest_lora32v20.partitions
	zip --junk-paths ./Release/rnode_firmware_latest_lora32v20.zip ./Release/esptool/esptool.py build/rnode_firmware_latest_lora32v20.boot_app0 build/rnode_firmware_latest_lora32v20.bin build/rnode_firmware_latest_lora32v20.bootloader build/rnode_firmware_latest_lora32v20.partitions
	rm -r build

release-lora32_v21:
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x37\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/2.0.2/tools/partitions/boot_app0.bin build/rnode_firmware_latest_lora32v21.boot_app0
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bin build/rnode_firmware_latest_lora32v21.bin
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_latest_lora32v21.bootloader
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.partitions.bin build/rnode_firmware_latest_lora32v21.partitions
	zip --junk-paths ./Release/rnode_firmware_latest_lora32v21.zip ./Release/esptool/esptool.py build/rnode_firmware_latest_lora32v21.boot_app0 build/rnode_firmware_latest_lora32v21.bin build/rnode_firmware_latest_lora32v21.bootloader build/rnode_firmware_latest_lora32v21.partitions
	rm -r build

release-lora32_v20_extled:
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x36\" \"-DEXTERNAL_LEDS=true\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/2.0.2/tools/partitions/boot_app0.bin build/rnode_firmware_latest_lora32v20.boot_app0
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bin build/rnode_firmware_latest_lora32v20.bin
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_latest_lora32v20.bootloader
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.partitions.bin build/rnode_firmware_latest_lora32v20.partitions
	zip --junk-paths ./Release/rnode_firmware_latest_lora32v20_extled.zip ./Release/esptool/esptool.py build/rnode_firmware_latest_lora32v20.boot_app0 build/rnode_firmware_latest_lora32v20.bin build/rnode_firmware_latest_lora32v20.bootloader build/rnode_firmware_latest_lora32v20.partitions
	rm -r build

release-lora32_v21_extled:
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x37\" \"-DEXTERNAL_LEDS=true\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/2.0.2/tools/partitions/boot_app0.bin build/rnode_firmware_latest_lora32v21.boot_app0
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bin build/rnode_firmware_latest_lora32v21.bin
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_latest_lora32v21.bootloader
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.partitions.bin build/rnode_firmware_latest_lora32v21.partitions
	zip --junk-paths ./Release/rnode_firmware_latest_lora32v21_extled.zip ./Release/esptool/esptool.py build/rnode_firmware_latest_lora32v21.boot_app0 build/rnode_firmware_latest_lora32v21.bin build/rnode_firmware_latest_lora32v21.bootloader build/rnode_firmware_latest_lora32v21.partitions
	rm -r build

##################################
release-heltec32_v2:
	arduino-cli compile --fqbn esp32:esp32:heltec_wifi_lora_32_V2 -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x38\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/2.0.2/tools/partitions/boot_app0.bin build/rnode_firmware_latest_heltec32v2.boot_app0
	cp build/esp32.esp32.heltec_wifi_lora_32_V2/RNode_Firmware.ino.bin build/rnode_firmware_latest_heltec32v2.bin
	cp build/esp32.esp32.heltec_wifi_lora_32_V2/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_latest_heltec32v2.bootloader
	cp build/esp32.esp32.heltec_wifi_lora_32_V2/RNode_Firmware.ino.partitions.bin build/rnode_firmware_latest_heltec32v2.partitions
	zip --junk-paths ./Release/rnode_firmware_latest_heltec32v2.zip ./Release/esptool/esptool.py build/rnode_firmware_latest_heltec32v2.boot_app0 build/rnode_firmware_latest_heltec32v2.bin build/rnode_firmware_latest_heltec32v2.bootloader build/rnode_firmware_latest_heltec32v2.partitions
	rm -r build

release-heltec32_v2_extled:
	arduino-cli compile --fqbn esp32:esp32:heltec_wifi_lora_32_V2 -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x38\" \"-DEXTERNAL_LEDS=true\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/2.0.2/tools/partitions/boot_app0.bin build/rnode_firmware_latest_heltec32v2.boot_app0
	cp build/esp32.esp32.heltec_wifi_lora_32_V2/RNode_Firmware.ino.bin build/rnode_firmware_latest_heltec32v2.bin
	cp build/esp32.esp32.heltec_wifi_lora_32_V2/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_latest_heltec32v2.bootloader
	cp build/esp32.esp32.heltec_wifi_lora_32_V2/RNode_Firmware.ino.partitions.bin build/rnode_firmware_latest_heltec32v2.partitions
	zip --junk-paths ./Release/rnode_firmware_latest_heltec32v2.zip ./Release/esptool/esptool.py build/rnode_firmware_latest_heltec32v2.boot_app0 build/rnode_firmware_latest_heltec32v2.bin build/rnode_firmware_latest_heltec32v2.bootloader build/rnode_firmware_latest_heltec32v2.partitions
	rm -r build

release-rnode_ng_20:
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x40\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/2.0.2/tools/partitions/boot_app0.bin build/rnode_firmware_latest_ng20.boot_app0
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bin build/rnode_firmware_latest_ng20.bin
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_latest_ng20.bootloader
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.partitions.bin build/rnode_firmware_latest_ng20.partitions
	zip --junk-paths ./Release/rnode_firmware_latest_ng20.zip ./Release/esptool/esptool.py build/rnode_firmware_latest_ng20.boot_app0 build/rnode_firmware_latest_ng20.bin build/rnode_firmware_latest_ng20.bootloader build/rnode_firmware_latest_ng20.partitions
	rm -r build

release-rnode_ng_21:
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x41\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/2.0.2/tools/partitions/boot_app0.bin build/rnode_firmware_latest_ng21.boot_app0
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bin build/rnode_firmware_latest_ng21.bin
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_latest_ng21.bootloader
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.partitions.bin build/rnode_firmware_latest_ng21.partitions
	zip --junk-paths ./Release/rnode_firmware_latest_ng21.zip ./Release/esptool/esptool.py build/rnode_firmware_latest_ng21.boot_app0 build/rnode_firmware_latest_ng21.bin build/rnode_firmware_latest_ng21.bootloader build/rnode_firmware_latest_ng21.partitions
	rm -r build
#################################

release-featheresp32:
	arduino-cli compile --fqbn esp32:esp32:featheresp32 -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x34\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/2.0.2/tools/partitions/boot_app0.bin build/rnode_firmware_latest_featheresp32.boot_app0
	cp build/esp32.esp32.featheresp32/RNode_Firmware.ino.bin build/rnode_firmware_latest_featheresp32.bin
	cp build/esp32.esp32.featheresp32/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_latest_featheresp32.bootloader
	cp build/esp32.esp32.featheresp32/RNode_Firmware.ino.partitions.bin build/rnode_firmware_latest_featheresp32.partitions
	zip --junk-paths ./Release/rnode_firmware_latest_featheresp32.zip ./Release/esptool/esptool.py build/rnode_firmware_latest_featheresp32.boot_app0 build/rnode_firmware_latest_featheresp32.bin build/rnode_firmware_latest_featheresp32.bootloader build/rnode_firmware_latest_featheresp32.partitions
	rm -r build

release-genericesp32:
	arduino-cli compile --fqbn esp32:esp32:esp32 -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x35\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/2.0.2/tools/partitions/boot_app0.bin build/rnode_firmware_latest_esp32_generic.boot_app0
	cp build/esp32.esp32.esp32/RNode_Firmware.ino.bin build/rnode_firmware_latest_esp32_generic.bin
	cp build/esp32.esp32.esp32/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_latest_esp32_generic.bootloader
	cp build/esp32.esp32.esp32/RNode_Firmware.ino.partitions.bin build/rnode_firmware_latest_esp32_generic.partitions
	zip --junk-paths ./Release/rnode_firmware_latest_esp32_generic.zip ./Release/esptool/esptool.py build/rnode_firmware_latest_esp32_generic.boot_app0 build/rnode_firmware_latest_esp32_generic.bin build/rnode_firmware_latest_esp32_generic.bootloader build/rnode_firmware_latest_esp32_generic.partitions
	rm -r build

release-mega2560:
	arduino-cli compile --fqbn arduino:avr:mega -e
	cp build/arduino.avr.mega/RNode_Firmware.ino.hex Release/rnode_firmware_latest_m2560.hex
	rm -r build
