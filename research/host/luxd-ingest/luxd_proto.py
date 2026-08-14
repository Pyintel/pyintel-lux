"""
Phase 4 Host luxd Ingest Daemon Prototype — Receives Lux binary frames from UART or UDP,
de-tokenizes symbol IDs using symbols.json, and writes records to a SQLite telemetry database (telemetry.db).

Usage:
  UART Mode: python luxd_proto.py --transport uart --port COM9 --baud 115200
  UDP Mode:  python luxd_proto.py --transport udp --port 4210
"""

import argparse
import json
import os
import socket
import sqlite3
import struct
import sys
import time
from datetime import datetime
import serial
from rich.console import Console
from rich.table import Table

SYNC = b'\x4C\x58'  # 'LX'
HEADER_SIZE = 14

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

def load_symbols(json_path: str):
    symbols_map = {}
    if os.path.exists(json_path):
        with open(json_path, "r", encoding="utf-8") as f:
            data = json.load(f)
            for hex_id, sym_info in data.get("symbols", {}).items():
                int_id = int(hex_id, 16)
                symbols_map[int_id] = sym_info["name"]
    return symbols_map

def decode_payload(ptype: int, plen: int, data: bytes):
    info = PAYLOAD_TYPES.get(ptype)
    if info is None or ptype == 0x00:
        return None
    name, size, fmt = info
    if fmt and len(data) >= size:
        return struct.unpack(fmt, data[:size])[0]
    return data.hex()

