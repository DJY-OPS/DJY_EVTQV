#ifndef CAN_COMM_H
#define CAN_COMM_H

#include "can_messages.h"

void CAN_Init(void);
bool CAN_SendSensorData(uint16_t sas_angle, uint16_t tps_raw);
bool CAN_SendHeartbeat(uint8_t status);
bool CAN_SendDriverControl(const DjyDriverControl *control);
DjyRearStatus CAN_GetRearStatus(void);
bool CAN_IsRearStatusFresh(void);
uint32_t CAN_GetTxDropCount(void);
uint32_t CAN_GetRxErrorCount(void);

#endif /* CAN_COMM_H */
