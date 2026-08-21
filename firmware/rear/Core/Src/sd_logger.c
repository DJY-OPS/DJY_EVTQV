#include "sd_logger.h"
#include "imu_sensor.h"
#include "main.h"
#include "fatfs.h"
#include <stdio.h>
#include <string.h>

#define LOG_QUEUE_CAPACITY 32u
#define LOG_FLUSH_BATCH     4u
#define LOG_SYNC_RECORDS  100u

typedef struct {
    uint32_t time_ms;
    TV_t tv;
    float lateral_acc;
    SafeAction_t action;
} LogRecord;

/* Single producer (TIM6 ISR), single consumer (main loop). Formatting and all
 * FATFS operations stay out of the 100 Hz control interrupt. */
static LogRecord s_queue[LOG_QUEUE_CAPACITY];
static volatile uint8_t s_head;
static volatile uint8_t s_tail;
static volatile uint32_t s_drop_count;
static uint16_t s_records_since_sync;
static FIL s_file;
static volatile bool s_open;

void SD_Logger_Init(void) {
    s_head = 0u;
    s_tail = 0u;
    s_drop_count = 0u;
    s_records_since_sync = 0u;
    s_open = false;

    if (f_mount(&USERFatFS, USERPath, 1) != FR_OK) return;
    if (f_open(&s_file, "tvlog.csv", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) return;
    const char *header =
      "t_ms,sas,tps_pct,rpm_l,rpm_r,yaw,lat,des_yaw,yaw_err,"
      "dP,P_l,P_r,dac_l,dac_r,tv,fault,speed,traction\r\n";
    UINT written;
    if (f_write(&s_file, header, strlen(header), &written) != FR_OK) {
        (void)f_close(&s_file);
        return;
    }
    (void)f_sync(&s_file);
    s_open = true;
}

void SD_Logger_Write(const TV_t *tv, SafeAction_t action) {
    if (!s_open) return;
    uint8_t head = s_head;
    uint8_t next = (uint8_t)((head + 1u) % LOG_QUEUE_CAPACITY);
    if (next == s_tail) {
        ++s_drop_count;
        return;
    }

    s_queue[head].time_ms = HAL_GetTick();
    s_queue[head].tv = *tv;
    s_queue[head].lateral_acc = IMU_GetLateralAcc();
    s_queue[head].action = action;
    __DMB();
    s_head = next;
}

void SD_Logger_Flush(void) {
    if (!s_open) return;

    for (uint8_t count = 0u; count < LOG_FLUSH_BATCH && s_tail != s_head; ++count) {
        uint8_t tail = s_tail;
        LogRecord record = s_queue[tail];
        char line[176];
        int length = snprintf(line, sizeof(line),
            "%lu,%u,%.1f,%u,%u,%.3f,%.2f,%.3f,%.3f,%.2f,%.2f,%.2f,%u,%u,%d,%d,%.2f,%.2f\r\n",
            (unsigned long)record.time_ms, 0u,
            (double)(record.tv.tps_fraction * 100.0f),
            record.tv.rpm_left, record.tv.rpm_right,
            (double)record.tv.imu_yaw_rate, (double)record.lateral_acc,
            (double)record.tv.desired_yaw, (double)record.tv.yaw_error,
            (double)record.tv.delta_power, (double)record.tv.power_left,
            (double)record.tv.power_right, record.tv.dac_left,
            record.tv.dac_right, record.tv.tv_active ? 1 : 0,
            (int)record.action, (double)record.tv.vehicle_speed,
            (double)record.tv.traction_scale);

        if (length > 0) {
            if (length >= (int)sizeof(line)) length = (int)sizeof(line) - 1;
            UINT written;
            if (f_write(&s_file, line, (UINT)length, &written) != FR_OK) {
                s_open = false;
                return;
            }
        }
        __DMB();
        s_tail = (uint8_t)((tail + 1u) % LOG_QUEUE_CAPACITY);

        if (++s_records_since_sync >= LOG_SYNC_RECORDS) {
            s_records_since_sync = 0u;
            (void)f_sync(&s_file);
        }
    }
}

void SD_Logger_Close(void) {
    if (!s_open) return;
    while (s_tail != s_head) SD_Logger_Flush();
    (void)f_sync(&s_file);
    (void)f_close(&s_file);
    s_open = false;
}
