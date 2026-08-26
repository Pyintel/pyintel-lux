/**
 * Pyintel Lux — Universal Multi-Bearer Mesh & Dynamic Transport Engine Implementation
 * 
 * Implements transparent multi-hop routing, packet deduplication, peer discovery,
 * adaptive transport fallback, and bulletproof topology border management:
 *   1. Auto-Healing Watchdog (recovers automatically if link breaks)
 *   2. Rendezvous Knock Protocol (allows late-boot stragglers to join instantly)
 *   3. Stability Guard (prevents premature network freeze)
 */

#include "../include/lux_mesh.h"
#include <string.h>

/* ── Standard CRC-32 (IEEE 802.3) ─────────────────────────────────── */
static uint32_t lux_mesh_crc32(const char *str) {
    if (!str) return 0;
    uint32_t crc = 0xFFFFFFFF;
    while (*str) {
        uint8_t byte = (uint8_t)*str++;
        crc ^= byte;
        for (uint8_t i = 0; i < 8; i++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(int32_t)(crc & 1)));
        }
    }
    return ~crc;
}

/* ── CRC-16-CCITT (Polynomial 0x1021, Init 0xFFFF) ─────────────────── */
static uint16_t lux_mesh_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/* ── Mesh Engine Initialization ───────────────────────────────────── */
void lux_mesh_init(lux_mesh_engine_t *mesh, const char *network_uuid, uint16_t node_id) {
    if (!mesh) return;
    memset(mesh, 0, sizeof(lux_mesh_engine_t));
    mesh->net_hash = lux_mesh_crc32(network_uuid ? network_uuid : "pyintel-lux-default");
    mesh->node_id = node_id ? node_id : 0x0042;
    mesh->seq_counter = 0;
    mesh->relay_enabled = true;
    mesh->border_locked = false;
    mesh->auto_heal_enabled = true;
    mesh->auto_heal_timeout_ms = LUX_AUTO_HEAL_TIMEOUT_MS;
    mesh->boot_time_ms = 0;
    mesh->last_peer_discovered_ms = 0;
    mesh->last_peer_packet_ms = 0;
    mesh->auto_seal_deadline_ms = 0;
    mesh->is_initialized = true;
}

/* ── Driver Registration ──────────────────────────────────────────── */
bool lux_mesh_register_driver(lux_mesh_engine_t *mesh, lux_transport_driver_t *driver, void *config) {
    if (!mesh || !driver || mesh->driver_count >= LUX_BEARER_COUNT) return false;

    if (driver->init && !driver->init(config)) {
        return false;
    }

    mesh->drivers[mesh->driver_count++] = driver;
    return true;
}

void lux_mesh_set_relay(lux_mesh_engine_t *mesh, bool enable) {
    if (mesh) mesh->relay_enabled = enable;
}

/* ── Packet Deduplication Cache Check ──────────────────────────────── */
static bool is_duplicate(lux_mesh_engine_t *mesh, uint16_t src, uint16_t seq, uint32_t now) {
    for (uint8_t i = 0; i < LUX_DEDUP_CACHE_SIZE; i++) {
        if (mesh->dedup_cache[i].src_node == src && mesh->dedup_cache[i].seq_num == seq) {
            // Found duplicate packet within 5 seconds
            if (now - mesh->dedup_cache[i].timestamp_ms < 5000) {
                return true;
            }
        }
    }
    // Record into ring buffer
    uint8_t idx = mesh->dedup_head;
    mesh->dedup_cache[idx].src_node = src;
    mesh->dedup_cache[idx].seq_num = seq;
    mesh->dedup_cache[idx].timestamp_ms = now;
    mesh->dedup_head = (idx + 1) % LUX_DEDUP_CACHE_SIZE;
    return false;
}

