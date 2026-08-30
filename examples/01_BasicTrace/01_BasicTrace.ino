/**
 * Pyintel Lux — Example 01: Basic Trace
 * 
 * Demonstrates basic initialization, periodic heartbeats, and typed trace emissions.
 */

#include <Lux.h>

#define SYM_LOOP_COUNTER  0x0101
#define SYM_ANALOG_A0     0x0102
#define SYM_UPTIME_SEC    0x0103

uint32_t counter = 0;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000); // Wait for native USB if needed

    // Initialize Lux with Serial transport
    Lux.begin(Serial);
}

void loop() {
    // Keep internal tick engine running (emits 1 Hz heartbeats automatically)
    Lux.tick();

    // Trace loop counter as uint32
    Lux.trace(SYM_LOOP_COUNTER, counter++);

    // Trace analog input A0 as uint16
    uint16_t adc_val = analogRead(A0);
    Lux.trace(SYM_ANALOG_A0, adc_val);

    // Trace float uptime
    float uptime_s = millis() / 1000.0f;
    Lux.trace(SYM_UPTIME_SEC, uptime_s);

    // Delay between iterations
    delay(50);
}
