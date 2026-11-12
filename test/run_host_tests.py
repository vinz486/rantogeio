#!/usr/bin/env python3
"""Compile the actual controller/driver against simulated GPIO, NVS and queues."""
import os
from pathlib import Path
import subprocess
import tempfile

os.chdir(Path(__file__).resolve().parent.parent)
with tempfile.TemporaryDirectory(prefix="rantoge-tests-") as temporary:
    binary = str(Path(temporary) / "motion-tests")
    subprocess.run([
        "g++", "-std=c++11", "-Wall", "-Wextra", "-Werror", "-pthread",
        "-fsanitize=address,undefined", "-Itest/host", "-Iinclude",
        "test/host/motion_test.cpp", "test/host/ota_test.cpp", "test/host/runtime.cpp",
        "src/ClockManager.cpp", "src/StepperDriver.cpp", "src/TimeZoneManager.cpp",
        "src/CASIO.cpp", "src/WifiManager.cpp", "src/OtaManager.cpp", "-o", binary,
    ], check=True)
    subprocess.run([binary], check=True, env={**os.environ, "TZ": "UTC"})
subprocess.run(["node", "test/ui_test.js"], check=True)