/* ── Peer Table Management ────────────────────────────────────────── */
static void update_peer(lux_mesh_engine_t *mesh, uint16_t node_id, lux_bearer_id_t bearer, int8_t rssi, uint32_t uptime_ms, uint32_t now) {
    if (node_id == 0x0000 || node_id == mesh->node_id) return;

    mesh->last_peer_packet_ms = now;

    for (uint8_t i = 0; i < mesh->peer_count; i++) {
        if (mesh->peers[i].node_id == node_id) {
            mesh->peers[i].capabilities |= (1 << bearer);
            mesh->peers[i].primary_bearer = bearer;
            mesh->peers[i].rssi = rssi;
            mesh->peers[i].last_seen_ms = now;
            if (uptime_ms > 0) mesh->peers[i].uptime_ms = uptime_ms;
            mesh->peers[i].rx_count++;
            mesh->peers[i].alive = true;
            return;
        }
    }

    if (mesh->peer_count < LUX_MAX_PEERS) {
        lux_peer_node_t *p = &mesh->peers[mesh->peer_count++];
        p->node_id = node_id;
        p->capabilities = (1 << bearer);
        p->primary_bearer = bearer;
        p->rssi = rssi;
        p->last_seen_ms = now;
        p->uptime_ms = uptime_ms;
        p->rx_count = 1;
        p->tx_count = 0;
        p->alive = true;
        mesh->last_peer_discovered_ms = now;
    }
}

/* ── Frame Assembly and Multi-Bearer Emission ─────────────────────── */
static int emit_frame(lux_mesh_engine_t *mesh, uint16_t dst_node, uint16_t symbol, uint8_t type, const uint8_t *payload, uint8_t len) {
    if (!mesh || !mesh->is_initialized) return -1;
    if (len > LUX_MAX_PAYLOAD_SIZE) len = LUX_MAX_PAYLOAD_SIZE;

    uint8_t packet[LUX_MAX_PACKET_SIZE];

    // 1. Pack 12-Byte Mesh Wire Envelope
    lux_mesh_envelope_t *env = (lux_mesh_envelope_t *)packet;
    env->sync[0]    = LUX_MESH_SYNC_0;
    env->sync[1]    = LUX_MESH_SYNC_1;
    env->net_hash   = mesh->net_hash;
    env->src_node   = mesh->node_id;
    env->dst_node   = dst_node;
    env->hop_count  = LUX_DEFAULT_HOP_LIMIT;
    env->flags      = 0;

    // 2. Pack 14-Byte Inner Lux Telemetry Header
    lux_frame_header_t *inner = (lux_frame_header_t *)(packet + LUX_MESH_HEADER_SIZE);
    inner->sync[0]       = LUX_FRAME_SYNC_0;
    inner->sync[1]       = LUX_FRAME_SYNC_1;
    inner->seq_num       = mesh->seq_counter++;
    inner->symbol_id     = symbol;
    inner->timestamp_us  = 0;
    inner->payload_type  = type;
    inner->payload_len   = len;
    inner->crc16         = lux_mesh_crc16((const uint8_t *)inner, 12);

    // 3. Copy Payload
    if (payload && len > 0) {
        memcpy(packet + LUX_MESH_HEADER_SIZE + LUX_FRAME_HEADER_SIZE, payload, len);
    }

    size_t total_len = LUX_MESH_HEADER_SIZE + LUX_FRAME_HEADER_SIZE + len;

    // 4. Emit through all registered and active transport drivers
    int sent_count = 0;
    for (uint8_t i = 0; i < mesh->driver_count; i++) {
        lux_transport_driver_t *drv = mesh->drivers[i];
        if (drv && drv->is_available && drv->is_available()) {
            if (dst_node == LUX_NODE_BROADCAST) {
                if (drv->broadcast && drv->broadcast(packet, total_len) >= 0) {
                    sent_count++;
                }
            } else {
                if (drv->send && drv->send(packet, total_len, dst_node) >= 0) {
                    sent_count++;
                }
            }
        }
    }
    return sent_count;
}

int lux_mesh_broadcast(lux_mesh_engine_t *mesh, uint16_t symbol, uint8_t type, const uint8_t *payload, uint8_t len) {
    return emit_frame(mesh, LUX_NODE_BROADCAST, symbol, type, payload, len);
}

int lux_mesh_send_to(lux_mesh_engine_t *mesh, uint16_t dst_node, uint16_t symbol, uint8_t type, const uint8_t *payload, uint8_t len) {
    return emit_frame(mesh, dst_node, symbol, type, payload, len);
}

