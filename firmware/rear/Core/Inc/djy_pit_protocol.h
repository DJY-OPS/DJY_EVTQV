#ifndef DJY_PIT_PROTOCOL_H
#define DJY_PIT_PROTOCOL_H
#include "djy_telemetry_protocol.h"

/* A complete six-value transaction, independent of the leased LIVE_TV command.
 * v2/type3 SET, v2/type4 STATUS, LE fixed-point, CRC16. No native structs on wire.
 * SET revision is compare-and-swap; STATUS revision is the current revision.
 * Values are volatile until STM reset. No flash writes. */
#define DJY_PIT_SIZE 42u
#define DJY_PIT_SCHEMA 2u
#define DJY_PIT_SET 3u
#define DJY_PIT_STATUS 4u
enum { DJY_PIT_OK, DJY_PIT_RANGE, DJY_PIT_LOCKED, DJY_PIT_CONFLICT };
typedef struct {
    uint32_t kp, ki, kd, ramp, delta, budget;
} DjyPitValues;
typedef struct {
    uint32_t request_id, revision;
    DjyPitValues values;
    uint8_t status, allowed;
} DjyPitPacket;
static inline bool djy_pit_values_valid(const DjyPitValues *v) {
    return v->kp <= 60000u && v->kp % 100u == 0u &&
        v->ki <= 4000u && v->ki % 100u == 0u &&
        v->kd <= 10000u && v->kd % 100u == 0u &&
        v->ramp >= 10u && v->ramp <= 250u && v->ramp % 10u == 0u &&
        v->delta <= 4000u && v->delta % 100u == 0u &&
        v->budget <= 10000u && v->budget % 100u == 0u;
}
static inline bool djy_pit_values_equal(const DjyPitValues *a, const DjyPitValues *b) {
    return a->kp==b->kp && a->ki==b->ki && a->kd==b->kd &&
        a->ramp==b->ramp && a->delta==b->delta && a->budget==b->budget;
}
static inline void djy_pit_pack(uint8_t *p, uint8_t type, const DjyPitPacket *v) {
    p[0]=0xd5; p[1]=0x4a; p[2]=DJY_PIT_SCHEMA; p[3]=type;
    djy_tm_put_u16(p+4,DJY_PIT_SIZE-8u);
    djy_tm_put_u32(p+6,v->request_id); djy_tm_put_u32(p+10,v->revision);
    djy_tm_put_u32(p+14,v->values.kp); djy_tm_put_u32(p+18,v->values.ki);
    djy_tm_put_u32(p+22,v->values.kd); djy_tm_put_u32(p+26,v->values.ramp);
    djy_tm_put_u32(p+30,v->values.delta); djy_tm_put_u32(p+34,v->values.budget);
    p[38]=v->status; p[39]=v->allowed;
    djy_tm_put_u16(p+40,djy_tm_crc16(p,40));
}
static inline bool djy_pit_unpack(DjyPitPacket *v, const uint8_t *p, uint8_t type) {
    if(p[0]!=0xd5 || p[1]!=0x4a || p[2]!=DJY_PIT_SCHEMA || p[3]!=type ||
       djy_tm_get_u16(p+4)!=DJY_PIT_SIZE-8u ||
       djy_tm_get_u16(p+40)!=djy_tm_crc16(p,40) ||
       p[38]>DJY_PIT_CONFLICT || p[39]>1u) return false;
    v->request_id=djy_tm_get_u32(p+6); v->revision=djy_tm_get_u32(p+10);
    v->values.kp=djy_tm_get_u32(p+14); v->values.ki=djy_tm_get_u32(p+18);
    v->values.kd=djy_tm_get_u32(p+22); v->values.ramp=djy_tm_get_u32(p+26);
    v->values.delta=djy_tm_get_u32(p+30); v->values.budget=djy_tm_get_u32(p+34);
    v->status=p[38]; v->allowed=p[39];
    return true;
}

/* Telemetry v1 and tuning status v2 share one bounded receive stream. */
typedef struct { uint8_t bytes[DJY_TELEMETRY_SIZE]; uint16_t used, consumed; uint32_t errors; } DjyRearRx;
static inline unsigned djy_rear_push(DjyRearRx *r, uint8_t byte) {
    if(r->consumed) {
        r->used-=r->consumed;
        memmove(r->bytes,r->bytes+r->consumed,r->used);
        r->consumed=0;
    }
    r->bytes[r->used++]=byte;
    while(r->used) {
        if(r->bytes[0]!=0xd5) goto discard;
        if(r->used<2) return 0;
        if(r->bytes[1]!=0x4a) goto discard;
        if(r->used<6) return 0;
        unsigned size;
        if(djy_tm_header_valid(r->bytes)) size=DJY_TELEMETRY_SIZE;
        else if(r->bytes[2]==DJY_PIT_SCHEMA && r->bytes[3]==DJY_PIT_STATUS &&
                djy_tm_get_u16(r->bytes+4)==DJY_PIT_SIZE-8u) size=DJY_PIT_SIZE;
        else { ++r->errors; goto discard; }
        if(r->used<size) return 0;
        if(djy_tm_get_u16(r->bytes+size-2)==djy_tm_crc16(r->bytes,size-2)) {
            unsigned kind=r->bytes[3];
            if(r->used==size) r->used=0;
            else r->consumed=(uint16_t)size;
            return kind;
        }
        ++r->errors;
discard:
        --r->used; memmove(r->bytes,r->bytes+1,r->used);
    }
    return 0;
}
#endif
