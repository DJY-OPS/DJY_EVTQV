# Hardware and commissioning

## 보드 역할

- Front NUCLEO-F446RE: TPS(PA0), AS5147 SAS(SPI1 PB3/PB4/PB5, CS PB6),
  CAN1(PA11/PA12), ST-Link VCP USART2(PA2/PA3)
- Rear NUCLEO-F446RE: 좌/우 RPM TIM2(PA0/PA1), IMU USART3(PC10/PC11),
  좌/우 스로틀 DAC(PA4/PA5), CAN1(PA11/PA12), SD SPI2(PB13/PB14/PB15, CS PB1)

MCU CAN 핀을 차량 CANH/CANL에 직접 연결하면 안 된다. 양쪽 모두 적절한 CAN 트랜시버를
거쳐야 하고, 전원을 끈 상태에서 버스 양 끝 120 ohm 종단과 공통 기준 접지를 확인한다.
세부 커넥터 위치는 각 프로젝트의 `Core/Inc/board_config.h`가 기준이다.

## 최초 시운전 순서

1. 구동륜을 지면에서 분리하고 비상정지 수단을 준비한다.
2. 리어 PA4/PA5를 모터 컨트롤러에서 분리한 상태로 두 보드 CAN 통신부터 확인한다.
3. 프론트 CLI에서 `TQV=0`, `TV=OFF`, `REGEN=OFF`를 전송한다.
4. CAN 분석기로 `0x100` 100 Hz, `0x110` 100 Hz, `0x300` 10 Hz,
   `0x310` 20 Hz와 CRC/sequence를 확인한다.
5. TPS 단선, SAS 오류, 프론트 전원 차단을 각각 시험한다. TPS/CAN 오류에서는 리어 DAC가
   안전 전압으로 가야 하고, 설정 프레임만 끊겼을 때는 기본 추진을 유지하며 차동만 0%로
   내려가야 한다.
6. DAC를 컨트롤러에 연결하기 전에 멀티미터/오실로스코프로 off 및 전 구간 전압을 검증한다.
7. 저출력 제한 상태에서 `TQV=10`부터 올리며 좌/우 출력 방향과 조향 부호를 확인한다.

## 회생제동 활성화 전 필수 항목

- ND72680B 정확한 모델의 회생 입력 핀 또는 CAN 명령 사양
- 독립적인 이중 브레이크 입력과 두 채널 상호 타당성 검사
- BMS의 SOC, 팩 전압, 최대 충전전류, 셀 과전압, 온도 정보
- 명령 타임아웃 시 회생 0%, 센서 불일치 시 회생 0%
- 기계식/유압식 제동은 STM32와 독립적으로 정상 작동

현재 펌웨어는 이 조건이 충족되지 않아 회생 실제 출력이 코드상 0%로 잠겨 있다.

## 워치독과 SD 카드

두 보드는 약 2초 IWDG를 사용한다. 정상 메인 루프에서만 갱신하므로 메인 또는 제어 인터럽트가
멈추면 재부팅된다. 리어 `Error_Handler()`는 DAC가 초기화된 상태라면 재부팅 대기 전에 양쪽을
off 전압으로 내린다.

현재 `FATFS/Target/user_diskio.c`는 CubeMX 기본 템플릿이며 실제 SPI SD 드라이버가 아니다.
마운트 실패 시 로거는 비활성화되고 TQV 제어는 계속된다. SD를 사용하기 전 카드 모듈에 맞춘
SPI 초기화/read/write/ioctl 구현과 전원 차단 복구 시험이 필요하다.
