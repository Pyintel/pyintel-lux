/**
 * Pyintel Lux — Example 05: Hardware Timer Interrupt Telemetry
 * 
 * Demonstrates hardware timer interrupt driven emission (AVR Timer1),
 * proving that Lux emit operations are ISR-safe and deterministic.
 */

#include <Lux.h>

#define SYM_ISR_TICK_COUNT    0x0501
#define SYM_MAIN_LOOP_COUNT   0x0502

volatile uint32_t isr_tick_counter = 0;
uint32_t main_loop_counter = 0;

void setup() {
    Serial.begin(115200);
    Lux.begin(Serial);

#if defined(__AVR__) || defined(ARDUINO_ARCH_AVR)
    // Enable 100 Hz hardware Timer1 interrupt on AVR
    Lux.enableTimer1Interrupt(100);
#endif
}

void loop() {
    Lux.tick();

    // Trace main loop progress
    Lux.trace(SYM_MAIN_LOOP_COUNT, main_loop_counter++);

    delay(100);
}
