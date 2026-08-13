# Pyintel Lux — ESP32 Research Plan

Two ESP32 boards. One research goal: prove that the Lux binary protocol works
end-to-end across the full stack before writing a single line of "production" SDK code.

---

## Hardware You Need

| Item | Role |
|---|---|
| ESP32 Board A ("Emitter") | Runs firmware that emits Lux binary frames |
| ESP32 Board B ("Receiver / Relay") | Receives frames, relays to host via USB-Serial |
| USB cables × 2 | Connect both boards to your PC |
| PC (Windows) | Runs `luxd` host ingest script + web dashboard |

Both boards can be the same model (ESP32-DevKitC, ESP32-WROOM, etc.).

---

## Toolchain Setup (Do This First)

```bash
# 1. Install ESP-IDF v5.x (the official Espressif framework)
#    Download from: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/
#    Use the Windows installer — sets up Python, CMake, Ninja automatically.

# 2. Verify your install
idf.py --version    # should print 5.x.x

# 3. Install Python deps for the host research scripts
pip install pyserial rich

# 4. Install Node.js (for lux-web dashboard later)
#    https://nodejs.org — LTS version
```

---

## Phase Overview

```
Phase 1  →  Phase 2  →  Phase 3  →  Phase 4  →  Phase 5
UART emit    UDP stream   ESP-NOW     Host luxd    Web dashboard
(1 board)   (1 board +   P2P mesh    ingest        live Grafana
             PC)          (2 boards)  proxy         or browser UI
```

---

## Phase 1 — UART Emit & Host Decode (1 ESP32)

**Goal:** Prove the 12-byte Lux frame travels from ESP32 firmware → USB-Serial → Python host decoder correctly.

**What you learn:**
- How to write an ESP-IDF component
- How `lux_emit_u32()` assembles the binary frame in C with zero heap usage
- How CRC-16 protects frame integrity
- How a Python script re-syncs on `0x4C 0x58` and decodes fields

**Steps:**
1. Open `research/esp32/phase1-uart-emit/` in VS Code with the ESP-IDF extension.
2. Flash `main/main.c` to Board A.
3. Run `host/decode.py` on your PC — it reads the COM port and prints decoded frames.
4. Observe: symbol ID, timestamp, payload value in the terminal.

**Success criteria:** Python host prints decoded `LUX_SYM_HEARTBEAT` frames at ~1 Hz with correct µs timestamps.

**Directory:** `research/esp32/phase1-uart-emit/`

---

## Phase 2 — UDP Stream (1 ESP32 + PC on same Wi-Fi)

**Goal:** Replace UART with UDP — ESP32 sends Lux frames over Wi-Fi to the PC host.

**What you learn:**
- How the transport layer is swapped with zero changes to frame assembly code
- How `lwIP` UDP sockets work on ESP-IDF
- How to handle packet loss (CRC check + re-sync)

**Steps:**
1. Copy Phase 1 firmware, replace the `lux_write_fn` UART callback with a UDP `sendto()`.
2. Set your Wi-Fi SSID/password in `sdkconfig` (menuconfig).
3. Run `host/udp_decode.py` — listens on UDP port `4210` (Lux default port).
4. Verify same frame output as Phase 1 but now wireless.

**Success criteria:** Frames arrive over UDP with < 1% loss on a local network.

**Directory:** `research/esp32/phase2-udp-stream/`

---

## Phase 3 — ESP-NOW P2P Mesh (2 ESP32s, no router)

**Goal:** Board A emits Lux frames directly to Board B over ESP-NOW (no Wi-Fi router required). Board B relays decoded data to PC via UART.

**What you learn:**
- ESP-NOW — Espressif's connectionless 802.11 protocol (250-byte max payload, ~1ms latency)
- Edge-to-Edge topology with zero infrastructure dependency
- How `lux-mesh` transport switching will work in the real SDK
- MAC address pairing between two ESP32 nodes

**Steps:**
1. Flash `phase3-esp-now-mesh/emitter/` to Board A.
2. Flash `phase3-esp-now-mesh/receiver/` to Board B.
3. Board B receives Lux frames over ESP-NOW and forwards them over UART to your PC.
4. Run `host/decode.py` as in Phase 1 — same output, different transport path.

**Success criteria:** Board B relays Lux frames from Board A with ESP-NOW; host decoder sees correct frames with original Board A timestamps intact.

**Directory:** `research/esp32/phase3-esp-now-mesh/`

---

## Phase 4 — Host `luxd` Ingest Proxy (Python prototype)

**Goal:** Build a minimal Python prototype of `luxd` — the eventual Rust ingest daemon. Receives Lux frames from any source, de-tokenizes using `symbols.json`, and writes to SQLite.

**What you learn:**
- How the symbol dictionary maps IDs back to human-readable names
- How `luxd` will store telemetry for Grafana / dashboards
- The shape of the eventual Rust `lux-bridge` implementation

**Steps:**
1. Run `lux-dict-gen` (manual step for now — write `symbols.json` by hand).
2. Run `research/host/luxd-ingest/luxd_proto.py` — reads from UDP or serial.
3. Watch `telemetry.db` fill with decoded rows.
4. Open `telemetry.db` in DB Browser for SQLite and explore the data.

**Success criteria:** SQLite contains rows with `symbol_name`, `timestamp_us`, `payload_value` for every frame emitted by the ESP32.

**Directory:** `research/host/luxd-ingest/`

---

## Phase 5 — Web Dashboard (Browser Live View)

**Goal:** Stream live Lux frames from the host to a browser using WebSockets. Decode with `@pyintel/lux-web` (TypeScript `DataView`) and render a live chart.

**What you learn:**
- How `lux-web` will consume the binary stream in the browser
- WebSocket framing and `ArrayBuffer` handling in TypeScript
- The full end-to-end trace: ESP32 interrupt → Wi-Fi/UART → luxd → WebSocket → browser chart

**Steps:**
1. Run `research/host/web-dashboard/server.py` — bridges UDP/serial → WebSocket.
2. Open `research/host/web-dashboard/index.html` in browser.
3. Watch live sensor values plot in real time.

**Success criteria:** Browser chart updates in real time with data originating from the ESP32 firmware, with sub-100ms visible latency on a local network.

**Directory:** `research/host/web-dashboard/`

---

## Research Questions to Answer Per Phase

Track your findings in each phase's `NOTES.md`:

- What is the actual measured frame rate sustainable over each transport?
- What is the worst-case frame assembly time on the ESP32 (use `esp_timer_get_time()`)?
- How many bytes of stack does `lux_emit_u32()` consume?
- What happens when the host is slow — does the ESP32 block or drop?
- What is the measured CRC failure rate on each transport?

---

## Next After Research

Once all 5 phases pass, you have empirical data to:
1. Freeze `spec/wire-format.md` (no more guessing — real numbers)
2. Write production `lux-emb` C library under `lux-emb/c/`
3. Port to Rust `no_std` under `lux-emb/rust/`
4. Rewrite `luxd` in Rust under `lux-bridge/`
5. Write `@pyintel/lux-web` TypeScript package under `lux-web/`
