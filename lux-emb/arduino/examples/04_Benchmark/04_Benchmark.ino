/**
 * Pyintel Lux — Example 04: Microsecond Emission Benchmark
 * 
 * Measures the exact microsecond execution time and cycle overhead
 * of Lux frame assembly and CRC calculation across MCU architectures.
 */

#include <Lux.h>

#define SYM_BENCH_EMIT_TIME_US   0x0401
#define SYM_BENCH_BURST_TIME_US  0x0402
#define SYM_BENCH_FRAMES_SEC     0x0403
#define SYM_BENCH_DUMMY_DATA     0x0404

// Custom in-memory sink to isolate pure MCU computation overhead from serial baud limit
static size_t total_sink_bytes = 0;
lux_status_t benchmarkMemorySink(const uint8_t *buf, size_t len, void *ctx) {
    (void)buf;
    (void)ctx;
    total_sink_bytes += len;
    return LUX_OK;
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 1000);

    // Initialize with memory sink to measure pure MCU execution speed
    Lux.begin(benchmarkMemorySink);
    Lux.setHeartbeatInterval(0); // Disable heartbeat during benchmark
}

void loop() {
    // 1. Single-call microsecond timing test
    uint32_t t_start = micros();
    Lux.trace(SYM_BENCH_DUMMY_DATA, (uint32_t)123456);
    uint32_t t_single_us = micros() - t_start;

    // 2. Burst 50 frames test
    uint32_t t_burst_start = micros();
    const uint16_t BURST_COUNT = 50;
    for (uint16_t i = 0; i < BURST_COUNT; i++) {
        Lux.trace(SYM_BENCH_DUMMY_DATA, (uint32_t)i);
    }
    Lux.flush();
    uint32_t t_burst_us = micros() - t_burst_start;
    float frames_per_sec = (BURST_COUNT * 1000000.0f) / t_burst_us;

    // 3. Switch back to Serial temporarily to report metrics to host
    Lux.begin(Serial);
    Lux.trace(SYM_BENCH_EMIT_TIME_US, (uint32_t)t_single_us);
    Lux.trace(SYM_BENCH_BURST_TIME_US, (uint32_t)t_burst_us);
    Lux.trace(SYM_BENCH_FRAMES_SEC, frames_per_sec);
    Lux.flush();

    // Return to memory sink for next iteration
    Lux.begin(benchmarkMemorySink);

    delay(2000);
}
