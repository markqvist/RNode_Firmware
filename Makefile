# Copyright (C) 2024, Mark Qvist

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

# Version 2.0.17 of the Arduino ESP core is based on ESP-IDF v4.4.7
ARDUINO_ESP_CORE_VER = 2.0.17

all: release

clean:
	-rm -r ./build
	-rm ./Release/rnode_firmware*

prep: prep-avr prep-esp32 prep-samd

prep-avr:
	arduino-cli core update-index --config-file arduino-cli.yaml
	arduino-cli core install arduino:avr --config-file arduino-cli.yaml
	arduino-cli core install unsignedio:avr --config-file arduino-cli.yaml

prep-esp32:
	arduino-cli core update-index --config-file arduino-cli.yaml
	arduino-cli core install esp32:esp32@$(ARDUINO_ESP_CORE_VER) --config-file arduino-cli.yaml
	arduino-cli lib install "Adafruit SSD1306"
	arduino-cli lib install "Adafruit SH110X"
	arduino-cli lib install "Adafruit ST7735 and ST7789 Library"
	arduino-cli lib install "Adafruit NeoPixel"
	arduino-cli lib install "XPowersLib"
	arduino-cli lib install "Crypto"

prep-samd:
	arduino-cli core update-index --config-file arduino-cli.yaml
	arduino-cli core install adafruit:samd --config-file arduino-cli.yaml

prep-nrf:
	arduino-cli core update-index --config-file arduino-cli.yaml
	arduino-cli core install rakwireless:nrf52 --config-file arduino-cli.yaml
	arduino-cli core install Heltec_nRF52:Heltec_nRF52 --config-file arduino-cli.yaml
	arduino-cli core install adafruit:nrf52 --config-file arduino-cli.yaml
	arduino-cli lib install "GxEPD2"
	arduino-cli config set library.enable_unsafe_install true
	arduino-cli lib install --git-url https://github.com/liamcottle/esp8266-oled-ssd1306#e16cee124fe26490cb14880c679321ad8ac89c95
	pip install adafruit-nrfutil --upgrade

console-site:
	make -C Console clean site

spiffs: console-site spiffs-image 

spiffs-image:
	python Release/esptool/spiffsgen.py 1966080 ./Console/build Release/console_image.bin

upload-spiffs:
	@echo Deploying SPIFFS image...
	python ./Release/esptool/esptool.py --chip esp32 --port /dev/ttyACM0 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 4MB 0x210000 ./Release/console_image.bin

check_bt_buffers:
	@./esp32_btbufs.py ~/.arduino15/packages/esp32/hardware/esp32/$(ARDUINO_ESP_CORE_VER)/libraries/BluetoothSerial/src/BluetoothSerial.cpp

firmware:
	arduino-cli compile --log --fqbn unsignedio:avr:rnode

firmware-mega2560:
	arduino-cli compile --log --fqbn arduino:avr:mega

firmware-tbeam: check_bt_buffers
	arduino-cli compile --log --fqbn esp32:esp32:t-beam -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x33\""

firmware-tbeam_sx126x: check_bt_buffers
	arduino-cli compile --log --fqbn esp32:esp32:t-beam -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x33\" \"-DMODEM=0x03\""

firmware-t3s3:
	arduino-cli compile --log --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc" -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x42\" \"-DMODEM=0x03\""

firmware-t3s3_sx127x:
	arduino-cli compile --log --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc" -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x42\" \"-DMODEM=0x01\""

firmware-t3s3_sx1280_pa:
	arduino-cli compile --log --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc" -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x42\" \"-DMODEM=0x04\""

firmware-tdeck:
	arduino-cli compile --log --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc" -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x3B\""

firmware-tbeam_supreme:
	arduino-cli compile --log --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc" -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=-DBOARD_MODEL=0x3D"

firmware-lora32_v10: check_bt_buffers
	arduino-cli compile --log --fqbn esp32:esp32:ttgo-lora32 -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x39\""

firmware-lora32_v10_extled: check_bt_buffers
	arduino-cli compile --log --fqbn esp32:esp32:ttgo-lora32 -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x39\" \"-DEXTERNAL_LEDS=true\""

firmware-lora32_v20: check_bt_buffers
	arduino-cli compile --log --fqbn esp32:esp32:ttgo-lora32 -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x36\" \"-DEXTERNAL_LEDS=true\""

