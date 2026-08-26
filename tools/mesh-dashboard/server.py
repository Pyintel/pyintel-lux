#!/usr/bin/env python3
"""
Pyintel Lux — Live Mesh WebSocket Bridge & Dashboard Server
Streams live multi-node telemetry and message feeds from COM9 to browser UI.
"""

import sys
import time
import json
import asyncio
import argparse
import threading
import http.server
import functools
import pathlib
import serial
import serial.tools.list_ports
import websockets

SYMBOL_NAMES = {
    0x0001: "LUX_SYM_HEARTBEAT",
    0x0002: "LUX_SYM_RESET",
    0x0003: "LUX_SYM_OVERFLOW",
    0x0004: "LUX_SYM_TRANSPORT_SWITCH",
    0x0005: "LUX_SYM_DEVICE_INFO",
    0x0006: "LUX_SYM_PIN_REPORT",
    0x0007: "LUX_SYM_BORDER_LOCK",
    0x0008: "LUX_SYM_BORDER_UNLOCK",
    0x0009: "LUX_SYM_BORDER_KNOCK",
    0x0201: "SYM_PING",
    0x0202: "SYM_PONG",
    0x0301: "SYM_SENSOR_TEMP",
    0x0302: "SYM_SENSOR_VOLT",
    0x0310: "SYM_UNO_POT_VAL",
    0x0320: "SYM_PICO_TEMP",
    0x0321: "SYM_PICO_LED_CMD",
    0x0330: "SYM_CLUE_TEMP",
    0x0331: "SYM_CLUE_BUTTON_A",
    0x0332: "SYM_CLUE_BUTTON_B",
    0x0333: "SYM_CLUE_LED_CMD",
    0x0340: "SYM_UBIT_TEMP",
    0x0341: "SYM_UBIT_BUTTON_A",
    0x0342: "SYM_UBIT_BUTTON_B",
    0x0343: "SYM_UBIT_ANALOG_P0",
    0x0344: "SYM_UBIT_LED_CMD",
    0x0350: "SYM_ESP32_TEMP",
    0x0351: "SYM_ESP32_HALL",
    0x0352: "SYM_ESP32_HEAP",
    0x0353: "SYM_ESP32_LED_CMD",
    0x0360: "SYM_ESP32S3_TEMP",
    0x0361: "SYM_ESP32S3_HEAP",
    0x0362: "SYM_ESP32S3_LED_CMD",
    0x0401: "SYM_ALERT_TRIGGER",
}

