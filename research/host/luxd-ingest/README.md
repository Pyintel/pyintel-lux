# Phase 4 — Host luxd Ingest Prototype

**Status:** Not started (complete Phase 3 first)

A Python prototype of the eventual Rust `luxd` daemon. Receives Lux frames (UDP or serial), de-tokenizes using `symbols.json`, writes rows to SQLite.

## Run
```bash
pip install pyserial rich
python luxd_proto.py --transport uart --port COM3 --baud 115200
# or
python luxd_proto.py --transport udp --port 4210
```

## Output
Creates `telemetry.db` with a `frames` table:
- `id`, `received_at`, `symbol_id`, `symbol_name`, `timestamp_us`, `payload_type`, `payload_value`

## Notes
<!-- Add your observations here -->