firmware-lora32_v21: check_bt_buffers
	arduino-cli compile --log --fqbn esp32:esp32:ttgo-lora32 -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x37\""

firmware-lora32_v21_extled: check_bt_buffers
	arduino-cli compile --log --fqbn esp32:esp32:ttgo-lora32 -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x37\" \"-DEXTERNAL_LEDS=true\""

firmware-lora32_v21_tcxo: check_bt_buffers
	arduino-cli compile --log --fqbn esp32:esp32:ttgo-lora32 -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x37\" \"-DENABLE_TCXO=true\""

firmware-heltec32_v2: check_bt_buffers
	arduino-cli compile --log --fqbn esp32:esp32:heltec_wifi_lora_32_V2 -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x38\""

firmware-heltec32_v2_extled: check_bt_buffers
	arduino-cli compile --log --fqbn esp32:esp32:heltec_wifi_lora_32_V2 -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x38\" \"-DEXTERNAL_LEDS=true\""

firmware-heltec32_v3:
	arduino-cli compile --log --fqbn esp32:esp32:heltec_wifi_lora_32_V3 -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x3A\""

firmware-rnode_ng_20: check_bt_buffers
	arduino-cli compile --log --fqbn esp32:esp32:ttgo-lora32 -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x40\""

firmware-rnode_ng_21: check_bt_buffers
	arduino-cli compile --log --fqbn esp32:esp32:ttgo-lora32 -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x41\""

firmware-featheresp32: check_bt_buffers
	arduino-cli compile --log --fqbn esp32:esp32:featheresp32 -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x34\""

firmware-genericesp32: check_bt_buffers
	arduino-cli compile --log --fqbn esp32:esp32:esp32 -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x35\""

firmware-rak4631:
	arduino-cli compile --log --fqbn rakwireless:nrf52:WisCoreRAK4631Board -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x51\""

firmware-heltec_t114:
	arduino-cli compile --log --fqbn Heltec_nRF52:Heltec_nRF52:HT-n5262 -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x3C\""

firmware-techo:
	arduino-cli compile --log --fqbn adafruit:nrf52:pca10056 -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x44\""

upload:
	arduino-cli upload -p /dev/ttyUSB0 --fqbn unsignedio:avr:rnode

upload-mega2560:
	arduino-cli upload -p /dev/ttyACM0 --fqbn arduino:avr:mega

upload-tbeam:
	arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:t-beam
	@sleep 1
	rnodeconf /dev/ttyUSB0 --firmware-hash $$(./partition_hashes ./build/esp32.esp32.t-beam/RNode_Firmware.ino.bin)
	@sleep 3
	python ./Release/esptool/esptool.py --chip esp32 --port /dev/ttyACM0 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 4MB 0x210000 ./Release/console_image.bin

upload-tbeam_sx1262:
	arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:t-beam
	@sleep 1
	rnodeconf /dev/ttyACM0 --firmware-hash $$(./partition_hashes ./build/esp32.esp32.t-beam/RNode_Firmware.ino.bin)
	#@sleep 3
	#python ./Release/esptool/esptool.py --chip esp32 --port /dev/ttyACM0 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 4MB 0x210000 ./Release/console_image.bin

upload-lora32_v10:
	arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:ttgo-lora32
	@sleep 1
	rnodeconf /dev/ttyUSB0 --firmware-hash $$(./partition_hashes ./build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bin)
	@sleep 3
	python ./Release/esptool/esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 4MB 0x210000 ./Release/console_image.bin

upload-lora32_v20:
	arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:ttgo-lora32
	@sleep 1
	rnodeconf /dev/ttyUSB0 --firmware-hash $$(./partition_hashes ./build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bin)
	@sleep 3
	python ./Release/esptool/esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 4MB 0x210000 ./Release/console_image.bin

upload-lora32_v21:
	arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:ttgo-lora32
	@sleep 1
	rnodeconf /dev/ttyACM0 --firmware-hash $$(./partition_hashes ./build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bin)
	@sleep 3
	python ./Release/esptool/esptool.py --chip esp32 --port /dev/ttyACM0 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 4MB 0x210000 ./Release/console_image.bin

upload-heltec32_v2:
	arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:heltec_wifi_lora_32_V2
	@sleep 1
	rnodeconf /dev/ttyUSB0 --firmware-hash $$(./partition_hashes ./build/esp32.esp32.heltec_wifi_lora_32_V2/RNode_Firmware.ino.bin)
	@sleep 3
	python ./Release/esptool/esptool.py --chip esp32 --port /dev/ttyUSB1 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 4MB 0x210000 ./Release/console_image.bin

