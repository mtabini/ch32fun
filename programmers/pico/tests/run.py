#!/usr/bin/env python3
"""Run host behavior tests; SDK queue synchronization and USB require hardware tests."""
import pathlib, subprocess, tempfile
root = pathlib.Path(__file__).resolve().parent
with tempfile.TemporaryDirectory() as directory:
    binary = pathlib.Path(directory) / "bridge-test"
    subprocess.run(["cc", "-std=c11", "-fsanitize=address,undefined", "-g", "-I", str(root / "fakes"), str(root / "bridge_test.c"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
