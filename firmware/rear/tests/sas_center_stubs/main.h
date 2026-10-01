#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
typedef enum { HAL_OK = 0, HAL_ERROR = 1 } HAL_StatusTypeDef;
#define FLASH_TYPEERASE_SECTORS 0u
#define FLASH_SECTOR_7 7u
#define FLASH_VOLTAGE_RANGE_3 2u
#define FLASH_TYPEPROGRAM_WORD 2u
#define FLASH_FLAG_EOP 0
#define FLASH_FLAG_OPERR 0
#define FLASH_FLAG_WRPERR 0
#define FLASH_FLAG_PGAERR 0
#define FLASH_FLAG_PGPERR 0
#define FLASH_FLAG_PGSERR 0
#define __HAL_FLASH_CLEAR_FLAG(x) ((void)(x))
typedef struct { uint32_t TypeErase, Banks, Sector, NbSectors, VoltageRange; } FLASH_EraseInitTypeDef;
extern uint32_t g_flash[0x20000 / 4];
extern uint32_t g_tick, g_rx_us, g_erases, g_programs;
#define SAS_FLASH_BASE ((uintptr_t)g_flash)
static inline void HAL_FLASH_Unlock(void) {}
static inline void HAL_FLASH_Lock(void) {}
static inline uint32_t HAL_GetTick(void) { return g_tick; }
static inline HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *e, uint32_t *bad) { (void)e; *bad = 0xFFFFFFFFu; memset(g_flash, 0xFF, sizeof g_flash); ++g_erases; return HAL_OK; }
static inline HAL_StatusTypeDef HAL_FLASH_Program(uint32_t t, uintptr_t a, uint64_t d) { (void)t; *(uint32_t *)a &= (uint32_t)d; ++g_programs; return HAL_OK; }
