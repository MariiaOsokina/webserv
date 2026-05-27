#!/usr/bin/env python3

import subprocess
import sys
import os

# Path to executable
EXECUTABLE = "./bin/webserv"

# List of config tests
TEST_CASES = [
    {
        "name": "Duplicate Directive",
        "file": "config/test_duplicate_directive.conf",
        "should_pass": False,
    },
    {
        "name": "Invalid HTTP Method",
        "file": "config/test_invalid_method.conf",
        "should_pass": False,
    },
    {
        "name": "Missing Brace",
        "file": "config/test_missing_brace.conf",
        "should_pass": False,
    },
    {
        "name": "Missing Semicolon",
        "file": "config/test_missing_semicolon.conf",
        "should_pass": False,
    },
    {
        "name": "Out of range port",
        "file": "config/test_invalid_port.conf",
        "should_pass": False,
    },
    {
        "name": "Unknown directive",
        "file": "config/test_unknown_directive.conf",
        "should_pass": False,
    },
    {
        "name": "Extra closing braces",
        "file": "config/test_extra_braces.conf",
        "should_pass": False,
    },

    # Add VALID config tests too
    {
        "name": "Valid Config",
        "file": "config/advanced.conf",
        "should_pass": True,
    },
]

GREEN = "\033[92m"
RED = "\033[91m"
RESET = "\033[0m"


def run_test(test):
    config_path = test["file"]
    should_pass = test["should_pass"]

    print(f"\nRunning: {test['name']}")
    print(f"Config : {config_path}")

    if not os.path.exists(config_path):
        print(f"{RED}ERROR: Config file does not exist{RESET}")
        return False

    try:
        result = subprocess.run(
            [EXECUTABLE, "--check", config_path],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=10,
        )

        exit_code = result.returncode

        # Expected success
        if should_pass:
            if exit_code == 0:
                print(f"{GREEN}PASS{RESET} (valid config accepted)")
                return True
            else:
                print(f"{RED}FAIL{RESET} (valid config rejected)")
                print(result.stderr.decode())
                return False

        # Expected failure
        else:
            if exit_code != 0:
                print(f"{GREEN}PASS{RESET} (invalid config rejected)")
                return True
            else:
                print(f"{RED}FAIL{RESET} (invalid config accepted)")
                return False

    except subprocess.TimeoutExpired:
        print(f"{RED}FAIL{RESET} (program hung / timeout)")
        return False

    except Exception as e:
        print(f"{RED}ERROR{RESET}: {e}")
        return False


def main():
    passed = 0

    for test in TEST_CASES:
        if run_test(test):
            passed += 1

    total = len(TEST_CASES)

    print("\n==============================")
    print(f"Passed {passed}/{total} tests")

    if passed == total:
        print(f"{GREEN}ALL TESTS PASSED{RESET}")
        sys.exit(0)
    else:
        print(f"{RED}SOME TESTS FAILED{RESET}")
        sys.exit(1)


if __name__ == "__main__":
    main()