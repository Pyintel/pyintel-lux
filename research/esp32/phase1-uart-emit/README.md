# Phase 1 — UART Emit & Host Decode

**Status:** In progress  
**Board count:** 1 (Board A only)  
**Transport:** UART over USB-Serial  

## Goal
Emit Lux binary frames from ESP32 over UART. Decode them on the PC with Python.

## Files
- `main/` — ESP-IDF project for Board A
- `host/decode.py` — Python decoder that reads the COM port

## How to run

### Flash the ESP32
```bash
cd research/esp32/phase1-uart-emit
idf.py set-target esp32s3   # Set target according to connected chip (e.g. esp32s3)
idf.py menuconfig   # set: Component config → Lux Research → UART port & baud rate
idf.py build flash monitor -p COM9
```

### Run the host decoder & scientific CSV benchmark logger
```bash
# Listens for 10 seconds, logs CSV, then auto-runs stats.py
python host/decode.py --port COM9 --baud 115200 --duration 10 --csv lux_telemetry.csv

# Or run the stats analyzer independently on any existing CSV log:
python host/stats.py --csv lux_telemetry.csv
```
Replace `COM9` with your actual ESP32 COM port (check Device Manager).

## Scientific Benchmark CSV & Analyzer (`host/stats.py`)
`decode.py` listens for a specified duration (default: 10 seconds), streams packet data to `lux_telemetry.csv`, and automatically launches `host/stats.py` upon completion.

`host/stats.py` outputs key statistical benchmarks ready to be copied into your notes:
- **Total Frames Captured**
- **Inter-frame Delay (ms):** Min, Max, Mean ± StdDev
- **Throughput / FPS:** Min, Max, Mean ± StdDev
- **ESP32 Clock Delta (µs):** Min, Max, Mean ± StdDev
- **Data Integrity / CRC Pass Rate (%):** Checksum validity ratio

## Scientific Benchmark Data (Optimized Engine — 14-byte Header, Batching, CRC LUT & Sequence Loss Tracking)

### 📊 Benchmark Summary (`lux_telemetry.csv`)
- **Target Board:** ESP32-S3 (COM9 @ 115200 baud)
- **Total Frames Captured:** 121 frames (2,178 bytes)
- **Packet Delivery Rate (PDR):** **121/121 delivered (100.0% PDR, 0 lost)**
- **Data Integrity:** **121/121 CRC-16 CCITT frames passed (100.0% OK)**

#### Transmission & Timing Breakdown
- **Intra-burst Delay (`HEARTBEAT` → `APP_COUNTER`):** Min = 1.09 ms | Max = 17.02 ms | **Mean: 3.97 ± 2.93 ms**
- **Inter-burst Loop Sleep Delay (`APP_COUNTER` → `HEARTBEAT`):** Min = 400.11 ms | Max = 1030.84 ms | **Mean: 986.00 ± 76.76 ms**
- **ESP32 Microsecond Clock Delta (`esp_dt`):** Min = **4 µs** (Hardware emission speed) | Max = 999,996 µs | **Mean: 500,000.0 µs**

#### Metric Statistics Table
| Metric | Count | Min | Max | Mean ± StdDev |
| :--- | :---: | :---: | :---: | :---: |
| **Intra-burst Delay (HB → Counter)** | 60 | 1.09 ms | 17.02 ms | 3.97 ± 2.93 ms |
| **Inter-burst Loop Delay (Counter → HB)** | 60 | 400.11 ms | 1030.84 ms | 986.00 ± 76.76 ms |
| **Overall Inter-frame Delay** | 120 | 1.09 ms | 1030.84 ms | 494.98 ± 494.01 ms |
| **ESP32 Microsecond Clock Delta** | 120 | 4 µs | 999,996 µs | 500,000.0 ± 499,996.0 µs |

## Notes
- **Observation (COM Port Scan):** ESP32 detected on **COM9** (`Silicon Labs CP210x USB to UART Bridge`, VID:PID `10C4:EA60`).
- **Target Chip Mismatch & Fix:** The build failed during flashing because `sdkconfig` was configured for `esp32`, but `esptool` detected an **ESP32-S3** board connected on COM9 (`fatal error: This chip is ESP32-S3, not ESP32`). Set target using `idf.py set-target esp32s3` before building and flashing.
- **ESP-IDF Monitor Garbage Output Explained:** `idf.py monitor` outputs garbage characters like `I LX=LXs...` because `app_main` emits raw **Lux binary frames** directly to `UART_NUM_0` (which is shared with stdout/console UART on GPIO 43/44 of the S3). The monitor attempts to render binary frame bytes (magic bytes `0x4C 0x55 0x58` = `"LUX"`, binary timestamps, checksums, symbol IDs) as ASCII log text. Run `python host/decode.py --port COM9 --baud 115200` to decode the actual binary stream.
- **Ninja Lock Note:** If ninja raises `WriteFile(.ninja_lock): Unable to create file. Permission denied`, clear leftover build locks or run `idf.py fullclean`.
- **User Comment:** Previously flashed with PlatformIO build, needs to be reflashed with ESP-IDF Phase 1 binary.
- **Fix (Build path):** Fixed relative path in `main/CMakeLists.txt` to `${CMAKE_SOURCE_DIR}/../../../lux-core/include` and added `lux.c` to `SRCS`.
