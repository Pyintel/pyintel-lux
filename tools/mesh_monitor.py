#!/usr/bin/env python3
"""
Pyintel Lux — Live PC Mesh Monitor & Terminal Inspector
Connects to any node via USB Serial (and/or UDP 4210) to inspect all mesh traffic in real time.
"""

import sys
import time
import argparse
import serial

class LuxMeshMonitor:
    def __init__(self, port="COM9", baud=115200):
        self.port = port
        self.baud = baud
        self.running = True

    def start(self):
        print("\n" + "=" * 70, flush=True)
        print("  🛰️  PYINTEL LUX — PC MESH MONITOR & LIVE TRAFFIC INSPECTOR", flush=True)
        print("=" * 70, flush=True)
        print(f"Opening Serial on {self.port} @ {self.baud} baud...\n", flush=True)

        try:
            ser = serial.Serial(self.port, self.baud, timeout=0.1)
            ser.dtr = True
            ser.rts = True
        except Exception as e:
            print(f"❌ Error opening {self.port}: {e}", flush=True)
            return

        print(f"✅ Connected to {self.port}. Listening for live mesh telemetry...\n", flush=True)

        line_buffer = ""
        while self.running:
            try:
                raw = ser.read(ser.in_waiting or 1)
                if raw:
                    text = raw.decode("utf-8", errors="replace")
                    line_buffer += text
                    while "\n" in line_buffer:
                        line, line_buffer = line_buffer.split("\n", 1)
                        line = line.strip("\r")
                        if line:
                            self.handle_line(line)
            except KeyboardInterrupt:
                break
            except Exception as e:
                print(f"⚠️ Read error: {e}", flush=True)
                time.sleep(0.5)

        ser.close()
        print("\nMonitor stopped.", flush=True)

    def handle_line(self, line):
        timestamp = time.strftime("%H:%M:%S")

        # Colorize output
        if "PEER DISCOVERED" in line:
            print(f"[{timestamp}] 🟢 \033[92m{line}\033[0m", flush=True)
        elif "PING" in line:
            print(f"[{timestamp}] 🏓 \033[96m{line}\033[0m", flush=True)
        elif "PONG" in line:
            print(f"[{timestamp}] ⚡ \033[93m{line}\033[0m", flush=True)
        elif "TELEMETRY" in line:
            print(f"[{timestamp}] 📊 \033[94m{line}\033[0m", flush=True)
        elif "┌" in line or "│" in line or "└" in line or "├" in line:
            print(f"\033[95m{line}\033[0m", flush=True)
        elif "← RX" in line:
            print(f"[{timestamp}] \033[92m{line}\033[0m", flush=True)
        elif "→ TX" in line:
            print(f"[{timestamp}] \033[94m{line}\033[0m", flush=True)
        else:
            print(f"[{timestamp}] {line}", flush=True)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Pyintel Lux Mesh PC Monitor")
    parser.add_argument("--port", default="COM9", help="Serial port to monitor")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    args = parser.parse_args()

    monitor = LuxMeshMonitor(port=args.port, baud=args.baud)
    monitor.start()
