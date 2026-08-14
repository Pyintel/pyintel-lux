# Pyintel Lux

> Open binary telemetry standard — zero-copy, sub-kilobyte observability from bare-metal microcontrollers to web dashboards.

**Short-form:** `lux` | **Sync magic:** `0x4C 0x58` (`LX`) | **Status:** In Production

> This project is still in active development. Contributions are welcome — open an issue, start a discussion, or submit a PR.

---

## What is Pyintel Lux?

Pyintel Lux is an open binary telemetry standard that does for constrained edge and embedded systems what OpenTelemetry does for cloud-native microservices — at 1/100th the footprint.

| | OpenTelemetry | Pyintel Lux |
|---|---|---|
| Wire format | JSON / Protobuf (HTTP) | 14-byte zero-copy binary frame |
| MCU support | None | First-class `no_std` C & Rust |
| Runtime RAM on device | MBs | `< 1 KB static` |
| Transport | HTTP / gRPC | UART, CAN, UDP, ESP-NOW, WebSockets |
| Topology | Cloud-centric | Edge-to-Edge P2P + Edge-to-Server |
| String handling | Runtime formatting | Compile-time tokenization |

---

## Repository Structure

```
pyintel-lux/
├── spec/                    Wire frame spec & symbol dictionary schema
├── lux-core/                Core frame assembly & sync protocol (C header)
├── lux-emb/                 no_std MCU SDK — C and Rust (ESP32, RP2350, STM32)
├── lux-client/              Desktop SDKs — Python, .NET, C++
├── lux-web/                 @pyintel/lux-web — TypeScript / WASM decoder
├── lux-bridge/              luxd — Rust ingest proxy & OTLP bridge
├── lux-mesh/                Dynamic transport fallback manager
├── tools/lux-dict-gen/      Compile-time symbol dictionary extractor
├── research/                Hands-on ESP32 experiments (start here)
├── examples/                Working end-to-end example sketches
└── docs/                    Deep-dive documentation
```

---

## Technical Blog & Empirical Benchmarks

- ✍️ **[Technical Blog Series](research/BLOG.md)** — *Building Lux: A Microsecond-Latency Binary Telemetry Engine for Microcontrollers*
- 📖 **[Research Overview & Results](research/README.md)** — Complete 5-phase empirical hardware benchmarks.
- ⚡ **[Phase 1 — UART Emit](research/esp32/phase1-uart-emit/README.md)** — Hardware emission over USB-Serial (100% PDR, 4 µs emit time).
- 📡 **[Phase 2 — UDP Stream over Wi-Fi](research/esp32/phase2-udp-stream/README.md)** — Wireless UDP broadcast with 0.00 ms batched intra-delay & sequence loss tracking.
- 🔗 **[Phase 3 — ESP-NOW P2P Mesh](research/esp32/phase3-esp-now-mesh/README.md)** — Offline 2-board wireless mesh bridge (100% PDR, 4.06 ms latency).
- 🗄️ **[Phase 4 — Host luxd Ingest Proxy](research/host/luxd-ingest/README.md)** — Symbol dictionary de-tokenization & SQLite storage.
- 🖥️ **[Phase 5 — Web Dashboard](research/host/web-dashboard/README.md)** — Live WebSocket bridge & in-browser `DataView` Chart.js rendering.

---

## Quickstart — ESP32 Research Lab

See [`research/README.md`](research/README.md) for the ordered research plan with two ESP32 boards.

---

## License

| Asset | License |
|---|---|
| `spec/` — Wire frame specification | CC0 1.0 (Public Domain) |
| `lux-emb/` — MCU SDK | MIT |
| `lux-bridge/` — `luxd` ingest proxy | Apache 2.0 |
| `lux-client/`, `lux-web/` — SDKs | MIT |
| Documentation | CC-BY-4.0 |
