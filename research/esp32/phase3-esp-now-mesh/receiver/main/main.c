/**
 * Phase 3 — Lux ESP-NOW Receiver & UART Relay (Board B)
 * Receives Lux binary frames wirelessly via ESP-NOW from Board A,
 * and forwards raw binary bytes immediately to UART0 (USB-Serial) for host decoding.
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

#define LUX_UART_PORT   UART_NUM_0
#define LUX_UART_BAUD   115200

static const char *TAG = "lux_phase3_receiver";

/* ── ESP-NOW Receive Callback ────────────────────────────────────────── */
static void esp_now_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    (void)recv_info;
    if (len > 0) {
        /* Relay raw incoming Lux binary frame bytes directly to UART */
        uart_write_bytes(LUX_UART_PORT, (const char *)data, len);
    }
}

static void uart_init(void) {
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

static void wifi_espnow_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(esp_now_recv_cb));
    ESP_LOGI(TAG, "ESP-NOW Receiver ready & listening for frames...");
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    uart_init();
    wifi_espnow_init();
    ESP_LOGI(TAG, "Lux Phase 3 Relay active: ESP-NOW Wireless → UART0 Output");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
