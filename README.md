# DJY EV Torque Vectoring Firmware

2026-09-21 현재 차량 작업에 사용한 `Documents/TV` 전방·후방 STM 소스 스냅샷입니다.

- `firmware/rear`: 2026-09-20 마지막 후방 SAS 영점 보정 적용에 사용한 `stm_back` 소스.
- `firmware/front`: 같은 작업 폴더의 시간 동기화·센서 계측을 포함한 `stm_front` 소스.
- 전방·후방 프로젝트는 각각 STM32CubeIDE에서 가져와 빌드할 수 있습니다.

## 고속 텔레메트리 변경

후방 USART1의 ESP 송신을 238바이트 CRC16 바이너리, 460800 baud,
최대 100 Hz로 변경했다. USART2(ST-Link) 진단은 115200 baud, 최대 5 Hz를 유지한다.
전방 소스와 아래 차량 설정은 이 통신 변경에서 수정하지 않았다.
바이너리 수신을 지원하는 ESP 펌웨어와 함께 적용해야 한다.
[이식 범위와 검증](docs/STM_TELEMETRY_PORT.md)을 참고하세요.

## 후방 적용 설정

| 항목 | 값 |
|---|---|
| 합계 출력 요구 상한 | 10 kW |
| 모터별 출력 요구 상한 | 7 kW |
| SAS 영점 | 6845 counts |
| PID Kp / Ki / Kd | 50 / 1 / 5 |
| 타이어 지름 / 감속비 | 45 cm / 4:1 |

출력 요구량을 DAC 스로틀 전압으로 변환합니다. 실측 팩 전력을 피드백해 10 kW로 제한하는 구조는 아닙니다.
조향 영점은 해당 차량의 장착 상태에 맞춘 값입니다.

## 빌드와 검증

1. STM32CubeIDE에서 `firmware/front`, `firmware/rear`를 Existing Projects로 가져옵니다.
2. 프로젝트 이름은 각각 `stm_front`, `stm_back`입니다.
3. 실제 마지막 후방 적용 구성은 Debug입니다. 빌드와 장치 다운로드는 별도 작업입니다.
4. 호스트 ΔP 회귀 검증: `python -m unittest discover -s firmware/rear/tests -p test_delta_allocation.py`.
   GCC 또는 `DJY_HOST_ZIG`가 가리키는 Zig가 필요합니다.

이번 업로드는 소스 보관 작업이며 차량에 새 펌웨어를 기록하지 않았습니다.
[적용 버전·검증 기록](docs/DEPLOYED_FIRMWARE_20260921.md)과
[소스 SHA256 목록](docs/DEPLOYED_SOURCE_SHA256.json)을 참고하세요.

## 이전 자료

기존 `docs/`의 구현 현황·CAN·하드웨어 설명과 `shared/include/`는 이전 저장소 구성의 자료도 포함합니다.
현재 적용 동작을 판단할 때는 `firmware/front`, `firmware/rear`의 소스 및 위 적용 기록을 우선합니다.
이전 프로토콜 테스트는 `tests/legacy/`에 구분해 보존했습니다. 현재 펌웨어의 회귀 테스트로 사용하지 않습니다.