def init_db(db_path: str):
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS frames (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            received_at TEXT NOT NULL,
            transport TEXT NOT NULL,
            seq_num INTEGER NOT NULL,
            symbol_id INTEGER NOT NULL,
            symbol_name TEXT NOT NULL,
            esp_timestamp_us INTEGER NOT NULL,
            payload_type TEXT NOT NULL,
            payload_value TEXT,
            frame_bytes INTEGER NOT NULL,
            crc_ok INTEGER NOT NULL
        )
    """)
    conn.commit()
    return conn

def parse_lux_buffer(buffer: bytes, symbols_map: dict):
    frames = []
    offset = 0
    while offset < len(buffer):
        idx = buffer.find(SYNC, offset)
        if idx == -1 or (len(buffer) - idx) < HEADER_SIZE:
            break

        header_bytes = buffer[idx:idx + HEADER_SIZE]
        seq_num  = struct.unpack_from("<H", header_bytes, 2)[0]
        sym_id   = struct.unpack_from("<H", header_bytes, 4)[0]
        ts_us    = struct.unpack_from("<I", header_bytes, 6)[0]
        ptype    = header_bytes[10]
        plen     = header_bytes[11]
        crc_recv = struct.unpack_from("<H", header_bytes, 12)[0]

        crc_calc = crc16_ccitt(header_bytes[:12])
        crc_ok   = 1 if crc_calc == crc_recv else 0

        payload_offset = idx + HEADER_SIZE
        payload_data = buffer[payload_offset:payload_offset + plen]
        payload_val  = decode_payload(ptype, plen, payload_data)
        frame_size_bytes = HEADER_SIZE + plen

        sym_name = symbols_map.get(sym_id, f"SYM_0x{sym_id:04X}")
        ptype_name = PAYLOAD_TYPES.get(ptype, ("unknown",))[0]

        frames.append({
            "seq_num":    seq_num,
            "sym_id":     sym_id,
            "sym_name":   sym_name,
            "ts_us":      ts_us,
            "ptype":      ptype_name,
            "payload":    str(payload_val) if payload_val is not None else "",
            "crc_ok":     crc_ok,
            "frame_size": frame_size_bytes,
        })
        offset = payload_offset + plen

    return frames

def main():
    parser = argparse.ArgumentParser(description="Pyintel Lux — Host luxd Ingest Daemon Prototype")
    parser.add_argument("--transport", choices=["uart", "udp"], default="uart", help="Transport mode")
    parser.add_argument("--port", default="COM9", help="Serial port or UDP port (e.g., COM9 or 4210)")
    parser.add_argument("--baud", type=int, default=115200, help="UART Baud rate")
    parser.add_argument("--db", default="telemetry.db", help="SQLite database output path")
    parser.add_argument("--symbols", default="symbols.json", help="Symbol dictionary JSON path")
    parser.add_argument("--duration", type=float, default=30.0, help="Ingest duration in seconds (0 for infinite)")
    args = parser.parse_args()

    console = Console()
    console.print("[bold cyan]Pyintel Lux[/bold cyan] — Host [bold green]luxd[/bold green] Ingest Daemon Prototype")

    symbols_map = load_symbols(args.symbols)
    console.print(f"Loaded [yellow]{len(symbols_map)}[/yellow] symbol definitions from [bold green]{args.symbols}[/bold green]")

    conn = init_db(args.db)
    console.print(f"Connected to SQLite telemetry database at [bold green]{args.db}[/bold green]\n")

    frame_count = 0
    start_time = time.time()

    if args.transport == "uart":
        console.print(f"Listening on Serial [yellow]{args.port}[/yellow] @ {args.baud} baud...")
        with serial.Serial(args.port, args.baud, timeout=1) as ser:
            try:
                buf = b''
                while True:
                    if args.duration > 0 and (time.time() - start_time) >= args.duration:
                        break

                    b = ser.read(1)
                    if not b:
                        continue
                    buf += b
                    if len(buf) >= HEADER_SIZE:
                        frames = parse_lux_buffer(buf, symbols_map)
                        for f in frames:
                            frame_count += 1
                            now_iso = datetime.now().isoformat()
                            conn.execute("""
                                INSERT INTO frames (received_at, transport, seq_num, symbol_id, symbol_name, esp_timestamp_us, payload_type, payload_value, frame_bytes, crc_ok)
                                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                            """, (now_iso, "uart", f["seq_num"], f["sym_id"], f["sym_name"], f["ts_us"], f["ptype"], f["payload"], f["frame_size"], f["crc_ok"]))
                            conn.commit()
                            console.print(f"[dim]{frame_count:5d}[/dim] [cyan]{f['sym_name']:<22}[/cyan] seq={f['seq_num']} val={f['payload']} → [green]DB Saved[/green]")
                        if frames:
                            buf = b''
            except KeyboardInterrupt:
                console.print("\n[yellow]Stopped by user.[/yellow]")
    else:
        udp_port = int(args.port) if args.port.isdigit() else 4210
        console.print(f"Listening on UDP socket [yellow]0.0.0.0:{udp_port}[/yellow]...")
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind(("0.0.0.0", udp_port))
        sock.settimeout(1.0)
        try:
            while True:
                if args.duration > 0 and (time.time() - start_time) >= args.duration:
                    break
                try:
                    data, addr = sock.recvfrom(2048)
                except socket.timeout:
                    continue

                frames = parse_lux_buffer(data, symbols_map)
                for f in frames:
                    frame_count += 1
                    now_iso = datetime.now().isoformat()
                    conn.execute("""
                        INSERT INTO frames (received_at, transport, seq_num, symbol_id, symbol_name, esp_timestamp_us, payload_type, payload_value, frame_bytes, crc_ok)
                        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                    """, (now_iso, "udp", f["seq_num"], f["sym_id"], f["sym_name"], f["ts_us"], f["ptype"], f["payload"], f["frame_size"], f["crc_ok"]))
                    conn.commit()
                    console.print(f"[dim]{frame_count:5d}[/dim] [cyan]{f['sym_name']:<22}[/cyan] seq={f['seq_num']} val={f['payload']} → [green]DB Saved[/green]")
        except KeyboardInterrupt:
            console.print("\n[yellow]Stopped by user.[/yellow]")
        finally:
            sock.close()

    cursor = conn.cursor()
    cursor.execute("SELECT COUNT(*) FROM frames")
    total_saved = cursor.fetchone()[0]
    conn.close()

    console.print(f"\n[bold green]luxd Ingestion Complete![/bold green] Total records stored in '{args.db}': [bold yellow]{total_saved}[/bold yellow]")

if __name__ == "__main__":
    main()
