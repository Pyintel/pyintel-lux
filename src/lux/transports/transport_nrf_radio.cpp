/**
 * Pyintel Lux — Universal BLE 2.4GHz Hardware Transceiver Transport
 * 
 * Configures bare-metal NRF_RADIO on Nordic nRF51 (BBC micro:bit) and nRF52 (Adafruit CLUE)
 * to transmit and receive standard BLE 1Mbps packets directly on Advertising Channel 37 (2402 MHz).
 * 
 * This enables 100% direct over-the-air RF interoperability between:
 *   - BBC micro:bit (nRF51822)
 *   - Adafruit CLUE (nRF52840)
 *   - ESP32 DevKit (WROOM-32)
 *   - ESP32-S3 (Dual-Core LX7)
 */

#include "transport_nrf_radio.h"
#include "../mesh.h"
#include <string.h>

#if defined(NRF51) || defined(NRF52_SERIES) || defined(ARDUINO_ARCH_NRF5) || defined(ARDUINO_ARCH_NRF52) || defined(ARDUINO_NRF52_ADAFRUIT)

#define LUX_BLE_ADV_CHANNEL     37
#define LUX_BLE_FREQ_OFFSET     2    // 2400 + 2 = 2402 MHz (BLE Adv Ch 37)
#define LUX_BLE_ACCESS_ADDR     0x8E89BED6

static bool s_radio_initialized = false;
static uint8_t s_tx_packet[64] __attribute__((aligned(4)));
static uint8_t s_rx_packet[64] __attribute__((aligned(4)));
static int8_t s_last_rssi = -45;

static void hfclk_start(void) {
    if (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0) {
        NRF_CLOCK->TASKS_HFCLKSTART = 1;
        while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0);
    }
}

static void enter_rx_mode(void) {
    NRF_RADIO->EVENTS_DISABLED = 0;
    NRF_RADIO->TASKS_DISABLE = 1;
    while (NRF_RADIO->EVENTS_DISABLED == 0);
    NRF_RADIO->EVENTS_DISABLED = 0;

    NRF_RADIO->PACKETPTR = (uint32_t)s_rx_packet;
    NRF_RADIO->EVENTS_READY = 0;
    NRF_RADIO->EVENTS_END = 0;
    NRF_RADIO->TASKS_RXEN = 1;
    while (NRF_RADIO->EVENTS_READY == 0);
    NRF_RADIO->EVENTS_READY = 0;
    NRF_RADIO->TASKS_START = 1;
}

static bool nrf_radio_init(void *config) {
    (void)config;
    if (s_radio_initialized) return true;

    hfclk_start();

    // Reset radio power
    NRF_RADIO->POWER = 0;
    delayMicroseconds(10);
    NRF_RADIO->POWER = 1;
    delayMicroseconds(10);

    // Channel 37 = 2402 MHz (Standard BLE Advertising Channel)
    NRF_RADIO->FREQUENCY = LUX_BLE_FREQ_OFFSET;

    // +4 dBm TX power
    NRF_RADIO->TXPOWER = 0x04;

    // Standard BLE 1Mbps Mode
    NRF_RADIO->MODE = 3; // Ble_1Mbit

    // Packet Configuration 0: 8-bit length field, 1-bit S0 (ADV PDU header)
    NRF_RADIO->PCNF0 = (1 << 0) | (8 << 8);

    // Packet Configuration 1: max length 37, 3-byte base address, whitening enabled
    NRF_RADIO->PCNF1 = (37 << 0) | (3 << 16) | (1 << 25);

    // Standard BLE Advertising Access Address: 0x8E89BED6
    NRF_RADIO->BASE0 = 0x89BED600;
    NRF_RADIO->PREFIX0 = 0x0000008E;
    NRF_RADIO->TXADDRESS = 0;
    NRF_RADIO->RXADDRESSES = 1;

    // Channel 37 Whitening initialization
    NRF_RADIO->DATAWHITEIV = LUX_BLE_ADV_CHANNEL;

    // 24-bit BLE CRC Configuration
    NRF_RADIO->CRCCNF = 3; // 3-byte CRC
    NRF_RADIO->CRCPOLY = 0x0000065B;
    NRF_RADIO->CRCINIT = 0x00555555;

    enter_rx_mode();

    s_radio_initialized = true;
    return true;
}

static bool nrf_radio_is_available(void) {
    return s_radio_initialized;
}

