/**
 * Phase 2 — Lux UDP Emitter over Wi-Fi
 * Connects to Wi-Fi network and streams Lux binary frames over UDP to broadcast/host PC.
 * Zero-heap frame formatting, identical to Phase 1. Only transport callback changed.
 */

#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "esp_timer.h"
#include "lux/lux.h"

/* ── Wi-Fi & UDP Configuration ────────────────────────────────────────── */
#define WIFI_SSID       "TINDU"
#define WIFI_PASS       "Bubba#2024"
#define UDP_PORT        4210
#define UDP_HOST_IP     "255.255.255.255"  /* UDP Broadcast to subnet */

#define SYM_APP_UPTIME   0x0100
#define SYM_APP_COUNTER  0x0101

static const char *TAG = "lux_phase2_udp";
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static int s_sock = -1;
static struct sockaddr_in s_dest_addr;

/* ── Transport write callback (UDP socket) ────────────────────────────── */
static lux_status_t udp_write(const uint8_t *buf, size_t len, void *ctx) {
    (void)ctx;
    if (s_sock < 0) {
        return LUX_ERR_TRANSPORT;
    }
    int err = sendto(s_sock, buf, len, 0, (struct sockaddr *)&s_dest_addr, sizeof(s_dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Error occurred during sendto: errno %d", errno);
        return LUX_ERR_TRANSPORT;
    }
    return LUX_OK;
}

/* ── Monotonic clock (µs) ─────────────────────────────────────────────── */
static uint32_t get_time_us(void) {
    return (uint32_t)(esp_timer_get_time() & 0xFFFFFFFF);
}

/* ── Wi-Fi Event Handler ──────────────────────────────────────────────── */
static void event_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Retrying Wi-Fi connection...");
        esp_wifi_connect();
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

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

    ESP_LOGI(TAG, "wifi_init_sta finished. Connecting to SSID: %s", WIFI_SSID);

    /* Waiting until connection is established */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP SSID:%s", WIFI_SSID);
    }
}

/* ── App entry ────────────────────────────────────────────────────────── */
void app_main(void) {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Starting Lux Phase 2 UDP Emitter...");
    wifi_init_sta();

    // Create UDP Socket
    s_dest_addr.sin_addr.s_addr = inet_addr(UDP_HOST_IP);
    s_dest_addr.sin_family = AF_INET;
    s_dest_addr.sin_port = htons(UDP_PORT);

    s_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (s_sock < 0) {
        ESP_LOGE(TAG, "Unable to create UDP socket: errno %d", errno);
        return;
    }

    // Enable Broadcast on Socket
    int broadcast = 1;
    setsockopt(s_sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    ESP_LOGI(TAG, "UDP Socket created, broadcasting to port %d", UDP_PORT);

    /* Lux context init with UDP callback */
    lux_ctx_t lux;
    lux_init(&lux, udp_write, NULL, get_time_us);

    uint32_t counter = 0;
    while (1) {
        uint32_t uptime_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

        /* Emit heartbeat */
        lux_emit_u32(&lux, LUX_SYM_HEARTBEAT, uptime_ms);

        /* Emit app counter */
        lux_emit_u32(&lux, SYM_APP_COUNTER, counter++);

        /* Flush batched frame packet to UDP */
        lux_flush(&lux);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
