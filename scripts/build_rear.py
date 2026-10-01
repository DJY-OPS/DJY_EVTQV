"""Build Rear without CubeIDE; never opens a debugger or writes a device.

Set DJY_ARM_GCC to arm-none-eabi-gcc.exe, or put it on PATH.
Outputs go to firmware/rear/{Debug,Release}/{binary,ascii}/.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def build(mode, debug):
    compiler = os.environ.get('DJY_ARM_GCC') or shutil.which('arm-none-eabi-gcc')
    if not compiler:
        raise SystemExit('Set DJY_ARM_GCC to the installed ARM GCC executable')
    gcc = Path(compiler)
    suffix = gcc.suffix
    root = ROOT / 'firmware/rear'
    out = root / ('Debug' if debug else 'Release') / mode
    out.mkdir(parents=True, exist_ok=True)
    flags = ['-mcpu=cortex-m4', '-mthumb', '-mfpu=fpv4-sp-d16', '-mfloat-abi=hard',
             '-DUSE_HAL_DRIVER', '-DSTM32F446xx', '-std=gnu11', '-g3',
             '-Og' if debug else '-O2', '-ffunction-sections', '-fdata-sections',
             '-Wall', '-Wextra', f'-DESP_LINK_BINARY_TELEMETRY={int(mode == "binary")}']
    if debug:
        flags.append('-DDEBUG')
    for folder in ('Core/Inc', 'Drivers/STM32F4xx_HAL_Driver/Inc',
                   'Drivers/STM32F4xx_HAL_Driver/Inc/Legacy',
                   'Drivers/CMSIS/Device/ST/STM32F4xx/Include', 'Drivers/CMSIS/Include',
                   'FATFS/Target', 'FATFS/App', 'Middlewares/Third_Party/FatFs/src'):
        flags += ['-I', str(root / folder)]
    sources = []
    for folder in ('Core/Src', 'Drivers/STM32F4xx_HAL_Driver/Src', 'FATFS',
                   'Middlewares/Third_Party/FatFs/src'):
        sources += sorted((root / folder).rglob('*.c'))
    objects = []
    commands = []

    def run(command):
        commands.append(subprocess.list2cmdline(list(map(str, command))))
        result = subprocess.run(command, capture_output=True)
        with (out / 'build.log').open('ab') as log:
            log.write(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(result.stderr.decode(errors='replace'))

    (out / 'build.log').write_bytes(b'')
    for source in sources + sorted((root / 'Core/Startup').glob('*.s')):
        obj = out / ('_'.join(source.relative_to(root).parts) + '.o')
        language = ['-x', 'assembler-with-cpp'] if source.suffix == '.s' else []
        run([str(gcc), *flags, *language, '-c', str(source), '-o', str(obj)])
        objects.append(obj)
    # CubeIDE's READONLY output-section syntax needs binutils >= 2.37.
    # Older toolchains can use an output-local copy with that keyword removed.
    linker = out / 'STM32F446RETX_FLASH.ld'
    linker.write_text((root / linker.name).read_text().replace(' (READONLY)', ''))
    rsp = out / 'objects.rsp'
    rsp.write_text('\n'.join('"' + p.as_posix() + '"' for p in objects))
    elf = out / 'stm_back.elf'
    run([str(gcc), '@' + str(rsp), '-mcpu=cortex-m4', '-mthumb', '-mfpu=fpv4-sp-d16',
         '-mfloat-abi=hard', '-T' + str(linker), '--specs=nano.specs', '--specs=nosys.specs',
         '-Wl,--gc-sections', '-Wl,-Map=' + str(out / 'stm_back.map'),
         '-Wl,--start-group', '-lc', '-lm', '-Wl,--end-group', '-o', str(elf)])
    for kind, ext in (('binary', 'bin'), ('ihex', 'hex')):
        run([str(gcc.with_name('arm-none-eabi-objcopy' + suffix)), '-O', kind,
             str(elf), str(elf.with_suffix('.' + ext))])
    run([str(gcc.with_name('arm-none-eabi-size' + suffix)), str(elf)])
    (out / 'commands.txt').write_text('\n'.join(commands), encoding='utf-8')
    print(f'Built {elf} (NOT flashed)')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=['binary', 'ascii'], default='binary')
    parser.add_argument('--debug', action='store_true')
    args = parser.parse_args()
    build(args.mode, args.debug)
