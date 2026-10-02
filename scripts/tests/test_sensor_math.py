"""Build and run the sensor_math host test.

The conversions in main/apps/smart_home_hub/sensor_math.h used to live inside
the I2C driver, so the only way to check them was to flash a board and read the
log. They are plain arithmetic now, so compile them with the host compiler and
run them here.
"""

import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path(__file__).with_name("sensor_math_test.cc")
INCLUDE_DIR = ROOT / "main" / "apps" / "smart_home_hub"


def find_compiler():
    for name in ("g++", "clang++", "c++"):
        path = shutil.which(name)
        if path:
            return path
    return None


class SensorMathTest(unittest.TestCase):
    def test_sensor_math(self):
        compiler = find_compiler()
        if compiler is None:
            self.skipTest("no host C++ compiler (g++/clang++) available")

        with tempfile.TemporaryDirectory() as tmp:
            binary = os.path.join(tmp, "sensor_math_test")
            compile_result = subprocess.run(
                [
                    compiler,
                    "-std=gnu++17",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    f"-I{INCLUDE_DIR}",
                    str(SOURCE),
                    "-o",
                    binary,
                ],
                capture_output=True,
                text=True,
            )
            self.assertEqual(
                compile_result.returncode,
                0,
                f"compiling {SOURCE.name} failed:\n{compile_result.stderr}",
            )
            # -Werror above already rejects warnings; keep any that survive
            # visible instead of silently dropping them.
            self.assertEqual(compile_result.stderr, "", compile_result.stderr)

            run_result = subprocess.run([binary], capture_output=True, text=True)
            if run_result.returncode != 0:
                self.fail(f"{SOURCE.name} failed:\n{run_result.stdout}{run_result.stderr}")


if __name__ == "__main__":
    unittest.main()
