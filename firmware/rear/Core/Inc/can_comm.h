#ifndef CAN_COMM_B_H
#define CAN_COMM_B_H
#include <stdint.h>
#include <stdbool.h>
#include "can_messages.h"

typedef struct { uint16_t sas_angle; uint16_t tps_raw; } SensorData_t;
typedef struct { uint16_t left; uint16_t right; } MotorRPM_t;

void         CAN_Init(void);
SensorData_t CAN_GetSensorData(void);
MotorRPM_t   CAN_GetMotorRPM(void);
uint8_t      CAN_GetHeartbeatStatus(void);
bool         CAN_IsSensorFresh(void);
bool         CAN_IsHeartbeatFresh(void);   /* 상태 비트를 신뢰해도 되는지 */
bool         CAN_IsRpmFresh(void);
DjyDriverControl CAN_GetDriverControl(void);
bool         CAN_IsDriverControlFresh(void);
bool         CAN_SendRearStatus(const DjyRearStatus *status);
bool         CAN_GetPendingPitConfig(DjyPitConfig *config, uint8_t last_applied_sequence);
bool         CAN_SendPitConfigAck(const DjyPitConfig *config);
bool         CAN_SendRearDrivetrain(uint16_t rpm_left, uint16_t rpm_right,
                                    uint16_t dac_left, uint16_t dac_right);
uint32_t     CAN_GetControlErrorCount(void);
uint32_t     CAN_GetTxDropCount(void);

/* ★TEMP 진단 도구: 컨트롤러 CAN H/L의 비트레이트/ID 레이아웃을 모르는 상태에서
 * 스니핑하기 위한 함수. main.c의 CAN_SNIFF_MODE=1일 때만 호출된다.
 * 블로킹이며 리턴하지 않는다 — 후보 비트레이트를 순서대로 시도해 USART2로
 * 결과를 출력한 뒤, 가장 프레임이 많이 잡힌 비트레이트로 고정해 계속 덤프한다.
 * ★반드시 Board A와 물리적으로 분리한 상태(별도 배선)에서만 쓸 것 — 이 함수가
 * CAN1을 재초기화하면서 Board A와의 정상 통신을 깬다. */
void         CAN_SniffSweep(void);

#endif /* CAN_COMM_B_H */
