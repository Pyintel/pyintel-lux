/**
 * Pyintel Lux — ESP-NOW P2P Wireless Transport Implementation
 */

#include "transport_espnow.h"
#include "../mesh.h"
#include <string.h>

#if defined(ESP32)

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define ESPNOW_QUEUE_SIZE 4
#define ESPNOW_MAX_PKT_LEN 160

typedef struct {
    uint8_t  data[ESPNOW_MAX_PKT_LEN];
    size_t   len;
    uint8_t  src_mac[6];
} espnow_packet_t;

static espnow_packet_t s_espnow_queue[ESPNOW_QUEUE_SIZE];
static volatile uint8_t s_queue_head = 0;
static volatile uint8_t s_queue_tail = 0;
static volatile uint8_t s_queue_count = 0;
static bool s_espnow_initialized = false;
static uint8_t s_bcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#if ESP_ARDUINO_VERSION_MAJOR >= 3
static void on_esp_now_recv(const esp_now_recv_info_t *info, const uint8_t *data, int data_len) {
    if (!data || data_len <= 0 || data_len > ESPNOW_MAX_PKT_LEN) return;
    if (s_queue_count >= ESPNOW_QUEUE_SIZE) return; // Drop on overflow

    uint8_t head = s_queue_head;
    memcpy(s_espnow_queue[head].data, data, (size_t)data_len);
    s_espnow_queue[head].len = (size_t)data_len;
    if (info && info->src_addr) {
        memcpy(s_espnow_queue[head].src_mac, info->src_addr, 6);
    }
    s_queue_head = (head + 1) % ESPNOW_QUEUE_SIZE;
    s_queue_count++;
}
#else
static void on_esp_now_recv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
    if (!data || data_len <= 0 || data_len > ESPNOW_MAX_PKT_LEN) return;
    if (s_queue_count >= ESPNOW_QUEUE_SIZE) return;

    uint8_t head = s_queue_head;
    memcpy(s_espnow_queue[head].data, data, (size_t)data_len);
    s_espnow_queue[head].len = (size_t)data_len;
    if (mac_addr) {
        memcpy(s_espnow_queue[head].src_mac, mac_addr, 6);
    }
    s_queue_head = (head + 1) % ESPNOW_QUEUE_SIZE;
    s_queue_count++;
}
#endif

static bool espnow_init(void *config) {
    (void)config;
    if (s_espnow_initialized) return true;

    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        return false;
    }

    esp_now_peer_info_t peer_info;
    memset(&peer_info, 0, sizeof(peer_info));
    memcpy(peer_info.peer_addr, s_bcast_mac, 6);
    peer_info.channel = 0;
    peer_info.encrypt = false;
    esp_now_add_peer(&peer_info);

#if ESP_ARDUINO_VERSION_MAJOR >= 3
    esp_now_register_recv_cb(on_esp_now_recv);
#else
    esp_now_register_recv_cb(on_esp_now_recv);
#endif

    s_queue_head = 0;
    s_queue_tail = 0;
    s_queue_count = 0;
    s_espnow_initialized = true;
    return true;
}

static bool espnow_is_available(void) {
    return s_espnow_initialized;
}

static int espnow_send(const uint8_t *buf, size_t len, uint16_t dst_node) {
    (void)dst_node;
    if (!s_espnow_initialized || !buf || len == 0) return -1;
    esp_err_t err = esp_now_send(s_bcast_mac, buf, len);
    return (err == ESP_OK) ? (int)len : -1;
}

static int espnow_broadcast(const uint8_t *buf, size_t len) {
    return espnow_send(buf, len, LUX_NODE_BROADCAST);
}

static int espnow_receive(uint8_t *buf, size_t max_len, uint16_t *src_node) {
    if (!s_espnow_initialized || s_queue_count == 0 || !buf) return 0;

    uint8_t tail = s_queue_tail;
    size_t copy_len = s_espnow_queue[tail].len;
    if (copy_len > max_len) copy_len = max_len;

    memcpy(buf, s_espnow_queue[tail].data, copy_len);

    if (src_node && copy_len >= LUX_MESH_HEADER_SIZE) {
        lux_mesh_envelope_t *env = (lux_mesh_envelope_t *)s_espnow_queue[tail].data;
        if (env->sync[0] == LUX_MESH_SYNC_0 && env->sync[1] == LUX_MESH_SYNC_1) {
            *src_node = env->src_node;
        }
    }

    s_queue_tail = (tail + 1) % ESPNOW_QUEUE_SIZE;
    s_queue_count--;
    return (int)copy_len;
}

static int8_t espnow_get_rssi(void) {
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        return ap_info.rssi;
    }
    return -45; // Nominal P2P RSSI estimate
}

