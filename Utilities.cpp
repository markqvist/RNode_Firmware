#include <Arduino.h>
#include <LoRa.h>
#include "Config.h"
#include "Framing.h"

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
	Serial.write(FEND);
	Serial.write(CMD_STAT_RSSI);
	Serial.write((uint8_t)last_rssi+rssi_offset);
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

void setTXPower() {
	if (radio_online) LoRa.setTxPower(lora_txp);
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

