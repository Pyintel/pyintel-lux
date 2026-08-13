# Phase 3 — ESP-NOW P2P Mesh

**Status:** Not started (complete Phase 2 first)  
**Board count:** 2 (Board A = Emitter, Board B = Receiver/Relay)  
**Transport:** ESP-NOW (connectionless 802.11, no router)

## Goal
Board A emits Lux frames directly to Board B over ESP-NOW. Board B relays them to the PC via UART. The host decoder from Phase 1 works unchanged — same frame format, different transport path.

## Key ESP-NOW facts
- No Wi-Fi AP required — works completely offline
- Max payload: 250 bytes (Lux 12-byte header + payload fits easily)
- ~1ms typical latency
- Pairing uses MAC addresses — you need to hardcode Board A's MAC into Board B's firmware

## Steps
1. Find Board A's MAC: flash a minimal sketch that prints `esp_wifi_get_mac()` over serial.
2. Flash `emitter/` to Board A with Board B's MAC hardcoded as peer.
3. Flash `receiver/` to Board B with Board A's MAC hardcoded as peer.
4. Board B forwards received Lux frames out UART → run `host/decode.py` from Phase 1.

## Notes
<!-- Add your observations here -->
