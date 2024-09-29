#pragma once
#include <Arduino.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

template <size_t n>
class BLEFIFO {
private:
	uint8_t buffer[n];
	int head = 0;
	int tail = 0;

public:
	void push(uint8_t value) {
		buffer[head] = value;
		head = (head + 1) % n;
		if (head == tail) { tail = (tail + 1) % n; }
	}

	int pop() {
		if (head == tail) {
			return -1;
		} else {
			uint8_t value = buffer[tail];
			tail = (tail + 1) % n;
			return value;
		}
	}

	void clear() { head = 0; tail = 0; }

	int get(size_t index) {
		if (index >= this->getLength()) {
			return -1;
		} else {
			return buffer[(tail + index) % n];
		}
	}

	size_t getLength() {
		if (head >= tail) {
			return head - tail;
		} else {
			return n - tail + head;
		}
	}
};

#define RX_BUFFER_SIZE 6144
#define BLE_BUFFER_SIZE 512 // Must fit in max GATT attribute length
#define MIN_MTU 50

class BLESerial : public BLECharacteristicCallbacks, public BLEServerCallbacks, public BLESecurityCallbacks, public Stream {
public:
  BLESerial();

  void begin(const char *name);
  void end();
  void onWrite(BLECharacteristic *characteristic);
  int available();
  int peek();
  int read();
  size_t readBytes(uint8_t *buffer, size_t bufferSize);
  size_t write(uint8_t byte);
  size_t write(const uint8_t *buffer, size_t bufferSize);
  size_t print(const char *value);
  void flush();
  void onConnect(BLEServer *server);
  void onDisconnect(BLEServer *server);

  uint32_t onPassKeyRequest();
  void onPassKeyNotify(uint32_t passkey);
  bool onSecurityRequest();
  void onAuthenticationComplete(esp_ble_auth_cmpl_t);
  bool onConfirmPIN(uint32_t pin);

  bool connected();

  BLEServer *ble_server;
  BLEAdvertising *ble_adv;
  BLEService *SerialService;
  BLECharacteristic *TxCharacteristic;
  BLECharacteristic *RxCharacteristic;
  size_t transmitBufferLength;
  unsigned long long lastFlushTime;

private:
  BLESerial(BLESerial const &other) = delete;
  void operator=(BLESerial const &other) = delete;

  BLEFIFO<RX_BUFFER_SIZE> rx_buffer;
  size_t numAvailableLines;
  uint8_t transmitBuffer[BLE_BUFFER_SIZE];

  int ConnectedDeviceCount;
  void SetupSerialService();

  uint16_t peerMTU;
  uint16_t maxTransferSize = BLE_BUFFER_SIZE;

  bool checkMTU();

  const char *BLE_SERIAL_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
  const char *BLE_RX_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
  const char *BLE_TX_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

  bool started = false;
};
