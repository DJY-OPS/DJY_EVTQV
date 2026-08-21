#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/* =====================================================================
 *  Board B  —  NUCLEO-F446RE  핀 배치도 (제어 + 출력 + 로깅)
 * ---------------------------------------------------------------------
 *  ★2026-08 재배치: 서브시스템별로 커넥터 위치를 붙여서 하네스를 묶기 쉽게
 *    바꿨다. 배치 원칙은 "센서는 전부 CN7, 출력·통신·로깅은 전부 CN10".
 *
 *  ▣ CN7 — 센서 전용
 *  기능           주변장치          핀      커넥터 위치       AF
 *  -------------  ---------------- ------  ----------------  ----
 *  IMU TX(→센서)  USART3_TX        PC10    CN7-1             AF7
 *  IMU RX(←센서)  USART3_RX        PC11    CN7-2             AF7
 *                 (GND)                    CN7-8
 *  좌 RPM 펄스    TIM2_CH1(IC)     PA0     CN7-28  (A0)      AF1
 *  우 RPM 펄스    TIM2_CH2(IC)     PA1     CN7-30  (A1)      AF1
 *  좌 스로틀 출력 DAC1_OUT1        PA4     CN7-32  (A2)      analog
 *  상태 LED       GPIO_Output      PB0     CN7-34  (A3)      -
 *  TV 토글 스위치 GPIO_Input PU     PC13    CN7-23 / 온보드B1 -
 *
 *  ▣ CN10 — 출력·통신·로깅
 *  기능           주변장치          핀      커넥터 위치       AF
 *  -------------  ---------------- ------  ----------------  ----
 *  우 스로틀 출력 DAC1_OUT2        PA5     CN10-11 (D13)     analog
 *  CAN TX         CAN1_TX          PA12    CN10-12           AF9
 *  CAN RX         CAN1_RX          PA11    CN10-14           AF9
 *  SD CS          GPIO_Output(SW)  PB1     CN10-24           -
 *  SD MOSI        SPI2_MOSI        PB15    CN10-26           AF5
 *  SD MISO        SPI2_MISO        PB14    CN10-28           AF5
 *  SD SCK         SPI2_SCK         PB13    CN10-30           AF5
 *  디버그 UART    USART2 TX/RX     PA2/PA3 CN10-35/37 (VCP)  AF7
 *
 *  ★재배치로 해결된 것
 *   - IMU가 CN10-21/33(12칸 이격) → CN7-1/2(인접)로. GND도 CN7-8로 가까워짐.
 *   - SD CS가 CN10-16(SPI 블록과 7칸 이격) → CN10-24로. SD가 24·26·28·30 연속.
 *   - RPM 펄스가 SD SPI(짝수 26/28/30)와 커넥터에서 맞물려 있던 배치를 해소.
 *     12MHz SPI 클럭 바로 옆을 지나던 SPD 입력을 CN7로 완전히 분리했다.
 *   - TIM3(16비트) → TIM2(32비트). 주기 측정 상한이 655ms에서 사실상 무제한이
 *     되어 극저속 RPM도 측정 가능해졌다.
 *
 *  ★움직일 수 없는 제약
 *   - DAC1_OUT1/OUT2는 칩에서 PA4/PA5에 고정이라 대체 핀이 없다. 그래서 좌/우
 *     스로틀 출력이 CN7과 CN10으로 갈라지는 건 피할 수 없다.
 *   - USART2(PA2/PA3)는 ST-Link 가상COM에 배선돼 있어 옮기면 PuTTY를 못 쓴다.
 *
 *  ★배선 주의: PA0/PA1(A0/A1)이 PA4(A2, 좌 스로틀)와 같은 CN8 헤더에 모인다.
 *    SPD 펄스선은 GND와 트위스트 페어로 묶고 DAC 선과 물리적으로 떨어뜨릴 것.
 *
 *  ★ 온보드 LD2(PA5)는 DAC_OUT2와 같은 핀이므로 사용할 수 없다.
 *    상태 LED는 외부 LED를 PB0에 달아 쓴다(직렬 저항 330Ω, 액티브 High).
 *  ★ Board B는 TPS/SAS를 직접 읽지 않는다. 전부 CAN(0x100)으로 수신한다.
 *  ★ RPM은 CAN이 아니라 컨트롤러(ND72680B) 30핀 커넥터의 **18번 핀**
 *    (ALARM/SPD 겸용, 실측 피크 3.7~4.4V — F446 5V톨러런트 핀에 직결 가능)을
 *    TIM3 Input Capture로 직접 측정한다.
 *    ★★ 13번 핀은 SPD가 아니라 RXD(시리얼)였음 — 착각해서 한참 헤맸음.
 *    12관/NS 시리즈 컨트롤러 기준: 13번=RXD, 18번=ALARM/SPD, 9번=SPA(아날로그,
 *    최대 60V — 절대 직결 금지).
 *    ★★ 소프트웨어 Display 탭의 "SpecialFrame"을 반드시 0으로 설정할 것
 *    (0=순수 속도펄스 모드. 기본값 21은 One-Line 디지털 프로토콜 모드라
 *    펄스로 오인식되어 엉뚱한 값이 나옴 — 실제로 겪었던 문제).
 *    Fardriver 소프트웨어의 "Speed Pulse"(1~16) 설정값과
 *    vehicle_params.h의 RPM_PULSES_PER_REV를 반드시 일치시킬 것.
 *
 *  CubeMX 설정 요약
 *    DAC1   : OUT1/OUT2, Output Buffer Enable, 트리거 None
 *    USART3 : 115200 8N1, RX에 DMA1 Stream1 Ch4 — Circular 모드 필수
 *    SPI2   : Full-Duplex Master, 8-bit, CPOL=Low CPHA=1Edge(Mode0),
 *             초기화 시 <400kHz → 마운트 후 ~12MHz, NSS = Software
 *    CAN1   : 500kbps (Board A와 동일), RX FIFO0 인터럽트 활성
 *    TIM6   : 100Hz 인터럽트 (제어 루프)
 *    TIM2   : Input Capture Direct, CH1/CH2, Rising Edge, Filter=8, Pull-up,
 *             PSC=839(84MHz/840=100kHz→10us/tick), ARR=0xFFFFFFFF(32비트),
 *             NVIC TIM2 global interrupt Enable
 *             ★SPD 신호 노이즈 대비 PA0/PA1 앞단에 RC 저역통과 필터
 *             (직렬 1kΩ + 핀-GND 간 10nF) 하드웨어 추가 권장
 * ===================================================================== */

