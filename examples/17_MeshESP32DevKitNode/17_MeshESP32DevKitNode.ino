/**
 * Pyintel Lux — ESP32 DevKit Multi-Bearer Mesh Node
 * 
 * Hardware: ESP32 DevKit (WROOM-32 / WROVER)
 * Transports Active: ESP-NOW (2.4GHz) + Wi-Fi UDP + BLE + USB Serial
 * Features:
 *   - Auto-joins universal Pyintel Lux swarm via Network UUID
 *   - Emits internal temperature & uptime telemetry
 *   - Supports initBorder(), lockTopology(), unlockBorder(), and knock()
 *   - Built-in Auto-Healing watchdog
 */

#include <Arduino.h>
#include <Lux.h>

#define NETWORK_UUID "f47ac10b-58cc-4372-a567-0e02b2c3d479"

// Custom symbols
#define SYM_ESP32_TEMP        0x0350  // float (deg C)
#define SYM_ESP32_HALL        0x0351  // int32
#define SYM_ESP32_HEAP        0x0352  // uint32 (free heap bytes)
#define SYM_ESP32_LED_CMD     0x0353  // uint8 (0=OFF, 1=ON)

#define PIN_LED               2       // Built-in LED on most ESP32 DevKits

static uint32_t last_send = 0;

#ifdef __cplusplus
extern "C" {
uint8_t temprature_sens_read();
}
#endif

float readESP32Temperature() {
#if defined(temprature_sens_read)
    // Legacy internal temperature sensor
    return (temprature_sens_read() - 32.0f) / 1.8f;
#else
    return temperatureRead(); // Modern ESP-IDF API
#endif
}

void onLedCommand(uint16_t from_node, uint16_t symbol, const uint8_t *payload, uint8_t len) {
    if (len >= 1) {
        digitalWrite(PIN_LED, payload[0] ? HIGH : LOW);
        Lux.trace(0x0202, (uint8_t)payload[0]); // Acknowledge
    }
}

void setup() {
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);

    Serial.begin(115200);
    delay(500);

    // Initialize multi-bearer mesh engine on universal network UUID
    Lux.beginMesh(NETWORK_UUID, Serial, 115200);

    // Enable auto-healing with 8s watchdog
    Lux.setAutoHealing(true, 8000);

    // Register actuators & handlers
    Lux.onMessage(SYM_ESP32_LED_CMD, onLedCommand);

    Lux.deviceInfo("ESP32-DevKit-P2P", 1);
}

void loop() {
    Lux.tick();

    // Handle interactive PC CLI / Dashboard commands
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'p' || c == 'P') {
            Lux.broadcast(0x0201, (uint32_t)millis()); // SYM_PING
        } else if (c == 'l' || c == 'L') {
            Lux.list(Serial);
        } else if (c == 'b' || c == 'B') {
            Lux.initBorder();
        } else if (c == 'u' || c == 'U') {
            Lux.unlockBorder();
        } else if (c == 'k' || c == 'K') {
            Lux.knock();
        }
    }

    // Periodic sensor telemetry broadcast every 2 seconds
    if (millis() - last_send >= 2000) {
        last_send = millis();

        float temp_c = readESP32Temperature();
        Lux.broadcast(SYM_ESP32_TEMP, temp_c);

        uint32_t free_heap = ESP.getFreeHeap();
        Lux.broadcast(SYM_ESP32_HEAP, free_heap);
    }
}
