/**
 * Pyintel Lux — Official Arduino Library C++ Implementation
 * Ultra-lightweight, zero-heap binary telemetry and autonomous mesh networking engine.
 */

#include "Lux.h"
#include "lux/transports/transport_serial.h"
#include "lux/transports/transport_espnow.h"
#include "lux/transports/transport_wifi_udp.h"
#include "lux/transports/transport_ble.h"
#include "lux/transports/transport_nrf_radio.h"

LuxClass Lux;

#if defined(__AVR__) || defined(ARDUINO_ARCH_AVR)
#include <avr/io.h>
#include <avr/interrupt.h>

extern unsigned int __bss_end;
extern unsigned int __heap_start;
extern void *__brkval;

ISR(TIMER1_COMPA_vect) {
    Lux.tick();
}
#endif

/* ── Constructor ──────────────────────────────────────────────────── */
LuxClass::LuxClass()
    : _stream(nullptr),
      _last_heartbeat_ms(0),
      _heartbeat_interval_ms(1000),
      _last_flush_ms(0),
      _flush_timeout_ms(10),
      _is_initialized(false),
      _mesh_active(false),
      _border_locked(false),
      _auto_heal_enabled(true),
      _auto_heal_timeout_ms(8000),
      _boot_time_ms(0),
      _last_peer_packet_ms(0),
      _auto_seal_deadline_ms(0),
      _net_hash(0),
      _node_id(0x0042),
      _mesh_seq(0),
      _transport_count(0),
      _peer_count(0),
      _handler_count(0),
      _catchall_handler(nullptr),
      _debug_enabled(false),
      _debug_stream(&Serial),
      _debug_level(LUX_DEBUG_FULL),
      _last_debug_table_ms(0) {
    memset(&_ctx, 0, sizeof(_ctx));
    memset(_transports, 0, sizeof(_transports));
    memset(_peers, 0, sizeof(_peers));
    memset(_handlers, 0, sizeof(_handlers));
}

lux_status_t LuxClass::streamWriteCallback(const uint8_t *buf, size_t len, void *ctx) {
    LuxClass *self = (LuxClass *)ctx;
    if (!self) return LUX_ERR_NULL;

    if (self->_mesh_active) {
        // In mesh mode, emit out through all active transports
        for (uint8_t i = 0; i < self->_transport_count; i++) {
            if (self->_transports[i] && self->_transports[i]->is_available()) {
                self->_transports[i]->broadcast(buf, len);
            }
        }
        return LUX_OK;
    }

    if (!self->_stream) return LUX_ERR_NULL;
    size_t written = self->_stream->write(buf, len);
    return (written == len) ? LUX_OK : LUX_ERR_TRANSPORT;
}

uint32_t LuxClass::arduinoMicrosSource() {
    return micros();
}

/* ── Standard Single-Node Initialization ──────────────────────────── */
lux_status_t LuxClass::begin(Stream &stream, unsigned long baud) {
    (void)baud;
    _stream = &stream;
    _mesh_active = false;
    lux_transport_serial_set_stream(&stream);

    lux_status_t status = lux_init(&_ctx, streamWriteCallback, this, arduinoMicrosSource);
    if (status != LUX_OK) return status;

    _last_heartbeat_ms = millis();
    _last_flush_ms = millis();
    _is_initialized = true;

    return lux_emit_u8(&_ctx, LUX_SYM_RESET, 0x00);
}

lux_status_t LuxClass::begin(lux_write_fn custom_write, void *ctx) {
    _stream = nullptr;
    _mesh_active = false;

    lux_status_t status = lux_init(&_ctx, custom_write, ctx, arduinoMicrosSource);
    if (status != LUX_OK) return status;

    _last_heartbeat_ms = millis();
    _last_flush_ms = millis();
    _is_initialized = true;

    return lux_emit_u8(&_ctx, LUX_SYM_RESET, 0x00);
}

lux_status_t LuxClass::beginMesh(const char *network_uuid, Stream &stream, unsigned long baud) {
    _stream = &stream;
    lux_transport_serial_set_stream(&stream);
    return beginMesh(network_uuid, nullptr, nullptr);
}

