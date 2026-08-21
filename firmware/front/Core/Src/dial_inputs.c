#include "dial_inputs.h"
#include "driver_controls.h"
#include "main.h"

extern ADC_HandleTypeDef hadc1;

#define TV_DIAL_CHANNEL    ADC_CHANNEL_10  /* PC0 / Nucleo A5 */
#define REGEN_DIAL_CHANNEL ADC_CHANNEL_11  /* PC1 / Nucleo A4 */

static uint16_t s_tv_filtered;
static uint16_t s_regen_filtered;
static uint8_t s_tv_percent;
static uint8_t s_regen_percent;

static uint16_t read_channel(uint32_t channel) {
    ADC_ChannelConfTypeDef config = {0};
    config.Channel = channel;
    config.Rank = 1;
    config.SamplingTime = ADC_SAMPLETIME_84CYCLES;
    if (HAL_ADC_ConfigChannel(&hadc1, &config) != HAL_OK) return 0u;
    if (HAL_ADC_Start(&hadc1) != HAL_OK) return 0u;
    uint16_t value = 0u;
    if (HAL_ADC_PollForConversion(&hadc1, 2u) == HAL_OK) {
        value = (uint16_t)HAL_ADC_GetValue(&hadc1);
    }
    (void)HAL_ADC_Stop(&hadc1);
    return value;
}

static uint8_t percent_with_5pct_detents(uint16_t raw) {
    uint16_t percent = (uint16_t)(((uint32_t)raw * 100u + 2047u) / 4095u);
    percent = (uint16_t)(((percent + 2u) / 5u) * 5u);
    return percent > 100u ? 100u : (uint8_t)percent;
}

void DialInputs_Init(void) {
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &gpio);

    s_tv_filtered = read_channel(TV_DIAL_CHANNEL);
    s_regen_filtered = read_channel(REGEN_DIAL_CHANNEL);
    DialInputs_Update10ms();
}

void DialInputs_Update10ms(void) {
    uint16_t tv = read_channel(TV_DIAL_CHANNEL);
    uint16_t regen = read_channel(REGEN_DIAL_CHANNEL);
    /* alpha=1/4: enough filtering for a potentiometer without sluggish feel. */
    s_tv_filtered = (uint16_t)((3u * s_tv_filtered + tv + 2u) / 4u);
    s_regen_filtered = (uint16_t)((3u * s_regen_filtered + regen + 2u) / 4u);
    s_tv_percent = percent_with_5pct_detents(s_tv_filtered);
    s_regen_percent = percent_with_5pct_detents(s_regen_filtered);
    DriverControls_SetTvPercent(s_tv_percent);
    DriverControls_SetRegenPercent(s_regen_percent);
    DriverControls_SetTvEnabled(s_tv_percent > 0u);
    DriverControls_SetRegenEnabled(s_regen_percent > 0u);
}

uint8_t DialInputs_GetTvPercent(void) { return s_tv_percent; }
uint8_t DialInputs_GetRegenPercent(void) { return s_regen_percent; }

