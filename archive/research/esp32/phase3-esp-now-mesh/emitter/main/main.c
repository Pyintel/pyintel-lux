/**
 * Phase 3 — Lux ESP-NOW Emitter (Board A)
 * Transmits Lux binary frames directly to Board B over connectionless 802.11 ESP-NOW.
 * Zero-heap frame assembly, identical to Phase 1 and 2.
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "lux/lux.h"

#define SYM_APP_UPTIME   0x0100
#define SYM_APP_COUNTER  0x0101

static const char *TAG = "lux_phase3_emitter";

/* Replace with Board B's actual MAC address! Broadcast MAC {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF} works by default */
static uint8_t s_receiver_mac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

/* ── Transport write callback (ESP-NOW) ──────────────────────────────── */
static lux_status_t esp_now_write(const uint8_t *buf, size_t len, void *ctx) {
    (void)ctx;
    esp_err_t err = esp_now_send(s_receiver_mac, buf, len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW send failed: %d", err);
        return LUX_ERR_TRANSPORT;
    }
    return LUX_OK;
}

static uint32_t get_time_us(void) {
    return (uint32_t)(esp_timer_get_time() & 0xFFFFFFFF);
}

static void wifi_espnow_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* Initialize ESP-NOW */
    ESP_ERROR_CHECK(esp_now_init());

    /* Register peer */
    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, s_receiver_mac, 6);
    peer_info.channel = 0;
    peer_info.encrypt = false;

    if (esp_now_add_peer(&peer_info) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ESP-NOW peer!");
    } else {
        ESP_LOGI(TAG, "ESP-NOW Peer registered successfully.");
    }
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Starting Lux Phase 3 ESP-NOW Emitter...");
    wifi_espnow_init();

    lux_ctx_t lux;
    lux_init(&lux, esp_now_write, NULL, get_time_us);

    uint32_t counter = 0;
    while (1) {
        uint32_t uptime_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

        lux_emit_u32(&lux, LUX_SYM_HEARTBEAT, uptime_ms);
        lux_emit_u32(&lux, SYM_APP_COUNTER, counter++);
        lux_flush(&lux);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
