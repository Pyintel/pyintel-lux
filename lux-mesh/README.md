# Pyintel Lux — Universal Multi-Bearer Mesh Engine (`lux-mesh`)

> Zero-copy, sub-kilobyte binary mesh routing and dynamic transport fallback across all physical and wireless communication media.

---

## 📡 Supported Communication Bearers

| Bearer | Transport ID | Layer / Physical Hardware | Primary Targets | Typical Latency | Range |
|---|---|---|---|---|---|
| **UART Serial** | `LUX_BEARER_SERIAL` (`0`) | USB-CDC / Hardware UART (`Serial1`) | ATmega328P, SAMD21, PC Gateway | `< 0.01 ms` | Wired (`< 5m`) |
| **Nordic 2.4GHz RF** | `LUX_BEARER_NRF_RADIO` (`1`) | Bare-metal `NRF_RADIO` (2407 MHz) | BBC micro:bit v1/v2, Adafruit CLUE | `< 0.5 ms` | `30m – 50m` |
| **Bluetooth Low Energy** | `LUX_BEARER_BLE` (`2`) | BLE Advertisements / NUS / GATT | Adafruit CLUE, ESP32, Smartphones | `20 ms – 100 ms` | `10m – 30m` |
| **ESP-NOW** | `LUX_BEARER_ESPNOW` (`3`) | Connectionless 802.11 Wi-Fi MAC | ESP32, ESP32-S3, ESP32-C3, ESP8266 | `1.5 ms – 4.0 ms` | `100m – 200m` |
| **Wi-Fi UDP** | `LUX_BEARER_WIFI_UDP` (`4`) | Socket Broadcast / Multicast (`:8888`) | ESP32, Raspberry Pi Pico W, Host | `2 ms – 15 ms` | LAN / Wi-Fi |
| **LoRa Sub-GHz** | `LUX_BEARER_LORA` (`5`) | Semtech SX1276 / SX1262 SPI | Long-range outdoor telemetry | `50 ms – 300 ms` | `2 km – 15 km` |
| **CAN Bus** | `LUX_BEARER_CAN` (`6`) | MCP2515 / ESP32 TWAI (Differential) | Industrial, Automotive & Robotics | `< 0.2 ms` | `40m – 1 km` |

---

## 🏗️ Architecture & Packet Format

### 12-Byte Mesh Wire Envelope (`lux_mesh_envelope_t`)
```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|       Sync ('M', 'X')         |           Network UUID Hash   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|        Network UUID Hash      |    Source Node ID             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   Destination Node ID         | Hop Count     | Flags         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### 14-Byte Inner Lux Telemetry Frame (`lux_frame_header_t`)
```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|       Sync ('L', 'X')         |    Sequence Number            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|    Symbol ID (e.g. 0x0330)    |    Microsecond Timestamp      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|    Microsecond Timestamp      | Payload Type  | Payload Len   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|    Header CRC-16 (CCITT)      | Payload Bytes (0..128B) ...   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

---

## 🚀 Key Features

1. **Transparent Transport Fallback**: If an ESP-NOW peer goes out of range, the mesh automatically routes over BLE or Wi-Fi UDP without application-level re-coding.
2. **Multi-Hop Flood Routing**: Intermediate nodes decrement `hop_count` and rebroadcast across alternate physical interfaces (e.g., received over 2.4 GHz RF $\rightarrow$ forwarded over USB Serial).
3. **Hardware-Accelerated Deduplication**: A 32-entry circular hash cache discards duplicate multi-path packets within 5 ms.
4. **Zero-Heap Bare-Metal Design**: Operates entirely in static buffers without dynamic memory allocations (`malloc`/`free`), making it safe for 8-bit AVR microcontrollers.
