/**
 * Pyintel Lux — ESP32-S3 Multi-Bearer Mesh Node
 * 
 * Hardware: ESP32-S3 (Dual Core LX7, Native USB CDC / JTAG)
 * Transports Active: ESP-NOW (2.4GHz) + Wi-Fi UDP + BLE + USB Serial
 * Features:
 *   - Auto-joins universal Pyintel Lux swarm via Network UUID
 *   - Emits internal temperature, heap & sensor telemetry
 *   - Supports initBorder(), lockTopology(), unlockBorder(), and knock()
 *   - Built-in Auto-Healing watchdog
 */

#include <Arduino.h>
#include <Lux.h>

#define NETWORK_UUID "f47ac10b-58cc-4372-a567-0e02b2c3d479"

// Custom symbols
#define SYM_ESP32S3_TEMP       0x0360  // float (deg C)
#define SYM_ESP32S3_HEAP       0x0361  // uint32 (free heap bytes)
#define SYM_ESP32S3_LED_CMD    0x0362  // uint8 (0=OFF, 1=ON)

#ifdef RGB_BUILTIN
#define PIN_LED RGB_BUILTIN
#elif defined(LED_BUILTIN)
#define PIN_LED LED_BUILTIN
#else
#define PIN_LED 2
#endif

static uint32_t last_send = 0;

void onLedCommand(uint16_t from_node, uint16_t symbol, const uint8_t *payload, uint8_t len) {
    if (len >= 1) {
#ifdef RGB_BUILTIN
        if (payload[0]) {
            neopixelWrite(RGB_BUILTIN, 0, 32, 64); // Cyan
        } else {
            neopixelWrite(RGB_BUILTIN, 0, 0, 0);   // OFF
        }
#else
        digitalWrite(PIN_LED, payload[0] ? HIGH : LOW);
#endif
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
    Lux.onMessage(SYM_ESP32S3_LED_CMD, onLedCommand);

    Lux.deviceInfo("ESP32-S3-P2P", 1);
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

        float temp_c = temperatureRead();
        Lux.broadcast(SYM_ESP32S3_TEMP, temp_c);

        uint32_t free_heap = ESP.getFreeHeap();
        Lux.broadcast(SYM_ESP32S3_HEAP, free_heap);
    }
}