/* ── Autonomous Multi-Transport Mesh Initialization ──────────────── */
lux_status_t LuxClass::beginMesh(const char *network_uuid, const char *wifi_ssid, const char *wifi_pass) {
    if (!network_uuid) return LUX_ERR_NULL;

    _mesh_active = true;
    _net_hash = lux_crc32(network_uuid);
    _node_id = lux_generate_node_id();
    _transport_count = 0;
    _peer_count = 0;

    // 1. Register Serial transport (always present)
    lux_transport_serial.init(_stream ? _stream : &Serial);
    _transports[_transport_count++] = &lux_transport_serial;

#if LUX_HAS_ESPNOW
    // 2. Register ESP-NOW transport (ESP32/ESP8266)
    if (lux_transport_espnow.init(nullptr)) {
        _transports[_transport_count++] = &lux_transport_espnow;
    }
#endif

#if LUX_HAS_WIFI_UDP
    // 3. Register Wi-Fi UDP transport if SSID provided
    if (wifi_ssid && strlen(wifi_ssid) > 0) {
        lux_wifi_udp_config_t cfg = {
            .ssid = wifi_ssid,
            .pass = wifi_pass,
            .port = LUX_UDP_PORT
        };
        if (lux_transport_wifi_udp.init(&cfg)) {
            _transports[_transport_count++] = &lux_transport_wifi_udp;
        }
    }
#endif

#if LUX_HAS_BLE
    // 4. Register BLE transport (Nordic nRF52 / ESP32)
    if (lux_transport_ble.init(nullptr)) {
        _transports[_transport_count++] = &lux_transport_ble;
    }
#endif

#if LUX_HAS_NRF_RADIO
    // 5. Register Nordic 2.4GHz Hardware Radio (BBC micro:bit / Adafruit CLUE)
    if (lux_transport_nrf_radio.init(nullptr)) {
        _transports[_transport_count++] = &lux_transport_nrf_radio;
    }
#endif

    lux_status_t status = lux_init(&_ctx, streamWriteCallback, this, arduinoMicrosSource);
    if (status != LUX_OK) return status;

    _last_heartbeat_ms = millis();
    _last_flush_ms = millis();
    _last_debug_table_ms = millis();
    _is_initialized = true;

    // Broadcast startup heartbeat to announce ourselves to the mesh
    heartbeat();

    return LUX_OK;
}

/* ── Mesh Frame Packing & Emission ────────────────────────────────── */
lux_status_t LuxClass::emitMeshFrame(uint16_t dst_node, uint16_t symbol, lux_payload_type_t type, const uint8_t *payload, uint8_t len) {
    if (!_is_initialized) return LUX_ERR_NULL;

    uint8_t packet[LUX_MAX_PAYLOAD_SIZE + LUX_MESH_HEADER_SIZE + LUX_HEADER_SIZE];

    // 1. Pack 12-byte Mesh Envelope
    lux_mesh_envelope_t *env = (lux_mesh_envelope_t *)packet;
    env->sync[0]    = LUX_MESH_SYNC_0;
    env->sync[1]    = LUX_MESH_SYNC_1;
    env->net_hash   = _net_hash;
    env->src_node   = _node_id;
    env->dst_node   = dst_node;
    env->hop_count  = LUX_DEFAULT_HOP_LIMIT;
    env->flags      = 0;

    // 2. Pack 14-byte Inner Lux Frame
    lux_frame_header_t *inner = (lux_frame_header_t *)(packet + LUX_MESH_HEADER_SIZE);
    inner->sync[0]       = LUX_SYNC_0;
    inner->sync[1]       = LUX_SYNC_1;
    inner->seq_num       = _mesh_seq++;
    inner->symbol_id     = symbol;
    inner->timestamp_us  = micros();
    inner->payload_type  = (uint8_t)type;
    inner->payload_len   = len;

    // Compute CRC16 over inner header [0..11]
    inner->crc16 = lux_crc16((const uint8_t *)inner, 12);

    // 3. Copy Payload
    if (payload && len > 0) {
        memcpy(packet + LUX_MESH_HEADER_SIZE + LUX_HEADER_SIZE, payload, len);
    }

    size_t total_len = LUX_MESH_HEADER_SIZE + LUX_HEADER_SIZE + len;

    // 4. Emit through all available active transports
    for (uint8_t i = 0; i < _transport_count; i++) {
        if (_transports[i] && _transports[i]->is_available()) {
            if (dst_node == LUX_NODE_BROADCAST) {
                _transports[i]->broadcast(packet, total_len);
            } else {
                _transports[i]->send(packet, total_len, dst_node);
            }
        }
    }

    // 5. Debug Monitor Logging
    if (_debug_enabled && _debug_stream && _debug_level >= LUX_DEBUG_TRAFFIC) {
        printDebugFrame(false, _node_id, dst_node, symbol, type, payload, len,
                        LUX_TRANSPORT_SERIAL, env->hop_count, true);
    }

    return LUX_OK;
}

/* ── Typed Mesh Send & Broadcast ─────────────────────────────────── */
lux_status_t LuxClass::send(uint16_t target_node, uint16_t symbol, uint8_t val) {
    return emitMeshFrame(target_node, symbol, LUX_TYPE_U8, &val, 1);
}

lux_status_t LuxClass::send(uint16_t target_node, uint16_t symbol, uint16_t val) {
    uint8_t raw[2] = { (uint8_t)(val & 0xFF), (uint8_t)((val >> 8) & 0xFF) };
    return emitMeshFrame(target_node, symbol, LUX_TYPE_U16, raw, 2);
}

lux_status_t LuxClass::send(uint16_t target_node, uint16_t symbol, uint32_t val) {
    uint8_t raw[4] = {
        (uint8_t)(val & 0xFF), (uint8_t)((val >> 8) & 0xFF),
        (uint8_t)((val >> 16) & 0xFF), (uint8_t)((val >> 24) & 0xFF)
    };
    return emitMeshFrame(target_node, symbol, LUX_TYPE_U32, raw, 4);
}

