# Phase 2 — UDP Stream

**Status:** Not started (complete Phase 1 first)  
**Board count:** 1 (Board A) + PC on same Wi-Fi  
**Transport:** UDP → port 4210

## Goal
Swap Phase 1's UART `lux_write_fn` callback for a UDP `sendto()`. Frame assembly code is unchanged — only the transport layer differs. This proves transport independence.

## Steps
1. Copy Phase 1 firmware, replace `uart_write` callback with `udp_write`.
2. Set Wi-Fi credentials in `sdkconfig` via `idf.py menuconfig`.
3. Run `host/udp_decode.py --host 0.0.0.0 --port 4210`.

## Notes
<!-- Add your observations here -->