/* ── Border Lock & Battery Saver Implementation ──────────────────── */
void lux_mesh_lock_topology(lux_mesh_engine_t *mesh) {
    if (!mesh || mesh->border_locked) return;
    mesh->border_locked = true;

    // Determine which bearer drivers have active peers
    uint8_t active_mask = 0;
    for (uint8_t p = 0; p < mesh->peer_count; p++) {
        if (mesh->peers[p].alive) {
            active_mask |= mesh->peers[p].capabilities;
        }
    }
    active_mask |= LUX_CAP_SERIAL; // Always keep host serial interface

    // Power down / sleep all unused transport drivers
    for (uint8_t i = 0; i < mesh->driver_count; i++) {
        lux_transport_driver_t *drv = mesh->drivers[i];
        if (drv && !(active_mask & (1 << drv->id))) {
            if (drv->deinit) {
                drv->deinit();
            }
        }
    }
}

void lux_mesh_init_border(lux_mesh_engine_t *mesh) {
    if (!mesh) return;
    lux_mesh_lock_topology(mesh);
    uint8_t flags = 0x01;
    lux_mesh_broadcast(mesh, 0x0007, 1, &flags, 1); // LUX_SYM_BORDER_LOCK
}

void lux_mesh_unlock_border(lux_mesh_engine_t *mesh) {
    if (!mesh || !mesh->border_locked) return;
    mesh->border_locked = false;
    mesh->auto_seal_deadline_ms = 0;

    // Re-initialize all transport drivers back to full discovery mode
    for (uint8_t i = 0; i < mesh->driver_count; i++) {
        lux_transport_driver_t *drv = mesh->drivers[i];
        if (drv && drv->init) {
            drv->init(NULL);
        }
    }

    uint8_t flags = 0x00;
    lux_mesh_broadcast(mesh, 0x0008, 1, &flags, 1); // LUX_SYM_BORDER_UNLOCK
}

bool lux_mesh_is_border_locked(lux_mesh_engine_t *mesh) {
    return mesh ? mesh->border_locked : false;
}

void lux_mesh_knock(lux_mesh_engine_t *mesh) {
    if (!mesh) return;
    // Ensure all drivers are briefly enabled to emit the knock
    for (uint8_t i = 0; i < mesh->driver_count; i++) {
        if (mesh->drivers[i] && mesh->drivers[i]->init) {
            mesh->drivers[i]->init(NULL);
        }
    }
    uint8_t caps = 0xFF;
    lux_mesh_broadcast(mesh, 0x0009, 1, &caps, 1); // LUX_SYM_BORDER_KNOCK
}

void lux_mesh_set_auto_heal(lux_mesh_engine_t *mesh, bool enable, uint32_t timeout_ms) {
    if (!mesh) return;
    mesh->auto_heal_enabled = enable;
    mesh->auto_heal_timeout_ms = timeout_ms ? timeout_ms : LUX_AUTO_HEAL_TIMEOUT_MS;
}

/* ── Callback Registration ────────────────────────────────────────── */
void lux_mesh_on_message(lux_mesh_engine_t *mesh, uint16_t symbol, lux_mesh_rx_callback_t callback) {
    if (!mesh || !callback || mesh->handler_count >= LUX_MAX_HANDLERS) return;
    mesh->handler_symbols[mesh->handler_count] = symbol;
    mesh->handlers[mesh->handler_count] = callback;
    mesh->handler_count++;
}

void lux_mesh_on_all_messages(lux_mesh_engine_t *mesh, lux_mesh_rx_callback_t callback) {
    if (mesh) mesh->catchall_handler = callback;
}

