# Phase 7 — Dynamic Auto-Choose Multi-Transport Mesh & Live Dashboard

**Status:** ✅ **DYNAMIC AUTO-CHOOSE TRANSPORT MESH**  
**Setup:**
- **Board A (Free-Range Emitter @ COM9):** Evaluates transport dynamically (Wired UART <-> ESP-NOW P2P <-> Wi-Fi UDP).
- **Board B (Relay Station @ COM21):** Direct USB-Serial connection to PC gateway.
- **PC Dashboard (Live Visualizer):** Real-time web visualizer showing active path switching & RSSI signal strength.

---

## 🎯 Proximity-Based Auto-Choose Transport Switch Cascade

Board A continuously evaluates signal quality, cable status & link health:

1. **🔌 RX/TX UART Serial (Ultra-Close Range / Cable):** Emits over direct serial when physically wired.
2. **📡 ESP-NOW P2P Mesh (Medium Range, 0.5m–30m):** Direct 802.11 peer-to-peer wireless with **zero router dependency**.
3. **🌐 Wi-Fi UDP Broadcast (Far Range / Infrastructure):** Automatically switches to local Wi-Fi router network when distance increases or P2P drops.

---

## 🚀 How to Run the Real-Time Test

### Step 1: Flash Free-Range Board A (Emitter @ COM9)
```powershell
cd research/esp32/phase7-auto-choose/emitter
idf.py set-target esp32s3
idf.py build flash -p COM9
```

### Step 2: Launch Multi-Channel Gateway Server
```powershell
python research/esp32/phase7-auto-choose/server.py --port COM21 --baud 115200 --udp-port 4210 --ws-port 8765
```

### Step 3: Open Live Dashboard UI
Double-click `research/esp32/phase7-auto-choose/index.html` or open it in your browser!
