#include <EEPROM.h>
#include "LoRa.h"
#include "ROM.h"
#include "Config.h"
#include "Framing.h"
#include "MD5.h"

void led_rx_on()  { digitalWrite(pin_led_rx, HIGH); }
void led_rx_off() {	digitalWrite(pin_led_rx, LOW); }
void led_tx_on()  { digitalWrite(pin_led_tx, HIGH); }
void led_tx_off() { digitalWrite(pin_led_tx, LOW); }

void led_indicate_error(int cycles) {
	bool forever = (cycles == 0) ? true : false;
	cycles = forever ? 1 : cycles;
	while(cycles > 0) {
        digitalWrite(pin_led_rx, HIGH);
        digitalWrite(pin_led_tx, LOW);
        delay(100);
        digitalWrite(pin_led_rx, LOW);
        digitalWrite(pin_led_tx, HIGH);
        delay(100);
        if (!forever) cycles--;
    }
    digitalWrite(pin_led_rx, LOW);
    digitalWrite(pin_led_tx, LOW);
}

void led_indicate_warning(int cycles) {
	bool forever = (cycles == 0) ? true : false;
	cycles = forever ? 1 : cycles;
	digitalWrite(pin_led_tx, HIGH);
	while(cycles > 0) {
        digitalWrite(pin_led_tx, LOW);
        delay(100);
        digitalWrite(pin_led_tx, HIGH);
        delay(100);
        if (!forever) cycles--;
    }
    digitalWrite(pin_led_tx, LOW);
}

void led_indicate_info(int cycles) {
	bool forever = (cycles == 0) ? true : false;
	cycles = forever ? 1 : cycles;
	while(cycles > 0) {
        digitalWrite(pin_led_rx, LOW);
        delay(100);
        digitalWrite(pin_led_rx, HIGH);
        delay(100);
        if (!forever) cycles--;
    }
    digitalWrite(pin_led_rx, LOW);
}

uint8_t led_standby_min = 1;
uint8_t led_standby_max = 40;
uint8_t led_standby_value = led_standby_min;
int8_t  led_standby_direction = 0;
unsigned long led_standby_ticks = 0;
unsigned long led_standby_wait = 11000;
void led_indicate_standby() {
	led_standby_ticks++;
	if (led_standby_ticks > led_standby_wait) {
		led_standby_ticks = 0;
		if (led_standby_value <= led_standby_min) {
			led_standby_direction = 1;
		} else if (led_standby_value >= led_standby_max) {
			led_standby_direction = -1;
		}
		led_standby_value += led_standby_direction;
		analogWrite(pin_led_rx, led_standby_value);
		digitalWrite(pin_led_tx, 0);
	}
}

void led_indicate_not_ready() {
	led_standby_ticks++;
	if (led_standby_ticks > led_standby_wait) {
		led_standby_ticks = 0;
		if (led_standby_value <= led_standby_min) {
			led_standby_direction = 1;
		} else if (led_standby_value >= led_standby_max) {
			led_standby_direction = -1;
		}
		led_standby_value += led_standby_direction;
		analogWrite(pin_led_tx, led_standby_value);
		digitalWrite(pin_led_rx, 0);
	}
}

void escapedSerialWrite(uint8_t byte) {
	if (byte == FEND) { Serial.write(FESC); byte = TFEND; }
    if (byte == FESC) { Serial.write(FESC); byte = TFESC; }
    Serial.write(byte);
}

void kiss_indicate_error(uint8_t error_code) {
	Serial.write(FEND);
	Serial.write(CMD_ERROR);
	Serial.write(error_code);
	Serial.write(FEND);
}

void kiss_indicate_radiostate() {
	Serial.write(FEND);
	Serial.write(CMD_RADIO_STATE);
	Serial.write(radio_online);
	Serial.write(FEND);
}

