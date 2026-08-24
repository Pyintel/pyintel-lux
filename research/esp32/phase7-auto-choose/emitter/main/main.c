/**
 * Phase 6 — Dynamic Adaptive Transport Mesh Emitter (Free-Range Board A)
 * Continuously evaluates link proximity & RSSI quality:
 * 1. Ultra-Close / Wired Proximity (RX/TX UART)
 * 2. Medium Proximity (ESP-NOW P2P Mesh - No Router, High Speed)
 * 3. Far Distance (Wi-Fi UDP Subnet Broadcast / Router Infrastructure)
 * Emits active transport status symbol (0x0004) & RSSI signal strength (0x0005)
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "lwip/sockets.h"
#include "lux/lux.h"

#define WIFI_SSID       "TINDU"
#define WIFI_PASS       "Bubba#2024"
#define UDP_PORT        4210
#define UDP_HOST_IP     "255.255.255.255"

#define LUX_UART_PORT   UART_NUM_0
#define LUX_UART_BAUD   115200

#define SYM_APP_UPTIME       0x0100
#define SYM_APP_COUNTER      0x0101
#define LUX_SYM_SIGNAL_RSSI  0x0005  /* Custom RSSI telemetry symbol */

static const char *TAG = "lux_adaptive_mesh";

typedef enum {
    TRANSPORT_UART   = 0,
    TRANSPORT_ESPNOW = 1,
    TRANSPORT_UDP    = 2
} active_transport_t;

static active_transport_t s_current_transport = TRANSPORT_ESPNOW;
static uint8_t s_receiver_mac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static int s_udp_sock = -1;
static struct sockaddr_in s_udp_dest;
static bool s_wifi_connected = false;
static int8_t s_last_rssi = -50;
static uint32_t s_espnow_fail_count = 0;

/* ── Transport Dispatcher Callback ──────────────────────────────────── */
static lux_status_t adaptive_transport_write(const uint8_t *buf, size_t len, void *ctx) {
    (void)ctx;
    esp_err_t err;

    switch (s_current_transport) {
        case TRANSPORT_UART:
            uart_write_bytes(LUX_UART_PORT, (const char *)buf, len);
            return LUX_OK;

        case TRANSPORT_ESPNOW:
            err = esp_now_send(s_receiver_mac, buf, len);
            if (err == ESP_OK) {
                s_espnow_fail_count = 0;
                return LUX_OK;
            }
            s_espnow_fail_count++;
            if (s_espnow_fail_count > 3) {
                ESP_LOGW(TAG, "ESP-NOW link degraded! Dynamic switch -> Wi-Fi UDP Broadcast");
                s_current_transport = TRANSPORT_UDP;
            }
            __attribute__((fallthrough));
            /* Fallthrough to UDP if ESP-NOW drops */

        case TRANSPORT_UDP:
        default:
            if (s_wifi_connected && s_udp_sock >= 0) {
                int sent = sendto(s_udp_sock, buf, len, 0, (struct sockaddr *)&s_udp_dest, sizeof(s_udp_dest));
                if (sent >= 0) {
                    return LUX_OK;
                }
            }
            ESP_LOGW(TAG, "UDP broadcast unavailable. Dynamic switch -> UART Direct");
            s_current_transport = TRANSPORT_UART;
            uart_write_bytes(LUX_UART_PORT, (const char *)buf, len);
            return LUX_OK;
    }
}

static uint32_t get_time_us(void) {
    return (uint32_t)(esp_timer_get_time() & 0xFFFFFFFF);
}

static void init_uart(void) {
    uart_config_t uart_cfg = {
        .baud_rate  = LUX_UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(LUX_UART_PORT, &uart_cfg);
    uart_driver_install(LUX_UART_PORT, 512, 512, 0, NULL, 0);
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_wifi_connected = true;
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            s_last_rssi = ap_info.rssi;
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_connected = false;
        s_last_rssi = -95;
    }
}

static void init_wireless(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));
    
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Init ESP-NOW
    if (esp_now_init() == ESP_OK) {
        esp_now_peer_info_t peer_info = {};
        memcpy(peer_info.peer_addr, s_receiver_mac, 6);
        peer_info.channel = 0;
        peer_info.encrypt = false;
        esp_now_add_peer(&peer_info);
    }

    // Init UDP Broadcast socket
    s_udp_dest.sin_addr.s_addr = inet_addr(UDP_HOST_IP);
    s_udp_dest.sin_family = AF_INET;
    s_udp_dest.sin_port = htons(UDP_PORT);
    s_udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (s_udp_sock >= 0) {
        int bcast = 1;
        setsockopt(s_udp_sock, SOL_SOCKET, SO_BROADCAST, &bcast, sizeof(bcast));
    }
}

/* Phase 7: Dynamic Auto-Choose Adaptive Transport Switch */
static void evaluate_adaptive_transport(void) {
    if (s_wifi_connected) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            s_last_rssi = ap_info.rssi;
        }
    }

    /* Dynamic Proximity & Quality Auto-Choose Decision Tree */
    if (s_last_rssi > -60 && s_espnow_fail_count == 0) {
        // High Signal Proximity -> Prefer ESP-NOW Direct P2P Mesh
        s_current_transport = TRANSPORT_ESPNOW;
    } else if (s_wifi_connected) {
        // Range extended / Wi-Fi active -> Use UDP Broadcast
        s_current_transport = TRANSPORT_UDP;
    } else {
        // Wired / Direct Link Fallback -> UART
        s_current_transport = TRANSPORT_UART;
    }
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    init_uart();
    init_wireless();

    lux_ctx_t lux;
    lux_init(&lux, adaptive_transport_write, NULL, get_time_us);

    uint32_t counter = 0;
    while (1) {
        evaluate_adaptive_transport();

        uint32_t uptime_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

        lux_emit_u32(&lux, LUX_SYM_HEARTBEAT, uptime_ms);
        lux_emit_u32(&lux, SYM_APP_COUNTER, counter++);
        
        /* Send active transport mode enum (0=UART, 1=ESP-NOW, 2=UDP) */
        lux_emit_u8(&lux, LUX_SYM_TRANSPORT_SWITCH, (uint8_t)s_current_transport);
        
        /* Send live RSSI signal strength (dBm) */
        lux_emit_i32(&lux, LUX_SYM_SIGNAL_RSSI, (int32_t)s_last_rssi);

        lux_flush(&lux);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
