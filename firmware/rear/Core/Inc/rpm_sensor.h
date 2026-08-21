#ifndef RPM_SENSOR_H
#define RPM_SENSOR_H
#include <stdint.h>
#include <stdbool.h>

void     RPM_Init(void);
void     RPM_Update(void);     /* ★100Hz 제어 틱에서 1회 호출 — 필터 갱신 */
uint16_t RPM_GetLeft(void);    /* [RPM] 필터 적용값 */
uint16_t RPM_GetRight(void);   /* [RPM] 필터 적용값 */
uint16_t RPM_GetLeftRaw(void);  /* 필터 전 원본 — 디버그/캘리브레이션용 */
uint16_t RPM_GetRightRaw(void);
bool     RPM_IsFresh(void);    /* 좌/우 둘 다 최근에 펄스 수신했는지 */

/* ★TEMP 진단용: 캡처 인터럽트 누적 횟수(주기 계산 성공 여부와 무관) */
extern volatile uint32_t g_rpm_cap_count_l;
extern volatile uint32_t g_rpm_cap_count_r;
/* 노이즈로 판정해 폐기한 캡처 수 — 실차에서 급증하면 SPD 배선을 의심할 것 */
extern volatile uint32_t g_rpm_glitch_l;
extern volatile uint32_t g_rpm_glitch_r;

#endif /* RPM_SENSOR_H */
