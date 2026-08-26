/**
 * Pyintel Lux — Universal Multi-Bearer Mesh & Dynamic Transport Engine
 * 
 * Provides zero-copy, sub-kilobyte binary mesh routing and transparent transport
 * fallback across all edge physical and wireless communication bearers.
 */

#ifndef LUX_MESH_ENGINE_H
#define LUX_MESH_ENGINE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Protocol Constants ───────────────────────────────────────────── */
#define LUX_MESH_SYNC_0             0x4D  /* 'M' */
#define LUX_MESH_SYNC_1             0x58  /* 'X' */
#define LUX_FRAME_SYNC_0            0x4C  /* 'L' */
#define LUX_FRAME_SYNC_1            0x58  /* 'X' */

#define LUX_MESH_HEADER_SIZE        12
#define LUX_FRAME_HEADER_SIZE       14
#define LUX_MAX_PAYLOAD_SIZE        128
#define LUX_MAX_PACKET_SIZE         (LUX_MESH_HEADER_SIZE + LUX_FRAME_HEADER_SIZE + LUX_MAX_PAYLOAD_SIZE)

#define LUX_NODE_BROADCAST          0xFFFF
#define LUX_NODE_GATEWAY            0x0000
#define LUX_DEFAULT_HOP_LIMIT       4
#define LUX_MAX_PEERS               16
#define LUX_MAX_HANDLERS            16
#define LUX_DEDUP_CACHE_SIZE        32
#define LUX_PEER_TIMEOUT_MS         4000
#define LUX_MIN_DISCOVERY_MS        5000  /* Minimum stability window before locking */
#define LUX_AUTO_HEAL_TIMEOUT_MS    8000  /* Auto-heal if all peers lost for > 8s */
#define LUX_KNOCK_WINDOW_MS         6000  /* Temporary rendezvous window on knock */

/* ── Transport Bearer Identifiers ─────────────────────────────────── */
typedef enum {
    LUX_BEARER_SERIAL    = 0,  /* Wired UART / USB CDC */
    LUX_BEARER_NRF_RADIO = 1,  /* Nordic 2.4GHz Hardware Transceiver (micro:bit/CLUE) */
    LUX_BEARER_BLE       = 2,  /* Bluetooth Low Energy (Adv/NUS/Mesh) */
    LUX_BEARER_ESPNOW    = 3,  /* ESP-NOW 2.4GHz Peer-to-Peer */
    LUX_BEARER_WIFI_UDP  = 4,  /* Wi-Fi UDP Socket Broadcast */
    LUX_BEARER_LORA      = 5,  /* Semtech SX127x/SX126x Sub-GHz LoRa */
    LUX_BEARER_CAN       = 6,  /* CAN 2.0B / TWAI Industrial Differential Bus */
    LUX_BEARER_COUNT     = 7,
    LUX_BEARER_UNKNOWN   = 0xFF
} lux_bearer_id_t;

/* ── Transport Capability Bitmasks ────────────────────────────────── */
#define LUX_CAP_SERIAL      (1 << LUX_BEARER_SERIAL)
#define LUX_CAP_NRF_RADIO   (1 << LUX_BEARER_NRF_RADIO)
#define LUX_CAP_BLE         (1 << LUX_BEARER_BLE)
#define LUX_CAP_ESPNOW      (1 << LUX_BEARER_ESPNOW)
#define LUX_CAP_WIFI_UDP    (1 << LUX_BEARER_WIFI_UDP)
#define LUX_CAP_LORA        (1 << LUX_BEARER_LORA)
#define LUX_CAP_CAN         (1 << LUX_BEARER_CAN)

/* ── 12-Byte Binary Mesh Wire Envelope ────────────────────────────── */
#pragma pack(push, 1)
typedef struct {
    uint8_t  sync[2];        /* 0x4D, 0x58 ("MX") */
    uint32_t net_hash;       /* CRC-32 of Network UUID */
    uint16_t src_node;       /* Originating Node ID (e.g. 0x310F) */
    uint16_t dst_node;       /* Destination Node ID (0xFFFF = Broadcast) */
    uint8_t  hop_count;      /* Decremented at each relay */
    uint8_t  flags;          /* Flags: Bit 0 = Encrypted, Bit 1 = AckReq */
} lux_mesh_envelope_t;

/* ── 14-Byte Inner Lux Telemetry Frame ────────────────────────────── */
typedef struct {
    uint8_t  sync[2];        /* 0x4C, 0x58 ("LX") */
    uint16_t seq_num;        /* Monotonic sequence counter */
    uint16_t symbol_id;      /* Dictionary Symbol ID (e.g. 0x0330) */
    uint32_t timestamp_us;   /* Hardware microsecond timestamp */
    uint8_t  payload_type;   /* 1=U8, 2=U16, 3=U32, 4=I32, 5=F32, 6=Bytes */
    uint8_t  payload_len;    /* Payload byte count [0..128] */
    uint16_t crc16;          /* CRC-16-CCITT over header bytes [0..11] */
} lux_frame_header_t;
#pragma pack(pop)

