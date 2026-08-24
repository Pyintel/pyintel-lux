/**
 * Pyintel Lux — Example 02: MultiSensor
 * 
 * Demonstrates high-frequency multi-channel sensor telemetry with minimal CPU overhead.
 */

#include <Lux.h>

#define SYM_SENSOR_VOLTAGE   0x0201
#define SYM_SENSOR_CURRENT   0x0202
#define SYM_SENSOR_POWER     0x0203
#define SYM_SENSOR_TEMP      0x0204
#define SYM_SYSTEM_STATE     0x0205

enum SystemState {
    STATE_IDLE = 0,
    STATE_SAMPLING = 1,
    STATE_TRANSMITTING = 2
};

void setup() {
    Serial.begin(115200);
    Lux.begin(Serial);

    // Announce device capabilities to host
    Lux.deviceInfo("LuxSensorNode-v1");
}

void loop() {
    Lux.tick();

    // Read simulated or physical sensor metrics
    float voltage = (analogRead(A0) * 5.0f) / 1023.0f;
    float current = (analogRead(A1) * 2.5f) / 1023.0f;
    float power = voltage * current;
    float temperature = 25.0f + (analogRead(A2) * 0.1f);

    // Emit sensor values in rapid succession (batched automatically into single TX buffer)
    Lux.trace(SYM_SENSOR_VOLTAGE, voltage);
    Lux.trace(SYM_SENSOR_CURRENT, current);
    Lux.trace(SYM_SENSOR_POWER, power);
    Lux.trace(SYM_SENSOR_TEMP, temperature);
    Lux.trace(SYM_SYSTEM_STATE, (uint8_t)STATE_SAMPLING);

    // Flush batch to wire
    Lux.flush();

    delay(25);
}
