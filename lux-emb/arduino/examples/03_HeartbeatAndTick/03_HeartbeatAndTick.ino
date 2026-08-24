/**
 * Pyintel Lux — Example 03: Heartbeat and Tick Engine
 * 
 * Demonstrates non-blocking background polling, customizable heartbeat rates,
 * and memory telemetry without blocking user application code.
 */

#include <Lux.h>

#define SYM_FREE_RAM_BYTES  0x0301
#define SYM_CUSTOM_EVENT    0x0302

uint32_t last_event_ms = 0;

void setup() {
    Serial.begin(115200);
    
    // Start Lux
    Lux.begin(Serial);

    // Set custom heartbeat rate to 2 Hz (every 500 ms)
    Lux.setHeartbeatInterval(500);

    // Set fast batch flush timeout to 5 ms
    Lux.setFlushTimeout(5);
}

void loop() {
    // 1. Always call tick() at the top of loop() to process pending heartbeats/flushes
    Lux.tick();

    // 2. Perform periodic application work every 2 seconds
    uint32_t now = millis();
    if (now - last_event_ms >= 2000) {
        last_event_ms = now;

        // Emit free RAM telemetry
        uint16_t free_ram = Lux.getFreeRam();
        Lux.trace(SYM_FREE_RAM_BYTES, free_ram);

        // Emit custom zero-payload event marker
        Lux.trace(SYM_CUSTOM_EVENT);
    }
}
