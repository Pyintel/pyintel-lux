# Pyintel Lux — Development Roadmap

## Phase Order

Work through these in sequence. Do not skip phases — each one builds on the last.

### Research Track (Start Here — ESP32 Lab)
```
[ ] Phase 1 — UART Emit & Host Decode       research/esp32/phase1-uart-emit/
[ ] Phase 2 — UDP Stream                    research/esp32/phase2-udp-stream/
[ ] Phase 3 — ESP-NOW P2P Mesh (2 boards)   research/esp32/phase3-esp-now-mesh/
[ ] Phase 4 — Host luxd Ingest Prototype    research/host/luxd-ingest/
[ ] Phase 5 — Web Dashboard Live View       research/host/web-dashboard/
```

### Production SDK Track (After Research)
```
[ ] Freeze spec/wire-format.md (backed by measured data from research)
[ ] lux-emb/c/    — production no_std C library
[ ] lux-emb/rust/ — production no_std Rust crate
[ ] tools/lux-dict-gen/ — compile-time symbol extractor CLI
[ ] lux-bridge/   — rewrite luxd in Rust
[ ] lux-client/   — Python / .NET / C++ desktop SDKs
[ ] lux-web/      — @pyintel/lux-web TypeScript WASM decoder
[ ] lux-mesh/     — dynamic transport fallback manager
```

### Launch Track
```
[ ] Public spec release (CC0) on pyintel.cc/lux
[ ] GitHub org repo pyintel/pyintel-lux
[ ] lux-emb Arduino/ESP-IDF component registry publish
[ ] pyintel-lux PyPI publish
[ ] @pyintel/lux-web npm publish
[ ] luxd crates.io publish
```
