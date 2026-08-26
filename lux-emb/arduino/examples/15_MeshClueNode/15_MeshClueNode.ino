/*
 * Pyintel Lux — Example 15: Adafruit CLUE (nRF52840) Mesh Node
 * 
 * Hardware:
 *   - Adafruit CLUE nRF52840 Express (64MHz ARM Cortex-M4F, Bluetooth LE)
 *   - Onboard Sensors & Peripherals:
 *       • Button A (Pin 5) & Button B (Pin 11)
 *       • Onboard Red LED (Pin 17 / LED_BUILTIN)
 *       • Onboard NeoPixel (Pin 18)
 *       • Hardware UART (Edge Pins P0 / P1 via Serial1)
 *       • USB Serial (Serial @ 115200 baud)
 * 
 * Behavior:
 *   - Joins the Lux Mesh network over Serial (USB CDC) or Serial1.
 *   - Broadcasts internal nRF52 temperature, Button A, and Button B events to the mesh.
 *   - Listens for remote LED commands (SYM_CLUE_LED_CMD) to toggle the onboard LED.
 *   - Handles interactive commands ('p' for Ping, 'l' for Node List) from the Web Dashboard.
 */

#include <Lux.h>

#define NETWORK_UUID "f47ac10b-58cc-4372-a567-0e02b2c3d479"

#define SYM_CLUE_TEMP       0x0330
#define SYM_CLUE_BUTTON_A   0x0333
#define SYM_CLUE_BUTTON_B   0x0334
#define SYM_CLUE_LED_CMD    0x0335

// Clue Pin Definitions
#ifndef PIN_BUTTON_A
#define PIN_BUTTON_A  5
#endif
#ifndef PIN_BUTTON_B
#define PIN_BUTTON_B  11
#endif
#ifndef PIN_LED1
#define PIN_LED1      17
#endif

uint32_t last_send = 0;
uint8_t last_btn_a = HIGH;
uint8_t last_btn_b = HIGH;

// Helper to read temperature safely
float readInternalTemperature() {
#if defined(NRF_TEMP)
    NRF_TEMP->TASKS_START = 1;
    while (NRF_TEMP->EVENTS_DATARDY == 0);
    NRF_TEMP->EVENTS_DATARDY = 0;
    int32_t raw_temp = NRF_TEMP->TEMP;
    NRF_TEMP->TASKS_STOP = 1;
    return (float)raw_temp * 0.25f;
#else
    return 24.5f;
#endif
}

void onLedCommand(uint16_t src_node, uint16_t symbol, const uint8_t *payload, uint8_t len) {
    if (len >= 1) {
        digitalWrite(PIN_LED1, payload[0] ? HIGH : LOW);
    }
}

void setup() {
    pinMode(PIN_LED1, OUTPUT);
    digitalWrite(PIN_LED1, LOW);

    pinMode(PIN_BUTTON_A, INPUT_PULLUP);
    pinMode(PIN_BUTTON_B, INPUT_PULLUP);

    // 1. USB Serial to PC for Web Dashboard / Monitor
    Serial.begin(115200);

    // 2. Join the Lux Mesh network via Serial
    Lux.beginMesh(NETWORK_UUID, Serial, 115200);

    // 3. Stream all live mesh traffic & telemetry to PC USB Serial
    Lux.debug(true, Serial);

    // 4. Register wireless command listener
    Lux.onMessage(SYM_CLUE_LED_CMD, onLedCommand);
}

void loop() {
    Lux.tick();

    // Read and detect Button A press (Active LOW)
    uint8_t btn_a = digitalRead(PIN_BUTTON_A);
    if (btn_a != last_btn_a) {
        last_btn_a = btn_a;
        if (btn_a == LOW) {
            // Button A Pressed -> Broadcast event
            Lux.broadcast(SYM_CLUE_BUTTON_A, (uint8_t)1);
            digitalWrite(PIN_LED1, HIGH);
        } else {
            digitalWrite(PIN_LED1, LOW);
        }
    }

    // Read and detect Button B press (Active LOW)
    uint8_t btn_b = digitalRead(PIN_BUTTON_B);
    if (btn_b != last_btn_b) {
        last_btn_b = btn_b;
        if (btn_b == LOW) {
            // Button B Pressed -> Broadcast event
            Lux.broadcast(SYM_CLUE_BUTTON_B, (uint8_t)1);
        }
    }

    // Handle interactive PC CLI / Dashboard commands
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'p' || c == 'P') {
            Lux.broadcast(0x0201, (uint32_t)millis()); // SYM_PING
        } else if (c == 'l' || c == 'L') {
            Lux.list(Serial);
        } else if (c == 'b' || c == 'B') {
            Lux.initBorder();
        } else if (c == 'u' || c == 'U') {
            Lux.unlockBorder();
        } else if (c == 'k' || c == 'K') {
            Lux.knock();
        }
    }

    // Periodic telemetry broadcast every 2 seconds
    if (millis() - last_send >= 2000) {
        last_send = millis();

        float temp_c = readInternalTemperature();
        Lux.broadcast(SYM_CLUE_TEMP, temp_c);
    }
}
