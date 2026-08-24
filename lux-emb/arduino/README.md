# Pyintel Lux — Official Arduino Library

**Ultra-lightweight binary telemetry and autonomous multi-transport mesh networking engine for microcontrollers.**

---

## ⚡ Key Capabilities

- **Autonomous Multi-Transport Mesh:** One line (`Lux.beginMesh(uuid)`) activates all available radios (ESP-NOW, Wi-Fi UDP, Serial, BLE) and forms a self-healing mesh with peer discovery.
- **Microsecond Binary Telemetry:** 14-byte compact wire frames with CRC-16 integrity and zero runtime heap allocation (`malloc`).
- **Live Mesh Inspector (`Lux.debug(true)`):** Plug any single board into USB to turn your Serial Monitor into a promiscuous packet sniffer and live health monitor for the entire mesh.
- **Mesh Visibility (`Lux.list()`):** Print a formatted ASCII status table showing all online nodes, active radios, RSSI, and uptime.
- **Cross-Platform:** Runs on AVR (Uno/Nano/Mega), SAMD21, RP2040/RP2350, ESP32 (S3/C3/C6), ESP8266, STM32.

---

## 🚀 Quickstart: 2-Node Mesh in 30 Seconds

### Node A (Ping Sender) / Node B (Pong Responder)

Both boards run the exact same code and talk to each other over ESP-NOW, Wi-Fi, or Serial with **zero router setup**:

```cpp
#include <Lux.h>

const char *NETWORK_UUID = "f47ac10b-58cc-4372-a567-0e02b2c3d479";

#define SYM_PING 0x0201
#define SYM_PONG 0x0202

void setup() {
    Serial.begin(115200);

    // Join mesh across all available radios
    Lux.beginMesh(NETWORK_UUID);

    // Register callback for incoming pings
    Lux.onMessage(SYM_PING, [](uint16_t from_node, uint16_t sym, const uint8_t *data, uint8_t len) {
        Serial.print("Ping from 0x");
        Serial.println(from_node, HEX);
        // Reply with pong
        Lux.send(from_node, SYM_PONG, (uint32_t)1);
    });
}

void loop() {
    Lux.tick(); // Ingests frames across all active radios

    // Broadcast ping every 3 seconds
    static uint32_t last_tx = 0;
    if (millis() - last_tx >= 3000) {
        Lux.broadcast(SYM_PING, (uint32_t)42);
        last_tx = millis();
    }
}
```

---

## 🔍 Live Mesh Inspector & Debug Sniffer

Plug any board into USB and enable monitor mode:

```cpp
#include <Lux.h>

void setup() {
    Serial.begin(115200);
    Lux.beginMesh("f47ac10b-58cc-4372-a567-0e02b2c3d479");

    // Enable live sniffer on Serial Monitor
    Lux.debug(true);
}

void loop() {
    Lux.tick();

    // Type 'l' in Serial Monitor to print the device table
    if (Serial.read() == 'l') {
        Lux.list(Serial);
    }
}
```

---

## 📚 Library Examples

| Example | Description |
|---|---|
| `01_BasicTrace` | Fast microsecond telemetry streaming over Serial |
| `02_MultiSensor` | Batched multi-sensor trace payloads |
| `03_HeartbeatAndTick` | Non-blocking tick engine and free SRAM tracking |
| `04_Benchmark` | Microsecond emission latency benchmarks |
| `05_HardwareTimer` | 100 Hz hardware Timer1 ISR telemetry on AVR |
| `06_CoexistenceTest` | Concurrent execution with `Wire` (I2C) and `SPI` |
| `07_MeshPingPong` | Multi-board P2P messaging and RTT measurement |
| `08_MeshBroadcast` | Distributed sensor telemetry broadcasting |
| `09_MeshInspector` | Live promiscuous sniffer with interactive `Lux.list()` |
