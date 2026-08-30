/**
 * Pyintel Lux — Universal BLE GAP Broadcast Transport
 * Enables cross-vendor mesh broadcasting from ESP32/ESP32-S3 to Nordic nRF51/nRF52.
 */

#include "transport_ble.h"
#include "../mesh.h"
#include <string.h>

#define LUX_BLE_COMPANY_ID_0    0xFF
#define LUX_BLE_COMPANY_ID_1    0xFF

/* ═══════════════════════════════════════════════════════════════════
   1. NORDIC nRF52 (ADAFRUIT CLUE)
   ═══════════════════════════════════════════════════════════════════ */
#if defined(ARDUINO_NRF52_ADAFRUIT) || defined(NRF52_SERIES) || defined(ARDUINO_ARCH_NRF52)

#include <bluefruit.h>

static bool s_ble_initialized = false;

static bool ble_init(void *config) {
    (void)config;
    if (s_ble_initialized) return true;

    if (!Bluefruit.begin()) return false;
    Bluefruit.setTxPower(4);
    Bluefruit.setName("Lux-CLUE");

    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.restartOnDisconnect(true);
    Bluefruit.Advertising.setInterval(32, 244);
    Bluefruit.Advertising.start(0);

    s_ble_initialized = true;
    return true;
}

static bool ble_is_available(void) { return s_ble_initialized; }

static int ble_broadcast(const uint8_t *buf, size_t len) {
    if (!s_ble_initialized || !buf || len == 0) return -1;
    if (len > 26) len = 26;

    uint8_t mfg_data[28];
    mfg_data[0] = LUX_BLE_COMPANY_ID_0;
    mfg_data[1] = LUX_BLE_COMPANY_ID_1;
    memcpy(mfg_data + 2, buf, len);

    Bluefruit.Advertising.clearData();
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addManufacturerData(mfg_data, len + 2);
    Bluefruit.Advertising.start(1);
    return (int)len;
}

static int ble_send(const uint8_t *buf, size_t len, uint16_t dst_node) {
    (void)dst_node;
    return ble_broadcast(buf, len);
}

static int ble_receive(uint8_t *buf, size_t max_len, uint16_t *src_node) {
    (void)buf; (void)max_len; (void)src_node;
    return 0;
}

static int8_t ble_get_rssi(void) { return -50; }
static void ble_deinit(void) {
    if (s_ble_initialized) {
        Bluefruit.Advertising.stop();
        s_ble_initialized = false;
    }
}

/* ═══════════════════════════════════════════════════════════════════
   2. ESP32 & ESP32-S3
   ═══════════════════════════════════════════════════════════════════ */
#elif defined(ESP32)

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEAdvertising.h>

static bool s_esp32_ble_initialized = false;

static bool ble_init(void *config) {
    (void)config;
    if (s_esp32_ble_initialized) return true;

    BLEDevice::init("Lux-ESP32");
    s_esp32_ble_initialized = true;
    return true;
}

static bool ble_is_available(void) {
    return s_esp32_ble_initialized;
}

static int ble_broadcast(const uint8_t *buf, size_t len) {
    if (!s_esp32_ble_initialized || !buf || len == 0) return -1;
    if (len > 26) len = 26;

    BLEAdvertisementData advData;
    advData.setFlags(0x06); // General Discoverable

    String mfgData = "";
    mfgData += (char)LUX_BLE_COMPANY_ID_0;
    mfgData += (char)LUX_BLE_COMPANY_ID_1;
    for (size_t i = 0; i < len; i++) {
        mfgData += (char)buf[i];
    }
    advData.setManufacturerData(mfgData);

    BLEAdvertising *pAdv = BLEDevice::getAdvertising();
    pAdv->stop();
    pAdv->setAdvertisementData(advData);
    pAdv->start();
    return (int)len;
}

static int ble_send(const uint8_t *buf, size_t len, uint16_t dst_node) {
    (void)dst_node;
    return ble_broadcast(buf, len);
}

static int ble_receive(uint8_t *buf, size_t max_len, uint16_t *src_node) {
    (void)buf; (void)max_len; (void)src_node;
    return 0;
}

static int8_t ble_get_rssi(void) { return -50; }
static void ble_deinit(void) {
    if (s_esp32_ble_initialized) {
        BLEDevice::deinit(false);
        s_esp32_ble_initialized = false;
    }
}

#else
static bool ble_init(void *config) { (void)config; return false; }
static bool ble_is_available(void) { return false; }
static int ble_send(const uint8_t *buf, size_t len, uint16_t dst_node) { (void)buf; (void)len; (void)dst_node; return -1; }
static int ble_broadcast(const uint8_t *buf, size_t len) { (void)buf; (void)len; return -1; }
static int ble_receive(uint8_t *buf, size_t max_len, uint16_t *src_node) { (void)buf; (void)max_len; (void)src_node; return 0; }
static int8_t ble_get_rssi(void) { return -127; }
static void ble_deinit(void) {}
#endif

lux_transport_t lux_transport_ble = {
    .id           = LUX_TRANSPORT_BLE,
    .name         = "BLE",
    .init         = ble_init,
    .is_available = ble_is_available,
    .send         = ble_send,
    .broadcast    = ble_broadcast,
    .receive      = ble_receive,
    .get_rssi     = ble_get_rssi,
    .deinit       = ble_deinit
};
