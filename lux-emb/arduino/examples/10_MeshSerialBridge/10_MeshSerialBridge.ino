/*
 * Pyintel Lux — Example 10: ESP32 Transparent Serial-to-Wireless Mesh Bridge
 * 
 * Hardware Wiring:
 *   - Arduino Uno Pin 0 (RX)  <--->  ESP32 Pin 17 (TX2)
 *   - Arduino Uno Pin 1 (TX)  <--->  1k/2k Divider  <--->  ESP32 Pin 16 (RX2)
 *   - Arduino Uno GND         <--->  ESP32 GND
 * 
 * How It Works:
 *   1. ESP32 runs both ESP-NOW (Wireless 2.4GHz) and Serial2 (Hardware UART to Uno).
 *   2. Packets emitted by the Arduino Uno are transparently bridged to all ESP32s over ESP-NOW.
 *   3. Packets from remote ESP32s addressed to the Uno (or broadcast) are relayed down Serial2.
 *   4. The entire wireless swarm automatically discovers the Uno in their mesh peer tables!
 */

#include <Lux.h>

#define NETWORK_UUID "f47ac10b-58cc-4372-a567-0e02b2c3d479"

#define UNO_RX_PIN 16
#define UNO_TX_PIN 17
#define BRIDGE_BAUD 115200

// Buffer for UART bridging
uint8_t bridge_buf[256];
uint8_t bridge_idx = 0;

void setup() {
    // 1. USB Debug Serial to PC
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("\n========================================"));
    Serial.println(F("  Pyintel Lux — ESP32 Mesh Bridge Node  "));
    Serial.println(F("========================================"));

    // 2. Hardware UART2 connected to Arduino Uno
    Serial2.begin(BRIDGE_BAUD, SERIAL_8N1, UNO_RX_PIN, UNO_TX_PIN);

    // 3. Start Lux Mesh on ESP-NOW + Wi-Fi
    Lux.beginMesh(NETWORK_UUID);
    Lux.debug(true, Serial);

    Serial.println(F("ESP32 Bridge Active! Forwarding between Uno (UART2) <-> Swarm (ESP-NOW)..."));
}

void loop() {
    Lux.tick();

    // 1. Ingest frames from Arduino Uno via Serial2 -> Blast to ESP-NOW Swarm
    while (Serial2.available()) {
        uint8_t b = Serial2.read();

        // Check for Mesh frame sync 'M' 'X'
        if (bridge_idx == 0 && b != LUX_MESH_SYNC_0) {
            continue;
        }
        if (bridge_idx == 1 && b != LUX_MESH_SYNC_1) {
            bridge_idx = 0;
            continue;
        }

        bridge_buf[bridge_idx++] = b;

        // When we have enough bytes for header, determine full frame size
        if (bridge_idx >= (LUX_MESH_HEADER_SIZE + LUX_HEADER_SIZE)) {
            lux_frame_header_t *inner = (lux_frame_header_t *)(bridge_buf + LUX_MESH_HEADER_SIZE);
            size_t total_expected = LUX_MESH_HEADER_SIZE + LUX_HEADER_SIZE + inner->payload_len;

            if (bridge_idx >= total_expected || bridge_idx >= sizeof(bridge_buf)) {
                // Forward Uno's frame to all wireless mesh nodes over ESP-NOW!
                Lux.broadcast(inner->symbol_id, bridge_buf + LUX_MESH_HEADER_SIZE + LUX_HEADER_SIZE, inner->payload_len);
                bridge_idx = 0;
            }
        }
    }
}
