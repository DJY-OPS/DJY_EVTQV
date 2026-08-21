# 토크벡터링 구현 현황

기준일: 2026-08-21

이 문서는 현재 `DJY_TQV` 소스가 실제로 어디까지 구현되어 있는지, 보드에 올리기 전에
무엇을 추가 확인해야 하는지를 정리한다.

## 현재 전체 구조

```text
스티어링 휠 물리 다이얼
  ├─ TQV 10 kΩ 포텐셔미터 ─ PC0(A5)
  └─ REGEN 10 kΩ 포텐셔미터 ─ PC1(A4)
                       │
                       v
Front STM32F446RE ── CAN 500 kbit/s ──> Rear STM32F446RE
   0x100 센서                         │
   0x110 운전자 요청                  ├─ 좌/우 DAC → 모터 컨트롤러
                                      ├─ 0x310 실제 적용 상태
ESP32 <──── 같은 차량 CAN ────────────┼─ 0x311 RPM/DAC
   └─ 0x120 피트 상한/램프 ──────────>└─ 0x320 설정 ACK
```

## 구현 완료된 부분

### 1. Front STM 물리 다이얼

- TQV 다이얼: `PC0 / ADC1_IN10 / Nucleo A5`
- 회생 다이얼: `PC1 / ADC1_IN11 / Nucleo A4`
- 100 Hz 측정
- 간단한 저역통과 필터 적용
- 운전 중 값이 흔들리지 않도록 5% 단위로 양자화
- 0% 위치에서는 해당 기능 enable 요청도 해제
- TPS와 다이얼이 ADC1을 공유하므로 TPS 측정 전에 항상 PA0 채널을 다시 선택

관련 파일:

- `firmware/front/Core/Src/dial_inputs.c`
- `firmware/front/Core/Inc/dial_inputs.h`
- `firmware/front/Core/Src/tps_sensor.c`
- `firmware/front/Core/Src/main.c`

### 2. Front → Rear 운전자 설정 통신

`0x110 DRIVER_CONTROL`을 100 Hz로 보낸다.

- TQV 요청 0~100%
- 회생 요청 0~100%
- 주행 모드
- TQV/회생 enable 비트
- 매 프레임 증가하는 sequence
- 프로토콜 버전
- CRC-8/SAE-J1850

Rear STM은 CRC, 버전, 범위, sequence 반복 여부를 검사한다. 마지막 정상 프레임 이후
150 ms 이상 지나면 설정 통신을 stale로 판정하고 TQV를 0% 방향으로 램프다운한다.

### 3. Rear STM 토크벡터링 강도 적용

물리 다이얼의 TQV 요청값은 `TV_SetStrength()`를 통해 최종 좌·우 차동출력 강도에
연결되어 있다.

- 요청값을 그대로 계단 입력하지 않고 설정된 변화율로 램프 적용
- 피트 TQV 상한보다 큰 다이얼 요청은 상한에서 제한
- Front 설정 통신이 끊기면 50:50 방향으로 복귀
- IMU/RPM/SAS 안전 판정은 기존 `Safety_Update()` 경로를 계속 사용
- STOP 진입 시 DAC 안전상태, TV/ED 비활성화, PID 적분 및 필터 상태 초기화
- TQV와 ED는 동시에 개입하지 않도록 배타적으로 동작

관련 파일:

- `firmware/rear/Core/Src/control_settings.c`
- `firmware/rear/Core/Src/torque_vectoring.c`
- `firmware/rear/Core/Src/safety_monitor.c`
- `firmware/rear/Core/Src/main.c`

### 4. 정차 피트 설정

PC/ESP에서 실제 좌·우 DAC 값을 직접 쓰지는 않는다. 물리 다이얼 요청에 적용할 다음
휘발성 설정만 변경한다.

- TQV 최대 허용률
- 회생 최대 허용률
- TQV 변화율 10~250%/s
- 회생 변화율 5~100%/s
- TQV/회생 허용 플래그

