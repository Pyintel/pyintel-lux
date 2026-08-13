# Phase 5 — Web Dashboard

**Status:** Not started (complete Phase 4 first)

Bridges incoming Lux frames (UDP/serial) to a WebSocket. Browser connects and renders a live chart using vanilla JS + `DataView` — a prototype of `@pyintel/lux-web`.

## Run
```bash
pip install pyserial websockets rich
python server.py --transport udp --port 4210 --ws-port 8765
# then open index.html in your browser
```

## Notes
<!-- Add your observations here -->
