# Rear STM firmware

The Rear firmware supports CRC-protected pit tuning commands, stationary-state
checks at the control boundary, revision checks and retransmission handling.
Settings are held in RAM and reset on reboot. Pit tuning requires a compatible
ESP gateway implementing the v2 PIT protocol; the existing v1 telemetry format
is unchanged. Front firmware is unchanged by this update.

UART mode must match the ESP gateway:

- Default binary: 460800 baud, telemetry up to 100 Hz.
- Legacy ASCII: 115200 baud, telemetry at 5 Hz.

The current main branch's 52/11 gear ratio and IMU zero confirmation checks are
preserved. Telemetry power values remain controller calculations, not a new
BMS measurement or calibration.

## Build

Set `DJY_ARM_GCC` to the installed `arm-none-eabi-gcc` executable, then run:

```text
python scripts/build_rear.py --mode binary
python scripts/build_rear.py --mode ascii
```

Outputs are written under `firmware/rear/Release/{binary,ascii}`. Add `--debug`
for Debug builds. The script does not flash a board.

## Host checks

```text
python -m unittest discover -s firmware/rear/tests -p "test_*.py" -v
```

Use GCC on PATH or set `DJY_HOST_ZIG` to a Zig executable. The sender/receiver
integration tests require `DJY_ESP_PROJECT` pointing to a compatible
`esp32_ev_gateway` checkout; they skip when that dependency is unavailable.
Host tests and compilation do not replace board or vehicle validation.
