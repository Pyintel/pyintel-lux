/**
 * Phase 1 — Lux UART Emitter
 * Emits Lux binary frames over UART0 (USB-Serial) once per second.
 * No heap allocation. No printf in the hot path.
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include "lux/lux.h"

/* ── Config ─────────────────────────────────────────────────────────── */
#define LUX_UART_PORT   UART_NUM_0
#define LUX_UART_BAUD   115200
#define LUX_UART_TX     1
#define LUX_UART_RX     3

/* ── User symbol IDs (extend here as you add experiments) ────────────── */
#define SYM_APP_UPTIME   0x0100  /* payload: u32 uptime_ms */
#define SYM_APP_COUNTER  0x0101  /* payload: u32 loop counter */

/* ── Transport write callback ─────────────────────────────────────────── */
static lux_status_t uart_write(const uint8_t *buf, size_t len, void *ctx) {
    (void)ctx;
    int written = uart_write_bytes(LUX_UART_PORT, (const char *)buf, len);
    return (written == (int)len) ? LUX_OK : LUX_ERR_TRANSPORT;
}

/* ── Monotonic clock (µs) ─────────────────────────────────────────────── */
static uint32_t get_time_us(void) {
    return (uint32_t)(esp_timer_get_time() & 0xFFFFFFFF);
}

/* ── App entry ────────────────────────────────────────────────────────── */
void app_main(void) {
    /* UART init */
    uart_config_t uart_cfg = {
        .baud_rate  = LUX_UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(LUX_UART_PORT, &uart_cfg);
    uart_driver_install(LUX_UART_PORT, 256, 256, 0, NULL, 0);

    /* Lux context */
    lux_ctx_t lux;
    lux_init(&lux, uart_write, NULL, get_time_us);

    uint32_t counter = 0;
    while (1) {
        uint32_t uptime_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

        /* Emit heartbeat (system symbol) */
        lux_emit_u32(&lux, LUX_SYM_HEARTBEAT, uptime_ms);

        /* Emit app counter */
        lux_emit_u32(&lux, SYM_APP_COUNTER, counter++);

        /* Flush batch buffer to UART */
        lux_flush(&lux);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
