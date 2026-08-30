/**
 * Pyintel Lux — Example 07: Mesh Ping Pong
 * 
 * Demonstrates autonomous multi-transport mesh networking between two boards.
 * When two boards running this sketch join the same Network UUID, they discover
 * each other and exchange live ping-pong messages over ESP-NOW, Wi-Fi UDP, or Serial.
 */

#include <Lux.h>

// Shared Network UUID — change this to create your own isolated mesh network
const char *MESH_NETWORK_UUID = "f47ac10b-58cc-4372-a567-0e02b2c3d479";

// Application Symbol IDs
#define SYM_PING    0x0201
#define SYM_PONG    0x0202

uint32_t last_ping_ms = 0;
uint32_t ping_counter = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("========================================"));
    Serial.println(F("  Pyintel Lux — Mesh Ping Pong Node     "));
    Serial.println(F("========================================"));

    // 1. Join the UUID mesh network across all available radios
    // (Optionally pass Wi-Fi SSID and Password as 2nd and 3rd parameters)
    Lux.beginMesh(MESH_NETWORK_UUID);

    // 2. Enable Live Mesh Debug Monitor & Promiscuous Logging
    Lux.debug(true);

    Serial.print(F("My Node ID: 0x"));
    Serial.println(Lux.getNodeId(), HEX);

    // 3. Register handler for incoming PING requests
    Lux.onMessage(SYM_PING, [](uint16_t from_node, uint16_t symbol, const uint8_t *payload, uint8_t len) {
        uint32_t received_count = 0;
        if (len >= 4) {
            received_count = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8) |
                             ((uint32_t)payload[2] << 16) | ((uint32_t)payload[3] << 24);
        }

        Serial.print(F("<- PING from Node 0x"));
        Serial.print(from_node, HEX);
        Serial.print(F(" | Val: "));
        Serial.println(received_count);

        // Reply directly with PONG
        Lux.send(from_node, SYM_PONG, received_count);
    });

    // 4. Register handler for incoming PONG responses
    Lux.onMessage(SYM_PONG, [](uint16_t from_node, uint16_t symbol, const uint8_t *payload, uint8_t len) {
        uint32_t pong_count = 0;
        if (len >= 4) {
            pong_count = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8) |
                         ((uint32_t)payload[2] << 16) | ((uint32_t)payload[3] << 24);
        }

        Serial.print(F("-> PONG received from Node 0x"));
        Serial.print(from_node, HEX);
        Serial.print(F(" | RTT Ack: "));
        Serial.println(pong_count);
    });

    Serial.println(F("Listening for peers on mesh... (type 'l' to list devices)"));
}

void loop() {
    // Background mesh engine polls all transports (ESP-NOW, UDP, Serial)
    Lux.tick();

    // Check for interactive user commands from Serial Monitor
    if (Serial.available() > 0) {
        char cmd = (char)Serial.read();
        if (cmd == 'l' || cmd == 'L') {
            Lux.list(Serial);
        }
    }

    // Broadcast a PING every 3 seconds
    if (millis() - last_ping_ms >= 3000) {
        ping_counter++;
        Serial.print(F("[TX] Broadcasting PING #"));
        Serial.println(ping_counter);

        Lux.broadcast(SYM_PING, ping_counter);
        last_ping_ms = millis();
    }
}
