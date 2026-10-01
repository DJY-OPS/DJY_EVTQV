#include "esp_link.h"
#include "pit_tuning.h"
#include <string.h>

static UART_HandleTypeDef *s_uart;
static uint8_t s_rx_byte;
static uint8_t s_frame[DJY_PIT_SIZE];
static uint8_t s_length;
static uint32_t s_last_byte_ms;
static volatile DjyUartLiveTv s_command;
static volatile uint32_t s_last_rx_ms;
static volatile bool s_seen;

/* Exposed for CubeIDE Live Expressions. */
volatile uint32_t g_esp_rx_count;
volatile uint32_t g_esp_crc_err;
volatile uint32_t g_esp_byte_count;
volatile uint32_t g_esp_rearm_fail;
volatile uint32_t g_esp_uart_err;

/* Caller owns RX state (UART IRQ or a short PRIMASK critical section).
 * Never abort TX or force HAL state fields: the telemetry frame owns TX. */
static void restart_receive(void) {
    if (s_uart != 0 &&
        HAL_UART_Receive_IT(s_uart, &s_rx_byte, 1u) != HAL_OK) {
        ++g_esp_rearm_fail;
    }
}

void EspLink_Watchdog(void) {
    if (s_uart == 0) return;
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    if (s_uart->RxState != HAL_UART_STATE_BUSY_RX ||
        !__HAL_UART_GET_IT_SOURCE(s_uart, UART_IT_RXNE)) {
        s_length = 0u;
        (void)HAL_UART_AbortReceive(s_uart);
        /* F4 clears PE/FE/NE/ORE together with ONE SR/DR read sequence. */
        __HAL_UART_CLEAR_OREFLAG(s_uart);
        restart_receive();
    }
    __set_PRIMASK(mask);
}

static void discard_byte(void) {
    --s_length;
    memmove(s_frame, s_frame + 1u, s_length);
}

static void consume_byte(uint8_t value) {
    uint32_t now = HAL_GetTick();
    ++g_esp_byte_count;
    /* The longest command takes <4 ms in either supported mode. Do not join
     * an old truncated command to one received after the next 100 ms period. */
    if (s_length != 0u && now - s_last_byte_ms > ESP_LINK_FRAME_GAP_MS) {
        s_length = 0u;
    }
    s_last_byte_ms = now;
    s_frame[s_length++] = value;
    while (s_length != 0u) {
        if (s_frame[0] != DJY_UART_MAGIC_0) {
            discard_byte();
            continue;
        }
        if (s_length < 2u) return;
        if (s_frame[1] != DJY_UART_MAGIC_1) {
            discard_byte();
            continue;
        }
        if (s_length < 4u) return;
        if(s_frame[2]==DJY_PIT_SCHEMA && s_frame[3]==DJY_PIT_SET) {
            if(s_length>=6u && djy_tm_get_u16(s_frame+4)!=DJY_PIT_SIZE-8u) {
                ++g_esp_crc_err; discard_byte(); continue;
            }
            if(s_length < DJY_PIT_SIZE) return;
            DjyPitPacket packet;
            if(djy_pit_unpack(&packet,s_frame,DJY_PIT_SET) && packet.status==0 && packet.allowed==0) {
                PitTuning_Queue(&packet); s_length=0; return;
            }
            ++g_esp_crc_err; discard_byte(); continue;
        }
        if(s_frame[2]!=DJY_UART_PROTOCOL_VERSION || s_frame[3]!=DJY_UART_TYPE_LIVE_TV) {
            ++g_esp_crc_err; discard_byte(); continue;
        }
        if (s_length < DJY_UART_LIVE_TV_SIZE) return;
        DjyUartLiveTv decoded;
        if (djy_uart_unpack_live_tv(&decoded, s_frame)) {
            s_command = decoded;
            s_last_rx_ms = now;
            s_seen = true;  /* Tick zero and 32-bit wrap are valid timestamps. */
            ++g_esp_rx_count;
            s_length -= DJY_UART_LIVE_TV_SIZE;
            memmove(s_frame,s_frame+DJY_UART_LIVE_TV_SIZE,s_length);
            continue;
        }
        ++g_esp_crc_err;
        /* Retain any next header embedded inside a rejected frame. */
        discard_byte();
    }
}

void EspLink_Init(UART_HandleTypeDef *uart) {
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    if (s_uart != 0) (void)HAL_UART_AbortReceive(s_uart);
    s_uart = uart;
    s_length = 0u;
    s_last_byte_ms = 0u;
    s_last_rx_ms = 0u;
    s_seen = false;
    g_esp_rx_count = 0u;
    g_esp_crc_err = 0u;
    g_esp_byte_count = 0u;
    g_esp_rearm_fail = 0u;
    g_esp_uart_err = 0u;
    memset((void *)&s_command, 0, sizeof(s_command));
    restart_receive();
    __set_PRIMASK(mask);
}

bool EspLink_GetLiveTv(DjyUartLiveTv *command) {
    if (command == 0) return false;
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    *command = s_command;
    uint32_t last_rx_ms = s_last_rx_ms;
    bool seen = s_seen;
    __set_PRIMASK(mask);
    return seen && (HAL_GetTick() - last_rx_ms) <= ESP_LINK_TIMEOUT_MS;
}

bool EspLink_IsFresh(void) {
    DjyUartLiveTv command;
    return EspLink_GetLiveTv(&command);
}

uint32_t EspLink_GetRxCount(void) { return g_esp_rx_count; }
uint32_t EspLink_GetCrcErrorCount(void) { return g_esp_crc_err; }

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (s_uart != 0 && huart == s_uart) {
        /* HAL may call RX complete BEFORE ErrorCallback for the same byte.
         * Rearming here clears ErrorCode and can suppress ORE recovery. */
        if (huart->ErrorCode != HAL_UART_ERROR_NONE) {
            ++g_esp_byte_count;
            s_length = 0u;
            return;
        }
        consume_byte(s_rx_byte);
        restart_receive();
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (s_uart != 0 && huart == s_uart) {
        ++g_esp_uart_err;
        s_length = 0u;
        (void)HAL_UART_AbortReceive(huart);
        __HAL_UART_CLEAR_OREFLAG(huart);
        restart_receive();
    }
}
