#include "ble_airtags.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/utils.h"

#if __has_include(<NimBLEExtAdvertising.h>)
#define NIMBLE_V2_PLUS 1
#include <NimBLEDevice.h>
#else
#include <BLEDevice.h>
#endif

#include "esp_mac.h"

struct AirTagDevice {
    uint8_t mac[6];
    uint8_t payload[62]; // Max adv size
    size_t payload_len;
    int8_t rssi;
};

static std::vector<AirTagDevice> detected_airtags;
static int selected_airtag_idx = -1;
static BLEScan *pAirTagScan = nullptr;

#ifdef NIMBLE_V2_PLUS
class AirTagCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice *advertisedDevice) override {
#else
class AirTagCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice *advertisedDevice) {
#endif
        const uint8_t *payload = advertisedDevice->getPayload().data();
        size_t len = advertisedDevice->getPayload().size();

        bool is_airtag = false;
        if (len >= 4) {
            for (size_t i = 0; i <= len - 4; i++) {
                if ((payload[i] == 0x1E && payload[i + 1] == 0xFF &&
                     payload[i + 2] == 0x4C && payload[i + 3] == 0x00) ||
                    (payload[i] == 0x4C && payload[i + 1] == 0x00 &&
                     payload[i + 2] == 0x12 && payload[i + 3] == 0x19)) {
                    is_airtag = true;
                    break;
                }
            }
        }

        if (is_airtag) {
            NimBLEAddress addr = advertisedDevice->getAddress();
            const uint8_t* mac = addr.getVal();

            bool found = false;
            for (auto& tag : detected_airtags) {
                if (memcmp(tag.mac, mac, 6) == 0) {
                    tag.rssi = advertisedDevice->getRSSI();
                    found = true;
                    break;
                }
            }
            if (!found && detected_airtags.size() < 30) {
                AirTagDevice new_tag;
                memcpy(new_tag.mac, mac, 6);
                new_tag.rssi = advertisedDevice->getRSSI();
                new_tag.payload_len = len > sizeof(new_tag.payload) ? sizeof(new_tag.payload) : len;
                memcpy(new_tag.payload, payload, new_tag.payload_len);
                detected_airtags.push_back(new_tag);
            }
        }
    }
};

void airtagScan() {
    detected_airtags.clear();
    selected_airtag_idx = -1;

    displayTextLine("Scanning for AirTags...");

    BLEDevice::init("");
    pAirTagScan = BLEDevice::getScan();
#ifdef NIMBLE_V2_PLUS
    pAirTagScan->setScanCallbacks(new AirTagCallbacks());
#else
    pAirTagScan->setAdvertisedDeviceCallbacks(new AirTagCallbacks());
#endif
    pAirTagScan->setActiveScan(true);
    pAirTagScan->setInterval(100);
    pAirTagScan->setWindow(99);

    pAirTagScan->start(0, false); // Start in background (duration 0 = forever)

    unsigned long startTime = millis();

    tft.fillScreen(bruceConfig.bgColor);
    drawMainBorderWithTitle("AirTag Sniffer");
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(FM);

    int last_sec = -1;
    int last_found = -1;

    while (millis() - startTime < 10000) { // 10 seconds
        if (check(EscPress)) {
            break;
        }

        int curr_sec = 10 - ((millis() - startTime) / 1000);
        int curr_found = detected_airtags.size();

        if (curr_sec != last_sec) {
            tft.fillRect(10, 70, 200, 20, bruceConfig.bgColor);
            tft.setCursor(10, 70);
            tft.printf("Scanning... %ds", curr_sec);
            last_sec = curr_sec;
        }

        if (curr_found != last_found) {
            tft.fillRect(10, 100, 200, 20, bruceConfig.bgColor);
            tft.setCursor(10, 100);
            tft.printf("Found: %d", curr_found);
            last_found = curr_found;
        }

        delay(50);
    }
    pAirTagScan->stop();
    pAirTagScan->clearResults();

    if (detected_airtags.empty()) {
        displayError("No AirTags found.");
        delay(1000);
        return;
    }

    std::vector<Option> tagOptions;
    for (size_t i = 0; i < detected_airtags.size(); i++) {
        char macStr[13];
        snprintf(macStr, sizeof(macStr), "%02X%02X%02X%02X%02X%02X",
                 detected_airtags[i].mac[0], detected_airtags[i].mac[1], detected_airtags[i].mac[2],
                 detected_airtags[i].mac[3], detected_airtags[i].mac[4], detected_airtags[i].mac[5]);

        String label = String(macStr) + " [" + String(detected_airtags[i].rssi) + "]";
        tagOptions.push_back({label.c_str(), [i]() {
            selected_airtag_idx = i;
        }});
    }
    tagOptions.push_back({"Cancel", []() { returnToMenu = true; }});

    loopOptions(tagOptions, MENU_TYPE_SUBMENU, "Select AirTag");
}