void kiss_indicate_stat_rx() {
	Serial.write(FEND);
	Serial.write(CMD_STAT_RX);
	escapedSerialWrite(stat_rx>>24);
	escapedSerialWrite(stat_rx>>16);
	escapedSerialWrite(stat_rx>>8);
	escapedSerialWrite(stat_rx);
	Serial.write(FEND);
}

void kiss_indicate_stat_tx() {
	Serial.write(FEND);
	Serial.write(CMD_STAT_TX);
	escapedSerialWrite(stat_tx>>24);
	escapedSerialWrite(stat_tx>>16);
	escapedSerialWrite(stat_tx>>8);
	escapedSerialWrite(stat_tx);
	Serial.write(FEND);
}

void kiss_indicate_stat_rssi() {
	uint8_t packet_rssi_val = (uint8_t)(last_rssi+rssi_offset);
	Serial.write(FEND);
	Serial.write(CMD_STAT_RSSI);
	Serial.write(packet_rssi_val);
	Serial.write(FEND);
}

void kiss_indicate_radio_lock() {
	Serial.write(FEND);
	Serial.write(CMD_RADIO_LOCK);
	Serial.write(radio_locked);
	Serial.write(FEND);
}

void kiss_indicate_spreadingfactor() {
	Serial.write(FEND);
	Serial.write(CMD_SF);
	Serial.write((uint8_t)lora_sf);
	Serial.write(FEND);
}

void kiss_indicate_codingrate() {
	Serial.write(FEND);
	Serial.write(CMD_CR);
	Serial.write((uint8_t)lora_cr);
	Serial.write(FEND);
}

void kiss_indicate_txpower() {
	Serial.write(FEND);
	Serial.write(CMD_TXPOWER);
	Serial.write((uint8_t)lora_txp);
	Serial.write(FEND);
}

void kiss_indicate_bandwidth() {
	Serial.write(FEND);
	Serial.write(CMD_BANDWIDTH);
	escapedSerialWrite(lora_bw>>24);
	escapedSerialWrite(lora_bw>>16);
	escapedSerialWrite(lora_bw>>8);
	escapedSerialWrite(lora_bw);
	Serial.write(FEND);
}

void kiss_indicate_frequency() {
	Serial.write(FEND);
	Serial.write(CMD_FREQUENCY);
	escapedSerialWrite(lora_freq>>24);
	escapedSerialWrite(lora_freq>>16);
	escapedSerialWrite(lora_freq>>8);
	escapedSerialWrite(lora_freq);
	Serial.write(FEND);
}

void kiss_indicate_random(uint8_t byte) {
	Serial.write(FEND);
	Serial.write(CMD_RANDOM);
	Serial.write(byte);
	Serial.write(FEND);
}

void kiss_indicate_ready() {
	Serial.write(FEND);
	Serial.write(CMD_READY);
	Serial.write(0x01);
	Serial.write(FEND);
}

void kiss_indicate_promisc() {
	Serial.write(FEND);
	Serial.write(CMD_PROMISC);
	if (promisc) {
		Serial.write(0x01);
	} else {
		Serial.write(0x00);
	}
	Serial.write(FEND);
}

void kiss_indicate_detect() {
	Serial.write(FEND);
	Serial.write(CMD_DETECT);
	Serial.write(DETECT_RESP);
	Serial.write(FEND);
}

void kiss_indicate_version() {
	Serial.write(FEND);
	Serial.write(CMD_FW_VERSION);
	Serial.write(MAJ_VERS);
	Serial.write(MIN_VERS);
	Serial.write(FEND);
}

bool isSplitPacket(uint8_t header) {
	return (header & FLAG_SPLIT);
}

uint8_t packetSequence(uint8_t header) {
	return header >> 4;
}

void getPacketData(int len) {
	while (len--) {
		pbuf[read_len++] = LoRa.read();
	}
}

void setSpreadingFactor() {
	if (radio_online) LoRa.setSpreadingFactor(lora_sf);
}

