"""
Phase 1 Host Decoder — reads Lux binary frames from a serial COM port.
Usage: python decode.py --port COM3 --baud 115200
Deps:  pip install pyserial rich
"""

import argparse
import struct
import serial
from rich.console import Console
from rich.table import Table
from datetime import datetime

SYNC = b'\x4C\x58'  # 'LX'
HEADER_SIZE = 12

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

def read_frame(port: serial.Serial):
    # Re-sync: scan for LX magic
    buf = b''
    while True:
        b = port.read(1)
        if not b:
            continue
        buf += b
        if buf[-2:] == SYNC:
            break
        if len(buf) > 4096:
            buf = b''

    # Read remainder of header (10 more bytes after the 2-byte sync)
    rest = port.read(HEADER_SIZE - 2)
    if len(rest) < HEADER_SIZE - 2:
        return None

    header_bytes = SYNC + rest
    sym_id, ts_us, ptype, plen, crc_recv = struct.unpack_from("<HHI BB H", header_bytes, offset=0)[1:]
    # Actually unpack cleanly:
    sync     = header_bytes[0:2]
    sym_id   = struct.unpack_from("<H", header_bytes, 2)[0]
    ts_us    = struct.unpack_from("<I", header_bytes, 4)[0]
    ptype    = header_bytes[8]
    plen     = header_bytes[9]
    crc_recv = struct.unpack_from("<H", header_bytes, 10)[0]

    crc_calc = crc16_ccitt(header_bytes[:10])
    crc_ok   = crc_calc == crc_recv

    payload_data = port.read(plen) if plen > 0 else b''
    payload_val  = decode_payload(ptype, plen, payload_data)

    return {
        "sym_id":   sym_id,
        "sym_name": symbol_name(sym_id),
        "ts_us":    ts_us,
        "ptype":    PAYLOAD_TYPES.get(ptype, ("?",))[0],
        "payload":  payload_val,
        "crc_ok":   crc_ok,
    }

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port",  default="COM3")
    parser.add_argument("--baud",  type=int, default=115200)
    args = parser.parse_args()

    console = Console()
    console.print(f"[bold cyan]Pyintel Lux[/bold cyan] — Phase 1 UART Decoder")
    console.print(f"Listening on [yellow]{args.port}[/yellow] @ {args.baud} baud\n")

    with serial.Serial(args.port, args.baud, timeout=2) as port:
        frame_count = 0
        while True:
            frame = read_frame(port)
            if frame is None:
                continue
            frame_count += 1
            crc_str = "[green]OK[/green]" if frame["crc_ok"] else "[red]FAIL[/red]"
            console.print(
                f"[dim]{frame_count:5d}[/dim] "
                f"[cyan]{frame['sym_name']:<30}[/cyan] "
                f"ts=[yellow]{frame['ts_us']:>10}µs[/yellow] "
                f"type=[magenta]{frame['ptype']:<6}[/magenta] "
                f"val=[bold]{frame['payload']}[/bold] "
                f"crc={crc_str}"
            )

if __name__ == "__main__":
    main()