lux_status_t LuxClass::send(uint16_t target_node, uint16_t symbol, int32_t val) {
    return send(target_node, symbol, (uint32_t)val);
}

lux_status_t LuxClass::send(uint16_t target_node, uint16_t symbol, float val) {
    uint8_t raw[4];
    memcpy(raw, &val, 4);
    return emitMeshFrame(target_node, symbol, LUX_TYPE_F32, raw, 4);
}

lux_status_t LuxClass::send(uint16_t target_node, uint16_t symbol, const uint8_t *data, uint8_t len) {
    return emitMeshFrame(target_node, symbol, LUX_TYPE_BYTES, data, len);
}

lux_status_t LuxClass::send(uint16_t target_node, uint16_t symbol, const char *str) {
    if (!str) return LUX_ERR_NULL;
    return emitMeshFrame(target_node, symbol, LUX_TYPE_BYTES, (const uint8_t *)str, (uint8_t)strlen(str));
}

lux_status_t LuxClass::broadcast(uint16_t symbol, uint8_t val) {
    return send(LUX_NODE_BROADCAST, symbol, val);
}

lux_status_t LuxClass::broadcast(uint16_t symbol, uint16_t val) {
    return send(LUX_NODE_BROADCAST, symbol, val);
}

lux_status_t LuxClass::broadcast(uint16_t symbol, uint32_t val) {
    return send(LUX_NODE_BROADCAST, symbol, val);
}

lux_status_t LuxClass::broadcast(uint16_t symbol, int32_t val) {
    return send(LUX_NODE_BROADCAST, symbol, val);
}

lux_status_t LuxClass::broadcast(uint16_t symbol, float val) {
    return send(LUX_NODE_BROADCAST, symbol, val);
}

lux_status_t LuxClass::broadcast(uint16_t symbol, const uint8_t *data, uint8_t len) {
    return send(LUX_NODE_BROADCAST, symbol, data, len);
}

lux_status_t LuxClass::broadcast(uint16_t symbol, const char *str) {
    return send(LUX_NODE_BROADCAST, symbol, str);
}

/* ── Message Callbacks & Ingestion ───────────────────────────────── */
void LuxClass::onMessage(uint16_t symbol, LuxMessageHandler handler) {
    if (!handler) return;
    for (uint8_t i = 0; i < _handler_count; i++) {
        if (_handlers[i].symbol_id == symbol) {
            _handlers[i].handler = handler;
            return;
        }
    }
    if (_handler_count < LUX_MAX_HANDLERS) {
        _handlers[_handler_count].symbol_id = symbol;
        _handlers[_handler_count].handler = handler;
        _handler_count++;
    }
}

void LuxClass::onMessage(LuxMessageHandler handler) {
    _catchall_handler = handler;
}

void LuxClass::dispatchMessage(uint16_t from_node, uint16_t symbol, const uint8_t *payload, uint8_t len) {
    bool handled = false;
    for (uint8_t i = 0; i < _handler_count; i++) {
        if (_handlers[i].symbol_id == symbol && _handlers[i].handler) {
            _handlers[i].handler(from_node, symbol, payload, len);
            handled = true;
        }
    }
    if (!handled && _catchall_handler) {
        _catchall_handler(from_node, symbol, payload, len);
    }
}

/* ── Peer Table Management ────────────────────────────────────────── */
void LuxClass::updatePeer(uint16_t node_id, lux_transport_id_t transport_id, int8_t rssi, uint32_t uptime_ms) {
    if (node_id == 0x0000 || node_id == _node_id) return;

    for (uint8_t i = 0; i < _peer_count; i++) {
        if (_peers[i].node_id == node_id) {
            _peers[i].transports |= (1 << transport_id);
            _peers[i].rssi = rssi;
            _peers[i].last_seen_ms = millis();
            if (uptime_ms > 0) _peers[i].uptime_ms = uptime_ms;
            _peers[i].rx_count++;
            _peers[i].alive = true;
            return;
        }
    }

    if (_peer_count < LUX_MAX_PEERS) {
        LuxPeer &p = _peers[_peer_count++];
        p.node_id = node_id;
        p.transports = (1 << transport_id);
        p.rssi = rssi;
        p.last_seen_ms = millis();
        p.uptime_ms = uptime_ms;
        p.rx_count = 1;
        p.tx_count = 0;
        p.alive = true;

        if (_debug_enabled && _debug_stream) {
            _debug_stream->print(F("[PEER DISCOVERED] Node: 0x"));
            _debug_stream->print(node_id, HEX);
            _debug_stream->print(F(" | RSSI: "));
            _debug_stream->print(rssi);
            _debug_stream->println(F(" dBm"));
        }
    }
}

void LuxClass::pruneDeadPeers() {
    uint32_t now = millis();
    for (uint8_t i = 0; i < _peer_count; i++) {
        if (now - _peers[i].last_seen_ms > LUX_PEER_TIMEOUT_MS) {
            _peers[i].alive = false;
        }
    }
}

