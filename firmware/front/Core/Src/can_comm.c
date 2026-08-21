#include "can_comm.h"
#include "board_config.h"
#include "main.h"

extern CAN_HandleTypeDef hcan1;

static CAN_TxHeaderTypeDef s_tx_sensor;
static CAN_TxHeaderTypeDef s_tx_heartbeat;
static CAN_TxHeaderTypeDef s_tx_control;
static volatile DjyRearStatus s_rear_status;
static volatile uint32_t s_rear_status_ms;
static volatile uint32_t s_tx_drop_count;
static volatile uint32_t s_rx_error_count;

#define REAR_STATUS_TIMEOUT_MS 250u

static bool can_send(const CAN_TxHeaderTypeDef *header, uint8_t data[8]) {
    uint32_t mailbox;
    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0u) {
        ++s_tx_drop_count;
        return false;
    }
    if (HAL_CAN_AddTxMessage(&hcan1, (CAN_TxHeaderTypeDef *)header,
                            data, &mailbox) != HAL_OK) {
        ++s_tx_drop_count;
        return false;
    }
    return true;
}

static CAN_TxHeaderTypeDef tx_header(uint32_t id, uint32_t dlc) {
    CAN_TxHeaderTypeDef header = {0};
    header.StdId = id;
    header.IDE = CAN_ID_STD;
    header.RTR = CAN_RTR_DATA;
    header.DLC = dlc;
    header.TransmitGlobalTime = DISABLE;
    return header;
}

void CAN_Init(void) {
    CAN_FilterTypeDef filter = {0};
    filter.FilterBank = 0;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter.FilterActivation = ENABLE;
    (void)HAL_CAN_ConfigFilter(&hcan1, &filter);

    s_tx_sensor    = tx_header(DJY_CAN_ID_SENSOR_DATA, DJY_CAN_DLC_SENSOR_DATA);
    s_tx_heartbeat = tx_header(DJY_CAN_ID_HEARTBEAT, DJY_CAN_DLC_HEARTBEAT);
    s_tx_control   = tx_header(DJY_CAN_ID_DRIVER_CONTROL, DJY_CAN_DLC_DRIVER_CONTROL);

    SET_BIT(hcan1.Instance->MCR, CAN_MCR_ABOM);
    hcan1.Init.AutoBusOff = ENABLE;
    (void)HAL_CAN_Start(&hcan1);
    (void)HAL_CAN_ActivateNotification(&hcan1,
        CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_ERROR | CAN_IT_BUSOFF);
}

bool CAN_SendSensorData(uint16_t sas_angle, uint16_t tps_raw) {
    uint8_t data[8] = {0};
    djy_can_pack_u16(&data[0], sas_angle & 0x3fffu);
    djy_can_pack_u16(&data[2], tps_raw & 0x0fffu);
    return can_send(&s_tx_sensor, data);
}

bool CAN_SendHeartbeat(uint8_t status) {
    uint8_t data[8] = {status};
    return can_send(&s_tx_heartbeat, data);
}

bool CAN_SendDriverControl(const DjyDriverControl *control) {
    uint8_t data[8];
    djy_pack_driver_control(data, control);
    return can_send(&s_tx_control, data);
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    CAN_RxHeaderTypeDef header;
    uint8_t data[8];
    if (hcan->Instance != CAN1 ||
        HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &header, data) != HAL_OK) {
        ++s_rx_error_count;
        return;
    }
    if (header.IDE != CAN_ID_STD || header.RTR != CAN_RTR_DATA ||
        header.StdId != DJY_CAN_ID_REAR_STATUS ||
        header.DLC != DJY_CAN_DLC_REAR_STATUS) {
        return;
    }

    DjyRearStatus decoded;
    if (!djy_unpack_rear_status(&decoded, data)) {
        ++s_rx_error_count;
        return;
    }
    s_rear_status = decoded;
    s_rear_status_ms = HAL_GetTick();
}

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan) {
    (void)hcan;
    ++s_rx_error_count;
}

DjyRearStatus CAN_GetRearStatus(void) {
    DjyRearStatus status;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    status = s_rear_status;
    if (primask == 0u) __enable_irq();
    return status;
}

bool CAN_IsRearStatusFresh(void) {
    uint32_t timestamp = s_rear_status_ms;
    return timestamp != 0u && (HAL_GetTick() - timestamp) <= REAR_STATUS_TIMEOUT_MS;
}

uint32_t CAN_GetTxDropCount(void) { return s_tx_drop_count; }
uint32_t CAN_GetRxErrorCount(void) { return s_rx_error_count; }
