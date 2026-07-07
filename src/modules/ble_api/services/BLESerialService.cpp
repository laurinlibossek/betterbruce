#if !defined(LITE_VERSION)
#include "BLESerialService.h"
#include <NimBLEDevice.h>

BLESerialService::BLESerialService() : BruceBLEService() {
    memset(rxBuffer, 0, BLE_RX_BUFFER_SIZE);
}

BLESerialService::~BLESerialService() {}

class BLESerialCallbacks : public NimBLECharacteristicCallbacks {
    BLESerialService *service;

    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override {
        // Push incoming BLE data into the ring buffer
        NimBLEAttValue val = pCharacteristic->getValue();
        service->pushToRxBuffer(val.data(), val.size());
    }

public:
    explicit BLESerialCallbacks(BLESerialService *svc) : service(svc) {}
};

void BLESerialService::pushToRxBuffer(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        size_t nextHead = (rxHead + 1) % BLE_RX_BUFFER_SIZE;
        if (nextHead == rxTail) {
            // Buffer full, drop oldest byte
            rxTail = (rxTail + 1) % BLE_RX_BUFFER_SIZE;
        }
        rxBuffer[rxHead] = data[i];
        rxHead = nextHead;
    }
}

void BLESerialService::setup(NimBLEServer *pServer) {
    pService = pServer->createService("4371ec0b-3d43-49f9-b731-7c72a4a7bb91");

    serial_char = pService->createCharacteristic(
        "d555ed97-bf2a-4f46-b3eb-d1fcdd7325e9", // Battery Level
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::WRITE
    );

    callbacks = new BLESerialCallbacks(this);
    serial_char->setCallbacks(callbacks);

    pService->start();
    pServer->getAdvertising()->addServiceUUID(pService->getUUID());
}

void BLESerialService::end() { delete callbacks; }

int BLESerialService::available() {
    if (rxHead >= rxTail) return rxHead - rxTail;
    return BLE_RX_BUFFER_SIZE - rxTail + rxHead;
}

size_t BLESerialService::readBytes(char *buffer, size_t length) {
    size_t count = 0;
    unsigned long start = millis();
    while (count < length && (millis() - start < 5000)) {
        if (available()) {
            buffer[count++] = rxBuffer[rxTail];
            rxTail = (rxTail + 1) % BLE_RX_BUFFER_SIZE;
            start = millis(); // Reset timeout on each byte received
        } else {
            delay(2);
        }
    }
    return count;
}

size_t BLESerialService::println(const String &s) {
    String toSend = s + "\r\n";
    serial_char->notify(toSend);
    vTaskDelay(pdMS_TO_TICKS(10)); // Add some delay to ensure data is read by the client
    return toSend.length();
}

size_t BLESerialService::print(const String &s) {
    serial_char->notify(s);
    vTaskDelay(pdMS_TO_TICKS(10));
    return s.length();
}

size_t BLESerialService::println(size_t n) {
    String s = String(n);
    return println(s);
}

void BLESerialService::vprintf(const char *fmt, va_list args) {
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    
    if (size <= 0) return;
    
    char *buf = new char[size + 1];
    vsnprintf(buf, size + 1, fmt, args);
    
    serial_char->notify(reinterpret_cast<const uint8_t *>(buf), size);
    vTaskDelay(pdMS_TO_TICKS(10));
    delete[] buf;
}

String BLESerialService::readStringUntil(char terminator) {
    String result = "";
    unsigned long start = millis();
    while (millis() - start < 5000) {
        if (available()) {
            char c = rxBuffer[rxTail];
            rxTail = (rxTail + 1) % BLE_RX_BUFFER_SIZE;
            if (c == terminator) break;
            result += c;
            start = millis(); // Reset timeout on data
        } else {
            delay(2);
        }
    }
    return result;
}

size_t BLESerialService::println(const uint32_t n) {
    String s = String(n);
    return println(s);
}

size_t BLESerialService::print(const int n, int format) {
    String s = String(n, format);
    return print(s);
}

size_t BLESerialService::println(const int n, int format) {
    String s = String(n, format);
    return println(s);
}

size_t BLESerialService::println() { return println(""); }

size_t BLESerialService::write(uint8_t *str, size_t size) {
    serial_char->notify(str, size);
    vTaskDelay(pdMS_TO_TICKS(10));
    return size;
}

void BLESerialService::setMTU(uint16_t mtu) { this->mtu = mtu; }

#endif
