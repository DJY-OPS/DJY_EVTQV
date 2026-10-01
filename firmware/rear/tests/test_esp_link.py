"""Exercise the actual Rear receiver using a small UART/HAL test double."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from test_delta_allocation import compiler, PROJECT


class EspLinkTests(unittest.TestCase):
    def test_stream_errors_timeout_and_full_duplex_recovery(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            for name in ('esp_link.h', 'djy_uart_protocol.h', 'pit_tuning.h',
                         'djy_pit_protocol.h', 'djy_telemetry_protocol.h'):
                shutil.copyfile(PROJECT / 'Core/Inc' / name, folder / name)
            (folder / 'main.h').write_text(r'''
#pragma once
#include <stdint.h>
typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY } HAL_StatusTypeDef;
enum { HAL_UART_STATE_READY, HAL_UART_STATE_BUSY_RX, HAL_UART_STATE_BUSY_TX };
enum { HAL_UART_ERROR_NONE, HAL_UART_ERROR_ORE, HAL_UART_ERROR_FE, HAL_UART_ERROR_NE = 4 };
#define UART_IT_RXNE 1u
typedef struct { uint32_t RxState, gState, ErrorCode, rx_irq; } UART_HandleTypeDef;
extern uint32_t irq_mask;
#define __get_PRIMASK() irq_mask
#define __disable_irq() (irq_mask = 1u)
#define __set_PRIMASK(x) (irq_mask = (x))
#define __HAL_UART_GET_IT_SOURCE(h, i) ((h)->rx_irq & (i))
void clear_errors(UART_HandleTypeDef *h);
#define __HAL_UART_CLEAR_OREFLAG(h) clear_errors(h)
uint32_t HAL_GetTick(void);
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *, uint8_t *, uint16_t);
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *);
void HAL_UART_ErrorCallback(UART_HandleTypeDef *);
''', encoding='utf-8')
            exe = folder / 'esp_link.exe'
            result = subprocess.run(compiler() + ['-std=c11', '-Wall', '-Wextra', '-Werror',
                '-I', str(folder), str(PROJECT / 'Core/Src/esp_link.c'),
                str(PROJECT / 'tests/esp_link_harness.c'), '-o', str(exe)],
                capture_output=True, timeout=180)
            self.assertEqual(result.returncode, 0, result.stderr.decode(errors='replace'))
            result = subprocess.run([str(exe)], capture_output=True, timeout=20)
            self.assertEqual(result.returncode, 0, result.stderr.decode(errors='replace'))
            print(result.stdout.decode().strip())


if __name__ == '__main__':
    unittest.main()
