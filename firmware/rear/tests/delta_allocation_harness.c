#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "torque_vectoring.h"
#include "vehicle_params.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static float lateral_acc;
float IMU_GetLateralAcc(void) { return lateral_acc; }
static unsigned checked;
static const float tolerance = 0.00002f;

static TV_t setup(int direction, int tv_mode) {
    TV_t t = {0};
    TV_Init();
    TV_SetTVEnabled(tv_mode != 0);
    TV_SetEDEnabled(true);
    TV_SetStrength(1.0f);
    t.rpm_left = t.rpm_right = 1661;
    t.steering_angle_rad = direction * 0.2f;
    t.tps_fraction = 1.0f;
    lateral_acc = 0.0f;
    return t;
}

static void tick(TV_t *t) {
    TV_Update(t);
    float pedal = fminf(1.0f, fmaxf(0.0f, t->tps_fraction));
    float demand = fminf(pedal * P_SUM_MAX_KW, 2.0f * MOTOR_MAX_KW);
    float fraction = fminf(1.0f, fmaxf(0.0f, TV_DELTA_DEMAND_FRAC));
    CHECK(isfinite(t->power_left) && isfinite(t->power_right));
    CHECK(t->power_left >= 0 && t->power_right >= 0);
    CHECK(t->power_left <= MOTOR_MAX_KW + tolerance);
    CHECK(t->power_right <= MOTOR_MAX_KW + tolerance);
    CHECK(fabsf(t->power_left + t->power_right - demand) < tolerance);
    CHECK(fabsf(t->delta_power - (t->power_right - t->power_left)) < tolerance);
    CHECK(fabsf(t->delta_power) <= fraction * demand + tolerance);
    CHECK(t->power_left + tolerance >= (1.0f - fraction) * demand * 0.5f);
    CHECK(t->power_right + tolerance >= (1.0f - fraction) * demand * 0.5f);
    CHECK(t->dac_left <= DAC_RESOLUTION && t->dac_right <= DAC_RESOLUTION);
    if (t->tv_active) {
        CHECK(fabsf(t->delta_power) <= DELTA_POWER_MAX_KW + tolerance);
        CHECK(fabsf(t->delta_power) <= DELTA_FORCE_MAX_N * t->vehicle_speed * 0.001f + tolerance);
    } else if (t->ed_active) {
        CHECK(fabsf(t->delta_power) <= ED_DELTA_MAX_KW + tolerance);
    } else {
        CHECK(fabsf(t->delta_power) < tolerance);
    }
    if (pedal == 0) {
        uint16_t off = (uint16_t)(V_THROTTLE_OFF * DAC_CODE_PER_V);
        CHECK(t->dac_left == off && t->dac_right == off);
    }
    checked++;
}

static uint32_t random_state = 0x2468ACE1u;
static uint32_t next_random(void) {
    random_state = random_state * 1664525u + 1013904223u;
    return random_state;
}

int main(void) {
    for (int mode = 0; mode <= 1; mode++) {
        for (int dir = -1; dir <= 1; dir += 2) {
            TV_t t = setup(dir, mode);
            for (int k = 0; k < 200; k++) tick(&t);
            t.tps_fraction = 0.1f;
            tick(&t);  /* Regression: old slew state must obey the NEW pedal cap. */
            CHECK(t.power_left > 0 && t.power_right > 0);
            if (mode && dir == 1) {
                printf("full_to_10_percent left=%.6f right=%.6f dp=%.6f\n", t.power_left, t.power_right, t.delta_power);
            }
            for (int k = 0; k < 100; k++) tick(&t);
            t.tps_fraction = 0;
            tick(&t);
            t.tps_fraction = 1;
            tick(&t);
            CHECK(fabsf(t.delta_power) <= DELTA_POWER_SLEW_KW_S * CONTROL_DT + tolerance);
            for (int k = 0; k < 100; k++) tick(&t);
            t.rpm_left = t.rpm_right = 400; /* Reduced force cap or TV->ED transition. */
            tick(&t);
            TV_SetTVEnabled(false);
            tick(&t);
            TV_SetEDEnabled(false);
            tick(&t);
            CHECK(!t.tv_active && !t.ed_active);
            CHECK(fabsf(t.power_left - t.power_right) < tolerance);
            TV_Reset();
            TV_SetTVEnabled(true);
            t.rpm_left = t.rpm_right = 1661;
            tick(&t);
            CHECK(fabsf(t.delta_power) <= DELTA_POWER_SLEW_KW_S * CONTROL_DT + tolerance);
        }
    }
    TV_t t = setup(1, 1);
    for (int k = 0; k < 10000; k++) {
        t.tps_fraction = ((int)(next_random() % 141) - 20) * 0.01f;
        t.steering_angle_rad = ((int)(next_random() % 81) - 40) * 0.01f;
        t.rpm_left = (uint16_t)(next_random() % 8001);
        t.rpm_right = (uint16_t)(next_random() % 8001);
        t.imu_yaw_rate = ((int)(next_random() % 201) - 100) * 0.01f;
        lateral_acc = ((int)(next_random() % 2001) - 1000) * 0.01f;
        TV_SetTVEnabled((next_random() & 8) != 0);
        TV_SetEDEnabled((next_random() & 16) != 0);
        TV_SetStrength((next_random() % 101) * 0.01f);
        tick(&t);
    }
    printf("PASS %u control ticks, MOTOR_MAX_KW=%.2f\n", checked, (double)MOTOR_MAX_KW);
    return 0;
}
