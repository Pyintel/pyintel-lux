/**
 * Pyintel Lux — Official Arduino Library C++ API
 * Ultra-lightweight, zero-heap binary telemetry and autonomous mesh networking engine.
 * 
 * Supports AVR (Uno/Nano/Mega), SAMD21, RP2040/RP2350, ESP32 (S3/C3/C6), ESP8266, STM32.
 */

#ifndef PYINTEL_LUX_ARDUINO_H
#define PYINTEL_LUX_ARDUINO_H

#include <Arduino.h>
#include "lux/lux.h"
#include "lux/mesh.h"
#include "lux/transport.h"

class LuxClass {
public:
    LuxClass();

    /* ── Standard Single-Node Telemetry Mode (Backward Compatible) ──── */
    lux_status_t begin(Stream &stream = Serial, unsigned long baud = 115200);
    lux_status_t begin(lux_write_fn custom_write, void *ctx = nullptr);

    /* ── Autonomous Multi-Transport Mesh Mode ───────────────────────── */

    /**
     * Join a UUID-based mesh network across all available communication channels.
     * @param network_uuid String UUID (e.g. "f47ac10b-58cc-4372-a567-0e02b2c3d479")
     * @param wifi_ssid    Optional Wi-Fi SSID (nullptr to skip router Wi-Fi)
     * @param wifi_pass    Optional Wi-Fi password
     * @return LUX_OK on success
     */
    lux_status_t beginMesh(const char *network_uuid,
                           const char *wifi_ssid = nullptr,
                           const char *wifi_pass = nullptr);

    /**
     * Join a UUID-based mesh network specifying a dedicated Serial/UART stream.
     * Useful for Arduino Uno (Pins 0/1), SoftwareSerial, or ESP32 hardware UART2.
     */
    lux_status_t beginMesh(const char *network_uuid,
                           Stream &stream,
                           unsigned long baud = 115200);

    /** Get this board's unique 16-bit Node ID */
    uint16_t getNodeId() const { return _node_id; }

    /** Override this board's Node ID manually */
    void setNodeId(uint16_t id) { _node_id = id; }

    /** Get the 32-bit hash of the configured Network UUID */
    uint32_t getNetworkHash() const { return _net_hash; }

    /** Is this node running in mesh mode? */
    bool isMeshActive() const { return _mesh_active; }

    /* ── Mesh Messaging (Unicast & Broadcast) ────────────────────────── */
    lux_status_t send(uint16_t target_node, uint16_t symbol, uint8_t val);
    lux_status_t send(uint16_t target_node, uint16_t symbol, uint16_t val);
    lux_status_t send(uint16_t target_node, uint16_t symbol, uint32_t val);
    lux_status_t send(uint16_t target_node, uint16_t symbol, int32_t val);
    lux_status_t send(uint16_t target_node, uint16_t symbol, float val);
    lux_status_t send(uint16_t target_node, uint16_t symbol, const uint8_t *data, uint8_t len);
    lux_status_t send(uint16_t target_node, uint16_t symbol, const char *str);

    lux_status_t broadcast(uint16_t symbol, uint8_t val);
    lux_status_t broadcast(uint16_t symbol, uint16_t val);
    lux_status_t broadcast(uint16_t symbol, uint32_t val);
    lux_status_t broadcast(uint16_t symbol, int32_t val);
    lux_status_t broadcast(uint16_t symbol, float val);
    lux_status_t broadcast(uint16_t symbol, const uint8_t *data, uint8_t len);
    lux_status_t broadcast(uint16_t symbol, const char *str);

    /* ── Message Callbacks & Ingestion ───────────────────────────────── */
    void onMessage(uint16_t symbol, LuxMessageHandler handler);
    void onMessage(LuxMessageHandler handler);

    /* ── Peer Discovery & Mesh Visibility ────────────────────────────── */
    uint8_t        getPeerCount() const { return _peer_count; }
    LuxPeer        getPeer(uint8_t index) const;
    const LuxPeer* findPeer(uint16_t node_id) const;
    void           list(Stream &output = Serial);

    const char*    getActiveTransport() const;
    int8_t         getRSSI() const;

    /* ── Live Debug / Monitor Mode ───────────────────────────────────── */
    void debug(bool enable, Stream &output = Serial);
    void setDebugVerbosity(lux_debug_level_t level) { _debug_level = level; }

