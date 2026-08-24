"""
Phase 6 Multi-Transport Adaptive Ingest Server & WebSocket Gateway
Listens on UART (Serial COM port) and UDP (4210) SIMULTANEOUSLY.
Ingests binary Lux frames from either physical path, parses active transport switches (0x0004)
and RSSI values (0x0005), and broadcasts live telemetry to the Web Dashboard.

Usage: python server.py [--port COM21] [--baud 115200] [--udp-port 4210] [--ws-port 8765]
Deps:  pip install websockets pyserial rich
"""

import asyncio
import argparse
import socket
import struct
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
    console.print(f"[bold green]Dashboard Connected:[/bold green] {websocket.remote_address} (Clients: {len(connected_clients)})")
    try:
        await websocket.wait_closed()
    finally:
        connected_clients.remove(websocket)
        console.print(f"[yellow]Dashboard Disconnected:[/yellow] {websocket.remote_address}")

async def broadcast_bytes(data: bytes, origin_channel: str):
    if not connected_clients:
        return
    # Prepend 1 byte transport tag (0x01=UART_RELAY, 0x02=UDP_DIRECT) for dashboard visualization
    tag = b'\x01' if origin_channel == "uart" else b'\x02'
    packet = tag + data
    to_remove = set()
    for ws in connected_clients:
        try:
            await ws.send(packet)
        except Exception:
            to_remove.add(ws)
    for ws in to_remove:
        connected_clients.discard(ws)

def uart_receiver_thread(port: str, baud: int, loop):
    console.print(f"Opening UART Relay Listener on [yellow]{port}[/yellow] @ {baud} baud...")
    try:
        with serial.Serial(port, baud, timeout=1) as ser:
            buf = b''
            while True:
                b = ser.read(ser.in_waiting or 1)
                if not b:
                    time.sleep(0.005)
                    continue
                buf += b
                if len(buf) >= HEADER_SIZE:
                    idx = buf.find(SYNC)
                    if idx != -1 and len(buf) - idx >= HEADER_SIZE:
                        plen = buf[idx + 11]
                        frame_len = HEADER_SIZE + plen
                        if len(buf) - idx >= frame_len:
                            frame_bytes = buf[idx:idx + frame_len]
                            asyncio.run_coroutine_threadsafe(broadcast_bytes(frame_bytes, "uart"), loop)
                            buf = buf[idx + frame_len:]
    except Exception as e:
        console.print(f"[red]UART Serial Error on {port}:[/red] {e}")

def udp_receiver_thread(port: int, loop):
    console.print(f"Opening UDP Wireless Listener on [yellow]0.0.0.0:{port}[/yellow]...")
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("0.0.0.0", port))
    sock.settimeout(0.5)

    while True:
        try:
            data, addr = sock.recvfrom(2048)
            if data:
                asyncio.run_coroutine_threadsafe(broadcast_bytes(data, "udp"), loop)
        except socket.timeout:
            continue
        except Exception as e:
            console.print(f"[red]UDP Error:[/red] {e}")
            break

async def main_async(args):
    loop = asyncio.get_running_loop()
    ws_server = await websockets.serve(register_client, "0.0.0.0", args.ws_port)
    console.print(f"\n[bold cyan]Pyintel Lux Dynamic Transport Server[/bold cyan] live on [bold green]ws://localhost:{args.ws_port}[/bold green]\n")

    # Start simultaneous dual-path listeners
    loop.run_in_executor(None, uart_receiver_thread, args.port, args.baud, loop)
    loop.run_in_executor(None, udp_receiver_thread, args.udp_port, loop)

    await asyncio.Future()

def main():
    parser = argparse.ArgumentParser(description="Pyintel Lux — Dynamic Adaptive Transport Multi-Channel Gateway")
    parser.add_argument("--port", default="COM21", help="Serial COM port for Board B Relay or Direct UART")
    parser.add_argument("--baud", type=int, default=115200, help="UART Baud Rate")
    parser.add_argument("--udp-port", type=int, default=4210, help="UDP Wireless Port")
    parser.add_argument("--ws-port", type=int, default=8765, help="WebSocket Server Port")
    args = parser.parse_args()

    try:
        asyncio.run(main_async(args))
    except KeyboardInterrupt:
        console.print("\n[yellow]Gateway stopped by user.[/yellow]")

if __name__ == "__main__":
    main()
