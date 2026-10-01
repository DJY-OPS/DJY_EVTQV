#include "pit_tuning.h"
#include "main.h"
#include "vehicle_params.h"
#include "torque_vectoring.h"
#include "control_settings.h"
static DjyPitPacket current, pending, last;
static volatile bool queued;
static bool stationary_seen, last_seen;
static uint32_t stationary_since;
void PitTuning_Init(void) {
    memset(&current,0,sizeof(current));
    current.values=(DjyPitValues){(uint32_t)(PID_KP*1000), (uint32_t)(PID_KI*1000),
        (uint32_t)(PID_KD*1000),200u,(uint32_t)(DELTA_POWER_MAX_KW*1000),
        (uint32_t)(P_SUM_MAX_KW*1000)};
    queued=stationary_seen=last_seen=false;
}
void PitTuning_Queue(const DjyPitPacket *packet) {
    uint32_t mask=__get_PRIMASK(); __disable_irq();
    if(!queued) { pending=*packet; queued=true; }
    __set_PRIMASK(mask);
}
void PitTuning_Tick(bool stationary, uint32_t now) {
    if(!stationary) stationary_seen=false;
    else if(!stationary_seen) { stationary_since=now; stationary_seen=true; }
    current.allowed=stationary_seen && now-stationary_since>=1000u;
    if(!queued) return;
    DjyPitPacket request=pending; queued=false;
    /* Retransmission reports the same outcome, without resetting PID again. */
    if(last_seen && request.request_id==last.request_id && request.revision==last.revision &&
       djy_pit_values_equal(&request.values,&last.values)) return;
    current.request_id=request.request_id;
    if(!request.request_id || !djy_pit_values_valid(&request.values)) current.status=DJY_PIT_RANGE;
    else if(request.revision!=current.revision ||
            (last_seen && request.request_id==last.request_id)) current.status=DJY_PIT_CONFLICT;
    else if(!current.allowed) current.status=DJY_PIT_LOCKED;
    else {
        current.values=request.values;
        TV_ApplyPitValues(&current.values);
        ControlSettings_SetTvRamp(current.values.ramp);
        ++current.revision;
        current.status=DJY_PIT_OK;
    }
    last=request; last_seen=true;
}
DjyPitPacket PitTuning_Status(void) {
    uint32_t mask=__get_PRIMASK(); __disable_irq();
    DjyPitPacket value=current;
    __set_PRIMASK(mask); return value;
}