void setCodingRate() {
	if (radio_online) LoRa.setCodingRate4(lora_cr);
}

void setTXPower() {
	if (radio_online) {
		if (model == MODEL_A4) LoRa.setTxPower(lora_txp, PA_OUTPUT_RFO_PIN);
		if (model == MODEL_A9) LoRa.setTxPower(lora_txp, PA_OUTPUT_PA_BOOST_PIN);
	}
}


void getBandwidth() {
	if (radio_online) {
			lora_bw = LoRa.getSignalBandwidth();
	}
}

void setBandwidth() {
	if (radio_online) {
		LoRa.setSignalBandwidth(lora_bw);
		getBandwidth();
	}
}

void getFrequency() {
	if (radio_online) {
		lora_freq = LoRa.getFrequency();
	}
}

void setFrequency() {
	if (radio_online) {
		LoRa.setFrequency(lora_freq);
		getFrequency();
	}
}

uint8_t getRandom() {
	if (radio_online) {
		return LoRa.random();
	} else {
		return 0x00;
	}
}

void promisc_enable() {
	promisc = true;
}

void promisc_disable() {
	promisc = false;
}

bool eeprom_info_locked() {
	uint8_t lock_byte = EEPROM.read(eeprom_addr(ADDR_INFO_LOCK));
	if (lock_byte == INFO_LOCK_BYTE) {
		return true;
	} else {
		return false;
	}
}

void eeprom_dump_info() {
	for (int addr = ADDR_PRODUCT; addr <= ADDR_INFO_LOCK; addr++) {
		uint8_t byte = EEPROM.read(eeprom_addr(addr));
		escapedSerialWrite(byte);
	}
}

void eeprom_dump_config() {
	for (int addr = ADDR_CONF_SF; addr <= ADDR_CONF_OK; addr++) {
		uint8_t byte = EEPROM.read(eeprom_addr(addr));
		escapedSerialWrite(byte);
	}
}

void eeprom_dump_all() {
	for (int addr = 0; addr < EEPROM_RESERVED; addr++) {
		uint8_t byte = EEPROM.read(eeprom_addr(addr));
		escapedSerialWrite(byte);
	}
}

void kiss_dump_eeprom() {
	Serial.write(FEND);
	Serial.write(CMD_ROM_READ);
	eeprom_dump_all();
	Serial.write(FEND);
}

void eeprom_write(uint8_t addr, uint8_t byte) {
	if (!eeprom_info_locked() && addr >= 0 && addr < EEPROM_RESERVED) {
		EEPROM.update(eeprom_addr(addr), byte);
	} else {
		kiss_indicate_error(ERROR_EEPROM_LOCKED);
	}
}

void eeprom_erase() {
	for (int addr = 0; addr < EEPROM_RESERVED; addr++) {
		EEPROM.update(eeprom_addr(addr), 0xFF);
	}
	while (true) { led_tx_on(); led_rx_off(); }
}

bool eeprom_lock_set() {
	if (EEPROM.read(eeprom_addr(ADDR_INFO_LOCK)) == INFO_LOCK_BYTE) {
		return true;
	} else {
		return false;
	}
}

bool eeprom_product_valid() {
	if (EEPROM.read(eeprom_addr(ADDR_PRODUCT)) == PRODUCT_RNODE) {
		return true;
	} else {
		return false;
	}
}

bool eeprom_model_valid() {
	model = EEPROM.read(eeprom_addr(ADDR_MODEL));
	if (model == MODEL_A4 || model == MODEL_A9) {
		return true;
	} else {
		return false;
	}
}

bool eeprom_hwrev_valid() {
	hwrev = EEPROM.read(eeprom_addr(ADDR_HW_REV));
	if (hwrev != 0x00 && hwrev != 0xFF) {
		return true;
	} else {
		return false;
	}
}

