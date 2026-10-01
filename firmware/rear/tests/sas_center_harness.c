#include <stdio.h>
#include <assert.h>
#include <math.h>
#include "main.h"
#include "vehicle_params.h"
#include "sas_center.h"
uint32_t g_flash[0x20000 / 4]; uint32_t g_tick, g_rx_us, g_erases, g_programs;
static void tick(uint16_t raw, float v, float yaw, float ay, uint16_t l, uint16_t r) { ++g_rx_us; g_tick += 10; SasCenter_Update(raw, v, yaw, ay, l, r, true); }
static void stop_for(uint32_t ms) { for (uint32_t t = 0; t < ms; t += 10) { g_tick += 10; SasCenter_Service(true); } SasCenter_Service(false); }
int main(void) {
    memset(g_flash, 0xFF, sizeof g_flash);
    /* 1. empty flash -> default */
    SasCenter_Init(); assert(SasCenter_GetSource() == SAS_CENTER_SRC_DEFAULT && SasCenter_GetRaw() == SAS_CENTER_RAW);
    /* 2. wheels-up stand: no yaw ever -> must not learn */
    for (int i = 0; i < 1000; ++i) tick(6000, 5.0f, 0.0f, 0.0f, 1000, 1000);
    assert(SasCenter_GetRaw() == SAS_CENTER_RAW);
    /* 3. wrap math */
    assert(fabsf(SasCenter_Offset((float)((SAS_CENTER_RAW + 8000) % 16384)) - 8000.0f) < 0.5f);
    assert(fabsf(SasCenter_Offset((float)((SAS_CENTER_RAW + 9000) % 16384)) - (9000.0f - 16384.0f)) < 0.5f);
    /* 4. start from a stored 6810, drive: one real turn, then straight at 7092 */
    memset(g_flash, 0xFF, sizeof g_flash);
    g_flash[0] = 0x5A5A0000u | 6810u; g_flash[1] = ~g_flash[0];
    SasCenter_Init(); assert(SasCenter_GetSource() == SAS_CENTER_SRC_FLASH && SasCenter_GetRaw() == 6810);
    tick(4000, 5.0f, 0.6f, 3.0f, 900, 1100);
    for (int i = 0; i < 99; ++i) tick(7092, 8.0f, 0.01f, 0.2f, 2000, 2010);
    assert(SasCenter_GetSource() == SAS_CENTER_SRC_FLASH);          /* not yet 100 samples */
    tick(7092, 8.0f, 0.01f, 0.2f, 2000, 2010);
    assert(SasCenter_GetSource() == SAS_CENTER_SRC_STRAIGHT && SasCenter_GetRaw() == 7092);
    for (int i = 0; i < 300; ++i) tick(7092, 8.0f, 0.0f, 0.0f, 2000, 1000);   /* wheelspin: ignored */
    for (int i = 0; i < 300; ++i) tick(5000, 8.0f, 0.0f, 0.0f, 2000, 2000);   /* far value: gated, then re-capture */
    for (int i = 0; i < 300; ++i) tick(7100, 8.0f, 0.0f, 0.0f, 2000, 2000);
    /* 5. stop >2 s -> saved once; reboot -> same value from flash */
    uint16_t learned = SasCenter_GetRaw(); uint32_t p0 = g_programs;
    stop_for(3000); assert(g_programs == p0 + 2);
    stop_for(3000); assert(g_programs == p0 + 2);   /* unchanged value -> no new record */
    SasCenter_Init(); assert(SasCenter_GetSource() == SAS_CENTER_SRC_FLASH && SasCenter_GetRaw() == learned);
    assert(abs((int)SasCenter_GetRaw() - 7100) <= 2);
    /* 6. big slip: grounded run straight at raw 5000 -> capture 5000 -> ALARM */
    tick(5000, 5.0f, 0.5f, 3.0f, 1000, 1000);
    for (int i = 0; i < 400; ++i) tick(5000, 8.0f, 0.0f, 0.0f, 2000, 2000);
    printf("slip test: center %u level %u\n", SasCenter_GetRaw(), SasCenter_GetLevel());
    assert(SasCenter_GetLevel() == SAS_CENTER_LEVEL_ALARM && abs((int)SasCenter_GetRaw() - 5000) < 5);
    /* 6b. sensor re-assembled far outside the learning window: alarm, then recovery */
    memset(g_flash, 0xFF, sizeof g_flash);
    SasCenter_Init();
    tick(4000, 5.0f, 0.6f, 3.0f, 900, 1100);
    for (int i = 0; i < 150; ++i) tick((uint16_t)(SAS_CENTER_RAW + 5000), 8.0f, 0.0f, 0.0f, 2000, 2000);
    assert(SasCenter_GetLevel() == SAS_CENTER_LEVEL_ALARM && SasCenter_GetRaw() == SAS_CENTER_RAW);
    for (int i = 0; i < 100; ++i) tick(7100, 8.0f, 0.0f, 0.0f, 2000, 2000);
    assert(SasCenter_GetLevel() == SAS_CENTER_LEVEL_OK && SasCenter_GetRaw() == 7100);
    /* 7. full sector -> erased at boot, value preserved via fallback */
    for (unsigned i = 0; i < 0x20000 / 4; i += 2) { g_flash[i] = 0x5A5A0000u | 7100u; g_flash[i + 1] = ~g_flash[i]; }
    uint32_t e0 = g_erases; SasCenter_Init(); assert(g_erases == e0 + 1 && SasCenter_GetRaw() == 7100);
    puts("SAS center: stand guard, wrap, replay capture, save/reload, slip alarm, out-of-window alarm, full-sector erase PASS");
    return 0;
}