upload-heltec32_v3:
	arduino-cli upload -p /dev/ttyUSB1 --fqbn esp32:esp32:heltec_wifi_lora_32_V3
	@sleep 1
	rnodeconf /dev/ttyUSB1 --firmware-hash $$(./partition_hashes ./build/esp32.esp32.heltec_wifi_lora_32_V3/RNode_Firmware.ino.bin)
	@sleep 3
	python ./Release/esptool/esptool.py --chip esp32-s3 --port /dev/ttyUSB1 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 4MB 0x210000 ./Release/console_image.bin

upload-tdeck:
	arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:esp32s3
	@sleep 1
	rnodeconf /dev/ttyACM0 --firmware-hash $$(./partition_hashes ./build/esp32.esp32.esp32s3/RNode_Firmware.ino.bin)
	@sleep 3
	python ./Release/esptool/esptool.py --chip esp32-s3 --port /dev/ttyACM0 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 4MB 0x210000 ./Release/console_image.bin

upload-tbeam_supreme:
	arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:esp32s3
	@sleep 1
	rnodeconf /dev/ttyACM0 --firmware-hash $$(./partition_hashes ./build/esp32.esp32.esp32s3/RNode_Firmware.ino.bin)
	@sleep 3
	python ./Release/esptool/esptool.py --chip esp32-s3 --port /dev/ttyACM0 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 4MB 0x210000 ./Release/console_image.bin

upload-rnode_ng_20:
	arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:ttgo-lora32
	@sleep 1
	rnodeconf /dev/ttyUSB0 --firmware-hash $$(./partition_hashes ./build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bin)
	@sleep 3
	python ./Release/esptool/esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 4MB 0x210000 ./Release/console_image.bin

upload-rnode_ng_21:
	arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:ttgo-lora32
	@sleep 1
	rnodeconf /dev/ttyACM0 --firmware-hash $$(./partition_hashes ./build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bin)
	@sleep 3
	python ./Release/esptool/esptool.py --chip esp32 --port /dev/ttyACM0 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 4MB 0x210000 ./Release/console_image.bin

upload-t3s3:
	arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:esp32s3
	@sleep 1
	rnodeconf /dev/ttyACM0 --firmware-hash $$(./partition_hashes ./build/esp32.esp32.esp32s3/RNode_Firmware.ino.bin)
	@sleep 3
	python ./Release/esptool/esptool.py --chip esp32s3 --port /dev/ttyACM0 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 4MB 0x210000 ./Release/console_image.bin

upload-featheresp32:
	arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:featheresp32
	@sleep 1
	rnodeconf /dev/ttyUSB0 --firmware-hash $$(./partition_hashes ./build/esp32.esp32.featheresp32/RNode_Firmware.ino.bin)
	@sleep 3
	python ./Release/esptool/esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 4MB 0x210000 ./Release/console_image.bin

upload-rak4631:
	arduino-cli upload -p /dev/ttyACM0 --fqbn rakwireless:nrf52:WisCoreRAK4631Board
	@sleep 1
	rnodeconf /dev/ttyACM0 --firmware-hash $$(./partition_hashes from_device /dev/ttyACM0)

upload-heltec_t114:
	arduino-cli upload -p /dev/ttyACM0 --fqbn Heltec_nRF52:Heltec_nRF52:HT-n5262
	@sleep 1
	rnodeconf /dev/ttyACM0 --firmware-hash $$(./partition_hashes from_device /dev/ttyACM0)

upload-techo:
	arduino-cli upload -p /dev/ttyACM0 --fqbn adafruit:nrf52:pca10056
	@sleep 6
	rnodeconf /dev/ttyACM0 --firmware-hash $$(./partition_hashes from_device /dev/ttyACM0)

release: release-all

release-all: console-site spiffs-image release-tbeam release-tbeam_sx1262 release-lora32_v10 release-lora32_v20 release-lora32_v21 release-lora32_v10_extled release-lora32_v20_extled release-lora32_v21_extled release-lora32_v21_tcxo release-featheresp32 release-genericesp32 release-heltec32_v2 release-heltec32_v3 release-heltec32_v2_extled release-heltec_t114 release-techo release-rnode_ng_20 release-rnode_ng_21 release-t3s3 release-t3s3_sx127x release-t3s3_sx1280_pa release-tdeck release-tbeam_supreme release-rak4631 release-hashes

