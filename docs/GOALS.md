# Pyintel Lux — Engineering Goals & Target Specifications

> These are the hard quantitative targets that define Pyintel Lux.
> Every design decision is measured against this document.
> If a target is not met, it is a bug, not a trade-off.

---

## Why Move from OpenTelemetry?

OpenTelemetry was designed for servers. It assumes:
- Heap allocator available
- Megabytes of RAM
- OS networking stack (TCP/IP, HTTP/2, gRPC)
- CPU time to spare for JSON/Protobuf serialization
- A stable internet connection to a cloud collector

None of these assumptions hold on bare-metal microcontrollers, vehicle CAN buses,
real-time robotics, or low-power sensor nodes. Applying OTel to these environments
means fighting the framework at every step.

Pyintel Lux is built from the opposite set of assumptions.

---

## 1. Memory Targets

| Metric | OpenTelemetry (OTel C++) | Pyintel Lux Target | Notes |
|---|---|---|---|
| Flash footprint (MCU) | ~200–500 KB | **< 4 KB** | Lux-emb C library only |
| Static RAM on MCU | ~50–200 KB | **< 1 KB** | No heap, no dynamic buffers |
| Heap allocation at runtime | Yes (malloc) | **Zero** | All buffers are static or stack |
| Stack usage per `lux_emit()` call | N/A | **< 64 bytes** | Measured target, Phase 1 |
| Binary size (luxd ingest daemon) | N/A | **< 5 MB** | Single Rust binary, no runtime deps |

---

## 2. Wire Protocol Efficiency

| Metric | OpenTelemetry (OTLP/JSON) | OpenTelemetry (OTLP/Protobuf) | Pyintel Lux Target |
|---|---|---|---|
| Minimum frame size (single event) | ~150–400 bytes | ~40–80 bytes | **12 bytes** |
| Fixed header overhead | ~100 bytes | ~20 bytes | **12 bytes** |
| String data on wire | Full UTF-8 strings | Full UTF-8 strings | **Zero** (token IDs only) |
| Runtime string formatting | Yes | Yes | **Never** (compile-time only) |
| Payload size reduction vs JSON | — | ~60% | **70–90%** |
| Payload size reduction vs Protobuf | — | — | **40–70%** |
| CRC protection | No (relies on TLS) | No (relies on TLS) | **CRC-16/CCITT per frame** |

---

## 3. Transfer Speed & Throughput

| Transport | Target Frame Rate | Notes |
|---|---|---|
| UART @ 115200 baud | **~800 frames/sec** | 12-byte min frame = ~10 bytes/frame on wire |
| UART @ 921600 baud | **~6,500 frames/sec** | High-speed serial |
| UDP (local LAN) | **~50,000 frames/sec** | MTU-limited, single-socket |
| ESP-NOW | **~700 frames/sec** | 250-byte max payload, ~1ms latency |
| CAN 2.0 @ 1 Mbps | **~500 frames/sec** | 8-byte CAN payload → fits 1 Lux arg per frame |
| CAN-FD @ 8 Mbps | **~4,000 frames/sec** | 64-byte CAN-FD payload → full Lux frame |
| WebSocket (local) | **~10,000 frames/sec** | Browser dashboard ingestion target |

> **Measurement mandate:** Phase 1–3 research must produce real measured numbers
> for each transport to validate or revise these targets before the spec is frozen.

---

## 4. Latency Targets

| Metric | Target | Notes |
|---|---|---|
| `lux_emit_u32()` execution time on ESP32 @ 240MHz | **< 2 µs** | Measured with `esp_timer_get_time()` |
| UART frame-to-host latency (115200 baud) | **< 1.5 ms** | 12 bytes @ 115200 = ~1.04ms |
| UDP frame-to-host latency (local LAN) | **< 2 ms** | Including IP/UDP stack |
| ESP-NOW node-to-node latency | **< 5 ms** | Measured end-to-end |
| luxd ingest-to-SQLite write latency | **< 10 ms** | Per frame, single writer |
| luxd-to-browser WebSocket latency | **< 50 ms** | End-to-end on local network |

