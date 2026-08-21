#include "rpm_sensor.h"
#include "board_config.h"
#include "vehicle_params.h"
#include "common_types.h"
#include "filters.h"
#include "main.h"

/* CubeMX: TIM2 Input Capture Direct, CH1=PA0(좌)/CH2=PA1(우), Rising Edge,
 * PSC=839 (84MHz/840=100kHz → 10us/tick), ARR=0xFFFFFFFF, Filter=8, Pull-up.
 * 컨트롤러 30핀 커넥터 18번 핀(ALARM/SPD, 실측 피크 3.7~4.4V) → PA0/PA1 직결.
 * ★13번 핀(RXD)이 아니라 18번 핀이 진짜 SPD임 — board_config.h 주석 참고.
 * ★컨트롤러 소프트웨어 Display 탭 "SpecialFrame"이 0(순수 속도펄스)이어야
 * 정상 동작. 기본값 21(One-Line 모드)이면 디지털 통신 데이터가 나와서
 * 펄스로 오인식되어 엉뚱한 값이 찍힘.
 * ★GPIO 대체기능(AF1) 설정은 stm32f4xx_hal_msp.c의 HAL_TIM_IC_MspInit()에서
 * 정상 생성됨(main.c의 MX_GPIO_Init_2 USER CODE 블록은 중복 안전장치).
 *
 * ★TIM3(16비트) → TIM2(32비트)로 이전. 카운터가 32비트라 10us/tick에서
 * 한 바퀴 도는 데 11.9시간이 걸리므로 랩어라운드를 사실상 신경 쓸 필요가 없다.
 * 예전 16비트에서는 655ms가 측정 한계라 극저속에서 주기를 못 재고 잘렸다. */
extern TIM_HandleTypeDef htim2;

static volatile uint32_t s_last_capture_l = 0, s_last_capture_r = 0;
static volatile uint32_t s_period_us_l    = 0, s_period_us_r    = 0;
static volatile uint32_t s_last_ms_l      = 0, s_last_ms_r      = 0;
static volatile bool     s_first_l = true, s_first_r = true;

/* ★TEMP 진단용: 캡처 인터럽트가 실제로 몇 번 들어왔는지(주기 계산과 무관) */
volatile uint32_t g_rpm_cap_count_l = 0;
volatile uint32_t g_rpm_cap_count_r = 0;
/* 노이즈로 판정해 버린 캡처 수 — 실차에서 이 값이 폭증하면 SPD 배선 문제다.
 * (RC 필터 추가 / 실드선 / 접지 분리를 검토할 것) */
volatile uint32_t g_rpm_glitch_l = 0;
volatile uint32_t g_rpm_glitch_r = 0;

/* 제어 틱마다 갱신되는 필터 출력 */
static uint16_t   s_rpm_l = 0, s_rpm_r = 0;
static Deglitch_t s_dg_l,  s_dg_r;
static LPF1_t     s_lpf_l, s_lpf_r;

void RPM_Init(void) {
    Deglitch_Reset(&s_dg_l);  Deglitch_Reset(&s_dg_r);
    LPF1_Reset(&s_lpf_l);     LPF1_Reset(&s_lpf_r);
    s_rpm_l = 0; s_rpm_r = 0;
    HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1);
    HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_2);
}

/* 캡처 인터럽트: 직전 캡처와의 차이(틱) → 주기[us] 갱신.
 * TIM2는 32비트 카운터라 uint32_t 뺄셈이 그대로 랩어라운드를 처리한다
 * (예전 TIM3는 16비트라 & 0xFFFF 마스크가 필요했다).
 *
 * ★1차 노이즈 방어(지연 0): 물리적으로 불가능하게 짧은 간격의 에지는 버린다.
 *   RPM_MIN_PERIOD_US(≈11.9ms = 8000RPM 상당)보다 짧으면 노이즈로 확정.
 *   전기적 노이즈 펄스는 us 단위라 이 한 줄로 사실상 전부 걸러진다.
 *   중요한 건 "버릴 때 s_last_capture를 갱신하지 않는" 것 — 그래야 다음에
 *   들어오는 진짜 에지가 직전 진짜 에지 기준으로 올바른 주기를 만든다. */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance != TIM2) return;
    uint32_t now = HAL_GetTick();

    if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
        uint32_t cap = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
        g_rpm_cap_count_l++;
        if (!s_first_l) {
            uint32_t period = (cap - s_last_capture_l) * RPM_TICK_US;
            if (period < RPM_MIN_PERIOD_US) { g_rpm_glitch_l++; return; }  /* 노이즈 폐기 */
            s_period_us_l = period;
        }
        s_first_l = false;
        s_last_capture_l = cap;
        s_last_ms_l = now;
    } else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) {
        uint32_t cap = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
        g_rpm_cap_count_r++;
        if (!s_first_r) {
            uint32_t period = (cap - s_last_capture_r) * RPM_TICK_US;
            if (period < RPM_MIN_PERIOD_US) { g_rpm_glitch_r++; return; }  /* 노이즈 폐기 */
            s_period_us_r = period;
        }
        s_first_r = false;
        s_last_capture_r = cap;
        s_last_ms_r = now;
    }
}

