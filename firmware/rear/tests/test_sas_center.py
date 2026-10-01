"""SAS straight-line center learning: stand guard, wrap, capture, flash save/reload, slip alarm."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from test_delta_allocation import compiler, PROJECT

HERE = Path(__file__).resolve().parent


class SasCenterTests(unittest.TestCase):
    def test_learning_and_flash(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            for header in (PROJECT / 'Core/Inc').glob('*.h'):
                shutil.copyfile(header, folder / header.name)
            for stub in (HERE / 'sas_center_stubs').glob('*.h'):
                shutil.copyfile(stub, folder / stub.name)
            exe = folder / 'sas_center.exe'
            build = subprocess.run(compiler() + ['-std=c11', '-O0', '-UNDEBUG', '-I', str(folder),
                                   str(HERE / 'sas_center_harness.c'), str(PROJECT / 'Core/Src/sas_center.c'),
                                   '-lm', '-o', str(exe)], capture_output=True)
            self.assertEqual(build.returncode, 0, build.stderr.decode(errors='replace'))
            result = subprocess.run([str(exe)], capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr.decode(errors='replace'))
            self.assertIn(b'PASS', result.stdout)


if __name__ == '__main__':
    unittest.main()
