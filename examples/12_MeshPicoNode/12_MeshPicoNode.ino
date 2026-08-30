/*
 * Pyintel Lux — Example 12: Raspberry Pi Pico (RP2040) Pure UART Mesh Node
 * 
 * Hardware Wiring (3 Wires directly, NO RESISTORS NEEDED!):
 *   - Pico GP0 (UART0 TX)  <--->  ESP32 Pin 16 (RX2)  [Direct 3.3V connection]
 *   - Pico GP1 (UART0 RX)  <--->  ESP32 Pin 17 (TX2)  [Direct 3.3V connection]
 *   - Pico GND             <--->  ESP32 GND
 * 
 * Behavior:
 *   - Both Pico and ESP32 run at native 3.3V logic levels.
 *   - Joins the Lux Mesh network over Serial1 (Pins GP0/GP1 @ 115200 baud).
 *   - Reads the RP2040 onboard temperature sensor and broadcasts to the wireless mesh!
 *   - Listens for remote LED commands to toggle the Pico's onboard LED.
 */

#include <Lux.h>

#define NETWORK_UUID "f47ac10b-58cc-4372-a567-0e02b2c3d479"

#define SYM_PICO_TEMP     0x0320
#define SYM_PICO_LED_CMD  0x0321

uint32_t last_send = 0;

void onLedCommand(uint16_t src_node, uint16_t symbol, const uint8_t *payload, uint8_t len) {
    if (len >= 1) {
        digitalWrite(LED_BUILTIN, payload[0] ? HIGH : LOW);
    }
}

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    // 1. USB Serial to PC for Web Dashboard / Debugging
    Serial.begin(115200);

    // 2. Hardware UART0 to ESP32 Bridge (GP0 TX, GP1 RX)
    Serial1.begin(115200);

    // 3. Join the Lux Mesh network via Serial1
    Lux.beginMesh(NETWORK_UUID, Serial1, 115200);

    // 4. Stream all live mesh traffic to PC USB Serial
    Lux.debug(true, Serial);

    // 5. Register wireless command listener
    Lux.onMessage(SYM_PICO_LED_CMD, onLedCommand);
}

void loop() {
    Lux.tick();

    // Handle interactive commands from PC Web Dashboard
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'p' || c == 'P') {
            Lux.broadcast(0x0201, (uint32_t)millis()); // SYM_PING
        } else if (c == 'l' || c == 'L') {
            Lux.list(Serial);
        }
    }

    // Broadcast RP2040 onboard temperature every 2 seconds
    if (millis() - last_send >= 2000) {
        last_send = millis();

        // Read onboard temperature in Celsius
        float temp_c = analogReadTemp();

        // Broadcast float telemetry to the entire wireless mesh!
        Lux.broadcast(SYM_PICO_TEMP, temp_c);
    }
}
