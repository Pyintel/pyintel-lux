/*
 * Pyintel Lux — Example 11: Arduino Uno R3 Pure UART Mesh Node
 * 
 * Hardware:
 *   - Arduino Uno R3 (ATmega328P @ 16MHz, NO Wi-Fi/BLE)
 *   - Connect Uno Pin 0 (RX) -> ESP32 Pin 17 (TX2)
 *   - Connect Uno Pin 1 (TX) -> 1k/2k resistor voltage divider -> ESP32 Pin 16 (RX2)
 *   - Connect Uno GND -> ESP32 GND
 * 
 * Behavior:
 *   - Joins the Lux Mesh network over UART Serial.
 *   - The ESP32 acts as a transparent wireless bridge over ESP-NOW.
 *   - Every node on the wireless mesh can send/receive telemetry to/from this Uno!
 */

#include <Lux.h>

#define NETWORK_UUID "f47ac10b-58cc-4372-a567-0e02b2c3d479"

#define SYM_UNO_POT_VAL   0x0310
#define SYM_UNO_LED_CMD   0x0311

uint32_t last_send = 0;
uint16_t sample_counter = 0;

void onLedCommand(uint16_t src_node, uint16_t symbol, const uint8_t *payload, uint8_t len) {
    if (len >= 1) {
        digitalWrite(LED_BUILTIN, payload[0] ? HIGH : LOW);
    }
}

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    // Initialize Uno on hardware Serial @ 115200 baud
    Serial.begin(115200);

    // Join the Lux Mesh network using Serial transport
    Lux.beginMesh(NETWORK_UUID, Serial, 115200);

    // Register callback for wireless LED commands from any ESP32 node
    Lux.onMessage(SYM_UNO_LED_CMD, onLedCommand);
}

void loop() {
    Lux.tick();

    // Broadcast analog sensor telemetry to the entire wireless mesh every 2 seconds
    if (millis() - last_send >= 2000) {
        last_send = millis();
        sample_counter++;

        // Read analog pin A0 (potentiometer / sensor)
        uint16_t sensor_val = analogRead(A0);

        // Broadcast to all ESP32s on the mesh!
        Lux.broadcast(SYM_UNO_POT_VAL, sensor_val);
    }
}
