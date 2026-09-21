# 최근 적용 펌웨어 소스 — 2026-09-21

## 출처와 적용 버전

`Documents/TV/stm_back`와 `Documents/TV/stm_front`에서 빌드 산출물·캐시를 제외하고 299개 파일을 복사했다.
원본 폴더의 소스는 수정하지 않았다. 각 파일의 원본 바이트 SHA256은 `DEPLOYED_SOURCE_SHA256.json`에 기록했다.

후방의 마지막 확인된 적용은 2026-09-20 SAS 영점 보정이다.

- 합계 출력 요구 상한: 10 kW, 모터별 7 kW
- SAS_CENTER_RAW: 6845 (정위치 52개 표본, raw 6842~6848)
- PID: 50 / 1 / 5
- 타이어 반경: 0.225 m, 감속비: 4:1
- 당시 적용 ELF SHA256: C218E01904DAC1C779A93B57A006200E3888D26B175C1588783CF7D084EA92EA
- 당시 적용 BIN 및 플래시 재읽기 SHA256: C24C6D033BD5A54019A6D35AD12F747677AF896BDF5363DB9DBD7BBB50C4EDE0
- 적용 이미지 크기: 70,024 bytes, base 0x08000000

이번 작업에서 원본 `stm_back/Debug/stm_back.elf`가 위 적용 ELF와 일치하는 것을 확인했다.
전방은 현재 작업 폴더의 소스를 함께 동기화했으며, 이번에 전방 보드의 플래시를 읽어 비교한 것은 아니다.

## 재빌드 검증

STM32CubeIDE 2.1.1 / GNU Tools for STM32 14.3.rel1로 새 체크아웃의 전방·후방 Debug/Release를 빌드했다.
후방 ΔP 배분 호스트 테스트 3개, 총 34,884 제어 틱이 통과했다.

CubeIDE가 재생성한 objects.list는 시간 계측 모듈을 이름순으로 배치하므로 당시 링크 순서와 다르다.
기본 재빌드 BIN은 70,028 bytes였고 적용 이미지와 바이트 단위로 같지는 않았다.
같은 새 빌드의 오브젝트를 `rear-deployed-link-order.rsp` 순서로 다시 링크하면 70,024 bytes가 되며,
BIN SHA256이 당시 보드에 적용한 C24C6D...C4EDE0과 정확히 일치했다.
ELF의 디버그 경로는 새 체크아웃 위치를 포함하므로 ELF 전체의 동일성을 주장하지 않는다.

재현 절차: CubeIDE에서 후방 Debug를 빌드한 다음, ARM GCC가 PATH에 있는 셸에서
`firmware/rear/Debug`를 작업 디렉터리로 사용한다.

```powershell
arm-none-eabi-gcc -o reproduced.elf '@../../../docs/rear-deployed-link-order.rsp' -mcpu=cortex-m4 '-T../STM32F446RETX_FLASH.ld' --specs=nosys.specs '-Wl,--gc-sections' -static --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb '-Wl,--start-group' -lc -lm '-Wl,--end-group'
arm-none-eabi-objcopy -O binary reproduced.elf reproduced.bin
Get-FileHash -LiteralPath reproduced.bin -Algorithm SHA256
```

## 범위

이번에는 소스와 검증 기록만 저장소에 올린다. 차량 재플래시·PID 변경·영점 변경은 수행하지 않았다.
10 kW는 STM의 출력 요구량 상한이며 실측 전력 피드백에 의한 입력 전력 제한이 아니다.
과거 문서와 shared/include는 이전 구성도 포함하므로 현재 동작은 각 firmware 프로젝트 소스를 기준으로 확인한다.