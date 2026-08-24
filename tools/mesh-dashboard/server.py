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
    0x0201: "SYM_PING",
    0x0202: "SYM_PONG",
    0x0301: "SYM_SENSOR_TEMP",
    0x0302: "SYM_SENSOR_VOLT",
    0x0401: "SYM_ALERT_TRIGGER",
}

class MeshBridge:
    def __init__(self, port="COM9", baud=115200, ws_port=8765):
        self.port = port
        self.baud = baud
        self.ws_port = ws_port
        self.clients = set()
        self.ser = None
        self.running = True
        self.local_node_id = None

        self.peers = {}
        self.total_msgs = 0
        self.msg_rate = 0.0
        self.rate_count = 0
        self.last_rate_calc = time.time()

    def start_serial_thread(self, loop):
        threading.Thread(target=self.serial_worker, args=(loop,), daemon=True).start()
        threading.Thread(target=self.prune_worker, args=(loop,), daemon=True).start()

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

    def serial_worker(self, loop):
        while self.running:
            try:
                print(f"Connecting to Serial on {self.port} @ {self.baud} baud...", flush=True)
                self.ser = serial.Serial(self.port, self.baud, timeout=0.1)
                self.ser.dtr = True
                self.ser.rts = True
                print(f"✅ Serial connected on {self.port}!", flush=True)

                line_buf = ""
                while self.running:
                    raw = self.ser.read(self.ser.in_waiting or 1)
                    if raw:
                        text = raw.decode("utf-8", errors="replace")
                        line_buf += text
                        while "\n" in line_buf:
                            line, line_buf = line_buf.split("\n", 1)
                            line = line.strip("\r")
                            if line:
                                self.process_line(line, loop)
            except Exception as e:
                print(f"⚠️ Serial error: {e}, reconnecting in 1s...", flush=True)
                time.sleep(1.0)

    def process_line(self, line, loop):
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

            msg_data = {
                "type": "frame",
                "dir": "RX" if is_rx else "TX",
                "src": src,
                "dst": dst,
                "sym_id": sym_id,
                "sym_name": sym_name,
                "val": val_raw,
                "hop": hop,
                "crc": crc,
                "timestamp": time.strftime("%H:%M:%S") + f".{int((now % 1)*1000):03d}"
            }

        # Parse table rows: | 0xB89A | Serial+NOW+WiFi (ME) | 0 | 24s | ● LIVE |
        elif "│" in line and "0x" in line:
            cols = [c.strip() for c in line.split("│") if c.strip()]
            if len(cols) >= 4:
                node_id = cols[0]
                transports = cols[1]
                rssi = cols[2]
                uptime_str = cols[3]
                status = cols[4] if len(cols) > 4 else "LIVE"

                if "(ME)" in transports:
                    self.local_node_id = node_id

                if node_id.startswith("0x"):
                    if node_id not in self.peers:
                        self.peers[node_id] = {
                            "node_id": node_id,
                            "transport": transports,
                            "rssi": rssi,
                            "uptime": 0,
                            "last_seen": time.time() if node_id == self.local_node_id else 0,
                            "rx_count": 1,
                            "tx_count": 0,
                            "alive": (node_id == self.local_node_id)
                        }
                    else:
                        p = self.peers[node_id]
                        p["transport"] = transports
                        p["rssi"] = rssi
                        if node_id == self.local_node_id:
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
                if action == "list" and self.ser:
                    self.ser.write(b"l\n")
                elif action == "ping" and self.ser:
                    self.ser.write(b"p\n")
        except:
            pass
        finally:
            self.clients.discard(websocket)
            print(f"Browser client disconnected (remaining: {len(self.clients)})", flush=True)

async def main():
    parser = argparse.ArgumentParser(description="Pyintel Lux Mesh WebSocket Bridge")
    parser.add_argument("--port", default="COM9", help="Serial COM port")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    parser.add_argument("--ws-port", type=int, default=8765, help="WebSocket port")
    args = parser.parse_args()

    bridge = MeshBridge(port=args.port, baud=args.baud, ws_port=args.ws_port)
    loop = asyncio.get_running_loop()
    bridge.start_serial_thread(loop)

    print(f"Pyintel Lux Mesh Bridge live on ws://localhost:{args.ws_port}", flush=True)
    async with websockets.serve(bridge.handler, "0.0.0.0", args.ws_port):
        await asyncio.Future()

if __name__ == "__main__":
    asyncio.run(main())