release-hashes:
	python ./release_hashes.py > ./Release/release.json

release-rnode:
	arduino-cli compile --fqbn unsignedio:avr:rnode -e
	cp build/unsignedio.avr.rnode/RNode_Firmware.ino.hex Release/rnode_firmware.hex
	rm -r build

release-tbeam: check_bt_buffers
	arduino-cli compile --fqbn esp32:esp32:t-beam -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x33\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/$(ARDUINO_ESP_CORE_VER)/tools/partitions/boot_app0.bin build/rnode_firmware_tbeam.boot_app0
	cp build/esp32.esp32.t-beam/RNode_Firmware.ino.bin build/rnode_firmware_tbeam.bin
	cp build/esp32.esp32.t-beam/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_tbeam.bootloader
	cp build/esp32.esp32.t-beam/RNode_Firmware.ino.partitions.bin build/rnode_firmware_tbeam.partitions
	zip --junk-paths ./Release/rnode_firmware_tbeam.zip ./Release/esptool/esptool.py ./Release/console_image.bin build/rnode_firmware_tbeam.boot_app0 build/rnode_firmware_tbeam.bin build/rnode_firmware_tbeam.bootloader build/rnode_firmware_tbeam.partitions
	rm -r build

release-tbeam_sx1262: check_bt_buffers
	arduino-cli compile --fqbn esp32:esp32:t-beam -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x33\" \"-DMODEM=0x03\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/$(ARDUINO_ESP_CORE_VER)/tools/partitions/boot_app0.bin build/rnode_firmware_tbeam_sx1262.boot_app0
	cp build/esp32.esp32.t-beam/RNode_Firmware.ino.bin build/rnode_firmware_tbeam_sx1262.bin
	cp build/esp32.esp32.t-beam/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_tbeam_sx1262.bootloader
	cp build/esp32.esp32.t-beam/RNode_Firmware.ino.partitions.bin build/rnode_firmware_tbeam_sx1262.partitions
	zip --junk-paths ./Release/rnode_firmware_tbeam_sx1262.zip ./Release/esptool/esptool.py ./Release/console_image.bin build/rnode_firmware_tbeam_sx1262.boot_app0 build/rnode_firmware_tbeam_sx1262.bin build/rnode_firmware_tbeam_sx1262.bootloader build/rnode_firmware_tbeam_sx1262.partitions
	rm -r build

release-lora32_v10: check_bt_buffers
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x39\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/$(ARDUINO_ESP_CORE_VER)/tools/partitions/boot_app0.bin build/rnode_firmware_lora32v10.boot_app0
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bin build/rnode_firmware_lora32v10.bin
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_lora32v10.bootloader
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.partitions.bin build/rnode_firmware_lora32v10.partitions
	zip --junk-paths ./Release/rnode_firmware_lora32v10.zip ./Release/esptool/esptool.py ./Release/console_image.bin build/rnode_firmware_lora32v10.boot_app0 build/rnode_firmware_lora32v10.bin build/rnode_firmware_lora32v10.bootloader build/rnode_firmware_lora32v10.partitions
	rm -r build

release-lora32_v20: check_bt_buffers
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x36\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/$(ARDUINO_ESP_CORE_VER)/tools/partitions/boot_app0.bin build/rnode_firmware_lora32v20.boot_app0
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bin build/rnode_firmware_lora32v20.bin
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_lora32v20.bootloader
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.partitions.bin build/rnode_firmware_lora32v20.partitions
	zip --junk-paths ./Release/rnode_firmware_lora32v20.zip ./Release/esptool/esptool.py ./Release/console_image.bin build/rnode_firmware_lora32v20.boot_app0 build/rnode_firmware_lora32v20.bin build/rnode_firmware_lora32v20.bootloader build/rnode_firmware_lora32v20.partitions
	rm -r build

release-lora32_v21: check_bt_buffers
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x37\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/$(ARDUINO_ESP_CORE_VER)/tools/partitions/boot_app0.bin build/rnode_firmware_lora32v21.boot_app0
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bin build/rnode_firmware_lora32v21.bin
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_lora32v21.bootloader
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.partitions.bin build/rnode_firmware_lora32v21.partitions
	zip --junk-paths ./Release/rnode_firmware_lora32v21.zip ./Release/esptool/esptool.py ./Release/console_image.bin build/rnode_firmware_lora32v21.boot_app0 build/rnode_firmware_lora32v21.bin build/rnode_firmware_lora32v21.bootloader build/rnode_firmware_lora32v21.partitions
	rm -r build

