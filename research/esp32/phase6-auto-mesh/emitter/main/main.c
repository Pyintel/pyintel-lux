/**
 * Phase 6 — Dynamic Transport Fallback & Selection Engine (Board A Emitter)
 * Evaluates transport availability in real time:
 * 1. Primary: ESP-NOW Direct P2P Wireless Mesh (Fastest, zero infrastructure)
 * 2. Secondary: Wi-Fi UDP Broadcast (Local Network)
 * 3. Fallback: UART Serial Output (Wired fail-safe)
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

#define SYM_APP_UPTIME   0x0100
#define SYM_APP_COUNTER  0x0101

static const char *TAG = "lux_phase6_auto";

typedef enum {
    TRANSPORT_ESPNOW = 0,
    TRANSPORT_UDP    = 1,
    TRANSPORT_UART   = 2
} active_transport_t;

static active_transport_t s_current_transport = TRANSPORT_ESPNOW;
static uint8_t s_receiver_mac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; // Broadcast peer
static int s_udp_sock = -1;
static struct sockaddr_in s_udp_dest;
static bool s_wifi_connected = false;

/* ── Transport Callbacks ────────────────────────────────────────────── */
static lux_status_t auto_transport_write(const uint8_t *buf, size_t len, void *ctx) {
    (void)ctx;
    esp_err_t err;

    switch (s_current_transport) {
        case TRANSPORT_ESPNOW:
            err = esp_now_send(s_receiver_mac, buf, len);
            if (err == ESP_OK) {
                return LUX_OK;
            }
            ESP_LOGW(TAG, "ESP-NOW write failed (%d). Falling back to UDP...", err);
            s_current_transport = TRANSPORT_UDP;
            /* Fallthrough to UDP */

        case TRANSPORT_UDP:
            if (s_wifi_connected && s_udp_sock >= 0) {
                int sent = sendto(s_udp_sock, buf, len, 0, (struct sockaddr *)&s_udp_dest, sizeof(s_udp_dest));
                if (sent >= 0) {
                    return LUX_OK;
                }
            }
            ESP_LOGW(TAG, "UDP write failed. Falling back to UART...");
            s_current_transport = TRANSPORT_UART;
            /* Fallthrough to UART */

        case TRANSPORT_UART:
        default:
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
        ESP_LOGI(TAG, "Wi-Fi Connected for UDP transport fallback");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_connected = false;
    }
}

static void init_wireless(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Init ESP-NOW
    if (esp_now_init() == ESP_OK) {
        esp_now_peer_info_t peer_info = {};
        memcpy(peer_info.peer_addr, s_receiver_mac, 6);
        peer_info.channel = 0;
        peer_info.encrypt = false;
        esp_now_add_peer(&peer_info);
        ESP_LOGI(TAG, "Primary Transport active: ESP-NOW P2P Mesh");
    }

    // Prepare UDP socket fallback
    s_udp_dest.sin_addr.s_addr = inet_addr(UDP_HOST_IP);
    s_udp_dest.sin_family = AF_INET;
    s_udp_dest.sin_port = htons(UDP_PORT);
    s_udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (s_udp_sock >= 0) {
        int bcast = 1;
        setsockopt(s_udp_sock, SOL_SOCKET, SO_BROADCAST, &bcast, sizeof(bcast));
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
    lux_init(&lux, auto_transport_write, NULL, get_time_us);

    uint32_t counter = 0;
    while (1) {
        uint32_t uptime_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

        /* Emit system heartbeat & app counter */
        lux_emit_u32(&lux, LUX_SYM_HEARTBEAT, uptime_ms);
        lux_emit_u32(&lux, SYM_APP_COUNTER, counter++);

        /* Emit transport switch status symbol */
        lux_emit_u8(&lux, LUX_SYM_TRANSPORT_SWITCH, (uint8_t)s_current_transport);

        /* Flush batch payload to active dynamic transport */
        lux_flush(&lux);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
