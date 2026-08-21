#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/* =====================================================================
 *  Board A  —  NUCLEO-F446RE  핀 배치도 (센서 입력 + CAN 송신 전용)
 * ---------------------------------------------------------------------
 *  기능          주변장치          핀      Nucleo 커넥터      AF
 *  ------------  ----------------  ------  -----------------  ----
 *  TPS (아날로그) ADC1_IN0          PA0     CN8-1  (A0)        analog
 *  SAS SCK       SPI1_SCK          PB3     CN10-31 (D3*)      AF5
 *  SAS MISO      SPI1_MISO         PB4     CN10-27 (D5*)      AF5
 *  SAS MOSI      SPI1_MOSI         PB5     CN10-29 (D4*)      AF5
 *  SAS CS        GPIO_Output(SW)   PB6     CN10-17 (D10)      -
 *  CAN RX        CAN1_RX           PA11    CN10-14            AF9
 *  CAN TX        CAN1_TX           PA12    CN10-12            AF9
 *  상태 LED      GPIO_Output       PA5     LD2 (온보드)       -
 *  디버그 UART   USART2 TX/RX      PA2/PA3 ST-Link VCP        AF7
 *  SWD           SWDIO/SWCLK       PA13/PA14                  -
 *
 *  ★ SPI1은 기본핀(PA5/PA6/PA7) 대신 PB3/PB4/PB5로 리맵했다.
 *    SPI1_SCK 기본핀 PA5가 온보드 LD2와 같은 핀이라 동시 사용이 불가능하다.
 *    부작용: PB3(JTDO/SWO) 점유 → SWO 트레이스 출력 불가. SWD 디버깅은 정상.
 *
 *  CubeMX 설정 요약
 *    ADC1  : IN0, 12-bit, 단일변환, 소프트웨어 트리거, 샘플타임 ≥ 84cycles
 *    SPI1  : Full-Duplex Master, Motorola, 16-bit, CPOL=Low CPHA=2Edge(Mode1),
 *            MSB first, Prescaler → ~1MHz, NSS = Software
 *    CAN1  : 500kbps, Prescaler/BitTiming은 APB1 클럭에 맞춰 산출
 *    TIM6  : 100Hz 인터럽트 (제어 주기)
 * ===================================================================== */

/* ── TPS (ADC) ─────────────────────────────────────────────────────── */
#define TPS_ADC_PORT    GPIOA
#define TPS_ADC_PIN     GPIO_PIN_0      /* PA0 = ADC1_IN0 (A0) */
#define TPS_ADC_CHANNEL ADC_CHANNEL_0

/* ── SAS : AS5147 (SPI1, 소프트웨어 CS) ────────────────────────────── */
#define SAS_SPI_SCK_PORT   GPIOB
#define SAS_SPI_SCK_PIN    GPIO_PIN_3   /* PB3  SPI1_SCK  (AF5) */
#define SAS_SPI_MISO_PORT  GPIOB
#define SAS_SPI_MISO_PIN   GPIO_PIN_4   /* PB4  SPI1_MISO (AF5) */
#define SAS_SPI_MOSI_PORT  GPIOB
#define SAS_SPI_MOSI_PIN   GPIO_PIN_5   /* PB5  SPI1_MOSI (AF5) */
#define SAS_CS_PORT        GPIOB
#define SAS_CS_PIN         GPIO_PIN_6   /* PB6  GPIO 출력, idle=High */

/* ── CAN1 ──────────────────────────────────────────────────────────── */
#define CAN_RX_PORT     GPIOA
#define CAN_RX_PIN      GPIO_PIN_11     /* PA11 CAN1_RX (AF9) */
#define CAN_TX_PORT     GPIOA
#define CAN_TX_PIN      GPIO_PIN_12     /* PA12 CAN1_TX (AF9) */

/* ── 상태 LED (NUCLEO LD2) ─────────────────────────────────────────── */
#define LED_PORT        GPIOA
#define LED_PIN         GPIO_PIN_5      /* PA5 — SPI1 리맵으로 확보됨 */

/* ── TPS 유효 범위 (12bit, 3.3V 기준) ──────────────────────────────
 *  ★실측(2026-08-06): 실제 페달 idle~풀프레스 전압 0.7V~2.5V.
 *   0.7V → 0.7/3.3*4095 ≈ 868  (0% 페달)
 *   2.5V → 2.5/3.3*4095 ≈ 3102 (100% 페달)
 *  단선(→0V)·단락(→3.3V) 진단용 밴드. MARGIN은 풀제로 페달 끝단에서
 *  오검출을 막기 위한 여유. */
#define TPS_ADC_MIN     868u
#define TPS_ADC_MAX     3102u
#define TPS_ADC_MARGIN  50u

#define CONTROL_FREQ_HZ 100u
#define HEARTBEAT_DIV   10u    /* 100Hz / 10 = 10Hz */

#endif /* BOARD_CONFIG_H */