LuxPeer LuxClass::getPeer(uint8_t index) const {
    if (index < _peer_count) {
        return _peers[index];
    }
    LuxPeer empty;
    memset(&empty, 0, sizeof(empty));
    return empty;
}

const LuxPeer* LuxClass::findPeer(uint16_t node_id) const {
    for (uint8_t i = 0; i < _peer_count; i++) {
        if (_peers[i].node_id == node_id) {
            return &_peers[i];
        }
    }
    return nullptr;
}

/* ── ASCII Mesh Device List Table ─────────────────────────────────── */
void LuxClass::list(Stream &out) {
    out.println();
    out.println(F("┌────────┬────────────────────────────┬───────┬─────────┬────────┐"));
    out.println(F("│ Node   │ Transports                 │ RSSI  │ Uptime  │ Status │"));
    out.println(F("├────────┼────────────────────────────┼───────┼─────────┼────────┤"));

    // Self
    out.print(F("│ 0x"));
    if (_node_id < 0x1000) out.print('0');
    if (_node_id < 0x0100) out.print('0');
    if (_node_id < 0x0010) out.print('0');
    out.print(_node_id, HEX);
    out.print(F(" │ Serial"));
#if LUX_HAS_ESPNOW
    out.print(F("+NOW"));
#endif
#if LUX_HAS_WIFI_UDP
    out.print(F("+WiFi"));
#endif
#if LUX_HAS_NRF_RADIO
    out.print(F("+nRF-Radio"));
#endif
    out.print(F(" (ME)           │   0   │ "));
    out.print(millis() / 1000);
    out.print(F("s   │ ● LIVE │\n"));

    // Peers
    for (uint8_t i = 0; i < _peer_count; i++) {
        out.print(F("│ 0x"));
        if (_peers[i].node_id < 0x1000) out.print('0');
        if (_peers[i].node_id < 0x0100) out.print('0');
        if (_peers[i].node_id < 0x0010) out.print('0');
        out.print(_peers[i].node_id, HEX);
        out.print(F(" │ "));

        if (_peers[i].transports & LUX_FLAG_TRANSPORT_ESPNOW) out.print(F("ESP-NOW "));
        if (_peers[i].transports & LUX_FLAG_TRANSPORT_WIFI_UDP) out.print(F("WiFi-UDP "));
        if (_peers[i].transports & LUX_FLAG_TRANSPORT_NRF_RADIO) out.print(F("nRF-Radio "));
        if (_peers[i].transports & LUX_FLAG_TRANSPORT_SERIAL) out.print(F("Serial "));

        out.print(F("         │  "));
        out.print(_peers[i].rssi);
        out.print(F("  │ "));
        out.print(_peers[i].uptime_ms / 1000);
        out.print(F("s   │ "));
        out.println(_peers[i].alive ? F("● LIVE │") : F("○ IDLE │"));
    }

    out.println(F("└────────┴────────────────────────────┴───────┴─────────┴────────┘"));
    out.println();
}

const char* LuxClass::getActiveTransport() const {
    if (_transport_count == 0) return "None";
    return _transports[0]->name;
}

int8_t LuxClass::getRSSI() const {
    int8_t best = -127;
    for (uint8_t i = 0; i < _transport_count; i++) {
        if (_transports[i] && _transports[i]->is_available()) {
            int8_t r = _transports[i]->get_rssi();
            if (r > best) best = r;
        }
    }
    return best;
}

/* ── Debug Monitor Mode ───────────────────────────────────────────── */
void LuxClass::debug(bool enable, Stream &output) {
    _debug_enabled = enable;
    _debug_stream = &output;

    if (enable && _debug_stream) {
        _debug_stream->println();
        _debug_stream->println(F("╔══════════════════════════════════════════════════════════════╗"));
        _debug_stream->println(F("║  PYINTEL LUX — PROMISCUOUS MESH MONITOR                      ║"));
        _debug_stream->print(F("║  This Node: 0x"));
        _debug_stream->print(_node_id, HEX);
        _debug_stream->print(F(" | Net Hash: 0x"));
        _debug_stream->print(_net_hash, HEX);
        _debug_stream->println(F("                   ║"));
        _debug_stream->println(F("╚══════════════════════════════════════════════════════════════╝"));
    }
}

