#!/usr/bin/env python3
"""
Query the live Lux Mesh status table from the connected board on COM9.
"""

import time
import serial
import serial.tools.list_ports

def query_mesh():
    ports = [p.device for p in serial.tools.list_ports.comports()]
    if not ports:
        print("❌ No serial ports found. Make sure a board is plugged in.")
        return

    port = ports[0]
    print(f"📡 Querying live mesh status from {port}...\n")

    try:
        ser = serial.Serial(port, 115200, timeout=0.1)
        ser.dtr = True
        ser.rts = True
    except Exception as e:
        print(f"❌ Error opening {port}: {e}")
        return

    # Send 'l' command to trigger Lux.list()
    time.sleep(0.5)
    ser.write(b"l\n")
    time.sleep(0.1)
    ser.write(b"l\n")

    t_end = time.time() + 4.0
    buffer = ""

    while time.time() < t_end:
        raw = ser.read(ser.in_waiting or 1)
        if raw:
            text = raw.decode("utf-8", errors="replace")
            buffer += text
            sys_out = text.replace("\r", "")
            print(sys_out, end="", flush=True)

    ser.close()

if __name__ == "__main__":
    query_mesh()
