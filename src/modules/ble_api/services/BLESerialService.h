#pragma once
#if !defined(LITE_VERSION)
#include "BruceBLEService.hpp"

#include <SerialDevice.h>

#define BLE_RX_BUFFER_SIZE 4096

class BLESerialCallbacks;

class BLESerialService : public BruceBLEService, public SerialDevice {
    NimBLECharacteristic *serial_char = nullptr;
    BLESerialCallbacks *callbacks = nullptr;

    // Ring buffer for incoming BLE data
    uint8_t rxBuffer[BLE_RX_BUFFER_SIZE];
    volatile size_t rxHead = 0;
    volatile size_t rxTail = 0;

public:
    BLESerialService();
    ~BLESerialService() override;
    void setup(NimBLEServer *pServer) override;
    void end() override;
    size_t println() override;
    size_t println(size_t n) override;
    size_t println(const String &s) override;
    size_t println(int n, int format) override;
    size_t print(const String &s) override;
    size_t print(int n, int format = DEC) override;
    void vprintf(const char *str, va_list args) override;
    size_t println(uint32_t n) override;
    size_t write(uint8_t *str, size_t size) override;
    void flush() override {}
    String readStringUntil(char terminator) override;
    int available() override;
    size_t readBytes(char *buffer, size_t length) override;
    void setMTU(uint16_t mtu);

    // Called by BLESerialCallbacks to push data into ring buffer
    void pushToRxBuffer(const uint8_t *data, size_t len);
};
#endif
