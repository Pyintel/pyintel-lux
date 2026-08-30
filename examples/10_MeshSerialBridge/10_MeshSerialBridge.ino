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
    // Forward remote wireless mesh messages down Serial2 to the wired Pico node!
    if (src_node != Lux.getNodeId()) {
        uint8_t out_buf[128];
        lux_mesh_envelope_t *env = (lux_mesh_envelope_t *)out_buf;
        env->sync[0] = LUX_MESH_SYNC_0;
        env->sync[1] = LUX_MESH_SYNC_1;
        env->net_hash = Lux.getNetworkHash();
        env->src_node = src_node;
        env->dst_node = LUX_NODE_BROADCAST;
        env->hop_count = 3;
        env->flags = 0;

        lux_frame_header_t *inner = (lux_frame_header_t *)(out_buf + LUX_MESH_HEADER_SIZE);
        inner->sync[0] = 'L';
        inner->sync[1] = 'X';
        inner->seq_num = 1;
        inner->symbol_id = symbol;
        inner->timestamp_us = micros();
        inner->payload_type = (symbol == LUX_SYM_HEARTBEAT) ? LUX_TYPE_U32 : LUX_TYPE_BYTES;
        inner->payload_len = len;
        inner->crc16 = lux_crc16((const uint8_t *)inner, 12);

        if (payload && len > 0 && len <= (sizeof(out_buf) - LUX_MESH_HEADER_SIZE - LUX_HEADER_SIZE)) {
            memcpy(out_buf + LUX_MESH_HEADER_SIZE + LUX_HEADER_SIZE, payload, len);
        }

        size_t total = LUX_MESH_HEADER_SIZE + LUX_HEADER_SIZE + len;
        Serial2.write(out_buf, total);
    }
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

        // Also emit Bridge's own heartbeat down Serial2 so Pico discovers the Bridge Node too!
        uint8_t out_buf[64];
        lux_mesh_envelope_t *env = (lux_mesh_envelope_t *)out_buf;
        env->sync[0] = LUX_MESH_SYNC_0;
        env->sync[1] = LUX_MESH_SYNC_1;
        env->net_hash = Lux.getNetworkHash();
        env->src_node = Lux.getNodeId();
        env->dst_node = LUX_NODE_BROADCAST;
        env->hop_count = 3;
        env->flags = 0;

        lux_frame_header_t *inner = (lux_frame_header_t *)(out_buf + LUX_MESH_HEADER_SIZE);
        inner->sync[0] = 'L';
        inner->sync[1] = 'X';
        inner->seq_num = 1;
        inner->symbol_id = LUX_SYM_HEARTBEAT;
        inner->timestamp_us = micros();
        inner->payload_type = LUX_TYPE_U32;
        inner->payload_len = 4;
        inner->crc16 = lux_crc16((const uint8_t *)inner, 12);

        uint32_t up = (uint32_t)millis();
        memcpy(out_buf + LUX_MESH_HEADER_SIZE + LUX_HEADER_SIZE, &up, 4);

        Serial2.write(out_buf, LUX_MESH_HEADER_SIZE + LUX_HEADER_SIZE + 4);
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
