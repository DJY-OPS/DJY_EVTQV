#include "sas_center.h"
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include "main.h"
#include "vehicle_params.h"
#include "common_types.h"
#include "can_comm.h"

/* ── 플래시 저장 영역 ─────────────────────────────────────────────────
 * STM32F446RE 섹터 7 (0x08060000, 128KB). 펌웨어는 약 76KB라 섹터 0~4만 쓴다.
 * 레코드 8바이트 = {0x5A5A0000 | 영점, ~그 값}. 지우지 않고 빈 칸에 이어 쓰기만
 * 해서 주행 중 플래시 지우기(1~2초 정지)가 절대 없다. 가득 차면 다음 부팅 때,
 * 워치독과 제어 루프가 시작되기 전에만 지운다. */
#ifndef SAS_FLASH_BASE                       /* 호스트 시험에서 RAM 배열로 바꿔 끼운다 */
#define SAS_FLASH_BASE      0x08060000u
#endif
#define SAS_FLASH_SIZE      0x00020000u
#define SAS_FLASH_SECTOR    FLASH_SECTOR_7
#define SAS_REC_MAGIC       0x5A5A0000u
#define SAS_REC_COUNT       (SAS_FLASH_SIZE / 8u)

#define SAS_WRAP            16384
#define SAS_HALF            8192

static volatile float    s_center = (float)SAS_CENTER_RAW;
static volatile uint8_t  s_source = SAS_CENTER_SRC_DEFAULT;
static volatile bool     s_tracking;            /* false = CAPTURE, true = TRACK */

static uint16_t s_buf[SAS_LEARN_CAPTURE_N];     /* CAPTURE 버퍼 (영점 기준 오프셋 + SAS_HALF) */
static uint16_t s_buf_n;
static uint16_t s_track_reject;
static uint16_t s_window_reject;                /* 직진인데 학습 한계 밖인 샘플 수 */
static volatile bool s_out_of_window;           /* 센서가 한계 밖으로 돌아감 → 경보 */
static uint32_t s_last_rx_us;
static bool     s_grounded;                    /* 이번 전원에서 차가 실제로 땅에서 돈 적이 있나 */

static uint32_t s_next_slot;                    /* 다음에 쓸 레코드 번호 */
static uint16_t s_saved = 0xFFFFu;              /* 마지막으로 플래시에 쓴 영점 */
static uint32_t s_stationary_since;
static bool     s_stationary_seen;
static bool     s_stop_handled;                /* 이번 정지에서 이미 저장을 시도함 */

static inline int wrap_offset(int d) {
    while (d >=  SAS_HALF) d -= SAS_WRAP;
    while (d <  -SAS_HALF) d += SAS_WRAP;
    return d;
}

float SasCenter_Offset(float raw) {
    float d = raw - s_center;
    while (d >=  (float)SAS_HALF) d -= (float)SAS_WRAP;
    while (d <  -(float)SAS_HALF) d += (float)SAS_WRAP;
    return d;
}

float SasCenter_Unwrap(float raw) { return s_center + SasCenter_Offset(raw); }

uint16_t SasCenter_GetRaw(void) {
    int c = (int)lrintf(s_center) % SAS_WRAP;
    if (c < 0) c += SAS_WRAP;
    return (uint16_t)c;
}
uint8_t SasCenter_GetSource(void) { return s_source; }

uint8_t SasCenter_GetLevel(void) {
    if (s_out_of_window) return SAS_CENTER_LEVEL_ALARM;
    float dev = fabsf(s_center - (float)SAS_CENTER_RAW);
    if (dev > (float)SAS_CENTER_ALARM_RAW) return SAS_CENTER_LEVEL_ALARM;
    if (dev > (float)SAS_CENTER_WARN_RAW)  return SAS_CENTER_LEVEL_WARN;
    return SAS_CENTER_LEVEL_OK;
}

/* 학습값이 갈 수 있는 한계 — 이 밖은 센서가 사실상 고장이므로 클램프하고
 * Level 2(ALARM)가 TV를 막는다. 0~16383 안에 머물러 저장도 그대로 된다. */
