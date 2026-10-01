#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef struct { uint32_t front_us, received_us; uint16_t span_us, sequence; bool timestamped; } SensorTiming_t;
extern uint32_t g_rx_us;
static inline SensorTiming_t CAN_GetSensorTiming(void) { SensorTiming_t t = {0, g_rx_us, 0, 0, true}; return t; }
