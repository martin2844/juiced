"""Seal a staged side-motif package after its all-car load/save test passes."""
import argparse
import hashlib
import json
import re
import shutil
import zipfile
from pathlib import Path


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--package', type=Path, required=True)
    ap.add_argument('--validation', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    a = ap.parse_args()
    rows = json.loads((a.package/'catalogue.json').read_text())
    log = a.validation.read_text()
    expected = {(r['car'], r['mesh']) for r in rows}
    actual = {(car, mesh) for car, mesh in
              re.findall(r'^PASS (\S+) \d+ (\S+) ', log, re.M)}
    if actual != expected or 'FAIL ' in log or not log.rstrip().endswith(
            f'Validated {len(rows)} vinyls in garage/race plus save roundtrip'):
        raise ValueError('Incomplete or mismatched all-car validation')
    if a.output.exists():
        raise ValueError('Refusing to overwrite an existing archive')
    shutil.copyfile(a.validation, a.package/'validation.txt')
    files = sorted(f for f in a.package.rglob('*') if f.is_file() and f.name != 'SHA256SUMS')
    (a.package/'SHA256SUMS').write_text(''.join(
        hashlib.sha256(f.read_bytes()).hexdigest()+'  '+f.relative_to(a.package).as_posix()+'\n'
        for f in files))
    with zipfile.ZipFile(a.output, 'x', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for f in sorted(a.package.rglob('*')):
            if f.is_file():
                z.write(f, f.relative_to(a.package.parent))
    with zipfile.ZipFile(a.output) as z:
        if z.testzip() is not None:
            raise RuntimeError('ZIP corruption')
    print(a.output, a.output.stat().st_size, 'bytes', hashlib.sha256(a.output.read_bytes()).hexdigest())


if __name__ == '__main__':
    main()
