# Pyintel Lux — Network Topology & Multi-Bearer Architecture Specification

> **Core Philosophy:** Pure Peer-to-Peer (P2P) Sovereign Swarm — Zero Master/Slave Hierarchy, Protocol-Agnostic Multi-Bearer Bridging.

---

## 📜 Fundamental Principles of Lux Mesh

### Rule 1: No Master / Slave Hierarchy
* **Pure Symmetric Peer-to-Peer (P2P):** There are **no masters, no slaves, no central brokers, and no single points of failure**.
* Every node possesses a unique 16-bit Node ID (`0x0001–0xFFFE`).
* Every node is sovereign and autonomous — any node can broadcast telemetry, listen to events, send targeted control commands, or act as a transparent multi-hop packet relay.

### Rule 2: Network Membership Bound Exclusively to Network UUID
* Network isolation and membership is strictly governed by the **Network UUID** (e.g. `"f47ac10b-58cc-4372-a567-0e02b2c3d479"`).
* The 32-bit CRC-32 hash of the UUID (`net_hash`) is embedded in every 12-byte mesh envelope (`lux_mesh_envelope_t`).
* **Any node with the same Network UUID automatically joins the same network**, discovering all other nodes without manual pairing or provisioning.

### Rule 3: Protocol-Agnostic Multi-Bearer Mesh Bridging
* **Nodes do NOT need to use the same physical communication protocol.**
* Multi-radio nodes (such as ESP32, Adafruit CLUE, or gateway bridges) dynamically translate and bridge packets across heterogeneous physical media:

```
┌────────────────────────┐                               ┌────────────────────────┐
│     BBC micro:bit      │                               │       ESP32 Node       │
│     (Nordic 2.4GHz)    │                               │       (ESP-NOW)        │
└───────────┬────────────┘                               └───────────┬────────────┘
            │                                                        │
            │ (Nordic RF)                                            │ (ESP-NOW)
            ▼                                                        ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                           PYINTEL LUX UNIFIED MESH FABRIC                       │
│                     (Bound by Common 128-Bit Network UUID)                      │
└─────────────────────────────────────────────────────────────────────────────────┘
            ▲                                                        ▲
            │ (BLE / Nordic RF)                                      │ (Wi-Fi UDP / Serial)
            │                                                        │
┌───────────┴────────────┐                               ┌───────────┴────────────┐
│     Adafruit CLUE      │                               │   PC / Web Dashboard   │
│     (BLE / nRF52)      │                               │  (USB Serial / UDP)    │
└────────────────────────┘                               └────────────────────────┘
```

---

## 🔄 Multi-Bearer Relay Behavior
1. **Ingest:** A node receives a valid packet on any interface (Serial, Nordic RF, BLE, ESP-NOW, Wi-Fi UDP, LoRa, or CAN).
2. **UUID Match:** If `env->net_hash == local_net_hash`, the frame belongs to the swarm.
3. **Deduplication:** The 32-entry circular deduplication cache verifies the packet has not been processed recently.
4. **Local Dispatch:** If `dst_node == local_node_id` or `dst_node == 0xFFFF` (Broadcast), local symbol callbacks are invoked.
5. **Relay (Flood):** If `hop_count > 1`, `hop_count` is decremented by 1 and the exact same binary frame is re-emitted across **all other active transport bearers**.