/* ── Ingestion, Auto-Healing & Rendezvous Polling Engine ──────────── */
void lux_mesh_poll(lux_mesh_engine_t *mesh, uint32_t current_millis) {
    if (!mesh || !mesh->is_initialized) return;

    if (mesh->boot_time_ms == 0) {
        mesh->boot_time_ms = current_millis;
        mesh->last_peer_packet_ms = current_millis;
    }

    // 1. Auto-Seal Rendezvous Window Check
    if (mesh->auto_seal_deadline_ms > 0 && current_millis >= mesh->auto_seal_deadline_ms) {
        mesh->auto_seal_deadline_ms = 0;
        lux_mesh_lock_topology(mesh);
    }

    // 2. Auto-Healing Watchdog Check: If locked and all peers lost for > timeout, heal network
    if (mesh->border_locked && mesh->auto_heal_enabled && mesh->peer_count > 0) {
        if (current_millis - mesh->last_peer_packet_ms > mesh->auto_heal_timeout_ms) {
            lux_mesh_unlock_border(mesh);
            lux_mesh_knock(mesh); // Broadcast knock to wake any asleep neighbors
        }
    }

    // 3. Multi-Driver Ingest Loop
    uint8_t rx_buf[LUX_MAX_PACKET_SIZE];

    for (uint8_t i = 0; i < mesh->driver_count; i++) {
        lux_transport_driver_t *drv = mesh->drivers[i];
        if (!drv || !drv->is_available || !drv->is_available() || !drv->receive) continue;

        uint16_t src_node = 0;
        int received_bytes = drv->receive(rx_buf, sizeof(rx_buf), &src_node);

        if (received_bytes >= (int)(LUX_MESH_HEADER_SIZE + LUX_FRAME_HEADER_SIZE)) {
            // Verify Mesh sync "MX"
            if (rx_buf[0] != LUX_MESH_SYNC_0 || rx_buf[1] != LUX_MESH_SYNC_1) continue;

            lux_mesh_envelope_t *env = (lux_mesh_envelope_t *)rx_buf;

            // Verify Network UUID hash match
            if (env->net_hash != mesh->net_hash) continue;

            // Ignore our own looped back broadcasts
            if (env->src_node == mesh->node_id) continue;

            lux_frame_header_t *inner = (lux_frame_header_t *)(rx_buf + LUX_MESH_HEADER_SIZE);

            // Verify inner CRC-16
            uint16_t expected_crc = lux_mesh_crc16((const uint8_t *)inner, 12);
            if (expected_crc != inner->crc16) continue;

            // Check deduplication cache
            if (is_duplicate(mesh, env->src_node, inner->seq_num, current_millis)) {
                continue;
            }

            uint8_t *payload_ptr = rx_buf + LUX_MESH_HEADER_SIZE + LUX_FRAME_HEADER_SIZE;
            uint8_t payload_len = inner->payload_len;

            // Update peer table & last seen packet
            int8_t rssi = drv->get_rssi ? drv->get_rssi() : 0;
            update_peer(mesh, env->src_node, drv->id, rssi, 0, current_millis);

            // Special System Handlers
            if (inner->symbol_id == 0x0007) { // LUX_SYM_BORDER_LOCK
                lux_mesh_lock_topology(mesh);
            } else if (inner->symbol_id == 0x0008) { // LUX_SYM_BORDER_UNLOCK
                lux_mesh_unlock_border(mesh);
            } else if (inner->symbol_id == 0x0009) { // LUX_SYM_BORDER_KNOCK (Straggler Join Knock)
                if (mesh->border_locked) {
                    // Temporarily unlock all radios for a 6-second rendezvous window to admit straggler
                    lux_mesh_unlock_border(mesh);
                    mesh->auto_seal_deadline_ms = current_millis + LUX_KNOCK_WINDOW_MS;
                }
            }

            // Deliver to registered callbacks
            if (env->dst_node == mesh->node_id || env->dst_node == LUX_NODE_BROADCAST) {
                bool handled = false;
                for (uint8_t h = 0; h < mesh->handler_count; h++) {
                    if (mesh->handler_symbols[h] == inner->symbol_id && mesh->handlers[h]) {
                        mesh->handlers[h](env->src_node, inner->symbol_id, payload_ptr, payload_len);
                        handled = true;
                    }
                }
                if (!handled && mesh->catchall_handler) {
                    mesh->catchall_handler(env->src_node, inner->symbol_id, payload_ptr, payload_len);
                }
            }

            // Multi-Hop Flood Relay (Rebroadcast across alternate bearers if hop count remains)
            if (mesh->relay_enabled && env->hop_count > 1) {
                env->hop_count--;
                for (uint8_t j = 0; j < mesh->driver_count; j++) {
                    if (j != i && mesh->drivers[j] && mesh->drivers[j]->is_available && mesh->drivers[j]->is_available()) {
                        if (mesh->drivers[j]->broadcast) {
                            mesh->drivers[j]->broadcast(rx_buf, (size_t)received_bytes);
                        }
                    }
                }
            }
        }
    }
}