release-lora32_v10_extled: check_bt_buffers
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x39\" \"-DEXTERNAL_LEDS=true\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/$(ARDUINO_ESP_CORE_VER)/tools/partitions/boot_app0.bin build/rnode_firmware_lora32v10.boot_app0
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bin build/rnode_firmware_lora32v10.bin
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_lora32v10.bootloader
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.partitions.bin build/rnode_firmware_lora32v10.partitions
	zip --junk-paths ./Release/rnode_firmware_lora32v10.zip ./Release/esptool/esptool.py ./Release/console_image.bin build/rnode_firmware_lora32v10.boot_app0 build/rnode_firmware_lora32v10.bin build/rnode_firmware_lora32v10.bootloader build/rnode_firmware_lora32v10.partitions
	rm -r build

release-lora32_v20_extled: check_bt_buffers
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x36\" \"-DEXTERNAL_LEDS=true\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/$(ARDUINO_ESP_CORE_VER)/tools/partitions/boot_app0.bin build/rnode_firmware_lora32v20.boot_app0
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bin build/rnode_firmware_lora32v20.bin
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_lora32v20.bootloader
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.partitions.bin build/rnode_firmware_lora32v20.partitions
	zip --junk-paths ./Release/rnode_firmware_lora32v20_extled.zip ./Release/esptool/esptool.py ./Release/console_image.bin build/rnode_firmware_lora32v20.boot_app0 build/rnode_firmware_lora32v20.bin build/rnode_firmware_lora32v20.bootloader build/rnode_firmware_lora32v20.partitions
	rm -r build

release-lora32_v21_extled: check_bt_buffers
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x37\" \"-DEXTERNAL_LEDS=true\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/$(ARDUINO_ESP_CORE_VER)/tools/partitions/boot_app0.bin build/rnode_firmware_lora32v21.boot_app0
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bin build/rnode_firmware_lora32v21.bin
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_lora32v21.bootloader
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.partitions.bin build/rnode_firmware_lora32v21.partitions
	zip --junk-paths ./Release/rnode_firmware_lora32v21_extled.zip ./Release/esptool/esptool.py ./Release/console_image.bin build/rnode_firmware_lora32v21.boot_app0 build/rnode_firmware_lora32v21.bin build/rnode_firmware_lora32v21.bootloader build/rnode_firmware_lora32v21.partitions
	rm -r build

release-lora32_v21_tcxo: check_bt_buffers
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x37\" \"-DENABLE_TCXO=true\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/$(ARDUINO_ESP_CORE_VER)/tools/partitions/boot_app0.bin build/rnode_firmware_lora32v21_tcxo.boot_app0
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bin build/rnode_firmware_lora32v21_tcxo.bin
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_lora32v21_tcxo.bootloader
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.partitions.bin build/rnode_firmware_lora32v21_tcxo.partitions
	zip --junk-paths ./Release/rnode_firmware_lora32v21_tcxo.zip ./Release/esptool/esptool.py ./Release/console_image.bin build/rnode_firmware_lora32v21_tcxo.boot_app0 build/rnode_firmware_lora32v21_tcxo.bin build/rnode_firmware_lora32v21_tcxo.bootloader build/rnode_firmware_lora32v21_tcxo.partitions
	rm -r build

release-heltec32_v2: check_bt_buffers
	arduino-cli compile --fqbn esp32:esp32:heltec_wifi_lora_32_V2 -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x38\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/$(ARDUINO_ESP_CORE_VER)/tools/partitions/boot_app0.bin build/rnode_firmware_heltec32v2.boot_app0
	cp build/esp32.esp32.heltec_wifi_lora_32_V2/RNode_Firmware.ino.bin build/rnode_firmware_heltec32v2.bin
	cp build/esp32.esp32.heltec_wifi_lora_32_V2/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_heltec32v2.bootloader
	cp build/esp32.esp32.heltec_wifi_lora_32_V2/RNode_Firmware.ino.partitions.bin build/rnode_firmware_heltec32v2.partitions
	zip --junk-paths ./Release/rnode_firmware_heltec32v2.zip ./Release/esptool/esptool.py ./Release/console_image.bin build/rnode_firmware_heltec32v2.boot_app0 build/rnode_firmware_heltec32v2.bin build/rnode_firmware_heltec32v2.bootloader build/rnode_firmware_heltec32v2.partitions
	rm -r build

