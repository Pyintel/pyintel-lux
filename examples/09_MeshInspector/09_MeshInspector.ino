/**
 * Pyintel Lux — Example 09: Mesh Inspector & Sniffer Node
 * 
 * Turns any microcontroller into a live mesh monitoring station.
 * - Promiscuously decodes and logs all mesh traffic across all radios
 * - Responds to serial keystrokes:
 *     'l' -> Print live ASCII mesh device table (Lux.list())
 *     'b' -> Broadcast test alert
 *     'h' -> Print help menu
 */

#include <Lux.h>

const char *MESH_NETWORK_UUID = "f47ac10b-58cc-4372-a567-0e02b2c3d479";

#define SYM_ALERT_TRIGGER   0x0401

void printMenu() {
    Serial.println();
    Serial.println(F("── Lux Mesh Inspector Commands ──"));
    Serial.println(F("  'l' : List all discovered mesh devices & status"));
    Serial.println(F("  'b' : Broadcast test alert message"));
    Serial.println(F("  'h' : Print this command menu"));
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    // 1. Join Mesh Network (optionally provide Wi-Fi credentials)
    Lux.beginMesh(MESH_NETWORK_UUID);

    // 2. Enable Promiscuous Debug Monitor Mode
    Lux.debug(true);

    printMenu();
}

void loop() {
    // Background mesh engine handles all RX/TX, routing, and debug logging
    Lux.tick();

    // Check for interactive user commands from Serial Monitor
    if (Serial.available() > 0) {
        char cmd = (char)Serial.read();

        if (cmd == 'l' || cmd == 'L') {
            Lux.list(Serial);
        } else if (cmd == 'b' || cmd == 'B') {
            Serial.println(F("[INSPECTOR] Broadcasting Test Alert (0x0401)..."));
            Lux.broadcast(SYM_ALERT_TRIGGER, (uint32_t)1);
        } else if (cmd == 'h' || cmd == 'H') {
            printMenu();
        }
    }
}
