# CAN protocol

- Bus: Classical CAN, 500 kbit/s, standard 11-bit identifier
- Multi-byte integer byte order: little endian
- `0x100`, `0x300`은 기존 ESP32 텔레메트리와 호환되도록 유지
- 안전 관련 새 프레임은 CRC-8/SAE-J1850과 순환 카운터 사용

| ID | 방향 | 주기 | DLC | 내용 |
|---:|---|---:|---:|---|
| `0x100` | Front → Rear/ESP | 10 ms | 4 | SAS 14-bit, TPS 12-bit |
| `0x110` | Front → Rear/ESP | 10 ms | 8 | 운전자 설정 |
| `0x120` | ESP → Rear | 정차 설정 시 | 8 | 피트 상한/램프 설정 |
| `0x200` | Left controller → Rear | 장치 의존 | ≥2 | 좌 RPM(레거시 예비 경로) |
| `0x201` | Right controller → Rear | 장치 의존 | ≥2 | 우 RPM(레거시 예비 경로) |
| `0x300` | Front → Rear/ESP | 100 ms | 1 | SAS/TPS 상태 비트 |
| `0x310` | Rear → Front/ESP | 50 ms | 8 | 실제 적용 상태/폴트 |
| `0x311` | Rear → ESP | 50 ms | 8 | RPM L/R, DAC L/R |
| `0x320` | Rear → ESP | 설정 적용 시 | 8 | 피트 설정 ACK |

## 0x110 DRIVER_CONTROL

| Byte | 값 |
|---:|---|
| 0 | TQV 요청 강도 0..100% |
| 1 | 회생 최대 요청 0..100% |
| 2 | Mode: 0 Qualifying, 1 Race, 2 Charge, 3 Attack |
| 3 | bit0 TV enable, bit1 regen request enable |
| 4 | 매 프레임 증가하는 8-bit sequence |
| 5 | 프로토콜 버전, 현재 1 |
| 6 | 예약, 반드시 0 |
| 7 | byte 0..6 CRC-8/SAE-J1850 |

리어는 동일 sequence 반복을 유효성 갱신으로 인정하지 않는다. 150 ms 동안 새 유효 프레임이
없으면 TQV 목표를 0%로 바꾸고 50:50 분배로 램프다운한다. 기본 추진력은 유지한다.

## 0x310 REAR_STATUS

| Byte | 값 |
|---:|---|
| 0 | 실제 적용 중인 TQV 강도 0..100% |
| 1 | 실제 적용 중인 회생 강도(현재 항상 0%) |
| 2 | 적용 모드 |
| 3 | bit0 control fresh, bit1 TV active, bit2 ED active, bit3 regen ready, bit4 fault |
| 4 | 리어 `FaultCode` |
| 5 | 상태 sequence |
| 6 | 프로토콜 버전, 현재 1 |
| 7 | byte 0..6 CRC-8/SAE-J1850 |

공통 정의의 단일 원본은 `shared/include/djy_can_protocol.h`다. ESP32에서도 숫자를 다시
손으로 복사하지 말고 이 헤더 또는 동일 스키마로 생성한 코드를 사용한다.
# 피트/텔레메트리 확장

| ID | 방향 | 주기/조건 | 내용 |
|---|---|---|---|
| `0x120` | ESP → Rear | 정차 설정 시 | TV 상한, 회생 상한, 두 램프, 허용 플래그, seq, CRC |
| `0x311` | Rear → ESP | 20 Hz | RPM L/R, DAC L/R, 각 little-endian uint16 |
| `0x320` | Rear → ESP | 설정 적용 시 | 실제 적용한 `0x120` payload의 ACK |

`0x120`은 TPS 2% 이하, 좌·우 RPM 30 이하, Front 센서 프레임 최신 조건에서만 적용된다.
값은 휘발성이며 리셋 시 TV 상한 100%, 회생 상한 0%, TV 램프 200%/s로 돌아간다.