class MeshBridge:
    def __init__(self, target_port="auto", baud=115200, ws_port=8765):
        self.target_port = target_port
        self.baud = baud
        self.ws_port = ws_port
        self.clients = set()
        self.active_serials = {}  # port -> Serial instance
        self.active_threads = {}  # port -> Thread instance
        self.running = True
        self.local_node_id = None

        self.peers = {}
        self.total_msgs = 0
        self.msg_rate = 0.0
        self.rate_count = 0
        self.last_rate_calc = time.time()

    def start_serial_thread(self, loop):
        threading.Thread(target=self.port_scanner_worker, args=(loop,), daemon=True).start()
        threading.Thread(target=self.prune_worker, args=(loop,), daemon=True).start()

    def port_scanner_worker(self, loop):
        """Continuously discover and connect to all active USB serial boards (multi-node ingest)"""
        while self.running:
            try:
                all_ports = serial.tools.list_ports.comports()
                valid_ports = [
                    p.device for p in all_ports 
                    if "BTHENUM" not in p.hwid and "BTH" not in p.hwid and p.device != "COM12"
                ]

                if self.target_port != "auto":
                    targets = [self.target_port] if self.target_port in valid_ports else []
                else:
                    targets = valid_ports

                for port in targets:
                    if port not in self.active_threads or not self.active_threads[port].is_alive():
                        t = threading.Thread(target=self.single_port_worker, args=(port, loop), daemon=True)
                        self.active_threads[port] = t
                        t.start()
            except Exception as e:
                pass
            time.sleep(1.5)

    def prune_worker(self, loop):
        """Periodically prune nodes that haven't sent a packet in > 3.5 seconds"""
        while self.running:
            time.sleep(0.5)
            now = time.time()
            changed = False
            for p in self.peers.values():
                if p["node_id"] != self.local_node_id:
                    if now - p["last_seen"] > 3.5:
                        if p["alive"]:
                            p["alive"] = False
                            changed = True

            if changed and self.clients:
                payload = {
                    "type": "update",
                    "node_count": len([p for p in self.peers.values() if p["alive"]]),
                    "total_nodes": len(self.peers),
                    "msg_rate": round(self.msg_rate, 1),
                    "total_msgs": self.total_msgs,
                    "peers": list(self.peers.values()),
                    "msg": None
                }
                asyncio.run_coroutine_threadsafe(self.broadcast(json.dumps(payload)), loop)

    def single_port_worker(self, port, loop):
        ser = None
        try:
            print(f"Connecting to Serial on {port} @ {self.baud} baud...", flush=True)
            ser = serial.Serial(port, self.baud, timeout=0.1)
            ser.dtr = True
            ser.rts = True
            self.active_serials[port] = ser
            print(f"✅ Serial connected on {port}!", flush=True)

            line_buf = ""
            while self.running:
                raw = ser.read(ser.in_waiting or 1)
                if raw:
                    text = raw.decode("utf-8", errors="replace")
                    line_buf += text
                    while "\n" in line_buf:
                        line, line_buf = line_buf.split("\n", 1)
                        line = line.strip("\r")
                        if line:
                            self.process_line(line, loop, port=port)
        except Exception as e:
            print(f"⚠️ Serial connection closed on {port}: {e}", flush=True)
        finally:
            if port in self.active_serials:
                try:
                    self.active_serials[port].close()
                except:
                    pass
                del self.active_serials[port]

    def write_all(self, data: bytes):
        for port, ser in list(self.active_serials.items()):
            try:
                ser.write(data)
            except Exception:
                pass

    def process_line(self, line, loop, port=""):
        now = time.time()
        self.total_msgs += 1
        self.rate_count += 1

        if now - self.last_rate_calc >= 1.0:
            self.msg_rate = self.rate_count / (now - self.last_rate_calc)
            self.rate_count = 0
            self.last_rate_calc = now

        msg_data = None

        # Parse banner: "This Node: 0xB89A" or "My Node ID: 0xB89A"
        if "This Node:" in line or "My Node ID:" in line:
            parts = line.split()
            for i, part in enumerate(parts):
                if part.startswith("0x"):
                    self.local_node_id = part.strip("|")

        # Parse debug frame lines
        if "RX src=" in line or "TX src=" in line:
            is_rx = "RX src=" in line
            parts = line.split()
            parsed = {}
            for p in parts:
                if "=" in p:
                    k, v = p.split("=", 1)
                    parsed[k] = v

            src = parsed.get("src", "0x0000")
            dst = parsed.get("dst", "BCAST")
            sym_raw = parsed.get("sym", "0x0")
            val_raw = parsed.get("val", "0")
            transport = parsed.get("via", "ESP-NOW")
            hop = parsed.get("hop", "4")
            crc = parsed.get("crc", "OK")

            try:
                sym_id = int(sym_raw, 16) if sym_raw.startswith("0x") else int(sym_raw)
            except:
                sym_id = 0

            sym_name = SYMBOL_NAMES.get(sym_id, f"SYM_0x{sym_id:04X}")

            # Update peer
            if src != "0x0000":
                if not is_rx:
                    self.local_node_id = src

                if src not in self.peers:
                    self.peers[src] = {
                        "node_id": src,
                        "transport": "Serial+NOW+WiFi (ME)" if src == self.local_node_id else transport,
                        "rssi": "0" if src == self.local_node_id else "-45",
                        "uptime": 0,
                        "last_seen": time.time(),
                        "rx_count": 0,
                        "tx_count": 0,
                        "alive": True
                    }
                p = self.peers[src]
                p["last_seen"] = time.time()
                p["alive"] = True
                if is_rx:
                    p["rx_count"] += 1
                else:
                    p["tx_count"] += 1

                if sym_id == 0x0001:
                    try:
                        p["uptime"] = int(val_raw)
                    except:
                        pass

            val_clean = "".join(c for c in val_raw if c.isprintable()).strip()
            if not val_clean:
                val_clean = val_raw

            if sym_id in (0x0320, 0x0330, 0x0340):
                try:
                    val_clean = f"{float(val_clean):.2f} °C"
                except:
                    pass

            msg_data = {
                "type": "frame",
                "dir": "RX" if is_rx else "TX",
                "src": src,
                "dst": dst,
                "sym_id": sym_id,
                "sym_name": sym_name,
                "val": val_clean,
                "hop": hop,
                "crc": crc,
                "timestamp": time.strftime("%H:%M:%S") + f".{int((now % 1)*1000):03d}"
            }

        # Parse table rows: | 0xB89A | Serial+NOW+WiFi (ME) | 0 | 24s | ● LIVE |
        elif "│" in line and "0x" in line:
            cols = [c.strip() for c in line.split("│") if c.strip()]
            if len(cols) >= 4:
                raw_node = cols[0].split()[0]
                if not raw_node.startswith("0x") or len(raw_node) != 6:
                    return None
                node_id = raw_node
                transports = "".join(c for c in cols[1] if c.isprintable() or c == ' ').strip()
                rssi = cols[2].split()[0]
                uptime_str = cols[3].split()[0]
                status = cols[4] if len(cols) > 4 else "LIVE"

                if "(ME)" in transports:
                    self.local_node_id = node_id

                if node_id not in self.peers:
                    self.peers[node_id] = {
                        "node_id": node_id,
                        "transport": transports,
                        "rssi": rssi,
                        "uptime": 0,
                        "last_seen": time.time() if node_id == self.local_node_id else time.time(),
                        "rx_count": 1,
                        "tx_count": 0,
                        "alive": True
                    }
                else:
                    p = self.peers[node_id]
                    p["transport"] = transports
                    p["rssi"] = rssi
                    p["last_seen"] = time.time()
                    p["alive"] = True

        # Prune alive status (3.5s timeout for remote nodes)
        for p in self.peers.values():
            if p["node_id"] != self.local_node_id and (now - p["last_seen"] > 3.5):
                p["alive"] = False

        # Broadcast state to WebSocket clients
        payload = {
            "type": "update",
            "node_count": len([p for p in self.peers.values() if p["alive"]]),
            "total_nodes": len(self.peers),
            "msg_rate": round(self.msg_rate, 1),
            "total_msgs": self.total_msgs,
            "peers": list(self.peers.values()),
            "msg": msg_data,
            "raw_line": line
        }

        asyncio.run_coroutine_threadsafe(self.broadcast(json.dumps(payload)), loop)

    async def broadcast(self, data):
        if not self.clients:
            return
        dead = set()
        for ws in self.clients:
            try:
                await ws.send(data)
            except:
                dead.add(ws)
        self.clients -= dead

    async def handler(self, websocket):
        self.clients.add(websocket)
        print(f"Browser client connected (total: {len(self.clients)})", flush=True)

        # Send initial snapshot
        init_data = {
            "type": "init",
            "node_count": len([p for p in self.peers.values() if p["alive"]]),
            "peers": list(self.peers.values()),
            "total_msgs": self.total_msgs,
            "msg_rate": self.msg_rate
        }
        await websocket.send(json.dumps(init_data))

        try:
            async for message in websocket:
                # Handle commands from browser (e.g. ping, list)
                req = json.loads(message)
                action = req.get("action")
                if action == "list":
                    self.write_all(b"l\n")
                elif action == "ping":
                    self.write_all(b"p\n")
                elif action == "border_lock":
                    self.write_all(b"b\n")
                elif action == "border_unlock":
                    self.write_all(b"u\n")
                elif action == "knock":
                    self.write_all(b"k\n")
        except:
            pass
        finally:
            self.clients.discard(websocket)
            print(f"Browser client disconnected (remaining: {len(self.clients)})", flush=True)

