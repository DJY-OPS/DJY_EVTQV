#ifndef DIAL_INPUTS_H
#define DIAL_INPUTS_H

#include <stdint.h>

/* Two 10 kOhm linear potentiometers:
 * PC0 (A5) = TQV strength, PC1 (A4) = regen request.
 * Outer legs go to 3V3/GND, wiper goes to the STM pin. */
void DialInputs_Init(void);
void DialInputs_Update10ms(void);
uint8_t DialInputs_GetTvPercent(void);
uint8_t DialInputs_GetRegenPercent(void);

#endif

