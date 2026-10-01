#include "esp_link.h"
#include "pit_tuning.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

uint32_t irq_mask;
static unsigned pit_count;
static DjyPitPacket pit_last;
void PitTuning_Queue(const DjyPitPacket *p) { ++pit_count; pit_last=*p; }
static uint32_t tick, arms, aborts, clears, fail_arms;
static uint8_t *rx_destination;
static UART_HandleTypeDef uart;
uint32_t HAL_GetTick(void) { return tick; }
void clear_errors(UART_HandleTypeDef *h) { (void)h; ++clears; }
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *h, uint8_t *p, uint16_t n) {
    assert(n == 1u);
    ++arms;
    if (fail_arms) { --fail_arms; return HAL_BUSY; }
    if (h->RxState != HAL_UART_STATE_READY) return HAL_BUSY;
    rx_destination = p;
    h->RxState = HAL_UART_STATE_BUSY_RX;
    h->ErrorCode = HAL_UART_ERROR_NONE;
    h->rx_irq = UART_IT_RXNE;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *h) {
    ++aborts;
    h->RxState = HAL_UART_STATE_READY;
    h->ErrorCode = HAL_UART_ERROR_NONE;
    h->rx_irq = 0u;
    return HAL_OK;
}
static void feed_byte(uint8_t v) {
    assert(uart.RxState == HAL_UART_STATE_BUSY_RX && uart.rx_irq);
    *rx_destination = v;
    uart.RxState = HAL_UART_STATE_READY;
    uart.rx_irq = 0;
    HAL_UART_RxCpltCallback(&uart);
}
static void feed(const uint8_t *p, unsigned n) {
    for (unsigned i = 0; i < n; ++i) feed_byte(p[i]);
}
static void reset(uint32_t now) {
    tick = now;
    fail_arms = 0;
    irq_mask = 0;
    EspLink_Init(&uart);
    assert(irq_mask == 0 && !EspLink_IsFresh());
}
int main(void) {
    uint8_t frame[9];
    DjyUartLiveTv input = {213, 50, 80, 1}, out;
    djy_uart_pack_live_tv(frame, &input);
    assert(djy_uart_crc8((const uint8_t *)"123456789", 9) == 0x4b);
    reset(0);
    feed(frame, 9);
    assert(EspLink_GetLiveTv(&out) && out.sequence == 213);
    tick = 500; assert(EspLink_IsFresh());
    tick = 501; assert(!EspLink_IsFresh());
    assert(!EspLink_GetLiveTv(NULL));
    irq_mask = 1; EspLink_GetLiveTv(&out); assert(irq_mask == 1); irq_mask = 0;
    reset(UINT32_MAX - 200u); feed(frame, 9);
    tick = 299; assert(EspLink_IsFresh());
    tick = 300; assert(!EspLink_IsFresh());

    /* Every truncation must recover the first following complete command. */
    for (unsigned cut = 1; cut < 9; ++cut) {
        reset(1000); feed(frame, cut); feed(frame, 9);
        assert(g_esp_rx_count == 1 && EspLink_GetLiveTv(&out));
        assert(out.strength_percent == 50);
        reset(1000); feed(frame, cut); tick += 100; feed(frame, 9);
        assert(g_esp_rx_count == 1);
    }
    /* Insertions, deletion, noise, bad CRC, wrong type/version/range/flags. */
    for (unsigned pos = 0; pos < 9; ++pos) {
        reset(1000); feed(frame, pos); feed_byte(0x66); feed(frame + pos, 9-pos);
        feed(frame, 9); assert(EspLink_IsFresh() && g_esp_rx_count >= 1);
        reset(1000); feed(frame, pos); feed(frame + pos + 1, 8-pos);
        feed(frame, 9); assert(EspLink_IsFresh() && g_esp_rx_count == 1);
    }
    for (unsigned field = 2; field < 9; ++field) {
        if (field == 4) continue; /* Every sequence value is legal. */
        uint8_t bad[9]; memcpy(bad, frame, 9); bad[field] = 0xff;
        if (field != 8) bad[8] = djy_uart_crc8(bad, 8);
        reset(1000); feed(bad, 9); assert(!EspLink_IsFresh());
        feed(frame, 9); assert(EspLink_IsFresh() && g_esp_rx_count == 1);
    }
    reset(1000);
    for (unsigned seq = 0; seq < 1024; ++seq) {
        input.sequence = (uint8_t)seq; djy_uart_pack_live_tv(frame, &input);
        feed(frame, 9);
    }
    assert(g_esp_rx_count == 1024 && g_esp_crc_err == 0);

    /* HAL invokes RxCplt before ErrorCallback on FE/NE/ORE + RXNE.
     * Do not accept even a CRC-correct final byte marked with a UART error. */
    for (unsigned error = 1; error <= 4; error *= 2) {
        reset(1000); feed(frame, 8);
        uint32_t before = arms;
        uart.gState = HAL_UART_STATE_BUSY_TX;
        *rx_destination = frame[8];
        uart.RxState = HAL_UART_STATE_READY; uart.rx_irq = 0;
        uart.ErrorCode = error;
        HAL_UART_RxCpltCallback(&uart);
        assert(arms == before && uart.ErrorCode == error && !EspLink_IsFresh());
        uint32_t clear_before = clears;
        HAL_UART_ErrorCallback(&uart);
        assert(clears == clear_before + 1 && g_esp_uart_err == 1);
        assert(uart.gState == HAL_UART_STATE_BUSY_TX);
        feed(frame, 9); assert(EspLink_IsFresh());
    }
    /* A failed arm is retried by the main loop without rewriting TX state. */
    reset(1000); fail_arms = 1; feed_byte(0);
    assert(g_esp_rearm_fail == 1 && uart.RxState == HAL_UART_STATE_READY);
    irq_mask = 1; EspLink_Watchdog(); assert(irq_mask == 1); irq_mask = 0;
    assert(uart.gState == HAL_UART_STATE_BUSY_TX);
    feed(frame, 9); assert(EspLink_IsFresh());
    uint32_t saved_aborts = aborts; EspLink_Watchdog(); assert(aborts == saved_aborts);
    uart.rx_irq = 0; EspLink_Watchdog();
    assert(uart.rx_irq && aborts == saved_aborts + 1);
    UART_HandleTypeDef other = {0};
    uint32_t saved_rx = g_esp_rx_count, saved_err = g_esp_uart_err;
    HAL_UART_RxCpltCallback(&other); HAL_UART_ErrorCallback(&other);
    assert(g_esp_rx_count == saved_rx && g_esp_uart_err == saved_err);
    /* New pit frames coexist with live frames, including truncated/corrupt input. */
    DjyPitPacket pit={.request_id=123,.values={30000,2000,5000,200,4000,10000}};
    uint8_t wire[DJY_PIT_SIZE];djy_pit_pack(wire,DJY_PIT_SET,&pit);
    for(unsigned cut=0;cut<DJY_PIT_SIZE;++cut) {
        reset(1000);pit_count=0;feed(wire,cut);feed(wire,sizeof(wire));
        assert(pit_count==1 && pit_last.request_id==123);
        feed(frame,9);assert(EspLink_IsFresh());
    }
    for(unsigned i=0;i<DJY_PIT_SIZE;++i) for(unsigned bit=0;bit<8;++bit) {
        uint8_t bad[DJY_PIT_SIZE];memcpy(bad,wire,sizeof(wire));bad[i]^=1u<<bit;
        reset(1000);pit_count=0;feed(bad,sizeof(bad));assert(!pit_count);
        feed(wire,sizeof(wire));assert(pit_count==1);
    }
    reset(1000);pit_count=0;feed(wire,10);tick+=51;feed(wire,sizeof(wire));
    assert(pit_count==1 && !EspLink_IsFresh()); /* Pit edits never renew live leases. */
    EspLink_Init(NULL); EspLink_Watchdog(); assert(!EspLink_IsFresh());
    puts("ESP RX: corruption/resync, timeout/wrap, HAL error ordering and full-duplex recovery PASS");
    return 0;
}
