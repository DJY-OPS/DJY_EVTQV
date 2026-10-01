#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "pit_tuning.h"
#include "torque_vectoring.h"
#include "control_settings.h"
uint32_t mask;
float IMU_GetLateralAcc(void) { return 0; }
static uint32_t now=0,request_id=0;
static void boot(void) { TV_Init(); ControlSettings_Init(); PitTuning_Init(); }
static DjyPitPacket change(DjyPitValues values) {
    DjyPitPacket request={.request_id=++request_id,.revision=PitTuning_Status().revision,.values=values};
    PitTuning_Queue(&request); PitTuning_Tick(true,++now); return request;
}
static float yaw_output(DjyPitValues values) {
    change(values); assert(PitTuning_Status().status==DJY_PIT_OK);
    TV_SetTVEnabled(true); TV_SetStrength(1);
    TV_t tv={.rpm_left=3000,.rpm_right=3000,.tps_fraction=1,.steering_angle_rad=.005f};
    TV_Update(&tv);return tv.delta_power;
}
int main(void) {
    boot();DjyPitPacket original=PitTuning_Status();
    DjyPitValues values={30000,2000,5000,200,4000,10000};
    assert(djy_pit_values_equal(&original.values,&values));
    assert(!original.allowed && !original.revision && ControlSettings_GetTvRampPercentPerSecond()==200);
    DjyPitPacket rejected=change(values);assert(PitTuning_Status().status==DJY_PIT_LOCKED);
    now=1000;PitTuning_Tick(true,now);assert(!PitTuning_Status().allowed);
    ++now;PitTuning_Tick(true,now);assert(PitTuning_Status().allowed);
    PitTuning_Queue(&rejected);PitTuning_Tick(true,++now);
    assert(!PitTuning_Status().revision); /* An old rejection cannot turn into an apply. */
    values=(DjyPitValues){1000,0,0,100,500,6000};
    DjyPitPacket applied=change(values);DjyPitPacket status=PitTuning_Status();
    assert(status.revision==1 && status.status==DJY_PIT_OK);
    assert(djy_pit_values_equal(&status.values,&values));
    DjyPitValues actual=TV_GetPitValues();assert(djy_pit_values_equal(&actual,&values));
    assert(ControlSettings_GetTvRampPercentPerSecond()==100);
    for(unsigned i=0;i<25;++i)ControlSettings_UpdateEsp10ms(100,100,true);
    assert(ControlSettings_GetTvAppliedPercent()==25);
    PitTuning_Queue(&applied);PitTuning_Tick(true,++now);assert(PitTuning_Status().revision==1);
    /* Stale revision and invalid values never partially change control state. */
    applied.request_id=++request_id;applied.values.kp=2000;
    PitTuning_Queue(&applied);PitTuning_Tick(true,++now);
    assert(PitTuning_Status().status==DJY_PIT_CONFLICT && TV_GetPitValues().kp==1000);
    DjyPitValues invalid=values;invalid.budget=10100;change(invalid);
    assert(PitTuning_Status().status==DJY_PIT_RANGE && TV_GetPitValues().budget==6000);
    PitTuning_Tick(false,++now);assert(!PitTuning_Status().allowed);
    change(values);assert(PitTuning_Status().status==DJY_PIT_LOCKED);
    now+=1000;PitTuning_Tick(true,now);assert(PitTuning_Status().allowed);
    /* Actual PID gains affect output, not merely the reported settings. */
    values=(DjyPitValues){0,0,0,200,4000,10000};
    assert(fabsf(yaw_output(values))<.00001f);
    values.kp=1000;float one=yaw_output(values);values.kp=2000;float two=yaw_output(values);
    assert(one>.001f && fabsf(two-2*one)<.0001f);
    /* Duplicate success must not reset the PID's integral. */
    values.kp=0;values.ki=1000;applied=change(values);
    TV_t tv={.rpm_left=3000,.rpm_right=3000,.tps_fraction=1,.steering_angle_rad=.005f};
    TV_Update(&tv);float first=tv.delta_power;
    PitTuning_Queue(&applied);PitTuning_Tick(true,++now);TV_Update(&tv);
    assert(first>0 && tv.delta_power>first*1.5f);
    /* Both TV and ED honor the edited differential/total limits. */
    values=(DjyPitValues){30000,2000,5000,10,100,3000};change(values);
    for(unsigned mode=0;mode<2;++mode) {
        TV_SetTVEnabled(mode!=0);TV_SetEDEnabled(true);tv.steering_angle_rad=.3f;
        for(unsigned i=0;i<100;++i) {
            TV_Update(&tv);assert(fabsf(tv.delta_power)<=.10001f);
            assert(fabsf(tv.power_left+tv.power_right-3)<.0001f);
        }
    }
    values.budget=0;change(values);TV_Update(&tv);
    assert(tv.power_left==0 && tv.power_right==0 && tv.delta_power==0);
    boot();status=PitTuning_Status();assert(!status.revision);
    assert(djy_pit_values_equal(&status.values,&original.values));
    /* Stationary dwell uses wrapping tick arithmetic. */
    PitTuning_Tick(true,UINT32_MAX-499u);PitTuning_Tick(true,499u);assert(!PitTuning_Status().allowed);
    PitTuning_Tick(true,500u);assert(PitTuning_Status().allowed);
    assert(mask==0);puts("PIT atomic edits, gates, PID response, ramp, TV/ED caps, reset and tick wrap PASS");
}
