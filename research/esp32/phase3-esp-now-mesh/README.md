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

### 3. Flash Board B (Receiver / Relay)
Connect Board B to a COM port (e.g. COM10) and flash `receiver`:
```powershell
cd research/esp32/phase3-esp-now-mesh/receiver
idf.py set-target esp32s3
idf.py build flash -p COM10
```

### 4. Run Host Decoder on Board B's COM Port
```powershell
python research/esp32/phase1-uart-emit/host/decode.py --port COM10 --baud 115200 --duration 30 --csv lux_espnow_telemetry.csv
```

---

## Success Criteria
- Board B relays Lux binary frames from Board A over ESP-NOW.
- Python host decoder prints decoded `HEARTBEAT` & `APP_COUNTER` frames with **100% CRC integrity** and **0% sequence loss**.
