# Phase 4 — Host luxd Ingest Prototype

**Status:** ✅ **COMPLETE & FUNCTIONAL**  

A Python prototype of the eventual Rust `luxd` daemon. Receives Lux binary frames (UART or UDP), de-tokenizes symbol IDs using `symbols.json`, and writes structured rows to a SQLite database (`telemetry.db`).

## Run Ingest Proxy

### UART Serial Ingest
```powershell
python research/host/luxd-ingest/luxd_proto.py --transport uart --port COM9 --baud 115200 --duration 30 --db telemetry.db
```

### UDP Wireless Ingest
```powershell
python research/host/luxd-ingest/luxd_proto.py --transport udp --port 4210 --duration 30 --db telemetry.db
```

## Database Schema (`telemetry.db`)
`luxd_proto.py` creates a `frames` table storing:
- `id`: Auto-increment primary key
- `received_at`: ISO 8601 host arrival timestamp
- `transport`: `uart` or `udp`
- `seq_num`: Monotonic 16-bit sequence number
- `symbol_id`: Integer token (e.g. `0x0001`, `0x0101`)
- `symbol_name`: De-tokenized string from `symbols.json` (e.g. `LUX_SYM_HEARTBEAT`)
- `esp_timestamp_us`: Microsecond hardware timestamp from ESP32
- `payload_type` & `payload_value`: Decoded type and value
- `frame_bytes`: Packet size in bytes
- `crc_ok`: Checksum status (`1` for OK, `0` for FAIL)

## Symbol Dictionary (`symbols.json`)
The symbol map translates 16-bit numeric symbol IDs emitted by microcontrollers back into human-readable strings without sending string bytes over the wire.
