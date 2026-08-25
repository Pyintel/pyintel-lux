# Pyintel Lux — Development Roadmap

## Phase Order

Work through these in sequence. Do not skip phases — each one builds on the last.

### Research Track (Completed & Archived)
```
[x] Phase 1 — UART Emit & Host Decode       archive/research/esp32/phase1-uart-emit/
[x] Phase 2 — UDP Stream                    archive/research/esp32/phase2-udp-stream/
[x] Phase 3 — ESP-NOW P2P Mesh (2 boards)   archive/research/esp32/phase3-esp-now-mesh/
[x] Phase 4 — Host luxd Ingest Prototype    archive/research/host/luxd-ingest/
[x] Phase 5 — Web Dashboard Live View       archive/research/host/web-dashboard/
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