static void espnow_deinit(void) {
    if (s_espnow_initialized) {
        esp_now_deinit();
        s_espnow_initialized = false;
    }
}

#elif defined(ESP8266)

#include <ESP8266WiFi.h>
#include <espnow.h>

#define ESPNOW_QUEUE_SIZE 4
#define ESPNOW_MAX_PKT_LEN 160

typedef struct {
    uint8_t  data[ESPNOW_MAX_PKT_LEN];
    size_t   len;
} espnow_packet_t;

static espnow_packet_t s_espnow_queue[ESPNOW_QUEUE_SIZE];
static volatile uint8_t s_queue_head = 0;
static volatile uint8_t s_queue_tail = 0;
static volatile uint8_t s_queue_count = 0;
static bool s_espnow_initialized = false;
static uint8_t s_bcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static void on_esp_now_recv_8266(uint8_t *mac_addr, uint8_t *data, uint8_t data_len) {
    (void)mac_addr;
    if (!data || data_len <= 0 || data_len > ESPNOW_MAX_PKT_LEN) return;
    if (s_queue_count >= ESPNOW_QUEUE_SIZE) return;

    uint8_t head = s_queue_head;
    memcpy(s_espnow_queue[head].data, data, data_len);
    s_espnow_queue[head].len = data_len;
    s_queue_head = (head + 1) % ESPNOW_QUEUE_SIZE;
    s_queue_count++;
}

static bool espnow_init(void *config) {
    (void)config;
    if (s_espnow_initialized) return true;

    WiFi.mode(WIFI_STA);
    if (esp_now_init() != 0) return false;

    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_add_peer(s_bcast_mac, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
    esp_now_register_recv_cb(on_esp_now_recv_8266);

    s_queue_head = 0;
    s_queue_tail = 0;
    s_queue_count = 0;
    s_espnow_initialized = true;
    return true;
}

static bool espnow_is_available(void) {
    return s_espnow_initialized;
}

static int espnow_send(const uint8_t *buf, size_t len, uint16_t dst_node) {
    (void)dst_node;
    if (!s_espnow_initialized || !buf || len == 0) return -1;
    int res = esp_now_send(s_bcast_mac, (uint8_t *)buf, len);
    return (res == 0) ? (int)len : -1;
}

static int espnow_broadcast(const uint8_t *buf, size_t len) {
    return espnow_send(buf, len, LUX_NODE_BROADCAST);
}

static int espnow_receive(uint8_t *buf, size_t max_len, uint16_t *src_node) {
    if (!s_espnow_initialized || s_queue_count == 0 || !buf) return 0;

    uint8_t tail = s_queue_tail;
    size_t copy_len = s_espnow_queue[tail].len;
    if (copy_len > max_len) copy_len = max_len;

    memcpy(buf, s_espnow_queue[tail].data, copy_len);

    if (src_node && copy_len >= LUX_MESH_HEADER_SIZE) {
        lux_mesh_envelope_t *env = (lux_mesh_envelope_t *)s_espnow_queue[tail].data;
        if (env->sync[0] == LUX_MESH_SYNC_0 && env->sync[1] == LUX_MESH_SYNC_1) {
            *src_node = env->src_node;
        }
    }

    s_queue_tail = (tail + 1) % ESPNOW_QUEUE_SIZE;
    s_queue_count--;
    return (int)copy_len;
}

static int8_t espnow_get_rssi(void) {
    return (int8_t)WiFi.RSSI();
}

static void espnow_deinit(void) {
    if (s_espnow_initialized) {
        esp_now_deinit();
        s_espnow_initialized = false;
    }
}

#else

// Stubs for non-ESP platforms
static bool espnow_init(void *config) { (void)config; return false; }
static bool espnow_is_available(void) { return false; }
static int espnow_send(const uint8_t *buf, size_t len, uint16_t dst_node) { (void)buf; (void)len; (void)dst_node; return -1; }
static int espnow_broadcast(const uint8_t *buf, size_t len) { (void)buf; (void)len; return -1; }
static int espnow_receive(uint8_t *buf, size_t max_len, uint16_t *src_node) { (void)buf; (void)max_len; (void)src_node; return 0; }
static int8_t espnow_get_rssi(void) { return -127; }
static void espnow_deinit(void) {}

#endif

lux_transport_t lux_transport_espnow = {
    .id           = LUX_TRANSPORT_ESPNOW,
    .name         = "ESP-NOW",
    .init         = espnow_init,
    .is_available = espnow_is_available,
    .send         = espnow_send,
    .broadcast    = espnow_broadcast,
    .receive      = espnow_receive,
    .get_rssi     = espnow_get_rssi,
    .deinit       = espnow_deinit
};
