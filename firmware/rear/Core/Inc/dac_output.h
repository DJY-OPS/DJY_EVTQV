#ifndef DAC_OUTPUT_H
#define DAC_OUTPUT_H
#include <stdint.h>

void DAC_Output_Init(void);
void DAC_SetLeftThrottle(uint16_t dac_code);   /* DAC1_OUT1, PA4 */
void DAC_SetRightThrottle(uint16_t dac_code);  /* DAC1_OUT2, PA5 (LD2와 공유핀) */
void DAC_SetSafeState(void);                   /* 양측 오프(0.90V) */

#endif /* DAC_OUTPUT_H */
