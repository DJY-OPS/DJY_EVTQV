#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "djy_can_protocol.h"
#include "driver_controls.h"
#include "control_settings.h"

static void test_crc_and_frames(void) {
    const uint8_t check[] = "123456789";
    assert(djy_crc8(check, 9u) == 0x4bu);

    DjyDriverControl input = {73u, 25u, DJY_MODE_ATTACK,
        DJY_CONTROL_FLAG_TV_ENABLE | DJY_CONTROL_FLAG_REGEN_ENABLE, 254u};
    uint8_t bytes[8];
    djy_pack_driver_control(bytes, &input);
    DjyDriverControl output = {0};
    assert(djy_unpack_driver_control(&output, bytes));
    assert(output.tv_percent == 73u && output.regen_percent == 25u);
    assert(output.mode == DJY_MODE_ATTACK && output.sequence == 254u);
    bytes[1] ^= 1u;
    assert(!djy_unpack_driver_control(&output, bytes));

    DjyRearStatus status_in = {50u, 0u, DJY_MODE_RACE,
        DJY_REAR_STATUS_CONTROL_FRESH | DJY_REAR_STATUS_TV_ACTIVE, 3u, 9u};
    djy_pack_rear_status(bytes, &status_in);
    DjyRearStatus status_out = {0};
    assert(djy_unpack_rear_status(&status_out, bytes));
    assert(status_out.tv_applied_percent == 50u && status_out.fault_code == 3u);

    DjyPitConfig config_in = {65u, 30u, 15u, 8u,
        DJY_PIT_FLAG_TV_PERMITTED | DJY_PIT_FLAG_REGEN_PERMITTED, 22u};
    djy_pack_pit_config(bytes, &config_in);
    DjyPitConfig config_out = {0};
    assert(djy_unpack_pit_config(&config_out, bytes));
    assert(config_out.tv_limit_percent == 65u && config_out.regen_limit_percent == 30u);
    assert(config_out.sequence == 22u);
}

static void test_front_controls(void) {
    DriverControls_Init();
    DriverControls_SetTvPercent(70u);
    DriverControls_SetRegenPercent(20u);
    DriverControls_SetMode(DJY_MODE_QUALIFYING);
    DriverControls_SetRegenEnabled(true);
    DjyDriverControl first = DriverControls_NextMessage();
    DjyDriverControl second = DriverControls_NextMessage();
    assert(first.tv_percent == 70u && first.regen_percent == 20u);
    assert(first.mode == DJY_MODE_QUALIFYING);
    assert((first.flags & DJY_CONTROL_FLAG_REGEN_ENABLE) != 0u);
    assert(first.sequence == 0u && second.sequence == 1u);
}

static void test_rear_ramp_and_fallback(void) {
    DjyDriverControl command = {50u, 40u, DJY_MODE_RACE,
        DJY_CONTROL_FLAG_TV_ENABLE | DJY_CONTROL_FLAG_REGEN_ENABLE, 1u};
    ControlSettings_Init();
    DjyPitConfig config = {45u, 25u, 20u, 10u,
        DJY_PIT_FLAG_TV_PERMITTED | DJY_PIT_FLAG_REGEN_PERMITTED, 1u};
    ControlSettings_ApplyPitConfig(&config);
    for (int i = 0; i < 25; ++i) ControlSettings_Update10ms(&command, true);
    assert(ControlSettings_GetTvAppliedPercent() == 45u);
    assert(ControlSettings_GetRegenRequestedPercent() == 25u);
    assert(ControlSettings_GetRegenAppliedPercent() == 0u);
    for (int i = 0; i < 25; ++i) ControlSettings_Update10ms(&command, false);
    assert(ControlSettings_GetTvAppliedPercent() == 0u);
    assert(!ControlSettings_IsFresh());
}

int main(void) {
    test_crc_and_frames();
    test_front_controls();
    test_rear_ramp_and_fallback();
    puts("protocol/control tests: PASS");
    return 0;
}