static float clamp_center(float c) {
    return CLAMP(c, (float)(SAS_CENTER_RAW - SAS_LEARN_WINDOW_RAW),
                    (float)(SAS_CENTER_RAW + SAS_LEARN_WINDOW_RAW));
}

static bool record_valid(uint32_t w0, uint32_t w1) {
    return (w0 & 0xFFFF0000u) == SAS_REC_MAGIC && w1 == ~w0;
}

void SasCenter_Init(void) {
    const volatile uint32_t *f = (const volatile uint32_t *)SAS_FLASH_BASE;
    uint32_t i;
    bool found = false;
    uint16_t value = 0;

    /* 첫 빈 칸까지 훑으며 마지막 유효 레코드를 찾는다. 반쯤 쓰다 전원이 꺼진
     * 레코드(두 번째 워드만 0xFFFFFFFF)는 무효로 건너뛴다. */
    for (i = 0; i < SAS_REC_COUNT; ++i) {
        uint32_t w0 = f[2u * i], w1 = f[2u * i + 1u];
        if (w0 == 0xFFFFFFFFu && w1 == 0xFFFFFFFFu) break;
        if (record_valid(w0, w1)) { value = (uint16_t)(w0 & 0xFFFFu); found = true; }
    }
    s_next_slot = i;

    if (s_next_slot >= SAS_REC_COUNT) {          /* 가득 참 → 지우고 마지막 값만 다시 씀 */
        FLASH_EraseInitTypeDef er = {0};
        uint32_t bad = 0;
        er.TypeErase    = FLASH_TYPEERASE_SECTORS;
        er.Sector       = SAS_FLASH_SECTOR;
        er.NbSectors    = 1;
        er.VoltageRange = FLASH_VOLTAGE_RANGE_3;
        HAL_FLASH_Unlock();
        (void)HAL_FLASHEx_Erase(&er, &bad);
        HAL_FLASH_Lock();
        s_next_slot = 0;
        s_saved = 0xFFFFu;
    } else if (found) {
        s_saved = value;
    }

    if (found && fabsf((float)value - (float)SAS_CENTER_RAW) <= (float)SAS_LEARN_WINDOW_RAW) {
        s_center = (float)value;
        s_source = SAS_CENTER_SRC_FLASH;
    } else {
        s_center = (float)SAS_CENTER_RAW;
        s_source = SAS_CENTER_SRC_DEFAULT;
    }
    s_tracking = false;
    s_buf_n = 0;
    s_track_reject = 0;
    s_window_reject = 0;
    s_out_of_window = false;
}

static uint16_t median_u16(uint16_t *a, uint16_t n) {
    for (uint16_t i = 1; i < n; ++i) {           /* 삽입 정렬 — 100개, 캡처당 1회 */
        uint16_t x = a[i];
        int j = (int)i - 1;
        while (j >= 0 && a[j] > x) { a[j + 1] = a[j]; --j; }
        a[j + 1] = x;
    }
    return a[n / 2u];
}