/* ── DAC : 모터 컨트롤러 스로틀 지령 ───────────────────────────────── */
#define DAC_LEFT_PORT      GPIOA
#define DAC_LEFT_PIN       GPIO_PIN_4    /* PA4  DAC1_OUT1 — 좌측 */
#define DAC_LEFT_CHANNEL   DAC_CHANNEL_1
#define DAC_RIGHT_PORT     GPIOA
#define DAC_RIGHT_PIN      GPIO_PIN_5    /* PA5  DAC1_OUT2 — 우측 */
#define DAC_RIGHT_CHANNEL  DAC_CHANNEL_2

/* ── IMU : WitMotion (USART3 + DMA RX Circular) ────────────────────── */
#define IMU_UART_TX_PORT   GPIOC
#define IMU_UART_TX_PIN    GPIO_PIN_10   /* PC10 USART3_TX (AF7) — CN7-1 */
#define IMU_UART_RX_PORT   GPIOC
#define IMU_UART_RX_PIN    GPIO_PIN_11   /* PC11 USART3_RX (AF7) — CN7-2 */
#define IMU_UART_BAUD      115200u

/* ── CAN1 ──────────────────────────────────────────────────────────── */
#define CAN_RX_PORT        GPIOA
#define CAN_RX_PIN         GPIO_PIN_11   /* PA11 CAN1_RX (AF9) */
#define CAN_TX_PORT        GPIOA
#define CAN_TX_PIN         GPIO_PIN_12   /* PA12 CAN1_TX (AF9) */

/* ── SD 카드 : SPI2 + FATFS ────────────────────────────────────────── */
#define SD_SPI_SCK_PORT    GPIOB
#define SD_SPI_SCK_PIN     GPIO_PIN_13   /* PB13 SPI2_SCK  (AF5) */
#define SD_SPI_MISO_PORT   GPIOB
#define SD_SPI_MISO_PIN    GPIO_PIN_14   /* PB14 SPI2_MISO (AF5) */
#define SD_SPI_MOSI_PORT   GPIOB
#define SD_SPI_MOSI_PIN    GPIO_PIN_15   /* PB15 SPI2_MOSI (AF5) */
#define SD_CS_PORT         GPIOB
#define SD_CS_PIN          GPIO_PIN_1    /* PB1 GPIO 출력, idle=High — CN10-24
                                          * CubeMX에서 User Label을 SD_CS로 두면
                                          * FATFS user_diskio가 SD_CS_GPIO_Port /
                                          * SD_CS_Pin 이름으로 참조한다. */

/* ── TV 토글 스위치 (내부 풀업, 눌림/ON = RESET) ───────────────────── */
#define TV_SWITCH_PORT     GPIOC
#define TV_SWITCH_PIN      GPIO_PIN_13   /* PC13 — 온보드 B1 유저 버튼 */
#define TV_SWITCH_ON_STATE GPIO_PIN_RESET

/* ── 상태 LED (외부, PA5는 DAC가 점유) ─────────────────────────────── */
#define LED_PORT           GPIOB
#define LED_PIN            GPIO_PIN_0    /* PB0 */

/* ── 좌/우 RPM 펄스 (컨트롤러 SPD 핀, TIM2 Input Capture, 32비트) ──── */
#define RPM_LEFT_PORT      GPIOA
#define RPM_LEFT_PIN       GPIO_PIN_0    /* PA0  TIM2_CH1 (AF1) — CN7-28 / A0 */
#define RPM_RIGHT_PORT     GPIOA
#define RPM_RIGHT_PIN      GPIO_PIN_1    /* PA1  TIM2_CH2 (AF1) — CN7-30 / A1 */

#endif /* BOARD_CONFIG_H */