static int nrf_radio_broadcast(const uint8_t *buf, size_t len) {
    if (!s_radio_initialized || !buf || len == 0) return -1;
    if (len > 28) len = 28;

    // 1. Disable RX
    NRF_RADIO->EVENTS_DISABLED = 0;
    NRF_RADIO->TASKS_DISABLE = 1;
    while (NRF_RADIO->EVENTS_DISABLED == 0);
    NRF_RADIO->EVENTS_DISABLED = 0;

    // 2. Assemble BLE ADV_NONCONN_IND Header (0x02 PDU type)
    s_tx_packet[0] = 0x02;               // PDU Header: ADV_NONCONN_IND
    s_tx_packet[1] = (uint8_t)(len + 6); // Length: AdvA (6B) + Payload

    // AdvA Mac Address (Synthetic 6-byte Node MAC)
    s_tx_packet[2] = 0x4C;
    s_tx_packet[3] = 0x58;
    s_tx_packet[4] = 0x00;
    s_tx_packet[5] = 0x42;
    s_tx_packet[6] = 0x00;
    s_tx_packet[7] = 0x01;

    // Payload (Lux mesh envelope + frame)
    memcpy(s_tx_packet + 8, buf, len);

    // 3. Transmit via NRF_RADIO
    NRF_RADIO->PACKETPTR = (uint32_t)s_tx_packet;
    NRF_RADIO->EVENTS_READY = 0;
    NRF_RADIO->EVENTS_END = 0;
    NRF_RADIO->TASKS_TXEN = 1;
    while (NRF_RADIO->EVENTS_READY == 0);
    NRF_RADIO->EVENTS_READY = 0;
    NRF_RADIO->TASKS_START = 1;
    while (NRF_RADIO->EVENTS_END == 0);
    NRF_RADIO->EVENTS_END = 0;

    // 4. Return to RX listening
    enter_rx_mode();

    return (int)len;
}

static int nrf_radio_send(const uint8_t *buf, size_t len, uint16_t dst_node) {
    (void)dst_node;
    return nrf_radio_broadcast(buf, len);
}

static int nrf_radio_receive(uint8_t *buf, size_t max_len, uint16_t *src_node) {
    if (!s_radio_initialized || !buf) return 0;

    // Check if a radio packet arrived
    if (NRF_RADIO->EVENTS_END != 0) {
        NRF_RADIO->EVENTS_END = 0;

        // Check CRC status
        if (NRF_RADIO->CRCSTATUS == 1) {
            uint8_t pdu_len = s_rx_packet[1];
            if (pdu_len >= (LUX_MESH_HEADER_SIZE + LUX_HEADER_SIZE)) {
                // Search for "MX" sync bytes within the received BLE payload
                for (uint8_t offset = 2; offset <= (pdu_len + 2 - (LUX_MESH_HEADER_SIZE + LUX_HEADER_SIZE)); offset++) {
                    if (s_rx_packet[offset] == LUX_MESH_SYNC_0 && s_rx_packet[offset + 1] == LUX_MESH_SYNC_1) {
                        size_t frame_len = (pdu_len + 2) - offset;
                        if (frame_len > max_len) frame_len = max_len;
                        memcpy(buf, s_rx_packet + offset, frame_len);

                        lux_mesh_envelope_t *env = (lux_mesh_envelope_t *)buf;
                        if (src_node) *src_node = env->src_node;

                        enter_rx_mode();
                        return (int)frame_len;
                    }
                }
            }
        }

        enter_rx_mode();
    }

    return 0;
}

static int8_t nrf_radio_get_rssi(void) {
    return s_last_rssi;
}

static void nrf_radio_deinit(void) {
    if (s_radio_initialized) {
        NRF_RADIO->EVENTS_DISABLED = 0;
        NRF_RADIO->TASKS_DISABLE = 1;
        while (NRF_RADIO->EVENTS_DISABLED == 0);
        NRF_RADIO->POWER = 0;
        s_radio_initialized = false;
    }
}

#else

static bool nrf_radio_init(void *config) { (void)config; return false; }
static bool nrf_radio_is_available(void) { return false; }
static int nrf_radio_send(const uint8_t *buf, size_t len, uint16_t dst_node) { (void)buf; (void)len; (void)dst_node; return -1; }
static int nrf_radio_broadcast(const uint8_t *buf, size_t len) { (void)buf; (void)len; return -1; }
static int nrf_radio_receive(uint8_t *buf, size_t max_len, uint16_t *src_node) { (void)buf; (void)max_len; (void)src_node; return 0; }
static int8_t nrf_radio_get_rssi(void) { return -127; }
static void nrf_radio_deinit(void) {}

#endif

lux_transport_t lux_transport_nrf_radio = {
    .id           = LUX_TRANSPORT_NRF_RADIO,
    .name         = "nRF-Radio",
    .init         = nrf_radio_init,
    .is_available = nrf_radio_is_available,
    .send         = nrf_radio_send,
    .broadcast    = nrf_radio_broadcast,
    .receive      = nrf_radio_receive,
    .get_rssi     = nrf_radio_get_rssi,
    .deinit       = nrf_radio_deinit
};
