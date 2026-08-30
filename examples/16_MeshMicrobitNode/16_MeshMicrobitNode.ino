/*
 * Pyintel Lux — Example 16: BBC micro:bit (v1.5 / v2) Pure UART Mesh Node
 * 
 * Hardware:
 *   - BBC micro:bit v1.5 (Nordic nRF51822 @ 16MHz) or micro:bit v2 (nRF52833)
 *   - 5x5 LED Matrix Display
 *   - 2 Push Buttons (Button A: Pin 5, Button B: Pin 11)
 *   - Onboard/Internal Temperature Sensor
 *   - Edge Connector Pins (P0: Analog/Digital, P1, P2)
 *   - USB Serial Interface (DAPLink @ 115200 baud)
 * 
 * Behavior:
 *   - Joins the Lux Mesh network over Serial (DAPLink USB CDC @ 115200 baud).
 *   - Broadcasts internal nRF temperature, Button A & B states, and Pin 0 analog sensor data.
 *   - Listens for remote LED commands (SYM_UBIT_LED_CMD) to toggle the 5x5 LED matrix center LED.
 *   - Ultra-compact memory footprint: Fits comfortably inside the 16KB RAM constraint of micro:bit v1!
 */

#include <Lux.h>

#define NETWORK_UUID "f47ac10b-58cc-4372-a567-0e02b2c3d479"

#define SYM_UBIT_TEMP       0x0340
#define SYM_UBIT_BUTTON_A   0x0341
#define SYM_UBIT_BUTTON_B   0x0342
#define SYM_UBIT_ANALOG_P0  0x0343
#define SYM_UBIT_LED_CMD    0x0344

// Pin definitions for BBC micro:bit
#ifndef PIN_BUTTON_A
#define PIN_BUTTON_A  5
#endif
#ifndef PIN_BUTTON_B
#define PIN_BUTTON_B  11
#endif
#ifndef PIN_P0
#define PIN_P0        0
#endif

// Micro:bit v1 5x5 Matrix Pin Mapping (Center LED: Row 1, Col 1 or Row 2, Col 2)
#ifndef LED_ROW1
#define LED_ROW1      26
#endif
#ifndef LED_COL1
#define LED_COL1      3
#endif

uint32_t last_send = 0;
uint8_t last_btn_a = HIGH;
uint8_t last_btn_b = HIGH;

// Direct hardware register access to Nordic nRF on-die temperature sensor (Works on nRF51 and nRF52)
float readMicrobitTemperature() {
#if defined(NRF_TEMP)
    NRF_TEMP->TASKS_START = 1;
    while (NRF_TEMP->EVENTS_DATARDY == 0);
    NRF_TEMP->EVENTS_DATARDY = 0;
    int32_t raw = NRF_TEMP->TEMP; // in units of 0.25 C
    NRF_TEMP->TASKS_STOP = 1;
    return (float)raw * 0.25f;
#else
    return 22.0f;
#endif
}

void onLedCommand(uint16_t src_node, uint16_t symbol, const uint8_t *payload, uint8_t len) {
    if (len >= 1) {
        bool turn_on = payload[0] > 0;
#if defined(LED_ROW1) && defined(LED_COL1)
        digitalWrite(LED_ROW1, turn_on ? HIGH : LOW);
        digitalWrite(LED_COL1, turn_on ? LOW : HIGH); // Columns are active LOW on micro:bit
#endif
#if defined(LED_BUILTIN)
        digitalWrite(LED_BUILTIN, turn_on ? HIGH : LOW);
#endif
    }
}

void setup() {
    // Configure buttons (micro:bit buttons are active LOW)
    pinMode(PIN_BUTTON_A, INPUT_PULLUP);
    pinMode(PIN_BUTTON_B, INPUT_PULLUP);

#if defined(LED_ROW1) && defined(LED_COL1)
    pinMode(LED_ROW1, OUTPUT);
    pinMode(LED_COL1, OUTPUT);
    digitalWrite(LED_ROW1, LOW);
    digitalWrite(LED_COL1, HIGH);
#endif

    // 1. USB Serial via DAPLink interface @ 115200 baud
    Serial.begin(115200);

    // 2. Join the Lux Mesh network via Serial
    Lux.beginMesh(NETWORK_UUID, Serial, 115200);

    // 3. Stream all live mesh telemetry & events to PC USB Serial
    Lux.debug(true, Serial);

    // 4. Register wireless command listener
    Lux.onMessage(SYM_UBIT_LED_CMD, onLedCommand);
}

void loop() {
    Lux.tick();

    // 1. Read Button A state change
    uint8_t btn_a = digitalRead(PIN_BUTTON_A);
    if (btn_a != last_btn_a) {
        last_btn_a = btn_a;
        if (btn_a == LOW) {
            Lux.broadcast(SYM_UBIT_BUTTON_A, (uint8_t)1);
#if defined(LED_ROW1) && defined(LED_COL1)
            digitalWrite(LED_ROW1, HIGH);
            digitalWrite(LED_COL1, LOW);
#endif
        } else {
#if defined(LED_ROW1) && defined(LED_COL1)
            digitalWrite(LED_ROW1, LOW);
            digitalWrite(LED_COL1, HIGH);
#endif
        }
    }

    // 2. Read Button B state change
    uint8_t btn_b = digitalRead(PIN_BUTTON_B);
    if (btn_b != last_btn_b) {
        last_btn_b = btn_b;
        if (btn_b == LOW) {
            Lux.broadcast(SYM_UBIT_BUTTON_B, (uint8_t)1);
        }
    }

    // 3. Handle interactive commands from PC Web Dashboard
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

    // 4. Broadcast sensor telemetry to the entire mesh every 2 seconds
    if (millis() - last_send >= 2000) {
        last_send = millis();

        // Read internal die temperature
        float temp_c = readMicrobitTemperature();
        Lux.broadcast(SYM_UBIT_TEMP, temp_c);

        // Read analog voltage from edge pin P0
        uint16_t p0_val = analogRead(PIN_P0);
        Lux.broadcast(SYM_UBIT_ANALOG_P0, p0_val);
    }
}
