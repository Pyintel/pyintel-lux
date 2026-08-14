# Phase 6 — Dynamic Auto-Transport Selection & Fallback Engine

**Status:** Code Scaffolded & Ready to Test  
**Board count:** 2 (Board A = Emitter, Board B = Receiver/Relay)  
**Transports Evaluated:** ESP-NOW $\rightarrow$ Wi-Fi UDP Broadcast $\rightarrow$ UART Serial  

## Goal
Prove that Lux can dynamically select and switch between transports at runtime based on link availability without losing data or modifying framing code.

## Automatic Selection Priority
1. **Primary (ESP-NOW P2P Mesh):** Lowest latency, zero-infrastructure wireless P2P.
2. **Secondary (Wi-Fi UDP Broadcast):** High-bandwidth subnet broadcast over local Wi-Fi.
3. **Fallback (UART Serial Output):** Wired fail-safe output.

## How to Run

### 1. Flash Board A (Auto-Mesh Emitter @ COM9)
```powershell
cd research/esp32/phase6-auto-mesh/emitter
idf.py set-target esp32s3
idf.py build flash -p COM9
```

### 2. Flash Board B (Receiver Relay @ COM21)
```powershell
cd research/esp32/phase3-esp-now-mesh/receiver
idf.py set-target esp32
idf.py build flash -p COM21
```

### 3. Run Dynamic Host Decoder
```powershell
python research/esp32/phase1-uart-emit/host/decode.py --port COM21 --baud 115200 --duration 30 --csv lux_auto_telemetry.csv
```
