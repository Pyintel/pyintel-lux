/**
 * Pyintel Lux — Serial (UART / USB-CDC) Transport Implementation
 */

#include "transport_serial.h"
#include "../mesh.h"

static Stream *s_serial_stream = &Serial;
static uint8_t s_rx_buf[LUX_MAX_PAYLOAD_SIZE + LUX_MESH_HEADER_SIZE + LUX_HEADER_SIZE];
static size_t  s_rx_index = 0;
static size_t  s_expected_len = 0;
static bool    s_in_sync = false;

void lux_transport_serial_set_stream(Stream *stream) {
    if (stream) {
        s_serial_stream = stream;
    }
}

static bool serial_init(void *config) {
    if (config) {
        s_serial_stream = (Stream *)config;
    }
    s_rx_index = 0;
    s_expected_len = 0;
    s_in_sync = false;
    return (s_serial_stream != nullptr);
}

static bool serial_is_available(void) {
    return (s_serial_stream != nullptr);
}

static int serial_send(const uint8_t *buf, size_t len, uint16_t dst_node) {
    (void)dst_node;
    if (!s_serial_stream || !buf || len == 0) return -1;
    size_t written = s_serial_stream->write(buf, len);
    return (int)written;
}

static int serial_broadcast(const uint8_t *buf, size_t len) {
    return serial_send(buf, len, LUX_NODE_BROADCAST);
}

static int serial_receive(uint8_t *buf, size_t max_len, uint16_t *src_node) {
    if (!s_serial_stream || !buf) return 0;

    while (s_serial_stream->available() > 0) {
        uint8_t b = (uint8_t)s_serial_stream->read();

        if (!s_in_sync) {
            if (s_rx_index == 0) {
                if (b == LUX_MESH_SYNC_0 || b == LUX_SYNC_0) {
                    s_rx_buf[0] = b;
                    s_rx_index = 1;
                }
            } else if (s_rx_index == 1) {
                if ((s_rx_buf[0] == LUX_MESH_SYNC_0 && b == LUX_MESH_SYNC_1) ||
                    (s_rx_buf[0] == LUX_SYNC_0 && b == LUX_SYNC_1)) {
                    s_rx_buf[1] = b;
                    s_rx_index = 2;
                    s_in_sync = true;
                    s_expected_len = 0;
                } else {
                    s_rx_index = 0;
                }
            }
            continue;
        }

        // We are in sync, accumulate into buffer
        if (s_rx_index < sizeof(s_rx_buf)) {
            s_rx_buf[s_rx_index++] = b;
        } else {
            // Buffer overflow - reset sync
            s_in_sync = false;
            s_rx_index = 0;
            continue;
        }

        // Calculate expected frame length once we have headers
        if (s_expected_len == 0) {
            if (s_rx_buf[0] == LUX_MESH_SYNC_0) {
                // Mesh frame: Mesh header (12) + Inner Lux Header (14)
                if (s_rx_index >= (LUX_MESH_HEADER_SIZE + LUX_HEADER_SIZE)) {
                    uint8_t inner_payload_len = s_rx_buf[LUX_MESH_HEADER_SIZE + 11];
                    s_expected_len = LUX_MESH_HEADER_SIZE + LUX_HEADER_SIZE + inner_payload_len;
                }
            } else {
                // Raw Lux frame: 14 bytes header + payload
                if (s_rx_index >= LUX_HEADER_SIZE) {
                    uint8_t payload_len = s_rx_buf[11];
                    s_expected_len = LUX_HEADER_SIZE + payload_len;
                }
            }
        }

        // Full packet assembled
        if (s_expected_len > 0 && s_rx_index >= s_expected_len) {
            size_t total = s_expected_len;
            if (total > max_len) total = max_len;
            memcpy(buf, s_rx_buf, total);

            if (src_node && s_rx_buf[0] == LUX_MESH_SYNC_0) {
                lux_mesh_envelope_t *env = (lux_mesh_envelope_t *)s_rx_buf;
                *src_node = env->src_node;
            }

            // Reset state machine for next frame
            s_in_sync = false;
            s_rx_index = 0;
            s_expected_len = 0;
            return (int)total;
        }
    }

    return 0;
}

static int8_t serial_get_rssi(void) {
    return 0; // Wired link has 0 dBm attenuation
}

static void serial_deinit(void) {
    s_rx_index = 0;
    s_in_sync = false;
}

lux_transport_t lux_transport_serial = {
    .id           = LUX_TRANSPORT_SERIAL,
    .name         = "Serial",
    .init         = serial_init,
    .is_available = serial_is_available,
    .send         = serial_send,
    .broadcast    = serial_broadcast,
    .receive      = serial_receive,
    .get_rssi     = serial_get_rssi,
    .deinit       = serial_deinit
};
