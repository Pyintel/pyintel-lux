"""
Phase 2 Host UDP Decoder — reads Lux binary frames over Wi-Fi (UDP).
Streams scientific telemetry to CSV and runs statistical analysis.

Usage: python host/udp_decode.py [--port 4210] [--duration 30] [--csv lux_udp_telemetry.csv]
Deps:  pip install pyserial rich
"""

import argparse
import csv
import socket
import struct
import subprocess
import sys
import time
from datetime import datetime
from rich.console import Console

SYNC = b'\x4C\x58'  # 'LX'
HEADER_SIZE = 14

SYSTEM_SYMBOLS = {
    0x0001: "LUX_SYM_HEARTBEAT",
    0x0002: "LUX_SYM_RESET",
    0x0003: "LUX_SYM_OVERFLOW",
    0x0004: "LUX_SYM_TRANSPORT_SWITCH",
}

APP_SYMBOLS = {
    0x0100: "APP_UPTIME",
    0x0101: "APP_COUNTER",
}

PAYLOAD_TYPES = {
    0x00: ("none",  0,  None),
    0x01: ("u8",    1,  "B"),
    0x02: ("u16",   2,  "<H"),
    0x03: ("u32",   4,  "<I"),
    0x04: ("i32",   4,  "<i"),
    0x05: ("f32",   4,  "<f"),
    0x06: ("bytes", -1, None),
    0x07: ("str_ref",2, "<H"),
}

def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = (crc << 1) ^ 0x1021 if crc & 0x8000 else crc << 1
        crc &= 0xFFFF
    return crc

def symbol_name(sym_id: int) -> str:
    return SYSTEM_SYMBOLS.get(sym_id) or APP_SYMBOLS.get(sym_id) or f"SYM_0x{sym_id:04X}"

def decode_payload(ptype: int, plen: int, data: bytes):
    info = PAYLOAD_TYPES.get(ptype)
    if info is None or ptype == 0x00:
        return "—"
    name, size, fmt = info
    if fmt and len(data) >= size:
        return struct.unpack(fmt, data[:size])[0]
    return data.hex()

def parse_lux_packet(packet_bytes: bytes):
    if len(packet_bytes) < HEADER_SIZE:
        return []

    frames = []
    offset = 0

    while offset < len(packet_bytes):
        # Look for LX magic
        idx = packet_bytes.find(SYNC, offset)
        if idx == -1 or (len(packet_bytes) - idx) < HEADER_SIZE:
            break

        header_bytes = packet_bytes[idx:idx + HEADER_SIZE]
        seq_num  = struct.unpack_from("<H", header_bytes, 2)[0]
        sym_id   = struct.unpack_from("<H", header_bytes, 4)[0]
        ts_us    = struct.unpack_from("<I", header_bytes, 6)[0]
        ptype    = header_bytes[10]
        plen     = header_bytes[11]
        crc_recv = struct.unpack_from("<H", header_bytes, 12)[0]

        crc_calc = crc16_ccitt(header_bytes[:12])
        crc_ok   = crc_calc == crc_recv

        payload_offset = idx + HEADER_SIZE
        payload_data = packet_bytes[payload_offset:payload_offset + plen]
        payload_val  = decode_payload(ptype, plen, payload_data)
        frame_size_bytes = HEADER_SIZE + plen

        frames.append({
            "seq_num":    seq_num,
            "sym_id":     sym_id,
            "sym_name":   symbol_name(sym_id),
            "ts_us":      ts_us,
            "ptype":      PAYLOAD_TYPES.get(ptype, ("?",))[0],
            "payload":    payload_val,
            "crc_ok":     crc_ok,
            "frame_size": frame_size_bytes,
        })

        offset = payload_offset + plen

    return frames