    /* ── Core Background Engine ──────────────────────────────────────── */
    lux_status_t tick();
    lux_status_t flush();
    void         setHeartbeatInterval(uint32_t interval_ms);
    void         setFlushTimeout(uint32_t timeout_ms);

    /* ── Direct Telemetry Emit Methods ───────────────────────────────── */
    lux_status_t trace(uint16_t symbol_id);
    lux_status_t trace_none(uint16_t symbol_id);

    lux_status_t trace(uint16_t symbol_id, uint8_t val);
    lux_status_t trace_u8(uint16_t symbol_id, uint8_t val);

    lux_status_t trace(uint16_t symbol_id, uint16_t val);
    lux_status_t trace_u16(uint16_t symbol_id, uint16_t val);

    lux_status_t trace(uint16_t symbol_id, uint32_t val);
    lux_status_t trace_u32(uint16_t symbol_id, uint32_t val);

    lux_status_t trace(uint16_t symbol_id, int8_t val);
    lux_status_t trace(uint16_t symbol_id, int16_t val);
    lux_status_t trace(uint16_t symbol_id, int32_t val);
    lux_status_t trace_i32(uint16_t symbol_id, int32_t val);

    lux_status_t trace(uint16_t symbol_id, float val);
    lux_status_t trace(uint16_t symbol_id, double val);
    lux_status_t trace_f32(uint16_t symbol_id, float val);

    lux_status_t trace(uint16_t symbol_id, const uint8_t *data, uint8_t len);
    lux_status_t trace_bytes(uint16_t symbol_id, const uint8_t *data, uint8_t len);

    lux_status_t trace(uint16_t symbol_id, const char *str);
    lux_status_t trace_str(uint16_t symbol_id, const char *str);
    lux_status_t trace(uint16_t symbol_id, const String &str);
    lux_status_t trace_str(uint16_t symbol_id, const String &str);

    /* ── System Helpers ──────────────────────────────────────────────── */
    lux_status_t heartbeat();
    lux_status_t deviceInfo(const char *board_name, uint32_t version = 1);
    lux_status_t pinReport(uint8_t pin, uint16_t value);
    static uint16_t getFreeRam();
    lux_ctx_t*   getContext() { return &_ctx; }

#if defined(__AVR__) || defined(ARDUINO_ARCH_AVR)
    bool enableTimer1Interrupt(uint16_t freq_hz = 100);
    void disableTimer1Interrupt();
#endif

private:
    lux_ctx_t          _ctx;
    Stream            *_stream;
    uint32_t           _last_heartbeat_ms;
    uint32_t           _heartbeat_interval_ms;
    uint32_t           _last_flush_ms;
    uint32_t           _flush_timeout_ms;
    bool               _is_initialized;

    /* Mesh State */
    bool               _mesh_active;
    uint32_t           _net_hash;
    uint16_t           _node_id;
    uint16_t           _mesh_seq;
    lux_transport_t   *_transports[LUX_MAX_TRANSPORTS];
    uint8_t            _transport_count;

    /* Peer Table */
    LuxPeer            _peers[LUX_MAX_PEERS];
    uint8_t            _peer_count;

    /* Handlers */
    lux_handler_slot_t _handlers[LUX_MAX_HANDLERS];
    uint8_t            _handler_count;
    LuxMessageHandler  _catchall_handler;

    /* Debug Monitor */
    bool               _debug_enabled;
    Stream            *_debug_stream;
    lux_debug_level_t  _debug_level;
    uint32_t           _last_debug_table_ms;

    /* Internal Methods */
    void updatePeer(uint16_t node_id, lux_transport_id_t transport_id, int8_t rssi, uint32_t uptime_ms = 0);
    void pruneDeadPeers();
    void dispatchMessage(uint16_t from_node, uint16_t symbol, const uint8_t *payload, uint8_t len);
    lux_status_t emitMeshFrame(uint16_t dst_node, uint16_t symbol, lux_payload_type_t type, const uint8_t *payload, uint8_t len);
    void printDebugFrame(bool is_rx, uint16_t src, uint16_t dst, uint16_t sym, uint8_t type, const uint8_t *payload, uint8_t len, lux_transport_id_t transport_id, uint8_t hop, bool crc_ok);

    static lux_status_t streamWriteCallback(const uint8_t *buf, size_t len, void *ctx);
    static uint32_t     arduinoMicrosSource();
};

extern LuxClass Lux;

#endif /* PYINTEL_LUX_ARDUINO_H */
