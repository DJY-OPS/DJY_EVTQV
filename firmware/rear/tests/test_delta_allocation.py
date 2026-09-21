"""Run the actual TV/PID/ED sources on the host; no vehicle I/O."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

PROJECT = Path(__file__).resolve().parents[1]


def compiler():
    gcc = shutil.which('gcc')
    if gcc:
        return [gcc]
    zig = Path(os.environ.get('DJY_HOST_ZIG', str(Path.home() / 'telemetry-tools/host-tests/ziglang/zig.exe')))
    if not zig.is_file():
        raise RuntimeError('Install gcc or set DJY_HOST_ZIG to zig.exe')
    return [str(zig), 'cc']


class DeltaAllocationTests(unittest.TestCase):
    def check_configuration(self, motor_max=None):
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            for header in (PROJECT / 'Core/Inc').glob('*.h'):
                shutil.copyfile(header, folder / header.name)
            if motor_max is not None:
                params = folder / 'vehicle_params.h'
                content = params.read_text(encoding='utf-8')
                import re
                content, count = re.subn(r'(#define\s+MOTOR_MAX_KW\s+)\S+', lambda m: m[1] + str(motor_max) + 'f', content)
                self.assertEqual(count, 1)
                params.write_text(content, encoding='utf-8')
            exe = folder / 'allocation.exe'
            sources = [PROJECT / 'Core/Src' / (name + '.c')
                       for name in ('torque_vectoring', 'pid_controller', 'electronic_diff')]
            command = compiler() + ['-std=gnu11', '-O0', '-I', str(folder),
                                   str(PROJECT / 'tests/delta_allocation_harness.c'),
                                   *map(str, sources), '-o', str(exe), '-lm']
            result = subprocess.run(command, capture_output=True, timeout=180)
            self.assertEqual(result.returncode, 0, result.stderr.decode(errors='replace'))
            result = subprocess.run([str(exe)], capture_output=True, timeout=20)
            self.assertEqual(result.returncode, 0, result.stderr.decode(errors='replace'))
            print(result.stdout.decode().strip())

    def test_current_vehicle(self):
        self.check_configuration()

    def test_motor_headroom_limits_delta(self):
        self.check_configuration(5.0)

    def test_total_demand_exceeds_motor_budget(self):
        self.check_configuration(4.0)


if __name__ == '__main__':
    unittest.main()
