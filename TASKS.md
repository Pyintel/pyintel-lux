# Pyintel Lux & Apex Studio — Master Task Plan

> **Scope:** Unified task backlog for integrating Pyintel Lux binary telemetry, Arduino ecosystem compatibility, and in-browser WebSerial flashing in [Apex Studio](file:///P:/Projects/Pyintel/Apex/apex-studio).

---

## 📋 Status Overview

```
Phase 1 (Arduino Core Lib)  → Phase 2 (Apex Base Sketch)  → Phase 3 (Web Flasher)     → Phase 4 (Apex UI & Telemetry) → Phase 5 (Protocol Freeze)
[x] COMPLETE                  [ ] Not Started             [ ] In Progress (Apex)      [ ] In Progress (Apex)          [ ] Research Complete
```

---

## Phase 1 — Arduino Ecosystem Integration (`lux-arduino`)

**Goal:** Package `lux-core` as an official, zero-dependency Arduino library that runs on 8-bit AVR, SAMD, RP2040, and ESP32 with 100% standard library compatibility.

- [x] **1.1. Create `lux-arduino` Library Structure**
  - **Path:** `lux-emb/arduino/`
  - Created standard Arduino library structure: `library.properties`, `src/Lux.h`, `src/Lux.cpp`, `src/lux/lux.h`, `src/lux/lux.c`, `keywords.txt`, `README.md`, `examples/`.
  - Wrapped `lux-core` for clean C++ usage (`Lux.begin()`, `Lux.trace()`, `Lux.trace_u32()`, `Lux.trace_f32()`, `Lux.trace_bytes()`, `Lux.trace_str()`).
- [x] **1.2. Multi-Architecture Compatibility & Testing**
  - **Targets:** ATmega328P (Uno/Nano @ 16MHz), ATmega2560 (Mega), SAMD21 (Zero/MKR Zero/Nano 33 IoT), RP2040/RP2350, ESP32.
  - Verified static RAM stays `< 500 bytes` on ATmega328P (uses only 345 bytes dynamic RAM including `Serial` and `Lux`).
  - Added Flash `PROGMEM` lookup for CRC-16 table on AVR (saving 512 bytes SRAM).
  - Added compile-time preprocessor guards for multi-platform headers and byte packing.
- [x] **1.3. Background Polling / Non-Blocking Tick Engine**
  - Implemented non-blocking `Lux.tick()` to handle serial buffer flushing without blocking the user's `loop()`.
  - Implemented automatic heartbeat generator (`setHeartbeatInterval`) and flush timer (`setFlushTimeout`).
  - Added hardware Timer interrupt hook (`Timer1` on AVR via `enableTimer1Interrupt`).
- [x] **1.4. Standard Library Coexistence Benchmarking**
  - Verified concurrent operation with `Wire` (I2C), `SPI`, and `Serial` in `06_CoexistenceTest.ino`.
  - Verified zero timer or resource conflict across standard Arduino libraries.

---

## Phase 2 — Apex Base Arduino Firmware ("Zero-Code" Node)

**Goal:** Provide a standard pre-compiled Arduino sketch that transforms any plugged-in Arduino into a zero-code smart node with instant browser control.

- [ ] **2.1. Standard Base Sketch Implementation**
  - **Path:** `examples/arduino-apex-base-node/arduino-apex-base-node.ino`
  - Implement standard command parser responding to Lux control frames (`0x0010–0x0030`).
- [ ] **2.2. Implement Core Hardware Control Handlers**
  - `LUX_CMD_PIN_MODE` (`0x0010`): Configure pin mode (INPUT, OUTPUT, INPUT_PULLUP, PWM).
  - `LUX_CMD_PIN_WRITE` (`0x0011`): Digital write (`HIGH`/`LOW`) and analog PWM duty cycle (`0–255`).
  - `LUX_CMD_PIN_READ` (`0x0012`): Immediate telemetry broadcast of analog ADC / digital pin state.
- [ ] **2.3. Heartbeat & Hardware Capability Descriptor**
  - Emit `LUX_SYM_HEARTBEAT` at 1 Hz with board uptime and free RAM.
  - Emit `LUX_SYM_DEVICE_INFO` upon connection with board signature (AVR Uno, Nano, ESP32, etc.) and pin capabilities.

---

## Phase 3 — Apex Studio Browser Flashing Engine (WebSerial)

**Goal:** Enable one-click browser-based flashing for Arduino (Optiboot/STK500) and ESP32 directly inside Apex Studio.

- [ ] **3.1. STK500v1 WebSerial Flasher for Classic Arduinos**
  - **Path:** [`apex-studio/src/components/node-flasher/`](file:///P:/Projects/Pyintel/Apex/apex-studio/src/components/node-flasher)
  - Integrate in-browser STK500v1 protocol (via `stk500-js` / `avrgirl-arduino`) using `navigator.serial`.
  - Support auto-reset via DTR/RTS toggle for Uno / Nano / Mega.
- [ ] **3.2. ESP32 WebSerial Flasher Integration**
  - Integrate `esptool-js` in [Apex Node Flasher](file:///P:/Projects/Pyintel/Apex/apex-studio/src/components/node-flasher) for ESP32, ESP32-S3, ESP32-C3, and ESP32-C6.
  - Provide binary preset selector (Base Lux Agent, MicroPython VM, Custom User Sketch).
- [ ] **3.3. RP2040 / RP2350 Web Flashing**
  - Support WebSerial 1200-baud reset trigger to jump into BOOTSEL mode + WebUSB Picotool.
- [ ] **3.4. Telemetry Quiesce & Flashing Handshake**
  - Implement `LUX_CMD_ENTER_FLASH` (`0x0020`): Host signals board to stop telemetry streaming, flush UART FIFOs, and prepare for serial bootloader takeover.

---

## Phase 4 — Apex Studio Live UI & Telemetry Bridge

**Goal:** Connect the browser WebSerial stream directly to the Apex Studio UI for real-time visualization and control.

- [ ] **4.1. `@pyintel/lux-web` WebSerial Demuxer**
  - **Path:** [`lux-web/src/`](file:///P:/Projects/Pyintel/Pyintel/pyintel-lux/lux-web)
  - Build TypeScript binary stream decoder using `DataView` over WebSerial `ReadableStream`.
  - Handle packet frame synchronization (`0x4C 0x58`), sequence loss checking, and CRC validation in web workers.
- [ ] **4.2. Apex Trace Studio (Oscilloscope & Grapher)**
  - Wire decoded Lux frames to Chart.js / Canvas in [Apex Studio](file:///P:/Projects/Pyintel/Apex/apex-studio).
  - Support plotting >1,000 samples/sec with zero browser UI lag.
- [ ] **4.3. Interactive Node Studio Bidirectional Canvas**
  - **Path:** [`apex-studio/src/components/node-studio/`](file:///P:/Projects/Pyintel/Apex/apex-studio/src/components/node-studio)
  - Link visual GPIO pin canvas to live Lux telemetry (pins glow green/red on hardware changes).
  - Clicking a pin in the browser immediately sends `LUX_CMD_PIN_WRITE` over WebSerial.

---

## Phase 5 — Protocol Hardening & Specification Freeze

**Goal:** Solidify the wire specification and host tooling based on empirical test results.

- [ ] **5.1. Update & Freeze `spec/wire-format.md`**
  - Document system control symbols (`0x0010–0x00FF`) in [`spec/wire-format.md`](file:///P:/Projects/Pyintel/Pyintel/pyintel-lux/spec/wire-format.md).
- [ ] **5.2. Variable Payload CRC Review**
  - Evaluate whether payload bytes in `LUX_TYPE_BYTES` require full frame CRC or dual-stage CRC for noisy industrial UART.
- [ ] **5.3. 64-Bit Monotonic Epoch Unwrapping**
  - Implement 32-bit microsecond rollover detection in `@pyintel/lux-web` and `luxd` to ensure uninterrupted multi-day timestamps.
- [ ] **5.4. Symbol Dictionary Build Hash & Version Pinning**
  - Include a 16-bit dictionary hash in `LUX_SYM_HEARTBEAT` to guarantee host `symbols.json` matches on-chip firmware.

---

## Phase 6 — Advanced Multi-Tier Execution (Future Expansion)

- [ ] **6.1. MicroPython / Wasm3 Stream Bridge**
  - Route MicroPython REPL or Wasm3 bytecode over `LUX_STREAM_REPL_IN` / `LUX_STREAM_REPL_OUT` for interactive live coding.
- [ ] **6.2. Dual-Partition Recovery Manager (ESP32 & RP2350)**
  - Implement partition switching so the Lux Base Agent always survives bad user sketches.
