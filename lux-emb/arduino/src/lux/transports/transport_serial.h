/**
 * Pyintel Lux — Serial (UART / USB-CDC) Transport Driver
 * Provides streaming binary mesh transport over standard Arduino Stream.
 */

#ifndef LUX_TRANSPORT_SERIAL_H
#define LUX_TRANSPORT_SERIAL_H

#include <Arduino.h>
#include "../transport.h"

#ifdef __cplusplus
extern "C" {
#endif

extern lux_transport_t lux_transport_serial;

void lux_transport_serial_set_stream(Stream *stream);

#ifdef __cplusplus
}
#endif

#endif /* LUX_TRANSPORT_SERIAL_H */
