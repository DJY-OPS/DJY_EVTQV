#ifndef SAS_CENTER_H
#define SAS_CENTER_H
#include <stdint.h>
#include <stdbool.h>

/* SAS 직진 영점 자동 학습 (2026-10-02).
 *
 * 정지 상태에서 맞춘 영점은 조향 유격 때문에 주행 직진 값과 2~3° 다르고,
 * SAS 커플링이 미끄러지면 영점 자체가 움직인다. 그래서 "IMU가 직진이라고
 * 확인한 순간"의 SAS 원시값으로 영점을 잡는다.
 *
 *   부팅   : 플래시(섹터 7)에 저장된 마지막 영점 → 없으면 SAS_CENTER_RAW
 *   CAPTURE: 직진 샘플을 누적 100개(=1초) 모아 중앙값으로 한 번에 설정
 *   TRACK  : 이후 직진마다 시정수 SAS_LEARN_TAU_S로 천천히 따라감
 *   저장   : 정지 2초 이상 + 마지막 저장값과 10카운트 이상 차이 → 플래시 이어쓰기
 *
 * 영점이 SAS_CENTER_RAW에서 SAS_CENTER_ALARM_RAW를 넘게 벗어나면 커플링이
 * 크게 미끄러진 것으로 보고 Level 2를 돌려준다 → main.c가 TV를 끄고 ED만 쓴다.
 *
 * 원시값은 14비트(0~16383)로 한 바퀴를 넘어가므로, 모든 계산은 현재 영점
 * 기준으로 펼친(unwrap) 값으로 한다. */

enum { SAS_CENTER_SRC_DEFAULT = 0, SAS_CENTER_SRC_FLASH = 1, SAS_CENTER_SRC_STRAIGHT = 2 };
enum { SAS_CENTER_LEVEL_OK = 0, SAS_CENTER_LEVEL_WARN = 1, SAS_CENTER_LEVEL_ALARM = 2 };

/* 부팅 시 1회 — 반드시 MX_IWDG_Init() 이전 (섹터가 가득 찼으면 1~2초 지움). */
void     SasCenter_Init(void);

/* 100Hz 제어 ISR에서 매 틱. raw = CAN 원시값, v = 차속[m/s]. */
void     SasCenter_Update(uint16_t raw, float v, float yaw_rate, float lat_acc,
                          uint16_t rpm_left, uint16_t rpm_right, bool inputs_valid);

/* 메인 루프에서 호출 — 정지 상태가 2초 이상이면 필요할 때 플래시에 기록. */
void     SasCenter_Service(bool stationary);

/* raw를 현재 영점 기준으로 펼친 값 (영점 ± 8192 범위, 필터 입력용). */
float    SasCenter_Unwrap(float raw);
/* raw − 영점 [카운트], 한 바퀴 넘어감 처리 포함. */
float    SasCenter_Offset(float raw);

uint16_t SasCenter_GetRaw(void);     /* 현재 영점 (텔레메트리용) */
uint8_t  SasCenter_GetSource(void);
uint8_t  SasCenter_GetLevel(void);

#endif