release-heltec32_v3:
	arduino-cli compile --fqbn esp32:esp32:heltec_wifi_lora_32_V3 -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x3A\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/$(ARDUINO_ESP_CORE_VER)/tools/partitions/boot_app0.bin build/rnode_firmware_heltec32v3.boot_app0
	cp build/esp32.esp32.heltec_wifi_lora_32_V3/RNode_Firmware.ino.bin build/rnode_firmware_heltec32v3.bin
	cp build/esp32.esp32.heltec_wifi_lora_32_V3/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_heltec32v3.bootloader
	cp build/esp32.esp32.heltec_wifi_lora_32_V3/RNode_Firmware.ino.partitions.bin build/rnode_firmware_heltec32v3.partitions
	zip --junk-paths ./Release/rnode_firmware_heltec32v3.zip ./Release/esptool/esptool.py ./Release/console_image.bin build/rnode_firmware_heltec32v3.boot_app0 build/rnode_firmware_heltec32v3.bin build/rnode_firmware_heltec32v3.bootloader build/rnode_firmware_heltec32v3.partitions
	rm -r build

release-heltec32_v2_extled: check_bt_buffers
	arduino-cli compile --fqbn esp32:esp32:heltec_wifi_lora_32_V2 -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x38\" \"-DEXTERNAL_LEDS=true\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/$(ARDUINO_ESP_CORE_VER)/tools/partitions/boot_app0.bin build/rnode_firmware_heltec32v2.boot_app0
	cp build/esp32.esp32.heltec_wifi_lora_32_V2/RNode_Firmware.ino.bin build/rnode_firmware_heltec32v2.bin
	cp build/esp32.esp32.heltec_wifi_lora_32_V2/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_heltec32v2.bootloader
	cp build/esp32.esp32.heltec_wifi_lora_32_V2/RNode_Firmware.ino.partitions.bin build/rnode_firmware_heltec32v2.partitions
	zip --junk-paths ./Release/rnode_firmware_heltec32v2.zip ./Release/esptool/esptool.py ./Release/console_image.bin build/rnode_firmware_heltec32v2.boot_app0 build/rnode_firmware_heltec32v2.bin build/rnode_firmware_heltec32v2.bootloader build/rnode_firmware_heltec32v2.partitions
	rm -r build

release-rnode_ng_20: check_bt_buffers
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x40\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/$(ARDUINO_ESP_CORE_VER)/tools/partitions/boot_app0.bin build/rnode_firmware_ng20.boot_app0
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bin build/rnode_firmware_ng20.bin
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_ng20.bootloader
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.partitions.bin build/rnode_firmware_ng20.partitions
	zip --junk-paths ./Release/rnode_firmware_ng20.zip ./Release/esptool/esptool.py ./Release/console_image.bin build/rnode_firmware_ng20.boot_app0 build/rnode_firmware_ng20.bin build/rnode_firmware_ng20.bootloader build/rnode_firmware_ng20.partitions
	rm -r build

release-rnode_ng_21: check_bt_buffers
	arduino-cli compile --fqbn esp32:esp32:ttgo-lora32 -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x41\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/$(ARDUINO_ESP_CORE_VER)/tools/partitions/boot_app0.bin build/rnode_firmware_ng21.boot_app0
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bin build/rnode_firmware_ng21.bin
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_ng21.bootloader
	cp build/esp32.esp32.ttgo-lora32/RNode_Firmware.ino.partitions.bin build/rnode_firmware_ng21.partitions
	zip --junk-paths ./Release/rnode_firmware_ng21.zip ./Release/esptool/esptool.py ./Release/console_image.bin build/rnode_firmware_ng21.boot_app0 build/rnode_firmware_ng21.bin build/rnode_firmware_ng21.bootloader build/rnode_firmware_ng21.partitions
	rm -r build

