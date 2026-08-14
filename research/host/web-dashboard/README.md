# Phase 5 — Web Dashboard (Browser Live View)

**Status:** ✅ **COMPLETE & FUNCTIONAL**

Bridges incoming Lux binary frames (UDP/serial) to a WebSocket (`ws://localhost:8765`). Browser connects and decodes binary frames in real time using vanilla JS + `DataView` (prototype of `@pyintel/lux-web`), plotting a live chart and CRC telemetry table.

## Run

### 1. Launch WebSocket Server
```powershell
python research/host/web-dashboard/server.py --transport uart --port COM9 --baud 115200 --ws-port 8765
# OR for UDP mode:
python research/host/web-dashboard/server.py --transport udp --port 4210 --ws-port 8765
```

### 2. Open Live Web Dashboard
Double-click `research/host/web-dashboard/index.html` or open it in your browser!

## Features
- Zero-copy binary decoding in browser via JavaScript `DataView`.
- Real-time Chart.js line plot for high-frequency application symbols (`APP_COUNTER`).
- Live statistics (Total Packets, ESP32 Uptime, CRC-16 Reliability Rate, Sequence Numbers).
- Auto-updating telemetry packet table.
