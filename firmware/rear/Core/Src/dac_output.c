#include "dac_output.h"
#include "board_config.h"
#include "vehicle_params.h"
#include "common_types.h"
#include "main.h"

/* CubeMX: DAC1 OUT1=PA4(좌), OUT2=PA5(우), 출력버퍼 Enable, 트리거 None
 * 채널 번호는 board_config.h의 DAC_LEFT_CHANNEL / DAC_RIGHT_CHANNEL 사용 */
extern DAC_HandleTypeDef hdac;

static uint16_t volt_to_code(float v) {
    float c = v / DAC_VREF * (float)DAC_RESOLUTION;
    return (uint16_t)CLAMP(c, 0.0f, (float)DAC_RESOLUTION);
}

void DAC_Output_Init(void) {
    HAL_DAC_Start(&hdac, DAC_LEFT_CHANNEL);
    HAL_DAC_Start(&hdac, DAC_RIGHT_CHANNEL);
    DAC_SetSafeState();
}

void DAC_SetLeftThrottle(uint16_t dac_code) {
    HAL_DAC_SetValue(&hdac, DAC_LEFT_CHANNEL, DAC_ALIGN_12B_R,
                     CLAMP(dac_code, 0, DAC_RESOLUTION));
}
void DAC_SetRightThrottle(uint16_t dac_code) {
    HAL_DAC_SetValue(&hdac, DAC_RIGHT_CHANNEL, DAC_ALIGN_12B_R,
                     CLAMP(dac_code, 0, DAC_RESOLUTION));
}
void DAC_SetSafeState(void) {
    uint16_t off = volt_to_code(V_THROTTLE_OFF);   /* 0.90V → 모터 오프 */
    HAL_DAC_SetValue(&hdac, DAC_LEFT_CHANNEL,  DAC_ALIGN_12B_R, off);
    HAL_DAC_SetValue(&hdac, DAC_RIGHT_CHANNEL, DAC_ALIGN_12B_R, off);
}
