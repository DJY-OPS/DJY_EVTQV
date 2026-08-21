#include "tps_sensor.h"
#include "board_config.h"
#include "main.h"

/* CubeMX: ADC1_IN0 = PA0 (board_config.h의 TPS_ADC_PIN), 12-bit, 단일변환 */
extern ADC_HandleTypeDef hadc1;

void TPS_Init(void) {
    /* 채널이 board_config.h와 일치하는지 명시적으로 한 번 세팅.
     * (CubeMX에서 이미 설정돼 있어도 무해하며, 핀 변경 시 누락을 막는다.) */
    ADC_ChannelConfTypeDef ch = {0};
    ch.Channel      = TPS_ADC_CHANNEL;
    ch.Rank         = 1;
    ch.SamplingTime = ADC_SAMPLETIME_84CYCLES;
    HAL_ADC_ConfigChannel(&hadc1, &ch);

    /* F446은 별도 SW 캘리브레이션 불필요. 첫 변환 안정화용 더미 1회. */
    (void)TPS_ReadRaw();
}

uint16_t TPS_ReadRaw(void) {
    /* DialInputs shares ADC1 and changes the regular channel. Always restore
     * TPS IN0 here so a dial read can never become the throttle sample. */
    ADC_ChannelConfTypeDef ch = {0};
    ch.Channel = TPS_ADC_CHANNEL;
    ch.Rank = 1;
    ch.SamplingTime = ADC_SAMPLETIME_84CYCLES;
    if (HAL_ADC_ConfigChannel(&hadc1, &ch) != HAL_OK) return 0u;
    uint16_t v = 0;
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 2) == HAL_OK) {
        v = (uint16_t)HAL_ADC_GetValue(&hadc1);
    }
    HAL_ADC_Stop(&hadc1);
    return v;   /* 변환 실패 시 0 → TPS_IsValid에서 fault 처리 */
}

bool TPS_IsValid(uint16_t raw) {
    return (raw >= (TPS_ADC_MIN - TPS_ADC_MARGIN)) &&
           (raw <= (TPS_ADC_MAX + TPS_ADC_MARGIN));
}