release-t3s3:
	arduino-cli compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc" -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x42\" \"-DMODEM=0x03\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/$(ARDUINO_ESP_CORE_VER)/tools/partitions/boot_app0.bin build/rnode_firmware_t3s3.boot_app0
	cp build/esp32.esp32.esp32s3/RNode_Firmware.ino.bin build/rnode_firmware_t3s3.bin
	cp build/esp32.esp32.esp32s3/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_t3s3.bootloader
	cp build/esp32.esp32.esp32s3/RNode_Firmware.ino.partitions.bin build/rnode_firmware_t3s3.partitions
	zip --junk-paths ./Release/rnode_firmware_t3s3.zip ./Release/esptool/esptool.py ./Release/console_image.bin build/rnode_firmware_t3s3.boot_app0 build/rnode_firmware_t3s3.bin build/rnode_firmware_t3s3.bootloader build/rnode_firmware_t3s3.partitions
	rm -r build

release-t3s3_sx1280_pa:
	arduino-cli compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc" -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x42\" \"-DMODEM=0x04\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/$(ARDUINO_ESP_CORE_VER)/tools/partitions/boot_app0.bin build/rnode_firmware_t3s3_sx1280_pa.boot_app0
	cp build/esp32.esp32.esp32s3/RNode_Firmware.ino.bin build/rnode_firmware_t3s3_sx1280_pa.bin
	cp build/esp32.esp32.esp32s3/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_t3s3_sx1280_pa.bootloader
	cp build/esp32.esp32.esp32s3/RNode_Firmware.ino.partitions.bin build/rnode_firmware_t3s3_sx1280_pa.partitions
	zip --junk-paths ./Release/rnode_firmware_t3s3_sx1280_pa.zip ./Release/esptool/esptool.py ./Release/console_image.bin build/rnode_firmware_t3s3_sx1280_pa.boot_app0 build/rnode_firmware_t3s3_sx1280_pa.bin build/rnode_firmware_t3s3_sx1280_pa.bootloader build/rnode_firmware_t3s3_sx1280_pa.partitions
	rm -r build

release-t3s3_sx127x:
	arduino-cli compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc" -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x42\" \"-DMODEM=0x01\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/$(ARDUINO_ESP_CORE_VER)/tools/partitions/boot_app0.bin build/rnode_firmware_t3s3_sx127x.boot_app0
	cp build/esp32.esp32.esp32s3/RNode_Firmware.ino.bin build/rnode_firmware_t3s3_sx127x.bin
	cp build/esp32.esp32.esp32s3/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_t3s3_sx127x.bootloader
	cp build/esp32.esp32.esp32s3/RNode_Firmware.ino.partitions.bin build/rnode_firmware_t3s3_sx127x.partitions
	zip --junk-paths ./Release/rnode_firmware_t3s3_sx127x.zip ./Release/esptool/esptool.py ./Release/console_image.bin build/rnode_firmware_t3s3_sx127x.boot_app0 build/rnode_firmware_t3s3_sx127x.bin build/rnode_firmware_t3s3_sx127x.bootloader build/rnode_firmware_t3s3_sx127x.partitions
	rm -r build

release-tdeck:
	arduino-cli compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc" -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x3B\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/$(ARDUINO_ESP_CORE_VER)/tools/partitions/boot_app0.bin build/rnode_firmware_tdeck.boot_app0
	cp build/esp32.esp32.esp32s3/RNode_Firmware.ino.bin build/rnode_firmware_tdeck.bin
	cp build/esp32.esp32.esp32s3/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_tdeck.bootloader
	cp build/esp32.esp32.esp32s3/RNode_Firmware.ino.partitions.bin build/rnode_firmware_tdeck.partitions
	zip --junk-paths ./Release/rnode_firmware_tdeck.zip ./Release/esptool/esptool.py ./Release/console_image.bin build/rnode_firmware_tdeck.boot_app0 build/rnode_firmware_tdeck.bin build/rnode_firmware_tdeck.bootloader build/rnode_firmware_tdeck.partitions
	rm -r build

release-tbeam_supreme:
	arduino-cli compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc" -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x3D\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/$(ARDUINO_ESP_CORE_VER)/tools/partitions/boot_app0.bin build/rnode_firmware_tbeam_supreme.boot_app0
	cp build/esp32.esp32.esp32s3/RNode_Firmware.ino.bin build/rnode_firmware_tbeam_supreme.bin
	cp build/esp32.esp32.esp32s3/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_tbeam_supreme.bootloader
	cp build/esp32.esp32.esp32s3/RNode_Firmware.ino.partitions.bin build/rnode_firmware_tbeam_supreme.partitions
	zip --junk-paths ./Release/rnode_firmware_tbeam_supreme.zip ./Release/esptool/esptool.py ./Release/console_image.bin build/rnode_firmware_tbeam_supreme.boot_app0 build/rnode_firmware_tbeam_supreme.bin build/rnode_firmware_tbeam_supreme.bootloader build/rnode_firmware_tbeam_supreme.partitions
	rm -r build

