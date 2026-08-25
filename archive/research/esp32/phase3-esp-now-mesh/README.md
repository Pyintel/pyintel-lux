# Phase 3 — ESP-NOW P2P Mesh (2 ESP32s, No Router)

**Status:** Code Scaffolded & Ready for 2-Board Test  
**Board count:** 2 (Board A = Emitter, Board B = Receiver/Relay)  
**Transport:** ESP-NOW (connectionless 802.11, offline direct link)  

## Goal
Board A emits Lux binary frames directly to Board B over **ESP-NOW**. Board B receives the frames wirelessly and forwards them out its USB-Serial UART port to the host PC. 

The Python host decoder (`decode.py`) from Phase 1 reads Board B's COM port completely unchanged — proving Lux transport transparency.

---

## Architecture Overview

```text
[Board A: Emitter] ──(Wireless ESP-NOW @ 802.11)──> [Board B: Relay] ──(UART USB-Serial)──> [Host PC: decode.py]
```

---

## How to Run

### 1. (Optional) Read Hardware MAC Addresses
If you want unicast pairing instead of broadcast, flash `tools/mac_scanner`:
```powershell
cd research/esp32/phase3-esp-now-mesh/tools/mac_scanner
idf.py set-target esp32s3
idf.py build flash monitor -p COM9
```

### 2. Flash Board A (Emitter)
Connect Board A to COM9 and flash `emitter`:
```powershell
cd research/esp32/phase3-esp-now-mesh/emitter
idf.py set-target esp32s3
idf.py build flash -p COM9
```

### 3. Flash Board B (Receiver / Relay — Standard ESP32 DevKitV1 @ COM21)
Connect Board B to COM21 and flash `receiver`:
```powershell
cd research/esp32/phase3-esp-now-mesh/receiver
idf.py set-target esp32
idf.py build flash -p COM21
```

### 4. Run Host Decoder on Board B's COM Port
```powershell
python research/esp32/phase1-uart-emit/host/decode.py --port COM21 --baud 115200 --duration 30 --csv lux_espnow_telemetry.csv
```

---

## Scientific Benchmark Data (30-second ESP-NOW Wireless Mesh Capture)

### 📊 Benchmark Summary (`lux_espnow_telemetry.csv`)
- **Topology:** Board A (ESP32-S3 Emitter) ──(ESP-NOW 802.11)──> Board B (ESP32 DevKitV1 Relay) ──(UART @ 115200)──> Host PC
- **Total Frames Captured:** 59 frames (1,062 bytes over 30s)
- **Packet Delivery Rate (PDR):** **59/59 delivered (100.0% PDR, 0 lost)**
- **Data Integrity:** **59/59 CRC-16 CCITT frames passed (100.0% OK)**

#### Transmission Breakdown (P2P ESP-NOW Mesh)
- **Intra-burst Transmission Delay (`HEARTBEAT` → `APP_COUNTER`):** Min = 2.16 ms | Max = 18.95 ms | **Mean: 4.06 ± 2.96 ms**
- **Inter-burst Loop Sleep Delay (`APP_COUNTER` → `HEARTBEAT`):** Min = 115.84 ms | Max = 1006.58 ms | **Mean: 965.59 ± 160.71 ms**
- **ESP32 Microsecond Clock Delta (`esp_dt`):** Min = **10 µs** | Max = 999,996 µs | **Mean: 500,000.0 ± 499,990.0 µs**

#### Metric Statistics Table
| Metric | Count | Min | Max | Mean ± StdDev |
| :--- | :---: | :---: | :---: | :---: |
| **Intra-burst Delay (HB → Counter)** | 29 | 2.16 ms | 18.95 ms | 4.06 ± 2.96 ms |
| **Inter-burst Loop Delay (Counter → HB)** | 29 | 115.84 ms | 1006.58 ms | 965.59 ± 160.71 ms |
| **Overall Inter-frame Delay** | 58 | 2.16 ms | 1006.58 ms | 484.82 ± 494.02 ms |
| **ESP32 Microsecond Clock Delta** | 58 | 10 µs | 999,996 µs | 500,000.0 ± 499,990.0 µs |
