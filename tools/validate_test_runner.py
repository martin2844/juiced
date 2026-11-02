#!/usr/bin/env python3
"""Check runner selection failures without touching player saves.

Usage: python3 tools/validate_test_runner.py TEST_BINARY EVIDENCE_JSON
"""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
cases = [
    ("unknown-suite", ["--suite", "DefinitelyNotRegistered"], 2),
    ("unknown-list-suite", ["--list", "--suite", "DefinitelyNotRegistered"], 2),
    ("wrong-kind", ["--unit", "--suite", "SaveDevice"], 2),
    ("conflicting-kinds", ["--unit", "--integration"], 2),
    ("missing-suite", ["--suite"], 2),
    ("missing-game", ["--game"], 2),
    ("unknown-option", ["--unt"], 2),
    ("filtered-list", ["--list", "--integration", "--suite", "SaveDevice"], 0),
    ("save-device", ["--integration", "--suite", "SaveDevice"], 0),
    ("unit", ["--unit"], 0),
]
results = []
with tempfile.TemporaryDirectory(prefix="oj-runner-check-") as save_root:
    for name, args, expected in cases:
        result = subprocess.run([str(binary), save_root, *args], text=True,
                                capture_output=True, timeout=120)
        ok = result.returncode == expected
        if name == "filtered-list":
            lines = result.stdout.splitlines()
            ok = ok and bool(lines) and all(line.startswith("integration SaveDevice.") for line in lines)
        results.append(dict(name=name, expected=expected, actual=result.returncode,
                            ok=ok, stdout=result.stdout, stderr=result.stderr))
Path(sys.argv[2]).write_text(json.dumps(results, indent=2) + "\n")
for result in results:
    print(f"{'PASS' if result['ok'] else 'FAIL'} {result['name']}")
sys.exit(0 if all(result['ok'] for result in results) else 1)