Rear STM은 다음 조건에서만 `0x120 PIT_CONFIG`를 적용한다.

1. Front 센서 CAN 프레임이 최신 상태
2. TPS 2% 이하
3. 좌·우 모터 RPM 각각 30 이하
4. CRC, 버전, 범위가 정상

적용에 성공하면 동일 설정을 `0x320 PIT_CONFIG_ACK`로 돌려보낸다. 설정은 Flash에
저장하지 않으며 전원을 다시 켜면 TQV 상한 100%, 회생 상한 0%, TQV 램프 200%/s의
안전 기본값으로 복귀한다.

### 5. ESP/텔레메트리용 상태 프레임

- `0x310`: TQV 실제 적용률, 회생 실제 적용률, 모드, 활성 상태, fault, sequence, CRC
- `0x311`: 좌·우 RPM 및 좌·우 DAC
- `0x320`: 피트 설정 적용 ACK

공통 ID, payload, CRC 구현은 `shared/include/djy_can_protocol.h`를 단일 기준으로 사용한다.

## 원래 코드에서 실제로 바뀐 점

기존에도 Front 센서값을 Rear로 보내고 Rear가 좌·우 모터를 연동하는 기본 구조는 있었다.
이번에 추가·변경된 부분은 다음이다.

- 물리 TQV/회생 다이얼 ADC 입력 추가
- 운전자 설정 전용 `0x110` 프레임 및 CRC/sequence/timeout 추가
- 요청값과 Rear 실제 적용값 분리
- TQV 강도에 피트 상한과 가변 램프 적용
- 정차 피트 설정 `0x120`과 Rear ACK `0x320` 추가
- ESP가 읽을 RPM/DAC 프레임 `0x311` 추가
- 통신 단절 시 TQV 램프다운 동작 명시
- STOP 시 PID windup 방지를 위한 TV 내부 상태 초기화
- 공통 프로토콜을 Front/Rear/ESP가 함께 사용할 수 있도록 분리

## 아직 완료되지 않은 부분

### 실제 회생제동 출력

회생 다이얼 입력, CAN 요청, 피트 상한, 화면 표시 경로는 구현되어 있지만 Rear STM의
`regen_applied_percent`는 의도적으로 항상 0%다. 다음 항목이 확인되기 전에는 이 제한을
제거하면 안 된다.

- ND72680B가 사용하는 실제 회생 명령 핀 또는 CAN 프로토콜
- 브레이크 센서 이중화 및 plausibility 검사
- BMS 충전 허용 전류
- 셀 최고전압과 배터리 온도 제한
- 저속 및 휠 잠김 시 회생 해제 조건
- 회생 명령 단절 시 컨트롤러가 0 토크로 돌아오는지 확인

### 실차 최종 검증

- STM32CubeIDE에서 Front/Rear 프로젝트 최종 ARM 링크 빌드
- 두 STM 보드에 실제 플래시
- 다이얼 끝점 및 회전 방향 확인
- CAN 종단저항을 포함한 실차 버스 검증
- 바퀴를 띄운 상태에서 좌·우 DAC 방향 및 상한 검증
- 저속부터 단계적으로 TQV 강도 증가

현재 PC 호스트에서 공통 프로토콜, CRC, 설정 상한, 램프, timeout/fallback 로직의 단위
테스트는 통과했다. 이 결과가 실제 차량 검증을 대신하지는 않는다.

## 검증 명령

호스트 단위 테스트:

```powershell
clang -std=c11 -Wall -Wextra -Werror `
  -I shared\include `
  -I firmware\front\Core\Inc `
  -I firmware\rear\Core\Inc `
  tests\test_protocol.c `
  firmware\front\Core\Src\driver_controls.c `
  firmware\rear\Core\Src\control_settings.c `
  -o protocol_test.exe
.\protocol_test.exe
```

현재 결과: `protocol/control tests: PASS`

