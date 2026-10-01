"""Actual STM control sources: atomic pit edits, live output and reset behavior."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from test_delta_allocation import compiler, PROJECT


class PitTuningTests(unittest.TestCase):
    def test_control_transaction(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder=Path(tmp)
            for header in (PROJECT/'Core/Inc').glob('*.h'):
                shutil.copyfile(header,folder/header.name)
            (folder/'main.h').write_text('''#pragma once
#include <stdint.h>
extern uint32_t mask;
#define __get_PRIMASK() mask
#define __disable_irq() (mask=1)
#define __set_PRIMASK(x) (mask=(x))
''')
            exe=folder/'pit.exe'
            sources=[PROJECT/'Core/Src'/f'{name}.c' for name in
                     ('pit_tuning','control_settings','torque_vectoring','pid_controller','electronic_diff')]
            result=subprocess.run(compiler()+['-std=gnu11','-Wall','-Wextra','-Werror',
                '-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(folder),
                str(PROJECT/'tests/pit_tuning_harness.c'),*map(str,sources),'-o',str(exe),'-lm'],
                capture_output=True,timeout=180)
            self.assertEqual(result.returncode,0,result.stderr.decode(errors='replace'))
            result=subprocess.run([str(exe)],capture_output=True,timeout=20)
            self.assertEqual(result.returncode,0,result.stderr.decode(errors='replace'))
            print(result.stdout.decode().strip())
