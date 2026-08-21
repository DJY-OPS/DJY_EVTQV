#ifndef DJY_WATCHDOG_H
#define DJY_WATCHDOG_H

#include "stm32f4xx.h"

/* LSI is nominally 32 kHz. Divider 64 and reload 999 produce about 2 s.
 * LSI tolerance changes the exact time, so this is a recovery watchdog rather
 * than a precision timer. IWDG cannot be stopped after it is started. */
#define DJY_IWDG_PRESCALER_DIV64 4u
#define DJY_IWDG_RELOAD_2S       999u

static inline void DjyWatchdog_Init(void) {
    IWDG->KR = 0xccccu; /* start hardware and LSI */
    IWDG->KR = 0x5555u; /* enable PR/RLR writes */
    IWDG->PR = DJY_IWDG_PRESCALER_DIV64;
    IWDG->RLR = DJY_IWDG_RELOAD_2S;
    for (volatile uint32_t wait = 0u; IWDG->SR != 0u && wait < 100000u; ++wait) {
        /* bounded register-update wait */
    }
    IWDG->KR = 0xaaaau;
}

static inline void DjyWatchdog_Kick(void) {
    IWDG->KR = 0xaaaau;
}

#endif /* DJY_WATCHDOG_H */
