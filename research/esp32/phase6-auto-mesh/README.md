# Phase 6 — Dynamic Adaptive Transport Mesh & Real-Time Dashboard

**Status:** ✅ **FULLY IMPLEMENTED & READY TO TEST**  
**Setup:**
- **Board A (Free-Range Emitter):** Battery or USB power, moves freely around the room.
- **Board B (Relay Station @ COM21):** Plugged directly into PC via USB-Serial.
- **PC Dashboard (Live Visualizer):** Real-time web visualizer showing active transport path & RSSI signal strength.

---

## 🎯 Proximity-Based Transport Switch Cascade

Board A continuously evaluates signal quality & link status:

1. **🔌 RX/TX UART Serial (Ultra-Close Range / Cable):** Emits over direct serial when physically wired.
2. **📡 ESP-NOW P2P Mesh (Medium Range, 0.5m–30m):** Direct 802.11 peer-to-peer wireless with **zero router dependency**.
3. **🌐 Wi-Fi UDP Broadcast (Far Range / Infrastructure):** Automatically switches to local Wi-Fi router network when distance increases or P2P drops.

---

## 🚀 How to Run the Real-Time Test

### Step 1: Flash Free-Range Board A (Emitter @ COM9)
```powershell
cd research/esp32/phase6-auto-mesh/emitter
idf.py set-target esp32s3
idf.py build flash -p COM9
```
*(Once flashed, you can unplug Board A and power it via a power bank or USB wall charger so it becomes free-range!)*

### Step 2: Flash Station Board B (Relay @ COM21)
```powershell
cd research/esp32/phase3-esp-now-mesh/receiver
idf.py set-target esp32
idf.py build flash -p COM21
```

### Step 3: Launch Multi-Channel Gateway Server
```powershell
python research/esp32/phase6-auto-mesh/server.py --port COM21 --baud 115200 --udp-port 4210 --ws-port 8765
```

### Step 4: Open Live Dashboard UI
Double-click `research/esp32/phase6-auto-mesh/index.html` or open it in your browser!

---

## 📊 Live Visual Features
- **Topology Banner**: Highlights the active transport path in real time (**🔌 UART**, **📡 ESP-NOW**, or **🌐 UDP**).
- **RSSI Signal Meter**: Real-time signal strength in dBm.
- **Dynamic Telemetry Chart**: Real-time graph updating continuously as Board A switches transports.