bool eeprom_checksum_valid() {
	char *data = (char*)malloc(CHECKSUMMED_SIZE);
	for (uint8_t  i = 0; i < CHECKSUMMED_SIZE; i++) {
		char byte = EEPROM.read(eeprom_addr(i));
		data[i] = byte;
	}
	
	unsigned char *hash = MD5::make_hash(data, CHECKSUMMED_SIZE);
	bool checksum_valid = true;
	for (uint8_t i = 0; i < 16; i++) {
		uint8_t stored_chk_byte = EEPROM.read(eeprom_addr(ADDR_CHKSUM+i));
		uint8_t calced_chk_byte = (uint8_t)hash[i];
		if (stored_chk_byte != calced_chk_byte) {
			checksum_valid = false;
		}
	}

	free(hash);
	free(data);
	return checksum_valid;
}

bool eeprom_have_conf() {
	if (EEPROM.read(eeprom_addr(ADDR_CONF_OK)) == CONF_OK_BYTE) {
		return true;
	} else {
		return false;
	}
}

void eeprom_conf_load() {
	if (eeprom_have_conf()) {
		lora_sf = EEPROM.read(eeprom_addr(ADDR_CONF_SF));
		lora_cr = EEPROM.read(eeprom_addr(ADDR_CONF_CR));
		lora_txp = EEPROM.read(eeprom_addr(ADDR_CONF_TXP));
		lora_freq = (uint32_t)EEPROM.read(eeprom_addr(ADDR_CONF_FREQ)+0x00) << 24 | (uint32_t)EEPROM.read(eeprom_addr(ADDR_CONF_FREQ)+0x01) << 16 | (uint32_t)EEPROM.read(eeprom_addr(ADDR_CONF_FREQ)+0x02) << 8 | (uint32_t)EEPROM.read(eeprom_addr(ADDR_CONF_FREQ)+0x03);
		lora_bw = (uint32_t)EEPROM.read(eeprom_addr(ADDR_CONF_BW)+0x00) << 24 | (uint32_t)EEPROM.read(eeprom_addr(ADDR_CONF_BW)+0x01) << 16 | (uint32_t)EEPROM.read(eeprom_addr(ADDR_CONF_BW)+0x02) << 8 | (uint32_t)EEPROM.read(eeprom_addr(ADDR_CONF_BW)+0x03);
	}
}

void eeprom_conf_save() {
	if (hw_ready && radio_online) {
		EEPROM.update(eeprom_addr(ADDR_CONF_SF), lora_sf);
		EEPROM.update(eeprom_addr(ADDR_CONF_CR), lora_cr);
		EEPROM.update(eeprom_addr(ADDR_CONF_TXP), lora_txp);

		EEPROM.update(eeprom_addr(ADDR_CONF_BW)+0x00, lora_bw>>24);
		EEPROM.update(eeprom_addr(ADDR_CONF_BW)+0x01, lora_bw>>16);
		EEPROM.update(eeprom_addr(ADDR_CONF_BW)+0x02, lora_bw>>8);
		EEPROM.update(eeprom_addr(ADDR_CONF_BW)+0x03, lora_bw);

		EEPROM.update(eeprom_addr(ADDR_CONF_FREQ)+0x00, lora_freq>>24);
		EEPROM.update(eeprom_addr(ADDR_CONF_FREQ)+0x01, lora_freq>>16);
		EEPROM.update(eeprom_addr(ADDR_CONF_FREQ)+0x02, lora_freq>>8);
		EEPROM.update(eeprom_addr(ADDR_CONF_FREQ)+0x03, lora_freq);

		EEPROM.update(eeprom_addr(ADDR_CONF_OK), CONF_OK_BYTE);
		led_indicate_info(10);
	} else {
		led_indicate_warning(10);
	}
}

void eeprom_conf_delete() {
	EEPROM.update(eeprom_addr(ADDR_CONF_OK), 0x00);
}

void unlock_rom() {
	led_indicate_error(50);
	eeprom_erase();
}
