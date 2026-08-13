# Phase 1 — UART Emit & Host Decode

**Status:** Not started  
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
idf.py set-target esp32
idf.py menuconfig   # set: Component config → Lux Research → UART port & baud rate
idf.py build flash monitor
```

### Run the host decoder
```bash
python host/decode.py --port COM3 --baud 115200
```
Replace `COM3` with your actual ESP32 COM port (check Device Manager).

## Success criteria
- Host terminal prints decoded `HEARTBEAT` frames at ~1 Hz
- Symbol ID = `0x0001`, timestamp increases monotonically, payload = uptime ms

## Notes
<!-- Add your observations here as you run experiments -->