---

## 5. CPU Cost on MCU

| Metric | Target | Notes |
|---|---|---|
| CPU cycles per `lux_emit_u32()` | **< 500 cycles** | @ 240MHz = < 2.1 µs |
| Interrupt-safe emit | **Yes** | No mutex, no blocking I/O in the emit path |
| RTOS task blocking | **Never** | Emit path must be usable from ISR context |
| FreeRTOS tick interference | **Zero** | No `vTaskDelay`, no queue waits in hot path |

---

## 6. Transport & Topology Requirements

| Requirement | Status |
|---|---|
| UART / DMA | Required — Phase 1 |
| Raw UDP | Required — Phase 2 |
| ESP-NOW (ESP32 only) | Required — Phase 3 |
| SPI | Planned — post-research |
| CAN 2.0 / CAN-FD | Planned — Athera Motion Core integration |
| WebSockets / WebTransport | Required — Phase 5 |
| Edge-to-Edge P2P (no router) | Required — Phase 3 |
| Edge-to-Server streaming | Required — Phase 2+ |
| Dynamic transport fallback | Required — lux-mesh |
| Air-gapped / offline operation | Required — zero cloud dependency |

---

## 7. Platform Support Matrix

| Platform | Language | Status |
|---|---|---|
| ESP32 (Xtensa LX7 / RISC-V) | C (`no_std`) | Phase 1 — active |
| RP2350 (Athera Motion Core) | C / Rust (`no_std`) | Planned |
| STM32 (Cortex-M) | C (`no_std`) | Planned |
| Linux / macOS / Windows desktop | Python, C++, .NET | Planned |
| Browser (WASM) | TypeScript | Planned |
| Rust `no_std` embedded | Rust | Planned |

---

## 8. Security Requirements

| Requirement | Target |
|---|---|
| Encryption | Optional symmetric AES-128-CTR payload encryption |
| Authentication | Optional HMAC-SHA256 frame signing |
| Default | Plaintext — optimized for local/air-gapped deployments |
| Cloud dependency | **Zero** — fully self-hosted by design |

---

## 9. Compatibility Goals

| Requirement | Target |
|---|---|
| OTLP export (via luxd bridge) | Full OTLP JSON + Protobuf export from luxd |
| Grafana datasource | Via ClickHouse or InfluxDB write from luxd |
| Jaeger / Zipkin | Via OTLP bridge |
| Prometheus scrape endpoint | Planned — luxd `/metrics` endpoint |
| OpenTelemetry SDK drop-in | **Not a goal** — Lux is a replacement, not a wrapper |

---

## 10. Open Standard Requirements

| Requirement | Target |
|---|---|
| Wire spec license | CC0 1.0 — fully public domain |
| Vendor lock-in | Zero — any device can implement the 12-byte frame |
| Cross-vendor adoption | Any manufacturer can build a Lux-compatible device |
| Governance | Pyintel-led, publicly documented at `pyintel.cc/lux` |

---

## 11. Research Validation Checklist

These questions must be answered with real measured data before v0.1 spec freeze:

- [ ] What is the actual `lux_emit_u32()` cycle count on ESP32-S3 @ 240MHz?
- [ ] What is the maximum sustainable UART frame rate before buffer overflow?
- [ ] What is the measured ESP-NOW node-to-node latency under load?
- [ ] What is the measured CRC failure rate on each transport under RF interference?
- [ ] How many bytes of stack does `lux_emit_bytes(64)` consume on ESP32?
- [ ] What is the luxd ingest throughput ceiling on a Raspberry Pi 4?
- [ ] What is the measured payload size reduction vs OTLP/JSON on real sensor data?
- [ ] Can `lux_emit_u32()` be called safely from an ESP32 ISR without RTOS impact?

---

*All targets are engineering commitments, not marketing claims.
Measured results from the Phase 1–5 research track replace estimated values above.*