void LuxClass::printDebugFrame(bool is_rx, uint16_t src, uint16_t dst, uint16_t sym, uint8_t type, const uint8_t *payload, uint8_t len, lux_transport_id_t transport_id, uint8_t hop, bool crc_ok) {
    if (!_debug_stream) return;

    _debug_stream->print(is_rx ? F("← RX ") : F("→ TX "));
    _debug_stream->print(F("src=0x")); _debug_stream->print(src, HEX);
    _debug_stream->print(F(" dst="));
    if (dst == LUX_NODE_BROADCAST) {
        _debug_stream->print(F("BCAST"));
    } else {
        _debug_stream->print(F("0x")); _debug_stream->print(dst, HEX);
    }
    _debug_stream->print(F(" sym=0x")); _debug_stream->print(sym, HEX);
    _debug_stream->print(F(" type=")); _debug_stream->print(type);
    _debug_stream->print(F(" len=")); _debug_stream->print(len);

    if (payload && len > 0) {
        _debug_stream->print(F(" val="));
        if (type == LUX_TYPE_U8) {
            _debug_stream->print(payload[0]);
        } else if (type == LUX_TYPE_U16 && len >= 2) {
            uint16_t v = payload[0] | (payload[1] << 8);
            _debug_stream->print(v);
        } else if (type == LUX_TYPE_U32 && len >= 4) {
            uint32_t v = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8) | ((uint32_t)payload[2] << 16) | ((uint32_t)payload[3] << 24);
            _debug_stream->print(v);
        } else if (type == LUX_TYPE_F32 && len >= 4) {
            float f;
            memcpy(&f, payload, 4);
            _debug_stream->print(f, 2);
        } else {
            for (uint8_t i = 0; i < len && i < 8; i++) {
                _debug_stream->print((char)payload[i]);
            }
        }
    }

    _debug_stream->print(F(" hop=")); _debug_stream->print(hop);
    _debug_stream->print(F(" crc=")); _debug_stream->println(crc_ok ? F("OK") : F("ERR"));
}

/* ── Background Ingestion & Non-Blocking Tick Engine ──────────────── */
lux_status_t LuxClass::tick() {
    if (!_is_initialized) return LUX_ERR_NULL;

    uint32_t now = millis();
    if (_boot_time_ms == 0) {
        _boot_time_ms = now;
        _last_peer_packet_ms = now;
    }

    // 0. Auto-Seal Temporary Rendezvous Window Check
    if (_auto_seal_deadline_ms > 0 && now >= _auto_seal_deadline_ms) {
        _auto_seal_deadline_ms = 0;
        initBorder();
    }

    // 0.1 Auto-Healing Watchdog Check: if locked and lost all peers, heal network
    if (_mesh_active && _border_locked && _auto_heal_enabled && _peer_count > 0) {
        if (now - _last_peer_packet_ms > _auto_heal_timeout_ms) {
            unlockBorder();
            knock();
            if (_debug_enabled && _debug_stream) {
                _debug_stream->println(F("🩹 [AUTO-HEAL TRIGGERED] Link lost. Promiscuous discovery re-enabled."));
            }
        }
    }

    // 1. Heartbeat generator
    if (_heartbeat_interval_ms > 0 && (now - _last_heartbeat_ms >= _heartbeat_interval_ms)) {
        heartbeat();
        _last_heartbeat_ms = now;
    }

    // 2. Buffer flush timeout
    if (_ctx.tx_buf_len > 0 && (now - _last_flush_ms >= _flush_timeout_ms)) {
        flush();
    }

    // 3. Multi-Transport Ingestion Loop
    uint8_t rx_buf[LUX_MAX_PAYLOAD_SIZE + LUX_MESH_HEADER_SIZE + LUX_HEADER_SIZE];

    for (uint8_t i = 0; i < _transport_count; i++) {
        if (!_transports[i] || !_transports[i]->is_available()) continue;

        uint16_t src_node = 0;
        int received_bytes = _transports[i]->receive(rx_buf, sizeof(rx_buf), &src_node);

        if (received_bytes >= (int)(LUX_MESH_HEADER_SIZE + LUX_HEADER_SIZE)) {
            // Check for Mesh frame
            if (rx_buf[0] == LUX_MESH_SYNC_0 && rx_buf[1] == LUX_MESH_SYNC_1) {
                lux_mesh_envelope_t *env = (lux_mesh_envelope_t *)rx_buf;

                // Validate Network UUID hash
                if (env->net_hash != _net_hash) {
                    continue; // Belongs to a different mesh network
                }

                lux_frame_header_t *inner = (lux_frame_header_t *)(rx_buf + LUX_MESH_HEADER_SIZE);

                // Verify CRC-16 of inner header
                uint16_t computed_crc = lux_crc16((const uint8_t *)inner, 12);
                bool crc_ok = (computed_crc == inner->crc16);

                if (!crc_ok) {
                    if (_debug_enabled && _debug_stream) {
                        _debug_stream->print(F("[CRC ERROR] from Node 0x"));
                        _debug_stream->println(env->src_node, HEX);
                    }
                    continue;
                }

                _last_peer_packet_ms = now;
                uint8_t *payload_ptr = rx_buf + LUX_MESH_HEADER_SIZE + LUX_HEADER_SIZE;
                uint8_t payload_len = inner->payload_len;

                // Update peer table
                uint32_t peer_uptime = 0;
                if (inner->symbol_id == LUX_SYM_HEARTBEAT && inner->payload_type == LUX_TYPE_U32 && payload_len >= 4) {
                    peer_uptime = (uint32_t)payload_ptr[0] | ((uint32_t)payload_ptr[1] << 8) |
                                  ((uint32_t)payload_ptr[2] << 16) | ((uint32_t)payload_ptr[3] << 24);
                }
                updatePeer(env->src_node, _transports[i]->id, _transports[i]->get_rssi(), peer_uptime);

                // Debug print
                if (_debug_enabled && _debug_stream && _debug_level >= LUX_DEBUG_TRAFFIC) {
                    printDebugFrame(true, env->src_node, env->dst_node, inner->symbol_id,
                                    inner->payload_type, payload_ptr, payload_len,
                                    _transports[i]->id, env->hop_count, crc_ok);
                }

                // Deliver to local handlers if addressed to us or broadcast
                if (env->dst_node == _node_id || env->dst_node == LUX_NODE_BROADCAST) {
                    // System Border Lock / Unlock Handlers
                    if (inner->symbol_id == LUX_SYM_BORDER_LOCK && !_border_locked) {
                        _border_locked = true;
                        // Power down unused transports
                        uint8_t active_mask = 0;
                        for (uint8_t p = 0; p < _peer_count; p++) {
                            if (_peers[p].alive) active_mask |= _peers[p].transports;
                        }
                        active_mask |= LUX_FLAG_TRANSPORT_SERIAL;
                        for (uint8_t t = 0; t < _transport_count; t++) {
                            if (_transports[t] && !(active_mask & (1 << _transports[t]->id))) {
                                if (_transports[t]->deinit) _transports[t]->deinit();
                            }
                        }
                    } else if (inner->symbol_id == LUX_SYM_BORDER_UNLOCK && _border_locked) {
                        _border_locked = false;
                        for (uint8_t t = 0; t < _transport_count; t++) {
                            if (_transports[t] && _transports[t]->init) _transports[t]->init(nullptr);
                        }
                    } else if (inner->symbol_id == LUX_SYM_BORDER_KNOCK) {
                        // Straggler join knock received: open 6-second rendezvous window
                        if (_border_locked) {
                            unlockBorder();
                            _auto_seal_deadline_ms = now + 6000;
                            if (_debug_enabled && _debug_stream) {
                                _debug_stream->println(F("🚪 [RENDEZVOUS WINDOW OPENED] Admitting new node for 6s."));
                            }
                        }
                    }

                    dispatchMessage(env->src_node, inner->symbol_id, payload_ptr, payload_len);
                }

                // Multi-hop relay: if not addressed to us and hop limit remaining, retransmit
                if (env->dst_node != _node_id && env->dst_node != LUX_NODE_BROADCAST && env->hop_count > 0) {
                    env->hop_count--;
                    for (uint8_t j = 0; j < _transport_count; j++) {
                        if (j != i && _transports[j] && _transports[j]->is_available()) {
                            _transports[j]->broadcast(rx_buf, (size_t)received_bytes);
                        }
                    }
                }
            }
        }
    }

    // 4. Prune dead peers
    pruneDeadPeers();

    // 5. Periodic Full Debug Status Table
    if (_debug_enabled && _debug_stream && _debug_level == LUX_DEBUG_FULL && (now - _last_debug_table_ms >= 5000)) {
        list(*_debug_stream);
        _last_debug_table_ms = now;
    }

    return LUX_OK;
}

