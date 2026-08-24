/**
 * Pyintel Lux — Mesh Engine Header
 * Handles UUID-based mesh networking, node identification, peer routing tables,
 * promiscuous debug sniffing, and multi-transport message dispatch.
 */

#ifndef LUX_MESH_H
#define LUX_MESH_H

#include <Arduino.h>
#include "lux.h"
#include "transport.h"

#define LUX_MESH_SYNC_0         0x4D   /* 'M' */
#define LUX_MESH_SYNC_1         0x58   /* 'X' */
#define LUX_MESH_HEADER_SIZE    12

#define LUX_NODE_BROADCAST      0xFFFF
#define LUX_NODE_INVALID        0x0000
#define LUX_MAX_PEERS           16
#define LUX_MAX_HANDLERS        8
#define LUX_DEFAULT_HOP_LIMIT   4
#define LUX_PEER_TIMEOUT_MS     5000

#if defined(__GNUC__) || defined(__clang__)
#define LUX_MESH_PACKED __attribute__((packed))
#elif defined(_MSC_VER)
#define LUX_MESH_PACKED
#pragma pack(push, 1)
#else
#define LUX_MESH_PACKED
#endif

/**
 * 12-Byte Mesh Header Envelope (prepended to inner Lux frame)
 */
typedef struct LUX_MESH_PACKED {
    uint8_t  sync[2];        /* 0x4D 0x58 ('MX')                  */
    uint32_t net_hash;       /* CRC-32 of network UUID string     */
    uint16_t src_node;       /* Sender's unique 16-bit Node ID    */
    uint16_t dst_node;       /* 0xFFFF for broadcast, or unicast  */
    uint8_t  hop_count;      /* TTL / hop limit (decremented)     */
    uint8_t  flags;          /* [7:4]=flags, [3:0]=transport_id   */
} lux_mesh_envelope_t;

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

/** Discovered Peer Node Metadata */
struct LuxPeer {
    uint16_t node_id;              /* Peer's unique 16-bit ID */
    uint8_t  transports;           /* Bitmask of active transports */
    int8_t   rssi;                 /* Best signal strength (-dBm), 0 = wired */
    uint32_t last_seen_ms;         /* millis() timestamp of last frame */
    uint32_t uptime_ms;            /* Peer's reported uptime */
    uint16_t rx_count;             /* Total frames received from peer */
    uint16_t tx_count;             /* Total frames sent to peer */
    bool     alive;                /* true if heard from within 5 seconds */
};

/** Symbol Message Handler Callback */
typedef void (*LuxMessageHandler)(uint16_t from_node, uint16_t symbol, const uint8_t *payload, uint8_t len);

/** Handler Slot Entry */
typedef struct {
    uint16_t          symbol_id;
    LuxMessageHandler handler;
} lux_handler_slot_t;

/** Debug Verbosity */
typedef enum {
    LUX_DEBUG_OFF     = 0,
    LUX_DEBUG_ERRORS  = 1,
    LUX_DEBUG_TRAFFIC = 2,
    LUX_DEBUG_FULL    = 3,
} lux_debug_level_t;

/**
 * Compute CRC-32 of a string (used for Network UUID hash)
 */
uint32_t lux_crc32(const char *str);

/**
 * Generate a unique 16-bit Node ID from hardware identifiers
 */
uint16_t lux_generate_node_id(void);

#endif /* LUX_MESH_H */
