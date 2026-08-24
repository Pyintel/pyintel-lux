/*
 * Pyintel Lux — Example 10: ESP32 Transparent Serial-to-Wireless Mesh Bridge
 * 
 * Hardware Wiring:
 *   - External Node TX (Pico GP0 / Uno TX)  <--->  ESP32 Pin 16 (RX2)
 *   - External Node RX (Pico GP1 / Uno RX)  <--->  ESP32 Pin 17 (TX2)
 *   - External Node GND                     <--->  ESP32 GND
 */

#include <Lux.h>

#define NETWORK_UUID "f47ac10b-58cc-4372-a567-0e02b2c3d479"

#define BRIDGE_RX_PIN 16
#define BRIDGE_TX_PIN 17
#define BRIDGE_BAUD   115200

uint32_t last_heartbeat = 0;
uint8_t  bridge_buf[256];
uint8_t  bridge_idx = 0;

void onMeshMessage(uint16_t src_node, uint16_t symbol, const uint8_t *payload, uint8_t len) {
    // Relay remote wireless mesh messages down Serial2 to the wired Pico/Uno node
    // Format and send as a full mesh packet down Serial2
}

void setup() {
    // 1. USB Debug Serial to PC Web Dashboard
    Serial.begin(115200);
    delay(500);

    Serial.println(F("\n========================================"));
    Serial.println(F("  Pyintel Lux — ESP32 Mesh Bridge Node  "));
    Serial.println(F("========================================"));

    // 2. Hardware UART2 connected to Pico / Uno (Pins 16 RX2, 17 TX2)
    Serial2.begin(BRIDGE_BAUD, SERIAL_8N1, BRIDGE_RX_PIN, BRIDGE_TX_PIN);

    // 3. Start Lux Mesh on ESP-NOW + Wi-Fi
    Lux.beginMesh(NETWORK_UUID);
    Lux.debug(true, Serial);

    // 4. Wildcard listener
    Lux.onMessage(onMeshMessage);

    Serial.println(F("ESP32 Bridge Active! Forwarding between UART2 (Pins 16/17) <-> ESP-NOW Wireless..."));
}

void loop() {
    Lux.tick();

    // 1. Periodic Bridge Heartbeat (1 second)
    if (millis() - last_heartbeat >= 1000) {
        last_heartbeat = millis();
        Lux.broadcast(LUX_SYM_HEARTBEAT, (uint32_t)millis());
    }

    // 2. Ingest frames from Pico/Uno via Serial2 -> Blast to ESP-NOW Swarm & Web Dashboard
    while (Serial2.available()) {
        uint8_t b = Serial2.read();

        // Sync header check 'M' 'X' (0x4D, 0x58)
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
            lux_mesh_envelope_t *env = (lux_mesh_envelope_t *)bridge_buf;
            lux_frame_header_t  *inner = (lux_frame_header_t *)(bridge_buf + LUX_MESH_HEADER_SIZE);
            
            size_t total_expected = LUX_MESH_HEADER_SIZE + LUX_HEADER_SIZE + inner->payload_len;

            if (bridge_idx >= total_expected || bridge_idx >= sizeof(bridge_buf)) {
                // Relay Pico's intact mesh envelope (preserving its Node ID) to ESP-NOW and Peer Table!
                Lux.relayRawMeshFrame(bridge_buf, bridge_idx, LUX_TRANSPORT_SERIAL);
                
                bridge_idx = 0;
            }
        }
    }
}