void SasCenter_Update(uint16_t raw, float v, float yaw_rate, float lat_acc,
                      uint16_t rpm_left, uint16_t rpm_right, bool inputs_valid) {
#if SAS_BYPASS_MODE
    (void)raw; (void)v; (void)yaw_rate; (void)lat_acc; (void)rpm_left; (void)rpm_right; (void)inputs_valid;
#else
    /* 앞 보드에서 새 CAN 프레임이 왔을 때만 샘플 1개 — 같은 값 중복 방지 */
    uint32_t rx_us = CAN_GetSensorTiming().received_us;
    bool fresh = (rx_us != s_last_rx_us);
    s_last_rx_us = rx_us;

    if (!inputs_valid) return;
    /* 바퀴를 띄운 시험대에서는 바퀴는 돌아도 차체가 안 돈다. 그 상태의 조향값은
     * 정지 영점(유격 때문에 2~3° 틀림)이므로, 차가 실제로 땅에서 한 번이라도
     * 돌았던 전원 주기에서만 학습한다. */
    if (fabsf(yaw_rate) > SAS_GROUNDED_YAW || fabsf(lat_acc) > SAS_GROUNDED_AY) s_grounded = true;
    if (!fresh || !s_grounded) return;
    if (v < SAS_LEARN_MIN_SPEED || fabsf(yaw_rate) > SAS_LEARN_MAX_YAW ||
        fabsf(lat_acc) > SAS_LEARN_MAX_AY) return;
    /* 직진이면 좌우 바퀴 회전수가 같다 — 휠스핀/한쪽 미끄러짐 구간 제외 */
    {
        float sum = (float)rpm_left + (float)rpm_right;
        float diff = fabsf((float)rpm_left - (float)rpm_right);
        if (sum <= 0.0f || diff > SAS_LEARN_MAX_RPM_IMBALANCE * 0.5f * sum) return;
    }

    int off = wrap_offset((int)raw - (int)lrintf(s_center));

    if (!s_tracking) {
        /* CAPTURE: 공칭 영점에서 너무 먼 값(센서 고장)만 거르고 모은다 */
        float candidate = s_center + (float)off;
        if (fabsf(candidate - (float)SAS_CENTER_RAW) > (float)SAS_LEARN_WINDOW_RAW) {
            /* 직진인데 계속 한계 밖 = 센서를 다시 조립하며 크게 돌아감. 영점을
             * 믿을 수 없으므로 경보(→ TV 금지). 한계 안의 직진이 잡히면 풀린다. */
            if (s_window_reject < SAS_LEARN_CAPTURE_N && ++s_window_reject >= SAS_LEARN_CAPTURE_N)
                s_out_of_window = true;
            return;
        }
        s_buf[s_buf_n++] = (uint16_t)(off + SAS_HALF);
        if (s_buf_n >= SAS_LEARN_CAPTURE_N) {
            int med = (int)median_u16(s_buf, s_buf_n) - SAS_HALF;
            s_center   = clamp_center(s_center + (float)med);
            s_source   = SAS_CENTER_SRC_STRAIGHT;
            s_out_of_window = false;
            s_window_reject = 0;
            s_tracking = true;
            s_buf_n = 0;
            s_track_reject = 0;
        }
        return;
    }

    /* TRACK: 영점 근처 값만 천천히 따라간다. 직진인데 계속 멀리 있으면
     * (주행 중 커플링이 크게 미끄러짐) CAPTURE로 돌아가 다시 잡는다. */
    if (off > SAS_LEARN_TRACK_GATE_RAW || off < -SAS_LEARN_TRACK_GATE_RAW) {
        if (++s_track_reject >= SAS_LEARN_CAPTURE_N) {
            s_tracking = false;
            s_buf_n = 0;
            s_track_reject = 0;
        }
        return;
    }
    if (s_track_reject) --s_track_reject;
    s_center = clamp_center(s_center + (float)off * (CONTROL_DT / SAS_LEARN_TAU_S));
#endif
}

void SasCenter_Service(bool stationary) {
    uint32_t now = HAL_GetTick();
    if (!stationary) { s_stationary_seen = false; s_stop_handled = false; return; }
    if (!s_stationary_seen) { s_stationary_seen = true; s_stationary_since = now; return; }
    if (s_stop_handled || now - s_stationary_since < SAS_SAVE_STOP_MS) return;
    if (s_source != SAS_CENTER_SRC_STRAIGHT) return;     /* 직진으로 잡은 값만 저장 */
    if (s_next_slot >= SAS_REC_COUNT) return;            /* 가득 참 — 다음 부팅 때 지움 */

    uint16_t c = SasCenter_GetRaw();
    if (s_saved != 0xFFFFu && abs((int)c - (int)s_saved) < SAS_SAVE_MIN_DELTA_RAW) return;
    s_stop_handled = true;         /* 정지 1회당 최대 1번 — 실패해도 칸을 연달아 태우지 않음 */

    uint32_t w0 = SAS_REC_MAGIC | c, w1 = ~w0;
    uintptr_t addr = (uintptr_t)SAS_FLASH_BASE + 8u * s_next_slot;
    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
    /* 워드 하나 프로그램은 약 16µs — 제어 ISR 지연은 무시할 수준 */
    bool ok = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, w0) == HAL_OK &&
              HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + 4u, w1) == HAL_OK;
    HAL_FLASH_Lock();
    ++s_next_slot;                 /* 실패한 칸은 무효 레코드로 남기고 넘어간다 */
    if (ok) s_saved = c;
}
