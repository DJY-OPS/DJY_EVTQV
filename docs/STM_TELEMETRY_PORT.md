# STM 고속 텔레메트리 이식

기준 저장소는 `DJY-OPS/DJY_EVTQV`의 `b390885987085bd7dabee423c2d23de4d5cabe86`이다.
`DJY_EvTLMT` 작업 브랜치의 `1a87c24fb899337d06bfd59b61e41b2f12c38439`에서
이미 구현한 STM 통신 변경만 옮겼다. ESP·서버·웹 소스는 텔레메트리 저장소에서 관리한다.
소스 이식과 호스트 검증을 수행했으며 보드 다운로드는 수행하지 않았다.

## 적용 파일과 동작

- `firmware/rear/Core/Inc/djy_telemetry_protocol.h`: 238바이트 바이너리 프레임,
  CRC16, sequence, 센서·제어값과 진단 필드.
- `firmware/rear/Core/Src/main.c`: 완료된 10ms 제어 틱마다 송신을 요청하고,
  메인 루프에서 USART1 비동기 송신. 송신 누락은 sequence와 skipped 카운터로 표시.
- `firmware/rear/Core/Inc/timing_diag.h`, `Core/Src/timing_diag.c`:
  기존 ASCII 확장의 19개 시간 진단 필드를 바이너리에 제공.
- `firmware/rear/stm_back.ioc`: USART1 460800 baud 설정.

후방 → ESP 링크는 460800 baud / 8N1 / 최대 100 Hz다.
후방 USART2(ST-Link) ASCII 진단은 115200 baud / 최대 5 Hz다.
ESP의 USB-UART 전체 표본 저장은 별도의 ESP 기능이며 후방 ST-Link와 다르다.
ESP → 후방의 기존 9바이트 제어 프레임은 그대로 사용한다.
100 Hz는 전송 요청률이며 메인 루프나 송신기가 밀릴 때 모든 틱의 송신을 보장하지 않는다.

## 새 저장소에서 보존한 내용

전방의 124개 공통 프로젝트·소스 파일은 줄바꿈 차이를 제외하면 기존 제공본과 같아
덮어쓰지 않았다. 시간 동기화 공통 소스 5개는 전방·후방 간 동일하다.

후방 `vehicle_params.h`, `torque_vectoring.c`와 기존 테스트도 변경하지 않았다.
기준 저장소의 SAS 영점 6845, 타이어 반경 0.225m, 합계 출력 요구 상한 10kW,
모터별 7kW 및 개선된 최종 차동 배분을 유지한다.
이는 이전 텔레메트리 저장소 사본의 영점 9175, 반경 0.230m, 합계 9.5kW와 다르다.
통신 변경으로 이 차량 설정을 이전 값으로 되돌리지 않았다.

10kW는 예측 출력 요구량의 상한이며 실측 배터리 전력의 초과 방지를 보장하지 않는다.
RPM 노이즈 판별, SAS I/O 오류 처리, IMU 캘리브레이션 제한은 이번 이식에서 변경하지 않았다.
기존 `DEPLOYED_SOURCE_SHA256.json`은 이전 보관 소스의 해시이며 이 변경 후 소스 해시가 아니다.

## 검증

- 새 저장소의 실제 TV/PID/ED 소스로 기존 출력 배분 테스트 3개 통과:
  총 34,884 제어 틱, 모터별 상한 7/5/4kW 조건.
- 기존 텔레메트리 저장소의 `test_binary_telemetry.py`에서 `REAR` 경로를
  이 저장소의 `firmware/rear/Core`로 지정해 2개 통과:
  실제 송신 함수와 ESP 수신 함수, CRC, 바이트 유실·삽입·손상, 누락 표본,
  재시작 및 sequence wrap 검사.
- `test_board_time_sync.py`의 `REAR`, `FRONT`를 이 저장소로 지정해 3개 통과:
  공유 소스 일치, CAN 프레임 쌍 처리, 시간 오프셋·드리프트·재시작 검사.
- 변경한 C 파일 2개는 Clang의 Cortex-M4 대상 구문 검사 통과.
  저장소의 최소 C 라이브러리 스텁에 빠진 표준 함수 선언 3개만 임시 헤더로 보완했다.
  이 검사는 ARM GCC 전체 빌드·링크 검증을 대신하지 않는다.

ARM GCC/CubeIDE 링크 빌드와 실물 통신 검증은 이번 환경에서 수행하지 않았다.
새로운 적용 펌웨어가 생성되거나 차량에 기록됐다는 의미는 아니다.