void airtagSpoof() {
    if (selected_airtag_idx < 0 || selected_airtag_idx >= detected_airtags.size()) {
        displayError("No AirTag selected!");
        delay(1000);
        return;
    }

    AirTagDevice& tag = detected_airtags[selected_airtag_idx];

    drawMainBorderWithTitle("Spoofing AirTag");
    padprintln("");
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             tag.mac[0], tag.mac[1], tag.mac[2], tag.mac[3], tag.mac[4], tag.mac[5]);
    padprintln(String("MAC: ") + macStr);

    String payloadStr = "Data: ";
    int printLen = tag.payload_len > 12 ? 12 : tag.payload_len;
    for (int i = 0; i < printLen; i++) {
        char hex[4];
        snprintf(hex, sizeof(hex), "%02X ", tag.payload[i]);
        payloadStr += hex;
    }
    padprintln(payloadStr);
    padprintln("Press ESC to stop");

    uint8_t newMac[6];
    memcpy(newMac, tag.mac, 6);
    // Setting random bit in MSB for random address format if required by ESP
    if ((newMac[5] & 0xC0) != 0xC0) {
        newMac[5] = (newMac[5] & 0x3F) | 0xC0;
    }
#ifdef NIMBLE_V2_PLUS
    esp_base_mac_addr_set(newMac);
#endif

    BLEDevice::init("");
    BLEAdvertising* pAdv = BLEDevice::getAdvertising();

    BLEAdvertisementData advData;

    uint8_t* mfg_data_start = nullptr;
    size_t mfg_data_len = 0;
    size_t idx = 0;
    while (idx < tag.payload_len) {
        uint8_t flen = tag.payload[idx];
        if (flen == 0 || idx + flen >= tag.payload_len) break;
        if (tag.payload[idx+1] == 0xFF && flen >= 3) {
            mfg_data_start = &tag.payload[idx+2];
            mfg_data_len = flen - 1;
            break;
        }
        idx += flen + 1;
    }

    if (mfg_data_start) {
        advData.setFlags(0x1A);

        uint8_t adv_buf[31];
        size_t aben = 0;
        size_t max_mfg_len = 29 - 2;
        size_t copy_len = mfg_data_len > max_mfg_len ? max_mfg_len : mfg_data_len;

        adv_buf[aben++] = copy_len + 1;
        adv_buf[aben++] = 0xFF;
        memcpy(&adv_buf[aben], mfg_data_start, copy_len);
        aben += copy_len;

#ifdef NIMBLE_V2_PLUS
        advData.addData(adv_buf, aben);
#else
        std::vector<uint8_t> payloadVector(adv_buf, adv_buf + aben);
        advData.addData(payloadVector);
#endif
    } else {
        // Fallback
        advData.setFlags(0x1A);
        if (tag.payload_len > 2) {
#ifdef NIMBLE_V2_PLUS
            advData.addData(&tag.payload[2], tag.payload_len - 2);
#else
            std::vector<uint8_t> payloadVector(&tag.payload[2], &tag.payload[2] + tag.payload_len - 2);
            advData.addData(payloadVector);
#endif
        }
    }

    pAdv->setAdvertisementData(advData);
    pAdv->start();

    while (1) {
        if (check(EscPress)) {
            pAdv->stop();
            break;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void airtagMenu() {
    std::vector<Option> options;
    options.push_back({"Scan AirTags", [](){ airtagScan(); }});
    options.push_back({"Spoof Selected", [](){ airtagSpoof(); }});
    options.push_back({"Back", [](){ returnToMenu = true; }});
    loopOptions(options, MENU_TYPE_SUBMENU, "AirTag Tools");
}


// --- Flipper Detect Logic ---

struct FlipperDevice {
    uint8_t mac[6];
    char name[32];
    int8_t rssi;
    String type;
};
static std::vector<FlipperDevice> detected_flippers;
static BLEScan *pFlipperScan = nullptr;

#ifdef NIMBLE_V2_PLUS
class FlipperCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice *advertisedDevice) override {
#else
class FlipperCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice *advertisedDevice) {
#endif
        const uint8_t *payload = advertisedDevice->getPayload().data();
        size_t len = advertisedDevice->getPayload().size();

        String type_found = "";

        size_t idx = 0;
        while (idx < len) {
            uint8_t flen = payload[idx];
            if (flen == 0 || idx + flen >= len) break;
            uint8_t ftype = payload[idx + 1];
            uint8_t payload_len = flen - 1;
            const uint8_t *fpayload = &payload[idx + 2];

            auto checkUuid = [&](uint16_t uuid) {
                if (uuid == 0x3082) { if (type_found != "White") type_found = "White"; }
                else if (uuid == 0x3081) { if (type_found.isEmpty()) type_found = "Black"; }
                else if (uuid == 0x3083) { if (type_found != "White") type_found = "Transparent"; }
            };

            if ((ftype == 0x02 || ftype == 0x03) && payload_len >= 2) { // 16-bit UUIDs
                for (size_t i = 0; i + 1 < payload_len; i += 2) {
                    uint16_t uuid = fpayload[i] | (fpayload[i + 1] << 8);
                    checkUuid(uuid);
                }
            } else if ((ftype == 0x04 || ftype == 0x05) && payload_len >= 4) { // 32-bit UUIDs
                for (size_t i = 0; i + 3 < payload_len; i += 4) {
                    uint16_t uuid = fpayload[i] | (fpayload[i + 1] << 8); // Lower 16-bits
                    checkUuid(uuid);
                }
            } else if ((ftype == 0x06 || ftype == 0x07) && payload_len >= 16) { // 128-bit UUIDs
                for (size_t i = 0; i + 15 < payload_len; i += 16) {
                    for (int j = 0; j <= 14; j++) {
                        uint16_t uuid = fpayload[i + j] | (fpayload[i + j + 1] << 8);
                        checkUuid(uuid);
                    }
                }
            }
            if (!type_found.isEmpty() && type_found == "White") break; // White is highest priority
            idx += flen + 1;
        }

        if (!type_found.isEmpty()) {
            NimBLEAddress addr = advertisedDevice->getAddress();
            const uint8_t* mac = addr.getVal();

            bool found = false;
            for (auto& f : detected_flippers) {
                if (memcmp(f.mac, mac, 6) == 0) {
                    f.rssi = advertisedDevice->getRSSI();
                    found = true;
                    break;
                }
            }
            if (!found && detected_flippers.size() < 30) {
                FlipperDevice f;
                memcpy(f.mac, mac, 6);
                f.rssi = advertisedDevice->getRSSI();
                f.type = type_found;
                String n = advertisedDevice->getName().c_str();
                strncpy(f.name, n.c_str(), 31);
                f.name[31] = '\0';
                detected_flippers.push_back(f);
            }
        }
    }
};

