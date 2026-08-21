# DJY EV Torque Vectoring Firmware

STM32F446RE 프론트/리어 제어기와 공통 CAN 프로토콜을 한곳에서 관리하는 프로젝트다.

현재 구현된 범위와 아직 실제 차량에서 검증해야 할 항목은
[토크벡터링 구현 현황](docs/IMPLEMENTATION_STATUS.md)에 정리되어 있다.

## 구조

```text
DJY_TQV/
├─ firmware/
│  ├─ front/             프론트 STM32CubeIDE 프로젝트 (`stm_front`)
│  └─ rear/              리어 STM32CubeIDE 프로젝트 (`stm_back`)
├─ shared/include/       두 보드와 ESP32가 공유할 CAN 프로토콜
├─ docs/                 CAN, 하드웨어, 검증 문서
└─ tests/                PC에서 실행하는 프로토콜/설정 단위 테스트
```

프론트는 SAS와 TPS를 100 Hz로 측정해 CAN으로 보내며, TQV 강도·회생 요청·주행 모드를
`0x110`으로 전송한다. 리어는 프레임의 CRC, 버전, 범위, 순환 카운터, 타임아웃을 확인한 뒤
TQV 강도를 설정된 램프로 적용한다. 리어 적용 상태는 `0x310`, RPM/DAC는 `0x311`으로
20 Hz 회신되어 ESP32에서도 같은 값을 읽을 수 있다. 정차 중 피트 상한·램프 설정은
`0x120`, Rear STM 적용 확인은 `0x320` ACK를 사용한다.

스티어링 휠의 10 kΩ TQV/REGEN 포텐셔미터는 각각 Front STM PC0(A5), PC1(A4)에
연결한다. 펌웨어가 100 Hz로 읽어 5% 단위 물리 다이얼 요청으로 `0x110`에 싣는다.

## 중요한 현재 제한

TQV 강도 조절은 실제 차동전력에 연결되어 있다. 회생제동은 요청과 상태 전달만 구현되어
있으며 실제 모터 컨트롤러 출력은 항상 0%다. ND72680B의 정확한 회생 입력 방식, 이중 브레이크
센서, BMS 충전허용 전류·전압·온도 제한이 확인되기 전에는 이 제한을 제거하면 안 된다.

주행 모드는 현재 표시·통신 상태다. 모드에 따라 최대출력을 바꾸는 기능은 차량별 출력 한계가
확정된 뒤 별도의 캘리브레이션으로 추가한다.

리어의 기존 FATFS `user_diskio.c`는 CubeMX 기본 템플릿으로 실제 SD SPI 블록 드라이버가
아직 없다. 따라서 SD 카드는 마운트되지 않으며 제어는 로깅 없이 계속된다. SD 로깅을 사용하려면
카드 모듈에 맞는 disk I/O 드라이버를 별도로 완성해야 한다.

## CubeIDE 빌드

1. STM32CubeIDE에서 `File > Import > Existing Projects into Workspace`를 선택한다.
2. `firmware/front`와 `firmware/rear`를 각각 가져온다. 프로젝트 이름은 `stm_front`,
   `stm_back`이라 충돌하지 않는다.
3. 각 프로젝트에서 `Build Configurations > Set Active > Debug` 후 빌드한다.
4. NUCLEO-F446RE를 ST-Link로 연결하고 해당 프로젝트의 Run/Debug로 기록한다.

이 PC에는 현재 ARM 임베디드 툴체인이 설치되어 있지 않으므로 저장소 안에서는 공통 로직의
호스트 단위 테스트와 Cortex-M4 대상 구문 검사까지 실행했다. 실제 STM32 링크 빌드는
CubeIDE에서 한 번 더 확인해야 한다.

## 프론트 벤치 CLI

프론트 ST-Link VCP를 115200 8N1로 열고 다음 명령을 줄바꿈과 함께 보낼 수 있다.

```text
TQV=50
TV=ON
TV=OFF
REGEN=30
REGEN=ON
REGEN=OFF
MODE=QUALIFYING
MODE=RACE
MODE=CHARGE
MODE=ATTACK
```

이 CLI는 벤치 시운전용이다. 실차의 두 물리 다이얼은 `dial_inputs.c`에 연결되어 있다.
무선 피트 명령은 리어 출력을 직접 덮어쓰지 않고 물리 다이얼 요청의 상한과 램프만 조정한다.

실제 배선과 단계별 검증은 [하드웨어/시운전 문서](docs/HARDWARE_AND_COMMISSIONING.md),
프레임 정의는 [CAN 프로토콜 문서](docs/CAN_PROTOCOL.md)를 따른다.
