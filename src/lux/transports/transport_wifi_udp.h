/**
 * Pyintel Lux — Wi-Fi UDP Broadcast Transport Driver
 * Multi-board LAN broadcast transport over UDP port 4210.
 */

#ifndef LUX_TRANSPORT_WIFI_UDP_H
#define LUX_TRANSPORT_WIFI_UDP_H

#include "../transport.h"

#define LUX_UDP_PORT 4210

typedef struct {
    const char *ssid;
    const char *pass;
    uint16_t    port;
} lux_wifi_udp_config_t;

#ifdef __cplusplus
extern "C" {
#endif

extern lux_transport_t lux_transport_wifi_udp;

#ifdef __cplusplus
}
#endif

#endif /* LUX_TRANSPORT_WIFI_UDP_H */
