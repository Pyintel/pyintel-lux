# Contributing to Pyintel Lux

Thank you for your interest in contributing to **Pyintel Lux**! We welcome contributions from developers, researchers, and hardware enthusiasts of all experience levels.

---

## 📜 Code of Conduct

By participating in this project, you agree to abide by our [Code of Conduct](CODE_OF_CONDUCT.md). Please read it to understand our community standards.

---

## 🛠️ How Can You Contribute?

### 1. Reporting Bugs
If you find a bug, missing transport feature, or unexpected behavior:
1. Search the existing [GitHub Issues](https://github.com/Pyintel/pyintel-lux/issues) to ensure it hasn't already been reported.
2. Open a new issue using the **Bug Report** template.
3. Provide clear reproduction steps, board type (ESP32, RP2040, STM32, AVR), transport used, and compiler/toolchain details.

### 2. Suggesting Enhancements
Have an idea for a new transport bearer (e.g. LoRa, CAN-FD), SDK feature, or host tool enhancement?
1. Open an issue using the **Feature Request** template.
2. Explain the use case, architecture impact, and why it benefits the Pyintel Lux standard.

### 3. Submitting Pull Requests (PRs)
1. Fork the repository and create a descriptive feature branch (`git checkout -b feature/can-bus-transport`).
2. Follow the codebase style and conventions:
   - **`no_std` C/C++ Engine:** Absolutely **zero runtime dynamic heap allocation (`malloc`/`free`)**. Static memory or stack allocation only (`< 1 KB` RAM constraint).
   - **Performance First:** Keep emit execution time sub-microsecond and frame size strictly compliant with the [14-Byte Wire Specification](https://github.com/Pyintel/pyintel-lux/wiki/Wire-Frame-Specification).
3. Ensure all tests pass. If working on SDK features, add test coverage or example sketches where applicable.
4. Commit your changes using clear commit messages (e.g., `feat(transport): add CAN-FD driver support`).
5. Open a Pull Request against the `master` branch.

---

## 🏗️ Local Development & Testing

- For **Arduino / C++ SDK**, test sketches against hardware boards or PlatformIO build environments:
  ```bash
  cd build/esp32s3_bridge
  pio run
  ```
- For **Rust tools (`luxd`)**, verify using Cargo:
  ```bash
  cargo test
  ```

---

## 📬 Contact & Questions

If you have questions about the wire standard or architecture, feel free to open a GitHub Discussion or reach out to **dev@pyintel.cc**.
