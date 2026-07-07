#pragma once

#include <Arduino.h>
#include <USB.h>
#include <USBHID.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#if defined(USB_as_HID) // Requires USB HID enabled

#include "u2f.h"

#define U2F_HID_PACKET_LEN 64

class USB_U2F : public USBHIDDevice {
private:
    USBHID hid;
    bool active = false;
    TaskHandle_t _taskHandle = NULL;
    QueueHandle_t _rxQueue = NULL;

    static void _u2fTaskConfig(void *pvParameters);

public:
    U2fData* u2f_instance = NULL;

    USB_U2F(void);
    void begin(U2fData* u2f_data);
    void end(void);

    // USBHIDDevice interface implementation
    uint16_t _onGetDescriptor(uint8_t* buffer) override;
    void _onOutput(uint8_t report_id, const uint8_t* buffer, uint16_t len) override;

    // Send 64-byte response
    void sendResponse(const uint8_t* buf);
};

extern USB_U2F U2F_HID;

#endif // USB_as_HID
