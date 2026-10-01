#ifndef PIT_TUNING_H
#define PIT_TUNING_H
#include "djy_pit_protocol.h"
void PitTuning_Init(void);
void PitTuning_Queue(const DjyPitPacket *packet);
/* Called only in the 100 Hz control ISR, after sensor updates. */
void PitTuning_Tick(bool stationary, uint32_t now);
DjyPitPacket PitTuning_Status(void);
#endif