void flipperScan() {
    detected_flippers.clear();

    BLEDevice::init("");
    pFlipperScan = BLEDevice::getScan();
#ifdef NIMBLE_V2_PLUS
    pFlipperScan->setScanCallbacks(new FlipperCallbacks());
#else
    pFlipperScan->setAdvertisedDeviceCallbacks(new FlipperCallbacks());
#endif
    pFlipperScan->setActiveScan(true);
    pFlipperScan->setInterval(100);
    pFlipperScan->setWindow(99);

    pFlipperScan->start(0, false);

    unsigned long startTime = millis();

    tft.fillScreen(bruceConfig.bgColor);
    drawMainBorderWithTitle("Detect Flipper");
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(FM);

    int last_sec = -1;
    int last_found = -1;

    while (millis() - startTime < 10000) { // 10 seconds
        if (check(EscPress)) {
            break;
        }

        int curr_sec = 10 - ((millis() - startTime) / 1000);
        int curr_found = detected_flippers.size();

        if (curr_sec != last_sec) {
            tft.fillRect(10, 70, 200, 20, bruceConfig.bgColor);
            tft.setCursor(10, 70);
            tft.printf("Scanning... %ds", curr_sec);
            last_sec = curr_sec;
        }

        if (curr_found != last_found) {
            tft.fillRect(10, 100, 200, 20, bruceConfig.bgColor);
            tft.setCursor(10, 100);
            tft.printf("Found: %d", curr_found);
            last_found = curr_found;
        }

        delay(50);
    }
    pFlipperScan->stop();
    pFlipperScan->clearResults();

    if (detected_flippers.empty()) {
        displayError("No Flippers found.");
        delay(1000);
        return;
    }

    std::vector<Option> flipperOptions;
    for (size_t i = 0; i < detected_flippers.size(); i++) {
        char macStr[18];
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 detected_flippers[i].mac[0], detected_flippers[i].mac[1], detected_flippers[i].mac[2],
                 detected_flippers[i].mac[3], detected_flippers[i].mac[4], detected_flippers[i].mac[5]);

        String label = String(detected_flippers[i].name);
        if (label.isEmpty() || label == "<no name>") label = String(macStr);
        label += " [" + detected_flippers[i].type + "]";

        flipperOptions.push_back({label.c_str(), [i, macStr]() {
            drawMainBorderWithTitle("Flipper Info");
            tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
            tft.setTextSize(FM);
            tft.drawString(String("MAC: ") + macStr, 10, 48);
            tft.drawString("Name: " + String(detected_flippers[i].name), 10, 66);
            tft.drawString("Type: " + detected_flippers[i].type, 10, 84);
            tft.drawString("Signal: " + String(detected_flippers[i].rssi) + " dBm", 10, 102);

            tft.drawCentreString("Press " + String(BTN_ALIAS) + " to close", tftWidth / 2, tftHeight - 20, 1);
            delay(300);
            while (1) {
                if (check(SelPress) || check(EscPress) || check(PrevPress)) break;
                delay(10);
            }
        }});
    }
    flipperOptions.push_back({"Cancel", []() { returnToMenu = true; }});

    loopOptions(flipperOptions, MENU_TYPE_SUBMENU, "Select Flipper");
}

void flipperMenu() {
    flipperScan();
}
