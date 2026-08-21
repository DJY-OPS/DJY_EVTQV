#include "driver_controls.h"

#define DEFAULT_TV_PERCENT     100u
#define DEFAULT_REGEN_PERCENT    0u
#define DEFAULT_DRIVE_MODE DJY_MODE_RACE

static DjyDriverControl s_control;

static uint8_t percent_clamped(uint8_t value) {
    return value > 100u ? 100u : value;
}

void DriverControls_Init(void) {
    s_control.tv_percent    = DEFAULT_TV_PERCENT;
    s_control.regen_percent = DEFAULT_REGEN_PERCENT;
    s_control.mode          = DEFAULT_DRIVE_MODE;
    s_control.flags         = DJY_CONTROL_FLAG_TV_ENABLE;
    s_control.sequence      = 0u;
}

void DriverControls_SetTvPercent(uint8_t percent) {
    s_control.tv_percent = percent_clamped(percent);
}

void DriverControls_SetRegenPercent(uint8_t percent) {
    s_control.regen_percent = percent_clamped(percent);
}

void DriverControls_SetMode(DjyDriveMode mode) {
    if (mode <= DJY_MODE_ATTACK) s_control.mode = mode;
}

void DriverControls_SetTvEnabled(bool enabled) {
    if (enabled) s_control.flags |= DJY_CONTROL_FLAG_TV_ENABLE;
    else         s_control.flags &= (uint8_t)~DJY_CONTROL_FLAG_TV_ENABLE;
}

void DriverControls_SetRegenEnabled(bool enabled) {
    if (enabled) s_control.flags |= DJY_CONTROL_FLAG_REGEN_ENABLE;
    else         s_control.flags &= (uint8_t)~DJY_CONTROL_FLAG_REGEN_ENABLE;
}

DjyDriverControl DriverControls_NextMessage(void) {
    DjyDriverControl message = s_control;
    message.sequence = s_control.sequence++;
    return message;
}

DjyDriverControl DriverControls_GetCurrent(void) {
    return s_control;
}
