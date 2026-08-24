/**
 * Pyintel Lux — Example 08: Mesh Sensor Broadcast
 * 
 * Demonstrates multi-node sensor telemetry broadcasting across a Lux mesh.
 * Each node periodically broadcasts its local sensors (temperature, uptime),
 * and prints telemetry received from all other nodes in the network.
 */

#include <Lux.h>

const char *MESH_NETWORK_UUID = "f47ac10b-58cc-4372-a567-0e02b2c3d479";

#define SYM_SENSOR_TEMP    0x0301
#define SYM_SENSOR_VOLT    0x0302

uint32_t last_sensor_tx = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("========================================"));
    Serial.println(F("  Pyintel Lux — Sensor Broadcast Node   "));
    Serial.println(F("========================================"));

    // Join mesh network
    Lux.beginMesh(MESH_NETWORK_UUID);

    Serial.print(F("Node Online. ID: 0x"));
    Serial.println(Lux.getNodeId(), HEX);

    // Register callback for temperature from any peer node
    Lux.onMessage(SYM_SENSOR_TEMP, [](uint16_t from_node, uint16_t symbol, const uint8_t *payload, uint8_t len) {
        float temp = 0.0f;
        if (len >= 4) {
            memcpy(&temp, payload, 4);
        }
        Serial.print(F("[TELEMETRY] Node 0x"));
        Serial.print(from_node, HEX);
        Serial.print(F(" Temp: "));
        Serial.print(temp, 1);
        Serial.println(F(" °C"));
    });

    // Register callback for voltage
    Lux.onMessage(SYM_SENSOR_VOLT, [](uint16_t from_node, uint16_t symbol, const uint8_t *payload, uint8_t len) {
        float volt = 0.0f;
        if (len >= 4) {
            memcpy(&volt, payload, 4);
        }
        Serial.print(F("[TELEMETRY] Node 0x"));
        Serial.print(from_node, HEX);
        Serial.print(F(" Battery: "));
        Serial.print(volt, 2);
        Serial.println(F(" V"));
    });
}

void loop() {
    // Non-blocking mesh ingestion
    Lux.tick();

    // Broadcast simulated local sensor readings every 2 seconds
    if (millis() - last_sensor_tx >= 2000) {
        float simulated_temp = 24.5f + (random(-20, 20) / 10.0f);
        float simulated_volt = 3.7f + (random(-10, 10) / 100.0f);

        Lux.broadcast(SYM_SENSOR_TEMP, simulated_temp);
        Lux.broadcast(SYM_SENSOR_VOLT, simulated_volt);

        last_sensor_tx = millis();
    }
}