lux_status_t LuxClass::relayRawMeshFrame(const uint8_t *frame_data, size_t len, lux_transport_id_t incoming_transport) {
    if (!frame_data || len < (LUX_MESH_HEADER_SIZE + LUX_HEADER_SIZE)) return LUX_ERR_NULL;

    // Validate sync bytes
    if (frame_data[0] != LUX_MESH_SYNC_0 || frame_data[1] != LUX_MESH_SYNC_1) return LUX_ERR_TRANSPORT;

    lux_mesh_envelope_t *env = (lux_mesh_envelope_t *)frame_data;
    if (env->net_hash != _net_hash) return LUX_OK; // Filtered

    lux_frame_header_t *inner = (lux_frame_header_t *)(frame_data + LUX_MESH_HEADER_SIZE);
    uint16_t computed_crc = lux_crc16((const uint8_t *)inner, 12);
    bool crc_ok = (computed_crc == inner->crc16);
    if (!crc_ok) return LUX_ERR_TRANSPORT;

    uint8_t *payload_ptr = (uint8_t *)(frame_data + LUX_MESH_HEADER_SIZE + LUX_HEADER_SIZE);
    uint8_t payload_len = inner->payload_len;

    // 1. Update peer table for the originating wired node (Pico / Uno)
    uint32_t peer_uptime = 0;
    if (inner->symbol_id == LUX_SYM_HEARTBEAT && inner->payload_type == LUX_TYPE_U32 && payload_len >= 4) {
        peer_uptime = (uint32_t)payload_ptr[0] | ((uint32_t)payload_ptr[1] << 8) |
                      ((uint32_t)payload_ptr[2] << 16) | ((uint32_t)payload_ptr[3] << 24);
    }
    updatePeer(env->src_node, LUX_TRANSPORT_SERIAL, 0, peer_uptime);

    // 2. Print debug stream to PC USB Serial
    if (_debug_enabled && _debug_stream) {
        printDebugFrame(true, env->src_node, env->dst_node, inner->symbol_id, inner->payload_type,
                        payload_ptr, payload_len, incoming_transport, env->hop_count, crc_ok);
    }

    // 3. Forward the intact raw envelope over wireless transports (ESP-NOW / Wi-Fi UDP)
    for (uint8_t i = 0; i < _transport_count; i++) {
        if (_transports[i] && _transports[i]->id != incoming_transport && _transports[i]->is_available()) {
            _transports[i]->send(frame_data, len, env->dst_node);
        }
    }

    // 4. Dispatch to registered local handlers
    dispatchMessage(env->src_node, inner->symbol_id, payload_ptr, payload_len);

    return LUX_OK;
}

