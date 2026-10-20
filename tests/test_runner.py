#!/usr/bin/env python3
import os
import subprocess
import sys

def run_tests():
    compiler = os.path.join("build", "minigo.exe")
    if not os.path.exists(compiler):
        print(f"Error: {compiler} not found. Please build the project first.")
        sys.exit(1)

    examples_dir = "examples"
    test_cases = [
        ("01_hello.go", 0, "42\n52\n32\n22\n"),
        ("02_arithmetic.go", 53, "53\n27\n15\n"),
        ("03_control_flow.go", 45, "1\n45\n"),
        ("04_functions.go", 42, "42\n"),
    ]

    passed = 0
    total = len(test_cases)

    print("========================================")
    print(" Running Mini Go Compiler Integration Tests ")
    print("========================================")

    for filename, expected_code, expected_output in test_cases:
        filepath = os.path.join(examples_dir, filename)
        bin_path = os.path.join("build", f"test_{filename}.exe")

        print(f"--> Testing {filename}...", end=" ", flush=True)

        # 1. Compile
        comp_res = subprocess.run([compiler, "-o", bin_path, filepath], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if comp_res.returncode != 0:
            print("FAILED (Compilation error)")
            print(comp_res.stderr)
            continue

        # 2. Run
        run_res = subprocess.run([bin_path], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if run_res.returncode != expected_code:
            print(f"FAILED (Exit code mismatch: expected {expected_code}, got {run_res.returncode})")
            continue

        if expected_output and run_res.stdout != expected_output:
            print(f"FAILED (Output mismatch)")
            print(f"Expected:\n{expected_output}")
            print(f"Got:\n{run_res.stdout}")
            continue

        print("PASSED")
        passed += 1

    print("========================================")
    print(f"Results: {passed}/{total} tests passed.")
    print("========================================")

    if passed != total:
        sys.exit(1)

if __name__ == "__main__":
    run_tests()