release-featheresp32: check_bt_buffers
	arduino-cli compile --fqbn esp32:esp32:featheresp32 -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x34\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/$(ARDUINO_ESP_CORE_VER)/tools/partitions/boot_app0.bin build/rnode_firmware_featheresp32.boot_app0
	cp build/esp32.esp32.featheresp32/RNode_Firmware.ino.bin build/rnode_firmware_featheresp32.bin
	cp build/esp32.esp32.featheresp32/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_featheresp32.bootloader
	cp build/esp32.esp32.featheresp32/RNode_Firmware.ino.partitions.bin build/rnode_firmware_featheresp32.partitions
	zip --junk-paths ./Release/rnode_firmware_featheresp32.zip ./Release/esptool/esptool.py ./Release/console_image.bin build/rnode_firmware_featheresp32.boot_app0 build/rnode_firmware_featheresp32.bin build/rnode_firmware_featheresp32.bootloader build/rnode_firmware_featheresp32.partitions
	rm -r build

release-genericesp32: check_bt_buffers
	arduino-cli compile --fqbn esp32:esp32:esp32 -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x35\""
	cp ~/.arduino15/packages/esp32/hardware/esp32/$(ARDUINO_ESP_CORE_VER)/tools/partitions/boot_app0.bin build/rnode_firmware_esp32_generic.boot_app0
	cp build/esp32.esp32.esp32/RNode_Firmware.ino.bin build/rnode_firmware_esp32_generic.bin
	cp build/esp32.esp32.esp32/RNode_Firmware.ino.bootloader.bin build/rnode_firmware_esp32_generic.bootloader
	cp build/esp32.esp32.esp32/RNode_Firmware.ino.partitions.bin build/rnode_firmware_esp32_generic.partitions
	zip --junk-paths ./Release/rnode_firmware_esp32_generic.zip ./Release/esptool/esptool.py ./Release/console_image.bin build/rnode_firmware_esp32_generic.boot_app0 build/rnode_firmware_esp32_generic.bin build/rnode_firmware_esp32_generic.bootloader build/rnode_firmware_esp32_generic.partitions
	rm -r build

release-mega2560:
	arduino-cli compile --fqbn arduino:avr:mega -e --build-property "compiler.cpp.extra_flags=\"-DMODEM=0x01\""
	cp build/arduino.avr.mega/RNode_Firmware.ino.hex Release/rnode_firmware_m2560.hex
	rm -r build

release-rak4631:
	arduino-cli compile --fqbn rakwireless:nrf52:WisCoreRAK4631Board -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x51\""
	cp build/rakwireless.nrf52.WisCoreRAK4631Board/RNode_Firmware.ino.hex build/rnode_firmware_rak4631.hex
	adafruit-nrfutil dfu genpkg --dev-type 0x0052 --application build/rnode_firmware_rak4631.hex Release/rnode_firmware_rak4631.zip

release-heltec_t114:
	arduino-cli compile --fqbn Heltec_nRF52:Heltec_nRF52:HT-n5262 -e --build-property "build.partitions=no_ota" --build-property "upload.maximum_size=2097152" --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x3C\""
	cp build/Heltec_nRF52.Heltec_nRF52.HT-n5262/RNode_Firmware.ino.hex build/rnode_firmware_heltec_t114.hex
	adafruit-nrfutil dfu genpkg --dev-type 0x0052 --application build/rnode_firmware_heltec_t114.hex Release/rnode_firmware_heltec_t114.zip

release-techo:
	arduino-cli compile --log --fqbn adafruit:nrf52:pca10056 -e --build-property "compiler.cpp.extra_flags=\"-DBOARD_MODEL=0x44\""
	cp build/adafruit.nrf52.pca10056/RNode_Firmware.ino.hex build/rnode_firmware_techo.hex
	adafruit-nrfutil dfu genpkg --dev-type 0x0052 --application build/rnode_firmware_techo.hex Release/rnode_firmware_techo.zip
