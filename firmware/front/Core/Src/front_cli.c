#include "front_cli.h"
#include "driver_controls.h"
#include "main.h"

extern UART_HandleTypeDef huart2;

static char s_line[32];
static uint8_t s_length;

static uint16_t text_length(const char *text) {
    uint16_t length = 0u;
    while (text[length] != '\0') ++length;
    return length;
}

static bool text_equals(const char *left, const char *right) {
    while (*left != '\0' && *left == *right) { ++left; ++right; }
    return *left == *right;
}

static bool text_starts_with(const char *text, const char *prefix) {
    while (*prefix != '\0') {
        if (*text++ != *prefix++) return false;
    }
    return true;
}

static void reply(const char *text) {
    (void)HAL_UART_Transmit(&huart2, (uint8_t *)text, text_length(text), 10u);
}

static bool parse_percent(const char *text, uint8_t *value) {
    uint16_t parsed = 0u;
    uint8_t digits = 0u;
    while (*text >= '0' && *text <= '9') {
        parsed = (uint16_t)(parsed * 10u + (uint16_t)(*text - '0'));
        ++text;
        ++digits;
    }
    if (digits == 0u || *text != '\0' || parsed > 100u) return false;
    *value = (uint8_t)parsed;
    return true;
}

static void process_line(void) {
    uint8_t value;
    if (text_starts_with(s_line, "TQV=") && parse_percent(&s_line[4], &value)) {
        DriverControls_SetTvPercent(value); reply("OK TQV\r\n");
    } else if (text_starts_with(s_line, "REGEN=") &&
               parse_percent(&s_line[6], &value)) {
        DriverControls_SetRegenPercent(value); reply("OK REGEN REQUEST\r\n");
    } else if (text_equals(s_line, "TV=ON")) {
        DriverControls_SetTvEnabled(true); reply("OK TV ON\r\n");
    } else if (text_equals(s_line, "TV=OFF")) {
        DriverControls_SetTvEnabled(false); reply("OK TV OFF\r\n");
    } else if (text_equals(s_line, "REGEN=ON")) {
        DriverControls_SetRegenEnabled(true); reply("OK REGEN REQUEST ON\r\n");
    } else if (text_equals(s_line, "REGEN=OFF")) {
        DriverControls_SetRegenEnabled(false); reply("OK REGEN REQUEST OFF\r\n");
    } else if (text_equals(s_line, "MODE=QUALIFYING")) {
        DriverControls_SetMode(DJY_MODE_QUALIFYING); reply("OK MODE\r\n");
    } else if (text_equals(s_line, "MODE=RACE")) {
        DriverControls_SetMode(DJY_MODE_RACE); reply("OK MODE\r\n");
    } else if (text_equals(s_line, "MODE=CHARGE")) {
        DriverControls_SetMode(DJY_MODE_CHARGE); reply("OK MODE\r\n");
    } else if (text_equals(s_line, "MODE=ATTACK")) {
        DriverControls_SetMode(DJY_MODE_ATTACK); reply("OK MODE\r\n");
    } else {
        reply("ERR: TQV=0..100, REGEN=0..100, TV=ON/OFF, REGEN=ON/OFF, MODE=...\r\n");
    }
}

void FrontCli_Init(void) {
    s_length = 0u;
    reply("DJY front CLI ready\r\n");
}

void FrontCli_Process(void) {
    uint8_t byte;
    for (uint8_t i = 0u; i < 16u; ++i) {
        if (HAL_UART_Receive(&huart2, &byte, 1u, 0u) != HAL_OK) return;
        if (byte == '\r' || byte == '\n') {
            if (s_length != 0u) {
                s_line[s_length] = '\0';
                process_line();
                s_length = 0u;
            }
        } else if (s_length < (uint8_t)(sizeof(s_line) - 1u)) {
            s_line[s_length++] = (char)byte;
        } else {
            s_length = 0u;
            reply("ERR LINE TOO LONG\r\n");
        }
    }
}