static float period_to_rpm(uint32_t period_us) {
    if (period_us == 0) return 0.0f;
    /* freq[Hz] = 1e6/period_us, RPM = freq * 60 / PULSES_PER_REV * RPM_CAL_GAIN(실측보정) */
    float raw = 60000000.0f / ((float)period_us * (float)RPM_PULSES_PER_REV);
    return raw * RPM_CAL_GAIN;
}

/* 마지막 캡처 이후 흐른 시간이 저장된 주기보다 길면, 그 경과시간을 주기로 본다.
 * ★이게 없으면 감속/정지 시 RPM이 마지막 값에 그대로 얼어붙어 있다가
 * RPM_STALE_MS(400ms)가 지나는 순간 0으로 절벽처럼 떨어진다. 그 400ms 동안
 * "정지했는데 차속이 높다"고 착각해서 TV가 붙어버릴 수 있다.
 * 경과시간을 주기로 쓰면 RPM이 자연스럽게 우하향으로 감쇠한다. */
static float measure_rpm(uint32_t period_us, uint32_t last_ms, uint32_t now_ms) {
    uint32_t age = now_ms - last_ms;
    if (age > RPM_STALE_MS || period_us == 0) return 0.0f;
    uint32_t effective = period_us;
    uint32_t age_us    = age * 1000u;
    if (age_us > effective) effective = age_us;
    return period_to_rpm(effective);
}

/* 100Hz 제어 틱에서 1회 호출.
 * 2차 방어: Deglitch(잔여 스파이크) → LPF fc=5Hz. 차속은 느린 물리량이라
 * 32ms 군지연이 제어 성능에 영향을 주지 않는다(차속은 TV 활성 판정과
 * 목표요레이트 스케일에만 쓰인다). */
void RPM_Update(void) {
    uint32_t now = HAL_GetTick();
    const float alpha = LPF1_Alpha(RPM_LPF_FC_HZ, CONTROL_DT);

    float l = measure_rpm(s_period_us_l, s_last_ms_l, now);
    float r = measure_rpm(s_period_us_r, s_last_ms_r, now);

    l = Deglitch_Update(&s_dg_l, l, RPM_MAX_STEP, DEGLITCH_MAX_REJECT);
    r = Deglitch_Update(&s_dg_r, r, RPM_MAX_STEP, DEGLITCH_MAX_REJECT);

    l = LPF1_Update(&s_lpf_l, l, alpha);
    r = LPF1_Update(&s_lpf_r, r, alpha);

    s_rpm_l = (uint16_t)CLAMP(l, 0.0f, 65535.0f);
    s_rpm_r = (uint16_t)CLAMP(r, 0.0f, 65535.0f);
}

uint16_t RPM_GetLeft(void)  { return s_rpm_l; }
uint16_t RPM_GetRight(void) { return s_rpm_r; }

/* 필터 전 원본 — 디버그 출력/캘리브레이션용 */
uint16_t RPM_GetLeftRaw(void) {
    return (uint16_t)measure_rpm(s_period_us_l, s_last_ms_l, HAL_GetTick());
}
uint16_t RPM_GetRightRaw(void) {
    return (uint16_t)measure_rpm(s_period_us_r, s_last_ms_r, HAL_GetTick());
}

bool RPM_IsFresh(void) {
    uint32_t now = HAL_GetTick();
    return ((now - s_last_ms_l) <= RPM_STALE_MS) &&
           ((now - s_last_ms_r) <= RPM_STALE_MS);
}
