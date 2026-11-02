#!/usr/bin/env python3
"""Calibrate UserSaves against an isolated copy of a known-good profile tree.

Usage: python3 tools/validate_user_saves.py TEST_BINARY SAVE_ROOT EVIDENCE_DIR
Never writes to SAVE_ROOT. A temporary tree holds all deliberately broken saves.
"""

import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile


def omit_entry(path, omitted):
    """Independent encoder: keep the archive valid while removing one entry."""
    raw = path.read_bytes()
    count, directory = struct.unpack_from('<II', raw)
    assert directory + count * 132 + 8 == len(raw)
    entries = []
    offset = 8
    for index in range(count):
        record = directory + index * 132
        name = raw[record:record + 128].split(b'\0', 1)[0]
        size, = struct.unpack_from('<I', raw, record + 128)
        assert offset + size <= directory
        if name.decode('ascii') != omitted:
            entries.append((name, raw[offset:offset + size]))
        offset += size
    assert offset == directory and len(entries) == count - 1
    data = b''.join(value for _, value in entries)
    checksum = 0x5A5A5A5A5A5A5A5A
    for byte in data:
        checksum = (((checksum << 1) | (checksum >> 63)) & ((1 << 64) - 1)) ^ byte
    table = b''.join(name.ljust(128, b'\0') + struct.pack('<I', len(value))
                     for name, value in entries)
    path.write_bytes(struct.pack('<II', len(entries), 8 + len(data)) + data
                     + table + struct.pack('<Q', checksum))


def main():
    binary, source, evidence = (Path(arg).resolve() for arg in sys.argv[1:])
    evidence.mkdir(parents=True, exist_ok=True)
    drivers = sorted(p.name for p in (source / 'SAVE').iterdir()
                     if p.is_dir() and p.name.startswith('P'))
    assert drivers, 'Supply a known-good save containing a driver'
    results = []
    cases = ['valid', 'empty', 'missing-reg', 'bad-userdata', 'mixed-bad-userdata',
             'missing-archive', 'bad-hash', 'missing-help', 'missing-settings',
             'missing-arcade', 'no-career']
    with tempfile.TemporaryDirectory(prefix='openjuice-save-controls-') as temp:
        for case in cases:
            root = Path(temp) / case
            if case == 'empty':
                root.mkdir()
            else:
                shutil.copytree(source, root)
            driver = root / 'SAVE' / drivers[0]
            archive = driver / 'juicey.dat'
            if case == 'missing-reg':
                (root / 'reg.cf1').unlink()
            elif case == 'bad-userdata':
                (driver / 'USERDATA').write_bytes(b'bad')
            elif case == 'mixed-bad-userdata':
                broken = root / 'SAVE' / 'PCalibrationBroken'
                assert not broken.exists()
                shutil.copytree(driver, broken)
                (broken / 'USERDATA').write_bytes(b'bad')
            elif case == 'missing-archive':
                archive.unlink()
            elif case == 'bad-hash':
                raw = bytearray(archive.read_bytes())
                raw[-1] ^= 1
                archive.write_bytes(raw)
            elif case in ('missing-help', 'missing-settings', 'missing-arcade', 'no-career'):
                omit_entry(archive, {'missing-help': 'UIHelp.sav', 'missing-settings': 'settings.dat',
                                    'missing-arcade': 'ArcadeUnlock.sav',
                                    'no-career': 'CMSGlobal.sav'}[case])
            run = subprocess.run([str(binary), str(root), '--integration', '--suite', 'UserSaves'],
                                 capture_output=True, text=True, timeout=60)
            (evidence / (case + '.log')).write_text(run.stdout + run.stderr)
            expected = 0 if case in ('valid', 'no-career') else 1
            # A crash/timeout is not successful rejection by the verifier.
            ok = run.returncode == expected and (expected == 0 or 'FAIL ' in run.stdout)
            results.append(dict(case=case, expected_exit=expected, actual_exit=run.returncode, ok=ok))
            print(f'{"PASS" if ok else "FAIL"} {case}: exit {run.returncode}, expected {expected}')
    (evidence / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    return 0 if all(result['ok'] for result in results) else 1


if __name__ == '__main__':
    sys.exit(main())
