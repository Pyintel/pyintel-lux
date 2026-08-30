/**
 * Pyintel Lux — Mesh Engine Implementation
 * Utilities for CRC-32 UUID hashing and platform-specific unique Node ID derivation.
 */

#include "mesh.h"

#if defined(ESP32)
  #include <esp_system.h>
  #include <esp_mac.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#elif defined(ARDUINO_ARCH_RP2040)
  #include <pico/unique_id.h>
#elif defined(ARDUINO_ARCH_SAMD)
  #include <sam.h>
#elif defined(__AVR__) || defined(ARDUINO_ARCH_AVR)
  #include <avr/eeprom.h>
#endif

/* ── Standard IEEE 802.3 CRC-32 ───────────────────────────────────── */
uint32_t lux_crc32(const char *str) {
    if (!str) return 0;
    uint32_t crc = 0xFFFFFFFF;
    while (*str) {
        uint8_t byte = (uint8_t)*str++;
        crc ^= byte;
        for (uint8_t i = 0; i < 8; i++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(int32_t)(crc & 1)));
        }
    }
    return ~crc;
}

/* ── CRC-16 helper for node ID generation ─────────────────────────── */
static uint16_t calc_id_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    // Reserve 0x0000 and 0xFFFF
    if (crc == 0x0000) crc = 0x0001;
    if (crc == 0xFFFF) crc = 0xFFFE;
    return crc;
}

/* ── Platform-Specific Deterministic Node ID Derivation ───────────── */
uint16_t lux_generate_node_id(void) {
#if defined(ESP32)
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    return calc_id_crc16(mac, 6);

#elif defined(ESP8266)
    uint32_t chip_id = ESP.getChipId();
    uint8_t raw[4] = {
        (uint8_t)(chip_id & 0xFF),
        (uint8_t)((chip_id >> 8) & 0xFF),
        (uint8_t)((chip_id >> 16) & 0xFF),
        (uint8_t)((chip_id >> 24) & 0xFF)
    };
    return calc_id_crc16(raw, 4);

#elif defined(ARDUINO_ARCH_RP2040)
    pico_unique_board_id_t board_id;
    pico_get_unique_board_id(&board_id);
    return calc_id_crc16((const uint8_t *)board_id.id, PICO_UNIQUE_BOARD_ID_SIZE_BYTES);

#elif defined(ARDUINO_ARCH_SAMD)
    // Read SAMD21 128-bit factory unique serial number
    uint32_t serial[4];
    serial[0] = *(uint32_t *)0x0080A00C;
    serial[1] = *(uint32_t *)0x0080A040;
    serial[2] = *(uint32_t *)0x0080A044;
    serial[3] = *(uint32_t *)0x0080A048;
    return calc_id_crc16((const uint8_t *)serial, 16);

#elif defined(__AVR__) || defined(ARDUINO_ARCH_AVR)
    // On AVR ATmega328P, check EEPROM addresses 0x00-0x01
    uint16_t eeprom_id = eeprom_read_word((uint16_t *)0);
    if (eeprom_id != 0x0000 && eeprom_id != 0xFFFF) {
        return eeprom_id;
    }
    // Generate pseudo-random from analog noise and persist
    uint16_t noise = (uint16_t)(analogRead(A0) ^ (analogRead(A1) << 6) ^ (micros() & 0xFFFF));
    if (noise == 0x0000 || noise == 0xFFFF) noise = 0x0042;
    eeprom_write_word((uint16_t *)0, noise);
    return noise;

#elif defined(NRF52_SERIES) || defined(ARDUINO_ARCH_NRF52) || defined(ARDUINO_NRF52_ADAFRUIT) || defined(NRF51) || defined(ARDUINO_ARCH_NRF51)
    uint32_t dev_addr[2];
    dev_addr[0] = NRF_FICR->DEVICEADDR[0];
    dev_addr[1] = NRF_FICR->DEVICEADDR[1];
    return calc_id_crc16((const uint8_t *)dev_addr, 8);

#else
    return 0x0042; // Generic fallback
#endif
}
