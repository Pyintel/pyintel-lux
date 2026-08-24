#!/usr/bin/env python3
"""
Pyintel Lux — 120-Second High-Precision Mesh Flow Capture & Benchmark Analyzer
Captures all live wire traffic, calculates latency, throughput, PDR, and packet statistics.
"""

import sys
import time
import json
import serial
import serial.tools.list_ports

DURATION_SEC = 120
OUTPUT_FILE = "mesh_capture_120s.json"

def run_capture(port="COM9", baud=115200):
    print(f"\n=======================================================")
    print(f"  🛰️  PYINTEL LUX — 120-SECOND LIVE BENCHMARK CAPTURE")
    print(f"=======================================================")
    print(f"Connecting to {port} @ {baud} baud for {DURATION_SEC}s capture...\n")

    try:
        ser = serial.Serial(port, baud, timeout=0.1)
        ser.dtr = True
        ser.rts = True
    except Exception as e:
        print(f"❌ Error opening {port}: {e}")
        return

    frames = []
    nodes = {}
    pings_sent = {}
    rtt_samples = []
    crc_ok_count = 0
    crc_err_count = 0
    total_bytes = 0

    t_start = time.time()
    t_end = t_start + DURATION_SEC
    last_print = t_start

    # Send a list command to query current network topology
    time.sleep(0.5)
    ser.write(b"l\n")

    while time.time() < t_end:
        now = time.time()
        elapsed = now - t_start
        remaining = max(0, int(t_end - now))

        raw = ser.read(ser.in_waiting or 1)
        if raw:
            total_bytes += len(raw)
            text = raw.decode("utf-8", errors="replace")
            lines = text.split("\n")
            for line in lines:
                line = line.strip("\r")
                if not line:
                    continue

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
                    hop = parsed.get("hop", "4")
                    crc = parsed.get("crc", "OK")

                    if crc == "OK":
                        crc_ok_count += 1
                    else:
                        crc_err_count += 1

                    if src not in nodes:
                        nodes[src] = {"first_seen": elapsed, "last_seen": elapsed, "rx": 0, "tx": 0}
                    nodes[src]["last_seen"] = elapsed
                    if is_rx:
                        nodes[src]["rx"] += 1
                    else:
                        nodes[src]["tx"] += 1

                    # Ping-pong RTT tracking
                    if "SYM_PING" in line or sym_raw in ["0x201", "513"]:
                        try:
                            pings_sent[int(val_raw)] = now
                        except:
                            pass
                    elif "SYM_PONG" in line or sym_raw in ["0x202", "514"]:
                        try:
                            v = int(val_raw)
                            if v in pings_sent:
                                rtt = (now - pings_sent[v]) * 1000.0
                                rtt_samples.append(rtt)
                        except:
                            pass

                    frames.append({
                        "t_rel": round(elapsed, 4),
                        "dir": "RX" if is_rx else "TX",
                        "src": src,
                        "dst": dst,
                        "sym": sym_raw,
                        "val": val_raw,
                        "hop": hop,
                        "crc": crc
                    })

        if now - last_print >= 5.0:
            rate = len(frames) / max(0.1, elapsed)
            print(f"[{int(elapsed):03d}s / {DURATION_SEC}s] Captured: {len(frames)} frames | Rate: {rate:.1f} fps | Active Nodes: {len(nodes)} | Bytes: {total_bytes}", flush=True)
            last_print = now

    ser.close()

    total_time = time.time() - t_start
    summary = {
        "duration_sec": round(total_time, 2),
        "total_frames": len(frames),
        "total_bytes_raw": total_bytes,
        "avg_frame_rate_hz": round(len(frames) / total_time, 2),
        "avg_byte_throughput_bps": round((total_bytes * 8) / total_time, 2),
        "crc_ok": crc_ok_count,
        "crc_err": crc_err_count,
        "pdr_percent": round((crc_ok_count / max(1, crc_ok_count + crc_err_count)) * 100, 2),
        "unique_nodes": list(nodes.keys()),
        "node_stats": nodes,
        "rtt_samples_count": len(rtt_samples),
        "rtt_min_ms": round(min(rtt_samples), 2) if rtt_samples else 0.0,
        "rtt_max_ms": round(max(rtt_samples), 2) if rtt_samples else 0.0,
        "rtt_avg_ms": round(sum(rtt_samples) / max(1, len(rtt_samples)), 2) if rtt_samples else 0.0,
        "frames_sample": frames[:200]
    }

    with open(OUTPUT_FILE, "w") as f:
        json.dump(summary, f, indent=2)

    print(f"\n✅ Capture Complete! Summary saved to {OUTPUT_FILE}")
    print(f"Total Frames: {summary['total_frames']}")
    print(f"Avg Frame Rate: {summary['avg_frame_rate_hz']} Hz")
    print(f"Packet Delivery Ratio (PDR): {summary['pdr_percent']}%")
    print(f"Avg RTT Latency: {summary['rtt_avg_ms']} ms (samples: {len(rtt_samples)})")
    print(f"Unique Nodes Tracked: {summary['unique_nodes']}\n")

if __name__ == "__main__":
    run_capture()
