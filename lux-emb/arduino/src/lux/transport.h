/**
 * Pyintel Lux — Transport Abstraction Layer
 * Defines the unified interface for all physical and wireless transports:
 * Serial (UART/USB), ESP-NOW, Wi-Fi UDP, BLE, and LoRa.
 */

#ifndef LUX_TRANSPORT_H
#define LUX_TRANSPORT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Maximum simultaneous active transports per board ────────────── */
#define LUX_MAX_TRANSPORTS      4
#define LUX_MAX_PAYLOAD_SIZE    128

/* ── Transport Identifiers ───────────────────────────────────────── */
typedef enum {
    LUX_TRANSPORT_SERIAL   = 0,
    LUX_TRANSPORT_ESPNOW   = 1,
    LUX_TRANSPORT_WIFI_UDP = 2,
    LUX_TRANSPORT_BLE      = 3,
    LUX_TRANSPORT_LORA     = 4,
    LUX_TRANSPORT_UNKNOWN  = 0xFF
} lux_transport_id_t;

/* ── Transport Bitmask Flags (for peer capabilities) ─────────────── */
#define LUX_FLAG_TRANSPORT_SERIAL   (1 << LUX_TRANSPORT_SERIAL)
#define LUX_FLAG_TRANSPORT_ESPNOW   (1 << LUX_TRANSPORT_ESPNOW)
#define LUX_FLAG_TRANSPORT_WIFI_UDP (1 << LUX_TRANSPORT_WIFI_UDP)
#define LUX_FLAG_TRANSPORT_BLE      (1 << LUX_TRANSPORT_BLE)
#define LUX_FLAG_TRANSPORT_LORA     (1 << LUX_TRANSPORT_LORA)

/* ── Transport Interface Definition ──────────────────────────────── */
typedef struct lux_transport {
    lux_transport_id_t id;
    const char        *name;           /* "Serial", "ESP-NOW", "WiFi-UDP", "BLE" */

    bool (*init)(void *config);
    bool (*is_available)(void);
    int  (*send)(const uint8_t *buf, size_t len, uint16_t dst_node);
    int  (*broadcast)(const uint8_t *buf, size_t len);
    int  (*receive)(uint8_t *buf, size_t max_len, uint16_t *src_node);
    int8_t (*get_rssi)(void);          /* RSSI in dBm, or 0 for wired */
    void (*deinit)(void);
} lux_transport_t;

/* ── Automatic Platform Hardware Capabilities Detection ──────────── */

/* ESP32 Family */
#if defined(ESP32)
  #define LUX_HAS_SERIAL    1
  #define LUX_HAS_ESPNOW    1
  #define LUX_HAS_WIFI_UDP  1
  #if defined(CONFIG_BT_ENABLED) || defined(CONFIG_BLUEDROID_ENABLED)
    #define LUX_HAS_BLE     1
  #else
    #define LUX_HAS_BLE     0
  #endif

/* ESP8266 */
#elif defined(ESP8266)
  #define LUX_HAS_SERIAL    1
  #define LUX_HAS_ESPNOW    1
  #define LUX_HAS_WIFI_UDP  1
  #define LUX_HAS_BLE       0

/* RP2040 / Pico */
#elif defined(ARDUINO_ARCH_RP2040)
  #define LUX_HAS_SERIAL    1
  #if defined(ARDUINO_RASPBERRY_PI_PICO_W)
    #define LUX_HAS_WIFI_UDP 1
    #define LUX_HAS_ESPNOW   0
    #define LUX_HAS_BLE      1
  #else
    #define LUX_HAS_WIFI_UDP 0
    #define LUX_HAS_ESPNOW   0
    #define LUX_HAS_BLE      0
  #endif

/* AVR / SAMD21 / STM32 (Serial only by default, SPI LoRa extensible) */
#elif defined(__AVR__) || defined(ARDUINO_ARCH_AVR) || defined(ARDUINO_ARCH_SAMD) || defined(STM32)
  #define LUX_HAS_SERIAL    1
  #define LUX_HAS_ESPNOW    0
  #define LUX_HAS_WIFI_UDP  0
  #define LUX_HAS_BLE       0

/* Generic default fallback */
#else
  #define LUX_HAS_SERIAL    1
  #define LUX_HAS_ESPNOW    0
  #define LUX_HAS_WIFI_UDP  0
  #define LUX_HAS_BLE       0
#endif

#ifdef __cplusplus
}
#endif

#endif /* LUX_TRANSPORT_H */
