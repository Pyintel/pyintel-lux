# Phase 6 — Direct Wired RX/TX Cable Link & Real-Time Dashboard

**Status:** ✅ **DEDICATED WIRED UART LINK**  
**Setup:**
- **Board A (Emitter @ COM9):** Wired direct RX/TX cable link to Board B / PC.
- **Board B (Relay Station / Serial Host @ COM21):** Plugged directly into PC via USB-Serial.
- **PC Dashboard (Live Visualizer):** Real-time web visualizer showing 🔌 RX/TX UART telemetry & frame transmission reliability.

---

## 🔌 Wired Proximity (0–0.5m) RX/TX UART Link

Phase 6 focuses on verifying direct wired serial communication:

1. **🔌 RX/TX UART Serial:** Transmits Lux telemetry frames over physical UART cables (RX to TX inverse connection).
2. **Reliability Verification:** Real-time CRC verification, telemetry sequence counting, and dashboard display over serial gateway.

> **Note:** For dynamic multi-transport switching (Wired UART <-> ESP-NOW P2P <-> Wi-Fi UDP), see **Phase 7 (`phase7-auto-choose`)**.

---

## 🚀 How to Run the Real-Time Test

### Step 1: Flash Board A (Emitter @ COM9)
```powershell
cd research/esp32/phase6-wired-uart/emitter
idf.py set-target esp32s3
idf.py build flash -p COM9
```

### Step 2: Launch Multi-Channel Gateway Server
```powershell
python research/esp32/phase6-wired-uart/server.py --port COM21 --baud 115200 --udp-port 4210 --ws-port 8765
```

### Step 3: Open Live Dashboard UI
Double-click `research/esp32/phase6-wired-uart/index.html` or open it in your browser!
