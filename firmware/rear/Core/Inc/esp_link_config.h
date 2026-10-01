#ifndef ESP_LINK_CONFIG_H
#define ESP_LINK_CONFIG_H

#include "djy_telemetry_protocol.h"

/* Match DJY_EVTLMT/esp32_ev_gateway EV_REAR_UART_BINARY.
 * 1: 238-byte CRC16 telemetry at 460800 8N1, up to 100 Hz.
 * 0: legacy ASCII at 115200 8N1, 5 Hz. Select on BOTH boards.
 * Reverse commands: 9-byte LIVE_TV at 10 Hz and 42-byte PIT_SET on edits.
 * USART1 PA9 TX -> ESP GPIO18 RX; PA10 RX <- ESP GPIO17 TX.
 */
#ifndef ESP_LINK_BINARY_TELEMETRY
#define ESP_LINK_BINARY_TELEMETRY 1
#endif
#if ESP_LINK_BINARY_TELEMETRY == 1
#define ESP_LINK_BAUD DJY_TELEMETRY_BAUD
#elif ESP_LINK_BINARY_TELEMETRY == 0
#define ESP_LINK_BAUD 115200u
#else
#error "ESP_LINK_BINARY_TELEMETRY must be 0 or 1"
#endif

#endif
