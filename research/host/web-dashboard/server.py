"""
Phase 5 Host WebSocket Server — Bridges incoming Lux binary frames (UART or UDP) to a WebSocket (port 8765).
Streams raw binary frames to connected web browsers for real-time charting.

Usage:
  UART Mode: python server.py --transport uart --port COM9 --baud 115200 --ws-port 8765
  UDP Mode:  python server.py --transport udp --port 4210 --ws-port 8765
Deps:  pip install websockets pyserial rich
"""

import asyncio
import argparse
import os
import socket
import sys
import time
import serial
import websockets
from rich.console import Console

SYNC = b'\x4C\x58'
HEADER_SIZE = 14

connected_clients = set()
console = Console()

async def register_client(websocket):
    connected_clients.add(websocket)
    console.print(f"[green]Client connected:[/green] {websocket.remote_address} (Total clients: {len(connected_clients)})")
    try:
        await websocket.wait_closed()
    finally:
        connected_clients.remove(websocket)
        console.print(f"[yellow]Client disconnected:[/yellow] {websocket.remote_address} (Remaining: {len(connected_clients)})")

async def broadcast_bytes(data: bytes):
    if not connected_clients:
        return
    # Broadcast raw binary packet to all connected WebSockets
    to_remove = set()
    for ws in connected_clients:
        try:
            await ws.send(data)
        except Exception:
            to_remove.add(ws)
    for ws in to_remove:
        connected_clients.discard(ws)

def run_uart_listener(port: str, baud: int, loop):
    console.print(f"Opening Serial port [yellow]{port}[/yellow] @ {baud} baud...")
    with serial.Serial(port, baud, timeout=1) as ser:
        buf = b''
        while True:
            b = ser.read(ser.in_waiting or 1)
            if not b:
                time.sleep(0.01)
                continue
            buf += b
            if len(buf) >= HEADER_SIZE:
                # Find all LX sync magic frames
                idx = buf.find(SYNC)
                if idx != -1 and len(buf) - idx >= HEADER_SIZE:
                    plen = buf[idx + 11]
                    frame_len = HEADER_SIZE + plen
                    if len(buf) - idx >= frame_len:
                        frame_bytes = buf[idx:idx + frame_len]
                        asyncio.run_coroutine_threadsafe(broadcast_bytes(frame_bytes), loop)
                        buf = buf[idx + frame_len:]

def run_udp_listener(port: int, loop):
    console.print(f"Opening UDP socket on [yellow]0.0.0.0:{port}[/yellow]...")
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("0.0.0.0", port))
    sock.settimeout(0.5)

    while True:
        try:
            data, addr = sock.recvfrom(2048)
            if data:
                asyncio.run_coroutine_threadsafe(broadcast_bytes(data), loop)
        except socket.timeout:
            continue
        except Exception as e:
            console.print(f"[red]UDP Error:[/red] {e}")
            break

async def main_async(args):
    loop = asyncio.get_running_loop()
    ws_server = await websockets.serve(register_client, "0.0.0.0", args.ws_port)
    console.print(f"[bold cyan]Pyintel Lux[/bold cyan] — Phase 5 WebSocket Server live on [bold green]ws://localhost:{args.ws_port}[/bold green]")

    if args.transport == "uart":
        loop.run_in_executor(None, run_uart_listener, args.port, args.baud, loop)
    else:
        udp_port = int(args.port) if str(args.port).isdigit() else 4210
        loop.run_in_executor(None, run_udp_listener, udp_port, loop)

    await asyncio.Future()  # run forever

def main():
    parser = argparse.ArgumentParser(description="Pyintel Lux — Phase 5 WebSocket Telemetry Bridge")
    parser.add_argument("--transport", choices=["uart", "udp"], default="uart", help="Source transport mode")
    parser.add_argument("--port", default="COM9", help="Serial COM port or UDP port")
    parser.add_argument("--baud", type=int, default=115200, help="UART baud rate")
    parser.add_argument("--ws-port", type=int, default=8765, help="WebSocket server port")
    args = parser.parse_args()

    try:
        asyncio.run(main_async(args))
    except KeyboardInterrupt:
        console.print("\n[yellow]WebSocket server stopped by user.[/yellow]")

if __name__ == "__main__":
    main()