lux_status_t LuxClass::flush() {
    if (!_is_initialized) return LUX_ERR_NULL;
    _last_flush_ms = millis();
    return lux_flush(&_ctx);
}

void LuxClass::setHeartbeatInterval(uint32_t interval_ms) {
    _heartbeat_interval_ms = interval_ms;
}

void LuxClass::setFlushTimeout(uint32_t timeout_ms) {
    _flush_timeout_ms = timeout_ms;
}

/* ── Standard Telemetry Tracing Overloads ─────────────────────────── */
lux_status_t LuxClass::trace(uint16_t symbol_id) {
    if (_mesh_active) return broadcast(symbol_id, (uint8_t)0);
    return lux_emit_none(&_ctx, symbol_id);
}

lux_status_t LuxClass::trace_none(uint16_t symbol_id) {
    return trace(symbol_id);
}

lux_status_t LuxClass::trace(uint16_t symbol_id, uint8_t val) {
    if (_mesh_active) return broadcast(symbol_id, val);
    return lux_emit_u8(&_ctx, symbol_id, val);
}

lux_status_t LuxClass::trace_u8(uint16_t symbol_id, uint8_t val) {
    return trace(symbol_id, val);
}

lux_status_t LuxClass::trace(uint16_t symbol_id, uint16_t val) {
    if (_mesh_active) return broadcast(symbol_id, val);
    return lux_emit_u16(&_ctx, symbol_id, val);
}

lux_status_t LuxClass::trace_u16(uint16_t symbol_id, uint16_t val) {
    return trace(symbol_id, val);
}

lux_status_t LuxClass::trace(uint16_t symbol_id, uint32_t val) {
    if (_mesh_active) return broadcast(symbol_id, val);
    return lux_emit_u32(&_ctx, symbol_id, val);
}

lux_status_t LuxClass::trace_u32(uint16_t symbol_id, uint32_t val) {
    return trace(symbol_id, val);
}

lux_status_t LuxClass::trace(uint16_t symbol_id, int8_t val) {
    return trace(symbol_id, (int32_t)val);
}

lux_status_t LuxClass::trace(uint16_t symbol_id, int16_t val) {
    return trace(symbol_id, (int32_t)val);
}

lux_status_t LuxClass::trace(uint16_t symbol_id, int32_t val) {
    if (_mesh_active) return broadcast(symbol_id, val);
    return lux_emit_i32(&_ctx, symbol_id, val);
}

lux_status_t LuxClass::trace_i32(uint16_t symbol_id, int32_t val) {
    return trace(symbol_id, val);
}

lux_status_t LuxClass::trace(uint16_t symbol_id, float val) {
    if (_mesh_active) return broadcast(symbol_id, val);
    return lux_emit_f32(&_ctx, symbol_id, val);
}

lux_status_t LuxClass::trace(uint16_t symbol_id, double val) {
    return trace(symbol_id, (float)val);
}

lux_status_t LuxClass::trace_f32(uint16_t symbol_id, float val) {
    return trace(symbol_id, val);
}

lux_status_t LuxClass::trace(uint16_t symbol_id, const uint8_t *data, uint8_t len) {
    if (_mesh_active) return broadcast(symbol_id, data, len);
    return lux_emit_bytes(&_ctx, symbol_id, data, len);
}

lux_status_t LuxClass::trace_bytes(uint16_t symbol_id, const uint8_t *data, uint8_t len) {
    return trace(symbol_id, data, len);
}

lux_status_t LuxClass::trace(uint16_t symbol_id, const char *str) {
    if (!str) return LUX_ERR_NULL;
    if (_mesh_active) return broadcast(symbol_id, str);
    return lux_emit_bytes(&_ctx, symbol_id, (const uint8_t *)str, (uint8_t)strlen(str));
}

lux_status_t LuxClass::trace_str(uint16_t symbol_id, const char *str) {
    return trace(symbol_id, str);
}

lux_status_t LuxClass::trace(uint16_t symbol_id, const String &str) {
    return trace(symbol_id, str.c_str());
}

lux_status_t LuxClass::trace_str(uint16_t symbol_id, const String &str) {
    return trace(symbol_id, str.c_str());
}

/* ── System Helpers ──────────────────────────────────────────────── */
lux_status_t LuxClass::heartbeat() {
    uint32_t uptime_ms = millis();
    if (_mesh_active) {
        return broadcast(LUX_SYM_HEARTBEAT, uptime_ms);
    }
    return lux_emit_u32(&_ctx, LUX_SYM_HEARTBEAT, uptime_ms);
}

