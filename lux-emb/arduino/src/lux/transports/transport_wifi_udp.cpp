/**
 * Pyintel Lux — Wi-Fi UDP Broadcast Transport Implementation
 */

#include "transport_wifi_udp.h"
#include "../mesh.h"
#include <string.h>

#if defined(ESP32)

#include <WiFi.h>
#include <WiFiUdp.h>

static WiFiUDP s_udp;
static bool s_udp_initialized = false;
static uint16_t s_udp_port = LUX_UDP_PORT;

static bool wifi_udp_init(void *config) {
    if (config) {
        lux_wifi_udp_config_t *cfg = (lux_wifi_udp_config_t *)config;
        if (cfg->port > 0) s_udp_port = cfg->port;

        if (cfg->ssid && strlen(cfg->ssid) > 0) {
            WiFi.mode(WIFI_STA);
            if (cfg->pass && strlen(cfg->pass) > 0) {
                WiFi.begin(cfg->ssid, cfg->pass);
            } else {
                WiFi.begin(cfg->ssid);
            }
        }
    }

    s_udp.begin(s_udp_port);
    s_udp_initialized = true;
    return true;
}

static bool wifi_udp_is_available(void) {
    return s_udp_initialized && (WiFi.status() == WL_CONNECTED);
}

static int wifi_udp_send(const uint8_t *buf, size_t len, uint16_t dst_node) {
    (void)dst_node;
    if (!s_udp_initialized || !buf || len == 0) return -1;

    IPAddress bcast(255, 255, 255, 255);
    s_udp.beginPacket(bcast, s_udp_port);
    size_t written = s_udp.write(buf, len);
    s_udp.endPacket();

    return (written == len) ? (int)len : -1;
}

static int wifi_udp_broadcast(const uint8_t *buf, size_t len) {
    return wifi_udp_send(buf, len, LUX_NODE_BROADCAST);
}

static int wifi_udp_receive(uint8_t *buf, size_t max_len, uint16_t *src_node) {
    if (!s_udp_initialized || !buf) return 0;

    int packet_size = s_udp.parsePacket();
    if (packet_size <= 0) return 0;

    int bytes_read = s_udp.read(buf, max_len);
    if (bytes_read > 0 && src_node && (size_t)bytes_read >= LUX_MESH_HEADER_SIZE) {
        lux_mesh_envelope_t *env = (lux_mesh_envelope_t *)buf;
        if (env->sync[0] == LUX_MESH_SYNC_0 && env->sync[1] == LUX_MESH_SYNC_1) {
            *src_node = env->src_node;
        }
    }

    return bytes_read;
}

static int8_t wifi_udp_get_rssi(void) {
    return (int8_t)WiFi.RSSI();
}

static void wifi_udp_deinit(void) {
    if (s_udp_initialized) {
        s_udp.stop();
        s_udp_initialized = false;
    }
}

#elif defined(ESP8266)

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

static WiFiUDP s_udp;
static bool s_udp_initialized = false;
static uint16_t s_udp_port = LUX_UDP_PORT;

static bool wifi_udp_init(void *config) {
    if (config) {
        lux_wifi_udp_config_t *cfg = (lux_wifi_udp_config_t *)config;
        if (cfg->port > 0) s_udp_port = cfg->port;

        if (cfg->ssid && strlen(cfg->ssid) > 0) {
            WiFi.mode(WIFI_STA);
            if (cfg->pass && strlen(cfg->pass) > 0) {
                WiFi.begin(cfg->ssid, cfg->pass);
            } else {
                WiFi.begin(cfg->ssid);
            }
        }
    }

    s_udp.begin(s_udp_port);
    s_udp_initialized = true;
    return true;
}

static bool wifi_udp_is_available(void) {
    return s_udp_initialized && (WiFi.status() == WL_CONNECTED);
}

static int wifi_udp_send(const uint8_t *buf, size_t len, uint16_t dst_node) {
    (void)dst_node;
    if (!s_udp_initialized || !buf || len == 0) return -1;

    IPAddress bcast(255, 255, 255, 255);
    s_udp.beginPacket(bcast, s_udp_port);
    size_t written = s_udp.write(buf, len);
    s_udp.endPacket();

    return (written == len) ? (int)len : -1;
}

static int wifi_udp_broadcast(const uint8_t *buf, size_t len) {
    return wifi_udp_send(buf, len, LUX_NODE_BROADCAST);
}

static int wifi_udp_receive(uint8_t *buf, size_t max_len, uint16_t *src_node) {
    if (!s_udp_initialized || !buf) return 0;

    int packet_size = s_udp.parsePacket();
    if (packet_size <= 0) return 0;

    int bytes_read = s_udp.read(buf, max_len);
    if (bytes_read > 0 && src_node && (size_t)bytes_read >= LUX_MESH_HEADER_SIZE) {
        lux_mesh_envelope_t *env = (lux_mesh_envelope_t *)buf;
        if (env->sync[0] == LUX_MESH_SYNC_0 && env->sync[1] == LUX_MESH_SYNC_1) {
            *src_node = env->src_node;
        }
    }

    return bytes_read;
}

static int8_t wifi_udp_get_rssi(void) {
    return (int8_t)WiFi.RSSI();
}

static void wifi_udp_deinit(void) {
    if (s_udp_initialized) {
        s_udp.stop();
        s_udp_initialized = false;
    }
}

#else

// Stubs for non-WiFi platforms
static bool wifi_udp_init(void *config) { (void)config; return false; }
static bool wifi_udp_is_available(void) { return false; }
static int wifi_udp_send(const uint8_t *buf, size_t len, uint16_t dst_node) { (void)buf; (void)len; (void)dst_node; return -1; }
static int wifi_udp_broadcast(const uint8_t *buf, size_t len) { (void)buf; (void)len; return -1; }
static int wifi_udp_receive(uint8_t *buf, size_t max_len, uint16_t *src_node) { (void)buf; (void)max_len; (void)src_node; return 0; }
static int8_t wifi_udp_get_rssi(void) { return -127; }
static void wifi_udp_deinit(void) {}

#endif

lux_transport_t lux_transport_wifi_udp = {
    .id           = LUX_TRANSPORT_WIFI_UDP,
    .name         = "WiFi-UDP",
    .init         = wifi_udp_init,
    .is_available = wifi_udp_is_available,
    .send         = wifi_udp_send,
    .broadcast    = wifi_udp_broadcast,
    .receive      = wifi_udp_receive,
    .get_rssi     = wifi_udp_get_rssi,
    .deinit       = wifi_udp_deinit
};