/* ── Abstract Transport Bearer Driver Interface ───────────────────── */
typedef struct lux_transport_driver {
    lux_bearer_id_t id;
    const char     *name;              /* e.g. "Serial", "nRF-Radio", "BLE", "ESP-NOW" */
    uint8_t         priority;          /* Lower number = preferred transport (1=Fastest) */

    bool (*init)(void *config);
    bool (*is_available)(void);
    int  (*send)(const uint8_t *buf, size_t len, uint16_t dst_node);
    int  (*broadcast)(const uint8_t *buf, size_t len);
    int  (*receive)(uint8_t *buf, size_t max_len, uint16_t *src_node);
    int8_t (*get_rssi)(void);          /* Link quality RSSI in dBm (0 for wired) */
    void (*deinit)(void);
} lux_transport_driver_t;

/* ── Discovered Peer Node Descriptor ──────────────────────────────── */
typedef struct {
    uint16_t node_id;
    uint8_t  capabilities;             /* Bitmask of active transports */
    lux_bearer_id_t primary_bearer;    /* Best detected transport route */
    int8_t   rssi;                     /* Last recorded RSSI */
    uint32_t last_seen_ms;             /* Millis timestamp of last packet */
    uint32_t uptime_ms;                /* Remote node reported uptime */
    uint32_t rx_count;                 /* Total packets received */
    uint32_t tx_count;                 /* Total packets sent to peer */
    bool     alive;                    /* True if active within timeout */
} lux_peer_node_t;

/* ── Packet Deduplication History Entry ────────────────────────────── */
typedef struct {
    uint16_t src_node;
    uint16_t seq_num;
    uint32_t timestamp_ms;
} lux_dedup_entry_t;

/* ── Callback Signatures ──────────────────────────────────────────── */
typedef void (*lux_mesh_rx_callback_t)(uint16_t src_node, uint16_t symbol, const uint8_t *payload, uint8_t len);

/* ── Unified Mesh Engine Class / Context ──────────────────────────── */
typedef struct {
    uint32_t                net_hash;
    uint16_t                node_id;
    uint16_t                seq_counter;
    bool                    is_initialized;
    bool                    relay_enabled;
    bool                    border_locked;
    bool                    auto_heal_enabled;
    uint32_t                auto_heal_timeout_ms;
    uint32_t                boot_time_ms;
    uint32_t                last_peer_discovered_ms;
    uint32_t                last_peer_packet_ms;
    uint32_t                auto_seal_deadline_ms;

    lux_transport_driver_t *drivers[LUX_BEARER_COUNT];
    uint8_t                 driver_count;

    lux_peer_node_t         peers[LUX_MAX_PEERS];
    uint8_t                 peer_count;

    lux_dedup_entry_t       dedup_cache[LUX_DEDUP_CACHE_SIZE];
    uint8_t                 dedup_head;

    lux_mesh_rx_callback_t  handlers[LUX_MAX_HANDLERS];
    uint16_t                handler_symbols[LUX_MAX_HANDLERS];
    uint8_t                 handler_count;
    lux_mesh_rx_callback_t  catchall_handler;
} lux_mesh_engine_t;

/* ── Public Mesh Engine API ───────────────────────────────────────── */
void lux_mesh_init(lux_mesh_engine_t *mesh, const char *network_uuid, uint16_t node_id);
bool lux_mesh_register_driver(lux_mesh_engine_t *mesh, lux_transport_driver_t *driver, void *config);
void lux_mesh_set_relay(lux_mesh_engine_t *mesh, bool enable);

/* ── Border Lock & Battery Saver API ──────────────────────────────── */
void lux_mesh_init_border(lux_mesh_engine_t *mesh);
void lux_mesh_lock_topology(lux_mesh_engine_t *mesh);
void lux_mesh_unlock_border(lux_mesh_engine_t *mesh);
bool lux_mesh_is_border_locked(lux_mesh_engine_t *mesh);

/* ── Auto-Healing & Rendezvous Knock API ──────────────────────────── */
void lux_mesh_knock(lux_mesh_engine_t *mesh);
void lux_mesh_set_auto_heal(lux_mesh_engine_t *mesh, bool enable, uint32_t timeout_ms);

int  lux_mesh_broadcast(lux_mesh_engine_t *mesh, uint16_t symbol, uint8_t type, const uint8_t *payload, uint8_t len);
int  lux_mesh_send_to(lux_mesh_engine_t *mesh, uint16_t dst_node, uint16_t symbol, uint8_t type, const uint8_t *payload, uint8_t len);

void lux_mesh_on_message(lux_mesh_engine_t *mesh, uint16_t symbol, lux_mesh_rx_callback_t callback);
void lux_mesh_on_all_messages(lux_mesh_engine_t *mesh, lux_mesh_rx_callback_t callback);

void lux_mesh_poll(lux_mesh_engine_t *mesh, uint32_t current_millis);
void lux_mesh_prune_peers(lux_mesh_engine_t *mesh, uint32_t current_millis);

/* Built-in standard driver instances */
extern lux_transport_driver_t lux_driver_serial;
extern lux_transport_driver_t lux_driver_nrf_radio;
extern lux_transport_driver_t lux_driver_ble;
extern lux_transport_driver_t lux_driver_espnow;
extern lux_transport_driver_t lux_driver_wifi_udp;
extern lux_transport_driver_t lux_driver_lora;
extern lux_transport_driver_t lux_driver_can;

#ifdef __cplusplus
}
#endif

#endif /* LUX_MESH_ENGINE_H */