def main():
    parser = argparse.ArgumentParser(description="Pyintel Lux — Phase 2 Scientific UDP Decoder & Benchmark Logger")
    parser.add_argument("--host", default="0.0.0.0", help="Listen address")
    parser.add_argument("--port", type=int, default=4210, help="UDP Port")
    parser.add_argument("--duration", type=float, default=30.0, help="Logging duration in seconds (0 for infinite)")
    parser.add_argument("--csv", default="lux_udp_telemetry.csv", help="CSV log output filename")
    args = parser.parse_args()

    console = Console()
    console.print(f"[bold cyan]Pyintel Lux[/bold cyan] — Phase 2 Scientific UDP Decoder & Benchmark Logger")
    duration_str = f"{args.duration}s" if args.duration > 0 else "continuous"
    console.print(f"Listening on UDP [yellow]{args.host}:{args.port}[/yellow] | Duration: [bold magenta]{duration_str}[/bold magenta] | CSV: [bold green]{args.csv}[/bold green]\n")

    fieldnames = [
        "frame_index",
        "seq_num",
        "seq_delta",
        "iso_timestamp",
        "pc_recv_time_s",
        "inter_frame_delay_ms",
        "instant_fps",
        "moving_avg_fps",
        "symbol_name",
        "symbol_id",
        "esp_timestamp_us",
        "esp_ts_delta_us",
        "payload_type",
        "payload_value",
        "frame_bytes",
        "crc_ok"
    ]

    csv_file = open(args.csv, "w", newline="", encoding="utf-8")
    writer = csv.DictWriter(csv_file, fieldnames=fieldnames)
    writer.writeheader()
    csv_file.flush()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((args.host, args.port))
    sock.settimeout(1.0)

    last_recv_time = None
    last_esp_ts = None
    last_seq_num = None
    frame_timestamps = []
    window_size = 20
    frame_count = 0
    start_time = time.time()

    try:
        while True:
            now = time.time()
            if args.duration > 0 and (now - start_time) >= args.duration:
                console.print(f"\n[bold green]Completed {args.duration}s UDP capture session.[/bold green]")
                break

            try:
                data, addr = sock.recvfrom(2048)
            except socket.timeout:
                continue

            recv_time = time.time()
            frames = parse_lux_packet(data)

            for frame in frames:
                frame_count += 1
                iso_ts = datetime.fromtimestamp(recv_time).isoformat()

                # Sequence number gap analysis
                seq_num = frame["seq_num"]
                seq_delta = (seq_num - last_seq_num) if last_seq_num is not None else 1
                if seq_delta < 0:
                    seq_delta += 65536

                inter_frame_delay_ms = (recv_time - last_recv_time) * 1000.0 if last_recv_time else 0.0
                instant_fps = (1.0 / (recv_time - last_recv_time)) if (last_recv_time and recv_time > last_recv_time) else 0.0
                
                frame_timestamps.append(recv_time)
                if len(frame_timestamps) > window_size:
                    frame_timestamps.pop(0)

                if len(frame_timestamps) > 1 and (frame_timestamps[-1] - frame_timestamps[0]) > 0:
                    avg_fps = (len(frame_timestamps) - 1) / (frame_timestamps[-1] - frame_timestamps[0])
                else:
                    avg_fps = instant_fps

                esp_ts = frame["ts_us"]
                esp_ts_delta = (esp_ts - last_esp_ts) if last_esp_ts is not None else 0
                if esp_ts_delta < 0:
                    esp_ts_delta += 0xFFFFFFFF + 1

                last_recv_time = recv_time
                last_esp_ts = esp_ts
                last_seq_num = seq_num

                row = {
                    "frame_index": frame_count,
                    "seq_num": seq_num,
                    "seq_delta": seq_delta,
                    "iso_timestamp": iso_ts,
                    "pc_recv_time_s": f"{recv_time:.6f}",
                    "inter_frame_delay_ms": round(inter_frame_delay_ms, 3),
                    "instant_fps": round(instant_fps, 2),
                    "moving_avg_fps": round(avg_fps, 2),
                    "symbol_name": frame["sym_name"],
                    "symbol_id": f"0x{frame['sym_id']:04X}",
                    "esp_timestamp_us": esp_ts,
                    "esp_ts_delta_us": esp_ts_delta,
                    "payload_type": frame["ptype"],
                    "payload_value": frame["payload"],
                    "frame_bytes": frame["frame_size"],
                    "crc_ok": frame["crc_ok"]
                }
                writer.writerow(row)
                csv_file.flush()

                crc_str = "[green]OK[/green]" if frame["crc_ok"] else "[red]FAIL[/red]"
                drop_str = f"[bold red]LOST {seq_delta-1}[/bold red]" if seq_delta > 1 else "[green]0 drop[/green]"
                console.print(
                    f"[dim]{frame_count:5d}[/dim] "
                    f"seq=[yellow]{seq_num:5d}[/yellow] "
                    f"[cyan]{frame['sym_name']:<22}[/cyan] "
                    f"val=[bold]{str(frame['payload']):<8}[/bold] "
                    f"delay=[yellow]{inter_frame_delay_ms:6.1f}ms[/yellow] "
                    f"fps=[green]{avg_fps:5.2f}[/green] "
                    f"loss={drop_str} "
                    f"crc={crc_str}"
                )
    except KeyboardInterrupt:
        console.print("\n[bold yellow]Logging stopped by user.[/bold yellow]")
    finally:
        sock.close()
        csv_file.close()

    # Launch stats script automatically
    stats_script = str(sys.modules['os'].path.abspath(sys.modules['os'].path.join(sys.modules['os'].path.dirname(__file__), "../../../host/stats.py")))
    console.print(f"\n[bold cyan]Launching benchmark stats analyzer ([green]{stats_script}[/green])...[/bold cyan]\n")
    subprocess.run([sys.executable, stats_script, "--csv", args.csv])

if __name__ == "__main__":
    main()
