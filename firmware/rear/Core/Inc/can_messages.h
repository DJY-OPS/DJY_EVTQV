#ifndef CAN_MESSAGES_H
#define CAN_MESSAGES_H

#include "../../../../shared/include/djy_can_protocol.h"

/* Backward-compatible names used by the original firmware. */
#define CAN_ID_SENSOR_DATA  DJY_CAN_ID_SENSOR_DATA
#define CAN_ID_LEFT_RPM     DJY_CAN_ID_LEFT_RPM
#define CAN_ID_RIGHT_RPM    DJY_CAN_ID_RIGHT_RPM
#define CAN_ID_HEARTBEAT    DJY_CAN_ID_HEARTBEAT
#define CAN_DLC_SENSOR_DATA DJY_CAN_DLC_SENSOR_DATA
#define CAN_DLC_HEARTBEAT   DJY_CAN_DLC_HEARTBEAT
#define HB_STATUS_OK        DJY_HB_STATUS_OK
#define HB_STATUS_SAS_ERR   DJY_HB_STATUS_SAS_ERR
#define HB_STATUS_TPS_ERR   DJY_HB_STATUS_TPS_ERR
#define can_pack_u16        djy_can_pack_u16
#define can_unpack_u16      djy_can_unpack_u16

#endif /* CAN_MESSAGES_H */