def start_http_server(port, directory):
    handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory=str(directory))
    httpd = http.server.ThreadingHTTPServer(("0.0.0.0", port), handler)
    httpd.serve_forever()

async def main():
    parser = argparse.ArgumentParser(description="Pyintel Lux Mesh WebSocket Bridge & Dashboard")
    parser.add_argument("--port", default="auto", help="Serial COM port (default: auto - multi-port auto discover)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    parser.add_argument("--ws-port", type=int, default=8765, help="WebSocket port")
    parser.add_argument("--http-port", type=int, default=8080, help="HTTP dashboard port (default: 8080)")
    args = parser.parse_args()

    # Start HTTP server for dashboard in background thread
    dash_dir = pathlib.Path(__file__).resolve().parent
    threading.Thread(target=start_http_server, args=(args.http_port, dash_dir), daemon=True).start()

    bridge = MeshBridge(target_port=args.port, baud=args.baud, ws_port=args.ws_port)
    loop = asyncio.get_running_loop()
    bridge.start_serial_thread(loop)

    print("=" * 60, flush=True)
    print("🚀 Pyintel Lux Mesh Swarm Dashboard is LIVE!", flush=True)
    print(f"📡 Serial Ingest : Auto-discovering all USB serial nodes @ {args.baud} baud", flush=True)
    print(f"⚡ WebSocket Hub : ws://localhost:{args.ws_port}", flush=True)
    print(f"🌐 Web UI       : http://localhost:{args.http_port}", flush=True)
    print("=" * 60, flush=True)

    async with websockets.serve(bridge.handler, "0.0.0.0", args.ws_port):
        await asyncio.Future()

if __name__ == "__main__":
    asyncio.run(main())