void lux_mesh_prune_peers(lux_mesh_engine_t *mesh, uint32_t current_millis) {
    if (!mesh) return;
    for (uint8_t i = 0; i < mesh->peer_count; i++) {
        if (current_millis - mesh->peers[i].last_seen_ms > LUX_PEER_TIMEOUT_MS) {
            mesh->peers[i].alive = false;
        }
    }
}

/* ── Stubs for Built-in Transport Drivers (Serial, nRF-Radio, BLE, ESP-NOW, WiFi-UDP, LoRa, CAN) ── */
static bool default_init(void *cfg) { (void)cfg; return true; }
static bool default_is_avail(void) { return true; }
static int  default_send(const uint8_t *b, size_t l, uint16_t d) { (void)d; return (int)l; }
static int  default_bcast(const uint8_t *b, size_t l) { return (int)l; }
static int  default_rx(uint8_t *b, size_t m, uint16_t *s) { (void)b; (void)m; (void)s; return 0; }
static int8_t default_rssi(void) { return 0; }
static void default_deinit(void) {}

lux_transport_driver_t lux_driver_serial = {
    .id = LUX_BEARER_SERIAL, .name = "Serial", .priority = 1,
    .init = default_init, .is_available = default_is_avail,
    .send = default_send, .broadcast = default_bcast, .receive = default_rx,
    .get_rssi = default_rssi, .deinit = default_deinit
};

lux_transport_driver_t lux_driver_nrf_radio = {
    .id = LUX_BEARER_NRF_RADIO, .name = "nRF-Radio", .priority = 2,
    .init = default_init, .is_available = default_is_avail,
    .send = default_send, .broadcast = default_bcast, .receive = default_rx,
    .get_rssi = default_rssi, .deinit = default_deinit
};

lux_transport_driver_t lux_driver_ble = {
    .id = LUX_BEARER_BLE, .name = "BLE", .priority = 3,
    .init = default_init, .is_available = default_is_avail,
    .send = default_send, .broadcast = default_bcast, .receive = default_rx,
    .get_rssi = default_rssi, .deinit = default_deinit
};

lux_transport_driver_t lux_driver_espnow = {
    .id = LUX_BEARER_ESPNOW, .name = "ESP-NOW", .priority = 2,
    .init = default_init, .is_available = default_is_avail,
    .send = default_send, .broadcast = default_bcast, .receive = default_rx,
    .get_rssi = default_rssi, .deinit = default_deinit
};

lux_transport_driver_t lux_driver_wifi_udp = {
    .id = LUX_BEARER_WIFI_UDP, .name = "WiFi-UDP", .priority = 4,
    .init = default_init, .is_available = default_is_avail,
    .send = default_send, .broadcast = default_bcast, .receive = default_rx,
    .get_rssi = default_rssi, .deinit = default_deinit
};

lux_transport_driver_t lux_driver_lora = {
    .id = LUX_BEARER_LORA, .name = "LoRa", .priority = 5,
    .init = default_init, .is_available = default_is_avail,
    .send = default_send, .broadcast = default_bcast, .receive = default_rx,
    .get_rssi = default_rssi, .deinit = default_deinit
};

lux_transport_driver_t lux_driver_can = {
    .id = LUX_BEARER_CAN, .name = "CAN", .priority = 1,
    .init = default_init, .is_available = default_is_avail,
    .send = default_send, .broadcast = default_bcast, .receive = default_rx,
    .get_rssi = default_rssi, .deinit = default_deinit
};