lux_status_t LuxClass::deviceInfo(const char *board_name, uint32_t version) {
    (void)version;
    if (!board_name) return LUX_ERR_NULL;
    uint8_t len = (uint8_t)strlen(board_name);
    if (_mesh_active) {
        return broadcast(LUX_SYM_DEVICE_INFO, (const uint8_t *)board_name, len);
    }
    return lux_emit_bytes(&_ctx, LUX_SYM_DEVICE_INFO, (const uint8_t *)board_name, len);
}

lux_status_t LuxClass::pinReport(uint8_t pin, uint16_t value) {
    uint8_t payload[3];
    payload[0] = pin;
    payload[1] = (uint8_t)(value & 0xFF);
    payload[2] = (uint8_t)((value >> 8) & 0xFF);
    if (_mesh_active) {
        return broadcast(LUX_SYM_PIN_REPORT, payload, 3);
    }
    return lux_emit_bytes(&_ctx, LUX_SYM_PIN_REPORT, payload, 3);
}

uint16_t LuxClass::getFreeRam() {
#if defined(__AVR__) || defined(ARDUINO_ARCH_AVR)
    int free_memory;
    if ((int)__brkval == 0) {
        free_memory = ((int)&free_memory) - ((int)&__bss_end);
    } else {
        free_memory = ((int)&free_memory) - ((int)__brkval);
    }
    return (uint16_t)(free_memory > 0 ? free_memory : 0);
#elif defined(ESP32)
    return (uint16_t)(ESP.getFreeHeap() > 0xFFFF ? 0xFFFF : ESP.getFreeHeap());
#elif defined(ESP8266)
    return (uint16_t)(ESP.getFreeHeap() > 0xFFFF ? 0xFFFF : ESP.getFreeHeap());
#else
    return 2048;
#endif
}

#if defined(__AVR__) || defined(ARDUINO_ARCH_AVR)
bool LuxClass::enableTimer1Interrupt(uint16_t freq_hz) {
    if (freq_hz == 0 || freq_hz > 1000) return false;

    cli();
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1  = 0;

    uint32_t ocr = (16000000UL / (256UL * freq_hz)) - 1;
    if (ocr > 65535) ocr = 65535;

    OCR1A = (uint16_t)ocr;
    TCCR1B |= (1 << WGM12);
    TCCR1B |= (1 << CS12);
    TIMSK1 |= (1 << OCIE1A);
    sei();

    return true;
}

void LuxClass::disableTimer1Interrupt() {
    TIMSK1 &= ~(1 << OCIE1A);
}
#endif

/* ── Topology Border Lock & Battery Saver Implementation ──────────── */
void LuxClass::initBorder() {
    if (!_is_initialized) return;
    _border_locked = true;

    // Broadcast LUX_SYM_BORDER_LOCK (0x0007) so all peers freeze simultaneously
    uint8_t flags = 0x01;
    broadcast(LUX_SYM_BORDER_LOCK, flags);

    // Identify which transports have active peers
    uint8_t active_mask = 0;
    for (uint8_t p = 0; p < _peer_count; p++) {
        if (_peers[p].alive) {
            active_mask |= _peers[p].transports;
        }
    }
    active_mask |= LUX_FLAG_TRANSPORT_SERIAL; // Keep serial for host UI

    // Power down unused radio transports for maximum power savings
    for (uint8_t i = 0; i < _transport_count; i++) {
        if (_transports[i]) {
            if (!(active_mask & (1 << _transports[i]->id))) {
                if (_transports[i]->deinit) {
                    _transports[i]->deinit();
                }
            }
        }
    }

    if (_debug_enabled && _debug_stream) {
        _debug_stream->println(F("🛡️ [BORDER LOCKED] Topology frozen. Unused radios powered down for battery savings."));
    }
}

void LuxClass::unlockBorder() {
    if (!_is_initialized) return;
    _border_locked = false;

    // Re-initialize all transports back into promiscuous discovery
    for (uint8_t i = 0; i < _transport_count; i++) {
        if (_transports[i] && _transports[i]->init) {
            _transports[i]->init(nullptr);
        }
    }

    uint8_t flags = 0x00;
    broadcast(LUX_SYM_BORDER_UNLOCK, flags);

    if (_debug_enabled && _debug_stream) {
        _debug_stream->println(F("🔓 [BORDER UNLOCKED] Promiscuous multi-bearer discovery resumed."));
    }
}

void LuxClass::knock() {
    if (!_is_initialized) return;

    // Ensure all transports are briefly initialized to transmit the knock
    for (uint8_t i = 0; i < _transport_count; i++) {
        if (_transports[i] && _transports[i]->init) {
            _transports[i]->init(nullptr);
        }
    }

    uint8_t caps = 0xFF;
    broadcast(LUX_SYM_BORDER_KNOCK, caps);

    if (_debug_enabled && _debug_stream) {
        _debug_stream->println(F("🚪 [KNOCK TRANSMITTED] Requesting rendezvous window from locked peers."));
    }
}

void LuxClass::setAutoHealing(bool enable, uint32_t timeout_ms) {
    _auto_heal_enabled = enable;
    _auto_heal_timeout_ms = timeout_ms ? timeout_ms : 8000;
}
