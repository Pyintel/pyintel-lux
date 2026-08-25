/**
 * Phase 6 — Board B (ESP32 Wire Relay Receiver)
 * Reads incoming Lux binary bytes from GPIO 16 (RX) over physical jumper wire,
 * and relays them directly to UART0 (USB port COM21) for Python server parsing.
 */

#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "nvs_flash.h"

#define WIRE_UART_PORT   UART_NUM_2
#define USB_UART_PORT    UART_NUM_0
#define LUX_BAUD         115200

#define WIRE_RX_PIN      16
#define WIRE_TX_PIN      17

static const char *TAG = "lux_wire_relay";

static void init_uarts(void) {
    /* 1. USB UART (Port 0 -> PC COM21) */
    uart_config_t usb_cfg = {
        .baud_rate  = LUX_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(USB_UART_PORT, &usb_cfg);
    uart_driver_install(USB_UART_PORT, 1024, 1024, 0, NULL, 0);

    /* 2. Wire Jumper UART (Port 2 -> GPIO 16 RX / GPIO 17 TX) */
    uart_config_t wire_cfg = {
        .baud_rate  = LUX_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_driver_install(WIRE_UART_PORT, 1024, 1024, 0, NULL, 0);
    uart_param_config(WIRE_UART_PORT, &wire_cfg);
    uart_set_pin(WIRE_UART_PORT, WIRE_TX_PIN, WIRE_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    ESP_LOGI(TAG, "Wire Relay Ready: Jumper RX (GPIO%d) -> USB COM21", WIRE_RX_PIN);
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    init_uarts();

    /* Suppress text logs on USB UART so only pure binary Lux frames reach PC */
    esp_log_level_set("*", ESP_LOG_NONE);

    uint8_t rx_buf[256];
    while (1) {
        /* Read raw bytes coming from physical jumper wire (GPIO 16) */
        int len = uart_read_bytes(WIRE_UART_PORT, rx_buf, sizeof(rx_buf), pdMS_TO_TICKS(10));
        if (len > 0) {
            /* Relay raw binary immediately out to PC USB (COM21) */
            uart_write_bytes(USB_UART_PORT, (const char *)rx_buf, len);
        }
    }
}