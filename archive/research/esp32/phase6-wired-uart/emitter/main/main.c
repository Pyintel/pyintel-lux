/**
 * Phase 6 — Dedicated Wired Proximity RX/TX UART Serial Link
 * Emits binary Lux protocol frames continuously over physical RX/TX cable link.
 */

#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "lux/lux.h"

#define LUX_UART_PORT   UART_NUM_0
#define LUX_UART_BAUD   115200

#define SYM_APP_UPTIME       0x0100
#define SYM_APP_COUNTER      0x0101
#define LUX_SYM_SIGNAL_RSSI  0x0005  /* Signal RSSI telemetry symbol */

static const char *TAG = "lux_wired_uart";

typedef enum {
    TRANSPORT_UART   = 0,
    TRANSPORT_ESPNOW = 1,
    TRANSPORT_UDP    = 2
} active_transport_t;

static active_transport_t s_current_transport = TRANSPORT_UART;

/* ── Transport Dispatcher Callback ──────────────────────────────────── */
static lux_status_t uart_transport_write(const uint8_t *buf, size_t len, void *ctx) {
    (void)ctx;
    uart_write_bytes(LUX_UART_PORT, (const char *)buf, len);
    uart_wait_tx_done(LUX_UART_PORT, pdMS_TO_TICKS(100));
    return LUX_OK;
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
    uart_driver_install(LUX_UART_PORT, 1024, 1024, 0, NULL, 0);
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    init_uart();

    /* Suppress text log messages on UART0 so only pure binary Lux frames transmit */
    esp_log_level_set("*", ESP_LOG_NONE);

    lux_ctx_t lux;
    lux_init(&lux, uart_transport_write, NULL, get_time_us);

    uint32_t counter = 0;
    while (1) {
        uint32_t uptime_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

        lux_emit_u32(&lux, LUX_SYM_HEARTBEAT, uptime_ms);
        lux_emit_u32(&lux, SYM_APP_COUNTER, counter++);
        
        /* Send active transport mode enum (0=UART) */
        lux_emit_u8(&lux, LUX_SYM_TRANSPORT_SWITCH, (uint8_t)s_current_transport);
        
        /* Direct Wired link indicator (0 dBm = physical cable link) */
        lux_emit_i32(&lux, LUX_SYM_SIGNAL_RSSI, 0);

        lux_flush(&lux);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
